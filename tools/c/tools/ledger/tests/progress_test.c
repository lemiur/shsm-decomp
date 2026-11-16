/* ledger: TestProgress of tools/test_ledger.py (test_pure_order_when_no_table_exists is in helpers_test.c). */
#include "fixture.h"
#include <stdlib.h>

static AvpStr generate(Fixture *fx) {
    FxRun r = FX_PROGRESS(fx, "--out", "-");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    return r.out;
}

/* the line holding "| `name` |" */
static AvpStr row(AvpStr out, const char *name) {
    AvpStr want = avp_pyfmt(avp_arena(), "| `%s` |", name);
    AvpStrVec lines = avp_str_split(out, AVP_S("\n"), -1);
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_contains(lines.v[i], want)) return lines.v[i];
    return AVP_S("<no row>");
}

AVP_TEST(test_numbers) {
    Fixture fx = fx_new();
    AvpStr out = generate(&fx);
    CHECK(has(out, "| **matched under the rules** (headline) | 4 / 11 | 56 / 136 |"));
    CHECK(has(out, "| bytes matched (linked from source, excluding `ORIGINAL-ASM`) | 5 / 11 | 68 / 136 |"));
    CHECK(has(out, "| `ORIGINAL-ASM` (linked, listed apart, outside both counts and totals) | 1 | 8 |"));
    CHECK(has(out, "| linked from source in total (objdiff \"complete\") | 6 | 76 |"));
    CHECK(has(out, "| `Matching` units | 3 / 6 | |"));
    CHECK(has(out, "(12 functions and 144 code bytes in all, counting the dtk pad blocks)"));
    CHECK(has(out, "Hand-written prose stays."));
    CHECK(!has(out, "| stale |"));
    fx_done(&fx);
}

AVP_TEST(test_function_rows) {
    Fixture fx = fx_new();
    AvpStr out = generate(&fx);
    CHECK_STR(row(out, "AlphaOne"), "| `lib/Alpha` | `AlphaOne` | 0x80002000 | 16 | matched |  |");
    CHECK(has(row(out, "AlphaAsm"), "| ORIGINAL-ASM |"));
    CHECK(has(row(out, "AlphaPend"), "| MATCH-PENDING (R17: hand-written reason) |"));
    CHECK(has(row(out, "BetaA"), "| parked, 96.4% (MATCH-PENDING: R17, exceptions.md) | unit name unconfirmed; "
                                 "renamed from `fn_80003000`: provenance note |"));
    CHECK(avp_str_endswith(row(out, "fn_80003014"), AVP_S("| parked, not written | placeholder; unit name unconfirmed |")));
    CHECK(has(row(out, "BetaAsm"), "| parked, 100% (ORIGINAL-ASM when linked; not counted) |"));
    CHECK(has(row(out, "BetaNoScoreAsm"), "| parked, asm (ORIGINAL-ASM when linked; not counted) |"));
    fx_done(&fx);
}

AVP_TEST(test_game_section_and_unit_tag) {
    Fixture fx = fx_new();
    AvpStr out = generate(&fx);
    AvpStr before, game;
    REQUIRE(avp_str_partition(out, AVP_S("## Game code"), &before, &game));
    CHECK(has(game, "| `game/Gamma` (unit name unconfirmed) | `fn_80004000__5GammaFv` | 0x80004000 | 12 | matched | placeholder |"));
    CHECK(has(game, "| `game/Gamma` (unit name unconfirmed) | `__sinit_\\Gamma_cpp` | 0x8000400C | 20 | matched | "
                    "placeholder (compiler-generated name) |"));
    CHECK(has(game, "| `game/Gamma` (unit name unconfirmed) | `@36@fn_80004000__5GammaFv` | 0x80004020 | 8 | matched | "
                    "compiler-generated thunk, placeholder |"));
    CHECK(has(game, "| `game/Delta` | `fn_80005000__5DeltaFv` | 0x80005000 | 24 | parked, 50.0% | placeholder |"));
    CHECK(!has(before, "Gamma"));
    CHECK(has(game, "Tail prose."));
    fx_done(&fx);
}

