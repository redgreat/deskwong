#include "ai_usage_screen.h"
#include "ai_provider_icons.h"
#include "ui_fonts.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static lv_obj_t *s_panel;
static ai_provider_t s_windows[4];
static int s_count;
static bool s_visible;

static void rect(lv_draw_ctx_t *ctx, int x, int y, int w, int h, bool black, int border) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=black?lv_color_black():lv_color_white(); d.bg_opa=LV_OPA_COVER;
    d.border_color=lv_color_black(); d.border_opa=border?LV_OPA_COVER:LV_OPA_TRANSP; d.border_width=border;
    lv_area_t a={(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w-1),(lv_coord_t)(y+h-1)}; lv_draw_rect(ctx,&d,&a);
}
static void label(lv_draw_ctx_t *ctx,const char *v,const lv_font_t *font,int x,int y,int w,lv_text_align_t align,bool white) {
    lv_draw_label_dsc_t d; lv_draw_label_dsc_init(&d); d.font=font;d.color=white?lv_color_white():lv_color_black();d.align=align;
    lv_area_t a={(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w-1),(lv_coord_t)(y+font->line_height-1)};lv_draw_label(ctx,&d,&a,v,NULL);
}
static void icon(lv_draw_ctx_t *ctx,const char *id,int size,int x,int y) {
    lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.recolor=lv_color_black();d.recolor_opa=LV_OPA_COVER;
    lv_area_t a={(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+size-1),(lv_coord_t)(y+size-1)};lv_draw_img(ctx,&d,&a,ai_provider_icon(id,size));
}
static void fmt_time(char *out,size_t size,int64_t value,bool date_if_old) {
    if(value<=0){snprintf(out,size,"--");return;} time_t t=(time_t)value,now_t=time(NULL);struct tm local={},now={};
#ifdef _WIN32
    bool ok=localtime_s(&local,&t)==0;localtime_s(&now,&now_t);
#else
    bool ok=localtime_r(&t,&local)!=NULL;localtime_r(&now_t,&now);
#endif
    if(!ok){snprintf(out,size,"--");return;}strftime(out,size,date_if_old&&(local.tm_year!=now.tm_year||local.tm_yday!=now.tm_yday)?"%m/%d":"%H:%M",&local);
}
static void panel_draw(lv_event_t *e) {
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(e);lv_area_t area;lv_obj_get_coords(lv_event_get_target(e),&area);int ox=area.x1,oy=area.y1;
    rect(ctx,ox,oy,380,30,true,0);label(ctx,"AI 用量",&lv_font_zh_14,ox+12,oy+7,100,LV_TEXT_ALIGN_LEFT,true);
    if(s_count<=0){label(ctx,"暂无数据",&lv_font_zh_10,ox+250,oy+9,116,LV_TEXT_ALIGN_RIGHT,true);return;}
    bool stale=false,unavailable=false;int64_t observed=0;const char *ids[2]={NULL,NULL};int idx[2][2]={{-1,-1},{-1,-1}},per[2]={0,0},apps=0;
    for(int i=0;i<s_count;++i){const char *id=s_windows[i].app_id[0]?s_windows[i].app_id:"chatgpt";int app=-1;for(int j=0;j<apps;++j)if(!strcmp(ids[j],id))app=j;if(app<0){if(apps>=2)continue;app=apps++;ids[app]=id;}if(per[app]<2)idx[app][per[app]++]=i;stale=stale||!strcmp(s_windows[i].app_status,"stale")||!strcmp(s_windows[i].app_status,"cached");unavailable=unavailable||!strcmp(s_windows[i].app_status,"unavailable");if(s_windows[i].observed_at>observed)observed=s_windows[i].observed_at;}
    char stamp[16],status[32];fmt_time(stamp,sizeof(stamp),observed,false);snprintf(status,sizeof(status),"%s%s%s",unavailable?"部分不可用":stale?"缓存":"更新",observed>0?" ":"",observed>0?stamp:"");label(ctx,status,&lv_font_zh_10,ox+220,oy+9,146,LV_TEXT_ALIGN_RIGHT,true);
    for(int app=0;app<apps;++app){bool single=apps==1;int x=ox+(single?18:8+app*184),w=single?344:180;rect(ctx,x,oy+38,w,218,false,1);int is=single?32:24,first=idx[app][0];icon(ctx,ids[app],is,x+(single?78:12),oy+(single?46:50));const char *name=first>=0&&s_windows[first].app_name[0]?s_windows[first].app_name:ids[app];label(ctx,name,&lv_font_zh_20,x+(single?122:48),oy+49,w-(single?132:56),LV_TEXT_ALIGN_LEFT,false);
        for(int win=0;win<2;++win){int n=idx[app][win],y=oy+98+win*76,inner=w-16;const ai_provider_t *item=n>=0?&s_windows[n]:NULL;label(ctx,item&&item->label[0]?item->label:"--",&lv_font_zh_14,x+8,y,54,LV_TEXT_ALIGN_LEFT,false);char pct[16],reset[32],rt[16];if(item&&item->remaining_percent>=0)snprintf(pct,sizeof(pct),"%.0f%%",item->remaining_percent);else strcpy(pct,"--%");label(ctx,pct,&lv_font_digits_28,x+62,y-8,w-78,LV_TEXT_ALIGN_RIGHT,false);fmt_time(rt,sizeof(rt),item?item->resets_at:0,true);snprintf(reset,sizeof(reset),"%s 重置",rt);label(ctx,reset,&lv_font_zh_10,x+8,y+28,inner,LV_TEXT_ALIGN_RIGHT,false);rect(ctx,x+8,y+44,inner,6,false,1);float rem=item?item->remaining_percent:0;if(!isfinite(rem)||rem<0)rem=0;if(rem>100)rem=100;int fill=(int)(rem*inner/100.0f+0.5f);if(fill>0)rect(ctx,x+8,y+44,fill,6,true,0);}
    }
}
void ai_usage_screen_init(int width,int height){(void)width;(void)height;lv_obj_t *root=lv_scr_act();s_panel=lv_obj_create(root);lv_obj_remove_style_all(s_panel);lv_obj_set_pos(s_panel,10,18);lv_obj_set_size(s_panel,380,264);lv_obj_set_style_bg_color(s_panel,lv_color_white(),0);lv_obj_set_style_bg_opa(s_panel,LV_OPA_COVER,0);lv_obj_set_style_border_color(s_panel,lv_color_black(),0);lv_obj_set_style_border_width(s_panel,2,0);lv_obj_set_style_radius(s_panel,7,0);lv_obj_clear_flag(s_panel,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(s_panel,panel_draw,LV_EVENT_DRAW_MAIN,NULL);ai_usage_screen_hide();}
void ai_usage_screen_update(const ai_provider_t *w,int count){s_count=count<0?0:count>4?4:count;if(s_count&&w)memcpy(s_windows,w,sizeof(s_windows[0])*s_count);if(s_panel)lv_obj_invalidate(s_panel);}
void ai_usage_screen_show(void){lv_obj_clear_flag(s_panel,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(s_panel);s_visible=true;}
void ai_usage_screen_hide(void){if(s_panel)lv_obj_add_flag(s_panel,LV_OBJ_FLAG_HIDDEN);s_visible=false;}
bool ai_usage_screen_visible(void){return s_visible;}
