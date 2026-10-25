#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_MAX_PROVIDERS 8

typedef struct {
    char id[16];
    char app_id[16];
    char app_name[16];
    char app_status[16];
    char app_error[48];        // 服务端返回的应用级失败原因，只在查不到额度时有值
    int64_t observed_at;       // application sample time, Unix seconds
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

/* 设备端「四选二」：selected 是逗号分隔的应用 id 白名单（最多两个，顺序即屏显上下顺序）。
 * 原地压缩 items 并返回过滤后的条数；白名单为空或都没命中时退化为服务端顺序的前两个应用。 */
int aiusage_service_pick(ai_provider_t *items, int count, const char *selected);

/* 缓存最近一次拉取的完整结果（不按设备端选择过滤），供设备 Web 后台 /api/ai 预览所有应用 */
void aiusage_service_publish(const ai_provider_t *items, int count);
int aiusage_service_cached(const ai_provider_t **out);

#ifdef __cplusplus
}
#endif
