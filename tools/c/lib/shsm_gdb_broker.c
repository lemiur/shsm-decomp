/* dolphin.py's session broker (Broker), its client (BrokerClient), the JSON forms of stops and state, and
 * ShsmGdbTarget (shsm_gdb.md). */
#include "shsm_gdb.h"
#include "shsm_gdb_priv.h"

#include "avp/fmt.h"
#include "shsm_cli.h"

#include <stdio.h>

/* ---- JSON forms ---- */

AvpJson *shsm_gdb_json_from_bytes(AvpArena *a, const void *p, size_t n) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("__bytes__"), avp_json_str(a, gdbp_hex(a, p, n)));
    return o;
}

bool shsm_gdb_json_bytes(AvpArena *a, const AvpJson *v, ShsmBytes *out) {
    if (!avp_json_is_obj(v) || !avp_json_has(v, AVP_S("__bytes__"))) return false;
    const AvpJson *h = avp_json_getz(v, "__bytes__");
    if (!avp_json_is_str(h))
        shsm_raise(SHSM_EXC_TYPE, "fromhex() argument must be str or bytes-like, not %s", gdbp_type_name(h));
    *out = gdbp_fromhex(a, gdbp_utf8_to_latin1(a, h->u.str));
    return true;
}

static AvpJson *opt_uint(AvpArena *a, bool has, u64 v) { return has ? avp_json_uint(a, v) : avp_json_null(); }

AvpJson *shsm_gdb_stop_to_json(AvpArena *a, const ShsmGdbStop *st) {
    if (!st) return avp_json_null();
    AvpJson *o = avp_json_obj(a);
    char k = st->kind;
    avp_json_set(a, o, AVP_S("kind"), avp_json_str(a, gdbp_latin1_to_utf8(a, avp_strn(&k, 1))));
    avp_json_set(a, o, AVP_S("signal"), avp_json_int(a, st->signal));
    AvpJson *regs = avp_json_obj(a);
    for (size_t i = 0; i < st->nregs; i++) {
        char key[32];
        snprintf(key, sizeof key, "%02llx", (unsigned long long)st->regs[i].num);
        avp_json_set(a, regs, avp_str(key), avp_json_uint(a, st->regs[i].value));
    }
    avp_json_set(a, o, AVP_S("regs"), regs);
    avp_json_set(a, o, AVP_S("pc"), opt_uint(a, st->has_pc, st->pc));
    avp_json_set(a, o, AVP_S("sp"), opt_uint(a, st->has_sp, st->sp));
    avp_json_set(a, o, AVP_S("raw"), avp_json_str(a, gdbp_latin1_to_utf8(a, st->raw)));
    return o;
}

static u64 json_u64(const AvpJson *v) {
    if (v && v->kind == AVP_JSON_INT && !v->big) return (u64)v->u.i;
    if (v && v->kind == AVP_JSON_INT && v->big) {
        u64 r = 0;
        for (size_t i = 0; i < v->u.str.n; i++)
            if (v->u.str.p[i] >= '0' && v->u.str.p[i] <= '9') r = r * 10 + (u64)(v->u.str.p[i] - '0');
        return r;
    }
    return 0;
}

static void malformed(void) { shsm_raise(SHSM_EXC_GDB, "malformed reply from the session broker"); }

