/* The importable part of query (query.h, query_a.h, query_notes.h) in process on the synthetic checkout of
 * tools/test_query.py: test_doc_notes_entry_point, and the helpers its CLI tests exercise (resolve, summarize,
 * row_note, describe_sym / sym_line, the DOL reader, cstr, _fmt_float, parse_int, parse_addr, note_terms,
 * comment_blocks). */
#include "query_a.h"
#include "query_fixture.h"
#include "query_notes.h"

static QueryCtx *ctx_open(void) { return query_ctx_for_root(avp_arena(), qf_root()); }

/* Runs resolve and returns "name" or "!<exception message>". */
static const char *try_resolve(QueryCtx *ctx, const char *text, const char *kind) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        const ShsmSymRow *s = query_resolve(ctx, avp_str(text), kind);
        avp_try_end(&t);
        return avp_cz(avp_arena(), s->name);
    }
    if (t.code != SHSM_EXC_USAGE) return avp_sprintf(avp_arena(), "!exception %d: %s", t.code, t.msg);
    return avp_sprintf(avp_arena(), "!%s", t.msg);
}

AVP_TEST(resolve_name_partial_address_and_errors) {
    QueryCtx *ctx = ctx_open();
    const char *same[] = {"fn_80001000__Fv", "fn_80001000", "0x80001008", "fn_80001000()", "80001004"};
    for (size_t i = 0; i < 5; i++) CHECK_CZ(try_resolve(ctx, same[i], NULL), "fn_80001000__Fv");
    CHECK_CZ(try_resolve(ctx, "unk_Ramp::fn_80001018", NULL), "fn_80001018__8unk_RampFf");
    CHECK_CZ(try_resolve(ctx, "lbl_80002000", "object"), "lbl_80002000");
    CHECK_CZ(try_resolve(ctx, "0x8000200C", NULL), "lbl_80002008");
    CHECK_CZ(try_resolve(ctx, "nosuch", "function"), "!no function matches 'nosuch'");
    CHECK_CZ(try_resolve(ctx, "nosuch", NULL), "!no symbol matches 'nosuch'");
    CHECK_CZ(try_resolve(ctx, "fn_8000", "function"),
             "!'fn_8000' is ambiguous: fn_80001000__Fv, fn_80001018__8unk_RampFf");
    CHECK_CZ(try_resolve(ctx, "0x80009000", "function"), "!no function at 0x80009000");
    CHECK_CZ(try_resolve(ctx, "helper", "object"), "!helper is a function, not a object");
    query_ctx_close(ctx);
}

static ShsmAsmBlock *fn_block(QueryCtx *ctx, const char *name) {
    ShsmAsmFile *af = NULL; /* kept open: the rows view into it */
    return shsm_asmindex_function(ctx->a, query_ix(ctx), shsm_target_name(avp_str(name)), &af);
}

AVP_TEST(summarize_counts) {
    QueryCtx *ctx = ctx_open();
    ShsmAsmBlock *b = fn_block(ctx, "fn_80001000__Fv");
    REQUIRE(b);
    QuerySummary s = query_summarize(b->rows.v, b->rows.n);
    CHECK_EQ_I64(s.instructions, 4);
    CHECK_EQ_I64(s.calls, 1);
    CHECK_EQ_I64(s.tail_calls, 0);
    CHECK_EQ_I64(s.frame, 0);
    CHECK_EQ_I64(s.min_gpr, -1);
    CHECK(s.floats);
    b = fn_block(ctx, "fn_80001018__8unk_RampFf");
    REQUIRE(b);
    s = query_summarize(b->rows.v, b->rows.n);
    CHECK_EQ_I64(s.tail_calls, 1);
    CHECK_EQ_I64(s.branches, 1);
    CHECK_EQ_I64(s.calls, 1);
    AvpJson *sj = query_summary_json(ctx->a, &s);
    CHECK(avp_json_has(sj, AVP_S("saved_gprs")) && avp_json_is_null(jg(sj, "saved_gprs")));
    CHECK_EQ_I64(avp_json_get_i64(sj, "tail_calls", -1), 1);
    b = fn_block(ctx, "helper");
    REQUIRE(b);
    s = query_summarize(b->rows.v, b->rows.n);
    CHECK_EQ_I64(s.instructions, 2);
    CHECK_EQ_I64(s.branches, 1);
    CHECK_EQ_I64(s.tail_calls, 0);
    CHECK(!s.floats);
    query_ctx_close(ctx);
}

AVP_TEST(row_note_values) {
    QueryCtx *ctx = ctx_open();
    ShsmAsmBlock *b = fn_block(ctx, "fn_80001000__Fv");
    REQUIRE(b && b->rows.n == 4);
    CHECK_STR(query_row_note(ctx, &b->rows.v[0]), "");          /* @ha: no note */
    CHECK_STR(query_row_note(ctx, &b->rows.v[1]), "= \"a\\\"b\\n\"");
    CHECK_STR(query_row_note(ctx, &b->rows.v[2]), "= 1.5f");
    CHECK_STR(query_row_note(ctx, &b->rows.v[3]), "");          /* bl helper: demangles to itself */
    b = fn_block(ctx, "fn_80001018__8unk_RampFf");
    REQUIRE(b && b->rows.n == 3);
    CHECK_STR(query_row_note(ctx, &b->rows.v[1]), "= 0.25");
    CHECK_STR(query_row_note(ctx, &b->rows.v[2]), "fn_80001000()"); /* tail call: the callee demangled */
    query_ctx_close(ctx);
}

