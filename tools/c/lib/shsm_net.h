/* TCP sockets for the tools (Winsock): a client connection with a connect timeout, send-all and receive with
 * timeouts, a local listener, and dolphin.py's free_port(). Every blocking operation takes a timeout. Documented in
 * shsm_net.md.
 *
 * Sockets are kept in non-blocking mode; the timeouts are implemented with select(). Failures come back as a status
 * plus a ShsmNetError whose text is what str(e) of Python's OSError gives: "[WinError 10061] No connection could be
 * made because the target machine actively refused it", "timed out" for a timeout (TimeoutError), "[Errno 11001]
 * getaddrinfo failed" for a host that does not resolve. Nothing here raises, except shsm_net_free_port. */
#pragma once
#include "avp/base.h"

typedef u64 ShsmSocket;
#define SHSM_SOCKET_INVALID ((ShsmSocket) ~(u64)0)

typedef enum {
    SHSM_NET_OK = 0,
    SHSM_NET_TIMEOUT, /* nothing within the timeout (also: nothing waiting, for a zero timeout) */
    SHSM_NET_CLOSED,  /* the peer closed the connection (recv returned 0 bytes) */
    SHSM_NET_ERROR    /* an OS error: err->winerror, err->text */
} ShsmNetStatus;

typedef struct {
    int winerror;    /* the WSA error code; 0 for a timeout or a closed connection */
    char text[512];  /* str(OSError): "[WinError N] <message>", "timed out", "" when closed */
} ShsmNetError;

/* WSAStartup(2.2), once per process (thread-safe); every function below calls it. A failure is fatal. */
void shsm_net_init(void);

/* socket.create_connection((host, port), timeout): resolves host (getaddrinfo, any family), tries each address in
 * turn within `timeout` seconds each (<= 0: no timeout) and returns the first that connects (non-blocking mode).
 * On failure the last address's error. */
ShsmNetStatus shsm_net_connect(const char *host, int port, double timeout, ShsmSocket *out, ShsmNetError *err);
/* setsockopt(IPPROTO_TCP, TCP_NODELAY, 1) */
void shsm_net_set_nodelay(ShsmSocket s);
/* socket.sendall with a total timeout (<= 0: none). An invalid socket is SHSM_NET_ERROR (WSAENOTSOCK). */
ShsmNetStatus shsm_net_send_all(ShsmSocket s, const void *p, size_t n, double timeout, ShsmNetError *err);
/* socket.recv(cap) after waiting up to `timeout` seconds for data: OK with *got > 0, CLOSED (*got 0), TIMEOUT, or
 * ERROR. timeout 0 polls without waiting (Python's non-blocking recv: TIMEOUT stands for BlockingIOError);
 * timeout < 0 waits without limit. */
ShsmNetStatus shsm_net_recv(ShsmSocket s, void *buf, size_t cap, double timeout, size_t *got, ShsmNetError *err);
/* A listening TCP socket on host:port (port 0: any free port; host NULL: "127.0.0.1"), backlog as listen(n). The
 * bound port in *bound_port. */
ShsmNetStatus shsm_net_listen(const char *host, int port, int backlog, ShsmSocket *out, int *bound_port,
                              ShsmNetError *err);
/* accept() waiting up to `timeout` seconds (< 0: no limit); the connection comes back in non-blocking mode. */
ShsmNetStatus shsm_net_accept(ShsmSocket listener, double timeout, ShsmSocket *out, ShsmNetError *err);
/* close(); SHSM_SOCKET_INVALID is ignored. */
void shsm_net_close(ShsmSocket s);
/* dolphin.free_port(): binds 127.0.0.1 port 0, notes the port, closes. Raises SHSM_EXC_OS with the OSError text when
 * the bind fails. */
int shsm_net_free_port(void);
/* Fills err for a WSA error code: winerror and "[WinError N] <FormatMessage text>". */
void shsm_net_set_error(ShsmNetError *err, int winerror);
