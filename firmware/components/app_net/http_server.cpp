#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "wifi_sta.h"
#include "http_server.h"
#include "weather_service.h"
#include "worktime_service.h"
#include "aiusage_service.h"
#include "net_scheduler.h"
#include "racebox_service.h"
#include "app_mqtt.h"

static const char *TAG = "httpd";
static app_config_t *s_cfg = NULL;
static char s_token[33] = {0};

static void gen_token(void) {
    uint32_t a = esp_random(), b = esp_random();
    snprintf(s_token, sizeof(s_token), "%08lx%08lx", (unsigned long)a, (unsigned long)b);
}

/* ---------- 工具 ---------- */
static esp_err_t send_json(httpd_req_t *req, int code, cJSON *obj) {
    char *body = cJSON_PrintUnformatted(obj);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, body ? body : "{}");
    if (body) free(body);
    return err;
}

static esp_err_t send_err(httpd_req_t *req, int code, const char *msg) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "code", code);
    cJSON_AddStringToObject(o, "message", msg);
    esp_err_t err = send_json(req, code, o);
    cJSON_Delete(o);
    return err;
}

/* 从请求头取 token（Authorization: Bearer xxx 或 X-Token: xxx） */
static bool authed(httpd_req_t *req) {
    if (s_token[0] == '\0') return false;
    char buf[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", buf, sizeof(buf)) == ESP_OK) {
        if (strstr(buf, "Bearer ") == buf) {
            return strcmp(buf + 7, s_token) == 0;
        }
    }
    if (httpd_req_get_hdr_value_str(req, "X-Token", buf, sizeof(buf)) == ESP_OK) {
        return strcmp(buf, s_token) == 0;
    }
    return false;
}

/* ---------- 处理函数 ---------- */
static esp_err_t health_handler(httpd_req_t *req) {
    char ip[16] = {0};
    wifi_get_ip(ip, sizeof(ip));
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "code", 0);
    cJSON_AddStringToObject(o, "message", "ok");
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "status", "ok");
    cJSON_AddBoolToObject(d, "wifi_connected", wifi_is_connected());
    cJSON_AddBoolToObject(d, "ap_mode", wifi_is_ap_mode());
    cJSON_AddStringToObject(d, "ip", ip);
    cJSON_AddNumberToObject(d, "mqtt_status", app_mqtt_status());
    cJSON_AddStringToObject(d, "mqtt_message", app_mqtt_status_text());
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    cJSON_AddStringToObject(d, "firmware_version", app->version);
    cJSON_AddStringToObject(d, "build_date", app->date);
    cJSON_AddStringToObject(d, "build_time", app->time);
    cJSON_AddStringToObject(d, "running_partition", running ? running->label : "unknown");
    cJSON_AddNumberToObject(d, "running_address", running ? running->address : 0);
    cJSON_AddItemToObject(o, "data", d);
    esp_err_t err = send_json(req, 0, o);
    cJSON_Delete(o);
    return err;
}

static esp_err_t login_handler(httpd_req_t *req) {
    char body[256] = {0};
    int rlen = httpd_req_recv(req, body, sizeof(body) - 1);
    if (rlen <= 0) return send_err(req, 400, "bad request");
    body[rlen] = 0;
    cJSON *j = cJSON_Parse(body);
    if (!j) return send_err(req, 400, "bad json");
    cJSON *u = cJSON_GetObjectItem(j, "user");
    cJSON *p = cJSON_GetObjectItem(j, "pass");
    if (!u || !p) { cJSON_Delete(j); return send_err(req, 400, "missing user/pass"); }

    if (strcmp(u->valuestring, s_cfg->admin_user) == 0 &&
        strcmp(p->valuestring, s_cfg->admin_pass) == 0) {
        gen_token();
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "code", 0);
        cJSON_AddStringToObject(o, "message", "ok");
        cJSON_AddStringToObject(o, "token", s_token);
        esp_err_t err = send_json(req, 0, o);
        cJSON_Delete(o);
        cJSON_Delete(j);
        return err;
    }
    cJSON_Delete(j);
    return send_err(req, 401, "invalid credentials");
}

