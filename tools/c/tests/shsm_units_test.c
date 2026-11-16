/* shsm_units and shsm_repo on a fixture checkout whose path holds spaces: configure.py statuses, build.ninja edges,
 * objdiff.json units, get_unit errors, compile_argv. */
#include "shsm_units.h"
#include "shsm_test_util.h"

static const char CONFIGURE[] =
    "DEFAULT_VERSION = 0\n"
    "VERSIONS = [\n"
    "    \"TEST01\",  # 0: the test\n"
    "]\n"
    "libs = [\n"
    "    Object(Matching, \"game/A.cpp\"),\n"
    "    Object( NonMatching ,\n        \"game/B.c\" ),\n"
    "    Object(MatchingFor(\"TEST01\"), \"game/C.cpp\"),\n"
    "    Object(Equivalent, \"game/D.cpp\"),\n"
    "    Object(Bogus, \"game/E.cpp\"),\n"
    "    Object(Matching, \"game/F.cpp\"),\n"
    "    Object(NonMatching, \"game/A.cpp\"),\n"
    "]\n";

static const char NINJA[] =
    "rule mwcc\n  command = x\n"
    "# game/A.cpp: game (linked True)\n"
    "build build\\TEST01\\src\\game\\A.o: mwcc_sjis src\\game\\A.cpp | $\n"
    "    tools\\compilers\\$mw_version\\mwcceppc.exe || $\n"
    "    pre-compile\n"
    "  mw_version = Wii\\1.0\n"
    "  cflags = -O4,p -pragma \"cats off\" -i include $\n"
    "      -DX=1 -lang=c++\n"
    "  basedir = build\\TEST01\\src\\game\n"
    "# game/B.c: sdk (linked\n"
    "# False)\n"
    "build build\\TEST01\\src\\game\\B.o: mwcc src\\game\\B.c\n"
    "  mw_version = GC/3.0a5\n"
    "  cflags = -O3 '-i' \"dir with spaces\" a$$b \"q\\\"x\" e\\ f\n"
    "build build\\TEST01\\src\\game\\B.ctx: decompctx src\\game\\B.c\n"
    "# game/C.cpp: game (linked False)\n"
    "build build\\TEST01\\src\\game\\C.o: mwcc src\\game\\C.cpp\n"
    "  mw_version = Wii/1.0\n"
    "  cflags = -O3\n";

static const char OBJDIFF[] =
    "{\"units\": [\n"
    " {\"name\": \"main/game/A\", \"target_path\": \"build/TEST01/obj/game/A.o\", \"base_path\": "
    "\"build/TEST01/src/game/A.o\", \"metadata\": {\"source_path\": \"src/game/A.cpp\", \"complete\": true, "
    "\"progress_categories\": [\"game\"]}},\n"
    " {\"name\": \"main/game/B\", \"target_path\": \"build/TEST01/obj/game/B.o\", \"base_path\": "
    "\"build/TEST01/src/game/B.o\", \"metadata\": {}},\n"
    " {\"name\": \"main/auto_x\", \"target_path\": \"build/TEST01/obj/auto_x.o\", \"metadata\": {\"auto_generated\": "
    "true}}\n]}\n";

static const char *make_fixture(void) {
    const char *d = fx_dir("units fixture");
    fx_write(d, "configure.py", CONFIGURE);
    fx_write(d, "build.ninja", NINJA);
    fx_write(d, "objdiff.json", OBJDIFF);
    fx_write(d, "config/TEST01/config.yml", "object: orig/TEST01/sys/main.dol\nobject_base: \"base dir\"  # c\n");
    fx_write(d, "config/TEST01/splits.txt", "game/S.c:\n\t.text start:0x10 end:0x20\n");
    fx_write(d, "tools/README", "x\n");
    fx_write(d, "build/tools/sjiswrap.exe", "");
    fx_write(d, "src/game/A.cpp", "int a;\n");
    return d;
}

AVP_TEST(configure_status) {
    ShsmConfStatus *st = NULL;
    size_t n = shsm_parse_configure_status(avp_arena(), avp_str(CONFIGURE), &st);
    REQUIRE(n == 5);
    CHECK_CZ(st[0].path, "game/A");
    CHECK_CZ(st[0].status, "NonMatching"); /* the later entry wins, at the first place */
    CHECK_CZ(st[1].status, "NonMatching");
    CHECK_CZ(st[2].status, "MatchingFor");
    CHECK_CZ(st[3].status, "Equivalent");
    CHECK_CZ(st[4].path, "game/F");
}

