#include "lvgl.h"
#include "main_screen.h"
#include "ui_fonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static lv_obj_t *s_time, *s_year, *s_month, *s_total, *s_status, *s_race_check, *s_points;
static lv_obj_t *s_lunar_full, *s_weather, *s_temperature, *s_humidity, *s_sun, *s_cloud, *s_bell, *s_ai[2], *s_remind;
static lv_obj_t *s_cell[42], *s_day[42], *s_lunar[42], *s_track[42], *s_fill[42];
static lv_obj_t *s_summary_fill, *s_ai_fill[2];
/* 状态栏：离线时显示的后台地址胶囊、RaceBox 图标 */
static lv_obj_t *s_ip, *s_race_img;
/* 温度：数字 + 自绘的「°」圆圈 + 湿度 */
static lv_obj_t *s_degree_out, *s_indoor_temp, *s_degree_in, *s_indoor_humi;
static bool s_offline = false;
static volatile bool s_remind_visible = false;
static int s_grid_height;
static const uint8_t s_racebox_alpha[] = {
#include "racebox_logo_alpha.inc"
};
static const lv_img_dsc_t s_racebox_img = {
    .header = {.cf = LV_IMG_CF_ALPHA_1BIT, .always_zero = 0, .reserved = 0, .w = 32, .h = 24},
    .data_size = sizeof(s_racebox_alpha), .data = s_racebox_alpha
};
static const uint8_t s_chatgpt_alpha[] = {
#include "chatgpt_logo_alpha.inc"
};
static const lv_img_dsc_t s_chatgpt_img = {
    .header = {.cf = LV_IMG_CF_ALPHA_1BIT, .always_zero = 0, .reserved = 0, .w = 24, .h = 24},
    .data_size = sizeof(s_chatgpt_alpha), .data = s_chatgpt_alpha
};

