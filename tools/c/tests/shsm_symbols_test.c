/* shsm_symbols: symbols.txt and splits.txt parsing and lookups (tools/common/symbols.py). */
#include "shsm_symbols.h"
#include "shsm_test_util.h"

static const char SYMS[] =
    "fn_A = .text:0x80000000; // type:function size:0x10 scope:global\n"
    "lbl_X = .text:0x80000004; // type:label\r\n"
    "obj = .data:0x80001000; // type:object size:0x8 scope:local align:8 data:4byte hidden flagz\n"
    "bad line\n"
    "nosize = .text:0x80000020;\n"
    "fn_A = .text:0x80000040; // type:function size:0x20 scope:local\n"
    "\n"
    "big = .text:0x80000000;   //   type:function size:40\n"
    "trail = .text:0x80000000; x\n";

AVP_TEST(symbols_parse) {
    ShsmSymbolTable *t = shsm_parse_symbols(avp_arena(), avp_str(SYMS), NULL);
    REQUIRE(t->n == 6);
    const ShsmSymbol *a = shsm_sym_by_name(t, AVP_S("fn_A"));
    REQUIRE(a);
    CHECK_EQ_U64(a->lineno, 1);
    CHECK_EQ_U64(a->address, 0x80000000);
    CHECK(a->has_size && a->size == 0x10);
    CHECK_CZ(a->type, "function");
    CHECK_CZ(a->scope, "global");
    CHECK_CZ(a->raw, "fn_A = .text:0x80000000; // type:function size:0x10 scope:global");
    AvpPtrVec all = {0};
    CHECK_EQ_U64(shsm_sym_all_by_name(t, AVP_S("fn_A"), &all), 2);
    const ShsmSymbol *o = shsm_sym_by_name(t, AVP_S("obj"));
    REQUIRE(o);
    CHECK(o->hidden);
    CHECK(o->has_align && o->align == 8);
    CHECK_CZ(o->data, "4byte");
    CHECK_EQ_U64(o->nflags, 1);
    CHECK_STR(o->flags[0], "flagz");
    CHECK_STR(shsm_symbol_attr(o, "scope"), "local");
    const ShsmSymbol *l = shsm_sym_by_name(t, AVP_S("lbl_X"));
    CHECK_CZ(l->raw, "lbl_X = .text:0x80000004; // type:label");
    CHECK(!shsm_sym_by_name(t, AVP_S("nosize"))->has_size);
    CHECK_EQ_U64(shsm_sym_by_name(t, AVP_S("big"))->size, 0x40); /* int("40", 16) */
    CHECK(!shsm_sym_by_name(t, AVP_S("trail")));
}

AVP_TEST(symbols_lookups) {
    ShsmSymbolTable *t = shsm_parse_symbols(avp_arena(), avp_str(SYMS), NULL);
    AvpPtrVec at = {0};
    CHECK_EQ_U64(shsm_sym_at(t, 0x80000000, NULL, &at), 2);
    CHECK_CZ(((ShsmSymbol *)at.v[0])->name, "big");
    CHECK_CZ(((ShsmSymbol *)at.v[1])->name, "fn_A");
    CHECK(shsm_sym_first_at(t, 0x80000000, ".data") == NULL);
    CHECK_CZ(shsm_sym_containing(t, 0x80000008)->name, "fn_A");
    CHECK_CZ(shsm_sym_containing(t, 0x80000030)->name, "big");
    CHECK_CZ(shsm_sym_describe(avp_arena(), t, 0x80000030), "big+0x30");
    CHECK_CZ(shsm_sym_describe(avp_arena(), t, 0x80000004), "lbl_X");
    CHECK_CZ(shsm_sym_describe(avp_arena(), t, 0x90000000), "0x90000000");
    size_t n = 0;
    ShsmSymbol *const *r = shsm_sym_in_range(t, 0x80000004, 0x80000040, &n);
    CHECK_EQ_U64(n, 2);
    CHECK_CZ(r[0]->name, "lbl_X");
    CHECK_CZ(r[1]->name, "nosize");
    AvpPtrVec f = {0};
    CHECK_EQ_U64(shsm_sym_functions(t, &f), 3);
    AvpPtrVec d = {0};
    CHECK_EQ_U64(shsm_sym_in_section(t, ".data", &d), 1);
}

