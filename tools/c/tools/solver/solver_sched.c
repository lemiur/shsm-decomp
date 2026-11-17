/* solver_sched: the order test of a register/schedule stop and the scheduler's trace (mwtrace's library: mwt_sched.h).
 *   - the order test (pure): at the first differing row, retail and ours hold the same instructions in another order
 *     within one block (the opcodes and, with the register names masked, the operands are the same multiset over a
 *     window that crosses no branch target and no branch before its last row): the stop is class `schedule`;
 *   - a schedule stop's condition: the two instructions located in our object's pass-2 blocks, the pair query (the
 *     key that decided, or the edge that held retail's first), pass 1's decision where pass 2 kept pass 1's order by
 *     program order; the self-check: a `trace` gap is a model gap, an `after-scheduler` gap a stated fact;
 *   - a temporaries-only register record: pass 1's order of the two temporaries' defining instructions;
 *   - sched --against for the best candidate (and at most one probe) of a schedule stop.
 * The numbers only state conditions: no candidate is generated from them. */
#include "solver.h"

#include <stdlib.h>

const char *const SV_TRACE_NAMES[SV_NTRACE] = {"inline", "regalloc", "sched"};

#define SV_ORDER_WINDOW 64     /* rows the order test looks ahead */
#define SV_SCHED_AGAINST_MAX 2 /* sched --against traces per stop: the best candidate and one probe */

/* ---- the order test (pure) ------------------------------------------------------------------------------------- */

static bool is_reg_token(const char *p, size_t n) {
    if (n < 2 || (p[0] != 'r' && p[0] != 'f')) return false;
    for (size_t i = 1; i < n; i++)
        if (p[i] < '0' || p[i] > '9') return false;
    return true;
}

const char *sv_mask_regs(AvpArena *a, const char *norm) {
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *p = norm ? norm : "";
    /* the mnemonic stays */
    while (*p && *p != ' ') avp_buf_putc(&b, *p++);
    while (*p) {
        if (*p == ' ' || *p == ',' || *p == '(' || *p == ')') {
            avp_buf_putc(&b, *p++);
            continue;
        }
        const char *q = p;
        while (*q && *q != ' ' && *q != ',' && *q != '(' && *q != ')') q++;
        if (is_reg_token(p, (size_t)(q - p))) avp_buf_putc(&b, p[0] == 'r' ? 'R' : 'F');
        else avp_buf_putn(&b, p, (size_t)(q - p));
        p = q;
    }
    return b.p ? b.p : "";
}

static bool is_branch(const char *norm) { return norm && norm[0] == 'b'; }

void sv_order_rows(AvpArena *a, const CcFn *fn, SvORow **out, size_t *n) {
    *n = fn ? fn->nrows : 0;
    *out = AVP_NEWN(a, SvORow, *n + 1);
    for (size_t i = 0; i < *n; i++) {
        (*out)[i].norm = avp_cz(a, fn->rows[i].norm.n ? fn->rows[i].norm : fn->rows[i].text);
        (*out)[i].branch = is_branch((*out)[i].norm);
    }
    for (size_t i = 0; i < *n; i++)
        if (fn->rows[i].has_label && fn->rows[i].bidx >= 0 && (size_t)fn->rows[i].bidx < *n) (*out)[fn->rows[i].bidx].target = true;
}

typedef struct {
    const char *mask;
    int count;
} MaskCount;

static void tally(MaskCount *mc, size_t *nmc, const char *m, int d) {
    for (size_t i = 0; i < *nmc; i++)
        if (!strcmp(mc[i].mask, m)) {
            mc[i].count += d;
            return;
        }
    mc[*nmc].mask = m;
    mc[*nmc].count = d;
    (*nmc)++;
}

/* An instruction's identity across two register allocations: rows before the first differing row are identical on
 * both sides, so a register read before any definition in the window holds the same value on both; a register defined
 * inside the window is named by the instruction that defined it. Two rows are "the same instruction" when their
 * opcode, immediates and operands' values are equal (hash-consed: each canonical form is a small integer of a table
 * both sides share). */
typedef struct {
    AvpMap *forms; /* canonical text -> val.i: its id */
} Canon;

