/* StepTests of tools/test_gate.py: the step logic with the build, the checksum and the sub-tools stubbed.
 *   test_preflight_dirty_refused         -> preflight_dirty_refused
 *   test_preflight_allowed_churn_is_clean -> preflight_allowed_churn_is_clean
 *   test_preflight_contract_mismatch     -> preflight_contract_mismatch
 *   test_preflight_missing_tools         -> preflight_missing_tools
 *   test_check_tools_is_doctor_check_required -> check_tools_is_doctor_check_required
 *   test_contract_checks                 -> contract_checks
 *   test_checksum_ok                     -> checksum_ok
 *   test_checksum_red_runs_attribution   -> checksum_red_runs_attribution
 *   test_checksum_tools_disagree         -> checksum_tools_disagree
 *   test_coverage_refuses_empty_work_list -> coverage_refuses_empty_work_list
 *   test_coverage_names_unit_below_100   -> coverage_names_unit_below_100
 *   test_coverage_missing_unit           -> coverage_missing_unit
 *   test_coverage_ok                     -> coverage_ok
 *   test_regressions_empty_baseline_refused -> regressions_empty_baseline_refused
 *   test_regressions_none                -> regressions_none
 *   test_regressions_tools_disagree      -> regressions_tools_disagree
 *   test_layout                          -> layout
 *   test_rules                           -> rules
 *   test_ledger                          -> ledger
 * The coverage tests write the report the Python mocked (read_report) and answer load_units through the hook. */
#include "gate_fixtures.h"

static TfRunner RUN;

static Gate *gate_with(const char *tmp, const TfEntry *table, size_t n, GateOpts o) {
    RUN = (TfRunner){table, n, {0}};
    AvpBuf *out = AVP_NEW(TA, AvpBuf);
    avp_buf_init(out, TA);
    Gate *g = gate_new(TA, tmp, &o, tf_run, &RUN, out);
    g->rules_tools = tf_tools(&RUN);
    g->tree = tmp; /* the steps read the tree; here the fixture is the tree */
    return g;
}

static GateStep *step(const char *name) {
    GateStep *st = AVP_NEW(TA, GateStep);
    gate_step_init(TA, st, name);
    return st;
}

static const AvpStrVec *PROBLEMS;
static void mocked_tools(Gate *g, AvpStrVec *out) {
    (void)g;
    for (size_t i = 0; PROBLEMS && i < PROBLEMS->n; i++) avp_vec_push(out, PROBLEMS->v[i]);
}

static const char *fixture(void) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, NULL);
    return tmp;
}

#define ST(lit) lit, sizeof(lit) - 1
static TfEntry status_entry(const char *text, size_t n) {
    return (TfEntry){"status", 0, text, NULL, NULL, n};
}

