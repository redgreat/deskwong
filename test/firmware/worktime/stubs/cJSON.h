#pragma once
#include <stdint.h>
#include <stdbool.h>

/* cJSON 最小替代：主机测试只需要编译期结构和“解析失败”语义 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON {
    int type;
    int valueint;
    double valuedouble;
    char *valuestring;
    struct cJSON *child;
    struct cJSON *next;
} cJSON;

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *name);
int cJSON_IsArray(const cJSON *item);
int cJSON_IsString(const cJSON *item);
int cJSON_IsNumber(const cJSON *item);
double cJSON_GetNumberValue(const cJSON *item);
void cJSON_Delete(cJSON *item);

#define cJSON_ArrayForEach(element, array) \
    for (element = (((array) != NULL) ? (array)->child : NULL); \
         (element != NULL); (element) = (element)->next)

#ifdef __cplusplus
}
#endif
