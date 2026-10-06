/*
 * voice_screen.h —— 小智语音对话弹窗（屏幕中下方）
 *
 * 样式对齐天气黄历/同步弹窗：黑色阴影 + 白底黑框面板 + 黑色标题条。
 * 标题条右侧显示会话状态（连接中/聆听中/播放中），
 * 正文两行：问（ASR 识别文本）/ AI（TTS 回复文本），对话字体 GB2312 一级字库。
 */
#ifndef VOICE_SCREEN_H
#define VOICE_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void voice_screen_init(int width, int height);
/* 状态词（连接中/聆听中/播放中），NULL=不变 */
void voice_screen_set_status(const char *status);
/* 问行：ASR 识别文本；答行：TTS 回复文本。NULL 或空串=不变 */
void voice_screen_set_question(const char *text);
void voice_screen_set_answer(const char *text);
void voice_screen_show(void);
void voice_screen_hide(void);
bool voice_screen_visible(void);

#ifdef __cplusplus
}
#endif
#endif /* VOICE_SCREEN_H */
