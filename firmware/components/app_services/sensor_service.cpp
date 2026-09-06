#include "sensor_service.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "adc_bsp.h"
#include "esp_log.h"

static Shtc3Port *s_shtc3 = NULL;

void sensor_service_init(void *i2c_bus) {
    I2cMasterBus *bus = (I2cMasterBus *)i2c_bus;
    s_shtc3 = new Shtc3Port(*bus);
    Adc_PortInit();
}

int sensor_service_read(float *temp, float *humi) {
    if (s_shtc3 == NULL) return 1;
    return s_shtc3->Shtc3_ReadTempHumi(temp, humi);
}

uint8_t sensor_battery_level(void) {
    return Adc_GetBatteryLevel();
}

float sensor_battery_voltage(void) {
    return Adc_GetBatteryVoltage();
}
