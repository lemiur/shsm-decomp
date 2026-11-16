/* The GDB remote serial protocol client of tools/dolphin.py: framing, GdbClient, the RPC dispatcher (shsm_gdb.md).
 * The broker, the broker client and ShsmGdbTarget are in shsm_gdb_broker.c. */
#include "shsm_gdb.h"
#include "shsm_gdb_priv.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "shsm_cli.h"

#include <math.h>
#include <stdio.h>

/* ---- exceptions ---- */

bool shsm_exc_is_gdb(int exc) {
    return exc == SHSM_EXC_GDB || exc == SHSM_EXC_GDB_TIMEOUT || exc == SHSM_EXC_GDB_DISCONNECTED;
}

const char *shsm_gdb_exc_class(int exc) {
    switch (exc) {
    case SHSM_EXC_GDB: return "GdbError";
    case SHSM_EXC_GDB_TIMEOUT: return "GdbTimeout";
    case SHSM_EXC_GDB_DISCONNECTED: return "GdbDisconnected";
    default: return shsm_exc_class(exc);
    }
}

/* ---- text helpers (shared with shsm_gdb_broker.c) ---- */

AvpStr gdbp_latin1_to_utf8(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_reserve(&b, s.n + 1);
    for (size_t i = 0; i < s.n; i++) avp_buf_putcp(&b, (u8)s.p[i]);
    if (!b.p) return avp_str_dup(a, AVP_S(""));
    return avp_buf_str(&b);
}

AvpStr gdbp_utf8_to_latin1(AvpArena *a, AvpStr s) {
    char *out = avp_alloc(a, s.n + 1);
    size_t n = 0;
    for (size_t i = 0; i < s.n;) {
        u8 c = (u8)s.p[i];
        u32 cp;
        size_t k;
        if (c < 0x80) {
            cp = c;
            k = 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.n) {
            cp = ((u32)(c & 0x1F) << 6) | ((u8)s.p[i + 1] & 0x3F);
            k = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = 0x100;
            k = 3;
        } else {
            cp = 0x100;
            k = 4;
        }
        out[n++] = cp < 0x100 ? (char)cp : '?';
        i += k;
    }
    return avp_strn(out, n);
}

const char *gdbp_cz_latin1(AvpArena *a, AvpStr s) { return avp_cz(a, gdbp_latin1_to_utf8(a, s)); }

static int hexval(u8 c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* str.isspace for a Latin-1 character (what int() strips) */
static bool l1_space(u8 c) { return (c >= 9 && c <= 13) || (c >= 0x1C && c <= 0x20) || c == 0x85 || c == 0xA0; }

bool gdbp_int16(AvpStr s, u64 *out) {
    size_t i = 0, n = s.n;
    while (i < n && l1_space((u8)s.p[i])) i++;
    while (n > i && l1_space((u8)s.p[n - 1])) n--;
    if (i < n && s.p[i] == '+') i++;
    else if (i < n && s.p[i] == '-') return false; /* a negative value: not modelled (D-shsm_gdb-1) */
    bool prefix = false;
    if (i + 1 < n && s.p[i] == '0' && (s.p[i + 1] == 'x' || s.p[i + 1] == 'X')) {
        i += 2;
        prefix = true;
    }
    u64 v = 0;
    size_t digits = 0;
    bool last_us = prefix; /* "0x_f" is allowed: an underscore may follow the prefix */
    for (; i < n; i++) {
        u8 c = (u8)s.p[i];
        if (c == '_') {
            if (last_us && !(prefix && digits == 0 && i >= 2 && (s.p[i - 1] == 'x' || s.p[i - 1] == 'X')))
                return false;
            if (!prefix && digits == 0) return false;
            last_us = true;
            continue;
        }
        int h = hexval(c);
        if (h < 0) return false;
        if (v >> 60) return false; /* beyond 64 bits (D-shsm_gdb-1) */
        v = v * 16 + (u64)h;
        digits++;
        last_us = false;
    }
    if (digits == 0 || last_us) return false;
    *out = v;
    return true;
}

u64 gdbp_int16_or_raise(AvpArena *a, AvpStr s) {
    u64 v;
    if (!gdbp_int16(s, &v)) {
        AvpStr u = gdbp_latin1_to_utf8(a, s);
        shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 16: %s", avp_cz(a, avp_repr_str(a, u)));
    }
    return v;
}

static bool ascii_space(u8 c) { return c == ' ' || (c >= 9 && c <= 13); }

ShsmBytes gdbp_fromhex(AvpArena *a, AvpStr s) {
    u8 *out = avp_alloc(a, s.n / 2 + 1);
    size_t n = 0, i = 0;
    while (i < s.n) {
        if (ascii_space((u8)s.p[i])) {
            while (i < s.n && ascii_space((u8)s.p[i])) i++;
            if (i >= s.n) break;
        }
        int top = hexval((u8)s.p[i]);
        if (top < 0) shsm_raise(SHSM_EXC_VALUE, "non-hexadecimal number found in fromhex() arg at position %zu", i);
        i++;
        if (i >= s.n) shsm_raise(SHSM_EXC_VALUE, "fromhex() arg must contain an even number of hexadecimal digits");
        int bot = hexval((u8)s.p[i]);
        if (bot < 0) shsm_raise(SHSM_EXC_VALUE, "non-hexadecimal number found in fromhex() arg at position %zu", i);
        i++;
        out[n++] = (u8)(top * 16 + bot);
    }
    return (ShsmBytes){out, n};
}

AvpStr gdbp_hex(AvpArena *a, const void *p, size_t n) {
    static const char digits[] = "0123456789abcdef";
    char *out = avp_alloc(a, n * 2 + 1);
    const u8 *q = (const u8 *)p;
    for (size_t i = 0; i < n; i++) {
        out[2 * i] = digits[q[i] >> 4];
        out[2 * i + 1] = digits[q[i] & 15];
    }
    return avp_strn(out, n * 2);
}

/* ---- protocol helpers ---- */

u8 shsm_gdb_checksum(const void *p, size_t n) {
    const u8 *q = (const u8 *)p;
    unsigned s = 0;
    for (size_t i = 0; i < n; i++) s += q[i];
    return (u8)(s & 0xFF);
}

AvpStr shsm_gdb_frame(AvpArena *a, AvpStr payload) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, '$');
    size_t start = b.n;
    for (size_t i = 0; i < payload.n; i++) {
        char c = payload.p[i];
        if (c == '$' || c == '#' || c == '}' || c == '*') {
            avp_buf_putc(&b, 0x7D);
            avp_buf_putc(&b, (char)(c ^ 0x20));
        } else {
            avp_buf_putc(&b, c);
        }
    }
    u8 cs = shsm_gdb_checksum(b.p + start, b.n - start);
    avp_buf_printf(&b, "#%02x", cs);
    return avp_buf_str(&b);
}

