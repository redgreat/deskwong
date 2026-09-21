#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "racebox_service.h"
#include "racebox_ble.h"
#include "app_mqtt.h"

static const char *TAG = "racebox";
static volatile racebox_state_t s_state = RACEBOX_IDLE;
static int s_point_count = 0;
static bool s_synced_today = false;
static bool s_auto_erase = true;
static char s_upload_topic[96] = "deskwong/racebox/data";
static char s_device[64] = "";
static int s_total = 0;
static int s_received = 0;
static char s_message[64] = "";
static int64_t s_last_activity_us = 0;
static uint8_t s_rx_stream[4096];
static size_t s_rx_stream_len = 0;

/* UBX 风格命令（见 doc/rules/04 §2.2，来自 racewong） */
static const uint8_t CMD_DOWNLOAD[] = {0xB5, 0x62, 0xFF, 0x23, 0x00, 0x00, 0x22, 0x65};

static void set_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_message, sizeof(s_message), fmt, ap);
    va_end(ap);
}

static void on_ble_disc(const char *name) {
    snprintf(s_device, sizeof(s_device), "%s", name);
    s_state = RACEBOX_CONNECTING;
    set_message("发现 %s", name);
}

static void on_ble_scan_done(void) {
    if (s_state == RACEBOX_SCANNING) {
        s_state = RACEBOX_FAILED;
        set_message("未发现设备");
        ESP_LOGW(TAG, "no RaceBox device found");
    }
}

static void on_ble_conn(bool connected) {
    if (connected) {
        /* BLE 层只会在 NUS 特征发现和通知订阅完成后回调到这里。
         * 此设备的手机同步无需安全码，直接发送历史数据下载命令。 */
        s_rx_stream_len = 0;
        int rc = racebox_ble_send(CMD_DOWNLOAD, sizeof(CMD_DOWNLOAD));
        if (rc == 0) {
            s_state = RACEBOX_DOWNLOADING;
            s_last_activity_us = esp_timer_get_time();
            set_message("等待设备发送数据");
            ESP_LOGI(TAG, "download command sent");
        } else {
            s_state = RACEBOX_FAILED;
            set_message("下载命令发送失败");
            ESP_LOGE(TAG, "download command failed rc=%d", rc);
        }
    } else {
        if (s_state == RACEBOX_DOWNLOADING || s_state == RACEBOX_UPLOADING ||
            s_state == RACEBOX_CONNECTING || s_state == RACEBOX_QUERYING) {
            s_state = RACEBOX_FAILED;
            set_message("连接中断");
            ESP_LOGW(TAG, "disconnected during transfer");
        }
    }
}

static bool checksum_ok(const uint8_t *frame, size_t frame_len) {
    if (frame_len < 8) return false;
    uint8_t a = 0, b = 0;
    for (size_t i = 2; i + 2 < frame_len; ++i) { a += frame[i]; b += a; }
    return a == frame[frame_len - 2] && b == frame[frame_len - 1];
}

static void handle_frame(const uint8_t *data, size_t len) {
    int plen = data[4] | (data[5] << 8);
    s_last_activity_us = esp_timer_get_time();
    /* 不逐帧打印：RaceBox 下载时每秒会产生数百帧，在 NimBLE host
     * 回调中输出串口日志会阻塞通知消费并耗尽控制器 ACL 缓冲。 */
    if (data[2] != 0xFF) return;

    if (data[3] == 0x03 && plen >= 2 && len >= 10 && data[6] == 0xFF) {
        s_state = RACEBOX_FAILED;
        if (data[7] == 0x23) set_message("设备拒绝下载");
        else set_message("设备拒绝命令");
        ESP_LOGE(TAG, "NACK command=0x%02x", data[7]);
    } else if (data[3] == 0x23 && plen >= 4 && len >= 12) {
            s_total = (int)((uint32_t)data[6] | ((uint32_t)data[7] << 8) |
                            ((uint32_t)data[8] << 16) | ((uint32_t)data[9] << 24));
            set_message("正在下载轨迹数据");
            ESP_LOGI(TAG, "download accepted, expected=%d", s_total);
    } else if (data[3] == 0x21) {
        s_received++;
        if (s_received == 1 || s_received % 100 == 0)
            ESP_LOGI(TAG, "history record %d/%d", s_received, s_total);
    } else if (data[3] == 0x02 && plen >= 2 && len >= 10 &&
               data[6] == 0xFF && data[7] == 0x23) {
            s_state = RACEBOX_DONE;
            s_point_count = s_received;
            s_synced_today = true;
            set_message("下载完成 %d 条", s_received);
            ESP_LOGI(TAG, "download complete received=%d expected=%d", s_received, s_total);
    }
}