ShsmGdbStop *shsm_gdb_stop_from_json(AvpArena *a, const AvpJson *v) {
    if (avp_json_is_null(v)) return NULL;
    if (!avp_json_is_obj(v)) malformed();
    ShsmGdbStop *st = AVP_NEW(a, ShsmGdbStop);
    AvpStr kind = gdbp_utf8_to_latin1(a, avp_json_get_str(v, "kind", AVP_S("")));
    st->kind = kind.n ? kind.p[0] : 0;
    st->signal = (int)avp_json_get_i64(v, "signal", 0);
    const AvpJson *regs = avp_json_getz(v, "regs");
    if (avp_json_is_obj(regs)) {
        size_t n = avp_json_len(regs);
        st->regs = AVP_NEWN(a, ShsmGdbReg, n ? n : 1);
        for (size_t i = 0; i < n; i++) {
            const AvpJsonMember *m = avp_json_member(regs, i);
            u64 num = 0;
            gdbp_int16(m->key, &num);
            st->regs[st->nregs++] = (ShsmGdbReg){num, json_u64(m->val)};
        }
    }
    const AvpJson *pc = avp_json_getz(v, "pc"), *sp = avp_json_getz(v, "sp");
    st->has_pc = avp_json_is_int(pc);
    st->pc = json_u64(pc);
    st->has_sp = avp_json_is_int(sp);
    st->sp = json_u64(sp);
    st->raw = gdbp_utf8_to_latin1(a, avp_json_get_str(v, "raw", AVP_S("")));
    return st;
}

AvpJson *shsm_gdb_breakpoints_to_json(AvpArena *a, const ShsmGdbBreakpoint *bps, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("type"), avp_json_int(a, bps[i].type));
        avp_json_set(a, o, AVP_S("address"), avp_json_int(a, bps[i].address));
        avp_json_set(a, o, AVP_S("length"), avp_json_int(a, bps[i].length));
        avp_json_arr_push(a, arr, o);
    }
    return arr;
}

static size_t breakpoints_from_json(AvpArena *a, const AvpJson *v, ShsmGdbBreakpoint **out) {
    size_t n = avp_json_is_arr(v) ? avp_json_len(v) : 0;
    ShsmGdbBreakpoint *bps = AVP_NEWN(a, ShsmGdbBreakpoint, n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        const AvpJson *o = avp_json_at(v, i);
        bps[i] = (ShsmGdbBreakpoint){avp_json_get_i64(o, "type", 0), avp_json_get_i64(o, "address", 0),
                                     avp_json_get_i64(o, "length", 0)};
    }
    *out = bps;
    return n;
}

AvpJson *shsm_gdb_state_to_json(AvpArena *a, const ShsmGdbState *s) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("running"), avp_json_bool(s->running));
    avp_json_set(a, o, AVP_S("exited"), avp_json_bool(s->exited));
    avp_json_set(a, o, AVP_S("stop"), shsm_gdb_stop_to_json(a, s->stop));
    avp_json_set(a, o, AVP_S("breakpoints"), shsm_gdb_breakpoints_to_json(a, s->bps, s->nbps));
    avp_json_set(a, o, AVP_S("bad_checksums"), avp_json_int(a, s->bad_checksums));
    return o;
}

ShsmGdbState shsm_gdb_state_from_json(AvpArena *a, const AvpJson *v) {
    if (!avp_json_is_obj(v)) malformed();
    ShsmGdbState s = {0};
    s.running = avp_json_truthy(avp_json_getz(v, "running"));
    s.exited = avp_json_truthy(avp_json_getz(v, "exited"));
    s.stop = shsm_gdb_stop_from_json(a, avp_json_getz(v, "stop"));
    s.nbps = breakpoints_from_json(a, avp_json_getz(v, "breakpoints"), &s.bps);
    s.bad_checksums = avp_json_get_i64(v, "bad_checksums", 0);
    return s;
}

