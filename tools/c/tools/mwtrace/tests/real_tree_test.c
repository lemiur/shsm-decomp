/* Real-tree tests of mwtrace inline (skipped under SHSM_NO_REAL_TREE=1): the known cases and the probe fixtures,
 * each traced by build/tools/mwtrace.exe into build/mwtrace-test/<case> (its inline.json), asserting the decisions, rules and
 * numbers established in docs/compiler_inline.md and the self-check. They need this checkout's build.ninja, the
 * compilers and build/tools/mwtrace.exe. Numbers that follow a unit's source as it is worked on (statement counts,
 * call counts) are checked as relations, the compiler's own limits exactly. */
#include "avp/avp.h"
#include "avp/test.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

static bool skip(void) {
    AvpArena *a = avp_arena();
    const char *nrt = avp_getenv(a, "SHSM_NO_REAL_TREE");
    if (nrt && *nrt && strcmp(nrt, "0") != 0) {
        printf("skipped: SHSM_NO_REAL_TREE is set\n");
        return true;
    }
    return false;
}

/* mwtrace.exe inline <args...> --out build/mwtrace-test/<name>: the parsed inline.json, *rc the exit code. */
static AvpJson *trace(const char *name, const char *const *args, size_t n, i64 *rc) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    AvpPtrVec argv = {0};
    avp_vec_push(&argv, (void *)avp_join(a, r->root, "build", "tools", "mwtrace.exe"));
    avp_vec_push(&argv, (void *)"inline");
    for (size_t i = 0; i < n; i++) avp_vec_push(&argv, (void *)args[i]);
    avp_vec_push(&argv, (void *)"--out");
    avp_vec_push(&argv, (void *)avp_join(a, r->root, "build", "mwtrace-test", name));
    ShsmRun res = shsm_run(a, (const char *const *)argv.v, argv.n, r->root, 600);
    *rc = res.returncode;
    /* the report from its file (the compiler's own console lines can reach stdout) */
    AvpStr text = {0};
    AvpJsonError err;
    AvpJson *j = NULL;
    if (avp_read_text(a, avp_join(a, r->root, "build", "mwtrace-test", name, "inline.json"), AVP_TM_UTF8, &text))
        j = avp_json_parse(a, text, &err);
    if (!j) printf("%s: no JSON (exit %lld): %.*s\n", name, (long long)res.returncode, AVP_SF(shsm_run_output(a, &res)));
    else printf("%s: %.2f s\n", name, avp_json_get_f64(j, "seconds", -1));
    return j;
}

static const AvpJson *caller_of(const AvpJson *j, const char *caller) {
    const AvpJson *cs = avp_json_getz(j, "callers");
    for (size_t i = 0; i < avp_json_len(cs); i++) {
        const AvpJson *c = avp_json_at(cs, i);
        if (avp_str_eqz(avp_json_get_str(c, "caller", AVP_S("")), caller)) return c;
    }
    return NULL;
}

/* The ordinal-th site of callee (link or source name) in caller, or NULL. */
static const AvpJson *site_of(const AvpJson *j, const char *caller, const char *callee, i64 ordinal) {
    const AvpJson *c = caller_of(j, caller);
    const AvpJson *ss = c ? avp_json_getz(c, "sites") : NULL;
    for (size_t i = 0; i < avp_json_len(ss); i++) {
        const AvpJson *s = avp_json_at(ss, i);
        bool name = avp_str_eqz(avp_json_get_str(s, "callee", AVP_S("")), callee) ||
                    avp_str_eqz(avp_json_get_str(s, "callee_name", AVP_S("")), callee);
        if (name && avp_json_get_i64(s, "ordinal", 0) == ordinal) return s;
    }
    return NULL;
}

static bool str_is(const AvpJson *o, const char *k, const char *v) { return avp_str_eqz(avp_json_get_str(o, k, AVP_S("")), v); }
static bool str_starts(const AvpJson *o, const char *k, const char *v) {
    return avp_str_startswith(avp_json_get_str(o, k, AVP_S("")), avp_str(v));
}
static const char *selfcheck(const AvpJson *j) {
    return avp_cz(avp_arena(), avp_json_get_str(avp_json_getz(j, "selfcheck"), "status", AVP_S("?")));
}

