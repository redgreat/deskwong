#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"

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
#include "net_scheduler.h"
#include "audio_service.h"
#include "main_screen.h"
#include "sync_screen.h"

static const char *TAG = "main";

/* 原生横屏 400×300 单色反射屏 */
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
static volatile bool g_summary_dirty = false;

static void Lvgl_FlushCallback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    static bool first_flush = true;
    static int dirty_x1 = LCD_WIDTH;
    static int dirty_x2 = -1;
    if (area->x1 < dirty_x1) dirty_x1 = area->x1;
    if (area->x2 > dirty_x2) dirty_x2 = area->x2;
    uint16_t *buffer = (uint16_t *)color_map;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite;
            RlcdPort.RLCD_SetPixel(x, y, color);
            buffer++;
        }
    }
    /* LVGL can flush several dirty areas in one refresh cycle. Update the
     * framebuffer for every area, but transfer it to the RLCD only once. */
    if (lv_disp_flush_is_last(drv)) {
        if (first_flush || (dirty_x1 <= 0 && dirty_x2 >= LCD_WIDTH - 1)) {
            RlcdPort.RLCD_Display();
        } else if (dirty_x1 <= dirty_x2) {
            int x1 = dirty_x1 < 0 ? 0 : dirty_x1;
            int x2 = dirty_x2 >= LCD_WIDTH ? LCD_WIDTH - 1 : dirty_x2;
            RlcdPort.RLCD_DisplayXRange((uint16_t)x1, (uint16_t)x2);
        }
        if (first_flush) {
            first_flush = false;
            ESP_LOGI(TAG, "first display frame flushed (%dx%d)", LCD_WIDTH, LCD_HEIGHT);
        }
        dirty_x1 = LCD_WIDTH;
        dirty_x2 = -1;
    }
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
    int first_wd = (calendar_weekday(year, month, 1) + 6) % 7;
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
        if (g_worktime.year == year && g_worktime.month == month) {
            c->work_hours = g_worktime.daily_hours[d];
        }
    }
}

/* ---- 周期拉取任务：交给 net_scheduler 排队串行执行，互不抢占网络 ---- */
static void job_weather(void *ctx) {
    if (weather_service_fetch(&g_weather) == 0) {
        g_summary_dirty = true;
        ESP_LOGI(TAG, "weather refreshed");
    }
}

static void job_worktime(void *ctx) {
    datetime_t now;
    time_service_now(&now);
    worktime_service_fetch(now.year, now.month, &g_worktime);
    g_summary_dirty = true;
}

static void job_aiusage(void *ctx) {
    aiusage_service_fetch(g_ai, AI_MAX_PROVIDERS, &g_ai_count);
    g_summary_dirty = true;
}

/* 配置里的分钟数 → 秒周期，并做下限保护 */
static uint32_t refresh_period(uint16_t minutes, uint32_t min_seconds) {
    uint32_t sec = (uint32_t)minutes * 60U;
    if (sec < min_seconds) sec = min_seconds;
    return sec;
}

