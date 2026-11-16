/* mwcc_symbols: the output pieces of the CLI (the --addr and --name lines, the --dump text, Python's '0x%x') on a
 * hand-made table. lib/shsm_pe's own test ports tools/mwcc_trace/test_symbols.py; the tool as a whole is checked by
 * the differential harness (diff/cases.txt) over every compiler under tools/compilers. */
#include "avp/test.h"
#include "mwcc_symbols.h"

static ShsmPeSymbols *table(void) {
    static ShsmPeFunc funcs[] = {{0x401000, 0x10, {"alpha", 5}, {"A.obj", 5}},
                                 {0x401020, 0x8, {"beta", 4}, {"A.obj", 5}},
                                 {0x401040, 0x10, {"Gamma", 5}, {"B.obj", 5}}};
    static ShsmPeRange ranges[] = {{0x401000, 0x401030, {"A.obj", 5}}, {0x401030, 0x401050, {"B.obj", 5}}};
    static ShsmPeDatum data[] = {{0x500000, {"table", 5}}};
    static ShsmPeSymbols t = {0x400000, funcs, 3, ranges, 2, data, 1};
    return &t;
}

AVP_TEST(hex_is_python_percent_x) {
    AvpArena *a = avp_arena();
    CHECK_CZ(mwcc_symbols_hex(a, 0), "0x0");
    CHECK_CZ(mwcc_symbols_hex(a, 0x5b88f0), "0x5b88f0");
    CHECK_CZ(mwcc_symbols_hex(a, -5), "0x-5");
}

AVP_TEST(addr_lines) {
    AvpArena *a = avp_arena();
    ShsmPeSymbols *t = table();
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x401000), "0x401000  alpha (A.obj)");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x401004), "0x401004  alpha+0x4 (A.obj)");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x401018), "0x401018  static in A.obj between alpha and beta");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x401028), "0x401028  static in A.obj after beta");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x401030), "0x401030  static in B.obj before Gamma");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x401050), "0x401050  no symbol");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x500000), "0x500000  table");
    CHECK_STR(mwcc_symbols_addr_line(a, t, 0x500001), "0x500001  no symbol");
    CHECK_STR(mwcc_symbols_addr_line(a, t, -1), "0x-1  no symbol");
}

AVP_TEST(name_lines) {
    AvpArena *a = avp_arena();
    AvpBuf b;
    avp_buf_init(&b, a);
    mwcc_symbols_name_lines(a, table(), AVP_S("GAMMA"), &b);
    mwcc_symbols_name_lines(a, table(), AVP_S("a"), &b);
    mwcc_symbols_name_lines(a, table(), AVP_S("zz"), &b);
    CHECK_STR(avp_buf_str(&b), "0x401040  Gamma (B.obj)\n"
                               "0x401000  alpha (A.obj)\n0x401020  beta (A.obj)\n0x401040  Gamma (B.obj)\n"
                               "zz  not found\n");
}

AVP_TEST(dump_text) {
    CHECK_STR(mwcc_symbols_dump_text(avp_arena(), table()),
              "va\tsize\tname\tmodule\n0x00401000\t16\talpha\tA.obj\n0x00401020\t8\tbeta\tA.obj\n"
              "0x00401040\t16\tGamma\tB.obj\n");
}

AVP_TEST_MAIN("mwcc_symbols")
