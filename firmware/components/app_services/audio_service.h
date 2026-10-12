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
    AUDIO_CUE_SIGNIN,          /* 企微签到提醒铃声：上行三音 */
    AUDIO_CUE_SIGNOUT,         /* 企微签退提醒铃声：下行三音 */
    AUDIO_CUE_WORKTIME,        /* 记录工时提醒铃声：双音重复 */
} audio_cue_t;
void audio_service_cue(audio_cue_t cue);
/* 提醒铃声音量：取后台配置的音量（voice_volume，0..100），默认 60。
 * 与系统提示音基准分离，保存配置后热更新；语音会话结束后仍恢复系统/小智上下文。 */
void audio_service_set_ring_volume(int vol);
/* 普通操作音量：系统提示音（按键音、RaceBox 状态音等）基准，0..100 默认 60 */
void audio_service_set_system_volume(int vol);
/* 小智下行语音播放上下文。只改变 TTS 音量；系统提示音仍使用原有音量。
 * begin/end 均幂等，所有语音异常退出路径也必须调用 end。 */
void audio_service_voice_begin(int vol);
void audio_service_voice_end(void);
/* 供语音服务复用的编解码句柄；音频未就绪时返回 NULL。
 * 返回类型是 esp_codec_dev_handle_t，为避免头文件扩散这里用 void*。 */
void *audio_service_playback_handle(void);
void *audio_service_record_handle(void);

#ifdef __cplusplus
}
#endif