AVP_TEST(test_unit_order_is_sticky_and_new_units_follow_their_predecessor) {
    Fixture fx = fx_new();
    AvpStr out = generate(&fx);
    AvpStrVec lines = avp_str_split(out, AVP_S("\n"), -1), order = {0};
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr g[6];
        if (!ledger_match_row(lines.v[i], g)) continue;
        bool seen = false;
        for (size_t k = 0; k < order.n && !seen; k++) seen = avp_str_eq(order.v[k], g[0]);
        if (!seen) avp_vec_push(&order, g[0]);
    }
    /* Beta stays before Alpha; Gamma (new, 0x80004000) sorts before Delta (listed) */
    static const char *const want[] = {"`lib/Beta`", "`lib/Alpha`", "`game/Gamma` (unit name unconfirmed)", "`game/Delta`"};
    REQUIRE(order.n == 4);
    for (size_t i = 0; i < 4; i++) CHECK_STR(order.v[i], want[i]);
    fx_done(&fx);
}

AVP_TEST(test_idempotent_and_check) {
    Fixture fx = fx_new();
    FxRun r = FX_PROGRESS0(&fx);
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    AvpStr first = fx_read(&fx, "docs/progress.md");
    r = FX_PROGRESS(&fx, "--check");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    FX_PROGRESS0(&fx);
    CHECK(avp_str_eq(fx_read(&fx, "docs/progress.md"), first));
    fx_done(&fx);
}

AVP_TEST(test_check_fails_on_stale_file_and_writes_nothing) {
    Fixture fx = fx_new();
    AvpStr before = fx_read(&fx, "docs/progress.md");
    FxRun r = FX_PROGRESS(&fx, "--check", "--diff");
    CHECK(r.code == 1);
    CHECK(has(r.err, "differs"));
    CHECK(has(r.out, "+| **matched under the rules**"));
    CHECK(avp_str_eq(fx_read(&fx, "docs/progress.md"), before));
    fx_done(&fx);
}

AVP_TEST(test_crlf_is_kept_and_check_ignores_line_endings) {
    Fixture fx = fx_new();
    fx_write_eol(&fx, "docs/progress.md", avp_str(PROGRESS), "\r\n");
    FxRun r = FX_PROGRESS0(&fx);
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    AvpStr text = fx_read(&fx, "docs/progress.md");
    CHECK(avp_str_count(text, AVP_S("\r\n")) == avp_str_count(text, AVP_S("\n")));
    CHECK(avp_str_count(text, AVP_S("\n")) > 20);
    r = FX_PROGRESS(&fx, "--check");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    fx_write_eol(&fx, "docs/progress.md", replace(text, "\r\n", "\n"), "\n");
    CHECK(FX_PROGRESS(&fx, "--check").code == 0);
    fx_done(&fx);
}

AVP_TEST(test_markers_replace_only_the_marked_blocks) {
    Fixture fx = fx_new();
    AvpStr marked = replace(avp_str(PROGRESS), "| measure | functions | code bytes |\n|---|---|---|\n| stale | 1 / 2 | 3 / 4 |",
                            "<!-- ledger:begin numbers -->\n| measure | functions | code bytes |\n|---|---|---|\n| stale | 1 / 2 | 3 / 4 |\n"
                            "<!-- ledger:end numbers -->");
    marked = replace(marked, "(1 functions and 2 code bytes in all",
                     "(<!-- ledger:begin gross -->1 functions and 2 code bytes<!-- ledger:end gross --> in all");
    marked = replace(marked, "\n| unit | function | address | size | status | naming |\n|---|---|---|---|---|---|\n| `lib/Beta`",
                     "\n<!-- ledger:begin status -->\n| unit | function | address | size | status | naming |\n|---|---|---|---|---|---|\n| `lib/Beta`");
    marked = replace(marked, "MATCH-PENDING (R17: hand-written reason) |  |\n\nProse between",
                     "MATCH-PENDING (R17: hand-written reason) |  |\n<!-- ledger:end status -->\n\nProse between");
    fx_write_eol(&fx, "docs/progress.md", marked, "\n");
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    CHECK(has(r.err, "status=marker"));
    CHECK(has(r.err, "numbers=marker"));
    CHECK(has(r.err, "gross=marker"));
    CHECK(has(r.err, "game=located"));
    CHECK(has(r.out, "<!-- ledger:begin gross -->12 functions and 144 code bytes<!-- ledger:end gross --> in all"));
    CHECK(has(r.out, "<!-- ledger:begin numbers -->\n| measure |"));
    CHECK(has(r.out, "<!-- ledger:end numbers -->"));
    CHECK(has(r.out, "Prose between the tables."));
    CHECK(has(r.out, "Hand-written game intro."));
    fx_done(&fx);
}

