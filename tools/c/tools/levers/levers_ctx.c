/* levers: FnContext (levers.py): construction, text and semantics helpers, apply/cand, generate. */
#include "levers.h"
#include "shsm_mangle.h"

#include <string.h>

const char *const LV_FAMILIES[6] = {"decl", "expr", "control", "temp", "type", "order"};

static u64 P(const char *p) { return (u64)(uintptr_t)p; }

/* ---- text helpers ---- */

LvS lv_slice(LvCtx *x, i64 s, i64 e) {
    i64 n = (i64)x->text.n;
    if (s < 0) s = 0;
    if (e > n) e = n;
    if (s > n) s = n;
    if (e < s) e = s;
    return (LvS){x->text.p + s, (size_t)(e - s)};
}

i64 lv_line_start(LvCtx *x, i64 pos) {
    if (pos > (i64)x->text.n) pos = (i64)x->text.n;
    for (i64 k = pos - 1; k >= 0; k--)
        if (x->text.p[k] == '\n') return k + 1;
    return 0;
}

i64 lv_line_end(LvCtx *x, i64 pos) {
    for (i64 k = pos < 0 ? 0 : pos; k < (i64)x->text.n; k++)
        if (x->text.p[k] == '\n') return k + 1;
    return (i64)x->text.n;
}

LvS lv_src(LvCtx *x, i32 a, i32 b) { return lv_slice(x, lv_at(x->code, a)->s, lv_at(x->code, b)->e); }
LvS lv_nt(LvCtx *x, const LvNode *n) { return lv_src(x, n->a, n->b); }

const char *lv_norm(LvCtx *x, i32 a, i32 b) {
    AvpBuf buf;
    avp_buf_init(&buf, x->a);
    for (i32 k = a; k <= b; k++) {
        if (k > a) avp_buf_putc(&buf, ' ');
        const LvTok *t = lv_at(x->code, k);
        avp_buf_putn(&buf, t->t, t->n);
    }
    return avp_buf_cz(&buf);
}

bool lv_lines(LvCtx *x, i32 a, i32 b, i64 *ls_out, i64 *le_out) {
    i64 s = lv_at(x->code, a)->s, e = lv_at(x->code, b)->e;
    i64 ls = lv_line_start(x, s);
    if (!lv_blank(lv_slice(x, ls, s))) return false;
    /* a directive line just above belongs to the statement: the statement's lines start at the directive */
    for (bool up = true; up;) {
        up = false;
        for (i32 q = 0; q < x->nppl; q++)
            if (x->ppl[q].le == ls) {
                ls = x->ppl[q].ls;
                up = true;
                break;
            }
    }
    i64 le = lv_line_end(x, e);
    LvS tail = lv_strip(lv_slice(x, e, le));
    if (tail.n && !lv_s_startswith(tail, LVS("//"))) return false;
    *ls_out = ls;
    *le_out = le;
    return true;
}

LvS lv_indent(LvCtx *x, i32 a) {
    i64 s = lv_at(x->code, a)->s;
    return lv_slice(x, lv_line_start(x, s), s);
}

static LvS indent_unit(LvCtx *x) {
    if (x->body.n) {
        LvS ind = lv_indent(x, x->body.v[0]->a);
        if (ind.n && ind.p[0] == '\t') return LVS("\t");
        LvS head = lv_slice(x, x->span0, lv_at(x->code, x->fn.head)->s);
        if (ind.n > head.n && lv_blank(ind)) return (LvS){ind.p + head.n, ind.n - head.n};
    }
    return LVS("    ");
}

/* str.splitlines(keepends=True) for latin-1 text. */
static size_t line_break_len(LvS s, size_t i) {
    u8 c = (u8)s.p[i];
    if (c == '\r') return (i + 1 < s.n && s.p[i + 1] == '\n') ? 2 : 1;
    if (c == '\n' || c == 0x0b || c == 0x0c || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x85) return 1;
    return 0;
}

