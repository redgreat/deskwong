#pragma once

#include <stdbool.h>
#include "worktime_service.h"
#include "aiusage_service.h"
#include "weather_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year, month, day;
    worktime_summary_t worktime;
    const ai_provider_t *ai;
    int ai_count;
    weather_now_t weather;
    weather_detail_t weather_detail;
    int racebox_points;
    bool racebox_complete;
} voice_context_snapshot_t;

void voice_context_service_init(const char *api_base, const char *token, const char *device_id);
bool voice_context_service_report(const voice_context_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