static void text_changed(lv_obj_t *o, const char *v) {
    if (o && v && strcmp(lv_label_get_text(o), v)) lv_label_set_text(o, v);
}
static void set_lunar(const char *text) {
    if (!text || !*text) text = "--";
    uint32_t chars = _lv_txt_get_encoded_length(text);
    lv_obj_set_style_text_font(s_lunar_full, chars <= 4 ? &lv_font_zh_20 : chars <= 5 ? &lv_font_zh_14 : &lv_font_zh_10, 0);
    text_changed(s_lunar_full, text);
}
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, bool black = false, int radius = 0) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, black ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *label(lv_obj_t *p, const char *text, const lv_font_t *font, int x, int y, int w = 0) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_black(), 0);
    lv_label_set_text(o, text); lv_obj_set_pos(o, x, y);
    if (w) { lv_obj_set_width(o, w); lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP); }
    return o;
}
static void draw_dots(lv_event_t *e) {
    lv_obj_t *o = lv_event_get_target(e);
    lv_area_t a; lv_obj_get_coords(o, &a);
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_obj_get_style_text_color(o, 0);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    for (int x = a.x1; x <= a.x2; x += 3)
        for (int y = a.y1; y <= a.y2; y += 2) {
            lv_area_t dot = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)x, (lv_coord_t)y};
            lv_draw_rect(ctx, &d, &dot);
        }
}
static void dotted(lv_obj_t *p, int x, int y, int w) {
    lv_obj_t *o = box(p, x, y, w, 1);
    lv_obj_add_event_cb(o, draw_dots, LV_EVENT_DRAW_MAIN, NULL);
}
static lv_obj_t *meter(lv_obj_t *p, int x, int y, int w, int h, lv_obj_t **fill) {
    lv_obj_t *o = box(p, x, y, w, h);
    lv_obj_add_event_cb(o, draw_dots, LV_EVENT_DRAW_MAIN, NULL);
    *fill = box(o, 0, 0, 0, h, true, 1);
    return o;
}
static void set_meter(lv_obj_t *fill, float percent, int width) {
    if (!isfinite(percent)) percent = 0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int pixels = (int)(width * percent / 100.0f + 0.5f);
    lv_obj_set_width(fill, pixels);
    if (pixels) lv_obj_clear_flag(fill, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(fill, LV_OBJ_FLAG_HIDDEN);
}
static void update_quota(int index, const ai_provider_t *quota) {
    char text[32];
    if (quota && quota->resets_at > 0) {
        time_t reset = (time_t)quota->resets_at;
        struct tm local;
#ifdef _WIN32
        bool converted = localtime_s(&local, &reset) == 0;
#else
        bool converted = localtime_r(&reset, &local) != NULL;
#endif
        if (converted)
            strftime(text, sizeof(text), index == 0 ? "%H:%M" : "%m/%d\n%H:%M", &local);
        else strcpy(text, "--:--");
    } else strcpy(text, index == 0 ? "--:--" : "--/--\n--:--");
    text_changed(s_ai[index], text);
    set_meter(s_ai_fill[index], quota ? quota->remaining_percent : 0, 79);
}
/* 自绘的摄氏度「°」小圆圈：单色屏上比字符更清晰，且不依赖字库是否含 ° */
static lv_obj_t *degree_mark(lv_obj_t *p, int size, int border) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, size, size);
    lv_obj_set_style_border_width(o, border, 0);
    lv_obj_set_style_border_color(o, lv_color_black(), 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void draw_sun(lv_event_t *e) {
    lv_area_t a; lv_obj_get_coords(lv_event_get_target(e), &a);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t circle; lv_draw_rect_dsc_init(&circle);
    circle.bg_opa = LV_OPA_TRANSP; circle.border_width = 2;
    circle.border_color = lv_color_black(); circle.radius = LV_RADIUS_CIRCLE;
    lv_area_t ring = {(lv_coord_t)(a.x1+7), (lv_coord_t)(a.y1+7), (lv_coord_t)(a.x1+23), (lv_coord_t)(a.y1+23)};
    lv_draw_rect(ctx, &circle, &ring);
    static const int rays[8][4] = {{15,0,15,3},{15,27,15,30},{0,15,3,15},{27,15,30,15},
        {4,4,6,6},{24,24,26,26},{4,26,6,24},{24,6,26,4}};
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line); line.width=2; line.color=lv_color_black();
    for (const auto &r : rays) {
        lv_point_t p={(lv_coord_t)(a.x1+r[0]),(lv_coord_t)(a.y1+r[1])};
        lv_point_t q={(lv_coord_t)(a.x1+r[2]),(lv_coord_t)(a.y1+r[3])};
        lv_draw_line(ctx,&line,&p,&q);
    }
}