AVP_TEST(symbols_bad_numbers) {
    CHECK_RAISES(shsm_parse_symbols(avp_arena(), AVP_S("x = .text:0x1; // size:zz\n"), NULL), SHSM_EXC_VALUE,
                 "invalid literal for int() with base 16: 'zz'");
    CHECK_RAISES(shsm_parse_symbols(avp_arena(), AVP_S("x = .text:0x1; // align:08\n"), NULL), SHSM_EXC_VALUE,
                 "invalid literal for int() with base 0: '08'");
    ShsmSymbolTable *t = shsm_parse_symbols(avp_arena(), AVP_S("x = .text:0x1; // size:0x10 align:0x20\n"), NULL);
    CHECK_EQ_U64(t->symbols[0].size, 16);
    CHECK_EQ_I64(t->symbols[0].align, 32);
}

static const char SPLITS[] =
    "Sections:\n"
    "\t.text       type:code align:4\n"
    "\t.data       type:data\n"
    "\n"
    "game/A.cpp: comment:0\n"
    "\t.text       start:0x80000000 end:0x80000040\n"
    "\t.data       start:0x80001000 end:0x80001008\n"
    "\n"
    "  // comment\n"
    "game/B.c:\n"
    "\t.text       start:0x80000040 end:0x80000080\n"
    "\t.bss        start:0x1\n";

AVP_TEST(splits_parse) {
    ShsmSplits *s = shsm_parse_splits(avp_arena(), avp_str(SPLITS));
    REQUIRE(s->nunits == 2);
    CHECK_EQ_U64(s->nsections, 2);
    CHECK_STR(s->sections[0].attrs[0].value, "code");
    const ShsmSplitUnit *a = shsm_splits_unit(s, "main/game/A");
    REQUIRE(a);
    CHECK_CZ(a->file, "game/A.cpp");
    CHECK_EQ_U64(a->nranges, 2);
    CHECK_CZ(a->ranges[1].section, ".data");
    CHECK_EQ_U64(a->nattrs, 1);
    CHECK_STR(a->attrs[0].key, "comment");
    const ShsmSplitUnit *b = shsm_splits_unit(s, "game/B.c");
    REQUIRE(b);
    CHECK_EQ_U64(b->nranges, 1);
    CHECK_CZ(shsm_splits_unit_of(s, 0x80000050), "game/B");
    CHECK_CZ(shsm_splits_unit_of(s, 0x80000000), "game/A");
    CHECK(shsm_splits_unit_of(s, 0x80000080) == NULL);
    CHECK_CZ(shsm_splits_range_of(s, 0x80001004)->section, ".data");
    ShsmSymbolTable *t = shsm_parse_symbols(avp_arena(), avp_str(SYMS), s);
    CHECK_CZ(shsm_sym_unit_of(t, 0x80000044), "game/B");
}

AVP_TEST(splits_repeated_header) {
    ShsmSplits *s = shsm_parse_splits(avp_arena(), AVP_S("a.c:\n\t.text start:0x10 end:0x20\nb.c:\n\t.text start:0x30 end:0x40\n"
                                                         "a.c:\n\t.data start:0x50 end:0x60\n"));
    REQUIRE(s->nunits == 2);
    CHECK_CZ(s->units[0].path, "a");
    CHECK_EQ_U64(s->units[0].nranges, 1);
    CHECK_CZ(s->units[0].ranges[0].section, ".data");
    CHECK_RAISES(shsm_parse_splits(avp_arena(), AVP_S("a.c:\n\t.text start:0xq end:0x20\n")), SHSM_EXC_VALUE,
                 "invalid literal for int() with base 16: '0xq'");
}

AVP_TEST_MAIN("shsm_symbols")
