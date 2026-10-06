#include "time_util.h"

/* Howard Hinnant 的 days_from_civil：y/m/d -> 距 1970-01-01 的天数（按日历字段，
 * 无时区）。hour/minute/second 原样叠加成秒。 */
int64_t time_util_epoch_seconds(const datetime_t *dt) {
    int y = dt->year;
    int m = dt->month;
    long long d = dt->day;
    if (m <= 2) { y -= 1; m += 12; }
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;                                  /* [0, 399] */
    long long doy = (153 * (m - 3) + 2) / 5 + d - 1;                /* [0, 365] */
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          /* [0, 146096] */
    long long days = era * 146097 + doe - 719468;
    return days * 86400 + dt->hour * 3600 + dt->minute * 60 + dt->second;
}

int64_t time_util_diff_seconds(const datetime_t *a, const datetime_t *b) {
    return time_util_epoch_seconds(a) - time_util_epoch_seconds(b);
}
