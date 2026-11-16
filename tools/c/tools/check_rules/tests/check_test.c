/* check_rules: the per-file checks (rules_check.h): one small source per rule group, checked as Checker.check_file
 * checks it, and the configure.py / exceptions.md readers. The whole-tree proof is the differential harness. */
#include "avp/test.h"
#include "rules_check.h"

typedef struct {
    RlChecker *ck;
    size_t start;
} Run;

/* check_file over one in-memory source (plus optional others), with run()'s prelude; status: "Matching" or NULL. */
static Run check2(const char *path, const char *src, const char *path2, const char *src2, const char *status) {
    AvpArena *a = avp_arena();
    AvpStrVec paths = {0}, texts = {0};
    avp_vec_push(&paths, avp_str(path));
    avp_vec_push(&texts, avp_str(src));
    if (path2) {
        avp_vec_push(&paths, avp_str(path2));
        avp_vec_push(&texts, avp_str(src2));
    }
    RlConfig cfg = {0};
    if (status) {
        cfg.unit_status = avp_map_new();
        AvpStr *v = AVP_NEW(a, AvpStr);
        *v = avp_str(status);
        avp_map_put(cfg.unit_status, avp_str(path + 4), NULL)->p = v;
    }
    RlChecker *ck = rl_checker_from(a, &paths, &texts, &cfg);
    rl_checker_prepare(ck);
    rl_check_file(ck, rl_checker_parse(ck, avp_str(path)));
    return (Run){ck, 0};
}
static Run check(const char *path, const char *src) { return check2(path, src, NULL, NULL, NULL); }

/* The number of findings of rule (and tier, when given) whose message contains needle (when given). */
static int count(Run r, const char *rule, const char *tier, const char *needle) {
    int n = 0;
    for (size_t i = r.start; i < r.ck->findings.n; i++) {
        const RlFinding *f = &r.ck->findings.v[i];
        if (strcmp(f->rule, rule) || (tier && strcmp(f->tier, tier))) continue;
        if (needle && !avp_str_contains(f->msg, avp_str(needle))) continue;
        n++;
    }
    return n;
}

AVP_TEST(tier_of) {
    CHECK(!strcmp(rl_tier_of("R0"), "free"));
    CHECK(!strcmp(rl_tier_of("R4"), "match"));
    CHECK(!strcmp(rl_tier_of("R37"), "checked"));
    CHECK(rl_tier_of("R38") == NULL);
    CHECK(rl_tier_of("R01") == NULL);
    CHECK(rl_tier_of("X1") == NULL);
}

AVP_TEST(statements_and_decls) {
    RlParsed *P = rl_parse(avp_arena(), AVP_S("src/t.c"),
                           AVP_S("void f(void) { int a = 1, *b; static const u8 *p; x = (y); s t = {1, {2}}; }"),
                           NULL);
    const RlFunc *f = &P->p.funcs.v[0];
    RlRange *st;
    size_t ns = rl_statements(P->toks.v, P->match, f->body + 1, f->body_end, &st);
    REQUIRE(ns == 4);
    RlDecl *d;
    size_t nd = rl_parse_decl(avp_arena(), P->toks.v + st[0].lo, (size_t)(st[0].hi - st[0].lo), &d);
    REQUIRE(nd == 2);
    CHECK_STR(d[0].name, "a");
    CHECK_STR(d[0].type, "int");
    CHECK_STR(d[1].name, "b");
    CHECK_STR(d[1].type, "int *");
    nd = rl_parse_decl(avp_arena(), P->toks.v + st[1].lo, (size_t)(st[1].hi - st[1].lo), &d);
    REQUIRE(nd == 1);
    CHECK_STR(d[0].type, "static const u8 *");
    CHECK(rl_parse_decl(avp_arena(), P->toks.v + st[2].lo, (size_t)(st[2].hi - st[2].lo), &d) == 0);
    /* the aggregate initialiser stays in its statement */
    CHECK(RL_TEQ(&P->toks.v[st[3].hi], ";"));
}