static int intern(Canon *c, const char *s) {
    bool created = false;
    AvpVal *v = avp_map_put(c->forms, avp_str(s), &created);
    if (created) v->i = (i64)avp_map_len(c->forms);
    return (int)v->i;
}

/* Whether the mnemonic writes no GPR or FPR through its first operand (stores, compares, branches, moves to
 * special registers, cache and sync operations). */
static bool no_def(const char *op, size_t n) {
    static const char *const P[] = {"st", "cmp", "b", "mt", "tw", "dcb", "icb", "sync", "isync", "eieio", "fcmp", "psq_st"};
    for (size_t i = 0; i < AVP_ARRAY_LEN(P); i++)
        if (n >= strlen(P[i]) && !strncmp(op, P[i], strlen(P[i]))) return true;
    return false;
}

/* The canonical id of one row on one side; vals: register name -> val.i the id of the value it holds (absent: the
 * live-in value, named by the register). The row's definitions are entered into vals. */
static int canon_row(AvpArena *a, Canon *c, AvpMap *vals, const char *norm) {
    const char *p = norm;
    while (*p && *p != ' ') p++;
    size_t opn = (size_t)(p - norm);
    bool defs_first = !no_def(norm, opn);
    bool update = norm[0] == 'l' && opn >= 2 && (norm[opn - 1] == 'u' || (opn >= 3 && norm[opn - 2] == 'u' && norm[opn - 1] == 'x'));
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putn(&b, norm, opn);
    const char *defs[2] = {NULL, NULL};
    int ndefs = 0, operand = 0;
    while (*p) {
        if (*p == ',') operand++;
        if (*p == ' ' || *p == ',' || *p == '(' || *p == ')') {
            avp_buf_putc(&b, *p++);
            continue;
        }
        const char *q = p;
        while (*q && *q != ' ' && *q != ',' && *q != '(' && *q != ')') q++;
        size_t n = (size_t)(q - p);
        if (is_reg_token(p, n)) {
            const char *r = avp_sprintf(a, "%.*s", (int)n, p);
            bool is_def = defs_first && operand == 0 && ndefs == 0;
            bool is_base = update && *q == ')';
            AvpVal *v = avp_map_get(vals, avp_str(r));
            if (is_def) {
                avp_buf_putz(&b, "D");
            } else {
                if (v) avp_buf_printf(&b, "v%lld", (long long)v->i);
                else avp_buf_printf(&b, "in:%s", r);
            }
            if ((is_def || is_base) && ndefs < 2) defs[ndefs++] = r;
        } else {
            avp_buf_putn(&b, p, n);
        }
        p = q;
    }
    int id = intern(c, b.p ? b.p : "");
    for (int k = 0; k < ndefs; k++) {
        bool created = false;
        avp_map_put(vals, avp_str(defs[k]), &created)->i = k ? intern(c, avp_sprintf(a, "%d+base", id)) : id;
    }
    return id;
}

