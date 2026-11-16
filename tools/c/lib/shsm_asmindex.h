/* The persistent sqlite index of the retail asm listings (tools/common/asmindex.py): build/index/asm.sqlite, the same
 * schema, schema version and meta rows as the Python, so that either language reads what the other wrote. Opening
 * refreshes changed listings (size + mtime_ns per file) under the write lock; read-only opening never writes unless
 * the index must change. Query API as AsmIndex. Documented in shsm_asmindex.md.
 *
 * None: integers SHSM_NONE, strings p == NULL (as shsm_asm.h). Results are allocated from the arena given.
 * Errors: sqlite3.OperationalError is SHSM_EXC_SQLITE (message as sqlite's, or the Python's own text). */
#pragma once
#include "shsm_asm.h"
#include "shsm_repo.h"

#define SHSM_ASMINDEX_SCHEMA_VERSION 2
#define SHSM_EXC_SQLITE 96 /* sqlite3.OperationalError (and other sqlite3.Error) */

typedef struct sqlite3 sqlite3;

/* last_refresh: scanned, parsed, removed, parse_seconds, seconds, ignored (stale listings), note. */
typedef struct {
    i64 scanned, parsed, removed;
    double parse_seconds, seconds;
    AvpStrVec ignored;
    const char *note;
} ShsmRefresh;

typedef struct ShsmAsmIndex {
    const ShsmRepo *repo;
    const char *asm_dir, *db_path;
    bool readonly;
    sqlite3 *db;
    AvpArena *arena;              /* the index's own allocations (paths, listings, last_refresh) */
    bool has_last_refresh;        /* last_refresh is None until a refresh ran */
    ShsmRefresh last_refresh;
    ShsmListings *listings;       /* of the last scan, or NULL */
    bool rebuilt;                 /* this open replaced an older index: rebuilt_files, rebuilt_seconds */
    i64 rebuilt_files;
    double rebuilt_seconds;
} ShsmAsmIndex;

/* AsmIndex.open(repo, refresh, rebuild, jobs, db_path, asm_dir, readonly). Zero fields are Python's defaults:
 * repo NULL = Repo.find(root) (root NULL: the cwd's checkout); no_refresh false = refresh; jobs 0 = CPU count (at
 * most 32); db_path NULL = <index_dir>/asm.sqlite; asm_dir NULL = the repo's. */
typedef struct {
    const ShsmRepo *repo;
    const char *root;
    bool no_refresh, rebuild, readonly;
    int jobs;
    const char *db_path, *asm_dir;
} ShsmAsmIndexOpts;
ShsmAsmIndex *shsm_asmindex_open(const ShsmAsmIndexOpts *o);
void shsm_asmindex_close(ShsmAsmIndex *ix);
/* refresh(jobs): re-parse new or changed listings, drop deleted ones; returns (and keeps) last_refresh. Raises
 * FILE_NOT_FOUND when the asm dir is missing, SHSM_EXC_SQLITE on a read-only index that is out of date. */
const ShsmRefresh *shsm_asmindex_refresh(ShsmAsmIndex *ix, int jobs);
void shsm_asmindex_clear(ShsmAsmIndex *ix);
/* True when no listing under the asm dir is new, changed (size, mtime) or gone since the index was written; reads, never
 * writes. A missing asm dir counts as current (nothing to be out of date against). */
bool shsm_asmindex_is_current(ShsmAsmIndex *ix);
/* Progress notice of a refresh that parses at least shsm_asmindex_notice_min listings (default 25), before it parses:
 * one line, to stderr by default; NULL silences it. */
extern void (*shsm_asmindex_notice)(const char *line);
extern size_t shsm_asmindex_notice_min;
i64 shsm_asmindex_total_changes(ShsmAsmIndex *ix); /* connection.total_changes since open */
/* last_refresh as the Python dict (scanned, parsed, removed, parse_seconds, seconds, ignored, note). */
AvpJson *shsm_refresh_json(AvpArena *a, const ShsmRefresh *r);

typedef struct {
    AvpStr kind;
    i64 count;
} ShsmKindCount;
typedef struct {
    i64 files, symbols, refs, strings, floats, sections, refs_resolved;
    AVP_VEC(ShsmKindCount) symbols_by_kind, refs_by_kind; /* GROUP BY order */
} ShsmIndexStats;
ShsmIndexStats shsm_asmindex_stats(AvpArena *a, ShsmAsmIndex *ix);
AvpJson *shsm_stats_json(AvpArena *a, const ShsmIndexStats *s);

/* ---- symbol rows ---- */
typedef struct {
    AvpStr name;
    i64 address, size;
    AvpStr kind, section, scope;
    bool hidden;
    AvpStr unit, file;
    i64 line_start, line_end, byte_start, byte_end;
    AvpStr demangled;
    i64 nrows;
} ShsmSymRow;
typedef AVP_VEC(ShsmSymRow) ShsmSymRows;
AvpJson *shsm_symrow_json(AvpArena *a, const ShsmSymRow *s); /* SymRow.to_dict() */
const char *shsm_symrow_unit_name(AvpArena *a, const ShsmSymRow *s); /* "main/" + unit */

