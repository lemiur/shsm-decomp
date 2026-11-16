/* gate_regress: the score-regression logic of the gate, and the pieces of tools/merge_lane.py and
 * tools/score_history.py it uses (ported here so the merge tool's later port links them).
 *
 *   report.py        compare_reports                                  -> gate_compare_reports
 *   gate.py          fallen_functions, classify_falls, format_fall    -> gate_fallen_functions, gate_classify_falls,
 *                                                                        gate_format_fall
 *   merge_lane.py    func_scores, parse_changes_text, is_unit_row,     -> gate_func_scores, gate_parse_changes_text,
 *                    json_function_regressions, report_index,            gate_is_unit_row,
 *                    _addr_int, merge_by_symbol, all_unit_index,         gate_json_function_regressions,
 *                    SourceSide, classify_decrease                       gate_report_index, gate_addr_int,
 *                                                                        gate_merge_by_symbol, gate_all_unit_index,
 *                                                                        GateSourceSide, gate_classify_decrease
 *   score_history.py demangle, FileIndex, source_units, guess_path     -> gate_sh_demangle, GateFileIndex,
 *                                                                        gate_source_units, gate_guess_path
 *
 * score_history.index_source (the per-function source hashes) is score_history's linked C index (sh_index_source,
 * tool_score_history), called through GateIndexer; its JSON result feeds GateFileIndex unchanged.
 *
 * Allocation: everything from the arena `a`; vectors are heap vectors (never freed in a short-lived tool). */
#pragma once
#include "avp/avp.h"
#include "shsm_report.h"
#include "shsm_symbols.h"

typedef struct GateRun GateRun;

/* ---- one fallen function (gate.py's {key: (unit, name, old, new)} and the classified tuples) ------------------ */
typedef struct GateFall {
    const char *key;      /* str(key): the address in decimal, or "('unit', 'name')" */
    bool has_addr;        /* the key is an address */
    i64 addr;
    const char *unit, *name;
    double old;
    bool has_new;         /* new is None when false ("gone") */
    double new_;
    const char *label;    /* the classification's label (edited / unchanged lists), else NULL */
} GateFall;
/* An insertion-ordered dict of falls by key (assignment to an existing key replaces it in place). */
typedef struct GateFalls {
    GateFall *v;
    size_t n, cap;
    AvpMap *index;        /* key -> position */
} GateFalls;
void gate_falls_put(GateFalls *fs, GateFall f);
const GateFall *gate_falls_get(const GateFalls *fs, const char *key);
const char *gate_tuple_key(AvpArena *a, const char *unit, const char *name); /* str((unit, name)) */
const char *gate_addr_key(AvpArena *a, i64 addr);                            /* str(addr) */

/* ---- report.compare_reports -------------------------------------------------------------------------------- */
typedef struct GateCmpRow {
    const char *unit, *fn;
    bool has_old, has_new;
    double old, new_;
} GateCmpRow;
typedef AVP_VEC(GateCmpRow) GateCmpRows;
GateCmpRows gate_compare_reports(const ShsmReport *old, const ShsmReport *new_, bool only_changed);

/* fallen_functions(base, fresh, units): units NULL is every baseline unit. Fills out (dict order). */
void gate_fallen_functions(AvpArena *a, const ShsmReport *base, const ShsmReport *fresh, const AvpSet *units,
                           GateFalls *out);
/* format_fall: "    %-44s %6.2f%% -> %-7s %s [%s]". */
const char *gate_format_fall(AvpArena *a, const GateFall *f);

