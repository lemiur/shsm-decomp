/* solver_reg: Phase 2, register / schedule decisions. For each register stop (and an `other` stop whose wall
 * classes include register/schedule):
 *   - our map: the -sym on compile of the scratch copy (control 4: the target's instructions must equal the
 *     baseline's) read with `dtk dwarf dump`; retail: the sibling's DWARF registers when the pair is exact with
 *     reliable offsets, else wall's renaming applied to our registers;
 *   - the first differing variable (wall's first differing row read through the maps);
 *   - the locals comparison against the sibling's Final and Debug DWARF (missing, extra, order, scope, type);
 *   - the condition from the measured declaration-order rule, asserted only when the rule predicts our own
 *     callee-saved registers (model check);
 *   - candidates (solver_cand.c) and probes (solver_probe.c), on the target and on the same-file helpers ours
 *     expands into it;
 *   - the verdict (sv_assign_verdict). */
#include "solver.h"
#include "wall.h"

/* ---- our map --------------------------------------------------------------------------------------------------- */

static void ensure_sym(SvRun *run, SvUnitWork *w) {
    if (w->sym_tried) return;
    w->sym_tried = true;
    AvpArena *a = run->a;
    static const char *const SYM[] = {"-sym", "on"};
    SvFlags fl = {0};
    fl.extra = SYM;
    fl.nextra = 2;
    w->sym = sv_build(&w->sc, a, w->text0, "sym", &fl);
    if (!w->sym->ok) {
        w->sym_error = avp_sprintf(a, "the -sym on compile failed: %s", w->sym->error ? w->sym->error : "?");
        return;
    }
    CcRepo cr = {run->r->root, run->r->version};
    const char *dtk = cc_repo_dtk(a, &cr);
    const char *txt = avp_path_join(a, w->sym->dir, "dwarf.txt");
    const char *argv[] = {dtk, "dwarf", "dump", w->sym->obj, "-o", txt};
    ShsmRun res = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        res = shsm_run(a, argv, AVP_ARRAY_LEN(argv), run->r->root, 120);
        avp_try_end(&t);
    } else {
        w->sym_error = avp_sprintf(a, "dtk dwarf dump could not run: %s", t.msg ? t.msg : "?");
        return;
    }
    AvpBytes b = {0};
    if (!shsm_run_ok(&res) || !avp_read_all(a, txt, &b)) {
        w->sym_error = avp_sprintf(a, "dtk dwarf dump failed: %.*s", AVP_SF(avp_str_strip(shsm_run_output(a, &res))));
        return;
    }
    SvDwFns *fns = AVP_NEW(a, SvDwFns);
    *fns = sv_dtk_dump_parse(a, avp_strn((const char *)b.v, b.n));
    w->symfns = fns;
}

/* Control 4: the target's rows (normalised: symbols and labels masked) in the -sym on object equal the baseline's. */
static bool sym_control(SvUnitWork *w, const char *symbol, const char **why) {
    const CcCompareResult *x = sv_result(w->base, symbol), *y = sv_result(w->sym, symbol);
    if (!x || !y || !x->compiled_fn || !y->compiled_fn) {
        *why = "the target is not in both objects";
        return false;
    }
    const CcFn *f = x->compiled_fn, *g = y->compiled_fn;
    if (f->nrows != g->nrows) {
        *why = "the -sym on object has another row count";
        return false;
    }
    for (size_t i = 0; i < f->nrows; i++)
        if (!avp_str_eq(f->rows[i].norm, g->rows[i].norm)) {
            *why = "an instruction differs under -sym on";
            return false;
        }
    return true;
}

/* ---- the sibling's DWARF --------------------------------------------------------------------------------------- */

