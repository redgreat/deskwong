#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "app_config.h"

static const char *TAG = "cfg";
#define NVS_NS "deskwong"

void app_config_defaults(app_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->wifi_ssid, "");
    strcpy(cfg->wifi_pass, "");
    strcpy(cfg->timezone, "Asia/Shanghai");
    strcpy(cfg->admin_user, "admin");
    strcpy(cfg->admin_pass, "admin");       // 首次启动引导修改
    strcpy(cfg->weather_location, "北京");
    strcpy(cfg->weather_key, "");
    strcpy(cfg->worktime_api_base, "");
    strcpy(cfg->worktime_token, "");
    strcpy(cfg->aiusage_api_base, "");
    strcpy(cfg->aiusage_token, "");
    strcpy(cfg->mqtt_broker, "");
    cfg->mqtt_port = 1883;
    strcpy(cfg->mqtt_user, "");
    strcpy(cfg->mqtt_pass, "");
    cfg->remind_signin_hh = 8;
    cfg->remind_signin_mm = 25;
    cfg->remind_signout_hh = 17;
    cfg->remind_signout_mm = 35;
    cfg->remind_worktime_hh = 17;
    cfg->remind_worktime_mm = 0;
    cfg->remind_enabled = true;
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
    get_str(h, "weather_key", cfg->weather_key, sizeof(cfg->weather_key));
    get_str(h, "wt_base", cfg->worktime_api_base, sizeof(cfg->worktime_api_base));
    get_str(h, "wt_token", cfg->worktime_token, sizeof(cfg->worktime_token));
    get_str(h, "ai_base", cfg->aiusage_api_base, sizeof(cfg->aiusage_api_base));
    get_str(h, "ai_token", cfg->aiusage_token, sizeof(cfg->aiusage_token));
    get_str(h, "mqtt_broker", cfg->mqtt_broker, sizeof(cfg->mqtt_broker));
    get_str(h, "mqtt_user", cfg->mqtt_user, sizeof(cfg->mqtt_user));
    get_str(h, "mqtt_pass", cfg->mqtt_pass, sizeof(cfg->mqtt_pass));
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
    nvs_close(h);
    ESP_LOGI(TAG, "config loaded");
    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t *cfg) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_str(h, "wifi_ssid", cfg->wifi_ssid);
    nvs_set_str(h, "wifi_pass", cfg->wifi_pass);
    nvs_set_str(h, "timezone", cfg->timezone);
    nvs_set_str(h, "admin_user", cfg->admin_user);
    nvs_set_str(h, "admin_pass", cfg->admin_pass);
    nvs_set_str(h, "weather_loc", cfg->weather_location);
    nvs_set_str(h, "weather_key", cfg->weather_key);
    nvs_set_str(h, "wt_base", cfg->worktime_api_base);
    nvs_set_str(h, "wt_token", cfg->worktime_token);
    nvs_set_str(h, "ai_base", cfg->aiusage_api_base);
    nvs_set_str(h, "ai_token", cfg->aiusage_token);
    nvs_set_str(h, "mqtt_broker", cfg->mqtt_broker);
    nvs_set_str(h, "mqtt_user", cfg->mqtt_user);
    nvs_set_str(h, "mqtt_pass", cfg->mqtt_pass);
    nvs_set_u16(h, "mqtt_port", cfg->mqtt_port);
    nvs_set_u8(h, "rem_si_hh", cfg->remind_signin_hh);
    nvs_set_u8(h, "rem_si_mm", cfg->remind_signin_mm);
    nvs_set_u8(h, "rem_so_hh", cfg->remind_signout_hh);
    nvs_set_u8(h, "rem_so_mm", cfg->remind_signout_mm);
    nvs_set_u8(h, "rem_wt_hh", cfg->remind_worktime_hh);
    nvs_set_u8(h, "rem_wt_mm", cfg->remind_worktime_mm);
    nvs_set_u8(h, "rem_en", cfg->remind_enabled ? 1 : 0);
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
