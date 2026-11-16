/* project: tools/ninja_syntax.py in C (ninja_syntax.h) and the shell quoting of project.py. Expected values were
 * produced by CPython 3.14.3 (textwrap.wrap(t, 76, break_long_words=False, break_on_hyphens=False),
 * ninja_syntax.Writer.variable / build, subprocess.list2cmdline, ninja_syntax.escape_path) and are hard-coded. */
#include <string.h>

#include "avp/test.h"
#include "ninja_syntax.h"
#include "project_int.h"

static const struct {
    const char *text, *wrapped;
} k_wrap[] = {
    {"", ""},
    {"short", "short"},
    {"  leading spaces and  double  spaces here", "  leading spaces and  double  spaces here"},
    {"a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a ", "a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a\na a a a a a a a a a a a"},
    {"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx tail", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\ntail"},
    {"word\ttab\nnewline yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy", "word    tab newline\nyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"},
    {"This can be used as an implicit to a target that should always be rerun, ignoring file modified times.", "This can be used as an implicit to a target that should always be rerun,\nignoring file modified times."},
    {"h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo ", "h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo\nh\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo h\u00e9llo"},
};

static const struct {
    const char *key, *value;
    int indent;
    const char *out;
} k_lines[] = {
    {"command", "$python tools\\decompctx.py $in -o $out -d $out.d $includes $excludes $defines", 1, "  command = $python tools\\decompctx.py $in -o $out -d $out.d $includes $\n      $excludes $defines\n"},
    {"k", "a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b ", 0, "k = a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b $\n    a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b a$ b $\n    a$ b a$ b \n"},
    {"k", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 1, "  k = $\n      xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"},
    {"k", "aaa bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb c d", 0, "k = aaa $\n    bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb $\n    c d\n"},
    {"k", "$$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ ", 0, "k = $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $\n    $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ $$ $$$ \n"},
    {"k", "\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9 z z z z", 0, "k = \u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9\u00e9 $\n    z z z z\n"},
    {"c", "cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" ", 1, "  c = cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd $\n      /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" cmd /c \"x && y\" \n"},
};

static const struct {
    size_t n;
    const char *args[3];
    const char *out;
} k_cmd[] = {
    {1, {"a"}, "a"},
    {1, {"a b"}, "\"a b\""},
    {1, {""}, "\"\""},
    {1, {"x\"y"}, "x\\\"y"},
    {1, {"a\\"}, "a\\"},
    {1, {"a b\\"}, "\"a b\\\\\""},
    {1, {"a\\\"b"}, "a\\\\\\\"b"},
    {3, {"C:\\x y\\z", "--q", "tab\tx"}, "\"C:\\x y\\z\" --q \"tab\tx\""},
    {1, {"\\\\\""}, "\\\\\\\\\\\""},
};

static const struct {
    const char *in, *out;
} k_escape[] = {
    {"a b", "a$ b"},
    {"C:\\x", "C$:\\x"},
    {"$ y", "$$$ y"},
    {"a$ b:c", "a$$$ b$:c"},
};

AVP_TEST(textwrap) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof k_wrap / sizeof k_wrap[0]; i++) {
        AvpStrVec lines = ninja_textwrap(a, k_wrap[i].text, 76);
        CHECK_STR(avp_str_join(a, AVP_S("\n"), lines.v, lines.n), k_wrap[i].wrapped);
    }
}

AVP_TEST(variable_wrapping) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof k_lines / sizeof k_lines[0]; i++) {
        AvpBuf b;
        avp_buf_init(&b, a);
        NinjaWriter w;
        ninja_writer_init(&w, a, &b);
        ninja_variable(&w, k_lines[i].key, k_lines[i].value, k_lines[i].indent);
        CHECK_STR(avp_buf_str(&b), k_lines[i].out);
    }
}

AVP_TEST(list2cmdline) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof k_cmd / sizeof k_cmd[0]; i++)
        CHECK_STR(avp_str(proj_list2cmdline(a, k_cmd[i].n, k_cmd[i].args)), k_cmd[i].out);
    CHECK_STR(avp_str(proj_cmd_path(a, "C:\\a\\b")), "C:\\a\\b");
    CHECK_STR(avp_str(proj_cmd_path(a, "C:\\a b\\c")), "\"C:\\a b\\c\"");
    CHECK_STR(avp_str(proj_chain(a, "x && y")), "cmd /c \"x && y\"");
}

AVP_TEST(escape_path) {
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof k_escape / sizeof k_escape[0]; i++)
        CHECK_STR(avp_str(ninja_escape_path(a, k_escape[i].in)), k_escape[i].out);
    CHECK_STR(avp_str(ninja_escape(a, "a$b$$")), "a$$b$$$$");
}

AVP_TEST(build_line) {
    AvpArena *a = avp_arena();
    AvpBuf b;
    avp_buf_init(&b, a);
    NinjaWriter w;
    ninja_writer_init(&w, a, &b);
    NinjaVar vars[] = {{"v", "a b"}, {"n", NULL}};
    ninja_build(&w, &(NinjaBuild){.outputs = NINJA_LIST(a, "o 1", "o:2"), .rule = "r", .inputs = NINJA_LIST(a, "i", NULL, ""),
                                  .implicit = NINJA_LIST(a, NULL), .order_only = ninja_one(a, "oo"),
                                  .implicit_outputs = ninja_one(a, "io"), .vars = vars, .nvars = 2, .pool = "p", .dyndep = "dd"});
    ninja_build(&w, &(NinjaBuild){.outputs = ninja_list(a, 0, NULL), .rule = "phony"});
    CHECK_STR(avp_buf_str(&b), "build o$ 1 o$:2 | io: r i | || oo\n  pool = p\n  dyndep = dd\n  v = a b\n  n = \nbuild : phony\n");
}

AVP_TEST_MAIN("project")
