/* eq_terms: Terms, the hash-consing table and simplifier of tools/eqcheck.py (L63-530). See eq_terms.h.
 *
 * Every function creates nodes in the Python's order: where the Python evaluates several constructor calls in
 * one expression (left to right), the C sequences them through locals, since C leaves argument order unspecified. */
#include "eq_terms.h"

#include "avp/hash.h"
#include "avp/map.h"
#include "avp/pool.h"

#include <stdlib.h>

const char *const EQ_ASSUMPTIONS[EQ_NASSUMPTIONS] = {
    "callees follow the EABI (preserve r1, r2, r13-r31, f14-f31, cr2-cr4; results in r3/r4/f1) and depend only "
    "on their argument registers and memory",
    "callee inputs: the volatile registers the retail callee may read (liveness over its listing), else the "
    "mangled name's count (plus one GPR for this or a hidden result pointer), else r3-r10 and f1-f8; no stack "
    "arguments for unmangled callees; cr1[eq] is an input only when a side sets it",
    "the stack frame is disjoint from globals and is reached by no pointer except frame addresses passed to "
    "callees; callees write the frame only through those and the LR-save word",
    "XER[SO] is clear; FPSCR is not observed (fcmpo = fcmpu); float add/mul are commutative (NaN payloads "
    "ignored); GQR0 is zero for psq_l/psq_st",
    "memory is ordinary except constant addresses at or above 0xC0000000 (hardware), whose accesses stay in "
    "order",
    "compiler-local data sits at its splits.txt layout address",
};

const char *const EQ_CALLEE_SAVED[EQ_NCALLEE_SAVED] = {
    "r1",  "r2",  "r13", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31", "f14", "f15", "f16", "f17", "f18",
    "f19", "f20", "f21", "f22", "f23", "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
    "cr8", "cr9", "cr10", "cr11", "cr12", "cr13", "cr14", "cr15", "cr16", "cr17", "cr18", "cr19", "lr",
};

const char *const EQ_REG_LOCS[EQ_NREG_LOCS] = {
    "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",   "r8",   "r9",   "r10",  "r11",  "r12",
    "r13",  "r14",  "r15",  "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",  "r24",  "r25",
    "r26",  "r27",  "r28",  "r29",  "r30",  "r31",  "f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",
    "f7",   "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15",  "f16",  "f17",  "f18",  "f19",
    "f20",  "f21",  "f22",  "f23",  "f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31",  "cr0",
    "cr1",  "cr2",  "cr3",  "cr4",  "cr5",  "cr6",  "cr7",  "cr8",  "cr9",  "cr10", "cr11", "cr12", "cr13",
    "cr14", "cr15", "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23", "cr24", "cr25", "cr26",
    "cr27", "cr28", "cr29", "cr30", "cr31", "ca",   "ctr",  "lr",
};

const char *const EQ_VOLATILE[EQ_NVOLATILE] = {
    "r0",   "r5",   "r6",   "r7",   "r8",   "r9",   "r10",  "r11",  "r12",  "f0",   "f2",   "f3",
    "f4",   "f5",   "f6",   "f7",   "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "cr0",  "cr1",
    "cr2",  "cr3",  "cr4",  "cr5",  "cr6",  "cr7",  "cr20", "cr21", "cr22", "cr23", "cr24", "cr25",
    "cr26", "cr27", "cr28", "cr29", "cr30", "cr31", "ca",   "ctr",  "lr",
};

u32 eq_mask_of(i64 mb, i64 me) {
    u64 m = EQ_MASK;
    if (mb <= me) return (u32)(((m >> mb) & (m << (31 - me))) & m);
    return (u32)(((m >> mb) | (m << (31 - me))) & m);
}

/* ---- ops ---------------------------------------------------------------------------------------------------- */
static const char *const KNOWN_OPS[EQ_OP__KNOWN] = {
    "c", "E", "bool", "sym", "u", "bv", "csym", "self", "nocr6",
    "lin", "mul", "and", "or", "xor", "shr", "sra", "rotl", "sext", "crw",
    "b2i", "bitof", "bnot", "band", "bor", "bxor", "lts", "ltu", "eqz",
    "los", "lou", "ha", "hi", "ld", "mv", "mem", "fimg", "args", "ext",
    "cat", "fa", "ioload", "st", "call", "ret", "clob", "memafter", "ind",
    "cadd", "cadde", "casra", "casraw", "flt", "feq", "fun", "fsel",
    "s2d", "d2s", "frsp", "fadds", "fsubs", "fmuls", "fdivs", "fmadds",
    "fmsubs", "fnmadds", "fnmsubs", "fres", "psq", "psq1", "psqb", "psqb1",
    "fneg", "fabs", "cntlzw", "rotlv",
};

static AvpMutex g_op_lock;
static AvpMap *g_op_ids;            /* name -> id */
static AVP_VEC(const char *) g_op_names; /* id -> name */

static void op_table_init(void) {
    if (g_op_ids) return;
    g_op_ids = avp_map_new();
    for (int i = 0; i < EQ_OP__KNOWN; i++) {
        bool created;
        avp_map_put(g_op_ids, avp_str(KNOWN_OPS[i]), &created)->i = i;
        avp_vec_push(&g_op_names, KNOWN_OPS[i]);
    }
}

