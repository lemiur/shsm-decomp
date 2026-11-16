/* Tests of json.h against CPython 3.14.8.
 *  - json_corpus.h: thousands of generated cases (json.loads + json.dumps with 23 option sets, and the exceptions),
 *    see the generator in its header comment. The option sets cover every json.dump/json.dumps combination the
 *    Python tools use (grep "json.dump" in the tools' .py files):
 *    none; indent=1/2/0 (alone and with sort_keys=True); sort_keys=True; allow_nan=False; ensure_ascii=False with
 *    allow_nan=False, with separators=(",", ":"), with indent=1/2, with sort_keys; separators=(",", ":") alone and
 *    with sort_keys. (default=str and the like are the caller's, see json.h.)
 *  - hand-written cases for the builder, the accessors, the streaming writer and the limits; the expected strings
 *    in them come from
 *        d = {"a": [1, 2.5, "x\u00e9"], "b": {}, "c": [], "d": None, "e": {"f": [True, False, {"g": 1}]}}
 *        json.dumps(d) / json.dumps(d, indent=2) / json.dumps(d, indent="\t", ensure_ascii=False) /
 *        json.dumps(d, separators=(",", ":"), ensure_ascii=False) / json.dumps([[], [[]], {}, [{}]], indent=1) /
 *        json.dumps({"b": 1, "a": {"z": 1, "y": 2}}, sort_keys=True, indent=1) */
#include "avp/avp.h"
#include "avp/json.h"
#include "avp/test.h"
#include <math.h>
#include "json_corpus.h"

static AvpJsonDumpOpts opts_for(int id) {
    AvpJsonDumpOpts o = {0};
    AvpStr comma = AVP_S(","), colon = AVP_S(":");
    switch (id) {
    case 0: break;
    case 1: o.indented = true; o.indent_width = 1; break;
    case 2: o.indented = true; o.indent_width = 2; break;
    case 3: o.indented = true; o.indent_width = 0; break;
    case 4: o.indented = true; o.indent = AVP_S("\t"); break;
    case 5: o.sort_keys = true; break;
    case 6: o.indented = true; o.indent_width = 1; o.sort_keys = true; break;
    case 7: o.indented = true; o.indent_width = 0; o.sort_keys = true; break;
    case 8: o.custom_separators = true; o.item_sep = comma; o.key_sep = colon; break;
    case 9: o.raw_unicode = true; o.reject_nan = true; break;
    case 10: o.raw_unicode = true; o.custom_separators = true; o.item_sep = comma; o.key_sep = colon; break;
    case 11: o.raw_unicode = true; o.reject_nan = true; o.custom_separators = true; o.item_sep = comma; o.key_sep = colon; o.sort_keys = true; break;
    case 12: o.indented = true; o.indent_width = 2; o.sort_keys = true; o.raw_unicode = true; break;
    case 13: o.indented = true; o.indent_width = 1; o.raw_unicode = true; break;
    case 14: o.indented = true; o.indent_width = 1; o.custom_separators = true; o.item_sep = AVP_S(" ,"); o.key_sep = AVP_S(" :"); break;
    case 15: o.reject_nan = true; break;
    case 16: o.raw_unicode = true; o.sort_keys = true; break;
    case 17: o.indented = true; o.indent_width = 4; break;
    case 18: o.indented = true; o.indent = AVP_S("  "); o.custom_separators = true; o.item_sep = AVP_S("; "); o.key_sep = AVP_S("= "); break;
    case 19: o.indented = true; o.indent_width = -1; break;
    case 20: o.indented = true; o.indent_width = 2; o.sort_keys = true; break;
    case 21: o.raw_unicode = true; o.custom_separators = true; o.item_sep = comma; o.key_sep = colon; o.sort_keys = true; break;
    case 22: o.raw_unicode = true; o.reject_nan = true; o.custom_separators = true; o.item_sep = comma; o.key_sep = colon; break;
    default: AVP_ASSERT(false);
    }
    return o;
}

