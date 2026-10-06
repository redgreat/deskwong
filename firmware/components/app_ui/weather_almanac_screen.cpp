#include "lvgl.h"
#include "ui_fonts.h"
#include "weather_almanac_screen.h"
#include <stdio.h>
#include <string.h>

#include "weather_icons.inc"

/* 400x300 单色屏上的 380x264 弹窗。
 * 天气区 7 列（含今天/当前小时），每列一个隐形近正方形单元（52 宽 x 56 高）：
 * 第一小行 图标+阴晴，第二小行 温度，第三小行 日期/小时；单元不画边框。
 * 日天气与小时天气之间、天气与黄历之间用 1px 细虚线分隔。
 *
 * 内存注意：设备/预览的 LVGL 池都是 128KB，主屏+同步屏已占 96KB。
 * 本屏 ~70 个对象必须共用静态 lv_style_t（对象本地样式属性每条都要从池里
 * 分配，曾把池耗尽导致 lv_obj_class_create_obj 解引用空指针崩溃）；
 * 文本一律写进静态缓冲再用 lv_label_set_text_static，避免每次刷新
 * 反复分配释放小字符串造成池碎片。 */
static const int PANEL_W = 380, PANEL_H = 264;
static const int CELL_MARGIN = 8;
static const int CELL_W = (PANEL_W - 2 * CELL_MARGIN) / WEATHER_DETAIL_SLOTS; /* 52 */

static lv_obj_t *s_panel, *s_back;
static lv_obj_t *s_day_icon[WEATHER_DETAIL_SLOTS], *s_day_text[WEATHER_DETAIL_SLOTS];
static lv_obj_t *s_hour_icon[WEATHER_DETAIL_SLOTS], *s_hour_text[WEATHER_DETAIL_SLOTS];
static lv_obj_t *s_day_temp_row, *s_day_label_row, *s_hour_temp_row, *s_hour_label_row;
static lv_obj_t *s_yi_l1, *s_yi_l2, *s_ji_l1, *s_ji_l2;   /* 宜/忌正文，最多两行 */
static lv_img_dsc_t s_day_img[WEATHER_DETAIL_SLOTS], s_hour_img[WEATHER_DETAIL_SLOTS];
static lv_point_t s_rule_pts[2];
static bool s_visible;

/* 单元格文本的静态缓冲（condition 最长 16B，来自 weather_service.h） */
static char s_day_cond_b[WEATHER_DETAIL_SLOTS][16];
static char s_day_temp_b[WEATHER_DETAIL_SLOTS][24];
static char s_day_label_b[WEATHER_DETAIL_SLOTS][16];
static char s_hour_cond_b[WEATHER_DETAIL_SLOTS][16];
static char s_hour_temp_b[WEATHER_DETAIL_SLOTS][16];
static char s_hour_label_b[WEATHER_DETAIL_SLOTS][16];
static char s_yi_b[sizeof(((weather_detail_t *)0)->almanac_yi)];
static char s_ji_b[sizeof(((weather_detail_t *)0)->almanac_ji)];
/* 宜/忌折行结果缓冲：一行最多是源文本的一个子串，加 "..." 的余量 */
static char s_yi_l1_b[sizeof(s_yi_b) + 4], s_yi_l2_b[sizeof(s_yi_b) + 4];
static char s_ji_l1_b[sizeof(s_ji_b) + 4], s_ji_l2_b[sizeof(s_ji_b) + 4];

/* 宜/忌徽章的纵向位置（28px 高） */
static const int BADGE_YI_Y = 176, BADGE_JI_Y = 214;
static const int BADGE_H = 28;
/* 正文可用宽度：x=42 起，右侧留 8px */
static const int CELL_TEXT_W = PANEL_W - 50;
/* 宜/忌正文用 zh_14：行框 15px、基线 2px，CJK 字面从行顶起高 13px，居中按字面算 */
static const int YIJI_INK_H = 13;
static const lv_font_t *const YIJI_FONT = &lv_font_zh_14;

