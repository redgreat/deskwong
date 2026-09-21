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

int aiusage_service_fetch(ai_provider_t *out, int max, int *count) {
    if (s_base[0] == '\0') return 1;
    char url[320];
    snprintf(url, sizeof(url), "%s/ai/usage", s_base);
    cJSON *j = http_get_json(url, s_token);
    if (!j) return 1;
    cJSON *data = cJSON_GetObjectItem(j, "data");
    if (!data) { cJSON_Delete(j); return 1; }
    cJSON *providers = cJSON_GetObjectItem(data, "providers");
    int n = 0;
    if (providers && cJSON_IsArray(providers)) {
        cJSON *it;
        cJSON_ArrayForEach(it, providers) {
            if (n >= max) break;
            memset(&out[n], 0, sizeof(out[n]));
            out[n].remaining_percent = -1;
            cpy(out[n].id, sizeof(out[n].id), cJSON_GetObjectItem(it, "id"));
            cpy(out[n].name, sizeof(out[n].name), cJSON_GetObjectItem(it, "name"));
            cpy(out[n].label, sizeof(out[n].label), cJSON_GetObjectItem(it, "label"));
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
