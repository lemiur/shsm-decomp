/* Unit tests of the levers port (ported from tools/test_levers.py).
 *
 * Not ported: FakeFilterTests.test_search_filters_fakes_before_compiling (it monkeypatches FnContext.generate; the C
 * generator has no seam, and the differential check diff/gen_check.py compares fake_reasons per candidate over the
 * real tree instead); DiffTests' `git apply --check` (apply_patch round trips every candidate's diff here, and the
 * harness applies one with --apply); RealTreeTests.test_real_sources_parse (diff/gen_check.py parses every function
 * of the real tree on both sides). */
#include <string.h>

#include "avp/avp.h"
#include "avp/test.h"
#include "levers_search.h"

static const char C_SRC[] =
    "typedef int s32;\ntypedef unsigned long u32;\ntypedef float f32;\nstruct S { int a; int b; };\n\n/* banner */\n"
    "void f(struct S* p, int n) {\n    int i;\n    int j = 0;\n    int k, m;\n    int t = n + 1;\n"
    "    for (i = 0; i < n; i++) {\n        j = j + p->a;\n    }\n    k = 3;\n    m = 4;\n    if (n == 0) {\n"
    "        p->a = 1;\n    } else {\n        p->b = 2;\n    }\n    if (p->a) {\n        if (n > 2) {\n"
    "            p->b = t;\n        }\n    }\n    p->a = i + n + k;\n    j = (n == 1) ? 3 : 4;\n}\n\n"
    "int g(int x) {\n    if (x < 0) {\n        return 1;\n    }\n    return 2;\n}\n\n"
    "void h(int* q) {\n    if (q == 0) {\n        return;\n    }\n    q[0] = 1;\n    q[1] = 2;\n}\n\n"
    "int u(int a, int b) {\n    int c = a * 2;\n    int d;\n    d = b + 1;\n    a += b;\n    b--;\n"
    "    if (!(a == b)) {\n        d = d | c;\n    }\n    return d;\n}\n\n"
    "void w(int* q, int n) {\n    int i = 0;\n    while (i < n) {\n        q[i] = 0;\n        i++;\n    }\n"
    "    do {\n        n--;\n    } while (n > 0);\n}\n\n"
    "void mix(int a) {\n    a = 1;\n    int b = a;\n    g(b);\n}\n";
static const char MIX[] = "\nvoid mix(int a) {\n    a = 1;\n    int b = a;\n    g(b);\n}\n";
static const char CPP_SRC[] =
    "class Foo {\npublic:\n    int Get() const { return m_value + 1; }\n    void Bar(float x);\n"
    "    void Bar(float x, int y);\n    int m_value;\n    float m_scale;\n};\n\n"
    "void Foo::Bar(float x) {\n    float s = x * m_scale;\n    m_scale = s + x;\n}\n\n"
    "void Foo::Bar(float x, int y) {\n    m_value = y;\n}\n";

static LvS S(const char *z) { return (LvS){z, strlen(z)}; }
static const char *Z(LvS s) { return avp_cz(avp_arena(), s); }

/* every generated text whose lever starts with prefix (and that contains must) */
static AvpCzVec texts(const char *src, const char *sym, const char *prefix, const char *must) {
    AvpArena *a = avp_arena();
    LvCtx *x = lv_ctx_new(a, S(src), sym);
    LvCandidates c = lv_generate(x, NULL, 0);
    AvpCzVec out = {0};
    for (size_t i = 0; i < c.n; i++) {
        if (strncmp(c.v[i].lever, prefix, strlen(prefix))) continue;
        char *t = avp_cz(a, c.v[i].text);
        if (must && !strstr(t, must)) continue;
        avp_vec_push(&out, t);
    }
    return out;
}

static const char *one(const char *src, const char *sym, const char *prefix, const char *must) {
    AvpCzVec v = texts(src, sym, prefix, must);
    return v.n ? v.v[0] : NULL;
}

/* fwd applies (the result parses) and back on a result restores src exactly */
static bool roundtrip(const char *src, const char *sym, const char *fwd, const char *back, const char *must) {
    AvpCzVec v = texts(src, sym, fwd, must);
    bool found = false;
    for (size_t i = 0; i < v.n; i++) {
        if (!lv_check_parses(avp_arena(), S(v.v[i]), sym)) return false;
        AvpCzVec b = texts(v.v[i], sym, back, NULL);
        for (size_t k = 0; k < b.n; k++)
            if (!strcmp(b.v[k], src)) found = true;
    }
    return found;
}

