/* eq_side: State, BlockRun and Side of tools/eqcheck.py (L533-1399), and the shared exception discipline. See
 * eq_side.h. Every node-making expression of the Python is sequenced left to right through locals, so node ids
 * follow the Python's allocation order. */
#include "eq_side.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shsm_cli.h"
#include "shsm_mangle.h"

/* ---- exceptions ------------------------------------------------------------------------------------------------- */

static AVP_THREAD_LOCAL EqExc t_exc;
static AVP_THREAD_LOCAL char *t_msg;
static AVP_THREAD_LOCAL bool t_pending;

static void exc_set(int code, char *msg) {
    free(t_msg);
    t_msg = msg;
    memset(&t_exc, 0, sizeof t_exc);
    t_exc.code = code;
    t_exc.msg = t_msg;
    t_exc.block = -1;
}

static char *vfmt(const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) n = 0;
    char *m = avp_xmalloc((size_t)n + 1);
    vsnprintf(m, (size_t)n + 1, fmt, ap);
    return m;
}

static AVP_NORETURN void exc_throw(void) {
    t_pending = true;
    avp_throw(t_exc.code, "%s", t_exc.msg);
}

void eq_raise(int code, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *m = vfmt(fmt, ap);
    va_end(ap);
    exc_set(code, m);
    exc_throw();
}

void eq_raise_verdict(const char *kind, int block, const EqArg *terms, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *m = vfmt(fmt, ap);
    va_end(ap);
    exc_set(EQ_EXC_VERDICT, m);
    t_exc.vkind = kind;
    t_exc.block = block;
    if (terms) {
        t_exc.has_terms = true;
        t_exc.terms[0] = terms[0];
        t_exc.terms[1] = terms[1];
    }
    exc_throw();
}

void eq_reraise(const EqExc *e) {
    EqExc c = *e;
    size_t n = strlen(e->msg ? e->msg : "");
    char *m = avp_xmalloc(n + 1);
    memcpy(m, e->msg ? e->msg : "", n + 1);
    exc_set(c.code, m);
    t_exc.vkind = c.vkind;
    t_exc.block = c.block;
    t_exc.has_terms = c.has_terms;
    t_exc.terms[0] = c.terms[0];
    t_exc.terms[1] = c.terms[1];
    exc_throw();
}

const EqExc *eq_exc_caught(const AvpTry *tr) {
    if (t_pending && t_exc.code == tr->code && strncmp(t_exc.msg, tr->msg, sizeof tr->msg - 1) == 0) {
        t_pending = false;
        return &t_exc;
    }
    t_pending = false;
    size_t n = strlen(tr->msg);
    char *m = avp_xmalloc(n + 1);
    memcpy(m, tr->msg, n + 1);
    exc_set(tr->code, m);
    return &t_exc;
}

EqExc *eq_exc_save(AvpArena *a, const EqExc *e) {
    EqExc *c = AVP_NEW(a, EqExc);
    *c = *e;
    c->msg = avp_strdup(a, e->msg ? e->msg : "");
    return c;
}

const char *eq_exc_class(int code) {
    switch (code) {
    case EQ_EXC_UNSUPPORTED: return "Unsupported";
    case EQ_EXC_NOTPROVEN: return "NotProven";
    case EQ_EXC_VERDICT: return "Verdict";
    case EQ_EXC_INDEX: return "IndexError";
    default: return shsm_exc_class(code);
    }
}

const char *eq_exc_str(AvpArena *a, const EqExc *e) {
    if (e->code == SHSM_EXC_KEY) return eq_py_repr(a, avp_str(e->msg));
    return avp_strdup(a, e->msg ? e->msg : "");
}

static AVP_NORETURN void raise_index(void) { eq_raise(EQ_EXC_INDEX, "list index out of range"); }

/* ---- small helpers ------------------------------------------------------------------------------------------------ */

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool str_eq(AvpStr s, const char *z) { return avp_str_eqz(s, z); }

static bool cz_in(const char *s, const char *const *list) {
    for (; *list; list++)
        if (strcmp(s, *list) == 0) return true;
    return false;
}

/* A register number: decimal without a leading zero (except "0"), below lim; -1 otherwise. */
static int regnum(const char *p, size_t n, int lim) {
    if (n == 0 || n > 2) return -1;
    if (n == 2 && p[0] == '0') return -1;
    int v = 0;
    for (size_t i = 0; i < n; i++) {
        if (!is_digit(p[i])) return -1;
        v = v * 10 + (p[i] - '0');
    }
    return v < lim ? v : -1;
}

int eq_loc_index(AvpStr s) {
    if (s.n >= 2 && s.p[0] == 'r') {
        int v = regnum(s.p + 1, s.n - 1, 32);
        return v < 0 ? -1 : v;
    }
    if (s.n >= 2 && s.p[0] == 'f') {
        int v = regnum(s.p + 1, s.n - 1, 32);
        return v < 0 ? -1 : 32 + v;
    }
    if (s.n >= 3 && s.p[0] == 'c' && s.p[1] == 'r') {
        int v = regnum(s.p + 2, s.n - 2, 32);
        return v < 0 ? -1 : 64 + v;
    }
    if (str_eq(s, "ca")) return 96;
    if (str_eq(s, "ctr")) return 97;
    if (str_eq(s, "lr")) return 98;
    return -1;
}

const char *eq_py_repr(AvpArena *a, AvpStr s) {
    bool sq = false, dq = false;
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] == '\'') sq = true;
        if (s.p[i] == '"') dq = true;
    }
    char q = (sq && !dq) ? '"' : '\'';
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, q);
    for (size_t i = 0; i < s.n; i++) {
        u8 c = (u8)s.p[i];
        if (c == '\\') avp_buf_printf(&b, "\\\\");
        else if (c == (u8)q) avp_buf_printf(&b, "\\%c", q);
        else if (c == '\n') avp_buf_printf(&b, "\\n");
        else if (c == '\r') avp_buf_printf(&b, "\\r");
        else if (c == '\t') avp_buf_printf(&b, "\\t");
        else if (c < 0x20 || c == 0x7F) avp_buf_printf(&b, "\\x%02x", c);
        else avp_buf_putc(&b, (char)c);
    }
    avp_buf_putc(&b, q);
    return avp_buf_cz(&b);
}

static AVP_NORETURN void int_error(AvpStr tok, int base) {
    AvpArena *sc = avp_scratch();
    eq_raise(SHSM_EXC_VALUE, "invalid literal for int() with base %d: %s", base, eq_py_repr(sc, tok));
}

static int digit_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 99;
}

/* Digits of base b with single underscores between them (lead: an underscore may come first, after a prefix). */
static bool parse_digits(AvpStr s, size_t i, int b, bool lead, u64 *out) {
    u64 v = 0;
    size_t nd = 0;
    while (i < s.n) {
        if (s.p[i] == '_') {
            if (nd == 0 && !lead) return false;
            i++;
            if (i >= s.n || digit_val(s.p[i]) >= b) return false;
        }
        int d = digit_val(s.p[i]);
        if (d >= b) return false;
        v = v * (u64)b + (u64)d;
        nd++;
        i++;
    }
    if (nd == 0) return false;
    *out = v;
    return true;
}

i64 eq_py_int0(AvpStr tok) {
    AvpStr s = avp_str_strip(tok);
    size_t i = 0;
    bool neg = false;
    if (i < s.n && (s.p[i] == '+' || s.p[i] == '-')) neg = s.p[i++] == '-';
    u64 v;
    if (i + 1 < s.n && s.p[i] == '0' && (s.p[i + 1] == 'x' || s.p[i + 1] == 'X' || s.p[i + 1] == 'o' ||
                                          s.p[i + 1] == 'O' || s.p[i + 1] == 'b' || s.p[i + 1] == 'B')) {
        char p = (char)(s.p[i + 1] | 0x20);
        int b = p == 'x' ? 16 : p == 'o' ? 8 : 2;
        if (!parse_digits(s, i + 2, b, true, &v)) int_error(tok, 0);
    } else {
        if (!parse_digits(s, i, 10, false, &v)) int_error(tok, 0);
        if (s.p[i] == '0')
            for (size_t j = i; j < s.n; j++)
                if (s.p[j] != '0' && s.p[j] != '_') int_error(tok, 0);
    }
    return neg ? (i64)(0 - v) : (i64)v;
}

i64 eq_py_int10(AvpStr tok) {
    AvpStr s = avp_str_strip(tok);
    size_t i = 0;
    bool neg = false;
    if (i < s.n && (s.p[i] == '+' || s.p[i] == '-')) neg = s.p[i++] == '-';
    u64 v;
    if (!parse_digits(s, i, 10, false, &v)) int_error(tok, 10);
    return neg ? (i64)(0 - v) : (i64)v;
}

/* "%+#x" % v */
static const char *signed_hex(AvpArena *a, i64 v) {
    u64 m = v < 0 ? 0 - (u64)v : (u64)v;
    return avp_sprintf(a, "%c0x%llx", v < 0 ? '-' : '+', (unsigned long long)m);
}

/* a growable arena array */
static void *grow(AvpArena *a, void *p, size_t *cap, size_t need, size_t elt) {
    if (need <= *cap) return p;
    size_t nc = *cap ? *cap * 2 : 8;
    while (nc < need) nc *= 2;
    void *q = avp_arena_realloc(a, p, *cap * elt, nc * elt);
    *cap = nc;
    return q;
}

/* ---- callee_arity --------------------------------------------------------------------------------------------------- */

/* s.replace(old, "") */
static char *remove_all(AvpArena *a, const char *s, const char *old) {
    size_t lo = strlen(old), n = strlen(s);
    char *out = avp_alloc(a, n + 1);
    size_t k = 0;
    for (size_t i = 0; i < n;) {
        if (strncmp(s + i, old, lo) == 0) {
            i += lo;
            continue;
        }
        out[k++] = s[i++];
    }
    out[k] = 0;
    return out;
}

EqArity eq_callee_arity(const char *name) {
    EqArity none = {EQ_ARITY_UNKNOWN, 0, 0};
    if (!name || !*name) return none;
    AvpArena *sc = avp_scratch();
    AvpArenaMark mark = avp_arena_mark(sc);
    ShsmDemangled *volatile d = NULL;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        d = shsm_mangle_parse(sc, name);
        avp_try_end(&tr);
    } else {
        d = NULL; /* except Exception */
    }
    EqArity out = none;
    if (d == NULL || strcmp(d->kind, "function") != 0) goto done;
    int gpr = 1, fpr = 0;
    if (d->nscope) gpr++;
    for (size_t k = 0; k < d->nparams; k++) {
        char *p1 = remove_all(sc, d->params[k], "const ");
        char *p2 = remove_all(sc, p1, "volatile ");
        AvpStr t = avp_str_strip(avp_str(p2));
        if (str_eq(t, "void") || t.n == 0) continue;
        if (str_eq(t, "...")) goto done;
        if (avp_str_findc(t, '*') >= 0 || avp_str_findc(t, '&') >= 0) gpr++;
        else if (str_eq(t, "float") || str_eq(t, "double")) fpr++;
        else if (avp_str_contains(t, AVP_S("long long"))) gpr += 3;
        else gpr++;
    }
    if (gpr > 8 || fpr > 8) out.kind = EQ_ARITY_STACK;
    else {
        out.kind = EQ_ARITY_REGS;
        out.ng = gpr;
        out.nf = fpr;
    }
