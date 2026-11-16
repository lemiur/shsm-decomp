/* PendingFlipTests of tools/test_gate.py: --pending-flip, the gate of `tools/unit.py flip`.
 *   test_check_accepts_exactly_the_one_status_change -> check_accepts_exactly_the_one_status_change
 *   test_check_refuses_any_other_difference          -> check_refuses_any_other_difference
 *   test_preflight_passes_only_the_verified_flip     -> preflight_passes_only_the_verified_flip
 *   test_preflight_refuses_other_paths_and_other_edits -> preflight_refuses_other_paths_and_other_edits
 *   test_green_pending_run_is_ok_pending_never_ok    -> green_pending_run_is_ok_pending_never_ok
 *   test_pending_flip_refuses_other_modes            -> pending_flip_refuses_other_modes */
#include "gate_fixtures.h"

static const char CFG_HEAD[] = "config.libs = [\n"
                               "    # Object(NonMatching, \"revolution/os/OSNet.c\"),\n"
                               "    Object(NonMatching, \"revolution/os/OSNet.c\", extra_cflags=[\"-inline noauto\"]),\n"
                               "    Object(NonMatching, \"revolution/dsp/dsp_task.c\"),\n"
                               "    Object(Matching, \"revolution/os/OSInit.c\"),\n"
                               "    Object(Equivalent, \"game/Eq.cpp\"),\n"
                               "]\n";

/* flipped(text, path): the first uncommented Object(NonMatching, "<path>" becomes Matching */
static const char *flipped(const char *text, const char *path) {
    AvpStr t = avp_str(text);
    const char *old = avp_sprintf(TA, "Object(NonMatching, \"%s\"", path);
    i64 start = avp_str_find(t, AVP_S("    Object"), 0, AVP_END);
    i64 i = avp_str_find(t, avp_str(old), start, AVP_END);
    AVP_ASSERT(i >= 0);
    return avp_sprintf(TA, "%.*sObject(Matching, \"%s\"%s", (int)i, text, path, text + i + strlen(old));
}

static const char *check(const char *work, const char *unit) {
    return gate_check_pending_flip(TA, avp_str(CFG_HEAD), avp_str(work), unit);
}

AVP_TEST(check_accepts_exactly_the_one_status_change) {
    const char *work = flipped(CFG_HEAD, "revolution/os/OSNet.c");
    static const char *const names[] = {"revolution/os/OSNet.c", "revolution/os/OSNet", "main/revolution/os/OSNet"};
    for (size_t i = 0; i < 3; i++) CHECK_CZ(check(work, names[i]), "");
    const char *crlf = avp_cz(TA, avp_str_replace(TA, avp_str(work), AVP_S("\n"), AVP_S("\r\n"), -1));
    CHECK_CZ(check(crlf, "revolution/os/OSNet"), "");
}

AVP_TEST(check_refuses_any_other_difference) {
    const char *net = flipped(CFG_HEAD, "revolution/os/OSNet.c");
    const char *both = flipped(net, "revolution/dsp/dsp_task.c");
    const char *inline_auto =
        avp_cz(TA, avp_str_replace(TA, avp_str(net), AVP_S("\"-inline noauto\""), AVP_S("\"-inline auto\""), -1));
    const char *extra = avp_sprintf(TA, "%s# extra\n", net);
    const char *osinit = avp_cz(TA, avp_str_replace(TA, avp_str(CFG_HEAD), AVP_S("Matching, \"revolution/os/OSInit"),
                                                    AVP_S("NonMatching, \"revolution/os/OSInit"), -1));
    typedef struct {
        const char *work, *unit, *why;
    } Case;
    Case cases[8] = {{CFG_HEAD, "revolution/os/OSNet", "unchanged"},
                     {both, "revolution/os/OSNet", "more than"},
                     {inline_auto, "revolution/os/OSNet", "more than"},
                     {extra, "revolution/os/OSNet", "more than"},
                     {net, "revolution/dsp/dsp_task", "more than"},
                     {osinit, "revolution/os/OSInit", "Matching at HEAD"},
                     {CFG_HEAD, "game/Eq", "Equivalent at HEAD"},
                     {CFG_HEAD, "game/Nope", "no Object entry"}};
    for (size_t i = 0; i < 8; i++) {
        const char *got = check(cases[i].work, cases[i].unit);
        CHECK_MSG(tf_contains(got, cases[i].why), "%s: %s", cases[i].why, got);
    }
}

static TfRunner PRE_RUNNER;
static TfEntry PRE_ENTRIES[2];

static void no_problems(Gate *g, AvpStrVec *problems) {
    (void)g;
    (void)problems;
}

static GateStep *preflight(const char *status, size_t status_n, const char *work) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, NULL);
    tf_write(tf_join(tmp, "configure.py"), work);
    PRE_ENTRIES[0] = (TfEntry){"status", 0, status, NULL, NULL, status_n};
    PRE_ENTRIES[1] = (TfEntry){"show", 0, CFG_HEAD, NULL, NULL};
    PRE_RUNNER = (TfRunner){PRE_ENTRIES, 2, {0}};
    GateOpts o = tf_opts();
    o.pending_flip = "revolution/os/OSNet";
    AvpBuf *out = AVP_NEW(TA, AvpBuf);
    avp_buf_init(out, TA);
    Gate *g = gate_new(TA, tmp, &o, tf_run, &PRE_RUNNER, out);
    g->check_tools = no_problems;
    GateStep *st = AVP_NEW(TA, GateStep);
    gate_step_init(TA, st, "preflight");
    gate_step_preflight(g, st);
    return st;
}

