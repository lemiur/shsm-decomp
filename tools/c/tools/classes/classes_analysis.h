/* classes_analysis.h: `class Analysis` of tools/classes.py (lines 926-1934).
 *
 * Part 1 (lines 926-1427, classes_analysis.c): __init__'s vtable collection, _sites, _groups, complete_off,
 * vt_of_ctor, _bases, _looks_dtor, fit_len, _image_at, _images, _ancestors, _prune_ancestors, _compatible, _overlap,
 * _covered, _is_descendant, _spans, _slots, and (from part 2's range, because _sites needs them) vt_at and vt_addr.
 * Part 2 (lines 1430-1934: _names, _mangled_only, _members_and_fields, _finish, _propagate, _layout, to_json, the
 * stats) fills the section marked PART 2 below.
 *
 * Conventions (as classes_prog.h): Python None for a string is an AvpStr with p == NULL (CLASSES_NONE); for an int a
 * has_* flag; dicts are insertion-ordered; lists are vectors in list order. Every vtable-keyed dict of the Python
 * (vgroups, vgroup_ev, vinstallers, group_conflicts, member_objects, cand, member_named, dtor_funcs, bases, exclusive,
 * slots, rtti) is a field of the vtable's ClassesVtable record (every key of those dicts is a key of self.vt); the
 * record says, field by field, how "the key is absent" is represented. Memory comes from the analysis arena (vectors
 * and maps are heap objects never freed). */
#ifndef CLASSES_ANALYSIS_H
#define CLASSES_ANALYSIS_H

#include "classes_prog.h"

/* ---- vptr store sites (_sites) ------------------------------------------------------------------------------- */
/* A store tuple (idx, off, vt, add) of a site: the vtable store at instruction idx through token+off of vt+add (vt is
 * a key of self.vt: the stored symbol itself, or the vtable vt_at found at sym+addend). */
typedef struct {
    i64 idx, off;
    AvpStr vt;
    i64 add;
} ClassesSt;

/* "final": {off: st}, the LAST store at each offset, keys in first-store order. st is an index into stores (Python
 * compares the tuples by identity: `final[st[1]] is not st`). */
typedef struct {
    i64 off;
    size_t st;
} ClassesFinal;

/* {member vt: lo}: the vtables of the site other than the owner, with the lowest offset each is stored at. */
typedef struct {
    AvpStr vt;
    i64 lo;
} ClassesSiteMember;

typedef enum { CLASSES_K_CTOR, CLASSES_K_DTOR, CLASSES_K_FACTORY, CLASSES_K_ALLOC, CLASSES_K_OTHER } ClassesKind;
#define CLASSES_KIND_BIT(k) (1u << (k))
extern const char *const classes_kind_names[5]; /* "ctor", "dtor", "factory", "alloc", "other" */

/* One dict of site_list: every (function, token) with at least one vtable store, prog.info order, then token order
 * (first store). */
typedef struct {
    AvpStr func, token;
    AVP_VEC(ClassesSt) stores;   /* "stores": every vtable store of the token, instruction order */
    AVP_VEC(ClassesFinal) final; /* "final" */
    AVP_VEC(size_t) over;        /* "over": indices into stores of the overwritten stores of another vtable */
    AvpStrVec vts;               /* "vts": sorted(set(st.vt for final)), code point order */
    ClassesKind kind;            /* "kind": ctor / dtor (token a3), factory (token in info.nw), alloc */
    AvpStr owner;                /* "owner" (None: p NULL); "own" is owner.p != NULL */
    AVP_VEC(ClassesSiteMember) members; /* "members", vts order */
} ClassesSite;

/* ---- vtable groups (_groups) ---------------------------------------------------------------------------------- */
/* vgroups[vt]: {addend: this_off or None}, insertion order: the merged store pairs (first value wins), then 0 -> None
 * when 0 was not stored; or the shape-only split {0: None, 4*i: None...}. */
typedef struct {
    i64 a;
    bool has_o;
    i64 o;
} ClassesGroupEnt;
typedef AVP_VEC(ClassesGroupEnt) ClassesGroup;
/* vgroups[vt].get(a): the entry, NULL when a is not a key (an entry with has_o false is the value None). */
const ClassesGroupEnt *classes_group_get(const ClassesGroup *g, i64 a);

