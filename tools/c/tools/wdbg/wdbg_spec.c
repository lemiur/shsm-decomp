/* The wdbg spec language: tokenizer, parser and tree-walking interpreter (SPEC.md documents the language). */
#include "wdbg_spec.h"

#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

const char *const wdbg_reg_names[WDBG_NREGS] = {
    "ContextFlags", "Dr0", "Dr1", "Dr2", "Dr3", "Dr6", "Dr7", "SegGs", "SegFs", "SegEs", "SegDs",
    "Edi", "Esi", "Ebx", "Edx", "Ecx", "Eax", "Ebp", "Eip", "SegCs", "EFlags", "Esp", "SegSs"};

/* ---- tokens ------------------------------------------------------------------------------------------------------ */

typedef enum { T_INT, T_STR, T_BYTES, T_NAME, T_OP, T_NL, T_EOF } TokKind;
typedef struct {
    TokKind k;
    AvpStr text; /* T_NAME, T_OP: the spelling; T_STR, T_BYTES: the value */
    i64 i;
    int line;
} Tok;
typedef AVP_VEC(Tok) TokVec;

typedef struct {
    AvpArena *a;
    const char *path;
    const char *err;
} Ctx;

static bool syntax(Ctx *c, int line, const char *msg) {
    c->err = avp_sprintf(c->a, "%s:%d: %s", c->path, line, msg);
    return false;
}

static int hexval(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static bool is_name_start(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; }
static bool is_name_char(char ch) { return is_name_start(ch) || (ch >= '0' && ch <= '9'); }

static const char *const ops3[] = {"//=", "<<=", ">>="};
static const char *const ops2[] = {"==", "!=", "<=", ">=", "//", "<<", ">>", "+=", "-=", "*=", "%=", "&=", "|=", "^="};
static const char ops1[] = "(),=<>+-*%&|^~";

static bool tokenize(Ctx *c, AvpStr src, TokVec *out) {
    const char *p = src.p, *e = src.p + src.n;
    int line = 1, depth = 0;
    bool line_has_tokens = false;
    while (p < e) {
        char ch = *p;
        if (ch == '\r' || ch == '\n') {
            if (ch == '\r' && p + 1 < e && p[1] == '\n') p++;
            p++;
            if (depth == 0 && line_has_tokens) {
                avp_vec_push(out, ((Tok){.k = T_NL, .line = line}));
                line_has_tokens = false;
            }
            line++;
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\f') {
            p++;
            continue;
        }
        if (ch == '#') {
            while (p < e && *p != '\r' && *p != '\n') p++;
            continue;
        }
        if (ch == '\\') { /* a backslash before the line end continues the line */
            const char *q = p + 1;
            if (q < e && *q == '\r') q++;
            if (q < e && *q == '\n') q++;
            if (q == p + 1 && q < e) return syntax(c, line, "unexpected character after line continuation character");
            p = q;
            line++;
            continue;
        }
        Tok t = {.line = line};
        if (ch >= '0' && ch <= '9') {
            const char *s = p;
            while (p < e && (is_name_char(*p))) p++;
            AvpStr lit = {s, (size_t)(p - s)};
            bool overflow = false;
            i64 v = 0;
            int base = 10;
            AvpStr digits = lit;
            if (lit.n > 2 && lit.p[0] == '0' && (lit.p[1] == 'x' || lit.p[1] == 'X')) base = 16, digits = (AvpStr){lit.p + 2, lit.n - 2};
            else if (lit.n > 2 && lit.p[0] == '0' && (lit.p[1] == 'b' || lit.p[1] == 'B')) base = 2, digits = (AvpStr){lit.p + 2, lit.n - 2};
            else if (lit.n > 2 && lit.p[0] == '0' && (lit.p[1] == 'o' || lit.p[1] == 'O')) base = 8, digits = (AvpStr){lit.p + 2, lit.n - 2};
            if (!avp_parse_int(digits, base, &v, &overflow) || overflow) return syntax(c, line, "invalid integer literal");
            t.k = T_INT;
            t.i = v;
            t.text = lit;
        } else if ((ch == 'x' || ch == 'X') && p + 1 < e && (p[1] == '"' || p[1] == '\'')) {
            char q = p[1];
            p += 2;
            AvpBuf b;
            avp_buf_init(&b, c->a);
            for (;;) {
                if (p >= e || *p == '\r' || *p == '\n') return syntax(c, line, "unterminated bytes literal");
                if (*p == q) {
                    p++;
                    break;
                }
                if (*p == ' ' || *p == '\t') {
                    p++;
                    continue;
                }
                int hi = hexval(*p), lo = p + 1 < e ? hexval(p[1]) : -1;
                if (hi < 0 || lo < 0) return syntax(c, line, "bytes literal: expected pairs of hex digits");
                avp_buf_putc(&b, (char)(hi * 16 + lo));
                p += 2;
            }
            t.k = T_BYTES;
            t.text = avp_buf_str(&b);
        } else if (ch == '"' || ch == '\'') {
            char q = ch;
            p++;
            AvpBuf b;
            avp_buf_init(&b, c->a);
            for (;;) {
                if (p >= e || *p == '\r' || *p == '\n') return syntax(c, line, "unterminated string literal");
                char d = *p++;
                if (d == q) break;
                if (d != '\\') {
                    avp_buf_putc(&b, d);
                    continue;
                }
                if (p >= e) return syntax(c, line, "unterminated string literal");
                char x = *p++;
                switch (x) {
                case 'n': avp_buf_putc(&b, '\n'); break;
                case 't': avp_buf_putc(&b, '\t'); break;
                case 'r': avp_buf_putc(&b, '\r'); break;
                case '0': avp_buf_putc(&b, '\0'); break;
                case '\\': avp_buf_putc(&b, '\\'); break;
                case '\'': avp_buf_putc(&b, '\''); break;
                case '"': avp_buf_putc(&b, '"'); break;
                case 'x': {
                    int hi = p < e ? hexval(p[0]) : -1, lo = p + 1 < e ? hexval(p[1]) : -1;
                    if (hi < 0 || lo < 0) return syntax(c, line, "string literal: \\x needs two hex digits");
                    avp_buf_putcp(&b, (u32)(hi * 16 + lo));
                    p += 2;
                    break;
                }
                default:
                    avp_buf_putc(&b, '\\');
                    avp_buf_putc(&b, x);
                }
            }
            t.k = T_STR;
            t.text = avp_buf_str(&b);
        } else if (is_name_start(ch)) {
            const char *s = p;
            while (p < e && is_name_char(*p)) p++;
            t.k = T_NAME;
            t.text = (AvpStr){s, (size_t)(p - s)};
        } else {
            size_t n = 0;
            for (size_t i = 0; i < sizeof ops3 / sizeof *ops3 && !n; i++)
                if (e - p >= 3 && memcmp(p, ops3[i], 3) == 0) n = 3;
            for (size_t i = 0; i < sizeof ops2 / sizeof *ops2 && !n; i++)
                if (e - p >= 2 && memcmp(p, ops2[i], 2) == 0) n = 2;
            if (!n && strchr(ops1, ch)) n = 1;
            if (!n) return syntax(c, line, avp_sprintf(c->a, "invalid character '%c'", ch));
            if (ch == '(') depth++;
            if (ch == ')' && --depth < 0) return syntax(c, line, "unmatched ')'");
            t.k = T_OP;
            t.text = (AvpStr){p, n};
            p += n;
        }
        line_has_tokens = true;
        avp_vec_push(out, t);
    }
    if (depth) return syntax(c, line, "'(' was never closed");
    if (line_has_tokens) avp_vec_push(out, ((Tok){.k = T_NL, .line = line}));
    avp_vec_push(out, ((Tok){.k = T_EOF, .line = line}));
    return true;
}

/* ---- syntax tree ------------------------------------------------------------------------------------------------- */

typedef enum { E_LIT, E_NAME, E_CALL, E_NEG, E_POS, E_INV, E_NOT, E_BIN, E_AND, E_OR, E_IF } ExprKind;
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_FLOORDIV, OP_MOD, OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
               OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE } BinOp;

