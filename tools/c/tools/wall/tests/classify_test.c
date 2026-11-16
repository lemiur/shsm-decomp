/* Tests for wall_classify: tools/test_wall.py TestClasses, TestControlFlow (but test_cfg) and TestRegister (but
 * test_abstraction), comparing the fields the Python asserts, plus the whole classify() dict of each case and of
 * a few more against the Python's json.dumps (classify_cases.h). */
#include "avp/test.h"
#include "wall_classify.h"
#include "wall_testrows.h"

#include "classify_cases.h"

static const char SIMPLE[] = "cmpwi r3, 0x0\nbeq .L_4\nli r3, 0x1\nblr\nli r3, 0x2\nblr";

static AvpJson *run(const char *retail, const char *compiled) {
    AvpArena *a = avp_arena();
    return wall_classify(a, wall_test_fn(a, retail), wall_test_fn(a, compiled), NULL, NULL, NULL, NULL, 0);
}

static AvpStr dump(const AvpJson *v) { return avp_json_dumps(avp_arena(), v, NULL); }
static AvpJson *get(const AvpJson *o, const char *k) { return avp_json_getz(o, k); }
static AvpStr sget(const AvpJson *o, const char *k) { return avp_json_get_str(o, k, AVP_S("")); }
static bool has_str(const AvpJson *arr, const char *s) {
    for (size_t i = 0; i < avp_json_len(arr); i++)
        if (avp_str_eqz(avp_json_str_or(avp_json_at(arr, i), AVP_S("")), s)) return true;
    return false;
}
static bool is_bool(const AvpJson *v) { return v && v->kind == AVP_JSON_BOOL; }
static bool joined_contains(const AvpJson *arr, const char *s) {
    return avp_str_contains(dump(arr), avp_str(s));
}

/* ---- TestClasses ---- */

AVP_TEST(test_exact) {
    AvpJson *d = run(SIMPLE, SIMPLE);
    CHECK(avp_json_truthy(get(d, "exact")));
    CHECK_STR(sget(d, "verdict"), "EXACT");
}

AVP_TEST(test_referent) {
    AvpJson *d = run("lis r3, lbl_80001000@ha\naddi r3, r3, lbl_80001000@l\nblr",
                     "lis r3, lbl_80002000@ha\naddi r3, r3, lbl_80002000@l\nblr");
    CHECK_STR(sget(d, "wall"), WALL_REFERENT);
    CHECK_EQ_I64(avp_json_len(get(d, "referent")), 2);
    CHECK(avp_str_startswith(sget(d, "verdict"), AVP_S("WALL referent")));
}

AVP_TEST(test_different_callee_is_referent_first) {
    AvpJson *d = run("bl fn_80001000\nblr", "bl fn_80002000\nblr");
    CHECK_STR(sget(d, "wall"), WALL_REFERENT);
    CHECK(joined_contains(get(get(d, "calls"), "readings"), "different callee"));
}

AVP_TEST(test_retail_calls_compiled_inlines) {
    AvpJson *d = run("mr r3, r31\nbl fn_80001000\nblr",
                     "mr r3, r31\nlwz r0, 0x4(r3)\naddi r0, r0, 0x1\nstw r0, 0x4(r3)\nblr");
    CHECK_STR(sget(d, "wall"), WALL_CALL);
    CHECK(avp_str_contains(sget(d, "verdict"), AVP_S("retail calls fn_80001000 where compiled inlines it")));
}

AVP_TEST(test_compiled_calls_retail_inlines) {
    AvpJson *d = run("mr r3, r31\nlwz r0, 0x4(r3)\naddi r0, r0, 0x1\nstw r0, 0x4(r3)\nblr",
                     "mr r3, r31\nbl fn_80001000\nblr");
    CHECK(avp_str_contains(sget(d, "verdict"), AVP_S("compiled calls fn_80001000 where retail has it inlined")));
}

AVP_TEST(test_call_order) {
    AvpJson *d = run("bl fn_80001000\nbl fn_80002000\nblr", "bl fn_80002000\nbl fn_80001000\nblr");
    CHECK(has_str(get(get(d, "calls"), "readings"), "same calls in a different order"));
    CHECK(has_str(get(d, "classes"), WALL_CALL));
}

AVP_TEST(test_savegpr_versus_stmw_is_flags) {
    AvpJson *d = run("stwu r1, -0x20(r1)\naddi r11, r1, 0x20\nbl _savegpr_27\nli r3, 0x0\nblr",
                     "stwu r1, -0x20(r1)\nstmw r27, 0xc(r1)\nli r3, 0x0\nblr");
    CHECK(avp_json_truthy(get(get(d, "calls"), "flags_only")));
    CHECK(joined_contains(get(get(d, "calls"), "helpers"), "-use_lmw_stmw"));
}

AVP_TEST(test_savegpr_range_is_register) {
    AvpJson *d = run("bl _savegpr_24\nmr r24, r3\nblr", "bl _savegpr_26\nmr r26, r3\nblr");
    CHECK(!has_str(get(d, "classes"), WALL_REFERENT));
    CHECK(!has_str(get(d, "classes"), WALL_CALL));
    CHECK_STR(sget(d, "wall"), WALL_REGISTER);
}