static esp_err_t config_get_handler(httpd_req_t *req) {
    if (!authed(req)) return send_err(req, 401, "unauthorized");
    app_config_t stored = *s_cfg;
    app_config_load(&stored);
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "wifi_ssid", stored.wifi_ssid);
    cJSON_AddStringToObject(d, "wifi_pass", "******");   // 脱敏
    cJSON_AddStringToObject(d, "timezone", stored.timezone);
    cJSON_AddStringToObject(d, "admin_user", stored.admin_user);
    cJSON_AddStringToObject(d, "admin_pass", "******");
    cJSON_AddStringToObject(d, "weather_location", stored.weather_location);
    cJSON_AddStringToObject(d, "weather_api_url", stored.weather_api_url);
    cJSON_AddStringToObject(d, "weather_key", stored.weather_key[0] ? "******" : "");
    cJSON_AddNumberToObject(d, "weather_refresh_minutes", stored.weather_refresh_minutes);
    cJSON_AddStringToObject(d, "worktime_api_base", stored.worktime_api_base);
    cJSON_AddStringToObject(d, "worktime_token", stored.worktime_token[0] ? "******" : "");
    cJSON_AddNumberToObject(d, "worktime_refresh_minutes", stored.worktime_refresh_minutes);
    cJSON_AddStringToObject(d, "aiusage_api_base", stored.aiusage_api_base);
    cJSON_AddStringToObject(d, "aiusage_token", stored.aiusage_token[0] ? "******" : "");
    cJSON_AddNumberToObject(d, "aiusage_refresh_minutes", stored.aiusage_refresh_minutes);
    cJSON_AddStringToObject(d, "mqtt_broker", stored.mqtt_broker);
    cJSON_AddNumberToObject(d, "mqtt_port", stored.mqtt_port);
    cJSON_AddStringToObject(d, "mqtt_user", stored.mqtt_user);
    cJSON_AddStringToObject(d, "mqtt_pass", stored.mqtt_pass[0] ? "******" : "");
    cJSON_AddStringToObject(d, "racebox_upload_topic", stored.racebox_upload_topic);
    cJSON_AddStringToObject(d, "racebox_device_name", stored.racebox_device_name);
    cJSON_AddStringToObject(d, "racebox_device_lock", stored.racebox_device_lock);
    cJSON_AddBoolToObject(d, "racebox_auto_erase", stored.racebox_auto_erase);
    cJSON_AddNumberToObject(d, "remind_signin_hh", stored.remind_signin_hh);
    cJSON_AddNumberToObject(d, "remind_signin_mm", stored.remind_signin_mm);
    cJSON_AddNumberToObject(d, "remind_signout_hh", stored.remind_signout_hh);
    cJSON_AddNumberToObject(d, "remind_signout_mm", stored.remind_signout_mm);
    cJSON_AddNumberToObject(d, "remind_worktime_hh", stored.remind_worktime_hh);
    cJSON_AddNumberToObject(d, "remind_worktime_mm", stored.remind_worktime_mm);
    cJSON_AddBoolToObject(d, "remind_enabled", stored.remind_enabled);
    cJSON_AddBoolToObject(d, "voice_enabled", stored.voice_enabled);
    cJSON_AddStringToObject(d, "voice_wake_word", stored.voice_wake_word);
    cJSON_AddStringToObject(d, "voice_server_url", stored.voice_server_url);
    cJSON_AddStringToObject(d, "voice_token", stored.voice_token[0] ? "******" : "");
    cJSON_AddStringToObject(d, "voice_device_id", stored.voice_device_id);
    cJSON_AddNumberToObject(d, "voice_volume", stored.voice_volume);
    cJSON_AddNumberToObject(d, "voice_listen_mode", stored.voice_listen_mode);
    cJSON_AddNumberToObject(d, "voice_aec_level", stored.voice_aec_level);
    cJSON_AddNumberToObject(d, "voice_reply_seconds", stored.voice_reply_seconds);
    cJSON_AddBoolToObject(d, "voice_tts_scroll", stored.voice_tts_scroll);
    cJSON_AddBoolToObject(d, "voice_mcp_enabled", stored.voice_mcp_enabled);

    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "code", 0);
    cJSON_AddItemToObject(o, "data", d);
    esp_err_t err = send_json(req, 0, o);
    cJSON_Delete(o);
    return err;
}

