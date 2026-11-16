/* tools/test_unit.py CheckTest ported to C: `unit.exe check` on a unit made by `unit.exe new`.
 *
 * Mapping (CheckTest.test_X -> here X): fresh_scaffold_passes_with_force_active_warning,
 * stale_listing_from_an_earlier_split_is_ignored, without_split_record_every_listing_is_read,
 * check_writes_only_the_documented_cache (the C port has no cache file, DEVIATIONS D-unit-1: the whole tree must be
 * unchanged), isolated_unit_needs_force_active, local_symbol_is_not_referenced_by_another_units_local_of_the_same_name,
 * global_symbol_read_by_outside_code_is_referenced_from_outside, wrong_address_comment_is_an_error,
 * name_mismatch_and_missing_banner_warn, overlap_and_end_rule_errors, end_after_last_instruction_before_asm_unit,
 * function_missing_from_symbols, functions_named_gap_are_functions_not_padding, unknown_unit,
 * accepts_alternative_spellings. */
#include "fixture.h"

static const char *SPLITS_REL = "config/R5WEA4/splits.txt";
static const char *SYMBOLS_REL = "config/R5WEA4/symbols.txt";
static const char *TEXT_ASM = "build/R5WEA4/asm/auto_03_80000000_text.s";

/* setUp: a repository with game/CFooTest.cpp made by `new` */
static Repo check_repo(void) {
    Repo r = repo_new(false);
    Cli c = RUN(r, "new", "game/CFooTest.cpp", "--text", "0x80010000-0x80010040");
    if (c.rc != 0) fprintf(stderr, "setUp: new exited %lld\n%.*s%.*s\n", (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
    AVP_ASSERT(c.rc == 0);
    return r;
}

#define CHECKU(r) RUN(r, "check", "game/CFooTest.cpp")

static void put_lf(const Repo *r, const char *rel, AvpStr s) { fx_put(r->root, rel, avp_cz(fx_a(), s), "\n"); }

AVP_TEST(fresh_scaffold_passes_with_force_active_warning) {
    Repo r = check_repo();
    Cli c = CHECKU(r);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "[OK] split present");
    CHECK_IN(c.out, "[OK] .text ends at the last instruction (0x80010040)");
    CHECK_IN(c.out, "[OK] every function in the asm range (4) is in symbols.txt");
    CHECK_IN(c.out, "3 address comments match symbols.txt and have banners");
    /* lbl_80500004/lbl_80500010 are read by outside code, so the unit is referenced from outside */
    CHECK_IN(c.out, "referenced from outside");
    repo_cleanup(&r);
}

AVP_TEST(stale_listing_from_an_earlier_split_is_ignored) {
    Repo r = check_repo();
    write_split_record(r.root);
    write_stale_listing(r.root);
    Cli c = CHECKU(r);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "[OK] every function in the asm range (4) is in symbols.txt");
    CHECK_IN(c.out, "[INFO] 1 stale asm listing from an earlier split");
    CHECK_IN(c.out, "auto_03_80010000_text.s");
    CHECK_NOT_IN(c.out, "fn_80010000_old");
    repo_cleanup(&r);
}

AVP_TEST(without_split_record_every_listing_is_read) {
    Repo r = check_repo();
    write_stale_listing(r.root);
    Cli c = CHECKU(r);
    CHECK_IN(c.out, "functions in the retail asm but not in symbols.txt: fn_80010000_old");
    CHECK_IN(c.out, "no usable split record");
    repo_cleanup(&r);
}

/* every file of the tree with its bytes, in walk order */
static AvpStr snapshot(const char *root) {
    AvpArena *a = fx_a();
    AvpBuf b;
    avp_buf_init(&b, a);
    AvpWalk *w = avp_walk_open(root, true);
    AVP_ASSERT(w != NULL);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        for (size_t i = 0; i < it.files.n; i++) {
            const char *p = avp_join(a, avp_cz(a, it.dirpath), avp_cz(a, it.files.v[i]));
            AvpBytes bytes = {0};
            AVP_ASSERT(avp_read_all(a, p, &bytes));
            avp_buf_printf(&b, "%s %zu\n", p, bytes.n);
            avp_buf_putn(&b, bytes.v, bytes.n);
        }
    }
    avp_walk_close(w);
    return avp_buf_str(&b);
}

AVP_TEST(check_writes_only_the_documented_cache) {
    Repo r = check_repo();
    AvpStr before = snapshot(r.root);
    Cli c = CHECKU(r);
    (void)c;
    AvpStr after = snapshot(r.root);
    CHECK_BYTES_EQ(before, after);
    repo_cleanup(&r);
}