/* 共享样式 */
static lv_style_t st_bg_black, st_bg_white;
static lv_style_t st_zh10;        /* 单元格文本：zh_10 黑字居中 */
static lv_style_t st_zh10_wrap;   /* 备用：zh_10 黑字左对齐换行 */
static lv_style_t st_zh14_wrap;   /* 宜/忌正文：zh_14 黑字左对齐换行 */
static lv_style_t st_title;       /* 标题：zh_14 白字居中 */
static lv_style_t st_badge_yi;    /* 宜：黑底白字圆角 */
static lv_style_t st_badge_ji;    /* 忌：白底黑字描边圆角 */
static lv_style_t st_icon;        /* 1-bit 图标按黑色着色 */
static lv_style_t st_dash;        /* 细虚线分隔 */

static void init_styles(void) {
    lv_style_init(&st_bg_black);
    lv_style_set_bg_color(&st_bg_black, lv_color_black());
    lv_style_set_bg_opa(&st_bg_black, LV_OPA_COVER);
    lv_style_init(&st_bg_white);
    lv_style_set_bg_color(&st_bg_white, lv_color_white());
    lv_style_set_bg_opa(&st_bg_white, LV_OPA_COVER);

    lv_style_init(&st_zh10);
    lv_style_set_text_font(&st_zh10, &lv_font_zh_10);
    lv_style_set_text_color(&st_zh10, lv_color_black());
    lv_style_set_text_align(&st_zh10, LV_TEXT_ALIGN_CENTER);
    lv_style_init(&st_zh10_wrap);
    lv_style_set_text_font(&st_zh10_wrap, &lv_font_zh_10);
    lv_style_set_text_color(&st_zh10_wrap, lv_color_black());
    lv_style_set_text_align(&st_zh10_wrap, LV_TEXT_ALIGN_LEFT);
    lv_style_init(&st_zh14_wrap);
    lv_style_set_text_font(&st_zh14_wrap, &lv_font_zh_14);
    lv_style_set_text_color(&st_zh14_wrap, lv_color_black());
    lv_style_set_text_align(&st_zh14_wrap, LV_TEXT_ALIGN_LEFT);
    lv_style_init(&st_title);
    lv_style_set_text_font(&st_title, &lv_font_zh_14);
    lv_style_set_text_color(&st_title, lv_color_white());
    lv_style_set_text_align(&st_title, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&st_badge_yi);
    lv_style_set_bg_color(&st_badge_yi, lv_color_black());
    lv_style_set_bg_opa(&st_badge_yi, LV_OPA_COVER);
    lv_style_set_radius(&st_badge_yi, 5);
    lv_style_set_text_font(&st_badge_yi, &lv_font_zh_14);
    lv_style_set_text_color(&st_badge_yi, lv_color_white());
    lv_style_set_text_align(&st_badge_yi, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_top(&st_badge_yi, 6);
    lv_style_init(&st_badge_ji);
    lv_style_set_bg_opa(&st_badge_ji, LV_OPA_TRANSP);
    lv_style_set_radius(&st_badge_ji, 5);
    lv_style_set_border_color(&st_badge_ji, lv_color_black());
    lv_style_set_border_width(&st_badge_ji, 1);
    lv_style_set_text_font(&st_badge_ji, &lv_font_zh_14);
    lv_style_set_text_color(&st_badge_ji, lv_color_black());
    lv_style_set_text_align(&st_badge_ji, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_top(&st_badge_ji, 6);

    lv_style_init(&st_icon);
    lv_style_set_img_recolor(&st_icon, lv_color_black());
    lv_style_set_img_recolor_opa(&st_icon, LV_OPA_COVER);

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

/* 徽章直接用带背景的 label，一个对象顶原来的 box+label 两个 */
static lv_obj_t *badge(lv_obj_t *p, const char *ch, int x, int y, bool solid) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, solid ? &st_badge_yi : &st_badge_ji, 0);
    lv_label_set_text_static(o, ch);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, 27, 28);
    return o;
}

