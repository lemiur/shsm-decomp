/* draft_text.c: names and paths, the substitution map, clone_header and the grading helpers. Expected values are
 * the Python's (draft.py's functions run on the same inputs; JSON strings are its json.dumps output). */
#include "avp/test.h"
#include "draft.h"
#include "shsm_re.h"

#include <string.h>

static const char *dumps(AvpJson *v) {
    AvpJsonDumpOpts o = {0};
    return avp_cz(avp_arena(), avp_json_dumps(avp_arena(), v, &o));
}

static const char *jstr(AvpStr s) { /* json.dumps(s) */
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_json_put_string(&b, s, false);
    return avp_buf_cz(&b);
}

AVP_TEST(paths) {
    AvpArena *a = avp_arena();
    ShsmRepo *r = shsm_repo_find(NULL);
    char *rel;
    REQUIRE(avp_path_relpath(a, draft_root(a, r), r->root, &rel));
    CHECK_CZ(rel, "build\\draft");
    REQUIRE(avp_path_relpath(a, draft_rejects_path(a, r), r->root, &rel));
    CHECK_CZ(rel, "config\\R5WEA4\\draft_rejects.tsv");
    CHECK_CZ(draft_rel_path(a, r, avp_path_join(a, r->root, "src\\game\\X.cpp")), "src/game/X.cpp");
    CHECK_CZ(draft_rel_path(a, r, "Z:\\other\\f.c"), "Z:/other/f.c");
}

AVP_TEST(names) {
    AvpArena *a = avp_arena();
    CHECK_CZ(draft_safe_name(a, "__sinit_\\PadRumble_cpp"), "__sinit__PadRumble_cpp");
    CHECK_CZ(draft_safe_name(a, "a/b:c*d?e\"f<g>h|i"), "a_b_c_d_e_f_g_h_i");
    CHECK_CZ(draft_strip_ext(a, "game/X.cpp"), "game/X");
    CHECK_CZ(draft_strip_ext(a, ""), "");
    CHECK(draft_strip_ext(a, NULL) == NULL);
    CHECK_CZ(draft_strip_ext(a, "a.b/c"), "a.b/c");
    CHECK_CZ(draft_plain_unit(a, "main/game/PadRumble"), "game/PadRumble");
    CHECK_CZ(draft_plain_unit(a, "game/X.cpp"), "game/X");
    CHECK_CZ(draft_plain_unit(a, NULL), "");
    i64 s = 0, e = 0;
    CHECK(draft_parse_range(AVP_S(" 0x80001000 - 80002000 "), &s, &e));
    CHECK_EQ_I64(s, 2147487744);
    CHECK_EQ_I64(e, 2147491840);
    CHECK(!draft_parse_range(AVP_S("80001000-8000200"), &s, &e));
    CHECK(draft_parse_range(AVP_S("800010aB-0x8000200F\n"), &s, &e));
    CHECK_EQ_I64(s, 2147487915);
    CHECK_EQ_I64(e, 2147491855);
}

AVP_TEST(split_and_demangle) {
    /* symbol, plain, rest (NULL: None), demangled name, class (NULL: None) */
    static const char *const CASES[][5] = {
        {"fn_800067CC__8unk_RampFf", "fn_800067CC", "8unk_RampFf", "fn_800067CC", "unk_Ramp"},
        {"__ct__8unk_RampFv", "__ct", "8unk_RampFv", "unk_Ramp", "unk_Ramp"},
        {"__sinit_\\PadRumble_cpp", "__sinit_\\PadRumble_cpp", NULL, "__sinit_\\PadRumble_cpp", NULL},
        {"foo", "foo", NULL, "foo", NULL},
        {"a__", "a", "", "a__", NULL},
        {"__dt__Q21N1CFv", "__dt", "Q21N1CFv", "~C", "C"},
        {"x__FP", "x", "FP", "x__FP", NULL},
        {"operator==", "operator==", NULL, "operator==", NULL},
        {"__as__1AFRC1A", "__as", "1AFRC1A", "__as", "A"},
        {"fn_80001234", "fn_80001234", NULL, "fn_80001234", NULL},
        {"@123", "@123", NULL, "@123", NULL},
        {"f__Q", "f", "Q", "f__Q", NULL},
        {"__vt__8unk_Ramp", "__vt", "8unk_Ramp", "__vt", "unk_Ramp"},
    };
    AvpArena *a = avp_arena();
    for (size_t i = 0; i < sizeof CASES / sizeof *CASES; i++) {
        AvpStr rest;
        AvpStr plain = draft_split_mangled(avp_str(CASES[i][0]), &rest);
        CHECK_CZ(avp_cz(a, plain), CASES[i][1]);
        if (CASES[i][2]) {
            REQUIRE(rest.p != NULL);
            CHECK_CZ(avp_cz(a, rest), CASES[i][2]);
        } else {
            CHECK_MSG(rest.p == NULL, CASES[i][0]);
        }
        const char *cls;
        CHECK_CZ(draft_demangled_parts(a, CASES[i][0], &cls), CASES[i][3]);
        CHECK_CZ(cls ? cls : "None", CASES[i][4] ? CASES[i][4] : "None");
    }
}

