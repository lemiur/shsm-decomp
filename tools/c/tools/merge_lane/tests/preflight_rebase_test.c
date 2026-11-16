/* TestPreflight and TestRebase of tools/test_merge_lane.py (a real repository, stub commands).
 *   TestPreflight.test_dirty_tree_refused                    -> dirty_tree_refused
 *   TestPreflight.test_untracked_file_counts_as_dirty        -> untracked_file_counts_as_dirty
 *   TestPreflight.test_missing_branch                        -> missing_branch
 *   TestPreflight.test_missing_onto                          -> missing_onto
 *   TestPreflight.test_slot_not_a_worktree_of_this_repo      -> slot_not_a_worktree_of_this_repo
 *   TestPreflight.test_rebase_in_progress_refused            -> rebase_in_progress_refused
 *   TestPreflight.test_lists_lane_units                      -> lists_lane_units
 *   TestRebase.test_conflict_stops_and_lists_paths           -> conflict_stops_and_lists_paths
 *   TestRebase.test_conflict_advice_for_ledger_and_generated -> conflict_advice_for_ledger_and_generated
 *   TestRebase.test_conflicting_generated_ledger_commit_is_skipped -> conflicting_generated_ledger_commit_is_skipped
 *   TestRebase.test_ledger_commit_touching_one_ledger_file_is_skipped -> ledger_commit_touching_one_ledger_file_is_skipped
 *   TestRebase.test_non_conflicting_ledger_commit_is_kept    -> non_conflicting_ledger_commit_is_kept
 *   TestRebase.test_source_conflict_before_a_ledger_commit_still_stops -> source_conflict_before_a_ledger_commit_still_stops
 *   TestRebase.test_ledger_commit_skipped_then_a_later_source_conflict_stops -> ledger_commit_skipped_then_a_later_source_conflict_stops
 *   TestRebase.test_gate_refuses_with_unmerged_paths         -> gate_refuses_with_unmerged_paths
 *   TestRebase.test_clean_rebase_onto_moved_main             -> clean_rebase_onto_moved_main
 * The fixture sits in a folder whose name holds a space ("repo dir"). */
#include "ml_test_fixtures.h"

/* ---- TestPreflight ---------------------------------------------------------------------------------------------- */
AVP_TEST(dirty_tree_refused) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    mlfx_write(c->fx.slot, "src/a.cpp", AVP_S("dirty\n"));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "not clean");
    TX_ROW(c, "preflight", "FAIL");
    TX_ROW(c, "rebase", "SKIP");
    TX_NO_CALLS(c);
    tx_end(c);
}

AVP_TEST(untracked_file_counts_as_dirty) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    mlfx_write(c->fx.slot, "stray.txt", AVP_S("x\n"));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "stray.txt");
    tx_end(c);
}

AVP_TEST(missing_branch) {
    TxCase *c = tx_begin();
    tx_run(c, "lane/nope", NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "does not exist");
    tx_end(c);
}

AVP_TEST(missing_onto) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    tx_run(c, NULL, "trunk", NULL);
    TX_RC(c, 1);
    TX_IN(c, "onto branch trunk does not exist");
    tx_end(c);
}

AVP_TEST(slot_not_a_worktree_of_this_repo) {
    TxCase *c = tx_begin();
    const char *other = avp_path_join(TA, c->tmp, "other");
    avp_makedirs(other, true);
    TX_GIT(c, other, "init", "-q", "-b", "main");
    TX_GIT(c, other, "config", "user.email", "t@example.com");
    TX_GIT(c, other, "config", "user.name", "tester");
    mlfx_write(other, "f", AVP_S("x"));
    TX_GIT(c, other, "add", "-A");
    TX_GIT(c, other, "commit", "-q", "-m", "x");
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    tx_main(c, "--slot", other, "--branch", "lane/x", "--repo", c->fx.main, "--log-dir", c->fx.logs, NULL);
    TX_RC(c, 1);
    TX_IN(c, "preflight");
    tx_end(c);
}

