/* Real-tree tests of the decision solver (skipped under SHSM_NO_REAL_TREE=1): the three controls on a real unit (and
 * the pipeline control failing when pointed at the unmodified source), the Phase 0 classes of known stops in
 * today's tree, the OSNet flag records, and --case on a Matching unit. They need the project build of this checkout
 * (build.ninja, the retail listings) and build/tools/solver.exe. */
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

/* (i) controls 1 to 3 on OSNet, and the pipeline control can fail */
AVP_TEST(controls_on_osnet) {
    if (skip()) return;
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const ShsmUnit *u = shsm_get_unit(r, "main/revolution/os/OSNet");
    SvScorer sc;
    sv_scorer_init(a, &sc, r, u);
    AvpBytes b = {0};
    REQUIRE(avp_read_all(a, avp_path_join(a, r->root, u->source), &b));
    AvpStr text = avp_strn((const char *)b.v, b.n);
    /* 1: pipeline */
    const char *why = NULL;
    CHECK_MSG(sv_control_pipeline(&sc, a, text, &why), "pipeline control: %s", why ? why : "");
    SvBuild *plain = sv_build(&sc, a, text, "test-plain", NULL);
    REQUIRE(plain->ok);
    CHECK(!sv_build_has_control_fn(plain)); /* the unmodified source has no control function: the control fails */
    /* 2: baseline: the unit's own source in place against the scratch copy */
    SvBuild *ref = sv_build_file(&sc, a, avp_path_join(a, r->root, u->source), "test-ref", NULL);
    REQUIRE(ref->ok);
    CHECK_MSG(sv_control_baseline(a, ref, plain, "NWC24Shutdown_", &why), "baseline: %s", why ? why : "");
    /* ... and it fails against a build that differs (the flag dropped) */
    AvpStrVec g = sv_flag_words(a, "-inline noauto");
    SvFlags drop = {0};
    drop.groups = &g;
    drop.ngroups = 1;
    SvBuild *dropped = sv_build(&sc, a, text, "test-drop", &drop);
    REQUIRE(dropped->ok);
    CHECK(!sv_control_baseline(a, ref, dropped, "NWC24Shutdown_", &why));
    /* 3: the flag changes the generated control source */
    CHECK_EQ_I64(sv_control_flag(&sc, a, &drop, NULL, &why), 1);
    /* a flag change that is no change leaves the control source as it is */
    CHECK_EQ_I64(sv_control_flag(&sc, a, NULL, NULL, &why), 0);
}

/* Runs solver.exe on the given units into a test folder and returns the parsed records.json. */
static AvpJson *run_solver(const char *const *units, size_t n, const char *outname, i64 *rc) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *out = avp_join(a, r->root, "build", "tmp", "solver-test", outname);
    AvpPtrVec argv = {0};
    avp_vec_push(&argv, (void *)avp_join(a, r->root, "build", "tools", "solver.exe"));
    for (size_t i = 0; i < n; i++) avp_vec_push(&argv, (void *)units[i]);
    avp_vec_push(&argv, (void *)"--out");
    avp_vec_push(&argv, (void *)out);
    ShsmRun res = shsm_run(a, (const char *const *)argv.v, argv.n, r->root, 600);
    *rc = res.returncode;
    AvpBytes b = {0};
    if (!avp_read_all(a, avp_path_join(a, out, "records.json"), &b)) return NULL;
    AvpJsonError err = {0};
    return avp_json_parse(a, avp_strn((const char *)b.v, b.n), &err);
}

static const AvpJson *find_record(const AvpJson *doc, const char *fn) {
    const AvpJson *recs = avp_json_getz(doc, "records");
    for (size_t i = 0; recs && i < avp_json_len(recs); i++) {
        const AvpJson *x = avp_json_at(recs, i);
        if (avp_str_eqz(avp_json_get_str(x, "function", AVP_S("")), fn)) return x;
    }
    return NULL;
}

