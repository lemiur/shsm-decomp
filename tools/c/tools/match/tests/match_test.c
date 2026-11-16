/* match: the pure pieces of the port against values tools/match.py gives (dependency files, compiler error lines,
 * function and summary lines, the folding of exact functions, changes, JSON equality). The tool as a whole is checked
 * by the differential harness (diff/cases.txt). */
#include "avp/test.h"
#include "match.h"

static AvpJson *rec(const char *name, const char *status, i64 rows, i64 targets, const double *pct, i64 size) {
    AvpArena *a = avp_arena();
    AvpJson *r = avp_json_obj(a);
    avp_json_set(a, r, AVP_S("name"), avp_json_strz(a, name));
    avp_json_set(a, r, AVP_S("status"), avp_json_strz(a, status));
    avp_json_set(a, r, AVP_S("rows"), avp_json_int(a, rows));
    avp_json_set(a, r, AVP_S("targets"), avp_json_int(a, targets));
    avp_json_set(a, r, AVP_S("unresolved"), avp_json_int(a, 0));
    avp_json_set(a, r, AVP_S("percent"), pct ? avp_json_float(a, *pct) : avp_json_null());
    avp_json_set(a, r, AVP_S("size"), avp_json_int(a, size));
    return r;
}

static AvpJson *strs(const char *const *v, size_t n) {
    AvpJson *arr = avp_json_arr(avp_arena());
    for (size_t i = 0; i < n; i++) avp_json_arr_push(avp_arena(), arr, avp_json_strz(avp_arena(), v[i]));
    return arr;
}

static AvpJson *sample_records(void) {
    AvpArena *a = avp_arena();
    double p100 = 100.0, p87 = 87.123456, p0 = 0.0;
    AvpJson *recs = avp_json_arr(a);
    AvpJson *e = rec("a", "EXACT", 0, 0, &p100, 0x40);
    static const char *const vr[] = {"x", "y"};
    avp_json_set(a, e, AVP_S("value_rows"), strs(vr, 2));
    avp_json_arr_push(a, recs, e);
    avp_json_arr_push(a, recs, rec("b", "DIFF", 3, 1, &p87, 0x100));
    avp_json_arr_push(a, recs, rec("c", "MISSING", 0, 0, &p0, 0));
    avp_json_arr_push(a, recs, rec("d", "EXTRA", 0, 0, NULL, 8));
    AvpJson *p = rec("e", "MATCH-PENDING", 0, 0, &p100, 12);
    static const char *const mr[] = {"R4", "R17", "R4"};
    avp_json_set(a, p, AVP_S("match_rules"), strs(mr, 3));
    avp_json_arr_push(a, recs, p);
    return recs;
}

AVP_TEST(parse_depfile) {
    AvpStrVec d = match_parse_depfile(avp_arena(), AVP_S("x.o: a.c \\\n  b\\ c.h  sub/d.h a.c C:\\abs\\e.h\n"), "X:\\r");
    REQUIRE(d.n == 4);
    CHECK_STR(d.v[0], "X:\\r\\a.c");
    CHECK_STR(d.v[1], "X:\\r\\b c.h");
    CHECK_STR(d.v[2], "X:\\r\\sub\\d.h");
    CHECK_STR(d.v[3], "C:\\abs\\e.h");
}

AVP_TEST(first_error_lines) {
    AvpStrVec v = match_first_error_lines(avp_arena(), AVP_S("### Usage Warning\n# a\n# b\nerr1  \n\n# c\nerr2\n"), 12);
    REQUIRE(v.n == 3);
    CHECK_STR(v.v[0], "err1");
    CHECK_STR(v.v[1], "# c");
    CHECK_STR(v.v[2], "err2");
    v = match_first_error_lines(avp_arena(), AVP_S("1\n2\n3\n4\n"), 3);
    REQUIRE(v.n == 4);
    CHECK_STR(v.v[3], "... 1 more lines, more compiler output");
}

