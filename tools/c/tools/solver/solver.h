/* solver: the decision solver. For every stop (a non-exact function of a NonMatching unit) it states which compiler
 * decision diverged, the condition retail's decision implies (a sentence built only from evidence the tool read),
 * and the forms and probes it compiled to test that condition. It never writes src/ or include/ (except --apply, which
 * writes one unit source), never edits configure.py, config.yml, symbols.txt or splits.txt, and never commits: all its
 * compiles run on scratch copies under build/tmp/solver/.
 *
 * Phases:
 *   0  the clean stop list (solver_stops.c): classes, the parked-exact `flag` records, the controls;
 *   1  inline / call-set decisions (solver_inline.c): where retail defines each differing callee, the condition, the
 *      witness and sibling evidence, the probe-flag probes, the model check;
 *   2  register / schedule (solver_reg.c, solver_dwarf.c, solver_cand.c, solver_probe.c): DWARF comparison,
 *      candidates, probes;
 *   3  control flow (solver_layout.c): wall's control finding as the condition, branch-structure candidates.
 *
 * Every phase states its condition also in the compiler's own numbers where a trace applies (solver_trace.c, through
 * mwtrace's library): the inliner's decision at the differing call site, the register allocator's nodes of the first
 * differing variable, the scheduler's selection of the first instruction pair of a schedule stop (solver_sched.c);
 * --trace picks the kinds, --no-trace leaves them out.
 *
 * Files: solver_rulings.c (config/R5WEA4/solver.tsv), solver_case.c (--case files), solver_score.c (scratch copies,
 * compiles, the scorer, the controls), solver_source.c (definitions and the callee-mention closure),
 * solver_record.c (records, verdicts, causes, timing, the summary tables), solver_apply.c (--apply and the saved
 * candidates), solver_cli.c (the command). */
#pragma once
#include "avp/avp.h"
#include "mwt_inline.h"
#include "mwt_regalloc.h"
#include "mwt_sched.h"
#include "cc_compare.h"
#include "cc_dwdump.h"
#include "levers.h"
#include "levers_search.h"
#include "shsm_cli.h"
#include "shsm_report.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"
#include "shsm_units.h"

/* ---- classes and verdicts -------------------------------------------------------------------------------------- */
typedef enum {
    SV_NOT_STARTED,
    SV_SOURCE_CALL,
    SV_INLINE,
    SV_FLAG,
    SV_REFERENT,
    SV_CONTROL_FLOW,
    SV_REGISTER,
    SV_SCHEDULE,   /* a register/schedule stop whose first difference is the same instructions in another order in one block */
    SV_OTHER,
    SV_NCLASS
} SvClass;
extern const char *const SV_CLASS_NAMES[SV_NCLASS]; /* "not-started", "source-call", ... */

typedef enum { SV_MATCH, SV_CLOSER, SV_NO_FORM, SV_CONDITION_ONLY, SV_MODEL_GAP, SV_SKIPPED, SV_NVERDICT } SvVerdict;
extern const char *const SV_VERDICT_NAMES[SV_NVERDICT]; /* "MATCH", "CLOSER", "NO-FORM-WITHIN-RULES", ... */

/* The kinds of compiler trace (--trace inline,regalloc,sched). */
typedef enum { SV_TRACE_INLINE, SV_TRACE_REGALLOC, SV_TRACE_SCHED, SV_NTRACE } SvTraceKind;
extern const char *const SV_TRACE_NAMES[SV_NTRACE]; /* "inline", "regalloc", "sched" */

/* ---- the rulings table (solver_rulings.c) ---------------------------------------------------------------------- */
typedef enum { SV_RULE_SURVIVING_SOURCE, SV_RULE_LEVER_FLAG, SV_RULE_PROBE_FLAG, SV_RULE_CONST_ACCESSOR } SvRuleKind;
typedef struct {
    SvRuleKind kind;
    const char *arg1, *arg2; /* arg2: const-accessor's const spelling, else NULL */
    const char *note;        /* "" when the row has none */
    size_t line;
} SvRuling;
typedef struct {
    SvRuling *v;
    size_t n, cap;
} SvRulings;
/* Parses the table text: `kind<TAB>args...<TAB>note`, `#` comment lines and blank lines skipped. false with *err
 * ("solver.tsv:<line>: ...") on an unknown kind or a missing argument. */
bool sv_rulings_parse(AvpArena *a, AvpStr text, SvRulings *out, const char **err);
/* config/<ver>/solver.tsv of the checkout; a missing file is an empty table. Raises SHSM_EXC_VALUE on a bad row. */
SvRulings sv_rulings_load(AvpArena *a, const ShsmRepo *r);
/* The surviving-source row whose prefix starts the unit's name or path, or NULL. */
const SvRuling *sv_rulings_surviving(const SvRulings *t, const char *unit_name, const char *unit_path);
bool sv_rulings_is_lever_flag(const SvRulings *t, const char *flag);

/* ---- case files (solver_case.c) -------------------------------------------------------------------------------- */
typedef struct {
    AvpStr find, replace;
    size_t line;
} SvEdit;
typedef struct {
    const char *key, *value; /* "class" / "verdict" / "verdict-not" / "callee", the expected value */
    size_t line;
} SvExpect;
typedef struct {
    const char *path; /* the case file */
    const char *unit, *function;
    AVP_VEC(SvExpect) expects;
    AVP_VEC(SvEdit) edits;
} SvCase;
/* Parses a case file:
 *     # comment
 *     unit main/fmod/fmod_memory
 *     function set__Q24FMOD7MemPoolFiii
 *     expect verdict MATCH            (also: expect class <c>, expect verdict-not <v>, expect callee <name>,
 *                                      expect winner <form> (the MATCH / CLOSER candidate's form), expect contains
 *                                      <text> / not-contains <text> (the readable blocks), expect candidates <n>)
 *     find <<EOF                      (a heredoc: the lines up to the terminator line, joined with "\n")
 *     ...
 *     EOF
 *     replace <<EOF
 *     ...
 *     EOF
 * `find` and `replace` alternate; one-line forms `find: text` / `replace: text` are accepted. false with *err. */
