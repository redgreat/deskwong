#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 接收 RaceBox TX 特征通知的回调（data 为 UBX 帧） */
typedef void (*racebox_ble_rx_cb_t)(const uint8_t *data, int len);
/* 连接状态变化回调 */
typedef void (*racebox_ble_conn_cb_t)(bool connected);

void racebox_ble_init(void);
/* 开始扫描并连接 RaceBox（Nordic UART 服务） */
void racebox_ble_start(void);
void racebox_ble_stop(void);
bool racebox_ble_is_connected(void);
/* 写命令到 RX 特征，成功返回 0 */
int racebox_ble_send(const uint8_t *cmd, int len);
void racebox_ble_set_rx_cb(racebox_ble_rx_cb_t cb);
void racebox_ble_set_conn_cb(racebox_ble_conn_cb_t cb);

#ifdef __cplusplus
}
#endif
