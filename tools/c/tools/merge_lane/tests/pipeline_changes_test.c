/* TestPipeline, TestChanges and TestChangesOutput of tools/test_merge_lane.py.
 *   TestPipeline.test_order_and_full_pass                    -> order_and_full_pass
 *   TestPipeline.test_gate_failure_stops_and_attributes      -> gate_failure_stops_and_attributes
 *   TestPipeline.test_gate_without_success_line_is_red_even_with_exit_zero -> gate_without_success_line_is_red_even_with_exit_zero
 *   TestPipeline.test_gate_deletes_only_the_slot_build       -> gate_deletes_only_the_slot_build
 *   TestPipeline.test_gate_refuses_main_checkout             -> gate_refuses_main_checkout
 *   TestPipeline.test_steps_subset_skips_the_rest            -> steps_subset_skips_the_rest
 *   TestPipeline.test_layout_failure                         -> layout_failure
 *   TestChanges.test_edited_function_that_fell_blocks        -> edited_function_that_fell_blocks
 *   TestChanges.test_unchanged_function_dip_is_informational -> unchanged_function_dip_is_informational
 *   TestChanges.test_layout_only_edit_does_not_count_as_a_function_edit -> layout_only_edit_does_not_count_as_a_function_edit
 *   TestChanges.test_header_change_labels_unchanged_dip      -> header_change_labels_unchanged_dip
 *   TestChanges.test_function_missing_from_lane_report_blocks -> function_missing_from_lane_report_blocks
 *   TestChanges.test_rename_without_score_loss_is_not_a_decrease -> rename_without_score_loss_is_not_a_decrease
 *   TestChanges.test_renamed_auto_unit_is_not_a_regression   -> renamed_auto_unit_is_not_a_regression
 *   TestChanges.test_function_moved_into_a_source_unit_keeps_its_score -> function_moved_into_a_source_unit_keeps_its_score
 *   TestChanges.test_score_lost_across_a_unit_rename_blocks  -> score_lost_across_a_unit_rename_blocks
 *   TestChanges.test_function_in_two_lane_units_takes_the_best_score -> function_in_two_lane_units_takes_the_best_score
 *   TestChanges.test_vanished_function_with_nonzero_baseline_blocks_but_zero_does_not -> vanished_function_with_nonzero_baseline_blocks_but_zero_does_not
 *   TestChanges.test_improvements_form_the_review_sample     -> improvements_form_the_review_sample
 *   TestChanges.test_baseline_comes_from_onto_not_the_lane   -> baseline_comes_from_onto_not_the_lane
 *   TestChangesOutput.test_parse_skips_headers_separators_and_ninja_lines -> parse_skips_headers_separators_and_ninja_lines
 *       (parse_changes_text and is_unit_row are gate_regress's gate_parse_changes_text and gate_is_unit_row)
 *   TestChangesOutput.test_improvements_and_unit_rows_are_not_counted_as_regressions -> improvements_and_unit_rows_are_not_counted_as_regressions
 *   TestChangesOutput.test_printed_table_disagreeing_with_the_json_fails -> printed_table_disagreeing_with_the_json_fails
 *   TestChangesOutput.test_stale_changes_json_is_not_read    -> stale_changes_json_is_not_read
 *   TestChangesOutput.test_comparison_that_did_not_run_fails -> comparison_that_did_not_run_fails
 *   TestChangesOutput.test_comparison_without_units_means_no_function_changed -> comparison_without_units_means_no_function_changed
 *   TestChangesOutput.test_comparison_without_units_but_printed_regressions_disagrees -> comparison_without_units_but_printed_regressions_disagrees
 * The Python tests that patch Stubs.ninja to rewrite the lane report set TxCase.ninja_report (the same rewrite after
 * the stub's own). */
#include "ml_test_fixtures.h"

/* ---- TestPipeline ----------------------------------------------------------------------------------------------- */
AVP_TEST(order_and_full_pass) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_CALLS(c, "configure", "ninja", "classes_scan", "ninja_changes", "classes_scan", "rules_changed", "rules_tree", "rules_tree",
             "layout");
    const char *steps[] = {"preflight", "rebase", "gate", "changes", "rules", "layout"};
    for (int i = 0; i < 6; i++) TX_ROW(c, steps[i], "PASS");
    TX_ROW(c, "ledger", "SKIP");
    TX_IN(c, "build/R5WEA4/main.dol: OK");
    TX_IN(c, "REMINDER: the reader-test review");
    TX_IN(c, "full log: ");
    /* the whole output is also in <logs>/merge.log, outside the slot's build/ */
    AvpStr full = tx_read(avp_path_join(TA, c->fx.logs, "merge.log"));
    CHECK(avp_str_contains(full, AVP_S("== gate ==")) && avp_str_contains(full, AVP_S("== summary ==")));
    tx_end(c);
}

