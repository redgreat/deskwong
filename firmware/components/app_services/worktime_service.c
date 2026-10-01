#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "http_util.h"
#include "worktime_service.h"
#include "holiday_service.h"

static const char *TAG = "worktime";
static char s_base[160] = "";
static char s_token[96] = "";

/* 本地兜底标准工时：与服务端 worktime.expected_daily_hours 的默认值一致 */
#define WT_DAILY_HOURS 8.0f
/* 缓存沿用配置命名空间，"恢复出厂设置" 会一并清除 */
#define WT_NVS_NAMESPACE "deskwong"
#define WT_CACHE_MAGIC   0x57543131u /* "WT11" */

typedef struct {
    uint32_t magic;
    int year;
    int month;
    float recorded_hours;
    float expected_hours;
    float daily_hours[32];
} worktime_cache_blob_t;

/* NVS 写入会关闭 flash cache，而调用方的栈可能落在 PSRAM（cache 关闭时不可访问）。
 * 因此缓存快照交给一个内部 RAM 栈的小任务落盘，与 racebox 按日状态的做法一致。 */
#define WT_CACHE_STACK 4096
static worktime_summary_t s_pending;
static portMUX_TYPE s_pending_mux = portMUX_INITIALIZER_UNLOCKED;
static StaticTask_t *s_cache_tcb;
static StackType_t *s_cache_stack;
static TaskHandle_t s_cache_task;

static void cache_key(char *key, size_t len, int year, int month) {
    snprintf(key, len, "wt_%04d%02d", year, month);
}

/* 分母缺失时用本地日历兜底，保证屏幕永远有数可显示 */
static void normalize(worktime_summary_t *out) {
    if (out->recorded_hours < 0) out->recorded_hours = 0;
    if (out->expected_hours <= 0) out->expected_hours = worktime_expected_hours(out->year, out->month);
    out->ratio = out->expected_hours > 0 ? out->recorded_hours / out->expected_hours : 0;
}

float worktime_expected_hours(int year, int month) {
    int workdays = holiday_workdays(year, month);
    if (workdays <= 0) return 0;
    return workdays * WT_DAILY_HOURS;
}