/* re.sub(r"<!-- ledger:(?:begin|end) \w+ -->\n?", "", s) for the four block names */
static AvpStr strip_markers(AvpStr s) {
    static const char *const names[] = {"numbers", "gross", "status", "game"};
    for (size_t i = 0; i < 4; i++)
        for (int e = 0; e < 2; e++) {
            AvpStr m = avp_pyfmt(avp_arena(), "<!-- ledger:%s %s -->", e ? "end" : "begin", names[i]);
            s = avp_str_replace(avp_arena(), s, avp_str_concat(avp_arena(), m, AVP_S("\n")), AVP_S(""), -1);
            s = avp_str_replace(avp_arena(), s, m, AVP_S(""), -1);
        }
    return s;
}

AVP_TEST(test_add_markers_wraps_located_blocks_once_and_keeps_the_content) {
    Fixture fx = fx_new();
    FxRun plain = FX_PROGRESS(&fx, "--out", "-");
    CHECK(plain.code == 0);
    FxRun marked = FX_PROGRESS(&fx, "--add-markers", "--out", "-");
    CHECK(marked.code == 0);
    static const char *const names[] = {"numbers", "gross", "status", "game"};
    for (size_t i = 0; i < 4; i++) {
        CHECK(avp_str_count(marked.out, avp_pyfmt(avp_arena(), "<!-- ledger:begin %s -->", names[i])) == 1);
        CHECK(avp_str_count(marked.out, avp_pyfmt(avp_arena(), "<!-- ledger:end %s -->", names[i])) == 1);
    }
    CHECK(avp_str_eq(strip_markers(marked.out), plain.out));
    fx_write_eol(&fx, "docs/progress.md", marked.out, "\n");
    CHECK(avp_str_eq(FX_PROGRESS(&fx, "--add-markers", "--out", "-").out, marked.out));
    CHECK(FX_PROGRESS(&fx, "--check").code == 0);
    fx_done(&fx);
}

static void set_path(AvpJson *root, const char *const *path, AvpJson *v) {
    AvpJson *o = root;
    for (size_t i = 0; path[i + 1]; i++) {
        const char *k = path[i];
        o = (k[0] >= '0' && k[0] <= '9') ? avp_json_at(o, (size_t)atoi(k)) : avp_json_getz(o, k);
    }
    size_t last = 0;
    while (path[last + 1]) last++;
    avp_json_set(avp_arena(), o, avp_str(path[last]), v);
}
#define PATH(...) ((const char *const[]){__VA_ARGS__, NULL})

AVP_TEST(test_problem_when_configure_and_report_disagree) {
    Fixture fx = fx_new();
    AvpJson *rep = make_report();
    set_path(rep, PATH("units", "1", "metadata", "complete"), avp_json_bool(false));
    set_path(rep, PATH("measures", "complete_units"), avp_json_int(avp_arena(), 2));
    char *p = fx_write_json(&fx, "report2.json", rep);
    AvpStr before = fx_read(&fx, "docs/progress.md");
    FxRun r = fx_runv(FX_ARGS("progress", "--root", fx.root, "--report", p, "--checker-json", fx.checker));
    CHECK(r.code == 1);
    CHECK(has(r.err, "PROBLEM: lib/Alpha: configure.py says Matching but the report says complete=False"));
    CHECK(avp_str_eq(fx_read(&fx, "docs/progress.md"), before));
    fx_done(&fx);
}

AVP_TEST(test_problem_when_a_linked_function_is_not_exact) {
    Fixture fx = fx_new();
    AvpJson *rep = make_report();
    set_path(rep, PATH("units", "1", "functions", "0", "fuzzy_match_percent"), avp_json_float(avp_arena(), 99.0));
    char *p = fx_write_json(&fx, "report2.json", rep);
    FxRun r = fx_runv(FX_ARGS("progress", "--root", fx.root, "--report", p, "--checker-json", fx.checker, "--check"));
    CHECK(r.code == 1);
    CHECK(has(r.err, "AlphaOne: linked but scored 99.0"));
    fx_done(&fx);
}

