/* tools/test_unit.py NewTest ported to C: `unit.exe new` on the synthetic repository.
 *
 * Mapping (test_unit.py NewTest.test_X -> here X): regen_command, regen_removes_stale_auto_listings,
 * regen_without_split_record_removes_nothing, infers_ranges, configure_and_files, confirmed_omits_comment,
 * dry_run_writes_nothing, data_none, include_and_exclude_data, never_overwrites, refuses_text_overlap,
 * refuses_straddled_function, refuses_existing_unit, refuses_data_overlap, end_rule_with_asm_neighbour,
 * other_library_and_c_style, object_in_an_existing_split_is_left_to_it,
 * exclude_data_names_the_owner_to_come, data_only_unit_takes_the_excluded_objects,
 * data_only_unit_reuses_an_existing_header, data_only_unit_needs_the_objects_it_defines,
 * data_only_position_keeps_sections_ascending, unknown_lib.
 *
 * The Python mocked unit.regen; here <root>/tools/bin/ninja.exe is a copy of this test executable (fixture.h). */
#include "fixture.h"

#define NEW0(r) RUN(r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040")
#define NEWX(r, ...) RUN(r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040", __VA_ARGS__)

static const char *SPLITS_REL = "config/R5WEA4/splits.txt";

static AvpStr cat(AvpStr x, const char *y) { return avp_str_concat(fx_a(), x, avp_str(y)); }

AVP_TEST(regen_command) {
    Repo r = repo_new(false);
    repo_install_fake_ninja(&r, "");
    Cli c = NEWX(r, "--regen");
    CHECK_RC(c, 0);
    CHECK_EQ_I64(fx_ninja_calls(&r), 1);
    CHECK_IN(c.out, "tools/bin/ninja.exe build.ninja: ok");
    repo_cleanup(&r);
}

AVP_TEST(regen_removes_stale_auto_listings) {
    Repo r = repo_new(false);
    write_stale_listing(r.root);
    write_stale_listing_at(r.root, "game/Old.s"); /* stale but not an auto_* unit: named and kept */
    repo_install_fake_ninja(&r, "record");         /* the new split's record */
    Cli c = NEWX(r, "--regen");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "removed stale asm listing: build/R5WEA4/asm/auto_03_80010000_text.s");
    CHECK(!fx_exists(r.root, "build/R5WEA4/asm/auto_03_80010000_text.s"));
    CHECK_IN(c.out, "stale asm listing kept (not an auto_* unit): game/Old.s");
    CHECK(fx_exists(r.root, "build/R5WEA4/asm/game/Old.s"));
    for (size_t i = 0; i < 4; i++) {
        const char *rel = avp_sprintf(fx_a(), "build/R5WEA4/asm/%s.s", FX_LISTINGS[i]);
        CHECK_MSG(fx_exists(r.root, rel), "%s is gone", rel);
    }
    repo_cleanup(&r);
}

AVP_TEST(regen_without_split_record_removes_nothing) {
    Repo r = repo_new(false);
    write_stale_listing(r.root);
    repo_install_fake_ninja(&r, "");
    Cli c = NEWX(r, "--regen");
    CHECK_RC(c, 0);
    CHECK(fx_exists(r.root, "build/R5WEA4/asm/auto_03_80010000_text.s"));
    CHECK_IN(c.out, "stale asm listings not removed: no usable split record");
    repo_cleanup(&r);
}

AVP_TEST(infers_ranges) {
    Repo r = repo_new(false);
    Cli c = NEW0(r);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "`tools/bin/ninja.exe build.ninja` (or rerun with --regen)");
    AvpStr sp = fx_read(r.root, SPLITS_REL);
    CHECK_IN(sp, "game/CFooTest.cpp:\n"
                 "\t.text       start:0x80010000 end:0x80010040\n"
                 "\t.ctors      start:0x803D8368 end:0x803D836C\n"
                 "\t.data       start:0x803E0000 end:0x803E002C\n"
                 "\t.sbss       start:0x80500000 end:0x8050000C\n"
                 "\t.sdata2     start:0x80600000 end:0x80600004\n");
    /* inserted between Other and After (address order) */
    CHECK_BEFORE(sp, "game/Other.cpp", "game/CFooTest.cpp");
    CHECK_BEFORE(sp, "game/CFooTest.cpp", "game/After.cpp");
    /* the shared object inside the hull is taken, the adjacent shared one is reported and not taken */
    CHECK_IN(c.out, "lbl_80500004: shared with fn_80010040 but inside the hull");
    CHECK_IN(c.out, "lbl_80500010: shared with fn_80000000, outside the hull (ADJACENT, ambiguous): NOT taken");
    CHECK_IN(c.out, "gap_07_803E0028_data: unreferenced zero padding");
    CHECK_IN(c.out, "force_active: ");
    repo_cleanup(&r);
}

