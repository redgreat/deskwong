/* wifi_reason 主机测试：正常映射、边界与未知码 */
#include <stdio.h>
#include <string.h>
#include "wifi_reason.h"

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

int main(void) {
    /* 正常路径：排障常见原因码 */
    CHECK(strcmp(wifi_reason_str(201), "NO_AP_FOUND") == 0, "201 -> NO_AP_FOUND");
    CHECK(strcmp(wifi_reason_str(15), "4WAY_HANDSHAKE_TIMEOUT") == 0, "15 -> 4WAY_HANDSHAKE_TIMEOUT");
    CHECK(strcmp(wifi_reason_str(204), "HANDSHAKE_TIMEOUT") == 0, "204 -> HANDSHAKE_TIMEOUT");
    CHECK(strcmp(wifi_reason_str(2), "AUTH_EXPIRE") == 0, "2 -> AUTH_EXPIRE");
    CHECK(strcmp(wifi_reason_str(202), "AUTH_FAIL") == 0, "202 -> AUTH_FAIL");
    CHECK(strcmp(wifi_reason_str(203), "ASSOC_FAIL") == 0, "203 -> ASSOC_FAIL");

    /* 边界/错误路径：未知与异常码一律 UNKNOWN */
    CHECK(strcmp(wifi_reason_str(0), "UNKNOWN") == 0, "0 -> UNKNOWN");
    CHECK(strcmp(wifi_reason_str(-1), "UNKNOWN") == 0, "-1 -> UNKNOWN");
    CHECK(strcmp(wifi_reason_str(999), "UNKNOWN") == 0, "999 -> UNKNOWN");
    CHECK(strcmp(wifi_reason_str(2147483647), "UNKNOWN") == 0, "INT_MAX -> UNKNOWN");

    /* 表内全部码均返回非空字符串 */
    static const int codes[] = {1, 2, 4, 8, 15, 200, 201, 202, 203, 204, 205, 206, 207};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        const char *s = wifi_reason_str(codes[i]);
        CHECK(s != NULL && s[0] != '\0', "mapped reason returns non-empty string");
    }

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("wifi_reason host test passed\n");
    return 0;
}