done:
    avp_arena_reset(sc, mark);
    return out;
}

EqArityMap *eq_arity_map_new(AvpArena *a) {
    EqArityMap *m = AVP_NEW(a, EqArityMap);
    m->a = a;
    return m;
}

bool eq_arity_map_get(const EqArityMap *m, int id, EqArity *out) {
    for (size_t k = 0; k < m->n; k++)
        if (m->ids[k] == id) {
            *out = m->vals[k];
            return true;
        }
    return false;
}

void eq_arity_map_set(EqArityMap *m, int id, EqArity v) {
    for (size_t k = 0; k < m->n; k++)
        if (m->ids[k] == id) {
            m->vals[k] = v;
            return;
        }
    size_t c1 = m->cap, c2 = m->cap;
    m->ids = grow(m->a, m->ids, &c1, m->n + 1, sizeof *m->ids);
    m->vals = grow(m->a, m->vals, &c2, m->n + 1, sizeof *m->vals);
    m->cap = c1;
    m->ids[m->n] = id;
    m->vals[m->n] = v;
    m->n++;
}

/* ---- State ----------------------------------------------------------------------------------------------------------- */

EqState *eq_state_new(AvpArena *a) { return AVP_NEW(a, EqState); }

EqState *eq_state_copy(AvpArena *a, const EqState *s) {
    EqState *c = AVP_NEW(a, EqState);
    memcpy(c->r, s->r, sizeof c->r);
    c->nxr = c->xrcap = s->nxr;
    c->xr = s->nxr ? avp_memdup(a, s->xr, s->nxr * sizeof *s->xr) : NULL;
    c->nslots = c->slotcap = s->nslots;
    c->slots = s->nslots ? avp_memdup(a, s->slots, s->nslots * sizeof *s->slots) : NULL;
    c->mem = s->mem;
    c->nstores = c->storecap = s->nstores;
    c->stores = s->nstores ? avp_memdup(a, s->stores, s->nstores * sizeof *s->stores) : NULL;
    c->frame = s->frame;
    c->ncall = s->ncall;
    c->ndead = c->deadcap = s->ndead;
    c->dead = s->ndead ? avp_memdup(a, s->dead, s->ndead * sizeof *s->dead) : NULL;
    return c;
}

EqState *eq_state_entry(EqTerms *t) {
    EqState *s = eq_state_new(t->a);
    for (int k = 0; k < EQ_NREG_LOCS; k++) s->r[k] = eq_entry(t, EQ_REG_LOCS[k]);
    EqArg m = eq_as("mem");
    s->mem = eq_leaf_d(t, EQ_OP_E, &m, 1);
    return s;
}

EqNode *eq_state_reg(const EqState *s, AvpStr name) {
    int k = eq_loc_index(name);
    if (k >= 0) return s->r[k];
    for (size_t i = 0; i < s->nxr; i++)
        if (str_eq(name, s->xr[i].name)) return s->xr[i].v;
    return NULL;
}

EqNode *eq_state_reg_req(const EqState *s, AvpStr name) {
    EqNode *v = eq_state_reg(s, name);
    if (!v) {
        AvpArena *sc = avp_scratch();
        eq_raise(SHSM_EXC_KEY, "%s", avp_cz(sc, name));
    }
    return v;
}

void eq_state_set_reg(AvpArena *a, EqState *s, AvpStr name, EqNode *v) {
    int k = eq_loc_index(name);
    if (k >= 0) {
        s->r[k] = v;
        return;
    }
    for (size_t i = 0; i < s->nxr; i++)
        if (str_eq(name, s->xr[i].name)) {
            s->xr[i].v = v;
            return;
        }
    s->xr = grow(a, s->xr, &s->xrcap, s->nxr + 1, sizeof *s->xr);
    s->xr[s->nxr].name = avp_cz(a, name);
    s->xr[s->nxr].v = v;
    s->nxr++;
}

static EqNode *regz(const EqState *s, const char *name) { return eq_state_reg_req(s, avp_str(name)); }
static void set_regz(AvpArena *a, EqState *s, const char *name, EqNode *v) { eq_state_set_reg(a, s, avp_str(name), v); }

static i64 slot_index(const EqState *s, i64 off, i64 w) {
    for (size_t k = 0; k < s->nslots; k++)
        if (s->slots[k].off == off && s->slots[k].w == w) return (i64)k;
    return -1;
}

EqNode *eq_state_slot_get(const EqState *s, i64 off, i64 w) {
    i64 k = slot_index(s, off, w);
    return k < 0 ? NULL : s->slots[k].v;
}

void eq_state_slot_set(AvpArena *a, EqState *s, i64 off, i64 w, EqNode *v) {
    i64 k = slot_index(s, off, w);
    if (k >= 0) {
        s->slots[k].v = v;
        return;
    }
    s->slots = grow(a, s->slots, &s->slotcap, s->nslots + 1, sizeof *s->slots);
    s->slots[s->nslots++] = (EqSlot){off, w, v};
}

EqNode *eq_state_slot_pop(EqState *s, i64 off, i64 w) {
    i64 k = slot_index(s, off, w);
    if (k < 0) return NULL;
    EqNode *v = s->slots[k].v;
    memmove(&s->slots[k], &s->slots[k + 1], (s->nslots - (size_t)k - 1) * sizeof *s->slots);
    s->nslots--;
    return v;
}

void eq_state_store_push(AvpArena *a, EqState *s, EqNode *n) {
    s->stores = grow(a, s->stores, &s->storecap, s->nstores + 1, sizeof *s->stores);
    s->stores[s->nstores++] = n;
}

bool eq_state_dead_has(const EqState *s, i64 off, i64 w) {
    for (size_t k = 0; k < s->ndead; k++)
        if (s->dead[k].off == off && s->dead[k].w == w) return true;
    return false;
}

void eq_state_dead_add(AvpArena *a, EqState *s, i64 off, i64 w) {
    if (eq_state_dead_has(s, off, w)) return;
    s->dead = grow(a, s->dead, &s->deadcap, s->ndead + 1, sizeof *s->dead);
    s->dead[s->ndead++] = (EqSlotKey){off, w};
}

static void add_effect(AvpArena *a, EqBlockRun *run, int kind, EqNode *n, const char *text, u32 declared) {
    run->effects = grow(a, run->effects, &run->effcap, run->neffects + 1, sizeof *run->effects);
    run->effects[run->neffects++] = (EqEffect){kind, n, text, declared};
}

/* ---- split / is_io ------------------------------------------------------------------------------------------------- */

static bool arg_is_int(EqArg x, i64 v) { return (x.kind == EQ_ARG_INT || x.kind == EQ_ARG_BOOL) && x.i == v; }

EqSplit eq_split(EqNode *addr) {
    EqSplit s;
    memset(&s, 0, sizeof s);
    if (addr->op == EQ_OP_C) {
        s.none = true;
        s.off = eq_s32(addr->args[0].i);
    } else if (addr->op == EQ_OP_LIN) {
        s.items = addr->args + 1;
        s.n = addr->nargs - 1;
        s.off = eq_s32(addr->args[0].i);
    } else {
        s.is_pair = true;
        s.n = 2;
        s.pair[0] = eq_an(addr);
        s.pair[1] = eq_ai(1);
        s.off = 0;
    }
    return s;
}

EqArg eq_split_item(const EqSplit *s, int k) { return s->is_pair ? s->pair[k] : s->items[k]; }

bool eq_split_base_eq(const EqSplit *a, const EqSplit *b) {
    if (a->none || b->none) return a->none && b->none;
    if (a->n != b->n) return false;
    for (int k = 0; k < a->n; k++)
        if (eq_arg_differs(eq_split_item(a, k), eq_split_item(b, k))) return false;
    return true;
}

/* b == (sp0, 1) */
static bool base_is_sp(const EqSplit *s, const EqNode *sp0) {
    if (s->none || s->n != 2) return false;
    EqArg x = eq_split_item(s, 0), y = eq_split_item(s, 1);
    return x.kind == EQ_ARG_NODE && x.n == sp0 && arg_is_int(y, 1);
}

bool eq_is_io(const EqNode *addr) { return addr->op == EQ_OP_C && addr->args[0].i >= 0xC0000000ll; }

/* ---- Side ------------------------------------------------------------------------------------------------------------ */

EqSide *eq_side_new(const char *name, const CcFn *fn, EqResolveFn resolve, void *resolve_ctx, EqTerms *t, EqLive *live) {
    AvpArena *a = t->a;
    EqSide *s = AVP_NEW(a, EqSide);
    s->name = avp_strdup(a, name);
    s->fn = fn;
    s->rows = fn->rows;
    s->nrows = fn->nrows;
    s->resolve = resolve;
    s->resolve_ctx = resolve_ctx;
    s->t = t;
    s->live = live;
    s->arity_map = NULL;
    s->a = a;
    s->cfg = wall_cfg_of_fn(a, fn);
    s->ops = AVP_NEWN(a, const char **, fn->nrows ? fn->nrows : 1);
    s->nops = AVP_NEWN(a, size_t, fn->nrows ? fn->nrows : 1);
    return s;
}

EqNode *eq_side_fresh(EqSide *s, const char *tag) {
    s->ucount++;
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    EqNode *n = eq_unknown(s->t, s->name, avp_sprintf(sc, "%s#%lld", tag, (long long)s->ucount));
    avp_arena_reset(sc, m);
    return n;
}

static bool sp_char(char c) { return cc_is_space((u8)c); }

static AvpStr strip_ws(AvpStr x) {
    while (x.n && sp_char(x.p[0])) x.p++, x.n--;
    while (x.n && sp_char(x.p[x.n - 1])) x.n--;
    return x;
}

const char **eq_side_operands(EqSide *s, size_t i, size_t *n) {
    if (s->ops[i]) {
        *n = s->nops[i];
        return s->ops[i];
    }
    AvpStr text = s->rows[i].text;
    size_t p = 0;
    while (p < text.n && sp_char(text.p[p])) p++;
    while (p < text.n && !sp_char(text.p[p])) p++;
    while (p < text.n && sp_char(text.p[p])) p++;
    size_t cnt = 0, cap = 0;
    const char **o = NULL;
    if (p < text.n) {
        AvpStr rest = avp_strn(text.p + p, text.n - p);
        size_t st = 0;
        for (size_t k = 0; k <= rest.n; k++) {
            if (k == rest.n || rest.p[k] == ',') {
                o = grow(s->a, (void *)o, &cap, cnt + 1, sizeof *o);
                o[cnt++] = avp_cz(s->a, strip_ws(avp_strn(rest.p + st, k - st)));
                st = k + 1;
            }
        }
    }
    if (!o) o = AVP_NEWN(s->a, const char *, 1);
    s->ops[i] = o;
    s->nops[i] = cnt;
    *n = cnt;
    return o;
}

EqNode *eq_side_addr_of(EqSide *s, size_t i) {
    EqNode *a = s->resolve(s->resolve_ctx, s->t, &s->rows[i]);
    if (!a) eq_raise(EQ_EXC_NOTPROVEN, "unresolved relocation at %.*s", AVP_SF(s->rows[i].text));
    return a;
}

