/* Port of the command-line tests of tools/test_gatecheck.py's LayoutTests and AlignTests; each runs the gatecheck
 * executable on fixture files in a fresh temporary root (the attribute command's tests are in attribute_test.c).
 *
 * Python test -> C test
 *   LayoutTests.test_cli_layout_all_and_single                      -> cli_layout_all_and_single
 *   LayoutTests.test_cli_layout_refuses_empty_work_and_misused_overrides
 *                                                                   -> cli_layout_refuses_empty_work_and_misused_overrides
 *   LayoutTests.test_cli_layout_reports_a_code_size_difference      -> cli_layout_reports_a_code_size_difference
 *   AlignTests.test_cli_requires_unit                               -> cli_requires_unit
 * New: cli_layout_stops_on_a_stale_object.
 */
#include "gc_fixtures.h"

AVP_TEST(cli_layout_all_and_single) {
    const char *td = fx_tmp_root();
    fx_mkdirs(td, "build/R5WEA4/src/game");
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(7, FX_START, fx_retail_blob()));
    fx_write_text(td, "configure.py", "Object(Matching, \"game/Foo.cpp\"),\nObject(NonMatching, \"game/Missing.cpp\"),\n");
    fx_write_text(td, "build/R5WEA4/asm/game/Foo.s", fx_retail_asm());
    FxBuilt o = fx_built_defaults();
    fx_write_bytes(td, "build/R5WEA4/src/game/Foo.o", fx_built(&o));
    FxRun r1 = fx_gatecheck(td, FX_ARGS("layout", "game/Foo"));
    FxRun r2 = fx_gatecheck(td, FX_ARGS("layout", "--all"));
    CHECK_EQ_I64(r1.rc, 0);
    CHECK_EQ_I64(r2.rc, 0);
    CHECK(fx_contains(r1.out, "ok    Matching    game/Foo.cpp") || fx_contains(r2.out, "ok    Matching    game/Foo.cpp"));
    CHECK(!fx_contains(r1.out, "Missing") && !fx_contains(r2.out, "Missing"));

    o.s1 = AVP_S("zz\0");
    fx_write_bytes(td, "build/R5WEA4/src/game/Foo.o", fx_built(&o));
    r1 = fx_gatecheck(td, FX_ARGS("layout", "main/game/Foo.cpp"));
    r2 = fx_gatecheck(td, FX_ARGS("layout", "--all"));
    CHECK_EQ_I64(r1.rc, 1);
    CHECK_EQ_I64(r2.rc, 1);
    CHECK(fx_contains(r1.out, "CONTENT") || fx_contains(r2.out, "CONTENT"));
    CHECK(fx_contains(r1.out, "DIFF  Matching    game/Foo.cpp") || fx_contains(r2.out, "DIFF  Matching    game/Foo.cpp"));
    avp_tempdir_cleanup(td);
}

AVP_TEST(cli_layout_refuses_empty_work_and_misused_overrides) {
    const char *td = fx_tmp_root();
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(7, FX_START, fx_retail_blob()));
    fx_write_text(td, "configure.py", "Object(Matching, \"game/Foo.cpp\"),\n");
    FxRun r = fx_gatecheck(td, FX_ARGS("layout", "--all")); /* no objects: nothing verified */
    CHECK_EQ_I64(r.rc, 1);
    CHECK(fx_contains(r.out, "units checked: 0"));
    CHECK(fx_contains(r.err, "nothing was verified"));
    const char *obj = avp_path_join(avp_arena(), td, "x.o");
    r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo", "game/Bar", "--object", obj));
    CHECK_EQ_I64(r.rc, 2);
    CHECK(fx_contains(r.err, "give exactly one unit"));
    r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo", "game/Bar", "--asm", obj));
    CHECK_EQ_I64(r.rc, 2);
    CHECK(fx_contains(r.err, "give exactly one unit"));
    r = fx_gatecheck(td, FX_ARGS("layout", "--all", "--object", obj));
    CHECK_EQ_I64(r.rc, 2);
    CHECK(fx_contains(r.err, "give exactly one unit"));
    avp_tempdir_cleanup(td);
}

