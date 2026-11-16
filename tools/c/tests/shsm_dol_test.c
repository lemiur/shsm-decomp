/* shsm_dol: the DOL reader on a synthetic image (header fields, segment lookup, reads across and within segments,
 * bss, the GateError texts). */
#include "shsm_dol.h"
#include "shsm_repo.h"
#include "shsm_test_util.h"

#include <string.h>

static void put32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

/* T0 at 0x80003100 (16 bytes), T1 empty, D0 at 0x80003110 (8 bytes, contiguous with T0), D10 at 0x80010000 (4 bytes);
 * bss 0x80020000+0x40, entry 0x80003104. */
static u8 *synthetic(size_t *n) {
    static u8 img[0x100 + 28];
    memset(img, 0, sizeof img);
    put32(img + 0, 0x100);
    put32(img + 72, 0x80003100);
    put32(img + 144, 16);
    put32(img + 4 * 7, 0x110);
    put32(img + 72 + 4 * 7, 0x80003110);
    put32(img + 144 + 4 * 7, 8);
    put32(img + 4 * 17, 0x118);
    put32(img + 72 + 4 * 17, 0x80010000);
    put32(img + 144 + 4 * 17, 4);
    put32(img + 0xD8, 0x80020000);
    put32(img + 0xDC, 0x40);
    put32(img + 0xE0, 0x80003104);
    for (size_t i = 0x100; i < sizeof img; i++) img[i] = (u8)i;
    *n = sizeof img;
    return img;
}

AVP_TEST(synthetic_header) {
    size_t n;
    u8 *img = synthetic(&n);
    ShsmDol *d = shsm_dol_parse(avp_arena(), img, n);
    REQUIRE(d->nsegs == 3);
    CHECK_EQ_I64(d->entry, 0x80003104);
    CHECK_EQ_I64(d->bss_addr, 0x80020000);
    CHECK_EQ_I64(d->bss_size, 0x40);
    char nb[4];
    CHECK_CZ(shsm_dol_seg_name(&d->segs[0], nb), "T0");
    CHECK_CZ(shsm_dol_seg_name(&d->segs[1], nb), "D0");
    CHECK_CZ(shsm_dol_seg_name(&d->segs[2], nb), "D10");
    CHECK(shsm_dol_seg_is_text(&d->segs[0]));
    CHECK(!shsm_dol_seg_is_text(&d->segs[1]));
    CHECK_EQ_I64(d->segs[2].index, 17);
    CHECK_EQ_I64(shsm_dol_seg_end(&d->segs[1]), 0x80003118);
}

AVP_TEST(synthetic_lookup_and_read) {
    size_t n;
    u8 *img = synthetic(&n);
    ShsmDol *d = shsm_dol_parse(avp_arena(), img, n);
    CHECK(shsm_dol_segment_at(d, 0x80003100) == &d->segs[0]);
    CHECK(shsm_dol_segment_at(d, 0x8000310F) == &d->segs[0]);
    CHECK(shsm_dol_segment_at(d, 0x80003110) == &d->segs[1]);
    CHECK(shsm_dol_segment_at(d, 0x80003118) == NULL);
    CHECK(shsm_dol_segment_at(d, 0x800030FF) == NULL);
    CHECK_EQ_I64(shsm_dol_file_offset(d, 0x80003104), 0x104);
    CHECK_EQ_I64(shsm_dol_file_offset(d, 0x80010003), 0x11B);
    CHECK_EQ_I64(shsm_dol_file_offset(d, 0x80010004), -1);
    AvpStr s;
    /* across T0 and D0 (contiguous): gatecheck's read gathers, codecmp's does not */
    REQUIRE(shsm_dol_read(avp_arena(), d, 0x8000310E, 4, &s));
    REQUIRE(s.n == 4);
    CHECK((u8)s.p[0] == 0x0E && (u8)s.p[1] == 0x0F && (u8)s.p[2] == 0x10 && (u8)s.p[3] == 0x11);
    CHECK(!shsm_dol_read_within(d, 0x8000310E, 4, &s));
    CHECK(shsm_dol_read_within(d, 0x80003110, 8, &s) && (u8)s.p[7] == 0x17);
    CHECK(!shsm_dol_read_within(d, 0x80003110, 9, &s));
    CHECK(!shsm_dol_read(avp_arena(), d, 0x80003114, 8, &s));
    CHECK(shsm_dol_read(avp_arena(), d, 0x80000000, 0, &s) && s.n == 0);
    CHECK(shsm_dol_in_bss(d, 0x80020000));
    CHECK(shsm_dol_in_bss(d, 0x8002003F));
    CHECK(!shsm_dol_in_bss(d, 0x80020040));
    CHECK(!shsm_dol_in_bss(d, 0x8001FFFF));
}

AVP_TEST(synthetic_errors) {
    size_t n;
    u8 *img = synthetic(&n);
    CHECK_RAISES(shsm_dol_parse(avp_arena(), img, 0xFF), SHSM_EXC_VALUE, "DOL too short (255 bytes)");
    CHECK_RAISES(shsm_dol_parse(avp_arena(), img, n - 1), SHSM_EXC_VALUE,
                 "DOL section 17 runs past the end of the file");
    const char *dir = fx_dir("dol fixture");
    const char *missing = avp_join(avp_arena(), dir, "none.dol");
    CHECK_RAISES(shsm_dol_load(avp_arena(), missing), SHSM_EXC_OS, NULL);
}

AVP_TEST_MAIN("shsm_dol")
