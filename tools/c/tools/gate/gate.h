/* gate: the byte gate as one command (a port of python tools/gate.py): preflight, clean rebuild, checksum,
 * coverage, regressions, layout, rules, ledger; --quick, --selftest and --pending-flip.
 *
 * Every subprocess goes through an injectable runner (Gate.runner, the Python's `runner=` parameter), so the tests
 * and the differential stub answer the build, the checksum, git and the sub-tools. The steps themselves are
 * function pointers (Gate.step_fn) so a test can script them, as test_gate.py's ScriptedGate subclass does; the
 * doctor check (Gate.check_tools), the unit table (Gate.load_units) and the quick mode's classification
 * (GateQuick.classify) are hooks for the same reason.
 *
 * Output goes to `out` (an AvpBuf) when non-NULL, else to stdout (flushed after every line, as print(flush=True)).
 * Errors the Python raised as cli.UsageError are shsm_raise(SHSM_EXC_USAGE, ...); main maps them to
 * "gate: error: <msg>" and exit 2. */
#pragma once
#include "avp/avp.h"
#include "shsm_cli.h"
#include "shsm_report.h"

#define GATE_NSTEPS 8
#define GATE_EPS 1e-6
enum { GATE_EXIT_OK_PENDING = 3 };
extern const char *const gate_step_names[GATE_NSTEPS]; /* STEPS */
bool gate_needs_build(const char *step);                /* NEEDS_BUILD */
extern const char *const gate_allow_dirty[];            /* ALLOW_DIRTY, NULL-terminated */

/* ---- the runner (cli.run) --------------------------------------------------------------------------------------- */
typedef struct GateRun {
    i64 returncode;   /* -1 with timed_out (Python's None) */
    bool timed_out;
    AvpStr out, err;  /* text (utf-8, errors="replace", newlines translated) */
    double seconds;
} GateRun;
/* runner(argv, cwd=cwd, env=env, timeout=timeout). env NULL: inherit; timeout <= 0: none. A program that cannot
 * be found raises SHSM_EXC_USAGE "cannot run <argv0>: [WinError 2] ..." (cli.run's UsageError). */
typedef GateRun (*GateRunner)(void *ctx, AvpArena *a, const char *const *argv, size_t argc, const char *cwd,
                              const AvpEnv *env, double timeout);
GateRun gate_default_runner(void *ctx, AvpArena *a, const char *const *argv, size_t argc, const char *cwd,
                            const AvpEnv *env, double timeout);
static inline bool gate_run_ok(const GateRun *r) { return !r->timed_out && r->returncode == 0; }
AvpStr gate_run_output(AvpArena *a, const GateRun *r);   /* stdout + stderr */
const char *gate_rc_str(AvpArena *a, const GateRun *r);  /* "%s" % returncode: "None" for a timeout */

/* ---- small helpers ---------------------------------------------------------------------------------------------- */
char *gate_sha1_file(AvpArena *a, const char *path);     /* hex; raises the OSError on failure */
AvpStr gate_plain(AvpArena *a, AvpStr text);             /* without ANSI colour codes */
AvpStrVec gate_tail(AvpArena *a, AvpStr text, size_t n); /* the last n non-blank lines of plain(text) */
bool gate_is_allowed_dirty(AvpArena *a, const char *path);
typedef struct GatePorcelain {
    AvpStr code, path;
} GatePorcelain;
typedef AVP_VEC(GatePorcelain) GatePorcelainVec;
GatePorcelainVec gate_parse_porcelain_z(AvpStr text);    /* parse_porcelain_z */
/* parse_json_text: the first JSON object of a tool's output; NULL with *err ("no JSON object in the output", or
 * the decoder's message) when there is none. */