AVP_TEST(gate_keeps_the_c_tools) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    mlfx_write(c->fx.slot, "build/tools/configure.exe", AVP_S("x"));
    mlfx_write(c->fx.slot, "build/tools-cmake/CMakeCache.txt", AVP_S("x"));
    mlfx_write(c->fx.slot, "build/index/sibling/g/sibling.sqlite", AVP_S("x"));
    mlfx_write(c->fx.slot, "build/binutils/powerpc-eabi-objdump.exe", AVP_S("x"));
    mlfx_write(c->fx.slot, "build/R5WEA4/stale.o", AVP_S("x"));
    tx_run(c, NULL, NULL, "--steps", "gate", NULL);
    TX_ROW(c, "gate", "PASS");
    CHECK(avp_isfile(avp_path_join(avp_scratch(), c->fx.slot, "build/tools/configure.exe")));
    CHECK(avp_isfile(avp_path_join(avp_scratch(), c->fx.slot, "build/tools-cmake/CMakeCache.txt")));
    CHECK(avp_isfile(avp_path_join(avp_scratch(), c->fx.slot, "build/index/sibling/g/sibling.sqlite")));
    CHECK(avp_isfile(avp_path_join(avp_scratch(), c->fx.slot, "build/binutils/powerpc-eabi-objdump.exe")));
    CHECK(!avp_exists(avp_path_join(avp_scratch(), c->fx.slot, "build/R5WEA4/stale.o")));
    tx_end(c);
}

AVP_TEST(gate_failure_stops_and_attributes) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.gate_ok = false;
    c->s.gate_rc = 1;
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_ROW(c, "gate", "FAIL");
    TX_ROW(c, "changes", "SKIP");
    CHECK(mlst_called(&c->s, "attribute"));
    TX_IN(c, "symbol fn_A differs");
    TX_IN(c, "RED: ninja exit 1, no `main.dol: OK` line");
    CHECK(!mlst_called(&c->s, "ninja_changes"));
    TX_NOT_IN(c, "gate line:");
    tx_end(c);
}

AVP_TEST(gate_without_success_line_is_red_even_with_exit_zero) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.gate_ok = false;
    c->s.gate_rc = 0;
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "success line `main.dol: OK` was not printed");
    tx_end(c);
}

AVP_TEST(gate_deletes_only_the_slot_build) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    mlfx_write(c->fx.slot, "build/old.txt", AVP_S("x"));
    mlfx_write(c->fx.main, "build/keep.txt", AVP_S("x"));
    tx_run(c, NULL, NULL, "--steps", "rebase,gate", NULL);
    CHECK(!avp_exists(avp_path_joinv(TA, 3, (const char *const[]){c->fx.slot, "build", "old.txt"})));
    CHECK(avp_exists(avp_path_joinv(TA, 3, (const char *const[]){c->fx.main, "build", "keep.txt"})));
    tx_end(c);
}

AVP_TEST(gate_refuses_main_checkout) {
    TxCase *c = tx_begin();
    TX_GIT(c, c->fx.main, "checkout", "-q", "-b", "lane/y", "main");
    /* the main checkout holds main; lane/y is checked out in it, onto==main is not the same branch */
    tx_main(c, "--slot", c->fx.main, "--branch", "lane/y", "--onto", "main", "--repo", c->fx.main, "--log-dir",
            c->fx.logs, "--steps", "gate", NULL);
    TX_RC(c, 1);
    TX_IN(c, "main checkout");
    TX_NO_CALLS(c);
    tx_end(c);
}

AVP_TEST(steps_subset_skips_the_rest) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    tx_run(c, NULL, NULL, "--steps", "rebase,layout", NULL);
    TX_RC(c, 0);
    TX_ROW(c, "gate", "SKIP");
    TX_CALLS(c, "layout");
    tx_end(c);
}

AVP_TEST(layout_failure) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_lane());
    c->s.layout_rc = 1;
    tx_run(c, NULL, NULL, "--steps", "rebase,gate,changes,rules,layout", NULL);
    TX_RC(c, 1);
    TX_ROW(c, "layout", "FAIL");
    tx_end(c);
}

/* ---- TestChanges ------------------------------------------------------------------------------------------------ */
AVP_TEST(edited_function_that_fell_blocks) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited()); /* fn_A edited */
    c->s.score_a = avp_json_float(TA, 90.0);
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 90.0]]");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_ROW(c, "changes", "FAIL");
    TX_ROW(c, "rules", "SKIP");
    TX_IN(c, "BLOCKING: edited function");
    TX_IN(c, "function source changed");
    tx_end(c);
}

AVP_TEST(unchanged_function_dip_is_informational) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited()); /* edits fn_A only */
    c->s.score_b = avp_json_float(TA, 95.0);
    c->s.changes_rows = tx_json("[[\"fn_B\", " B_ADDR_S ", 100.0, 95.0]]");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 1 unchanged");
    TX_IN(c, "function source identical");
    TX_IN(c, "rest of the file changed");
    tx_end(c);
}

