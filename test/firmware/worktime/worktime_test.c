/* 主机测试：主屏“应记录”工时的已过工作日口径。
 * 期望值与 test/firmware/holiday/holiday_test.c、test/service/main_test.go 同一份节假日表。 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "worktime_service.h"
#include "holiday_service.h"

int main(void) {
    /* 整月应收：法定工作日 × 8 小时（与 holiday_workdays 主机测试一致） */
    assert(worktime_expected_hours(2026, 10) == 18 * 8.0f);
    assert(worktime_expected_hours(2026, 3) == 22 * 8.0f);
    assert(worktime_expected_hours(0, 0) == 0);
    assert(worktime_expected_hours(2026, 13) == 0);

    /* 已过天数折算：1 日到 today_day（含）的法定工作日 × 8 */
    assert(worktime_expected_hours_elapsed(2026, 10, 12) == 4 * 8.0f);   /* 10/8、9、10 调休、12 */
    assert(worktime_expected_hours_elapsed(2026, 10, 31) == 18 * 8.0f);   /* 到月末等于整月 */
    assert(worktime_expected_hours_elapsed(2026, 10, 40) == 18 * 8.0f);  /* 超过月末按月末截断 */
    assert(worktime_expected_hours_elapsed(2026, 10, 7) == 0);           /* 国庆全假，尚无可计工作日 */
    /* 需求原文口径：10 号已过 7 个工作日 → 7×8=56 */
    assert(worktime_expected_hours_elapsed(2026, 3, 10) == 7 * 8.0f);
    /* 边界：非法输入返回 0，界面按 0 处理而不是给负数 */
    assert(worktime_expected_hours_elapsed(2026, 10, 0) == 0);
    assert(worktime_expected_hours_elapsed(2026, 10, -1) == 0);
    assert(worktime_expected_hours_elapsed(2026, 13, 10) == 0);
    assert(worktime_expected_hours_elapsed(0, 0, 10) == 0);
    assert(worktime_expected_hours_elapsed(2026, 2, 14) == 11 * 8.0f);   /* 2/14 调休计入 */

    /* 接口未配置 / 拉取失败：保留整月兜底值，由显示层折算到已过工作日 */
    worktime_service_init(NULL, NULL);
    worktime_summary_t sum;
    memset(&sum, 0, sizeof(sum));
    assert(worktime_service_fetch(2026, 10, &sum) != 0);
    assert(sum.expected_hours == 0);

    memset(&sum, 0, sizeof(sum));
    sum.recorded_hours = -5;
    worktime_service_load(2026, 10, &sum);
    assert(sum.year == 2026 && sum.month == 10);
    assert(sum.recorded_hours == 0);                 /* 负记录被归一化 */
    assert(sum.expected_hours == 18 * 8.0f);         /* 无服务端数据时本地整月兜底 */
    assert(worktime_expected_hours_elapsed(2026, 10, 12) * 8 / 8 == 4 * 8.0f);

    puts("worktime elapsed expected hours passed");
    return 0;
}