AvpJson *gate_parse_json_text(AvpArena *a, AvpStr text, const char **err);
/* str(v) of a JSON value as Python prints it after json.load: None, True, 12, 1.5, the text of a string. */
const char *gate_json_pystr(AvpArena *a, const AvpJson *v);
/* repr of a list of strings: ['a', "b'c"]. */
const char *gate_repr_str_list(AvpArena *a, const AvpStr *items, size_t n);
/* A path below top that is a junction or symlink, or NULL. */
const char *gate_has_reparse_points(AvpArena *a, const char *top);
/* remove_tree: raises SHSM_EXC_USAGE when a link is below top; else shutil.rmtree with the read-only fix. */
void gate_remove_tree(AvpArena *a, const char *top);
void gate_link_or_copy(AvpArena *a, const char *src, const char *dst); /* hard link, else copy2 (raises) */

/* ---- the hash contract ------------------------------------------------------------------------------------------ */
typedef struct GateContract {
    const char *sha1, *path, *yml_hash, *retail;
} GateContract;
/* read_contract: false with *err (str(e) of the OSError / ValueError) when it cannot be read. */
bool gate_read_contract(AvpArena *a, const char *build_sha1, const char *config_yml, const char *root,
                        GateContract *out, const char **err);
/* verify_contract -> ok; *why the reason, *sha1 the contract (NULL when unreadable). retail NULL: config.yml's. */
bool gate_verify_contract(AvpArena *a, const char *build_sha1, const char *config_yml, const char *root,
                          const char *retail, const char **why, const char **sha1);

/* ---- --pending-flip --------------------------------------------------------------------------------------------- */
const char *gate_unit_stem(AvpArena *a, const char *unit);
/* check_pending_flip: "" when work is head with exactly unit's status NonMatching -> Matching, else the reason. */
const char *gate_check_pending_flip(AvpArena *a, AvpStr head, AvpStr work, const char *unit);

/* ---- options ---------------------------------------------------------------------------------------------------- */
typedef struct GateOpts {
    const char *root;         /* NULL: the checkout of the executable */
    bool quick, selftest, keep_going, merge, allow_dirty, no_objcache, verbose, json, debug;
    const char *since;        /* NULL: none */
    const char *pending_flip; /* NULL: none */
    i64 jobs;                 /* 0: none */
    const char *configure_exe; /* --configure-exe (absolute), NULL: configure.exe beside the gate */
} GateOpts;

/* The configure command (tools/c/tools/configure/DESIGN.md): --configure-exe, else configure.exe in exe_dir (the
 * folder of the running gate, build/tools/). It reads configure.py in the directory it runs in. */
const char *gate_configure_exe(AvpArena *a, const GateOpts *opts, const char *exe_dir);

/* ---- the rule checker and the class scanner --------------------------------------------------------------------- */
/* The rules step's sub-tools, shaped as runs (exit status, stdout, stderr). The defaults (a NULL function) are the
 * linked tools: the class scan of tool_classes with its default base rule (the constructor/destructor evidence,
 * the owner's ruling of 2026-11-13) and tool_check_rules' main() (rl_run). The tests and the stub route them
 * through their scripted runner instead (gate_rules_tools_via_runner). */
typedef struct GateRulesTools {
    /* `classes scan` in root: keeps or writes <root>/build/<version>/classes.json */
    GateRun (*scan)(void *ctx, AvpArena *a, const char *root);
    /* `check_rules --json [--no-summary] [paths...]` with --root root */
    GateRun (*check)(void *ctx, AvpArena *a, const char *root, bool no_summary, const char *const *paths, size_t n);
    void *ctx;
} GateRulesTools;
GateRun gate_classes_scan_linked(void *ctx, AvpArena *a, const char *root);
GateRun gate_check_rules_linked(void *ctx, AvpArena *a, const char *root, bool no_summary, const char *const *paths,
                                size_t n);
/* The tools as command lines through a runner ({"classes", "scan"}, {"check_rules", "--json", ...}; cwd root). */
typedef struct GateRunnerTools {
    GateRunner runner;
    void *runner_ctx;
    const AvpEnv *env;
} GateRunnerTools;
GateRulesTools gate_rules_tools_via_runner(GateRunnerTools *rt);

