#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "http_util.h"
#include "weather_service.h"

static const char *TAG = "weather";
static char s_api_url[160] = "";
static char s_location[96] = "北京";
static char s_key[96] = "";
static char s_almanac_url[160] = "";
static char s_almanac_key[96] = "";
static char s_host[96] = "";         /* 从 s_api_url 里提取的 https://host */
static char s_resolved_id[32] = "";  /* GeoAPI 解析出来的 LocationID */
static bool s_geo_tried = false;
static volatile bool s_changed = false;

#define OPEN_METEO_DEFAULT "https://api.open-meteo.com/v1/forecast"

/* UTF-8 百分号编码：device>=480 或 echo（时间 popularity）时遇到中文，直接拼 URL 会让 http client 解析失败 */
static void url_encode(const char *in, char *out, size_t out_len) {
    static const char *hex = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 4 < out_len; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == ',') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0F];
        }
    }
    out[o] = 0;
}

/* 从 https://host/path 里取出 https://host */
static void extract_host(const char *url, char *host, size_t len) {
    const char *p = strstr(url, "://");
    if (!p) { snprintf(host, len, "%s", url); return; }
    p += 3;
    const char *slash = strchr(p, '/');
    size_t n = slash ? (size_t)(slash - url) : strlen(url);
    if (n >= len) n = len - 1;
    memcpy(host, url, n);
    host[n] = 0;
}

/* 纯数字 = LocationID；含逗号 = 经纬度。都不是则调 GeoAPI 换 LocationID */
static int need_lookup(void) {
    if (s_resolved_id[0]) return 0;
    const char *p = s_location;
    int digits = 0, comma = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') digits++;
        else if (*p == ',' ) comma++;
        else if (*p == '.' || *p == '-' || *p == ' ') continue;
        else return 1;   /* 出现其它字符 = 城市名 */
    }
    return (digits > 0 && comma == 0) ? 0 : (comma == 1 ? 0 : 1);
}

static void try_geo_lookup(void) {
    if (s_geo_tried || s_key[0] == 0 || s_host[0] == 0) return;
    s_geo_tried = true;
    if (!need_lookup()) return;
    char enc[160], url[512];
    url_encode(s_location, enc, sizeof(enc));
    snprintf(url, sizeof(url), "%s/geo/v2/city/lookup?location=%s&number=1&range=cn&key=%s",
             s_host, enc, s_key);
    cJSON *j = http_get_json(url, NULL);
    if (!j) { ESP_LOGW(TAG, "geo lookup failed for '%s'", s_location); return; }
    cJSON *arr = cJSON_GetObjectItem(j, "location");
    if (arr && cJSON_GetArraySize(arr) > 0) {
        cJSON *first = cJSON_GetArrayItem(arr, 0);
        cJSON *id = cJSON_GetObjectItem(first, "id");
        cJSON *name = cJSON_GetObjectItem(first, "name");
        if (id && cJSON_IsString(id)) {
            snprintf(s_resolved_id, sizeof(s_resolved_id), "%s", id->valuestring);
            ESP_LOGI(TAG, "geo: '%s' -> LocationID %s (%s)", s_location, s_resolved_id,
                     name && cJSON_IsString(name) ? name->valuestring : "?");
        }
    } else {
        ESP_LOGW(TAG, "geo: no match for '%s'", s_location);
    }
    cJSON_Delete(j);
}

/* WMO weather code → 中文（Open-Meteo 使用 WMO 4677 码） */
static const char *wmo_text(int code) {
    switch (code) {
    case 0:  return "晴";
    case 1:  return "少云";
    case 2:  return "多云";
    case 3:  return "阴";
    case 45:
    case 48: return "雾";
    case 51:
    case 53:
    case 55: return "毛毛雨";
    case 56:
    case 57: return "冻毛毛雨";
    case 61: return "小雨";
    case 63: return "中雨";
    case 65: return "大雨";
    case 66:
    case 67: return "冻雨";
    case 71: return "小雪";
    case 73: return "中雪";
    case 75: return "大雪";
    case 77: return "米雪";
    case 80: return "阵雨";
    case 81: return "强阵雨";
    case 82: return "暴雨";
    case 85:
    case 86: return "阵雪";
    case 95: return "雷阵雨";
    case 96:
    case 99: return "雷暴冰雹";
    default: return "--";
    }
}