/* repr() of a json.loads value */
static void py_repr(AvpBuf *b, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) {
        avp_buf_putz(b, "None");
        return;
    }
    switch (v->kind) {
    case AVP_JSON_BOOL: avp_buf_putz(b, v->b ? "True" : "False"); break;
    case AVP_JSON_INT:
        if (v->big) avp_buf_put(b, v->u.str);
        else avp_buf_printf(b, "%lld", (long long)v->u.i);
        break;
    case AVP_JSON_FLOAT: {
        char tmp[32];
        avp_float_repr(v->u.f, tmp);
        avp_buf_putz(b, tmp);
        break;
    }
    case AVP_JSON_STRING: avp_buf_repr_str(b, v->u.str); break;
    case AVP_JSON_ARRAY:
        avp_buf_putc(b, '[');
        for (size_t i = 0; i < v->u.arr.n; i++) {
            if (i) avp_buf_putz(b, ", ");
            py_repr(b, v->u.arr.v[i]);
        }
        avp_buf_putc(b, ']');
        break;
    default:
        avp_buf_putc(b, '{');
        for (size_t i = 0; i < v->u.obj.n; i++) {
            if (i) avp_buf_putz(b, ", ");
            avp_buf_repr_str(b, v->u.obj.v[i].key);
            avp_buf_putz(b, ": ");
            py_repr(b, v->u.obj.v[i].val);
        }
        avp_buf_putc(b, '}');
        break;
    }
}

static const char *py_repr_cz(AvpArena *a, const AvpJson *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    py_repr(&b, v);
    return avp_buf_cz(&b);
}

static const char *py_str_cz(AvpArena *a, const AvpJson *v) {
    if (avp_json_is_str(v)) return avp_cz(a, v->u.str);
    return py_repr_cz(a, v);
}

/* ---- Broker ---- */

struct ShsmBroker {
    ShsmGdbClient *gdb;
    char *token;
    ShsmSocket lsock;
    int port;
    bool stopping;
    ShsmBrokerAlive alive;
    ShsmBrokerLog log;
    void *ctx;
    AvpArena *arena;
};

ShsmBroker *shsm_broker_new(ShsmGdbClient *gdb, const char *token, const char *host, int port, ShsmBrokerAlive alive,
                            ShsmBrokerLog log, void *ctx) {
    ShsmSocket s;
    int bound = 0;
    ShsmNetError err;
    if (shsm_net_listen(host, port, 4, &s, &bound, &err) != SHSM_NET_OK) shsm_raise(SHSM_EXC_OS, "%s", err.text);
    ShsmBroker *b = avp_xcalloc(1, sizeof *b);
    b->gdb = gdb;
    b->arena = avp_arena_new(0);
    b->token = avp_strdup(b->arena, token ? token : "");
    b->lsock = s;
    b->port = bound;
    b->alive = alive;
    b->log = log;
    b->ctx = ctx;
    return b;
}

int shsm_broker_port(const ShsmBroker *b) { return b->port; }
bool shsm_broker_stopping(const ShsmBroker *b) { return b->stopping; }

static AvpJson *fail(AvpArena *a, const char *error, const char *kind) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("ok"), avp_json_bool(false));
    avp_json_set(a, o, AVP_S("error"), avp_json_strz(a, error));
    avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, kind));
    return o;
}

static AvpJson *ok(AvpArena *a, AvpJson *result) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("ok"), avp_json_bool(true));
    avp_json_set(a, o, AVP_S("result"), result);
    return o;
}

/* list(req.get("args", [])) */
static AvpJson *args_list(AvpArena *a, const AvpJson *req) {
    const AvpJson *v = avp_json_getz(req, "args");
    AvpJson *arr = avp_json_arr(a);
    if (!avp_json_has(req, AVP_S("args"))) return arr;
    if (v->kind == AVP_JSON_ARRAY) return (AvpJson *)v;
    if (v->kind == AVP_JSON_STRING) { /* iterating a str gives its characters */
        AvpStr s = v->u.str;
        for (size_t i = 0; i < s.n;) {
            size_t k = 1;
            u8 c = (u8)s.p[i];
            if (c >= 0xF0) k = 4;
            else if (c >= 0xE0) k = 3;
            else if (c >= 0xC0) k = 2;
            if (i + k > s.n) k = s.n - i;
            avp_json_arr_push(a, arr, avp_json_str(a, avp_strn(s.p + i, k)));
            i += k;
        }
        return arr;
    }
    if (v->kind == AVP_JSON_OBJECT) { /* iterating a dict gives its keys */
        for (size_t i = 0; i < v->u.obj.n; i++) avp_json_arr_push(a, arr, avp_json_str(a, v->u.obj.v[i].key));
        return arr;
    }
    shsm_raise(SHSM_EXC_TYPE, "'%s' object is not iterable", gdbp_type_name(v));
}