static const char *kind_name(int k) { return k == AVP_JSON_E_DECODE ? "DECODE" : k == AVP_JSON_E_VALUE ? "VALUE" : k == AVP_JSON_E_RECURSION ? "RECURSION" : "UNICODE"; }

static void fmt_error(AvpBuf *b, const AvpJsonError *e) {
    if (e->kind == AVP_JSON_E_DECODE)
        avp_buf_printf(b, "E|DECODE|%.*s|%lld|%lld|%lld", AVP_SF(e->msg), (long long)e->lineno, (long long)e->colno, (long long)e->pos);
    else
        avp_buf_printf(b, "E|%s|%.*s", kind_name(e->kind), AVP_SF(e->msg));
}

AVP_TEST(corpus_against_cpython) {
    AvpArena *a = avp_arena_new(0);
    int failures = 0;
    size_t errors = 0, successes = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(avp_json_cases); i++) {
        const AvpJsonCase *c = &avp_json_cases[i];
        AvpArenaMark mark = avp_arena_mark(a);
        AvpBuf got = {0};
        AvpJsonError err = {0};
        AvpJson *v = avp_json_parse(a, avp_strn(c->text, c->tn), &err);
        if (!v) {
            fmt_error(&got, &err);
            errors++;
        } else {
            AvpJsonDumpOpts o = opts_for(c->opt);
            AvpJsonError derr = {0};
            o.err = &derr;
            AvpStr s = avp_json_dumps(a, v, &o);
            if (!s.p) fmt_error(&got, &derr);
            else avp_buf_put(&got, s);
            successes++;
        }
        AvpStr g = avp_buf_str(&got);
        if (!(g.n == c->en && memcmp(g.p, c->exp, c->en) == 0)) {
            if (failures++ < 8)
                avp_test__fail(__FILE__, __LINE__, "case %zu (opt %d) text \"%.*s\"\n   expected \"%.*s\"\n   got      \"%.*s\"", i, c->opt,
                               (int)AVP_MIN(c->tn, 200), c->text, (int)AVP_MIN(c->en, 300), c->exp, (int)AVP_MIN(g.n, 300), g.p);
        }
        avp_buf_free(&got);
        avp_arena_reset(a, mark);
    }
    CHECK_EQ_I64(failures, 0);
    CHECK(errors > 1000);
    CHECK(successes > 2000);
    avp_arena_free(a);
}

/* ---- the document of the header comment, built three ways ---- */
static const char *EXP_DEFAULT =
    "{\"a\": [1, 2.5, \"x\\u00e9\"], \"b\": {}, \"c\": [], \"d\": null, \"e\": {\"f\": [true, false, {\"g\": 1}]}}";
static const char *EXP_INDENT2 =
    "{\n  \"a\": [\n    1,\n    2.5,\n    \"x\\u00e9\"\n  ],\n  \"b\": {},\n  \"c\": [],\n  \"d\": null,\n  \"e\": {\n"
    "    \"f\": [\n      true,\n      false,\n      {\n        \"g\": 1\n      }\n    ]\n  }\n}";
static const char *EXP_TAB_RAW =
    "{\n\t\"a\": [\n\t\t1,\n\t\t2.5,\n\t\t\"x\303\251\"\n\t],\n\t\"b\": {},\n\t\"c\": [],\n\t\"d\": null,\n\t\"e\": {\n"
    "\t\t\"f\": [\n\t\t\ttrue,\n\t\t\tfalse,\n\t\t\t{\n\t\t\t\t\"g\": 1\n\t\t\t}\n\t\t]\n\t}\n}";
static const char *EXP_COMPACT_RAW =
    "{\"a\":[1,2.5,\"x\303\251\"],\"b\":{},\"c\":[],\"d\":null,\"e\":{\"f\":[true,false,{\"g\":1}]}}";