/* vinstallers[vt]: {"ctor": [...], "dtor", "factory", "alloc", "other"} function lists (with repeats), indexed by
 * ClassesKind. */
typedef struct {
    AvpStrVec k[5];
} ClassesInstallers;

typedef struct {
    i64 off;
    AvpStr vt;
} ClassesMemberObj;   /* member_objects[vt] element (offset, member vt) */
typedef struct {
    i64 off;
    AvpStr cls, ev;
} ClassesMemberNamed; /* member_named[vt] element (offset, class, evidence) */

/* cand[vt] element (b, off, evidence): b a vtable, or ("name", X) (b_is_name, b = X); off an int, or ("store-at", o)
 * (off_is_store, off = o). */
typedef struct {
    bool b_is_name;
    AvpStr b;
    bool off_is_store;
    i64 off;
    AvpStr ev;
    /* base rule 1 only (classes_baserule.c; zero under the reference rule): the evidence tier (CLASSES_BR_*) and,
     * when the evidence itself places the base's table, its word in the derived vtable (has_word). */
    int tier;
    bool has_word;
    i64 word;
} ClassesCand;

/* Base rule 1 (scan --base-rule ctor): the evidence tiers, weakest first (classes_baserule.c explains the rule). */
enum { CLASSES_BR_REF = 0, CLASSES_BR_DTOR = 1, CLASSES_BR_VPTR = 2 };
/* A member verdict of base rule 1: the subobject at this+off is a member object, by evidence of the given tier. */
typedef struct {
    i64 off;
    int tier;
    AvpStr ev;
} ClassesMemberVerdict;

/* bases[vt] element, the dict {"base", "offset", "word", "evidence"} (key order as listed). base: a vtable, or the
 * tuple ("name", X) (base_is_name, base = X). offset / word: None when has_* is false. evidence: a list, mutated in
 * place (_prune_ancestors appends to a kept base's list), so a base is shared by pointer (identity matters). */
typedef struct {
    bool base_is_name;
    AvpStr base;
    bool has_offset;
    i64 offset;
    bool has_word;
    i64 word;
    AvpStrVec evidence;
} ClassesBase;
typedef AVP_VEC(ClassesBase *) ClassesBaseList;

/* slots[vt] element, the dict _slots builds. Key order of the Python dict:
 *   header: index, offset, target, status, [rtti, rtti_offset];
 *   other:  index, offset, target, [thunk {adjust, target}], status, [base, base_target, base_index], group.
 * offset is index * 4; target may be None; status "header", "inherited", "override", "unresolved", "new", "null";
 * base_target may be None while has_base. group is "embedded" / "complete" (None p for a header). */
typedef struct {
    i64 index;
    AvpStr target;
    AvpStr status;
    bool has_rtti;
    AvpStr rtti;
    i64 rtti_offset;
    bool has_thunk;
    i64 thunk_adjust;
    AvpStr thunk_target;
    bool has_base;
    AvpStr base, base_target;
    i64 base_index;
    AvpStr group;
} ClassesSlot;

/* _spans element (start, end, base vt or None, uncertain). */
typedef struct {
    i64 start, end;
    AvpStr base;
    bool uncertain;
} ClassesSpan;
typedef AVP_VEC(ClassesSpan) ClassesSpans;

