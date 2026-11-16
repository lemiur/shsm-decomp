/* tools/test_objcache.py ported: the wrapper with a fake compiler standing in for mwcceppc.
 *
 * The fake compiler (tests/fake/fake_mwcc.c, objcache_fake_mwcc.exe beside this test, run with the identity file
 * bin/mwcceppc.fake of each test's temporary tree as its first argument) follows `#include "name"` lines through the
 * includer's directory and then each -i directory, writes an object that depends on its arguments and every byte it
 * read, and a dependency file in MWCC's -MMD format (absolute header paths under the working directory). It appends
 * one line per run to bin/calls.log, so a test can tell a hit from a miss. Most
 * tests call oc_run_wrapped in-process; the command-line, bypass and concurrency tests run objcache.exe. */
#include "avp/test.h"
#include "objcache.h"
#include "shsm_cache.h"

#include <string.h>
#include <windows.h>

/* The fake compiler's identity file (bin/mwcceppc.fake): objcache keys on its bytes as on a compiler's. */
static const char FAKE_COMPILER[] = "fake mwcceppc, compiler version 1\n";

/* ------------------------------------------------------------------------------------------------ fixture */

static AvpArena *A;
static char *TMP, *ROOT, *CACHE;
static const char *FAKE; /* objcache_fake_mwcc.exe */

static void write_age(const char *path, const char *text, double age) {
    avp_makedirs(avp_cz(A, avp_path_dirname(path)), true);
    REQUIRE(avp_write_atomic(path, text, strlen(text)));
    i64 t = avp_time_ns() - (i64)(age * 1e9);
    avp_utime_ns(path, t, t);
}

static void write_file(const char *path, const char *text) {
    write_age(path, text, 60);
}

static char *rp(const char *root, const char *rel) {
    return avp_path_join(A, root, rel);
}

static void set_up(void) {
    A = avp_arena_new(0);
    if (!FAKE) FAKE = avp_path_join(avp_arena(), avp_exe_dir(), "objcache_fake_mwcc.exe");
    char *base = avp_path_join(A, avp_getcwd(A), "build\\tmp");
    avp_makedirs(base, true);
    TMP = avp_mkdtemp(A, "objcache-test-", NULL, base);
    ROOT = rp(TMP, "root");
    CACHE = rp(TMP, "cache");
    SetEnvironmentVariableW(L"SHSM_CACHE_DIR", avp_to_wz(A, CACHE));
    SetEnvironmentVariableW(L"SHSM_OBJCACHE", NULL);
    SetEnvironmentVariableW(L"MWCIncludes", NULL);
    write_file(rp(ROOT, "bin\\mwcceppc.fake"), FAKE_COMPILER);
    write_file(rp(ROOT, "src\\unit.c"), "#include \"a.h\"\nint x;\n");
    write_file(rp(ROOT, "inc2\\a.h"), "#include \"b.h\"\n");
    write_file(rp(ROOT, "inc2\\b.h"), "int b;\n");
    avp_makedirs(rp(ROOT, "inc1"), true);
    avp_makedirs(rp(ROOT, "build"), true);
}

static void tear_down(void) {
    avp_rmtree(TMP, AVP_RMTREE_IGNORE_ERRORS);
    SetEnvironmentVariableW(L"SHSM_CACHE_DIR", NULL);
    avp_arena_free(A);
}

typedef struct {
    const char *out;
    const char *flag;
    const char *src;
} ArgOpts;

static size_t make_argv(const char **av, ArgOpts o) {
    size_t n = 0;
    av[n++] = FAKE;
    av[n++] = "bin\\mwcceppc.fake";
    av[n++] = "-i";
    av[n++] = "inc1";
    av[n++] = "-i";
    av[n++] = "inc2";
    if (o.flag) av[n++] = o.flag;
    av[n++] = "-MMD";
    av[n++] = "-c";
    av[n++] = o.src ? o.src : "src\\unit.c";
    av[n++] = "-o";
    av[n++] = o.out ? o.out : "build\\o";
    return n;
}

