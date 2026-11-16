/* download_tool: the URLs, the zip unpacking and the HTTP fetch against values tools/download_tool.py gives. Nothing here
 * touches the network: the fetch talks to a listener on 127.0.0.1 started by the test. The tool as a whole is checked
 * by the differential harness (diff/cases.txt). */
#include <string.h>
#include <windows.h>

#include "avp/test.h"
#include "download_tool.h"
#include "dt_http.h"
#include "dt_zip_fixture.h"
#include "shsm_net.h"

#define TMP "build/tmp/download_tool_test"

AVP_TEST(urls) {
    AvpArena *a = avp_arena();
    CHECK_CZ(dt_tool_url(a, "dtk", "v1.8.4", "windows", "amd64"),
             "https://github.com/encounter/decomp-toolkit/releases/download/v1.8.4/dtk-windows-x86_64.exe");
    CHECK_CZ(dt_tool_url(a, "dtk", "v1.8.4", "linux", "aarch64"),
             "https://github.com/encounter/decomp-toolkit/releases/download/v1.8.4/dtk-linux-aarch64");
    CHECK_CZ(dt_tool_url(a, "dtk", "t", "darwin", "arm64"),
             "https://github.com/encounter/decomp-toolkit/releases/download/t/dtk-macos-arm64");
    CHECK_CZ(dt_tool_url(a, "objdiff-cli", "v3.8.2", "windows", "amd64"),
             "https://github.com/encounter/objdiff/releases/download/v3.8.2/objdiff-cli-windows-x86_64.exe");
    CHECK_CZ(dt_tool_url(a, "binutils", "2.42-2", "windows", "amd64"),
             "https://github.com/encounter/gc-wii-binutils/releases/download/2.42-2/windows-x86_64.zip");
    CHECK_CZ(dt_tool_url(a, "binutils", "2.42-2", "darwin", "arm64"),
             "https://github.com/encounter/gc-wii-binutils/releases/download/2.42-2/macos-universal.zip");
    CHECK_CZ(dt_tool_url(a, "compilers", "20240706", "windows", "amd64"),
             "https://files.decomp.dev/compilers_20240706.zip");
    CHECK_CZ(dt_tool_url(a, "sjiswrap", "v1.2.2", "windows", "amd64"),
             "https://github.com/encounter/sjiswrap/releases/download/v1.2.2/sjiswrap-windows-x86.exe");
    CHECK_CZ(dt_tool_url(a, "wibo", "1.0.0", "windows", "amd64"),
             "https://github.com/decompals/wibo/releases/download/1.0.0/wibo-amd64");
    CHECK_CZ(dt_tool_url(a, "wibo", "1.0.0", "darwin", "arm64"),
             "https://github.com/decompals/wibo/releases/download/1.0.0/wibo-macos");
    CHECK(dt_tool_url(a, "nosuch", "t", "windows", "amd64") == NULL);
}

AVP_TEST(zip_member_names) {
    AvpArena *a = avp_arena();
    CHECK_CZ(dt_zip_arcname(a, avp_str("top.txt")), "top.txt");
    CHECK_CZ(dt_zip_arcname(a, avp_str("sub/dir/f.bin")), "sub\\dir\\f.bin");
    CHECK_CZ(dt_zip_arcname(a, avp_str("../evil.txt")), "evil.txt");
    CHECK_CZ(dt_zip_arcname(a, avp_str("/abs/./x/../y")), "abs\\x\\y");
    CHECK_CZ(dt_zip_arcname(a, avp_str("C:\\drive.txt")), "drive.txt");
    CHECK_CZ(dt_zip_arcname(a, avp_str("a:b/c*.txt")), "b\\c_.txt");
    CHECK_CZ(dt_zip_arcname(a, avp_str("x:y?z|w\"<>.o")), "y_z_w___.o");
    CHECK_CZ(dt_zip_arcname(a, avp_str("trail. /x. ")), "trail\\x");
    CHECK_CZ(dt_zip_arcname(a, avp_str("dir/")), "dir");
    CHECK_CZ(dt_zip_arcname(a, avp_str("//")), "");
    CHECK_CZ(dt_zip_arcname(a, avp_str("...")), "");
}

static bool read_file(const char *path, AvpBytes *b) {
    memset(b, 0, sizeof *b);
    return avp_read_all(avp_arena(), path, b);
}

AVP_TEST(zip_extract_all) {
    AvpArena *a = avp_arena();
    const char *dest = TMP "/zip";
    avp_rmtree(dest, 0);
    DtErr err;
    CHECK(dt_zip_extract_all(a, dt_zip_fixture, sizeof dt_zip_fixture, dest, &err));
    AvpBytes b;
    CHECK(read_file(TMP "/zip/top.txt", &b));
    CHECK(b.n == 6 && memcmp(b.v, "hello\n", 6) == 0);
    CHECK(read_file(TMP "/zip/sub/dir/data.bin", &b));
    CHECK(b.n == 256 * 40);
    for (size_t i = 0; i < b.n; i++)
        if (b.v[i] != (u8)(i & 255)) {
            CHECK_MSG(false, "data.bin byte %zu", i);
            break;
        }
    CHECK(avp_isdir(TMP "/zip/emptydir"));
    CHECK(avp_isfile(TMP "/zip/evil.txt"));
    CHECK(avp_isfile(TMP "/zip/drive.txt"));
    CHECK(avp_isfile(TMP "/zip/b/c_.txt"));
    CHECK(avp_isfile(TMP "/zip/trail/x"));
    CHECK(avp_isfile(TMP "/zip/abs/rooted.txt"));
    /* extracting again overwrites */
    CHECK(dt_zip_extract_all(a, dt_zip_fixture, sizeof dt_zip_fixture, dest, &err));
}

