/* ml_stubs: see ml_stubs.h. Mirrors tools/test_merge_lane.py (Stubs, setUp, report, changes, changes_text). */
#include "ml_stubs.h"
#include "changes_fmt.h"
#include <stdlib.h>
#include <string.h>

const char *const MLFX_A_SRC = "// banner\n"
                               "int fn_A(int x) {\n"
                               "    return x + 1;\n"
                               "}  // 0x80000100\n"
                               "\n"
                               "int fn_B(int x) {\n"
                               "    return x * 2;\n"
                               "}  // 0x80000200\n";
const char *const MLFX_CONFIGURE = "libs = [\n"
                                   "    Object(Matching, \"a.cpp\"),\n"
                                   "    Object(NonMatching, \"b.cpp\"),\n"
                                   "]\n";

const char *mlfx_git(AvpArena *a, const char *cwd, const char *const *args) {
    size_t n = 0;
    while (args[n]) n++;
    const char **av = AVP_NEWN(a, const char *, n + 4);
    av[0] = "git", av[1] = "-C", av[2] = cwd;
    for (size_t i = 0; i < n; i++) av[3 + i] = args[i];
    av[n + 3] = NULL;
    AvpProcResult p = avp_run(&(AvpProcSpec){.argv = av, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = a});
    if (p.status != AVP_PROC_OK || p.returncode != 0) {
        AvpStrVec parts = {0};
        for (size_t i = 0; i < n; i++) avp_vec_push(&parts, avp_str(args[i]));
        avp_throw(AVP_ERR_PROC, "git %.*s: %.*s", AVP_SF(avp_str_join(a, AVP_S(" "), parts.v, parts.n)),
                  AVP_SF(ml_decode(a, p.err)));
    }
    return avp_cz(a, avp_str_strip(ml_decode(a, p.out)));
}

void mlfx_write(const char *root, const char *rel, AvpStr text) {
    AvpArena *a = avp_scratch();
    const char *p = avp_path_join(a, root, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(p)), true);
    if (!avp_write_file(p, text.p, text.n)) avp_throw(AVP_ERR_IO, "cannot write %s", p);
}

AvpStr mlfx_dumps(AvpArena *a, const AvpJson *v) { return avp_json_dumps(a, v, &(AvpJsonDumpOpts){0}); }

AvpJson *mlfx_report(AvpArena *a, const AvpJson *score_a, const AvpJson *score_b, const char *src) {
    AvpJson *funcs = avp_json_arr(a);
    const char *names[2] = {"fn_A", "fn_B"};
    const AvpJson *scores[2] = {score_a, score_b};
    i64 addrs[2] = {MLFX_A_ADDR, MLFX_B_ADDR};
    for (int i = 0; i < 2; i++) {
        AvpJson *f = avp_json_obj(a);
        avp_json_set(a, f, AVP_S("name"), avp_json_strz(a, names[i]));
        avp_json_set(a, f, AVP_S("size"), avp_json_strz(a, "8"));
        avp_json_set(a, f, AVP_S("fuzzy_match_percent"), (AvpJson *)scores[i]);
        AvpJson *md = avp_json_obj(a);
        avp_json_set(a, md, AVP_S("virtual_address"), avp_json_strz(a, avp_sprintf(a, "%lld", (long long)addrs[i])));
        avp_json_set(a, f, AVP_S("metadata"), md);
        avp_json_arr_push(a, funcs, f);
    }
    AvpJson *u = avp_json_obj(a), *md = avp_json_obj(a), *units = avp_json_arr(a), *doc = avp_json_obj(a);
    avp_json_set(a, u, AVP_S("name"), avp_json_strz(a, "main/a"));
    avp_json_set(a, u, AVP_S("functions"), funcs);
    avp_json_set(a, md, AVP_S("source_path"), avp_json_strz(a, src ? src : "src/a.cpp"));
    avp_json_set(a, u, AVP_S("metadata"), md);
    avp_json_arr_push(a, units, u);
    avp_json_set(a, doc, AVP_S("units"), units);
    return doc;
}

