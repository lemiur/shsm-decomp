/* sibling: pair the retail functions with the functions of a sibling binary that keeps its symbols (Gormiti, Wii,
 * Climax 2010: an ELF with .symtab and a CodeWarrior link map), and report names, objects and libraries for them.
 * Evidence only: it never edits source or symbols.txt, and it is never gate input.
 *
 * Both sides are dtk asm trees fingerprinted with twins' normalisation (exact: relocation targets blanked to their
 * kind; ref: targets kept). The sibling tree is written by dtk itself (elf config, elf2dol, dol split) from the ELF.
 * Strings live in the arena given. */
#pragma once
#include "avp/arena.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"

#include <sqlite3.h>

#define SIBLING_SCHEMA "1"
#define SIBLING_DEFAULT_NAME "gormiti"
#define SIBLING_SMALL_ROWS 5 /* bodies under this many instructions pair only through sequence context */

/* ---- tiers ---- */
enum { SIB_NONE = 0, SIB_EXACT = 1, SIB_EXACT_SEQ = 2, SIB_CALLS = 3, SIB_SEQ_SIZE = 4, SIB_NTIERS = 5 };
extern const char *const SIBLING_TIERS[SIB_NTIERS]; /* "", "exact", "exact-seq", "calls", "seq-size" */
int sibling_tier_index(const char *name);           /* 0 when unknown */

/* ---- the link map ---- */
typedef struct {
    i64 address, size; /* address SHSM_NONE-like 0 for UNUSED entries */
    const char *name, *lib, *object; /* lib "" when the object is not from a library */
    i64 order;                      /* position of the symbol within its object (0-based, .text lines only) */
    i64 before; /* UNUSED entries: the order of the object's next .text symbol (the used entries listed before it) */
} SibMapSym;
typedef AVP_VEC(SibMapSym) SibMapSymVec;
typedef struct {
    SibMapSymVec text;   /* .text section layout symbols with a size (section and .dwarf lines dropped) */
    SibMapSymVec unused; /* UNUSED lines of .text (dead-stripped by the linker) */
} SibMap;
/* Parse a CodeWarrior map's ".text section layout" (and nothing else). */
SibMap *sibling_parse_map(AvpArena *a, AvpStr text);
/* Split "<name> \t<lib.a> <object>" style tails: *lib "" when the first word does not end in ".a". */
void sibling_split_owner(AvpArena *a, AvpStr tail, const char **lib, const char **object);

/* A data symbol of the map (any section but .text, .init and the debug ones). */
typedef struct {
    const char *section, *name, *lib, *object; /* lib "" when the object is not from a library */
    i64 address, size, align;
} SibMapData;
/* The first symbol of that exact (mangled) name, or (name NULL) the one whose range holds address. */
bool sibling_map_find(AvpArena *a, AvpStr map, const char *name, i64 address, SibMapData *out);

/* ---- dtk input preparation (pure text transforms, tested) ---- */
/* splits.txt from `dtk elf config` -> a splits file dol split accepts: the Sections: block kept, only .text ranges,
 * empty ranges dropped, ends clamped to text_end, unit names without drive colons or backslashes, and object names
 * that would collide in dtk's obj/ directory renamed (<stem>_<n>.<ext>). */
AvpStr sibling_fix_splits(AvpArena *a, AvpStr text, u64 text_end);
/* symbols.txt from `dtk elf config` -> functions of .text/.init, data objects (labels dropped), local data names made
 * unique with a "_a<ADDR>" suffix. */
AvpStr sibling_fix_symbols(AvpArena *a, AvpStr text);
/* The end address of the ELF's .text section (big-endian ELF32); 0 when the file is not such an ELF. */
u64 sibling_elf_text_end(AvpStr elf);

/* ---- the pairing model ---- */
typedef struct {
    i64 address, size, nrows;
    const char *name;
    const char *lib, *object; /* sibling: map library and object; SHSM: libraries.tsv library and asm unit */
    i64 order;
    char exact[21], ref[21];
    const char **callees; /* ordered call/tail-call targets (rel24 to another function) */
    size_t ncallees;
    bool initialiser; /* a static initialiser (__sinit..., a .ctors entry): never paired, its body differs in data only */
    i64 peer; /* index into the other side, -1 */
    int tier;
} SibFn;
typedef AVP_VEC(SibFn) SibFnVec;

typedef struct {
    i64 exact, exact_seq, calls, seq_size, rounds, rejected; /* rejected: pairs undone because their callees contradict them */
} SibPairCounts;
/* Pair s (SHSM) with g (sibling); both in address order. Fills peer and tier on both sides. */
void sibling_pair(SibFnVec *s, SibFnVec *g, SibPairCounts *counts);

/* ---- the database ---- */
typedef struct {
    const ShsmRepo *repo;
    const char *name;     /* sibling name (gormiti) */
    const char *dir;      /* build/sibling/<name> */
    const char *db_path;  /* build/index/sibling/<name>/sibling.sqlite */
    sqlite3 *db;
} Sibling;
Sibling *sibling_open(const ShsmRepo *r, const char *name);
void sibling_close(Sibling *sb);
bool sibling_has_index(Sibling *sb);
bool sibling_has_pairs(Sibling *sb);
/* index: run dtk on the ELF, parse the map and the asm tree, store the sibling functions. Returns the count. */
i64 sibling_index(Sibling *sb, const char *elf, const char *map, int jobs, ShsmOutput *out);
/* Load a side: the sibling functions, or (shsm) the retail functions with their stored pairing. */
SibFnVec sibling_load_sibling(AvpArena *a, Sibling *sb);
SibFnVec sibling_load_shsm_asm(AvpArena *a, const ShsmRepo *r, const char *asm_dir, int jobs); /* fresh from asm */
SibFnVec sibling_load_shsm(AvpArena *a, Sibling *sb); /* from the pairs table */
void sibling_store_pairs(Sibling *sb, const SibFnVec *s, const SibFnVec *g);
/* UNUSED members of an object (lib may be ""). */
AvpStrVec sibling_unused_of(AvpArena *a, Sibling *sb, const char *lib, const char *object);
/* Every UNUSED member, in map order (before -1 when the database predates its position). */
SibMapSymVec sibling_unused_all(AvpArena *a, Sibling *sb);

