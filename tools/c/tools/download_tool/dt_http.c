#include "dt_http.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

static void set_error(DtHttpResult *r, DWORD code) {
    wchar_t msg[512] = L"";
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             GetModuleHandleW(L"winhttp.dll"), code, 0, msg, 512, NULL);
    while (n > 0 && (msg[n - 1] == L'\r' || msg[n - 1] == L'\n' || msg[n - 1] == L' ' || msg[n - 1] == L'.')) n--;
    msg[n] = 0;
    char text[512] = "";
    WideCharToMultiByte(CP_UTF8, 0, msg, -1, text, (int)sizeof text, NULL, NULL);
    snprintf(r->error, sizeof r->error, "[WinError %lu] %s", (unsigned long)code, text);
}

bool dt_http_get(const char *url, bool no_proxy, const DtSink *sink, DtHttpResult *r) {
    memset(r, 0, sizeof *r);
    bool ok = false;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    wchar_t *wurl = avp_to_wz(avp_scratch(), url);

    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    uc.dwSchemeLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc) || !uc.lpszHostName) {
        snprintf(r->error, sizeof r->error, "unknown url type: %s", url);
        return false;
    }
    wchar_t host[256];
    if (uc.dwHostNameLength >= 256) {
        snprintf(r->error, sizeof r->error, "host name too long");
        return false;
    }
    memcpy(host, uc.lpszHostName, uc.dwHostNameLength * sizeof(wchar_t));
    host[uc.dwHostNameLength] = 0;
    size_t plen = (uc.lpszUrlPath ? uc.dwUrlPathLength : 0) + (uc.lpszExtraInfo ? uc.dwExtraInfoLength : 0);
    wchar_t *path = (wchar_t *)avp_alloc(avp_scratch(), (plen + 2) * sizeof(wchar_t));
    size_t o = 0;
    if (uc.lpszUrlPath) {
        memcpy(path, uc.lpszUrlPath, uc.dwUrlPathLength * sizeof(wchar_t));
        o = uc.dwUrlPathLength;
    }
    if (uc.lpszExtraInfo) {
        memcpy(path + o, uc.lpszExtraInfo, uc.dwExtraInfoLength * sizeof(wchar_t));
        o += uc.dwExtraInfoLength;
    }
    if (o == 0) path[o++] = L'/';
    path[o] = 0;

    ses = WinHttpOpen(L"Mozilla/5.0",
                      no_proxy ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) {
        set_error(r, GetLastError());
        goto done;
    }
    WinHttpSetTimeouts(ses, 0, 60000, 60000, 300000);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) {
        set_error(r, GetLastError());
        goto done;
    }
    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        set_error(r, GetLastError());
        goto done;
    }
    DWORD redirects = 10;
    WinHttpSetOption(req, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &redirects, sizeof redirects);
    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req, NULL)) {
        set_error(r, GetLastError());
        goto done;
    }
    DWORD status = 0, sz = sizeof status;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &sz, WINHTTP_NO_HEADER_INDEX);
    r->status = (int)status;
    wchar_t reason[128] = L"";
    sz = sizeof reason;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, reason, &sz,
                        WINHTTP_NO_HEADER_INDEX);
    WideCharToMultiByte(CP_UTF8, 0, reason, -1, r->reason, (int)sizeof r->reason, NULL, NULL);
    if (status < 200 || status > 299) goto done;
    if (sink->begin && !sink->begin(sink->ctx)) {
        r->status = 0;
        goto done;
    }
    static char buf[1 << 16];
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(req, buf, sizeof buf, &got)) {
            set_error(r, GetLastError());
            r->status = 0;
            goto done;
        }
        if (got == 0) break;
        if (!sink->data(sink->ctx, buf, got)) {
            r->status = 0;
            goto done;
        }
    }
    ok = true;
done:
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    return ok;
}