/* One key of self.vt with everything the vtable-keyed dicts hold for it. */
typedef struct ClassesVtable {
    AvpStr name;
    size_t index;          /* position in self.vt */
    ClassesWords *words;   /* self.vt[name] */
    ClassesRttiList *rtti; /* self.rtti[name]; NULL: not a key */
    ClassesGroup group;    /* self.vgroups[name] (every vtable has one) */
    AvpStr group_ev;       /* self.vgroup_ev[name]: "stores" / "stores (mixed site, offsets normalised)" /
                              "shape only (zero-pair split)" */
    bool has_inst;         /* name in self.vinstallers (a defaultdict: part 2's `self.vinstallers[vt]` would add it;
                              it is never iterated, so reading `inst` with has_inst false is the empty lists) */
    ClassesInstallers inst;
    AvpStrVec conflicts;   /* self.group_conflicts[name] (defaultdict(list); empty = absent, never iterated) */
    /* self.member_objects[name]: a set of (offset, vt), here unique in insertion order. A Python set of tuples
     * iterates in hash order: every reader sorts it (sorted(...) at classes.py 1686), so sort before use. */
    AVP_VEC(ClassesMemberObj) member_objects;
    AVP_VEC(ClassesCand) cand; /* self.cand[name] (defaultdict(list)) */
    /* self.member_named[name]: a set of (offset, class, evidence), unique in insertion order; sort before use
     * (classes.py 1690 does). */
    AVP_VEC(ClassesMemberNamed) member_named;
    /* self.dtor_funcs[name]: a Python set of function names, NULL when name is not a key. Here insertion order
     * (the vtable's own dtor sites in site_list order, then the slot destructors in slot order). In Python a set of
     * strings iterates in per-process hash order: _bases only sorts it or tests membership, but part 2's
     * `for f in self.dtor_funcs.get(vt, ())` (classes.py 1588) iterates it, observable only when two members share
     * an address: part 2 must not rely on this order being Python's. The key order of the dict dtor_funcs is
     * ClassesAnalysis.dtor_order. */
    AvpSet *dtor_funcs;
    ClassesBaseList bases; /* self.bases[name] (set for every vtable by _images) */
    i64 exclusive;         /* self.exclusive[name] */
    AVP_VEC(ClassesSlot) slots; /* self.slots[name] */
    AVP_VEC(ClassesMemberVerdict) verdicts; /* base rule 1: member verdicts (empty under the reference rule) */
    /* internal: fn_index id of each word (-1 for None) */
    i32 *word_fi;
} ClassesVtable;

/* self.embedded: {(sym, X): "sym+0xX"}, insertion order (find_embedded_vtables order). */
typedef struct {
    AvpStr sym;
    i64 base;
    AvpStr name;
} ClassesEmbeddedName;
/* self.embedded_by_sym[sym]: [(X, name)], one element of the vector held in the map. */
typedef struct {
    i64 base;
    AvpStr name;
} ClassesEmbeddedAt;
typedef AVP_VEC(ClassesEmbeddedAt) ClassesEmbeddedAtList;

typedef struct {
    i64 addr;
    ClassesVtable *vt;
} ClassesVtStart;

/* ---- PART 2 records (classes_analysis2.c) ---------------------------------------------------------------------- */
/* registry[vt] element (string, sinit, factory, size or None, target). */
typedef struct {
    AvpStr string, sinit, factory;
    bool has_size;
    i64 size;
    AvpStr target;
} ClassesRegEnt;
typedef AVP_VEC(ClassesRegEnt) ClassesRegList;

/* name_candidates element; members element (the dict {function: role}, insertion order). */
typedef struct {
    AvpStr name, source;
} ClassesNameCand;
typedef struct {
    AvpStr name, role;
} ClassesMember;

/* A field record of _members_and_fields / _propagate: uses is the Counter {(width, kind): n} in insertion order;
 * funcs the set of functions (only its size is read); via the set of derived class names (NULL: no "via" key). */
typedef struct {
    int width;
    AvpStr kind;
    i64 n;
} ClassesUse;
typedef struct {
    i64 off;
    AVP_VEC(ClassesUse) uses;
    AvpSet *funcs;
    i64 loads, stores;
    bool sign, pointer;
    AvpSet *via;
} ClassesField;
typedef struct {
    AVP_VEC(ClassesField *) v; /* insertion order */
    AvpIMap *idx;              /* offset -> index into v */
} ClassesFields;

/* cls["bases"] element {class, key, offset, vtable_word, primary, evidence, [size_estimate]}. */
typedef struct {
    AvpStr cls;     /* "class" */
    AvpStr key;     /* None: p NULL */
    struct ClassesRec *rec; /* self.classes[key] (NULL when key is None) */
    bool has_offset;
    i64 offset;
    bool has_word;
    i64 word;
    AvpStrVec evidence;
    bool has_size_est;
    i64 size_est;
} ClassesClsBase;

