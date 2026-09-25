#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "app_config.h"

static const char *TAG = "cfg";
#define NVS_NS "deskwong"

/* 配置结构版本号：新增出厂默认值时 +1，老设备升级后自动补齐一次 */
#define CFG_VERSION 3

/* 一次性迁移：老设备 NVS 里已有空值会盖掉出厂默认值，这里把关键字段补回来 */
static void migrate(app_config_t *cfg, uint8_t stored_ver) {
    if (stored_ver >= CFG_VERSION) return;
    cfg->racebox_auto_erase = false;
    if (cfg->weather_api_url[0] == 0)
        strcpy(cfg->weather_api_url, "https://nc2tujbtc3.re.qweatherapi.com/v7/weather/now");
    if (cfg->weather_key[0] == 0)
        strcpy(cfg->weather_key, "3062881a9edf45679f3dc304f7a17762");
    if (cfg->weather_location[0] == 0 || strcmp(cfg->weather_location, "北京") == 0)
        strcpy(cfg->weather_location, "101120201");
    ESP_LOGW(TAG, "config migrated v%d -> v%d", stored_ver, CFG_VERSION);
}

void app_config_defaults(app_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->wifi_ssid, "");
    strcpy(cfg->wifi_pass, "");
    strcpy(cfg->timezone, "Asia/Shanghai");
    strcpy(cfg->admin_user, "admin");
    strcpy(cfg->admin_pass, "admin");       // 首次启动引导修改
    /* 和风天气（默认直连，不经过后台服务）
     * 项目凭据 ID: K95D49T6MN
     * 开发者 ID:   Q96531EFDD
     * API Host:    nc2tujbtc3.re.qweatherapi.com
     * 注意：Key 明文存在 NVS，固件泄露即 Key 泄露；如需更强的隔离，请改用自己的天气代理。 */
    strcpy(cfg->weather_location, "101120201");   // LocationID，青岛；可在后台网页改成你的城市或 "经度,纬度"
    strcpy(cfg->weather_api_url, "https://nc2tujbtc3.re.qweatherapi.com/v7/weather/now");
    strcpy(cfg->weather_key, "3062881a9edf45679f3dc304f7a17762");
    cfg->weather_refresh_minutes = 30;
    strcpy(cfg->worktime_api_base, "");
    strcpy(cfg->worktime_token, "");
    cfg->worktime_refresh_minutes = 30;
    strcpy(cfg->aiusage_api_base, "");
    strcpy(cfg->aiusage_token, "");
    cfg->aiusage_refresh_minutes = 10;
    strcpy(cfg->mqtt_broker, "");
    cfg->mqtt_port = 1883;
    strcpy(cfg->mqtt_user, "");
    strcpy(cfg->mqtt_pass, "");
    strcpy(cfg->racebox_upload_topic, "deskwong/racebox/data");
    strcpy(cfg->racebox_device_name, "RaceBox");
    strcpy(cfg->racebox_device_lock, "");
    cfg->racebox_auto_erase = false;
    cfg->remind_signin_hh = 8;
    cfg->remind_signin_mm = 25;
    cfg->remind_signout_hh = 17;
    cfg->remind_signout_mm = 35;
    cfg->remind_worktime_hh = 17;
    cfg->remind_worktime_mm = 0;
    cfg->remind_enabled = true;
    /* 语音（小智） */
    cfg->voice_enabled = false;              // 协议层未接入前默认关闭
    strcpy(cfg->voice_wake_word, "你好小智");
    strcpy(cfg->voice_server_url, "");
    strcpy(cfg->voice_token, "");
    strcpy(cfg->voice_device_id, "");
    cfg->voice_volume = 60;
    cfg->voice_listen_mode = 0;
    cfg->voice_aec_level = 2;
    cfg->voice_reply_seconds = 30;
    cfg->voice_tts_scroll = true;
    cfg->voice_mcp_enabled = true;
}

esp_err_t app_config_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

/* 读取字符串键；不存在则保持 out 的默认值不变 */
static void get_str(nvs_handle_t h, const char *key, char *out, size_t len) {
    size_t l = len;
    esp_err_t err = nvs_get_str(h, key, out, &l);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "get %s: %d", key, err);
    }
}