bool lv_reindent(LvCtx *x, LvS s, int delta, bool skip_first, LvS *out) {
    AvpBuf b;
    avp_buf_init(&b, x->a);
    size_t i = 0, idx = 0;
    while (i < s.n) {
        size_t j = i, br = 0;
        while (j < s.n && !(br = line_break_len(s, j))) j++;
        size_t end = j < s.n ? j + br : s.n;
        LvS line = {s.p + i, end - i};
        if (lv_blank(line) || (skip_first && idx == 0)) avp_buf_put(&b, line);
        else if (delta > 0) {
            avp_buf_put(&b, x->unit);
            avp_buf_put(&b, line);
        } else if (lv_s_startswith(line, x->unit))
            avp_buf_put(&b, (LvS){line.p + x->unit.n, line.n - x->unit.n});
        else
            return false;
        i = end;
        idx++;
    }
    char *z = avp_buf_cz(&b);
    *out = (LvS){z, b.n};
    return true;
}

bool lv_block_inner(LvCtx *x, const LvStmt *blk, i64 *le_out, i64 *ls_out) {
    if (blk->kind != LS_BLOCK) return false;
    const LvTok *lb = lv_at(x->code, blk->a), *rb = lv_at(x->code, blk->b);
    i64 le = lv_line_end(x, lb->e);
    if (!lv_blank(lv_slice(x, lb->e, le))) return false;
    i64 ls = lv_line_start(x, rb->s);
    if (!lv_blank(lv_slice(x, ls, rb->s)) || ls < le) return false;
    *le_out = le;
    *ls_out = ls;
    return true;
}

/* ---- semantics helpers ---- */

bool lv_has_side(const LvNode *n) {
    if (n->kind == LN_CALL || n->kind == LN_ASSIGN) return true;
    if ((n->kind == LN_UNARY || n->kind == LN_POST) && (!strcmp(n->op, "++") || !strcmp(n->op, "--"))) return true;
    for (i32 k = 0; k < n->nk; k++)
        if (lv_has_side(n->kids[k])) return true;
    return false;
}

static bool cast_paren(LvCtx *x, i32 k) {
    AvpTry t;
    bool r = false;
    if (AVP_TRY(&t)) {
        LvExprParser p;
        lv_expr_init(&p, x->a, x->code, x->m, k, x->code.n - 1, x->typenames);
        r = lv_expr_is_cast(&p, k);
        avp_try_end(&t);
    } else {
        if (t.code != LV_EXC_PARSE && t.code != LV_EXC_KEY) avp_throw(t.code, "%s", t.msg);
        r = false;
    }
    return r;
}

bool lv_has_call(LvCtx *x, i32 a, i32 b) {
    for (i32 k = a; k <= b; k++) {
        const LvTok *t = lv_at(x->code, k);
        if (!TEQ(t, "(") || k <= a) continue;
        const LvTok *pv = lv_at(x->code, k - 1);
        if (!(pv->k == LV_ID || TEQ(pv, ")") || TEQ(pv, "]"))) continue;
        if (lv_in(pv->t, LV_NOT_TYPE) || TEQ(pv, "sizeof")) continue;
        if (cast_paren(x, k)) continue;
        return true;
    }
    return false;
}

LvIds lv_ids(LvCtx *x, i32 a, i32 b) {
    AVP_VEC(const char *) v = {0};
    for (i32 k = a; k <= b; k++) {
        const LvTok *t = lv_at(x->code, k);
        if (t->k != LV_ID) continue;
        bool have = false;
        for (size_t q = 0; q < v.n; q++)
            if (v.v[q] == t->t) {
                have = true;
                break;
            }
        if (!have) avp_vec_push(&v, t->t);
    }
    LvIds r = {v.n ? avp_memdup(x->a, v.v, sizeof(char *) * v.n) : NULL, (i32)v.n};
    avp_vec_free(&v);
    return r;
}

