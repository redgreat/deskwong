#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "aiusage_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 月历格子类型 */
enum {
    CAL_NORMAL = 0,   // 普通工作日
    CAL_WEEKEND,      // 周末
    CAL_HOLIDAY,      // 法定节假日
    CAL_WORKDAY,      // 调休上班日（周末上班）
};

typedef struct {
    int day;           // 公历日号，0=空
    char lunar[16];    // 农历/节气/标记文本
    int type;          // 见上枚举
    bool is_today;     // 今天：叠加边框
} calendar_cell_t;

void main_screen_init(int width, int height);
void main_screen_update_time(const char *date, const char *time, const char *week, const char *lunar);
void main_screen_update_calendar(const calendar_cell_t cells[42]);
void main_screen_update_bottom(const char *info);
void main_screen_show_remind(const char *msg);

#ifdef __cplusplus
}
#endif