AVP_TEST(configure_and_files) {
    Repo r = repo_new(false);
    Cli c = NEW0(r);
    CHECK_RC(c, 0);
    AvpStr cfg = fx_read(r.root, "configure.py");
    CHECK_IN(cfg, "Object(NonMatching, \"game/CFooTest.cpp\"),");
    CHECK_BEFORE(cfg, "game/Other.cpp", "game/CFooTest.cpp");
    CHECK_BEFORE(cfg, "game/CFooTest.cpp", "a comment that belongs");
    CHECK_IN(cfg, "\r\n"); /* configure.py keeps its CRLF */
    AvpStr src = fx_read(r.root, "src/game/CFooTest.cpp");
    CHECK_IN(src, "\r\n"); /* new files follow the neighbours' line ending */
    src = fx_lf(src);
    CHECK(avp_str_startswith(src, AVP_S("// unit name unconfirmed")));
    CHECK_IN(src, "#include <game/CFooTest.h>");
    CHECK_IN(src, "/*\n====================\nCFooTest::CFooTest\n====================\n*/\n// 0x80010000\n"
                  "CFooTest::CFooTest() {\n    // TODO\n}\n");
    CHECK_IN(src, "// 0x80010010\nvoid CFooTest::fn_80010010( int ) {");
    CHECK_IN(src, "void CFooTest::fn_80010020( const CBarTest&, const char* ) {");
    /* functions in address order, the sinit has no stub */
    CHECK_BEFORE(src, "0x80010000", "0x80010010");
    CHECK_NOT_IN(src, "// 0x80010030");
    CHECK_IN(src, "Compiler-generated, no source of their own:");
    CHECK_IN(src, "//   .sbss 0x80500000 size 0x4 lbl_80500000");
    CHECK_NOT_IN(src, "gap_07");
    AvpStr hdr = fx_read_lf(r.root, "include/game/CFooTest.h");
    CHECK_IN(hdr, "#ifndef GAME_CFOOTEST_H");
    CHECK_IN(hdr, "class CBarTest;");
    CHECK_IN(hdr, "class CFooTest {");
    CHECK_IN(hdr, "    CFooTest();");
    CHECK_IN(hdr, "    void fn_80010010( int );  // in a vtable-like data object");
    repo_cleanup(&r);
}

AVP_TEST(confirmed_omits_comment) {
    Repo r = repo_new(false);
    Cli c = NEWX(r, "--confirmed");
    CHECK_RC(c, 0);
    CHECK(!avp_str_startswith(fx_read(r.root, "src/game/CFooTest.cpp"), AVP_S("// unit name unconfirmed")));
    repo_cleanup(&r);
}

AVP_TEST(dry_run_writes_nothing) {
    Repo r = repo_new(false);
    AvpStr b_splits = fx_read(r.root, SPLITS_REL), b_cfg = fx_read(r.root, "configure.py");
    Cli c = NEWX(r, "--dry-run");
    CHECK_RC(c, 0);
    CHECK_BYTES_EQ(b_splits, fx_read(r.root, SPLITS_REL));
    CHECK_BYTES_EQ(b_cfg, fx_read(r.root, "configure.py"));
    CHECK(!fx_exists(r.root, "src/game/CFooTest.cpp"));
    repo_cleanup(&r);
}

AVP_TEST(data_none) {
    Repo r = repo_new(false);
    Cli c = NEWX(r, "--data", "none");
    CHECK_RC(c, 0);
    CHECK_IN(fx_read(r.root, SPLITS_REL), "game/CFooTest.cpp:\n\t.text       start:0x80010000 end:0x80010040\n\n");
    repo_cleanup(&r);
}