AVP_TEST(ninja_compiles) {
    ShsmNinjaCompile *nj = NULL;
    size_t n = shsm_parse_ninja_compiles(avp_arena(), avp_str(NINJA), &nj);
    REQUIRE(n == 3);
    CHECK_CZ(nj[0].object, "build/TEST01/src/game/A.o");
    CHECK_CZ(nj[0].rule, "mwcc_sjis");
    CHECK_CZ(nj[0].mw_version, "Wii/1.0");
    CHECK(nj[0].linked);
    REQUIRE(nj[0].cflags.n == 7);
    CHECK_STR(nj[0].cflags.v[2], "cats off");
    CHECK_STR(nj[0].cflags.v[5], "-DX=1");
    CHECK_CZ(nj[1].library, "sdk");
    CHECK(!nj[1].linked);
    REQUIRE(nj[1].cflags.n == 6);
    CHECK_STR(nj[1].cflags.v[2], "dir with spaces");
    CHECK_STR(nj[1].cflags.v[3], "a$b");
    CHECK_STR(nj[1].cflags.v[4], "q\"x");
    CHECK_STR(nj[1].cflags.v[5], "e f");
    CHECK_STR(shsm_join_ninja_lines(avp_arena(), AVP_S("a $\n   b $\r\n\n c$\nd")), "a b c d");
    CHECK_RAISES(shsm_parse_ninja_compiles(avp_arena(), AVP_S("# a.c: l (linked True)\nbuild x: mwcc a.c\n  cflags = \"x\n"),
                                           &nj),
                 SHSM_EXC_VALUE, "No closing quotation");
}

AVP_TEST(units_and_errors) {
    const char *root = make_fixture();
    ShsmRepo *r = shsm_repo_find(avp_join(avp_arena(), root, "src", "game"));
    CHECK_CZ(r->root, root);
    CHECK_CZ(r->version, "TEST01");
    CHECK_CZ(r->retail_dol, avp_join(avp_arena(), root, "base dir", "orig", "TEST01", "sys", "main.dol"));
    const ShsmUnits *us = shsm_load_units(r);
    REQUIRE(us->n == 6);
    CHECK_CZ(us->v[0]->name, "main/game/A");
    CHECK_CZ(us->v[0]->status, "NonMatching");
    CHECK_CZ(us->v[2]->status, "Asm");
    CHECK_CZ(us->v[3]->name, "main/game/C");
    const ShsmUnit *a = shsm_get_unit(r, "game/A.cpp");
    CHECK_CZ(a->library, "game");
    CHECK(a->complete);
    CHECK_RAISES(shsm_get_unit(r, "game/Z"), SHSM_EXC_KEY,
                 "unknown unit game/Z (not in objdiff.json, configure.py or splits.txt)");
    CHECK_RAISES(shsm_get_unit(r, "game/S.c"), SHSM_EXC_UNIT_NOT_BUILT,
                 "unit game/S.c is in splits.txt but not in build.ninja (added since build.ninja was generated); "
                 "regenerate it with `tools/bin/ninja.exe build.ninja`");
    CHECK_RAISES(shsm_get_unit(r, "main/game/F"), SHSM_EXC_UNIT_NOT_BUILT,
                 "unit main/game/F is in configure.py but not in build.ninja (added since build.ninja was generated); "
                 "regenerate it with `tools/bin/ninja.exe build.ninja`");
    const char *work = avp_join(avp_arena(), root, "work dir");
    const char *src = avp_join(avp_arena(), root, "src", "game", "A.cpp");
    AvpStrVec argv = shsm_compile_argv(avp_arena(), r, a, src, shsm_object_for(avp_arena(), src, work),
                                       &(ShsmCompileOpts){.sjis = -1, .depfile = true});
    REQUIRE(argv.n >= 4);
    CHECK_STR(argv.v[0], r->sjiswrap);
    CHECK_STR(argv.v[argv.n - 1], work);
    const char *bad = avp_join(avp_arena(), work, "other.o");
    char msg[1024];
    snprintf(msg, sizeof msg, "with depfile=True MWCC names the object after the source: %s must be <dir>/A.o", bad);
    CHECK_RAISES(shsm_compile_argv(avp_arena(), r, a, src, bad, &(ShsmCompileOpts){.sjis = -1, .depfile = true}),
                 SHSM_EXC_VALUE, msg);
    CHECK_RAISES(shsm_compile_argv(avp_arena(), r, us->v[2], src, work, NULL), SHSM_EXC_VALUE,
                 "unit main/auto_x has no compile command in build.ninja (asm unit, or regenerate it with "
                 "`tools/bin/ninja.exe build.ninja`)");
    AvpStrVec plain = shsm_compile_argv(avp_arena(), r, a, src, bad,
                                        &(ShsmCompileOpts){.sjis = 0, .drop_flags = (const char *[]){"-O4,p"},
                                                           .ndrop = 1, .extra_flags = (const char *[]){"-inline", "off"},
                                                           .nextra = 2, .mw_version = "Wii/1.1"});
    CHECK_STR(plain.v[0], avp_join(avp_arena(), root, "tools", "compilers", "Wii", "1.1", "mwcceppc.exe"));
    CHECK_STR(plain.v[1], "-pragma");
    CHECK_STR(plain.v[plain.n - 1], bad);
    CHECK_STR(plain.v[plain.n - 5], "off");
}

