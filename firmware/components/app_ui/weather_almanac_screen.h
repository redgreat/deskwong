#pragma once
#include <stdbool.h>
#include "weather_service.h"
#ifdef __cplusplus
extern "C" {
#endif
void weather_almanac_screen_init(int width, int height);
void weather_almanac_screen_update(const weather_detail_t *detail);
void weather_almanac_screen_show(void);
void weather_almanac_screen_hide(void);
bool weather_almanac_screen_visible(void);
#ifdef __cplusplus
}
#endif