AVP_TEST(rebase_in_progress_refused) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    TX_MAIN(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "x + 1", "x + 3"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 1);
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL); /* slot is now mid-rebase */
    TX_RC(c, 1);
    TX_IN(c, "rebase in progress");
    tx_end(c);
}

AVP_TEST(lists_lane_units) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 0);
    TX_IN(c, "main/a  Matching (source edited)");
    tx_end(c);
}

/* ---- TestRebase ------------------------------------------------------------------------------------------------- */
AVP_TEST(conflict_stops_and_lists_paths) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    TX_MAIN(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "x + 1", "x + 3"));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_ROW(c, "rebase", "FAIL");
    const char *skipped[] = {"gate", "changes", "rules", "layout"};
    for (int i = 0; i < 4; i++) TX_ROW(c, skipped[i], "SKIP");
    TX_IN(c, "UU src/a.cpp");
    TX_IN(c, "never auto-resolves");
    TX_NO_CALLS(c); /* nothing built in the half-finished rebase */
    tx_end(c);
}

AVP_TEST(conflict_advice_for_ledger_and_generated) {
    TxCase *c = tx_begin();
    const char *slot = c->fx.slot;
    /* commits that also carry source are not generated ledger commits: they are never skipped */
    TX_LANE(c, "docs/progress.md", AVP_S("lane row\n"));
    TX_GIT(c, slot, "checkout", "-q", "lane/x");
    mlfx_write(slot, "src/b.cpp", AVP_S("int fn_C(void) {\n    return 1;\n}  // 0x80000300\n"));
    TX_GIT(c, slot, "add", "-A");
    TX_GIT(c, slot, "commit", "-q", "--amend", "--no-edit");
    mlfx_write(slot, ML_BASELINE, AVP_S("lane baseline\n"));
    mlfx_write(slot, "include/h.h", AVP_S("#define H 3\n"));
    TX_GIT(c, slot, "add", "-A");
    TX_GIT(c, slot, "commit", "-q", "-m", "lane baseline");
    TX_GIT(c, slot, "checkout", "-q", "--detach");
    TX_MAIN(c, "docs/progress.md", AVP_S("main row\n"));
    TX_MAIN(c, ML_BASELINE, AVP_S("main baseline\n"));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "score ledger");
    TX_IN(c, "ledger.exe progress");
    /* the second lane commit conflicts only after the first is resolved; the first stop is the ledger */
    TX_IN(c, "UU docs/progress.md");
    tx_end(c);
}

/* TestRebase.ledger_commit: a generated ledger commit on the lane branch (only baseline_report.json and progress.md) */
static void ledger_commit(TxCase *c) {
    const char *slot = c->fx.slot;
    TX_GIT(c, slot, "checkout", "-q", "lane/x");
    mlfx_write(slot, ML_BASELINE, AVP_S("{\"lane\": true}\n"));
    mlfx_write(slot, "docs/progress.md", AVP_S("lane ledger\n"));
    TX_GIT(c, slot, "add", "-A");
    TX_GIT(c, slot, "commit", "-q", "-m", "review: lane/x merge (ledger, baseline)");
    TX_GIT(c, slot, "checkout", "-q", "--detach");
}

static void ledger_main_moves(TxCase *c) {
    mlfx_main_commit(&c->fx, "docs/progress.md", AVP_S("main ledger\n"), "main ledger");
    mlfx_main_commit(&c->fx, ML_BASELINE, AVP_S("{\"main\": true}\n"), "main baseline");
}

