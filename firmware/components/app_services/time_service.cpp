#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time_service.h"
#include "time_util.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "calendar_service.h"

static const char *TAG = "time";

/* 系统时间与 RTC 相差超过该值才写回 RTC（RTC 秒级精度 + I2C 读取耗时，±1s 抖动不算漂移） */
#define TIME_RTC_DRIFT_THRESHOLD_SEC 2
/* 首次同步前的等待轮询间隔；之后每 TIME_RTC_RECHECK_SEC 校一次漂移 */
#define TIME_SNTP_POLL_MS (10 * 1000)
#define TIME_RTC_RECHECK_SEC (30 * 60)

static bool datetime_valid(int year, int month, int day, int hour, int minute, int second) {
    return year >= 2025 && year <= 2099 && month >= 1 && month <= 12 &&
           day >= 1 && day <= 31 && hour <= 23 && minute <= 59 && second <= 59;
}

static datetime_t build_datetime(void) {
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {};
    datetime_t dt = {};
    sscanf(__DATE__, "%3s %d %d", mon, &dt.day, &dt.year);
    const char *p = strstr(months, mon);
    dt.month = p ? (int)((p - months) / 3) + 1 : 1;
    sscanf(__TIME__, "%d:%d:%d", &dt.hour, &dt.minute, &dt.second);
    return dt;
}

esp_err_t time_service_init(void *i2c_bus, const char *timezone) {
    // ESP-IDF/newlib uses POSIX TZ strings, not desktop IANA zone files.
    const char *tz = timezone && *timezone ? timezone : "Asia/Shanghai";
    setenv("TZ", strcmp(tz, "Asia/Shanghai") == 0 ? "CST-8" : tz, 1);
    tzset();
    I2cMasterBus *bus = (I2cMasterBus *)i2c_bus;
    Rtc_Setup(bus, 0x51);   // PCF85063
    rtcTimeStruct_t rtc = {};
    Rtc_GetTime(&rtc);
    if (!datetime_valid(rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second)) {
        datetime_t fallback = build_datetime();
        time_service_set(&fallback);
        ESP_LOGW(TAG, "RTC invalid (likely backup lost), initialized from build time: %04d-%02d-%02d %02d:%02d:%02d",
                 fallback.year, fallback.month, fallback.day,
                 fallback.hour, fallback.minute, fallback.second);
        Rtc_GetTime(&rtc);
    }
    /* RTC 时间灌回系统时钟：time(NULL)/TLS 证书校验立刻有合理值，
     * NTP 没跑起来之前系统时钟不会停在 1970。 */
    if (datetime_valid(rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second)) {
        struct tm tmv = {};
        tmv.tm_year = rtc.year - 1900;
        tmv.tm_mon = rtc.month - 1;
        tmv.tm_mday = rtc.day;
        tmv.tm_hour = rtc.hour;
        tmv.tm_min = rtc.minute;
        tmv.tm_sec = rtc.second;
        time_t base = mktime(&tmv);
        if (base > 0) {
            struct timeval tv = { .tv_sec = base, .tv_usec = 0 };
            settimeofday(&tv, NULL);
        }
    }
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

static bool sys_datetime(datetime_t *dt) {
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    if (!datetime_valid(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                        tmv.tm_hour, tmv.tm_min, tmv.tm_sec)) {
        return false;
    }
    dt->year = tmv.tm_year + 1900;
    dt->month = tmv.tm_mon + 1;
    dt->day = tmv.tm_mday;
    dt->hour = tmv.tm_hour;
    dt->minute = tmv.tm_min;
    dt->second = tmv.tm_sec;
    return true;
}

static void ntp_sync_task(void *arg) {
    /* RTC 掉电后（比如电池耗尽再开机）唯一可靠的对时来源就是 SNTP。
     * 网络什么时候就绪就等到什么时候，绝不限时放弃——路由器可能比设备
     * 晚几分钟才恢复。SNTP 首次同步成功后把系统时间写回 RTC；之后
     * SNTP 会按周期刷新系统时间，这里定期把漂移写回 RTC 纠偏。 */
    bool synced_once = false;
    bool first_logged = false;
    int wait_rounds = 0;
    for (;;) {
        if (!synced_once) {
            if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(TIME_SNTP_POLL_MS)) != ESP_OK) {
                if (++wait_rounds % 60 == 1) {
                    ESP_LOGW(TAG, "waiting for first SNTP sync (%d s)...", wait_rounds * TIME_SNTP_POLL_MS / 1000);
                }
                continue;
            }
            synced_once = true;
        } else {
            vTaskDelay(pdMS_TO_TICKS(TIME_RTC_RECHECK_SEC * 1000));
        }
        datetime_t sys = {};
        if (!sys_datetime(&sys)) continue;
        rtcTimeStruct_t rtc = {};
        Rtc_GetTime(&rtc);
        bool rtc_ok = datetime_valid(rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second);
        long long drift = 0;
        if (rtc_ok) {
            datetime_t rdt = { rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second, 0 };
            drift = time_util_diff_seconds(&sys, &rdt);
        }
        if (!first_logged) {
            first_logged = true;
            ESP_LOGI(TAG, "SNTP synced: sys=%04d-%02d-%02d %02d:%02d:%02d rtc=%s drift=%llds",
                     sys.year, sys.month, sys.day, sys.hour, sys.minute, sys.second,
                     rtc_ok ? "valid" : "invalid", drift);
        }
        if (!rtc_ok || llabs(drift) >= TIME_RTC_DRIFT_THRESHOLD_SEC) {
            time_service_set(&sys);
            ESP_LOGI(TAG, "NTP -> RTC: %04d-%02d-%02d %02d:%02d:%02d",
                     sys.year, sys.month, sys.day, sys.hour, sys.minute, sys.second);
        }
    }
}

void time_service_sync_ntp(void) {
    static bool started = false;
    if (started) return;
    started = true;
    /* pool.ntp.org 在国内网络经常超时，默认用阿里云 NTP */
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&cfg));
    xTaskCreate(ntp_sync_task, "ntp_sync", 4096, NULL, 3, NULL);
}
