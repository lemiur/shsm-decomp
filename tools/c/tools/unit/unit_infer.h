/* unit_infer: the analyses of tools/unit.py that `new`, `check` and other tools (draft's promote) call in-process:
 * the .text range rules, the data-section inference, the .ctors entries and the outside references. See API.md. */
#pragma once
#include "unit.h"

#define UNIT_N_DATA_SECTIONS 7
extern const char *const UNIT_DATA_SECTIONS[UNIT_N_DATA_SECTIONS]; /* .rodata .data .bss .sdata .sbss .sdata2 .sbss2 */
extern const char *const UNIT_SPLIT_SECTIONS[10];                  /* .text .ctors .dtors then the data sections */

typedef struct {
    const char *level; /* ERROR, WARN, INFO, OK */
    const char *msg;
} UnitFinding;
typedef AVP_VEC(UnitFinding) UnitFindings;

/* text_range_findings(proj, path, (lo, hi), splits, cfg, exclude_path). */
UnitFindings unit_text_range_findings(UnitProject *p, const char *path, i64 lo, i64 hi, const UnitSplits *splits,
                                      const UnitConfig *cfg, const char *exclude_path);

/* DataResult. */
typedef struct {
    UnitRanges ranges;                          /* section -> (lo, hi), in UNIT_DATA_SECTIONS order */
    AvpStrVec notes[UNIT_N_DATA_SECTIONS];      /* per data section */
    bool has_notes[UNIT_N_DATA_SECTIONS];       /* `sec in res.notes` */
    AvpStrVec warnings;
    UnitBlockVec owned;                         /* blocks inside the ranges */
    AvpStrVec left;                             /* referenced objects that belong to another unit */
    AvpStrVec shared;                           /* sorted names of shared objects not taken */
    bool no_asm;                                /* the .text range has no asm blocks */
} UnitDataResult;

/* infer_data(proj, idx, text_rng, splits, includes, excludes). has_text false: a data-only unit made of the
 * includes. includes/excludes are the --include-data/--exclude-data values in command-line order (a repeated
 * name counts once). Raises UNIT_TOOL_ERROR (unknown or claimed symbols, no includes without .text). */
UnitDataResult *unit_infer_data(UnitProject *p, UnitIndex *x, bool has_text, i64 lo, i64 hi, const UnitSplits *splits,
                                const AvpStrVec *includes, const AvpStrVec *excludes);

/* find_ctors(proj, funcs): true and the .ctors range when the unit's static initialisers have contiguous entries;
 * notes are the lines to print either way. */
bool unit_find_ctors(UnitProject *p, const UnitSymPtrVec *funcs, i64 *lo, i64 *hi, AvpStrVec *notes);

/* unit_xref(idx, ranges): the names of the blocks inside the ranges, and {block name: [outside referrer names]}
 * (insertion order; values are AvpStrVec*). */
void unit_xref(AvpArena *a, const UnitIndex *x, const UnitRanges *ranges, AvpSet **inside, AvpMap **ext_hits);

/* _registration_object: the `this` a static initialiser passes to unk_801A8F6C's constructor, or NULL. */
const char *unit_registration_object(const UnitIndex *x, const UnitSymPtrVec *sinit_funcs);

/* claimed_by: the path of the split whose range in the block's section holds it, or NULL. */
const char *unit_claimed_by(const UnitSplits *splits, const UnitBlock *t);

/* An exception unit.py does not catch (a TypeError in gap_hint for a data-only unit): a traceback, exit 1. */
enum { UNIT_UNCAUGHT = 0x554F };