typedef struct Expr {
    ExprKind k;
    BinOp op;
    WdbgVal lit;
    AvpStr name;
    struct Expr *a, *b, *c;
    struct Expr **args;
    size_t nargs;
    int line;
} Expr;

typedef enum { S_ASSIGN, S_OUT, S_WR, S_SNAP, S_IF, S_BP, S_BUILD } StmtKind;
typedef struct Stmt {
    StmtKind k;
    int line;
    AvpStr name;    /* S_ASSIGN */
    int aug;        /* S_ASSIGN: -1 plain, else the BinOp of an augmented assignment */
    Expr *e;        /* S_ASSIGN value, S_IF test, S_BP address */
    Expr **args;    /* S_OUT, S_WR, S_SNAP */
    size_t nargs;
    struct Stmt **body, **orelse;
    size_t nbody, norelse;
} Stmt;
typedef AVP_VEC(Stmt *) StmtVec;
typedef AVP_VEC(Expr *) ExprVec;

typedef struct {
    u32 addr;
    const Stmt *bp;
    bool dead; /* the handler failed once: it is not run again */
} Bp;

struct WdbgSpec {
    AvpArena *a;   /* the tree, variables */
    AvpArena *tmp; /* one statement's temporaries */
    const char *path;
    Stmt **top;
    size_t ntop;
    AvpMap *vars;   /* name -> WdbgVal* */
    AvpIMap *bpidx; /* addr -> index into bps */
    AVP_VEC(Bp) bps;
    AvpIMap *hits;
    const WdbgHost *host;
    u32 *const *regs; /* during a hit */
    int line;         /* the statement being run (an error names it) */
};

typedef struct {
    Ctx *c;
    const Tok *t;
    size_t i;
} Parser;

static const Tok *peek(Parser *p) { return &p->t[p->i]; }
static bool is_op(const Tok *t, const char *op) { return t->k == T_OP && avp_str_eq(t->text, avp_str(op)); }
static bool is_kw(const Tok *t, const char *kw) { return t->k == T_NAME && avp_str_eq(t->text, avp_str(kw)); }

static const char *const keywords[] = {"bp", "build", "end", "if", "elif", "else", "out", "wr", "snap", "and", "or", "not"};
static bool is_keyword(AvpStr s) {
    for (size_t i = 0; i < sizeof keywords / sizeof *keywords; i++)
        if (avp_str_eq(s, avp_str(keywords[i]))) return true;
    return false;
}

static Expr *new_expr(Parser *p, ExprKind k, int line) {
    Expr *e = AVP_NEW(p->c->a, Expr);
    e->k = k;
    e->line = line;
    return e;
}

static Expr *parse_expr(Parser *p);

static Expr *parse_primary(Parser *p) {
    const Tok *t = peek(p);
    if (t->k == T_INT || t->k == T_STR || t->k == T_BYTES) {
        p->i++;
        Expr *e = new_expr(p, E_LIT, t->line);
        e->lit = t->k == T_INT ? (WdbgVal){.k = WDBG_INT, .i = t->i}
                               : (WdbgVal){.k = t->k == T_STR ? WDBG_STR : WDBG_BYTES, .s = t->text};
        /* adjacent string literals concatenate, as Python's */
        while (t->k != T_INT && peek(p)->k == t->k) {
            e->lit.s = avp_str_concat(p->c->a, e->lit.s, peek(p)->text);
            p->i++;
        }
        return e;
    }
    if (is_op(t, "(")) {
        p->i++;
        Expr *e = parse_expr(p);
        if (!e) return NULL;
        if (!is_op(peek(p), ")")) return syntax(p->c, peek(p)->line, "expected ')'"), NULL;
        p->i++;
        return e;
    }
    if (t->k == T_NAME && !is_keyword(t->text)) {
        p->i++;
        if (is_op(peek(p), "(")) {
            p->i++;
            Expr *e = new_expr(p, E_CALL, t->line);
            e->name = t->text;
            ExprVec args = {0};
            if (!is_op(peek(p), ")")) {
                for (;;) {
                    Expr *x = parse_expr(p);
                    if (!x) return NULL;
                    avp_vec_push(&args, x);
                    if (is_op(peek(p), ",")) {
                        p->i++;
                        continue;
                    }
                    break;
                }
            }
            if (!is_op(peek(p), ")")) return syntax(p->c, peek(p)->line, "expected ')' or ','"), NULL;
            p->i++;
            e->nargs = args.n;
            e->args = avp_memdup(p->c->a, args.v, args.n * sizeof *args.v);
            avp_vec_free(&args);
            return e;
        }
        Expr *e = new_expr(p, E_NAME, t->line);
        e->name = t->text;
        return e;
    }
    return syntax(p->c, t->line, t->k == T_NL || t->k == T_EOF ? "expected an expression" : "invalid syntax"), NULL;
}