int eq_op_id(const char *name) {
    avp_mutex_lock(&g_op_lock);
    op_table_init();
    bool created;
    AvpVal *v = avp_map_put(g_op_ids, avp_str(name), &created);
    if (created) {
        size_t n = strlen(name);
        char *copy = avp_xmalloc(n + 1);
        memcpy(copy, name, n + 1);
        v->i = (i64)g_op_names.n;
        avp_vec_push(&g_op_names, copy);
    }
    int id = (int)v->i;
    avp_mutex_unlock(&g_op_lock);
    return id;
}

const char *eq_op_name(int op) {
    if (op >= 0 && op < EQ_OP__KNOWN) return KNOWN_OPS[op];
    avp_mutex_lock(&g_op_lock);
    op_table_init();
    const char *s = (op >= 0 && (size_t)op < g_op_names.n) ? g_op_names.v[op] : "?";
    avp_mutex_unlock(&g_op_lock);
    return s;
}

bool eq_is_single_op(int op) {
    switch (op) {
    case EQ_OP_S2D: case EQ_OP_FRSP: case EQ_OP_FADDS: case EQ_OP_FSUBS: case EQ_OP_FMULS: case EQ_OP_FDIVS:
    case EQ_OP_FMADDS: case EQ_OP_FMSUBS: case EQ_OP_FNMADDS: case EQ_OP_FNMSUBS: case EQ_OP_FRES: case EQ_OP_PSQ1:
        return true;
    default:
        return false;
    }
}

bool eq_is_sp_prop(int op) {
    switch (op) {
    case EQ_OP_LIN: case EQ_OP_AND: case EQ_OP_OR: case EQ_OP_XOR: case EQ_OP_SHR: case EQ_OP_SRA: case EQ_OP_ROTL:
    case EQ_OP_MUL:
        return true;
    default:
        return false;
    }
}

/* ---- the table ------------------------------------------------------------------------------------------------ */
static bool is_num(int kind) { return kind == EQ_ARG_INT || kind == EQ_ARG_BOOL; }

bool eq_arg_differs(EqArg x, EqArg y) {
    if (x.kind == EQ_ARG_NODE) return !(y.kind == EQ_ARG_NODE && y.n == x.n);
    if (y.kind == EQ_ARG_NODE) return true;
    if (is_num(x.kind)) return !(is_num(y.kind) && x.i == y.i);
    return !(y.kind == EQ_ARG_STR && strcmp(x.s, y.s) == 0);
}

static u64 mix(u64 h, u64 x) { return avp_hash64_u64(h ^ (x + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2))); }

static u64 key_hash(int op, const EqArg *args, int n) {
    u64 h = mix(0x51ED270B27B1A5ull, (u64)op);
    h = mix(h, (u64)n);
    for (int i = 0; i < n; i++) {
        const EqArg *x = &args[i];
        if (x->kind == EQ_ARG_NODE) h = mix(mix(h, 1), (u64)x->n->id);
        else if (is_num(x->kind)) h = mix(mix(h, 2), (u64)x->i);
        else h = mix(mix(h, 3), avp_hash64(x->s, strlen(x->s), 7));
    }
    return h;
}

static bool key_eq(const EqNode *n, int op, const EqArg *args, int nargs) {
    if (n->op != op || n->nargs != nargs) return false;
    for (int i = 0; i < nargs; i++) {
        const EqArg *x = &n->args[i], *y = &args[i];
        if (x->kind == EQ_ARG_NODE || y->kind == EQ_ARG_NODE) {
            if (x->kind != y->kind || x->n != y->n) return false;
        } else if (is_num(x->kind) || is_num(y->kind)) {
            if (!is_num(x->kind) || !is_num(y->kind) || x->i != y->i) return false;
        } else if (strcmp(x->s, y->s) != 0) {
            return false;
        }
    }
    return true;
}

static void table_grow(EqTerms *t) {
    size_t ncap = t->cap ? t->cap * 2 : 1024;
    EqNode **ns = avp_xcalloc(ncap, sizeof *ns);
    for (size_t i = 0; i < t->cap; i++) {
        EqNode *n = t->slots[i];
        if (!n) continue;
        size_t j = (size_t)n->hash & (ncap - 1);
        while (ns[j]) j = (j + 1) & (ncap - 1);
        ns[j] = n;
    }
    avp_xfree(t->slots);
    t->slots = ns;
    t->cap = ncap;
}

/* The node of the key, or NULL with *slot the free slot where it goes. */
static EqNode *table_find(EqTerms *t, int op, const EqArg *args, int nargs, u64 h, size_t *slot) {
    if ((t->len + 1) * 2 > t->cap) table_grow(t);
    size_t j = (size_t)h & (t->cap - 1);
    for (;;) {
        EqNode *n = t->slots[j];
        if (!n) {
            *slot = j;
            return NULL;
        }
        if (n->hash == h && key_eq(n, op, args, nargs)) return n;
        j = (j + 1) & (t->cap - 1);
    }
}

static EqNode *table_add(EqTerms *t, size_t slot, int op, const EqArg *args, int nargs, u64 h, u32 nz) {
    EqNode *n = AVP_NEW(t->a, EqNode);
    n->op = op;
    n->opname = eq_op_name(op);
    n->nargs = nargs;
    n->args = nargs ? AVP_NEWN(t->a, EqArg, nargs) : NULL;
    for (int i = 0; i < nargs; i++) {
        n->args[i] = args[i];
        if (args[i].kind == EQ_ARG_STR) n->args[i].s = avp_strdup(t->a, args[i].s);
    }
    n->id = (int)t->len;
    n->nz = nz;
    n->hash = h;
    t->slots[slot] = n;
    t->len++;
    return n;
}

