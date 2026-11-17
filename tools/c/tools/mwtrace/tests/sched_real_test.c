/* Real-tree tests of mwtrace sched (skipped under SHSM_NO_REAL_TREE=1): the known cases, each traced by
 * build/tools/mwtrace.exe into build/mwtrace-test/sched-<case> (its sched.json), asserting the self-check and the facts
 * docs/compiler_regalloc.md ("Scheduler") records. Both compiler builds are covered (FMOD with Wii/1.0a, RenderWare and
 * the game with Wii/1.0); the fixtures are tests/fixtures/sched_*.c, compiled with game/unk_8000A040's flags. */
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
    return avp_join(a, shsm_repo_find(NULL)->root, "build", "mwtrace-test", avp_sprintf(a, "sched-%s", name));
}

static const char *fixture(const char *name) {
    AvpArena *a = avp_arena();
    return avp_join(a, shsm_repo_find(NULL)->root, "tools", "c", "tools", "mwtrace", "tests", "fixtures", name);
}

/* mwtrace.exe sched <args...> --out build/mwtrace-test/sched-<name>: the parsed sched.json, *rc the exit code. */
static AvpJson *trace(const char *name, const char *const *args, size_t n, i64 *rc) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = shsm_repo_find(NULL);
    AvpPtrVec argv = {0};
    avp_vec_push(&argv, (void *)avp_join(a, r->root, "build", "tools", "mwtrace.exe"));
    avp_vec_push(&argv, (void *)"sched");
    for (size_t i = 0; i < n; i++) avp_vec_push(&argv, (void *)args[i]);
    avp_vec_push(&argv, (void *)"--out");
    avp_vec_push(&argv, (void *)test_dir(name));
    ShsmRun res = shsm_run(a, (const char *const *)argv.v, argv.n, r->root, 600);
    *rc = res.returncode;
    AvpStr text = {0};
    AvpJsonError err;
    AvpJson *j = NULL;
    if (avp_read_text(a, avp_path_join(a, test_dir(name), "sched.json"), AVP_TM_UTF8, &text)) j = avp_json_parse(a, text, &err);
    if (!j) printf("%s: no JSON (exit %lld): %.*s\n", name, (long long)res.returncode, AVP_SF(shsm_run_output(a, &res)));
    return j;
}

static AvpJson *at(AvpJson *v, size_t i) { return v ? avp_json_at(v, i) : NULL; }
static AvpJson *get(AvpJson *v, const char *k) { return v ? avp_json_getz(v, k) : NULL; }
static i64 num(AvpJson *v) { return v ? avp_json_i64(v, -999) : -999; }
static AvpStr str(AvpJson *v) { return avp_json_str_or(v, AVP_S("")); }

static bool no_gaps(AvpJson *j) { return j && avp_json_len(get(get(j, "check"), "gaps")) == 0; }

static bool has_str(AvpJson *arr, const char *part) {
    for (size_t i = 0; i < avp_json_len(arr); i++)
        if (avp_str_contains(str(at(arr, i)), avp_str(part))) return true;
    return false;
}

/* key 4 in pass 1: in probe, ADDI (priority 2) is issued over LI (4) at cycle 1; swapping the source order of
 * `i = 0` and `p = table` gives the scheduler the same input, so nothing differs */
AVP_TEST(key4_against) {
    if (skip()) return;
    const char *args[] = {"main/game/unk_8000A040", "--function", "probe", "--src", fixture("sched_key4_a.c"), "--against",
                          fixture("sched_key4_b.c")};
    i64 rc;
    AvpJson *j = trace("key4", args, 7, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(get(j, "a")) && no_gaps(get(j, "b")));
    CHECK(has_str(get(j, "differences"), "no selection differs"));
    AvpJson *b0 = at(get(at(get(get(j, "a"), "passes"), 0), "blocks"), 0);
    AvpJson *s2 = at(get(b0, "selections"), 2);
    CHECK_EQ_I64(num(get(s2, "cycle")), 1);
    CHECK(avp_str_eqz(str(get(at(get(b0, "nodes"), num(get(s2, "chosen"))), "op")), "ADDI"));
    CHECK(avp_str_eqz(str(get(at(get(s2, "comparisons"), 0), "key")), "priority"));
}

