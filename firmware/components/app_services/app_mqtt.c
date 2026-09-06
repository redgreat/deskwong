#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"   /* esp-mqtt 组件头 */
#include "app_mqtt.h"

static const char *TAG = "app_mqtt";
static esp_mqtt_client_handle_t s_client = NULL;
static app_mqtt_state_cb_t s_cb = NULL;
static bool s_connected = false;
static char s_uri[160];

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    if (event_id == MQTT_EVENT_CONNECTED) {
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        if (s_cb) s_cb(true);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        if (s_cb) s_cb(false);
    }
}

void app_mqtt_init(const char *broker, uint16_t port,
                   const char *user, const char *pass,
                   app_mqtt_state_cb_t state_cb) {
    if (broker == NULL || broker[0] == '\0') {
        ESP_LOGW(TAG, "no broker configured, skip");
        return;
    }
    s_cb = state_cb;
    if (strstr(broker, "://")) {
        snprintf(s_uri, sizeof(s_uri), "%s", broker);
    } else {
        snprintf(s_uri, sizeof(s_uri), "mqtt://%s", broker);
    }
    esp_mqtt_client_config_t cfg = {0};
    cfg.broker.address.uri = s_uri;
    cfg.broker.address.port = port;
    if (user && user[0]) {
        cfg.credentials.username = user;
        cfg.credentials.authentication.password = pass;
    }
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

bool app_mqtt_is_connected(void) {
    return s_connected;
}

int app_mqtt_publish(const char *topic, const char *payload, int qos) {
    if (s_client == NULL || !s_connected) return 1;
    int id = esp_mqtt_client_publish(s_client, topic, payload, 0, qos, 0);
    return id < 0 ? 1 : 0;
}
