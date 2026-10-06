#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
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
#include "voice_service.h"
#include "voice_wake.h"
#include "voice_context_service.h"
#include "main_screen.h"
#include "sync_screen.h"
#include "weather_almanac_screen.h"
#include "voice_screen.h"

static const char *TAG = "main";

/* 语音事件只保存最新快照；UI 任务拿到 LVGL 锁后重放，避免一次锁失败就丢弹窗。 */
static portMUX_TYPE g_voice_ui_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t g_voice_ui_seq = 0;
static voice_state_t g_voice_ui_state = VOICE_IDLE;
static char g_voice_ui_status[20] = "";
static char g_voice_ui_question[160] = "...";
static char g_voice_ui_answer[256] = "...";
static TickType_t g_voice_ui_error_until = 0;

static void on_voice_event(const voice_event_t *ev, void *ctx) {
    (void)ctx;
    ESP_LOGI(TAG, "voice: state=%d text=%s emotion=%s", (int)ev->state,
             ev->text ? ev->text : "-", ev->emotion ? ev->emotion : "-");
    const char *vtag = ev->state == VOICE_CONNECTING ? "正在唤醒"
                     : ev->state == VOICE_RECONNECTING ? "服务重连中"
                     : ev->state == VOICE_LISTENING  ? "聆听中"
                     : ev->state == VOICE_SPEAKING   ? "播放中"
                                                     : NULL;
    taskENTER_CRITICAL(&g_voice_ui_mux);
    g_voice_ui_state = ev->state;
    if (vtag) snprintf(g_voice_ui_status, sizeof(g_voice_ui_status), "%s", vtag);
    if (ev->state == VOICE_LISTENING && ev->text)
        snprintf(g_voice_ui_question, sizeof(g_voice_ui_question), "%s", ev->text);
    if (ev->state == VOICE_SPEAKING && ev->text)
        snprintf(g_voice_ui_answer, sizeof(g_voice_ui_answer), "%s", ev->text);
    if (ev->state == VOICE_IDLE) {
        const char *err = voice_service_last_error();
        if (err && err[0]) {
            snprintf(g_voice_ui_status, sizeof(g_voice_ui_status), "服务不可用");
            g_voice_ui_error_until = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
        } else {
            g_voice_ui_error_until = 0;
        }
    }
    g_voice_ui_seq++;
    taskEXIT_CRITICAL(&g_voice_ui_mux);
}

/* 原生横屏 400×300 单色反射屏 */
static DisplayPort RlcdPort(RLCD_MOSI_PIN, RLCD_SCK_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN, LCD_WIDTH, LCD_HEIGHT);
static I2cMasterBus I2cbus(ESP32_I2C_SCL_PIN, ESP32_I2C_SDA_PIN, 0);

static app_config_t g_cfg;

/* 外部服务数据缓存 */
static weather_now_t g_weather;
static weather_detail_t g_weather_detail;
static worktime_summary_t g_worktime;
static ai_provider_t g_ai[AI_MAX_PROVIDERS];
static int g_ai_count = 0;
static float g_temp = 0, g_humi = 0;
static uint8_t g_battery = 0;
static volatile bool g_summary_dirty = false;
/* 跨月或时间跳变后需要立刻重算工时（含应收工时） */
static volatile bool g_worktime_stale = false;
static volatile bool g_calendar_mode = false;
static volatile bool g_calendar_dirty = false;
static volatile int g_calendar_year = 0, g_calendar_month = 0;
static volatile TickType_t g_calendar_last_action = 0;
static volatile int g_worktime_year = 0, g_worktime_month = 0;
/* 天气黄历弹窗：按下刷新数据并重置计时，超时自动收起回主屏（秒数走配置） */
static volatile TickType_t g_almanac_shown_tick = 0;

