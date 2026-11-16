/* TestRules, TestRulesClasses and TestRulesFailClosed of tools/test_merge_lane.py.
 *   TestRules.test_blocking_findings_fail                    -> blocking_findings_fail
 *   TestRules.test_new_finding_in_untouched_function_fails   -> new_finding_in_untouched_function_fails
 *   TestRules.test_new_finding_in_touched_function_is_not_header_breakage -> new_finding_in_touched_function_is_not_header_breakage
 *   TestRules.test_finding_present_on_both_sides_is_not_new  -> finding_present_on_both_sides_is_not_new
 *   TestRules.test_new_finding_in_untouched_file_fails       -> new_finding_in_untouched_file_fails
 *   TestRules.test_non_match_tier_new_finding_is_informational -> non_match_tier_new_finding_is_informational
 *   TestRulesClasses.test_gate_then_rules_rescans_classes_and_both_sides_read_it -> gate_then_rules_rescans_classes_and_both_sides_read_it
 *   TestRulesClasses.test_default_commands_scan_classes_and_pass_them_to_the_tree_check -> default_commands_scan_classes_and_pass_them_to_the_tree_check
 *       differs: the port has no DEFAULT_COMMANDS argv table (the class scanner and the rule checker are linked);
 *       checked through behaviour instead: the default classes_scan is the class scanner (in a root without asm it
 *       answers "classes.exe: error" with exit 2), and the default rules_tree reads the classes.json named by its
 *       {classes} field (its R13 note names that file, not the root's build/R5WEA4/classes.json)
 *   TestRulesClasses.test_classes_scan_failure_fails_rules   -> classes_scan_failure_fails_rules
 *   TestRulesClasses.test_r13_skipped_fails_closed           -> r13_skipped_fails_closed
 *   TestRulesClasses.test_r13_that_ran_passes                -> r13_that_ran_passes
 *   TestRulesFailClosed.test_unreadable_touched_file_stops_red -> unreadable_touched_file_stops_red
 *       (the patched rules_tree_cmd is MlStubs.remove_after_lane_tree)
 *   TestRulesFailClosed.test_unindexable_touched_file_stops_red -> unindexable_touched_file_stops_red
 *       (the patched Ctx.score_history is MlStubs.broken_index, the MlHooks.index hook)
 *   TestRulesFailClosed.test_tree_check_without_findings_list_fails -> tree_check_without_findings_list_fails
 *   TestRulesFailClosed.test_tree_check_exit_2_fails         -> tree_check_exit_2_fails
 *   TestRulesFailClosed.test_tree_check_that_scanned_nothing_fails -> tree_check_that_scanned_nothing_fails
 *   TestRulesFailClosed.test_changed_check_without_blocking_list_fails -> changed_check_without_blocking_list_fails
 *   TestRulesFailClosed.test_failed_git_diff_stops_red       -> failed_git_diff_stops_red
 *       (the patched Ctx.git is MlStubs.fail_git_diff_head, the MlHooks.git hook) */
#include "ml_test_fixtures.h"

#define FINDING(path, line, rule, tier, msg, fn)                                                                   \
    "{\"path\": \"" path "\", \"line\": " #line ", \"rule\": \"" rule "\", \"tier\": \"" tier "\", \"message\": \"" \
    msg "\", \"function\": \"" fn "\"}"

/* TestRulesFailClosed.FB */
#define FB FINDING("src/a.cpp", 7, "R4", "match", "cast", "fn_B")

/* ---- TestRules -------------------------------------------------------------------------------------------------- */
AVP_TEST(blocking_findings_fail) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
#define F17 FINDING("src/a.cpp", 3, "R17", "match", "fake", "fn_A")
    c->s.rules_changed = tx_json("{\"scanned\": [\"src/a.cpp\"], \"findings\": [" F17 "], \"blocking\": [" F17
                                 "], \"functions\": [], \"exit\": 1}");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "BLOCKING [match] findings");
    TX_IN(c, "src/a.cpp:3: R17");
    TX_ROW(c, "layout", "SKIP");
    tx_end(c);
}

AVP_TEST(new_finding_in_untouched_function_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited()); /* fn_A edited, fn_B not */
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": [" FB "]}}");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "shared-header breakage");
    TX_IN(c, "(in fn_B)");
    tx_end(c);
}

AVP_TEST(new_finding_in_touched_function_is_not_header_breakage) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": ["
                              FINDING("src/a.cpp", 3, "R4", "match", "cast", "fn_A") "]}}");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "1 in functions or files the lane touched");
    tx_end(c);
}

AVP_TEST(finding_present_on_both_sides_is_not_new) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    /* fb2: moved, same finding */
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": [" FB "]}, \"lane\": {\"findings\": ["
                              FINDING("src/a.cpp", 8, "R4", "match", "cast", "fn_B") "]}}");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    tx_end(c);
}

AVP_TEST(new_finding_in_untouched_file_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "include/h.h", AVP_S("#define H 3\n"));
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": ["
                              FINDING("src/b.cpp", 1, "R2", "match", "x", "fn_C") "]}}");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "src/b.cpp:1: R2");
    tx_end(c);
}

AVP_TEST(non_match_tier_new_finding_is_informational) {
    TxCase *c = tx_begin();
    TX_LANE(c, "include/h.h", AVP_S("#define H 3\n"));
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": ["
                              FINDING("src/b.cpp", 1, "R8", "free", "x", "fn_C") "]}}");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "informational");
    tx_end(c);
}

/* ---- TestRulesClasses: the gate step deletes build/ and with it classes.json: rules rescans it, or R13 is silently
 * skipped -------------------------------------------------------------------------------------------------------- */