AvpStr shsm_gdb_unescape(AvpArena *a, AvpStr raw) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t i = 0;
    while (i < raw.n) {
        u8 c = (u8)raw.p[i];
        if (c == 0x7D && i + 1 < raw.n) {
            avp_buf_putc(&b, (char)((u8)raw.p[i + 1] ^ 0x20));
            i += 2;
        } else if (c == 0x2A && b.n > 0 && i + 1 < raw.n) {
            int count = (int)(u8)raw.p[i + 1] - 29;
            char last = b.p[b.n - 1];
            for (int k = 0; k < count; k++) avp_buf_putc(&b, last);
            i += 2;
        } else {
            avp_buf_putc(&b, (char)c);
            i++;
        }
    }
    if (!b.p) return avp_str_dup(a, AVP_S(""));
    return avp_buf_str(&b);
}

static bool all_hex(const char *p, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (hexval((u8)p[i]) < 0) return false;
    return true;
}

bool shsm_gdb_is_stop_reply(AvpStr p) {
    if (p.n == 0) return false;
    char c = p.p[0];
    if (c == 'T' || c == 'S' || c == 'W' || c == 'X') return p.n >= 3 && all_hex(p.p + 1, 2);
    return c == 'O' && !avp_str_eq(p, AVP_S("OK")) && p.n > 1 && all_hex(p.p + 1, p.n - 1);
}

bool shsm_gdb_is_hex(AvpStr p) { return p.n > 0 && all_hex(p.p, p.n); }

AvpStr shsm_gdb_check_error(AvpStr cmd, AvpStr p) {
    if (p.n == 3 && p.p[0] == 'E' && shsm_gdb_is_hex(avp_strn(p.p + 1, 2))) {
        AvpArena *a = avp_scratch();
        size_t k = 0;
        while (k < cmd.n && cmd.p[k] != ',') k++;
        if (k > 16) k = 16;
        shsm_raise(SHSM_EXC_GDB, "stub refused %s: %s", gdbp_cz_latin1(a, avp_strn(cmd.p, k)), gdbp_cz_latin1(a, p));
    }
    return p;
}

ShsmGdbStop *shsm_gdb_parse_stop(AvpArena *a, AvpStr p) {
    if (p.n == 0) shsm_raise(SHSM_EXC_VALUE, "string index out of range");
    ShsmGdbStop *st = AVP_NEW(a, ShsmGdbStop);
    st->kind = p.p[0];
    st->signal = (int)gdbp_int16_or_raise(a, avp_strn(p.p + 1, p.n >= 3 ? 2 : p.n - 1));
    st->raw = avp_str_dup(a, p);
    if (st->kind != 'T') return st;
    size_t cap = 0;
    AvpStr rest = p.n > 3 ? avp_strn(p.p + 3, p.n - 3) : AVP_S("");
    size_t i = 0;
    for (;;) {
        size_t j = i;
        while (j < rest.n && rest.p[j] != ';') j++;
        AvpStr part = avp_strn(rest.p + i, j - i);
        const char *colon = memchr(part.p, ':', part.n);
        if (colon) {
            AvpStr k = avp_strn(part.p, (size_t)(colon - part.p));
            AvpStr v = avp_strn(colon + 1, part.n - k.n - 1);
            u64 n, val;
            if (gdbp_int16(k, &n) && gdbp_int16(v, &val)) {
                size_t r = 0;
                while (r < st->nregs && st->regs[r].num != n) r++;
                if (r == st->nregs) {
                    if (st->nregs == cap) {
                        size_t ncap = cap ? cap * 2 : 4;
                        st->regs = avp_arena_realloc(a, st->regs, cap * sizeof *st->regs, ncap * sizeof *st->regs);
                        cap = ncap;
                    }
                    st->nregs++;
                }
                st->regs[r] = (ShsmGdbReg){n, val};
                if (n == SHSM_GDB_REG_PC) {
                    st->has_pc = true;
                    st->pc = val;
                } else if (n == 1) {
                    st->has_sp = true;
                    st->sp = val;
                }
            }
        }
        if (j >= rest.n) break;
        i = j + 1;
    }
    return st;
}

ShsmGdbStop *shsm_gdb_stop_copy(AvpArena *a, const ShsmGdbStop *st) {
    if (!st) return NULL;
    ShsmGdbStop *c = AVP_NEW(a, ShsmGdbStop);
    *c = *st;
    c->regs = st->nregs ? avp_memdup(a, st->regs, st->nregs * sizeof *st->regs) : NULL;
    c->raw = avp_str_dup(a, st->raw);
    return c;
}

/* ---- the client ---- */

typedef AVP_VEC(AvpStr) PacketVec; /* heap strings */
typedef AVP_VEC(ShsmGdbBreakpoint) BpVec;

struct ShsmGdbClient {
    ShsmSocket sock;
    double timeout;
    int retries;
    double sock_timeout; /* the Python socket's current timeout: < 0 blocking */
    AvpBytes buf;
    size_t boff;
    PacketVec packets;
    AvpBytes acks;
    i64 bad_checksums;
    bool running, exited, closed;
    ShsmGdbStop *last_stop; /* in stop_arena */
    AvpArena *stop_arena;
    BpVec bps;
    u8 *rbuf;
};