static void Lvgl_FlushCallback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    static bool first_flush = true;
    static int dirty_x1 = LCD_WIDTH;
    static int dirty_x2 = -1;
    static int dirty_y1 = LCD_HEIGHT;
    static int dirty_y2 = -1;
    if (area->x1 < dirty_x1) dirty_x1 = area->x1;
    if (area->x2 > dirty_x2) dirty_x2 = area->x2;
    if (area->y1 < dirty_y1) dirty_y1 = area->y1;
    if (area->y2 > dirty_y2) dirty_y2 = area->y2;
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
        /* 弹窗开合、整版日历这类大面积变化直接整屏传输：面板按列对局部窗口
         * 写入，窗口两侧边界上会残留上一帧的内容（黄历弹窗隐藏后左缘留边）。
         * 小面积（时钟跳动等）仍走列窗口，省下整屏传输的时间。 */
        bool large_area = (dirty_x2 - dirty_x1 + 1) * (dirty_y2 - dirty_y1 + 1) >=
                          LCD_WIDTH * LCD_HEIGHT / 4;
        if (first_flush || large_area || (dirty_x1 <= 0 && dirty_x2 >= LCD_WIDTH - 1)) {
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
        dirty_y1 = LCD_HEIGHT;
        dirty_y2 = -1;
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
    datetime_t now; time_service_now(&now);
    /* g_weather 是同批实况：小时预报首条若跳过了当前小时，用它补位 */
    if (weather_service_fetch_detail(&g_weather_detail, &g_weather,
                                     now.year, now.month, now.day, now.hour) == 0 && Lvgl_lock(100)) {
        weather_almanac_screen_update(&g_weather_detail);
        Lvgl_unlock();
    }
    net_scheduler_request(NET_JOB_VOICE_CONTEXT);
}

static void job_worktime(void *ctx) {
    datetime_t now;
    time_service_now(&now);
    int year = g_worktime_year ? g_worktime_year : now.year;
    int month = g_worktime_month ? g_worktime_month : now.month;
    if (worktime_service_fetch(year, month, &g_worktime) != 0) {
        /* 拉取失败：保留缓存/本地应收工时，屏幕不出现 -- */
        ESP_LOGW(TAG, "worktime refresh failed, keep %04d-%02d %.1f/%.1f",
                 g_worktime.year, g_worktime.month, g_worktime.recorded_hours, g_worktime.expected_hours);
        if (g_worktime.year != year || g_worktime.month != month)
            worktime_service_load(year, month, &g_worktime);
    }
    g_summary_dirty = true;
    g_calendar_dirty = true;
    net_scheduler_request(NET_JOB_VOICE_CONTEXT);
}

static void job_aiusage(void *ctx) {
    aiusage_service_fetch(g_ai, AI_MAX_PROVIDERS, &g_ai_count);
    g_summary_dirty = true;
    net_scheduler_request(NET_JOB_VOICE_CONTEXT);
}

