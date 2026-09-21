/* 周期拉取调度器：串行 + 排队 + 错峰
 * 所有网络拉取（天气 / 工时 / AI 用量）共用这一个 worker，
 * 保证同一时刻只有一个 HTTP 请求在飞，且不会互相抢占 http_util 的共享缓冲。 */
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "net_scheduler.h"

static const char *TAG = "sched";

typedef struct {
    net_job_fn fn;
    void *ctx;
    uint32_t period_sec;
    uint32_t countdown;
    bool pending;
    bool registered;
    uint32_t last_run;
} job_t;

static job_t s_jobs[NET_JOB_MAX];
static bool s_online = false;
static uint32_t s_pending = 0;

static uint32_t now_sec(void) {
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

void net_scheduler_init(void) {
    memset(s_jobs, 0, sizeof(s_jobs));
    s_online = false;
    s_pending = 0;
}

void net_scheduler_register(net_job_t id, net_job_fn fn, void *ctx, uint32_t period_sec) {
    if (id >= NET_JOB_MAX) return;
    s_jobs[id].fn = fn;
    s_jobs[id].ctx = ctx;
    s_jobs[id].period_sec = period_sec;
    /* 首次启动按 id 错开，避免开机瞬间一起打网络 */
    s_jobs[id].countdown = period_sec > 0 ? period_sec + (uint32_t)id * 5U : 0;
    s_jobs[id].pending = false;
    s_jobs[id].registered = true;
    s_jobs[id].last_run = 0;
    ESP_LOGI(TAG, "job %d registered period=%us", (int)id, (unsigned)period_sec);
}

void net_scheduler_set_period(net_job_t id, uint32_t period_sec) {
    if (id >= NET_JOB_MAX) return;
    s_jobs[id].period_sec = period_sec;
    if (period_sec == 0) {
        s_jobs[id].countdown = 0;
        return;
    }
    /* 周期变短时立刻生效；变长时按新周期重新计算，但至少留 5 秒缓冲 */
    uint32_t elapsed = now_sec() - s_jobs[id].last_run;
    s_jobs[id].countdown = elapsed >= period_sec ? 5U : period_sec - elapsed;
    ESP_LOGI(TAG, "job %d period -> %us", (int)id, (unsigned)period_sec);
}

void net_scheduler_request(net_job_t id) {
    if (id >= NET_JOB_MAX || !s_jobs[id].registered) return;
    if (!s_jobs[id].pending) {
        s_jobs[id].pending = true;
        s_pending++;
    }
    s_jobs[id].countdown = s_jobs[id].period_sec;
}

void net_scheduler_set_online(bool online) {
    if (online == s_online) return;
    s_online = online;
    ESP_LOGI(TAG, "network %s", online ? "online" : "offline");
    if (online) {
        /* 恢复上线：把有周期的任务排队补跑，靠「一次 tick 只跑一个」自然错峰 */
        for (int i = 0; i < NET_JOB_MAX; i++) {
            if (s_jobs[i].registered && s_jobs[i].period_sec > 0) {
                if (!s_jobs[i].pending) {
                    s_jobs[i].pending = true;
                    s_pending++;
                }
            }
        }
    }
}

void net_scheduler_tick(uint32_t elapsed_sec) {
    for (int i = 0; i < NET_JOB_MAX; i++) {
        job_t *j = &s_jobs[i];
        if (!j->registered || j->period_sec == 0) continue;
        if (j->countdown > elapsed_sec) {
            j->countdown -= elapsed_sec;
        } else {
            j->countdown = 0;
            if (!j->pending) {
                j->pending = true;
                s_pending++;
            }
        }
    }
    if (!s_online || s_pending == 0) return;

    /* 一次 tick 只跑一个任务：排队串行，不并发抢网络 */
    for (int i = 0; i < NET_JOB_MAX; i++) {
        job_t *j = &s_jobs[i];
        if (!j->registered || !j->pending) continue;
        j->pending = false;
        s_pending--;
        j->countdown = j->period_sec;
        j->last_run = now_sec();
        ESP_LOGI(TAG, "run job %d", i);
        if (j->fn) j->fn(j->ctx);
        return;
    }
}

uint32_t net_scheduler_last_run(net_job_t id) {
    if (id >= NET_JOB_MAX) return 0;
    return s_jobs[id].last_run;
}

uint32_t net_scheduler_pending_count(void) {
    return s_pending;
}