/* game/CDrawingTest (Wii/1.0): fn_8000CFCC's call of fn_801785B4 under the unit's flags and under -inline noauto */
AVP_TEST(cdrawingtest_fn_801785B4) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"game/CDrawingTest"};
    AvpJson *j = trace("cdrawingtest", args, 1, &rc);
    REQUIRE(j);
    CHECK_EQ_I64(rc, 0);
    CHECK(str_is(j, "compiler", "Wii/1.0"));
    CHECK_MSG(!strcmp(selfcheck(j), "OK"), "self-check %s", selfcheck(j));
    const AvpJson *s = site_of(j, "fn_8000CFCC__12CDrawingTestFv", "fn_801785B4__12CDrawingTestFv", 1);
    REQUIRE(s);
    CHECK(str_is(s, "decision", "called"));
    CHECK(str_is(s, "rule", "not-visited")); /* no callee of fn_8000CFCC is a candidate: the visitor skips it */
    CHECK(str_starts(s, "detail", "auto-statements"));
    CHECK_EQ_I64(avp_json_get_i64(s, "limit", -1), 15); /* inline_max_auto_size */
    CHECK(avp_json_get_i64(s, "cost", -1) > 15);
    /* the same call under -inline noauto: still called, now because auto-inline is off */
    const char *args2[] = {"game/CDrawingTest", "--flag", "-inline noauto"};
    AvpJson *j2 = trace("cdrawingtest-noauto", args2, 3, &rc);
    REQUIRE(j2);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j2), "OK"));
    const AvpJson *s2 = site_of(j2, "fn_8000CFCC__12CDrawingTestFv", "fn_801785B4__12CDrawingTestFv", 1);
    REQUIRE(s2);
    CHECK(str_is(s2, "decision", "called"));
    CHECK(str_starts(s2, "detail", "noauto"));
}

/* revolution/os/OSNet (Wii/1.0): auto-inline expands the NWC24 entry points (dropping the unit's -inline noauto) */
AVP_TEST(osnet_auto_inlining) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"revolution/os/OSNet"};
    AvpJson *j = trace("osnet", args, 1, &rc);
    REQUIRE(j);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j), "OK"));
    const AvpJson *s = site_of(j, "__OSInitNet", "NWC24SuspendScheduler", 1);
    REQUIRE(s);
    CHECK(str_is(s, "decision", "called"));
    CHECK(str_starts(s, "detail", "noauto"));
    const char *args2[] = {"revolution/os/OSNet", "--drop-flag", "-inline noauto"};
    AvpJson *j2 = trace("osnet-auto", args2, 3, &rc);
    REQUIRE(j2);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j2), "OK"));
    const char *const expanded[][2] = {{"__OSInitNet", "NWC24SuspendScheduler"},
                                       {"__OSInitNet", "NWC24iPrepareShutdown"},
                                       {"__OSInitNet", "NWC24iSynchronizeRtcCounter"},
                                       {"NWC24Shutdown_", "NWC24iRequestShutdown"}};
    for (size_t i = 0; i < AVP_ARRAY_LEN(expanded); i++) {
        const AvpJson *e = site_of(j2, expanded[i][0], expanded[i][1], 1);
        CHECK_MSG(e, "%s -> %s: no site", expanded[i][0], expanded[i][1]);
        if (!e) continue;
        CHECK_MSG(str_is(e, "decision", "expanded") && str_is(e, "rule", "level0"), "%s -> %s", expanded[i][0], expanded[i][1]);
        CHECK(avp_str_contains(avp_json_get_str(e, "detail", AVP_S("")), AVP_S("candidate: auto-inline")));
    }
}

