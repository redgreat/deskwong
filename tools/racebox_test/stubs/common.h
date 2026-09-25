#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_OK 0
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define pdPASS 1
#define pdTRUE 1
#define portMAX_DELAY 0xffffffff
static int32_t saved_date, saved_points;
static uint8_t saved_accum;
typedef int nvs_handle_t;
typedef void *TaskHandle_t;
typedef int StaticTask_t;
typedef uint32_t StackType_t;
static int64_t esp_timer_get_time(void){return 123456;}
static unsigned esp_random(void){return 123;}
static void *heap_caps_malloc(size_t n,int flags){return malloc(n);}
static void *heap_caps_calloc(size_t n,size_t size,int flags){return calloc(n,size);}
static int nvs_open(const char *s,int mode,nvs_handle_t *h){*h=1;return 0;}
static int nvs_get_i32(nvs_handle_t h,const char *k,int32_t *v){*v=!strcmp(k,"date")?saved_date:saved_points;return 0;}
static int nvs_set_i32(nvs_handle_t h,const char *k,int32_t v){if(!strcmp(k,"date"))saved_date=v;else saved_points=v;return 0;}
static int nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v){*v=saved_accum;return 0;}
static int nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v){saved_accum=v;return 0;}
static int nvs_commit(nvs_handle_t h){return 0;}
static void nvs_close(nvs_handle_t h){}
static int xTaskCreate(void (*fn)(void*),const char *s,int n,void *arg,int pri,TaskHandle_t *h){return 0;}
static TaskHandle_t xTaskCreateStatic(void (*fn)(void*),const char *s,int n,void *arg,int pri,StackType_t *stack,StaticTask_t *tcb){return (TaskHandle_t)1;}
static unsigned ulTaskNotifyTake(int clear,unsigned ticks){return 1;}
static void xTaskNotifyGive(TaskHandle_t h){}
static void vTaskDelay(unsigned ticks){}
#define pdMS_TO_TICKS(ms) (ms)
static void vTaskDelete(void *h){}
