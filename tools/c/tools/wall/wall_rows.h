/* wall_rows: the row helpers of tools/wall.py (L74-163 and L397-400): the class names and their order, register
 * and immediate abstraction, the callee-saved and helper tests, the compare tests and the control role of a row
 * (row_kind). Rows are CcRow (cc_listing.h).
 *
 * The Python's regular expressions are hand-coded matchers with the same results on the listings' text. Python's
 * \w on a str is Unicode; here a word character is [A-Za-z0-9_] or any byte >= 0x80 (listings and symbol names are
 * ASCII, so the two agree on every real input). */
#pragma once
#include "avp/avp.h"
#include "cc_listing.h"

/* ---- classes (REFERENT ... REGISTER, CLASS_ORDER, ORDER_NOTE) ---- */
/* The same spellings as wall_classify.h (an identical redefinition is allowed). */
#define WALL_REFERENT "referent"
#define WALL_CALL "call set"
#define WALL_CONTROL "control flow"
#define WALL_OTHER "other"
#define WALL_REGISTER "register/schedule"
#define WALL_NCLASSES 5
/* CLASS_ORDER: referent, call set, control flow, other, register/schedule. */
extern const char *const WALL_CLASS_ORDER[WALL_NCLASSES];
/* CLASS_ORDER.index(cls), or -1 when cls is not a class (the Python's `in CLASS_ORDER` test). */
int wall_class_index(AvpStr cls);
/* ORDER_NOTE, the sentence the argument parsers append to their descriptions. */
extern const char WALL_ORDER_NOTE[];

/* ---- row abstraction ---- */
/* abstract_regs(norm): every register (\b(cr|r|f)\d+\b) replaced by its class CR, R or F; r1, r2 and r13 kept.
 * The result is NUL-terminated, in arena a. */
AvpStr wall_abstract_regs(AvpArena *a, AvpStr norm);
/* abstract_values(abstract): every immediate (_NUM: (?<![\w@#])-?(?:0x[0-9a-fA-F]+|\d+)\b) replaced by "#".
 * NUL-terminated, in arena a. */
AvpStr wall_abstract_values(AvpArena *a, AvpStr abstract);
/* regs_of(norm): the allocated registers of a row in text order ("@sda21(r0)" read as "@sda21" first; r1, r2 and
 * r13 left out). Returns the count; *out is an arena array of NUL-terminated copies (a valid pointer when 0). */
size_t wall_regs_of(AvpArena *a, AvpStr norm, AvpStr **out);
/* save_helper(name): "_savegpr" for "_savegpr_24" (_SAVEREST: ^_(?:save|rest)[gf]pr_(\d+)$), else false. *out is a
 * view into name (a prefix, so NOT NUL-terminated). An empty name is the Python's None. */
bool wall_save_helper(AvpStr name, AvpStr *out);
/* callee_saved(reg): reg is exactly r<N> or f<N> with N >= 14. */
bool wall_callee_saved(AvpStr reg);
/* HELPER.match(name): a compiler runtime helper (_savegpr/_restgpr..., __ptmf_*, __construct_array, __dl__FPv,
 * __cvt_*, __div2i ... __dynamic_cast); `$` also accepts one trailing "\n", as Python's does. */
bool wall_is_helper(AvpStr name);
/* op in _COMPARES (cmpw, cmplw, cmpwi, cmplwi, fcmpu, fcmpo, cmp, cmpl, cmpi, cmpli). */
bool wall_is_compare(AvpStr op);
/* {a, b} in _SIGNED_PAIRS: {cmpw, cmplw} or {cmpwi, cmplwi} (so a != b). */
bool wall_is_signed_pair(AvpStr a, AvpStr b);
/* _USES_R1.search(s): s names r1 (\br1\b). */
bool wall_uses_r1(AvpStr s);
/* _R1_SLOT (-?(?:0x[0-9a-fA-F]+|\d+))\(r1\): the first match at or after byte pos (re.search(s, pos); finditer is a
 * loop that continues at *end). *num is group 1 (the offset text, a view into s). False when there is none. */
bool wall_r1_slot_find(AvpStr s, size_t pos, size_t *start, size_t *end, AvpStr *num);

/* ---- control role of a row (COND ... CALLK, _TERMINATORS, row_kind) ---- */
typedef enum {
    WALL_K_NONE = 0, /* the Python's None */
    WALL_K_COND,     /* "cond": local conditional branch */
    WALL_K_GOTO,     /* "goto": local unconditional branch */
    WALL_K_FALL,     /* "fall": a block that falls through (block kinds only, never a row kind) */
    WALL_K_RET,      /* "ret": blr */
    WALL_K_TAIL,     /* "tail": b to a symbol */
    WALL_K_CONDRET,  /* "condret": b<cc>lr */
    WALL_K_CONDTAIL, /* "condtail": conditional branch to a symbol */
    WALL_K_SWITCH,   /* "switch": bctr (and conditional b<cc>ctr) */
    WALL_K_CALL,     /* "call": bl, bctrl, blrl ... (CALLK) */
} WallKind;
/* The kind's Python string ("cond", "goto", ...); NULL for WALL_K_NONE. */
const char *wall_kind_name(WallKind k);
/* k in _TERMINATORS (cond, goto, ret, tail, condret, condtail, switch). */
bool wall_kind_is_terminator(WallKind k);
/* _COND_BRANCH.match(op): b + eq|ne|lt|gt|le|ge|ns|so|nl|ng|un|nu|dnz|dz|dnzt|dnzf|dzt|dzf|c, exactly. */
bool wall_is_cond_branch(AvpStr op);
/* row_kind(row) (wall.py L137-163): the control role of a row. */
WallKind wall_row_kind(const CcRow *row);
/* is_self_call(row) (wall.py L397-400): a self-recursive call, which codecmp makes a local `bl` to row 0. */
bool wall_is_self_call(const CcRow *row);
