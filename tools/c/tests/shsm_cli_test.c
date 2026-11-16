/* shsm_cli: capped lines, --limit parsing, exception texts, subprocess runs with spaces and quotes, timeouts. */
#include "shsm_cli.h"
#include "shsm_test_util.h"

AVP_TEST(cap_lines) {
    AvpStr lines[] = {AVP_S("a"), AVP_S("b"), AVP_S("c")};
    AvpStrVec v = shsm_cap_lines(avp_arena(), lines, 3, 2, false, "use --all");
    REQUIRE(v.n == 3);
    CHECK_STR(v.v[2], "... 1 more lines, use --all");
    v = shsm_cap_lines(avp_arena(), lines, 3, 0, false, "x");
    CHECK_EQ_U64(v.n, 3);
    v = shsm_cap_lines(avp_arena(), lines, 3, 1, true, "x");
    CHECK_EQ_U64(v.n, 3);
    v = shsm_cap_lines(avp_arena(), lines, 3, 3, false, "x");
    CHECK_EQ_U64(v.n, 3);
    v = shsm_cap_lines(avp_arena(), lines, 3, 1, false, "more compiler output");
    CHECK_STR(v.v[1], "... 2 more lines, more compiler output");
}

AVP_TEST(limit_type) {
    i64 n = -1;
    AvpStr msg = {0};
    CHECK(shsm_limit_type(avp_arena(), AVP_S(" ALL "), &n, &msg) && n == 0);
    CHECK(shsm_limit_type(avp_arena(), AVP_S("inf"), &n, &msg) && n == 0);
    CHECK(shsm_limit_type(avp_arena(), AVP_S("12"), &n, &msg) && n == 12);
    CHECK(shsm_limit_type(avp_arena(), AVP_S("1_0"), &n, &msg) && n == 10);
    CHECK(!shsm_limit_type(avp_arena(), AVP_S("-3"), &n, &msg));
    CHECK_STR(msg, "a line count cannot be negative");
    CHECK(!shsm_limit_type(avp_arena(), AVP_S("x'y"), &n, &msg));
    CHECK_STR(msg, "expected a line count or 'all', got \"x'y\"");
}

AVP_TEST(exception_texts) {
    CHECK_STR(shsm_exc_str(avp_arena(), SHSM_EXC_KEY, "unknown unit x"), "'unknown unit x'");
    CHECK_STR(shsm_exc_main_text(avp_arena(), SHSM_EXC_KEY, "unknown unit x"), "unknown unit x");
    CHECK_STR(shsm_exc_str(avp_arena(), SHSM_EXC_UNIT_NOT_BUILT, "unit x"), "unit x");
    CHECK_STR(shsm_exc_main_text(avp_arena(), SHSM_EXC_VALUE, "first\nsecond"), "first");
    CHECK_STR(shsm_exc_main_text(avp_arena(), SHSM_EXC_OS, ""), "OSError");
    CHECK(shsm_exc_is_key(SHSM_EXC_UNIT_NOT_BUILT) && shsm_exc_is_os(SHSM_EXC_FILE_NOT_FOUND));
}

static const char *child_exe(void) { return avp_path_abspath(avp_arena(), "build/tools/tests/shsm_child.exe"); }

AVP_TEST(run_quoting_and_spaces) {
    const char *dir = fx_dir("cli dir with spaces");
    const char *args[] = {child_exe(), "args", "a b", "q\"x", "", "back\\slash\\","tail\\\" q", "é"};
    ShsmRun r = shsm_run(avp_arena(), args, AVP_ARRAY_LEN(args), dir, 60);
    REQUIRE(shsm_run_ok(&r));
    AvpJson *got = avp_json_parse(avp_arena(), r.out, NULL);
    REQUIRE(avp_json_len(got) == AVP_ARRAY_LEN(args) - 1);
    CHECK(avp_str_eqz(avp_json_at(got, 0)->u.str, dir));
    for (size_t i = 2; i < AVP_ARRAY_LEN(args); i++)
        CHECK_MSG(avp_str_eqz(avp_json_at(got, i - 1)->u.str, args[i]), "argument %zu: %.*s", i,
                  AVP_SF(avp_json_at(got, i - 1)->u.str));
}

AVP_TEST(run_exit_timeout_missing) {
    const char *fail[] = {child_exe(), "fail"};
    ShsmRun r = shsm_run(avp_arena(), fail, 2, NULL, 60);
    CHECK_EQ_I64(r.returncode, 3);
    CHECK(!shsm_run_ok(&r));
    CHECK_STR(r.err, "bad thing\nmore");
    CHECK_STR(shsm_run_output(avp_arena(), &r), "out\nbad thing\nmore");
    CHECK_RAISES(shsm_run_check(avp_arena(), &r, "C:\\x\\child.exe"), SHSM_EXC_USAGE, "child.exe failed (exit 3): out");
    /* a child that outlives its timeout: the timeout leaves room for a slow interpreter start on a loaded machine
     * (the partial output must be there), and the child sleeps far longer than any start, so a run that returns
     * well before the sleep ends was cut by the timeout */
    const char *slow[] = {child_exe(), "slow"};
    ShsmRun s = shsm_run(avp_arena(), slow, 2, NULL, 10.0);
    CHECK(s.timed_out);
    CHECK_EQ_I64(s.returncode, -1);
    CHECK_STR(s.out, "early\n");
    CHECK(s.seconds >= 9.0 && s.seconds < 200);
    const char *missing[] = {"no such program here", "x"};
    CHECK_RAISES(shsm_run(avp_arena(), missing, 2, NULL, 5), SHSM_EXC_USAGE,
                 "cannot run no such program here: [WinError 2] The system cannot find the file specified");
}

AVP_TEST(parser_limit) {
    AvpArgParser *p = shsm_cli_parser("t.py", "test", true, true, true);
    AvpArgsExit ex;
    AvpStr a1[] = {AVP_S("--limit"), AVP_S("all"), AVP_S("--json")};
    AvpArgs *r = avp_args_try_parse(p, a1, 3, false, &ex);
    REQUIRE(r);
    CHECK_EQ_I64(avp_args_int(r, "limit"), 0);
    CHECK(avp_args_bool(r, "json"));
    AvpArgs *r2 = avp_args_try_parse(p, NULL, 0, false, &ex);
    REQUIRE(r2);
    CHECK_EQ_I64(avp_args_int(r2, "limit"), 40);
    CHECK(avp_args_is_none(r2, "root"));
    AvpStr a3[] = {AVP_S("--limit"), AVP_S("-1")};
    CHECK(avp_args_try_parse(p, a3, 2, false, &ex) == NULL);
    CHECK_EQ_I64(ex.code, 2);
    CHECK_MSG(avp_str_contains(ex.err, AVP_S("t.py: error: argument --limit: a line count cannot be negative")),
              "%.*s", AVP_SF(ex.err));
    AvpStr help = avp_args_format_help(p);
    CHECK(avp_str_contains(help, AVP_S("cap text output at N lines (default 40; 0 or 'all': no cap)")));
}

AVP_TEST_MAIN("shsm_cli")