/* ---- merge_lane.py: report_changes.json and report.json ---------------------------------------------------- */
typedef struct GateScore {
    const char *unit;
    const char *name, *onto_name;  /* NULL for None */
    const AvpJson *addr;           /* the raw metadata.virtual_address, NULL for None */
    bool has_addr_int;             /* addr_int (merge_by_symbol) */
    i64 addr_int;
    bool has_from, has_to;
    double from, to;
    const char *unit_lane;         /* merge_by_symbol: the lane side's unit, NULL for None */
    const char *key_name;          /* func_scores: the name in the entry's key (NULL for None) */
} GateScore;
typedef AVP_VEC(GateScore) GateScores;
/* _addr_int(addr): int(addr), false for None or a value int() refuses. */
bool gate_addr_int(const AvpJson *addr, i64 *out);
/* func_scores(change_json): the entries in dict order. Raises SHSM_EXC_TYPE where float() of a null score did. */
GateScores gate_func_scores(AvpArena *a, const AvpJson *change_json);
/* merge_by_symbol(scores): one entry per function (merged.values() order). */
GateScores gate_merge_by_symbol(AvpArena *a, const GateScores *scores);
typedef struct GateHit {
    const char *unit, *name;       /* name NULL for None */
    double score;
} GateHit;
typedef struct GateUnitIndex {
    AvpIMap *by_addr;              /* addr -> GateHit * */
    AvpMap *by_name;               /* gate_tuple_key(unit, name) -> GateHit * */
} GateUnitIndex;
/* all_unit_index(report): over every unit; an address in two units keeps the better score. */
GateUnitIndex gate_all_unit_index(AvpArena *a, const AvpJson *report);
const GateHit *gate_unit_index_addr(const GateUnitIndex *ix, i64 addr);
const GateHit *gate_unit_index_name(AvpArena *a, const GateUnitIndex *ix, const char *unit, const char *name);

typedef struct GateChangeRow {
    const char *name, *key;
    double from, to;
} GateChangeRow;
typedef AVP_VEC(GateChangeRow) GateChangeRows;
GateChangeRows gate_parse_changes_text(AvpArena *a, AvpStr text);       /* parse_changes_text */
bool gate_is_unit_row(const GateChangeRow *row, const AvpSet *unit_names); /* is_unit_row */
i64 gate_json_function_regressions(const AvpJson *cj);                  /* json_function_regressions */

/* score_history.source_units(report): the source units with their (symbol, addr, score, size). */
typedef struct GateSrcFunc {
    const char *symbol;
    i64 addr;
    double score;
    i64 size;
} GateSrcFunc;
typedef struct GateSrcUnit {
    const char *unit, *source_path;    /* source_path "" when absent */
    GateSrcFunc *funcs;
    size_t nfuncs;
} GateSrcUnit;
typedef AVP_VEC(GateSrcUnit) GateSrcUnits;
GateSrcUnits gate_source_units(AvpArena *a, const AvpJson *report);
/* report_index(report, sh): by_addr key gate_tuple_key(unit, decimal addr), by_name key gate_tuple_key(unit,
 * symbol); values GateReportEntry *. */
typedef struct GateReportEntry {
    const char *unit, *source_path, *symbol;
    double score;
    i64 addr;
} GateReportEntry;
typedef struct GateReportIndex {
    AvpMap *by_addr, *by_name;
} GateReportIndex;
GateReportIndex gate_report_index(AvpArena *a, const AvpJson *report);

/* ---- score_history.py: demangle, FileIndex, guess_path ----------------------------------------------------- */
/* demangle(sym): candidate source names; *sinit true (and no candidates) for a static initialiser. */
AvpStrVec gate_sh_demangle(AvpArena *a, const char *sym, bool *sinit);
typedef bool (*GateExistsFn)(void *ctx, const char *rel_path);
/* guess_path(unit, exists): "src/<unit>.<ext>" for the first of .cpp .c .cp .cc that exists, else "". */
const char *gate_guess_path(AvpArena *a, const char *unit, GateExistsFn exists, void *ctx);

/* FileIndex over index_source's JSON ({"funcs": [{name, line, end, addr, hash}], "unit_hash", "rest_hash"}). */
typedef struct GateFileIndex {
    const AvpJson *data;
    AvpIMap *by_addr;              /* addr -> func object (the first) */
    AvpMap *by_name;               /* name -> AvpPtrVec * of func objects */
    const char *unit_hash, *rest_hash;
} GateFileIndex;
GateFileIndex *gate_file_index_new(AvpArena *a, const AvpJson *data);
/* lookup(symbol, addr) -> the func object or NULL; *basis "addr", "name", "sinit" or "unit". */
const AvpJson *gate_file_index_lookup(AvpArena *a, const GateFileIndex *fi, const char *symbol, bool has_addr,
                                      i64 addr, const char **basis);

/* index_source(path, text) -> its JSON. Raises (any class) where the Python raised; the message is str(e). */
typedef struct GateIndexer {
    AvpJson *(*index)(struct GateIndexer *ix, AvpArena *a, const char *path, AvpStr text);
    void *ctx;
} GateIndexer;
/* The default index: score_history's linked index_source (sh_index_source), as its JSON. */
AvpJson *gate_index_linked(GateIndexer *ix, AvpArena *a, const char *path, AvpStr text);

