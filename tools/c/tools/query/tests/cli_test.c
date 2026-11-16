/* query.exe on the synthetic checkout of tools/test_query.py: QueryHelpTest and QueryFixtureTest, one C test per
 * Python test, checking the same output lines, JSON fields, exit codes and error messages. */
#include "query_fixture.h"

/* QueryHelpTest */

AVP_TEST(top_level_help_lists_every_subcommand) {
    /* tools/check_docs.py reads the {a,b,...} list to verify the docs/tools.md row */
    QfRun r = qf_runv(NULL, (const char *const[]){"--help", NULL});
    CHECK_EQ_I64(r.rc, 0);
    CHECK(has(r.out, "{fn,callers,callees,xref,strings,find-string,find-float,find-const,data,addr,unit,"
                     "vtable,notes} ..."));
}

/* QueryFixtureTest: fn */

AVP_TEST(fn_by_name_partial_and_address) {
    const char *args[] = {"fn_80001000__Fv", "fn_80001000", "0x80001008", "fn_80001000()"};
    for (size_t i = 0; i < 4; i++) {
        AvpJson *d = jg(QF_J("fn", args[i]), "function");
        CHECK_MSG(avp_str_eqz(avp_json_get_str(d, "name", AVP_S("")), "fn_80001000__Fv"), "%s", args[i]);
    }
    AvpJson *d = jg(QF_J("fn", "fn_80001000__Fv"), "function");
    CHECK_CZ(js(jg(d, "unit")), "main/game/Alpha");
    CHECK_CZ(js(jg(d, "status")), "Matching");
    CHECK_EQ_F64(avp_json_f64(jg(d, "percent"), -1), 100.0);
    AvpJson *rows = jg(d, "rows");
    CHECK_CZ(jcol(rows, "mnemonic"), "lis,addi,lfs,bl");
    CHECK_CZ(js(jg(avp_json_at(rows, 2), "note")), "= 1.5f");
    CHECK_CZ(js(jg(avp_json_at(rows, 1), "note")), "= \"a\\\"b\\n\"");
}

AVP_TEST(fn_text_and_raw) {
    const char *out = QF_Q("fn", "helper", "--raw");
    CHECK(has(out, "2C030000"));
    CHECK(has(out, ".L_80001014:"));
}

AVP_TEST(fn_summary) {
    AvpJson *s = jg(jg(QF_J("fn", "fn_80001000__Fv", "--summary"), "function"), "summary");
    CHECK_EQ_I64(avp_json_get_i64(s, "instructions", -1), 4);
    CHECK_EQ_I64(avp_json_get_i64(s, "calls", -1), 1);
    CHECK(avp_json_get_bool(s, "float", false));
    s = jg(jg(QF_J("fn", "fn_80001018__8unk_RampFf", "--summary"), "function"), "summary");
    CHECK_EQ_I64(avp_json_get_i64(s, "tail_calls", -1), 1);
}

AVP_TEST(unknown_and_ambiguous) {
    QfRun r = QF_RUN("fn", "nosuch");
    CHECK_EQ_I64(r.rc, 2);
    CHECK(has(r.err, "no function matches"));
    r = QF_RUN("fn", "fn_8000");
    CHECK_EQ_I64(r.rc, 2);
    CHECK(has(r.err, "ambiguous"));
}

/* calls */

AVP_TEST(callers) {
    AvpJson *c = jg(QF_J("callers", "helper"), "callers");
    CHECK_CZ(jcol(c, "name"), "fn_80001000__Fv,fn_80001018__8unk_RampFf,jumper");
    CHECK_CZ(js(jg(avp_json_at(c, 2), "kind")), "branch");
    CHECK_CZ(js(jg(avp_json_at(c, 0), "status")), "Matching");
    CHECK(!avp_json_get_bool(avp_json_at(c, 2), "placeholder", true));
    CHECK(avp_json_get_bool(avp_json_at(c, 0), "placeholder", false));
}

AVP_TEST(callees_depth) {
    AvpJson *c = jg(QF_J("callees", "jumper", "--depth", "2"), "callees");
    CHECK_EQ_U64(avp_json_len(c), 2);
    CHECK(jfind(c, "name", "helper") != NULL);
    AvpJson *f = jfind(c, "name", "fn_80001000__Fv");
    REQUIRE(f);
    CHECK_CZ(jcol(jg(f, "children"), "name"), "helper");
    const char *out = QF_Q("callees", "jumper", "--depth", "2");
    CHECK(has(line_at(out, 2), "    "));
}

/* xref / strings */