#define RECV_SIZE 65536

ShsmGdbClient *shsm_gdb_client_new(ShsmSocket sock, double timeout, int retries) {
    ShsmGdbClient *c = avp_xcalloc(1, sizeof *c);
    c->sock = sock;
    c->timeout = timeout;
    c->retries = retries;
    c->sock_timeout = SHSM_GDB_CONNECT_TIMEOUT;
    c->rbuf = avp_xmalloc(RECV_SIZE);
    return c;
}

ShsmGdbClient *shsm_gdb_connect(const char *host, int port, double timeout, double wait) {
    double end = avp_monotonic() + (wait > 0 ? wait : 0.0);
    for (;;) {
        ShsmSocket s;
        ShsmNetError err;
        if (shsm_net_connect(host, port, SHSM_GDB_CONNECT_TIMEOUT, &s, &err) == SHSM_NET_OK) {
            shsm_net_set_nodelay(s);
            return shsm_gdb_client_new(s, timeout, SHSM_GDB_RETRIES);
        }
        if (avp_monotonic() >= end)
            shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "cannot connect to the GDB stub at %s:%d: %s", host, port, err.text);
        avp_sleep(0.25);
    }
}

void shsm_gdb_close(ShsmGdbClient *c) {
    c->closed = true;
    shsm_net_close(c->sock);
    c->sock = SHSM_SOCKET_INVALID;
}

void shsm_gdb_client_free(ShsmGdbClient *c) {
    if (!c) return;
    shsm_gdb_close(c);
    for (size_t i = 0; i < c->packets.n; i++) avp_xfree((void *)c->packets.v[i].p);
    avp_vec_free(&c->packets);
    avp_vec_free(&c->buf);
    avp_vec_free(&c->acks);
    avp_vec_free(&c->bps);
    if (c->stop_arena) avp_arena_free(c->stop_arena);
    avp_xfree(c->rbuf);
    avp_xfree(c);
}

bool shsm_gdb_running(const ShsmGdbClient *c) { return c->running; }
bool shsm_gdb_exited(const ShsmGdbClient *c) { return c->exited; }
bool shsm_gdb_closed(const ShsmGdbClient *c) { return c->closed; }
i64 shsm_gdb_bad_checksums(const ShsmGdbClient *c) { return c->bad_checksums; }
const ShsmGdbStop *shsm_gdb_last_stop(const ShsmGdbClient *c) { return c->last_stop; }
double shsm_gdb_timeout(const ShsmGdbClient *c) { return c->timeout; }

static double or_default(const ShsmGdbClient *c, double timeout) {
    return (timeout == 0 || isnan(timeout)) ? c->timeout : timeout;
}

static void set_last_stop(ShsmGdbClient *c, const ShsmGdbStop *st) {
    AvpArena *old = c->stop_arena;
    c->stop_arena = avp_arena_new(4096);
    c->last_stop = shsm_gdb_stop_copy(c->stop_arena, st);
    if (old) avp_arena_free(old);
}

static void raw_send(ShsmGdbClient *c, const void *p, size_t n) {
    ShsmNetError err;
    ShsmNetStatus st = shsm_net_send_all(c->sock, p, n, c->sock_timeout, &err);
    if (st != SHSM_NET_OK) {
        c->closed = true;
        shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "connection to the GDB stub lost: %s", err.text);
    }
}

static void buf_append(ShsmGdbClient *c, const u8 *p, size_t n) {
    if (c->boff) {
        memmove(c->buf.v, c->buf.v + c->boff, c->buf.n - c->boff);
        c->buf.n -= c->boff;
        c->boff = 0;
    }
    avp_vec_extend(&c->buf, p, n);
}

static void parse(AvpArena *a, ShsmGdbClient *c) {
    while (c->boff < c->buf.n) {
        const u8 *b = c->buf.v + c->boff;
        size_t left = c->buf.n - c->boff;
        if (b[0] == '+' || b[0] == '-') {
            avp_vec_push(&c->acks, b[0]);
            c->boff++;
            continue;
        }
        if (b[0] != '$') {
            c->boff++;
            continue;
        }
        const u8 *h = memchr(b, '#', left);
        if (!h || left < (size_t)(h - b) + 3) return;
        size_t hn = (size_t)(h - b);
        AvpStr raw = avp_strn((const char *)b + 1, hn - 1);
        AvpStr cs = avp_strn((const char *)h + 1, 2);
        u64 want;
        bool ok = gdbp_int16(cs, &want) && want == shsm_gdb_checksum(raw.p, raw.n);
        AvpStr payload = ok ? shsm_gdb_unescape(a, raw) : AVP_S("");
        c->boff += hn + 3;
        if (!ok) {
            c->bad_checksums++;
            raw_send(c, "-", 1);
            continue;
        }
        raw_send(c, "+", 1);
        char *heap = avp_xmalloc(payload.n + 1);
        memcpy(heap, payload.p, payload.n);
        heap[payload.n] = 0;
        avp_vec_push(&c->packets, avp_strn(heap, payload.n));
    }
}

static void fill(AvpArena *a, ShsmGdbClient *c, double deadline) {
    double left = deadline - avp_monotonic();
    if (left <= 0) shsm_raise(SHSM_EXC_GDB_TIMEOUT, "timed out waiting for the GDB stub");
    c->sock_timeout = left < 0.25 ? left : 0.25;
    size_t got = 0;
    ShsmNetError err;
    ShsmNetStatus st = shsm_net_recv(c->sock, c->rbuf, RECV_SIZE, c->sock_timeout, &got, &err);
    if (st == SHSM_NET_TIMEOUT) return;
    if (st == SHSM_NET_ERROR) {
        c->closed = true;
        shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "connection to the GDB stub lost: %s", err.text);
    }
    if (st == SHSM_NET_CLOSED) {
        c->closed = true;
        shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "the GDB stub closed the connection");
    }
    buf_append(c, c->rbuf, got);
    parse(a, c);
}