AVP_TEST(cast_info) {
    RlParsed *P = rl_parse(avp_arena(), AVP_S("src/t.c"), AVP_S("x = (u8 *)p + (int)q - f(r) * (Foo)s;"), NULL);
    const RlTok *t = P->toks.v;
    size_t n = P->toks.n;
    CHECK(rl_cast_info(t, n, P->match, 2, false) == 5);  /* (u8 *) */
    CHECK(rl_cast_info(t, n, P->match, 8, false) == 10); /* (int) */
    CHECK(rl_cast_info(t, n, P->match, 14, false) < 0);  /* f(r): a call */
    CHECK(rl_cast_info(t, n, P->match, 19, false) < 0);  /* (Foo): not a scalar, not a pointer */
}

AVP_TEST(identifiers_r7) {
    Run r = check("src/t.c", "/* f */\nvoid fn_80001234(void) { int temp1 = 0; temp1 = temp1 + 1; }\n");
    CHECK(count(r, "R7", "checked", "placeholder name 'fn_80001234' (address-derived function name)") == 1);
    CHECK(count(r, "R7", NULL, "'temp1' (numbered generic name): open naming work [3 uses]") == 1);
    r = check("src/t.c", "int Data_12345678;\n");
    CHECK(count(r, "R7", NULL, "address-like name 'Data_12345678'") == 1);
}

AVP_TEST(dialect_r22) {
    Run r = check("src/t.cpp", "enum class E { A };\nint *p = nullptr;\n#include <stdint.h>\n");
    CHECK(count(r, "R22", "free", "'nullptr' is not C++98") == 1);
    CHECK(count(r, "R22", "free", "'enum class' is not C++98") == 1);
    CHECK(count(r, "R22", "free", "<stdint.h>") == 1);
    r = check("src/t.c", "int *p = nullptr;\n");
    CHECK(count(r, "R22", NULL, NULL) == 0);
}

AVP_TEST(offsets_r4) {
    Run r = check("src/t.c", "/* f */\nint f(u8 *p) {\n    return *(u32 *)((u8 *)p + 0x10);\n}\n");
    CHECK(count(r, "R4", "match", "fixed-offset access: a byte pointer '+ 0x10'") == 1);
    r = check("src/t.c", "/* f */\nvoid f(u8 *p) {\n    g((u32 *)((u8 *)p + 8));\n}\n");
    CHECK(count(r, "R4", "advisory", "literal offset 8") == 1);
    r = check("src/t.c", "#define BUF_OFFSET 0x20\n/* f */\nvoid f(char *p) {\n    g(p + BUF_OFFSET);\n}\n");
    CHECK(count(r, "R4", "match", "offset macro 'BUF_OFFSET' offsets a pointer") == 1);
    r = check("src/t.c", "#define BUF_OFFSET 0x20\n/* f */\nvoid f(int n) {\n    g(n + BUF_OFFSET);\n}\n");
    CHECK(count(r, "R4", NULL, NULL) == 0);
}

AVP_TEST(volatile_r17) {
    Run r = check("src/t.c", "/* f */\nvoid f(void) {\n    volatile int i;\n    g(i);\n}\n");
    CHECK(count(r, "R17", "match", "volatile local variable") == 1);
    r = check("src/t.c", "/* f */\nvoid f(void) {\n    volatile u32 a; // volatile: delay loop, tp os.c\n"
                         "    for (a = 0; a < 1000; a++) {}\n}\n");
    CHECK(count(r, "R17", NULL, "volatile") == 0);
    r = check("src/t.c", "extern volatile u32 reg;\n");
    CHECK(count(r, "R17", "match", "volatile variable without a '// volatile: <evidence>' note") == 1);
    r = check("src/t.c", "extern volatile u32 reg; // volatile: hardware register\n");
    CHECK(count(r, "R17", NULL, NULL) == 0);
}

/* Ruling of 2026-11-15, 7(a): a file-scope volatile written from callback context, with its note. A static local is
 * still a local. */
