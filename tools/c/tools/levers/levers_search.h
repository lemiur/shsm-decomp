/* levers: scoring, the search, ranking, diffs and the report (levers.py "scoring and search" and "output"), and the
 * command line (levers_cli.c). */
#pragma once
#include "levers.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_units.h"

#define LV_MAX_DEPTH 4

/* ---- scores ---- */
typedef struct {
    const char *name;
    i64 rows, tm;
    bool exact;
} LvOther;
/* Score(ok, error, rows, tm, exact, sig, others, nrows); error / sig NULL are None. */
typedef struct LvScore {
    bool ok;
    const char *error; /* utf-8 */
    i64 rows, tm, nrows;
    bool exact;
    const char *sig;
    LvOther *others;
    size_t nothers;
} LvScore;
/* A scorer: compile `text` as the unit's source and score it. Called from pool threads; allocate from `a` (the
 * calling thread's long-lived arena). May throw (the search re-raises the first throw in candidate order). */
typedef LvScore *(*LvScoreFn)(void *ctx, AvpArena *a, LvS text);

/* status of a score against the base: "error", "improved", "breaks", "neutral", "worse"; *broken the other functions
 * that got worse (sorted) */
const char *lv_classify(AvpArena *a, const LvScore *base, const LvScore *sc, AvpCzVec *broken);

/* ---- the search ---- */
typedef struct {
    const char *family, *lever, *desc; /* desc: utf-8 */
    i64 line;
} LvStep;
typedef struct {
    LvS text;
    const LvStep *path;
    size_t npath;
} LvState;
typedef struct {
    LvState st;
    LvScore *sc;
    const char *status;
    AvpCzVec broken;
} LvResult;
typedef AVP_VEC(LvResult) LvResults;

typedef struct {
    i64 depth, frontier, jobs;
    double deadline; /* 0: t0 + 3600 */
    bool stop_on_exact;
    const char *const *families; /* NULL: all */
    size_t nfam;
    const char *wall; /* NULL: unknown */
    LvScore *base;    /* NULL: score text0 */
} LvSearchOpts;

typedef struct {
    LvScore *base;
    LvResults results;
    AvpMap *stats; /* Counter, insertion order (val.i) */
    double seconds;
    i64 levels;
} LvSearch;

const char *const *lv_family_order(const char *wall);
LvSearch lv_search(AvpArena *a, LvS text0, const char *symbol, LvScoreFn fn, void *ctx, const LvSearchOpts *o);
/* ranked(results): improving states (distinct by compiled code, best first) and the ones that break another. */
void lv_ranked(AvpArena *a, const LvResults *res, LvResults *imp, LvResults *brk);

/* ---- the scorer of the unit ---- */
typedef struct {
    const ShsmRepo *repo;
    const ShsmUnit *unit;
    const char *symbol, *basename, *workroot;
    const char *const *extra;
    size_t nextra;
    AvpAtomic64 counter;
} LvScorer;
LvScore *lv_scorer_score(void *ctx, AvpArena *a, LvS text);

/* ---- output ---- */
char *lv_utf8(AvpArena *a, LvS s);       /* latin-1 text as utf-8 */
const char *lv_sha(AvpArena *a, LvS text); /* _sha: sha1 of the text's utf-8 encoding */
AvpStrVec lv_splitlines_keep(LvS s);     /* str.splitlines(keepends=True) of a latin-1 text */
LvS lv_make_diff(AvpArena *a, LvS old, LvS nw, const char *relpath);
/* apply_patch: false with *err (ValueError's message) when a hunk does not match */
bool lv_apply_patch(AvpArena *a, LvS text, LvS diff, LvS *out, const char **err);
const char *lv_safe_name(AvpArena *a, const char *name);
const char *lv_steps_text(AvpArena *a, const LvStep *path, size_t n);
const char *lv_stats_line(AvpArena *a, const AvpMap *stats);
AvpJson *lv_steps_json(AvpArena *a, const LvStep *path, size_t n);
/* write_outputs: the report document (also written to <odir>/report.json) */
AvpJson *lv_write_outputs(AvpArena *a, const char *odir, LvS text0, const char *relpath, const char *srcpath,
                          const char *symbol, const char *unit_name, const LvSearch *res);

/* ---- the command ---- */
int lv_main(int argc, char **argv);