AVP_TEST(include_and_exclude_data) {
    Repo r = repo_new(false);
    Cli c = NEWX(r, "--include-data", "lbl_80500010");
    CHECK_RC(c, 0);
    CHECK_IN(fx_read(r.root, SPLITS_REL), ".sbss       start:0x80500000 end:0x80500014");
    repo_cleanup(&r);
}

AVP_TEST(never_overwrites) {
    Repo r = repo_new(false);
    fx_put(r.root, "src/game/CFooTest.cpp", "mine\n", "\n");
    AvpStr before = fx_read(r.root, SPLITS_REL);
    Cli c = NEW0(r);
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "already exists");
    CHECK_STR(fx_read(r.root, "src/game/CFooTest.cpp"), "mine\n");
    CHECK_BYTES_EQ(before, fx_read(r.root, SPLITS_REL)); /* nothing half-written */
    repo_cleanup(&r);
}

/* a header that exists before its source (include/climax/Audio/System.h) is left alone; the stub is written */
AVP_TEST(existing_header_is_left_alone) {
    Repo r = repo_new(false);
    fx_put(r.root, "include/game/CFooTest.h", "// mine\n", "\n");
    Cli c = NEWX(r, "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "include/game/CFooTest.h already exists: left as it is");
    CHECK(!fx_exists(r.root, "src/game/CFooTest.cpp"));
    c = NEW0(r);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "include/game/CFooTest.h already exists: left as it is; only the source stub is written");
    CHECK_STR(fx_read(r.root, "include/game/CFooTest.h"), "// mine\n");
    CHECK_IN(fx_read(r.root, "src/game/CFooTest.cpp"), "#include <game/CFooTest.h>");
    CHECK_IN(fx_read(r.root, "configure.py"), "Object(NonMatching, \"game/CFooTest.cpp\"),");
    repo_cleanup(&r);
}

AVP_TEST(refuses_text_overlap) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/Dup.cpp", "--text", "0x80010040-0x80010060");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "overlaps game/After.cpp");
    CHECK(!fx_exists(r.root, "src/game/Dup.cpp"));
    repo_cleanup(&r);
}

AVP_TEST(refuses_straddled_function) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/Cut.cpp", "--text", "0x80010000-0x80010038");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "straddles");
    repo_cleanup(&r);
}

AVP_TEST(refuses_existing_unit) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/Other.cpp", "--text", "0x80010000-0x80010040");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "already has a split");
    repo_cleanup(&r);
}

AVP_TEST(refuses_data_overlap) {
    Repo r = repo_new(false);
    /* a split that already claims .sbss inside the inferred hull */
    AvpStr sp = cat(fx_read(r.root, SPLITS_REL), "\ngame/Claim.cpp:\n\t.sbss       start:0x80500004 end:0x80500008\n");
    fx_put(r.root, SPLITS_REL, avp_cz(fx_a(), sp), "\n");
    Cli c = NEW0(r);
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.out, ".sbss");
    CHECK_IN(c.out, "overlaps game/Claim.cpp");
    CHECK(!fx_exists(r.root, "src/game/CFooTest.cpp"));
    repo_cleanup(&r);
}

AVP_TEST(end_rule_with_asm_neighbour) {
    Repo r = repo_new(false);
    /* fn_80010060 ends at 0x8001006C; 0x80010070 starts an asm function */
    Cli c = RUN(r, "new", "game/Pad.cpp", "--text", "0x80010060-0x80010070");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.out, "must end its .text at its last instruction (use END=0x8001006C)");
    c = RUN(r, "new", "game/Pad.cpp", "--text", "0x80010060-0x8001006C");
    CHECK_RC(c, 0);
    CHECK_IN(fx_read(r.root, "src/game/Pad.cpp"), "void fn_80010060()");
    repo_cleanup(&r);
}