AVP_TEST(zip_errors) {
    AvpArena *a = avp_arena();
    DtErr err;
    CHECK(!dt_zip_extract_all(a, (const u8 *)"not a zip file at all", 21, TMP "/bad", &err));
    CHECK_CZ(err.cls, "zipfile.BadZipFile");
    CHECK_CZ(err.text, "File is not a zip file");
    CHECK(!dt_zip_extract_all(a, dt_zip_fixture, 10, TMP "/bad", &err));
    CHECK_CZ(err.cls, "zipfile.BadZipFile");
    /* a flipped byte in the stored member "hello" */
    u8 copy[sizeof dt_zip_fixture];
    memcpy(copy, dt_zip_fixture, sizeof copy);
    for (size_t i = 0; i + 5 < sizeof copy; i++)
        if (memcmp(copy + i, "hello", 5) == 0) {
            copy[i] ^= 1;
            break;
        }
    avp_rmtree(TMP "/crc", 0);
    CHECK(!dt_zip_extract_all(a, copy, sizeof copy, TMP "/crc", &err));
    CHECK_CZ(err.cls, "zipfile.BadZipFile");
    CHECK_CZ(err.text, "Bad CRC-32 for file 'top.txt'");
}

/* ---- the HTTP fetch against a local listener ---------------------------------------------------------------------- */

static ShsmSocket g_listener;

static void serve_one(ShsmSocket c) {
    char req[2048];
    size_t used = 0;
    ShsmNetError ne;
    while (used < sizeof req - 1) {
        size_t got = 0;
        if (shsm_net_recv(c, req + used, sizeof req - 1 - used, 5.0, &got, &ne) != SHSM_NET_OK) break;
        used += got;
        req[used] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    req[used] = 0;
    const char *resp;
    if (strstr(req, "GET /x/redirect "))
        resp = "HTTP/1.1 302 Found\r\nLocation: /x/file\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    else if (strstr(req, "GET /x/file ") && strstr(req, "User-Agent: Mozilla/5.0"))
        resp = "HTTP/1.1 200 OK\r\nContent-Length: 7\r\nConnection: close\r\n\r\npayload";
    else if (strstr(req, "GET /x/empty "))
        resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    else
        resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    shsm_net_send_all(c, resp, strlen(resp), 5.0, &ne);
    shsm_net_close(c);
}

static DWORD WINAPI server_thread(LPVOID arg) {
    int connections = (int)(INT_PTR)arg;
    for (int i = 0; i < connections; i++) {
        ShsmSocket c;
        ShsmNetError ne;
        if (shsm_net_accept(g_listener, 20.0, &c, &ne) != SHSM_NET_OK) break;
        serve_one(c);
    }
    return 0;
}

typedef struct {
    char body[64];
    size_t n;
    int begins;
} Collect;

static bool c_begin(void *ctx) {
    ((Collect *)ctx)->begins++;
    return true;
}
static bool c_data(void *ctx, const void *p, size_t n) {
    Collect *c = ctx;
    memcpy(c->body + c->n, p, n);
    c->n += n;
    return true;
}

AVP_TEST(http_get_local) {
    ShsmNetError ne;
    int port = 0;
    CHECK(shsm_net_listen("127.0.0.1", 0, 8, &g_listener, &port, &ne) == SHSM_NET_OK);
    HANDLE th = CreateThread(NULL, 0, server_thread, (LPVOID)(INT_PTR)4, 0, NULL);
    char url[128];
    DtSink sink;
    DtHttpResult r;
    Collect c;

    /* a redirect, then the file; the request carries the Mozilla/5.0 user agent */
    memset(&c, 0, sizeof c);
    sink = (DtSink){c_begin, c_data, &c};
    snprintf(url, sizeof url, "http://127.0.0.1:%d/x/redirect", port);
    CHECK(dt_http_get(url, true, &sink, &r));
    CHECK(c.begins == 1 && c.n == 7 && memcmp(c.body, "payload", 7) == 0);

    /* an error status: nothing reaches the sink */
    memset(&c, 0, sizeof c);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/x/missing", port);
    CHECK(!dt_http_get(url, true, &sink, &r));
    CHECK(r.status == 404 && strcmp(r.reason, "Not Found") == 0 && c.begins == 0);

    /* an empty body still begins */
    memset(&c, 0, sizeof c);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/x/empty", port);
    CHECK(dt_http_get(url, true, &sink, &r));
    CHECK(c.begins == 1 && c.n == 0);

    WaitForSingleObject(th, 30000);
    CloseHandle(th);
    shsm_net_close(g_listener);

    /* nothing listens any more: a transport error */
    memset(&c, 0, sizeof c);
    CHECK(!dt_http_get(url, true, &sink, &r));
    CHECK(r.status == 0 && r.error[0] != 0);
}

AVP_TEST_MAIN("download_tool")
