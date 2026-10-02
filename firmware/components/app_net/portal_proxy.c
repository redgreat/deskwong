#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "portal_proxy.h"
#include "portal_rewrite.h"

static const char *TAG = "portal";

#define PROXY_MAX_BODY     (96 * 1024)
#define PROXY_MAX_POST     (8 * 1024)
#define PROXY_TIMEOUT_MS   10000
#define PROXY_PROBE_URL    "http://connect.rom.miui.com/generate_204"
#define PROXY_HOST_MAX     64
#define PROXY_PATH_MAX     256

/* ---------- 内部工具 ---------- */

static void *proxy_malloc(size_t n) {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n);
    return p;
}

/* req->uri 拆出 host、path（不含 query）；query 由调用方另行截取。
 * 返回 0 成功；-1 非法。 */
static int parse_proxy_uri(const char *uri, char *host, size_t host_cap,
                           char *path, size_t path_cap,
                           char *query, size_t query_cap) {
    if (strncmp(uri, "/p/", 3) != 0) return -1;
    const char *rest = uri + 3;
    const char *qmark = strchr(rest, '?');
    size_t plain_len = qmark ? (size_t)(qmark - rest) : strlen(rest);

    const char *slash = memchr(rest, '/', plain_len);
    size_t host_len = slash ? (size_t)(slash - rest) : plain_len;
    if (host_len == 0 || host_len >= host_cap) return -1;
    for (size_t i = 0; i < host_len; i++) {
        char c = rest[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':';
        if (!ok) return -1;
        if (c == '.' && i + 1 < host_len && rest[i + 1] == '.') return -1;
    }
    memcpy(host, rest, host_len);
    host[host_len] = '\0';

    size_t path_len = plain_len - host_len;             /* 0 或以 '/' 开头 */
    if (path_len == 0) {
        snprintf(path, path_cap, "/");
    } else {
        if (path_len >= path_cap) return -1;
        memcpy(path, rest + host_len, path_len);
        path[path_len] = '\0';
    }
    if (qmark && qmark[1]) {
        size_t qlen = strlen(qmark + 1);
        if (qlen >= query_cap) return -1;
        strcpy(query, qmark + 1);
    } else {
        query[0] = '\0';
    }
    return 0;
}

typedef struct {
    char *buf;              /* 响应体（调用方释放） */
    size_t len;
    int status;
    char ct[96];            /* Content-Type（可能为空） */
    char location[256];     /* Location 头（可能为空） */
    esp_err_t open_err;     /* open/连接阶段错误 */
} fetch_result_t;

/* 抓取 URL；allow_redirect=false 时不跟随 3xx，便于探测门户 */
static esp_err_t fetch_url(const char *url, bool is_post,
                           const char *post_ct, const char *post_body, size_t post_len,
                           bool allow_redirect, fetch_result_t *out) {
    memset(out, 0, sizeof(*out));
    esp_http_client_config_t conf = {
        .url = url,
        .timeout_ms = PROXY_TIMEOUT_MS,
        .disable_auto_redirect = !allow_redirect,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&conf);
    if (!client) return ESP_FAIL;
    esp_http_client_set_method(client, is_post ? HTTP_METHOD_POST : HTTP_METHOD_GET);
    esp_http_client_set_header(client, "User-Agent",
                               "Mozilla/5.0 (Linux; deskwong) AppleWebKit/537.36");
    if (is_post && post_body) {
        esp_http_client_set_header(client, "Content-Type",
                                   post_ct[0] ? post_ct : "application/x-www-form-urlencoded");
    }

    esp_err_t err = esp_http_client_open(client, (is_post && post_body) ? (int)post_len : 0);
    if (err != ESP_OK) {
        out->open_err = err;
        esp_http_client_cleanup(client);
        return err;
    }
    if (is_post && post_body && post_len > 0) {
        int w = esp_http_client_write(client, post_body, (int)post_len);
        if (w < 0) ESP_LOGW(TAG, "post body write failed");
    }
    esp_http_client_fetch_headers(client);
    out->status = esp_http_client_get_status_code(client);

    char *hdr = NULL;
    if (esp_http_client_get_header(client, "Content-Type", &hdr) == ESP_OK && hdr) {
        strncpy(out->ct, hdr, sizeof(out->ct) - 1);
    }
    hdr = NULL;
    if (esp_http_client_get_header(client, "Location", &hdr) == ESP_OK && hdr) {
        strncpy(out->location, hdr, sizeof(out->location) - 1);
    }

    out->buf = (char *)proxy_malloc(PROXY_MAX_BODY);
    if (out->buf) {
        size_t total = 0;
        while (total < PROXY_MAX_BODY) {
            int n = esp_http_client_read(client, out->buf + total, PROXY_MAX_BODY - total);
            if (n <= 0) break;
            total += (size_t)n;
        }
        out->len = total;
    } else {
        ESP_LOGE(TAG, "response buffer alloc failed");
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}

/* 大小写无关前缀匹配（host gcc 与 IDF newlib 通用，不依赖 strncasecmp） */
static int ci_prefix(const char *s, const char *prefix) {
    while (*prefix) {
        char a = *s, b = *prefix;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
        s++; prefix++;
    }
    return 1;
}

static bool ct_is_html(const char *ct) {
    if (ct[0] == '\0') return true;                     /* 无 Content-Type：按 HTML 处理 */
    while (*ct == ' ' || *ct == '\t') ct++;
    return ci_prefix(ct, "text/html");
}

static esp_err_t send_bytes(httpd_req_t *req, const char *ct, const char *buf, size_t len) {
    httpd_resp_set_type(req, ct[0] ? ct : "application/octet-stream");
    /* 门户页不缓存，避免放行状态变化后拿到旧页 */
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, (ssize_t)len);
}

static esp_err_t send_text(httpd_req_t *req, int code, const char *msg) {
    httpd_resp_set_status(req, code == 200 ? "200 OK" :
                              code == 302 ? "302 Found" : "502 Bad Gateway");
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, msg, (ssize_t)strlen(msg));
}

