#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 周期拉取任务：天气 / 工时 / AI 用量。
 * 设计要点（避免互相冲突）：
 *   1. 所有网络请求都在同一个 worker 里串行执行，http_util 的共享缓冲不会互相踩；
 *   2. 到期的任务只是「入队」，每次 tick 最多执行一个，天然错峰，不会三件事同时打网络；
 *   3. 配置变更或网络恢复时可以 request 一次，同样走队列，不会打断正在跑的任务。
 */
typedef enum {
    NET_JOB_WEATHER = 0,
    NET_JOB_WORKTIME,
    NET_JOB_AIUSAGE,
    NET_JOB_MAX
} net_job_t;

typedef void (*net_job_fn)(void *ctx);

void net_scheduler_init(void);

/* 注册任务。period_sec 为周期（秒），0 表示只按需触发 */
void net_scheduler_register(net_job_t id, net_job_fn fn, void *ctx, uint32_t period_sec);

/* 修改周期（网页保存配置后调用） */
void net_scheduler_set_period(net_job_t id, uint32_t period_sec);

/* 立即安排一次（配置变更、网络恢复、语音指令等） */
void net_scheduler_request(net_job_t id);

/* 网络可用性：离线时任务不出队但不丢，恢复上线后立刻补跑（错峰） */
void net_scheduler_set_online(bool online);

/* 主循环调用；elapsed_sec 为距上次调用经过的秒数。内部最多执行一个任务。 */
void net_scheduler_tick(uint32_t elapsed_sec);

/* 诊断：某任务上次执行的时间戳（秒，自启动起） */
uint32_t net_scheduler_last_run(net_job_t id);
uint32_t net_scheduler_pending_count(void);

#ifdef __cplusplus
}
#endif
