#include "holiday_service.h"
#include "calendar_service.h"

/* 法定节假日区间（2025-2026，国务院发布） */
typedef struct {
    int year;
    int sm, sd;   // 起始月日
    int em, ed;   // 结束月日
    const char *name;
} holiday_range_t;

static const holiday_range_t s_holidays[] = {
    /* 2025 */
    {2025, 1, 1,  1, 1,  "元旦"},
    {2025, 1, 28, 2, 4,  "春节"},
    {2025, 4, 4,  4, 6,  "清明节"},
    {2025, 5, 1,  5, 5,  "劳动节"},
    {2025, 5, 31, 6, 2,  "端午节"},
    {2025, 10, 1, 10, 8, "国庆节"},
    /* 2026 */
    {2026, 1, 1,  1, 3,  "元旦"},
    {2026, 2, 15, 2, 23, "春节"},
    {2026, 4, 4,  4, 6,  "清明节"},
    {2026, 5, 1,  5, 5,  "劳动节"},
    {2026, 6, 19, 6, 21, "端午节"},
    {2026, 9, 25, 9, 27, "中秋节"},
    {2026, 10, 1, 10, 7, "国庆节"},
};

/* 调休上班日（周末但需上班） */
typedef struct { int year, m, d; } workday_t;
static const workday_t s_workdays[] = {
    {2025, 1, 26}, {2025, 2, 8},  {2025, 4, 27}, {2025, 9, 28}, {2025, 10, 11},
    {2026, 1, 4},  {2026, 2, 14}, {2026, 2, 28}, {2026, 5, 9},  {2026, 9, 20},
    {2026, 10, 10},
};

static int in_range(int m, int d, int sm, int sd, int em, int ed) {
    if (sm == em) return (m == sm && d >= sd && d <= ed);
    /* 跨月 */
    if (m == sm) return d >= sd;
    if (m == em) return d <= ed;
    return (m > sm && m < em);
}

int holiday_query(int year, int month, int day, const char **name) {
    int n = sizeof(s_holidays) / sizeof(s_holidays[0]);
    for (int i = 0; i < n; i++) {
        const holiday_range_t *h = &s_holidays[i];
        if (h->year == year && in_range(month, day, h->sm, h->sd, h->em, h->ed)) {
            if (name) *name = h->name;
            return DAY_HOLIDAY;
        }
    }
    int m = sizeof(s_workdays) / sizeof(s_workdays[0]);
    for (int i = 0; i < m; i++) {
        if (s_workdays[i].year == year && s_workdays[i].m == month && s_workdays[i].d == day) {
            if (name) *name = "调休上班";
            return DAY_WORKDAY;
        }
    }
    if (name) *name = NULL;
    return DAY_NORMAL;
}

bool holiday_is_weekend(int year, int month, int day) {
    int wd = calendar_weekday(year, month, day);
    return (wd == 0 || wd == 6);
}

bool holiday_is_workday(int year, int month, int day) {
    int t = holiday_query(year, month, day, NULL);
    if (t == DAY_HOLIDAY) return false;
    if (t == DAY_WORKDAY) return true;
    return !holiday_is_weekend(year, month, day);
}