static bool has(const char *t, const char *needle) { return t && strstr(t, needle); }

static const char *with_mix(const char *src) { return avp_sprintf(avp_arena(), "%s%s", src, MIX); }

static const char *replace(const char *src, const char *old, const char *nw) {
    AvpStr r = avp_str_replace(avp_arena(), S(src), S(old), S(nw), -1);
    return Z(r);
}

/* ---- tokens and definitions ---- */

AVP_TEST(tokens_offsets_and_kinds) {
    const char *src = "a = b->c; // x\n#define Q 1\n\"s\\\"t\" 1.5f";
    LvToks t = lv_tokenize(avp_arena(), S(src));
    static const char *const kinds[] = {"id", "op", "id", "op", "id", "op", "comment", "pp", "str", "num"};
    REQUIRE(t.n == 10);
    for (i32 i = 0; i < t.n; i++) {
        CHECK(strlen(t.v[i].t) == (size_t)(t.v[i].e - t.v[i].s) && !memcmp(src + t.v[i].s, t.v[i].t, t.v[i].n));
        CHECK_CZ(lv_kind_name(t.v[i].k), kinds[i]);
    }
}

static int ctx_error(const char *src, const char *sym) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        lv_ctx_new(avp_arena(), S(src), sym);
        avp_try_end(&t);
        return 0;
    }
    return t.code;
}

AVP_TEST(find_definitions) {
    const char *syms[] = {"f", "g", "h"};
    for (int i = 0; i < 3; i++) CHECK_CZ(lv_ctx_new(avp_arena(), S(C_SRC), syms[i])->fn.name, syms[i]);
    LvCtx *x = lv_ctx_new(avp_arena(), S(CPP_SRC), "Bar__3FooFf");
    CHECK_CZ(x->fn.cls, "Foo");
    CHECK(x->fn.nparams == 1);
    CHECK(lv_ctx_new(avp_arena(), S(CPP_SRC), "Bar__3FooFfi")->fn.nparams == 2);
    CHECK_CZ(lv_ctx_new(avp_arena(), S(CPP_SRC), "Get__3FooCFv")->fn.cls, "Foo");
    CHECK(ctx_error(CPP_SRC, "Bar__3BazFf") == LV_EXC_PARSE);
    CHECK(lv_ctx_new(avp_arena(), S("METHODDEF(void *)\nalloc_small (int a, int b)\n{\n  return 0;\n}\n"), "alloc_small")
              ->fn.nparams == 2);
    CHECK_CZ(lv_ctx_new(avp_arena(), S("class A { A(int x); int m; };\nA::A(int x) : m(x) {\n    m = 1;\n}\n"),
                         "__ct__1AFi")->fn.name, "A");
}

AVP_TEST(preprocessor_in_body_refused) {
    CHECK(ctx_error("void f(void) {\n#if 1\n    g();\n#endif\n}\n", "f") == LV_EXC_PARSE);
    CHECK(ctx_error("void f(void) {\n#line 40\n    g();\n}\n", "f") == 0);
}

AVP_TEST(extern_c_function_is_found) {
    const char *src = "extern \"C\" void fn_801D197C() {\n    int a = 1;\n    g(a);\n}\n"
                      "extern \"C\" {\nvoid inside(int q) {\n    g(q);\n}\n}\n";
    CHECK_CZ(lv_ctx_new(avp_arena(), S(src), "fn_801D197C")->fn.name, "fn_801D197C");
    CHECK_CZ(lv_ctx_new(avp_arena(), S(src), "inside")->fn.name, "inside");
}

AVP_TEST(definition_found_by_address_comment) {
    /* fn_<address> names a definition by the project's annotation: the header comment ends with its address */
    const char *src = "// 0x801D197C\nvoid Renamed(int a) {\n    g(a);\n}\n\n"
                      "void Other(int a) {\n    g(a);\n} // 0x801D2000\n";
    CHECK_CZ(lv_ctx_new(avp_arena(), S(src), "fn_801D197C")->fn.name, "Renamed");
    CHECK_CZ(lv_ctx_new(avp_arena(), S(src), "fn_801D2000__Fi")->fn.name, "Other");
    /* any other symbol takes the address the CLI read from symbols.txt */
    lv_set_symbol_address("WhateverSymbol", 0x801D197C);
    CHECK_CZ(lv_ctx_new(avp_arena(), S(src), "WhateverSymbol")->fn.name, "Renamed");
    lv_set_symbol_address(NULL, 0);
    /* the error names the lookups tried */
    AvpTry t;
    if (AVP_TRY(&t)) {
        lv_ctx_new(avp_arena(), S(src), "fn_801D9999");
        avp_try_end(&t);
        CHECK(!"no error");
    } else {
        CHECK(strstr(t.msg, "no definition of fn_801D9999") && strstr(t.msg, "0x801D9999"));
    }
    if (AVP_TRY(&t)) {
        lv_ctx_new(avp_arena(), S(src), "__sinit_\\x_cpp");
        avp_try_end(&t);
        CHECK(!"no error");
    } else {
        CHECK(strstr(t.msg, "static initialiser") && strstr(t.msg, "no `// 0xADDRESS` lookup"));
    }
}

