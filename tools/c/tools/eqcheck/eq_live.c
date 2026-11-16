/* eq_live: callee argument liveness (tools/eqcheck.py L1405-1584), _rpo / diff_path / first_diff (L1591-1630) and
 * the _BRANCH / _HELPER patterns (L536-537). See eq_live.h. The patterns run through shsm_re (Python's re). */
#include "eq_live.h"

#include "shsm_asmindex.h"
#include "shsm_re.h"

#include <stdio.h>

/* ---- patterns ---------------------------------------------------------------------------------------------------- */
#define BRANCH_RE "^b(?P<ctr>dnz|dz)?(?P<cc>eq|ne|lt|ge|gt|le|so|ns|un|nu|nl|ng)?(?P<lr>lr|ctr)?(?P<link>l)?$"
#define HELPER_RE "^_(save|rest)(gpr|fpr)_(\\d+)$"
#define REGTOK_RE "(?<![\\w@.])([rf]\\d{1,2})(?![\\w@])"
#define DEF_FIRST_RE                                                                                               \
    "^(l(?!mw)[a-z]*|mr|li|lis|mf\\w+|add\\w*|sub\\w*|mul\\w*|div\\w*|neg|and\\w*|or\\w*|xor\\w*|"                \
    "nand|nor|eqv|ext\\w+|cntlzw|sra\\w*|slw\\w*|srw\\w*|rlwinm|rlwnm|clr\\w+|rot\\w+|f(?!cmp)\\w+|"              \
    "ps_\\w+|psq_l\\w*|not)\\.?$"
#define UPDATE_RE "^(l\\w*u\\w*|st\\w*u\\w*)$"

bool eq_branch_match(AvpStr mn, EqBranch *out) {
    ShsmReMatch m;
    if (!shsm_re_match(shsm_re(BRANCH_RE, 0), mn, 0, AVP_END, &m)) return false;
    if (out) {
        out->ctr = shsm_re_group(&m, 1);
        out->cc = shsm_re_group(&m, 2);
        out->lr = shsm_re_group(&m, 3);
        out->link = shsm_re_group(&m, 4);
    }
    return true;
}

bool eq_helper_match(AvpStr name, AvpStr *kind, AvpStr *cls, i64 *n) {
    ShsmReMatch m;
    if (!name.p) name = AVP_S("");
    if (!shsm_re_match(shsm_re(HELPER_RE, 0), name, 0, AVP_END, &m)) return false;
    if (kind) *kind = shsm_re_group(&m, 1);
    if (cls) *cls = shsm_re_group(&m, 2);
    if (n) {
        bool ovf;
        i64 v = 0;
        avp_parse_int(shsm_re_group(&m, 3), 10, &v, &ovf);
        *n = v;
    }
    return true;
}

/* ---- argument registers ----------------------------------------------------------------------------------------- */
const char *const EQ_ARG_REGS[EQ_NARG_REGS] = {
    "r0", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "f0", "f1",
    "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13",
};
/* r3..r10 = bits 1..8, f1..f8 = bits 12..19 */
const u32 EQ_STD_ARGS = (((1u << 8) - 1) << 1) | (((1u << 8) - 1) << 12);

u32 eq_abit(AvpStr name) {
    for (int k = 0; k < EQ_NARG_REGS; k++)
        if (avp_str_eqz(name, EQ_ARG_REGS[k])) return 1u << k;
    return 0;
}

u32 eq_reg_mask(const AvpStr *names, size_t n) {
    u32 m = 0;
    for (size_t i = 0; i < n; i++) m |= eq_abit(names[i]);
    return m;
}

/* ---- CalleeLiveness ---------------------------------------------------------------------------------------------- */
typedef struct {
    bool none;
    u32 live, writes;
} Summ;

