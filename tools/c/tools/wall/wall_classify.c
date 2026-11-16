/* wall_classify: the classifier of tools/wall.py (L378-864): _hex, Resolver, _calls, analyse_calls, _align, _stack,
 * _fallback_targets, classify, _cfg_differences, _loops and _evidence_sentence. See wall_classify.h. */
#include "wall_classify.h"
#include "avp/difflib.h"
#include "avp/fmt.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/sort.h"
#include "wall_cfg.h"

#include <string.h>

/* ---- small helpers ---------------------------------------------------------------------------------------------- */

static AvpJson *jstr(AvpArena *a, AvpStr s) { return avp_json_str(a, s); }
static AvpJson *jstrz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
static AvpJson *jint(AvpArena *a, i64 v) { return avp_json_int(a, v); }
static AvpJson *jopt(AvpArena *a, i64 v) { return v < 0 ? avp_json_null() : avp_json_int(a, v); }
static void jset(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }
static AvpStr op_of(const CcRow *r) { return avp_str_rstrip_chars(r->op, AVP_S("+-")); }

AvpStr wall_hex(AvpArena *a, i64 addr) { return avp_pyfmt(a, "0x%X", addr); }

/* ---- calls (wall.py L382-481) ------------------------------------------------------------------------------------- */

size_t wall_calls(AvpArena *a, const CcRow *rows, size_t nrows, WallCall **out) {
    WallCall *v = AVP_NEWN(a, WallCall, nrows ? nrows : 1);
    size_t n = 0;
    for (size_t i = 0; i < nrows; i++) {
        const CcRow *r = &rows[i];
        WallKind k = wall_row_kind(r);
        if (wall_is_self_call(r))
            v[n++] = (WallCall){i, AVP_S("<self>"), WALL_K_CALL};
        else if ((k == WALL_K_CALL || k == WALL_K_TAIL || k == WALL_K_CONDTAIL) && r->has_ref)
            v[n++] = (WallCall){i, r->ref.name, k};
        else if (k == WALL_K_CALL && avp_str_eqz(op_of(r), "bctrl"))
            v[n++] = (WallCall){i, AVP_S("<indirect>"), k};
    }
    *out = v;
    return n;
}

/* A Resolver key: a retail address (int) or the name (str); Python compares an int and a str as different. */
typedef struct {
    bool is_int;
    i64 v;
    AvpStr s;
} Key;

static bool key_eq(Key x, Key y) {
    return x.is_int == y.is_int && (x.is_int ? x.v == y.v : avp_str_eq(x.s, y.s));
}

static Key key_str(AvpStr s) { return (Key){false, 0, s}; }

/* Resolver.key(name) */
static Key resolve(CcProject *p, AvpStr name) {
    if (p) {
        const AvpI64Vec *c = cc_project_addresses(p, name);
        if (c && c->n == 1) return (Key){true, c->v[0], {0}};
    }
    i64 e;
    if (cc_embedded_address(name, &e)) return (Key){true, e, {0}};
    return key_str(name);
}

typedef struct {
    Key k;
    AvpStr name;
} KeyName;
typedef AVP_VEC(KeyName) KeyNames;

typedef struct {
    Key k;
    i64 n;
} KeyCount;
typedef AVP_VEC(KeyCount) Counter;

static AvpStr *names_get(KeyNames *m, Key k) {
    for (size_t i = 0; i < m->n; i++)
        if (key_eq(m->v[i].k, k)) return &m->v[i].name;
    return NULL;
}

static void names_setdefault(KeyNames *m, Key k, AvpStr name) {
    if (!names_get(m, k)) avp_vec_push(m, ((KeyName){k, name}));
}

/* str(name_of[k]) */
static AvpStr name_of(KeyNames *m, Key k) {
    AvpStr *s = names_get(m, k);
    return s ? *s : (AvpStr){"", 0};
}

static i64 counter_get(const Counter *c, Key k) {
    for (size_t i = 0; i < c->n; i++)
        if (key_eq(c->v[i].k, k)) return c->v[i].n;
    return 0;
}

static void counter_add(Counter *c, Key k, i64 n) {
    for (size_t i = 0; i < c->n; i++)
        if (key_eq(c->v[i].k, k)) {
            c->v[i].n += n;
            return;
        }
    avp_vec_push(c, ((KeyCount){k, n}));
}

/* a - b (Counter.__sub__: positive counts only, a's order). */
static Counter counter_sub(const Counter *x, const Counter *y) {
    Counter out = {0};
    for (size_t i = 0; i < x->n; i++) {
        i64 m = x->v[i].n - counter_get(y, x->v[i].k);
        if (m > 0) avp_vec_push(&out, ((KeyCount){x->v[i].k, m}));
    }
    return out;
}

typedef AVP_VEC(Key) Keys;

/* Counter.elements() */
static Keys counter_elements(const Counter *c) {
    Keys out = {0};
    for (size_t i = 0; i < c->n; i++)
        for (i64 q = 0; q < c->v[i].n; q++) avp_vec_push(&out, c->v[i].k);
    return out;
}

static bool is_save_rest(AvpStr name) {
    return avp_str_startswith(name, AVP_S("_savegpr")) || avp_str_startswith(name, AVP_S("_restgpr"));
}

