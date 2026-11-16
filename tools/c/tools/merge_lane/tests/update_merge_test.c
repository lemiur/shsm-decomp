/* TestUpdateAndMerge and TestPinnedOnto of tools/test_merge_lane.py.
 *   TestUpdateAndMerge.test_update_commits_baseline_and_ledger -> update_commits_baseline_and_ledger
 *   TestUpdateAndMerge.test_update_not_run_after_a_failed_step -> update_not_run_after_a_failed_step
 *   TestUpdateAndMerge.test_ledger_failure_does_not_commit    -> ledger_failure_does_not_commit
 *   TestUpdateAndMerge.test_merge_fast_forwards_checked_out_main -> merge_fast_forwards_checked_out_main
 *   TestUpdateAndMerge.test_merge_moves_branch_not_checked_out_anywhere -> merge_moves_branch_not_checked_out_anywhere
 *   TestUpdateAndMerge.test_merge_refuses_when_onto_moved_after_rebase -> merge_refuses_when_onto_moved_after_rebase
 *       (the patched Stubs.layout that commits on main is an MlMutation on "layout")
 *   TestUpdateAndMerge.test_merge_without_update_in_this_run_refused_by_arguments -> merge_without_update_in_this_run_refused_by_arguments
 *       differs: ml.parse_args's SystemExit is ml_main's return code 2 (argparse's exit code)
 *   TestPinnedOnto.test_rebase_uses_the_pinned_commit_and_merge_stops -> rebase_uses_the_pinned_commit_and_merge_stops
 *       (the patched STEP_FUNCS["rebase"] is an MlMutation on "step:rebase", run by MlHooks.before_step)
 *   TestPinnedOnto.test_baseline_and_rules_use_the_pinned_commit -> baseline_and_rules_use_the_pinned_commit
 *       (the patched configure is an MlMutation on "configure"; ctx.onto_ref() at rules_changed is MlStubs.seen_onto) */
#include "ml_test_fixtures.h"

/* TestUpdateAndMerge.prepare */
static void prepare(TxCase *c) {
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
}

/* One main_commit(rel, text) on the first call of `on`. */
static void mutate_on(TxCase *c, const char *on, const char *rel, AvpStr text) {
    c->s.mutations = AVP_NEWN(TA, MlMutation, 1);
    c->s.mutations[0] = (MlMutation){on, rel, text, false};
    c->s.nmutations = 1;
}

static size_t count_char(const char *s, char ch) {
    size_t n = 0;
    for (; *s; s++) n += *s == ch;
    return n;
}

AVP_TEST(update_commits_baseline_and_ledger) {
    TxCase *c = tx_begin();
    prepare(c);
    c->s.score_b = avp_json_float(TA, 100.0);
    tx_run(c, NULL, NULL, "--update", NULL);
    TX_RC(c, 0);
    TX_ROW(c, "ledger", "PASS");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "log", "-1", "--format=%s"), "review: lane/x merge (ledger, baseline)");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "status", "--porcelain"), "");
    CHECK_EQ_U64(count_char(TX_GIT(c, c->fx.slot, "show", "--stat", "--format=", "HEAD"), '|'), 1); /* progress.md only */
    TX_EQ_CZ(TX_GIT(c, c->fx.main, "rev-parse", "main"), TX_GIT(c, c->fx.slot, "rev-parse", "main")); /* not merged */
    tx_end(c);
}

AVP_TEST(update_not_run_after_a_failed_step) {
    TxCase *c = tx_begin();
    prepare(c);
    c->s.layout_rc = 1;
    tx_run(c, NULL, NULL, "--update", NULL);
    TX_RC(c, 1);
    TX_ROW(c, "ledger", "SKIP");
    CHECK(!mlst_called(&c->s, "ledger_progress"));
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "log", "-1", "--format=%s"), "lane work");
    tx_end(c);
}

