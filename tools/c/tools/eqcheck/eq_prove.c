/* eq_prove: Prover, the relocation resolver and prove_functions of tools/eqcheck.py (L1607-2247). See eq_prove.h.
 * Every node-making expression of the Python is sequenced left to right through locals, so node ids follow the
 * Python's allocation order. Exceptions follow eq_side.h: Unsupported, NotProven and Verdict are thrown with
 * eq_raise / eq_raise_verdict and caught exactly where the Python catches them. Locals live in arenas or in the
 * Prover (never modified between an AVP_TRY and its throw), so nothing needs volatile. */
#include "eq_prove.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shsm_cli.h"

/* ---- small helpers ------------------------------------------------------------------------------------------------ */

static void *grow(AvpArena *a, void *p, size_t *cap, size_t need, size_t elt) {
    if (need <= *cap) return p;
    size_t nc = *cap ? *cap * 2 : 8;
    while (nc < need) nc *= 2;
    void *q = avp_alloc(a, nc * elt);
    if (p && *cap) memcpy(q, p, *cap * elt);
    *cap = nc;
    return q;
}

/* Python's "%+#x" % v */
static const char *py_plus_hash_x(AvpArena *a, i64 v) {
    u64 m = v < 0 ? (u64)0 - (u64)v : (u64)v;
    return avp_sprintf(a, "%c0x%llx", v < 0 ? '-' : '+', (unsigned long long)m);
}

const char *eq_slot_locname(AvpArena *a, i64 off, i64 w) {
    return avp_sprintf(a, "r1%s/%lld", py_plus_hash_x(a, off), (long long)w);
}

/* str(("s", off, w)) */
static const char *slot_str(AvpArena *a, i64 off, i64 w) {
    return avp_sprintf(a, "('s', %lld, %lld)", (long long)off, (long long)w);
}

static int cmp_int(const void *x, const void *y, void *ctx) {
    (void)ctx;
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

/* x.op of an argument the Python reads as a node: a non-node raises AttributeError in the Python (unreachable on
 * the engine's terms, DEVIATIONS D-eqcheck-5). */
static const EqNode *as_node(EqArg x) {
    if (x.kind != EQ_ARG_NODE)
        eq_raise(SHSM_EXC_TYPE, "'%s' object has no attribute 'op'",
                 x.kind == EQ_ARG_STR ? "str" : x.kind == EQ_ARG_BOOL ? "bool" : "int");
    return x.n;
}

/* x is y for two argument values (nodes by identity; ints and strs, which the Python never compares with `is`
 * here, by value). */
static bool arg_is(EqArg x, EqArg y) {
    if (x.kind == EQ_ARG_NODE || y.kind == EQ_ARG_NODE) return x.kind == y.kind && x.n == y.n;
    return !eq_arg_differs(x, y);
}

/* ---- relocation resolution (L2147-2232) ---------------------------------------------------------------------- */

static bool is_hexdigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

bool eq_quoted_search(AvpStr text, AvpStr *name, AvpStr *off_hex) {
    size_t i = 0;
    while (i < text.n) {
        if (text.p[i] != '"') {
            i++;
            continue;
        }
        size_t j = i + 1;
        while (j < text.n && text.p[j] != '"') j++;
        if (j >= text.n) return false; /* no closing quote after this one: none after it either */
        if (j == i + 1) {              /* "" : [^"]+ needs one character; retry at the second quote */
            i = j;
            continue;
        }
        *name = avp_strn(text.p + i + 1, j - i - 1);
        *off_hex = (AvpStr){0};
        size_t k = j + 1;
        if (k + 3 < text.n + 1 && k + 2 < text.n && text.p[k] == '+' && text.p[k + 1] == '0' && text.p[k + 2] == 'x') {
            size_t e = k + 3;
            while (e < text.n && is_hexdigit(text.p[e])) e++;
            if (e > k + 3) *off_hex = avp_strn(text.p + k + 1, e - k - 1);
        }
        return true;
    }
    return false;
}

/* int(s, 16) of "0x..." (the digits after the prefix; 64-bit wrap as D-eqcheck-3) */
static i64 hex_value(AvpStr s) {
    u64 v = 0;
    for (size_t k = 2; k < s.n; k++) {
        char c = s.p[k];
        int d = c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
        v = v * 16 + (u64)d;
    }
    return (i64)v;
}

size_t eq_target_hints(AvpArena *a, const CcCompareResult *res, CcContext *ctx, EqHint **out) {
    *out = NULL;
    if (!ctx || !res) return 0;
    const CcFn *rfn = res->retail_fn, *cfn = res->compiled_fn;
    EqHint *v = NULL;
    size_t n = 0, cap = 0;
    AvpArena *sc = avp_scratch();
    for (size_t k = 0; k < res->npairs; k++) {
        const CcPair *p = &res->pairs[k];
        if (!p->retail || !p->compiled) continue;
        const CcRow *rrow = &rfn->rows[p->retail->index], *crow = &cfn->rows[p->compiled->index];
        if (!crow->reloc || !rrow->has_ref) continue;
        AvpArenaMark mk = avp_arena_mark(sc);
        AvpStr detail;
        CcStatus st = cc_context_check(sc, ctx, rrow, crow, rfn, cfn, &detail);
        i64 addr = 0;
        bool ok = st == CC_MATCH && cc_context_retail_address(sc, ctx, &rrow->ref, &addr, NULL);
        avp_arena_reset(sc, mk);
        if (!ok) continue;
        v = grow(a, v, &cap, n + 1, sizeof *v);
        v[n].row = crow;
        v[n].addr = addr;
        n++;
    }
    *out = v;
    return n;
}

struct EqResolver {
    const CcFn *fn;
    CcContext *ctx;
    const CcFn *rfn;
    bool *has_hint;
    i64 *hint;
    bool *done;
    EqNode **val;
};

EqResolver *eq_resolver_new(AvpArena *a, const CcFn *fn, CcContext *ctx, const CcFn *rfn, const EqHint *hints,
                            size_t nhints) {
    EqResolver *r = AVP_NEW(a, EqResolver);
    size_t n = fn->nrows ? fn->nrows : 1;
    r->fn = fn;
    r->ctx = ctx;
    r->rfn = rfn;
    r->has_hint = AVP_NEWN(a, bool, n);
    r->hint = AVP_NEWN(a, i64, n);
    r->done = AVP_NEWN(a, bool, n);
    r->val = AVP_NEWN(a, EqNode *, n);
    for (size_t k = 0; k < nhints; k++) {
        const CcRow *row = hints[k].row;
        if (fn->nrows && row >= fn->rows && row < fn->rows + fn->nrows) {
            size_t i = (size_t)(row - fn->rows);
            r->has_hint[i] = true;
            r->hint[i] = hints[k].addr;
        }
    }
    return r;
}

static bool in_sections(CcContext *ctx, i64 x) {
    const CcUnitSecs *s = cc_context_sections(ctx);
    for (size_t q = 0; q < s->n; q++)
        if (s->v[q].start <= x && x < s->v[q].end) return true;
    return false;
}

/* _compiled_address(fn, row, t, ctx, rfn) (L2186-2213) */
static EqNode *compiled_address(EqResolver *r, const CcRow *row, EqTerms *t) {
    const CcElf *elf = r->fn->elf;
    const CcReloc *rel = row->reloc;
    if (rel->sym >= elf->nsymbols) return NULL;
    const CcSym *sym = &elf->symbols[rel->sym];
    if (!r->ctx) return sym->name.n ? eq_sym(t, sym->name.p, rel->addend) : NULL;
    AvpArena *sc = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(sc);
    i64 *cands = NULL;
    size_t nc = 0;
    bool has = cc_context_compiled_addresses(sc, r->ctx, elf, rel, &cands, &nc);
    if (has && nc) {
        if (nc > 1) {
            size_t ninside = 0;
            i64 last = 0;
            for (size_t k = 0; k < nc; k++)
                if (in_sections(r->ctx, cands[k] - rel->addend)) ninside++, last = cands[k];
            if (ninside == 1) {
                cands[0] = last;
                nc = 1;
            }
        }
        if (nc == 1) {
            i64 v = cands[0];
            avp_arena_reset(sc, mk);
            return eq_c(t, v);
        }
    }
    if (sym->name.n && sym->shndx == 0 && !cc_is_compiler_local(sym->name)) {
        avp_arena_reset(sc, mk);
        return eq_sym(t, sym->name.p, rel->addend);
    }
    if (!has && sym->shndx && r->rfn) {
        i64 lay;
        if (cc_context_layout_address(r->ctx, elf, sym, rel->addend, &lay)) {
            CcStatus st = cc_context_data_check(sc, r->ctx, r->fn, r->rfn, sym, rel->addend, lay, NULL);
            if (st == CC_MATCH) {
                avp_arena_reset(sc, mk);
                return eq_c(t, lay);
            }
        }
    }
    avp_arena_reset(sc, mk);
    EqArg args[2] = {eq_as(sym->name.n ? sym->name.p : "(section)"), eq_ai(rel->addend)};
    return eq_leaf(t, EQ_OP_CSYM, args, 2, true, true, false, EQ_MASK);
}

/* resolve(row) of make_resolver (L2157-2182) */
static EqNode *resolve_row(EqResolver *r, EqTerms *t, const CcRow *row) {
    if (row->reloc && r->fn->elf) return compiled_address(r, row, t);
    AvpStr qname = {0}, qoff = {0};
    bool quoted = eq_quoted_search(row->text, &qname, &qoff);
    if (!(row->has_ref || (!row->reloc && !row->has_label && quoted))) return NULL;
    CcRef ref;
    if (!row->has_ref) {
        ref.name = avp_str(avp_cz(t->a, qname));
        ref.off = 0;
        ref.kind = AVP_S("rel24");
    } else {
        ref = row->ref;
    }
    if (!r->ctx) return eq_sym(t, ref.name.p, ref.off);
    AvpArena *sc = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(sc);
    i64 a = 0;
    bool has_a = cc_context_retail_address(sc, r->ctx, &ref, &a, NULL);
    if (!has_a && quoted) {
        /* codecmp's operand parser cuts names that contain '@'; the listing quotes them */
        const AvpI64Vec *cv = cc_project_addresses(r->ctx->project, qname);
        size_t nc = cv ? cv->n : 0;
        i64 one = 0;
        if (nc > 1) {
            size_t ninside = 0;
            for (size_t k = 0; k < nc; k++)
                if (in_sections(r->ctx, cv->v[k])) ninside++, one = cv->v[k];
            nc = ninside;
        } else if (nc == 1) {
            one = cv->v[0];
        }
        if (nc == 1) {
            a = one + (qoff.p ? hex_value(qoff) : 0);
            has_a = true;
        }
    }
    avp_arena_reset(sc, mk);
    return has_a ? eq_c(t, a) : eq_sym(t, ref.name.p, ref.off);
}

EqNode *eq_resolve(void *resolver, EqTerms *t, const CcRow *row) {
    EqResolver *r = resolver;
    AVP_ASSERT(r->fn->nrows && row >= r->fn->rows && row < r->fn->rows + r->fn->nrows);
    size_t i = (size_t)(row - r->fn->rows);
    if (r->done[i]) return r->val[i];
    EqNode *v = r->has_hint[i] ? eq_c(t, r->hint[i]) : resolve_row(r, t, row);
    r->done[i] = true;
    r->val[i] = v;
    return v;
}

/* ---- the Prover ------------------------------------------------------------------------------------------------- */

typedef struct {
    int b;
    const char *s;
} PairRec;

typedef struct Prover {
    AvpArena *a;   /* the proof's arena (t->a) */
    AvpArena *out; /* the result's arena */
    EqTerms *t;
    EqSide *sides[2];
    bool ret_explicit;
    const char *const *ret;
    int nret;
    int max_rounds;
    const char **assumptions;
    size_t nassumptions;
    const WallIso *mapping; /* NULL = None */
    int *swaps;             /* sorted(m[1]); NULL with mapping None */
    size_t nswaps;
    PairRec *pairs; /* pairs_seen: (str(b), "x=y") records, duplicates removed at the end */
    size_t npairs, pcap;
    AvpJson *explain;    /* list (in out) */
    const char *ret_reg; /* ret_reg of the Verdict check_exit raised last */
} Prover;

static const char *const DEFAULT_RET[3] = {"r3", "r4", "f1"};

static void reset_assumptions(Prover *p) {
    p->assumptions = AVP_NEWN(p->a, const char *, 4);
    p->assumptions[0] = EQ_ASSUMPTIONS[0];
    p->assumptions[1] = EQ_ASSUMPTIONS[1];
    p->assumptions[2] = EQ_ASSUMPTIONS[3];
    p->nassumptions = 3;
}

static int map_get(const Prover *p, int a) {
    if (a < 0 || (size_t)a >= p->mapping->nmap_a) return -1;
    return p->mapping->map[a];
}

/* self.mapping[a]: every reachable retail block is mapped by the isomorphism */
static int map_req(const Prover *p, int a) {
    int v = map_get(p, a);
    AVP_ASSERT(v >= 0);
    return v;
}

static int reg_index(const char *name) {
    int k = eq_loc_index(avp_str(name));
    if (k < 0) eq_raise(SHSM_EXC_KEY, "%s", name);
    return k;
}

/* ---- share_arities (L1676-1691) ---- */

static EqNode *target_or_null(EqSide *s, size_t i) {
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        EqNode *n = eq_side_target(s, i);
        avp_try_end(&tr);
        return n;
    }
    const EqExc *e = eq_exc_caught(&tr);
    if (e->code != EQ_EXC_NOTPROVEN && e->code != EQ_EXC_UNSUPPORTED) eq_reraise(e);
    return NULL;
}