AVP_TEST(volatile_r17_global) {
    Run r = check("src/t.c", "volatile u16 tok = 1; // volatile: written by the GX draw sync callback\n");
    CHECK(count(r, "R17", NULL, "volatile") == 0);
    r = check("src/t.c", "volatile u16 tok = 1;\n");
    CHECK(count(r, "R17", "match", "volatile variable without a '// volatile: <evidence>' note") == 1);
    r = check("src/t.c", "/* f */\nvoid f(void) {\n    static volatile int s; // volatile: callback\n    g(s);\n}\n");
    CHECK(count(r, "R17", "match", "volatile local variable") == 1);
}

/* Ruling of 2026-11-15, 7(b): a volatile local stored once and read only by a release-disabled assertion. */
#define ASSERT_DEFS "#ifdef _DEBUG\n#define ASSERTMSG1(exp, msg, p) (void)((exp) || (panic((msg), (p)), 0))\n#else\n" \
                    "#define ASSERTMSG1(exp, msg, p) ((void)0)\n#endif\n"
#define ASSERT_NOTE "// volatile: read by a release-disabled assertion, zeldaret/tp dsp_task.c\n"
static int assert_only(const char *defs, const char *note, const char *decl, const char *body) {
    char src[1024];
    snprintf(src, sizeof src, "%s/* f */\nvoid f(void) {\n    %s    %s\n%s}\n", defs, note, decl, body);
    return count(check("src/t.c", src), "R17", "match", "volatile local variable");
}
AVP_TEST(volatile_r17_assert_only) {
    const char *ok = "    mail = read();\n    ASSERTMSG1(mail == 1, \"sync (0x%08X)\", mail);\n";
    CHECK(assert_only(ASSERT_DEFS, ASSERT_NOTE, "volatile u32 mail;", ok) == 0);
    /* the nearest rejected shapes */
    CHECK(assert_only(ASSERT_DEFS, "", "volatile u32 mail;", ok) == 1);
    CHECK(assert_only(ASSERT_DEFS, "// volatile: tp dsp_task.c\n", "volatile u32 mail;", ok) == 1);
    CHECK(assert_only(ASSERT_DEFS, ASSERT_NOTE, "volatile u32 mail = read();",
                      "    ASSERTMSG1(mail == 1, \"sync\", mail);\n") == 1);
    CHECK(assert_only(ASSERT_DEFS, ASSERT_NOTE, "volatile u32 mail;", "    mail = read();\n    g(mail);\n") == 1);
    CHECK(assert_only(ASSERT_DEFS, ASSERT_NOTE, "volatile u32 mail;",
                      "    mail = read();\n    mail = read();\n    ASSERTMSG1(mail == 1, \"sync\", mail);\n") == 1);
    CHECK(assert_only(ASSERT_DEFS, ASSERT_NOTE, "volatile u32 mail;", "    mail = read();\n") == 1);
    CHECK(assert_only(ASSERT_DEFS, ASSERT_NOTE, "volatile u32 mail;",
                      "    mail = read();\n    ASSERTMSG1(mail == 1, \"sync\", g(mail));\n    h(mail);\n") == 1);
    CHECK(assert_only("#define ASSERTMSG1(exp, msg, p) check((exp), (msg), (p))\n", ASSERT_NOTE, "volatile u32 mail;",
                      ok) == 1);
}

/* check2 with an exceptions record read into the configuration */
static Run check_allow(const char *path, const char *src, const char *path2, const char *src2, const char *md) {
    AvpArena *a = avp_arena();
    AvpStrVec paths = {0}, texts = {0};
    avp_vec_push(&paths, avp_str(path));
    avp_vec_push(&texts, avp_str(src));
    if (path2) {
        avp_vec_push(&paths, avp_str(path2));
        avp_vec_push(&texts, avp_str(src2));
    }
    RlConfig cfg = {0};
    rl_allow_from_markdown(a, &cfg, avp_str(md), AVP_S("docs/exceptions.md"));
    RlChecker *ck = rl_checker_from(a, &paths, &texts, &cfg);
    rl_checker_prepare(ck);
    rl_check_file(ck, rl_checker_parse(ck, avp_str(path)));
    return (Run){ck, 0};
}
static int r18(Run r, const char *status, const char *func, const char *needle) {
    int n = 0;
    for (size_t i = r.start; i < r.ck->findings.n; i++) {
        const RlFinding *f = &r.ck->findings.v[i];
        if (strcmp(f->rule, "R18") || (status && strcmp(f->status, status))) continue;
        if (func && !avp_str_eqz(f->func, func)) continue;
        if (needle && !avp_str_contains(f->msg, avp_str(needle))) continue;
        n++;
    }
    return n;
}

