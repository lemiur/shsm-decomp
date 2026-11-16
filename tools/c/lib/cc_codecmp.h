/* Function comparison with resolved relocation targets: a port of tools/codecmp.py (its docstring is the spec).
 *
 *     CcProject *p = cc_project_new(root);                     // symbols.txt, main.dol, splits.txt (lazy)
 *     CcContext *ctx = cc_context_new(p, AVP_S("game/PadRumble"));
 *     AvpMap *retail = cc_load_asm(a, asm_path, &err);         // cc_listing.h
 *     CcFnResult *r = cc_compare_function(a, rfn, cfn, ctx, true);
 *     cc_fn_result_exact(r), r->edit, r->targets.n, r->unresolved.n, r->notes.n
 *
 * Row texts and target details are the Python's strings byte for byte. Memory comes from the arena given (results,
 * details) or the project's own arena (project data). A CcProject may be shared by threads (its lazy loads are
 * locked); a CcContext and the CcFn values it compares belong to one thread at a time. */
#pragma once
#include "cc_listing.h"
#include "shsm_dol.h"

/* ---- project data ---------------------------------------------------------------------------------------------- */

typedef struct {
    i64 addr, size;
    AvpStr name;
} CcSpan;

typedef struct {
    AvpStr name; /* section name (".text", ".data", ...) */
    i64 start, end;
} CcSecRange;

typedef struct {
    CcSecRange *v; /* splits.txt order (a repeated section keeps its first position and the last range) */
    size_t n;
} CcUnitSecs;

/* codecmp.Dol: the retail image, read with the shared DOL reader (shsm_dol.h). */
typedef ShsmDol CcDol;

typedef struct CcProject CcProject;

/* Project(root): nothing is read until first used. */
CcProject *cc_project_new(const char *root);
const char *cc_project_root(const CcProject *p);
/* names.get(name): the addresses symbols.txt lists for name (file order), NULL when none. Fatal (avp_fatal) when
 * symbols.txt cannot be read, as the Python raised. */
const AvpI64Vec *cc_project_addresses(CcProject *p, AvpStr name);
/* spans: sorted [(address, size, name)]. */
const CcSpan *cc_project_spans(CcProject *p, size_t *n);
/* dol: the retail image, NULL when orig/R5WEA4/sys/main.dol does not exist. */
const CcDol *cc_project_dol(CcProject *p);
/* unit_sections(unit): {section: (start, end)} of the splits.txt block whose path without extension is the unit
 * ("main/" prefix dropped); empty when none. */
CcUnitSecs cc_project_unit_sections(CcProject *p, AvpStr unit);
/* describe(addr): "name", "name+0xN" or "0x%08X". */
AvpStr cc_project_describe(AvpArena *a, CcProject *p, i64 addr);

/* Dol.read(addr, n): true with *out the n bytes when they lie inside one initialised section. */
static inline bool cc_dol_read(const CcDol *d, i64 addr, i64 n, AvpStr *out) {
    return shsm_dol_read_within(d, addr, n, out);
}
static inline bool cc_dol_in_bss(const CcDol *d, i64 addr) { return shsm_dol_in_bss(d, addr); }

/* parse_symbols / parse_splits on text (exposed for the tests): names -> AvpI64Vec* (val.p), spans sorted;
 * splits path -> CcUnitSecs* (val.p). */
void cc_parse_symbols(AvpArena *a, AvpStr text, AvpMap **names, CcSpan **spans, size_t *nspans);
AvpMap *cc_parse_splits(AvpArena *a, AvpStr text);

/* embedded_address(name): the address a generated name carries (fn_/lbl_/jumptable_/dtor_/sinit_<addr>[__...],
 * gap_N_<addr>_x, @x_<addr>). */
bool cc_embedded_address(AvpStr name, i64 *addr);
/* _COMPILER_LOCAL.match(name): @NNN, @stringBaseN, @x_<addr>, ...x.N, @etb_x, @eti_x. */
bool cc_is_compiler_local(AvpStr name);

/* ---- context ---------------------------------------------------------------------------------------------------- */

typedef enum { CC_MATCH, CC_MISMATCH, CC_UNRESOLVED } CcStatus;
const char *cc_status_name(CcStatus s); /* "match", "mismatch", "unresolved" */

typedef struct CcContext {
    CcProject *project;
    AvpStr unit; /* empty: no unit (no layout decisions) */
    bool secs_loaded;
    CcUnitSecs secs;
} CcContext;