AvpJson *mlfx_report_f(AvpArena *a, double score_a, double score_b) {
    return mlfx_report(a, avp_json_float(a, score_a), avp_json_float(a, score_b), NULL);
}

AvpJson *mlfx_changes(AvpArena *a, const AvpJson *rows) {
    AvpJson *funcs = avp_json_arr(a);
    for (size_t i = 0; rows && i < avp_json_len(rows); i++) {
        const AvpJson *r = avp_json_at(rows, i);
        AvpJson *e = avp_json_obj(a), *md = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("name"), avp_json_at(r, 0));
        const AvpJson *addr = avp_json_at(r, 1);
        avp_json_set(a, md, AVP_S("virtual_address"),
                     avp_json_strz(a, avp_cz(a, avp_pyfmt(a, "%s", gate_json_pystr(a, addr)))));
        avp_json_set(a, e, AVP_S("metadata"), md);
        for (int k = 2; k < 4; k++) {
            const AvpJson *v = avp_json_at(r, k);
            if (!v || avp_json_is_null(v)) continue;
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("fuzzy_match_percent"), (AvpJson *)v);
            avp_json_set(a, e, k == 2 ? AVP_S("from") : AVP_S("to"), o);
        }
        avp_json_arr_push(a, funcs, e);
    }
    AvpJson *doc = avp_json_obj(a), *u = avp_json_obj(a), *units = avp_json_arr(a);
    avp_json_set(a, doc, AVP_S("from"), avp_json_obj(a));
    avp_json_set(a, doc, AVP_S("to"), avp_json_obj(a));
    avp_json_set(a, u, AVP_S("name"), avp_json_strz(a, "main/a"));
    avp_json_set(a, u, AVP_S("functions"), funcs);
    avp_json_arr_push(a, units, u);
    avp_json_set(a, doc, AVP_S("units"), units);
    return doc;
}

AvpStr mlfx_changes_text(AvpArena *a, const AvpJson *doc, bool progress) {
    CfChanges regr = {0}, prog = {0};
    cf_get_changes(doc, &regr, &prog);
    CfChanges rows = {0};
    if (progress)
        for (size_t i = 0; i < prog.n; i++) avp_vec_push(&rows, prog.v[i]);
    for (size_t i = 0; i < regr.n; i++) avp_vec_push(&rows, regr.v[i]);
    AvpStr table = cf_plaintext(a, rows.v, rows.n);
    return avp_str(avp_sprintf(a, "[1/2] REPORT_CHANGES build/R5WEA4/report_changes.json\n[2/2] CHANGES_FMT changes\n%.*s\n",
                               AVP_SF(table)));
}

void mlfx_fixed_dates(void) {
    avp_setenv("GIT_AUTHOR_DATE", "2026-01-01T00:00:00+0000");
    avp_setenv("GIT_COMMITTER_DATE", "2026-01-01T00:00:00+0000");
}

MlFixture mlfx_setup(AvpArena *a, const char *tmp) {
    MlFixture fx = {a, tmp, avp_path_join(a, tmp, "main_checkout"), avp_path_join(a, tmp, "slot"),
                    avp_path_join(a, tmp, "logs")};
    avp_makedirs(fx.main, true);
    MLFX_GIT(a, fx.main, "init", "-q", "-b", "main");
    MLFX_GIT(a, fx.main, "config", "user.email", "t@example.com");
    MLFX_GIT(a, fx.main, "config", "user.name", "tester");
    MLFX_GIT(a, fx.main, "config", "core.autocrlf", "false");
    mlfx_write(fx.main, "src/a.cpp", avp_str(MLFX_A_SRC));
    mlfx_write(fx.main, "src/b.cpp", AVP_S("int fn_C(void) {\n    return 0;\n}  // 0x80000300\n"));
    mlfx_write(fx.main, "include/h.h", AVP_S("#define H 1\n"));
    mlfx_write(fx.main, "configure.py", avp_str(MLFX_CONFIGURE));
    mlfx_write(fx.main, ".gitignore", AVP_S("build/\n"));
    mlfx_write(fx.main, "docs/progress.md", AVP_S("ledger\n"));
    mlfx_write(fx.main, ML_BASELINE, mlfx_dumps(a, mlfx_report_f(a, 100.0, 100.0)));
    MLFX_GIT(a, fx.main, "add", "-A");
    MLFX_GIT(a, fx.main, "commit", "-q", "-m", "base");
    MLFX_GIT(a, fx.main, "worktree", "add", "-q", "--detach", fx.slot, "main");
    return fx;
}

