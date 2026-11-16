/* classes.h: classes.json for C tools (tools/classes.py: the reader side, the input stamp and the scan driver).
 *
 * classes.json is what `classes.py scan` (or classes.exe scan) writes into build/R5WEA4/classes.json:
 * {"inputs": stamp, "stats": {...}, "methods": {function: "name(args)"}, "classes": [class, ...]}. This header gives
 * other tools a typed view of it without any CLI concerns (link `LIBS tool_classes`):
 *
 *     ClassesPaths p = classes_default_paths(a, root);
 *     ClassesDb *db = classes_db_load(a, p.out);                  // raises SHSM_EXC_* on a missing/bad file
 *     const char *why = classes_stale_reason(a, db, p.asm_dir, p.symbols);   // NULL: current (classes_staleness)
 *     const ClassesClass *c = classes_find(db, "CHitEffectManager");          // find_class
 *     for (size_t i = 0; i < db->n; i++) { const ClassesClass *x = &db->classes[i]; ... }
 *
 * Every string is NUL-terminated and lives in the arena passed to the loader; a JSON null (or an absent optional
 * member) is NULL. Each class keeps its JSON object (`json`) for callers that need a member this view does not
 * decode, and for `show --json`. Addresses and offsets stay the hex strings classes.json records ("0x803E8B78");
 * classes_hex parses one. Lookups follow classes.py: classes_find is find_class (first match in document order),
 * classes_by_key is the `{x["key"]: x}` index (the last class with that key wins). */
#ifndef CLASSES_H
#define CLASSES_H

#include "avp/avp.h"

/* ---- the document --------------------------------------------------------------------------------------------- */
typedef struct {
    const char **v;
    size_t n;
} ClassesStrs;

typedef struct {               /* ctors, dtors, factories, other_installers */
    const char *name, *address;
} ClassesFn;

typedef struct {               /* members: a function of the class and why (ctor, dtor, factory, slot, mangled) */
    const char *name, *address, *role;
} ClassesMember;

typedef struct {               /* name_candidates */
    const char *name, *source;
} ClassesNameCand;

typedef struct {               /* registry: a class-registry call in a static initialiser */
    const char *string, *sinit, *factory, *size;
} ClassesRegistry;

typedef struct {               /* bases */
    const char *cls;           /* "class" */
    const char *key;           /* the base's class key (may name no class in the file: a base without a vtable) */
    const char *offset;        /* NULL: unknown */
    bool primary;
    bool has_vtable_word;
    i64 vtable_word;
    const char *size_estimate; /* NULL when absent */
    ClassesStrs evidence;
} ClassesBase;

typedef struct {               /* member_objects */
    const char *offset, *cls;
    ClassesStrs evidence;
} ClassesMemberObject;

typedef struct {               /* fields: the draft field layout */
    const char *offset;
    i64 width;
    const char *kind;          /* int, float, double, paired */
    const char *type;          /* u8, s16, void*, f32[2], ... */
    i64 loads, stores, functions;
    const char *inherited_from;      /* NULL: the class's own field */
    ClassesStrs seen_in_derived;
    ClassesStrs conflict;
} ClassesField;

typedef struct {               /* vtable words */
    i64 index;
    const char *offset;
    const char *status;        /* header, new, override, inherited, null, unresolved */
    const char *group;         /* complete, embedded; NULL for a header word */
    const char *target;        /* NULL: a null word */
    const char *target_address;
    const char *base;          /* inherited/override: the base class name */
    bool has_base_target;
    const char *base_target;   /* NULL with has_base_target: the base's slot was null */
    bool has_base_index;
    i64 base_index;
    bool has_thunk;
    i64 thunk_adjust;          /* subtracted from this */
    const char *thunk_target;
    const char *rtti, *rtti_offset;
    AvpJson *json;
} ClassesWord;

typedef struct {
    const char *addend;
    const char *this_offset;   /* NULL: unknown */
} ClassesGroup;