bool sv_case_parse(AvpArena *a, AvpStr text, const char *path, SvCase *out, const char **err);
/* Applies the edits to text in order. Each find text must occur exactly once (in a CRLF text a find written with
 * "\n" is matched as "\r\n", and its replacement written so too); false with *err naming the edit's line and the
 * count otherwise. */
bool sv_case_apply(AvpArena *a, const SvCase *c, AvpStr text, AvpStr *out, const char **err);

/* ---- scratch copies, compiles, the scorer (solver_score.c) ----------------------------------------------------- */
/* One unit's scratch area: build/tmp/solver/<unit path>/<tag or counter>/<the source's base name> (the base name is
 * kept: MWCC's __FILE__ is the compiled file's base name and FMOD embeds it). */
typedef struct {
    const ShsmRepo *r;
    const ShsmUnit *u;
    const char *basename; /* of the unit's source */
    const char *workdir;  /* build/tmp/solver/<unit path> */
    AvpAtomic64 counter;
} SvScorer;
void sv_scorer_init(AvpArena *a, SvScorer *s, const ShsmRepo *r, const ShsmUnit *u);

/* A flag change: words appended after the unit's cflags (extra), words removed (drop, ShsmCompileOpts.drop_flags:
 * every occurrence of that exact word), and groups removed (a group is a flag with its argument, e.g. the two words
 * "-inline" "noauto": its last consecutive occurrence in the command line is removed; drop_flags cannot remove one
 * word of a repeated option such as -inline). */
typedef struct {
    const char *const *extra;
    size_t nextra;
    const char *const *drop;
    size_t ndrop;
    const AvpStrVec *groups;
    size_t ngroups;
    bool preprocess; /* -EP: the preprocessed text on stdout instead of an object */
} SvFlags;

/* One compile and its comparison with retail. */
typedef struct {
    bool ok;
    const char *error; /* the compiler's first error, or why the comparison failed */
    const char *dir, *src, *obj;
    CcObjectComparison *oc; /* NULL when !ok or preprocess */
    AvpStr preprocessed;    /* preprocess only */
} SvBuild;
/* Writes text to <workdir>/<tag>/<basename> (tag NULL: c<counter>), compiles it with the unit's flags changed by
 * fl (NULL: unchanged), and compares the object with retail. Never throws: a failure is ok=false with error. */
SvBuild *sv_build(SvScorer *s, AvpArena *a, AvpStr text, const char *tag, const SvFlags *fl);
/* The same for a file that already exists (the unit's own source in place): the stop's reference compile. */
SvBuild *sv_build_file(SvScorer *s, AvpArena *a, const char *src, const char *tag, const SvFlags *fl);
/* The comparison result of one function (compiled or retail name), NULL when the object does not emit it. */
const CcCompareResult *sv_result(const SvBuild *b, const char *symbol);
/* Levers' Score of one function in a build (rows that differ, target mismatches, exact, the signature of the
 * compiled rows, and the other functions): what lv_classify compares. A function the object does not emit is
 * ok=false "function not in the compiled object". */
LvScore *sv_lvscore(AvpArena *a, const SvBuild *b, const char *symbol);
/* Phase seam for candidates: score a text variant of the unit against a baseline build. Fills rows before/after,
 * exact and broken (lv_classify's other functions that got worse) and returns lv_classify's status ("improved",
 * "breaks", "neutral", "worse", "error"). */
const char *sv_score_variant(SvScorer *s, AvpArena *a, const SvBuild *base, AvpStr text, const char *symbol,
                             const SvFlags *fl, i64 *rows_before, i64 *rows_after, bool *exact, AvpCzVec *broken);
/* True when both object files have identical bytes. */
bool sv_objects_equal(const char *x, const char *y);
/* The words of a flag string ("-inline noauto" -> "-inline", "noauto"), shlex-split. */
AvpStrVec sv_flag_words(AvpArena *a, const char *flag);

/* ---- controls (solver_score.c) --------------------------------------------------------------------------------- */
#define SV_CONTROL_FN "solver_control_fn"
/* The control function appended to a text (1: the pipeline control). */
AvpStr sv_with_control_fn(AvpArena *a, AvpStr text);
/* Whether a build's object holds the control function (any compiled name starting with solver_control_fn). */
bool sv_build_has_control_fn(const SvBuild *b);
/* Control 1: the text plus the control function, compiled: true when the object shows the function. *why: the
 * failure ("compile failed: ...", "solver_control_fn not in the object"). */
bool sv_control_pipeline(SvScorer *s, AvpArena *a, AvpStr text, const char **why);
/* Control 2: a function's differing row count and compiled-row signature in two builds (the stop's reference and
 * the unmodified scratch copy) agree. Both absent is agreement. */
bool sv_control_baseline(AvpArena *a, const SvBuild *ref, const SvBuild *base, const char *symbol, const char **why);
/* Control 3: a generated two-function source compiled with `without` and with `with` flags: 1 when the objects'
 * listings differ, 0 when they are the same, -1 when a compile failed (*why). */
int sv_control_flag(SvScorer *s, AvpArena *a, const SvFlags *without, const SvFlags *with, const char **why);

/* ---- source reading (solver_source.c) -------------------------------------------------------------------------- */
/* A source text parsed once (levers' tokenizer: not thread-safe, so call from one thread). */
typedef struct {
    AvpStr text;
    LvToks toks, code;
    i32 *m;
    LvFnDefs defs;
    bool ok;
    const char *error;
} SvSource;
SvSource *sv_source_parse(AvpArena *a, AvpStr text);
/* The definition of a symbol (mangled or plain) in the source: false when none (levers' definition finder). */
bool sv_source_find(AvpArena *a, const SvSource *s, const char *symbol, LvFnDef *out);
/* 1-based line of a token offset. */
i64 sv_source_line(const SvSource *s, i32 tok);
/* The name a call to a symbol is written with in source: "free" for free__Q24FMOD7MemPoolFPvPCciUi, the class name
 * for a constructor or destructor, "new"/"delete" for the operators, the symbol itself for a plain name. */
