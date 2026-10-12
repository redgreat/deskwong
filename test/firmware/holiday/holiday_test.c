/* 主机测试：法定工作日数量与调休判定。
 * 期望值同时写进 test/service/main_test.go，保证固件与服务端的节假日表口径一致。 */
#include <assert.h>
#include <stdio.h>
#include "holiday_service.h"

int main(void) {
    /* 与服务端 expectedHours 的用例一一对应 */
    assert(holiday_workdays(2026, 3) == 22);   /* 无节假日 */
    assert(holiday_workdays(2026, 2) == 16);   /* 春节 2/15-2/23，2/14、2/28 调休 */
    assert(holiday_workdays(2026, 9) == 22);   /* 中秋 9/25-9/27，9/20 调休 */
    assert(holiday_workdays(2026, 10) == 18);  /* 国庆 10/1-10/7，10/10 调休 */
    assert(holiday_workdays(2025, 10) == 18);  /* 国庆 10/1-10/8，10/11 调休 */

    /* 调休上班的周末计入 */
    assert(holiday_is_workday(2026, 10, 10));
    assert(holiday_is_workday(2026, 9, 20));
    /* 法定节假日与自然周末都不计入 */
    assert(!holiday_is_workday(2026, 10, 1));
    assert(!holiday_is_workday(2026, 10, 3));
    assert(!holiday_is_workday(2026, 9, 26));

    /* 表外年份按自然周末判断，非法年月返回 0 */
    assert(holiday_workdays(2024, 1) > 0);
    assert(holiday_workdays(0, 0) == 0);
    assert(holiday_workdays(2026, 13) == 0);

    /* 已过工作日：主屏“应记录”按本月 1 日到当天（含）折算，需求口径为工作日 ×8 小时 */
    assert(holiday_workdays_upto(2026, 10, 12) == 4);   /* 10/8、10/9、10/10 调休、10/12 */
    assert(holiday_workdays_upto(2026, 10, 31) == 18);  /* 到月末等于整月 */
    assert(holiday_workdays_upto(2026, 10, 40) == 18);  /* 超过月末按月末截断 */
    assert(holiday_workdays_upto(2026, 10, 7) == 0);    /* 国庆 10/1-10/7 全假，尚无可计工作日 */
    assert(holiday_workdays_upto(2026, 3, 10) == 7);    /* 10 号已过 7 个工作日 → 7×8=56 */
    assert(holiday_workdays_upto(2026, 2, 14) == 11);   /* 2/14 调休计入 */
    /* 边界：当天之前没有工作日、非法输入都返回 0 */
    assert(holiday_workdays_upto(2026, 10, 0) == 0);
    assert(holiday_workdays_upto(2026, 10, -3) == 0);
    assert(holiday_workdays_upto(0, 0, 10) == 0);
    assert(holiday_workdays_upto(2026, 13, 10) == 0);

    puts("holiday workday table passed");
    return 0;
}