static void share_arities(Prover *p) {
    EqArityMap *amap = eq_arity_map_new(p->a);
    for (int k = 0; k < 2; k++) {
        EqSide *s = p->sides[k];
        for (size_t i = 0; i < s->nrows; i++) {
            const CcRow *row = &s->rows[i];
            WallKind kind = wall_row_kind(row);
            if ((kind != WALL_K_CALL && kind != WALL_K_TAIL && kind != WALL_K_CONDTAIL) || !row->has_ref) continue;
            EqNode *tgt = target_or_null(s, i);
            if (!tgt) continue;
            EqArity ar = eq_callee_arity(avp_cz(p->a, row->ref.name));
            EqArity old;
            bool known = eq_arity_map_get(amap, tgt->id, &old);
            if (ar.kind != EQ_ARITY_UNKNOWN || !known) {
                if (ar.kind != EQ_ARITY_UNKNOWN) eq_arity_map_set(amap, tgt->id, ar);
                else eq_arity_map_set(amap, tgt->id, known ? old : (EqArity){EQ_ARITY_UNKNOWN, 0, 0});
            }
        }
    }
    p->sides[0]->arity_map = amap;
    p->sides[1]->arity_map = amap;
}

/* ---- pairings (L1693-1721) ---- */

typedef struct {
    AvpStr **norms; /* per block: its rows' norms, sorted */
    size_t *n;
} BlockTexts;

static int cmp_avpstr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

static BlockTexts block_texts(AvpArena *a, const EqSide *s) {
    BlockTexts bt;
    size_t nb = s->cfg->nblocks ? s->cfg->nblocks : 1;
    bt.norms = AVP_NEWN(a, AvpStr *, nb);
    bt.n = AVP_NEWN(a, size_t, nb);
    for (size_t b = 0; b < s->cfg->nblocks; b++) {
        const WallBlock *blk = &s->cfg->blocks[b];
        size_t n = blk->end - blk->start;
        AvpStr *v = AVP_NEWN(a, AvpStr, n ? n : 1);
        for (size_t i = 0; i < n; i++) v[i] = s->rows[blk->start + i].norm;
        avp_sort(v, n, sizeof *v, cmp_avpstr, NULL, false);
        bt.norms[b] = v;
        bt.n[b] = n;
    }
    return bt;
}

typedef struct {
    BlockTexts r, c;
} SimCtx;

/* sim(a, b): sum((texts(R, a) & texts(C, b)).values()) */
static i64 sim(void *ctx, int a, int b) {
    const SimCtx *sc = ctx;
    const AvpStr *x = sc->r.norms[a], *y = sc->c.norms[b];
    size_t nx = sc->r.n[a], ny = sc->c.n[b], i = 0, j = 0;
    i64 n = 0;
    while (i < nx && j < ny) {
        int c = avp_str_cmp(x[i], y[j]);
        if (c == 0) n++, i++, j++;
        else if (c < 0) i++;
        else j++;
    }
    return n;
}

/* Returns the number of candidate mappings written to out[0..2). */
static int pairings(Prover *p, WallIso *out) {
    for (int k = 0; k < 2; k++) {
        const EqSide *s = p->sides[k];
        for (size_t i = 0; i < s->nrows; i++)
            if (wall_row_kind(&s->rows[i]) == WALL_K_SWITCH) eq_raise(EQ_EXC_UNSUPPORTED, "bctr (jump table)");
    }
    SimCtx *sc = AVP_NEW(p->a, SimCtx);
    sc->r = block_texts(p->a, p->sides[0]);
    sc->c = block_texts(p->a, p->sides[1]);
    int n = 0;
    for (int kw = 0; kw < 2; kw++) {
        WallIso m = wall_match_cfgs(p->a, p->sides[0]->cfg, p->sides[1]->cfg, WALL_MATCH_BUDGET, kw ? sim : NULL,
                                    kw ? sc : NULL);
        if (m.status == WALL_ISO_BUDGET) {
            if (!n) eq_raise_verdict("NOT PROVEN", -1, NULL, "control-flow graph alignment gave up (budget)");
            continue;
        }
        if (m.status == WALL_ISO_NONE)
            eq_raise_verdict("NOT PROVEN", -1, NULL, "control flow differs (no isomorphism of the block graphs)");
        if (!n || !wall_iso_mapping_equal(&m, &out[0])) out[n++] = m;
    }
    return n;
}

/* ---- _result (L1723-1741) ---- */

static AvpJson *jstr(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }


typedef struct {
    const char *k, *s;
} KeyStr;

static int cmp_keystr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const KeyStr *a = x, *b = y;
    int c = strcmp(a->k, b->k);
    return c ? c : strcmp(a->s, b->s);
}