static Expr *parse_unary(Parser *p) {
    const Tok *t = peek(p);
    ExprKind k = is_op(t, "-") ? E_NEG : is_op(t, "+") ? E_POS : is_op(t, "~") ? E_INV : E_LIT;
    if (k == E_LIT) return parse_primary(p);
    p->i++;
    Expr *e = new_expr(p, k, t->line);
    e->a = parse_unary(p);
    return e->a ? e : NULL;
}

typedef struct {
    const char *op;
    BinOp bin;
} OpName;

/* One left-associative binary level: operands from `next`, operators from `ops`. */
static Expr *parse_level(Parser *p, Expr *(*next)(Parser *), const OpName *ops, size_t nops) {
    Expr *l = next(p);
    if (!l) return NULL;
    for (;;) {
        const Tok *t = peek(p);
        size_t i = 0;
        while (i < nops && !is_op(t, ops[i].op)) i++;
        if (i == nops) return l;
        p->i++;
        Expr *e = new_expr(p, E_BIN, t->line);
        e->op = ops[i].bin;
        e->a = l;
        e->b = next(p);
        if (!e->b) return NULL;
        l = e;
    }
}

static const OpName term_ops[] = {{"*", OP_MUL}, {"//", OP_FLOORDIV}, {"%", OP_MOD}};
static const OpName arith_ops[] = {{"+", OP_ADD}, {"-", OP_SUB}};
static const OpName shift_ops[] = {{"<<", OP_SHL}, {">>", OP_SHR}};
static const OpName band_ops[] = {{"&", OP_AND}};
static const OpName bxor_ops[] = {{"^", OP_XOR}};
static const OpName bor_ops[] = {{"|", OP_OR}};
static const OpName cmp_ops[] = {{"==", OP_EQ}, {"!=", OP_NE}, {"<", OP_LT}, {"<=", OP_LE}, {">", OP_GT}, {">=", OP_GE}};

static Expr *parse_term(Parser *p) { return parse_level(p, parse_unary, term_ops, 3); }
static Expr *parse_arith(Parser *p) { return parse_level(p, parse_term, arith_ops, 2); }
static Expr *parse_shift(Parser *p) { return parse_level(p, parse_arith, shift_ops, 2); }
static Expr *parse_band(Parser *p) { return parse_level(p, parse_shift, band_ops, 1); }
static Expr *parse_bxor(Parser *p) { return parse_level(p, parse_band, bxor_ops, 1); }
static Expr *parse_bor(Parser *p) { return parse_level(p, parse_bxor, bor_ops, 1); }

static Expr *parse_cmp(Parser *p) {
    Expr *l = parse_level(p, parse_bor, cmp_ops, 0);
    if (!l) return NULL;
    const Tok *t = peek(p);
    for (size_t i = 0; i < 6; i++) {
        if (!is_op(t, cmp_ops[i].op)) continue;
        p->i++;
        Expr *e = new_expr(p, E_BIN, t->line);
        e->op = cmp_ops[i].bin;
        e->a = l;
        e->b = parse_bor(p);
        if (!e->b) return NULL;
        for (size_t k = 0; k < 6; k++)
            if (is_op(peek(p), cmp_ops[k].op)) return syntax(p->c, t->line, "chained comparisons are not supported"), NULL;
        return e;
    }
    return l;
}

static Expr *parse_not(Parser *p) {
    const Tok *t = peek(p);
    if (!is_kw(t, "not")) return parse_cmp(p);
    p->i++;
    Expr *e = new_expr(p, E_NOT, t->line);
    e->a = parse_not(p);
    return e->a ? e : NULL;
}

static Expr *parse_bool(Parser *p, bool is_or) {
    Expr *l = is_or ? parse_bool(p, false) : parse_not(p);
    if (!l) return NULL;
    while (is_kw(peek(p), is_or ? "or" : "and")) {
        Expr *e = new_expr(p, is_or ? E_OR : E_AND, peek(p)->line);
        p->i++;
        e->a = l;
        e->b = is_or ? parse_bool(p, false) : parse_not(p);
        if (!e->b) return NULL;
        l = e;
    }
    return l;
}

/* expr := or_expr ['if' or_expr 'else' expr] (Python's conditional expression) */
static Expr *parse_expr(Parser *p) {
    Expr *l = parse_bool(p, true);
    if (!l) return NULL;
    if (!is_kw(peek(p), "if")) return l;
    Expr *e = new_expr(p, E_IF, peek(p)->line);
    p->i++;
    e->a = l;
    e->b = parse_bool(p, true);
    if (!e->b) return NULL;
    if (!is_kw(peek(p), "else")) return syntax(p->c, peek(p)->line, "expected 'else' after 'if' expression"), NULL;
    p->i++;
    e->c = parse_expr(p);
    return e->c ? e : NULL;
}

static bool expect_nl(Parser *p) {
    if (peek(p)->k == T_NL) {
        p->i++;
        return true;
    }
    return syntax(p->c, peek(p)->line, "expected the end of the line");
}

static bool parse_args(Parser *p, ExprVec *out) {
    for (;;) {
        Expr *x = parse_expr(p);
        if (!x) return false;
        avp_vec_push(out, x);
        if (!is_op(peek(p), ",")) return true;
        p->i++;
    }
}

static bool parse_block(Parser *p, StmtVec *out, bool top);

