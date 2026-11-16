/* classes_prog.h: the parsed program of classes.exe (tools/classes.py lines 182-923: parse_data_regions, the asm and
 * symbols parsers, the CodeWarrior class-name helpers, analyze_function, Program, the vtable finders and hx).
 *
 * This is the contract `class Analysis` is ported against. Every Python field is visible here with its exact
 * semantics; the conventions:
 *   - Python None for a string is an AvpStr with p == NULL (CLASSES_NONE(s)); for an int, a has_* flag.
 *   - Python ints are i64 (every value the asm can produce fits; see "Python ints" below).
 *   - dicts are insertion-ordered AvpMap / AvpIMap (a duplicate key keeps its first position and takes the last
 *     value, as a dict assignment does); lists are vectors in list order.
 *   - All memory comes from the arena given to the constructor (maps are heap objects never freed).
 *
 * Python ints: parse_int is int() on a stripped string (Unicode whitespace and decimal digits, underscores between
 * digits, 0x after an optional '-'). A value that does not fit in 64 bits wraps modulo 2**64 instead of growing (only
 * masked uses could see such a literal; dtk writes none). */
#ifndef CLASSES_PROG_H
#define CLASSES_PROG_H

#include "avp/avp.h"

#define CLASSES_NONE(s) ((s).p == NULL)

/* ---- splits.txt ----------------------------------------------------------------------------------------------- */
/* parse_data_regions: (start, end, library) of every `.data` split, sorted as Python sorts the tuples. library is the
 * unit's top-level directory (`game`, `climax`, `MetroTRK`, ...). REGION_OF_LIBRARY is {"game": "game",
 * "climax": "climax-core"} (classes_region_of_library: NULL p when absent). */
typedef struct {
    i64 start, end;
    AvpStr library;
} ClassesRegion;
typedef AVP_VEC(ClassesRegion) ClassesRegions;
ClassesRegions classes_parse_data_regions(AvpArena *a, AvpStr text);
AvpStr classes_region_of_library(AvpStr library);

/* ---- parsing helpers ------------------------------------------------------------------------------------------ */
AvpStr classes_unquote(AvpStr s);                                /* "x" -> x (only when both ends are quotes) */
AvpStrVec classes_split_operands(AvpArena *a, AvpStr text);      /* split_operands: commas outside quotes, each stripped;
                                                                    the vector's storage is in `a` */
bool classes_parse_int(AvpStr s, i64 *out);                      /* parse_int; false where Python raises ValueError */

/* ---- functions and data objects -------------------------------------------------------------------------------- */
typedef struct {
    i64 addr;      /* the row's address */
    AvpStr mn;     /* the mnemonic as written (`stwu`, `bne+`, `or.`) */
    AvpStr *ops;   /* split_operands of the rest; nops == 0 when the row has no operand text */
    u32 nops;
} ClassesInsn;
typedef AVP_VEC(ClassesInsn) ClassesInsns;

/* Func: name (unquoted), addr (the first row's address; has_addr false = None for a function without rows), unit
 * (the .s path relative to the asm directory, no extension, '/' separators), section (the last `.section`/`.text`
 * directive before it; None before any), demangled (the last `# ...` comment before the `.fn` when it holds a '(',
 * else None), insns. */
typedef struct {
    AvpStr name;
    bool has_addr;
    i64 addr;
    AvpStr unit, section, demangled;
    ClassesInsns insns;
} ClassesFunc;

/* An item of a .4byte row: ('int', value & 0xFFFFFFFF) or ('sym', name, addend). */
typedef enum { CLASSES_IT_INT, CLASSES_IT_SYM } ClassesItemKind;
typedef struct {
    ClassesItemKind kind;
    AvpStr name; /* sym only (unquoted) */
    i64 v;       /* int: the value; sym: the addend (not masked) */
} ClassesItem;
typedef AVP_VEC(ClassesItem) ClassesItems;

