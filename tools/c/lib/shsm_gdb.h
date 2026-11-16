/* The GDB remote serial protocol client of tools/dolphin.py (lines 1-660): framing, the GdbClient state machine and
 * its commands, the session broker (Broker) that serves them as JSON lines over a local socket, its client
 * (BrokerClient), and ShsmGdbTarget, one interface over a direct client or a broker client. Documented in
 * shsm_gdb.md (API, ownership, errors, examples).
 *
 * Text. Packets are bytes; the Python decodes them as Latin-1. Here a packet payload is an AvpStr of those bytes
 * (one byte per character, not UTF-8); conversions to JSON or to messages turn each byte into its code point. */
#pragma once
#include "avp/json.h"
#include "shsm_net.h"

#define SHSM_GDB_DEFAULT_TIMEOUT 5.0 /* dolphin.DEFAULT_TIMEOUT: one packet round trip */
#define SHSM_GDB_CONNECT_TIMEOUT 3.0 /* dolphin.CONNECT_TIMEOUT */
#define SHSM_GDB_MEM_CHUNK 0x400     /* dolphin.MEM_CHUNK: bytes per `m`/`M` packet */
#define SHSM_GDB_RETRIES 3           /* GdbClient(retries=3) */
/* A timeout argument of 0 is Python's None: the client's own timeout. */
#define SHSM_GDB_NONE 0.0

enum {
    SHSM_GDB_REG_PC = 0x40,
    SHSM_GDB_REG_MSR = 0x41,
    SHSM_GDB_REG_CR = 0x42,
    SHSM_GDB_REG_LR = 0x43,
    SHSM_GDB_REG_CTR = 0x44,
    SHSM_GDB_REG_XER = 0x45,
    SHSM_GDB_REG_FPSCR = 0x46,
    SHSM_GDB_REG_PVR = 0x57
};

/* ---- exceptions ------------------------------------------------------------------------------------------------
 * dolphin.GdbError and its subclasses, all cli.UsageError (exit 2): shsm_cli_main prints "<prog>: error: <message>"
 * and returns 2 for them as for any code but SHSM_EXC_FINDING. Codes 96-98 (shsm_cli.h uses 64-72). */
enum {
    SHSM_EXC_GDB = 96,        /* GdbError: the stub refused a command or the conversation failed */
    SHSM_EXC_GDB_TIMEOUT,     /* GdbTimeout */
    SHSM_EXC_GDB_DISCONNECTED /* GdbDisconnected */
};
bool shsm_exc_is_gdb(int exc);           /* except GdbError (any of the three) */
const char *shsm_gdb_exc_class(int exc); /* "GdbError" / "GdbTimeout" / "GdbDisconnected", else shsm_exc_class */

/* ---- protocol helpers ---- */
typedef struct {
    const u8 *p;
    size_t n;
} ShsmBytes;

u8 shsm_gdb_checksum(const void *p, size_t n);
/* frame(payload): "$<escaped>#cc" (escaping $ # } *) */
AvpStr shsm_gdb_frame(AvpArena *a, AvpStr payload);
/* unescape(raw): undoes } escapes and * run-length encoding */
AvpStr shsm_gdb_unescape(AvpArena *a, AvpStr raw);
bool shsm_gdb_is_stop_reply(AvpStr p); /* T/S/W/X stop replies, O<hex> console output (not OK) */
bool shsm_gdb_is_hex(AvpStr p);
/* check_error(cmd, p): raises SHSM_EXC_GDB "stub refused <cmd before ',', 16 chars>: Exx" for an Exx reply. */
AvpStr shsm_gdb_check_error(AvpStr cmd, AvpStr p);