/* Open-Meteo WMO code -> closest QWeather weather icon. */
static int wmo_icon(int code) {
    switch (code) {
    case 0: return 100;
    case 1: return 102;
    case 2: return 103;
    case 3: return 104;
    case 45: return 500;
    case 48: return 501;
    case 51: case 53: case 55: return 309;
    case 56: case 57: case 66: case 67: return 313;
    case 61: return 305;
    case 63: return 306;
    case 65: return 307;
    case 71: return 400;
    case 73: return 401;
    case 75: return 402;
    case 77: return 407;
    case 80: return 300;
    case 81: case 82: return 301;
    case 85: case 86: return 407;
    case 95: return 302;
    case 96: case 99: return 304;
    default: return 999;
    }
}

void weather_service_init(const char *api_url, const char *location, const char *key,
                          const char *almanac_url, const char *almanac_key) {
    if (api_url && api_url[0]) {
        strncpy(s_api_url, api_url, sizeof(s_api_url) - 1);
        s_api_url[sizeof(s_api_url) - 1] = 0;
    }
    if (location && location[0]) strncpy(s_location, location, sizeof(s_location) - 1);
    if (key) strncpy(s_key, key, sizeof(s_key) - 1);
    if (almanac_url) strncpy(s_almanac_url, almanac_url, sizeof(s_almanac_url) - 1);
    if (almanac_key) strncpy(s_almanac_key, almanac_key, sizeof(s_almanac_key) - 1);
    extract_host(s_api_url, s_host, sizeof(s_host));
    /* 位置/Key 可能刚被网页改过，重新解析一次 */
    s_geo_tried = false;
    s_resolved_id[0] = 0;
    s_changed = true;
}

static void endpoint_from_now(char *out, size_t len, const char *leaf) {
    strncpy(out, s_api_url, len - 1); out[len - 1] = 0;
    char *p = strstr(out, "/v7/weather/now");
    if (p) snprintf(p, len - (size_t)(p - out), "/v7/weather/%s", leaf);
    else {
        p = strrchr(out, '/');
        if (p) snprintf(p + 1, len - (size_t)(p + 1 - out), "%s", leaf);
    }
}

static void copy_json_text(cJSON *obj, const char *key, char *out, size_t len) {
    cJSON *v = obj ? cJSON_GetObjectItem(obj, key) : NULL;
    if (cJSON_IsString(v) && v->valuestring) {
        strncpy(out, v->valuestring, len - 1); out[len - 1] = 0;
    }
}

