/*
 * voice_wake.h —— ESP-SR 唤醒词常驻监听（WakeNet9，"你好小智"）
 *
 * feed 任务常驻读取录音句柄喂 AFE；fetch 任务检测到 WAKENET_DETECTED 后
 * 停读 codec、播提示音并启动语音会话（start_by_wake）。
 * 会话期间监听自动暂停（无 AEC，避免喇叭自唤醒），teardown 后恢复。
 */
#ifndef VOICE_WAKE_H
#define VOICE_WAKE_H

#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 AFE 并创建 feed/fetch 任务。模型从 "model" 分区读取。
 * 是否真正监听由门控决定（voice_enabled + listen_mode + WiFi + 会话态），
 * 配置改了即刻生效，无需重建。 */
void voice_wake_init(const app_config_t *cfg);

/* 会话开始前调用：停读录音句柄并把 feed 任务停稳（至多一个音频帧的等待）。
 * 返回 false 表示停稳超时，调用方不应继续独占录音句柄。 */
bool voice_wake_suspend(int timeout_ms);

/* 会话结束（teardown/采集线程退出）后调用，恢复监听。幂等。 */
void voice_wake_resume(void);

#ifdef __cplusplus
}
#endif
#endif /* VOICE_WAKE_H */
