/* Port of tools/test_gatecheck.py ElfTests.
 *
 * Python test -> C test
 *   ElfTests.test_roundtrip                        -> roundtrip
 *   ElfTests.test_rejects_little_endian_and_garbage -> rejects_little_endian_and_garbage
 */
#include "gc_fixtures.h"

AVP_TEST(roundtrip) {
    AvpArena *a = avp_arena();
    FxElfSec secs[2] = {{".data", 1, 3, 8, AVP_S("\x01\x02\x03\x04\x05\x06\x07\x08"), 0, 0},
                        {".bss", GC_SHT_NOBITS, 3, 8, {0}, 16, 0}};
    FxElfSym syms[4] = {{"a", 0, 4, GC_STT_OBJECT, 0, 1},
                        {"b", 4, 4, GC_STT_OBJECT, 0, 1},
                        {"c", 0, 16, GC_STT_OBJECT, 0, 2},
                        {"ext", 0, 0, 0, 1, 0}};
    FxElfRel rels[1] = {{1, 0, 4, 1, 0}};
    GcElf *e = fx_elf_parse(fx_elf(secs, 2, syms, 4, rels, 1));
    REQUIRE(e->nsecs >= 3);
    CHECK_CZ(e->secs[1].name, ".data");
    CHECK_CZ(e->secs[2].name, ".bss");
    gc_elf_symbols(a, e);
    REQUIRE(e->nsyms >= 5);
    CHECK_CZ(e->syms[1].name, "a");
    CHECK_CZ(e->syms[2].name, "b");
    CHECK_CZ(e->syms[3].name, "c");
    CHECK_CZ(e->syms[4].name, "ext");
    CHECK_EQ_U64(e->nsyms, 5);
    CHECK_EQ_I64(e->syms[3].size, 16);
    GcRelVec rel = {0};
    gc_elf_relocs_for(e, 1, &rel);
    REQUIRE(rel.n >= 1);
    CHECK_EQ_I64(rel.v[0].offset, 0);
    CHECK_EQ_I64(rel.v[0].sym, 4);
    CHECK_EQ_I64(rel.v[0].type, 1);
    AvpStr d = gc_elf_section_data(e, &e->secs[1]);
    CHECK_EQ_U64(d.n, 8);
    for (int i = 0; i < 8; i++) CHECK_EQ_I64((u8)d.p[i], i + 1);
}

AVP_TEST(rejects_little_endian_and_garbage) {
    CHECK(fx_elf_is_gate_error(AVP_S("hello")));
    FxElfSec s = {".data", 1, 3, 4, fx_fill(0, 4), 0, 0};
    AvpStr data = fx_copy(fx_elf(&s, 1, NULL, 0, NULL, 0));
    ((char *)data.p)[5] = 1;
    CHECK(fx_elf_is_gate_error(data));
}

AVP_TEST_MAIN("gatecheck")