/* sibling.exe fn <name> [--sibling] [--build debug] --json, cached; NULL when it has no such function. */
static const SvSibFn *sib_get(SvRun *run, const char *name, bool debug, bool by_name) {
    AvpArena *a = run->a;
    if (!run->sib_cache) run->sib_cache = avp_map_new();
    const char *key = avp_sprintf(a, "%s\x1f%d\x1f%d", name, debug, by_name);
    bool created = false;
    AvpVal *v = avp_map_put(run->sib_cache, avp_str(key), &created);
    if (!created) return v->p;
    v->p = NULL;
    const char *exe = avp_join(a, run->r->root, "build", "tools", "sibling.exe");
    const char *argv[8];
    size_t n = 0;
    argv[n++] = exe;
    argv[n++] = "fn";
    argv[n++] = name;
    if (by_name) argv[n++] = "--sibling";
    if (debug) {
        argv[n++] = "--build";
        argv[n++] = "debug";
    }
    argv[n++] = "--json";
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmRun res = shsm_run(a, argv, n, run->r->root, 120);
        if (shsm_run_ok(&res)) {
            SvSibFn *f = AVP_NEW(a, SvSibFn);
            if (sv_sibling_parse(a, res.out, f)) v->p = f;
        }
        avp_try_end(&t);
    }
    return v->p;
}

/* The target's Final and Debug DWARF: the pair first, then a same-named sibling function. */
static void sibling_of(SvP2 *p, const char *name, bool pair_first) {
    const SvSibFn *f = pair_first ? sib_get(p->run, name, false, false) : NULL;
    bool by_name = false;
    if (!f) {
        f = sib_get(p->run, name, false, true);
        by_name = true;
    }
    if (f) p->sib = *f;
    const SvSibFn *g = sib_get(p->run, name, true, by_name);
    if (!g && !f) g = sib_get(p->run, name, true, true);
    if (g) p->dbg = *g;
    p->sib_exact = p->sib.ok && p->sib.tier && !strcmp(p->sib.tier, "exact") && p->sib.offsets_reliable;
}

/* ---- our locals from the source -------------------------------------------------------------------------------- */

SvVars sv_source_locals(AvpArena *a, LvCtx *x) {
    SvVars out = {0};
    for (size_t i = 0; i < x->locals.n; i++) {
        const LvLocal *l = &x->locals.v[i];
        SvVar v = {0};
        v.name = l->name;
        AvpBuf b;
        avp_buf_init(&b, a);
        for (i32 k = 0; k < l->s->decl->nwords; k++) {
            if (!strcmp(l->s->decl->words[k], "static")) continue;
            if (b.n) avp_buf_putc(&b, ' ');
            avp_buf_putz(&b, l->s->decl->words[k]);
        }
        /* the declarator's stars */
        for (i32 k = l->dt->a; k < l->dt->name_i; k++)
            if (TEQ(&x->code.v[k], "*")) avp_buf_putz(&b, " *");
        v.type = b.p ? b.p : "";
        v.line = sv_line_of(x->text, x->code.v[l->s->a].s);
        v.index = (i64)i;
        v.depth = 0;
        for (size_t bi = 0; bi < x->blocks.n; bi++) {
            const LvBlock *bl = &x->blocks.v[bi];
            for (i32 k = 0; k < bl->L.n; k++)
                if (bl->L.v[k] == l->s) {
                    v.depth = bl->owner ? 1 : 0;
                    v.block = bl->owner ? avp_sprintf(a, "%p", (void *)bl->L.v) : NULL;
                }
        }
        if (l->s->decl->is_static) continue; /* a static local is no register variable */
        avp_vec_push(&out, v);
    }
    return out;
}

