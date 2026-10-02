/*
 * voice_service.c —— 小智（xiaozhi）WebSocket 语音客户端
 *
 * 链路：OTA 取 websocket 地址/token -> WSS 握手 -> hello 交换 -> Opus 双向流
 * 上行：麦克风 16kHz 单声道 Opus（60ms 帧）
 * 下行：服务端 Opus（通常 24kHz），解码后重采样到 16kHz 播放
 *
 * 协议参考 78/xiaozhi-esp32 docs/websocket.md：
 *   请求头 Authorization / Protocol-Version / Device-Id / Client-Id
 *   文本帧 JSON（hello、listen、abort、mcp、tts、stt、llm、system、alert）
 *   二进制帧为 Opus 音频
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_efuse.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"

#include "esp_codec_dev.h"
#include "cJSON.h"
#include "opus.h"

#include "app_config.h"
#include "audio_service.h"
#include "voice_service.h"

static const char *TAG = "voice";

/* 未配置 voice_server_url 时，用官方公有服务端的 OTA 接口换取地址与 token */
#define XZ_DEFAULT_OTA_URL   "https://api.tenclass.net/xiaozhi/ota/"
#define XZ_PROTOCOL_VERSION  1

#define MIC_RATE      16000
#define FRAME_MS      60
#define MIC_SAMPLES   (MIC_RATE * FRAME_MS / 1000)          /* 960 */
#define MAX_DEC_SAMPLES (48000 * FRAME_MS / 1000)           /* 2880，按 48k 上限 */
#define OPUS_OUT_MAX  1500

#define HELLO_BIT     BIT0

#define VOICE_NVS_NS        "voice"
#define VOICE_NVS_CLIENT_ID "clientid"

typedef struct {
    bool inited;
    voice_state_t state;
    SemaphoreHandle_t mutex;
    EventGroupHandle_t events;

    const app_config_t *cfg;

    esp_websocket_client_handle_t ws;
    char ws_url[192];
    char token[128];
    char device_id[24];   /* MAC */
    char client_id[40];   /* UUID，首次生成后存 NVS */
    char session_id[40];
    int  down_rate;       /* 服务端下行采样率，由 hello 决定 */

    OpusEncoder *enc;
    OpusDecoder *dec;

    /* 音频缓冲：上行与下行分开，避免采集线程与 WS 回调争用 */
    int16_t *in_stereo;
    opus_int16 *in_mono;
    opus_int16 *dec_pcm;
    opus_int16 *out_mono;
    int16_t *out_stereo;
    uint8_t *opus_buf;

    TaskHandle_t mic_task;
    volatile bool mic_run;

    voice_event_cb_t cb;
    void *cb_ctx;

    char last_err[160];
} voice_ctx_t;

static voice_ctx_t s;

/* ------------------------------------------------------------------ 基础工具 */

static void lock(void)
{
    if (s.mutex) xSemaphoreTake(s.mutex, portMAX_DELAY);
}

static void unlock(void)
{
    if (s.mutex) xSemaphoreGive(s.mutex);
}

static void set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s.last_err, sizeof(s.last_err), fmt, ap);
    va_end(ap);
    ESP_LOGE(TAG, "%s", s.last_err);
}

static void notify(voice_state_t st, const char *text, const char *emotion)
{
    s.state = st;
    if (s.cb) {
        voice_event_t ev = { .state = st, .text = text, .emotion = emotion };
        s.cb(&ev, s.cb_ctx);
    }
}

/* 按 PSRAM 优先分配，失败退回内部 RAM */
static void *alloc_buf(size_t bytes)
{
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    return p;
}