/* Context(unit, project). unit as given to the Python (compare.context passes unit_path(unit)). */
CcContext *cc_context_new(CcProject *p, AvpStr unit);
/* Context.check(rrow, crow, rfn, cfn): (status, detail) of the relocation target of a row pair of equal text. */
CcStatus cc_context_check(AvpArena *a, CcContext *ctx, const CcRow *r, const CcRow *c, const CcFn *rfn,
                          const CcFn *cfn, AvpStr *detail);
/* Context.raw_operand_check: the proof text (true) that a raw retail 16-bit operand is the compiled row's
 * compiler-local literal, else false. */
bool cc_context_raw_operand_check(AvpArena *a, CcContext *ctx, const CcRow *r, const CcRow *c, const CcFn *rfn,
                                  const CcFn *cfn, AvpStr *proof);

/* The Context methods tools/eqcheck.py's relocation resolver calls (exported for the eqcheck port; the comparison
 * itself uses the same code):
 * cc_context_sections        Context.sections: the unit's {section: (start, end)}, splits.txt order (loaded once).
 * cc_context_retail_address  Context.retail_address(ref): true with *addr, else false with *why (why may be NULL).
 * cc_context_compiled_addresses  Context.compiled_addresses(elf, reloc): false for None, else the candidates (in a).
 * cc_context_layout_address  Context.layout_address(elf, sym, addend): false for None.
 * cc_context_data_check      Context.data_check(cfn, rfn, sym, addend, ra) at depth 0 (detail may be NULL). */
const CcUnitSecs *cc_context_sections(CcContext *ctx);
bool cc_context_retail_address(AvpArena *a, CcContext *ctx, const CcRef *ref, i64 *addr, AvpStr *why);
bool cc_context_compiled_addresses(AvpArena *a, CcContext *ctx, const CcElf *elf, const CcReloc *reloc, i64 **cands,
                                   size_t *n);
bool cc_context_layout_address(CcContext *ctx, const CcElf *elf, const CcSym *sym, i64 addend, i64 *out);
CcStatus cc_context_data_check(AvpArena *a, CcContext *ctx, const CcFn *cfn, const CcFn *rfn, const CcSym *sym,
                               i64 addend, i64 ra, AvpStr *detail);

/* ---- comparison ------------------------------------------------------------------------------------------------- */

/* TargetDiff(i, j, retail, compiled, detail, kind): kind "branch" / "target" (mismatches), "stray" / "unresolved"
 * (undecidable), "value" (a row matched by a value proof, FnResult.notes). retail/compiled are the rows' text. */
typedef struct {
    size_t i, j;
    AvpStr retail, compiled, detail;
    const char *kind;
} CcTargetDiff;

typedef struct {
    CcTargetDiff *v;
    size_t n;
} CcTdList;

/* (index, retail norm, compiled norm); a missing side is "". */
typedef struct {
    size_t index;
    AvpStr retail, compiled;
} CcPositional;

typedef struct CcFnResult {
    AvpStr name;              /* the retail function's name */
    size_t rows;              /* max(retail rows, compiled rows) */
    size_t edit;              /* alignment edit count of the normalised rows */
    CcPositional *positional; /* position-by-position differences (cc.py's) */
    size_t npositional;
    CcTdList targets;         /* mismatches, in retail row order */
    CcTdList unresolved;
    CcTdList notes;           /* kind "value": rows matched by a value proof */
    size_t checked_targets;
} CcFnResult;

/* FnResult.exact: edit == 0, no positional difference, no target mismatch. */
bool cc_fn_result_exact(const CcFnResult *r);

/* edit_count(want, got). */
size_t cc_edit_count(const AvpStr *want, size_t nw, const AvpStr *got, size_t ng);
/* compare_function(rfn, cfn, ctx, targets): ctx NULL or targets false skips the target check. Changes cfn's row
 * norms where a value proof equates a raw operand (as the Python). */
CcFnResult *cc_compare_function(AvpArena *a, const CcFn *rfn, CcFn *cfn, CcContext *ctx, bool targets);
/* retail_name_for(cname, retail): the exact name, else the shared fn_<addr> prefix (an exact prefix name, else the
 * first retail name starting with it); empty .p == NULL when none. */
AvpStr cc_retail_name_for(AvpStr cname, const AvpMap *retail);
/* cc_codecmp_object = codecmp.compare_object(retail, compiled, ctx, targets, names): {compiled name: FnResult} (val.p), compiled order.
 * names: compiled name -> AvpStr* retail name overrides (val.p), or NULL. */
AvpMap *cc_codecmp_object(AvpArena *a, const AvpMap *retail, const AvpMap *compiled, CcContext *ctx, bool targets,
                          const AvpMap *names);
