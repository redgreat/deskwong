#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 接收 RaceBox TX 特征通知的回调（data 为 UBX 帧） */
typedef void (*racebox_ble_rx_cb_t)(const uint8_t *data, int len);
/* 连接状态变化回调 */
typedef void (*racebox_ble_conn_cb_t)(bool connected);
/* 扫描结束（超时/取消）且未连上设备的回调 */
typedef void (*racebox_ble_scan_done_cb_t)(void);
/* 扫描中发现匹配设备（可能多次）的回调 */
typedef void (*racebox_ble_disc_cb_t)(const char *name);

void racebox_ble_init(void);
/* 开始扫描并连接 RaceBox（Nordic UART 服务） */
void racebox_ble_start(void);
void racebox_ble_stop(void);
bool racebox_ble_is_connected(void);
/* 把最近一次匹配到的设备名拷贝到 out */
void racebox_ble_peer_name(char *out, size_t len);
/* 设备过滤：locked 非空时只允许完全匹配（忽略大小写）该名称的设备，
 * 否则按 prefix 前缀匹配（prefix 为空时用 "RaceBox"）。 */
void racebox_ble_set_filter(const char *prefix, const char *locked);
void racebox_ble_set_scan_done_cb(racebox_ble_scan_done_cb_t cb);
void racebox_ble_set_disc_cb(racebox_ble_disc_cb_t cb);
/* 写命令到 RX 特征，成功返回 0 */
int racebox_ble_send(const uint8_t *cmd, int len);
void racebox_ble_set_rx_cb(racebox_ble_rx_cb_t cb);
void racebox_ble_set_conn_cb(racebox_ble_conn_cb_t cb);

#ifdef __cplusplus
}
#endif
