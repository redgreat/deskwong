#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化音频（ES8311 播放通路），失败静默降级 */
void audio_service_init(void);
/* 播放一声简短提示音（1kHz 短 beep） */
void audio_service_beep(void);
/* 开机音（两声上行提示） */
void audio_service_boot_sound(void);
/* 关机音（下行提示），重启用 */
void audio_service_poweroff_sound(void);

/* 业务流程阶段提示音：不同音高组合，不听屏幕也能分辨结果 */
typedef enum {
    AUDIO_CUE_START = 0,       /* 开始下载 */
    AUDIO_CUE_DEVICE_FOUND,    /* 搜到设备 */
    AUDIO_CUE_DOWNLOAD_DONE,   /* 下载完成 */
    AUDIO_CUE_UPLOAD_DONE,     /* 上传完成 */
    AUDIO_CUE_ERROR,           /* 下载 / 上传出错 */
    AUDIO_CUE_NO_DATA,         /* 设备无定位数据 */
    AUDIO_CUE_CANCEL,          /* 手动中断 */
    AUDIO_CUE_KEY,             /* 统一按键反馈音（无固定功能的按键操作） */
    AUDIO_CUE_KEY_TOGGLE,      /* 同步中短按：收起 / 展开弹窗 */
} audio_cue_t;
void audio_service_cue(audio_cue_t cue);
/* 设置扬声器音量 0-100 */
void audio_service_set_volume(int vol);
/* 供语音服务复用的编解码句柄；音频未就绪时返回 NULL。
 * 返回类型是 esp_codec_dev_handle_t，为避免头文件扩散这里用 void*。 */
void *audio_service_playback_handle(void);
void *audio_service_record_handle(void);

#ifdef __cplusplus
}
#endif
