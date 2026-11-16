/* The tests of tools/test_pick.py: libraries and filters, the world model, every mode on the synthetic checkout of
 * tools/common/_fixture.py (fixture_data.h, generated from it), the score-history parsing and fallback, the command
 * line (pick.exe from the build), and the real-tree checks. */
#include "avp/test.h"
#include "fixture_data.h"
#include "pick.h"
#include "shsm_symbols.h"

#include <string.h>

/* ---- fixture ---- */

static void fx_write(const char *root, const char *rel, AvpStr text) {
    AvpArena *a = avp_arena();
    const char *p = avp_join(a, root, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(p)), true);
    AVP_ASSERT(avp_write_file(p, text.p, text.n));
}

static AvpJson *jparse(const char *text) {
    return avp_json_parse(avp_arena(), avp_str(text), NULL);
}

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

static const char *report_path(const char *root) { return avp_join(avp_arena(), root, "build", FX_VERSION, "report.json"); }

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
 * fresh folder build/tmp/pick/<name>. */
static const char *make_checkout(const char *name) {
    AvpArena *a = avp_arena();
    const char *root = avp_path_abspath(a, avp_join(a, "build", "tmp", "pick", name));
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
    /* baseline: units[1].functions[0].fuzzy_match_percent = 50.0 */
    AvpJson *b = jparse(FX_REPORT);
    jset(avp_json_at(avp_json_getz(avp_json_at(avp_json_getz(b, "units"), 1), "functions"), 0), "fuzzy_match_percent",
         avp_json_float(a, 50.0));
    fx_write(root, "config/" FX_VERSION "/baseline_report.json", avp_json_dumps(a, b, NULL));
    fx_write(root, "config/" FX_VERSION "/libraries.tsv", avp_str(FX_LIBS));
    fx_write(root, "docs/exceptions.md", avp_str(FX_EXCEPTIONS));
    /* test_pick.make_checkout's add(d) */
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

static PickArgs args_for(const char *mode) {
    PickArgs o;
    pick_args_default(&o);
    o.mode = mode;
    return o;
}

static size_t count_items(const PickItems *it) { return it->n; }
static const char *item_s(const AvpJson *it, const char *k) {
    return avp_cz(avp_arena(), avp_json_get_str(it, k, AVP_S("")));
}

/* ---- TestLibraries ---- */

AVP_TEST(libraries_parse_and_lookup) {
    AvpArena *a = avp_arena();
    ShsmLibraries *libs = shsm_libraries_parse(a, avp_str(FX_LIBS), NULL);
    CHECK_EQ_I64((i64)libs->rows.n, 3);
    CHECK_CZ(shsm_libraries_lookup(a, libs, 0x80001000), "game/G1");
    CHECK_CZ(shsm_libraries_lookup(a, libs, 0x800010FF), "game/G1");
    CHECK_CZ(shsm_libraries_lookup(a, libs, 0x80001100), "sdk/os");
    CHECK(shsm_libraries_lookup(a, libs, 0x80006000) == NULL);
    CHECK(shsm_libraries_lookup(a, libs, 0x80000FFC) == NULL);
    CHECK(shsm_libraries_lookup(a, libs, INT64_MIN) == NULL);
    CHECK_EQ_I64((i64)shsm_libraries_problems(a, libs, true, 0x80001000, 0x80006000).n, 0);
}

AVP_TEST(libraries_problems) {
    AvpArena *a = avp_arena();
    ShsmLibraries *libs = shsm_libraries_parse(a, AVP_S("0x10\t0x20\ta\n0x18\t0x30\tb\n0x40\t0x40\tc\n"), NULL);
    AvpStrVec probs = shsm_libraries_problems(a, libs, true, 0x10, 0x50);
    bool overlap = false, gap1 = false, gap2 = false, empty = false;
    for (size_t i = 0; i < probs.n; i++) {
        overlap |= avp_str_startswith(probs.v[i], AVP_S("overlap"));
        gap1 |= avp_str_startswith(probs.v[i], AVP_S("gap: 0x00000030-0x00000040"));
        gap2 |= avp_str_startswith(probs.v[i], AVP_S("gap: 0x00000040-0x00000050"));
        empty |= avp_str_contains(probs.v[i], AVP_S("empty range"));
    }
    CHECK(overlap && gap1 && gap2 && empty);
}

static bool parse_fails(const char *text) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_libraries_parse(avp_arena(), avp_str(text), NULL);
        avp_try_end(&t);
        return false;
    }
    return t.code == SHSM_EXC_VALUE;
}