typedef struct {
    const char *symbol, *address, *size, *group_evidence;
    ClassesGroup *groups;
    size_t n_groups;
    ClassesStrs group_conflicts;
    ClassesWord *words;
    size_t n_words;
} ClassesVtable;

typedef struct ClassesClass {
    AvpJson *json;
    const char *key, *name, *name_source, *unit;
    ClassesNameCand *name_candidates;
    size_t n_name_candidates;
    const char *vtable_symbol;
    ClassesVtable *vtable;     /* NULL: no vtable */
    ClassesFn *ctors, *dtors, *factories, *other_installers;
    size_t n_ctors, n_dtors, n_factories, n_other_installers;
    ClassesRegistry *registry;
    size_t n_registry;
    ClassesMember *members;
    size_t n_members;
    ClassesBase *bases;
    size_t n_bases;
    ClassesStrs derived;       /* class keys */
    ClassesStrs vptrs, own_vptrs, field_conflicts;
    ClassesMemberObject *member_objects;
    size_t n_member_objects;
    ClassesField *fields;
    size_t n_fields;
    const char *base_end;      /* NULL when absent or null */
    const char *size;          /* NULL: no exact size (see size_estimate) */
    const char *size_source;
    bool has_size_estimate;
    i64 size_estimate;
} ClassesClass;

typedef struct ClassesDb {
    AvpJson *doc;              /* the whole document */
    AvpJson *inputs;           /* doc["inputs"] (NULL when absent) */
    AvpJson *stats;
    AvpJson *methods;          /* function name -> "name(args)" (NULL when absent) */
    ClassesClass *classes;
    size_t n;
    AvpMap *by_key;            /* key -> index + 1 (last wins) */
} ClassesDb;

/* Decode a parsed document. A member the readers need that is missing or of the wrong type raises SHSM_EXC_KEY /
 * SHSM_EXC_TYPE (the KeyError/TypeError the Python readers would meet). */
ClassesDb *classes_db_from_json(AvpArena *a, AvpJson *doc);
/* json.load(open(path, encoding="utf-8")) then classes_db_from_json. Raises SHSM_EXC_FILE_NOT_FOUND/OS on an
 * unreadable file and SHSM_EXC_VALUE with the JSONDecodeError text on a bad one. */
ClassesDb *classes_db_load(AvpArena *a, const char *path);

/* ---- lookups --------------------------------------------------------------------------------------------------- */
/* find_class(db, q): name, key or vtable symbol; then an address `[0x]XXXXXXXX` of the vtable or of a constructor,
 * destructor or factory (class by class, in that order); then a name candidate. NULL when nothing matches. */
const ClassesClass *classes_find(const ClassesDb *db, const char *q);
const ClassesClass *classes_by_key(const ClassesDb *db, const char *key);       /* {x["key"]: x}[key] */
const ClassesClass *classes_by_name(const ClassesDb *db, const char *name);     /* first class with this name */
const ClassesClass *classes_by_vtable_symbol(const ClassesDb *db, const char *symbol);
/* The class whose vtable, constructor, destructor or factory sits at addr (find_class's address step). */
const ClassesClass *classes_by_address(const ClassesDb *db, u32 addr);
/* db["methods"].get(function): the demangled "name(args)", or NULL. */
const char *classes_method(const ClassesDb *db, const char *function);
/* int(s, 16) of a recorded hex string ("0x2C"); false when s is NULL or not a number. */
bool classes_hex(const char *s, i64 *out);

/* ---- input stamp ----------------------------------------------------------------------------------------------- */
typedef struct {
    const char *asm_dir;       /* --asm */
    const char *symbols;       /* --symbols */
    const char *out;           /* --out (classes.json) */
    const char *splits;        /* --splits */
} ClassesPaths;
/* The defaults of classes.py for a checkout root: build\R5WEA4\asm, config\R5WEA4\symbols.txt,
 * build\R5WEA4\classes.json, config\R5WEA4\splits.txt (os.path.join of the root). */
ClassesPaths classes_default_paths(AvpArena *a, const char *root);