/* {k: sorted(v) for k, v in sorted(self.pairs_seen.items())} if self.pairs_seen else None */
static AvpJson *pairs_json(Prover *p) {
    AvpArena *a = p->out;
    if (!p->npairs) return avp_json_null();
    KeyStr *v = AVP_NEWN(p->a, KeyStr, p->npairs);
    for (size_t i = 0; i < p->npairs; i++) {
        v[i].k = avp_sprintf(p->a, "%d", p->pairs[i].b);
        v[i].s = p->pairs[i].s;
    }
    avp_sort(v, p->npairs, sizeof *v, cmp_keystr, NULL, false);
    AvpJson *o = avp_json_obj(a);
    AvpJson *cur = NULL;
    for (size_t i = 0; i < p->npairs; i++) {
        if (!cur || strcmp(v[i].k, v[i - 1].k) != 0) {
            cur = avp_json_arr(a);
            avp_json_set(a, o, avp_str(v[i].k), cur);
        } else if (strcmp(v[i].s, v[i - 1].s) == 0) {
            continue;
        }
        avp_json_arr_push(a, cur, avp_json_strz(a, v[i].s));
    }
    return o;
}

/* _result(kind, reason, block, terms, unmodelled) (L1723-1741). block -1 = None, terms NULL = None. */
static AvpJson *result(Prover *p, const char *kind, const char *reason, int block, const EqArg *terms,
                       const char *unmodelled) {
    AvpArena *a = p->out;
    AvpJson *o = avp_json_obj(a);
    bool equiv = strcmp(kind, "EQUIVALENT") == 0;
    avp_json_set(a, o, AVP_S("verdict"), avp_json_strz(a, kind));
    avp_json_set(a, o, AVP_S("reason"), avp_json_strz(a, reason));
    avp_json_set(a, o, AVP_S("block"), block >= 0 ? avp_json_int(a, block) : avp_json_null());
    const char *tt[2] = {NULL, NULL};
    if (terms) {
        AvpJson *arr = avp_json_arr(a);
        for (int k = 0; k < 2; k++) {
            tt[k] = eq_show_arg(a, terms[k], 8);
            avp_json_arr_push(a, arr, avp_json_strz(a, tt[k]));
        }
        avp_json_set(a, o, AVP_S("terms"), arr);
    } else {
        avp_json_set(a, o, AVP_S("terms"), avp_json_null());
    }
    avp_json_set(a, o, AVP_S("unmodelled"), jstr(a, unmodelled));
    if (equiv) {
        AvpJson *arr = avp_json_arr(a);
        for (size_t k = 0; k < p->nassumptions; k++) avp_json_arr_push(a, arr, avp_json_strz(a, p->assumptions[k]));
        avp_json_set(a, o, AVP_S("assumptions"), arr);
    } else {
        avp_json_set(a, o, AVP_S("assumptions"), avp_json_null());
    }
    if (p->mapping && p->mapping->norder) {
        AvpJson *bl = avp_json_obj(a);
        for (size_t k = 0; k < p->mapping->nmap_a; k++)
            if (p->mapping->map[k] >= 0)
                avp_json_set(a, bl, avp_str(avp_sprintf(a, "%zu", k)), avp_json_int(a, p->mapping->map[k]));
        avp_json_set(a, o, AVP_S("blocks"), bl);
    } else {
        avp_json_set(a, o, AVP_S("blocks"), avp_json_null());
    }
    if (p->swaps) {
        AvpJson *arr = avp_json_arr(a);
        for (size_t k = 0; k < p->nswaps; k++) avp_json_arr_push(a, arr, avp_json_int(a, p->swaps[k]));
        avp_json_set(a, o, AVP_S("swapped"), arr);
    } else {
        avp_json_set(a, o, AVP_S("swapped"), avp_json_null());
    }
    avp_json_set(a, o, AVP_S("pairs"), pairs_json(p));
    const char *line;
    if (equiv) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "EQUIVALENT (proved; assumptions: ");
        for (size_t k = 0; k < p->nassumptions; k++) {
            if (k) avp_buf_putz(&b, "; ");
            avp_buf_putz(&b, p->assumptions[k]);
        }
        avp_buf_putz(&b, ")");
        line = avp_buf_cz(&b);
    } else {
        const char *where = block >= 0 ? avp_sprintf(a, "block %d: ", block) : "";
        const char *tx = terms ? avp_sprintf(a, " [retail %s | compiled %s]", tt[0], tt[1]) : "";
        line = avp_sprintf(a, "%s: %s%s%s", kind, where, reason, tx);
    }
    avp_json_set(a, o, AVP_S("line"), avp_json_strz(a, line));
    if (p->explain && avp_json_len(p->explain)) avp_json_set(a, o, AVP_S("explain"), p->explain);
    return o;
}

/* ---- block-entry relation (L1775-1912) ---- */

typedef struct {
    int p, q; /* -1, -1: ("entry", "entry") */
} Edge;

typedef struct {
    EqBlockRun **outs[2]; /* by block of each side; NULL = absent */
} Outs;

/* edges_into(b, outs) (L1775-1786) */
static size_t edges_into(Prover *p, int b, const Outs *o, Edge **out) {
    const WallCfg *g = p->sides[0]->cfg;
    const WallBlock *blk = &g->blocks[b];
    Edge *res = AVP_NEWN(p->a, Edge, blk->npreds + 1);
    size_t n = 0;
    if (b == 0) res[n++] = (Edge){-1, -1};
    int *preds = AVP_NEWN(p->a, int, blk->npreds ? blk->npreds : 1);
    size_t np = blk->npreds;
    if (np) memcpy(preds, blk->preds, np * sizeof *preds);
    avp_sort(preds, np, sizeof *preds, cmp_int, NULL, false);
    for (size_t k = 0; k < np; k++) {
        int pr = preds[k];
        if (k && pr == preds[k - 1]) continue;
        if (g->reachable[pr] && o->outs[0][pr]) {
            int q = map_req(p, pr);
            if (o->outs[1][q]) res[n++] = (Edge){pr, q};
        }
    }
    *out = res;
    return n;
}

static EqNode *read_or_fresh(EqSide *s, EqState *st, i64 off, i64 w) {
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        EqNode *v = eq_side_read_slot(s, st, off, w);
        avp_try_end(&tr);
        return v;
    }
    const EqExc *e = eq_exc_caught(&tr);
    if (e->code != EQ_EXC_NOTPROVEN) eq_reraise(e);
    return eq_side_fresh(s, "mixed");
}

static int cmp_slotkey(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const EqSlotKey *a = x, *b = y;
    if (a->off != b->off) return a->off < b->off ? -1 : 1;
    return (a->w > b->w) - (a->w < b->w);
}

static int cmp_slot(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const EqSlot *a = x, *b = y;
    if (a->off != b->off) return a->off < b->off ? -1 : 1;
    return (a->w > b->w) - (a->w < b->w);
}

/* _partition(keys) (L1868-1888): keys sorted, overlapping ranges split at every boundary. keys is sorted in place;
 * the result is allocated in a. */
static size_t partition(AvpArena *a, EqSlotKey *keys, size_t n, EqSlotKey **out) {
    avp_sort(keys, n, sizeof *keys, cmp_slotkey, NULL, false);
    EqSlotKey *res = NULL;
    size_t nres = 0, cap = 0;
    size_t c0 = 0, c1 = 0; /* the cluster keys[c0..c1) */
    i64 end = 0;
    i64 *cuts = AVP_NEWN(a, i64, 2 * n + 1);
    for (size_t i = 0; i <= n; i++) {
        bool flush = i == n || !(c1 > c0 && keys[i].off < end);
        if (!flush) {
            c1 = i + 1;
            if (keys[i].off + keys[i].w > end) end = keys[i].off + keys[i].w;
            continue;
        }
        if (c1 - c0 == 1) {
            res = grow(a, res, &cap, nres + 1, sizeof *res);
            res[nres++] = keys[c0];
        } else if (c1 > c0) {
            size_t nc = 0;
            for (size_t k = c0; k < c1; k++) {
                cuts[nc++] = keys[k].off;
                cuts[nc++] = keys[k].off + keys[k].w;
            }
            avp_sort_i64(cuts, nc);
            size_t u = 0;
            for (size_t k = 0; k < nc; k++)
                if (!u || cuts[k] != cuts[u - 1]) cuts[u++] = cuts[k];
            for (size_t k = 0; k + 1 < u; k++) {
                res = grow(a, res, &cap, nres + 1, sizeof *res);
                res[nres++] = (EqSlotKey){cuts[k], cuts[k + 1] - cuts[k]};
            }
        }
        if (i < n) {
            c0 = i;
            c1 = i + 1;
            end = keys[i].off + keys[i].w;
        }
    }
    *out = res;
    return nres;
}

/* One side's locations at a block entry: REG_LOCS by index, then the slot keys. */
typedef struct {
    size_t nloc;
    EqSlotKey *keys; /* slot keys (loc EQ_NREG_LOCS + j) */
    size_t nkeys;
    EqNode **vec; /* [nloc * E] */
    EqNode **res; /* [nloc]: result[k][loc], NULL when not set */
} Locs;

typedef struct {
    u64 h;
    EqNode *const *vec; /* the first loc's vector */
    int *locs;
    size_t n, cap;
} Group;

typedef struct {
    Group *v;
    size_t n, cap;
} Groups;

static u64 vec_hash(EqNode *const *vec, size_t E) {
    u64 h = 1469598103934665603ull;
    for (size_t e = 0; e < E; e++) h = (h ^ (u64)(u32)vec[e]->id) * 1099511628211ull;
    return h;
}

