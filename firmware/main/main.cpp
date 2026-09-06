#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "user_config.h"
#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "button_bsp.h"

#include "app_config.h"
#include "wifi_sta.h"
#include "http_server.h"
#include "time_service.h"
#include "sensor_service.h"
#include "calendar_service.h"
#include "holiday_service.h"
#include "weather_service.h"
#include "worktime_service.h"
#include "aiusage_service.h"
#include "reminder_service.h"
#include "app_mqtt.h"
#include "racebox_service.h"
#include "audio_service.h"
#include "main_screen.h"

static const char *TAG = "main";

/* 竖屏 300×400 单色反射屏 */
static DisplayPort RlcdPort(RLCD_MOSI_PIN, RLCD_SCK_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN, LCD_WIDTH, LCD_HEIGHT);
static I2cMasterBus I2cbus(ESP32_I2C_SCL_PIN, ESP32_I2C_SDA_PIN, 0);

static app_config_t g_cfg;

/* 外部服务数据缓存 */
static weather_now_t g_weather;
static worktime_summary_t g_worktime;
static ai_provider_t g_ai[AI_MAX_PROVIDERS];
static int g_ai_count = 0;
static float g_temp = 0, g_humi = 0;
static uint8_t g_battery = 0;

static void Lvgl_FlushCallback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    uint16_t *buffer = (uint16_t *)color_map;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite;
            RlcdPort.RLCD_SetPixel(x, y, color);
            buffer++;
        }
    }
    RlcdPort.RLCD_Display();
    lv_disp_flush_ready(drv);
}