/* One value of self.classes. The JSON members are emitted in the Python dict's key order by to_json. */
typedef struct ClassesRec {
    size_t index; /* position in self.classes */
    AvpStr key, name, name_source;
    AVP_VEC(ClassesNameCand) cands;
    ClassesVtable *vt; /* vtable_symbol (NULL: None) */
    AVP_VEC(ClassesMember) members;
    AvpMap *member_idx;
    ClassesFields fields; /* "_fields" */
    /* _finish */
    AvpStr unit;
    AvpJson *vtable, *ctors, *dtors, *factories, *other_installers, *registry, *member_objects;
    bool has_vt_address;
    i64 vt_address;
    AVP_VEC(ClassesClsBase) bases;
    bool has_size;
    i64 size;
    AvpStr size_source;
    AvpI64Vec vptrs;
    /* _layout */
    bool done;
    AvpJson *fields_json, *field_conflicts;
    bool has_base_end;
    i64 base_end;
    AvpI64Vec own_vptrs;
    bool has_size_est;
    i64 size_est;
    AvpStrVec derived;
} ClassesRec;

typedef struct ClassesAnalysis {
    AvpArena *a;
    ClassesProgram *prog;            /* self.prog */
    int base_rule;                   /* 0: the reference's rule; 1: scan --base-rule ctor (classes_baserule.c) */
    ClassesRegions data_regions;     /* self.data_regions */
    AvpMap *vt;                      /* self.vt: name -> ClassesVtable* (find_vtables order, then the embedded) */
    ClassesVtable **vts;             /* the same records in self.vt order */
    size_t nvt;
    AvpMap *rtti;                    /* self.rtti: name -> ClassesRttiList* (find_vtables' dict, then the embedded) */
    AVP_VEC(ClassesEmbeddedName) embedded; /* self.embedded */
    AvpMap *embedded_by_sym;         /* self.embedded_by_sym: sym -> ClassesEmbeddedAtList* (first-seen sym order) */
    AVP_VEC(ClassesSite) sites;      /* self.site_list */
    AvpMap *sites_of_func;           /* func -> (u64)(first site index << 32 | count): a function's sites are
                                        contiguous in site_list */
    /* self.ctor_of: {(func, kind): owner} for own ctor / dtor sites, first site wins; split by kind (no reader
     * iterates the dict). */
    AvpMap *ctor_of_ctor, *ctor_of_dtor; /* func -> AvpStr* owner */
    AvpStrVec dtor_order;            /* the keys of the dict dtor_funcs, insertion order */
    /* vt_at's sorted (vt_addr(v), v) list, built on first use */
    ClassesVtStart *vt_sorted;
    size_t nvt_sorted;
    bool vt_sorted_built;

    /* ---- PART 2 -------------------------------------------------------------------------------------------- */
    AVP_VEC(ClassesRec *) recs; /* self.classes, insertion order */
    AvpMap *classes;            /* key -> ClassesRec* */
    AvpMap *factories;          /* self.factories: function -> AvpStr* vtable */
    AvpMap *factory_tokens;     /* self.factory_tokens: function -> AvpSet* */
    AvpMap *registry;           /* self.registry: vtable -> ClassesRegList* */
    AvpMap *key_of_vt, *key_of_name; /* -> ClassesRec* */
} ClassesAnalysis;

/* ---- entry points ---------------------------------------------------------------------------------------------- */
/* Analysis(prog, data_regions) up to and including _slots (regions NULL = []). */
ClassesAnalysis *classes_analysis_part1(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions);
/* PART 2 (declared here, defined by part 2): the rest of __init__ (_names, _mangled_only, _members_and_fields,
 * _finish), and the full constructor = part1 + part2. */
void classes_analysis_part2(ClassesAnalysis *an);
ClassesAnalysis *classes_analysis_new(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions);
/* The same with a base rule: 0 the reference's (the two functions above), 1 the constructor rule. */
ClassesAnalysis *classes_analysis_part1_rule(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions,
                                             int base_rule);
ClassesAnalysis *classes_analysis_new_rule(AvpArena *a, ClassesProgram *prog, const ClassesRegions *regions,
                                           int base_rule);