typedef struct {
    int rc;
    AvpStr out, err;
} Ran;

static Ran run_cache_env(const char *root, ArgOpts o, const char *extra_env) {
    const char *av[16];
    size_t n = make_argv(av, o);
    OcEnv cur = oc_env_current(A);
    const char **vars = avp_alloc(A, (cur.n + 2) * sizeof(char *));
    memcpy(vars, cur.vars, cur.n * sizeof(char *));
    size_t nv = cur.n;
    if (extra_env) vars[nv++] = extra_env;
    OcEnv env = {vars, nv};
    AvpBuf out, err;
    avp_buf_init(&out, A);
    avp_buf_init(&err, A);
    OcRunOpts ro = {&out, &err, true};
    Ran r;
    r.rc = oc_run_wrapped(A, av, n, root ? root : ROOT, &env, &ro);
    r.out = avp_buf_str(&out);
    r.err = avp_buf_str(&err);
    return r;
}

static Ran run_cache(ArgOpts o) {
    return run_cache_env(NULL, o, NULL);
}

static int calls(const char *root) {
    AvpBytes b = {0};
    if (!avp_read_all(A, rp(root ? root : ROOT, "bin\\calls.log"), &b)) return 0;
    int n = 0;
    for (size_t i = 0; i < b.n; i++) n += b.v[i] == '\n';
    return n;
}

static AvpStr readf(const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(A, path, &b)) return (AvpStr){NULL, 0};
    return (AvpStr){(const char *)b.v, b.n};
}

typedef struct {
    AvpStr obj, dep;
} Outs;

static Outs outputs(const char *root, const char *out) {
    char *d = rp(root ? root : ROOT, out ? out : "build\\o");
    return (Outs){readf(rp(d, "unit.o")), readf(rp(d, "unit.d"))};
}

static bool outs_eq(Outs x, Outs y) {
    return x.obj.p && x.dep.p && avp_str_eq(x.obj, y.obj) && avp_str_eq(x.dep, y.dep);
}

/* the outputs of the compile run without the cache (removed afterwards) */
static Outs plain(const char *root, ArgOpts o) {
    const char *av[16];
    size_t n = make_argv(av, o);
    av[n] = NULL;
    root = root ? root : ROOT;
    AvpProcSpec spec = {.argv = av, .cwd = root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = A};
    AvpProcResult r = avp_run(&spec);
    CHECK_EQ_I64(r.returncode, 0);
    Outs res = outputs(root, o.out);
    avp_rmtree(rp(root, o.out ? o.out : "build\\o"), 0);
    return res;
}

static int count_kind(OcKind k) {
    OcEntries es = oc_entries(A, oc_cache_dir(A), true);
    int n = 0;
    for (size_t i = 0; i < es.n; i++) n += es.v[i].kind == k;
    return n;
}

static const char *first_of_kind(OcKind k) {
    OcEntries es = oc_entries(A, oc_cache_dir(A), true);
    for (size_t i = 0; i < es.n; i++)
        if (es.v[i].kind == k) return es.v[i].key;
    return NULL;
}

static const char *exe_path(void) {
    return avp_path_normpath(A, avp_path_join(A, avp_exe_dir(), "..\\objcache.exe"));
}

/* ------------------------------------------------------------------------------------------------ tests */

AVP_TEST(miss_then_hit_byte_identical) {
    set_up();
    Outs want = plain(NULL, (ArgOpts){"build\\o2"});
    CHECK_EQ_I64(run_cache((ArgOpts){0}).rc, 0);
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache((ArgOpts){"build\\o2"}).rc, 0);
    CHECK_MSG(calls(NULL) == n, "a hit must not run the compiler");
    CHECK(outs_eq(outputs(NULL, "build\\o2"), want));
    CHECK_STR(readf(rp(ROOT, "build\\" OC_STATS_LOG)), "mh");
    tear_down();
}