AVP_TEST(other_library_and_c_style) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "Runtime/b.c", "--text", "0x80010060-0x8001006C", "--lib", "Runtime");
    CHECK_RC(c, 0);
    AvpStr cfg = fx_read(r.root, "configure.py");
    CHECK_BEFORE(cfg, "Runtime/a.c", "Runtime/b.c");
    AvpStr src = fx_read_lf(r.root, "src/Runtime/b.c");
    CHECK_IN(src, "void fn_80010060() {\n    // TODO\n} // 0x80010060");
    CHECK(!fx_exists(r.root, "include/Runtime/b.h"));
    c = RUN(r, "new", "zzz/c.c", "--text", "0x80010070-0x80010080");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "cannot infer the library");
    repo_cleanup(&r);
}

AVP_TEST(object_in_an_existing_split_is_left_to_it) {
    Repo r = repo_new(false);
    AvpStr sp = cat(fx_read(r.root, SPLITS_REL), "\ngame/TheCfg.cpp:\n\t.sdata2     start:0x80600000 end:0x80600004\n");
    fx_put(r.root, SPLITS_REL, avp_cz(fx_a(), sp), "\n");
    Cli c = NEWX(r, "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "data owners: ");
    CHECK_IN(c.out, "other unit: lbl_80600000 (.sdata2 0x80600000, size 0x4): already in split game/TheCfg.cpp");
    CHECK_NOT_IN(c.out, "\t.sdata2");
    c = RUN(r, "new", "game/Pad.cpp", "--text", "0x80010060-0x8001006C", "--include-data", "lbl_80600000");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "--include-data lbl_80600000: .sdata2 0x80600000 already belongs to split game/TheCfg.cpp");
    repo_cleanup(&r);
}

AVP_TEST(exclude_data_names_the_owner_to_come) {
    Repo r = repo_new(false);
    Cli c = NEWX(r, "--dry-run", "--exclude-data", "lbl_80600000");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "other unit: lbl_80600000 (.sdata2 0x80600000, size 0x4): excluded by --exclude-data "
                    "(give it to its unit with `unit.exe new <path> --include-data lbl_80600000`)");
    CHECK_NOT_IN(c.out, "\t.sdata2");
    CHECK_IN(c.out, "1 left to other units");
    CHECK_IN(c.out, "1 shared and not taken"); /* lbl_80500010 is next to the .sbss hull; 80500004 is taken */
    CHECK_IN(c.out, "not taken: lbl_80500010 (shared; --include-data lbl_80500010 to take it)");
    c = NEWX(r, "--exclude-data", "no_such_symbol");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "--exclude-data no_such_symbol: no such symbol");
    repo_cleanup(&r);
}

AVP_TEST(data_only_unit_takes_the_excluded_objects) {
    Repo r = repo_new(false);
    Cli c = NEWX(r, "--exclude-data", "lbl_80600000");
    CHECK_RC(c, 0);
    {
        AvpStr sp = fx_read(r.root, SPLITS_REL);
        i64 after = fx_pos(sp, "game/After.cpp"), mine = fx_pos(sp, "game/CFooTest.cpp:");
        REQUIRE(after >= 0 && mine >= 0);
        CHECK_NOT_IN(avp_str_slice(avp_str_slice(sp, 0, after), mine + (i64)strlen("game/CFooTest.cpp:"), AVP_END),
                     ".sdata2");
    }
    c = RUN(r, "new", "game/FooCfg.cpp", "--include-data", "lbl_80600000");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "data-only unit (no .text)");
    CHECK_IN(c.out, "read by game/CFooTest.cpp: the new unit is placed after it");
    AvpStr sp = fx_read(r.root, SPLITS_REL);
    CHECK_IN(sp, "game/FooCfg.cpp:\n\t.sdata2     start:0x80600000 end:0x80600004\n");
    CHECK_BEFORE(sp, "game/CFooTest.cpp", "game/FooCfg.cpp");
    CHECK_BEFORE(sp, "game/FooCfg.cpp", "game/After.cpp");
    AvpStr cfg = fx_read(r.root, "configure.py");
    CHECK_IN(cfg, "Object(NonMatching, \"game/FooCfg.cpp\"),");
    CHECK_BEFORE(cfg, "game/CFooTest.cpp", "game/FooCfg.cpp");
    CHECK_BEFORE(cfg, "game/FooCfg.cpp", "a comment that belongs");
    AvpStr src = fx_read(r.root, "src/game/FooCfg.cpp");
    CHECK_IN(src, "//   .sdata2 0x80600000 size 0x4 lbl_80600000");
    CHECK_IN(src, "#include <game/FooCfg.h>");
    CHECK_IN(fx_read(r.root, "include/game/FooCfg.h"), "#ifndef GAME_FOOCFG_H"); /* the include resolves */
    /* the first unit's split is not disturbed and `check` accepts both */
    c = RUN(r, "check", "game/FooCfg.cpp");
    CHECK_NOT_IN(c.out, "[ERROR]");
    repo_cleanup(&r);
}

