/* time_util 主机测试：NTP 对时写回 RTC 前的差值比较逻辑 */
#include <stdio.h>
#include <stdlib.h>
#include "time_util.h"

static int failures = 0;

static void expect_diff(const datetime_t *a, const datetime_t *b, long long want, const char *name) {
    long long got = time_util_diff_seconds(a, b);
    if (got != want) {
        printf("FAIL %s: got %lld, want %lld\n", name, got, want);
        failures++;
    } else {
        printf("ok   %s (%llds)\n", name, got);
    }
}

static datetime_t dt(int y, int mo, int d, int h, int mi, int s) {
    datetime_t v = { y, mo, d, h, mi, s, 0 };
    return v;
}

int main(void) {
    datetime_t a = dt(2025, 10, 5, 12, 0, 0);

    expect_diff(&a, &a, 0, "same datetime diff is zero");

    datetime_t plus90 = dt(2025, 10, 5, 12, 1, 30);
    expect_diff(&plus90, &a, 90, "90 seconds later");
    expect_diff(&a, &plus90, -90, "negative direction");

    datetime_t next_day = dt(2025, 10, 6, 12, 0, 0);
    expect_diff(&next_day, &a, 86400, "one day");

    datetime_t month_end = dt(2025, 1, 31, 23, 59, 59);
    datetime_t month_start = dt(2025, 2, 1, 0, 0, 0);
    expect_diff(&month_start, &month_end, 1, "month boundary 31d");

    datetime_t feb28 = dt(2025, 2, 28, 0, 0, 0);
    datetime_t mar01 = dt(2025, 3, 1, 0, 0, 0);
    expect_diff(&mar01, &feb28, 86400, "non-leap Feb has 28 days");
    datetime_t feb28_l = dt(2024, 2, 28, 0, 0, 0);
    datetime_t feb29_l = dt(2024, 2, 29, 0, 0, 0);
    expect_diff(&feb29_l, &feb28_l, 86400, "leap Feb 29 exists");
    datetime_t mar01_l = dt(2024, 3, 1, 0, 0, 0);
    expect_diff(&mar01_l, &feb29_l, 86400, "leap Feb 29 to Mar 1");

    datetime_t year_end = dt(2024, 12, 31, 23, 59, 59);
    datetime_t year_start = dt(2025, 1, 1, 0, 0, 0);
    expect_diff(&year_start, &year_end, 1, "year boundary");

    /* RTC 掉电回退到 2000-01-01 与真实日期差出整数天（两端都取零点） */
    datetime_t today0 = dt(2025, 10, 5, 0, 0, 0);
    datetime_t rtc_reset = dt(2000, 1, 1, 0, 0, 0);
    long long d = time_util_diff_seconds(&today0, &rtc_reset);
    if (d % 86400 != 0 || d < 0) {
        printf("FAIL rtc reset diff not whole days: %lld\n", d);
        failures++;
    } else {
        printf("ok   rtc reset diff = %lld days\n", d / 86400);
    }

    /* epoch 换算的单调性：时间越晚数值越大 */
    if (time_util_epoch_seconds(&a) >= time_util_epoch_seconds(&next_day)) {
        printf("FAIL epoch not monotonic\n");
        failures++;
    } else {
        printf("ok   epoch monotonic\n");
    }

    /* 免打扰时段判断：跨午夜是重点，配错会把设备整晚静音或整晚不静音 */
    struct { int hour; uint8_t s, e; bool want; const char *name; } ranges[] = {
        { 23, 23, 7, true,  "quiet 23:00 boundary start" },
        {  3, 23, 7, true,  "quiet 03:00 inside overnight" },
        {  6, 23, 7, true,  "quiet 06:59 inside overnight" },
        {  7, 23, 7, false, "quiet 07:00 boundary end (exclusive)" },
        { 12, 23, 7, false, "quiet noon outside overnight" },
        { 22, 23, 7, false, "quiet 22:00 before start" },
        { 14,  9, 18, true,  "quiet 14:00 inside same-day" },
        {  9,  9, 18, true,  "quiet 09:00 same-day start" },
        { 18,  9, 18, false, "quiet 18:00 same-day end exclusive" },
        {  8,  9, 18, false, "quiet 08:00 same-day before" },
        { 12,  0,  0, false, "quiet disabled when start==end" },
        { 12,  0, 23, true,  "quiet full day 00-23 start" },
        { 12, 99,  7, false, "quiet invalid start rejected" },
        { 12,  0, 99, false, "quiet invalid end rejected" },
    };
    for (size_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++) {
        bool got = time_util_hour_in_range(ranges[i].hour, ranges[i].s, ranges[i].e);
        if (got != ranges[i].want) {
            printf("FAIL %s: got %d want %d\n", ranges[i].name, got, ranges[i].want);
            failures++;
        } else {
            printf("ok   %s\n", ranges[i].name);
        }
    }

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("time_util tests passed.\n");
    return 0;
}
