#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DAY_NORMAL = 0,     // 普通日（工作日/周末，按自然周末判断）
    DAY_HOLIDAY = 1,    // 法定节假日
    DAY_WORKDAY = 2,    // 调休上班日（周末但需上班）
} day_type_t;

/* 查询某日的类型；name 输出节日名（可为 NULL）。成功返回 0 */
int holiday_query(int year, int month, int day, const char **name);

/* 是否工作日（法定节假日休息，调休上班日上班） */
bool holiday_is_workday(int year, int month, int day);

/* 是否周末（自然周六/周日） */
bool holiday_is_weekend(int year, int month, int day);

#ifdef __cplusplus
}
#endif
