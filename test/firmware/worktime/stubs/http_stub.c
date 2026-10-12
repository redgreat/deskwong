/* 主机测试桩：JSON 解析与 HTTP 拉取都不真正执行，统一返回失败，
 * 用于验证 worktime_service 的本地兜底与已过工作日折算口径。 */
#include "http_util.h"

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *name) { (void)object; (void)name; return NULL; }
int cJSON_IsArray(const cJSON *item) { (void)item; return 0; }
int cJSON_IsString(const cJSON *item) { (void)item; return 0; }
int cJSON_IsNumber(const cJSON *item) { (void)item; return 0; }
double cJSON_GetNumberValue(const cJSON *item) { (void)item; return 0.0; }
void cJSON_Delete(cJSON *item) { (void)item; }

cJSON *http_get_json(const char *url, const char *bearer_token) { (void)url; (void)bearer_token; return NULL; }
bool http_post_json(const char *url, const char *bearer_token, const char *device_id,
                    const char *json_body) {
    (void)url; (void)bearer_token; (void)device_id; (void)json_body;
    return false;
}