/* Declarations under a `RULE-EXCEPTION: R30` comment: the names they declare and the comment-and-declaration text. */
static void r30_declarations(SvP2 *p) {
    LvCtx *x = p->x;
    AvpArena *a = p->run->a;
    for (i32 i = 0; i < x->comments.n; i++) {
        const LvTok *c = &x->comments.v[i];
        LvS ct = {c->t, c->n};
        if (!lv_s_contains(ct, LVS("RULE-EXCEPTION: R30"))) continue;
        sv_note(a, p->rec, "%s%s carries: %.*s", p->helper ? "helper " : "", p->helper ? p->helper : "the function",
                AVP_SF(avp_str_replace(a, lv_strip(ct), AVP_S("\n"), AVP_S(" "), -1)));
        p->rec->rule_exception = true;
        for (size_t q = 0; q < x->all_stmts.n; q++) {
            LvStmt *s = x->all_stmts.v[q];
            if (s->kind != LS_DECL || x->code.v[s->a].s < c->e) continue;
            for (i32 d = 0; d < s->decl->ndtors; d++) avp_vec_push(&p->protected_names, (char *)s->decl->dtors[d].name);
            i64 ls = lv_line_start(x, c->s), le = lv_line_end(x, x->code.v[s->b].e);
            avp_vec_push(&p->protected_text, avp_cz(a, lv_slice(x, ls, le)));
            break;
        }
    }
}

/* ---- ours / retail at the first differing row ------------------------------------------------------------------ */

typedef AVP_VEC(int) IntVec;

static IntVec row_regs(const char *text) {
    IntVec out = {0};
    if (!text) return out;
    const char *p = text;
    while (*p && *p != ' ') p++; /* the mnemonic */
    while (*p) {
        while (*p && (*p == ' ' || *p == ',' || *p == '(' || *p == ')')) p++;
        const char *q = p;
        while (*q && *q != ' ' && *q != ',' && *q != '(' && *q != ')') q++;
        char buf[16];
        size_t n = (size_t)(q - p);
        if (n && n < sizeof buf) {
            memcpy(buf, p, n);
            buf[n] = 0;
            int r = sv_reg_num(buf);
            avp_vec_push(&out, r);
        } else if (n) {
            avp_vec_push(&out, -1);
        }
        p = q;
    }
    return out;
}

static const char *reg_text(AvpArena *a, int r) {
    return r < 0 ? "?" : avp_sprintf(a, "%c%d", r >= 100 ? 'f' : 'r', r % 100);
}

/* The variable of our map in register r (callee-saved registers name one variable; volatile ones only when one). */
static const char *var_in(const SvDwFn *map, int r, bool *is_param) {
    const char *hit = NULL;
    int n = 0;
    for (size_t i = 0; i < map->locals.n; i++)
        if (sv_reg_num(map->locals.v[i].loc) == r) {
            hit = map->locals.v[i].name;
            *is_param = false;
            n++;
        }
    for (size_t i = 0; i < map->params.n; i++)
        if (sv_reg_num(map->params.v[i].loc) == r) {
            hit = map->params.v[i].name;
            *is_param = true;
            n++;
        }
    if (map->this_loc && sv_reg_num(map->this_loc) == r) {
        hit = "this";
        *is_param = true;
        n++;
    }
    return n == 1 ? hit : NULL;
}

/* Retail's register of one of our variables: the sibling's when paired exactly, else wall's renaming. */
static int retail_reg(const SvP2 *p, const AvpJson *reg, const char *name, int ours) {
    if (p->sib_exact && name) {
        const SvVar *v = sv_vars_find(&p->sib.locals, name);
        if (!v) v = sv_vars_find(&p->sib.params, name);
        int r = v ? sv_reg_num(v->loc) : -1;
        if (r >= 0) return r;
    }
    const AvpJson *sw = reg ? avp_json_getz(reg, "swapped") : NULL;
    for (size_t i = 0; sw && i < avp_json_len(sw); i++) {
        const AvpJson *e = avp_json_at(sw, i);
        const char *c = avp_cz(p->run->a, avp_json_get_str(e, "compiled", AVP_S("")));
        if (sv_reg_num(c) == ours) return sv_reg_num(avp_cz(p->run->a, avp_json_get_str(e, "retail", AVP_S(""))));
    }
    if (reg && avp_json_get_bool(reg, "consistent", false)) return ours;
    return -1;
}

