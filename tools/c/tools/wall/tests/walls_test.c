/* Tests of build/walls.json, the notes and the real tree: tools/test_wall.py TestWallsJson.test_stale_units_dropped,
 * TestNotes.test_assert_text_is_not_a_note and TestRealTree. (TestProof is covered by cli_test.c's
 * wants_and_apply_proof and proof_errors.) The real-tree tests return without checks, passing, when the build
 * outputs they need are absent, as the Python skips. */
#include "avp/fs.h"
#include "avp/json.h"
#include "avp/test.h"
#include "cc_compare.h"
#include "match.h"
#include "shsm_cli.h"
#include "wall.h"
#include "../../../tests/shsm_test_util.h"

#include <stdlib.h>

/* ---- the synthetic checkout (tools/common/_fixture.py, trimmed to what the unit table and the staleness test read) */

static const char FX_CONFIGURE[] = "DEFAULT_VERSION = 0\n"
                                   "VERSIONS = [\n"
                                   "    \"TESTV1\",  # 0: test\n"
                                   "]\n"
                                   "config.libs = [\n"
                                   "    {\n"
                                   "        \"lib\": \"game\",\n"
                                   "        \"objects\": [\n"
                                   "            Object(Matching, \"game/Alpha.cpp\"),\n"
                                   "            Object(\n"
                                   "                NonMatching,\n"
                                   "                \"game/Beta.c\",\n"
                                   "                extra_cflags=[\"-O0\"],\n"
                                   "            ),\n"
                                   "        ],\n"
                                   "    },\n"
                                   "]\n";

static const char FX_SYMBOLS[] =
    "fn_80001000__Fv = .text:0x80001000; // type:function size:0x10 scope:global\n"
    "jumper = .text:0x80001100; // type:function size:0x8 scope:global\n";

static const char FX_OBJDIFF[] =
    "{\"units\": [{\"name\": \"main/game/Alpha\", \"target_path\": \"build/TESTV1/obj/game/Alpha.o\", "
    "\"base_path\": \"build/TESTV1/src/game/Alpha.o\", \"metadata\": {\"complete\": true, "
    "\"source_path\": \"src/game/Alpha.cpp\", \"progress_categories\": [\"game\"], \"auto_generated\": false}}, "
    "{\"name\": \"main/game/Beta\", \"target_path\": \"build/TESTV1/obj/game/Beta.o\", \"base_path\": "
    "\"build/TESTV1/src/game/Beta.o\", \"metadata\": {\"complete\": false, \"source_path\": \"src/game/Beta.c\", "
    "\"progress_categories\": [\"game\"]}}]}";

static const char FX_NINJA[] =
    "# Source files\n"
    "# game/Alpha.cpp: game (linked True)\n"
    "build build\\TESTV1\\src\\game\\Alpha.o: mwcc_sjis src\\game\\Alpha.cpp | $\n"
    "    tools\\compilers\\$mw_version\\mwcceppc.exe || pre-compile\n"
    "  mw_version = Wii\\1.0\n"
    "  cflags = -nodefaults -proc gekko -pragma \"cats off\" -O4,p $\n"
    "      -i include -lang=c++\n"
    "  basedir = build\\TESTV1\\src\\game\n"
    "\n"
    "# game/Beta.c: game (linked\n"
    "# False)\n"
    "build $\n"
    "    build\\TESTV1\\src\\game\\Beta.o: $\n"
    "    mwcc $\n"
    "    src\\game\\Beta.c\n"
    "  mw_version = GC\\3.0a5\n"
    "  cflags = -O4,p -O0\n";

static const char *make_checkout(const char *name) {
    const char *root = fx_dir(name);
    fx_write(root, "configure.py", FX_CONFIGURE);
    fx_write(root, "tools/.keep", "");
    fx_write(root, "config/TESTV1/config.yml", "object_base: orig/TESTV1\nobject: sys/main.dol\n");
    fx_write(root, "config/TESTV1/symbols.txt", FX_SYMBOLS);
    fx_write(root, "config/TESTV1/splits.txt", "Sections:\n\t.text type:code align:8\n");
    fx_write(root, "build/TESTV1/asm/game/Alpha.s", ".file \"Alpha.cpp\"\n");
    fx_write(root, "build/TESTV1/asm/game/Beta.s", ".file \"Beta.c\"\n");
    fx_write(root, "objdiff.json", FX_OBJDIFF);
    fx_write(root, "build.ninja", FX_NINJA);
    return root;
}

