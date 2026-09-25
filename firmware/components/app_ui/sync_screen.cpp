#include "lvgl.h"
#include "sync_screen.h"
#include "ui_fonts.h"
#include <stdio.h>
#include <string.h>

/* 400x300 单色反射屏：白底黑字，弹窗覆盖在主屏之上 */
static lv_obj_t *s_panel = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_state = NULL;
static lv_obj_t *s_count = NULL;
static lv_obj_t *s_uploaded = NULL;
static lv_obj_t *s_elapsed = NULL;
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

static void text_set(lv_obj_t *o, const char *v) {
    if (o && v && strcmp(lv_label_get_text(o), v)) lv_label_set_text(o, v);
}

extern "C" void sync_screen_init(int width, int height) {
    int panel_w = 224;
    int panel_h = 128;
    int panel_x = (width - panel_w) / 2;
    int panel_y = 40;

    /* Crisp offset backing reads as a deliberate card shadow on a 1-bit panel. */
    lv_obj_t *back = sbox(lv_scr_act(), panel_x + 5, panel_y + 5, panel_w, panel_h, true, 12);
    lv_obj_add_flag(back, LV_OBJ_FLAG_HIDDEN);
    s_panel = sbox(lv_scr_act(), panel_x, panel_y, panel_w, panel_h, false, 10);
    lv_obj_set_style_border_width(s_panel, 2, 0);
    lv_obj_set_style_border_color(s_panel, lv_color_black(), 0);
    lv_obj_set_style_border_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_user_data(s_panel, back);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    /* 标题条：黑底白字 */
    /* Full-width header shares the card's top edge; the square lower fill
     * removes the one-pixel inset/notch caused by two independent radii. */
    sbox(s_panel, 0, 14, panel_w, 14, true, 0);
    lv_obj_t *bar = sbox(s_panel, 0, 0, panel_w, 22, true, 10);
    s_title = lv_label_create(bar);
    lv_obj_remove_style_all(s_title);
    lv_obj_set_style_text_font(s_title, &lv_font_zh_14, 0);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_label_set_text(s_title, "RaceBox设备同步");
    lv_obj_set_width(s_title, panel_w - 22);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(s_title, 8, 4);

    s_state = slabel(s_panel, "准备中...", &lv_font_zh_14, 10, 34, panel_w - 20);
    lv_obj_set_style_text_align(s_state, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *download_box = sbox(s_panel, 10, 57, 98, 27, true, 7);
    s_count = slabel(download_box, "下载 0 条", &lv_font_zh_14, 3, 5, 92);
    lv_obj_set_style_text_color(s_count, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_count, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *upload_box = sbox(s_panel, 116, 57, 98, 27, false, 7);
    lv_obj_set_style_border_width(upload_box, 2, 0);
    lv_obj_set_style_border_color(upload_box, lv_color_black(), 0);
    lv_obj_set_style_border_opa(upload_box, LV_OPA_COVER, 0);
    s_uploaded = slabel(upload_box, "上传 0 条", &lv_font_zh_14, 3, 5, 92);
    lv_obj_set_style_text_align(s_uploaded, LV_TEXT_ALIGN_CENTER, 0);

    s_bar_width = panel_w - 20;
    s_bar = sbox(s_panel, 10, 94, s_bar_width, 8, false, 3);
    lv_obj_set_style_border_width(s_bar, 1, 0);
    lv_obj_set_style_border_color(s_bar, lv_color_black(), 0);
    lv_obj_set_style_border_opa(s_bar, LV_OPA_COVER, 0);
    s_bar_fill = sbox(s_bar, 1, 1, 0, 6, true, 2);
    lv_obj_add_flag(s_bar_fill, LV_OBJ_FLAG_HIDDEN);

    sbox(s_panel, 10, 114, 43, 1, true, 0);
    sbox(s_panel, 171, 114, 43, 1, true, 0);
    s_elapsed = slabel(s_panel, "总计用时 0 秒", &lv_font_zh_10, 56, 108, 112);
    lv_obj_set_style_text_align(s_elapsed, LV_TEXT_ALIGN_CENTER, 0);

}

extern "C" void sync_screen_show(void) {
    if (!s_panel) return;
    s_visible = true;
    lv_obj_t *back = (lv_obj_t *)lv_obj_get_user_data(s_panel);
    if (back) { lv_obj_clear_flag(back, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(back); }
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
}

extern "C" void sync_screen_hide(void) {
    if (!s_panel) return;
    s_visible = false;
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *back = (lv_obj_t *)lv_obj_get_user_data(s_panel);
    if (back) lv_obj_add_flag(back, LV_OBJ_FLAG_HIDDEN);
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

    text_set(s_title, p->device[0] ? p->device : "RaceBox设备同步");

    /* message 已经是面向用户的完整状态；有详情时不再重复拼接 state_text。 */
    text_set(s_state, p->message[0] ? p->message : state_text(p->state));

    char count[48];
    snprintf(count, sizeof(count), "下载 %d 条", p->received);
    text_set(s_count, count);
    snprintf(count, sizeof(count), "上传 %d 条", p->uploaded);
    text_set(s_uploaded, count);
    snprintf(count, sizeof(count), "总计用时 %d 秒", p->elapsed_seconds);
    text_set(s_elapsed, count);

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