AVP_TEST(layout_only_edit_does_not_count_as_a_function_edit) {
    TxCase *c = tx_begin();
    /* comments and whitespace do not change the token hash */
    TX_LANE(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "    return x + 1;", "    return   x + 1;  // note"));
    c->s.score_a = avp_json_float(TA, 90.0);
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 90.0]]");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 1 unchanged");
    tx_end(c);
}

AVP_TEST(header_change_labels_unchanged_dip) {
    TxCase *c = tx_begin();
    TX_LANE(c, "include/h.h", AVP_S("#define H 3\n"));
    c->s.score_a = avp_json_float(TA, 90.0);
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 90.0]]");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "include/ or configure.py changed");
    tx_end(c);
}

AVP_TEST(function_missing_from_lane_report_blocks) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, null]]");
    /* the lane report still lists fn_A at that address under another unit: remove it */
    c->ninja_report = "{\"units\": [{\"name\": \"main/a\", \"functions\": [], \"metadata\": {\"source_path\": \"src/a.cpp\"}}]}";
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "not in the lane's report");
    tx_end(c);
}

AVP_TEST(rename_without_score_loss_is_not_a_decrease) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "fn_A", "fn_A2"));
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, null], [\"fn_A2\", " A_ADDR_S ", null, 100.0]]");
    AvpJson *rep = mlfx_report_f(TA, 100.0, 100.0);
    AvpJson *f0 = avp_json_at(avp_json_getz(avp_json_at(avp_json_getz(rep, "units"), 0), "functions"), 0);
    avp_json_set(TA, f0, AVP_S("name"), avp_json_strz(TA, "fn_A2"));
    c->ninja_report = avp_cz(TA, mlfx_dumps(TA, rep));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 0 unchanged");
    tx_end(c);
}

/* TestChanges._fn(name, addr, f=None, t=None); f, t JSON numbers or NULL */
static const char *fn_(const char *name, const char *addr, const char *f, const char *t) {
    AvpBuf b;
    avp_buf_init(&b, TA);
    avp_buf_printf(&b, "{\"name\": \"%s\", \"metadata\": {\"virtual_address\": \"%s\"}", name, addr);
    if (f) avp_buf_printf(&b, ", \"from\": {\"fuzzy_match_percent\": %s}", f);
    if (t) avp_buf_printf(&b, ", \"to\": {\"fuzzy_match_percent\": %s}", t);
    avp_buf_putz(&b, "}");
    return avp_cz(TA, avp_buf_str(&b));
}

/* TestChanges._lane_fn(name, addr, score) */
static const char *lane_fn(const char *name, const char *addr, const char *score) {
    return avp_sprintf(TA, "{\"name\": \"%s\", \"size\": \"8\", \"fuzzy_match_percent\": %s, "
                           "\"metadata\": {\"virtual_address\": \"%s\"}}",
                       name, score, addr);
}

/* TestChanges._split_rename: the baseline functions sit in an auto unit (base_functions, a JSON list of report
 * functions); the lane report and the changes JSON carry other units (JSON lists). */
static void split_rename(TxCase *c, const char *base_functions, const char *lane_units, const char *change_units) {
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    const char *base = avp_sprintf(TA, "{\"units\": [{\"name\": \"main/auto_03_80000000_text\", "
                                       "\"metadata\": {\"auto_generated\": true}, \"functions\": %s}]}",
                                   base_functions);
    mlfx_write(c->fx.main, ML_BASELINE, avp_str(base));
    TX_GIT(c, c->fx.main, "add", "-A");
    TX_GIT(c, c->fx.main, "commit", "-q", "-m", "auto baseline");
    c->ninja_report = avp_sprintf(TA, "{\"units\": %s}", lane_units);
    c->s.changes_doc = tx_json(avp_sprintf(TA, "{\"from\": {}, \"to\": {}, \"units\": %s}", change_units));
    c->s.changes_rows = avp_json_arr(TA);
}

#define AUTO_OLD "main/auto_03_80000000_text"
#define AUTO_NEW "main/auto_03_80000100_text"

