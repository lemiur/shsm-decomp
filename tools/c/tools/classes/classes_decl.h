/* classes decl: a compilable class declaration draft for a class of the retail binary, from its retail vtable, the
 * sibling pairing (sibling.exe pair), the sibling's own __vt__ objects and its DWARF (sibling.exe types / fn).
 *
 * The model functions (cd_*) are pure: they take plain arrays, so the tests drive them with synthetic inputs; the
 * loader (classes_decl_run) gathers the inputs from classes.json, the sibling database and the sibling ELF.
 *
 * Vtable ownership: a retail vtable whose complete-group slots, named by retail mangled symbols or by the pairing,
 * equal the slots of one sibling __vt__ object (same count, or retail's extra trailing words null) is that sibling
 * class's vtable; without such a table the constructor's mangled class names it, else the class of its last named
 * slot. Slot names: the retail symbol when it is a mangled member name that is no placeholder, else the paired
 * sibling function, else the sibling vtable's function at the same index, else the placeholder. Slot kinds follow
 * MWCC's layout: the primary base's slots come first (an override replaces the base's function), every later slot
 * is a new virtual of the class. */
#pragma once
#include "classes.h"

enum { CD_SRC_NONE = 0, CD_SRC_RETAIL = 1, CD_SRC_PAIR = 2, CD_SRC_SIBVT = 3, CD_SRC_DERIVED = 4, CD_NSRC = 5 };
extern const char *const CD_SRC_NAMES[CD_NSRC]; /* "placeholder", "retail", "pairing", "sibling vtable", "derived" */
enum { CD_KIND_INHERITED = 1, CD_KIND_OVERRIDE = 2, CD_KIND_NEW = 3 };

typedef struct {
    u32 offset;          /* the word's byte offset in the vtable */
    const char *retail;  /* the retail symbol; NULL: a null word (or __pure_virtual) */
    u32 address;
    const char *pair;    /* the paired sibling function; NULL: unpaired */
} CdWord;

typedef struct {         /* a sibling vtable (SibVtable without the ELF) */
    const char *symbol, *cls;
    const char **slots;  /* NULL: a 0 word */
    size_t n;
    bool weak;
} CdSibVt;

typedef struct {
    i64 index;           /* the matching candidate, -1: none */
    size_t agree, compared, contradict;
    size_t used;         /* slots of the retail table the match covers (the sibling table's count) */
    const char **tied;   /* other candidates that fit as well (ambiguous match: index -1) */
    size_t ntied;
} CdVtMatch;

/* The sibling vtable equal to the retail words: every compared slot agrees (a null word with a 0 or __pure_virtual
 * slot), at least one named slot agrees, and the retail table is as long as the candidate or longer by null words
 * only. Several fitting candidates: the one with more agreeing slots, then the one named prefer; else ambiguous. */
CdVtMatch cd_match_vtable(AvpArena *a, const CdWord *w, size_t n, const CdSibVt *c, size_t nc, const char *prefer);
/* Where a retail data object holding two adjacent tables splits: the index k of a pair of null words (the second
 * table's offset/RTTI header) such that w[0..k) equals a sibling table of exactly k slots and a function word
 * follows the pair; 0 when the object is one table (trailing nulls are padding). */
size_t cd_split_point(AvpArena *a, const CdWord *w, size_t n, const CdSibVt *c, size_t nc);
/* The distinct qualified names among names whose last component is the unqualified q (EventI -> FMOD::EventI). */
AvpStrVec cd_short_matches(AvpArena *a, const char *q, const char *const *names, size_t n);
/* MWCC's DWARF drops const from the parameters of function pointer types (FMOD_RESULT (*)(char *, ...) for
 * FMOD_FILE_OPENCALLBACK's const char *name); a retail mangled symbol taking the same callback type is the authority.
 * cd_scan_callbacks records each "(*)(list)" of a demangled name in callbacks (key: the list without const and
 * spaces; value: its spelling, NULL when two spellings disagree); cd_callback_consts returns decl with the first
 * function pointer's parameter list respelled from it, NULL when nothing changes. */
void cd_scan_callbacks(AvpArena *a, AvpMap *callbacks, const char *demangled);
const char *cd_callback_consts(AvpArena *a, AvpMap *callbacks, const char *decl);