static bool alloc_buffers(void)
{
    if (s.in_stereo) return true;
    s.in_stereo  = (int16_t *)alloc_buf(MIC_SAMPLES * 2 * sizeof(int16_t));
    s.in_mono    = (opus_int16 *)alloc_buf(MIC_SAMPLES * sizeof(opus_int16));
    s.dec_pcm    = (opus_int16 *)alloc_buf(MAX_DEC_SAMPLES * sizeof(opus_int16));
    s.out_mono   = (opus_int16 *)alloc_buf(MIC_SAMPLES * sizeof(opus_int16));
    s.out_stereo = (int16_t *)alloc_buf(MIC_SAMPLES * 2 * sizeof(int16_t));
    s.opus_buf   = (uint8_t *)alloc_buf(OPUS_OUT_MAX);
    if (!s.in_stereo || !s.in_mono || !s.dec_pcm || !s.out_mono || !s.out_stereo || !s.opus_buf) {
        set_error("音频缓冲分配失败（内存不足）");
        return false;
    }
    return true;
}

/* ------------------------------------------------------------- 设备标识 */

static void ensure_device_id(void)
{
    uint8_t mac[6] = {0};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        uint32_t r = esp_random();
        memcpy(mac, &r, 4);
    }
    snprintf(s.device_id, sizeof(s.device_id), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void ensure_client_id(void)
{
    nvs_handle_t h;
    if (nvs_open(VOICE_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "nvs open failed, client id 只存在于内存");
        h = 0;
    }
    if (h) {
        size_t len = sizeof(s.client_id);
        if (nvs_get_str(h, VOICE_NVS_CLIENT_ID, s.client_id, &len) == ESP_OK && s.client_id[0]) {
            nvs_close(h);
            return;
        }
    }
    uint32_t r0 = esp_random(), r1 = esp_random(), r2 = esp_random(), r3 = esp_random();
    snprintf(s.client_id, sizeof(s.client_id), "%08lx-%04lx-%04lx-%04lx-%04lx%08lx",
             (unsigned long)r0,
             (unsigned long)(r1 & 0xffff),
             (unsigned long)(((r1 >> 16) & 0x0fff) | 0x4000),
             (unsigned long)(((r2 >> 16) & 0x0fff) | 0x8000),
             (unsigned long)(r2 & 0xffff),
             (unsigned long)r3);
    if (h) {
        nvs_set_str(h, VOICE_NVS_CLIENT_ID, s.client_id);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "生成 client-id: %s", s.client_id);
}

/* ------------------------------------------------------------- OTA 换取地址 */

/* POST 官方/自建 OTA 接口，取 websocket.url 与 websocket.token */
static bool ota_fetch(void)
{
    char body[512];
    snprintf(body, sizeof(body),
             "{\"version\":1,\"force\":false,\"device_id\":\"%s\",\"client_id\":\"%s\","
             "\"chip_model_name\":\"esp32s3\",\"application\":{\"name\":\"deskwong\",\"version\":\"0.0.5\"},"
             "\"board\":{\"type\":\"s3-rlcd-4-2\"}}",
             s.device_id, s.client_id);

    const char *ota_url = (s.cfg && s.cfg->voice_server_url[0]) ? s.cfg->voice_server_url
                                                                : XZ_DEFAULT_OTA_URL;
    /* 配置里填的可能是 wss 地址，那就直接用，不再走 OTA */
    if (strncmp(ota_url, "ws", 2) == 0) {
        snprintf(s.ws_url, sizeof(s.ws_url), "%s", ota_url);
        if (s.cfg && s.cfg->voice_token[0]) {
            snprintf(s.token, sizeof(s.token), "%s", s.cfg->voice_token);
        } else {
            snprintf(s.token, sizeof(s.token), "test-token");
        }
        return true;
    }

    char *resp = (char *)alloc_buf(2048);
    if (!resp) {
        set_error("OTA 响应缓冲分配失败");
        return false;
    }

    esp_http_client_config_t cfg = {
        .url = ota_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        heap_caps_free(resp);
        set_error("OTA 客户端初始化失败");
        return false;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "Device-Id", s.device_id);
    esp_http_client_set_header(client, "Client-Id", s.client_id);

    bool ok = false;
    int total = 0;
    if (esp_http_client_open(client, (int)strlen(body)) == ESP_OK) {
        int w = esp_http_client_write(client, body, (int)strlen(body));
        if (w > 0) {
            int status = esp_http_client_fetch_headers(client);
            if (status > 0) {
                int len;
                while (total < 2047 && (len = esp_http_client_read(client, resp + total, 2047 - total)) > 0) {
                    total += len;
                }
                resp[total] = 0;
                ok = (esp_http_client_get_status_code(client) >= 200 &&
                      esp_http_client_get_status_code(client) < 300);
                ESP_LOGI(TAG, "OTA http=%d len=%d", esp_http_client_get_status_code(client), total);
            }
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (ok && total > 0) {
        cJSON *root = cJSON_Parse(resp);
        if (root) {
            cJSON *ws = cJSON_GetObjectItem(root, "websocket");
            cJSON *u = ws ? cJSON_GetObjectItem(ws, "url") : NULL;
            cJSON *t = ws ? cJSON_GetObjectItem(ws, "token") : NULL;
            if (cJSON_IsString(u) && u->valuestring[0]) {
                snprintf(s.ws_url, sizeof(s.ws_url), "%s", u->valuestring);
                snprintf(s.token, sizeof(s.token), "%s",
                         (cJSON_IsString(t) && t->valuestring[0]) ? t->valuestring : "");
                ESP_LOGI(TAG, "OTA 取得 ws=%s token_len=%d", s.ws_url, (int)strlen(s.token));
                cJSON_Delete(root);
                heap_caps_free(resp);
                return true;
            }
            /* 服务端可能要求激活：打印原文便于排查 */
            char *dump = cJSON_PrintUnformatted(root);
            ESP_LOGW(TAG, "OTA 无 websocket 字段：%s", dump ? dump : "");
            if (dump) cJSON_free(dump);
            cJSON_Delete(root);
            set_error("OTA 未返回 websocket 地址");
        } else {
            ESP_LOGW(TAG, "OTA 非 JSON：%.120s", resp);
            set_error("OTA 响应解析失败");
        }
    } else {
        set_error("OTA 请求失败");
    }
    heap_caps_free(resp);
    return false;
}

/* ------------------------------------------------------------- 发送文本帧 */

static int ws_send_text(const char *json)
{
    if (!s.ws || !esp_websocket_client_is_connected(s.ws)) return -1;
    return esp_websocket_client_send_text(s.ws, json, (int)strlen(json), pdMS_TO_TICKS(2000));
}

static void send_hello(void)
{
    char buf[320];
    bool mcp = s.cfg ? s.cfg->voice_mcp_enabled : true;
    snprintf(buf, sizeof(buf),
             "{\"type\":\"hello\",\"version\":%d,\"features\":{\"mcp\":%s},"
             "\"transport\":\"websocket\",\"audio_params\":{\"format\":\"opus\","
             "\"sample_rate\":%d,\"channels\":1,\"frame_duration\":%d}}",
             XZ_PROTOCOL_VERSION, mcp ? "true" : "false", MIC_RATE, FRAME_MS);
    ESP_LOGI(TAG, "-> %s", buf);
    ws_send_text(buf);
}

static void send_listen(const char *state, const char *mode)
{
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"%s\",\"mode\":\"%s\"}",
             s.session_id, state, mode);
    ws_send_text(buf);
}

static void send_abort(const char *reason)
{
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"session_id\":\"%s\",\"type\":\"abort\",\"reason\":\"%s\"}",
             s.session_id, reason);
    ws_send_text(buf);
}

/* MCP 工具调用暂未实现，明确回错，避免服务端一直等响应 */
static void send_mcp_error(int id)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"id\":%d,"
             "\"error\":{\"code\":-32601,\"message\":\"not implemented\"}}}",
             s.session_id, id);
    ws_send_text(buf);
}

