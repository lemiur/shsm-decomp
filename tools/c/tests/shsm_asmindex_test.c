/* lib/shsm_asmindex and lib/shsm_libraries on synthetic checkouts: the cases of tools/common/test_common.py TestIndex,
 * TestStaleListings (index part), TestIndexExact and TestLibrariesAndPaths.test_libraries. */
#include "avp/fs.h"
#include "shsm_asm_fixture.h"
#include "shsm_asmindex.h"
#include "shsm_libraries.h"
#include "shsm_test_util.h"

#include <sqlite3.h>

#define CHECK_SV(s, lit) CHECK_MSG((s).p && avp_str_eqz((s), (lit)), "%.*s != %s", AVP_SF(s), (lit))

static const char *STALE_S = "# 0x80001010..0x80001018 | size: 0x8\n.text\n.balign 4\n\n"
                             "# .text:0x0 | 0x80001010 | size: 0x8\n.fn helper, global\n"
                             "/* 80001010 00000000  4E 80 00 20 */\tblr\n"
                             "/* 80001014 00000004  4E 80 00 20 */\tblr\n.endfn helper\n";

/* A checkout at build/tmp/cpilot/B/<name> with build/TESTV1/asm/game/{Alpha,Beta}.s. */
static ShsmRepo *checkout(const char *name) {
    const char *root = fx_dir(name);
    ShsmRepo *r = shsm_repo_new(root, "TESTV1");
    fx_write(r->asm_dir, "game/Alpha.s", ALPHA_S);
    fx_write(r->asm_dir, "game/Beta.s", BETA_S);
    return r;
}

static ShsmAsmIndex *open_ix(const ShsmRepo *r) { return shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = r, .jobs = 1}); }

