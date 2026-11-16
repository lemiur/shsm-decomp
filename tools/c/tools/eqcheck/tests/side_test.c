/* Tests for eq_side: Side.run_block over hand-written listings (block by block in id order, the state threaded
 * through, as tools/eqcheck.py's prover never does but which exercises every instruction family), then every node
 * the case made, in id order with its op, nz, flags and arguments (so node ids and canonical operand order are checked);
 * plus callee_arity, _int and Side.crbit samples. Expected text (side_expected.h) is printed by tools/eqcheck.py on
 * the same inputs with the same resolver (c_<hex> a constant address, none_* unresolved, any other name a symbol).
 * The two cases with callee liveness need the real asm index (skipped when build/index/asm.sqlite is absent). */
#include "avp/test.h"
#include "eq_side.h"
#include "shsm_cli.h"
#include "side_expected.h"

#include <stdio.h>
#include <stdlib.h>
#include "../../wall/tests/wall_testrows.h"

static EqNode *resolve(void *ctx, EqTerms *t, const CcRow *row) {
    (void)ctx;
    if (!row->has_ref) return NULL;
    AvpStr n = row->ref.name;
    if (avp_str_startswith(n, AVP_S("c_"))) {
        char *z = avp_cz(avp_scratch(), avp_str_slice(n, 2, AVP_END));
        return eq_c(t, (i64)strtoull(z, NULL, 16) + row->ref.off);
    }
    if (avp_str_startswith(n, AVP_S("none_"))) return NULL;
    return eq_sym(t, avp_cz(t->a, n), row->ref.off);
}

static const char *ref(AvpArena *a, const EqNode *n) { return n ? avp_sprintf(a, "#%d", n->id) : "None"; }

static void put_arg(AvpBuf *b, EqArg x) {
    switch (x.kind) {
    case EQ_ARG_NODE: avp_buf_printf(b, "#%d", x.n->id); break;
    case EQ_ARG_BOOL: avp_buf_printf(b, "%s", x.i ? "True" : "False"); break;
    case EQ_ARG_INT: avp_buf_printf(b, "%lld", (long long)x.i); break;
    default: avp_buf_printf(b, "s:%s", x.s); break;
    }
}

static int cmp_key(const void *x, const void *y) {
    const EqSlotKey *a = x, *b = y;
    if (a->off != b->off) return a->off < b->off ? -1 : 1;
    return (a->w > b->w) - (a->w < b->w);
}

static EqLive *g_live;

