#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "racebox_service.h"
#include "racebox_ble.h"
#include "app_mqtt.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RACEBOX_RAW_RECORD_SIZE 80u
#define RACEBOX_MQTT_HEADER_SIZE 136u
#define RACEBOX_MQTT_BATCH_RECORDS 720
#define RACEBOX_RECORDS_PER_CHUNK 64

typedef struct racebox_record_chunk {
    struct racebox_record_chunk *next;
    uint8_t records[RACEBOX_RECORDS_PER_CHUNK * RACEBOX_RAW_RECORD_SIZE];
} racebox_record_chunk_t;

typedef struct racebox_session {
    struct racebox_session *next;
    int index;
    int start_offset;
    volatile int record_count;
    volatile bool complete;
    uint64_t start_utc;
    volatile uint64_t end_utc;
    uint32_t start_itow;
    int32_t start_nanoseconds;
} racebox_session_t;

static const char *TAG = "racebox";
static volatile racebox_state_t s_state = RACEBOX_IDLE;
static int s_point_count = 0;
static bool s_synced_today = false;
static bool s_auto_erase = false;
static bool s_count_accum_mode = false;
static char s_upload_topic[96] = "deskwong/racebox/data";
static char s_device[64] = "";
static int s_total = 0;
static volatile int s_received = 0;
static char s_message[64] = "";
static int64_t s_last_activity_us = 0;
static int64_t s_started_us = 0;
static uint8_t s_rx_stream[4096];
static size_t s_rx_stream_len = 0;
static racebox_record_chunk_t *s_record_chunks;
static racebox_record_chunk_t *s_record_tail;
static volatile int s_chunk_count;
static int s_cached_offset;
static volatile int s_uploaded;
static racebox_session_t *s_sessions;
static racebox_session_t *s_session_tail;
static racebox_session_t *s_open_session;
static int s_session_count;
static volatile bool s_download_done, s_worker_active, s_cancel_upload;
static volatile bool s_erase_pending;
static bool s_bad_stream;
static int s_today, s_sync_day;
static volatile bool s_save_daily;
static TaskHandle_t s_upload_task;
static StaticTask_t *s_upload_tcb;
static StackType_t *s_upload_stack;
static void upload_worker(void *arg);
static TaskHandle_t s_daily_save_task;
static StaticTask_t *s_daily_save_tcb;
static StackType_t *s_daily_save_stack;

static void free_record_chunks(void) {
    racebox_record_chunk_t *chunk = s_record_chunks;
    while (chunk) {
        racebox_record_chunk_t *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    s_record_chunks = NULL;
    s_record_tail = NULL;
    s_chunk_count = 0;
    s_cached_offset = 0;
}

static void free_sessions(void) {
    racebox_session_t *session = s_sessions;
    while (session) {
        racebox_session_t *next = session->next;
        free(session);
        session = next;
    }
    s_sessions = NULL;
    s_session_tail = NULL;
    s_open_session = NULL;
    s_session_count = 0;
}

static bool append_record(const uint8_t *record) {
    int slot = s_received % RACEBOX_RECORDS_PER_CHUNK;
    if (slot == 0) {
        racebox_record_chunk_t *chunk = heap_caps_malloc(sizeof(*chunk),
                                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!chunk) return false;
        chunk->next = NULL;
        if (s_record_tail) s_record_tail->next = chunk;
        else s_record_chunks = chunk;
        s_record_tail = chunk;
        s_chunk_count++;
    }
    memcpy(s_record_tail->records + (size_t)slot * RACEBOX_RAW_RECORD_SIZE,
           record, RACEBOX_RAW_RECORD_SIZE);
    return true;
}

static bool copy_record_range(int offset, int count, uint8_t *out) {
    if (offset < s_cached_offset || count < 0 || offset > s_received - count || !out) return false;
    racebox_record_chunk_t *chunk = s_record_chunks;
    int chunk_index = (offset - s_cached_offset) / RACEBOX_RECORDS_PER_CHUNK;
    for (int i = 0; chunk && i < chunk_index; ++i) chunk = chunk->next;
    int slot = (offset - s_cached_offset) % RACEBOX_RECORDS_PER_CHUNK;
    while (count > 0 && chunk) {
        int take = RACEBOX_RECORDS_PER_CHUNK - slot;
        if (take > count) take = count;
        memcpy(out, chunk->records + (size_t)slot * RACEBOX_RAW_RECORD_SIZE,
               (size_t)take * RACEBOX_RAW_RECORD_SIZE);
        out += (size_t)take * RACEBOX_RAW_RECORD_SIZE;
        count -= take;
        chunk = chunk->next;
        slot = 0;
    }
    return count == 0;
}

static void release_uploaded_chunks(void) {
    while (s_record_chunks && s_uploaded - s_cached_offset >= RACEBOX_RECORDS_PER_CHUNK) {
        racebox_record_chunk_t *done = s_record_chunks;
        s_record_chunks = done->next;
        if (!s_record_chunks) s_record_tail = NULL;
        free(done);
        s_cached_offset += RACEBOX_RECORDS_PER_CHUNK;
        s_chunk_count--;
    }
}

static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static int32_t i32(const uint8_t *p) { return (int32_t)u32(p); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1]<<8; }
static int16_t i16(const uint8_t *p) { return (int16_t)u16(p); }
static void put_u16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put_u32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void put_u64(uint8_t *p, uint64_t v) {
    put_u32(p, (uint32_t)v);
    put_u32(p + 4, (uint32_t)(v >> 32));
}
static uint64_t record_utc14(const uint8_t *p) {
    return (uint64_t)u16(p + 4) * 10000000000ULL +
           (uint64_t)p[6] * 100000000ULL + (uint64_t)p[7] * 1000000ULL +
           (uint64_t)p[8] * 10000ULL + (uint64_t)p[9] * 100ULL + p[10];
}