AVP_TEST(compiler_output_is_replayed) {
    set_up();
    write_file(rp(ROOT, "inc2\\b.h"), "WARN\n");
    Ran r1 = run_cache((ArgOpts){0});
    Ran r2 = run_cache((ArgOpts){0});
    CHECK_EQ_I64(r1.rc, 0);
    CHECK_EQ_I64(r2.rc, 0);
    CHECK(avp_str_find(r1.out, AVP_S("warning"), 0, -1) >= 0);
    CHECK(avp_str_eq(r1.out, r2.out));
    CHECK_EQ_I64(calls(NULL), 1);
    tear_down();
}

typedef void (*Change)(void);

static void assert_miss_after(Change change, ArgOpts o) {
    run_cache((ArgOpts){0});
    if (change) change();
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache(o).rc, 0);
    CHECK_MSG(calls(NULL) == n + 1, "expected a miss");
    Outs got = outputs(NULL, NULL);
    CHECK(outs_eq(got, plain(NULL, o)));
}

static void change_source(void) {
    write_file(rp(ROOT, "src\\unit.c"), "#include \"a.h\"\nint y;\n");
}
static void change_header(void) {
    write_file(rp(ROOT, "inc2\\b.h"), "int c;\n");
}
static void change_compiler(void) {
    AvpStr s = avp_str(FAKE_COMPILER);
    AvpStr r = avp_str_replace(A, s, AVP_S("version 1"), AVP_S("version 2"), -1);
    write_file(rp(ROOT, "bin\\mwcceppc.fake"), avp_cz(A, r));
}
static void add_dll(void) {
    write_file(rp(ROOT, "bin\\lmgr.dll"), "x");
}

AVP_TEST(changed_source_misses) {
    set_up();
    assert_miss_after(change_source, (ArgOpts){0});
    tear_down();
}

AVP_TEST(changed_header_misses) {
    set_up();
    assert_miss_after(change_header, (ArgOpts){0});
    tear_down();
}

AVP_TEST(changed_flag_misses) {
    set_up();
    assert_miss_after(NULL, (ArgOpts){.flag = "-DFOO"});
    tear_down();
}

AVP_TEST(changed_compiler_misses) {
    set_up();
    assert_miss_after(change_compiler, (ArgOpts){0});
    tear_down();
}

AVP_TEST(new_dll_beside_compiler_misses) {
    set_up();
    assert_miss_after(add_dll, (ArgOpts){0});
    tear_down();
}

AVP_TEST(shadowing_header_misses) {
    set_up();
    run_cache((ArgOpts){0});
    Outs before = outputs(NULL, NULL);
    /* a.h found in inc2; a new a.h in the earlier directory inc1 must win */
    write_file(rp(ROOT, "inc1\\a.h"), "int shadow;\n");
    int n = calls(NULL);
    run_cache((ArgOpts){0});
    CHECK_MSG(calls(NULL) == n + 1, "a shadowing header must be a miss");
    Outs got = outputs(NULL, NULL);
    CHECK(!outs_eq(got, before));
    CHECK(avp_str_find(got.dep, AVP_S("inc1"), 0, -1) >= 0);
    CHECK(outs_eq(got, plain(NULL, (ArgOpts){0})));
    tear_down();
}

AVP_TEST(shadowing_nested_header_misses) {
    set_up();
    run_cache((ArgOpts){0});
    /* b.h is included from inc2/a.h; a new inc1/b.h must still be noticed (conservatively) */
    write_file(rp(ROOT, "inc1\\b.h"), "int shadow;\n");
    int n = calls(NULL);
    run_cache((ArgOpts){0});
    CHECK_EQ_I64(calls(NULL), n + 1);
    tear_down();
}