AVP_TEST(c_escape) {
    CHECK_CZ(draft_c_escape(avp_arena(), avp_str("a\\b\"c\nd\te\x01\x7f\xC3\xA9\xE3\x81\x82")),
              "a\\\\b\\\"c\\nd\\te\\x01\\x7F\\xE9\\x3042");
}

static AvpStr S(const char *cz) { return avp_str(cz); }

static DraftSubst *sample_subst(void) {
    AvpArena *a = avp_arena();
    DraftSubst *s = draft_subst_new(a);
    draft_subst_add(s, s->idents, "ident", S("fn_1"), S("fn_2"), S("why1"));
    draft_subst_add(s, s->idents, "ident", S("fn_1"), S("fn_3"), S("why2"));
    draft_subst_add(s, s->idents, "ident", S("fn_1"), S("fn_2"), S("why3"));
    draft_subst_add(s, s->classes, "class", S("unk_A"), S("unk_Bee"), S("cls"));
    draft_subst_add(s, s->strings, "string", S("Hello\n"), S("World\""), S("str"));
    avp_imap_put(s->addresses, 0x80001000, NULL)->i = 0x80002000;
    draft_subst_unmapped(s, "offset", S("lbl_X"), S("same symbol"));
    draft_smap_set(a, s->renames, S("fn_80003000"), S("x__8unk_BeeFv"));
    return s;
}

AVP_TEST(subst) {
    AvpArena *a = avp_arena();
    DraftSubst *s = sample_subst();
    CHECK_CZ(dumps(draft_subst_json(a, s)),
              "{\"idents\": {\"fn_1\": \"fn_2\"}, \"classes\": {\"unk_A\": \"unk_Bee\"}, \"strings\": {\"Hello\\n\": "
              "\"World\\\"\"}, \"addresses\": {\"0x80001000\": \"0x80002000\"}, \"renames\": {\"fn_80003000\": "
              "\"x__8unk_BeeFv\"}, \"evidence\": [[\"ident\", \"fn_1\", \"fn_2\", \"why1\"], [\"class\", \"unk_A\", "
              "\"unk_Bee\", \"cls\"], [\"string\", \"Hello\\n\", \"World\\\"\", \"str\"]], \"unmapped\": [[\"offset\", "
              "\"lbl_X\", \"same symbol\"]], \"conflicts\": [\"ident fn_1 -> fn_2 and fn_3\"]}");
    CHECK_CZ(jstr(draft_subst_tsv(a, s)),
              "\"kind\\told\\tnew\\tevidence\\nident\\tfn_1\\tfn_2\\twhy1\\nclass\\tunk_A\\tunk_Bee\\tcls\\nstring\\t"
              "Hello\\n\\tWorld\\\"\\tstr\\nunmapped-offset\\tlbl_X\\t\\tsame symbol\\nconflict\\tident fn_1 -> fn_2 "
              "and fn_3\\t\\t\\n\"");
    CHECK_CZ(draft_remangle(a, S("fn_1__5unk_AFP5unk_A15unk_A"), s->classes, s->idents),
              "fn_2__7unk_BeeFP7unk_Bee15unk_A");
    CHECK_CZ(draft_remangle(a, S("fn_1"), s->classes, s->idents), "fn_2");
    AvpMap *cl = avp_map_new(), *none = avp_map_new();
    draft_smap_set(a, cl, S("unk_A"), S("\\g<0>X")); /* the replacement is a template, as re.sub's */
    CHECK_CZ(draft_remangle(a, S("q__5unk_AF"), cl, none), "q__65unk_AXF");
}

