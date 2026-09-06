#include <string.h>
#include "reminder_service.h"
#include "holiday_service.h"

static remind_cb_t s_cb = NULL;
static bool s_enabled = true;
static uint8_t s_times[3][2] = {{8, 25}, {17, 35}, {17, 0}};
static int s_last_minute = -1;

void reminder_service_init(remind_cb_t cb) {
    s_cb = cb;
}

void reminder_service_set_times(uint8_t si_hh, uint8_t si_mm,
                                uint8_t so_hh, uint8_t so_mm,
                                uint8_t wt_hh, uint8_t wt_mm) {
    s_times[REMIND_SIGNIN][0] = si_hh;
    s_times[REMIND_SIGNIN][1] = si_mm;
    s_times[REMIND_SIGNOUT][0] = so_hh;
    s_times[REMIND_SIGNOUT][1] = so_mm;
    s_times[REMIND_WORKTIME][0] = wt_hh;
    s_times[REMIND_WORKTIME][1] = wt_mm;
}

void reminder_service_set_enabled(bool enabled) {
    s_enabled = enabled;
}

bool reminder_is_workday(int year, int month, int day) {
    return holiday_is_workday(year, month, day);
}

void reminder_service_tick(const datetime_t *now) {
    if (!s_enabled || s_cb == NULL) return;
    if (!reminder_is_workday(now->year, now->month, now->day)) return;

    int cur = now->hour * 60 + now->minute;
    if (cur == s_last_minute) return;   // 同一分钟去重

    for (int i = 0; i < REMIND_NONE; i++) {
        int t = s_times[i][0] * 60 + s_times[i][1];
        if (cur == t) {
            s_last_minute = cur;
            s_cb((remind_type_t)i);
        }
    }
}
