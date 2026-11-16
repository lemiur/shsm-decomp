/* brief: tools/brief.py in C, the starting briefing for one function or one unit.
 *
 * Files: brief_util.c (the small formatters, caps, the page budget, rendering, work order, the compile profile
 * line), brief_world.c (the World: lazily loaded project data), brief_fn.c (the function sections),
 * brief_unit.c (the unit sections), brief_run.c (brief_function, brief_unit, run, the parser).
 *
 * Section data is kept as JSON (the Python's dicts, keys in the Python's insertion order, tuples as arrays), so
 * that --json prints it as json.dumps did and the formatters read what the Python's formatters read. Every
 * allocation comes from the thread's long-lived arena. A level is an int: 0 the default, higher smaller, and
 * BRIEF_FULL (-1) for the Python's None (no caps). */
#pragma once
#include "avp/avp.h"
#include "query.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_libraries.h"
#include "shsm_repo.h"
#include "shsm_report.h"
#include "shsm_symbols.h"
#include "shsm_units.h"

#define BRIEF_FULL (-1)
#define BRIEF_DEFAULT_LINES 60
#define BRIEF_LISTING_LINES 40
#define BRIEF_TWIN_MIN_SIZE 16
#define BRIEF_PLAN_WINDOW 64
#define BRIEF_NEAR_CLASS_CODE 0x2000

typedef AvpStrVec BriefLines;

/* ---- small helpers (brief_util.c) ---- */
void brief_add(BriefLines *L, AvpStr s);
void brief_addz(BriefLines *L, const char *s);
void brief_addf_(BriefLines *L, const char *fmt, ...);  /* avp_pyfmt rules; use the macro */
#define brief_addf(L, fmt, ...) brief_addf_((L), (fmt) AVP__WIDEN(__VA_ARGS__))
AvpStr brief_clip(AvpStr text, i64 n);                  /* _clip */
const char *brief_clipz(const char *text, i64 n);
const char *brief_pct(const AvpJson *p);                /* _pct: "-" for None */
const char *brief_hex(i64 v);                           /* _hex: "0x%08X" */
/* str() of a JSON value as Python prints it (None, True, ints, float repr, the string itself). */
AvpStr brief_py_str(const AvpJson *v);
const char *brief_py_strz(const AvpJson *v);
bool brief_truthy(const AvpJson *v);                    /* Python truth of a JSON value (NULL: None) */
AvpStr brief_jstr(const AvpJson *obj, const char *key); /* a str member (p NULL when absent, None or not a str) */
/* lvl(level, caps): the cap of a level (-1: None) from a tuple of caps. */
i64 brief_lvl(int level, const i64 *caps, size_t n);
/* _cap: the shown count (all when n < 0) and *hidden. */
size_t brief_cap(size_t len, i64 n, size_t *hidden);
void brief_more(BriefLines *L, size_t k, const char *what);
AvpJson *brief_jz(const char *s);                       /* a JSON str, or null for NULL */
AvpJson *brief_ji(i64 v);                               /* a JSON int, or null for SHSM_NONE */
AvpJson *brief_jb(bool b);
void brief_set(AvpJson *obj, const char *key, AvpJson *v);
AvpJson *brief_arr_of(size_t n, ...);                   /* an array of n JSON values */

/* ---- sections ---- */
typedef struct BriefSection BriefSection;
typedef BriefLines (*BriefFmt)(const BriefSection *s, int level);
struct BriefSection {
    const char *name;
    AvpJson *data;          /* NULL only for an unavailable section */
    BriefFmt fmt;           /* NULL: no lines */
    int levels;
    const char *unavailable; /* NULL: available */
    double seconds;
    i64 first;              /* the listing's first cap (--listing-lines) */
};
BriefLines brief_section_lines(const BriefSection *s, int level);
AvpJson *brief_section_json(const BriefSection *s, int level);

