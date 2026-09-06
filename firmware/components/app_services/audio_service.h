#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化音频（ES8311 播放通路），失败静默降级 */
void audio_service_init(void);
/* 播放一声简短提示音（1kHz 短 beep） */
void audio_service_beep(void);
/* 设置扬声器音量 0-100 */
void audio_service_set_volume(int vol);

#ifdef __cplusplus
}
#endif