AVP_TEST(failed_compile_stores_nothing) {
    set_up();
    write_file(rp(ROOT, "inc2\\b.h"), "#error\n");
    Ran r = run_cache((ArgOpts){0});
    CHECK_EQ_I64(r.rc, 1);
    CHECK(avp_str_find(r.out, AVP_S("error"), 0, -1) >= 0);
    CHECK_EQ_I64(oc_entries(A, oc_cache_dir(A), false).n, 0);
    CHECK_EQ_I64(run_cache((ArgOpts){0}).rc, 1);
    CHECK_EQ_I64(calls(NULL), 2);
    tear_down();
}

AVP_TEST(damaged_entry_is_a_miss_and_removed) {
    set_up();
    run_cache((ArgOpts){0});
    CHECK_EQ_I64(count_kind(OC_KIND_RESULT), 1);
    const char *key = first_of_kind(OC_KIND_RESULT);
    REQUIRE(key);
    ShsmCache *c = shsm_cache_new(oc_cache_dir(A));
    const char *path = shsm_cache_path(A, c, key);
    AvpStr d = readf(path);
    char *bad = avp_memdup(A, d.p, d.n);
    bad[d.n / 2] = (char)~bad[d.n / 2];
    avp_write_atomic(path, bad, d.n);
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache((ArgOpts){0}).rc, 0);
    CHECK_EQ_I64(calls(NULL), n + 1);
    Outs got = outputs(NULL, NULL);
    CHECK(outs_eq(got, plain(NULL, (ArgOpts){0})));
    AvpStr again = readf(path);
    CHECK_MSG(again.p && !(again.n == d.n && !memcmp(again.p, bad, d.n)), "stored again, not the damaged bytes");
    tear_down();
}

AVP_TEST(damaged_manifest_is_a_miss) {
    set_up();
    run_cache((ArgOpts){0});
    const char *key = first_of_kind(OC_KIND_MANIFEST);
    REQUIRE(key);
    ShsmCache *c = shsm_cache_new(oc_cache_dir(A));
    const char *path = shsm_cache_path(A, c, key);
    AvpStr d = readf(path);
    avp_write_atomic(path, d.p, 20); /* truncated */
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache((ArgOpts){0}).rc, 0);
    CHECK_EQ_I64(calls(NULL), n + 1);
    tear_down();
}

AVP_TEST(concurrent_stores_same_key) {
    set_up();
    AvpProc *ps[6];
    for (int i = 0; i < 6; i++) {
        const char *av[20];
        av[0] = exe_path();
        av[1] = "run";
        av[2] = "--";
        size_t n = make_argv(av + 3, (ArgOpts){avp_sprintf(A, "build\\c%d", i)});
        av[3 + n] = NULL;
        AvpProcSpec spec = {.argv = av, .cwd = ROOT, .out = AVP_IO_NULL, .err = AVP_IO_NULL, .arena = A};
        AvpProcResult fail;
        ps[i] = avp_proc_start(&spec, &fail);
        REQUIRE(ps[i]);
    }
    for (int i = 0; i < 6; i++) {
        i64 rc = -1;
        avp_proc_wait(ps[i], -1, &rc);
        CHECK_EQ_I64(rc, 0);
        avp_proc_close(ps[i]);
    }
    Outs want = plain(NULL, (ArgOpts){"build\\z"});
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache((ArgOpts){"build\\z"}).rc, 0);
    CHECK_EQ_I64(calls(NULL), n);
    CHECK(outs_eq(outputs(NULL, "build\\z"), want));
    for (int i = 0; i < 6; i++) {
        Outs o = outputs(NULL, avp_sprintf(A, "build\\c%d", i));
        CHECK(o.obj.p && avp_str_eq(o.obj, want.obj));
    }
    /* every run's event reached the stats log (appends are locked) */
    CHECK_EQ_I64(readf(rp(ROOT, "build\\" OC_STATS_LOG)).n, 7);
    tear_down();
}

