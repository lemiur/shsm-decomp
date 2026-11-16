/* merge_lane: the C port of tools/merge_lane.py (the mechanical part of the lane merge procedure).
 *
 *   ml_core.c     helpers, Ctx (git, the command dispatch with its log files), the driver (parse_args, run)
 *   ml_deps.c     the default commands: ninja (external), configure.exe, gatecheck.exe and ledger.exe (the C
 *                 ports beside this executable), the rule checker and the class scanner (linked)
 *   ml_steps.c    preflight, rebase, gate, layout, ledger, merge
 *   ml_changes.c  changes (gate_regress's ports of func_scores .. classify_decrease, score_history's index)
 *   ml_rules.c    rules (classes scan, check_rules --changed, whole-tree onto vs lane)
 *
 * Every external action goes through ml_run_cmd, keyed as DEFAULT_COMMANDS; MlHooks replaces any of them (and git,
 * the source indexer and a step's start) for the tests and the differential harness's stub executable. */
#pragma once
#include "avp/avp.h"
#include "gate.h"
#include "gate_regress.h"

#define ML_BASELINE "config/R5WEA4/baseline_report.json"
#define ML_SPLITS "config/R5WEA4/splits.txt"
#define ML_SYMBOLS "config/R5WEA4/symbols.txt"
#define ML_REPORT "build/R5WEA4/report.json"
#define ML_BASELINE_COPY "build/R5WEA4/baseline.json"
#define ML_CHANGES_JSON "build/R5WEA4/report_changes.json"
#define ML_CLASSES_JSON "build/R5WEA4/classes.json"
#define ML_PROGRESS "docs/progress.md"
#define ML_EPS 0.005
#define ML_SKIP_LIMIT 100

/* avp_throw code of a red step (StepFail); the message and summary are in Ml.fail_msg / fail_summary. */
enum { ML_STEP_FAIL = 0x4D4C };

typedef struct Ml Ml;

/* Extra format fields of one command call ({"root": ..., "classes": ...}); NULL when not given. */
typedef struct MlExtra {
    const char *root, *classes;
} MlExtra;

/* The result of `git` (rc, stdout, stderr), decoded utf-8 with errors="replace". */
typedef struct MlGit {
    i64 rc;
    AvpStr out, err;
} MlGit;

typedef struct MlHooks {
    void *ctx;
    /* commands[key]: true when the hook answered the call (rc, text), false to run the default. */
    bool (*command)(void *hctx, Ml *ml, const char *key, const MlExtra *extra, i64 *rc, AvpStr *text);
    /* Ctx.git: true when the hook answered (as a patched Ctx.git). */
    bool (*git)(void *hctx, Ml *ml, const char *const *args, size_t n, const char *cwd, MlGit *res);
    /* score_history.index_source: NULL uses the linked C index. Raises (avp_throw) as the Python raised. */
    AvpJson *(*index)(void *hctx, AvpArena *a, const char *path, AvpStr text);
    /* Called before a step function runs (a patched STEP_FUNCS entry). */
    void (*before_step)(void *hctx, Ml *ml, const char *step);
    /* Output sink: NULL writes to stdout (flushed per line). */
    AvpBuf *out;
} MlHooks;

typedef struct MlUnit {
    const char *unit, *status;
} MlUnit;
typedef AVP_VEC(MlUnit) MlUnits;

struct Ml {
    AvpArena *a;
    const char *slot, *branch, *onto, *repo, *log_dir;
    bool update, merge, reviewed, take_union;
    bool tools_changed;   /* the rebase of this run changed tools/c */
    AvpStrVec steps;      /* the requested core steps */
    AvpSet *passed;       /* steps that PASSed in this run */
    MlUnits lane_units;
    AvpStrVec lane_files;
    const char *tip;      /* NULL for None */
    const char *onto_rev; /* pinned by preflight */
    const MlHooks *hooks;
    GateIndexer ix;
    /* StepFail */
    const char *fail_msg, *fail_summary;
    void *full_log;       /* AvpFile of <log_dir>/merge.log: every line ml_out printed (NULL: none) */
};

/* ---- ml_core.c --------------------------------------------------------------------------------------------------- */
void ml_out(Ml *ml, const char *line);                       /* print(line) */
static inline const AvpStr *ml_sp(AvpArena *a, AvpStr s) { /* &s for a %S argument */
    AvpStr *p = AVP_NEW(a, AvpStr);
    *p = s;
    return p;
}
#define ML_FIND(h, n) avp_str_find((h), (n), 0, AVP_END)
#define ML_F(ml, ...) avp_cz((ml)->a, avp_pyfmt((ml)->a, __VA_ARGS__))
AVP_NORETURN void ml_fail(Ml *ml, const char *msg, const char *summary); /* raise StepFail(msg, summary) */
const char *ml_join_ev(Ml *ml, const AvpStrVec *ev);         /* "\n".join(ev) */
void ml_ev(Ml *ml, AvpStrVec *ev, const char *line);
void ml_ev_tail(Ml *ml, AvpStrVec *ev, AvpStr text, size_t n); /* ev.extend("    " + ln for ln in tail(text, n)) */
AvpStrVec ml_tail(AvpArena *a, AvpStr text, size_t n);
const char *ml_norm(AvpArena *a, const char *path);          /* normcase(realpath(path)) */
bool ml_norm_eq(AvpArena *a, const char *x, const char *y);
const char *ml_unit_of(AvpArena *a, AvpStr src_rel);         /* "main/" + splitext(src_rel)[0] */
AvpMap *ml_parse_objects(AvpArena *a, AvpStr text);          /* path -> status (val.p const char *), dict order */
const char *ml_classify_conflict(AvpStr path);               /* generated / ledger / exceptions / manual */
const char *ml_resolve_tool(AvpArena *a, const char *root, const char *name); /* NULL when absent */
const char *ml_short(AvpArena *a, const char *sha);          /* (sha or "")[:9] */
AvpStr ml_decode(AvpArena *a, AvpStr bytes);                 /* bytes.decode("utf-8", "replace") */
void ml_write_text(const char *path, AvpStr text);           /* open(path, "w", encoding="utf-8").write(text) */
bool ml_read_text(AvpArena *a, const char *path, AvpStr *text); /* open(path, encoding="utf-8", errors="replace") */

