/* check_rules: the lexer (rules_lex.h): tokens, comments, directives, `#if 0` arms, literals, line numbers. */
#include "avp/test.h"
#include "rules_lex.h"

static RlLexed lx(const char *src) { return rl_lex(avp_arena(), avp_str(src), 1, true); }

static void check_tok(const RlTok *t, RlKind k, const char *text, i32 line) {
    CHECK_MSG(t->k == k, "%s: kind %s", text, rl_kind_names[t->k]);
    CHECK_STR(t->t, text);
    CHECK_EQ_I64(t->line, line);
}

AVP_TEST(tokens_and_kinds) {
    RlLexed r = lx("int x_1 = 0x1Fu + 1.5e+3f - .5 >>= a->b; s$ ...");
    REQUIRE(r.ok);
    static const struct {
        RlKind k;
        const char *t;
    } want[] = {{RL_ID, "int"}, {RL_ID, "x_1"}, {RL_OP, "="},   {RL_NUM, "0x1Fu"}, {RL_OP, "+"},  {RL_NUM, "1.5e+3f"},
                {RL_OP, "-"},   {RL_NUM, ".5"}, {RL_OP, ">>="}, {RL_ID, "a"},      {RL_OP, "->"}, {RL_ID, "b"},
                {RL_OP, ";"},   {RL_ID, "s$"},  {RL_OP, "..."}};
    REQUIRE(r.toks.n == sizeof want / sizeof *want);
    for (size_t i = 0; i < r.toks.n; i++) check_tok(&r.toks.v[i], want[i].k, want[i].t, 1);
}

AVP_TEST(number_edges) {
    RlLexed r = lx("1e 0x 3..4");
    REQUIRE(r.ok);
    REQUIRE(r.toks.n == 6);
    check_tok(&r.toks.v[0], RL_NUM, "1", 1); /* the exponent needs digits */
    check_tok(&r.toks.v[1], RL_ID, "e", 1);
    check_tok(&r.toks.v[2], RL_NUM, "0", 1); /* 0x without digits */
    check_tok(&r.toks.v[3], RL_ID, "x", 1);
    check_tok(&r.toks.v[4], RL_NUM, "3.", 1);
    check_tok(&r.toks.v[5], RL_NUM, ".4", 1);
}

AVP_TEST(strings_and_chars) {
    RlLexed r = lx("\"a\\\"b\" '\\'' L\"w\"\n\"open\nx");
    REQUIRE(r.ok);
    REQUIRE(r.toks.n == 6);
    check_tok(&r.toks.v[0], RL_STR, "\"a\\\"b\"", 1);
    check_tok(&r.toks.v[1], RL_CHR, "'\\''", 1);
    check_tok(&r.toks.v[2], RL_ID, "L", 1);
    check_tok(&r.toks.v[3], RL_STR, "\"w\"", 1);
    /* an unterminated literal keeps its newline and the line count lags */
    check_tok(&r.toks.v[4], RL_STR, "\"open\n", 2);
    check_tok(&r.toks.v[5], RL_ID, "x", 2);
}

AVP_TEST(comments) {
    RlLexed r = lx("// lead\nint a; // tail\r\n/* b\nc */ int b;\n/* open");
    REQUIRE(r.ok);
    REQUIRE(r.comments.n == 4);
    CHECK_STR(r.comments.v[0].text, " lead");
    CHECK(!r.comments.v[0].trailing);
    CHECK_STR(r.comments.v[1].text, " tail");
    CHECK(r.comments.v[1].trailing);
    CHECK_EQ_I64(r.comments.v[2].line, 3);
    CHECK_EQ_I64(r.comments.v[2].end_line, 4);
    CHECK_STR(r.comments.v[2].text, " b\nc ");
    CHECK_STR(r.comments.v[3].text, " open");
    check_tok(&r.toks.v[r.toks.n - 2], RL_ID, "b", 4);
}

