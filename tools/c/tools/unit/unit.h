/* unit: the shared model of tools/unit.py (symbols.txt, splits.txt, configure.py, config.yml, the DOL's .ctors
 * table, the retail asm blocks and the CodeWarrior demangler the stubs need). The logic files:
 *   unit_core.c      text files, symbols, splits, configure.py, force_active, DOL, the project
 *   unit_asm.c       the asm block parser and index (unit.py's AsmIndex)
 *   unit_demangle.c  demangle() and sig_text()
 *   unit_infer.c     text_range_findings, infer_data, find_ctors, unit_xref (unit_infer.h)
 *   unit_stub.c      build_source, split_entry_text, insert_object (unit_stub.h)
 *   unit_cmd.c       new, check, park, flip
 *   unit_cli.c       the argument parser and main() */
#pragma once
#include "avp/avp.h"
#include "shsm_cli.h"

#define UNIT_GAME "R5WEA4"
#define UNIT_MAX_PAD_EXTEND 0x20
#define UNIT_WEAK_RUN_SLOP 8
#define UNIT_GATE_PENDING_EXIT 3
#define UNIT_REGEN_COMMAND "tools/bin/ninja.exe build.ninja"

/* unit.py's ToolError: printed as `error: <message>`, exit 2. The message is unit_error_message() (it can be
 * longer than AvpTry.msg). */
enum { UNIT_TOOL_ERROR = 0x554E };
AVP_NORETURN void unit_fail(AVP_PRINTF_FMT const char *fmt, ...);
const char *unit_error_message(void);

/* out(): one line on stdout, or into the capture buffer while one is set (flip runs check silently). */
void unit_out(AVP_PRINTF_FMT const char *fmt, ...);
void unit_out_str(AvpStr s); /* s and a newline */
void unit_capture(AvpBuf *b); /* NULL: back to stdout */
/* A progress line `[ Ns] step <name>[: <note>]` with the seconds since the first one (or since unit_step_reset),
 * written and flushed at once: a step that runs for minutes (build, gate) is announced when it starts. */
void unit_step_reset(void);
void unit_step(const char *name, const char *note);

/* ---- text files with their own line endings ---- */
/* read_text: the text with \n endings and the file's eol ("\r\n" when it holds one, else "\n"). */
AvpStr unit_read_text(AvpArena *a, const char *path, const char **eol);
void unit_write_text(AvpArena *a, const char *path, AvpStr text, const char *eol);
const char *unit_guess_eol(AvpArena *a, const char *dir);

/* ---- symbols.txt ---- */
typedef struct {
    const char *name, *sec, *kind, *scope, *rest;
    i64 addr, size;
} UnitSym;
typedef AVP_VEC(UnitSym) UnitSymVec;
typedef AVP_VEC(const UnitSym *) UnitSymPtrVec;
typedef AVP_VEC(size_t) UnitSizeVec;
UnitSymVec unit_load_symbols(AvpArena *a, const char *path);

/* ---- splits.txt ---- */
typedef struct {
    const char *sec;
    i64 lo, hi;
} UnitRange;
typedef AVP_VEC(UnitRange) UnitRanges; /* a dict section -> (start, end): insertion order */
const UnitRange *unit_ranges_get(const UnitRanges *r, const char *sec);
void unit_ranges_set(UnitRanges *r, const char *sec, i64 lo, i64 hi);

typedef struct {
    const char *path;
    UnitRanges ranges;
    size_t first_line, last_line;
} UnitSplit;
typedef AVP_VEC(UnitSplit) UnitSplits;
UnitSplits unit_load_splits(AvpArena *a, AvpStr text);
bool unit_overlaps(i64 a0, i64 a1, i64 b0, i64 b1);

/* ---- configure.py ---- */
typedef struct {
    const char *path, *status;
    size_t status_start, status_end, start, end;
    size_t lib;
} UnitObj;
typedef struct {
    const char *name;
    size_t start, end;
} UnitLib;
typedef struct {
    AvpStr text;
    AVP_VEC(UnitLib) libs;
    AVP_VEC(UnitObj) objects;
} UnitConfig;
UnitConfig *unit_parse_configure(AvpArena *a, AvpStr text);
const UnitObj *unit_config_find(const UnitConfig *c, const char *path);

/* ---- config.yml force_active ---- */
AvpStrVec unit_load_force_active(AvpArena *a, const char *path);