static Stmt *parse_stmt(Parser *p, bool top) {
    const Tok *t = peek(p);
    Stmt *s = AVP_NEW(p->c->a, Stmt);
    s->line = t->line;
    s->aug = -1;
    if (t->k != T_NAME) return syntax(p->c, t->line, "expected a statement"), NULL;
    if (is_kw(t, "bp")) {
        if (!top) return syntax(p->c, t->line, "'bp' inside a block"), NULL;
        p->i++;
        s->k = S_BP;
        if (!(s->e = parse_expr(p)) || !expect_nl(p)) return NULL;
        StmtVec body = {0};
        if (!parse_block(p, &body, false)) return NULL;
        if (!is_kw(peek(p), "end")) return syntax(p->c, peek(p)->line, "expected 'end' closing 'bp'"), NULL;
        p->i++;
        if (!expect_nl(p)) return NULL;
        s->nbody = body.n;
        s->body = avp_memdup(p->c->a, body.v, body.n * sizeof *body.v);
        avp_vec_free(&body);
        return s;
    }
    if (is_kw(t, "if") || is_kw(t, "elif")) { /* elif: reached through an else branch */
        p->i++;
        s->k = S_IF;
        if (!(s->e = parse_expr(p)) || !expect_nl(p)) return NULL;
        StmtVec body = {0}, orelse = {0};
        if (!parse_block(p, &body, false)) return NULL;
        if (is_kw(peek(p), "elif")) {
            /* elif: an else block holding one if, which consumes the 'end' */
            Stmt *inner = parse_stmt(p, false);
            if (!inner) return NULL;
            avp_vec_push(&orelse, inner);
        } else {
            if (is_kw(peek(p), "else")) {
                p->i++;
                if (!expect_nl(p) || !parse_block(p, &orelse, false)) return NULL;
            }
            if (!is_kw(peek(p), "end")) return syntax(p->c, peek(p)->line, "expected 'end' closing 'if'"), NULL;
            p->i++;
            if (!expect_nl(p)) return NULL;
        }
        s->nbody = body.n;
        s->body = avp_memdup(p->c->a, body.v, body.n * sizeof *body.v);
        s->norelse = orelse.n;
        s->orelse = avp_memdup(p->c->a, orelse.v, orelse.n * sizeof *orelse.v);
        avp_vec_free(&body);
        avp_vec_free(&orelse);
        return s;
    }
    if (is_kw(t, "build")) { /* build "Wii/1.0", "Wii/1.0a": the compiler builds this spec's addresses are valid for */
        if (!top) return syntax(p->c, t->line, "'build' inside a block"), NULL;
        p->i++;
        s->k = S_BUILD;
        ExprVec args = {0};
        if (!parse_args(p, &args)) return NULL;
        if (!args.n) return syntax(p->c, t->line, "build takes the compiler build names it is valid for"), NULL;
        s->nargs = args.n;
        s->args = avp_memdup(p->c->a, args.v, args.n * sizeof *args.v);
        avp_vec_free(&args);
        return expect_nl(p) ? s : NULL;
    }
    if (is_kw(t, "out") || is_kw(t, "wr") || is_kw(t, "snap")) {
        p->i++;
        s->k = is_kw(t, "out") ? S_OUT : is_kw(t, "wr") ? S_WR : S_SNAP;
        ExprVec args = {0};
        if (!parse_args(p, &args)) return NULL;
        if (s->k == S_WR && args.n != 2) return syntax(p->c, t->line, "wr takes an address and a bytes value"), NULL;
        if (s->k == S_SNAP && args.n != 1 && args.n != 3) return syntax(p->c, t->line, "snap takes a name, or a name, lo and hi"), NULL;
        s->nargs = args.n;
        s->args = avp_memdup(p->c->a, args.v, args.n * sizeof *args.v);
        avp_vec_free(&args);
        return expect_nl(p) ? s : NULL;
    }
    if (is_keyword(t->text)) return syntax(p->c, t->line, avp_sprintf(p->c->a, "unexpected '%.*s'", AVP_SF(t->text))), NULL;
    p->i++;
    const Tok *op = peek(p);
    static const OpName aug_ops[] = {{"+=", OP_ADD}, {"-=", OP_SUB}, {"*=", OP_MUL}, {"//=", OP_FLOORDIV}, {"%=", OP_MOD},
                                     {"&=", OP_AND}, {"|=", OP_OR}, {"^=", OP_XOR}, {"<<=", OP_SHL}, {">>=", OP_SHR}};
    if (!is_op(op, "=")) {
        size_t i = 0;
        while (i < sizeof aug_ops / sizeof *aug_ops && !is_op(op, aug_ops[i].op)) i++;
        if (i == sizeof aug_ops / sizeof *aug_ops) return syntax(p->c, t->line, "expected '=' after a name"), NULL;
        s->aug = (int)aug_ops[i].bin;
    }
    p->i++;
    s->k = S_ASSIGN;
    s->name = t->text;
    if (!(s->e = parse_expr(p))) return NULL;
    return expect_nl(p) ? s : NULL;
}

static bool parse_block(Parser *p, StmtVec *out, bool top) {
    for (;;) {
        const Tok *t = peek(p);
        if (t->k == T_EOF) return top ? true : syntax(p->c, t->line, "expected 'end'");
        if (!top && (is_kw(t, "end") || is_kw(t, "else") || is_kw(t, "elif"))) return true;
        if (top && (is_kw(t, "end") || is_kw(t, "else") || is_kw(t, "elif")))
            return syntax(p->c, t->line, avp_sprintf(p->c->a, "unexpected '%.*s'", AVP_SF(t->text)));
        Stmt *s = parse_stmt(p, top);
        if (!s) return false;
        avp_vec_push(out, s);
    }
}

WdbgSpec *wdbg_spec_parse(AvpArena *a, const char *path, AvpStr text, const char **err) {
    Ctx c = {a, path, NULL};
    TokVec toks = {0};
    if (!tokenize(&c, text, &toks)) {
        avp_vec_free(&toks);
        *err = c.err;
        return NULL;
    }
    Parser p = {&c, toks.v, 0};
    StmtVec top = {0};
    bool ok = parse_block(&p, &top, true);
    avp_vec_free(&toks);
    if (!ok) {
        avp_vec_free(&top);
        *err = c.err;
        return NULL;
    }
    WdbgSpec *s = AVP_NEW(a, WdbgSpec);
    s->a = a;
    s->tmp = avp_arena_new(0);
    s->path = path;
    s->ntop = top.n;
    s->top = avp_memdup(a, top.v, top.n * sizeof *top.v);
    avp_vec_free(&top);
    s->vars = avp_map_new();
    s->bpidx = avp_imap_new();
    s->hits = avp_imap_new();
    return s;
}

/* ---- values ------------------------------------------------------------------------------------------------------ */

#define RAISE(...) avp_throw(AVP_ERR_OTHER, __VA_ARGS__)

static const char *type_name(WdbgVal v) { return v.k == WDBG_INT ? "int" : v.k == WDBG_STR ? "str" : "bytes"; }

static AvpStr bytes_repr(AvpArena *a, AvpStr s) {
    bool has_sq = memchr(s.p, '\'', s.n) != NULL, has_dq = memchr(s.p, '"', s.n) != NULL;
    char q = has_sq && !has_dq ? '"' : '\'';
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, 'b');
    avp_buf_putc(&b, q);
    for (size_t i = 0; i < s.n; i++) {
        u8 ch = (u8)s.p[i];
        if (ch == (u8)q || ch == '\\') avp_buf_printf(&b, "\\%c", ch);
        else if (ch == '\t') avp_buf_putz(&b, "\\t");
        else if (ch == '\n') avp_buf_putz(&b, "\\n");
        else if (ch == '\r') avp_buf_putz(&b, "\\r");
        else if (ch < 0x20 || ch >= 0x7f) avp_buf_printf(&b, "\\x%02x", ch);
        else avp_buf_putc(&b, (char)ch);
    }
    avp_buf_putc(&b, q);
    return avp_buf_str(&b);
}