/* a #line before a statement tags it: every variant keeps the directive text, order and its statement */
static const char LINE_SRC[] =
    "void k(int* q, int a, int c) {\n    int b;\n    a = 1;\n#line 40\n    c = 6;\n    b = a + 2;\n    g(b);\n"
    "#line 50\n    q[0] = a + c;\n    q[1] = 2;\n}\n";

AVP_TEST(line_directives_stay_with_their_statement) {
    AvpArena *a = avp_arena();
    LvCandidates c = lv_generate(lv_ctx_new(a, S(LINE_SRC), "k"), NULL, 0);
    REQUIRE(c.n > 0);
    for (size_t i = 0; i < c.n; i++) {
        const char *t = avp_cz(a, c.v[i].text);
        const char *d40 = strstr(t, "#line 40"), *d50 = strstr(t, "#line 50");
        REQUIRE(d40 && d50 && d40 < d50);
        /* the statement after each directive keeps its identifiers, however its spelling changed */
        const char *e40 = strchr(d40, ';'), *e50 = strchr(d50, ';');
        char h40[64], h50[64];
        snprintf(h40, sizeof h40, "%.*s", (int)(e40 - d40), d40);
        snprintf(h50, sizeof h50, "%.*s", (int)(e50 - d50), d50);
        CHECK_MSG(strstr(h40, "c") && strstr(h40, "6"), "%s: %s", c.v[i].lever, h40);
        CHECK_MSG(strstr(h50, "q") && strstr(h50, "[0]"), "%s: %s", c.v[i].lever, h50);
        CHECK(lv_check_parses(a, c.v[i].text, "k"));
    }
}

AVP_TEST(line_directive_moves_with_its_statement) {
    /* swapping two independent statements carries the directive of the one that moves */
    const char *src = "void k(int a, int b) {\n    a = 1;\n#line 40\n    b = 2;\n}\n";
    AvpCzVec v = texts(src, "k", "swap independent", NULL);
    REQUIRE(v.n == 1);
    CHECK(has(v.v[0], "#line 40\n    b = 2;\n    a = 1;"));
}

AVP_TEST(line_directive_never_tags_another_statement) {
    /* a declaration moved to just before its first use goes above that use's directive, not between them */
    const char *src = with_mix("void k(int a) {\n    int b;\n    a = 1;\n#line 40\n    b = a;\n    g(b);\n}\n");
    AvpCzVec v = texts(src, "k", "move declaration", NULL);
    REQUIRE(v.n > 0);
    for (size_t i = 0; i < v.n; i++) CHECK_MSG(has(v.v[i], "#line 40\n    b = a"), "%s", v.v[i]);
}

/* ---- decl ---- */

AVP_TEST(swap_declarations) {
    const char *t = one(C_SRC, "f", "swap declarations", "    int j = 0;\n    int i;\n");
    REQUIRE(t);
    CHECK(lv_check_parses(avp_arena(), S(t), "f"));
    CHECK(roundtrip(C_SRC, "f", "swap declarations", "swap declarations", NULL));
}

AVP_TEST(move_to_first_use_and_top) {
    const char *t = one(C_SRC, "f", "move declaration to first use", "int t = n + 1;\n    int i;\n    for");
    REQUIRE(t);
    CHECK(lv_check_parses(avp_arena(), S(t), "f"));
    const char *src = with_mix("void v(int a) {\n    int b;\n    a = 1;\n    b = a;\n    g(b);\n}\n");
    t = one(src, "v", "move declaration to first use", NULL);
    CHECK(has(t, "    a = 1;\n    int b;\n    b = a;"));
    AvpCzVec back = texts(t, "v", "move declaration to top of block", NULL);
    bool found = false;
    for (size_t i = 0; i < back.n; i++) found |= !strcmp(back.v[i], src);
    CHECK(found);
    CHECK(texts("void v(int a) {\n    int b = a;\n    a = 1;\n    g(b);\n}\n", "v", "move declaration", NULL).n == 0);
}