SvOrderDiff sv_order_diff(AvpArena *a, const SvORow *retail, size_t nr, const SvORow *ours, size_t no) {
    SvOrderDiff d = {false, -1, -1, -1, -1, NULL};
    size_t n = nr < no ? nr : no, i = 0;
    while (i < n && !strcmp(retail[i].norm, ours[i].norm)) i++;
    if (i == n && nr == no) {
        d.why = "no row differs";
        return d;
    }
    d.row = (i64)i;
    if (i >= nr || i >= no) {
        d.why = avp_sprintf(a, "row %zu: an instruction present on one side only", i);
        return d;
    }
    Canon c = {avp_map_new()};
    AvpMap *vr = avp_map_new(), *vo = avp_map_new();
    int *cr = AVP_NEWN(a, int, SV_ORDER_WINDOW + 1), *co = AVP_NEWN(a, int, SV_ORDER_WINDOW + 1);
    MaskCount *mc = AVP_NEWN(a, MaskCount, 2 * SV_ORDER_WINDOW + 2);
    size_t nmc = 0, end = 0, k = 0;
    int nonzero = 0;
    bool moved = false; /* some position holds another instruction */
    for (size_t t = i; t < n && t < i + SV_ORDER_WINDOW; t++, k++) {
        if (t > i && (retail[t].target || ours[t].target || retail[t - 1].branch || ours[t - 1].branch)) break;
        cr[k] = canon_row(a, &c, vr, retail[t].norm);
        co[k] = canon_row(a, &c, vo, ours[t].norm);
        if (cr[k] != co[k]) moved = true;
        const char *ma[2] = {avp_sprintf(a, "%d", cr[k]), avp_sprintf(a, "%d", co[k])};
        for (int s = 0; s < 2; s++) {
            int before = 0;
            for (size_t x = 0; x < nmc; x++)
                if (!strcmp(mc[x].mask, ma[s])) before = mc[x].count;
            tally(mc, &nmc, ma[s], s ? -1 : 1);
            int after = before + (s ? -1 : 1);
            nonzero += (after != 0) - (before != 0);
        }
        if (nonzero == 0 && moved) {
            end = t + 1;
            break;
        }
    }
    size_t span = k;
    avp_map_free(c.forms);
    avp_map_free(vr);
    avp_map_free(vo);
    if (!moved) {
        d.why = avp_sprintf(a, "rows %zu..%zu hold the same instructions in the same order with other registers (first: "
                               "ours `%s`, retail `%s`)",
                            i, i + (span ? span - 1 : 0), ours[i].norm, retail[i].norm);
        return d;
    }
    if (!end) {
        d.why = avp_sprintf(a, "from row %zu no window within one block holds the same instructions on both sides (ours "
                               "`%s`, retail `%s`)",
                            i, ours[i].norm, retail[i].norm);
        return d;
    }
    /* the first position whose instruction differs, and where each side has the other's */
    size_t f = 0;
    while (f < end - i && cr[f] == co[f]) f++;
    d.reorder = true;
    d.end = (i64)end;
    d.row = (i64)(i + f);
    for (size_t x = f + 1; x < end - i && d.ours_at < 0; x++)
        if (co[x] == cr[f]) d.ours_at = (i64)(i + x);
    for (size_t x = f + 1; x < end - i && d.retail_at < 0; x++)
        if (cr[x] == co[f]) d.retail_at = (i64)(i + x);
    d.why = avp_sprintf(a, "rows %zu..%zu hold the same %zu instructions in another order (registers defined there "
                           "renamed): retail has `%s` at row %lld, ours at row %lld",
                        i, end - 1, end - i, retail[i + f].norm, (long long)d.row, (long long)d.ours_at);
    return d;
}

/* ---- the condition's words (pure) ------------------------------------------------------------------------------ */

const char *sv_sched_held_kind(AvpArena *a, const char *held) {
    if (!held || !*held) return "";
    if (strstr(held, "can-issue")) return "can-issue 0";
    const char *e = strstr(held, "(edge ");
    const char *s = e ? e + 6 : NULL;
    if (!s) {
        e = strstr(held, ", edge ");
        s = e ? e + 7 : NULL;
    }
    if (!s) return strstr(held, "no reason") ? "no reason found" : "";
    const char *q = s;
    while (*q && *q != ' ' && *q != ',') q++;
    const char *kind = avp_sprintf(a, "%.*s", (int)(q - s), s);
    if (strstr(held, "earliest cycle")) return avp_sprintf(a, "%s latency", kind);
    return kind;
}

const char *sv_sched_cause(AvpArena *a, const MwtSchedPair *p) {
    if (!p) return "schedule: no trace";
    if (p->both_ready) return avp_sprintf(a, "schedule: %s", mwt_sched_key_name(p->key));
    const char *k = sv_sched_held_kind(a, avp_cz(a, p->held));
    return avp_sprintf(a, "schedule: held by %s", *k ? k : "?");
}

static const char *pair_words(AvpArena *a, const MwtSchedPair *p, int block_index, const char *ours_first) {
    if (p->both_ready)
        return avp_sprintf(a, "lost to `%s` on %.*s (the winner's number first) at cycle %d of block %d (index %d), pass %d",
                           ours_first, AVP_SF(p->key_words), p->cycle, p->block, block_index, p->pass);
    return avp_sprintf(a, "was not ready when `%s` issued at cycle %d of block %d (index %d), pass %d: %.*s", ours_first,
                       p->cycle, p->block, block_index, p->pass, AVP_SF(p->held));
}