static const char *run_case(AvpArena *a, const SideCase *c) {
    EqTerms *t = eq_terms_new(a);
    CcFn *f = wall_test_fn(a, c->src);
    EqLive *live = NULL;
    if (c->live) {
        if (!g_live) g_live = eq_live_new(NULL, 4);
        live = g_live;
    }
    EqSide *side = eq_side_new("retail", f, resolve, NULL, t, live);
    side->escaped = c->escaped != 0;
    if (c->amap) {
        side->arity_map = eq_arity_map_new(a);
        if (*c->amap) {
            AvpStrVec p = avp_str_split(avp_str(c->amap), AVP_S(":"), -1);
            EqNode *s = eq_sym(t, avp_cz(a, p.v[1]), 0);
            EqArity ar = {EQ_ARITY_UNKNOWN, 0, 0};
            if (avp_str_eqz(p.v[0], "stack")) ar.kind = EQ_ARITY_STACK;
            else if (avp_str_eqz(p.v[0], "regs")) {
                ar.kind = EQ_ARITY_REGS;
                ar.ng = atoi(avp_cz(a, p.v[2]));
                ar.nf = atoi(avp_cz(a, p.v[3]));
            }
            eq_arity_map_set(side->arity_map, s->id, ar);
            avp_vec_free(&p);
        }
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putn(&b, "", 0);
    const EqState *st = eq_state_entry(t);
    for (size_t bid = 0; bid < side->cfg->nblocks; bid++) {
        EqBlockRun *volatile run = NULL;
        AvpTry tr;
        if (AVP_TRY(&tr)) {
            run = eq_side_run_block(side, (int)bid, st);
            avp_try_end(&tr);
        } else {
            const EqExc *e = eq_exc_caught(&tr);
            avp_buf_printf(&b, "B%zu raise %s: %s\n", bid, eq_exc_class(e->code), eq_exc_str(a, e));
            continue;
        }
        avp_buf_printf(&b, "B%zu %s\n", bid, wall_kind_name(run->kind));
        for (size_t k = 0; k < run->neffects; k++) {
            const EqEffect *e = &run->effects[k];
            if (e->kind == EQ_EFF_CALL) {
                avp_buf_printf(&b, " eff call %s | %s | ", ref(a, e->n), e->text);
                bool first = true;
                for (int r = 0; r < EQ_NARG_REGS; r++)
                    if (e->declared & (1u << r)) {
                        avp_buf_printf(&b, "%s%s", first ? "" : ",", EQ_ARG_REGS[r]);
                        first = false;
                    }
                avp_buf_printf(&b, "\n");
            } else if (e->kind == EQ_EFF_IO) {
                avp_buf_printf(&b, " eff io %s | %s\n", ref(a, e->n), e->text);
            } else {
                avp_buf_printf(&b, " eff st %s\n", ref(a, e->n));
            }
        }
        avp_buf_printf(&b, " cond %s exit %s\n", ref(a, run->cond), ref(a, run->exit_tail));
        const EqState *ns = run->state;
        for (int k = 0; k < EQ_NREG_LOCS; k++)
            if (ns->r[k] != st->r[k]) avp_buf_printf(&b, " r %s %s\n", EQ_REG_LOCS[k], ref(a, ns->r[k]));
        for (size_t k = 0; k < ns->nxr; k++) avp_buf_printf(&b, " x %s %s\n", ns->xr[k].name, ref(a, ns->xr[k].v));
        for (size_t k = 0; k < ns->nslots; k++)
            avp_buf_printf(&b, " slot %lld %lld %s\n", (long long)ns->slots[k].off, (long long)ns->slots[k].w,
                           ref(a, ns->slots[k].v));
        avp_buf_printf(&b, " mem %s stores ", ref(a, ns->mem));
        for (size_t k = 0; k < ns->nstores; k++) avp_buf_printf(&b, "%s%s", k ? " " : "", ref(a, ns->stores[k]));
        avp_buf_printf(&b, " frame %s ncall %lld dead ", ref(a, ns->frame), (long long)ns->ncall);
        EqSlotKey *dead = AVP_NEWN(a, EqSlotKey, ns->ndead + 1);
        if (ns->ndead) memcpy(dead, ns->dead, ns->ndead * sizeof *dead);
        qsort(dead, ns->ndead, sizeof *dead, cmp_key);
        for (size_t k = 0; k < ns->ndead; k++)
            avp_buf_printf(&b, "%s%lld:%lld", k ? " " : "", (long long)dead[k].off, (long long)dead[k].w);
        avp_buf_printf(&b, " esc %s\n", side->wants_escape ? "True" : "False");
        st = ns;
    }
    size_t n = eq_terms_len(t);
    EqNode **byid = AVP_NEWN(a, EqNode *, n + 1);
    for (size_t k = 0; k < t->cap; k++)
        if (t->slots[k]) byid[t->slots[k]->id] = t->slots[k];
    /* every node, in id order; the expected text keeps only the count and the FNV-1a hash of this dump (set
     * SIDE_DUMP=1 to see both sides' dumps: the generator writes the Python's to stderr) */
    AvpBuf d;
    avp_buf_init(&d, a);
    avp_buf_putn(&d, "", 0);
    for (size_t k = 0; k < n; k++) {
        const EqNode *x = byid[k];
        if (!x) {
            avp_buf_printf(&d, "#%zu missing\n", k);
            continue;
        }
        avp_buf_printf(&d, "#%d %s %X %d%d%d [", x->id, x->opname, x->nz, x->glob, x->local, x->sp);
        for (int i = 0; i < x->nargs; i++) {
            if (i) avp_buf_printf(&d, " ");
            put_arg(&d, x->args[i]);
        }
        avp_buf_printf(&d, "]\n");
    }
    AvpStr dump = avp_buf_str(&d);
    if (getenv("SIDE_DUMP")) fprintf(stderr, "=== %.*s\n%.*s", (int)strcspn(c->src, "\n"), c->src, AVP_SF(dump));
    u64 h = 0xCBF29CE484222325ull;
    for (size_t k = 0; k < dump.n; k++) h = (h ^ (u8)dump.p[k]) * 0x100000001B3ull;
    avp_buf_printf(&b, "nodes %zu %016llX\n", n, (unsigned long long)h);
    eq_terms_free(t);
    return avp_buf_cz(&b);
}

/* The first differing line of two texts (for the failure message). */
static void first_diff(const char *got, const char *want, char *out, size_t cap) {
    size_t line = 1;
    const char *g = got, *w = want;
    while (*g && *w) {
        const char *ge = strchr(g, '\n'), *we = strchr(w, '\n');
        size_t gl = ge ? (size_t)(ge - g) : strlen(g), wl = we ? (size_t)(we - w) : strlen(w);
        if (gl != wl || memcmp(g, w, gl) != 0) {
            snprintf(out, cap, "line %zu: got [%.*s] want [%.*s]", line, (int)gl, g, (int)wl, w);
            return;
        }
        if (!ge || !we) break;
        g = ge + 1;
        w = we + 1;
        line++;
    }
    snprintf(out, cap, "line %zu: lengths differ (got %zu, want %zu bytes)", line, strlen(got), strlen(want));
}

AVP_TEST(run_blocks) {
    bool have_index = avp_isfile("build/index/asm.sqlite");
    if (!have_index) printf("real tree: build/index/asm.sqlite absent, liveness cases skipped\n");
    for (size_t k = 0; k < sizeof SIDE_CASES / sizeof *SIDE_CASES; k++) {
        const SideCase *c = &SIDE_CASES[k];
        if (c->live && !have_index) continue;
        AvpArena *a = avp_arena_new(0);
        const char *got = run_case(a, c);
        if (strcmp(got, c->expected) != 0) {
            char msg[2048];
            first_diff(got, c->expected, msg, sizeof msg);
            CHECK_MSG(false, "case %zu (%s): %s", k, c->src, msg);
        }
        avp_arena_free(a);
    }
    if (g_live) eq_live_free(g_live);
}

AVP_TEST(callee_arity) {
    for (size_t k = 0; k < sizeof ARITY_NAMES / sizeof *ARITY_NAMES; k++) {
        EqArity ar = eq_callee_arity(ARITY_NAMES[k]);
        char got[32];
        if (ar.kind == EQ_ARITY_UNKNOWN) snprintf(got, sizeof got, "None");
        else if (ar.kind == EQ_ARITY_STACK) snprintf(got, sizeof got, "stack");
        else snprintf(got, sizeof got, "%d,%d", ar.ng, ar.nf);
        CHECK_MSG(strcmp(got, ARITY_EXPECTED[k]) == 0, "%s: got %s want %s", ARITY_NAMES[k], got, ARITY_EXPECTED[k]);
    }
}

/* the outcome of f(tok) as the generator prints it */
static const char *outcome(AvpArena *a, int which, const char *tok) {
    i64 volatile v = 0;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        v = which == 0 ? eq_py_int0(avp_str(tok)) : eq_side_crbit(a, avp_str(tok));
        avp_try_end(&tr);
    } else {
        const EqExc *e = eq_exc_caught(&tr);
        return avp_sprintf(a, "raise %s: %s", eq_exc_class(e->code), eq_exc_str(a, e));
    }
    return avp_sprintf(a, "%lld", (long long)v);
}