static AvpJson *tree_doc(AvpArena *a) {
    AvpJson *d = avp_json_obj(a);
    AvpJson *arr = avp_json_arr(a);
    avp_json_arr_push(a, arr, avp_json_int(a, 1));
    avp_json_arr_push(a, arr, avp_json_float(a, 2.5));
    avp_json_arr_push(a, arr, avp_json_strz(a, "x\303\251"));
    avp_json_set(a, d, AVP_S("a"), arr);
    avp_json_set(a, d, AVP_S("b"), avp_json_obj(a));
    avp_json_set(a, d, AVP_S("c"), avp_json_arr(a));
    avp_json_set(a, d, AVP_S("d"), avp_json_null());
    AvpJson *e = avp_json_obj(a), *f = avp_json_arr(a), *g = avp_json_obj(a);
    avp_json_arr_push(a, f, avp_json_bool(true));
    avp_json_arr_push(a, f, avp_json_bool(false));
    avp_json_set(a, g, AVP_S("g"), avp_json_int(a, 1));
    avp_json_arr_push(a, f, g);
    avp_json_set(a, e, AVP_S("f"), f);
    avp_json_set(a, d, AVP_S("e"), e);
    return d;
}

static void stream_doc(AvpJsonW *w) {
    avp_jsonw_begin_object(w);
    avp_jsonw_key(w, AVP_S("a"));
    avp_jsonw_begin_array(w);
    avp_jsonw_i64(w, 1);
    avp_jsonw_f64(w, 2.5);
    avp_jsonw_str(w, AVP_S("x\303\251"));
    avp_jsonw_end_array(w);
    avp_jsonw_key(w, AVP_S("b"));
    avp_jsonw_begin_object(w);
    avp_jsonw_end_object(w);
    avp_jsonw_key(w, AVP_S("c"));
    avp_jsonw_begin_array(w);
    avp_jsonw_end_array(w);
    avp_jsonw_key(w, AVP_S("d"));
    avp_jsonw_null(w);
    avp_jsonw_key(w, AVP_S("e"));
    avp_jsonw_begin_object(w);
    avp_jsonw_key(w, AVP_S("f"));
    avp_jsonw_begin_array(w);
    avp_jsonw_bool(w, true);
    avp_jsonw_bool(w, false);
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_i64(w, "g", 1);
    avp_jsonw_end_object(w);
    avp_jsonw_end_array(w);
    avp_jsonw_end_object(w);
    avp_jsonw_end_object(w);
}

static void check_stream(const AvpJsonDumpOpts *o, const char *expected) {
    AvpBuf b = {0};
    AvpJsonW w;
    avp_jsonw_init(&w, &b, o);
    stream_doc(&w);
    CHECK(avp_jsonw_finish(&w));
    CHECK_STR(avp_buf_str(&b), expected);
    avp_buf_free(&b);
}

AVP_TEST(streaming_writer) {
    AvpJsonDumpOpts o = {0};
    check_stream(&o, EXP_DEFAULT);
    check_stream(&AVP_JSON_INDENT(2), EXP_INDENT2);
    o = (AvpJsonDumpOpts){.indented = true, .indent = AVP_S("\t"), .raw_unicode = true};
    check_stream(&o, EXP_TAB_RAW);
    o = AVP_JSON_COMPACT;
    o.raw_unicode = true;
    check_stream(&o, EXP_COMPACT_RAW);
}