/* Ruling of 2026-11-15, 8: R18 belongs to the function whose own body holds assembly. */
#define HELPER_H "#ifndef V_H\n#define V_H\ninline void V3dSub(register float* d) {\n    asm {\n        ps_sub f0, f1, f2\n" \
                 "    }\n}\n#endif\n"
#define HELPER_USE "#include <v.h>\n/* f */\nvoid f(float* d) {\n    V3dSub(d);\n}\n"
#define LISTED "### Header-level asm inlines (ORIGINAL-ASM)\n| unit | function | evidence |\n|---|---|---|\n| `include/v.h` | `V3dSub` | 3 call sites |\n"
AVP_TEST(asm_helpers_r18) {
    /* a listed header-level asm inline: allowed as ORIGINAL-ASM; its caller has no finding */
    Run r = check_allow("include/v.h", HELPER_H, "src/u.c", HELPER_USE, LISTED);
    CHECK(r18(r, "allowed", "V3dSub", NULL) == 1);
    CHECK(r18(r, "violation", NULL, NULL) == 0);
    r = check_allow("src/u.c", HELPER_USE, "include/v.h", HELPER_H, LISTED);
    CHECK(r18(r, NULL, NULL, NULL) == 0);
    /* not listed (a source marker does not excuse it) */
    r = check_allow("include/v.h", HELPER_H, NULL, NULL, "## ORIGINAL-ASM\n");
    CHECK(r18(r, "violation", "V3dSub", "helper not listed as ORIGINAL-ASM") == 1);
    r = check_allow("include/v.h", "// ASM-HELPER: retail\n" HELPER_H, NULL, NULL, "## ORIGINAL-ASM\n");
    CHECK(r18(r, "violation", "V3dSub", "helper not listed as ORIGINAL-ASM") == 1);
    /* a source-file helper called once: the finding is the caller's */
    const char *once = "static inline void H(register float* d) {\n    asm {\n        nop\n    }\n}\n"
                       "/* f */\nvoid f(float* d) {\n    H(d);\n}\n";
    r = check_allow("src/u.c", once, NULL, NULL, "");
    CHECK(r18(r, "violation", "f", "inline-asm helper 'H' (line 1) is called only here") == 1);
    CHECK(r18(r, NULL, "H", NULL) == 0);
    /* called twice: a helper; the finding stays on it */
    const char *twice = "static inline void H(register float* d) {\n    asm {\n        nop\n    }\n}\n"
                        "/* f */\nvoid f(float* d) {\n    H(d);\n    H(d + 3);\n}\n";
    r = check_allow("src/u.c", twice, NULL, NULL, "");
    CHECK(r18(r, "violation", "H", "inline assembly in 'H'") == 1);
    CHECK(r18(r, NULL, "f", NULL) == 0);
    /* an `asm` function called once keeps its own finding */
    r = check_allow("src/u.c", "asm void H(void) {\n    nofralloc\n    blr\n}\n/* f */\nvoid f(void) {\n    H();\n}\n",
                    NULL, NULL, "");
    CHECK(r18(r, "violation", "H", "function 'H' is written in assembly") == 1);
}