/* Obj: addr/size come from the last location comment (`# .data:0x0 | 0x80001234 | size: 0x10`) seen anywhere earlier
 * in the same file (never reset between objects: a later object without its own comment takes the previous one);
 * has_loc false = (None, None) before any. pure_words: only .4byte rows (and every .4byte operand a number or
 * sym[+addend]). string: the text (still escaped) of the first .string row when no item came before it and no
 * string was taken yet, else None. data: the byte image (relocations as zero words); has_data false = None (a
 * .float/.double row or an undecodable row seen). */
typedef struct {
    AvpStr name;
    bool has_loc;
    i64 addr, size;
    AvpStr unit, section;
    ClassesItems items;
    bool pure_words;
    AvpStr string;
    bool has_data;
    AvpBytes data; /* heap vector */
} ClassesObj;

/* symbols.txt: name -> (section, address, type); type is the first `type:\w+` of the comment, None (p NULL) without. */
typedef struct {
    AvpStr section;
    i64 addr;
    AvpStr type;
} ClassesSymbol;

/* parse_asm_text: adds one .s file's functions and objects to funcs / objs (name -> ClassesFunc* / ClassesObj*, values
 * in .p). An object or function still open at the end of the text is dropped. */
void classes_parse_asm_text(AvpArena *a, AvpStr text, AvpStr unit, AvpMap *funcs, AvpMap *objs);
/* parse_symbols_text: name -> ClassesSymbol* (in .p). */
AvpMap *classes_parse_symbols_text(AvpArena *a, AvpStr text);

/* ---- CodeWarrior mangled names --------------------------------------------------------------------------------- */
/* _parse_qual: `<len><Name>` or `Q<n><len><Name>...` at the start of rest -> true with the class (`A::B`, allocated
 * in a) and the remainder (a view into rest). */
bool classes_parse_qual(AvpArena *a, AvpStr rest, AvpStr *cls, AvpStr *remainder);
AvpStr classes_mangled_class(AvpArena *a, AvpStr name);   /* None (p NULL) when not a member function / __vt__ */
AvpStr classes_vt_symbol_class(AvpArena *a, AvpStr name); /* mangled_class of a `__vt__` name, else None */
AvpStr classes_method_text(const ClassesFunc *f);         /* `fn_X(args)` part of demangled; None without */

/* ---- per-function analysis ------------------------------------------------------------------------------------- */
/* A tracked register value (a Python tuple, or None):
 *   OBJ ("obj", token, off): an object pointer plus off; token "a3" (r3 at entry) or "ret@%08X" (a bl's result,
 *       the call's address, upper-case hex);
 *   SYM ("sym", name, addend): the address name+addend (from lis @ha / addi @l);
 *   IMM ("imm", v): a constant (lis: (v << 16) & 0xFFFFFFFF; li: v as written, not masked; addi: v + imm);
 *   HA  ("ha", name): lis name@ha;   SDA ("sda", name): li name@sda21. */
typedef enum { CLASSES_V_NONE = 0, CLASSES_V_OBJ, CLASSES_V_SYM, CLASSES_V_IMM, CLASSES_V_HA, CLASSES_V_SDA } ClassesValKind;
typedef struct {
    ClassesValKind kind;
    AvpStr name; /* OBJ: the token; SYM/HA/SDA: the symbol */
    i64 v;       /* OBJ: off; SYM: addend; IMM: the value */
} ClassesVal;
bool classes_val_eq(const ClassesVal *x, const ClassesVal *y); /* tuple == (None == None) */

/* calls: (index, addr, target, {reg: value}, tail). args[r - 3] for r3..r10, kind NONE where Python's dict has no
 * key (the dict holds the non-None registers in ascending order). target is unquoted; tail: a `b`. */