static EqImm imm_n(i64 v) {
    EqImm im = {true, v, NULL, NULL};
    return im;
}

EqImm eq_side_imm(EqSide *s, size_t i, AvpStr tok, bool is_signed) {
    i64 at = avp_str_rfind(tok, AVP_S("@"), 0, AVP_END);
    if (at >= 0) {
        AvpStr k = avp_str_slice(tok, at + 1, AVP_END);
        i64 lp = avp_str_findc(k, '(');
        if (lp >= 0) k.n = (size_t)lp;
        EqNode *a = eq_side_addr_of(s, i);
        const char *kind = str_eq(k, "sda21") ? "sda21"
                           : str_eq(k, "ha")  ? "ha"
                           : str_eq(k, "h")   ? "h"
                           : str_eq(k, "l")   ? "l"
                                              : avp_cz(s->a, k);
        EqImm im = {false, 0, kind, a};
        if (strcmp(kind, "sda21") == 0) return im;
        if (a->op == EQ_OP_C) {
            i64 v = a->args[0].i, h;
            if (strcmp(kind, "ha") == 0) h = (v + 0x8000) >> 16;
            else if (strcmp(kind, "h") == 0) h = v >> 16;
            else if (strcmp(kind, "l") == 0) h = v;
            else eq_raise(SHSM_EXC_KEY, "%s", kind);
            h &= 0xFFFF;
            return imm_n(is_signed && strcmp(kind, "l") == 0 ? eq_s16(h) : h);
        }
        return im;
    }
    i64 v = eq_py_int0(tok);
    return imm_n(is_signed ? eq_s16(v) : (v & 0xFFFF));
}

EqNode *eq_side_imm_node(EqSide *s, EqImm im, bool is_signed) {
    EqTerms *t = s->t;
    if (im.is_n) return eq_c(t, im.n);
    if (strcmp(im.kind, "l") == 0) return eq_node1(t, is_signed ? EQ_OP_LOS : EQ_OP_LOU, im.a, EQ_MASK);
    if (strcmp(im.kind, "sda21") == 0) return im.a;
    eq_raise(EQ_EXC_UNSUPPORTED, "relocation half @%s as a low immediate", im.kind);
}

EqNode *eq_side_hi_node(EqSide *s, EqImm im) {
    EqTerms *t = s->t;
    if (im.is_n) return eq_c(t, (im.n & 0xFFFF) << 16);
    if (strcmp(im.kind, "ha") == 0) {
        EqNode *h = eq_node1(t, EQ_OP_HA, im.a, EQ_MASK);
        return eq_mulc(t, h, 0x10000);
    }
    if (strcmp(im.kind, "h") == 0) {
        EqNode *h = eq_node1(t, EQ_OP_HI, im.a, EQ_MASK);
        return eq_mulc(t, h, 0x10000);
    }
    eq_raise(EQ_EXC_UNSUPPORTED, "relocation half @%s as a high immediate", im.kind);
}

/* _MEM: ^(?P<disp>.*)\((?P<base>r\d+)\)$ */
static bool mem_match(AvpStr tok, AvpStr *disp, AvpStr *base) {
    if (!tok.n || tok.p[tok.n - 1] != ')') return false;
    i64 lp = avp_str_rfind(tok, AVP_S("("), 0, AVP_END);
    if (lp < 0) return false;
    AvpStr b = avp_strn(tok.p + lp + 1, tok.n - (size_t)lp - 2);
    if (b.n < 2 || b.p[0] != 'r') return false;
    for (size_t k = 1; k < b.n; k++)
        if (!is_digit(b.p[k])) return false;
    *disp = avp_strn(tok.p, (size_t)lp);
    *base = b;
    return true;
}

EqNode *eq_side_ea(EqSide *s, EqState *st, size_t i, AvpStr tok, const char **base) {
    AvpStr disp, b;
    if (!mem_match(tok, &disp, &b)) eq_raise(EQ_EXC_UNSUPPORTED, "memory operand %s", eq_py_repr(s->a, tok));
    EqImm im = eq_side_imm(s, i, disp.n ? disp : AVP_S("0"), true);
    *base = avp_cz(s->a, b);
    if (!im.is_n && strcmp(im.kind, "sda21") == 0) return im.a;
    EqNode *bn = str_eq(b, "r0") ? eq_c(s->t, 0) : eq_state_reg_req(st, b);
    EqNode *n = eq_side_imm_node(s, im, true);
    return eq_add(s->t, bn, n);
}

/* ---- memory ------------------------------------------------------------------------------------------------------ */

bool eq_side_stack_off(const EqSide *s, const EqNode *addr, i64 *out) {
    i64 off;
    if (addr == s->t->sp0) off = 0;
    else if (addr->op == EQ_OP_LIN && addr->nargs == 3 && addr->args[1].kind == EQ_ARG_NODE &&
             addr->args[1].n == s->t->sp0 && arg_is_int(addr->args[2], 1))
        off = eq_s32(addr->args[0].i);
    else
        return false;
    if (off < 0 || off == 4) {
        *out = off;
        return true;
    }
    return false;
}

bool eq_side_may_alias(const EqSide *s, EqNode *a1, i64 w1, EqNode *a2, i64 w2) {
    EqSplit b1 = eq_split(a1), b2 = eq_split(a2);
    if (eq_split_base_eq(&b1, &b2)) return b1.off < b2.off + w2 && b2.off < b1.off + w1;
    if ((b1.none && base_is_sp(&b2, s->t->sp0)) || (b2.none && base_is_sp(&b1, s->t->sp0))) return false;
    return true;
}

EqNode *eq_side_ext(EqSide *s, EqNode *v, i64 srcw, i64 k, i64 w) {
    EqTerms *t = s->t;
    if (k == 0 && w == srcw) return v;
    if (v->op == EQ_OP_CAT) {
        i64 pos = 0;
        for (int i = 0; i + 1 < v->nargs; i += 2) {
            EqNode *pv = v->args[i].n;
            i64 pw = v->args[i + 1].i;
            if (pos <= k && k + w <= pos + pw) return eq_side_ext(s, pv, pw, k - pos, w);
            pos += pw;
        }
    }
    if (srcw <= 4) {
        EqNode *x = eq_shr(t, v, 8 * (srcw - k - w));
        return eq_and_mask(t, x, eq_wmask(w));
    }
    EqArg args[4] = {eq_an(v), eq_ai(srcw), eq_ai(k), eq_ai(w)};
    return eq_node(t, EQ_OP_EXT, args, 4, eq_wmask(w));
}

EqNode *eq_side_cat(EqSide *s, const EqPiece *pieces, size_t n) {
    EqTerms *t = s->t;
    i64 total = 0;
    for (size_t k = 0; k < n; k++) total += pieces[k].w;
    if (total <= 4) {
        EqNode **out = AVP_NEWN(s->a, EqNode *, n ? n : 1);
        i64 pos = total;
        for (size_t k = 0; k < n; k++) {
            pos -= pieces[k].w;
            EqNode *m = eq_and_mask(t, pieces[k].v, eq_wmask(pieces[k].w));
            out[k] = eq_shl(t, m, 8 * pos);
        }
        return eq_orv(t, out, n);
    }
    EqArg *flat = AVP_NEWN(s->a, EqArg, 2 * n ? 2 * n : 1);
    for (size_t k = 0; k < n; k++) {
        flat[2 * k] = eq_an(pieces[k].v);
        flat[2 * k + 1] = eq_ai(pieces[k].w);
    }
    return eq_node(t, EQ_OP_CAT, flat, (int)(2 * n), EQ_MASK);
}

EqNode *eq_side_read_slot(EqSide *s, EqState *st, i64 off, i64 w) {
    for (size_t k = 0; k < st->ndead; k++) {
        i64 o2 = st->dead[k].off, w2 = st->dead[k].w;
        if (o2 < off + w && off < o2 + w2)
            eq_raise(EQ_EXC_NOTPROVEN, "reads the frame word a callee wrote its return address to (r1%s)",
                     signed_hex(s->a, off));
    }
    EqNode *hit = eq_state_slot_get(st, off, w);
    if (hit) return hit;
    EqSlot *over = AVP_NEWN(s->a, EqSlot, st->nslots ? st->nslots : 1);
    size_t nover = 0;
    for (size_t k = 0; k < st->nslots; k++) {
        i64 o2 = st->slots[k].off, w2 = st->slots[k].w;
        if (o2 <= off && off + w <= o2 + w2) return eq_side_ext(s, st->slots[k].v, w2, off - o2, w);
        if (o2 < off + w && off < o2 + w2) over[nover++] = st->slots[k];
    }
    if (nover) {
        /* over.sort(key=lambda x: x[0]): stable */
        for (size_t k = 1; k < nover; k++) {
            EqSlot x = over[k];
            size_t j = k;
            while (j > 0 && over[j - 1].off > x.off) {
                over[j] = over[j - 1];
                j--;
            }
            over[j] = x;
        }
        i64 pos = off;
        EqPiece *pieces = AVP_NEWN(s->a, EqPiece, 2 * nover + 1);
        size_t np = 0;
        for (size_t k = 0; k < nover; k++) {
            i64 o2 = over[k].off, w2 = over[k].w;
            if (o2 > pos) {
                EqNode *d = eq_side_default_slot(s, st, pos, o2 - pos);
                pieces[np++] = (EqPiece){d, o2 - pos};
                pos = o2;
            }
            i64 lo = o2 > pos ? o2 : pos, hi = (o2 + w2 < off + w) ? o2 + w2 : off + w;
            if (lo < pos || hi <= lo)
                eq_raise(EQ_EXC_NOTPROVEN, "overlapping stack accesses at r1%s", signed_hex(s->a, off));
            EqNode *e = eq_side_ext(s, over[k].v, w2, lo - o2, hi - lo);
            pieces[np++] = (EqPiece){e, hi - lo};
            pos = hi;
        }
        if (pos < off + w) {
            EqNode *d = eq_side_default_slot(s, st, pos, off + w - pos);
            pieces[np++] = (EqPiece){d, off + w - pos};
        }
        return eq_side_cat(s, pieces, np);
    }
    return eq_side_default_slot(s, st, off, w);
}

EqNode *eq_side_default_slot(EqSide *s, EqState *st, i64 off, i64 w) {
    if (!st->frame) {
        AvpArena *sc = avp_scratch();
        AvpArenaMark m = avp_arena_mark(sc);
        EqArg name = eq_as(avp_sprintf(sc, "slot%+lld.%lld", (long long)off, (long long)w));
        EqNode *n = eq_leaf_d(s->t, EQ_OP_E, &name, 1);
        avp_arena_reset(sc, m);
        return n;
    }
    EqArg args[3] = {eq_an(st->frame), eq_ai(off), eq_ai(w)};
    return eq_node(s->t, EQ_OP_FA, args, 3, eq_wmask(w));
}

