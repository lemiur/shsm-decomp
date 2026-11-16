/* twins (tools/twins.py): code the binary contains more than once. Three fingerprints per retail function (exact,
 * ref, shape) in build/index/twins.sqlite, the same schema, stamp and rows as the Python's, and the model the
 * subcommands query. Other tools link this header (tool_twins) for twins.open_db and the Twins model.
 *
 * Strings are UTF-8 and live in the arena given (or the Twins' own arena). */
#pragma once
#include "avp/arena.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

#define TWINS_SCHEMA "2"
#define TWINS_DEFAULT_MIN_SIZE 16
enum { TWINS_EXACT = 0, TWINS_REF = 1, TWINS_SHAPE = 2, TWINS_NLEVELS = 3 };
extern const char *const TWINS_LEVELS[TWINS_NLEVELS]; /* "exact", "ref", "shape" */
int twins_level_index(const char *name);               /* -1 when unknown */

/* ---- fingerprints ---- */
/* normalise_row(row, fn_start, fn_name): the (exact, ref, shape) spellings of one row. fn_name may be NULL. */
void twins_normalise_row(AvpArena *a, const ShsmAsmRow *row, i64 fn_start, const char *fn_name, AvpStr out[3]);
/* fingerprint(block): the three digests (sha1 hex, first 20 digits), NUL-terminated. */
void twins_fingerprint(const ShsmAsmBlock *b, char out[3][21]);

/* ---- the database (open_db) ---- */
const char *twins_db_path(AvpArena *a, const ShsmRepo *r); /* <index_dir>/twins.sqlite */
/* _stamp(ix): sha1 hex of "<SCHEMA>|<repr of the files rows>". */
const char *twins_stamp(AvpArena *a, ShsmAsmIndex *ix);
/* open_db(repo, ix, rebuild, jobs): the connection to the fingerprint table, rebuilt when the stamp differs or
 * rebuild is set (jobs <= 0: CPU count). Close with sqlite3_close. Raises SHSM_EXC_SQLITE on sqlite errors. */
sqlite3 *twins_open_db(const ShsmRepo *r, ShsmAsmIndex *ix, bool rebuild, int jobs);

/* ---- the model ---- */
typedef struct TwinsFn {
    i64 address;
    const char *name, *unit; /* unit NULL: None */
    i64 size, nrows;
    const char *fp[TWINS_NLEVELS];
    bool matched;
    const char *status;      /* Matching / NonMatching / ... / Asm */
    const char *source;      /* NULL: None */
    const char *lib;
} TwinsFn;
typedef AVP_VEC(TwinsFn *) TwinsFnVec;

typedef struct {
    AvpArena *arena;
    const ShsmRepo *repo;     /* NULL in a hand-built model */
    ShsmAsmIndex *ix;         /* NULL in a hand-built model */
    TwinsFnVec fns;           /* address order */
    AvpIMap *by_addr;         /* address -> TwinsFn * */
    AvpMap *groups[TWINS_NLEVELS]; /* fingerprint -> TwinsFnVec * (first-seen order) */
} Twins;

/* Twins(root, rebuild, jobs): the fingerprint table (refreshed), the units, the report and the libraries. */
Twins *twins_load(const char *root, bool rebuild, int jobs);
void twins_close(Twins *tw);
/* A model built by hand (tests): twins_new, then twins_add in address order. */
Twins *twins_new(void);
TwinsFn *twins_add(Twins *tw, const TwinsFn *f);
/* resolve(text): the function named or at text; raises SHSM_EXC_USAGE as the Python. */
TwinsFn *twins_resolve(Twins *tw, const char *text);
/* twins(f, level): the other members of f's group. */
TwinsFnVec twins_of(AvpArena *a, const Twins *tw, const TwinsFn *f, int level);
/* operand_diffs(a, b, cap=6): the difference lines and *n, the number of differing rows. */
AvpStrVec twins_operand_diffs(AvpArena *a, Twins *tw, const TwinsFn *x, const TwinsFn *y, size_t cap, i64 *n);
bool twins_lib_matches(AvpArena *a, const TwinsFn *f, const char *lib);
AvpJson *twins_fn_json(AvpArena *a, const TwinsFn *f); /* Fn.to_dict() */

typedef struct {
    i64 functions, bytes, with_twin, with_twin_bytes, groups, unmatched_with_matched_twin,
        unmatched_with_matched_twin_bytes, unmatched_in_families, unmatched_in_families_bytes, matched_with_twin;
} TwinsStats;
void twins_compute_stats(const Twins *tw, i64 min_size, TwinsStats out[TWINS_NLEVELS]);
AvpJson *twins_stats_json(AvpArena *a, const TwinsStats *s);

/* ---- subcommands: lines and --json data into an open cli.Output ---- */
typedef struct {
    const char *function;  /* of */
    i64 min_size;
    const char *lib;       /* NULL: None */
    const char *level;
    i64 min_count;
    bool all;
    i64 limit;             /* 0: no cap */
} TwinsArgs;
/* Where a subcommand writes: an open cli.Output, or (out NULL, tests) the lines and the data object collected. */
typedef struct {
    ShsmOutput *out;
    AvpStrVec lines;
    AvpJson *data;
} TwinsOut;
int twins_cmd_of(const TwinsArgs *args, Twins *tw, TwinsOut *out);
int twins_cmd_unmatched(const TwinsArgs *args, Twins *tw, TwinsOut *out);
int twins_cmd_families(const TwinsArgs *args, Twins *tw, TwinsOut *out);
int twins_cmd_stats(const TwinsArgs *args, Twins *tw, TwinsOut *out);

int twins_main(int argc, char **argv);