/* ---- steps ------------------------------------------------------------------------------------------------------ */
typedef enum { GATE_NOT_RUN = 0, GATE_PASS, GATE_WARN, GATE_FAIL } GateStatus;
const char *gate_status_name(GateStatus s); /* "not-run", "pass", "warn", "fail" */
typedef struct GateStep {
    const char *name;
    GateStatus status;
    const char *reason;
    AvpStrVec lines;
    double seconds;
    AvpJson *data;            /* an object, insertion order as the Python dict */
} GateStep;
void gate_step_init(AvpArena *a, GateStep *st, const char *name);
GateStep *gate_step_fail(GateStep *st, const char *reason);
GateStep *gate_step_ok(GateStep *st, const char *reason);
GateStep *gate_step_warn(GateStep *st, const char *reason);
void gate_step_line(AvpArena *a, GateStep *st, const char *line);
void gate_step_lines_indented(AvpArena *a, GateStep *st, const AvpStrVec *lines); /* "    " + each */
AvpJson *gate_step_dict(AvpArena *a, const GateStep *st);                          /* Step.to_dict */

/* verdict(steps, dirty, merge) -> ok; *bad the failing step (a "step missing" step when one is absent) or NULL. */
bool gate_verdict(AvpArena *a, GateStep *steps, size_t n, bool dirty, bool merge, GateStep **bad);
/* final_line -> ok; *line the last line. pending: the verified pending flip (NULL: none). */
bool gate_final_line(AvpArena *a, GateStep *steps, size_t n, bool dirty, bool merge, const char *dol_sha1,
                     i64 units, i64 functions, double seconds, const char *pending, const char **line);

/* ---- the unit table the gate reads (units.load_units, the fields it uses) --------------------------------------- */
typedef struct GateUnit {
    const char *name, *path, *status, *source, *base_obj; /* source / base_obj NULL when absent */
    bool has_source;
} GateUnit;
typedef AVP_VEC(GateUnit) GateUnits;
void gate_load_units_real(void *ctx, AvpArena *a, const char *root, const char *version, GateUnits *out);
typedef void (*GateLoadUnits)(void *ctx, AvpArena *a, const char *root, const char *version, GateUnits *out);

/* ---- the indexer of the regression classification (see gate_regress.h) ----------------------------------------- */
#include "gate_regress.h"

/* ---- the gate --------------------------------------------------------------------------------------------------- */
typedef struct Gate Gate;
typedef void (*GateStepFn)(Gate *g, GateStep *st);
/* check_tools(): the problems doctor's check_required() reports ("<status> <name>: <detail>"). */
typedef void (*GateCheckTools)(Gate *g, AvpStrVec *problems);

struct Gate {
    AvpArena *a;
    const char *root;         /* absolute */
    GateOpts opts;
    GateRunner runner;
    void *runner_ctx;
    AvpBuf *out;              /* NULL: stdout */
    const char *version, *gate_dir, *tree;
    AvpEnv *env;              /* the build's environment (SHSM_OBJCACHE=0 with --no-objcache) */
    AvpStrVec dirty;          /* sorted */
    const char *pending;      /* --pending-flip or NULL */
    bool pending_ok;
    GateStep steps[GATE_NSTEPS];
    const char *dol_sha1;
    i64 units_linked, functions_linked;
    ShsmReport *report;
    const char *rules_json;   /* build/gate/rules.json once written, else NULL */
    AvpJson *snapshot;        /* after gate_execute */
    const char *exe_dir;      /* where configure.exe, gatecheck.exe and ledger.exe are */
    /* hooks (defaults set by gate_new) */
    GateStepFn step_fn[GATE_NSTEPS];
    GateCheckTools check_tools;
    GateLoadUnits load_units;
    void *load_units_ctx;
    GateIndexer indexer;
    GateRulesTools rules_tools;
};
/* Gate(root, args, runner, out). Raises what read_version raised. */
Gate *gate_new(AvpArena *a, const char *root, const GateOpts *opts, GateRunner runner, void *runner_ctx, AvpBuf *out);
void gate_say(Gate *g, const char *text);   /* print unless --json */
/* gate.run(argv): runner with cwd = the tree (cwd NULL) and the gate's environment. */
GateRun gate_run(Gate *g, const char *const *argv, size_t argc, const char *cwd, double timeout);
GateRun gate_git(Gate *g, const char *const *args, size_t n); /* git -C root ... */
#define GATE_ARGV_(...) (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *)
#define GATE_GIT(g, ...) gate_git((g), GATE_ARGV_(__VA_ARGS__))