AVP_TEST(substitute) {
    DraftSubst *s = sample_subst();
    AvpStr text = S("void unk_A::fn_1() { puts(\"Hello\\n\"); x = 0x80001000; y = 0x80001001; c = 'a'; UNK_A_SIZE; "
                    "GAME_UNK_A_H; unk_Ax; s=\"World\"; }");
    CHECK_CZ(avp_cz(avp_arena(), draft_substitute(avp_arena(), text, s)),
              "void unk_Bee::fn_2() { puts(\"World\\\"\"); x = 0x80002000; y = 0x80001001; c = 'a'; UNK_BEE_SIZE; "
              "GAME_UNK_BEE_H; unk_Ax; s=\"World\"; }");
}

AVP_TEST(own_address_name) {
    CHECK(draft_own_address_name(S("fn_80001000__Fv"), 0x80001000));
    CHECK(!draft_own_address_name(S("fn_80001000"), 0x80001001));
    CHECK(!draft_own_address_name(S("fn_80001000"), DRAFT_NONE));
    CHECK(!draft_own_address_name(S("fn_8000100a"), 0x8000100a));
}

AVP_TEST(screen) {
    DraftScreenHits h = draft_screen(S("int param_1;\nuVar3 = local_1c;\nreading aid\n(code *)x; CONCAT44 (a); "
                                       "undefined4 b; in_r3 FUN_80001234"));
    AvpArena *a = avp_arena();
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < h.n; i++) {
        AvpJson *x = avp_json_arr(a);
        avp_json_arr_push(a, x, avp_json_int(a, h.v[i].line));
        avp_json_arr_push(a, x, avp_json_strz(a, h.v[i].label));
        avp_json_arr_push(a, x, avp_json_str(a, h.v[i].text));
        avp_json_arr_push(a, arr, x);
    }
    CHECK_CZ(dumps(arr),
              "[[1, \"Ghidra parameter name\", \"param_1\"], [2, \"Ghidra variable name\", \"uVar3\"], [2, \"Ghidra "
              "stack-slot name\", \"local_1c\"], [3, \"the listing's reading-aid notice\", \"reading aid\"], [4, "
              "\"Ghidra address label\", \"FUN_80001234\"], [4, \"Ghidra type\", \"undefined4\"], [4, \"Ghidra "
              "register artefact\", \"in_r3\"], [4, \"Ghidra pseudo-op\", \"CONCAT44 (\"], [4, \"Ghidra code pointer "
              "cast\", \"(code *)\"]]");
}

AVP_TEST(compiled_name_for) {
    static const char *const names[] = {"fn_1", "fn_1__8unk_RampFv", "__ct__8unk_RampFv", "g__Fv", "h__3FooFi"};
    AvpArena *a = avp_arena();
    const char *r;
    CHECK_CZ(draft_compiled_name_for(a, names, 5, "fn_1", NULL), "fn_1");
    CHECK_CZ(draft_compiled_name_for(a, names, 5, "fn_1", "unk_Ramp"), "fn_1__8unk_RampFv");
    CHECK_CZ(draft_compiled_name_for(a, names, 5, "unk_Ramp", "unk_Ramp"), "__ct__8unk_RampFv");
    CHECK_CZ(draft_compiled_name_for(a, names, 5, "g", NULL), "g__Fv");
    CHECK_CZ(draft_compiled_name_for(a, names, 5, "g", ""), "g__Fv");
    r = draft_compiled_name_for(a, names, 5, "h", "oo");
    CHECK_CZ(r ? r : "None", "h__3FooFi");
    CHECK(draft_compiled_name_for(a, names, 5, "zz", NULL) == NULL);
}

AVP_TEST(rename_explains) {
    AvpArena *a = avp_arena();
    AvpMap *ren = avp_map_new();
    draft_smap_set(a, ren, S("fn_80001000"), S("Foo__3BarFv"));
    draft_smap_set(a, ren, S("lbl_8000"), S("lbl_new"));
    draft_smap_set(a, ren, S("fn_8000100"), S("short"));
    AvpStr none = {NULL, 0};
    CHECK(draft_rename_explains(a, S("bl fn_80001000"), S("bl Foo__3BarFv"), none, ren));
    CHECK(!draft_rename_explains(a, S("bl fn_80001000x"), S("bl Foo__3BarFvx"), none, ren));
    CHECK(draft_rename_explains(a, S("lis r3, lbl_8000@ha"), S("lis r3, lbl_new@ha"),
                                S("points to Foo__3BarFv+0x8 (0x80001008), retail 0x80001008 is fn_80001000"), ren));
    CHECK(!draft_rename_explains(a, S("lis r3, lbl_8000@ha"), S("lis r3, lbl_new@ha"),
                                 S("points to X+0x8 (0x80001008), retail 0x80001008 is Y"), ren));
    CHECK(draft_rename_explains(a, none, S(""), none, ren));
    CHECK(!draft_rename_explains(a, S("a"), S("a"), none, avp_map_new()));
}

