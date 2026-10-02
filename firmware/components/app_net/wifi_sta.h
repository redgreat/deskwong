#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 WiFi：优先 STA 连接配置的 SSID；SSID 为空则进入 AP 配置模式 */
esp_err_t wifi_init(const char *ssid, const char *pass);
/* 同上，但 with_ap=true 时在 STA 之上叠加配网热点（APSTA 共存）：
 * 已保存的网络继续连接，同时开放 deskwong-setup 供配网/门户代理使用 */
esp_err_t wifi_init_ex(const char *ssid, const char *pass, bool with_ap);
/* 运行中叠加配网热点（APSTA 共存，不重启不断 STA）；已在配网模式时幂等返回。
 * 热点仅在本次开机内有效，重启后自动关闭。 */
esp_err_t wifi_start_ap(void);
bool wifi_is_connected(void);
bool wifi_is_ap_mode(void);
void wifi_get_ip(char *ip, size_t len);

#ifdef __cplusplus
}
#endif