AVP_TEST(libraries_bad_rows) {
    CHECK(parse_fails("0x10\t0x20\n"));
    CHECK(parse_fails("zz\t0x20\ta\n"));
}

AVP_TEST(lib_matches) {
    AvpArena *a = avp_arena();
    CHECK(pick_lib_matches(a, "sdk/os", "sdk", NULL));
    CHECK(pick_lib_matches(a, "sdk/os", "os", NULL));
    CHECK(pick_lib_matches(a, "sdk/os", "SDK/OS", NULL));
    CHECK(!pick_lib_matches(a, "sdk/os", "sd", NULL));
    CHECK(!pick_lib_matches(a, "fmod", "sdk", NULL));
    CHECK(pick_lib_matches(a, "crt", "os", "os"));
    CHECK(pick_lib_matches(a, NULL, "unknown", NULL));
    CHECK(pick_lib_matches(a, "fmod", NULL, NULL));
}

AVP_TEST(reference_flag) {
    CHECK_CZ(pick_reference_flag("sdk/os", NULL), "yes");
    CHECK_CZ(pick_reference_flag("sdk/bte", NULL), "yes");
    CHECK_CZ(pick_reference_flag("fmod", NULL), "no");
    CHECK_CZ(pick_reference_flag("game/G3", NULL), "no");
    CHECK_CZ(pick_reference_flag("nonsense", NULL), "?");
}

AVP_TEST(pct) {
    AvpArena *a = avp_arena();
    CHECK_CZ(pick_pct(a, false, 0), "-");
    CHECK_CZ(pick_pct(a, true, 100.0), "100%");
    CHECK_CZ(pick_pct(a, true, 99.99), "99.9%");
    CHECK_CZ(pick_pct(a, true, 12.5), "12.5%");
}

/* ---- TestWorld ---- */

AVP_TEST(world_model) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("world"), NULL);
    REQUIRE(w->fns.n >= 2);
    CHECK_CZ(w->fns.v[0]->name, "fn_80001000__Fv");
    CHECK_CZ(w->fns.v[1]->name, "helper");
    for (size_t i = 0; i < w->fns.n; i++) {
        const PickFn *f = w->fns.v[i];
        if (strcmp(f->name, "fn_80001000__Fv") == 0) CHECK(!strcmp(f->status, "Matching") && !strcmp(f->library, "game/G1"));
        if (strcmp(f->name, "jumper") == 0) CHECK(!strcmp(f->status, "NonMatching") && !strcmp(f->library, "sdk/os"));
        if (strcmp(f->name, "fn_80005000") == 0) CHECK(!strcmp(f->status, "Asm") && !strcmp(f->library, "fmod"));
    }
    CHECK_CZ(pick_world_unit_lib(w, "game/Beta"), "sdk/os");
}

/* ---- TestModes ---- */

AVP_TEST(close_and_candidates) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("close"), NULL);
    PickArgs o = args_for("close");
    PickItems work, cands;
    pick_mode_close(a, w, &o, &work, &cands);
    REQUIRE(work.n == 1);
    CHECK_CZ(item_s(work.v[0], "unit"), "game/Beta");
    CHECK_EQ_I64(avp_json_get_i64(work.v[0], "left", -1), 1);
    CHECK_CZ(item_s(avp_json_at(avp_json_getz(work.v[0], "remaining"), 0), "symbol"), "jumper");
    CHECK_CZ(item_s(work.v[0], "command"), "build/tools/match.exe game/Beta --diff jumper");
    CHECK_EQ_I64((i64)cands.n, 0);

    const char *root = make_checkout("close all exact");
    AvpJson *d = read_report_json(root);
    jset(avp_json_at(avp_json_getz(avp_json_at(avp_json_getz(d, "units"), 1), "functions"), 0), "fuzzy_match_percent",
         avp_json_float(a, 100.0));
    write_report_json(root, d);
    w = pick_world_new(a, root, NULL);
    pick_mode_close(a, w, &o, &work, &cands);
    CHECK_EQ_I64((i64)work.n, 0);
    REQUIRE(cands.n == 1);
    CHECK_CZ(item_s(cands.v[0], "command"), "build/tools/unit.exe flip game/Beta");
}