/* A stop reply, parse_stop's dict. */
typedef struct {
    u64 num;    /* register number (the dict key is "%02x" % num) */
    u64 value;
} ShsmGdbReg;
typedef struct ShsmGdbStop {
    char kind;          /* p[0]: 'T', 'S', 'W', 'X' (or 'O' when halt_reason got console output) */
    int signal;         /* int(p[1:3], 16) */
    ShsmGdbReg *regs;   /* "nn:value" pairs of a T reply, dict order */
    size_t nregs;
    bool has_pc, has_sp; /* "pc" / "sp": register 0x40 / 1, else None */
    u64 pc, sp;
    AvpStr raw;         /* the payload (Latin-1 bytes) */
} ShsmGdbStop;
ShsmGdbStop *shsm_gdb_parse_stop(AvpArena *a, AvpStr p);
/* {"kind", "signal", "regs": {"%02x": value}, "pc", "sp", "raw"} in Python's key order; NULL stop -> null */
AvpJson *shsm_gdb_stop_to_json(AvpArena *a, const ShsmGdbStop *st);
/* The reverse (a broker's result); null -> NULL. Raises SHSM_EXC_GDB on a malformed object. */
ShsmGdbStop *shsm_gdb_stop_from_json(AvpArena *a, const AvpJson *v);
ShsmGdbStop *shsm_gdb_stop_copy(AvpArena *a, const ShsmGdbStop *st); /* deep copy; NULL -> NULL */

typedef struct {
    i64 type;     /* Z type: 0 exec, 1 hw, 2 write, 3 read, 4 access */
    i64 address;
    i64 length;
} ShsmGdbBreakpoint;

/* GdbClient.state() */
typedef struct {
    bool running, exited;
    ShsmGdbStop *stop;            /* last stop, NULL for None */
    ShsmGdbBreakpoint *bps;       /* breakpoints(): sorted by (address, type) */
    size_t nbps;
    i64 bad_checksums;
} ShsmGdbState;
/* {"running", "exited", "stop", "breakpoints": [{"type", "address", "length"}], "bad_checksums"} */
AvpJson *shsm_gdb_state_to_json(AvpArena *a, const ShsmGdbState *s);
ShsmGdbState shsm_gdb_state_from_json(AvpArena *a, const AvpJson *v);
AvpJson *shsm_gdb_breakpoints_to_json(AvpArena *a, const ShsmGdbBreakpoint *bps, size_t n);

/* ---- GdbClient ---------------------------------------------------------------------------------------------------
 * One connection to a stub. Every wait has a deadline: a stalled stub raises SHSM_EXC_GDB_TIMEOUT, a closed
 * connection SHSM_EXC_GDB_DISCONNECTED. One thread at a time. Results are allocated from the caller's arena; the
 * client's own state is on the heap (shsm_gdb_client_free). */
typedef struct ShsmGdbClient ShsmGdbClient;
/* GdbClient(sock, timeout, retries): takes ownership of a connected socket. */
ShsmGdbClient *shsm_gdb_client_new(ShsmSocket sock, double timeout, int retries);
/* GdbClient.connect(host, port, timeout, wait): retries every 0.25 s for up to `wait` seconds; raises
 * SHSM_EXC_GDB_DISCONNECTED "cannot connect to the GDB stub at <host>:<port>: <OSError text>". */
ShsmGdbClient *shsm_gdb_connect(const char *host, int port, double timeout, double wait);
void shsm_gdb_client_free(ShsmGdbClient *c); /* closes the socket (if still open) and frees the client */

/* read-only views of the state */
bool shsm_gdb_running(const ShsmGdbClient *c);
bool shsm_gdb_exited(const ShsmGdbClient *c);
bool shsm_gdb_closed(const ShsmGdbClient *c);
i64 shsm_gdb_bad_checksums(const ShsmGdbClient *c);
const ShsmGdbStop *shsm_gdb_last_stop(const ShsmGdbClient *c); /* owned by the client: valid until the next call */
double shsm_gdb_timeout(const ShsmGdbClient *c);

/* transport */
void shsm_gdb_poll(ShsmGdbClient *c);                                  /* read what has arrived, note stop replies */
void shsm_gdb_send_packet(ShsmGdbClient *c, AvpStr payload, double timeout); /* send, wait for '+', resend on '-' */
typedef bool (*ShsmGdbAccept)(AvpStr p);
/* send payload, return the first reply `accept` takes (Latin-1 bytes, from a); stop replies meanwhile are noted */
AvpStr shsm_gdb_request(AvpArena *a, ShsmGdbClient *c, AvpStr payload, ShsmGdbAccept accept, double timeout);
void shsm_gdb_drain(ShsmGdbClient *c, double quiet, double limit); /* drain(quiet=0.4, limit=3.0) */