const char *sv_sched_condition(AvpArena *a, const char *retail_first, const char *ours_first, const MwtSchedPair *p,
                               int block_index, const MwtSchedPair *p1, int block1_index, bool p1_same) {
    const char *head = avp_sprintf(a, "retail issues `%s` first: in ours it %s", retail_first,
                                   pair_words(a, p, block_index, ours_first));
    if (!(p->both_ready && p->key == MWT_KEY_ORDER)) {
        if (!p1) return avp_sprintf(a, "%s; pass %d decided", head, p->pass);
        if (p1_same)
            return avp_sprintf(a, "%s; pass %d decided (in pass 1 it also %s)", head, p->pass,
                               pair_words(a, p1, block1_index, ours_first));
        return avp_sprintf(a, "%s; pass %d decided (pass 1 issued `%s` first: %s at cycle %d of block %d (index %d))", head,
                           p->pass, retail_first,
                           p1->both_ready ? avp_sprintf(a, "on %.*s", AVP_SF(p1->key_words))
                                          : avp_sprintf(a, "the other was not ready: %.*s", AVP_SF(p1->held)),
                           p1->cycle, p1->block, block1_index);
    }
    if (!p1) return avp_sprintf(a, "%s; pass 2 kept its input order, which pass 1 made (pass 1's selection of the two not "
                                   "found in its trace)",
                                head);
    if (p1->both_ready && p1->key == MWT_KEY_ORDER)
        return avp_sprintf(a, "%s; pass 2 kept its input order, which pass 1 made: in pass 1 it %s; both passes keep the "
                              "program order",
                           head, pair_words(a, p1, block1_index, ours_first));
    return avp_sprintf(a, "%s; pass 2 kept its input order, which pass 1 made: in pass 1 it %s; pass 1 decided", head,
                       pair_words(a, p1, block1_index, ours_first));
}

/* ---- the traced compiles (cached in the unit's work) ----------------------------------------------------------- */

typedef struct {
    MwtSchedRun run;
    bool ok;
    const char *error;
} SvSchedTraced;

static void jset(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }
static AvpJson *jz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

static const char *trace_dir(AvpArena *a, SvUnitWork *w) {
    return avp_path_join(a, avp_path_join(a, w->sc.workdir, "trace"), avp_sprintf(a, "%d", ++w->trace_counter));
}

static void add_secs(SvRun *run, double t0) {
    double d = avp_time() - t0;
    run->trace_seconds += d;
    run->trace_kind_seconds[SV_TRACE_SCHED] += d;
    run->ntraces++;
}

/* The scheduler's trace of fn in text (text.p NULL: our unmodified text, every function of the unit in one compile),
 * cached per function and text. */
static const SvSchedTraced *sched_trace(SvRun *run, SvUnitWork *w, const char *fn, LvS text) {
    AvpArena *a = run->a;
    if (!w->sched_traces) w->sched_traces = avp_map_new();
    const char *sha = text.p ? avp_sha1_hex(a, text.p, text.n) : "";
    bool created = false;
    AvpVal *v = avp_map_put(w->sched_traces, avp_str(avp_sprintf(a, "%s\x1f%s", fn, sha)), &created);
    if (!created) return v->p;
    SvSchedTraced *t = AVP_NEW(a, SvSchedTraced);
    v->p = t;
    const char *src = w->base && w->base->ok ? w->base->src : NULL;
    if (!src) {
        t->error = "the unmodified scratch copy did not compile";
        run->traces_failed++;
        return t;
    }
    const char *dir = trace_dir(a, w);
    if (!text.p) {
        if (!w->sched_unit && !w->sched_unit_error) {
            MwtSchedUnit *u = AVP_NEW(a, MwtSchedUnit);
            const char *why = NULL;
            double t0 = avp_time();
            if (mwt_sched_unit_run(a, w->u->name, src, NULL, NULL, avp_path_join(a, dir, "sched"), u, &why)) w->sched_unit = u;
            else w->sched_unit_error = why ? why : "?";
            add_secs(run, t0);
            if (!w->sched_unit) run->traces_failed++;
        }
        if (!w->sched_unit) {
            t->error = w->sched_unit_error;
            return t;
        }
        t->ok = mwt_sched_unit_fn(a, w->sched_unit, fn, &t->run);
        if (!t->ok) t->error = t->run.error ? t->run.error : "?";
        return t;
    }
    const char *sdir = avp_path_join(a, dir, "src");
    avp_makedirs(sdir, true);
    src = avp_path_join(a, sdir, w->sc.basename);
    if (!avp_write_file(src, text.p, text.n)) {
        t->error = avp_sprintf(a, "cannot write %s", src);
        run->traces_failed++;
        return t;
    }
    double t0 = avp_time();
    t->ok = mwt_sched_run(a, w->u->name, src, NULL, NULL, avp_path_join(a, dir, "sched"), fn, &t->run);
    add_secs(run, t0);
    if (!t->ok) {
        t->error = t->run.error ? t->run.error : "?";
        run->traces_failed++;
    }
    return t;
}