typedef struct {
    BriefSection *v[8];
    size_t n;
} BriefSections;
/* layout(secs, budget, full): one level per section. */
void brief_layout(const BriefSections *secs, i64 budget, bool full, int *levels);
BriefLines brief_render(const BriefSections *secs, const int *levels);
/* parse_sections(text, allowed): NULL when text is empty; raises SHSM_EXC_USAGE for an unknown name. */
AvpStrVec *brief_parse_sections(const char *text, const char *const *allowed, size_t nallowed);
bool brief_wants(const AvpStrVec *want, const char *name); /* want NULL: every section */
const char *brief_title(const char *name);

/* ---- work order and the compile profile ---- */
/* work_order(items): items are unit-function dicts (name, address, size, twin, callees); a new array. */
AvpJson *brief_work_order(const AvpJson *items);
AvpJson *brief_base_words(const AvpStrVec *cflags);    /* _base_words: a JSON array of str */
const char *brief_profile_line(const AvpJson *p, const char *lib);

/* ---- the world (brief_world.c) ---- */
typedef struct BriefWorld {
    AvpArena *a;
    const ShsmRepo *repo;
    const char *root;
    QueryCtx *ctx;
    const ShsmUnits *units_;
    ShsmSplits *splits_;
    bool report_loaded;
    ShsmReport *report_;
    AvpIMap *report_by_addr_; /* address -> ShsmFunctionReport * */
    bool libs_loaded;
    const ShsmLibraries *libs_;
    AvpMap *state_cache;      /* unit name -> AvpJson * (NULL: None) */
    AvpMap *inputs_cache;     /* "<unit>|<ptr>" -> bool */
    bool classes_loaded;
    AvpJson *classes_;
    int classes_exc;
    const char *classes_msg;
    bool walls_loaded;
    AvpJson *walls_;
    double walls_mtime;
    bool eq_loaded;
    AvpJson *eq_;
    double eq_mtime;
    void *twins_db;           /* sqlite3 * */
    bool headers_loaded;
    AvpStrVec header_paths, header_texts;
    AvpMap *header_of_cache;
    AvpMap *source_cache;     /* source path -> AvpStrVec * of lines (NULL: unreadable) */
} BriefWorld;

BriefWorld *brief_world_new(const char *root);
void brief_world_close(BriefWorld *W);
ShsmAsmIndex *brief_ix(BriefWorld *W);
const ShsmSplits *brief_splits(BriefWorld *W);
const ShsmReport *brief_report(BriefWorld *W);
const ShsmFunctionReport *brief_report_at(BriefWorld *W, i64 address);
const char *brief_library(BriefWorld *W, i64 address);          /* NULL: None */
const char *brief_reference(const char *lib, const char **section); /* ("?", None) when lib is None */
const ShsmUnit *brief_unit(BriefWorld *W, const char *unit_name);
const ShsmUnit *brief_source_unit(BriefWorld *W, const char *unit_name);
bool brief_matched(BriefWorld *W, i64 address, const char *unit_name);
const AvpJson *brief_match_state(BriefWorld *W, const char *unit_name);
bool brief_inputs_current(BriefWorld *W, const char *unit_name, const AvpJson *inputs, double recorded_time);
/* score(s): (percent as a JSON value or NULL for None, origin or NULL, exact). */
typedef struct {
    AvpJson *percent;
    const char *origin;
    bool exact;
} BriefScore;
BriefScore brief_score(BriefWorld *W, const ShsmSymRow *s);
const AvpJson *brief_classes(BriefWorld *W);  /* raises FileNotFoundError as the Python */
const AvpJson *brief_wall_entry(BriefWorld *W, const char *unit_name, AvpStr name);
const AvpJson *brief_eqcheck_entry(BriefWorld *W, const char *unit_name, AvpStr name);
void *brief_twins_db(BriefWorld *W);
const char *brief_header_of(BriefWorld *W, const char *name);      /* NULL: None */
i64 brief_source_line(BriefWorld *W, const char *source, i64 address, const char *name); /* SHSM_NONE: None */
const char *brief_unit_name_of(const ShsmSymRow *s);
/* The twins rows of one fingerprint (exact/ref/shape) by address order. */
typedef struct {
    i64 address;
    const char *name, *unit; /* unit NULL: None */
    i64 size;
} BriefTwinRow;
typedef AVP_VEC(BriefTwinRow) BriefTwinRows;
/* The (exact, ref, shape) fingerprints of an address; false when the table has no row. */
bool brief_twins_fps(BriefWorld *W, i64 address, const char *fp[3]);
BriefTwinRows brief_twins_rows(BriefWorld *W, int level, const char *fp, bool ordered, i64 not_address);

