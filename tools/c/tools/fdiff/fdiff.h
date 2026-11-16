/* fdiff.exe: a port of python tools/mwcc_trace/fdiff.py (side-by-side retail/compiled listing of one function from
 * objdiff-cli's one-shot JSON, with the relocation targets checked by the cc_codecmp library).
 *
 * The run goes through an FdiffEnv whose hooks stand in for the Python functions the tests stub (build_unit,
 * objdiff_json, target_result, shutil.which, subprocess.run); a NULL hook is the real one. Environment errors are
 * thrown with FDIFF_ENV_ERROR (the Python's EnvError) and printed as "error: <message>", exit 2. */
#pragma once
#include "avp/avp.h"

/* FDIFF_CRASH: what the Python did not catch (a tool that cannot start): the message on stderr, exit 1. */
enum { FDIFF_ENV_ERROR = 0x4644, FDIFF_CRASH };

/* One codecmp target mismatch as fdiff reads it (TargetDiff: i, j, kind, detail). */
typedef struct {
    size_t i, j;
    AvpStr kind, detail;
} FdiffTarget;

/* codecmp's FnResult as fdiff reads it: targets, checked_targets, len(unresolved). */
typedef struct {
    FdiffTarget *v;
    size_t n;
    size_t checked;
    size_t nunresolved;
} FdiffTargets;

/* A row of rows_of: (k, objdiff differs, retail text, compiled text, retail index, compiled index); an index is -1
 * where Python had None (a gap). */
typedef struct {
    size_t k;
    bool diff;
    AvpStr fa, fb;
    i64 i, j;
} FdiffRow;

typedef struct {
    FdiffRow *v;
    size_t n;
} FdiffRows;

/* target_marks: per row k the detail lines (marks[k].n == 0: no mark), plus the details no row carries. */
typedef struct {
    AvpStrVec *marks; /* rows.n entries */
    AvpStrVec loose;
} FdiffMarks;

typedef struct FdiffEnv {
    const char *root; /* NULL: avp_root() */
    void *ctx;
    /* build_unit(unit): output (the ninja tail) appended to out; throws FDIFF_ENV_ERROR. */
    void (*build_unit)(void *ctx, AvpArena *a, AvpBuf *out, const char *unit);
    /* objdiff_json(unit, sym): the parsed document; throws FDIFF_ENV_ERROR. */
    AvpJson *(*objdiff_json)(void *ctx, AvpArena *a, const char *unit, const char *sym);
    /* target_result(unit, sym): false where Python returned None; throws FDIFF_ENV_ERROR. */
    bool (*target_result)(void *ctx, AvpArena *a, const char *unit, const char *sym, FdiffTargets *out);
    /* shutil.which(name): NULL when absent. */
    char *(*which)(void *ctx, AvpArena *a, const char *name);
    /* subprocess.run(argv, cwd=root, capture_output=True, text=True): false when it could not start. */
    bool (*run)(void *ctx, AvpArena *a, const char *const *argv, const char *cwd, i64 *rc, AvpStr *out, AvpStr *err);
} FdiffEnv;

/* The parser (prog fdiff.py, the module docstring as description and epilog). */
AvpArgParser *fdiff_parser(void);

/* parse_json(text): the document; throws FDIFF_ENV_ERROR as the Python's. */
AvpJson *fdiff_parse_json(AvpArena *a, AvpStr text);
/* find_symbol(doc, side, sym): NULL when absent. */
const AvpJson *fdiff_find_symbol(const AvpJson *doc, const char *side, AvpStr sym);
FdiffRows fdiff_rows_of(AvpArena *a, const AvpJson *left, const AvpJson *right);
FdiffMarks fdiff_target_marks(AvpArena *a, const FdiffRows *rows, const FdiffTargets *res);
/* render(rows, marks, show_all): the output lines. */
AvpStrVec fdiff_render(AvpArena *a, const FdiffRows *rows, const FdiffMarks *marks, bool show_all);

/* main(argv) after parsing: run(args) with the EnvError mapped to "error: ..." and exit 2. Output to out. */
int fdiff_run(const FdiffEnv *env, AvpArena *a, const AvpArgs *args, AvpBuf *out);