const char *sv_source_name(AvpArena *a, const char *symbol);
/* The identifiers of a function's body and of the bodies of every function of the same text it names, transitively
 * (name -> the helper whose body holds it first, "" for the function's own body). */
typedef struct {
    AvpMap *names; /* identifier -> val.p: const char * helper ("" own body) */
    bool ok;
} SvMentions;
SvMentions sv_source_mentions(AvpArena *a, const SvSource *s, const LvFnDef *fn);

/* ---- records (solver_record.c) --------------------------------------------------------------------------------- */
typedef struct {
    int number;           /* 1-based among the candidates of the function's records (sv_number_records); 0 unset */
    LvS text;             /* the whole unit text with the edit: what --apply writes */
    const char *form, *desc, *evidence;
    i64 line;
    bool compiled, exact;
    i64 rows_before, rows_after;
    AvpCzVec broken, rules;
} SvCandidate;
typedef struct {
    const char *lever;  /* "flag -inline noauto" */
    const char *result; /* a sentence: "target exact", "rows 12 -> 8", "no effect on the object", ... */
    bool closes, improves, no_effect, error;
    bool lever_ruled; /* the probed flag is a lever-flag row */
    int number;       /* numbered after the function's candidates (sv_number_records); a probe is never applied */
    AvpCzVec broken;  /* other functions the probe made worse ("closes A, opens B") */
} SvProbe;

enum { SV_DIR_NONE, SV_DIR_OURS_EXPANDED, SV_DIR_RETAIL_EXPANDED };

typedef struct SvRecord {
    const char *function, *unit, *callee; /* callee: the decision's callee (inline, source-call, flag: the flag) */
    bool has_percent;
    double percent;
    i64 rows_retail, rows_compiled, rows_differing;
    SvClass cls;
    const char *wall;      /* wall's class, NULL when wall gave none */
    AvpCzVec wall_classes; /* wall's class list (the first is `wall`) */
    const char *wall_verdict;
    i64 site_addr, site_row; /* retail address and compiled row of the first differing decision; -1 unknown */
    const char *ours, *retail, *condition;
    AvpCzVec evidence;
    AVP_VEC(SvCandidate) candidates;
    AVP_VEC(SvProbe) probes;
    SvVerdict verdict;
    const char *cause;
    AvpCzVec notes;
    int direction; /* SV_DIR_* */
    /* verdict inputs */
    bool model_gap, control_failed, surviving_source, phase_built, rule_exception;
    /* timing and the run it came from */
    double seconds;                   /* this decision's share of the stop's time: classification, controls, candidates */
    bool ran_candidates, budget_hit;  /* candidates were compiled; a candidate or probe batch hit the stop's budget */
    const char *source, *source_sha1; /* the unit source (repo-relative) and its sha1 when the run read it; apply checks it */
    bool from_case;                   /* the run edited the scratch text (--case): never applied */
    /* the compiler traces (solver_trace.c): NULL / empty under --no-trace or when no trace applies */
    AvpJson *trace;                   /* records.json's "trace" object */
    const char *trace_condition;      /* the condition in the trace's numbers (the text and records.tsv) */
} SvRecord;
typedef AVP_VEC(SvRecord *) SvRecords;

SvRecord *sv_record_new(AvpArena *a, const char *unit, const char *function, SvClass cls);
void sv_note(AvpArena *a, SvRecord *rec, const char *fmt, ...);
void sv_evidence(AvpArena *a, SvRecord *rec, const char *fmt, ...);
/* The verdict rules of the brief, from the record's inputs, candidates and probes (sets rec->verdict). */
SvVerdict sv_assign_verdict(SvRecord *rec);

/* The cause table: records grouped by cause (count, functions in first-seen order), most frequent first. */
typedef struct {
    const char *cause;
    size_t count;
    AvpCzVec functions;
    size_t verdicts[SV_NVERDICT]; /* how many of the records reached each verdict */
} SvCause;
typedef AVP_VEC(SvCause) SvCauses;
SvCauses sv_group_causes(AvpArena *a, const SvRecords *recs);

AvpJson *sv_record_json(AvpArena *a, const SvRecord *rec);
AvpStr sv_records_tsv(AvpArena *a, const SvRecords *recs);
AvpStr sv_causes_tsv(AvpArena *a, const SvCauses *causes);
/* The readable block of one record (lines). */
void sv_record_lines(AvpArena *a, const SvRecord *rec, AvpStrVec *out);

/* ---- timing, the two summary tables, --apply (solver_record.c, solver_apply.c) ------------------------------------ */
/* Per-stop timing: a record that ran candidates is a solved stop; its seconds are the sample. */
typedef struct {
    size_t stops, solved, budget_hit;
    double median, max, total_solved;
} SvTiming;
SvTiming sv_timing(const SvRecords *recs);
/* A candidate that rests on a declaration order no sibling DWARF evidences (form "decl", or its text says so). */
bool sv_candidate_unevidenced(const SvCandidate *c);
/* Table 1 (one row per record: function, unit, class, verdict, rows, best candidate or probe in a few words) and
 * table 2 (cause, count, verdicts, functions), as aligned text lines. */
void sv_tables(AvpArena *a, const SvRecords *recs, const SvCauses *causes, AvpStrVec *out);
/* Numbers the candidates of every function's records 1.. (in record order) and its probes after them. */
void sv_number_records(SvRecords *recs);
/* Writes <outdir>/apply/<function>.json and one text file per candidate for every function that has candidates or
 * probes (the last run of that function; an older file of the function is replaced). false on an I/O failure. */
