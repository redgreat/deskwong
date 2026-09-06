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

void racebox_service_init(void);
/* 按键触发：开始一次采集流程（BLE 下载 → 解析 → MQTT 上传 → 擦除） */
void racebox_service_trigger(void);
racebox_state_t racebox_service_state(void);

/* 上传一批记录到 MQTT（deskwong/racebox/data，QoS 1）；成功返回 0 */
int racebox_publish_records(const racebox_record_t *recs, int count,
                            const char *imp_stamp, const char *file_name);

#ifdef __cplusplus
}
#endif