/* 细虚线：1px 线宽、3px 段 3px 空，单对象实现。
 * 早期版本逐段创建小方块（两条分隔线 ~80 个对象），会挤爆 128KB LVGL 池。 */
static void dashed_rule(lv_obj_t *p, int y) {
    lv_obj_t *line = lv_line_create(p);
    lv_obj_remove_style_all(line);
    lv_obj_add_style(line, &st_dash, 0);
    lv_obj_set_pos(line, CELL_MARGIN, y);
    lv_line_set_points(line, s_rule_pts, 2);
}

static const uint8_t *icon_data(int code) {
    for (const auto &icon : weather_icons) if (icon.code == code) return icon.data;
    return nullptr;
}

static void set_icon(lv_obj_t *obj, lv_img_dsc_t *dsc, int code) {
    const uint8_t *data = icon_data(code);
    if (!data) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    dsc->header.cf = LV_IMG_CF_ALPHA_1BIT;
    dsc->header.always_zero = 0;
    dsc->header.reserved = 0;
    dsc->header.w = 28;
    dsc->header.h = 28;
    dsc->data_size = ((28 + 7) / 8) * 28;
    dsc->data = data;
    lv_img_set_src(obj, dsc);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

typedef struct {
    const char *items[WEATHER_DETAIL_SLOTS];
} weather_text_row_t;

static weather_text_row_t s_day_temp_draw, s_day_label_draw, s_hour_temp_draw, s_hour_label_draw;

/* 一整行只占一个轻量绘制对象，但每列文字仍独立测宽和定位。这样既不会依赖
 * 多行 label 的 CENTER 行为，也不会为了 28 段短文本耗尽 128KB LVGL 对象池。 */
static void draw_weather_text_row(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    weather_text_row_t *row = (weather_text_row_t *)lv_event_get_user_data(e);
    lv_area_t obj_area;
    lv_obj_get_coords(obj, &obj_area);
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font = &lv_font_zh_10;
    dsc.color = lv_color_black();
    for (int i = 0; i < WEATHER_DETAIL_SLOTS; ++i) {
        const char *text = row->items[i] ? row->items[i] : "--";
        int text_w = (int)lv_txt_get_width(text, (uint32_t)strlen(text), dsc.font,
                                           0, LV_TEXT_FLAG_NONE);
        if (text_w < 1) text_w = 1;
        if (text_w > CELL_W) text_w = CELL_W;
        lv_area_t text_area = {
            .x1 = (lv_coord_t)(obj_area.x1 + CELL_MARGIN + i * CELL_W + (CELL_W - text_w) / 2),
            .y1 = obj_area.y1,
            .x2 = (lv_coord_t)(obj_area.x1 + CELL_MARGIN + i * CELL_W + (CELL_W - text_w) / 2 + text_w - 1),
            .y2 = (lv_coord_t)(obj_area.y1 + dsc.font->line_height - 1),
        };
        lv_draw_label(draw_ctx, &dsc, &text_area, text, NULL);
    }
}

static lv_obj_t *weather_text_row(lv_obj_t *parent, int y, weather_text_row_t *row) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, 0, y);
    lv_obj_set_size(o, PANEL_W, lv_font_zh_10.line_height);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(o, draw_weather_text_row, LV_EVENT_DRAW_MAIN, row);
    return o;
}

