/* 农历转换（基于 cdate，BSD 许可，https://github.com/vincascm/cdate）
 * 数据范围 1900-2050 */
#include <string.h>
#include <stdio.h>
#include "calendar_service.h"

/* 1900-2051 农历数据 */
static const unsigned int calendar_data[] = {
0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
0x05aa0, 0x076a3, 0x096d0, 0x04bd7, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
0x14b63};

/* 节气预计算值 */
static const int jieqi_data[] = {
    0, 21208, 42467, 63836, 85337, 107014, 128867, 150921, 173149,
    195551, 218072, 240693, 263343, 285989, 308563, 331033, 353350,
    375494, 397447, 419210, 440795, 462224, 483532, 504758};

#define NIAN_BIT(nian, bit) (calendar_data[nian - 1900] & bit)
#define WHICH_RUN_YUE(nian) (NIAN_BIT(nian, 0xF))

static int days_of_run_rue(int nian) {
    if (WHICH_RUN_YUE(nian))
        return (NIAN_BIT(nian, 0x10000) > 0) ? 30 : 29;
    return 0;
}

static int days_of_nian(int nian) {
    int i, sum = 348;
    for (i = 0x8000; i > 8; i >>= 1)
        if (NIAN_BIT(nian, i) > 0) sum++;
    return sum + days_of_run_rue(nian);
}

static int is_bissextile(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 1 : 0;
}

static int sum_to_premonth(int year, int month) {
    int i, sum = (month - 1) ? (month - 1) * 30 : 0;
    for (i = 1; i <= month - 1; i++)
        switch (i) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: sum++; break;
        case 2: sum -= 2; if (is_bissextile(year)) sum++; break;
        }
    return sum;
}

static int sub_two_date(int y1, int m1, int d1, int y2, int m2, int d2) {
    int i, sum = sum_to_premonth(y1, m1) + 365 - sum_to_premonth(y2, m2);
    if (is_bissextile(y2)) sum--;
    sum += d1 - d2;
    for (i = y2 + 1; i < y1; i++)
        if (is_bissextile(i)) sum++;
    if ((y1 - y2) > 0) sum += (y1 - y2 - 1) * 365;
    else if (y1 == y2) sum -= 365;
    else return -1;
    return sum;
}

static int which_day_is_jieqi(int year, int n) {
    short dpm[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int a, i = 0;
    if (is_bissextile(year)) dpm[2]++;
    a = 525948 * (year - 1900) - sub_two_date(year, 1, 6, 1900, 1, 6) * 24 * 60
        + jieqi_data[n] + 125;
    if (a < 0) i--;
    a = i + 6 + a / (24 * 60);
    for (i = 0; (i < 13) && (a > 28); i++)
        a -= dpm[i];
    return a;
}

int calendar_solar_to_lunar(int year, int month, int day, lunar_date_t *out) {
    static int percalc_val[] = {0, 18279, 36529};
    unsigned int i, x = 0, run_yue, is_run_yue = 0;
    int all_days;

    if (year < 1901 || year > 2050) return 1;

    all_days = sub_two_date(year, month, day, 1900, 1, 31);
    for (i = 0; i < 5; i++)
        if ((year > (i * 50 + 1900)) && (year <= ((i + 1) * 50 + 1900))) break;
    all_days -= percalc_val[i];
    for (i = i * 50 + 1900; (all_days > 0) && (i < 2050); i++) {
        x = days_of_nian(i);
        all_days -= x;
    }
    if (all_days < 0) { all_days += x; i--; }
    out->lunar_year = i;
    run_yue = WHICH_RUN_YUE(i);
    for (i = 1; i < 13 && all_days > 0; i++) {
        if ((run_yue > 0) && (i == run_yue + 1) && (is_run_yue == 0)) {
            --i; is_run_yue = 1; x = days_of_run_rue(out->lunar_year);
        } else {
            x = NIAN_BIT(out->lunar_year, 0x10000 >> i) ? 30 : 29;
        }
        all_days -= x;
        if (is_run_yue == 1 && i == (run_yue + 1)) is_run_yue = 0;
    }
    if (all_days < 0) { all_days += x; i--; }
    if ((all_days == 0) && (run_yue > 0) && (i == run_yue + 1)) {
        if (is_run_yue == 1) is_run_yue = 0;
        else { is_run_yue = 1; --i; }
    }
    out->lunar_month = i;
    out->lunar_day = all_days + 1;
    out->is_leap = is_run_yue;

    if (day == which_day_is_jieqi(year, (month - 1) * 2))
        out->jieqi = (month - 1) * 2;
    else if (day == which_day_is_jieqi(year, (month - 1) * 2 + 1))
        out->jieqi = (month - 1) * 2 + 1;
    else
        out->jieqi = 24;
    return 0;
}

static const char *jieqi_name[] = {
    "小寒","大寒","立春","雨水","惊蛰","春分","清明","谷雨","立夏","小满","芒种","夏至",
    "小暑","大暑","立秋","处暑","白露","秋分","寒露","霜降","立冬","小雪","大雪","冬至"};

static const char *month_name[] = {
    "", "正月","二月","三月","四月","五月","六月","七月","八月","九月","十月","冬月","腊月"};

static const char *day_name[] = {
    "", "初一","初二","初三","初四","初五","初六","初七","初八","初九","初十",
    "十一","十二","十三","十四","十五","十六","十七","十八","十九","二十",
    "廿一","廿二","廿三","廿四","廿五","廿六","廿七","廿八","廿九","三十"};

void calendar_lunar_str(const lunar_date_t *l, char *buf, size_t len) {
    if (l->jieqi >= 0 && l->jieqi < 24) {
        snprintf(buf, len, "%s", jieqi_name[l->jieqi]);
        return;
    }
    if (l->is_leap) {
        snprintf(buf, len, "闰%s%s", month_name[l->lunar_month], day_name[l->lunar_day]);
    } else {
        snprintf(buf, len, "%s%s", month_name[l->lunar_month], day_name[l->lunar_day]);
    }
}

void calendar_lunar_day_str(const lunar_date_t *l, char *buf, size_t len) {
    if (l->jieqi >= 0 && l->jieqi < 24) {
        snprintf(buf, len, "%s", jieqi_name[l->jieqi]);
        return;
    }
    snprintf(buf, len, "%s", day_name[l->lunar_day]);
}

int calendar_month_days(int year, int month) {
    static const int dpm[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_bissextile(year)) return 29;
    if (month < 1 || month > 12) return 0;
    return dpm[month];
}

int calendar_weekday(int year, int month, int day) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    y -= month < 3;
    return (y + y / 4 - y / 100 + y / 400 + t[month - 1] + day) % 7;
}