void mlfx_lane_commit(MlFixture *fx, const char *rel, AvpStr text, const char *msg, const char *branch, bool new_branch) {
    AvpArena *a = fx->a;
    if (!msg) msg = "lane work";
    if (!branch) branch = "lane/x";
    if (new_branch) MLFX_GIT(a, fx->slot, "checkout", "-q", "-b", branch, "main");
    else MLFX_GIT(a, fx->slot, "checkout", "-q", branch);
    mlfx_write(fx->slot, rel, text);
    MLFX_GIT(a, fx->slot, "add", "-A");
    MLFX_GIT(a, fx->slot, "commit", "-q", "-m", msg);
    MLFX_GIT(a, fx->slot, "checkout", "-q", "--detach");
}

void mlfx_main_commit(MlFixture *fx, const char *rel, AvpStr text, const char *msg) {
    AvpArena *a = fx->a;
    mlfx_write(fx->main, rel, text);
    MLFX_GIT(a, fx->main, "add", "-A");
    MLFX_GIT(a, fx->main, "commit", "-q", "-m", msg ? msg : "main moves");
}

/* ---- stubs ------------------------------------------------------------------------------------------------------- */
static AvpJson *parse(AvpArena *a, const char *text) {
    AvpJsonError e;
    AvpJson *j = avp_json_parse(a, avp_str(text), &e);
    AVP_ASSERT(j);
    return j;
}

void mlst_init(MlStubs *s, AvpArena *a, MlFixture *fx) {
    *s = (MlStubs){0};
    s->a = a, s->fx = fx;
    s->score_a = avp_json_float(a, 100.0), s->score_b = avp_json_float(a, 100.0);
    s->changes_rows = avp_json_arr(a);
    s->gate_ok = true;
    s->rules_changed = parse(a, "{\"scanned\": [\"src/a.cpp\"], \"findings\": [], \"blocking\": [], \"functions\": [], "
                                "\"exit\": 0}");
    s->rules_tree = parse(a, "{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": []}}");
    s->changes_write = true;
}

bool mlst_called(const MlStubs *s, const char *key) {
    for (size_t i = 0; i < s->calls.n; i++)
        if (avp_str_eqz(s->calls.v[i], key)) return true;
    return false;
}

static void mutate(MlStubs *s, const char *on) {
    for (size_t i = 0; i < s->nmutations; i++) {
        MlMutation *m = &s->mutations[i];
        if (m->done || strcmp(m->on, on)) continue;
        m->done = true;
        if (m->in_slot) mlfx_write(s->fx->slot, m->rel, m->text);
        else mlfx_main_commit(s->fx, m->rel, m->text, NULL);
    }
}

static i64 rc_of(const AvpJson *o, const char *k, i64 dflt) {
    const AvpJson *v = avp_json_getz(o, k);
    return v && avp_json_is_num(v) ? avp_json_i64(v, dflt) : dflt;
}

