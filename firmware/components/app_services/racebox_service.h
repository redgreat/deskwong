#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RaceBox 单条记录（字段对齐 lc_racebox 表，见 doc/SPECIFICATIONS.md） */
typedef struct {
    int itow;
    int year, month, day, hour, minute, second;
    int time_accuracy, nanoseconds;
    int fix_status, numberof_svs;
    double longitude, latitude;
    double wgs_altitude, msl_altitude;
    double horizontal_accuracy, vertical_accuracy;
    double speed, heading;
    int speed_accuracy, heading_accuracy, pdop;
    double gforce_x, gforce_y, gforce_z;
    double rotation_rate_x, rotation_rate_y, rotation_rate_z;
} racebox_record_t;

typedef enum {
    RACEBOX_IDLE = 0,
    RACEBOX_SCANNING,
    RACEBOX_CONNECTING,
    RACEBOX_QUERYING,
    RACEBOX_DOWNLOADING,
    RACEBOX_UPLOADING,
    RACEBOX_ERASING,
    RACEBOX_DONE,
    RACEBOX_FAILED,
} racebox_state_t;

/* 同步进度：供 UI 显示（弹窗 / 同步屏） */
typedef struct {
    racebox_state_t state;
    int total;          // 总条数，0 表示未知
    int received;       // 已接收 / 已上传条数
    int percent;        // 0..100，<0 表示进度不确定
    int uploaded;       // MQTT PUBACK-confirmed records
    int elapsed_seconds;// 本次同步从按键触发起的总用时
    int speed_kbps_x10; // 下载平均速度，单位 0.1 KB/s
    bool download_done;
    bool erase_phase;   // 已进入设备清理阶段（失败/完成后仍用于 UI 展示结果）
    int erase_percent;  // 设备报告的清理进度，0..100；<0 表示尚未报告
    int erase_elapsed_seconds;
    char device[64];    // 已连接设备名
    char message[64];   // 一句话状态
} racebox_progress_t;

void racebox_service_init(const char *upload_topic, bool auto_erase,
                          const char *device_prefix, const char *device_lock);
/* 运行中修改配置（网页保存后调用） */
void racebox_service_reconfigure(const char *upload_topic, bool auto_erase,
                                 const char *device_prefix, const char *device_lock);
/* 按键触发：开始一次采集流程（BLE 下载 → 解析 → MQTT 上传 → 擦除） */
void racebox_service_trigger(void);
/* 取消当前扫描/连接/下载并断开 BLE。 */
void racebox_service_cancel(void);
racebox_state_t racebox_service_state(void);
void racebox_service_progress(racebox_progress_t *out);
int racebox_service_point_count(void);
bool racebox_service_synced_today(void);
void racebox_service_day_tick(int year, int month, int day);

/* 同步各阶段的提示音事件：由服务层打点，UI 任务取出后播放。
 * 不能直接在 NimBLE 回调里播音——那会阻塞通知消费并耗尽 ACL 缓冲。 */
typedef enum {
    RB_SND_NONE = 0,
    RB_SND_START,           /* 触发同步 / 开始下载 */
    RB_SND_DEVICE_FOUND,    /* 搜到设备 */
    RB_SND_DOWNLOAD_DONE,   /* 下载完成 */
    RB_SND_UPLOAD_DONE,     /* 上传完成 */
    RB_SND_ERROR_DOWNLOAD,  /* 设备侧 / 下载出错 */
    RB_SND_ERROR_UPLOAD,    /* MQTT / 上传出错 */
    RB_SND_NO_DATA,         /* 设备无定位数据 */
    RB_SND_CANCEL,          /* 手动中断同步 */
} racebox_sound_t;

/* 取出并清除一个待播放的提示音事件；没有则返回 RB_SND_NONE */
racebox_sound_t racebox_service_take_sound(void);

/* 上传 RBX2 二进制批次到 MQTT（QoS 1）；成功返回 0。 */
int racebox_publish_raw_batch(const uint8_t *records, int offset, int count,
                              const uint8_t sync_id[16], int session_index,
                              int session_offset, int session_total,
                              uint64_t session_start_utc, uint64_t session_end_utc,
                              uint32_t session_start_itow, int32_t session_start_nanoseconds);

#ifdef __cplusplus
}
#endif
