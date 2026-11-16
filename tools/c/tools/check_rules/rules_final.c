/* rules_final: the Checker's driver and its last phases (check_rules.py L2205-2229, L4261-4414): run(),
 * apply_exceptions with _exception_cover (R0), apply_allow, compute_units, blocking, build_functions, exit_code, and
 * the stage-3 dump of the state run() leaves. */
#include "rules_check.h"
#include <string.h>

/* ---- run() ---- */
void rl_checker_run(RlChecker *ck) {
    AvpArena *a = ck->a;
    ck->checked.n = 0;
    ck->checked_set = avp_set_new();
    for (size_t i = 0; i < ck->scan.n; i++) avp_vec_push(&ck->checked, ck->scan.v[i]);
    AvpStrVec extra = {0};
    AvpSet *seen = avp_set_new(); /* scan + extra, for the membership tests */
    for (size_t i = 0; i < ck->scan.n; i++) avp_set_add(seen, ck->scan.v[i]);
    for (size_t i = 0; i < ck->scan.n && !ck->failed; i++) {
        AvpStr p = ck->scan.v[i];
        const AvpStrVec *cl = rl_checker_closure(ck, p);
        for (size_t k = 0; k < cl->n; k++) {
            AvpStr h = cl->v[k];
            if (!avp_str_eq(h, p) && avp_set_add(seen, h)) avp_vec_push(&extra, h);
        }
    }
    avp_set_free(seen);
    for (size_t i = 0; i < extra.n; i++) avp_vec_push(&ck->checked, extra.v[i]);
    avp_vec_free(&extra);
    for (size_t i = 0; i < ck->checked.n; i++) avp_set_add(ck->checked_set, ck->checked.v[i]);
    if (ck->failed) return;
    rl_checker_prepare(ck);
    for (size_t i = 0; i < ck->checked.n && !ck->failed; i++) rl_check_file(ck, rl_checker_parse(ck, ck->checked.v[i]));
    if (ck->failed) return;
    rl_check_cross_file(ck);
    if (ck->failed || ck->error) return;
    rl_check_vtables(ck);
    if (ck->failed || ck->error) return;
    rl_apply_exceptions(ck);
    rl_apply_allow(ck);
    rl_compute_units(ck);
    rl_build_functions(ck);
    (void)a;
}

/* ---- RULE-EXCEPTION (L4261-4340) ---- */
static size_t code_points(AvpStr s) {
    size_t n = 0;
    for (size_t i = 0; i < s.n; i++)
        if (((u8)s.p[i] & 0xC0) != 0x80) n++;
    return n;
}

static void exception_cover(const RlParsed *P, const RlComment *c, i32 *lo, i32 *hi) {
    *lo = c->line;
    *hi = c->end_line;
    if (c->trailing) return;
    const RlTok *toks = P->toks.v;
    size_t n = P->toks.n, idx = n;
    for (size_t i = 0; i < n; i++)
        if (toks[i].line > c->end_line) {
            idx = i;
            break;
        }
    if (idx == n) return;
    for (size_t f = 0; f < P->p.funcs.n; f++) {
        const RlFunc *fn = &P->p.funcs.v[f];
        if (fn->line == toks[idx].line && fn->head <= (i32)idx && (i32)idx <= fn->body) {
            *hi = fn->end_line;
            return;
        }
    }
    int depth = 0;
    size_t k = idx;
    while (k < n) {
        const RlTok *t = &toks[k];
        if (RL_TEQ(t, "(") || RL_TEQ(t, "[")) depth++;
        else if (RL_TEQ(t, ")") || RL_TEQ(t, "]")) depth--;
        else if (depth <= 0 && (RL_TEQ(t, ";") || RL_TEQ(t, "{") || RL_TEQ(t, "}"))) break;
        k++;
    }
    if (k > n - 1) k = n - 1;
    *hi = toks[k].line;
}