bool sv_apply_save(AvpArena *a, const char *outdir, const SvRecords *recs);
typedef struct {
    bool ok;
    const char *error;       /* why not (ok false) */
    const char *unit, *source, *form, *desc; /* source: repo-relative path written */
    i64 rows_before, rows_after;
    bool exact;
} SvApplied;
/* Applies candidate n of the function's last run (<outdir>/apply) to its source file under root. Refuses a probe, a
 * candidate that is not exact or closer, one with broken functions or rule findings, a run that edited the text
 * (--case), and a source file that no longer has the run's sha1. Writes that one file, nothing else. */
SvApplied sv_apply(AvpArena *a, const char *root, const char *outdir, const char *function, i64 n);

/* ---- Phase 0 (solver_stops.c) ---------------------------------------------------------------------------------- */
typedef AVP_VEC(AvpStrVec) SvGroups; /* flag groups: each the words of one option with its arguments */
/* One call difference wall found, judged against the source. */
typedef struct {
    const char *callee;
    const char *kind;  /* wall's finding kind (a "different" pair is split into its two sides) */
    bool retail_calls; /* retail calls the callee more often than ours (ours expanded it, or the source lacks it) */
    SvClass cls;       /* SV_SOURCE_CALL or SV_INLINE */
    const char *via;   /* where the source names the callee: "" the body, a helper name, NULL not named */
    AvpCzVec explains; /* calls this decision explains ("compiled calls D, retail does not" dropped for it) */
    int count;         /* how many of wall's findings name this callee on this side */
} SvDecision;
typedef AVP_VEC(SvDecision) SvDecisions;

/* Whether the source closure of the function names a callee: true with *via ("" own body, else the helper). */
typedef bool (*SvMentionFn)(void *ctx, const char *callee, const char **via);
/* Whether retail's copy of the intermediate function calls the callee. */
typedef bool (*SvRetailCallsFn)(void *ctx, const char *intermediate, const char *callee);

typedef struct {
    SvClass cls;      /* the function's class when it has no call decisions, else SV_INLINE / SV_SOURCE_CALL */
    const char *rule; /* the Phase 0 rule that decided it */
    SvDecisions decisions;
} SvStop;
/* Phase 0's rules on one wall document (wall_classify_result's dict, or the not-emitted record wall's scan writes:
 * name null, "missing": true). defined_in_source: whether the unit source defines the function (used for a
 * not-emitted one). mention / retail_calls may be NULL (nothing named; no intermediate evidence). Pure. */
SvStop sv_classify_stop(AvpArena *a, const AvpJson *d, bool defined_in_source, SvMentionFn mention, void *mctx,
                        SvRetailCallsFn retail_calls, void *rctx);

/* A unit's work (filled by the unit job, then the sequential phases). */
typedef struct SvUnitWork {
    const ShsmUnit *u;
    SvScorer sc;
    AvpStr text0;           /* the source the stops are read from (the case's edited text under --case) */
    const char *ref_source; /* the file compiled as the stop's reference */
    SvBuild *ref, *base;    /* the reference compile and the unmodified scratch copy */
    bool pipeline_ok;
    const char *pipeline_why;
    AvpStr preprocessed;    /* .p NULL when preprocessing failed */
    const char *pp_error;
    SvSource *src, *pp;     /* text0 and the preprocessed text parsed (sequential phase) */
    const char *error;      /* the unit could not be compiled: no records but one note */
    double seconds;         /* the unit's compile time */
    /* parked-exact detection */
    bool all_exact;
    SvGroups unit_only;     /* the unit's flag groups that no other unit of its library has */
    AvpPtrVec dropped;      /* SvBuild * per unit_only flag: compiled without it */
    /* Phase 2, filled on the first register stop of the unit */
    bool sym_tried;
    SvBuild *sym;           /* the -sym on compile of the scratch copy */
    void *symfns;           /* SvDwFns *: its dtk dwarf dump parsed, NULL when unavailable */
    const char *sym_error;
    void *rules_base;       /* the rule checker's pending counts on the unmodified scratch copy (solver_cand.c), NULL until needed */
    /* the compiler traces of this unit's text (solver_trace.c), cached per compile */
    AvpMap *inl_traces;     /* flag-change key -> val.p: SvInlTraced * */
    AvpMap *ra_traces;      /* function (and variant text) -> val.p: SvRaTraced * */
    MwtRaUnit *ra_unit;     /* every function of the unmodified text traced in one compile, NULL until needed */
    const char *ra_unit_error; /* why that compile could not be made */
    MwtSchedUnit *sched_unit;  /* the scheduler's trace of every function of the unmodified text, NULL until needed */
    const char *sched_unit_error;
    AvpMap *sched_traces;   /* function (and variant text) -> val.p: SvSchedTraced * (solver_sched.c) */
    int trace_counter;     /* the scratch folders build/tmp/solver/<unit>/trace/<n> */
} SvUnitWork;

typedef struct SvRun SvRun;
/* The parked-exact `flag` records of a unit (OSNet). */
void sv_phase0_flags(SvRun *run, SvUnitWork *w, SvRecords *out);
/* The stop records of a unit (Phase 0 classes; Phase 1 fills its inline records). */
void sv_phase0_stops(SvRun *run, SvUnitWork *w, SvRecords *out);
/* The unit-only flag groups of a unit against the other units of its library (pure on the cflags lists). */
SvGroups sv_unit_only_flags(AvpArena *a, const ShsmUnit *u, const ShsmUnit *const *others, size_t nothers);
/* cflags split into option groups: a word starting with '-' plus the words after it that do not. */
SvGroups sv_flag_groups(AvpArena *a, const AvpStrVec *cflags);
/* A group as one flag text: its words joined with ' ' (a word holding a space in double quotes). */
const char *sv_group_text(AvpArena *a, const AvpStrVec *g);

/* ---- Phase 1 (solver_inline.c) --------------------------------------------------------------------------------- */
/* Fills an inline record (condition, evidence, probes, model check). caller_fn: our compiled function (NULL when
 * not emitted). */