AVP_TEST(disabled_bypasses) {
    set_up();
    Ran r = run_cache_env(NULL, (ArgOpts){0}, "SHSM_OBJCACHE=0");
    CHECK_EQ_I64(r.rc, 0);
    CHECK(!avp_exists(CACHE));
    CHECK(!avp_exists(rp(ROOT, "build\\" OC_STATS_LOG)));
    CHECK_EQ_I64(calls(NULL), 1);
    tear_down();
}

AVP_TEST(disabled_bypasses_in_the_executable) {
    set_up();
    const char *av[20];
    av[0] = exe_path();
    av[1] = "run";
    av[2] = "--";
    size_t n = make_argv(av + 3, (ArgOpts){0});
    av[3 + n] = NULL;
    AvpEnv *env = avp_env_copy(A, avp_env_current(A));
    avp_env_set(env, "SHSM_OBJCACHE", "off");
    AvpProcSpec spec = {.argv = av, .cwd = ROOT, .env = env, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = A};
    AvpProcResult r = avp_run(&spec);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(!avp_exists(CACHE));
    CHECK_EQ_I64(calls(NULL), 1);
    tear_down();
}

static void copy_tree(const char *from, const char *to) {
    AvpWalk *w = avp_walk_open(from, true);
    AvpWalkItem it;
    while (avp_walk_next(w, &it)) {
        char *dp = avp_cz(A, it.dirpath);
        char *rel = NULL;
        avp_path_relpath(A, dp, from, &rel);
        for (size_t i = 0; i < it.files.n; i++) {
            char *f = avp_cz(A, it.files.v[i]);
            if (!strcmp(f, "calls.log")) continue;
            char *dst = avp_path_joinv(A, 3, (const char *const[]){to, rel, f});
            avp_makedirs(avp_cz(A, avp_path_dirname(dst)), true);
            avp_copy2(avp_path_join(A, dp, f), dst);
        }
        size_t k = 0; /* skip build/ */
        for (size_t i = 0; i < it.dirs.n; i++)
            if (!avp_str_eqz(it.dirs.v[i], "build")) it.dirs.v[k++] = it.dirs.v[i];
        it.dirs.n = k;
    }
    avp_walk_close(w);
    avp_makedirs(rp(to, "build"), true);
    avp_makedirs(rp(to, "inc1"), true);
}

AVP_TEST(other_checkout_hits_byte_identical) {
    set_up();
    run_cache((ArgOpts){0});
    char *root2 = rp(TMP, "another checkout, longer");
    copy_tree(ROOT, root2);
    Outs want = plain(root2, (ArgOpts){0});
    int n = calls(root2);
    CHECK_EQ_I64(run_cache_env(root2, (ArgOpts){0}, NULL).rc, 0);
    CHECK_MSG(calls(root2) == n, "expected a hit from the other checkout's entry");
    CHECK(outs_eq(outputs(root2, NULL), want));
    tear_down();
}

AVP_TEST(mw_environment_is_keyed) {
    set_up();
    run_cache((ArgOpts){0});
    int n = calls(NULL);
    run_cache_env(NULL, (ArgOpts){0}, "MWCIncludes=x");
    CHECK_EQ_I64(calls(NULL), n + 1);
    tear_down();
}

AVP_TEST(uncacheable_forms_run_plain) {
    set_up();
    run_cache((ArgOpts){"build\\x.o"});
    /* MWCC prints an absolute -o under the working directory relative: never served from the cache */
    CHECK_EQ_I64(run_cache((ArgOpts){rp(ROOT, "build\\abs")}).rc, 0);
    CHECK_EQ_I64(oc_entries(A, oc_cache_dir(A), false).n, 0);
    CHECK_EQ_I64(calls(NULL), 2);
    CHECK_STR(readf(rp(ROOT, "build\\" OC_STATS_LOG)), "uu");
    tear_down();
}