/* ---- proposed unit ranges (units, stats; sibling_units.c) ---- */
typedef struct {
    const char *lib, *object; /* the sibling object */
    size_t first, last;       /* retail indices of its first and last paired member (the span) */
    bool clean;               /* ordered, nothing of another object inside, no overlap */
} SibSpanRef;
typedef struct {
    i64 address, size;
    const char *name;
    bool mine;       /* in the proposed range of this object */
    bool assigned;   /* the evidence decides its owner; false: ambiguous or a candidate */
    bool candidate;  /* only size or map capacity points to the object: left out of the proposed range */
    const char *why; /* the evidence, or why it is ambiguous */
} SibGapFn;
typedef AVP_VEC(SibGapFn) SibGapFnVec;
typedef struct {
    i64 start, end;           /* the proposed .text range (the span when nothing is added) */
    i64 pad_end;              /* end, or the next function's start when only alignment padding lies between */
    bool proposed;            /* false: the span is not clean, nothing proposed */
    SibGapFnVec before, after; /* gap functions taken (mine) and ambiguous ones left, address order */
} SibProposal;
/* Extend each clean span over the unpaired retail functions between it and its neighbours (spans sorted by start),
 * deciding each gap function's owner from the sibling map (the objects' unpaired and UNUSED entries: count, order,
 * size, name), lower-tier pairs, callers, retail libraries and (splits non-NULL) the split units. */
SibProposal *sibling_propose_units(AvpArena *a, const SibFnVec *s, const SibFnVec *g, const SibMapSymVec *unused,
                                   const ShsmSplits *splits, const SibSpanRef *spans, size_t n, unsigned tiers);

/* ---- names and placeholders ---- */
bool sibling_is_placeholder(const char *name);
bool sibling_is_initialiser(const char *name); /* a static initialiser: never paired */
/* "same", "same-name" (same qualified name, other signature) or "differ". */
const char *sibling_name_agreement(AvpArena *a, const char *shsm, const char *sib);

/* ---- subcommands ---- */
typedef struct {
    const char *lib;     /* NULL: every library */
    unsigned tiers;      /* bit per tier */
    const char *target;  /* of */
    bool all;
    bool no_splits;      /* units: leave the split units out of the proposed ranges' evidence */
    i64 limit;
} SibArgs;
int sibling_cmd_pair_report(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibPairCounts *c,
                            const SibArgs *args, ShsmOutput *out);
int sibling_cmd_of(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out);
int sibling_cmd_names(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out);
int sibling_cmd_units(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out);
int sibling_cmd_stats(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args, ShsmOutput *out);

int sibling_main(int argc, char **argv);

/* ---- apply (renames in bulk) ---- */
typedef struct {
    SibArgs base;   /* tiers; lib unused */
    AvpStrVec libs; /* retail libraries (libraries.tsv); none: every library */
    bool dry_run;
    bool all;       /* every entry of each class, not the first SIBLING_APPLY_EXAMPLES */
} SibApplyArgs;
#define SIBLING_APPLY_EXAMPLES 10
/* Per retail function: true when it belongs to an object span that is clean (ordered, no other object inside, no
 * overlap), the criterion of `units`. s->n + 1 entries. */
bool *sibling_clean_flags(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args);
/* Rename the placeholder retail functions whose pair lies in a clean span to the sibling name, in symbols.txt and
 * everywhere names.exe rename would; the classes of the rest are listed. Refuses a dirty tree unless dry_run. */
int sibling_cmd_apply(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibApplyArgs *args, ShsmOutput *out);
/* "@36@__dt__Q25Audio11MaterialMapFv" (the compiler's this-adjusting thunk): what it is a thunk to; NULL otherwise. */
const char *sibling_thunk_note(AvpArena *a, const char *name);
/* A constructor or destructor by its CodeWarrior name (__ct__/__dt__) whose class the mangled name gives. */
bool sibling_is_ctor_dtor(AvpArena *a, const char *sibling_name);

/* The sibling functions with the retail function's own mangled name (none for a placeholder name): the first, -1 when
 * none; *count (may be NULL) is how many. A same-named function whose body differs is another revision's copy. */
i64 sibling_same_named(const SibFnVec *g, const char *name, size_t *count);
/* The `of` / `brief` line for a retail function without a pair when such a function exists; NULL otherwise. */
const char *sibling_same_named_note(AvpArena *a, const SibFn *f, const SibFnVec *g);

/* The index of the function of v that text names: a name, a demangled name or, by_address (retail side), an address
 * (0x8032C1E0, 8032C1E0, fn_8032C1E0); -1 when none. */
i64 sibling_resolve(AvpArena *a, const SibFnVec *v, const char *text, bool by_address);
/* fn_<8 hex digits>, with or without a mangled suffix: a name the project has not replaced. */
bool sibling_is_fn_placeholder(const char *name);