static bool start_session(const uint8_t *record) {
    racebox_session_t *session = heap_caps_calloc(1, sizeof(*session),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!session) return false;
    session->index = s_session_count++;
    session->start_offset = s_received;
    session->start_utc = record_utc14(record);
    session->end_utc = session->start_utc;
    session->start_itow = u32(record);
    session->start_nanoseconds = i32(record + 16);
    if (s_session_tail) s_session_tail->next = session;
    else s_sessions = session;
    s_session_tail = session;
    s_open_session = session;
    return true;
}

static void update_open_session(const uint8_t *record) {
    if (!s_open_session) return;
    s_open_session->record_count = s_received - s_open_session->start_offset;
    s_open_session->end_utc = record_utc14(record);
}

static void finish_open_session(void) {
    if (!s_open_session) return;
    if (s_open_session->record_count > 0) {
        s_open_session->complete = true;
        ESP_LOGI(TAG, "session %d complete offset=%d records=%d %llu_%llu",
                 s_open_session->index, s_open_session->start_offset,
                 s_open_session->record_count,
                 (unsigned long long)s_open_session->start_utc,
                 (unsigned long long)s_open_session->end_utc);
    }
    s_open_session = NULL;
}
static uint32_t crc32_ieee(const uint8_t *data, size_t len) {
    uint32_t crc=0xffffffffu;
    while (len--) {
        crc ^= *data++;
        for (int bit=0; bit<8; ++bit) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return crc^0xffffffffu;
}

static void finish_success(void) {
    bool same_day = s_synced_today && s_sync_day != 0 && s_sync_day == s_today;
    if (s_auto_erase && s_count_accum_mode && same_day) s_point_count += s_received;
    else s_point_count = s_received;
    s_count_accum_mode = s_auto_erase;
    s_sync_day = s_today;
    s_synced_today = true;
    s_save_daily = true;
    if (s_daily_save_task) xTaskNotifyGive(s_daily_save_task);
}
static void decode_record(const uint8_t *p, racebox_record_t *r) {
    memset(r, 0, sizeof(*r));
    r->itow=u32(p); r->year=u16(p+4); r->month=p[6]; r->day=p[7];
    r->hour=p[8]; r->minute=p[9]; r->second=p[10];
    r->time_accuracy=u32(p+12); r->nanoseconds=i32(p+16);
    r->fix_status=p[20]; r->numberof_svs=p[23];
    r->longitude=i32(p+24)/1e7; r->latitude=i32(p+28)/1e7;
    r->wgs_altitude=i32(p+32)/1000.0; r->msl_altitude=i32(p+36)/1000.0;
    r->horizontal_accuracy=u32(p+40)/1000.0; r->vertical_accuracy=u32(p+44)/1000.0;
    r->speed=i32(p+48)/1000.0; r->heading=i32(p+52)/1e5;
    r->speed_accuracy=u32(p+56); r->heading_accuracy=u32(p+60); r->pdop=u16(p+64);
    r->gforce_x=i16(p+68)/1000.0; r->gforce_y=i16(p+70)/1000.0; r->gforce_z=i16(p+72)/1000.0;
    r->rotation_rate_x=i16(p+74)/100.0; r->rotation_rate_y=i16(p+76)/100.0; r->rotation_rate_z=i16(p+78)/100.0;
}


/* UBX 风格命令（见 doc/SPECIFICATIONS.md §3，来自 racewong） */
static const uint8_t CMD_DOWNLOAD[] = {0xB5, 0x62, 0xFF, 0x23, 0x00, 0x00, 0x22, 0x65};
static const uint8_t CMD_ERASE[] = {0xB5, 0x62, 0xFF, 0x24, 0x00, 0x00, 0x23, 0x68};

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
        s_erase_pending = false;
        s_state = RACEBOX_FAILED;
        s_cancel_upload = true;
        set_message("未发现设备");
        ESP_LOGW(TAG, "no RaceBox device found");
    }
}