AVP_TEST(describe_sym_and_sym_line) {
    QueryCtx *ctx = ctx_open();
    QuerySymDesc d = query_describe_sym(ctx, AVP_S("fn_80001000__Fv"), SHSM_NONE);
    CHECK_STR(d.unit, "main/game/Alpha");
    CHECK_CZ(d.status, "Matching");
    CHECK(d.placeholder);
    CHECK(d.has_percent);
    CHECK_EQ_F64(d.percent, 100.0);
    CHECK_STR(query_sym_line(ctx->a, &d, AVP_S("")),
              "M? fn_80001000__Fv  {fn_80001000()}  main/game/Alpha Matching  100.0%");
    d = query_describe_sym(ctx, AVP_S("jumper"), SHSM_NONE);
    CHECK_CZ(d.status, "NonMatching");
    CHECK(!d.placeholder);
    CHECK_STR(query_sym_line(ctx->a, &d, AVP_S("")), ".  jumper  main/game/Beta NonMatching  12.5%");
    CHECK_CZ(query_status(ctx, AVP_S("game/Gamma")), "Equivalent");
    CHECK_CZ(query_status(ctx, AVP_S("main/nosuch")), "?");
    query_ctx_close(ctx);
}

AVP_TEST(dol_read) {
    QueryCtx *ctx = ctx_open();
    const QueryDol *d = query_dol(ctx);
    REQUIRE(d);
    CHECK_EQ_U64(d->nsections, 3);
    AvpStr b = query_dol_read(ctx->a, d, 0x80001000, 4);
    REQUIRE(b.p && b.n == 4);
    CHECK_MEM(b.p, "\x3C\x60\x80\x00", 4);
    b = query_dol_read(ctx->a, d, 0x8000200C, 4);
    REQUIRE(b.p && b.n == 4);
    CHECK_MEM(b.p, "\x80\x00\x20\x02", 4);
    b = query_dol_read(ctx->a, d, 0x80004010, 4); /* .bss */
    REQUIRE(b.p && b.n == 4);
    CHECK_MEM(b.p, "\0\0\0\0", 4);
    CHECK(query_dol_read(ctx->a, d, 0x80005000, 4).p == NULL);
    CHECK(query_dol_read(ctx->a, d, 0x80001104, 8).p == NULL); /* runs past .text */
    query_ctx_close(ctx);
}

AVP_TEST(cstr_escapes_and_cut) {
    AvpArena *a = avp_arena();
    CHECK_STR(query_cstr(a, AVP_S("a\"b\n\t\r\x01\x7f\\"), 0), "\"a\\\"b\\n\\t\\r\\x01\\x7F\\\\\"");
    CHECK_STR(query_cstr(a, AVP_S("abcdefghij"), 8), "\"abcde...\"");
    CHECK_STR(query_cstr(a, AVP_S("abcdefgh"), 8), "\"abcdefgh\"");
    CHECK_STR(query_cstr(a, AVP_S(""), 0), "\"\"");
}

AVP_TEST(fmt_float_shortest_literal) {
    AvpArena *a = avp_arena();
    CHECK_STR(query_fmt_float(a, true, 1.5, true), "1.5f");
    CHECK_STR(query_fmt_float(a, true, 0.25, false), "0.25");
    CHECK_STR(query_fmt_float(a, true, (double)0.1f, true), "0.1f");
    CHECK_STR(query_fmt_float(a, true, 3.0, false), "3.0");
    CHECK_STR(query_fmt_float(a, true, 1e20, false), "1e+20");
    CHECK_STR(query_fmt_float(a, true, 100000000000.0, false), "1e+11");
    CHECK_STR(query_fmt_float(a, false, 0, true), "0");
}

AVP_TEST(fmt_float_extremes) {
    AvpArena *a = avp_arena();
    /* FLT_MAX: the 7-digit decimal 3.402824e+38 is beyond the float range; it must not abort. */
    CHECK_STR(query_fmt_float(a, true, (double)3.4028234663852886e+38, true), "3.4028235e+38f");
    CHECK_STR(query_fmt_float(a, true, -(double)3.4028234663852886e+38, true), "-3.4028235e+38f");
    CHECK_STR(query_fmt_float(a, true, 1.7976931348623157e+308, false), "1.7976931348623157e+308");
    CHECK_STR(query_fmt_float(a, true, 1e300, true), "1e+300f");
}

