/* The driver tests of tools/test_gate.py (DriverTests), with the steps scripted (ScriptedGate).
 *   test_all_pass_is_ok                                      -> all_pass_is_ok
 *   test_each_step_red_in_turn                               -> each_step_red_in_turn
 *   test_keep_going_runs_later_steps_but_stays_red           -> keep_going_runs_later_steps_but_stays_red
 *   test_build_failure_blocks_dependent_steps_even_with_keep_going -> build_failure_blocks_dependent_steps
 *   test_a_step_that_does_nothing_never_counts_as_passed     -> a_step_that_does_nothing_never_counts_as_passed
 *   test_crashing_step_is_red                                -> crashing_step_is_red
 *   test_ledger_warning_passes_unless_merge                  -> ledger_warning_passes_unless_merge
 *   test_only_ledger_may_warn                                -> only_ledger_may_warn
 *   test_dirty_tree_never_ok                                 -> dirty_tree_never_ok
 *   test_verdict_missing_step                                -> verdict_missing_step
 *   test_second_gate_refused_while_locked                    -> second_gate_refused_while_locked
 *   test_main_exit_codes                                     -> main_exit_codes */
#include "gate_fixtures.h"

#define SHA "6a3964eaf1ebe8225283363e4161a793a03683d3"

/* ScriptedGate.outcome: per step "pass" (default), "fail", "warn", "nothing" or "crash"; DIRTY sets dirty paths */
static const char *OUTCOME[GATE_NSTEPS];
static const char *DIRTY_PATH;

static void scripted(Gate *g, GateStep *st) {
    size_t k = 0;
    while (strcmp(gate_step_names[k], st->name) != 0) k++;
    const char *what = OUTCOME[k] ? OUTCOME[k] : "pass";
    if (strcmp(what, "pass") == 0) gate_step_ok(st, "fine");
    else if (strcmp(what, "fail") == 0) gate_step_fail(st, avp_sprintf(TA, "%s broke", st->name));
    else if (strcmp(what, "warn") == 0) gate_step_warn(st, avp_sprintf(TA, "%s is stale", st->name));
    else if (strcmp(what, "crash") == 0) avp_throw(AVP_ERR_OTHER, "boom");
    if (strcmp(st->name, "checksum") == 0 && strcmp(what, "pass") == 0) g->dol_sha1 = SHA;
    if (strcmp(st->name, "coverage") == 0 && strcmp(what, "pass") == 0) {
        g->units_linked = 3;
        g->functions_linked = 30;
    }
    if (strcmp(st->name, "preflight") == 0 && DIRTY_PATH) {
        g->dirty = (AvpStrVec){0};
        avp_vec_push(&g->dirty, avp_str(DIRTY_PATH));
    }
}

static void script_steps(Gate *g, void *ctx) {
    (void)ctx;
    for (size_t i = 0; i < GATE_NSTEPS; i++) g->step_fn[i] = scripted;
}

static void set_outcome(const char *step, const char *what) {
    for (size_t i = 0; i < GATE_NSTEPS; i++) OUTCOME[i] = NULL;
    DIRTY_PATH = NULL;
    for (size_t i = 0; step && i < GATE_NSTEPS; i++)
        if (strcmp(gate_step_names[i], step) == 0) OUTCOME[i] = what;
}

static TfRunner RUNNER;

static Gate *run_gate(const char *tmp, GateOpts o, bool *ok, const char **line) {
    RUNNER = tf_git_runner();
    AvpBuf *out = AVP_NEW(TA, AvpBuf);
    avp_buf_init(out, TA);
    Gate *g = gate_new(TA, tmp, &o, tf_run, &RUNNER, out);
    script_steps(g, NULL);
    *ok = gate_execute(g, line);
    return g;
}

static const char *fixture(void) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, NULL);
    return tmp;
}

