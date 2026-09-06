#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_mqtt_state_cb_t)(bool connected);

void app_mqtt_init(const char *broker, uint16_t port,
                   const char *user, const char *pass,
                   app_mqtt_state_cb_t state_cb);
bool app_mqtt_is_connected(void);
/* 发布消息，qos 0/1/2；成功返回 0 */
int app_mqtt_publish(const char *topic, const char *payload, int qos);

#ifdef __cplusplus
}
#endif
