/* levers: the fake-form filter and the validity check (levers.py fake_forms, fake_reasons, check_parses). */
#include "levers.h"

#include <string.h>

static void bump(AvpArena *a, LvCounter *c, const char *k) {
    for (size_t i = 0; i < c->n; i++)
        if (!strcmp(c->v[i].k, k)) {
            c->v[i].n++;
            return;
        }
    LvCount e = {avp_strdup(a, k), 1};
    avp_vec_push(c, e);
}

i64 lv_counter_get(const LvCounter *c, const char *k) {
    for (size_t i = 0; i < c->n; i++)
        if (!strcmp(c->v[i].k, k)) return c->v[i].n;
    return 0;
}

/* re.match(r"#\s*pragma\b", s) */
static bool pragma_line(const char *s, size_t n) {
    size_t i = 0;
    if (i >= n || s[i] != '#') return false;
    i++;
    while (i < n && lv_isspace((u8)s[i])) i++;
    if (n - i < 6 || memcmp(s + i, "pragma", 6)) return false;
    i += 6;
    return i >= n || !lv_isword((u8)s[i]);
}

/* FnContext(text, symbol) under a try: NULL with *code set when it raised one of the caught classes */
static LvCtx *try_ctx(AvpArena *a, LvS text, const char *symbol, bool *failed) {
    AvpTry t;
    LvCtx *x = NULL;
    *failed = false;
    if (AVP_TRY(&t)) {
        x = lv_ctx_new(a, text, symbol);
        avp_try_end(&t);
    } else {
        if (!lv_exc_caught(t.code)) avp_throw(t.code, "%s", t.msg);
        *failed = true;
    }
    return x;
}

LvCounter lv_fake_forms(AvpArena *a, LvS fn_text, const char *symbol) {
    LvCounter cnt = {0};
    LvToks toks = lv_tokenize(a, fn_text);
    for (i32 i = 0; i < toks.n; i++)
        if (toks.v[i].k == LV_PP && pragma_line(toks.v[i].t, toks.v[i].n)) bump(a, &cnt, "pragma");
    LvToks code = lv_code_tokens(a, toks);
    for (i32 i = 0; i < code.n; i++) {
        const LvTok *t = &code.v[i];
        if (t->k == LV_ID) {
            if (TEQ(t, "volatile") || TEQ(t, "register") || TEQ(t, "goto")) bump(a, &cnt, t->t);
            else if (TEQ(t, "asm") || TEQ(t, "__asm") || TEQ(t, "__asm__")) bump(a, &cnt, "asm");
            else if (TEQ(t, "_Pragma") || TEQ(t, "__pragma")) bump(a, &cnt, "pragma");
        }
        if (TEQ(t, "{") && i + 1 < code.n && TEQ(&code.v[i + 1], "}")) bump(a, &cnt, "empty block");
        if (TEQ(t, "while") && i > 0 && TEQ(&code.v[i - 1], "}") && i + 4 < code.n && TEQ(&code.v[i + 1], "(") &&
            TEQ(&code.v[i + 2], "0") && TEQ(&code.v[i + 3], ")") && TEQ(&code.v[i + 4], ";"))
            bump(a, &cnt, "do-while(0)");
    }
    const char *name = symbol;
    AvpTry t;
    if (AVP_TRY(&t)) {
        i32 *m = lv_match_brackets(a, code);
        if (!symbol) {
            LvFnDefs defs = lv_scan_definitions(a, code, m);
            if (!defs.n) avp_throw(LV_EXC_INDEX, "list index out of range");
            name = defs.v[0].name;
            avp_vec_free(&defs);
        } else
            lv_find_definition(a, code, m, symbol);
        avp_try_end(&t);
    } else {
        if (!lv_exc_caught(t.code)) avp_throw(t.code, "%s", t.msg);
        bump(a, &cnt, "unparsable");
        return cnt;
    }
    bool failed;
    LvCtx *x = try_ctx(a, fn_text, name, &failed);
    if (failed) {
        bump(a, &cnt, "unparsable");
        return cnt;
    }
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        if (s->kind == LS_EMPTY) bump(a, &cnt, "empty statement");
        if (s->kind == LS_DECL)
            for (i32 d = 0; d < s->decl->ndtors; d++) {
                AvpVal *v = avp_imap_get(x->body_ids, (u64)(uintptr_t)s->decl->dtors[d].name);
                if (!v || v->i <= 1) bump(a, &cnt, "unused local");
            }
    }
    LvNodes w = {0};
    for (size_t q = 0; q < x->exprs.n; q++) {
        w.n = 0;
        lv_walk(x->exprs.v[q].n, &w);
        for (size_t i = 0; i < w.n; i++) {
            if (w.v[i]->kind == LN_COMMA) bump(a, &cnt, "comma operator");
            else if (w.v[i]->kind == LN_CAST) bump(a, &cnt, "cast");
        }
    }
    avp_vec_free(&w);
    return cnt;
}

AvpCzVec lv_fake_reasons(AvpArena *a, const LvCounter *orig, LvS cand_fn, const char *symbol) {
    LvCounter b = lv_fake_forms(a, cand_fn, symbol);
    AvpCzVec out = {0};
    for (size_t i = 0; i < b.n; i++)
        if (b.v[i].n > lv_counter_get(orig, b.v[i].k)) avp_vec_push(&out, (char *)b.v[i].k);
    avp_vec_free(&b);
    for (size_t i = 1; i < out.n; i++)
        for (size_t j = i; j > 0 && strcmp(out.v[j - 1], out.v[j]) > 0; j--) {
            char *t = out.v[j];
            out.v[j] = out.v[j - 1];
            out.v[j - 1] = t;
        }
    return out;
}

bool lv_check_parses(AvpArena *a, LvS text, const char *symbol) {
    bool failed;
    try_ctx(a, text, symbol, &failed);
    return !failed;
}
