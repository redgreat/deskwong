#include <string.h>
#include "portal_rewrite.h"

/* 属性值里允许出现的 URL 形态：
 *   http://host/rest  https://host/rest  //host/rest  /root-relative  其他原样透传 */
typedef struct {
    const char *in;
    size_t len;
    size_t pos;
    char *out;
    size_t cap;
    size_t wpos;
} rw_t;

static int rw_put(rw_t *r, const char *s, size_t n) {
    /* 预留 1 字节便于调用方当 C 字符串使用 */
    if (r->wpos + n >= r->cap) return -2;
    memcpy(r->out + r->wpos, s, n);
    r->wpos += n;
    return 0;
}

static int ci_starts_with(const char *s, size_t slen, size_t pos, const char *prefix) {
    size_t plen = strlen(prefix);
    if (pos + plen > slen) return 0;
    for (size_t i = 0; i < plen; i++) {
        char a = s[pos + i], b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

/* host[:port] 合法字符集，防注入与路径穿越 */
static int host_char_ok(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':';
}

/* 把 authority 起点的 URL 改写为 /p/<host>/<rest>；返回 0 已改写，1 无法改写（透传），-2 缓冲不足 */
static int rewrite_authority_url(rw_t *r, size_t vpos, size_t vlen) {
    const char *v = r->in + vpos;
    size_t i = 0;
    while (i < vlen && host_char_ok(v[i])) i++;
    if (i == 0 || i > 64) return 1;                    /* 空 host 或超长：透传 */
    for (size_t k = 0; k < i; k++) {
        if (v[k] == '.' && k + 1 < i && v[k + 1] == '.') return 1;   /* 拒绝 ".." */
    }
    if (i < vlen && v[i] != '/' && v[i] != '?' && v[i] != '#') return 1;  /* host 后跟非法字符：透传 */
    if (rw_put(r, "/p/", 3) != 0) return -2;
    if (rw_put(r, v, i) != 0) return -2;
    if (i >= vlen || v[i] == '?' || v[i] == '#') {
        if (rw_put(r, "/", 1) != 0) return -2;         /* host-only → 补 "/" */
    }
    if (i < vlen && rw_put(r, v + i, vlen - i) != 0) return -2;
    return 0;
}

/* 改写单个属性值；返回 0 已写（可能原样），-2 缓冲不足 */
static int rewrite_url_value(rw_t *r, const char *base_host, size_t vpos, size_t vlen) {
    const char *v = r->in + vpos;
    size_t skip = 0;
    if (ci_starts_with(v, vlen, 0, "http://")) skip = 7;
    else if (ci_starts_with(v, vlen, 0, "https://")) skip = 8;
    else if (vlen >= 2 && v[0] == '/' && v[1] == '/') skip = 2;
    if (skip) {
        int rc = rewrite_authority_url(r, vpos + skip, vlen - skip);
        if (rc == -2) return -2;
        if (rc == 0) return 0;
        return rw_put(r, v, vlen);          /* host 不合法：原样透传 */
    }
    if (vlen >= 1 && v[0] == '/' && base_host[0]) {
        if (rw_put(r, "/p/", 3) != 0) return -2;
        if (rw_put(r, base_host, strlen(base_host)) != 0) return -2;
        return rw_put(r, v, vlen);
    }
    return rw_put(r, v, vlen);
}

static const char *const ATTRS[] = {"href", "src", "action"};

int portal_rewrite_html(const char *base_host, const char *in, size_t in_len,
                        char *out, size_t out_cap, size_t *out_len) {
    if (!base_host || !in || !out || !out_len || out_cap == 0) return -1;
    rw_t r = { .in = in, .len = in_len, .pos = 0, .out = out, .cap = out_cap, .wpos = 0 };

    while (r.pos < r.len) {
        int handled = 0;
        for (size_t a = 0; a < sizeof(ATTRS) / sizeof(ATTRS[0]) && !handled; a++) {
            const char *name = ATTRS[a];
            size_t nlen = strlen(name);
            if (!ci_starts_with(in, in_len, r.pos, name)) continue;
            size_t j = r.pos + nlen;
            while (j < r.len && (in[j] == ' ' || in[j] == '\t')) j++;
            if (j >= r.len || in[j] != '=') continue;
            j++;
            while (j < r.len && (in[j] == ' ' || in[j] == '\t')) j++;
            if (j >= r.len || (in[j] != '"' && in[j] != '\'')) continue;
            char quote = in[j];
            size_t vstart = j + 1, vend = vstart;
            while (vend < r.len && in[vend] != quote) vend++;

            if (rw_put(&r, in + r.pos, vstart - r.pos) != 0) return -2;
            int rc = rewrite_url_value(&r, base_host, vstart, vend - vstart);
            if (rc == -2) return -2;
            if (vend < r.len && rw_put(&r, &quote, 1) != 0) return -2;
            r.pos = (vend < r.len) ? vend + 1 : vend;
            handled = 1;
        }
        if (!handled) {
            if (rw_put(&r, in + r.pos, 1) != 0) return -2;
            r.pos++;
        }
    }
    if (r.wpos + 1 >= r.cap) return -2;
    out[r.wpos] = '\0';
    *out_len = r.wpos;
    return 0;
}

int portal_rewrite_location(const char *loc, char *out, size_t out_cap) {
    if (!loc || !out || out_cap == 0) return -1;
    size_t len = strlen(loc);
    size_t off = 0;
    if (ci_starts_with(loc, len, 0, "http://")) off = 7;
    else if (ci_starts_with(loc, len, 0, "https://")) off = 8;
    else if (loc[0] == '/' && loc[1] == '/') off = 2;
    else return -1;                                     /* 相对 Location：无从推断 host */
    size_t w = 0;
    if (w + 3 >= out_cap) return -2;
    memcpy(out, "/p/", 3); w += 3;
    for (size_t i = off; i < len; i++) {
        if (w + 1 >= out_cap) return -2;
        out[w++] = loc[i];
    }
    out[w] = '\0';
    return 0;
}