static bool vec_ids_eq(EqNode *const *x, EqNode *const *y, size_t E) {
    for (size_t e = 0; e < E; e++)
        if (x[e]->id != y[e]->id) return false;
    return true;
}

static Group *group_find(const Groups *g, u64 h, EqNode *const *vec, size_t E) {
    for (size_t k = 0; k < g->n; k++)
        if (g->v[k].h == h && vec_ids_eq(g->v[k].vec, vec, E)) return &g->v[k];
    return NULL;
}

static const char *loc_str(AvpArena *a, const Locs *L, size_t loc) {
    if (loc < EQ_NREG_LOCS) return EQ_REG_LOCS[loc];
    const EqSlotKey *k = &L->keys[loc - EQ_NREG_LOCS];
    return slot_str(a, k->off, k->w);
}

static void dead_union(AvpArena *a, EqState *dst, const EqState *src) {
    for (size_t i = 0; i < src->ndead; i++) eq_state_dead_add(a, dst, src->dead[i].off, src->dead[i].w);
}

/* in_state(b, outs) (L1788-1865): false for None. */
static bool in_state(Prover *p, int b, const Outs *o, EqState **pair) {
    EqTerms *t = p->t;
    AvpArena *a = p->a;
    Edge *edges;
    size_t E = edges_into(p, b, o, &edges);
    if (!E) return false;
    if (E == 1 && edges[0].p < 0) {
        EqState *st = eq_state_entry(t);
        pair[0] = st;
        pair[1] = eq_state_copy(a, st);
        return true;
    }
    if (E == 1) {
        pair[0] = eq_state_copy(a, o->outs[0][edges[0].p]->state);
        pair[1] = eq_state_copy(a, o->outs[1][edges[0].q]->state);
        return true;
    }
    EqState **states[2];
    for (int k = 0; k < 2; k++) {
        states[k] = AVP_NEWN(a, EqState *, E);
        for (size_t e = 0; e < E; e++) {
            int blk = k ? edges[e].q : edges[e].p;
            states[k][e] = blk < 0 ? eq_state_entry(t) : o->outs[k][blk]->state;
        }
    }
    Locs L[2];
    for (int k = 0; k < 2; k++) {
        EqSide *side = p->sides[k];
        /* keys = set of every state's slot keys, dead = the union of their dead words */
        size_t nall = 0;
        for (size_t e = 0; e < E; e++) nall += states[k][e]->nslots;
        EqSlotKey *keys = AVP_NEWN(a, EqSlotKey, nall ? nall : 1);
        size_t nk = 0;
        EqState *dead = eq_state_new(a);
        for (size_t e = 0; e < E; e++) {
            const EqState *s = states[k][e];
            for (size_t i = 0; i < s->nslots; i++) {
                bool dup = false;
                for (size_t j = 0; j < nk && !dup; j++)
                    dup = keys[j].off == s->slots[i].off && keys[j].w == s->slots[i].w;
                if (!dup) keys[nk++] = (EqSlotKey){s->slots[i].off, s->slots[i].w};
            }
            dead_union(a, dead, s);
        }
        size_t nf = 0;
        for (size_t j = 0; j < nk; j++) {
            bool hit = false;
            for (size_t d = 0; d < dead->ndead && !hit; d++) {
                i64 o2 = dead->dead[d].off, w2 = dead->dead[d].w;
                hit = o2 < keys[j].off + keys[j].w && keys[j].off < o2 + w2;
            }
            if (!hit) keys[nf++] = keys[j];
        }
        L[k].nkeys = partition(a, keys, nf, &L[k].keys);
        L[k].nloc = EQ_NREG_LOCS + L[k].nkeys;
        L[k].vec = AVP_NEWN(a, EqNode *, L[k].nloc * E);
        L[k].res = AVP_NEWN(a, EqNode *, L[k].nloc);
        for (size_t loc = 0; loc < EQ_NREG_LOCS; loc++)
            for (size_t e = 0; e < E; e++) {
                EqNode *v = states[k][e]->r[loc];
                AVP_ASSERT(v);
                L[k].vec[loc * E + e] = v;
            }
        for (size_t j = 0; j < L[k].nkeys; j++)
            for (size_t e = 0; e < E; e++)
                L[k].vec[(EQ_NREG_LOCS + j) * E + e] =
                    read_or_fresh(side, states[k][e], L[k].keys[j].off, L[k].keys[j].w);
    }
    /* facts, and groups of locations by their id vectors (insertion order) */
    Groups G[2] = {{0}, {0}};
    for (int k = 0; k < 2; k++) {
        for (size_t loc = 0; loc < L[k].nloc; loc++) {
            EqNode *const *vec = &L[k].vec[loc * E];
            bool same = true;
            for (size_t e = 1; e < E && same; e++) same = vec[e] == vec[0];
            if (same && vec[0]->glob) {
                L[k].res[loc] = vec[0];
                continue;
            }
            u64 h = vec_hash(vec, E);
            Group *g = group_find(&G[k], h, vec, E);
            if (!g) {
                G[k].v = grow(a, G[k].v, &G[k].cap, G[k].n + 1, sizeof *G[k].v);
                g = &G[k].v[G[k].n++];
                memset(g, 0, sizeof *g);
                g->h = h;
                g->vec = vec;
            }
            g->locs = grow(a, g->locs, &g->cap, g->n + 1, sizeof *g->locs);
            g->locs[g->n++] = (int)loc;
        }
    }
    for (size_t gi = 0; gi < G[0].n; gi++) {
        const Group *g0 = &G[0].v[gi];
        const Group *g1 = group_find(&G[1], g0->h, g0->vec, E);
        if (!g1) continue;
        const char *name = NULL;
        for (size_t i = 0; i < g0->n; i++) {
            const char *s = loc_str(a, &L[0], (size_t)g0->locs[i]);
            if (!name || strcmp(s, name) < 0) name = s;
        }
        EqArg args[3] = {eq_ai(b), eq_as("p"), eq_as(name)};
        EqNode *v = eq_leaf(t, EQ_OP_BV, args, 3, false, false, false, EQ_MASK);
        for (size_t i = 0; i < g0->n; i++) L[0].res[g0->locs[i]] = v;
        for (size_t i = 0; i < g1->n; i++) L[1].res[g1->locs[i]] = v;
    }
    for (int k = 0; k < 2; k++) {
        const char *side = p->sides[k]->name;
        for (size_t gi = 0; gi < G[k].n; gi++) {
            const Group *g = &G[k].v[gi];
            for (size_t i = 0; i < g->n; i++) {
                size_t loc = (size_t)g->locs[i];
                if (L[k].res[loc]) continue;
                EqArg args[3] = {eq_ai(b), eq_as(side), eq_as(loc_str(a, &L[k], loc))};
                L[k].res[loc] = eq_leaf(t, EQ_OP_BV, args, 3, false, true, false, EQ_MASK);
            }
        }
    }
    /* memory and frame */
    EqNode **mems[2];
    for (int k = 0; k < 2; k++) {
        mems[k] = AVP_NEWN(a, EqNode *, E);
        for (size_t e = 0; e < E; e++) mems[k][e] = eq_side_mem_now(p->sides[k], states[k][e]);
    }
    bool same = true;
    for (size_t e = 0; e < E && same; e++) same = mems[0][e] == mems[1][e] && mems[0][e] == mems[0][0];
    EqNode *mem;
    if (same && mems[0][0]->glob) {
        mem = mems[0][0];
    } else {
        EqArg args[2] = {eq_ai(b), eq_as("mem")};
        mem = eq_leaf(t, EQ_OP_BV, args, 2, false, false, false, EQ_MASK);
    }
    bool all_none = true;
    for (int k = 0; k < 2; k++)
        for (size_t e = 0; e < E; e++) all_none = all_none && !states[k][e]->frame;
    EqNode *frame;
    if (all_none) {
        frame = NULL;
    } else {
        EqNode *f0 = states[0][0]->frame;
        same = true;
        for (size_t e = 0; e < E && same; e++)
            same = states[0][e]->frame == states[1][e]->frame && states[0][e]->frame == f0;
        if (same && f0->glob) {
            frame = f0;
        } else {
            EqArg args[2] = {eq_ai(b), eq_as("frame")};
            frame = eq_leaf(t, EQ_OP_BV, args, 2, false, false, false, EQ_MASK);
        }
    }
    for (int k = 0; k < 2; k++) {
        EqState *st = eq_state_new(a);
        for (size_t loc = 0; loc < EQ_NREG_LOCS; loc++) st->r[loc] = L[k].res[loc];
        for (size_t j = 0; j < L[k].nkeys; j++)
            eq_state_slot_set(a, st, L[k].keys[j].off, L[k].keys[j].w, L[k].res[EQ_NREG_LOCS + j]);
        st->mem = mem;
        st->frame = frame;
        st->ncall = 1000 * ((i64)b + 1);
        for (size_t e = 0; e < E; e++) dead_union(a, st, states[k][e]);
        pair[k] = st;
    }
    return true;
}