AVP_TEST(test_other_immediate) {
    AvpJson *d = run("lwz r3, 0x10(r4)\nblr", "lwz r3, 0x14(r4)\nblr");
    CHECK_STR(sget(d, "wall"), WALL_OTHER);
    CHECK(avp_str_contains(sget(d, "verdict"), AVP_S("immediate")));
}

AVP_TEST(test_unexplained) {
    AvpJson *d = run("lwz r3, 0x10(r4)\nblr", "fadds f1, f2, f3\nblr");
    CHECK_EQ_I64(avp_json_get_i64(d, "unexplained", -1), 1);
    CHECK(avp_str_startswith(sget(d, "verdict"), AVP_S("WALL other")));
}

/* ---- TestControlFlow ---- */

AVP_TEST(test_inverted_condition) {
    AvpJson *d = run(SIMPLE, "cmpwi r3, 0x0\nbne .L_4\nli r3, 0x2\nblr\nli r3, 0x1\nblr");
    CHECK_STR(sget(d, "wall"), WALL_CONTROL);
    AvpJson *iso = get(get(d, "control"), "isomorphic");
    CHECK(is_bool(iso) && avp_json_truthy(iso));
    CHECK_EQ_I64(avp_json_get_i64(get(d, "control"), "inverted", -1), 1);
    CHECK(avp_str_contains(sget(d, "verdict"), AVP_S("inverted condition")));
}

AVP_TEST(test_block_order) {
    AvpJson *d = run("b .L_3\nli r3, 0x2\nblr\nli r3, 0x1\nb .L_1", "b .L_1\nli r3, 0x1\nb .L_3\nli r3, 0x2\nblr");
    AvpJson *iso = get(get(d, "control"), "isomorphic");
    CHECK(is_bool(iso) && avp_json_truthy(iso));
    CHECK(avp_str_contains(sget(d, "verdict"), AVP_S("different block order")));
}

AVP_TEST(test_loop_rotation) {
    AvpJson *d = run("cmpwi r3, 0x0\nbeq .L_4\nsubi r3, r3, 0x1\nb .L_0\nblr",
                     "b .L_2\nsubi r3, r3, 0x1\ncmpwi r3, 0x0\nbne .L_1\nblr");
    CHECK_STR(sget(d, "wall"), WALL_CONTROL);
    CHECK(avp_str_contains(sget(d, "verdict"), AVP_S("loop rotation")));
    CHECK_STR(dump(get(get(get(d, "control"), "retail"), "loops")), "{\"top-test\": 1}");
    CHECK_STR(dump(get(get(get(d, "control"), "compiled"), "loops")), "{\"bottom-test\": 1}");
}

AVP_TEST(test_early_return) {
    AvpJson *d = run(SIMPLE, "cmpwi r3, 0x0\nbeq .L_4\nli r3, 0x1\nb .L_5\nli r3, 0x2\nblr");
    CHECK(joined_contains(get(get(d, "control"), "findings"), "early return versus shared epilogue"));
}

AVP_TEST(test_switch) {
    AvpJson *d = run("cmplwi r3, 0x2\nbgt .L_7\nlis r4, jumptable_80003000@ha\nslwi r0, r3, 2\n"
                     "addi r4, r4, jumptable_80003000@l\nlwzx r0, r4, r0\nmtctr r0\nbctr\nli r3, 0x0\nblr",
                     "cmpwi r3, 0x1\nbeq .L_4\ncmpwi r3, 0x2\nbeq .L_4\nli r3, 0x0\nblr");
    CHECK(joined_contains(get(get(d, "control"), "findings"), "switch lowered differently"));
}

AVP_TEST(test_signedness) {
    AvpJson *d = run("cmpw r3, r4\nblt .L_3\nli r3, 0x0\nblr", "cmplw r3, r4\nblt .L_3\nli r3, 0x0\nblr");
    CHECK_STR(sget(d, "wall"), WALL_CONTROL);
    CHECK(avp_str_contains(dump(get(d, "row_evidence")), AVP_S("signedness")));
}

AVP_TEST(test_compare_constant) {
    AvpJson *d = run("cmpwi r3, 0x1\nbeq .L_3\nli r3, 0x0\nblr", "cmpwi r3, 0x2\nbeq .L_3\nli r3, 0x0\nblr");
    CHECK_STR(sget(d, "wall"), WALL_CONTROL);
    CHECK(avp_str_contains(dump(get(d, "row_evidence")), AVP_S("compare constant")));
}

/* ---- TestRegister ---- */

