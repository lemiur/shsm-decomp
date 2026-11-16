/* changes_fmt: the pure pieces of the port against values tools/changes_fmt.py gives (format_float, the comparison of
 * JSON numbers, get_changes' order, the plain text and markdown tables, the exceptions of malformed documents). The
 * whole tool is checked by the differential harness (diff/cases.txt). */
#include "avp/test.h"
#include "changes_fmt.h"
#include <math.h>

static CfValue fl(double d) { return (CfValue){CF_FLOAT, 0, d, {"", 0}}; }
static CfValue in(i64 i) { return (CfValue){CF_INT, i, 0.0, {"", 0}}; }

static void check_float(CfValue v, const char *expect) { CHECK_STR(cf_format_float(avp_arena(), v), expect); }

AVP_TEST(format_float_clamps_just_below_100) {
    check_float(fl(100.0), "100.00");
    check_float(fl(99.995), " 99.99");
    check_float(fl(99.99), " 99.99");
    check_float(fl(99.9900001), " 99.99");
    check_float(fl(99.9999999), " 99.99");
    check_float(fl(99.98), " 99.98");
}

AVP_TEST(format_float_widths) {
    check_float(fl(0), "  0.00");
    check_float(fl(-5), " -5.00");
    check_float(fl(12345.678), "12345.68");
    check_float(in(7), "  7.00");
    check_float(in(-7), " -7.00");
    check_float(fl(1e20), "100000000000000000000.00");
    check_float(fl(NAN), "   nan");
    check_float(fl(INFINITY), "   inf");
}

AVP_TEST(format_float_rejects_a_string) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        cf_format_float(avp_arena(), (CfValue){CF_STR, 0, 0, AVP_S("x")});
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK(strstr(t.msg, "TypeError") != NULL);
    }
}

AVP_TEST(greater_compares_ints_and_floats_exactly) {
    CHECK(cf_greater(in(3), in(2)));
    CHECK(!cf_greater(in(2), in(2)));
    CHECK(cf_greater(fl(2.5), in(2)));
    CHECK(cf_greater(in(3), fl(2.5)));
    CHECK(!cf_greater(in(2), fl(2.0)));
    CHECK(!cf_greater(fl(2.0), in(2)));
    /* 2**53 + 1 is an int Python does not round to the float 2**53 */
    CHECK(cf_greater(in(9007199254740993LL), fl(9007199254740992.0)));
    CHECK(!cf_greater(fl(9007199254740992.0), in(9007199254740993LL)));
    CHECK(cf_greater(in(INT64_MAX), fl(9223372036854775807.0 - 1024.0)));
    CHECK(!cf_greater(in(INT64_MAX), fl(9223372036854775808.0)));
    CHECK(cf_greater(fl(9223372036854775808.0), in(INT64_MAX)));
    CHECK(cf_greater(in(-1), fl(-1.5)));
}

AVP_TEST(greater_nan_is_never_greater) {
    double nan = NAN;
    CHECK(!cf_greater(fl(nan), fl(1)));
    CHECK(!cf_greater(fl(1), fl(nan)));
    CHECK(!cf_greater(fl(nan), in(1)));
    CHECK(!cf_greater(in(1), fl(nan)));
    CHECK(cf_greater(fl(INFINITY), fl(1)));
}

AVP_TEST(greater_strings_by_code_point_and_type_errors) {
    CHECK(cf_greater((CfValue){CF_STR, 0, 0, AVP_S("b")}, (CfValue){CF_STR, 0, 0, AVP_S("a")}));
    CHECK(!cf_greater((CfValue){CF_STR, 0, 0, AVP_S("a")}, (CfValue){CF_STR, 0, 0, AVP_S("a")}));
    AvpTry t;
    if (AVP_TRY(&t)) {
        cf_greater((CfValue){CF_OTHER, 0, 0, AVP_S("NoneType")}, fl(1));
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_CZ(t.msg, "TypeError: '>' not supported between instances of 'NoneType' and 'float'");
    }
}

