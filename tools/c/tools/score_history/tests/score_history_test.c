/* Unit tests of score_history (the port of tools/test_score_history.py). Like the Python, they build a throw-away git
 * repository with synthetic objdiff reports and sources; never the real repository, its build directory or its
 * database. Not ported: test_show_without_analysis_row_gives_a_message (it monkeypatches History.analyse; the C
 * analysis cannot be replaced, and the message path is unreachable without that). */
#include "sh_priv.h"
#include "shsm_git.h"

#include "avp/test.h"

#include <string.h>

/* ---- fixtures ---------------------------------------------------------------------------------------------------- */

static const char FOO_V1[] =
    "// Foo.cpp\n"
    "\n"
    "#include <game/Foo.h>\n"
    "\n"
    "/*\n"
    "====================\n"
    "Foo::fn_80000010\n"
    "====================\n"
    "*/\n"
    "// 0x80000010\n"
    "void Foo::fn_80000010( int x ) {\n"
    "    value = x + 1;\n"
    "}\n"
    "\n"
    "// 0x80000020\n"
    "void Foo::fn_80000020() {\n"
    "    value = 0;\n"
    "}\n"
    "\n"
    "void Foo::fn_80000030() {\n"
    "    value = 3;\n"
    "} // 0x80000030\n";

static const char PLAIN_C[] =
    "#include <os.h>\n"
    "\n"
    "void OSDoIt(int a) {\n"
    "    a++;\n"
    "} // 0x80000100\n"
    "\n"
    "static int helper(int b) { return b; }\n";

static AvpStr rep(AvpArena *a, AvpStr s, const char *old, const char *new_) {
    return avp_str_replace(a, s, avp_str(old), avp_str(new_), -1);
}
static AvpStr foo_v1(void) { return avp_strn(FOO_V1, sizeof FOO_V1 - 1); }
static AvpStr foo_reflowed(AvpArena *a) {
    AvpStr s = rep(a, foo_v1(), "value = x + 1;", "value =   x + 1;  // reflowed");
    return rep(a, s, "====================\nFoo::fn_80000010", "====================\nFoo::fn_80000010 (header edited)");
}
static AvpStr foo_v2(AvpArena *a) { return rep(a, foo_v1(), "value = x + 1;", "value = x + 2;"); }
static AvpStr foo_v3(AvpArena *a) { return rep(a, foo_v2(a), "value = 0;", "value = 7;"); }

static i64 A(i64 n) { return 0x80000000LL + n; }

/* ---- demangle ---------------------------------------------------------------------------------------------------- */

static AvpStr dm0(const char *sym) {
    bool sinit;
    AvpStrVec v = sh_demangle(avp_arena(), avp_str(sym), &sinit);
    return v.n ? v.v[0] : AVP_S("");
}

AVP_TEST(demangle_names) {
    CHECK_STR(dm0("fn_800067CC__8unk_RampFf"), "unk_Ramp::fn_800067CC");
    CHECK_STR(dm0("__ct__12unk_80006C7CFv"), "unk_80006C7C::unk_80006C7C");
    CHECK_STR(dm0("__dt__17CPadRumbleTriggerFv"), "CPadRumbleTrigger::~CPadRumbleTrigger");
    CHECK_STR(dm0("fn_800067A0__Fv"), "fn_800067A0");
    CHECK_STR(dm0("@36@fn_8009CB1C__19CScreenShakeTriggerFR7unk_Msg"), "CScreenShakeTrigger::fn_8009CB1C");
    CHECK_STR(dm0("GetValue__Q23Foo3BarCFv"), "Foo::Bar::GetValue");
    bool sinit;
    sh_demangle(avp_arena(), AVP_S("__sinit_\\PadRumble_cpp"), &sinit);
    CHECK(sinit);
    AvpStrVec v = sh_demangle(avp_arena(), AVP_S("OSCreateAlarm"), &sinit);
    CHECK(!sinit && v.n == 1);
    CHECK_STR(v.v[0], "OSCreateAlarm");
    v = sh_demangle(avp_arena(), AVP_S("__OSInitAlarm"), &sinit);
    CHECK(v.n == 1);
    CHECK_STR(v.v[0], "__OSInitAlarm");
}

/* ---- index ------------------------------------------------------------------------------------------------------- */

static const ShIdxFunc *by_name(const ShIndex *ix, const char *name) {
    for (size_t i = 0; i < ix->nfuncs; i++)
        if (avp_str_eqz(ix->funcs[i].name, name)) return &ix->funcs[i];
    return NULL;
}

