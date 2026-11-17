/* Real-tree tests of mwtrace regalloc (skipped under SHSM_NO_REAL_TREE=1): the known cases, each traced by
 * build/tools/mwtrace.exe into build/mwtrace-test/ra-<case> (its regalloc.json), asserting the self-check and the facts
 * docs/compiler_regalloc.md records. The --against texts are the solver's regression cases (tools/c/tools/solver/
 * tests/cases/*.case): their find/replace edits applied to the unit's source. Both compiler builds are covered
 * (FMOD with Wii/1.0a, RenderWare and the game with Wii/1.0). */
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

static const char *test_dir(const char *name) {
    AvpArena *a = avp_arena();
    return avp_join(a, shsm_repo_find(NULL)->root, "build", "mwtrace-test", avp_sprintf(a, "ra-%s", name));
}

/* mwtrace.exe regalloc <args...> --out build/mwtrace-test/ra-<name>: the parsed regalloc.json, *rc the exit code. */
static AvpJson *trace(const char *name, const char *const *args, size_t n, i64 *rc) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    AvpPtrVec argv = {0};
    avp_vec_push(&argv, (void *)avp_join(a, r->root, "build", "tools", "mwtrace.exe"));
    avp_vec_push(&argv, (void *)"regalloc");
    for (size_t i = 0; i < n; i++) avp_vec_push(&argv, (void *)args[i]);
    avp_vec_push(&argv, (void *)"--out");
    avp_vec_push(&argv, (void *)test_dir(name));
    ShsmRun res = shsm_run(a, (const char *const *)argv.v, argv.n, r->root, 600);
    *rc = res.returncode;
    AvpStr text = {0};
    AvpJsonError err;
    AvpJson *j = NULL;
    if (avp_read_text(a, avp_path_join(a, test_dir(name), "regalloc.json"), AVP_TM_UTF8, &text)) j = avp_json_parse(a, text, &err);
    if (!j) printf("%s: no JSON (exit %lld): %.*s\n", name, (long long)res.returncode, AVP_SF(shsm_run_output(a, &res)));
    return j;
}

/* The unit's source with a solver case's find/replace edits applied, written under the test's folder. */
static const char *case_text(const char *name, const char *casefile, const char *src) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    AvpStr c = {0}, s = {0};
    if (!avp_read_text(a, avp_join(a, r->root, "tools", "c", "tools", "solver", "tests", "cases", casefile), AVP_TM_UTF8, &c) ||
        !avp_read_text(a, avp_path_join(a, r->root, src), AVP_TM_UTF8, &s))
        return NULL;
    s = avp_str_replace(a, s, AVP_S("\r\n"), AVP_S("\n"), -1);
    c = avp_str_replace(a, c, AVP_S("\r\n"), AVP_S("\n"), -1);
    i64 at = 0;
    for (;;) {
        i64 f = avp_str_find(c, AVP_S("find <<EOT\n"), at, (i64)c.n);
        if (f < 0) break;
        i64 fs = f + 11, fe = avp_str_find(c, AVP_S("\nEOT\n"), fs, (i64)c.n);
        i64 r0 = avp_str_find(c, AVP_S("replace <<EOT\n"), fe, (i64)c.n);
        if (fe < 0 || r0 < 0) return NULL;
        i64 rs = r0 + 14, re = avp_str_find(c, AVP_S("\nEOT\n"), rs, (i64)c.n);
        if (re < 0) return NULL;
        AvpStr find = avp_str_slice(c, fs, fe + 1), repl = avp_str_slice(c, rs, re + 1);
        i64 k = avp_str_find(s, find, 0, (i64)s.n);
        if (k < 0) {
            printf("%s: the case's find text is not in %s\n", name, src);
            return NULL;
        }
        s = avp_str(avp_sprintf(a, "%.*s%.*s%.*s", (int)k, s.p, AVP_SF(repl), (int)(s.n - (size_t)k - find.n),
                                s.p + k + (i64)find.n));
        at = re + 5;
    }
    const char *dir = test_dir(name);
    avp_makedirs(dir, true);
    const char *ext = strrchr(src, '.');
    const char *out = avp_path_join(a, dir, avp_sprintf(a, "b%s", ext ? ext : ".c"));
    if (!avp_write_file(out, s.p, s.n)) return NULL;
    return out;
}