AVP_TEST(function_lines) {
    AvpJson *recs = sample_records();
    static const char *const want[] = {
        "EXACT                  2 row(s) by value    0x40  a",
        "DIFF            87.12% 3 rows 1 tgt        0x100  b",
        "MISSING          0.00% not in the object     0x0  c",
        "EXTRA                  not in retail         0x8  d",
        "MATCH-PENDING  100.00%                       0xc  e  (byte-exact, [match] R17,R4)",
    };
    for (size_t i = 0; i < AVP_ARRAY_LEN(want); i++) CHECK_STR(match_function_line(avp_arena(), avp_json_at(recs, i)), want[i]);
}

AVP_TEST(summary_line) {
    AvpArena *a = avp_arena();
    AvpJson *res = avp_json_obj(a);
    avp_json_set(a, res, AVP_S("unit"), avp_json_strz(a, "main/x"));
    avp_json_set(a, res, AVP_S("ok"), avp_json_bool(true));
    avp_json_set(a, res, AVP_S("functions"), sample_records());
    AvpJson *lay = avp_json_obj(a);
    avp_json_set(a, lay, AVP_S("status"), avp_json_strz(a, "differs"));
    avp_json_set(a, res, AVP_S("layout"), lay);
    AvpJson *rr = avp_json_obj(a);
    avp_json_set(a, rr, AVP_S("ok"), avp_json_bool(true));
    static const char *const f[] = {"1", "2"};
    avp_json_set(a, rr, AVP_S("findings"), strs(f, 2));
    avp_json_set(a, rr, AVP_S("r13_note"), avp_json_strz(a, "skipped"));
    avp_json_set(a, res, AVP_S("rules"), rr);
    avp_json_set(a, res, AVP_S("percent_source"), avp_json_strz(a, "objdiff"));
    avp_json_set(a, res, AVP_S("cached"), avp_json_bool(true));
    AvpJson *sec = avp_json_obj(a);
    avp_json_set(a, sec, AVP_S("compile"), avp_json_float(a, 1.234));
    avp_json_set(a, sec, AVP_S("compare"), avp_json_float(a, 0.5));
    avp_json_set(a, sec, AVP_S("total"), avp_json_float(a, 2.0));
    avp_json_set(a, res, AVP_S("seconds"), sec);
    CHECK_STR(match_summary_line(a, res),
              "main/x: 1/4 exact (1 diff, 1 missing, 1 match-pending, 1 extra) worst c 0.00% layout differs rules 2 "
              "[match], R13 NOT CHECKED | % from objdiff | compile 1.23s (cached), compare 0.50s, total 2.00s");
    MatchSummary s = match_summarise(res);
    CHECK(!s.matched && s.layout_bad && s.total == 4 && s.exact == 1);
    /* more lines than the limit: exact functions folded into one line */
    AvpStrVec v = match_unit_lines(a, res, false, 2);
    REQUIRE(v.n == 4);
    CHECK_STR(v.v[2], "... 2 more lines, use --all");
    CHECK_STR(v.v[3], "(1 exact functions not listed, use --all)");
    CHECK_EQ_U64(match_unit_lines(a, res, true, 2).n, 5);
}

