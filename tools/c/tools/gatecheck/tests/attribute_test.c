/* Port of tools/test_gatecheck.py AttributeTests. The tests that run the command line (test_missing_input,
 * test_elf_that_does_not_belong_to_the_dol_is_not_used and the CLI half of test_report_and_cli_exit_codes) run the
 * gatecheck executable.
 *
 * Python test -> C test
 *   AttributeTests.test_attribution                    -> attribution
 *   AttributeTests.test_report_and_cli_exit_codes      -> report_and_cli_exit_codes
 *   AttributeTests.test_missing_input                  -> missing_input
 *   AttributeTests.test_elf_that_does_not_belong_to_the_dol_is_not_used
 *                                                      -> elf_that_does_not_belong_to_the_dol_is_not_used
 */
#include "gc_fixtures.h"

typedef struct {
    ShsmDol *retail, *built;
    GcUnitMap *units;
    GcSymbolMap *syms;
} Ctx;

static Ctx ctx_new(void) {
    Ctx c;
    AvpStr tx = fx_cat(fx_range(256), fx_range(256));
    AvpStr data = fx_fill(0x55, 0x40);
    char *p = (char *)fx_copy(tx).p;
    p[0x44] = (char)~p[0x44]; /* inside fn_80006040 */
    p[0x45] ^= 0x01;
    p[0x120] ^= 0x01; /* inside Bar (NonMatching unit) */
    char *d2 = (char *)fx_copy(data).p;
    d2[0x21] ^= 0x01; /* past lbl_80300000 (size 0x20) */
    FxDolSec sr[2] = {{0, 0x80006000u, tx}, {7, 0x80300000u, data}};
    FxDolSec sb[2] = {{0, 0x80006000u, avp_strn(p, 512)}, {7, 0x80300000u, avp_strn(d2, 0x40)}};
    c.retail = fx_dol_parse(fx_dol(sr, 2));
    c.built = fx_dol_parse(fx_dol(sb, 2));
    c.units = fx_unit_map(FX_SPLITS, FX_CONFIGURE);
    c.syms = fx_symbol_map(FX_SYMBOLS);
    return c;
}

static const GcGroup *find_group(const GcAttribution *r, const char *unit, const char *sym) {
    for (size_t i = 0; i < r->groups.n; i++)
        if (strcmp(r->groups.v[i]->unit, unit) == 0 && strcmp(r->groups.v[i]->sym, sym) == 0) return r->groups.v[i];
    return NULL;
}

AVP_TEST(attribution) {
    AvpArena *a = avp_arena();
    Ctx c = ctx_new();
    GcAttribution *res = gc_attribute(a, c.retail, c.built, c.units, c.syms, c.syms, 2);
    CHECK_EQ_U64(res->header.n, 0);
    CHECK_EQ_I64(res->total_bytes, 4);
    const GcGroup *f = find_group(res, "game/Foo.cpp", "fn_80006040__3Foo");
    REQUIRE(f != NULL);
    CHECK_EQ_I64(f->nbytes, 2);
    CHECK_EQ_I64(f->first, 0x80006044);
    CHECK_CZ(f->section, ".text");
    REQUIRE(f->words.n >= 1);
    CHECK_EQ_I64(f->words.v[0].addr, 0x80006044);
    const GcGroup *b = find_group(res, "revolution/os/Bar.c", "BarInit");
    REQUIRE(b != NULL);
    CHECK_EQ_I64(b->nbytes, 1);
    CHECK_CZ(gc_unit_status(c.units, "revolution/os/Bar.c"), "NonMatching");
    const GcGroup *d = find_group(res, "game/Foo.cpp", "<after lbl_80300000>");
    REQUIRE(d != NULL);
    CHECK_CZ(d->section, ".data");
    CHECK_EQ_I64(d->nbytes, 1);
    const GcFirst *first = NULL;
    for (size_t i = 0; i < res->first.n; i++)
        if (strcmp(res->first.v[i].section, ".text") == 0) first = &res->first.v[i];
    REQUIRE(first != NULL);
    CHECK_EQ_I64(first->addr, 0x80006044);
}