static void on_ble_conn(bool connected) {
    if (connected) {
        /* BLE 层只会在 NUS 特征发现和通知订阅完成后回调到这里。
         * 此设备的手机同步无需安全码，直接发送历史数据下载命令。 */
        s_rx_stream_len = 0;
        const uint8_t *cmd = s_erase_pending ? CMD_ERASE : CMD_DOWNLOAD;
        size_t cmd_len = s_erase_pending ? sizeof(CMD_ERASE) : sizeof(CMD_DOWNLOAD);
        int rc = racebox_ble_send(cmd, cmd_len);
        if (rc == 0) {
            s_state = s_erase_pending ? RACEBOX_ERASING : RACEBOX_DOWNLOADING;
            s_last_activity_us = esp_timer_get_time();
            set_message(s_erase_pending ? "正在清理设备数据" : "等待设备发送数据");
            ESP_LOGI(TAG, "%s command sent", s_erase_pending ? "erase" : "download");
        } else {
            s_state = RACEBOX_FAILED;
            s_erase_pending = false;
            set_message("设备命令发送失败");
            ESP_LOGE(TAG, "device command failed rc=%d", rc);
        }
    } else {
        if (s_state == RACEBOX_DOWNLOADING || (s_state == RACEBOX_UPLOADING && !s_download_done) ||
            s_state == RACEBOX_CONNECTING || s_state == RACEBOX_QUERYING || s_state == RACEBOX_ERASING) {
            s_erase_pending = false;
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
    /* 不逐帧打印：RaceBox 下载时每秒会产生数百帧，在 NimBLE host
     * 回调中输出串口日志会阻塞通知消费并耗尽控制器 ACL 缓冲。 */
    if (data[2] != 0xFF) return;
    if (s_state == RACEBOX_ERASING) {
        if (data[3] == 0x02 && plen == 2 && data[6] == 0xFF && data[7] == 0x24) {
            s_erase_pending = false;
            finish_success();
            s_state = RACEBOX_DONE;
            set_message("上传完成，设备数据已清理");
            ESP_LOGI(TAG,"erase confirmed; daily points=%d internal_free=%u",
                     s_point_count,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
            racebox_ble_stop();
        } else if (data[3] == 0x03 && plen >= 2 && data[6] == 0xFF && data[7] == 0x24) {
            s_erase_pending = false;
            s_state = RACEBOX_FAILED;
            set_message("上传完成，但设备清理失败");
            racebox_ble_stop();
        }
        return;
    }
    if (s_state != RACEBOX_DOWNLOADING) return;
    s_last_activity_us = esp_timer_get_time();

    if (data[3] == 0x03 && plen >= 2 && len >= 10 && data[6] == 0xFF) {
        s_cancel_upload = true;
        s_state = RACEBOX_FAILED;
        if (data[7] == 0x23) set_message("设备拒绝下载");
        else set_message("设备拒绝命令");
        ESP_LOGE(TAG, "NACK command=0x%02x", data[7]);
        racebox_ble_stop();
    } else if (data[3] == 0x23 && plen >= 4 && len >= 12) {
            s_total = (int)((uint32_t)data[6] | ((uint32_t)data[7] << 8) |
                            ((uint32_t)data[8] << 16) | ((uint32_t)data[9] << 24));
            if (s_total < 0) {
                s_state = RACEBOX_FAILED; set_message("设备数据条数异常"); racebox_ble_stop(); return;
            }
            if (app_mqtt_status() == APP_MQTT_UNCONFIGURED) {
                s_state=RACEBOX_FAILED; set_message("MQTT 未配置，无法流式同步"); racebox_ble_stop(); return;
            }
            s_worker_active = true;
            set_message("轨迹数据云端同步中");
            ESP_LOGI(TAG, "download accepted, expected=%d chunk_bytes=%u",
                     s_total, (unsigned)sizeof(racebox_record_chunk_t));
            if (app_mqtt_start() != 0 || !s_upload_tcb || !s_upload_stack ||
                !(s_upload_task = xTaskCreateStatic(upload_worker, "racebox_upload", 6144,
                                                     NULL, 3, s_upload_stack, s_upload_tcb))) {
                s_worker_active=false; s_state=RACEBOX_FAILED; set_message("上传任务启动失败");
                app_mqtt_stop(); racebox_ble_stop(); return;
            }
    } else if (data[3] == 0x21 || data[3] == 0x01) {
        if (plen != RACEBOX_RAW_RECORD_SIZE || (s_total > 0 && s_received >= s_total)) {
            s_state = RACEBOX_FAILED; set_message("数据长度异常"); racebox_ble_stop(); return;
        }
        if (!s_open_session && !start_session(data + 6)) {
            s_cancel_upload=true; s_state = RACEBOX_FAILED; set_message("轨迹分段内存不足"); racebox_ble_stop(); return;
        }
        if (!append_record(data + 6)) {
            ESP_LOGE(TAG, "trajectory cache exhausted received=%d uploaded=%d chunks=%d "
                          "psram_free=%u psram_largest=%u",
                     s_received, s_uploaded, s_chunk_count,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            s_cancel_upload=true; s_state = RACEBOX_FAILED; set_message("轨迹缓存不足"); racebox_ble_stop(); return;
        }
        s_received++;
        update_open_session(data + 6);
        if (s_received == 1 || s_received % 100 == 0)
            ESP_LOGI(TAG, "history record %d/%d", s_received, s_total);
    } else if (data[3] == 0x26) {
        /* RaceBox 的真实轨迹段边界；与 racewong 的文件名切分规则一致。 */
        finish_open_session();
    } else if (data[3] == 0x02 && plen == 2 && data[6] == 0xFF && data[7] == 0x23) {
            if (s_bad_stream) {
                s_cancel_upload=true; s_state=RACEBOX_FAILED; set_message("数据校验失败，请重试"); racebox_ble_stop(); return;
            }
            finish_open_session();
            s_download_done = true;
            s_state = RACEBOX_UPLOADING;
            set_message("下载完成，正在上传剩余数据");
            ESP_LOGI(TAG, "download complete received=%d expected=%d chunks=%d cached_bytes=%u",
                     s_received, s_total, s_chunk_count,
                     (unsigned)((size_t)s_chunk_count * sizeof(racebox_record_chunk_t)));
            racebox_ble_stop();
    }
}

static void on_ble_rx(const uint8_t *data, int len) {
    /* BLE 通知边界不等于 UBX 帧边界：一条通知可能含半帧或多帧。先进入
     * 流缓冲，再按 B5 62 + payload length + checksum 连续取完整帧。 */
    if (!data || len <= 0) return;
    if ((size_t)len > sizeof(s_rx_stream) - s_rx_stream_len) {
        ESP_LOGW(TAG, "RX stream overflow, reset (%u + %d)", (unsigned)s_rx_stream_len, len);
        s_rx_stream_len = 0;
        s_bad_stream = true;
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
        else { s_bad_stream = true; ESP_LOGW(TAG, "UBX checksum error len=%u", (unsigned)frame_len); }
        memmove(s_rx_stream, s_rx_stream + frame_len, s_rx_stream_len - frame_len);
        s_rx_stream_len -= frame_len;
    }
}

static void upload_worker(void *arg) {
    {
        uint8_t import_id[16];
        for (int i=0; i<4; ++i) put_u32(import_id+i*4, esp_random());
        bool ok = true;
        for (int i = 0; i < 100 && !app_mqtt_is_connected() && !s_cancel_upload; ++i)
            vTaskDelay(pdMS_TO_TICKS(100));
        if (!app_mqtt_is_connected()) { ok=false; set_message("MQTT 连接失败，停止同步"); racebox_ble_stop(); }
        uint8_t *batch = heap_caps_malloc(RACEBOX_MQTT_BATCH_RECORDS * RACEBOX_RAW_RECORD_SIZE,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!batch) { ok=false; set_message("上传批次内存不足"); racebox_ble_stop(); }
        racebox_session_t *session = s_sessions;
        while (ok) {
            if (s_cancel_upload) { ok=false; set_message("上传已取消"); break; }
            if (!session) {
                session = s_sessions;
                if (!session) {
                    if (s_download_done) break;
                    vTaskDelay(pdMS_TO_TICKS(20));
                    continue;
                }
            }
            int session_offset = s_uploaded - session->start_offset;
            int available = session->record_count - session_offset;
            /* 开放中的轨迹段保留最后一批，等 0x26 后携带准确结束时间和总数。 */
            int count = session->complete ? available : available - RACEBOX_MQTT_BATCH_RECORDS;
            if (count <= 0) {
                if (session->complete && available == 0) {
                    if (session == s_session_tail && !s_download_done) {
                        vTaskDelay(pdMS_TO_TICKS(20));
                        continue;
                    }
                    racebox_session_t *done = session;
                    session = done->next;
                    if (s_sessions == done) s_sessions = session;
                    if (s_session_tail == done) s_session_tail = session;
                    free(done);
                    continue;
                }
                if (s_download_done && !session->complete) {
                    ok=false; set_message("轨迹分段不完整"); break;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            if (count > RACEBOX_MQTT_BATCH_RECORDS) count=RACEBOX_MQTT_BATCH_RECORDS;
            int off = s_uploaded;
            set_message(s_download_done ? "下载完成，正在上传剩余数据" : "轨迹数据云端同步中");
            if (!copy_record_range(off,count,batch)) {
                ok=false; set_message("轨迹缓存读取失败"); break;
            }
            int rc=1;
            for (int attempt=1; attempt<=3 && rc; ++attempt) {
                rc=racebox_publish_raw_batch(batch,off,count,import_id,
                                              session->index,session_offset,
                                              session->complete ? session->record_count : 0,
                                              session->start_utc,
                                              session->complete ? session->end_utc : 0,
                                              session->start_itow,session->start_nanoseconds);
                if (rc && attempt<3) {
                    ESP_LOGW(TAG,"binary batch offset=%d retry=%d",off,attempt);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
            if (rc) { ok=false;set_message("MQTT 上传失败，请重试");break; }
            s_uploaded += count;
            release_uploaded_chunks();
            ESP_LOGI(TAG,"MQTT confirmed %d/%d",s_uploaded,s_received);
        }
        bool start_erase = ok && s_download_done && s_received && s_auto_erase;
        if (ok && !start_erase) {
            finish_success();
            set_message(s_received ? "下载及上传完成" : "下载完成，无历史数据");
        }
        ESP_LOGI(TAG,"summary downloaded=%d uploaded=%d auto_erase=%d",
                 s_received,s_uploaded,(int)s_auto_erase);
        free(batch);
        s_worker_active=false;
        if (!ok) racebox_ble_stop();
        app_mqtt_stop();
        ESP_LOGI(TAG,"MQTT stopped; internal_free=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
        if (start_erase) {
            s_erase_pending=true;
            s_state=RACEBOX_SCANNING;
            set_message("上传完成，正在连接设备清理数据");
            racebox_ble_start();
        } else {
            s_state = ok ? RACEBOX_DONE : RACEBOX_FAILED;
        }
    }
    vTaskDelete(NULL);
}

/* NVS writes disable the flash cache. The regular UI task can use a PSRAM
 * stack, which is inaccessible while the cache is disabled. Keep all daily
 * state writes on a small task whose stack and TCB are explicitly internal. */
static void daily_save_worker(void *arg) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (s_save_daily) {
            int date = s_sync_day;
            int points = s_point_count;
            nvs_handle_t h;
            bool saved = false;
            if (nvs_open("racebox_day", NVS_READWRITE, &h) == ESP_OK) {
                saved = nvs_set_i32(h, "date", date) == ESP_OK &&
                        nvs_set_i32(h, "points", points) == ESP_OK &&
                        nvs_set_u8(h, "accum", s_count_accum_mode ? 1 : 0) == ESP_OK &&
                        nvs_commit(h) == ESP_OK;
                nvs_close(h);
            }
            if (saved && date == s_sync_day && points == s_point_count) {
                s_save_daily = false;
                ESP_LOGI(TAG, "daily state saved date=%d points=%d stack_free=%u",
                         date, points, (unsigned)uxTaskGetStackHighWaterMark(NULL));
            } else {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}

void racebox_service_day_tick(int year, int month, int day) {
    if (year < 2025) return;
    int date=year*10000+month*100+day;
    s_today=date;
    if (s_synced_today && !s_sync_day) { s_sync_day=date; s_save_daily=true; }
    if (s_sync_day && s_sync_day!=date) {
        s_sync_day=0; s_point_count=0; s_synced_today=false; s_save_daily=true;
    }
    if (s_save_daily && s_daily_save_task) xTaskNotifyGive(s_daily_save_task);
}

void racebox_service_init(const char *upload_topic, bool auto_erase,
                          const char *device_prefix, const char *device_lock) {
    nvs_handle_t h;
    if (nvs_open("racebox_day",NVS_READONLY,&h)==ESP_OK) {
        int32_t date=0, points=0; uint8_t accum=0;
        nvs_get_i32(h,"date",&date); nvs_get_i32(h,"points",&points); nvs_get_u8(h,"accum",&accum); nvs_close(h);
        s_sync_day=date; s_point_count=points; s_synced_today=date>0;
        s_count_accum_mode=accum!=0;
    }
    s_upload_tcb = heap_caps_calloc(1, sizeof(*s_upload_tcb), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_upload_stack = heap_caps_calloc(6144, sizeof(*s_upload_stack), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_daily_save_tcb = heap_caps_calloc(1, sizeof(*s_daily_save_tcb), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    /* NVS commit traverses the flash/NVS layers while cache is disabled and
     * needs substantially more than the original 8KB stack. */
    s_daily_save_stack = heap_caps_calloc(4096, sizeof(*s_daily_save_stack), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_daily_save_tcb && s_daily_save_stack) {
        s_daily_save_task = xTaskCreateStatic(daily_save_worker, "racebox_day", 4096, NULL, 2,
                                               s_daily_save_stack, s_daily_save_tcb);
    }
    if (!s_daily_save_task) ESP_LOGE(TAG, "daily state task allocation failed");
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
    if (s_worker_active) return;
    free_record_chunks();
    free_sessions();
    s_uploaded=0; s_download_done=false; s_cancel_upload=false; s_bad_stream=false; s_erase_pending=false;
    s_total = 0;
    s_received = 0;
    s_rx_stream_len = 0;
    s_started_us = esp_timer_get_time();
    s_device[0] = 0;
    s_state = RACEBOX_SCANNING;
    set_message("搜索蓝牙设备...");
    racebox_ble_start();
    ESP_LOGI(TAG, "trigger: scanning RaceBox");
}

void racebox_service_cancel(void) {
    if (s_state == RACEBOX_IDLE || s_state == RACEBOX_DONE || s_state == RACEBOX_FAILED) return;
    if (s_worker_active) { s_cancel_upload=true; return; }
    s_erase_pending=false;
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
    if ((s_state == RACEBOX_QUERYING || s_state == RACEBOX_DOWNLOADING || s_state == RACEBOX_ERASING) && s_last_activity_us > 0 &&
        esp_timer_get_time() - s_last_activity_us > 30000000LL) {
        bool was_querying = s_state == RACEBOX_QUERYING;
        bool was_erasing = s_state == RACEBOX_ERASING;
        s_state = RACEBOX_FAILED;
        s_cancel_upload=true;
        s_erase_pending=false;
        set_message(was_erasing ? "上传完成，但设备清理超时" :
                    (was_querying ? "设备响应超时，请重试" : "下载超时，请重试"));
        ESP_LOGE(TAG, "transfer timeout received=%d expected=%d", s_received, s_total);
        racebox_ble_stop();
    }
    out->uploaded=s_uploaded;
    out->elapsed_seconds=s_started_us > 0 ? (int)((esp_timer_get_time()-s_started_us)/1000000LL) : 0;
    out->speed_kbps_x10 = out->elapsed_seconds > 0
        ? (int)((int64_t)s_received * RACEBOX_RAW_RECORD_SIZE * 10 /
                ((int64_t)out->elapsed_seconds * 1024)) : 0;
    out->download_done=s_download_done;
    out->state = s_state;
    out->total = s_total;
    out->received = s_received;
    /* 0x23 reports an upper bound, not an exact record count. */
    out->percent = s_download_done ? (s_received > 0 ? s_uploaded*100/s_received : 100) : (s_total > 0 && s_received <= s_total)
                       ? (int)((int64_t)s_received * 100 / s_total)
                       : -1;
    snprintf(out->device, sizeof(out->device), "%s", s_device);
    snprintf(out->message, sizeof(out->message), "%s", s_message);
}

int racebox_service_point_count(void) { return s_point_count; }
bool racebox_service_synced_today(void) { return s_synced_today; }

int racebox_publish_raw_batch(const uint8_t *records, int offset, int count,
                              const uint8_t sync_id[16], int session_index,
                              int session_offset, int session_total,
                              uint64_t session_start_utc, uint64_t session_end_utc,
                              uint32_t session_start_itow, int32_t session_start_nanoseconds) {
    if (!records || !sync_id || offset<0 || session_index<0 || session_offset<0 ||
        count<=0 || count>RACEBOX_MQTT_BATCH_RECORDS) return 1;
    size_t raw_len=(size_t)count*RACEBOX_RAW_RECORD_SIZE;
    size_t frame_len=RACEBOX_MQTT_HEADER_SIZE+raw_len;
    uint8_t *frame=heap_caps_calloc(1,frame_len,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!frame) return 1;
    memcpy(frame,"RBX2",4);
    frame[4]=2;                         /* protocol version */
    bool session_final = session_total > 0 && session_offset + count == session_total;
    bool sync_final = session_final && s_download_done && offset + count == s_received;
    frame[5]=(sync_final ? 1 : 0) | (session_final ? 2 : 0);
    put_u16(frame+6,RACEBOX_MQTT_HEADER_SIZE);
    put_u16(frame+8,RACEBOX_RAW_RECORD_SIZE);
    put_u16(frame+10,(uint16_t)count);
    put_u32(frame+12,(uint32_t)offset);
    put_u32(frame+16,(uint32_t)(sync_final ? s_received : s_total));
    put_u32(frame+20,(uint32_t)s_sync_day);
    memcpy(frame+24,sync_id,16);
    memcpy(frame+40,s_device,strnlen(s_device,47));
    put_u32(frame+88,(uint32_t)session_index);
    put_u32(frame+92,(uint32_t)session_offset);
    put_u32(frame+96,(uint32_t)session_total);
    put_u64(frame+100,session_start_utc);
    put_u64(frame+108,session_end_utc);
    put_u32(frame+116,session_start_itow);
    put_u32(frame+120,(uint32_t)session_start_nanoseconds);
    memcpy(frame+RACEBOX_MQTT_HEADER_SIZE,records,raw_len);
    put_u32(frame+128,crc32_ieee(records,raw_len));
    put_u32(frame+132,crc32_ieee(frame,132));
    int ret=app_mqtt_publish_bytes_confirmed(s_upload_topic,frame,frame_len,60000);
    ESP_LOGI(TAG,"publish binary offset=%d records=%d bytes=%u -> %d",
             offset,count,(unsigned)frame_len,ret);
    free(frame);
    return ret;
}
