/* TCP sockets over Winsock (shsm_net.md). */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "shsm_net.h"

#include "avp/arena.h"
#include "avp/clock.h"
#include "avp/pool.h"
#include "avp/win.h"
#include "shsm_cli.h"

#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

static AvpOnce g_wsa_once;

static void wsa_start(void *ctx) {
    AVP_UNUSED(ctx);
    WSADATA wd;
    int rc = WSAStartup(MAKEWORD(2, 2), &wd);
    if (rc != 0) avp_fatal("%s: WSAStartup failed (%d)", avp_tool_name(), rc);
}

void shsm_net_init(void) { avp_once(&g_wsa_once, wsa_start, NULL); }

void shsm_net_set_error(ShsmNetError *err, int winerror) {
    if (!err) return;
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    AvpStr s = avp_win_error_str(sc, (unsigned)winerror);
    err->winerror = winerror;
    snprintf(err->text, sizeof err->text, "%.*s", AVP_SF(s));
    avp_arena_reset(sc, m);
}

static void set_timeout_error(ShsmNetError *err) {
    if (!err) return;
    err->winerror = 0;
    snprintf(err->text, sizeof err->text, "timed out");
}

static void set_closed(ShsmNetError *err) {
    if (!err) return;
    err->winerror = 0;
    err->text[0] = 0;
}

static void set_nonblocking(SOCKET s, bool on) {
    u_long v = on ? 1 : 0;
    ioctlsocket(s, FIONBIO, &v);
}

/* select() for one socket: 1 ready, 0 timeout, -1 error (WSAGetLastError). timeout < 0 waits without limit. For
 * write, a socket in the except set (a failed connect) counts as ready. */
static int wait_socket(SOCKET s, bool write, double timeout) {
    fd_set rs, ws, es;
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);
    if (write) {
        FD_SET(s, &ws);
        FD_SET(s, &es);
    } else {
        FD_SET(s, &rs);
    }
    struct timeval tv, *ptv = NULL;
    if (timeout >= 0) {
        double us = timeout * 1e6;
        if (us > 2e15) us = 2e15;
        i64 t = (i64)us;
        tv.tv_sec = (long)(t / 1000000);
        tv.tv_usec = (long)(t % 1000000);
        ptv = &tv;
    }
    int n = select(0, write ? NULL : &rs, write ? &ws : NULL, write ? &es : NULL, ptv);
    if (n == SOCKET_ERROR) return -1;
    return n > 0 ? 1 : 0;
}

static ShsmNetStatus connect_one(const struct addrinfo *ai, double timeout, SOCKET *out, ShsmNetError *err) {
    SOCKET s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (s == INVALID_SOCKET) {
        shsm_net_set_error(err, WSAGetLastError());
        return SHSM_NET_ERROR;
    }
    set_nonblocking(s, true);
    if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0) {
        *out = s;
        return SHSM_NET_OK;
    }
    int e = WSAGetLastError();
    if (e != WSAEWOULDBLOCK) {
        shsm_net_set_error(err, e);
        closesocket(s);
        return SHSM_NET_ERROR;
    }
    int r = wait_socket(s, true, timeout > 0 ? timeout : -1);
    if (r < 0) {
        shsm_net_set_error(err, WSAGetLastError());
        closesocket(s);
        return SHSM_NET_ERROR;
    }
    if (r == 0) {
        set_timeout_error(err);
        closesocket(s);
        return SHSM_NET_TIMEOUT;
    }
    int soerr = 0, len = (int)sizeof soerr;
    if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &len) != 0) soerr = WSAGetLastError();
    if (soerr != 0) {
        shsm_net_set_error(err, soerr);
        closesocket(s);
        return SHSM_NET_ERROR;
    }
    *out = s;
    return SHSM_NET_OK;
}

ShsmNetStatus shsm_net_connect(const char *host, int port, double timeout, ShsmSocket *out, ShsmNetError *err) {
    shsm_net_init();
    *out = SHSM_SOCKET_INVALID;
    char service[16];
    snprintf(service, sizeof service, "%d", port);
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, service, &hints, &res);
    if (rc != 0) {
        if (err) {
            err->winerror = rc;
            snprintf(err->text, sizeof err->text, "[Errno %d] getaddrinfo failed", rc);
        }
        return SHSM_NET_ERROR;
    }
    ShsmNetStatus st = SHSM_NET_ERROR;
    if (!res && err) shsm_net_set_error(err, WSAHOST_NOT_FOUND);
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        SOCKET s;
        st = connect_one(ai, timeout, &s, err);
        if (st == SHSM_NET_OK) {
            *out = (ShsmSocket)s;
            break;
        }
    }
    freeaddrinfo(res);
    return st;
}

void shsm_net_set_nodelay(ShsmSocket s) {
    int one = 1;
    setsockopt((SOCKET)s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, (int)sizeof one);
}

