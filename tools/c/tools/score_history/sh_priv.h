/* score_history internals: the private copy of the rule checker's lexer and function parser (sh_lex.c), source
 * indexing (sh_index.c), the database and recording (sh_db.c), the analysis and commands (sh_cmd.c) and the CLI
 * (sh_cli.c). */
#pragma once
#include "score_history.h"
#include "avp/avp.h"

#include <sqlite3.h>

/* ---- lexer (check_rules.lex, pair_brackets) -------------------------------------------------------------------- */
enum { SH_ID, SH_NUM, SH_STR, SH_CHR, SH_OP };
typedef struct {
    AvpStr t;
    int line;
    u8 k;
} ShTok;
typedef AVP_VEC(ShTok) ShTokVec;
typedef struct {
    int line, end_line;
    AvpStr text;
    bool trailing;
} ShComment;
typedef AVP_VEC(ShComment) ShCommentVec;
typedef struct {
    int line, end_line;
    AvpStr name, rest;
} ShDirective;
typedef AVP_VEC(ShDirective) ShDirectiveVec;
typedef struct {
    ShTokVec toks;
    ShCommentVec comments;
    ShDirectiveVec directives;
} ShLex;

/* The throw code of the lexer's crash (Python: `m.group(0)` of a failed _OP match, an AttributeError). */
#define SH_EXC_CRASH 0x5348

/* lex(text): text is valid UTF-8 (decoded with errors="replace"). Vectors are heap vectors (sh_lex_free). */
void sh_lex(AvpArena *a, AvpStr text, ShLex *out);
void sh_lex_free(ShLex *lx);
/* pair_brackets: partner index per token, -1 for none (arena array of n ints). */
int *sh_pair_brackets(AvpArena *a, const ShTok *toks, size_t n);

/* A function definition: name, first and last line. */
typedef struct {
    AvpStr name;
    int line, end_line;
} ShFunc;
typedef AVP_VEC(ShFunc) ShFuncVec;
/* Parsed(path, text).funcs (asm bodies masked as the rule checker does), from the lexed file. */
void sh_parsed_funcs(AvpArena *a, const ShLex *lx, ShFuncVec *out);
/* _KEYWORDS of score_history */
bool sh_is_keyword(AvpStr s);
/* ADDR_RE.search(comment text): the first `0x8XXXXXXX` alone on its line. */
bool sh_addr_in_comment(AvpStr text, i64 *addr);
/* score_history.rough_functions over the file's tokens. */
void sh_rough_functions(AvpArena *a, const ShTok *toks, size_t n, ShFuncVec *out);

/* bytes.decode("utf-8", "replace"): maximal invalid subparts become one U+FFFD each. */
AvpStr sh_decode_utf8(AvpArena *a, AvpStr bytes);
/* text.replace("\r\n", "\n").replace("\r", "\n") */
AvpStr sh_normalise(AvpArena *a, AvpStr text);
/* hashlib.sha1(s.encode()).hexdigest() */
char *sh_sha1(AvpArena *a, AvpStr s);

/* ---- source index (index_source, FileIndex, demangle) ---------------------------------------------------------- */
typedef struct {
    AvpStr name;
    int line, end;
    bool has_addr;
    i64 addr;
    AvpStr hash;
} ShIdxFunc;
typedef struct {
    ShIdxFunc *funcs;
    size_t nfuncs;
    AvpStr unit_hash, rest_hash;
    bool parsed;
    /* _texts: {hash: text} in insertion order (only from a fresh index) */
    AvpStr *text_hash, *text;
    size_t ntexts;
    AvpIMap *by_addr; /* addr -> func index + 1 (first wins) */
    AvpMap *by_name;  /* name -> (count << 32) | first func index */
} ShIndex;

ShIndex *sh_index_source(AvpArena *a, AvpStr text);
/* json.dumps of the cached dict ({"funcs", "unit_hash", "rest_hash", "parsed"}) and back. */
AvpStr sh_index_to_json(AvpArena *a, const ShIndex *ix);
ShIndex *sh_index_from_json(AvpArena *a, AvpStr json);
/* FileIndex.lookup: the function (NULL) and the basis ("addr", "name", "sinit", "unit"). */
const ShIdxFunc *sh_index_lookup(AvpArena *a, const ShIndex *ix, AvpStr symbol, i64 addr, const char **basis);
/* demangle(sym): candidates (a vector of strings), or *sinit when the symbol is a static initialiser. */
AvpStrVec sh_demangle(AvpArena *a, AvpStr sym, bool *sinit);

/* ---- database -------------------------------------------------------------------------------------------------- */
typedef struct {
    AvpArena *a;
    sqlite3 *db;
    const char *repo;
    AvpBuf *out;
    AvpMap *mem; /* Indexer.mem: cache key -> ShIndex* */
} ShCtx;

/* Raise a Python exception class name and message: printed as the traceback's last line, exit 1. */
#define SH_EXC_RUNTIME 0x5349 /* RuntimeError */
#define SH_EXC_EXIT 0x534A    /* SystemExit(message) */
#define SH_EXC_OTHER 0x534B   /* any other uncaught exception; msg is "Class: text" */

void sh_db_open(ShCtx *c, const char *path);
void sh_db_close(ShCtx *c);
void sh_exec(ShCtx *c, const char *sql);
sqlite3_stmt *sh_prepare(ShCtx *c, const char *sql);
AVP_NORETURN void sh_sql_fail(ShCtx *c);

typedef struct {
    bool rev_given, report_given, include_stale;
    const char *rev, *report, *baseline;
} ShRecordOpts;
int sh_cmd_record(ShCtx *c, const ShRecordOpts *o);
int sh_cmd_backfill(ShCtx *c, const char *baseline, bool first_parent, bool limit_given, i64 limit);

typedef struct {
    const char *unit, *vs;
    bool lost, regressed, all, json;
} ShStatusOpts;
int sh_cmd_status(ShCtx *c, const ShStatusOpts *o);
int sh_cmd_show(ShCtx *c, const char *symbol, bool source_given, const char *source, bool json);
