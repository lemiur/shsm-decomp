/* Real-tree tests of Phase 2 (skipped under SHSM_NO_REAL_TREE=1): the regression corpus. The case files under
 * tests/cases/ revert a closing form in the scratch copy and state what the solver must find (solver.exe exits 0
 * only when every expectation holds); the live stops are checked here from records.json. They need the project
 * build of this checkout, build/tools/solver.exe and sibling.exe, and the sibling index. */
#include "avp/test.h"
#include "solver.h"

static bool skip(void) {
    AvpArena *a = avp_arena();
    const char *nrt = avp_getenv(a, "SHSM_NO_REAL_TREE");
    if (nrt && *nrt && strcmp(nrt, "0") != 0) {
        printf("skipped: SHSM_NO_REAL_TREE is set\n");
        return true;
    }
    return false;
}

static const ShsmRepo *repo(void) { return shsm_repo_find(NULL); }

static AvpJson *run(const char *const *args, size_t n, const char *outname, i64 *rc, AvpStr *output) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *out = avp_join(a, r->root, "build", "tmp", "solver-test", outname);
    AvpPtrVec argv = {0};
    avp_vec_push(&argv, (void *)avp_join(a, r->root, "build", "tools", "solver.exe"));
    for (size_t i = 0; i < n; i++) avp_vec_push(&argv, (void *)args[i]);
    avp_vec_push(&argv, (void *)"--out");
    avp_vec_push(&argv, (void *)out);
    ShsmRun res = shsm_run(a, (const char *const *)argv.v, argv.n, r->root, 900);
    *rc = res.returncode;
    if (output) *output = shsm_run_output(a, &res);
    AvpBytes b = {0};
    if (!avp_read_all(a, avp_path_join(a, out, "records.json"), &b)) return NULL;
    AvpJsonError err = {0};
    return avp_json_parse(a, avp_strn((const char *)b.v, b.n), &err);
}

static const AvpJson *record_of(const AvpJson *doc, const char *fn) {
    const AvpJson *recs = avp_json_getz(doc, "records");
    for (size_t i = 0; recs && i < avp_json_len(recs); i++)
        if (avp_str_eqz(avp_json_get_str(avp_json_at(recs, i), "function", AVP_S("")), fn)) return avp_json_at(recs, i);
    return NULL;
}

static bool verdict_is(const AvpJson *x, const char *v) { return avp_str_eqz(avp_json_get_str(x, "verdict", AVP_S("")), v); }

/* Whether any string of a list field (or its objects' field) contains all the needles. */
static bool list_has(const AvpJson *x, const char *list, const char *field, const char *n1, const char *n2) {
    const AvpJson *l = avp_json_getz(x, list);
    for (size_t i = 0; l && i < avp_json_len(l); i++) {
        const AvpJson *e = avp_json_at(l, i);
        AvpStr s = field ? avp_json_get_str(e, field, AVP_S("")) : avp_json_str_or(e, AVP_S(""));
        if (avp_str_contains(s, avp_str(n1)) && (!n2 || avp_str_contains(s, avp_str(n2)))) return true;
    }
    return false;
}

static bool notes_have(const AvpJson *doc, const char *n1, const char *n2) {
    const AvpJson *l = avp_json_getz(doc, "notes");
    for (size_t i = 0; l && i < avp_json_len(l); i++) {
        AvpStr s = avp_json_str_or(avp_json_at(l, i), AVP_S(""));
        if (avp_str_contains(s, avp_str(n1)) && (!n2 || avp_str_contains(s, avp_str(n2)))) return true;
    }
    return false;
}

