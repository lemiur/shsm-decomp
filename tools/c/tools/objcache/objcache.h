/* objcache: the shared object cache in front of every MWCC compile (tools/objcache.py, ccache's "direct mode").
 *
 * The Python module's docstring is the specification and every soundness rule of it is kept here: the manifest key
 * (the compiler and the DLLs beside it, each wrapper in front of it, the argument list with the -o value replaced by
 * a placeholder, the MW* environment, the source path as given and its bytes), the dependency sets (each header's
 * sha256 plus the include probes that make a shadowing header a miss), the rule that only an exit code 0 whose
 * outputs parse and whose inputs were not modified during the compile is stored, the dependency file re-rendered for
 * the current output and working directory (MWCC's "\ " escaped spaces), the hash memo with its racy-timestamp rule,
 * and the stats log appended under a lock on its first byte.
 *
 * Own entry format (DEVIATIONS.md D-objcache-1): the Python entries hold marshal data, which C does not read. The C
 * entries live in the same cache directory (<root>/objcache, the shsm_cache stamp format, so prune and clear handle
 * both) under their own key namespace (FORMAT "objcache-c1") and their own magics (OC_RESULT_MAGIC,
 * OC_MANIFEST_MAGIC), so neither tool ever reads the other's entries; the C hash memo is its own file. */
#pragma once
#include "avp/avp.h"

#define OC_FORMAT "objcache-c1"
#define OC_RESULT_MAGIC "OCRES1-c1\n"
#define OC_MANIFEST_MAGIC "OCMAN1-c1\n"
#define OC_MEMO_MAGIC "OCMEMO1-c1\n"
#define OC_PY_RESULT_PREFIX "OCRES1-"   /* the Python entries' magics start so (then the Python version) */
#define OC_PY_MANIFEST_PREFIX "OCMAN1-"
#define OC_OUT "\x01OUT\x01"            /* the -o value inside a stored argument list */
#define OC_CWD_MARK "\x01CWD\x01"       /* the working directory inside stored compiler output */
#define OC_MAX_DEPSETS 16
#define OC_DEFAULT_MAX_GB 2.0
#define OC_AUTO_PRUNE_ONE_IN 500
#define OC_MTIME_SLACK 0.5
#define OC_RACY_NS (3LL * 1000 * 1000 * 1000)
#define OC_STATS_LOG "objcache-stats.log"

/* ---- environment ---- */
const char *oc_default_root(AvpArena *a); /* $SHSM_CACHE_DIR, else %LOCALAPPDATA%\shsm-cache */
const char *oc_cache_dir(AvpArena *a);    /* <default root>\objcache */
/* SHSM_OBJCACHE: off for "0", "off", "no", "false" (stripped, lower-cased); value NULL = unset = on. */
bool oc_enabled_value(const char *value);

/* An environment as NAME=VALUE strings (the wrapper's own, or a test's). */
typedef struct {
    const char *const *vars;
    size_t n;
} OcEnv;
OcEnv oc_env_current(AvpArena *a);
const char *oc_env_get(const OcEnv *e, const char *name); /* case-insensitive name, NULL when absent */

/* ---- small helpers ---- */
char *oc_norm(AvpArena *a, const char *p);          /* normcase(normpath(p)) */
char *oc_sha_hex(AvpArena *a, const void *p, size_t n);
/* sha256 hex of a file, NULL when it cannot be read; counts oc_files_hashed (tests check hits read no header). */
char *oc_file_sha(AvpArena *a, const char *path);
extern volatile long oc_files_hashed;
extern void (*oc_on_hash)(const char *path); /* a test hook: called with every file hashed */
/* cwd.rstrip("\\/") + "\\" + p for kind 'r', else p */
char *oc_join_dep(AvpArena *a, const char *cwd, char kind, const char *p);

