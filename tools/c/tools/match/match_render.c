/* match: text rendering (tools/match.py _pct, function_line, summary_line, unit_lines, diff_lines) and
 * resolve_symbol. */
#include "match.h"

#include "shsm_mangle.h"

static bool status_is(const AvpJson *rec, const char *s) {
    return avp_str_eqz(avp_json_get_str(rec, "status", AVP_S("")), s);
}

AvpStr match_pct(AvpArena *a, const AvpJson *rec) {
    const AvpJson *p = avp_json_getz(rec, "percent");
    if (!avp_json_is_num(p)) return AVP_S("      - ");
    return avp_pyfmt(a, "%7.2f%%", avp_json_f64(p, 0.0));
}

static int cmp_str(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

AvpStr match_function_line(AvpArena *a, const AvpJson *rec) {
    AvpStr status = avp_json_get_str(rec, "status", AVP_S(""));
    bool pending = status_is(rec, MATCH_PENDING);
    AvpStr detail;
    if (status_is(rec, MATCH_EXACT)) {
        const AvpJson *vr = avp_json_getz(rec, "value_rows");
        detail = avp_json_truthy(vr) ? avp_pyfmt(a, "%d row(s) by value", (i64)avp_json_len(vr)) : AVP_S("");
    } else if (status_is(rec, MATCH_MISSING)) {
        detail = AVP_S("not in the object");
    } else if (status_is(rec, MATCH_EXTRA)) {
        detail = AVP_S("not in retail");
    } else if (pending) {
        const AvpJson *mr = avp_json_getz(rec, "match_rules");
        AvpSet *set = avp_set_new();
        for (size_t i = 0; avp_json_is_arr(mr) && i < avp_json_len(mr); i++)
            avp_set_add(set, avp_json_str_or(avp_json_at(mr, i), AVP_S("")));
        AvpStrVec rules = avp_set_sorted(set);
        AvpStr joined = avp_str_join(a, AVP_S(","), rules.v, rules.n);
        avp_vec_free(&rules);
        avp_set_free(set);
        detail = avp_pyfmt(a, "byte-exact, [match] %S", &joined);
    } else {
        detail = avp_pyfmt(a, "%d rows %d tgt", avp_json_get_i64(rec, "rows", 0), avp_json_get_i64(rec, "targets", 0));
    }
    AvpStr pct = (status_is(rec, MATCH_EXACT) || status_is(rec, MATCH_EXTRA)) ? AVP_S("") : match_pct(a, rec);
    AvpStr shown = pending ? AVP_S("") : detail;
    AvpStr name = avp_json_get_str(rec, "name", AVP_S(""));
    AvpStr line = avp_pyfmt(a, "%-13S %8S %-17S %#7x  %S", &status, &pct, &shown, avp_json_get_i64(rec, "size", 0), &name);
    if (pending) line = avp_pyfmt(a, "%S  (%S)", &line, &detail);
    return line;
}

AvpStr match_summary_line(AvpArena *a, const AvpJson *result) {
    const AvpJson *sec = avp_json_getz(result, "seconds");
    AvpStr unit = avp_json_get_str(result, "unit", AVP_S(""));
    if (!avp_json_truthy(avp_json_getz(result, "ok")))
        return avp_pyfmt(a, "%S: COMPILE FAILED (%.2fs)", &unit, avp_json_get_f64(sec, "total", 0.0));
    MatchSummary s = match_summarise(result);
    AvpStrVec parts = {0};
    avp_vec_push(&parts, avp_pyfmt(a, "%S: %d/%d exact", &unit, s.exact, s.total));
    static const int order[] = {MATCH_S_DIFF, MATCH_S_MISSING, MATCH_S_PENDING, MATCH_S_EXTRA};
    static const char *const lower[] = {"exact", "diff", "missing", "extra", "match-pending"};
    AvpStrVec extra = {0};
    for (size_t i = 0; i < AVP_ARRAY_LEN(order); i++)
        if (s.counts[order[i]]) avp_vec_push(&extra, avp_pyfmt(a, "%d %s", s.counts[order[i]], lower[order[i]]));
    if (extra.n) {
        AvpStr j = avp_str_join(a, AVP_S(", "), extra.v, extra.n);
        avp_vec_push(&parts, avp_pyfmt(a, "(%S)", &j));
    }
    avp_vec_free(&extra);
    if (s.worst) {
        AvpStr wn = avp_json_get_str(s.worst, "name", AVP_S(""));
        AvpStr wp = avp_str_strip(match_pct(a, s.worst));
        avp_vec_push(&parts, avp_pyfmt(a, "worst %S %S", &wn, &wp));
    }
    const AvpJson *lay = avp_json_getz(result, "layout");
    if (avp_json_truthy(lay)) {
        AvpStr st = avp_json_get_str(lay, "status", AVP_S(""));
        avp_vec_push(&parts, avp_pyfmt(a, "layout %S", &st));
    }
    const AvpJson *rr = avp_json_getz(result, "rules");
    if (avp_json_truthy(rr)) avp_vec_push(&parts, match_rules_verdict(a, rr));
    AvpStr psrc = avp_json_get_str(result, "percent_source", AVP_S("None"));
    avp_vec_push(&parts, avp_pyfmt(a, "| %% from %S | compile %.2fs%s, compare %.2fs, total %.2fs", &psrc,
                                   avp_json_get_f64(sec, "compile", 0.0),
                                   avp_json_truthy(avp_json_getz(result, "cached")) ? " (cached)" : "",
                                   avp_json_get_f64(sec, "compare", 0.0), avp_json_get_f64(sec, "total", 0.0)));
    AvpStr line = avp_str_join(a, AVP_S(" "), parts.v, parts.n);
    avp_vec_free(&parts);
    return line;
}

AvpStrVec match_unit_lines(AvpArena *a, const AvpJson *result, bool show_all, i64 limit) {
    const AvpJson *recs = avp_json_getz(result, "functions");
    size_t n = avp_json_is_arr(recs) ? avp_json_len(recs) : 0;
    AvpStrVec lines = {0};
    for (size_t i = 0; i < n; i++) avp_vec_push(&lines, match_function_line(a, avp_json_at(recs, i)));
    if (show_all || !limit || (i64)lines.n <= limit) return lines;
    size_t hidden = 0;
    AvpStrVec shown = {0};
    for (size_t i = 0; i < n; i++) {
        if (status_is(avp_json_at(recs, i), MATCH_EXACT)) hidden++;
        else avp_vec_push(&shown, lines.v[i]);
    }
    AvpStrVec out = shsm_cap_lines(a, shown.v, shown.n, limit, false, "use --all");
    if (hidden) avp_vec_push(&out, avp_pyfmt(a, "(%d exact functions not listed, use --all)", (i64)hidden));
    avp_vec_free(&shown);
    avp_vec_free(&lines);
    return out;
}

AvpStrVec match_diff_lines(AvpArena *a, const CcCompareResult *cres, const AvpJson *rec, i64 context, bool all_rows,
                           i64 limit, bool show_all) {
    const CcCounts *c = &cres->counts;
    AvpStr dem = rec ? avp_json_get_str(rec, "demangled", AVP_S("")) : cres->name;
    AvpStr status = rec ? avp_json_get_str(rec, "status", AVP_S(""))
                        : (cres->exact ? AVP_S("EXACT") : AVP_S("DIFF"));
    AvpStr head = avp_pyfmt(a,
                            "--- %S  [%S]  %S, %d rows differ, %d target mismatches, %d unresolved, retail %d / "
                            "compiled %d rows",
                            &cres->name, &dem, &status, (i64)(c->replace + c->delete_ + c->insert),
                            (i64)c->target_mismatches, (i64)c->unresolved, (i64)c->retail_rows, (i64)c->compiled_rows);
    const AvpJson *pv = rec ? avp_json_getz(rec, "percent") : NULL;
    if (avp_json_is_num(pv)) head = avp_str_concat(a, head, avp_pyfmt(a, ", %.2f%%", avp_json_f64(pv, 0.0)));
    size_t np = cres->npairs;
    const CcTdList *notes = cres->raw ? &cres->raw->notes : NULL;
    /* by_value: {retail row index: detail}, the last note of an index wins */
    AvpIMap *by_value = avp_imap_new();
    for (size_t i = 0; notes && i < notes->n; i++) avp_imap_put(by_value, notes->v[i].i, NULL)->p = (void *)&notes->v[i].detail;
    bool *keep = AVP_NEWN(a, bool, np + 1);
    for (size_t i = 0; i < np; i++) {
        const CcPair *p = &cres->pairs[i];
        bool hot = cc_pair_differs(p) || p->target || (p->retail && avp_imap_has(by_value, p->retail->index));
        if (all_rows) keep[i] = true;
        else if (hot) {
            for (i64 j = (i64)i - context; j <= (i64)i + context; j++)
                if (j >= 0 && j < (i64)np) keep[j] = true;
        }
    }
    AvpStrVec body = {0};
    i64 last = -1;
    for (size_t i = 0; i < np; i++) {
        if (!keep[i]) continue;
        const CcPair *p = &cres->pairs[i];
        if (last >= 0 && (i64)i != last + 1) avp_vec_push(&body, AVP_S("   ..."));
        last = (i64)i;
        const char *m;
        switch (p->status) {
        case CC_REPLACE: m = "!"; break;
        case CC_DELETE: m = "-"; break;
        case CC_INSERT: m = "+"; break;
        default: m = p->target ? (strcmp(p->target, "mismatch") == 0 ? "T" : "?") : " "; break;
        }
        const AvpStr *value = NULL;
        if (p->retail && p->status == CC_EQUAL) {
            AvpVal *v = avp_imap_get(by_value, p->retail->index);
            if (v && ((const AvpStr *)v->p)->n) value = v->p;
        }
        if (value) m = "=";
        AvpStr ri = p->retail ? avp_pyfmt(a, "%3d", (i64)p->retail->index) : AVP_S("   ");
        AvpStr ci = p->compiled ? avp_pyfmt(a, "%3d", (i64)p->compiled->index) : AVP_S("   ");
        AvpStr rt = p->retail ? p->retail->text : AVP_S(""), ct = p->compiled ? p->compiled->text : AVP_S("");
        avp_vec_push(&body, avp_pyfmt(a, "%s %S %-44S | %S %S", m, &ri, &rt, &ci, &ct));
        if (value) avp_vec_push(&body, avp_pyfmt(a, "      value: %S", value));
        if (p->target && strcmp(p->target, "mismatch") == 0) {
            const CcTargetDiff *t = NULL;
            for (size_t k = 0; cres->target_mismatches && k < cres->target_mismatches->n; k++)
                if (cres->target_mismatches->v[k].i == p->retail->index) t = &cres->target_mismatches->v[k];
            AvpStr d = (t && t->detail.n) ? t->detail : (p->detail.p && p->detail.n) ? p->detail : AVP_S("differs");
            avp_vec_push(&body, avp_pyfmt(a, "      target: %S", &d));
        }
    }
    avp_imap_free(by_value);
    if (!body.n) avp_vec_push(&body, AVP_S("   (no differing rows)"));
    const char *hint = avp_sprintf(a, "cut at --limit %lld: rerun with --all or --limit 0 to show every row",
                                   (long long)limit);
    AvpStrVec capped = shsm_cap_lines(a, body.v, body.n, show_all ? 0 : limit, show_all, hint);
    avp_vec_free(&body);
    AvpStrVec out = {0};
    avp_vec_push(&out, head);
    avp_vec_extend(&out, capped.v, capped.n);
    avp_vec_free(&capped);
    return out;
}

AvpStr match_resolve_symbol(AvpArena *a, AvpStr query, const AvpStr *names, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (avp_str_eq(names[i], query)) return query;
    AvpStr *dem = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) dem[i] = avp_str(shsm_demangle_or_name(a, avp_cz(a, names[i])));
    AvpStrVec hits = {0};
    for (size_t i = 0; i < n; i++)
        if (avp_str_eq(dem[i], query) || avp_str_eqz(query, shsm_qualified(a, avp_cz(a, names[i]))))
            avp_vec_push(&hits, names[i]);
    if (hits.n == 1) return hits.v[0];
    if (!hits.n) {
        AvpStr q = avp_str_lower(a, query);
        for (size_t i = 0; i < n; i++)
            if (avp_str_contains(avp_str_lower(a, names[i]), q) || avp_str_contains(avp_str_lower(a, dem[i]), q))
                avp_vec_push(&hits, names[i]);
    }
    if (hits.n == 1) return hits.v[0];
    if (!hits.n) shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(a, "no function of this unit matches %R", &query).p);
    avp_sort(hits.v, hits.n, sizeof(AvpStr), cmp_str, NULL, false);
    AvpStr joined = avp_str_join(a, AVP_S(", "), hits.v, AVP_MIN(hits.n, (size_t)8));
    if (hits.n > 8) joined = avp_str_concat(a, joined, AVP_S(" ..."));
    shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(a, "%R is ambiguous: %S", &query, &joined).p);
}