static bool command(void *hctx, Ml *ml, const char *key, const MlExtra *extra, i64 *rc, AvpStr *text) {
    MlStubs *s = hctx;
    AvpArena *a = s->a;
    if (s->unknown_keys) return false;
    mutate(s, key);
    avp_vec_push(&s->calls, avp_str(avp_strdup(a, key)));
    if (!strcmp(key, "configure")) {
        *rc = 0, *text = AVP_S("configured\n");
    } else if (!strcmp(key, "ninja")) {
        mlfx_write(ml->slot, ML_REPORT, mlfx_dumps(a, mlfx_report(a, s->score_a, s->score_b, NULL)));
        if (s->lane_report) mlfx_write(ml->slot, ML_REPORT, mlfx_dumps(a, s->lane_report));
        *text = avp_str(avp_sprintf(a, "[1/9] LINK\nbuild/R5WEA4/main.dol: %s\n", s->gate_ok ? "OK" : "FAILED"));
        *rc = s->gate_rc;
    } else if (!strcmp(key, "attribute")) {
        *rc = 1, *text = AVP_S("unit main/a: symbol fn_A differs at 0x80000100\n");
    } else if (!strcmp(key, "ninja_changes")) {
        AvpJson *doc = s->changes_doc ? s->changes_doc : mlfx_changes(a, s->changes_rows);
        if (s->changes_write) mlfx_write(ml->slot, ML_CHANGES_JSON, mlfx_dumps(a, doc));
        *rc = 0, *text = s->changes_text ? avp_str(s->changes_text) : mlfx_changes_text(a, doc, false);
    } else if (!strcmp(key, "classes_scan")) {
        if (s->classes_rc == 0) mlfx_write(ml->slot, ML_CLASSES_JSON, AVP_S("{\"classes\": []}"));
        *rc = s->classes_rc, *text = s->classes_rc == 0 ? AVP_S("classes.json written\n") : AVP_S("scan failed\n");
    } else if (!strcmp(key, "rules_changed")) {
        s->seen_onto = avp_strdup(a, ml_onto_ref(ml));
        *rc = rc_of(s->rules_changed, "exit", 0), *text = mlfx_dumps(a, s->rules_changed);
    } else if (!strcmp(key, "rules_tree")) {
        const char *cl = extra->classes ? extra->classes : "";
        avp_vec_push(&s->tree_classes, avp_str(avp_strdup(a, cl)));
        avp_vec_push(&s->tree_classes_exist, avp_isfile(cl));
        bool lane = ml_norm_eq(a, extra->root, ml->slot);
        AvpJson *doc = avp_json_obj(a);
        AvpJson *sc = avp_json_arr(a);
        avp_json_arr_push(a, sc, avp_json_strz(a, "src/a.cpp"));
        avp_json_arr_push(a, sc, avp_json_strz(a, "src/b.cpp"));
        avp_json_set(a, doc, AVP_S("scanned"), sc);
        avp_json_set(a, doc, AVP_S("exit"), avp_json_int(a, s->rules_tree_rc));
        const AvpJson *side = avp_json_getz(s->rules_tree, lane ? "lane" : "onto");
        for (size_t i = 0; side && i < side->u.obj.n; i++)
            avp_json_set(a, doc, side->u.obj.v[i].key, side->u.obj.v[i].val);
        *rc = s->rules_tree_rc, *text = mlfx_dumps(a, doc);
        if (lane && s->remove_after_lane_tree) avp_remove(avp_path_join(a, ml->slot, s->remove_after_lane_tree));
    } else if (!strcmp(key, "layout")) {
        *rc = s->layout_rc, *text = AVP_S("ok    Matching    a.cpp\n\nunits checked: 1; Matching 1 (0 with differences)\n");
    } else if (!strcmp(key, "build_tools")) {
        *rc = 0, *text = AVP_S("tools built\n");
    } else if (!strcmp(key, "ledger_progress")) {
        avp_append_file(avp_path_join(a, ml->slot, ML_PROGRESS), "updated\n", 8);
        *rc = s->ledger_rc, *text = AVP_S("progress written\n");
    } else if (!strcmp(key, "ledger_exceptions")) {
        *rc = 0, *text = AVP_S("exceptions ok\n");
    } else {
        return false;
    }
    return true;
}