EqNode *eq_node(EqTerms *t, int op, const EqArg *args, int nargs, u32 nz) {
    u64 h = key_hash(op, args, nargs);
    size_t slot;
    EqNode *n = table_find(t, op, args, nargs, h, &slot);
    if (n) return n;
    n = table_add(t, slot, op, args, nargs, h, nz);
    bool glob = true, local = false, sp = false;
    for (int i = 0; i < nargs; i++) {
        if (args[i].kind != EQ_ARG_NODE) continue;
        glob = glob && args[i].n->glob;
        local = local || args[i].n->local;
        sp = sp || args[i].n->sp;
    }
    n->glob = glob;
    n->local = local;
    n->sp = eq_is_sp_prop(op) && sp;
    return n;
}

EqNode *eq_node_n(EqTerms *t, int op, EqNode *const *kids, int nkids, u32 nz) {
    EqArg buf[8];
    EqArg *args = nkids <= 8 ? buf : avp_xmalloc(sizeof *args * (size_t)nkids);
    for (int i = 0; i < nkids; i++) args[i] = eq_an(kids[i]);
    EqNode *n = eq_node(t, op, args, nkids, nz);
    if (args != buf) avp_xfree(args);
    return n;
}

EqNode *eq_leaf(EqTerms *t, int op, const EqArg *args, int nargs, bool glob, bool local, bool sp, u32 nz) {
    u64 h = key_hash(op, args, nargs);
    size_t slot;
    EqNode *n = table_find(t, op, args, nargs, h, &slot);
    if (n) return n;
    n = table_add(t, slot, op, args, nargs, h, nz);
    n->glob = glob;
    n->local = local;
    n->sp = sp;
    return n;
}

EqTerms *eq_terms_new(AvpArena *a) {
    EqTerms *t = AVP_NEW(a, EqTerms);
    t->a = a;
    EqArg tr = eq_ab(true), fa = eq_ab(false), r1 = eq_as("r1");
    t->T = eq_leaf_d(t, EQ_OP_BOOL, &tr, 1);
    t->F = eq_leaf_d(t, EQ_OP_BOOL, &fa, 1);
    t->sp0 = eq_leaf(t, EQ_OP_E, &r1, 1, true, false, true, EQ_MASK);
    return t;
}

void eq_terms_free(EqTerms *t) {
    if (!t) return;
    avp_xfree(t->slots);
    t->slots = NULL;
    t->cap = t->len = 0;
}

EqNode *eq_c(EqTerms *t, i64 v) {
    u32 x = (u32)v;
    EqArg a = eq_ai(x);
    return eq_leaf(t, EQ_OP_C, &a, 1, true, false, false, x);
}

static u32 cval(const EqNode *n) { return (u32)n->args[0].i; }

EqNode *eq_entry(EqTerms *t, const char *loc) {
    if (strcmp(loc, "r1") == 0) return t->sp0;
    u32 nz = (strcmp(loc, "ca") == 0 || strncmp(loc, "cr", 2) == 0) ? 1 : EQ_MASK;
    EqArg a = eq_as(loc);
    return eq_leaf(t, EQ_OP_E, &a, 1, true, false, false, nz);
}

EqNode *eq_unknown(EqTerms *t, const char *side, const char *tag) {
    EqArg a[2] = {eq_as(side), eq_as(tag)};
    return eq_leaf(t, EQ_OP_U, a, 2, false, true, false, EQ_MASK);
}

EqNode *eq_sym(EqTerms *t, const char *name, i64 off) {
    EqArg a = eq_as(name);
    EqNode *s = eq_leaf_d(t, EQ_OP_SYM, &a, 1);
    EqNode *c = eq_c(t, off);
    return eq_add(t, s, c);
}

/* ---- lin ----------------------------------------------------------------------------------------------------- */
/* The Python's acc dict: insertion-ordered, keyed by node identity. Zero values stay in place until compacted (a
 * dict comprehension in the Python); del removes; a new key goes to the end. */
typedef struct {
    EqNode *t;
    u32 v;
} AccEnt;
typedef AVP_VEC(AccEnt) Acc;

static ptrdiff_t acc_find(const Acc *acc, const EqNode *t) {
    for (size_t i = 0; i < acc->n; i++)
        if (acc->v[i].t == t) return (ptrdiff_t)i;
    return -1;
}

static void acc_add(Acc *acc, EqNode *t, u32 d) {
    ptrdiff_t i = acc_find(acc, t);
    if (i < 0) {
        AccEnt e = {t, d};
        avp_vec_push(acc, e);
    } else {
        acc->v[i].v = acc->v[i].v + d;
    }
}

static void acc_compact(Acc *acc) {
    size_t o = 0;
    for (size_t i = 0; i < acc->n; i++)
        if (acc->v[i].v) acc->v[o++] = acc->v[i];
    acc->n = o;
}

static int cmp_acc_id(const void *x, const void *y) {
    int a = ((const AccEnt *)x)->t->id, b = ((const AccEnt *)y)->t->id;
    return a < b ? -1 : a > b;
}

static int bit_length(u32 v) {
    int n = 0;
    while (v) {
        n++;
        v >>= 1;
    }
    return n;
}