AVP_TEST(renamed_auto_unit_is_not_a_regression) {
    /* the lane's split renames the auto unit: the baseline functions are "from"-only in the old unit and "to"-only in
     * the new ones, with the same scores */
    TxCase *c = tx_begin();
    split_rename(c,
                 avp_sprintf(TA, "[%s, %s]", lane_fn("fn_A", A_ADDR_S, "100.0"), lane_fn("fn_B", B_ADDR_S, "60.0")),
                 avp_sprintf(TA, "[{\"name\": \"" AUTO_NEW "\", \"metadata\": {\"auto_generated\": true}, "
                                 "\"functions\": [%s, %s]}]",
                             lane_fn("fn_A", A_ADDR_S, "100.0"), lane_fn("fn_B", B_ADDR_S, "60.0")),
                 avp_sprintf(TA, "[{\"name\": \"" AUTO_OLD "\", \"functions\": [%s, %s]}, "
                                 "{\"name\": \"" AUTO_NEW "\", \"functions\": [%s, %s]}]",
                             fn_("fn_A", A_ADDR_S, "100.0", NULL), fn_("fn_B", B_ADDR_S, "60.0", NULL),
                             fn_("fn_A", A_ADDR_S, NULL, "100.0"), fn_("fn_B", B_ADDR_S, NULL, "60.0")));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 0 unchanged");
    tx_end(c);
}

AVP_TEST(function_moved_into_a_source_unit_keeps_its_score) {
    /* auto unit -> source unit: improved, not gone */
    TxCase *c = tx_begin();
    split_rename(c, avp_sprintf(TA, "[%s]", lane_fn("fn_A", A_ADDR_S, "40.0")),
                 avp_sprintf(TA, "[{\"name\": \"main/a\", \"metadata\": {\"source_path\": \"src/a.cpp\"}, "
                                 "\"functions\": [%s]}]",
                             lane_fn("fn_A", A_ADDR_S, "100.0")),
                 avp_sprintf(TA, "[{\"name\": \"" AUTO_OLD "\", \"functions\": [%s]}, "
                                 "{\"name\": \"main/a\", \"functions\": [%s]}]",
                             fn_("fn_A", A_ADDR_S, "40.0", NULL), fn_("fn_A", A_ADDR_S, NULL, "100.0")));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "1 new or improved functions (1 at 100%)");
    tx_end(c);
}

AVP_TEST(score_lost_across_a_unit_rename_blocks) {
    TxCase *c = tx_begin();
    split_rename(c, avp_sprintf(TA, "[%s]", lane_fn("fn_A", A_ADDR_S, "100.0")),
                 avp_sprintf(TA, "[{\"name\": \"" AUTO_NEW "\", \"metadata\": {\"auto_generated\": true}, "
                                 "\"functions\": [%s]}]",
                             lane_fn("fn_A", A_ADDR_S, "70.0")),
                 avp_sprintf(TA, "[{\"name\": \"" AUTO_OLD "\", \"functions\": [%s]}, "
                                 "{\"name\": \"" AUTO_NEW "\", \"functions\": [%s]}]",
                             fn_("fn_A", A_ADDR_S, "100.0", NULL), fn_("fn_A", A_ADDR_S, NULL, "70.0")));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "BLOCKING: edited function");
    tx_end(c);
}

AVP_TEST(function_in_two_lane_units_takes_the_best_score) {
    /* a split unit plus the old auto unit both list the address; the better score counts */
    TxCase *c = tx_begin();
    split_rename(c, avp_sprintf(TA, "[%s]", lane_fn("fn_A", A_ADDR_S, "90.0")),
                 avp_sprintf(TA, "[{\"name\": \"main/a\", \"metadata\": {\"source_path\": \"src/a.cpp\"}, "
                                 "\"functions\": [%s]}, "
                                 "{\"name\": \"" AUTO_OLD "\", \"metadata\": {\"auto_generated\": true}, "
                                 "\"functions\": [%s]}]",
                             lane_fn("fn_A", A_ADDR_S, "90.0"), lane_fn("fn_A", A_ADDR_S, "0.0")),
                 avp_sprintf(TA, "[{\"name\": \"main/a\", \"functions\": [%s]}, "
                                 "{\"name\": \"" AUTO_OLD "\", \"functions\": [%s]}]",
                             fn_("fn_A", A_ADDR_S, NULL, "90.0"), fn_("fn_A", A_ADDR_S, "90.0", "0.0")));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 0 unchanged");
    tx_end(c);
}

AVP_TEST(vanished_function_with_nonzero_baseline_blocks_but_zero_does_not) {
    TxCase *c = tx_begin();
    split_rename(c, avp_sprintf(TA, "[%s, %s]", lane_fn("fn_A", A_ADDR_S, "35.0"), lane_fn("fn_B", B_ADDR_S, "0.0")),
                 "[{\"name\": \"" AUTO_NEW "\", \"metadata\": {\"auto_generated\": true}, \"functions\": []}]",
                 avp_sprintf(TA, "[{\"name\": \"" AUTO_OLD "\", \"functions\": [%s, %s]}]",
                             fn_("fn_A", A_ADDR_S, "35.0", NULL), fn_("fn_B", B_ADDR_S, "0.0", NULL)));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "not in the lane's report");
    AvpStr blocking = tx_split1(c->text, "BLOCKING");
    CHECK_MSG(tx_has(blocking, "fn_A"), "fn_A not after BLOCKING: %.*s", AVP_SF(c->text));
    CHECK_MSG(!tx_has(blocking, "fn_B"), "fn_B after BLOCKING: %.*s", AVP_SF(c->text));
    tx_end(c);
}