/* ---- the hash memo ---- */
typedef struct OcMemo OcMemo;
typedef struct {
    u64 ino;
    i64 size, mtime_ns, hashed_ns;
    const char *sha; /* hex */
} OcMemoEntry;
const char *oc_memo_path(AvpArena *a, const char *cwd);
OcMemo *oc_memo_load(AvpArena *a, const char *cwd); /* never NULL; empty when absent or damaged */
size_t oc_memo_count(const OcMemo *m);
const OcMemoEntry *oc_memo_find(const OcMemo *m, const char *normpath_key);
bool oc_memo_entry_holds(const OcMemoEntry *e, u64 ino, i64 size, i64 mtime_ns);
/* the memo's sha when its entry holds, else the file hashed afresh; NULL when it cannot be read */
char *oc_memo_sha(AvpArena *a, OcMemo *m, const char *path);
/* prehash: write the memo of working directory root; tops NULL means {"include", "src"}. */
typedef struct {
    i64 files, hashed, kept;
} OcPrehash;
bool oc_prehash(AvpArena *a, const char *root, const char *compilers, const char *const *tops, size_t ntops, int jobs,
                OcPrehash *out);

/* ---- the command line ---- */
typedef struct {
    const char *const *argv;
    size_t argc;
    const char *cwd;
    size_t ci;     /* the compiler's index */
    size_t oi;     /* the index of the -o value */
    const char *src, *out, *obj, *dep;
    AvpStrVec include_dirs;
} OcCommand;
/* false with *why set when the compile cannot be cached */
bool oc_command_parse(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, OcCommand *c,
                      const char **why);
char *oc_cmd_path(AvpArena *a, const OcCommand *c, const char *p); /* os.path.join(cwd, p) */
/* sha256 over the compiler, the DLLs beside it and each wrapper in front of it; memo NULL: every file hashed afresh */
const char *oc_compiler_identity(AvpArena *a, const OcCommand *c, OcMemo *memo, const char **why);
const char *oc_manifest_key(AvpArena *a, const OcCommand *c, const char *compiler_id, AvpStr source,
                            const OcEnv *env);

/* ---- the dependency file ---- */
AvpStr oc_render_depfile(AvpArena *a, const char *target, const char *src, const char *const *deps, size_t n);
bool oc_parse_depfile(AvpArena *a, AvpStr data, const char *src, const char **target, AvpStrVec *deps,
                      const char **why);

/* ---- include probes ---- */
/* every name an #include spells (separators made '\'); false (computed include, non-ASCII name) with *why */
bool oc_include_names(AvpArena *a, AvpStr data, AvpSet *names, const char **why);

/* ---- the wrapper ---- */
typedef struct {
    AvpBuf *out, *err; /* NULL: the process's stdout / stderr (raw bytes) */
    bool no_auto_prune;
} OcRunOpts;
int oc_run_wrapped(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, const OcEnv *env,
                   const OcRunOpts *o);

/* ---- entries and maintenance ---- */
typedef enum { OC_KIND_OTHER, OC_KIND_RESULT, OC_KIND_MANIFEST } OcKind;
typedef struct {
    const char *path, *key;
    i64 size;
    i64 mtime_ns;
    OcKind kind; /* C or Python entries alike */
    bool c_result; /* a C result entry (the ones verify checks) */
} OcEntry;
typedef AVP_VEC(OcEntry) OcEntries;
/* tools/common/cache.py Cache.entries() over root, with each entry's kind from its magic when with_kind */
OcEntries oc_entries(AvpArena *a, const char *root, bool with_kind);
/* Cache.prune: (removed, freed) */
void oc_prune(AvpArena *a, const char *root, i64 max_bytes, i64 *removed, i64 *freed);
/* verify one C result entry: "ok", "stale" or "bad: <reason>" */
const char *oc_verify_entry(AvpArena *a, const char *root, const char *cache_root, const char *key,
                            const char *scratch);

/* result entries, for the tests */
typedef struct {
    AvpStrVec argv;
    const char *src, *src_sha, *compiler, *target_suffix, *obj_sha, *cwd, *manifest;
    AvpStrVec dep_kinds, dep_paths, dep_shas;
    AvpStr out_text, err_text;
    double created;
    AvpStr obj;
} OcResult;
bool oc_unpack_result(AvpArena *a, AvpStr data, OcResult *r);
AvpStr oc_pack_result(AvpArena *a, const OcResult *r);