/* ------------------------------------------------------------- 下行音频播放 */

/* 把解码后的 PCM（down_rate 单声道）线性重采样到 16k，再复制成立体声播放 */
static void play_decoded(int samples)
{
    esp_codec_dev_handle_t play = (esp_codec_dev_handle_t)audio_service_playback_handle();
    if (!play || samples <= 0) return;

    int out_n = MIC_SAMPLES;
    for (int i = 0; i < out_n; i++) {
        float pos = (float)i * (float)samples / (float)out_n;
        int i0 = (int)pos;
        if (i0 > samples - 1) i0 = samples - 1;
        int i1 = (i0 + 1 < samples) ? i0 + 1 : i0;
        float frac = pos - (float)i0;
        int32_t v = s.dec_pcm[i0] + (int32_t)((s.dec_pcm[i1] - s.dec_pcm[i0]) * frac);
        s.out_mono[i] = (opus_int16)v;
    }
    for (int i = 0; i < out_n; i++) {
        s.out_stereo[i * 2] = (int16_t)s.out_mono[i];
        s.out_stereo[i * 2 + 1] = (int16_t)s.out_mono[i];
    }
    esp_codec_dev_write(play, (uint8_t *)s.out_stereo, (uint32_t)(out_n * 2 * sizeof(int16_t)));
}