void rl_apply_exceptions(RlChecker *ck) {
    AvpArena *a = ck->a;
    static const ShsmRe *form;
    if (!form) form = shsm_re("RULE-EXCEPTION:\\s*(R\\d{1,2}(?:\\s*/\\s*R\\d{1,2})*)\\s*,\\s*(\\S.*?)\\s*$", SHSM_RE_S);
    ck->exceptions.n = 0;
    for (size_t pi = 0; pi < ck->checked.n; pi++) {
        AvpStr path = ck->checked.v[pi];
        RlParsed *P = rl_checker_parse(ck, path);
        for (size_t ci = 0; ci < P->comments.n; ci++) {
            const RlComment *c = &P->comments.v[ci];
            if (!avp_str_contains(c->text, AVP_S("RULE-EXCEPTION"))) continue;
            AvpStr text = avp_str_strip(c->text);
            ShsmReMatch m;
            RlException *ex = RL_PUSH(a, &ck->exceptions);
            ex->path = path;
            ex->line = c->line;
            ex->reason = AVP_S("");
            ex->problem = AVP_S("");
            if (shsm_re_search(form, text, 0, AVP_END, &m)) {
                AvpStrVec parts = avp_str_split(shsm_re_group(&m, 1), AVP_S("/"), -1);
                for (size_t k = 0; k < parts.n; k++) *RL_PUSH(a, &ex->rules) = avp_str_strip(parts.v[k]);
                avp_vec_free(&parts);
                ex->reason = avp_str_strip(avp_str_rstrip_chars(avp_str_strip(shsm_re_group(&m, 2)), AVP_S("*/")));
                AvpStrVec bad = {0};
                for (size_t k = 0; k < ex->rules.n; k++)
                    if (!rl_tier_of(avp_cz(a, ex->rules.v[k]))) avp_vec_push(&bad, ex->rules.v[k]);
                AvpStrVec words = avp_str_split(ex->reason, (AvpStr){0}, -1);
                if (bad.n)
                    ex->problem = rl_fmt(a, "unknown rule %.*s", AVP_SF(avp_str_join(a, AVP_S(", "), bad.v, bad.n)));
                else if (code_points(ex->reason) < 12 || words.n < 3)
                    ex->problem = AVP_S("reason too short: give the reason the bytes give");
                else
                    ex->valid = true;
                avp_vec_free(&bad);
                avp_vec_free(&words);
            } else {
                ex->problem = AVP_S("malformed: write '// RULE-EXCEPTION: <rule>, <reason>'");
            }
            exception_cover(P, c, &ex->lo, &ex->hi);
            if (!ex->valid)
                rl_add(ck, P, c->line, "R0", "free", rl_fmt(a, "RULE-EXCEPTION %.*s", AVP_SF(ex->problem)), (AvpStr){0});
        }
    }
    size_t nf = ck->findings.n; /* a snapshot: the R0 findings added below are not tested */
    for (size_t fi = 0; fi < nf; fi++) {
        if (strcmp(ck->findings.v[fi].status, "violation")) continue;
        for (size_t ei = 0; ei < ck->exceptions.n; ei++) {
            RlException *ex = &ck->exceptions.v[ei];
            RlFinding *f = &ck->findings.v[fi];
            if (!avp_str_eq(ex->path, f->path) || !ex->valid) continue;
            bool listed = false;
            for (size_t k = 0; k < ex->rules.n; k++)
                if (avp_str_eqz(ex->rules.v[k], f->rule)) listed = true;
            if (!listed) continue;
            if (ex->lo <= f->line && f->line <= ex->hi) {
                if (!strcmp(f->tier, "match")) {
                    if (!ex->refused) {
                        ex->refused = true;
                        const char *rule = f->rule;
                        rl_add(ck, rl_checker_parse(ck, f->path), ex->line, "R0", "free",
                               rl_fmt(a,
                                      "RULE-EXCEPTION cannot waive the [match] rule %s: the function stays "
                                      "MATCH-PENDING",
                                      rule),
                               (AvpStr){0});
                    }
                    continue;
                }
                f->status = "excepted";
                f->reason = ex->reason;
                ex->suppressed++;
                break;
            }
        }
    }
}

/* ---- the allow list (L4343-4361) ---- */
static AvpStr path_func_key(AvpArena *a, const RlFinding *f) {
    if (!f->func.p) return rl_fmt(a, "%.*s\x02", AVP_SF(f->path));
    return rl_fmt(a, "%.*s\x01%.*s", AVP_SF(f->path), AVP_SF(f->func));
}

