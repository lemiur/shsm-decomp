/* draft_lex.c: levers' tokenizer, brackets and definition scanner, and draft.py's source scanning. Expected values
 * are the Python's (levers.tokenize, draft.scan_source ... run on the same texts; the JSON strings are its
 * json.dumps output).
 *
 * Cross-check hook: with DRAFT_DUMP_LIST=<file of paths> and DRAFT_DUMP_OUT=<file>, the test `dump_tree` writes one
 * JSON line per path (tokens, scan_source, top_level_statements, marked_functions, includes_of, class_block of every
 * class, screen) in the form a Python script writes for the same files, to compare the two over the whole tree. */
#include "avp/test.h"
#include "draft.h"
#include "shsm_re.h"

#include <stdlib.h>
#include <string.h>

static AvpStr dumps(AvpJson *v) {
    AvpJsonDumpOpts o = {0};
    return avp_json_dumps(avp_arena(), v, &o);
}

static AvpJson *defs_json(AvpArena *a, const DraftDefs *defs) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < defs->n; i++) avp_json_arr_push(a, arr, draft_def_json(a, &defs->v[i]));
    return arr;
}

static AvpJson *toks_json(AvpArena *a, const DraftToks *t) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < t->n; i++) {
        AvpJson *x = avp_json_arr(a);
        avp_json_arr_push(a, x, avp_json_strz(a, draft_tok_kind_name(t->v[i].k)));
        avp_json_arr_push(a, x, avp_json_str(a, t->v[i].t));
        avp_json_arr_push(a, x, avp_json_int(a, t->v[i].s));
        avp_json_arr_push(a, x, avp_json_int(a, t->v[i].e));
        avp_json_arr_push(a, arr, x);
    }
    return arr;
}

static AvpJson *stmts_json(AvpArena *a, const DraftStmts *s) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < s->n; i++) {
        AvpJson *x = avp_json_arr(a);
        avp_json_arr_push(a, x, avp_json_int(a, s->v[i].start));
        avp_json_arr_push(a, x, avp_json_int(a, s->v[i].end));
        avp_json_arr_push(a, x, avp_json_str(a, s->v[i].text));
        avp_json_arr_push(a, arr, x);
    }
    return arr;
}

static AvpJson *opt_def(AvpArena *a, const DraftDef *d) { return d ? draft_def_json(a, d) : avp_json_null(); }

static const char T1[] = "#include <a.h>\n/* c */ int x = 0x1F + .5e+3; // tail\r\n  s = \"a\\\"b\"; c='\\n'; a->*b <<= 3 "
                         "... ::f\n#define M(x) \\\n  x\n$id 1.f 07UL";

AVP_TEST(tokenize) {
    DraftToks t = draft_tokenize(avp_str(T1));
    CHECK_CZ(avp_cz(avp_arena(), dumps(toks_json(avp_arena(), &t))),
              "[[\"pp\", \"#include <a.h>\", 0, 14], [\"comment\", \"/* c */\", 15, 22], [\"id\", \"int\", 23, 26], "
              "[\"id\", \"x\", 27, 28], [\"op\", \"=\", 29, 30], [\"num\", \"0x1F\", 31, 35], [\"op\", \"+\", 36, 37], "
              "[\"num\", \".5e+3\", 38, 43], [\"op\", \";\", 43, 44], [\"comment\", \"// tail\", 45, 52], "
              "[\"id\", \"s\", 56, 57], [\"op\", \"=\", 58, 59], [\"str\", \"\\\"a\\\\\\\"b\\\"\", 60, 66], "
              "[\"op\", \";\", 66, 67], [\"id\", \"c\", 68, 69], [\"op\", \"=\", 69, 70], [\"chr\", \"'\\\\n'\", 70, 74], "
              "[\"op\", \";\", 74, 75], [\"id\", \"a\", 76, 77], [\"op\", \"->*\", 77, 80], [\"id\", \"b\", 80, 81], "
              "[\"op\", \"<<=\", 82, 85], [\"num\", \"3\", 86, 87], [\"op\", \"...\", 88, 91], [\"op\", \"::\", 92, 94], "
              "[\"id\", \"f\", 94, 95], [\"pp\", \"#define M(x) \\\\\\n  x\", 96, 114], [\"id\", \"$id\", 115, 118], "
              "[\"num\", \"1.f\", 119, 122], [\"num\", \"07UL\", 123, 127]]");
    DraftToks c = draft_code_tokens(&t);
    CHECK_EQ_I64((i64)c.n, (i64)t.n - 4);
}

