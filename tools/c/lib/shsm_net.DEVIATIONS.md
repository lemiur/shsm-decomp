# shsm_net deviations

## D-shsm_net-1. OSError texts are rebuilt, not raised by Python
- Behaviour: error texts are built from the WSA error code as `"[WinError N] <FormatMessage text>"` (`avp_win_error_str`),
  `"timed out"` for timeouts and `"[Errno N] getaddrinfo failed"` for resolution failures. For the errors the tools
  meet (10061 refused, 10054 reset, 10038 not a socket) this is Python's `str(e)` exactly (the refused text is checked in
  `tests/shsm_gdb_test.c` `connect_refused`); an error Python reports through another path (an `errno`-style
  `[Errno N]` OSError from the C runtime) could be spelled differently.
- Reason: Python formats the same Windows message through the same FormatMessage call; the remaining cases do not occur
  with local TCP sockets.

## D-shsm_net-2. Timeouts are per call, sockets always non-blocking
- Behaviour: a timeout is an argument of each operation instead of state set with `settimeout`; select() granularity
  (microseconds) instead of Python's poll() (milliseconds, rounded up).
- Reason: no observable difference in output; the callers pass the timeout the Python had set.
