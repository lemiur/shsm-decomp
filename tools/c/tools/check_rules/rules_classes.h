/* rules_classes: the C++ class model and the classes.json reader of tools/rules/check_rules.py (L1366-1853):
 * demangle_member, mangled_params, _demangle_parts, SrcMethod, _param_types, SrcClass, _member_function,
 * _class_members, parse_cpp_classes (Parsed.cpp_classes), load_classes, input_stamp, _stamp_key, classes_stale,
 * template_name, RetailIndex, slot_implementation, retail_complete_slots. rules_check.md documents the API.
 *
 * A Python None string is an AvpStr with p == NULL. classes.json records are AvpJson objects (identity = pointer). */
#pragma once
#include "rules_parse.h"

/* ---- mangled names ---- */
/* _demangle_parts(sym): true with (cls, name, rest) for a member symbol; false with *name = the symbol after a
 * thunk prefix (Python's (None, sym, None)). */
bool rl_demangle_parts(AvpArena *a, AvpStr sym, AvpStr *cls, AvpStr *name, AvpStr *rest);
/* demangle_member(sym): cls (None when the symbol is not a member) and name (None for a None or empty sym). */
void rl_demangle_member(AvpArena *a, AvpStr sym, AvpStr *cls, AvpStr *name);
/* mangled_params(sym), None when the symbol does not demangle to a member with a parameter list. */
AvpStr rl_mangled_params(AvpArena *a, AvpStr sym);
/* template_name(name), None when the name has no trailing argument list. */
AvpStr rl_template_name(AvpStr name);

/* ---- source classes ---- */
typedef struct {
    const AvpStr *params; /* normalised parameter types */
    u32 np;
    bool const_;
} RlSig;
bool rl_sig_eq(const RlSig *x, const RlSig *y);

typedef struct {
    AvpStr name; /* "~" for the destructor */
    i32 line;
    bool virtual_, pure, static_, ctor;
    RlSig sig;
} RlSrcMethod;

typedef struct {
    AvpStr name; /* qualified with enclosing namespaces and classes */
    AvpStr short_, path;
    i32 line;
    RL_VEC(AvpStr) bases;
    bool virtual_bases, template_;
    RL_VEC(RlSrcMethod) methods;
} RlSrcClass;
typedef RL_VEC(RlSrcClass) RlSrcClasses;

/* P.cpp_classes(): parse_cpp_classes(P), memoized on P (in P's arena). */
const RlSrcClasses *rl_cpp_classes(RlParsed *P);
/* SrcClass.display(m) */
AvpStr rl_class_display(AvpArena *a, const RlSrcClass *c, AvpStr m);

/* ---- classes.json ---- */
/* load_classes(path): the parsed document, or NULL and *note (the reason the R13 slot check is skipped). */
AvpJson *rl_load_classes(AvpArena *a, const char *path, AvpStr *note);

typedef struct RlRetailIndex RlRetailIndex;
/* RetailIndex(data). Returns NULL and sets *error (a Python exception class name) when the Python raises (a
 * malformed address). */
RlRetailIndex *rl_retail_index(AvpArena *a, const AvpJson *data, const char **error);
/* RetailIndex.find(sc): the record, or NULL with *how the reason; *how is the way it was found otherwise. */
const AvpJson *rl_retail_find(RlRetailIndex *ri, const RlSrcClass *sc, AvpStr *how);
/* RetailIndex.instances(sc): the records in name order (*count; arena array). */
const AvpJson **rl_retail_instances(RlRetailIndex *ri, const RlSrcClass *sc, size_t *count);

/* slot_implementation(w): None as {NULL, 0}. */
AvpStr rl_slot_implementation(AvpArena *a, const AvpJson *w);

enum { RL_SLOT_NAMED, RL_SLOT_UNNAMED, RL_SLOT_NULL, RL_SLOT_UNRESOLVED };
typedef struct {
    const AvpJson *index, *offset; /* the word's values (NULL: absent) */
    AvpStr target, cls, name, params; /* None as p == NULL */
    int kind;
} RlRetailSlot;
/* retail_complete_slots(rc) (*count; arena array). */
RlRetailSlot *rl_retail_complete_slots(AvpArena *a, const AvpJson *rc, size_t *count);

/* Python str() of a JSON value as `"%s" % v` prints it (None, str, int, bool, float; containers as repr). */
AvpStr rl_py_str(AvpArena *a, const AvpJson *v);
/* JSON string value or None (NULL, null or a non-string). */
AvpStr rl_json_strv(const AvpJson *v);
/* int(s, 16): false when Python raises (ValueError for a malformed string, TypeError for a non-string). */
bool rl_int16(const AvpJson *v, i64 *out, const char **error);