/* fmod/fmod_eventprojecti (Wii/1.0a): every MemPool::free site of loadSampleData, and where the object parts */
AVP_TEST(fmod_loadsampledata_free) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"fmod/fmod_eventprojecti"};
    AvpJson *j = trace("fmod-eventprojecti", args, 1, &rc);
    REQUIRE(j);
    CHECK(str_is(j, "compiler", "Wii/1.0a"));
    const char *caller = "loadSampleData__Q24FMOD13EventProjectIFPiiPPciUi", *free_ = "free__Q24FMOD7MemPoolFPvPCciUi";
    i64 nfree = 0;
    for (i64 k = 1; site_of(j, caller, free_, k); k++) {
        const AvpJson *s = site_of(j, caller, free_, k);
        nfree++;
        CHECK(str_is(s, "decision", "called"));
        CHECK(str_is(s, "rule", "not-candidate"));
        CHECK(avp_str_startswith(avp_json_get_str(s, "via", AVP_S("")), AVP_S("FMOD_Memory_FreeNotNull#")));
        const AvpJson *via = site_of(j, caller, "FMOD_Memory_FreeNotNull", k);
        CHECK(via && str_is(via, "decision", "expanded") && str_is(via, "rule", "level0"));
    }
    CHECK(nfree > 0);
    /* the trace and the post-inline IR agree; the object has fewer calls (removed after the inliner): TRACE-GAP */
    CHECK(!strcmp(selfcheck(j), "TRACE-GAP"));
    CHECK_EQ_I64(rc, 1);
    const AvpJson *gaps = avp_json_getz(avp_json_getz(j, "selfcheck"), "gaps");
    bool seen = false;
    for (size_t i = 0; i < avp_json_len(gaps); i++) {
        const AvpJson *g = avp_json_at(gaps, i);
        CHECK_MSG(!str_is(g, "stage", "trace-vs-ir") && !str_is(g, "stage", "trace-vs-object"), "gap stage %.*s",
                  AVP_SF(avp_json_get_str(g, "stage", AVP_S(""))));
        if (!str_is(g, "caller", caller) || !str_is(g, "callee", free_)) continue;
        seen = true;
        CHECK(str_is(g, "stage", "after-inliner"));
        CHECK_EQ_I64(avp_json_get_i64(g, "trace_calls", -1), nfree);
        CHECK_EQ_I64(avp_json_get_i64(g, "ir_calls", -1), nfree);
        CHECK(avp_json_get_i64(g, "object_calls", -1) < nfree);
    }
    CHECK(seen);
}

/* climax/RwPlugin (Matching, Wii/1.0): trace and object agree at every site */
AVP_TEST(rwplugin_control) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"climax/RwPlugin"};
    AvpJson *j = trace("rwplugin", args, 1, &rc);
    REQUIRE(j);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j), "OK"));
    size_t expanded = 0, called = 0;
    const AvpJson *cs = avp_json_getz(j, "callers");
    for (size_t i = 0; i < avp_json_len(cs); i++) {
        const AvpJson *ss = avp_json_getz(avp_json_at(cs, i), "sites");
        for (size_t k = 0; k < avp_json_len(ss); k++) {
            if (str_is(avp_json_at(ss, k), "decision", "expanded")) expanded++;
            else called++;
        }
    }
    CHECK(expanded > 0 && called > 0);
}