AVP_TEST(improvements_form_the_review_sample) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 40.0, 100.0]]");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "review sample (reader test, procedure step 5): 1 new or improved functions (1 at 100%)");
    AvpStr sample = tx_read(avp_path_join(TA, c->fx.logs, "review-sample.txt"));
    CHECK_MSG(tx_has(sample, "main/a  fn_A"), "review-sample.txt: %.*s", AVP_SF(sample));
    tx_end(c);
}

AVP_TEST(baseline_comes_from_onto_not_the_lane) {
    /* the lane changes its own baseline file; the step must still use main's */
    TxCase *c = tx_begin();
    TX_LANE(c, ML_BASELINE, AVP_S("{\"units\": []}"));
    c->s.changes_rows = avp_json_arr(TA);
    tx_run(c, NULL, NULL, "--steps", "rebase,gate,changes", NULL);
    TX_RC(c, 0);
    AvpJsonError e;
    AvpJson *doc = avp_json_parse(TA, tx_read(avp_path_join(TA, c->fx.slot, ML_BASELINE_COPY)), &e);
    REQUIRE(doc);
    const AvpJson *u0 = avp_json_at(avp_json_getz(doc, "units"), 0);
    REQUIRE(u0);
    const AvpJson *name = avp_json_getz(u0, "name");
    REQUIRE(name && avp_json_is_str(name));
    CHECK_STR(name->u.str, "main/a");
    tx_end(c);
}

/* ---- a function that left its unit by a resplit ----------------------------------------------------------------- */
#define SPLITS_HEAD "Sections:\n\t.text       type:code align:4\n\na.cpp:\n"
#define SPLITS_WIDE SPLITS_HEAD "\t.text       start:0x80000100 end:0x80000210\n"
#define SPLITS_SHRUNK SPLITS_HEAD "\t.text       start:0x80000100 end:0x80000200\n"

/* main holds splits.txt (unit a covers fn_A and fn_B), symbols.txt and a baseline with fn_B at b_score; the lane
 * shrinks the unit to fn_A alone (fn_B is then linked from the retail asm in an auto unit), edits src/a.cpp and has
 * fn_A at a_score. */
static void resplit_case(TxCase *c, const char *a_score, const char *b_score) {
    mlfx_main_commit(&c->fx, "config/R5WEA4/splits.txt", AVP_S(SPLITS_WIDE), NULL);
    mlfx_main_commit(&c->fx, "config/R5WEA4/symbols.txt",
            AVP_S("fn_A = .text:0x80000100; // type:function size:0x8\n"
                  "fn_B = .text:0x80000200; // type:function size:0x8\n"),
            NULL);
    mlfx_main_commit(&c->fx, ML_BASELINE,
            avp_str(avp_sprintf(TA, "{\"units\": [{\"name\": \"main/a\", \"metadata\": {\"source_path\": \"src/a.cpp\"}, "
                                    "\"functions\": [%s, %s]}]}",
                                lane_fn("fn_A", A_ADDR_S, "100.0"), lane_fn("fn_B", B_ADDR_S, b_score))),
            NULL);
    /* the resplit also takes the stub out of the source */
    TX_LANE(c, "src/a.cpp",
            tx_replace(avp_cz(TA, tx_a_edited()), "\nint fn_B(int x) {\n    return x * 2;\n}  // 0x80000200\n", ""));
    mlfx_lane_commit(&c->fx, "config/R5WEA4/splits.txt", AVP_S(SPLITS_SHRUNK), NULL, NULL, false);
    c->ninja_report = avp_sprintf(
        TA, "{\"units\": [{\"name\": \"main/a\", \"metadata\": {\"source_path\": \"src/a.cpp\"}, \"functions\": [%s]}, "
            "{\"name\": \"" AUTO_NEW "\", \"metadata\": {\"auto_generated\": true}, \"functions\": [%s]}]}",
        lane_fn("fn_A", A_ADDR_S, a_score), lane_fn("fn_B", B_ADDR_S, "0.0"));
    c->s.changes_doc = tx_json(avp_sprintf(
        TA, "{\"from\": {}, \"to\": {}, \"units\": [{\"name\": \"main/a\", \"functions\": [%s, %s]}, "
            "{\"name\": \"" AUTO_NEW "\", \"functions\": [%s]}]}",
        fn_("fn_A", A_ADDR_S, "100.0", a_score), fn_("fn_B", B_ADDR_S, b_score, NULL),
        fn_("fn_B", B_ADDR_S, NULL, "0.0")));
    c->s.changes_rows = avp_json_arr(TA);
}

