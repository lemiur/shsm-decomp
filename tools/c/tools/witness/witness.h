/* witness: the PS2 build as a second-compiler witness (port of tools/witness.py).
 *
 * The model shared by the modules of this tool:
 *   witness_ps2.c   the executable (ISO 9660, SYSTEM.CNF, ELF), the MIPS decoder, function recovery, features,
 *                   data runs: find_exe, load_exe, Elf, decode, format_insn, build_ps2
 *   witness_wii.c   the Wii side from the retail listings and the asm index: build_wii
 *   witness_map.c   Correspondence, validate, _hkey
 *   witness_db.c    the sqlite database: _TABLES, _stamp, build, save, load, lookup
 *   witness_cmd.c   the Witness methods and the subcommands (witness_cmd.h); witness_known.c the known-helper
 *                   site scan (tools/fragments.py known_sites); witness_cli.c the parser
 *   witness_lookup.h the read-only lookup for other tools (stands alone)
 *
 * Python sets are modelled so that their iteration order is the CPython one: sets of int are AvpPyIntSet
 * (CPython set order, given the same insertion sequence); sets of str are AvpSet (insertion order: the
 * Python's str-set order depends on PYTHONHASHSEED, see DEVIATIONS.md D-witness-2 and -4). Dicts keep insertion
 * order. Where the Python iterates a str or tuple set into a decision, witness_map.c iterates it sorted (D-witness-3). */
#ifndef WITNESS_H
#define WITNESS_H

#include "avp/avp.h"
#include "shsm_repo.h"
#include "witness_lookup.h"

#define WT_SCHEMA "1"
#define WT_ADVISORY "advisory evidence from the PS2 build (another compiler, MIPS); never an input to the gate"
#define WT_HOLDOUT_MOD 5
#define WT_MIN_ANCHOR_TEXT 3
#define WT_MAX_RATIO 8.0
#define WT_DEFAULT_GAP 8
#define WT_NTIERS 6
extern const char *const WT_TIERS[WT_NTIERS]; /* string table constant call callees callers */

/* WT_NONE (Python None in an int slot: an unresolved callee, a missing value) is defined in witness_lookup.h. */

/* ---- FnInfo: one function on either side ---- */
typedef struct WtFn {
    i64 addr, size, n, frame, back;
    const char *name, *unit, *lib; /* NULL = None (always None on the PS2 side) */
    AvpI64Vec calls;               /* callee address per call, WT_NONE when unresolved */
    AvpI64Vec sites;               /* call site address per call */
    AvpSet *strs;                  /* set of str */
    AvpPyIntSet *floats, *ints, *offs;
} WtFn;

WtFn *wt_fn_new(i64 addr);

/* {address: FnInfo} in insertion order. */
typedef struct WtSide {
    AVP_VEC(WtFn *) v; /* insertion order: v.v[0..v.n) */
    AvpIMap *idx;      /* (u64)addr -> index into v (AvpVal.i) */
} WtSide;

void wt_side_init(WtSide *s);
WtFn *wt_side_get(const WtSide *s, i64 addr); /* NULL when absent */
void wt_side_put(WtSide *s, WtFn *f);         /* dict assignment: replaces in place when present */

/* ---- data-table runs: [(kind, value)] with kind 'f' (value fn address), 's' (text) or 'o' (None) ---- */
typedef struct WtRunItem {
    char kind;
    i64 fn;           /* kind 'f' */
    const char *text; /* kind 's' (never NULL there) */
} WtRunItem;
typedef AVP_VEC(WtRunItem) WtRun;
typedef AVP_VEC(WtRun) WtRuns;

/* ---- PS2 executable (witness_ps2.c) ---- */
typedef struct WtExe {
    const char *path;   /* exe[0] */
    const char *member; /* exe[1]: NULL (a loose ELF) or "boot" (an ISO image) */
} WtExe;

/* find_exe: raises SHSM_EXC_USAGE when there is none. */
WtExe wt_find_exe(const ShsmRepo *r, const char *exe);
/* load_exe: (label, bytes). */
const char *wt_load_exe(AvpArena *a, WtExe exe, AvpBytes *data);

typedef struct WtSection {
    const char *name;
    i64 addr, offset, size;
    const char *kind; /* "text", "data", "bss", ... as the Python */
} WtSection;

typedef struct WtFacts {
    i64 machine, entry, gp, words;
    AVP_VEC(WtSection) sections;
} WtFacts;

/* build_ps2(Elf(data)): fills fns (dict order = recovered span order), runs and facts. */
void wt_build_ps2(AvpArena *a, const u8 *data, size_t size, WtSide *fns, WtRuns *runs, WtFacts *facts);

/* ps2 subcommand helpers: the decoded listing of one function as text lines (format_insn), see witness_ps2.h. */

/* ---- Wii side (witness_wii.c) ---- */
struct ShsmAsmIndex;
void wt_build_wii(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix, int jobs, WtSide *fns, WtRuns *runs);

/* ---- correspondence (witness_map.c) ---- */
typedef struct WtPair {
    i64 ps2;
    const char *tier;
    const char *ev; /* evidence text */
} WtPair;

typedef struct WtInl {
    i64 wii, helper, parent; /* parent WT_NONE = None */
    const char *ev;
} WtInl;
typedef AVP_VEC(WtInl) WtInls;

typedef struct WtCorr WtCorr; /* the Correspondence; fields in witness_map.h */

/* ---- validation figures (meta["validation"]) ---- */

/* ---- the database (witness_db.c): db_path and lookup, for other tools, in witness_lookup.h ---- */

#endif