static bool git_hook(void *hctx, Ml *ml, const char *const *args, size_t n, const char *cwd, MlGit *res) {
    MlStubs *s = hctx;
    (void)ml, (void)cwd;
    if (!s->fail_git_diff_head || n < 3) return false;
    if (strcmp(args[0], "diff") || strcmp(args[1], "--name-only") || strcmp(args[n - 1], "HEAD")) return false;
    *res = (MlGit){128, AVP_S(""), AVP_S("fatal: simulated")};
    return true;
}

static AvpJson *broken_index(void *hctx, AvpArena *a, const char *path, AvpStr text) {
    (void)hctx, (void)a, (void)path, (void)text;
    avp_throw(AVP_ERR_FORMAT, "tokenizer failed");
}

static void before_step(void *hctx, Ml *ml, const char *step) {
    (void)ml;
    mutate(hctx, avp_sprintf(((MlStubs *)hctx)->a, "step:%s", step));
}

MlHooks mlst_hooks(MlStubs *s, AvpBuf *out) {
    return (MlHooks){.ctx = s, .command = command, .git = git_hook, .index = s->broken_index ? broken_index : NULL,
                     .before_step = before_step, .out = out};
}

void mlst_apply_json(MlStubs *s, const AvpJson *o) {
    AvpArena *a = s->a;
    if (!o) return;
    const AvpJson *v;
    if ((v = avp_json_getz(o, "lane_scores"))) {
        if (avp_json_getz(v, "fn_A")) s->score_a = (AvpJson *)avp_json_getz(v, "fn_A");
        if (avp_json_getz(v, "fn_B")) s->score_b = (AvpJson *)avp_json_getz(v, "fn_B");
    }
    if ((v = avp_json_getz(o, "changes_rows"))) s->changes_rows = (AvpJson *)v;
    if ((v = avp_json_getz(o, "gate_ok"))) s->gate_ok = avp_json_truthy(v);
    s->gate_rc = rc_of(o, "gate_rc", s->gate_rc);
    if ((v = avp_json_getz(o, "rules_changed"))) s->rules_changed = (AvpJson *)v;
    if ((v = avp_json_getz(o, "rules_tree"))) s->rules_tree = (AvpJson *)v;
    s->rules_tree_rc = rc_of(o, "rules_tree_rc", s->rules_tree_rc);
    if ((v = avp_json_getz(o, "changes_doc")) && !avp_json_is_null(v)) s->changes_doc = (AvpJson *)v;
    if ((v = avp_json_getz(o, "changes_text")) && avp_json_is_str(v)) s->changes_text = avp_cz(a, v->u.str);
    if ((v = avp_json_getz(o, "lane_report")) && !avp_json_is_null(v)) s->lane_report = (AvpJson *)v;
    if ((v = avp_json_getz(o, "changes_write"))) s->changes_write = avp_json_truthy(v);
    s->layout_rc = rc_of(o, "layout_rc", s->layout_rc);
    s->ledger_rc = rc_of(o, "ledger_rc", s->ledger_rc);
    s->classes_rc = rc_of(o, "classes_rc", s->classes_rc);
}

int mlfx_run(MlFixture *fx, MlStubs *s, const char *branch, const char *onto, const char *const *extra, size_t n,
             AvpStr *text) {
    AvpArena *a = fx->a;
    char **argv = AVP_NEWN(a, char *, n + 12);
    int k = 0;
    argv[k++] = "merge_lane";
    const char *fixed[] = {"--slot", fx->slot, "--branch", branch ? branch : "lane/x", "--onto", onto ? onto : "main",
                           "--repo", fx->main, "--log-dir", fx->logs};
    for (int i = 0; i < 10; i++) argv[k++] = (char *)fixed[i];
    for (size_t i = 0; i < n; i++) argv[k++] = (char *)extra[i];
    AvpBuf out;
    avp_buf_init(&out, a);
    MlHooks h = mlst_hooks(s, &out);
    int rc = ml_main(k, argv, &h);
    *text = avp_buf_str(&out);
    return rc;
}