static const MwtSchedBlock *block_of(const MwtSchedFunc *f, int pass, int seq) {
    const MwtSchedPass *p = mwt_sched_pass(f, pass);
    return p && seq >= 0 && (size_t)seq < p->blocks.n ? &p->blocks.v[seq] : NULL;
}

/* The pass's block and node of an instruction by its pcode (stable across passes). */
static bool find_pcode(const MwtSchedFunc *f, int pass, u64 pcode, int *block, int *node) {
    const MwtSchedPass *p = mwt_sched_pass(f, pass);
    for (size_t bi = 0; p && bi < p->blocks.n; bi++)
        for (size_t ni = 0; ni < p->blocks.v[bi].nodes.n; ni++)
            if (p->blocks.v[bi].nodes.v[ni].pcode == pcode) {
                *block = (int)bi;
                *node = (int)ni;
                return true;
            }
    return false;
}

static bool find_ra(const MwtSchedFunc *f, int pass, i64 ra, int *block, int *node) {
    const MwtSchedPass *p = mwt_sched_pass(f, pass);
    for (size_t bi = 0; p && bi < p->blocks.n; bi++)
        for (size_t ni = 0; ni < p->blocks.v[bi].nodes.n; ni++)
            if (p->blocks.v[bi].nodes.v[ni].ra == ra) {
                *block = (int)bi;
                *node = (int)ni;
                return true;
            }
    return false;
}

static AvpJson *pair_json(AvpArena *a, const MwtSchedFunc *f, const MwtSchedPair *p) {
    AvpJson *o = mwt_sched_pair_json(a, f, p);
    const MwtSchedBlock *b = block_of(f, p->pass, p->block);
    if (b) {
        jset(a, o, "block_index", avp_json_int(a, b->index));
        jset(a, o, "first_label", avp_json_strz(a, mwt_sched_node_label(a, &b->nodes.v[p->first])));
        jset(a, o, "second_label", avp_json_strz(a, mwt_sched_node_label(a, &b->nodes.v[p->second])));
    }
    if (!p->both_ready) jset(a, o, "held_kind", avp_json_strz(a, sv_sched_held_kind(a, avp_cz(a, p->held))));
    return o;
}

