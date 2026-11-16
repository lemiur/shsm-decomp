/* unit_plan: tools/unit_plan.py in C. Proposed `.text` unit cuts with their evidence, the calibration on the units
 * already split (`check`), and plan_for() for other tools (static library tool_unit_plan; API.md). */
#pragma once
#include "avp/avp.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

/* ---- constants (unit_plan.py) ---- */
#define UP_MAX_SPAN 300   /* wider spans are extern globals shared across units */
#define UP_LOOSE_SPAN 40  /* other data counts as private only within this many functions */
#define UP_TAIL_MAX 3     /* functions whose address a sinit takes after it */
#define UP_CLASS_SPAN 40  /* a class whose functions spread wider is ignored */
#define UP_THUNK_MAX 0x10 /* a this-adjust thunk: at most this many bytes */
#define UP_NONE INT64_MIN /* Python None in integer fields */
#define UP_PS2_NEAR 8     /* PS2 ranks at most this far apart (witness's default --gap) are PS2 neighbours: ps2run */
#define UP_PS2_SPAN 20    /* mapped functions farther than this many functions from the gap are not consulted */

/* Tags: the POSITIVE tuple's order first, then the two that are not positive. A tag set is a bit mask. */
enum {
    UP_T_SINIT,
    UP_T_DUPFLOAT,
    UP_T_DUPFLOAT_N, /* dupfloat~ */
    UP_T_DUPSTR,
    UP_T_DUPSTR_N,
    UP_T_FILE,
    UP_T_FILE_N,
    UP_T_WEAK,
    UP_T_NOCROSS,
    UP_T_TEXTPAD,
    UP_T_DATAPAD,
    UP_T_ORDER,
    UP_T_CLASS,
    UP_T_PS2,   /* the PS2 counterparts on either side are far apart in PS2 link order */
    UP_T_PS2_N, /* ps2~: the same, with several free gaps between the two mapped functions */
    UP_NPOSITIVE,
    UP_T_INCLASS = UP_NPOSITIVE,
    UP_T_PS2RUN, /* the PS2 counterparts on either side are PS2 neighbours: evidence against a cut */
    UP_T_KNOWN,
    UP_NTAGS
};
typedef u32 UpTags;
typedef i64 UpPair[2];
#define UP_BIT(t) ((UpTags)1u << (t))
extern const char *const UP_TAG_NAMES[UP_NTAGS];
/* sorted(tags) as names; "+".join of them (a combination key). */
AvpStrVec up_tag_names(UpTags ts);
AvpStr up_tag_key(AvpArena *a, UpTags ts);

/* ---- the model ---- */
typedef enum { UP_KIND_OTHER, UP_KIND_FLOAT, UP_KIND_STRING } UpKind;

typedef struct {
    i64 address, size;
    const char *name, *scope; /* scope "global" "local" "weak" */
    bool sinit;               /* a .ctors target */
    AvpI64Vec taken;          /* addresses of functions it takes (a set: sorted, unique) */
    AvpI64Vec data;           /* data addresses it references (sorted) */
    const char *unit;         /* source unit path when its unit has source, else NULL */
    const char *lib;
    AvpStrVec strings;        /* name-like strings it references, first-use order */
    i64 thunk;                /* the function a this-adjust thunk branches to, else UP_NONE */
    bool has_ps2;             /* the witness maps it to a PS2 function (false: no PS2 evidence) */
    i64 ps2;                  /* that function's PS2 link-order rank (witness_lookup.h wt_lookup_order) */
} UpFunc;

typedef struct {
    i64 address, size;
    const char *section, *scope;
    AvpStr value; /* p NULL: None (float repr or string text, NULs kept) */
    UpKind kind;
} UpDatum;

typedef struct {
    const char *name;
    AvpI64Vec addrs; /* sorted */
} UpClass;

typedef struct {
    const char *section;
    AvpI64Vec points; /* addresses of data that follow a hole, sorted */
} UpPads;

typedef struct {
    UpFunc *fns; /* address order */
    size_t n;
    UpDatum *data; /* the {address: Datum} dict's values in insertion order */
    size_t ndata;
    AvpIMap *data_at; /* address -> index into data */
    i64 (*local_calls)[2];
    size_t nlocal;
    AvpIMap *align; /* fn address -> the alignment its library uses (16 only when known) */
    UpClass *classes;
    size_t nclasses;
    const char *class_note; /* why the class evidence is missing or stale, else NULL */
    UpPads *pads;
    size_t npads;
} UpModel;

