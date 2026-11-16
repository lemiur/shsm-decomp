/* cc: the pure pieces of the port against what tools/mwcc_trace/cc.py gives (unit_flags on a build.ninja text,
 * the listing filter, norm, split_fns, the retail block search, the --no-targets comparison text). The tool as a
 * whole is checked by the differential harness (diff/cases.txt). There is no Python test of cc.py to port. */
#include "avp/test.h"
#include "avp/fs.h"
#include "avp/proc.h"
#include "cctool.h"

static const char NINJA[] =
    "rule mwcc\n"
    "  command = x\n"
    "\n"
    "# src/revolution/os/OSCache.c: os (linked True)\n"
    "build build\\R5WEA4\\src\\revolution\\os\\OSCache.o: mwcc src\\revolution\\os\\OSCache.c $\n"
    "    || pre\n"
    "  mw_version = Wii\\1.0\n"
    "  cflags = -nodefaults  -proc gekko $\n"
    "      -pragma \"cats off\"\n"
    "  basefile = b\n"
    "build build\\R5WEA4\\src\\game\\Pad.o: mwcc_sjis src\\game\\Pad.cpp\n"
    "  cflags = -O3\n"
    "  mw_version = GC\\3.0a5\n";

AVP_TEST(unit_flags_takes_the_first_block_naming_the_unit) {
    AvpArena *a = avp_arena();
    AvpStr ver;
    AvpStrVec cf = {0};
    const char *err = NULL;
    CHECK_EQ_I64(cctool_unit_flags(a, avp_str(NINJA), "revolution/os/OSCache", &ver, &cf, &err), 0);
    CHECK_STR(ver, "Wii/1.0");
    REQUIRE(cf.n == 5);
    CHECK_STR(cf.v[0], "-nodefaults");
    CHECK_STR(cf.v[3], "-pragma");
    CHECK_STR(cf.v[4], "cats off");
    /* "src\\" + the unit path must be a substring of the head */
    CHECK_EQ_I64(cctool_unit_flags(a, avp_str(NINJA), "os/OSCache", &ver, &cf, &err), 1);
    CHECK_EQ_I64(cctool_unit_flags(a, avp_str(NINJA), "game/Pad", &ver, &cf, &err), 0);
    CHECK_STR(ver, "GC/3.0a5");
    CHECK_EQ_I64(cctool_unit_flags(a, avp_str(NINJA), "game/Pa", &ver, &cf, &err), 1);
    CHECK_EQ_I64(cctool_unit_flags(a, avp_str(NINJA), "nosuch", &ver, &cf, &err), 1);
}

AVP_TEST(prefix_len) {
    CHECK_EQ_I64(cctool_prefix_len(AVP_S("/* 80006860 00000000  94 21 FF F0 */\tstwu r1")), 36);
    CHECK_EQ_I64(cctool_prefix_len(AVP_S("/* 80006860 00000000 94 21 FF F0 */\tstwu r1")), 0);
    CHECK_EQ_I64(cctool_prefix_len(AVP_S("/* 0 0  */")), 0);
    CHECK_EQ_I64(cctool_prefix_len(AVP_S("/* 0 0  1 */")), 12);
    CHECK_EQ_I64(cctool_prefix_len(AVP_S("    li r3, 0x0")), 0);
}

static const char LISTING[] =
    ".text\n"
    ".fn foo, global\n"
    "/* 00000000 00000000  38 60 00 00 */\tli r3, 0x0\n"
    ".L_00000004:\n"
    "/* 00000004 00000004  48 00 00 01 */\tbl fn_80001234   \n"
    ".endfn foo\n"
    ".fn bar, local\n"
    "/* 00000008 00000008  4E 80 00 20 */\tblr\n"
    ".endfn bar\n";

AVP_TEST(filter_listing) {
    AvpArena *a = avp_arena();
    CHECK_STR(cctool_filter_listing(a, avp_str(LISTING), NULL, false),
              "foo:\n    li r3, 0x0\n.L_00000004:\n    bl fn_80001234\nbar:\n    blr");
    AvpSet *only = avp_set_new();
    avp_set_add(only, AVP_S("bar"));
    CHECK_STR(cctool_filter_listing(a, avp_str(LISTING), only, true), "bar:\n/* 00000008 00000008  4E 80 00 20 */\tblr");
}

AVP_TEST(norm) {
    AvpArena *a = avp_arena();
    CHECK_STR(cctool_norm(a, AVP_S("/* 0 0  48 */\tbl   fn_801A9940__Fv")), "bl SYM__Fv");
    CHECK_STR(cctool_norm(a, AVP_S("    lwz r4, lbl_8052A574@sda21(r13)")), "lwz r4, SYM@sda21(r13)");
    CHECK_STR(cctool_norm(a, AVP_S("    bne .L_80006A1C")), "bne L");
    CHECK_STR(cctool_norm(a, AVP_S("    bl xfn_80001234")), "bl xfn_80001234");
    CHECK_STR(cctool_norm(a, AVP_S("    bl fn_x")), "bl fn_x");
    CHECK_STR(cctool_norm(a, AVP_S(".L_80006A1C:")), "");
    CHECK_STR(cctool_norm(a, AVP_S("  # comment")), "");
    CHECK_STR(cctool_norm(a, AVP_S("   ")), "");
}

