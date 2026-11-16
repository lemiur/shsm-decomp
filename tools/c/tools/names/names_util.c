/* names.py helpers: plain_of, plain_index, split_params, clip, and the whole-identifier matcher (ident_rx). */
#include "names.h"
#include "names_priv.h"

#include "shsm_mangle.h"

#include <string.h>

NamesPlain names_plain_of(AvpArena *a, const char *name) {
    ShsmDemangled *d = shsm_mangle_parse(a, name);
    NamesPlain p = {.plain = name, .scope = NULL, .nscope = 0, .raw = name};
    if (d == NULL || (d->local && *d->local) || (d->thunk && *d->thunk)) return p;
    p.plain = d->name;
    p.scope = d->scope;
    p.nscope = d->nscope;
    p.raw = d->raw_name;
    return p;
}

NamesCtx *names_ctx_new(const ShsmRepo *repo, const ShsmSymbolTable *st) {
    AvpArena *a = avp_arena();
    NamesCtx *cx = AVP_NEW(a, NamesCtx);
    cx->a = a;
    cx->repo = repo;
    cx->st = st ? st : shsm_load_symbols(repo, true);
    return cx;
}

static void build_index(NamesCtx *cx) {
    if (cx->by_plain) return;
    const ShsmSymbolTable *st = cx->st;
    cx->plains = AVP_NEWN(cx->a, NamesPlain, st->n ? st->n : 1);
    AvpMap *m = avp_map_new_cap(st->n);
    for (size_t i = 0; i < st->n; i++) {
        cx->plains[i] = names_plain_of(cx->a, st->symbols[i].name);
        bool created;
        AvpVal *v = avp_map_put(m, avp_str(cx->plains[i].plain), &created);
        if (created) v->p = AVP_NEW(cx->a, AvpPtrVec);
        avp_vec_push((AvpPtrVec *)v->p, (void *)&st->symbols[i]);
    }
    cx->by_plain = m;
}

const NamesPlain *names_sym_plain(NamesCtx *cx, const ShsmSymbol *s) {
    build_index(cx);
    return &cx->plains[s - cx->st->symbols];
}

const AvpPtrVec *names_by_plain(NamesCtx *cx, const char *plain) {
    static const AvpPtrVec EMPTY = {0};
    build_index(cx);
    AvpVal *v = avp_map_get(cx->by_plain, avp_str(plain));
    return v ? (const AvpPtrVec *)v->p : &EMPTY;
}

bool names_has(const NamesCtx *cx, const char *name) { return shsm_sym_by_name(cx->st, avp_str(name)) != NULL; }

size_t names_all_by_name(const NamesCtx *cx, const char *name, AvpPtrVec *out) {
    out->n = 0;
    return shsm_sym_all_by_name(cx->st, avp_str(name), out);
}

bool names_scope_eq(const char *const *a, size_t na, const char *const *b, size_t nb) {
    if (na != nb) return false;
    for (size_t i = 0; i < na; i++)
        if (strcmp(a[i], b[i]) != 0) return false;
    return true;
}

AvpStrVec names_split_params(AvpArena *a, AvpStr text) {
    AvpStrVec out = {0};
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < text.n; i++) {
        char ch = text.p[i];
        if (ch == '<' || ch == '(') depth++;
        else if (ch == '>' || ch == ')') depth--;
        if (ch == ',' && depth == 0) {
            avp_vec_push(&out, avp_str_dup(a, avp_strn(text.p + start, i - start)));
            start = i + 1;
        }
    }
    AvpStr cur = avp_strn(text.p + start, text.n - start);
    if (avp_str_strip(cur).n) avp_vec_push(&out, avp_str_dup(a, cur));
    return out;
}

AvpStr names_clip(AvpArena *a, AvpStr line, size_t width) {
    line = avp_str_strip(line);
    if (avp_u8_len(line) <= width) return line;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, avp_u8_slice(line, 0, (i64)width - 3));
    avp_buf_putz(&b, "...");
    return avp_buf_str(&b);
}

/* ---- whole-identifier matcher: (?<![A-Za-z0-9_$@])(?:w1|w2|...)(?![A-Za-z0-9_$@]), longest first ---- */

bool names_word_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '$' ||
           c == '@';
}

struct NamesIdentRx {
    AvpStrVec *buckets; /* by first byte: words, longest first */
    bool has_empty;
};

static int by_len_desc(const void *x, const void *y) {
    const AvpStr *p = x, *q = y;
    return p->n < q->n ? 1 : p->n > q->n ? -1 : 0;
}

NamesIdentRx *names_ident_rx(AvpArena *a, const AvpStr *words, size_t n) {
    NamesIdentRx *rx = AVP_NEW(a, NamesIdentRx);
    rx->buckets = AVP_NEWN(a, AvpStrVec, 256);
    for (size_t i = 0; i < n; i++) {
        if (words[i].n == 0) {
            rx->has_empty = true;
            continue;
        }
        avp_vec_push(&rx->buckets[(unsigned char)words[i].p[0]], words[i]);
    }
    for (int c = 0; c < 256; c++)
        if (rx->buckets[c].n > 1) qsort(rx->buckets[c].v, rx->buckets[c].n, sizeof(AvpStr), by_len_desc);
    return rx;
}

bool names_ident_find(const NamesIdentRx *rx, AvpStr s, size_t pos, size_t *start, size_t *len) {
    for (size_t i = pos; i <= s.n; i++) {
        if (i > 0 && names_word_char((unsigned char)s.p[i - 1])) continue;
        if (i < s.n) {
            const AvpStrVec *b = &rx->buckets[(unsigned char)s.p[i]];
            for (size_t k = 0; k < b->n; k++) {
                AvpStr w = b->v[k];
                if (w.n > s.n - i || memcmp(s.p + i, w.p, w.n) != 0) continue;
                if (i + w.n < s.n && names_word_char((unsigned char)s.p[i + w.n])) continue;
                *start = i;
                *len = w.n;
                return true;
            }
        }
        if (rx->has_empty && !(i < s.n && names_word_char((unsigned char)s.p[i]))) {
            *start = i;
            *len = 0;
            return true;
        }
    }
    return false;
}

size_t names_ident_count(const NamesIdentRx *rx, AvpStr s) {
    size_t n = 0, pos = 0, st, ln;
    while (pos <= s.n && names_ident_find(rx, s, pos, &st, &ln)) {
        n++;
        pos = st + (ln ? ln : 1);
    }
    return n;
}

AvpStr names_ident_sub(AvpArena *a, const NamesIdentRx *rx, AvpStr s, NamesSubFn fn, void *ctx) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t pos = 0, last = 0, st, ln;
    while (pos <= s.n && names_ident_find(rx, s, pos, &st, &ln)) {
        avp_buf_put(&b, avp_strn(s.p + last, st - last));
        avp_buf_put(&b, fn(ctx, avp_strn(s.p + st, ln)));
        last = st + ln;
        if (ln) {
            pos = last;
        } else {
            if (st < s.n) avp_buf_putc(&b, s.p[st]);
            pos = last = st + 1;
        }
    }
    if (last < s.n) avp_buf_put(&b, avp_strn(s.p + last, s.n - last));
    return avp_buf_str(&b);
}