AVP_TEST(tree_builder_and_dumps) {
    AvpArena *a = avp_arena_new(0);
    AvpJson *d = tree_doc(a);
    CHECK_STR(avp_json_dumps(a, d, NULL), EXP_DEFAULT);
    AvpJsonDumpOpts o = AVP_JSON_INDENT(2);
    CHECK_STR(avp_json_dumps(a, d, &o), EXP_INDENT2);
    o = (AvpJsonDumpOpts){.indented = true, .indent = AVP_S("\t"), .raw_unicode = true};
    CHECK_STR(avp_json_dumps(a, d, &o), EXP_TAB_RAW);
    /* dump_buf appends */
    AvpBuf b = {0};
    avp_buf_putz(&b, "> ");
    o = AVP_JSON_COMPACT;
    o.raw_unicode = true;
    CHECK(avp_json_dump_buf(&b, d, &o));
    CHECK_EQ_U64(b.n, 2 + strlen(EXP_COMPACT_RAW));
    avp_buf_free(&b);
    /* empty containers with indent */
    AvpJson *nest = avp_json_arr(a);
    avp_json_arr_push(a, nest, avp_json_arr(a));
    AvpJson *in = avp_json_arr(a);
    avp_json_arr_push(a, in, avp_json_arr(a));
    avp_json_arr_push(a, nest, in);
    avp_json_arr_push(a, nest, avp_json_obj(a));
    AvpJson *w = avp_json_arr(a);
    avp_json_arr_push(a, w, avp_json_obj(a));
    avp_json_arr_push(a, nest, w);
    o = AVP_JSON_INDENT(1);
    CHECK_STR(avp_json_dumps(a, nest, &o), "[\n [],\n [\n  []\n ],\n {},\n [\n  {}\n ]\n]");
    /* sort_keys with a nested object */
    AvpJson *s = avp_json_obj(a), *z = avp_json_obj(a);
    avp_json_set(a, s, AVP_S("b"), avp_json_int(a, 1));
    avp_json_set(a, z, AVP_S("z"), avp_json_int(a, 1));
    avp_json_set(a, z, AVP_S("y"), avp_json_int(a, 2));
    avp_json_set(a, s, AVP_S("a"), z);
    o = AVP_JSON_INDENT(1);
    o.sort_keys = true;
    CHECK_STR(avp_json_dumps(a, s, &o), "{\n \"a\": {\n  \"y\": 2,\n  \"z\": 1\n },\n \"b\": 1\n}");
    avp_arena_free(a);
}

AVP_TEST(ints_floats_and_big_ints) {
    AvpArena *a = avp_arena_new(0);
    CHECK_STR(avp_json_dumps(a, avp_json_int(a, INT64_MIN), NULL), "-9223372036854775808");
    CHECK_STR(avp_json_dumps(a, avp_json_uint(a, UINT64_MAX), NULL), "18446744073709551615");
    CHECK_STR(avp_json_dumps(a, avp_json_uint(a, 9223372036854775808ull), NULL), "9223372036854775808");
    CHECK_STR(avp_json_dumps(a, avp_json_int_digits(a, AVP_S("1180591620717411303424")), NULL), "1180591620717411303424");
    CHECK_STR(avp_json_dumps(a, avp_json_int_digits(a, AVP_S("-0")), NULL), "0");
    CHECK_STR(avp_json_dumps(a, avp_json_int_digits(a, AVP_S("-9223372036854775808")), NULL), "-9223372036854775808");
    AvpJson *big = avp_json_parse(a, AVP_S("123456789012345678901234567890"), NULL);
    CHECK(big && big->kind == AVP_JSON_INT && big->big);
    CHECK_EQ_I64(avp_json_i64(big, -7), -7);
    CHECK_EQ_F64(avp_json_f64(big, 0), 1.2345678901234568e29);
    CHECK_STR(avp_json_dumps(a, big, NULL), "123456789012345678901234567890");
    CHECK_STR(avp_json_dumps(a, avp_json_float(a, -0.0), NULL), "-0.0");
    CHECK_STR(avp_json_dumps(a, avp_json_float(a, 1e22), NULL), "1e+22");
    CHECK_STR(avp_json_dumps(a, avp_json_float(a, NAN), NULL), "NaN");
    CHECK_STR(avp_json_dumps(a, avp_json_float(a, -INFINITY), NULL), "-Infinity");
    AvpJsonDumpOpts o = {.reject_nan = true};
    AvpJsonError e = {0};
    o.err = &e;
    AvpStr s = avp_json_dumps(a, avp_json_float(a, INFINITY), &o);
    CHECK(s.p == NULL);
    CHECK_EQ_I64(e.kind, AVP_JSON_E_VALUE);
    CHECK_STR(e.msg, "Out of range float values are not JSON compliant: inf");
    avp_arena_free(a);
}