/* ---- the condition --------------------------------------------------------------------------------------------- */

static const char *joined(AvpArena *a, const AvpCzVec *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < v->n; i++) avp_buf_printf(&b, "%s%s", i ? ", " : "", v->v[i]);
    return b.p ? b.p : "";
}

static void condition(SvP2 *p, const SvDwFn *map, const AvpJson *reg) {
    AvpArena *a = p->run->a;
    SvRecord *rec = p->rec;
    size_t n = 0;
    const char **names = AVP_NEWN(a, const char *, p->ours.n + 1);
    int *ours = AVP_NEWN(a, int, p->ours.n + 1), *ret = AVP_NEWN(a, int, p->ours.n + 1);
    for (size_t i = 0; i < p->ours.n; i++) {
        const SvVar *m = sv_vars_find(&map->locals, p->ours.v[i].name);
        if (!m) continue;
        names[n] = p->ours.v[i].name;
        ours[n] = sv_reg_num(m->loc);
        ret[n] = retail_reg(p, reg, names[n], ours[n]);
        n++;
    }
    const char *why = NULL;
    int model = sv_order_model(a, names, ours, n, &why);
    if (model == 0) {
        sv_note(a, rec, "declaration-order rule does not predict our own registers (model gap): %s", why);
        rec->condition = avp_sprintf(a, "%s (no declaration order asserted: the rule does not predict our own "
                                        "registers)", rec->wall_verdict ? rec->wall_verdict : "register/schedule");
        return;
    }
    if (model < 0) {
        sv_evidence(a, rec, "declaration-order rule: %s (nothing to read an order from)", why);
        rec->condition = avp_sprintf(a, "%s (no callee-saved locals to read a declaration order from)",
                                     rec->wall_verdict ? rec->wall_verdict : "register/schedule");
        return;
    }
    sv_evidence(a, rec, "declaration-order rule predicts our own callee-saved registers (model check holds)");
    /* only the locals whose retail register is known and callee-saved in both */
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (sv_reg_callee_saved(ours[i]) && sv_reg_callee_saved(ret[i])) {
            names[k] = names[i];
            ours[k] = ours[i];
            ret[k] = ret[i];
            k++;
        }
    AvpCzVec o = sv_implied_order(a, names, ours, k), r = sv_implied_order(a, names, ret, k);
    bool same = o.n == r.n;
    for (size_t i = 0; same && i < o.n; i++) same = !strcmp(o.v[i], r.v[i]);
    if (k < 2)
        rec->condition = "fewer than two callee-saved locals have a known retail register: no declaration order implied";
    else if (same)
        rec->condition = avp_sprintf(a, "retail's registers imply the same declaration order as ours (%s): the "
                                        "difference is not the declaration order of these locals",
                                     joined(a, &o));
    else
        rec->condition = avp_sprintf(a, "retail's registers imply the declaration order %s; ours declares %s",
                                     joined(a, &r), joined(a, &o));
}

/* ---- same-file helpers ----------------------------------------------------------------------------------------- */

static bool calls_named(AvpArena *a, const CcFn *fn, const char *name) {
    if (!fn) return false;
    WallCall *calls = NULL;
    size_t n = wall_calls(a, fn->rows, fn->nrows, &calls);
    for (size_t i = 0; i < n; i++) {
        const char *c = avp_cz(a, calls[i].name);
        const char *sn = sv_source_name(a, c);
        if (!strcmp(c, name) || (sn && !strcmp(sn, name))) return true;
    }
    return false;
}