AVP_TEST(report_and_cli_exit_codes) {
    AvpArena *a = avp_arena();
    Ctx c = ctx_new();
    GcAttribution *res = gc_attribute(a, c.retail, c.built, c.units, c.syms, c.syms, 3);
    AvpStrVec lines = gc_format_attribution(a, res, c.units, 12, 40, false, "", "");
    AvpBuf text;
    avp_buf_init(&text, a);
    for (size_t i = 0; i < lines.n; i++) {
        avp_buf_put(&text, lines.v[i]);
        avp_buf_putc(&text, '\n');
    }
    AvpStr t = avp_buf_str(&text);
    CHECK(fx_contains(t, "== game/Foo.cpp [Matching"));
    CHECK(fx_contains(t, "fn_80006040__3Foo"));

    const char *td = fx_tmp_root();
    fx_write_meta(td);
    fx_mkdirs(td, "build/R5WEA4");
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(0, 0x80006000u, fx_range(256)));
    fx_write_bytes(td, "build/R5WEA4/main.dol", fx_dol1(0, 0x80006000u, fx_range(256)));
    FxRun r = fx_gatecheck(td, FX_ARGS("attribute"));
    CHECK_EQ_I64(r.rc, 0);
    CHECK(fx_contains(r.out, "identical"));
    char *bad = (char *)fx_copy(fx_range(256)).p;
    bad[0x45] ^= 2;
    fx_write_bytes(td, "build/R5WEA4/main.dol", fx_dol1(0, 0x80006000u, avp_strn(bad, 256)));
    r = fx_gatecheck(td, FX_ARGS("attribute"));
    CHECK_EQ_I64(r.rc, 1);
    CHECK(fx_contains(r.out, "fn_80006040__3Foo"));
    CHECK(fx_contains(r.out, "DIFFERENT"));
    avp_tempdir_cleanup(td);
}

AVP_TEST(missing_input) {
    const char *td = fx_tmp_root();
    FxRun r = fx_gatecheck(td, FX_ARGS("attribute"));
    CHECK_EQ_I64(r.rc, 2);
    avp_tempdir_cleanup(td);
}

static FxRun run_elf(const char *td, AvpStr elf_text) {
    FxElfSec s = {".text", 1, 6, 4, elf_text, 0, 0x80006000u};
    FxElfSym y = {"ElfOnlyName", 0x80006000u, 0x100, GC_STT_FUNC, 1, 1};
    fx_write_bytes(td, "build/R5WEA4/main.elf", fx_elf_ex(&s, 1, &y, 1, NULL, 0, 2));
    return fx_gatecheck(td, FX_ARGS("attribute"));
}

AVP_TEST(elf_that_does_not_belong_to_the_dol_is_not_used) {
    AvpStr text = fx_range(256);
    char *bad = (char *)fx_copy(text).p;
    bad[0x45] ^= 2;
    const char *td = fx_tmp_root();
    fx_write_meta(td);
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(0, 0x80006000u, text));
    fx_write_bytes(td, "build/R5WEA4/main.dol", fx_dol1(0, 0x80006000u, avp_strn(bad, 256)));
    /* the ELF the DOL was made from: its names are used */
    FxRun r = run_elf(td, avp_strn(bad, 256));
    CHECK_EQ_I64(r.rc, 1);
    CHECK(fx_contains(r.out, "ElfOnlyName"));
    CHECK(!fx_contains(r.err, "does not belong"));
    /* a stale ELF (an older link): refused, loudly */
    r = run_elf(td, text);
    CHECK_EQ_I64(r.rc, 1);
    CHECK(fx_contains(r.err, "WARNING"));
    CHECK(fx_contains(r.err, "does not belong to"));
    CHECK(fx_contains(r.err, "section .text differs from the DOL at 0x80006045"));
    CHECK(!fx_contains(r.out, "ElfOnlyName"));
    CHECK(fx_contains(r.out, "fn_80006040__3Foo")); /* symbols.txt names instead */
    avp_tempdir_cleanup(td);
}

AVP_TEST_MAIN("gatecheck")