AVP_TEST(data_only_unit_reuses_an_existing_header) {
    Repo r = repo_new(false);
    CHECK_EQ_I64(NEWX(r, "--exclude-data", "lbl_80600000").rc, 0);
    fx_put(r.root, "include/game/FooCfg.h", "// mine\n", "\n");
    Cli c = RUN(r, "new", "game/FooCfg.cpp", "--include-data", "lbl_80600000");
    CHECK_RC(c, 0);
    CHECK_STR(fx_read(r.root, "include/game/FooCfg.h"), "// mine\n");
    CHECK_IN(fx_read(r.root, "src/game/FooCfg.cpp"), "#include <game/FooCfg.h>");
    repo_cleanup(&r);
}

AVP_TEST(data_only_unit_needs_the_objects_it_defines) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/FooCfg.cpp");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "data-only");
    c = RUN(r, "new", "game/FooCfg.cpp", "--include-data", "lbl_80600000", "--data", "none");
    CHECK_EQ_I64(c.rc, 2);
    CHECK(!fx_exists(r.root, "src/game/FooCfg.cpp"));
    repo_cleanup(&r);
}

AVP_TEST(data_only_position_keeps_sections_ascending) {
    Repo r = repo_new(false);
    /* an earlier split holds a higher .sdata2 range: the new entry has to go before it, not after the reader */
    AvpStr sp = fx_replace(fx_read(r.root, SPLITS_REL),
                           "game/After.cpp:\n\t.text       start:0x80010040 end:0x80010060\n",
                           "game/After.cpp:\n\t.text       start:0x80010040 end:0x80010060\n"
                           "\t.sdata2     start:0x80600100 end:0x80600104\n");
    fx_put(r.root, SPLITS_REL, avp_cz(fx_a(), sp), "\n");
    CHECK_EQ_I64(NEWX(r, "--exclude-data", "lbl_80600000").rc, 0);
    Cli c = RUN(r, "new", "game/FooCfg.cpp", "--include-data", "lbl_80600000");
    CHECK_RC(c, 0);
    CHECK_BEFORE(fx_read(r.root, SPLITS_REL), "game/FooCfg.cpp", "game/After.cpp");
    repo_cleanup(&r);
}

AVP_TEST(unknown_lib) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/Pad.cpp", "--text", "0x80010060-0x8001006C", "--lib", "nope");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "no such library");
    repo_cleanup(&r);
}

static void with_sdk_libs(Repo *r) {
    AvpStr cfg = fx_read_lf(r->root, "configure.py");
    const char *head = "config.libs = [\n";
    const char *add = "config.libs = [\n"
                      "    RVLSDKLib(\"NdevExi2AD\", [Object(NonMatching, \"revolution/NdevExi2AD/n.c\")]),\n"
                      "    RVLSDKLib(\"os\", [Object(Matching, \"revolution/os/OSx.c\")]),\n";
    const char *text = avp_cz(fx_a(), cfg);
    const char *at = strstr(text, head);
    AVP_ASSERT(at != NULL);
    fx_put(r->root, "configure.py",
           avp_sprintf(fx_a(), "%.*s%s%s", (int)(at - text), text, add, at + strlen(head)), "\r\n");
}

AVP_TEST(infer_lib_new_sdk_module_is_refused) {
    Repo r = repo_new(false);
    with_sdk_libs(&r);
    Cli c = RUN(r, "new", "revolution/usb/usb.c", "--text", "0x80010060-0x8001006C");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "cannot infer the library");
    CHECK_IN(c.err, "RVLSDKLib(\"usb\")");
    CHECK(!fx_exists(r.root, "src/revolution/usb/usb.c"));
    repo_cleanup(&r);
}