void sv_phase1_decision(SvRun *run, SvUnitWork *w, SvRecord *rec, const SvDecision *dec, const CcCompareResult *res);
/* The probe-flag rows compiled as probes on the unit's text (self: the target is not emitted in the baseline). */
void sv_probe_flags(SvRun *run, SvUnitWork *w, SvRecord *rec, bool self);

/* ---- Phase 2: DWARF readings (solver_dwarf.c, pure) ------------------------------------------------------------ */
/* A variable of a listing and a function of a dtk dwarf dump: the shared parser's types (lib/cc_dwdump.h). */
typedef CcDwVar SvVar;
typedef CcDwVars SvVars;
typedef CcDwFn SvDwFn;
typedef CcDwFns SvDwFns;
static inline SvDwFns sv_dtk_dump_parse(AvpArena *a, AvpStr text) { return cc_dtk_dump_parse(a, text); }
static inline const SvDwFn *sv_dtk_find(const SvDwFns *fns, i64 lo, const char *name) { return cc_dtk_find(fns, lo, name); }
static inline const SvVar *sv_vars_find(const SvVars *v, const char *name) { return cc_dw_vars_find(v, name); }

/* The sibling's DWARF of one function (`sibling.exe fn ... --json`). */
typedef struct {
    bool ok;
    const char *name, *tier;
    bool offsets_reliable;
    SvVars params, locals; /* locals flattened in listing order, depth and block set */
    AvpCzVec only_in_debug;
} SvSibFn;
/* Parses sibling.exe's JSON (the document or its "function" object); false for "{}" or no function. */
bool sv_sibling_parse(AvpArena *a, AvpStr json, SvSibFn *out);

/* A register location as a number: "r31" -> 31, "f30" -> 130; -1 for anything else. */
int sv_reg_num(const char *loc);
bool sv_reg_callee_saved(int reg);

/* The locals comparison of ours against the sibling's Final and Debug DWARF. */
typedef enum { SV_LC_MISSING, SV_LC_EXTRA, SV_LC_ORDER, SV_LC_SCOPE, SV_LC_TYPE, SV_LC_TYPEDEF } SvLcKind;
extern const char *const SV_LC_KEYS[6]; /* "dwarf-local-missing", "dwarf-local-extra", "dwarf-order", ..., "dwarf-typedef" */
typedef struct {
    SvLcKind kind;
    const char *name;  /* the local */
    const char *other; /* order: the local it should precede; type: the DWARF's type; else NULL */
    const char *text;  /* the sentence */
    const SvVar *dw;   /* the DWARF's entry (NULL for an extra local) */
} SvLcFinding;
typedef AVP_VEC(SvLcFinding) SvLcFindings;
/* ours: the source's locals (declaration order, depth, block). final / debug: the sibling's builds (debug may be
 * NULL: not read). our_types: our dtk listing (resolved types), may be NULL. Pure. */
SvLcFindings sv_locals_compare(AvpArena *a, const SvVars *ours, const SvVars *final_, const SvVars *debug,
                               const SvVars *our_types);
/* The union of the sibling's locals: Final's, then Debug's names Final lacks. */
SvVars sv_dwarf_union(AvpArena *a, const SvVars *final_, const SvVars *debug);
/* Whether two type spellings name the same type once struct/class/union/enum keywords and spaces are dropped. */
bool sv_types_same(const char *x, const char *y);

/* The measured rule (docs/compiler_regalloc.md): across a call the first-declared local gets r31, then r30, ...
 * Given the locals in declaration order and our registers (-1: not a register), returns 1 when the callee-saved
 * ones descend in declaration order (the rule predicts our own registers), 0 when not (*why names the first pair
 * against it), -1 when fewer than two locals hold callee-saved registers (no evidence). */
int sv_order_model(AvpArena *a, const char *const *names, const int *regs, size_t n, const char **why);
/* The declaration order the registers imply: the names with callee-saved registers sorted by register, highest
 * first, per register class. */
AvpCzVec sv_implied_order(AvpArena *a, const char *const *names, const int *regs, size_t n);

/* Why a candidate's function text is no candidate form: levers' fake forms (volatile, casts, asm, #pragma, ...) the
 * original function text has fewer of, plus a new `RULE-EXCEPTION` or `__typeof__`. Empty: a candidate form. */
AvpCzVec sv_candidate_rejects(AvpArena *a, LvS orig_fn, LvS cand_fn, const char *symbol);
/* The same with the original's fake forms counted once (NULL: counted here). */
AvpCzVec sv_candidate_rejects_with(AvpArena *a, const LvCounter *orig_forms, LvS orig_fn, LvS cand_fn,
                                   const char *symbol);

/* ---- Phase 2: register / schedule (solver_reg.c, solver_cand.c, solver_probe.c) -------------------------------- */
/* One edit tried: a candidate (rule-abiding form) or a probe (a lever compiled only to state the condition). */
typedef struct {
    const char *form;     /* candidates: "reuse-local", "dwarf-order", ...; probes: "decl-order", "const-read" */
    const char *desc, *evidence;
    const char *helper;   /* the same-file helper the edit is in, NULL for the target */
    i64 line;
    LvS text;             /* the whole unit text */
    bool probe;
    int depth;            /* search depth of a reuse-local state */
    const char *site;     /* const-read: the spelling X */
    /* results */
    bool compiled, skipped, exact, same_object;
    bool unchecked; /* improving or exact but past the rule-check limit of its batch: not listed */
    const char *error, *status;
    i64 rows_before, rows_after;
    LvScore *score;
    AvpCzVec broken, rules;
} SvTry;
typedef AVP_VEC(SvTry) SvTries;

