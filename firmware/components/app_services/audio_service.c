#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "codec_board.h"
#include "codec_init.h"
#include "audio_service.h"

static const char *TAG = "audio";
static esp_codec_dev_handle_t s_playback = NULL;
static esp_codec_dev_handle_t s_record = NULL;
void audio_service_boot_sound(void);
void audio_service_poweroff_sound(void);

static void mic_self_test(void) {
    if (s_record == NULL) return;
    static int16_t samples[320 * 2];
    int64_t sum_sq[2] = {0, 0};
    int64_t sum[2] = {0, 0};
    int peak[2] = {0, 0};
    int frames = 0;
    for (int block = 0; block < 10; ++block) {
        int rc = esp_codec_dev_read(s_record, samples, sizeof(samples));
        if (rc != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "mic self-test read failed: %d", rc);
            return;
        }
        for (int i = 0; i < 320; ++i) {
            for (int ch = 0; ch < 2; ++ch) {
                int v = samples[i * 2 + ch];
                int a = v < 0 ? -v : v;
                sum[ch] += v;
                sum_sq[ch] += (int64_t)v * v;
                if (a > peak[ch]) peak[ch] = a;
            }
        }
        frames += 320;
    }
    double rms_l = sqrt((double)sum_sq[0] / frames);
    double rms_r = sqrt((double)sum_sq[1] / frames);
    ESP_LOGI(TAG, "mic self-test: frames=%d L(rms=%.1f peak=%d dc=%.1f) R(rms=%.1f peak=%d dc=%.1f)",
             frames, rms_l, peak[0], (double)sum[0] / frames,
             rms_r, peak[1], (double)sum[1] / frames);
    if (peak[0] < 8 && peak[1] < 8) ESP_LOGW(TAG, "mic self-test: no input signal");
}

void audio_service_init(void) {
    set_codec_board_type("S3_RLCD_4_2");
    codec_init_cfg_t cfg = {0};
    cfg.in_mode = CODEC_I2S_MODE_TDM;
    cfg.out_mode = CODEC_I2S_MODE_TDM;
    cfg.in_use_tdm = false;
    cfg.reuse_dev = false;
    if (init_codec(&cfg) != 0) {
        ESP_LOGE(TAG, "codec init failed, beep disabled");
        return;
    }
    s_playback = get_playback_handle();
    s_record = get_record_handle();
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    if (s_playback == NULL || esp_codec_dev_open(s_playback, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "playback open failed");
        s_playback = NULL;
    } else {
        /* 编解码器打开后默认是静音（muted）的，必须显式解除静音，
         * 否则扬声器全程无声（beep / TTS / 开机音都听不到）。 */
        esp_codec_dev_set_out_mute(s_playback, false);
        esp_codec_dev_set_out_vol(s_playback, 60);
    }
    if (s_record == NULL || esp_codec_dev_open(s_record, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "record open failed");
        s_record = NULL;
    } else {
        esp_codec_dev_set_in_gain(s_record, 30.0f);
    }
    ESP_LOGI(TAG, "audio init: playback=%s record=%s", s_playback ? "ok" : "failed",
             s_record ? "ok" : "failed");
    mic_self_test();
    audio_service_boot_sound();
}

/* 播放一段正弦提示音（16kHz 2ch 16bit），阻塞写入 I2S */
static void play_tone(uint16_t freq, uint32_t dur_ms, uint8_t vol)
{
    if (!s_playback) return;
    const int sr = 16000;
    int n = (int)((uint64_t)sr * dur_ms / 1000);
    int16_t *buf = (int16_t *)heap_caps_malloc((size_t)n * 2 * sizeof(int16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return;
    for (int i = 0; i < n; i++) {
        int16_t v = (int16_t)(sinf(2.0f * 3.14159265f * freq * i / sr) * 9000);
        buf[i * 2] = v;
        buf[i * 2 + 1] = v;
    }
    esp_codec_dev_set_out_vol(s_playback, vol);
    esp_codec_dev_write(s_playback, (uint8_t *)buf, (uint32_t)(n * 2 * sizeof(int16_t)));
    esp_codec_dev_set_out_vol(s_playback, 60);
    heap_caps_free(buf);
}

/* 开机音：两声上行“滴-嘟” */
void audio_service_boot_sound(void)
{
    play_tone(660, 130, 70);
    vTaskDelay(pdMS_TO_TICKS(60));
    play_tone(990, 180, 70);
}

/* 关机音：三声下行旋律 */
void audio_service_poweroff_sound(void)
{
    play_tone(784, 160, 70);
    vTaskDelay(pdMS_TO_TICKS(70));
    play_tone(587, 160, 70);
    vTaskDelay(pdMS_TO_TICKS(70));
    play_tone(392, 320, 70);
    /* esp_codec_dev_write 只是把数据入 I2S 环形缓冲便返回，真实播音滞后；
     * 不加等待的话 esp_restart 会直接复位外设、把声音截断，所以这里等 DMA 播完。 */
    vTaskDelay(pdMS_TO_TICKS(600));
}

/* 业务流程提示音：不同阶段用不同音高组合，便于不听屏幕也能分辨结果 */
void audio_service_cue(audio_cue_t cue)
{
    switch (cue) {
    case AUDIO_CUE_START:          /* 上行两声：开始下载 */
        play_tone(523, 90, 70);
        vTaskDelay(pdMS_TO_TICKS(50));
        play_tone(784, 120, 70);
        break;
    case AUDIO_CUE_DEVICE_FOUND:   /* 单声高音：已搜到设备 */
        play_tone(1046, 90, 70);
        break;
    case AUDIO_CUE_DOWNLOAD_DONE:  /* 上行两声（更高）：下载完成 */
        play_tone(659, 100, 70);
        vTaskDelay(pdMS_TO_TICKS(60));
        play_tone(880, 150, 70);
        break;
    case AUDIO_CUE_UPLOAD_DONE:    /* 上行三声：上传完成 */
        play_tone(523, 90, 70);
        vTaskDelay(pdMS_TO_TICKS(50));
        play_tone(659, 90, 70);
        vTaskDelay(pdMS_TO_TICKS(50));
        play_tone(784, 170, 70);
        break;
    case AUDIO_CUE_ERROR:          /* 下行双低音：出错 */
        play_tone(330, 150, 70);
        vTaskDelay(pdMS_TO_TICKS(70));
        play_tone(233, 260, 70);
        break;
    case AUDIO_CUE_CANCEL:         /* 下行两声：已中断 */
        play_tone(700, 90, 70);
        vTaskDelay(pdMS_TO_TICKS(60));
        play_tone(440, 170, 70);
        break;
    default:
        break;
    }
}

void audio_service_set_volume(int vol) {
    if (s_playback) esp_codec_dev_set_out_vol(s_playback, vol);
}

void *audio_service_playback_handle(void) {
    return (void *)s_playback;
}

void *audio_service_record_handle(void) {
    return (void *)s_record;
}

void audio_service_beep(void) {
    if (s_playback == NULL) return;
    /* 0.15s 1kHz 正弦波，16kHz 2ch 16bit */
    static int16_t beep[4800];
    for (int i = 0; i < 2400; i++) {
        int16_t v = (int16_t)(sinf(2.0f * 3.14159265f * 1000 * i / 16000) * 8000);
        beep[i * 2] = v;
        beep[i * 2 + 1] = v;
    }
    esp_codec_dev_write(s_playback, beep, sizeof(beep));
    ESP_LOGI(TAG, "beep");
}
