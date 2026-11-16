/* ml_changes: merge_lane.py's step_changes. func_scores .. classify_decrease are gate_regress's ports (tool_gate);
 * index_source is score_history's C index (tool_score_history), proven equal to the Python on every file of the tree
 * (its D-score_history-1). */
#include "merge_lane.h"
#include "sh_priv.h"
#include "shsm_cli.h"
#include <string.h>

#define F(...) ML_F(ml, __VA_ARGS__)
#define EV(...) ml_ev(ml, ev, F(__VA_ARGS__))

AvpJson *ml_index_source(Ml *ml, AvpArena *a, const char *path, AvpStr text) {
    if (ml->hooks && ml->hooks->index) return ml->hooks->index(ml->hooks->ctx, a, path, text);
    ShIndex *ix = sh_index_source(a, text);
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(a, sh_index_to_json(a, ix), &err);
    if (!doc) avp_throw(AVP_ERR_FORMAT, "index_source: %s", avp_cz(a, avp_json_error_str(a, &err)));
    return doc;
}

static AvpJson *indexer(GateIndexer *ix, AvpArena *a, const char *path, AvpStr text) {
    return ml_index_source((Ml *)ix->ctx, a, path, text);
}

static bool read_lane(void *ctx, AvpArena *a, const char *path, AvpStr *text) {
    Ml *ml = ctx;
    return ml_read_text(a, avp_path_join(a, ml->slot, path), text);
}

static bool read_onto(void *ctx, AvpArena *a, const char *path, AvpStr *text) {
    Ml *ml = ctx;
    AvpStr b;
    if (!ml_show_file(ml, ml_onto_ref(ml), path, &b)) return false;
    *text = ml_decode(a, b);
    return true;
}

static bool exists_lane(void *ctx, const char *rel) {
    Ml *ml = ctx;
    return avp_exists(avp_path_join(ml->a, ml->slot, rel));
}

typedef struct Entry {
    GateScore s;
    const char *path_lane, *path_onto, *label;
    bool edited;
} Entry;
typedef AVP_VEC(Entry *) EntryVec;

static const char *fmt_entry(Ml *ml, const Entry *e) {
    const char *t = e->s.has_to ? F("%6.2f%%", e->s.to) : "gone";
    const char *nm = e->s.name ? e->s.name : (e->s.onto_name ? e->s.onto_name : "?");
    AvpStr cut = avp_u8_slice(avp_str(nm), 0, 44);
    return F("%-44S %7.2f%% -> %-8s %s  [%s]", &cut, e->s.has_from ? e->s.from : 0.0, t, e->s.unit, e->label);
}

static int sample_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Entry *p = *(Entry *const *)x, *q = *(Entry *const *)y;
    int kp = (p->s.has_to ? p->s.to : 0) >= 99.995 ? 0 : 1, kq = (q->s.has_to ? q->s.to : 0) >= 99.995 ? 0 : 1;
    if (kp != kq) return kp - kq;
    int c = strcmp(p->s.unit, q->s.unit);
    if (c) return c;
    return strcmp(p->s.name ? p->s.name : "", q->s.name ? q->s.name : "");
}

static bool load_json_file(Ml *ml, const char *path, AvpJson **doc, const char **err) {
    AvpBytes b = {0};
    if (!avp_read_all(ml->a, path, &b)) {
        *err = avp_cz(ml->a, avp_io_err_str(ml->a));
        return false;
    }
    AvpStr text;
    if (!avp_decode_text(ml->a, (AvpStr){(const char *)b.v, b.n}, AVP_TM_UTF8, &text)) {
        *err = avp_cz(ml->a, avp_io_err_str(ml->a));
        return false;
    }
    AvpJsonError je;
    *doc = avp_json_parse(ml->a, text, &je);
    if (!*doc) {
        *err = avp_cz(ml->a, avp_json_error_str(ml->a, &je));
        return false;
    }
    return true;
}

static const char *num_or_none(Ml *ml, const AvpJson *v) { return gate_json_pystr(ml->a, v); }

