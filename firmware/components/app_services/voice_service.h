#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "app_config.h"

/* 语音会话状态（对齐小智协议的设备状态机） */
typedef enum {
    VOICE_IDLE = 0,
    VOICE_CONNECTING,
    VOICE_LISTENING,
    VOICE_SPEAKING,
} voice_state_t;

/* 推送给 UI 的一次性事件，text/emotion 可能为 NULL */
typedef struct {
    voice_state_t state;
    const char *text;
    const char *emotion;
} voice_event_t;

typedef void (*voice_event_cb_t)(const voice_event_t *ev, void *ctx);

/* 初始化，不联网。cfg 指针必须长期有效（传 &g_cfg）。 */
void voice_service_init(const app_config_t *cfg);
void voice_service_set_event_cb(voice_event_cb_t cb, void *ctx);

/* 开始一轮对话（会建链、握手、开始推流）。可在任务或 HTTP 线程调用。 */
bool voice_service_start(void);
/* 结束/中断当前对话并释放链路资源 */
void voice_service_stop(void);

voice_state_t voice_service_state(void);
/* 最近一次失败原因，NULL 表示没有错误 */
const char *voice_service_last_error(void);

#ifdef __cplusplus
}
#endif