AVP_TEST(parse_int_and_addr) {
    AvpArena *a = avp_arena();
    CHECK_EQ_I64(query_parse_int(a, AVP_S("3F800000")), 0x3F800000);
    CHECK_EQ_I64(query_parse_int(a, AVP_S("0x3F800000")), 0x3F800000);
    CHECK_EQ_I64(query_parse_int(a, AVP_S("-0x8")), -8);
    CHECK_EQ_I64(query_parse_int(a, AVP_S("0d100")), 100);
    CHECK_EQ_I64(query_parse_int(a, AVP_S(" 1_0 ")), 16);
    CHECK_RAISES(query_parse_int(a, AVP_S("zz")), SHSM_EXC_USAGE, "not a number: zz");
    CHECK_EQ_I64(query_parse_addr(AVP_S("0x80001008")), 0x80001008);
    CHECK_EQ_I64(query_parse_addr(AVP_S(" 80001008 ")), 0x80001008);
    CHECK_EQ_I64(query_parse_addr(AVP_S("0x1000")), SHSM_NONE);
    CHECK_EQ_I64(query_parse_addr(AVP_S("nosuch")), SHSM_NONE);
}

AVP_TEST(small_formatters) {
    AvpArena *a = avp_arena();
    CHECK_STR(query_pct(a, false, 0), "  -   ");
    CHECK_STR(query_pct(a, true, 12.5), " 12.5%");
    CHECK_CZ(query_mark("Matching"), "M");
    CHECK_CZ(query_mark("NonMatching"), ".");
    CHECK(query_is_placeholder(a, AVP_S("fn_80001000__Fv")));
    CHECK(query_is_placeholder(a, AVP_S("fn_800067CC__8unk_RampFf")));
    CHECK(query_is_placeholder(a, AVP_S("@123")));
    CHECK(!query_is_placeholder(a, AVP_S("helper")));
    CHECK(!query_is_placeholder(a, AVP_S("")));
    CHECK_STR(query_demangled(a, AVP_S("fn_80001018__8unk_RampFf")), "unk_Ramp::fn_80001018(float)");
    CHECK(query_demangled(a, AVP_S("helper")).p == NULL);
}

AVP_TEST(note_terms_of_a_member) {
    QueryCtx *ctx = ctx_open();
    const ShsmSymRow *sym = NULL;
    AvpStrVec t = query_note_terms(ctx, AVP_S("fn_80001018"), &sym);
    REQUIRE(sym);
    CHECK_STR(sym->name, "fn_80001018__8unk_RampFf");
    REQUIRE(t.n == 3);
    CHECK_STR(t.v[0], "fn_80001018");
    CHECK_STR(t.v[1], "fn_80001018__8unk_RampFf");
    CHECK_STR(t.v[2], "0x80001018");
    t = query_note_terms(ctx, AVP_S("R4"), &sym);
    CHECK(sym == NULL);
    CHECK_EQ_U64(t.n, 1);
    query_ctx_close(ctx);
}

AVP_TEST(comment_blocks_of_the_source) {
    AvpStrVec lines = avp_str_splitlines(avp_str(QF_ALPHA_CPP), false);
    QueryBlocks b = query_comment_blocks(lines.v, lines.n);
    REQUIRE(b.n == 1);
    CHECK_EQ_I64(b.v[0].first, 3);
    CHECK_EQ_I64(b.v[0].last, 10);
    const AvpStr open[] = {AVP_S("/* never closed"), AVP_S("x")};
    b = query_comment_blocks(open, 2);
    REQUIRE(b.n == 1);
    CHECK_EQ_I64(b.v[0].last, 3); /* an unterminated block ends one past n */
}

/* test_doc_notes_entry_point */
AVP_TEST(doc_notes_entry_point) {
    QueryCtx *ctx = ctx_open();
    AvpStrVec terms = {0};
    QueryNoteHits hits = query_doc_notes(ctx, AVP_S("fn_80001018"), NULL, &terms);
    bool term = false;
    for (size_t i = 0; i < terms.n; i++) term |= avp_str_eqz(terms.v[i], "fn_80001018");
    CHECK(term);
    CHECK(hits.n > 0);
    for (size_t i = 0; i < hits.n; i++) CHECK(avp_str_startswith(hits.v[i].file, AVP_S("docs/")));
    QueryNotesOpts all = {.docs_only = false};
    QueryNoteHits all_hits = query_doc_notes(ctx, AVP_S("fn_80001018"), &all, NULL);
    CHECK(all_hits.n >= hits.n);
    const char *names[16];
    size_t nn = 0;
    for (size_t i = 0; i < hits.n && nn < 16; i++) {
        const char *base = avp_cz(ctx->a, avp_path_basename(avp_cz(ctx->a, hits.v[i].file)));
        bool seen = false;
        for (size_t k = 0; k < nn; k++) seen |= strcmp(names[k], base) == 0;
        if (!seen) names[nn++] = base;
    }
    QueryNotesOpts ex = {.docs_only = true, .exclude = names, .nexclude = nn};
    QueryNoteHits rest = query_doc_notes(ctx, AVP_S("fn_80001018"), &ex, NULL);
    CHECK_EQ_U64(rest.n, 0);
    query_ctx_close(ctx);
}

AVP_TEST_MAIN("query")