struct EqLive {
    const ShsmRepo *repo;
    int depth;
    AvpIMap *memo; /* addr -> Summ* */
    int ix_state;  /* 0 not opened yet (None), 1 open, -1 unavailable (False) */
    ShsmAsmIndex *ix;
    AvpArena *arena; /* the memo's entries */
};

EqLive *eq_live_new(const ShsmRepo *repo, int depth) {
    EqLive *lv = avp_xcalloc(1, sizeof *lv);
    lv->repo = repo;
    lv->depth = depth;
    lv->memo = avp_imap_new();
    lv->arena = avp_arena_new(0);
    return lv;
}

void eq_live_free(EqLive *lv) {
    if (!lv) return;
    if (lv->ix) shsm_asmindex_close(lv->ix);
    avp_imap_free(lv->memo);
    avp_arena_free(lv->arena);
    avp_xfree(lv);
}

void eq_live_disable_index(EqLive *lv) {
    if (lv->ix) shsm_asmindex_close(lv->ix);
    lv->ix = NULL;
    lv->ix_state = -1;
}

/* _index(): the index, or NULL (opening failed, remembered). */
static ShsmAsmIndex *live_index(EqLive *lv) {
    if (lv->ix_state == 0) {
        AvpTry t;
        ShsmAsmIndex *ix = NULL;
        if (AVP_TRY(&t)) {
            ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = lv->repo, .readonly = true, .no_refresh = true});
            avp_try_end(&t);
        } else {
            ix = NULL;
        }
        lv->ix = ix;
        lv->ix_state = ix ? 1 : -1;
    }
    return lv->ix_state == 1 ? lv->ix : NULL;
}

static void memo_set(EqLive *lv, i64 addr, bool none, u32 live, u32 writes) {
    bool created;
    AvpVal *v = avp_imap_put(lv->memo, (u64)addr, &created);
    Summ *s = created ? AVP_NEW(lv->arena, Summ) : v->p;
    s->none = none;
    s->live = live;
    s->writes = writes;
    v->p = s;
}

static Summ summary(EqLive *lv, i64 addr, int depth) {
    AvpVal *v = avp_imap_get(lv->memo, (u64)addr);
    if (v) return *(Summ *)v->p;
    if (depth > lv->depth) return (Summ){false, EQ_ALL_ARGS, 0};
    ShsmAsmIndex *ix = live_index(lv);
    if (!ix) return (Summ){true, 0, 0};
    memo_set(lv, addr, false, EQ_ALL_ARGS, 0); /* a recursive call reads everything, writes nothing */
    AvpArena *sa = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(sa);
    ShsmAsmFile *file = NULL;
    ShsmAsmBlock *blk = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        blk = shsm_asmindex_function(sa, ix, shsm_target_addr(addr), &file);
        avp_try_end(&t);
    } else {
        blk = NULL;
    }
    Summ res = {true, 0, 0};
    if (blk && blk->address == addr && blk->rows.n) {
        res.none = false;
        eq_live_analyse(lv, blk->rows.v, blk->rows.n, depth, &res.live, &res.writes);
    }
    if (file) shsm_asm_free(file);
    avp_arena_reset(sa, mk);
    memo_set(lv, addr, res.none, res.live, res.writes);
    return res;
}

bool eq_live_summary(EqLive *lv, i64 addr, int depth, u32 *live, u32 *writes) {
    Summ s = summary(lv, addr, depth);
    if (s.none) return false;
    if (live) *live = s.live;
    if (writes) *writes = s.writes;
    return true;
}

bool eq_live_call(EqLive *lv, i64 addr, u32 *live, u32 *writes) { return eq_live_summary(lv, addr, 0, live, writes); }