AVP_TEST(near) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("near"), NULL);
    PickArgs o = args_for("near");
    CHECK_EQ_I64((i64)count_items(&(PickItems){0}) + (i64)pick_mode_near(a, w, &o).n, 0);
    o.threshold = 10;
    PickItems items = pick_mode_near(a, w, &o);
    REQUIRE(items.n == 1);
    CHECK_CZ(item_s(items.v[0], "symbol"), "jumper");
    o.lib = "fmod";
    CHECK_EQ_I64((i64)pick_mode_near(a, w, &o).n, 0);
    o.lib = NULL;
    o.has_min_size = true;
    o.min_size = 0x10;
    CHECK_EQ_I64((i64)pick_mode_near(a, w, &o).n, 0);
}

AVP_TEST(fresh) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("fresh"), NULL);
    PickArgs o = args_for("fresh");
    PickItems items = pick_mode_fresh(a, w, &o, 0);
    REQUIRE(items.n == 2);
    CHECK_CZ(item_s(items.v[0], "symbol"), "fn_80005000");
    CHECK_CZ(item_s(items.v[1], "symbol"), "fn_80005010");
    for (size_t i = 0; i < items.n; i++) CHECK_CZ(item_s(items.v[i], "reference"), "no");
    CHECK_CZ(item_s(items.v[0], "command"), "build/tools/brief.exe fn_80005000");
    CHECK(strstr(item_s(items.v[0], "unit_command"), "--text 0x80005000-0x80005010") != NULL);
    avp_json_set(a, items.v[0], AVP_S("strings"), avp_json_arr(a));
    AvpStr line = pick_fmt_fresh(a, items.v[0]);
    CHECK(avp_str_contains(line, AVP_S("build/tools/brief.exe fn_80005000  then  build/tools/unit.exe new")));
    o.small = true;
    PickItems small = pick_mode_fresh(a, w, &o, 0);
    REQUIRE(small.n);
    CHECK_EQ_I64(avp_json_get_i64(small.v[0], "size", -1), 16);
    o.small = false;
    o.lib = "sdk";
    CHECK_EQ_I64((i64)pick_mode_fresh(a, w, &o, 0).n, 0);
}

AVP_TEST(fresh_call_graph) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("fresh call graph");
    AvpJson *d = read_report_json(root);
    AvpJson *u1 = avp_json_at(avp_json_getz(d, "units"), 1);
    AvpJson *md = avp_json_obj(a);
    jset(md, "auto_generated", avp_json_bool(true));
    jset(u1, "metadata", md);
    jset(u1, "name", avp_json_strz(a, "main/auto_02_80001100_text"));
    write_report_json(root, d);
    PickWorld *w = pick_world_new(a, root, NULL);
    PickArgs o = args_for("fresh");
    PickItems items = pick_mode_fresh(a, w, &o, 0);
    const AvpJson *j = NULL;
    for (size_t i = 0; i < items.n; i++)
        if (strcmp(item_s(items.v[i], "symbol"), "jumper") == 0) j = items.v[i];
    REQUIRE(j);
    CHECK_EQ_I64(avp_json_get_i64(j, "callees", -1), 2);
    CHECK_EQ_I64(avp_json_get_i64(j, "callees_matched", -1), 2);
    CHECK(!avp_json_get_bool(j, "leaf", true));
    CHECK_EQ_I64(avp_json_get_i64(j, "rows", -1), 2);
    o.leaf = true;
    PickItems leaf = pick_mode_fresh(a, w, &o, 0);
    bool found = false;
    for (size_t i = 0; i < leaf.n; i++) found |= strcmp(item_s(leaf.v[i], "symbol"), "jumper") == 0;
    CHECK(found);
}

AVP_TEST(pending) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("pending"), NULL);
    PickArgs o = args_for("pending");
    PickItems items = pick_mode_pending(a, w, &o);
    REQUIRE(items.n == 1);
    CHECK_CZ(item_s(items.v[0], "unit"), "game/Alpha");
    CHECK_CZ(item_s(items.v[0], "symbol"), "fn_80001000__Fv");
    const AvpJson *rules = avp_json_getz(items.v[0], "rules");
    REQUIRE(avp_json_len(rules) == 1);
    CHECK_STR(avp_json_at(rules, 0)->u.str, "R17");
    CHECK(avp_json_get_bool(items.v[0], "linked", false));
    CHECK(strstr(item_s(items.v[0], "command"), "check_rules.exe --units main/game/Alpha") != NULL);
}