AvpStr wdbg_val_str(AvpArena *a, WdbgVal v) {
    if (v.k == WDBG_INT) return avp_str(avp_sprintf(a, "%lld", (long long)v.i));
    if (v.k == WDBG_STR) return v.s;
    return bytes_repr(a, v.s);
}

AvpStr wdbg_val_repr(AvpArena *a, WdbgVal v) {
    if (v.k == WDBG_STR) return avp_repr_str(a, v.s);
    return wdbg_val_str(a, v);
}

static size_t cp_len(AvpStr s) {
    size_t n = 0;
    for (size_t i = 0; i < s.n; i++) n += ((u8)s.p[i] & 0xc0) != 0x80;
    return n;
}

static bool truthy(WdbgVal v) { return v.k == WDBG_INT ? v.i != 0 : v.s.n != 0; }

static WdbgVal vint(i64 i) { return (WdbgVal){.k = WDBG_INT, .i = i}; }
static WdbgVal vstr(AvpStr s) { return (WdbgVal){.k = WDBG_STR, .s = s}; }
static WdbgVal vbytes(AvpStr s) { return (WdbgVal){.k = WDBG_BYTES, .s = s}; }

/* ---- % formatting ------------------------------------------------------------------------------------------------ */

static bool fmt_fail(const char **err, AvpArena *a, const char *msg) {
    *err = avp_strdup(a, msg);
    return false;
}

bool wdbg_format(AvpArena *a, AvpStr fmt, const WdbgVal *args, size_t n, AvpStr *out, const char **err) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t ai = 0;
    size_t i = 0;
    while (i < fmt.n) {
        char ch = fmt.p[i];
        if (ch != '%') {
            avp_buf_putc(&b, ch);
            i++;
            continue;
        }
        size_t start = i++;
        if (i < fmt.n && fmt.p[i] == '(') return fmt_fail(err, a, "TypeError: format requires a mapping");
        AvpBuf spec;
        avp_buf_init(&spec, a);
        avp_buf_putc(&spec, '%');
        while (i < fmt.n && strchr("-+ #0", fmt.p[i])) avp_buf_putc(&spec, fmt.p[i++]);
        for (int part = 0; part < 2; part++) {
            if (part == 1) {
                if (i >= fmt.n || fmt.p[i] != '.') break;
                avp_buf_putc(&spec, fmt.p[i++]);
            }
            if (i < fmt.n && fmt.p[i] == '*') {
                i++;
                if (ai >= n) return fmt_fail(err, a, "TypeError: not enough arguments for format string");
                if (args[ai].k != WDBG_INT) return fmt_fail(err, a, "TypeError: * wants int");
                avp_buf_printf(&spec, "%lld", (long long)args[ai++].i);
            } else {
                while (i < fmt.n && fmt.p[i] >= '0' && fmt.p[i] <= '9') avp_buf_putc(&spec, fmt.p[i++]);
            }
        }
        while (i < fmt.n && strchr("hlL", fmt.p[i])) i++;
        if (i >= fmt.n) return fmt_fail(err, a, "ValueError: incomplete format");
        char type = fmt.p[i++];
        if (type == '%') {
            avp_buf_putc(&b, '%');
            continue;
        }
        if (!strchr("diouxXeEfFgGcrsa", type)) {
            *err = avp_sprintf(a, "ValueError: unsupported format character '%c' (0x%x) at index %zu", type, (unsigned)(u8)type,
                               i - 1);
            return false;
        }
        (void)start;
        if (ai >= n) return fmt_fail(err, a, "TypeError: not enough arguments for format string");
        WdbgVal v = args[ai++];
        if (type == 'i' || type == 'u') type = 'd';
        if (strchr("dxXo", type)) {
            if (v.k != WDBG_INT) {
                *err = avp_sprintf(a, "TypeError: %%%c format: %s is required, not %s", type,
                                   type == 'd' ? "a real number" : "an integer", type_name(v));
                return false;
            }
            avp_buf_putc(&spec, type);
            avp_buf_pyfmt(&b, avp_buf_cz(&spec), (long long)v.i);
        } else if (strchr("eEfFgG", type)) {
            if (v.k != WDBG_INT) {
                *err = avp_sprintf(a, "TypeError: must be real number, not %s", type_name(v));
                return false;
            }
            avp_buf_putc(&spec, type);
            avp_buf_pyfmt(&b, avp_buf_cz(&spec), (double)v.i);
        } else if (type == 'c') {
            if (v.k == WDBG_INT) {
                if (v.i < 0 || v.i > 0x10ffff) return fmt_fail(err, a, "OverflowError: %c arg not in range(0x110000)");
                avp_buf_putc(&spec, 'c');
                avp_buf_pyfmt(&b, avp_buf_cz(&spec), (long long)v.i);
            } else if (v.k == WDBG_STR && v.s.n && cp_len(v.s) == 1) {
                avp_buf_putc(&spec, 's');
                avp_buf_pyfmt(&b, avp_buf_cz(&spec), avp_cz(a, v.s));
            } else {
                *err = avp_sprintf(a, "TypeError: %%c requires an int or a unicode character, not %s", type_name(v));
                return false;
            }
        } else {
            AvpStr text = type == 's' ? wdbg_val_str(a, v) : wdbg_val_repr(a, v);
            avp_buf_putc(&spec, 's');
            avp_buf_pyfmt(&b, avp_buf_cz(&spec), avp_cz(a, text));
        }
    }
    if (ai < n) return fmt_fail(err, a, "TypeError: not all arguments converted during string formatting");
    *out = avp_buf_str(&b);
    return true;
}

/* ---- evaluation -------------------------------------------------------------------------------------------------- */

static i64 need_int(WdbgVal v, const char *what) {
    if (v.k != WDBG_INT) RAISE("TypeError: %s: an integer is required, not %s", what, type_name(v));
    return v.i;
}

static WdbgVal eval(WdbgSpec *s, const Expr *e);