/* pops packets[0] into the arena */
static AvpStr pop_packet(AvpArena *a, ShsmGdbClient *c) {
    AvpStr h = c->packets.v[0];
    avp_vec_remove(&c->packets, 0);
    AvpStr p = avp_str_dup(a, h);
    avp_xfree((void *)h.p);
    return p;
}

static void note_stop(AvpArena *a, ShsmGdbClient *c, AvpStr p) {
    if (p.p[0] == 'O') return;
    ShsmGdbStop *st = shsm_gdb_parse_stop(a, p);
    set_last_stop(c, st);
    c->running = false;
    if (st->kind == 'W' || st->kind == 'X') c->exited = true;
}

/* note every queued stop reply and drop the queue (keep_others: keep the other packets, as poll does) */
static void process_packets(AvpArena *a, ShsmGdbClient *c, bool keep_others) {
    PacketVec rest = {0};
    while (c->packets.n) {
        AvpStr h = c->packets.v[0];
        if (shsm_gdb_is_stop_reply(h) || !keep_others) {
            AvpStr p = pop_packet(a, c);
            if (shsm_gdb_is_stop_reply(p)) note_stop(a, c, p);
        } else {
            avp_vec_remove(&c->packets, 0);
            avp_vec_push(&rest, h);
        }
    }
    avp_vec_free(&c->packets);
    c->packets = rest;
}

void shsm_gdb_poll(ShsmGdbClient *c) {
    if (c->closed) return;
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    for (;;) {
        size_t got = 0;
        ShsmNetError err;
        ShsmNetStatus st = shsm_net_recv(c->sock, c->rbuf, RECV_SIZE, 0, &got, &err);
        if (st == SHSM_NET_TIMEOUT) break;
        if (st == SHSM_NET_ERROR) {
            c->closed = true;
            shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "connection to the GDB stub lost: %s", err.text);
        }
        if (st == SHSM_NET_CLOSED) {
            c->closed = true;
            shsm_raise(SHSM_EXC_GDB_DISCONNECTED, "the GDB stub closed the connection");
        }
        buf_append(c, c->rbuf, got);
    }
    c->sock_timeout = -1;
    parse(a, c);
    process_packets(a, c, true);
    avp_arena_reset(a, m);
}

void shsm_gdb_send_packet(ShsmGdbClient *c, AvpStr payload, double timeout) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    double deadline = avp_monotonic() + or_default(c, timeout);
    AvpStr fr = shsm_gdb_frame(a, payload);
    for (int i = 0; i < c->retries + 1; i++) {
        avp_vec_clear(&c->acks);
        raw_send(c, fr.p, fr.n);
        while (!c->acks.n) fill(a, c, deadline);
        if (c->acks.v[0] == '+') {
            avp_vec_remove(&c->acks, 0);
            avp_arena_reset(a, m);
            return;
        }
    }
    AvpStr head = gdbp_latin1_to_utf8(a, avp_strn(payload.p, payload.n < 16 ? payload.n : 16));
    shsm_raise(SHSM_EXC_GDB, "the stub rejected %s %d times", avp_cz(a, avp_repr_str(a, head)), c->retries + 1);
}

static AvpStr next_packet(AvpArena *a, ShsmGdbClient *c, double deadline) {
    while (!c->packets.n) fill(a, c, deadline);
    return pop_packet(a, c);
}

AvpStr shsm_gdb_request(AvpArena *a, ShsmGdbClient *c, AvpStr payload, ShsmGdbAccept accept, double timeout) {
    double deadline = avp_monotonic() + or_default(c, timeout);
    shsm_gdb_poll(c);
    shsm_gdb_send_packet(c, payload, timeout);
    for (;;) {
        AvpStr p = next_packet(a, c, deadline);
        if (accept(p)) return p;
        if (shsm_gdb_is_stop_reply(p)) note_stop(a, c, p);
    }
}

void shsm_gdb_drain(ShsmGdbClient *c, double quiet, double limit) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    double end = avp_monotonic() + limit;
    double last = avp_monotonic();
    while (avp_monotonic() < end && avp_monotonic() - last < quiet) {
        size_t n = c->packets.n;
        double d = avp_monotonic() + quiet;
        AvpTry t;
        if (AVP_TRY(&t)) {
            fill(a, c, end < d ? end : d);
            avp_try_end(&t);
        } else {
            if (t.code == SHSM_EXC_GDB_TIMEOUT) break;
            avp_throw(t.code, "%s", t.msg);
        }
        if (c->packets.n != n || c->acks.n) {
            last = avp_monotonic();
            avp_vec_clear(&c->acks);
        }
        process_packets(a, c, false);
    }
    avp_arena_reset(a, m);
}

/* ---- commands ---- */

static bool accept_hex_or_e(AvpStr p) { return shsm_gdb_is_hex(p) || (p.n && p.p[0] == 'E'); }
static bool accept_reg(AvpStr p) { return shsm_gdb_is_hex(p) || (p.n && p.p[0] == 'E') || p.n == 0; }
static bool accept_ok(AvpStr p) { return avp_str_eq(p, AVP_S("OK")) || (p.n && p.p[0] == 'E') || p.n == 0; }
static bool accept_stop(AvpStr p) { return shsm_gdb_is_stop_reply(p); }

ShsmGdbStop *shsm_gdb_halt_reason(AvpArena *a, ShsmGdbClient *c) {
    AvpStr p = shsm_gdb_request(a, c, AVP_S("?"), accept_stop, SHSM_GDB_NONE);
    ShsmGdbStop *st = shsm_gdb_parse_stop(a, p);
    set_last_stop(c, st);
    return st;
}

size_t shsm_gdb_read_gprs(ShsmGdbClient *c, u64 out[32]) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpStr p = shsm_gdb_check_error(AVP_S("g"), shsm_gdb_request(a, c, AVP_S("g"), accept_hex_or_e, SHSM_GDB_NONE));
    size_t n = p.n / 8;
    if (n > 32) n = 32;
    for (size_t i = 0; i < n; i++) out[i] = gdbp_int16_or_raise(a, avp_strn(p.p + i * 8, 8));
    avp_arena_reset(a, m);
    return n;
}

