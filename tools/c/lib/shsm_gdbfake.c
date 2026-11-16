/* The fake GDB stub of tools/test_dolphin.py (FakeStub), a test fixture (shsm_gdbfake.md). */
#include "shsm_gdbfake.h"

#include "avp/arena.h"
#include "avp/str.h"

#include <stdio.h>
#include <stdlib.h>

#define POLL 0.05

static char *xstrdup(const char *s, size_t n) {
    char *p = avp_xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

ShsmGdbFake *shsm_gdbfake_new(void) {
    ShsmGdbFake *f = avp_xcalloc(1, sizeof *f);
    f->gpr[1] = 0x8000F000u;
    f->pc = 0x80001000u;
    f->lr = 0x80001104u;
    f->conn = SHSM_SOCKET_INVALID;
    ShsmNetError err;
    if (shsm_net_listen("127.0.0.1", 0, 1, &f->lsock, &f->port, &err) != SHSM_NET_OK)
        avp_panic(__FILE__, __LINE__, "fake stub cannot listen: %s", err.text);
    return f;
}

static u8 cs(const char *p, size_t n) {
    unsigned s = 0;
    for (size_t i = 0; i < n; i++) s += (u8)p[i];
    return (u8)(s & 0xFF);
}

static void sendall(ShsmGdbFake *f, const void *p, size_t n) {
    ShsmNetError err;
    shsm_net_send_all(f->conn, p, n, 10.0, &err);
}

static void send_payload(ShsmGdbFake *f, const char *payload, bool bad) {
    size_t n = strlen(payload);
    char c[3];
    if (bad) snprintf(c, sizeof c, "%s", cs(payload, n) != 0 ? "00" : "01");
    else snprintf(c, sizeof c, "%02x", cs(payload, n));
    AvpBuf b = {0};
    avp_buf_putc(&b, '$');
    avp_buf_putn(&b, payload, n);
    avp_buf_putc(&b, '#');
    avp_buf_putn(&b, c, 2);
    sendall(f, b.p, b.n);
    avp_buf_free(&b);
    char *old = f->last;
    f->last = xstrdup(payload, n);
    avp_xfree(old);
}

static void reply(ShsmGdbFake *f, const char *payload) {
    if (f->corrupt > 0) {
        f->corrupt--;
        send_payload(f, payload, true);
    } else {
        send_payload(f, payload, false);
    }
}

static void stop_reply(ShsmGdbFake *f, int sig) {
    char p[64];
    f->running = false;
    snprintf(p, sizeof p, "T%02x40:%08x;01:%08x;", sig, f->pc, f->gpr[1]);
    reply(f, p);
}

static bool bp_has(ShsmGdbFake *f, u32 a) {
    for (size_t i = 0; i < f->bps.n; i++)
        if (f->bps.v[i] == a) return true;
    return false;
}

static void bp_add(ShsmGdbFake *f, u32 a) {
    if (!bp_has(f, a)) avp_vec_push(&f->bps, a);
}

static void bp_discard(ShsmGdbFake *f, u32 a) {
    for (size_t i = 0; i < f->bps.n; i++)
        if (f->bps.v[i] == a) {
            avp_vec_remove(&f->bps, i);
            return;
        }
}

static u64 hex(const char *p, size_t n) {
    char tmp[40];
    if (n >= sizeof tmp) n = sizeof tmp - 1;
    memcpy(tmp, p, n);
    tmp[n] = 0;
    return strtoull(tmp, NULL, 16);
}

static void regval(ShsmGdbFake *f, u64 n, char *out, size_t cap) {
    if (n < 32) {
        snprintf(out, cap, "%08x", f->gpr[n]);
    } else if (n < 64) {
        u64 bits;
        memcpy(&bits, &f->fpr[n - 32], 8);
        snprintf(out, cap, "%016llx", (unsigned long long)bits);
    } else {
        u32 v = 0;
        switch (n) {
        case 0x40: v = f->pc; break;
        case 0x41: v = 0x2032; break;
        case 0x43: v = f->lr; break;
        default: v = 0; break;
        }
        snprintf(out, cap, "%08x", v);
    }
}

static bool advance(ShsmGdbFake *f) {
    while (f->prog_pos < f->nprogram) {
        const ShsmGdbFakeStep *s = &f->program[f->prog_pos++];
        f->pc = s->pc;
        for (int i = 0; i < s->ngpr; i++) f->gpr[s->gpr[i].index] = s->gpr[i].value;
        for (int i = 0; i < s->nfpr; i++) f->fpr[s->fpr[i].index] = s->fpr[i].value;
        if (s->has_lr) f->lr = s->lr;
        if (bp_has(f, f->pc)) return true;
    }
    return false;
}

static bool startswith(const char *p, const char *prefix) { return prefix && !strncmp(p, prefix, strlen(prefix)); }

/* one command; false ends the connection */
static bool command(ShsmGdbFake *f, const char *p) {
    avp_vec_push(&f->received, xstrdup(p, strlen(p)));
    if (startswith(p, f->drop_on)) return false;
    if (startswith(p, f->stall_on)) return true;
    size_t n = strlen(p);
    if (!strcmp(p, "?")) {
        stop_reply(f, 5);
    } else if (!strcmp(p, "g")) {
        char out[32 * 8 + 1];
        for (int i = 0; i < 32; i++) snprintf(out + i * 8, 9, "%08x", f->gpr[i]);
        reply(f, out);
    } else if (p[0] == 'p') {
        char out[32];
        regval(f, hex(p + 1, n - 1), out, sizeof out);
        reply(f, out);
    } else if (p[0] == 'm') {
        const char *comma = strchr(p, ',');
        u64 a = hex(p + 1, (size_t)(comma - p - 1)), len = hex(comma + 1, strlen(comma + 1));
        if (a < SHSM_GDBFAKE_MEM_BASE || a + len > (u64)SHSM_GDBFAKE_MEM_BASE + SHSM_GDBFAKE_MEM_SIZE) {
            reply(f, "E01");
        } else {
            char *out = avp_xmalloc(len * 2 + 1);
            for (u64 i = 0; i < len; i++) snprintf(out + i * 2, 3, "%02x", f->mem[a - SHSM_GDBFAKE_MEM_BASE + i]);
            out[len * 2] = 0;
            reply(f, out);
            avp_xfree(out);
        }
    } else if (p[0] == 'M') {
        const char *comma = strchr(p, ','), *colon = strchr(p, ':');
        u64 a = hex(p + 1, (size_t)(comma - p - 1)), len = hex(comma + 1, (size_t)(colon - comma - 1));
        if (a < SHSM_GDBFAKE_MEM_BASE || a + len > (u64)SHSM_GDBFAKE_MEM_BASE + SHSM_GDBFAKE_MEM_SIZE) {
            reply(f, "E02");
        } else {
            const char *d = colon + 1;
            for (u64 i = 0; i < len && d[2 * i] && d[2 * i + 1]; i++)
                f->mem[a - SHSM_GDBFAKE_MEM_BASE + i] = (u8)hex(d + 2 * i, 2);
            reply(f, "OK");
        }
    } else if (p[0] == 'Z' || p[0] == 'z') {
        const char *c1 = strchr(p, ',');
        const char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
        size_t klen = c1 ? (size_t)(c1 - p - 1) : n - 1;
        char k = klen == 1 ? p[1] : 0;
        if (!(klen == 0 || (klen == 1 && k >= '0' && k <= '4'))) { /* k not in "01234" */
            reply(f, "");
            return true;
        }
        if (klen == 1 && (k == '0' || k == '1') && c1 && c2) {
            u32 a = (u32)hex(c1 + 1, (size_t)(c2 - c1 - 1));
            if (p[0] == 'Z') bp_add(f, a);
            else bp_discard(f, a);
        }
        reply(f, "OK");
    } else if (!strcmp(p, "s")) {
        if (!bp_has(f, f->pc)) f->pc += 4;
        stop_reply(f, 5);
    } else if (!strcmp(p, "c")) {
        if (bp_has(f, f->pc) || advance(f)) stop_reply(f, 5);
        else f->running = true;
    } else if (!strcmp(p, "k")) {
        return false;
    } else if (!strcmp(p, "D")) {
        reply(f, "OK");
        return false;
    } else {
        reply(f, "");
    }
    return true;
}

static void serve(void *arg) {
    ShsmGdbFake *f = arg;
    ShsmNetError err;
    for (;;) {
        if (f->stop) goto done;
        ShsmNetStatus st = shsm_net_accept(f->lsock, POLL, &f->conn, &err);
        if (st == SHSM_NET_OK) break;
        if (st != SHSM_NET_TIMEOUT) goto done;
    }
    AvpBytes buf = {0};
    u8 rb[65536];
    for (;;) {
        if (f->stop) break;
        size_t got = 0;
        ShsmNetStatus st = shsm_net_recv(f->conn, rb, sizeof rb, POLL, &got, &err);
        if (st == SHSM_NET_TIMEOUT) continue;
        if (st != SHSM_NET_OK) break;
        avp_vec_extend(&buf, rb, got);
        bool keep = true;
        size_t i = 0;
        avp_mutex_lock(&f->lock);
        while (i < buf.n && keep) {
            u8 c = buf.v[i];
            if (c == '+') {
                i++;
            } else if (c == '-') {
                i++;
                send_payload(f, f->last ? f->last : "", false);
            } else if (c == 0x03) {
                i++;
                avp_vec_push(&f->received, xstrdup("\x03", 1));
                if (f->running) {
                    stop_reply(f, 2);
                    send_payload(f, "", false);
                    stop_reply(f, 2);
                }
            } else if (c == '$') {
                u8 *h = memchr(buf.v + i, '#', buf.n - i);
                if (!h || buf.n < (size_t)(h - buf.v) + 3) break;
                size_t hp = (size_t)(h - buf.v);
                char *payload = xstrdup((const char *)buf.v + i + 1, hp - i - 1);
                char want[3];
                snprintf(want, sizeof want, "%02x", cs(payload, hp - i - 1));
                bool good = memcmp(h + 1, want, 2) == 0;
                i = hp + 3;
                if (f->nak > 0 || !good) {
                    f->nak = f->nak > 0 ? f->nak - 1 : 0;
                    sendall(f, "-", 1);
                } else {
                    sendall(f, "+", 1);
                    keep = command(f, payload);
                }
                avp_xfree(payload);
            } else {
                i++;
            }
        }
        avp_mutex_unlock(&f->lock);
        memmove(buf.v, buf.v + i, buf.n - i);
        buf.n -= i;
        if (!keep) break;
    }
    avp_vec_free(&buf);
done:
    shsm_net_close(f->conn);
    f->conn = SHSM_SOCKET_INVALID;
    shsm_net_close(f->lsock);
    f->lsock = SHSM_SOCKET_INVALID;
}

void shsm_gdbfake_start(ShsmGdbFake *f) { f->thread = avp_thread_start(serve, f, 0); }

void shsm_gdbfake_free(ShsmGdbFake *f) {
    if (!f) return;
    f->stop = 1;
    if (f->thread) avp_thread_join(f->thread);
    else shsm_net_close(f->lsock);
    for (size_t i = 0; i < f->received.n; i++) avp_xfree(f->received.v[i]);
    avp_vec_free(&f->received);
    avp_vec_free(&f->bps);
    avp_xfree(f->last);
    avp_xfree(f);
}

size_t shsm_gdbfake_received_count(ShsmGdbFake *f, const char *payload) {
    size_t n = 0;
    avp_mutex_lock(&f->lock);
    for (size_t i = 0; i < f->received.n; i++)
        if (!strcmp(f->received.v[i], payload)) n++;
    avp_mutex_unlock(&f->lock);
    return n;
}

size_t shsm_gdbfake_received_total(ShsmGdbFake *f) {
    avp_mutex_lock(&f->lock);
    size_t n = f->received.n;
    avp_mutex_unlock(&f->lock);
    return n;
}

size_t shsm_gdbfake_bp_count(ShsmGdbFake *f) {
    avp_mutex_lock(&f->lock);
    size_t n = f->bps.n;
    avp_mutex_unlock(&f->lock);
    return n;
}