AVP_TEST(object_semantics) {
    AvpArena *a = avp_arena_new(0);
    AvpJson *o = avp_json_obj(a);
    /* dict semantics: replace keeps the position, delete + set goes to the end */
    avp_json_set(a, o, AVP_S("a"), avp_json_int(a, 1));
    avp_json_set(a, o, AVP_S("b"), avp_json_int(a, 2));
    AvpJson *prev = avp_json_set(a, o, AVP_S("a"), avp_json_int(a, 3));
    CHECK(prev && avp_json_i64(prev, 0) == 1);
    CHECK_STR(avp_json_dumps(a, o, NULL), "{\"a\": 3, \"b\": 2}");
    CHECK(avp_json_del(o, AVP_S("a")));
    CHECK(!avp_json_del(o, AVP_S("a")));
    avp_json_set(a, o, AVP_S("a"), avp_json_int(a, 4));
    CHECK_STR(avp_json_dumps(a, o, NULL), "{\"b\": 2, \"a\": 4}");
    CHECK_STR(avp_json_member(o, 0)->key, "b");
    CHECK(avp_json_member(o, 2) == NULL);
    /* many members: the index */
    AvpJson *big = avp_json_obj(a);
    for (int i = 0; i < 500; i++) avp_json_set(a, big, avp_str(avp_sprintf(a, "key%d", i)), avp_json_int(a, i));
    CHECK_EQ_U64(avp_json_len(big), 500);
    for (int i = 0; i < 500; i += 7) CHECK_EQ_I64(avp_json_get_i64(big, avp_sprintf(a, "key%d", i), -1), i);
    CHECK(avp_json_get(big, AVP_S("nokey")) == NULL);
    avp_json_set(a, big, AVP_S("key3"), avp_json_int(a, 333));
    CHECK_EQ_U64(avp_json_len(big), 500);
    CHECK_EQ_I64(avp_json_get_i64(big, "key3", -1), 333);
    CHECK_STR(avp_json_member(big, 3)->key, "key3");
    for (int i = 0; i < 500; i += 2) CHECK(avp_json_del(big, avp_str(avp_sprintf(a, "key%d", i))));
    CHECK_EQ_U64(avp_json_len(big), 250);
    CHECK(avp_json_get(big, AVP_S("key4")) == NULL);
    CHECK_EQ_I64(avp_json_get_i64(big, "key5", -1), 5);
    CHECK_STR(avp_json_member(big, 0)->key, "key1");
    /* a parsed object with a repeated key: first position, last value */
    AvpJson *p = avp_json_parse(a, AVP_S("{\"a\":1,\"b\":2,\"a\":3}"), NULL);
    CHECK_STR(avp_json_dumps(a, p, NULL), "{\"a\": 3, \"b\": 2}");
    /* a parsed object with 40 keys and repeats (the index path) */
    AvpBuf t = {0};
    avp_buf_putc(&t, '{');
    for (int i = 0; i < 40; i++) avp_buf_printf(&t, "%s\"k%d\":%d", i ? "," : "", i % 25, i);
    avp_buf_putc(&t, '}');
    AvpJson *q = avp_json_parse(a, avp_buf_str(&t), NULL);
    CHECK_EQ_U64(avp_json_len(q), 25);
    CHECK_EQ_I64(avp_json_get_i64(q, "k3", -1), 28);
    CHECK_EQ_I64(avp_json_get_i64(q, "k24", -1), 24);
    CHECK_STR(avp_json_member(q, 24)->key, "k24");
    avp_buf_free(&t);
    avp_arena_free(a);
}

