/* hashes (tools/hashes.py): names from the string hashes and interned names of the retail binary. The hash
 * functions transcribed from the retail code, the static-initialiser pair reader, the derived database
 * build/index/hashes.sqlite (the same schema, stamp and rows as the Python's, so either language reads what the
 * other wrote) and the subcommands. Other tools link this header (tool_hashes) for hashes.var_names, the hash
 * functions and the database.
 *
 * Byte strings (the Python's bytes) are AvpStr views of raw bytes; text (the Python's str, the latin-1 decoding of
 * those bytes) is UTF-8. Results are allocated from the arena given. */
#pragma once
#include "avp/arena.h"
#include "avp/json.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_dol.h"
#include "shsm_repo.h"

#define HASHES_DB_SCHEMA 4
#define HASHES_ARC_MAGIC 0xFA10u

/* ---- hash functions ---- */
u32 hashes_arcname(AvpStr b);                /* fn_801B2C7C: basename, A-Z lower-cased, h = (h * 33) ^ c */
u32 hashes_djb2(AvpStr b, i64 maxlen);       /* fn_801B6020: h = 5381; h = h * 33 + c; maxlen -1: all */
u32 hashes_djb2fold(AvpStr b, i64 maxlen);   /* fn_801B605C: djb2 over c & 0xDF */
u32 hashes_eventxor(AvpStr b);               /* HashString: h = (h << 1) ^ c */
u32 hashes_crc32(AvpStr b);                  /* fn_801B570C: zlib's CRC-32 (over every byte given) */

typedef u32 (*HashesFunc)(AvpStr b);
typedef struct {
    const char *name;
    HashesFunc func;
    const char *routine, *evidence;
    bool name_hash;
} HashesFn;
enum { HASHES_COUNT = 5, HASHES_NAME_COUNT = 4 };
extern const HashesFn HASHES[HASHES_COUNT];  /* arcname djb2 djb2fold eventxor crc32; NAME_HASHES: the first four */
const HashesFn *hashes_by_name(const char *name); /* NULL when unknown */
AvpJson *hashes_fn_json(AvpArena *a, const HashesFn *h); /* HashFn.to_dict() */

/* ---- value classification ---- */
const char *hashes_excluded(u32 v);          /* "small integer" / "address" / "bit mask" / NULL */
double hashes_expected_chance(i64 corpus_size, i64 candidates);
const char *hashes_strength(double expected);

/* ---- the retail DOL and archives ---- */
/* Dol.cstring(address, limit=512): a NUL-terminated run of printable bytes (0x20-0x7E or >= 0x80) starting at
 * address, false when none. */
bool hashes_cstring(const ShsmDol *d, i64 address, AvpStr *out);
/* Dol.read(address, size): within one section. */
bool hashes_dol_read(const ShsmDol *d, i64 address, i64 size, AvpStr *out);
/* read_arc_table(path): the entry hashes, in table order (the index is the position); empty when the file is not
 * an archive. Reads the 16-byte header and the entry table only. Raises the fs failure when the file cannot be
 * opened. */
AvpU64Vec hashes_read_arc_table(const char *path);

typedef struct {
    AvpStr text;   /* bytes */
    i64 address;
} HashesDolString;
typedef AVP_VEC(HashesDolString) HashesDolStrings;
/* dol_strings(dol): NUL-terminated printable runs (2+ characters) in the data sections, first address each, in
 * first-seen order. */
HashesDolStrings hashes_dol_strings(AvpArena *a, const ShsmDol *d);

typedef struct {
    u32 value;
    const char *kind; /* "data", "lis/ori", "lis/addi", "addis/cmplwi" */
    i64 address;
} HashesConst;
typedef AVP_VEC(HashesConst) HashesConsts;
void hashes_scan_code_words(const u32 *words, size_t n, i64 base, HashesConsts *out);
HashesConsts hashes_code_constants(const ShsmDol *d);  /* heap vector (avp_vec_free) */

/* ---- static-initialiser pattern reader ---- */
typedef struct {
    const char *kind; /* "intern", "djb2", "djb2fold", "registry" */
    const char *sreg; /* the register holding the string */
    bool obj;         /* 'obj': r3 is the variable the routine fills; else 'ret' */
} HashesCallSpec;
typedef struct {
    u32 address;
    HashesCallSpec spec;
} HashesRoutine;
extern const HashesRoutine HASHES_CALL_ROUTINES[5];
const char *hashes_call_term(const char *call);  /* CALL_TERMS.get(call, call) */

typedef struct {
    i64 var_addr;
    AvpStr var;      /* p NULL: None */
    i64 var_off;
    AvpStr text;     /* UTF-8 */
    const char *call, *via;
    i64 site;
} HashesPair;
typedef AVP_VEC(HashesPair) HashesPairs;

/* The callbacks of read_pairs: target_of(row) (the row's resolved address; NULL: use sym_addr) or sym_addr(name)
 * (+ the row's offset), and deref(address or pointer) -> the string text (UTF-8) and its via. */