static void make_weather_row(lv_obj_t *parent, int top, lv_obj_t **icons, lv_obj_t **texts) {
    for (int i = 0; i < WEATHER_DETAIL_SLOTS; ++i) {
        int x = CELL_MARGIN + i * CELL_W;
        icons[i] = lv_img_create(parent);
        lv_obj_remove_style_all(icons[i]);
        lv_obj_add_style(icons[i], &st_icon, 0);
        lv_obj_set_pos(icons[i], x, top);
        lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
        /* 阴晴只放得下两个字，放不下整体裁切（LABEL 长度模式不能换行增高） */
        texts[i] = lv_label_create(parent);
        lv_obj_remove_style_all(texts[i]);
        lv_obj_add_style(texts[i], &st_zh10, 0);
        lv_obj_set_pos(texts[i], x + 29, top + 8);
        lv_obj_set_width(texts[i], CELL_W - 29);
        lv_label_set_long_mode(texts[i], LV_LABEL_LONG_CLIP);
        lv_label_set_text_static(texts[i], "--");
    }
}

static lv_obj_t *cell_label(lv_obj_t *parent, int x, int y, int w, lv_style_t *st) {
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, st, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    return o;
}

/* ---- 宜/忌正文：手动折行 + 垂直居中 + 省略号 ----
 * lv_label 的 WRAP 模式既不能限制行数也不会加省略号，黄历文本
 * 按黄历字体字宽手动折成最多两行；放不下的部分以 "..." 结尾。
 * 一行在 28px 徽章带内按字面居中；两行时上下各占一半分别居中。 */

static const char *utf8_next(const char *p) {
    uint8_t c = (uint8_t)*p++;
    int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : 3;
    while (extra-- && *p) ++p;
    return p;
}

static const char *utf8_prev(const char *start, const char *p) {
    --p;
    while (p > start && ((uint8_t)*p & 0xC0) == 0x80) --p;
    return p;
}

static bool text_fits(const char *s, const char *e, int extra_px) {
    return lv_txt_get_width(s, (uint32_t)(e - s), YIJI_FONT, 0, LV_TEXT_FLAG_NONE) + extra_px <= CELL_TEXT_W;
}

/* 把 src 折成最多两行写入 l1/l2，返回行数。第二行放不下时从尾部
 * 逐字回退并加 "..."；下一行行首不能是顿号等禁则标点。 */
static int wrap_two_lines(const char *src, char *l1, size_t l1sz, char *l2, size_t l2sz) {
    static const char *const kinsoku[] = {"、", "，", "。", "；", "：", "）"};
    const char *p = src, *cut = src;
    while (*p) {
        const char *next = utf8_next(p);
        if (!text_fits(src, next, 0)) break;
        cut = next;
        p = next;
    }
    if (!*p) {                       /* 一行放得下 */
        snprintf(l1, l1sz, "%s", src);
        l2[0] = '\0';
        return 1;
    }
    p = cut;
    for (unsigned i = 0; i < sizeof(kinsoku) / sizeof(kinsoku[0]); ++i) {
        if (!strncmp(p, kinsoku[i], strlen(kinsoku[i]))) {
            /* 标点连同其前一个字符一起移到第二行行首，第一行不超宽 */
            if (p > src) p = utf8_prev(src, p);
            break;
        }
    }
    int ell_w = (int)lv_txt_get_width("...", 3, YIJI_FONT, 0, LV_TEXT_FLAG_NONE);
    const char *end = p + strlen(p);
    if (text_fits(p, end, 0)) {          /* 第二行原样放得下，不加省略号 */
        snprintf(l2, l2sz, "%s", p);
    } else {                             /* 放不下：尾部逐字回退，截断成 "..." */
        while (end > p && !text_fits(p, end, ell_w)) {
            end = utf8_prev(p, end);
        }
        snprintf(l2, l2sz, "%.*s...", (int)(end - p), p);
    }
    snprintf(l1, l1sz, "%.*s", (int)(p - src), src);
    return 2;
}

