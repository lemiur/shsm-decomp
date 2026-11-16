/* Port of tools/test_gatecheck.py DolTests (class DolTests -> dol_test.c).
 *
 * Python test -> C test
 *   DolTests.test_parse_and_read          -> parse_and_read
 *   DolTests.test_truncated               -> truncated
 *   DolTests.test_identical               -> identical
 *   DolTests.test_byte_runs_and_merge     -> byte_runs_and_merge
 *   DolTests.test_diff_with_size_change   -> diff_with_size_change
 *   DolTests.test_shift_hint              -> shift_hint   (random.Random(1) is replaced by a fixed LCG: the test
 *                                            only needs 1024 bytes of mixed values)
 *   DolTests.test_bss_header              -> bss_header
 */
#include "gc_fixtures.h"

AVP_TEST(parse_and_read) {
    AvpArena *a = avp_arena();
    FxDolSec s[2] = {{0, 0x80004000u, fx_cat(fx_cat(AVP_S("\x01\x02\x03\x04"), AVP_S("\x01\x02\x03\x04")),
                                             fx_cat(AVP_S("\x01\x02\x03\x04"), AVP_S("\x01\x02\x03\x04")))},
                     {7, 0x80100000u, fx_fill(0xAA, 8)}};
    ShsmDol *d = fx_dol_parse(fx_dol(s, 2));
    REQUIRE(d->nsegs == 2);
    CHECK_EQ_I64(d->segs[0].index, 0);
    CHECK_EQ_I64(d->segs[1].index, 7);
    AvpStr got;
    CHECK(shsm_dol_read(a, d, 0x80004004, 4, &got));
    CHECK_MEM(got.p, "\x01\x02\x03\x04", 4);
    CHECK_EQ_U64(got.n, 4);
    CHECK(!shsm_dol_read(a, d, 0x80004100, 4, &got));
    CHECK_EQ_U64(d->bss_addr, 0x80500000u);
    CHECK_EQ_U64(d->entry, 0x80004000u);
}

AVP_TEST(truncated) {
    CHECK(fx_dol_is_gate_error(fx_fill(0, 10)));
    AvpStr full = fx_dol1(0, 0x80004000u, fx_fill(0, 16));
    CHECK(fx_dol_is_gate_error(avp_strn(full.p, 0x108)));
}

AVP_TEST(identical) {
    AvpArena *a = avp_arena();
    ShsmDol *x = fx_dol_parse(fx_dol1(0, 0x80004000u, fx_fill(1, 64)));
    ShsmDol *y = fx_dol_parse(fx_dol1(0, 0x80004000u, fx_fill(1, 64)));
    GcRunVec runs = {0}, only_r = {0}, only_b = {0};
    gc_diff_dols(x, y, &runs, &only_r, &only_b);
    CHECK_EQ_U64(runs.n, 0);
    CHECK_EQ_U64(only_r.n, 0);
    CHECK_EQ_U64(only_b.n, 0);
    CHECK_EQ_U64(gc_header_differences(a, x, y).n, 0);
}

AVP_TEST(byte_runs_and_merge) {
    u8 x[100], y[100];
    for (int i = 0; i < 100; i++) x[i] = y[i] = (u8)i;
    y[10] ^= 1;
    y[12] ^= 1; /* gap of 1: merged */
    y[60] ^= 1;
    GcRunVec runs = {0};
    gc_byte_runs(x, y, 100, 0x1000, &runs);
    REQUIRE(runs.n == 3);
    CHECK_EQ_I64(runs.v[0].start, 0x100A);
    CHECK_EQ_I64(runs.v[0].end, 0x100B);
    CHECK_EQ_I64(runs.v[1].start, 0x100C);
    CHECK_EQ_I64(runs.v[1].end, 0x100D);
    CHECK_EQ_I64(runs.v[2].start, 0x103C);
    CHECK_EQ_I64(runs.v[2].end, 0x103D);
    GcRunVec m = gc_merge_runs(&runs, 3);
    REQUIRE(m.n == 2);
    CHECK_EQ_I64(m.v[0].start, 0x100A);
    CHECK_EQ_I64(m.v[0].end, 0x100D);
    CHECK_EQ_I64(m.v[0].nbytes, 2);
    CHECK_EQ_I64(m.v[1].start, 0x103C);
    CHECK_EQ_I64(m.v[1].end, 0x103D);
    CHECK_EQ_I64(m.v[1].nbytes, 1);
}

