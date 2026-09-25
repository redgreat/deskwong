#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"   /* esp-mqtt 组件头 */
#include "app_mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include <errno.h>
#include <limits.h>

static const char *TAG = "app_mqtt";
static esp_mqtt_client_handle_t s_client = NULL;
static app_mqtt_state_cb_t s_cb = NULL;
static bool s_connected = false;
static char s_uri[160];
static char s_user[64];
static char s_pass[64];
static uint16_t s_port;
static bool s_uri_has_scheme;
static QueueHandle_t s_ack;
static SemaphoreHandle_t s_publish_lock;
static volatile app_mqtt_status_t s_status = APP_MQTT_UNCONFIGURED;
static char s_status_text[64] = "MQTT 未配置";

static void set_status(app_mqtt_status_t status, const char *text) {
    s_status = status;
    snprintf(s_status_text, sizeof(s_status_text), "%s", text ? text : "");
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    if (event_id == MQTT_EVENT_PUBLISHED) {
        esp_mqtt_event_handle_t event = event_data;
        if (s_ack) xQueueSend(s_ack, &event->msg_id, 0);
    } else if (event_id == MQTT_EVENT_CONNECTED) {
        s_connected = true;
        set_status(APP_MQTT_CONNECTED, "MQTT 已连接");
        ESP_LOGI(TAG, "connected");
        if (s_cb) s_cb(true);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        s_connected = false;
        /* MQTT_EVENT_ERROR carries the useful DNS/socket/CONNACK reason and is
         * normally followed by DISCONNECTED.  Keep that specific reason. */
        if (s_status != APP_MQTT_FAILED)
            set_status(APP_MQTT_FAILED, "MQTT 连接失败");
        ESP_LOGW(TAG, "disconnected");
        if (s_cb) s_cb(false);
    } else if (event_id == MQTT_EVENT_ERROR) {
        esp_mqtt_event_handle_t event = event_data;
        esp_mqtt_error_codes_t *e = event ? event->error_handle : NULL;
        set_status(APP_MQTT_FAILED, "MQTT 连接失败");
        if (e) {
            ESP_LOGE(TAG, "error type=%d tls=0x%x sock=%d (%s) connack=%d",
                     e->error_type, e->esp_tls_last_esp_err, e->esp_transport_sock_errno,
                     e->esp_transport_sock_errno ? strerror(e->esp_transport_sock_errno) : "none",
                     e->connect_return_code);
        } else {
            ESP_LOGE(TAG, "error without details");
        }
    }
}

void app_mqtt_init(const char *broker, uint16_t port,
                   const char *user, const char *pass,
                   app_mqtt_state_cb_t state_cb) {
    if (broker == NULL || broker[0] == '\0') {
        set_status(APP_MQTT_UNCONFIGURED, "MQTT 未配置");
        ESP_LOGW(TAG, "no broker configured, skip");
        return;
    }
    s_cb = state_cb;
    s_port = port;
    s_uri_has_scheme = strstr(broker, "://") != NULL;
    snprintf(s_user, sizeof(s_user), "%s", user ? user : "");
    snprintf(s_pass, sizeof(s_pass), "%s", pass ? pass : "");
    if (s_uri_has_scheme) {
        snprintf(s_uri, sizeof(s_uri), "%s", broker);
    } else {
        snprintf(s_uri, sizeof(s_uri), "mqtt://%s", broker);
    }
    set_status(APP_MQTT_READY, "MQTT 已配置");
}

int app_mqtt_start(void) {
    if (s_status == APP_MQTT_UNCONFIGURED) return 1;
    if (s_client) return 0;
    set_status(APP_MQTT_CONNECTING, "MQTT 连接中");
    s_ack = xQueueCreate(8, sizeof(int));
    s_publish_lock = xSemaphoreCreateMutex();
    if (!s_ack || !s_publish_lock) {
        set_status(APP_MQTT_FAILED, "MQTT 初始化失败");
        return 1;
    }
    esp_mqtt_client_config_t cfg = {0};
    cfg.broker.address.uri = s_uri;
    if (!s_uri_has_scheme) cfg.broker.address.port = s_port;
    cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.task.stack_size = 4096;
    if (s_user[0]) {
        cfg.credentials.username = s_user;
        cfg.credentials.authentication.password = s_pass;
    }
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        set_status(APP_MQTT_FAILED, "MQTT 初始化失败");
        return 1;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    return 0;
}

void app_mqtt_stop(void) {
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_connected = false;
    if (s_ack) { vQueueDelete(s_ack); s_ack = NULL; }
    if (s_publish_lock) { vSemaphoreDelete(s_publish_lock); s_publish_lock = NULL; }
}

typedef struct {
    EventGroupHandle_t events;
    char *message;
    size_t message_len;
} mqtt_test_ctx_t;

#define MQTT_TEST_OK  BIT0
#define MQTT_TEST_END BIT1