/* _callee(name, depth): (uses, writes) of a called symbol. */
static Summ callee(EqLive *lv, AvpStr name, int depth) {
    if (eq_helper_match(name, NULL, NULL, NULL)) return (Summ){false, eq_abitz("r11"), 0};
    ShsmAsmIndex *ix = live_index(lv);
    bool found = false;
    ShsmSymRow sr = {0};
    if (ix) { /* None.symbol(...) raises AttributeError in the Python: caught, sr = None */
        AvpArena *sa = avp_scratch();
        AvpArenaMark mk = avp_arena_mark(sa);
        AvpTry t;
        if (AVP_TRY(&t)) {
            found = shsm_asmindex_symbol(sa, ix, name, &sr);
            avp_try_end(&t);
        } else {
            found = false;
        }
        avp_arena_reset(sa, mk); /* only sr.address is read below */
    }
    if (!found) return (Summ){false, EQ_ALL_ARGS, 0};
    Summ m = summary(lv, sr.address, depth + 1);
    return m.none ? (Summ){false, EQ_ALL_ARGS, 0} : m;
}

typedef struct {
    int v[2];
    int n;
} Succ;

static bool is_empty_sym(AvpStr s) { return !s.p || s.n == 0; }

void eq_live_analyse(EqLive *lv, const ShsmAsmRow *rows, size_t n, int depth, u32 *live_out, u32 *writes_out) {
    AvpIMap *at = avp_imap_new();
    for (size_t i = 0; i < n; i++) {
        bool created;
        avp_imap_put(at, (u64)rows[i].address, &created)->i = (i64)i;
    }
    Succ *succ = avp_xcalloc(n ? n : 1, sizeof *succ);
    u32 *uses = avp_xcalloc(n ? n : 1, sizeof *uses), *kills = avp_xcalloc(n ? n : 1, sizeof *kills);
    bool *has_exit = avp_xcalloc(n ? n : 1, sizeof *has_exit);
    u32 *exit_extra = avp_xcalloc(n ? n : 1, sizeof *exit_extra); /* `extra or 0` */
    const ShsmRe *regtok = shsm_re(REGTOK_RE, 0), *def_first = shsm_re(DEF_FIRST_RE, 0),
                 *update = shsm_re(UPDATE_RE, 0);
    AVP_VEC(AvpStr) regs = {0};
    for (size_t i = 0; i < n; i++) {
        const ShsmAsmRow *row = &rows[i];
        AvpStr mn = avp_str_rstrip_chars(row->mnemonic.p ? row->mnemonic : AVP_S(""), AVP_S("+-"));
        AvpStr ops = row->operands.p ? row->operands : AVP_S("");
        regs.n = 0;
        ShsmReIter it;
        ShsmReMatch m;
        shsm_re_iter_init(&it, regtok, ops, 0, AVP_END);
        while (shsm_re_iter_next(&it, &m)) avp_vec_push(&regs, shsm_re_group(&m, 1));
        Succ nxt = {{0, 0}, 0};
        if (i + 1 < n) nxt = (Succ){{(int)i + 1, 0}, 1};
        u32 u = 0, d = 0;
        Succ sc = nxt;
        Succ none = {{0, 0}, 0};
        if (avp_str_startswith(mn, AVP_S("b"))) {
            EqBranch b;
            if (!eq_branch_match(mn, &b)) {
                u = EQ_ALL_ARGS;
                sc = none;
                has_exit[i] = true;
                exit_extra[i] = 0;
            } else {
                bool cond = (b.cc.p && b.cc.n) || (b.ctr.p && b.ctr.n);
                AvpStr lrctr = b.lr;
                bool has_lrctr = lrctr.p && lrctr.n;
                if (b.link.p && b.link.n) {
                    if (has_lrctr || is_empty_sym(row->sym)) {
                        u = EQ_STD_ARGS;
                        d = 0;
                    } else {
                        Summ s = callee(lv, row->sym, depth);
                        u = s.live;
                        d = s.writes;
                    }
                } else if (has_lrctr && avp_str_eqz(lrctr, "lr")) {
                    sc = cond ? nxt : none;
                    has_exit[i] = true; /* a return: what is written so far */
                    exit_extra[i] = 0;
                } else if (has_lrctr && avp_str_eqz(lrctr, "ctr")) {
                    u = EQ_ALL_ARGS;
                    sc = cond ? nxt : none;
                    has_exit[i] = true;
                    exit_extra[i] = 0;
                } else if (row->label != SHSM_NONE && avp_imap_has(at, (u64)row->label) &&
                           row->label != rows[0].address) {
                    sc.v[0] = (int)avp_imap_get(at, (u64)row->label)->i;
                    sc.n = 1;
                    if (cond && nxt.n) sc.v[sc.n++] = nxt.v[0];
                } else if (!is_empty_sym(row->sym)) {
                    Summ s = callee(lv, row->sym, depth);
                    u = s.live;
                    sc = cond ? nxt : none;
                    has_exit[i] = true; /* a tail call: what is written so far plus what it writes */
                    exit_extra[i] = s.writes;
                } else {
                    u = EQ_ALL_ARGS;
                    sc = cond ? nxt : none;
                    has_exit[i] = true;
                    exit_extra[i] = 0;
                }
            }
        } else if (avp_str_eqz(mn, "stmw") || avp_str_eqz(mn, "lmw")) {
            i64 first = 0;
            if (regs.n && avp_str_startswith(regs.v[0], AVP_S("r"))) {
                bool ovf;
                avp_parse_int(avp_str_slice(regs.v[0], 1, AVP_END), 10, &first, &ovf);
            }
            u32 span = 0;
            for (i64 k = first; k < 32; k++) {
                char nm[8];
                snprintf(nm, sizeof nm, "r%d", (int)k);
                span |= eq_abitz(nm);
            }
            u32 rest = regs.n > 1 ? eq_reg_mask(regs.v + 1, regs.n - 1) : 0;
            if (avp_str_eqz(mn, "stmw")) {
                u = span | rest;
            } else {
                d = span;
                u = rest;
            }
        } else if (avp_str_eqz(mn, "sc") || avp_str_eqz(mn, "rfi") || avp_str_eqz(mn, "tw") ||
                   avp_str_eqz(mn, "twi") || avp_str_eqz(mn, "trap")) {
            u = EQ_ALL_ARGS;
        } else if (regs.n && !avp_str_startswith(mn, AVP_S("rlwimi")) && !avp_str_startswith(mn, AVP_S("inslwi")) &&
                   !avp_str_startswith(mn, AVP_S("insrwi")) && shsm_re_match(def_first, mn, 0, AVP_END, &m)) {
            d = eq_reg_mask(regs.v, 1);
            u = regs.n > 1 ? eq_reg_mask(regs.v + 1, regs.n - 1) : 0;
            if (regs.n > 1 && shsm_re_match(update, mn, 0, AVP_END, &m))
                d &= ~eq_reg_mask(regs.v + 1, regs.n - 1); /* an update form also writes its base: no kill claimed */
        } else {
            u = eq_reg_mask(regs.v, regs.n);
        }
        succ[i] = sc;
        uses[i] = u;
        kills[i] = d;
    }
    avp_vec_free(&regs);
    /* backward liveness */
    u32 *live = avp_xcalloc(n ? n : 1, sizeof *live);
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t ii = n; ii-- > 0;) {
            u32 out = 0;
            for (int k = 0; k < succ[ii].n; k++) out |= live[succ[ii].v[k]];
            u32 v = (out & ~kills[ii]) | uses[ii];
            if (v != live[ii]) {
                live[ii] = v;
                changed = true;
            }
        }
    }
    /* forward must-definition: ALL_ARGS is "not reached yet" */
    u32 *must_in = avp_xcalloc(n ? n : 1, sizeof *must_in);
    bool *reached = avp_xcalloc(n ? n : 1, sizeof *reached);
    for (size_t i = 0; i < n; i++) must_in[i] = EQ_ALL_ARGS;
    if (n) {
        must_in[0] = 0;
        reached[0] = true;
    }
    changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < n; i++) {
            if (!reached[i]) continue;
            u32 out = must_in[i] | kills[i];
            for (int k = 0; k < succ[i].n; k++) {
                int x = succ[i].v[k];
                u32 nv = reached[x] ? (must_in[x] & out) : out;
                if (!reached[x] || nv != must_in[x]) {
                    must_in[x] = nv;
                    reached[x] = true;
                    changed = true;
                }
            }
        }
    }
    u32 writes = EQ_ALL_ARGS;
    for (size_t i = 0; i < n; i++)
        if (has_exit[i] && reached[i]) writes &= must_in[i] | kills[i] | exit_extra[i];
    if (live_out) *live_out = n ? live[0] : EQ_ALL_ARGS;
    if (writes_out) *writes_out = writes;
    avp_xfree(live);
    avp_xfree(must_in);
    avp_xfree(reached);
    avp_xfree(succ);
    avp_xfree(uses);
    avp_xfree(kills);
    avp_xfree(has_exit);
    avp_xfree(exit_extra);
    avp_imap_free(at);
}