u64 shsm_gdb_read_register(ShsmGdbClient *c, i64 num) {
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpStr cmd = avp_pyfmt(a, "p%x", num);
    AvpStr p = shsm_gdb_check_error(cmd, shsm_gdb_request(a, c, cmd, accept_reg, SHSM_GDB_NONE));
    if (p.n == 0) shsm_raise(SHSM_EXC_GDB, "the stub does not report register %s", avp_pyfmt(a, "0x%x", num).p);
    u64 v = gdbp_int16_or_raise(a, p);
    avp_arena_reset(a, m);
    return v;
}

void shsm_gdb_read_registers(ShsmGdbClient *c, const i64 *nums, size_t n, u64 *out) {
    for (size_t i = 0; i < n; i++) out[i] = shsm_gdb_read_register(c, nums[i]);
}

ShsmBytes shsm_gdb_read_memory(AvpArena *a, ShsmGdbClient *c, i64 addr, i64 length) {
    u8 *out = length > 0 ? avp_alloc(a, (size_t)length) : avp_alloc(a, 0);
    i64 have = 0;
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    while (have < length) {
        i64 n = length - have < SHSM_GDB_MEM_CHUNK ? length - have : SHSM_GDB_MEM_CHUNK;
        AvpStr cmd = avp_pyfmt(sc, "m%x,%x", addr + have, n);
        AvpStr p = shsm_gdb_check_error(cmd, shsm_gdb_request(sc, c, cmd, accept_hex_or_e, SHSM_GDB_NONE));
        ShsmBytes chunk = gdbp_fromhex(sc, p);
        if ((i64)chunk.n != n)
            shsm_raise(SHSM_EXC_GDB, "%s",
                       avp_pyfmt(sc, "short read at 0x%08X: %d of %d bytes", addr + have, (i64)chunk.n, n).p);
        memcpy(out + have, chunk.p, chunk.n);
        have += n;
        avp_arena_reset(sc, m);
    }
    return (ShsmBytes){out, (size_t)(length > 0 ? length : 0)};
}

void shsm_gdb_write_memory(ShsmGdbClient *c, i64 addr, const void *data, size_t n) {
    const u8 *d = (const u8 *)data;
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    for (size_t i = 0; i < n; i += SHSM_GDB_MEM_CHUNK) {
        size_t k = n - i < SHSM_GDB_MEM_CHUNK ? n - i : SHSM_GDB_MEM_CHUNK;
        AvpStr hex = gdbp_hex(sc, d + i, k);
        AvpStr cmd = avp_pyfmt(sc, "M%x,%x:%S", addr + (i64)i, (i64)k, &hex);
        AvpStr p = shsm_gdb_check_error(cmd, shsm_gdb_request(sc, c, cmd, accept_ok, SHSM_GDB_NONE));
        if (!avp_str_eq(p, AVP_S("OK")))
            shsm_raise(SHSM_EXC_GDB, "%s", avp_pyfmt(sc, "the stub did not confirm the write at 0x%08X", addr + (i64)i).p);
        avp_arena_reset(sc, m);
    }
}

static size_t bp_find(const ShsmGdbClient *c, i64 kind, i64 addr, i64 length) {
    for (size_t i = 0; i < c->bps.n; i++)
        if (c->bps.v[i].type == kind && c->bps.v[i].address == addr && c->bps.v[i].length == length) return i;
    return (size_t)-1;
}

void shsm_gdb_insert_breakpoint(ShsmGdbClient *c, i64 addr, i64 kind, i64 length) {
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    AvpStr cmd = avp_pyfmt(sc, "Z%d,%x,%x", kind, addr, length);
    AvpStr p = shsm_gdb_check_error(cmd, shsm_gdb_request(sc, c, cmd, accept_ok, SHSM_GDB_NONE));
    if (!avp_str_eq(p, AVP_S("OK")))
        shsm_raise(SHSM_EXC_GDB, "%s", avp_pyfmt(sc, "the stub does not support breakpoint type Z%d", kind).p);
    if (bp_find(c, kind, addr, length) == (size_t)-1) avp_vec_push(&c->bps, ((ShsmGdbBreakpoint){kind, addr, length}));
    avp_arena_reset(sc, m);
}

void shsm_gdb_remove_breakpoint(ShsmGdbClient *c, i64 addr, i64 kind, i64 length) {
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    AvpStr cmd = avp_pyfmt(sc, "z%d,%x,%x", kind, addr, length);
    shsm_gdb_check_error(cmd, shsm_gdb_request(sc, c, cmd, accept_ok, SHSM_GDB_NONE));
    size_t i = bp_find(c, kind, addr, length);
    if (i != (size_t)-1) avp_vec_remove(&c->bps, i);
    avp_arena_reset(sc, m);
}

size_t shsm_gdb_breakpoints(AvpArena *a, const ShsmGdbClient *c, ShsmGdbBreakpoint **out) {
    size_t n = c->bps.n;
    ShsmGdbBreakpoint *v = AVP_NEWN(a, ShsmGdbBreakpoint, n ? n : 1);
    if (n) memcpy(v, c->bps.v, n * sizeof *v);
    /* sorted(key=(address, type)): stable insertion sort */
    for (size_t i = 1; i < n; i++) {
        ShsmGdbBreakpoint x = v[i];
        size_t j = i;
        while (j > 0 && (v[j - 1].address > x.address || (v[j - 1].address == x.address && v[j - 1].type > x.type))) {
            v[j] = v[j - 1];
            j--;
        }
        v[j] = x;
    }
    *out = v;
    return n;
}

