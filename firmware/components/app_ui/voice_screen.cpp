#include "lvgl.h"
#include "ui_fonts.h"
#include "voice_screen.h"
#include <stdio.h>
#include <string.h>

/* 400x300 单色屏中下方的 384x96 弹窗（同黄历/同步窗体风格）。
 * 内存纪律同 weather_almanac_screen：对象共用静态 lv_style_t，
 * 文本写静态缓冲再 lv_label_set_text_static，别反复分配小字符串。 */
static const int PANEL_W = 384, PANEL_H = 96;

static lv_obj_t *s_panel, *s_back;
static lv_obj_t *s_status, *s_q, *s_a;
static lv_point_t s_rule_pts[2];
static bool s_visible;
static const int Q_ROW_Y = 29, A_ROW_Y = 63, DIALOG_ROW_H = 30;

static char s_status_b[16];
static char s_q_b[160];    /* ASR 文本，放不下自动换行裁切 */
static char s_a_b[256];    /* TTS 回复文本 */

static lv_style_t st_bg_black, st_bg_white;
static lv_style_t st_title;      /* 标题白字 */
static lv_style_t st_status;     /* 状态白字右对齐 */
static lv_style_t st_badge_solid, st_badge_line;  /* 问/AI 徽章 */
static lv_style_t st_dialog;     /* 对话正文：dialog_14 黑字左对齐换行 */
static lv_style_t st_dash;

static void init_styles(void) {
    lv_style_init(&st_bg_black);
    lv_style_set_bg_color(&st_bg_black, lv_color_black());
    lv_style_set_bg_opa(&st_bg_black, LV_OPA_COVER);
    lv_style_init(&st_bg_white);
    lv_style_set_bg_color(&st_bg_white, lv_color_white());
    lv_style_set_bg_opa(&st_bg_white, LV_OPA_COVER);

    lv_style_init(&st_title);
    lv_style_set_text_font(&st_title, &lv_font_zh_14);
    lv_style_set_text_color(&st_title, lv_color_white());
    lv_style_set_text_align(&st_title, LV_TEXT_ALIGN_LEFT);
    lv_style_init(&st_status);
    lv_style_set_text_font(&st_status, &lv_font_zh_14);
    lv_style_set_text_color(&st_status, lv_color_white());
    lv_style_set_text_align(&st_status, LV_TEXT_ALIGN_RIGHT);

    lv_style_init(&st_badge_solid);
    lv_style_set_bg_color(&st_badge_solid, lv_color_black());
    lv_style_set_bg_opa(&st_badge_solid, LV_OPA_COVER);
    lv_style_set_radius(&st_badge_solid, 5);
    lv_style_set_text_font(&st_badge_solid, &lv_font_zh_14);
    lv_style_set_text_color(&st_badge_solid, lv_color_white());
    lv_style_set_text_align(&st_badge_solid, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_top(&st_badge_solid, 4);
    lv_style_init(&st_badge_line);
    lv_style_set_bg_opa(&st_badge_line, LV_OPA_TRANSP);
    lv_style_set_radius(&st_badge_line, 5);
    lv_style_set_border_color(&st_badge_line, lv_color_black());
    lv_style_set_border_width(&st_badge_line, 1);
    lv_style_set_text_font(&st_badge_line, &lv_font_zh_14);
    lv_style_set_text_color(&st_badge_line, lv_color_black());
    lv_style_set_text_align(&st_badge_line, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_top(&st_badge_line, 4);

    lv_style_init(&st_dialog);
    lv_style_set_text_font(&st_dialog, &lv_font_dialog_14);
    lv_style_set_text_color(&st_dialog, lv_color_black());
    lv_style_set_text_align(&st_dialog, LV_TEXT_ALIGN_LEFT);

    lv_style_init(&st_dash);
    lv_style_set_line_color(&st_dash, lv_color_black());
    lv_style_set_line_width(&st_dash, 1);
    lv_style_set_line_opa(&st_dash, LV_OPA_COVER);
    lv_style_set_line_dash_width(&st_dash, 3);
    lv_style_set_line_dash_gap(&st_dash, 3);
}

static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, bool black, int radius) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, black ? &st_bg_black : &st_bg_white, 0);
    if (radius) lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *badge(lv_obj_t *p, const char *ch, int x, int y, bool solid) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, solid ? &st_badge_solid : &st_badge_line, 0);
    lv_label_set_text_static(o, ch);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, 27, 24);
    return o;
}

static lv_obj_t *dialog_label(lv_obj_t *p, int x, int y, int w, int h, const char *init) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, &st_dialog, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(o, init);
    return o;
}

