/* rules_parse: the parse model of tools/rules/check_rules.py (Func, Agg, VarDecl, Proto, uses_cpp_syntax,
 * scan_storage_macros, Parsed, _Parser), shared by the rule checker and the tools that find function bodies the same
 * way (they use `Parsed(path, text)`). rules_lex.md documents the API and the Python it ports.
 *
 * A parse lives in the arena given to rl_parse (one arena per file lets a caller free it). Token indices (head,
 * lparen, body, ...) index P->toks, the stream with the asm bodies masked. */
#pragma once
#include "rules_lex.h"
#include "shsm_re.h"

typedef struct {
    AvpStr name; /* "" when the parameter has no name */
    AvpStr type; /* type text (ptext) */
    bool reg;    /* `register` among its tokens */
} RlParam;
typedef RL_VEC(RlParam) RlParams;

typedef struct {
    AvpStr name;          /* qualified (Cls::name, operator==, <anon>) */
    i32 line, end_line;   /* first head token's line (else the '{' line); the '}' line */
    i32 head;             /* index of the first head token */
    i32 lparen, rparen;   /* the parameter list's '(' and its match (rparen = lparen when unmatched) */
    i32 body, body_end;   /* '{' and '}' */
    bool is_asm, is_static, virtual_, template_, anon_ns;
    RlParams params;
    AvpStr in_class;      /* the enclosing class's qualified name, or "" */
    bool has_addr;        /* Func.addr is not None (set by the checker, never by the parser) */
    u32 addr;
} RlFunc;
typedef RL_VEC(RlFunc) RlFuncs;

typedef struct {
    AvpStr type, name;
    i32 line;
} RlMemberDecl;

typedef struct {
    AvpStr kind, name; /* struct/class/union/enum; qualified name or "" */
    i32 line, keyword_line;
    RL_VEC(AvpStr) members;               /* ptext of each data-member statement */
    RL_VEC(RlMemberDecl) member_decls;    /* (type_text, name, line) */
    i64 first_hidden;                     /* -1 for None */
    AvpStr body_text;
} RlAgg;
typedef RL_VEC(RlAgg) RlAggs;

typedef struct {
    AvpStr name, type_text, ns;
    i32 line;
    bool is_extern, is_static, is_const, has_init, addr_bound, in_function;
} RlVar;
typedef RL_VEC(RlVar) RlVars;

typedef struct {
    AvpStr name;
    i32 line;
    bool is_extern, empty_params;
} RlProto;
typedef RL_VEC(RlProto) RlProtos;

typedef struct {
    AvpStr kind, name;
    i32 line;
} RlFwd;
typedef RL_VEC(RlFwd) RlFwds;

typedef struct {
    AvpStr name;
    i32 line;
} RlNameLine; /* typedefs and virtuals: (name, line) */
typedef RL_VEC(RlNameLine) RlNameLines;

typedef struct {
    i32 line;
    AvpStr name; /* the enclosing function, or "" */
    AvpStr kind; /* "function", "block" or "token:<word>" */
} RlAsmSite;

typedef struct {
    i32 lo, hi; /* token range [lo, hi) of the raw stream replaced by __ASM_BODY__ ; */
} RlMask;

typedef struct {
    i32 tok;        /* index of the __ASM_BODY__ token in the masked stream (Python keys by id(token)) */
    AvpStrVec ids;  /* sorted distinct identifier texts of the masked range */
} RlAsmIds;

/* What one _Parser run collects (the seven lists nested class parsers share). */
typedef struct {
    RlFuncs funcs;
    RlAggs aggs;
    RlVars vars;
    RlFwds fwds;
    RlProtos protos;
    RlNameLines typedefs;
    RlNameLines virtuals;
} RlParseOut;

/* Storage-class macros: name -> RL_STORAGE_STATIC / RL_STORAGE_EXTERN in AvpVal.i. */
enum { RL_STORAGE_NONE = 0, RL_STORAGE_STATIC = 1, RL_STORAGE_EXTERN = 2 };

