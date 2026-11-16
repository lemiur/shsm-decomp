/* check_rules: the command-line module (rules_cli.h): file selection, the text report and the --json document. The
 * whole command line is proven by the differential harness (diff/cases.txt). */
#include "avp/test.h"
#include "rules_cli.h"
#include <string.h>

static AvpStrVec strs(const char *const *v) {
    AvpStrVec r = {0};
    for (size_t i = 0; v[i]; i++) avp_vec_push(&r, avp_str(v[i]));
    return r;
}

static void put(const char *dir, const char *rel, const char *text) {
    AvpArena *a = avp_arena();
    char *path = avp_path_join(a, dir, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(path)), true);
    REQUIRE(avp_write_file(path, text, strlen(text)));
}

AVP_TEST(units_to_files) {
    AvpArena *a = avp_arena();
    AvpStrVec src = strs((const char *const[]){"src/a/foo.c", "include/a/foo.h", "src/a/foo.h", "src/a/bar.cpp",
                                                "src/a/foo.cpp", NULL});
    AvpStrVec out = {0};
    const char *err;
    AvpStrVec n1 = strs((const char *const[]){"main/a/foo", NULL});
    CHECK(rl_units_to_files(a, &n1, &src, &out, &err));
    REQUIRE(out.n == 3); /* foo.c, foo.h and foo.cpp under src/, in sources order; include/ never matches */
    CHECK_STR(out.v[0], "src/a/foo.c");
    CHECK_STR(out.v[1], "src/a/foo.h");
    CHECK_STR(out.v[2], "src/a/foo.cpp");
    out.n = 0;
    AvpStrVec n2 = strs((const char *const[]){"a/bar.CPP", "a/bar", NULL}); /* extension dropped; no de-duplication */
    CHECK(rl_units_to_files(a, &n2, &src, &out, &err));
    CHECK(out.n == 2);
    out.n = 0;
    AvpStrVec n3 = strs((const char *const[]){"a/bar", "src/a/bar", NULL}); /* a src/ prefix is not removed */
    CHECK(!rl_units_to_files(a, &n3, &src, &out, &err));
    CHECK_CZ(err, "no source file for unit 'src/a/bar'");
}

AVP_TEST(assembly_source_units_are_never_read) {
    /* user's ruling 2026-11-14: a `.s` unit is assembled, not checked; --units selects nothing for it and a named
     * `.s` path is skipped, while a directory walk never picks it up */
    AvpArena *a = avp_arena();
    char *root = avp_mkdtemp(a, "tmp", NULL, NULL);
    REQUIRE(root);
    put(root, "src/t/vec.s", ".include \"macros.inc\"\n");
    put(root, "src/t/c.c", "");
    put(root, "configure.py", "config.libs = [Object(Matching, \"t/vec.s\"), Object(Matching, \"t/c.c\")]\n");
    put(root, "docs/exceptions.md", "");
    CHECK(rl_asm_unit(a, root, AVP_S("main/t/vec")));
    CHECK(rl_asm_unit(a, root, AVP_S("t/vec.s")));
    CHECK(!rl_asm_unit(a, root, AVP_S("main/t/c")));
    CHECK(!rl_asm_unit(a, root, AVP_S("main/t/none")));
    AvpStrVec out = {0};
    const char *err;
    AvpStrVec p1 = strs((const char *const[]){"src", "src/t/vec.s", NULL});
    CHECK(rl_collect_files(a, root, &p1, &out, &err));
    REQUIRE(out.n == 1);
    CHECK_STR(out.v[0], "src/t/c.c");
    RlRunOpts o = {0};
    o.root = root;
    o.units = strs((const char *const[]){"main/t/vec", NULL});
    o.no_summary = true;
    AvpBuf so, se;
    avp_buf_init(&so, a);
    avp_buf_init(&se, a);
    int rc = rl_run(a, &o, &so, &se);
    CHECK_MSG(rc == 0, "rc %d: %.*s", rc, (int)se.n, se.p);
    CHECK(!avp_str_contains(avp_buf_str(&se), AVP_S("no source file")));
    avp_rmtree(root, 0);
}

