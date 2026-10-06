#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int weekday;   // 0=周日 .. 6=周六
} datetime_t;

/* 纯字段换算，不引入时区：返回值只保证「时间越晚数值越大」，可当无时区 epoch 用。
 * 两个 datetime_t 的差值与此刻的时区无关，因此绝对值不要直接当 UTC 时间戳用。 */
int64_t time_util_epoch_seconds(const datetime_t *dt);

/* a - b 的秒数差（按日历字段直接计算，不受时区影响） */
int64_t time_util_diff_seconds(const datetime_t *a, const datetime_t *b);

#ifdef __cplusplus
}
#endif
