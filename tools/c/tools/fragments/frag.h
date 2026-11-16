/* frag: the internals of fragments (tools/fragments.py): row normalisation, the repeat engine (Corpus), rendering,
 * the fragment database and the model the subcommands query. Other tools use fragments.h, not this header.
 *
 * Strings are UTF-8. A Corpus owns its arena; everything a Frags model returns lives in its arena. */
#pragma once
#include "fragments.h"

#include "avp/arena.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_asm.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_libraries.h"
#include "shsm_repo.h"
#include "shsm_units.h"

#include <sqlite3.h>

#define FRAG_SCHEMA "4"
#define FRAG_DEFAULT_WINDOW 6
#define FRAG_DEFAULT_SITES 4
#define FRAG_RENDER_SAMPLE 200
#define FRAG_SEP (-1)
enum { FRAG_STRICT = 0, FRAG_LOOSE = 1, FRAG_NLEVELS = 2 };
extern const char *const FRAG_LEVELS[FRAG_NLEVELS];  /* "strict", "loose" */
extern const char FRAG_LEVEL_PREFIX[FRAG_NLEVELS];  /* 'S', 'L' */
int frag_level_index(const char *name);               /* -1 when unknown */

/* ---- row normalisation ---- */
typedef struct {
    AvpStr loose, strict, disp;
    i32 regs[8];
    int nregs;
} FragNorm;
/* frame_flags(rows): true for each prologue/epilogue row (and blr, .4byte). out has n entries. */
void frag_frame_flags(const ShsmAsmRow *rows, size_t n, bool *out);
/* norm_row(row, fn_start, fn_name): fn_name may be NULL. */
void frag_norm_row(AvpArena *a, const ShsmAsmRow *row, i64 fn_start, AvpStr fn_name, FragNorm *out);

/* ---- rendering ---- */
typedef struct {
    const i32 *regs;
    int nregs;
    AvpStr disp;
} FragRow;
typedef struct {
    AvpStr mn, ops;
} FragRenamed;
/* rename_rows(rows) */
FragRenamed *frag_rename_rows(AvpArena *a, const FragRow *rows, size_t n);
typedef struct {
    i32 row, atom;
    AvpStr label;
} FragCol;
typedef struct {
    AVP_VEC(AvpStr) lines;
    AVP_VEC(FragCol) cols;
    AvpStrVec *values; /* one per occurrence */
} FragRender;
/* render(occ_rows): nocc occurrences of len rows each (rows[o * len + j]). want_values false skips cols/values. */
FragRender frag_render(AvpArena *a, const FragRow *rows, size_t nocc, size_t len);
void frag_render_free(FragRender *r, size_t nocc);

/* ---- the repeat engine (Corpus) ---- */
typedef struct {
    i32 fi, p;
} FragPos;
typedef AVP_VEC(FragPos) FragOcc;
typedef struct {
    i32 length;
    FragOcc occ;
} FragRepeat;
typedef AVP_VEC(FragRepeat) FragRepeats;

typedef struct {
    size_t n;
    i32 *tok[FRAG_NLEVELS];
    i32 *segend;
    u32 *roff; /* n + 1 offsets into rpool */
    i32 *rpool;
    AvpStr *disp;
    /* info: binary (address, name, unit, first row address) or source (path, name, token base) */
    i64 address, first;
    const char *name, *unit, *path;
    const void *src; /* source: the body's first RlTok */
} FragFn;

typedef struct {
    bool sep;
    AvpStr tok[FRAG_NLEVELS];
    const i32 *regs;
    int nregs;
    AvpStr disp;
} FragRowIn;

typedef struct FragCorpus {
    int nlevels;
    AvpArena *arena;
    AvpMap *intern[FRAG_NLEVELS];
    AVP_VEC(AvpStr) rev[FRAG_NLEVELS];
    AVP_VEC(FragFn) fns;
    i32 nvars; /* variable ids are 0 .. nvars-1 */
    /* canon scratch */
    i32 *cmap;
    u32 *cgen;
    u32 gen;
    i32 ccap;
} FragCorpus;

FragCorpus *frag_corpus_new(int nlevels);
void frag_corpus_free(FragCorpus *c);
/* add(info, rows): returns the function index; the caller fills its info fields. Strings are copied. */
i32 frag_corpus_add(FragCorpus *c, const FragRowIn *rows, size_t n);
/* canon(fi, a, b) into out (cleared first) */
void frag_canon(FragCorpus *c, i32 fi, i32 a, i32 b, AvpI32Vec *out);
FragRepeats frag_seeds(FragCorpus *c, int level, i32 n, i32 k);
FragRepeat frag_extend(FragCorpus *c, int level, const FragOcc *occ, i32 n);
FragRepeats frag_repeats(FragCorpus *c, int level, i32 n, i32 k);
void frag_repeats_free(FragRepeats *r);
/* boilerplate(corpus, level, length, occ): "switch", "destructor" or NULL */
const char *frag_boilerplate(FragCorpus *c, int level, const FragRepeat *f);
/* drop_contained(frags) (in rank order): the kept ones, in order (the dropped ones are freed). */
FragRepeats frag_drop_contained(FragRepeats in);
/* The corpus rows of one function as FragRow (regs and disp), rows [s, s+len). */
void frag_corpus_rows(const FragCorpus *c, i32 fi, i32 s, i32 len, FragRow *out);

