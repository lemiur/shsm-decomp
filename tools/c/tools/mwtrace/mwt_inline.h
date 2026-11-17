/* mwtrace inline: what the inliner decided at each call site of one compile, and by which numbers.
 *
 * The trace log (written by the spec MWT_INLINE_SPEC under the debugger) is parsed into the inliner's events
 * (mwt_inline_parse), the events are turned into call sites per caller with their decision, rule and numbers
 * (mwt_inline_analyse), and the result is checked against the object the same compile wrote (the self-check:
 * "called" sites must be the calls of the compiled function, expanded ones must be absent). docs/compiler_inline.md
 * describes the compiler code each event and rule comes from. */
#pragma once
#include "avp/avp.h"
#include "mwt_run.h"

extern const char *const MWT_INLINE_SPEC;

/* ---- the parsed log ---- */
typedef struct {
    u64 callee;
    AvpStr name;
    bool indirect;        /* through a function pointer or the vtable: no relocation */
} MwtBodyCall;
typedef AVP_VEC(MwtBodyCall) MwtBodyCalls;

typedef struct {
    u64 obj;
    size_t log_line;      /* the log line of its DEF */
    AvpStr name;          /* the object's name (the source name: "fn_801785B4", "__ct") */
    u32 qual;             /* Object qualifiers: 0x10 the inline keyword */
    bool autoinline;      /* auto-inline was on when it was defined */
    bool dont_inline;
    bool has_result;
    bool candidate;       /* CheckInlineability's answer */
    bool qual4;           /* refused for qualifier 0x4 */
    u32 tflags;           /* its type's flags after the check (0x800 auto-inline candidate, 0x20000 never_inline) */
    int auto_status;      /* 0 not tested, 1 ok, 2 too many statements, 3 locals too large */
    i64 auto_count, auto_value; /* statements counted; the limit (2) or the locals' bytes (1, 3) */
    int kind;             /* the classifier's answer, -1 unknown */
    AvpStr kind_reason;
    MwtBodyCalls body;    /* the calls of its body when it was defined (CheckInlineability's tree walk) */
    bool has_ir;
    MwtBodyCalls ir;      /* the calls of its IR when ComputeInlineability last measured it outside a visit */
} MwtDef;

typedef struct {
    int level;
    AvpStr rule;          /* the decision's return block ("pass", "not-candidate", "level1-cost", ...), "" none */
    i64 cost;             /* the callee's cost (its function data +8), -1 when not read */
    int kind;             /* the callee's inlinability kind at the check, -1 when not read */
    int xp;               /* after the expansion call: -1 not called, 0 not inlined in place, 1 inlined */
    i64 queued;           /* the statement-inline queue length when it was tested, -1 not tested */
    size_t seq;           /* its position among the visit's checks */
    i64 limit;            /* bottom-up mode: inline_max_size as halved for the caller, -1 otherwise */
    i64 size;             /* bottom-up mode: the callee's estimated expanded size, -1 otherwise */
    size_t log_line;      /* the line of the log it came from */
} MwtCheck;
typedef AVP_VEC(MwtCheck) MwtChecks;

typedef struct {
    u64 e;                /* the call node */
    u64 callee;
    AvpStr callee_name;
    u32 qual, tflags, frflags;
    int dt;               /* the callee object's datatype */
    size_t first_index;   /* order of its first check in the visit */
    int first_level;
    i64 offset;           /* source offset of the statement holding it, -1 unknown */
    u64 fp;               /* that statement's file */
    MwtChecks checks;
    bool expanded;
    int expanded_level;
    bool by_statements;   /* expanded through the statement-inline queue */
    /* filled by the analysis */
    i64 via;              /* the site (index in the visit) whose expansion brought this call in; -1 none */
    AVP_VEC(i64) via_alts; /* when the order does not tell which of several: their indices */
    int ordinal;
    i64 line;
    AvpStr file;
    AvpStr decision, rule, detail;
    i64 cost, limit;      /* -1 when the rule compares none */
    bool unchecked;       /* not a checked node: a call of a body expanded at the last pass, or of an unvisited caller */
    bool discarded;       /* inside an expansion the growth test undid */
    bool indirect;        /* a virtual call through the vtable (no relocation) */
} MwtSite;
typedef AVP_VEC(MwtSite) MwtSites;