AVP_TEST(typed_accessors) {
    AvpArena *a = avp_arena_new(0);
    AvpJson *o = avp_json_parse(a, AVP_S("{\"i\":5,\"f\":1.5,\"s\":\"x\",\"b\":true,\"n\":null,\"l\":[1],\"o\":{},\"e\":\"\",\"z\":0.0}"), NULL);
    CHECK_EQ_I64(avp_json_get_i64(o, "i", 9), 5);
    CHECK_EQ_I64(avp_json_get_i64(o, "f", 9), 9);
    CHECK_EQ_I64(avp_json_get_i64(o, "missing", 9), 9);
    CHECK_EQ_F64(avp_json_get_f64(o, "f", 0), 1.5);
    CHECK_EQ_F64(avp_json_get_f64(o, "i", 0), 5.0);
    CHECK_EQ_F64(avp_json_get_f64(o, "s", -1), -1.0);
    CHECK_STR(avp_json_get_str(o, "s", AVP_S("dflt")), "x");
    CHECK_STR(avp_json_get_str(o, "i", AVP_S("dflt")), "dflt");
    CHECK(avp_json_get_bool(o, "b", false));
    CHECK(avp_json_get_bool(o, "s", true));
    CHECK(avp_json_get_or(o, AVP_S("i"), NULL) == avp_json_getz(o, "i"));
    CHECK(avp_json_get_or(o, AVP_S("nope"), avp_json_bool(true)) == avp_json_bool(true));
    CHECK(avp_json_is_null(avp_json_getz(o, "n")));
    CHECK(avp_json_is_null(avp_json_getz(o, "nope")));
    CHECK(!avp_json_is_null(avp_json_getz(o, "i")));
    CHECK(avp_json_is_arr(avp_json_getz(o, "l")) && avp_json_is_obj(avp_json_getz(o, "o")) && avp_json_is_str(avp_json_getz(o, "s")));
    CHECK(avp_json_is_num(avp_json_getz(o, "f")) && avp_json_is_int(avp_json_getz(o, "i")) && !avp_json_is_int(avp_json_getz(o, "f")));
    CHECK_EQ_I64(avp_json_i64(avp_json_at(avp_json_getz(o, "l"), 0), 0), 1);
    CHECK(avp_json_at(avp_json_getz(o, "l"), 1) == NULL);
    CHECK(avp_json_truthy(avp_json_getz(o, "i")) && avp_json_truthy(avp_json_getz(o, "s")) && avp_json_truthy(avp_json_getz(o, "l")));
    CHECK(!avp_json_truthy(avp_json_getz(o, "n")) && !avp_json_truthy(avp_json_getz(o, "e")) && !avp_json_truthy(avp_json_getz(o, "o")));
    CHECK(!avp_json_truthy(avp_json_getz(o, "z")) && !avp_json_truthy(NULL) && avp_json_truthy(avp_json_getz(o, "b")));
    avp_arena_free(a);
}

/* ---- the reader's options and limits ---- */
static void check_err(AvpArena *a, AvpStr text, unsigned flags, int kind, const char *msg, i64 line, i64 col, i64 pos) {
    AvpJsonError e = {0};
    AvpJson *v = avp_json_parse_ex(a, text, flags, &e);
    CHECK(v == NULL);
    CHECK_EQ_I64(e.kind, kind);
    CHECK_STR(e.msg, msg);
    CHECK_EQ_I64(e.lineno, line);
    CHECK_EQ_I64(e.colno, col);
    CHECK_EQ_I64(e.pos, pos);
}