/* The state of one register stop (or of a same-file helper inside it). */
typedef struct SvP2 {
    SvRun *run;
    SvUnitWork *w;
    SvRecord *rec;
    const CcCompareResult *res;
    const char *symbol;    /* the target (retail name) */
    const char *fn_symbol; /* the function whose text is edited: the target or the helper's name */
    const char *helper;    /* NULL for the target */
    LvCtx *x;              /* fn_symbol's context on w->text0, NULL when unparsable */
    LvS text;              /* w->text0 */
    SvVars ours;           /* our locals from the source */
    SvSibFn sib, dbg;      /* sibling Final / Debug (ok false when none) */
    bool sib_exact;        /* paired exactly with reliable offsets */
    SvVars dwarf;          /* union of Final and Debug locals */
    SvLcFindings findings;
    AvpCzVec protected_names; /* declarations under a RULE-EXCEPTION: R30 comment */
    AvpCzVec protected_text;  /* the comment-and-declaration texts a candidate must keep */
    bool surviving;
    double deadline;
    LvScore *base;            /* the target's score in the unmodified scratch copy */
    const MwtRaRun *ra;       /* the register allocator's trace of the target in the unmodified text, NULL: none */
    size_t decl_dropped;      /* unevidenced declaration swaps the same-sweep filter dropped here */
    size_t against_runs;      /* regalloc --against traces made for this stop */
    const MwtSchedRun *sched; /* the scheduler's trace of the target in the unmodified text (schedule stops), NULL */
    size_t sched_against_runs; /* sched --against traces made for this stop */
} SvP2;

/* Our locals in a function's text (declaration order, depth, block; static locals left out). */
SvVars sv_source_locals(AvpArena *a, LvCtx *x);
/* Phase 2 on one register stop (the record exists, wall's fields filled). d: wall's document. */
void sv_phase2_register(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpJson *d, const CcCompareResult *res);
/* The candidates of a stop (or helper): generated, filtered, compiled, the frontier searched to depth 2. */
void sv_p2_candidates(SvP2 *p);
/* The probes of a stop (or helper). */
void sv_p2_probes(SvP2 *p);
/* Declaration moves (each local's declaration before or after another's): probes against the DWARF (moves toward
 * the DWARF are left to the DWARF candidates; levers' decl family added as probes), or, as_candidates, the "decl"
 * candidates of a function the sibling lacks. */
void sv_p2_decl_moves(SvP2 *p, SvTries *out, AvpSet *seen, bool as_candidates);
/* Compiles the tries in parallel against the target (budget p->deadline) and classifies them (main thread). */
void sv_p2_score(SvP2 *p, SvTry *v, size_t n);
/* The scratch text with edits applied ({s, e, text}, sorted, non-overlapping); false on an overlap. */
bool sv_apply_edits(AvpArena *a, LvS text, LvEdit *edits, size_t n, LvS *out);
/* 1-based line of a byte offset. */
i64 sv_line_of(LvS text, i64 pos);
/* The generator state of one batch of candidates (solver_cand.c): the fake-form filter, the seen set, the budget. */
typedef struct {
    SvP2 *p;
    SvTries *out;
    AvpSet *seen;
    LvS orig_fn;
    size_t rejected, kept_r30;
    LvCounter *forms; /* the original function's fake forms, counted once */
    size_t over_budget;
} SvGen;
/* Adds a try; a candidate passes the filter (fake forms, RULE-EXCEPTION, the R30 declarations kept). false when it
 * was a repeat, rejected or over the budget. */
bool sv_gen_add_try(SvGen *g, const char *form, const char *desc, const char *evidence, i64 line, LvS text, bool probe,
                    int depth);
/* The scored tries of a batch into the record's candidates (kept: exact, or fewer differing rows). */
void sv_p2_record(SvP2 *p, SvTry *v, size_t n, size_t *counts, AvpMap *tally);
/* A context for fn_symbol on the stop's text; false when the text does not parse there. */
bool sv_p2_open_ctx(SvP2 *p, const char *fn_symbol);
/* The functions of the same text the target's body calls, that neither our object nor retail calls here. */
AvpCzVec sv_p2_expanded_helpers(SvP2 *p);

/* ---- Phase 3: control flow (solver_layout.c) ------------------------------------------------------------------- */
/* Phase 3 on one control-flow stop (the record exists, wall's fields filled): the condition from wall's control
 * finding, the levers `control` and `expr` families, the solver's else-after-return and merge-ifs forms. */
void sv_phase3_control(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpJson *d, const CcCompareResult *res);
/* The solver's own forms (else-after-return, merge-ifs) of one function context, added to g's tries (unit tests). */
void sv_p3_own_forms(SvGen *g, LvCtx *x);

/* Whether the compiler of the unit accepts `__typeof__` (cached per profile and language). */
int sv_typeof_accepted(SvRun *run, SvUnitWork *w, const char **why);
/* Compiles a generated source with the unit's flags (no comparison): 1 compiled, 0 rejected (*why the error). */
int sv_compile_snippet(SvScorer *s, AvpArena *a, const char *tag, const char *ext, AvpStr text, const char **why);

/* ---- the compiler traces (solver_trace.c) ---------------------------------------------------------------------- */
/* One call site's decision as the inline trace gives it (mwt_inline.h). Numbers the trace did not give are -1. */
typedef struct {
    const char *callee;       /* link name (else the source name) */
    const char *decision;     /* "expanded" or "called" */
    const char *rule;         /* the trace's rule: "level0", "level1-cost", "not-candidate", "always-inline", ... */
    const char *detail;       /* the trace's detail text, "" none */
    const char *reason;       /* expanded: "inline keyword" or "auto-inline"; called: the not-candidate key before the
                               * detail's ':' ("noauto", "auto-statements", "not-yet-defined", "no-body", ...), else
                               * the rule */
    i64 cost, limit;          /* the deciding rule's numbers */
    i64 statements, max_statements; /* the callee's statements as CheckInlineability counted them, inline_max_auto_size */
    int level;                /* the level it was expanded at (-1 called) */
    i64 line;                 /* the site's source line */
} SvInlSite;
typedef AVP_VEC(SvInlSite) SvInlSites;

/* A disagreement of the inline trace's self-check for one caller and callee (MwtGap), with retail's count. */
typedef struct {
    const char *stage; /* "after-inliner", "simple-optimizer", "trace-vs-ir", "trace-vs-object", "trace" */
    i64 trace_calls, ir_calls, object_calls, retail_calls;
    const char *why;
} SvInlGap;