AVP_TEST(drafts) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("drafts");
    PickWorld *w = pick_world_new(a, root, NULL);
    PickArgs o = args_for("drafts");
    const char *note = NULL;
    PickItems items = pick_mode_drafts(a, w, &o, &note);
    CHECK_EQ_I64((i64)items.n, 0);
    CHECK(note && strstr(note, "build/tools/draft.exe sweep"));
    const char *ix = avp_join(a, w->repo->index_dir, "asm.sqlite");
    fx_write(root, avp_join(a, "build", "index", "asm.sqlite"), AVP_S(""));
    fx_write(root, avp_join(a, "build", "draft", "sweep.json"),
             AVP_S("{\"results\": ["
                   "{\"symbol\": \"fn_80009000\", \"lib\": \"game/G1\", \"verdict\": \"MATCH\", \"twin\": \"t1\", "
                   "\"unit_path\": \"game/A.cpp\"}, "
                   "{\"symbol\": \"fn_80009100\", \"lib\": \"game/G1\", \"verdict\": \"PARTIAL\", \"twin\": \"t2\"}, "
                   "{\"symbol\": \"fn_80001000__Fv\", \"lib\": \"game/G1\", \"verdict\": \"MATCH\", \"twin\": \"t3\"}]}"));
    i64 now_ns = (i64)(avp_time() * 1e9);
    AVP_ASSERT(avp_utime_ns(ix, now_ns - (i64)100e9, now_ns - (i64)100e9));
    items = pick_mode_drafts(a, w, &o, &note);
    CHECK(note == NULL);
    REQUIRE(items.n == 1);
    CHECK_CZ(item_s(items.v[0], "symbol"), "fn_80009000");
    CHECK_CZ(item_s(items.v[0], "command"), "build/tools/draft.exe twin fn_80009000");
    AVP_ASSERT(avp_utime_ns(ix, now_ns + (i64)100e9, now_ns + (i64)100e9));
    items = pick_mode_drafts(a, w, &o, &note);
    CHECK(note && strstr(note, "older than the asm index"));
    CHECK_EQ_I64((i64)items.n, 1);
}

AVP_TEST(lost_unavailable_without_history) {
    AvpArena *a = avp_arena();
    PickWorld *w = pick_world_new(a, make_checkout("lost"), NULL);
    PickArgs o = args_for("lost");
    const char *note = NULL;
    PickItems items = pick_mode_lost(a, w, &o, &note);
    CHECK_EQ_I64((i64)items.n, 0);
    CHECK(note && strstr(note, "history unavailable"));
}

/* ---- TestHistory ---- */

AVP_TEST(parse_lost_text) {
    PickItems rows = pick_parse_lost_text(avp_arena(), avp_str(FX_HISTORY_TEXT));
    REQUIRE(rows.n == 1);
    CHECK_CZ(item_s(rows.v[0], "symbol"), "jumper");
    CHECK_EQ_F64(avp_json_get_f64(rows.v[0], "historical", 0), 50.0);
    CHECK(avp_str_startswith(avp_json_get_str(rows.v[0], "recovery", AVP_S("")),
                             AVP_S("git show abc123def456:src/game/Beta.c")));
}

typedef struct {
    int rc[2];
    const char *out[2], *err[2];
    int calls;
    bool json_first;
} FakeHistory;

static int fake_runner(void *ctx, AvpArena *a, int argc, const char *const *argv, AvpBuf *out, AvpBuf *err) {
    (void)a;
    FakeHistory *h = ctx;
    int k = h->calls++;
    if (k == 0) h->json_first = argc > 0 && strcmp(argv[argc - 1], "--json") == 0;
    avp_buf_putz(out, h->out[k]);
    avp_buf_putz(err, h->err[k]);
    return h->rc[k];
}