ShsmNetStatus shsm_net_send_all(ShsmSocket s, const void *p, size_t n, double timeout, ShsmNetError *err) {
    shsm_net_init();
    const char *q = (const char *)p;
    double deadline = timeout > 0 ? avp_monotonic() + timeout : 0;
    if (s == SHSM_SOCKET_INVALID) {
        shsm_net_set_error(err, WSAENOTSOCK);
        return SHSM_NET_ERROR;
    }
    while (n > 0) {
        int chunk = n > 0x40000000 ? 0x40000000 : (int)n;
        int k = send((SOCKET)s, q, chunk, 0);
        if (k > 0) {
            q += k;
            n -= (size_t)k;
            continue;
        }
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK) {
            shsm_net_set_error(err, e);
            return SHSM_NET_ERROR;
        }
        double left = -1;
        if (timeout > 0) {
            left = deadline - avp_monotonic();
            if (left <= 0) {
                set_timeout_error(err);
                return SHSM_NET_TIMEOUT;
            }
        }
        int r = wait_socket((SOCKET)s, true, left);
        if (r < 0) {
            shsm_net_set_error(err, WSAGetLastError());
            return SHSM_NET_ERROR;
        }
    }
    return SHSM_NET_OK;
}

ShsmNetStatus shsm_net_recv(ShsmSocket s, void *buf, size_t cap, double timeout, size_t *got, ShsmNetError *err) {
    shsm_net_init();
    *got = 0;
    if (s == SHSM_SOCKET_INVALID) {
        shsm_net_set_error(err, WSAENOTSOCK);
        return SHSM_NET_ERROR;
    }
    if (timeout != 0) {
        int r = wait_socket((SOCKET)s, false, timeout);
        if (r < 0) {
            shsm_net_set_error(err, WSAGetLastError());
            return SHSM_NET_ERROR;
        }
        if (r == 0) {
            set_timeout_error(err);
            return SHSM_NET_TIMEOUT;
        }
    }
    int want = cap > 0x40000000 ? 0x40000000 : (int)cap;
    int k = recv((SOCKET)s, (char *)buf, want, 0);
    if (k > 0) {
        *got = (size_t)k;
        return SHSM_NET_OK;
    }
    if (k == 0) {
        set_closed(err);
        return SHSM_NET_CLOSED;
    }
    int e = WSAGetLastError();
    if (e == WSAEWOULDBLOCK) {
        set_timeout_error(err);
        return SHSM_NET_TIMEOUT;
    }
    shsm_net_set_error(err, e);
    return SHSM_NET_ERROR;
}

ShsmNetStatus shsm_net_listen(const char *host, int port, int backlog, ShsmSocket *out, int *bound_port,
                              ShsmNetError *err) {
    shsm_net_init();
    *out = SHSM_SOCKET_INVALID;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        shsm_net_set_error(err, WSAGetLastError());
        return SHSM_NET_ERROR;
    }
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u_short)port);
    if (inet_pton(AF_INET, host ? host : "127.0.0.1", &sa.sin_addr) != 1) {
        shsm_net_set_error(err, WSAEINVAL);
        closesocket(s);
        return SHSM_NET_ERROR;
    }
    if (bind(s, (struct sockaddr *)&sa, (int)sizeof sa) != 0 || listen(s, backlog) != 0) {
        shsm_net_set_error(err, WSAGetLastError());
        closesocket(s);
        return SHSM_NET_ERROR;
    }
    int len = (int)sizeof sa;
    if (getsockname(s, (struct sockaddr *)&sa, &len) != 0) {
        shsm_net_set_error(err, WSAGetLastError());
        closesocket(s);
        return SHSM_NET_ERROR;
    }
    set_nonblocking(s, true);
    if (bound_port) *bound_port = ntohs(sa.sin_port);
    *out = (ShsmSocket)s;
    return SHSM_NET_OK;
}

ShsmNetStatus shsm_net_accept(ShsmSocket listener, double timeout, ShsmSocket *out, ShsmNetError *err) {
    shsm_net_init();
    *out = SHSM_SOCKET_INVALID;
    if (listener == SHSM_SOCKET_INVALID) {
        shsm_net_set_error(err, WSAENOTSOCK);
        return SHSM_NET_ERROR;
    }
    int r = wait_socket((SOCKET)listener, false, timeout);
    if (r < 0) {
        shsm_net_set_error(err, WSAGetLastError());
        return SHSM_NET_ERROR;
    }
    if (r == 0) {
        set_timeout_error(err);
        return SHSM_NET_TIMEOUT;
    }
    SOCKET c = accept((SOCKET)listener, NULL, NULL);
    if (c == INVALID_SOCKET) {
        int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK) {
            set_timeout_error(err);
            return SHSM_NET_TIMEOUT;
        }
        shsm_net_set_error(err, e);
        return SHSM_NET_ERROR;
    }
    set_nonblocking(c, true);
    *out = (ShsmSocket)c;
    return SHSM_NET_OK;
}

void shsm_net_close(ShsmSocket s) {
    if (s == SHSM_SOCKET_INVALID) return;
    closesocket((SOCKET)s);
}

int shsm_net_free_port(void) {
    ShsmSocket s;
    int port = 0;
    ShsmNetError err;
    shsm_net_init();
    SOCKET raw = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (raw == INVALID_SOCKET) {
        shsm_net_set_error(&err, WSAGetLastError());
        shsm_raise(SHSM_EXC_OS, "%s", err.text);
    }
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = 0;
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    int len = (int)sizeof sa;
    if (bind(raw, (struct sockaddr *)&sa, (int)sizeof sa) != 0 || getsockname(raw, (struct sockaddr *)&sa, &len) != 0) {
        shsm_net_set_error(&err, WSAGetLastError());
        closesocket(raw);
        shsm_raise(SHSM_EXC_OS, "%s", err.text);
    }
    port = ntohs(sa.sin_port);
    s = (ShsmSocket)raw;
    shsm_net_close(s);
    return port;
}
