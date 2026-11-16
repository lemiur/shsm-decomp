/* match: the one-command matching inner loop, a port of tools/match.py (its docstring and --help are the spec).
 * Compile a unit privately, compare every function with retail, print one line per function and a summary.
 *
 * A unit result is the Python's result dict, kept as an AvpJson object with the same keys in the same order, so the
 * --json document and the state files in build/tmp/match/state/ are json.dumps' bytes. Exceptions are raised as the
 * Python raised them (shsm_raise with the SHSM_EXC_* class); shsm_cli_main turns an uncaught one into the error line.
 *
 * Files: match_state.c (paths, state files, state inputs, dependency files, JSON helpers), match_tools.c (compiling
 * with the cache, the listing cache, objdiff, the rule checker, gatecheck layout), match_analyse.c (one unit,
 * summaries, changes), match_render.c (text lines, symbol resolution), match_cli.c (the command line). */
#pragma once
#include "avp/avp.h"
#include "cc_compare.h"
#include "shsm_cli.h"
#include "shsm_deps.h"
#include "shsm_repo.h"
#include "shsm_units.h"

#define MATCH_EXACT "EXACT"
#define MATCH_DIFF "DIFF"
#define MATCH_MISSING "MISSING"
#define MATCH_EXTRA "EXTRA"
#define MATCH_PENDING "MATCH-PENDING"
#define MATCH_CACHE_VERSION "match-v1"
#define MATCH_ERROR_LINES 12
#define MATCH_STATE_INPUTS_VERSION 1
#define MATCH_R13_UNCHECKED "R13 not checked: run build/tools/classes.exe scan"

/* ---- JSON helpers ---------------------------------------------------------------------------------------------- */
AvpJson *match_jstr(AvpArena *a, const char *s); /* a string, or null for NULL */
AvpJson *match_jf(AvpArena *a, double v);
AvpJson *match_ji(AvpArena *a, i64 v);
AvpJson *match_json_copy(AvpArena *a, const AvpJson *v); /* a deep copy into a */
bool match_json_eq(const AvpJson *x, const AvpJson *y);  /* Python ==: dicts unordered, 1 == 1.0, NULL is None */
const char *match_jget_cz(AvpArena *a, const AvpJson *obj, const char *key); /* a str member, NULL for absent/null */

/* ---- paths and state ------------------------------------------------------------------------------------------- */
const char *match_dir(AvpArena *a, const ShsmRepo *r);
const char *match_state_path(AvpArena *a, const ShsmRepo *r, const char *unit);
/* load_state: the saved document, NULL when missing or unreadable (OSError, ValueError). */
AvpJson *match_load_state(AvpArena *a, const ShsmRepo *r, const char *unit);
const char *match_rel_or_abs(AvpArena *a, const ShsmRepo *r, const char *path);
const char *match_flags_key(AvpArena *a, const ShsmUnit *u);

/* [(absolute path, sha256)] of a dependency list. */
typedef struct {
    AvpStr *paths, *digests;
    size_t n;
} MatchDeps;
/* state_inputs(repo, u, source, deps): null when deps is NULL or a file cannot be read. */
AvpJson *match_state_inputs(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source,
                            const MatchDeps *deps);
/* stale_asm_line: a warning line when symbols.txt or splits.txt is newer than the unit's split asm, else NULL. */
const char *match_stale_asm_line(AvpArena *a, const ShsmRepo *r, const char *unit);
/* save_state(repo, unit, result) */
void match_save_state(AvpArena *a, const ShsmRepo *r, const char *unit, const AvpJson *result);

/* parse_depfile(text, cwd): absolute normalised prerequisites, the target dropped, duplicates removed. */
AvpStrVec match_parse_depfile(AvpArena *a, AvpStr text, const char *cwd);
/* depfile_digests(repo, source, out_dir): NULL when the dependency file or a file it names cannot be read. */
MatchDeps *match_depfile_digests(AvpArena *a, const ShsmRepo *r, const char *source, const char *out_dir);

