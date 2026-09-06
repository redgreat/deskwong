#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_WIFI_SSID_MAX   33
#define CFG_WIFI_PASS_MAX   65
#define CFG_STR_MAX         96
#define CFG_URL_MAX         160

/* 全量应用配置（持久化到 NVS，敏感项加密存储） */
typedef struct {
    /* 网络 */
    char wifi_ssid[CFG_WIFI_SSID_MAX];
    char wifi_pass[CFG_WIFI_PASS_MAX];
    /* 系统 */
    char timezone[CFG_STR_MAX];           // IANA 时区，默认 Asia/Shanghai
    char admin_user[32];                  // 默认 admin
    char admin_pass[CFG_STR_MAX];         // 管理员密码
    /* 天气 */
    char weather_location[CFG_STR_MAX];   // 城市或经纬度
    char weather_key[CFG_STR_MAX];        // 和风天气 Key
    /* 工时 */
    char worktime_api_base[CFG_URL_MAX];
    char worktime_token[CFG_STR_MAX];
    /* AI 用量 */
    char aiusage_api_base[CFG_URL_MAX];
    char aiusage_token[CFG_STR_MAX];
    /* RaceBox MQTT */
    char mqtt_broker[CFG_URL_MAX];
    uint16_t mqtt_port;
    char mqtt_user[64];
    char mqtt_pass[64];
    /* 提醒（本地屏显 + 一声提示音） */
    uint8_t remind_signin_hh;             // 08
    uint8_t remind_signin_mm;             // 25
    uint8_t remind_signout_hh;            // 17
    uint8_t remind_signout_mm;            // 35
    uint8_t remind_worktime_hh;           // 17
    uint8_t remind_worktime_mm;           // 00
    bool remind_enabled;
} app_config_t;

esp_err_t app_config_init(void);
esp_err_t app_config_load(app_config_t *cfg);
esp_err_t app_config_save(const app_config_t *cfg);
esp_err_t app_config_reset(void);
void app_config_defaults(app_config_t *cfg);

#ifdef __cplusplus
}
#endif
