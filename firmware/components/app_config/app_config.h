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
    char weather_api_url[CFG_URL_MAX];    // 和风天气实时天气请求地址
    char weather_key[CFG_STR_MAX];        // 和风天气 Key
    uint16_t weather_refresh_minutes;     // 天气刷新周期，分钟
    /* 工时 */
    char worktime_api_base[CFG_URL_MAX];
    char worktime_token[CFG_STR_MAX];
    uint16_t worktime_refresh_minutes;    // 工时记录刷新周期，分钟（最小 5）
    /* AI 用量 */
    char aiusage_api_base[CFG_URL_MAX];
    char aiusage_token[CFG_STR_MAX];
    uint16_t aiusage_refresh_minutes;     // AI 用量刷新周期，分钟（最小 1）
    /* RaceBox MQTT */
    char mqtt_broker[CFG_URL_MAX];
    uint16_t mqtt_port;
    char mqtt_user[64];
    char mqtt_pass[64];
    char racebox_upload_topic[96];
    char racebox_device_name[64];   // 蓝牙设备名前缀（留空时用默认 "RaceBox"）
    char racebox_device_lock[64];   // 锁定唯一设备名/地址，非空时只连这一台
    bool racebox_auto_erase;
    /* 提醒（本地屏显 + 一声提示音） */
    uint8_t remind_signin_hh;             // 08
    uint8_t remind_signin_mm;             // 25
    uint8_t remind_signout_hh;            // 17
    uint8_t remind_signout_mm;            // 35
    uint8_t remind_worktime_hh;           // 17
    uint8_t remind_worktime_mm;           // 00
    bool remind_enabled;
    /* 语音（小智 xiaozhi-esp32）——见 doc/plan/09 */
    bool voice_enabled;                   // 总开关
    char voice_wake_word[32];             // 唤醒词
    char voice_server_url[CFG_URL_MAX];   // 小智服务端 WebSocket 地址
    char voice_token[CFG_STR_MAX];        // 设备鉴权（写入脱敏）
    char voice_device_id[64];             // 设备 ID / 激活码
    uint8_t voice_volume;                 // 0..100
    uint8_t voice_listen_mode;            // 0=唤醒词 1=长按 2=两者
    uint8_t voice_aec_level;              // 回声消除强度 0..3
    uint8_t voice_reply_seconds;          // 单次对话最长时长
    bool voice_tts_scroll;                // 回答文字在底部滚动显示
    bool voice_mcp_enabled;               // 允许服务端调用设备工具
} app_config_t;

esp_err_t app_config_init(void);
esp_err_t app_config_load(app_config_t *cfg);
esp_err_t app_config_save(const app_config_t *cfg);
esp_err_t app_config_reset(void);
void app_config_defaults(app_config_t *cfg);

#ifdef __cplusplus
}
#endif