AVP_TEST(isolated_unit_needs_force_active) {
    Repo r = check_repo();
    /* drop the outside readers of the unit's symbols from the asm: nothing references the unit */
    AvpStr text = fx_read(r.root, TEXT_ASM);
    text = fx_replace(text, "lwz r0, lbl_80500004@sda21(r0)", "nop");
    text = fx_replace(text, "lwz r0, lbl_80500010@sda21(r0)", "nop");
    text = fx_replace(text, "bl fn_80010060", "nop");
    put_lf(&r, TEXT_ASM, text);
    Cli c = CHECKU(r);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "[WARN] nothing outside the unit references any of its symbols");
    CHECK_IN(c.out, "lbl_80500000"); /* the registration object: what the sinit passes to the registry ctor */
    /* list it: the warning becomes OK */
    AvpStr yml = fx_replace(fx_read(r.root, "config/R5WEA4/config.yml"), "lbl_80500FFF",
                            "lbl_80500FFF\r\n  - lbl_80500000");
    fx_put(r.root, "config/R5WEA4/config.yml", avp_cz(fx_a(), fx_lf(yml)), "\r\n");
    c = CHECKU(r);
    CHECK_IN(c.out, "already listed under force_active: lbl_80500000");
    repo_cleanup(&r);
}

/* Isolate the unit as in isolated_unit_needs_force_active, but let the unit's lbl_80500008 be `shared`, a symbol of
 * the given scope, and let outside code read a `shared` of its own object file. */
static Cli share_name_with_outside_symbol(const Repo *r, const char *scope) {
    AvpArena *a = fx_a();
    AvpStr text = fx_read(r->root, TEXT_ASM);
    text = fx_replace(text, "lwz r0, lbl_80500004@sda21(r0)", "nop");
    text = fx_replace(text, "bl fn_80010060", "nop");
    text = fx_replace(text, "lwz r0, lbl_80500010@sda21(r0)", "lwz r0, shared@sda21(r0)");
    text = fx_replace(text, "lbl_80500008", "shared");
    put_lf(r, TEXT_ASM, text);
    const char *sbss = "build/R5WEA4/asm/auto_10_80500000_sbss.s";
    text = fx_read(r->root, sbss);
    text = fx_replace(text, ".obj lbl_80500008, global", avp_sprintf(a, ".obj shared, %s", scope));
    text = fx_replace(text, ".endobj lbl_80500008", ".endobj shared");
    put_lf(r, sbss, text);
    /* outside the unit's ranges: another object file's own `shared` */
    fx_put(r->root, "build/R5WEA4/asm/auto_10_80500100_sbss.s",
           ".include \"macros.inc\"\n.file \"auto_10_80500100_sbss\"\n\n# 0x80500100..0x80500104 | size: 0x4\n"
           ".section .sbss, \"wa\", @nobits\n.balign 8\n\n# .sbss:0x0 | 0x80500100 | size: 0x4\n"
           ".obj shared, local\n\t.skip 0x4\n.endobj shared\n",
           "\n");
    return CHECKU(*r);
}

AVP_TEST(local_symbol_is_not_referenced_by_another_units_local_of_the_same_name) {
    Repo r = check_repo();
    Cli c = share_name_with_outside_symbol(&r, "local");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "[WARN] nothing outside the unit references any of its symbols");
    CHECK_NOT_IN(c.out, "referenced from outside");
    repo_cleanup(&r);
}

AVP_TEST(global_symbol_read_by_outside_code_is_referenced_from_outside) {
    Repo r = check_repo();
    Cli c = share_name_with_outside_symbol(&r, "global");
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "referenced from outside (e.g. shared by fn_80000000)");
    repo_cleanup(&r);
}

AVP_TEST(wrong_address_comment_is_an_error) {
    Repo r = check_repo();
    AvpStr src = fx_replace(fx_read(r.root, "src/game/CFooTest.cpp"), "// 0x80010010", "// 0x80010014");
    fx_put(r.root, "src/game/CFooTest.cpp", avp_cz(fx_a(), fx_lf(src)), "\r\n");
    Cli c = CHECKU(r);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "[ERROR] line");
    CHECK_IN(c.out, "0x80010014 is not a function of this unit");
    repo_cleanup(&r);
}

AVP_TEST(name_mismatch_and_missing_banner_warn) {
    Repo r = check_repo();
    AvpStr src = fx_replace(fx_read_lf(r.root, "src/game/CFooTest.cpp"), "CFooTest::fn_80010010( int )",
                            "CFooTest::other( int )");
    src = fx_replace(src, "/*\n====================\nCFooTest::fn_80010020\n====================\n*/\n", "");
    fx_put(r.root, "src/game/CFooTest.cpp", avp_cz(fx_a(), src), "\r\n");
    Cli c = CHECKU(r);
    CHECK_EQ_I64(c.rc, 0);
    CHECK_IN(c.out, "is followed by `CFooTest::other`");
    CHECK_IN(c.out, "no banner comment above the function at 0x80010020");
    repo_cleanup(&r);
}

AVP_TEST(overlap_and_end_rule_errors) {
    Repo r = check_repo();
    AvpStr sp = fx_replace1(fx_read(r.root, SPLITS_REL), "end:0x80010040", "end:0x80010044");
    put_lf(&r, SPLITS_REL, sp);
    Cli c = CHECKU(r);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "[ERROR]");
    CHECK_IN(c.out, "overlaps game/After.cpp");
    repo_cleanup(&r);
}