bool lv_ids_has(LvIds s, const char *p) {
    for (i32 i = 0; i < s.n; i++)
        if (s.v[i] == p) return true;
    return false;
}
bool lv_ids_hasz(LvIds s, const char *z) {
    for (i32 i = 0; i < s.n; i++)
        if (!strcmp(s.v[i], z)) return true;
    return false;
}
bool lv_ids_meet(LvIds s, LvIds t) {
    for (i32 i = 0; i < s.n; i++)
        if (lv_ids_has(t, s.v[i])) return true;
    return false;
}

LvWrites lv_writes(LvCtx *x, i32 a, i32 b) {
    LvWrites out = {0};
    LvNodes w = {0};
    for (size_t q = 0; q < x->exprs.n; q++) {
        LvNode *n = x->exprs.v[q].n;
        if (n->a < a || n->b > b) continue;
        w.n = 0;
        lv_walk(n, &w);
        for (size_t i = 0; i < w.n; i++) {
            LvNode *e = w.v[i];
            LvNode *tgt = NULL;
            if (e->kind == LN_ASSIGN) tgt = e->kids[0];
            else if ((e->kind == LN_UNARY || e->kind == LN_POST) && (!strcmp(e->op, "++") || !strcmp(e->op, "--")))
                tgt = e->kids[0];
            else if (e->kind == LN_UNARY && !strcmp(e->op, "&"))
                tgt = e->kids[0];
            if (tgt) {
                const LvTok *lt = lv_at(x->code, tgt->b);
                LvWrite wr = {lv_norm(x, tgt->a, tgt->b), lv_ids(x, tgt->a, tgt->b), lt->k == LV_ID ? lt->t : NULL};
                avp_vec_push(&out, wr);
            }
        }
    }
    avp_vec_free(&w);
    return out;
}

LvS lv_wrap(LvCtx *x, const LvNode *n, int need) {
    LvS t = lv_nt(x, n);
    return lv_prec(n) >= need ? t : LV_CAT(x->a, LVS("("), t, LVS(")"));
}

LvS lv_negate(LvCtx *x, const LvNode *n) {
    if (n->kind == LN_BIN && (!strcmp(n->op, "==") || !strcmp(n->op, "!="))) {
        const LvTok *o = lv_at(x->code, n->opi);
        return LV_CAT(x->a, lv_slice(x, lv_at(x->code, n->a)->s, o->s), !strcmp(n->op, "==") ? LVS("!=") : LVS("=="),
                      lv_slice(x, o->e, lv_at(x->code, n->b)->e));
    }
    if (n->kind == LN_UNARY && !strcmp(n->op, "!")) {
        LvNode *k = n->kids[0];
        return k->kind == LN_PAREN ? lv_nt(x, k->kids[0]) : lv_nt(x, k);
    }
    if (lv_prec(n) >= 14) return LV_CAT(x->a, LVS("!"), lv_wrap(x, n, 14));
    return LV_CAT(x->a, LVS("!("), lv_nt(x, n), LVS(")"));
}

/* ---- construction ---- */

static void add_expr(LvCtx *x, LvStmt *s, const char *role, LvRange r) {
    if (!r.ok) return;
    AvpTry t;
    LvNode *n = NULL;
    if (AVP_TRY(&t)) {
        LvExprParser p;
        lv_expr_init(&p, x->a, x->code, x->m, r.a, r.b, x->typenames);
        n = lv_expr_parse(&p);
        avp_try_end(&t);
    } else {
        if (!lv_exc_caught(t.code)) avp_throw(t.code, "%s", t.msg);
        return;
    }
    avp_vec_push(&x->exprs, ((LvExprEntry){s, role, n}));
}

