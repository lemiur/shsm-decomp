/* decomp: tools/decomp.py (Ghidra decompiler listings of the retail functions: reading aids, never source).
 * Internal interface of the tool library; other tools use decomp_listing.h. */
#pragma once
#include "avp/avp.h"
#include "decomp_listing.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"

#define DC_CONTEXT_VERSION 4
#define DC_GEKKO_LANGUAGE "PowerPC:BE:32:Gekko_Broadway"
#define DC_DEFAULT_LANGUAGE "PowerPC:BE:32:default"

/* ---- store (decomp_store.c) ---- */
/* load_json(path, default): the parsed document, or dflt when the file is missing or not JSON. */
AvpJson *dc_load_json(AvpArena *a, const char *path, AvpJson *dflt);
/* save_json(path, data): json.dump(indent=1, sort_keys=True) + "\n" through path + ".tmp" and os.replace. */
void dc_save_json(const char *path, const AvpJson *data);
/* str(v) of a JSON value as Python prints it ("None", "True", 377.2, the string itself). */
AvpStr dc_pystr(AvpArena *a, const AvpJson *v);
/* merge_results(index, results_tsv, stamp, ghidra_version): (ok, failed) of this export. */
void dc_merge_results(AvpArena *a, AvpJson *index, const char *results_tsv, const AvpJson *stamp,
                      const AvpJson *ghidra_version, i64 *ok, i64 *failed);
/* stale_entries(index, stamp): the keys whose stamp differs, sorted. */
bool dc_entry_has_file(AvpArena *a, const DecompPaths *p, const AvpJson *e);
AvpStrVec dc_stale_entries(AvpArena *a, const AvpJson *index, const char *stamp);
/* e.get(key) != s for a string s (a missing or non-string value differs). */
bool dc_json_ne_str(const AvpJson *obj, const char *key, const char *s);
/* int(text, 16) as Python (0x prefix, sign, underscores, surrounding whitespace); raises ValueError otherwise. */
i64 dc_int16(AvpStr text);
i64 dc_int10(AvpStr text);

/* ---- locating Ghidra (decomp_locate.c) ---- */
typedef struct {
    const char *install, *version, *release, *settings_dir, *language;
    i64 java_min;
} DcInstall;
AvpMap *dc_read_properties(AvpArena *a, const char *path); /* values are char * in .p */
const char *dc_headless_script(AvpArena *a, const char *install);
const char *dc_resolve_install(AvpArena *a, const char *path); /* NULL: None */
/* appdata NULL: $APPDATA, else ~ */
DcInstall dc_install_info(AvpArena *a, const char *install, const char *appdata);
const char *dc_find_language(AvpArena *a, const char *install, const char *settings_dir, const char *language_id);
/* locate_ghidra(arg, env, candidates, appdata). env_dir: $GHIDRA_INSTALL_DIR (NULL/"" unset); candidates NULL: the
 * built-in list. *how is set. Raises SHSM_EXC_USAGE. */
DcInstall dc_locate_ghidra(AvpArena *a, const char *arg, const char *env_dir, const char *const *candidates,
                           size_t ncandidates, const char *appdata, const char **how);
void dc_default_candidates(AvpArena *a, const char *const **out, size_t *n);
/* parse_java_version(text): -1 for None */
i64 dc_parse_java_version(AvpStr text);
i64 dc_java_major(void); /* -1: no java */

/* ---- headless command lines (decomp_headless.c) ---- */
typedef struct {
    const char *script;
    const char *const *args;
    size_t nargs;
} DcScript;
typedef struct {
    const char *import_path, *process, *processor, *cspec, *loader;
    const DcScript *pre;
    size_t npre;
    const DcScript *post;
    size_t npost;
    bool no_analysis, read_only, overwrite;
    const char *script_path; /* NULL: tools/ghidra next to this executable's checkout */
} DcHeadlessOpts;
/* headless_argv(...): the NULL-terminated argv (count in *argc). Raises SHSM_EXC_USAGE for an empty argument or
 * one with '!'. */
const char **dc_headless_argv(AvpArena *a, const char *install, const char *project_dir, const DcHeadlessOpts *o,
                              size_t *argc);
const char *dc_scripts_dir(AvpArena *a);
/* run_headless(paths, argv, label): seconds and the log text; raises SHSM_EXC_USAGE on a locked project or a
 * failed run. A test may replace the runner (dc_headless_hook) or shorten the lock wait. */
