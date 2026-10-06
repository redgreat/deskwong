#pragma once
#include <stdint.h>
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

#ifdef __cplusplus
}
#endif