AVP_TEST(split_and_merge_init) {
    const char *t = one(C_SRC, "f", "split initialisation", "int j;\n    j = 0;");
    REQUIRE(t);
    CHECK(lv_check_parses(avp_arena(), S(t), "f"));
    CHECK(roundtrip(C_SRC, "f", "split initialisation", "merge initialisation", "int j;\n    j = 0;"));
    CHECK(has(one(C_SRC, "u", "merge initialisation", NULL), "int d = b + 1;"));
}

AVP_TEST(c89_files_keep_declarations_first) {
    const char *src = "void v(int a) {\n    int c = 2;\n    int b;\n    a = 1;\n    b = a;\n    g(b, c);\n}\n";
    CHECK(!lv_mixed_ok(lv_ctx_new(avp_arena(), S(src), "v")));
    CHECK(texts(src, "v", "move declaration to first use", NULL).n == 0);
    CHECK(texts(src, "v", "split initialisation", NULL).n == 0);
    CHECK(texts(with_mix(src), "v", "move declaration to first use", NULL).n > 0);
    CHECK(lv_mixed_ok(lv_ctx_new(avp_arena(), S(avp_sprintf(avp_arena(), "class A {};\n%s", src)), "v")));
}

AVP_TEST(split_and_merge_declarators) {
    const char *t = one(C_SRC, "f", "split declarators", "int k;\n    int m;");
    REQUIRE(t);
    CHECK(roundtrip(C_SRC, "f", "split declarators", "merge declarators", NULL));
    CHECK(lv_check_parses(avp_arena(), S(t), "f"));
}

/* ---- expr ---- */

AVP_TEST(commute) {
    CHECK(has(one(C_SRC, "f", "commute +", "p->a + j"), "j = p->a + j;"));
    CHECK(has(one(C_SRC, "f", "commute ==", "0 == n"), "if (0 == n)"));
    CHECK(roundtrip(C_SRC, "f", "commute ==", "commute ==", "0 == n"));
    CHECK(has(one(C_SRC, "f", "commute +", "k + (i + n)"), "p->a = k + (i + n);"));
}

AVP_TEST(mirror) {
    CHECK(has(one(C_SRC, "f", "mirror <", NULL), "n > i"));
    CHECK(roundtrip(C_SRC, "f", "mirror <", "mirror >", "n > i"));
}

AVP_TEST(compound) {
    CHECK(has(one(C_SRC, "f", "x = x + n", NULL), "j += p->a;"));
    CHECK(roundtrip(C_SRC, "f", "x = x + n", "+= to x = x + n", NULL));
    CHECK(has(one(C_SRC, "u", "+= to", NULL), "a = a + b;"));
}

AVP_TEST(incdec) {
    CHECK(has(one(C_SRC, "f", "x++", NULL), "++i)"));
    CHECK(roundtrip(C_SRC, "f", "x++", "x++", NULL));
    CHECK(has(one(C_SRC, "u", "x--", NULL), "    --b;"));
}

AVP_TEST(not_equal) {
    CHECK(has(one(C_SRC, "u", "!(a == b) to", NULL), "if (a != b)"));
    CHECK(roundtrip(C_SRC, "u", "!(a == b) to", "a != b to !(a == b)", NULL));
}

AVP_TEST(reassociate) {
    CHECK(has(one(C_SRC, "f", "a + b + c to", NULL), "i + (n + k)"));
    CHECK(roundtrip(C_SRC, "f", "a + b + c to", "a + (b + c) to", NULL));
    CHECK(texts(CPP_SRC, "Bar__3FooFf", "a + b + c", NULL).n == 0);
}

/* ---- control ---- */

AVP_TEST(ternary) {
    const char *t = one(C_SRC, "f", "?: to if/else assignment", NULL);
    CHECK(has(t, "if (n == 1) {\n        j = 3;\n    } else {\n        j = 4;\n    }"));
    CHECK(has(one(t, "f", "if/else assignment to ?:", NULL), "j = n == 1 ? 3 : 4;"));
    CHECK(has(one(C_SRC, "g", "if and return to ?:", NULL), "return x < 0 ? 1 : 2;"));
    CHECK(roundtrip(C_SRC, "g", "if and return to ?:", "?: to if and return", NULL));
}

AVP_TEST(invert_if) {
    CHECK(has(one(C_SRC, "f", "invert if/else", NULL), "if (n != 0) {\n        p->b = 2;\n    } else {\n        p->a = 1;\n    }"));
    CHECK(roundtrip(C_SRC, "f", "invert if/else", "invert if/else", NULL));
}