AvpJson *wall_analyse_calls(AvpArena *a, const CcFn *rfn, const CcFn *cfn, CcProject *project) {
    AvpArena *t = avp_arena_new(0);
    WallCall *rc, *cc;
    size_t nrc = wall_calls(t, rfn->rows, rfn->nrows, &rc), ncc = wall_calls(t, cfn->rows, cfn->nrows, &cc);
    Key *rk = AVP_NEWN(t, Key, nrc + 1), *ck = AVP_NEWN(t, Key, ncc + 1);
    KeyNames names = {0};
    for (size_t i = 0; i < nrc; i++) rk[i] = resolve(project, rc[i].name);
    for (size_t i = 0; i < ncc; i++) ck[i] = resolve(project, cc[i].name);
    for (size_t i = 0; i < nrc; i++) names_setdefault(&names, rk[i], rc[i].name);
    for (size_t i = 0; i < ncc; i++) names_setdefault(&names, ck[i], cc[i].name);
    /* norm(k): save_helper(str(name_of[k])) or k */
    Key *rn = AVP_NEWN(t, Key, nrc + 1), *cn = AVP_NEWN(t, Key, ncc + 1);
    for (int side = 0; side < 2; side++) {
        size_t n = side ? ncc : nrc;
        Key *src = side ? ck : rk, *dst = side ? cn : rn;
        for (size_t i = 0; i < n; i++) {
            AvpStr h;
            dst[i] = wall_save_helper(name_of(&names, src[i]), &h) ? key_str(avp_str_dup(t, h)) : src[i];
        }
    }
    Counter rcnt = {0}, ccnt = {0};
    for (size_t i = 0; i < nrc; i++) counter_add(&rcnt, rn[i], 1);
    for (size_t i = 0; i < ncc; i++) counter_add(&ccnt, cn[i], 1);
    for (int side = 0; side < 2; side++) {
        const Counter *c = side ? &ccnt : &rcnt;
        for (size_t i = 0; i < c->n; i++) {
            Key k = c->v[i].k;
            names_setdefault(&names, k, k.is_int ? avp_pyfmt(t, "%d", k.v) : k.s);
        }
    }
    i64 size = (i64)cfn->nrows - (i64)rfn->nrows;
    Counter only_r = counter_sub(&rcnt, &ccnt), only_c = counter_sub(&ccnt, &rcnt);
    AvpJson *readings = avp_json_arr(a), *helpers = avp_json_arr(a), *findings = avp_json_arr(a);
    bool rstm = false, cstm = false, rsave = false, csave = false;
    for (size_t i = 0; i < rfn->nrows; i++) rstm = rstm || avp_str_eqz(rfn->rows[i].op, "stmw");
    for (size_t i = 0; i < cfn->nrows; i++) cstm = cstm || avp_str_eqz(cfn->rows[i].op, "stmw");
    for (size_t i = 0; i < rcnt.n; i++) rsave = rsave || is_save_rest(name_of(&names, rcnt.v[i].k));
    for (size_t i = 0; i < ccnt.n; i++) csave = csave || is_save_rest(name_of(&names, ccnt.v[i].k));
    bool flags_only = false;
    if ((rstm && csave && !cstm) || (cstm && rsave && !rstm)) {
        avp_json_arr_push(a, helpers,
                          jstr(a, avp_pyfmt(t,
                                            "retail saves with %s, compiled with %s: a flags question "
                                            "(-use_lmw_stmw), not source",
                                            rstm ? "stmw" : "_savegpr", cstm ? "stmw" : "_savegpr")));
        flags_only = true;
    }
    Keys er = counter_elements(&only_r), ec = counter_elements(&only_c);
    Keys plain_r = {0}, plain_c = {0};
    for (size_t i = 0; i < er.n; i++)
        if (!wall_is_helper(name_of(&names, er.v[i]))) avp_vec_push(&plain_r, er.v[i]);
    for (size_t i = 0; i < ec.n; i++)
        if (!wall_is_helper(name_of(&names, ec.v[i]))) avp_vec_push(&plain_c, ec.v[i]);
    for (int side = 0; side < 2; side++) {
        const Keys *e = side ? &ec : &er;
        for (size_t i = 0; i < e->n; i++) {
            AvpStr nm = name_of(&names, e->v[i]);
            if (wall_is_helper(nm) && !is_save_rest(nm))
                avp_json_arr_push(a, helpers,
                                  jstr(a, avp_pyfmt(t, side ? "helper %S called by compiled only"
                                                             : "helper %S called by retail only",
                                                    &nm)));
        }
    }
    size_t pr0 = 0, pc0 = 0; /* the slices plain_r[pr0:], plain_c[pc0:] */
    size_t nmax = plain_r.n > plain_c.n ? plain_r.n : plain_c.n;
    if (plain_r.n && plain_c.n && (size < 0 ? -size : size) <= 4 * (i64)nmax) {
        size_t nz = plain_r.n < plain_c.n ? plain_r.n : plain_c.n;
        for (size_t q = 0; q < nz; q++) {
            AvpStr x = name_of(&names, plain_r.v[q]), y = name_of(&names, plain_c.v[q]);
            avp_json_arr_push(a, readings,
                              jstr(a, avp_pyfmt(t, "different callee: retail calls %S, compiled calls %S", &x, &y)));
            AvpJson *f = avp_json_obj(a);
            jset(a, f, "kind", avp_json_strz(a, "different"));
            jset(a, f, "retail", jstr(a, x));
            jset(a, f, "compiled", jstr(a, y));
            avp_json_arr_push(a, findings, f);
        }
        pr0 = plain_c.n < plain_r.n ? plain_c.n : plain_r.n;
        pc0 = plain_r.n < plain_c.n ? plain_r.n : plain_c.n;
    }
    for (size_t q = pr0; q < plain_r.n; q++) {
        AvpStr x = name_of(&names, plain_r.v[q]);
        AvpJson *f = avp_json_obj(a);
        if (size > 0) {
            avp_json_arr_push(a, readings,
                              jstr(a, avp_pyfmt(t, "retail calls %S where compiled inlines it (compiled %+d rows)",
                                                &x, size)));
            jset(a, f, "kind", avp_json_strz(a, "inlined-in-compiled"));
        } else {
            avp_json_arr_push(a, readings,
                              jstr(a, avp_pyfmt(t, "retail calls %S, compiled does not (compiled %+d rows)", &x,
                                                size)));
            jset(a, f, "kind", avp_json_strz(a, "missing-in-compiled"));
        }
        jset(a, f, "callee", jstr(a, x));
        avp_json_arr_push(a, findings, f);
    }
    for (size_t q = pc0; q < plain_c.n; q++) {
        AvpStr x = name_of(&names, plain_c.v[q]);
        AvpJson *f = avp_json_obj(a);
        if (size < 0) {
            avp_json_arr_push(
                a, readings,
                jstr(a, avp_pyfmt(t, "compiled calls %S where retail has it inlined (compiled %+d rows)", &x, size)));
            jset(a, f, "kind", avp_json_strz(a, "inlined-in-retail"));
        } else {
            avp_json_arr_push(a, readings,
                              jstr(a, avp_pyfmt(t, "compiled calls %S, retail does not (compiled %+d rows)", &x,
                                                size)));
            jset(a, f, "kind", avp_json_strz(a, "extra-in-compiled"));
        }
        jset(a, f, "callee", jstr(a, x));
        avp_json_arr_push(a, findings, f);
    }
    bool order_differs = false;
    if (!only_r.n && !only_c.n) {
        if (nrc != ncc)
            order_differs = true;
        else
            for (size_t i = 0; i < nrc && !order_differs; i++) order_differs = !key_eq(rn[i], cn[i]);
    }
    if (order_differs) avp_json_arr_push(a, readings, avp_json_strz(a, "same calls in a different order"));
    bool differs = only_r.n || only_c.n || order_differs;

    AvpJson *d = avp_json_obj(a);
    AvpJson *jr = avp_json_arr(a), *jc = avp_json_arr(a);
    for (size_t i = 0; i < nrc; i++) avp_json_arr_push(a, jr, jstr(a, rc[i].name));
    for (size_t i = 0; i < ncc; i++) avp_json_arr_push(a, jc, jstr(a, cc[i].name));
    jset(a, d, "retail", jr);
    jset(a, d, "compiled", jc);
    for (int side = 0; side < 2; side++) {
        const Keys *e = side ? &ec : &er;
        AvpStr *v = AVP_NEWN(t, AvpStr, e->n + 1);
        for (size_t i = 0; i < e->n; i++) v[i] = name_of(&names, e->v[i]);
        avp_sort_strs(v, e->n);
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < e->n; i++) avp_json_arr_push(a, arr, jstr(a, v[i]));
        jset(a, d, side ? "compiled_only" : "retail_only", arr);
    }
    jset(a, d, "size_diff", jint(a, size));
    jset(a, d, "readings", readings);
    jset(a, d, "helpers", helpers);
    jset(a, d, "findings", findings);
    jset(a, d, "order_differs", avp_json_bool(order_differs));
    jset(a, d, "differs", avp_json_bool(differs));
    jset(a, d, "flags_only",
         avp_json_bool(flags_only && pr0 >= plain_r.n && pc0 >= plain_c.n && !order_differs));

    avp_vec_free(&names);
    avp_vec_free(&rcnt);
    avp_vec_free(&ccnt);
    avp_vec_free(&only_r);
    avp_vec_free(&only_c);
    avp_vec_free(&er);
    avp_vec_free(&ec);
    avp_vec_free(&plain_r);
    avp_vec_free(&plain_c);
    avp_arena_free(t);
    return d;
}

/* ---- _fallback_targets (wall.py L503-520) ------------------------------------------------------------------------- */