static void handle_audio(const uint8_t *data, int len)
{
    if (!s.dec) return;
    int samples = opus_decode(s.dec, data, len, s.dec_pcm, MAX_DEC_SAMPLES, 0);
    if (samples <= 0) {
        ESP_LOGW(TAG, "opus_decode 失败：%d", samples);
        return;
    }
    play_decoded(samples);
}

/* ------------------------------------------------------------- 下行 JSON */

static void handle_json(const char *txt, int len)
{
    /* 文本帧不一定以 0 结尾，先拷贝到静态缓冲再解析；超长只截断，避免栈压力 */
    static char copy[1024];
    int n = len < (int)sizeof(copy) - 1 ? len : (int)sizeof(copy) - 1;
    memcpy(copy, txt, (size_t)n);
    copy[n] = 0;

    cJSON *root = cJSON_Parse(copy);
    if (!root) {
        ESP_LOGW(TAG, "非 JSON 文本帧：%.*s", len > 100 ? 100 : len, txt);
        return;
    }
    cJSON *t = cJSON_GetObjectItem(root, "type");
    const char *type = cJSON_IsString(t) ? t->valuestring : "";
    ESP_LOGI(TAG, "<- %.*s", len > 200 ? 200 : len, txt);

    if (strcmp(type, "hello") == 0) {
        cJSON *sid = cJSON_GetObjectItem(root, "session_id");
        if (cJSON_IsString(sid)) snprintf(s.session_id, sizeof(s.session_id), "%s", sid->valuestring);
        cJSON *ap = cJSON_GetObjectItem(root, "audio_params");
        cJSON *sr = ap ? cJSON_GetObjectItem(ap, "sample_rate") : NULL;
        int rate = cJSON_IsNumber(sr) ? sr->valueint : 24000;
        if (rate != s.down_rate || !s.dec) {
            s.down_rate = rate;
            if (s.dec) opus_decoder_destroy(s.dec);
            int err = 0;
            s.dec = opus_decoder_create(rate, 1, &err);
            if (!s.dec || err != OPUS_OK) set_error("Opus 解码器创建失败 rate=%d err=%d", rate, err);
        }
        ESP_LOGI(TAG, "握手完成 session=%s 下行采样率=%d", s.session_id, s.down_rate);
        if (s.events) xEventGroupSetBits(s.events, HELLO_BIT);
    } else if (strcmp(type, "stt") == 0) {
        cJSON *txt2 = cJSON_GetObjectItem(root, "text");
        const char *v = cJSON_IsString(txt2) ? txt2->valuestring : NULL;
        if (v) notify(VOICE_LISTENING, v, NULL);
    } else if (strcmp(type, "llm") == 0) {
        cJSON *emo = cJSON_GetObjectItem(root, "emotion");
        const char *v = cJSON_IsString(emo) ? emo->valuestring : NULL;
        if (v) notify(s.state, NULL, v);
    } else if (strcmp(type, "tts") == 0) {
        cJSON *st = cJSON_GetObjectItem(root, "state");
        const char *v = cJSON_IsString(st) ? st->valuestring : "";
        if (strcmp(v, "start") == 0) {
            notify(VOICE_SPEAKING, NULL, NULL);
        } else if (strcmp(v, "stop") == 0) {
            /* 自动续听：回到采集态，麦克风线程继续推流 */
            notify(VOICE_LISTENING, NULL, NULL);
        } else if (strcmp(v, "sentence_start") == 0) {
            cJSON *txt2 = cJSON_GetObjectItem(root, "text");
            const char *sv = cJSON_IsString(txt2) ? txt2->valuestring : NULL;
            notify(VOICE_SPEAKING, sv, NULL);
        }
    } else if (strcmp(type, "mcp") == 0) {
        cJSON *payload = cJSON_GetObjectItem(root, "payload");
        cJSON *id = payload ? cJSON_GetObjectItem(payload, "id") : NULL;
        cJSON *method = payload ? cJSON_GetObjectItem(payload, "method") : NULL;
        const char *m = cJSON_IsString(method) ? method->valuestring : "";
        if (strcmp(m, "tools/call") == 0) {
            ESP_LOGW(TAG, "MCP 工具调用暂未实现，回错：%s", m);
        }
        send_mcp_error(cJSON_IsNumber(id) ? id->valueint : 0);
    } else if (strcmp(type, "system") == 0) {
        cJSON *cmd = cJSON_GetObjectItem(root, "command");
        const char *v = cJSON_IsString(cmd) ? cmd->valuestring : "";
        ESP_LOGW(TAG, "服务端 system 指令：%s", v);
        if (strcmp(v, "reboot") == 0) esp_restart();
    } else if (strcmp(type, "alert") == 0) {
        cJSON *msg = cJSON_GetObjectItem(root, "message");
        ESP_LOGW(TAG, "alert: %s", cJSON_IsString(msg) ? msg->valuestring : "");
    }
    cJSON_Delete(root);
}