AVP_TEST(test_problem_when_symbols_disagree) {
    Fixture fx = fx_new();
    AvpBuf sb;
    avp_buf_init(&sb, avp_arena());
    symbols_add(&sb, AUTO, FX_N(AUTO));
    symbols_add(&sb, BETA, FX_N(BETA));
    symbols_add(&sb, GAMMA, FX_N(GAMMA));
    symbols_add(&sb, DELTA, FX_N(DELTA));
    static const FxFn moved[] = {{"AlphaOne", 0x80002004, 16, -1, NULL}};
    symbols_add(&sb, moved, 1);
    fx_write(&fx, "config/R5WEA4/symbols.txt", avp_buf_cz(&sb));
    FxRun r = FX_PROGRESS(&fx, "--check");
    CHECK(r.code == 1);
    CHECK(has(r.err, "lib/Alpha::AlphaOne: report address/size"));
    fx_done(&fx);
}

static AvpJson *checker_with(const char *const *rec, bool replace_first) {
    AvpJson *c = checker_json();
    AvpJson *funcs = avp_json_getz(c, "functions");
    if (replace_first) funcs->u.arr.v[0] = fx_rec(rec);
    else avp_json_arr_push(avp_arena(), funcs, fx_rec(rec));
    return c;
}

AVP_TEST(test_checker_pending_that_maps_to_no_row_is_a_problem) {
    Fixture fx = fx_new();
    static const char *const extra[] = {"path", "src/lib/Alpha.c", "name", "Alpha::Ghost", "status", "MATCH-PENDING",
                                        "match_rules", "R17", "unit_status", "Matching", NULL};
    fx_write_json(&fx, "checker.json", checker_with(extra, false));
    FxRun r = FX_PROGRESS(&fx, "--check");
    CHECK(r.code == 1);
    CHECK(has(r.err, "Alpha::Ghost: checker MATCH-PENDING in a Matching unit maps to no ledger row"));
    fx_done(&fx);
}

AVP_TEST(test_checker_pending_in_a_matching_unit_is_subtracted_from_the_headline) {
    Fixture fx = fx_new();
    static const char *const extra[] = {"path", "src/lib/Alpha.c", "name", "AlphaOne", "status", "MATCH-PENDING",
                                        "match_rules", "R18", "unit_status", "Matching", NULL};
    fx_write_json(&fx, "checker.json", checker_with(extra, true));
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    CHECK(has(r.out, "(headline) | 3 / 11 | 40 / 136 |"));
    CHECK(has(r.out, "| `AlphaOne` | 0x80002000 | 16 | MATCH-PENDING (R18) |"));
    fx_done(&fx);
}

AVP_TEST(test_dropped_pending_note_warns) {
    Fixture fx = fx_new();
    AvpStr text = replace(avp_str(PROGRESS), "| `lib/Alpha` | `AlphaOne` | 0x80002000 | 16 | matched |  |",
                          "| `lib/Alpha` | `AlphaOne` | 0x80002000 | 16 | parked, 10.0% (MATCH-PENDING: gone) |  |");
    fx_write_eol(&fx, "docs/progress.md", text, "\n");
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK(r.code == 0);
    CHECK(has(r.err, "the old status note \"MATCH-PENDING: gone\" is dropped"));
    fx_done(&fx);
}

AVP_TEST(test_old_not_compiled_wording_is_normalised) {
    Fixture fx = fx_new();
    fx_write_eol(&fx, "docs/progress.md", replace(avp_str(PROGRESS), "parked, 90.0% | renamed", "parked, not compiled | renamed"),
                 "\n");
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK_MSG(r.code == 0, "%.*s", AVP_SF(r.err));
    CHECK(!has(r.out, "not compiled"));
    fx_done(&fx);
}

AVP_TEST(test_unmarked_file_without_tables_warns) {
    Fixture fx = fx_new();
    fx_write(&fx, "docs/progress.md", "# nothing here\n");
    FxRun r = FX_PROGRESS(&fx, "--out", "-");
    CHECK(r.code == 0);
    CHECK(has(r.err, "block 'numbers' not found"));
    CHECK_STR(r.out, "# nothing here\n");
    fx_done(&fx);
}

AVP_TEST_MAIN("ledger")