/* The steps (the defaults of step_fn). */
void gate_step_preflight(Gate *g, GateStep *st);
void gate_step_build(Gate *g, GateStep *st);
void gate_step_checksum(Gate *g, GateStep *st);
void gate_step_coverage(Gate *g, GateStep *st);
void gate_step_regressions(Gate *g, GateStep *st);
void gate_step_layout(Gate *g, GateStep *st);
void gate_step_rules(Gate *g, GateStep *st);
void gate_step_ledger(Gate *g, GateStep *st);
/* Parts of the steps exposed for tests. */
const char *gate_check_pending(Gate *g, const GatePorcelainVec *entries); /* "" when the flip is verified */
void gate_check_tools_doctor(Gate *g, AvpStrVec *problems);               /* the default check_tools */
i64 gate_materialise(Gate *g);
const char *gate_baseline_rev(Gate *g);

/* execute() -> ok; *line the final line. Writes build/gate/last.json (and last-ok.json). */
bool gate_execute(Gate *g, const char **line);

/* ---- --quick ---------------------------------------------------------------------------------------------------- */
/* parse_ninja_deps: {object: [deps]} (AvpMap of AvpStrVec *), paths root-relative with `/`. */
AvpMap *gate_parse_ninja_deps(AvpArena *a, AvpStr text, const char *root);
/* reachable_units -> AvpMap unit name -> reason (const char *), in units order. */
AvpMap *gate_reachable_units(AvpArena *a, const GateUnits *units, const AvpStrVec *files, AvpMap *deps,
                             const char *root);
typedef struct GateQuick {
    GateRunner runner;
    void *runner_ctx;
    AvpBuf *out;              /* NULL: stdout */
    GateLoadUnits load_units;
    void *load_units_ctx;
    GateIndexer indexer;
    GateRulesTools rules_tools; /* NULL functions: the linked tools */
    const char *exe_dir;        /* configure.exe; NULL: avp_exe_dir() */
    /* classify_falls stand-in (tests); NULL: gate_classify_falls */
    void (*classify)(void *ctx, AvpArena *a, const GateFalls *fallen, GateFalls *edited, GateFalls *unchanged);
    void *classify_ctx;
} GateQuick;
bool gate_run_quick(AvpArena *a, const GateOpts *opts, const char *root, GateQuick *q, const char **line);

/* ---- --selftest ------------------------------------------------------------------------------------------------- */
typedef struct GateSelftest {
    GateRunner runner;
    void *runner_ctx;
    AvpBuf *out;
    const char *exe_dir;      /* gatecheck.exe */
} GateSelftest;
bool gate_run_selftest(AvpArena *a, const GateOpts *opts, const char *root, GateSelftest *s, const char **line);

/* ---- main ------------------------------------------------------------------------------------------------------- */
/* Hooks a front end may install before gate_main (the differential stub's runner and check_tools). */
typedef struct GateMainHooks {
    GateRunner runner;
    void *runner_ctx;
    GateCheckTools check_tools; /* NULL: doctor */
    const char *exe_dir;        /* NULL: avp_exe_dir() */
    void (*setup)(Gate *g, void *ctx); /* called on the Gate before it runs (tests script its steps) */
    void *setup_ctx;
    bool tools_via_runner;      /* the rules step's checker and scanner as command lines through runner (the stub) */
} GateMainHooks;
int gate_main(int argc, char **argv, const GateMainHooks *hooks);
