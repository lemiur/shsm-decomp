/* gate_tools: the configure command and the rules step's linked sub-tools (gate.h, "the rule checker and the class
 * scanner"). The class scan (always the constructor/destructor base rule, whatever SHSM_CLASSES_BASE_RULE says) and
 * the rule checker run in process, each from its own library (tool_classes, tool_check_rules); their result is
 * shaped as a run so the steps read it as they read every other sub-tool. */
#include "gate.h"
#include "classes.h"
#include "rules_cli.h"
#include "shsm_cli.h"

const char *gate_configure_exe(AvpArena *a, const GateOpts *opts, const char *exe_dir) {
    if (opts->configure_exe) return opts->configure_exe;
    return avp_path_join(a, exe_dir ? exe_dir : avp_exe_dir(), "configure.exe");
}

static GateRun traceback_run(AvpArena *a, i64 rc, const char *text) {
    GateRun r = {0};
    r.returncode = rc;
    r.out = AVP_S("");
    r.err = avp_str(avp_strdup(a, text));
    return r;
}

/* ---- linked ---------------------------------------------------------------------------------------------------- */
GateRun gate_classes_scan_linked(void *ctx, AvpArena *a, const char *root) {
    (void)ctx;
    double t0 = avp_time();
    ClassesScanArgs sa = {0};
    sa.paths = classes_default_paths(a, root);
    if (!avp_isfile(sa.paths.splits)) sa.paths.splits = NULL;
    sa.base_rule = CLASSES_BASE_RULE_CTOR; /* the scanner's default, fixed here: SHSM_CLASSES_BASE_RULE is ignored */
    sa.root = root;
    AvpTry t;
    if (AVP_TRY(&t)) {
        classes_check_inputs(sa.paths.asm_dir, sa.paths.symbols);
        classes_run_scan(a, &sa, true);
        avp_try_end(&t);
        GateRun r = {0};
        r.out = avp_str(avp_sprintf(a, "classes scan (linked, base rule ctor): %s\n", sa.paths.out));
        r.err = AVP_S("");
        r.seconds = avp_time() - t0;
        return r;
    }
    if (t.code == SHSM_EXC_USAGE) return traceback_run(a, 2, avp_sprintf(a, "classes: error: %s\n", t.msg ? t.msg : ""));
    AvpStr text = shsm_exc_str(a, t.code, t.msg);
    return traceback_run(a, 1, avp_sprintf(a, "gate.exe: error: %s: %.*s\n", shsm_exc_class(t.code),
                                           AVP_SF(text)));
}

GateRun gate_check_rules_linked(void *ctx, AvpArena *a, const char *root, bool no_summary, const char *const *paths,
                                size_t n) {
    (void)ctx;
    double t0 = avp_time();
    RlRunOpts o = {0};
    o.root = root;
    o.json = true;
    o.no_summary = no_summary;
    for (size_t i = 0; i < n; i++) avp_vec_push(&o.paths, avp_str(paths[i]));
    AvpBuf out, err;
    avp_buf_init(&out, a);
    avp_buf_init(&err, a);
    GateRun r = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        r.returncode = rl_run(a, &o, &out, &err);
        avp_try_end(&t);
    } else {
        AvpStr text = shsm_exc_str(a, t.code, t.msg);
        avp_buf_printf(&err, "gate.exe: error: %s: %.*s\n", shsm_exc_class(t.code), AVP_SF(text));
        r.returncode = 1;
    }
    r.out = avp_buf_str(&out);
    r.err = avp_buf_str(&err);
    r.seconds = avp_time() - t0;
    return r;
}

/* ---- through a runner -------------------------------------------------------------------------------------------- */
static GateRun scan_via(void *ctx, AvpArena *a, const char *root) {
    GateRunnerTools *rt = ctx;
    const char *argv[] = {"classes", "scan"};
    return rt->runner(rt->runner_ctx, a, argv, 2, root, rt->env, 0);
}

static GateRun check_via(void *ctx, AvpArena *a, const char *root, bool no_summary, const char *const *paths,
                         size_t n) {
    GateRunnerTools *rt = ctx;
    const char **argv = AVP_NEWN(a, const char *, n + 5);
    size_t k = 0;
    argv[k++] = "check_rules";
    argv[k++] = "--json";
    argv[k++] = "--root";
    argv[k++] = root;
    if (no_summary) argv[k++] = "--no-summary";
    for (size_t i = 0; i < n; i++) argv[k++] = paths[i];
    return rt->runner(rt->runner_ctx, a, argv, k, root, rt->env, 0);
}

GateRulesTools gate_rules_tools_via_runner(GateRunnerTools *rt) { return (GateRulesTools){scan_via, check_via, rt}; }