static bool check_ok(const AvpJson *side) {
    const AvpJson *sc = avp_json_getz(side, "self_check");
    return sc && avp_json_get_bool(sc, "ok", false);
}

static bool has_diff(const AvpJson *j, const char *prefix) {
    const AvpJson *d = avp_json_getz(j, "differences");
    for (size_t i = 0; i < avp_json_len(d); i++)
        if (avp_str_startswith(avp_json_str_or(avp_json_at(d, i), AVP_S("")), avp_str(prefix))) return true;
    printf("  no difference line starting \"%s\"\n", prefix);
    return false;
}

static bool any_diff_has(const AvpJson *j, const char *needle) {
    const AvpJson *d = avp_json_getz(j, "differences");
    for (size_t i = 0; i < avp_json_len(d); i++) {
        AvpStr s = avp_json_str_or(avp_json_at(d, i), AVP_S(""));
        if (avp_str_find(s, avp_str(needle), 0, (i64)s.n) >= 0) return true;
    }
    return false;
}

static void seconds(const char *name, const AvpJson *side) { printf("%s: %.2f s\n", name, avp_json_get_f64(side, "seconds", -1)); }

AVP_TEST(mempool_set_against) {
    if (skip()) return;
    const char *b = case_text("mempool", "mempool_set.case", "src/fmod/fmod_memory.cpp");
    REQUIRE(b != NULL);
    const char *args[] = {"main/fmod/fmod_memory", "--function", "set__Q24FMOD7MemPoolFiii", "--against", b};
    i64 rc = -1;
    AvpJson *j = trace("mempool", args, 5, &rc);
    REQUIRE(j != NULL);
    CHECK_EQ_I64(rc, 0);
    const AvpJson *A = avp_json_getz(j, "a"), *B = avp_json_getz(j, "b");
    CHECK(avp_str_eqz(avp_json_get_str(A, "compiler", AVP_S("")), "Wii/1.0a"));
    CHECK(check_ok(A) && check_ok(B));
    seconds("mempool A", A);
    /* the closing edit adds nodes to A (two compiler-made objects in the locals list); the named variables keep
     * their registers (only temporaries move) */
    CHECK(has_diff(j, "GPR nodes only in A:"));
    CHECK(!any_diff_has(j, "`bitoffset`"));
    CHECK(!any_diff_has(j, "`byteoffset`"));
}

AVP_TEST(worldsynccamera_r30_against) {
    if (skip()) return;
    const char *b = case_text("wsc", "worldsynccamera_r30.case", "src/rwsdk/world/baworobj.c");
    REQUIRE(b != NULL);
    const char *args[] = {"main/rwsdk/world/baworobj", "--function", "WorldSyncCamera", "--against", b};
    i64 rc = -1;
    AvpJson *j = trace("wsc", args, 5, &rc);
    REQUIRE(j != NULL);
    CHECK_EQ_I64(rc, 0);
    const AvpJson *A = avp_json_getz(j, "a"), *B = avp_json_getz(j, "b");
    CHECK(avp_str_eqz(avp_json_get_str(A, "compiler", AVP_S("")), "Wii/1.0"));
    CHECK(check_ok(A) && check_ok(B));
    seconds("WorldSyncCamera A", A);
    /* spSect declared last (A) is the lowest local node and is coloured after the five locals declared before it;
     * declared after spaStack (B) it is the highest and coloured first: the five shift one register down */
    CHECK(has_diff(j, "`spSect`: r26 in A, r31 in B; node 40 in A, 45 in B; selected 6th in A, 1st in B"));
    CHECK(has_diff(j, "`plpPlanes`: r31 in A, r30 in B; node 45 in A, 44 in B; selected 1st in A, 2nd in B"));
    CHECK(has_diff(j, "`position`: r27 in A, r26 in B"));
}

