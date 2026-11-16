/* Port of tools/test_gatecheck.py MetadataTests.
 *
 * Python test -> C test
 *   MetadataTests.test_splits        -> splits
 *   MetadataTests.test_symbols       -> symbols
 *   MetadataTests.test_configure     -> configure
 *   MetadataTests.test_classify_word -> classify_word
 */
#include "gc_fixtures.h"

AVP_TEST(splits) {
    AvpArena *a = avp_arena();
    GcSplitVec sp = gc_parse_splits(a, avp_str(FX_SPLITS));
    REQUIRE(sp.n == 4);
    CHECK_EQ_I64(sp.v[0].start, 0x80006000);
    CHECK_EQ_I64(sp.v[0].end, 0x80006100);
    CHECK_CZ(sp.v[0].section, ".text");
    CHECK_CZ(sp.v[0].unit, "game/Foo.cpp");
    bool ctors = false;
    for (size_t i = 0; i < sp.n; i++)
        ctors |= sp.v[i].start == 0x803D8360 && sp.v[i].end == 0x803D8364 && strcmp(sp.v[i].section, ".ctors") == 0 &&
                 strcmp(sp.v[i].unit, "game/Foo.cpp") == 0;
    CHECK(ctors);
    AvpMap *st = avp_map_new();
    avp_map_put(st, AVP_S("game/Foo.cpp"), NULL)->p = (void *)"Matching";
    GcUnitMap *um = gc_unit_map(a, &sp, st);
    const char *unit, *sec;
    size_t idx;
    CHECK(gc_unit_lookup(um, 0x80006050, &unit, &sec, &idx));
    CHECK_CZ(unit, "game/Foo.cpp");
    CHECK_CZ(sec, ".text");
    CHECK(gc_unit_lookup(um, 0x80006100, &unit, &sec, &idx));
    CHECK_CZ(unit, "revolution/os/Bar.c");
    CHECK_CZ(sec, ".text");
    CHECK(!gc_unit_lookup(um, 0x80007000, &unit, &sec, &idx));
    CHECK_CZ(gc_unit_status(um, "game/Foo.cpp"), "Matching");
    CHECK_CZ(gc_unit_status(um, "revolution/os/Bar.c"), "asm");
}

AVP_TEST(symbols) {
    AvpArena *a = avp_arena();
    GcSymTxtVec sy = gc_parse_symbols_txt(a, avp_str(FX_SYMBOLS));
    CHECK_EQ_U64(sy.n, 4);
    GcSymbolMap *sm = fx_symbol_map(FX_SYMBOLS);
    GcHit h;
    REQUIRE(gc_symbol_lookup(sm, 0x80006050, &h));
    CHECK_CZ(h.name, "fn_80006040__3Foo");
    CHECK_EQ_I64(h.addr, 0x80006040);
    REQUIRE(gc_symbol_lookup(sm, 0x80006080, &h));
    CHECK_CZ(h.name, "fn_80006040__3Foo");
    CHECK(!h.inside);
}

AVP_TEST(configure) {
    AvpArena *a = avp_arena();
    AvpMap *st = gc_parse_configure(a, avp_str(FX_CONFIGURE));
    CHECK_EQ_U64(avp_map_len(st), 2);
    AvpVal *v = avp_map_get(st, AVP_S("game/Foo.cpp"));
    REQUIRE(v != NULL);
    CHECK_CZ((const char *)v->p, "Matching");
    v = avp_map_get(st, AVP_S("revolution/os/Bar.c"));
    REQUIRE(v != NULL);
    CHECK_CZ((const char *)v->p, "NonMatching");
}

AVP_TEST(classify_word) {
    AvpArena *a = avp_arena();
    CHECK_CZ(gc_classify_word(a, 0x38600000, 0x38600001), "low16");
    CHECK_CZ(gc_classify_word(a, 0x38600000, 0x38800000), "rD/rS");
    CHECK_CZ(gc_classify_word(a, 0x38600000, 0x80600000), "opcode");
    CHECK_CZ(gc_classify_word(a, 0x48000010, 0x48000020), "branch target");
}

AVP_TEST_MAIN("gatecheck")