AVP_TEST(infer_lib_file_in_existing_sdk_module) {
    Repo r = repo_new(false);
    with_sdk_libs(&r);
    Cli c = RUN(r, "new", "revolution/os/OSy.c", "--text", "0x80010060-0x8001006C");
    CHECK_RC(c, 0);
    AvpStr cfg = fx_read(r.root, "configure.py");
    CHECK_BEFORE(cfg, "revolution/os/OSx.c", "revolution/os/OSy.c");
    CHECK_BEFORE(cfg, "revolution/os/OSy.c", "Runtime/a.c");
    repo_cleanup(&r);
}

AVP_TEST(infer_lib_new_game_file_and_subdirectory) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/sub/Deep.cpp", "--text", "0x80010060-0x8001006C");
    CHECK_RC(c, 0);
    CHECK_BEFORE(fx_read(r.root, "configure.py"), "game/After.cpp", "game/sub/Deep.cpp");
    repo_cleanup(&r);
}

/* ---- assembly units, extensions, name evidence, resplit ---- */

AVP_TEST(asm_path_gets_an_assembly_stub) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/CAsmTest.s", "--text", "0x80010000-0x80010040", "--confirmed");
    CHECK_RC(c, 0);
    CHECK_IN(fx_read(r.root, "configure.py"), "Object(NonMatching, \"game/CAsmTest.s\"),");
    AvpStr src = fx_read_lf(r.root, "src/game/CAsmTest.s");
    CHECK_IN(src, "# ORIGINAL-ASM: TODO");
    CHECK_IN(src, ".include \"macros.inc\"\n\n.section .text, \"ax\"\n");
    CHECK_IN(src, "# CFooTest::CFooTest   // 0x80010000\n.fn __ct__8CFooTestFv, global\n");
    CHECK_IN(src, ".endfn __ct__8CFooTestFv\n");
    CHECK_IN(src, ".fn " FX_SINIT_NAME ", global");
    CHECK_NOT_IN(src, "#include");
    CHECK_NOT_IN(src, "{");
    CHECK(!fx_exists(r.root, "include/game/CAsmTest.h")); /* no header for assembly */
    /* check reads the assembler's comment syntax */
    c = RUN(r, "check", "game/CAsmTest.s");
    CHECK_NOT_IN(c.out, "no `// 0x8XXXXXXX` address comments");
    CHECK_NOT_IN(c.out, "no `# ... // 0x8XXXXXXX` address comments");
    CHECK_NOT_IN(c.out, "[ERROR] line");
    CHECK_IN(c.out, "address comments match symbols.txt");
    /* a comment above the wrong .fn is reported */
    AvpStr bad = fx_replace1(src, "# CFooTest::CFooTest   // 0x80010000", "# CFooTest::CFooTest   // 0x80010010");
    fx_put(r.root, "src/game/CAsmTest.s", avp_cz(fx_a(), bad), "\n");
    c = RUN(r, "check", "game/CAsmTest.s");
    CHECK_IN(c.out, "comment 0x80010010 is followed by `.fn __ct__8CFooTestFv` but symbols.txt names fn_80010010__8CFooTestFi");
    repo_cleanup(&r);
}

AVP_TEST(asm_path_without_confirmation_says_unconfirmed_in_hash_comments) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/CAsmTest.s", "--text", "0x80010000-0x80010040");
    CHECK_RC(c, 0);
    CHECK(avp_str_startswith(fx_read_lf(r.root, "src/game/CAsmTest.s"), AVP_S("# unit name unconfirmed")));
    repo_cleanup(&r);
}

AVP_TEST(unit_path_needs_a_source_extension) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/CNoExt", "--text", "0x80010000-0x80010040");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "has no file extension: a unit is a .c, .cpp, .cp or .s file");
    c = RUN(r, "new", "game/CFooTest.txt", "--text", "0x80010000-0x80010040");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "has the extension \".txt\": a unit is a .c, .cpp, .cp or .s file");
    CHECK(!fx_exists(r.root, "src/game/CFooTest.txt"));
    repo_cleanup(&r);
}