typedef struct RlParsed {
    AvpArena *a;
    AvpStr path, text;
    const AvpMap *storage_global; /* the storage_macros argument (may be NULL) */
    AvpMap *storage_own;          /* scan_storage_macros(text): wins over the argument */
    bool is_header, is_cpp;
    AvpStrVec lines; /* text.split("\n") (views) */
    /* false: lex raised (rl_lex's ok); the lists below are then empty */
    bool ok;
    i32 error_line;
    RlToks toks;  /* masked stream */
    i32 *match;   /* pair_brackets(toks) */
    RlComments comments;
    RlDirectives directives;
    RL_VEC(RlAsmSite) asm_sites;
    RL_VEC(RlMask) masks;
    RL_VEC(RlAsmIds) asm_ids;
    RlParseOut p;          /* funcs, aggs, vars, fwds, protos, typedefs, virtuals */
    AvpSet *func_names;    /* f.name.split("::")[-1] */
    AvpStrVec includes;
    /* _comments_by_line: comment indices covering each line, CSR over lines 0..cbl_lines-1 */
    i32 cbl_lines;
    i32 *cbl_start; /* cbl_lines + 1 offsets into cbl_idx */
    i32 *cbl_idx;
    /* memos of the derived facts */
    int cpp_syntax_memo; /* -1 not computed */
    AvpSet *offset_macros_memo;
    AvpSet *volatile_noted_memo;
    void *cpp_classes_memo; /* rules_classes.h: rl_cpp_classes() */
} RlParsed;

/* scan_storage_macros(text), merged into m (later definitions overwrite). */
void rl_scan_storage_macros(AvpMap *m, AvpStr text);
/* P.storage_macros.get(name): RL_STORAGE_NONE when absent. */
int rl_storage_get(const RlParsed *P, AvpStr name);

/* Parsed(path, text, storage_macros). path is the repository-relative path (its suffix gives is_header, is_cpp). */
RlParsed *rl_parse(AvpArena *a, AvpStr path, AvpStr text, const AvpMap *storage_macros);
/* Frees the heap parts of a parse (its maps, sets and AvpStrVecs); the arena holds the rest. */
void rl_parsed_free(RlParsed *P);

/* uses_cpp_syntax(toks) */
bool rl_uses_cpp_syntax(const RlTok *toks, size_t n);

/* Derived facts (memoized on P): cpp_syntax(), offset_macros(), volatile_noted(). */
bool rl_cpp_syntax(RlParsed *P);
AvpSet *rl_offset_macros(RlParsed *P);
AvpSet *rl_volatile_noted(RlParsed *P);
/* Checker._volatile_decl_name(toks, i): the declared name, or {NULL, 0} for None. */
AvpStr rl_volatile_decl_name(const RlTok *toks, size_t n, size_t i);

/* Helpers: comments_on (indices into P->comments; *count set), has_note, func_at (index or -1),
 * no_copy_in_retail. */
const i32 *rl_comments_on(const RlParsed *P, i32 line, size_t *count);
bool rl_has_note(const RlParsed *P, i32 line, const ShsmRe *re, bool above);
i64 rl_func_at(const RlParsed *P, i32 line);
bool rl_no_copy_in_retail(const RlParsed *P, const RlFunc *fn);

/* The check_rules.py regexes the helpers are used with. */
const ShsmRe *rl_re_volatile_note(void); /* VOLATILE_NOTE */
const ShsmRe *rl_re_delay_note(void);    /* DELAY_NOTE */
const ShsmRe *rl_re_assert_note(void);   /* the assert-only volatile local's note (ruling of 2026-11-15) */
const ShsmRe *rl_re_byteptr_note(void);  /* BYTEPTR_NOTE */
const ShsmRe *rl_re_pad_note(void);      /* PAD_NOTE */
const ShsmRe *rl_re_exception(void);     /* EXCEPTION_RE */

/* The facts dump of one parse (the stage-1 proof; rules_lex.md "Facts dump"): one JSON object, compact. */
void rl_dump_facts(AvpJsonW *w, RlParsed *P);