AVP_TEST(function_that_left_its_unit_by_a_resplit_is_not_a_regression) {
    TxCase *c = tx_begin();
    resplit_case(c, "100.0", "20.0");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 0 unchanged, 1 left their unit");
    TX_IN(c, "left the unit (range shrunk by the lane; not a regression):");
    TX_IN(c, "fn_B");
    TX_IN(c, ".text 0x80000100-0x80000210 -> 0x80000100-0x80000200");
    TX_NOT_IN(c, "BLOCKING");
    tx_end(c);
}

AVP_TEST(function_inside_the_shrunk_range_that_fell_still_blocks) {
    TxCase *c = tx_begin();
    resplit_case(c, "90.0", "20.0");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "1 edited by the lane, 0 unchanged, 1 left their unit");
    AvpStr blocking = tx_split1(c->text, "BLOCKING");
    CHECK_MSG(tx_has(blocking, "fn_A"), "fn_A not after BLOCKING: %.*s", AVP_SF(c->text));
    CHECK_MSG(!tx_has(blocking, "fn_B"), "fn_B after BLOCKING: %.*s", AVP_SF(c->text));
    tx_end(c);
}

AVP_TEST(exact_function_that_left_its_unit_still_blocks) {
    TxCase *c = tx_begin();
    resplit_case(c, "100.0", "100.0");
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_NOT_IN(c, "left the unit (range shrunk");
    AvpStr blocking = tx_split1(c->text, "BLOCKING");
    CHECK_MSG(tx_has(blocking, "fn_B"), "fn_B not after BLOCKING: %.*s", AVP_SF(c->text));
    tx_end(c);
}

/* ---- a placeholder the lane removed ----------------------------------------------------------------------------- */
#define A_STUB_SRC                                                                                                 \
    "// banner\nint fn_A(int x) {\n    return x + 1;\n}  // 0x80000100\n\n"                                       \
    "int fn_B(int x) {\n    // TODO\n}  // 0x80000200\n"
#define A_NO_B_SRC "// banner\nint fn_A(int x) {\n    return x + 1;\n}  // 0x80000100\n"

/* main defines fn_B as base_b_src says and carries fn_B at 5% in the baseline; the lane removes the definition. */
static void removed_case(TxCase *c, const char *base_src) {
    mlfx_main_commit(&c->fx, "src/a.cpp", avp_str(base_src), NULL);
    mlfx_main_commit(&c->fx, ML_BASELINE,
            avp_str(avp_sprintf(TA, "{\"units\": [{\"name\": \"main/a\", \"metadata\": {\"source_path\": \"src/a.cpp\"}, "
                                    "\"functions\": [%s, %s]}]}",
                                lane_fn("fn_A", A_ADDR_S, "100.0"), lane_fn("fn_B", B_ADDR_S, "5.0"))),
            NULL);
    TX_LANE(c, "src/a.cpp", AVP_S(A_NO_B_SRC));
    c->ninja_report = avp_sprintf(
        TA, "{\"units\": [{\"name\": \"main/a\", \"metadata\": {\"source_path\": \"src/a.cpp\"}, \"functions\": [%s]}]}",
        lane_fn("fn_A", A_ADDR_S, "100.0"));
    c->s.changes_doc = tx_json(avp_sprintf(TA,
                                           "{\"from\": {}, \"to\": {}, \"units\": [{\"name\": \"main/a\", "
                                           "\"functions\": [%s]}]}",
                                           fn_("fn_B", B_ADDR_S, "5.0", NULL)));
    c->s.changes_rows = avp_json_arr(TA);
}

AVP_TEST(removed_placeholder_is_not_a_regression) {
    TxCase *c = tx_begin();
    removed_case(c, A_STUB_SRC);
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "0 edited by the lane, 0 unchanged, 0 left their unit, 1 placeholder(s) removed");
    TX_IN(c, "placeholder removed (function not yet written; not a regression):");
    TX_IN(c, "fn_B");
    TX_NOT_IN(c, "BLOCKING");
    tx_end(c);
}

AVP_TEST(removed_function_of_real_code_still_blocks) {
    TxCase *c = tx_begin();
    removed_case(c, avp_cz(TA, tx_replace(MLFX_A_SRC, "x * 2", "x * 3")));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_NOT_IN(c, "placeholder removed (function");
    AvpStr blocking = tx_split1(c->text, "BLOCKING");
    CHECK_MSG(tx_has(blocking, "fn_B"), "fn_B not after BLOCKING: %.*s", AVP_SF(c->text));
    tx_end(c);
}