AVP_TEST(verify_detects_a_wrong_object) {
    set_up();
    run_cache((ArgOpts){0});
    const char *key = first_of_kind(OC_KIND_RESULT);
    REQUIRE(key);
    char *scratch = rp(ROOT, "build\\v");
    avp_makedirs(scratch, true);
    const char *cache = oc_cache_dir(A);
    CHECK_CZ(oc_verify_entry(A, ROOT, cache, key, scratch), "ok");
    ShsmCache *c = shsm_cache_new(cache);
    AvpStr data;
    REQUIRE(shsm_cache_get(A, c, key, &data));
    OcResult r;
    REQUIRE(oc_unpack_result(A, data, &r));
    char *bad = avp_alloc(A, r.obj.n + 1);
    memcpy(bad, r.obj.p, r.obj.n);
    bad[r.obj.n] = 'x';
    r.obj = (AvpStr){bad, r.obj.n + 1};
    r.obj_sha = oc_sha_hex(A, bad, r.obj.n);
    AvpStr packed = oc_pack_result(A, &r);
    shsm_cache_put(c, key, packed.p, packed.n);
    CHECK_CZ(oc_verify_entry(A, ROOT, cache, key, scratch), "bad: object differs");
    write_file(rp(ROOT, "inc2\\b.h"), "changed\n");
    CHECK_CZ(oc_verify_entry(A, ROOT, cache, key, scratch), "stale");
    tear_down();
}

AVP_TEST(depfile_round_trip) {
    A = avp_arena_new(0);
    AvpStr data = AVP_S("build\\o\\u.o: src\\u.c \\\r\n\tX:\\r\\inc\\a.h \\\r\n\tX:\\r\\inc\\b.h \r\n");
    const char *target, *why;
    AvpStrVec deps = {0};
    REQUIRE(oc_parse_depfile(A, data, "src\\u.c", &target, &deps, &why));
    CHECK_CZ(target, "build\\o\\u.o");
    REQUIRE(deps.n == 2);
    CHECK_STR(deps.v[0], "X:\\r\\inc\\a.h");
    CHECK_STR(deps.v[1], "X:\\r\\inc\\b.h");
    const char *dz[2] = {deps.v[0].p, deps.v[1].p};
    CHECK(avp_str_eq(oc_render_depfile(A, target, "src\\u.c", dz, 2), data));
    deps.n = 0;
    REQUIRE(oc_parse_depfile(A, AVP_S("build\\o\\u.o: src\\u.c \r\n"), "src\\u.c", &target, &deps, &why));
    CHECK_CZ(target, "build\\o\\u.o");
    CHECK_EQ_I64(deps.n, 0);
    CHECK(!oc_parse_depfile(A, AVP_S("build\\o\\u.o: src\\u.c\n"), "src\\u.c", &target, &deps, &why));
    avp_vec_free(&deps);
    avp_arena_free(A);
}

AVP_TEST(depfile_escaped_spaces) {
    A = avp_arena_new(0);
    /* MWCC writes a space in a header path as '\ ' (a checkout path with spaces) */
    AvpStr data = AVP_S("build\\o\\u.o: src\\u.c \\\r\n\tX:\\Game\\ Folder\\inc\\a.h \r\n");
    const char *target, *why;
    AvpStrVec deps = {0};
    REQUIRE(oc_parse_depfile(A, data, "src\\u.c", &target, &deps, &why));
    REQUIRE(deps.n == 1);
    CHECK_STR(deps.v[0], "X:\\Game Folder\\inc\\a.h");
    const char *dz[1] = {deps.v[0].p};
    CHECK(avp_str_eq(oc_render_depfile(A, target, "src\\u.c", dz, 1), data));
    avp_vec_free(&deps);
    avp_arena_free(A);
}