/* ---- preflight ------------------------------------------------------------------------------------------------- */
AVP_TEST(preflight_dirty_refused) {
    const char *tmp = fixture();
    TfEntry t[] = {status_entry(ST(" M src/a.c\0?? src/new.c\0"))};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    GateStep *st = step("preflight");
    gate_step_preflight(g, st);
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "dirty tree"));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(preflight_allowed_churn_is_clean) {
    const char *tmp = fixture();
    TfEntry t[] = {status_entry(ST("?? build/score_history.sqlite\0?? tools/__pycache__/x.pyc\0"))};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    PROBLEMS = NULL;
    g->check_tools = mocked_tools;
    GateStep *st = step("preflight");
    gate_step_preflight(g, st);
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
    CHECK_EQ_I64(g->dirty.n, 0);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(preflight_contract_mismatch) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, "0000000000000000000000000000000000000000");
    TfEntry t[] = {status_entry(ST(""))};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    PROBLEMS = NULL;
    g->check_tools = mocked_tools;
    GateStep *st = step("preflight");
    gate_step_preflight(g, st);
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "hash contract"));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(preflight_missing_tools) {
    const char *tmp = fixture();
    TfEntry t[] = {status_entry(ST(""))};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    AvpStrVec p = {0};
    avp_vec_push(&p, AVP_S("tools/bin/dtk.exe missing"));
    PROBLEMS = &p;
    g->check_tools = mocked_tools;
    GateStep *st = step("preflight");
    gate_step_preflight(g, st);
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "pinned tools"));
    PROBLEMS = NULL;
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(check_tools_is_doctor_check_required) {
    const char *tmp = fixture();
    avp_makedirs(tf_join(tmp, "tools/bin"), true);
    TfEntry t[] = {{"--version", 0, "1.13.2\n", NULL, NULL}};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    /* the C gate links doctor's check_required (DEVIATIONS.md D-gate-1) */
    tf_write(tf_join(tmp, "tools/bin/ninja.exe"), "");
    AvpStrVec problems = {0};
    gate_check_tools_doctor(g, &problems);
    AvpStr joined = avp_str_join(TA, AVP_S("\n"), problems.v, problems.n);
    CHECK(avp_str_contains(joined, AVP_S("MISSING dtk")));
    CHECK(avp_str_contains(joined, AVP_S("MISSING objdiff-cli")));
    CHECK(avp_str_contains(joined, AVP_S("MISSING compilers")));
    CHECK(!avp_str_contains(joined, AVP_S("ninja"))); /* answered through the gate's runner: 1.13.2 is the pin */
    CHECK(tf_called(&RUN, "--version"));
    CHECK(!avp_str_contains(joined, AVP_S("slot")));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(contract_checks) {
    const char *tmp = fixture();
    const char *cfg = tf_join(tmp, "config/R5WEA4");
    const char *why, *sha;
    CHECK(gate_verify_contract(TA, tf_join(cfg, "build.sha1"), tf_join(cfg, "config.yml"), tmp, NULL, &why, &sha));
    tf_append(tf_join(tmp, "orig/R5WEA4/sys/main.dol"), "x");
    CHECK(!gate_verify_contract(TA, tf_join(cfg, "build.sha1"), tf_join(cfg, "config.yml"), tmp, NULL, &why, &sha));
    CHECK(tf_contains(why, "retail DOL"));
    avp_tempdir_cleanup(tmp);
}

/* ---- checksum -------------------------------------------------------------------------------------------------- */
AVP_TEST(checksum_ok) {
    const char *tmp = fixture();
    TfEntry t[] = {{"shasum", 0, "build/R5WEA4/main.dol: \x1b[32mOK\x1b[39m\n", NULL, NULL}};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    GateStep *st = step("checksum");
    gate_step_checksum(g, st);
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(checksum_red_runs_attribution) {
    const char *tmp = fixture();
    tf_append(tf_join(tmp, "build/R5WEA4/main.dol"), "!");
    TfEntry t[] = {{"shasum", 1, "build/R5WEA4/main.dol: FAILED\n", NULL, NULL},
                   {"attribute", 1, "== game/PadRumble.cpp [Matching]  1 bytes in 1 symbols\n   .text    fn_X\n", NULL,
                    NULL}};
    Gate *g = gate_with(tmp, t, 2, tf_opts());
    GateStep *st = step("checksum");
    gate_step_checksum(g, st);
    CHECK(st->status == GATE_FAIL);
    CHECK_MSG(tf_contains(st->reason, "game/PadRumble.cpp"), "%s", st->reason);
    CHECK(tf_called(&RUN, "attribute"));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(checksum_tools_disagree) {
    const char *tmp = fixture();
    tf_append(tf_join(tmp, "build/R5WEA4/main.dol"), "!");
    TfEntry t[] = {{"shasum", 0, "build/R5WEA4/main.dol: OK\n", NULL, NULL}};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    GateStep *st = step("checksum");
    gate_step_checksum(g, st);
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "disagree"));
    avp_tempdir_cleanup(tmp);
}

/* ---- coverage -------------------------------------------------------------------------------------------------- */
static const char *const *STATUSES;
static size_t NSTATUS;

static void fake_units(void *ctx, AvpArena *a, const char *root, const char *version, GateUnits *out) {
    (void)ctx;
    (void)root;
    (void)version;
    for (size_t i = 0; i < NSTATUS; i++) {
        GateUnit u = {avp_sprintf(a, "main/u%zu", i), avp_sprintf(a, "u%zu", i), STATUSES[i], NULL, NULL, false};
        avp_vec_push(out, u);
    }
}

/* fake_report: {unit: [(name, fuzzy)]} with the unit's fuzzy the minimum (100 when empty), complete */
typedef struct {
    const char *unit;
    const char *names[3];
    double scores[3];
    size_t n;
} FakeUnit;

static void write_fake_report(const char *path, const FakeUnit *us, size_t n) {
    AvpJson *units = avp_json_arr(TA);
    for (size_t i = 0; i < n; i++) {
        AvpJson *u = avp_json_obj(TA), *fs = avp_json_arr(TA), *md = avp_json_obj(TA), *ms = avp_json_obj(TA);
        double mn = 100.0;
        for (size_t k = 0; k < us[i].n; k++) {
            AvpJson *f = avp_json_obj(TA);
            avp_json_set(TA, f, AVP_S("name"), avp_json_strz(TA, us[i].names[k]));
            avp_json_set(TA, f, AVP_S("fuzzy_match_percent"), avp_json_float(TA, us[i].scores[k]));
            avp_json_arr_push(TA, fs, f);
            if (k == 0 || us[i].scores[k] < mn) mn = us[i].scores[k];
        }
        avp_json_set(TA, u, AVP_S("name"), avp_json_strz(TA, us[i].unit));
        avp_json_set(TA, ms, AVP_S("fuzzy_match_percent"), avp_json_float(TA, mn));
        avp_json_set(TA, u, AVP_S("measures"), ms);
        avp_json_set(TA, md, AVP_S("complete"), avp_json_bool(true));
        avp_json_set(TA, u, AVP_S("metadata"), md);
        avp_json_set(TA, u, AVP_S("functions"), fs);
        avp_json_arr_push(TA, units, u);
    }
    AvpJson *doc = avp_json_obj(TA);
    avp_json_set(TA, doc, AVP_S("units"), units);
    tf_write(path, avp_cz(TA, avp_json_dumps(TA, doc, NULL)));
}

static GateStep *coverage(const char *const *statuses, size_t ns, const FakeUnit *us, size_t nu) {
    const char *tmp = fixture();
    write_fake_report(tf_join(tmp, "build/R5WEA4/report.json"), us, nu);
    STATUSES = statuses;
    NSTATUS = ns;
    Gate *g = gate_with(tmp, NULL, 0, tf_opts());
    g->load_units = fake_units;
    GateStep *st = step("coverage");
    gate_step_coverage(g, st);
    return st;
}

AVP_TEST(coverage_refuses_empty_work_list) {
    static const char *const s[] = {"NonMatching"};
    FakeUnit u[] = {{"main/u0", {"f"}, {50.0}, 1}};
    GateStep *st = coverage(s, 1, u, 1);
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "nothing is linked"));
}