void shsm_gdb_clear_breakpoints(ShsmGdbClient *c) {
    size_t n = c->bps.n;
    if (!n) return;
    ShsmGdbBreakpoint *snap = avp_xmalloc(n * sizeof *snap);
    memcpy(snap, c->bps.v, n * sizeof *snap);
    AvpTry t;
    if (AVP_TRY(&t)) {
        for (size_t i = 0; i < n; i++) shsm_gdb_remove_breakpoint(c, snap[i].address, snap[i].type, snap[i].length);
        avp_try_end(&t);
    } else {
        avp_xfree(snap);
        avp_throw(t.code, "%s", t.msg);
    }
    avp_xfree(snap);
}

static u64 current_pc(ShsmGdbClient *c) {
    if (c->last_stop && c->last_stop->has_pc) return c->last_stop->pc;
    return shsm_gdb_read_register(c, SHSM_GDB_REG_PC);
}

/* _exec_bps_at(pc): the exec breakpoints (types 0 and 1) at pc, dict order, into a heap array */
static size_t exec_bps_at(ShsmGdbClient *c, u64 pc, ShsmGdbBreakpoint **out) {
    size_t n = 0;
    ShsmGdbBreakpoint *v = avp_xmalloc((c->bps.n + 1) * sizeof *v);
    for (size_t i = 0; i < c->bps.n; i++) {
        ShsmGdbBreakpoint b = c->bps.v[i];
        if ((b.type == 0 || b.type == 1) && (u64)b.address == pc) v[n++] = b;
    }
    *out = v;
    return n;
}

static ShsmGdbStop *raw_step(AvpArena *a, ShsmGdbClient *c, double timeout) {
    shsm_gdb_send_packet(c, AVP_S("s"), timeout);
    c->running = true;
    ShsmGdbStop *st = shsm_gdb_wait_stop(a, c, timeout);
    if (!st) shsm_raise(SHSM_EXC_GDB_TIMEOUT, "no stop reply after a single step");
    return st;
}

ShsmGdbStop *shsm_gdb_step(AvpArena *a, ShsmGdbClient *c, i64 count, double timeout) {
    if (c->running) shsm_raise(SHSM_EXC_GDB, "the CPU is running; pause it first");
    ShsmGdbStop *st = NULL;
    for (i64 i = 0; i < (count > 1 ? count : 1); i++) {
        ShsmGdbBreakpoint *here;
        size_t nh = exec_bps_at(c, current_pc(c), &here);
        AvpTry t;
        if (AVP_TRY(&t)) {
            for (size_t k = 0; k < nh; k++) shsm_gdb_remove_breakpoint(c, here[k].address, here[k].type, here[k].length);
            avp_try_end(&t);
        } else {
            avp_xfree(here);
            avp_throw(t.code, "%s", t.msg);
        }
        if (AVP_TRY(&t)) {
            st = raw_step(a, c, or_default(c, timeout));
            avp_try_end(&t);
        } else {
            /* finally: put the breakpoints back (an exception there replaces the first one, as in Python) */
            AvpTry t2;
            if (AVP_TRY(&t2)) {
                for (size_t k = 0; k < nh; k++)
                    shsm_gdb_insert_breakpoint(c, here[k].address, here[k].type, here[k].length);
                avp_try_end(&t2);
            } else {
                avp_xfree(here);
                avp_throw(t2.code, "%s", t2.msg);
            }
            avp_xfree(here);
            avp_throw(t.code, "%s", t.msg);
        }
        if (AVP_TRY(&t)) {
            for (size_t k = 0; k < nh; k++) shsm_gdb_insert_breakpoint(c, here[k].address, here[k].type, here[k].length);
            avp_try_end(&t);
        } else {
            avp_xfree(here);
            avp_throw(t.code, "%s", t.msg);
        }
        avp_xfree(here);
    }
    return st;
}

bool shsm_gdb_resume(ShsmGdbClient *c, double timeout) {
    if (c->running) return false;
    ShsmGdbBreakpoint *here;
    size_t nh = exec_bps_at(c, current_pc(c), &here);
    avp_xfree(here);
    if (nh) {
        AvpArena *sc = avp_scratch();
        AvpArenaMark m = avp_arena_mark(sc);
        shsm_gdb_step(sc, c, 1, timeout);
        avp_arena_reset(sc, m);
        nh = exec_bps_at(c, current_pc(c), &here);
        avp_xfree(here);
        if (nh) return true; /* stepped onto another breakpoint: report it as a hit */
    }
    shsm_gdb_send_packet(c, AVP_S("c"), timeout);
    c->running = true;
    return true;
}

ShsmGdbStop *shsm_gdb_wait_stop(AvpArena *a, ShsmGdbClient *c, double timeout) {
    if (!c->running) return shsm_gdb_stop_copy(a, c->last_stop);
    double deadline = avp_monotonic() + (timeout > 0 ? timeout : 0.0);
    while (c->running) {
        while (c->packets.n) {
            AvpStr p = pop_packet(a, c);
            if (shsm_gdb_is_stop_reply(p)) {
                note_stop(a, c, p);
                if (!c->running) return shsm_gdb_stop_copy(a, c->last_stop);
            }
        }
        AvpTry t;
        if (AVP_TRY(&t)) {
            fill(a, c, deadline);
            avp_try_end(&t);
        } else {
            if (t.code == SHSM_EXC_GDB_TIMEOUT) return NULL;
            avp_throw(t.code, "%s", t.msg);
        }
    }
    return shsm_gdb_stop_copy(a, c->last_stop);
}

ShsmGdbStop *shsm_gdb_interrupt(AvpArena *a, ShsmGdbClient *c, double timeout) {
    if (!c->running) return shsm_gdb_stop_copy(a, c->last_stop);
    raw_send(c, "\x03", 1);
    ShsmGdbStop *st = shsm_gdb_wait_stop(a, c, or_default(c, timeout));
    if (!st) shsm_raise(SHSM_EXC_GDB_TIMEOUT, "the CPU did not stop after an interrupt");
    shsm_gdb_drain(c, 0.4, 3.0);
    c->running = false;
    return shsm_gdb_stop_copy(a, c->last_stop);
}

