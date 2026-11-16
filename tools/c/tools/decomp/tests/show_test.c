/* Ports tools/test_decomp.py ShowTests (show and status on a made-up listing store, the listing entry point, the
 * on-demand export with the headless runner replaced) and tests run_headless itself against small .bat launchers:
 * argument quoting through a path with spaces, the "Unable to lock project" retry and the failure messages. */
#include "avp/test.h"
#include "decomp.h"

#include <stdlib.h>
#include <string.h>

/* REQUIRE for helpers that return a value: a failed requirement ends the test executable. */
#define MUST(cond) do { if (!(cond)) { avp_test__fail(__FILE__, __LINE__, "%s (required)", #cond); exit(1); } } while (0)

static const char SYMBOLS_TXT[] =
    "memcpy = .init:0x80005F34; // type:function size:0x29C scope:global\n"
    "__ct__4CFooFv = .text:0x80010000; // type:function size:0x40 scope:global\n"
    "__dt__4CFooFv = .text:0x80010040; // type:function size:0x40 scope:global\n"
    "lbl_80526720 = .sdata:0x80526720; // type:object size:0x4 data:4byte\n"
    "lbl_8052B5E0 = .sdata2:0x8052B5E0; // type:object size:0x4 data:float\n";

static void put_file(const char *path, const char *text) {
    AvpArena *a = avp_arena();
    REQUIRE(avp_makedirs(avp_cz(a, avp_path_dirname(path)), true));
    REQUIRE(avp_write_file(path, text, strlen(text)));
}

typedef struct {
    const char *root;
    ShsmRepo *repo;
    DecompPaths *paths;
    const char *stamp;
} Fixture;

/* FakeRepo + ShowTests.setUp: a checkout with only symbols.txt, one listing and its index entry */
static Fixture fixture(const char *name) {
    AvpArena *a = avp_arena();
    Fixture f;
    f.root = avp_path_abspath(a, avp_join(a, "build", "tmp", "decomp_test", name, "check out"));
    avp_rmtree(f.root, AVP_RMTREE_IGNORE_ERRORS);
    put_file(avp_join(a, f.root, "configure.py"), "# fixture\n");
    put_file(avp_join(a, f.root, "tools", "README"), "fixture\n");
    put_file(avp_join(a, f.root, "config", "R5WEA4", "config.yml"), "name: fixture\n");
    put_file(avp_join(a, f.root, "config", "R5WEA4", "symbols.txt"), SYMBOLS_TXT);
    f.repo = shsm_repo_new(f.root, "R5WEA4");
    f.paths = decomp_paths(a, f.repo);
    f.stamp = decomp_symbols_stamp(a, f.repo->symbols_txt);
    AvpBuf body;
    avp_buf_init(&body, a);
    avp_buf_putz(&body, "// symbol: __ct__4CFooFv\n\nvoid __ct__4CFooFv(CFoo *this)\n{\n");
    for (int i = 0; i < 60; i++) avp_buf_putz(&body, "  x;\n");
    avp_buf_putz(&body, "}\n");
    put_file(avp_join(a, f.paths->decomp, "80010000___ct__4CFooFv.c"), avp_buf_cz(&body));
    AvpJsonError err = {0};
    AvpJson *index = avp_json_parse(
        a,
        avp_pyfmt(a,
                  "{\"functions\": {\"80010000\": {\"name\": \"__ct__4CFooFv\", \"file\": "
                  "\"80010000___ct__4CFooFv.c\", \"status\": \"ok\", \"refs\": [], \"stamp\": \"%s\"}}}",
                  f.stamp),
        &err);
    MUST(index);
    dc_save_json(f.paths->index, index);
    return f;
}

typedef struct {
    i64 rc;
    AvpStr out;
} Ran;

static Ran run_exe(const Fixture *f, const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **argv = AVP_NEWN(a, const char *, n + 4);
    argv[0] = avp_path_normpath(a, avp_join(a, avp_exe_dir(), "..", "decomp.exe"));
    argv[1] = "--root";
    argv[2] = f->root;
    for (size_t i = 0; i < n; i++) argv[3 + i] = args[i];
    AvpProcSpec spec = {.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true, .timeout = 120,
                        .arena = a};
    AvpProcResult r = avp_run(&spec);
    MUST(r.status == AVP_PROC_OK);
    return (Ran){r.returncode, r.out};
}
#define RUN(f, ...) run_exe((f), (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))