AvpJson *shsm_broker_handle(AvpArena *a, ShsmBroker *b, const AvpJson *req) {
    if (!avp_json_is_obj(req)) return fail(a, "bad request", "usage"); /* D-shsm_gdb-3 */
    const AvpJson *tok = avp_json_getz(req, "token");
    if (!avp_json_is_str(tok) || !avp_str_eq(tok->u.str, avp_str(b->token))) return fail(a, "bad token", "auth");
    const AvpJson *method = avp_json_getz(req, "method");
    bool is_str = avp_json_is_str(method);
    if (is_str && avp_str_eq(method->u.str, AVP_S("shutdown"))) {
        b->stopping = true;
        AvpTry t;
        if (AVP_TRY(&t)) {
            shsm_gdb_kill(b->gdb);
            avp_try_end(&t);
        } else if (!shsm_exc_is_gdb(t.code)) {
            avp_throw(t.code, "%s", t.msg);
        }
        return ok(a, avp_json_bool(true));
    }
    if (is_str && avp_str_eq(method->u.str, AVP_S("ping"))) return ok(a, avp_json_bool(true));
    if (!is_str || !shsm_gdb_is_rpc_method(method->u.str))
        return fail(a, avp_sprintf(a, "unknown method %s", py_repr_cz(a, method)), "usage");
    AvpTry t;
    if (AVP_TRY(&t)) {
        AvpJson *args = args_list(a, req);
        AvpJson *result = shsm_gdb_dispatch(a, b->gdb, method->u.str, args);
        avp_try_end(&t);
        return ok(a, result);
    }
    if (t.code == SHSM_EXC_GDB_TIMEOUT) return fail(a, avp_strdup(a, t.msg), "timeout");
    if (t.code == SHSM_EXC_GDB_DISCONNECTED) {
        b->stopping = true;
        return fail(a, avp_strdup(a, t.msg), "disconnected");
    }
    if (t.code == SHSM_EXC_GDB || t.code == SHSM_EXC_TYPE || t.code == SHSM_EXC_VALUE)
        return fail(a, avp_strdup(a, t.msg), "error");
    avp_throw(t.code, "%s", t.msg);
}

AvpStr shsm_broker_handle_line(AvpArena *a, ShsmBroker *b, AvpStr line) {
    AvpJsonError err = {0};
    AvpJson *req = avp_json_parse_ex(a, line, AVP_JSON_BYTES, &err);
    AvpJson *resp = req ? shsm_broker_handle(a, b, req) : fail(a, "bad request", "usage");
    AvpBuf out;
    avp_buf_init(&out, a);
    AvpJsonDumpOpts o = {0};
    avp_json_dump_buf(&out, resp, &o);
    avp_buf_putc(&out, '\n');
    return avp_buf_str(&out);
}

#define CONN_TIMEOUT 600.0
#define RECV_SIZE 65536

