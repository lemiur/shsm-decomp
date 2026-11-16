/* names (tools/names.py): demangle, mangle, find symbols, rename one symbol or a class consistently across
 * symbols.txt, sources, headers, docs and config lists, and check names against the built objects and the address
 * comments. Other tools link this header (tool_names) for the rename planner: plan a rename over a checkout, read the
 * planned edits, write them.
 *
 *     NamesCtx *cx = names_ctx_new(repo, NULL);               // symbols.txt of the checkout (splits attached)
 *     NamesPlan *p = names_plan_rename(cx, "fn_80001018", "Tick", false, false);   // raises SHSM_EXC_USAGE
 *     names_compute_edits(p);                                  // file edits, conflicts, notes
 *     if (p->conflicts.n == 0) names_write_plan(p);            // --apply
 *
 * Strings are UTF-8 and allocated from the arena of the context (the thread's long-lived arena). Errors the Python
 * raised are raised with shsm_raise (SHSM_EXC_USAGE for its UsageError, the mangle module's classes otherwise). */
#pragma once
#include "avp/arena.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"

/* ---- names and the plain-name index ---- */

/* names.plain_of(name): the source name ("fn_800067CC"), its scope ("unk_Ramp") and the mangled base ("fn_800067CC",
 * "__ct"); the name itself, no scope, for an unmangled name, a thunk or an @LOCAL@ static. */
typedef struct {
    const char *plain;
    const char **scope;
    size_t nscope;
    const char *raw;
} NamesPlain;

typedef struct NamesCtx {
    AvpArena *a;
    const ShsmRepo *repo;
    const ShsmSymbolTable *st;
    /* internal: plain_of per symbol and {plain: AvpPtrVec * of symbols}, built on first use */
    NamesPlain *plains;
    AvpMap *by_plain;
} NamesCtx;

/* A context over symbols.txt of repo (st NULL: load it, splits attached). */
NamesCtx *names_ctx_new(const ShsmRepo *repo, const ShsmSymbolTable *st);
NamesPlain names_plain_of(AvpArena *a, const char *name);
/* plain_of of a symbol of the context's table (cached). */
const NamesPlain *names_sym_plain(NamesCtx *cx, const ShsmSymbol *s);
/* plain_index(st).get(plain, []): the symbols whose plain name is plain, file order (n 0 when none). */
const AvpPtrVec *names_by_plain(NamesCtx *cx, const char *plain);
/* name in st */
bool names_has(const NamesCtx *cx, const char *name);
/* split_params(text): the parameter list cut at top-level commas (<> and () keep theirs); a trailing blank part is
 * dropped, earlier empty parts kept. */
AvpStrVec names_split_params(AvpArena *a, AvpStr text);
/* clip(line, width): stripped, cut to width code points with "..." */
AvpStr names_clip(AvpArena *a, AvpStr line, size_t width);

/* ---- the rename plan ---- */

typedef struct {
    size_t line;      /* 1-based */
    AvpStr old, new_; /* without the line terminator; old is "" for an inserted line */
} NamesEdit;

typedef struct {
    const char *rel;              /* root-relative, forward slashes */
    AVP_VEC(NamesEdit) edits;     /* in the Python's order (symbols.txt sorted, others in line order) */
    AvpStr text;                  /* the file's new text (what --apply writes) */
} NamesFile;

typedef struct {
    const ShsmSymbol *sym;
    const char *new_name;
} NamesSymRename;

typedef struct {
    const char *kind;     /* "__ct" or "__dt" */
    const char *cls, *qual, *name, *old; /* class, qualified scope, "Cls" / "~Cls", the stub's plain name */
    AvpStrVec decls;      /* member declarations queued for the class body */
} NamesSpecial;

typedef struct NamesPlan {
    NamesCtx *cx;
    AVP_VEC(NamesSymRename) symbols;
    AvpMap *idents;               /* old identifier -> const char * new identifier (insertion order) */
    AVP_VEC(NamesFile *) files;   /* edited files in the Python's dict order */
    AvpStrVec conflicts, notes;
    bool local;
    AvpStrVec site_words;         /* identifiers whose source mentions are listed in the notes, not edited */
    NamesSpecial *special;        /* a stub renamed to a constructor or destructor, else NULL */
    /* --signature: the parameter lists and classes are the human's. */
    bool signature;
    AvpStrVec sig_changes;        /* "<demangled old>  ->  <demangled new>", one per renamed symbol */
    AvpStrVec sig_words;          /* the plain names whose declarations and definitions need a hand edit */
    AvpStrVec sig_classes;        /* the classes (last scope component) the plan touches, to filter the sites */
    bool sig_has_free;            /* a renamed symbol is a free function */
    AvpStrVec hand_sites;         /* "file:line: text" of each such declaration or definition (compute_edits) */
} NamesPlan;

/* The switches of `names.exe rename`. */
typedef struct {
    bool local;        /* --local: the renamed line becomes scope:local */
    bool symbols_only; /* --symbols-only: symbols.txt alone, any name */
    bool signature;    /* --signature: the new mangled name may change the class or the parameter codes */
    bool family;       /* --family: every symbol with the same plain name (an override family) together */
} NamesRenameOpts;