AVP_TEST(casts_r19_r26) {
    Run r = check("src/t.c", "/* f */\nvoid f(void *p, int n) {\n    g((u8 *)(u32 *)p, (int)n);\n"
                             "    h(((S *)p)->x);\n}\n");
    CHECK(count(r, "R19", "checked", "cast chain (u8*)(u32*)") == 1);
    CHECK(count(r, "R19", "free", "cast of 'n' to its own type (int)") == 1);
    CHECK(count(r, "R26", "checked", "void* parameter 'p'") == 1);
}

AVP_TEST(function_body_r14_r17) {
    Run r = check("src/t.c", "/* f */\nvoid f(int a) {\n    ;\n    if (a) { }\n    if (a) ;\n    { g(); }\n"
                             "    goto L1;\nL1:\n    g();\n    a = a;\n    h(a + 0);\n    do { g(); } while (0);\n}\n");
    CHECK(count(r, "R17", "match", "bare ';' statement") == 1);
    CHECK(count(r, "R17", "match", "empty 'if' block") == 1);
    CHECK(count(r, "R17", "match", "empty 'if' body") == 1);
    CHECK(count(r, "R17", "advisory", "arbitrary '{ }' block") == 1);
    CHECK(count(r, "R14", "checked", "label 'L1' is not a meaningful cleanup/retry name") == 1);
    CHECK(count(r, "R17", "match", "self-assignment 'a = a'") == 1);
    CHECK(count(r, "R17", "match", "'+ 0' is a no-op steering expression") == 1);
    CHECK(count(r, "R17", "match", "do { } while ( 0 )") == 1);
}

AVP_TEST(locals_control_literals) {
    Run r = check("src/t.c", "/* f */\nvoid f(int a) {\n    int unused;\n    extern int g_x;\n    register int k = a;\n"
                             "    if (a) g(k);\n    while ((a = h()) != 0) {}\n    if (a = 2) {}\n"
                             "    a = 4096 & k;\n    a = 0x10000;\n    a = (b, c);\n    for (; k < 0x10; k++) {}\n}\n");
    CHECK(count(r, "R17", "match", "local 'unused' is never read") == 1);
    CHECK(count(r, "R2", "match", "local extern global declaration") == 1);
    CHECK(count(r, "R17", "advisory", "'register' local") == 1);
    CHECK(count(r, "R35", "free", "'if' body without braces") == 1);
    CHECK(count(r, "R35", "checked", "assignment inside a condition") == 1);
    CHECK(count(r, "R9", "free", "decimal mask/flag 4096") == 1);
    CHECK(count(r, "R11", "free", "unexplained constant 0x10000") == 1);
    CHECK(count(r, "R9", "free", "hex loop/compare bound 0x10") == 1);
}

/* Ruling of 2026-11-16: the engine's debug-warning macro RWS_WARN (include/climax/Debug.h) is a release no-op; a
 * local that only its arguments read is read by the Debug build's warning, as with RWASSERT. */
AVP_TEST(locals_read_by_debug_warning) {
    const char *defs = "#define RWS_WARN(cond, text)\n";
    char src[512];
    snprintf(src, sizeof src, "%s/* f */\nvoid f(void) {\n    int* existing = find();\n"
                              "    RWS_WARN( existing != NULL, \"exists\" );\n    g();\n}\n", defs);
    Run r = check("src/t.cpp", src);
    CHECK(count(r, "R17", "match", "local 'existing' is never read") == 0);
    CHECK(count(r, "R17", NULL, NULL) == 0);
    snprintf(src, sizeof src, "%s/* f */\nvoid f(void) {\n    int* existing = find();\n    g();\n}\n", defs);
    r = check("src/t.cpp", src);
    CHECK(count(r, "R17", "match", "local 'existing' is never read") == 1);
}