AVP_TEST(early_return) {
    CHECK(has(one(C_SRC, "h", "early return to wrapped rest", NULL), "if (q != 0) {\n        q[0] = 1;\n        q[1] = 2;\n    }\n}"));
    CHECK(roundtrip(C_SRC, "h", "early return to wrapped rest", "wrapped tail to early return", NULL));
}

AVP_TEST(else_after_return) {
    CHECK(has(one(C_SRC, "g", "rest after return into else", NULL), "} else {\n        return 2;\n    }"));
    CHECK(roundtrip(C_SRC, "g", "rest after return into else", "else after return to rest", NULL));
}

AVP_TEST(void_tail) {
    const char *src = "void d(int t) {\n    if (t == 16) {\n        a();\n        return;\n    }\n    b();\n}\n";
    CHECK(has(one(src, "d", "return and rest to else", NULL),
              "    if (t == 16) {\n        a();\n    } else {\n        b();\n    }\n}"));
    CHECK(roundtrip(src, "d", "return and rest to else", "else to return and rest", NULL));
    const char *ni = replace(replace(src, "void d", "int d"), "b();", "return b();");
    CHECK(texts(ni, "d", "return and rest to else", NULL).n == 0);
}

AVP_TEST(for_while) {
    CHECK(has(one(C_SRC, "f", "for to while", NULL), "i = 0;\n    while (i < n) {\n        j = j + p->a;\n        i++;\n    }"));
    CHECK(roundtrip(C_SRC, "f", "for to while", "while to for", NULL));
    CHECK(has(one(C_SRC, "w", "while to for", NULL), "for (; i < n; i++) {"));
}

AVP_TEST(while_do) {
    CHECK(has(one(C_SRC, "w", "while to if/do-while", NULL), "if (i < n) {\n        do {\n            q[i] = 0;"));
    CHECK(roundtrip(C_SRC, "w", "while to if/do-while", "if/do-while to while", NULL));
    CHECK(texts(replace(C_SRC, "    do {\n        n--;\n    } while (n > 0);\n", ""), "w", "while to if/do-while", NULL)
              .n == 0);
}

AVP_TEST(if_merge_split) {
    CHECK(has(one(C_SRC, "f", "merge nested ifs", NULL), "if (p->a && n > 2) {\n        p->b = t;\n    }"));
    CHECK(roundtrip(C_SRC, "f", "merge nested ifs", "split && into nested ifs", NULL));
}

/* ---- temp, type, order ---- */

AVP_TEST(inline_single_use_local) {
    const char *t = one("int v(int a) {\n    int c = a * 2;\n    int d = 1;\n    return d + c;\n}\n", "v",
                        "inline single-use local", "d + (a * 2)");
    REQUIRE(t);
    CHECK(lv_check_parses(avp_arena(), S(t), "v"));
    CHECK(texts("int v(int a) {\n    int c = a;\n    g();\n    return c;\n}\n", "v", "inline", NULL).n == 0);
    CHECK(texts("int v(int a) {\n    int c = a;\n    a = 3;\n    return c;\n}\n", "v", "inline", NULL).n == 0);
}

AVP_TEST(introduce_temp) {
    const char *src = with_mix("struct T { int* r; };\nint v(struct T* p) {\n    int* r0 = p->r;\n    int z;\n    z = 0;\n"
                               "    p->r[0] = 1;\n    p->r[1] = 2;\n    return z;\n}\n");
    const char *t = one(src, "v", "introduce local", NULL);
    CHECK(has(t, "    z = 0;\n    int* r = p->r;\n    r[0] = 1;\n    r[1] = 2;"));
    CHECK(t && lv_check_parses(avp_arena(), S(t), "v"));
    CHECK(texts(replace(src, "    p->r[1] = 2;", "    g();\n    p->r[1] = 2;"), "v", "introduce", NULL).n == 0);
}

AVP_TEST(retype) {
    CHECK(has(one(C_SRC, "f", "local int to unsigned int", NULL), "    unsigned int i;"));
    CHECK(has(one(C_SRC, "f", "local int spelled s32", NULL), "    s32 i;"));
    CHECK(roundtrip(C_SRC, "f", "local int to unsigned int", "local unsigned int to int", NULL));
    CHECK(has(one(replace(CPP_SRC, "class Foo", "typedef float f32;\nclass Foo"), "Bar__3FooFf", "local float spelled f32",
                  NULL),
              "f32 s = x * m_scale;"));
}

