#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char text[32];
    int temp;
    int humidity;
    int wind_scale;
} weather_now_t;

void weather_service_init(const char *location, const char *key);
/* 拉取当前天气，成功返回 0，失败返回 1 */
int weather_service_fetch(weather_now_t *out);

#ifdef __cplusplus
}
#endif
