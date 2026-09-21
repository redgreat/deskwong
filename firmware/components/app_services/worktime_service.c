#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "http_util.h"
#include "worktime_service.h"

static const char *TAG = "worktime";
static char s_base[160] = "";
static char s_token[96] = "";

void worktime_service_init(const char *base, const char *token) {
    if (base) strncpy(s_base, base, sizeof(s_base) - 1);
    if (token) strncpy(s_token, token, sizeof(s_token) - 1);
}

int worktime_service_fetch(int year, int month, worktime_summary_t *out) {
    if (s_base[0] == '\0') return 1;
    char url[320];
    snprintf(url, sizeof(url), "%s/worktime/summary?year=%d&month=%d", s_base, year, month);
    cJSON *j = http_get_json(url, s_token);
    if (!j) return 1;
    cJSON *data = cJSON_GetObjectItem(j, "data");
    if (!data) { cJSON_Delete(j); return 1; }
    memset(out, 0, sizeof(*out));
    cJSON *r = cJSON_GetObjectItem(data, "total_recorded_hours");
    cJSON *e = cJSON_GetObjectItem(data, "total_expected_hours");
    out->year = year;
    out->month = month;
    out->recorded_hours = r ? (float)cJSON_GetNumberValue(r) : 0;
    out->expected_hours = e ? (float)cJSON_GetNumberValue(e) : 0;
    out->ratio = out->expected_hours > 0 ? out->recorded_hours / out->expected_hours : 0;
    cJSON *days = cJSON_GetObjectItem(data, "days");
    if (cJSON_IsArray(days)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, days) {
            cJSON *day = cJSON_GetObjectItem(item, "day");
            cJSON *hours = cJSON_GetObjectItem(item, "recorded_hours");
            if (!hours) hours = cJSON_GetObjectItem(item, "hours");
            int d = day ? day->valueint : 0;
            if (!d) {
                cJSON *date = cJSON_GetObjectItem(item, "date");
                if (cJSON_IsString(date) && strlen(date->valuestring) >= 10)
                    d = atoi(date->valuestring + 8);
            }
            if (d >= 1 && d <= 31 && hours) out->daily_hours[d] = (float)cJSON_GetNumberValue(hours);
        }
    }
    cJSON_Delete(j);
    ESP_LOGI(TAG, "%d-%02d: %.1f/%.1f", year, month, out->recorded_hours, out->expected_hours);
    return 0;
}