static void set_str_field(cJSON *j, const char *key, char *dst, size_t len) {
    cJSON *it = cJSON_GetObjectItem(j, key);
    if (it && cJSON_IsString(it)) {
        strncpy(dst, it->valuestring, len - 1);
        dst[len - 1] = 0;
    }
}

static esp_err_t config_put_handler(httpd_req_t *req) {
    if (!authed(req)) return send_err(req, 401, "unauthorized");
    app_config_t pending = *s_cfg;
    app_config_load(&pending);
    if (req->content_len <= 0 || req->content_len > 8192) return send_err(req, 400, "invalid config size");
    char *body = (char *)calloc(1, req->content_len + 1);
    if (!body) return send_err(req, 500, "out of memory");
    int received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, body + received, req->content_len - received);
        if (n <= 0) { free(body); return send_err(req, 400, "incomplete config"); }
        received += n;
    }
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) return send_err(req, 400, "bad json");

    /* 仅更新传入的字段；"******" 视为未修改 */
    cJSON *it;
    set_str_field(j, "wifi_ssid", pending.wifi_ssid, sizeof(pending.wifi_ssid));
    it = cJSON_GetObjectItem(j, "wifi_pass");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.wifi_pass, it->valuestring, sizeof(pending.wifi_pass) - 1);
        pending.wifi_pass[sizeof(pending.wifi_pass) - 1] = 0;
    }
    set_str_field(j, "timezone", pending.timezone, sizeof(pending.timezone));
    set_str_field(j, "admin_user", pending.admin_user, sizeof(pending.admin_user));
    it = cJSON_GetObjectItem(j, "admin_pass");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.admin_pass, it->valuestring, sizeof(pending.admin_pass) - 1);
        pending.admin_pass[sizeof(pending.admin_pass) - 1] = 0;
    }
    set_str_field(j, "weather_location", pending.weather_location, sizeof(pending.weather_location));
    set_str_field(j, "weather_api_url", pending.weather_api_url, sizeof(pending.weather_api_url));
    it = cJSON_GetObjectItem(j, "weather_key");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.weather_key, it->valuestring, sizeof(pending.weather_key) - 1);
        pending.weather_key[sizeof(pending.weather_key) - 1] = 0;
    }
    it = cJSON_GetObjectItem(j, "weather_refresh_minutes");
    if (it && cJSON_IsNumber(it)) {
        int minutes = it->valueint;
        if (minutes < 5) minutes = 5;
        if (minutes > 1440) minutes = 1440;
        pending.weather_refresh_minutes = (uint16_t)minutes;
    }
    set_str_field(j, "worktime_api_base", pending.worktime_api_base, sizeof(pending.worktime_api_base));
    it = cJSON_GetObjectItem(j, "worktime_token");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.worktime_token, it->valuestring, sizeof(pending.worktime_token) - 1);
        pending.worktime_token[sizeof(pending.worktime_token) - 1] = 0;
    }
    it = cJSON_GetObjectItem(j, "worktime_refresh_minutes");
    if (it && cJSON_IsNumber(it)) {
        int minutes = it->valueint;
        if (minutes < 5) minutes = 5;
        if (minutes > 1440) minutes = 1440;
        pending.worktime_refresh_minutes = (uint16_t)minutes;
    }
    set_str_field(j, "aiusage_api_base", pending.aiusage_api_base, sizeof(pending.aiusage_api_base));
    it = cJSON_GetObjectItem(j, "aiusage_token");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.aiusage_token, it->valuestring, sizeof(pending.aiusage_token) - 1);
        pending.aiusage_token[sizeof(pending.aiusage_token) - 1] = 0;
    }
    it = cJSON_GetObjectItem(j, "aiusage_refresh_minutes");
    if (it && cJSON_IsNumber(it)) {
        int minutes = it->valueint;
        if (minutes < 1) minutes = 1;
        if (minutes > 1440) minutes = 1440;
        pending.aiusage_refresh_minutes = (uint16_t)minutes;
    }
    set_str_field(j, "mqtt_broker", pending.mqtt_broker, sizeof(pending.mqtt_broker));
    set_str_field(j, "racebox_upload_topic", pending.racebox_upload_topic, sizeof(pending.racebox_upload_topic));
    set_str_field(j, "racebox_device_name", pending.racebox_device_name, sizeof(pending.racebox_device_name));
    set_str_field(j, "racebox_device_lock", pending.racebox_device_lock, sizeof(pending.racebox_device_lock));
    it = cJSON_GetObjectItem(j, "racebox_auto_erase");
    if (it && cJSON_IsBool(it)) pending.racebox_auto_erase = cJSON_IsTrue(it);
    set_str_field(j, "mqtt_user", pending.mqtt_user, sizeof(pending.mqtt_user));
    it = cJSON_GetObjectItem(j, "mqtt_pass");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.mqtt_pass, it->valuestring, sizeof(pending.mqtt_pass) - 1);
        pending.mqtt_pass[sizeof(pending.mqtt_pass) - 1] = 0;
    }
    it = cJSON_GetObjectItem(j, "mqtt_port");
    if (it && cJSON_IsNumber(it)) {
        if (it->valueint < 1 || it->valueint > 65535) {
            cJSON_Delete(j);
            return send_err(req, 400, "MQTT 端口必须在 1 到 65535 之间");
        }
        pending.mqtt_port = (uint16_t)it->valueint;
    }
    it = cJSON_GetObjectItem(j, "remind_signin_hh");
    if (it && cJSON_IsNumber(it)) pending.remind_signin_hh = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signin_mm");
    if (it && cJSON_IsNumber(it)) pending.remind_signin_mm = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signout_hh");
    if (it && cJSON_IsNumber(it)) pending.remind_signout_hh = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signout_mm");
    if (it && cJSON_IsNumber(it)) pending.remind_signout_mm = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_worktime_hh");
    if (it && cJSON_IsNumber(it)) pending.remind_worktime_hh = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_worktime_mm");
    if (it && cJSON_IsNumber(it)) pending.remind_worktime_mm = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_enabled");
    if (it && cJSON_IsBool(it)) pending.remind_enabled = cJSON_IsTrue(it);
    it = cJSON_GetObjectItem(j, "voice_enabled");
    if (it && cJSON_IsBool(it)) pending.voice_enabled = cJSON_IsTrue(it);
    set_str_field(j, "voice_wake_word", pending.voice_wake_word, sizeof(pending.voice_wake_word));
    set_str_field(j, "voice_server_url", pending.voice_server_url, sizeof(pending.voice_server_url));
    it = cJSON_GetObjectItem(j, "voice_token");
    if (it && cJSON_IsString(it) && it->valuestring[0] && strcmp(it->valuestring, "******") != 0) {
        strncpy(pending.voice_token, it->valuestring, sizeof(pending.voice_token) - 1);
        pending.voice_token[sizeof(pending.voice_token) - 1] = 0;
    }
    set_str_field(j, "voice_device_id", pending.voice_device_id, sizeof(pending.voice_device_id));
    it = cJSON_GetObjectItem(j, "voice_volume");
    if (it && cJSON_IsNumber(it)) pending.voice_volume = (uint8_t)(it->valueint > 100 ? 100 : (it->valueint < 0 ? 0 : it->valueint));
    it = cJSON_GetObjectItem(j, "voice_listen_mode");
    if (it && cJSON_IsNumber(it)) pending.voice_listen_mode = (uint8_t)(it->valueint > 2 ? 2 : it->valueint);
    it = cJSON_GetObjectItem(j, "voice_aec_level");
    if (it && cJSON_IsNumber(it)) pending.voice_aec_level = (uint8_t)(it->valueint > 3 ? 3 : it->valueint);
    it = cJSON_GetObjectItem(j, "voice_reply_seconds");
    if (it && cJSON_IsNumber(it)) pending.voice_reply_seconds = (uint8_t)(it->valueint > 120 ? 120 : it->valueint);
    it = cJSON_GetObjectItem(j, "voice_tts_scroll");
    if (it && cJSON_IsBool(it)) pending.voice_tts_scroll = cJSON_IsTrue(it);
    it = cJSON_GetObjectItem(j, "voice_mcp_enabled");
    if (it && cJSON_IsBool(it)) pending.voice_mcp_enabled = cJSON_IsTrue(it);

    char mqtt_check[96];
    if (app_mqtt_test_config(pending.mqtt_broker, pending.mqtt_port,
                             pending.mqtt_user, pending.mqtt_pass,
                             10000, mqtt_check, sizeof(mqtt_check)) != 0) {
        cJSON_Delete(j);
        return send_err(req, 400, mqtt_check);
    }
    cJSON_Delete(j);
    if (app_config_save(&pending) != ESP_OK) return send_err(req, 500, "配置保存失败");
    return send_err(req, 0, pending.mqtt_broker[0]
                              ? "MQTT 校验成功，已保存到设备 Flash，重启后生效"
                              : "MQTT 已关闭，配置已保存到设备 Flash，重启后生效");
}