AVP_TEST(show_prints_notice_and_caps) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("caps");
    Ran r = RUN(&f, "show", "__ct__4CFooFv");
    CHECK_EQ_I64(r.rc, 0);
    AvpStrVec lines = avp_str_splitlines(r.out, false);
    REQUIRE(lines.n > 2);
    CHECK_STR(lines.v[0], DECOMP_NOTICE);
    CHECK(avp_str_contains(r.out, AVP_S("void __ct__4CFooFv(CFoo *this)")));
    CHECK(!avp_str_contains(r.out, AVP_S("WARNING")));
    CHECK(avp_str_contains(lines.v[lines.n - 1], AVP_S("more lines")));
    r = RUN(&f, "show", "0x80010000", "--all");
    CHECK(!avp_str_contains(r.out, AVP_S("more lines")));
    CHECK(avp_str_endswith(avp_str_rstrip(r.out), AVP_S("}")));
    (void)a;
}

AVP_TEST(show_warns_when_stale) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("stale");
    AvpJson *idx = decomp_load_index(a, f.paths);
    avp_json_set(a, decomp_index_entry(idx, 0x80010000), AVP_S("stamp"), avp_json_strz(a, "000000000000"));
    dc_save_json(f.paths->index, idx);
    Ran r = RUN(&f, "show", "80010000");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(avp_str_contains(r.out, AVP_S("WARNING: made from an older symbols.txt")));
    CHECK(avp_str_contains(r.out, AVP_S("refresh")));
}

typedef struct {
    int calls;
    bool read_only;
} HookState;

static double fake_headless(void *ctx, const DecompPaths *p, const char *const *argv, size_t argc, const char *label,
                            AvpStr *log) {
    (void)p, (void)label;
    AvpArena *a = avp_arena();
    HookState *h = ctx;
    h->calls++;
    size_t i = 0;
    for (size_t k = 0; k < argc; k++) {
        if (!strcmp(argv[k], "ShsmExportDecomp.java")) i = k;
        if (!strcmp(argv[k], "-readOnly")) h->read_only = true;
    }
    MUST(i && i + 3 < argc);
    const char *out_dir = argv[i + 1], *addr_file = argv[i + 2], *results = argv[i + 3];
    AvpStr addrs;
    MUST(avp_read_text(a, addr_file, AVP_TM_UTF8, &addrs));
    CHECK_STR(avp_str_strip(addrs), "80010040");
    put_file(avp_join(a, out_dir, "80010040___dt__4CFooFv.c"), "void __dt__4CFooFv(CFoo *this, short param_1)\n{\n}\n");
    put_file(results, "address\tname\tfile\tstatus\treason\trefs\n"
                      "80010040\t__dt__4CFooFv\t80010040___dt__4CFooFv.c\tok\t\t\n");
    *log = AVP_S("EXPORT DONE");
    return 1.0;
}

AVP_TEST(show_exports_on_demand) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("ondemand");
    put_file(f.paths->state, avp_sprintf(a, "{\"install\": \"G\", \"program\": \"main.elf\", \"stamp\": \"%s\", "
                                            "\"version\": \"11.4\"}",
                                         f.stamp));
    put_file(avp_join(a, f.paths->project, DECOMP_PROJECT_NAME ".gpr"), "");
    HookState h = {0};
    dc_headless_hook = fake_headless;
    dc_headless_hook_ctx = &h;
    const ShsmSymbol *s = dc_resolve(a, shsm_load_symbols(f.repo, true), "__dt__4CFooFv");
    DecompListing L = dc_listing_sym(a, f.repo, s, true, 60);
    dc_headless_hook = NULL;
    CHECK_EQ_I64(h.calls, 1);
    CHECK(h.read_only);
    CHECK(L.found);
    CHECK(avp_str_contains(L.text, AVP_S("short param_1")));
    AvpJson *e = decomp_index_entry(decomp_load_index(a, f.paths), 0x80010040);
    REQUIRE(e);
    CHECK_STR(avp_json_getz(e, "stamp")->u.str, f.stamp);
    CHECK_STR(avp_json_getz(e, "ghidra")->u.str, "11.4");
}

AVP_TEST(show_without_project_is_a_usage_error) {
    Fixture f = fixture("noproject");
    CHECK_EQ_I64(RUN(&f, "show", "__dt__4CFooFv").rc, SHSM_EXIT_ERROR);
}

AVP_TEST(unknown_symbol) {
    Fixture f = fixture("unknown");
    CHECK_EQ_I64(RUN(&f, "show", "no_such_function").rc, SHSM_EXIT_ERROR);
}