AVP_TEST(gate_then_rules_rescans_classes_and_both_sides_read_it) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    mlfx_write(c->fx.slot, ML_CLASSES_JSON, AVP_S("{}")); /* a classes.json from before the gate: deleted with build/ */
    tx_run(c, NULL, NULL, "--steps", "rebase,gate,rules", NULL);
    TX_RC(c, 0);
    TX_CALLS(c, "configure", "ninja", "classes_scan", "classes_scan", "rules_changed", "rules_tree", "rules_tree");
    const char *want = avp_path_join(TA, c->fx.slot, ML_CLASSES_JSON);
    REQUIRE(c->s.tree_classes.n == 2 && c->s.tree_classes_exist.n == 2);
    for (size_t i = 0; i < 2; i++) {
        const char *got = avp_cz(TA, c->s.tree_classes.v[i]);
        CHECK_MSG(ml_norm_eq(TA, got, want), "classes %s != %s", got, want);
        CHECK(c->s.tree_classes_exist.v[i]);
    }
    TX_IN(c, "classes.exe scan: exit 0");
    tx_end(c);
}

static Ml *ctx_for(const char *root) {
    Ml *ml = AVP_NEW(TA, Ml);
    ml->a = TA;
    ml->slot = root, ml->branch = "b", ml->onto = "main", ml->repo = root;
    ml->log_dir = avp_path_join(TA, root, "logs");
    ml->passed = avp_set_new();
    return ml;
}

AVP_TEST(default_commands_scan_classes_and_pass_them_to_the_tree_check) {
    const char *tmp = avp_mkdtemp(TA, "merge_lane_test_", "", NULL);
    const char *root = avp_path_join(TA, tmp, "repo dir");
    mlfx_write(root, "src/a.cpp", avp_str(MLFX_A_SRC));
    mlfx_write(root, "configure.py", avp_str(MLFX_CONFIGURE));
    Ml *ml = ctx_for(root);
    AvpStr text = AVP_S("");
    i64 rc = ml_run_cmd(ml, "classes_scan", "classes.log", NULL, root, &text);
    CHECK_EQ_I64(rc, 2);
    CHECK_MSG(tx_has(text, "classes.exe: error"), "classes_scan: %.*s", AVP_SF(text));
    const char *classes = avp_path_joinv(TA, 3, (const char *const[]){tmp, "elsewhere", "classes.json"});
    MlExtra ex = {root, classes};
    text = AVP_S("");
    rc = ml_run_cmd(ml, "rules_tree", "tree.log", &ex, root, &text);
    const char *err = NULL;
    AvpJson *doc = ml_parse_json_text(TA, text, &err);
    CHECK_MSG(doc != NULL, "rules_tree (exit %lld) printed no JSON: %s\n%.*s", (long long)rc, err ? err : "",
              AVP_SF(text));
    if (doc) {
        const AvpJson *r13 = avp_json_getz(doc, "r13");
        const AvpJson *note = r13 ? avp_json_getz(r13, "note") : NULL;
        CHECK_MSG(note && avp_json_is_str(note) && tx_has(note->u.str, classes), "r13 does not name %s:\n%.*s",
                  classes, AVP_SF(text));
    }
    avp_rmtree(tmp, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST(classes_scan_failure_fails_rules) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.classes_rc = 1;
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "classes.exe scan exited 1");
    CHECK(!mlst_called(&c->s, "rules_changed"));
    tx_end(c);
}

AVP_TEST(r13_skipped_fails_closed) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json(
        "{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": [], \"r13\": {\"note\": \"R13 slot checks skipped: "
        "build/R5WEA4/classes.json not found (run build/tools/classes.exe scan)\"}}}");
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "R13 slot checks skipped");
    tx_end(c);
}

AVP_TEST(r13_that_ran_passes) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": [], \"r13\": {\"note\": null, \"classes\": 3}}, "
                              "\"lane\": {\"findings\": [], \"r13\": {\"note\": null, \"classes\": 3}}}");
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 0);
    tx_end(c);
}

/* ---- TestRulesFailClosed ---------------------------------------------------------------------------------------- */
AVP_TEST(unreadable_touched_file_stops_red) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": [" FB "]}}");
    c->s.remove_after_lane_tree = "src/a.cpp"; /* the file can no longer be read */
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_ROW(c, "rules", "FAIL");
    TX_IN(c, "cannot read src/a.cpp");
    tx_end(c);
}

AVP_TEST(unindexable_touched_file_stops_red) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": [" FB "]}}");
    c->s.broken_index = true;
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "cannot index the functions of src/a.cpp");
    tx_end(c);
}

AVP_TEST(tree_check_without_findings_list_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"error\": \"crashed\"}}");
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "lacks findings");
    tx_end(c);
}

AVP_TEST(tree_check_exit_2_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree_rc = 2;
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "exited 2");
    tx_end(c);
}

AVP_TEST(tree_check_that_scanned_nothing_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_tree = tx_json("{\"onto\": {\"findings\": []}, \"lane\": {\"findings\": [], \"scanned\": []}}");
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "scanned no file");
    tx_end(c);
}

AVP_TEST(changed_check_without_blocking_list_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.rules_changed = tx_json("{\"scanned\": [\"src/a.cpp\"], \"findings\": []}");
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "lacks blocking");
    tx_end(c);
}

AVP_TEST(failed_git_diff_stops_red) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.fail_git_diff_head = true;
    tx_run(c, NULL, NULL, "--steps", "rebase,rules", NULL);
    TX_RC(c, 1);
    TX_IN(c, "simulated");
    tx_end(c);
}

AVP_TEST_MAIN("merge_lane_rules")
