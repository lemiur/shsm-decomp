/* The tests of tools/test_status.py on the synthetic checkout of tools/test_pick.py (tools/common/_fixture.py,
 * pick's fixture_data.h, plus libraries.tsv), the command line (status.exe from the build), and the real-tree
 * checks; plus the checker bridge's failure note through a replaced runner. */
#include "../../pick/tests/fixture_data.h"
#include "avp/test.h"
#include "shsm_re.h"
#include "status.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- fixture (as pick_test.c's) ---- */

static void fx_write(const char *root, const char *rel, AvpStr text) {
    AvpArena *a = avp_arena();
    const char *p = avp_join(a, root, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(p)), true);
    AVP_ASSERT(avp_write_file(p, text.p, text.n));
}

static AvpJson *jparse(const char *text) { return avp_json_parse(avp_arena(), avp_str(text), NULL); }

static void jset(AvpJson *o, const char *k, AvpJson *v) { avp_json_set(avp_arena(), o, avp_str(k), v); }

static AvpJson *fn_entry(const char *name, const char *size, u64 va) {
    AvpArena *a = avp_arena();
    AvpJson *f = avp_json_obj(a), *md = avp_json_obj(a);
    jset(f, "name", avp_json_strz(a, name));
    jset(f, "size", avp_json_strz(a, size));
    jset(md, "virtual_address", avp_json_str(a, avp_pyfmt(a, "%d", (i64)va)));
    jset(f, "metadata", md);
    return f;
}

static const char *report_path(const char *root) {
    return avp_join(avp_arena(), root, "build", FX_VERSION, "report.json");
}

static AvpJson *read_report_json(const char *root) {
    AvpStr t = {0};
    AVP_ASSERT(avp_read_text(avp_arena(), report_path(root), AVP_TM_UTF8, &t));
    return avp_json_parse(avp_arena(), t, NULL);
}

static void write_report_json(const char *root, const AvpJson *d) {
    AvpStr t = avp_json_dumps(avp_arena(), d, NULL);
    fx_write(root, avp_join(avp_arena(), "build", FX_VERSION, "report.json"), t);
}

/* _fixture.make_checkout() plus test_pick.make_checkout()'s libraries.tsv, exceptions table and auto unit, in a
 * fresh folder build/tmp/status/<name>. */
static const char *make_checkout(const char *name) {
    AvpArena *a = avp_arena();
    const char *root = avp_path_abspath(a, avp_join(a, "build", "tmp", "status", name));
    if (avp_isdir(root)) avp_rmtree(root, 0);
    AVP_ASSERT(avp_makedirs(root, true));
    fx_write(root, "configure.py", avp_str(FX_CONFIGURE));
    fx_write(root, "tools/.keep", AVP_S(""));
    fx_write(root, "config/" FX_VERSION "/config.yml", AVP_S("object_base: orig/" FX_VERSION "\nobject: sys/main.dol\n"));
    fx_write(root, "config/" FX_VERSION "/symbols.txt", avp_str(FX_SYMBOLS));
    fx_write(root, "config/" FX_VERSION "/splits.txt", avp_str(FX_SPLITS));
    fx_write(root, "build/" FX_VERSION "/asm/game/Alpha.s", avp_str(FX_ALPHA_S));
    fx_write(root, "build/" FX_VERSION "/asm/game/Beta.s", avp_str(FX_BETA_S));
    fx_write(root, "objdiff.json", avp_str(FX_OBJDIFF));
    fx_write(root, "build.ninja", avp_str(FX_NINJA));
    AvpJson *rep = jparse(FX_REPORT);
    AvpJson *units = avp_json_getz(rep, "units");
    AvpJson *b = jparse(FX_REPORT);
    jset(avp_json_at(avp_json_getz(avp_json_at(avp_json_getz(b, "units"), 1), "functions"), 0), "fuzzy_match_percent",
         avp_json_float(a, 50.0));
    fx_write(root, "config/" FX_VERSION "/baseline_report.json", avp_json_dumps(a, b, NULL));
    fx_write(root, "config/" FX_VERSION "/libraries.tsv", avp_str(FX_LIBS));
    fx_write(root, "docs/exceptions.md", avp_str(FX_EXCEPTIONS));
    jset(avp_json_at(avp_json_getz(avp_json_at(units, 0), "functions"), 1), "fuzzy_match_percent",
         avp_json_float(a, 100.0));
    AvpJson *md = avp_json_obj(a);
    avp_json_set(a, md, AVP_S("virtual_address"), avp_json_str(a, avp_pyfmt(a, "%d", (i64)0x80001100)));
    jset(avp_json_at(avp_json_getz(avp_json_at(units, 1), "functions"), 0), "metadata", md);
    AvpJson *au = avp_json_obj(a), *amd = avp_json_obj(a), *fl = avp_json_arr(a);
    jset(au, "name", avp_json_strz(a, "main/auto_01_80005000_text"));
    jset(au, "measures", avp_json_obj(a));
    jset(amd, "auto_generated", avp_json_bool(true));
    jset(au, "metadata", amd);
    avp_json_arr_push(a, fl, fn_entry("fn_80005010", "64", 0x80005010));
    avp_json_arr_push(a, fl, fn_entry("fn_80005000", "16", 0x80005000));
    avp_json_arr_push(a, fl, fn_entry("pad_01_80005050_text", "4", 0x80005050));
    jset(au, "functions", fl);
    avp_json_arr_push(a, units, au);
    write_report_json(root, rep);
    return root;
}