/* Every case file of the corpus: solver.exe --case exits 0 (all its expectations held). */
AVP_TEST(corpus_cases) {
    if (skip()) return;
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *dir = avp_join(a, r->root, "tools", "c", "tools", "solver", "tests", "cases");
    AvpStrVec files = {0};
    REQUIRE(avp_listdir(a, dir, &files));
    size_t n = 0;
    for (size_t i = 0; i < files.n; i++) {
        if (!avp_str_endswith(files.v[i], AVP_S(".case"))) continue;
        n++;
        const char *path = avp_path_join(a, dir, avp_cz(a, files.v[i]));
        const char *args[] = {"--case", path};
        i64 rc = -1;
        AvpStr out = {0};
        AvpJson *doc = run(args, 2, avp_sprintf(a, "case-%.*s", AVP_SF(files.v[i])), &rc, &out);
        CHECK_MSG(rc == 0 && doc, "%.*s: solver.exe exited %lld:\n%.*s", AVP_SF(files.v[i]), (long long)rc, AVP_SF(out));
        /* the declaration-order probe of an R30 case is traced against ours: the variable the RULE-EXCEPTION protects
         * moves, and the probe's result says how in the allocator's numbers */
        static const char *const AGAINST[][3] = {
            {"worldsynccamera_r30.case", "WorldSyncCamera", "`spSect`"},
            {"freelistfreereal_r30.case", "_rwFreeListFreeReal", "`heap`"},
        };
        for (size_t k = 0; doc && k < AVP_ARRAY_LEN(AGAINST); k++) {
            if (!avp_str_eqz(files.v[i], AGAINST[k][0])) continue;
            const AvpJson *x = record_of(doc, AGAINST[k][1]);
            REQUIRE(x);
            CHECK_MSG(list_has(x, "probes", "result", "regalloc --against (A ours, B the probe): ", AGAINST[k][2]),
                      "%s: no traced declaration probe moves %s", AGAINST[k][0], AGAINST[k][2]);
            const AvpJson *tr = avp_json_getz(x, "trace");
            CHECK(tr && avp_str_eqz(avp_json_get_str(tr, "self_check", AVP_S("")), "ok"));
            CHECK(tr && avp_json_len(avp_json_getz(tr, "against")) >= 1);
        }
    }
    CHECK(n >= 4);
}

/* A register stop of game code: the regalloc trace states both nodes (the differing variable and the one holding
 * retail's register in ours) with their sweep, positions, degree and spill cost. (CPointOfInterest's
 * fn_800C5320, the earlier case, is a schedule stop: retail issues the prologue's lfs before the stwu.) */
AVP_TEST(game_register_stop_nodes) {
    if (skip()) return;
    const char *fn = "fn_8009CB1C__13CButtonPuzzleFR7unk_Msg";
    const char *args[] = {"main/game/CButtonPuzzle", "--function", fn};
    i64 rc = -1;
    AvpJson *doc = run(args, AVP_ARRAY_LEN(args), "game-register", &rc, NULL);
    CHECK_EQ_I64(rc, 0);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, fn);
    REQUIRE(x);
    CHECK(avp_str_eqz(avp_json_get_str(x, "class", AVP_S("")), "register"));
    const AvpJson *tr = avp_json_getz(x, "trace");
    REQUIRE(tr);
    CHECK(avp_str_eqz(avp_json_get_str(tr, "kind", AVP_S("")), "regalloc"));
    CHECK(avp_str_eqz(avp_json_get_str(tr, "self_check", AVP_S("")), "ok"));
    static const char *const NODES[] = {"variable", "holder"};
    for (int k = 0; k < 2; k++) {
        const AvpJson *nd = avp_json_getz(tr, NODES[k]);
        CHECK_MSG(nd != NULL, "no %s node", NODES[k]);
        if (!nd) continue;
        CHECK(avp_json_get_i64(nd, "node", -1) >= 32);
        CHECK(avp_json_get_i64(nd, "sweep", -1) >= 1);
        CHECK(avp_json_get_i64(nd, "push_position", -1) >= 0);
        CHECK(avp_json_get_i64(nd, "select_position", -1) >= 0);
        CHECK(avp_json_get_i64(nd, "degree", -1) >= 0);
        CHECK(avp_json_getz(nd, "spill_cost") != NULL);
        CHECK(avp_json_get_str(nd, "register", AVP_S("")).n > 0);
    }
    CHECK(avp_json_get_str(avp_json_getz(tr, "variable"), "retail_register", AVP_S("")).n > 0);
    CHECK(avp_str_contains(avp_json_get_str(x, "condition", AVP_S("")), AVP_S("must be coloured")));
    CHECK(avp_str_contains(avp_json_get_str(x, "cause", AVP_S("")), AVP_S("; trace: ")));
}

/* _rpSkinBlendBodyP before ba2a4870: the form is byte-exact (no stop), and the DWARF comparison of that text names
 * gqr5value and gqr5address as locals the sibling lists and ours lacks. */