AVP_TEST(status_counts) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("status");
    Ran r = RUN(&f, "--json", "status");
    AvpJsonError err = {0};
    AvpJson *d = avp_json_parse(a, r.out, &err);
    REQUIRE(d);
    CHECK_EQ_I64(avp_json_i64(avp_json_getz(d, "listings"), -1), 1);
    CHECK_EQ_I64(avp_json_i64(avp_json_getz(d, "stale"), -1), 0);
    CHECK_EQ_I64(avp_json_i64(avp_json_getz(d, "failed"), -1), 0);
    CHECK_EQ_I64((i64)avp_json_len(avp_json_getz(d, "failed_functions")), 0);
    const char *keys[] = {"found", "install", "version", "language", "project"};
    for (int i = 0; i < 5; i++) CHECK_MSG(avp_json_getz(d, keys[i]) != NULL, "key %s", keys[i]);
    CHECK(!avp_json_bool_or(avp_json_getz(d, "project"), true));
    CHECK_EQ_I64(r.rc, 0);
    r = RUN(&f, "status", "--json"); /* the shared options after the subcommand */
    d = avp_json_parse(a, r.out, &err);
    REQUIRE(d);
    CHECK_EQ_I64(avp_json_i64(avp_json_getz(d, "listings"), -1), 1);
}

AVP_TEST(listing_entry_point) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("entry");
    DecompListing L = decomp_listing_at(a, f.repo, 0x80010000);
    CHECK(L.found);
    CHECK(avp_str_contains(L.text, AVP_S("void __ct__4CFooFv(CFoo *this)")));
    CHECK_STR(avp_json_getz(L.entry, "status")->u.str, "ok");
    CHECK(!L.stale);
    L = decomp_listing_at(a, f.repo, 0x80010040); /* no listing: no export, no Ghidra */
    CHECK(!L.found && L.entry == NULL && !L.stale);
}

/* ---- run_headless against real launchers ---- */

static const char *launcher(const Fixture *f, const char *body) {
    AvpArena *a = avp_arena();
    const char *bat = avp_join(a, f->root, "fake ghidra", "support", "analyzeHeadless.bat");
    put_file(bat, body);
    return bat;
}

AVP_TEST(run_headless_quotes_arguments_and_logs) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("quote");
    const char *bat = launcher(&f, "@echo off\r\necho ARGS %*\r\nexit /b 0\r\n");
    const char *argv[] = {bat, "a b", "c", NULL};
    AvpStr log;
    double s = dc_run_headless(a, f.paths, argv, 3, "quote", &log);
    CHECK(s >= 0);
    CHECK(avp_str_contains(log, AVP_S("ARGS \"a b\" c")));
    CHECK(avp_isfile(avp_join(a, f.paths->logs, "quote.log")));
}

AVP_TEST(run_headless_retries_a_locked_project) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("lock");
    const char *bat = launcher(&f, "@echo off\r\necho x>> \"%~dp0count.txt\"\r\n"
                                   "echo ERROR Unable to lock project! shsm\r\nexit /b 1\r\n");
    const char *argv[] = {bat, NULL};
    dc_lock_wait_seconds = 0.01;
    AvpTry t;
    AvpStr log;
    if (AVP_TRY(&t)) {
        dc_run_headless(a, f.paths, argv, 1, "lock", &log);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_EQ_I64(t.code, SHSM_EXC_USAGE);
        CHECK_CZ(t.msg, "the Ghidra project is in use by another run (log: build/ghidra/logs/lock.log)");
    }
    AvpStr count;
    REQUIRE(avp_read_text(a, avp_join(a, f.root, "fake ghidra", "support", "count.txt"), AVP_TM_UTF8, &count));
    CHECK_EQ_I64((i64)avp_str_count(count, AVP_S("x")), 3);
}

AVP_TEST(run_headless_failure_messages) {
    AvpArena *a = avp_arena();
    Fixture f = fixture("fail");
    const char *bat = launcher(&f, "@echo off\r\necho one\r\necho REPORT SCRIPT ERROR: boom\r\nexit /b 0\r\n");
    const char *argv[] = {bat, NULL};
    AvpTry t;
    AvpStr log;
    if (AVP_TRY(&t)) {
        dc_run_headless(a, f.paths, argv, 1, "x", &log);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_CZ(t.msg, "analyzeHeadless failed (exit 0): REPORT SCRIPT ERROR: boom (log: build/ghidra/logs/x.log)");
    }
    bat = launcher(&f, "@echo off\r\necho fine\r\nexit /b 3\r\n");
    if (AVP_TRY(&t)) {
        dc_run_headless(a, f.paths, argv, 1, "y", &log);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_CZ(t.msg, "analyzeHeadless failed (exit 3): see the log (log: build/ghidra/logs/y.log)");
    }
}

AVP_TEST_MAIN("decomp_show")