AVP_TEST(build_query_refresh) {
    ShsmRepo *r = checkout("asmindex basic");
    AvpArena *a = avp_arena();
    ShsmAsmIndex *ix = open_ix(r);
    CHECK_EQ_I64(ix->last_refresh.parsed, 2);
    ShsmIndexStats st = shsm_asmindex_stats(a, ix);
    CHECK_EQ_I64(st.files, 2);
    CHECK_EQ_U64(st.symbols_by_kind.n, 3);
    ShsmSymRow s;
    REQUIRE(shsm_asmindex_symbol(a, ix, AVP_S("helper"), &s));
    CHECK_SV(s.unit, "game/Alpha");
    REQUIRE(shsm_asmindex_symbol_at(a, ix, 0x80001100, &s));
    CHECK_SV(s.name, "jumper");
    REQUIRE(shsm_asmindex_containing(a, ix, 0x8000101C, &s));
    CHECK_SV(s.name, "fn_80001018__8unk_RampFf");
    CHECK_CZ(shsm_asmindex_unit_of(a, ix, 0x80001104), "main/game/Beta");
    ShsmCallers c = shsm_asmindex_callers(a, ix, shsm_target_name(AVP_S("helper")), false);
    CHECK_EQ_U64(c.n, 3);
    c = shsm_asmindex_callers(a, ix, shsm_target_name(AVP_S("fn_80001000__Fv")), true);
    REQUIRE(c.n == 3);
    CHECK_SV(c.v[0].name, "fn_80001018__8unk_RampFf");
    CHECK_EQ_I64(c.v[0].site, 0x80001020);
    CHECK_CZ(c.v[0].kind, "branch");
    CHECK_SV(c.v[1].name, "jumper");
    CHECK_CZ(c.v[1].kind, "call");
    CHECK_SV(c.v[2].name, "lbl_80002008");
    CHECK_CZ(c.v[2].kind, "ptr");
    ShsmCallees ce = shsm_asmindex_callees(a, ix, shsm_target_name(AVP_S("jumper")));
    REQUIRE(ce.n == 2);
    CHECK_SV(ce.v[0].name, "fn_80001000__Fv");
    CHECK_CZ(ce.v[1].kind, "branch");
    CHECK_EQ_I64(ce.v[1].count, 1);
    ShsmDataRefs d = shsm_asmindex_data_refs(a, ix, shsm_target_name(AVP_S("fn_80001000__Fv")));
    REQUIRE(d.n == 2);
    CHECK_SV(d.v[0].sym, "lbl_80002000");
    CHECK_SV(d.v[1].sym, "lbl_80003000");
    ShsmStrRows so = shsm_asmindex_strings_of(a, ix, shsm_target_name(AVP_S("fn_80001000__Fv")), false);
    REQUIRE(so.n == 1);
    CHECK_SV(so.v[0].text, "a\"b\n");
    ShsmFloatRows fo = shsm_asmindex_floats_of(a, ix, shsm_target_name(AVP_S("fn_80001018__8unk_RampFf")));
    REQUIRE(fo.n == 1);
    CHECK_SV(fo.v[0].owner, "lbl_80003008");
    CHECK_EQ_F64(fo.v[0].value, 0.25);
    CHECK_EQ_U64(shsm_asmindex_find_strings(a, ix, AVP_S("b\n"), 200).n, 1);
    ShsmStrRows us = shsm_asmindex_users_of_string(a, ix, AVP_S("a\"b"), 200);
    REQUIRE(us.n >= 1);
    CHECK_SV(us.v[0].owner, "fn_80001000__Fv");
    ShsmAsmFile *f;
    ShsmAsmBlock *fn = shsm_asmindex_function(a, ix, shsm_target_addr(0x80001014), &f);
    REQUIRE(fn);
    CHECK_SV(fn->name, "helper");
    CHECK_EQ_I64(fn->address, 0x80001010);
    CHECK_EQ_U64(fn->rows.n, 2);
    CHECK_SV(fn->section, ".text");
    shsm_asm_free(f);
    ShsmAsmBlock *obj = shsm_asmindex_object(a, ix, shsm_target_name(AVP_S("lbl_80002008")), &f);
    REQUIRE(obj && obj->items.n > 1);
    CHECK_SV(obj->items.v[1].sym, "lbl_80002000");
    shsm_asm_free(f);
    ShsmSymRows u = shsm_asmindex_unit_symbols(a, ix, "main/game/Beta", NULL);
    REQUIRE(u.n == 1);
    CHECK_SV(u.v[0].name, "jumper");
    ShsmRefs rt = shsm_asmindex_refs_to(a, ix, shsm_target_name(AVP_S("jumper")), NULL, 0);
    REQUIRE(rt.n == 1);
    CHECK(rt.v[0].from_sym.p == NULL);
    CHECK_CZ(rt.v[0].kind, "ptr");
    ShsmSymRows se = shsm_asmindex_search(a, ix, AVP_S("fn_%"), "function", 200);
    REQUIRE(se.n);
    CHECK_SV(se.v[0].name, "fn_80001000__Fv");
    shsm_asmindex_close(ix);

    ix = open_ix(r); /* warm: nothing re-parsed */
    CHECK_EQ_I64(ix->last_refresh.parsed, 0);
    shsm_asmindex_close(ix);
    const char *p = avp_join(a, r->asm_dir, "game", "Beta.s");
    fx_write(r->asm_dir, "game/Beta.s", avp_sprintf(a, "%s\n", BETA_S));
    AvpStat fst;
    REQUIRE(avp_stat(p, &fst));
    avp_utime_ns(p, fst.atime_ns, fst.mtime_ns + 1000000000LL);
    ix = open_ix(r);
    CHECK_EQ_I64(ix->last_refresh.parsed, 1);
    CHECK_EQ_I64(shsm_asmindex_stats(a, ix).symbols, 9);
    shsm_asmindex_close(ix);
    avp_remove(p);
    ix = open_ix(r);
    CHECK_EQ_I64(ix->last_refresh.removed, 1);
    CHECK(!shsm_asmindex_symbol(a, ix, AVP_S("jumper"), &s));
    shsm_asmindex_close(ix);
    CHECK(avp_exists(avp_join(a, r->index_dir, "asm.sqlite")));
}

static void write_record(const ShsmRepo *r, const char *modules) {
    fx_write(r->version_build_dir, "config.json",
             avp_sprintf(avp_arena(),
                         "{\"version\": \"1.8.4\", \"units\": [{\"object\": \"o\", \"name\": \"game/Alpha.cpp\"}, "
                         "{\"object\": \"o\", \"name\": \"game/Beta.c\"}], \"modules\": %s}",
                         modules));
}

