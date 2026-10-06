#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "cJSON.h"
#include "http_util.h"
#include "voice_context_service.h"

static const char *TAG = "voice_ctx";
static char s_url[256];
static char s_token[96];
static char s_device_id[64];

void voice_context_service_init(const char *api_base, const char *token, const char *device_id) {
    s_url[0] = s_token[0] = s_device_id[0] = 0;
    if (!api_base || !api_base[0] || !device_id || !device_id[0]) return;
    char base[192];
    snprintf(base, sizeof(base), "%s", api_base);
    size_t n = strlen(base);
    while (n && base[n - 1] == '/') base[--n] = 0;
    const char *suffixes[] = {"/worktime/summary", "/worktime", "/ai/usage"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        size_t sl = strlen(suffixes[i]);
        if (n >= sl && strcmp(base + n - sl, suffixes[i]) == 0) {
            base[n - sl] = 0;
            break;
        }
    }
    snprintf(s_url, sizeof(s_url), "%s/voice/context/report", base);
    snprintf(s_token, sizeof(s_token), "%s", token ? token : "");
    snprintf(s_device_id, sizeof(s_device_id), "%s", device_id);
}

bool voice_context_service_report(const voice_context_snapshot_t *s) {
    time_t now = time(NULL);
    if (!s || !s_url[0] || !s_token[0] || now < 1577836800) return false;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    cJSON_AddNumberToObject(root, "observed_at", (double)now);
    cJSON *wt = cJSON_AddObjectToObject(root, "worktime");
    float today = (s->day >= 1 && s->day <= 31) ? s->worktime.daily_hours[s->day] : 0;
    cJSON_AddNumberToObject(wt, "today_hours", today);
    cJSON_AddNumberToObject(wt, "month_recorded_hours", s->worktime.recorded_hours);
    cJSON_AddNumberToObject(wt, "month_expected_hours", s->worktime.expected_hours);
    cJSON *ai = cJSON_AddArrayToObject(root, "ai_usage");
    for (int i = 0; i < s->ai_count && i < AI_MAX_PROVIDERS; i++) {
        if (s->ai[i].remaining_percent < 0) continue;
        cJSON *item = cJSON_CreateObject();
        const char *name = s->ai[i].name[0] ? s->ai[i].name :
                           (s->ai[i].label[0] ? s->ai[i].label : s->ai[i].id);
        cJSON_AddStringToObject(item, "name", name);
        cJSON_AddNumberToObject(item, "remaining_percent", s->ai[i].remaining_percent);
        cJSON_AddNumberToObject(item, "resets_at", (double)s->ai[i].resets_at);
        cJSON_AddItemToArray(ai, item);
    }
    cJSON *weather = cJSON_AddObjectToObject(root, "weather");
    cJSON_AddStringToObject(weather, "text", s->weather.text);
    cJSON_AddNumberToObject(weather, "temperature_c", s->weather.temp);
    cJSON_AddNumberToObject(weather, "humidity_percent", s->weather.humidity);
    cJSON_AddStringToObject(weather, "almanac_yi", s->weather_detail.almanac_yi);
    cJSON_AddStringToObject(weather, "almanac_ji", s->weather_detail.almanac_ji);
    cJSON *race = cJSON_AddObjectToObject(root, "racebox");
    char date[16];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", s->year, s->month, s->day);
    cJSON_AddStringToObject(race, "date", date);
    cJSON_AddNumberToObject(race, "synced_points", s->racebox_points);
    cJSON_AddBoolToObject(race, "sync_complete", s->racebox_complete);
    char *body = cJSON_PrintUnformatted(root);
    bool ok = body && http_post_json(s_url, s_token, s_device_id, body);
    ESP_LOGI(TAG, "context report %s points=%d", ok ? "ok" : "failed", s->racebox_points);
    cJSON_free(body);
    cJSON_Delete(root);
    return ok;
}