static void place_almanac_text(lv_obj_t *l1, lv_obj_t *l2, const char *src,
                               char *b1, size_t b1sz, char *b2, size_t b2sz, int badge_y) {
    int lines = wrap_two_lines(src, b1, b1sz, b2, b2sz);
    lv_label_set_text_static(l1, b1);
    lv_obj_set_y(l1, badge_y + (lines == 2 ? (BADGE_H / 2 - YIJI_INK_H) / 2
                                           : (BADGE_H - YIJI_INK_H) / 2));
    if (lines == 2) {
        lv_label_set_text_static(l2, b2);
        lv_obj_set_y(l2, badge_y + BADGE_H / 2 + (BADGE_H / 2 - YIJI_INK_H) / 2);
        lv_obj_clear_flag(l2, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(l2, LV_OBJ_FLAG_HIDDEN);
    }
}

extern "C" void weather_almanac_screen_init(int width, int height) {
    (void)width; (void)height;
    init_styles();
    const int x = 10, y = 18;
    s_back = box(lv_scr_act(), x + 5, y + 5, PANEL_W, PANEL_H, true, 12);
    lv_obj_add_flag(s_back, LV_OBJ_FLAG_HIDDEN);
    s_panel = box(lv_scr_act(), x, y, PANEL_W, PANEL_H, false, 10);
    lv_obj_set_style_border_width(s_panel, 2, 0);
    lv_obj_set_style_border_color(s_panel, lv_color_black(), 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    box(s_panel, 0, 0, PANEL_W, 25, true, 9);
    box(s_panel, 0, 17, PANEL_W, 8, true, 0);
    lv_obj_t *title = lv_label_create(s_panel);
    lv_obj_remove_style_all(title);
    lv_obj_add_style(title, &st_title, 0);
    lv_label_set_text_static(title, "天气黄历");
    lv_obj_set_pos(title, 8, 4);
    lv_obj_set_width(title, PANEL_W - 16);

    s_rule_pts[0].x = 0; s_rule_pts[0].y = 0;
    s_rule_pts[1].x = PANEL_W - 2 * CELL_MARGIN; s_rule_pts[1].y = 0;

    for (int i = 0; i < WEATHER_DETAIL_SLOTS; ++i) {
        snprintf(s_day_temp_b[i], sizeof(s_day_temp_b[i]), "--℃/--℃");
        snprintf(s_day_label_b[i], sizeof(s_day_label_b[i]), "--");
        snprintf(s_hour_temp_b[i], sizeof(s_hour_temp_b[i]), "--℃");
        snprintf(s_hour_label_b[i], sizeof(s_hour_label_b[i]), "--");
        s_day_temp_draw.items[i] = s_day_temp_b[i];
        s_day_label_draw.items[i] = s_day_label_b[i];
        s_hour_temp_draw.items[i] = s_hour_temp_b[i];
        s_hour_label_draw.items[i] = s_hour_label_b[i];
    }
    make_weather_row(s_panel, 31, s_day_icon, s_day_text);
    s_day_temp_row = weather_text_row(s_panel, 62, &s_day_temp_draw);
    s_day_label_row = weather_text_row(s_panel, 74, &s_day_label_draw);
    dashed_rule(s_panel, 92);
    make_weather_row(s_panel, 99, s_hour_icon, s_hour_text);
    s_hour_temp_row = weather_text_row(s_panel, 130, &s_hour_temp_draw);
    s_hour_label_row = weather_text_row(s_panel, 142, &s_hour_label_draw);
    dashed_rule(s_panel, 160);

    badge(s_panel, "宜", 8, BADGE_YI_Y, true);
    badge(s_panel, "忌", 8, BADGE_JI_Y, false);
    s_yi_l1 = cell_label(s_panel, 42, BADGE_YI_Y, CELL_TEXT_W, &st_zh14_wrap);
    s_yi_l2 = cell_label(s_panel, 42, BADGE_YI_Y, CELL_TEXT_W, &st_zh14_wrap);
    s_ji_l1 = cell_label(s_panel, 42, BADGE_JI_Y, CELL_TEXT_W, &st_zh14_wrap);
    s_ji_l2 = cell_label(s_panel, 42, BADGE_JI_Y, CELL_TEXT_W, &st_zh14_wrap);
    place_almanac_text(s_yi_l1, s_yi_l2, "请在后台配置黄历接口",
                       s_yi_l1_b, sizeof(s_yi_l1_b), s_yi_l2_b, sizeof(s_yi_l2_b), BADGE_YI_Y);
    place_almanac_text(s_ji_l1, s_ji_l2, "暂无数据",
                       s_ji_l1_b, sizeof(s_ji_l1_b), s_ji_l2_b, sizeof(s_ji_l2_b), BADGE_JI_Y);
}

extern "C" void weather_almanac_screen_update(const weather_detail_t *d) {
    if (!s_panel || !d) return;
    for (int i = 0; i < WEATHER_DETAIL_SLOTS; ++i) {
        const weather_forecast_item_t *day = &d->daily[i];
        set_icon(s_day_icon[i], &s_day_img[i], day->valid ? day->icon : 0);
        snprintf(s_day_cond_b[i], sizeof(s_day_cond_b[i]), "%s",
                 day->valid && day->condition[0] ? day->condition : "--");
        if (day->valid) {
            snprintf(s_day_temp_b[i], sizeof(s_day_temp_b[i]), "%d℃/%d℃",
                     day->temp_max, day->temp_min);
            snprintf(s_day_label_b[i], sizeof(s_day_label_b[i]), "%s", day->label);
        } else {
            snprintf(s_day_temp_b[i], sizeof(s_day_temp_b[i]), "--℃/--℃");
            snprintf(s_day_label_b[i], sizeof(s_day_label_b[i]), "--");
        }
        lv_label_set_text_static(s_day_text[i], s_day_cond_b[i]);

        const weather_forecast_item_t *hour = &d->hourly[i];
        set_icon(s_hour_icon[i], &s_hour_img[i], hour->valid ? hour->icon : 0);
        snprintf(s_hour_cond_b[i], sizeof(s_hour_cond_b[i]), "%s",
                 hour->valid && hour->condition[0] ? hour->condition : "--");
        if (hour->valid) {
            snprintf(s_hour_temp_b[i], sizeof(s_hour_temp_b[i]), "%d℃", hour->temp_min);
            snprintf(s_hour_label_b[i], sizeof(s_hour_label_b[i]), "%s", hour->label);
        } else {
            snprintf(s_hour_temp_b[i], sizeof(s_hour_temp_b[i]), "--℃");
            snprintf(s_hour_label_b[i], sizeof(s_hour_label_b[i]), "--");
        }
        lv_label_set_text_static(s_hour_text[i], s_hour_cond_b[i]);
    }
    lv_obj_invalidate(s_day_temp_row);
    lv_obj_invalidate(s_day_label_row);
    lv_obj_invalidate(s_hour_temp_row);
    lv_obj_invalidate(s_hour_label_row);
    snprintf(s_yi_b, sizeof(s_yi_b), "%s", d->almanac_yi[0] ? d->almanac_yi : "请在后台配置黄历接口");
    snprintf(s_ji_b, sizeof(s_ji_b), "%s", d->almanac_ji[0] ? d->almanac_ji : "暂无数据");
    place_almanac_text(s_yi_l1, s_yi_l2, s_yi_b,
                       s_yi_l1_b, sizeof(s_yi_l1_b), s_yi_l2_b, sizeof(s_yi_l2_b), BADGE_YI_Y);
    place_almanac_text(s_ji_l1, s_ji_l2, s_ji_b,
                       s_ji_l1_b, sizeof(s_ji_l1_b), s_ji_l2_b, sizeof(s_ji_l2_b), BADGE_JI_Y);
}

extern "C" void weather_almanac_screen_show(void) {
    if (!s_panel) return;
    s_visible = true;
    lv_obj_clear_flag(s_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_back);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
}

extern "C" void weather_almanac_screen_hide(void) {
    if (!s_panel) return;
    s_visible = false;
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_back, LV_OBJ_FLAG_HIDDEN);
}

extern "C" bool weather_almanac_screen_visible(void) { return s_visible; }
