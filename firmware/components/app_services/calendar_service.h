#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int lunar_year;
    int lunar_month;   // 1-12
    int lunar_day;     // 1-30
    int is_leap;       // 是否闰月
    int jieqi;         // 节气序号 0-23；24=无节气
} lunar_date_t;

/* 公历转农历，年范围 1901-2050；成功返回 0 */
int calendar_solar_to_lunar(int year, int month, int day, lunar_date_t *out);

/* 农历完整字符串：如 "正月初一"、"闰二月十五"、"廿三"；节气优先返回节气名 */
void calendar_lunar_str(const lunar_date_t *l, char *buf, size_t len);

/* 农历日简短字符串：如 "初一"、"十五"、"廿三" */
void calendar_lunar_day_str(const lunar_date_t *l, char *buf, size_t len);

/* 公历某月天数 */
int calendar_month_days(int year, int month);

/* 星期，0=周日 .. 6=周六 */
int calendar_weekday(int year, int month, int day);

#ifdef __cplusplus
}
#endif