/* The self-check's gaps: `trace` ones make a model gap (the block named), `after-scheduler` ones are facts. */
static bool sched_gaps(AvpArena *a, SvRecord *rec, const MwtSchedCheck *ck, AvpJson *j, bool state) {
    AvpJson *jg = avp_json_arr(a);
    bool trace_gap = false;
    for (size_t i = 0; i < ck->gaps.n; i++) {
        const MwtSchedGap *g = &ck->gaps.v[i];
        AvpJson *o = avp_json_obj(a);
        jset(a, o, "stage", avp_json_str(a, g->stage));
        jset(a, o, "pass", avp_json_int(a, g->pass));
        jset(a, o, "block", avp_json_int(a, g->block));
        jset(a, o, "why", avp_json_str(a, g->why));
        avp_json_arr_push(a, jg, o);
        bool tr = avp_str_eqz(g->stage, "trace");
        if (!state) {
            trace_gap = trace_gap || tr;
            continue;
        }
        const char *where = g->block >= 0 ? avp_sprintf(a, "pass %d block %d%s", g->pass, g->block,
                                                        g->pos >= 0 ? avp_sprintf(a, " position %d", g->pos) : "")
                                          : "the function";
        if (tr) {
            if (!trace_gap) sv_note(a, rec, "sched trace self-check: TRACE-GAP trace in %s: %.*s", where, AVP_SF(g->why));
            trace_gap = true;
        } else {
            sv_evidence(a, rec, "sched trace: a later pass moved an instruction (after-scheduler, %s): %.*s", where,
                        AVP_SF(g->why));
        }
    }
    if (j) {
        jset(a, j, "self_check", avp_json_strz(a, trace_gap ? "TRACE-GAP" : ck->gaps.n ? "after-scheduler" : "ok"));
        jset(a, j, "gaps", jg);
    }
    return trace_gap;
}

/* ---- the phase hooks ------------------------------------------------------------------------------------------- */

static const char *row_text(AvpArena *a, const CcFn *fn, i64 i) {
    return fn && i >= 0 && (size_t)i < fn->nrows ? avp_cz(a, fn->rows[i].text) : "?";
}

int sv_trace_schedule(SvP2 *p, const SvOrderDiff *d, const CcCompareResult *res) {
    SvRun *run = p->run;
    if (!run->trace_kind[SV_TRACE_SCHED] || !d->reorder) return 0;
    AvpArena *a = run->a;
    SvRecord *rec = p->rec;
    const SvSchedTraced *t = sched_trace(run, p->w, rec->function, (LvS){0});
    AvpJson *j = avp_json_obj(a);
    rec->trace = j;
    jset(a, j, "kind", avp_json_strz(a, "sched"));
    jset(a, j, "function", avp_json_strz(a, rec->function));
    jset(a, j, "ok", avp_json_bool(t->ok));
    jset(a, j, "error", jz(a, t->error));
    jset(a, j, "against", avp_json_arr(a));
    if (!t->ok) {
        sv_note(a, rec, "sched trace: could not be made (%s): the order is stated from the rows only", t->error);
        return 0;
    }
    p->sched = &t->run;
    jset(a, j, "seconds", avp_json_float(a, avp_py_round(t->run.total_secs, 2)));
    const MwtSchedFunc *f = t->run.fn;
    if (sched_gaps(a, rec, &t->run.check, j, true)) rec->model_gap = true;
    else sv_evidence(a, rec, "sched trace self-check holds: every selection replayed, %d instruction(s) and %d register(s) "
                             "against the object",
                     t->run.check.ops_checked, t->run.check.regs_checked);
    /* ours issues the instruction at d->row first; retail's first is ours' row d->ours_at */
    const CcFn *ofn = res ? res->compiled_fn : NULL;
    const char *ours_first = row_text(a, ofn, d->row), *retail_first = row_text(a, ofn, d->ours_at);
    int bo = -1, no = -1, br = -1, nr = -1;
    bool lo = mwt_sched_locate(f, 2, (int)d->row, &bo, &no), lr = mwt_sched_locate(f, 2, (int)d->ours_at, &br, &nr);
    if (!lo || !lr || bo != br) {
        const char *why = !lo || !lr ? avp_sprintf(a, "row %lld or row %lld is no node of a block pass 2 scheduled",
                                                   (long long)d->row, (long long)d->ours_at)
                                     : avp_sprintf(a, "rows %lld and %lld are in pass 2's blocks %d and %d", (long long)d->row,
                                                   (long long)d->ours_at, bo, br);
        jset(a, j, "located", avp_json_strz(a, why));
        sv_evidence(a, rec, "sched trace: %s: not one scheduling decision", why);
        return -1;
    }
    MwtSchedPair p2;
    const char *why = NULL;
    if (!mwt_sched_pair(a, f, 2, bo, no, nr, &p2, &why)) {
        rec->trace_condition = avp_sprintf(a, "the sched trace cannot answer the pair (%s)", why);
        rec->condition = rec->trace_condition;
        jset(a, j, "condition", avp_json_strz(a, rec->condition));
        return 1;
    }
    jset(a, j, "pass2", pair_json(a, f, &p2));
    const MwtSchedBlock *b2 = block_of(f, 2, bo);
    MwtSchedPair p1, *pp1 = NULL;
    int b1i = -1;
    bool p1_same = true;
    {
        /* the same two instructions in pass 1 (pcodes are stable across passes) */
        int c1 = -1, n1o = -1, c1r = -1, n1r = -1;
        if (find_pcode(f, 1, b2->nodes.v[p2.first].pcode, &c1, &n1o) &&
            find_pcode(f, 1, b2->nodes.v[p2.second].pcode, &c1r, &n1r) && c1 == c1r &&
            mwt_sched_pair(a, f, 1, c1, n1o, n1r, &p1, &why)) {
            pp1 = &p1;
            p1_same = p1.first == n1o;
            b1i = block_of(f, 1, c1)->index;
            jset(a, j, "pass1", pair_json(a, f, &p1));
        }
    }
    bool pass2_kept = p2.both_ready && p2.key == MWT_KEY_ORDER;
    const char *cond = sv_sched_condition(a, retail_first, ours_first, &p2, b2->index, pp1, b1i, p1_same);
    /* pass 2 kept pass 1's order (program order): pass 1 decided */
    const MwtSchedPair *decided = pass2_kept && pp1 ? pp1 : &p2;
    const char *key = sv_sched_cause(a, decided);
    jset(a, j, "decided_pass", avp_json_int(a, decided->pass));
    jset(a, j, "cause_key", avp_json_strz(a, key));
    jset(a, j, "condition", avp_json_strz(a, cond));
    rec->trace_condition = cond;
    rec->condition = cond;
    return 1;
}