AVP_TEST(cli_layout_reports_a_code_size_difference) {
    const char *td = fx_tmp_root();
    fx_mkdirs(td, "build/R5WEA4/src/game");
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(7, FX_START, fx_retail_blob()));
    fx_write_text(td, "configure.py", "Object(Matching, \"game/Foo.cpp\"),\n");
    FxFn fns[] = {{"func", 8}};
    fx_write_text(td, "build/R5WEA4/asm/game/Foo.s",
                  avp_sprintf(avp_arena(), "%s%s", fx_code_asm(0x80006000u, 0x80006008u, fns, 1), fx_retail_asm()));
    FxBuilt o = fx_built_defaults();
    fx_write_bytes(td, "build/R5WEA4/src/game/Foo.o", fx_built(&o)); /* func is 16 bytes in the object */
    FxRun r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo"));
    CHECK_EQ_I64(r.rc, 1);
    CHECK(fx_contains(r.out, "TOTAL    .text    code size: retail split 0x8, built 0x10"));
    avp_tempdir_cleanup(td);
}

/* A built object older than its source or symbols.txt is not judged: exit status 3 and a rebuild hint. */
static void set_mtime_plus(const char *root, const char *rel, double secs) {
    const char *p = avp_path_join(avp_arena(), root, rel);
    double now = 0;
    AVP_ASSERT(avp_getmtime(avp_path_join(avp_arena(), root, "build/R5WEA4/src/game/Foo.o"), &now));
    i64 ns = (i64)((now + secs) * 1e9);
    AVP_ASSERT(avp_utime_ns(p, ns, ns));
}

AVP_TEST(cli_layout_stops_on_a_stale_object) {
    const char *td = fx_tmp_root();
    fx_mkdirs(td, "build/R5WEA4/src/game");
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(7, FX_START, fx_retail_blob()));
    fx_write_text(td, "configure.py", "Object(Matching, \"game/Foo.cpp\"),\n");
    fx_write_text(td, "build/R5WEA4/asm/game/Foo.s", fx_retail_asm());
    FxBuilt o = fx_built_defaults();
    fx_write_bytes(td, "build/R5WEA4/src/game/Foo.o", fx_built(&o));
    fx_write_text(td, "src/game/Foo.cpp", "int x;\n");
    fx_write_text(td, "config/R5WEA4/symbols.txt", "");
    set_mtime_plus(td, "src/game/Foo.cpp", -10);
    set_mtime_plus(td, "config/R5WEA4/symbols.txt", -10);
    FxRun r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo"));
    CHECK_EQ_I64(r.rc, 0); /* the object is the newest file */
    set_mtime_plus(td, "src/game/Foo.cpp", 60);
    r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo"));
    CHECK_EQ_I64(r.rc, 3);
    CHECK(fx_contains(r.err, "STALE object build/R5WEA4/src/game/Foo.o is older than src/game/Foo.cpp"));
    CHECK(fx_contains(r.err, "tools/bin/ninja.exe build/R5WEA4/src/game/Foo.o"));
    CHECK(!fx_contains(r.out, "layout matches"));
    r = fx_gatecheck(td, FX_ARGS("layout", "--all"));
    CHECK_EQ_I64(r.rc, 3);
    CHECK(fx_contains(r.err, "1 of 1 objects are older than their inputs"));
    /* a changed symbols.txt makes every object stale */
    set_mtime_plus(td, "src/game/Foo.cpp", -10);
    set_mtime_plus(td, "config/R5WEA4/symbols.txt", 60);
    r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo"));
    CHECK_EQ_I64(r.rc, 3);
    CHECK(fx_contains(r.err, "older than"));
    /* --object names the object itself: no staleness judgement */
    r = fx_gatecheck(td, FX_ARGS("layout", "game/Foo", "--object", avp_path_join(avp_arena(), td, "build/R5WEA4/src/game/Foo.o")));
    CHECK_EQ_I64(r.rc, 0);
    avp_tempdir_cleanup(td);
}

AVP_TEST(cli_requires_unit) {
    const char *td = fx_tmp_root();
    fx_write_bytes(td, "orig/R5WEA4/sys/main.dol", fx_dol1(0, 0x80004000u, fx_fill(0, 4)));
    FxRun r = fx_gatecheck(td, FX_ARGS("layout"));
    CHECK_EQ_I64(r.rc, 2);
    avp_tempdir_cleanup(td);
}

AVP_TEST_MAIN("gatecheck")