static void draw_cloud(lv_event_t *e) {
    lv_area_t a; lv_obj_get_coords(lv_event_get_target(e), &a);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    static const int outline[][2] = {
        {4,22},{3,20},{3,17},{5,14},{8,13},{10,9},{13,7},{17,7},
        {20,9},{21,12},{25,12},{28,15},{28,19},{26,22},{4,22}
    };
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    line.width = 2; line.color = lv_color_black(); line.round_start = 1; line.round_end = 1;
    for (size_t i = 1; i < sizeof(outline) / sizeof(outline[0]); ++i) {
        lv_point_t p = {(lv_coord_t)(a.x1 + outline[i-1][0]), (lv_coord_t)(a.y1 + outline[i-1][1])};
        lv_point_t q = {(lv_coord_t)(a.x1 + outline[i][0]), (lv_coord_t)(a.y1 + outline[i][1])};
        lv_draw_line(ctx, &line, &p, &q);
    }
}
void main_screen_init(int width, int height) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    s_grid_height = height - 88 - 4;
    s_status = label(scr, LV_SYMBOL_BATTERY_FULL " --%  " LV_SYMBOL_WIFI, &lv_font_montserrat_14, 7, 9, 90);
    s_bell = label(scr, LV_SYMBOL_BELL, &lv_font_montserrat_14, 98, 9);
    lv_obj_add_flag(s_bell, LV_OBJ_FLAG_HIDDEN);
    box(scr, 116, 7, 1, 18, true);
    lv_obj_t *race = lv_img_create(scr); lv_img_set_src(race, &s_racebox_img);
    lv_obj_set_style_img_recolor(race, lv_color_black(), 0);
    lv_obj_set_style_img_recolor_opa(race, LV_OPA_COVER, 0); lv_obj_set_pos(race, 125, 4);
    s_race_img = race;
    /* 离线（AP 模式）时显示后台地址胶囊，让用户知道连哪个 IP 去配网 */
    s_ip = label(scr, "", &lv_font_zh_10, 125, 5, 0);
    lv_obj_set_style_border_width(s_ip, 2, 0);
    lv_obj_set_style_border_color(s_ip, lv_color_black(), 0);
    lv_obj_set_style_border_opa(s_ip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_ip, 7, 0);
    lv_obj_set_style_pad_hor(s_ip, 6, 0);
    lv_obj_set_style_pad_ver(s_ip, 3, 0);
    lv_obj_add_flag(s_ip, LV_OBJ_FLAG_HIDDEN);
    s_race_check = label(scr, LV_SYMBOL_OK, &lv_font_montserrat_14, 152, 1);
    lv_obj_set_style_bg_color(s_race_check, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_race_check, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_race_check, lv_color_white(), 0);
    lv_obj_set_style_radius(s_race_check, 7, 0);
    lv_obj_add_flag(s_race_check, LV_OBJ_FLAG_HIDDEN);
    s_points = label(scr, LV_SYMBOL_GPS " 0", &lv_font_montserrat_14, 175, 7);
    lv_obj_set_style_border_width(s_points, 1, 0);
    lv_obj_set_style_border_color(s_points, lv_color_black(), 0);
    lv_obj_set_style_radius(s_points, 9, 0);
    lv_obj_set_style_pad_hor(s_points, 6, 0);
    lv_obj_set_style_pad_ver(s_points, 2, 0);
    box(scr, 278, 5, 1, 22, true);
    s_time = label(scr, "--:--:--", &lv_font_digits_28, 284, 1, 112);
    lv_obj_set_style_text_align(s_time, LV_TEXT_ALIGN_RIGHT, 0);
    box(scr, 6, 32, width-12, 2, true);

    s_year = label(scr, "----", &lv_font_digits_28, 8, 43);
    label(scr, "年", &lv_font_zh_14, 74, 53);
    s_month = label(scr, "--", &lv_font_digits_36, 92, 37, 42);
    lv_obj_set_style_text_align(s_month, LV_TEXT_ALIGN_CENTER, 0);
    label(scr, "月", &lv_font_zh_14, 136, 53);
    s_total = label(scr, "--/-- --%", &lv_font_digits_18, 163, 44, 136);
    lv_obj_set_style_text_align(s_total, LV_TEXT_ALIGN_RIGHT, 0);
    meter(scr, 180, 64, 119, 5, &s_summary_fill);
    const char *wh[] = {"一", "二", "三", "四", "五", "六", "日"};
    for (int i = 0; i < 7; ++i) {
        lv_obj_t *h = label(scr, wh[i], &lv_font_zh_14, 5+i*43, 73, 42);
        lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    }
    dotted(scr, 6, 89, 295);
    for (int i = 0; i < 42; ++i) {
        s_cell[i] = box(scr, 5+(i%7)*43, 92+(i/7)*34, 41, 33, false, 4);
        s_day[i] = label(s_cell[i], "", &lv_font_digits_18, 0, 0, 41);
        lv_obj_set_style_text_align(s_day[i], LV_TEXT_ALIGN_CENTER, 0);
        s_lunar[i] = label(s_cell[i], "", &lv_font_zh_10, 0, 20, 41);
        lv_obj_set_style_text_align(s_lunar[i], LV_TEXT_ALIGN_CENTER, 0);
        s_track[i] = meter(s_cell[i], 6, 31, 29, 3, &s_fill[i]);
        lv_obj_add_flag(s_cell[i], LV_OBJ_FLAG_HIDDEN);
    }
    box(scr, 307, 41, 1, height-47, true);
    lv_obj_t *tag = box(scr, 316, 42, 35, 18, true, 3);
    lv_obj_t *t = label(tag, "农历", &lv_font_zh_14, 3, 1);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    s_lunar_full = label(scr, "--", &lv_font_zh_20, 315, 66, 81);
    box(scr, 315, 94, 80, 1, true);
    s_sun = box(scr, 315, 102, 31, 31);
    lv_obj_add_event_cb(s_sun, draw_sun, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(s_sun, LV_OBJ_FLAG_HIDDEN);
    s_cloud = box(scr, 315, 102, 31, 31);
    lv_obj_add_event_cb(s_cloud, draw_cloud, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(s_cloud, LV_OBJ_FLAG_HIDDEN);
    s_weather = label(scr, "天气 --", &lv_font_zh_14, 315, 110, 80);
    s_temperature = label(scr, "--", &lv_font_digits_28, 315, 135);
    s_degree_out = degree_mark(scr, 10, 2);
    /* The ring is a superscript: keep it level with the top of the large digits. */
    lv_obj_set_pos(s_degree_out, 349, 134);
    s_humidity = label(scr, "", &lv_font_zh_10, 367, 146, 28);
    dotted(scr, 315, 162, 80);
    tag = box(scr, 316, 171, 35, 18, true, 3);
    t = label(tag, "室内", &lv_font_zh_14, 3, 1);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    /* 室内：温度 + 上标度数圆圈 + 湿度，圆圈不参与文字基线。 */
    s_indoor_temp = label(scr, "--", &lv_font_montserrat_14, 353, 172);
    s_degree_in = degree_mark(scr, 4, 1);
    lv_obj_set_pos(s_degree_in, 370, 169);
    s_indoor_humi = label(scr, "--%", &lv_font_zh_10, 375, 176);
    dotted(scr, 315, 201, 80);
    /* ChatGPT 图标 + 「额度」整体居中：24(图标)+6(间距)+28(文字)=58，居中于 80 宽的列 */
    lv_obj_t *chatgpt = lv_img_create(scr);
    lv_img_set_src(chatgpt, &s_chatgpt_img); lv_obj_set_pos(chatgpt, 326, 205);
    lv_obj_set_style_img_recolor(chatgpt, lv_color_black(), 0);
    lv_obj_set_style_img_recolor_opa(chatgpt, LV_OPA_COVER, 0);
    label(scr, "额度", &lv_font_zh_14, 356, 209);
    label(scr, "5小时", &lv_font_zh_10, 315, 235);
    s_ai[0] = label(scr, "--:--", &lv_font_zh_14, 350, 232, 45);
    meter(scr, 315, 252, 79, 5, &s_ai_fill[0]);
    label(scr, "每周", &lv_font_zh_10, 315, 270);
    s_ai[1] = label(scr, "--/--\n--:--", &lv_font_zh_10, 345, 264, 50);
    meter(scr, 315, 289, 79, 5, &s_ai_fill[1]);
    s_remind = label(scr, "", &lv_font_zh_14, 7, height-25, width-14);
    lv_obj_set_style_bg_color(s_remind, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_remind, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_remind, lv_color_white(), 0);
    lv_obj_set_style_pad_all(s_remind, 4, 0);
    lv_obj_add_flag(s_remind, LV_OBJ_FLAG_HIDDEN);
}
void main_screen_update_time(const char *date, const char *time, const char *week, const char *lunar) {
    (void)week;
    if (date && strlen(date) >= 7) {
        char b[16]; snprintf(b, sizeof(b), "%.4s", date); text_changed(s_year, b);
        snprintf(b, sizeof(b), "%d", atoi(date+5)); text_changed(s_month, b);
    }
    if (time) text_changed(s_time, time);
    if (lunar && *lunar) set_lunar(lunar);
}
void main_screen_update_calendar(const calendar_cell_t cells[42]) {
    int last = 0;
    for (int i = 0; i < 42; ++i) if (cells[i].day) last = i;
    int rows = last/7+1; if (rows < 5) rows = 5;
    int ch = (s_grid_height-4)/rows;
    for (int i = 0; i < 42; ++i) {
        const calendar_cell_t *c = &cells[i];
        if (!c->day) { lv_obj_add_flag(s_cell[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s_cell[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(s_cell[i], 92+(i/7)*ch); lv_obj_set_height(s_cell[i], ch-1);
        lv_obj_set_y(s_lunar[i], rows == 6 ? 18 : 20);
        lv_obj_set_y(s_track[i], ch-4);
        char b[8]; snprintf(b, sizeof(b), "%d", c->day); text_changed(s_day[i], b);
        text_changed(s_lunar[i], c->lunar);
        lv_color_t fg = c->is_today ? lv_color_white() : lv_color_black();
        lv_obj_set_style_bg_color(s_cell[i], c->is_today ? lv_color_black() : lv_color_white(), 0);
        lv_obj_set_style_text_color(s_day[i], fg, 0); lv_obj_set_style_text_color(s_lunar[i], fg, 0);
        lv_obj_set_style_bg_color(s_track[i], c->is_today ? lv_color_black() : lv_color_white(), 0);
        lv_obj_set_style_text_color(s_track[i], fg, 0);
        lv_obj_set_style_bg_color(s_fill[i], fg, 0);
        lv_obj_set_style_border_color(s_cell[i], lv_color_black(), 0);
        lv_obj_set_style_border_width(s_cell[i], !c->is_today && c->type == CAL_HOLIDAY ? 1 : 0, 0);
        lv_obj_set_style_text_decor(s_day[i], c->type == CAL_WEEKEND ? LV_TEXT_DECOR_UNDERLINE : LV_TEXT_DECOR_NONE, 0);
        set_meter(s_fill[i], c->work_hours/8.0f*100, 29);
        if (c->work_hours <= 0) lv_obj_add_flag(s_track[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_track[i], LV_OBJ_FLAG_HIDDEN);
    }
}
void main_screen_update_summary(float recorded, float expected, const char *weather, const char *lunar,
                                const ai_provider_t *ai5h, const ai_provider_t *aiweek, bool synced, int points,
                                float indoor_temp, float indoor_humidity) {
    char b[96];
    if (expected > 0) snprintf(b, sizeof(b), "%.0f/%.0f %.0f%%", recorded, expected, recorded/expected*100);
    else snprintf(b, sizeof(b), "%.0f/-- --%%", recorded);
    text_changed(s_total, b); set_meter(s_summary_fill, expected > 0 ? recorded/expected*100 : 0, 119);
    if (synced && !s_offline) lv_obj_clear_flag(s_race_check, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_race_check, LV_OBJ_FLAG_HIDDEN);
    if (points >= 1000000) snprintf(b, sizeof(b), LV_SYMBOL_GPS " %.1fM", points/1000000.0f);
    else if (points >= 1000) snprintf(b, sizeof(b), LV_SYMBOL_GPS " %.1fk", points/1000.0f);
    else snprintf(b, sizeof(b), LV_SYMBOL_GPS " %d", points < 0 ? 0 : points);
    text_changed(s_points, b);
    set_lunar(lunar);
    const char *newline = weather ? strchr(weather, '\n') : NULL;
    int temperature = 0, humidity = 0;
    bool valid_weather = newline && sscanf(newline+1, "%dC / %d%%", &temperature, &humidity) == 2;
    bool sunny = valid_weather && strncmp(weather, "晴", strlen("晴")) == 0;
    bool cloudy = valid_weather && (strstr(weather, "阴") != NULL || strstr(weather, "云") != NULL);
    if (sunny) lv_obj_clear_flag(s_sun, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_sun, LV_OBJ_FLAG_HIDDEN);
    if (cloudy) lv_obj_clear_flag(s_cloud, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_cloud, LV_OBJ_FLAG_HIDDEN);
    bool has_icon = sunny || cloudy;
    lv_obj_set_x(s_weather, has_icon ? 350 : 315); lv_obj_set_width(s_weather, has_icon ? 45 : 80);
    if (valid_weather) snprintf(b, sizeof(b), "%.*s", (int)(newline-weather), weather);
    else snprintf(b, sizeof(b), "%s", weather && *weather ? weather : "天气 --");
    text_changed(s_weather, b);
    if (valid_weather) snprintf(b, sizeof(b), "%d", temperature); else strcpy(b, "--");
    text_changed(s_temperature, b);
    lv_obj_update_layout(s_temperature);
    /* 度数圆圈紧跟数字，并固定在大号数字的右上角。 */
    lv_obj_set_x(s_degree_out, 315 + lv_obj_get_width(s_temperature) + 1);
    lv_obj_set_y(s_degree_out, 134);
    if (valid_weather) lv_obj_clear_flag(s_degree_out, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_degree_out, LV_OBJ_FLAG_HIDDEN);
    if (valid_weather) snprintf(b, sizeof(b), "湿%d%%", humidity); else b[0] = 0;
    text_changed(s_humidity, b);

    if (isfinite(indoor_temp)) snprintf(b, sizeof(b), "%.0f", indoor_temp);
    else strcpy(b, "--");
    text_changed(s_indoor_temp, b);
    lv_obj_update_layout(s_indoor_temp);
    /* 与“室内”标签的底边 y=189 对齐。 */
    lv_obj_set_y(s_indoor_temp, 189 - lv_obj_get_height(s_indoor_temp));
    /* 标签自适应真实数字宽度，圆圈紧贴最后一位数字。 */
    int degree_x = 353 + lv_obj_get_width(s_indoor_temp) - 1;
    lv_obj_set_x(s_degree_in, degree_x + 2);
    lv_obj_set_y(s_degree_in, 175);
    if (isfinite(indoor_temp)) lv_obj_clear_flag(s_degree_in, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_degree_in, LV_OBJ_FLAG_HIDDEN);
    if (isfinite(indoor_humidity)) snprintf(b, sizeof(b), "%.0f%%", indoor_humidity);
    else strcpy(b, "--%");
    text_changed(s_indoor_humi, b);
    lv_obj_update_layout(s_indoor_humi);
    lv_obj_set_y(s_indoor_humi, 189 - lv_obj_get_height(s_indoor_humi));
    /* 湿度紧跟在上标后面，不再贴着右侧边缘。 */
    lv_obj_set_x(s_indoor_humi, degree_x + 9);
    update_quota(0, ai5h);
    update_quota(1, aiweek);
}

void main_screen_update_net(bool connected, const char *ip) {
    s_offline = !connected;
    if (!connected) {
        /* 未联网：只用 ASCII，避免小字库缺少汉字时显示方框。 */
        char b[48];
        if (ip && ip[0]) snprintf(b, sizeof(b), "AP  %s", ip);
        else strcpy(b, "AP  192.168.4.1");
        text_changed(s_ip, b);
        lv_obj_clear_flag(s_ip, LV_OBJ_FLAG_HIDDEN);
        if (s_race_img) lv_obj_add_flag(s_race_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_points, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_race_check, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ip, LV_OBJ_FLAG_HIDDEN);
        if (s_race_img) lv_obj_clear_flag(s_race_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_points, LV_OBJ_FLAG_HIDDEN);
    }
}
void main_screen_update_bottom(const char *info) { (void)info; }
void main_screen_update_status(const char *status) {
    if (!status) return;
    const char *p = strchr(status, '|');
    int battery = p ? atoi(p+1) : 0;
    if (battery < 0) battery = 0;
    if (battery > 100) battery = 100;
    const char *icon = battery > 75 ? LV_SYMBOL_BATTERY_FULL : battery > 40 ? LV_SYMBOL_BATTERY_2 : battery > 10 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
    char b[64]; snprintf(b, sizeof(b), "%s %d%%  %s", icon, battery, strstr(status, "WiFi") ? LV_SYMBOL_WIFI : "AP");
    text_changed(s_status, b);
}
void main_screen_show_remind(const char *msg) {
    text_changed(s_remind, msg); lv_obj_clear_flag(s_remind, LV_OBJ_FLAG_HIDDEN);
    s_remind_visible = true;
}
void main_screen_hide_remind(void) {
    lv_obj_add_flag(s_remind, LV_OBJ_FLAG_HIDDEN);
    s_remind_visible = false;
}
bool main_screen_remind_visible(void) {
    return s_remind_visible;
}
void main_screen_set_reminder_enabled(bool enabled) {
    if (enabled) lv_obj_clear_flag(s_bell, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_bell, LV_OBJ_FLAG_HIDDEN);
}
