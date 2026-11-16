/* levers: the lever catalogue of tools/levers.py (phase 1: tokenizer, definition finder, statement and expression
 * parsers, FnContext with every lever family, the fake-form filter). Internal to the levers tool.
 *
 * Text model: the Python reads the source as latin-1 with newline="", so one character is one byte and every offset
 * is a byte offset. Python's str whitespace, \w, isdigit and splitlines are reproduced for latin-1 bytes.
 *
 * Exceptions: the Python's exception classes are avp_throw codes (LV_EXC_*), with the Python message for ParseError.
 * Token texts are interned (one process-wide table, not thread-safe), so equal texts are equal pointers. */
#pragma once
#include "avp/avp.h"

/* ---- exceptions ---- */
enum {
    LV_EXC_PARSE = 100, /* ParseError */
    LV_EXC_KEY,         /* KeyError */
    LV_EXC_INDEX,       /* IndexError */
    LV_EXC_TYPE,        /* TypeError */
    LV_EXC_ATTR,        /* AttributeError */
    LV_EXC_DEMANGLE,    /* an exception out of mangle.parse */
};
const char *lv_exc_name(int code); /* "ParseError", ...; NULL for a code that is not one of ours */
AVP_NORETURN void lv_parse_error(const char *fmt, ...);
/* True when code is ParseError, KeyError or IndexError (the classes the Python catches). */
bool lv_exc_caught(int code);

/* ---- latin-1 str helpers (Python semantics) ---- */
typedef AvpStr LvS;
#define LVS(lit) AVP_S(lit)
bool lv_isspace(u8 c);
bool lv_isword(u8 c);
LvS lv_strip(LvS s);
LvS lv_lstrip(LvS s);
LvS lv_rstrip(LvS s);
bool lv_blank(LvS s); /* not s.strip() */
bool lv_s_eq(LvS a, LvS b);
bool lv_s_eqz(LvS a, const char *z);
bool lv_s_startswith(LvS s, LvS p);
bool lv_s_contains(LvS hay, LvS needle);
LvS lv_catv(AvpArena *a, const LvS *parts, size_t n);
#define LV_CAT(a, ...) lv_catv((a), (LvS[]){__VA_ARGS__}, sizeof((LvS[]){__VA_ARGS__}) / sizeof(LvS))
const char *lv_repr(AvpArena *a, LvS s); /* repr(str) of a latin-1 text, as latin-1 bytes */

/* ---- tokens ---- */
enum { LV_ID, LV_NUM, LV_STR, LV_CHR, LV_OP, LV_COMMENT, LV_PP };
typedef struct {
    u8 k;
    const char *t; /* interned, NUL-terminated */
    u32 n;
    i32 s, e;
} LvTok;
typedef struct {
    LvTok *v;
    i32 n;
} LvToks;

const char *lv_intern(const char *p, size_t n);
const char *lv_internz(const char *z);
const char *lv_intern_find(const char *z); /* NULL when the text was never interned */
const char *lv_kind_name(u8 k);

LvToks lv_tokenize(AvpArena *a, LvS text);
LvToks lv_code_tokens(AvpArena *a, LvToks toks);
/* match_brackets: m[i] is the partner of bracket i, -1 for other tokens. */
i32 *lv_match_brackets(AvpArena *a, LvToks code);

/* Python list indexing (negative from the end; IndexError outside) and dict lookup (KeyError). */
const LvTok *lv_at(LvToks c, i64 i);
i32 lv_m(const i32 *m, LvToks c, i64 i);
#define TEQ(tok, lit) (strcmp((tok)->t, lit) == 0)
bool lv_in(const char *s, const char *const *tbl); /* tbl NULL-terminated */

extern const char *const LV_BUILTIN[], *const LV_QUALS[], *const LV_SCALAR_TYPEDEFS[], *const LV_INT_WORDS[],
    *const LV_FLOAT_WORDS[], *const LV_NARROW[], *const LV_NOT_TYPE[], *const LV_NOT_NAME[], *const LV_ASSIGN[],
    *const LV_UNARY[];
int lv_bin_prec(const char *op); /* 0 when not a binary operator */

/* ---- function definitions ---- */
typedef struct {
    i32 a, b; /* half-open */
} LvSpan;
typedef struct {
    const char *name; /* interned */
    const char **quals;
    i32 nquals;
    const char *cls; /* NULL: None */
    i32 nparams;
    i32 head, lbrace, rbrace;
    bool is_void;
    LvSpan *params;
} LvFnDef;
typedef AVP_VEC(LvFnDef) LvFnDefs;

