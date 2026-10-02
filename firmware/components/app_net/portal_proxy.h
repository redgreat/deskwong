#pragma once
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GET /p/<host>/<path>：经设备 STA 抓取 http://<host><path> 并返回；
 * HTML 内容做链接改写，其余原样透传。门户页代理的核心入口。 */
esp_err_t portal_proxy_handler(httpd_req_t *req);

/* GET /p/check：经设备 STA 请求探测 URL（不跟随重定向），
 * 返回 JSON {online, status, location}，location 已改写为 /p/... 本地地址 */
esp_err_t portal_check_handler(httpd_req_t *req);

/* GET /p/start：探测 → 302 跳到 /p/... 门户页（浏览器一步进入配网代理流程）；
 * 已放行时返回提示 JSON；探测返回 200 门户页时直接代理渲染 */
esp_err_t portal_start_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
