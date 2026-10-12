#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "time_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* i2c_bus 传 I2cMasterBus*（C++ 类型，避免在 C 头文件暴露 class） */
esp_err_t time_service_init(void *i2c_bus, const char *timezone);
void time_service_now(datetime_t *dt);
void time_service_set(const datetime_t *dt);
void time_service_sync_ntp(void);

/* 当前本地时间是否落在 [start_hh, end_hh) 小时区间内（支持跨午夜，如 23→7）。
 * start == end 表示区间为空（即功能未启用）。RTC 时间不可信（掉电未对时）时
 * 一律返回 false——宁可少静默一次，也不能因为读不到时间把设备永久静音。 */
bool time_service_in_hour_range(uint8_t start_hh, uint8_t end_hh);

#ifdef __cplusplus
}
#endif
