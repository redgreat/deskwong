#pragma once
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int weekday;   // 0=周日 .. 6=周六
} datetime_t;

/* i2c_bus 传 I2cMasterBus*（C++ 类型，避免在 C 头文件暴露 class） */
esp_err_t time_service_init(void *i2c_bus, const char *timezone);
void time_service_now(datetime_t *dt);
void time_service_set(const datetime_t *dt);
void time_service_sync_ntp(void);

#ifdef __cplusplus
}
#endif