AVP_TEST(tokenize_non_ascii) {
    /* one token per code point outside identifiers; offsets are bytes */
    DraftToks t = draft_tokenize(avp_str("a\xC3\xA9" "b \xD9\xA3x"));
    REQUIRE(t.n == 4);
    CHECK_CZ(draft_tok_kind_name(t.v[1].k), "op");
    CHECK_EQ_I64(t.v[1].s, 1);
    CHECK_EQ_I64(t.v[1].e, 3);
    CHECK_CZ(draft_tok_kind_name(t.v[3].k), "num"); /* ARABIC-INDIC DIGIT THREE: \d, then \w */
    CHECK_EQ_I64(t.v[3].e, 8);
}

static const char *bracket_error(const char *text) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        DraftToks toks = draft_tokenize(avp_str(text));
        DraftToks code = draft_code_tokens(&toks);
        draft_match_brackets(avp_arena(), &code);
        avp_try_end(&t);
        return "";
    }
    CHECK_EQ_I64(t.code, DRAFT_EXC_PARSE);
    return avp_strdup(avp_arena(), t.msg);
}

AVP_TEST(match_brackets) {
    CHECK_CZ(bracket_error("f(]"), "unbalanced ']' at offset 2");
    CHECK_CZ(bracket_error("{("), "unclosed '('");
    CHECK_CZ(bracket_error("a)"), "unbalanced ')' at offset 1");
    CHECK_CZ(bracket_error("f(a[1]) { }"), "");
}

static const char T2[] =
    "// banner one\n"
    "// 0x80001000\n"
    "void A::f(int a, int b) { if (a) { b++; } }\n"
    "\n"
    "namespace N {\n"
    "/* doc */\n"
    "class C : public B {\n"
    "public:\n"
    "    C() : x(0) {}\n"
    "    ~C() {}\n"
    "    int operator()(int) const { return 1; }\n"
    "    bool operator==(const C& o) const { return true; }\n"
    "    static void* Make(void) { return 0; }\n"
    "    int x;\n"
    "};\n"
    "}\n"
    "\n"
    "extern \"C\" {\n"
    "int g(void) { return 0; } // 0x80002000\n"
    "}\n"
    "\n"
    "template <class T> struct S<T>::Inner;\n"
    "template <class T> void S<T>::Inner::h(T t) { }\n"
    "METHODDEF(void *) j_alloc(int n) { return 0; }\n"
    "int arr[3] = {1, 2, 3};\n"
    "enum E { E_A, E_B };\n"
    "struct Q { int a; } q;\n";

AVP_TEST(scan_source) {
    AvpArena *a = avp_arena();
    DraftDefs defs = draft_scan_source(a, avp_str(T2));
    CHECK_CZ(avp_cz(a, dumps(defs_json(a, &defs))),
              "[{\"name\": \"f\", \"cls\": \"A\", \"start\": 28, \"end\": 71, \"banner\": 14, \"address\": 2147487744}, "
              "{\"name\": \"C\", \"cls\": \"C\", \"start\": 130, \"end\": 143, \"banner\": 126, \"address\": null}, "
              "{\"name\": \"~C\", \"cls\": \"C\", \"start\": 148, \"end\": 155, \"banner\": 144, \"address\": null}, "
              "{\"name\": \"operator()\", \"cls\": \"C\", \"start\": 160, \"end\": 199, \"banner\": 156, \"address\": null}, "
              "{\"name\": \"Make\", \"cls\": \"C\", \"start\": 259, \"end\": 296, \"banner\": 255, \"address\": null}, "
              "{\"name\": \"g\", \"cls\": null, \"start\": 327, \"end\": 352, \"banner\": 327, \"address\": 2147491840}, "
              "{\"name\": \"h\", \"cls\": \"Inner\", \"start\": 409, \"end\": 456, \"banner\": 409, \"address\": null}, "
              "{\"name\": \"j_alloc\", \"cls\": null, \"start\": 457, \"end\": 503, \"banner\": 457, \"address\": null}]");
    DraftStmts st = draft_top_level_statements(a, avp_str(T2), &defs);
    CHECK_CZ(avp_cz(a, dumps(stmts_json(a, &st))),
              "[[239, 252, \"return true;\"], [296, 307, \"int x;\"], [309, 310, \";\"], "
              "[368, 408, \"template <class T> struct S<T>::Inner;\"], [526, 527, \";\"], [547, 548, \";\"], "
              "[559, 566, \"int a;\"], [568, 571, \"q;\"]]");
    DraftMarks mk = draft_marked_functions(a, avp_str(T2));
    REQUIRE(mk.n == 2);
    CHECK_EQ_I64(mk.v[0].address, 0x80001000);
    CHECK_CZ(mk.v[0].name, "f");
    CHECK_CZ(mk.v[0].cls, "A");
    CHECK(mk.v[1].cls == NULL);
}