EqNode *eq_lin(EqTerms *t, i64 k64, const EqTerm *terms, size_t nterms) {
    Acc acc = {0};
    u32 k = (u32)k64;
    for (size_t j = 0; j < nterms; j++) {
        EqNode *x = terms[j].t;
        u32 cf = (u32)terms[j].cf;
        if (!cf) continue;
        if (x->op == EQ_OP_C) {
            k = k + cf * cval(x);
        } else if (x->op == EQ_OP_LIN) {
            k = k + cf * (u32)x->args[0].i;
            for (int i = 1; i < x->nargs; i += 2) acc_add(&acc, x->args[i].n, cf * (u32)x->args[i + 1].i);
        } else {
            acc_add(&acc, x, cf);
        }
    }
    acc_compact(&acc);
    /* relocation halves: (sym@ha << 16) + sext(sym@l) is the address of sym */
    size_t nha = 0;
    EqNode **has = NULL;
    for (size_t i = 0; i < acc.n; i++)
        if (acc.v[i].t->op == EQ_OP_HA) nha++;
    if (nha) {
        has = avp_xmalloc(sizeof *has * nha);
        nha = 0;
        for (size_t i = 0; i < acc.n; i++)
            if (acc.v[i].t->op == EQ_OP_HA) has[nha++] = acc.v[i].t;
    }
    for (size_t h = 0; h < nha; h++) {
        EqNode *ha = has[h];
        EqNode *lo = eq_node(t, EQ_OP_LOS, &ha->args[0], 1, EQ_MASK);
        ptrdiff_t ih = acc_find(&acc, ha), il = acc_find(&acc, lo);
        if (ih >= 0 && acc.v[ih].v == 0x10000 && il >= 0 && acc.v[il].v == 1) {
            avp_vec_remove(&acc, ih);
            il = acc_find(&acc, lo);
            avp_vec_remove(&acc, il);
            EqNode *a = ha->args[0].n;
            if (a->op != EQ_OP_LIN) {
                acc_add(&acc, a, 1);
            } else {
                for (int i = 1; i < a->nargs; i += 2) acc_add(&acc, a->args[i].n, (u32)a->args[i + 1].i);
                k = k + (u32)a->args[0].i;
            }
            acc_compact(&acc);
        }
    }
    avp_xfree(has);
    EqNode *res;
    if (!acc.n) {
        res = eq_c(t, k);
    } else if (k == 0 && acc.n == 1 && acc.v[0].v == 1) {
        res = acc.v[0].t;
    } else {
        qsort(acc.v, acc.n, sizeof *acc.v, cmp_acc_id); /* ids are unique: no stability question */
        u32 nz = EQ_MASK;
        if (k == 0 && acc.n == 1) {
            u32 v = acc.v[0].v;
            if ((v & (v - 1)) == 0) nz = (u32)(((u64)acc.v[0].t->nz << (bit_length(v) - 1)) & EQ_MASK);
        }
        int nargs = 1 + 2 * (int)acc.n;
        EqArg *args = avp_xmalloc(sizeof *args * (size_t)nargs);
        args[0] = eq_ai(k);
        for (size_t i = 0; i < acc.n; i++) {
            args[1 + 2 * i] = eq_an(acc.v[i].t);
            args[2 + 2 * i] = eq_ai(acc.v[i].v);
        }
        res = eq_node(t, EQ_OP_LIN, args, nargs, nz);
        avp_xfree(args);
    }
    avp_vec_free(&acc);
    return res;
}

EqNode *eq_addv(EqTerms *t, EqNode *const *xs, size_t n) {
    EqTerm buf[8];
    EqTerm *ts = n <= 8 ? buf : avp_xmalloc(sizeof *ts * n);
    for (size_t i = 0; i < n; i++) ts[i] = (EqTerm){xs[i], 1};
    EqNode *r = eq_lin(t, 0, ts, n);
    if (ts != buf) avp_xfree(ts);
    return r;
}

EqNode *eq_add(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *xs[2] = {a, b};
    return eq_addv(t, xs, 2);
}

EqNode *eq_add3(EqTerms *t, EqNode *a, EqNode *b, EqNode *c) {
    EqNode *xs[3] = {a, b, c};
    return eq_addv(t, xs, 3);
}

EqNode *eq_sub(EqTerms *t, EqNode *a, EqNode *b) {
    EqTerm ts[2] = {{a, 1}, {b, -1}};
    return eq_lin(t, 0, ts, 2);
}

EqNode *eq_mulc(EqTerms *t, EqNode *a, i64 c) {
    EqTerm ts[1] = {{a, c}};
    return eq_lin(t, 0, ts, 1);
}

static void sort2(EqNode **a, EqNode **b) {
    if ((*b)->id < (*a)->id) {
        EqNode *x = *a;
        *a = *b;
        *b = x;
    }
}

EqNode *eq_mul(EqTerms *t, EqNode *a, EqNode *b) {
    if (a->op == EQ_OP_C) return eq_mulc(t, b, cval(a));
    if (b->op == EQ_OP_C) return eq_mulc(t, a, cval(b));
    EqNode *xs[2] = {a, b};
    sort2(&xs[0], &xs[1]);
    return eq_node_n(t, EQ_OP_MUL, xs, 2, EQ_MASK);
}

/* ---- node lists: uniq by id, sorted by id ------------------------------------------------------------------- */
typedef AVP_VEC(EqNode *) NodeVec;

static int cmp_node_id(const void *x, const void *y) {
    int a = (*(EqNode *const *)x)->id, b = (*(EqNode *const *)y)->id;
    return a < b ? -1 : a > b;
}

