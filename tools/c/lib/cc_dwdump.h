/* The `dtk dwarf dump` text of an object compiled with -sym on, read as variable -> location per function: the
 * parser the solver (Phase 2, our register map) and mwtrace regalloc (the self-check against the -sym on object)
 * share. Pure: nothing here runs dtk or reads files except cc_dwdump_run. */
#pragma once
#include "avp/avp.h"

/* One variable of a listing: ours (dtk dwarf dump of the -sym on object, or the source) or the sibling's DWARF. */
typedef struct {
    const char *name, *type;
    const char *loc;   /* "r31", "f30", "r1+0xC", "fb+0xc", "[r31+0x8]", "reg65535"; NULL for a source local */
    i64 line;          /* declaration line (sibling: DWARF decl_line; source: line in the text); 0 unknown */
    int depth;         /* 0: function scope; 1 and more: inside a block */
    const char *block; /* block key ("802DA390-802DA6B0", or the source block's identity); NULL at function scope */
    i64 index;         /* position in the listing (source: declaration order) */
    bool is_param;
} CcDwVar;
typedef AVP_VEC(CcDwVar) CcDwVars;
/* One function of a dtk dwarf dump. */
typedef struct {
    const char *name; /* as printed ("MemPool::set") */
    i64 lo, hi;       /* the Range line; -1 unknown */
    const char *this_loc;
    CcDwVars params, locals;
} CcDwFn;
typedef AVP_VEC(CcDwFn) CcDwFns;
/* Parses `dtk dwarf dump` text: the functions with their Range, parameters (`name /\* r26 *\/`) and locals
 * (`type name; // r31`). Locals of an inlined helper do not appear (dtk prints what the DWARF lists). */
CcDwFns cc_dtk_dump_parse(AvpArena *a, AvpStr text);
/* The function whose Range starts at lo (lo >= 0), else the one whose printed name ends with name's last
 * component; NULL when none. */
const CcDwFn *cc_dtk_find(const CcDwFns *fns, i64 lo, const char *name);
/* The variable of that name (in listing order), NULL when none. */
const CcDwVar *cc_dw_vars_find(const CcDwVars *v, const char *name);
/* Runs `<dtk> dwarf dump <obj> -o <txt>` from cwd and returns the text; false with *why. */
bool cc_dwdump_run(AvpArena *a, const char *dtk, const char *obj, const char *txt, const char *cwd, AvpStr *text,
                   const char **why);