static ShsmAsmRow row(i64 address, const char *sym, i64 off, const char *reloc) {
    ShsmAsmRow r = {0};
    r.address = address;
    r.sym = sym ? avp_str(sym) : (AvpStr){NULL, 0};
    r.sym_off = off;
    r.reloc = reloc;
    return r;
}

AVP_TEST(pair_relocations) {
    ShsmAsmRow tw[] = {row(0x100, NULL, SHSM_NONE, NULL), row(0x104, "fn_80001000", SHSM_NONE, "rel24"),
                       row(0x108, "lbl_80002000", 4, "ha"), row(0x10C, NULL, SHSM_NONE, "l"),
                       row(0x110, "a", SHSM_NONE, NULL)};
    ShsmAsmRow tg[] = {row(0x200, NULL, SHSM_NONE, NULL), row(0x204, "fn_80003000", SHSM_NONE, "rel24"),
                       row(0x208, "lbl_80004000", 8, "ha"), row(0x20C, "x", SHSM_NONE, "l")};
    DraftPairs pairs = {0};
    AvpStrVec problems = {0};
    draft_pair_relocations(avp_arena(), tw, 5, tg, 4, &pairs, &problems);
    REQUIRE(pairs.n == 2);
    CHECK_CZ(avp_cz(avp_arena(), pairs.v[0].tsym), "fn_80001000");
    CHECK_EQ_I64(pairs.v[0].toff, 0);
    CHECK_CZ(avp_cz(avp_arena(), pairs.v[1].gsym), "lbl_80004000");
    CHECK_EQ_I64(pairs.v[1].toff, 4);
    CHECK_EQ_I64(pairs.v[1].goff, 8);
    REQUIRE(problems.n == 2);
    CHECK_CZ(avp_cz(avp_arena(), problems.v[0]), "row counts differ (5, 4)");
    CHECK_CZ(avp_cz(avp_arena(), problems.v[1]), "relocation on one side only at 0x0000020C");
}

static DraftInfo info(void *ctx, AvpStr sym, i64 off) {
    (void)ctx;
    (void)off;
    static const char *const STR[][2] = {{"str_a", "abc"}, {"str_b", "xyz"}, {"str_c", "abc"}};
    static const i64 ADDR[] = {0x80005000, 0x80006000, 0x80007000};
    for (int i = 0; i < 3; i++)
        if (avp_str_eqz(sym, STR[i][0])) return (DraftInfo){ADDR[i], avp_str(STR[i][1])};
    AvpStr rest;
    i64 addr;
    if (!draft_addr_name(draft_split_mangled(sym, &rest), &addr)) addr = 0x80009999;
    return (DraftInfo){addr, {NULL, 0}};
}