/* Model(fns, data, local_calls, func_align, classes, class_note): fns sorted by address (stable); data given in
 * dict order with unique addresses; align pairs (address, alignment). Arrays are copied. */
UpModel *up_model_new(AvpArena *a, const UpFunc *fns, size_t n, const UpDatum *data, size_t nd,
                      const i64 (*local_calls)[2], size_t nl, const i64 (*align)[2], size_t nal, const UpClass *classes,
                      size_t nc, const char *class_note);
/* model.index[address]: the last function at address, -1 when none. */
i64 up_model_index(const UpModel *m, i64 address);
const UpDatum *up_model_datum(const UpModel *m, i64 address); /* NULL when absent */
static inline i64 up_fn_end(const UpFunc *f) { return f->address + f->size; }

/* load_model(ix, r): the whole .text of the retail binary. */
UpModel *up_load_model(AvpArena *a, ShsmAsmIndex *ix, const ShsmRepo *r);
/* is_thunk(rows): `addi`/`subi r3, r3, N` (one or more) then `b target`. */
bool up_is_thunk(const AvpStr *mnemonics, const AvpStr *operands, size_t n);
/* classes_json(r): os.path.join(r.version_build_dir, "classes.json"). */
const char *up_classes_json_path(AvpArena *a, const ShsmRepo *r);
/* load_classes(r): the database (NULL: None) and *note, why the evidence is missing or stale (NULL: current); the
 * stamp comparison is the classes tool's (classes_stale_reason_inputs). */
const AvpJson *up_classes_load(AvpArena *a, const ShsmRepo *r, const char **note);
/* classes.mangled_class(name): the class of a CW-mangled member name, NULL for None. */
const char *up_mangled_class(AvpArena *a, const char *name);
/* class_groups(db, fns): [(class name, sorted function addresses)]. */
UpClass *up_class_groups(AvpArena *a, const AvpJson *db, const UpFunc *fns, size_t n, size_t *nout);
/* _flag_align(cflags): 0 for None. */
i64 up_flag_align(const AvpStrVec *cflags);

/* ---- library ranges ---- */
typedef struct {
    i64 start, end;
    AvpStrVec names;
} UpLibRange;
/* find_library(lib, repo) over a libraries table (NULL: the checkout's). Raises SHSM_EXC_USAGE. */
UpLibRange up_find_library(AvpArena *a, const char *lib, const ShsmRepo *r, const void *libs /* ShsmLibraries* */);
/* parse_range(text). Raises SHSM_EXC_USAGE. */
void up_parse_range(AvpArena *a, const char *text, i64 *start, i64 *end);

/* ---- evidence ---- */
typedef struct {
    size_t ngaps; /* n - 1 (0 when n < 1) */
    UpTags *tags; /* per gap g (cut after fns[g]) */
    i32 *strong, *loose;
} UpEvidence;
UpEvidence up_gap_evidence(AvpArena *a, const UpModel *m);

typedef struct {
    i64 hit, total;
} UpCount;
typedef struct {
    /* calib["tag"]: POSITIVE tags in dict order */
    u8 tag_order[UP_NPOSITIVE];
    size_t ntags;
    UpCount tag[UP_NPOSITIVE];
    bool has_tag[UP_NPOSITIVE];
    /* calib["combo"]: keys in dict order */
    AvpStrVec combo_keys;
    AVP_VEC(UpCount) combo;
    AvpMap *combo_index; /* key -> index */
    /* Each PS2 tag (UP_PS2_TAG[k]) over the predicted labelled gaps that have other positive evidence: ps2_cut[k]
     * over the true cuts, ps2_other[k] over the others (hit: gaps with the tag). */
    UpCount ps2_cut[3], ps2_other[3];
} UpCalib;
#define UP_NPS2 3
#define UP_PS2_TAGS (UP_BIT(UP_T_PS2) | UP_BIT(UP_T_PS2_N) | UP_BIT(UP_T_PS2RUN))
extern const int UP_PS2_TAG[UP_NPS2]; /* ps2, ps2~, ps2run */
/* The add-one smoothed likelihood ratio P(tag | cut) / P(tag | no cut) of UP_PS2_TAG[k]. */
double up_ps2_ratio(const UpCalib *calib, int k);
/* combine(ts): the in-sample precision of the combination when it was seen at least 5 times; else, with PS2 tags
 * and other positive evidence, the probability without the PS2 tags with its odds times their ratios; else the
 * product over the positive tags. calib NULL: 0.5. */
double up_combine(UpTags ts, const UpCalib *calib);