/* ---- TestChangesOutput ------------------------------------------------------------------------------------------ */
static const char *const REALISTIC = "$ tools/bin/ninja.exe changes\n"
                                     "[1/2] REPORT_CHANGES build/R5WEA4/report_changes.json\n"
                                     "[2/2] CHANGES_FMT changes\n"
                                     "<details>\n"
                                     "<summary>Detected 2 regressions compared to the base:</summary>\n"
                                     "\n"
                                     "| Name | Type | Before | After |\n"
                                     "| ---- | ---- | ------ | ----- |\n"
                                     "| `fn_B` | Fuzzy match | 100.00% | 95.00% |\n"
                                     "</details>\n"
                                     "       Total | matched_code |  42.00% ->  41.00%\n"
                                     "      main/a | fuzzy_match  | 100.00% ->  97.50%\n"
                                     "        fn_B | fuzzy_match  | 100.00% ->  95.00%\n"
                                     "        fn_A | fuzzy_match  |  80.00% ->  99.99%\n"
                                     "fn_long[...] | fuzzy_match  |   0.00% -> 100.00%";

AVP_TEST(parse_skips_headers_separators_and_ninja_lines) {
    GateChangeRows rows = gate_parse_changes_text(TA, avp_str(REALISTIC));
    static const struct {
        const char *name, *key;
        double from, to;
    } want[] = {{"Total", "matched_code", 42.0, 41.0},
                {"main/a", "fuzzy_match", 100.0, 97.5},
                {"fn_B", "fuzzy_match", 100.0, 95.0},
                {"fn_A", "fuzzy_match", 80.0, 99.99},
                {"fn_long[...]", "fuzzy_match", 0.0, 100.0}};
    REQUIRE(rows.n == 5);
    for (size_t i = 0; i < 5; i++) {
        CHECK_CZ(rows.v[i].name, want[i].name);
        CHECK_CZ(rows.v[i].key, want[i].key);
        CHECK_EQ_F64(rows.v[i].from, want[i].from);
        CHECK_EQ_F64(rows.v[i].to, want[i].to);
    }
    AvpSet *units = avp_set_new();
    avp_set_add(units, AVP_S("main/a"));
    AvpStrVec regr = {0}, impr = {0};
    for (size_t i = 0; i < rows.n; i++) {
        if (gate_is_unit_row(&rows.v[i], units)) continue;
        if (rows.v[i].to < rows.v[i].from) avp_vec_push(&regr, avp_str(rows.v[i].name));
        if (rows.v[i].to > rows.v[i].from) avp_vec_push(&impr, avp_str(rows.v[i].name));
    }
    REQUIRE(regr.n == 1); /* the one regression */
    CHECK_STR(regr.v[0], "fn_B");
    REQUIRE(impr.n == 2); /* improvements */
    CHECK_STR(impr.v[0], "fn_A");
    CHECK_STR(impr.v[1], "fn_long[...]");
}

AVP_TEST(improvements_and_unit_rows_are_not_counted_as_regressions) {
    /* the old step counted every printed line holding "|" (here four) as a regression line */
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.score_b = avp_json_float(TA, 95.0);
    AvpJson *doc = mlfx_changes(TA, tx_json("[[\"fn_A\", " A_ADDR_S ", 80.0, 100.0], [\"fn_B\", " B_ADDR_S ", 100.0, 95.0]]"));
    AvpJson *u0 = avp_json_at(avp_json_getz(doc, "units"), 0);
    avp_json_set(TA, u0, AVP_S("from"), tx_json("{\"fuzzy_match_percent\": 90.0, \"matched_code_percent\": 50.0}"));
    avp_json_set(TA, u0, AVP_S("to"), tx_json("{\"fuzzy_match_percent\": 97.5, \"matched_code_percent\": 50.0}"));
    avp_json_set(TA, doc, AVP_S("from"), tx_json("{\"matched_code_percent\": 42.0}"));
    avp_json_set(TA, doc, AVP_S("to"), tx_json("{\"matched_code_percent\": 41.0}"));
    c->s.changes_doc = doc;
    c->s.changes_text = avp_cz(TA, mlfx_changes_text(TA, doc, true));
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 0);
    TX_IN(c, "1 function regression(s), 1 function improvement(s), 2 unit or total row(s)");
    tx_end(c);
}

AVP_TEST(printed_table_disagreeing_with_the_json_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_rows = tx_json("[[\"fn_B\", " B_ADDR_S ", 100.0, 95.0]]");
    c->s.changes_text = "[1/1] CHANGES_FMT changes\n"; /* printed nothing; the JSON has a regression */
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_ROW(c, "changes", "FAIL");
    TX_IN(c, "disagree");
    tx_end(c);
}