AVP_TEST(build_subst) {
    AvpArena *a = avp_arena();
    static const char *const P[][2] = {
        {"fn_80001000__8unk_AFv", "fn_80003000__8unk_BFv"}, {"str_a", "str_b"}, {"str_a", "str_c"},
        {"lbl_80002000", "lbl_80002000"}, {"Real__Fv", "Other__Fv"}, {"lbl_80002004", "retailname"},
        {"helper__8unk_AFv", "fn_80004000"}, {"fn_80001100", "fn_80003100__Q21N1BFi"}, {"same", "same"},
        {"named__Fv", "fn_8000AAAA"}};
    DraftPairs pairs = {0};
    for (size_t i = 0; i < 10; i++)
        avp_vec_push(&pairs, ((DraftPair){S(P[i][0]), i == 3 ? 4 : 0, S(P[i][1]), i == 3 ? 8 : 0}));
    DraftSubst *s = draft_build_subst(a, &pairs, info, NULL, S("fn_80001200__8unk_AFv"), 0x80001200,
                                      S("fn_80003200__8unk_BFv"), 0x80003200, "unk_A", "unk_C", "vtable evidence");
    CHECK_CZ(dumps(draft_subst_json(a, s)),
              "{\"idents\": {\"fn_80001200__8unk_AFv\": \"fn_80003200__8unk_BFv\", \"fn_80001000__8unk_AFv\": "
              "\"fn_80003000__8unk_BFv\", \"lbl_80002004\": \"retailname\", \"fn_80001100\": \"fn_80003100\"}, "
              "\"classes\": {\"unk_A\": \"unk_C\"}, \"strings\": {\"abc\": \"xyz\"}, \"addresses\": {\"0x80001200\": "
              "\"0x80003200\"}, \"renames\": {\"fn_80004000\": \"helper__8unk_AFv\", \"fn_8000AAAA\": \"named__Fv\"}, "
              "\"evidence\": [[\"class\", \"unk_A\", \"unk_C\", \"vtable evidence\"], [\"ident\", "
              "\"fn_80001200__8unk_AFv\", \"fn_80003200__8unk_BFv\", \"the function itself: fn_80001200__8unk_AFv -> "
              "fn_80003200__8unk_BFv\"], [\"ident\", \"fn_80001000__8unk_AFv\", \"fn_80003000__8unk_BFv\", "
              "\"relocation: fn_80001000__8unk_AFv -> fn_80003000__8unk_BFv\"], [\"string\", \"abc\", \"xyz\", "
              "\"str_a+0 -> str_b+0\"], [\"ident\", \"lbl_80002004\", \"retailname\", \"relocation: lbl_80002004 -> "
              "retailname\"], [\"ident\", \"fn_80001100\", \"fn_80003100\", \"relocation: fn_80001100 -> "
              "fn_80003100__Q21N1BFi\"]], \"unmapped\": [[\"offset\", \"lbl_80002000\", \"same symbol, offsets 4 and "
              "8\"], [\"name\", \"Real__Fv\", \"named Other in the target (Other__Fv): source keeps Real\"]], "
              "\"conflicts\": []}");
    DraftPairs empty = {0};
    DraftSubst *s3 = draft_build_subst(a, &empty, info, NULL, S("f"), 1, S("g"), 2, "unk_A", "unk_B", NULL);
    CHECK_CZ(dumps(draft_subst_json(a, s3)),
              "{\"idents\": {\"f\": \"g\"}, \"classes\": {\"unk_A\": \"unk_B\"}, \"strings\": {}, \"addresses\": "
              "{\"0x00000001\": \"0x00000002\"}, \"renames\": {}, \"evidence\": [[\"class\", \"unk_A\", \"unk_B\", "
              "null], [\"ident\", \"f\", \"g\", \"the function itself: f -> g (the target's own name)\"]], "
              "\"unmapped\": [], \"conflicts\": []}");
    CHECK_CZ(jstr(draft_subst_tsv(a, s3)),
              "\"kind\\told\\tnew\\tevidence\\nclass\\tunk_A\\tunk_B\\tNone\\nident\\tf\\tg\\tthe function itself: f -> "
              "g (the target's own name)\\n\"");
}

AVP_TEST(clone_header) {
    AvpArena *a = avp_arena();
    DraftSubst *s = sample_subst();
    AvpStr hdr = S("#include <game/Base.h>\n#include <game/Other.h>\n\n/* the doc */\nclass unk_A : public Base {\n"
                   "public:\n    void fn_1();\n    UNK_A_FIELD;\n};\n\nstruct Helper {\n    int z;\n};\n\n"
                   "extern int lbl_80001000;\nextern int fn_1;\nextern float lbl_8000ABCD;\n");
    AvpSet *used = avp_set_new();
    avp_set_add(used, S("lbl_8000ABCD"));
    AvpStrVec dropped = {0};
    CHECK_CZ(jstr(draft_clone_header(a, hdr, "game/unk_A.h", "unk_A", "unk_Bee", s, used, &dropped)),
              "\"#ifndef GAME_UNK_BEE_H\\n#define GAME_UNK_BEE_H\\n\\n#include <game/Base.h>\\n#include "
              "<game/Other.h>\\n#include <game/unk_A.h>\\n\\n/*\\n * Same layout and members as unk_A (twin draft). "
              "Member names invented.\\n */\\nclass unk_Bee : public Base {\\npublic:\\n    void fn_2();\\n    "
              "UNK_BEE_FIELD;\\n};\\n\\nextern float lbl_8000ABCD;\\n\\n#endif\\n\"");
    REQUIRE(dropped.n == 1);
    CHECK_CZ(avp_cz(a, dropped.v[0]), "lbl_80001000");
    AvpStrVec d2 = {0};
    CHECK_CZ(jstr(draft_clone_header(a, S("int x;\n"), NULL, "unk_A", NULL, s, avp_set_new(), &d2)),
              "\"#ifndef GAME__H\\n#define GAME__H\\n\\n\\n\\n#endif\\n\"");
    CHECK_EQ_I64((i64)d2.n, 0);
}