AVP_TEST(scan_definitions_fields) {
    AvpArena *a = avp_arena();
    DraftToks toks = draft_tokenize(avp_str(T2));
    DraftToks code = draft_code_tokens(&toks);
    i64 *m = draft_match_brackets(a, &code);
    DraftFnDefs d = draft_scan_definitions(a, &code, m);
    REQUIRE(d.n == 8);
    CHECK_EQ_I64(d.v[0].nparams, 2);
    CHECK(d.v[0].is_void);
    CHECK_EQ_I64((i64)d.v[0].quals.n, 1);
    CHECK_EQ_I64(d.v[4].nparams, 0); /* Make(void) */
    CHECK(!d.v[4].is_void);          /* void* */
    CHECK(d.v[1].is_void);           /* constructor */
    CHECK_EQ_I64((i64)d.v[6].quals.n, 2); /* S<T>::Inner::h */
    CHECK_CZ(avp_cz(a, d.v[6].quals.v[0]), "S");
}

AVP_TEST(find_def) {
    AvpArena *a = avp_arena();
    AvpStr t2 = avp_str(T2);
    CHECK_CZ(avp_cz(a, dumps(opt_def(a, draft_find_def(a, t2, "f__1AFii", DRAFT_NONE)))),
              "{\"name\": \"f\", \"cls\": \"A\", \"start\": 28, \"end\": 71, \"banner\": 14, \"address\": 2147487744}");
    CHECK_CZ(avp_cz(a, dumps(opt_def(a, draft_find_def(a, t2, "g", 0x80002000)))),
              "{\"name\": \"g\", \"cls\": null, \"start\": 327, \"end\": 352, \"banner\": 327, \"address\": 2147491840}");
    CHECK_CZ(avp_cz(a, dumps(opt_def(a, draft_find_def(a, t2, "__dt__Q21N1CFv", DRAFT_NONE)))),
              "{\"name\": \"~C\", \"cls\": \"C\", \"start\": 148, \"end\": 155, \"banner\": 144, \"address\": null}");
    CHECK(draft_find_def(a, t2, "nothing", DRAFT_NONE) == NULL);
}

AVP_TEST(class_block) {
    AvpArena *a = avp_arena();
    i64 s = 0, e = 0;
    CHECK(draft_class_block(a, avp_str(T2), "C", &s, &e));
    CHECK_EQ_I64(s, 87);
    CHECK_EQ_I64(e, 310);
    CHECK(draft_class_block(a, avp_str(T2), "Q", &s, &e));
    CHECK_EQ_I64(s, 549);
    CHECK_EQ_I64(e, 568);
    CHECK(!draft_class_block(a, avp_str(T2), "Inner", &s, &e));
}

AVP_TEST(includes_of) {
    AvpStrVec v = draft_includes_of(avp_str("  #  include <game/PadRumble.h>\n#include \"x/y.h\"\n# include<z>\n"));
    REQUIRE(v.n == 3);
    CHECK_CZ(avp_cz(avp_arena(), v.v[0]), "game/PadRumble.h");
    CHECK_CZ(avp_cz(avp_arena(), v.v[1]), "x/y.h");
    CHECK_CZ(avp_cz(avp_arena(), v.v[2]), "z");
}

AVP_TEST(extern_of) {
    AvpArena *a = avp_arena();
    CHECK_CZ(draft_extern_of(a, AVP_S("u32 lbl_80001234 = f( \"a\" );"), "lbl_80001234"), "extern u32 lbl_80001234;");
    CHECK_CZ(draft_extern_of(a, AVP_S("static int lbl_X[4] = {0};"), "lbl_X"), "static int lbl_X[4];");
    CHECK_CZ(draft_extern_of(a, AVP_S("extern float fn_1(void);"), "fn_1"), "extern float fn_1");
    CHECK(draft_extern_of(a, AVP_S("  int a;; "), "b") == NULL);
    CHECK(draft_extern_of(a, AVP_S("const char* lbl_Y[2][3];"), "lbl_Y") == NULL);
}

