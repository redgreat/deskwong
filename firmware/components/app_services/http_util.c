#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "puff.h"       /* zlib 官方极简 inflate（public domain），解 gzip 负载 */
#include "http_util.h"

static const char *TAG = "http";
#define HTTP_BUF_SIZE 16384
/* 静态 16KB 数组会占用宝贵的内部 DRAM，改成从 PSRAM 分配，给 TLS 握手腾地方 */
static char *g_buf = NULL;

static char *ensure_buf(void) {
    if (g_buf) return g_buf;
    g_buf = (char *)heap_caps_malloc(HTTP_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_buf) {
        ESP_LOGW(TAG, "PSRAM alloc failed, fall back to internal");
        g_buf = (char *)heap_caps_malloc(HTTP_BUF_SIZE, MALLOC_CAP_8BIT);
    }
    if (!g_buf) ESP_LOGE(TAG, "http buffer alloc failed");
    return g_buf;
}

/* gzip 头：10 字节固定 + 可选 FEXTRA/FNAME/FCOMMENT/FHCRC */
static int gzip_header_len(const uint8_t *p, int len) {
    if (len < 18 || p[0] != 0x1f || p[1] != 0x8b) return -1;
    int flg = p[3], off = 10;
    if (flg & 0x04) {
        if (off + 2 > len) return -1;
        off += 2 + (p[off] | (p[off + 1] << 8));
    }
    if (flg & 0x08) { while (off < len && p[off]) off++; off++; }
    if (flg & 0x10) { while (off < len && p[off]) off++; off++; }
    if (flg & 0x02) off += 2;
    if (off + 8 > len) return -1;
    return off;
}

/* 和风天气/部分 CDN 即使要求 identity 也照样返回 gzip，这里就地解压回明文 JSON */
static int inflate_gzip(char *buf, int total) {
    int hlen = gzip_header_len((const uint8_t *)buf, total);
    if (hlen < 0) return total;
    unsigned char *out = (unsigned char *)heap_caps_malloc(HTTP_BUF_SIZE,
                                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out) return total;
    unsigned long dlen = HTTP_BUF_SIZE - 1;
    unsigned long slen = (unsigned long)(total - hlen - 8);
    int rc = puff(out, &dlen, (const unsigned char *)(buf + hlen), &slen);
    if (rc != 0 || dlen == 0 || dlen >= HTTP_BUF_SIZE) {
        ESP_LOGW(TAG, "gzip inflate failed rc=%d (%d bytes)", rc, total);
        heap_caps_free(out);
        return total;
    }
    memcpy(buf, out, dlen);
    buf[dlen] = 0;
    heap_caps_free(out);
    ESP_LOGI(TAG, "gzip inflated %d -> %lu bytes", total, dlen);
    return (int)dlen;
}

cJSON *http_get_json(const char *url, const char *bearer_token) {
    if (!ensure_buf()) return NULL;
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
        .buffer_size = HTTP_BUF_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "init fail: %s", url);
        return NULL;
    }
    if (bearer_token && bearer_token[0]) {
        char hdr[256];
        snprintf(hdr, sizeof(hdr), "Bearer %s", bearer_token);
        esp_http_client_set_header(client, "Authorization", hdr);
    }
    /* 小型 MCU 不解压 gzip/curl 默认行为，显式要求明文 */
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_http_client_set_header(client, "User-Agent", "deskwong/1.0");
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open fail: %s (err=0x%x %s)", url, err, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }
    /* 必须 fetch_headers 之后才能读到 body，否则直接 read 会拿到 0 字节 */
    int status = esp_http_client_fetch_headers(client);
    if (status < 0) {
        ESP_LOGE(TAG, "headers fail: %d (%s)", status, url);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }
    int http_status = esp_http_client_get_status_code(client);
    int total = 0;
    int len;
    while (total < HTTP_BUF_SIZE - 1) {
        len = esp_http_client_read(client, g_buf + total, HTTP_BUF_SIZE - total - 1);
        if (len <= 0) break;
        total += len;
    }
    if (total <= 0) {
        ESP_LOGW(TAG, "http %d empty body: %s", http_status, url);
        return NULL;
    }
    g_buf[total] = 0;
    if (g_buf[0] == 0x1f) total = inflate_gzip(g_buf, total);
    if (http_status < 200 || http_status >= 300) {
        ESP_LOGW(TAG, "http %d len=%d head=%.90s", http_status, total, g_buf);
        return NULL;
    }
    if (g_buf[0] != '{' && g_buf[0] != '[') {
        ESP_LOGW(TAG, "non-json body: %.60s", g_buf);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (total <= 0) {
        ESP_LOGW(TAG, "empty body: %s", url);
        return NULL;
    }
    return cJSON_Parse(g_buf);
}