AVP_TEST(stale_changes_json_is_not_read) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    mlfx_write(c->fx.slot, ML_REPORT, mlfx_dumps(TA, mlfx_report_f(TA, 100.0, 100.0)));
    mlfx_write(c->fx.slot, ML_CHANGES_JSON,
               mlfx_dumps(TA, mlfx_changes(TA, tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]"))));
    c->s.changes_write = false;
    c->s.changes_text = "";
    tx_run(c, NULL, NULL, "--steps", "rebase,changes", NULL);
    TX_RC(c, 1);
    TX_IN(c, "cannot read");
    tx_end(c);
}

AVP_TEST(comparison_that_did_not_run_fails) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    static const char *const docs[] = {"{\"units\": []}", "{\"from\": {\"fuzzy_match_percent\": 1.0}}", "[]",
                                       "{\"from\": {}, \"to\": {}, \"units\": {}}"};
    for (int i = 0; i < 4; i++) {
        c->s.changes_doc = tx_json(docs[i]);
        c->s.changes_text = "[1/2] REPORT_CHANGES\n[2/2] CHANGES_FMT changes\n";
        tx_run(c, NULL, NULL, "--steps", "rebase,gate,changes", NULL);
        TX_RC(c, 1);
        TX_IN(c, "holds no from/to comparison; the comparison did not run");
    }
    tx_end(c);
}

AVP_TEST(comparison_without_units_means_no_function_changed) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_replace(MLFX_A_SRC, "// banner", "// banner, reworded"));
    static const char *const docs[] = {
        "{\"from\": {\"fuzzy_match_percent\": 12.5, \"matched_code_percent\": 10.0}, "
        "\"to\": {\"fuzzy_match_percent\": 12.5, \"matched_code_percent\": 10.0}}",
        "{\"from\": {\"fuzzy_match_percent\": 12.5, \"matched_code_percent\": 10.0}, "
        "\"to\": {\"fuzzy_match_percent\": 12.5, \"matched_code_percent\": 10.0}, \"units\": []}"};
    for (int i = 0; i < 2; i++) {
        c->s.changes_doc = tx_json(docs[i]);
        c->s.changes_text = "[1/2] REPORT_CHANGES\n[2/2] CHANGES_FMT changes\n";
        tx_run(c, NULL, NULL, NULL);
        TX_RC(c, 0);
        TX_IN(c, "lists no unit: no function changed against the main baseline");
        TX_ROW(c, "changes", "PASS");
        i64 k = avp_str_find(c->text, AVP_S("== summary =="), 0, AVP_END);
        REQUIRE(k >= 0);
        CHECK(tx_has(avp_str_slice(c->text, k, AVP_END), "no function changed"));
        AvpStr sample = tx_read(avp_path_join(c->a, c->fx.logs, "review-sample.txt"));
        CHECK_MSG(sample.n == 0, "review-sample.txt: %.*s", AVP_SF(sample));
    }
    tx_end(c);
}

AVP_TEST(comparison_without_units_but_printed_regressions_disagrees) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    c->s.changes_doc = tx_json("{\"from\": {}, \"to\": {}}");
    c->s.changes_text = "fn_A | fuzzy_match | 100.00% ->  50.00%\n";
    tx_run(c, NULL, NULL, NULL);
    TX_RC(c, 1);
    TX_IN(c, "disagree");
    tx_end(c);
}


/* ---- the slot's tools follow the rebase ---------------------------------------------------------------------------- */
AVP_TEST(rebase_that_changes_tools_c_rebuilds_build_tools_before_the_gate) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    TX_MAIN(c, "tools/c/tools/check_rules/rule.c", AVP_S("/* a new R17 allowance */\n"));
    c->s.changes_rows = tx_json("[[\"fn_A\", " A_ADDR_S ", 100.0, 100.0]]");
    tx_run(c, NULL, NULL, "--steps", "rebase,gate", NULL);
    TX_RC(c, 1);
    TX_IN(c, "the rebase changed tools/c");
    TX_IN(c, "build/tools is stale");
    TX_IN(c, "run the same command again");
    TX_ROW(c, "gate", "FAIL");
    CHECK(mlst_called(&c->s, "build_tools"));
    CHECK(!mlst_called(&c->s, "ninja")); /* nothing was built or wiped by the old tools */
    TX_ROW(c, "rebase", "PASS");
    tx_end(c);
}

AVP_TEST(stamp_of_another_tree_makes_the_tools_stale_and_the_same_tree_does_not) {
    TxCase *c = tx_begin();
    TX_LANE(c, "src/a.cpp", tx_a_edited());
    TX_MAIN(c, "tools/c/tools/x/x.c", AVP_S("int x;\n"));
    tx_run(c, NULL, NULL, "--steps", "rebase", NULL);
    TX_RC(c, 0);
    mlfx_write(c->fx.slot, "build/tools/.tools-c-tree", AVP_S("0123456789abcdef0123456789abcdef01234567\n"));
    tx_run(c, NULL, NULL, "--steps", "gate", NULL);
    TX_RC(c, 1);
    TX_IN(c, "build/tools was built from tools/c 0123456");
    c->s.calls.n = 0;
    tx_run(c, NULL, NULL, "--steps", "gate", NULL); /* the first run wrote the stamp of the slot's tools/c tree */
    TX_RC(c, 0);
    CHECK(!mlst_called(&c->s, "build_tools"));
    tx_end(c);
}
AVP_TEST_MAIN("merge_lane_pipeline_changes")