const char *ml_step_changes(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    ml_require_no_unmerged(ml);
    ml->ix = (GateIndexer){indexer, ml};
    const char *report_path = avp_path_join(a, ml->slot, ML_REPORT);
    if (!avp_exists(report_path)) ml_fail(ml, F("%s does not exist; run the gate step first", report_path), NULL);
    AvpStr base_bytes;
    if (!ml_show_file(ml, ml_onto_ref(ml), ML_BASELINE, &base_bytes))
        ml_fail(ml, F("%s (%s) has no %s", ml->onto, ml_short(a, ml_onto_ref(ml)), ML_BASELINE), NULL);
    const char *base_path = avp_path_join(a, ml->slot, ML_BASELINE_COPY);
    avp_makedirs(avp_cz(a, avp_path_dirname(base_path)), true);
    if (!avp_write_file(base_path, base_bytes.p, base_bytes.n)) shsm_raise_io();
    EV("baseline: %s:%s (%d bytes) -> %s", ml->onto, ML_BASELINE, base_bytes.n, ML_BASELINE_COPY);
    const char *cj_path = avp_path_join(a, ml->slot, ML_CHANGES_JSON);
    if (avp_exists(cj_path) && !avp_remove(cj_path)) shsm_raise_io();
    AvpStr text;
    i64 rc = ml_run_cmd(ml, "ninja_changes", "changes-ninja.log", NULL, NULL, &text);
    if (rc != 0) {
        EV("ninja changes exited %d (log %s)", rc, ml_log_path(ml, "changes-ninja.log"));
        ml_ev_tail(ml, ev, text, 15);
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    AvpJson *cj;
    const char *err;
    if (!load_json_file(ml, cj_path, &cj, &err)) {
        EV("cannot read %s: %s", cj_path, err);
        ml_fail(ml, ml_join_ev(ml, ev), F("cannot read %s", ML_CHANGES_JSON));
    }
    /* objdiff writes the two reports' totals ("from", "to") whenever it compared them, and a unit entry only for a
     * unit that differs: no totals means the comparison did not run; totals without units mean nothing changed. */
    const AvpJson *cfrom = avp_json_is_obj(cj) ? avp_json_getz(cj, "from") : NULL;
    const AvpJson *cto = avp_json_is_obj(cj) ? avp_json_getz(cj, "to") : NULL;
    const AvpJson *cunits = avp_json_is_obj(cj) ? avp_json_getz(cj, "units") : NULL;
    if (!avp_json_is_obj(cj) || !cfrom || !avp_json_is_obj(cfrom) || !cto || !avp_json_is_obj(cto) ||
        (cunits && !avp_json_is_arr(cunits))) {
        EV("%s holds no from/to comparison; the comparison did not run", ML_CHANGES_JSON);
        ml_fail(ml, ml_join_ev(ml, ev), F("%s holds no comparison", ML_CHANGES_JSON));
    }
    size_t nunits = cunits ? avp_json_len(cunits) : 0;
    GateChangeRows rows = gate_parse_changes_text(a, text);
    AvpSet *unit_names = avp_set_new();
    for (size_t i = 0; i < nunits; i++) {
        const AvpJson *nm = avp_json_getz(avp_json_at(cunits, i), "name");
        avp_set_add(unit_names, nm && avp_json_is_str(nm) ? nm->u.str : AVP_S("\x01None"));
    }
    size_t nfn = 0, nregr = 0;
    for (size_t i = 0; i < rows.n; i++) {
        if (gate_is_unit_row(&rows.v[i], unit_names)) continue;
        nfn++;
        if (rows.v[i].to < rows.v[i].from) nregr++;
    }
    i64 json_regr = gate_json_function_regressions(cj);
    EV("ninja changes printed %d row(s): %d function regression(s), %d function improvement(s), %d unit or total "
       "row(s) (log %s); the decision below reads %s",
       rows.n, nregr, nfn - nregr, rows.n - nfn, ml_log_path(ml, "changes-ninja.log"), ML_CHANGES_JSON);
    if ((i64)nregr != json_regr) {
        EV("ninja changes printed %d function regression row(s) but %s has %d: the printed table and the JSON this step "
           "reads disagree",
           nregr, ML_CHANGES_JSON, json_regr);
        ml_fail(ml, ml_join_ev(ml, ev), F("ninja changes output and %s disagree", ML_CHANGES_JSON));
    }
    if (!nunits) {
        EV("%s compares the reports and lists no unit: no function changed against the %s baseline", ML_CHANGES_JSON,
           ml->onto);
        ml_write_text(ml_log_path(ml, "review-sample.txt"), AVP_S(""));
        EV("review sample (reader test, procedure step 5): 0 new or improved functions; full list %s",
           ml_log_path(ml, "review-sample.txt"));
        return "no function changed";
    }
    GateScores scores = gate_func_scores(a, cj);
    AvpJson *lane_report;
    if (!load_json_file(ml, report_path, &lane_report, &err)) avp_throw(AVP_ERR_FORMAT, "%s", err);
    AvpJsonError je;
    AvpJson *onto_report = avp_json_parse(a, ml_decode(a, base_bytes), &je);
    if (!onto_report) avp_throw(AVP_ERR_FORMAT, "%s", avp_cz(a, avp_json_error_str(a, &je)));
    GateReportIndex lane_ix = gate_report_index(a, lane_report), onto_ix = gate_report_index(a, onto_report);

    MlGit d = ML_GIT(ml, "diff", "--name-only", ml_onto_ref(ml), "HEAD");
    if (d.rc != 0)
        ml_fail(ml, F("git diff --name-only %s HEAD failed (exit %d): %S", ml_short(a, ml_onto_ref(ml)), d.rc,
                      ml_sp(ml->a, avp_str_strip(d.err))), NULL);
    bool headers_changed = false;
    AvpStrVec dl = avp_str_splitlines(d.out, false);
    for (size_t i = 0; i < dl.n; i++)
        if (avp_str_strip(dl.v[i]).n &&
            (avp_str_startswith(dl.v[i], AVP_S("include/")) || avp_str_eqz(dl.v[i], "configure.py")))
            headers_changed = true;

    GateSourceSide lane_side = {read_lane, ml, &ml->ix, NULL}, onto_side = {read_onto, ml, &ml->ix, NULL};
    GateUnitIndex any = gate_all_unit_index(a, lane_report);
    GateScores merged = gate_merge_by_symbol(a, &scores);
    EntryVec decreases = {0}, progress = {0}, gone = {0};
    for (size_t i = 0; i < merged.n; i++) {
        Entry *e = AVP_NEW(a, Entry);
        e->s = merged.v[i];
        if (e->s.has_from) {
            const GateHit *hit = e->s.has_addr_int ? gate_unit_index_addr(&any, e->s.addr_int)
                                                   : gate_unit_index_name(a, &any, e->s.unit, e->s.onto_name);
            if (hit && (!e->s.has_to || hit->score > e->s.to)) {
                e->s.unit_lane = hit->unit, e->s.name = hit->name;
                e->s.has_to = true, e->s.to = hit->score;
            }
            if (!e->s.has_to) {
                if (e->s.from > ML_EPS) avp_vec_push(&gone, e);
                continue;
            }
        } else if (e->s.has_to) {
            if (e->s.to > ML_EPS) avp_vec_push(&progress, e);
            continue;
        } else {
            continue;
        }
        if (e->s.to < e->s.from - ML_EPS) avp_vec_push(&decreases, e);
        else if (e->s.to > e->s.from + ML_EPS) avp_vec_push(&progress, e);
    }
    /* A function the lane's resplit moved out of its unit's range is linked from retail asm again, and the baseline
     * still lists it under the old unit with the stub's score: not a regression (gate_resplit_left). */
    EntryVec left = {0};
    if (decreases.n || gone.n) {
        AvpStr lane_splits, lane_symbols, base_splits;
        GateResplit *rs = NULL;
        if (ml_read_text(a, avp_path_join(a, ml->slot, ML_SPLITS), &lane_splits) &&
            ml_read_text(a, avp_path_join(a, ml->slot, ML_SYMBOLS), &lane_symbols) &&
            ml_show_file(ml, ml_onto_ref(ml), ML_SPLITS, &base_splits)) {
            base_splits = ml_decode(a, base_splits);
            rs = gate_resplit_new(a, &lane_splits, &base_splits, &lane_symbols);
        }
        for (int pass = 0; rs && pass < 2; pass++) {
            EntryVec *list = pass ? &gone : &decreases, keep = {0};
            for (size_t i = 0; i < list->n; i++) {
                Entry *e = list->v[i];
                GateLeftUnit l;
                if (gate_resplit_left(a, rs, e->s.unit, e->s.onto_name ? e->s.onto_name : e->s.name, e->s.has_addr_int,
                                      e->s.addr_int, e->s.from, &l)) {
                    e->label = gate_left_unit_line(a, e->s.onto_name ? e->s.onto_name : e->s.name, e->s.from,
                                                   e->s.has_to, e->s.to, e->s.unit, &l);
                    avp_vec_push(&left, e);
                } else {
                    avp_vec_push(&keep, e);
                }
            }
            *list = keep;
        }
    }
    EntryVec edited = {0}, unchanged = {0}, placed = {0};
    for (int pass = 0; pass < 2; pass++) {
        EntryVec *list = pass ? &gone : &decreases;
        for (size_t i = 0; i < list->n; i++) {
            Entry *e = list->v[i];
            const char *lu = e->s.unit_lane ? e->s.unit_lane : e->s.unit;
            AvpVal *lh = e->s.name ? avp_map_get(lane_ix.by_name, avp_str(gate_tuple_key(a, lu, e->s.name))) : NULL;
            AvpVal *oh = e->s.onto_name ? avp_map_get(onto_ix.by_name, avp_str(gate_tuple_key(a, e->s.unit, e->s.onto_name)))
                                        : NULL;
            e->path_lane = lh ? ((GateReportEntry *)lh->p)->source_path : "";
            e->path_onto = oh ? ((GateReportEntry *)oh->p)->source_path : "";
            if (!e->path_lane) e->path_lane = "";
            if (!e->path_onto) e->path_onto = "";
            GateDecrease dec = {e->s.unit, e->s.name, e->s.onto_name, e->s.has_addr_int, e->s.addr_int,
                                e->path_lane, e->path_onto};
            if (gate_placeholder_removed(a, &dec, &lane_side, &onto_side, exists_lane, ml)) {
                e->label = "placeholder removed";
                avp_vec_push(&placed, e);
                continue;
            }
            if (pass == 0) {
                e->edited = gate_classify_decrease(a, &dec, &lane_side, &onto_side, exists_lane, ml, headers_changed,
                                                   &e->label);
                avp_vec_push(e->edited ? &edited : &unchanged, e);
            } else {
                e->edited = true;
                e->label = F("function is in the %s baseline but not in the lane's report", ml->onto);
                avp_vec_push(&edited, e);
            }
        }
    }
    EV("decreased functions: %d edited by the lane, %d unchanged, %d left their unit, %d placeholder(s) removed", edited.n,
       unchanged.n, left.n, placed.n);
    if (placed.n) {
        EV("%s", GATE_PLACEHOLDER_HEADER);
        for (size_t i = 0; i < placed.n; i++) EV("    %s", fmt_entry(ml, placed.v[i]));
    }
    if (left.n) {
        EV("%s", GATE_LEFT_UNIT_HEADER);
        for (size_t i = 0; i < left.n; i++) EV("%s", left.v[i]->label);
    }
    if (unchanged.n) {
        EV("informational (unchanged functions, not blocking):");
        for (size_t i = 0; i < unchanged.n && i < 25; i++) EV("    %s", fmt_entry(ml, unchanged.v[i]));
        if (unchanged.n > 25) EV("    ... %d more (see %s)", unchanged.n - 25, ml_log_path(ml, "changes-unchanged.txt"));
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < unchanged.n; i++) {
            if (i) avp_buf_putc(&b, '\n');
            avp_buf_putz(&b, fmt_entry(ml, unchanged.v[i]));
        }
        avp_buf_putc(&b, '\n');
        ml_write_text(ml_log_path(ml, "changes-unchanged.txt"), avp_buf_str(&b));
    }
    AvpStrVec unit_drops = {0};
    for (size_t i = 0; i < avp_json_len(cunits); i++) {
        const AvpJson *u = avp_json_at(cunits, i);
        static const char *const KS[] = {"fuzzy_match_percent", "matched_code_percent"};
        const AvpJson *fr = avp_json_getz(u, "from"), *to = avp_json_getz(u, "to"), *md = avp_json_getz(u, "metadata");
        for (int k = 0; k < 2; k++) {
            const AvpJson *va = fr && avp_json_is_obj(fr) ? avp_json_getz(fr, KS[k]) : NULL;
            const AvpJson *vb = to && avp_json_is_obj(to) ? avp_json_getz(to, KS[k]) : NULL;
            if (!va || avp_json_is_null(va)) continue;
            double fa = avp_json_f64(va, 0.0), fb = vb && avp_json_truthy(vb) ? avp_json_f64(vb, 0.0) : 0.0;
            const AvpJson *ag = md && avp_json_is_obj(md) ? avp_json_getz(md, "auto_generated") : NULL;
            if (fb < fa - ML_EPS && !(ag && avp_json_truthy(ag)))
                avp_vec_push(&unit_drops, avp_str(F("%s %s %.2f%% -> %.2f%%", num_or_none(ml, avp_json_getz(u, "name")),
                                                    KS[k], fa, fb)));
        }
    }
    if (unit_drops.n) {
        EV("unit-level decreases (informational): %d", unit_drops.n);
        for (size_t i = 0; i < unit_drops.n && i < 10; i++) EV("    %S", &unit_drops.v[i]);
    }
    avp_sort(progress.v, progress.n, sizeof(Entry *), sample_cmp, NULL, false);
    AvpBuf rs;
    avp_buf_init(&rs, a);
    size_t full = 0;
    for (size_t i = 0; i < progress.n; i++) {
        const Entry *e = progress.v[i];
        if (e->s.to >= 99.995) full++;
        avp_buf_putz(&rs, F("%s -> %s  %s  %s\n", e->s.has_from ? F("%.2f%%", e->s.from) : "new", F("%.2f%%", e->s.to),
                            e->s.unit, e->s.name));
    }
    for (size_t i = 0; i < edited.n; i++) avp_buf_putz(&rs, F("REGRESSED %s\n", fmt_entry(ml, edited.v[i])));
    ml_write_text(ml_log_path(ml, "review-sample.txt"), avp_buf_str(&rs));
    EV("review sample (reader test, procedure step 5): %d new or improved functions (%d at 100%%); full list %s",
       progress.n, full, ml_log_path(ml, "review-sample.txt"));
    for (size_t i = 0; i < progress.n && i < 12; i++) {
        const Entry *e = progress.v[i];
        AvpStr cut = avp_u8_slice(avp_str(e->s.name ? e->s.name : "?"), 0, 44);
        EV("    %-44S %s -> %.2f%%  %s", &cut, e->s.has_from ? F("%.2f%%", e->s.from) : "new", e->s.to, e->s.unit);
    }
    if (progress.n > 12) EV("    ... %d more", progress.n - 12);
    if (edited.n) {
        EV("BLOCKING: edited function(s) whose score fell (fix in the lane or have the user adjudicate):");
        AvpStrVec names = {0};
        for (size_t i = 0; i < edited.n; i++) {
            EV("    %s", fmt_entry(ml, edited.v[i]));
            const Entry *e = edited.v[i];
            if (i < 4) avp_vec_push(&names, avp_str(e->s.name ? e->s.name : (e->s.onto_name ? e->s.onto_name : "?")));
        }
        const char *joined = avp_cz(a, avp_str_join(a, AVP_S(", "), names.v, names.n));
        const char *summary = F("%d edited function(s) fell: %s", edited.n, joined);
        ml_fail(ml, ml_join_ev(ml, ev), summary);
    }
    return F("%d improved, %d dips in unchanged functions, %d left their unit, %d placeholder(s) removed, no edited function fell",
             progress.n, unchanged.n, left.n, placed.n);
}
