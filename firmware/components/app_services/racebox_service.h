#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RaceBox 单条记录（字段对齐 lc_racebox 表，见 doc/rules/04） */
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

/* 上传一批记录到 MQTT（deskwong/racebox/data，QoS 1）；成功返回 0 */
int racebox_publish_records(const racebox_record_t *recs, int count,
                            const char *imp_stamp, const char *file_name);

#ifdef __cplusplus
}
#endif