AVP_TEST(diff_with_size_change) {
    AvpArena *a = avp_arena();
    FxDolSec sa[2] = {{0, 0x80004000u, fx_fill(1, 32)}, {7, 0x80010000u, fx_fill(2, 16)}};
    FxDolSec sb[2] = {{0, 0x80004000u, fx_fill(1, 48)}, {7, 0x80010000u, fx_fill(2, 16)}};
    ShsmDol *x = fx_dol_parse(fx_dol(sa, 2));
    ShsmDol *y = fx_dol_parse(fx_dol(sb, 2));
    GcRunVec runs = {0}, only_r = {0}, only_b = {0};
    gc_diff_dols(x, y, &runs, &only_r, &only_b);
    CHECK_EQ_U64(runs.n, 0);
    CHECK_EQ_U64(only_r.n, 0);
    REQUIRE(only_b.n == 1);
    CHECK_EQ_I64(only_b.v[0].start, 0x80004020);
    CHECK_EQ_I64(only_b.v[0].end, 0x80004030);
    AvpStrVec hd = gc_header_differences(a, x, y);
    REQUIRE(hd.n == 1);
    CHECK(avp_str_contains(hd.v[0], AVP_S("section text 0")));
    CHECK(avp_str_contains(hd.v[0], AVP_S("size +16")));
}

AVP_TEST(shift_hint) {
    AvpArena *a = avp_arena();
    char text[1024];
    u32 st = 12345;
    for (int i = 0; i < 1024; i++) {
        st = st * 1103515245u + 12345u;
        text[i] = (char)(st >> 16);
    }
    AvpBuf sh;
    avp_buf_init(&sh, a);
    avp_buf_putn(&sh, text, 100);
    for (int i = 0; i < 4; i++) avp_buf_putc(&sh, 0);
    avp_buf_putn(&sh, text + 100, 1020 - 100);
    avp_buf_putn(&sh, text + 1020, 4);
    ShsmDol *retail = fx_dol_parse(fx_dol1(0, 0x80004000u, avp_strn(text, 1024)));
    ShsmDol *built = fx_dol_parse(fx_dol1(0, 0x80004000u, avp_buf_str(&sh)));
    GcRunVec runs = {0}, only_r = {0}, only_b = {0};
    gc_diff_dols(retail, built, &runs, &only_r, &only_b);
    REQUIRE(runs.n >= 1);
    i64 k = 0;
    CHECK(gc_shift_hint(a, retail, built, runs.v[0].start, &k));
    CHECK_EQ_I64(k, 4);
    CHECK(!gc_shift_hint(a, retail, retail, 0x80004000, &k));
}

AVP_TEST(bss_header) {
    AvpArena *a = avp_arena();
    FxDolSec s = {0, 0x80004000u, fx_fill(1, 4)};
    ShsmDol *x = fx_dol_parse(fx_dol_ex(&s, 1, 0x80500000u, 0x100, 0x80004000u));
    ShsmDol *y = fx_dol_parse(fx_dol_ex(&s, 1, 0x80500000u, 0x120, 0x80004000u));
    AvpStrVec hd = gc_header_differences(a, x, y);
    bool any = false;
    for (size_t i = 0; i < hd.n; i++) any |= avp_str_startswith(hd.v[i], AVP_S("bss"));
    CHECK(any);
}

AVP_TEST_MAIN("gatecheck")