static void collect_exprs(LvCtx *x) {
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        switch (s->kind) {
        case LS_EXPR: add_expr(x, s, "expr", s->expr); break;
        case LS_RETURN: add_expr(x, s, "return", s->expr); break;
        case LS_DECL:
            for (i32 d = 0; d < s->decl->ndtors; d++)
                if (s->decl->dtors[d].init.ok && !s->decl->dtors[d].complex) add_expr(x, s, "init", s->decl->dtors[d].init);
            break;
        case LS_IF:
        case LS_WHILE:
        case LS_SWITCH:
        case LS_DO: add_expr(x, s, "cond", (LvRange){s->lp + 1, s->rp - 1, true}); break;
        case LS_FOR:
            if (s->init.ok) {
                LvDecl *d = lv_parse_decl(x->a, x->code, s->init.a, s->init.b + 1, x->m);
                if (d) {
                    for (i32 k = 0; k < d->ndtors; k++)
                        if (d->dtors[k].init.ok && !d->dtors[k].complex) add_expr(x, s, "init", d->dtors[k].init);
                } else
                    add_expr(x, s, "for-init", s->init);
            }
            add_expr(x, s, "cond", s->cond);
            add_expr(x, s, "step", s->step);
            break;
        default: break;
        }
    }
}

static void note(LvCtx *x, const char *const *words, i32 nw, const LvDtor *dt) {
    if (dt->array) return;
    bool sub_int = true, any_float = false;
    for (i32 i = 0; i < nw; i++) {
        if (!lv_in(words[i], LV_INT_WORDS)) sub_int = false;
        if (lv_in(words[i], LV_FLOAT_WORDS)) any_float = true;
    }
    if (dt->ptr || sub_int) avp_iset_add(x->int_names, P(dt->name));
    if (any_float && !dt->ptr) avp_iset_add(x->float_names, P(dt->name));
}

static void collect_types(LvCtx *x) {
    for (i32 i = 0; i < x->fn.nparams; i++) {
        i32 pa = x->fn.params[i].a, pb = x->fn.params[i].b;
        LvDecl *d = pb > pa ? lv_parse_decl(x->a, x->code, pa, pb, x->m) : NULL;
        if (d && d->ndtors == 1) note(x, d->words, d->nwords, &d->dtors[0]);
    }
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        if (s->kind == LS_DECL) {
            for (i32 k = 0; k < s->decl->ndtors; k++) {
                LvDtor *dt = &s->decl->dtors[k];
                note(x, s->decl->words, s->decl->nwords, dt);
                bool have = false;
                for (size_t j = 0; j < x->locals.n; j++)
                    if (x->locals.v[j].name == dt->name) have = true;
                if (!have) avp_vec_push(&x->locals, ((LvLocal){dt->name, s, dt}));
            }
        } else if (s->kind == LS_FOR && s->init.ok) {
            LvDecl *d = lv_parse_decl(x->a, x->code, s->init.a, s->init.b + 1, x->m);
            if (d)
                for (i32 k = 0; k < d->ndtors; k++) note(x, d->words, d->nwords, &d->dtors[k]);
        }
    }
}

/* re.search(r"\b(?:if|while|for|switch) \( ", s) */
static bool pad_search(LvS s) {
    static const char *const kw[] = {"if", "while", "for", "switch"};
    for (size_t i = 0; i < s.n; i++) {
        if (i > 0 && lv_isword((u8)s.p[i - 1])) continue;
        for (int k = 0; k < 4; k++) {
            size_t n = strlen(kw[k]);
            if (i + n + 3 <= s.n && !memcmp(s.p + i, kw[k], n) && !memcmp(s.p + i + n, " ( ", 3)) {
                /* \b before the keyword: the keyword starts with a word character */
                return true;
            }
        }
    }
    return false;
}

/* ---- preprocessor directives inside the body ----
 * A `#line N` before a statement sets the __LINE__ its allocator-tag macros see, so it belongs to the statement that
 * follows. The directive is an opaque line: a variant keeps it text-for-text, in order, and the statement after it
 * keeps every identifier, number and string of its head (an edit may fold more into it, never take one out or put a
 * different statement there). */

/* `#line ...`: '#', optional blanks, the word line, then a blank or the end. */
static bool pp_is_line(const LvTok *t) {
    const char *p = t->t + 1;
    while (*p == ' ' || *p == '\t') p++;
    return !strncmp(p, "line", 4) && (p[4] == 0 || p[4] == ' ' || p[4] == '\t');
}