static void on_ble_rx(const uint8_t *data, int len) {
    /* BLE 通知边界不等于 UBX 帧边界：一条通知可能含半帧或多帧。先进入
     * 流缓冲，再按 B5 62 + payload length + checksum 连续取完整帧。 */
    if (!data || len <= 0) return;
    if ((size_t)len > sizeof(s_rx_stream) - s_rx_stream_len) {
        ESP_LOGW(TAG, "RX stream overflow, reset (%u + %d)", (unsigned)s_rx_stream_len, len);
        s_rx_stream_len = 0;
        if ((size_t)len > sizeof(s_rx_stream)) return;
    }
    memcpy(s_rx_stream + s_rx_stream_len, data, (size_t)len);
    s_rx_stream_len += (size_t)len;

    while (s_rx_stream_len >= 2) {
        size_t start = 0;
        while (start + 1 < s_rx_stream_len &&
               !(s_rx_stream[start] == 0xB5 && s_rx_stream[start + 1] == 0x62)) start++;
        if (start > 0) {
            memmove(s_rx_stream, s_rx_stream + start, s_rx_stream_len - start);
            s_rx_stream_len -= start;
        }
        if (s_rx_stream_len < 6) return;
        size_t plen = (size_t)s_rx_stream[4] | ((size_t)s_rx_stream[5] << 8);
        size_t frame_len = 6 + plen + 2;
        if (frame_len > sizeof(s_rx_stream)) {
            ESP_LOGE(TAG, "invalid UBX length=%u", (unsigned)plen);
            memmove(s_rx_stream, s_rx_stream + 2, s_rx_stream_len - 2);
            s_rx_stream_len -= 2;
            continue;
        }
        if (s_rx_stream_len < frame_len) return;
        if (checksum_ok(s_rx_stream, frame_len)) handle_frame(s_rx_stream, frame_len);
        else ESP_LOGW(TAG, "UBX checksum error len=%u", (unsigned)frame_len);
        memmove(s_rx_stream, s_rx_stream + frame_len, s_rx_stream_len - frame_len);
        s_rx_stream_len -= frame_len;
    }
}

void racebox_service_init(const char *upload_topic, bool auto_erase,
                          const char *device_prefix, const char *device_lock) {
    racebox_ble_init();
    racebox_ble_set_conn_cb(on_ble_conn);
    racebox_ble_set_rx_cb(on_ble_rx);
    racebox_ble_set_disc_cb(on_ble_disc);
    racebox_ble_set_scan_done_cb(on_ble_scan_done);
    s_state = RACEBOX_IDLE;
    racebox_service_reconfigure(upload_topic, auto_erase, device_prefix, device_lock);
}

void racebox_service_reconfigure(const char *upload_topic, bool auto_erase,
                                 const char *device_prefix, const char *device_lock) {
    s_auto_erase = auto_erase;
    if (upload_topic && upload_topic[0]) {
        strncpy(s_upload_topic, upload_topic, sizeof(s_upload_topic) - 1);
        s_upload_topic[sizeof(s_upload_topic) - 1] = 0;
    }
    racebox_ble_set_filter(device_prefix, device_lock);
    ESP_LOGI(TAG, "config: topic=%s erase=%d prefix=%s lock=%s",
             s_upload_topic, (int)auto_erase,
             device_prefix ? device_prefix : "", device_lock ? device_lock : "");
}

void racebox_service_trigger(void) {
    if (s_state != RACEBOX_IDLE && s_state != RACEBOX_DONE && s_state != RACEBOX_FAILED) {
        ESP_LOGW(TAG, "busy, state=%d", s_state);
        return;
    }
    s_total = 0;
    s_received = 0;
    s_rx_stream_len = 0;
    s_device[0] = 0;
    s_state = RACEBOX_SCANNING;
    set_message("搜索蓝牙设备...");
    racebox_ble_start();
    ESP_LOGI(TAG, "trigger: scanning RaceBox");
}

void racebox_service_cancel(void) {
    if (s_state == RACEBOX_IDLE || s_state == RACEBOX_DONE || s_state == RACEBOX_FAILED) return;
    racebox_ble_stop();
    s_state = RACEBOX_FAILED;
    set_message("同步已取消");
    ESP_LOGI(TAG, "sync cancelled by user");
}

racebox_state_t racebox_service_state(void) {
    return s_state;
}