AVP_TEST(skingcn_dwarf_locals) {
    if (skip()) return;
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *path = avp_join(a, r->root, "tools", "c", "tools", "solver", "tests", "cases", "skinblendbodyp_dwarf.exact");
    const char *args[] = {"--case", path};
    i64 rc = -1;
    AvpJson *doc = run(args, 2, "skingcn-exact", &rc, NULL);
    REQUIRE(doc);
    CHECK_EQ_I64(rc, 0);
    CHECK(record_of(doc, "_rpSkinBlendBodyP") == NULL);
    CHECK(notes_have(doc, "_rpSkinBlendBodyP: exact", NULL));
    /* the comparison on the reverted text */
    AvpBytes cb = {0}, sb = {0};
    REQUIRE(avp_read_all(a, path, &cb));
    SvCase cs;
    const char *err = NULL;
    REQUIRE(sv_case_parse(a, avp_strn((const char *)cb.v, cb.n), path, &cs, &err));
    const ShsmUnit *u = shsm_get_unit(r, cs.unit);
    REQUIRE(avp_read_all(a, avp_path_join(a, r->root, u->source), &sb));
    AvpStr text;
    REQUIRE(sv_case_apply(a, &cs, avp_strn((const char *)sb.v, sb.n), &text, &err));
    LvCtx *x = lv_ctx_new(a, text, "_rpSkinBlendBodyP");
    SvVars ours = sv_source_locals(a, x);
    CHECK(sv_vars_find(&ours, "gqr5value") == NULL);
    const char *exe = avp_join(a, r->root, "build", "tools", "sibling.exe");
    const char *fa[] = {exe, "fn", "_rpSkinBlendBodyP", "--json"};
    const char *da[] = {exe, "fn", "_rpSkinBlendBodyP", "--build", "debug", "--json"};
    ShsmRun fr = shsm_run(a, fa, AVP_ARRAY_LEN(fa), r->root, 120), dr = shsm_run(a, da, AVP_ARRAY_LEN(da), r->root, 120);
    SvSibFn fin, dbg;
    REQUIRE(sv_sibling_parse(a, fr.out, &fin));
    bool have_dbg = sv_sibling_parse(a, dr.out, &dbg);
    SvLcFindings f = sv_locals_compare(a, &ours, &fin.locals, have_dbg ? &dbg.locals : NULL, NULL);
    bool v = false, ad = false;
    for (size_t i = 0; i < f.n; i++) {
        if (f.v[i].kind != SV_LC_MISSING) continue;
        v = v || !strcmp(f.v[i].name, "gqr5value");
        ad = ad || !strcmp(f.v[i].name, "gqr5address");
    }
    CHECK_MSG(v, "gqr5value is not reported missing");
    CHECK_MSG(ad, "gqr5address is not reported missing");
}

/* DSPResampler::execute (live): never MATCH; NO-FORM-WITHIN-RULES through a const-read probe on mNoDMA. */
AVP_TEST(dspresampler_const_read) {
    if (skip()) return;
    const char *fn = "execute__Q24FMOD12DSPResamplerFPfPPfPUiiPi16FMOD_SPEAKERMODEUi";
    const char *args[] = {"main/fmod/fmod_dsp_resampler", "--function", fn};
    i64 rc = -1;
    AvpJson *doc = run(args, 3, "dspresampler", &rc, NULL);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, fn);
    REQUIRE(x);
    CHECK(!verdict_is(x, "MATCH"));
    CHECK(verdict_is(x, "NO-FORM-WITHIN-RULES"));
    CHECK(list_has(x, "probes", "lever", "const-read", "mNoDMA"));
    CHECK(list_has(x, "probes", "result", "no const accessor is listed", "user's ruling 2026-11-16"));
}

/* alloc_small (live): surviving source, no candidate, never the three-term sum. */
AVP_TEST(alloc_small_surviving_source) {
    if (skip()) return;
    const char *args[] = {"main/libjpeg/jmemmgr", "--function", "alloc_small"};
    i64 rc = -1;
    AvpJson *doc = run(args, 3, "alloc_small", &rc, NULL);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, "alloc_small");
    REQUIRE(x);
    CHECK_EQ_U64(avp_json_len(avp_json_getz(x, "candidates")), 0);
    CHECK(verdict_is(x, "NO-FORM-WITHIN-RULES") || verdict_is(x, "CONDITION-ONLY"));
    CHECK(avp_str_contains(avp_json_get_str(x, "note", AVP_S("")), AVP_S("surviving-source libjpeg/")));
    CHECK(!avp_str_contains(avp_json_dumps(avp_arena(), x, NULL), AVP_S("total_space_allocated + slop + min_request")));
}

/* SkinGeometryRead (live): the decision sits in the inlined helper SkinFindMaxWeights; declaring j before i closes
 * the target and opens SkinAtomicRead. */