typedef struct {
    i64 index, addr;
    AvpStr target;
    ClassesVal args[8];
    bool tail;
} ClassesCall;
/* stores: (index, token, offset, sym, addend): stw/stwu of a SYM value through an OBJ register (any token). */
typedef struct {
    i64 index;
    AvpStr tok;
    i64 off;
    AvpStr sym;
    i64 addend;
} ClassesStore;
/* access dicts {off, width, kind, rw, signed, pointer, tok}: loads/stores through an OBJ register whose token is
 * "a3" or "ret@...". kind "int" / "float" / "double" / "paired"; rw "load" / "store"; signed: -1 None, 0 False,
 * 1 True (set True later by an extsb/extsh of an unsigned 1/2-byte load); pointer: set when a later memory operand
 * uses the loaded word (lwz) as its base. */
typedef struct {
    i64 off;
    int width;
    AvpStr kind, rw;
    int sign;
    bool pointer;
    AvpStr tok;
} ClassesAccess;
typedef struct {
    AvpStr tok;
    i64 size;
} ClassesNew;

/* FuncInfo. nw: token -> size for `__nw__` results with an IMM r3 (insertion order; classes_info_nw lookup).
 * thunk (has_thunk): (adjust, target) for `subi/addi r3, r3, N; b target` (adjust = N for subi, -N for addi,
 * non-zero, target not a .L label). returns_this: at least one blr and r3 == ("obj", "a3", 0) at every one. */
typedef struct {
    AVP_VEC(ClassesCall) calls;
    AVP_VEC(ClassesStore) stores;
    AVP_VEC(ClassesAccess) access;
    AVP_VEC(ClassesNew) nw;
    bool has_thunk;
    i64 thunk_adjust;
    AvpStr thunk_target;
    bool calls_dl;
    bool returns_this;
} ClassesInfo;
const ClassesNew *classes_info_nw(const ClassesInfo *inf, AvpStr tok); /* tok in inf.nw: the entry, else NULL */

/* analyze_function(func, returns_this): returns_this is a set of names (NULL = the empty frozenset); a bl to one of
 * them whose r3 holds an OBJ keeps that OBJ in r3. */
ClassesInfo *classes_analyze_function(AvpArena *a, const ClassesFunc *f, const AvpSet *returns_this);

/* ---- the program ----------------------------------------------------------------------------------------------- */
/* Program: funcs / objs / symbols as parsed (name -> ClassesFunc* / ClassesObj* / ClassesSymbol*, in .p); by_addr:
 * address -> ClassesFunc* of the first function (funcs order) with that address; info: name -> ClassesInfo*, in funcs
 * order, from a first pass with no returns_this, then a second pass (with rt = the names whose info.returns_this)
 * over every function with a `bl` whose first operand, AS WRITTEN (still quoted), is in rt; analyze_function itself
 * compares the unquoted target. */
typedef struct ClassesProgram {
    AvpArena *a;
    AvpMap *funcs, *objs, *symbols, *info;
    AvpIMap *by_addr;
    /* cstring's lazy index (built on first use) */
    ClassesObj **obj_sorted;
    i64 *obj_addrs;
    size_t nsorted;
    bool sorted_built;
    const char *note; /* from_dir: the listings of an earlier split it skipped (one line), NULL when none */
} ClassesProgram;

ClassesProgram *classes_program_new(AvpArena *a, AvpMap *funcs, AvpMap *objs, AvpMap *symbols);
/* Program.from_dir: every *.s under asm_dir in os.walk order (directories as FindFirstFile lists them, files sorted
 * by code point per directory), read as UTF-8 with errors="replace" and universal newlines; symbols_path NULL or
 * missing: no symbols; else read as strict UTF-8 (a decode error raises SHSM_EXC_VALUE). A listing the last split's
 * record (config.json beside asm_dir) does not name is left by an earlier split: it is skipped and counted in `note`
 * (as asmindex does); without a usable record every listing is read. */
ClassesProgram *classes_program_from_dir(AvpArena *a, const char *asm_dir, const char *symbols_path);
/* Program.from_texts: units[i] -> texts[i] in order. Every parsed string is a view into the texts (from_dir keeps the
 * files' text in the arena), so the caller keeps units, texts and symbols_text alive as long as the program. */