/* The functions of the same text the target's body calls, that neither our object nor retail calls here. */
AvpCzVec sv_p2_expanded_helpers(SvP2 *p) {
    AvpCzVec out = {0};
    SvSource *src = p->w->src;
    if (!src || !src->ok || !p->x) return out;
    LvCtx *x = p->x;
    for (i32 k = x->lb + 1; k + 1 < x->rb; k++) {
        const LvTok *t = &x->code.v[k];
        if (t->k != LV_ID || !TEQ(&x->code.v[k + 1], "(")) continue;
        if (k > 0 && (TEQ(&x->code.v[k - 1], ".") || TEQ(&x->code.v[k - 1], "->"))) continue;
        bool def = false;
        for (size_t d = 0; d < src->defs.n && !def; d++)
            if (src->defs.v[d].name == t->t && src->defs.v[d].lbrace != x->fn.lbrace) def = true;
        if (!def) continue;
        bool seen = false;
        for (size_t i = 0; i < out.n; i++) seen = seen || !strcmp(out.v[i], t->t);
        if (seen) continue;
        if (calls_named(p->run->a, p->res->compiled_fn, t->t) || calls_named(p->run->a, p->res->retail_fn, t->t))
            continue;
        avp_vec_push(&out, (char *)t->t);
    }
    return out;
}

/* ---- the DWARF comparison of one function (target or helper) --------------------------------------------------- */

static void dwarf_compare(SvP2 *p, const SvDwFn *map, AvpCzVec *keys) {
    AvpArena *a = p->run->a;
    const char *who = p->helper ? avp_sprintf(a, "helper %s: ", p->helper) : "";
    if (!p->sib.ok && !p->dbg.ok) {
        sv_evidence(a, p->rec, "%sthe sibling has no function of this name (no DWARF to compare)", who);
        avp_vec_push(keys, "no-sibling-dwarf");
        return;
    }
    if (p->sib.ok)
        sv_evidence(a, p->rec, "%ssibling DWARF (Final): %s%s%s", who, p->sib.name,
                    p->sib.tier ? avp_sprintf(a, ", pair tier %s", p->sib.tier) : ", same-named (unpaired)",
                    p->sib_exact ? ", registers are retail's" : ", registers are the sibling's, not retail's");
    else
        sv_evidence(a, p->rec, "%ssibling DWARF: only the Debug build has %s", who, p->dbg.name);
    p->dwarf = sv_dwarf_union(a, p->sib.ok ? &p->sib.locals : NULL, p->dbg.ok ? &p->dbg.locals : NULL);
    p->findings = sv_locals_compare(a, &p->ours, p->sib.ok ? &p->sib.locals : &p->dbg.locals,
                                    p->dbg.ok ? &p->dbg.locals : NULL, map ? &map->locals : NULL);
    if (!p->findings.n) sv_evidence(a, p->rec, "%sthe locals agree with the sibling's DWARF", who);
    for (size_t i = 0; i < p->findings.n; i++) {
        sv_evidence(a, p->rec, "%s%s: %s", who, SV_LC_KEYS[p->findings.v[i].kind], p->findings.v[i].text);
        if (p->findings.v[i].kind == SV_LC_TYPEDEF) continue; /* a typedef name: no cause, no candidate */
        const char *k = SV_LC_KEYS[p->findings.v[i].kind];
        bool seen = false;
        for (size_t j = 0; j < keys->n; j++) seen = seen || !strcmp(keys->v[j], k);
        if (!seen) avp_vec_push(keys, (char *)k);
    }
}

/* A context for fn_symbol on the stop's text; false when the text does not parse there. */
bool sv_p2_open_ctx(SvP2 *p, const char *fn_symbol) {
    AvpTry t;
    p->x = NULL;
    if (AVP_TRY(&t)) {
        p->x = lv_ctx_new(p->run->a, p->text, fn_symbol);
        avp_try_end(&t);
    } else {
        p->x = NULL;
    }
    return p->x != NULL;
}

/* The best scoring candidate of the target (from index c0: exact first, then the fewest differing rows) traced
 * against ours: the regalloc lines of the variables it moves, as evidence. */