CcTdList wall_fallback_targets(AvpArena *a, const CcFn *rfn, const CcFn *cfn) {
    AvpArena *t = avp_arena_new(0);
    AvpStr *rn = AVP_NEWN(t, AvpStr, rfn->nrows + 1), *cn = AVP_NEWN(t, AvpStr, cfn->nrows + 1);
    cc_fn_norms(rfn, rn);
    cc_fn_norms(cfn, cn);
    AvpSeqMatcher *m = avp_seqm_new_str(t, rn, rfn->nrows, cn, cfn->nrows, &avp_seqm_no_autojunk);
    size_t nops;
    const AvpOpcode *ops = avp_seqm_opcodes(m, &nops);
    AVP_VEC(CcTargetDiff) out = {0};
    for (size_t q = 0; q < nops; q++) {
        if (ops[q].tag != AVP_OP_EQUAL) continue;
        for (size_t k = 0; k < ops[q].i2 - ops[q].i1; k++) {
            size_t i = ops[q].i1 + k, j = ops[q].j1 + k;
            const CcRow *r = &rfn->rows[i], *c = &cfn->rows[j];
            if (r->has_ref && c->has_ref && !avp_str_eq(r->ref.name, c->ref.name)) {
                i64 ea, eb;
                bool ha = cc_embedded_address(r->ref.name, &ea), hb = cc_embedded_address(c->ref.name, &eb);
                if (ha && hb && ea == eb) continue;
                CcTargetDiff td = {i, j, r->text, c->text, avp_pyfmt(a, "%S != %S", &r->ref.name, &c->ref.name),
                                   "target"};
                avp_vec_push(&out, td);
            } else if (r->bidx >= 0 && c->bidx >= 0 && r->bidx - (i64)i != c->bidx - (i64)j) {
                CcTargetDiff td = {i, j, r->text, c->text, AVP_S("branch target differs"), "branch"};
                avp_vec_push(&out, td);
            }
        }
    }
    CcTdList l = {AVP_NEWN(a, CcTargetDiff, out.n + 1), out.n};
    if (out.n) memcpy(l.v, out.v, out.n * sizeof *l.v);
    avp_vec_free(&out);
    avp_arena_free(t);
    return l;
}

/* ---- _stack (wall.py L490-500) ------------------------------------------------------------------------------------ */

/* int(x, 0) of an _R1_SLOT offset ("-0x20", "8"). */
static i64 parse_num(AvpStr s) {
    bool neg = false;
    size_t i = 0;
    if (i < s.n && s.p[i] == '-') neg = true, i++;
    i64 v = 0;
    if (i + 1 < s.n && s.p[i] == '0' && (s.p[i + 1] == 'x' || s.p[i + 1] == 'X')) {
        for (i += 2; i < s.n; i++) {
            char c = s.p[i];
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10;
            v = v * 16 + d;
        }
    } else {
        for (; i < s.n; i++) v = v * 10 + (s.p[i] - '0');
    }
    return neg ? -v : v;
}

typedef struct {
    bool has_frame;
    i64 frame;
    AvpI64Vec slots; /* distinct offsets, sorted */
} Stack;

static Stack stack_of(const CcFn *fn) {
    Stack s = {0};
    for (size_t q = 0; q < fn->nrows; q++) {
        const CcRow *r = &fn->rows[q];
        size_t st, en;
        AvpStr num;
        if (avp_str_eqz(r->op, "stwu") && avp_str_startswith(r->norm, AVP_S("stwu r1,"))) {
            if (wall_r1_slot_find(r->norm, 0, &st, &en, &num) && !s.has_frame) {
                s.has_frame = true;
                s.frame = -parse_num(num);
            }
            continue;
        }
        size_t pos = 0;
        while (wall_r1_slot_find(r->norm, pos, &st, &en, &num)) {
            avp_vec_push(&s.slots, parse_num(num));
            pos = en;
        }
    }
    avp_sort_i64(s.slots.v, s.slots.n);
    size_t w = 0;
    for (size_t i = 0; i < s.slots.n; i++)
        if (!w || s.slots.v[w - 1] != s.slots.v[i]) s.slots.v[w++] = s.slots.v[i];
    s.slots.n = w;
    return s;
}

/* sorted(set(x) - set(y)) of two sorted distinct lists */
static AvpJson *slots_minus(AvpArena *a, const AvpI64Vec *x, const AvpI64Vec *y) {
    AvpJson *arr = avp_json_arr(a);
    size_t j = 0;
    for (size_t i = 0; i < x->n; i++) {
        while (j < y->n && y->v[j] < x->v[i]) j++;
        if (j < y->n && y->v[j] == x->v[i]) continue;
        avp_json_arr_push(a, arr, jint(a, x->v[i]));
    }
    return arr;
}

/* ---- classify (wall.py L523-791) ---------------------------------------------------------------------------------- */

enum { C_REF, C_CALL, C_CONTROL, C_OTHER, C_REG, C_UNEXP, NCAT };
static const char *const CAT_NAME[NCAT] = {WALL_REFERENT, WALL_CALL, WALL_CONTROL,
                                           WALL_OTHER,    WALL_REGISTER, "unexplained"};

typedef struct {
    AvpArena *a;
    const CcFn *rfn, *cfn;
    i64 count[NCAT];
    AvpJson *ev[NCAT];
    int order[NCAT]; /* categories in first-note order (Counter / defaultdict insertion order) */
    int norder;
    bool reg_stack_slot; /* some REGISTER note says "stack slot" */
} Notes;

/* note(category, i, j, why); i / j -1 is None */
static void note(Notes *n, int cat, i64 i, i64 j, AvpStr why) {
    AvpArena *a = n->a;
    if (!n->ev[cat]) {
        n->ev[cat] = avp_json_arr(a);
        n->order[n->norder++] = cat;
    }
    n->count[cat]++;
    AvpJson *e = avp_json_obj(a);
    jset(a, e, "retail", jopt(a, i));
    jset(a, e, "compiled", jopt(a, j));
    jset(a, e, "retail_text", i >= 0 ? jstr(a, n->rfn->rows[i].text) : avp_json_null());
    jset(a, e, "compiled_text", j >= 0 ? jstr(a, n->cfn->rows[j].text) : avp_json_null());
    jset(a, e, "why", jstr(a, why));
    avp_json_arr_push(a, n->ev[cat], e);
    if (cat == C_REG && avp_str_eqz(why, "stack slot")) n->reg_stack_slot = true;
}

static u32 intern(AvpMap *m, AvpStr s) {
    bool created;
    AvpVal *v = avp_map_put(m, s, &created);
    if (created) v->i = (i64)avp_map_len(m) - 1;
    return (u32)v->i;
}

/* content(g, ab): per block, the sorted ids of its rows that do not start with "b" (a Counter as a sorted list). */
typedef struct {
    u32 **ids;
    size_t *n;
} Content;

static int cmp_u32(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    u32 p = *(const u32 *)x, q = *(const u32 *)y;
    return p < q ? -1 : p > q;
}

static Content content_of(AvpArena *t, const WallCfg *g, const AvpStr *ab, const u32 *id) {
    Content c = {AVP_NEWN(t, u32 *, g->nblocks + 1), AVP_NEWN(t, size_t, g->nblocks + 1)};
    for (size_t b = 0; b < g->nblocks; b++) {
        const WallBlock *bl = &g->blocks[b];
        c.ids[b] = AVP_NEWN(t, u32, bl->end - bl->start + 1);
        for (size_t r = bl->start; r < bl->end; r++)
            if (!(ab[r].n && ab[r].p[0] == 'b')) c.ids[b][c.n[b]++] = id[r];
        avp_sort(c.ids[b], c.n[b], sizeof(u32), cmp_u32, NULL, false);
    }
    return c;
}

typedef struct {
    Content r, c;
} SimCtx;

/* sum((cr_[a] & cc_c[b]).values()) */
static i64 sim_fn(void *ctx, int x, int y) {
    SimCtx *s = ctx;
    const u32 *p = s->r.ids[x], *q = s->c.ids[y];
    size_t np = s->r.n[x], nq = s->c.n[y], i = 0, j = 0;
    i64 n = 0;
    while (i < np && j < nq) {
        if (p[i] < q[j]) i++;
        else if (p[i] > q[j]) j++;
        else n++, i++, j++;
    }
    return n;
}

/* _loops(s) */
static AvpStr loops_text(AvpArena *a, const AvpJson *summary) {
    const AvpJson *lp = avp_json_getz(summary, "loops");
    size_t n = avp_json_len(lp);
    AvpStr *keys = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) keys[i] = avp_json_member(lp, i)->key;
    avp_sort_strs(keys, n);
    if (!n) return AVP_S("none");
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_pyfmt(&b, "%d %S", avp_json_get_i64(lp, avp_cz(a, keys[i]), 0), &keys[i]);
    }
    return avp_strn(avp_buf_cz(&b), b.n);
}

static i64 loops_sum(const AvpJson *summary) {
    const AvpJson *lp = avp_json_getz(summary, "loops");
    i64 s = 0;
    for (size_t i = 0; i < avp_json_len(lp); i++) s += avp_json_i64(avp_json_member(lp, i)->val, 0);
    return s;
}

