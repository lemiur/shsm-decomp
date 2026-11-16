/* ledger: TestExceptionsCheck of tools/test_ledger.py. */
#include "fixture.h"

/* `docs/exceptions.md:LINE:` prefix of the line of the original text containing snippet */
static AvpStr at_line(const char *snippet) {
    AvpStrVec lines = avp_str_splitlines(avp_str(EXCEPTIONS), false);
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_contains(lines.v[i], avp_str(snippet))) return avp_pyfmt(avp_arena(), "docs/exceptions.md:%d:", (i64)i + 1);
    return AVP_S("<none>");
}

static void edit(Fixture *fx, const char *old, const char *new_) {
    CHECK(has(avp_str(EXCEPTIONS), old));
    fx_write_eol(fx, "docs/exceptions.md", replace(avp_str(EXCEPTIONS), old, new_), "\n");
}

static bool has_at(AvpStr out, const char *snippet, const char *rest) {
    AvpStr pre = at_line(snippet);
    return avp_str_contains(out, avp_pyfmt(avp_arena(), "%S %s", &pre, rest));
}

AVP_TEST(test_clean_file) {
    Fixture fx = fx_new();
    FxRun r = FX_EXC0(&fx);
    CHECK_MSG(r.code == 0, "%.*s%.*s", AVP_SF(r.out), AVP_SF(r.err));
    CHECK_STR(r.out, "");
    fx_done(&fx);
}

AVP_TEST(test_unit_not_in_configure) {
    Fixture fx = fx_new();
    edit(&fx, "| `lib/Alpha` | `AlphaAsm`", "| `lib/Renamed` | `AlphaAsm`");
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has_at(r.out, "| `lib/Alpha` | `AlphaAsm`",
                 "ORIGINAL-ASM table: unit `lib/Renamed` is not in configure.py (renamed or removed?)"));
    fx_done(&fx);
}

AVP_TEST(test_missing_function) {
    Fixture fx = fx_new();
    edit(&fx, "`AlphaAsm` (1 function, 8 bytes)", "`AlphaGone` (1 function, 8 bytes)");
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has_at(r.out, "| `lib/Alpha` | `AlphaAsm`", "ORIGINAL-ASM table: `AlphaGone` is not a symbol"));
    fx_done(&fx);
}

AVP_TEST(test_original_asm_without_an_asm_body_in_source) {
    Fixture fx = fx_new();
    AvpJson *c = checker_json();
    AvpJson *allowed = avp_json_getz(c, "allowed");
    avp_json_set(avp_arena(), c, AVP_S("allowed"), avp_json_arr(avp_arena()));
    for (size_t i = 0; i < avp_json_len(allowed); i++)
        if (!avp_str_eqz(avp_json_getz(avp_json_at(allowed, i), "function")->u.str, "AlphaAsm"))
            avp_json_arr_push(avp_arena(), avp_json_getz(c, "allowed"), avp_json_at(allowed, i));
    avp_json_set(avp_arena(), avp_json_at(avp_json_getz(c, "functions"), 1), AVP_S("status"), avp_json_strz(avp_arena(), "ok"));
    fx_write_json(&fx, "checker.json", c);
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has_at(r.out, "| `lib/Alpha` | `AlphaAsm`",
                 "ORIGINAL-ASM table: `AlphaAsm` of `lib/Alpha` has no assembly body in source"));
    fx_done(&fx);
}

AVP_TEST(test_original_asm_row_without_report_row) {
    Fixture fx = fx_new();
    AvpJson *rep = make_report();
    AvpJson *fl = avp_json_getz(avp_json_at(avp_json_getz(rep, "units"), 1), "functions");
    AvpJson *keep = avp_json_arr(avp_arena());
    for (size_t i = 0; i < avp_json_len(fl); i++)
        if (!avp_str_eqz(avp_json_getz(avp_json_at(fl, i), "name")->u.str, "AlphaAsm"))
            avp_json_arr_push(avp_arena(), keep, avp_json_at(fl, i));
    avp_json_set(avp_arena(), avp_json_at(avp_json_getz(rep, "units"), 1), AVP_S("functions"), keep);
    fx_write_json(&fx, "report.json", rep);
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has(r.out, "`AlphaAsm` of Matching unit `lib/Alpha` is not in the objdiff report"));
    fx_done(&fx);
}