AVP_TEST(target_namespaces_and_wrapped_class_block) {
    AvpArena *a = avp_arena();
    AvpStrVec ns = draft_namespaces_of(a, "__dt__Q24FMOD19ChannelRealManual3DFv");
    REQUIRE(ns.n == 1);
    CHECK_CZ(avp_cz(a, ns.v[0]), "FMOD");
    CHECK_EQ_I64((i64)draft_namespaces_of(a, "__dt__10CFooTestFv").n, 0);   /* a global class */
    CHECK_EQ_I64((i64)draft_namespaces_of(a, "fn_80010000").n, 0);          /* not a mangled name */
    AvpStrVec two = draft_namespaces_of(a, "f__Q35Outer5Inner3BazFv");
    REQUIRE(two.n == 2);
    CHECK_CZ(avp_cz(a, two.v[1]), "Inner");
    AvpStr hdr = S("#include <a.h>\n\n/* doc */\nclass Baz : public Base {\npublic:\n    void f();\n};\n\nextern int x;\n");
    AvpStr w = draft_wrap_class_block(a, hdr, "Baz", &ns);
    CHECK_CZ(avp_cz(a, w), "#include <a.h>\n\nnamespace FMOD {\n\n/* doc */\nclass Baz : public Base {\npublic:\n    void f();\n};\n"
                           "\n} // namespace FMOD\n\n\nextern int x;\n");
    CHECK_CZ(avp_cz(a, draft_wrap_class_block(a, hdr, "Missing", &ns)), avp_cz(a, hdr)); /* no such class: unchanged */
}

AVP_TEST(class_defines_member_inline) {
    AvpArena *a = avp_arena();
    AvpStr h = AVP_S("class Other {\n    virtual ~Other() {}\n};\n"
                     "class ChannelSoftware : public ChannelRealManual3D {\n"
                     "public:\n"
                     "    ChannelSoftware();\n"
                     "    virtual             ~ChannelSoftware() {}\n"
                     "    FMOD_RESULT         getPosition(unsigned int *position, FMOD_TIMEUNIT postype) const;\n"
                     "    int                 isPlaying() const { return 1; }\n"
                     "    virtual void        stop();\n"
                     "};\n");
    CHECK(draft_class_defines_member(a, h, "ChannelSoftware", "~ChannelSoftware"));
    CHECK(draft_class_defines_member(a, h, "ChannelSoftware", "isPlaying"));
    CHECK(!draft_class_defines_member(a, h, "ChannelSoftware", "ChannelSoftware")); /* declared, not defined */
    CHECK(!draft_class_defines_member(a, h, "ChannelSoftware", "getPosition"));
    CHECK(!draft_class_defines_member(a, h, "ChannelSoftware", "stop"));
    CHECK(!draft_class_defines_member(a, h, "Missing", "stop"));
    CHECK(draft_class_defines_member(a, h, "Other", "~Other")); /* the block of that class only */
    CHECK(!draft_class_defines_member(a, h, "Other", "isPlaying"));
}

AVP_TEST(declare_member_cuts_the_in_class_body) {
    AvpArena *a = avp_arena();
    AvpStr h = AVP_S("class Segment : public ChannelReal {\n"
                     "    Segment();\n"
                     "    virtual             ~Segment() {}\n"
                     "    int                 get() const { return mX; }\n"
                     "};\n");
    CHECK_CZ(avp_cz(a, draft_declare_member(a, h, "~Segment")),
             "class Segment : public ChannelReal {\n    Segment();\n    virtual             ~Segment();\n"
             "    int                 get() const { return mX; }\n};\n");
    CHECK_CZ(avp_cz(a, draft_declare_member(a, h, "get")),
             "class Segment : public ChannelReal {\n    Segment();\n    virtual             ~Segment() {}\n"
             "    int                 get() const;\n};\n");
    CHECK_CZ(avp_cz(a, draft_declare_member(a, h, "nothing")), avp_cz(a, h)); /* not found: unchanged */
}

AVP_TEST_MAIN("draft")