AVP_TEST(hash_ignores_comments_and_layout) {
    AvpArena *a = avp_arena();
    ShIndex *x = sh_index_source(a, foo_v1()), *y = sh_index_source(a, foo_reflowed(a)), *z = sh_index_source(a, foo_v2(a));
    REQUIRE(x->nfuncs == 3 && y->nfuncs == 3 && z->nfuncs == 3);
    for (size_t i = 0; i < 3; i++) {
        CHECK(avp_str_eq(x->funcs[i].name, y->funcs[i].name));
        CHECK(avp_str_eq(x->funcs[i].hash, y->funcs[i].hash));
    }
    CHECK(!avp_str_eq(by_name(x, "Foo::fn_80000010")->hash, by_name(z, "Foo::fn_80000010")->hash));
    CHECK(avp_str_eq(by_name(x, "Foo::fn_80000020")->hash, by_name(z, "Foo::fn_80000020")->hash));
}

AVP_TEST(addresses_header_and_trailing) {
    ShIndex *x = sh_index_source(avp_arena(), foo_v1());
    REQUIRE(x->nfuncs == 3);
    CHECK(by_name(x, "Foo::fn_80000010")->has_addr && by_name(x, "Foo::fn_80000010")->addr == 0x80000010);
    CHECK(by_name(x, "Foo::fn_80000020")->has_addr && by_name(x, "Foo::fn_80000020")->addr == 0x80000020);
    CHECK(by_name(x, "Foo::fn_80000030")->has_addr && by_name(x, "Foo::fn_80000030")->addr == 0x80000030);
}

AVP_TEST(c_file_and_rough_fallback) {
    AvpArena *a = avp_arena();
    ShIndex *x = sh_index_source(a, avp_strn(PLAIN_C, sizeof PLAIN_C - 1));
    const char *basis;
    sh_index_lookup(a, x, AVP_S("OSDoIt"), 0x80000100, &basis);
    CHECK_CZ(basis, "addr");
    sh_index_lookup(a, x, AVP_S("helper"), 0x80000999, &basis);
    CHECK_CZ(basis, "name");
}

AVP_TEST(crlf_same_hash) {
    AvpArena *a = avp_arena();
    ShIndex *x = sh_index_source(a, foo_v1());
    ShIndex *y = sh_index_source(a, rep(a, foo_v1(), "\n", "\r\n"));
    CHECK(avp_str_eq(sh_index_to_json(a, x), sh_index_to_json(a, y)));
    CHECK(avp_str_eq(x->unit_hash, y->unit_hash));
}

AVP_TEST(lookup_by_name_and_sinit) {
    AvpArena *a = avp_arena();
    ShIndex *x = sh_index_source(a, foo_v1());
    const char *basis;
    const ShIdxFunc *f = sh_index_lookup(a, x, AVP_S("fn_80000020__3FooFv"), 0x1, &basis);
    REQUIRE(f != NULL);
    CHECK_STR(f->name, "Foo::fn_80000020");
    CHECK_CZ(basis, "name");
    CHECK(sh_index_lookup(a, x, AVP_S("__sinit_\\Foo_cpp"), 0x2, &basis) == NULL);
    CHECK_CZ(basis, "sinit");
    CHECK(sh_index_lookup(a, x, AVP_S("dtor_80001234"), 0x3, &basis) == NULL);
    CHECK_CZ(basis, "unit");
}

AVP_TEST(index_json_round_trip) {
    AvpArena *a = avp_arena();
    ShIndex *x = sh_index_source(a, foo_v1());
    AvpStr j = sh_index_to_json(a, x);
    CHECK(avp_str_startswith(j, AVP_S("{\"funcs\": [{\"name\": \"Foo::fn_80000010\", \"line\": 11, \"end\": 13, \"addr\": 2147483664")));
    CHECK(avp_str_eq(sh_index_to_json(a, sh_index_from_json(a, j)), j));
}

AVP_TEST(lexer_details) {
    AvpArena *a = avp_arena();
    ShLex lx;
    /* #if 0 arms are dropped, numbers, multi-character operators, a non-ASCII operator code point */
    sh_lex(a, AVP_S("#if 0\nint dead;\n#else\nint x = 0x1fu + 1.5e3f;\n#endif\na <<= b->c; \xc3\xa9"), &lx);
    const char *want[] = {"int", "x", "=", "0x1fu", "+", "1.5e3f", ";", "a", "<<=", "b", "->", "c", ";", "\xc3\xa9"};
    REQUIRE(lx.toks.n == sizeof want / sizeof *want);
    for (size_t i = 0; i < lx.toks.n; i++) CHECK_STR(lx.toks.v[i].t, want[i]);
    CHECK(lx.directives.n == 3);
    sh_lex_free(&lx);
    /* decoding as Python's errors="replace": one U+FFFD per maximal invalid subpart */
    CHECK_STR(sh_decode_utf8(a, AVP_S("a\xe2\x82" "b\xff\xf0\x9f\x98")), "a\xef\xbf\xbd" "b\xef\xbf\xbd\xef\xbf\xbd");
    i64 addr;
    CHECK(sh_addr_in_comment(AVP_S(" text\n   0x80001234  \n"), &addr) && addr == 0x80001234);
    CHECK(!sh_addr_in_comment(AVP_S(" see 0x80001234"), &addr));
}

