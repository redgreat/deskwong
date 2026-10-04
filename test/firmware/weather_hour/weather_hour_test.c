#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../../firmware/components/app_services/weather_service.h"

/* 覆盖 weather_service.h 里 weather_align_hourly 的纯逻辑：
 * 和风 24h 首条预报小时与设备当前小时对不上时，用实况补当前小时这一格。 */
static void fill(weather_detail_t *d, int start_hour, int n) {
    memset(d, 0, sizeof(*d));
    for (int i = 0; i < WEATHER_DETAIL_SLOTS && i < n; ++i) {
        weather_forecast_item_t *it = &d->hourly[i];
        snprintf(it->label, sizeof(it->label), "%02d时", (start_hour + i) % 24);
        snprintf(it->condition, sizeof(it->condition), "预报%d", i);
        it->icon = 100 + i;
        it->temp_min = it->temp_max = 20 + i;
        it->valid = true;
    }
}

int main(void) {
    weather_now_t now = {}; strcpy(now.text, "晴"); now.icon = 100; now.temp = 26;
    weather_detail_t d;

    /* 首条预报就是当前小时：原样保留 */
    fill(&d, 9, 7);
    weather_align_hourly(&d, &now, 9, 9);
    assert(!strcmp(d.hourly[0].label, "09时") && !strcmp(d.hourly[0].condition, "预报0"));
    assert(d.hourly[6].temp_min == 26);

    /* 首条预报是下一小时：实况补当前小时，预报整体后移一格，原第 7 格挤出 */
    fill(&d, 10, 7);
    weather_align_hourly(&d, &now, 9, 10);
    assert(!strcmp(d.hourly[0].label, "09时") && !strcmp(d.hourly[0].condition, "晴"));
    assert(d.hourly[0].icon == 100 && d.hourly[0].temp_min == 26 && d.hourly[0].temp_max == 26);
    assert(!strcmp(d.hourly[1].label, "10时") && !strcmp(d.hourly[1].condition, "预报0") && d.hourly[1].icon == 100);
    assert(!strcmp(d.hourly[2].label, "11时") && d.hourly[2].icon == 101);
    assert(!strcmp(d.hourly[6].label, "15时") && d.hourly[6].icon == 105);

    /* 实况不可用 / 无实况数据 / 首行无效：不做任何调整 */
    weather_now_t noicon = now; noicon.icon = 0;
    fill(&d, 10, 7);
    weather_align_hourly(&d, &noicon, 9, 10);
    assert(!strcmp(d.hourly[0].label, "10时"));

    fill(&d, 10, 7);
    weather_align_hourly(&d, NULL, 9, 10);
    assert(!strcmp(d.hourly[0].label, "10时"));

    fill(&d, 10, 0);
    weather_align_hourly(&d, &now, 9, 10);
    assert(!d.hourly[0].valid);

    puts("weather hourly current-slot alignment passed");
    return 0;
}
