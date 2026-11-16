/* Internal helpers shared by the dolphin tool's files (not API). */
#pragma once
#include "avp/avp.h"
#include "dolphin.h"
#include "shsm_cli.h"
#include "shsm_text.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DJ_SET(a, obj, key, val) avp_json_set((a), (obj), AVP_S(key), (val))
#define DJ_STR(a, cz) avp_json_strz((a), (cz))
#define DJ_INT(a, v) avp_json_int((a), (i64)(v))

/* SymbolLayer */
typedef struct DolphinSyms {
    ShsmSymbolTable *st;
    u64 image_end;
    AvpMap *dm; /* demangled (and its name before "(") -> AvpPtrVec * of symbols; NULL until built */
} DolphinSyms;
DolphinSyms *dolphin_syms(DolphinEnv *env);
const char *dolphin_describe(DolphinEnv *env, u64 addr);
const ShsmSymbol *dolphin_function_at(DolphinEnv *env, u64 addr);

/* ClassLayer (classes.json) */
typedef struct DolphinClasses {
    const char *path;
    AvpJson *doc;
    AvpIMap *by_vt; /* vtable address -> index into dolphin_class_addends / classes */
    AvpPtrVec cls;  /* class objects per by_vt entry */
    AvpU64Vec add;  /* addend per by_vt entry */
} DolphinClasses;
DolphinClasses *dolphin_classes(DolphinEnv *env);
bool dolphin_classes_available(DolphinEnv *env);
AvpJson *dolphin_class_by_vtable(DolphinEnv *env, u64 addr, u64 *addend); /* NULL: (None, None) */
AvpJson *dolphin_class_by_name(DolphinEnv *env, AvpStr name);

/* Dol: the retail main.dol; false when the DOL is missing (env.dol None); *ok false when the range is not there */
bool dolphin_have_dol(DolphinEnv *env);
const u8 *dolphin_dol_read(DolphinEnv *env, u64 address, u64 size); /* NULL: None */

/* values */
bool dolphin_in_ram(u64 v);
const char *dolphin_fmt_value(DolphinEnv *env, u64 v);
const char *dolphin_fmt_float(AvpArena *a, double f); /* _fmt_float */
const char *dolphin_f32(AvpArena *a, u64 bits);       /* _f32 */
const char *dolphin_f64(AvpArena *a, u64 bits);       /* _f64: repr of the double */
const char *dolphin_region(DolphinEnv *env, u64 ptr);
const char *dolphin_hex(AvpArena *a, const u8 *p, size_t n); /* bytes.hex() */
const char *dolphin_json_text(AvpArena *a, AvpStr utf8);     /* json.dumps(str) */
const char *dolphin_latin1_to_utf8(AvpArena *a, const u8 *p, size_t n);
u32 dolphin_be32(const u8 *p);
u16 dolphin_be16(const u8 *p);
const char *dolphin_now(AvpArena *a); /* time.strftime("%Y-%m-%d %H:%M:%S") */
void dolphin_push(AvpStrVec *lines, AvpArena *a, const char *text);

/* Env.ix: the asm index (build/index/asm.sqlite), opened read-only and refreshed only when it does not exist yet;
 * NULL when it cannot be opened (Python's None, e.g. without build/<ver>/asm). */
struct ShsmAsmIndex *dolphin_ix(DolphinEnv *env);

/* mangle.parse(name) reduced to what default_signature reads (dolphin_mangle.c, on shsm_mangle) */
typedef struct {
    bool function;  /* kind == "function" */
    size_t nscope;
    AvpStr *params; /* C spellings */
    size_t nparams;
} DolphinParsed;
bool dolphin_mangle_parse(AvpArena *a, const char *mangled, DolphinParsed *out);