/* args NULL-terminated */
MlGit ml_git(Ml *ml, const char *cwd, const AvpEnv *env, const char *const *args);
#define ML_ARGV_(...) ((const char *const[]){__VA_ARGS__, NULL})
#define ML_GIT(ml, ...) ml_git((ml), NULL, NULL, ML_ARGV_(__VA_ARGS__))
#define ML_GIT_IN(ml, cwd, ...) ml_git((ml), (cwd), NULL, ML_ARGV_(__VA_ARGS__))
const char *ml_rev(Ml *ml, const char *rev, const char *cwd); /* NULL when it does not name a commit */
bool ml_show_file(Ml *ml, const char *rev, const char *path, AvpStr *bytes);
typedef struct MlWorktree {
    const char *path, *head, *branch; /* branch NULL for None */
} MlWorktree;
typedef AVP_VEC(MlWorktree) MlWorktrees;
MlWorktrees ml_worktrees(Ml *ml);
AvpStrVec ml_status_lines(Ml *ml, const char *cwd, bool untracked); /* raises StepFail on a git failure */
typedef struct MlUnmerged {
    AvpStr code, path;
} MlUnmerged;
typedef AVP_VEC(MlUnmerged) MlUnmergedVec;
MlUnmergedVec ml_unmerged(Ml *ml);
AvpStrVec ml_op_in_progress(Ml *ml, const char *cwd);       /* labels in order: rebase, merge, cherry-pick, revert */
const char *ml_onto_ref(Ml *ml);
const char *ml_log_path(Ml *ml, const char *name);
bool ml_tools_stale(Ml *ml, const char **why); /* build/tools older than the slot's tools/c (ml_steps.c) */
void ml_tools_guard(Ml *ml, AvpStrVec *ev);   /* rebuilds a stale build/tools and stops the step: run again */
/* Rebase conflicts in configure.py or splits.txt (ml_conflict.c): the hunks in compact form into report and, when every hunk
 * is a list of whole units, the file with the union of both sides in *resolved (the lane's version of a unit both name). */
bool ml_conflict_report(Ml *ml, const char *path, AvpStr text, AvpStrVec *report, AvpStr *resolved);

/* Ctx.run(key, log_name, extra, cwd): the command's exit code and text; the text also goes to the log file. */
i64 ml_run_cmd(Ml *ml, const char *key, const char *log_name, const MlExtra *extra, const char *cwd, AvpStr *text);

/* main(argv, commands): the whole tool; argv[0] is the program. Returns the exit code (argparse errors 2). */
int ml_main(int argc, char **argv, const MlHooks *hooks);

/* ---- ml_deps.c: the default commands ----------------------------------------------------------------------------- */
i64 ml_default_cmd(Ml *ml, const char *key, const char *log, const MlExtra *extra, const char *cwd, AvpStr *text);

/* ---- steps ------------------------------------------------------------------------------------------------------- */
typedef const char *(*MlStepFn)(Ml *ml, AvpStrVec *ev); /* the summary; red: ml_fail */
const char *ml_step_preflight(Ml *ml, AvpStrVec *ev);
const char *ml_step_rebase(Ml *ml, AvpStrVec *ev);
const char *ml_step_gate(Ml *ml, AvpStrVec *ev);
const char *ml_step_changes(Ml *ml, AvpStrVec *ev);
const char *ml_step_rules(Ml *ml, AvpStrVec *ev);
const char *ml_step_layout(Ml *ml, AvpStrVec *ev);
const char *ml_step_ledger(Ml *ml, AvpStrVec *ev);
const char *ml_step_merge(Ml *ml, AvpStrVec *ev);
void ml_require_no_unmerged(Ml *ml);

/* rules helpers (exposed for the tests) */
AvpJson *ml_parse_json_text(AvpArena *a, AvpStr text, const char **err); /* NULL with *err = str(ValueError) */
const char *ml_check_rules_doc(AvpArena *a, const AvpJson *doc, i64 rc, const char *what, const char *const *keys,
                               size_t nkeys);
/* The indexer the changes and rules steps use (hooks->index, else score_history's C index). */
AvpJson *ml_index_source(Ml *ml, AvpArena *a, const char *path, AvpStr text);