AVP_TEST(include_names_and_computed_includes) {
    A = avp_arena_new(0);
    const char *why;
    AvpSet *s = avp_set_new();
    CHECK(oc_include_names(A, AVP_S("#include \"a/b.h\"\n  # include <c.h>\r\n#includes x\n"), s, &why));
    CHECK_EQ_I64(avp_set_len(s), 2);
    CHECK(avp_set_has(s, AVP_S("a\\b.h")));
    CHECK(avp_set_has(s, AVP_S("c.h")));
    CHECK(!oc_include_names(A, AVP_S("#include MACRO\n"), s, &why));
    CHECK_CZ(why, "computed #include");
    avp_set_free(s);
    avp_arena_free(A);
}

AVP_TEST(checkout_path_with_spaces_hits) {
    set_up();
    char *root = rp(TMP, "Game Folder checkout");
    copy_tree(ROOT, root);
    Outs want = plain(root, (ArgOpts){"build\\o2"});
    CHECK_EQ_I64(run_cache_env(root, (ArgOpts){0}, NULL).rc, 0);
    int n = calls(root);
    CHECK_EQ_I64(run_cache_env(root, (ArgOpts){"build\\o2"}, NULL).rc, 0);
    CHECK_MSG(calls(root) == n, "a hit must not run the compiler");
    CHECK(outs_eq(outputs(root, "build\\o2"), want));
    CHECK_STR(readf(rp(root, "build\\" OC_STATS_LOG)), "mh");
    tear_down();
}

/* ------------------------------------------------------------------------------------------------ the hash memo */

static OcPrehash prehash(void) {
    static const char *const tops[] = {"src", "inc1", "inc2"};
    OcPrehash ph = {0};
    oc_prehash(A, ROOT, rp(ROOT, "bin"), tops, 3, 2, &ph);
    return ph;
}

static const OcMemoEntry *memo_entry(const char *rel) {
    OcMemo *m = oc_memo_load(A, ROOT);
    return oc_memo_find(m, oc_norm(A, avp_path_abspath(A, rp(ROOT, rel))));
}

static int header_reads;
static void count_header_reads(const char *path) {
    size_t n = strlen(path);
    if (n > 2 && !strcmp(path + n - 2, ".h")) header_reads++;
}

AVP_TEST(prehash_memo_serves_a_hit_without_reading_files) {
    set_up();
    Outs want = plain(NULL, (ArgOpts){"build\\o2"});
    CHECK_EQ_I64(run_cache((ArgOpts){0}).rc, 0);
    OcPrehash ph = prehash();
    CHECK_EQ_I64(ph.files, 3); /* unit.c, a.h, b.h (bin/ has no .exe or .dll) */
    CHECK_EQ_I64(ph.hashed, 3);
    CHECK_EQ_I64(ph.kept, 0);
    ph = prehash();
    CHECK_MSG(ph.hashed == 0 && ph.kept == 3, "unchanged files are not read again");
    CHECK(memo_entry("inc2\\b.h") != NULL);
    header_reads = 0;
    oc_on_hash = count_header_reads;
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache((ArgOpts){"build\\o2"}).rc, 0);
    oc_on_hash = NULL;
    CHECK_MSG(calls(NULL) == n, "a hit");
    CHECK_MSG(header_reads == 0, "headers come from the memo");
    CHECK(outs_eq(outputs(NULL, "build\\o2"), want));
    tear_down();
}

static void edit_b_30(void) {
    write_age(rp(ROOT, "inc2\\b.h"), "int d;\n", 30);
}
static i64 recorded_mtime;
static void edit_b_longer_same_mtime(void) {
    char *p = rp(ROOT, "inc2\\b.h");
    avp_write_atomic(p, "int longer;\n", 12);
    avp_utime_ns(p, recorded_mtime, recorded_mtime);
}

AVP_TEST(memo_never_turns_a_changed_header_into_a_hit) {
    set_up();
    run_cache((ArgOpts){0});
    prehash();
    assert_miss_after(edit_b_30, (ArgOpts){0}); /* same size, new mtime (an ordinary edit) */
    prehash();
    const OcMemoEntry *e = memo_entry("inc2\\b.h");
    REQUIRE(e);
    recorded_mtime = e->mtime_ns; /* another size, mtime set back to the recorded one */
    assert_miss_after(edit_b_longer_same_mtime, (ArgOpts){0});
    tear_down();
}