static const char *const DOC =
    "{\"from\":{\"fuzzy_match_percent\":10.0,\"matched_code_percent\":20},"
    "\"to\":{\"fuzzy_match_percent\":11.5,\"matched_code_percent\":19.995},"
    "\"units\":[{\"name\":\"main/a\",\"from\":{\"fuzzy_match_percent\":99.995,\"complete_data_percent\":50},"
    "\"to\":{\"fuzzy_match_percent\":100.0,\"complete_data_percent\":40},"
    "\"functions\":[{\"name\":\"fn_80001234\",\"from\":{\"fuzzy_match_percent\":100.0},\"to\":{\"fuzzy_match_percent\":99.999}},"
    "{\"name\":\"g\",\"from\":{\"fuzzy_match_percent\":1},\"to\":{\"fuzzy_match_percent\":2}},"
    "{\"name\":\"\",\"from\":{\"fuzzy_match_percent\":9},\"to\":{\"fuzzy_match_percent\":1}},"
    "{\"name\":null,\"from\":{\"fuzzy_match_percent\":9},\"to\":{\"fuzzy_match_percent\":1}}]}]}";

static AvpJson *parse(const char *text) {
    AvpJsonError err = {0};
    AvpJson *j = avp_json_parse(avp_arena(), avp_str(text), &err);
    AVP_ASSERT(j != NULL);
    return j;
}

AVP_TEST(get_changes_order_and_split) {
    CfChanges reg = {0}, prog = {0};
    cf_get_changes(parse(DOC), &reg, &prog);
    REQUIRE(reg.n == 5);
    REQUIRE(prog.n == 3);
    CHECK(!reg.v[0].has_name);
    CHECK_STR(reg.v[0].key, "matched_code");
    CHECK_STR(reg.v[1].name, "main/a");
    CHECK_STR(reg.v[1].key, "complete_data");
    CHECK_STR(reg.v[2].name, "fn_80001234");
    CHECK_STR(reg.v[3].name, "");
    CHECK(reg.v[3].has_name);
    CHECK(!reg.v[4].has_name);
    CHECK_STR(prog.v[0].key, "fuzzy_match");
    CHECK(!prog.v[0].has_name);
    CHECK_STR(prog.v[1].name, "main/a");
    CHECK_STR(prog.v[2].name, "g");
    avp_vec_free(&reg);
    avp_vec_free(&prog);
}

AVP_TEST(plaintext_table) {
    AvpArena *a = avp_arena();
    CfChanges reg = {0}, prog = {0};
    cf_get_changes(parse(DOC), &reg, &prog);
    CHECK_STR(cf_plaintext(a, reg.v, reg.n),
              "      Total | matched_code  |  20.00% ->  20.00%\n"
              "     main/a | complete_data |  50.00% ->  40.00%\n"
              "fn_80001234 | fuzzy_match   | 100.00% ->  99.99%\n"
              "            | fuzzy_match   |   9.00% ->   1.00%\n"
              "      Total | fuzzy_match   |   9.00% ->   1.00%");
    CfChanges both = {0};
    avp_vec_extend(&both, prog.v, prog.n);
    avp_vec_extend(&both, reg.v, reg.n);
    CHECK_STR(cf_plaintext(a, both.v, both.n),
              "      Total | fuzzy_match   |  10.00% ->  11.50%\n"
              "     main/a | fuzzy_match   |  99.99% -> 100.00%\n"
              "          g | fuzzy_match   |   1.00% ->   2.00%\n"
              "      Total | matched_code  |  20.00% ->  20.00%\n"
              "     main/a | complete_data |  50.00% ->  40.00%\n"
              "fn_80001234 | fuzzy_match   | 100.00% ->  99.99%\n"
              "            | fuzzy_match   |   9.00% ->   1.00%\n"
              "      Total | fuzzy_match   |   9.00% ->   1.00%");
    CHECK_STR(cf_plaintext(a, NULL, 0), "");
}