/* ---- options ---- */
typedef struct {
    const char *target, *unit, *section;
    i64 lines, listing_lines;
    bool full, asm_, no_listing, no_compile, timings, json;
    const char *root;
    AvpStrVec *sections;
} BriefOpts;

/* ---- the briefings (brief_fn.c, brief_unit.c) ---- */
typedef struct {
    const char *keys[12];
    double vals[12];
    size_t n;
} BriefTimings;
void brief_timing(BriefTimings *t, const char *key, double v);
/* build_section(name, gather, fmt, levels): NULL when gather gives None. */
typedef AvpJson *(*BriefGather)(BriefWorld *W, void *ctx);
BriefSection *brief_build_section(BriefWorld *W, const char *name, BriefGather gather, void *ctx, BriefFmt fmt,
                                  int levels);
const char *brief_function(BriefWorld *W, const BriefOpts *o, BriefSections *out, BriefTimings *t);
const char *brief_unit_briefing(BriefWorld *W, const BriefOpts *o, BriefSections *out, BriefTimings *t);

/* exposed for tests */
BriefLines brief_fmt_identity(const BriefSection *s, int level);
BriefLines brief_fmt_status(const BriefSection *s, int level);
BriefLines brief_fmt_twins(const BriefSection *s, int level);
BriefLines brief_fmt_context(const BriefSection *s, int level);
BriefLines brief_fmt_shape(const BriefSection *s, int level);
BriefLines brief_fmt_listing(const BriefSection *s, int level);
BriefLines brief_fmt_start(const BriefSection *s, int level);
BriefLines brief_fmt_unit_header(const BriefSection *s, int level);
BriefLines brief_fmt_unit_functions(const BriefSection *s, int level);
BriefLines brief_fmt_unit_classes(const BriefSection *s, int level);
BriefLines brief_fmt_unit_includes(const BriefSection *s, int level);
BriefLines brief_fmt_unit_notes(const BriefSection *s, int level);
BriefLines brief_fmt_list(const BriefSection *s, int level);

int brief_main(int argc, char **argv);
/* The page `brief <target> --lines N [--unit]` prints, in process, as lines appended to `out` (the arena's strings);
 * the exit code the command would give (0, else 1 for a finding and 2 for any other error, whose message is
 * dropped as a caller capturing only stdout would). For draft's packet. */
int brief_page(const char *root, const char *target, bool unit, i64 lines, AvpStrVec *out);

/* ---- the sibling counterpart (brief_sibling.c): data from build/index/sibling when it exists, else null ---- */
/* {name, demangled, tier, lib, object, order} of the retail function at address, or null. A function with no pair whose
 * real name exists in the sibling gives {same_named: true, name, demangled, lib, object, order, size, retail_size}. */
AvpJson *brief_sibling_fn(BriefWorld *W, i64 address, const char *name, i64 size);
/* [{lib, object, start, end, paired, clean, unused, relation, proposed, prop_start, prop_end}] of the sibling object
 * spans overlapping [start, end), or null when there is no sibling database or nothing overlaps. relation: "equals"
 * (the paired span is the range), "equals-proposed" (the range is the span's proposed unit range), "inside",
 * "covers", "partly". */
AvpJson *brief_sibling_range(BriefWorld *W, i64 start, i64 end);
void brief_sibling_fn_lines(BriefLines *L, const AvpJson *d);    /* one line, nothing for null */
void brief_sibling_range_lines(BriefLines *L, const AvpJson *d); /* a heading and up to three spans */