static esp_err_t restart_handler(httpd_req_t *req) {
    if (!authed(req)) return send_err(req, 401, "unauthorized");
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "code", 0);
    cJSON_AddStringToObject(o, "message", "restarting");
    esp_err_t err = send_json(req, 0, o);
    cJSON_Delete(o);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return err;
}

static esp_err_t factory_reset_handler(httpd_req_t *req) {
    if (!authed(req)) return send_err(req, 401, "unauthorized");
    app_config_reset();
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "code", 0);
    cJSON_AddStringToObject(o, "message", "factory reset, restarting");
    esp_err_t err = send_json(req, 0, o);
    cJSON_Delete(o);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return err;
}

static esp_err_t logs_handler(httpd_req_t *req) {
    if (!authed(req)) return send_err(req, 401, "unauthorized");
    /* 日志脱敏：仅返回最近运行摘要，不含密钥/经纬度明细 */
    char ip[16] = {0};
    wifi_get_ip(ip, sizeof(ip));
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "code", 0);
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "ip", ip);
    cJSON_AddBoolToObject(d, "wifi_connected", wifi_is_connected());
    cJSON_AddStringToObject(d, "firmware", "deskwong-0.1.0");
    cJSON_AddItemToObject(o, "data", d);
    esp_err_t err = send_json(req, 0, o);
    cJSON_Delete(o);
    return err;
}