static void center_dialog_text(lv_obj_t *label, const char *text, int row_y) {
    if (!label || !text) return;
    lv_point_t size = {};
    lv_coord_t width = lv_obj_get_width(label);
    lv_coord_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    lv_coord_t line_space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);
    lv_txt_get_size(&size, text, &lv_font_dialog_14, letter_space, line_space,
                    width, LV_TEXT_FLAG_NONE);
    lv_coord_t visible_h = size.y > DIALOG_ROW_H ? DIALOG_ROW_H : size.y;
    lv_obj_set_y(label, row_y + (DIALOG_ROW_H - visible_h) / 2);
}

extern "C" void voice_screen_init(int width, int height) {
    (void)width;
    const int x = 8, y = height - PANEL_H - 8;   /* 屏幕中下方，顶部留给时钟 */
    init_styles();

    s_back = box(lv_scr_act(), x + 5, y + 5, PANEL_W, PANEL_H, true, 12);
    lv_obj_add_flag(s_back, LV_OBJ_FLAG_HIDDEN);
    s_panel = box(lv_scr_act(), x, y, PANEL_W, PANEL_H, false, 10);
    lv_obj_set_style_border_width(s_panel, 2, 0);
    lv_obj_set_style_border_color(s_panel, lv_color_black(), 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    /* 标题条：左侧标题，右侧状态 */
    box(s_panel, 0, 0, PANEL_W, 24, true, 9);
    box(s_panel, 0, 17, PANEL_W, 7, true, 0);
    lv_obj_t *title = lv_label_create(s_panel);
    lv_obj_remove_style_all(title);
    lv_obj_add_style(title, &st_title, 0);
    lv_label_set_text_static(title, "小智语音");
    lv_obj_set_pos(title, 8, 4);
    s_status = lv_label_create(s_panel);
    lv_obj_remove_style_all(s_status);
    lv_obj_add_style(s_status, &st_status, 0);
    lv_obj_set_pos(s_status, PANEL_W - 110, 4);
    lv_obj_set_width(s_status, 102);

    /* 问（ASR）/ AI（TTS）两行正文，中间细虚线 */
    s_rule_pts[0].x = 0; s_rule_pts[0].y = 0;
    s_rule_pts[1].x = PANEL_W - 16; s_rule_pts[1].y = 0;

    badge(s_panel, "问", 8, 30, true);
    s_q = dialog_label(s_panel, 42, Q_ROW_Y, PANEL_W - 50, DIALOG_ROW_H, "...");
    lv_obj_t *line = lv_line_create(s_panel);
    lv_obj_remove_style_all(line);
    lv_obj_add_style(line, &st_dash, 0);
    lv_obj_set_pos(line, 8, 60);
    lv_line_set_points(line, s_rule_pts, 2);
    badge(s_panel, "AI", 8, 66, false);
    s_a = dialog_label(s_panel, 42, A_ROW_Y, PANEL_W - 50, DIALOG_ROW_H, "...");

    snprintf(s_status_b, sizeof(s_status_b), "连接中");
    snprintf(s_q_b, sizeof(s_q_b), "...");
    snprintf(s_a_b, sizeof(s_a_b), "...");
    center_dialog_text(s_q, s_q_b, Q_ROW_Y);
    center_dialog_text(s_a, s_a_b, A_ROW_Y);
}

extern "C" void voice_screen_set_status(const char *status) {
    if (!s_panel || !status || !status[0]) return;
    snprintf(s_status_b, sizeof(s_status_b), "%s", status);
    lv_label_set_text_static(s_status, s_status_b);
}

extern "C" void voice_screen_set_question(const char *text) {
    if (!s_panel || !text || !text[0]) return;
    snprintf(s_q_b, sizeof(s_q_b), "%s", text);
    lv_label_set_text_static(s_q, s_q_b);
    center_dialog_text(s_q, s_q_b, Q_ROW_Y);
}

/* 独立更新回复行：STT 与 TTS 分行展示 */
extern "C" void voice_screen_set_answer(const char *text) {
    if (!s_panel || !text || !text[0]) return;
    snprintf(s_a_b, sizeof(s_a_b), "%s", text);
    lv_label_set_text_static(s_a, s_a_b);
    center_dialog_text(s_a, s_a_b, A_ROW_Y);
}

extern "C" void voice_screen_show(void) {
    if (!s_panel) return;
    s_visible = true;
    lv_obj_clear_flag(s_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_back);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
}

extern "C" void voice_screen_hide(void) {
    if (!s_panel) return;
    s_visible = false;
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_back, LV_OBJ_FLAG_HIDDEN);
}

extern "C" bool voice_screen_visible(void) { return s_visible; }