/* ---- graph helpers ---------------------------------------------------------------------------------------------- */
size_t eq_rpo(AvpArena *a, const WallCfg *cfg, int **out) {
    size_t nb = cfg->nblocks;
    int *order = AVP_NEWN(a, int, nb ? nb : 1);
    size_t no = 0;
    *out = order;
    if (!nb) return 0;
    bool *seen = avp_xcalloc(nb, sizeof *seen);
    typedef struct {
        int u;
        size_t pos;
    } Frame;
    Frame *stack = avp_xmalloc(sizeof *stack * (nb + 1));
    size_t sp = 0;
    stack[sp++] = (Frame){0, 0};
    seen[0] = true;
    while (sp) {
        Frame *f = &stack[sp - 1];
        const WallBlock *b = &cfg->blocks[f->u];
        if (f->pos >= b->nsucc) {
            order[no++] = f->u;
            sp--;
            continue;
        }
        int v = b->succ[f->pos++];
        if (!seen[v]) {
            seen[v] = true;
            stack[sp++] = (Frame){v, 0};
        }
    }
    for (size_t i = 0; i < no / 2; i++) {
        int x = order[i];
        order[i] = order[no - 1 - i];
        order[no - 1 - i] = x;
    }
    avp_xfree(seen);
    avp_xfree(stack);
    return no;
}