esp_err_t app_config_load(app_config_t *cfg) {
    app_config_defaults(cfg);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs open failed, use defaults");
        return err;
    }
    get_str(h, "wifi_ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
    get_str(h, "wifi_pass", cfg->wifi_pass, sizeof(cfg->wifi_pass));
    get_str(h, "timezone", cfg->timezone, sizeof(cfg->timezone));
    get_str(h, "admin_user", cfg->admin_user, sizeof(cfg->admin_user));
    get_str(h, "admin_pass", cfg->admin_pass, sizeof(cfg->admin_pass));
    get_str(h, "weather_loc", cfg->weather_location, sizeof(cfg->weather_location));
    get_str(h, "weather_url", cfg->weather_api_url, sizeof(cfg->weather_api_url));
    get_str(h, "weather_key", cfg->weather_key, sizeof(cfg->weather_key));
    nvs_get_u16(h, "weather_freq", &cfg->weather_refresh_minutes);
    if (cfg->weather_refresh_minutes < 5) cfg->weather_refresh_minutes = 5;
    get_str(h, "wt_base", cfg->worktime_api_base, sizeof(cfg->worktime_api_base));
    get_str(h, "wt_token", cfg->worktime_token, sizeof(cfg->worktime_token));
    nvs_get_u16(h, "wt_freq", &cfg->worktime_refresh_minutes);
    if (cfg->worktime_refresh_minutes < 5) cfg->worktime_refresh_minutes = 5;
    get_str(h, "ai_base", cfg->aiusage_api_base, sizeof(cfg->aiusage_api_base));
    get_str(h, "ai_token", cfg->aiusage_token, sizeof(cfg->aiusage_token));
    nvs_get_u16(h, "ai_freq", &cfg->aiusage_refresh_minutes);
    if (cfg->aiusage_refresh_minutes < 1) cfg->aiusage_refresh_minutes = 1;
    get_str(h, "mqtt_broker", cfg->mqtt_broker, sizeof(cfg->mqtt_broker));
    get_str(h, "mqtt_user", cfg->mqtt_user, sizeof(cfg->mqtt_user));
    get_str(h, "mqtt_pass", cfg->mqtt_pass, sizeof(cfg->mqtt_pass));
    get_str(h, "rb_topic", cfg->racebox_upload_topic, sizeof(cfg->racebox_upload_topic));
    get_str(h, "rb_device", cfg->racebox_device_name, sizeof(cfg->racebox_device_name));
    get_str(h, "rb_lock", cfg->racebox_device_lock, sizeof(cfg->racebox_device_lock));
    uint8_t rb_erase = cfg->racebox_auto_erase ? 1 : 0;
    nvs_get_u8(h, "rb_erase", &rb_erase);
    cfg->racebox_auto_erase = rb_erase != 0;
    nvs_get_u16(h, "mqtt_port", &cfg->mqtt_port);
    nvs_get_u8(h, "rem_si_hh", &cfg->remind_signin_hh);
    nvs_get_u8(h, "rem_si_mm", &cfg->remind_signin_mm);
    nvs_get_u8(h, "rem_so_hh", &cfg->remind_signout_hh);
    nvs_get_u8(h, "rem_so_mm", &cfg->remind_signout_mm);
    nvs_get_u8(h, "rem_wt_hh", &cfg->remind_worktime_hh);
    nvs_get_u8(h, "rem_wt_mm", &cfg->remind_worktime_mm);
    uint8_t ren = cfg->remind_enabled ? 1 : 0;
    nvs_get_u8(h, "rem_en", &ren);
    cfg->remind_enabled = ren != 0;
    uint8_t von = cfg->voice_enabled ? 1 : 0;
    nvs_get_u8(h, "vo_en", &von);
    cfg->voice_enabled = von != 0;
    get_str(h, "vo_word", cfg->voice_wake_word, sizeof(cfg->voice_wake_word));
    get_str(h, "vo_url", cfg->voice_server_url, sizeof(cfg->voice_server_url));
    get_str(h, "vo_token", cfg->voice_token, sizeof(cfg->voice_token));
    get_str(h, "vo_devid", cfg->voice_device_id, sizeof(cfg->voice_device_id));
    nvs_get_u8(h, "vo_vol", &cfg->voice_volume);
    nvs_get_u8(h, "vo_mode", &cfg->voice_listen_mode);
    nvs_get_u8(h, "vo_aec", &cfg->voice_aec_level);
    nvs_get_u8(h, "vo_secs", &cfg->voice_reply_seconds);
    uint8_t vs = cfg->voice_tts_scroll ? 1 : 0;
    nvs_get_u8(h, "vo_scroll", &vs);
    cfg->voice_tts_scroll = vs != 0;
    uint8_t vm = cfg->voice_mcp_enabled ? 1 : 0;
    nvs_get_u8(h, "vo_mcp", &vm);
    cfg->voice_mcp_enabled = vm != 0;
    uint8_t ver = 0;
    nvs_get_u8(h, "cfg_ver", &ver);
    nvs_close(h);

    if (ver < CFG_VERSION) {
        migrate(cfg, ver);
        /* 落盘版本号，迁移只跑一次，之后用户在网页上的修改正常生效 */
        nvs_handle_t w;
        if (nvs_open(NVS_NS, NVS_READWRITE, &w) == ESP_OK) {
            nvs_set_u8(w, "cfg_ver", CFG_VERSION);
            nvs_set_u8(w, "rb_erase", 0);
            nvs_set_str(w, "weather_loc", cfg->weather_location);
            nvs_set_str(w, "weather_url", cfg->weather_api_url);
            nvs_set_str(w, "weather_key", cfg->weather_key);
            nvs_commit(w);
            nvs_close(w);
        }
    }
    ESP_LOGI(TAG, "config loaded (ver=%d)", CFG_VERSION);
    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t *cfg) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    if ((err = nvs_set_str(h, "wifi_ssid", cfg->wifi_ssid)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "wifi_pass", cfg->wifi_pass)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "timezone", cfg->timezone)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "admin_user", cfg->admin_user)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "admin_pass", cfg->admin_pass)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "weather_loc", cfg->weather_location)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "weather_url", cfg->weather_api_url)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "weather_key", cfg->weather_key)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u16(h, "weather_freq", cfg->weather_refresh_minutes)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "wt_base", cfg->worktime_api_base)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "wt_token", cfg->worktime_token)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u16(h, "wt_freq", cfg->worktime_refresh_minutes)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "ai_base", cfg->aiusage_api_base)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "ai_token", cfg->aiusage_token)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u16(h, "ai_freq", cfg->aiusage_refresh_minutes)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "mqtt_broker", cfg->mqtt_broker)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "mqtt_user", cfg->mqtt_user)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "mqtt_pass", cfg->mqtt_pass)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "rb_topic", cfg->racebox_upload_topic)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "rb_device", cfg->racebox_device_name)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "rb_lock", cfg->racebox_device_lock)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rb_erase", cfg->racebox_auto_erase ? 1 : 0)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u16(h, "mqtt_port", cfg->mqtt_port)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_si_hh", cfg->remind_signin_hh)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_si_mm", cfg->remind_signin_mm)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_so_hh", cfg->remind_signout_hh)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_so_mm", cfg->remind_signout_mm)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_wt_hh", cfg->remind_worktime_hh)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_wt_mm", cfg->remind_worktime_mm)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "rem_en", cfg->remind_enabled ? 1 : 0)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_en", cfg->voice_enabled ? 1 : 0)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "vo_word", cfg->voice_wake_word)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "vo_url", cfg->voice_server_url)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "vo_token", cfg->voice_token)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_str(h, "vo_devid", cfg->voice_device_id)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_vol", cfg->voice_volume)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_mode", cfg->voice_listen_mode)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_aec", cfg->voice_aec_level)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_secs", cfg->voice_reply_seconds)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_scroll", cfg->voice_tts_scroll ? 1 : 0)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "vo_mcp", cfg->voice_mcp_enabled ? 1 : 0)) != ESP_OK) { nvs_close(h); return err; }
    if ((err = nvs_set_u8(h, "cfg_ver", CFG_VERSION)) != ESP_OK) { nvs_close(h); return err; }
    err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "config saved");
    return err;
}

esp_err_t app_config_reset(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    return err;
}