AVP_TEST(all_pass_is_ok) {
    const char *tmp = fixture();
    set_outcome(NULL, NULL);
    bool ok;
    const char *line;
    run_gate(tmp, tf_opts(), &ok, &line);
    CHECK(ok);
    CHECK(tf_starts(line, "GATE OK " SHA " units=3 functions=30 ("));
    AvpStr text;
    REQUIRE(avp_read_text(TA, tf_join(tmp, "build/gate/last.json"), AVP_TM_UTF8, &text));
    AvpJsonError e;
    AvpJson *snap = avp_json_parse(TA, text, &e);
    REQUIRE(snap);
    CHECK_STR(avp_json_get_str(snap, "result", AVP_S("")), "OK");
    CHECK_STR(avp_json_get_str(snap, "commit", AVP_S("")), "abc123");
    CHECK(avp_isfile(tf_join(tmp, "build/gate/last-ok.json")));
    CHECK(!avp_exists(tf_join(tmp, "build/gate/lock")));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(each_step_red_in_turn) {
    const char *tmp = fixture();
    for (size_t k = 0; k < GATE_NSTEPS; k++) {
        set_outcome(gate_step_names[k], "fail");
        GateOpts o = tf_opts();
        o.merge = true;
        bool ok;
        const char *line;
        Gate *g = run_gate(tmp, o, &ok, &line);
        CHECK(!ok);
        CHECK_MSG(tf_starts(line, avp_sprintf(TA, "GATE RED %s: ", gate_step_names[k])), "%s", line);
        for (size_t j = k + 1; j < GATE_NSTEPS; j++) CHECK(g->steps[j].status == GATE_NOT_RUN);
    }
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(keep_going_runs_later_steps_but_stays_red) {
    const char *tmp = fixture();
    set_outcome("checksum", "fail");
    GateOpts o = tf_opts();
    o.keep_going = true;
    bool ok;
    const char *line;
    Gate *g = run_gate(tmp, o, &ok, &line);
    CHECK(!ok);
    CHECK(tf_starts(line, "GATE RED checksum: "));
    CHECK(tf_step(g, "rules")->status == GATE_PASS);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(build_failure_blocks_dependent_steps) {
    const char *tmp = fixture();
    set_outcome("build", "fail");
    GateOpts o = tf_opts();
    o.keep_going = true;
    bool ok;
    const char *line;
    Gate *g = run_gate(tmp, o, &ok, &line);
    CHECK(tf_starts(line, "GATE RED build: "));
    for (size_t i = 0; i < GATE_NSTEPS; i++)
        if (gate_needs_build(g->steps[i].name)) CHECK(g->steps[i].status == GATE_NOT_RUN);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(a_step_that_does_nothing_never_counts_as_passed) {
    const char *tmp = fixture();
    for (size_t k = 0; k < GATE_NSTEPS; k++) {
        set_outcome(gate_step_names[k], "nothing");
        bool ok;
        const char *line;
        run_gate(tmp, tf_opts(), &ok, &line);
        CHECK(!ok);
        CHECK_MSG(tf_starts(line, avp_sprintf(TA, "GATE RED %s: ", gate_step_names[k])), "%s", line);
    }
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(crashing_step_is_red) {
    const char *tmp = fixture();
    set_outcome("layout", "crash");
    bool ok;
    const char *line;
    run_gate(tmp, tf_opts(), &ok, &line);
    CHECK(!ok);
    CHECK_MSG(tf_contains(line, "GATE RED layout: layout crashed"), "%s", line);
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(ledger_warning_passes_unless_merge) {
    const char *tmp = fixture();
    set_outcome("ledger", "warn");
    bool ok;
    const char *line;
    run_gate(tmp, tf_opts(), &ok, &line);
    CHECK_MSG(ok, "%s", line);
    GateOpts o = tf_opts();
    o.merge = true;
    run_gate(tmp, o, &ok, &line);
    CHECK(!ok);
    CHECK(tf_starts(line, "GATE RED ledger: "));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(only_ledger_may_warn) {
    const char *tmp = fixture();
    set_outcome("rules", "warn");
    bool ok;
    const char *line;
    run_gate(tmp, tf_opts(), &ok, &line);
    CHECK(!ok);
    CHECK(tf_starts(line, "GATE RED rules: "));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(dirty_tree_never_ok) {
    const char *tmp = fixture();
    set_outcome(NULL, NULL);
    DIRTY_PATH = "src/x.c";
    GateOpts o = tf_opts();
    o.allow_dirty = true;
    bool ok;
    const char *line;
    run_gate(tmp, o, &ok, &line);
    CHECK(!ok);
    CHECK_MSG(tf_starts(line, "GATE DIRTY "), "%s", line);
    CHECK(!tf_contains(line, "GATE OK"));
    DIRTY_PATH = NULL;
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(verdict_missing_step) {
    GateStep steps[GATE_NSTEPS - 1];
    for (size_t i = 0; i + 1 < GATE_NSTEPS; i++) {
        gate_step_init(TA, &steps[i], gate_step_names[i]);
        gate_step_ok(&steps[i], "x");
    }
    GateStep *bad = NULL;
    CHECK(!gate_verdict(TA, steps, GATE_NSTEPS - 1, false, false, &bad));
    REQUIRE(bad);
    CHECK_CZ(bad->name, "ledger");
}

AVP_TEST(second_gate_refused_while_locked) {
    const char *tmp = fixture();
    set_outcome(NULL, NULL);
    tf_write(tf_join(tmp, "build/gate/lock"), "");
    int code = 0;
    const char *msg = NULL;
    bool ok;
    const char *line;
    TF_RAISES(code, msg, run_gate(tmp, tf_opts(), &ok, &line));
    CHECK_EQ_I64(code, SHSM_EXC_USAGE);
    CHECK(tf_contains(msg, "another gate holds"));
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(main_exit_codes) {
    const char *tmp = fixture();
    static const struct {
        const char *step, *what;
        int code;
        const char *prefix;
    } cases[] = {{NULL, NULL, 0, "GATE OK"}, {"rules", "fail", 1, "GATE RED rules"}};
    for (size_t i = 0; i < 2; i++) {
        set_outcome(cases[i].step, cases[i].what);
        RUNNER = tf_git_runner();
        GateMainHooks h = {tf_run, &RUNNER, NULL, NULL, script_steps, NULL};
        char *argv[] = {"gate", "--root", (char *)tmp};
        int rc = gate_main(3, argv, &h);
        CHECK_EQ_I64(rc, cases[i].code);
        AvpStr last;
        REQUIRE(avp_read_text(TA, tf_join(tmp, "build/gate/last.json"), AVP_TM_UTF8, &last));
        AvpJsonError e;
        AvpJson *snap = avp_json_parse(TA, last, &e);
        REQUIRE(snap);
        CHECK(avp_str_startswith(avp_json_get_str(snap, "line", AVP_S("")), avp_str(cases[i].prefix)));
    }
    avp_tempdir_cleanup(tmp);
}

AVP_TEST_MAIN("gate")
