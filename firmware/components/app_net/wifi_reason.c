#include "wifi_reason.h"

/* 取值来自 esp_wifi_types.h 的 wifi_err_reason_t，只列排障常用的子集。
 * 15/204 是 PSK 密码错误或握手失败，201 是扫不到 SSID，202/203 是认证/关联被拒。 */
const char *wifi_reason_str(int reason) {
    switch (reason) {
    case 1:   return "UNSPECIFIED";
    case 2:   return "AUTH_EXPIRE";
    case 4:   return "ASSOC_EXPIRE";
    case 8:   return "ASSOC_LEAVE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    case 206: return "AP_TSF_RESET";
    case 207: return "ROAMING";
    default:  return "UNKNOWN";
    }
}