void eq_side_write_slot(EqSide *s, EqState *st, i64 off, i64 w, EqNode *val) {
    size_t nd = 0;
    for (size_t k = 0; k < st->ndead; k++) {
        i64 o2 = st->dead[k].off, w2 = st->dead[k].w;
        if (!(off <= o2 && o2 + w2 <= off + w)) st->dead[nd++] = st->dead[k];
    }
    st->ndead = nd;
    size_t nk = st->nslots;
    EqSlotKey *keys = AVP_NEWN(s->a, EqSlotKey, nk ? nk : 1);
    for (size_t k = 0; k < nk; k++) keys[k] = (EqSlotKey){st->slots[k].off, st->slots[k].w};
    for (size_t k = 0; k < nk; k++) {
        i64 o2 = keys[k].off, w2 = keys[k].w;
        if (o2 < off + w && off < o2 + w2) {
            EqNode *v = eq_state_slot_pop(st, o2, w2);
            if (!v) eq_raise(SHSM_EXC_KEY, "(%lld, %lld)", (long long)o2, (long long)w2);
            if (o2 < off) {
                EqNode *e = eq_side_ext(s, v, w2, 0, off - o2);
                eq_state_slot_set(s->a, st, o2, off - o2, e);
            }
            if (o2 + w2 > off + w) {
                EqNode *e = eq_side_ext(s, v, w2, off + w - o2, o2 + w2 - off - w);
                eq_state_slot_set(s->a, st, off + w, o2 + w2 - off - w, e);
            }
        }
    }
    eq_state_slot_set(s->a, st, off, w, val);
}

/* node(op, [first] + list) */
static EqNode *node_list(EqSide *s, int op, EqNode *first, EqNode *const *xs, size_t n, EqNode *last) {
    size_t total = 1 + n + (last ? 1 : 0);
    EqArg *args = AVP_NEWN(s->a, EqArg, total);
    args[0] = eq_an(first);
    for (size_t k = 0; k < n; k++) args[1 + k] = eq_an(xs[k]);
    if (last) args[1 + n] = eq_an(last);
    return eq_node(s->t, op, args, (int)total, EQ_MASK);
}

EqNode *eq_side_load(EqSide *s, EqState *st, EqBlockRun *run, EqNode *addr, i64 w) {
    i64 off;
    if (eq_side_stack_off(s, addr, &off)) return eq_side_read_slot(s, st, off, w);
    EqTerms *t = s->t;
    if (eq_is_io(addr)) {
        EqNode *mv = node_list(s, EQ_OP_MV, st->mem, st->stores, st->nstores, NULL);
        EqArg la[3] = {eq_ai(w), eq_an(addr), eq_an(mv)};
        EqNode *v = eq_node(t, EQ_OP_LD, la, 3, eq_wmask(w));
        EqNode *io = eq_node(t, EQ_OP_IOLOAD, la, 3, EQ_MASK);
        add_effect(s->a, run, EQ_EFF_IO, io,
                   avp_sprintf(s->a, "load %lld from %s", (long long)w, eq_show(s->a, addr, EQ_SHOW_DEPTH)), 0);
        return v;
    }
    EqNode **alias = AVP_NEWN(s->a, EqNode *, st->nstores ? st->nstores : 1);
    size_t na = 0;
    for (size_t k = 0; k < st->nstores; k++) {
        EqNode *x = st->stores[k];
        if (eq_side_may_alias(s, x->args[1].n, x->args[0].i, addr, w)) alias[na++] = x;
    }
    if (na) {
        EqNode *last = alias[na - 1];
        if (last->args[1].n == addr && last->args[0].i == w) return last->args[2].n;
        EqSplit b1 = eq_split(last->args[1].n), b2 = eq_split(addr);
        if (eq_split_base_eq(&b1, &b2) && b1.off <= b2.off && b2.off + w <= b1.off + last->args[0].i)
            return eq_side_ext(s, last->args[2].n, last->args[0].i, b2.off - b1.off, w);
    }
    EqNode *mv = na ? node_list(s, EQ_OP_MV, st->mem, alias, na, NULL) : st->mem;
    EqArg la[3] = {eq_ai(w), eq_an(addr), eq_an(mv)};
    return eq_node(t, EQ_OP_LD, la, 3, eq_wmask(w));
}

void eq_side_store(EqSide *s, EqState *st, EqBlockRun *run, EqNode *addr, i64 w, EqNode *val, bool backchain) {
    i64 off;
    if (eq_side_stack_off(s, addr, &off)) {
        eq_side_write_slot(s, st, off, w, val);
        return;
    }
    if (val->sp && !backchain)
        eq_raise(EQ_EXC_NOTPROVEN, "a frame address is stored to memory (%s)", eq_show(s->a, addr, EQ_SHOW_DEPTH));
    EqArg sa[3] = {eq_ai(w), eq_an(addr), eq_an(val)};
    EqNode *sn = eq_node(s->t, EQ_OP_ST, sa, 3, EQ_MASK);
    if (eq_is_io(addr)) {
        add_effect(s->a, run, EQ_EFF_IO, sn,
                   avp_sprintf(s->a, "store %lld to %s", (long long)w, eq_show(s->a, addr, EQ_SHOW_DEPTH)), 0);
        st->mem = node_list(s, EQ_OP_MEM, st->mem, st->stores, st->nstores, sn);
        st->nstores = 0;
        return;
    }
    eq_state_store_push(s->a, st, sn);
    add_effect(s->a, run, EQ_EFF_ST, sn, NULL, 0);
}

static int cmp_id(const void *x, const void *y) {
    int a = (*(EqNode *const *)x)->id, b = (*(EqNode *const *)y)->id;
    return (a > b) - (a < b);
}

EqNode *eq_side_mem_now(EqSide *s, const EqState *st) {
    if (!st->nstores) return st->mem;
    EqNode **xs = avp_memdup(s->a, st->stores, st->nstores * sizeof *xs);
    qsort(xs, st->nstores, sizeof *xs, cmp_id); /* ids are unique: the order is total */
    return node_list(s, EQ_OP_MEM, st->mem, xs, st->nstores, NULL);
}

/* ---- condition register ------------------------------------------------------------------------------------------ */

static void set_cr(EqSide *s, EqState *st, i64 bit, EqNode *v) {
    char name[32];
    snprintf(name, sizeof name, "cr%lld", (long long)bit);
    set_regz(s->a, st, name, v);
}

static EqNode *get_cr(const EqState *st, i64 bit) {
    char name[32];
    snprintf(name, sizeof name, "cr%lld", (long long)bit);
    return regz(st, name);
}

void eq_side_set_cmp(EqSide *s, EqState *st, i64 f, EqNode *lt, EqNode *gt, EqNode *eq, EqNode *so) {
    i64 b = 4 * f;
    set_cr(s, st, b, lt);
    set_cr(s, st, b + 1, gt);
    set_cr(s, st, b + 2, eq);
    set_cr(s, st, b + 3, so);
}

void eq_side_set_cr0(EqSide *s, EqState *st, EqNode *v) {
    EqTerms *t = s->t;
    EqNode *z = eq_c(t, 0);
    EqNode *lt = eq_lt(t, true, v, z);
    EqNode *gt = eq_lt(t, true, z, v);
    EqNode *eq = eq_eq(t, v, z);
    eq_side_set_cmp(s, st, 0, lt, gt, eq, t->F);
}

static int bits_of(AvpStr b) {
    if (str_eq(b, "lt")) return 0;
    if (str_eq(b, "gt")) return 1;
    if (str_eq(b, "eq")) return 2;
    return 3; /* so, un */
}

i64 eq_side_crbit(AvpArena *a, AvpStr tok) {
    bool digits = tok.n > 0;
    for (size_t k = 0; k < tok.n; k++)
        if (!is_digit(tok.p[k])) digits = false;
    if (digits) return eq_py_int10(tok);
    /* _CRBIT: ^(?:4\*)?(?:cr(?P<f>\d)\+?)?(?P<b>lt|gt|eq|so|un)$ */
    AvpStr r = tok;
    if (avp_str_startswith(r, AVP_S("4*"))) r = avp_str_slice(r, 2, AVP_END);
    i64 f = 0;
    if (r.n >= 3 && r.p[0] == 'c' && r.p[1] == 'r' && is_digit(r.p[2])) {
        f = r.p[2] - '0';
        r = avp_str_slice(r, 3, AVP_END);
        if (r.n && r.p[0] == '+') r = avp_str_slice(r, 1, AVP_END);
    }
    if (!(str_eq(r, "lt") || str_eq(r, "gt") || str_eq(r, "eq") || str_eq(r, "so") || str_eq(r, "un")))
        eq_raise(EQ_EXC_UNSUPPORTED, "condition bit %s", eq_py_repr(a, tok));
    return 4 * f + bits_of(r);
}

/* ---- calls --------------------------------------------------------------------------------------------------------- */

static int cmp_slot(const void *x, const void *y) {
    const EqSlot *a = x, *b = y;
    if (a->off != b->off) return a->off < b->off ? -1 : 1;
    return (a->w > b->w) - (a->w < b->w);
}

EqNode *eq_side_frame_image(EqSide *s, const EqState *st) {
    EqSlot *xs = AVP_NEWN(s->a, EqSlot, st->nslots + 1);
    if (st->nslots) memcpy(xs, st->slots, st->nslots * sizeof *xs);
    qsort(xs, st->nslots, sizeof *xs, cmp_slot); /* keys are unique */
    EqArg *out = AVP_NEWN(s->a, EqArg, 3 * st->nslots + 1);
    int n = 0;
    for (size_t k = 0; k < st->nslots; k++) {
        if (eq_side_is_save(xs[k].v)) continue;
        out[n++] = eq_ai(xs[k].off);
        out[n++] = eq_ai(xs[k].w);
        out[n++] = eq_an(xs[k].v);
    }
    return eq_node(s->t, EQ_OP_FIMG, out, n, EQ_MASK);
}

bool eq_side_is_save(const EqNode *v) {
    if (v->op == EQ_OP_PSQB || v->op == EQ_OP_PSQB1) v = v->args[0].n;
    if (v->op != EQ_OP_E || v->nargs < 1 || v->args[0].kind != EQ_ARG_STR) return false;
    for (int k = 0; k < EQ_NCALLEE_SAVED; k++)
        if (strcmp(v->args[0].s, EQ_CALLEE_SAVED[k]) == 0) return true;
    return false;
}

i64 eq_side_sp_off(EqSide *s, const EqState *st) {
    EqNode *r1 = regz(st, "r1");
    if (r1 == s->t->sp0) return 0;
    if (r1->op == EQ_OP_LIN && r1->nargs == 3 && r1->args[1].kind == EQ_ARG_NODE && r1->args[1].n == s->t->sp0 &&
        arg_is_int(r1->args[2], 1))
        return eq_s32(r1->args[0].i);
    eq_raise(EQ_EXC_NOTPROVEN, "the stack pointer is not a constant offset from its entry value");
}

