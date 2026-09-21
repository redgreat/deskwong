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
    bool is_today;     // 今天：黑底白字
    float work_hours;  // 0..8+，在日期下方显示进度条
} calendar_cell_t;

void main_screen_init(int width, int height);
void main_screen_update_time(const char *date, const char *time, const char *week, const char *lunar);
/* cells uses Monday as the first column. */
void main_screen_update_calendar(const calendar_cell_t cells[42]);
void main_screen_update_bottom(const char *info);
void main_screen_update_status(const char *status);
/* 联网状态：未联网时状态栏显示可访问的后台地址，并隐藏 RaceBox 图标腾出空间 */
void main_screen_update_net(bool connected, const char *ip);
void main_screen_update_summary(float recorded, float expected, const char *weather,
                                const char *lunar, const ai_provider_t *ai5h, const ai_provider_t *aiweek,
                                bool racebox_synced, int racebox_points,
                                float indoor_temp, float indoor_humidity);
void main_screen_show_remind(const char *msg);
void main_screen_hide_remind(void);
bool main_screen_remind_visible(void);
void main_screen_set_reminder_enabled(bool enabled);

#ifdef __cplusplus
}
#endif