/* PART 2: Analysis.to_json(inputs) (inputs NULL or empty: no "inputs" member); stats() alone. */
AvpJson *classes_analysis_to_json(ClassesAnalysis *an, AvpJson *inputs);
AvpJson *classes_analysis_stats(ClassesAnalysis *an);
/* _ctype(width, kind, fl) */
const char *classes_field_ctype(int width, AvpStr kind, const ClassesField *fl, AvpArena *a);

/* ---- the phases (called in this order by classes_analysis_part1) ------------------------------------------------ */
void classes_an_sites(ClassesAnalysis *an);
void classes_an_groups(ClassesAnalysis *an);
void classes_an_bases(ClassesAnalysis *an);
void classes_an_images(ClassesAnalysis *an); /* ends with _prune_ancestors */
void classes_an_slots(ClassesAnalysis *an);

/* ---- helpers --------------------------------------------------------------------------------------------------- */
static inline ClassesVtable *classes_an_vt(const ClassesAnalysis *an, AvpStr name) {
    AvpVal *v = avp_map_get(an->vt, name);
    return v ? (ClassesVtable *)v->p : NULL;
}
/* complete_off(vt): vgroups[vt].get(0); false = None. vt must be a key of self.vt (Python raises KeyError). */
bool classes_an_complete_off(const ClassesAnalysis *an, const ClassesVtable *vt, i64 *off);
/* vt_of_ctor(fname, kinds): the owner of the first own site of fname whose kind is in the mask
 * (CLASSES_KIND_BIT(...)); None (p NULL) otherwise. The Python default kinds=("ctor",) is CLASSES_KIND_BIT(CTOR). */
AvpStr classes_an_vt_of_ctor(const ClassesAnalysis *an, AvpStr fname, unsigned kinds);
/* vt_addr(vt): `name+0xX` (X upper-case hex, name not an object) -> vt_addr(name) + X; else the object's address;
 * else the symbol's; false = None. */
bool classes_an_vt_addr(const ClassesAnalysis *an, AvpStr vt, i64 *addr);
/* vt_at(base, add): the vtable holding base+add and the offset into it (bisect_right over (addr, name)); NULL = None. */
ClassesVtable *classes_an_vt_at(ClassesAnalysis *an, i64 base, i64 add, i64 *off);
/* fit_len(bvt): len(words), less trailing None words (keeping 3) for a `__vt__` name. */
size_t classes_an_fit_len(const ClassesVtable *bvt);
bool classes_an_looks_dtor(const ClassesAnalysis *an, AvpStr fname);
/* _is_descendant(a, b): a derives from b (through self.bases, ("name", X) bases skipped); true past depth 32. */
bool classes_an_is_descendant(const ClassesAnalysis *an, AvpStr a, AvpStr b, int depth);
/* _spans(vt): sorted (Python tuple order; a None base compared with a string raises TypeError, as Python does). */
ClassesSpans classes_an_spans(const ClassesAnalysis *an, const ClassesVtable *vt, int depth);
/* ---- base rule 1 (classes_baserule.c) ---------------------------------------------------------------------------- */
/* _bases, ctor-call before the owner's first store: true when the sub-constructor's vptr evidence decided the call
 * (a base candidate or a member verdict was recorded); false: the reference's handling applies. */
bool classes_br_ctor_call(ClassesAnalysis *an, ClassesVtable *r, const ClassesSite *s, const ClassesCall *call);
/* _bases, dtor-call with r3 = this+off in one of r's destructors; ref_bvt the reference's base vtable for the target
 * (None: p NULL). true when the call was decided here. */
bool classes_br_dtor_call(ClassesAnalysis *an, ClassesVtable *r, const ClassesCall *call, AvpStr ref_bvt);
/* _images: whether candidate ci of r (its base offset resolved to off) is admitted against r's other candidates
 * (resolved offsets in offs, valid where ok) and member verdicts. */
bool classes_br_admit(const ClassesVtable *r, size_t ci, const i64 *offs, const bool *ok);
/* _images, after the candidates became r->bases: conflict and thunk evidence on the admitted bases. */
void classes_br_finish(ClassesAnalysis *an, ClassesVtable *r);

/* Python's a // 4 and a % 4 */
static inline i64 classes_floordiv4(i64 v) { return (v - (((v % 4) + 4) % 4)) / 4; }
static inline i64 classes_mod4(i64 v) { return ((v % 4) + 4) % 4; }

#endif
