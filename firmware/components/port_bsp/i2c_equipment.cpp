#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include "i2c_equipment.h"
#include "i2c_bsp.h"

/* ======================= SHTC3 温湿度传感器 ======================= */

Shtc3Port::Shtc3Port(I2cMasterBus& i2cbus) :
i2cbus_(i2cbus) {
    i2c_master_bus_handle_t I2cMasterBus = i2cbus_.Get_I2cBusHandle();
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = Shtc3Address;
    dev_cfg.scl_speed_hz    = 400000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(I2cMasterBus, &dev_cfg, &I2c_DevShtc3));

    Shtc3_Wakeup();
    Shtc3_SoftReset();
    vTaskDelay(pdMS_TO_TICKS(20)); //20MS
    Shtc3_GetId();
    ESP_LOGI(TAG, "ID:%04x", shtc3_id);
}

Shtc3Port::~Shtc3Port() {
}

etError Shtc3Port::Shtc3_GetId() {
    uint8_t senBuf[2]  = {(READ_ID >> 8), (READ_ID & 0xff)};
    uint8_t readBuf[3] = {0, 0, 0};
    int     err        = i2cbus_.i2c_master_write_read_dev(I2c_DevShtc3, senBuf, 2, readBuf, 3);
    etError error      = (err == ESP_OK) ? NO_ERROR : ACK_ERROR;
    if (error != NO_ERROR) {
        ESP_LOGE("shtc3", "GetId WRITE Failure");
        return error;
    }
    error = Shtc3_CheckCrc(readBuf, 2, readBuf[2]);
    if (error != NO_ERROR) {
        ESP_LOGE("shtc3", "GetId CRC Failure");
        return error;
    }
    shtc3_id = ((readBuf[0] << 8) | readBuf[1]);
    return error;
}

uint16_t Shtc3Port::Shtc3_GetShtc3Id() {
    return shtc3_id;
}

etError Shtc3Port::Shtc3_Wakeup() {
    uint8_t senBuf[2] = {(WAKEUP >> 8), (WAKEUP & 0xff)};
    int     err       = i2cbus_.i2c_write_buff(I2c_DevShtc3, -1, senBuf, 2);
    etError error     = (err == ESP_OK) ? NO_ERROR : ACK_ERROR;
    vTaskDelay(pdMS_TO_TICKS(50)); //50MS
    if (error != NO_ERROR)
        ESP_LOGE("shtc3", "Wakeup Failure");
    return error;
}

etError Shtc3Port::Shtc3_SoftReset() {
    uint8_t senBuf[2] = {(SOFT_RESET >> 8), (SOFT_RESET & 0xff)};
    int     err       = i2cbus_.i2c_write_buff(I2c_DevShtc3, -1, senBuf, 2);
    etError error     = (err == ESP_OK) ? NO_ERROR : ACK_ERROR;
    if (error != NO_ERROR)
        ESP_LOGE("shtc3", "SoftReset Failure");
    return error;
}

etError Shtc3Port::Shtc3_CheckCrc(uint8_t data[], uint8_t nbrOfBytes, uint8_t checksum) {
    uint8_t bit;        // bit mask
    uint8_t crc = 0xFF; // calculated checksum
    uint8_t byteCtr;    // byte counter

    for (byteCtr = 0; byteCtr < nbrOfBytes; byteCtr++) {
        crc ^= (data[byteCtr]);
        for (bit = 8; bit > 0; --bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC_POLYNOMIAL;
            } else {
                crc = (crc << 1);
            }
        }
    }

    if (crc != checksum) {
        return CHECKSUM_ERROR;
    } else {
        return NO_ERROR;
    }
}

float Shtc3Port::Shtc3_CalcTemperature(uint16_t rawValue) {
    return 175 * (float) rawValue / 65536.0f - 45.0f - SHTC3_PETP_VOL;
}

float Shtc3Port::Shtc3_CalcHumidity(uint16_t rawValue) {
    return 100 * (float) rawValue / 65536.0f;
}