static void age_report(const char *root, bool older) {
    AvpArena *a = avp_arena();
    double now = 0;
    AVP_ASSERT(avp_getmtime(avp_join(a, root, "configure.py"), &now));
    i64 t = (i64)((older ? now - 1000 : now + 1000) * 1e9);
    AVP_ASSERT(avp_utime_ns(report_path(root), t, t));
}

static AvpProcResult run_status(const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **argv = AVP_NEWN(a, const char *, n + 2);
    argv[0] = avp_join(a, avp_exe_dir(), "..", "status.exe");
    for (size_t i = 0; i < n; i++) argv[i + 1] = args[i];
    return avp_run_text(a, NULL, argv);
}

static const StatusRow *row_named(const StatusRows *rows, const char *name) {
    for (size_t i = 0; i < rows->n; i++)
        if (strcmp(rows->v[i]->library, name) == 0) return rows->v[i];
    return NULL;
}

static void check_sums(const StatusRow *r) {
    i64 fn = 0, by = 0;
    fn = r->asm_n, by = r->asm_b;
    for (int k = 0; k < STATUS_NKINDS; k++) fn += r->n[k], by += r->b[k];
    CHECK_EQ_I64(r->functions, fn);
    CHECK_EQ_I64(r->bytes, by);
}

/* ---- TestStale ---- */

AVP_TEST(stale_report_note) {
    const char *root = make_checkout("stale");
    age_report(root, true);
    AvpProcResult r = run_status((const char *[]){"--root", root}, 2);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("older than configure.py")));
    CHECK(avp_str_contains(r.out, AVP_S("tools/bin/ninja.exe")));
    CHECK(avp_str_contains(r.out, AVP_S("--report config/" FX_VERSION "/baseline_report.json")));
    age_report(root, false);
    r = run_status((const char *[]){"--root", root}, 2);
    CHECK(!avp_str_contains(r.out, AVP_S("older than")));
}

/* ---- TestTable ---- */

