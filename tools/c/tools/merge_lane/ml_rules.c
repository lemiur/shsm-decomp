/* ml_rules: merge_lane.py's step_rules (parse_json_text, check_rules_doc, finding_key, extract_git_tree). */
#include "merge_lane.h"
#include "shsm_archive.h"
#include "shsm_cli.h"
#include <string.h>

#define F(...) ML_F(ml, __VA_ARGS__)
#define EV(...) ml_ev(ml, ev, F(__VA_ARGS__))

AvpJson *ml_parse_json_text(AvpArena *a, AvpStr text, const char **err) {
    i64 i = avp_str_findc(text, '{');
    if (i < 0) {
        *err = "no JSON in output";
        return NULL;
    }
    AvpJsonError e;
    AvpJson *doc = avp_json_parse(a, avp_str_slice(text, i, AVP_END), &e);
    if (!doc) *err = avp_cz(a, avp_json_error_str(a, &e));
    return doc;
}

static bool is_list(const AvpJson *v) { return v && avp_json_is_arr(v); }

const char *ml_check_rules_doc(AvpArena *a, const AvpJson *doc, i64 rc, const char *what, const char *const *keys,
                               size_t nkeys) {
    if (!doc || !avp_json_is_obj(doc)) return avp_sprintf(a, "%s: the JSON is not an object", what);
    AvpStrVec missing = {0};
    bool want_scanned = false;
    for (size_t i = 0; i < nkeys; i++) {
        if (!is_list(avp_json_getz(doc, keys[i]))) avp_vec_push(&missing, avp_str(keys[i]));
        want_scanned |= !strcmp(keys[i], "scanned");
    }
    if (missing.n)
        return avp_sprintf(a, "%s: the JSON lacks %.*s", what, AVP_SF(avp_str_join(a, AVP_S(", "), missing.v, missing.n)));
    if (rc != 0 && rc != 1) return avp_cz(a, avp_pyfmt(a, "%s exited %d (0 or 1 expected)", what, rc));
    const AvpJson *ex = avp_json_getz(doc, "exit");
    if (ex) {
        bool same;
        if (ex->kind == AVP_JSON_BOOL) same = (ex->b ? 1 : 0) == rc;
        else if (avp_json_is_num(ex)) same = avp_json_f64(ex, 0.0) == (double)rc;
        else same = false;
        if (!same)
            return avp_cz(a, avp_pyfmt(a, "%s exited %d but its JSON records exit %s", what, rc, gate_json_pystr(a, ex)));
    }
    const AvpJson *sc = avp_json_getz(doc, "scanned");
    if ((!sc || !avp_json_truthy(sc)) && want_scanned) return avp_sprintf(a, "%s scanned no file", what);
    const AvpJson *r13 = avp_json_getz(doc, "r13");
    if (r13 && avp_json_is_obj(r13)) {
        const AvpJson *note = avp_json_getz(r13, "note");
        if (note && avp_json_truthy(note)) return avp_sprintf(a, "%s: %s", what, gate_json_pystr(a, note));
    }
    return NULL;
}

static const char *finding_key(AvpArena *a, const AvpJson *f) {
    static const char *const K[] = {"path", "rule", "function", "message"};
    AvpBuf b;
    avp_buf_init(&b, a);
    for (int i = 0; i < 4; i++) {
        const AvpJson *v = avp_json_getz(f, K[i]);
        avp_buf_put(&b, v ? avp_json_dumps(a, v, &AVP_JSON_COMPACT) : AVP_S("<absent>"));
        avp_buf_putc(&b, '\x1f');
    }
    return avp_buf_cz(&b);
}

static const char *get_s(Ml *ml, const AvpJson *o, const char *k) { return gate_json_pystr(ml->a, avp_json_getz(o, k)); }

static const char *finding_line(Ml *ml, const AvpJson *f, bool with_fn) {
    const AvpJson *fn = avp_json_getz(f, "function");
    const char *in = with_fn && fn && avp_json_truthy(fn) ? F("  (in %s)", gate_json_pystr(ml->a, fn)) : "";
    return F("    %s:%s: %s [%s] %s%s", get_s(ml, f, "path"), get_s(ml, f, "line"), get_s(ml, f, "rule"),
             get_s(ml, f, "tier"), get_s(ml, f, "message"), in);
}