void racebox_service_progress(racebox_progress_t *out) {
    if (!out) return;
    if ((s_state == RACEBOX_QUERYING || s_state == RACEBOX_DOWNLOADING) && s_last_activity_us > 0 &&
        esp_timer_get_time() - s_last_activity_us > 30000000LL) {
        bool was_querying = s_state == RACEBOX_QUERYING;
        s_state = RACEBOX_FAILED;
        set_message(was_querying ? "设备响应超时，请重试" : "下载超时，请重试");
        ESP_LOGE(TAG, "transfer timeout received=%d expected=%d", s_received, s_total);
        racebox_ble_stop();
    }
    out->state = s_state;
    out->total = s_total;
    out->received = s_received;
    out->percent = (s_total > 0 && s_received <= s_total)
                       ? (int)(s_received * 100 / s_total)
                       : -1;
    snprintf(out->device, sizeof(out->device), "%s", s_device);
    snprintf(out->message, sizeof(out->message), "%s", s_message);
}

int racebox_service_point_count(void) { return s_point_count; }
bool racebox_service_synced_today(void) { return s_synced_today; }

int racebox_publish_records(const racebox_record_t *recs, int count,
                            const char *imp_stamp, const char *file_name) {
    if (recs == NULL || count <= 0) return 1;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", "deskwong-01");
    cJSON_AddStringToObject(root, "imp_stamp", imp_stamp ? imp_stamp : "");
    cJSON_AddStringToObject(root, "file_name", file_name ? file_name : "");
    cJSON_AddNumberToObject(root, "count", count);

    cJSON *arr = cJSON_AddArrayToObject(root, "records");
    for (int i = 0; i < count; i++) {
        const racebox_record_t *r = &recs[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "itow", r->itow);
        cJSON_AddNumberToObject(o, "year", r->year);
        cJSON_AddNumberToObject(o, "month", r->month);
        cJSON_AddNumberToObject(o, "day", r->day);
        cJSON_AddNumberToObject(o, "hour", r->hour);
        cJSON_AddNumberToObject(o, "minute", r->minute);
        cJSON_AddNumberToObject(o, "second", r->second);
        cJSON_AddNumberToObject(o, "time_accuracy", r->time_accuracy);
        cJSON_AddNumberToObject(o, "nanoseconds", r->nanoseconds);
        cJSON_AddNumberToObject(o, "fix_status", r->fix_status);
        cJSON_AddNumberToObject(o, "numberof_svs", r->numberof_svs);
        cJSON_AddNumberToObject(o, "longitude", r->longitude);
        cJSON_AddNumberToObject(o, "latitude", r->latitude);
        cJSON_AddNumberToObject(o, "wgs_altitude", r->wgs_altitude);
        cJSON_AddNumberToObject(o, "msl_altitude", r->msl_altitude);
        cJSON_AddNumberToObject(o, "horizontal_accuracy", r->horizontal_accuracy);
        cJSON_AddNumberToObject(o, "vertical_accuracy", r->vertical_accuracy);
        cJSON_AddNumberToObject(o, "speed", r->speed);
        cJSON_AddNumberToObject(o, "heading", r->heading);
        cJSON_AddNumberToObject(o, "speed_accuracy", r->speed_accuracy);
        cJSON_AddNumberToObject(o, "heading_accuracy", r->heading_accuracy);
        cJSON_AddNumberToObject(o, "pdop", r->pdop);
        cJSON_AddNumberToObject(o, "gforce_x", r->gforce_x);
        cJSON_AddNumberToObject(o, "gforce_y", r->gforce_y);
        cJSON_AddNumberToObject(o, "gforce_z", r->gforce_z);
        cJSON_AddNumberToObject(o, "rotation_rate_x", r->rotation_rate_x);
        cJSON_AddNumberToObject(o, "rotation_rate_y", r->rotation_rate_y);
        cJSON_AddNumberToObject(o, "rotation_rate_z", r->rotation_rate_z);
        cJSON_AddItemToArray(arr, o);
    }

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return 1;

    s_state = RACEBOX_UPLOADING;
    set_message("上传 %d 条", count);
    int ret = app_mqtt_publish(s_upload_topic, body, 1);
    if (ret == 0) {
        s_point_count = count;
        s_synced_today = true;
        s_total = count;
        s_received = count;
        s_state = RACEBOX_DONE;
        set_message("同步完成 %d 条", count);
    } else {
        s_state = RACEBOX_FAILED;
        set_message("上传失败");
    }
    ESP_LOGI(TAG, "publish %d records -> %d", count, ret);
    free(body);
    return ret;
}
