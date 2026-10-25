/* AI 用量独立页：RaceBox 同步卡片同款窗体（白卡 + 黑色偏移衬底 + 黑色标题条），
 * 2x2 四张小卡把四个应用都列出来，没数据的先空着。
 * 整页只占一个 LVGL 对象，其余全部在 LV_EVENT_DRAW_MAIN 里画，
 * 避免对象池（128KB）被十几个标签顶爆。 */
#include "ai_usage_screen.h"
#include "ai_provider_icons.h"
#include "ui_fonts.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 固定四个应用目录，与服务端 aiCatalog / 设备端四选二一致 */
static const char *k_app_ids[4] = {"chatgpt", "devin", "claude", "mimo"};
#define AI_PAGE_APP_COUNT 4
#define AI_PAGE_CARD_W    168
#define AI_PAGE_CARD_H    90
#define AI_PAGE_PANEL_W   364
#define AI_PAGE_PANEL_H   224

static lv_obj_t *s_panel;
static ai_provider_t s_windows[AI_MAX_PROVIDERS];
static int s_count;
static bool s_visible;

static void rect(lv_draw_ctx_t *ctx, int x, int y, int w, int h, bool black, int border, int radius) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color=black?lv_color_black():lv_color_white(); d.bg_opa=LV_OPA_COVER;
    d.border_color=lv_color_black(); d.border_opa=border?LV_OPA_COVER:LV_OPA_TRANSP; d.border_width=border;
    d.radius=(lv_coord_t)radius;
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
/* 重置时间必须具体到几点几分：当天只显示 HH:MM，跨天补上 MM/DD */
static void fmt_reset(char *out,size_t size,int64_t value) {
    if(value<=0){snprintf(out,size,"--");return;}
    time_t t=(time_t)value,now_t=time(NULL);struct tm local={},now={};
#ifdef _WIN32
    bool ok=localtime_s(&local,&t)==0;localtime_s(&now,&now_t);
#else
    bool ok=localtime_r(&t,&local)!=NULL;localtime_r(&now_t,&now);
#endif
    if(!ok){snprintf(out,size,"--");return;}
    bool today=local.tm_year==now.tm_year&&local.tm_yday==now.tm_yday;
    strftime(out,size,today?"%H:%M":"%m/%d %H:%M",&local);
}
/* 第 app 个应用的第 win 条窗口；该应用没数据时返回 NULL（卡片留空）。
 * 只认带窗口 id 的条目：服务端可能返回只有状态没有窗口的应用占位。 */