/* _sig(pair) (L1890-1894) as a flat vector (equal exactly when the Python's tuples are). */
static i64 *sig_of(AvpArena *a, EqState *const *pair, size_t *n) {
    size_t cap = 0;
    for (int k = 0; k < 2; k++) cap += EQ_NREG_LOCS + 3 * pair[k]->nslots + 2 * pair[k]->ndead + 4;
    i64 *v = AVP_NEWN(a, i64, cap);
    size_t m = 0;
    for (int k = 0; k < 2; k++) {
        const EqState *s = pair[k];
        for (size_t loc = 0; loc < EQ_NREG_LOCS; loc++) v[m++] = s->r[loc]->id;
        EqSlot *sl = AVP_NEWN(a, EqSlot, s->nslots ? s->nslots : 1);
        if (s->nslots) memcpy(sl, s->slots, s->nslots * sizeof *sl);
        avp_sort(sl, s->nslots, sizeof *sl, cmp_slot, NULL, false);
        v[m++] = (i64)s->nslots;
        for (size_t i = 0; i < s->nslots; i++) {
            v[m++] = sl[i].off;
            v[m++] = sl[i].w;
            v[m++] = sl[i].v->id;
        }
        v[m++] = s->mem->id;
        v[m++] = s->frame ? s->frame->id : -1;
        EqSlotKey *d = AVP_NEWN(a, EqSlotKey, s->ndead ? s->ndead : 1);
        if (s->ndead) memcpy(d, s->dead, s->ndead * sizeof *d);
        avp_sort(d, s->ndead, sizeof *d, cmp_slotkey, NULL, false);
        v[m++] = (i64)s->ndead;
        for (size_t i = 0; i < s->ndead; i++) {
            v[m++] = d[i].off;
            v[m++] = d[i].w;
        }
    }
    *n = m;
    return v;
}

typedef struct {
    Outs o;
    EqState ***ins; /* [nblocks of retail]: the pair at the block's entry, NULL when never set */
} Fix;

/* fixpoint() (L1896-1917) */
static Fix *fixpoint(Prover *p) {
    EqSide *R = p->sides[0], *C = p->sides[1];
    AvpArena *a = p->a;
    int *order;
    size_t norder = eq_rpo(a, R->cfg, &order);
    size_t nr = R->cfg->nblocks ? R->cfg->nblocks : 1, nc = C->cfg->nblocks ? C->cfg->nblocks : 1;
    Fix *f = AVP_NEW(a, Fix);
    f->o.outs[0] = AVP_NEWN(a, EqBlockRun *, nr);
    f->o.outs[1] = AVP_NEWN(a, EqBlockRun *, nc);
    f->ins = AVP_NEWN(a, EqState **, nr);
    i64 **sigs = AVP_NEWN(a, i64 *, nr);
    size_t *nsigs = AVP_NEWN(a, size_t, nr);
    for (int round = 0; round < p->max_rounds; round++) {
        bool changed = false;
        for (size_t oi = 0; oi < norder; oi++) {
            int b = order[oi];
            EqState **pair = AVP_NEWN(a, EqState *, 2);
            if (!in_state(p, b, &f->o, pair)) continue;
            size_t ns;
            i64 *sig = sig_of(a, pair, &ns);
            if (sigs[b] && nsigs[b] == ns && memcmp(sigs[b], sig, ns * sizeof *sig) == 0 && f->o.outs[0][b]) continue;
            changed = true;
            sigs[b] = sig;
            nsigs[b] = ns;
            f->ins[b] = pair;
            int cb = map_req(p, b);
            f->o.outs[0][b] = eq_side_run_block(R, b, pair[0]);
            f->o.outs[1][cb] = eq_side_run_block(C, cb, pair[1]);
        }
        if (!changed) return f;
    }
    eq_raise(EQ_EXC_NOTPROVEN, "block-entry relation did not converge in %d rounds", p->max_rounds);
}

/* ---- checks (L1920-2108) ---- */

/* _has_op(n, op) (L2112-2122) */
static bool has_op(Prover *p, EqNode *n, int op) {
    size_t cap = 64, sp = 0;
    EqNode **stack = AVP_NEWN(p->a, EqNode *, cap);
    size_t nt = eq_terms_len(p->t);
    u8 *seen = AVP_NEWN(p->a, u8, nt ? nt : 1);
    stack[sp++] = n;
    while (sp) {
        EqNode *x = stack[--sp];
        if (x->op == op) return true;
        if (seen[x->id] || !x->local) continue;
        seen[x->id] = 1;
        for (int k = 0; k < x->nargs; k++)
            if (x->args[k].kind == EQ_ARG_NODE) {
                stack = grow(p->a, stack, &cap, sp + 1, sizeof *stack);
                stack[sp++] = x->args[k].n;
            }
    }
    return false;
}

/* mismatch(b, what, a, c) (L1920-1932): always raises a Verdict. */
static AVP_NORETURN void mismatch(Prover *p, int b, const char *what, EqNode *a, EqNode *c) {
    EqArg terms[2] = {eq_an(a), eq_an(c)};
    EqArgPair d = eq_first_diff(terms[0], terms[1]);
    if (as_node(d.a)->op == EQ_OP_CSYM || as_node(d.b)->op == EQ_OP_CSYM || has_op(p, a, EQ_OP_CSYM) ||
        has_op(p, c, EQ_OP_CSYM))
        eq_raise_verdict("NOT PROVEN", b, terms,
                         "%s: a compiled relocation target is not resolved to a retail address", what);
    EqArgPair *path;
    size_t np = eq_diff_path(p->a, terms[0], terms[1], &path);
    for (size_t k = 0; k < np; k++) {
        const EqNode *x2 = as_node(path[k].a);
        if (x2->op != EQ_OP_LD) continue;
        const EqNode *y2 = as_node(path[k].b);
        if (y2->op == EQ_OP_LD && !eq_arg_differs(x2->args[0], y2->args[0]) && arg_is(x2->args[1], y2->args[1]))
            eq_raise_verdict("NOT PROVEN", b, terms, "%s: a load of %s moves across a store that may alias it", what,
                             eq_show_arg(p->a, x2->args[1], EQ_SHOW_DEPTH));
    }
    if (a->local || c->local)
        eq_raise_verdict("NOT PROVEN", b, terms, "%s: values not shown equal (a side-specific value reaches it)",
                         what);
    eq_raise_verdict("DIFFERENT", b, terms, "%s", what);
}

/* The Verdict e raised again with another kind and a suffix on its reason (`v.kind = ...; v.reason += ...;
 * raise`). */
static AVP_NORETURN void reraise_verdict(Prover *p, const EqExc *e, const char *kind, const char *suffix) {
    const char *msg = avp_sprintf(p->a, "%s%s", e->msg, suffix);
    EqArg terms[2] = {{0}, {0}};
    bool has = e->has_terms;
    if (has) {
        terms[0] = e->terms[0];
        terms[1] = e->terms[1];
    }
    int block = e->block;
    eq_raise_verdict(kind, block, has ? terms : NULL, "%s", msg);
}

/* record_pairs(ins) (L1934-1948) */
typedef struct {
    int id;
    int reg; /* REG_LOCS index, or -1 for a slot */
    i64 off, w;
} VarLoc;

static void record_pairs(Prover *p, EqState ***ins, size_t nblocks) {
    for (size_t b = 0; b < nblocks; b++) {
        EqState **pair = ins[b];
        if (!pair) continue;
        /* the locations of each side whose value is a shared ("p") block variable */
        VarLoc *vl[2];
        size_t nvl[2];
        for (int k = 0; k < 2; k++) {
            const EqState *st = pair[k];
            vl[k] = AVP_NEWN(p->a, VarLoc, EQ_NREG_LOCS + st->nslots);
            nvl[k] = 0;
            for (size_t i = 0; i < EQ_NREG_LOCS + st->nslots; i++) {
                const EqNode *v = i < EQ_NREG_LOCS ? st->r[i] : st->slots[i - EQ_NREG_LOCS].v;
                if (v->op != EQ_OP_BV || v->nargs < 2 || v->args[1].kind != EQ_ARG_STR || strcmp(v->args[1].s, "p"))
                    continue;
                VarLoc x = {v->id, i < EQ_NREG_LOCS ? (int)i : -1, 0, 0};
                if (i >= EQ_NREG_LOCS) {
                    x.off = st->slots[i - EQ_NREG_LOCS].off;
                    x.w = st->slots[i - EQ_NREG_LOCS].w;
                }
                vl[k][nvl[k]++] = x;
            }
        }
        for (size_t i = 0; i < nvl[0]; i++)
            for (size_t j = 0; j < nvl[1]; j++) {
                const VarLoc *la = &vl[0][i], *lb = &vl[1][j];
                if (la->id != lb->id) continue;
                bool equal = la->reg >= 0 ? la->reg == lb->reg : (lb->reg < 0 && la->off == lb->off && la->w == lb->w);
                if (equal) continue;
                const char *na = la->reg >= 0 ? EQ_REG_LOCS[la->reg] : eq_slot_locname(p->a, la->off, la->w);
                const char *nb = lb->reg >= 0 ? EQ_REG_LOCS[lb->reg] : eq_slot_locname(p->a, lb->off, lb->w);
                p->pairs = grow(p->a, p->pairs, &p->pcap, p->npairs + 1, sizeof *p->pairs);
                p->pairs[p->npairs++] = (PairRec){(int)b, avp_sprintf(p->a, "%s=%s", na, nb)};
            }
    }
}