AVP_TEST(ledger_failure_does_not_commit) {
    TxCase *c = tx_begin();
    prepare(c);
    c->s.ledger_rc = 1;
    tx_run(c, NULL, NULL, "--update", NULL);
    TX_RC(c, 1);
    TX_ROW(c, "ledger", "FAIL");
    /* the progress ledger the stub rewrote and the baseline copy were put back: the slot is clean, not left dirty */
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "status", "--porcelain"), "");
    TX_IN(c, "restored config/R5WEA4/baseline_report.json and docs/progress.md");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "log", "-1", "--format=%s"), "lane work");
    tx_end(c);
}

/* ---- the build re-sorts splits.txt ---- */
#define SPLITS_REL "config/R5WEA4/splits.txt"
#define SPLITS_HEAD "Sections:\n\t.text       type:code align:8\n\nb.cpp:\n\t.text       start:0x80000200 end:0x80000300\n\n" \
                    "a.cpp:\n\t.text       start:0x80000100 end:0x80000200\n"
#define SPLITS_SORTED "Sections:\n\t.text       type:code align:8\n\na.cpp:\n\t.text       start:0x80000100 end:0x80000200\n\n" \
                      "b.cpp:\n\t.text       start:0x80000200 end:0x80000300"

static TxCase *splits_case(const char *rewritten) {
    TxCase *c = tx_begin();
    mlfx_lane_commit(&c->fx, SPLITS_REL, avp_str(SPLITS_HEAD), "lane splits", "lane/x", true);
    mlfx_lane_commit(&c->fx, "src/a.cpp", tx_a_edited(), NULL, "lane/x", false);
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
    c->s.score_b = avp_json_float(TA, 100.0);
    c->s.mutations = AVP_NEWN(TA, MlMutation, 1);
    c->s.mutations[0] = (MlMutation){"ninja", SPLITS_REL, avp_str(rewritten), false, true};
    c->s.nmutations = 1;
    return c;
}

AVP_TEST(update_commits_the_units_the_build_put_in_address_order) {
    TxCase *c = splits_case(SPLITS_SORTED);
    tx_run(c, NULL, NULL, "--update", NULL);
    TX_RC(c, 0);
    CHECK(c->s.mutations[0].done);
    TX_ROW(c, "ledger", "PASS");
    TX_IN(c, "split: keep units in address order");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "log", "-2", "--format=%s"),
             "review: lane/x merge (ledger, baseline)\nsplit: keep units in address order");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "status", "--porcelain"), "");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "show", "HEAD~1:" SPLITS_REL), SPLITS_SORTED);
    tx_end(c);
}

AVP_TEST(update_does_not_commit_a_splits_change_that_is_not_a_reordering) {
    TxCase *c = splits_case(SPLITS_SORTED "\n\nc.cpp:\n\t.text       start:0x80000300 end:0x80000400\n");
    tx_run(c, NULL, NULL, "--update", NULL);
    TX_RC(c, 1);
    TX_ROW(c, "ledger", "FAIL");
    TX_IN(c, "slot tree is not clean before the update");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "log", "-1", "--format=%s"), "lane work");
    tx_end(c);
}

AVP_TEST(merge_fast_forwards_checked_out_main) {
    TxCase *c = tx_begin();
    prepare(c);
    tx_run(c, NULL, NULL, "--update", "--merge", "--reviewed", NULL);
    TX_RC(c, 0);
    const char *tip = TX_GIT(c, c->fx.slot, "rev-parse", "HEAD");
    TX_EQ_CZ(TX_GIT(c, c->fx.main, "rev-parse", "main"), tip);
    TX_EQ_CZ(TX_GIT(c, c->fx.main, "status", "--porcelain", "--untracked-files=no"), ""); /* work tree followed */
    TX_IN(c, "Nothing was pushed");
    tx_end(c);
}

AVP_TEST(merge_moves_branch_not_checked_out_anywhere) {
    TxCase *c = tx_begin();
    TX_GIT(c, c->fx.main, "branch", "trunk", "main");
    TX_GIT(c, c->fx.main, "checkout", "-q", "-b", "elsewhere");
    mlfx_lane_commit(&c->fx, "src/a.cpp", tx_a_edited(), NULL, "lane/x", true);
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
    tx_run(c, NULL, "trunk", "--update", "--merge", "--reviewed", NULL);
    TX_RC(c, 0);
    TX_IN(c, "not checked out anywhere");
    TX_EQ_CZ(TX_GIT(c, c->fx.main, "rev-parse", "trunk"), TX_GIT(c, c->fx.slot, "rev-parse", "HEAD"));
    TX_EQ_CZ(TX_GIT(c, c->fx.main, "rev-parse", "main"), TX_GIT(c, c->fx.slot, "rev-parse", "main"));
    tx_end(c);
}