static bool key_has(const char **v, i32 n, const char *s) {
    for (i32 i = 0; i < n; i++)
        if (v[i] == s) return true;
    return false;
}

/* The key of the statement after the directive toks.v[pi]: its identifier, number and string tokens, up to its
 * first top-level ';' or '{'. */
static i32 pp_key(AvpArena *a, LvToks t, i32 pi, const char ***out) {
    const char **v = avp_alloc(a, sizeof(char *) * (size_t)(t.n + 1));
    i32 n = 0, depth = 0, j = pi + 1;
    while (j < t.n && (t.v[j].k == LV_COMMENT || t.v[j].k == LV_PP)) j++;
    for (; j < t.n; j++) {
        const LvTok *k = &t.v[j];
        if (k->k == LV_OP) {
            if (TEQ(k, "(") || TEQ(k, "[")) depth++;
            else if (TEQ(k, ")") || TEQ(k, "]")) {
                if (--depth < 0) break;
            } else if (depth == 0 && (TEQ(k, ";") || TEQ(k, "{") || TEQ(k, "}")))
                break;
        } else if ((k->k == LV_ID || k->k == LV_NUM || k->k == LV_STR) && !key_has(v, n, k->t))
            v[n++] = k->t;
    }
    *out = v;
    return n;
}

static void pp_collect(LvCtx *x, i64 bs, i64 be) {
    i32 cnt = 0;
    for (i32 i = 0; i < x->toks.n; i++) {
        const LvTok *t = &x->toks.v[i];
        if (t->k != LV_PP || t->s < bs || t->s >= be) continue;
        if (!pp_is_line(t)) lv_parse_error("preprocessor directive %s inside the function body (only #line is supported)", t->t);
        cnt++;
    }
    x->pp = avp_alloc(x->a, sizeof(LvPp) * (size_t)(cnt ? cnt : 1));
    x->ppl = avp_alloc(x->a, sizeof(LvPpLine) * (size_t)(cnt ? cnt : 1));
    for (i32 i = 0; i < x->toks.n; i++) {
        const LvTok *t = &x->toks.v[i];
        if (t->k != LV_PP || t->s < bs || t->s >= be) continue;
        LvPp *p = &x->pp[x->npp++];
        p->text = t->t;
        p->nkey = pp_key(x->a, x->toks, i, &p->key);
        i64 ls = lv_line_start(x, t->s), le = lv_line_end(x, t->e);
        if (lv_blank(lv_slice(x, ls, t->s)) && lv_blank(lv_slice(x, t->e, le)))
            x->ppl[x->nppl++] = (LvPpLine){ls, le};
    }
}

/* True when the variant text keeps the function's directives as they were: the same lines in the same order, each
 * followed by a statement whose head still holds all of the old statement's identifiers, numbers and strings. */
static bool pp_preserved(LvCtx *x, LvS res) {
    if (!x->npp) return true;
    i64 e1 = x->span1 + (i64)res.n - (i64)x->text.n;
    if (e1 < x->span0 || e1 > (i64)res.n) return false;
    LvToks nt = lv_tokenize(x->a, (LvS){res.p + x->span0, (size_t)(e1 - x->span0)});
    i32 k = 0;
    for (i32 i = 0; i < nt.n; i++) {
        if (nt.v[i].k != LV_PP) continue;
        if (k >= x->npp || strcmp(nt.v[i].t, x->pp[k].text)) return false;
        const char **key;
        i32 nk = pp_key(x->a, nt, i, &key);
        for (i32 q = 0; q < x->pp[k].nkey; q++)
            if (!key_has(key, nk, x->pp[k].key[q])) return false;
        k++;
    }
    return k == x->npp;
}

/* ---- the definition of a symbol ---- */

static const char *g_addr_symbol;
static u64 g_addr_value;

void lv_set_symbol_address(const char *symbol, u64 addr) {
    g_addr_symbol = symbol ? lv_internz(symbol) : NULL;
    g_addr_value = addr;
}

/* By name first (the mangled name's source name and class, as match.exe and permute.exe read it), then by the
 * `// 0x<address>` annotation of the project. The error says which lookups were tried. */