/* _rowspan(side, b) (L2129-2131) */
static AvpJson *rowspan(AvpArena *a, const EqSide *s, int b) {
    const WallBlock *blk = &s->cfg->blocks[b];
    AvpJson *arr = avp_json_arr(a);
    avp_json_arr_push(a, arr, avp_json_strz(a, avp_sprintf(a, "0x%llX", (unsigned long long)s->rows[blk->start].addr)));
    avp_json_arr_push(a, arr,
                      avp_json_strz(a, avp_sprintf(a, "0x%llX", (unsigned long long)s->rows[blk->end - 1].addr)));
    return arr;
}

static const char *eff_kind(const EqEffect *e) {
    return e->kind == EQ_EFF_CALL ? "call" : e->kind == EQ_EFF_IO ? "io" : "st";
}

/* _eff_text(e) (L2134-2142) */
static const char *eff_text(AvpArena *a, const EqEffect *e) {
    const EqNode *n = e->n;
    if (e->kind == EQ_EFF_ST)
        return avp_sprintf(a, "store%s %s <- %s", eq_str_arg(a, n->args[0]), eq_show_arg(a, n->args[1], 5),
                           eq_show_arg(a, n->args[2], 5));
    if (e->kind == EQ_EFF_CALL) {
        const EqNode *args = as_node(n->args[1]);
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "call %s (", eq_show_arg(a, n->args[0], 3));
        for (int k = 0; k + 1 < args->nargs; k += 2) {
            if (k) avp_buf_putz(&b, ", ");
            avp_buf_printf(&b, "%s=%s", eq_str_arg(a, args->args[k]), eq_show_arg(a, args->args[k + 1], 3));
        }
        avp_buf_putz(&b, ")");
        return avp_buf_cz(&b);
    }
    return avp_sprintf(a, "%s %s", eff_kind(e), e->text ? e->text : "None");
}

/* compare_stores(b, a, c) (L2034-2066) */
static void compare_stores(Prover *p, int b, EqNode *const *a, size_t na, EqNode *const *c, size_t nc) {
    const EqSide *side = p->sides[0];
    AvpArena *ar = p->a;
    i64 *ia = AVP_NEWN(ar, i64, na + 1), *ic = AVP_NEWN(ar, i64, nc + 1);
    for (size_t i = 0; i < na; i++) ia[i] = a[i]->id;
    for (size_t i = 0; i < nc; i++) ic[i] = c[i]->id;
    i64 *sa = avp_memdup(ar, ia, (na + 1) * sizeof *ia), *sc = avp_memdup(ar, ic, (nc + 1) * sizeof *ic);
    avp_sort_i64(sa, na);
    avp_sort_i64(sc, nc);
    if (na != nc || memcmp(sa, sc, na * sizeof *sa) != 0) {
        EqNode **onlya = AVP_NEWN(ar, EqNode *, na + 1), **onlyc = AVP_NEWN(ar, EqNode *, nc + 1);
        size_t noa = 0, noc = 0;
        for (size_t i = 0; i < na; i++) {
            bool in = false;
            for (size_t j = 0; j < nc && !in; j++) in = a[i] == c[j];
            if (!in) onlya[noa++] = a[i];
        }
        for (size_t i = 0; i < nc; i++) {
            bool in = false;
            for (size_t j = 0; j < na && !in; j++) in = c[i] == a[j];
            if (!in) onlyc[noc++] = c[i];
        }
        for (size_t i = 0; i < noa; i++)
            for (size_t j = 0; j < noc; j++) {
                EqNode *x = onlya[i], *y = onlyc[j];
                if (arg_is(x->args[1], y->args[1]) && !eq_arg_differs(x->args[0], y->args[0]))
                    mismatch(p, b, avp_sprintf(ar, "value stored to %s", eq_show_arg(ar, x->args[1], EQ_SHOW_DEPTH)),
                             x->args[2].n, y->args[2].n);
            }
        if (noa && noc) {
            EqNode *x = onlya[0], *y = onlyc[0];
            if (arg_is(x->args[1], y->args[1]) && !eq_arg_differs(x->args[0], y->args[0]))
                mismatch(p, b, avp_sprintf(ar, "value stored to %s", eq_show_arg(ar, x->args[1], EQ_SHOW_DEPTH)),
                         x->args[2].n, y->args[2].n);
            if (arg_is(x->args[2], y->args[2]) && !eq_arg_differs(x->args[0], y->args[0]))
                mismatch(p, b,
                         avp_sprintf(ar, "address of a store of %s", eq_show_arg(ar, x->args[2], EQ_SHOW_DEPTH)),
                         x->args[1].n, y->args[1].n);
            mismatch(p, b, "store", x, y);
        }
        if (na != nc || noa || noc) {
            EqNode *extra = noa ? onlya[0] : noc ? onlyc[0] : NULL;
            const char *to = extra ? avp_sprintf(ar, " to %s", eq_show_arg(ar, extra->args[1], EQ_SHOW_DEPTH)) : "";
            eq_raise_verdict("DIFFERENT", b, NULL, "%s side has an extra store%s",
                             noa || na > nc ? "retail" : "compiled", to);
        }
        eq_raise_verdict("DIFFERENT", b, NULL, "the same stores occur a different number of times");
    }
    if (memcmp(ia, ic, na * sizeof *ia) == 0) return;
    for (size_t i = 1; i < na; i++)
        if (sa[i] == sa[i - 1]) eq_raise_verdict("NOT PROVEN", b, NULL, "a repeated store is reordered");
    /* pos = {x.id: k for k, x in enumerate(c)} (ids are distinct here) */
    size_t *pos = AVP_NEWN(ar, size_t, na + 1);
    for (size_t i = 0; i < na; i++)
        for (size_t k = 0; k < nc; k++)
            if (c[k] == a[i]) pos[i] = k;
    for (size_t i = 0; i < na; i++)
        for (size_t j = i + 1; j < na; j++) {
            EqNode *x = a[i], *y = a[j];
            if (pos[i] > pos[j] && eq_side_may_alias(side, x->args[1].n, x->args[0].i, y->args[1].n, y->args[0].i)) {
                EqArg terms[2] = {eq_an(x), eq_an(y)};
                eq_raise_verdict("NOT PROVEN", b, terms, "stores to %s and %s are reordered and may alias",
                                 eq_show_arg(ar, x->args[1], EQ_SHOW_DEPTH), eq_show_arg(ar, y->args[1], EQ_SHOW_DEPTH));
            }
        }
}

/* segs(effs) of compare_effects: the store runs between the other effects ("bars"). Segment i is
 * st[bound[i] .. bound[i + 1]), for i in 0..nbars. */
typedef struct {
    EqNode **st;
    size_t *bound;
    const EqEffect **bars;
    size_t nbars;
} Segs;

static Segs segs(AvpArena *a, const EqBlockRun *run) {
    Segs s;
    size_t n = run->neffects;
    s.st = AVP_NEWN(a, EqNode *, n + 1);
    s.bound = AVP_NEWN(a, size_t, n + 2);
    s.bars = AVP_NEWN(a, const EqEffect *, n + 1);
    s.nbars = 0;
    size_t nst = 0;
    for (size_t i = 0; i < n; i++) {
        const EqEffect *e = &run->effects[i];
        if (e->kind == EQ_EFF_ST) {
            s.st[nst++] = e->n;
        } else {
            s.bars[s.nbars++] = e;
            s.bound[s.nbars] = nst;
        }
    }
    s.bound[0] = 0;
    s.bound[s.nbars + 1] = nst;
    return s;
}

/* mismatch of a call argument the callee's prototype does not declare: a DIFFERENT verdict becomes NOT PROVEN */
static AVP_NORETURN void mismatch_undeclared(Prover *p, int b, const char *what, EqNode *a, EqNode *c) {
    AvpTry tr;
    if (AVP_TRY(&tr)) mismatch(p, b, what, a, c);
    const EqExc *e = eq_exc_caught(&tr);
    if (e->code == EQ_EXC_VERDICT && strcmp(e->vkind, "DIFFERENT") == 0)
        reraise_verdict(p, e, "NOT PROVEN", " (a register the callee may read; its prototype does not declare it)");
    eq_reraise(e);
}