AVP_TEST(index_holds_no_duplicate_from_a_stale_listing) {
    ShsmRepo *r = checkout("asmindex stale");
    fx_write(r->asm_dir, "auto_03_80001010_text.s", STALE_S);
    write_record(r, "[]");
    AvpArena *a = avp_arena();
    ShsmAsmIndex *ix = open_ix(r);
    CHECK_EQ_I64(shsm_asmindex_stats(a, ix).files, 2);
    CHECK_EQ_U64(shsm_asmindex_symbols_named(a, ix, AVP_S("helper")).n, 1);
    REQUIRE(ix->last_refresh.ignored.n == 1);
    CHECK_SV(ix->last_refresh.ignored.v[0], "auto_03_80001010_text.s");
    CHECK(ix->last_refresh.note && strstr(ix->last_refresh.note, "auto_03_80001010_text.s"));
    shsm_asmindex_close(ix);
    ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = r, .jobs = 1, .readonly = true});
    REQUIRE(ix->last_refresh.ignored.n == 1);
    shsm_asmindex_close(ix);
}

AVP_TEST(is_current_follows_the_listings) {
    ShsmRepo *r = checkout("asmindex current");
    ShsmAsmIndex *ix = open_ix(r);
    CHECK(shsm_asmindex_is_current(ix));
    shsm_asmindex_close(ix);
    ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = r, .jobs = 1, .readonly = true, .no_refresh = true});
    CHECK(shsm_asmindex_is_current(ix));
    /* a listing that changes after the index was written: the read-only index is out of date and says so */
    fx_write(r->asm_dir, "game/Beta.s", avp_sprintf(avp_arena(), "%s\n# changed\n", BETA_S));
    CHECK(!shsm_asmindex_is_current(ix));
    shsm_asmindex_close(ix);
    /* a listing that is gone */
    ix = open_ix(r);
    CHECK(shsm_asmindex_is_current(ix));
    avp_remove(avp_join(avp_arena(), r->asm_dir, "game", "Beta.s"));
    CHECK(!shsm_asmindex_is_current(ix));
    shsm_asmindex_close(ix);
}

static char g_notice[400];
static int g_notices;
static void keep_notice(const char *line) {
    snprintf(g_notice, sizeof g_notice, "%s", line);
    g_notices++;
}

/* A refresh that parses listings says so before it parses; one that finds nothing to do stays silent. */
AVP_TEST(refresh_announces_what_it_parses) {
    ShsmRepo *r = checkout("asmindex notice");
    void (*saved)(const char *) = shsm_asmindex_notice;
    size_t saved_min = shsm_asmindex_notice_min;
    shsm_asmindex_notice = keep_notice;
    shsm_asmindex_notice_min = 1;
    g_notices = 0;
    ShsmAsmIndex *ix = open_ix(r);
    shsm_asmindex_close(ix);
    CHECK_EQ_I64(g_notices, 1);
    CHECK(strstr(g_notice, "refreshing") && strstr(g_notice, "2 of 2 listing(s) changed"));
    ix = open_ix(r); /* nothing changed */
    shsm_asmindex_close(ix);
    CHECK_EQ_I64(g_notices, 1);
    shsm_asmindex_notice_min = 3; /* below the threshold: silent */
    fx_write(r->asm_dir, "game/Beta.s", avp_sprintf(avp_arena(), "%s\n# changed\n", BETA_S));
    ix = open_ix(r);
    shsm_asmindex_close(ix);
    CHECK_EQ_I64(g_notices, 1);
    shsm_asmindex_notice = saved;
    shsm_asmindex_notice_min = saved_min;
}

AVP_TEST(index_drops_a_listing_once_the_split_stops_naming_it) {
    ShsmRepo *r = checkout("asmindex drop");
    fx_write(r->asm_dir, "auto_03_80001010_text.s", STALE_S);
    AvpArena *a = avp_arena();
    ShsmAsmIndex *ix = open_ix(r);
    CHECK_EQ_I64(shsm_asmindex_stats(a, ix).files, 3);
    CHECK(ix->last_refresh.note && strstr(ix->last_refresh.note, "no usable split record"));
    shsm_asmindex_close(ix);
    write_record(r, "[]");
    ix = open_ix(r);
    CHECK_EQ_I64(ix->last_refresh.removed, 1);
    CHECK_EQ_U64(shsm_asmindex_symbols_named(a, ix, AVP_S("helper")).n, 1);
    shsm_asmindex_close(ix);
}

/* ---- TestIndexExact ---- */

static ShsmRepo *g_exact;
static const char *g_exact_asm, *g_exact_db;