AVP_TEST(skingeometryread_helper) {
    if (skip()) return;
    const char *args[] = {"main/rwsdk/plugin/skin2/rpskin", "--function", "SkinGeometryRead"};
    i64 rc = -1;
    AvpJson *doc = run(args, 3, "skingeometryread", &rc, NULL);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, "SkinGeometryRead");
    REQUIRE(x);
    CHECK(verdict_is(x, "NO-FORM-WITHIN-RULES"));
    CHECK(list_has(x, "evidence", NULL, "same-file helper SkinFindMaxWeights", NULL));
    bool probe = list_has(x, "probes", "result", "opens SkinAtomicRead", NULL) &&
                 list_has(x, "probes", "lever", "helper SkinFindMaxWeights", NULL);
    bool cand = list_has(x, "candidates", "description", "helper SkinFindMaxWeights", "opens SkinAtomicRead");
    CHECK(probe || cand);
}

/* --function on an exact function: one line naming it exact and quoting its RULE-EXCEPTION, no record. */
AVP_TEST(exact_function_quotes_rule_exception) {
    if (skip()) return;
    static const char *const cases[][2] = {{"main/rwsdk/world/baworobj", "WorldSyncCamera"},
                                           {"main/rwsdk/src/plcore/bamemory", "_rwFreeListFreeReal"}};
    for (size_t i = 0; i < 2; i++) {
        const char *args[] = {cases[i][0], "--function", cases[i][1]};
        i64 rc = -1;
        AvpJson *doc = run(args, 3, avp_sprintf(avp_arena(), "exact-%zu", i), &rc, NULL);
        REQUIRE(doc);
        CHECK_EQ_I64(rc, 0);
        CHECK(record_of(doc, cases[i][1]) == NULL);
        CHECK_MSG(notes_have(doc, avp_sprintf(avp_arena(), "%s: exact", cases[i][1]), "RULE-EXCEPTION: R30"),
                  "%s: no exact line quoting RULE-EXCEPTION: R30", cases[i][1]);
    }
}

/* Phase 3 (live tree, case file): ChannelEmulated::update with its two returning ifs reverted from one `||` condition
 * to two ifs. The control-flow stop finds the merged condition (an exact, clean candidate of the form merge-ifs); the
 * run is a --case run, so --apply refuses it. */
AVP_TEST(phase3_channel_emulated_merge_ifs) {
    if (skip()) return;
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *fn = "update__Q24FMOD15ChannelEmulatedFi";
    const char *path = avp_join(a, r->root, "tools", "c", "tools", "solver", "tests", "cases", "channelemulated_update.case");
    const char *args[] = {"--case", path};
    i64 rc = -1;
    AvpStr output = {0};
    AvpJson *doc = run(args, 2, "phase3-channel", &rc, &output);
    REQUIRE(doc);
    CHECK_MSG(rc == 0, "solver.exe --case exited %lld:\n%.*s", (long long)rc, AVP_SF(output));
    const AvpJson *x = record_of(doc, fn);
    REQUIRE(x);
    CHECK(verdict_is(x, "MATCH"));
    CHECK(avp_str_eqz(avp_json_get_str(x, "class", AVP_S("")), "control-flow"));
    const AvpJson *cl = avp_json_getz(x, "candidates");
    bool merged = false;
    for (size_t i = 0; cl && i < avp_json_len(cl); i++) {
        const AvpJson *c = avp_json_at(cl, i);
        merged = merged || (avp_str_eqz(avp_json_get_str(c, "form", AVP_S("")), "merge-ifs") &&
                            avp_json_get_bool(c, "exact", false) && avp_json_len(avp_json_getz(c, "broken")) == 0 &&
                            avp_json_len(avp_json_getz(c, "rules")) == 0);
    }
    CHECK_MSG(merged, "no exact clean merge-ifs candidate");
    const AvpJson *tm = avp_json_getz(doc, "timing");
    REQUIRE(tm);
    CHECK(avp_json_get_i64(tm, "solved", 0) >= 1);
    CHECK(avp_json_get_bool(x, "ran_candidates", false));
    /* the scratch text was edited by the case: never applied to the real source */
    const char *aargs[] = {"--apply", "1", "--function", fn};
    AvpStr aout = {0};
    run(aargs, 4, "phase3-channel", &rc, &aout);
    CHECK_EQ_I64(rc, 2);
    CHECK(avp_str_contains(aout, AVP_S("--case")));
    AvpBytes b = {0};
    REQUIRE(avp_read_all(a, avp_join(a, r->root, "src", "fmod", "fmod_channel_emulated.cpp"), &b));
    CHECK(avp_str_contains(avp_strn((const char *)b.v, b.n), AVP_S("|| ( mMode & FMOD_VIRTUAL_PLAYFROMSTART )")));
}

static const char *str_of(const AvpJson *x, const char *k) {
    return avp_cz(avp_arena(), avp_json_get_str(x, k, AVP_S("")));
}

