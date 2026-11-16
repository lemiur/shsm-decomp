# shsm_net: TCP sockets (Winsock)

Ports the socket calls of `tools/dolphin.py` (`socket.create_connection`, `settimeout`, `sendall`, `recv`, a listening
socket, `free_port()`). Header `lib/shsm_net.h`; links `ws2_32` through `#pragma comment(lib, "ws2_32.lib")` in
`shsm_net.c` (every executable that pulls the object in gets it; no CMake change).

## Model

- `ShsmSocket` is a `u64` holding the Winsock `SOCKET`; `SHSM_SOCKET_INVALID` is the closed/none value.
- Every socket this module returns is in non-blocking mode; each operation waits with `select()` up to its own timeout.
  That is the Python `settimeout(t)` behaviour, with the timeout passed per call instead of stored on the socket.
- Results are a `ShsmNetStatus`: `SHSM_NET_OK`, `SHSM_NET_TIMEOUT` (nothing within the timeout; for a zero timeout:
  nothing waiting, Python's `BlockingIOError`), `SHSM_NET_CLOSED` (the peer closed: `recv` returned 0 bytes),
  `SHSM_NET_ERROR` (an OS error).
- A `ShsmNetError` receives `winerror` and `text`, which is `str(e)` of the Python exception: `"[WinError 10061] No
  connection could be made because the target machine actively refused it"` (FormatMessage text, through
  `avp_win_error_str`, the same function Python's message comes from), `"timed out"` for a timeout, `"[Errno 11001]
  getaddrinfo failed"` for a host that does not resolve. Nothing raises except `shsm_net_free_port`.
- `shsm_net_init()` runs `WSAStartup(2.2)` once per process (`avp_once`, thread-safe); every function calls it.

## API

| function | Python |
|---|---|
| `shsm_net_connect(host, port, timeout, &sock, &err)` | `socket.create_connection((host, port), timeout)`; timeout <= 0: none; getaddrinfo, each address tried in turn, the last error reported |
| `shsm_net_set_nodelay(sock)` | `setsockopt(IPPROTO_TCP, TCP_NODELAY, 1)` |
| `shsm_net_send_all(sock, p, n, timeout, &err)` | `sendall` with a total timeout (<= 0: none) |
| `shsm_net_recv(sock, buf, cap, timeout, &got, &err)` | `recv(cap)` after up to `timeout` s; 0 polls; < 0 waits without limit |
| `shsm_net_listen(host, port, backlog, &sock, &bound_port, &err)` | `socket(); bind((host or "127.0.0.1", port)); listen(backlog); getsockname()[1]` |
| `shsm_net_accept(listener, timeout, &conn, &err)` | `settimeout(timeout); accept()` |
| `shsm_net_close(sock)` | `close()` (invalid ignored) |
| `shsm_net_free_port()` | `dolphin.free_port()`; raises `SHSM_EXC_OS` with the OSError text |
| `shsm_net_set_error(&err, wsaerror)` | fills `err` for a WSA code |

Operations on `SHSM_SOCKET_INVALID` fail with WSAENOTSOCK (`"[WinError 10038] An operation was attempted on something
that is not a socket"`), as Python's do on a closed socket.

## Example

    ShsmSocket s;
    ShsmNetError err;
    if (shsm_net_connect("127.0.0.1", port, 3.0, &s, &err) != SHSM_NET_OK)
        shsm_raise(SHSM_EXC_OS, "%s", err.text);
    shsm_net_send_all(s, "ping\n", 5, 5.0, &err);
    char buf[256];
    size_t got;
    switch (shsm_net_recv(s, buf, sizeof buf, 5.0, &got, &err)) { ... }
    shsm_net_close(s);

## Tests

`tests/shsm_gdb_test.c` exercises the module through the GDB client and broker (connect, refused connection and its
`[WinError 10061]` text, timeouts, a closed peer, listen/accept, `free_port`).

## Provenance

Written for this project; no code was taken from another project of the author's (whose tools/c has an unrelated Winsock HTTP
module in `core/src/http.c`).
