/* decompctx: the pure pieces of the port against values tools/decompctx.py gives (the three line patterns, the
 * prelude, sanitize_path, process_file over text without includes to read). The tool as a whole (files read and
 * written, include search, depfile) is checked by the differential harness (diff/cases.txt). */
#include "avp/test.h"
#include "decompctx.h"

static AvpStr inc(const char *line, bool *ok) {
    AvpStr g = {0};
    *ok = decompctx_match_include(avp_str_strip(avp_str(line)), &g);
    return g;
}

AVP_TEST(include_pattern) {
    bool ok;
    CHECK_STR(inc("#include <a/b.h>", &ok), "a/b.h");
    CHECK(ok);
    CHECK_STR(inc("  #  include\t\"x.h\"  // c", &ok), "x.h");
    CHECK(ok);
    CHECK_STR(inc("#include<x.h>", &ok), "x.h");
    CHECK(ok);
    /* the group is lazy: it ends at the first > or " after its first character */
    CHECK_STR(inc("#include \"\"foo\"\"", &ok), "\"foo");
    CHECK(ok);
    CHECK_STR(inc("#include <a\"b>", &ok), "a");
    CHECK(ok);
    CHECK_STR(inc("#include <>>", &ok), ">");
    CHECK(ok);
    inc("#include <x.h", &ok);
    CHECK(!ok);
    inc("#include <", &ok);
    CHECK(!ok);
    inc("#include x.h", &ok);
    CHECK(!ok);
    inc("#includes <x.h>", &ok);
    CHECK(!ok);
    inc("# define include <x>", &ok);
    CHECK(!ok);
}

AVP_TEST(guard_and_once_patterns) {
    AvpStr g = {0};
    CHECK(decompctx_match_guard(avp_str("#ifndef FOO_H"), &g));
    CHECK_STR(g, "FOO_H");
    CHECK(decompctx_match_guard(avp_str("#  ifndef\tA B"), &g));
    CHECK_STR(g, "A B");
    CHECK(!decompctx_match_guard(avp_str("#ifndef"), &g));
    CHECK(!decompctx_match_guard(avp_str("#ifndefX"), &g));
    CHECK(!decompctx_match_guard(avp_str("#ifdef X"), &g));
    CHECK(decompctx_match_once(avp_str("#pragma once")));
    CHECK(decompctx_match_once(avp_str("#  pragma \t once")));
    CHECK(!decompctx_match_once(avp_str("#pragma once // c")));
    CHECK(!decompctx_match_once(avp_str("#pragmaonce")));
    CHECK(!decompctx_match_once(avp_str("#pragma once2")));
}

AVP_TEST(prelude) {
    AvpArena *a = avp_arena();
    AvpStrVec none = {0};
    CHECK_STR(decompctx_prelude(a, &none), "");
    AvpStrVec d = {0};
    avp_vec_push(&d, avp_str("A"));
    avp_vec_push(&d, avp_str("B=1"));
    avp_vec_push(&d, avp_str("C="));
    avp_vec_push(&d, avp_str("D=x=y"));
    CHECK_STR(decompctx_prelude(a, &d),
              "/* decompctx prelude */\n#define A\n#define B 1\n#define C \n#define D x=y\n"
              "/* end decompctx prelude */\n\n");
}

AVP_TEST(sanitize_path) {
    AvpArena *a = avp_arena();
    CHECK_STR(decompctx_sanitize_path(a, avp_str("a\\b c\\d.h")), "a/b\\ c/d.h");
    CHECK_STR(decompctx_sanitize_path(a, avp_str("")), "");
}

AVP_TEST(process_text_guards_and_excludes) {
    AvpArena *a = avp_arena();
    DecompctxState st;
    AvpBuf out;
    avp_buf_init(&out, a);
    decompctx_init(&st, a, "X:\\root");
    avp_vec_push(&st.exclude_globs, avp_str("*.inc"));
    CHECK(decompctx_process_text(&st, "src\\a.h", avp_str("#ifndef A_H\nint x;\n#include \"b.inc\"\nlast"), &out));
    CHECK_STR(avp_buf_str(&out),
              "#ifndef A_H\nint x;\n/* \"src\\a.h\" line 2 \"b.inc\" */\n/* Skipped excluded file */\n"
              "/* end \"b.inc\" */\nlast");
    /* the same guard again: the file is dropped */
    size_t n = avp_buf_str(&out).n;
    CHECK(decompctx_process_text(&st, "src\\c.h", avp_str("#ifndef A_H\nint y;\n"), &out));
    CHECK(avp_buf_str(&out).n == n);
    /* #pragma once keys on the file name */
    CHECK(decompctx_process_text(&st, "src\\p.h", avp_str("#pragma once\nq\n"), &out));
    CHECK_STR(avp_strn(avp_buf_str(&out).p + n, avp_buf_str(&out).n - n), "#pragma once\nq\n");
    n = avp_buf_str(&out).n;
    CHECK(decompctx_process_text(&st, "src\\p.h", avp_str("#pragma once\nq\n"), &out));
    CHECK(avp_buf_str(&out).n == n);
}

AVP_TEST_MAIN("decompctx")