/* the first-function quirk: the dead entry copy is a pass-1 node only when probe is the file's first function */
AVP_TEST(first_function) {
    if (skip()) return;
    const char *args[] = {"main/game/unk_8000A040", "--function", "probe", "--src", fixture("sched_first.c"), "--against",
                          fixture("sched_second.c")};
    i64 rc;
    AvpJson *j = trace("first", args, 7, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(has_str(get(j, "differences"), "pass 1 block 0: node only in A: #0 MR r32= r3"));
    CHECK(has_str(get(j, "differences"), "first difference: pass 1 block 0"));
}

static bool has_mem_edge(AvpJson *block) {
    AvpJson *es = get(block, "edges");
    for (size_t i = 0; i < avp_json_len(es); i++)
        if (has_str(get(at(es, i), "kinds"), "mem-")) return true;
    return false;
}

/* memory: through a plain pointer each load is held behind the stores to dst (may_alias 1); through a pointer to
 * const no pair aliases */
AVP_TEST(memory_dependence) {
    if (skip()) return;
    const char *pa[] = {"main/game/unk_8000A040", "--function", "copy_plain", "--src", fixture("sched_alias.c")};
    const char *ca[] = {"main/game/unk_8000A040", "--function", "copy_const", "--src", fixture("sched_alias.c")};
    i64 rc;
    AvpJson *p = trace("alias-plain", pa, 5, &rc);
    CHECK_EQ_I64(rc, 0);
    AvpJson *pb = at(get(at(get(p, "passes"), 0), "blocks"), 0);
    CHECK(has_mem_edge(pb));
    AvpJson *c = trace("alias-const", ca, 5, &rc);
    CHECK_EQ_I64(rc, 0);
    AvpJson *cb = at(get(at(get(c, "passes"), 0), "blocks"), 0);
    CHECK(!has_mem_edge(cb));
    AvpJson *ms = get(cb, "memory");
    CHECK(avp_json_len(ms) > 0);
    for (size_t i = 0; i < avp_json_len(ms); i++) CHECK_EQ_I64(num(get(at(ms, i), "may_alias")), 0);
}

/* FMOD (Wii/1.0a): enableDoubleBuffer passes the self-check; at the first differing rows against retail, mr r28, r4
 * (row 9) waits for stw r28 (row 8) by a war edge, and divwu (row 34) for lwz r4 by a raw edge of latency 2 */
AVP_TEST(fmod_enable_double_buffer) {
    if (skip()) return;
    const char *args[] = {"main/fmod/fmod_file", "--function", "enableDoubleBuffer__Q24FMOD4FileFUiPv", "--pair", "@7,@9"};
    i64 rc;
    AvpJson *j = trace("edb", args, 5, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(j));
    CHECK(avp_str_eqz(str(get(j, "build")), "Wii/1.0a"));
    AvpJson *pr = get(j, "pair");
    CHECK(!avp_json_bool_or(get(pr, "both_ready"), true));
    CHECK(avp_str_contains(str(get(pr, "held")), AVP_S("edge war r28, latency 0")));
    const char *args2[] = {"main/fmod/fmod_file", "--function", "enableDoubleBuffer__Q24FMOD4FileFUiPv", "--pair", "@31,@34"};
    AvpJson *j2 = trace("edb2", args2, 5, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(avp_str_contains(str(get(get(j2, "pair"), "held")), AVP_S("edge raw r4, latency 2")));
}

/* RenderWare (Wii/1.0): TriListNBTDataSetup8's first differing row is an order difference: clrlslwi of r5 after the
 * two of r0; pass 2 keeps it behind by height 14 > 9, pass 1 by program order */
AVP_TEST(nbt_setup) {
    if (skip()) return;
    const char *args[] = {"main/rwsdk/plugin/matfx/gcn/nbtGen", "--function", "TriListNBTDataSetup8", "--pair", "@43,@45"};
    i64 rc;
    AvpJson *j = trace("trilist", args, 5, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(j));
    AvpJson *pr = get(j, "pair");
    CHECK(avp_json_bool_or(get(pr, "both_ready"), false));
    CHECK(avp_str_eqz(str(get(pr, "key")), "height"));
    CHECK(avp_str_contains(str(get(pr, "key_words")), AVP_S("height 14 > 9")));
    const char *a1[] = {"main/rwsdk/plugin/matfx/gcn/nbtGen", "--function", "TriListNBTDataSetup8", "--pass", "1", "--pair",
                        "@43,@45"};
    AvpJson *j1 = trace("trilist1", a1, 7, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(avp_str_eqz(str(get(get(j1, "pair"), "key")), "order"));
    const char *s[] = {"main/rwsdk/plugin/matfx/gcn/nbtGen", "--function", "TriStripNBTDataSetup8"};
    AvpJson *js = trace("tristrip", s, 3, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(js));
}

/* game (Wii/1.0): in fn_8000A76C's pass 1, the LFS regalloc numbers #300 is issued at cycle 0 before the LFD it
 * numbers #298, by key 1 (the LFS is critical: slack 0; the LFD's slack is 4) */
AVP_TEST(unk_8000A040_loads) {
    if (skip()) return;
    const char *args[] = {"main/game/unk_8000A040", "--function", "fn_8000A76C__12unk_8000A040Ff", "--pass", "1",
                          "--block", "36", "--pair", "0,7"};
    i64 rc;
    AvpJson *j = trace("a76c", args, 9, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(j));
    AvpJson *nodes = get(at(get(at(get(j, "passes"), 0), "blocks"), 36), "nodes");
    CHECK(avp_str_eqz(str(get(at(nodes, 0), "op")), "LFS") && num(get(at(nodes, 0), "ra")) == 300);
    CHECK(avp_str_eqz(str(get(at(nodes, 7), "op")), "LFD") && num(get(at(nodes, 7), "ra")) == 298);
    AvpJson *pr = get(j, "pair");
    CHECK_EQ_I64(num(get(pr, "first")), 0);
    CHECK_EQ_I64(num(get(pr, "cycle")), 0);
    CHECK(avp_str_eqz(str(get(pr, "key")), "critical"));
}

/* controls: a function of a Matching unit in each build, both passes, the self-check clean */
AVP_TEST(controls) {
    if (skip()) return;
    const char *g[] = {"main/game/PadRumble", "--function", "fn_80006860__8unk_RampFfff"};
    const char *f[] = {"main/fmod/fmod_memory", "--function", "set__Q24FMOD7MemPoolFiii"};
    i64 rc;
    AvpJson *jg = trace("ctl-game", g, 3, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(jg));
    CHECK_EQ_I64(avp_json_len(get(jg, "passes")), 2);
    CHECK(avp_str_eqz(str(get(jg, "build")), "Wii/1.0"));
    AvpJson *jf = trace("ctl-fmod", f, 3, &rc);
    CHECK_EQ_I64(rc, 0);
    CHECK(no_gaps(jf));
    CHECK_EQ_I64(avp_json_len(get(jf, "passes")), 2);
    CHECK(avp_str_eqz(str(get(jf, "build")), "Wii/1.0a"));
}

AVP_TEST_MAIN()