/* SourceSide(sh, read): read(path) -> the text, or false for None. Indexes are cached per path. */
typedef bool (*GateReadFn)(void *ctx, AvpArena *a, const char *path, AvpStr *text);
typedef struct GateSourceSide {
    GateReadFn read;
    void *read_ctx;
    GateIndexer *ix;
    AvpMap *cache;                 /* path -> GateFileIndex * (NULL value: None) */
} GateSourceSide;
GateFileIndex *gate_source_side_index(AvpArena *a, GateSourceSide *s, const char *path);

/* classify_decrease's entry. */
typedef struct GateDecrease {
    const char *unit;
    const char *name;              /* the lane symbol, NULL for None */
    const char *onto_name;
    bool has_addr;
    i64 addr;
    const char *path_lane, *path_onto; /* "" when unknown */
} GateDecrease;
/* classify_decrease -> edited; *label why. */
bool gate_classify_decrease(AvpArena *a, const GateDecrease *e, GateSourceSide *lane, GateSourceSide *onto,
                            GateExistsFn exists_lane, void *exists_ctx, bool headers_changed, const char **label);

/* ---- gate.py classify_falls -------------------------------------------------------------------------------- */
/* git(*args) -> the run (the gate's runner). */
typedef GateRun (*GateGitFn)(void *ctx, AvpArena *a, const char *const *args, size_t n);
/* classify_falls(ml, tree, git, rev, report, fallen): fills edited and unchanged (labels set), in str(key)
 * order. The tree's tools/score_history.py must exist (the Python loads it first). */
void gate_classify_falls(AvpArena *a, const char *tree, GateGitFn git, void *git_ctx, const char *rev,
                         const ShsmReport *report, const GateFalls *fallen, GateIndexer *ix, GateFalls *edited,
                         GateFalls *unchanged, GateFalls *placeholder);

/* ---- a function that left its unit by a resplit (not a regression) ------------------------------------------------ */
/* `unit.exe resplit` shrinks a unit's .text range; the functions that fall outside it go back to a dtk auto unit,
 * while the baseline still lists them under the old unit with the stub's meaningless score. The two inputs are the
 * lane's splits.txt, the base revision's splits.txt and the lane's symbols.txt (their texts). */
typedef struct GateResplit {
    const ShsmSplits *lane, *base;
    const ShsmSymbolTable *syms;
} GateResplit;
/* NULL when a text is missing (NULL): no function is then exempt. */
GateResplit *gate_resplit_new(AvpArena *a, const AvpStr *lane_splits, const AvpStr *base_splits,
                              const AvpStr *lane_symbols);
typedef struct GateLeftUnit {
    const char *old_range, *new_range; /* ".text 0x801AC530-0x801AC770", ".text 0x801AC530-0x801AC72C" */
} GateLeftUnit;
/* True when the baseline function (report unit name, symbol name, optional report address) lies outside every range of
 * its unit in the lane's splits, lay inside one in the base's, and was not exact (`from` below 100%). The address is
 * the symbol's in symbols.txt, else the report's. */
bool gate_resplit_left(AvpArena *a, const GateResplit *rs, const char *unit, const char *name, bool has_addr, i64 addr,
                       double from, GateLeftUnit *out);
/* "    fn  20.00% ->  0.00%  unit  [.text 0x... -> 0x...]" (to < 0: "gone"). */
const char *gate_left_unit_line(AvpArena *a, const char *name, double from, bool has_to, double to, const char *unit,
                                const GateLeftUnit *l);
#define GATE_LEFT_UNIT_HEADER "left the unit (range shrunk by the lane; not a regression):"

/* ---- a placeholder that the lane removed (not a regression) ------------------------------------------------------ */
/* True when the lane's source has no definition of the function while the base revision's source defines it with the
 * body `unit.exe new` generates (shsm_is_placeholder_body): the function was not yet written, and removing its
 * placeholder (an empty body is inlined into its caller) is not a loss. A base body of real code is false. */
bool gate_placeholder_removed(AvpArena *a, const GateDecrease *e, GateSourceSide *lane, GateSourceSide *onto,
                              GateExistsFn exists_lane, void *exists_ctx);
#define GATE_PLACEHOLDER_HEADER "placeholder removed (function not yet written; not a regression):"