/* extract_git_tree: src/, include/, docs/exceptions.md and configure.py of rev, into dest. */
static void extract_git_tree(Ml *ml, const char *rev, const char *dest) {
    AvpArena *a = ml->a;
    const char *av[] = {"git", "-C", ml->slot, "archive", "--format=tar", rev, NULL};
    AvpProcResult p = avp_run(&(AvpProcSpec){.argv = av, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = a});
    if (p.status == AVP_PROC_START_FAILED) avp_throw(AVP_ERR_PROC, "%s", p.message ? p.message : "cannot run git");
    if (p.returncode != 0)
        ml_fail(ml, F("git archive %s failed: %S", rev, ml_sp(ml->a, avp_str_strip(ml_decode(a, p.err)))), NULL);
    ShsmTarReader r;
    shsm_tar_open(&r, p.out.p, p.out.n);
    ShsmTarMember m;
    int k;
    while ((k = shsm_tar_next(&r, a, &m)) == 1) {
        AvpStr n = avp_str(m.name);
        if (m.kind != SHSM_TAR_FILE) continue;
        if (!(avp_str_startswith(n, AVP_S("src/")) || avp_str_startswith(n, AVP_S("include/")) ||
              avp_str_eqz(n, "docs/exceptions.md") || avp_str_eqz(n, "configure.py")))
            continue;
        const char *target = avp_path_join(a, dest, m.name);
        avp_makedirs(avp_cz(a, avp_path_dirname(target)), true);
        if (!avp_write_file(target, m.data, (size_t)m.size)) shsm_raise_io();
    }
    if (k < 0) avp_throw(AVP_ERR_FORMAT, "ReadError: %s", r.err ? r.err : "bad tar");
}

typedef struct {
    Ml *ml;
    AvpStrVec *ev;
    AvpMap *cache; /* path -> AvpSet * of changed names, NULL value: new file */
} Changed;

/* changed_functions(path): the names whose source the lane changed; *is_new for a file new in the lane. */
static AvpSet *changed_functions(Changed *c, AvpStr path, bool *is_new) {
    Ml *ml = c->ml;
    AvpArena *a = ml->a;
    AvpStrVec *ev = c->ev;
    AvpVal *v = avp_map_get(c->cache, path);
    if (v) {
        *is_new = v->p == NULL;
        return v->p;
    }
    const char *p = avp_cz(a, path);
    AvpStr lt;
    AvpBytes lb = {0};
    if (!avp_read_all(a, avp_path_join(a, ml->slot, p), &lb)) {
        ml_ev(ml, ev, F("cannot read %s in the lane to decide which functions it touched: %S", p,
                        ml_sp(ml->a, avp_io_err_str(a))));
        ml_fail(ml, ml_join_ev(ml, ev), F("cannot read %s", p));
    }
    avp_decode_text(a, (AvpStr){(const char *)lb.v, lb.n}, AVP_TM_UTF8_REPLACE, &lt);
    AvpStr ob;
    if (!ml_show_file(ml, ml_onto_ref(ml), p, &ob)) {
        MlGit e = ML_GIT(ml, "cat-file", "-e", avp_sprintf(a, "%s:%s", ml_onto_ref(ml), p));
        if (e.rc == 0) {
            ml_ev(ml, ev, F("cannot read %s at %s", p, ml_short(a, ml_onto_ref(ml))));
            ml_fail(ml, ml_join_ev(ml, ev), F("cannot read %s at %s", p, ml->onto));
        }
        avp_map_put(c->cache, path, NULL)->p = NULL;
        *is_new = true;
        return NULL;
    }
    AvpJson *li = NULL, *oi = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        li = ml_index_source(ml, a, p, lt);
        oi = ml_index_source(ml, a, p, ml_decode(a, ob));
        avp_try_end(&t);
    } else {
        ml_ev(ml, ev, F("cannot index the functions of %s: %s: %s", p, shsm_exc_class(t.code), t.msg));
        ml_fail(ml, ml_join_ev(ml, ev), F("cannot index %s", p));
    }
    AvpSet *oh = avp_set_new(); /* name \x1f hash */
    const AvpJson *of = avp_json_getz(oi, "funcs");
    for (size_t i = 0; of && i < avp_json_len(of); i++) {
        const AvpJson *f = avp_json_at(of, i);
        avp_set_add(oh, avp_str(F("%s\x1f%s", get_s(ml, f, "name"), get_s(ml, f, "hash"))));
    }
    AvpSet *names = avp_set_new();
    const AvpJson *lf = avp_json_getz(li, "funcs");
    for (size_t i = 0; lf && i < avp_json_len(lf); i++) {
        const AvpJson *f = avp_json_at(lf, i);
        if (!avp_set_has(oh, avp_str(F("%s\x1f%s", get_s(ml, f, "name"), get_s(ml, f, "hash")))))
            avp_set_add(names, avp_str(get_s(ml, f, "name")));
    }
    avp_map_put(c->cache, path, NULL)->p = names;
    *is_new = false;
    return names;
}

typedef struct {
    AvpStr path, name;
} Pend;

static int pend_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Pend *p = x, *q = y;
    int c = avp_str_cmp(p->path, q->path);
    return c ? c : avp_str_cmp(p->name, q->name);
}