static void check_class(const AvpJson *doc, const char *fn, const char *cls) {
    const AvpJson *x = find_record(doc, fn);
    CHECK_MSG(x != NULL, "no record for %s", fn);
    if (!x) return;
    AvpStr got = avp_json_get_str(x, "class", AVP_S(""));
    CHECK_MSG(avp_str_eqz(got, cls), "%s: class %.*s, expected %s", fn, AVP_SF(got), cls);
}

/* (ii) Phase 0 on today's tree, (iii) OSNet's flag records */
AVP_TEST(phase0_known_stops_and_osnet) {
    if (skip()) return;
    static const char *const units[] = {"main/game/CAIFrozenLakeEnemyRespot",
                                        "main/rwsdk/src/driver/wii/dlsprite",
                                        "main/rwsdk/plugin/matfx/gcn/nbtGen",
                                        "main/rwsdk/plugin/skin2/rpskin",
                                        "main/libjpeg/jmemmgr",
                                        "main/climax/Localisation",
                                        "main/revolution/os/OSNet"};
    i64 rc = -1;
    AvpJson *doc = run_solver(units, AVP_ARRAY_LEN(units), "phase0", &rc);
    CHECK_EQ_I64(rc, 0);
    REQUIRE(doc);
    check_class(doc, "fn_8011FCC0", "not-started");
    check_class(doc, "_rwDlRasterCamera_ZClearRect", "not-started");
    /* CalcMeshNBTs: src/rwsdk/plugin/matfx/gcn/nbtGen.c only declares it (`extern void CalcMeshNBTs(...)`, "not
     * reconstructed yet"), so the unit source does not define it: not-started, not inline. */
    check_class(doc, "CalcMeshNBTs", "not-started");
    check_class(doc, "SkinGeometryRead", "register");
    check_class(doc, "alloc_small", "register");
    check_class(doc, "fn_801D2630", "control-flow");
    const AvpJson *x = find_record(doc, "fn_8011FCC0");
    if (x) CHECK(avp_str_eqz(avp_json_get_str(x, "verdict", AVP_S("")), "SKIPPED"));
    /* alloc_small: surviving source, no candidate */
    x = find_record(doc, "alloc_small");
    if (x) CHECK_EQ_U64(avp_json_len(avp_json_getz(x, "candidates")), 0);

    /* OSNet: flag records naming -inline noauto, no candidates, NO-FORM-WITHIN-RULES, flag control evidence */
    const AvpJson *recs = avp_json_getz(doc, "records");
    size_t nflag = 0, ntraced = 0;
    for (size_t i = 0; recs && i < avp_json_len(recs); i++) {
        const AvpJson *y = avp_json_at(recs, i);
        if (!avp_str_eqz(avp_json_get_str(y, "unit", AVP_S("")), "main/revolution/os/OSNet")) continue;
        nflag++;
        CHECK(avp_str_eqz(avp_json_get_str(y, "class", AVP_S("")), "flag"));
        CHECK(avp_str_eqz(avp_json_get_str(y, "callee", AVP_S("")), "-inline noauto"));
        CHECK(avp_str_contains(avp_json_get_str(y, "condition", AVP_S("")), AVP_S("-inline noauto")));
        CHECK_EQ_U64(avp_json_len(avp_json_getz(y, "candidates")), 0);
        CHECK(avp_str_eqz(avp_json_get_str(y, "verdict", AVP_S("")), "NO-FORM-WITHIN-RULES"));
        const AvpJson *ev = avp_json_getz(y, "evidence");
        bool ctl = false;
        for (size_t k = 0; ev && k < avp_json_len(ev); k++)
            ctl = ctl || avp_str_eqz(avp_json_str_or(avp_json_at(ev, k), AVP_S("")),
                                     "flag control: -inline noauto changes the generated control source");
        CHECK(ctl);
        /* the inline trace of the compile without the flag: the decisions it changes, in the trace's numbers */
        const AvpJson *tr = avp_json_getz(y, "trace");
        REQUIRE(tr && tr->kind == AVP_JSON_OBJECT);
        CHECK(avp_str_eqz(avp_json_get_str(tr, "kind", AVP_S("")), "inline-flag"));
        CHECK(avp_str_eqz(avp_json_get_str(tr, "self_check", AVP_S("")), "ok"));
        const AvpJson *ch = avp_json_getz(tr, "changed");
        if (ch && avp_json_len(ch)) ntraced++;
        for (size_t k = 0; ch && k < avp_json_len(ch); k++) {
            const AvpJson *wo = avp_json_getz(avp_json_at(ch, k), "without");
            REQUIRE(wo);
            CHECK(avp_str_eqz(avp_json_get_str(wo, "decision", AVP_S("")), "expanded"));
            CHECK(avp_json_get_str(wo, "rule", AVP_S("")).n > 0);
            CHECK(avp_json_get_i64(wo, "statements", -1) >= 0);
            CHECK(avp_json_get_i64(wo, "max_statements", -1) >= 0);
            const AvpJson *with = avp_json_getz(avp_json_at(ch, k), "with");
            CHECK(with && avp_str_eqz(avp_json_get_str(with, "decision", AVP_S("")), "called"));
        }
    }
    CHECK(nflag >= 5); /* without the flag five of OSNet's eight functions differ */
    CHECK(ntraced >= 4); /* and for four of them the trace names the call sites the flag stops expanding */
    /* every unit passed the pipeline control */
    const AvpJson *controls = avp_json_getz(doc, "controls");
    for (size_t i = 0; controls && i < controls->u.obj.n; i++)
        CHECK_MSG(avp_json_get_bool(controls->u.obj.v[i].val, "pipeline", false), "pipeline control failed for %.*s",
                  AVP_SF(controls->u.obj.v[i].key));
}