AVP_TEST(reader_flags) {
    AvpArena *a = avp_arena_new(0);
    /* json.load(open(f, encoding="utf-8")) on a file with a BOM, and the utf-8-sig cases */
    check_err(a, AVP_S("\357\273\277[1]"), 0, AVP_JSON_E_DECODE, "Unexpected UTF-8 BOM (decode using utf-8-sig)", 1, 1, 0);
    AvpJson *v = avp_json_parse_ex(a, AVP_S("\357\273\277[1, \"\303\251\"]"), AVP_JSON_STRIP_BOM, NULL);
    CHECK(v && avp_json_len(v) == 2);
    v = avp_json_parse_ex(a, AVP_S("\357\273\277[1]"), AVP_JSON_BYTES, NULL);
    CHECK(v && avp_json_len(v) == 1);
    /* positions after a skipped BOM are relative to the decoded text */
    check_err(a, AVP_S("\357\273\277[1,]"), AVP_JSON_STRIP_BOM, AVP_JSON_E_DECODE, "Illegal trailing comma before end of array", 1, 3, 2);
    /* positions count code points, lines count \n only */
    check_err(a, AVP_S("[\"\303\251\303\251\",\n\"\303\251\", x]"), 0, AVP_JSON_E_DECODE, "Expecting value", 2, 6, 12);
    check_err(a, AVP_S("[1]\r\n[2]"), 0, AVP_JSON_E_DECODE, "Extra data", 2, 1, 5);
    /* the hooks of the project's json.loads calls */
    check_err(a, AVP_S("[NaN]"), AVP_JSON_REJECT_CONSTANTS, AVP_JSON_E_VALUE, "invalid JSON constant: NaN", 0, 0, 0);
    check_err(a, AVP_S("-Infinity"), AVP_JSON_REJECT_CONSTANTS, AVP_JSON_E_VALUE, "invalid JSON constant: -Infinity", 0, 0, 0);
    CHECK(avp_json_parse_ex(a, AVP_S("[NaN]"), 0, NULL) != NULL);
    check_err(a, AVP_S("[1e999]"), AVP_JSON_REJECT_NONFINITE, AVP_JSON_E_VALUE, "nonfinite JSON number", 0, 0, 0);
    CHECK(avp_json_parse_ex(a, AVP_S("[1e99, 7]"), AVP_JSON_REJECT_NONFINITE, NULL) != NULL);
    check_err(a, AVP_S("{\"a\":1,\"b\":{\"c\":1,\"a\":2},\"a\":3}"), AVP_JSON_REJECT_DUP_KEYS, AVP_JSON_E_VALUE, "duplicate JSON key: a", 0, 0, 0);
    CHECK(avp_json_parse_ex(a, AVP_S("{\"a\":1,\"b\":{\"c\":1,\"a\":2}}"), AVP_JSON_REJECT_DUP_KEYS, NULL) != NULL);
    /* invalid UTF-8 is the UnicodeDecodeError of reading the file */
    AvpJsonError e = {0};
    CHECK(avp_json_parse(a, AVP_S("[\"a\377\"]"), &e) == NULL);
    CHECK_EQ_I64(e.kind, AVP_JSON_E_UNICODE);
    CHECK_STR(e.msg, "'utf-8' codec can't decode byte 0xff in position 3: invalid start byte");
    /* str(exception) */
    check_err(a, AVP_S("[1,"), 0, AVP_JSON_E_DECODE, "Expecting value", 1, 4, 3);
    CHECK(avp_json_parse(a, AVP_S("[1,"), &e) == NULL);
    CHECK_STR(avp_json_error_str(a, &e), "Expecting value: line 1 column 4 (char 3)");
    /* without an error slot the error is thrown as AVP_ERR_FORMAT with Python's text */
    AvpTry t;
    if (AVP_TRY(&t)) {
        avp_json_parse(a, AVP_S("{\"a\" 1}"), NULL);
        avp_try_end(&t);
        CHECK(!"parse should have thrown");
    } else {
        CHECK_EQ_I64(t.code, AVP_ERR_FORMAT);
        CHECK_CZ(t.msg, "Expecting ':' delimiter: line 1 column 6 (char 5)");
    }
    avp_arena_free(a);
}

AVP_TEST(limits) {
    AvpArena *a = avp_arena_new(0);
    /* 4300 digits are fine, 4301 are the ValueError of int() (floats are not limited) */
    AvpBuf b = {0};
    for (int i = 0; i < 4300; i++) avp_buf_putc(&b, '7');
    AvpJson *v = avp_json_parse(a, avp_buf_str(&b), NULL);
    CHECK(v && v->kind == AVP_JSON_INT && v->big);
    CHECK_EQ_U64(v->u.str.n, 4300);
    avp_buf_putc(&b, '7');
    AvpJsonError e = {0};
    CHECK(avp_json_parse(a, avp_buf_str(&b), &e) == NULL);
    CHECK_EQ_I64(e.kind, AVP_JSON_E_VALUE);
    CHECK_STR(e.msg,
              "Exceeds the limit (4300 digits) for integer string conversion: value has 4301 digits; use sys.set_int_max_str_digits() to increase the limit");
    avp_buf_free(&b);
    /* deep nesting is iterative: 9000 levels parse and dump, far deeper is a RecursionError */
    size_t depth = 9000;
    for (size_t i = 0; i < depth; i++) {
        if (i % 2) avp_buf_putc(&b, '[');
        else avp_buf_putz(&b, "{\"k\":");
    }
    avp_buf_putc(&b, '1');
    for (size_t i = depth; i-- > 0;) avp_buf_putc(&b, i % 2 ? ']' : '}');
    v = avp_json_parse(a, avp_buf_str(&b), &e);
    REQUIRE(v != NULL);
    AvpStr back = avp_json_dumps(a, v, &AVP_JSON_COMPACT);
    CHECK(back.n == b.n && memcmp(back.p, b.p, b.n) == 0);
    avp_buf_free(&b);
    for (size_t i = 0; i < 20000; i++) avp_buf_putc(&b, '[');
    CHECK(avp_json_parse(a, avp_buf_str(&b), &e) == NULL);
    CHECK_EQ_I64(e.kind, AVP_JSON_E_RECURSION);
    avp_buf_free(&b);
    avp_arena_free(a);
}

