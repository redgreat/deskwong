#ifndef WIFI_REASON_H
#define WIFI_REASON_H

#ifdef __cplusplus
extern "C" {
#endif

/* WiFi 断开原因码（wifi_err_reason_t）→ 可读短名；未知码返回 "UNKNOWN"。
 * 纯 C 无 ESP 依赖，主机测试直接编译。 */
const char *wifi_reason_str(int reason);

#ifdef __cplusplus
}
#endif

#endif