/* RenderWare: TriListNBTDataSetup8's first difference is an order difference in one block (the clrlslwi of r5 at
 * retail row 43, ours at 45): class schedule, decided in pass 2 by height; pass 1 had the same order by program order.
 * TriStripNBTDataSetup8 (the same unit) renames registers in the same order: register. */
AVP_TEST(schedule_stop_height) {
    if (skip()) return;
    const char *args[] = {"main/rwsdk/plugin/matfx/gcn/nbtGen"};
    i64 rc = -1;
    AvpJson *doc = run(args, AVP_ARRAY_LEN(args), "schedule", &rc, NULL);
    CHECK_EQ_I64(rc, 0);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, "TriListNBTDataSetup8");
    REQUIRE(x);
    CHECK(!strcmp(str_of(x, "class"), "schedule"));
    CHECK(!strcmp(str_of(x, "cause"), "schedule: height"));
    const AvpJson *tr = avp_json_getz(x, "trace");
    REQUIRE(tr);
    CHECK(!strcmp(str_of(tr, "kind"), "sched"));
    CHECK(!strcmp(str_of(tr, "self_check"), "ok"));
    const AvpJson *p2 = avp_json_getz(tr, "pass2");
    REQUIRE(p2);
    CHECK(!strcmp(str_of(p2, "key"), "height"));
    CHECK(strstr(str_of(p2, "key_words"), "height 14 > 9") != NULL);
    CHECK_EQ_I64(avp_json_get_i64(tr, "decided_pass", -1), 2);
    CHECK(!strcmp(str_of(avp_json_getz(tr, "pass1"), "key"), "order"));
    CHECK(strstr(str_of(x, "condition"), "retail issues `clrlslwi r12, r5, 24, 2` first") != NULL);
    CHECK(strstr(str_of(x, "condition"), "on height 14 > 9") != NULL);
    CHECK(list_has(x, "evidence", NULL, "order test: rows 43..45", NULL));
    const AvpJson *s = record_of(doc, "TriStripNBTDataSetup8");
    REQUIRE(s);
    CHECK(!strcmp(str_of(s, "class"), "register"));
    CHECK(list_has(s, "evidence", NULL, "order test:", "same order with other registers"));
}

/* game: fn_8000A76C's first difference is between temporaries only; the scheduler's pass 1 issues the LFS (regalloc
 * #300) before the LFD (#298) by the critical key (slack 0 against 4), which fixes which is defined first */
AVP_TEST(temporaries_pass1_order) {
    if (skip()) return;
    const char *fn = "fn_8000A76C__12unk_8000A040Ff";
    const char *args[] = {"main/game/unk_8000A040", "--function", fn};
    i64 rc = -1;
    AvpJson *doc = run(args, AVP_ARRAY_LEN(args), "temps", &rc, NULL);
    CHECK_EQ_I64(rc, 0);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, fn);
    REQUIRE(x);
    CHECK(!strcmp(str_of(x, "class"), "register"));
    const char *c = str_of(x, "condition");
    CHECK(strstr(c, "temporaries only") != NULL);
    CHECK(strstr(c, "pass 1 issues the LFS (regalloc #300") != NULL);
    CHECK(strstr(c, "before the LFD (#298") != NULL);
    CHECK(strstr(c, "critical (slack 0 <= cycle 0; #7 slack 4)") != NULL);
    CHECK(avp_json_getz(avp_json_getz(x, "trace"), "sched_temps") != NULL);
}

/* RenderWare: in RxLockedPipeUnlock the -sym on DWARF has no readable location for unlockStartBlockSize: the
 * register self-check lists it as not covered, which is no model gap */
AVP_TEST(unreadable_dwarf_not_a_model_gap) {
    if (skip()) return;
    const char *args[] = {"main/rwsdk/src/pipe/p2/p2define", "--function", "RxLockedPipeUnlock", "--diagnose-only"};
    i64 rc = -1;
    AvpJson *doc = run(args, AVP_ARRAY_LEN(args), "unreadable", &rc, NULL);
    CHECK_EQ_I64(rc, 0);
    REQUIRE(doc);
    const AvpJson *x = record_of(doc, "RxLockedPipeUnlock");
    REQUIRE(x);
    CHECK(strcmp(str_of(x, "verdict"), "MODEL-GAP") != 0);
    CHECK(!strcmp(str_of(avp_json_getz(x, "trace"), "self_check"), "ok"));
    CHECK(list_has(x, "evidence", NULL, "not covered: unlockStartBlockSize", "no readable location"));
}

AVP_TEST_MAIN("solver_phase2_real_tree")
