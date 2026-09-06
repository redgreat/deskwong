#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year;
    int month;
    float recorded_hours;
    float expected_hours;
    float ratio;   // recorded/expected，由设备端算
} worktime_summary_t;

void worktime_service_init(const char *base, const char *token);
/* 拉取某月工时汇总，成功返回 0 */
int worktime_service_fetch(int year, int month, worktime_summary_t *out);

#ifdef __cplusplus
}
#endif
