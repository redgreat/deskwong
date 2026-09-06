#include "lvgl.h"
#include "main_screen.h"
#include "font_zh_14.h"
#include "lvgl_bsp.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_lbl_date = NULL;
static lv_obj_t *s_lbl_time = NULL;
static lv_obj_t *s_lbl_lunar = NULL;
static lv_obj_t *s_lbl_bottom = NULL;
static lv_obj_t *s_lbl_remind = NULL;
static lv_obj_t *s_cells[42];

static void style_cell(lv_obj_t *cell, int type, bool is_today) {
    /* 单色屏：用黑白反色 + 边框区分 */
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    if (type == CAL_WEEKEND || type == CAL_HOLIDAY) {
        lv_obj_set_style_bg_color(cell, lv_color_black(), 0);
        lv_obj_set_style_text_color(cell, lv_color_white(), 0);
    } else {
        lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
        lv_obj_set_style_text_color(cell, lv_color_black(), 0);
    }
    if (is_today) {
        lv_obj_set_style_border_width(cell, 2, 0);
        lv_obj_set_style_border_color(cell, lv_color_black(), 0);
    }
}

void main_screen_init(int width, int height) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_black(), 0);

    /* 顶部：日期（左）+ 星期（右） */
    s_lbl_date = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_zh_14, 0);
    lv_label_set_text(s_lbl_date, "----/--/--");
    lv_obj_align(s_lbl_date, LV_ALIGN_TOP_LEFT, 4, 2);

    /* 农历（右） */
    s_lbl_lunar = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_lunar, &lv_font_zh_14, 0);
    lv_label_set_text(s_lbl_lunar, "");
    lv_obj_align(s_lbl_lunar, LV_ALIGN_TOP_RIGHT, -4, 2);

    /* 时间（大字） */
    s_lbl_time = lv_label_create(scr);
    lv_label_set_text(s_lbl_time, "--:--");
    lv_obj_set_style_text_font(s_lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_time, LV_ALIGN_TOP_MID, 0, 20);

    /* 星期标题行 */
    static const char *week_head = "日 一 二 三 四 五 六";
    lv_obj_t *head = lv_label_create(scr);
    lv_obj_set_style_text_font(head, &lv_font_zh_14, 0);
    lv_label_set_text(head, week_head);
    lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_pad_hor(head, 8, 0);

    /* 月历网格 7×6 */
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 7; c++) {
            int idx = r * 7 + c;
            lv_obj_t *cell = lv_label_create(scr);
            lv_label_set_text(cell, "");
            lv_obj_set_style_text_font(cell, &lv_font_zh_14, 0);
            lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_size(cell, 42, 48);
            lv_obj_set_pos(cell, 3 + c * 42, 112 + r * 48);
            s_cells[idx] = cell;
        }
    }

    /* 底部信息 */
    s_lbl_bottom = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_bottom, &lv_font_zh_14, 0);
    lv_label_set_text(s_lbl_bottom, "");
    lv_obj_align(s_lbl_bottom, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    /* 提醒弹条 */
    s_lbl_remind = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_remind, &lv_font_zh_14, 0);
    lv_label_set_text(s_lbl_remind, "");
    lv_obj_align(s_lbl_remind, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(s_lbl_remind, LV_OBJ_FLAG_HIDDEN);
}

void main_screen_update_time(const char *date, const char *time, const char *week, const char *lunar) {
    char buf[40];
    if (date && week) {
        snprintf(buf, sizeof(buf), "%s %s", date, week);
        lv_label_set_text(s_lbl_date, buf);
    }
    if (s_lbl_time) lv_label_set_text(s_lbl_time, time);
    if (s_lbl_lunar && lunar) lv_label_set_text(s_lbl_lunar, lunar);
}

void main_screen_update_calendar(const calendar_cell_t cells[42]) {
    for (int i = 0; i < 42; i++) {
        const calendar_cell_t *c = &cells[i];
        if (c->day == 0) {
            lv_label_set_text(s_cells[i], "");
            lv_obj_set_style_bg_opa(s_cells[i], LV_OPA_TRANSP, 0);
            continue;
        }
        char buf[40];
        snprintf(buf, sizeof(buf), "%d\n%s", c->day, c->lunar);
        lv_label_set_text(s_cells[i], buf);
        style_cell(s_cells[i], c->type, c->is_today);
    }
}

void main_screen_update_bottom(const char *info) {
    if (s_lbl_bottom && info) lv_label_set_text(s_lbl_bottom, info);
}

void main_screen_show_remind(const char *msg) {
    if (!s_lbl_remind) return;
    lv_label_set_text(s_lbl_remind, msg);
    lv_obj_clear_flag(s_lbl_remind, LV_OBJ_FLAG_HIDDEN);
}