static LvFnDef find_fn(LvCtx *x, const char *symbol) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        LvFnDef d = lv_find_definition(x->a, x->code, x->m, symbol);
        avp_try_end(&t);
        return d;
    }
    if (t.code != LV_EXC_PARSE || strncmp(t.msg, "no definition of", 16) != 0) avp_throw(t.code, "%s", t.msg);
    u64 addr = 0;
    bool have = lv_symbol_address(symbol, &addr);
    if (!have && g_addr_symbol && g_addr_value && !strcmp(g_addr_symbol, symbol)) {
        addr = g_addr_value;
        have = true;
    }
    const char *hint = !strncmp(symbol, "__sinit_", 8) ? " (a static initialiser has no source definition)"
                       : !strncmp(symbol, "__dt__", 6) || !strncmp(symbol, "__ct__", 6)
                           ? " (an implicit or in-class special member has no out-of-line definition here)"
                           : "";
    if (!have)
        lv_parse_error("%s; no `// 0xADDRESS` lookup: the symbol is not fn_<address> and symbols.txt gave no address%s",
                       t.msg, hint);
    LvFnDef d;
    if (lv_find_definition_at(x->a, x->text, x->toks, x->code, x->m, addr, &d)) return d;
    lv_parse_error("%s; no definition has a header or closing comment ending `// 0x%08llX` either%s", t.msg,
                   (unsigned long long)addr, hint);
}

LvCtx *lv_ctx_new(AvpArena *a, LvS text, const char *symbol) {
    LvCtx *x = avp_alloc(a, sizeof *x);
    x->a = a;
    x->text = text;
    x->symbol = symbol;
    x->toks = lv_tokenize(a, text);
    x->code = lv_code_tokens(a, x->toks);
    x->m = lv_match_brackets(a, x->code);
    x->fn = find_fn(x, symbol);
    LvToks c = x->code;
    x->lb = x->fn.lbrace;
    x->rb = x->fn.rbrace;
    i64 bs = lv_at(c, x->lb)->s, be = lv_at(c, x->rb)->e;
    x->span0 = lv_line_start(x, lv_at(c, x->fn.head)->s);
    x->span1 = be;
    pp_collect(x, bs, be);
    x->body = lv_body_parse(a, c, x->m, x->lb + 1, x->rb);
    AVP_VEC(LvTok) cm = {0};
    for (i32 i = 0; i < x->toks.n; i++)
        if (x->toks.v[i].k == LV_COMMENT && x->span0 <= x->toks.v[i].s && x->toks.v[i].s < be)
            avp_vec_push(&cm, x->toks.v[i]);
    x->comments = (LvToks){cm.n ? avp_memdup(a, cm.v, sizeof(LvTok) * cm.n) : NULL, (i32)cm.n};
    avp_vec_free(&cm);
    x->typenames = lv_collect_typenames(c);
    x->nl = lv_s_contains(text, LVS("\r\n")) ? LVS("\r\n") : LVS("\n");
    x->pad = pad_search(lv_slice(x, x->span0, be)) ? LVS(" ") : LVS("");
    x->unit = indent_unit(x);
    x->file_has_do = false;
    for (i32 i = 0; i < c.n; i++)
        if (TEQ(&c.v[i], "do")) x->file_has_do = true;
    x->mixed = -1;
    x->file_ids = avp_iset_new();
    for (i32 i = 0; i < c.n; i++)
        if (c.v[i].k == LV_ID) avp_iset_add(x->file_ids, P(c.v[i].t));
    x->body_ids = avp_imap_new();
    for (i32 k = x->lb + 1; k < x->rb; k++)
        if (c.v[k].k == LV_ID) avp_imap_put(x->body_ids, P(c.v[k].t), NULL)->i += 1;
    lv_iter_stmts(a, x->body, &x->all_stmts);
    lv_iter_blocks(x->body, NULL, &x->blocks);
    collect_exprs(x);
    x->int_names = avp_iset_new();
    x->float_names = avp_iset_new();
    collect_types(x);
    return x;
}