AVP_TEST(read_version_fallbacks) {
    const char *d = fx_dir("version fallback");
    fx_write(d, "configure.py", "VERSIONS = [\n    \"V1\",  # 0: comment with [brackets]\n]\n");
    fx_write(d, "config/V2/config.yml", "x: 1\n");
    fx_write(d, "config/other/readme", "x\n");
    CHECK_CZ(shsm_read_version(d), "V2"); /* the lazy match ends inside the comment: literal_eval fails */
    fx_write(d, "configure.py", "DEFAULT_VERSION = 1\nVERSIONS = ['A', \"B\" 'C',]\n");
    CHECK_CZ(shsm_read_version(d), "BC");
    fx_write(d, "configure.py", "DEFAULT_VERSION = 5\nVERSIONS = ['A']\n");
    CHECK_CZ(shsm_read_version(d), "V2");
    fx_write(d, "config/V3/config.yml", "x: 1\n");
    char msg[1024];
    snprintf(msg, sizeof msg, "cannot determine the version name in %s", d);
    CHECK_RAISES(shsm_read_version(d), SHSM_EXC_VALUE, msg);
    CHECK_RAISES(shsm_find_root("C:\\"), SHSM_EXC_FILE_NOT_FOUND,
                 "not inside a checkout (no configure.py/config/tools): C:\\");
}

AVP_TEST(repo_paths) {
    AvpArena *a = avp_arena();
    CHECK_CZ(shsm_unit_path(a, "main/game/PadRumble"), "game/PadRumble");
    CHECK_CZ(shsm_unit_path(a, "game\\PadRumble.cpp"), "game/PadRumble");
    CHECK_CZ(shsm_unit_path(a, "a/b.C"), "a/b");
    CHECK_CZ(shsm_unit_path(a, "a.b/c"), "a.b/c");
    CHECK_CZ(shsm_unit_path(a, "x.h"), "x.h");
    CHECK_CZ(shsm_unit_name(a, "game/A.cpp"), "main/game/A");
    ShsmRepo r = {0};
    r.root = "X:\\r oot";
    r.bin_dir = "X:\\r oot\\tools\\bin";
    r.compilers_dir = "X:\\r oot\\tools\\compilers";
    r.asm_dir = "X:\\r oot\\build\\V\\asm";
    CHECK_CZ(shsm_repo_tool(a, &r, "dtk"), "X:\\r oot\\tools\\bin\\dtk.exe");
    CHECK_CZ(shsm_repo_tool(a, &r, "dtk.exe"), "X:\\r oot\\tools\\bin\\dtk.exe");
    CHECK_CZ(shsm_repo_compiler(a, &r, "Wii\\1.0", NULL), "X:\\r oot\\tools\\compilers\\Wii\\1.0\\mwcceppc.exe");
    CHECK_CZ(shsm_repo_asm_path(a, &r, "main/game/PadRumble"), "X:\\r oot\\build\\V\\asm\\game\\PadRumble.s");
    CHECK_CZ(shsm_repo_rel(a, &r, "X:\\r oot\\src\\x.c"), "src/x.c");
    CHECK_RAISES(shsm_repo_compiler(a, &r, NULL, NULL), SHSM_EXC_TYPE, NULL);
}

AVP_TEST_MAIN("shsm_units")