AVP_TEST(name_evidence_replaces_the_unconfirmed_comment) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040", "--name-evidence",
                "// Unit name and boundary: Gormiti link map (fmodex.a fmod_file.obj)");
    CHECK_RC(c, 0);
    AvpStr src = fx_read_lf(r.root, "src/game/CFooTest.cpp");
    CHECK(avp_str_startswith(src, AVP_S("// Unit name and boundary: Gormiti link map (fmodex.a fmod_file.obj)\n\n")));
    CHECK_NOT_IN(src, "unit name unconfirmed");
    /* the same text in a .s stub is a # comment */
    c = RUN(r, "new", "game/CAsmTest.s", "--text", "0x80010060-0x8001006C", "--name-evidence",
            "// Unit name and boundary: Gormiti link map");
    CHECK_RC(c, 0);
    CHECK(avp_str_startswith(fx_read_lf(r.root, "src/game/CAsmTest.s"), AVP_S("# Unit name and boundary: Gormiti link map\n")));
    repo_cleanup(&r);
}

AVP_TEST(resplit_rewrites_the_entry_of_a_parked_unit) {
    Repo r = repo_new(false);
    CHECK_RC(NEW0(r), 0);
    AvpStr cfg = fx_read(r.root, "configure.py"), src = fx_read(r.root, "src/game/CFooTest.cpp");
    Cli c = RUN(r, "resplit", "game/CFooTest.cpp", "--text", "0x80010000-0x80010030", "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "splits.txt entry (replaces the unit's current one):");
    CHECK_IN(c.out, "dry run: nothing written");
    CHECK_IN(fx_read(r.root, SPLITS_REL), "game/CFooTest.cpp:\n\t.text       start:0x80010000 end:0x80010040\n");
    c = RUN(r, "resplit", "game/CFooTest.cpp", "--text", "0x80010000-0x80010030");
    CHECK_RC(c, 0);
    AvpStr sp = fx_read(r.root, SPLITS_REL);
    CHECK_IN(sp, "game/CFooTest.cpp:\n\t.text       start:0x80010000 end:0x80010030\n");
    CHECK_NOT_IN(sp, "start:0x80010000 end:0x80010040");
    CHECK_IN(sp, "game/After.cpp:\n\t.text       start:0x80010040 end:0x80010060\n"); /* the neighbours stay */
    CHECK_BEFORE(sp, "game/Other.cpp", "game/CFooTest.cpp");
    CHECK_BEFORE(sp, "game/CFooTest.cpp", "game/After.cpp");
    CHECK_BYTES_EQ(cfg, fx_read(r.root, "configure.py"));
    CHECK_BYTES_EQ(src, fx_read(r.root, "src/game/CFooTest.cpp"));
    /* an --include-data of an object the old split held is free again */
    c = RUN(r, "resplit", "game/CFooTest.cpp", "--text", "0x80010000-0x80010030", "--include-data", "lbl_80500010",
            "--dry-run");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "forced by --include-data");
    repo_cleanup(&r);
}

AVP_TEST(resplit_refuses_a_matching_unit_and_an_unknown_one) {
    Repo r = repo_new(false);
    CHECK_RC(NEW0(r), 0);
    AvpStr cfg = fx_read(r.root, "configure.py");
    AvpStr matching = fx_replace(cfg, "NonMatching, \"game/CFooTest.cpp\"", "Matching, \"game/CFooTest.cpp\"");
    fx_put(r.root, "configure.py", avp_cz(fx_a(), matching), "\n");
    AvpStr before = fx_read(r.root, SPLITS_REL);
    Cli c = RUN(r, "resplit", "game/CFooTest.cpp", "--text", "0x80010000-0x80010030");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "game/CFooTest.cpp is Matching in configure.py; resplit only re-splits a parked (NonMatching) unit");
    CHECK_BYTES_EQ(before, fx_read(r.root, SPLITS_REL));
    c = RUN(r, "resplit", "game/Nothing.cpp", "--text", "0x80010000-0x80010030");
    CHECK_EQ_I64(c.rc, 2);
    CHECK_IN(c.err, "no split named");
    repo_cleanup(&r);
}

UNIT_TEST_MAIN("unit")