static size_t read_mem(WdbgSpec *s, i64 addr, void *buf, size_t n) {
    if (addr < 0 || addr > 0xffffffffLL) return 0;
    return s->host->read(s->host->ctx, (u32)addr, buf, n);
}

static int reg_index(AvpStr name) {
    for (int i = 0; i < WDBG_NREGS; i++)
        if (avp_str_eq(name, avp_str(wdbg_reg_names[i]))) return i;
    return -1;
}

static WdbgVal binop(WdbgSpec *s, BinOp op, WdbgVal l, WdbgVal r) {
    static const char *const names[] = {"+", "-", "*", "//", "%", "&", "|", "^", "<<", ">>", "==", "!=", "<", "<=", ">", ">="};
    if (op == OP_EQ || op == OP_NE) {
        bool eq = l.k == r.k && (l.k == WDBG_INT ? l.i == r.i : avp_str_eq(l.s, r.s));
        return vint(op == OP_EQ ? eq : !eq);
    }
    if (op >= OP_LT) {
        int c;
        if (l.k == WDBG_INT && r.k == WDBG_INT) c = l.i < r.i ? -1 : l.i > r.i;
        else if (l.k == r.k) {
            size_t m = l.s.n < r.s.n ? l.s.n : r.s.n;
            c = memcmp(l.s.p, r.s.p, m);
            if (!c) c = l.s.n < r.s.n ? -1 : l.s.n > r.s.n;
        } else {
            RAISE("TypeError: '%s' not supported between instances of '%s' and '%s'", names[op], type_name(l), type_name(r));
        }
        bool res = op == OP_LT ? c < 0 : op == OP_LE ? c <= 0 : op == OP_GT ? c > 0 : c >= 0;
        return vint(res);
    }
    if (op == OP_ADD && l.k == r.k && l.k != WDBG_INT) {
        AvpStr cat = avp_str_concat(s->tmp, l.s, r.s);
        return (WdbgVal){.k = l.k, .s = cat};
    }
    if (l.k != WDBG_INT || r.k != WDBG_INT)
        RAISE("TypeError: unsupported operand type(s) for %s: '%s' and '%s'", names[op], type_name(l), type_name(r));
    i64 a = l.i, b = r.i;
    switch (op) {
    case OP_ADD: return vint((i64)((u64)a + (u64)b));
    case OP_SUB: return vint((i64)((u64)a - (u64)b));
    case OP_MUL: return vint((i64)((u64)a * (u64)b));
    case OP_FLOORDIV:
    case OP_MOD: {
        if (b == 0) RAISE("ZeroDivisionError: %s", op == OP_MOD ? "integer modulo by zero" : "integer division or modulo by zero");
        i64 q = a / b, m = a % b;
        if (m != 0 && ((m < 0) != (b < 0))) q--, m += b;
        return vint(op == OP_MOD ? m : q);
    }
    case OP_AND: return vint(a & b);
    case OP_OR: return vint(a | b);
    case OP_XOR: return vint(a ^ b);
    case OP_SHL:
    case OP_SHR:
        if (b < 0) RAISE("ValueError: negative shift count");
        if (op == OP_SHR) return vint(b >= 64 ? (a < 0 ? -1 : 0) : a >> b);
        return vint(b >= 64 ? 0 : (i64)((u64)a << b));
    default: break;
    }
    avp_panic(__FILE__, __LINE__, "binop %d", (int)op);
}

