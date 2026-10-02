/* portal_rewrite 主机测试：正常改写、透传、边界与错误路径 */
#include <stdio.h>
#include <string.h>
#include "portal_rewrite.h"

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

#define RW(input, expect) do {                                              \
    char out[1024]; size_t olen = 0;                                        \
    int rc = portal_rewrite_html("1.1.1.1", input, strlen(input),           \
                                 out, sizeof(out), &olen);                  \
    CHECK(rc == 0, #input ": rewrite rc==0");                               \
    CHECK(olen == strlen(expect) && memcmp(out, expect, olen) == 0,         \
          #input ": content mismatch");                                     \
} while (0)

int main(void) {
    /* 正常路径：绝对 URL / 协议相对 / 根相对 / 相对路径 */
    RW("<a href=\"http://1.1.1.1/portal/login?x=1\">go</a>",
       "<a href=\"/p/1.1.1.1/portal/login?x=1\">go</a>");
    RW("<img src=\"https://p.example.com:8443/q.png\">",
       "<img src=\"/p/p.example.com:8443/q.png\">");
    RW("<a href=\"http://1.1.1.1\">home</a>",
       "<a href=\"/p/1.1.1.1/\">home</a>");
    RW("<script src=\"/static/q.js\"></script>",
       "<script src=\"/p/1.1.1.1/static/q.js\"></script>");
    RW("<img src=\"img/q.png\">", "<img src=\"img/q.png\">");
    RW("<script src=\"//cdn.example.com/a.js\"></script>",
       "<script src=\"/p/cdn.example.com/a.js\"></script>");

    /* 表单 action 与大小写、单引号 */
    RW("<FORM ACTION=\"http://1.1.1.1/login\" METHOD=\"post\"></FORM>",
       "<FORM ACTION=\"/p/1.1.1.1/login\" METHOD=\"post\"></FORM>");
    RW("<a href='http://1.1.1.1/x'>q</a>", "<a href='/p/1.1.1.1/x'>q</a>");

    /* 非 URL 值原样透传 */
    RW("<a href=\"javascript:void(0)\">j</a>", "<a href=\"javascript:void(0)\">j</a>");
    RW("<a href=\"#top\">t</a>", "<a href=\"#top\">t</a>");
    RW("<a href=\"mailto:a@b.c\">m</a>", "<a href=\"mailto:a@b.c\">m</a>");

    /* 属性之外正文里的 http:// 不得被改写 */
    RW("<p>visit http://1.1.1.1/x now</p>", "<p>visit http://1.1.1.1/x now</p>");

    /* 边界：空输入 */
    {
        char out[16]; size_t olen = 99;
        CHECK(portal_rewrite_html("1.1.1.1", "", 0, out, sizeof(out), &olen) == 0,
              "empty input rc");
        CHECK(olen == 0, "empty input len");
    }

    /* 错误路径：缓冲不足返回 -2；非法参数返回 -1 */
    {
        char out[8]; size_t olen = 0;
        const char *big = "<a href=\"http://1.1.1.1/very/long/path/that/never/fits\">x</a>";
        CHECK(portal_rewrite_html("1.1.1.1", big, strlen(big), out, sizeof(out), &olen) == -2,
              "small buffer -> -2");
        CHECK(portal_rewrite_html(NULL, big, 10, out, sizeof(out), &olen) == -1,
              "null base host -> -1");
        CHECK(portal_rewrite_html("1.1.1.1", big, 10, NULL, sizeof(out), &olen) == -1,
              "null out -> -1");
    }

    /* host 不合法（含非主机字符）时原样透传不崩溃 */
    RW("<a href=\"http://bad host/x\">b</a>", "<a href=\"http://bad host/x\">b</a>");

    /* location 改写 */
    {
        char loc[128];
        CHECK(portal_rewrite_location("http://1.1.1.1/portal?userip=1&usermac=2",
                                      loc, sizeof(loc)) == 0, "location rewrite rc");
        CHECK(strcmp(loc, "/p/1.1.1.1/portal?userip=1&usermac=2") == 0, "location content");
        CHECK(portal_rewrite_location("/relative/path", loc, sizeof(loc)) == -1,
              "relative location -> -1");
    }

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("portal_rewrite host test passed\n");
    return 0;
}