typedef struct {
    size_t g;
    UpTags tags;
    double p;
} UpCut;
typedef AVP_VEC(UpCut) UpCuts;
UpCuts up_predict(const UpModel *m, const UpEvidence *ev, const UpCalib *calib, double min_p);

/* truth(m, splits): per gap -1 (unlabelled), 0 (inside a source unit), 1 (a cut). ranges: the .text (start, end) of
 * every split unit that has one, any order. */
i8 *up_truth(AvpArena *a, const UpModel *m, const i64 (*ranges)[2], size_t nr);
i8 *up_truth_splits(AvpArena *a, const UpModel *m, const ShsmRepo *r, size_t *nunits);

typedef struct {
    const char *lib;
    i64 cuts, tp, fp, missed;
} UpLibRow;
typedef AVP_VEC(UpLibRow) UpLibRows;
typedef struct {
    UpCalib *calib;
    i64 recall[UP_NPOSITIVE];
    i64 ncuts;
} UpEvaluation;
UpEvaluation up_evaluate(AvpArena *a, const UpModel *m, const UpEvidence *ev, const i8 *lab);
typedef struct {
    i64 tp[3], fp[3], missed[3]; /* high, medium+, all */
    UpLibRows libs;              /* first-labelled-gap order */
} UpBands;
UpBands up_bands(const UpModel *m, const UpEvidence *ev, const UpCalib *calib, const i8 *lab, double lib_threshold);
AvpStr up_accuracy_header(AvpArena *a, const UpCalib *calib);
AvpJson *up_calib_tag_json(AvpArena *a, const UpCalib *calib);

/* ---- plan ---- */
typedef struct {
    i64 start, end, functions;
    UpTags end_tags; /* known included */
    bool has_conf;
    double confidence;
    AvpStrVec names; /* at most 6 */
    AvpStrVec known_units;
    bool sinit;
    const char *command;
} UpUnit;
typedef AVP_VEC(UpUnit) UpUnits;
/* plan(m, start, end, calib, tags, strong, min_p, known); known may be NULL. Raises SHSM_EXC_USAGE when the range
 * holds no function. */
UpUnits up_plan(AvpArena *a, const UpModel *m, i64 start, i64 end, const UpCalib *calib, const UpEvidence *ev,
                double min_p, const i8 *known);
const char *up_path_for(AvpArena *a, const AvpStrVec *names);
const char *up_conf(bool has, double p);
AvpJson *up_unit_json(AvpArena *a, const UpUnit *u);
double up_min_p(const char *min_conf); /* MIN_P[min_conf]; -1 when unknown */

/* ---- the disk cache and plan_for ---- */
typedef struct {
    UpModel *m;
    UpEvidence ev;
    UpCalib *calib;
    i8 *lab;
} UpState;
/* load_cached(r, ix, build): from build/index/unit_plan.cbin when its stamp is current, else computed and stored;
 * NULL when not current and build is false. */
UpState *up_load_cached(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix, bool build);
const char *up_cache_path(AvpArena *a, const ShsmRepo *r);
/* The cache stamp from the index's `files` summary (count, newest mtime, total size) and the input files' stats. */
AvpStr up_stamp_inputs(AvpArena *a, const ShsmRepo *r, const char *files_row);
/* _stamp(r, ix): the stamp load_cached keys the cache on (a caller keeping the state in memory keys on it too). */
AvpStr up_cache_stamp(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix);
/* The model on its own through the cache format (for tests). */
void up_model_write(AvpCacheWriter *w, const UpModel *m);
UpModel *up_model_read(AvpArena *a, AvpCacheReader *rd);

typedef struct {
    i64 start, end;
    UpCalib *accuracy;
    const char *header;
    UpUnits units;
    const char *class_note;
} UpPlanDoc;
/* plan_for(start, end, repo, min_conf, ix, build, refresh). r NULL: the cwd's checkout; ix NULL: opened read-only
 * (refresh as given) and closed. NULL when build is false and the cache is not current. */
UpPlanDoc *up_plan_for(AvpArena *a, i64 start, i64 end, const ShsmRepo *r, const char *min_conf, ShsmAsmIndex *ix,
                       bool build, bool refresh);
AvpJson *up_plan_doc_json(AvpArena *a, const UpPlanDoc *d); /* {range, accuracy, header, units, class_note} */
/* warn_classes(m): one stderr warning per process and note. */
void up_warn_classes(const UpModel *m);

/* the front end */
int unit_plan_main(int argc, char **argv);