/* compare_effects(b, er, ec) (L1993-2032) */
static void compare_effects(Prover *p, int b, const EqBlockRun *rr, const EqBlockRun *cr) {
    AvpArena *ar = p->a;
    Segs sr = segs(ar, rr), sc = segs(ar, cr);
    size_t nbr = sr.nbars, nbc = sc.nbars, top = nbr > nbc ? nbr : nbc;
    for (size_t i = 0; i <= top; i++) {
        if (i <= nbr && i <= nbc)
            compare_stores(p, b, sr.st + sr.bound[i], sr.bound[i + 1] - sr.bound[i], sc.st + sc.bound[i],
                           sc.bound[i + 1] - sc.bound[i]);
        if (i >= nbr && i >= nbc) break;
        if (i >= nbr || i >= nbc) {
            const EqEffect *extra = i < nbr ? sr.bars[i] : sc.bars[i];
            eq_raise_verdict("DIFFERENT", b, NULL, "%s side has an extra %s (%s)", i < nbr ? "retail" : "compiled",
                             eff_kind(extra), extra->text ? extra->text : "None");
        }
        const EqEffect *x = sr.bars[i], *y = sc.bars[i];
        if (x->n == y->n) continue;
        if (x->n->op == EQ_OP_CALL && y->n->op == EQ_OP_CALL) {
            EqArg tx = x->n->args[0], ty = y->n->args[0];
            if (!arg_is(tx, ty)) {
                EqArg terms[2] = {tx, ty};
                eq_raise_verdict("DIFFERENT", b, terms, "call %zu: different callee (%s vs %s)", i + 1, x->text,
                                 y->text);
            }
            EqArg axa = x->n->args[1], aya = y->n->args[1];
            if (!arg_is(axa, aya)) {
                const EqNode *ax = as_node(axa), *ay = as_node(aya);
                for (int k = 0; k + 1 < ax->nargs; k += 2) {
                    EqArg reg = ax->args[k];
                    /* da = dict(...): the first occurrence of a key gives its position, the last its value */
                    bool first = true;
                    for (int q = 0; q < k && first; q += 2) first = eq_arg_differs(ax->args[q], reg);
                    if (!first) continue;
                    EqArg va = ax->args[k + 1];
                    for (int q = k + 2; q + 1 < ax->nargs; q += 2)
                        if (!eq_arg_differs(ax->args[q], reg)) va = ax->args[q + 1];
                    bool in_c = false;
                    EqArg vc = {0};
                    for (int q = 0; q + 1 < ay->nargs; q += 2)
                        if (!eq_arg_differs(ay->args[q], reg)) in_c = true, vc = ay->args[q + 1];
                    if (!in_c || arg_is(va, vc)) continue;
                    const char *rname = eq_str_arg(ar, reg);
                    const char *what = avp_sprintf(ar, "call %zu (%s): argument %s", i + 1, x->text, rname);
                    bool declared = reg.kind == EQ_ARG_STR && (eq_abitz(reg.s) & x->declared);
                    if (!declared && strcmp(rname, "cr1[eq]") != 0)
                        mismatch_undeclared(p, b, what, as_node(va) ? va.n : NULL, as_node(vc) ? vc.n : NULL);
                    mismatch(p, b, what, as_node(va) ? va.n : NULL, as_node(vc) ? vc.n : NULL);
                }
                eq_raise_verdict("NOT PROVEN", b, NULL, "call %zu (%s): argument registers differ", i + 1, x->text);
            }
            if (!arg_is(x->n->args[2], y->n->args[2]))
                mismatch(p, b, avp_sprintf(ar, "call %zu (%s): memory before the call", i + 1, x->text),
                         as_node(x->n->args[2]) ? x->n->args[2].n : NULL, as_node(y->n->args[2]) ? y->n->args[2].n : NULL);
            if (x->n->nargs > 3) {
                EqArg terms[2] = {x->n->args[3], y->n->nargs > 3 ? y->n->args[3] : eq_ai(0)};
                if (y->n->nargs <= 3) eq_raise(EQ_EXC_INDEX, "tuple index out of range");
                eq_raise_verdict("NOT PROVEN", b, terms,
                                 "call %zu (%s): frame contents passed to the callee differ (escaped frame)", i + 1,
                                 x->text);
            }
        }
        mismatch(p, b, avp_sprintf(ar, "%s %zu", eff_kind(x), i + 1), x->n, y->n);
    }
}

/* check_exit(b, rr, cr) (L2068-2098) */
static void check_exit(Prover *p, int b, const EqBlockRun *rr, const EqBlockRun *cr) {
    EqTerms *t = p->t;
    const EqBlockRun *runs[2] = {rr, cr};
    for (int k = 0; k < 2; k++) {
        const EqState *st = runs[k]->state;
        for (int j = 0; j < EQ_NCALLEE_SAVED; j++) {
            const char *loc = EQ_CALLEE_SAVED[j];
            EqNode *v = st->r[reg_index(loc)];
            AVP_ASSERT(v);
            EqNode *e1 = eq_entry(t, loc);
            if (v != e1) {
                EqNode *e2 = eq_entry(t, loc);
                EqArg terms[2] = {eq_an(v), eq_an(e2)};
                eq_raise_verdict("NOT PROVEN", b, terms, "%s side does not restore %s at the exit", p->sides[k]->name,
                                 loc);
            }
        }
    }
    if (rr->exit_tail || cr->exit_tail) {
        if (!(rr->exit_tail && cr->exit_tail)) eq_raise_verdict("DIFFERENT", b, NULL, "a tail call on one side only");
        return; /* the call effect already compared */
    }
    for (int k = 0; k < p->nret; k++) {
        const char *reg = p->ret[k];
        int idx = reg_index(reg);
        EqNode *a = rr->state->r[idx], *c = cr->state->r[idx];
        AVP_ASSERT(a && c);
        if (a->sp || c->sp) eq_raise(EQ_EXC_NOTPROVEN, "a frame address is returned");
        if (a == c) continue;
        AvpTry tr;
        if (AVP_TRY(&tr)) mismatch(p, b, avp_sprintf(p->a, "return value %s", reg), a, c);
        const EqExc *e = eq_exc_caught(&tr);
        if (e->code != EQ_EXC_VERDICT) eq_reraise(e);
        p->ret_reg = reg;
        if (!p->ret_explicit)
            reraise_verdict(p, e, "NOT PROVEN", " (the return type is unknown: --ret names the registers that matter)");
        eq_reraise(e);
    }
}

/* check(ins, outs, explain) (L1950-1991) */
static void check(Prover *p, const Fix *f, bool explain) {
    EqSide *R = p->sides[0], *C = p->sides[1];
    AvpArena *ar = p->a;
    record_pairs(p, f->ins, R->cfg->nblocks);
    int *order;
    size_t norder = eq_rpo(ar, R->cfg, &order);
    for (size_t oi = 0; oi < norder; oi++) {
        int b = order[oi];
        if (!f->o.outs[0][b]) eq_raise(EQ_EXC_NOTPROVEN, "block %d was never reached by the relation", b);
        int cb = map_req(p, b);
        const EqBlockRun *rr = f->o.outs[0][b], *cr = f->o.outs[1][cb];
        AVP_ASSERT(cr);
        if (explain) {
            AvpArena *a = p->out;
            AvpJson *d = avp_json_obj(a);
            avp_json_set(a, d, AVP_S("retail_block"), avp_json_int(a, b));
            avp_json_set(a, d, AVP_S("compiled_block"), avp_json_int(a, cb));
            avp_json_set(a, d, AVP_S("retail_rows"), rowspan(a, R, b));
            avp_json_set(a, d, AVP_S("compiled_rows"), rowspan(a, C, cb));
            const EqBlockRun *runs[2] = {rr, cr};
            for (int k = 0; k < 2; k++) {
                AvpJson *arr = avp_json_arr(a);
                for (size_t i = 0; i < runs[k]->neffects; i++)
                    avp_json_arr_push(a, arr, avp_json_strz(a, eff_text(a, &runs[k]->effects[i])));
                avp_json_set(a, d, k ? AVP_S("compiled") : AVP_S("retail"), arr);
            }
            AvpJson *cond = avp_json_arr(a);
            for (int k = 0; k < 2; k++)
                avp_json_arr_push(a, cond, runs[k]->cond ? avp_json_strz(a, eq_show(a, runs[k]->cond, 8)) : avp_json_null());
            avp_json_set(a, d, AVP_S("cond"), cond);
            avp_json_arr_push(a, p->explain, d);
        }
        compare_effects(p, b, rr, cr);
        const WallBlock *blk = &R->cfg->blocks[b], *cblk = &C->cfg->blocks[cb];
        WallKind kind = blk->kind;
        if (kind == WALL_K_COND) {
            int tr = blk->succ[0], fr = blk->nsucc > 1 ? blk->succ[1] : -1;
            int tc = cblk->nsucc ? cblk->succ[0] : -1, fc = cblk->nsucc > 1 ? cblk->succ[1] : -1;
            if (tr == fr) {
            } else if (map_get(p, tr) == tc && map_get(p, fr) == fc) {
                if (rr->cond != cr->cond) mismatch(p, b, "branch condition", rr->cond, cr->cond);
            } else if (map_get(p, tr) == fc && map_get(p, fr) == tc) {
                EqNode *nc = eq_bnot(p->t, cr->cond);
                if (rr->cond != nc) {
                    EqNode *nc2 = eq_bnot(p->t, cr->cond);
                    mismatch(p, b, "branch condition (successors crossed)", rr->cond, nc2);
                }
            } else {
                eq_raise(EQ_EXC_NOTPROVEN, "block %d: successors do not correspond", b);
            }
        } else if (kind == WALL_K_CONDRET || kind == WALL_K_CONDTAIL) {
            if (rr->cond != cr->cond)
                mismatch(p, b,
                         avp_sprintf(ar, "conditional %s condition", kind == WALL_K_CONDRET ? "return" : "tail call"),
                         rr->cond, cr->cond);
            check_exit(p, b, rr, cr);
        } else if (kind == WALL_K_RET || kind == WALL_K_TAIL) {
            check_exit(p, b, rr, cr);
        }
    }
}

/* ---- _prove and prove (L1631-1773) ---- */

static void run_or_skip(EqSide *s, int b, EqTerms *t) {
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        eq_side_run_block(s, b, eq_state_entry(t));
        avp_try_end(&tr);
        return;
    }
    const EqExc *e = eq_exc_caught(&tr);
    if (e->code != EQ_EXC_NOTPROVEN) eq_reraise(e);
}