ShsmGdbState shsm_gdb_state(AvpArena *a, const ShsmGdbClient *c) {
    ShsmGdbState s = {0};
    s.running = c->running;
    s.exited = c->exited;
    s.stop = shsm_gdb_stop_copy(a, c->last_stop);
    s.nbps = shsm_gdb_breakpoints(a, c, &s.bps);
    s.bad_checksums = c->bad_checksums;
    return s;
}

void shsm_gdb_detach(ShsmGdbClient *c) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_gdb_send_packet(c, AVP_S("D"), SHSM_GDB_NONE);
        avp_try_end(&t);
    } else {
        shsm_gdb_close(c);
        avp_throw(t.code, "%s", t.msg);
    }
    shsm_gdb_close(c);
}

void shsm_gdb_kill(ShsmGdbClient *c) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        raw_send(c, "$k#6b", 5);
        avp_try_end(&t);
    } else if (!shsm_exc_is_gdb(t.code)) {
        shsm_gdb_close(c);
        avp_throw(t.code, "%s", t.msg);
    }
    shsm_gdb_close(c);
}

/* ---- the RPC dispatcher ---- */

const char *const shsm_gdb_rpc_methods[] = {"state", "halt_reason", "read_gprs", "read_register", "read_registers",
                                            "read_memory", "write_memory", "insert_breakpoint", "remove_breakpoint",
                                            "breakpoints", "clear_breakpoints", "resume", "wait_stop", "step",
                                            "interrupt", NULL};

bool shsm_gdb_is_rpc_method(AvpStr name) {
    for (int i = 0; shsm_gdb_rpc_methods[i]; i++)
        if (avp_str_eq(name, avp_str(shsm_gdb_rpc_methods[i]))) return true;
    return false;
}

typedef struct {
    const char *name;
    const char *params[3];
    int nreq, nmax;
} MethodSig;

static const MethodSig g_sigs[] = {
    {"state", {0}, 0, 0},
    {"halt_reason", {0}, 0, 0},
    {"read_gprs", {0}, 0, 0},
    {"read_register", {"num"}, 1, 1},
    {"read_registers", {"nums"}, 1, 1},
    {"read_memory", {"addr", "length"}, 2, 2},
    {"write_memory", {"addr", "data"}, 2, 2},
    {"insert_breakpoint", {"addr", "kind", "length"}, 1, 3},
    {"remove_breakpoint", {"addr", "kind", "length"}, 1, 3},
    {"breakpoints", {0}, 0, 0},
    {"clear_breakpoints", {0}, 0, 0},
    {"resume", {"timeout"}, 0, 1},
    {"wait_stop", {"timeout"}, 1, 1},
    {"step", {"count", "timeout"}, 0, 2},
    {"interrupt", {"timeout"}, 0, 1},
};

typedef struct {
    const AvpJson *v; /* NULL: a decoded bytes argument */
    ShsmBytes bytes;
} RpcArg;

const char *gdbp_type_name(const AvpJson *v) {
    if (!v) return "NoneType";
    switch (v->kind) {
    case AVP_JSON_NULL: return "NoneType";
    case AVP_JSON_BOOL: return "bool";
    case AVP_JSON_INT: return "int";
    case AVP_JSON_FLOAT: return "float";
    case AVP_JSON_STRING: return "str";
    case AVP_JSON_ARRAY: return "list";
    default: return "dict";
    }
}

static const char *arg_type_name(const RpcArg *x) { return x->v ? gdbp_type_name(x->v) : "bytes"; }

static i64 arg_int(const RpcArg *x) {
    if (x->v && x->v->kind == AVP_JSON_INT && !x->v->big) return x->v->u.i;
    if (x->v && x->v->kind == AVP_JSON_BOOL) return x->v->b ? 1 : 0;
    shsm_raise(SHSM_EXC_TYPE, "%%x format: an integer is required, not %s", arg_type_name(x));
}

static i64 json_int(const AvpJson *v) {
    RpcArg x = {v, {0}};
    return arg_int(&x);
}

/* `timeout or self.timeout`: None/0 -> SHSM_GDB_NONE */
static double arg_timeout(const RpcArg *x) {
    if (!x || (x->v && x->v->kind == AVP_JSON_NULL)) return SHSM_GDB_NONE;
    if (x->v && avp_json_is_num(x->v) && !x->v->big) return avp_json_f64(x->v, 0);
    if (x->v && x->v->kind == AVP_JSON_BOOL) return x->v->b ? 1.0 : SHSM_GDB_NONE;
    shsm_raise(SHSM_EXC_TYPE, "unsupported operand type(s) for +: 'float' and '%s'", arg_type_name(x));
}

static ShsmBytes arg_bytes(AvpArena *a, const RpcArg *x) {
    if (!x->v) return x->bytes;
    if (x->v->kind == AVP_JSON_ARRAY) { /* bytes(list of ints) */
        size_t n = avp_json_len(x->v);
        u8 *p = avp_alloc(a, n + 1);
        for (size_t i = 0; i < n; i++) {
            const AvpJson *e = avp_json_at(x->v, i);
            if (!e || !(e->kind == AVP_JSON_INT || e->kind == AVP_JSON_BOOL))
                shsm_raise(SHSM_EXC_TYPE, "'%s' object cannot be interpreted as an integer", gdbp_type_name(e));
            i64 b = json_int(e);
            if (b < 0 || b > 255) shsm_raise(SHSM_EXC_VALUE, "bytes must be in range(0, 256)");
            p[i] = (u8)b;
        }
        return (ShsmBytes){p, n};
    }
    if (x->v->kind == AVP_JSON_STRING) shsm_raise(SHSM_EXC_TYPE, "string argument without an encoding");
    shsm_raise(SHSM_EXC_TYPE, "cannot convert '%s' object to bytes", arg_type_name(x));
}

static AvpJson *stop_or_null(AvpArena *a, const ShsmGdbStop *st) { return shsm_gdb_stop_to_json(a, st); }

