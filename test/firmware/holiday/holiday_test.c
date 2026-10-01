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

    puts("holiday workday table passed");
    return 0;
}