AVP_TEST(collect_files) {
    AvpArena *a = avp_arena();
    char *root = avp_mkdtemp(a, "tmp", NULL, NULL);
    REQUIRE(root);
    put(root, "src/b/z.c", "");
    put(root, "src/b/a.H", "");
    put(root, "src/a.cpp", "");
    put(root, "src/notes.txt", "");
    put(root, "src/c/d.inc", "");
    AvpStrVec out = {0};
    const char *err;
    AvpStrVec p1 = strs((const char *const[]){"src", "src/notes.txt", NULL});
    CHECK(rl_collect_files(a, root, &p1, &out, &err));
    REQUIRE(out.n == 5); /* top-down, directories and files sorted; a plain file is kept whatever its name */
    CHECK_STR(out.v[0], "src/a.cpp");
    CHECK_STR(out.v[1], "src/b/a.H");
    CHECK_STR(out.v[2], "src/b/z.c");
    CHECK_STR(out.v[3], "src/c/d.inc");
    CHECK_STR(out.v[4], "src/notes.txt");
    out.n = 0;
    AvpStrVec p2 = strs((const char *const[]){"src/a.cpp", "nope", NULL});
    CHECK(!rl_collect_files(a, root, &p2, &out, &err));
    CHECK_CZ(err, "no such file or directory: nope");
    out.n = 0;
    AvpStrVec p3 = strs((const char *const[]){avp_path_join(a, root, "src/b"), NULL}); /* an absolute path */
    CHECK(rl_collect_files(a, root, &p3, &out, &err));
    CHECK(out.n == 2 && avp_str_eqz(out.v[0], "src/b/a.H"));
    avp_tempdir_cleanup(root);
}

/* Checker.run over in-memory sources, every path scanned; the first unit is Matching. */
static RlChecker *checker(const char *const *paths, const char *const *texts) {
    AvpArena *a = avp_arena();
    AvpStrVec P = strs(paths), T = strs(texts);
    RlConfig cfg = {0};
    cfg.unit_status = avp_map_new();
    AvpStr *v = AVP_NEW(a, AvpStr);
    *v = AVP_S("Matching");
    avp_map_put(cfg.unit_status, avp_str(paths[0] + 4), NULL)->p = v;
    RlChecker *ck = rl_checker_from(a, &P, &T, &cfg);
    for (size_t i = 0; i < P.n; i++) avp_vec_push(&ck->scan, P.v[i]);
    ck->classes_note = AVP_S("R13 slot checks skipped: no classes.json given");
    rl_checker_run(ck);
    return ck;
}

AVP_TEST(report_text) {
    static const char *const paths[] = {"src/m.c", NULL};
    static const char *const texts[] = {"/* f */\nint f(void) { volatile int v = 0; return v; } // 0x80001000\n", NULL};
    RlChecker *ck = checker(paths, texts);
    REQUIRE(!ck->failed && !ck->error);
    CHECK(rl_exit_code(ck) == 1);
    AvpStr t = rl_report_text(ck, (AvpStr){0}, false, false);
    CHECK(avp_str_contains(t, AVP_S("src/m.c:2: R17 [match]")));
    CHECK(avp_str_contains(t, AVP_S("\n# summary by file\n# src/m.c ")));
    CHECK(avp_str_contains(t, AVP_S(" Matching    functions=1   match=")));
    CHECK(avp_str_contains(t, AVP_S("\n# src/m.c::f [2-2] MATCH-PENDING match=")));
    CHECK(avp_str_contains(t, AVP_S("\n# R13 slot checks skipped: no classes.json given\n")));
    CHECK(avp_str_contains(t, AVP_S("\n# MATCH-PENDING in a Matching unit: src/m.c::f\n")));
    CHECK(avp_str_endswith(t, AVP_S("\n# result: FAIL: match-tier findings in Matching units\n")));
    /* --no-summary keeps the MATCH-PENDING and result lines; --tier filters the findings, not the exit code */
    AvpStr n = rl_report_text(ck, AVP_S("advisory"), true, false);
    CHECK(!avp_str_contains(n, AVP_S("R17")));
    CHECK(!avp_str_contains(n, AVP_S("# summary")));
    CHECK(avp_str_startswith(n, AVP_S("# MATCH-PENDING in a Matching unit: src/m.c::f\n# result: FAIL")));
}

AVP_TEST(report_json) {
    static const char *const paths[] = {"src/m.c", NULL};
    static const char *const texts[] = {"/* f */\nint f(void) { return 0; } // 0x80001000\n", NULL};
    RlChecker *ck = checker(paths, texts);
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    rl_report_json(ck, &b, AVP_S("R:\\oot"), (AvpStr){0});
    AvpStr s = avp_buf_str(&b);
    CHECK(avp_str_startswith(s, AVP_S("{\n  \"root\": \"R:\\\\oot\",\n  \"scanned\": [\n    \"src/m.c\"\n  ],\n")));
    CHECK(avp_str_contains(s, AVP_S("\n  \"units\": {\n    \"m.c\": \"Matching\"\n  },\n")));
    CHECK(avp_str_contains(s, AVP_S("\n  \"exception_comments\": [],\n")));
    CHECK(avp_str_contains(s, AVP_S("\"hand_reviewed\": [\n    \"R5\",\n    \"R6\",\n    \"R8\",\n    \"R10\",")));
    CHECK(avp_str_endswith(s, AVP_S("\n  \"exit\": 0\n}\n")));
    AvpJsonError err;
    CHECK(avp_json_parse(avp_arena(), s, &err) != NULL);
}

AVP_TEST_MAIN("check_rules_cli")