LvSpan *lv_split_top(AvpArena *a, LvToks c, i32 lo, i32 hi, const i32 *m, const char *sep, i32 *count);
LvFnDefs lv_scan_definitions(AvpArena *a, LvToks c, const i32 *m);
typedef struct {
    const char *name;
    const char *cls; /* NULL: None */
    i32 nparams;     /* -1: None */
} LvTarget;
LvTarget lv_symbol_target(AvpArena *a, const char *symbol);
LvFnDef lv_find_definition(AvpArena *a, LvToks c, const i32 *m, const char *symbol);
bool lv_symbol_address(const char *symbol, u64 *addr);
bool lv_find_definition_at(AvpArena *a, LvS text, LvToks toks, LvToks c, const i32 *m, u64 addr, LvFnDef *out);
/* The address the CLI read for symbol from symbols.txt (0 when it has none); fn_<address> names carry their own. */
void lv_set_symbol_address(const char *symbol, u64 addr);

/* ---- declarations ---- */
typedef struct {
    i32 a, b;
    bool ok;
} LvRange; /* inclusive token range, ok false: None */
typedef struct {
    i32 a, b, name_i;
    const char *name;
    LvRange init;
    i32 eq; /* -1: None */
    bool ptr, array, complex;
} LvDtor;
typedef struct {
    i32 spec_a, spec_b;
    const char **words;
    i32 nwords;
    LvDtor *dtors;
    i32 ndtors;
    bool is_static;
} LvDecl;
LvDecl *lv_parse_decl(AvpArena *a, LvToks c, i32 lo, i32 semi, const i32 *m);

/* ---- expressions ---- */
enum {
    LN_COMMA, LN_ASSIGN, LN_TERN, LN_BIN, LN_UNARY, LN_SIZEOF, LN_CAST, LN_PAREN, LN_PRIM, LN_CALL, LN_INDEX,
    LN_MEMBER, LN_POST
};
typedef struct LvNode {
    u8 kind;
    i32 a, b;
    const char *op; /* NULL: None */
    struct LvNode **kids;
    i32 nk;
    i32 opi;          /* -1: None */
    i32 ex_a, ex_b;   /* cast: the type's token range; tern: ex_a the ':' index */
} LvNode;
typedef AVP_VEC(LvNode *) LvNodes;
int lv_prec(const LvNode *n);
void lv_walk(const LvNode *n, LvNodes *out); /* appends n and its descendants, preorder */

typedef struct {
    AvpArena *a;
    LvToks code;
    const i32 *m;
    i32 i, end;
    AvpISet *typenames; /* interned pointers */
} LvExprParser;
void lv_expr_init(LvExprParser *p, AvpArena *a, LvToks c, const i32 *m, i32 lo, i32 hi, AvpISet *typenames);
LvNode *lv_expr_parse(LvExprParser *p);
bool lv_expr_is_cast(LvExprParser *p, i32 i);

/* ---- statements ---- */
enum {
    LS_BLOCK, LS_IF, LS_WHILE, LS_SWITCH, LS_FOR, LS_DO, LS_LABEL, LS_EMPTY, LS_RETURN, LS_JUMP, LS_DECL, LS_EXPR
};
typedef struct LvStmt LvStmt;
typedef struct {
    LvStmt **v;
    i32 n;
} LvStmts;
struct LvStmt {
    u8 kind;
    i32 a, b;
    i32 lp, rp, wh;
    LvStmt *then, *els, *body; /* body: while/switch/for/do */
    LvStmts list;              /* block body */
    LvRange expr, init, cond, step;
    i32 semi0, semi1;
    LvDecl *decl;
};
LvStmts lv_body_parse(AvpArena *a, LvToks c, const i32 *m, i32 lo, i32 end);
void lv_iter_stmts(AvpArena *a, LvStmts L, AvpPtrVec *out);
typedef struct {
    LvStmt *owner; /* NULL: the function body */
    LvStmts L;
} LvBlock;
typedef AVP_VEC(LvBlock) LvBlocks;
void lv_iter_blocks(LvStmts L, LvStmt *owner, LvBlocks *out);
AvpISet *lv_collect_typenames(LvToks c);

/* ---- the function context ---- */
typedef struct {
    const char *family, *lever;
    i64 line;
    LvS text;
    LvS desc;
    i64 span0, span1;
} LvCandidate;
typedef AVP_VEC(LvCandidate) LvCandidates;

typedef struct {
    LvStmt *s;
    const char *role;
    LvNode *n;
} LvExprEntry;

typedef struct {
    i64 s, e;
    LvS text;
} LvEdit;
typedef AVP_VEC(LvEdit) LvEdits;

typedef struct {
    const char *name;
    LvStmt *s;
    LvDtor *dt;
} LvLocal;

/* A preprocessor directive (#line) inside the function: opaque, tagging the statement that follows it.
 * key: the identifier, number and string tokens of that statement's head (to its first top-level ';' or '{'). */
typedef struct {
    const char *text;
    const char **key;
    i32 nkey;
} LvPp;
typedef struct {
    i64 ls, le; /* a line holding only a directive, with its newline */
} LvPpLine;