typedef struct {
    void *ctx;
    bool (*target_of)(void *ctx, const ShsmAsmRow *row, i64 *addr);
    bool (*sym_addr)(void *ctx, AvpStr sym, i64 *addr);
    bool (*deref)(void *ctx, bool ptr, i64 address, AvpStr *text, const char **via);
} HashesReader;
/* read_pairs(block, calls, ...): calls maps a callee name to its HashesCallSpec * (AvpVal.p). */
HashesPairs hashes_read_pairs(AvpArena *a, const ShsmAsmBlock *b, const AvpMap *calls, const HashesReader *rd);

/* ---- corpus helpers ---- */
/* format_derivations(corpus): the formats (one %s, no other conversion) and the identifier-like bases. */
void hashes_format_derivations(AvpArena *a, const AvpStr *corpus, size_t n, AvpStrVec *fmts, AvpStrVec *bases);
/* read_words(paths): names from --words files (text by line and token; binary by printable runs of 4+), as a set of
 * byte strings (insertion order). Raises the fs failure on an unreadable file. */
AvpSet *hashes_read_words(const AvpStrVec *paths);

/* source_names(repo): variable -> AvpSet * of the quoted strings (UTF-8) on the lines of src/ and include/ that
 * mention it. */
AvpMap *hashes_source_names(const ShsmRepo *r);
const char *hashes_var_status(const AvpMap *names, AvpStr var, AvpStr text); /* "new" / "named" / "mismatch" */
/* pair_classes: (primary class, function's class); primary "-" when there is none. */
void hashes_pair_classes(AvpArena *a, AvpStr function, AvpStr text, AvpStr call, AvpStr *primary, AvpStr *fcls);

/* latin-1 bytes <-> text: _t(b) and _b(s) (code points above 0xFF become '?'). */
AvpStr hashes_text(AvpArena *a, AvpStr bytes);
AvpStr hashes_bytes(AvpArena *a, AvpStr text);
/* _show(t): "..." with \\, \" and \n escaped. */
AvpStr hashes_show(AvpArena *a, AvpStr text);

/* ---- the database ---- */
typedef struct sqlite3 sqlite3;
typedef struct {
    const ShsmRepo *repo;
    ShsmAsmIndex *ix;
    const char *path;
    sqlite3 *db;
    ShsmDol *dol;    /* loaded by a build */
} HashesDb;
const char *hashes_db_path(AvpArena *a, const ShsmRepo *r); /* <index_dir>/hashes.sqlite */
/* HashDb._stamp(): "<schema>|<asm index built>|<DOL>|<data.arc>|<igc.arc>". */
const char *hashes_stamp(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix);
/* HashDb(repo, ix, rebuild): the connection, the tables rebuilt when the stamp differs or rebuild is set. */
HashesDb *hashes_db_open(const ShsmRepo *r, ShsmAsmIndex *ix, bool rebuild);
void hashes_db_close(HashesDb *h);

/* var_names(repo, unit, addresses, functions): the pairs `vars` knows whose unit is unit (any spelling, NULL: none),
 * whose variable address is in addresses or whose function address is in functions, as the Python's dicts
 * {var_addr, var, var_off, text, call, function, function_addr, sinit, unit, kind}; reads hashes.sqlite as it stands
 * (never builds it). NULL when the file is absent or unreadable. repo NULL: Repo.find(cwd). */
typedef struct {
    i64 var_addr;
    const char *var;
    i64 var_off;
    const char *text, *call, *function;
    i64 function_addr;
    bool sinit;
    const char *unit, *kind;
} HashesVarName;
typedef AVP_VEC(HashesVarName) HashesVarNames;
HashesVarNames *hashes_var_names(AvpArena *a, const ShsmRepo *r, const char *unit, const i64 *addresses,
                                 size_t naddresses, const i64 *functions, size_t nfunctions);
AvpJson *hashes_var_name_json(AvpArena *a, const HashesVarName *v);

/* ---- subcommands ---- */
typedef struct {
    const char *cmd;
    bool unverified;
    /* scan */
    AvpStrVec functions, units, ranges;
    const char *lib;
    bool no_arc;
    const char *root;
    /* of / find */
    AvpStrVec strings, values, words;
    bool has_words;
    /* vars */
    const char *status, *pattern, *grep, *cls, *call;
    bool by_class;
} HashesArgs;
/* Where a subcommand writes: an open cli.Output, or (out NULL, tests) the lines and the data object collected. */
typedef struct {
    ShsmOutput *out;
    AvpStrVec lines;
    AvpJson *data;
} HashesOut;
/* The verified hash names: SELECT DISTINCT fn FROM truth. */
AvpSet *hashes_verified(HashesDb *h);
int hashes_cmd_scan(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out);
int hashes_cmd_of(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out);
int hashes_cmd_find(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out);
int hashes_cmd_stats(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out);
int hashes_cmd_functions(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out);

/* cmd_vars over given rows (the database's, or a test's) and source names. */
typedef struct {
    i64 var_addr;
    const char *var;
    i64 var_off;
    const char *text, *call, *via;
    i64 site;
    const char *function;
    i64 function_addr;
    i64 sinit;
    const char *unit;
} HashesVarRow;
typedef AVP_VEC(HashesVarRow) HashesVarRows;
HashesVarRows hashes_db_var_rows(AvpArena *a, HashesDb *h);
int hashes_cmd_vars_rows(const HashesArgs *args, const HashesVarRows *rows, const AvpMap *names, HashesOut *out);
int hashes_cmd_vars(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out);

int hashes_main(int argc, char **argv);