static bool loops_equal(const AvpJson *x, const AvpJson *y) {
    const AvpJson *p = avp_json_getz(x, "loops"), *q = avp_json_getz(y, "loops");
    if (avp_json_len(p) != avp_json_len(q)) return false;
    for (size_t i = 0; i < avp_json_len(p); i++) {
        const AvpJsonMember *m = avp_json_member(p, i);
        const AvpJson *o = avp_json_get(q, m->key);
        if (!o || avp_json_i64(o, 0) != avp_json_i64(m->val, 0)) return false;
    }
    return true;
}

/* sig(g, ab): Counter of the non-branch row tuples of the blocks of more than one row (the tuple as its ids). */
static AvpMap *block_sigs(const WallCfg *g, const AvpStr *ab, const u32 *id) {
    AvpMap *m = avp_map_new();
    AVP_VEC(u32) key = {0};
    for (size_t b = 0; b < g->nblocks; b++) {
        const WallBlock *bl = &g->blocks[b];
        if (bl->end - bl->start <= 1) continue;
        avp_vec_clear(&key);
        for (size_t r = bl->start; r < bl->end; r++)
            if (!(ab[r].n && ab[r].p[0] == 'b')) avp_vec_push(&key, id[r]);
        bool created;
        avp_map_put(m, avp_strn((const char *)key.v, key.n * sizeof(u32)), &created)->i += 1;
    }
    avp_vec_free(&key);
    return m;
}

/* _cfg_differences(gr, gc, ra, ca) (wall.py L794-822), appended to out. */
static void cfg_differences(AvpArena *t, AvpStrVec *out, const WallCfg *gr, const WallCfg *gc, const AvpJson *sr,
                            const AvpJson *sc, const AvpStr *ra, const AvpStr *ca, const u32 *rid, const u32 *cid) {
    size_t n0 = out->n;
#define G(s, k) avp_json_get_i64((s), (k), 0)
    if (G(sr, "switches") != G(sc, "switches"))
        avp_vec_push(out, avp_pyfmt(t,
                                    "switch lowered differently: retail %d jump table(s) (bctr), compiled %d; %d vs "
                                    "%d conditional blocks",
                                    G(sr, "switches"), G(sc, "switches"), G(sr, "conds"), G(sc, "conds")));
    i64 nr = loops_sum(sr), nc = loops_sum(sc);
    if (nr == nc && !loops_equal(sr, sc)) {
        AvpStr x = loops_text(t, sr), y = loops_text(t, sc);
        avp_vec_push(out, avp_pyfmt(t, "loop rotation: retail loops %S, compiled %S", &x, &y));
    } else if (nr != nc) {
        avp_vec_push(out, avp_pyfmt(t, "different loop count: retail %d loop(s), compiled %d", nr, nc));
    }
    if (G(sr, "returns") != G(sc, "returns"))
        avp_vec_push(out, avp_pyfmt(t, "early return versus shared epilogue: retail %d return(s), compiled %d",
                                    G(sr, "returns"), G(sc, "returns")));
    if (G(sr, "blocks") != G(sc, "blocks")) {
        AvpMap *dr = block_sigs(gr, ra, rid), *dc = block_sigs(gc, ca, cid);
        size_t dup = 0;
        AvpMapIter it = {0};
        while (avp_map_next(dr, &it)) {
            AvpVal *o = avp_map_get(dc, it.key);
            if (o && o->i != it.val->i) dup++;
        }
        avp_map_free(dr);
        avp_map_free(dc);
        AvpBuf b;
        avp_buf_init(&b, t);
        avp_buf_pyfmt(&b, "%s: retail %d blocks, compiled %d",
                      G(sc, "blocks") > G(sr, "blocks") ? "extra block(s) in compiled" : "missing block(s) in compiled",
                      G(sr, "blocks"), G(sc, "blocks"));
        if (dup) avp_buf_pyfmt(&b, "; %d block(s) duplicated on one side (a duplicated tail)", dup);
        avp_vec_push(out, avp_strn(avp_buf_cz(&b), b.n));
    }
    if (out->n == n0)
        avp_vec_push(out, avp_pyfmt(t, "same block, edge and loop counts but differently wired (retail %d blocks / %d "
                                       "edges)",
                                    G(sr, "blocks"), G(sr, "edges")));
#undef G
}

/* The register map (wall.py L650-677). */
typedef struct {
    AvpStr b;
    i64 n;
} RegCount;
typedef struct {
    AvpStr a;
    AVP_VEC(RegCount) bs;
} RegRow;
typedef AVP_VEC(RegRow) RegMap;

static RegRow *regmap_row(RegMap *m, AvpMap *idx, AvpStr a) {
    bool created;
    AvpVal *v = avp_map_put(idx, a, &created);
    if (created) {
        v->i = (i64)m->n;
        RegRow r = {a, {0}};
        avp_vec_push(m, r);
    }
    return &m->v[v->i];
}

static void regrow_add(RegRow *r, AvpStr b) {
    for (size_t i = 0; i < r->bs.n; i++)
        if (avp_str_eq(r->bs.v[i].b, b)) {
            r->bs.v[i].n++;
            return;
        }
    RegCount c = {b, 1};
    avp_vec_push(&r->bs, c);
}

/* bs.most_common(1)[0][0]: the highest count, the first inserted on a tie */
static AvpStr regrow_top(const RegRow *r) {
    size_t best = 0;
    for (size_t i = 1; i < r->bs.n; i++)
        if (r->bs.v[i].n > r->bs.v[best].n) best = i;
    return r->bs.v[best].b;
}

typedef struct {
    AvpStr a, b;
} StrPair;

static int cmp_pair(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const StrPair *p = x, *q = y;
    int c = avp_str_cmp(p->a, q->a);
    return c ? c : avp_str_cmp(p->b, q->b);
}

/* The (blk, reg) keys of local / local_rev. */
static AvpStr blk_key(AvpArena *t, int blk, AvpStr reg) { return avp_pyfmt(t, "%d\x01%S", blk, &reg); }

typedef struct {
    AvpStr first; /* the first member of a set of registers */
    bool multi;   /* the set has more than one member */
} RegSet;

static AvpJson *str_array(AvpArena *a, const AvpStr *v, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, jstr(a, v[i]));
    return arr;
}

/* _evidence_sentence (wall.py L829-864) */
typedef struct {
    const Notes *notes;
    const AvpJson *referents, *calls;
    const AvpStrVec *findings;
    bool consistent;
    const StrPair *swapped;
    size_t nswapped;
    const Stack *sr, *sc;
    const AvpJson *stack;
    size_t nmoved;
    int in_block; /* -1 None, 0 False, 1 True */
} Evidence;

static AvpStr frame_text(AvpArena *t, const Stack *s) { return s->has_frame ? avp_pyfmt(t, "%d", s->frame) : AVP_S("None"); }