AVP_TEST(coverage_names_unit_below_100) {
    static const char *const s[] = {"Matching", "Matching"};
    FakeUnit u[] = {{"main/u0", {"f"}, {100.0}, 1}, {"main/u1", {"g"}, {99.0}, 1}};
    GateStep *st = coverage(s, 2, u, 2);
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "main/u1"));
}

AVP_TEST(coverage_missing_unit) {
    static const char *const s[] = {"Matching"};
    GateStep *st = coverage(s, 1, NULL, 0);
    CHECK(st->status == GATE_FAIL);
}

AVP_TEST(coverage_ok) {
    static const char *const s[] = {"Matching", "NonMatching"};
    FakeUnit u[] = {{"main/u0", {"f", "g"}, {100.0, 100.0}, 2}, {"main/u1", {"h"}, {3.0}, 1}};
    GateStep *st = coverage(s, 2, u, 2);
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
    CHECK_EQ_I64(avp_json_get_i64(st->data, "functions", -1), 2);
}

/* ---- regressions ----------------------------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    i64 addr;
    double score;
} Fn;

static void write_report(const char *path, const Fn *fs, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, TA);
    avp_buf_putz(&b, "{\"measures\": {}, \"units\": [{\"name\": \"main/u\", \"measures\": {}, \"metadata\": {\"complete\": "
                     "false}, \"functions\": [");
    for (size_t i = 0; i < n; i++) {
        char sb[32];
        avp_float_repr(fs[i].score, sb);
        avp_buf_printf(&b, "%s{\"name\": \"%s\", \"size\": \"8\", \"fuzzy_match_percent\": %s, \"metadata\": "
                           "{\"virtual_address\": \"%lld\"}}",
                       i ? ", " : "", fs[i].name, sb, (long long)fs[i].addr);
    }
    avp_buf_putz(&b, "]}]}");
    tf_write(path, avp_buf_cz(&b));
}

typedef struct {
    const char *tmp, *changes;
} ChangesCtx;

static void write_changes(void *ctx, const char *cwd) {
    (void)cwd;
    ChangesCtx *c = ctx;
    tf_write(tf_join(c->tmp, "build/R5WEA4/report_changes.json"), c->changes);
}

static GateStep *regressions(const Fn *base, size_t nb, const Fn *fresh, size_t nf, const char *changes) {
    const char *tmp = fixture();
    write_report(tf_join(tmp, "config/R5WEA4/baseline_report.json"), base, nb);
    write_report(tf_join(tmp, "build/R5WEA4/report.json"), fresh, nf);
    ChangesCtx *c = AVP_NEW(TA, ChangesCtx);
    *c = (ChangesCtx){tmp, changes};
    TfEntry *t = AVP_NEW(TA, TfEntry);
    *t = (TfEntry){"report_changes", 0, "", write_changes, c};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    GateStep *st = step("regressions");
    gate_step_regressions(g, st);
    return st;
}

AVP_TEST(regressions_empty_baseline_refused) {
    Fn fresh[] = {{"f", 1, 100.0}};
    GateStep *st = regressions(NULL, 0, fresh, 1, "{\"units\": []}");
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "nothing to compare"));
}

AVP_TEST(regressions_none) {
    Fn base[] = {{"f", 1, 50.0}}, fresh[] = {{"f", 1, 60.0}};
    GateStep *st = regressions(base, 1, fresh, 1, "{\"units\": []}");
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
}

AVP_TEST(regressions_tools_disagree) {
    Fn base[] = {{"f", 1, 50.0}}, fresh[] = {{"f", 1, 40.0}};
    GateStep *st = regressions(base, 1, fresh, 1, "{\"units\": []}");
    CHECK(st->status == GATE_FAIL);
    CHECK(tf_contains(st->reason, "disagree"));
}


/* ---- a function that left its unit, a removed placeholder ------------------------------------------------------- */
#define SPLITS_HEAD "Sections:\n\t.text       type:code align:4\n\nu.cpp:\n"
#define SPLITS_WIDE SPLITS_HEAD "\t.text       start:0x80000000 end:0x80000020\n"
#define SPLITS_SHRUNK SPLITS_HEAD "\t.text       start:0x80000000 end:0x80000010\n"
#define SYMBOLS "f = .text:0x80000000; // type:function size:0x10\ng = .text:0x80000010; // type:function size:0x10\n"
#define SRC_F "int f(void) {\n    return 1;\n}  // 0x80000000\n"
#define SRC_G_REAL "int g(void) {\n    return 2;\n}  // 0x80000010\n"
#define SRC_G_STUB "int g(void) {\n    // TODO\n}  // 0x80000010\n"
#define CHANGES_G(from)                                                                                            \
    "{\"units\": [{\"name\": \"main/u\", \"functions\": [{\"name\": \"g\", \"metadata\": {\"virtual_address\": "   \
    "\"2147483664\"}, \"from\": {\"fuzzy_match_percent\": " from "}}]}]}"

