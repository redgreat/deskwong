#pragma once
#include <stdbool.h>
#include "time_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REMIND_SIGNIN = 0,   // 企微签到（上班前）
    REMIND_SIGNOUT,      // 企微签退（下班后）
    REMIND_WORKTIME,     // 记录工时（下班前）
    REMIND_NONE
} remind_type_t;

typedef void (*remind_cb_t)(remind_type_t type);

void reminder_service_init(remind_cb_t cb);
void reminder_service_set_times(uint8_t si_hh, uint8_t si_mm,
                                uint8_t so_hh, uint8_t so_mm,
                                uint8_t wt_hh, uint8_t wt_mm);
void reminder_service_set_enabled(bool enabled);
/* 每秒调用，检查并触发提醒 */
void reminder_service_tick(const datetime_t *now);
/* 判断是否工作日（当前仅排除周末；法定节假日/调休后续接入） */
bool reminder_is_workday(int year, int month, int day);

#ifdef __cplusplus
}
#endif