AVP_TEST(changes) {
    AvpArena *a = avp_arena();
    AvpJson *old = avp_json_obj(a), *of = avp_json_obj(a);
    double p50 = 50.0, p60 = 60.0, p100 = 100.0;
    avp_json_set(a, of, AVP_S("b"), rec("b", "DIFF", 3, 0, &p50, 4));
    avp_json_set(a, of, AVP_S("c"), rec("c", "EXACT", 0, 0, &p100, 4));
    avp_json_set(a, of, AVP_S("gone"), rec("gone", "DIFF", 1, 0, &p50, 4));
    avp_json_set(a, of, AVP_S("same"), rec("same", "EXACT", 0, 0, &p100, 4));
    avp_json_set(a, old, AVP_S("functions"), of);
    AvpJson *now = avp_json_arr(a);
    avp_json_arr_push(a, now, rec("b", "DIFF", 2, 0, &p60, 4));
    avp_json_arr_push(a, now, rec("c", "DIFF", 1, 0, &p60, 4));
    avp_json_arr_push(a, now, rec("same", "EXACT", 0, 0, &p100, 4));
    avp_json_arr_push(a, now, rec("new", "EXACT", 0, 0, &p100, 4));
    MatchChange *ch = NULL;
    size_t n = match_changes(a, old, now, false, &ch);
    REQUIRE(n == 4);
    CHECK_CZ(ch[0].kind, "improved");
    CHECK_CZ(ch[1].kind, "newly broken");
    CHECK_CZ(ch[2].kind, "new");
    CHECK_CZ(ch[3].kind, "gone");
    CHECK_EQ_U64(match_changes(a, old, now, true, &ch), 3);
}

AVP_TEST(stale_asm_line) {
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "matchstale", "", NULL);
    REQUIRE(dir != NULL);
    ShsmRepo r = {0};
    r.root = dir;
    r.version = "R5WEA4";
    r.asm_dir = avp_join(a, dir, "asm");
    r.symbols_txt = avp_join(a, dir, "symbols.txt");
    r.splits_txt = avp_join(a, dir, "splits.txt");
    const char *asm_path = shsm_repo_asm_path(a, &r, "main/game/Foo");
    REQUIRE(avp_makedirs(avp_cz(a, avp_path_dirname(asm_path)), true));
    REQUIRE(avp_write_file(asm_path, "x", 1));
    REQUIRE(avp_write_file(r.symbols_txt, "x", 1));
    REQUIRE(avp_write_file(r.splits_txt, "x", 1));
    const i64 sec = 1000000000LL;
    REQUIRE(avp_utime_ns(asm_path, 2000 * sec, 2000 * sec));
    REQUIRE(avp_utime_ns(r.symbols_txt, 1000 * sec, 1000 * sec));
    REQUIRE(avp_utime_ns(r.splits_txt, 1000 * sec, 1000 * sec));
    CHECK(match_stale_asm_line(a, &r, "main/game/Foo") == NULL);
    REQUIRE(avp_utime_ns(r.symbols_txt, 3000 * sec, 3000 * sec));
    const char *line = match_stale_asm_line(a, &r, "main/game/Foo");
    REQUIRE(line != NULL);
    CHECK(strstr(line, "is older than config/R5WEA4/symbols.txt") != NULL);
    CHECK(strstr(line, "run `tools/bin/ninja.exe`") != NULL);
    REQUIRE(avp_utime_ns(r.symbols_txt, 1000 * sec, 1000 * sec));
    REQUIRE(avp_utime_ns(r.splits_txt, 3000 * sec, 3000 * sec));
    line = match_stale_asm_line(a, &r, "main/game/Foo");
    REQUIRE(line != NULL);
    CHECK(strstr(line, "splits.txt") != NULL);
    CHECK(match_stale_asm_line(a, &r, "main/game/NoAsm") == NULL);
    avp_tempdir_cleanup(dir);
}

AVP_TEST(json_eq) {
    AvpArena *a = avp_arena();
    AvpJson *x = avp_json_parse(a, AVP_S("{\"a\": 1, \"b\": [1.0, null, \"s\"]}"), NULL);
    AvpJson *y = avp_json_parse(a, AVP_S("{\"b\": [1, null, \"s\"], \"a\": 1.0}"), NULL);
    CHECK(match_json_eq(x, y));
    CHECK(match_json_eq(match_json_copy(a, x), x));
    CHECK(!match_json_eq(x, avp_json_parse(a, AVP_S("{\"a\": 1}"), NULL)));
    CHECK(match_json_eq(NULL, avp_json_null()));
}

AVP_TEST_MAIN("match")