AVP_TEST(directives) {
    RlLexed r = lx("#  define X(a) \\\n  ((a) + 1) // c\nint y;\n#include <a/b.h>\n x # y\n#\n");
    REQUIRE(r.ok);
    REQUIRE(r.directives.n == 3);
    const RlDirective *d = &r.directives.v[0];
    CHECK_STR(d->name, "define");
    CHECK_STR(d->rest, "X(a)    ((a) + 1)");
    CHECK_EQ_I64(d->line, 1);
    CHECK_EQ_I64(d->end_line, 2);
    REQUIRE(d->toks.n > 0);
    CHECK_EQ_I64(d->toks.v[d->toks.n - 1].line, 1);
    REQUIRE(r.comments.n == 1);
    CHECK(r.comments.v[0].trailing);
    CHECK_STR(r.directives.v[1].rest, "<a/b.h>");
    CHECK_STR(r.directives.v[2].name, "");
    /* a # that does not open its line is an op token */
    bool hash_tok = false;
    for (size_t i = 0; i < r.toks.n; i++) hash_tok |= RL_TEQ(&r.toks.v[i], "#");
    CHECK(hash_tok);
}

AVP_TEST(if_zero_arms) {
    RlLexed r = lx("#if 0\nint dead; // gone\n#if X\n#else\n#endif\n#elif 1 // kept\nint live;\n#endif\n#if (0L)\nint d2;\n");
    REQUIRE(r.ok);
    REQUIRE(r.toks.n == 3);
    check_tok(&r.toks.v[1], RL_ID, "live", 7);
    /* #if 0, #elif and #endif are recorded; nothing inside the arm, and an unclosed arm is cut */
    REQUIRE(r.directives.n == 4);
    CHECK_STR(r.directives.v[0].name, "if");
    CHECK_STR(r.directives.v[1].name, "elif");
    CHECK_STR(r.directives.v[2].name, "endif");
    CHECK_STR(r.directives.v[3].rest, "(0L)");
    REQUIRE(r.comments.n == 1);
    CHECK_STR(r.comments.v[0].text, " kept");
}

AVP_TEST(continuation_and_lines) {
    RlLexed r = lx("a \\\nb\n\n  c");
    REQUIRE(r.ok);
    REQUIRE(r.toks.n == 3);
    check_tok(&r.toks.v[1], RL_ID, "b", 2);
    check_tok(&r.toks.v[2], RL_ID, "c", 4);
    RlLexed s = rl_lex(avp_arena(), avp_str("x"), 7, false);
    check_tok(&s.toks.v[0], RL_ID, "x", 7);
}

AVP_TEST(unicode_and_whitespace_error) {
    RlLexed r = lx("int caf\xc3\xa9 = \xd9\xa1\xd9\xa2;");
    REQUIRE(r.ok);
    check_tok(&r.toks.v[2], RL_OP, "\xc3\xa9", 1);
    check_tok(&r.toks.v[4], RL_NUM, "\xd9\xa1\xd9\xa2", 1); /* re's \d is Unicode */
    RlLexed bad = lx("int\n\xc2\xa0x;");                       /* U+00A0: _OP.match None in the Python */
    CHECK(!bad.ok);
    CHECK_EQ_I64(bad.error_line, 2);
    CHECK(lx("// \xc2\xa0 in a comment\n").ok);
}

AVP_TEST(pair_brackets_and_ptext) {
    RlLexed r = lx("( [ { ) } ] ( \"(\"");
    i32 *m = rl_pair_brackets(avp_arena(), r.toks.v, r.toks.n);
    /* ')' pops to '(' dropping '[' and '{'; the later '}' and ']' find nothing */
    CHECK_EQ_I64(m[0], 3);
    CHECK_EQ_I64(m[3], 0);
    CHECK_EQ_I64(m[1], -1);
    CHECK_EQ_I64(m[2], -1);
    CHECK_EQ_I64(m[4], -1);
    CHECK_EQ_I64(m[6], -1);
    CHECK_STR(rl_ptext(avp_arena(), r.toks.v, 3), "( [ {");
    CHECK_STR(rl_ptext(avp_arena(), r.toks.v, 0), "");
}

AVP_TEST(skip_quoted) {
    CHECK_EQ_I64((i64)rl_skip_quoted(avp_str("'a\\'b' x"), 0, '\''), 6);
    CHECK_EQ_I64((i64)rl_skip_quoted(avp_str("\"ab"), 0, '"'), 3);
    CHECK_EQ_I64((i64)rl_skip_quoted(avp_str("\"a\nb\""), 0, '"'), 3);
}

AVP_TEST_MAIN("check_rules")