AVP_TEST(swap_statements) {
    const char *t = one(C_SRC, "f", "swap independent statements", "    m = 4;\n    k = 3;\n");
    REQUIRE(t);
    CHECK(roundtrip(C_SRC, "f", "swap independent statements", "swap independent statements", "m = 4;\n    k = 3;"));
    CHECK(lv_check_parses(avp_arena(), S(t), "f"));
    const char *src = "void z(int* q, int a) {\n    int b;\n    a = 1;\n    b = a;\n    q[0] = 1;\n    q[1] = 2;\n}\n";
    AvpCzVec got = texts(src, "z", "swap independent", NULL);
    REQUIRE(got.n == 1);
    CHECK(has(got.v[0], "q[0] = 1;\n    b = a;"));
}

AVP_TEST(cpp_member) {
    AvpArena *a = avp_arena();
    LvCandidates c = lv_generate(lv_ctx_new(a, S(CPP_SRC), "Bar__3FooFf"), NULL, 0);
    bool commute = false, inl = false;
    size_t n = strlen(CPP_SRC);
    for (size_t i = 0; i < c.n; i++) {
        commute |= !strcmp(c.v[i].lever, "commute *");
        inl |= !strcmp(c.v[i].lever, "inline single-use local");
        CHECK(lv_check_parses(a, c.v[i].text, "Bar__3FooFf"));
        size_t tail = c.v[i].text.n - (size_t)c.v[i].span1;
        CHECK(!memcmp(c.v[i].text.p + c.v[i].span1, CPP_SRC + n - tail, tail));
    }
    CHECK(commute && inl);
}

AVP_TEST(every_candidate_parses_and_is_honest) {
    struct {
        const char *src;
        const char *syms[5];
    } sets[2] = {{C_SRC, {"f", "g", "h", "u", "w"}}, {CPP_SRC, {"Bar__3FooFf", "Get__3FooCFv", NULL}}};
    AvpArena *a = avp_arena();
    for (int s = 0; s < 2; s++)
        for (int k = 0; k < 5 && sets[s].syms[k]; k++) {
            const char *sym = sets[s].syms[k];
            LvCtx *x = lv_ctx_new(a, S(sets[s].src), sym);
            LvCounter orig = lv_fake_forms(a, lv_slice(x, x->span0, x->span1), sym);
            LvCandidates c = lv_generate(x, NULL, 0);
            for (size_t i = 0; i < c.n; i++) {
                CHECK_MSG(lv_check_parses(a, c.v[i].text, sym), "%s %s", sym, c.v[i].lever);
                LvS fn = {c.v[i].text.p + c.v[i].span0, (size_t)(c.v[i].span1 - c.v[i].span0)};
                CHECK_MSG(lv_fake_reasons(a, &orig, fn, sym).n == 0, "%s %s", sym, c.v[i].lever);
                CHECK(!memcmp(c.v[i].text.p, sets[s].src, (size_t)x->span0));
            }
        }
}

AVP_TEST(comments_are_never_dropped) {
    AvpCzVec v = texts("void k(int a, int b) {\n    a = 1; // one\n    b = 2;\n}\n", "k", "", NULL);
    CHECK(v.n > 0);
    for (size_t i = 0; i < v.n; i++) CHECK(strstr(v.v[i], "// one"));
}

/* ---- fake filter ---- */

static const char BASE[] = "void f(int a) {\n    int b;\n    b = a + 1;\n    g(b);\n}\n";

static AvpCzVec reasons_of(const char *base, const char *cand) {
    AvpArena *a = avp_arena();
    LvCounter o = lv_fake_forms(a, S(base), "f");
    return lv_fake_reasons(a, &o, S(cand), "f");
}

static bool in_list(AvpCzVec v, const char *k) {
    for (size_t i = 0; i < v.n; i++)
        if (!strcmp(v.v[i], k)) return true;
    return false;
}

AVP_TEST(fake_honest_edit_passes) { CHECK(reasons_of(BASE, replace(BASE, "a + 1", "1 + a")).n == 0); }