/* The baseline has f (100%) and g (g_score); the fresh report only f; g is gone from the lane's source and from its
 * unit's range when shrink. base_src is the baseline revision's source. */
static GateStep *dropped(const char *g_score, const char *changes, const char *base_src, const char *lane_src,
                         bool shrink) {
    const char *tmp = fixture();
    Fn base[] = {{"f", 2147483648LL, 100.0}, {"g", 2147483664LL, 0}};
    base[1].score = g_score[0] == '1' ? 100.0 : 20.0;
    Fn fresh[] = {{"f", 2147483648LL, 100.0}};
    write_report(tf_join(tmp, "config/R5WEA4/baseline_report.json"), base, 2);
    write_report(tf_join(tmp, "build/R5WEA4/report.json"), fresh, 1);
    tf_write(tf_join(tmp, "config/R5WEA4/splits.txt"), shrink ? SPLITS_SHRUNK : SPLITS_WIDE);
    tf_write(tf_join(tmp, "config/R5WEA4/symbols.txt"), SYMBOLS);
    tf_write(tf_join(tmp, "src/u.cpp"), lane_src);
    ChangesCtx *c = AVP_NEW(TA, ChangesCtx);
    *c = (ChangesCtx){tmp, changes};
    TfEntry *t = AVP_NEWN(TA, TfEntry, 4);
    t[0] = (TfEntry){"report_changes", 0, "", write_changes, c};
    t[1] = (TfEntry){"--format=%H", 0, "REV\n", NULL, NULL};
    t[2] = (TfEntry){"splits.txt", 0, SPLITS_WIDE, NULL, NULL};
    t[3] = (TfEntry){"src/u.cpp", 0, base_src, NULL, NULL};
    Gate *g = gate_with(tmp, t, 4, tf_opts());
    GateStep *st = step("regressions");
    gate_step_regressions(g, st);
    return st;
}

static bool has_line(const GateStep *st, const char *needle) {
    for (size_t i = 0; i < st->lines.n; i++)
        if (avp_str_contains(st->lines.v[i], avp_str(needle))) return true;
    return false;
}

AVP_TEST(regressions_function_that_left_its_unit_is_not_a_regression) {
    GateStep *st = dropped("20", CHANGES_G("20.0"), SRC_F SRC_G_REAL, SRC_F, true);
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
    CHECK(tf_contains(st->reason, "1 left their unit"));
    CHECK(has_line(st, "left the unit (range shrunk by the lane; not a regression):"));
    CHECK(has_line(st, ".text 0x80000000-0x80000020 -> 0x80000000-0x80000010"));
}

