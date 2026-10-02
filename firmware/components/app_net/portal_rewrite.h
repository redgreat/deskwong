#ifndef PORTAL_REWRITE_H
#define PORTAL_REWRITE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把 HTML 中 href/src/action 属性里的绝对 http(s) URL 与协议相对 //host/... 改写为
 * /p/<host>/<path>；根相对路径（/x）补 /p/<base_host> 前缀；其余内容逐字节透传。
 * 这样门户页内的相对/根相对引用会自然落到本代理路径下。
 *
 * base_host：门户主机（host[:port]，不带 scheme），用于根相对路径补前缀。
 * out 容量不足时返回 -2（此时 out 内容不完整，调用方应放弃改写并原样透传）。
 * 返回 0 成功并写 *out_len；-1 参数错误。 */
int portal_rewrite_html(const char *base_host, const char *in, size_t in_len,
                        char *out, size_t out_cap, size_t *out_len);

/* 把 Location 头（http(s)://host/rest）改写为本地 /p/host/rest；改写不了返回 -1 */
int portal_rewrite_location(const char *loc, char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
