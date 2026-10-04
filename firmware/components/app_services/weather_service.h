#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char text[32];
    int temp;
    int humidity;
    int wind_scale;
    int icon;  // QWeather icon code; 0 when weather is unavailable
} weather_now_t;

#define WEATHER_DETAIL_SLOTS 7
typedef struct {
    char label[8];       /* M/D or HH */
    char condition[16];  /* localized condition */
    int icon;
    int temp_min;
    int temp_max;
    bool valid;
} weather_forecast_item_t;

typedef struct {
    weather_forecast_item_t daily[WEATHER_DETAIL_SLOTS];
    weather_forecast_item_t hourly[WEATHER_DETAIL_SLOTS];
    char almanac_yi[192];
    char almanac_ji[192];
    char updated_at[20];
    bool weather_valid;
    bool almanac_valid;
} weather_detail_t;

/* 和风 24h 首条预报小时可能因数据更新时机落到下一小时，而弹窗要求当前小时
 * 也算。首条预报小时 first_fx_hour 与设备当前小时对不上时，用刚拉到的实况
 * now 补出当前小时这一格，原预报整体后移一格。
 * now 缺失 / 实况不可用（icon=0）/ 小时一致时不做任何调整。
 * 纯函数且只依赖上方结构体，主机测试直接包含本头文件即可覆盖。 */
static inline void weather_align_hourly(weather_detail_t *d, const weather_now_t *now,
                                        int hour, int first_fx_hour) {
    if (!d->hourly[0].valid || !now || !now->icon || first_fx_hour == hour) return;
    memmove(&d->hourly[1], &d->hourly[0], sizeof(d->hourly[0]) * (WEATHER_DETAIL_SLOTS - 1));
    weather_forecast_item_t *it = &d->hourly[0];
    snprintf(it->label, sizeof(it->label), "%02d时", hour);
    strncpy(it->condition, now->text, sizeof(it->condition) - 1);
    it->condition[sizeof(it->condition) - 1] = 0;
    it->icon = now->icon;
    it->temp_min = it->temp_max = now->temp;
    it->valid = true;
}

void weather_service_init(const char *api_url, const char *location, const char *key,
                          const char *almanac_url, const char *almanac_key);
/* Returns true once after configuration changes so the worker can refresh immediately. */
bool weather_service_take_changed(void);
/* 拉取当前天气，成功返回 0，失败返回 1 */
int weather_service_fetch(weather_now_t *out);
/* 拉取 7 天 + 7 小时预报与当日黄历。各段成功即独立替换调用方缓存，失败段保留旧数据。
 * now/hour 用于小时行兜底：和风 24h 首条通常是当前小时，但更新时机可能落到下一小时，
 * 首条预报小时与设备当前小时对不上时，用实况观测补出当前小时这一格。 */
int weather_service_fetch_detail(weather_detail_t *out, const weather_now_t *now,
                                 int year, int month, int day, int hour);

#ifdef __cplusplus
}
#endif