AVP_TEST(conflicting_generated_ledger_commit_is_skipped) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    ledger_commit(c);
    ledger_main_moves(c);
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 0);
    TX_IN(c, "skipped the lane's generated ledger commit");
    TX_IN(c, "review: lane/x merge (ledger, baseline)");
    TX_IN(c, "1 commit(s) ahead of main");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "rev-parse", "--abbrev-ref", "HEAD"), "lane/x");
    /* the source commit stays, main's ledger files are untouched */
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "log", "--format=%s", "main..lane/x"), "lane work");
    CHECK_STR(tx_read(avp_path_joinv(TA, 3, (const char *const[]){c->fx.slot, "docs", "progress.md"})),
              "main ledger\n");
    TX_NO_CALLS(c);
    tx_end(c);
}

AVP_TEST(ledger_commit_touching_one_ledger_file_is_skipped) {
    TxCase *c = tx_begin();
    const char *slot = c->fx.slot;
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    TX_GIT(c, slot, "checkout", "-q", "lane/x");
    mlfx_write(slot, "docs/progress.md", AVP_S("lane row\n"));
    TX_GIT(c, slot, "add", "-A");
    TX_GIT(c, slot, "commit", "-q", "-m", "lane ledger only");
    TX_GIT(c, slot, "checkout", "-q", "--detach");
    TX_MAIN(c, "docs/progress.md", AVP_S("main row\n"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 0);
    TX_IN(c, "lane ledger only");
    TX_EQ_CZ(TX_GIT(c, slot, "log", "--format=%s", "main..lane/x"), "lane work");
    tx_end(c);
}

AVP_TEST(non_conflicting_ledger_commit_is_kept) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    ledger_commit(c);
    TX_MAIN(c, "include/h.h", AVP_S("#define H 2\n"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 0);
    TX_NOT_IN(c, "skipped");
    TX_IN(c, "2 commit(s) ahead of main");
    tx_end(c);
}

AVP_TEST(source_conflict_before_a_ledger_commit_still_stops) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    ledger_commit(c);
    ledger_main_moves(c);
    TX_MAIN(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "x + 1", "x + 3"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 1);
    TX_IN(c, "UU src/a.cpp");
    TX_NOT_IN(c, "skipped"); /* the source commit comes first and stops the rebase */
    TX_NO_CALLS(c);
    tx_end(c);
}

AVP_TEST(ledger_commit_skipped_then_a_later_source_conflict_stops) {
    TxCase *c = tx_begin();
    const char *slot = c->fx.slot;
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    ledger_commit(c);
    ledger_main_moves(c);
    TX_GIT(c, slot, "checkout", "-q", "lane/x");
    /* a source edit after the ledger commit that conflicts with main */
    mlfx_write(slot, "src/b.cpp", AVP_S("int fn_C(void) {\n    return 5;\n}  // 0x80000300\n"));
    TX_GIT(c, slot, "add", "-A");
    TX_GIT(c, slot, "commit", "-q", "-m", "lane b");
    TX_GIT(c, slot, "checkout", "-q", "--detach");
    TX_MAIN(c, "src/b.cpp", AVP_S("int fn_C(void) {\n    return 6;\n}  // 0x80000300\n"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 1);
    TX_IN(c, "skipped the lane's generated ledger commit");
    TX_IN(c, "UU src/b.cpp");
    TX_IN(c, "never auto-resolves");
    tx_end(c);
}

AVP_TEST(gate_refuses_with_unmerged_paths) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    TX_MAIN(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "x + 1", "x + 3"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    tx_run(c, NULL, NULL, "--steps", "gate", NULL); /* preflight sees the rebase and refuses */
    TX_RC(c, 1);
    TX_NO_CALLS(c);
    tx_end(c);
}

AVP_TEST(clean_rebase_onto_moved_main) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    TX_MAIN(c, "include/h.h", AVP_S("#define H 2\n"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 0);
    TX_IN(c, "1 commit(s) ahead of main");
    TX_EQ_CZ(TX_GIT(c, c->fx.slot, "rev-parse", "--abbrev-ref", "HEAD"), "lane/x");
    tx_end(c);
}

AVP_TEST_MAIN("merge_lane_preflight_rebase")