static AvpStat racy_st;
static void same_stat_edit(void) {
    char *p = rp(ROOT, "inc2\\b.h");
    avp_write_atomic(p, "int c;\n", 7); /* same size */
    avp_utime_ns(p, racy_st.atime_ns, racy_st.mtime_ns);
}

AVP_TEST(racy_timestamp_is_rehashed) {
    set_up();
    /* b.h's mtime inside the racy window of its hashing: prehash must not trust it. The mtime is set ten minutes
     * ahead, so the window holds however long the run and the prehash take on a loaded machine (a file written
     * "just now" leaves the 3-second window when they take longer). */
    char *p = rp(ROOT, "inc2\\b.h");
    write_age(p, "int b;\n", -600);
    avp_stat(p, &racy_st);
    run_cache((ArgOpts){0});
    prehash();
    CHECK(memo_entry("inc2\\b.h") == NULL);
    assert_miss_after(same_stat_edit, (ArgOpts){0});
    tear_down();
}

AVP_TEST(memo_entry_rules) {
    i64 now = 1000 + OC_RACY_NS;
    OcMemoEntry good = {7, 10, 1000, now, "sha"};
    CHECK(oc_memo_entry_holds(&good, 7, 10, 1000));
    OcMemoEntry bad[] = {{8, 10, 1000, now, "sha"}, {7, 11, 1000, now, "sha"}, {7, 10, 1001, now, "sha"},
                         {7, 10, 1000, now - 1, "sha"}};
    for (size_t i = 0; i < 4; i++) CHECK(!oc_memo_entry_holds(&bad[i], 7, 10, 1000));
    CHECK(!oc_memo_entry_holds(NULL, 7, 10, 1000));
}

AVP_TEST(damaged_memo_is_ignored) {
    set_up();
    run_cache((ArgOpts){0});
    prehash();
    const char *mp = oc_memo_path(A, ROOT);
    AvpStr d = readf(mp);
    REQUIRE(d.n > 20);
    char *bad = avp_memdup(A, d.p, d.n);
    memset(bad + 12, 0xFF, 3);
    avp_write_atomic(mp, bad, d.n);
    CHECK_EQ_I64(oc_memo_count(oc_memo_load(A, ROOT)), 0);
    int n = calls(NULL);
    CHECK_EQ_I64(run_cache((ArgOpts){0}).rc, 0);
    CHECK_MSG(calls(NULL) == n, "still a hit (hashed afresh)");
    assert_miss_after(edit_b_30, (ArgOpts){0});
    tear_down();
}

/* ------------------------------------------------------------------------------------------------ environment */

AVP_TEST(cache_helpers_agree_with_shsm_cache) {
    A = avp_arena_new(0);
    char *base = avp_path_join(A, avp_getcwd(A), "build\\tmp");
    avp_makedirs(base, true);
    char *tmp = avp_mkdtemp(A, "objcache-helpers-", NULL, base);
    SetEnvironmentVariableW(L"SHSM_CACHE_DIR", avp_to_wz(A, tmp));
    CHECK_CZ(oc_default_root(A), shsm_cache_default_root(A));
    CHECK_CZ(oc_cache_dir(A), avp_path_join(A, tmp, "objcache"));
    CHECK(oc_enabled_value(NULL));
    CHECK(oc_enabled_value("1"));
    CHECK(!oc_enabled_value(" Off "));
    CHECK(!oc_enabled_value("FALSE"));
    CHECK(!oc_enabled_value("0"));
    CHECK(!oc_enabled_value("no"));
    SetEnvironmentVariableW(L"SHSM_CACHE_DIR", NULL);
    avp_rmtree(tmp, AVP_RMTREE_IGNORE_ERRORS);
    avp_arena_free(A);
}

AVP_TEST_MAIN("objcache")
