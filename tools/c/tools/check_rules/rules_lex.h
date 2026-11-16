/* rules_lex: the C/C++ lexer of tools/rules/check_rules.py (Tok, Comment, Directive, lex, _skip_quoted,
 * pair_brackets, ptext), shared by the rule checker and the tools that read source the same way (lever search,
 * draft pipeline, fragment finder). rules_lex.md documents the API and the Python it ports.
 *
 * Text is UTF-8 as check_rules reads it (open(..., encoding="utf-8", errors="replace"), universal newlines: no \r
 * reaches the lexer from a file; rl_read_source does that decoding). Token, comment and directive texts are views
 * into the lexed text (or static strings for the parser's placeholders), so the text must outlive the results.
 * Everything else is allocated in the arena passed in. */
#pragma once
#include "avp/avp.h"
#include <string.h>

/* ---- arena-backed vectors ------------------------------------------------------------------------------------
 * A struct { T *v; size_t n, cap; } grown inside an arena (avp_arena_realloc), so a whole parse is freed with its
 * arena. RL_PUSH returns a pointer to the new (zeroed) slot. */
#define RL_VEC(T) \
    struct {      \
        T *v;     \
        size_t n; \
        size_t cap; \
    }
void rl_vec_grow(AvpArena *a, void **v, size_t *cap, size_t need, size_t elem);
#define RL_PUSH(a, vp) \
    (rl_vec_grow((a), (void **)&(vp)->v, &(vp)->cap, (vp)->n + 1, sizeof(*(vp)->v)), \
     memset(&(vp)->v[(vp)->n], 0, sizeof(*(vp)->v)), &(vp)->v[(vp)->n++])

/* ---- records ----------------------------------------------------------------------------------------------- */
typedef enum { RL_ID, RL_NUM, RL_STR, RL_CHR, RL_OP } RlKind;
extern const char *const rl_kind_names[5]; /* "id", "num", "str", "chr", "op" (Tok.k) */

typedef struct {
    AvpStr t;   /* source text; str and chr keep their quotes */
    i32 line;   /* line where the token starts */
    u8 k;       /* RlKind */
} RlTok;
typedef RL_VEC(RlTok) RlToks;
typedef RL_VEC(const RlTok *) RlTokPtrs; /* a Python list of Tok objects (identity = pointer) */

typedef struct {
    i32 line, end_line;
    AvpStr text;   /* body without delimiters (// text has trailing \r removed) */
    bool trailing; /* a token precedes it on its first line (always true inside a directive) */
} RlComment;
typedef RL_VEC(RlComment) RlComments;

typedef struct {
    i32 line, end_line;
    AvpStr name; /* the word after #, or "" */
    AvpStr rest; /* the text after the name: comments and continuations turned into spaces, stripped */
    RlToks toks; /* lex(rest, line, top=False) */
} RlDirective;
typedef RL_VEC(RlDirective) RlDirectives;

typedef struct {
    RlToks toks;
    RlComments comments;
    RlDirectives directives;
    /* false: the Python raises AttributeError (a Unicode whitespace character outside the lexer's skip set, such as
     * U+00A0, in code); error_line is the line it stands on. */
    bool ok;
    i32 error_line;
} RlLexed;

/* lex(text, line, top): tokens, comments and directives in source order, with the `#if 0` arms cut. */
RlLexed rl_lex(AvpArena *a, AvpStr text, i32 line, bool top);

/* _skip_quoted(text, i, q): the byte index just past the literal opening at text[i] (quote q). */
size_t rl_skip_quoted(AvpStr text, size_t i, char q);

/* pair_brackets(toks): match[i] is the partner index of each matched (), [] and {} op token, -1 elsewhere. The
 * array has n entries (NULL-free even for n == 0). */
i32 *rl_pair_brackets(AvpArena *a, const RlTok *toks, size_t n);

/* ptext: the token texts joined with single spaces. */
AvpStr rl_ptext(AvpArena *a, const RlTok *toks, size_t n);
AvpStr rl_ptext_ptrs(AvpArena *a, const RlTok *const *toks, size_t n);

/* Token text tests. */
static inline bool rl_teq(const RlTok *t, const char *lit) {
    size_t n = strlen(lit);
    return t->t.n == n && memcmp(t->t.p, lit, n) == 0;
}
#define RL_TEQ(tok, lit) ((tok)->t.n == sizeof(lit) - 1 && memcmp((tok)->t.p, (lit), sizeof(lit) - 1) == 0)
/* t.t in (w0, w1, ...): words is a NULL-terminated list. */
bool rl_tin(const RlTok *t, const char *const *words);
bool rl_sin(AvpStr s, const char *const *words);

/* Reads a source file the way check_rules does: UTF-8 with errors="replace", universal newlines. false on an I/O
 * error (fs.h's avp_io_err). */
bool rl_read_source(AvpArena *a, const char *path, AvpStr *out);