AVP_TEST(json_then_text_fallback) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("history");
    fx_write(root, "build/score_history.sqlite", AVP_S(""));
    PickWorld *w = pick_world_new(a, root, NULL);
    PickArgs o = args_for("lost");
    const char *note = NULL;
    FakeHistory h = {{0, 0},
                     {"[{\"unit\": \"main/game/Beta\", \"symbol\": \"jumper\", \"address\": \"0x80001100\", "
                      "\"current\": 12.5, \"best\": 12.5, \"historical\": 50.0, \"lost\": true, \"historical_kind\": "
                      "\"git\", \"historical_key\": \"abc123def4567\", \"historical_path\": \"src/game/Beta.c\"}]",
                      ""},
                     {"", ""}, 0, false};
    pick_set_history_runner(fake_runner, &h);
    PickItems items = pick_mode_lost(a, w, &o, &note);
    CHECK(note == NULL);
    REQUIRE(items.n == 1);
    CHECK_CZ(item_s(items.v[0], "command"), "git show abc123def456:src/game/Beta.c");
    CHECK_EQ_I64(avp_json_get_i64(items.v[0], "size", -1), 8);
    CHECK_CZ(item_s(items.v[0], "library"), "sdk/os");
    CHECK(h.json_first);
    /* --json refused (an older score_history): falls back to the text form */
    FakeHistory h2 = {{2, 0}, {"", FX_HISTORY_TEXT}, {"unrecognized arguments: --json", ""}, 0, false};
    pick_set_history_runner(fake_runner, &h2);
    items = pick_mode_lost(a, w, &o, &note);
    REQUIRE(items.n == 1);
    CHECK_CZ(item_s(items.v[0], "symbol"), "jumper");
    /* both fail: degrade, do not raise */
    FakeHistory h3 = {{1, 1}, {"", ""}, {"boom", "boom"}, 0, false};
    pick_set_history_runner(fake_runner, &h3);
    items = pick_mode_lost(a, w, &o, &note);
    pick_set_history_runner(NULL, NULL);
    CHECK_EQ_I64((i64)items.n, 0);
    CHECK(note && strstr(note, "history unavailable"));
    CHECK(note && strstr(note, "exit 1: boom"));
}

/* ---- TestCli (pick.exe next to the tests folder) ---- */

static AvpProcResult run_pick(const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **argv = AVP_NEWN(a, const char *, n + 2);
    argv[0] = avp_join(a, avp_exe_dir(), "..", "pick.exe");
    for (size_t i = 0; i < n; i++) argv[i + 1] = args[i];
    return avp_run_text(a, NULL, argv);
}

AVP_TEST(cli_summary_and_json) {
    const char *root = make_checkout("cli");
    AvpProcResult r = run_pick((const char *[]){"--root", root}, 2);
    CHECK_EQ_I64(r.returncode, 0);
    const char *heads[] = {"== lost", "== close: 1", "== near", "== fresh: 2", "== pending: 1"};
    for (size_t i = 0; i < 5; i++) CHECK_MSG(avp_str_contains(r.out, avp_str(heads[i])), "%s", heads[i]);
    r = run_pick((const char *[]){"--root", root, "--json"}, 3);
    AvpJson *d = avp_json_parse(avp_arena(), r.out, NULL);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_getz(d, "fresh"), "count", -1), 2);
    const AvpJson *units = avp_json_getz(avp_json_getz(d, "close"), "units");
    CHECK_CZ(item_s(avp_json_at(avp_json_getz(units, "items"), 0), "unit"), "game/Beta");
}

AVP_TEST(cli_cap) {
    const char *root = make_checkout("cli cap");
    AvpProcResult r = run_pick((const char *[]){"--root", root, "fresh", "--limit", "1"}, 5);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("... 1 more")));
    r = run_pick((const char *[]){"--root", root, "fresh", "--limit", "1", "--json"}, 6);
    AvpJson *d = avp_json_parse(avp_arena(), r.out, NULL);
    CHECK_EQ_I64((i64)avp_json_len(avp_json_getz(avp_json_getz(d, "fresh"), "items")), 1);
}

static void age_report(const char *root, bool older) {
    AvpArena *a = avp_arena();
    double now = 0;
    AVP_ASSERT(avp_getmtime(avp_join(a, root, "configure.py"), &now));
    i64 t = (i64)((older ? now - 1000 : now + 1000) * 1e9);
    AVP_ASSERT(avp_utime_ns(report_path(root), t, t));
}

