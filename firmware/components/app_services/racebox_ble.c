/* RaceBox BLE 客户端（NimBLE 中央）
 * 连接 Nordic UART 服务（NUS），读写 UBX 命令
 * 协议见 doc/rules/04 §2.2 */
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "host/ble_hs.h"
#include "host/ble_att.h"
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
static bool s_link_connected = false;
static racebox_ble_rx_cb_t s_rx_cb = NULL;
static racebox_ble_conn_cb_t s_conn_cb = NULL;
static racebox_ble_scan_done_cb_t s_scan_done_cb = NULL;
static racebox_ble_disc_cb_t s_disc_cb = NULL;
static bool s_scanning = false;
static bool s_restart_after_disconnect = false;
static char s_name_prefix[64] = "RaceBox";
static char s_name_lock[64] = "";
static char s_peer_name[64] = "";
static uint16_t s_nus_start = 0;
static uint16_t s_nus_end = 0;
static uint32_t s_notify_count = 0;

void racebox_ble_peer_name(char *out, size_t len) {
    if (!out || len == 0) return;
    snprintf(out, len, "%s", s_peer_name);
}

void racebox_ble_set_filter(const char *prefix, const char *locked) {
    if (prefix && prefix[0]) snprintf(s_name_prefix, sizeof(s_name_prefix), "%s", prefix);
    else snprintf(s_name_prefix, sizeof(s_name_prefix), "RaceBox");
    snprintf(s_name_lock, sizeof(s_name_lock), "%s", locked ? locked : "");
}

void racebox_ble_set_scan_done_cb(racebox_ble_scan_done_cb_t cb) { s_scan_done_cb = cb; }
void racebox_ble_set_disc_cb(racebox_ble_disc_cb_t cb) { s_disc_cb = cb; }

/* 设备名匹配：锁定名非空时要求完全一致（忽略大小写），否则按前缀匹配 */
static bool name_matches(const char *name, int len) {
    if (!name || len <= 0) return false;
    int n = len;
    if (n >= (int)sizeof(s_peer_name)) n = (int)sizeof(s_peer_name) - 1;
    if (s_name_lock[0]) {
        return strlen(s_name_lock) == (size_t)n && strncasecmp(name, s_name_lock, (size_t)n) == 0;
    }
    size_t plen = strlen(s_name_prefix);
    return (size_t)n >= plen && strncasecmp(name, s_name_prefix, plen) == 0;
}

static void set_connected(bool c) {
    if (s_connected != c) {
        s_connected = c;
        if (s_conn_cb) s_conn_cb(c);
    }
}

static int gap_event(struct ble_gap_event *event, void *arg);
static void fail_gatt_setup(const char *step, int status);
static int disc_svc_fn(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *svc, void *arg);

static void start_nus_discovery(uint16_t conn_handle) {
    int rc = ble_gattc_disc_svc_by_uuid(conn_handle, &nus_svc_uuid.u,
                                        disc_svc_fn, NULL);
    if (rc != 0) fail_gatt_setup("NUS service discovery start", rc);
}

static int mtu_done(uint16_t conn_handle, const struct ble_gatt_error *error,
                    uint16_t mtu, void *arg) {
    (void)arg;
    if (error->status == 0) {
        ESP_LOGI(TAG, "ATT MTU negotiated: %u", mtu);
    } else {
        /* RaceBox 也能以默认 MTU 工作；协商失败时继续发现 NUS。 */
        ESP_LOGW(TAG, "ATT MTU exchange failed status=%d, using %u",
                 error->status, ble_att_mtu(conn_handle));
    }
    start_nus_discovery(conn_handle);
    return 0;
}