/* "expands X (auto-inline candidate: 10 statements, limit 15; level 0 has no size limit)", "calls X (not a
 * candidate: 16 statements, limit 15)": the site's decision in the trace's numbers. Pure. */
const char *sv_inl_site_text(AvpArena *a, const SvInlSite *s);
/* The condition retail's opposite decision implies, as a plain fact in the site's numbers (retail_calls: retail calls
 * the callee where ours expands it; else retail expands it where ours calls it). A rule the trace gives no number for
 * is said so. Pure. */
const char *sv_inl_condition(AvpArena *a, const SvInlSite *s, bool retail_calls);
/* The deciding rule as a cause-table label: "auto-inline: under 16 statements", "inline keyword", "definition order",
 * "no auto-inline", ... Pure. */
const char *sv_inl_rule_label(AvpArena *a, const SvInlSite *s);
/* What a self-check gap means for an inline record: 1 the inliner's calls equal retail's and a later stage removes
 * them (after-inliner, simple-optimizer): not an inline decision; 0 the trace disagrees with the object (trace-vs-ir,
 * trace-vs-object, trace): a model gap; 2 a later stage removes calls and the inliner's count is not retail's. Pure. */
int sv_inl_gap_kind(const SvInlGap *g);
/* The gap's counts in words ("trace 15, post-inline IR 15, object 11, retail 15"). Pure. */
const char *sv_inl_gap_counts(AvpArena *a, const SvInlGap *g);

/* One register allocator node as the record states it (mwt_regalloc.h), in the class's register numbering. */
typedef struct {
    const char *name;      /* the variable; NULL for a temporary */
    char kind;             /* 'p' parameter, 'l' local, 'o' another object, 't' temporary */
    const char *def_op;    /* a temporary's first defining opcode, NULL unknown */
    i64 def_at;            /* that instruction's number, -1 unknown */
    const char *orig;      /* an inlined callee's object (named @NNN): its name in the callee, else NULL */
    const char *inl_from;  /* that callee */
    bool inl_param;        /* a parameter of the callee */
    int index, root;       /* node index; its coalescing root (index when not coalesced) */
    int sweep, push_pos, select_pos, degree, push_degree;
    const char *push_how;  /* "low", "optimistic", "after", "" not pushed */
    i64 cost;
    int reg, retail_reg;   /* ours, retail's (-1 unknown) */
} SvRaNode;
typedef struct {
    int cls;               /* MWT_RC_GPR or MWT_RC_FPR */
    int K;                 /* simplify's degree bound */
    SvRaNode var;          /* the first differing named variable */
    SvRaNode holder;       /* the node that holds retail's register of var in ours */
    bool have_holder;
} SvRaFacts;
/* "`a` (node 40, local): sweep 1, pushed 12th (low), selected 30th, degree 14, spill cost 22; r30 in ours, r31 in
 * retail". Pure. */
const char *sv_ra_node_text(AvpArena *a, int cls, const SvRaNode *n);
/* The condition from the allocator's model as facts: which of the two must be coloured first, and what in ours
 * colours the other first (one sweep: the higher index; across sweeps: the later sweep). *shape: "one sweep",
 * "across sweeps", "order holds" (ours already colours them in the needed order), "no holder". Pure. */
const char *sv_ra_condition(AvpArena *a, const SvRaFacts *f, const char **shape);
/* The first difference is between temporaries only: x holds ours' register, y retail's (in ours); either may be
 * NULL (no node of ours has that register). Pure. */
const char *sv_ra_temps_condition(AvpArena *a, int cls, int ours_reg, int retail_reg, const SvRaNode *x, size_t nx,
                                  const SvRaNode *y, size_t ny);
/* The pairs of names whose relative order differs between two declaration orders (names in both only), as
 * "a\x1fb" strings with a declared before b in `from`. Pure. */
AvpCzVec sv_inverted_pairs(AvpArena *a, const char *const *from, size_t nfrom, const char *const *to, size_t nto);
/* The same-sweep test of an unevidenced declaration swap: 1 when some inverted pair has both nodes pushed in the
 * same sweep of trace pass p (the swap can trade their registers by the model), 0 when every pair whose nodes the
 * trace has is in different sweeps, -1 when the trace has no node of any pair (the filter cannot tell: kept). */
int sv_swap_same_sweep(const MwtRaPass *p, const AvpCzVec *pairs);
/* The node facts of one trace node (n) of pass p (positions from its coalescing root). */
SvRaNode sv_ra_node_of(AvpArena *a, const MwtRaPass *p, const MwtRaNode *n);

/* ---- the scheduler's trace (solver_sched.c) ---- */
/* One row of a function as the order test reads it: codecmp's normalised text, whether it branches, whether a branch
 * of the function targets it. */
typedef struct {
    const char *norm;
    bool branch, target;
} SvORow;
/* The order test on the first differing row: whether retail and ours hold the same instructions in another order in
 * one block there (same opcodes and, with the register names masked, the same operands, no branch target inside and
 * no branch before the window's last row). */
typedef struct {
    bool reorder;          /* the first difference is an order difference */
    i64 row;               /* the first differing row (the same index on both sides), -1 none */
    i64 end;               /* reorder: the window's end (exclusive): rows [row, end) hold the same instructions */
    i64 ours_at;           /* reorder: ours' row of the instruction retail has at `row` */
    i64 retail_at;         /* reorder: retail's row of the instruction ours has at `row` */
    const char *why;       /* the reason in words (either way) */
} SvOrderDiff;
SvOrderDiff sv_order_diff(AvpArena *a, const SvORow *retail, size_t nr, const SvORow *ours, size_t no);
/* The rows of a compare result's two functions in SvORow form. */
void sv_order_rows(AvpArena *a, const CcFn *fn, SvORow **out, size_t *n);
/* A register name in a normalised row masked ("lwz r3, 0x4(r4)" -> "lwz R, 0x4(R)"). Pure. */
const char *sv_mask_regs(AvpArena *a, const char *norm);

