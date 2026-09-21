#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_MAX_PROVIDERS 8

typedef struct {
    char id[16];
    char name[32];
    char label[32];
    char display[32];
    char status[16];
    int window_minutes;       // 300 = 5 hours, 10080 = weekly
    float remaining_percent;  // -1 when unavailable
    int64_t resets_at;         // Unix seconds, 0 when unavailable
} ai_provider_t;

void aiusage_service_init(const char *base, const char *token);
/* 拉取 AI 用量 providers，成功返回 0 */
int aiusage_service_fetch(ai_provider_t *out, int max, int *count);

#ifdef __cplusplus
}
#endif