static void job_voice_context(void *ctx) {
    (void)ctx;
    datetime_t now;
    time_service_now(&now);
    voice_context_snapshot_t snap = {};
    snap.year = now.year;
    snap.month = now.month;
    snap.day = now.day;
    snap.worktime = g_worktime;
    snap.ai = g_ai;
    snap.ai_count = g_ai_count;
    snap.weather = g_weather;
    snap.weather_detail = g_weather_detail;
    snap.racebox_points = racebox_service_point_count();
    snap.racebox_complete = racebox_service_synced_today();
    voice_context_service_report(&snap);
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
        /* 跨月/校时后工时汇总作废 → 同样插队重算，避免一直显示上个月的数 */
        if (g_worktime_stale) {
            g_worktime_stale = false;
            net_scheduler_request(NET_JOB_WORKTIME);
        }
        net_scheduler_tick(5);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* 同步终态弹窗停留时长：40 * 200ms = 8 秒，之后自动收起，
 * 否则失败/无数据这类需要看清的提示会一直挡着主屏。 */
#define SYNC_TERMINAL_HOLD_TICKS 40

static void ui_update_task(void *arg) {
    char date[16], time_str[12], lunar_full[32] = "";
    calendar_cell_t cells[42];
    uint32_t tick = 0;
    uint32_t terminal_tick = 0;
    uint32_t voice_ui_applied_seq = UINT32_MAX;
    int last_day = -1;
    racebox_state_t last_sync_state = RACEBOX_IDLE;
    while (1) {
        datetime_t dt;
        time_service_now(&dt);

        uint32_t voice_seq;
        voice_state_t voice_state;
        TickType_t voice_error_until;
        char voice_status[20], voice_q[160], voice_a[256];
        taskENTER_CRITICAL(&g_voice_ui_mux);
        voice_seq = g_voice_ui_seq;
        voice_state = g_voice_ui_state;
        voice_error_until = g_voice_ui_error_until;
        snprintf(voice_status, sizeof(voice_status), "%s", g_voice_ui_status);
        snprintf(voice_q, sizeof(voice_q), "%s", g_voice_ui_question);
        snprintf(voice_a, sizeof(voice_a), "%s", g_voice_ui_answer);
        taskEXIT_CRITICAL(&g_voice_ui_mux);
        bool error_hold = voice_state == VOICE_IDLE && voice_error_until != 0 &&
                          xTaskGetTickCount() < voice_error_until;
        bool error_expired = voice_state == VOICE_IDLE && voice_error_until != 0 && !error_hold;
        if (voice_seq != voice_ui_applied_seq || error_expired) {
            if (Lvgl_lock(50)) {
                if (voice_state != VOICE_IDLE || error_hold) {
                    if (!voice_screen_visible()) voice_screen_show();
                    voice_screen_set_status(voice_status);
                    voice_screen_set_question(voice_q);
                    voice_screen_set_answer(voice_a);
                } else {
                    voice_screen_hide();
                }
                Lvgl_unlock();
                voice_ui_applied_seq = voice_seq;
                if (error_expired) {
                    taskENTER_CRITICAL(&g_voice_ui_mux);
                    if (g_voice_ui_error_until == voice_error_until) g_voice_ui_error_until = 0;
                    taskEXIT_CRITICAL(&g_voice_ui_mux);
                }
            }
        }

        /* RaceBox 同步弹窗：200ms 轮询，进度条才跟得上 */
        racebox_progress_t prog;
        racebox_service_progress(&prog);

        /* 同步各阶段提示音：服务层只打点，声音放到这里播，
         * 避免在 NimBLE 回调里阻塞通知消费。 */
        racebox_sound_t snd = racebox_service_take_sound();
        if (snd != RB_SND_NONE) {
            switch (snd) {
            case RB_SND_START:          audio_service_cue(AUDIO_CUE_START); break;
            case RB_SND_DEVICE_FOUND:   audio_service_cue(AUDIO_CUE_DEVICE_FOUND); break;
            case RB_SND_DOWNLOAD_DONE:  audio_service_cue(AUDIO_CUE_DOWNLOAD_DONE); break;
            case RB_SND_UPLOAD_DONE:    audio_service_cue(AUDIO_CUE_UPLOAD_DONE); break;
            case RB_SND_ERROR_DOWNLOAD:
            case RB_SND_ERROR_UPLOAD:   audio_service_cue(AUDIO_CUE_ERROR); break;
            case RB_SND_NO_DATA:        audio_service_cue(AUDIO_CUE_NO_DATA); break;
            case RB_SND_CANCEL:         audio_service_cue(AUDIO_CUE_CANCEL); break;
            default: break;
            }
        }
        bool sync_busy = prog.state != RACEBOX_IDLE &&
                         prog.state != RACEBOX_DONE && prog.state != RACEBOX_FAILED;
        bool sync_terminal = prog.state == RACEBOX_DONE || prog.state == RACEBOX_FAILED;
        if (sync_busy) {
            if (Lvgl_lock(100)) {
                sync_screen_update(&prog);
                Lvgl_unlock();
            }
            terminal_tick = 0;
        } else if (sync_terminal) {
            if (prog.state != last_sync_state) {
                g_summary_dirty = true;
                net_scheduler_request(NET_JOB_VOICE_CONTEXT);
                if (Lvgl_lock(100)) {
                    sync_screen_update(&prog);
                    /* 成功立即收起；失败/无数据要停留让人看清，超时后自动收起 */
                    if (prog.state == RACEBOX_DONE) sync_screen_hide();
                    else sync_screen_show();
                    Lvgl_unlock();
                }
                terminal_tick = tick;
            } else if (terminal_tick && tick - terminal_tick > SYNC_TERMINAL_HOLD_TICKS) {
                if (Lvgl_lock(100)) {
                    sync_screen_hide();
                    Lvgl_unlock();
                }
                terminal_tick = 0;
            }
        } else {
            terminal_tick = 0;
        }
        last_sync_state = prog.state;

        /* 天气黄历弹窗超时自动收起，回到主屏；提示音告知已返回 */
        if (weather_almanac_screen_visible() &&
            xTaskGetTickCount() - g_almanac_shown_tick >
                pdMS_TO_TICKS((uint32_t)g_cfg.almanac_return_seconds * 1000U)) {
            if (Lvgl_lock(100)) {
                weather_almanac_screen_hide();
                Lvgl_unlock();
            }
            audio_service_cue(AUDIO_CUE_CANCEL);
        }

        if (g_calendar_mode && xTaskGetTickCount() - g_calendar_last_action >
                pdMS_TO_TICKS((uint32_t)g_cfg.calendar_return_seconds * 1000U)) {
            g_calendar_mode = false;
            g_worktime_year = dt.year; g_worktime_month = dt.month;
            worktime_service_load(dt.year, dt.month, &g_worktime);
            net_scheduler_request(NET_JOB_WORKTIME);
            g_calendar_dirty = true;
            audio_service_cue(AUDIO_CUE_CANCEL);
            ESP_LOGI(TAG, "calendar browse timeout -> main month");
        }

        if (g_calendar_dirty) {
            g_calendar_dirty = false;
            int cy = g_calendar_mode ? g_calendar_year : dt.year;
            int cm = g_calendar_mode ? g_calendar_month : dt.month;
            int selected_today = (cy == dt.year && cm == dt.month) ? dt.day : 0;
            prepare_calendar(cy, cm, selected_today, cells);
            snprintf(date, sizeof(date), "%04d-%02d-%02d", cy, cm, selected_today ? selected_today : 1);
            if (Lvgl_lock(100)) {
                main_screen_set_calendar_mode(g_calendar_mode);
                main_screen_update_time(date, NULL, NULL, NULL);
                main_screen_update_calendar(cells);
                Lvgl_unlock();
            }
        }

        /* 每秒：提醒轮询 + 时间/状态刷新 */
        if (tick % 5 == 0) {
            racebox_service_day_tick(dt.year, dt.month, dt.day);
            static int last_points = -1;
            static bool last_synced = false;
            int points = racebox_service_point_count();
            bool synced = racebox_service_synced_today();
            if (points != last_points || synced != last_synced) g_summary_dirty = true;
            last_points = points; last_synced = synced;
            reminder_service_tick(&dt);

            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
            char status[32];
            char ip[32] = {0};
            g_battery = sensor_battery_level();
            wifi_get_ip(ip, sizeof(ip));
            /* WiFi=已联网；AP=配网热点开着；OFF=两者皆无（避免把"没连上"误读成配网模式）。
             * 语音会话进行中在状态栏追加状态词（轮询周期 1s，足够当作屏显反馈）。 */
            const char *net = wifi_is_connected() ? "WiFi" : (wifi_is_ap_mode() ? "AP" : "OFF");
            voice_state_t vs = voice_service_state();
            const char *vtag = vs == VOICE_CONNECTING ? "正在唤醒"
                             : vs == VOICE_RECONNECTING ? "服务重连中"
                             : vs == VOICE_LISTENING  ? "聆听中"
                             : vs == VOICE_SPEAKING   ? "播放中"
                                                      : NULL;
            if (vtag) snprintf(status, sizeof(status), "%s | %u%% | %s", net, (unsigned)g_battery, vtag);
            else snprintf(status, sizeof(status), "%s | %u%%", net, (unsigned)g_battery);

            if (Lvgl_lock(100)) {
                main_screen_update_time(NULL, time_str, NULL, NULL);
                main_screen_update_status(status);
                main_screen_update_net(wifi_is_connected(), ip);
                main_screen_set_reminder_enabled(g_cfg.remind_enabled);
                Lvgl_unlock();
            }
        }

        /* Calendar and date change only at midnight. */
        if (dt.day != last_day && !g_calendar_mode) {
            snprintf(date, sizeof(date), "%04d-%02d-%02d", dt.year, dt.month, dt.day);

            lunar_date_t ld;
            if (calendar_solar_to_lunar(dt.year, dt.month, dt.day, &ld) == 0) {
                calendar_lunar_str(&ld, lunar_full, sizeof(lunar_full));
            } else {
                strcpy(lunar_full, "");
            }

            int cy = g_calendar_mode ? g_calendar_year : dt.year;
            int cm = g_calendar_mode ? g_calendar_month : dt.month;
            prepare_calendar(cy, cm, (cy == dt.year && cm == dt.month) ? dt.day : 0, cells);
            /* 跨月或 NTP 校时换了月份：工时汇总要按新月份重算 */
            if (g_worktime.year != dt.year || g_worktime.month != dt.month) g_worktime_stale = true;

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
            /* 工时拉取会改变每日进度条数据：与右上角汇总一起重画日历，
             * 否则新工时只更新总数，格子进度条要等到跨天才刷新。 */
            int cy = g_calendar_mode ? g_calendar_year : dt.year;
            int cm = g_calendar_mode ? g_calendar_month : dt.month;
            prepare_calendar(cy, cm, (cy == dt.year && cm == dt.month) ? dt.day : 0, cells);
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
                    weather, g_weather.text[0] ? g_weather.icon : 0, lunar_full, ai5h, aiweek,
                    racebox_service_synced_today(), racebox_service_point_count(), g_temp, g_humi);
                main_screen_update_calendar(cells);
                Lvgl_unlock();
            }
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* BOOT 长按确认：multi_button 按住 1 秒发 LONG_PRESS_START，
 * 这里再要求继续按满剩余时长才生效，防止"按住 BOOT 进烧录模式后松手"
 * 或无意短长按误触发配网重启。 */
static bool boot_still_held_ms(int ms) {
    int checks = ms / 50;
    for (int i = 0; i < checks; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!button_boot_pressed()) return false;
    }
    return button_boot_pressed();
}

/* 注意：硬件按键断电是瞬间掉电，固件来不及在断电后播关机音；
 * 之前"提前轮询按键播音"的做法会在按 KEY 触发 RaceBox 时误响，已移除。
 * 关机音只用于软件重启路径（/api/system/restart）。 */
static void button_task(void *arg) {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(GP18ButtonGroups, 0x07, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
        if (bits & 0x04) {
            racebox_state_t state = racebox_service_state();
            if (state != RACEBOX_IDLE && state != RACEBOX_DONE && state != RACEBOX_FAILED) {
                ESP_LOGI(TAG, "KEY long press -> cancel racebox sync");
                racebox_service_cancel();
            } else {
                datetime_t now; time_service_now(&now);
                g_calendar_mode = !g_calendar_mode;
                g_calendar_year = now.year; g_calendar_month = now.month;
                g_worktime_year = now.year; g_worktime_month = now.month;
                worktime_service_load(now.year, now.month, &g_worktime);
                g_calendar_last_action = xTaskGetTickCount(); g_calendar_dirty = true; g_summary_dirty = true;
                if (Lvgl_lock(100)) { weather_almanac_screen_hide(); sync_screen_hide(); Lvgl_unlock(); }
                net_scheduler_request(NET_JOB_WORKTIME);
                ESP_LOGI(TAG, "KEY long press -> calendar browse %s", g_calendar_mode ? "on" : "off");
                audio_service_cue(AUDIO_CUE_KEY_TOGGLE);
            }
        } else if (bits & 0x01) {
            if (g_calendar_mode) {
                int month = g_calendar_month - 1, year = g_calendar_year;
                if (month < 1) { month = 12; year--; }
                g_calendar_month = month; g_calendar_year = year;
                g_worktime_year = g_calendar_year; g_worktime_month = g_calendar_month;
                worktime_service_load(g_calendar_year, g_calendar_month, &g_worktime);
                g_calendar_last_action = xTaskGetTickCount(); g_calendar_dirty = true; g_summary_dirty = true;
                net_scheduler_request(NET_JOB_WORKTIME); audio_service_cue(AUDIO_CUE_KEY);
            } else if (main_screen_remind_visible()) {
                ESP_LOGI(TAG, "KEY click -> dismiss reminder");
                if (Lvgl_lock(100)) {
                    main_screen_hide_remind();
                    Lvgl_unlock();
                }
                audio_service_cue(AUDIO_CUE_KEY);
            } else if (racebox_service_state() != RACEBOX_IDLE &&
                       racebox_service_state() != RACEBOX_DONE &&
                       racebox_service_state() != RACEBOX_FAILED) {
                if (Lvgl_lock(100)) {
                    if (sync_screen_visible()) sync_screen_hide();
                    else sync_screen_show();
                    Lvgl_unlock();
                }
                ESP_LOGI(TAG, "KEY click -> toggle sync panel");
                /* 同步中短按只是收起/展开弹窗，与"强制结束"区分开 */
                audio_service_cue(AUDIO_CUE_KEY_TOGGLE);
            } else if (sync_screen_visible()) {
                if (Lvgl_lock(100)) { sync_screen_hide(); Lvgl_unlock(); }
                ESP_LOGI(TAG, "KEY click -> dismiss sync summary");
                audio_service_cue(AUDIO_CUE_KEY);
            } else {
                ESP_LOGI(TAG, "KEY click -> racebox trigger");
                racebox_service_trigger();
                if (Lvgl_lock(100)) {
                    weather_almanac_screen_hide();
                    sync_screen_show();
                    Lvgl_unlock();
                }
            }
        }

        /* BOOT（GPIO0）：短按=天气黄历弹窗；按住 ≥3 秒=不重启，直接叠加配网
         * 热点（APSTA 共存，STA 照常连接）。之前"写标志+重启"的方案会在松手前
         * 把芯片带进下载模式（GPIO0 低电平复位），已废弃。
         * 语音对话只由唤醒词（或网页/HTTP 调试端点）触发，不占用按键。 */
        EventBits_t boot_bits = xEventGroupWaitBits(BootButtonGroups, 0x07, pdTRUE, pdFALSE, 0);
        if (boot_bits & 0x04) {
            if (boot_still_held_ms(2000)) {
                ESP_LOGW(TAG, "BOOT held ~3s -> start setup AP (no restart)");
                audio_service_cue(AUDIO_CUE_CANCEL);
                wifi_start_ap();
            } else {
                ESP_LOGI(TAG, "BOOT long press cancelled (released early)");
            }
        } else if (boot_bits & 0x01) {
            if (g_calendar_mode) {
                int month = g_calendar_month + 1, year = g_calendar_year;
                if (month > 12) { month = 1; year++; }
                g_calendar_month = month; g_calendar_year = year;
                g_worktime_year = g_calendar_year; g_worktime_month = g_calendar_month;
                worktime_service_load(g_calendar_year, g_calendar_month, &g_worktime);
                g_calendar_last_action = xTaskGetTickCount(); g_calendar_dirty = true; g_summary_dirty = true;
                net_scheduler_request(NET_JOB_WORKTIME);
            } else if (racebox_service_state() != RACEBOX_IDLE &&
                       racebox_service_state() != RACEBOX_DONE &&
                       racebox_service_state() != RACEBOX_FAILED) {
                ESP_LOGI(TAG, "BOOT click ignored while RaceBox sync is active");
            } else if (Lvgl_lock(100)) {
                /* 天气黄历弹窗：按一次显示/刷新（重置自动收起计时），再按一次隐藏 */
                if (weather_almanac_screen_visible()) {
                    weather_almanac_screen_hide();
                } else {
                    sync_screen_hide();
                    weather_almanac_screen_update(&g_weather_detail);
                    weather_almanac_screen_show();
                    net_scheduler_request(NET_JOB_WEATHER);
                    g_almanac_shown_tick = xTaskGetTickCount();
                }
                Lvgl_unlock();
            }
            audio_service_cue(AUDIO_CUE_KEY);
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

    weather_service_init(g_cfg.weather_api_url, g_cfg.weather_location, g_cfg.weather_key,
                         g_cfg.almanac_api_url, g_cfg.almanac_key);
    worktime_service_init(g_cfg.worktime_api_base, g_cfg.worktime_token);
    aiusage_service_init(g_cfg.aiusage_api_base, g_cfg.aiusage_token);
    /* 先用上次成功的月度汇总（没有就是 0 记录 + 本地应收工时）点亮屏幕，
     * 网络任务随后覆盖；这样重启或离线时也不会一直显示 -- */
    {
        datetime_t boot_time;
        time_service_now(&boot_time);
        worktime_service_load(boot_time.year, boot_time.month, &g_worktime);
        g_summary_dirty = true;
    }
    reminder_service_init(on_remind);
    reminder_service_set_times(g_cfg.remind_signin_hh, g_cfg.remind_signin_mm,
                               g_cfg.remind_signout_hh, g_cfg.remind_signout_mm,
                               g_cfg.remind_worktime_hh, g_cfg.remind_worktime_mm);
    reminder_service_set_enabled(g_cfg.remind_enabled);

    racebox_service_init(g_cfg.racebox_upload_topic, g_cfg.racebox_auto_erase,
                         g_cfg.racebox_device_name, g_cfg.racebox_device_lock);
    audio_service_init();
    /* 语音（小智）只初始化，不在这里联网：此时 WiFi 还没起来。
     * 对话由唤醒词、BOOT 短按或 POST /api/voice/start 触发。 */
    voice_service_init(&g_cfg);
    voice_service_set_event_cb(on_voice_event, NULL);
    const char *context_base = g_cfg.worktime_api_base[0] ? g_cfg.worktime_api_base
                                                          : g_cfg.aiusage_api_base;
    const char *context_token = g_cfg.worktime_token[0] ? g_cfg.worktime_token
                                                        : g_cfg.aiusage_token;
    voice_context_service_init(context_base, context_token, voice_service_device_id());
    /* 唤醒监听：任务常驻，门控（voice_enabled/listen_mode/WiFi/会话态）
     * 未满足时自动停驻，WiFi 连上且开启语音后即进入实时监听。 */
    voice_wake_init(&g_cfg);

    RlcdPort.RLCD_Init();
    /* Push a known frame immediately so panel init is visible even before LVGL runs. */
    RlcdPort.RLCD_Display();
    Lvgl_PortInit(LCD_WIDTH, LCD_HEIGHT, Lvgl_FlushCallback);
    if (Lvgl_lock(-1)) {
        main_screen_init(LCD_WIDTH, LCD_HEIGHT);
        sync_screen_init(LCD_WIDTH, LCD_HEIGHT);
        weather_almanac_screen_init(LCD_WIDTH, LCD_HEIGHT);
        voice_screen_init(LCD_WIDTH, LCD_HEIGHT);
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
    net_scheduler_register(NET_JOB_VOICE_CONTEXT, job_voice_context, NULL, 300);

    /* 开机只起 STA（无凭证时自动进纯 AP 配网）；热点由长按 BOOT 运行中开启 */
    wifi_init_ex(g_cfg.wifi_ssid, g_cfg.wifi_pass, false);
    /* esp-mqtt resolves the broker immediately. Initialize it only after
     * wifi_init has created the TCP/IP mailbox, even if WiFi is still joining. */
    app_mqtt_init(g_cfg.mqtt_broker, g_cfg.mqtt_port, g_cfg.mqtt_user, g_cfg.mqtt_pass, NULL);
    http_server_start(&g_cfg);
    time_service_sync_ntp();

    BaseType_t ui_ok = xTaskCreatePinnedToCoreWithCaps(
        ui_update_task, "ui", 8 * 1024, NULL, 3, NULL, 1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    /* 按键路径会读 NVS（worktime 缓存）：关 flash 缓存期间 PSRAM 不可访问，
     * PSRAM 栈的任务一执行 NVS/flash 就会触发 cache_utils 的
     * esp_task_stack_is_sane_cache_disabled 断言复位——栈必须放内部 RAM
     * （同 racebox_day worker 的处理）。 */
    BaseType_t button_ok = xTaskCreatePinnedToCoreWithCaps(
        button_task, "btn", 6 * 1024, NULL, 3, NULL, 1,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    /* 栈要给 TLS 握手 + gzip inflate(puff 约 2KB) 留足余量。该任务不做
     * flash/NVS 写入，放到 PSRAM 可释放 24KB 内部 RAM 给 WiFi、BLE 和 SPI DMA。 */
    BaseType_t network_ok = xTaskCreatePinnedToCoreWithCaps(
        network_service_task, "network_services", 24 * 1024, NULL, 2, NULL, 0,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ui_ok != pdPASS || button_ok != pdPASS || network_ok != pdPASS) {
        ESP_LOGE(TAG, "task creation failed: ui=%ld button=%ld network=%ld",
                 (long)ui_ok, (long)button_ok, (long)network_ok);
        abort();
    }


    ESP_LOGI(TAG, "boot done; internal free=%u PSRAM free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