AVP_TEST(class_header) {
    AvpArena *a = avp_arena();
    ShsmRepo *r = shsm_repo_find(NULL);
    AvpStr src = AVP_S("#include <game/Display.h>\n#include <game/PadRumble.h>\n");
    CHECK_CZ(draft_class_header(a, r->include_dir, src, "unk_Ramp", "src/game/PadRumble.cpp"), "game/PadRumble.h");
    /* not named after the source: the first include declaring the class */
    const char *h = draft_class_header(a, r->include_dir, src, "unk_Ramp", "src/game/Other.cpp");
    CHECK_CZ(h ? h : "None", "game/PadRumble.h");
    CHECK(draft_class_header(a, r->include_dir, src, NULL, "src/game/Other.cpp") == NULL);
}

AVP_TEST(pad_rumble) {
    AvpArena *a = avp_arena();
    AvpStr text;
    REQUIRE(avp_read_text(a, "src/game/PadRumble.cpp", AVP_TM_UTF8, &text));
    DraftDefs defs = draft_scan_source(a, text);
    CHECK_CZ(avp_cz(a, dumps(defs_json(a, &defs))),
              "[{\"name\": \"unk_Tweaks\", \"cls\": \"unk_Tweaks\", \"start\": 5477, \"end\": 7364, \"banner\": 5247, "
              "\"address\": null}, {\"name\": \"fn_800067A0\", \"cls\": null, \"start\": 7467, \"end\": 7545, \"banner\": "
              "7365, \"address\": 2147510176}, {\"name\": \"fn_800067B4\", \"cls\": null, \"start\": 7621, \"end\": 7676, "
              "\"banner\": 7546, \"address\": 2147510196}, {\"name\": \"fn_800067C0\", \"cls\": null, \"start\": 7752, "
              "\"end\": 7810, \"banner\": 7677, \"address\": 2147510208}, {\"name\": \"fn_800067CC\", \"cls\": "
              "\"unk_Ramp\", \"start\": 7970, \"end\": 8371, \"banner\": 7811, \"address\": 2147510220}, {\"name\": "
              "\"fn_80006860\", \"cls\": \"unk_Ramp\", \"start\": 8531, \"end\": 8955, \"banner\": 8372, \"address\": "
              "2147510368}, {\"name\": \"fn_80006954\", \"cls\": \"unk_Ramp\", \"start\": 9074, \"end\": 9353, "
              "\"banner\": 8956, \"address\": 2147510612}, {\"name\": \"fn_800069D8\", \"cls\": \"unk_Ramp\", \"start\": "
              "9480, \"end\": 9718, \"banner\": 9354, \"address\": 2147510744}, {\"name\": \"fn_80006A18\", \"cls\": "
              "null, \"start\": 9794, \"end\": 9848, \"banner\": 9719, \"address\": 2147510808}, {\"name\": "
              "\"fn_80006A20\", \"cls\": null, \"start\": 9924, \"end\": 9971, \"banner\": 9849, \"address\": "
              "2147510816}]");
}

/* ---- the whole-tree cross-check hook ---- */

static const char *const KINDS[] = {"id", "num", "str", "chr", "op", "comment", "pp"};

static i64 toks_digest(const DraftToks *t) {
    const i64 P = 1000000007;
    i64 sum = 0;
    for (size_t i = 0; i < t->n; i++) {
        i64 k = (i64)t->v[i].k;
        (void)KINDS;
        i64 term = (t->v[i].s * 7 + t->v[i].e * 13 + (i64)avp_u8_len(t->v[i].t) + k) % P;
        sum = (sum + (i64)((i + 1) % (size_t)P) * term % P) % P;
    }
    return sum;
}