/* --case on a Matching unit: the unit is compiled from the edited scratch copy; a find text that does not occur
 * exactly once is an error and nothing runs. */
AVP_TEST(case_on_matching_unit) {
    if (skip()) return;
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *dir = avp_join(a, r->root, "build", "tmp", "solver-test", "cases");
    REQUIRE(avp_makedirs(dir, true));
    const char *good = avp_path_join(a, dir, "rwplugin.case");
    const char text[] = "unit main/climax/RwPlugin\n"
                        "function ClimaxP1AtomicDataDestructor__FPvii\n"
                        "expect verdict MATCH\n" /* Phase 2 finds the const accessor again */
                        "find: if ( CLIMAXP1_ATOMIC_CONST_DATA( atomic )->item != NULL ) {\n"
                        "replace: if ( CLIMAXP1_ATOMIC_DATA( atomic )->item != NULL ) {\n";
    REQUIRE(avp_write_file(good, text, sizeof text - 1));
    const char *exe = avp_join(a, r->root, "build", "tools", "solver.exe");
    const char *out = avp_join(a, r->root, "build", "tmp", "solver-test", "case-out");
    const char *argv[] = {exe, "--case", good, "--out", out};
    ShsmRun res = shsm_run(a, argv, AVP_ARRAY_LEN(argv), r->root, 600);
    CHECK_EQ_I64(res.returncode, 0);
    AvpBytes b = {0};
    REQUIRE(avp_read_all(a, avp_path_join(a, out, "records.json"), &b));
    AvpJsonError err = {0};
    AvpJson *doc = avp_json_parse(a, avp_strn((const char *)b.v, b.n), &err);
    REQUIRE(doc);
    const AvpJson *x = find_record(doc, "ClimaxP1AtomicDataDestructor__FPvii");
    CHECK(x != NULL); /* the edit makes the function a stop */
    const char *bad = avp_path_join(a, dir, "bad.case");
    const char btext[] = "unit main/climax/RwPlugin\nfunction ClimaxP1AtomicDataDestructor__FPvii\n"
                         "find: this text is not in the source\nreplace: x\n";
    REQUIRE(avp_write_file(bad, btext, sizeof btext - 1));
    const char *argv2[] = {exe, "--case", bad, "--out", out};
    res = shsm_run(a, argv2, AVP_ARRAY_LEN(argv2), r->root, 600);
    CHECK_EQ_I64(res.returncode, 2);
    CHECK(avp_str_contains(shsm_run_output(a, &res), AVP_S("occurs 0 times")));
}