AVP_TEST(table_counts) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("table"), NULL);
    StatusRows rows;
    StatusRow *total;
    status_library_table(a, w, NULL, false, &rows, &total);
    CHECK_EQ_I64((i64)rows.n, 3);
    const StatusRow *game = row_named(&rows, "game"), *sdk = row_named(&rows, "sdk"), *fmod = row_named(&rows, "fmod");
    REQUIRE(game && sdk && fmod);
    CHECK_EQ_I64(game->n[0], 2);
    CHECK_EQ_I64(game->b[0], 24);
    CHECK_EQ_I64(sdk->n[2], 1);
    CHECK_EQ_I64(sdk->b[2], 8);
    CHECK_EQ_I64(fmod->n[3], 3); /* pad_ included */
    CHECK_EQ_I64(fmod->b[3], 84);
    CHECK_EQ_I64(total->functions, 6);
    i64 bytes = 0;
    for (size_t i = 0; i < rows.n; i++) bytes += rows.v[i]->bytes, check_sums(rows.v[i]);
    CHECK_EQ_I64(total->bytes, bytes);
    check_sums(total);
}

AVP_TEST(table_exact_unlinked_and_detail) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("exact");
    AvpJson *d = read_report_json(root);
    jset(avp_json_at(avp_json_getz(avp_json_at(avp_json_getz(d, "units"), 1), "functions"), 0), "fuzzy_match_percent",
         avp_json_float(a, 100.0));
    write_report_json(root, d);
    PickWorld *w = pick_world_new(a, root, NULL);
    StatusRows rows;
    StatusRow *total;
    status_library_table(a, w, NULL, true, &rows, &total);
    const StatusRow *os = row_named(&rows, "sdk/os");
    REQUIRE(os != NULL);
    CHECK_EQ_I64(os->n[1], 1);
    CHECK(row_named(&rows, "game/G1") != NULL);
    status_library_table(a, w, "os", false, &rows, &total);
    REQUIRE(rows.n == 1);
    CHECK_CZ(rows.v[0]->library, "sdk/os");
    StatusJsonVec ur = status_unit_rows(a, w, NULL, NULL);
    bool beta = false, alpha = false;
    for (size_t i = 0; i < ur.n; i++) {
        AvpStr u = avp_json_get_str(ur.v[i], "unit", AVP_S(""));
        const AvpJson *flags = avp_json_getz(ur.v[i], "flags");
        if (avp_str_eqz(u, "game/Beta")) {
            beta = true;
            bool found = false;
            for (size_t k = 0; k < avp_json_len(flags); k++)
                found |= avp_str_eqz(avp_json_str_or(avp_json_at(flags, k), AVP_S("")), "layout unverified");
            CHECK(found);
        } else if (avp_str_eqz(u, "game/Alpha")) {
            alpha = true;
            CHECK_EQ_I64((i64)avp_json_len(flags), 0);
        }
    }
    CHECK(beta && alpha);
}

/* ---- TestHeadline ---- */

AVP_TEST(headline_ledger_model) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("headline"), NULL);
    StatusHeadline *h = status_headline(a, w, w->report_path, NULL, true);
    /* Alpha (2 functions, 24 bytes) is linked; its fn_80001000__Fv is MATCH-PENDING in exceptions.md */
    CHECK_EQ_I64(h->linked[0], 2);
    CHECK_EQ_I64(h->linked[1], 24);
    CHECK_EQ_I64(h->rules[0], 1);
    CHECK_EQ_I64(h->rules[2], 8);
    CHECK(h->note && strstr(h->note, "without the rule checker"));
    CHECK_EQ_I64((i64)avp_json_len(h->ledger), 0);
}

static StatusCheckerRun failing_checker(void *ctx, AvpArena *a, const char *root) {
    (void)ctx, (void)a, (void)root;
    StatusCheckerRun r = {0};
    r.returncode = 3;
    r.err = AVP_S("  Traceback: boom  \n");
    return r;
}

static StatusCheckerRun garbage_checker(void *ctx, AvpArena *a, const char *root) {
    (void)ctx, (void)a, (void)root;
    StatusCheckerRun r = {0};
    r.returncode = 1;
    r.out = AVP_S("not json");
    return r;
}