static AvpJson *dump_one(AvpArena *a, const char *path, AvpStr text) {
    AvpJson *rec = avp_json_obj(a);
    avp_json_set(a, rec, AVP_S("path"), avp_json_strz(a, path));
    DraftToks toks = draft_tokenize(text);
    avp_json_set(a, rec, AVP_S("ntok"), avp_json_int(a, (i64)toks.n));
    avp_json_set(a, rec, AVP_S("toks"), avp_json_int(a, toks_digest(&toks)));
    avp_vec_free(&toks);
    AvpTry t;
    if (AVP_TRY(&t)) {
        DraftDefs defs = draft_scan_source(a, text);
        avp_json_set(a, rec, AVP_S("defs"), defs_json(a, &defs));
        DraftStmts st = draft_top_level_statements(a, text, &defs);
        avp_json_set(a, rec, AVP_S("stmts"), stmts_json(a, &st));
        DraftMarks mk = draft_marked_functions(a, text);
        AvpJson *marks = avp_json_arr(a);
        for (size_t i = 0; i < mk.n; i++) {
            AvpJson *x = avp_json_arr(a);
            avp_json_arr_push(a, x, avp_json_int(a, mk.v[i].address));
            avp_json_arr_push(a, x, avp_json_strz(a, mk.v[i].name));
            avp_json_arr_push(a, x, mk.v[i].cls ? avp_json_strz(a, mk.v[i].cls) : avp_json_null());
            avp_json_arr_push(a, marks, x);
        }
        avp_json_set(a, rec, AVP_S("marks"), marks);
        avp_try_end(&t);
    } else {
        avp_json_set(a, rec, AVP_S("error"), avp_json_strz(a, t.msg));
    }
    AvpStrVec incs = draft_includes_of(text);
    AvpJson *ji = avp_json_arr(a);
    for (size_t i = 0; i < incs.n; i++) avp_json_arr_push(a, ji, avp_json_str(a, incs.v[i]));
    avp_json_set(a, rec, AVP_S("includes"), ji);
    AvpJson *blocks = avp_json_obj(a);
    ShsmReFindall f = shsm_re_findall(shsm_re("^\\s*(?:class|struct)\\s+(\\w+)", SHSM_RE_M), text, 0, AVP_END);
    for (size_t i = 0; i < f.count; i++) {
        const char *name = avp_cz(a, shsm_re_findall_at(&f, i, 0));
        AvpTry tb;
        if (AVP_TRY(&tb)) {
            i64 s, e;
            AvpJson *v = avp_json_null();
            if (draft_class_block(a, text, name, &s, &e)) {
                v = avp_json_arr(a);
                avp_json_arr_push(a, v, avp_json_int(a, s));
                avp_json_arr_push(a, v, avp_json_int(a, e));
            }
            avp_json_set(a, blocks, avp_str(name), v);
            avp_try_end(&tb);
        } else {
            avp_json_set(a, blocks, avp_str(name), avp_json_strz(a, tb.msg));
        }
    }
    shsm_re_findall_free(&f);
    avp_json_set(a, rec, AVP_S("blocks"), blocks);
    DraftScreenHits hits = draft_screen(text);
    AvpJson *js = avp_json_arr(a);
    for (size_t i = 0; i < hits.n; i++) {
        AvpJson *x = avp_json_arr(a);
        avp_json_arr_push(a, x, avp_json_int(a, hits.v[i].line));
        avp_json_arr_push(a, x, avp_json_strz(a, hits.v[i].label));
        avp_json_arr_push(a, x, avp_json_str(a, hits.v[i].text));
        avp_json_arr_push(a, js, x);
    }
    avp_json_set(a, rec, AVP_S("screen"), js);
    return rec;
}

AVP_TEST(dump_tree) {
    const char *list = getenv("DRAFT_DUMP_LIST"), *out = getenv("DRAFT_DUMP_OUT");
    if (!list || !out) return;
    AvpArena *a = avp_arena();
    AvpStr text;
    REQUIRE(avp_read_text(a, list, AVP_TM_UTF8, &text));
    AvpStrVec paths = avp_str_split(text, AVP_S("\n"), -1);
    AvpBuf b;
    avp_buf_init(&b, NULL);
    for (size_t i = 0; i < paths.n; i++) {
        if (!paths.v[i].n) continue;
        AvpArena *fa = avp_arena_new(0);
        const char *p = avp_cz(fa, paths.v[i]);
        AvpStr src;
        REQUIRE(avp_read_text(fa, p, AVP_TM_UTF8, &src));
        AvpJsonDumpOpts o = {0};
        avp_json_dump_buf(&b, dump_one(fa, p, src), &o);
        avp_buf_putc(&b, '\n');
        avp_arena_free(fa);
    }
    REQUIRE(avp_write_text(out, avp_buf_str(&b), AVP_TM_UTF8_NL_LF));
    avp_buf_free(&b);
}

AVP_TEST_MAIN("draft")
