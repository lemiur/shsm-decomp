/* sibling_foreign: a foreign-architecture sibling (a PS2 MIPS ELF built with gcc, with .symtab and stabs in an
 * ECOFF .mdebug section: the Ghost Rider builds of the Climax lineage), indexed and paired with the retail Wii
 * functions across architectures.
 *
 * index   functions from .symtab (sized FUNCs) and the .mdebug procedure table (statics .symtab lacks), each with
 *         its source file (the procedure's file descriptor) and order in that file, the gnu-v2 demangled name, and
 *         the features witness's PS2 model extracts (strings, float and integer constants, callees, callers).
 * pair    witness's correspondence engine over the two sides (tiers string, table, constant, call, callees,
 *         callers), then `seq`: the unpaired functions between two pairs that sit in one source file, in the same
 *         order and count on both sides. Precision per tier is measured on retail functions that carry real names
 *         (RenderWare from the Gormiti map, libjpeg) against Ghost Rider's names; only tiers at 95% or better on at
 *         least 20 named pairs are default tiers of names and units.
 * types   stabs type definitions printed as declaration drafts (sibling_stabs.c).
 * Evidence only: nothing in the tree is edited. */
#pragma once
#include "sibling.h"
#include "sibling_mdebug.h"

/* ---- gnu-v2 demangling (sibling_gnu.c): NULL when the name is not mangled or not covered ---- */
const char *sib_gnu_demangle(AvpArena *a, const char *mangled);  /* "CZone::AddSpawn(RWS::...::CAssetList *)" */
const char *sib_gnu_qualified(AvpArena *a, const char *mangled); /* "CZone::AddSpawn" */

/* ---- stabs types (sibling_stabs.c) ---- */
int sib_stabs_types(AvpArena *a, SibMdebug *m, const char *elf, const char *name, bool full, ShsmOutput *out);
/* Tests: parse the stab strings of one file and render the named type (NULL when absent); *unknown counts the
 * constructs not understood. */
const char *sib_stabs_render(AvpArena *a, const char *const *stabs, size_t n, const char *name, bool full,
                             i64 *unknown);

/* ---- the foreign function list ---- */
typedef struct {
    i64 address, size;
    const char *name, *demangled; /* demangled NULL for C names and uncovered manglings */
    i32 file;                     /* file descriptor index, -1 when no procedure descriptor names the address */
    i32 group;                    /* file, or -2 - k for the k-th run of functions without a file (no debug info) */
    i64 order;                    /* rank by address within its group */
    bool from_symtab;             /* false: a procedure only .mdebug knows (a static function) */
} SibXFn;
typedef AVP_VEC(SibXFn) SibXFnVec;
/* Functions in address order. m may be NULL (no .mdebug: .symtab only, files unknown). Text sections only. */
SibXFnVec sib_foreign_functions(AvpArena *a, const SibElf *e, const SibMdebug *m);
/* Fill group and order of functions in address order (sib_foreign_functions does it). */
void sib_foreign_group(SibXFn *v, size_t n);

/* ---- seq tier (tested apart) ---- */
typedef struct {
    i64 address, size;
    i64 peer;  /* index into the other side, -1 */
    int tier;  /* 0 unpaired */
    const char *unit; /* retail: split unit or library, "" unknown; never paired across two units */
} SibSeqNode;
/* s: retail functions in address order; g: foreign functions in address order with file and order. Pairs, with
 * tier `tier`, the unpaired runs between consecutive paired retail functions whose peers sit in one file, in
 * order, with as many unpaired foreign functions between them (none paired elsewhere) as retail ones, every
 * pair within a size ratio of 4 (or both under 48 bytes). Returns the count paired. */
/* With tier_edge (0: off), also the runs across one file boundary: the unpaired tail of the first pair's file
 * followed by the unpaired head of the second pair's file, as many as the retail run between them. */
i64 sib_seq_fill(SibSeqNode *s, size_t ns, SibSeqNode *g, const SibXFn *gx, size_t ng, int tier, int tier_edge);

/* ---- the commands ---- */
bool sibling_is_foreign(Sibling *sb);
bool sibling_elf_is_foreign(const char *elf); /* a MIPS ELF */
int sibling_foreign_run(Sibling *sb, AvpStr cmd, AvpArgs *args, ShsmOutput *out);