etError Shtc3Port::Shtc3_GetTempAndHumiPolling(float *temp, float *humi) {
    int      err = 0;
    etError  error;
    uint16_t rawValueTemp;
    uint16_t rawValueHumi;
    uint8_t  bytes[6] = {0};
    uint8_t senBuf[2] = {(MEAS_T_RH_POLLING >> 8), (MEAS_T_RH_POLLING & 0xff)};
    err               = i2cbus_.i2c_write_buff(I2c_DevShtc3, -1, senBuf, 2);
    error             = (err == ESP_OK) ? NO_ERROR : ACK_ERROR;
    if (error != NO_ERROR) {
        ESP_LOGE("shtc3", "GetTempAndHumi WRITE Failure");
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    err   = i2cbus_.i2c_read_buff(I2c_DevShtc3, -1, bytes, 6);
    error = (err == ESP_OK) ? NO_ERROR : ACK_ERROR;
    if (error != NO_ERROR) {
        ESP_LOGE("shtc3", "GetTempAndHumi READ Failure");
        return error;
    }
    error = Shtc3_CheckCrc(bytes, 2, bytes[2]);
    if (error != NO_ERROR) {
        ESP_LOGE("shtc3", "GetTempAndHumi TempCRC Failure");
        return error;
    }
    error = Shtc3_CheckCrc(&bytes[3], 2, bytes[5]);
    if (error != NO_ERROR) {
        ESP_LOGE("shtc3", "GetTempAndHumi humidityCRC Failure");
        return error;
    }
    rawValueTemp = (bytes[0] << 8) | bytes[1];
    rawValueHumi = (bytes[3] << 8) | bytes[4];
    *temp        = Shtc3_CalcTemperature(rawValueTemp);
    *humi        = Shtc3_CalcHumidity(rawValueHumi);
    return error;
}

etError Shtc3Port::Shtc3_Sleep() {
    uint8_t senBuf[2] = {(SLEEP >> 8), (SLEEP & 0xff)};
    int     err       = i2cbus_.i2c_write_buff(I2c_DevShtc3, -1, senBuf, 2);
    etError error     = (err == ESP_OK) ? NO_ERROR : ACK_ERROR;
    if (error != NO_ERROR)
        ESP_LOGE("shtc3", "Sleep Failure");
    return error;
}

uint8_t Shtc3Port::Shtc3_ReadTempHumi(float *t,float *h) {
    etError      error;
    Shtc3_Wakeup();
    error = Shtc3_GetTempAndHumiPolling(t, h);
    if (error != NO_ERROR) {
        ESP_LOGW("shtc3", "error:%d", error);
        return 1;
    }
    Shtc3_Sleep();
    return 0;
}

/* ======================= PCF85063 RTC（轻量驱动） ======================= */

static i2c_master_dev_handle_t I2cRTCdev = NULL;
static I2cMasterBus           *I2cbus_   = NULL;

static uint8_t bcd2dec(uint8_t bcd) {
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

static uint8_t dec2bcd(uint8_t dec) {
    return (uint8_t)(((dec / 10) << 4) | (dec % 10));
}

void Rtc_Setup(I2cMasterBus *i2cbus,uint8_t dev_addr) {
    I2cbus_ = i2cbus;
    if (I2cRTCdev == NULL) {
        i2c_master_bus_handle_t BusHandle = i2cbus->Get_I2cBusHandle();
        i2c_device_config_t     dev_cfg   = {};
        dev_cfg.dev_addr_length           = I2C_ADDR_BIT_LEN_7;
        dev_cfg.scl_speed_hz              = 300000;
        dev_cfg.device_address            = dev_addr;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(BusHandle, &dev_cfg, &I2cRTCdev));
    }
}

void Rtc_SetTime(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint8_t second) {
    if (I2cRTCdev == NULL) return;
    uint8_t buf[7] = {
        (uint8_t)(dec2bcd(second) & 0x7F),  // 0x00 seconds, 清 OS 标志
        dec2bcd(minute),                    // 0x01 minutes
        dec2bcd(hour),                      // 0x02 hours
        dec2bcd(day),                       // 0x03 days (weekday 位写 0)
        dec2bcd(month),                     // 0x04 months
        dec2bcd((uint8_t)(year % 100)),     // 0x05 years
        0x00                                // 0x06 control
    };
    I2cbus_->i2c_write_buff(I2cRTCdev, 0x00, buf, 7);
}

void Rtc_GetTime(rtcTimeStruct_t *time) {
    if (I2cRTCdev == NULL) return;
    uint8_t buf[7] = {0};
    int ret = I2cbus_->i2c_read_buff(I2cRTCdev, 0x00, buf, 7);
    if (ret != ESP_OK) {
        ESP_LOGW("rtc", "read failed");
        return;
    }
    time->second = bcd2dec((uint8_t)(buf[0] & 0x7F));
    time->minute = bcd2dec((uint8_t)(buf[1] & 0x7F));
    time->hour   = bcd2dec((uint8_t)(buf[2] & 0x3F));
    time->day    = bcd2dec((uint8_t)(buf[3] & 0x3F));
    time->month  = bcd2dec((uint8_t)(buf[4] & 0x1F));
    time->year   = 2000 + bcd2dec(buf[5]);
    time->week   = 0; // 由调用方按日期另行计算
}