static esp_err_t ota_handler(httpd_req_t *req) {
    if (!authed(req)) return send_err(req, 401, "unauthorized");
    int total = 0;
    char cl[16] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Length", cl, sizeof(cl)) == ESP_OK) {
        total = atoi(cl);
    }
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) return send_err(req, 500, "no ota partition");

    esp_ota_handle_t handle;
    esp_err_t err = esp_ota_begin(update, total > 0 ? total : OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) return send_err(req, 500, "ota begin failed");

    char *buf = (char *)malloc(1024);
    if (!buf) {
        esp_ota_abort(handle);
        return send_err(req, 500, "out of memory");
    }
    int received = 0;
    while (1) {
        int r = httpd_req_recv(req, buf, 1024);
        if (r <= 0) break;
        if (esp_ota_write(handle, buf, r) != ESP_OK) {
            free(buf);
            esp_ota_abort(handle);
            return send_err(req, 500, "ota write failed");
        }
        received += r;
    }
    free(buf);
    err = esp_ota_end(handle);
    if (err != ESP_OK) return send_err(req, 500, "ota end failed");
    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) return send_err(req, 500, "set boot partition failed");

    ESP_LOGI(TAG, "OTA ok, %d bytes, restarting", received);
    send_err(req, 0, "ota ok, restarting");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

