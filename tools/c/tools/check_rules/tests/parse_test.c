/* check_rules: the parse model (rules_parse.h): functions with head/body/body_end, K&R definitions, asm masking,
 * aggregates, declarations, derived facts. */
#include "avp/test.h"
#include "rules_parse.h"

static RlParsed *parse(const char *path, const char *src) {
    return rl_parse(avp_arena(), avp_str(path), avp_str(src), NULL);
}

static const RlFunc *func(const RlParsed *P, const char *name) {
    for (size_t i = 0; i < P->p.funcs.n; i++)
        if (avp_str_eqz(P->p.funcs.v[i].name, name)) return &P->p.funcs.v[i];
    return NULL;
}

AVP_TEST(function_positions) {
    RlParsed *P = parse("src/t.c", "static int f(int a, char *b)\n{\n    return a;\n}\nvoid g(void) {}\n");
    REQUIRE(P->ok);
    REQUIRE(P->p.funcs.n == 2);
    const RlFunc *f = &P->p.funcs.v[0];
    CHECK_STR(f->name, "f");
    CHECK_EQ_I64(f->head, 0);
    CHECK_EQ_I64(f->lparen, 3);
    CHECK_EQ_I64(f->rparen, 10);
    CHECK(RL_TEQ(&P->toks.v[f->body], "{"));
    CHECK(RL_TEQ(&P->toks.v[f->body_end], "}"));
    CHECK_EQ_I64(f->line, 1);
    CHECK_EQ_I64(f->end_line, 4);
    CHECK(f->is_static);
    REQUIRE(f->params.n == 2);
    CHECK_STR(f->params.v[0].name, "a");
    CHECK_STR(f->params.v[0].type, "int");
    CHECK_STR(f->params.v[1].type, "char *");
    CHECK_EQ_I64((i64)P->p.funcs.v[1].params.n, 0);
    CHECK_EQ_I64(rl_func_at(P, 3), 0);
    CHECK_EQ_I64(rl_func_at(P, 6), -1);
}

AVP_TEST(knr_definition) {
    RlParsed *P = parse("src/z.c", "uLong adler32(adler, buf, len)\n    register uLong adler, *len;\n"
                                   "    const Bytef *buf;\n{\n    return adler;\n}\n");
    REQUIRE(P->p.funcs.n == 1);
    const RlFunc *f = &P->p.funcs.v[0];
    CHECK_STR(f->name, "adler32");
    REQUIRE(f->params.n == 3);
    CHECK_STR(f->params.v[1].name, "buf");
    CHECK_STR(f->params.v[1].type, "const Bytef *");
    CHECK_STR(f->params.v[2].type, "register uLong *"); /* later declarators take the first one's base type */
    CHECK(f->params.v[2].reg);
    CHECK_EQ_I64(f->end_line, 6);
}

AVP_TEST(asm_masking) {
    RlParsed *P = parse("src/a.c", "asm void fa(void) {\n    nofralloc\n    blr\n}\n"
                                   "void fb(void) {\n    asm { mr r3, r4 }\n    __emit(1);\n}\n");
    REQUIRE(P->p.funcs.n == 2);
    REQUIRE(P->asm_sites.n == 3);
    CHECK_STR(P->asm_sites.v[0].kind, "function");
    CHECK_STR(P->asm_sites.v[1].kind, "block");
    CHECK_STR(P->asm_sites.v[1].name, "fb");
    CHECK_STR(P->asm_sites.v[2].kind, "token:__emit");
    REQUIRE(P->asm_ids.n == 2);
    CHECK(RL_TEQ(&P->toks.v[P->asm_ids.v[0].tok], "__ASM_BODY__"));
    REQUIRE(P->asm_ids.v[0].ids.n == 2);
    CHECK_STR(P->asm_ids.v[0].ids.v[0], "blr");
    CHECK_STR(P->asm_ids.v[0].ids.v[1], "nofralloc");
    CHECK(P->p.funcs.v[0].is_asm);
    CHECK_EQ_I64(P->p.funcs.v[0].body_end - P->p.funcs.v[0].body, 3); /* { __ASM_BODY__ ; } */
}