static int fetch_forecasts(weather_detail_t *next, const weather_now_t *now, int hour) {
    if (!s_key[0] || !strstr(s_api_url, "qweather")) return -1;
    try_geo_lookup();
    char location[128];
    url_encode(s_resolved_id[0] ? s_resolved_id : s_location, location, sizeof(location));
    char base[192], url[512];
    endpoint_from_now(base, sizeof(base), "10d");
    snprintf(url, sizeof(url), "%s%clocation=%s&key=%s", base, strchr(base, '?') ? '&' : '?', location, s_key);
    cJSON *root = http_get_json(url, NULL);
    if (!root) return -1;
    cJSON *days = root ? cJSON_GetObjectItem(root, "daily") : NULL;
    int count = cJSON_IsArray(days) ? cJSON_GetArraySize(days) : 0;
    if (count == 0) {
        cJSON_Delete(root);
        endpoint_from_now(base, sizeof(base), "7d");
        snprintf(url, sizeof(url), "%s%clocation=%s&key=%s", base, strchr(base, '?') ? '&' : '?', location, s_key);
        root = http_get_json(url, NULL);
        if (!root) return -1;
        days = cJSON_GetObjectItem(root, "daily");
        count = cJSON_IsArray(days) ? cJSON_GetArraySize(days) : 0;
    }
    memset(next->daily, 0, sizeof(next->daily));
    for (int i = 0; i < WEATHER_DETAIL_SLOTS && i < count; ++i) {
        cJSON *d = cJSON_GetArrayItem(days, i); weather_forecast_item_t *it = &next->daily[i];
        char date[16] = ""; copy_json_text(d, "fxDate", date, sizeof(date));
        if (strlen(date) >= 10) snprintf(it->label, sizeof(it->label), "%d/%d", atoi(date + 5), atoi(date + 8));
        copy_json_text(d, "textDay", it->condition, sizeof(it->condition));
        cJSON *v = cJSON_GetObjectItem(d, "iconDay"); it->icon = cJSON_IsString(v) ? atoi(v->valuestring) : 0;
        v = cJSON_GetObjectItem(d, "tempMin"); it->temp_min = cJSON_IsString(v) ? atoi(v->valuestring) : 0;
        v = cJSON_GetObjectItem(d, "tempMax"); it->temp_max = cJSON_IsString(v) ? atoi(v->valuestring) : 0;
        it->valid = date[0] != 0;
    }
    cJSON_Delete(root);

    endpoint_from_now(base, sizeof(base), "24h");
    snprintf(url, sizeof(url), "%s%clocation=%s&key=%s", base, strchr(base, '?') ? '&' : '?', location, s_key);
    root = http_get_json(url, NULL);
    if (!root) return count ? 0 : -1;
    cJSON *hours = root ? cJSON_GetObjectItem(root, "hourly") : NULL;
    count = cJSON_IsArray(hours) ? cJSON_GetArraySize(hours) : 0;
    memset(next->hourly, 0, sizeof(next->hourly));
    for (int i = 0; i < WEATHER_DETAIL_SLOTS && i < count; ++i) {
        cJSON *h = cJSON_GetArrayItem(hours, i); weather_forecast_item_t *it = &next->hourly[i];
        char when[32] = ""; copy_json_text(h, "fxTime", when, sizeof(when));
        const char *t = strchr(when, 'T'); if (t && strlen(t) >= 3) snprintf(it->label, sizeof(it->label), "%.2s时", t + 1);
        copy_json_text(h, "text", it->condition, sizeof(it->condition));
        cJSON *v = cJSON_GetObjectItem(h, "icon"); it->icon = cJSON_IsString(v) ? atoi(v->valuestring) : 0;
        v = cJSON_GetObjectItem(h, "temp"); it->temp_min = it->temp_max = cJSON_IsString(v) ? atoi(v->valuestring) : 0;
        it->valid = when[0] != 0;
    }
    /* 和风 24h 首条预报小时可能与设备当前小时差一拍（数据更新时机）。
     * 弹窗要求当前小时也算：对不上时用主屏同批拉到的实况补出当前小时。 */
    if (count > 0 && next->hourly[0].valid) {
        char when0[32] = ""; copy_json_text(cJSON_GetArrayItem(hours, 0), "fxTime", when0, sizeof(when0));
        const char *t0 = strchr(when0, 'T');
        int fxh = (t0 && strlen(t0) >= 3) ? atoi(t0 + 1) : hour;
        weather_align_hourly(next, now, hour, fxh);
    }
    cJSON_Delete(root);
    next->weather_valid = next->daily[0].valid || next->hourly[0].valid;
    return next->weather_valid ? 0 : -1;
}