/* ---- repositories ------------------------------------------------------------------------------------------------ */

typedef struct {
    AvpArena *a;
    char *tmp, *repo, *db;
    i64 clock;
} Repo;

static void git(Repo *r, const char *const *args, size_t n) {
    char *env[7];
    env[0] = "GIT_AUTHOR_NAME=t";
    env[1] = "GIT_AUTHOR_EMAIL=t@t";
    env[2] = "GIT_COMMITTER_NAME=t";
    env[3] = "GIT_COMMITTER_EMAIL=t@t";
    env[4] = avp_sprintf(r->a, "GIT_AUTHOR_DATE=%lld +0000", r->clock);
    env[5] = avp_sprintf(r->a, "GIT_COMMITTER_DATE=%lld +0000", r->clock);
    env[6] = NULL;
    ShsmGitOpts o = {0};
    o.env = (const char *const *)env;
    ShsmGitRun g = shsm_git_run(r->a, r->repo, args, n, &o);
    CHECK_MSG(shsm_git_ok(&g), "git %s: %s", args[0], g.err.p);
}
#define GIT(r, ...) git((r), SHSM_GIT_ARGS_(__VA_ARGS__))

static void repo_init(Repo *r) {
    memset(r, 0, sizeof *r);
    r->a = avp_arena();
    r->tmp = avp_mkdtemp(r->a, "shtest", NULL, NULL);
    REQUIRE(r->tmp != NULL);
    r->repo = avp_path_join(r->a, r->tmp, "repo");
    r->db = avp_path_join(r->a, r->tmp, "hist.sqlite");
    avp_makedirs(r->repo, true);
    r->clock = 1700000000;
    GIT(r, "init", "-q", "-b", "main");
}

static void repo_done(Repo *r) { avp_tempdir_cleanup(r->tmp); }

static void write_file(Repo *r, const char *rel, AvpStr text) {
    char *full = avp_path_join(r->a, r->repo, rel);
    char *dir = avp_cz(r->a, avp_path_dirname(full));
    avp_makedirs(dir, true);
    REQUIRE(avp_write_file(full, text.p, text.n));
}

typedef struct {
    const char *sym;
    i64 addr;
    double score;
} RF;
typedef struct {
    const char *name, *src;
    const RF *funcs;
    size_t n;
} RU;

/* make_report(units) plus the auto-generated unit */
static AvpStr make_report(AvpArena *a, const RU *units, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    AvpJsonW w;
    avp_jsonw_init(&w, &b, NULL);
    avp_jsonw_begin_object(&w);
    avp_jsonw_key(&w, AVP_S("units"));
    avp_jsonw_begin_array(&w);
    for (size_t i = 0; i < n; i++) {
        avp_jsonw_begin_object(&w);
        avp_jsonw_kv_str(&w, "name", avp_str(units[i].name));
        avp_jsonw_key(&w, AVP_S("metadata"));
        avp_jsonw_begin_object(&w);
        avp_jsonw_kv_str(&w, "source_path", avp_str(units[i].src));
        avp_jsonw_kv_bool(&w, "auto_generated", false);
        avp_jsonw_end_object(&w);
        avp_jsonw_key(&w, AVP_S("functions"));
        avp_jsonw_begin_array(&w);
        for (size_t k = 0; k < units[i].n; k++) {
            avp_jsonw_begin_object(&w);
            avp_jsonw_kv_str(&w, "name", avp_str(units[i].funcs[k].sym));
            avp_jsonw_kv_str(&w, "size", AVP_S("16"));
            avp_jsonw_key(&w, AVP_S("metadata"));
            avp_jsonw_begin_object(&w);
            avp_jsonw_kv_str(&w, "virtual_address", avp_str(avp_sprintf(a, "%lld", units[i].funcs[k].addr)));
            avp_jsonw_end_object(&w);
            avp_jsonw_kv_str(&w, "address", AVP_S("0"));
            avp_jsonw_kv_f64(&w, "fuzzy_match_percent", units[i].funcs[k].score);
            avp_jsonw_end_object(&w);
        }
        avp_jsonw_end_array(&w);
        avp_jsonw_end_object(&w);
    }
    avp_jsonw_begin_object(&w);
    avp_jsonw_kv_str(&w, "name", AVP_S("main/auto_00_80000000_text"));
    avp_jsonw_key(&w, AVP_S("metadata"));
    avp_jsonw_begin_object(&w);
    avp_jsonw_kv_bool(&w, "auto_generated", true);
    avp_jsonw_end_object(&w);
    avp_jsonw_key(&w, AVP_S("functions"));
    avp_jsonw_begin_array(&w);
    avp_jsonw_begin_object(&w);
    avp_jsonw_kv_str(&w, "name", AVP_S("fn_asm"));
    avp_jsonw_kv_str(&w, "size", AVP_S("4"));
    avp_jsonw_kv_f64(&w, "fuzzy_match_percent", 100.0);
    avp_jsonw_key(&w, AVP_S("metadata"));
    avp_jsonw_begin_object(&w);
    avp_jsonw_kv_str(&w, "virtual_address", AVP_S("2147483648"));
    avp_jsonw_end_object(&w);
    avp_jsonw_end_object(&w);
    avp_jsonw_end_array(&w);
    avp_jsonw_end_object(&w);
    avp_jsonw_end_array(&w);
    avp_jsonw_kv_i64(&w, "version", 2);
    avp_jsonw_end_object(&w);
    avp_jsonw_finish(&w);
    return avp_buf_str(&b);
}