/* ---------- 处理函数 ---------- */

esp_err_t portal_proxy_handler(httpd_req_t *req) {
    char host[PROXY_HOST_MAX] = {0};
    char path[PROXY_PATH_MAX] = {0};
    char query[PROXY_PATH_MAX] = {0};
    if (parse_proxy_uri(req->uri, host, sizeof(host), path, sizeof(path), query, sizeof(query)) != 0) {
        return send_text(req, 502, "bad proxy path, use /p/<host>/<path>");
    }

    const size_t target_cap = PROXY_HOST_MAX + PROXY_PATH_MAX * 2 + 16;
    char *target = (char *)proxy_malloc(target_cap);
    if (!target) return send_text(req, 502, "out of memory");
    snprintf(target, target_cap, "http://%s%s%s%s",
             host, path, query[0] ? "?" : "", query);
    ESP_LOGI(TAG, "proxy %s %s", req->method == HTTP_POST ? "POST" : "GET", target);

    char post_ct[96] = {0};
    char *post_body = NULL;
    size_t post_len = 0;
    if (req->method == HTTP_POST && req->content_len > 0) {
        size_t n = (size_t)req->content_len < PROXY_MAX_POST ? (size_t)req->content_len : PROXY_MAX_POST;
        post_body = (char *)proxy_malloc(n + 1);
        if (post_body) {
            int got = httpd_req_recv(req, post_body, n);
            if (got > 0) post_body[got] = '\0';
            post_len = got > 0 ? (size_t)got : 0;
            httpd_req_get_hdr_value_str(req, "Content-Type", post_ct, sizeof(post_ct));
        }
    }

    fetch_result_t fr;
    esp_err_t err = fetch_url(target, req->method == HTTP_POST, post_ct, post_body, post_len,
                              true, &fr);
    free(post_body);
    free(target);
    if (err != ESP_OK || !fr.buf) {
        free(fr.buf);
        return send_text(req, 502, "portal fetch failed (portal only reachable via device STA)");
    }
    ESP_LOGI(TAG, "proxy result status=%d len=%u ct=%s", fr.status, (unsigned)fr.len, fr.ct);

    esp_err_t ret;
    if (ct_is_html(fr.ct)) {
        size_t out_cap = fr.len * 2 + 64;
        char *out = (char *)proxy_malloc(out_cap);
        size_t out_len = 0;
        if (out && portal_rewrite_html(host, fr.buf, fr.len, out, out_cap, &out_len) == 0) {
            ret = send_bytes(req, "text/html", out, out_len);
        } else {
            /* 缓冲不足或分配失败：原样透传，链接可能断但页面仍可见 */
            ESP_LOGW(TAG, "html rewrite skipped (alloc/size)");
            ret = send_bytes(req, "text/html", fr.buf, fr.len);
        }
        free(out);
    } else {
        ret = send_bytes(req, fr.ct, fr.buf, fr.len);
    }
    free(fr.buf);
    return ret;
}

esp_err_t portal_check_handler(httpd_req_t *req) {
    fetch_result_t fr;
    esp_err_t err = fetch_url(PROXY_PROBE_URL, false, NULL, NULL, 0, false, &fr);
    if (err != ESP_OK || !fr.buf) {
        free(fr.buf);
        return send_text(req, 502, "probe failed (STA must be associated to the portal network)");
    }
    bool online = fr.status == 204;
    char loc[PROXY_PATH_MAX] = {0};
    if (fr.location[0]) portal_rewrite_location(fr.location, loc, sizeof(loc));
    ESP_LOGI(TAG, "check status=%d online=%d location=%s", fr.status, online, fr.location);

    char body[384];
    snprintf(body, sizeof(body),
             "{\"online\":%s,\"status\":%d,\"location\":\"%s\",\"probe\":\"%s\"}",
             online ? "true" : "false", fr.status, loc[0] ? loc : fr.location, PROXY_PROBE_URL);
    free(fr.buf);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, (ssize_t)strlen(body));
}

esp_err_t portal_start_handler(httpd_req_t *req) {
    fetch_result_t fr;
    esp_err_t err = fetch_url(PROXY_PROBE_URL, false, NULL, NULL, 0, false, &fr);
    if (err != ESP_OK || !fr.buf) {
        free(fr.buf);
        return send_text(req, 502,
            "probe failed: device STA 未关联到目标网络，或门户不可达\n"
            "请确认设备已连接需要认证的 WiFi 后重试");
    }
    if (fr.status == 204) {
        free(fr.buf);
        return send_text(req, 200, "already online (no captive portal detected)");
    }
    /* 302 → 浏览器跟随到本地代理路径；200 门户页 → 直接代理渲染 */
    char loc[PROXY_PATH_MAX] = {0};
    int rewritable = fr.location[0] ? portal_rewrite_location(fr.location, loc, sizeof(loc)) : -1;
    free(fr.buf);

    if (rewritable == 0) {
        ESP_LOGI(TAG, "start -> redirect %s", loc);
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", loc);
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "redirecting", 11);
    }
    return send_text(req, 200,
        "portal detected but redirect URL not rewritable; open the portal host manually via /p/<host>/");
}