static void cache_write(const worktime_summary_t *s) {
    if (s->year < 2001 || s->month < 1 || s->month > 12) return;
    worktime_cache_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    blob.magic = WT_CACHE_MAGIC;
    blob.year = s->year;
    blob.month = s->month;
    blob.recorded_hours = s->recorded_hours;
    blob.expected_hours = s->expected_hours;
    memcpy(blob.daily_hours, s->daily_hours, sizeof(blob.daily_hours));
    nvs_handle_t h;
    if (nvs_open(WT_NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    char key[16];
    cache_key(key, sizeof(key), s->year, s->month);
    worktime_cache_blob_t old;
    size_t len = sizeof(old);
    bool unchanged = nvs_get_blob(h, key, &old, &len) == ESP_OK && len == sizeof(old) &&
                     memcmp(&old, &blob, sizeof(blob)) == 0;
    if (!unchanged) {
        if (nvs_set_blob(h, key, &blob, sizeof(blob)) != ESP_OK || nvs_commit(h) != ESP_OK)
            ESP_LOGW(TAG, "cache save failed for %04d-%02d", s->year, s->month);
        else
            ESP_LOGI(TAG, "cache saved %04d-%02d: %.1f/%.1f", s->year, s->month,
                     s->recorded_hours, s->expected_hours);
    }
    nvs_close(h);
}

static void cache_save_worker(void *arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        worktime_summary_t snap;
        taskENTER_CRITICAL(&s_pending_mux);
        snap = s_pending;
        taskEXIT_CRITICAL(&s_pending_mux);
        cache_write(&snap);
    }
}

/* 只保留最新一份汇总，真正的写盘由内部 RAM 栈任务执行 */
static void cache_store(const worktime_summary_t *s) {
    if (!s_cache_task) return;
    taskENTER_CRITICAL(&s_pending_mux);
    s_pending = *s;
    taskEXIT_CRITICAL(&s_pending_mux);
    xTaskNotifyGive(s_cache_task);
}

void worktime_service_init(const char *base, const char *token) {
    if (base) strncpy(s_base, base, sizeof(s_base) - 1);
    if (token) strncpy(s_token, token, sizeof(s_token) - 1);
    s_cache_tcb = heap_caps_calloc(1, sizeof(*s_cache_tcb), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_cache_stack = heap_caps_calloc(WT_CACHE_STACK, sizeof(*s_cache_stack),
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_cache_tcb && s_cache_stack)
        s_cache_task = xTaskCreateStatic(cache_save_worker, "wt_cache", WT_CACHE_STACK, NULL, 2,
                                         s_cache_stack, s_cache_tcb);
    if (!s_cache_task) ESP_LOGE(TAG, "worktime cache task allocation failed");
}

void worktime_service_load(int year, int month, worktime_summary_t *out) {
    memset(out, 0, sizeof(*out));
    out->year = year;
    out->month = month;
    nvs_handle_t h;
    if (nvs_open(WT_NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        worktime_cache_blob_t blob;
        size_t len = sizeof(blob);
        char key[16];
        cache_key(key, sizeof(key), year, month);
        if (nvs_get_blob(h, key, &blob, &len) == ESP_OK && len == sizeof(blob) &&
            blob.magic == WT_CACHE_MAGIC) {
            out->recorded_hours = blob.recorded_hours;
            out->expected_hours = blob.expected_hours;
            memcpy(out->daily_hours, blob.daily_hours, sizeof(out->daily_hours));
            ESP_LOGI(TAG, "cache loaded %04d-%02d: %.1f/%.1f", year, month,
                     out->recorded_hours, out->expected_hours);
        }
        nvs_close(h);
    }
    normalize(out);
}

int worktime_service_fetch(int year, int month, worktime_summary_t *out) {
    if (s_base[0] == 0) {
        ESP_LOGW(TAG, "worktime api base not configured; showing local expected hours");
        return 1;
    }
    /* 容错拼接：和 AI 用量一致，允许填服务根地址、/worktime 前缀或完整路径，避免出现
     * /worktime/worktime/summary 这类重复后缀。 */
    char base[160];
    snprintf(base, sizeof(base), "%s", s_base);
    size_t blen = strlen(base);
    while (blen > 0 && base[blen - 1] == '/') base[--blen] = 0;
    const char full[] = "/worktime/summary";
    const char shortp[] = "/worktime";
    if (blen >= sizeof(full) - 1 && strcmp(base + blen - (sizeof(full) - 1), full) == 0) {
        base[blen - (sizeof(full) - 1)] = 0;
        blen -= sizeof(full) - 1;
    } else if (blen >= sizeof(shortp) - 1 && strcmp(base + blen - (sizeof(shortp) - 1), shortp) == 0) {
        base[blen - (sizeof(shortp) - 1)] = 0;
        blen -= sizeof(shortp) - 1;
    }
    char url[320];
    snprintf(url, sizeof(url), "%s/worktime/summary?year=%d&month=%d", base, year, month);
    cJSON *j = http_get_json(url, s_token);
    if (!j) return 1;
    cJSON *data = cJSON_GetObjectItem(j, "data");
    if (!data) { cJSON_Delete(j); return 1; }
    memset(out, 0, sizeof(*out));
    cJSON *r = cJSON_GetObjectItem(data, "total_recorded_hours");
    cJSON *e = cJSON_GetObjectItem(data, "total_expected_hours");
    out->year = year;
    out->month = month;
    out->recorded_hours = r ? (float)cJSON_GetNumberValue(r) : 0;
    out->expected_hours = e ? (float)cJSON_GetNumberValue(e) : 0;
    cJSON *days = cJSON_GetObjectItem(data, "days");
    if (cJSON_IsArray(days)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, days) {
            cJSON *day = cJSON_GetObjectItem(item, "day");
            cJSON *hours = cJSON_GetObjectItem(item, "recorded_hours");
            if (!hours) hours = cJSON_GetObjectItem(item, "hours");
            int d = day ? day->valueint : 0;
            if (!d) {
                cJSON *date = cJSON_GetObjectItem(item, "date");
                if (cJSON_IsString(date) && strlen(date->valuestring) >= 10)
                    d = atoi(date->valuestring + 8);
            }
            if (d >= 1 && d <= 31 && hours) out->daily_hours[d] = (float)cJSON_GetNumberValue(hours);
        }
    }
    cJSON_Delete(j);
    normalize(out);
    cache_store(out);
    ESP_LOGI(TAG, "%d-%02d: %.1f/%.1f", year, month, out->recorded_hours, out->expected_hours);
    return 0;
}