static ShsmAsmIndex *exact(bool readonly) {
    if (!g_exact) {
        g_exact = checkout("asmindex exact");
        g_exact_asm = avp_join(avp_arena(), g_exact->root, "exact_asm");
        fx_write(g_exact_asm, "one.s", ONE_S);
        fx_write(g_exact_asm, "two.s", TWO_S);
        g_exact_db = avp_join(avp_arena(), g_exact->root, "build", "index", "exact.sqlite");
    }
    return shsm_asmindex_open(&(ShsmAsmIndexOpts){
        .repo = g_exact, .jobs = 1, .db_path = g_exact_db, .asm_dir = g_exact_asm, .readonly = readonly});
}

AVP_TEST(exact_queries_by_address) {
    AvpArena *a = avp_arena();
    ShsmAsmIndex *ix = exact(false);
    ShsmSymRows dups = shsm_asmindex_symbols_named(a, ix, AVP_S("dup"));
    REQUIRE(dups.n == 2);
    CHECK_EQ_I64(dups.v[0].address, 0x80005000);
    CHECK_EQ_I64(dups.v[1].address, 0x80005010);
    CHECK_EQ_U64(shsm_asmindex_refs_to(a, ix, shsm_target_name(AVP_S("dup")), NULL, 0).n, 2);
    ShsmRefs r = shsm_asmindex_refs_to(a, ix, shsm_target_addr(0x80005000), NULL, 0);
    REQUIRE(r.n == 1);
    CHECK_SV(r.v[0].from_sym, "useOne");
    ShsmSymRow s;
    REQUIRE(shsm_asmindex_symbol_at(a, ix, 0x80005010, &s));
    r = shsm_asmindex_refs_to(a, ix, shsm_target_sym(&s), NULL, 0);
    REQUIRE(r.n == 1);
    CHECK_SV(r.v[0].from_sym, "shared");
    r = shsm_asmindex_refs_from(a, ix, shsm_target_addr(0x80001000), NULL, 0);
    REQUIRE(r.n >= 1);
    CHECK_SV(r.v[0].to_sym, "dup");
    CHECK_EQ_I64(r.v[0].to_base, 0x80005000);
    CHECK_EQ_I64(r.v[0].to_addr, 0x80005000);
    CHECK_EQ_I64(r.v[0].owner_addr, 0x80001000);
    CHECK_SV(r.v[0].section, ".text");
    r = shsm_asmindex_refs_from(a, ix, shsm_target_addr(0x80001004), NULL, 0);
    CHECK_SV(r.v[0].from_sym, "useOne");
    ShsmDataRefs dt = shsm_asmindex_data_targets(a, ix, shsm_target_name(AVP_S("shared")));
    REQUIRE(dt.n == 1);
    CHECK_EQ_I64(dt.v[0].address, 0x80005010);
    static const char *const call[] = {"call"};
    r = shsm_asmindex_refs_from(a, ix, shsm_target_name(AVP_S("useOne")), call, 1);
    REQUIRE(r.n == 2);
    CHECK_SV(r.v[0].to_sym, "shared");
    CHECK_EQ_I64(r.v[0].to_base, 0x80001100);
    CHECK_EQ_I64(r.v[1].to_base, 0x80001000);
    r = shsm_asmindex_refs_at(a, ix, 0x80009004, NULL, 0);
    REQUIRE(r.n == 1);
    CHECK_EQ_I64(r.v[0].from_addr, 0x80007000);
    ShsmCallers c = shsm_asmindex_callers(a, ix, shsm_target_addr(0x80001100), false);
    REQUIRE(c.n == 1);
    CHECK_SV(c.v[0].name, "useOne");
    CHECK_EQ_I64(c.v[0].site, 0x80001008);
    shsm_asmindex_close(ix);
}

AVP_TEST(strings_through_a_pointer_object) {
    AvpArena *a = avp_arena();
    ShsmAsmIndex *ix = exact(false);
    CHECK_EQ_U64(shsm_asmindex_strings_of(a, ix, shsm_target_name(AVP_S("useOne")), false).n, 0);
    ShsmStrRows s = shsm_asmindex_strings_of(a, ix, shsm_target_name(AVP_S("useOne")), true);
    REQUIRE(s.n == 1);
    CHECK_SV(s.v[0].owner, "msgText");
    CHECK_SV(s.v[0].text, "hello");
    s = shsm_asmindex_strings_of(a, ix, shsm_target_name(AVP_S("msgPtr")), false);
    REQUIRE(s.n == 1);
    CHECK_EQ_I64(s.v[0].address, 0x80006000);
    s = shsm_asmindex_users_of_string(a, ix, AVP_S("hell"), 200);
    REQUIRE(s.n == 1);
    CHECK_SV(s.v[0].owner, "msgPtr");
    AvpJson *params = avp_json_arr(a);
    avp_json_arr_push(a, params, avp_json_int(a, 0x80006000));
    AvpJson *rows = shsm_asmindex_sql(a, ix, "SELECT owner, owner_addr FROM strings WHERE address = ?", params);
    CHECK_STR(avp_json_dumps(a, rows, &AVP_JSON_COMPACT), "[[\"msgText\",2147508224]]");
    shsm_asmindex_close(ix);
}