/* An exact function never gets a record. CDrawingTest's object also holds a weak four-byte stub of another class,
 * fn_8000CFCC__17CAttributeHandlerFv, sharing the fn_<addr> prefix of retail's fn_8000CFCC__12CDrawingTestFv (a tail
 * call): the stub is EXTRA, and no record may compare it with that retail function. */
AVP_TEST(exact_function_with_prefix_twin_has_no_record) {
    if (skip()) return;
    static const char *const units[] = {"main/game/CDrawingTest"};
    i64 rc = -1;
    AvpJson *doc = run_solver(units, AVP_ARRAY_LEN(units), "prefix-twin", &rc);
    CHECK_EQ_I64(rc, 0);
    REQUIRE(doc);
    CHECK(find_record(doc, "fn_8000CFCC__12CDrawingTestFv") == NULL);
    CHECK(find_record(doc, "fn_8000CFCC__17CAttributeHandlerFv") == NULL);
    /* and --function names it as exact */
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *exe = avp_join(a, r->root, "build", "tools", "solver.exe");
    const char *out = avp_join(a, r->root, "build", "tmp", "solver-test", "prefix-twin-fn");
    const char *argv[] = {exe, "main/game/CDrawingTest", "--function", "fn_8000CFCC__12CDrawingTestFv", "--out", out};
    ShsmRun res = shsm_run(a, argv, AVP_ARRAY_LEN(argv), r->root, 600);
    CHECK_EQ_I64(res.returncode, 0);
    AvpStr text = shsm_run_output(a, &res);
    CHECK(avp_str_contains(text, AVP_S("fn_8000CFCC__12CDrawingTestFv: exact in main/game/CDrawingTest")));
    CHECK(avp_str_contains(text, AVP_S("0 record(s)")));
}

/* A call-set stop whose calls the inliner keeps (its trace counts equal retail's) and a later stage removes: the
 * inline trace's self-check reclassifies it, its own cause, not MODEL-GAP. */
AVP_TEST(calls_removed_after_the_inliner) {
    if (skip()) return;
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo();
    const char *fn = "loadSampleData__Q24FMOD13EventProjectIFPiiPPciUi";
    const char *out = avp_join(a, r->root, "build", "tmp", "solver-test", "after-inliner");
    const char *argv[] = {avp_join(a, r->root, "build", "tools", "solver.exe"), "main/fmod/fmod_eventprojecti", "--function", fn,
                          "--out", out};
    ShsmRun res = shsm_run(a, argv, AVP_ARRAY_LEN(argv), r->root, 600);
    CHECK_EQ_I64(res.returncode, 0);
    AvpBytes b = {0};
    REQUIRE(avp_read_all(a, avp_path_join(a, out, "records.json"), &b));
    AvpJsonError err = {0};
    AvpJson *doc = avp_json_parse(a, avp_strn((const char *)b.v, b.n), &err);
    REQUIRE(doc);
    const AvpJson *x = find_record(doc, fn);
    REQUIRE(x);
    CHECK(!avp_str_eqz(avp_json_get_str(x, "verdict", AVP_S("")), "MODEL-GAP"));
    CHECK(!avp_str_eqz(avp_json_get_str(x, "class", AVP_S("")), "inline"));
    CHECK(avp_str_startswith(avp_json_get_str(x, "cause", AVP_S("")), AVP_S("calls removed after the inliner: ")));
    const AvpJson *g = avp_json_getz(avp_json_getz(x, "trace"), "gap");
    REQUIRE(g);
    CHECK(avp_str_eqz(avp_json_get_str(g, "stage", AVP_S("")), "after-inliner"));
    CHECK_EQ_I64(avp_json_get_i64(g, "trace_calls", -1), 15);
    CHECK_EQ_I64(avp_json_get_i64(g, "ir_calls", -1), 15);
    CHECK_EQ_I64(avp_json_get_i64(g, "object_calls", -1), 11);
    CHECK_EQ_I64(avp_json_get_i64(g, "retail_calls", -1), 15);
    CHECK(avp_str_contains(avp_json_get_str(x, "condition", AVP_S("")), AVP_S("trace 15, post-inline IR 15, object 11, retail 15")));
}

AVP_TEST_MAIN("solver_real_tree")