const char *ml_step_rules(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    ml_require_no_unmerged(ml);
    ml_tools_guard(ml, ev);
    AvpStr text;
    i64 rc = ml_run_cmd(ml, "classes_scan", "rules-classes.log", NULL, NULL, &text);
    EV("classes.exe scan: exit %d (log %s)", rc, ml_log_path(ml, "rules-classes.log"));
    if (rc != 0) {
        ml_ev_tail(ml, ev, text, 15);
        ml_fail(ml, ml_join_ev(ml, ev), F("classes.exe scan exited %d", rc));
    }
    rc = ml_run_cmd(ml, "rules_changed", "rules-changed.json", NULL, NULL, &text);
    const char *err = NULL;
    AvpJson *doc = ml_parse_json_text(a, text, &err);
    if (!doc) {
        EV("check_rules --changed %s: no JSON (exit %d): %s", ml->onto, rc, err);
        ml_ev_tail(ml, ev, text, 15);
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    static const char *const KEYS_CHANGED[] = {"findings", "blocking"};
    const char *bad_doc = ml_check_rules_doc(a, doc, rc, F("check_rules --changed %s", ml->onto), KEYS_CHANGED, 2);
    if (bad_doc) {
        EV("%s (log %s)", bad_doc, ml_log_path(ml, "rules-changed.json"));
        ml_fail(ml, ml_join_ev(ml, ev), bad_doc);
    }
    const AvpJson *blocking = avp_json_getz(doc, "blocking");
    const AvpJson *scanned = avp_json_getz(doc, "scanned"), *findings = avp_json_getz(doc, "findings");
    size_t nb = avp_json_len(blocking);
    EV("check_rules --changed %s: exit %d, %d file(s) scanned, %d violation(s), %d blocking", ml->onto, rc,
       scanned ? avp_json_len(scanned) : 0, avp_json_len(findings), nb);
    const AvpJson *fns = avp_json_getz(doc, "functions");
    AVP_VEC(Pend) pend = {0};
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; fns && i < avp_json_len(fns); i++) {
        const AvpJson *fn = avp_json_at(fns, i);
        const AvpJson *st = avp_json_getz(fn, "status");
        if (!st || !avp_json_is_str(st) || !avp_str_eqz(st->u.str, "MATCH-PENDING")) continue;
        Pend pn = {avp_str(get_s(ml, fn, "path")), avp_str(get_s(ml, fn, "name"))};
        const char *k = F("%S\x1f%S", &pn.path, &pn.name);
        if (avp_set_has(seen, avp_str(k))) continue;
        avp_set_add(seen, avp_str(k));
        avp_vec_push(&pend, pn);
    }
    avp_sort(pend.v, pend.n, sizeof(Pend), pend_cmp, NULL, false);
    if (pend.n) {
        EV("MATCH-PENDING functions in the scanned files: %d (a lane must not report these as matched)", pend.n);
        for (size_t i = 0; i < pend.n && i < 15; i++) EV("    %S::%S", &pend.v[i].path, &pend.v[i].name);
    }
    if (rc != 0 || nb) {
        EV("BLOCKING [match] findings in Matching units:");
        for (size_t i = 0; i < nb && i < 30; i++) ml_ev(ml, ev, finding_line(ml, avp_json_at(blocking, i), true));
        if (nb > 30) EV("    ... %d more (log %s)", nb - 30, ml_log_path(ml, "rules-changed.json"));
        ml_fail(ml, ml_join_ev(ml, ev), F("%d blocking [match] finding(s)", nb));
    }
    /* whole tree: onto vs lane, both judged against the slot's classes.json */
    const char *classes = avp_path_join(a, ml->slot, ML_CLASSES_JSON);
    const char *tmp = avp_mkdtemp(a, "merge_lane_onto_", "", NULL);
    AvpStr text_o = {0}, text_l = {0};
    i64 rc_o = 0, rc_l = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        extract_git_tree(ml, ml_onto_ref(ml), tmp);
        rc_o = ml_run_cmd(ml, "rules_tree", "rules-tree-onto.json", &(MlExtra){tmp, classes}, NULL, &text_o);
        rc_l = ml_run_cmd(ml, "rules_tree", "rules-tree-lane.json", &(MlExtra){ml->slot, classes}, NULL, &text_l);
        avp_try_end(&t);
    } else {
        avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
        avp_throw(t.code, "%s", t.msg); /* finally: rmtree, then re-raise */
    }
    avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
    AvpJson *onto_doc = ml_parse_json_text(a, text_o, &err);
    AvpJson *lane_doc = onto_doc ? ml_parse_json_text(a, text_l, &err) : NULL;
    if (!onto_doc || !lane_doc) {
        EV("whole-tree rule check produced no JSON: %s", err);
        ml_ev_tail(ml, ev, text_l.n ? text_l : text_o, 15);
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    static const char *const KEYS_TREE[] = {"findings", "scanned"};
    const AvpJson *docs[2] = {onto_doc, lane_doc};
    i64 rcs[2] = {rc_o, rc_l};
    const char *sides[2] = {ml->onto, "lane"}, *logs[2] = {"rules-tree-onto.json", "rules-tree-lane.json"};
    for (int i = 0; i < 2; i++) {
        bad_doc = ml_check_rules_doc(a, docs[i], rcs[i], F("whole-tree check_rules (%s)", sides[i]), KEYS_TREE, 2);
        if (bad_doc) {
            EV("%s (log %s)", bad_doc, ml_log_path(ml, logs[i]));
            ml_fail(ml, ml_join_ev(ml, ev), bad_doc);
        }
    }
    const AvpJson *of = avp_json_getz(onto_doc, "findings"), *lf = avp_json_getz(lane_doc, "findings");
    AvpMap *remaining = avp_map_new();
    for (size_t i = 0; i < avp_json_len(of); i++) avp_map_put(remaining, avp_str(finding_key(a, avp_json_at(of, i))), NULL)->i++;
    AVP_VEC(const AvpJson *) news = {0};
    for (size_t i = 0; i < avp_json_len(lf); i++) {
        const AvpJson *f = avp_json_at(lf, i);
        AvpVal *v = avp_map_get(remaining, avp_str(finding_key(a, f)));
        if (v && v->i > 0) v->i--;
        else avp_vec_push(&news, f);
    }
    AvpSet *touched = avp_set_new();
    for (size_t i = 0; i < ml->lane_files.n; i++) avp_set_add(touched, ml->lane_files.v[i]);
    MlGit d = ML_GIT(ml, "diff", "--name-only", ml_onto_ref(ml), "HEAD");
    if (d.rc != 0) {
        EV("git diff --name-only %s HEAD failed (exit %d): %S", ml_short(a, ml_onto_ref(ml)), d.rc,
           ml_sp(ml->a, avp_str_strip(d.err)));
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    AvpStrVec dl = avp_str_splitlines(d.out, false);
    for (size_t i = 0; i < dl.n; i++)
        if (avp_str_strip(dl.v[i]).n) avp_set_add(touched, dl.v[i]);
    Changed ch = {ml, ev, avp_map_new()};
    AVP_VEC(const AvpJson *) in_touched = {0}, untouched = {0};
    for (size_t i = 0; i < news.n; i++) {
        const AvpJson *f = news.v[i];
        const AvpJson *pj = avp_json_getz(f, "path"), *fj = avp_json_getz(f, "function");
        AvpStr p = pj && avp_json_is_str(pj) ? pj->u.str : AVP_S("\x01None");
        if (!avp_set_has(touched, p)) {
            avp_vec_push(&untouched, f);
        } else if (fj && avp_json_truthy(fj) && avp_str_startswith(p, AVP_S("src/"))) {
            bool is_new;
            AvpSet *names = changed_functions(&ch, p, &is_new);
            if (!is_new && !avp_set_has(names, avp_str(gate_json_pystr(a, fj)))) avp_vec_push(&untouched, f);
            else avp_vec_push(&in_touched, f);
        } else {
            avp_vec_push(&in_touched, f);
        }
    }
    EV("whole tree %s vs lane: %d violation(s) vs %d; %d new (%d in functions or files the lane touched, %d in functions "
       "it did not)",
       ml->onto, avp_json_len(of), avp_json_len(lf), news.n, in_touched.n, untouched.n);
    AVP_VEC(const AvpJson *) bad = {0}, other = {0};
    for (size_t i = 0; i < untouched.n; i++) {
        const AvpJson *tier = avp_json_getz(untouched.v[i], "tier");
        if (tier && avp_json_is_str(tier) && avp_str_eqz(tier->u.str, "match")) avp_vec_push(&bad, untouched.v[i]);
        else avp_vec_push(&other, untouched.v[i]);
    }
    if (other.n) {
        EV("new non-[match] findings in untouched functions (informational):");
        for (size_t i = 0; i < other.n && i < 10; i++) ml_ev(ml, ev, finding_line(ml, other.v[i], false));
    }
    if (bad.n) {
        EV("BLOCKING: new [match] findings in functions the lane did not touch (shared-header breakage):");
        for (size_t i = 0; i < bad.n && i < 30; i++) ml_ev(ml, ev, finding_line(ml, bad.v[i], true));
        ml_fail(ml, ml_join_ev(ml, ev), F("%d new [match] finding(s) in untouched functions", bad.n));
    }
    return news.n ? F("no blocking findings; %d new finding(s) all in touched code", news.n)
                  : "no blocking findings; no new findings";
}
