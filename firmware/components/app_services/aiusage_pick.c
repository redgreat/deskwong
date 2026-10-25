/* 设备端「四选二」：从服务端返回的（可能四个）应用里挑出本设备要展示的两个。
 * 单独成一个编译单元，不依赖 cJSON/http，这样可以在主机上直接跑单元测试。 */
#include <string.h>
#include "esp_log.h"
#include "aiusage_service.h"

static const char *TAG = "aiusage";

/* 最近一次拉取的完整结果（不按设备端选择过滤），供 /api/ai 预览所有应用的实时用量 */
static ai_provider_t s_cache[AI_MAX_PROVIDERS];
static int s_cache_count = 0;

int aiusage_service_pick(ai_provider_t *items, int count, const char *selected) {
    if (count <= 0 || !items) return 0;
    if (count > AI_MAX_PROVIDERS) count = AI_MAX_PROVIDERS;

    /* 解析白名单：最多两个 id，逗号分隔，顺序即屏显上下顺序 */
    char want[2][16] = {{0}, {0}};
    int want_count = 0;
    const char *p = selected;
    while (p && *p && want_count < 2) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        size_t len = 0;
        while (p[len] && p[len] != ',') len++;
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
        if (len > 0) {
            if (len > sizeof(want[0]) - 1) len = sizeof(want[0]) - 1;
            memcpy(want[want_count], p, len);
            want[want_count][len] = 0;
            want_count++;
        }
        p += len;
        if (*p == ',') p++;
    }

    /* 先按服务端顺序把窗口归到各自应用 */
    const char *group_id[AI_MAX_PROVIDERS];
    int group_idx[AI_MAX_PROVIDERS][AI_MAX_PROVIDERS];
    int group_len[AI_MAX_PROVIDERS];
    int groups = 0;
    for (int i = 0; i < count; ++i) {
        const char *id = items[i].app_id[0] ? items[i].app_id : "chatgpt";
        int g = -1;
        for (int j = 0; j < groups; ++j) {
            if (strcmp(group_id[j], id) == 0) { g = j; break; }
        }
        if (g < 0) {
            if (groups >= AI_MAX_PROVIDERS) continue;
            g = groups++;
            group_id[g] = id;
            group_len[g] = 0;
        }
        group_idx[g][group_len[g]++] = i;
    }

    /* 按白名单顺序输出，最多两个应用；白名单一个都没命中就退回服务端顺序 */
    int order[AI_MAX_PROVIDERS];
    int order_len = 0;
    int used[AI_MAX_PROVIDERS];
    for (int i = 0; i < AI_MAX_PROVIDERS; ++i) used[i] = 0;
    for (int w = 0; w < want_count && order_len < 2; ++w) {
        for (int g = 0; g < groups; ++g) {
            if (used[g] || strcmp(group_id[g], want[w]) != 0) continue;
            used[g] = 1;
            order[order_len++] = g;
            break;
        }
    }
    if (order_len == 0) {
        for (int g = 0; g < groups && order_len < 2; ++g) {
            used[g] = 1;
            order[order_len++] = g;
        }
    }

    ai_provider_t tmp[AI_MAX_PROVIDERS];
    int n = 0;
    for (int o = 0; o < order_len; ++o) {
        int g = order[o];
        for (int k = 0; k < group_len[g] && n < AI_MAX_PROVIDERS; ++k) {
            tmp[n++] = items[group_idx[g][k]];
        }
    }
    memcpy(items, tmp, sizeof(tmp[0]) * (size_t)n);
    if (n != count) {
        ESP_LOGI(TAG, "picked %d/%d window(s), app=%s%s%s", n, count,
                 order_len > 0 ? group_id[order[0]] : "-",
                 order_len > 1 ? "," : "",
                 order_len > 1 ? group_id[order[1]] : "");
    }
    return n;
}

void aiusage_service_publish(const ai_provider_t *items, int count) {
    if (count < 0) count = 0;
    if (count > AI_MAX_PROVIDERS) count = AI_MAX_PROVIDERS;
    if (count > 0 && items) memcpy(s_cache, items, sizeof(s_cache[0]) * (size_t)count);
    s_cache_count = count;
}

int aiusage_service_cached(const ai_provider_t **out) {
    if (out) *out = s_cache;
    return s_cache_count;
}