/* commands (GdbClient methods; NULL stop pointers are None) */
ShsmGdbStop *shsm_gdb_halt_reason(AvpArena *a, ShsmGdbClient *c);
size_t shsm_gdb_read_gprs(ShsmGdbClient *c, u64 out[32]);                   /* the count (up to 32) */
u64 shsm_gdb_read_register(ShsmGdbClient *c, i64 num);
void shsm_gdb_read_registers(ShsmGdbClient *c, const i64 *nums, size_t n, u64 *out);
ShsmBytes shsm_gdb_read_memory(AvpArena *a, ShsmGdbClient *c, i64 addr, i64 length);
void shsm_gdb_write_memory(ShsmGdbClient *c, i64 addr, const void *data, size_t n);
void shsm_gdb_insert_breakpoint(ShsmGdbClient *c, i64 addr, i64 kind, i64 length); /* defaults kind 0, length 4 */
void shsm_gdb_remove_breakpoint(ShsmGdbClient *c, i64 addr, i64 kind, i64 length);
size_t shsm_gdb_breakpoints(AvpArena *a, const ShsmGdbClient *c, ShsmGdbBreakpoint **out);
void shsm_gdb_clear_breakpoints(ShsmGdbClient *c);
ShsmGdbStop *shsm_gdb_step(AvpArena *a, ShsmGdbClient *c, i64 count, double timeout);
bool shsm_gdb_resume(ShsmGdbClient *c, double timeout);
ShsmGdbStop *shsm_gdb_wait_stop(AvpArena *a, ShsmGdbClient *c, double timeout); /* NULL: still running */
ShsmGdbStop *shsm_gdb_interrupt(AvpArena *a, ShsmGdbClient *c, double timeout);
ShsmGdbState shsm_gdb_state(AvpArena *a, const ShsmGdbClient *c);
void shsm_gdb_detach(ShsmGdbClient *c);
void shsm_gdb_kill(ShsmGdbClient *c);
void shsm_gdb_close(ShsmGdbClient *c);

/* The broker's dispatcher: getattr(gdb, method)(*args) for one of the RPC methods, arguments and result as JSON
 * (bytes as {"__bytes__": hex} in the result; arguments already decoded by the caller are not: pass the raw JSON
 * arguments, the dispatcher decodes {"__bytes__"} as _dec does). Raises SHSM_EXC_TYPE for an argument count or type
 * Python would reject (the TypeError text for counts), SHSM_EXC_VALUE, or the GDB exceptions. method must be one of
 * shsm_gdb_rpc_methods. */
AvpJson *shsm_gdb_dispatch(AvpArena *a, ShsmGdbClient *c, AvpStr method, const AvpJson *args);
/* RPC_METHODS: the 15 names, NULL-terminated */
extern const char *const shsm_gdb_rpc_methods[];
bool shsm_gdb_is_rpc_method(AvpStr name);

/* ---- Broker -------------------------------------------------------------------------------------------------------- */
typedef struct ShsmBroker ShsmBroker;
typedef bool (*ShsmBrokerAlive)(void *ctx);
typedef void (*ShsmBrokerLog)(void *ctx, const char *msg);
/* Broker(gdb, token, host, port, alive, log): binds and listens (backlog 4); host NULL: 127.0.0.1. The broker does not
 * own gdb until serve() returns (serve closes it, as the Python's finally does). Raises SHSM_EXC_OS when the bind
 * fails. alive / log may be NULL. */
ShsmBroker *shsm_broker_new(ShsmGdbClient *gdb, const char *token, const char *host, int port, ShsmBrokerAlive alive,
                            ShsmBrokerLog log, void *ctx);
int shsm_broker_port(const ShsmBroker *b);
bool shsm_broker_stopping(const ShsmBroker *b);
/* handle(req): the response object (Python's key order) */
AvpJson *shsm_broker_handle(AvpArena *a, ShsmBroker *b, const AvpJson *req);
/* one request line (without or with its "\n") -> the response line json.dumps(resp) + "\n" */
AvpStr shsm_broker_handle_line(AvpArena *a, ShsmBroker *b, AvpStr line);
/* serve(): until shutdown, a dead emulator (alive() false) or a lost stub; closes the listener and gdb. */
void shsm_broker_serve(ShsmBroker *b);
void shsm_broker_free(ShsmBroker *b); /* frees the broker (after serve; closes the listener if serve never ran) */