static AvpStr evidence_sentence(AvpArena *t, int wall, const Evidence *e) {
    const Notes *n = e->notes;
    if (wall == C_REF) {
        if (!avp_json_len(e->referents)) return avp_pyfmt(t, "%d row(s) name a different target", n->count[C_REF]);
        const AvpJson *r = avp_json_at(e->referents, 0);
        const AvpJson *d = avp_json_getz(r, "describe");
        const AvpJson *same = avp_json_is_obj(d) ? avp_json_getz(d, "same") : NULL;
        AvpStr x = avp_json_get_str(r, "retail", AVP_S("")), y = avp_json_get_str(r, "compiled", AVP_S(""));
        AvpStr tail = AVP_S("");
        if (same && avp_json_truthy(same)) {
            AvpStr s = avp_json_str_or(same, AVP_S(""));
            tail = avp_pyfmt(t, " (%S)", &s);
        }
        return avp_pyfmt(t, "%d row(s) name a different target, first %S vs %S%S", avp_json_len(e->referents), &x, &y,
                         &tail);
    }
    if (wall == C_CALL) {
        const AvpJson *rd = avp_json_getz(e->calls, "readings"), *hp = avp_json_getz(e->calls, "helpers");
        if (avp_json_len(rd)) {
            AvpStr first = avp_json_str_or(avp_json_at(rd, 0), AVP_S(""));
            if (avp_json_len(rd) > 1) return avp_pyfmt(t, "%S (+%d more)", &first, avp_json_len(rd) - 1);
            return first;
        }
        if (avp_json_len(hp)) return avp_json_str_or(avp_json_at(hp, 0), AVP_S(""));
        return avp_pyfmt(t, "%d call row(s) differ", n->count[C_CALL]);
    }
    if (wall == C_CONTROL) {
        if (e->findings->n) return e->findings->v[0];
        AvpStr why = AVP_S("");
        if (n->ev[C_CONTROL]) why = avp_json_get_str(avp_json_at(n->ev[C_CONTROL], 0), "why", AVP_S(""));
        return avp_pyfmt(t, "%d control row(s): %S", n->count[C_CONTROL], &why);
    }
    if (wall == C_OTHER) {
        const AvpJson *ev = avp_json_at(n->ev[C_OTHER], 0);
        AvpStr rt = avp_json_get_str(ev, "retail_text", AVP_S("")), ct = avp_json_get_str(ev, "compiled_text", AVP_S(""));
        return avp_pyfmt(t, "%d row(s) differ in an immediate, first retail row %d: %S | %S", n->count[C_OTHER],
                         avp_json_get_i64(ev, "retail", 0), &rt, &ct);
    }
    AvpBuf b;
    avp_buf_init(&b, t);
    bool any = false;
#define BIT_SEP() (any ? avp_buf_putz(&b, ", ") : (void)0, any = true)
    if (e->nswapped) {
        BIT_SEP();
        avp_buf_putz(&b, "renaming ");
        for (size_t i = 0; i < e->nswapped && i < 4; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_pyfmt(&b, "%S->%S", &e->swapped[i].a, &e->swapped[i].b);
        }
    }
    BIT_SEP();
    avp_buf_putz(&b, e->consistent ? "consistent" : "not one consistent renaming");
    if (e->nmoved) {
        BIT_SEP();
        avp_buf_pyfmt(&b, "%d moved row(s)%s", e->nmoved, e->in_block != 0 ? "" : " across blocks");
    }
    bool frame_eq = e->sr->has_frame == e->sc->has_frame && (!e->sr->has_frame || e->sr->frame == e->sc->frame);
    if (!frame_eq) {
        BIT_SEP();
        AvpStr x = frame_text(t, e->sr), y = frame_text(t, e->sc);
        avp_buf_pyfmt(&b, "frame %S vs %S", &x, &y);
    } else if (avp_json_len(avp_json_getz(e->stack, "retail_only_slots")) ||
               avp_json_len(avp_json_getz(e->stack, "compiled_only_slots"))) {
        BIT_SEP();
        avp_buf_putz(&b, "stack slots differ");
    }
#undef BIT_SEP
    AvpStr bits = avp_strn(avp_buf_cz(&b), b.n);
    return avp_pyfmt(t, "%d row(s); %S", n->count[C_REG], &bits);
}

static bool is_condish(WallKind k) { return k == WALL_K_COND || k == WALL_K_CONDRET || k == WALL_K_CONDTAIL; }

typedef struct {
    size_t i, j;
} IJ;

