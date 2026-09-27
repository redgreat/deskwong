#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "codec_board.h"
#include "codec_init.h"
#include "audio_service.h"

static const char *TAG = "audio";
static esp_codec_dev_handle_t s_playback = NULL;
static esp_codec_dev_handle_t s_record = NULL;

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
}

void audio_service_set_volume(int vol) {
    if (s_playback) esp_codec_dev_set_out_vol(s_playback, vol);
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