AVP_TEST(merge_refuses_when_onto_moved_after_rebase) {
    TxCase *c = tx_begin();
    prepare(c);
    mutate_on(c, "layout", "include/h.h", AVP_S("#define H 9\n")); /* main advances while the checks run */
    const char *before = TX_GIT(c, c->fx.main, "rev-parse", "main");
    tx_run(c, NULL, NULL, "--update", "--merge", "--reviewed", NULL);
    TX_RC(c, 1);
    TX_ROW(c, "merge", "FAIL");
    TX_IN(c, "moved during this run");
    TX_IN(c, "re-run this tool");
    const char *main_now = TX_GIT(c, c->fx.main, "rev-parse", "main");
    CHECK(strcmp(main_now, TX_GIT(c, c->fx.slot, "rev-parse", "HEAD")) != 0);
    TX_EQ_CZ(main_now, TX_GIT(c, c->fx.main, "rev-parse", "HEAD"));
    CHECK(strcmp(before, main_now) != 0); /* only the concurrent commit moved it */
    CHECK(c->s.mutations[0].done);
    tx_end(c);
}

AVP_TEST(merge_without_update_in_this_run_refused_by_arguments) {
    TxCase *c = tx_begin();
    char *argv[] = {"merge_lane", "--slot", (char *)c->fx.slot, "--branch", "lane/x", "--merge", "--reviewed", NULL};
    AvpBuf out;
    avp_buf_init(&out, TA);
    MlHooks h = mlst_hooks(&c->s, &out);
    int rc = ml_main(7, argv, &h);
    CHECK_EQ_I64(rc, 2);
    TX_NO_CALLS(c);
    tx_end(c);
}

/* ---- TestPinnedOnto --------------------------------------------------------------------------------------------- */
AVP_TEST(rebase_uses_the_pinned_commit_and_merge_stops) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
    const char *pinned = TX_GIT(c, c->fx.main, "rev-parse", "main");
    mutate_on(c, "step:rebase", "include/h.h", AVP_S("#define H 9\n")); /* main advances between preflight and rebase */
    tx_run(c, NULL, NULL, "--update", "--merge", "--reviewed", NULL);
    const char *moved = TX_GIT(c, c->fx.main, "rev-parse", "main");
    CHECK(strcmp(moved, pinned) != 0);
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "rev-parse", "HEAD~2"), pinned); /* lane work + ledger on the pinned id */
    TX_RC(c, 1);
    TX_ROW(c, "merge", "FAIL");
    TX_IN(c, "moved during this run");
    TX_EQ_CZ(TX_GIT(c, c->fx.main, "rev-parse", "main"), moved); /* nothing merged */
    tx_end(c);
}

AVP_TEST(baseline_and_rules_use_the_pinned_commit) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
    const char *pinned = TX_GIT(c, c->fx.main, "rev-parse", "main");
    const char *pinned_baseline = TX_GIT(c, c->fx.main, "show", avp_sprintf(TA, "main:%s", ML_BASELINE));
    /* main advances (with a new baseline) while the gate runs */
    mutate_on(c, "configure", ML_BASELINE, mlfx_dumps(TA, mlfx_report_f(TA, 1.0, 1.0)));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    CHECK(c->s.mutations[0].done);
    TX_EQ_CZ(c->s.seen_onto, pinned);
    AvpStr copy = avp_str_strip(tx_read(avp_path_join(TA, c->fx.slot, ML_BASELINE_COPY)));
    CHECK_MSG(avp_str_eqz(copy, pinned_baseline), "baseline copy %.*s != %s", AVP_SF(copy), pinned_baseline);
    TX_IN(c, avp_sprintf(TA, "pinned main at %s", pinned));
    tx_end(c);
}

AVP_TEST_MAIN("merge_lane_update_merge")