void rl_apply_allow(RlChecker *ck) {
    AvpArena *a = ck->a;
    AvpSet *allowed_asm = avp_set_new();
    for (size_t i = 0; i < ck->findings.n; i++) {
        const RlFinding *f = &ck->findings.v[i];
        if (!strcmp(f->rule, "R18") && !strcmp(f->status, "allowed")) avp_set_add(allowed_asm, path_func_key(a, f));
    }
    AvpStr source = ck->cfg.allow_source.n ? ck->cfg.allow_source : AVP_S("the allow list");
    for (size_t i = 0; i < ck->findings.n; i++) {
        RlFinding *f = &ck->findings.v[i];
        if (!strcmp(f->status, "violation") && !strcmp(f->rule, "R17") && !strcmp(f->tier, "advisory") &&
            avp_str_contains(f->msg, AVP_S("'register'")) && avp_set_has(allowed_asm, path_func_key(a, f))) {
            f->status = "allowed";
            f->reason = AVP_S("register variable of a listed inline-asm helper");
            continue;
        }
        if (strcmp(f->status, "violation") || !f->func.n) continue;
        AvpStr shrt = f->func, before, after;
        while (avp_str_partition(shrt, AVP_S("::"), &before, &after)) shrt = after;
        if (ck->cfg.rulings &&
            (avp_set_has(ck->cfg.rulings, rl_fmt(a, "%s\x01%.*s", f->rule, AVP_SF(f->func))) ||
             avp_set_has(ck->cfg.rulings, rl_fmt(a, "%s\x01%.*s", f->rule, AVP_SF(shrt))))) {
            f->status = "allowed";
            f->reason = rl_fmt(a, "review ruling in %.*s", AVP_SF(source));
        }
    }
    avp_set_free(allowed_asm);
}

/* ---- units, functions, exit code (L4364-4414) ---- */
void rl_compute_units(RlChecker *ck) {
    AvpArena *a = ck->a;
    AvpMap *affected = avp_map_new(); /* header -> AvpSet* of units */
    for (size_t i = 0; i < ck->paths.n; i++) {
        AvpStr path = ck->paths.v[i];
        if (!avp_str_startswith(path, AVP_S("src/")) || !rl_is_source_path(a, path)) continue;
        const AvpStrVec *cl = rl_checker_closure(ck, path);
        for (size_t k = 0; k < cl->n; k++) {
            bool created = false;
            AvpVal *v = avp_map_put(affected, cl->v[k], &created);
            if (created) v->p = avp_set_new();
            avp_set_add(v->p, path);
        }
    }
    AvpMap *sorted = avp_map_new(); /* memo: header -> AvpStrVec* */
    for (size_t i = 0; i < ck->findings.n; i++) {
        RlFinding *f = &ck->findings.v[i];
        AvpVal *v = avp_map_get(affected, f->path);
        if (!v) {
            AvpStr *u = AVP_NEW(a, AvpStr);
            *u = f->path;
            f->units = u;
            f->nunits = 1;
            continue;
        }
        bool created = false;
        AvpVal *s = avp_map_put(sorted, f->path, &created);
        if (created) {
            AvpStrVec sv = avp_set_sorted(v->p);
            AvpStr *arr = AVP_NEWN(a, AvpStr, sv.n + 1);
            for (size_t k = 0; k < sv.n; k++) arr[k] = avp_str_dup(a, sv.v[k]); /* the set owns its keys */
            AvpStrVec *keep = AVP_NEW(a, AvpStrVec);
            keep->v = arr;
            keep->n = sv.n;
            avp_vec_free(&sv);
            s->p = keep;
        }
        const AvpStrVec *keep = s->p;
        f->units = keep->v;
        f->nunits = keep->n;
    }
}

bool rl_blocking(RlChecker *ck, const RlFinding *f) {
    if (strcmp(f->status, "violation") || strcmp(f->tier, "match")) return false;
    for (size_t i = 0; i < f->nunits; i++)
        if (avp_str_eqz(rl_checker_status_of(ck, f->units[i]), "Matching")) return true;
    return false;
}

int rl_exit_code(RlChecker *ck) {
    for (size_t i = 0; i < ck->findings.n; i++)
        if (rl_blocking(ck, &ck->findings.v[i])) return 1;
    return 0;
}