/* the status text holds NULs: give its length */
#define ST(lit) lit, sizeof(lit) - 1

AVP_TEST(preflight_passes_only_the_verified_flip) {
    GateStep *st = preflight(ST(" M configure.py\0"), flipped(CFG_HEAD, "revolution/os/OSNet.c"));
    CHECK_MSG(st->status == GATE_PASS, "%s", st->reason);
    CHECK(tf_contains(st->reason, "pending flip"));
}

AVP_TEST(preflight_refuses_other_paths_and_other_edits) {
    const char *work = flipped(CFG_HEAD, "revolution/os/OSNet.c");
    GateStep *st = preflight(ST(" M configure.py\0 M src/a.c\0"), work);
    CHECK(st->status == GATE_FAIL && tf_contains(st->reason, "other than configure.py"));
    st = preflight(ST(" M configure.py\0?? src/new.c\0"), work);
    CHECK(st->status == GATE_FAIL && tf_contains(st->reason, "other than configure.py"));
    st = preflight(ST(""), work);
    CHECK(st->status == GATE_FAIL && tf_contains(st->reason, "unchanged"));
    st = preflight(ST(" M configure.py\0"), flipped(work, "revolution/dsp/dsp_task.c"));
    CHECK_MSG(st->status == GATE_FAIL && tf_contains(st->reason, "more than"), "%s", st->reason);
}

/* the steps scripted to pass; preflight leaves configure.py as the only dirty path */
static const char *LAYOUT_OUTCOME;

static void dirty_step(Gate *g, GateStep *st) {
    if (strcmp(st->name, "layout") == 0 && LAYOUT_OUTCOME) {
        gate_step_fail(st, "layout broke");
        return;
    }
    gate_step_ok(st, "fine");
    if (strcmp(st->name, "checksum") == 0) g->dol_sha1 = "6a3964eaf1ebe8225283363e4161a793a03683d3";
    if (strcmp(st->name, "coverage") == 0) {
        g->units_linked = 3;
        g->functions_linked = 30;
    }
    if (strcmp(st->name, "preflight") == 0) {
        g->dirty = (AvpStrVec){0};
        avp_vec_push(&g->dirty, AVP_S("configure.py"));
    }
}

static void script_dirty(Gate *g, void *ctx) {
    (void)ctx;
    for (size_t i = 0; i < GATE_NSTEPS; i++) g->step_fn[i] = dirty_step;
}

AVP_TEST(green_pending_run_is_ok_pending_never_ok) {
    const char *tmp = tf_tmpdir();
    tf_make_root(tmp, NULL, NULL);
    LAYOUT_OUTCOME = NULL;
    TfRunner r = tf_git_runner();
    GateOpts o = tf_opts();
    o.pending_flip = "revolution/os/OSNet";
    Gate *g = gate_new(TA, tmp, &o, tf_run, &r, AVP_NEW(TA, AvpBuf));
    avp_buf_init(g->out, TA);
    script_dirty(g, NULL);
    const char *line;
    bool ok = gate_execute(g, &line);
    CHECK(!ok);
    CHECK_MSG(tf_starts(line, "GATE OK-PENDING "), "%s", line);
    CHECK(!tf_contains(line, "GATE OK "));
    CHECK(tf_contains(line, "pending-flip=revolution/os/OSNet"));
    CHECK_STR(avp_json_get_str(g->snapshot, "result", AVP_S("")), "OK-PENDING");
    CHECK(!avp_exists(tf_join(tmp, "build/gate/last-ok.json")));
    GateMainHooks h = {tf_run, &r, NULL, NULL, script_dirty, NULL};
    char *argv[] = {"gate", "--root", (char *)tmp, "--pending-flip", "revolution/os/OSNet"};
    int rc = gate_main(5, argv, &h);
    CHECK_EQ_I64(rc, GATE_EXIT_OK_PENDING);
    CHECK(rc != 0);
    /* a red pending run is plain GATE RED */
    LAYOUT_OUTCOME = "fail";
    g = gate_new(TA, tmp, &o, tf_run, &r, AVP_NEW(TA, AvpBuf));
    avp_buf_init(g->out, TA);
    script_dirty(g, NULL);
    ok = gate_execute(g, &line);
    CHECK(!ok);
    CHECK_MSG(tf_starts(line, "GATE RED layout: "), "%s", line);
    CHECK(!g->pending_ok);
    LAYOUT_OUTCOME = NULL;
    avp_tempdir_cleanup(tmp);
}

AVP_TEST(pending_flip_refuses_other_modes) {
    static const char *const extra[] = {"--allow-dirty", "--quick", "--selftest"};
    for (size_t i = 0; i < 3; i++) {
        TfRunner r = tf_git_runner();
        GateMainHooks h = {tf_run, &r, NULL, NULL, NULL, NULL};
        char *argv[] = {"gate", "--pending-flip", "x", (char *)extra[i]};
        CHECK_EQ_I64(gate_main(4, argv, &h), 2);
    }
}

AVP_TEST_MAIN("gate")
