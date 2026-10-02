#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "wifi_sta.h"
#include "wifi_reason.h"

static const char *TAG = "wifi";

/* 配网热点激活期间 STA 的慢速重试间隔：每次 esp_wifi_connect 都是一轮全信道
 * 扫描（约 2 秒），会压制 softAP 信标导致手机搜不到热点，必须拉开重试间隔。 */
#define AP_ACTIVE_RETRY_US (30ULL * 1000000ULL)

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define AP_MODE_BIT        BIT2

static EventGroupHandle_t s_wifi_events;
static bool s_ap_mode = false;
static char s_ip[16] = {0};
static esp_timer_handle_t s_retry_timer = NULL;

static void retry_timer_cb(void *arg) {
    esp_wifi_connect();
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = (const wifi_event_sta_disconnected_t *)data;
        if (s_ap_mode) {
            ESP_LOGW(TAG, "STA disconnected, reason=%d (%s), retry in 30s (setup AP active)",
                     d->reason, wifi_reason_str(d->reason));
            if (s_retry_timer == NULL) {
                const esp_timer_create_args_t targs = {
                    .callback = retry_timer_cb,
                    .name = "wifi_retry",
                };
                if (esp_timer_create(&targs, &s_retry_timer) != ESP_OK) return;
            }
            esp_timer_stop(s_retry_timer);
            esp_timer_start_once(s_retry_timer, AP_ACTIVE_RETRY_US);
            return;
        }
        ESP_LOGW(TAG, "STA disconnected, reason=%d (%s), retry", d->reason, wifi_reason_str(d->reason));
        esp_wifi_connect();
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
    return wifi_init_ex(ssid, pass, false);
}

/* 填充配网热点配置；netif 创建与 set_config 由调用方统一定序（set_config 须在 set_mode 后） */
static void fill_ap_conf(wifi_config_t *ap_conf) {
    memset(ap_conf, 0, sizeof(*ap_conf));
    strcpy((char *)ap_conf->ap.ssid, "deskwong-setup");
    ap_conf->ap.ssid_len = strlen("deskwong-setup");
    ap_conf->ap.max_connection = 4;
    ap_conf->ap.authmode = WIFI_AUTH_OPEN;
    ap_conf->ap.channel = 6;
}

static void set_ap_ip_info(void) {
    esp_netif_ip_info_t ip = {0};
    ip.ip.addr = 0x0104A8C0; // 192.168.4.1
    ip.netmask.addr = 0x00FFFFFF;
    ip.gw.addr = 0x0104A8C0;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap) esp_netif_set_ip_info(ap, &ip);
}

esp_err_t wifi_init_ex(const char *ssid, const char *pass, bool with_ap) {
    s_wifi_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t mdns_err = mdns_init();
    if (mdns_err == ESP_OK) {
        mdns_hostname_set("deskwong");
        mdns_instance_name_set("deskwong 桌面摆件");
    } else ESP_LOGW(TAG, "mDNS unavailable: %s; use device IP", esp_err_to_name(mdns_err));

    if (ssid == NULL || ssid[0] == '\0') {
        /* 未配置 WiFi：进入 AP 模式，供手机/电脑连接后配置 */
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
        wifi_config_t ap_conf;
        fill_ap_conf(&ap_conf);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_conf));
        ESP_ERROR_CHECK(esp_wifi_start());
        set_ap_ip_info();
        snprintf(s_ip, sizeof(s_ip), "192.168.4.1");
        start_ap_mode();
        return ESP_OK;
    }

    /* STA 连接已保存网络；with_ap 时叠加配网热点（APSTA） */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t sta_conf = {0};
    strncpy((char *)sta_conf.sta.ssid, ssid, sizeof(sta_conf.sta.ssid) - 1);
    strncpy((char *)sta_conf.sta.password, pass, sizeof(sta_conf.sta.password) - 1);
    sta_conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    /* 隐藏 SSID（公司 2.4G 不广播名字）必须全信道扫描：fast scan 匹配不到隐藏 AP */
    sta_conf.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    if (with_ap) {
        /* AP 信道会跟随 STA 关联到的信道，手机可能短暂重连 */
        esp_netif_create_default_wifi_ap();
        wifi_config_t ap_conf;
        fill_ap_conf(&ap_conf);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_conf));
        set_ap_ip_info();
        snprintf(s_ip, sizeof(s_ip), "192.168.4.1");
        start_ap_mode();
        ESP_LOGI(TAG, "provision APSTA: deskwong-setup + STA");
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_conf));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "connecting to %s ...", ssid);
    return ESP_OK;
}

esp_err_t wifi_start_ap(void) {
    if (s_ap_mode) return ESP_OK;
    if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") == NULL) {
        esp_netif_create_default_wifi_ap();
    }
    wifi_config_t ap_conf;
    fill_ap_conf(&ap_conf);
    /* 运行中切模式：出错只记日志不 abort，避免把整块板子带崩 */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start AP: set mode failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &ap_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start AP: set config failed: %s", esp_err_to_name(err));
        return err;
    }
    set_ap_ip_info();
    snprintf(s_ip, sizeof(s_ip), "192.168.4.1");
    start_ap_mode();
    ESP_LOGI(TAG, "setup AP started (APSTA): deskwong-setup + STA");
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