/* The facts of one pair query in words, as a schedule record states them. retail_first: the label of the instruction
 * retail issues first (the pair's second); decided_pass: the pass the order comes from (2, or 1 where pass 2 kept
 * pass 1's order by program order). Pure. */
const char *sv_sched_condition(AvpArena *a, const char *retail_first, const char *ours_first, const MwtSchedPair *p,
                               int block_index, const MwtSchedPair *p1, int block1_index, bool p1_same);
/* The cause key of a schedule record: "schedule: <key>" or "schedule: held by <edge kind>". Pure. */
const char *sv_sched_cause(AvpArena *a, const MwtSchedPair *decided);
/* The kind of the edge a held pair names ("raw", "mem-store-load", "can-issue 0"), "" none. Pure. */
const char *sv_sched_held_kind(AvpArena *a, const char *held);
/* Phase 2 on a schedule stop: the scheduler's trace of the target, its self-check (a `trace` gap is a model gap; an
 * `after-scheduler` gap a fact), the pair of the two instructions and the condition. 1 stated, 0 no trace made (the
 * class rests on the rows), -1 the trace places the two instructions in no one scheduled block (not a schedule stop). */
int sv_trace_schedule(SvP2 *p, const SvOrderDiff *d, const CcCompareResult *res);
/* A temporaries-only register record: xs, the temporaries coloured ours' register at the first differing row; ys, those
 * coloured retail's. The scheduler's trace names the row's instruction (pass 2's node at the row, the same instruction
 * in pass 1, its regalloc number): x is the temporary of xs it defines, y the one of ys defined nearest it in the same
 * pass-1 block; the fact is pass 1's order of their definitions and the key. NULL when the trace is off. */
const char *sv_trace_temps_order(SvP2 *p, const SvRaNode *xs, size_t nx, const SvRaNode *ys, size_t ny);
/* The first row (positional) where two functions' normalised rows differ, -1 none. Pure. */
i64 sv_first_differing_row(const CcFn *retail, const CcFn *ours);
/* sched --against for one variant text of a schedule stop: the first selection that differs, NULL when no trace. */
const char *sv_trace_sched_against(SvP2 *p, LvS text, const char *what);

/* Phase hooks (no-ops under --no-trace). */
/* Phase 1: the inliner's decision at the record's differing site, the reclassification of a gap after the inliner. */
void sv_trace_inline_record(SvRun *run, SvUnitWork *w, SvRecord *rec, const CcCompareResult *res);
/* Phase 0's flag records: the dropped-flag compile's decisions for the sites whose decision the flag changes. */
void sv_trace_flag_record(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpStrVec *group);
/* A probe flag compiled for an inline record: the decision under the flag (appended to the probe's result). */
void sv_trace_probe_flag(SvRun *run, SvUnitWork *w, SvRecord *rec, SvProbe *pr, const AvpStrVec *words);
/* Phase 2: the target's regalloc trace (p->ra), the self-check (a gap is a model gap), the facts of the first
 * differing variable (var NULL: none named; ours_reg / retail_reg the first differing row's registers in the solver's
 * numbering) and the condition. false when no trace was made (the declaration-order check stays). */
bool sv_trace_register(SvP2 *p, const char *var, int ours_reg, int retail_reg);
/* Phase 2: regalloc --against for one variant text (a probe or the best candidate): the lines of the named variables
 * that moved, NULL when no trace was made (budget, failure). */
const char *sv_trace_against(SvP2 *p, LvS text, const char *what);
/* Phase 2's same-sweep filter of an unevidenced declaration swap (form "decl"): false when the trace shows the swap
 * cannot trade registers (it is dropped and counted). */
bool sv_trace_keep_decl(SvP2 *p, LvS text);

/* ---- the run (solver_cli.c) ------------------------------------------------------------------------------------ */
struct SvRun {
    AvpArena *a;
    const ShsmRepo *r;
    const ShsmSymbolTable *symtab;
    SvRulings rulings;
    bool diagnose_only;
    double deadline; /* 0: none */
    const char *function; /* --function / the case's function, NULL: all */
    i64 limit, stops;     /* --limit (0: none) and the stops taken so far */
    bool deadline_hit;
    const ShsmReport *report; /* the objdiff report for percentages, NULL when unreadable */
    void *sibling;        /* SibLookup *, NULL: no index */
    void *sibling_db;     /* Sibling *, NULL */
    AvpSet *sibling_unused; /* names of UNUSED members of the sibling map, NULL when unknown */
    bool witness_checked, witness_present;
    AvpMap *flag_controls; /* "<profile>\x1f<flag>" -> val.i: 1 differs, 0 same, -1 error */
    AvpStrVec run_notes;
    double stop_seconds;   /* Phase 2's per-stop budget for candidates and probes (--minutes) */
    i64 jobs;              /* parallel candidate compiles */
    AvpMap *sib_cache;     /* "<target>\x1f<build>\x1f<mode>" -> val.p: SvSibFn * (NULL: none) */
    AvpMap *typeof_ok;     /* "<profile>\x1f<ext>" -> val.i: 1 accepted, 0 rejected, -1 control failed */
    bool trace;            /* some compiler trace is on (default; --no-trace turns them off) */
    bool trace_kind[SV_NTRACE]; /* which kinds --trace names (default all) */
    double trace_seconds;  /* the time the traces took (the time: line) */
    double trace_kind_seconds[SV_NTRACE]; /* the same per kind */
    size_t ntraces, traces_failed, traces_skipped; /* compiles traced, traces that could not be made, skipped by a budget */
    size_t decl_dropped;   /* unevidenced declaration swaps the same-sweep filter dropped */
};
/* The flag control of one flag change for the unit's compiler profile, cached per (profile, flag). */
int sv_run_flag_control(SvRun *run, SvUnitWork *w, const char *flag, const SvFlags *without, const SvFlags *with,
                        const char **why);
int sv_main(int argc, char **argv);
