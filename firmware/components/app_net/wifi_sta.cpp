#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "wifi_sta.h"

static const char *TAG = "wifi";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define AP_MODE_BIT        BIT2

static EventGroupHandle_t s_wifi_events;
static bool s_ap_mode = false;
static char s_ip[16] = {0};

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!s_ap_mode) {
            ESP_LOGW(TAG, "STA disconnected, retry");
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got ip: %s", s_ip);
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void start_ap_mode(void) {
    s_ap_mode = true;
    ESP_LOGI(TAG, "start AP mode: deskwong-setup");
    xEventGroupSetBits(s_wifi_events, AP_MODE_BIT);
}

esp_err_t wifi_init(const char *ssid, const char *pass) {
    s_wifi_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set("deskwong");
    mdns_instance_name_set("deskwong 桌面摆件");

    if (ssid == NULL || ssid[0] == '\0') {
        /* 未配置 WiFi：进入 AP 模式，供手机/电脑连接后配置 */
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
        wifi_config_t ap_conf = {0};
        strcpy((char *)ap_conf.ap.ssid, "deskwong-setup");
        ap_conf.ap.ssid_len = strlen("deskwong-setup");
        ap_conf.ap.max_connection = 4;
        ap_conf.ap.authmode = WIFI_AUTH_OPEN;
        ap_conf.ap.channel = 6;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_conf));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_netif_ip_info_t ip = {0};
        ip.ip.addr = 0x0104A8C0; // 192.168.4.1
        ip.netmask.addr = 0x00FFFFFF;
        ip.gw.addr = 0x0104A8C0;
        esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (ap) esp_netif_set_ip_info(ap, &ip);
        snprintf(s_ip, sizeof(s_ip), "192.168.4.1");
        start_ap_mode();
        return ESP_OK;
    }

    /* STA 模式连接公司内网 */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t sta_conf = {0};
    strncpy((char *)sta_conf.sta.ssid, ssid, sizeof(sta_conf.sta.ssid) - 1);
    strncpy((char *)sta_conf.sta.password, pass, sizeof(sta_conf.sta.password) - 1);
    sta_conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_conf));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "connecting to %s ...", ssid);
    return ESP_OK;
}

bool wifi_is_connected(void) {
    if (s_wifi_events == NULL) return false;
    return (xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_is_ap_mode(void) {
    return s_ap_mode;
}

void wifi_get_ip(char *ip, size_t len) {
    strncpy(ip, s_ip, len - 1);
    ip[len - 1] = 0;
}
