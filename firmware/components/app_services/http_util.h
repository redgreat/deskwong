#pragma once
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GET 请求并解析 JSON，返回 cJSON*（调用方 cJSON_Delete）；失败返回 NULL */
cJSON *http_get_json(const char *url, const char *bearer_token);
/* POST JSON；仅返回 HTTP 2xx 是否成功，不记录请求体或 Token。 */
bool http_post_json(const char *url, const char *bearer_token, const char *device_id,
                    const char *json_body);

#ifdef __cplusplus
}
#endif
