/* neutral: is a header (or source) edit byte-neutral? A port of python tools/neutral.py. Every unit a change can
 * reach is compiled twice, from REV (a `git archive` of include/ and src/ extracted under build/neutral/tree-<sha>/)
 * and from the working tree, and the two objects are compared per function and per data section; a changed symbol
 * is judged against retail (cc_compare for functions, gatecheck's layout check for data sections).
 *
 * Files: neutral.c (the pure helpers: depfiles, argv remapping, cache keys, reach, compiler messages),
 * neutral_obj.c (object models and their comparison), neutral_run.c (git, compiling, judging, the command),
 * neutral_cli.c (the parser). */
#pragma once
#include "avp/avp.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_units.h"

/* ---- pure helpers (neutral.c) ------------------------------------------------------------------------------------ */

/* parse_depfile(path): the paths of an MWCC -MMD file (`target: source \` then one path per line, a space escaped
 * as `\ `). Raises what reading the file raises (shsm_raise_io). */
AvpStrVec neutral_parse_depfile_text(AvpArena *a, AvpStr text);
AvpStrVec neutral_parse_depfile(AvpArena *a, const char *path);

/* norm(p): normcase(abspath(p)) with forward slashes. */
const char *neutral_norm(AvpArena *a, const char *p);

/* remap_argv(argv, root, old_tree, source_rel): the old side's argv (-i dirs under include/ or src/ and the source
 * point into old_tree). A new vector; argv is not changed. */
AvpStrVec neutral_remap_argv(AvpArena *a, const AvpStrVec *argv, const char *root, const char *old_tree,
                             const char *source_rel);
/* key_argv(argv, root, old_tree, outdir): machine-specific path prefixes replaced by <OUT>, <OLD>, <ROOT>. */
AvpStrVec neutral_key_argv(AvpArena *a, const AvpStrVec *argv, const char *root, const char *old_tree,
                           const char *outdir);
/* rel_dep(path, root, old_tree): the root-relative `/` path of a dependency (case kept), else the absolute one. */
const char *neutral_rel_dep(AvpArena *a, const char *path, const char *root, const char *old_tree);

/* include_dirs(unit): the -i arguments of the unit's cflags, `/` separated, trailing `/` removed. */
AvpStrVec neutral_include_dirs(AvpArena *a, const ShsmUnit *u);

/* reach(units, files, deps, root): {unit name: reason} (val.p: const char *) in the units' order, and a note on how
 * they were found. deps NULL: no ninja dependency records (the include-directory fallback); else the AvpMap of
 * gate_parse_ninja_deps (object -> AvpStrVec *). */
AvpMap *neutral_reach(AvpArena *a, ShsmUnit *const *units, size_t nunits, const AvpStr *files, size_t nfiles,
                      const AvpMap *deps, const char *root, const char **note);

/* cli.first_error(output): the first real error of a CodeWarrior run, `file:line: (code) message`; {NULL, 0} when
 * the output holds no error block. */
AvpStr neutral_first_error(AvpArena *a, AvpStr output);

/* safe directory name of a unit path: re.sub(r"[^A-Za-z0-9_.-]", "_", path) */
const char *neutral_safe_name(AvpArena *a, const char *path);

void neutral_prepare_dirs(const char *new_dir, const char *old_dir);

/* ---- object models (neutral_obj.c) -------------------------------------------------------------------------------- */

/* A relocation target: (name, addend) for a global or named local symbol, (section, value + addend) for a
 * compiler-local one, ("?", idx, addend) for an index past the symbol table (has_extra). */
typedef struct {
    const char *name;
    i64 value;
    bool has_extra;
    i64 extra;
} NeutralTarget;
typedef struct {
    i64 off, type;
    NeutralTarget target;
} NeutralReloc;
typedef AVP_VEC(NeutralReloc) NeutralRelocVec;

typedef struct {
    AvpStr body;
    NeutralRelocVec relocs; /* sorted (canonical order) */
} NeutralFunc;
typedef struct {
    i64 size;
    AvpStr bytes;
    NeutralRelocVec relocs; /* sorted */
} NeutralData;
typedef struct {
    AvpMap *functions; /* name -> NeutralFunc * */
    AvpMap *data;      /* section name (#k for repeats) -> NeutralData * */
} NeutralModel;

/* object_model(path): raises what gatecheck's ELF reader raises (its exceptions are not caught). */
NeutralModel *neutral_object_model(AvpArena *a, const char *path);
NeutralModel *neutral_model_new(AvpArena *a);
void neutral_sort_relocs(NeutralRelocVec *v);

typedef struct {
    const char *name, *kind;
} NeutralChange;
typedef AVP_VEC(NeutralChange) NeutralChangeVec;
/* diff_models(old, new): changed functions and changed data sections, each sorted by name. */
void neutral_diff_models(const NeutralModel *old, const NeutralModel *new_, NeutralChangeVec *funcs,
                         NeutralChangeVec *data);

/* ---- the command (neutral_run.c, neutral_cli.c) -------------------------------------------------------------------- */
typedef struct {
    const char *root; /* --root (NULL: this checkout) */
    const char *since;
    AvpStrVec paths;
    int jobs;         /* 0: CPU count */
    bool no_cache;
} NeutralOpts;

int neutral_run(const NeutralOpts *o, const AvpArgs *args);
int neutral_main(int argc, char **argv);
