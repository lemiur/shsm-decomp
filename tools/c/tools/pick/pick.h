/* pick: a port of python tools/pick.py (ranked work lists: lost matches, nearly complete units, near-exact
 * functions, unstarted functions, MATCH-PENDING functions, mechanical drafts from matched twins).
 *
 * The library tool_pick also carries what tools/status.py imports from pick: the world model (pick_world_new, one
 * PickFn per report function), lib_matches, reference_flag, pct and pending_records. Work items are JSON objects
 * with the Python's dict keys in the Python's order (they are what --json prints). */
#pragma once
#include "avp/avp.h"
#include "shsm_cli.h"
#include "shsm_libraries.h"
#include "shsm_report.h"
#include "shsm_repo.h"
#include "shsm_units.h"

#define PICK_SECTION_LIMIT 5
#define PICK_SWEEP_COMMAND "build/tools/draft.exe sweep"

/* ---- libraries and filters ---- */
/* lib_matches(lib, query, unit_library): query (NULL or "": everything) names lib (NULL: "unknown") in full, its
 * group or any path component (case-insensitive), or equals the unit's configure.py library (NULL: none). */
bool pick_lib_matches(AvpArena *a, const char *lib, const char *query, const char *unit_library);
/* reference_flag(lib): the flag ("yes", "no", "partial", "inflate", "?") and the docs/references.md section (NULL:
 * None), looked up by the full library name (NULL: "unknown"), then by its group. */
const char *pick_reference_flag(const char *lib, const char **section);
/* REFERENCES: the table, for tests (n entries; library, flag, section or NULL). */
typedef struct {
    const char *library, *flag, *section;
} PickReference;
const PickReference *pick_references(size_t *n);
/* unit_matches(path, query): query NULL or "": true; else by prefix or substring. */
bool pick_unit_matches(AvpArena *a, const char *path, const char *query);
/* pct(p): "-" when has is false (None), "100%" at or above 100, else "%.1f%%" of min(round(p, 1), 99.9). */
const char *pick_pct(AvpArena *a, bool has, double p);

/* ---- the world model ---- */
typedef struct {
    const char *name;
    const char *unit;         /* unit path ("game/PadRumble") */
    const char *status;       /* Asm / Matching / NonMatching / Equivalent / ... */
    bool linked;              /* the report's metadata.complete */
    bool has_address;         /* false: None */
    i64 address;
    i64 size;
    double fuzzy;
    bool scored;              /* false: the report gives no percentage */
    const char *library;      /* attribution (libraries.tsv, else the unit's library, else "unknown") */
    const char *unit_library; /* configure.py library of the unit, or NULL */
    const char *demangled;    /* or NULL */
} PickFn;
typedef AVP_VEC(PickFn *) PickFnVec;

typedef struct {
    const ShsmRepo *repo;
    ShsmReport *report;
    const char *report_path;    /* report.path: the --report argument as given, else build/<ver>/report.json */
    const ShsmUnits *unit_table; /* NULL when it could not be loaded (Python's {}) */
    const ShsmLibraries *libraries;
    PickFnVec fns;              /* sorted by (address or 0, name) */
    AvpMap *by_unit;            /* unit path -> PickFnVec * (report order of units and functions) */
    AvpMap *unit_status;        /* unit path -> const char * */
    AvpMap *unit_library;       /* unit path -> const char * (NULL: None) */
    AvpIMap *by_addr;           /* address -> PickFn * (the last of an address wins), built on demand */
} PickWorld;

/* World(root, report_path): root NULL is the cwd's checkout; report_path NULL is build/<ver>/report.json. Raises
 * as the Python did (no report, a bad libraries.tsv as SHSM_EXC_USAGE). */