static bool spill_collect(void *ctx, const char *p, size_t n) {
    avp_buf_putn((AvpBuf *)ctx, p, n);
    return true;
}

AVP_TEST(spill_and_file) {
    AvpArena *a = avp_arena_new(0);
    AvpJson *d = avp_json_obj(a);
    AvpJson *arr = avp_json_arr(a);
    for (int i = 0; i < 3000; i++) avp_json_arr_push(a, arr, avp_json_float(a, i * 0.37));
    avp_json_set(a, d, AVP_S("values"), arr);
    avp_json_set(a, d, AVP_S("name"), avp_json_strz(a, "spill \303\251"));
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    AvpStr whole = avp_json_dumps(a, d, &o);
    AvpBuf small = {0}, sink = {0};
    AvpJsonW w;
    avp_jsonw_init(&w, &small, &o);
    avp_jsonw_set_spill(&w, 200, spill_collect, &sink);
    avp_jsonw_tree(&w, d);
    CHECK(avp_jsonw_finish(&w));
    CHECK(small.n == 0);
    CHECK(sink.n == whole.n && memcmp(sink.p, whole.p, whole.n) == 0);
    avp_buf_free(&small);
    avp_buf_free(&sink);
    /* json.dump to a FILE */
    FILE *f = tmpfile();
    REQUIRE(f != NULL);
    CHECK(avp_json_dump_file(f, d, &o));
    long len = ftell(f);
    CHECK_EQ_U64(len, whole.n);
    rewind(f);
    char *back = (char *)avp_alloc(a, whole.n + 1);
    CHECK_EQ_U64(fread(back, 1, whole.n, f), whole.n);
    CHECK(memcmp(back, whole.p, whole.n) == 0);
    fclose(f);
    avp_arena_free(a);
}

AVP_TEST(string_escaper) {
    AvpBuf b = {0};
    avp_json_put_string(&b, AVP_S("a\"b\\c\n\t\r\b\f\001\177 \303\251 \360\237\230\200 \355\240\200"), false);
    CHECK_STR(avp_buf_str(&b), "\"a\\\"b\\\\c\\n\\t\\r\\b\\f\\u0001\\u007f \\u00e9 \\ud83d\\ude00 \\ud800\"");
    avp_buf_clear(&b);
    avp_json_put_string(&b, AVP_S("a\"b\\c\n\001\177 \303\251 \360\237\230\200"), true);
    CHECK_STR(avp_buf_str(&b), "\"a\\\"b\\\\c\\n\\u0001\177 \303\251 \360\237\230\200\"");
    avp_buf_free(&b);
    /* a long string crosses the escaper's pieces, including multi-byte sequences at the boundary */
    AvpBuf in = {0}, exp = {0};
    for (int i = 0; i < 5000; i++) {
        avp_buf_putz(&in, "\303\251");
        avp_buf_putz(&exp, "\\u00e9");
    }
    avp_json_put_string(&b, avp_buf_str(&in), false);
    CHECK(b.n == exp.n + 2 && memcmp(b.p + 1, exp.p, exp.n) == 0);
    avp_buf_free(&b);
    avp_buf_free(&in);
    avp_buf_free(&exp);
}

AVP_TEST_MAIN("json")
