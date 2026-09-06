#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "wifi_sta.h"
#include "http_server.h"

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
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "wifi_ssid", s_cfg->wifi_ssid);
    cJSON_AddStringToObject(d, "wifi_pass", "******");   // 脱敏
    cJSON_AddStringToObject(d, "timezone", s_cfg->timezone);
    cJSON_AddStringToObject(d, "admin_user", s_cfg->admin_user);
    cJSON_AddStringToObject(d, "admin_pass", "******");
    cJSON_AddStringToObject(d, "weather_location", s_cfg->weather_location);
    cJSON_AddStringToObject(d, "weather_key", s_cfg->weather_key[0] ? "******" : "");
    cJSON_AddStringToObject(d, "worktime_api_base", s_cfg->worktime_api_base);
    cJSON_AddStringToObject(d, "worktime_token", s_cfg->worktime_token[0] ? "******" : "");
    cJSON_AddStringToObject(d, "aiusage_api_base", s_cfg->aiusage_api_base);
    cJSON_AddStringToObject(d, "aiusage_token", s_cfg->aiusage_token[0] ? "******" : "");
    cJSON_AddStringToObject(d, "mqtt_broker", s_cfg->mqtt_broker);
    cJSON_AddNumberToObject(d, "mqtt_port", s_cfg->mqtt_port);
    cJSON_AddStringToObject(d, "mqtt_user", s_cfg->mqtt_user);
    cJSON_AddStringToObject(d, "mqtt_pass", s_cfg->mqtt_pass[0] ? "******" : "");
    cJSON_AddNumberToObject(d, "remind_signin_hh", s_cfg->remind_signin_hh);
    cJSON_AddNumberToObject(d, "remind_signin_mm", s_cfg->remind_signin_mm);
    cJSON_AddNumberToObject(d, "remind_signout_hh", s_cfg->remind_signout_hh);
    cJSON_AddNumberToObject(d, "remind_signout_mm", s_cfg->remind_signout_mm);
    cJSON_AddNumberToObject(d, "remind_worktime_hh", s_cfg->remind_worktime_hh);
    cJSON_AddNumberToObject(d, "remind_worktime_mm", s_cfg->remind_worktime_mm);
    cJSON_AddBoolToObject(d, "remind_enabled", s_cfg->remind_enabled);

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
    char body[2048] = {0};
    int rlen = httpd_req_recv(req, body, sizeof(body) - 1);
    if (rlen <= 0) return send_err(req, 400, "bad request");
    body[rlen] = 0;
    cJSON *j = cJSON_Parse(body);
    if (!j) return send_err(req, 400, "bad json");

    /* 仅更新传入的字段；"******" 视为未修改 */
    cJSON *it;
    it = cJSON_GetObjectItem(j, "wifi_ssid");
    if (it && cJSON_IsString(it)) strncpy(s_cfg->wifi_ssid, it->valuestring, sizeof(s_cfg->wifi_ssid) - 1);
    it = cJSON_GetObjectItem(j, "wifi_pass");
    if (it && cJSON_IsString(it) && strcmp(it->valuestring, "******") != 0)
        strncpy(s_cfg->wifi_pass, it->valuestring, sizeof(s_cfg->wifi_pass) - 1);
    set_str_field(j, "timezone", s_cfg->timezone, sizeof(s_cfg->timezone));
    set_str_field(j, "admin_user", s_cfg->admin_user, sizeof(s_cfg->admin_user));
    it = cJSON_GetObjectItem(j, "admin_pass");
    if (it && cJSON_IsString(it) && strcmp(it->valuestring, "******") != 0)
        strncpy(s_cfg->admin_pass, it->valuestring, sizeof(s_cfg->admin_pass) - 1);
    set_str_field(j, "weather_location", s_cfg->weather_location, sizeof(s_cfg->weather_location));
    it = cJSON_GetObjectItem(j, "weather_key");
    if (it && cJSON_IsString(it) && strcmp(it->valuestring, "******") != 0)
        strncpy(s_cfg->weather_key, it->valuestring, sizeof(s_cfg->weather_key) - 1);
    set_str_field(j, "worktime_api_base", s_cfg->worktime_api_base, sizeof(s_cfg->worktime_api_base));
    it = cJSON_GetObjectItem(j, "worktime_token");
    if (it && cJSON_IsString(it) && strcmp(it->valuestring, "******") != 0)
        strncpy(s_cfg->worktime_token, it->valuestring, sizeof(s_cfg->worktime_token) - 1);
    set_str_field(j, "aiusage_api_base", s_cfg->aiusage_api_base, sizeof(s_cfg->aiusage_api_base));
    it = cJSON_GetObjectItem(j, "aiusage_token");
    if (it && cJSON_IsString(it) && strcmp(it->valuestring, "******") != 0)
        strncpy(s_cfg->aiusage_token, it->valuestring, sizeof(s_cfg->aiusage_token) - 1);
    set_str_field(j, "mqtt_broker", s_cfg->mqtt_broker, sizeof(s_cfg->mqtt_broker));
    set_str_field(j, "mqtt_user", s_cfg->mqtt_user, sizeof(s_cfg->mqtt_user));
    it = cJSON_GetObjectItem(j, "mqtt_pass");
    if (it && cJSON_IsString(it) && strcmp(it->valuestring, "******") != 0)
        strncpy(s_cfg->mqtt_pass, it->valuestring, sizeof(s_cfg->mqtt_pass) - 1);
    it = cJSON_GetObjectItem(j, "mqtt_port");
    if (it && cJSON_IsNumber(it)) s_cfg->mqtt_port = (uint16_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signin_hh");
    if (it && cJSON_IsNumber(it)) s_cfg->remind_signin_hh = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signin_mm");
    if (it && cJSON_IsNumber(it)) s_cfg->remind_signin_mm = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signout_hh");
    if (it && cJSON_IsNumber(it)) s_cfg->remind_signout_hh = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_signout_mm");
    if (it && cJSON_IsNumber(it)) s_cfg->remind_signout_mm = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_worktime_hh");
    if (it && cJSON_IsNumber(it)) s_cfg->remind_worktime_hh = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_worktime_mm");
    if (it && cJSON_IsNumber(it)) s_cfg->remind_worktime_mm = (uint8_t)it->valueint;
    it = cJSON_GetObjectItem(j, "remind_enabled");
    if (it && cJSON_IsBool(it)) s_cfg->remind_enabled = cJSON_IsTrue(it);

    cJSON_Delete(j);
    app_config_save(s_cfg);
    return send_err(req, 0, "ok");
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

    char buf[1024];
    int received = 0;
    while (1) {
        int r = httpd_req_recv(req, buf, sizeof(buf));
        if (r <= 0) break;
        if (esp_ota_write(handle, buf, r) != ESP_OK) {
            esp_ota_abort(handle);
            return send_err(req, 500, "ota write failed");
        }
        received += r;
    }
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
        int n = snprintf(path, sizeof(path), "/spiffs/www%s", uri);
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
    char chunk[1024];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) break;
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(f);
    return ESP_OK;
}

esp_err_t http_server_start(app_config_t *cfg) {
    s_cfg = cfg;
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.max_uri_handlers = 16;
    conf.stack_size = 8192;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &conf));

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
