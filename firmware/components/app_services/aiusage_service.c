#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "http_util.h"
#include "aiusage_service.h"

static const char *TAG = "aiusage";
static char s_base[160] = "";
static char s_token[96] = "";

void aiusage_service_init(const char *base, const char *token) {
    if (base) strncpy(s_base, base, sizeof(s_base) - 1);
    if (token) strncpy(s_token, token, sizeof(s_token) - 1);
}

static void cpy(char *dst, size_t n, cJSON *it) {
    if (it && cJSON_IsString(it)) strncpy(dst, it->valuestring, n - 1);
    else dst[0] = 0;
}

/* 服务端下发的短标签在桌面那一行里读着像漏了字（"5时"/"周"），这里补成完整说法。
 * 放在设备侧是为了不用等服务端重新部署就能改回来。 */
static void expand_label(char *dst, size_t n) {
    const char *full = NULL;
    if (!strcmp(dst, "5时")) full = "5小时";
    else if (!strcmp(dst, "周")) full = "每周";
    if (!full || strlen(full) > n - 1) return;
    strcpy(dst, full);
}

int aiusage_service_fetch(ai_provider_t *out, int max, int *count) {
    if (s_base[0] == '\0') return 1;
    char url[320];
    size_t base_len = strlen(s_base);
    while (base_len > 0 && s_base[base_len - 1] == '/') base_len--;
    static const char endpoint[] = "/ai/usage";
    size_t endpoint_len = sizeof(endpoint) - 1;
    if (base_len >= endpoint_len &&
        memcmp(s_base + base_len - endpoint_len, endpoint, endpoint_len) == 0) {
        snprintf(url, sizeof(url), "%.*s", (int)base_len, s_base);
    } else {
        snprintf(url, sizeof(url), "%.*s%s", (int)base_len, s_base, endpoint);
    }
    cJSON *j = http_get_json(url, s_token);
    if (!j) return 1;
    cJSON *data = cJSON_GetObjectItem(j, "data");
    if (!data) { cJSON_Delete(j); return 1; }
    int n = 0;
    cJSON *apps = cJSON_GetObjectItem(data, "apps");
    if (apps && cJSON_IsArray(apps)) {
        cJSON *app;
        cJSON_ArrayForEach(app, apps) {
            cJSON *windows = cJSON_GetObjectItem(app, "windows");
            cJSON *observed = cJSON_GetObjectItem(app, "observed_at");
            if (!windows || !cJSON_IsArray(windows)) continue;
            cJSON *window;
            int app_windows = 0;
            cJSON_ArrayForEach(window, windows) {
                if (n >= max || app_windows >= 2) break;
                memset(&out[n], 0, sizeof(out[n]));
                out[n].remaining_percent = -1;
                cpy(out[n].app_id, sizeof(out[n].app_id), cJSON_GetObjectItem(app, "id"));
                cpy(out[n].app_name, sizeof(out[n].app_name), cJSON_GetObjectItem(app, "name"));
                cpy(out[n].app_status, sizeof(out[n].app_status), cJSON_GetObjectItem(app, "status"));
                /* 服务端把查不到额度的原因放在这里（只对这个 token 可见），
                 * 透传给设备后台，用户不用去翻服务器日志就能知道为什么显示不出来 */
                cpy(out[n].app_error, sizeof(out[n].app_error), cJSON_GetObjectItem(app, "error"));
                if (cJSON_IsNumber(observed) && observed->valuedouble > 0)
                    out[n].observed_at = (int64_t)observed->valuedouble;
                cpy(out[n].id, sizeof(out[n].id), cJSON_GetObjectItem(window, "id"));
                cpy(out[n].label, sizeof(out[n].label), cJSON_GetObjectItem(window, "label"));
                expand_label(out[n].label, sizeof(out[n].label));
                strncpy(out[n].name, out[n].app_name, sizeof(out[n].name) - 1);
                cJSON *minutes = cJSON_GetObjectItem(window, "window_minutes");
                cJSON *remaining = cJSON_GetObjectItem(window, "remaining_percent");
                cJSON *reset = cJSON_GetObjectItem(window, "resets_at");
                if (cJSON_IsNumber(minutes)) out[n].window_minutes = minutes->valueint;
                if (cJSON_IsNumber(remaining)) out[n].remaining_percent = remaining->valuedouble;
                if (cJSON_IsNumber(reset) && reset->valuedouble > 0)
                    out[n].resets_at = (int64_t)reset->valuedouble;
                n++; app_windows++;
            }
        }
    }
    cJSON *providers = cJSON_GetObjectItem(data, "providers");
    if (n == 0 && providers && cJSON_IsArray(providers)) {
        cJSON *it;
        cJSON_ArrayForEach(it, providers) {
            if (n >= max) break;
            memset(&out[n], 0, sizeof(out[n]));
            out[n].remaining_percent = -1;
            cpy(out[n].id, sizeof(out[n].id), cJSON_GetObjectItem(it, "id"));
            cpy(out[n].name, sizeof(out[n].name), cJSON_GetObjectItem(it, "name"));
            strncpy(out[n].app_id, "chatgpt", sizeof(out[n].app_id) - 1);
            strncpy(out[n].app_name, "ChatGPT", sizeof(out[n].app_name) - 1);
            cpy(out[n].label, sizeof(out[n].label), cJSON_GetObjectItem(it, "label"));
            expand_label(out[n].label, sizeof(out[n].label));
            cpy(out[n].display, sizeof(out[n].display), cJSON_GetObjectItem(it, "display"));
            cpy(out[n].status, sizeof(out[n].status), cJSON_GetObjectItem(it, "status"));
            cJSON *window = cJSON_GetObjectItem(it, "window_minutes");
            cJSON *remaining = cJSON_GetObjectItem(it, "remaining_percent");
            cJSON *used = cJSON_GetObjectItem(it, "used_percent");
            cJSON *reset = cJSON_GetObjectItem(it, "resets_at");
            if (cJSON_IsNumber(window)) out[n].window_minutes = window->valueint;
            if (cJSON_IsNumber(remaining)) out[n].remaining_percent = remaining->valuedouble;
            else if (cJSON_IsNumber(used)) out[n].remaining_percent = 100 - used->valuedouble;
            if (cJSON_IsNumber(reset) && reset->valuedouble > 0)
                out[n].resets_at = (int64_t)reset->valuedouble;
            n++;
        }
    }
    *count = n;
    cJSON_Delete(j);
    ESP_LOGI(TAG, "providers: %d", n);
    return 0;
}