AVP_TEST(regressions_function_inside_the_range_that_fell_still_blocks) {
    GateStep *st = dropped("20", CHANGES_G("20.0"), SRC_F SRC_G_REAL, SRC_F, false);
    CHECK(st->status == GATE_FAIL);
    CHECK(!has_line(st, "left the unit"));
}

AVP_TEST(regressions_exact_function_that_left_its_unit_still_blocks) {
    GateStep *st = dropped("100", CHANGES_G("100.0"), SRC_F SRC_G_REAL, SRC_F, true);
    CHECK(st->status == GATE_FAIL);
    CHECK(!has_line(st, "left the unit"));
}

AVP_TEST(regressions_removed_placeholder_is_not_a_regression) {
    GateStep *st = dropped("20", CHANGES_G("20.0"), SRC_F SRC_G_STUB, SRC_F, false);
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
    CHECK(tf_contains(st->reason, "1 placeholder(s) removed"));
    CHECK(has_line(st, "placeholder removed (function not yet written; not a regression):"));
}

AVP_TEST(regressions_removed_function_of_real_code_still_blocks) {
    GateStep *st = dropped("20", CHANGES_G("20.0"), SRC_F SRC_G_REAL, SRC_F, false);
    CHECK(st->status == GATE_FAIL);
    CHECK(!has_line(st, "placeholder removed"));
}

/* ---- layout, rules, ledger ------------------------------------------------------------------------------------- */
AVP_TEST(layout) {
    const char *tmp = fixture();
    static const struct {
        i64 rc;
        const char *text;
        GateStatus status;
    } cases[] = {{0, "units checked: 5; Matching 5 (0 with differences)\n", GATE_PASS},
                 {0, "units checked: 0\n", GATE_FAIL},
                 {1, "DIFF  Matching    game/X\nunits checked: 5\n", GATE_FAIL},
                 {2, "boom\n", GATE_FAIL}};
    for (size_t i = 0; i < 4; i++) {
        TfEntry t[] = {{"layout", cases[i].rc, cases[i].text, NULL, NULL}};
        Gate *g = gate_with(tmp, t, 1, tf_opts());
        GateStep *st = step("layout");
        gate_step_layout(g, st);
        CHECK_MSG(st->status == cases[i].status, "%s: %s", cases[i].text, st->reason);
    }
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(rules) {
    const char *tmp = fixture();
    avp_makedirs(tf_join(tmp, "build/gate"), true);
    static const struct {
        i64 rc;
        const char *doc;
        GateStatus status;
    } cases[] = {
        {0, "{\"scanned\": [\"a.c\"], \"findings\": [], \"blocking\": []}", GATE_PASS},
        {1, "{\"scanned\": [\"a.c\"], \"findings\": [{}], \"blocking\": [{\"path\": \"a.c\", \"line\": 1, \"rule\": \"R17\"}]}",
         GATE_FAIL},
        {0, "{\"scanned\": [], \"findings\": [], \"blocking\": []}", GATE_FAIL},
        {2, "Traceback\n", GATE_FAIL},
    };
    for (size_t i = 0; i < 4; i++) {
        TfEntry t[] = {{"check_rules", cases[i].rc, cases[i].doc, NULL, NULL}};
        Gate *g = gate_with(tmp, t, 1, tf_opts());
        GateStep *st = step("rules");
        gate_step_rules(g, st);
        CHECK_MSG(st->status == cases[i].status, "%s: %s", cases[i].doc, st->reason);
    }
    TfEntry t[] = {{"classes scan", 1, "x", NULL, NULL}};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    GateStep *st = step("rules");
    gate_step_rules(g, st);
    CHECK(st->status == GATE_FAIL);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(ledger) {
    const char *tmp = fixture();
    TfEntry t[] = {{"progress", 1, "", NULL, NULL}};
    Gate *g = gate_with(tmp, t, 1, tf_opts());
    GateStep *st = step("ledger");
    gate_step_ledger(g, st);
    CHECK(st->status == GATE_WARN);
    GateOpts o = tf_opts();
    o.merge = true;
    g = gate_with(tmp, t, 1, o);
    st = step("ledger");
    gate_step_ledger(g, st);
    CHECK(st->status == GATE_FAIL);
    g = gate_with(tmp, NULL, 0, tf_opts());
    st = step("ledger");
    gate_step_ledger(g, st);
    CHECK(st->status == GATE_PASS);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST_MAIN("gate")