static void json_text_or_array(cJSON *obj, const char *key, char *out, size_t len) {
    cJSON *v = obj ? cJSON_GetObjectItem(obj, key) : NULL; out[0] = 0;
    if (cJSON_IsString(v)) { strncpy(out, v->valuestring, len - 1); out[len - 1] = 0; return; }
    if (!cJSON_IsArray(v)) return;
    cJSON *e = NULL; cJSON_ArrayForEach(e, v) if (cJSON_IsString(e)) {
        if (out[0]) strncat(out, "、", len - strlen(out) - 1);
        strncat(out, e->valuestring, len - strlen(out) - 1);
    }
}

/* 宜忌文案归一：把 "." 分隔（聚合数据风格"祭祀.祈福"）换成顿号，并去掉结尾分隔符 */
static void normalize_almanac_text(char *dst, size_t dstsz, const char *src) {
    size_t n = 0;
    for (; *src && n + 3 < dstsz; ++src) {
        if (*src == '.') {
            dst[n++] = (char)0xE3; dst[n++] = (char)0x80; dst[n++] = (char)0x81;  /* 、 */
        } else {
            dst[n++] = *src;
        }
    }
    while (n >= 3 && (uint8_t)dst[n - 3] == 0xE3 && (uint8_t)dst[n - 2] == 0x80 && (uint8_t)dst[n - 1] == 0x81)
        n -= 3;
    dst[n] = 0;
}

static int fetch_almanac(weather_detail_t *next, int year, int month, int day) {
    if (!s_almanac_url[0]) return -1;
    char url[384]; snprintf(url, sizeof(url), "%s%cdate=%04d-%02d-%02d&location=%s%s%s",
        s_almanac_url, strchr(s_almanac_url, '?') ? '&' : '?', year, month, day, s_location,
        s_almanac_key[0] ? "&key=" : "", s_almanac_key);
    cJSON *root = http_get_json(url, NULL);
    if (!root) return -1;
    char yi[sizeof(next->almanac_yi)], ji[sizeof(next->almanac_ji)];
    yi[0] = ji[0] = 0;
    /* 兼容常见容器与字段名：{data:{yi,ji}}（服务端内置）、{result:{yi,ji}}（聚合数据）、
     * 根节点直给；yi/ji 与 suit/avoid 都认 */
    static const char *const boxes[] = {"data", "result"};
    for (int i = -1; i < 2 && !yi[0] && !ji[0]; ++i) {
        cJSON *box = i < 0 ? root : cJSON_GetObjectItem(root, boxes[i]);
        if (!cJSON_IsObject(box)) continue;
        json_text_or_array(box, "yi", yi, sizeof(yi));
        if (!yi[0]) json_text_or_array(box, "suit", yi, sizeof(yi));
        json_text_or_array(box, "ji", ji, sizeof(ji));
        if (!ji[0]) json_text_or_array(box, "avoid", ji, sizeof(ji));
    }
    cJSON_Delete(root);
    if (!yi[0] && !ji[0]) return -1;
    if (yi[0]) normalize_almanac_text(next->almanac_yi, sizeof(next->almanac_yi), yi);
    else next->almanac_yi[0] = 0;
    if (ji[0]) normalize_almanac_text(next->almanac_ji, sizeof(next->almanac_ji), ji);
    else next->almanac_ji[0] = 0;
    next->almanac_valid = true;
    return 0;
}

int weather_service_fetch_detail(weather_detail_t *out, const weather_now_t *now,
                                 int year, int month, int day, int hour) {
    if (!out) return -1;
    weather_detail_t next = *out;
    int wr = fetch_forecasts(&next, now, hour), ar = fetch_almanac(&next, year, month, day);
    if (wr == 0 || ar == 0) {
        snprintf(next.updated_at, sizeof(next.updated_at), "%04d-%02d-%02d", year, month, day);
        *out = next; return 0;
    }
    return -1;
}

bool weather_service_take_changed(void) {
    bool changed = s_changed;
    s_changed = false;
    return changed;
}

