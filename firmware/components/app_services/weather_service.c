#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "http_util.h"
#include "weather_service.h"

static const char *TAG = "weather";
static char s_location[96] = "北京";
static char s_key[96] = "";

void weather_service_init(const char *location, const char *key) {
    if (location && location[0]) strncpy(s_location, location, sizeof(s_location) - 1);
    if (key) strncpy(s_key, key, sizeof(s_key) - 1);
}

int weather_service_fetch(weather_now_t *out) {
    char url[320];
    /* 和风天气：当前天气 */
    snprintf(url, sizeof(url),
             "https://devapi.qweather.com/v7/weather/now?location=%s&key=%s",
             s_location, s_key);
    cJSON *j = http_get_json(url, NULL);
    if (!j) return 1;
    cJSON *now = cJSON_GetObjectItem(j, "now");
    if (!now) { cJSON_Delete(j); return 1; }
    cJSON *t = cJSON_GetObjectItem(now, "text");
    cJSON *tmp = cJSON_GetObjectItem(now, "temp");
    cJSON *hum = cJSON_GetObjectItem(now, "humidity");
    cJSON *ws = cJSON_GetObjectItem(now, "windScale");
    if (t && cJSON_IsString(t)) strncpy(out->text, t->valuestring, sizeof(out->text) - 1);
    else strcpy(out->text, "--");
    out->temp = tmp ? atoi(tmp->valuestring) : 0;
    out->humidity = hum ? atoi(hum->valuestring) : 0;
    out->wind_scale = ws ? atoi(ws->valuestring) : 0;
    cJSON_Delete(j);
    ESP_LOGI(TAG, "now: %s %dC %d%%", out->text, out->temp, out->humidity);
    return 0;
}