/* sorted({p.id: p for p in parts}.values(), key=id): one per node, ascending id. */
static void uniq_sorted(NodeVec *v) {
    if (!v->n) return;
    qsort(v->v, v->n, sizeof *v->v, cmp_node_id);
    size_t o = 1;
    for (size_t i = 1; i < v->n; i++)
        if (v->v[i] != v->v[o - 1]) v->v[o++] = v->v[i];
    v->n = o;
}

static EqNode *node_from_vec(EqTerms *t, int op, NodeVec *parts, bool with_int, i64 last, u32 nz) {
    size_t n = parts->n + (with_int ? 1 : 0);
    EqArg *args = avp_xmalloc(sizeof *args * n);
    for (size_t i = 0; i < parts->n; i++) args[i] = eq_an(parts->v[i]);
    if (with_int) args[parts->n] = eq_ai(last);
    EqNode *r = eq_node(t, op, args, (int)n, nz);
    avp_xfree(args);
    return r;
}

/* ---- and / or / xor ------------------------------------------------------------------------------------------ */
EqNode *eq_andv(EqTerms *t, EqNode *const *xs, size_t nx) {
    u32 m = EQ_MASK;
    NodeVec parts = {0};
    for (size_t i = 0; i < nx; i++) {
        EqNode *x = xs[i];
        if (x->op == EQ_OP_AND) {
            for (int j = 0; j < x->nargs - 1; j++) {
                EqNode *y = x->args[j].n;
                if (y->op == EQ_OP_C) m &= cval(y);
                else avp_vec_push(&parts, y);
            }
            m &= (u32)x->args[x->nargs - 1].i;
        } else if (x->op == EQ_OP_C) {
            m &= cval(x);
        } else {
            avp_vec_push(&parts, x);
        }
    }
    EqNode *r;
    if (m == 0) {
        r = eq_c(t, 0);
    } else {
        uniq_sorted(&parts);
        if (!parts.n) {
            r = eq_c(t, m);
        } else if (parts.n == 1) {
            r = eq_and_mask(t, parts.v[0], m);
        } else {
            u32 nz = m;
            for (size_t i = 0; i < parts.n; i++) nz &= parts.v[i]->nz;
            r = node_from_vec(t, EQ_OP_AND, &parts, true, m, nz);
        }
    }
    avp_vec_free(&parts);
    return r;
}

EqNode *eq_and(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *xs[2] = {a, b};
    return eq_andv(t, xs, 2);
}

EqNode *eq_and_mask(EqTerms *t, EqNode *x, i64 m64) {
    u32 m = (u32)m64;
    if (x->op == EQ_OP_C) return eq_c(t, cval(x) & m);
    m &= x->nz;
    if (m == 0) return eq_c(t, 0);
    if (m == x->nz) return x;
    if (x->op == EQ_OP_SEXT && (u64)m < (1ull << x->args[1].i)) return eq_and_mask(t, x->args[0].n, m);
    if (x->op == EQ_OP_AND) {
        EqNode *c = eq_c(t, m);
        return eq_and(t, x, c);
    }
    EqArg args[2] = {eq_an(x), eq_ai(m)};
    return eq_node(t, EQ_OP_AND, args, 2, m);
}

EqNode *eq_orv(EqTerms *t, EqNode *const *xs, size_t nx) {
    u32 k = 0;
    NodeVec parts = {0};
    for (size_t i = 0; i < nx; i++) {
        EqNode *x = xs[i];
        if (x->op == EQ_OP_OR) {
            for (int j = 0; j < x->nargs; j++) {
                EqNode *y = x->args[j].n;
                if (y->op == EQ_OP_C) k |= cval(y);
                else avp_vec_push(&parts, y);
            }
        } else if (x->op == EQ_OP_C) {
            k |= cval(x);
        } else {
            avp_vec_push(&parts, x);
        }
    }
    EqNode *r = NULL;
    if (k == EQ_MASK) {
        r = eq_c(t, EQ_MASK);
        goto out;
    }
    uniq_sorted(&parts);
    if (!parts.n) {
        r = eq_c(t, k);
        goto out;
    }
    /* (sym@h << 16) | zext(sym@l) is the address of sym */
    if (k == 0 && parts.n == 2) {
        for (int w = 0; w < 2; w++) {
            EqNode *hi = parts.v[w], *lo = parts.v[1 - w];
            if (lo->op == EQ_OP_LOU && hi->op == EQ_OP_LIN && hi->nargs == 3 && hi->args[0].i == 0 &&
                hi->args[2].i == 0x10000 && hi->args[1].n->op == EQ_OP_HI &&
                hi->args[1].n->args[0].kind == EQ_ARG_NODE && lo->args[0].kind == EQ_ARG_NODE &&
                hi->args[1].n->args[0].n == lo->args[0].n) {
                r = lo->args[0].n;
                goto out;
            }
        }
    }
    if (k) {
        EqNode *c = eq_c(t, k);
        avp_vec_push(&parts, c);
    }
    if (parts.n == 1) {
        r = parts.v[0];
    } else {
        u32 nz = 0;
        for (size_t i = 0; i < parts.n; i++) nz |= parts.v[i]->nz;
        r = node_from_vec(t, EQ_OP_OR, &parts, false, 0, nz);
    }
out:
    avp_vec_free(&parts);
    return r;
}

EqNode *eq_or(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *xs[2] = {a, b};
    return eq_orv(t, xs, 2);
}