/* ---- mixed_ok ---- */

bool lv_mixed_ok(LvCtx *x) {
    if (x->mixed >= 0) return x->mixed != 0;
    ShsmDemangled *p = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        p = shsm_mangle_parse(x->a, x->symbol);
        avp_try_end(&t);
    } else {
        avp_throw(LV_EXC_DEMANGLE, "%s", t.msg);
    }
    bool mixed = p != NULL;
    for (i32 i = 0; !mixed && i < x->code.n; i++) {
        const LvTok *k = &x->code.v[i];
        if (TEQ(k, "class") || TEQ(k, "::") || TEQ(k, "namespace") || TEQ(k, "template")) mixed = true;
    }
    x->mixed = mixed;
    if (!mixed) {
        LvFnDefs defs = lv_scan_definitions(x->a, x->code, x->m);
        for (size_t di = 0; di < defs.n; di++) {
            LvStmts body = {0};
            AvpTry t2;
            bool ok;
            if (AVP_TRY(&t2)) {
                body = lv_body_parse(x->a, x->code, x->m, defs.v[di].lbrace + 1, defs.v[di].rbrace);
                avp_try_end(&t2);
                ok = true;
            } else {
                if (!lv_exc_caught(t2.code)) avp_throw(t2.code, "%s", t2.msg);
                ok = false;
            }
            if (!ok) continue;
            LvBlocks bl = {0};
            lv_iter_blocks(body, NULL, &bl);
            for (size_t b = 0; b < bl.n; b++) {
                LvStmts L = bl.v[b].L;
                i32 last = -1;
                for (i32 i = 0; i < L.n; i++)
                    if (L.v[i]->kind == LS_DECL) last = i;
                if (last < 0) continue;
                for (i32 i = 0; i < last; i++)
                    if (L.v[i]->kind != LS_DECL) {
                        x->mixed = 1;
                        avp_vec_free(&bl);
                        avp_vec_free(&defs);
                        return true;
                    }
            }
            avp_vec_free(&bl);
        }
        avp_vec_free(&defs);
    }
    return x->mixed != 0;
}

bool lv_decl_ok_at(LvCtx *x, LvStmts L, i32 idx, const LvStmt *moving) {
    if (lv_mixed_ok(x)) return true;
    for (i32 i = 0; i < idx && i < L.n; i++)
        if (L.v[i] != moving && L.v[i]->kind != LS_DECL) return false;
    return true;
}

LvNode *lv_expr_of(LvCtx *x, const LvStmt *st) {
    for (size_t q = 0; q < x->exprs.n; q++)
        if (x->exprs.v[q].s == st && (!strcmp(x->exprs.v[q].role, "expr") || !strcmp(x->exprs.v[q].role, "return")))
            return x->exprs.v[q].n;
    return NULL;
}

LvNode *lv_cond_of(LvCtx *x, const LvStmt *st) {
    for (size_t q = 0; q < x->exprs.n; q++)
        if (x->exprs.v[q].s == st && !strcmp(x->exprs.v[q].role, "cond")) return x->exprs.v[q].n;
    return NULL;
}

LvStmt *lv_sole(const LvStmt *st) {
    if (st->kind == LS_BLOCK) return st->list.n == 1 ? st->list.v[0] : NULL;
    return (LvStmt *)st;
}

LvS lv_if_head(LvCtx *x, LvS ind, LvS cond) {
    return LV_CAT(x->a, ind, LVS("if"), LVS(" ("), x->pad, cond, x->pad, LVS(") {"), x->nl);
}

/* ---- edits ---- */

static int edit_cmp(const void *pa, const void *pb) {
    const LvEdit *a = pa, *b = pb;
    if (a->s != b->s) return a->s < b->s ? -1 : 1;
    if (a->e != b->e) return a->e < b->e ? -1 : 1;
    return 0;
}