AVP_TEST(test_permutation) {
    AvpJson *d = run("mr r31, r3\nmr r30, r4\nadd r3, r31, r30\nblr", "mr r30, r3\nmr r31, r4\nadd r3, r30, r31\nblr");
    CHECK(avp_str_startswith(sget(d, "verdict"), AVP_S("EQUIVALENT-LOOKING")));
    AvpJson *sw = get(get(d, "register"), "swapped");
    REQUIRE(avp_json_len(sw) == 2);
    bool a = false, b = false;
    for (size_t i = 0; i < 2; i++) {
        AvpJson *s = avp_json_at(sw, i);
        a = a || (avp_str_eqz(sget(s, "retail"), "r31") && avp_str_eqz(sget(s, "compiled"), "r30"));
        b = b || (avp_str_eqz(sget(s, "retail"), "r30") && avp_str_eqz(sget(s, "compiled"), "r31"));
        CHECK(avp_json_truthy(get(s, "callee_saved")));
    }
    CHECK(a && b);
}

AVP_TEST(test_inconsistent_renaming) {
    AvpJson *d = run("mr r31, r3\nmr r30, r31\nblr", "mr r30, r3\nmr r30, r30\nblr");
    CHECK(!avp_json_truthy(get(get(d, "register"), "consistent")));
    CHECK(avp_str_startswith(sget(d, "verdict"), AVP_S("WALL register/schedule")));
}

AVP_TEST(test_schedule) {
    AvpJson *d = run("li r3, 0x1\nli r4, 0x2\nlwz r5, 0x0(r6)\nblr", "li r4, 0x2\nli r3, 0x1\nlwz r5, 0x0(r6)\nblr");
    CHECK_MSG(avp_str_startswith(sget(d, "verdict"), AVP_S("EQUIVALENT-LOOKING")), "%s", sget(d, "verdict").p);
    CHECK(avp_json_len(get(get(d, "register"), "moved")) > 0);
    AvpJson *ib = get(get(d, "register"), "moved_in_block");
    CHECK(is_bool(ib) && avp_json_truthy(ib));
}

AVP_TEST(test_stack_frame) {
    AvpJson *d = run("stwu r1, -0x20(r1)\nstw r3, 0x8(r1)\naddi r1, r1, 0x20\nblr",
                     "stwu r1, -0x30(r1)\nstw r3, 0xc(r1)\naddi r1, r1, 0x30\nblr");
    CHECK_STR(sget(d, "wall"), WALL_REGISTER);
    AvpJson *st = get(get(d, "register"), "stack");
    CHECK_STR(dump(get(st, "frame")), "[32, 48]");
    CHECK(avp_str_contains(dump(get(st, "retail_only_slots")), AVP_S("8")));
    CHECK(!avp_str_startswith(sget(d, "verdict"), AVP_S("EQUIVALENT")));
}

AVP_TEST(test_sda21_r0_is_not_a_register) {
    AvpJson *d = run("lwz r4, lbl_80001000@sda21(r0)\nli r0, 0x0\nstwx r0, r3, r4\nblr",
                     "lwz r0, lbl_80001000@sda21(r0)\nli r4, 0x0\nstwx r4, r3, r0\nblr");
    CHECK_MSG(avp_str_startswith(sget(d, "verdict"), AVP_S("EQUIVALENT-LOOKING")), "%s", sget(d, "verdict").p);
}

AVP_TEST(test_self_call_is_a_call) {
    AvpArena *a = avp_arena();
    static const CcTdList none = {0};
    CcFn *r = wall_test_fn(a, "stwu r1, -0x10(r1)\nbl .L_0\naddi r1, r1, 0x10\nblr");
    CHECK(avp_json_truthy(get(wall_classify(a, r, r, &none, NULL, NULL, NULL, 0), "exact")));
    WallCall *calls;
    size_t n = wall_calls(a, r->rows, r->nrows, &calls);
    REQUIRE(n == 1);
    CHECK_STR(calls[0].name, "<self>");
    CcFn *c = wall_test_fn(a, "stwu r1, -0x10(r1)\nbl g\naddi r1, r1, 0x10\nblr");
    AvpJson *d = wall_classify(a, r, c, &none, NULL, NULL, NULL, 0);
    CHECK_MSG(avp_str_eqz(sget(d, "wall"), WALL_CALL), "%s", sget(d, "verdict").p);
}

/* ---- the whole dict against the Python ---- */

AVP_TEST(classify_dicts_match_python) {
    AvpArena *a = avp_arena();
    static const CcTdList none = {0};
    AvpJsonDumpOpts compact = AVP_JSON_COMPACT;
    for (size_t k = 0; k < sizeof CLASSIFY_CASES / sizeof CLASSIFY_CASES[0]; k++) {
        const WallClassifyCase *c = &CLASSIFY_CASES[k];
        AvpJson *d = wall_classify(a, wall_test_fn(a, c->retail), wall_test_fn(a, c->compiled),
                                   c->has_targets ? &none : NULL, NULL, NULL, NULL, 0);
        AvpStr got = avp_json_dumps(a, d, &compact);
        CHECK_MSG(avp_str_eqz(got, c->expect), "%s:\n  got  %s\n  want %s", c->name, got.p, c->expect);
    }
}

AVP_TEST(hex) {
    CHECK_STR(wall_hex(avp_arena(), 0x8000ABCDLL), "0x8000ABCD");
    CHECK_STR(wall_hex(avp_arena(), 0), "0x0");
}

AVP_TEST_MAIN("wall_classify")