ClassesProgram *classes_program_from_texts(AvpArena *a, const AvpStr *units, const AvpStr *texts, size_t n,
                                           AvpStr symbols_text);

static inline ClassesFunc *classes_func(const ClassesProgram *p, AvpStr name) {
    AvpVal *v = avp_map_get(p->funcs, name);
    return v ? (ClassesFunc *)v->p : NULL;
}
static inline ClassesObj *classes_obj(const ClassesProgram *p, AvpStr name) {
    AvpVal *v = avp_map_get(p->objs, name);
    return v ? (ClassesObj *)v->p : NULL;
}
static inline ClassesSymbol *classes_symbol(const ClassesProgram *p, AvpStr name) {
    AvpVal *v = avp_map_get(p->symbols, name);
    return v ? (ClassesSymbol *)v->p : NULL;
}
static inline ClassesInfo *classes_info(const ClassesProgram *p, AvpStr name) {
    AvpVal *v = avp_map_get(p->info, name);
    return v ? (ClassesInfo *)v->p : NULL;
}

/* cstring(sym, addend): the NUL-terminated printable-ASCII text at sym+addend inside a decoded data object (objects
 * ordered by (addr, len(data), name), bisect_right), else None. The base is objs[sym].addr when sym is an object
 * (None when that object has no address: no fallback), else symbols[sym].addr. */
AvpStr classes_cstring(ClassesProgram *p, AvpStr sym, i64 addend);
bool classes_is_func(const ClassesProgram *p, AvpStr name); /* in funcs, or a `type:function` symbol */
/* resolve_thunk: (target, adjust) of a thunk, else (name, 0). */
AvpStr classes_resolve_thunk(const ClassesProgram *p, AvpStr name, i64 *adjust);
bool classes_faddr(const ClassesProgram *p, AvpStr name, i64 *addr); /* false = None */

/* ---- vtables --------------------------------------------------------------------------------------------------- */
/* A vtable's words: NULL p (None) for a zero or header word, else the function symbol. */
typedef AVP_VEC(AvpStr) ClassesWords;
/* {word index: (RTTI symbol, offset)} of the headers that name a type descriptor, ascending index. */
typedef struct {
    i64 k;
    AvpStr sym;
    i64 off;
} ClassesRtti;
typedef AVP_VEC(ClassesRtti) ClassesRttiList;

bool classes_is_rtti_word(const ClassesItem *w);
/* vtable_prefix(prog, words, is_whole, rtti, sized): true with *out (words in `a`) when items starts with a vtable;
 * rtti (may be NULL) receives the headers found before the cut. */
bool classes_vtable_prefix(AvpArena *a, const ClassesProgram *p, const ClassesItem *items, size_t n, bool is_whole,
                           ClassesRttiList *rtti, bool sized, ClassesWords *out);
/* find_vtables: name -> ClassesWords* (in .p) for every object in a data section (.data .rodata .sdata .sdata2) that
 * is or starts with a vtable (sized = the name starts with __vt__), objs order. rtti (may be NULL): name ->
 * ClassesRttiList* for the tables with named headers. */
AvpMap *classes_find_vtables(AvpArena *a, const ClassesProgram *p, AvpMap *rtti);

/* find_embedded_vtables(prog, known, rtti): {(sym, X): words}, in insertion order; known is the map find_vtables
 * gave (only its keys are read). */
typedef struct {
    AvpStr sym;
    i64 base;
    ClassesWords words;
    bool has_rtti; /* (sym, X) in the rtti dict */
    ClassesRttiList rtti;
} ClassesEmbedded;
typedef AVP_VEC(ClassesEmbedded) ClassesEmbeddedList;
ClassesEmbeddedList classes_find_embedded_vtables(AvpArena *a, const ClassesProgram *p, const AvpMap *known);

/* hx: "0x%X" / "-0x%X" (upper-case hex); Python's hx(None) is None: callers test their has_* flag. */
AvpStr classes_hx(AvpArena *a, i64 v);

#endif
