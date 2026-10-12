#pragma once
/* test/firmware/worktime 主机桩：只提供编译与“无网络”运行所需的最小声明，
 * 行为与真机无关的路径（HTTP 拉取）一律返回失败，用于验证本地兜底口径。 */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 0x110B
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4

typedef int nvs_handle_t;
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef int StaticTask_t;
typedef uint32_t StackType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(m) ((void)0)
#define taskEXIT_CRITICAL(m) ((void)0)
#define pdTRUE 1
#define portMAX_DELAY 0xffffffff
#define pdMS_TO_TICKS(ms) (ms)

static void *heap_caps_malloc(size_t n, int flags) { (void)flags; return malloc(n); }
static void *heap_caps_calloc(size_t n, size_t size, int flags) { (void)flags; return calloc(n, size); }
static void heap_caps_free(void *p) { free(p); }
static int nvs_open(const char *s, int mode, nvs_handle_t *h) { (void)s; (void)mode; *h = 1; return ESP_OK; }
static int nvs_get_blob(nvs_handle_t h, const char *k, void *out, size_t *len) {
    /* 主机测试没有 NVS：一律“键不存在”，走默认值/本地兜底 */
    (void)h; (void)k; (void)out; *len = 0; return ESP_ERR_NVS_NOT_FOUND;
}
static int nvs_set_blob(nvs_handle_t h, const char *k, const void *v, size_t len) {
    (void)h; (void)k; (void)v; (void)len; return ESP_OK;
}
static int nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
static void nvs_close(nvs_handle_t h) { (void)h; }

static SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
static int xSemaphoreTake(SemaphoreHandle_t m, unsigned t) { (void)m; (void)t; return 1; }
static void xSemaphoreGive(SemaphoreHandle_t m) { (void)m; }
static TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)1; }
/* 返回 NULL：cache 任务创建“失败”，load 走同线程直接读路径，
 * 主机测试才能真正同步拿到结果（真机上由内部 RAM 栈任务落盘）。 */
static TaskHandle_t xTaskCreateStatic(void (*fn)(void *), const char *name, int stack,
                                      void *arg, int pri, StackType_t *s, StaticTask_t *t) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)pri; (void)s; (void)t; return NULL;
}
static unsigned ulTaskNotifyTake(int clear, unsigned ticks) { (void)clear; (void)ticks; return 1; }
static void xTaskNotifyGive(TaskHandle_t h) { (void)h; }