typedef double (*DcHeadlessFn)(void *ctx, const DecompPaths *p, const char *const *argv, size_t argc,
                               const char *label, AvpStr *log);
extern DcHeadlessFn dc_headless_hook;
extern void *dc_headless_hook_ctx;
extern double dc_lock_wait_seconds; /* 10 */
double dc_run_headless(AvpArena *a, const DecompPaths *p, const char *const *argv, size_t argc, const char *label,
                       AvpStr *log);
/* elf_symbols(path, wanted) for _SDA_BASE_ / _SDA2_BASE_: has[i] / val[i] in the order of names. */
void dc_elf_symbols(AvpArena *a, const char *path, const char *const *names, size_t n, bool *has, u64 *val);
/* sda_bases(elf_path, table) */
void dc_sda_bases(AvpArena *a, const char *elf_path, const ShsmSymbolTable *t, u64 *sda, u64 *sda2);

/* ---- symbol rows (decomp_rows.c) ---- */
typedef struct {
    const char *kind; /* "F", "O", "L", "R" */
    i64 address, size;
    const char *name, *demangled, *dtype, *this_, *params;
} DcRow;
typedef AVP_VEC(DcRow) DcRows;
typedef struct {
    i64 start, end;
} DcRange;
typedef AVP_VEC(DcRange) DcRanges;

const char *dc_param_spec(AvpArena *a, const char *text); /* NULL: None */
AvpSet *dc_known_classes(AvpArena *a, const ShsmSymbolTable *t);
void dc_signature(AvpArena *a, const char *name, const AvpSet *classes, const char **this_, const char **params);
DcRows dc_symbol_rows(AvpArena *a, const ShsmSymbolTable *t);
AvpStr dc_format_rows(AvpArena *a, const DcRow *rows, size_t n);
DcRows dc_parse_rows(AvpArena *a, AvpStr text);
void dc_write_rows(AvpArena *a, const char *path, const DcRow *rows, size_t n);
DcRows dc_read_rows(AvpArena *a, const char *path);
bool dc_row_eq(const DcRow *x, const DcRow *y);
/* diff_rows(old, new): delta, sorted ranges, changed function addresses */
void dc_diff_rows(AvpArena *a, const DcRows *old, const DcRows *new_, DcRows *delta, DcRanges *ranges,
                  AvpISet **funcs);
/* affected(index, ranges, funcs): sorted keys */
AvpStrVec dc_affected(AvpArena *a, const AvpJson *index, const DcRanges *ranges, const AvpISet *funcs);

/* ---- commands (decomp_cmd.c) ---- */
typedef struct {
    const char *command;
    const char *root;
    const AvpArgs *args;
    /* setup */
    const char *ghidra;
    bool force;
    /* export */
    const char **symbols;
    size_t nsymbols;
    bool every, missing;
    const char *unit, *lib;
    i64 jobs, timeout;
    /* show */
    const char *symbol;
    /* status */
    bool failures;
} DcArgs;
int dc_cmd_setup(const DcArgs *g);
int dc_cmd_export(const DcArgs *g);
int dc_cmd_show(const DcArgs *g);
int dc_cmd_refresh(const DcArgs *g);
int dc_cmd_status(const DcArgs *g);
/* resolve(table, query): raises SHSM_EXC_KEY "no function %r in symbols.txt" */
const ShsmSymbol *dc_resolve(AvpArena *a, const ShsmSymbolTable *t, const char *query);
/* export(paths, state, functions, jobs, timeout, label): ok/failed/seconds */
void dc_export(AvpArena *a, const DecompPaths *p, const AvpJson *state, const ShsmSymbol *const *funcs, size_t n,
               i64 jobs, i64 timeout, const char *label, i64 *ok, i64 *failed, double *seconds);
/* listing(repo, symbol, export=True/False, timeout) for a resolved symbol: raises as the Python. */
DecompListing dc_listing_sym(AvpArena *a, const ShsmRepo *r, const ShsmSymbol *s, bool do_export, i64 timeout);
/* _state(paths, need_project): raises SHSM_EXC_USAGE "no Ghidra project: ..." */
AvpJson *dc_state(AvpArena *a, const DecompPaths *p, bool need_project);

int decomp_main(int argc, char **argv);