static void fail_gatt_setup(const char *step, int status) {
    ESP_LOGE(TAG, "%s failed status=%d", step, status);
    if (s_conn_cb) s_conn_cb(false);
    if (s_link_connected && s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

static int subscribe_done(uint16_t conn_handle, const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attr, void *arg) {
    (void)conn_handle; (void)attr; (void)arg;
    if (error->status != 0) {
        fail_gatt_setup("TX subscribe", error->status);
        return 0;
    }
    ESP_LOGI(TAG, "NUS ready (RX=%u TX=%u)", s_rx_chr, s_tx_chr);
    set_connected(true);
    return 0;
}

/* 特征发现回调：记录 RX/TX 句柄并订阅 TX 通知 */
static int disc_chr_fn(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg) {
    (void)arg;
    if (error->status == BLE_HS_EDONE) {
        if (s_rx_chr == 0 || s_tx_chr == 0) {
            fail_gatt_setup("NUS characteristic discovery", error->status);
            return 0;
        }
        /* CCCD 位于通知特征 value handle 后一个 handle。等订阅写入完成后，
         * 才向业务层报告连接成功，避免命令在 RX handle 尚未发现时丢失。 */
        uint8_t value[2] = {0x01, 0x00};
        int rc = ble_gattc_write_flat(conn_handle, s_tx_chr + 1, value, sizeof(value),
                                      subscribe_done, NULL);
        if (rc != 0) fail_gatt_setup("TX subscribe start", rc);
        return 0;
    }
    if (error->status != 0 || chr == NULL) {
        fail_gatt_setup("NUS characteristic discovery", error->status);
        return 0;
    }
    if (ble_uuid_cmp(&chr->uuid.u, &nus_rx_uuid.u) == 0) {
        s_rx_chr = chr->val_handle;
        ESP_LOGI(TAG, "found RX chr handle=%d", s_rx_chr);
    } else if (ble_uuid_cmp(&chr->uuid.u, &nus_tx_uuid.u) == 0) {
        s_tx_chr = chr->val_handle;
        ESP_LOGI(TAG, "found TX chr handle=%d", s_tx_chr);
    }
    return 0;
}

/* 服务发现回调：找到 NUS 服务后枚举其特征 */
static int disc_svc_fn(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *svc, void *arg) {
    (void)arg;
    if (error->status == BLE_HS_EDONE) {
        if (s_nus_start == 0 || s_nus_end == 0) {
            fail_gatt_setup("NUS service discovery", error->status);
            return 0;
        }
        int rc = ble_gattc_disc_all_chrs(conn_handle, s_nus_start, s_nus_end,
                                         disc_chr_fn, NULL);
        if (rc != 0) fail_gatt_setup("NUS characteristic discovery start", rc);
        return 0;
    }
    if (error->status != 0 || svc == NULL) {
        fail_gatt_setup("NUS service discovery", error->status);
        return 0;
    }
    if (ble_uuid_cmp(&svc->uuid.u, &nus_svc_uuid.u) == 0) {
        ESP_LOGI(TAG, "found NUS service");
        s_nus_start = svc->start_handle;
        s_nus_end = svc->end_handle;
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
            if (rc == 0 && name_matches((const char *)fields.name, fields.name_len)) {
                int n = fields.name_len;
                if (n >= (int)sizeof(s_peer_name)) n = (int)sizeof(s_peer_name) - 1;
                memcpy(s_peer_name, fields.name, (size_t)n);
                s_peer_name[n] = 0;
                ESP_LOGI(TAG, "found device '%s', connecting...", s_peer_name);
                if (s_disc_cb) s_disc_cb(s_peer_name);
                s_scanning = false;
                ble_gap_disc_cancel();
                int connect_rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                                 30000, NULL, gap_event, NULL);
                if (connect_rc != 0) ESP_LOGE(TAG, "connect start failed rc=%d", connect_rc);
            }
        }
        return 0;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        /* 扫描窗口结束仍未连上：通知上层收尾 */
        if (s_scanning) {
            s_scanning = false;
            ESP_LOGW(TAG, "scan finished, no device matched");
            if (s_scan_done_cb) s_scan_done_cb();
        }
        return 0;
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_link_connected = true;
            s_rx_chr = s_tx_chr = 0;
            s_nus_start = s_nus_end = 0;
            ESP_LOGI(TAG, "connected");
            /* 主动协商大 MTU。RaceBox 历史记录会连续高速通知；默认 23 字节
             * MTU 会把一条记录拆成大量 ACL 包，容易耗尽控制器缓冲。 */
            int rc = ble_gattc_exchange_mtu(s_conn_handle, mtu_done, NULL);
            if (rc != 0) {
                ESP_LOGW(TAG, "ATT MTU exchange start failed rc=%d", rc);
                start_nus_discovery(s_conn_handle);
            }
        } else {
            ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
            if (s_conn_cb) s_conn_cb(false);
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected reason=%d notifications=%lu", event->disconnect.reason,
                 (unsigned long)s_notify_count);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_link_connected = false;
        s_rx_chr = s_tx_chr = 0;
        set_connected(false);
        if (s_restart_after_disconnect) {
            s_restart_after_disconnect = false;
            ESP_LOGI(TAG, "restart scan after disconnect");
            racebox_ble_start();
        }
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
        s_notify_count++;
        if (s_notify_count == 1 || s_notify_count % 1000 == 0)
            ESP_LOGI(TAG, "notifications=%lu latest_bytes=%d", (unsigned long)s_notify_count, copied);
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

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started");
    /* Returns only after nimble_port_stop() is called. */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void racebox_ble_init(void) {
    nimble_port_init();
    int mtu_rc = ble_att_set_preferred_mtu(247);
    ESP_LOGI(TAG, "preferred ATT MTU=247 rc=%d", mtu_rc);
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = NULL;
    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "ble init done");
}

void racebox_ble_start(void) {
    if (s_scanning) return;
    if (s_link_connected) {
        /* 取消后可能仍在等待异步断连。先彻底断开，收到 DISCONNECT 后再扫描。 */
        s_restart_after_disconnect = true;
        int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        ESP_LOGI(TAG, "disconnect before rescan rc=%d", rc);
        return;
    }
    s_restart_after_disconnect = false;
    s_notify_count = 0;
    s_scanning = true;
    struct ble_gap_disc_params dp = {0};
    dp.filter_duplicates = 1;
    dp.passive = 0;
    dp.itvl = 0x0050;
    dp.window = 0x0050;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &dp, gap_event, NULL);
    ESP_LOGI(TAG, "scan start rc=%d", rc);
    if (rc != 0) {
        s_scanning = false;
        if (s_scan_done_cb) s_scan_done_cb();
    }
}

void racebox_ble_stop(void) {
    s_restart_after_disconnect = false;
    s_scanning = false;
    ble_gap_disc_cancel();
    if (s_link_connected && s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        ESP_LOGI(TAG, "disconnect requested rc=%d", rc);
    }
}

bool racebox_ble_is_connected(void) {
    return s_connected;
}

int racebox_ble_send(const uint8_t *cmd, int len) {
    if (!s_connected || s_rx_chr == 0) return 1;
    /* Nordic UART RX 支持 Write Without Response；下载期间使用它可避免写请求
     * 占住 GATT procedure，给高速历史通知留出 ACL 缓冲。 */
    int rc = ble_gattc_write_no_rsp_flat(s_conn_handle, s_rx_chr, cmd, (uint16_t)len);
    ESP_LOGI(TAG, "TX %d bytes no-rsp rc=%d", len, rc);
    return rc == 0 ? 0 : 1;
}

void racebox_ble_set_rx_cb(racebox_ble_rx_cb_t cb) {
    s_rx_cb = cb;
}

void racebox_ble_set_conn_cb(racebox_ble_conn_cb_t cb) {
    s_conn_cb = cb;
}