static void serve_conn(ShsmBroker *b, ShsmSocket conn) {
    AvpBytes in = {0};
    u8 *rb = avp_xmalloc(RECV_SIZE);
    bool eof = false;
    AvpTry t;
    if (AVP_TRY(&t)) {
        for (;;) {
            u8 *nl = in.n ? memchr(in.v, '\n', in.n) : NULL;
            size_t take;
            if (nl) {
                take = (size_t)(nl - in.v) + 1;
            } else if (eof) {
                if (!in.n) break;
                take = in.n; /* the last line without its newline */
            } else {
                size_t got = 0;
                ShsmNetError err;
                ShsmNetStatus st = shsm_net_recv(conn, rb, RECV_SIZE, CONN_TIMEOUT, &got, &err);
                if (st == SHSM_NET_OK) avp_vec_extend(&in, rb, got);
                else if (st == SHSM_NET_CLOSED) eof = true;
                else break; /* a timeout or an error: OSError, the connection ends */
                continue;
            }
            AvpArenaMark m = avp_arena_mark(b->arena);
            AvpStr resp = shsm_broker_handle_line(b->arena, b, avp_strn((const char *)in.v, take));
            ShsmNetError err;
            ShsmNetStatus st = shsm_net_send_all(conn, resp.p, resp.n, CONN_TIMEOUT, &err);
            avp_arena_reset(b->arena, m);
            memmove(in.v, in.v + take, in.n - take);
            in.n -= take;
            if (st != SHSM_NET_OK) break;
            if (b->stopping) break;
        }
        avp_try_end(&t);
    } else {
        avp_vec_free(&in);
        avp_xfree(rb);
        shsm_net_close(conn);
        avp_throw(t.code, "%s", t.msg);
    }
    avp_vec_free(&in);
    avp_xfree(rb);
    shsm_net_close(conn);
}

static void broker_log(ShsmBroker *b, const char *msg) {
    if (b->log) b->log(b->ctx, msg);
}

static void serve_loop(ShsmBroker *b) {
    while (!b->stopping) {
        if (b->alive && !b->alive(b->ctx)) {
            broker_log(b, "emulator gone; broker exits");
            break;
        }
        AvpTry t;
        if (AVP_TRY(&t)) {
            shsm_gdb_poll(b->gdb);
            avp_try_end(&t);
        } else {
            if (t.code != SHSM_EXC_GDB_DISCONNECTED) avp_throw(t.code, "%s", t.msg);
            broker_log(b, t.msg);
            break;
        }
        ShsmSocket conn;
        ShsmNetError err;
        ShsmNetStatus st = shsm_net_accept(b->lsock, 1.0, &conn, &err);
        if (st == SHSM_NET_TIMEOUT) continue;
        if (st != SHSM_NET_OK) shsm_raise(SHSM_EXC_OS, "%s", err.text);
        serve_conn(b, conn);
    }
}

static void serve_cleanup(ShsmBroker *b) {
    shsm_net_close(b->lsock);
    b->lsock = SHSM_SOCKET_INVALID;
    shsm_gdb_close(b->gdb);
}

void shsm_broker_serve(ShsmBroker *b) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        serve_loop(b);
        avp_try_end(&t);
    } else {
        serve_cleanup(b);
        avp_throw(t.code, "%s", t.msg);
    }
    serve_cleanup(b);
}

void shsm_broker_free(ShsmBroker *b) {
    if (!b) return;
    shsm_net_close(b->lsock);
    avp_arena_free(b->arena);
    avp_xfree(b);
}

/* ---- BrokerClient ---- */

struct ShsmBrokerClient {
    ShsmSocket sock;
    char *token;
    double timeout;
    AvpBytes in;
    bool timed_out; /* the file object of a timed-out read refuses further reads */
};

ShsmBrokerClient *shsm_broker_client_new(int port, const char *token, const char *host, double timeout) {
    ShsmSocket s;
    ShsmNetError err;
    if (shsm_net_connect(host ? host : "127.0.0.1", port, SHSM_GDB_CONNECT_TIMEOUT, &s, &err) != SHSM_NET_OK)
        shsm_raise(SHSM_EXC_GDB_DISCONNECTED,
                   "the session broker does not answer on port %d (%s); run `quit`, then `launch`", port, err.text);
    ShsmBrokerClient *b = avp_xcalloc(1, sizeof *b);
    b->sock = s;
    size_t n = strlen(token ? token : "");
    b->token = avp_xmalloc(n + 1);
    memcpy(b->token, token ? token : "", n + 1);
    b->timeout = timeout;
    return b;
}

