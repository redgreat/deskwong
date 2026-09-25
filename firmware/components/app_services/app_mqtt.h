#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_mqtt_state_cb_t)(bool connected);

typedef enum {
    APP_MQTT_UNCONFIGURED = 0,
    APP_MQTT_READY,
    APP_MQTT_CONNECTING,
    APP_MQTT_CONNECTED,
    APP_MQTT_FAILED,
} app_mqtt_status_t;

void app_mqtt_init(const char *broker, uint16_t port,
                   const char *user, const char *pass,
                   app_mqtt_state_cb_t state_cb);
bool app_mqtt_is_connected(void);
app_mqtt_status_t app_mqtt_status(void);
const char *app_mqtt_status_text(void);
int app_mqtt_start(void);
void app_mqtt_stop(void);
/* Validate broker reachability and credentials before persisting web config. */
int app_mqtt_test_config(const char *broker, uint16_t port, const char *user,
                         const char *pass, int timeout_ms, char *message, size_t message_len);
/* 发布消息，qos 0/1/2；成功返回 0 */
int app_mqtt_publish(const char *topic, const char *payload, int qos);
/* Wait for broker PUBACK, not merely insertion into the MQTT outbox. */
int app_mqtt_publish_confirmed(const char *topic, const char *payload, int timeout_ms);
/* Binary-safe QoS 1 publish. Returns only after the broker PUBACK arrives. */
int app_mqtt_publish_bytes_confirmed(const char *topic, const void *payload,
                                     size_t payload_len, int timeout_ms);

#ifdef __cplusplus
}
#endif