AVP_TEST(xref_groups) {
    AvpJson *d = QF_J("xref", "lbl_80002000");
    CHECK_CZ(jcol(jg(d, "data"), "name"), "fn_80001000__Fv");
    AvpJson *kinds = jg(avp_json_at(jg(d, "data"), 0), "kinds");
    CHECK_EQ_U64(avp_json_len(kinds), 2);
    CHECK_CZ(js(avp_json_at(kinds, 0)), "ha");
    CHECK_CZ(js(avp_json_at(kinds, 1)), "l");
    CHECK_CZ(jcol(jg(d, "pointers"), "name"), "lbl_80002008");
    AvpJson *offs = jg(avp_json_at(jg(d, "pointers"), 0), "offsets");
    CHECK_EQ_U64(avp_json_len(offs), 1);
    CHECK_EQ_I64(avp_json_i64(avp_json_at(offs, 0), -1), 2);
    d = QF_J("xref", "fn_80001018__8unk_RampFf");
    CHECK_CZ(jcol(jg(d, "pointers"), "name"), "__vt__8unk_Ramp");
}

AVP_TEST(strings_function_and_unit) {
    AvpJson *s = jg(QF_J("strings", "fn_80001000__Fv"), "strings");
    CHECK_CZ(jcol(s, "text"), "a\"b\n");
    s = jg(QF_J("strings", "game/Alpha"), "strings");
    CHECK_EQ_U64(avp_json_len(s), 1);
}

AVP_TEST(find_string) {
    AvpJson *s = jg(QF_J("find-string", "a\"b"), "strings");
    CHECK_CZ(js(jg(avp_json_at(jg(avp_json_at(s, 0), "users"), 0), "name")), "fn_80001000__Fv");
    CHECK_EQ_U64(avp_json_len(jg(QF_J("find-string", "-e", "^A.B", "-i"), "strings")), 1);
    CHECK_EQ_I64(QF_RUN("find-string", "zzz").rc, 1);
}

/* constants */

AVP_TEST(find_float) {
    AvpJson *f = jg(QF_J("find-float", "1.5"), "floats");
    CHECK_EQ_U64(avp_json_len(f), 1);
    CHECK_CZ(js(jg(avp_json_at(f, 0), "object")), "lbl_80003000");
    CHECK_CZ(js(jg(avp_json_at(jg(avp_json_at(f, 0), "users"), 0), "name")), "fn_80001000__Fv");
    CHECK_CZ(js(jg(avp_json_at(jg(QF_J("find-float", "0.25"), "floats"), 0), "kind")), "double");
    CHECK_EQ_I64(QF_RUN("find-float", "7.0").rc, 1);
}

/* [(h.symbol, h.offset) for h in hits if h.kind == kind] as "symbol+offset,..." */
static const char *hits_of(const AvpJson *hits, const char *kind) {
    AvpArena *a = avp_arena();
    const char *s = "";
    for (size_t i = 0; i < avp_json_len(hits); i++) {
        AvpJson *h = avp_json_at(hits, i);
        const char *k = js(jg(h, "kind"));
        if (!k || strcmp(k, kind) != 0) continue;
        const char *sym = js(jg(h, "symbol"));
        s = avp_sprintf(a, "%s%s%s+%lld", s, *s ? "," : "", sym ? sym : "-",
                        (long long)avp_json_get_i64(h, "offset", -1));
    }
    return s;
}

AVP_TEST(find_const) {
    AvpJson *d = QF_J("find-const", "0x80002000");
    CHECK_CZ(hits_of(jg(d, "hits"), "lis/addi"), "fn_80001000__Fv+0");
    CHECK_CZ(js(jg(d, "address_of")), "lbl_80002000");
    d = QF_J("find-const", "80001000");
    CHECK_CZ(hits_of(jg(d, "hits"), "data word"), "lbl_80002008+0");
}

/* data */

AVP_TEST(data_views) {
    AvpJson *d = QF_J("data", "lbl_80003000");
    CHECK(has(js(jg(d, "draft")), "float lbl_80003000 = 1.5f;"));
    d = QF_J("data", "lbl_80002000");
    CHECK(has(js(jg(d, "draft")), "char lbl_80002000[] = \"a\\\"b\\n\";"));
    d = QF_J("data", "lbl_80002008");
    CHECK_CZ(js(jg(d, "view")), "mixed (struct)");
    CHECK(has(js(jg(d, "draft")), "fn_80001000__Fv,"));
    CHECK(has(js(jg(d, "draft")), "(u8*)&lbl_80002000 + 0x2"));
    d = QF_J("data", "__vt__8unk_Ramp");
    CHECK_CZ(js(jg(d, "view")), "vtable");
    CHECK(has(js(jg(d, "draft")), "unk_Ramp::fn_80001018(float)"));
    const char *out = QF_Q("data", "lbl_80002008");
    CHECK(has(line_at(out, 1), "guess"));
}

AVP_TEST(data_forced_view) {
    AvpJson *d = QF_J("data", "lbl_80002008", "--as", "u32");
    CHECK(has(js(jg(d, "draft")), "0x80001000"));
    d = QF_J("data", "lbl_80002008", "--as", "ptr");
    CHECK(has(js(jg(d, "draft")), "fn_80001000__Fv"));
    CHECK(has(js(jg(d, "draft")), "lbl_80002000+0x2"));
}