/* plan_rename(repo, st, old, new, local, symbols_only) (compiler-generated names and --symbols-only: symbols.txt only). */
NamesPlan *names_plan_rename(NamesCtx *cx, const char *old, const char *new_, bool local, bool symbols_only);
/* The same with every switch. */
NamesPlan *names_plan_rename_opts(NamesCtx *cx, const char *old, const char *new_, const NamesRenameOpts *opts);
/* The switches of `names.exe attr`. */
typedef struct {
    const char *symbol;   /* a symbol name, or an address "0x8XXXXXXX" (needed when several symbols share a name) */
    const char *scope;    /* "global", "local" or "weak"; NULL: unchanged */
    bool has_size;
    i64 size;             /* the new size in bytes */
    bool has_align;
    i64 align;            /* a power of two */
    bool resplit;         /* --resplit: a size that crosses or falls short of the unit's section end moves that end in splits.txt */
} NamesAttrOpts;

/* The plan of `names.exe attr`: the one symbols.txt line with its scope, size or align changed; shrinking the last
 * symbol of a unit's section also moves that section's end: in splits.txt to the new end. A size that grows into the
 * next symbol or past the unit's range, a shrink that frees a range holding another symbol, a local that would sit twice
 * in one unit and a name several symbols carry are conflicts. The files are planned (no names_compute_edits needed);
 * names_write_plan writes them. Raises SHSM_EXC_USAGE for a missing symbol or a bad value. */
NamesPlan *names_plan_attr(NamesCtx *cx, const NamesAttrOpts *opts);
/* The switches of `names.exe add`: the address and size are the `0x...` strings as typed. */
typedef struct {
    const char *name, *section, *address, *size;
    const char *type;     /* "object" (default) or "function" */
    const char *scope;    /* "global", "local" or "weak"; NULL: no scope token */
    bool has_align;
    i64 align;
    const char *data;     /* "4byte", "string" ...; NULL: no data token */
} NamesAddOpts;
/* The plan of `names.exe add`: one new symbols.txt line, inserted in address order within its section in the form of
 * the neighbouring lines. An address that is already named, a range that overlaps a neighbour, a range outside the
 * section's ranges in splits.txt and a name that exists are conflicts. */
NamesPlan *names_plan_add(NamesCtx *cx, const NamesAddOpts *opts);
/* plan_class_rename(repo, st, old, new). */
NamesPlan *names_plan_class_rename(NamesCtx *cx, const char *old, const char *new_);
/* compute_edits(plan, st): the symbols.txt lines, every whole-identifier mention in src/, include/, docs/*.md,
 * configure.py and config.yml, the member declarations of a constructor/destructor target, and the notes on AGENTS.md
 * and the baseline report. Reads the files. */
void names_compute_edits(NamesPlan *p);
/* write_plan(plan): every edited file replaced through <file>.names-tmp. Raises SHSM_EXC_OS on a write failure. */
void names_write_plan(const NamesPlan *p);
/* Plan.to_dict(): {symbols, identifiers, conflicts, notes, edits}. */
AvpJson *names_plan_to_json(AvpArena *a, const NamesPlan *p);

/* ---- batches: many renames, one pass over the sources ---- */

/* An empty plan to merge planned renames into (what names_plan_rename starts from). */
NamesPlan *names_plan_new(NamesCtx *cx);
/* Append the symbols, identifier mappings, notes and conflicts of src to dst (src has no files yet). */
void names_plan_merge(NamesPlan *dst, const NamesPlan *src);
/* Which of words (whole identifiers) occur in the code under src/ and include/. */
AvpStrVec names_code_mentions(NamesCtx *cx, const AvpStrVec *words);
/* The planned file of rel, or NULL. */
NamesFile *names_plan_file(const NamesPlan *p, const char *rel);

/* ---- check ---- */

/* def_name(line): the function a definition line names ("A::f"), "" for a macro-only line, NULL for others. */
const char *names_def_name(AvpArena *a, AvpStr line);
/* elf_functions(path): defined STT_FUNC symbols (name as latin-1 text, binding); false when the file is not an
 * ELF32 big-endian object or is truncated (the Python's ValueError / struct.error). */
typedef struct {
    const char *name;
    int bind;
} NamesElfFunc;
typedef AVP_VEC(NamesElfFunc) NamesElfFuncs;
bool names_elf_functions(AvpArena *a, const char *path, NamesElfFuncs *out);

/* ---- subcommands (output through out; return the exit code) ---- */
typedef struct {
    const char *cmd;
    AvpStrVec names;                  /* demangle */
    const char *name, *cls, *params;  /* mangle (cls/params NULL when not given) */
    bool is_const;
    const char *text;                 /* find */
    const char *old, *new_;           /* rename */
    bool cls_rename, apply, local, symbols_only, signature, family;
    NamesAttrOpts attr;               /* attr */
    NamesAddOpts add;                 /* add */
} NamesArgs;

int names_cmd_demangle(const NamesArgs *na, ShsmOutput *out);
int names_cmd_mangle(const NamesArgs *na, ShsmOutput *out);
int names_cmd_find(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo);
int names_cmd_rename(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo);
int names_cmd_attr(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo);
int names_cmd_add(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo);
int names_cmd_check(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo);

/* The whole command line (names.py main). */
int names_main(int argc, char **argv);
