#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time_service.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "calendar_service.h"

static const char *TAG = "time";

esp_err_t time_service_init(void *i2c_bus, const char *timezone) {
    /* 默认东八区 Asia/Shanghai */
    setenv("TZ", "CST-8", 1);
    tzset();
    I2cMasterBus *bus = (I2cMasterBus *)i2c_bus;
    Rtc_Setup(bus, 0x51);   // PCF85063
    ESP_LOGI(TAG, "time service init, tz=%s", timezone ? timezone : "Asia/Shanghai");
    return ESP_OK;
}

void time_service_now(datetime_t *dt) {
    rtcTimeStruct_t t;
    Rtc_GetTime(&t);
    dt->year = t.year;
    dt->month = t.month;
    dt->day = t.day;
    dt->hour = t.hour;
    dt->minute = t.minute;
    dt->second = t.second;
    dt->weekday = calendar_weekday(t.year, t.month, t.day);
}

void time_service_set(const datetime_t *dt) {
    Rtc_SetTime((uint16_t)dt->year, (uint8_t)dt->month, (uint8_t)dt->day,
                (uint8_t)dt->hour, (uint8_t)dt->minute, (uint8_t)dt->second);
}

static void ntp_sync_task(void *arg) {
    /* 等 WiFi 起来后同步，重试若干次 */
    for (int i = 0; i < 15; i++) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        time_t now = time(NULL);
        struct tm tmv;
        localtime_r(&now, &tmv);
        if (tmv.tm_year >= (2025 - 1900)) {
            datetime_t dt;
            dt.year = tmv.tm_year + 1900;
            dt.month = tmv.tm_mon + 1;
            dt.day = tmv.tm_mday;
            dt.hour = tmv.tm_hour;
            dt.minute = tmv.tm_min;
            dt.second = tmv.tm_sec;
            time_service_set(&dt);
            ESP_LOGI(TAG, "NTP synced: %04d-%02d-%02d %02d:%02d:%02d",
                     dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
            break;
        }
    }
    vTaskDelete(NULL);
}

void time_service_sync_ntp(void) {
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&cfg);
    xTaskCreate(ntp_sync_task, "ntp_sync", 4096, NULL, 3, NULL);
}