AVP_TEST(int_and_crbit) {
    AvpArena *a = avp_arena();
    for (size_t k = 0; k < sizeof INT_TOKS / sizeof *INT_TOKS; k++) {
        const char *got = outcome(a, 0, INT_TOKS[k]);
        CHECK_MSG(strcmp(got, INT_EXPECTED[k]) == 0, "int(%s): got %s want %s", INT_TOKS[k], got, INT_EXPECTED[k]);
    }
    for (size_t k = 0; k < sizeof CRBIT_TOKS / sizeof *CRBIT_TOKS; k++) {
        const char *got = outcome(a, 1, CRBIT_TOKS[k]);
        CHECK_MSG(strcmp(got, CRBIT_EXPECTED[k]) == 0, "crbit(%s): got %s want %s", CRBIT_TOKS[k], got,
                  CRBIT_EXPECTED[k]);
    }
}

AVP_TEST(loc_index_and_state) {
    CHECK_EQ_I64(eq_loc_index(AVP_S("r0")), 0);
    CHECK_EQ_I64(eq_loc_index(AVP_S("r31")), 31);
    CHECK_EQ_I64(eq_loc_index(AVP_S("r32")), -1);
    CHECK_EQ_I64(eq_loc_index(AVP_S("r03")), -1);
    CHECK_EQ_I64(eq_loc_index(AVP_S("f0")), 32);
    CHECK_EQ_I64(eq_loc_index(AVP_S("cr31")), 95);
    CHECK_EQ_I64(eq_loc_index(AVP_S("cr32")), -1);
    CHECK_EQ_I64(eq_loc_index(AVP_S("ca")), 96);
    CHECK_EQ_I64(eq_loc_index(AVP_S("ctr")), 97);
    CHECK_EQ_I64(eq_loc_index(AVP_S("lr")), 98);
    CHECK_EQ_I64(eq_loc_index(AVP_S("sp")), -1);
    for (int k = 0; k < EQ_NREG_LOCS; k++) CHECK_EQ_I64(eq_loc_index(avp_str(EQ_REG_LOCS[k])), k);
    /* dict semantics of the slots: assignment keeps an existing key's position, pop removes */
    AvpArena *a = avp_arena_new(0);
    EqTerms *t = eq_terms_new(a);
    EqState *s = eq_state_entry(t);
    EqNode *x = eq_c(t, 1), *y = eq_c(t, 2);
    eq_state_slot_set(a, s, -8, 4, x);
    eq_state_slot_set(a, s, -4, 4, y);
    eq_state_slot_set(a, s, -8, 4, y);
    CHECK_EQ_I64(s->slots[0].off, -8);
    CHECK(s->slots[0].v == y);
    EqState *c = eq_state_copy(a, s);
    CHECK(eq_state_slot_pop(c, -8, 4) == y);
    CHECK_EQ_I64((i64)c->nslots, 1);
    CHECK_EQ_I64((i64)s->nslots, 2);
    CHECK(eq_state_slot_pop(c, -8, 4) == NULL);
    eq_terms_free(t);
    avp_arena_free(a);
}

