/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --font C:/Windows/Fonts/simhei.ttf -r 0x20-0x7E --symbols 日一二三四五六七八九十正冬腊闰初廿小寒大立春雨水惊蛰清明谷夏满芒种暑秋处白露霜降雪至元旦节劳动端午中国庆调休上班晴多云阴雾霾风月年时分周期农历天今室内北京杭州阵雷暴夹浮尘扬沙热冷未知准备企微签到签退记得记录工宜忌专注拖延每同步已连接失败扫描下载上传清除天气五周额度暂无数据不可用剩余温湿度设备搜索蓝牙正在读取状态轨迹服务器再次短按返回主屏完成空闲发现条取消令位例冰冻命如存少强待断毛等米纬经置要试请超送重雹需响应拒绝解锁间伴有极端降细特到薄浓度严重龙卷，。！？：；（） --size 10 --bpp 1 --no-compress --format lvgl --lv-font-name lv_font_zh_10 --no-kerning -o D:\wangcw\Documents\github\deskwong/firmware/components/app_ui/font_zh_10.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_ZH_10
#define LV_FONT_ZH_10 1
#endif

#if LV_FONT_ZH_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfa,

    /* U+0022 "\"" */
    0xf0,

    /* U+0023 "#" */
    0x52, 0xbe, 0xaf, 0xb2, 0x80,

    /* U+0024 "$" */
    0x4e, 0xdc, 0x65, 0xde, 0x40,

    /* U+0025 "%" */
    0xd7, 0x39, 0xf2, 0xb4, 0xe0,

    /* U+0026 "&" */
    0x66, 0x64, 0xb9, 0xf0,

    /* U+0027 "'" */
    0xc0,

    /* U+0028 "(" */
    0x4a, 0x49, 0x22, 0x20,

    /* U+0029 ")" */
    0x48, 0x92, 0x4a, 0x80,

    /* U+002A "*" */
    0x25, 0x5d, 0xf2, 0x0,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0xc0,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x8, 0x84, 0x42, 0x22, 0x10,

    /* U+0030 "0" */
    0x72, 0x53, 0x18, 0xa5, 0x44,

    /* U+0031 "1" */
    0x75, 0x54,

    /* U+0032 "2" */
    0x69, 0x12, 0x44, 0xf0,

    /* U+0033 "3" */
    0x2d, 0x12, 0x31, 0x96,

    /* U+0034 "4" */
    0x11, 0x94, 0xa9, 0x7c, 0x40,

    /* U+0035 "5" */
    0xf8, 0xe1, 0x19, 0xe0,

    /* U+0036 "6" */
    0x24, 0x69, 0x99, 0x60,

    /* U+0037 "7" */
    0xf1, 0x22, 0x44, 0x40,

    /* U+0038 "8" */
    0x69, 0x96, 0x99, 0x60,

    /* U+0039 "9" */
    0x69, 0x99, 0x62, 0x40,

    /* U+003A ":" */
    0x88,

    /* U+003B ";" */
    0x8c,

    /* U+003C "<" */
    0x0, 0x88, 0x88, 0x20, 0x83,

    /* U+003D "=" */
    0xf8, 0x3e,

    /* U+003E ">" */
    0x2, 0x8, 0x21, 0x11, 0x10,

    /* U+003F "?" */
    0x69, 0x12, 0x40, 0x40,

    /* U+0040 "@" */
    0x77, 0xfb, 0xde, 0xf9, 0x80,

    /* U+0041 "A" */
    0x23, 0x14, 0xa7, 0x4a, 0x20,

    /* U+0042 "B" */
    0xe9, 0x9e, 0x99, 0xe0,

    /* U+0043 "C" */
    0x69, 0x98, 0x99, 0x60,

    /* U+0044 "D" */
    0xe9, 0x99, 0x99, 0xe0,

    /* U+0045 "E" */
    0xf8, 0x8f, 0x88, 0xf0,

    /* U+0046 "F" */
    0xf8, 0x8e, 0x88, 0x80,

    /* U+0047 "G" */
    0x69, 0x8b, 0x99, 0x70,

    /* U+0048 "H" */
    0x99, 0x9f, 0x99, 0x90,

    /* U+0049 "I" */
    0xfe,

    /* U+004A "J" */
    0x11, 0x11, 0x99, 0x60,

    /* U+004B "K" */
    0x9a, 0xce, 0xa9, 0x90,

    /* U+004C "L" */
    0x88, 0x88, 0x88, 0xf0,

    /* U+004D "M" */
    0x9f, 0xff, 0xfd, 0xd0,

    /* U+004E "N" */
    0x9d, 0xdd, 0xbb, 0x90,

    /* U+004F "O" */
    0x69, 0x99, 0x99, 0x60,

    /* U+0050 "P" */
    0xe9, 0x9e, 0x88, 0x80,

    /* U+0051 "Q" */
    0x69, 0x99, 0xbb, 0x70,

    /* U+0052 "R" */
    0xe9, 0x9e, 0xaa, 0x90,

    /* U+0053 "S" */
    0x69, 0x86, 0x19, 0x60,

    /* U+0054 "T" */
    0xf9, 0x8, 0x42, 0x10, 0x80,

    /* U+0055 "U" */
    0x99, 0x99, 0x99, 0x60,

    /* U+0056 "V" */
    0x8c, 0x94, 0xa5, 0x10, 0x80,

    /* U+0057 "W" */
    0xad, 0x7b, 0xe7, 0x29, 0x40,

    /* U+0058 "X" */
    0x52, 0x88, 0x46, 0x2a, 0x40,

    /* U+0059 "Y" */
    0x8a, 0x94, 0x42, 0x10, 0x80,

    /* U+005A "Z" */
    0xf1, 0x24, 0x48, 0xf0,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x9c,

    /* U+005C "\\" */
    0x12, 0x44, 0x91, 0x20,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x3c,

    /* U+005E "^" */
    0x54,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x48,

    /* U+0061 "a" */
    0x79, 0x79, 0xf0,

    /* U+0062 "b" */
    0x88, 0xe9, 0x99, 0xe0,

    /* U+0063 "c" */
    0x69, 0x89, 0x60,

    /* U+0064 "d" */
    0x11, 0x79, 0x99, 0x70,

    /* U+0065 "e" */
    0x69, 0xf9, 0x60,

    /* U+0066 "f" */
    0x74, 0xf4, 0x44, 0x40,

    /* U+0067 "g" */
    0x7a, 0x9c, 0x87, 0xbc,

    /* U+0068 "h" */
    0x88, 0xbd, 0x99, 0x90,

    /* U+0069 "i" */
    0xbe,

    /* U+006A "j" */
    0x20, 0x92, 0x4f,

    /* U+006B "k" */
    0x88, 0xaa, 0xea, 0x90,

    /* U+006C "l" */
    0xfe,

    /* U+006D "m" */
    0xfd, 0x6b, 0x5a, 0x80,

    /* U+006E "n" */
    0xbd, 0x99, 0x90,

    /* U+006F "o" */
    0x69, 0x99, 0x60,

    /* U+0070 "p" */
    0xe9, 0x99, 0xe8,

    /* U+0071 "q" */
    0x79, 0x99, 0x71,

    /* U+0072 "r" */
    0xba, 0x48,

    /* U+0073 "s" */
    0xe8, 0x6a, 0xe0,

    /* U+0074 "t" */
    0x44, 0xf4, 0x44, 0x70,

    /* U+0075 "u" */
    0x99, 0x99, 0xf0,

    /* U+0076 "v" */
    0x92, 0x94, 0xc2, 0x0,

    /* U+0077 "w" */
    0xaf, 0x7d, 0xe5, 0x0,

    /* U+0078 "x" */
    0x53, 0x8, 0xa5, 0x0,

    /* U+0079 "y" */
    0x92, 0x94, 0x42, 0x20,

    /* U+007A "z" */
    0xf2, 0x48, 0xf0,

    /* U+007B "{" */
    0x69, 0x25, 0x92, 0x4c,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0xc9, 0x24, 0xd2, 0x58,

    /* U+007E "~" */
    0xc2,

    /* U+3002 "。" */
    0xf0,

    /* U+4E00 "一" */
    0xff, 0x80,

    /* U+4E03 "七" */
    0x10, 0x8, 0x4, 0x63, 0xcf, 0x0, 0x80, 0x40,
    0x21, 0x1f, 0x0,

    /* U+4E09 "三" */
    0x7f, 0x0, 0x0, 0xf, 0xc0, 0x0, 0x3, 0xfe,

    /* U+4E0A "上" */
    0x8, 0x4, 0x2, 0x1, 0xf0, 0x80, 0x40, 0x20,
    0x10, 0xff, 0x80,

    /* U+4E0B "下" */
    0xff, 0x84, 0x2, 0x1, 0x40, 0x90, 0x40, 0x20,
    0x10,

    /* U+4E0D "不" */
    0xff, 0x84, 0x6, 0x3, 0xc2, 0xb6, 0x4c, 0x20,
    0x10, 0x8, 0x0,

    /* U+4E13 "专" */
    0x8, 0x3f, 0x84, 0x1f, 0xf1, 0x1, 0xf8, 0x8,
    0x38, 0x6, 0x0,

    /* U+4E25 "严" */
    0xff, 0x8a, 0x85, 0xf, 0xf4, 0x2, 0x1, 0x1,
    0x0, 0x0, 0x0,

    /* U+4E2D "中" */
    0x10, 0x23, 0xfc, 0x99, 0x3f, 0xe4, 0x88, 0x10,

    /* U+4E3B "主" */
    0x10, 0x4, 0x3f, 0xe1, 0x7, 0xf0, 0x40, 0x20,
    0x10, 0xff, 0x80,

    /* U+4E5D "九" */
    0x10, 0x8, 0x1f, 0x82, 0x41, 0x21, 0x10, 0x88,
    0x85, 0xc3, 0x80,

    /* U+4E8C "二" */
    0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0xfe,

    /* U+4E91 "云" */
    0x7f, 0x0, 0x0, 0x1f, 0xf1, 0x0, 0x80, 0x88,
    0x8e, 0x78, 0x0,

    /* U+4E94 "五" */
    0xff, 0x10, 0x7e, 0x12, 0x22, 0x22, 0xff,

    /* U+4EAC "京" */
    0x8, 0x4, 0x3d, 0xe7, 0xe4, 0x12, 0x8, 0xfc,
    0x54, 0x4b, 0x44, 0x44, 0x0,

    /* U+4ECA "今" */
    0x8, 0xa, 0x4, 0x8d, 0x20, 0x9, 0xf8, 0x8,
    0x4, 0x4, 0x0,

    /* U+4EE4 "令" */
    0x8, 0xa, 0x8, 0x89, 0x38, 0xb, 0xf8, 0x8,
    0x28, 0x8, 0x2, 0x0,

    /* U+4F01 "企" */
    0x8, 0xc, 0x9, 0x89, 0x3a, 0x81, 0x78, 0xa0,
    0x50, 0x7f, 0x0,

    /* U+4F11 "休" */
    0x44, 0x22, 0x17, 0xf8, 0xc4, 0xe2, 0xa9, 0x56,
    0x89, 0x44, 0x0,

    /* U+4F20 "传" */
    0x12, 0xb, 0xe2, 0x41, 0xff, 0x24, 0x9, 0xe2,
    0x10, 0x9c, 0x21, 0x80,

    /* U+4F34 "伴" */
    0x24, 0x2a, 0x91, 0x1b, 0xe4, 0x43, 0xfd, 0x10,
    0x88, 0x44, 0x0,

    /* U+4F4D "位" */
    0x24, 0x22, 0x17, 0xf8, 0x4, 0xa2, 0x51, 0x28,
    0xbf, 0x40, 0x0,

    /* U+4F59 "余" */
    0x8, 0xa, 0x8, 0xff, 0xd0, 0x83, 0xf8, 0x28,
    0xd2, 0x99, 0x0,

    /* U+4F8B "例" */
    0x0, 0x3e, 0x52, 0xb9, 0xdd, 0x6b, 0xf5, 0x1a,
    0x91, 0x58, 0xa8, 0xc0,

    /* U+5143 "元" */
    0x7e, 0x0, 0x0, 0x1f, 0xe2, 0x81, 0x40, 0xa0,
    0x92, 0xc7, 0x0,

    /* U+516B "八" */
    0x2, 0x4, 0x81, 0x20, 0x48, 0x21, 0x8, 0x42,
    0x11, 0x2, 0x0, 0x0,

    /* U+516D "六" */
    0x0, 0x4, 0x2, 0x1f, 0xf0, 0x0, 0xb0, 0x88,
    0x82, 0xc0, 0x80,

    /* U+5185 "内" */
    0x10, 0xff, 0x91, 0x91, 0xa9, 0xa7, 0x81, 0x81,
    0x83,

    /* U+518D "再" */
    0xff, 0x82, 0x7, 0xf1, 0x24, 0x7f, 0x12, 0x4f,
    0xfd, 0x4, 0x43, 0x0,

    /* U+519C "农" */
    0x0, 0x2, 0x7, 0xf9, 0x22, 0x18, 0xd, 0x46,
    0x22, 0x8c, 0x28, 0xc4, 0x0,

    /* U+51AC "冬" */
    0x10, 0x7, 0xc3, 0x21, 0x30, 0xc, 0x3c, 0xe0,
    0xe0, 0x40, 0xe, 0x0,

    /* U+51B0 "冰" */
    0x4, 0x62, 0x1f, 0x42, 0xc5, 0x62, 0xaa, 0x95,
    0x49, 0xc, 0x0,

    /* U+51B7 "冷" */
    0x2, 0x11, 0x82, 0x90, 0x32, 0x32, 0x17, 0xe4,
    0x11, 0x38, 0x2, 0x0, 0x0,

    /* U+51BB "冻" */
    0x4, 0x2f, 0xca, 0x1, 0x41, 0xf9, 0x11, 0x2c,
    0xa5, 0x6, 0x0,

    /* U+51C6 "准" */
    0x4, 0x94, 0x5f, 0x34, 0x3f, 0x54, 0x9f, 0x94,
    0x9f, 0x10,

    /* U+5206 "分" */
    0x28, 0x12, 0x11, 0x88, 0x6f, 0xf0, 0x90, 0x88,
    0x48, 0xcc, 0x0,

    /* U+521D "初" */
    0x20, 0x7, 0xdc, 0xa4, 0x52, 0xab, 0x94, 0x92,
    0x49, 0x29, 0x80,

    /* U+5230 "到" */
    0xf8, 0xa0, 0x50, 0xb3, 0x5e, 0xa9, 0x17, 0xca,
    0x71, 0xc1, 0x80,

    /* U+5269 "剩" */
    0xf8, 0x91, 0x7e, 0xad, 0xde, 0xaf, 0xf5, 0xa3,
    0x49, 0x21, 0x80,

    /* U+52A1 "务" */
    0x10, 0x1f, 0x94, 0x81, 0x8f, 0xab, 0xf8, 0x44,
    0x22, 0xe2, 0x0,

    /* U+52A8 "动" */
    0x2, 0x39, 0x3, 0xfe, 0x54, 0x2a, 0x96, 0x53,
    0xc9, 0x9, 0x80,

    /* U+52B3 "劳" */
    0x24, 0xff, 0x24, 0xff, 0x91, 0x7e, 0x12, 0x22,
    0xc6,

    /* U+5317 "北" */
    0x14, 0xa, 0x5, 0x2e, 0xa1, 0x60, 0xa0, 0x51,
    0xe9, 0x17, 0x80,

    /* U+5341 "十" */
    0x8, 0x4, 0x2, 0x1f, 0xf0, 0x80, 0x40, 0x20,
    0x10, 0x8, 0x0,

    /* U+5348 "午" */
    0x20, 0x10, 0x1f, 0xc9, 0x8, 0x80, 0x43, 0xfe,
    0x10, 0x8, 0x4, 0x0,

    /* U+5377 "卷" */
    0x48, 0x5, 0x1f, 0xc2, 0xe, 0xfb, 0xfe, 0x98,
    0x42, 0x3f, 0x0,

    /* U+5386 "历" */
    0x7f, 0xa2, 0x11, 0xb, 0xe4, 0x92, 0x49, 0x25,
    0x22, 0xa3, 0x0,

    /* U+53D1 "发" */
    0x2a, 0x14, 0x9f, 0xe2, 0x1, 0xf1, 0x88, 0xa8,
    0x88, 0x3b, 0x80,

    /* U+53D6 "取" */
    0xf8, 0x2b, 0x95, 0x4e, 0xa7, 0x52, 0x91, 0xe9,
    0x2a, 0x10, 0x80,

    /* U+53EF "可" */
    0xff, 0xc0, 0x43, 0xd0, 0x94, 0x25, 0xf, 0x42,
    0x50, 0x4, 0x3, 0x0,

    /* U+540C "同" */
    0xff, 0x81, 0xbf, 0x81, 0xbd, 0xa5, 0xbd, 0x81,
    0x83,

    /* U+5468 "周" */
    0x7f, 0xa0, 0x57, 0xa9, 0x15, 0x6a, 0xf5, 0x4b,
    0x3d, 0x81, 0x80,

    /* U+547D "命" */
    0x8, 0xa, 0x8, 0xdb, 0x97, 0x72, 0xa9, 0x54,
    0xea, 0x4, 0x0,

    /* U+54CD "响" */
    0x8, 0x6f, 0xf4, 0x3a, 0xdd, 0xae, 0xd7, 0x5b,
    0x21, 0x11, 0x0,

    /* U+5668 "器" */
    0xee, 0xaa, 0x54, 0xff, 0x2c, 0xef, 0xaa, 0xee,
    0xaa,

    /* U+56DB "四" */
    0xff, 0x99, 0x99, 0x99, 0xa9, 0xa7, 0x81, 0xff,
    0x81,

    /* U+56DE "回" */
    0xff, 0x81, 0xbd, 0xa5, 0xa5, 0xbd, 0x81, 0xff,
    0x81,

    /* U+56FD "国" */
    0xff, 0x81, 0xbf, 0x89, 0xbf, 0x8b, 0xbf, 0x81,
    0xff,

    /* U+5728 "在" */
    0x8, 0x4, 0x1f, 0xe2, 0x41, 0x21, 0x7d, 0x88,
    0x44, 0x22, 0x1f, 0xc0,

    /* U+5904 "处" */
    0x22, 0xe, 0x84, 0xb1, 0x2a, 0xb2, 0x4, 0x81,
    0x20, 0xb0, 0xc3, 0x80,

    /* U+5907 "备" */
    0x10, 0x3, 0xf0, 0xc4, 0x7, 0xe, 0x8c, 0x7f,
    0xf, 0xe1, 0x24, 0x3f, 0x80,

    /* U+590F "夏" */
    0xff, 0xbf, 0x9f, 0xcf, 0xe7, 0xf1, 0xf3, 0x50,
    0x10, 0xf7, 0x80,

    /* U+591A "多" */
    0x10, 0x3e, 0xe4, 0x1c, 0xef, 0x31, 0x4a, 0xc,
    0xf0,

    /* U+5927 "大" */
    0x8, 0x4, 0x2, 0x1f, 0xf0, 0x80, 0xa0, 0x50,
    0x44, 0xc1, 0x80,

    /* U+5929 "天" */
    0x7f, 0x4, 0x2, 0x1f, 0xf0, 0x80, 0xa0, 0x58,
    0xc6, 0x80, 0x80,

    /* U+5931 "失" */
    0x10, 0x50, 0x7e, 0x90, 0xff, 0x18, 0x24, 0x63,
    0x80,

    /* U+5939 "夹" */
    0x8, 0x3f, 0x92, 0x45, 0x4f, 0xf8, 0x60, 0x48,
    0xc2, 0x80, 0x80,

    /* U+5982 "如" */
    0x20, 0x40, 0xff, 0x59, 0x99, 0x69, 0x37, 0x49,
    0x80,

    /* U+5B58 "存" */
    0x8, 0x3f, 0xc4, 0x5, 0xf6, 0x31, 0x7c, 0x88,
    0x44, 0x26, 0x0,

    /* U+5B8C "完" */
    0x8, 0x7f, 0xe0, 0x27, 0xcf, 0xf8, 0xa0, 0x50,
    0x49, 0xc7, 0x80,

    /* U+5B9C "宜" */
    0x8, 0x7f, 0xe0, 0x27, 0xc2, 0x21, 0xf0, 0xf8,
    0x44, 0xff, 0x80,

    /* U+5BA4 "室" */
    0x8, 0x7f, 0xa0, 0x4f, 0xc2, 0x20, 0xe1, 0xfc,
    0x10, 0xff, 0x80,

    /* U+5BD2 "寒" */
    0x4, 0x1e, 0xf3, 0xf8, 0x48, 0x3f, 0x3f, 0xf3,
    0x93, 0x1b, 0x1c, 0x0, 0x80,

    /* U+5C0F "小" */
    0x8, 0x4, 0x2, 0x9, 0x44, 0x92, 0x4a, 0x20,
    0x10, 0x18, 0x0,

    /* U+5C11 "少" */
    0x8, 0x4, 0xa, 0x49, 0x18, 0x80, 0x48, 0x8,
    0x18, 0x70, 0x0,

    /* U+5C18 "尘" */
    0x8, 0xa, 0x44, 0x92, 0x22, 0x8, 0x1f, 0xe0,
    0x80, 0x20, 0xff, 0xc0,

    /* U+5C4F "屏" */
    0x7f, 0x20, 0x9f, 0xc9, 0x45, 0xfa, 0x51, 0xff,
    0x14, 0x92, 0x0,

    /* U+5DDE "州" */
    0x21, 0x25, 0x25, 0x37, 0xb7, 0xad, 0x25, 0x45,
    0x45,

    /* U+5DE5 "工" */
    0x7f, 0x4, 0x2, 0x1, 0x0, 0x80, 0x43, 0xfe,

    /* U+5DF2 "已" */
    0xfe, 0x2, 0x2, 0x82, 0xfe, 0x80, 0x81, 0x81,
    0xff,

    /* U+5E74 "年" */
    0x30, 0xf, 0xe4, 0x40, 0xfe, 0x24, 0x9, 0xf,
    0xfc, 0x10, 0x4, 0x0,

    /* U+5E86 "庆" */
    0x4, 0x3f, 0xd1, 0x8, 0x87, 0xfa, 0x21, 0x29,
    0x92, 0xb0, 0x90, 0x0,

    /* U+5E94 "应" */
    0x4, 0x3f, 0xd0, 0x9, 0x26, 0x52, 0xa9, 0x48,
    0x84, 0xbf, 0x80, 0x0,

    /* U+5EA6 "度" */
    0x8, 0x3f, 0x94, 0x8f, 0xe5, 0xe3, 0xf1, 0x49,
    0x18, 0xb3, 0x80,

    /* U+5EF6 "延" */
    0x0, 0x3b, 0xc4, 0x21, 0x28, 0x6b, 0x8a, 0x8a,
    0xa1, 0x3e, 0x60, 0x27, 0xe0,

    /* U+5EFF "廿" */
    0x22, 0x11, 0x8, 0x9f, 0xf2, 0x21, 0x10, 0x88,
    0x7c, 0x22, 0x0,

    /* U+5F3A "强" */
    0xff, 0x18, 0xbb, 0x93, 0xed, 0x52, 0xf9, 0x10,
    0x8e, 0xd8, 0x80,

    /* U+5F55 "录" */
    0x7f, 0x0, 0x8f, 0x9f, 0xf0, 0x83, 0x78, 0xe9,
    0x93, 0x18, 0x0,

    /* U+5F85 "待" */
    0x12, 0xb, 0xe5, 0x20, 0xbf, 0x21, 0x1b, 0xe2,
    0x10, 0x94, 0x21, 0x0,

    /* U+5F97 "得" */
    0x3f, 0xa8, 0x6f, 0xef, 0xfd, 0xfa, 0xfd, 0x44,
    0x92, 0x43, 0x0,

    /* U+5FAE "微" */
    0x55, 0x25, 0x83, 0xf9, 0xa, 0xda, 0xb6, 0xa5,
    0xb1, 0x74, 0x46, 0xc0,

    /* U+5FCC "忌" */
    0x7f, 0x0, 0x47, 0xf1, 0x2, 0x7f, 0x82, 0xa,
    0x4a, 0x8d, 0x3e, 0x0,

    /* U+6001 "态" */
    0x10, 0xff, 0x18, 0x34, 0xc3, 0x0, 0xa9, 0xa3,
    0xbc,

    /* U+60CA "惊" */
    0x44, 0x2f, 0xf8, 0x1b, 0xe4, 0x92, 0x79, 0x10,
    0x9a, 0x54, 0x0,

    /* U+6210 "成" */
    0x5, 0x2, 0x1f, 0xe8, 0x87, 0x52, 0xa9, 0x48,
    0xac, 0x89, 0x88, 0x0,

    /* U+626B "扫" */
    0x20, 0x13, 0xfc, 0x24, 0x13, 0x7b, 0x4, 0x82,
    0x5f, 0x60, 0x80,

    /* U+626C "扬" */
    0x2f, 0x88, 0x47, 0x60, 0xbf, 0x35, 0x99, 0x62,
    0xa8, 0x8a, 0x6d, 0x80,

    /* U+62D2 "拒" */
    0x27, 0xc9, 0x7, 0x40, 0x9e, 0x34, 0x99, 0xe2,
    0x40, 0x90, 0x67, 0xc0, 0x0,

    /* U+62D6 "拖" */
    0x48, 0x27, 0xfe, 0x9, 0x26, 0xf6, 0xe9, 0x20,
    0x91, 0xcf, 0x80,

    /* U+6309 "按" */
    0x22, 0xb, 0xf7, 0x84, 0x88, 0x3f, 0xf9, 0x22,
    0xd0, 0x8c, 0x6c, 0xc0,

    /* U+636E "据" */
    0x2f, 0xca, 0x17, 0xfc, 0xa4, 0x3f, 0xfa, 0x42,
    0xfc, 0xb1, 0x77, 0xc0,

    /* U+63A5 "接" */
    0x44, 0x2f, 0xfa, 0x48, 0x46, 0xd6, 0xfd, 0x28,
    0x8c, 0xdd, 0x80,

    /* U+63CF "描" */
    0x4a, 0x2f, 0xfa, 0x8b, 0xed, 0x52, 0xf9, 0x54,
    0xaa, 0xdf, 0x0,

    /* U+641C "搜" */
    0x5f, 0x2a, 0xbf, 0xca, 0xa6, 0xe6, 0xf9, 0x28,
    0x98, 0xdf, 0x0,

    /* U+6570 "数" */
    0xac, 0x12, 0x3f, 0xee, 0xa8, 0xd7, 0xa9, 0x48,
    0x64, 0xed, 0x0,

    /* U+65AD "断" */
    0x63, 0x7a, 0x29, 0x1f, 0xfb, 0x57, 0x6a, 0x95,
    0xfa, 0x9, 0x0,

    /* U+65E0 "无" */
    0x7e, 0x10, 0x10, 0xff, 0x18, 0x28, 0x28, 0x49,
    0xc7,

    /* U+65E5 "日" */
    0xff, 0x6, 0xf, 0xf8, 0x30, 0x60, 0xff, 0x82,

    /* U+65E6 "旦" */
    0x7f, 0x20, 0x90, 0x4f, 0xe4, 0x12, 0x9, 0xfc,
    0x0, 0xff, 0x80,

    /* U+65F6 "时" */
    0x1, 0x70, 0xab, 0xfc, 0x2a, 0x95, 0x2b, 0x85,
    0x42, 0x3, 0x0,

    /* U+660E "明" */
    0xef, 0xa9, 0xa9, 0xef, 0xa9, 0xef, 0xa9, 0x11,
    0x12,

    /* U+6625 "春" */
    0x10, 0x7e, 0x7e, 0x10, 0xef, 0x7e, 0xc5, 0x7c,
    0x3c,

    /* U+6674 "晴" */
    0x0, 0x67, 0xb3, 0xdb, 0xfc, 0xf6, 0x4b, 0x3d,
    0x9e, 0x9, 0x0,

    /* U+6682 "暂" */
    0x47, 0x68, 0xff, 0xfa, 0x22, 0x7e, 0x42, 0x7e,
    0x7e,

    /* U+6691 "暑" */
    0x7f, 0x3f, 0x82, 0xf, 0xef, 0xf9, 0xfb, 0x84,
    0x7e, 0x3f, 0x0,

    /* U+66B4 "暴" */
    0x3f, 0x1f, 0x8f, 0xcf, 0xf7, 0xf8, 0xb1, 0xde,
    0x7a, 0xc, 0x0,

    /* U+6708 "月" */
    0x3f, 0x21, 0x3f, 0x21, 0x21, 0x3f, 0x21, 0x41,
    0x43,

    /* U+6709 "有" */
    0x8, 0x3f, 0xc4, 0x3, 0xe3, 0x12, 0xf8, 0x7c,
    0x22, 0x13, 0x0,

    /* U+670D "服" */
    0x77, 0x95, 0x27, 0x59, 0x50, 0x57, 0x9d, 0xa5,
    0x6b, 0x54, 0xb6, 0xc0,

    /* U+671F "期" */
    0x4b, 0xff, 0x52, 0xaf, 0x77, 0xaf, 0xfd, 0x13,
    0x19, 0x4, 0x80,

    /* U+672A "未" */
    0x8, 0x3f, 0x82, 0x1, 0xf, 0xf8, 0xe0, 0xa8,
    0x93, 0x88, 0x0,

    /* U+6761 "条" */
    0x10, 0x1f, 0x18, 0x8b, 0x81, 0x67, 0x4d, 0xfc,
    0x50, 0x6b, 0xc, 0x0,

    /* U+676D "杭" */
    0x22, 0xb, 0xe6, 0x0, 0x9c, 0x79, 0x1a, 0x42,
    0x90, 0xa4, 0x31, 0x80,

    /* U+6781 "极" */
    0x2f, 0x89, 0x47, 0x50, 0x96, 0x74, 0x99, 0xaa,
    0x90, 0xa6, 0x36, 0x40,

    /* U+6B21 "次" */
    0x4, 0x12, 0x4, 0xf8, 0xa2, 0x15, 0x9, 0x2,
    0x61, 0x28, 0x99, 0x80, 0x0,

    /* U+6B63 "正" */
    0xff, 0x4, 0x12, 0x9, 0xe4, 0x82, 0x41, 0x21,
    0xff,

    /* U+6B65 "步" */
    0x8, 0x17, 0x8a, 0x1f, 0xf0, 0x81, 0x49, 0x28,
    0x18, 0xf0, 0x0,

    /* U+6BCF "每" */
    0x0, 0x40, 0x7f, 0xbe, 0x52, 0xff, 0x52, 0x4a,
    0x7f, 0x6,

    /* U+6BDB "毛" */
    0x3, 0x1f, 0x0, 0x91, 0xf8, 0x8, 0xbf, 0xc0,
    0x80, 0x23, 0xf, 0x80,

    /* U+6C14 "气" */
    0x20, 0x3f, 0xb0, 0x17, 0xcf, 0xe0, 0x10, 0x8,
    0x5, 0x1, 0x0,

    /* U+6C34 "水" */
    0x8, 0x4, 0x3a, 0x45, 0xc2, 0xc2, 0x51, 0x25,
    0x11, 0x18, 0x0,

    /* U+6C99 "沙" */
    0x4, 0x22, 0x5, 0x52, 0x91, 0x42, 0x29, 0x9,
    0x8, 0xb8, 0x0,

    /* U+6CE8 "注" */
    0x8, 0x22, 0x7, 0xd0, 0x81, 0xf2, 0x21, 0x11,
    0x8, 0xbf, 0x80,

    /* U+6D53 "浓" */
    0x44, 0xf, 0xe1, 0x49, 0x10, 0xa, 0x82, 0xc7,
    0x91, 0x12, 0x88, 0x0,

    /* U+6D6E "浮" */
    0x0, 0xb, 0xe0, 0xa9, 0x24, 0xf, 0x80, 0x44,
    0x21, 0x7f, 0x42, 0x10, 0x80, 0x0,

    /* U+6D88 "消" */
    0x84, 0x95, 0x20, 0x42, 0x7e, 0x10, 0x97, 0xe5,
    0xfa, 0x42, 0x91, 0x80,

    /* U+6E05 "清" */
    0x84, 0x5e, 0x1f, 0xbf, 0x1f, 0x51, 0x5f, 0x9f,
    0x93,

    /* U+6E29 "温" */
    0x4f, 0xc, 0x40, 0xf1, 0x44, 0xe, 0x17, 0xe5,
    0xa9, 0x6a, 0xbf, 0xc0,

    /* U+6E7F "湿" */
    0x5f, 0x98, 0x47, 0xeb, 0xf0, 0x2, 0x55, 0x6a,
    0x94, 0x9f, 0x80,

    /* U+6EE1 "满" */
    0x5, 0x3f, 0xc1, 0x49, 0xe1, 0xf9, 0xad, 0x66,
    0xb5, 0x10, 0x80,

    /* U+70ED "热" */
    0x24, 0x1f, 0xc2, 0x53, 0xf4, 0x27, 0x5a, 0x20,
    0x1, 0x52, 0x80, 0x0,

    /* U+7259 "牙" */
    0xff, 0x2, 0x11, 0xf, 0xf0, 0xc0, 0xa0, 0x91,
    0x88, 0xc, 0x0,

    /* U+7279 "特" */
    0x22, 0x2b, 0xef, 0x22, 0xbf, 0x21, 0x1f, 0xfa,
    0x10, 0x94, 0x23, 0x0,

    /* U+72B6 "状" */
    0x24, 0x12, 0xa9, 0x7, 0xf2, 0x43, 0x22, 0xa8,
    0x52, 0x30, 0x0,

    /* U+73B0 "现" */
    0x7, 0x9d, 0x22, 0x69, 0xda, 0x26, 0x89, 0xa2,
    0x31, 0x55, 0x9, 0x80,

    /* U+73ED "班" */
    0x8, 0x75, 0xd2, 0x4b, 0x2f, 0xba, 0x49, 0x24,
    0xe2, 0x97, 0x80, 0x0,

    /* U+7528 "用" */
    0x7f, 0x49, 0x7f, 0x49, 0x49, 0x7f, 0x49, 0x49,
    0x8b,

    /* U+767D "白" */
    0x20, 0x43, 0xfc, 0x18, 0x3f, 0xe0, 0xc1, 0xff,
    0x4,

    /* U+77E5 "知" */
    0x0, 0x40, 0x3d, 0xf4, 0x92, 0x4f, 0xe4, 0x92,
    0xaf, 0x84, 0xc0, 0x0,

    /* U+77ED "短" */
    0x0, 0x27, 0xdc, 0x14, 0xe2, 0x93, 0xb8, 0x94,
    0xaa, 0x45, 0x47, 0xc0,

    /* U+79CB "秋" */
    0x12, 0x18, 0x82, 0xab, 0xea, 0x2b, 0x1c, 0x8a,
    0x50, 0x96, 0x28, 0xc0,

    /* U+79CD "种" */
    0x12, 0x31, 0xb, 0xff, 0x53, 0xab, 0xfe, 0xaa,
    0x44, 0x22, 0x0,

    /* U+7A7A "空" */
    0x8, 0x7f, 0xa0, 0x4c, 0x48, 0x13, 0xf8, 0x20,
    0x10, 0xff, 0x80,

    /* U+7ACB "立" */
    0x10, 0x4, 0x1f, 0xc0, 0x2, 0x21, 0x10, 0x50,
    0x28, 0xff, 0x80,

    /* U+7AEF "端" */
    0x49, 0xd, 0xe2, 0xbf, 0xa4, 0xdf, 0x57, 0xf7,
    0x17,

    /* U+7B49 "等" */
    0x42, 0x3f, 0xe9, 0x41, 0x3, 0xf7, 0xfc, 0x8,
    0xff, 0x22, 0x3, 0x0,

    /* U+7B7E "签" */
    0x22, 0xe, 0xe5, 0x50, 0x30, 0x33, 0xd7, 0xa0,
    0x90, 0x48, 0x7f, 0x80,

    /* U+7C73 "米" */
    0x9, 0x35, 0x2, 0x1f, 0xf1, 0xc1, 0x51, 0xa5,
    0x11, 0x8, 0x0,

    /* U+7D22 "索" */
    0x8, 0x3f, 0xbf, 0xf2, 0x17, 0xc3, 0xf8, 0x20,
    0x92, 0x98, 0x80,

    /* U+7EAC "纬" */
    0x22, 0x13, 0xea, 0x21, 0xbe, 0x42, 0x1b, 0xf0,
    0x27, 0xcb, 0x2, 0x0,

    /* U+7EC6 "细" */
    0x20, 0x27, 0xe6, 0xbd, 0x54, 0xfa, 0xd4, 0xab,
    0xff, 0x88, 0x80,

    /* U+7ECF "经" */
    0x0, 0x27, 0xb4, 0x9c, 0xa2, 0x8a, 0xfd, 0x88,
    0xe4, 0x1f, 0x80,

    /* U+7EDD "绝" */
    0x24, 0x13, 0xc8, 0x93, 0xbe, 0x4a, 0xbb, 0xe4,
    0x89, 0xa0, 0xf, 0x80,

    /* U+7F6E "置" */
    0xff, 0xff, 0x8, 0xf7, 0x7e, 0x7e, 0x7e, 0x7e,
    0xff,

    /* U+814A "腊" */
    0x75, 0x17, 0xe5, 0x51, 0xff, 0x50, 0x1f, 0xe5,
    0xfb, 0xfe, 0x38, 0x80,

    /* U+81F3 "至" */
    0xff, 0x24, 0x7e, 0x10, 0x10, 0x7e, 0x10, 0xff,

    /* U+8282 "节" */
    0x12, 0x3f, 0xf1, 0x21, 0xfe, 0x8, 0x82, 0x20,
    0x88, 0x24, 0x8, 0x0,

    /* U+8292 "芒" */
    0x12, 0x3f, 0xf1, 0x20, 0x20, 0xff, 0xc8, 0x2,
    0x0, 0x80, 0x3f, 0x0,

    /* U+84DD "蓝" */
    0xff, 0x8a, 0x15, 0xa, 0xf5, 0xa2, 0x81, 0xfc,
    0xaa, 0xff, 0x80,

    /* U+8584 "薄" */
    0x24, 0x7e, 0xbf, 0x3f, 0xbf, 0x3f, 0x7f, 0x92,
    0x6,

    /* U+86F0 "蛰" */
    0x44, 0x7f, 0x91, 0x5d, 0xe5, 0x9b, 0x79, 0xfc,
    0x92, 0xff, 0x80,

    /* U+8981 "要" */
    0x7f, 0x8a, 0x1f, 0xea, 0x92, 0xb7, 0xfc, 0x48,
    0x18, 0x73, 0x80,

    /* U+89E3 "解" */
    0x0, 0x9, 0xe5, 0x2b, 0xee, 0x68, 0x1e, 0xc6,
    0xd1, 0xff, 0x69, 0x22, 0x40,

    /* U+8BB0 "记" */
    0x0, 0x2f, 0x80, 0x58, 0x24, 0xf2, 0x49, 0x20,
    0xd1, 0x4f, 0x0,

    /* U+8BBE "设" */
    0x4f, 0x14, 0x82, 0x5a, 0x35, 0xf2, 0x49, 0x18,
    0xcc, 0x59, 0x80,

    /* U+8BD5 "试" */
    0x2, 0x90, 0xa1, 0xfb, 0x8, 0x5e, 0x12, 0x84,
    0x91, 0xb4, 0x51, 0x80, 0x0,

    /* U+8BF7 "请" */
    0x4, 0x2f, 0x87, 0xdf, 0xf4, 0xf2, 0x79, 0x3c,
    0x92, 0xb, 0x0,

    /* U+8BFB "读" */
    0x84, 0x2f, 0x81, 0x1b, 0x64, 0xc2, 0xa1, 0x7e,
    0x8c, 0x59, 0x0,

    /* U+8C03 "调" */
    0x5f, 0xaa, 0x47, 0xba, 0x95, 0xea, 0xf5, 0xea,
    0xbd, 0x20, 0x90, 0x80,

    /* U+8C37 "谷" */
    0x13, 0x19, 0x20, 0xc0, 0x48, 0x21, 0x9f, 0xd2,
    0x10, 0xfc, 0x21, 0x0,

    /* U+8D25 "败" */
    0x2, 0x1e, 0x84, 0xbd, 0xb2, 0x6e, 0x9a, 0xa3,
    0x11, 0x2c, 0x4, 0xc0,

    /* U+8D85 "超" */
    0x27, 0xbe, 0xa2, 0x4b, 0xe4, 0x27, 0xaf, 0x2e,
    0x7a, 0xc0, 0x9f, 0x80,

    /* U+8F68 "轨" */
    0x44, 0x7a, 0x13, 0xd4, 0xaf, 0x51, 0x28, 0xe5,
    0xd2, 0x31, 0x80,

    /* U+8F7D "载" */
    0x25, 0x3e, 0x3f, 0xe4, 0x85, 0x53, 0xd8, 0x49,
    0xf7, 0x14, 0x80,

    /* U+8FD4 "返" */
    0x5f, 0x28, 0x7, 0xda, 0x25, 0x62, 0x91, 0x94,
    0xd0, 0xbf, 0x80,

    /* U+8FDE "连" */
    0x88, 0x3f, 0x95, 0x1b, 0xe4, 0x43, 0xfd, 0x10,
    0xc8, 0x9f, 0x80,

    /* U+8FF9 "迹" */
    0x0, 0x10, 0x83, 0xfc, 0x14, 0x75, 0x8d, 0x52,
    0x54, 0xa4, 0x32, 0x17, 0xf0,

    /* U+9000 "退" */
    0x5f, 0x28, 0x87, 0xdb, 0xe5, 0x52, 0x91, 0x64,
    0xe0, 0x9f, 0x80,

    /* U+9001 "送" */
    0x11, 0x25, 0x17, 0xc0, 0x8d, 0xfa, 0x21, 0x2c,
    0xa0, 0xbf, 0x80,

    /* U+91CD "重" */
    0x6, 0x3c, 0x3f, 0xef, 0xc7, 0xe2, 0x50, 0xf0,
    0xfe, 0xff, 0x80,

    /* U+9501 "锁" */
    0x22, 0xd, 0xa4, 0x21, 0xde, 0x26, 0x9d, 0xa2,
    0x68, 0xcc, 0x2c, 0x80,

    /* U+95F0 "闰" */
    0x40, 0xf, 0xbf, 0x89, 0xbf, 0x89, 0xbf, 0x81,
    0x81,

    /* U+95F2 "闲" */
    0x40, 0x5f, 0x1, 0x89, 0xbf, 0x99, 0x9d, 0xab,
    0x89, 0x83,

    /* U+95F4 "间" */
    0x40, 0x5f, 0x1, 0x81, 0xbd, 0xbd, 0xa5, 0xbd,
    0x81, 0x83,

    /* U+9634 "阴" */
    0xef, 0xa9, 0xa9, 0xcf, 0xc9, 0xaf, 0xa9, 0x91,
    0xa3,

    /* U+9635 "阵" */
    0x0, 0xe8, 0xdf, 0xc8, 0xd4, 0xbf, 0xa4, 0xdf,
    0x84, 0x84,

    /* U+964D "降" */
    0x0, 0x39, 0xa, 0xf3, 0x58, 0xad, 0xac, 0x8f,
    0xf2, 0x7e, 0x82, 0x20, 0x80,

    /* U+9664 "除" */
    0x4, 0x72, 0x2a, 0x9a, 0x2d, 0xed, 0x22, 0xfd,
    0x48, 0x95, 0x46, 0x0,

    /* U+96E8 "雨" */
    0xff, 0x84, 0x1f, 0xe9, 0x15, 0xaa, 0x45, 0x6a,
    0x91, 0x41, 0x80,

    /* U+96EA "雪" */
    0x7f, 0x4, 0x3f, 0xfd, 0xd6, 0xe3, 0xf8, 0x4,
    0x7e, 0x7f, 0x0,

    /* U+96F7 "雷" */
    0x7f, 0x7f, 0xea, 0xe5, 0x60, 0x83, 0xf9, 0xfc,
    0x92, 0x7f, 0x0,

    /* U+96F9 "雹" */
    0x7f, 0x7f, 0xfa, 0xad, 0x43, 0xf2, 0xb, 0xf4,
    0xfc, 0x3f, 0x80,

    /* U+96FE "雾" */
    0x7f, 0x7f, 0xfb, 0xad, 0xe3, 0xe2, 0xa3, 0xfe,
    0xfc, 0x12, 0x30, 0x0,

    /* U+9700 "需" */
    0x7f, 0x7f, 0xfb, 0xad, 0xc0, 0x3, 0xf9, 0xfd,
    0x2a, 0x15, 0x0,

    /* U+971C "霜" */
    0x7f, 0xff, 0xee, 0x6e, 0x4f, 0xef, 0x69, 0xcf,
    0xcf,

    /* U+9732 "露" */
    0x7f, 0x7f, 0xff, 0xaf, 0xc7, 0x73, 0xd0, 0xb6,
    0x56, 0xf7, 0x0,

    /* U+973E "霾" */
    0x3e, 0xff, 0xea, 0x6a, 0xad, 0xef, 0x64, 0x5f,
    0xaf,

    /* U+989D "额" */
    0x27, 0xfd, 0x22, 0x8f, 0xee, 0xb2, 0xdb, 0xc8,
    0xea, 0x5c, 0x80,

    /* U+98CE "风" */
    0x7f, 0x10, 0x44, 0x51, 0x54, 0x49, 0x13, 0x45,
    0x52, 0x5, 0x80, 0x80,

    /* U+9F99 "龙" */
    0x0, 0x2, 0x80, 0x91, 0xfe, 0xc, 0x5, 0x41,
    0x50, 0xd8, 0x6c, 0x55, 0xe0,

    /* U+FF01 "！" */
    0xf2,

    /* U+FF08 "（" */
    0x2a, 0x49, 0x13,

    /* U+FF09 "）" */
    0x88, 0x92, 0x54,

    /* U+FF0C "，" */
    0xd0,

    /* U+FF1A "：" */
    0x90,

    /* U+FF1B "；" */
    0x98,

    /* U+FF1F "？" */
    0xe9, 0x22, 0x40, 0x40
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 80, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 80, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 3, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 8, .adv_w = 80, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 13, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 80, .box_w = 1, .box_h = 2, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 23, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 27, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 31, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 35, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 39, .adv_w = 80, .box_w = 1, .box_h = 2, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 40, .adv_w = 80, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 41, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 42, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 52, .adv_w = 80, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 58, .adv_w = 80, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 71, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 75, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 80, .box_w = 1, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 80, .box_w = 1, .box_h = 6, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 89, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 80, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 96, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 101, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 105, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 115, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 135, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 80, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 180, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 193, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 80, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 80, .box_w = 3, .box_h = 10, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 221, .adv_w = 80, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 225, .adv_w = 80, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 229, .adv_w = 80, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 230, .adv_w = 80, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 231, .adv_w = 80, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 232, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 235, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 249, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 253, .adv_w = 80, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 257, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 261, .adv_w = 80, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 80, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 265, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 80, .box_w = 1, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 283, .adv_w = 80, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 286, .adv_w = 80, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 288, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 295, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 80, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 310, .adv_w = 80, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 314, .adv_w = 80, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 317, .adv_w = 80, .box_w = 3, .box_h = 10, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 321, .adv_w = 80, .box_w = 1, .box_h = 11, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 323, .adv_w = 80, .box_w = 3, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 327, .adv_w = 80, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 328, .adv_w = 160, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 329, .adv_w = 160, .box_w = 9, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 331, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 342, .adv_w = 160, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 350, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 361, .adv_w = 160, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 370, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 381, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 392, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 403, .adv_w = 160, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 411, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 422, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 433, .adv_w = 160, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 452, .adv_w = 160, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 459, .adv_w = 160, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 472, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 483, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 495, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 506, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 517, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 529, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 540, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 551, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 562, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 574, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 585, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 597, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 608, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 617, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 629, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 642, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 654, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 665, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 678, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 689, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 699, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 710, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 721, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 732, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 743, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 754, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 765, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 774, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 785, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 796, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 808, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 819, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 830, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 841, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 852, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 864, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 873, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 884, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 895, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 906, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 915, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 924, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 933, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 942, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 954, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 966, .adv_w = 160, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 979, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 990, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 999, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1010, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1021, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1030, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1041, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1050, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1061, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1072, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1083, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1094, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1107, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1118, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1129, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1141, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1152, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1161, .adv_w = 160, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1169, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1178, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1190, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1202, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1214, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1225, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1238, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1249, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1260, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1271, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1283, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1294, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1306, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1318, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1327, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1338, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1350, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1361, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1373, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1386, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1397, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1409, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1421, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1432, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1443, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1454, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1465, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1476, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1485, .adv_w = 160, .box_w = 7, .box_h = 9, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 1493, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1504, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1515, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1524, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1533, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1544, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1553, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1564, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1575, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1584, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1595, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1607, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1618, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1629, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1641, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1653, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1665, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1678, .adv_w = 160, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1687, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1698, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1708, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1720, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1731, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1742, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1753, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1764, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1776, .adv_w = 160, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1790, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1802, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1811, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1823, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1834, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1845, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1857, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1868, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1880, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1891, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1903, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 1915, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1924, .adv_w = 160, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1933, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1945, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1957, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1969, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 1980, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 1991, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2002, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2011, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2023, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2035, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2046, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2057, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2069, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2080, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2091, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2103, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2112, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2124, .adv_w = 160, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2132, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2144, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2156, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2167, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2176, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2187, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2198, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2211, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2222, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2233, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 2246, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2257, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2268, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 2280, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2292, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2304, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2316, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2327, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2338, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2349, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2360, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 2373, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2384, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2395, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2406, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2418, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2427, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2437, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2447, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2456, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2466, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2479, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2491, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2502, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2513, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2524, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2535, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 2547, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2558, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2567, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2578, .adv_w = 160, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2587, .adv_w = 160, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 2598, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2610, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 2623, .adv_w = 160, .box_w = 1, .box_h = 7, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 2624, .adv_w = 160, .box_w = 3, .box_h = 8, .ofs_x = 6, .ofs_y = 0},
    {.bitmap_index = 2627, .adv_w = 160, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2630, .adv_w = 160, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 2631, .adv_w = 160, .box_w = 1, .box_h = 4, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 2632, .adv_w = 160, .box_w = 1, .box_h = 6, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 2633, .adv_w = 160, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x1dfe, 0x1e01, 0x1e07, 0x1e08, 0x1e09, 0x1e0b, 0x1e11,
    0x1e23, 0x1e2b, 0x1e39, 0x1e5b, 0x1e8a, 0x1e8f, 0x1e92, 0x1eaa,
    0x1ec8, 0x1ee2, 0x1eff, 0x1f0f, 0x1f1e, 0x1f32, 0x1f4b, 0x1f57,
    0x1f89, 0x2141, 0x2169, 0x216b, 0x2183, 0x218b, 0x219a, 0x21aa,
    0x21ae, 0x21b5, 0x21b9, 0x21c4, 0x2204, 0x221b, 0x222e, 0x2267,
    0x229f, 0x22a6, 0x22b1, 0x2315, 0x233f, 0x2346, 0x2375, 0x2384,
    0x23cf, 0x23d4, 0x23ed, 0x240a, 0x2466, 0x247b, 0x24cb, 0x2666,
    0x26d9, 0x26dc, 0x26fb, 0x2726, 0x2902, 0x2905, 0x290d, 0x2918,
    0x2925, 0x2927, 0x292f, 0x2937, 0x2980, 0x2b56, 0x2b8a, 0x2b9a,
    0x2ba2, 0x2bd0, 0x2c0d, 0x2c0f, 0x2c16, 0x2c4d, 0x2ddc, 0x2de3,
    0x2df0, 0x2e72, 0x2e84, 0x2e92, 0x2ea4, 0x2ef4, 0x2efd, 0x2f38,
    0x2f53, 0x2f83, 0x2f95, 0x2fac, 0x2fca, 0x2fff, 0x30c8, 0x320e,
    0x3269, 0x326a, 0x32d0, 0x32d4, 0x3307, 0x336c, 0x33a3, 0x33cd,
    0x341a, 0x356e, 0x35ab, 0x35de, 0x35e3, 0x35e4, 0x35f4, 0x360c,
    0x3623, 0x3672, 0x3680, 0x368f, 0x36b2, 0x3706, 0x3707, 0x370b,
    0x371d, 0x3728, 0x375f, 0x376b, 0x377f, 0x3b1f, 0x3b61, 0x3b63,
    0x3bcd, 0x3bd9, 0x3c12, 0x3c32, 0x3c97, 0x3ce6, 0x3d51, 0x3d6c,
    0x3d86, 0x3e03, 0x3e27, 0x3e7d, 0x3edf, 0x40eb, 0x4257, 0x4277,
    0x42b4, 0x43ae, 0x43eb, 0x4526, 0x467b, 0x47e3, 0x47eb, 0x49c9,
    0x49cb, 0x4a78, 0x4ac9, 0x4aed, 0x4b47, 0x4b7c, 0x4c71, 0x4d20,
    0x4eaa, 0x4ec4, 0x4ecd, 0x4edb, 0x4f6c, 0x5148, 0x51f1, 0x5280,
    0x5290, 0x54db, 0x5582, 0x56ee, 0x597f, 0x59e1, 0x5bae, 0x5bbc,
    0x5bd3, 0x5bf5, 0x5bf9, 0x5c01, 0x5c35, 0x5d23, 0x5d83, 0x5f66,
    0x5f7b, 0x5fd2, 0x5fdc, 0x5ff7, 0x5ffe, 0x5fff, 0x61cb, 0x64ff,
    0x65ee, 0x65f0, 0x65f2, 0x6632, 0x6633, 0x664b, 0x6662, 0x66e6,
    0x66e8, 0x66f5, 0x66f7, 0x66fc, 0x66fe, 0x671a, 0x6730, 0x673c,
    0x689b, 0x68cc, 0x6f97, 0xceff, 0xcf06, 0xcf07, 0xcf0a, 0xcf18,
    0xcf19, 0xcf1d
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 12290, .range_length = 53022, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 218, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_zh_10 = {
#else
lv_font_t lv_font_zh_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_ZH_10*/

