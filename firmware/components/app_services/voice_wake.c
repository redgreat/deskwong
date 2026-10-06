/*
 * voice_wake.c —— WakeNet9 唤醒词常驻监听（"你好小智"）
 *
 * 直接用 esp_wn_iface（不走 AFE 流水线）：单麦克风近讲场景用不到
 * AEC/NS/VAD/SE，AFE 的内部任务和环形缓冲要吃 ~23KB 内部 RAM，而这块
 * RAM 正是显示队列（ESP_ERR_NO_MEM）的稀缺资源。xiaozhi-esp32 官方的
 * EspWakeWord 同样是直接调 wn 接口。
 *
 * 单任务循环：读录音句柄 512 采样（32ms）→ 取左声道 → wakenet detect。
 * 命中后：停读 codec（与会话 mic_task 句柄交接）→ 播提示音 →
 * voice_service_start_by_wake()；失败延时后自动恢复，成功则保持暂停，
 * 会话 teardown 时由 voice_service 调 voice_wake_resume()。
 *
 * 门控（voice_enabled + listen_mode + WiFi + 会话态）每轮检查，
 * 网页改配置即刻生效，无需重建。
 */
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"

#include "esp_codec_dev.h"
#include "esp_netif.h"
#include "app_config.h"
#include "audio_service.h"
#include "voice_service.h"
#include "voice_wake.h"

static const char *TAG = "voice_wake";

typedef struct {
    bool inited;
    const app_config_t *cfg;

    srmodel_list_t *models;
    const esp_wn_iface_t *wn;
    model_iface_data_t *wn_data;
    int chunk;             /* 每次 detect 的单声道采样数（约 512 = 32ms） */
    int16_t *stereo_buf;   /* codec 读取缓冲 chunk*2ch */
    int16_t *mono_buf;     /* 左声道 → wakenet */

    volatile bool suspended;   /* 会话期间暂停监听（含唤醒触发后） */
    volatile int  park_gen;    /* 任务每次进入停驻分支 +1，suspend 等它变化 */
    volatile bool in_read;     /* 正在 esp_codec_dev_read */
    volatile bool armed;       /* 防重触发：clean 后见到一帧未命中才允许再触发 */
    int64_t last_trigger_us;
    uint32_t candidate_hits;
    uint32_t cooldown_drops;

    TaskHandle_t task;
} voice_wake_ctx_t;

static voice_wake_ctx_t s;

/* ------------------------------------------------------------------ 门控 */

/* STA 网卡是否已联网（等价 wifi_is_connected，但避免 app_services→app_net 环） */
static bool sta_up(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    return sta && esp_netif_is_netif_up(sta);
}

static bool can_listen(void)
{
    if (!s.inited || !s.cfg || s.suspended) return false;
    if (!s.cfg->voice_enabled) return false;
    /* 0=唤醒词 1=长按 2=两者 */
    if (s.cfg->voice_listen_mode != 0 && s.cfg->voice_listen_mode != 2) return false;
    if (!sta_up()) return false;
    if (voice_service_state() != VOICE_IDLE) return false;
    return true;
}

/* ------------------------------------------------------------ 监听任务 */

static void wake_task(void *arg)
{
    esp_codec_dev_handle_t rec = (esp_codec_dev_handle_t)audio_service_record_handle();
    if (!rec) {
        ESP_LOGE(TAG, "录音句柄不可用，唤醒监听退出");
        vTaskDelete(NULL);
        return;
    }
    s.armed = false;
    uint32_t frames = 0;
    int64_t last_diag_us = esp_timer_get_time();
    int64_t sum_sq = 0;
    int peak = 0;
    int max_detect_us = 0;
    for (;;) {
        if (!can_listen()) {
            s.in_read = false;
            s.park_gen++;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        s.in_read = true;
        int rc = esp_codec_dev_read(rec, (uint8_t *)s.stereo_buf,
                                    (uint32_t)(s.chunk * 2 * sizeof(int16_t)));
        s.in_read = false;
        if (rc != ESP_CODEC_DEV_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!can_listen()) continue;   /* 读完后门控可能已变化（如会话启动） */

        /* 16k/2ch 取左声道（单麦克风）给 wakenet */
        for (int i = 0; i < s.chunk; i++) {
            int v = s.stereo_buf[i * 2];
            int a = v < 0 ? -v : v;
            s.mono_buf[i] = (int16_t)v;
            sum_sq += (int64_t)v * v;
            if (a > peak) peak = a;
        }
        int64_t detect_begin_us = esp_timer_get_time();
        int r = s.wn->detect(s.wn_data, s.mono_buf);
        int detect_us = (int)(esp_timer_get_time() - detect_begin_us);
        if (detect_us > max_detect_us) max_detect_us = detect_us;
        frames++;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_diag_us >= 10000000) {
            double rms = frames ? sqrt((double)sum_sq / ((double)frames * s.chunk)) : 0.0;
            ESP_LOGI(TAG, "health frames=%u rms=%.1f peak=%d detect_max=%dus stack_free=%u core=%d",
                     (unsigned)frames, rms, peak, max_detect_us,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL), xPortGetCoreID());
            frames = 0;
            sum_sq = 0;
            peak = 0;
            max_detect_us = 0;
            last_diag_us = now_us;
        }
        if (r <= 0) {
            s.armed = true;   /* 先见过未命中帧才允许触发，防恢复瞬间误触发 */
            continue;
        }
        /* 注意：命中后绝不能调 wn->clean()——wn9 量化模型的 clean 内部
         * dl_convq_queue_bzero 会解引用空指针直接 panic（LoadProhibited，
         * addr2line 实锤）。唤醒状态机发完一次 DETECTED 后自动复位，
         * 防重触发交给 armed 标志。 */
        s.candidate_hits++;
        if (!s.armed) continue;
        if (s.last_trigger_us > 0 && now_us - s.last_trigger_us < 3000000) {
            s.cooldown_drops++;
            ESP_LOGW(TAG, "wake candidate ignored by 3s cooldown candidates=%u drops=%u",
                     (unsigned)s.candidate_hits, (unsigned)s.cooldown_drops);
            continue;
        }
        s.last_trigger_us = now_us;

        ESP_LOGI(TAG, "唤醒词命中，启动语音会话（wake栈余量=%u 字）",
                 (unsigned)uxTaskGetStackHighWaterMark(NULL));
        /* 停读并停稳，把录音句柄让给会话的 mic_task */
        s.suspended = true;
        int gen = s.park_gen;
        int waited = 0;
        while (s.park_gen == gen && waited < 1000) {
            vTaskDelay(pdMS_TO_TICKS(20));
            waited += 20;
        }

        audio_service_cue(AUDIO_CUE_START);
        /* 异步启动：会话建立含 OTA TLS 握手，不能压在本任务栈上（会爆栈重启）。
         * 失败时 voice_service 的清理路径会 resume 回来；成功则保持暂停，
         * 会话 teardown 后同样由 voice_service 调 resume。 */
        voice_service_request_start(true);
        vTaskDelay(pdMS_TO_TICKS(1000));   /* 避开提示音与唤醒词尾音 */
    }
}