static bool apply(LvCtx *x, const LvEdit *in, size_t n, LvS *out) {
    LvEdit *ed = avp_memdup(x->a, in, sizeof(LvEdit) * n);
    /* stable insertion sort by (s, e) */
    for (size_t i = 1; i < n; i++) {
        LvEdit k = ed[i];
        size_t j = i;
        while (j > 0 && edit_cmp(&ed[j - 1], &k) > 0) {
            ed[j] = ed[j - 1];
            j--;
        }
        ed[j] = k;
    }
    for (size_t i = 0; i + 1 < n; i++)
        if (ed[i + 1].s < ed[i].e) return false;
    LvS *parts = avp_alloc(x->a, sizeof(LvS) * (n ? n : 1));
    for (size_t i = 0; i < n; i++) parts[i] = ed[i].text;
    LvS added = lv_catv(x->a, parts, n);
    for (size_t i = 0; i < n; i++)
        for (i32 c = 0; c < x->comments.n; c++) {
            const LvTok *cm = &x->comments.v[c];
            if (ed[i].s <= cm->s && cm->s < ed[i].e && !lv_s_contains(added, lv_tok_s(cm))) return false;
        }
    AvpBuf b;
    avp_buf_init(&b, x->a);
    i64 pos = 0;
    for (size_t i = 0; i < n; i++) {
        avp_buf_put(&b, lv_slice(x, pos, ed[i].s));
        avp_buf_put(&b, ed[i].text);
        pos = ed[i].e;
    }
    avp_buf_put(&b, lv_slice(x, pos, (i64)x->text.n));
    char *z = avp_buf_cz(&b);
    LvS res = {z, b.n};
    if (lv_s_eq(res, x->text)) return false;
    if (!pp_preserved(x, res)) return false;
    *out = res;
    return true;
}

void lv_yield(LvCtx *x, const char *lever, i64 pos, const LvEdit *edits, size_t n) {
    LvS nw;
    if (!apply(x, edits, n, &nw)) return;
    i64 line = 1;
    for (i64 k = 0; k < pos && k < (i64)x->text.n; k++)
        if (x->text.p[k] == '\n') line++;
    i64 nn = (i64)nw.n, hi = pos < nn ? pos : nn, ls = 0, le = -1;
    for (i64 k = hi - 1; k >= 0; k--)
        if (nw.p[k] == '\n') {
            ls = k + 1;
            break;
        }
    for (i64 k = pos < 0 ? 0 : pos; k < nn; k++)
        if (nw.p[k] == '\n') {
            le = k;
            break;
        }
    i64 de = le >= 0 ? le : nn;
    LvS desc = ls < de ? (LvS){nw.p + ls, (size_t)(de - ls)} : (LvS){nw.p, 0};
    desc = lv_strip(desc);
    if (desc.n > 100) desc.n = 100;
    LvCandidate cd = {x->cur_family, lever, line, nw, desc, x->span0, x->span1 + nn - (i64)x->text.n};
    avp_vec_push(x->out, cd);
}

LvCandidates lv_generate(LvCtx *x, const char *const *families, size_t nfam) {
    LvCandidates out = {0};
    if (!nfam) {
        families = LV_FAMILIES;
        nfam = 6;
    }
    x->out = &out;
    for (size_t f = 0; f < nfam; f++) {
        const char *fam = families[f];
        x->cur_family = fam;
        if (!strcmp(fam, "decl"))
            for (int g = 0; g < 6; g++) lv_decl_family(x, g);
        else if (!strcmp(fam, "expr"))
            for (int g = 0; g < 6; g++) lv_expr_family(x, g);
        else if (!strcmp(fam, "control"))
            for (int g = 0; g < 8; g++) lv_control_family(x, g);
        else if (!strcmp(fam, "temp"))
            for (int g = 0; g < 2; g++) lv_temp_family(x, g);
        else if (!strcmp(fam, "type"))
            lv_type_family(x);
        else if (!strcmp(fam, "order"))
            lv_order_family(x);
        else
            avp_throw(LV_EXC_KEY, "'%s'", fam);
    }
    x->out = NULL;
    return out;
}