AVP_TEST(fake_each_form) {
    struct {
        const char *want, *old, *nw;
    } cases[] = {
        {"volatile", "int b;", "volatile int b;"},
        {"register", "int b;", "register int b;"},
        {"asm", "g(b);", "g(b);\n    asm { nop }"},
        {"pragma", "    g(b);", "#pragma optimization_level 0\n    g(b);"},
        {"goto", "g(b);", "goto out;\nout:\n    g(b);"},
        {"empty statement", "g(b);", "g(b);;"},
        {"empty block", "g(b);", "g(b);\n    {}"},
        {"do-while(0)", "g(b);", "do { g(b); } while (0);"},
        {"comma operator", "b = a + 1;", "b = a, b = b + 1;"},
        {"cast", "b = a + 1;", "b = (int)a + 1;"},
        {"unused local", "int b;", "int b;\n    int dummy;"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
        CHECK_MSG(in_list(reasons_of(BASE, replace(BASE, cases[i].old, cases[i].nw)), cases[i].want), "%s", cases[i].want);
}

AVP_TEST(fake_existing_forms_not_counted) {
    const char *base = replace(BASE, "b = a + 1;", "b = (int)a + 1;");
    CHECK(reasons_of(base, replace(base, "(int)a + 1", "1 + (int)a")).n == 0);
}

/* ---- search ---- */

static LvScore *mk(i64 rows, i64 tm, bool exact, const char *sig, const LvOther *others, size_t n) {
    LvScore *s = AVP_NEW(avp_arena(), LvScore);
    s->ok = true;
    s->rows = rows;
    s->tm = tm;
    s->exact = exact;
    s->sig = sig;
    s->nrows = 20;
    s->others = avp_memdup(avp_arena(), others, sizeof(LvOther) * (n ? n : 1));
    s->nothers = n;
    return s;
}

static const char *status_of(const LvScore *base, const LvScore *sc, const char **first_broken) {
    AvpCzVec b;
    const char *st = lv_classify(avp_arena(), base, sc, &b);
    *first_broken = b.n ? b.v[0] : NULL;
    return st;
}

AVP_TEST(acceptance_rule) {
    LvOther bo[2] = {{"o1", 2, 0, false}, {"o2", 0, 0, true}};
    LvScore *base = mk(5, 1, false, "base", bo, 2);
    const char *fb;
    CHECK_CZ(status_of(base, mk(0, 0, true, "x", bo, 2), &fb), "improved");
    LvOther w1[2] = {{"o1", 3, 0, false}, {"o2", 0, 0, true}};
    CHECK_CZ(status_of(base, mk(0, 0, true, "x", w1, 2), &fb), "breaks");
    CHECK_CZ(fb, "o1");
    LvOther w2[2] = {{"o1", 2, 0, false}, {"o2", 1, 0, false}};
    CHECK_CZ(status_of(base, mk(3, 0, false, "x", w2, 2), &fb), "breaks");
    CHECK_CZ(fb, "o2");
    CHECK_CZ(status_of(base, mk(3, 0, false, "x", bo, 1), &fb), "breaks");
    CHECK_CZ(fb, "o2");
    CHECK_CZ(status_of(base, mk(5, 1, false, "x", bo, 2), &fb), "neutral");
    CHECK_CZ(status_of(base, mk(5, 2, false, "x", bo, 2), &fb), "worse");
    LvScore *err = AVP_NEW(avp_arena(), LvScore);
    err->error = "err";
    CHECK_CZ(status_of(base, err, &fb), "error");
}

static const char SRC2[] = "void f(int a, int b) {\n    int i;\n    int j;\n    i = a + b;\n    j = a - b;\n    g(i, j);\n}\n";

typedef struct {
    const char *needle, *needle2;
    i64 rows, tm;
    bool exact;
    const char *sig;
    i64 other_rows;
} Rule;
typedef struct {
    const Rule *rules;
    size_t n;
} Stub;

static LvScore *stub_score(void *ctx, AvpArena *a, LvS text) {
    (void)a;
    Stub *s = ctx;
    const char *t = Z(text);
    for (size_t i = 0; i < s->n; i++) {
        const Rule *r = &s->rules[i];
        if (strstr(t, r->needle) && (!r->needle2 || strstr(t, r->needle2))) {
            LvOther o = {"other", r->other_rows, 0, false};
            return mk(r->rows, r->tm, r->exact, r->sig, &o, 1);
        }
    }
    LvOther o = {"other", 2, 0, false};
    return mk(5, 1, false, "base", &o, 1);
}

AVP_TEST(exact_at_cost_of_other_is_not_a_success) {
    Rule rules[] = {{"b + a", NULL, 0, 0, true, "e", 4},
                    {"j = -b + a", NULL, 0, 0, true, "e2", 2},
                    {"int j;\n    int i;", NULL, 3, 1, false, "s", 2}};
    Stub st = {rules, 3};
    LvSearchOpts o = {.depth = 1, .frontier = 6, .jobs = 2};
    LvSearch res = lv_search(avp_arena(), S(SRC2), "f", stub_score, &st, &o);
    LvResults imp, brk;
    lv_ranked(avp_arena(), &res.results, &imp, &brk);
    REQUIRE(brk.n > 0);
    CHECK(brk.v[0].broken.n == 1 && !strcmp(brk.v[0].broken.v[0], "other") && strstr(Z(brk.v[0].st.text), "b + a"));
    for (size_t i = 0; i < imp.n; i++) CHECK(!strstr(Z(imp.v[i].st.text), "b + a"));
    REQUIRE(imp.n > 0);
    CHECK(imp.v[0].sc->rows == 3);
    CHECK(avp_map_get(res.stats, AVP_S("breaks"))->i == 1);
}

AVP_TEST(depth_two_finds_a_pair) {
    Rule rules[] = {{"b + a", "int j;\n    int i;", 0, 0, true, "exact", 2},
                    {"b + a", NULL, 5, 1, false, "s1", 2},
                    {"int j;\n    int i;", NULL, 5, 1, false, "s2", 2}};
    Stub st = {rules, 3};
    LvSearchOpts o = {.depth = 1, .frontier = 6, .jobs = 2};
    LvSearch r1 = lv_search(avp_arena(), S(SRC2), "f", stub_score, &st, &o);
    for (size_t i = 0; i < r1.results.n; i++) CHECK(!r1.results.v[i].sc->exact);
    o.depth = 2;
    o.stop_on_exact = true;
    LvSearch r2 = lv_search(avp_arena(), S(SRC2), "f", stub_score, &st, &o);
    LvResults imp, brk;
    lv_ranked(avp_arena(), &r2.results, &imp, &brk);
    REQUIRE(imp.n > 0);
    CHECK(imp.v[0].sc->exact);
    CHECK(imp.v[0].st.npath == 2);
}

AVP_TEST(budget_skips) {
    Stub st = {NULL, 0};
    LvSearchOpts o = {.depth = 1, .frontier = 6, .jobs = 1, .deadline = 0.0001};
    o.base = stub_score(&st, avp_arena(), S(SRC2));
    LvSearch res = lv_search(avp_arena(), S(SRC2), "f", stub_score, &st, &o);
    CHECK(res.results.n == 0);
}

AVP_TEST(walls_order) {
    CHECK_CZ(lv_family_order("control flow")[0], "control");
    CHECK_CZ(lv_family_order("register/schedule")[0], "decl");
    const char *const *d = lv_family_order(NULL);
    for (int i = 0; i < 6; i++) CHECK_CZ(d[i], LV_FAMILIES[i]);
}

/* ---- diffs ---- */

AVP_TEST(diff_round_trip_and_safe_name) {
    AvpArena *a = avp_arena();
    const char *srcs[2] = {Z(avp_str_replace(a, S(C_SRC), AVP_S("\n"), AVP_S("\r\n"), -1)), NULL};
    char *lf = avp_strdup(a, C_SRC);
    lf[strlen(lf) - 1] = 0; /* no final newline */
    srcs[1] = lf;
    for (int s = 0; s < 2; s++) {
        const char *syms[2] = {"h", "w"};
        size_t total = 0;
        for (int k = 0; k < 2; k++) {
            LvCandidates c = lv_generate(lv_ctx_new(a, S(srcs[s]), syms[k]), NULL, 0);
            total += c.n;
            for (size_t i = 0; i < c.n; i++) {
                LvS d = lv_make_diff(a, S(srcs[s]), c.v[i].text, "build/tmp/x.c");
                LvS back;
                const char *err = NULL;
                CHECK_MSG(lv_apply_patch(a, S(srcs[s]), d, &back, &err), "%s", err ? err : "");
                CHECK(lv_s_eq(back, c.v[i].text));
            }
        }
        CHECK(total > 0);
    }
    LvS d = lv_make_diff(a, S("a\nb\n"), S("a\nc\n"), "x.c");
    LvS out;
    const char *err = NULL;
    CHECK(!lv_apply_patch(a, S("a\nz\n"), d, &out, &err) && err && strstr(err, "hunk does not match"));
    CHECK_CZ(lv_safe_name(a, "fn_800067A0__Fv"), "fn_800067A0__Fv");
    const char *sn = lv_safe_name(a, "__ct__Q24nw4r2ut8CharFv<int>");
    CHECK(strlen(sn) <= 79 && strchr(sn, '<') == NULL);
}

AVP_TEST_MAIN("levers")