typedef struct {
    u64 cur;
    int autoinline, maxauto, depth, bottomup, always, dont_inline, x112;
    bool per_function;    /* started by CInline_Expand (no -ipa file), else by the call-graph visitor */
    MwtSites sites;
    AvpIMap *by_e;        /* e -> site index + 1 (the live site of that node) */
    bool reverted, kept;
    i64 old_cost, new_cost;
    int max_level;
    bool has_ir;
    MwtBodyCalls ir;      /* the calls of its IR after the inlining (ComputeInlineability after the visit) */
} MwtVisit;
typedef AVP_VEC(MwtVisit) MwtVisits;

typedef struct {
    AvpStr build;
    AvpIMap *defs;        /* obj -> MwtDef* */
    AvpIMap *links;       /* obj -> AvpStr* link name */
    AvpIMap *files;       /* fp -> AvpStr* path */
    MwtVisits visits;
    AvpStrVec errors;     /* handler errors, compiler exceptions, a timeout */
    size_t nchecks;
} MwtTrace;

/* Parses a trace log. Unknown lines are ignored. */
void mwt_inline_parse(AvpArena *a, AvpStr log, MwtTrace *t);

/* ---- the analysis and the self-check ---- */
typedef struct {
    AvpStr caller;        /* link name */
    AvpStr callee;
    i64 trace_calls, ir_calls, object_calls; /* -1: not known (ir_calls: no post-inline IR snapshot) */
    AvpStr stage;         /* where the counts part: trace-vs-ir (the IR has a call no check saw), simple-optimizer
                           * (the trace leaves calls the post-inline IR lacks), after-inliner (the IR has them, the
                           * object not), trace-vs-object (no IR snapshot to tell), trace (the log is incomplete) */
    AvpStr why;           /* the disagreement in words */
} MwtGap;
typedef AVP_VEC(MwtGap) MwtGaps;

typedef struct {
    AvpStr caller;        /* link name (or the object's name when it has none) */
    bool visited;
    const MwtVisit *visit; /* NULL when not visited */
    MwtSites synth;       /* the sites of an unvisited caller (from the object) */
    bool in_object;
} MwtCaller;
typedef AVP_VEC(MwtCaller) MwtCallers;

typedef struct {
    MwtCallers callers;
    MwtGaps gaps;
    AvpStrVec untraced;   /* "caller -> callee xN": object calls of no trace site whose callee is no candidate */
    size_t nsites;
} MwtResult;

/* Builds the sites and their decisions, the lines (from the sources the log names), and compares with the object's
 * calls. objs may be NULL (no self-check). */
void mwt_inline_analyse(AvpArena *a, MwtTrace *t, const MwtObjFuncs *objs, const char *root, MwtResult *r);

/* The report: text (one line per site, grouped by caller) and JSON. only: one caller's link name, or NULL. */
AvpStr mwt_inline_text(AvpArena *a, const MwtJob *j, const MwtTrace *t, const MwtResult *r, const char *only,
                       double secs);
AvpJson *mwt_inline_json(AvpArena *a, const MwtJob *j, const MwtTrace *t, const MwtResult *r, const char *only,
                         double secs);

/* ---- one traced compile (the subcommand's work, reusable by other tools) ---- */
typedef struct {
    MwtJob job;
    MwtTrace trace;
    MwtObjFuncs objs;     /* the object's calls */
    MwtResult result;     /* the sites and the self-check (result.gaps empty: the trace equals the object) */
    double secs;          /* the compile under the debugger */
} MwtInlineRun;
/* Resolves the job (mwt_job_init: unit, src or NULL, flag words added and dropped, outdir), compiles under the
 * debugger with MWT_INLINE_SPEC, parses the log, reads the object's calls and analyses them. false with *why (a
 * compiler exit other than 0 is a failure naming the log). */
bool mwt_inline_run(AvpArena *a, const char *unit, const char *src, const AvpStrVec *adds, const AvpStrVec *drops,
                    const char *outdir, MwtInlineRun *out, const char **why);

/* The subcommand: mwtrace.exe inline <unit> [options]. */
int mwt_inline_main(AvpArena *a, int argc, char **argv);