static const char *const GPR_NAMES[] = {"r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10"};
static const char *const FPR_NAMES[] = {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8"};

EqNode *eq_side_call(EqSide *s, EqState *st, EqBlockRun *run, EqNode *target, const char *name, const char *desc,
                     bool tail) {
    EqTerms *t = s->t;
    AvpArena *a = s->a;
    EqArity dflt = eq_callee_arity(name), ar = dflt;
    if (s->arity_map) {
        EqArity got;
        if (eq_arity_map_get(s->arity_map, target->id, &got)) ar = got;
    }
    if (ar.kind == EQ_ARITY_STACK) eq_raise(EQ_EXC_NOTPROVEN, "call to %s passes arguments on the stack", name ? name : "None");
    bool has = ar.kind == EQ_ARITY_REGS;
    int ng = has ? ar.ng : 8, nf = has ? ar.nf : 8;
    const char *regs[EQ_NARG_REGS + 16];
    int nr = 0;
    for (int k = 0; k < ng && k < 8; k++) regs[nr++] = GPR_NAMES[k];
    for (int k = 0; k < nf && k < 8; k++) regs[nr++] = FPR_NAMES[k];
    u32 regmask = 0;
    for (int k = 0; k < nr; k++) regmask |= eq_abitz(regs[k]);
    u32 declared = 0;
    if (has) {
        for (int k = 0; k < ng - 1; k++) declared |= eq_abitz(regs[k]);
        for (int k = ng; k < nr; k++) declared |= eq_abitz(regs[k]);
    }
    u32 writes = 0;
    if (s->live && target->op == EQ_OP_C) {
        u32 lv, wr;
        if (eq_live_call(s->live, target->args[0].i, &lv, &wr)) {
            nr = 0;
            for (int k = 0; k < EQ_NARG_REGS; k++) {
                u32 bit = 1u << k;
                if ((lv & bit) && (!has || (regmask & bit))) regs[nr++] = EQ_ARG_REGS[k];
            }
            writes = wr;
        }
    }
    EqNode *vals[EQ_NARG_REGS + 16];
    for (int k = 0; k < nr; k++) vals[k] = regz(st, regs[k]);
    for (int k = 0; k < nr; k++)
        if (vals[k]->sp) s->wants_escape = true;
    EqNode *cr6 = regz(st, "cr6");
    EqArg *flat = AVP_NEWN(a, EqArg, 2 * nr + 2);
    int nf2 = 0;
    for (int k = 0; k < nr; k++) {
        flat[nf2++] = eq_as(regs[k]);
        flat[nf2++] = eq_an(vals[k]);
    }
    flat[nf2++] = eq_as("cr1[eq]");
    flat[nf2++] = eq_an(cr6->op == EQ_OP_BOOL ? cr6 : eq_leaf_d(t, EQ_OP_NOCR6, NULL, 0));
    EqNode *argn = eq_node(t, EQ_OP_ARGS, flat, nf2, EQ_MASK);
    EqNode *parts[4];
    int np = 0;
    parts[np++] = target;
    parts[np++] = argn;
    parts[np++] = eq_side_mem_now(s, st);
    if (s->escaped) parts[np++] = eq_side_frame_image(s, st);
    EqNode *callt = eq_node_n(t, EQ_OP_CALL, parts, np, EQ_MASK);
    add_effect(a, run, EQ_EFF_CALL, callt, desc, declared);
    if (tail) return callt;
    i64 cur = eq_side_sp_off(s, st);
    st->ncall++;
    /* A callee that depends only on what it reads leaves each volatile register as a function of the call and,
     * unless it always writes the register, of the value the register held before the call. */
    static const char *const EXTRA[] = {"r3", "r4", "f1"};
    for (int k = 0; k < EQ_NVOLATILE + 3; k++) {
        const char *r = k < EQ_NVOLATILE ? EQ_VOLATILE[k] : EXTRA[k - EQ_NVOLATILE];
        EqNode *pre = regz(st, r);
        bool ret = strcmp(r, "r3") == 0 || strcmp(r, "r4") == 0 || strcmp(r, "f1") == 0;
        u32 nz = (strncmp(r, "cr", 2) == 0 || strcmp(r, "ca") == 0) ? 1 : EQ_MASK;
        u32 bit = eq_abitz(r);
        EqArg ra[3] = {eq_an(callt), eq_as(r), eq_an(pre)};
        EqNode *v = eq_node(t, ret ? EQ_OP_RET : EQ_OP_CLOB, ra, (bit && (writes & bit)) ? 2 : 3, nz);
        set_regz(a, st, r, v);
    }
    char tag[64];
    snprintf(tag, sizeof tag, "lr@call%lld", (long long)st->ncall);
    set_regz(a, st, "lr", eq_side_fresh(s, tag)); /* the return address differs between the sides */
    st->mem = eq_node1(t, EQ_OP_MEMAFTER, callt, EQ_MASK);
    st->nstores = 0;
    if (s->escaped) {
        size_t n = 0;
        for (size_t k = 0; k < st->nslots; k++)
            if (eq_side_is_save(st->slots[k].v)) st->slots[n++] = st->slots[k];
        st->nslots = n;
        st->frame = callt;
    }
    EqNode *zero = eq_c(t, 0);
    eq_side_write_slot(s, st, cur + 4, 4, zero);
    eq_state_slot_pop(st, cur + 4, 4);
    eq_state_dead_add(a, st, cur + 4, 4);
    return callt;
}

void eq_side_helper(EqSide *s, EqState *st, EqBlockRun *run, AvpStr kind, AvpStr regs, i64 n) {
    EqTerms *t = s->t;
    EqNode *base = regz(st, "r11");
    bool gpr = str_eq(regs, "gpr");
    i64 w = gpr ? 4 : 8;
    for (i64 k = n; k < 32; k++) {
        EqNode *c = eq_c(t, -w * (32 - k));
        EqNode *addr = eq_add(t, base, c);
        char loc[16];
        snprintf(loc, sizeof loc, gpr ? "r%lld" : "f%lld", (long long)k);
        if (str_eq(kind, "save")) eq_side_store(s, st, run, addr, w, regz(st, loc), false);
        else set_regz(s->a, st, loc, eq_side_load(s, st, run, addr, w));
    }
    set_regz(s->a, st, "lr", eq_side_fresh(s, "lr"));
}

/* ---- one block ------------------------------------------------------------------------------------------------- */

EqBlockRun *eq_side_run_block(EqSide *s, int bid, const EqState *state) {
    AVP_ASSERT(bid >= 0 && (size_t)bid < s->cfg->nblocks);
    const WallBlock *blk = &s->cfg->blocks[bid];
    EqState *st = eq_state_copy(s->a, state);
    EqBlockRun *run = AVP_NEW(s->a, EqBlockRun);
    run->kind = blk->kind;
    for (size_t i = blk->start; i < blk->end; i++) {
        const CcRow *row = &s->rows[i];
        WallKind k = wall_row_kind(row);
        switch (k) {
        case WALL_K_NONE: eq_side_exec(s, st, run, i); break;
        case WALL_K_CALL: eq_side_exec_call(s, st, run, i); break;
        case WALL_K_SWITCH: eq_raise(EQ_EXC_UNSUPPORTED, "bctr (jump table)");
        case WALL_K_COND:
        case WALL_K_CONDRET:
        case WALL_K_CONDTAIL:
            run->cond = eq_side_branch_cond(s, st, i);
            if (k == WALL_K_CONDTAIL) {
                EqNode *tg = eq_side_target(s, i);
                run->exit_tail = eq_side_call(s, st, run, tg, eq_side_target_name(s, i), row->text.p, true);
            }
            break;
        case WALL_K_RET: break;
        case WALL_K_TAIL: {
            EqNode *tg = eq_side_target(s, i);
            run->exit_tail = eq_side_call(s, st, run, tg, eq_side_target_name(s, i), row->text.p, true);
            break;
        }
        case WALL_K_GOTO: break;
        default: eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(row->op));
        }
    }
    run->state = st;
    return run;
}

EqNode *eq_side_target(EqSide *s, size_t i) {
    const CcRow *row = &s->rows[i];
    if (wall_is_self_call(row) || (row->has_label && row->bidx == 0)) return eq_leaf_d(s->t, EQ_OP_SELF, NULL, 0);
    return eq_side_addr_of(s, i);
}

const char *eq_side_target_name(const EqSide *s, size_t i) {
    const CcRow *row = &s->rows[i];
    return row->has_ref ? avp_cz(s->a, row->ref.name) : NULL;
}

/* row.op.rstrip("+-") */
static AvpStr strip_hint(AvpStr op) {
    while (op.n && (op.p[op.n - 1] == '+' || op.p[op.n - 1] == '-')) op.n--;
    return op;
}

/* _CC */
static void cc_of(AvpStr cc, int *bit, bool *sense) {
    static const struct {
        const char *n;
        int bit;
        bool sense;
    } T[] = {{"eq", 2, true},  {"ne", 2, false}, {"lt", 0, true},  {"ge", 0, false},
             {"nl", 0, false}, {"gt", 1, true},  {"le", 1, false}, {"ng", 1, false},
             {"so", 3, true},  {"ns", 3, false}, {"un", 3, true},  {"nu", 3, false}};
    for (size_t k = 0; k < sizeof T / sizeof *T; k++)
        if (str_eq(cc, T[k].n)) {
            *bit = T[k].bit;
            *sense = T[k].sense;
            return;
        }
    AVP_ASSERT(0);
}

EqNode *eq_side_branch_cond(EqSide *s, EqState *st, size_t i) {
    EqTerms *t = s->t;
    const CcRow *row = &s->rows[i];
    AvpStr op = strip_hint(row->op);
    EqBranch m;
    if (!eq_branch_match(op, &m) || m.link.p || (m.lr.p && str_eq(m.lr, "ctr")))
        eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(row->op));
    size_t no;
    const char **all = eq_side_operands(s, i, &no);
    const char *ops0 = NULL;
    for (size_t k = 0; k < no; k++)
        if (strncmp(all[k], ".L", 2) != 0 && !strchr(all[k], '@')) {
            ops0 = all[k];
            break;
        }
    EqNode *cond = t->T;
    if (m.ctr.p) {
        EqNode *ctr = regz(st, "ctr");
        EqNode *m1 = eq_c(t, -1);
        set_regz(s->a, st, "ctr", eq_add(t, ctr, m1));
        EqNode *ctr2 = regz(st, "ctr");
        EqNode *z0 = eq_c(t, 0);
        EqNode *z = eq_eq(t, ctr2, z0);
        cond = str_eq(m.ctr, "dz") ? z : eq_bnot(t, z);
    }
    if (m.cc.p) {
        i64 f = 0;
        if (ops0 && strncmp(ops0, "cr", 2) == 0) {
            AvpStr d = avp_str(ops0 + 2);
            bool digits = d.n > 0;
            for (size_t k = 0; k < d.n; k++)
                if (!is_digit(d.p[k])) digits = false;
            if (digits) f = eq_py_int10(d);
        }
        int bit;
        bool sense;
        cc_of(m.cc, &bit, &sense);
        EqNode *b = get_cr(st, 4 * f + bit);
        EqNode *x = sense ? b : eq_bnot(t, b);
        cond = eq_band(t, cond, x);
    }
    if (!m.ctr.p && !m.cc.p) eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(row->op));
    return cond;
}