AVP_TEST(cli_stale_report_note) {
    const char *root = make_checkout("cli stale");
    age_report(root, true);
    AvpProcResult r = run_pick((const char *[]){"--root", root, "fresh"}, 3);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("older than configure.py")));
    CHECK(avp_str_contains(r.out, AVP_S("tools/bin/ninja.exe")));
    CHECK(avp_str_contains(r.out, AVP_S("--report config/" FX_VERSION "/baseline_report.json")));
    age_report(root, false);
    r = run_pick((const char *[]){"--root", root, "fresh"}, 3);
    CHECK(!avp_str_contains(r.out, AVP_S("older than")));
}

AVP_TEST(cli_no_report_is_error) {
    const char *root = make_checkout("cli no report");
    AVP_ASSERT(avp_remove(report_path(root)));
    AvpProcResult r = run_pick((const char *[]){"--root", root, "close"}, 3);
    CHECK_EQ_I64(r.returncode, 2);
}

/* ---- real tree ---- */

AVP_TEST(real_libraries_cover_text) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    if (!avp_isfile(r->libraries_tsv)) return;
    ShsmLibraries *libs = shsm_libraries_load(a, r);
    CHECK(libs->rows.n > 0);
    const ShsmSymbolTable *st = shsm_load_symbols(r, false);
    i64 start = INT64_MAX, end = 0;
    for (size_t i = 0; i < st->n; i++) {
        const ShsmSymbol *s = &st->symbols[i];
        if (strcmp(s->section, ".text") != 0 || !s->has_size || !s->size) continue;
        if ((i64)s->address < start) start = (i64)s->address;
        if ((i64)(s->address + s->size) > end) end = (i64)(s->address + s->size);
        CHECK_MSG(shsm_libraries_lookup(a, libs, (i64)s->address) != NULL, "%s", s->name);
    }
    AvpStrVec probs = shsm_libraries_problems(a, libs, true, start, end);
    CHECK_EQ_I64((i64)probs.n, 0);
}

AVP_TEST(real_references_sections_exist) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    const char *path = avp_join(a, r->root, "docs", "references.md");
    AvpStr text = {0};
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) return;
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    size_t n = 0;
    const PickReference *refs = pick_references(&n);
    for (size_t i = 0; i < n; i++) {
        if (!refs[i].section) continue;
        bool found = false;
        for (size_t k = 0; k < lines.n && !found; k++) {
            if (!avp_str_startswith(lines.v[k], AVP_S("#"))) continue;
            AvpStr h = avp_str_strip(avp_str_strip_chars(lines.v[k], AVP_S("#")));
            found = avp_str_startswith(h, avp_str(refs[i].section));
        }
        CHECK_MSG(found, "%s: %s", refs[i].library, refs[i].section);
    }
}

AVP_TEST(real_smoke) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    if (!avp_isfile(r->report_json)) return;
    PickWorld *w = pick_world_new(a, NULL, NULL);
    for (size_t i = 0; i < w->fns.n; i++) CHECK(w->fns.v[i]->library != NULL);
    PickArgs o = args_for("close");
    PickItems work, cands;
    pick_mode_close(a, w, &o, &work, &cands);
    for (size_t i = 0; i < work.n; i++) CHECK(avp_json_get_i64(work.v[i], "left", 0) > 0);
    o = args_for("near");
    PickItems near = pick_mode_near(a, w, &o);
    for (size_t i = 0; i < near.n; i++) {
        double p = avp_json_get_f64(near.v[i], "percent", 0);
        CHECK(p >= 90.0 && p < 100.0);
    }
    o = args_for("pending");
    PickItems pend = pick_mode_pending(a, w, &o);
    for (size_t i = 0; i < pend.n; i++) CHECK(*item_s(pend.v[i], "symbol"));
}

AVP_TEST(real_summary_runs) {
    const ShsmRepo *r = shsm_repo_find(NULL);
    if (!avp_isfile(r->report_json)) return;
    AvpProcResult p = run_pick((const char *[]){"--root", r->root}, 2);
    CHECK_EQ_I64(p.returncode, 0);
    CHECK(avp_str_contains(p.out, AVP_S("== close")));
}

AVP_TEST_MAIN("pick")
