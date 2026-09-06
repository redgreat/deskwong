#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "codec_board.h"
#include "codec_init.h"
#include "audio_service.h"

static const char *TAG = "audio";
static esp_codec_dev_handle_t s_playback = NULL;

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
    if (s_playback == NULL) {
        ESP_LOGE(TAG, "no playback handle");
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(s_playback, &fs);
    esp_codec_dev_set_out_vol(s_playback, 60);
    ESP_LOGI(TAG, "audio init ok");
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
