#include "lvgl.h"
#include "main_screen.h"
#include "sync_screen.h"
#include "calendar_service.h"
#include "ui_fonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <math.h>
static unsigned char pixels[400*300];
static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *colors) {
    for (int y=a->y1;y<=a->y2;++y) for (int x=a->x1;x<=a->x2;++x)
        pixels[y*400+x] = (colors++)->full < 0x7fff ? 0 : 255;
    lv_disp_flush_ready(drv);
}
static void save(const char *path) {
    lv_refr_now(NULL);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P5\n400 300\n255\n"); fwrite(pixels,1,sizeof(pixels),f); fclose(f);
}
int main(int argc,char **argv) {
    _putenv_s("TZ", "CST-8"); _tzset();
    lv_init();
    static lv_color_t buffer[400*300];
    static lv_disp_draw_buf_t db; lv_disp_draw_buf_init(&db,buffer,NULL,400*300);
    static lv_disp_drv_t drv; lv_disp_drv_init(&drv);
    drv.hor_res=400; drv.ver_res=300; drv.draw_buf=&db; drv.flush_cb=flush; lv_disp_drv_register(&drv);
    // Regression: generated Chinese glyphs must render with font decompression disabled.
    lv_font_glyph_dsc_t glyph;
    assert(lv_font_get_glyph_dsc(&lv_font_zh_14,&glyph,0x5e74,0));
    assert(lv_font_get_glyph_bitmap(&lv_font_zh_14,0x5e74));
    const uint32_t sync_glyphs[] = {0x6570,0x636e,0x540c,0x6b65,0x8bbe,0x5907,0x641c,0x7d22,0x84dd,0x7259,0x8fd4,0x56de};
    for (uint32_t ch : sync_glyphs) {
        assert(lv_font_get_glyph_dsc(&lv_font_zh_14, &glyph, ch, 0));
        assert(lv_font_get_glyph_bitmap(&lv_font_zh_14, ch));
    }
    main_screen_init(400,300);
    int month=argc>2?atoi(argv[2]):9;
    bool empty=argc>3 && strcmp(argv[3], "empty") == 0;
    char date[16]; snprintf(date,sizeof(date),"2026-%02d-11",month);
    lunar_date_t today; calendar_solar_to_lunar(2026,month,11,&today);
    char lunar[32]; calendar_lunar_str(&today,lunar,sizeof(lunar));
    main_screen_update_time(date,"09:41:26",NULL,lunar);
    main_screen_update_status(empty?"AP | 0%":"WiFi | 46%");
    main_screen_update_net(!empty, empty ? "192.168.4.1" : "20.20.10.114");
    main_screen_set_reminder_enabled(!empty);
    calendar_cell_t cells[42]={};
    int offset=(calendar_weekday(2026,month,1)+6)%7;
    for(int day=1;day<=calendar_month_days(2026,month);++day) {
        calendar_cell_t *c=&cells[offset+day-1]; c->day=day; c->is_today=day==11;
        int wd=calendar_weekday(2026,month,day); c->type=wd==0||wd==6?CAL_WEEKEND:CAL_NORMAL;
        lunar_date_t ld; calendar_solar_to_lunar(2026,month,day,&ld);
        calendar_lunar_day_str(&ld,c->lunar,sizeof(c->lunar));
        c->work_hours=empty||c->type==CAL_WEEKEND?0:(day%8)+1;
    }
    main_screen_update_calendar(cells);
    ai_provider_t five = {}, weekly = {};
    five.remaining_percent = 72; weekly.remaining_percent = 44;
    struct tm reset = {}; reset.tm_year=126; reset.tm_mon=8; reset.tm_mday=12; reset.tm_hour=18; reset.tm_min=30;
    five.resets_at = mktime(&reset); reset.tm_mday=18; weekly.resets_at=mktime(&reset);
    main_screen_update_summary(empty?0:64,empty?0:72,empty?NULL:"晴\n26C / 58%",empty?0:argc>4?atoi(argv[4]):100,lunar,empty?NULL:&five,empty?NULL:&weekly,!empty,empty?0:12800,empty?NAN:24.0f,empty?NAN:52.0f);
    sync_screen_init(400,300);
    if (argc>3 && strcmp(argv[3],"sync")==0) {
        racebox_progress_t p={};p.state=RACEBOX_FAILED;p.received=750;p.total=753;p.download_done=true;p.percent=0;p.elapsed_seconds=23;
        strcpy(p.device,"RaceBox Mini S 2254300997");strcpy(p.message,"下载完成，MQTT 未连接");
        sync_screen_update(&p);sync_screen_show();
    }
    save(argc>1?argv[1]:"preview.pgm");
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    printf("UI heap used: %u / %u bytes\n",(unsigned)(m.total_size-m.free_size),(unsigned)m.total_size);
    assert(m.free_size>16384);
    return 0;
}
