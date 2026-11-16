# shsm_gdb: GDB remote protocol client, session broker, target interface

Ports `tools/dolphin.py` lines 1-660: the protocol helpers (`checksum`, `frame`, `unescape`, `is_stop_reply`,
`parse_stop`, `is_hex`, `check_error`), `GdbClient`, `RPC_METHODS`, `_enc`/`_dec`, `Broker` and `BrokerClient`, plus
`ShsmGdbTarget`, one interface over either client for the dolphin tool and its MCP server. Header `lib/shsm_gdb.h`;
sources `lib/shsm_gdb.c` (helpers, client, dispatcher) and `lib/shsm_gdb_broker.c` (JSON forms, broker, broker client,
target); `lib/shsm_gdb_priv.h` holds helpers shared by the two files (not API). Sockets come from `shsm_net`; the fake
stub for tests is `shsm_gdbfake`.

## Constants

`SHSM_GDB_DEFAULT_TIMEOUT` 5.0, `SHSM_GDB_CONNECT_TIMEOUT` 3.0, `SHSM_GDB_MEM_CHUNK` 0x400, `SHSM_GDB_RETRIES` 3,
`SHSM_GDB_REG_PC/MSR/CR/LR/CTR/XER/FPSCR/PVR` (0x40-0x46, 0x57). `SHSM_GDB_NONE` (0.0) is Python's `timeout=None`:
every `timeout` parameter equal to 0 means "the client's own timeout" (Python's `timeout or self.timeout`).

## Exceptions

| code | Python class | broker `kind` |
|---|---|---|
| `SHSM_EXC_GDB` (96) | `GdbError` | `error` (also `TypeError`/`ValueError`: `SHSM_EXC_TYPE`/`SHSM_EXC_VALUE`) |
| `SHSM_EXC_GDB_TIMEOUT` (97) | `GdbTimeout` | `timeout` |
| `SHSM_EXC_GDB_DISCONNECTED` (98) | `GdbDisconnected` | `disconnected` |

All three are `cli.UsageError` subclasses: uncaught, `shsm_cli_main` prints `<prog>: error: <message>` and returns 2
(it does so for every code but `SHSM_EXC_FINDING`). `shsm_exc_is_gdb(code)` is `except GdbError` (any of the three);
`shsm_gdb_exc_class(code)` gives the class name. Codes 96-98 do not collide with `shsm_cli.h` (64-72) or core (1-5).
The message texts are the Python's, byte for byte:

    cannot connect to the GDB stub at %s:%d: <OSError>     timed out waiting for the GDB stub
    connection to the GDB stub lost: <OSError>             the GDB stub closed the connection
    the stub rejected %r %d times                          stub refused <cmd[:16] before ','>: Exx
    the stub does not report register 0x%x                 short read at 0x%08X: %d of %d bytes
    the stub did not confirm the write at 0x%08X           the stub does not support breakpoint type Z%d
    no stop reply after a single step                      the CPU did not stop after an interrupt
    the CPU is running; pause it first
    the session broker does not answer on port %d (<OSError>); run `quit`, then `launch`
    the session broker did not answer <method>             lost the session broker: <OSError>
    the session broker closed the connection

Exceptions are `avp_throw` codes: catch with `AVP_TRY`, test `t.code`, read `t.msg`. As everywhere in this tree, a
throw does not unwind: allocate from arenas between `AVP_TRY` and the call.

## Protocol helpers and values

Packet payloads are `AvpStr` of raw bytes (Python decodes them as Latin-1: one byte per character, not UTF-8). JSON
and message conversions turn each byte into its code point.

- `shsm_gdb_checksum(p, n)`, `shsm_gdb_frame(a, payload)` (`$...#cc`, escaping `$#}*`), `shsm_gdb_unescape(a, raw)`
  (`}` escapes and `*` RLE), `shsm_gdb_is_stop_reply(p)`, `shsm_gdb_is_hex(p)`, `shsm_gdb_check_error(cmd, p)`.
- `ShsmGdbStop` (parse_stop's dict): `kind` (`'T'`, `'S'`, `'W'`, `'X'`; `'O'` only from halt_reason), `signal`,
  `regs[nregs]` (`{num, value}`, dict order, the key is `"%02x" % num`), `has_pc`/`pc` (register 0x40), `has_sp`/`sp`
  (register 1), `raw`. `shsm_gdb_parse_stop(a, p)`, `shsm_gdb_stop_copy`, `shsm_gdb_stop_to_json` (key order kind,
  signal, regs, pc, sp, raw; NULL -> `null`), `shsm_gdb_stop_from_json`.
- `ShsmGdbBreakpoint` `{type, address, length}`; `shsm_gdb_breakpoints_to_json` (`[{"type","address","length"}]`).
- `ShsmGdbState` (state()): `running`, `exited`, `stop` (NULL: None), `bps[nbps]` sorted by (address, type),
  `bad_checksums`; `shsm_gdb_state_to_json` (key order running, exited, stop, breakpoints, bad_checksums),
  `shsm_gdb_state_from_json`.
- `ShsmBytes {const u8 *p; size_t n}`: memory. `shsm_gdb_json_from_bytes` is `_enc` (`{"__bytes__": hex}`),
  `shsm_gdb_json_bytes(a, v, &out)` is `_dec` (false when `v` is not such a dict).

## ShsmGdbClient (GdbClient)

    ShsmGdbClient *shsm_gdb_connect(host, port, timeout, wait);   /* retries every 0.25 s for `wait` s */
    ShsmGdbClient *shsm_gdb_client_new(sock, timeout, retries);   /* takes a connected socket */
    void shsm_gdb_client_free(c);                                 /* closes (if open) and frees */

State (read-only): `shsm_gdb_running`, `_exited`, `_closed`, `_bad_checksums`, `_last_stop` (owned by the client),
`_timeout`. Transport: `shsm_gdb_poll`, `shsm_gdb_send_packet(c, payload, timeout)`, `shsm_gdb_request(a, c, payload,
accept, timeout)` (the accept predicate is `bool (*)(AvpStr)`), `shsm_gdb_drain(c, 0.4, 3.0)`.

Commands (same names, arguments and order as the Python methods; results from `a`, NULL stop = None):

| C | Python | notes |
|---|---|---|
| `shsm_gdb_halt_reason(a, c)` | `halt_reason()` | `?`; sets last_stop |
| `shsm_gdb_read_gprs(c, u64 out[32])` | `read_gprs()` | returns the count (32 from Dolphin) |
| `shsm_gdb_read_register(c, num)` | `read_register(num)` | `p<num>` |
| `shsm_gdb_read_registers(c, nums, n, out)` | `read_registers(nums)` | |
| `shsm_gdb_read_memory(a, c, addr, length)` | `read_memory(addr, length)` | `m` packets of MEM_CHUNK |
| `shsm_gdb_write_memory(c, addr, data, n)` | `write_memory(addr, data)` | `M` packets |
| `shsm_gdb_insert_breakpoint(c, addr, kind, length)` | `insert_breakpoint(addr, kind=0, length=4)` | `Z` |
| `shsm_gdb_remove_breakpoint(c, addr, kind, length)` | `remove_breakpoint(...)` | `z` |
| `shsm_gdb_breakpoints(a, c, &out)` | `breakpoints()` | sorted by (address, type) |
| `shsm_gdb_clear_breakpoints(c)` | `clear_breakpoints()` | in insertion order |
| `shsm_gdb_step(a, c, count, timeout)` | `step(count=1, timeout=None)` | steps off an exec breakpoint at the PC (removed, stepped, reinserted) |
| `shsm_gdb_resume(c, timeout)` | `resume(timeout=None)` | false when already running; true when it stepped onto another breakpoint |
| `shsm_gdb_wait_stop(a, c, timeout)` | `wait_stop(timeout)` | NULL: still running after `timeout` (or never stopped) |
| `shsm_gdb_interrupt(a, c, timeout)` | `interrupt(timeout=None)` | 0x03, then drains the extra packets |
| `shsm_gdb_state(a, c)` | `state()` | |
| `shsm_gdb_detach(c)`, `shsm_gdb_kill(c)`, `shsm_gdb_close(c)` | `detach()`, `kill()`, `close()` | |

`shsm_gdb_dispatch(a, c, method, args)` is the broker's `getattr(gdb, method)(*args)`: JSON arguments (an array; each
`{"__bytes__"}` decoded first, as `_dec`), JSON result (`_enc`ed). Argument-count errors raise `SHSM_EXC_TYPE` with
Python's text (`GdbClient.read_memory() missing 1 required positional argument: 'length'`). `shsm_gdb_rpc_methods`
(NULL-terminated) and `shsm_gdb_is_rpc_method` are `RPC_METHODS`.

Ownership: the client's own state is on the heap; temporaries go to `avp_scratch()` (reset on normal return) or to
the arena passed in. One thread at a time.

## Broker and BrokerClient

    ShsmBroker *shsm_broker_new(gdb, token, host /*NULL: 127.0.0.1*/, port /*0: any*/, alive, log, ctx);
    int shsm_broker_port(b);  bool shsm_broker_stopping(b);
    void shsm_broker_serve(b);          /* blocks: until shutdown, alive() false, or a lost stub; closes gdb */
    AvpJson *shsm_broker_handle(a, b, req);              /* Broker.handle */
    AvpStr shsm_broker_handle_line(a, b, line);          /* one request line -> json.dumps(resp) + "\n" */
    void shsm_broker_free(b);           /* after serve; gdb stays the caller's to free */

`alive(ctx)` (NULL: always alive) and `log(ctx, msg)` (NULL: silent) are the Python's callbacks. serve() accepts with a
1 s timeout, polls the stub between connections, serves one connection at a time with a 600 s read timeout, one
JSON line per request, answered with `json.dumps(resp)` (default separators, ensure_ascii: the same bytes as the
Python broker). Responses: `{"ok": true, "result": ...}` or `{"ok": false, "error": ..., "kind": ...}` with kinds
`auth` (`bad token`), `usage` (`bad request`, `unknown method %r` with Python's repr of the value), `timeout`,
`disconnected` (the broker then stops), `error`. Methods `shutdown` (kills the stub connection, stops) and `ping`.

    ShsmBrokerClient *shsm_broker_client_new(port, token, host /*NULL*/, timeout /*60.0*/);
    AvpJson *shsm_broker_call(a, b, method, args /*JSON array or NULL*/, timeout /*0: keep*/);
    void shsm_broker_client_shutdown(a, b);
    void shsm_broker_client_free(b);

`shsm_broker_call` returns the raw `result` (bytes still as `{"__bytes__"}`) and raises the class the `kind` names.

The wire format interoperates with the Python in both directions (checked by hand: a Python `BrokerClient` drives
the C broker, the C client drives a Python `Broker`; see "Interop check" below).

## ShsmGdbTarget: the interface the tool and the MCP server use

    ShsmGdbTarget t = shsm_gdb_target_direct(client);     /* a GdbClient in this process */
    ShsmGdbTarget t = shsm_gdb_target_broker(bclient);    /* the session broker */

A tagged struct (`kind`, `client`, `broker`) passed by pointer; it owns nothing (free the client yourself). One
function per RPC method, typed:

    ShsmGdbState  shsm_target_state(a, t);
    ShsmGdbStop  *shsm_target_halt_reason(a, t);
    size_t        shsm_target_read_gprs(t, u64 out[32]);
    u64           shsm_target_read_register(t, num);
    void          shsm_target_read_registers(t, nums, n, out);
    ShsmBytes     shsm_target_read_memory(a, t, addr, length);
    void          shsm_target_write_memory(t, addr, data, n);
    void          shsm_target_insert_breakpoint(t, addr, kind, length);   /* Python defaults: kind 0, length 4 */
    void          shsm_target_remove_breakpoint(t, addr, kind, length);
    size_t        shsm_target_breakpoints(a, t, &out);
    void          shsm_target_clear_breakpoints(t);
    bool          shsm_target_resume(t, timeout);                         /* SHSM_GDB_NONE: default */
    ShsmGdbStop  *shsm_target_wait_stop(a, t, timeout);                   /* NULL: still running */
    ShsmGdbStop  *shsm_target_step(a, t, count, timeout);
    ShsmGdbStop  *shsm_target_interrupt(a, t, timeout);
    AvpJson      *shsm_target_call(a, t, method, args);                   /* generic: JSON in, JSON out */

Errors are the same exceptions either way (a broker error arrives as the class its `kind` names, with the same text).
The broker form uses `BrokerClient.__getattr__`'s socket timeout: 60 s, plus the wait for `wait_stop`. A timeout of
`SHSM_GDB_NONE` is left out of the broker call, as Python's default. Results are allocated from `a` (or nothing is
returned); the typed broker functions without an arena use `avp_scratch()` internally.

Example (a `cont`-like operation):

    ShsmGdbTarget t = broker ? shsm_gdb_target_broker(bc) : shsm_gdb_target_direct(gdb);
    shsm_target_resume(&t, SHSM_GDB_NONE);
    ShsmGdbStop *st = shsm_target_wait_stop(a, &t, 10.0);
    if (!st) shsm_raise(SHSM_EXC_FINDING, "still running after 10 s");
    printf("pc %08llx\n", (unsigned long long)st->pc);

## Tests

`tests/shsm_gdb_test.c`: TestFraming, TestClient (registers/memory/breakpoints, bad checksum retried, NAK resends,
stalled reply times out, dropped connection, connect refused, resume steps off a breakpoint, step, interrupt drains
extra packets) and TestBroker (round trip and token) against `shsm_gdbfake`, plus handle_line byte checks and the
direct target. `interop_by_hand` does nothing unless `SHSM_GDB_INTEROP_STUB=<port>` (serve a C broker, token `tok`,
on that stub; prints `PORT <n>`) or `SHSM_GDB_INTEROP_BROKER=<port>` (drive a broker with the C client) is set.

## Interop check

By hand (not part of CTest): a Python script starts `tools/test_dolphin.py`'s `FakeStub`, runs
`test_shsm_gdb_test.exe --filter interop_by_hand` with `SHSM_GDB_INTEROP_STUB`, and drives the C broker with
`dolphin.BrokerClient` (halt_reason, read/write memory, breakpoints, state, wait_stop, step, an E01 error, an unknown
method, a raw response line compared with the Python's bytes); then starts a Python `Broker` and runs the exe with
`SHSM_GDB_INTEROP_BROKER`. Both directions gave identical results.

## Provenance

Written for this project from `tools/dolphin.py`; no code taken from another project of the author's.