static esp_err_t static_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET %s, stack free=%u", req->uri,
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    /* 挂载 SPIFFS，读取 /www/<path>；默认 index.html */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    static bool mounted = false;
    if (!mounted) {
        if (esp_vfs_spiffs_register(&conf) == ESP_OK) mounted = true;
    }
    const char *uri = req->uri;
    char path[272];
    if (strcmp(uri, "/") == 0 || strcmp(uri, "") == 0) {
        snprintf(path, sizeof(path), "/spiffs/www/index.html");
    } else {
        size_t uri_len = strcspn(uri, "?#");
        if (uri_len == 0 || uri_len > 220 || strstr(uri, "..")) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad uri");
        }
        int n = snprintf(path, sizeof(path), "/spiffs/www%.*s", (int)uri_len, uri);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "uri too long");
        }
    }
    FILE *f = fopen(path, "r");
    if (!f) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");

    const char *ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0) httpd_resp_set_type(req, "text/html");
        else if (strcmp(ext, ".js") == 0) httpd_resp_set_type(req, "application/javascript");
        else if (strcmp(ext, ".css") == 0) httpd_resp_set_type(req, "text/css");
        else if (strcmp(ext, ".svg") == 0) httpd_resp_set_type(req, "image/svg+xml");
        else if (strcmp(ext, ".json") == 0) httpd_resp_set_type(req, "application/json");
        else if (strcmp(ext, ".png") == 0) httpd_resp_set_type(req, "image/png");
        else if (strcmp(ext, ".ico") == 0) httpd_resp_set_type(req, "image/x-icon");
    }
    char *chunk = (char *)malloc(1024);
    if (!chunk) {
        fclose(f);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    size_t n;
    while ((n = fread(chunk, 1, 1024, f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) break;
    }
    httpd_resp_send_chunk(req, NULL, 0);
    free(chunk);
    fclose(f);
    return ESP_OK;
}

esp_err_t http_server_start(app_config_t *cfg) {
    s_cfg = cfg;
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.max_uri_handlers = 16;
    conf.stack_size = 12 * 1024;
    /* SPIFFS/NVS/OTA disable flash cache. Their caller's stack must be in DRAM. */
    conf.task_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    conf.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "http server start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t uri;
    memset(&uri, 0, sizeof(uri));
    uri.uri = "/api/health";      uri.method = HTTP_GET;    uri.handler = health_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/auth/login";  uri.method = HTTP_POST;   uri.handler = login_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/config";      uri.method = HTTP_GET;    uri.handler = config_get_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/config";      uri.method = HTTP_PUT;    uri.handler = config_put_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/system/restart";       uri.method = HTTP_POST; uri.handler = restart_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/system/factory-reset"; uri.method = HTTP_POST; uri.handler = factory_reset_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/system/logs";          uri.method = HTTP_GET;  uri.handler = logs_handler;
    httpd_register_uri_handler(server, &uri);
    uri.uri = "/api/ota";                  uri.method = HTTP_POST; uri.handler = ota_handler;
    httpd_register_uri_handler(server, &uri);
    /* 静态资源兜底 */
    uri.uri = "/*";               uri.method = HTTP_GET;    uri.handler = static_handler;
    httpd_register_uri_handler(server, &uri);

    ESP_LOGI(TAG, "http server started");
    return ESP_OK;
}