static void write_walls(AvpArena *a, const ShsmRepo *repo, AvpJson *doc) {
    AvpStr s = avp_json_dumps(a, doc, NULL);
    const char *p = wall_walls_path(a, repo);
    REQUIRE(avp_makedirs(avp_cz(a, avp_path_dirname(p)), true));
    REQUIRE(avp_write_file(p, s.p, s.n));
}

static AvpJson *fn_rec(AvpArena *a, const char *unit, const char *name) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("unit"), avp_json_strz(a, unit));
    avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, name));
    return o;
}

AVP_TEST(test_stale_units_dropped) {
    AvpArena *a = avp_arena();
    const char *root = make_checkout("wall walls stale");
    const ShsmRepo *repo = shsm_repo_new(root, "TESTV1");
    const char *src = fx_write(root, "src/game/Alpha.cpp", "void f() {}\n");
    const ShsmUnit *u = shsm_get_unit(repo, "game/Alpha");
    MatchDeps none = {0};
    AvpJson *inputs = match_state_inputs(a, repo, u, src, &none);
    REQUIRE(inputs && avp_json_is_obj(inputs));

    AvpJson *doc = avp_json_obj(a), *units = avp_json_arr(a), *inp = avp_json_obj(a), *fns = avp_json_arr(a);
    avp_json_set(a, doc, AVP_S("version"), avp_json_int(a, WALLS_VERSION));
    avp_json_arr_push(a, units, avp_json_strz(a, "main/game/Alpha"));
    avp_json_arr_push(a, units, avp_json_strz(a, "main/game/Beta"));
    avp_json_set(a, doc, AVP_S("units"), units);
    avp_json_set(a, inp, AVP_S("main/game/Alpha"), inputs);
    avp_json_set(a, inp, AVP_S("main/game/Beta"), avp_json_null());
    avp_json_set(a, doc, AVP_S("inputs"), inp);
    avp_json_arr_push(a, fns, fn_rec(a, "main/game/Alpha", "f"));
    avp_json_arr_push(a, fns, fn_rec(a, "main/game/Beta", "g"));
    avp_json_set(a, doc, AVP_S("functions"), fns);
    write_walls(a, repo, doc);

    AvpJson *w = wall_load_walls(a, repo, false);
    REQUIRE(w);
    const AvpJson *wu = avp_json_getz(w, "units"), *wf = avp_json_getz(w, "functions"), *st = avp_json_getz(w, "stale");
    REQUIRE(avp_json_len(wu) == 1);
    CHECK_STR(avp_json_at(wu, 0)->u.str, "main/game/Alpha");
    REQUIRE(avp_json_len(wf) == 1);
    CHECK_STR(avp_json_get_str(avp_json_at(wf, 0), "name", AVP_S("")), "f");
    CHECK(avp_json_has(st, AVP_S("main/game/Beta")));
    CHECK(!avp_json_has(st, AVP_S("main/game/Alpha")));

    fx_write(root, "src/game/Alpha.cpp", "void f() { }\n");
    w = wall_load_walls(a, repo, false);
    REQUIRE(w);
    CHECK_EQ_I64((i64)avp_json_len(avp_json_getz(w, "units")), 0);
    CHECK_STR(avp_json_get_str(avp_json_getz(w, "stale"), "main/game/Alpha", AVP_S("")),
              "src/game/Alpha.cpp changed");
    w = wall_load_walls(a, repo, true);
    REQUIRE(w);
    CHECK_EQ_I64((i64)avp_json_len(avp_json_getz(w, "functions")), 2);
    avp_rmtree(root, 0);
}

AVP_TEST(test_assert_text_is_not_a_note) {
    AvpArena *a = avp_arena();
    const char *root = fx_dir("wall walls assert");
    const char *p = fx_write(root, "x.c",
                             "#include <x.h>\n// 0x80001000\nvoid F(void)\n{\n"
                             "    ASSERTMSG(a, \"size does not match\");\n}\n");
    WallNote *n;
    CHECK_EQ_I64((i64)wall_find_notes(a, p, &n), 0);
    avp_rmtree(root, 0);
}