static CfChange chg(const char *name, const char *key, CfValue from, CfValue to) {
    return (CfChange){name != NULL, avp_str(name), NULL, avp_str(key), from, to};
}

AVP_TEST(plaintext_truncates_long_names_to_the_column) {
    /* the column is 136 - 3 - len("complete_data") - 3 - 18 = 99 wide; a longer name keeps 94 characters and "[...]" */
    char x[151];
    memset(x, 'x', 150);
    x[150] = 0;
    CfChange c[] = {chg(NULL, "matched_code", in(50), fl(49.5)), chg("short", "complete_data", fl(3.5), in(2)),
                    chg(x, "fuzzy_match", fl(1), fl(0))};
    c[0].has_name = false;
    char want[600];
    snprintf(want, sizeof want,
             "%94sTotal | matched_code  |  50.00%% ->  49.50%%\n"
             "%94sshort | complete_data |   3.50%% ->   2.00%%\n"
             "%.94s[...] | fuzzy_match   |   1.00%% ->   0.00%%",
             "", "", x);
    CHECK_STR(cf_plaintext(avp_arena(), c, 3), want);
}

AVP_TEST(plaintext_pads_by_code_points) {
    CfChange c[] = {chg("caf\xc3\xa9", "fuzzy_match", fl(2), fl(1)), chg("abcd", "fuzzy_match", fl(2), fl(1))};
    CHECK_STR(cf_plaintext(avp_arena(), c, 2),
              "caf\xc3\xa9 | fuzzy_match |   2.00% ->   1.00%\n"
              "abcd | fuzzy_match |   2.00% ->   1.00%");
}

AVP_TEST(markdown_tables) {
    AvpArena *a = avp_arena();
    CfChanges reg = {0}, prog = {0};
    cf_get_changes(parse(DOC), &reg, &prog);
    CHECK_STR(cf_markdown(a, reg.v, reg.n, "regressions"),
              "<details>\n<summary>Detected 5 regressions compared to the base:</summary>\n\n"
              "| Name | Type | Before | After |\n| ---- | ---- | ------ | ----- |\n"
              "| Total | Matched code |  20.00% |  20.00% |\n"
              "| `main/a` | Complete data |  50.00% |  40.00% |\n"
              "| `fn_80001234` | Fuzzy match | 100.00% |  99.99% |\n"
              "| `` | Fuzzy match |   9.00% |   1.00% |\n"
              "| Total | Fuzzy match |   9.00% |   1.00% |\n"
              "</details>");
    CHECK_STR(cf_markdown(a, prog.v, prog.n, "progressions"),
              "<details>\n<summary>Detected 3 progressions compared to the base:</summary>\n\n"
              "| Name | Type | Before | After |\n| ---- | ---- | ------ | ----- |\n"
              "| Total | Fuzzy match |  10.00% |  11.50% |\n"
              "| `main/a` | Fuzzy match |  99.99% | 100.00% |\n"
              "| `g` | Fuzzy match |   1.00% |   2.00% |\n"
              "</details>");
    CHECK_STR(cf_markdown(a, NULL, 0, "regressions"), "");
}

AVP_TEST(markdown_shortens_names_over_100) {
    char y[102], z[101];
    memset(y, 'y', 101);
    y[101] = 0;
    memset(z, 'z', 100);
    z[100] = 0;
    CfChange c[] = {chg(y, "matched_code", fl(1), fl(0)), chg(z, "fuzzy_match", in(2), in(1))};
    AvpStr md = cf_markdown(avp_arena(), c, 2, "regressions");
    char want[600];
    snprintf(want, sizeof want,
             "<details>\n<summary>Detected 2 regressions compared to the base:</summary>\n\n"
             "| Name | Type | Before | After |\n| ---- | ---- | ------ | ----- |\n"
             "| `%.97s...` | Matched code |   1.00%% |   0.00%% |\n"
             "| `%s` | Fuzzy match |   2.00%% |   1.00%% |\n</details>",
             y, z);
    CHECK_STR(md, want);
}

