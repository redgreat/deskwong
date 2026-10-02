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
/* 供语音服务复用的编解码句柄；音频未就绪时返回 NULL。
 * 返回类型是 esp_codec_dev_handle_t，为避免头文件扩散这里用 void*。 */
void *audio_service_playback_handle(void);
void *audio_service_record_handle(void);

#ifdef __cplusplus
}
#endif