PickWorld *pick_world_new(AvpArena *a, const char *root, const char *report_path);
const PickFnVec *pick_world_unit_fns(const PickWorld *w, const char *path); /* NULL when the unit is unknown */
const char *pick_world_unit_status(const PickWorld *w, const char *path);  /* NULL when unknown */
const char *pick_world_unit_library(const PickWorld *w, const char *path); /* NULL: None or unknown */
PickFn *pick_world_by_address(PickWorld *w, i64 address);                   /* NULL when absent */
bool pick_world_matched(const PickFn *f);                                   /* linked and fuzzy >= 100 */
const char *pick_world_unit_lib(const PickWorld *w, const char *path);      /* World.unit_lib */
/* report.stale_note(repo, report_path): the note or NULL. */
const char *pick_stale_note(AvpArena *a, const ShsmRepo *repo, const char *report_path, bool can_pass_report);

/* ---- options ---- */
typedef struct {
    const char *mode;           /* NULL: the summary */
    const char *lib, *unit;     /* NULL: no filter */
    bool has_min_size, has_max_size;
    i64 min_size, max_size;
    double threshold;           /* default 90 */
    bool small, leaf, checker, all;
    i64 tiny;                   /* default 16 */
    i64 limit;                  /* --limit (0: no cap) */
} PickArgs;
void pick_args_default(PickArgs *o);
bool pick_keep(AvpArena *a, const PickFn *f, const PickArgs *o);

/* ---- modes: lists of JSON objects (the Python's item dicts) ---- */
typedef AVP_VEC(AvpJson *) PickItems;
/* pending_records(world, checker): {unit, symbol, rules, note, source} from docs/exceptions.md's MATCH-PENDING
 * tables and, with checker, the rule checker's MATCH-PENDING functions. */
PickItems pick_pending_records(AvpArena *a, PickWorld *w, bool checker);
PickItems pick_mode_lost(AvpArena *a, PickWorld *w, const PickArgs *o, const char **note);
void pick_mode_close(AvpArena *a, PickWorld *w, const PickArgs *o, PickItems *work, PickItems *candidates);
PickItems pick_mode_near(AvpArena *a, PickWorld *w, const PickArgs *o);
/* mode_fresh(world, args, limit): raises SHSM_EXC_FILE_NOT_FOUND when the asm index cannot be opened. limit 0:
 * args.limit. */
PickItems pick_mode_fresh(AvpArena *a, PickWorld *w, const PickArgs *o, i64 limit);
PickItems pick_mode_pending(AvpArena *a, PickWorld *w, const PickArgs *o);
PickItems pick_mode_drafts(AvpArena *a, PickWorld *w, const PickArgs *o, const char **note);
/* sweep_state(repo): the results array or NULL, and the note (NULL: None). */
AvpJson *pick_sweep_state(AvpArena *a, const ShsmRepo *repo, const char **note);

/* the text line of one item per mode */
AvpStr pick_fmt_lost(AvpArena *a, const AvpJson *it);
AvpStr pick_fmt_close(AvpArena *a, const AvpJson *it);
AvpStr pick_fmt_candidate(AvpArena *a, const AvpJson *it);
AvpStr pick_fmt_near(AvpArena *a, const AvpJson *it);
AvpStr pick_fmt_fresh(AvpArena *a, const AvpJson *it);
AvpStr pick_fmt_pending(AvpArena *a, const AvpJson *it);
AvpStr pick_fmt_drafts(AvpArena *a, const AvpJson *it);

/* ---- score history ---- */
/* parse_lost_text: rows of `score_history status --lost` text output. */
PickItems pick_parse_lost_text(AvpArena *a, AvpStr text);
/* The runner of score_history's CLI (argv without the program name): exit code, stdout and stderr text. Tests
 * replace it with pick_set_history_runner (NULL restores the linked score_history_run). */
typedef int (*PickHistoryRunner)(void *ctx, AvpArena *a, int argc, const char *const *argv, AvpBuf *out,
                                 AvpBuf *err);
void pick_set_history_runner(PickHistoryRunner fn, void *ctx);
/* history_rows(world): the rows, or NULL with *note. */
PickItems *pick_history_rows(AvpArena *a, PickWorld *w, const char **note);

/* ---- front end ---- */
int pick_main(int argc, char **argv);