/* _prove(explain) (L1743-1759) */
static void prove_inner(Prover *p, bool explain) {
    EqSide *R = p->sides[0], *C = p->sides[1];
    Fix *f = NULL;
    for (int escaped = 0; escaped < 2; escaped++) {
        R->escaped = C->escaped = escaped != 0;
        R->wants_escape = C->wants_escape = false;
        f = fixpoint(p);
        if (escaped || !(R->wants_escape || C->wants_escape)) break;
    }
    if (R->escaped) p->assumptions[p->nassumptions++] = EQ_ASSUMPTIONS[2];
    check(p, f, explain);
    /* unreachable blocks: never executed, but their instructions must still be modelled */
    for (int k = 0; k < 2; k++) {
        EqSide *s = p->sides[k];
        for (size_t b = 0; b < s->cfg->nblocks; b++)
            if (!s->cfg->reachable[b]) run_or_skip(s, (int)b, p->t);
    }
}

typedef struct {
    bool set, tuple;
    EqExc *e;
    const WallIso *mapping;
    int *swaps;
    size_t nswaps;
    AvpJson *explain;
    const char *ret_reg;
} First;

static AvpJson *prove(Prover *p, bool explain);

/* The "equivalent except the return register" retry of prove (L1652-1667); NULL when it does not apply. */
static AvpJson *ret_only_retry(Prover *p, const First *first) {
    const char *reg = first->ret_reg;
    if (!reg || p->ret_explicit || p->nret <= 1) return NULL;
    const char **rest = AVP_NEWN(p->a, const char *, p->nret);
    int nrest = 0;
    for (int k = 0; k < p->nret; k++)
        if (strcmp(p->ret[k], reg) != 0) rest[nrest++] = p->ret[k];
    Prover sub = *p;
    sub.ret = rest;
    sub.nret = nrest;
    sub.ret_explicit = false;
    AvpJson *r2 = prove(&sub, false);
    AvpJson *only = avp_json_getz(r2, "ret_only");
    bool equiv = avp_str_eqz(avp_json_get_str(r2, "verdict", AVP_S("")), "EQUIVALENT");
    if (!equiv && !avp_json_truthy(only)) return NULL;
    const char **regs = AVP_NEWN(p->a, const char *, (size_t)p->nret + 1 + (only ? avp_json_len(only) : 0));
    size_t nregs = 0;
    regs[nregs++] = reg;
    if (avp_json_truthy(only))
        for (size_t k = 0; k < avp_json_len(only); k++)
            regs[nregs++] = avp_cz(p->a, avp_json_str_or(avp_json_at(only, k), AVP_S("")));
    AvpBuf rb, kb;
    avp_buf_init(&rb, p->a);
    avp_buf_init(&kb, p->a);
    for (size_t k = 0; k < nregs; k++) {
        if (k) avp_buf_putz(&rb, ", ");
        avp_buf_putz(&rb, regs[k]);
    }
    for (int k = 0; k < p->nret; k++) {
        bool in = false;
        for (size_t j = 0; j < nregs && !in; j++) in = strcmp(p->ret[k], regs[j]) == 0;
        if (in) continue;
        if (kb.n) avp_buf_putz(&kb, ",");
        avp_buf_putz(&kb, p->ret[k]);
    }
    const char *keep = kb.n ? avp_buf_cz(&kb) : "void";
    const char *msg = avp_sprintf(p->a,
                                  "equivalent except the return register%s %s, which differ%s: proved if the function "
                                  "returns nothing there (--ret %s)",
                                  nregs > 1 ? "s" : "", avp_buf_cz(&rb), nregs > 1 ? "" : "s", keep);
    const EqExc *e = first->e;
    AvpJson *out = result(p, "NOT PROVEN", msg, e->block, e->has_terms ? e->terms : NULL, NULL);
    AvpJson *arr = avp_json_arr(p->out);
    for (size_t k = 0; k < nregs; k++) avp_json_arr_push(p->out, arr, avp_json_strz(p->out, regs[k]));
    avp_json_set(p->out, out, AVP_S("ret_only"), arr);
    return out;
}

/* prove(explain) (L1633-1674) */
static AvpJson *prove(Prover *p, bool explain) {
    First *first = AVP_NEW(p->a, First);
    share_arities(p);
    WallIso *maps = AVP_NEWN(p->a, WallIso, 2);
    int *nmaps = AVP_NEW(p->a, int);
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        *nmaps = pairings(p, maps);
        avp_try_end(&tr);
    } else {
        const EqExc *e = eq_exc_caught(&tr);
        if (e->code != EQ_EXC_VERDICT && e->code != EQ_EXC_UNSUPPORTED && e->code != EQ_EXC_NOTPROVEN) eq_reraise(e);
        *nmaps = 0;
        first->set = true;
        first->e = eq_exc_save(p->a, e);
    }
    for (int mi = 0; mi < *nmaps; mi++) {
        const WallIso *m = &maps[mi];
        p->mapping = m;
        p->nswaps = m->nswaps;
        p->swaps = AVP_NEWN(p->a, int, m->nswaps + 1);
        if (m->nswaps) memcpy(p->swaps, m->swaps, m->nswaps * sizeof *p->swaps);
        avp_sort(p->swaps, p->nswaps, sizeof *p->swaps, cmp_int, NULL, false);
        reset_assumptions(p);
        p->pairs = NULL;
        p->npairs = p->pcap = 0;
        p->explain = avp_json_arr(p->out);
        p->sides[0]->escaped = p->sides[1]->escaped = false;
        p->ret_reg = NULL;
        AvpTry t2;
        if (AVP_TRY(&t2)) {
            prove_inner(p, explain);
            avp_try_end(&t2);
            return result(p, "EQUIVALENT", "proved", -1, NULL, NULL);
        }
        const EqExc *e = eq_exc_caught(&t2);
        if (e->code == EQ_EXC_VERDICT || e->code == EQ_EXC_NOTPROVEN) {
            if (!first->set) {
                first->set = first->tuple = true;
                first->e = eq_exc_save(p->a, e);
                first->mapping = p->mapping;
                first->swaps = p->swaps;
                first->nswaps = p->nswaps;
                first->explain = p->explain;
                first->ret_reg = p->ret_reg;
            }
            continue;
        }
        if (e->code == EQ_EXC_UNSUPPORTED) {
            memset(first, 0, sizeof *first);
            first->set = true;
            first->e = eq_exc_save(p->a, e);
            break;
        }
        eq_reraise(e);
    }
    AVP_ASSERT(first->set);
    if (first->tuple) {
        p->mapping = first->mapping;
        p->swaps = first->swaps;
        p->nswaps = first->nswaps;
        p->explain = first->explain;
    }
    const EqExc *e = first->e;
    if (e->code == EQ_EXC_VERDICT) {
        AvpJson *r = ret_only_retry(p, first);
        if (r) return r;
        return result(p, e->vkind, e->msg, e->block, e->has_terms ? e->terms : NULL, NULL);
    }
    if (e->code == EQ_EXC_UNSUPPORTED)
        return result(p, "NOT PROVEN", avp_sprintf(p->a, "unmodelled instruction: %s", e->msg), -1, NULL, e->msg);
    return result(p, "NOT PROVEN", e->msg, -1, NULL, NULL);
}

/* ---- prove_functions (L2235-2247) ---- */

AvpJson *eq_prove_functions_ex(AvpArena *a, const CcFn *rfn, const CcFn *cfn, CcContext *ctx, EqRet ret, bool explain,
                               const CcCompareResult *res, EqLive *live, int max_rounds, bool seconds) {
    AvpArena *ta = avp_arena_new(0);
    EqHint *hints;
    size_t nhints = eq_target_hints(ta, res, ctx, &hints);
    EqResolver *rres = eq_resolver_new(ta, rfn, ctx, rfn, NULL, 0);
    EqResolver *cres = eq_resolver_new(ta, cfn, ctx, rfn, hints, nhints);
    EqTerms *t = eq_terms_new(ta);
    Prover *p = AVP_NEW(ta, Prover);
    p->a = ta;
    p->out = a;
    p->t = t;
    p->sides[0] = eq_side_new("retail", rfn, eq_resolve, rres, t, live);
    p->sides[1] = eq_side_new("compiled", cfn, eq_resolve, cres, t, live);
    p->ret_explicit = ret.n >= 0;
    p->ret = ret.n >= 0 ? ret.names : DEFAULT_RET;
    p->nret = ret.n >= 0 ? ret.n : 3;
    p->max_rounds = max_rounds;
    reset_assumptions(p);
    double t0 = avp_time();
    AvpJson *r = NULL;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        r = prove(p, explain);
        avp_try_end(&tr);
    } else {
        const EqExc *e = eq_exc_caught(&tr);
        eq_terms_free(t);
        avp_arena_free(ta);
        eq_reraise(e);
    }
    if (seconds) avp_json_set(a, r, AVP_S("seconds"), avp_json_float(a, avp_py_round(avp_time() - t0, 3)));
    eq_terms_free(t);
    avp_arena_free(ta);
    return r;
}

AvpJson *eq_prove_functions(AvpArena *a, const CcFn *rfn, const CcFn *cfn, CcContext *ctx, EqRet ret, bool explain,
                            const CcCompareResult *res, EqLive *live) {
    return eq_prove_functions_ex(a, rfn, cfn, ctx, ret, explain, res, live, 60, true);
}