void shsm_broker_client_free(ShsmBrokerClient *b) {
    if (!b) return;
    shsm_net_close(b->sock);
    avp_vec_free(&b->in);
    avp_xfree(b->token);
    avp_xfree(b);
}

AvpJson *shsm_broker_call(AvpArena *a, ShsmBrokerClient *b, const char *method, const AvpJson *args, double timeout) {
    AvpJson *req = avp_json_obj(a);
    avp_json_set(a, req, AVP_S("token"), avp_json_strz(a, b->token));
    avp_json_set(a, req, AVP_S("method"), avp_json_strz(a, method));
    avp_json_set(a, req, AVP_S("args"), args ? (AvpJson *)args : avp_json_arr(a));
    if (timeout != 0) b->timeout = timeout;
    AvpBuf line;
    avp_buf_init(&line, a);
    AvpJsonDumpOpts o = {0};
    avp_json_dump_buf(&line, req, &o);
    avp_buf_putc(&line, '\n');
    ShsmNetError err;
    ShsmNetStatus st = shsm_net_send_all(b->sock, line.p, line.n, b->timeout, &err);
    if (st == SHSM_NET_TIMEOUT) shsm_raise(SHSM_EXC_GDB_TIMEOUT, "the session broker did not answer %s", method);
    if (st != SHSM_NET_OK) shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "lost the session broker: %s", err.text);
    if (b->timed_out)
        shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "lost the session broker: cannot read from timed out object");
    /* readline */
    AvpStr resp_line;
    u8 rb[4096];
    for (;;) {
        u8 *nl = b->in.n ? memchr(b->in.v, '\n', b->in.n) : NULL;
        if (nl) {
            size_t take = (size_t)(nl - b->in.v) + 1;
            resp_line = avp_str_dup(a, avp_strn((const char *)b->in.v, take));
            memmove(b->in.v, b->in.v + take, b->in.n - take);
            b->in.n -= take;
            break;
        }
        size_t got = 0;
        st = shsm_net_recv(b->sock, rb, sizeof rb, b->timeout, &got, &err);
        if (st == SHSM_NET_OK) {
            avp_vec_extend(&b->in, rb, got);
            continue;
        }
        if (st == SHSM_NET_TIMEOUT) {
            b->timed_out = true;
            shsm_raise(SHSM_EXC_GDB_TIMEOUT, "the session broker did not answer %s", method);
        }
        if (st == SHSM_NET_ERROR) shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "lost the session broker: %s", err.text);
        resp_line = avp_str_dup(a, avp_strn((const char *)b->in.v, b->in.n)); /* EOF: what is left */
        b->in.n = 0;
        break;
    }
    if (resp_line.n == 0) shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "the session broker closed the connection");
    AvpJsonError jerr = {0};
    AvpJson *resp = avp_json_parse_ex(a, resp_line, AVP_JSON_BYTES, &jerr);
    if (!resp) shsm_raise(SHSM_EXC_VALUE, "%s", avp_cz(a, avp_json_error_str(a, &jerr)));
    if (!avp_json_truthy(avp_json_getz(resp, "ok"))) {
        AvpStr kind = avp_json_get_str(resp, "kind", AVP_S(""));
        int cls = avp_str_eq(kind, AVP_S("timeout"))        ? SHSM_EXC_GDB_TIMEOUT
                  : avp_str_eq(kind, AVP_S("disconnected")) ? SHSM_EXC_GDB_DISCONNECTED
                                                            : SHSM_EXC_GDB;
        const AvpJson *e = avp_json_getz(resp, "error");
        shsm_raise(cls, "%s", avp_json_has(resp, AVP_S("error")) ? py_str_cz(a, e) : "broker error");
    }
    AvpJson *r = avp_json_getz(resp, "result");
    return r ? r : avp_json_null();
}