AVP_TEST(headline_checker_unavailable) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("checker");
    PickWorld *w = pick_world_new(a, root, NULL);
    status_set_checker_runner(failing_checker, NULL);
    StatusHeadline *h = status_headline(a, w, w->report_path, NULL, false);
    CHECK(h->note && strstr(h->note, "rule checker unavailable (rule checker failed (exit 3): Traceback: boom)"));
    status_set_checker_runner(garbage_checker, NULL);
    h = status_headline(a, w, w->report_path, NULL, false);
    CHECK(h->note && strstr(h->note, "rule checker unavailable (rule checker printed no JSON: Expecting value"));
    status_set_checker_runner(NULL, NULL);
    CHECK_EQ_I64(h->linked[0], 2);
}

AVP_TEST(headline_ledger_block) {
    AvpArena *a = avp_arena();
    const char *root = avp_path_abspath(a, avp_join(a, "build", "tmp", "status", "block"));
    if (avp_isdir(root)) avp_rmtree(root, 0);
    fx_write(root, "docs/progress.md",
             AVP_S("x\n<!-- ledger:begin numbers -->\n| measure | functions | code bytes |\n|---|---|---|\n"
                   "| **matched under the rules** (headline) | 2883 / 13947 | 754656 / 3988520 |\n"
                   "| bytes matched (linked from source, excluding `ORIGINAL-ASM`) | 2888 / 13947 | 756676 / 3988520 |\n"
                   "| `ORIGINAL-ASM` (linked, listed apart, outside both counts and totals) | 88 | 14444 |\n"
                   "| linked from source in total (objdiff \"complete\") | 2976 | 771120 |\n"
                   "| `Matching` units | 326 / 979 | |\n<!-- ledger:end numbers -->\n"));
    AvpJson *b = status_ledger_block(a, root);
    REQUIRE(b != NULL);
    AvpStr s = avp_json_dumps(a, b, NULL);
    CHECK_STR(s, ("{\"rules\": [2883, 13947, 754656, 3988520], \"bytes\": [2888, 13947, 756676, 3988520], "
                       "\"asm\": [88, 14444], \"linked\": [2976, 771120], \"units\": [326, 979]}"));
    CHECK(status_ledger_block(a, avp_join(a, root, "nosuch")) == NULL);
}

/* ---- TestSince ---- */

static bool has_symbol(const AvpJson *list, const char *sym) {
    for (size_t i = 0; i < avp_json_len(list); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(list, i), "symbol", AVP_S("")), sym)) return true;
    return false;
}

AVP_TEST(since_changes) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("since");
    PickWorld *w = pick_world_new(a, root, NULL);
    ShsmReport *old = shsm_load_baseline(w->repo); /* jumper 50% there, 12.5% now; no auto functions */
    AvpJson *res = status_since(a, w, old, NULL);
    const AvpJson *reg = avp_json_getz(res, "regressed");
    CHECK_EQ_I64((i64)avp_json_len(reg), 1);
    CHECK(has_symbol(reg, "jumper"));
    CHECK(has_symbol(avp_json_getz(res, "gained"), "helper"));
    CHECK_EQ_I64((i64)avp_json_len(avp_json_getz(res, "added")), 3);
    CHECK_EQ_I64((i64)avp_json_len(avp_json_getz(res, "flipped_on")), 0);
    const char *base = avp_join(a, root, "config", FX_VERSION, "baseline_report.json");
    AvpProcResult r = run_status((const char *[]){"--root", root, "--since", base}, 4);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("regressed")));
}

AVP_TEST(since_bad_ref) {
    AvpProcResult r = run_status((const char *[]){"--root", make_checkout("badref"), "--since", "no-such-rev-xyz"}, 4);
    CHECK_EQ_I64(r.returncode, 2);
}

/* ---- TestSvgAndCli ---- */