void rl_build_functions(RlChecker *ck) {
    AvpArena *a = ck->a;
    ck->functions.n = 0;
    AvpMap *by_path = avp_map_new(); /* path -> RL_VEC(size_t)* of finding indices */
    typedef RL_VEC(size_t) Idx;
    for (size_t i = 0; i < ck->findings.n; i++) {
        bool created = false;
        AvpVal *v = avp_map_put(by_path, ck->findings.v[i].path, &created);
        if (created) v->p = AVP_NEW(a, Idx);
        *RL_PUSH(a, (Idx *)v->p) = i;
    }
    for (size_t pi = 0; pi < ck->checked.n; pi++) {
        AvpStr path = ck->checked.v[pi];
        RlParsed *P = rl_checker_parse(ck, path);
        AvpVal *bv = avp_map_get(by_path, path);
        const Idx *fl = bv ? bv->p : NULL;
        AvpStr unit_status = rl_checker_status_of(ck, path);
        AvpStr ukey = rl_unit_key(a, path);
        for (size_t fi = 0; fi < P->p.funcs.n; fi++) {
            const RlFunc *fn = &P->p.funcs.v[fi];
            RlFunctionRec *r = RL_PUSH(a, &ck->functions);
            r->path = path;
            r->name = fn->name;
            r->line = fn->line;
            r->end_line = fn->end_line;
            r->unit_status = unit_status;
            AvpSet *rules = avp_set_new();
            for (size_t k = 0; fl && k < fl->n; k++) {
                const RlFinding *x = &ck->findings.v[fl->v[k]];
                if (!(fn->line <= x->line && x->line <= fn->end_line) || strcmp(x->status, "violation")) continue;
                int t = 0;
                while (t < r->ntiers && strcmp(r->tiers[t], x->tier)) t++;
                if (t == r->ntiers && t < 5) r->tiers[r->ntiers++] = x->tier;
                if (t < 5) r->counts[t]++;
                if (!strcmp(x->tier, "match")) avp_set_add(rules, avp_str(x->rule));
            }
            AvpStrVec rs = avp_set_sorted(rules);
            for (size_t k = 0; k < rs.n; k++) *RL_PUSH(a, &r->match_rules) = avp_str_dup(a, rs.v[k]);
            avp_vec_free(&rs);
            avp_set_free(rules);
            bool pending = false;
            for (int t = 0; t < r->ntiers; t++)
                if (!strcmp(r->tiers[t], "match") && r->counts[t] > 0) pending = true;
            AvpStr shrt = fn->name, before, after;
            while (avp_str_partition(shrt, AVP_S("::"), &before, &after)) shrt = after;
            if ((fn->is_asm || P->is_header) && ck->cfg.original_asm &&
                avp_set_has(ck->cfg.original_asm, rl_fmt(a, "%.*s\x01%.*s", AVP_SF(ukey), AVP_SF(shrt))))
                r->status = "ORIGINAL-ASM";
            else if (pending)
                r->status = "MATCH-PENDING";
            else
                r->status = "ok";
        }
    }
}

/* ---- the stage-3 dump ---- */
static void line_end(AvpJsonW *w, AvpBuf *b) {
    avp_jsonw_finish(w);
    avp_buf_putc(b, '\n');
}

static void strs(AvpJsonW *w, const AvpStr *v, size_t n) {
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < n; i++) avp_jsonw_str(w, v[i]);
    avp_jsonw_end_array(w);
}

static void tree_or_null(AvpJsonW *w, const AvpJson *v) {
    if (v) avp_jsonw_tree(w, v);
    else avp_jsonw_null(w);
}

void rl_json_exception(AvpJsonW *w, const RlException *ex, bool cover) {
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_str(w, "path", ex->path);
    avp_jsonw_kv_i64(w, "line", ex->line);
    avp_jsonw_key(w, AVP_S("rules"));
    strs(w, ex->rules.v, ex->rules.n);
    avp_jsonw_kv_str(w, "reason", ex->reason);
    avp_jsonw_kv_bool(w, "valid", ex->valid);
    avp_jsonw_kv_str(w, "problem", ex->problem);
    avp_jsonw_kv_i64(w, "suppressed", ex->suppressed);
    if (cover) {
        avp_jsonw_key(w, AVP_S("cover"));
        avp_jsonw_begin_array(w);
        avp_jsonw_i64(w, ex->lo);
        avp_jsonw_i64(w, ex->hi);
        avp_jsonw_end_array(w);
    }
    if (ex->refused) avp_jsonw_kv_bool(w, "refused", true);
    avp_jsonw_end_object(w);
}

