#pragma once
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GET 请求并解析 JSON，返回 cJSON*（调用方 cJSON_Delete）；失败返回 NULL */
cJSON *http_get_json(const char *url, const char *bearer_token);

#ifdef __cplusplus
}
#endif
