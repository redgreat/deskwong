/* RaceBox BLE 客户端（NimBLE 中央）
 * 连接 Nordic UART 服务（NUS），读写 UBX 命令
 * 协议见 doc/rules/04 §2.2 */
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gatt/ble_svc_gatt.h"
#include "racebox_ble.h"

static const char *TAG = "racebox_ble";

/* Nordic UART Service UUID */
static const ble_uuid128_t nus_svc_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t nus_rx_uuid = BLE_UUID128_INIT(  /* 写特征（下发命令） */
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t nus_tx_uuid = BLE_UUID128_INIT(  /* 通知特征（接收数据） */
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_rx_chr = 0;   /* 写命令的特征 */
static uint16_t s_tx_chr = 0;   /* 收通知的特征 */
static bool s_connected = false;
static racebox_ble_rx_cb_t s_rx_cb = NULL;
static racebox_ble_conn_cb_t s_conn_cb = NULL;
static bool s_scanning = false;

static void set_connected(bool c) {
    if (s_connected != c) {
        s_connected = c;
        if (s_conn_cb) s_conn_cb(c);
    }
}

static int gap_event(struct ble_gap_event *event, void *arg);

/* 特征发现回调：记录 RX/TX 句柄并订阅 TX 通知 */
static int disc_chr_fn(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg) {
    if (error->status != 0) return 0;
    if (ble_uuid_cmp(&chr->uuid.u, &nus_rx_uuid.u) == 0) {
        s_rx_chr = chr->val_handle;
        ESP_LOGI(TAG, "found RX chr handle=%d", s_rx_chr);
    } else if (ble_uuid_cmp(&chr->uuid.u, &nus_tx_uuid.u) == 0) {
        s_tx_chr = chr->val_handle;
        ESP_LOGI(TAG, "found TX chr handle=%d", s_tx_chr);
        /* 订阅通知（CCCD 在特征 handle + 1） */
        uint8_t value[2] = {0x01, 0x00};
        ble_gattc_write_flat(conn_handle, s_tx_chr + 1, value, sizeof(value), NULL, NULL);
        ESP_LOGI(TAG, "subscribed TX notifications");
    }
    return 0;
}

/* 服务发现回调：找到 NUS 服务后枚举其特征 */
static int disc_svc_fn(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *svc, void *arg) {
    if (error->status != 0) {
        ESP_LOGW(TAG, "svc disc status=%d", error->status);
        return 0;
    }
    if (ble_uuid_cmp(&svc->uuid.u, &nus_svc_uuid.u) == 0) {
        ESP_LOGI(TAG, "found NUS service");
        ble_gattc_disc_all_chrs(conn_handle, svc->start_handle, svc->end_handle,
                                disc_chr_fn, NULL);
    }
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        /* 扫描到设备：按名字或服务 UUID 过滤 */
        if (s_scanning) {
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc == 0) {
                bool match = false;
                if (fields.name_len > 0) {
                    match = (strncasecmp((char *)fields.name, "RaceBox", fields.name_len) == 0 ||
                             strncasecmp((char *)fields.name, "racebox", fields.name_len) == 0);
                }
                if (!match && fields.num_uuids16 > 0) {
                    /* NUS 常见 16 位短 UUID 0x0001? 此处主要靠名字匹配 */
                    match = false;
                }
                if (match) {
                    ESP_LOGI(TAG, "found RaceBox, connecting...");
                    s_scanning = false;
                    ble_gap_disc_cancel();
                    ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, 30000, NULL,
                                    gap_event, NULL);
                }
            }
        }
        return 0;
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "connected");
            set_connected(true);
            ble_gattc_disc_all_svcs(s_conn_handle, disc_svc_fn, NULL);
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected");
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_rx_chr = s_tx_chr = 0;
        set_connected(false);
        return 0;
    case BLE_GAP_EVENT_NOTIFY_RX: {
        /* 收到 TX 特征通知（UBX 数据） */
        struct os_mbuf *om = event->notify_rx.om;
        uint8_t buf[512];
        int copied = 0;
        while (om && copied < (int)sizeof(buf)) {
            int n = om->om_len;
            if (n > (int)sizeof(buf) - copied) n = sizeof(buf) - copied;
            memcpy(buf + copied, om->om_data, n);
            copied += n;
            om = om->om_next.sle_next;
        }
        if (s_rx_cb) s_rx_cb(buf, copied);
        return 0;
    }
    default:
        return 0;
    }
}

static void on_sync(void) {
    ESP_LOGI(TAG, "nimble synced");
}

void racebox_ble_init(void) {
    nimble_port_init();
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = NULL;
    nimble_port_freertos_init(NULL);
    ESP_LOGI(TAG, "ble init done");
}

void racebox_ble_start(void) {
    if (s_scanning) return;
    s_scanning = true;
    struct ble_gap_disc_params dp = {0};
    dp.filter_duplicates = 1;
    dp.passive = 0;
    dp.itvl = 0x0050;
    dp.window = 0x0050;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &dp, gap_event, NULL);
    ESP_LOGI(TAG, "scan start rc=%d", rc);
}

void racebox_ble_stop(void) {
    if (s_connected) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    s_scanning = false;
    ble_gap_disc_cancel();
}

bool racebox_ble_is_connected(void) {
    return s_connected;
}

int racebox_ble_send(const uint8_t *cmd, int len) {
    if (!s_connected || s_rx_chr == 0) return 1;
    int rc = ble_gattc_write_flat(s_conn_handle, s_rx_chr, (void *)cmd, len, NULL, NULL);
    return rc == 0 ? 0 : 1;
}

void racebox_ble_set_rx_cb(racebox_ble_rx_cb_t cb) {
    s_rx_cb = cb;
}

void racebox_ble_set_conn_cb(racebox_ble_conn_cb_t cb) {
    s_conn_cb = cb;
}