void rl_json_function(AvpJsonW *w, const RlFunctionRec *r) {
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_str(w, "path", r->path);
    avp_jsonw_kv_str(w, "name", r->name);
    avp_jsonw_kv_i64(w, "line", r->line);
    avp_jsonw_kv_i64(w, "end_line", r->end_line);
    avp_jsonw_kv_str(w, "status", avp_str(r->status));
    avp_jsonw_key(w, AVP_S("counts"));
    avp_jsonw_begin_object(w);
    for (int t = 0; t < r->ntiers; t++) avp_jsonw_kv_i64(w, r->tiers[t], r->counts[t]);
    avp_jsonw_end_object(w);
    avp_jsonw_key(w, AVP_S("match_rules"));
    strs(w, r->match_rules.v, r->match_rules.n);
    avp_jsonw_kv_str(w, "unit_status", r->unit_status);
    avp_jsonw_end_object(w);
}

void rl_json_r13(AvpJsonW *w, const RlR13 *s) {
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_i64(w, "classes", s->classes);
    avp_jsonw_key(w, AVP_S("matched"));
    avp_jsonw_begin_array(w);
    for (size_t i = 0; i < s->matched.n; i++) {
        const RlR13Match *m = &s->matched.v[i];
        avp_jsonw_begin_object(w);
        avp_jsonw_kv_str(w, "class", m->cls);
        avp_jsonw_key(w, AVP_S("retail"));
        tree_or_null(w, m->retail);
        avp_jsonw_key(w, AVP_S("vtable"));
        tree_or_null(w, m->vtable);
        avp_jsonw_kv_str(w, "by", m->by);
        avp_jsonw_end_object(w);
    }
    avp_jsonw_end_array(w);
    avp_jsonw_key(w, AVP_S("unmatched"));
    strs(w, s->unmatched.v, s->unmatched.n);
    avp_jsonw_key(w, AVP_S("skipped"));
    strs(w, s->skipped.v, s->skipped.n);
    avp_jsonw_kv_i64(w, "added", s->added);
    avp_jsonw_kv_i64(w, "missing", s->missing);
    avp_jsonw_kv_i64(w, "order", s->order);
    avp_jsonw_kv_i64(w, "override", s->override_);
    avp_jsonw_kv_i64(w, "advisory", s->advisory);
    avp_jsonw_key(w, AVP_S("note"));
    if (s->note.p) avp_jsonw_str(w, s->note);
    else avp_jsonw_null(w);
    avp_jsonw_end_object(w);
}

void rl_dump_final(RlChecker *ck, AvpBuf *b) {
    AvpJsonW w;
    avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
    avp_jsonw_begin_object(&w);
    avp_jsonw_key(&w, AVP_S("checked"));
    strs(&w, ck->checked.v, ck->checked.n);
    avp_jsonw_end_object(&w);
    line_end(&w, b);
    for (size_t i = 0; i < ck->findings.n; i++) {
        avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
        rl_finding_json_full(&w, &ck->findings.v[i]);
        line_end(&w, b);
    }
    for (size_t i = 0; i < ck->exceptions.n; i++) {
        avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
        avp_jsonw_begin_object(&w);
        avp_jsonw_key(&w, AVP_S("exception"));
        rl_json_exception(&w, &ck->exceptions.v[i], true);
        avp_jsonw_end_object(&w);
        line_end(&w, b);
    }
    for (size_t i = 0; i < ck->functions.n; i++) {
        avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
        avp_jsonw_begin_object(&w);
        avp_jsonw_key(&w, AVP_S("function"));
        rl_json_function(&w, &ck->functions.v[i]);
        avp_jsonw_end_object(&w);
        line_end(&w, b);
    }
    avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
    avp_jsonw_begin_object(&w);
    avp_jsonw_key(&w, AVP_S("r13"));
    rl_json_r13(&w, &ck->r13);
    avp_jsonw_end_object(&w);
    line_end(&w, b);
    avp_jsonw_init(&w, b, &AVP_JSON_COMPACT);
    avp_jsonw_begin_object(&w);
    avp_jsonw_kv_i64(&w, "exit", rl_exit_code(ck));
    avp_jsonw_end_object(&w);
    line_end(&w, b);
}