ShsmSymRows shsm_asmindex_symbols_named(AvpArena *a, ShsmAsmIndex *ix, AvpStr name);
bool shsm_asmindex_symbol(AvpArena *a, ShsmAsmIndex *ix, AvpStr name, ShsmSymRow *out);
bool shsm_asmindex_symbol_at(AvpArena *a, ShsmAsmIndex *ix, i64 address, ShsmSymRow *out);
ShsmSymRows shsm_asmindex_at(AvpArena *a, ShsmAsmIndex *ix, i64 address);
bool shsm_asmindex_containing(AvpArena *a, ShsmAsmIndex *ix, i64 address, ShsmSymRow *out);
const char *shsm_asmindex_unit_of(AvpArena *a, ShsmAsmIndex *ix, i64 address); /* NULL: None */
ShsmSymRows shsm_asmindex_unit_symbols(AvpArena *a, ShsmAsmIndex *ix, const char *unit, const char *kind);
ShsmSymRows shsm_asmindex_functions(AvpArena *a, ShsmAsmIndex *ix);
ShsmSymRows shsm_asmindex_search(AvpArena *a, ShsmAsmIndex *ix, AvpStr pattern, const char *kind, i64 limit);

/* A query target: a name (every block so named), an address or a SymRow (exact). */
typedef struct {
    int t; /* 0 name, 1 address, 2 SymRow */
    AvpStr name;
    i64 addr;
    const ShsmSymRow *sym;
} ShsmTarget;
static inline ShsmTarget shsm_target_name(AvpStr n) { return (ShsmTarget){0, n, 0, NULL}; }
static inline ShsmTarget shsm_target_addr(i64 x) { return (ShsmTarget){1, {NULL, 0}, x, NULL}; }
static inline ShsmTarget shsm_target_sym(const ShsmSymRow *s) { return (ShsmTarget){2, {NULL, 0}, 0, s}; }

/* ---- blocks: function()/object() by name or address (a SymRow target uses its address); read(sym) ---- */
ShsmAsmBlock *shsm_asmindex_read(ShsmAsmIndex *ix, const ShsmSymRow *sym, ShsmAsmFile **file);
ShsmAsmBlock *shsm_asmindex_function(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, ShsmAsmFile **file);
ShsmAsmBlock *shsm_asmindex_object(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, ShsmAsmFile **file);

/* ---- references (kinds: NULL/0 = any) ---- */
typedef AVP_VEC(ShsmAsmRef) ShsmRefs;
ShsmRefs shsm_asmindex_refs_to(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, const char *const *kinds, size_t nk);
ShsmRefs shsm_asmindex_refs_at(AvpArena *a, ShsmAsmIndex *ix, i64 address, const char *const *kinds, size_t nk);
ShsmRefs shsm_asmindex_refs_from(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, const char *const *kinds, size_t nk);

typedef struct {
    AvpStr name;   /* from_sym (p NULL: None) */
    i64 site;
    const char *kind;
} ShsmCaller;
typedef AVP_VEC(ShsmCaller) ShsmCallers;
ShsmCallers shsm_asmindex_callers(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, bool include_pointers);
typedef struct {
    AvpStr name;
    const char *kind;
    i64 count;
} ShsmCallee;
typedef AVP_VEC(ShsmCallee) ShsmCallees;
ShsmCallees shsm_asmindex_callees(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t);
typedef struct {
    AvpStr caller;
    i64 caller_addr;
    AvpStr callee;
    i64 callee_addr;
    const char *kind;
    i64 site;
} ShsmCallEdge;
typedef AVP_VEC(ShsmCallEdge) ShsmCallEdges;
ShsmCallEdges shsm_asmindex_call_graph(AvpArena *a, ShsmAsmIndex *ix, const char *const *kinds, size_t nk);

typedef struct {
    AvpStr sym;
    i64 offset, address; /* address: data_targets only (SHSM_NONE when unknown) */
} ShsmDataRef;
typedef AVP_VEC(ShsmDataRef) ShsmDataRefs;
ShsmDataRefs shsm_asmindex_data_refs(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t);
ShsmDataRefs shsm_asmindex_data_targets(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t);

typedef struct {
    AvpStr owner;
    i64 address;
    AvpStr text;
} ShsmStrRow;
typedef AVP_VEC(ShsmStrRow) ShsmStrRows;
ShsmStrRows shsm_asmindex_strings_of(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, bool follow_pointers);
ShsmStrRows shsm_asmindex_find_strings(AvpArena *a, ShsmAsmIndex *ix, AvpStr needle, i64 limit);
ShsmStrRows shsm_asmindex_users_of_string(AvpArena *a, ShsmAsmIndex *ix, AvpStr needle, i64 limit);
typedef struct {
    AvpStr owner;
    i64 address;
    AvpStr kind;
    bool has_value;
    double value;
} ShsmFloatRow;
typedef AVP_VEC(ShsmFloatRow) ShsmFloatRows;
ShsmFloatRows shsm_asmindex_floats_of(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t);

/* ---- tables and sections ---- */
typedef struct {
    i64 address;
    AvpStr target;
    i64 target_addr;
    AvpStr section;
} ShsmTableEntry;
typedef AVP_VEC(ShsmTableEntry) ShsmTableEntries;
ShsmTableEntries shsm_asmindex_ctors(AvpArena *a, ShsmAsmIndex *ix);
ShsmTableEntries shsm_asmindex_dtors(AvpArena *a, ShsmAsmIndex *ix);
typedef AVP_VEC(ShsmAsmSection) ShsmSectionRanges;
/* merge: {base name: (start, end)} in first-seen order; else every run in start order. */
ShsmSectionRanges shsm_asmindex_section_ranges(AvpArena *a, ShsmAsmIndex *ix, bool merge);
AvpStr shsm_asmindex_section_of(AvpArena *a, ShsmAsmIndex *ix, i64 address); /* p NULL: None */
/* sql(query, params): params a JSON array (ints, floats, strings, null); rows as a JSON array of arrays. */
AvpJson *shsm_asmindex_sql(AvpArena *a, ShsmAsmIndex *ix, const char *query, const AvpJson *params);
