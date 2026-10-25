/* 设备端「AI 应用四选二」主机测试：正常路径 + 边界/错误路径。
 * 覆盖 aiusage_pick.c 的选择、排序、去重、回退与缓存读写。 */
#include <stdio.h>
#include <string.h>
#include "aiusage_service.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", (msg), __LINE__); failures++; } } while (0)

static const char *kFourApps[] = {"chatgpt", "devin", "claude", "mimo"};

/* 造数据：每个应用 windows_per_app 条窗口，按应用顺序排列 */
static int build(ai_provider_t *out, const char *const *app_ids, int apps, int windows_per_app) {
    int n = 0;
    for (int a = 0; a < apps; ++a) {
        for (int w = 0; w < windows_per_app; ++w) {
            memset(&out[n], 0, sizeof(out[n]));
            strncpy(out[n].app_id, app_ids[a], sizeof(out[n].app_id) - 1);
            snprintf(out[n].id, sizeof(out[n].id), "%s_w%d", app_ids[a], w);
            n++;
        }
    }
    return n;
}

int main(void) {
    ai_provider_t items[AI_MAX_PROVIDERS + 8];

    /* 1. 正常路径：从四个应用里选两个，顺序按选择顺序 */
    int n = build(items, kFourApps, 4, 2);
    CHECK(n == 8, "build 4 apps x 2 windows");
    n = aiusage_service_pick(items, n, "claude,chatgpt");
    CHECK(n == 4, "two apps -> 4 windows");
    CHECK(strcmp(items[0].app_id, "claude") == 0, "first app is claude");
    CHECK(strcmp(items[1].app_id, "claude") == 0, "claude keeps both windows");
    CHECK(strcmp(items[2].app_id, "chatgpt") == 0, "second app is chatgpt");
    CHECK(strcmp(items[3].app_id, "chatgpt") == 0, "chatgpt keeps both windows");

    /* 2. 只选一个应用 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "mimo");
    CHECK(n == 2, "single selection -> 2 windows");
    CHECK(strcmp(items[0].app_id, "mimo") == 0, "single selection app");

    /* 3. 边界：空选择退回服务端顺序的前两个应用 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "");
    CHECK(n == 4, "empty selection -> first two apps");
    CHECK(strcmp(items[0].app_id, "chatgpt") == 0 && strcmp(items[2].app_id, "devin") == 0, "empty selection order");

    /* 4. 边界：白名单一个都没命中时同样退回服务端顺序 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "nope,also-nope");
    CHECK(n == 4, "unknown ids fall back");
    CHECK(strcmp(items[0].app_id, "chatgpt") == 0, "fallback keeps server order");

    /* 5. 边界：部分命中只输出命中的应用，不补第二个未选应用 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "claude,nope");
    CHECK(n == 2, "partial match keeps only matched app");
    CHECK(strcmp(items[0].app_id, "claude") == 0, "partial match app");

    /* 6. 边界：重复 id 去重 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "devin,devin");
    CHECK(n == 2, "duplicate id deduped to one app");
    CHECK(strcmp(items[0].app_id, "devin") == 0, "dedupe keeps devin");

    /* 7. 边界：空白与多余逗号都要容忍 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "  mimo , , claude  ");
    CHECK(n == 4, "whitespace and empty tokens tolerated");
    CHECK(strcmp(items[0].app_id, "mimo") == 0 && strcmp(items[2].app_id, "claude") == 0, "whitespace order");

    /* 8. 错误路径：空输入、NULL 输入都不能崩且返回空结果 */
    CHECK(aiusage_service_pick(items, 0, "chatgpt") == 0, "count 0 -> 0");
    CHECK(aiusage_service_pick(NULL, 4, "chatgpt") == 0, "NULL items -> 0");
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, NULL);
    CHECK(n == 4, "NULL selection falls back to first two apps");

    /* 9. 边界：输入条数超过上限时被截断，不越界写 */
    ai_provider_t big[16];
    int bn = build(big, kFourApps, 4, 4);
    CHECK(bn == 16, "build 16 windows");
    bn = aiusage_service_pick(big, bn, "chatgpt");
    CHECK(bn == 4 && bn <= AI_MAX_PROVIDERS, "oversized input clamped");

    /* 10. 缓存：/api/ai 回读的必须是过滤后的结果 */
    n = build(items, kFourApps, 4, 2);
    n = aiusage_service_pick(items, n, "claude");
    aiusage_service_publish(items, n);
    const ai_provider_t *cached = NULL;
    int cn = aiusage_service_cached(&cached);
    CHECK(cn == 2, "cached count");
    CHECK(cached != NULL && strcmp(cached[0].app_id, "claude") == 0, "cached app is the selected one");
    aiusage_service_publish(NULL, -1);
    CHECK(aiusage_service_cached(NULL) == 0, "publish with invalid input clears cache");

    if (failures) {
        printf("AI pick host test failed: %d check(s)\n", failures);
        return 1;
    }
    printf("AI pick host test passed\n");
    return 0;
}
