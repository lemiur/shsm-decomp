/* ledger: assembly source units (`.s`, user's ruling 2026-11-14), a post-retirement change (DEVIATIONS.md D-ledger-asm-1).
 * A `.s` unit is keyed without its extension and is assembly throughout: every function in it is ORIGINAL-ASM, with no
 * R18 hit required (the rule checker does not read `.s` files). */
#include "fixture.h"

/* the fixture with lib/Alpha (and optionally lib/Beta) built from an assembly source file, and no checker facts for it */
static Fixture asm_fixture(bool beta_too) {
    Fixture fx = fx_new();
    AvpStr cfg = replace(avp_str(CONFIGURE), "\"lib/Alpha.c\"", "\"lib/Alpha.s\"");
    if (beta_too) cfg = replace(cfg, "\"lib/Beta.c\"", "\"lib/Beta.s\"");
    fx_write_eol(&fx, "configure.py", cfg, "\n");
    AvpArena *a = avp_arena();
    AvpJson *c = checker_json();
    AvpJson *funcs = avp_json_arr(a), *allowed = avp_json_arr(a);
    const AvpJson *of = avp_json_getz(c, "functions"), *oa = avp_json_getz(c, "allowed");
    for (size_t i = 0; i < avp_json_len(of); i++)
        if (!avp_str_eqz(avp_json_getz(avp_json_at(of, i), "path")->u.str, "src/lib/Alpha.c"))
            avp_json_arr_push(a, funcs, avp_json_at(of, i));
    for (size_t i = 0; i < avp_json_len(oa); i++) {
        AvpStr p = avp_json_getz(avp_json_at(oa, i), "path")->u.str;
        if (!avp_str_eqz(p, "src/lib/Alpha.c") && !(beta_too && avp_str_eqz(p, "src/lib/Beta.c")))
            avp_json_arr_push(a, allowed, avp_json_at(oa, i));
    }
    avp_json_set(a, c, AVP_S("functions"), funcs);
    avp_json_set(a, c, AVP_S("allowed"), allowed);
    fx_write_json(&fx, "checker.json", c);
    return fx;
}

static AvpStr row(AvpStr out, const char *name) {
    AvpStr want = avp_pyfmt(avp_arena(), "| `%s` |", name);
    AvpStrVec lines = avp_str_split(out, AVP_S("\n"), -1);
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_contains(lines.v[i], want)) return lines.v[i];
    return AVP_S("<no row>");
}

AVP_TEST(unit_key_strips_s) {
    AvpArena *a = avp_arena();
    CHECK_STR(ledger_unit_key(a, AVP_S("src/MetroTRK/Processor/ppc/Generic/__exception.s")),
              "MetroTRK/Processor/ppc/Generic/__exception");
    CHECK_STR(ledger_unit_key(a, AVP_S("main/lib/X.S")), "lib/X");
    CHECK_STR(ledger_unit_key(a, AVP_S("lib/X.c")), "lib/X");
    CHECK_STR(ledger_unit_key(a, AVP_S("lib/X.cpp")), "lib/X");
    CHECK_STR(ledger_unit_key(a, AVP_S("lib/X.sx")), "lib/X.sx");
    CHECK(ledger_is_asm_source(AVP_S("lib/X.s")));
    CHECK(ledger_is_asm_source(AVP_S("lib/X.S")));
    CHECK(!ledger_is_asm_source(AVP_S("lib/X.c")));
    CHECK(!ledger_is_asm_source(AVP_S("s")));
}

AVP_TEST(progress_counts_every_function_of_a_linked_s_unit_as_original_asm) {
    Fixture fx = asm_fixture(true);
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    CHECK(has(row(r.out, "AlphaOne"), "| ORIGINAL-ASM |"));
    CHECK(has(row(r.out, "AlphaAsm"), "| ORIGINAL-ASM |"));
    CHECK(has(row(r.out, "AlphaPend"), "| ORIGINAL-ASM |"));
    /* a parked `.s` unit: ORIGINAL-ASM when it links */
    CHECK(has(row(r.out, "BetaA"), "ORIGINAL-ASM when linked; not counted"));
    CHECK(has(r.out, "| `ORIGINAL-ASM` (linked, listed apart, outside both counts and totals) | 3 | 36 |"));
    fx_done(&fx);
}

AVP_TEST(exceptions_check_needs_no_r18_hit_but_every_function_listed) {
    Fixture fx = asm_fixture(false);
    FxRun r = FX_EXC0(&fx);
    CHECK(r.code == 1);
    CHECK_MSG(!has(r.out, "has no assembly body in source"), "%.*s", AVP_SF(r.out));
    CHECK(has(r.out, "`AlphaOne` of `lib/Alpha` is in an assembly source unit (ORIGINAL-ASM throughout) and is not "
                     "listed in the ORIGINAL-ASM table"));
    CHECK(has(r.out, "`AlphaPend` of `lib/Alpha` is in an assembly source unit"));
    CHECK(!has(r.out, "`AlphaAsm` of `lib/Alpha` is in an assembly source unit"));
    fx_done(&fx);
}

/* dtk's generated pad_ function over a `.s` unit's label-only bytes has no symbols.txt entry: not a problem there,
 * still one in a C unit */
static void report_with_pad(Fixture *fx) {
    AvpJson *rep = make_report();
    AvpJson *fl = avp_json_getz(avp_json_at(avp_json_getz(rep, "units"), 1), "functions");
    avp_json_set(avp_arena(), avp_json_at(fl, 1), AVP_S("name"), avp_json_strz(avp_arena(), "pad_03_80002010_text"));
    fx_write_json(fx, "report.json", rep);
}

AVP_TEST(dtk_pad_function_of_an_s_unit_needs_no_symbol) {
    Fixture fx = asm_fixture(false);
    report_with_pad(&fx);
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK_MSG(r.code == 0, "%.*s%.*s", AVP_SF(r.out), AVP_SF(r.err));
    CHECK(has(row(r.out, "pad_03_80002010_text"), "| ORIGINAL-ASM |"));
    fx_done(&fx);
    Fixture c = fx_new();
    report_with_pad(&c);
    r = FX_PROGRESS(&c, "--out", "-");
    CHECK(r.code != 0);
    CHECK(has(r.err, "pad_03_80002010_text: not in symbols.txt") || has(r.out, "pad_03_80002010_text: not in symbols.txt"));
    fx_done(&c);
}

AVP_TEST_MAIN("ledger")