static const char *week_cn(int wd) {
    static const char *n[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    if (wd < 0 || wd > 6) return "";
    return n[wd];
}

static void on_remind(remind_type_t type) {
    const char *msg = NULL;
    if (type == REMIND_SIGNIN) msg = "准备企微签到";
    else if (type == REMIND_SIGNOUT) msg = "准备企微签退";
    else if (type == REMIND_WORKTIME) msg = "记得记录今日工时";
    if (msg) {
        if (Lvgl_lock(100)) {
            main_screen_show_remind(msg);
            Lvgl_unlock();
        }
        audio_service_beep();
        ESP_LOGI(TAG, "remind: %s", msg);
    }
}

static void prepare_calendar(int year, int month, int today, calendar_cell_t cells[42]) {
    int first_wd = calendar_weekday(year, month, 1);
    int days = calendar_month_days(year, month);
    memset(cells, 0, sizeof(calendar_cell_t) * 42);
    for (int d = 1; d <= days; d++) {
        int idx = first_wd + d - 1;
        if (idx < 0 || idx >= 42) continue;
        calendar_cell_t *c = &cells[idx];
        c->day = d;
        lunar_date_t ld;
        if (calendar_solar_to_lunar(year, month, d, &ld) == 0) {
            calendar_lunar_day_str(&ld, c->lunar, sizeof(c->lunar));
        } else {
            c->lunar[0] = 0;
        }
        const char *hn = NULL;
        int ht = holiday_query(year, month, d, &hn);
        int wd = calendar_weekday(year, month, d);
        bool weekend = (wd == 0 || wd == 6);
        if (ht == DAY_HOLIDAY) {
            c->type = CAL_HOLIDAY;
            if (hn && hn[0]) strncpy(c->lunar, hn, sizeof(c->lunar) - 1);
        } else if (ht == DAY_WORKDAY) {
            c->type = CAL_WORKDAY;
            strcpy(c->lunar, "班");
        } else if (weekend) {
            c->type = CAL_WEEKEND;
        } else {
            c->type = CAL_NORMAL;
        }
        if (d == today) c->is_today = true;
    }
}

static void build_bottom(char *buf, size_t len) {
    char ai[64] = "";
    if (g_ai_count > 0) {
        snprintf(ai, sizeof(ai), " %s%s", g_ai[0].name, g_ai[0].display);
    }
    snprintf(buf, len, "%s%dC 室内%.0fC%.0f%% %.0f/%.0fh%s",
             g_weather.text, g_weather.temp, g_temp, g_humi,
             g_worktime.recorded_hours, g_worktime.expected_hours, ai);
}

static void fetch_services(void) {
    if (weather_service_fetch(&g_weather) == 0) {
        ESP_LOGI(TAG, "weather: %s %dC", g_weather.text, g_weather.temp);
    }
    datetime_t now;
    time_service_now(&now);
    worktime_service_fetch(now.year, now.month, &g_worktime);
    aiusage_service_fetch(g_ai, AI_MAX_PROVIDERS, &g_ai_count);
}

static void ui_update_task(void *arg) {
    char date[16], time_str[8], lunar_full[32];
    calendar_cell_t cells[42];
    uint32_t tick = 0;
    while (1) {
        datetime_t dt;
        time_service_now(&dt);
        reminder_service_tick(&dt);

        snprintf(date, sizeof(date), "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", dt.hour, dt.minute);

        lunar_date_t ld;
        if (calendar_solar_to_lunar(dt.year, dt.month, dt.day, &ld) == 0) {
            calendar_lunar_str(&ld, lunar_full, sizeof(lunar_full));
        } else {
            strcpy(lunar_full, "");
        }

        sensor_service_read(&g_temp, &g_humi);
        g_battery = sensor_battery_level();
        char ip[16] = {0};
        wifi_get_ip(ip, sizeof(ip));

        prepare_calendar(dt.year, dt.month, dt.day, cells);
        char bottom[128];
        build_bottom(bottom, sizeof(bottom));

        if (Lvgl_lock(100)) {
            main_screen_update_time(date, time_str, week_cn(dt.weekday), lunar_full);
            main_screen_update_calendar(cells);
            char b2[160];
            snprintf(b2, sizeof(b2), "%s | %s%d%% %s", bottom,
                     wifi_is_connected() ? "WiFi" : "AP", g_battery, ip);
            main_screen_update_bottom(b2);
            Lvgl_unlock();
        }

        if (tick % 600 == 0) {
            fetch_services();
        }
        tick++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void button_task(void *arg) {
    while (1) {
        /* KEY（GPIO18）单击 → 触发 RaceBox 采集 */
        EventBits_t bits = xEventGroupWaitBits(GP18ButtonGroups, 0x07, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (bits & 0x01) {
            ESP_LOGI(TAG, "KEY click -> racebox trigger");
            racebox_service_trigger();
        }
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "deskwong boot");

    app_config_init();
    app_config_load(&g_cfg);

    Custom_ButtonInit();
    time_service_init(&I2cbus, g_cfg.timezone);
    sensor_service_init(&I2cbus);

    weather_service_init(g_cfg.weather_location, g_cfg.weather_key);
    worktime_service_init(g_cfg.worktime_api_base, g_cfg.worktime_token);
    aiusage_service_init(g_cfg.aiusage_api_base, g_cfg.aiusage_token);
    reminder_service_init(on_remind);
    reminder_service_set_times(g_cfg.remind_signin_hh, g_cfg.remind_signin_mm,
                               g_cfg.remind_signout_hh, g_cfg.remind_signout_mm,
                               g_cfg.remind_worktime_hh, g_cfg.remind_worktime_mm);
    reminder_service_set_enabled(g_cfg.remind_enabled);

    app_mqtt_init(g_cfg.mqtt_broker, g_cfg.mqtt_port, g_cfg.mqtt_user, g_cfg.mqtt_pass, NULL);
    racebox_service_init();
    audio_service_init();

    RlcdPort.RLCD_Init();
    Lvgl_PortInit(LCD_WIDTH, LCD_HEIGHT, Lvgl_FlushCallback);
    if (Lvgl_lock(-1)) {
        main_screen_init(LCD_WIDTH, LCD_HEIGHT);
        Lvgl_unlock();
    }

    wifi_init(g_cfg.wifi_ssid, g_cfg.wifi_pass);
    http_server_start(&g_cfg);
    time_service_sync_ntp();

    xTaskCreatePinnedToCore(ui_update_task, "ui", 8 * 1024, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(button_task, "btn", 4 * 1024, NULL, 3, NULL, 1);

    ESP_LOGI(TAG, "boot done");
}