AVP_TEST(end_after_last_instruction_before_asm_unit) {
    Repo r = check_repo();
    RUN(r, "new", "game/Pad.cpp", "--text", "0x80010060-0x8001006C");
    AvpStr sp = fx_replace(fx_read(r.root, SPLITS_REL), "start:0x80010060 end:0x8001006C",
                           "start:0x80010060 end:0x80010070");
    put_lf(&r, SPLITS_REL, sp);
    Cli c = RUN(r, "check", "game/Pad.cpp");
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "a source unit followed by an asm unit must end its .text at its last instruction");
    repo_cleanup(&r);
}

AVP_TEST(function_missing_from_symbols) {
    Repo r = check_repo();
    AvpStr sy = fx_replace(fx_read(r.root, SYMBOLS_REL),
                           "fn_80010020__8CFooTestFRC8CBarTestPCc = .text:0x80010020; // type:function size:0x10\n", "");
    put_lf(&r, SYMBOLS_REL, sy);
    Cli c = CHECKU(r);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "in the retail asm but not in symbols.txt: fn_80010020__8CFooTestFRC8CBarTestPCc");
    repo_cleanup(&r);
}

AVP_TEST(functions_named_gap_are_functions_not_padding) {
    /* BTE has real functions called gap_*: only dtk's gap_<idx>_<addr>_<section> names are padding */
    Repo r = check_repo();
    static const char *const rels[] = {"config/R5WEA4/symbols.txt", "build/R5WEA4/asm/auto_03_80000000_text.s",
                                       "build/R5WEA4/asm/auto_07_803E0000_data.s"};
    for (size_t i = 0; i < 3; i++)
        put_lf(&r, rels[i], fx_replace(fx_read(r.root, rels[i]), "fn_80010010__8CFooTestFi", "gap_conn_init"));
    Cli c = CHECKU(r);
    CHECK_RC(c, 0);
    CHECK_IN(c.out, "[OK] every function in the asm range (4) is in symbols.txt");
    CHECK_NOT_IN(c.out, "have no asm block");
    repo_cleanup(&r);
}

AVP_TEST(pad_functions_are_accepted_only_in_an_assembly_source_unit) {
    /* user's ruling 2026-11-14 (D-unit-asm-1): dtk's generated pad_ function over bytes no symbol covers has no
     * symbols.txt entry; a `.s` unit assembles those bytes, a C++ unit does not */
    Repo r = check_repo();
    static const char *const rels[] = {"build/R5WEA4/asm/auto_03_80000000_text.s", "build/R5WEA4/asm/auto_07_803E0000_data.s"};
    for (size_t i = 0; i < 2; i++)
        put_lf(&r, rels[i], fx_replace(fx_read(r.root, rels[i]), "fn_80010010__8CFooTestFi", "pad_03_80010010_text"));
    AvpStr sy = fx_read(r.root, SYMBOLS_REL);
    AvpStrVec lines = avp_str_splitlines(sy, true);
    AvpBuf keep;
    avp_buf_init(&keep, fx_a());
    for (size_t i = 0; i < lines.n; i++)
        if (!avp_str_startswith(lines.v[i], AVP_S("fn_80010010__8CFooTestFi "))) avp_buf_put(&keep, lines.v[i]);
    put_lf(&r, SYMBOLS_REL, avp_buf_str(&keep));
    Cli c = CHECKU(r);
    CHECK_EQ_I64(c.rc, 1);
    CHECK_IN(c.out, "in the retail asm but not in symbols.txt: pad_03_80010010_text");
    /* the same range as an assembly source unit */
    static const char *const cfg[] = {"configure.py", "config/R5WEA4/splits.txt"};
    for (size_t i = 0; i < 2; i++)
        put_lf(&r, cfg[i], fx_replace(fx_read(r.root, cfg[i]), "game/CFooTest.cpp", "game/CFooTest.s"));
    fx_put(r.root, "src/game/CFooTest.s", ".include \"macros.inc\"\n", "\n");
    c = RUN(r, "check", "game/CFooTest.s");
    CHECK_MSG(c.rc == 0, "exit %lld\n%.*s%.*s", (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
    CHECK_IN(c.out, "[OK] assembly source unit: dtk's generated pad_03_80010010_text covers bytes the source assembles");
    CHECK_NOT_IN(c.out, "not in symbols.txt");
    repo_cleanup(&r);
}

AVP_TEST(unknown_unit) {
    Repo r = check_repo();
    Cli c = RUN(r, "check", "game/Nope.cpp");
    CHECK_EQ_I64(c.rc, 2);
    repo_cleanup(&r);
}

AVP_TEST(accepts_alternative_spellings) {
    Repo r = check_repo();
    static const char *const spellings[] = {"game/CFooTest", "src/game/CFooTest.cpp", "main/game/CFooTest"};
    for (size_t i = 0; i < 3; i++) {
        Cli c = RUN(r, "check", spellings[i]);
        CHECK_MSG(c.rc == 0, "%s: exit %lld\n%.*s%.*s", spellings[i], (long long)c.rc, AVP_SF(c.out), AVP_SF(c.err));
    }
    repo_cleanup(&r);
}

UNIT_TEST_MAIN("unit")
