#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "codec_board.h"
#include "codec_init.h"
#include "audio_service.h"

static const char *TAG = "audio";
static esp_codec_dev_handle_t s_playback = NULL;
static esp_codec_dev_handle_t s_record = NULL;
/* 所有提示音共用一把锁：esp_codec_dev_write 不是线程安全的，
 * RaceBox 提示音、开机/关机音、beep 若并发写入会交错成杂音。
 * 整段旋律（多音）持锁一次，保证不被别的提示音插进来打断。 */
static SemaphoreHandle_t s_lock = NULL;
/* 普通操作音量：除小智 TTS 外的一切声音（按键音、开关机音、RaceBox 状态音、
 * 企微签到/签退与工时记录提醒铃声）都直接按这个值播放，后台可调，默认 60。
 * 小智 TTS 走独立的“智能语音音量”（voice_begin 传入），两路互不影响。 */
static int s_system_volume = 60;
static int s_voice_volume = 60;
static bool s_voice_active = false;
/* 设置普通操作音量：直接作为非小智声音的播放音量（0..100），保存后热生效 */
void audio_service_set_system_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    s_system_volume = vol;
    if (s_playback && !s_voice_active) esp_codec_dev_set_out_vol(s_playback, s_system_volume);
    ESP_LOGI(TAG, "system cue volume=%d", s_system_volume);
}
void audio_service_boot_sound(void);
void audio_service_poweroff_sound(void);