/* 免密钥的 Open-Meteo：location 用 "纬度,经度"，例如 "36.07,120.38" */
static int fetch_open_meteo(weather_now_t *out) {
    double lat = 0, lon = 0;
    if (sscanf(s_location, "%lf,%lf", &lat, &lon) != 2 &&
        sscanf(s_location, "%lf %lf", &lat, &lon) != 2) {
        ESP_LOGW(TAG, "open-meteo 需要经纬度位置，例如 36.07,120.38");
        return 1;
    }
    const char *base = s_api_url[0] ? s_api_url : OPEN_METEO_DEFAULT;
    char url[384];
    snprintf(url, sizeof(url),
             "%s?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,weather_code&timezone=auto",
             base, lat, lon);
    cJSON *j = http_get_json(url, NULL);
    if (!j) return 1;
    cJSON *cur = cJSON_GetObjectItem(j, "current");
    if (!cur) { cJSON_Delete(j); return 1; }
    cJSON *tmp = cJSON_GetObjectItem(cur, "temperature_2m");
    cJSON *hum = cJSON_GetObjectItem(cur, "relative_humidity_2m");
    cJSON *code = cJSON_GetObjectItem(cur, "weather_code");
    out->icon = code && cJSON_IsNumber(code) ? wmo_icon(code->valueint) : 999;
    snprintf(out->text, sizeof(out->text), "%s", code ? wmo_text((int)(code->valuedouble)) : "--");
    out->temp = tmp ? (int)(tmp->valuedouble + 0.5) : 0;
    out->humidity = hum ? (int)(hum->valuedouble + 0.5) : 0;
    out->wind_scale = 0;
    cJSON_Delete(j);
    ESP_LOGI(TAG, "open-meteo now: %s %dC %d%%", out->text, out->temp, out->humidity);
    return 0;
}

/* 和风天气：需要 API Host + Key */
static int fetch_qweather(weather_now_t *out) {
    char url[512];
    try_geo_lookup();
    char enc[128];
    url_encode(s_resolved_id[0] ? s_resolved_id : s_location, enc, sizeof(enc));
    snprintf(url, sizeof(url), "%s%clocation=%s&key=%s",
             s_api_url, strchr(s_api_url, '?') ? '&' : '?', enc, s_key);
    cJSON *j = http_get_json(url, NULL);
    if (!j) return 1;
    cJSON *now = cJSON_GetObjectItem(j, "now");
    if (!now) { cJSON_Delete(j); return 1; }
    cJSON *t = cJSON_GetObjectItem(now, "text");
    cJSON *tmp = cJSON_GetObjectItem(now, "temp");
    cJSON *hum = cJSON_GetObjectItem(now, "humidity");
    cJSON *ws = cJSON_GetObjectItem(now, "windScale");
    cJSON *icon = cJSON_GetObjectItem(now, "icon");
    out->icon = icon && cJSON_IsString(icon) ? atoi(icon->valuestring) : 999;
    if (t && cJSON_IsString(t)) strncpy(out->text, t->valuestring, sizeof(out->text) - 1);
    else strcpy(out->text, "--");
    out->text[sizeof(out->text) - 1] = 0;
    out->temp = tmp ? atoi(tmp->valuestring) : 0;
    out->humidity = hum ? atoi(hum->valuestring) : 0;
    out->wind_scale = ws ? atoi(ws->valuestring) : 0;
    cJSON_Delete(j);
    ESP_LOGI(TAG, "qweather now: %s %dC %d%%", out->text, out->temp, out->humidity);
    return 0;
}

int weather_service_fetch(weather_now_t *out) {
    if (!out) return 1;
    out->text[0] = 0;
    out->temp = 0;
    out->humidity = 0;
    out->wind_scale = 0;
    out->icon = 0;
    /* 没填 Key 就走免密钥的 Open-Meteo；填了 Key 走和风天气 */
    if (!s_key[0]) return fetch_open_meteo(out);
    if (!s_api_url[0]) return 1;
    return fetch_qweather(out);
}
