#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "http_util.h"

static const char *TAG = "http";
#define HTTP_BUF_SIZE 16384
static char g_buf[HTTP_BUF_SIZE];

cJSON *http_get_json(const char *url, const char *bearer_token) {
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
        .buffer_size = HTTP_BUF_SIZE,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "init fail: %s", url);
        return NULL;
    }
    if (bearer_token && bearer_token[0]) {
        char hdr[256];
        snprintf(hdr, sizeof(hdr), "Bearer %s", bearer_token);
        esp_http_client_set_header(client, "Authorization", hdr);
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open fail: %s", url);
        esp_http_client_cleanup(client);
        return NULL;
    }
    int total = 0;
    int len;
    while (total < (int)sizeof(g_buf) - 1) {
        len = esp_http_client_read(client, (char *)g_buf + total, sizeof(g_buf) - total - 1);
        if (len <= 0) break;
        total += len;
    }
    g_buf[total] = 0;
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (total <= 0) {
        ESP_LOGW(TAG, "empty body: %s", url);
        return NULL;
    }
    return cJSON_Parse(g_buf);
}
