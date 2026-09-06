#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 WiFi：优先 STA 连接配置的 SSID；SSID 为空则进入 AP 配置模式 */
esp_err_t wifi_init(const char *ssid, const char *pass);
bool wifi_is_connected(void);
bool wifi_is_ap_mode(void);
void wifi_get_ip(char *ip, size_t len);

#ifdef __cplusplus
}
#endif