/* commit(msg, foo, foo_scores, symbols, extra_inc): returns HEAD */
static char *commit(Repo *r, const char *msg, AvpStr foo, double s10, double s20, double s30, const char *sym20,
                    const char *extra_inc) {
    RF foo_funcs[] = {{"fn_80000010__3FooFi", A(0x10), s10},
                      {sym20 ? sym20 : "fn_80000020__3FooFv", A(0x20), s20},
                      {"fn_80000030__3FooFv", A(0x30), s30},
                      {"__sinit_\\Foo_cpp", A(0x40), 100.0}};
    RF os_funcs[] = {{"OSDoIt", A(0x100), 100.0}};
    RU units[] = {{"main/game/Foo", "src/game/Foo.cpp", foo_funcs, 4}, {"main/os/OSDoIt", "src/os/OSDoIt.c", os_funcs, 1}};
    write_file(r, "src/game/Foo.cpp", foo);
    write_file(r, "src/os/OSDoIt.c", avp_strn(PLAIN_C, sizeof PLAIN_C - 1));
    write_file(r, "include/game/Foo.h", avp_str(avp_sprintf(r->a, "// header %s\n", extra_inc ? extra_inc : "base")));
    write_file(r, "configure.py", AVP_S("# cfg\n"));
    write_file(r, "config/R5WEA4/baseline_report.json", make_report(r->a, units, 2));
    GIT(r, "add", "-A");
    r->clock += 100;
    GIT(r, "commit", "-q", "-m", msg);
    return shsm_git_rev_parse(r->a, r->repo, "HEAD");
}

/* run_cli(*argv): the output; the exit code must be 0 */
static AvpStr run_rc(Repo *r, int want_rc, int n, const char *const *argv) {
    const char **full = avp_alloc(r->a, (size_t)(n + 4) * sizeof(char *));
    full[0] = "--repo";
    full[1] = r->repo;
    full[2] = "--db";
    full[3] = r->db;
    for (int i = 0; i < n; i++) full[4 + i] = argv[i];
    AvpBuf out, err;
    avp_buf_init(&out, r->a);
    avp_buf_init(&err, r->a);
    int rc = score_history_run(r->a, n + 4, full, &out, &err);
    CHECK_MSG(rc == want_rc, "exit %d, want %d: %.*s", rc, want_rc, AVP_SF(avp_buf_str(&err)));
    return out.n ? avp_buf_str(&out) : AVP_S("");
}
#define RUN(r, ...) run_rc((r), 0, (int)(sizeof((const char *[]){__VA_ARGS__}) / sizeof(char *)), (const char *[]){__VA_ARGS__})

static AvpJson *status_json(Repo *r, const char *extra1, const char *extra2, const char *extra3) {
    const char *argv[5] = {"status", "--json"};
    int n = 2;
    if (extra1) argv[n++] = extra1;
    if (extra2) argv[n++] = extra2;
    if (extra3) argv[n++] = extra3;
    AvpStr t = run_rc(r, 0, n, argv);
    AvpJson *j = avp_json_parse(r->a, t, NULL);
    CHECK(j != NULL);
    return j ? j : avp_json_arr(r->a);
}

static AvpJson *by_addr(AvpJson *list, const char *addr) {
    for (size_t i = 0; i < avp_json_len(list); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(list, i), "address", AVP_S("")), addr)) return avp_json_at(list, i);
    return NULL;
}

static double num(AvpJson *d, const char *k) { return avp_json_get_f64(d, k, -1); }
static bool contains(AvpStr hay, const char *needle) { return avp_str_contains(hay, avp_str(needle)); }

typedef struct {
    char *c1, *c2, *c3, *c4;
} Hist;