AVP_TEST(test_byte_count_mismatch) {
    Fixture fx = fx_new();
    edit(&fx, "(1 function, 8 bytes)", "(1 function, 12 bytes)");
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has(r.out, "ORIGINAL-ASM table: row says 1 functions, 12 bytes but the report has 1 functions, 8 bytes"));
    fx_done(&fx);
}

static void write_pending_alpha_one(Fixture *fx) {
    static const char *const extra[] = {"path", "src/lib/Alpha.c", "name", "AlphaOne", "status", "MATCH-PENDING",
                                        "match_rules", "R17", "unit_status", "Matching", NULL};
    AvpJson *c = checker_json();
    avp_json_getz(c, "functions")->u.arr.v[0] = fx_rec(extra);
    fx_write_json(fx, "checker.json", c);
}

AVP_TEST(test_checker_pending_in_matching_unit_must_be_listed) {
    Fixture fx = fx_new();
    write_pending_alpha_one(&fx);
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has(r.out, "checker reports `AlphaOne` MATCH-PENDING in Matching unit `lib/Alpha` but the MATCH-PENDING table "
                     "does not list it"));
    fx_done(&fx);
}

AVP_TEST(test_pending_in_nonmatching_unit_needs_no_listing) {
    Fixture fx = fx_new();
    FxRun r = FX_EXC0(&fx); /* BetaA is checker-pending in a NonMatching unit and unlisted */
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.out));
    fx_done(&fx);
}

AVP_TEST(test_function_found_elsewhere_is_only_a_note) {
    Fixture fx = fx_new();
    fx_write(&fx, "src/lib/Beta.c", "// Unit name unconfirmed\nvoid BetaA(void);\nvoid Wanderer(void);\n");
    edit(&fx, "`AlphaOne` | `static inline", "`Wanderer` | `static inline");
    FxRun r = FX_EXC0(&fx);
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.out));
    CHECK(has(r.out, "note: helpers table: `Wanderer` is not in the source of `lib/Alpha` (found elsewhere in src/include)"));
    r = FX_EXC(&fx, "--strict");
    CHECK(r.code == 1);
    fx_done(&fx);
}

AVP_TEST(test_ruling_function_must_exist) {
    Fixture fx = fx_new();
    edit(&fx, "| `AlphaOne` | R4 |", "| `NoSuchFunction` | R4 |");
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has(r.out, "rulings table: `NoSuchFunction` is not a symbol"));
    fx_done(&fx);
}

AVP_TEST(test_unit_with_missing_source_file) {
    Fixture fx = fx_new();
    avp_remove(fx_path(&fx, "src/lib/Epsilon.c"));
    edit(&fx, "| `lib/Alpha` | `AlphaOne` | `static", "| `lib/Epsilon` | `AlphaOne` | `static");
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK(has(r.out, "helpers table: unit `lib/Epsilon` has an Object() entry but src/lib/Epsilon.c does not exist"));
    fx_done(&fx);
}

AVP_TEST(test_marker_only_asm_is_a_note) {
    Fixture fx = fx_new();
    static const char *const orphan[] = {"path", "src/lib/Beta.c", "function", "Orphan", "rule", "R18", "status", "allowed",
                                         "message", "assembly body: function 'Orphan' is written in assembly", "reason",
                                         "ORIGINAL-ASM/ASM-HELPER marker in the source", NULL};
    AvpJson *c = checker_json();
    avp_json_arr_push(avp_arena(), avp_json_getz(c, "allowed"), fx_rec(orphan));
    fx_write_json(&fx, "checker.json", c);
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 0);
    CHECK(has(r.out, "note: `Orphan` of `lib/Beta` is assembly by source marker only"));
    fx_done(&fx);
}

AVP_TEST(test_pending_text_naming_unlisted_function_is_a_note) {
    Fixture fx = fx_new();
    edit(&fx, "| R17 (steering) | steering text |", "| R17 (steering) | steering text about `fn_80005000__5DeltaFv` |");
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 0);
    CHECK(has(r.out, "note: MATCH-PENDING table: the text names `fn_80005000__5DeltaFv` (a function of `game/Delta`)"));
    fx_done(&fx);
}

AVP_TEST_MAIN("ledger")