void shsm_broker_client_shutdown(AvpArena *a, ShsmBrokerClient *b) { shsm_broker_call(a, b, "shutdown", NULL, 0); }

/* ---- ShsmGdbTarget ---- */

ShsmGdbTarget shsm_gdb_target_direct(ShsmGdbClient *c) { return (ShsmGdbTarget){SHSM_GDB_TARGET_DIRECT, c, NULL}; }
ShsmGdbTarget shsm_gdb_target_broker(ShsmBrokerClient *b) { return (ShsmGdbTarget){SHSM_GDB_TARGET_BROKER, NULL, b}; }

#define RPC_TIMEOUT 60.0 /* BrokerClient.__getattr__: 60.0 + float(wait or 0) */

static AvpJson *bcall(AvpArena *a, ShsmGdbTarget *t, const char *method, AvpJson *args) {
    double wait = 0;
    if (!strcmp(method, "wait_stop") && args && avp_json_len(args))
        wait = avp_json_f64(avp_json_at(args, 0), 0);
    return shsm_broker_call(a, t->broker, method, args, RPC_TIMEOUT + wait);
}

static AvpJson *args_of(AvpArena *a, int n, const AvpJson *const *v) {
    AvpJson *arr = avp_json_arr(a);
    for (int i = 0; i < n; i++) avp_json_arr_push(a, arr, (AvpJson *)v[i]);
    return arr;
}

static AvpJson *timeout_args(AvpArena *a, double timeout) {
    AvpJson *arr = avp_json_arr(a);
    if (timeout != SHSM_GDB_NONE) avp_json_arr_push(a, arr, avp_json_float(a, timeout));
    return arr;
}

ShsmGdbState shsm_target_state(AvpArena *a, ShsmGdbTarget *t) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_state(a, t->client);
    return shsm_gdb_state_from_json(a, bcall(a, t, "state", NULL));
}

ShsmGdbStop *shsm_target_halt_reason(AvpArena *a, ShsmGdbTarget *t) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_halt_reason(a, t->client);
    return shsm_gdb_stop_from_json(a, bcall(a, t, "halt_reason", NULL));
}

size_t shsm_target_read_gprs(ShsmGdbTarget *t, u64 out[32]) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_read_gprs(t->client, out);
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpJson *r = bcall(a, t, "read_gprs", NULL);
    size_t n = avp_json_is_arr(r) ? avp_json_len(r) : 0;
    if (n > 32) n = 32;
    for (size_t i = 0; i < n; i++) out[i] = json_u64(avp_json_at(r, i));
    avp_arena_reset(a, m);
    return n;
}

u64 shsm_target_read_register(ShsmGdbTarget *t, i64 num) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_read_register(t->client, num);
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    const AvpJson *v[] = {avp_json_int(a, num)};
    u64 r = json_u64(bcall(a, t, "read_register", args_of(a, 1, v)));
    avp_arena_reset(a, m);
    return r;
}

void shsm_target_read_registers(ShsmGdbTarget *t, const i64 *nums, size_t n, u64 *out) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) {
        shsm_gdb_read_registers(t->client, nums, n, out);
        return;
    }
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpJson *list = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, list, avp_json_int(a, nums[i]));
    const AvpJson *v[] = {list};
    AvpJson *r = bcall(a, t, "read_registers", args_of(a, 1, v));
    for (size_t i = 0; i < n; i++) out[i] = json_u64(avp_json_at(r, i));
    avp_arena_reset(a, m);
}

ShsmBytes shsm_target_read_memory(AvpArena *a, ShsmGdbTarget *t, i64 addr, i64 length) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_read_memory(a, t->client, addr, length);
    const AvpJson *v[] = {avp_json_int(a, addr), avp_json_int(a, length)};
    AvpJson *r = bcall(a, t, "read_memory", args_of(a, 2, v));
    ShsmBytes out = {0};
    if (!shsm_gdb_json_bytes(a, r, &out)) malformed();
    return out;
}