static void network_service_task(void *arg) {
    bool last_online = false;
    while (1) {
        bool online = wifi_is_connected();
        if (online != last_online) {
            net_scheduler_set_online(online);
            last_online = online;
        }
        /* 天气配置变更 → 插队刷新一次（仍然排队，不会打断正在跑的任务） */
        if (weather_service_take_changed()) net_scheduler_request(NET_JOB_WEATHER);
        net_scheduler_tick(5);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void ui_update_task(void *arg) {
    char date[16], time_str[12], lunar_full[32] = "";
    calendar_cell_t cells[42];
    uint32_t tick = 0;
    int last_day = -1;
    uint32_t sync_hold = 0;   /* 同步结束后的停留时间（单位：200ms） */
    racebox_state_t last_sync_state = RACEBOX_IDLE;
    while (1) {
        datetime_t dt;
        time_service_now(&dt);

        /* RaceBox 同步弹窗：200ms 轮询，进度条才跟得上 */
        racebox_progress_t prog;
        racebox_service_progress(&prog);
        bool sync_busy = prog.state != RACEBOX_IDLE &&
                         prog.state != RACEBOX_DONE && prog.state != RACEBOX_FAILED;
        bool sync_terminal = prog.state == RACEBOX_DONE || prog.state == RACEBOX_FAILED;
        if (sync_busy) {
            if (Lvgl_lock(100)) {
                sync_screen_update(&prog);
                Lvgl_unlock();
            }
        } else if (sync_terminal && prog.state != last_sync_state) {
            sync_hold = 30;   /* 完成/取消后再停留约 6 秒 */
            if (Lvgl_lock(100)) {
                sync_screen_update(&prog);
                Lvgl_unlock();
            }
        } else if (sync_hold > 0) {
            sync_hold--;
        } else if (Lvgl_lock(100)) {
            if (sync_screen_visible()) sync_screen_hide();
            Lvgl_unlock();
        }
        last_sync_state = prog.state;

        /* 每秒：提醒轮询 + 时间/状态刷新 */
        if (tick % 5 == 0) {
            reminder_service_tick(&dt);

            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
            char status[32];
            char ip[32] = {0};
            g_battery = sensor_battery_level();
            wifi_get_ip(ip, sizeof(ip));
            snprintf(status, sizeof(status), "%s | %u%%",
                     wifi_is_connected() ? "WiFi" : "AP", (unsigned)g_battery);

            if (Lvgl_lock(100)) {
                main_screen_update_time(NULL, time_str, NULL, NULL);
                main_screen_update_status(status);
                main_screen_update_net(wifi_is_connected(), ip);
                main_screen_set_reminder_enabled(g_cfg.remind_enabled);
                Lvgl_unlock();
            }
        }

        /* Calendar and date change only at midnight. */
        if (dt.day != last_day) {
            snprintf(date, sizeof(date), "%04d-%02d-%02d", dt.year, dt.month, dt.day);

            lunar_date_t ld;
            if (calendar_solar_to_lunar(dt.year, dt.month, dt.day, &ld) == 0) {
                calendar_lunar_str(&ld, lunar_full, sizeof(lunar_full));
            } else {
                strcpy(lunar_full, "");
            }

            prepare_calendar(dt.year, dt.month, dt.day, cells);

            if (Lvgl_lock(100)) {
                main_screen_update_time(date, NULL, week_cn(dt.weekday), lunar_full);
                main_screen_update_calendar(cells);
                last_day = dt.day;
                Lvgl_unlock();
            }
        }

        /* 每 60 秒：传感器 + 右侧摘要 */
        if (tick % 300 == 0 || g_summary_dirty) {
            g_summary_dirty = false;
            sensor_service_read(&g_temp, &g_humi);
            if (Lvgl_lock(100)) {
                char weather[64];
                if (g_weather.text[0])
                    snprintf(weather, sizeof(weather), "%.24s\n%dC / %d%%", g_weather.text, g_weather.temp, g_weather.humidity);
                else
                    snprintf(weather, sizeof(weather), "天气 --");
                const ai_provider_t *ai5h = NULL, *aiweek = NULL;
                for (int i = 0; i < g_ai_count; ++i) {
                    if (!strstr(g_ai[i].id, "chatgpt") && !strstr(g_ai[i].id, "codex")) continue;
                    if (g_ai[i].window_minutes == 300) ai5h = &g_ai[i];
                    if (g_ai[i].window_minutes == 10080) aiweek = &g_ai[i];
                }
                main_screen_update_summary(g_worktime.recorded_hours, g_worktime.expected_hours,
                    weather, lunar_full, ai5h, aiweek,
                    racebox_service_synced_today(), racebox_service_point_count(), g_temp, g_humi);
                Lvgl_unlock();
            }
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void button_task(void *arg) {
    while (1) {
        /* KEY（GPIO18）单击 → 触发 RaceBox 采集；同步弹窗可见时再按一次收起 */
        EventBits_t bits = xEventGroupWaitBits(GP18ButtonGroups, 0x07, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (bits & 0x04) {
            racebox_state_t state = racebox_service_state();
            if (state != RACEBOX_IDLE && state != RACEBOX_DONE && state != RACEBOX_FAILED) {
                ESP_LOGI(TAG, "KEY long press -> cancel racebox sync");
                racebox_service_cancel();
            }
        } else if (bits & 0x01) {
            if (main_screen_remind_visible()) {
                ESP_LOGI(TAG, "KEY click -> dismiss reminder");
                if (Lvgl_lock(100)) {
                    main_screen_hide_remind();
                    Lvgl_unlock();
                }
            } else if (racebox_service_state() != RACEBOX_IDLE &&
                       racebox_service_state() != RACEBOX_DONE &&
                       racebox_service_state() != RACEBOX_FAILED) {
                if (Lvgl_lock(100)) {
                    if (sync_screen_visible()) sync_screen_hide();
                    else sync_screen_show();
                    Lvgl_unlock();
                }
                ESP_LOGI(TAG, "KEY click -> toggle sync panel");
            } else {
                ESP_LOGI(TAG, "KEY click -> racebox trigger");
                racebox_service_trigger();
                if (Lvgl_lock(100)) {
                    sync_screen_show();
                    Lvgl_unlock();
                }
            }
        }
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "deskwong boot");
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "UI2 firmware=%s built=%s %s partition=%s address=0x%lx",
             app->version, app->date, app->time, running ? running->label : "unknown",
             running ? (unsigned long)running->address : 0UL);

    app_config_init();
    app_config_load(&g_cfg);

    Custom_ButtonInit();
    time_service_init(&I2cbus, g_cfg.timezone);
    sensor_service_init(&I2cbus);

    weather_service_init(g_cfg.weather_api_url, g_cfg.weather_location, g_cfg.weather_key);
    worktime_service_init(g_cfg.worktime_api_base, g_cfg.worktime_token);
    aiusage_service_init(g_cfg.aiusage_api_base, g_cfg.aiusage_token);
    reminder_service_init(on_remind);
    reminder_service_set_times(g_cfg.remind_signin_hh, g_cfg.remind_signin_mm,
                               g_cfg.remind_signout_hh, g_cfg.remind_signout_mm,
                               g_cfg.remind_worktime_hh, g_cfg.remind_worktime_mm);
    reminder_service_set_enabled(g_cfg.remind_enabled);

    app_mqtt_init(g_cfg.mqtt_broker, g_cfg.mqtt_port, g_cfg.mqtt_user, g_cfg.mqtt_pass, NULL);
    racebox_service_init(g_cfg.racebox_upload_topic, g_cfg.racebox_auto_erase,
                         g_cfg.racebox_device_name, g_cfg.racebox_device_lock);
    audio_service_init();

    RlcdPort.RLCD_Init();
    /* Push a known frame immediately so panel init is visible even before LVGL runs. */
    RlcdPort.RLCD_Display();
    Lvgl_PortInit(LCD_WIDTH, LCD_HEIGHT, Lvgl_FlushCallback);
    if (Lvgl_lock(-1)) {
        main_screen_init(LCD_WIDTH, LCD_HEIGHT);
        sync_screen_init(LCD_WIDTH, LCD_HEIGHT);
        Lvgl_unlock();
    }

    /* 周期拉取统一交给调度器：排队串行、错峰，绝不并发抢网络 */
    net_scheduler_init();
    net_scheduler_register(NET_JOB_WEATHER, job_weather, NULL,
                           refresh_period(g_cfg.weather_refresh_minutes, 300));
    net_scheduler_register(NET_JOB_WORKTIME, job_worktime, NULL,
                           refresh_period(g_cfg.worktime_refresh_minutes, 300));
    net_scheduler_register(NET_JOB_AIUSAGE, job_aiusage, NULL,
                           refresh_period(g_cfg.aiusage_refresh_minutes, 60));

    wifi_init(g_cfg.wifi_ssid, g_cfg.wifi_pass);
    http_server_start(&g_cfg);
    time_service_sync_ntp();

    BaseType_t ui_ok = xTaskCreatePinnedToCoreWithCaps(
        ui_update_task, "ui", 8 * 1024, NULL, 3, NULL, 1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    BaseType_t button_ok = xTaskCreatePinnedToCoreWithCaps(
        button_task, "btn", 4 * 1024, NULL, 3, NULL, 1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    /* 栈要给 TLS 握手 + gzip inflate(puff 约 2KB) 留足余量。 */
    BaseType_t network_ok = xTaskCreatePinnedToCoreWithCaps(
        network_service_task, "network_services", 24 * 1024, NULL, 2, NULL, 0,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ui_ok != pdPASS || button_ok != pdPASS || network_ok != pdPASS) {
        ESP_LOGE(TAG, "task creation failed: ui=%ld button=%ld network=%ld",
                 (long)ui_ok, (long)button_ok, (long)network_ok);
        abort();
    }

    ESP_LOGI(TAG, "boot done; internal free=%u PSRAM free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