/* ---- tools ----------------------------------------------------------------------------------------------------- */
typedef struct {
    bool ok;
    const char *obj;
    AvpStr output;
    double seconds;
    bool cached;
    AvpStrVec argv;
    MatchDeps *deps; /* NULL: unknown */
} MatchCompiled;
MatchCompiled match_compile_unit(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source,
                                 const char *workdir, const char *const *extra, size_t nextra, bool use_cache);
/* assemble_unit: an assembly source unit (.s) built by ninja's `as` rule; the object is the unit's base object. */
MatchCompiled match_assemble_unit(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u);
/* _first_error_lines(output, n) */
AvpStrVec match_first_error_lines(AvpArena *a, AvpStr output, i64 n);
/* load_listing(repo, obj, use_cache, out_dir): {name: CcFn*}; raises as compare.load_compiled. */
AvpMap *match_load_listing(AvpArena *a, const ShsmRepo *r, const char *obj, bool use_cache, const char *out_dir);
/* objdiff_percents: {compiled function name: match_percent (val.f)}, NULL when objdiff cannot score. */
AvpMap *match_objdiff_percents(const ShsmRepo *r, const char *target_obj, const char *base_obj, const char *workdir);
/* run_rules(repo, unit, file): {ok, pending, findings, error[, r13_note]}. */
AvpJson *match_run_rules(AvpArena *a, const ShsmRepo *r, const char *unit, const char *file);
AvpStr match_rules_verdict(AvpArena *a, const AvpJson *rr);
/* run_layout(repo, unit, obj): {status, lines}. */
AvpJson *match_run_layout(AvpArena *a, const ShsmRepo *r, const char *unit, const char *obj);
/* A CcError raised as the Python exception it stands for. */
AVP_NORETURN void match_raise_cc(const CcError *err);

/* ---- one unit -------------------------------------------------------------------------------------------------- */
typedef struct {
    const char *file;          /* --file, NULL */
    bool built, use_cache, data, rules, keep;
    const AvpStr *symbols;     /* restriction, n == 0: none */
    size_t nsymbols;
} MatchAnalyseOpts;
/* analyse_unit: the result dict; *cmp (may be NULL) receives {compiled name: CcCompareResult*}. */
AvpJson *match_analyse_unit(AvpArena *a, const ShsmRepo *r, const char *unit, const MatchAnalyseOpts *o, AvpMap **cmp);

enum { MATCH_S_EXACT, MATCH_S_DIFF, MATCH_S_MISSING, MATCH_S_EXTRA, MATCH_S_PENDING, MATCH_S_COUNT };
int match_status_index(AvpStr status); /* -1 for an unknown status */
typedef struct {
    i64 counts[MATCH_S_COUNT];
    i64 exact, total;
    const AvpJson *worst;
    bool matched, layout_bad;
} MatchSummary;
MatchSummary match_summarise(const AvpJson *result);
int match_exit_code(AvpJson *const *results, size_t n);

typedef struct {
    const char *kind;
    AvpStr name;
    const AvpJson *old, *new_; /* NULL: None */
} MatchChange;
/* changes(old, new_records, restricted) */
size_t match_changes(AvpArena *a, const AvpJson *old, const AvpJson *records, bool restricted, MatchChange **out);

/* ---- text ------------------------------------------------------------------------------------------------------ */
AvpStr match_pct(AvpArena *a, const AvpJson *rec);
AvpStr match_function_line(AvpArena *a, const AvpJson *rec);
AvpStr match_summary_line(AvpArena *a, const AvpJson *result);
AvpStrVec match_unit_lines(AvpArena *a, const AvpJson *result, bool show_all, i64 limit);
AvpStrVec match_diff_lines(AvpArena *a, const CcCompareResult *cres, const AvpJson *rec, i64 context, bool all_rows,
                           i64 limit, bool show_all);
/* resolve_symbol(query, names): names sorted and distinct; raises SHSM_EXC_USAGE when none or several match. */
AvpStr match_resolve_symbol(AvpArena *a, AvpStr query, const AvpStr *names, size_t n);

/* ---- command line ---------------------------------------------------------------------------------------------- */
int match_main(int argc, char **argv);