void shsm_target_write_memory(ShsmGdbTarget *t, i64 addr, const void *data, size_t n) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) {
        shsm_gdb_write_memory(t->client, addr, data, n);
        return;
    }
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    const AvpJson *v[] = {avp_json_int(a, addr), shsm_gdb_json_from_bytes(a, data, n)};
    bcall(a, t, "write_memory", args_of(a, 2, v));
    avp_arena_reset(a, m);
}

static void target_bp(ShsmGdbTarget *t, const char *method, i64 addr, i64 kind, i64 length) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    const AvpJson *v[] = {avp_json_int(a, addr), avp_json_int(a, kind), avp_json_int(a, length)};
    bcall(a, t, method, args_of(a, 3, v));
    avp_arena_reset(a, m);
}

void shsm_target_insert_breakpoint(ShsmGdbTarget *t, i64 addr, i64 kind, i64 length) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) shsm_gdb_insert_breakpoint(t->client, addr, kind, length);
    else target_bp(t, "insert_breakpoint", addr, kind, length);
}

void shsm_target_remove_breakpoint(ShsmGdbTarget *t, i64 addr, i64 kind, i64 length) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) shsm_gdb_remove_breakpoint(t->client, addr, kind, length);
    else target_bp(t, "remove_breakpoint", addr, kind, length);
}

size_t shsm_target_breakpoints(AvpArena *a, ShsmGdbTarget *t, ShsmGdbBreakpoint **out) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_breakpoints(a, t->client, out);
    return breakpoints_from_json(a, bcall(a, t, "breakpoints", NULL), out);
}

void shsm_target_clear_breakpoints(ShsmGdbTarget *t) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) {
        shsm_gdb_clear_breakpoints(t->client);
        return;
    }
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    bcall(a, t, "clear_breakpoints", NULL);
    avp_arena_reset(a, m);
}

bool shsm_target_resume(ShsmGdbTarget *t, double timeout) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_resume(t->client, timeout);
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    bool r = avp_json_truthy(bcall(a, t, "resume", timeout_args(a, timeout)));
    avp_arena_reset(a, m);
    return r;
}

ShsmGdbStop *shsm_target_wait_stop(AvpArena *a, ShsmGdbTarget *t, double timeout) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_wait_stop(a, t->client, timeout);
    const AvpJson *v[] = {avp_json_float(a, timeout)};
    return shsm_gdb_stop_from_json(a, bcall(a, t, "wait_stop", args_of(a, 1, v)));
}

ShsmGdbStop *shsm_target_step(AvpArena *a, ShsmGdbTarget *t, i64 count, double timeout) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_step(a, t->client, count, timeout);
    AvpJson *args = avp_json_arr(a);
    avp_json_arr_push(a, args, avp_json_int(a, count));
    if (timeout != SHSM_GDB_NONE) avp_json_arr_push(a, args, avp_json_float(a, timeout));
    return shsm_gdb_stop_from_json(a, bcall(a, t, "step", args));
}

ShsmGdbStop *shsm_target_interrupt(AvpArena *a, ShsmGdbTarget *t, double timeout) {
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_interrupt(a, t->client, timeout);
    return shsm_gdb_stop_from_json(a, bcall(a, t, "interrupt", timeout_args(a, timeout)));
}

AvpJson *shsm_target_call(AvpArena *a, ShsmGdbTarget *t, const char *method, const AvpJson *args) {
    if (!shsm_gdb_is_rpc_method(avp_str(method)))
        shsm_raise(SHSM_EXC_GDB, "unknown method %s", avp_cz(a, avp_repr_str(a, avp_str(method))));
    if (t->kind == SHSM_GDB_TARGET_DIRECT) return shsm_gdb_dispatch(a, t->client, avp_str(method), args);
    return bcall(a, t, method, (AvpJson *)args);
}