/* ------------------------------------------------------------- WS 事件 */

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket 已连接");
        send_hello();
        break;
    case WEBSOCKET_EVENT_DATA:
        if (!data) break;
        if (data->op_code == 0x2) {
            /* 二进制帧：Opus 音频。分片帧直接丢弃，避免拼接逻辑引入杂音 */
            if (data->payload_len != data->data_len) {
                ESP_LOGW(TAG, "分片音频帧 %d/%d 忽略", data->data_len, data->payload_len);
                break;
            }
            handle_audio((const uint8_t *)data->data_ptr, data->data_len);
        } else if (data->op_code == 0x1) {
            if (data->payload_len != data->data_len) {
                ESP_LOGW(TAG, "分片文本帧 %d/%d 忽略", data->data_len, data->payload_len);
                break;
            }
            handle_json(data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket 断开");
        s.mic_run = false;
        notify(VOICE_IDLE, NULL, NULL);
        break;
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGW(TAG, "WebSocket 关闭");
        s.mic_run = false;
        notify(VOICE_IDLE, NULL, NULL);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket 错误");
        s.mic_run = false;
        notify(VOICE_IDLE, NULL, NULL);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------- 麦克风推流 */

static void mic_task(void *arg)
{
    ESP_LOGI(TAG, "麦克风任务已启动");
    while (s.mic_run) {
        if (s.state != VOICE_LISTENING) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        esp_codec_dev_handle_t rec = (esp_codec_dev_handle_t)audio_service_record_handle();
        if (!rec) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        int rc = esp_codec_dev_read(rec, (uint8_t *)s.in_stereo,
                                    (uint32_t)(MIC_SAMPLES * 2 * sizeof(int16_t)));
        if (rc != ESP_CODEC_DEV_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        /* 录音句柄是 16k/2ch，取左声道做单声道编码 */
        for (int i = 0; i < MIC_SAMPLES; i++) s.in_mono[i] = s.in_stereo[i * 2];

        lock();
        if (s.mic_run && s.enc && s.ws && esp_websocket_client_is_connected(s.ws)) {
            int n = opus_encode(s.enc, s.in_mono, MIC_SAMPLES, s.opus_buf, OPUS_OUT_MAX);
            if (n > 0) {
                int sent = esp_websocket_client_send_bin(s.ws, (const char *)s.opus_buf, n,
                                                         pdMS_TO_TICKS(500));
                if (sent < 0) ESP_LOGW(TAG, "音频帧发送失败");
            } else {
                ESP_LOGW(TAG, "opus_encode 失败：%d", n);
            }
        }
        unlock();
    }
    s.mic_task = NULL;
    vTaskDelete(NULL);
}

/* 创建采集任务：Opus 编码所需栈远大于内部 RAM 余量（设备内部常只剩二十几 KB），
 * 所以优先把栈放到 PSRAM，失败再退回内部小栈。 */
static BaseType_t create_mic_task(void)
{
    static const int psram_sizes[] = {32768, 24576, 16384};
    for (int i = 0; i < (int)(sizeof(psram_sizes) / sizeof(psram_sizes[0])); i++) {
        BaseType_t r = xTaskCreatePinnedToCoreWithCaps(mic_task, "voice_mic", psram_sizes[i],
                                                       NULL, 6, &s.mic_task, 1, MALLOC_CAP_SPIRAM);
        if (r == pdPASS) {
            ESP_LOGI(TAG, "麦克风任务已创建，栈=%d（PSRAM）", psram_sizes[i]);
            return r;
        }
        ESP_LOGW(TAG, "PSRAM 栈 %d 创建失败", psram_sizes[i]);
    }
    static const int int_sizes[] = {10240, 8192, 6144};
    for (int i = 0; i < (int)(sizeof(int_sizes) / sizeof(int_sizes[0])); i++) {
        BaseType_t r = xTaskCreatePinnedToCore(mic_task, "voice_mic", int_sizes[i],
                                               NULL, 6, &s.mic_task, 1);
        if (r == pdPASS) {
            ESP_LOGI(TAG, "麦克风任务已创建，栈=%d（内部）", int_sizes[i]);
            return r;
        }
    }
    return pdFAIL;
}

/* ------------------------------------------------------------- 生命周期 */

static void teardown(bool keep_error)
{
    s.mic_run = false;
    if (s.mic_task) {
        /* 等采集线程退出，避免它继续用已释放的编码器 */
        for (int i = 0; i < 30 && s.mic_task; i++) vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s.ws) {
        esp_websocket_client_stop(s.ws);
        esp_websocket_client_destroy(s.ws);
        s.ws = NULL;
    }
    if (s.enc) { opus_encoder_destroy(s.enc); s.enc = NULL; }
    if (s.dec) { opus_decoder_destroy(s.dec); s.dec = NULL; }
    s.session_id[0] = 0;
    if (!keep_error) s.last_err[0] = 0;
    notify(VOICE_IDLE, NULL, NULL);
}

void voice_service_init(const app_config_t *cfg)
{
    if (s.inited) return;
    memset(&s, 0, sizeof(s));
    s.cfg = cfg;
    s.mutex = xSemaphoreCreateMutex();
    s.events = xEventGroupCreate();
    s.state = VOICE_IDLE;
    s.down_rate = MIC_RATE;
    ensure_device_id();
    ensure_client_id();
    s.inited = true;
    ESP_LOGI(TAG, "语音服务就绪 device=%s client=%s", s.device_id, s.client_id);
}

void voice_service_set_event_cb(voice_event_cb_t cb, void *ctx)
{
    s.cb = cb;
    s.cb_ctx = ctx;
}

bool voice_service_start(void)
{
    if (!s.inited) {
        set_error("语音服务未初始化");
        return false;
    }
    lock();
    if (s.ws) {
        unlock();
        ESP_LOGW(TAG, "已有会话在跑，先停止再重开");
        voice_service_stop();
        lock();
    }
    s.last_err[0] = 0;

    if (s.cfg) audio_service_set_volume(s.cfg->voice_volume);

    if (!alloc_buffers()) { unlock(); return false; }
    if (!ota_fetch()) { unlock(); return false; }

    if (s.ws_url[0] == 0) {
        unlock();
        set_error("没有可用的 WebSocket 地址");
        return false;
    }
    notify(VOICE_CONNECTING, NULL, NULL);

    esp_websocket_client_config_t cfg = {
        .uri = s.ws_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        /* 事件回调里要跑 opus_decode + 重采样 + 写 codec，栈给足；
         * 但内部 RAM 紧张（WiFi+BLE+LVGL 已吃掉大半），不能无限加。 */
        .task_stack = 8192,
        .task_prio = 6,
        .network_timeout_ms = 10000,
        .ping_interval_sec = 10,
        .disable_auto_reconnect = true,
        .user_context = &s,
    };
    s.ws = esp_websocket_client_init(&cfg);
    if (!s.ws) {
        unlock();
        set_error("WebSocket 客户端初始化失败");
        return false;
    }

    char hdr[192];
    snprintf(hdr, sizeof(hdr), "Bearer %s", s.token);
    esp_websocket_client_append_header(s.ws, "Authorization", hdr);
    snprintf(hdr, sizeof(hdr), "%d", XZ_PROTOCOL_VERSION);
    esp_websocket_client_append_header(s.ws, "Protocol-Version", hdr);
    esp_websocket_client_append_header(s.ws, "Device-Id", s.device_id);
    esp_websocket_client_append_header(s.ws, "Client-Id", s.client_id);

    esp_websocket_register_events(s.ws, WEBSOCKET_EVENT_ANY, ws_event_handler, &s);

    if (s.events) xEventGroupClearBits(s.events, HELLO_BIT);
    esp_err_t err = esp_websocket_client_start(s.ws);
    if (err != ESP_OK) {
        unlock();
        teardown(false);
        set_error("WebSocket 启动失败：%s", esp_err_to_name(err));
        return false;
    }

    /* 等服务端 hello，协议建议 10 秒超时 */
    bool hello_ok = false;
    if (s.events) {
        EventBits_t bits = xEventGroupWaitBits(s.events, HELLO_BIT, pdTRUE, pdTRUE,
                                               pdMS_TO_TICKS(10000));
        hello_ok = (bits & HELLO_BIT) != 0;
    }
    if (!hello_ok) {
        unlock();
        teardown(false);
        set_error("等待服务端 hello 超时");
        return false;
    }

    int opus_err = OPUS_OK;
    s.enc = opus_encoder_create(MIC_RATE, 1, OPUS_APPLICATION_VOIP, &opus_err);
    if (!s.enc || opus_err != OPUS_OK) {
        unlock();
        teardown(false);
        set_error("Opus 编码器创建失败 err=%d", opus_err);
        return false;
    }
    opus_encoder_ctl(s.enc, OPUS_SET_BITRATE(24000));
    /* 降复杂度，显著减少编码栈占用与 CPU 开销，对语音识别足够 */
    opus_encoder_ctl(s.enc, OPUS_SET_COMPLEXITY(0));

    /* 手动触发模式：明确告知服务端开始听 */
    send_listen("start", "manual");
    s.mic_run = true;
    notify(VOICE_LISTENING, NULL, NULL);
    unlock();

    /* Opus 编码 + I2S 读取很吃栈：太小会踩穿栈（InstrFetchProhibited），
     * 太大内部 RAM 又分配不出（设备内部空闲通常只有几十 KB），所以逐级退让。 */
    ESP_LOGI(TAG, "建任务前 内部空闲=%u PSRAM空闲=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    BaseType_t r = create_mic_task();
    if (r != pdPASS) {
        s.mic_task = NULL;
        s.mic_run = false;
        set_error("麦克风任务创建失败（内部 RAM 不足）");
        voice_service_stop();
        return false;
    }
    ESP_LOGI(TAG, "语音会话已开始：%s", s.ws_url);
    return true;
}

void voice_service_stop(void)
{
    if (!s.inited) return;
    lock();
    if (s.ws) {
        send_abort("wake_word_detected");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    unlock();
    teardown(true);
    ESP_LOGI(TAG, "语音会话已结束");
}

voice_state_t voice_service_state(void)
{
    return s.state;
}

const char *voice_service_last_error(void)
{
    return s.last_err[0] ? s.last_err : NULL;
}