static void mqtt_test_handler(void *arg, esp_event_base_t base,
                              int32_t event_id, void *event_data) {
    mqtt_test_ctx_t *ctx = arg;
    if (!ctx || !ctx->events) return;
    if (event_id == MQTT_EVENT_CONNECTED) {
        snprintf(ctx->message, ctx->message_len, "MQTT 校验成功");
        xEventGroupSetBits(ctx->events, MQTT_TEST_OK | MQTT_TEST_END);
    } else if (event_id == MQTT_EVENT_ERROR) {
        esp_mqtt_event_handle_t event = event_data;
        esp_mqtt_error_codes_t *e = event ? event->error_handle : NULL;
        if (e && e->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            snprintf(ctx->message, ctx->message_len, "Broker 拒绝连接，请检查账号密码");
        } else if (e && e->esp_tls_last_esp_err == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME) {
            snprintf(ctx->message, ctx->message_len, "MQTT 域名无法解析");
        } else if (e && e->esp_transport_sock_errno) {
            snprintf(ctx->message, ctx->message_len, "MQTT 网络错误：%s",
                     strerror(e->esp_transport_sock_errno));
        } else {
            snprintf(ctx->message, ctx->message_len, "MQTT 连接失败");
        }
        xEventGroupSetBits(ctx->events, MQTT_TEST_END);
    }
}

int app_mqtt_test_config(const char *broker, uint16_t port, const char *user,
                         const char *pass, int timeout_ms, char *message, size_t message_len) {
    if (!message || message_len == 0) return 1;
    message[0] = 0;
    if (!broker || !broker[0]) {
        snprintf(message, message_len, "MQTT 已关闭");
        return 0;
    }
    if (port == 0 && !strstr(broker, "://")) {
        snprintf(message, message_len, "MQTT 端口无效");
        return 1;
    }
    char uri[160];
    bool has_scheme = strstr(broker, "://") != NULL;
    snprintf(uri, sizeof(uri), has_scheme ? "%s" : "mqtt://%s", broker);
    EventGroupHandle_t events = xEventGroupCreate();
    if (!events) {
        snprintf(message, message_len, "MQTT 校验内存不足");
        return 1;
    }
    mqtt_test_ctx_t ctx = {.events = events, .message = message, .message_len = message_len};
    esp_mqtt_client_config_t cfg = {0};
    cfg.broker.address.uri = uri;
    if (!has_scheme) cfg.broker.address.port = port;
    cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.credentials.username = user && user[0] ? user : NULL;
    cfg.credentials.authentication.password = user && user[0] ? pass : NULL;
    cfg.credentials.client_id = "deskwong-config-test";
    cfg.session.disable_clean_session = false;
    cfg.task.stack_size = 4096;
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    if (!client) {
        vEventGroupDelete(events);
        snprintf(message, message_len, "MQTT 校验初始化失败");
        return 1;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_test_handler, &ctx);
    int rc = esp_mqtt_client_start(client);
    EventBits_t bits = 0;
    if (rc == ESP_OK) bits = xEventGroupWaitBits(events, MQTT_TEST_END, pdFALSE, pdFALSE,
                                                 pdMS_TO_TICKS(timeout_ms));
    if (!(bits & MQTT_TEST_END)) snprintf(message, message_len, "MQTT 连接校验超时");
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    vEventGroupDelete(events);
    return (bits & MQTT_TEST_OK) ? 0 : 1;
}

bool app_mqtt_is_connected(void) {
    return s_connected;
}

int app_mqtt_publish(const char *topic, const char *payload, int qos) {
    if (s_client == NULL || !s_connected) return 1;
    int id = esp_mqtt_client_publish(s_client, topic, payload, 0, qos, 0);
    return id < 0 ? 1 : 0;
}

app_mqtt_status_t app_mqtt_status(void) { return s_status; }
const char *app_mqtt_status_text(void) { return s_status_text; }

int app_mqtt_publish_bytes_confirmed(const char *topic, const void *payload,
                                     size_t payload_len, int timeout_ms) {
    if (!s_client || !s_connected || !s_publish_lock || !payload || payload_len == 0 ||
        payload_len > INT_MAX) return 1;
    if (xSemaphoreTake(s_publish_lock, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return 1;
    xQueueReset(s_ack);
    int id = esp_mqtt_client_enqueue(s_client, topic, payload, (int)payload_len, 1, 0, true);
    int rc = 1, ack;
    TickType_t start = xTaskGetTickCount(), limit = pdMS_TO_TICKS(timeout_ms);
    while (id >= 0 && xTaskGetTickCount() - start < limit) {
        TickType_t left = limit - (xTaskGetTickCount() - start);
        if (xQueueReceive(s_ack, &ack, left) != pdTRUE) break;
        if (ack == id) { rc = 0; break; }
    }
    xSemaphoreGive(s_publish_lock);
    return rc;
}

int app_mqtt_publish_confirmed(const char *topic, const char *payload, int timeout_ms) {
    return payload ? app_mqtt_publish_bytes_confirmed(topic, payload, strlen(payload), timeout_ms) : 1;
}