AVP_TEST(call_graph_tables_and_sections) {
    AvpArena *a = avp_arena();
    ShsmAsmIndex *ix = exact(false);
    ShsmCallEdges e = shsm_asmindex_call_graph(a, ix, NULL, 0);
    REQUIRE(e.n == 2);
    CHECK_SV(e.v[0].caller, "useOne");
    CHECK_EQ_I64(e.v[0].callee_addr, 0x80001100);
    CHECK_EQ_I64(e.v[1].site, 0x8000100C);
    ShsmTableEntries t = shsm_asmindex_ctors(a, ix);
    REQUIRE(t.n == 1);
    CHECK_EQ_I64(t.v[0].address, 0x80008000);
    CHECK_SV(t.v[0].target, "useOne");
    CHECK_EQ_I64(t.v[0].target_addr, 0x80001000);
    t = shsm_asmindex_dtors(a, ix);
    REQUIRE(t.n == 1);
    CHECK_SV(t.v[0].section, ".dtors$10");
    ShsmSectionRanges m = shsm_asmindex_section_ranges(a, ix, true);
    bool text = false, dtors = false;
    for (size_t i = 0; i < m.n; i++) {
        if (avp_str_eqz(m.v[i].name, ".text"))
            text = m.v[i].start == 0x80001000 && m.v[i].end == 0x80001108;
        if (avp_str_eqz(m.v[i].name, ".dtors")) dtors = m.v[i].start == 0x80008100 && m.v[i].end == 0x80008104;
    }
    CHECK(text && dtors);
    CHECK_SV(shsm_asmindex_section_of(a, ix, 0x80009008), ".bss");
    CHECK(shsm_asmindex_section_of(a, ix, 0x90000000).p == NULL);
    shsm_asmindex_close(ix);
}

AVP_TEST(opening_does_not_write) {
    AvpArena *a = avp_arena();
    shsm_asmindex_close(exact(false));
    ShsmAsmIndex *ix = exact(false);
    CHECK_EQ_I64(shsm_asmindex_total_changes(ix), 0);
    CHECK(!ix->rebuilt);
    shsm_asmindex_close(ix);
    ix = exact(true);
    ShsmSymRow s;
    REQUIRE(shsm_asmindex_symbol(a, ix, AVP_S("shared"), &s));
    CHECK_EQ_I64(s.address, 0x80001100);
    CHECK(sqlite3_exec(ix->db, "DELETE FROM meta", NULL, NULL, NULL) != SQLITE_OK);
    shsm_asmindex_close(ix);
}

AVP_TEST(older_schema_is_rebuilt_once) {
    AvpArena *a = avp_arena();
    shsm_asmindex_close(exact(false));
    const char *db = avp_join(a, g_exact->root, "build", "index", "old.sqlite");
    avp_remove(db);
    sqlite3 *c;
    REQUIRE(sqlite3_open(db, &c) == SQLITE_OK);
    REQUIRE(sqlite3_exec(c,
                         "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT);"
                         "INSERT INTO meta VALUES ('schema', '1');"
                         "CREATE TABLE files (id INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL, unit TEXT NOT NULL,"
                         " size INTEGER, mtime_ns INTEGER);"
                         "CREATE TABLE refs (from_sym TEXT, from_addr INTEGER, to_sym TEXT NOT NULL, to_off INTEGER,"
                         " kind TEXT NOT NULL, file_id INTEGER NOT NULL);"
                         "INSERT INTO files VALUES (1, 'one.s', 'one', 1, 1);",
                         NULL, NULL, NULL) == SQLITE_OK);
    sqlite3_close(c);
    ShsmAsmIndexOpts o = {.repo = g_exact, .jobs = 1, .db_path = db, .asm_dir = g_exact_asm, .readonly = true};
    ShsmAsmIndex *ix = shsm_asmindex_open(&o);
    ShsmRefs r = shsm_asmindex_refs_to(a, ix, shsm_target_name(AVP_S("bigBuf")), NULL, 0);
    REQUIRE(r.n == 1);
    CHECK_EQ_I64(r.v[0].to_base, 0x80009000);
    shsm_asmindex_close(ix);
    o.readonly = false;
    ix = shsm_asmindex_open(&o);
    CHECK(!ix->rebuilt);
    CHECK_EQ_I64(shsm_asmindex_total_changes(ix), 0);
    shsm_asmindex_close(ix);
    REQUIRE(sqlite3_open(db, &c) == SQLITE_OK);
    REQUIRE(sqlite3_exec(c, "UPDATE meta SET value = '1' WHERE key = 'schema'", NULL, NULL, NULL) == SQLITE_OK);
    sqlite3_close(c);
    o.no_refresh = true;
    ix = shsm_asmindex_open(&o);
    CHECK(ix->rebuilt);
    CHECK_EQ_I64(ix->rebuilt_files, 2);
    CHECK_EQ_I64(shsm_asmindex_stats(a, ix).files, 2);
    shsm_asmindex_close(ix);
}