static Hist build_history(Repo *r) {
    Hist h;
    h.c1 = commit(r, "one", foo_v1(), 100, 50, 100, NULL, NULL);
    h.c2 = commit(r, "two", foo_v2(r->a), 80, 50, 100, NULL, NULL);
    h.c3 = commit(r, "three", foo_v3(r->a), 80, 60, 100, "Foo_RealName__3FooFv", NULL);
    h.c4 = commit(r, "four", avp_str_concat(r->a, foo_v3(r->a), AVP_S("\n// tail\nint extra;\n")), 80, 60, 90,
                  "Foo_RealName__3FooFv", NULL);
    RUN(r, "backfill");
    return h;
}

static i64 count(Repo *r, const char *sql) {
    sqlite3 *db;
    sqlite3_stmt *st;
    if (sqlite3_open_v2(r->db, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        CHECK_MSG(false, "cannot query %s", r->db);
        return -1;
    }
    sqlite3_step(st);
    i64 n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    sqlite3_close(db);
    return n;
}

AVP_TEST(backfill_idempotent) {
    Repo r;
    repo_init(&r);
    Hist h = build_history(&r);
    AvpStr out = RUN(&r, "backfill");
    CHECK(contains(out, "0 new snapshot(s)"));
    CHECK(contains(out, "4 snapshot(s) in the database"));
    out = RUN(&r, "record", "--rev", h.c2);
    CHECK(contains(out, "already recorded"));
    CHECK_EQ_I64(count(&r, "select count(*) from snapshots"), 4);
    CHECK_EQ_I64(count(&r, "select count(*) from rows"), 20);
    repo_done(&r);
}

AVP_TEST(current_best_historical_and_lost) {
    Repo r;
    repo_init(&r);
    Hist h = build_history(&r);
    AvpJson *rows = status_json(&r, NULL, NULL, NULL);
    AvpJson *f10 = by_addr(rows, "0x80000010");
    REQUIRE(f10 != NULL);
    CHECK(num(f10, "current") == 80 && num(f10, "best") == 80 && num(f10, "historical") == 100);
    CHECK(avp_json_get_bool(f10, "lost", false));
    CHECK_STR(avp_json_get_str(f10, "historical_key", AVP_S("")), h.c1);
    CHECK_STR(avp_json_get_str(f10, "historical_path", AVP_S("")), "src/game/Foo.cpp");
    CHECK_STR(avp_json_get_str(f10, "basis", AVP_S("")), "addr");
    AvpJson *f20 = by_addr(rows, "0x80000020");
    REQUIRE(f20 != NULL);
    CHECK(num(f20, "current") == 60 && num(f20, "best") == 60 && num(f20, "historical") == 60);
    CHECK(!avp_json_get_bool(f20, "lost", true));
    CHECK_STR(avp_json_get_str(f20, "symbol", AVP_S("")), "Foo_RealName__3FooFv");
    AvpJson *lost = status_json(&r, "--lost", NULL, NULL);
    CHECK(avp_json_len(lost) == 1 && by_addr(lost, "0x80000010"));
    AvpStr text = RUN(&r, "status", "--lost");
    CHECK(contains(text, avp_sprintf(r.a, "git show %.12s:src/game/Foo.cpp", h.c1)));
    repo_done(&r);
}

AVP_TEST(same_source_best_spans_snapshots) {
    Repo r;
    repo_init(&r);
    commit(&r, "one", foo_v1(), 100, 50, 100, NULL, NULL);
    commit(&r, "two", foo_v2(r.a), 80, 50, 100, NULL, NULL);
    commit(&r, "three", foo_reflowed(r.a), 90, 50, 100, NULL, NULL);
    RUN(&r, "backfill");
    AvpJson *f10 = by_addr(status_json(&r, NULL, NULL, NULL), "0x80000010");
    REQUIRE(f10 != NULL);
    CHECK(num(f10, "current") == 90 && num(f10, "best") == 100 && num(f10, "historical") == 100);
    CHECK(!avp_json_get_bool(f10, "lost", true));
    CHECK(avp_json_is_null(avp_json_getz(f10, "regression")));
    repo_done(&r);
}

AVP_TEST(regressions_classified) {
    Repo r;
    repo_init(&r);
    Hist h = build_history(&r);
    AvpJson *reg = status_json(&r, "--regressed", NULL, NULL);
    CHECK(avp_json_len(reg) == 1);
    CHECK_STR(avp_json_get_str(by_addr(reg, "0x80000030"), "regression", AVP_S("")), "unit changed elsewhere");
    char *vs = avp_sprintf(r.a, "%.8s", h.c1);
    reg = status_json(&r, "--regressed", "--vs", vs);
    CHECK_STR(avp_json_get_str(by_addr(reg, "0x80000010"), "regression", AVP_S("")), "source changed");
    CHECK_STR(avp_json_get_str(by_addr(reg, "0x80000030"), "regression", AVP_S("")), "unit changed elsewhere");
    AvpStr text = RUN(&r, "status", "--regressed", "--vs", vs);
    CHECK(contains(text, "== changed source: 1"));
    CHECK(contains(text, "== unchanged source: 1"));
    repo_done(&r);
}

AVP_TEST(header_change_label) {
    Repo r;
    repo_init(&r);
    commit(&r, "one", foo_v1(), 100, 50, 100, NULL, NULL);
    commit(&r, "two", foo_v1(), 100, 50, 90, NULL, "edited");
    RUN(&r, "backfill");
    AvpJson *reg = status_json(&r, "--regressed", NULL, NULL);
    CHECK(avp_json_len(reg) == 1);
    CHECK_STR(avp_json_get_str(by_addr(reg, "0x80000030"), "regression", AVP_S("")), "headers/flags changed");
    repo_done(&r);
}

AVP_TEST(unit_filter_summary_and_show) {
    Repo r;
    repo_init(&r);
    build_history(&r);
    AvpStr text = RUN(&r, "status", "--unit", "game/Foo");
    CHECK(contains(text, "fn_80000010__3FooFi"));
    CHECK(!contains(text, "OSDoIt"));
    CHECK(contains(RUN(&r, "status", "--unit", "main/os/OSDoIt"), "OSDoIt"));
    AvpStr summary = RUN(&r, "status");
    CHECK(contains(summary, "snapshots: 4"));
    CHECK(contains(summary, "lost matches (historical > best): 1"));
    const char *specs[] = {"fn_80000020__3FooFv", "Foo_RealName__3FooFv", "0x80000020"};
    for (size_t i = 0; i < 3; i++) {
        AvpStr t = RUN(&r, "show", specs[i]);
        CHECK(contains(t, "fn_80000020__3FooFv"));
        CHECK(contains(t, "Foo_RealName__3FooFv"));
    }
    AvpStr t = RUN(&r, "show", "fn_80000010__3FooFi");
    CHECK(contains(t, "100.00"));
    CHECK(contains(t, "*"));
    CHECK(contains(RUN(&r, "show", "0x80000010", "--source"), "x + 2"));
    AvpStr old_hash = avp_json_get_str(by_addr(status_json(&r, NULL, NULL, NULL), "0x80000010"), "historical_hash", AVP_S(""));
    CHECK(contains(RUN(&r, "show", "0x80000010", "--source", avp_cz(r.a, avp_str_slice(old_hash, 0, 8))), "x + 1"));
    repo_done(&r);
}

AVP_TEST(unlocated_function_uses_unit_hash) {
    Repo r;
    repo_init(&r);
    commit(&r, "one", foo_v1(), 100, 50, 100, NULL, NULL);
    RUN(&r, "backfill");
    AvpJson *rows = status_json(&r, NULL, NULL, NULL);
    AvpJson *f = by_addr(rows, "0x80000040");
    REQUIRE(f != NULL);
    CHECK_STR(avp_json_get_str(f, "symbol", AVP_S("")), "__sinit_\\Foo_cpp");
    CHECK_STR(avp_json_get_str(f, "basis", AVP_S("")), "sinit");
    repo_done(&r);
}

static void set_mtime(const char *p, double t) {
    i64 ns = (i64)(t * 1e9);
    CHECK(avp_utime_ns(p, ns, ns));
}

AVP_TEST(worktree_record) {
    Repo r;
    repo_init(&r);
    commit(&r, "one", foo_v1(), 100, 50, 100, NULL, NULL);
    RUN(&r, "backfill");
    RF foo_funcs[] = {{"fn_80000010__3FooFi", A(0x10), 100.0},
                      {"fn_80000020__3FooFv", A(0x20), 50.0},
                      {"fn_80000030__3FooFv", A(0x30), 70.0},
                      {"__sinit_\\Foo_cpp", A(0x40), 100.0}};
    RF os_funcs[] = {{"OSDoIt", A(0x100), 100.0}};
    RU units[] = {{"main/game/Foo", "src/game/Foo.cpp", foo_funcs, 4}, {"main/os/OSDoIt", "src/os/OSDoIt.c", os_funcs, 1}};
    write_file(&r, "build/R5WEA4/report.json", make_report(r.a, units, 2));
    char *rp = avp_path_join(r.a, r.repo, "build/R5WEA4/report.json");
    double future = avp_time() + 5;
    set_mtime(rp, future);
    CHECK(contains(RUN(&r, "record"), "recorded worktree:"));
    CHECK(contains(RUN(&r, "record"), "unchanged, timestamp refreshed"));
    AvpJson *rows = status_json(&r, NULL, NULL, NULL);
    AvpJson *f30 = by_addr(rows, "0x80000030");
    REQUIRE(f30 != NULL);
    CHECK(num(f30, "current") == 70 && num(f30, "historical") == 100);
    CHECK(!avp_json_get_bool(f30, "lost", true));
    CHECK(!avp_json_is_null(avp_json_getz(f30, "regression")));
    write_file(&r, "src/game/Foo.cpp", foo_v2(r.a));
    double past = avp_time() - 600;
    set_mtime(rp, past);
    set_mtime(avp_path_join(r.a, r.repo, "src/os/OSDoIt.c"), past - 100);
    AvpStr out = RUN(&r, "record");
    CHECK(contains(out, "skipped 1 unit(s)"));
    CHECK(contains(out, "main/game/Foo"));
    CHECK(contains(RUN(&r, "record", "--include-stale"), "recorded worktree:"));
    AvpJson *f10 = by_addr(status_json(&r, NULL, NULL, NULL), "0x80000010");
    CHECK(num(f10, "best") == 100);
    repo_done(&r);
}

AVP_TEST(lost_in_uncommitted_snapshot_points_to_stored_source) {
    Repo r;
    repo_init(&r);
    commit(&r, "one", foo_v1(), 50, 50, 100, NULL, NULL);
    RUN(&r, "backfill");
    RF f1[] = {{"fn_80000010__3FooFi", A(0x10), 99.0}};
    RU u1[] = {{"main/game/Foo", "src/game/Foo.cpp", f1, 1}};
    char *rp = avp_path_join(r.a, r.repo, "r.json");
    AvpStr rep1 = make_report(r.a, u1, 1);
    REQUIRE(avp_write_file(rp, rep1.p, rep1.n));
    write_file(&r, "src/game/Foo.cpp", foo_v2(r.a));
    double future = avp_time() + 5;
    set_mtime(rp, future);
    RUN(&r, "record", "--report", rp);
    write_file(&r, "src/game/Foo.cpp", foo_v1());
    RF f2[] = {{"fn_80000010__3FooFi", A(0x10), 60.0}};
    RU u2[] = {{"main/game/Foo", "src/game/Foo.cpp", f2, 1}};
    AvpStr rep2 = make_report(r.a, u2, 1);
    REQUIRE(avp_write_file(rp, rep2.p, rep2.n));
    set_mtime(rp, future + 5);
    RUN(&r, "record", "--report", rp);
    AvpStr text = RUN(&r, "status", "--lost");
    CHECK(contains(text, "uncommitted version"));
    CHECK(contains(text, "show 0x80000010 --source"));
    repo_done(&r);
}

/* ---- the row key and the old database ---------------------------------------------------------------------------- */

static const char OLD_ROWS_SCHEMA[] =
    "CREATE TABLE snapshots (id INTEGER PRIMARY KEY, key TEXT UNIQUE NOT NULL, kind TEXT NOT NULL,\n"
    "    time INTEGER NOT NULL, subject TEXT NOT NULL DEFAULT '', report_sha TEXT NOT NULL DEFAULT '',\n"
    "    env_sig TEXT NOT NULL DEFAULT '');\n"
    "CREATE TABLE rows (snap INTEGER NOT NULL REFERENCES snapshots(id), symbol TEXT NOT NULL, addr INTEGER NOT NULL,\n"
    "    unit TEXT NOT NULL, path TEXT NOT NULL DEFAULT '', score REAL NOT NULL, size INTEGER NOT NULL DEFAULT 0,\n"
    "    hash TEXT NOT NULL, basis TEXT NOT NULL, unit_hash TEXT NOT NULL DEFAULT '',\n"
    "    line_lo INTEGER NOT NULL DEFAULT 0, line_hi INTEGER NOT NULL DEFAULT 0,\n"
    "    PRIMARY KEY (snap, symbol)) WITHOUT ROWID;\n"
    "CREATE INDEX rows_addr ON rows(addr);\n";

static void exec_sql(sqlite3 *db, const char *sql) { CHECK(sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK); }

static void make_old_db(Repo *r) {
    sqlite3 *db;
    REQUIRE(sqlite3_open(r->db, &db) == SQLITE_OK);
    exec_sql(db, OLD_ROWS_SCHEMA);
    exec_sql(db, "INSERT INTO snapshots(id, key, kind, time, subject) VALUES (1, 'aaaa', 'git', 1, 'one')");
    exec_sql(db, "INSERT INTO snapshots(id, key, kind, time, subject) VALUES (2, 'bbbb', 'git', 2, 'two')");
    for (int snap = 1; snap <= 2; snap++) {
        double score = snap == 1 ? 100.0 : 70.0;
        exec_sql(db, avp_sprintf(r->a, "INSERT INTO rows(snap, symbol, addr, unit, score, hash, basis) VALUES "
                                       "(%d, 'fn_80000010', %lld, 'main/x', %.1f, 'h1-%d', 'addr')",
                                 snap, A(0x10), score, snap));
        exec_sql(db, avp_sprintf(r->a, "INSERT INTO rows(snap, symbol, addr, unit, score, hash, basis) VALUES "
                                       "(%d, 'OSDoIt', %lld, 'main/os', %.1f, 'h2', 'addr')",
                                 snap, A(0x100), 50.0 + snap));
    }
    sqlite3_close(db);
}

static void commit_units(Repo *r, const RU *units, size_t n) {
    write_file(r, "configure.py", AVP_S("# cfg\n"));
    write_file(r, "include/x.h", AVP_S("// x\n"));
    write_file(r, "config/R5WEA4/baseline_report.json", make_report(r->a, units, n));
    GIT(r, "add", "-A");
    r->clock += 100;
    GIT(r, "commit", "-q", "-m", "c");
}

AVP_TEST(same_name_in_two_units_is_tracked_twice) {
    Repo r;
    repo_init(&r);
    write_file(&r, "src/a.c", AVP_S("static int helper(int b) { return b; } // 0x80000500\n"));
    write_file(&r, "src/b.c", AVP_S("static int helper(int b) { return b + 1; } // 0x80000600\n"));
    RF fa[] = {{"helper", A(0x500), 100.0}}, fb[] = {{"helper", A(0x600), 40.0}};
    RU units[] = {{"main/a", "src/a.c", fa, 1}, {"main/b", "src/b.c", fb, 1}};
    commit_units(&r, units, 2);
    RUN(&r, "backfill");
    AvpJson *rows = status_json(&r, NULL, NULL, NULL);
    CHECK_STR(avp_json_get_str(by_addr(rows, "0x80000500"), "unit", AVP_S("")), "main/a");
    CHECK_STR(avp_json_get_str(by_addr(rows, "0x80000600"), "unit", AVP_S("")), "main/b");
    CHECK(num(by_addr(rows, "0x80000500"), "current") == 100 && num(by_addr(rows, "0x80000600"), "current") == 40);
    repo_done(&r);
}

AVP_TEST(old_database_is_migrated_in_place) {
    Repo r;
    repo_init(&r);
    make_old_db(&r);
    ShCtx c = {r.a, NULL, r.repo, NULL, NULL};
    sh_db_open(&c, r.db);
    sh_db_close(&c);
    CHECK_EQ_I64(count(&r, "SELECT COUNT(*) FROM rows"), 4);
    CHECK_EQ_I64(count(&r, "SELECT COUNT(*) FROM snapshots"), 2);
    CHECK_EQ_I64(count(&r, "SELECT COUNT(*) FROM sqlite_master WHERE name='rows_addr'"), 1);
    CHECK_EQ_I64(count(&r, "SELECT COUNT(*) FROM sqlite_master WHERE name='rows_migrating'"), 0);
    CHECK_EQ_I64(count(&r, "SELECT group_concat(name) = 'snap,unit,addr,symbol' FROM "
                           "(SELECT name FROM pragma_table_info('rows') WHERE pk > 0 ORDER BY pk)"), 1);
    sh_db_open(&c, r.db); /* idempotent */
    sh_db_close(&c);
    AvpJson *f = by_addr(status_json(&r, NULL, NULL, NULL), "0x80000010");
    REQUIRE(f != NULL);
    CHECK(num(f, "current") == 70 && num(f, "best") == 70 && num(f, "historical") == 100);
    sqlite3 *db;
    REQUIRE(sqlite3_open(r.db, &db) == SQLITE_OK);
    exec_sql(db, avp_sprintf(r.a, "INSERT INTO rows(snap, symbol, addr, unit, score, hash, basis) VALUES (2,'OSDoIt',%lld,"
                                  " 'main/y', 10.0, 'h3', 'addr')", A(0x700)));
    sqlite3_close(db);
    CHECK(by_addr(status_json(&r, NULL, NULL, NULL), "0x80000700") != NULL);
    repo_done(&r);
}

AVP_TEST(show_json_has_current_best_historical) {
    Repo r;
    repo_init(&r);
    make_old_db(&r);
    AvpJson *d = avp_json_parse(r.a, RUN(&r, "show", "0x80000010", "--json"), NULL);
    REQUIRE(d != NULL);
    AvpJson *f = avp_json_getz(d, "function");
    CHECK(num(f, "current") == 70 && num(f, "best") == 70 && num(f, "historical") == 100);
    AvpJson *tl = avp_json_getz(d, "timeline");
    CHECK(avp_json_len(tl) == 2 && num(avp_json_at(tl, 0), "score") == 100 && num(avp_json_at(tl, 1), "score") == 70);
    repo_done(&r);
}

AVP_TEST(errors_exit_1_with_a_message) {
    Repo r;
    repo_init(&r);
    make_old_db(&r);
    const char *argv[] = {"show", "nosuch"};
    run_rc(&r, 1, 2, argv);
    const char *argv2[] = {"status", "--vs", "zzz"};
    run_rc(&r, 1, 3, argv2);
    const char *argv3[] = {"nosuch"};
    run_rc(&r, 2, 1, argv3);
    repo_done(&r);
}

AVP_TEST_MAIN("score_history")