static AvpStr xml_unescape(AvpArena *a, AvpStr s) {
    s = avp_str_replace(a, s, AVP_S("&lt;"), AVP_S("<"), -1);
    s = avp_str_replace(a, s, AVP_S("&gt;"), AVP_S(">"), -1);
    return avp_str_replace(a, s, AVP_S("&amp;"), AVP_S("&"), -1);
}

static double attr_f(AvpStr tag, const char *name, double dflt) {
    AvpArena *a = avp_arena();
    ShsmReMatch m;
    const ShsmRe *re = shsm_re_try(avp_cz(a, avp_pyfmt(a, " %s=\"([^\"]*)\"", name)), 0);
    if (!shsm_re_search(re, tag, 0, AVP_END, &m)) return dflt;
    double v = dflt;
    avp_parse_float(shsm_re_group(&m, 1), &v);
    return v;
}

AVP_TEST(svg) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("svg");
    const char *out = avp_join(a, root, "chart.svg");
    AvpProcResult r = run_status((const char *[]){"--root", root, "--no-checker", "--svg", out}, 5);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("wrote")));
    AvpStr body = {0};
    REQUIRE(avp_read_text(a, out, AVP_TM_UTF8, &body));
    CHECK(avp_str_startswith(body, AVP_S("<svg xmlns=\"http://www.w3.org/2000/svg\"")));
    CHECK(avp_str_endswith(body, AVP_S("</svg>\n")));
    const char *bad[] = {"<script", "@import", "url(", "href="};
    for (int i = 0; i < 4; i++) CHECK_MSG(!avp_str_contains(body, avp_str(bad[i])), "%s", bad[i]);
    CHECK(avp_str_contains(body, AVP_S("matched under the rules")));
    /* layout: every text (by its estimated extent) inside the view box */
    ShsmReMatch m;
    REQUIRE(shsm_re_search(shsm_re("<svg [^>]*>", 0), body, 0, AVP_END, &m));
    AvpStr svgtag = shsm_re_group(&m, 0);
    double width = attr_f(svgtag, "width", 0), dsize = attr_f(svgtag, "font-size", 0);
    CHECK(width > 0 && dsize == 12);
    ShsmReIter it;
    shsm_re_iter_init(&it, shsm_re("(<text[^>]*>)([^<]*)</text>", 0), body, 0, AVP_END);
    size_t n = 0;
    while (shsm_re_iter_next(&it, &m)) {
        AvpStr tag = shsm_re_group(&m, 1), text = xml_unescape(a, shsm_re_group(&m, 2));
        double size = attr_f(tag, "font-size", dsize);
        bool bold = avp_str_contains(tag, AVP_S("font-weight=\"700\""));
        double tw = status_text_w(text, size, bold);
        CHECK_MSG(attr_f(tag, "x", 0) + tw <= width, "%.*s", AVP_SF(text));
        n++;
    }
    CHECK(n > 5);
}

static double lum(const char *c) {
    double v[3];
    for (int i = 0; i < 3; i++) {
        char hex[3] = {c[1 + 2 * i], c[2 + 2 * i], 0};
        double x = (double)strtol(hex, NULL, 16) / 255.0;
        v[i] = x <= 0.03928 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
    }
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];
}

AVP_TEST(svg_contrast) {
    for (int k = 0; k < STATUS_NKINDS; k++)
        CHECK_MSG(1.05 / (lum(STATUS_SVG_COLORS[k]) + 0.05) >= 3.0, "%s", STATUS_SVG_COLORS[k]); /* bars on white */
    for (int k = 0; k < 3; k++)
        CHECK_MSG(1.05 / (lum(STATUS_SVG_TEXT_COLORS[k]) + 0.05) >= 4.5, "%s", STATUS_SVG_TEXT_COLORS[k]);
}

AVP_TEST(human) {
    AvpArena *a = avp_arena();
    CHECK_STR(status_human(a, 1023), "1023 B");
    CHECK_STR(status_human(a, 1536), "2 KiB");
    CHECK_STR(status_human(a, 3 << 20), "3.0 MiB");
}

