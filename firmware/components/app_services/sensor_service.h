#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* i2c_bus 传 I2cMasterBus*（C++ 类型，避免在 C 头文件暴露 class） */
void sensor_service_init(void *i2c_bus);
/* 读取温湿度，成功返回 0，失败返回 1 */
int sensor_service_read(float *temp, float *humi);
uint8_t sensor_battery_level(void);
float sensor_battery_voltage(void);

#ifdef __cplusplus
}
#endif