AVP_TEST(function_meta_r15_r25_r33) {
    Run r = check("src/t.c", "int g();\nint h(void);\nvoid a()\n{\n    g(1); g(2); g(3);\n}  // 0x80001000\n"
                             "/* b */\nvoid b(void)\n{\n    g(1); g(2); g(3);\n}  // 0x80000800\n");
    CHECK(count(r, "R33", "free", "function without a header comment") == 1);
    CHECK(count(r, "R15", "checked", "0x80000800 is placed after 0x80001000 (a)") == 1);
    CHECK(count(r, "R25", "checked", "'()' parameter list in C") == 1);
    CHECK(count(r, "R25", "checked", "'()' prototype in C") == 1);
    CHECK(count(r, "R25", "checked", "prototype of 'g' in a source file") == 1);
    CHECK(count(r, "R25", "checked", "prototype of 'h' in a source file") == 1);
    REQUIRE(r.ck->P->p.funcs.n == 2);
    CHECK(r.ck->P->p.funcs.v[0].has_addr && r.ck->P->p.funcs.v[0].addr == 0x80001000u);
}

AVP_TEST(directives_r3_r31_r37) {
    Run r = check2("src/t.c", "#if 0\nint x;\n#endif\n#ifdef NON_MATCHING\n#endif\n#define SQ(x) x * x\n"
                              "// NONMATCHING here\n",
                   NULL, NULL, "Matching");
    CHECK(count(r, "R37", "checked", "'#if 0': dead or unconditional block") == 1);
    CHECK(count(r, "R37", "checked", "#ifdef in a Matching unit") == 1);
    CHECK(count(r, "R37", "checked", "NONMATCHING marker in a Matching unit") == 1);
    CHECK(count(r, "R31", "free", "parameter 'x' is not parenthesised") == 1);
    CHECK(count(r, "R31", "free", "expansion is not parenthesised") == 1);
    r = check("include/t.h", "int f(void);\n");
    CHECK(count(r, "R3", "free", "header without an include guard") == 1);
    r = check("include/t.h", "#ifndef T_H\n#define T_H\n#endif\n");
    CHECK(count(r, "R3", NULL, NULL) == 0);
}

AVP_TEST(comments_asm_declarations) {
    Run r = check("src/t.cpp", "// probe: try this\n// was at 0x80001234 once\n// matched at 98%\n"
                               "extern int g;\nstruct S {\n    u8 pad[4];\n\n    u8 pad2[4]; // pad: SDK name\n};\n"
                               "class C { virtual void dummy1(); };\n"
                               "asm void f(void) {\n    nofralloc\n}\n");
    CHECK(count(r, "R17", "match", "marked as a probe or fakematch") == 1);
    CHECK(count(r, "R21", "free", "address in a comment") == 1);
    CHECK(count(r, "R21", "free", "history, score or tooling note") == 1);
    CHECK(count(r, "R2", "match", "extern declaration of 'g' outside a header") == 1);
    CHECK(count(r, "R17", "match", "padding member 'pad'") == 1);
    CHECK(count(r, "R17", "match", "padding member 'pad2'") == 0);
    CHECK(count(r, "R13", "match", "padding/dummy virtual 'dummy1'") == 1);
    CHECK(count(r, "R18", "match", "function 'f' is written in assembly") == 1);
    r = check("src/t.c", "/* ORIGINAL-ASM: retail */\nasm void f(void) {\n    nofralloc\n}\n");
    REQUIRE(count(r, "R18", NULL, NULL) == 1);
    CHECK(!strcmp(r.ck->findings.v[r.ck->findings.n - 1].status, "allowed"));
    r = check("include/t.h", "#pragma once\nint g_count;\nextern int g_ok;\nstatic int s_ok;\n");
    CHECK(count(r, "R2", "match", "definition of 'g_count' in a header") == 1);
    CHECK(count(r, "R2", NULL, NULL) == 1);
}

AVP_TEST(no_copy_in_retail_drops_r17) {
    Run r = check("src/t.c", "/* f */\nvoid f(int a) {\n    a = a;\n    g(a); g(a); g(a);\n} // no copy in retail\n");
    CHECK(count(r, "R17", NULL, NULL) == 0);
}