void eq_side_exec_call(EqSide *s, EqState *st, EqBlockRun *run, size_t i) {
    const CcRow *row = &s->rows[i];
    AvpStr op = strip_hint(row->op);
    EqTerms *t = s->t;
    if (str_eq(op, "bl")) {
        const char *name = eq_side_target_name(s, i);
        AvpStr kind, cls;
        i64 n;
        if (eq_helper_match(avp_str(name ? name : ""), &kind, &cls, &n)) {
            eq_side_helper(s, st, run, kind, cls, n);
            return;
        }
        EqNode *tg = eq_side_target(s, i);
        eq_side_call(s, st, run, tg, name, row->text.p, false);
    } else if (str_eq(op, "bctrl")) {
        EqNode *ind = eq_node1(t, EQ_OP_IND, regz(st, "ctr"), EQ_MASK);
        eq_side_call(s, st, run, ind, NULL, row->text.p, false);
    } else if (str_eq(op, "blrl")) {
        EqNode *ind = eq_node1(t, EQ_OP_IND, regz(st, "lr"), EQ_MASK);
        eq_side_call(s, st, run, ind, NULL, row->text.p, false);
    } else {
        eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(row->op));
    }
}

/* ---- straight-line instructions ------------------------------------------------------------------------------------ */

typedef struct {
    EqSide *s;
    EqState *st;
    EqBlockRun *run;
    size_t i;
    const char **o;
    size_t no;
    EqTerms *t;
    AvpArena *a;
} X;

static AvpStr O(const X *x, size_t k) {
    if (k >= x->no) raise_index();
    return avp_str(x->o[k]);
}
static EqNode *R(const X *x, size_t k) { return eq_state_reg_req(x->st, O(x, k)); }
static EqNode *setr(X *x, size_t k, EqNode *v) {
    eq_state_set_reg(x->a, x->st, O(x, k), v);
    return v;
}
static i64 I(const X *x, size_t k) { return eq_py_int0(O(x, k)); }

static EqNode *sorted2(EqNode **out, EqNode *a, EqNode *b) {
    if (b->id < a->id) {
        out[0] = b;
        out[1] = a;
    } else {
        out[0] = a;
        out[1] = b;
    }
    return out[0];
}

/* node(op, sorted((a, b), key=id) + [ca], 1) (ca NULL: no third argument) */
static EqNode *carry(X *x, int op, EqNode *a, EqNode *b, EqNode *ca) {
    EqNode *k[3];
    sorted2(k, a, b);
    k[2] = ca;
    return eq_node_n(x->t, op, k, ca ? 3 : 2, 1);
}

static AVP_NORETURN void unpack_error(size_t got, size_t want) {
    if (got < want) eq_raise(SHSM_EXC_VALUE, "not enough values to unpack (expected %zu, got %zu)", want, got);
    eq_raise(SHSM_EXC_VALUE, "too many values to unpack (expected %zu, got %zu)", want, got);
}

/* mask_of with Python's shift semantics (a negative count raises ValueError; large counts shift everything out). */
static u32 py_mask_of(i64 mb, i64 me) {
    if (mb < 0 || 31 - me < 0) eq_raise(SHSM_EXC_VALUE, "negative shift count");
    u64 m = EQ_MASK;
    u64 r = mb >= 64 ? 0 : m >> mb;
    u64 l = (31 - me) >= 32 ? 0 : (m << (31 - me)) & m;
    return (u32)((mb <= me ? (r & l) : (r | l)) & m);
}

static i64 pymod(i64 a, i64 b) {
    i64 r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
}

static const char *const L_ADDI[] = {"li", "addi", "subi", "addic", "subic", NULL};
static const char *const L_LIS[] = {"lis", "addis", "subis", NULL};
static const char *const L_LOGI[] = {"ori", "oris", "xori", "xoris", "andi", "andis", NULL};
static const char *const L_UN[] = {"mr", "not", "neg", "extsb", "extsh", "cntlzw", NULL};
static const char *const L_BIN[] = {"add",   "subf",  "and",    "or",     "xor",  "andc",  "orc", "nor", "nand", "eqv",
                                    "mullw", "mulhw", "mulhwu", "divw",   "divwu", "slw",  "srw", "sraw", NULL};
static const char *const L_CARRY[] = {"addc", "adde", "addze", "addme", "subfc", "subfe", "subfze", "subfme",
                                      "subfic", NULL};
static const char *const L_RLW[] = {"rlwinm", "slwi",   "srwi",   "clrlwi", "clrrwi", "clrlslwi",
                                    "extlwi", "extrwi", "rotlwi", "rotrwi", "rlwnm",  "rotlw", NULL};
static const char *const L_RLWIMI[] = {"rlwimi", "inslwi", "insrwi", NULL};
static const char *const L_CMP[] = {"cmpw", "cmpwi", "cmplw", "cmplwi", NULL};
static const char *const L_CROP[] = {"cror",   "crand",  "crxor",  "crnor", "creqv",
                                     "crandc", "crorc",  "crnand", "crnot", "crmove", NULL};
static const char *const L_FBIN_COMM[] = {"fadd", "fadds", "fmul", "fmuls", NULL};
static const char *const L_FBIN_NC[] = {"fsub", "fsubs", "fdiv", "fdivs", NULL};
static const char *const L_FMA[] = {"fmadd", "fmadds", "fmsub", "fmsubs", "fnmadd", "fnmadds", "fnmsub", "fnmsubs", NULL};
static const char *const L_FUN[] = {"fres", "frsqrte", "fctiwz", "fctiw", NULL};

static bool memop(X *x, const char *base);
static void psq(X *x, const char *base);

/* the immediates o[from:] as ints */
static size_t ints_from(X *x, size_t from, i64 *out, size_t cap) {
    size_t n = 0;
    for (size_t k = from; k < x->no; k++) {
        i64 v = eq_py_int0(avp_str(x->o[k]));
        if (n < cap) out[n] = v;
        n++;
    }
    return n;
}