AVP_TEST(json_and_units) {
    const char *root = make_checkout("json");
    AvpProcResult r = run_status((const char *[]){"--root", root, "--no-checker", "--json"}, 4);
    CHECK_EQ_I64(r.returncode, 0);
    AvpJson *d = avp_json_parse(avp_arena(), r.out, NULL);
    REQUIRE(d != NULL);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_getz(d, "total"), "functions", -1), 6);
    AvpStr linked = avp_json_dumps(avp_arena(),
                                   avp_json_getz(avp_json_getz(avp_json_getz(d, "headline"), "computed"), "linked"), NULL);
    CHECK_STR(linked, "[2, 24]");
    r = run_status((const char *[]){"--root", root, "--units"}, 3);
    CHECK(avp_str_contains(r.out, AVP_S("game/Alpha")));
    CHECK(avp_str_contains(r.out, AVP_S("MATCH-PENDING x1")));
}

/* ---- TestRealSmoke ---- */

static i64 measure(const AvpJson *m, const char *k) {
    const AvpJson *v = avp_json_getz(m, k);
    if (!v) return 0;
    if (avp_json_is_str(v)) {
        i64 i = 0;
        avp_parse_int(avp_json_str_or(v, AVP_S("0")), 10, &i, NULL);
        return i;
    }
    return avp_json_i64(v, 0);
}

AVP_TEST(real_totals_agree_with_report) {
    AvpArena *a = avp_arena();
    const ShsmRepo *repo = shsm_repo_find(NULL);
    if (!avp_isfile(repo->report_json)) return;
    PickWorld *w = pick_world_new(a, NULL, NULL);
    StatusRows rows;
    StatusRow *total;
    status_library_table(a, w, NULL, false, &rows, &total);
    const AvpJson *m = w->report->measures;
    CHECK_EQ_I64(total->functions, measure(m, "total_functions"));
    CHECK_EQ_I64(total->bytes, measure(m, "total_code"));
    CHECK_EQ_I64(total->b[0] + total->asm_b, measure(m, "complete_code"));
    CHECK_EQ_I64(total->b[0] + total->asm_b + total->b[1], measure(m, "matched_code"));
    CHECK_EQ_I64(total->n[0] + total->asm_n + total->n[1], measure(m, "matched_functions"));
}

/* The per-library table lists ORIGINAL-ASM apart, from the ledger's sources (exceptions.md rows and `.s` units), and
 * agrees with the ledger's own count on the current tree. */
AVP_TEST(real_original_asm_agrees_with_ledger) {
    AvpArena *a = avp_arena();
    const ShsmRepo *repo = shsm_repo_find(NULL);
    if (!avp_isfile(repo->report_json)) return;
    PickWorld *w = pick_world_new(a, NULL, NULL);
    StatusRows rows;
    StatusRow *total;
    status_library_table(a, w, NULL, false, &rows, &total);
    StatusHeadline *h = status_headline(a, w, w->report_path, NULL, true);
    CHECK_EQ_I64(total->asm_n, h->asm_[0]);
    CHECK_EQ_I64(total->asm_b, h->asm_[1]);
    AvpStrVec t = status_fmt_table(a, &rows, total);
    CHECK(avp_str_contains(t.v[0], AVP_S(" asm ")));
    status_library_table(a, w, "renderware", false, &rows, &total);
    CHECK(total->asm_n >= 1); /* skinmatrixblend.s */
    CHECK(avp_str_contains(avp_json_dumps(a, status_row_json(a, total), NULL), AVP_S("\"original_asm\"")));
}

AVP_TEST(real_units_runs) {
    const ShsmRepo *repo = shsm_repo_find(NULL);
    if (!avp_isfile(repo->report_json)) return;
    AvpProcResult r = run_status((const char *[]){"--root", repo->root, "--units", "--lib", "os"}, 5);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("revolution/os/")));
}

AVP_TEST_MAIN("status")
