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
    float daily_hours[32]; // 1..31，每日已记录工时
} worktime_summary_t;

void worktime_service_init(const char *base, const char *token);
/* 拉取某月工时汇总，成功返回 0；失败时 out 保持原值 */
int worktime_service_fetch(int year, int month, worktime_summary_t *out);
/* 某月应收工时：法定工作日 × 每日标准工时（服务端未返回时兜底），无记录也为正数 */
float worktime_expected_hours(int year, int month);
/* 载入 NVS 中上次成功的月度汇总；无缓存时返回 0 记录 + 本地应收工时 */
void worktime_service_load(int year, int month, worktime_summary_t *out);

#ifdef __cplusplus
}
#endif