void eq_side_exec(EqSide *s, EqState *st, EqBlockRun *run, size_t i) {
    const CcRow *row = &s->rows[i];
    X xx = {s, st, run, i, NULL, 0, s->t, s->a};
    X *x = &xx;
    xx.o = eq_side_operands(s, i, &xx.no);
    EqTerms *t = s->t;
    AvpStr op = row->op;
    bool rec = avp_str_endswith(op, AVP_S("."));
    const char *base = avp_cz(s->a, rec ? avp_strn(op.p, op.n - 1) : op);
    EqNode *res = NULL;

    if (cz_in(base, L_ADDI)) {
        EqImm im;
        EqNode *src, *v;
        if (strcmp(base, "li") == 0) {
            im = eq_side_imm(s, i, O(x, 1), true);
            src = eq_c(t, 0);
        } else {
            im = eq_side_imm(s, i, O(x, 2), true);
            src = (str_eq(O(x, 1), "r0") && (strcmp(base, "addi") == 0 || strcmp(base, "subi") == 0)) ? eq_c(t, 0)
                                                                                                         : R(x, 1);
        }
        if (!im.is_n && strcmp(im.kind, "sda21") == 0) {
            v = im.a;
        } else {
            EqNode *n = eq_side_imm_node(s, im, true);
            if (strcmp(base, "subi") == 0 || strcmp(base, "subic") == 0) n = eq_mulc(t, n, -1);
            v = eq_add(t, src, n);
        }
        if (strcmp(base, "addic") == 0 || strcmp(base, "subic") == 0) {
            EqNode *n2 = eq_side_imm_node(s, im, true);
            if (strcmp(base, "subic") == 0) n2 = eq_mulc(t, n2, -1);
            set_regz(s->a, st, "ca", carry(x, EQ_OP_CADD, src, n2, NULL));
        }
        res = setr(x, 0, v);
    } else if (cz_in(base, L_LIS)) {
        EqImm im = eq_side_imm(s, i, O(x, x->no ? x->no - 1 : 0), false);
        EqNode *n = eq_side_hi_node(s, im);
        if (strcmp(base, "subis") == 0) n = eq_mulc(t, n, -1);
        EqNode *src = (strcmp(base, "lis") == 0 || str_eq(O(x, 1), "r0")) ? eq_c(t, 0) : R(x, 1);
        res = setr(x, 0, eq_add(t, src, n));
    } else if (cz_in(base, L_LOGI)) {
        EqImm im = eq_side_imm(s, i, O(x, 2), false);
        size_t bl = strlen(base);
        EqNode *n = base[bl - 1] != 's' ? eq_side_imm_node(s, im, false) : eq_side_hi_node(s, im);
        EqNode *a = R(x, 1), *v;
        if (base[0] == 'o') v = eq_or(t, a, n);
        else if (base[0] == 'x') v = eq_xor(t, a, n);
        else v = eq_and(t, a, n);
        res = setr(x, 0, v);
    } else if (cz_in(base, L_UN)) {
        EqNode *a = R(x, 1), *v;
        if (strcmp(base, "mr") == 0) v = a;
        else if (strcmp(base, "not") == 0) v = eq_not(t, a);
        else if (strcmp(base, "neg") == 0) v = eq_mulc(t, a, -1);
        else if (strcmp(base, "extsb") == 0) v = eq_sext(t, a, 8);
        else if (strcmp(base, "extsh") == 0) v = eq_sext(t, a, 16);
        else if (a->op == EQ_OP_C) {
            u64 c = (u64)a->args[0].i;
            int bl = 0;
            while (c) bl++, c >>= 1;
            v = eq_c(t, 32 - bl);
        } else
            v = eq_op(t, EQ_OP_CNTLZW, &a, 1, false, 0x3F);
        res = setr(x, 0, v);
    } else if (cz_in(base, L_BIN)) {
        EqNode *a = R(x, 1), *b = R(x, 2), *v;
        if (strcmp(base, "add") == 0) v = eq_add(t, a, b);
        else if (strcmp(base, "subf") == 0) v = eq_sub(t, b, a);
        else if (strcmp(base, "and") == 0) v = eq_and(t, a, b);
        else if (strcmp(base, "or") == 0) v = eq_or(t, a, b);
        else if (strcmp(base, "xor") == 0) v = eq_xor(t, a, b);
        else if (strcmp(base, "andc") == 0) v = eq_and(t, a, eq_not(t, b));
        else if (strcmp(base, "orc") == 0) v = eq_or(t, a, eq_not(t, b));
        else if (strcmp(base, "nor") == 0) v = eq_not(t, eq_or(t, a, b));
        else if (strcmp(base, "nand") == 0) v = eq_not(t, eq_and(t, a, b));
        else if (strcmp(base, "eqv") == 0) v = eq_not(t, eq_xor(t, a, b));
        else if (strcmp(base, "mullw") == 0) v = eq_mul(t, a, b);
        else if (strcmp(base, "mulhw") == 0 || strcmp(base, "mulhwu") == 0) {
            EqNode *ab[2] = {a, b};
            v = eq_op(t, eq_op_id(base), ab, 2, true, EQ_MASK);
        } else if (strcmp(base, "slw") == 0 || strcmp(base, "srw") == 0 || strcmp(base, "sraw") == 0) {
            if (b->op == EQ_OP_C) {
                i64 n = b->args[0].i & 63, n31 = n < 31 ? n : 31;
                if (strcmp(base, "slw") == 0) v = n < 32 ? eq_shl(t, a, n) : eq_c(t, 0);
                else if (strcmp(base, "srw") == 0) v = eq_shr(t, a, n);
                else v = eq_sra(t, a, n31);
                if (strcmp(base, "sraw") == 0) {
                    EqNode *ca;
                    if (n) {
                        EqArg ca_args[2] = {eq_an(a), eq_ai(n31)};
                        ca = eq_node(t, EQ_OP_CASRA, ca_args, 2, 1);
                    } else
                        ca = eq_c(t, 0);
                    set_regz(s->a, st, "ca", ca);
                }
            } else {
                EqNode *bm = eq_and_mask(t, b, 63);
                EqNode *ab[2] = {a, bm};
                v = eq_op(t, eq_op_id(base), ab, 2, false, EQ_MASK);
                if (strcmp(base, "sraw") == 0) {
                    EqNode *bm2 = eq_and_mask(t, b, 63);
                    EqNode *k2[2] = {a, bm2};
                    set_regz(s->a, st, "ca", eq_node_n(t, EQ_OP_CASRAW, k2, 2, 1));
                }
            }
        } else {
            if (b->op == EQ_OP_C && b->args[0].i == 1) v = a;
            else {
                EqNode *ab[2] = {a, b};
                v = eq_op(t, eq_op_id(base), ab, 2, false, EQ_MASK);
            }
        }
        res = setr(x, 0, v);
    } else if (cz_in(base, L_CARRY)) {
        EqNode *a = R(x, 1);
        EqNode *ca = regz(st, "ca");
        EqNode *v, *nca;
        if (strcmp(base, "addc") == 0) {
            EqNode *b = R(x, 2);
            v = eq_add(t, a, b);
            nca = carry(x, EQ_OP_CADD, a, b, NULL);
        } else if (strcmp(base, "adde") == 0) {
            EqNode *b = R(x, 2);
            v = eq_add3(t, a, b, ca);
            nca = carry(x, EQ_OP_CADDE, a, b, ca);
        } else if (strcmp(base, "addze") == 0) {
            v = eq_add(t, a, ca);
            EqNode *z = eq_c(t, 0);
            nca = carry(x, EQ_OP_CADDE, a, z, ca);
        } else if (strcmp(base, "addme") == 0) {
            EqNode *m1 = eq_c(t, -1);
            v = eq_add3(t, a, ca, m1);
            EqNode *mk = eq_c(t, EQ_MASK);
            nca = carry(x, EQ_OP_CADDE, a, mk, ca);
        } else if (strcmp(base, "subfc") == 0 || strcmp(base, "subfic") == 0) {
            EqNode *b;
            if (strcmp(base, "subfc") == 0) b = R(x, 2);
            else {
                EqImm im = eq_side_imm(s, i, O(x, 2), true);
                if (!im.is_n) eq_raise(SHSM_EXC_TYPE, "unsupported operand type(s) for &=: 'Node' and 'int'");
                b = eq_c(t, im.n);
            }
            v = eq_sub(t, b, a);
            EqNode *l = eq_lt(t, false, b, a);
            EqNode *nb = eq_bnot(t, l);
            nca = eq_b2i(t, nb);
        } else if (strcmp(base, "subfe") == 0) {
            EqNode *b = R(x, 2);
            EqNode *na = eq_not(t, a);
            v = eq_add3(t, na, b, ca);
            nca = carry(x, EQ_OP_CADDE, na, b, ca);
        } else if (strcmp(base, "subfze") == 0) {
            EqNode *na = eq_not(t, a);
            v = eq_add(t, na, ca);
            EqNode *z = eq_c(t, 0);
            nca = carry(x, EQ_OP_CADDE, na, z, ca);
        } else {
            EqNode *na = eq_not(t, a);
            EqNode *m1 = eq_c(t, -1);
            v = eq_add3(t, na, ca, m1);
            EqNode *mk = eq_c(t, EQ_MASK);
            nca = carry(x, EQ_OP_CADDE, na, mk, ca);
        }
        set_regz(s->a, st, "ca", nca);
        res = setr(x, 0, v);
    } else if (strcmp(base, "mulli") == 0) {
        EqNode *a = R(x, 1);
        res = setr(x, 0, eq_mulc(t, a, I(x, 2)));
    } else if (strcmp(base, "srawi") == 0) {
        EqNode *a = R(x, 1);
        i64 n = I(x, 2) & 31;
        EqNode *ca;
        if (n && (a->nz & 0x80000000u)) {
            EqArg ca_args[2] = {eq_an(a), eq_ai(n)};
            ca = eq_node(t, EQ_OP_CASRA, ca_args, 2, 1);
        } else
            ca = eq_c(t, 0);
        set_regz(s->a, st, "ca", ca);
        res = setr(x, 0, eq_sra(t, a, n));
    } else if (cz_in(base, L_RLW)) {
        EqNode *a = R(x, 1), *v;
        if (strcmp(base, "rlwnm") == 0 || strcmp(base, "rotlw") == 0) {
            EqNode *n = R(x, 2);
            i64 mb = 0, me = 31;
            if (strcmp(base, "rlwnm") == 0) {
                mb = I(x, 3);
                me = I(x, 4);
            }
            if (n->op == EQ_OP_C) v = eq_rot_mask(t, a, n->args[0].i & 31, py_mask_of(mb, me));
            else {
                EqNode *nm = eq_and_mask(t, n, 31);
                EqNode *k2[2] = {a, nm};
                EqNode *r = eq_op(t, EQ_OP_ROTLV, k2, 2, false, EQ_MASK);
                v = eq_and_mask(t, r, py_mask_of(mb, me));
            }
        } else {
            i64 av[8];
            size_t na = ints_from(x, 2, av, 8);
            i64 sh, mb, me;
            if (strcmp(base, "rlwinm") == 0) {
                if (na != 3) unpack_error(na, 3);
                sh = av[0], mb = av[1], me = av[2];
            } else {
                if (na < 1) raise_index();
                if (strcmp(base, "slwi") == 0) sh = av[0], mb = 0, me = 31 - av[0];
                else if (strcmp(base, "srwi") == 0) sh = 32 - av[0], mb = av[0], me = 31;
                else if (strcmp(base, "clrlwi") == 0) sh = 0, mb = av[0], me = 31;
                else if (strcmp(base, "clrrwi") == 0) sh = 0, mb = 0, me = 31 - av[0];
                else if (strcmp(base, "rotlwi") == 0) sh = av[0], mb = 0, me = 31;
                else if (strcmp(base, "rotrwi") == 0) sh = 32 - av[0], mb = 0, me = 31;
                else {
                    if (na < 2) raise_index();
                    if (strcmp(base, "clrlslwi") == 0) sh = av[1], mb = av[0] - av[1], me = 31 - av[1];
                    else if (strcmp(base, "extlwi") == 0) sh = av[1], mb = 0, me = av[0] - 1;
                    else sh = av[1] + av[0], mb = 32 - av[0], me = 31; /* extrwi */
                }
            }
            v = eq_rot_mask(t, a, pymod(sh, 32), py_mask_of(mb, me));
        }
        res = setr(x, 0, v);
    } else if (cz_in(base, L_RLWIMI)) {
        i64 av[8];
        size_t na = ints_from(x, 2, av, 8);
        i64 sh, mb, me;
        if (strcmp(base, "rlwimi") == 0) {
            if (na != 3) unpack_error(na, 3);
            sh = av[0], mb = av[1], me = av[2];
        } else {
            if (na < 2) raise_index();
            mb = av[1];
            me = av[1] + av[0] - 1;
            sh = strcmp(base, "inslwi") == 0 ? pymod(32 - av[1], 32) : pymod(32 - (av[1] + av[0]), 32);
        }
        u32 m = py_mask_of(mb, me);
        EqNode *r1 = R(x, 1);
        EqNode *rot = eq_rot_mask(t, r1, sh, m);
        EqNode *r0 = R(x, 0);
        EqNode *keep = eq_and_mask(t, r0, ~m & EQ_MASK);
        res = setr(x, 0, eq_or(t, rot, keep));
    } else if (cz_in(base, L_CMP)) {
        i64 f = 0;
        size_t sh = 0;
        if (strncmp(O(x, 0).p, "cr", 2) == 0) {
            f = eq_py_int10(avp_str(O(x, 0).p + 2));
            sh = 1;
        }
        EqNode *a = R(x, sh + 0), *b;
        bool sg = strcmp(base, "cmpw") == 0 || strcmp(base, "cmpwi") == 0;
        if (base[strlen(base) - 1] == 'i') {
            EqImm im = eq_side_imm(s, i, O(x, sh + 1), sg);
            b = eq_side_imm_node(s, im, sg);
        } else
            b = R(x, sh + 1);
        EqNode *lt = eq_lt(t, sg, a, b);
        EqNode *gt = eq_lt(t, sg, b, a);
        EqNode *eq = eq_eq(t, a, b);
        eq_side_set_cmp(s, st, f, lt, gt, eq, t->F);
        return;
    } else if (strcmp(base, "fcmpu") == 0 || strcmp(base, "fcmpo") == 0) {
        i64 f = eq_py_int10(avp_str(O(x, 0).p + (O(x, 0).n >= 2 ? 2 : O(x, 0).n)));
        EqNode *a = R(x, 1), *b = R(x, 2);
        EqNode *ab[2];
        sorted2(ab, a, b);
        EqNode *k1[2] = {a, b}, *k2[2] = {b, a};
        EqNode *lt = eq_node_n(t, EQ_OP_FLT, k1, 2, 1);
        EqNode *gt = eq_node_n(t, EQ_OP_FLT, k2, 2, 1);
        EqNode *eq = eq_node_n(t, EQ_OP_FEQ, ab, 2, 1);
        EqNode *un = eq_node_n(t, EQ_OP_FUN, ab, 2, 1);
        eq_side_set_cmp(s, st, f, lt, gt, eq, un);
        return;
    } else if (strcmp(base, "crclr") == 0 || strcmp(base, "crset") == 0) {
        EqNode *v = strcmp(base, "crclr") == 0 ? t->F : t->T;
        set_cr(s, st, eq_side_crbit(s->a, O(x, 0)), v);
        return;
    } else if (cz_in(base, L_CROP)) {
        EqNode *bits[8];
        size_t nb = 0;
        for (size_t k = 1; k < x->no; k++) {
            EqNode *b = get_cr(st, eq_side_crbit(s->a, avp_str(x->o[k])));
            if (nb < 8) bits[nb] = b;
            nb++;
        }
        EqNode *v;
        if (strcmp(base, "crnot") == 0) {
            if (nb < 1) raise_index();
            v = eq_bnot(t, bits[0]);
        } else if (strcmp(base, "crmove") == 0) {
            if (nb < 1) raise_index();
            v = bits[0];
        } else {
            if (nb != 2) unpack_error(nb, 2);
            EqNode *a = bits[0], *b = bits[1];
            if (strcmp(base, "cror") == 0) v = eq_bor(t, a, b);
            else if (strcmp(base, "crand") == 0) v = eq_band(t, a, b);
            else if (strcmp(base, "crxor") == 0) v = eq_bxor(t, a, b);
            else if (strcmp(base, "crnor") == 0) v = eq_bnot(t, eq_bor(t, a, b));
            else if (strcmp(base, "creqv") == 0) v = eq_bnot(t, eq_bxor(t, a, b));
            else if (strcmp(base, "crandc") == 0) v = eq_band(t, a, eq_bnot(t, b));
            else if (strcmp(base, "crorc") == 0) v = eq_bor(t, a, eq_bnot(t, b));
            else v = eq_bnot(t, eq_band(t, a, b)); /* crnand */
        }
        set_cr(s, st, eq_side_crbit(s->a, O(x, 0)), v);
        return;
    } else if (strcmp(base, "mfcr") == 0) {
        EqNode *crs[32];
        for (int k = 0; k < 32; k++) crs[k] = get_cr(st, k);
        setr(x, 0, eq_node_n(t, EQ_OP_CRW, crs, 32, EQ_MASK));
        return;
    } else if (strcmp(base, "mtcrf") == 0) {
        i64 fxm = I(x, 0);
        EqNode *v = R(x, 1);
        for (int f = 0; f < 8; f++)
            if ((fxm >> (7 - f)) & 1)
                for (int k = 4 * f; k < 4 * f + 4; k++) set_cr(s, st, k, eq_bit_of(t, v, k));
        return;
    } else if (strcmp(base, "mflr") == 0 || strcmp(base, "mfctr") == 0) {
        setr(x, 0, regz(st, base + 2));
        return;
    } else if (strcmp(base, "mtlr") == 0 || strcmp(base, "mtctr") == 0) {
        set_regz(s->a, st, base + 2, R(x, 0));
        return;
    } else if (strcmp(base, "nop") == 0) {
        return;
    } else if (strcmp(base, "lmw") == 0 || strcmp(base, "stmw") == 0) {
        const char *breg;
        EqNode *addr = eq_side_ea(s, st, i, O(x, 1), &breg);
        i64 n = eq_py_int10(avp_str(O(x, 0).p + (O(x, 0).n ? 1 : 0)));
        for (i64 k = n; k < 32; k++) {
            EqNode *c = eq_c(t, 4 * (k - n));
            EqNode *ad = eq_add(t, addr, c);
            char loc[24];
            snprintf(loc, sizeof loc, "r%lld", (long long)k);
            if (strcmp(base, "lmw") == 0) set_regz(s->a, st, loc, eq_side_load(s, st, run, ad, 4));
            else eq_side_store(s, st, run, ad, 4, regz(st, loc), false);
        }
        return;
    } else if (strncmp(base, "psq_", 4) == 0) {
        psq(x, base);
        return;
    } else if (memop(x, base)) {
        if (rec) eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(op));
        return;
    } else if (cz_in(base, L_FBIN_COMM) || cz_in(base, L_FBIN_NC)) {
        EqNode *ab[2] = {R(x, 1), NULL};
        ab[1] = R(x, 2);
        EqNode *v = eq_op(t, eq_op_id(base), ab, 2, cz_in(base, L_FBIN_COMM), EQ_MASK);
        if (rec) eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(op));
        setr(x, 0, v);
        return;
    } else if (cz_in(base, L_FMA)) {
        EqNode *a = R(x, 1), *c = R(x, 2), *b = R(x, 3);
        EqNode *k[3];
        sorted2(k, a, c);
        k[2] = b;
        setr(x, 0, eq_node_n(t, eq_op_id(base), k, 3, EQ_MASK));
        return;
    } else if (strcmp(base, "fmr") == 0 || strcmp(base, "ps_mr") == 0) {
        setr(x, 0, R(x, 1));
        return;
    } else if (strcmp(base, "fneg") == 0 || strcmp(base, "fabs") == 0 || strcmp(base, "fnabs") == 0) {
        EqNode *a = R(x, 1), *v;
        if (strcmp(base, "fneg") == 0 && a->op == EQ_OP_FNEG) v = a->args[0].n;
        else if (strcmp(base, "fabs") == 0 && (a->op == EQ_OP_FNEG || a->op == EQ_OP_FABS)) {
            EqNode *in = a->args[0].n;
            v = eq_op(t, EQ_OP_FABS, &in, 1, false, EQ_MASK);
        } else
            v = eq_op(t, eq_op_id(base), &a, 1, false, EQ_MASK);
        setr(x, 0, v);
        return;
    } else if (strcmp(base, "frsp") == 0) {
        EqNode *a = R(x, 1);
        setr(x, 0, eq_is_single_op(a->op) ? a : eq_op(t, EQ_OP_FRSP, &a, 1, false, EQ_MASK));
        return;
    } else if (cz_in(base, L_FUN)) {
        EqNode *a = R(x, 1);
        setr(x, 0, eq_op(t, eq_op_id(base), &a, 1, false, EQ_MASK));
        return;
    } else if (strcmp(base, "fsel") == 0) {
        EqNode *k[3];
        k[0] = R(x, 1);
        k[1] = R(x, 2);
        k[2] = R(x, 3);
        setr(x, 0, eq_node_n(t, EQ_OP_FSEL, k, 3, EQ_MASK));
        return;
    } else {
        eq_raise(EQ_EXC_UNSUPPORTED, "%.*s", AVP_SF(op));
    }
    if (rec) eq_side_set_cr0(s, st, res);
}

