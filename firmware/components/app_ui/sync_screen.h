#pragma once
#include <stdbool.h>
#include "racebox_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RaceBox 同步弹窗：按键触发后覆盖在主屏之上，展示
 * 蓝牙搜索 → 连接 → 下载进度 → 上传 → 擦除 → 完成/失败。
 * 所有接口都要求在 LVGL 锁内调用。 */
void sync_screen_init(int width, int height);
void sync_screen_show(void);
void sync_screen_hide(void);
bool sync_screen_visible(void);
void sync_screen_update(const racebox_progress_t *p);

#ifdef __cplusplus
}
#endif