typedef struct {
    const char *name;    /* the mangled name the declaration uses; NULL: unknown (placeholder) */
    int src;             /* CD_SRC_* */
    bool pure;
    const char *other;   /* a disagreeing name from another source (pairing vs sibling vtable) */
} CdName;
/* Name each word (sib: the matched sibling vtable or NULL). */
void cd_name_slots(AvpArena *a, const CdWord *w, size_t n, const CdSibVt *sib, CdName *out);
/* The qualified class of a mangled member name ("FMOD::DSPI"); NULL when it is not one. */
const char *cd_scope_of(AvpArena *a, const char *mangled);
/* The base slot count implied by names: one past the last slot named for one of ancestors (qualified names). */
size_t cd_infer_base_count(AvpArena *a, const CdName *names, size_t n, const char *const *ancestors, size_t nanc);
/* kind[i] for slot i of cls: below nbase inherited or override (base: the base's names, NULL when unknown: then a
 * slot named for cls is an override), from nbase on new. */
void cd_classify(AvpArena *a, const char *cls, const CdName *names, size_t n, size_t nbase, const CdName *base,
                 size_t nbase_names, int *kind);

typedef struct {
    const char *ret;     /* the return type (DWARF); NULL: unconfirmed */
    const char **pnames; /* parameter names (DWARF, `this` dropped); NULL or npn != parameter count: unnamed */
    size_t npn;
    bool weak;           /* the sibling function is weak: compiled from an inline definition (in the class) */
} CdSig;
/* One virtual declaration: "virtual FMOD_RESULT release( bool freethis )" (no ';'). ns: the namespace the class
 * lives in ("FMOD", "" for none), stripped from parameter types; placeholder: the name for an unknown slot;
 * fallback_ret: the return type an unconfirmed one is drafted as. */
const char *cd_method_decl(AvpArena *a, const char *cls_short, const char *ns, const char *mangled, const CdSig *sig,
                           bool pure, const char *placeholder, const char *fallback_ret);

typedef struct {
    const char *decl;    /* "float *mBuffer" */
    u32 off, size;
    const char *note;    /* drafted members: where the field comes from (comment); NULL for DWARF members */
} CdMember;
typedef struct {
    const char *name, *access;
    u32 off, size;
    bool is_virtual;
} CdBase;
typedef struct {
    bool found;
    const char *key;     /* class or struct */
    u32 size;
    CdBase *bases;
    size_t nbases;
    CdMember *m;
    size_t nm;
    size_t nlayouts;     /* distinct definitions of the name in the sibling */
} CdLayout;

typedef struct {
    const char *cls;     /* qualified */
    const char *vtable_symbol;
    u32 vtable_address;
    const char *sib_vtable; /* the matched sibling vtable symbol, NULL */
    const char *owner_source;
    const CdWord *w;
    const CdName *names;
    const int *kind;
    const CdSig *sigs;
    size_t n;            /* slots used (the sibling table's count when matched) */
    size_t nbase;
    const char *base_cls; /* qualified primary base, NULL */
    CdLayout layout;
    bool own_vptr;       /* the class introduces the vtable pointer */
    i64 vptr_off;        /* retail own vptr offset, -1 unknown */
    const char *retail_size; /* "0x108 (constructor)" style, NULL */
    AvpStrVec notes;     /* LAYOUT DIFFERS and other notes, printed in the class */
    AvpStrVec fwd;       /* classes to forward-declare */
    AvpStrVec needs;     /* "FMOD_RESULT (include/fmod/fmod.h)" */
    size_t count_src[CD_NSRC];
    CdMember *drafted;   /* no Gormiti layout: the fields retail reads (classes.json), as field_0x... members */
    size_t ndrafted;
    const char *fallback_ret;
    const char **slot_note; /* per slot: where it comes from in the classes.json chain of bases; NULL: no labels */
} CdClass;
/* The declaration with its provenance comment block (lines end "\n"). */
void cd_render(AvpArena *a, const CdClass *c, AvpBuf *out);
/* The members declared before the first virtual (their offsets below the class's own vptr), as an index count. */
size_t cd_members_before_vptr(const CdClass *c);

/* ---- the front end ---- */
typedef struct {
    const char *root;
    const char *classes_json;
    const char *symbols;
    const char *cls;     /* NULL with stats */
    bool with_bases, json, all, force, stats;
    const char *header;  /* --header PATH */
    i64 limit;
} CdArgs;
int classes_decl_run(AvpArena *a, const ClassesDb *db, const CdArgs *args);