AVP_TEST(split_fns_and_retail_block) {
    AvpArena *a = avp_arena();
    AvpMap *fns = cctool_split_fns(a, AVP_S("foo:\n    li r3, 0x0\n.L_1:\nbar:\n    blr"));
    REQUIRE(avp_map_len(fns) == 2);
    AvpStrVec *foo = avp_map_get(fns, AVP_S("foo"))->p;
    CHECK_EQ_I64(foo->n, 2);
    AvpStr body;
    CHECK(cctool_retail_has(avp_str(LISTING), AVP_S("foo")));
    CHECK(!cctool_retail_has(avp_str(LISTING), AVP_S("fo")));
    REQUIRE(cctool_retail_block(avp_str(LISTING), AVP_S("bar"), &body));
    CHECK_STR(body, "\n/* 00000008 00000008  4E 80 00 20 */\tblr\n");
    CHECK_STR(cctool_positional(a, AVP_S("foo"), foo, avp_str("\n    li r3, 0x1\n    blr\n")),
              "foo: 2/2 rows differ\n"
              "    0 li r3, 0x1                       | li r3, 0x0\n"
              "    1                                  | ");
}

AVP_TEST(fn_prefix) {
    CHECK_STR(cctool_fn_prefix(AVP_S("fn_801A9AE8__Fv")), "fn_801A9AE8");
    CHECK_STR(cctool_fn_prefix(AVP_S("fn_801a9AE8__Fv")), "");
    CHECK_STR(cctool_fn_prefix(AVP_S("fn_801A9AE")), "");
}

/* ---- the real tree: the process-level checks (skipped without a checkout that has built) ---- */

static AvpProcResult run_tool(const char *exe, const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **argv = AVP_NEWN(a, const char *, n + 2);
    argv[0] = avp_join(a, avp_exe_dir(), "..", exe);
    for (size_t i = 0; i < n; i++) argv[i + 1] = args[i];
    return avp_run_text(a, avp_root(), argv);
}

static bool have_tree(void) {
    AvpArena *a = avp_arena();
    return avp_isfile(avp_join(a, avp_root(), "build.ninja")) && avp_isfile(avp_join(a, avp_root(), "src", "fmod", "fmod.cpp"));
}

AVP_TEST(real_unit_names_flag_values_and_the_kept_object) {
    if (!have_tree()) return;
    AvpArena *a = avp_arena();
    const char *obj = avp_join(a, avp_root(), "build", "tmp", "cc_test_keep.o");
    avp_remove(obj);
    AvpProcResult r = run_tool("cc.exe", (const char *[]){"src/fmod/fmod.cpp", "-u", "main/fmod/fmod", "-f", "-DCC_TEST=1", "-o", obj, "-s", "FMOD_Channel_GetUserData"}, 9);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_isfile(obj));
    r = run_tool("cc.exe", (const char *[]){"src/fmod/fmod.cpp", "-u", "fmod/fmod", "-s", "FMOD_Channel_GetUserData"}, 5);
    CHECK_EQ_I64(r.returncode, 0);
}

AVP_TEST(real_file_macro_sets_the_compiled_file_name) {
    if (!have_tree()) return;
    AvpArena *a = avp_arena();
    const char *dir = avp_join(a, avp_root(), "build", "tmp", "cc_test");
    AVP_ASSERT(avp_makedirs(dir, true));
    const char *src = avp_join(a, dir, "probe.cpp");
    const char *obj = avp_join(a, dir, "probe.o");
    AVP_ASSERT(avp_write_text(src, AVP_S("const char* probe_file = __FILE__;\nint probe_line = __LINE__;\n"), AVP_TM_UTF8_NL_LF));
    avp_remove(obj);
    AvpProcResult r = run_tool("cc.exe", (const char *[]){src, "-u", "main/fmod/fmod", "--file-macro", "z:/probe/unit_file.cpp", "-o", obj}, 7);
    CHECK_EQ_I64(r.returncode, 0);
    AvpBytes b = {0};
    AVP_ASSERT(avp_read_all(a, obj, &b));
    AvpStr text = avp_strn((const char *)b.v, b.n);
    CHECK(avp_str_contains(text, AVP_S("unit_file.cpp")));
    avp_remove(obj);
    r = run_tool("cc.exe", (const char *[]){src, "-u", "main/fmod/fmod", "-o", obj}, 5);
    CHECK_EQ_I64(r.returncode, 0);
    AVP_ASSERT(avp_read_all(a, obj, &b));
    CHECK(!avp_str_contains(avp_strn((const char *)b.v, b.n), AVP_S("unit_file.cpp")));
}

/* the row count of cc.exe --cmp is match.exe's: both align with cc_align */
static bool first_int_before(AvpStr text, const char *marker, i64 *out) {
    i64 at = avp_str_find(text, avp_str(marker), 0, AVP_END);
    if (at < 1) return false;
    i64 e = at, s = at;
    while (s > 0 && text.p[s - 1] >= '0' && text.p[s - 1] <= '9') s--;
    return s < e && avp_parse_int(avp_strn(text.p + s, (size_t)(e - s)), 10, out, NULL);
}

AVP_TEST(real_cmp_counts_agree_with_match) {
    if (!have_tree()) return;
    AvpArena *a = avp_arena();
    AvpProcResult m = run_tool("match.exe", (const char *[]){"main/fmod/fmod_eventgroupi", "stealEventInstance"}, 2);
    if (m.returncode > 1) return;
    i64 want = -1, got = -1;
    if (!first_int_before(m.out, " rows", &want)) return;
    AvpProcResult c = run_tool("cc.exe", (const char *[]){"src/fmod/fmod_eventgroupi.cpp", "-u", "main/fmod/fmod_eventgroupi", "--cmp", "-s", "stealEventInstance__Q24FMOD11EventGroupIFPQ24FMOD6EventIPPQ24FMOD6EventIb"}, 7);
    CHECK_EQ_I64(c.returncode, 0);
    REQUIRE(first_int_before(c.out, "/", &got));
    CHECK_EQ_I64(got, want);
    (void)a;
}

AVP_TEST_MAIN("cc")