/* addr / unit / vtable */

AVP_TEST(addr) {
    AvpJson *d = QF_J("addr", "0x80001004");
    CHECK_CZ(js(jg(d, "symbol")), "fn_80001000__Fv");
    CHECK_EQ_I64(avp_json_get_i64(d, "offset", -1), 4);
    CHECK_CZ(js(jg(d, "unit")), "main/game/Alpha");
    CHECK_CZ(js(jg(d, "section")), ".text");
    CHECK(has(js(jg(d, "instruction")), "addi"));
    d = QF_J("addr", "0x8000200C");
    CHECK_CZ(js(jg(jg(d, "item"), "kind")), "ptr");
}

AVP_TEST(unit) {
    AvpJson *d = QF_J("unit", "game/Alpha", "--data");
    CHECK_CZ(jcol(jg(d, "functions"), "name"), "fn_80001000__Fv,helper,fn_80001018__8unk_RampFf");
    AvpJson *text = jg(jg(d, "sections"), ".text");
    CHECK_EQ_U64(avp_json_len(text), 2);
    CHECK_EQ_I64(avp_json_i64(avp_json_at(text, 0), -1), 0x80001000);
    CHECK_EQ_I64(avp_json_i64(avp_json_at(text, 1), -1), 0x80001024);
    CHECK_EQ_U64(avp_json_len(jg(d, "data")), 4);
    CHECK(has(QF_Q("unit", "Alpha"), "-O4,p"));
}

AVP_TEST(vtable) {
    const char *args[] = {"__vt__8unk_Ramp", "unk_Ramp"};
    for (size_t i = 0; i < 2; i++) {
        AvpJson *d = QF_J("vtable", args[i]);
        CHECK_CZ(js(jg(jg(d, "vtable"), "name")), "__vt__8unk_Ramp");
        CHECK_CZ(js(jg(jg(avp_json_at(jg(d, "slots"), 2), "target"), "name")), "fn_80001018__8unk_RampFf");
    }
}

/* notes */

static AvpJson *hit_at(const AvpJson *hits, const char *file, i64 line) {
    for (size_t i = 0; i < avp_json_len(hits); i++) {
        AvpJson *h = avp_json_at(hits, i);
        const char *f = js(jg(h, "file"));
        if (f && strcmp(f, file) == 0 && avp_json_get_i64(h, "line", -1) == line) return h;
    }
    return NULL;
}

AVP_TEST(notes_docs_and_source) {
    AvpJson *d = QF_J("notes", "fn_80001018");
    AvpJson *hits = jg(d, "hits");
    CHECK(hit_at(hits, "docs/exceptions.md", 7) != NULL);  /* table row (mangled name) */
    CHECK(hit_at(hits, "docs/exceptions.md", 14) != NULL); /* paragraph (plain name) */
    AvpJson *para = NULL;
    bool term = false;
    for (size_t i = 0; i < avp_json_len(jg(d, "terms")); i++)
        term |= avp_str_eqz(avp_json_str_or(avp_json_at(jg(d, "terms"), i), AVP_S("")), "fn_80001018__8unk_RampFf");
    CHECK(term);
    size_t nsrc = 0;
    AvpJson *src = NULL;
    for (size_t i = 0; i < avp_json_len(hits); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(hits, i), "file", AVP_S("")), "src/game/Alpha.cpp")) {
            nsrc++;
            src = avp_json_at(hits, i);
        }
    CHECK_EQ_U64(nsrc, 1);
    CHECK(src && has(js(jg(src, "text")), "Advances the ramp."));
    /* the first hit on line 14, whichever file */
    for (size_t i = 0; i < avp_json_len(hits); i++)
        if (avp_json_get_i64(avp_json_at(hits, i), "line", -1) == 14) {
            para = avp_json_at(hits, i);
            break;
        }
    REQUIRE(para);
    CHECK_CZ(js(jg(para, "heading")), "Exceptions record > Rulings > Alpha rulings");
    CHECK(has(js(jg(para, "text")), "spans two lines"));
}

AVP_TEST(notes_trailing_address_comment) {
    AvpJson *hits = jg(QF_J("notes", "0x80001000", "--source-only"), "hits");
    bool found = false;
    for (size_t i = 0; i < avp_json_len(hits); i++) found |= has(js(jg(avp_json_at(hits, i), "text")),
                                                                 "void plain( void ) {");
    CHECK(found);
}

AVP_TEST(notes_rule_id_and_nothing) {
    AvpJson *hits = jg(QF_J("notes", "R4", "--docs-only"), "hits");
    CHECK_EQ_U64(avp_json_len(hits), 1);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_at(hits, 0), "line", -1), 8);
    CHECK_EQ_I64(QF_RUN("notes", "nothing-mentions-this").rc, 1);
}

AVP_TEST(cap) {
    CHECK(has(QF_Q("fn", "fn_80001000__Fv", "--limit", "3"), "more lines, use --all"));
}

AVP_TEST_MAIN("query")