static void expect_exception(const char *doc, const char *prefix) {
    CfChanges reg = {0}, prog = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        cf_get_changes(parse(doc), &reg, &prog);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception for %s", doc);
    } else {
        CHECK_MSG(strncmp(t.msg, prefix, strlen(prefix)) == 0, "%s: got %s", doc, t.msg);
    }
}

AVP_TEST(malformed_documents_raise_what_python_raises) {
    expect_exception("[]", "AttributeError: 'list' object has no attribute 'get'");
    expect_exception("null", "AttributeError: 'NoneType'");
    expect_exception("{\"units\":[{}]}", "KeyError: 'name'");
    expect_exception("{\"units\":[[]]}", "TypeError: list indices");
    expect_exception("{\"units\":[null]}", "TypeError: 'NoneType' object is not subscriptable");
    expect_exception("{\"units\":null}", "TypeError: 'NoneType' object is not iterable");
    expect_exception("{\"units\":5}", "TypeError: 'int' object is not iterable");
    expect_exception("{\"units\":{\"a\":1}}", "TypeError");
    expect_exception("{\"from\":[]}", "AttributeError: 'list'");
    expect_exception("{\"from\":null}", "AttributeError: 'NoneType'");
    expect_exception("{\"from\":{\"fuzzy_match_percent\":null},\"to\":{}}", "TypeError: '>' not supported");
    expect_exception("{\"units\":[{\"name\":\"u\",\"functions\":null}]}", "TypeError: 'NoneType' object is not iterable");
    expect_exception("{\"units\":[{\"name\":\"u\",\"functions\":[{\"from\":{}}]}]}", "KeyError: 'name'");
}

AVP_TEST(empty_containers_iterate_to_nothing) {
    CfChanges reg = {0}, prog = {0};
    cf_get_changes(parse("{\"units\":{}}"), &reg, &prog);
    cf_get_changes(parse("{\"units\":\"\"}"), &reg, &prog);
    cf_get_changes(parse("{\"units\":[{\"name\":\"u\",\"functions\":{}}]}"), &reg, &prog);
    cf_get_changes(parse("{}"), &reg, &prog);
    CHECK_EQ_U64(reg.n, 0);
    CHECK_EQ_U64(prog.n, 0);
}

AVP_TEST(missing_values_default_to_zero_and_booleans_are_numbers) {
    CfChanges reg = {0}, prog = {0};
    cf_get_changes(parse("{\"units\":[{\"name\":\"u\",\"to\":{\"fuzzy_match_percent\":12.5}},"
                         "{\"name\":\"v\",\"from\":{\"fuzzy_match_percent\":true},\"to\":{\"fuzzy_match_percent\":0.5}}]}"),
                   &reg, &prog);
    REQUIRE(prog.n == 1);
    REQUIRE(reg.n == 1);
    CHECK_STR(prog.v[0].name, "u");
    CHECK(prog.v[0].from.kind == CF_FLOAT && prog.v[0].from.f == 0.0);
    CHECK_STR(reg.v[0].name, "v");
    CHECK(reg.v[0].from.kind == CF_INT && reg.v[0].from.i == 1);
}

AVP_TEST(non_string_names_fail_when_printed) {
    CfChanges reg = {0}, prog = {0};
    cf_get_changes(parse("{\"units\":[{\"name\":5,\"from\":{\"fuzzy_match_percent\":3},\"to\":{\"fuzzy_match_percent\":1}}]}"),
                   &reg, &prog);
    REQUIRE(reg.n == 1);
    CHECK_MSG(reg.v[0].bad_type != NULL, "the name 5 is kept as its type");
    AvpTry t;
    if (AVP_TRY(&t)) {
        cf_plaintext(avp_arena(), reg.v, reg.n);
        avp_try_end(&t);
        CHECK_MSG(false, "no exception");
    } else {
        CHECK_CZ(t.msg, "TypeError: object of type 'int' has no len()");
    }
}

AVP_TEST_MAIN("changes_fmt_format")