/* ---- the project ---- */
typedef struct {
    i64 word, entry;
} UnitCtorEnt;
struct UnitIndex;
typedef struct {
    AvpArena *a;
    const char *root; /* Path(root).resolve() */
    const char *symbols_path, *splits_path, *config_yml, *configure_path, *dol_path;
    bool syms_loaded;
    UnitSymVec syms;
    AvpMap *sym_by_name; /* name -> UnitSym* (the last one) */
    UnitSymPtrVec text_sorted; /* .text functions by address (stable) */
    struct UnitIndex *index;
    bool ctors_loaded;
    AVP_VEC(UnitCtorEnt) ctors; /* the DOL's .ctors table: non-zero words with their entry address */
} UnitProject;
UnitProject *unit_project_new(AvpArena *a, const char *root);
const UnitSymVec *unit_syms(UnitProject *p);
const UnitSym *unit_sym(UnitProject *p, const char *name);
/* text_funcs(lo, hi, sec): functions of sec with lo <= addr < hi, by address. */
UnitSymPtrVec unit_text_funcs(UnitProject *p, i64 lo, i64 hi, const char *sec);
UnitSplits unit_project_splits(UnitProject *p);
UnitConfig *unit_project_config(UnitProject *p);
struct UnitIndex *unit_project_index(UnitProject *p);
/* ctor_table().get(addr): the .ctors entries holding addr (count in *n, at most cap written). */
size_t unit_ctor_hits(UnitProject *p, i64 addr, i64 *out, size_t cap);
bool unit_in_ctor_table(UnitProject *p, i64 addr);
UnitSymPtrVec unit_sinit_funcs(UnitProject *p, const UnitSymPtrVec *funcs);
const UnitSplit *unit_find_split(UnitProject *p, const char *arg, const UnitSplits *splits);

bool unit_parse_range(AvpArena *a, const char *text, i64 *lo, i64 *hi);
int unit_func_align_for(const char *path);
bool unit_is_game_path(const char *path);
const char *unit_stem(AvpArena *a, const char *path); /* os.path.splitext(path)[0] */

/* ---- the asm index ---- */
typedef struct {
    const char *callee, *arg; /* arg NULL: None */
} UnitCtorArg;
typedef struct UnitBlock {
    const char *name, *kind, *sec, *scope;
    i64 addr, size;
    const char **refs; /* sorted */
    size_t nrefs;
    bool zero;
    bool words_only; /* an object made only of `.4byte` words: names or zero literals (a vtable or a pointer table) */
    UnitCtorArg *ctor_args;
    size_t nctor;
    const char **stores; /* sorted */
    size_t nstores;
} UnitBlock;
typedef AVP_VEC(UnitBlock *) UnitBlockVec;
typedef struct UnitIndex {
    UnitBlockVec blocks;
    AvpMap *by_name;   /* name -> UnitBlock* (the last block of that name) */
    AvpMap *by_sec;    /* section -> UnitBlockVec* by address */
    AvpMap *referrers; /* name -> UnitBlockVec* */
    const char *note;  /* the listings' note, or NULL */
    const char *record;
} UnitIndex;
/* AsmIndex.load(root): raises UNIT_TOOL_ERROR when build/R5WEA4/asm does not exist. */
UnitIndex *unit_index_load(AvpArena *a, const char *root);
/* The same from listing texts (tests). */
UnitIndex *unit_index_from_texts(AvpArena *a, const AvpStr *texts, size_t n);
UnitBlock *unit_index_block(const UnitIndex *x, const char *name);
const UnitBlockVec *unit_index_referrers(const UnitIndex *x, const char *name); /* never NULL */
UnitBlockVec unit_index_in_range(AvpArena *a, const UnitIndex *x, const char *sec, i64 lo, i64 hi);
bool unit_is_gap_name(const char *name); /* GAP_RE */
bool unit_is_pad_name(const char *name); /* dtk's pad_NN_XXXXXXXX_sec code filler, GAP_RE's shape */
bool unit_is_asm_source(const char *path); /* an assembly source unit (`.s`) */
bool unit_block_refs(const UnitBlock *b, const char *name);

/* ---- the demangler ---- */
typedef struct {
    const char *raw, *base, *cls, *thunk;
    AvpStrVec args; /* valid when has_args */
    bool has_args, is_const, is_ctor, is_dtor, sinit, mangled, complex;
    AvpStrVec used; /* the set of class names used, in first-seen order */
} UnitDemangled;
UnitDemangled unit_demangle(AvpArena *a, const char *name);
const char *unit_dm_short_cls(AvpArena *a, const UnitDemangled *d);
const char *unit_dm_fname(AvpArena *a, const UnitDemangled *d);
const char *unit_dm_qualified(AvpArena *a, const UnitDemangled *d);
bool unit_dm_generated(const UnitDemangled *d);
const char *unit_sig_text(AvpArena *a, const UnitDemangled *d, bool defining);
bool unit_is_base_type(const char *t); /* t in BASE_TYPES.values() */

/* ---- formatting ---- */
const char *unit_repr(AvpArena *a, const char *s); /* repr(str) */