typedef struct LvCtx {
    AvpArena *a;
    LvS text;
    const char *symbol;
    LvToks toks, code;
    i32 *m;
    LvFnDef fn;
    i32 lb, rb;
    i64 span0, span1;
    LvStmts body;
    LvToks comments;
    LvPp *pp; /* the directives of the function, in source order */
    i32 npp;
    LvPpLine *ppl; /* the lines holding only a directive */
    i32 nppl;
    AvpISet *typenames;
    LvS nl, pad, unit;
    bool file_has_do;
    int mixed; /* -1 unknown */
    AvpISet *file_ids;
    AvpIMap *body_ids; /* Counter */
    AVP_VEC(LvExprEntry) exprs;
    AvpISet *int_names, *float_names;
    AVP_VEC(LvLocal) locals;
    /* derived once: iter_stmts(body) and iter_blocks(body) */
    AvpPtrVec all_stmts;
    LvBlocks blocks;
    /* the lever being generated */
    const char *cur_family;
    LvCandidates *out;
} LvCtx;

LvCtx *lv_ctx_new(AvpArena *a, LvS text, const char *symbol);
LvCandidates lv_generate(LvCtx *x, const char *const *families, size_t nfam);
extern const char *const LV_FAMILIES[6];

/* helpers shared by the lever files */
LvS lv_src(LvCtx *x, i32 a, i32 b);
LvS lv_nt(LvCtx *x, const LvNode *n);
const char *lv_norm(LvCtx *x, i32 a, i32 b);
LvS lv_slice(LvCtx *x, i64 s, i64 e);
i64 lv_line_start(LvCtx *x, i64 pos);
i64 lv_line_end(LvCtx *x, i64 pos);
bool lv_lines(LvCtx *x, i32 a, i32 b, i64 *ls, i64 *le);
LvS lv_indent(LvCtx *x, i32 a);
bool lv_reindent(LvCtx *x, LvS s, int delta, bool skip_first, LvS *out);
bool lv_block_inner(LvCtx *x, const LvStmt *blk, i64 *le, i64 *ls);
bool lv_has_side(const LvNode *n);
bool lv_has_call(LvCtx *x, i32 a, i32 b);
typedef struct {
    const char **v;
    i32 n;
} LvIds;
LvIds lv_ids(LvCtx *x, i32 a, i32 b);
bool lv_ids_has(LvIds s, const char *p);
bool lv_ids_meet(LvIds s, LvIds t);
bool lv_ids_hasz(LvIds s, const char *z);
typedef struct {
    const char *key;
    LvIds ids;
    const char *last; /* NULL: None */
} LvWrite;
typedef AVP_VEC(LvWrite) LvWrites;
LvWrites lv_writes(LvCtx *x, i32 a, i32 b);
LvS lv_wrap(LvCtx *x, const LvNode *n, int need);
LvS lv_negate(LvCtx *x, const LvNode *n);
bool lv_mixed_ok(LvCtx *x);
bool lv_decl_ok_at(LvCtx *x, LvStmts L, i32 idx, const LvStmt *moving);
LvNode *lv_expr_of(LvCtx *x, const LvStmt *st);
LvNode *lv_cond_of(LvCtx *x, const LvStmt *st);
LvStmt *lv_sole(const LvStmt *st);
LvS lv_if_head(LvCtx *x, LvS ind, LvS cond);
/* Records one candidate (cand()); edits are copied. */
void lv_yield(LvCtx *x, const char *lever, i64 pos, const LvEdit *edits, size_t n);
#define LV_YIELD(x, lever, pos, ...) \
    lv_yield((x), (lever), (pos), (LvEdit[]){__VA_ARGS__}, sizeof((LvEdit[]){__VA_ARGS__}) / sizeof(LvEdit))
LvS lv_sz(const char *z);
LvS lv_tok_s(const LvTok *t);

/* the lever families */
void lv_decl_family(LvCtx *x, int which);
void lv_expr_family(LvCtx *x, int which);
void lv_control_family(LvCtx *x, int which);
void lv_temp_family(LvCtx *x, int which);
void lv_type_family(LvCtx *x);
void lv_order_family(LvCtx *x);

/* ---- fake-form filter ---- */
typedef struct {
    const char *k;
    i64 n;
} LvCount;
typedef AVP_VEC(LvCount) LvCounter;
LvCounter lv_fake_forms(AvpArena *a, LvS fn_text, const char *symbol);
i64 lv_counter_get(const LvCounter *c, const char *k);
/* Sorted names of the forms cand has more of than orig. */
AvpCzVec lv_fake_reasons(AvpArena *a, const LvCounter *orig, LvS cand_fn, const char *symbol);
bool lv_check_parses(AvpArena *a, LvS text, const char *symbol);
LvS lv_norm_text(AvpArena *a, LvS s); /* _norm */