/* ---- the real tree ---------------------------------------------------------------------------------------------- */

/* The checkout's repo when its build outputs can be used (the Python's _real); else NULL after saying why. */
static const ShsmRepo *real_tree(void) {
    const char *e = getenv("SHSM_NO_REAL_TREE");
    if (e && *e) {
        printf("skipped: SHSM_NO_REAL_TREE\n");
        return NULL;
    }
    if (!avp_exists("configure.py") || !avp_isdir("config")) {
        printf("skipped: not in the repository\n");
        return NULL;
    }
    const ShsmRepo *r = shsm_repo_find(NULL);
    if (!avp_isdir(r->asm_dir) || !avp_exists(r->retail_dol)) {
        printf("skipped: needs build/<ver>/asm and the retail DOL\n");
        return NULL;
    }
    return r;
}

AVP_TEST(test_built_matching_function_is_exact) {
    const ShsmRepo *r = real_tree();
    if (!r) return;
    AvpArena *a = avp_arena();
    const ShsmUnit *u = shsm_get_unit(r, "game/PadRumble");
    if (!u->base_obj || !avp_exists(u->base_obj)) {
        printf("skipped: no built PadRumble object\n");
        return;
    }
    const char *exe = avp_path_join(a, avp_cz(a, avp_path_dirname(avp_exe_dir())), "wall.exe");
    if (!avp_exists(exe)) {
        printf("skipped: no wall.exe\n");
        return;
    }
    const char *argv[] = {exe, "game/PadRumble", "fn_800067CC__8unk_RampFf", "--built"};
    ShsmRun run = shsm_run(a, argv, 4, r->root, 300);
    CHECK_MSG(run.returncode == 0, "%.*s%.*s", AVP_SF(run.out), AVP_SF(run.err));
    CHECK(avp_str_find(run.out, AVP_S("EXACT"), 0, AVP_END) >= 0);
}

AVP_TEST(test_retail_against_itself) {
    const ShsmRepo *r = real_tree();
    if (!r) return;
    AvpArena *a = avp_arena();
    CcRepo cr = {r->root, r->version};
    CcError err = {0};
    AvpMap *retail = cc_load_retail(&cr, AVP_S("main/game/PadRumble"), NULL, false, &err);
    if (!retail) {
        printf("skipped: no retail listing of game/PadRumble\n");
        return;
    }
    static const CcTdList none = {0};
    AvpMapIter it = {0};
    for (int k = 0; k < 5 && avp_map_next(retail, &it); k++) {
        const CcFn *f = it.val->p;
        AvpJson *d = wall_classify(a, f, f, &none, NULL, NULL, NULL, 0);
        CHECK(avp_json_truthy(avp_json_getz(d, "exact")));
    }
}

AVP_TEST(test_find_notes_on_sources) {
    /* parked-function notes vanish as their functions match: read every parked unit's source instead of naming
     * one, and require only well-formed results */
    const ShsmRepo *r = real_tree();
    if (!r) return;
    AvpArena *a = avp_arena();
    const ShsmUnits *us = shsm_load_units(r);
    size_t found = 0;
    for (size_t i = 0; i < us->n; i++) {
        const ShsmUnit *u = us->v[i];
        if (strcmp(u->status, "NonMatching") != 0 || !u->source) continue;
        const char *full = avp_path_join(a, r->root, u->source);
        if (!avp_exists(full)) continue;
        WallNote *n;
        size_t k = wall_find_notes(a, full, &n);
        for (size_t j = 0; j < k; j++) {
            CHECK(n[j].line >= 1);
            CHECK_MSG(!n[j].has_addr || (n[j].addr >= 0x80000000u && n[j].addr < 0x81800000u), "%s: 0x%llX", u->source,
                      (unsigned long long)n[j].addr);
            CHECK_MSG(n[j].text.n > 0, "%s:%lld", u->source, (long long)n[j].line);
            found++;
        }
    }
    if (!found) printf("skipped: no parked function with a note left in the tree\n");
}

AVP_TEST_MAIN("wall")