/* re.match(r"^(l[bhw]z|lha|lf[sd]|st[bhw]|stf[sd]|stfiw)(u?)(x?)$", base) */
static bool memop_match(const char *b, char *core, bool *upd, bool *idx) {
    static const char *const CORES[] = {"lbz", "lhz", "lwz", "lha", "lfs", "lfd", "stb", "sth",
                                        "stw", "stfs", "stfd", "stfiw", NULL};
    for (const char *const *c = CORES; *c; c++) {
        size_t n = strlen(*c);
        if (strncmp(b, *c, n) != 0) continue;
        const char *r = b + n;
        bool u = false, xx = false;
        if (*r == 'u') u = true, r++;
        if (*r == 'x') xx = true, r++;
        if (*r) continue;
        strcpy(core, *c);
        *upd = u;
        *idx = xx;
        return true;
    }
    return false;
}

static bool memop(X *x, const char *base) {
    EqSide *s = x->s;
    EqTerms *t = x->t;
    EqState *st = x->st;
    char core[8];
    bool upd, idx;
    if (!memop_match(base, core, &upd, &idx)) return false;
    if (strcmp(core, "stfiw") == 0 && !idx) return false;
    EqNode *addr;
    const char *basereg;
    if (idx) {
        EqNode *ra = (str_eq(O(x, 1), "r0") && !upd) ? eq_c(t, 0) : R(x, 1);
        EqNode *rb = R(x, 2);
        addr = eq_add(t, ra, rb);
        basereg = x->o[1];
    } else {
        addr = eq_side_ea(s, st, x->i, O(x, 1), &basereg);
        if (upd && strcmp(basereg, "r0") == 0) eq_raise(EQ_EXC_UNSUPPORTED, "%s", base);
    }
    static const struct {
        const char *n;
        i64 w;
        char k;
    } LOADS[] = {{"lbz", 1, 'z'}, {"lhz", 2, 'z'}, {"lha", 2, 'a'}, {"lwz", 4, 'z'}, {"lfs", 4, 's'}, {"lfd", 8, 'd'}},
      STORES[] = {{"stb", 1, 'i'}, {"sth", 2, 'i'}, {"stw", 4, 'i'}, {"stfs", 4, 's'}, {"stfd", 8, 'd'}};
    int li = -1;
    for (int k = 0; k < 6; k++)
        if (strcmp(core, LOADS[k].n) == 0) li = k;
    if (li >= 0) {
        i64 w = LOADS[li].w;
        char kind = LOADS[li].k;
        EqNode *v = eq_side_load(s, st, x->run, addr, w);
        if (kind == 'a') v = eq_sext(t, v, 16);
        else if (kind == 's') {
            if (v->op == EQ_OP_D2S) {
                EqNode *y = v->args[0].n;
                v = eq_is_single_op(y->op) ? y : eq_op(t, EQ_OP_FRSP, &y, 1, false, EQ_MASK);
            } else
                v = eq_op(t, EQ_OP_S2D, &v, 1, false, EQ_MASK);
        }
        eq_state_set_reg(x->a, st, O(x, 0), v);
    } else {
        i64 w;
        EqNode *val;
        if (strcmp(core, "stfiw") == 0) {
            w = 4;
            val = eq_side_ext(s, R(x, 0), 8, 4, 4);
        } else {
            int si = 0;
            for (int k = 0; k < 5; k++)
                if (strcmp(core, STORES[k].n) == 0) si = k;
            w = STORES[si].w;
            char kind = STORES[si].k;
            val = R(x, 0);
            if (kind == 'i' && w < 4) val = eq_and_mask(t, val, eq_wmask(w));
            else if (kind == 's') val = val->op == EQ_OP_S2D ? val->args[0].n : eq_op(t, EQ_OP_D2S, &val, 1, false, EQ_MASK);
        }
        bool backchain = strcmp(core, "stw") == 0 && upd && str_eq(O(x, 0), "r1") && strcmp(basereg, "r1") == 0;
        eq_side_store(s, st, x->run, addr, w, val, backchain);
    }
    if (upd) eq_state_set_reg(x->a, st, avp_str(basereg), addr);
    return true;
}

static void psq(X *x, const char *base) {
    EqSide *s = x->s;
    EqTerms *t = x->t;
    EqState *st = x->st;
    /* re.match(r"^psq_(l|st)(u?)(x?)$", base) */
    const char *r = base + 4;
    bool load;
    if (r[0] == 'l') load = true, r++;
    else if (r[0] == 's' && r[1] == 't') load = false, r += 2;
    else eq_raise(EQ_EXC_UNSUPPORTED, "%s", base);
    bool upd = false, idx = false;
    if (*r == 'u') upd = true, r++;
    if (*r == 'x') idx = true, r++;
    if (*r) eq_raise(EQ_EXC_UNSUPPORTED, "%s", base);
    EqNode *addr;
    const char *basereg;
    AvpStr wbit, q;
    if (idx) {
        EqNode *ra = str_eq(O(x, 1), "r0") ? eq_c(t, 0) : R(x, 1);
        EqNode *rb = R(x, 2);
        addr = eq_add(t, ra, rb);
        basereg = x->o[1];
        wbit = O(x, 3);
        q = O(x, 4);
    } else {
        addr = eq_side_ea(s, st, x->i, O(x, 1), &basereg);
        wbit = O(x, 2);
        q = O(x, 3);
    }
    if (!str_eq(q, "qr0")) eq_raise(EQ_EXC_UNSUPPORTED, "%s with %.*s", base, AVP_SF(q));
    bool one = eq_py_int0(wbit) == 1;
    i64 w = one ? 4 : 8;
    int tag = one ? EQ_OP_PSQB1 : EQ_OP_PSQB;
    int pop = one ? EQ_OP_PSQ1 : EQ_OP_PSQ;
    if (load) {
        EqNode *v = eq_side_load(s, st, x->run, addr, w);
        EqNode *res = v->op == tag ? v->args[0].n : eq_op(t, pop, &v, 1, false, EQ_MASK);
        eq_state_set_reg(x->a, st, O(x, 0), res);
    } else {
        EqNode *y = R(x, 0);
        EqNode *val = y->op == pop ? y->args[0].n : eq_op(t, tag, &y, 1, false, EQ_MASK);
        eq_side_store(s, st, x->run, addr, w, val, false);
    }
    if (upd) eq_state_set_reg(x->a, st, avp_str(basereg), addr);
}