AVP_TEST(freelistfreereal_r30_against) {
    if (skip()) return;
    const char *b = case_text("freelist", "freelistfreereal_r30.case", "src/rwsdk/src/plcore/bamemory.c");
    REQUIRE(b != NULL);
    const char *args[] = {"main/rwsdk/src/plcore/bamemory", "--function", "_rwFreeListFreeReal", "--against", b};
    i64 rc = -1;
    AvpJson *j = trace("freelist", args, 5, &rc);
    REQUIRE(j != NULL);
    CHECK_EQ_I64(rc, 0);
    const AvpJson *A = avp_json_getz(j, "a"), *B = avp_json_getz(j, "b");
    CHECK(check_ok(A) && check_ok(B));
    seconds("_rwFreeListFreeReal A", A);
    /* heap at function scope (A) is node 40, selected first, lowest free volatile r5; in the while block (B) it is
     * node 36, selected third: heapEntries and link take r5 and r6 */
    CHECK(has_diff(j, "`heap`: r5 in A, r7 in B; node 40 in A, 36 in B; selected 1st in A, 3rd in B"));
    CHECK(has_diff(j, "`heapEntries`: r6 in A, r5 in B"));
    CHECK(has_diff(j, "`link`: r7 in A, r6 in B"));
}

AVP_TEST(resampler_large) {
    if (skip()) return;
    const char *args[] = {"main/fmod/fmod_dsp_resampler", "--function",
                          "execute__Q24FMOD12DSPResamplerFPfPPfPUiiPi16FMOD_SPEAKERMODEUi"};
    i64 rc = -1;
    AvpJson *j = trace("resampler", args, 3, &rc);
    REQUIRE(j != NULL);
    CHECK_EQ_I64(rc, 0);
    CHECK(check_ok(j));
    seconds("DSPResampler::execute", j);
    /* the GPR class spills once: two attempts, the last coloured */
    const AvpJson *ps = avp_json_getz(j, "passes");
    int gpr = 0;
    bool coloured = false;
    for (size_t i = 0; i < avp_json_len(ps); i++) {
        const AvpJson *p = avp_json_at(ps, i);
        if (!avp_str_eqz(avp_json_get_str(p, "class", AVP_S("")), "GPR")) continue;
        gpr++;
        if (avp_json_get_bool(p, "final", false)) coloured = avp_json_get_bool(p, "coloured", false);
    }
    CHECK_EQ_I64(gpr, 2);
    CHECK(coloured);
}

AVP_TEST(game_functions_self_check) {
    if (skip()) return;
    static const char *const CASES[][2] = {
        {"main/game/CPointOfInterest", "fn_800C5320__16CPointOfInterestFPC9unk_RwV3d"},
        {"main/game/CPsyPointOfInterest", "fn_801A73A8__19CPsyPointOfInterestFRC19unk_AttributePacket"},
        {"main/game/unk_8000A040", "fn_8000A76C__12unk_8000A040Ff"},
    };
    for (size_t i = 0; i < 3; i++) {
        const char *args[] = {CASES[i][0], "--function", CASES[i][1]};
        i64 rc = -1;
        AvpJson *j = trace(avp_sprintf(avp_arena(), "game%zu", i), args, 3, &rc);
        CHECK(j != NULL);
        if (!j) continue;
        CHECK_EQ_I64(rc, 0);
        CHECK(check_ok(j));
        seconds(CASES[i][1], j);
    }
}

AVP_TEST(matching_control) {
    if (skip()) return;
    const char *args[] = {"main/climax/RwPlugin", "--function", "ClimaxP1EffectPluginAttach__Fv"};
    i64 rc = -1;
    AvpJson *j = trace("control", args, 3, &rc);
    REQUIRE(j != NULL);
    CHECK_EQ_I64(rc, 0);
    CHECK(check_ok(j));
}

AVP_TEST_MAIN()