EqNode *eq_xorv(EqTerms *t, EqNode *const *xs, size_t nx) {
    u32 k = 0;
    NodeVec seen = {0};
    AVP_VEC(u8) cnt = {0};
    for (size_t i = 0; i < nx; i++) {
        EqNode *x = xs[i];
        int ny = x->op == EQ_OP_XOR ? x->nargs : 1;
        for (int j = 0; j < ny; j++) {
            EqNode *y = x->op == EQ_OP_XOR ? x->args[j].n : x;
            if (y->op == EQ_OP_C) {
                k ^= cval(y);
                continue;
            }
            size_t q = 0;
            while (q < seen.n && seen.v[q] != y) q++;
            if (q == seen.n) {
                avp_vec_push(&seen, y);
                avp_vec_push(&cnt, (u8)0);
            }
            cnt.v[q] ^= 1;
        }
    }
    NodeVec parts = {0};
    for (size_t q = 0; q < seen.n; q++)
        if (cnt.v[q]) avp_vec_push(&parts, seen.v[q]);
    qsort(parts.v, parts.n, sizeof *parts.v, cmp_node_id);
    EqNode *r;
    if (!parts.n) {
        r = eq_c(t, k);
    } else {
        if (k) {
            EqNode *c = eq_c(t, k);
            avp_vec_push(&parts, c);
        }
        if (parts.n == 1) {
            r = parts.v[0];
        } else {
            u32 nz = 0;
            for (size_t i = 0; i < parts.n; i++) nz |= parts.v[i]->nz;
            r = node_from_vec(t, EQ_OP_XOR, &parts, false, 0, nz);
        }
    }
    avp_vec_free(&seen);
    avp_vec_free(&cnt);
    avp_vec_free(&parts);
    return r;
}

EqNode *eq_xor(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *xs[2] = {a, b};
    return eq_xorv(t, xs, 2);
}

EqNode *eq_not(EqTerms *t, EqNode *x) {
    EqNode *m = eq_c(t, EQ_MASK);
    return eq_xor(t, x, m);
}

/* ---- shifts ---------------------------------------------------------------------------------------------------- */
EqNode *eq_shl(EqTerms *t, EqNode *x, i64 n) {
    return (n & 31) ? eq_mulc(t, x, (i64)1 << (n & 31)) : x;
}

EqNode *eq_shr(EqTerms *t, EqNode *x, i64 n) {
    n &= 63;
    if (n == 0) return x;
    if (n >= 32) return eq_c(t, 0);
    if (x->op == EQ_OP_C) return eq_c(t, cval(x) >> n);
    if ((x->nz >> n) == 0) return eq_c(t, 0);
    if (x->op == EQ_OP_SHR) return eq_shr(t, x->args[0].n, x->args[1].i + n);
    EqArg args[2] = {eq_an(x), eq_ai(n)};
    return eq_node(t, EQ_OP_SHR, args, 2, x->nz >> n);
}

EqNode *eq_sra(EqTerms *t, EqNode *x, i64 n) {
    AVP_ASSERT(n >= 0); /* the Python raises ValueError on a negative count */
    if (n == 0) return x;
    if (x->op == EQ_OP_C) return eq_c(t, eq_s32(cval(x)) >> (n > 63 ? 63 : n));
    if (!(x->nz & 0x80000000u)) return eq_shr(t, x, n);
    EqArg args[2] = {eq_an(x), eq_ai(n)};
    return eq_node(t, EQ_OP_SRA, args, 2, EQ_MASK);
}

EqNode *eq_sext(EqTerms *t, EqNode *x, i64 bits) {
    AVP_ASSERT(bits >= 1 && bits <= 62);
    if (x->op == EQ_OP_C) {
        i64 v = (i64)(cval(x) & (((u64)1 << bits) - 1));
        return eq_c(t, (v >> (bits - 1)) ? v - ((i64)1 << bits) : v);
    }
    if ((u64)x->nz < ((u64)1 << (bits - 1))) return x;
    if (x->op == EQ_OP_SEXT && x->args[1].i <= bits) return x;
    EqNode *low = eq_and_mask(t, x, (i64)(((u64)1 << bits) - 1));
    if (low->op == EQ_OP_SEXT) return low;
    EqArg args[2] = {eq_an(low), eq_ai(bits)};
    return eq_node(t, EQ_OP_SEXT, args, 2, EQ_MASK);
}

EqNode *eq_rot_mask(EqTerms *t, EqNode *x, i64 s, i64 m64) {
    s &= 31;
    u32 m = (u32)m64;
    if (x->op == EQ_OP_CRW) {
        EqNode *out[32];
        size_t n = 0;
        for (int p = 0; p < 32; p++) {
            if ((m >> (31 - p)) & 1) {
                EqNode *b = eq_b2i(t, x->args[(p + s) % 32].n);
                out[n++] = eq_shl(t, b, 31 - p);
            }
        }
        return n ? eq_orv(t, out, n) : eq_c(t, 0);
    }
    if (s == 0) return eq_and_mask(t, x, m);
    u64 low = ((u64)1 << s) - 1;
    if ((m & low) == 0) {
        EqNode *y = eq_shl(t, x, s);
        return eq_and_mask(t, y, m);
    }
    if ((u64)m <= low) {
        EqNode *y = eq_shr(t, x, 32 - s);
        return eq_and_mask(t, y, m);
    }
    if (x->op == EQ_OP_C) {
        u64 v = cval(x);
        return eq_c(t, (i64)((((v << s) | (v >> (32 - s))) & EQ_MASK) & m));
    }
    EqArg args[2] = {eq_an(x), eq_ai(s)};
    EqNode *r = eq_node(t, EQ_OP_ROTL, args, 2, EQ_MASK);
    return eq_and_mask(t, r, m);
}