static const ai_provider_t *find_window(const char *app_id,int win) {
    int seen=0;
    for(int i=0;i<s_count;++i){
        const char *id=s_windows[i].app_id[0]?s_windows[i].app_id:"chatgpt";
        if(strcmp(id,app_id)||!s_windows[i].id[0])continue;
        if(seen++==win)return &s_windows[i];
    }
    return NULL;
}
static void draw_bar(lv_draw_ctx_t *ctx,int x,int y,int w,int h,float percent) {
    rect(ctx,x,y,w,h,false,1,2);
    if(!isfinite(percent)||percent<0)percent=0;
    if(percent>100)percent=100;
    int fill=(int)(percent*(w-2)/100.0f+0.5f);
    if(fill>0)rect(ctx,x+1,y+1,fill,h-2,true,0,1);
}
static void panel_draw(lv_event_t *e) {
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(e);lv_area_t area;lv_obj_get_coords(lv_event_get_target(e),&area);
    int ox=area.x1,oy=area.y1;
    /* RaceBox 卡片：右下偏移 4px 的黑色衬底当阴影，上层白卡 2px 黑边圆角 */
    rect(ctx,ox+4,oy+4,AI_PAGE_PANEL_W,AI_PAGE_PANEL_H,true,0,12);
    rect(ctx,ox,oy,AI_PAGE_PANEL_W,AI_PAGE_PANEL_H,false,2,10);
    /* 黑色标题条：先画圆角矩形，再把下半截补成方形，避免两个圆角叠出缺口 */
    rect(ctx,ox,oy,AI_PAGE_PANEL_W,20,true,0,10);
    rect(ctx,ox,oy+10,AI_PAGE_PANEL_W,10,true,0,0);
    label(ctx,"AI 用量",&lv_font_zh_14,ox,oy+3,AI_PAGE_PANEL_W,LV_TEXT_ALIGN_CENTER,true);

    int64_t observed=0;bool stale=false,unavailable=false;
    for(int i=0;i<s_count;++i){
        if(s_windows[i].observed_at>observed)observed=s_windows[i].observed_at;
        stale=stale||!strcmp(s_windows[i].app_status,"stale")||!strcmp(s_windows[i].app_status,"cached");
        unavailable=unavailable||!strcmp(s_windows[i].app_status,"unavailable");
    }
    char stamp[16],status[40];
    fmt_reset(stamp,sizeof(stamp),observed);
    snprintf(status,sizeof(status),"%s%s",observed>0?(unavailable?"部分不可用 ":stale?"缓存 ":"更新 "):"暂无数据",observed>0?stamp:"");
    label(ctx,status,&lv_font_zh_10,ox+180,oy+5,AI_PAGE_PANEL_W-186,LV_TEXT_ALIGN_RIGHT,true);

    for(int app=0;app<AI_PAGE_APP_COUNT;++app){
        int x=ox+10+(app%2)*(AI_PAGE_CARD_W+8),y=oy+28+(app/2)*(AI_PAGE_CARD_H+8);
        rect(ctx,x,y,AI_PAGE_CARD_W,AI_PAGE_CARD_H,false,1,6);
        const ai_provider_t *first=find_window(k_app_ids[app],0);
        icon(ctx,k_app_ids[app],16,x+8,y+7);
        const char *name=first&&first->app_name[0]?first->app_name:k_app_ids[app];
        label(ctx,name,&lv_font_zh_14,x+28,y+5,AI_PAGE_CARD_W-36,LV_TEXT_ALIGN_LEFT,false);
        for(int win=0;win<2;++win){
            const ai_provider_t *item=find_window(k_app_ids[app],win);
            int ry=y+29+win*28;
            label(ctx,item&&item->label[0]?item->label:"--",&lv_font_zh_10,x+8,ry+4,30,LV_TEXT_ALIGN_LEFT,false);
            char reset[16];fmt_reset(reset,sizeof(reset),item?item->resets_at:0);
            label(ctx,reset,&lv_font_zh_10,x+40,ry+4,68,LV_TEXT_ALIGN_LEFT,false);
            char pct[16];
            if(item&&item->remaining_percent>=0)snprintf(pct,sizeof(pct),"%.0f%%",item->remaining_percent);
            else strcpy(pct,"--%");
            label(ctx,pct,&lv_font_digits_18,x+106,ry,54,LV_TEXT_ALIGN_RIGHT,false);
            draw_bar(ctx,x+8,ry+23,AI_PAGE_CARD_W-16,4,item?item->remaining_percent:-1);
        }
    }
}
void ai_usage_screen_init(int width,int height){(void)width;(void)height;
    lv_obj_t *root=lv_scr_act();
    /* 对象比白卡大 4px，右下那 4px 用来画偏移衬底 */
    s_panel=lv_obj_create(root);lv_obj_remove_style_all(s_panel);
    lv_obj_set_pos(s_panel,16,30);
    lv_obj_set_size(s_panel,AI_PAGE_PANEL_W+4,AI_PAGE_PANEL_H+4);
    lv_obj_clear_flag(s_panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_panel,panel_draw,LV_EVENT_DRAW_MAIN,NULL);
    ai_usage_screen_hide();
}
void ai_usage_screen_update(const ai_provider_t *w,int count){
    s_count=count<0?0:count>AI_MAX_PROVIDERS?AI_MAX_PROVIDERS:count;
    if(s_count&&w)memcpy(s_windows,w,sizeof(s_windows[0])*s_count);
    if(s_panel)lv_obj_invalidate(s_panel);
}
void ai_usage_screen_show(void){lv_obj_clear_flag(s_panel,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(s_panel);s_visible=true;}
void ai_usage_screen_hide(void){if(s_panel)lv_obj_add_flag(s_panel,LV_OBJ_FLAG_HIDDEN);s_visible=false;}
bool ai_usage_screen_visible(void){return s_visible;}