AVP_TEST(header_language) {
    /* a .h included only by C++ sources is C++ (R25 does not apply); one a C source includes stays C */
    Run r = check2("include/a.h", "#pragma once\nint f();\n", "src/x.cpp", "#include \"a.h\"\n", NULL);
    CHECK(count(r, "R25", NULL, "'()' prototype in C") == 0);
    r = check2("include/a.h", "#pragma once\nint f();\n", "src/x.c", "#include \"a.h\"\n", NULL);
    CHECK(count(r, "R25", NULL, "'()' prototype in C") == 1);
}

AVP_TEST(configure_and_allow) {
    AvpArena *a = avp_arena();
    RlConfig cfg = {0};
    AvpStr conf = AVP_S("cflags_base = [\"-O4\", \"-i include\"]\n"
                        "cflags_z = [*cflags_base, \"-i include/zlib\", \"-i build/x\"]\n"
                        "def ZLib(name, objects):\n    return {\"lib\": name, \"cflags\": cflags_z, \"objects\": objects}\n"
                        "config.libs = [\n"
                        "    ZLib(\"zlib\", [Object(Matching, \"zlib/inflate.c\"),\n"
                        "        Object(NonMatching, \"zlib/crc32.c\", extra_cflags=[\"-i include/extra\"])]),\n"
                        "    {\"lib\": \"x\", \"cflags\": [\"-i inc2\"], \"objects\": [Object(Equivalent, \"x/a.c\")]},\n"
                        "    # Object(Matching, \"x/commented.c\")\n"
                        "]\n");
    rl_parse_units(a, &cfg, conf);
    rl_parse_include_dirs(a, &cfg, conf);
    CHECK(avp_map_len(cfg.unit_status) == 3);
    CHECK_STR(*(AvpStr *)avp_map_get(cfg.unit_status, AVP_S("zlib/crc32.c"))->p, "NonMatching");
    const AvpStrVec *d = avp_map_get(cfg.include_map, AVP_S("zlib/crc32.c"))->p;
    REQUIRE(d->n == 3);
    CHECK_STR(d->v[0], "include");
    CHECK_STR(d->v[1], "include/zlib");
    CHECK_STR(d->v[2], "include/extra");
    d = avp_map_get(cfg.include_map, AVP_S("x/a.c"))->p;
    REQUIRE(d->n == 1);
    CHECK_STR(d->v[0], "inc2");
    rl_allow_from_markdown(a, &cfg,
                           AVP_S("# ORIGINAL-ASM\n| unit | functions |\n|---|---|\n| `main/os/OSCache` | `DCFlush`, "
                                 "`ICInv` |\n## Listed inline-asm helpers\n| `os/OSFast` | `castF` | `castF(x)` uses "
                                 "`helper(y)` |\n"),
                           AVP_S("docs/exceptions.md"));
    CHECK(avp_set_has(cfg.original_asm, AVP_S("os/OSCache\x01" "DCFlush")));
    CHECK(avp_set_has(cfg.original_asm, AVP_S("os/OSCache\x01ICInv")));
    CHECK(avp_set_has(cfg.asm_helpers, AVP_S("os/OSFast\x01" "castF")));
    CHECK(avp_set_len(cfg.asm_helpers) == 1); /* the helper call in column 3's first code span is castF itself */
}

AVP_TEST(finding_text_and_json) {
    RlFinding f = {AVP_S("src/a.c"), 3, "R7", "checked", AVP_S("msg"), AVP_S("fn"), "allowed", AVP_S("why")};
    CHECK_STR(rl_finding_text(avp_arena(), &f), "src/a.c:3: R7 [checked] msg (in fn) -- allowed: why");
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    AvpJsonW w;
    avp_jsonw_init(&w, &b, &AVP_JSON_COMPACT);
    f.func = (AvpStr){0};
    rl_finding_json(&w, &f);
    avp_jsonw_finish(&w);
    CHECK_STR(avp_strn(b.p, b.n), "{\"path\":\"src/a.c\",\"line\":3,\"rule\":\"R7\",\"tier\":\"checked\",\"message\":"
                                  "\"msg\",\"function\":null,\"status\":\"allowed\",\"reason\":\"why\"}");
}

AVP_TEST_MAIN("check_rules")