/* ---- BrokerClient ---- */
typedef struct ShsmBrokerClient ShsmBrokerClient;
/* BrokerClient(port, token, host, timeout=60): raises SHSM_EXC_GDB_DISCONNECTED "the session broker does not answer
 * on port <port> (<OSError text>); run `quit`, then `launch`". host NULL: 127.0.0.1. */
ShsmBrokerClient *shsm_broker_client_new(int port, const char *token, const char *host, double timeout);
/* call(method, *args, timeout=timeout): args a JSON array (NULL: none; bytes as {"__bytes__": hex}); timeout 0 keeps
 * the socket's current timeout. Returns the result (bytes left as {"__bytes__"}: shsm_gdb_json_bytes decodes). */
AvpJson *shsm_broker_call(AvpArena *a, ShsmBrokerClient *b, const char *method, const AvpJson *args, double timeout);
void shsm_broker_client_shutdown(AvpArena *a, ShsmBrokerClient *b); /* call("shutdown") */
void shsm_broker_client_free(ShsmBrokerClient *b);                   /* close() and free */

/* _enc / _dec of bytes */
AvpJson *shsm_gdb_json_from_bytes(AvpArena *a, const void *p, size_t n); /* {"__bytes__": hex} */
/* {"__bytes__": hex} -> bytes (bytes.fromhex: SHSM_EXC_VALUE / SHSM_EXC_TYPE as Python); false when v is not such
 * a dict */
bool shsm_gdb_json_bytes(AvpArena *a, const AvpJson *v, ShsmBytes *out);

/* ---- ShsmGdbTarget: the RPC methods over a direct client or a broker client ------------------------------------- */
typedef enum { SHSM_GDB_TARGET_DIRECT, SHSM_GDB_TARGET_BROKER } ShsmGdbTargetKind;
typedef struct {
    ShsmGdbTargetKind kind;
    ShsmGdbClient *client;    /* DIRECT */
    ShsmBrokerClient *broker; /* BROKER */
} ShsmGdbTarget;
ShsmGdbTarget shsm_gdb_target_direct(ShsmGdbClient *c);
ShsmGdbTarget shsm_gdb_target_broker(ShsmBrokerClient *b);

/* One function per RPC method; the broker form calls it with BrokerClient.__getattr__'s timeout (60 s, plus the
 * wait for wait_stop). Timeouts equal to SHSM_GDB_NONE are left out of the call (Python's default None). */
ShsmGdbState shsm_target_state(AvpArena *a, ShsmGdbTarget *t);
ShsmGdbStop *shsm_target_halt_reason(AvpArena *a, ShsmGdbTarget *t);
size_t shsm_target_read_gprs(ShsmGdbTarget *t, u64 out[32]);
u64 shsm_target_read_register(ShsmGdbTarget *t, i64 num);
void shsm_target_read_registers(ShsmGdbTarget *t, const i64 *nums, size_t n, u64 *out);
ShsmBytes shsm_target_read_memory(AvpArena *a, ShsmGdbTarget *t, i64 addr, i64 length);
void shsm_target_write_memory(ShsmGdbTarget *t, i64 addr, const void *data, size_t n);
void shsm_target_insert_breakpoint(ShsmGdbTarget *t, i64 addr, i64 kind, i64 length);
void shsm_target_remove_breakpoint(ShsmGdbTarget *t, i64 addr, i64 kind, i64 length);
size_t shsm_target_breakpoints(AvpArena *a, ShsmGdbTarget *t, ShsmGdbBreakpoint **out);
void shsm_target_clear_breakpoints(ShsmGdbTarget *t);
bool shsm_target_resume(ShsmGdbTarget *t, double timeout);
ShsmGdbStop *shsm_target_wait_stop(AvpArena *a, ShsmGdbTarget *t, double timeout);
ShsmGdbStop *shsm_target_step(AvpArena *a, ShsmGdbTarget *t, i64 count, double timeout);
ShsmGdbStop *shsm_target_interrupt(AvpArena *a, ShsmGdbTarget *t, double timeout);
/* Generic form (an MCP server): method + JSON args -> JSON result, either way (bytes as {"__bytes__"}). */
AvpJson *shsm_target_call(AvpArena *a, ShsmGdbTarget *t, const char *method, const AvpJson *args);
