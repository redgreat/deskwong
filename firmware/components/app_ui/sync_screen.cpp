#include "lvgl.h"
#include "sync_screen.h"
#include "ui_fonts.h"
#include <stdio.h>
#include <string.h>

/* 400x300 单色反射屏：白底黑字，弹窗覆盖在主屏之上 */
static lv_obj_t *s_panel = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_device = NULL;
static lv_obj_t *s_state = NULL;
static lv_obj_t *s_count = NULL;
static lv_obj_t *s_hint = NULL;
static lv_obj_t *s_bar = NULL;
static lv_obj_t *s_bar_fill = NULL;
static int s_bar_width = 0;
static bool s_visible = false;

static lv_obj_t *sbox(lv_obj_t *p, int x, int y, int w, int h, bool black, int radius) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, black ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *slabel(lv_obj_t *p, const char *text, const lv_font_t *font, int x, int y, int w) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_black(), 0);
    lv_label_set_text(o, text);
    lv_obj_set_pos(o, x, y);
    if (w) {
        lv_obj_set_width(o, w);
        lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    }
    return o;
}

static void draw_border(lv_event_t *e) {
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target(e), &a);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_width = 2;
    d.border_color = lv_color_black();
    d.border_opa = LV_OPA_COVER;
    lv_draw_rect(ctx, &d, &a);
}

static void text_set(lv_obj_t *o, const char *v) {
    if (o && v && strcmp(lv_label_get_text(o), v)) lv_label_set_text(o, v);
}

extern "C" void sync_screen_init(int width, int height) {
    int panel_w = width - 40;
    int panel_h = 150;
    int panel_x = (width - panel_w) / 2;
    int panel_y = (height - panel_h) / 2;

    s_panel = sbox(lv_scr_act(), panel_x, panel_y, panel_w, panel_h, false, 4);
    lv_obj_add_event_cb(s_panel, draw_border, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    /* 标题条：黑底白字 */
    lv_obj_t *bar = sbox(s_panel, 0, 0, panel_w, 30, true, 0);
    s_title = lv_label_create(bar);
    lv_obj_remove_style_all(s_title);
    lv_obj_set_style_text_font(s_title, &lv_font_zh_20, 0);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_label_set_text(s_title, "RaceBox 数据同步");
    lv_obj_set_pos(s_title, 12, 6);

    s_device = slabel(s_panel, "设备 --", &lv_font_zh_14, 12, 40, panel_w - 24);
    s_state = slabel(s_panel, "准备中...", &lv_font_zh_20, 12, 62, panel_w - 24);

    s_bar_width = panel_w - 24;
    s_bar = sbox(s_panel, 12, 92, s_bar_width, 12, false, 0);
    lv_obj_add_event_cb(s_bar, draw_border, LV_EVENT_DRAW_MAIN, NULL);
    s_bar_fill = sbox(s_bar, 1, 1, 0, 10, true, 0);
    lv_obj_add_flag(s_bar_fill, LV_OBJ_FLAG_HIDDEN);

    /* 计数后面会带“条”，必须使用包含中文字形的字体。 */
    s_count = slabel(s_panel, "0 / 0", &lv_font_zh_20, 12, 108, s_bar_width);
    s_hint = slabel(s_panel, "再次短按 KEY 返回主屏", &lv_font_zh_10, 12, 130, panel_w - 24);
}

extern "C" void sync_screen_show(void) {
    if (!s_panel) return;
    s_visible = true;
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
}

extern "C" void sync_screen_hide(void) {
    if (!s_panel) return;
    s_visible = false;
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
}

extern "C" bool sync_screen_visible(void) {
    return s_visible;
}

static const char *state_text(racebox_state_t st) {
    switch (st) {
    case RACEBOX_SCANNING:   return "搜索蓝牙设备...";
    case RACEBOX_CONNECTING: return "正在连接设备";
    case RACEBOX_QUERYING:   return "读取设备状态";
    case RACEBOX_DOWNLOADING:return "下载轨迹数据";
    case RACEBOX_UPLOADING:  return "上传到服务器";
    case RACEBOX_ERASING:    return "清除设备内存";
    case RACEBOX_DONE:       return "同步完成";
    case RACEBOX_FAILED:     return "同步失败";
    default:                 return "空闲";
    }
}

extern "C" void sync_screen_update(const racebox_progress_t *p) {
    if (!s_panel || !p) return;

    char dev[80];
    if (p->device[0]) snprintf(dev, sizeof(dev), "设备 %s", p->device);
    else snprintf(dev, sizeof(dev), "设备 --");
    text_set(s_device, dev);

    /* message 已经是面向用户的完整状态；有详情时不再重复拼接 state_text。 */
    text_set(s_state, p->message[0] ? p->message : state_text(p->state));

    char count[48];
    if (p->total > 0) snprintf(count, sizeof(count), "%d / %d 条", p->received, p->total);
    else snprintf(count, sizeof(count), "%d 条", p->received);
    text_set(s_count, count);

    int pixels = 0;
    if (p->percent >= 0) {
        pixels = s_bar_width * p->percent / 100;
    } else if (p->state == RACEBOX_DONE) {
        pixels = s_bar_width;
    } else if (p->state == RACEBOX_SCANNING || p->state == RACEBOX_CONNECTING) {
        /* 不确定进度：显示一小段，表示在忙 */
        pixels = s_bar_width / 10;
    } else {
        pixels = s_bar_width / 3;
    }
    if (pixels > s_bar_width - 2) pixels = s_bar_width - 2;
    lv_obj_set_width(s_bar_fill, pixels);
    if (pixels > 0) lv_obj_clear_flag(s_bar_fill, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_bar_fill, LV_OBJ_FLAG_HIDDEN);
}