AVP_TEST(exceptions) {
    AvpArena *a = avp_arena();
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        AvpTry in;
        if (AVP_TRY(&in)) {
            eq_raise(EQ_EXC_NOTPROVEN, "inner %d", 1);
        } else {
            const EqExc *e = eq_exc_caught(&in);
            CHECK_EQ_I64(e->code, EQ_EXC_NOTPROVEN);
            CHECK(strcmp(e->msg, "inner 1") == 0);
            EqExc *saved = eq_exc_save(a, e);
            AvpTry in2;
            if (AVP_TRY(&in2)) {
                eq_raise(EQ_EXC_UNSUPPORTED, "other");
            } else {
                CHECK(strcmp(eq_exc_caught(&in2)->msg, "other") == 0);
            }
            eq_reraise(saved); /* to the outer try */
        }
    } else {
        const EqExc *e = eq_exc_caught(&tr);
        CHECK_EQ_I64(e->code, EQ_EXC_NOTPROVEN);
        CHECK(strcmp(e->msg, "inner 1") == 0);
        CHECK(strcmp(eq_exc_class(e->code), "NotProven") == 0);
    }
    /* a long reason keeps its whole text (avp_throw's buffer stops at 1023 bytes) */
    char *long_msg = avp_alloc(a, 3000);
    memset(long_msg, 'x', 2999);
    if (AVP_TRY(&tr)) {
        EqArg terms[2] = {eq_ai(1), eq_ai(2)};
        eq_raise_verdict("DIFFERENT", 3, terms, "%s", long_msg);
    } else {
        const EqExc *e = eq_exc_caught(&tr);
        CHECK_EQ_I64(e->code, EQ_EXC_VERDICT);
        CHECK_EQ_I64((i64)strlen(e->msg), 2999);
        CHECK(strcmp(e->vkind, "DIFFERENT") == 0);
        CHECK_EQ_I64(e->block, 3);
        CHECK(e->has_terms && e->terms[1].i == 2);
    }
    /* a throw from outside (shsm_raise) */
    if (AVP_TRY(&tr)) {
        shsm_raise(SHSM_EXC_KEY, "k");
    } else {
        const EqExc *e = eq_exc_caught(&tr);
        CHECK(strcmp(eq_exc_class(e->code), "KeyError") == 0);
        CHECK(strcmp(eq_exc_str(a, e), "'k'") == 0);
    }
}

AVP_TEST_MAIN("eqcheck_side")