i64 sv_first_differing_row(const CcFn *retail, const CcFn *ours) {
    if (!retail || !ours) return -1;
    size_t n = retail->nrows < ours->nrows ? retail->nrows : ours->nrows;
    for (size_t i = 0; i < n; i++)
        if (!avp_str_eq(retail->rows[i].norm, ours->rows[i].norm)) return (i64)i;
    return retail->nrows == ours->nrows ? -1 : (i64)n;
}

const char *sv_trace_temps_order(SvP2 *p, const SvRaNode *xs, size_t nx, const SvRaNode *ys, size_t ny) {
    SvRun *run = p->run;
    if (!run->trace_kind[SV_TRACE_SCHED] || p->helper) return NULL;
    AvpArena *a = run->a;
    const SvSchedTraced *t = sched_trace(run, p->w, p->rec->function, (LvS){0});
    if (!t->ok) return avp_sprintf(a, "the sched trace could not be made (%s)", t->error);
    const MwtSchedFunc *f = t->run.fn;
    if (sched_gaps(a, p->rec, &t->run.check, NULL, false))
        return "the sched trace's self-check has a gap at stage trace: the order of their definitions is not stated";
    /* the row's instruction: pass 2's node at the row, the same instruction in pass 1, its regalloc number */
    i64 row = p->res ? sv_first_differing_row(p->res->retail_fn, p->res->compiled_fn) : -1;
    int b2 = -1, n2 = -1, bx = -1, nxn = -1;
    if (row < 0 || !mwt_sched_locate(f, 2, (int)row, &b2, &n2))
        return avp_sprintf(a, "row %lld is no node of a block the scheduler's pass 2 scheduled", (long long)row);
    if (!find_pcode(f, 1, block_of(f, 2, b2)->nodes.v[n2].pcode, &bx, &nxn))
        return avp_sprintf(a, "row %lld's instruction is no node of a block the scheduler's pass 1 scheduled", (long long)row);
    const MwtSchedBlock *b = block_of(f, 1, bx);
    i64 r0 = b->nodes.v[nxn].ra;
    const SvRaNode *x = NULL, *y = NULL;
    for (size_t i = 0; i < nx && !x; i++)
        if (xs[i].def_at == r0) x = &xs[i];
    if (!x)
        return avp_sprintf(a, "the row's instruction (regalloc #%lld) first defines none of the temporaries in ours' register",
                           (long long)r0);
    int ny_node = -1;
    i64 best = -1;
    for (size_t i = 0; i < ny; i++) {
        int by = -1, nyy = -1;
        if (ys[i].def_at < 0 || ys[i].def_at == r0 || !find_ra(f, 1, ys[i].def_at, &by, &nyy) || by != bx) continue;
        i64 dist = ys[i].def_at > r0 ? ys[i].def_at - r0 : r0 - ys[i].def_at;
        if (!y || dist < best) {
            y = &ys[i];
            best = dist;
            ny_node = nyy;
        }
    }
    if (!y)
        return avp_sprintf(a, "no temporary in retail's register is defined in pass 1's block %d (index %d) with the row's "
                              "%s (regalloc #%lld)",
                           bx, b->index, x->def_op ? x->def_op : "?", (long long)r0);
    MwtSchedPair pr;
    const char *why = NULL;
    if (!mwt_sched_pair(a, f, 1, bx, nxn, ny_node, &pr, &why))
        return avp_sprintf(a, "the sched trace cannot answer the pair (%s)", why);
    bool x_first = pr.first == nxn;
    const SvRaNode *fa = x_first ? x : y, *sa = x_first ? y : x;
    const char *head = avp_sprintf(a, "the scheduler's pass 1 issues the %s (regalloc #%lld, defining node %d) before the %s "
                                      "(#%lld, node %d) at cycle %d of block %d (index %d)",
                                   fa->def_op ? fa->def_op : "?", (long long)fa->def_at, fa->index,
                                   sa->def_op ? sa->def_op : "?", (long long)sa->def_at, sa->index, pr.cycle, bx, b->index);
    if (pr.both_ready) return avp_sprintf(a, "%s by %.*s", head, AVP_SF(pr.key_words));
    return avp_sprintf(a, "%s: the other was not ready: %.*s", head, AVP_SF(pr.held));
}