AVP_TEST(missing_asm_dir_raises) {
    ShsmRepo *r = shsm_repo_new(fx_dir("asmindex none"), "TESTV1");
    CHECK_RAISES(open_ix(r), SHSM_EXC_FILE_NOT_FOUND, NULL);
}

/* ---- libraries ---- */

AVP_TEST(libraries) {
    AvpArena *a = avp_arena();
    ShsmLibraries *l =
        shsm_libraries_parse(a, AVP_S("# c\nstart\tend\tlibrary\tnote\n0x20\t0x30\tsdk/os\t\n0x10\t0x20\tgame/G1\tfirst\n"),
                             NULL);
    REQUIRE(l->rows.n == 2);
    CHECK_SV(l->rows.v[0].library, "game/G1");
    CHECK_SV(l->rows.v[1].library, "sdk/os");
    CHECK_CZ(shsm_libraries_lookup(a, l, 0x10), "game/G1");
    CHECK_CZ(shsm_libraries_lookup(a, l, 0x2F), "sdk/os");
    CHECK(shsm_libraries_lookup(a, l, 0x30) == NULL);
    CHECK(shsm_libraries_lookup(a, l, INT64_MIN) == NULL);
    const ShsmLibRow *row = shsm_libraries_row_of(l, 0x12);
    REQUIRE(row);
    CHECK_EQ_I64(row->start, 0x10);
    CHECK_SV(row->note, "first");
    CHECK_STR(shsm_library_group_of("sdk/os"), "sdk");
    CHECK_STR(shsm_library_group_of(NULL), "unknown");
    AvpStrVec p = shsm_libraries_problems(a, shsm_libraries_parse(a, AVP_S("0x10\t0x20\ta\n0x18\t0x30\tb\n"), NULL),
                                          true, 0, 0x40);
    REQUIRE(p.n == 3);
    CHECK_SV(p.v[0], "overlap: a 0x00000010-0x00000020 and b 0x00000018-0x00000030");
    CHECK_SV(p.v[1], "gap: 0x00000000-0x00000010");
    CHECK_SV(p.v[2], "gap: 0x00000030-0x00000040");
    CHECK_RAISES(shsm_libraries_parse(a, AVP_S("0x10\t0x20\n"), NULL), SHSM_EXC_VALUE,
                 "libraries:1: expected start, end, library, note");
    CHECK_RAISES(shsm_libraries_parse(a, AVP_S("0xZ\t0x20\tx\n"), "t.tsv"), SHSM_EXC_VALUE, "t.tsv:1: bad address");
    ShsmRepo *r = shsm_repo_new(fx_dir("libraries"), "TESTV1");
    CHECK_EQ_U64(shsm_load_libraries(r)->rows.n, 0);
    fx_write(r->root, "config/TESTV1/libraries.tsv", "0x80001000\t0x80001200\tgame/G1\t\n");
    CHECK_CZ(shsm_library_of(a, 0x80001100, r), "game/G1");
    CHECK(shsm_load_libraries(r) == shsm_load_libraries(r));
    AvpStrVec names = shsm_libraries_names(shsm_load_libraries(r));
    CHECK_EQ_U64(names.n, 1);
}

AVP_TEST_MAIN("shsm_asmindex")