EqNode *eq_op(EqTerms *t, int op, EqNode *const *args, int nargs, bool comm, u32 nz) {
    if (!comm) return eq_node_n(t, op, args, nargs, nz);
    EqNode *buf[8];
    EqNode **s = nargs <= 8 ? buf : avp_xmalloc(sizeof *s * (size_t)nargs);
    memcpy(s, args, sizeof *s * (size_t)nargs);
    /* sorted(args, key=id) is stable; equal ids are the same node, so any sort gives the same list */
    qsort(s, (size_t)nargs, sizeof *s, cmp_node_id);
    EqNode *r = eq_node_n(t, op, s, nargs, nz);
    if (s != buf) avp_xfree(s);
    return r;
}

/* ---- booleans ------------------------------------------------------------------------------------------------ */
EqNode *eq_b2i(EqTerms *t, EqNode *b) {
    if (b == t->T) return eq_c(t, 1);
    if (b == t->F) return eq_c(t, 0);
    return eq_node1(t, EQ_OP_B2I, b, 1);
}

EqNode *eq_bit_of(EqTerms *t, EqNode *x, int i) {
    if (x->op == EQ_OP_CRW) return x->args[i].n;
    if (x->op == EQ_OP_C) return ((cval(x) >> (31 - i)) & 1) ? t->T : t->F;
    EqArg args[2] = {eq_an(x), eq_ai(i)};
    return eq_node(t, EQ_OP_BITOF, args, 2, 1);
}

EqNode *eq_bnot(EqTerms *t, EqNode *b) {
    if (b == t->T) return t->F;
    if (b == t->F) return t->T;
    if (b->op == EQ_OP_BNOT) return b->args[0].n;
    return eq_node1(t, EQ_OP_BNOT, b, 1);
}

static bool vec_has(const NodeVec *v, const EqNode *n) {
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i] == n) return true;
    return false;
}

/* band / bor: absorb is F for band (T for bor), unit the other. */
static EqNode *bool_join(EqTerms *t, int op, EqNode *const *xs, size_t nx, EqNode *absorb, EqNode *unit) {
    NodeVec parts = {0};
    EqNode *r = NULL;
    for (size_t i = 0; i < nx && !r; i++) {
        EqNode *x = xs[i];
        int ny = x->op == op ? x->nargs : 1;
        for (int j = 0; j < ny; j++) {
            EqNode *y = x->op == op ? x->args[j].n : x;
            if (y == absorb) {
                r = absorb;
                break;
            }
            if (y != unit && !vec_has(&parts, y)) avp_vec_push(&parts, y);
        }
    }
    if (!r) {
        qsort(parts.v, parts.n, sizeof *parts.v, cmp_node_id);
        for (size_t i = 0; i < parts.n; i++) {
            EqNode *nb = eq_bnot(t, parts.v[i]);
            if (vec_has(&parts, nb)) {
                r = absorb;
                break;
            }
        }
    }
    if (!r) {
        if (!parts.n) r = unit;
        else if (parts.n == 1) r = parts.v[0];
        else r = node_from_vec(t, op, &parts, false, 0, 1);
    }
    avp_vec_free(&parts);
    return r;
}

EqNode *eq_bandv(EqTerms *t, EqNode *const *xs, size_t n) { return bool_join(t, EQ_OP_BAND, xs, n, t->F, t->T); }
EqNode *eq_band(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *xs[2] = {a, b};
    return eq_bandv(t, xs, 2);
}
EqNode *eq_borv(EqTerms *t, EqNode *const *xs, size_t n) { return bool_join(t, EQ_OP_BOR, xs, n, t->T, t->F); }
EqNode *eq_bor(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *xs[2] = {a, b};
    return eq_borv(t, xs, 2);
}

EqNode *eq_bxor(EqTerms *t, EqNode *a, EqNode *b) {
    if (a == b) return t->F;
    if (a == t->F) return b;
    if (b == t->F) return a;
    if (a == t->T) return eq_bnot(t, b);
    if (b == t->T) return eq_bnot(t, a);
    bool neg = false;
    if (a->op == EQ_OP_BNOT) {
        a = a->args[0].n;
        neg = !neg;
    }
    if (b->op == EQ_OP_BNOT) {
        b = b->args[0].n;
        neg = !neg;
    }
    EqNode *xs[2] = {a, b};
    sort2(&xs[0], &xs[1]);
    EqNode *r = eq_node_n(t, EQ_OP_BXOR, xs, 2, 1);
    return neg ? eq_bnot(t, r) : r;
}

EqNode *eq_lt(EqTerms *t, bool is_signed, EqNode *a, EqNode *b) {
    if (a == b) return t->F;
    if (a->op == EQ_OP_C && b->op == EQ_OP_C) {
        i64 x = cval(a), y = cval(b);
        if (is_signed) {
            x = eq_s32(x);
            y = eq_s32(y);
        }
        return x < y ? t->T : t->F;
    }
    if (!is_signed && b->op == EQ_OP_C && cval(b) == 0) return t->F;
    if (!is_signed && a->op == EQ_OP_C && cval(a) == 0) {
        EqNode *z = eq_c(t, 0);
        EqNode *e = eq_eq(t, b, z);
        return eq_bnot(t, e);
    }
    EqNode *xs[2] = {a, b};
    return eq_node_n(t, is_signed ? EQ_OP_LTS : EQ_OP_LTU, xs, 2, 1);
}