AVP_TEST(aggregates_and_decls) {
    RlParsed *P = parse("src/s.c", "typedef struct { int a, b; void (*cb)(int); } T, *PT;\n"
                                   "struct S { int c[4]; } s1 = {0};\nstruct fwd;\nextern int ext;\n"
                                   "static const int k = 3;\nint proto(void);\nint none();\nu32 reg : 0x80000000;\n"
                                   "#define local static\nlocal int lv;\n");
    REQUIRE(P->p.aggs.n == 2);
    CHECK_STR(P->p.aggs.v[0].name, "PT");
    CHECK_EQ_I64(P->p.aggs.v[0].first_hidden, 1);
    REQUIRE(P->p.aggs.v[0].members.n == 1);
    CHECK_STR(P->p.aggs.v[0].members.v[0], "int a , b");
    CHECK_STR(P->p.aggs.v[1].member_decls.v[0].name, "c");
    REQUIRE(P->p.fwds.n == 1);
    CHECK_STR(P->p.fwds.v[0].name, "fwd");
    REQUIRE(P->p.protos.n == 2);
    CHECK(!P->p.protos.v[0].empty_params);
    CHECK(P->p.protos.v[1].empty_params);
    REQUIRE(P->p.vars.n == 5);
    CHECK_STR(P->p.vars.v[0].name, "s1");
    CHECK_STR(P->p.vars.v[0].type_text, "struct S");
    CHECK(P->p.vars.v[1].is_extern);
    CHECK(P->p.vars.v[2].is_static && P->p.vars.v[2].is_const && P->p.vars.v[2].has_init);
    CHECK(P->p.vars.v[3].addr_bound);
    CHECK(P->p.vars.v[4].is_static); /* the file's own `#define local static` */
    CHECK_EQ_I64(rl_storage_get(P, avp_str("local")), RL_STORAGE_STATIC);
}

AVP_TEST(cpp_model) {
    RlParsed *P = parse("src/c.cpp", "namespace A { namespace { int h() { return 0; } }\n"
                                     "class B { public: virtual ~B(); virtual int f() const; int g() { return 1; } };\n"
                                     "int B::f() const { return 0; }\nbool operator==(int a, int b) { return 1; } }\n");
    CHECK(P->is_cpp && !P->is_header);
    CHECK(rl_cpp_syntax(P));
    const RlFunc *h = func(P, "h");
    REQUIRE(h);
    CHECK(h->anon_ns);
    REQUIRE(func(P, "B::g"));
    CHECK_STR(func(P, "B::g")->in_class, "B");
    REQUIRE(func(P, "B::f"));
    REQUIRE(func(P, "operator=="));
    REQUIRE(P->p.virtuals.n == 2);
    CHECK_STR(P->p.virtuals.v[0].name, "B::B"); /* the quirk: decl() names the token before '(' */
    CHECK(avp_set_has(P->func_names, avp_str("f")));
}

AVP_TEST(notes_and_derived) {
    RlParsed *P = parse("src/v.c", "#define DATA_OFFSET 4\n#define HDR_OFS 8\nvoid f(void) {\n"
                                   "    volatile u32 a; // volatile: delay loop, ref\n}\n"
                                   "void g(void) {\n} // no copy in retail (dead-stripped)\n");
    CHECK(avp_set_has(rl_offset_macros(P), avp_str("DATA_OFFSET")));
    CHECK(!avp_set_has(rl_offset_macros(P), avp_str("HDR_OFS")));
    CHECK(avp_set_has(rl_volatile_noted(P), avp_str("a")));
    CHECK(rl_has_note(P, 4, rl_re_delay_note(), true));
    CHECK(!rl_has_note(P, 5, rl_re_delay_note(), true)); /* a trailing comment does not cover the next line */
    CHECK(!rl_no_copy_in_retail(P, &P->p.funcs.v[0]));
    CHECK(rl_no_copy_in_retail(P, &P->p.funcs.v[1]));
    CHECK(!rl_cpp_syntax(P));
}

AVP_TEST(includes_and_lex_error) {
    RlParsed *P = parse("include/x.h", "#include <a/b.h>\n#include \"c.h\"\n#include MACRO\n");
    CHECK(P->is_header);
    REQUIRE(P->includes.n == 2);
    CHECK_STR(P->includes.v[1], "c.h");
    RlParsed *E = parse("src/e.c", "int\xc2\xa0x;");
    CHECK(!E->ok);
    rl_parsed_free(P);
}

AVP_TEST_MAIN("check_rules")