/* input_stamp(asm_dir, symbols): {"asm": {"dir", "files", "bytes", "mtime_ns"}, "symbols": {"path", "bytes",
 * "mtime_ns"}}. An unreadable symbols file raises (SHSM_EXC_FILE_NOT_FOUND/OS, os.stat's text). */
AvpJson *classes_input_stamp(AvpArena *a, const char *asm_dir, const char *symbols);
/* stamp_key(stamp): the five compared values (asm files, bytes, mtime_ns; symbols bytes, mtime_ns), each a JSON
 * value or NULL when absent; false when stamp is not an object (Python None). */
typedef struct {
    const AvpJson *v[5];
} ClassesStampKey;
bool classes_stamp_key(const AvpJson *stamp, ClassesStampKey *out);
bool classes_stamp_key_eq(const ClassesStampKey *x, const ClassesStampKey *y); /* tuple == (Python equality) */
/* The staleness verdict of a recorded input stamp (classes.json's "inputs") against the inputs at asm_dir and
 * symbols: the comparison stale_reason makes, for a caller with its own wording (check_rules.exe's R13 note).
 * Neither the scanner digest nor the base rule recorded in it is compared: the stamp says whether classes.json
 * describes the current inputs, whichever producer and rule wrote it. */
typedef enum {
    CLASSES_STAMP_CURRENT,    /* the five values match */
    CLASSES_STAMP_NONE,       /* inputs is not an object: no stamp recorded */
    CLASSES_STAMP_UNREADABLE, /* the inputs cannot be stat'ed now; error holds the OSError text */
    CLASSES_STAMP_CHANGED,    /* asm_changed / symbols_changed say which part differs */
} ClassesStampVerdict;
typedef struct {
    ClassesStampVerdict verdict;
    bool asm_changed, symbols_changed;
    const char *error;
    ClassesStampKey cur, rec; /* the compared values (cur only for CURRENT and CHANGED) */
} ClassesStaleness;
ClassesStaleness classes_staleness(AvpArena *a, const AvpJson *inputs, const char *asm_dir, const char *symbols);
/* stale_reason(db, asm_dir, symbols): NULL when db's input stamp matches the inputs, else why not (the text after
 * "is stale: " in the readers' warning), from classes_staleness. */
const char *classes_stale_reason(AvpArena *a, const ClassesDb *db, const char *asm_dir, const char *symbols);
/* The same text for a recorded "inputs" value read from classes.json without a ClassesDb (unit_plan's reader). */
const char *classes_stale_reason_inputs(AvpArena *a, const AvpJson *inputs, const char *asm_dir, const char *symbols);

/* ---- scanning (the producer side) ------------------------------------------------------------------------------ */
/* scan --base-rule (DEVIATIONS.md D-classes-2): how a subobject is told to be a base. CTOR, the constructor and
 * destructor evidence of classes_baserule.c, is the default (owner's ruling, 2026-11-13); SLOTS is the
 * slot-overlap rule of tools/classes.py, whose output the C scan reproduces byte for byte. */
enum { CLASSES_BASE_RULE_SLOTS = 0, CLASSES_BASE_RULE_CTOR = 1 };
/* The rule a scan without --base-rule uses: SHSM_CLASSES_BASE_RULE when it is "slots" or "ctor", else CTOR. */
int classes_default_base_rule(AvpArena *a);

typedef struct {
    ClassesPaths paths;
    bool json;                 /* scan --json: print the statistics as JSON */
    bool no_cache;             /* scan --no-cache */
    int base_rule;             /* scan --base-rule: CLASSES_BASE_RULE_* (main.c fills classes_default_base_rule) */
    const char *root;          /* the checkout root, for the "wrote <relpath>" line */
} ClassesScanArgs;
/* check_inputs: raises SHSM_EXC_USAGE with classes.py's InputError text (exit 2). */
void classes_check_inputs(const char *asm_dir, const char *symbols);
/* The C scanner digest (DEVIATIONS.md D-classes-1): sha256 hex of b"classes-c\0" + avp_tools_stamp() + b"\0" +
 * the splits file's bytes (when splits names a file). */