EqNode *eq_eq(EqTerms *t, EqNode *a, EqNode *b) {
    EqNode *d = eq_sub(t, a, b);
    if (d->op == EQ_OP_C) return cval(d) == 0 ? t->T : t->F;
    EqNode *nd = eq_mulc(t, d, -1);
    if (nd->id < d->id) d = nd;
    return eq_node1(t, EQ_OP_EQZ, d, 1);
}

/* ---- show ---------------------------------------------------------------------------------------------------- */
/* "0x%X" % v for a Python int: "%X" of a negative value is "-" and the magnitude, so -24 gives "0x-18". */
static void put_hex(AvpBuf *b, i64 v) {
    if (v < 0) avp_buf_printf(b, "0x-%llX", (unsigned long long)(0 - (u64)v));
    else avp_buf_printf(b, "0x%llX", (unsigned long long)v);
}

/* A lin constant or coefficient: hex below 0x80000000, else minus the hex of its negation. */
static void put_word(AvpBuf *b, i64 v) {
    if (v < 0x80000000ll) put_hex(b, v);
    else avp_buf_printf(b, "-0x%llX", (unsigned long long)(-eq_s32(v)));
}

static void show_into(AvpBuf *b, const EqNode *n, int depth);

static void show_arg_into(AvpBuf *b, EqArg x, int depth) {
    switch (x.kind) {
    case EQ_ARG_NODE: show_into(b, x.n, depth); break;
    case EQ_ARG_BOOL: avp_buf_putz(b, x.i ? "True" : "False"); break;
    case EQ_ARG_STR: avp_buf_putz(b, x.s); break;
    default:
        if (x.i > 9 || x.i < -9) put_hex(b, x.i);
        else avp_buf_printf(b, "%lld", (long long)x.i);
    }
}

/* "%s" % x */
static void str_arg_into(AvpBuf *b, EqArg x) {
    switch (x.kind) {
    case EQ_ARG_NODE: show_into(b, x.n, EQ_SHOW_DEPTH); break;
    case EQ_ARG_BOOL: avp_buf_putz(b, x.i ? "True" : "False"); break;
    case EQ_ARG_STR: avp_buf_putz(b, x.s); break;
    default: avp_buf_printf(b, "%lld", (long long)x.i);
    }
}

static void show_into(AvpBuf *b, const EqNode *n, int depth) {
    switch (n->op) {
    case EQ_OP_C: {
        i64 v = n->args[0].i;
        if (v > 9) put_hex(b, v);
        else avp_buf_printf(b, "%lld", (long long)v);
        return;
    }
    case EQ_OP_E:
        avp_buf_putz(b, "in.");
        str_arg_into(b, n->args[0]);
        return;
    case EQ_OP_BOOL:
        avp_buf_putz(b, n->args[0].i ? "true" : "false");
        return;
    case EQ_OP_SYM:
        str_arg_into(b, n->args[0]);
        return;
    case EQ_OP_U:
        avp_buf_putc(b, '?');
        str_arg_into(b, n->args[0]);
        avp_buf_putc(b, ':');
        str_arg_into(b, n->args[1]);
        return;
    case EQ_OP_BV:
        avp_buf_putc(b, 'v');
        str_arg_into(b, n->args[0]);
        avp_buf_putc(b, '.');
        str_arg_into(b, n->args[n->nargs - 1]);
        return;
    default: break;
    }
    if (depth <= 0) {
        avp_buf_printf(b, "%s(...)", n->opname);
        return;
    }
    switch (n->op) {
    case EQ_OP_LIN: {
        avp_buf_putc(b, '(');
        bool first = true;
        if (n->args[0].i) {
            put_word(b, n->args[0].i);
            first = false;
        }
        for (int i = 1; i < n->nargs; i += 2) {
            if (!first) avp_buf_putz(b, " + ");
            first = false;
            show_arg_into(b, n->args[i], depth - 1);
            i64 v = n->args[i + 1].i;
            if (v != 1) {
                avp_buf_putc(b, '*');
                put_word(b, v);
            }
        }
        avp_buf_putc(b, ')');
        return;
    }
    case EQ_OP_LD:
        avp_buf_printf(b, "ld%lld[", (long long)n->args[0].i);
        show_arg_into(b, n->args[1], depth - 1);
        avp_buf_putc(b, ']');
        return;
    case EQ_OP_MV: case EQ_OP_MEM: case EQ_OP_FIMG: case EQ_OP_ARGS:
        avp_buf_printf(b, "%s<%d>", n->opname, n->nargs);
        return;
    default:
        avp_buf_putz(b, n->opname);
        avp_buf_putc(b, '(');
        for (int i = 0; i < n->nargs; i++) {
            if (i) avp_buf_putz(b, ", ");
            show_arg_into(b, n->args[i], depth - 1);
        }
        avp_buf_putc(b, ')');
    }
}

char *eq_show(AvpArena *a, const EqNode *n, int depth) {
    AvpBuf b;
    avp_buf_init(&b, a);
    show_into(&b, n, depth);
    return avp_buf_cz(&b);
}

char *eq_show_arg(AvpArena *a, EqArg x, int depth) {
    AvpBuf b;
    avp_buf_init(&b, a);
    show_arg_into(&b, x, depth);
    return avp_buf_cz(&b);
}

char *eq_str_arg(AvpArena *a, EqArg x) {
    AvpBuf b;
    avp_buf_init(&b, a);
    str_arg_into(&b, x);
    return avp_buf_cz(&b);
}