AvpJson *shsm_gdb_dispatch(AvpArena *a, ShsmGdbClient *c, AvpStr method, const AvpJson *args) {
    const MethodSig *sig = NULL;
    for (size_t i = 0; i < AVP_ARRAY_LEN(g_sigs); i++)
        if (avp_str_eq(method, avp_str(g_sigs[i].name))) sig = &g_sigs[i];
    AVP_ASSERT(sig);
    size_t n = args ? avp_json_len(args) : 0;
    RpcArg *av = AVP_NEWN(a, RpcArg, n + 1);
    for (size_t i = 0; i < n; i++) { /* _dec */
        av[i].v = avp_json_at(args, i);
        if (shsm_gdb_json_bytes(a, av[i].v, &av[i].bytes)) av[i].v = NULL;
    }
    if ((int)n > sig->nmax) {
        if (sig->nreq == sig->nmax)
            shsm_raise(SHSM_EXC_TYPE, "GdbClient.%s() takes %d positional argument%s but %d were given", sig->name,
                       sig->nmax + 1, sig->nmax + 1 == 1 ? "" : "s", (int)n + 1);
        shsm_raise(SHSM_EXC_TYPE, "GdbClient.%s() takes from %d to %d positional arguments but %d were given", sig->name,
                   sig->nreq + 1, sig->nmax + 1, (int)n + 1);
    }
    if ((int)n < sig->nreq) {
        int missing = sig->nreq - (int)n;
        AvpBuf b;
        avp_buf_init(&b, a);
        for (int k = (int)n; k < sig->nreq; k++) {
            if (k > (int)n) avp_buf_putz(&b, k == sig->nreq - 1 ? (missing == 2 ? " and " : ", and ") : ", ");
            avp_buf_printf(&b, "'%s'", sig->params[k]);
        }
        shsm_raise(SHSM_EXC_TYPE, "GdbClient.%s() missing %d required positional argument%s: %s", sig->name, missing,
                   missing == 1 ? "" : "s", avp_buf_cz(&b));
    }
    const char *m = sig->name;
    if (!strcmp(m, "state")) {
        ShsmGdbState s = shsm_gdb_state(a, c);
        return shsm_gdb_state_to_json(a, &s);
    }
    if (!strcmp(m, "halt_reason")) return stop_or_null(a, shsm_gdb_halt_reason(a, c));
    if (!strcmp(m, "read_gprs")) {
        u64 r[32];
        size_t k = shsm_gdb_read_gprs(c, r);
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < k; i++) avp_json_arr_push(a, arr, avp_json_uint(a, r[i]));
        return arr;
    }
    if (!strcmp(m, "read_register")) return avp_json_uint(a, shsm_gdb_read_register(c, arg_int(&av[0])));
    if (!strcmp(m, "read_registers")) {
        const AvpJson *nums = av[0].v;
        AvpJson *arr = avp_json_arr(a);
        if (!nums || !(nums->kind == AVP_JSON_ARRAY))
            shsm_raise(SHSM_EXC_TYPE, "'%s' object is not iterable", arg_type_name(&av[0]));
        for (size_t i = 0; i < avp_json_len(nums); i++)
            avp_json_arr_push(a, arr, avp_json_uint(a, shsm_gdb_read_register(c, json_int(avp_json_at(nums, i)))));
        return arr;
    }
    if (!strcmp(m, "read_memory")) {
        i64 addr = arg_int(&av[0]), len = arg_int(&av[1]);
        ShsmBytes b = shsm_gdb_read_memory(a, c, addr, len);
        return shsm_gdb_json_from_bytes(a, b.p, b.n);
    }
    if (!strcmp(m, "write_memory")) {
        ShsmBytes b = arg_bytes(a, &av[1]);
        if (b.n) {
            i64 addr = arg_int(&av[0]);
            shsm_gdb_write_memory(c, addr, b.p, b.n);
        }
        return avp_json_bool(true);
    }
    if (!strcmp(m, "insert_breakpoint") || !strcmp(m, "remove_breakpoint")) {
        i64 kind = n > 1 ? arg_int(&av[1]) : 0, length = n > 2 ? arg_int(&av[2]) : 4;
        i64 addr = arg_int(&av[0]);
        if (m[0] == 'i') shsm_gdb_insert_breakpoint(c, addr, kind, length);
        else shsm_gdb_remove_breakpoint(c, addr, kind, length);
        return avp_json_bool(true);
    }
    if (!strcmp(m, "breakpoints")) {
        ShsmGdbBreakpoint *bps;
        size_t k = shsm_gdb_breakpoints(a, c, &bps);
        return shsm_gdb_breakpoints_to_json(a, bps, k);
    }
    if (!strcmp(m, "clear_breakpoints")) {
        shsm_gdb_clear_breakpoints(c);
        return avp_json_bool(true);
    }
    if (!strcmp(m, "resume")) return avp_json_bool(shsm_gdb_resume(c, arg_timeout(n ? &av[0] : NULL)));
    if (!strcmp(m, "wait_stop")) {
        double t = 0;
        if (c->running) {
            const RpcArg *x = &av[0];
            if (!x->v || !(avp_json_is_num(x->v) || x->v->kind == AVP_JSON_BOOL))
                shsm_raise(SHSM_EXC_TYPE, "'>' not supported between instances of 'float' and '%s'", arg_type_name(x));
            t = x->v->kind == AVP_JSON_BOOL ? (x->v->b ? 1.0 : 0.0) : avp_json_f64(x->v, 0);
        }
        return stop_or_null(a, shsm_gdb_wait_stop(a, c, t));
    }
    if (!strcmp(m, "step")) {
        i64 count = n > 0 ? arg_int(&av[0]) : 1;
        double t = n > 1 ? arg_timeout(&av[1]) : SHSM_GDB_NONE;
        return stop_or_null(a, shsm_gdb_step(a, c, count, t));
    }
    /* interrupt */
    return stop_or_null(a, shsm_gdb_interrupt(a, c, arg_timeout(n ? &av[0] : NULL)));
}
