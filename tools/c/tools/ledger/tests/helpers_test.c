/* ledger: TestHelpers of tools/test_ledger.py (fmt_pct, auto_naming, parse_configure, parse_status_cell,
 * parse_exceptions, parse_symbols), plus TestProgress.test_pure_order_when_no_table_exists. */
#include "fixture.h"

static void check_naming(const char *name, const char *want) {
    AvpStrVec v = ledger_auto_naming(avp_str(name));
    if (!want) {
        CHECK_MSG(v.n == 0, "%s", name);
        return;
    }
    REQUIRE(v.n == 1);
    CHECK_STR(v.v[0], want);
}

AVP_TEST(test_fmt_pct) {
    AvpArena *a = avp_arena();
    CHECK_STR(ledger_fmt_pct(a, 100.0), "100%");
    CHECK_STR(ledger_fmt_pct(a, 94.98508), "95.0%");
    CHECK_STR(ledger_fmt_pct(a, 99.96), "99.9%"); /* never reads as exact */
    CHECK_STR(ledger_fmt_pct(a, 0.04), "0.0%");
}

AVP_TEST(test_auto_naming) {
    check_naming("fn_800067CC__8unk_RampFf", "placeholder");
    check_naming("lbl_80529A38", "placeholder");
    check_naming("__ct__12unk_801A92A8Fv", "placeholder");
    check_naming("__sinit_\\PadRumble_cpp", "placeholder (compiler-generated name)");
    check_naming("@36@__dt__8CCounterFv", "compiler-generated thunk");
    check_naming("@36@fn_8009CB1C__8CCounterFR7unk_Msg", "compiler-generated thunk, placeholder");
    check_naming("OSInit", NULL);
    check_naming("fn_8009CB1CA", NULL); /* nine hex digits: not a placeholder address */
}

AVP_TEST(test_parse_configure_skips_comments_and_reads_multiline) {
    LedgerCfgVec c = ledger_parse_configure(avp_arena(), avp_str(CONFIGURE));
    static const char *const want[][3] = {{"Matching", "lib/Alpha", "lib/Alpha.c"},
                                          {"NonMatching", "lib/Beta", "lib/Beta.c"},
                                          {"Matching", "lib/Epsilon", "lib/Epsilon.c"},
                                          {"Matching", "game/Gamma", "game/Gamma.cpp"},
                                          {"NonMatching", "game/Delta", "game/Delta.cpp"}};
    REQUIRE(c.n == 5);
    for (size_t i = 0; i < 5; i++) {
        CHECK_STR(c.v[i].status, want[i][0]);
        CHECK_STR(c.v[i].unit, want[i][1]);
        CHECK_STR(c.v[i].path, want[i][2]);
    }
}

AVP_TEST(test_parse_status_cell) {
    AvpStr base, note;
    ledger_parse_status_cell(AVP_S("parked, 83.1% (MATCH-PENDING x, y.md)"), &base, &note);
    CHECK_STR(base, "parked, 83.1%");
    CHECK_STR(note, "MATCH-PENDING x, y.md");
    ledger_parse_status_cell(AVP_S("matched"), &base, &note);
    CHECK_STR(base, "matched");
    CHECK(note.p == NULL);
}

AVP_TEST(test_parse_exceptions_rows) {
    LedgerExcRows rows = ledger_parse_exceptions(avp_arena(), avp_str(EXCEPTIONS));
    static const char *const want[][3] = {{"MATCH-PENDING", "lib/Alpha", "AlphaPend"},
                                          {"ORIGINAL-ASM", "lib/Alpha", "AlphaAsm"},
                                          {"helpers", "lib/Alpha", "AlphaOne"},
                                          {"rulings", NULL, "AlphaOne"}};
    REQUIRE(rows.n == 4);
    for (size_t i = 0; i < 4; i++) {
        CHECK_CZ(rows.v[i]->table, want[i][0]);
        if (want[i][1]) {
            REQUIRE(rows.v[i]->units.n == 1);
            CHECK_STR(rows.v[i]->units.v[0], want[i][1]);
        } else CHECK(rows.v[i]->units.n == 0);
        REQUIRE(rows.v[i]->functions.n == 1);
        CHECK_STR(rows.v[i]->functions.v[0], want[i][2]);
    }
}

AVP_TEST(test_parse_symbols) {
    AvpMap *s = ledger_parse_symbols(avp_arena(), AVP_S("memcpy = .init:0x80005F34; // type:function size:0x29C scope:global\n"
                                                        "lbl = .data:0x80001000; // type:object\n"));
    AvpVal *v = avp_map_get(s, AVP_S("memcpy"));
    REQUIRE(v != NULL);
    const LedgerSymVec *m = (const LedgerSymVec *)v->p;
    REQUIRE(m->n == 1);
    CHECK_STR(m->v[0].section, ".init");
    CHECK_EQ_I64(m->v[0].addr, 0x80005F34);
    CHECK(m->v[0].has_size);
    CHECK_EQ_I64(m->v[0].size, 0x29C);
    CHECK_STR(m->v[0].attrs, "type:function size:0x29C scope:global");
    v = avp_map_get(s, AVP_S("lbl"));
    REQUIRE(v != NULL);
    CHECK(!((const LedgerSymVec *)v->p)->v[0].has_size);
}

AVP_TEST(test_pure_order_when_no_table_exists) {
    AvpArena *a = avp_arena();
    static const struct {
        const char *name;
        i64 addr;
    } in[] = {{"revolution/os/OS", 0x80210000},
              {"MSL_C/x", 0x80195000},
              {"libjpeg/j", 0x801B0000},
              {"revolution/NdevExi2AD/e", 0x80190000},
              {"Runtime.PPCEABI.H/r", 0x801A0000}};
    LedgerUnitVec units = {0};
    for (size_t i = 0; i < 5; i++) {
        LedgerUnit *u = AVP_NEW(a, LedgerUnit);
        u->name = avp_str(in[i].name);
        u->section = "sdk";
        LedgerFn *f = AVP_NEW(a, LedgerFn);
        f->addr = in[i].addr;
        avp_vec_push(&u->fns, f);
        avp_vec_push(&units, u);
    }
    AvpStrVec none = {0};
    LedgerUnitVec out = ledger_order_units(a, &units, &none);
    static const char *const want[] = {"revolution/NdevExi2AD/e", "MSL_C/x", "Runtime.PPCEABI.H/r", "libjpeg/j",
                                       "revolution/os/OS"};
    REQUIRE(out.n == 5);
    for (size_t i = 0; i < 5; i++) CHECK_STR(out.v[i]->name, want[i]);
}

AVP_TEST_MAIN("ledger")