/* The single differing argument pair of two nodes of the same shape: true when exactly one pair differs and its
 * retail side is a node. */
static bool step(EqArg *x, EqArg *y) {
    if (x->kind != EQ_ARG_NODE || y->kind != EQ_ARG_NODE) return false;
    EqNode *a = x->n, *b = y->n;
    if (a == b || a->op != b->op || a->nargs != b->nargs) return false;
    int nd = 0, at = -1;
    for (int i = 0; i < a->nargs; i++) {
        if (eq_arg_differs(a->args[i], b->args[i])) {
            if (nd++ == 0) at = i;
        }
    }
    if (nd != 1 || a->args[at].kind != EQ_ARG_NODE) return false;
    *x = a->args[at];
    *y = b->args[at];
    return true;
}

size_t eq_diff_path(AvpArena *ar, EqArg x, EqArg y, EqArgPair **out) {
    AVP_VEC(EqArgPair) path = {0};
    EqArgPair p0 = {x, y};
    avp_vec_push(&path, p0);
    while (step(&x, &y)) {
        EqArgPair p = {x, y};
        avp_vec_push(&path, p);
    }
    *out = AVP_NEWN(ar, EqArgPair, path.n);
    memcpy(*out, path.v, sizeof **out * path.n);
    size_t n = path.n;
    avp_vec_free(&path);
    return n;
}

EqArgPair eq_first_diff(EqArg x, EqArg y) {
    while (step(&x, &y)) {
    }
    return (EqArgPair){x, y};
}