/* ------------------------------------------------------------- 生命周期 */

void voice_wake_init(const app_config_t *cfg)
{
    if (s.inited) return;

    ESP_LOGI(TAG, "初始化前 内部空闲=%u PSRAM空闲=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    s.models = esp_srmodel_init("model");
    if (!s.models || s.models->num == 0) {
        ESP_LOGE(TAG, "model 分区无模型——检查 srmodels.bin 是否已烧录");
        return;
    }
    char *wn_name = esp_srmodel_filter(s.models, ESP_WN_PREFIX, NULL);
    if (!wn_name) {
        ESP_LOGE(TAG, "model 分区里没有唤醒词模型——检查 CONFIG_SR_WN_* 配置");
        return;
    }
    s.wn = esp_wn_handle_from_name(wn_name);
    if (!s.wn) {
        ESP_LOGE(TAG, "唤醒引擎获取失败：%s", wn_name);
        return;
    }
    s.wn_data = s.wn->create(wn_name, DET_MODE_95);
    if (!s.wn_data) {
        ESP_LOGE(TAG, "唤醒引擎创建失败（内存不足？）");
        return;
    }
    int threshold_percent = (cfg && cfg->voice_wake_threshold >= 40 && cfg->voice_wake_threshold <= 99)
                                ? cfg->voice_wake_threshold : 95;
    int threshold_rc = s.wn->set_det_threshold(s.wn_data, threshold_percent / 100.0f, 1);
    float actual_threshold = s.wn->get_det_threshold(s.wn_data, 1);
    s.chunk = s.wn->get_samp_chunksize(s.wn_data);
    ESP_LOGI(TAG, "唤醒模型：%s chunk=%d 采样（%dms） rate=%d threshold=%.2f set_rc=%d cooldown=3000ms", wn_name,
             s.chunk, s.chunk * 1000 / s.wn->get_samp_rate(s.wn_data),
             s.wn->get_samp_rate(s.wn_data), actual_threshold, threshold_rc);

    s.stereo_buf = (int16_t *)heap_caps_malloc(s.chunk * 2 * sizeof(int16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s.mono_buf = (int16_t *)heap_caps_malloc(s.chunk * sizeof(int16_t),
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s.stereo_buf || !s.mono_buf) {
        ESP_LOGE(TAG, "音频缓冲分配失败");
        return;
    }

    s.cfg = cfg;
    s.inited = true;

    ESP_LOGI(TAG, "初始化后 内部空闲=%u PSRAM空闲=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* 读 codec + wakenet 推理同任务串行：推理 32ms 预算内完成即可跟上实时。
     * 不碰 NVS/flash，PSRAM 栈安全（同 mic_task 先例）；wn9 推理栈给 20KB。 */
    /* WiFi/TCPIP/NimBLE 固定在 core 0。WakeNet 若也以高优先级固定到 core 0，
     * 连续推理会直接拖慢 OTA/WSS，表现为唤醒后界面卡住且服务端收不到音频。
     * 放到 core 1，优先级低于 UI/按键；codec read 会自然阻塞让出 CPU。 */
    BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(wake_task, "voice_wake", 20480,
                                                    NULL, 2, &s.task, 1, MALLOC_CAP_SPIRAM);
    if (ok != pdPASS) {
        ok = xTaskCreatePinnedToCore(wake_task, "voice_wake", 10240, NULL, 2, &s.task, 1);
    }
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "唤醒任务创建失败");
        return;
    }
}

bool voice_wake_suspend(int timeout_ms)
{
    if (!s.inited) return true;
    if (!s.task) return true;
    if (s.suspended) return true;   /* 已停（如唤醒任务自己发起的会话），无需再等 */
    int gen = s.park_gen;
    s.suspended = true;
    int waited = 0;
    while (s.park_gen == gen && waited < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(20));
        waited += 20;
    }
    if (s.park_gen != gen) return true;
    /* 兜底：任务可能正卡在一次 codec 读上 */
    return !s.in_read;
}

void voice_wake_resume(void)
{
    if (!s.inited) return;
    s.suspended = false;
    s.armed = false;   /* 恢复后先见到未命中帧再武装，防 stale 状态误触发 */
}