char *classes_scanner_digest(AvpArena *a, const char *splits);
/* content_key(asm_dir, symbols, scanner), with the C prefix b"classes-scan-c1\0". */
char *classes_content_key(AvpArena *a, const char *asm_dir, const char *symbols, const char *scanner);
/* _current(out, stamp, scanner): the document at out when its recorded stamp and scanner match, else NULL. */
AvpJson *classes_current(AvpArena *a, const char *out, const AvpJson *stamp, const char *scanner);
/* run_scan(args, quiet): keep a current classes.json, restore one from the cache, or scan and write it; unless
 * quiet, print the statistics and the "wrote ..." line on stdout. Returns the document. Honours
 * SHSM_CLASSES_CACHE=0 like no_cache. */
AvpJson *classes_run_scan(AvpArena *a, const ClassesScanArgs *args, bool quiet);
/* json.dump(doc, fh, indent=1) in a text-mode file: the bytes classes.json gets (CRLF, ensure_ascii, no final
 * newline). */
AvpStr classes_dump_file_text(AvpArena *a, const AvpJson *doc);

/* ---- readers (show, header, tree) ------------------------------------------------------------------------------
 * Each appends to out the text the Python prints (lines ending "\n"). They return NULL, or, where the Python
 * fails with an exception after printing part of its output, the exception's last traceback line ("KeyError:
 * 'x'", "TypeError: ..."): out then holds what was printed before it. */
const char *classes_show(AvpArena *a, const ClassesDb *db, const ClassesClass *c, AvpBuf *out);     /* show */
const char *classes_show_json(AvpArena *a, const ClassesClass *c, AvpBuf *out);                    /* show --json */
/* render_header(db, c) plus print's newline. */
const char *classes_header(AvpArena *a, const ClassesDb *db, const ClassesClass *c, AvpBuf *out);
/* ---- slot labels (classes_slots.c) -------------------------------------------------------------------------------
 * `<- Base` names the immediate base whose table has the slot, not where the slot comes from. The label walks the
 * chain of bases: introducer = the topmost class whose table has the index, holder = the class whose function the
 * table holds (the first class from the one asked up that is not merely inheriting the word), declare = the name
 * to declare under the project's convention (the introducing slot's fn_<address> plain name with the declaring
 * class's mangling), NULL for an inherited slot. A null entry is pure. */
typedef struct {
    const char *introducer;    /* NULL for no label (a header word) */
    const char *holder;        /* NULL: the table holds a null entry here */
    const char *intro_target;  /* the introducer's function; NULL: pure there */
    const char *declare;
    const char *via_offset;    /* a new slot that a secondary base's table also holds, at this offset (an override) */
    bool pure_intro, pure_here, inherited;
    bool topmost_known;        /* the chain ended at a base whose table classes.json does not have */
    bool mangling_unknown;     /* declare is the plain name only: the table's symbol has no mangled class */
} ClassesSlotLabel;
/* false for a header or unresolved word. */
bool classes_slot_label(AvpArena *a, const ClassesDb *db, const ClassesClass *c, const ClassesWord *w, ClassesSlotLabel *out);
/* "introduced by X (pure); held by Y; declare fn_..." */
const char *classes_slot_label_text(AvpArena *a, const ClassesWord *w, const ClassesSlotLabel *l);
const char *classes_slot_legend(void);                                  /* one line */
const ClassesWord *classes_word_at(const ClassesClass *c, i64 index);   /* the vtable word with this index */

/* tree [<class>] [--all] [--json]: root NULL is every tree. */
const char *classes_tree(AvpArena *a, const ClassesDb *db, const ClassesClass *root, bool all, bool json, AvpBuf *out);
/* _ctype(width, kind, fl): the draft C type of a field access. */
const char *classes_ctype(AvpArena *a, i64 width, const char *kind, bool is_signed, bool pointer);

#endif