/* ---- the database ---- */
const char *frag_db_path(AvpArena *a, const ShsmRepo *r);
const char *frag_stamp(AvpArena *a, ShsmAsmIndex *ix, i64 window, i64 sites);
FragCorpus *frag_build_corpus(const ShsmRepo *r, ShsmAsmIndex *ix, int jobs);
typedef void (*FragLogFn)(const char *msg);
/* open_db: the connection, rebuilt when the stamp differs or rebuild is set. */
sqlite3 *frag_open_db(const ShsmRepo *r, ShsmAsmIndex *ix, bool rebuild, int jobs, i64 window, i64 sites,
                      FragLogFn log);
AVP_NORETURN void frag_sql_fail(sqlite3 *db);

/* ---- the model ---- */
typedef struct {
    i64 fn_address;
    const char *fn_name, *unit;
    i64 row, address;
    bool matched;
    const char *status, *source, *lib;
} FragOccRec;
typedef AVP_VEC(FragOccRec) FragOccRecs;

typedef struct {
    const char *id, *level;
    i64 rank, length, nfuncs, nunits, nocc, score;
    AvpStrVec pattern;
    FragOccRecs occ;
} Frag;
typedef AVP_VEC(Frag *) FragList;

typedef struct {
    bool matched;
    const char *status, *source, *lib;
} FragFnInfo;

typedef struct Frags {
    AvpArena *arena;
    const ShsmRepo *repo;
    ShsmAsmIndex *ix;
    sqlite3 *con;
    AvpJson *meta; /* object, select order */
    const ShsmUnits *units;
    AvpISet *matched; /* NULL: no report */
    const ShsmLibraries *libs;
    AvpIMap *fncache; /* address -> FragFnInfo * */
} Frags;

Frags *frags_open(const char *root, bool rebuild, int jobs, i64 window, i64 sites, FragLogFn log);
void frags_close(Frags *fr);
const char *frags_meta(const Frags *fr, const char *key); /* NULL when absent */
FragFnInfo frags_fninfo(Frags *fr, i64 addr, const char *unit);
Frag *frags_fragment(Frags *fr, const char *fid); /* raises USAGE when unknown */
FragList frags_level(Frags *fr, const char *level);
void frags_resolve(Frags *fr, const char *text, ShsmSymRow *out);
/* occ_rows(occ, length, before, after): (regs, disp) rows; *n is set. */
FragRow *frags_occ_rows(Frags *fr, const FragOccRec *o, i64 length, int before, int after, size_t *n);
Frag *frag_of_stmt(AvpArena *a, sqlite3_stmt *st, int col0);
AvpJson *frag_json(AvpArena *a, const Frag *f, bool with_occ);
AvpJson *frag_occ_json(AvpArena *a, const FragOccRec *o);
bool frag_lib_matches(AvpArena *a, const char *lib, const char *want);

typedef struct {
    i64 address;
    Frag *f;
} FragAt;
typedef AVP_VEC(FragAt) FragAts;
FragAts frag_fragments_at(AvpArena *a, sqlite3 *con, i64 address, const char *level, i64 min_functions, bool overlaps);

/* ---- subcommands ---- */
typedef struct {
    const char *level, *lib, *id, *function;
    i64 min_len, min_functions, min_tokens, min_sites;
    bool unmatched, overlaps, all;
    i64 limit; /* args.limit (0: no cap) */
    AvpStrVec paths;
} FragArgs;
int frag_cmd_list(const FragArgs *args, Frags *fr, ShsmOutput *out);
int frag_cmd_show(const FragArgs *args, Frags *fr, ShsmOutput *out);
int frag_cmd_in(const FragArgs *args, Frags *fr, ShsmOutput *out);
int frag_cmd_stats(const FragArgs *args, Frags *fr, ShsmOutput *out);
int frag_cmd_known(const FragArgs *args, Frags *fr, ShsmOutput *out);
int frag_cmd_source(const FragArgs *args, const ShsmRepo *r, ShsmOutput *out);
AvpStr frag_compact(AvpArena *a, const AvpStrVec *pattern, size_t width);

/* ---- source side ---- */
bool frag_single_call(const AvpStr *texts, size_t n);
/* trim(texts, prev_ok): false for None; else [*s, *e) and *kind ("statements" or "expression"). */
bool frag_trim(const AvpStr *texts, size_t n, bool prev_ok, i64 *s, i64 *e, const char **kind);

int fragments_main(int argc, char **argv);