static void lock_audio(void)   { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock_audio(void) { if (s_lock) xSemaphoreGive(s_lock); }
static int restore_volume(void) { return s_voice_active ? s_voice_volume : s_system_volume; }

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
    s_lock = xSemaphoreCreateMutex();
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
        esp_codec_dev_set_out_vol(s_playback, s_system_volume);
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

/* 播放一段正弦提示音（16kHz 2ch 16bit），阻塞写入 I2S。
 * 调用前必须已持有 s_lock（整段旋律持锁，避免被别的提示音插断）。
 * 所有非小智声音统一按 s_system_volume（后台“普通操作音量”）播放，
 * 且波形幅度一律用 9000（见 tone），按键音、开关机音、RaceBox 状态音、
 * 签到/签退/工时记录提醒铃声的比例完全一致。 */
static void tone_amp(uint16_t freq, uint32_t dur_ms, int16_t amp)
{
    if (!s_playback) return;
    const int sr = 16000;
    int n = (int)((uint64_t)sr * dur_ms / 1000);
    if (n <= 0) return;
    int16_t *buf = (int16_t *)heap_caps_malloc((size_t)n * 2 * sizeof(int16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return;
    for (int i = 0; i < n; i++) {
        int16_t v = (int16_t)(sinf(2.0f * 3.14159265f * freq * i / sr) * amp);
        buf[i * 2] = v;
        buf[i * 2 + 1] = v;
    }
    esp_codec_dev_set_out_vol(s_playback, s_system_volume);
    esp_codec_dev_write(s_playback, (uint8_t *)buf, (uint32_t)(n * 2 * sizeof(int16_t)));
    /* 提示音结束后恢复进入提示音前的播放上下文：小智会话中恢复小智音量，
     * 其他时候恢复系统原始基准，不让后台 voice_volume 改变提示音。 */
    esp_codec_dev_set_out_vol(s_playback, restore_volume());
    heap_caps_free(buf);
}

static void tone(uint16_t freq, uint32_t dur_ms)
{
    tone_amp(freq, dur_ms, 9000);
}

/* 开机音：两声上行“滴-嘟” */
void audio_service_boot_sound(void)
{
    lock_audio();
    tone(660, 130);
    vTaskDelay(pdMS_TO_TICKS(60));
    tone(990, 180);
    unlock_audio();
}

/* 关机音：三声下行旋律。仅软件重启路径（/api/system/restart）可用——
 * 硬件按键断电是瞬间掉电，固件来不及在断电后播音。 */
void audio_service_poweroff_sound(void)
{
    lock_audio();
    tone(784, 160);
    vTaskDelay(pdMS_TO_TICKS(70));
    tone(587, 160);
    vTaskDelay(pdMS_TO_TICKS(70));
    tone(392, 320);
    /* esp_codec_dev_write 只是把数据入 I2S 环形缓冲便返回，真实播音滞后；
     * 不加等待的话 esp_restart 会直接复位外设、把声音截断，所以这里等 DMA 播完。 */
    vTaskDelay(pdMS_TO_TICKS(600));
    unlock_audio();
}

/* 业务流程提示音：不同阶段用不同音高组合，便于不听屏幕也能分辨结果 */
void audio_service_cue(audio_cue_t cue)
{
    lock_audio();
    switch (cue) {
    case AUDIO_CUE_START:          /* 上行两声：开始下载 */
        tone(523, 90);
        vTaskDelay(pdMS_TO_TICKS(50));
        tone(784, 120);
        break;
    case AUDIO_CUE_DEVICE_FOUND:   /* 单声高音：已搜到设备 */
        tone(1046, 90);
        break;
    case AUDIO_CUE_DOWNLOAD_DONE:  /* 上行两声（更高）：下载完成 */
        tone(659, 100);
        vTaskDelay(pdMS_TO_TICKS(60));
        tone(880, 150);
        break;
    case AUDIO_CUE_UPLOAD_DONE:    /* 上行三声：上传完成 */
        tone(523, 90);
        vTaskDelay(pdMS_TO_TICKS(50));
        tone(659, 90);
        vTaskDelay(pdMS_TO_TICKS(50));
        tone(784, 170);
        break;
    case AUDIO_CUE_ERROR:          /* 下行双音：出错 / 未发现设备 */
        tone(466, 150);
        vTaskDelay(pdMS_TO_TICKS(70));
        tone(349, 300);
        break;
    case AUDIO_CUE_CANCEL:         /* 下行两声：已中断 */
        tone(700, 90);
        vTaskDelay(pdMS_TO_TICKS(60));
        tone(440, 170);
        break;
    case AUDIO_CUE_NO_DATA:        /* 同高两声：设备无定位数据 */
        tone(523, 110);
        vTaskDelay(pdMS_TO_TICKS(90));
        tone(523, 110);
        break;
    case AUDIO_CUE_KEY:            /* 统一按键音：极短一声轻响，音量略低 */
        tone(1318, 45);
        break;
    case AUDIO_CUE_KEY_TOGGLE:     /* 同步中短按：上行两声轻响（收起/展开弹窗） */
        tone(988, 45);
        vTaskDelay(pdMS_TO_TICKS(40));
        tone(1318, 45);
        break;
    /* 企微签到 / 签退 / 记录工时提醒铃声：约 1 秒的多音旋律。
     * 音量、波形幅度与普通按键音完全同口径（都用 tone()），
     * 区别只在旋律长短和音高组合，不做任何额外放大。 */
    case AUDIO_CUE_SIGNIN:         /* 上行三音：该签到了 */
        tone(523, 120);
        vTaskDelay(pdMS_TO_TICKS(60));
        tone(659, 120);
        vTaskDelay(pdMS_TO_TICKS(60));
        tone(784, 220);
        break;
    case AUDIO_CUE_SIGNOUT:        /* 下行三音：该签退了 */
        tone(784, 120);
        vTaskDelay(pdMS_TO_TICKS(60));
        tone(659, 120);
        vTaskDelay(pdMS_TO_TICKS(60));
        tone(523, 220);
        break;
    case AUDIO_CUE_WORKTIME:       /* 双音重复：该记工时了 */
        tone(880, 130);
        vTaskDelay(pdMS_TO_TICKS(70));
        tone(880, 130);
        vTaskDelay(pdMS_TO_TICKS(70));
        tone(1174, 240);
        break;
    default:
        break;
    }
    unlock_audio();
}

void audio_service_voice_begin(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    lock_audio();
    s_voice_volume = vol;
    s_voice_active = true;
    if (s_playback) esp_codec_dev_set_out_vol(s_playback, vol);
    unlock_audio();
    ESP_LOGI(TAG, "voice playback volume=%d (system cues unchanged)", vol);
}

void audio_service_voice_end(void) {
    lock_audio();
    s_voice_active = false;
    if (s_playback) esp_codec_dev_set_out_vol(s_playback, s_system_volume);
    unlock_audio();
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
    lock_audio();
    esp_codec_dev_write(s_playback, beep, sizeof(beep));
    unlock_audio();
    ESP_LOGI(TAG, "beep");
}
