#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "driver/gpio.h"

/* LCD（单色全反射 RLCD，竖屏 300×400） */
#define LCD_WIDTH      300
#define LCD_HEIGHT     400

#define RLCD_DC_PIN    GPIO_NUM_5
#define RLCD_CS_PIN    GPIO_NUM_40
#define RLCD_SCK_PIN   GPIO_NUM_11
#define RLCD_MOSI_PIN  GPIO_NUM_12
#define RLCD_RST_PIN   GPIO_NUM_41
#define RLCD_TE_PIN    GPIO_NUM_6

/* I2C（SHTC3 / PCF85063） */
#define ESP32_I2C_SDA_PIN   GPIO_NUM_13
#define ESP32_I2C_SCL_PIN   GPIO_NUM_14

/* 按键 */
#define KEY_BOOT_PIN   GPIO_NUM_0
#define KEY_GP18_PIN   GPIO_NUM_18

/* 设备地址 */
#define SHTC3_ADDR     0x70
#define PCF85063_ADDR  0x51

#endif