static WdbgVal call(WdbgSpec *s, const Expr *e) {
    AvpArena *a = s->tmp;
    WdbgVal av[16];
    size_t n = e->nargs;
    if (n > 16) RAISE("TypeError: too many arguments");
    for (size_t i = 0; i < n; i++) av[i] = eval(s, e->args[i]);
    AvpStr f = e->name;
#define IS(x) avp_str_eq(f, AVP_S(x))
#define NARGS(lo, hi)                                                                                                \
    do {                                                                                                             \
        if (n < (lo) || n > (hi)) RAISE("TypeError: %.*s() takes %d to %d arguments (%zu given)", AVP_SF(f), lo, hi, n); \
    } while (0)
    if (IS("u8") || IS("u16") || IS("u32")) {
        NARGS(1, 1);
        size_t w = IS("u8") ? 1 : IS("u16") ? 2 : 4;
        u8 buf[4] = {0};
        i64 addr = need_int(av[0], avp_cz(a, f));
        if (read_mem(s, addr, buf, w) != w) return vint(0);
        return vint((i64)buf[0] | (i64)buf[1] << 8 | (i64)buf[2] << 16 | (i64)buf[3] << 24);
    }
    if (IS("rd")) {
        NARGS(2, 2);
        i64 addr = need_int(av[0], "rd"), cnt = need_int(av[1], "rd");
        if (cnt < 0 || cnt > (1 << 26)) RAISE("ValueError: rd: bad length");
        char *buf = avp_alloc(a, (size_t)cnt + 1);
        size_t got = read_mem(s, addr, buf, (size_t)cnt);
        return vbytes((AvpStr){buf, got});
    }
    if (IS("hits")) {
        NARGS(1, 1);
        i64 addr = need_int(av[0], "hits");
        AvpVal *v = addr >= 0 ? avp_imap_get(s->hits, (u64)addr) : NULL;
        if (!v) RAISE("KeyError: %lld", (long long)addr);
        return vint(v->i);
    }
    if (IS("env")) {
        NARGS(1, 2);
        if (av[0].k != WDBG_STR) RAISE("TypeError: str expected, not %s", type_name(av[0]));
        wchar_t *wn = avp_to_w(a, av[0].s);
        DWORD need = GetEnvironmentVariableW(wn, NULL, 0);
        if (need == 0) {
            if (n == 2) return av[1];
            RAISE("KeyError: %s", avp_cz(a, avp_repr_str(a, av[0].s)));
        }
        wchar_t *wv = avp_alloc(a, ((size_t)need + 1) * sizeof(wchar_t));
        DWORD got = GetEnvironmentVariableW(wn, wv, need + 1);
        return vstr(avp_from_w(a, wv, got));
    }
    if (IS("int")) {
        NARGS(1, 2);
        if (n == 1 && av[0].k == WDBG_INT) return av[0];
        int base = n == 2 ? (int)need_int(av[1], "int") : 10;
        if (av[0].k != WDBG_STR) RAISE("TypeError: int() can't convert non-string with explicit base");
        i64 v = 0;
        bool overflow = false;
        if (!avp_parse_int(av[0].s, base, &v, &overflow) || overflow)
            RAISE("ValueError: invalid literal for int() with base %d: %s", base, avp_cz(a, avp_repr_str(a, av[0].s)));
        return vint(v);
    }
    if (IS("str")) {
        NARGS(1, 1);
        return vstr(wdbg_val_str(a, av[0]));
    }
    if (IS("repr")) {
        NARGS(1, 1);
        return vstr(wdbg_val_repr(a, av[0]));
    }
    if (IS("hex")) {
        NARGS(1, 1);
        if (av[0].k != WDBG_INT) RAISE("TypeError: '%s' object cannot be interpreted as an integer", type_name(av[0]));
        return vstr(avp_pyfmt(a, "%#x", (long long)av[0].i));
    }
    if (IS("sym")) {
        NARGS(1, 1);
        if (av[0].k != WDBG_INT) return vstr(wdbg_val_str(a, av[0])); /* the Python's except: hex(a) fails too */
        return vstr(s->host->sym(s->host->ctx, a, av[0].i));
    }
    if (IS("compiler")) { /* the compiler build of this run */
        NARGS(0, 0);
        return vstr(avp_str(s->host->build ? s->host->build : ""));
    }
    if (IS("addr")) { /* the address of a compiler function of this build, by its NB11 name */
        NARGS(1, 1);
        if (av[0].k != WDBG_STR) RAISE("TypeError: addr() needs a function name, not %s", type_name(av[0]));
        i64 va = 0;
        if (!s->host->lookup || !s->host->lookup(s->host->ctx, a, av[0].s, &va))
            RAISE("LookupError: compiler build %s has no function named '%.*s' (NB11 names: build/tools/mwcc_symbols.exe)",
                  s->host->build ? s->host->build : "?", AVP_SF(av[0].s));
        return vint(va);
    }
    if (IS("cstr")) { /* b.split(b' ')[0].decode('latin1') */
        NARGS(1, 1);
        if (av[0].k != WDBG_BYTES) RAISE("TypeError: cstr() needs bytes, not %s", type_name(av[0]));
        size_t m = 0;
        while (m < av[0].s.n && av[0].s.p[m]) m++;
        char *buf = avp_alloc(a, 2 * m + 1);
        size_t w = 0;
        for (size_t i = 0; i < m; i++) {
            u8 ch = (u8)av[0].s.p[i];
            if (ch < 0x80) {
                buf[w++] = (char)ch;
            } else {
                buf[w++] = (char)(0xC0 | (ch >> 6));
                buf[w++] = (char)(0x80 | (ch & 0x3F));
            }
        }
        return vstr((AvpStr){buf, w});
    }
    if (IS("len")) {
        NARGS(1, 1);
        if (av[0].k == WDBG_INT) RAISE("TypeError: object of type 'int' has no len()");
        return vint(av[0].k == WDBG_STR ? (i64)cp_len(av[0].s) : (i64)av[0].s.n);
    }
    if (IS("fmt")) {
        if (n < 1 || av[0].k != WDBG_STR) RAISE("TypeError: fmt() needs a format string");
        AvpStr out;
        const char *err;
        if (!wdbg_format(a, av[0].s, av + 1, n - 1, &out, &err)) RAISE("%s", err);
        return vstr(out);
    }
    if (IS("le8") || IS("le16") || IS("le32")) {
        NARGS(1, 1);
        size_t w = IS("le8") ? 1 : IS("le16") ? 2 : 4;
        i64 v = need_int(av[0], avp_cz(a, f));
        i64 lim = (i64)1 << (8 * w);
        if (v < -(lim / 2) || v >= lim) RAISE("struct.error: argument out of range");
        char *buf = avp_alloc(a, 4);
        for (size_t i = 0; i < w; i++) buf[i] = (char)(u8)((u64)v >> (8 * i));
        return vbytes((AvpStr){buf, w});
    }
#undef IS
#undef NARGS
    RAISE("NameError: name '%.*s' is not defined", AVP_SF(f));
}

static WdbgVal eval(WdbgSpec *s, const Expr *e) {
    switch (e->k) {
    case E_LIT: return e->lit;
    case E_NAME: {
        if (s->regs) {
            int r = reg_index(e->name);
            if (r >= 0) return vint(*s->regs[r]);
        }
        AvpVal *v = avp_map_get(s->vars, e->name);
        if (!v) RAISE("NameError: name '%.*s' is not defined", AVP_SF(e->name));
        return *(WdbgVal *)v->p;
    }
    case E_CALL: return call(s, e);
    case E_NEG: return vint((i64)(0 - (u64)need_int(eval(s, e->a), "unary -")));
    case E_POS: return vint(need_int(eval(s, e->a), "unary +"));
    case E_INV: return vint(~need_int(eval(s, e->a), "unary ~"));
    case E_NOT: return vint(!truthy(eval(s, e->a)));
    case E_AND: {
        WdbgVal l = eval(s, e->a);
        return truthy(l) ? eval(s, e->b) : l;
    }
    case E_OR: {
        WdbgVal l = eval(s, e->a);
        return truthy(l) ? l : eval(s, e->b);
    }
    case E_IF: return truthy(eval(s, e->b)) ? eval(s, e->a) : eval(s, e->c);
    case E_BIN: {
        WdbgVal l = eval(s, e->a);
        WdbgVal r = eval(s, e->b);
        return binop(s, e->op, l, r);
    }
    }
    avp_panic(__FILE__, __LINE__, "expr %d", (int)e->k);
}

static void assign(WdbgSpec *s, AvpStr name, WdbgVal v) {
    if (s->regs) {
        int r = reg_index(name);
        if (r >= 0) {
            if (v.k != WDBG_INT) RAISE("TypeError: int expected instead of %s", type_name(v));
            *s->regs[r] = (u32)v.i;
            return;
        }
    }
    bool created = false;
    AvpVal *slot = avp_map_put(s->vars, avp_str_dup(s->a, name), &created);
    if (created) slot->p = AVP_NEW(s->a, WdbgVal);
    WdbgVal *dst = slot->p;
    if (v.k != WDBG_INT) v.s = avp_str_dup(s->a, v.s);
    *dst = v;
}

static void exec_block(WdbgSpec *s, Stmt *const *body, size_t n);

