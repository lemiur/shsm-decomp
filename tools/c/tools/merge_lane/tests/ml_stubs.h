/* ml_stubs: test-only stand-ins for merge_lane's external commands and the throwaway repository fixture
 * (tools/test_merge_lane.py's Stubs, LaneTest.setUp and helpers), shared by the CTest tests
 * harness's stub executable (merge_lane_stub.exe). Never linked into the production tool. git is real. */
#pragma once
#include "merge_lane.h"

/* ---- fixture ------------------------------------------------------------------------------------------------------ */
#define MLFX_A_ADDR 0x80000100
#define MLFX_B_ADDR 0x80000200
extern const char *const MLFX_A_SRC;
extern const char *const MLFX_CONFIGURE;

/* git(cwd, *args): raises AVP_ERR_PROC "git <args>: <stderr>" on a non-zero exit; returns stdout stripped. */
const char *mlfx_git(AvpArena *a, const char *cwd, const char *const *args); /* args NULL-terminated */
#define MLFX_GIT(a, cwd, ...) mlfx_git((a), (cwd), ML_ARGV_(__VA_ARGS__))
void mlfx_write(const char *root, const char *rel, AvpStr text); /* write(root, rel, text) with newline="\n" */
AvpStr mlfx_dumps(AvpArena *a, const AvpJson *v);                /* json.dumps(v) */
AvpJson *mlfx_report(AvpArena *a, const AvpJson *score_a, const AvpJson *score_b, const char *src); /* report() */
AvpJson *mlfx_report_f(AvpArena *a, double score_a, double score_b);
/* changes(rows): rows a JSON array of [name, addr, from|null, to|null] */
AvpJson *mlfx_changes(AvpArena *a, const AvpJson *rows);
AvpStr mlfx_changes_text(AvpArena *a, const AvpJson *doc, bool progress);
/* The process environment the fixtures and the tool share: fixed author and committer dates (both stubs set them, so
 * the two sides of the harness make the same commits). */
void mlfx_fixed_dates(void);

typedef struct MlFixture {
    AvpArena *a;
    const char *tmp, *main, *slot, *logs;
} MlFixture;
/* LaneTest.setUp in tmp (created). */
MlFixture mlfx_setup(AvpArena *a, const char *tmp);
void mlfx_lane_commit(MlFixture *fx, const char *rel, AvpStr text, const char *msg, const char *branch, bool new_branch);
void mlfx_main_commit(MlFixture *fx, const char *rel, AvpStr text, const char *msg);

/* ---- stubs -------------------------------------------------------------------------------------------------------- */
typedef struct MlMutation {
    const char *on;     /* a command key ("configure", "layout") or "step:<name>" (before that step function) */
    const char *rel;    /* main_commit(rel, text) on the first such call */
    AvpStr text;
    bool done;
    bool in_slot;       /* write rel into the slot tree, uncommitted (a file the build rewrites), not a main commit */
} MlMutation;

typedef struct MlStubs {
    AvpArena *a;
    MlFixture *fx;
    AvpStrVec calls;
    AvpJson *score_a, *score_b;    /* lane_scores fn_A, fn_B */
    AvpJson *changes_rows;         /* array */
    bool gate_ok;
    i64 gate_rc;
    AvpJson *rules_changed;        /* object */
    AvpJson *rules_tree;           /* {"onto": {...}, "lane": {...}} */
    i64 rules_tree_rc;
    AvpJson *changes_doc;          /* NULL: changes(changes_rows) */
    const char *changes_text;      /* NULL: changes_text(doc) */
    AvpJson *lane_report;          /* NULL: report(lane_scores); else the document ninja writes as report.json */
    bool changes_write;
    i64 layout_rc, ledger_rc, classes_rc;
    AvpStrVec tree_classes;        /* the classes path of each rules_tree call */
    AVP_VEC(bool) tree_classes_exist;
    const char *seen_onto;         /* ctx.onto_ref() at rules_changed */
    MlMutation *mutations;
    size_t nmutations;
    const char *remove_after_lane_tree; /* a slot path deleted after the lane's rules_tree call */
    bool fail_git_diff_head;       /* Ctx.git("diff", "--name-only", ..., "HEAD") -> 128, "", "fatal: simulated" */
    bool broken_index;             /* index_source raises ValueError("tokenizer failed") */
    bool unknown_keys;             /* every key unanswered: the default commands run (TestToolResolution) */
} MlStubs;

void mlst_init(MlStubs *s, AvpArena *a, MlFixture *fx); /* Stubs() defaults */
MlHooks mlst_hooks(MlStubs *s, AvpBuf *out);
bool mlst_called(const MlStubs *s, const char *key);
/* Applies a JSON stubs object ({"lane_scores": {...}, "changes_rows": [...], "gate_ok", ...}) over the defaults. */
void mlst_apply_json(MlStubs *s, const AvpJson *o);
/* run_tool(*extra, branch, onto): rc and the captured stdout. */
int mlfx_run(MlFixture *fx, MlStubs *s, const char *branch, const char *onto, const char *const *extra, size_t n,
             AvpStr *text);