const char *sv_trace_sched_against(SvP2 *p, LvS text, const char *what) {
    SvRun *run = p->run;
    if (!run->trace_kind[SV_TRACE_SCHED] || !p->sched || !p->sched->fn || p->helper) return NULL;
    AvpArena *a = run->a;
    if (p->sched_against_runs >= SV_SCHED_AGAINST_MAX || (p->deadline && avp_time() > p->deadline)) {
        run->traces_skipped++;
        return "sched --against: not traced (the stop's trace budget is used)";
    }
    p->sched_against_runs++;
    const SvSchedTraced *t = sched_trace(run, p->w, p->symbol, text);
    if (!t->ok || !t->run.fn) return avp_sprintf(a, "sched --against: could not be made (%s)", t->error ? t->error : "?");
    AvpStrVec d = mwt_sched_diff(a, p->sched->fn, t->run.fn);
    AvpJson *lines = avp_json_arr(a);
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t shown = 0;
    for (size_t i = 0; i < d.n; i++) {
        avp_json_arr_push(a, lines, avp_json_str(a, d.v[i]));
        /* the first selection that differs and both decisions; node-only lines are counted */
        bool sel = avp_str_startswith(d.v[i], AVP_S("first difference")) || avp_str_startswith(d.v[i], AVP_S("A: ")) ||
                   avp_str_startswith(d.v[i], AVP_S("B: ")) || avp_str_startswith(d.v[i], AVP_S("no selection differs"));
        if (sel && shown < 3) {
            avp_buf_printf(&b, "%s%.*s", shown ? "; " : "", AVP_SF(d.v[i]));
            shown++;
        }
    }
    AvpJson *arr = p->rec->trace ? avp_json_getz(p->rec->trace, "against") : NULL;
    if (arr) {
        AvpJson *o = avp_json_obj(a);
        jset(a, o, "what", avp_json_strz(a, what));
        jset(a, o, "lines", lines);
        jset(a, o, "self_check", avp_json_strz(a, t->run.check.gaps.n ? "TRACE-GAP" : "ok"));
        avp_json_arr_push(a, arr, o);
    }
    return avp_sprintf(a, "sched --against (A ours, B the %s): %s%s", what, b.n ? b.p : "no selection line",
                       t->run.check.gaps.n ? " [B's self-check: TRACE-GAP]" : "");
}