static void exec(WdbgSpec *s, const Stmt *st) {
    AvpArena *a = s->tmp;
    s->line = st->line;
    switch (st->k) {
    case S_BUILD: {
        const char *have = s->host->build ? s->host->build : "";
        AvpBuf want;
        avp_buf_init(&want, a);
        for (size_t i = 0; i < st->nargs; i++) {
            WdbgVal v = eval(s, st->args[i]);
            if (v.k != WDBG_STR) RAISE("TypeError: build takes strings, not %s", type_name(v));
            if (avp_str_eq(v.s, avp_str(have))) return;
            if (i) avp_buf_putz(&want, ", ");
            avp_buf_put(&want, v.s);
        }
        RAISE("BuildError: this spec's addresses are for compiler build %s; this run compiles with %s (the addresses "
              "differ between builds: name them with addr(\"Function\") + offset, and list the build after checking)",
              avp_buf_cz(&want), have);
    }
    case S_ASSIGN: {
        WdbgVal v = eval(s, st->e);
        if (st->aug >= 0) {
            WdbgVal cur;
            int r = s->regs ? reg_index(st->name) : -1;
            if (r >= 0) cur = vint(*s->regs[r]);
            else {
                AvpVal *old = avp_map_get(s->vars, st->name);
                if (!old) RAISE("NameError: name '%.*s' is not defined", AVP_SF(st->name));
                cur = *(WdbgVal *)old->p;
            }
            v = binop(s, (BinOp)st->aug, cur, v);
        }
        assign(s, st->name, v);
        return;
    }
    case S_OUT: {
        WdbgVal first = eval(s, st->args[0]);
        AvpStr text;
        if (st->nargs == 1) {
            text = wdbg_val_str(a, first);
        } else {
            if (first.k != WDBG_STR)
                RAISE("TypeError: unsupported operand type(s) for %%: '%s' and 'tuple'", type_name(first));
            WdbgVal *rest = AVP_NEWN(a, WdbgVal, st->nargs - 1);
            for (size_t i = 1; i < st->nargs; i++) rest[i - 1] = eval(s, st->args[i]);
            const char *err;
            if (!wdbg_format(a, first.s, rest, st->nargs - 1, &text, &err)) RAISE("%s", err);
        }
        s->host->out(s->host->ctx, text);
        return;
    }
    case S_WR: {
        WdbgVal addr = eval(s, st->args[0]);
        WdbgVal data = eval(s, st->args[1]);
        i64 at = need_int(addr, "wr");
        if (data.k != WDBG_BYTES) RAISE("TypeError: a bytes-like object is required, not '%s'", type_name(data));
        if (at >= 0 && at <= 0xffffffffLL) s->host->write(s->host->ctx, (u32)at, data.s.p, data.s.n);
        return;
    }
    case S_SNAP: {
        WdbgVal name = eval(s, st->args[0]);
        i64 lo = 0x687000, hi = 0x7c8000;
        if (st->nargs == 3) {
            lo = need_int(eval(s, st->args[1]), "snap");
            hi = need_int(eval(s, st->args[2]), "snap");
        }
        if (name.k != WDBG_STR) RAISE("TypeError: can only concatenate str (not \"%s\") to str", type_name(name));
        const char *err = NULL;
        if (!s->host->snap(s->host->ctx, name.s, lo, hi, &err)) RAISE("%s", err);
        return;
    }
    case S_IF:
        if (truthy(eval(s, st->e))) exec_block(s, st->body, st->nbody);
        else exec_block(s, st->orelse, st->norelse);
        return;
    case S_BP: {
        i64 addr = need_int(eval(s, st->e), "bp");
        if (addr < 0 || addr > 0xffffffffLL) RAISE("ValueError: breakpoint address out of range: %lld", (long long)addr);
        bool created = false;
        AvpVal *ix = avp_imap_put(s->bpidx, (u64)addr, &created);
        if (created) {
            ix->i = (i64)s->bps.n;
            avp_vec_push(&s->bps, ((Bp){(u32)addr, st}));
        } else {
            s->bps.v[ix->i].bp = st;
        }
        return;
    }
    }
}

static void exec_block(WdbgSpec *s, Stmt *const *body, size_t n) {
    for (size_t i = 0; i < n; i++) exec(s, body[i]);
}

bool wdbg_spec_load(WdbgSpec *s, const WdbgHost *host, const char **err) {
    s->host = host;
    s->regs = NULL;
    AvpArenaMark m = avp_arena_mark(s->tmp);
    AvpTry t;
    bool ok;
    if (AVP_TRY(&t)) {
        exec_block(s, s->top, s->ntop);
        avp_try_end(&t);
        ok = true;
    } else {
        *err = avp_strdup(s->a, avp_sprintf(s->tmp, "%s (%s:%d)", t.msg, avp_cz(s->tmp, avp_path_basename(s->path)), s->line));
        ok = false;
    }
    avp_arena_reset(s->tmp, m);
    return ok;
}

size_t wdbg_spec_bp_count(const WdbgSpec *s) { return s->bps.n; }
u32 wdbg_spec_bp_addr(const WdbgSpec *s, size_t i) { return s->bps.v[i].addr; }
bool wdbg_spec_has_bp(const WdbgSpec *s, u32 addr) { return avp_imap_get(s->bpidx, addr) != NULL; }

i64 wdbg_spec_hits(const WdbgSpec *s, u32 addr) {
    AvpVal *v = avp_imap_get(s->hits, addr);
    return v ? v->i : 0;
}

bool wdbg_spec_hit(WdbgSpec *s, u32 addr, u32 *const regs[WDBG_NREGS]) {
    AvpVal *ix = avp_imap_get(s->bpidx, addr);
    if (!ix) return true;
    bool created = false;
    avp_imap_put(s->hits, addr, &created)->i += 1;
    const Stmt *bp = s->bps.v[ix->i].bp;
    if (s->bps.v[ix->i].dead) return false;
    s->regs = regs;
    AvpArenaMark m = avp_arena_mark(s->tmp);
    AvpTry t;
    bool ok;
    if (AVP_TRY(&t)) {
        exec_block(s, bp->body, bp->nbody);
        avp_try_end(&t);
        ok = true;
    } else {
        /* once: the handler is stopped, so a failing spec does not print the same error at every event */
        s->bps.v[ix->i].dead = true;
        AvpStr line = avp_str(avp_sprintf(s->tmp, "HANDLER ERR %s (%s:%d, bp %#x; this handler is stopped)", t.msg,
                                          avp_cz(s->tmp, avp_path_basename(s->path)), s->line, addr));
        s->host->out(s->host->ctx, line);
        ok = false;
    }
    avp_arena_reset(s->tmp, m);
    s->regs = NULL;
    return ok;
}