AvpJson *wall_classify(AvpArena *a, const CcFn *rfn, const CcFn *cfn, const CcTdList *targets,
                       WallDescribeFn describe, void *describe_ctx, CcProject *project, size_t unresolved) {
    AvpArena *t = avp_arena_new(0);
    CcTdList fallback;
    if (!targets) {
        fallback = wall_fallback_targets(t, rfn, cfn);
        targets = &fallback;
    }
    const CcRow *R = rfn->rows, *C = cfn->rows;
    size_t nR = rfn->nrows, nC = cfn->nrows;
    AvpStr *ra = AVP_NEWN(t, AvpStr, nR + 1), *ca = AVP_NEWN(t, AvpStr, nC + 1);
    AvpStr *rv = AVP_NEWN(t, AvpStr, nR + 1), *cv = AVP_NEWN(t, AvpStr, nC + 1);
    u32 *rid = AVP_NEWN(t, u32, nR + 1), *cid = AVP_NEWN(t, u32, nC + 1);
    AvpMap *ids = avp_map_new();
    for (size_t i = 0; i < nR; i++) {
        ra[i] = wall_abstract_regs(t, R[i].norm);
        rv[i] = wall_abstract_values(t, ra[i]);
        rid[i] = intern(ids, ra[i]);
    }
    for (size_t j = 0; j < nC; j++) {
        ca[j] = wall_abstract_regs(t, C[j].norm);
        cv[j] = wall_abstract_values(t, ca[j]);
        cid[j] = intern(ids, ca[j]);
    }
    size_t nids = avp_map_len(ids);
    avp_map_free(ids);
    WallCfg *gr = wall_cfg_new(t, R, nR), *gc = wall_cfg_new(t, C, nC);

    Notes notes = {.a = a, .rfn = rfn, .cfn = cfn};

    /* alignment on register-abstracted rows */
    IJ *pairs = AVP_NEWN(t, IJ, nR + 1);
    size_t npairs = 0;
    size_t *loose_r = AVP_NEWN(t, size_t, nR + 1), *loose_c = AVP_NEWN(t, size_t, nC + 1);
    size_t nlr = 0, nlc = 0;
    AvpSeqMatcher *sm = avp_seqm_new_u32(t, rid, nR, cid, nC, &avp_seqm_no_autojunk);
    size_t nops;
    const AvpOpcode *ops = avp_seqm_opcodes(sm, &nops);
    for (size_t q = 0; q < nops; q++) {
        const AvpOpcode *o = &ops[q];
        if (o->tag == AVP_OP_EQUAL) {
            for (size_t k = 0; k < o->i2 - o->i1; k++) pairs[npairs++] = (IJ){o->i1 + k, o->j1 + k};
        } else {
            for (size_t i = o->i1; i < o->i2; i++) loose_r[nlr++] = i;
            for (size_t j = o->j1; j < o->j2; j++) loose_c[nlc++] = j;
        }
    }
    /* moved rows: same abstract text on both sides, not aligned */
    i64 *head = AVP_NEWN(t, i64, nids + 1), *tail = AVP_NEWN(t, i64, nids + 1), *nxt = AVP_NEWN(t, i64, nC + 1);
    for (size_t k = 0; k < nids; k++) head[k] = tail[k] = -1;
    for (size_t q = 0; q < nlc; q++) {
        size_t j = loose_c[q];
        nxt[j] = -1;
        if (tail[cid[j]] < 0) head[cid[j]] = (i64)j;
        else nxt[tail[cid[j]]] = (i64)j;
        tail[cid[j]] = (i64)j;
    }
    bool *used_r = AVP_NEWN(t, bool, nR + 1), *used_c = AVP_NEWN(t, bool, nC + 1);
    IJ *moved = AVP_NEWN(t, IJ, nR + 1);
    size_t nmoved = 0;
    for (size_t q = 0; q < nlr; q++) {
        size_t i = loose_r[q];
        i64 best = -1, bestd = 0;
        for (i64 j = head[rid[i]]; j >= 0; j = nxt[j]) {
            if (used_c[j]) continue;
            i64 dd = j - (i64)i;
            if (dd < 0) dd = -dd;
            if (best < 0 || dd < bestd) best = j, bestd = dd;
        }
        if (best >= 0) {
            used_c[best] = true;
            used_r[i] = true;
            moved[nmoved++] = (IJ){i, (size_t)best};
        }
    }

    /* targets: keyed by retail row (the last one for a row) */
    i64 *tby = AVP_NEWN(t, i64, nR + 1);
    for (size_t i = 0; i < nR; i++) tby[i] = -1;
    for (size_t q = 0; q < targets->n; q++)
        if (targets->v[q].i < nR) tby[targets->v[q].i] = (i64)q;
    AvpJson *referents = avp_json_arr(a);
#define DETAIL(td) ((td)->detail.p ? (td)->detail : AVP_S(""))
    for (size_t q = 0; q < npairs; q++) {
        size_t i = pairs[q].i, j = pairs[q].j;
        const CcTargetDiff *td = tby[i] >= 0 ? &targets->v[tby[i]] : NULL;
        AvpStr h1, h2;
        if (td && strcmp(td->kind, "branch") == 0) {
            AvpStr d = DETAIL(td);
            note(&notes, C_CONTROL, (i64)i, (i64)j, avp_pyfmt(t, "branch target: %S", &d));
        } else if (td && avp_str_contains(DETAIL(td), AVP_S("points to function offset"))) {
            note(&notes, C_CONTROL, (i64)i, (i64)j, avp_pyfmt(t, "jump table entry: %S", &td->detail));
        } else if (td && R[i].has_ref && C[j].has_ref && wall_save_helper(R[i].ref.name, &h1) &&
                   wall_save_helper(C[j].ref.name, &h2) && avp_str_eq(h1, h2)) {
            note(&notes, C_REG, (i64)i, (i64)j,
                 avp_pyfmt(t, "saved-register range (%S vs %S)", &R[i].ref.name, &C[j].ref.name));
        } else if (td) {
            note(&notes, C_REF, (i64)i, (i64)j, DETAIL(td));
            AvpJson *d = describe ? describe(describe_ctx, a, td->i, td->j) : NULL;
            AvpJson *e = avp_json_obj(a);
            jset(a, e, "retail_row", jint(a, (i64)i));
            jset(a, e, "compiled_row", jint(a, (i64)j));
            jset(a, e, "retail", jstr(a, R[i].has_ref ? R[i].ref.name : R[i].text));
            jset(a, e, "compiled", jstr(a, C[j].has_ref ? C[j].ref.name : C[j].text));
            jset(a, e, "detail", td->detail.p ? jstr(a, td->detail) : avp_json_null());
            jset(a, e, "describe", d ? d : avp_json_null());
            avp_json_arr_push(a, referents, e);
        } else if (!avp_str_eq(R[i].norm, C[j].norm)) {
            note(&notes, C_REG, (i64)i, (i64)j, AVP_S("registers"));
        }
    }
    /* targets on rows this alignment left loose (codecmp aligned them as equal): still referent evidence */
    bool *paired_i = AVP_NEWN(t, bool, nR + 1);
    for (size_t q = 0; q < npairs; q++) paired_i[pairs[q].i] = true;
    for (size_t q = 0; q < targets->n; q++) {
        const CcTargetDiff *td = &targets->v[q];
        AvpStr h;
        if (td->i < nR && paired_i[td->i]) continue;
        if (strcmp(td->kind, "branch") == 0) continue;
        if (avp_str_contains(DETAIL(td), AVP_S("points to function offset"))) continue;
        if (td->i < nR && R[td->i].has_ref && wall_save_helper(R[td->i].ref.name, &h)) continue;
        AvpJson *d = describe ? describe(describe_ctx, a, td->i, td->j) : NULL;
        AvpJson *e = avp_json_obj(a);
        jset(a, e, "retail_row", jint(a, (i64)td->i));
        jset(a, e, "compiled_row", jint(a, (i64)td->j));
        jset(a, e, "retail", jstr(a, td->retail));
        jset(a, e, "compiled", jstr(a, td->compiled));
        jset(a, e, "detail", td->detail.p ? jstr(a, td->detail) : avp_json_null());
        jset(a, e, "describe", d ? d : avp_json_null());
        avp_json_arr_push(a, referents, e);
    }
#undef DETAIL
    /* moved rows */
    for (size_t q = 0; q < nmoved; q++) note(&notes, C_REG, (i64)moved[q].i, (i64)moved[q].j, AVP_S("moved"));
    /* remaining loose rows, paired positionally inside each differing block */
    size_t *rr = AVP_NEWN(t, size_t, nR + 1), *cc_ = AVP_NEWN(t, size_t, nC + 1);
    for (size_t q = 0; q < nops; q++) {
        const AvpOpcode *o = &ops[q];
        if (o->tag == AVP_OP_EQUAL) continue;
        size_t nr = 0, nc = 0;
        for (size_t i = o->i1; i < o->i2; i++)
            if (!used_r[i]) rr[nr++] = i;
        for (size_t j = o->j1; j < o->j2; j++)
            if (!used_c[j]) cc_[nc++] = j;
        size_t nk = nr > nc ? nr : nc;
        for (size_t k = 0; k < nk; k++) {
            i64 i = k < nr ? (i64)rr[k] : -1, j = k < nc ? (i64)cc_[k] : -1;
            if (i >= 0 && j >= 0) {
                AvpStr ro = op_of(&R[i]), co = op_of(&C[j]);
                if (avp_str_eq(rv[i], cv[j])) {
                    if (wall_uses_r1(R[i].norm) && wall_uses_r1(C[j].norm))
                        note(&notes, C_REG, i, j, AVP_S("stack slot"));
                    else if (wall_is_compare(ro))
                        note(&notes, C_CONTROL, i, j, AVP_S("compare constant"));
                    else
                        note(&notes, C_OTHER, i, j, AVP_S("immediate"));
                } else if (wall_is_signed_pair(ro, co)) {
                    note(&notes, C_CONTROL, i, j, avp_pyfmt(t, "signedness (%S vs %S)", &ro, &co));
                } else if (is_condish(wall_row_kind(&R[i])) && is_condish(wall_row_kind(&C[j]))) {
                    note(&notes, C_CONTROL, i, j, avp_pyfmt(t, "condition (%S vs %S)", &ro, &co));
                } else if (wall_row_kind(&R[i]) == WALL_K_CALL || wall_row_kind(&C[j]) == WALL_K_CALL) {
                    note(&notes, C_CALL, i, j, AVP_S("call row"));
                } else {
                    note(&notes, C_UNEXP, i, j, AVP_S(""));
                }
            } else {
                const CcRow *row = i >= 0 ? &R[i] : &C[j];
                WallKind k_ = wall_row_kind(row);
                if (k_ == WALL_K_CALL || k_ == WALL_K_TAIL)
                    note(&notes, C_CALL, i, j, AVP_S("call only on one side"));
                else if (k_ != WALL_K_NONE)
                    note(&notes, C_CONTROL, i, j, AVP_S("branch only on one side"));
                else if ((avp_str_startswith(row->op, AVP_S("l")) || avp_str_startswith(row->op, AVP_S("st"))) &&
                         avp_str_contains(row->norm, AVP_S("(")))
                    note(&notes, C_UNEXP, i, j, AVP_S("extra load/store on one side (a reload or spill?)"));
                else
                    note(&notes, C_UNEXP, i, j, AVP_S(""));
            }
        }
    }

    /* register map from every pair whose abstract rows agree: callee-saved registers globally, volatile ones per
     * retail block */
    RegMap regmap = {0};
    AvpMap *regidx = avp_map_new();
    RegMap local = {0};
    AvpMap *localidx = avp_map_new();
    AVP_VEC(int) local_blk = {0};
    size_t npset = npairs + nmoved;
    for (size_t q = 0; q < npset; q++) {
        IJ p = q < npairs ? pairs[q] : moved[q - npairs];
        AvpStr *x, *y;
        size_t nx = wall_regs_of(t, R[p.i].norm, &x), ny = wall_regs_of(t, C[p.j].norm, &y);
        for (size_t k = 0; k < nx && k < ny; k++) {
            regrow_add(regmap_row(&regmap, regidx, x[k]), y[k]);
            if (!wall_callee_saved(x[k])) {
                int blk = gr->block_of[p.i];
                size_t before = local.n;
                RegRow *lr = regmap_row(&local, localidx, blk_key(t, blk, x[k]));
                if (local.n != before) {
                    lr->a = x[k];
                    avp_vec_push(&local_blk, blk);
                }
                regrow_add(lr, y[k]);
            }
        }
    }
    AvpSet *conflicts = avp_set_new();
    AvpMap *rev = avp_map_new(); /* b -> RegSet* of callee-saved a */
    for (size_t q = 0; q < regmap.n; q++) {
        RegRow *r = &regmap.v[q];
        if (!wall_callee_saved(r->a)) continue;
        if (r->bs.n > 1) avp_set_add(conflicts, r->a);
        for (size_t k = 0; k < r->bs.n; k++) {
            bool created;
            AvpVal *v = avp_map_put(rev, r->bs.v[k].b, &created);
            if (created) {
                RegSet *s = AVP_NEW(t, RegSet);
                s->first = r->a;
                v->p = s;
            } else if (!avp_str_eq(((RegSet *)v->p)->first, r->a)) {
                ((RegSet *)v->p)->multi = true;
            }
        }
    }
    AvpMap *local_rev = avp_map_new();
    for (size_t q = 0; q < local.n; q++) {
        RegRow *r = &local.v[q];
        if (r->bs.n > 1) avp_set_add(conflicts, r->a);
        for (size_t k = 0; k < r->bs.n; k++) {
            bool created;
            AvpVal *v = avp_map_put(local_rev, blk_key(t, local_blk.v[q], r->bs.v[k].b), &created);
            if (created) {
                RegSet *s = AVP_NEW(t, RegSet);
                s->first = r->a;
                v->p = s;
            } else if (!avp_str_eq(((RegSet *)v->p)->first, r->a)) {
                ((RegSet *)v->p)->multi = true;
            }
        }
    }
    {
        AvpMapIter it = {0};
        while (avp_map_next(local_rev, &it))
            if (((RegSet *)it.val->p)->multi) {
                AvpStr key = it.key;
                const char *sep = memchr(key.p, '\x01', key.n);
                AvpStr b = avp_strn(sep + 1, key.n - (size_t)(sep + 1 - key.p));
                avp_set_add(conflicts, avp_pyfmt(t, "<-%S", &b));
            }
        it = (AvpMapIter){0};
        while (avp_map_next(rev, &it))
            if (((RegSet *)it.val->p)->multi) avp_set_add(conflicts, avp_pyfmt(t, "<-%S", &it.key));
    }
    AvpStrVec conf = avp_set_sorted(conflicts);
    StrPair *swapped = AVP_NEWN(t, StrPair, regmap.n + 1);
    size_t nswapped = 0;
    for (size_t q = 0; q < regmap.n; q++) {
        AvpStr top = regrow_top(&regmap.v[q]);
        if (!avp_str_eq(top, regmap.v[q].a)) swapped[nswapped++] = (StrPair){regmap.v[q].a, top};
    }
    avp_sort(swapped, nswapped, sizeof *swapped, cmp_pair, NULL, false);
    bool consistent = conf.n == 0;

    /* control-flow graphs */
    AvpJson *calls = wall_analyse_calls(a, rfn, cfn, project);
    AvpJson *sr = wall_cfg_summary(a, gr), *sc = wall_cfg_summary(a, gc);
    AvpJson *cf = avp_json_obj(a);
    jset(a, cf, "retail", sr);
    jset(a, cf, "compiled", sc);
    jset(a, cf, "findings", avp_json_arr(a));
    jset(a, cf, "isomorphic", avp_json_null());
    AvpStrVec findings = {0};
    SimCtx simc = {content_of(t, gr, ra, rid), content_of(t, gc, ca, cid)};
    WallIso iso = wall_match_cfgs(t, gr, gc, WALL_MATCH_BUDGET, sim_fn, &simc);
    int in_block = -1;
    bool iso_true = false;
    if (iso.status == WALL_ISO_BUDGET) {
        jset(a, cf, "isomorphic", avp_json_strz(a, "undecided"));
        avp_vec_push(&findings, AVP_S("graph search gave up (too many alternatives)"));
    } else if (iso.status == WALL_ISO_NONE) {
        jset(a, cf, "isomorphic", avp_json_bool(false));
        cfg_differences(t, &findings, gr, gc, sr, sc, ra, ca, rid, cid);
        i64 *amap = AVP_NEWN(t, i64, nR + 1), *bmap = AVP_NEWN(t, i64, nC + 1);
        for (size_t i = 0; i < nR; i++) amap[i] = -1;
        for (size_t j = 0; j < nC; j++) bmap[j] = -1;
        for (size_t q = 0; q < npset; q++) {
            IJ p = q < npairs ? pairs[q] : moved[q - npairs];
            amap[p.i] = (i64)p.j;
            bmap[p.j] = (i64)p.i;
        }
        AvpJson *wb = avp_json_arr(a);
        size_t nwrong = 0;
        size_t wi = 0;
        i64 wti = 0, wback = -1;
        for (size_t q = 0; q < npairs; q++) {
            size_t i = pairs[q].i, j = pairs[q].j;
            i64 ti = R[i].bidx, tj = C[j].bidx;
            if (ti >= 0 && tj >= 0 && (size_t)ti < nR && amap[ti] >= 0 && amap[ti] != tj) {
                i64 back = (size_t)tj < nC ? bmap[tj] : -1;
                if (!nwrong) wi = i, wti = ti, wback = back;
                nwrong++;
                AvpJson *e = avp_json_obj(a);
                jset(a, e, "retail_row", jint(a, (i64)i));
                jset(a, e, "retail_target", jint(a, ti));
                jset(a, e, "compiled_target_as_retail", jopt(a, back));
                avp_json_arr_push(a, wb, e);
            }
        }
        if (nwrong) {
            AvpStr where = wback >= 0 ? avp_pyfmt(t, "the row aligned with retail row %d", wback)
                                      : AVP_S("a row with no retail counterpart");
            AvpStr msg = avp_pyfmt(t,
                                   "%d aligned branch(es) reach different blocks: first retail row %d (%S) goes to "
                                   "row %d, compiled goes to %S",
                                   nwrong, wi, &R[wi].text, wti, &where);
            if (findings.n && avp_str_startswith(findings.v[0], AVP_S("same block")))
                avp_vec_insert(&findings, 0, msg);
            else
                avp_vec_push(&findings, msg);
        }
        jset(a, cf, "wrong_branches", wb);
    } else {
        iso_true = true;
        jset(a, cf, "isomorphic", avp_json_bool(true));
        for (size_t q = 0; q < iso.nswaps; q++) {
            const WallBlock *ba = &gr->blocks[iso.swaps[q]], *bb = &gc->blocks[iso.map[iso.swaps[q]]];
            AvpStr h1 = wall_hex(t, R[ba->end - 1].addr), h2 = wall_hex(t, C[bb->end - 1].addr);
            avp_vec_push(&findings, avp_pyfmt(t, "inverted condition at retail %S: %S | compiled %S: %S", &h1,
                                              &R[ba->end - 1].text, &h2, &C[bb->end - 1].text));
        }
        int *order = AVP_NEWN(t, int, gr->nblocks + 1);
        i64 *seq = AVP_NEWN(t, i64, gr->nblocks + 1);
        size_t norder = 0;
        for (size_t b = 0; b < iso.nmap_a; b++)
            if (iso.map[b] >= 0) order[norder] = (int)b, seq[norder++] = iso.map[b];
        bool *keep = wall_lis_keep(t, seq, norder);
        AvpStrVec ooo = {0};
        for (size_t k = 0; k < norder; k++)
            if (!keep[k]) avp_vec_push(&ooo, wall_hex(t, R[gr->blocks[order[k]].start].addr));
        if (ooo.n) {
            AvpStr list = avp_str_join(t, AVP_S(", "), ooo.v, ooo.n < 4 ? ooo.n : 4);
            avp_vec_push(&findings,
                         avp_pyfmt(t, "different block order: %d block(s) placed differently (retail %S)", ooo.n,
                                   &list));
        }
        avp_vec_free(&ooo);
        in_block = 1;
        for (size_t q = 0; q < nmoved; q++)
            if (iso.map[gr->block_of[moved[q].i]] != gc->block_of[moved[q].j]) in_block = 0;
    }
    jset(a, cf, "findings", str_array(a, findings.v, findings.n));
    jset(a, cf, "inverted", jint(a, iso.status == WALL_ISO_MAPPING ? (i64)iso.nswaps : 0));
    if (gr->approx_switch || gc->approx_switch) jset(a, cf, "note", avp_json_strz(a, "jump-table successors approximated"));

    Stack str_ = stack_of(rfn), stc = stack_of(cfn);
    AvpJson *stack = avp_json_obj(a);
    {
        AvpJson *fr = avp_json_arr(a);
        avp_json_arr_push(a, fr, str_.has_frame ? jint(a, str_.frame) : avp_json_null());
        avp_json_arr_push(a, fr, stc.has_frame ? jint(a, stc.frame) : avp_json_null());
        jset(a, stack, "frame", fr);
        jset(a, stack, "retail_only_slots", slots_minus(a, &str_.slots, &stc.slots));
        jset(a, stack, "compiled_only_slots", slots_minus(a, &stc.slots, &str_.slots));
    }
    bool frame_eq = str_.has_frame == stc.has_frame && (!str_.has_frame || str_.frame == stc.frame);

    /* verdict */
    int present[NCAT];
    size_t npresent = 0;
    if (notes.count[C_REF] || avp_json_len(referents)) present[npresent++] = C_REF;
    if (avp_json_truthy(avp_json_getz(calls, "differs")) || notes.count[C_CALL]) present[npresent++] = C_CALL;
    i64 inverted = iso.status == WALL_ISO_MAPPING ? (i64)iso.nswaps : 0;
    if (!iso_true || inverted || findings.n || notes.count[C_CONTROL])
        if (!(iso_true && !findings.n && !notes.count[C_CONTROL])) present[npresent++] = C_CONTROL;
    if (notes.count[C_OTHER]) present[npresent++] = C_OTHER;
    if (notes.count[C_REG] || !frame_eq) present[npresent++] = C_REG;
    i64 unexplained = notes.count[C_UNEXP];
    i64 differing = 0;
    for (int c = 0; c < NCAT; c++) differing += notes.count[c];
    bool exact = !differing && !npresent && nR == nC;
    const char *wall = NULL;
    AvpStr line;
    if (exact) {
        line = AVP_S("EXACT");
    } else if (!npresent) {
        wall = WALL_OTHER;
        line = avp_pyfmt(t, "WALL other: %d unexplained row(s), no class explains them", unexplained);
    } else {
        wall = CAT_NAME[present[0]];
        Evidence ev = {&notes, referents, calls, &findings, consistent, swapped, nswapped,
                       &str_,  &stc,      stack, nmoved,    in_block};
        AvpStr sent = evidence_sentence(t, present[0], &ev);
        line = avp_pyfmt(t, "WALL %s: %S", wall, &sent);
        bool pure = npresent == 1 && present[0] == C_REG && !unexplained && consistent && in_block != 0 && frame_eq &&
                    !notes.reg_stack_slot;
        if (pure) {
            wall = WALL_REGISTER;
            line = avp_pyfmt(t, "EQUIVALENT-LOOKING: register/schedule only (%d rows)", notes.count[C_REG]);
        } else {
            AvpStrVec tl = {0};
            if (npresent > 1) {
                AvpBuf b;
                avp_buf_init(&b, t);
                avp_buf_putz(&b, "also ");
                for (size_t k = 1; k < npresent; k++) {
                    if (k > 1) avp_buf_putz(&b, ", ");
                    avp_buf_putz(&b, CAT_NAME[present[k]]);
                }
                avp_vec_push(&tl, avp_strn(avp_buf_cz(&b), b.n));
            }
            if (unexplained) avp_vec_push(&tl, avp_pyfmt(t, "%d unexplained row(s)", unexplained));
            if (unexplained > differing - unexplained) {
                line = avp_pyfmt(t, "MIXED %S", &line);
                avp_vec_push(&tl, AVP_S("most rows unexplained: weak verdict"));
            }
            if (tl.n) {
                AvpStr j = avp_str_join(t, AVP_S("; "), tl.v, tl.n);
                line = avp_pyfmt(t, "%S [%S]", &line, &j);
            }
            avp_vec_free(&tl);
        }
    }
    if (!exact && nR >= 8 && nC < nR / 2)
        line = avp_pyfmt(t, "INCOMPLETE? compiled is %d%% of retail's size; %S", (i64)(100 * nC / nR), &line);

    AvpJson *d = avp_json_obj(a);
    jset(a, d, "exact", avp_json_bool(exact));
    jset(a, d, "wall", jstrz(a, wall));
    AvpJson *cls = avp_json_arr(a);
    for (size_t k = 0; k < npresent; k++) avp_json_arr_push(a, cls, avp_json_strz(a, CAT_NAME[present[k]]));
    jset(a, d, "classes", cls);
    jset(a, d, "verdict", jstr(a, line));
    AvpJson *rows = avp_json_obj(a);
    jset(a, rows, "retail", jint(a, (i64)nR));
    jset(a, rows, "compiled", jint(a, (i64)nC));
    jset(a, rows, "differing", jint(a, differing));
    jset(a, d, "rows", rows);
    AvpJson *cats = avp_json_obj(a), *rowev = avp_json_obj(a);
    for (int k = 0; k < notes.norder; k++) {
        int c = notes.order[k];
        jset(a, cats, CAT_NAME[c], jint(a, notes.count[c]));
        jset(a, rowev, CAT_NAME[c], notes.ev[c]);
    }
    jset(a, d, "categories", cats);
    jset(a, d, "unexplained", jint(a, unexplained));
    jset(a, d, "unresolved_targets", jint(a, (i64)unresolved));
    jset(a, d, "referent", referents);
    jset(a, d, "calls", calls);
    jset(a, d, "control", cf);
    AvpJson *reg = avp_json_obj(a);
    jset(a, reg, "consistent", avp_json_bool(consistent));
    jset(a, reg, "conflicts", str_array(a, conf.v, conf.n));
    AvpJson *sw = avp_json_arr(a);
    for (size_t k = 0; k < nswapped; k++) {
        AvpJson *e = avp_json_obj(a);
        jset(a, e, "retail", jstr(a, swapped[k].a));
        jset(a, e, "compiled", jstr(a, swapped[k].b));
        jset(a, e, "callee_saved", avp_json_bool(wall_callee_saved(swapped[k].a) || wall_callee_saved(swapped[k].b)));
        avp_json_arr_push(a, sw, e);
    }
    jset(a, reg, "swapped", sw);
    AvpJson *mv = avp_json_arr(a);
    for (size_t k = 0; k < nmoved; k++) {
        AvpJson *e = avp_json_obj(a);
        jset(a, e, "retail", jint(a, (i64)moved[k].i));
        jset(a, e, "compiled", jint(a, (i64)moved[k].j));
        jset(a, e, "text", jstr(a, R[moved[k].i].text));
        avp_json_arr_push(a, mv, e);
    }
    jset(a, reg, "moved", mv);
    jset(a, reg, "moved_in_block", in_block < 0 ? avp_json_null() : avp_json_bool(in_block == 1));
    jset(a, reg, "stack", stack);
    jset(a, d, "register", reg);
    jset(a, d, "row_evidence", rowev);

    for (size_t q = 0; q < regmap.n; q++) avp_vec_free(&regmap.v[q].bs);
    for (size_t q = 0; q < local.n; q++) avp_vec_free(&local.v[q].bs);
    avp_vec_free(&regmap);
    avp_vec_free(&local);
    avp_vec_free(&local_blk);
    avp_map_free(regidx);
    avp_map_free(localidx);
    avp_map_free(rev);
    avp_map_free(local_rev);
    avp_set_free(conflicts);
    avp_vec_free(&conf);
    avp_vec_free(&findings);
    avp_vec_free(&str_.slots);
    avp_vec_free(&stc.slots);
    avp_arena_free(t);
    return d;
}
