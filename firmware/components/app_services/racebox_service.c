#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "cJSON.h"
#include "racebox_service.h"
#include "racebox_ble.h"
#include "app_mqtt.h"

static const char *TAG = "racebox";
static racebox_state_t s_state = RACEBOX_IDLE;

/* UBX 风格命令（见 doc/rules/04 §2.2，来自 racewong） */
static const uint8_t CMD_STATUS[]   = {0xB5, 0x62, 0xFF, 0x22, 0x00, 0x00, 0x21, 0x62};
static const uint8_t CMD_DOWNLOAD[] = {0xB5, 0x62, 0xFF, 0x23, 0x00, 0x00, 0x22, 0x65};
static const uint8_t CMD_ERASE[]    = {0xB5, 0x62, 0xFF, 0x24, 0x00, 0x00, 0x23, 0x68};

static void on_ble_conn(bool connected) {
    if (connected) {
        s_state = RACEBOX_QUERYING;
        racebox_ble_send(CMD_STATUS, sizeof(CMD_STATUS));
        racebox_ble_send(CMD_DOWNLOAD, sizeof(CMD_DOWNLOAD));
        s_state = RACEBOX_DOWNLOADING;
        ESP_LOGI(TAG, "status + download command sent");
    } else {
        if (s_state == RACEBOX_DOWNLOADING || s_state == RACEBOX_UPLOADING) {
            s_state = RACEBOX_FAILED;
            ESP_LOGW(TAG, "disconnected during transfer");
        }
    }
}

static bool s_erase_sent = false;

static void on_ble_rx(const uint8_t *data, int len) {
    /* UBX 帧解析：B5 62 头 + class/id + length + payload + CK_A/CK_B
     * TODO(M4)：真实 RaceBox 抓包后完善字段解析（itow/datetime/gps/imu） */
    if (len >= 6 && data[0] == 0xB5 && data[1] == 0x62) {
        int plen = data[4] | (data[5] << 8);
        ESP_LOGI(TAG, "UBX frame class=0x%02x id=0x%02x len=%d", data[2], data[3], plen);
    }
    /* 下载完成后：上传（桩）+ 擦除。真实解析在 M4 联调时接入 racebox_publish_records */
    if (!s_erase_sent) {
        s_erase_sent = true;
        racebox_ble_send(CMD_ERASE, sizeof(CMD_ERASE));
        s_state = RACEBOX_ERASING;
        ESP_LOGI(TAG, "erase command sent");
    }
}

void racebox_service_init(void) {
    racebox_ble_init();
    racebox_ble_set_conn_cb(on_ble_conn);
    racebox_ble_set_rx_cb(on_ble_rx);
    s_state = RACEBOX_IDLE;
}

void racebox_service_trigger(void) {
    if (s_state != RACEBOX_IDLE && s_state != RACEBOX_DONE && s_state != RACEBOX_FAILED) {
        ESP_LOGW(TAG, "busy, state=%d", s_state);
        return;
    }
    s_erase_sent = false;
    s_state = RACEBOX_SCANNING;
    racebox_ble_start();
    ESP_LOGI(TAG, "trigger: scanning RaceBox");
}

racebox_state_t racebox_service_state(void) {
    return s_state;
}

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

    int ret = app_mqtt_publish("deskwong/racebox/data", body, 1);
    ESP_LOGI(TAG, "publish %d records -> %d", count, ret);
    free(body);
    return ret;
}