/* the probe fixture: one call per rule (tests/fixtures/probe_inline.cpp, CDrawingTest's flags) */
AVP_TEST(probe_rules) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"game/CDrawingTest", "--src", "tools/c/tools/mwtrace/tests/fixtures/probe_inline.cpp"};
    AvpJson *j = trace("probe-inline", args, 3, &rc);
    REQUIRE(j);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j), "OK"));
    const char *c1 = "caller1__Fi";
    const AvpJson *s;
    CHECK((s = site_of(j, c1, "leaf", 1)) && str_is(s, "rule", "level0") && str_is(s, "decision", "expanded"));
    CHECK((s = site_of(j, c1, "big15", 1)) && str_is(s, "decision", "expanded")); /* 15 statements: a candidate */
    CHECK((s = site_of(j, c1, "big__Fi", 1)) && str_starts(s, "detail", "auto-statements") &&
          avp_json_get_i64(s, "cost", -1) == 16 && avp_json_get_i64(s, "limit", -1) == 15);
    CHECK((s = site_of(j, c1, "kw", 1)) && str_is(s, "decision", "expanded"));
    CHECK((s = site_of(j, c1, "vakw__Fie", 1)) && str_is(s, "rule", "kind0"));
    CHECK((s = site_of(j, c1, "va__Fie", 1)) && str_starts(s, "detail", "auto-not-inlinable"));
    CHECK((s = site_of(j, c1, "ni__Fi", 1)) && str_starts(s, "detail", "never-inline"));
    CHECK((s = site_of(j, c1, "stmtk", 1)) && str_is(s, "decision", "expanded") &&
          avp_str_contains(avp_json_get_str(s, "detail", AVP_S("")), AVP_S("as statements")));
    CHECK((s = site_of(j, c1, "ext__Fi", 1)) && str_is(s, "via", "stmtk#1"));
    /* rec: level 0 (no limit), level 1 (22 <= 40), level 2 (22 <= 28), level 3 refused (22 > 12) */
    CHECK((s = site_of(j, c1, "rec__Fi", 2)) && str_is(s, "rule", "level1-cost") && avp_json_get_i64(s, "limit", -1) == 40);
    CHECK((s = site_of(j, c1, "rec__Fi", 3)) && str_is(s, "rule", "level2-cost") && avp_json_get_i64(s, "limit", -1) == 28);
    CHECK((s = site_of(j, c1, "rec__Fi", 4)) && str_is(s, "rule", "level3-cost") && str_is(s, "decision", "called") &&
          avp_json_get_i64(s, "cost", -1) == 22 && avp_json_get_i64(s, "limit", -1) == 12);
    CHECK((s = site_of(j, c1, "rec__Fi", 4)) && str_is(s, "via", "rec__Fi#3"));
}

/* the order fixture without -ipa file: per-function, bottom-up inlining; a callee defined later is no candidate yet */
/* the growth fixture: the visitor undoes every expansion of a caller that grew too much; the object calls them all */
AVP_TEST(probe_growth_revert) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"game/CDrawingTest", "--src", "tools/c/tools/mwtrace/tests/fixtures/probe_growth.cpp"};
    AvpJson *j = trace("probe-growth", args, 3, &rc);
    REQUIRE(j);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j), "OK"));
    const AvpJson *c = caller_of(j, "grow__Fi");
    REQUIRE(c);
    CHECK(avp_json_get_bool(c, "reverted", false));
    const AvpJson *op = avp_json_getz(c, "options");
    CHECK(avp_json_get_i64(op, "cost_after", 0) > 2000);
    CHECK(avp_json_get_i64(op, "cost_after", 0) > 4 * avp_json_get_i64(op, "cost_before", 0));
    for (i64 k = 1; k <= 10; k++) {
        const AvpJson *s = site_of(j, "grow__Fi", "heavy__Fi", k);
        CHECK_MSG(s && str_is(s, "decision", "called") && str_is(s, "rule", "growth-revert"), "heavy #%lld", (long long)k);
        if (s) CHECK_EQ_I64(avp_json_get_i64(s, "limit", -1), 2000);
    }
}

AVP_TEST(probe_order_without_ipa) {
    if (skip()) return;
    i64 rc = -1;
    const char *args[] = {"revolution/os/OSNet", "--src", "tools/c/tools/mwtrace/tests/fixtures/probe_order.c", "--drop-flag",
                          "-inline noauto", "--drop-flag", "-ipa file"};
    AvpJson *j = trace("probe-order", args, AVP_ARRAY_LEN(args), &rc);
    REQUIRE(j);
    CHECK_EQ_I64(rc, 0);
    CHECK(!strcmp(selfcheck(j), "OK"));
    const AvpJson *s;
    CHECK((s = site_of(j, "early", "later", 1)) && str_is(s, "decision", "called") && str_starts(s, "detail", "not-yet-defined"));
    CHECK((s = site_of(j, "after", "later", 1)) && str_is(s, "decision", "expanded") && str_is(s, "rule", "bottom-up-size") &&
          avp_json_get_i64(s, "cost", -1) <= avp_json_get_i64(s, "limit", -1));
}

AVP_TEST_MAIN("mwtrace_real_tree")