static void best_against(SvP2 *p, size_t c0) {
    if ((!p->ra && !p->sched) || p->helper) return;
    const SvCandidate *best = NULL;
    for (size_t i = c0; i < p->rec->candidates.n; i++) {
        const SvCandidate *c = &p->rec->candidates.v[i];
        if (!c->compiled || c->rows_after < 0) continue;
        if (!best || (c->exact && !best->exact) || (c->exact == best->exact && c->rows_after < best->rows_after)) best = c;
    }
    if (!best) return;
    const char *what = avp_sprintf(p->run->a, "best candidate (%s, rows %lld -> %lld)", best->form,
                                   (long long)best->rows_before, (long long)best->rows_after);
    const char *t = sv_trace_against(p, best->text, what);
    if (t) sv_evidence(p->run->a, p->rec, "%s", t);
}

/* ---- the stop -------------------------------------------------------------------------------------------------- */

void sv_phase2_register(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpJson *d, const CcCompareResult *res) {
    AvpArena *a = run->a;
    rec->phase_built = true;
    SvP2 *p = AVP_NEW(a, SvP2);
    p->run = run;
    p->w = w;
    p->rec = rec;
    p->res = res;
    p->symbol = rec->function;
    p->fn_symbol = rec->function;
    p->text = w->text0;
    p->surviving = rec->surviving_source;
    p->deadline = avp_time() + (run->stop_seconds > 0 ? run->stop_seconds : 120);
    p->base = sv_lvscore(a, w->base, rec->function);
    const AvpJson *reg = avp_json_getz(d, "register");
    AvpCzVec keys = {0};
    if (!sv_p2_open_ctx(p, rec->function)) {
        sv_note(a, rec, "the function's text does not parse with levers' parser: no locals, candidates or probes");
        rec->cause = "register: unparsable";
        return;
    }
    p->ours = sv_source_locals(a, p->x);
    r30_declarations(p);

    /* our map, with control 4 */
    ensure_sym(run, w);
    const SvDwFn *map = NULL;
    if (w->sym_error) {
        sv_note(a, rec, "no register map: %s", w->sym_error);
    } else {
        const char *why = NULL;
        if (!sym_control(w, rec->function, &why)) {
            sv_note(a, rec, "-sym on control failed (%s): no register map", why);
        } else {
            i64 lo = res && res->compiled_fn && res->compiled_fn->nrows ? res->compiled_fn->rows[0].addr : -1;
            map = sv_dtk_find(w->symfns, lo, sv_source_name(a, rec->function));
            if (!map) sv_note(a, rec, "the -sym on object's DWARF has no entry for the function");
        }
    }

    /* the sibling's DWARF and the comparison */
    sibling_of(p, rec->function, true);
    dwarf_compare(p, map, &keys);

    /* the order test: the same instructions in another order in one block are the scheduler's decision */
    bool sched_stop = false;
    const char *sched_key = NULL;
    if (res && res->retail_fn && res->compiled_fn) {
        SvORow *ro = NULL, *oo = NULL;
        size_t nro = 0, noo = 0;
        sv_order_rows(a, res->retail_fn, &ro, &nro);
        sv_order_rows(a, res->compiled_fn, &oo, &noo);
        SvOrderDiff od = sv_order_diff(a, ro, nro, oo, noo);
        if (od.reorder) {
            int st = sv_trace_schedule(p, &od, res);
            if (st >= 0) {
                sched_stop = true;
                rec->cls = SV_SCHEDULE;
                sv_evidence(a, rec, "order test: %s", od.why);
                sv_evidence(a, rec, "first differing row: ours `%s`, retail `%s`", rec->ours ? rec->ours : "?",
                            rec->retail ? rec->retail : "?");
                const char *rf = avp_cz(a, res->compiled_fn->rows[od.ours_at].text);
                const char *of = avp_cz(a, res->compiled_fn->rows[od.row].text);
                rec->ours = avp_sprintf(a, "`%s` first (row %lld), retail's first `%s` at row %lld", of, (long long)od.row,
                                        rf, (long long)od.ours_at);
                rec->retail = avp_sprintf(a, "`%s` first (row %lld), ours' first instruction at row %lld",
                                          avp_cz(a, res->retail_fn->rows[od.row].text), (long long)od.row,
                                          (long long)od.retail_at);
                if (st == 1) {
                    const AvpJson *ck = avp_json_getz(rec->trace, "cause_key");
                    if (ck) sched_key = avp_cz(a, avp_json_str_or(ck, AVP_S("")));
                } else {
                    rec->condition = avp_sprintf(a, "retail issues `%s` first, ours issues `%s` first (%s); no scheduler "
                                                    "trace states the decision",
                                                 rf, of, od.why);
                }
            } else {
                sv_evidence(a, rec, "order test: %s; but the scheduler's trace places the two in no one block: register", od.why);
            }
        } else {
            sv_evidence(a, rec, "order test: %s: not an order difference", od.why);
        }
    }

    /* ours / retail: the first differing variable, else the first differing row's registers */
    const char *first_var = NULL;
    int first_ours = -1, first_retail = -1;
    if (!sched_stop) {
        IntVec cr = row_regs(rec->ours), rr = row_regs(rec->retail);
        for (size_t i = 0; i < cr.n && i < rr.n; i++)
            if (cr.v[i] >= 0 && rr.v[i] >= 0 && cr.v[i] != rr.v[i]) {
                first_ours = cr.v[i];
                first_retail = rr.v[i];
                break;
            }
        bool named = false;
        for (size_t i = 0; map && i < cr.n && i < rr.n && !named; i++) {
            if (cr.v[i] < 0 || rr.v[i] < 0 || cr.v[i] == rr.v[i]) continue;
            bool is_param = false;
            const char *v = var_in(map, cr.v[i], &is_param);
            if (!v) continue;
            int r = retail_reg(p, reg, v, cr.v[i]);
            if (r < 0) r = rr.v[i];
            sv_evidence(a, rec, "first differing row: ours `%s`, retail `%s`", rec->ours, rec->retail);
            rec->ours = avp_sprintf(a, "`%s`%s in %s", v, is_param ? " (parameter)" : "", reg_text(a, cr.v[i]));
            rec->retail = avp_sprintf(a, "`%s` in %s", v, reg_text(a, r));
            sv_evidence(a, rec, "first differing variable: `%s`: ours %s, retail %s", v, reg_text(a, cr.v[i]),
                        reg_text(a, r));
            first_var = v;
            first_ours = cr.v[i];
            first_retail = r;
            named = true;
        }
        if (map && !named) sv_note(a, rec, "no variable of our map names the first differing register: wall's row kept");
        avp_vec_free(&cr);
        avp_vec_free(&rr);
    }
    /* the regalloc trace: its self-check is the model check, its nodes state the condition; without it the
     * declaration-order rule */
    bool traced = !sched_stop && sv_trace_register(p, first_var, first_ours, first_retail);
    const char *trace_key = NULL;
    if (sched_stop) {
        /* the condition is the scheduler's (above) */
    } else if (traced) {
        const AvpJson *ck = avp_json_getz(rec->trace, "cause_key");
        if (ck) trace_key = avp_cz(a, avp_json_str_or(ck, AVP_S("")));
    } else if (map) {
        condition(p, map, reg);
    } else {
        rec->condition = rec->wall_verdict ? avp_sprintf(a, "wall: %s", rec->wall_verdict) : NULL;
    }

    /* candidates and probes on the target, then on the helpers ours expands into it */
    if (p->surviving) {
        const SvRuling *ru = sv_rulings_surviving(&run->rulings, w->u->name, w->u->path);
        sv_note(a, rec, "surviving source (solver.tsv: surviving-source %s%s%s): no respelling candidates", ru->arg1,
                *ru->note ? ", " : "", ru->note);
    }
    if (run->diagnose_only) {
        sv_note(a, rec, "--diagnose-only: no candidate or probe compiled");
    } else if (!p->base->ok) {
        sv_note(a, rec, "the unmodified scratch copy does not score the function: %s", p->base->error);
    } else {
        size_t c0 = rec->candidates.n;
        sv_p2_candidates(p);
        best_against(p, c0);
        sv_p2_probes(p);
    }
    AvpCzVec helpers = sv_p2_expanded_helpers(p);
    for (size_t i = 0; i < helpers.n; i++) {
        SvP2 *h = AVP_NEW(a, SvP2);
        *h = *p;
        h->helper = helpers.v[i];
        h->fn_symbol = helpers.v[i];
        h->ours = (SvVars){0};
        h->sib = (SvSibFn){0};
        h->dbg = (SvSibFn){0};
        h->dwarf = (SvVars){0};
        h->findings = (SvLcFindings){0};
        h->protected_names = (AvpCzVec){0};
        h->protected_text = (AvpCzVec){0};
        sv_evidence(a, rec, "same-file helper %s: ours and retail both expand it here (neither calls it)", h->helper);
        if (!sv_p2_open_ctx(h, h->helper)) {
            sv_note(a, rec, "helper %s does not parse: skipped", h->helper);
            continue;
        }
        h->ours = sv_source_locals(a, h->x);
        r30_declarations(h);
        sibling_of(h, h->helper, false);
        AvpCzVec hk = {0};
        dwarf_compare(h, NULL, &hk);
        for (size_t k = 0; k < hk.n; k++) avp_vec_push(&keys, avp_sprintf(a, "helper-%s", hk.v[k]));
        if (!run->diagnose_only && p->base->ok) {
            sv_p2_candidates(h);
            sv_p2_probes(h);
        }
    }
    /* the cause key */
    AvpBuf cb;
    avp_buf_init(&cb, a);
    avp_buf_putz(&cb, "register:");
    if (!keys.n) avp_buf_putz(&cb, " dwarf agrees");
    for (size_t i = 0; i < keys.n; i++) {
        bool dup = false;
        for (size_t j = 0; j < i; j++) dup = dup || !strcmp(keys.v[i], keys.v[j]);
        if (!dup) avp_buf_printf(&cb, "%s %s", i ? "," : "", keys.v[i]);
    }
    if (trace_key) avp_buf_printf(&cb, "; trace: %s", trace_key);
    rec->cause = cb.p;
    /* a schedule record groups by the key that decided or the edge kind that held retail's first */
    if (sched_stop) rec->cause = sched_key && *sched_key ? sched_key : "schedule: no trace";
    avp_vec_free(&keys);
}

i64 sv_line_of(LvS text, i64 pos) {
    i64 line = 1;
    for (i64 i = 0; i < pos && (size_t)i < text.n; i++)
        if (text.p[i] == '\n') line++;
    return line;
}

bool sv_apply_edits(AvpArena *a, LvS text, LvEdit *edits, size_t n, LvS *out) {
    /* sort by start */
    for (size_t i = 1; i < n; i++)
        for (size_t j = i; j > 0 && edits[j - 1].s > edits[j].s; j--) {
            LvEdit t = edits[j];
            edits[j] = edits[j - 1];
            edits[j - 1] = t;
        }
    AvpBuf b;
    avp_buf_init(&b, a);
    i64 at = 0;
    for (size_t i = 0; i < n; i++) {
        if (edits[i].s < at || edits[i].e < edits[i].s || edits[i].e > (i64)text.n) return false;
        avp_buf_put(&b, avp_strn(text.p + at, (size_t)(edits[i].s - at)));
        avp_buf_put(&b, edits[i].text);
        at = edits[i].e;
    }
    avp_buf_put(&b, avp_strn(text.p + at, text.n - (size_t)at));
    *out = avp_buf_str(&b);
    return true;
}
