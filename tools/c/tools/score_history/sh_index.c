/* Source indexing: score_history.index_source, FileIndex and demangle. */
#include "sh_priv.h"

#include <string.h>

#define SEP "\x1f"

/* sha1(SEP.join(t.t for t in toks)) */
static char *tok_hash(AvpArena *a, const ShTok *toks, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, avp_scratch());
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putc(&b, '\x1f');
        avp_buf_put(&b, toks[i].t);
    }
    u8 dg[20];
    avp_sha1(b.p ? b.p : "", b.n, dg);
    avp_arena_reset(avp_scratch(), mk);
    return avp_hex(a, dg, 20);
}

typedef struct {
    int lo, hi;
    AvpStr name;
} Span;

static int span_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Span *a = x, *b = y;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    if (a->hi != b->hi) return a->hi < b->hi ? -1 : 1;
    return avp_str_cmp(a->name, b->name);
}

/* The last "::" component without its leading "~"s is not a keyword. */
static bool name_ok(AvpStr name) {
    AvpStr before, last;
    if (!avp_str_rpartition(name, AVP_S("::"), &before, &last)) last = name;
    while (last.n && last.p[0] == '~') last = avp_str_slice(last, 1, AVP_END);
    return !sh_is_keyword(last);
}

static AvpStr lstrip_colon(AvpStr s) {
    while (s.n && s.p[0] == ':') s = avp_str_slice(s, 1, AVP_END);
    return s;
}

static void build_lookups(ShIndex *ix) {
    ix->by_addr = avp_imap_new();
    ix->by_name = avp_map_new();
    for (size_t i = 0; i < ix->nfuncs; i++) {
        const ShIdxFunc *f = &ix->funcs[i];
        if (f->has_addr) {
            bool created;
            AvpVal *v = avp_imap_put(ix->by_addr, (u64)f->addr, &created);
            if (created) v->i = (i64)i;
        }
        bool created;
        AvpVal *v = avp_map_put(ix->by_name, f->name, &created);
        if (created) v->i = ((i64)1 << 32) | (i64)i;
        else v->i += (i64)1 << 32;
    }
}

ShIndex *sh_index_source(AvpArena *a, AvpStr text) {
    text = sh_normalise(a, text);
    /* line start offsets of text.split("\n") */
    AVP_VEC(size_t) starts = {0};
    avp_vec_push(&starts, (size_t)0);
    for (size_t i = 0; i < text.n; i++)
        if (text.p[i] == '\n') avp_vec_push(&starts, i + 1);
    size_t nlines = starts.n;
    ShLex lx;
    sh_lex(a, text, &lx);
    ShIndex *ix = AVP_NEW(a, ShIndex);
    char *uh = tok_hash(a, lx.toks.v, lx.toks.n);
    ix->unit_hash = avp_str(uh);
    ix->rest_hash = ix->unit_hash;
    AVP_VEC(Span) found = {0};
    ShFuncVec pf = {0};
    sh_parsed_funcs(a, &lx, &pf);
    ix->parsed = true;
    for (size_t i = 0; i < pf.n; i++)
        if (name_ok(pf.v[i].name)) avp_vec_push(&found, ((Span){pf.v[i].line, pf.v[i].end_line, lstrip_colon(pf.v[i].name)}));
    avp_vec_free(&pf);
    ShFuncVec rough = {0};
    sh_rough_functions(a, lx.toks.v, lx.toks.n, &rough);
    for (size_t i = 0; i < rough.n; i++) {
        int lo = rough.v[i].line, hi = rough.v[i].end_line;
        bool overlap = false;
        for (size_t q = 0; q < found.n; q++)
            if (lo <= found.v[q].hi && found.v[q].lo <= hi) {
                overlap = true;
                break;
            }
        if (!overlap) avp_vec_push(&found, ((Span){lo, hi, rough.v[i].name}));
    }
    avp_vec_free(&rough);
    avp_sort(found.v, found.n, sizeof(Span), span_cmp, NULL, false);
    AVP_VEC(Span) spans = {0};
    for (size_t i = 0; i < found.n; i++) {
        if (spans.n && found.v[i].lo <= avp_vec_last(&spans).hi) continue; /* nested in the previous function */
        avp_vec_push(&spans, found.v[i]);
    }
    /* comments by line */
    AVP_VEC(u32) *by_line = avp_alloc(a, (nlines + 2) * sizeof *by_line);
    for (size_t c = 0; c < lx.comments.n; c++) {
        const ShComment *cm = &lx.comments.v[c];
        for (int ln = cm->line; ln <= cm->end_line; ln++)
            if (ln >= 0 && (size_t)ln < nlines + 2) avp_vec_push(&by_line[ln], (u32)c);
    }
    ix->funcs = avp_alloc(a, (spans.n ? spans.n : 1) * sizeof(ShIdxFunc));
    ix->text_hash = avp_alloc(a, (spans.n ? spans.n : 1) * sizeof(AvpStr));
    ix->text = avp_alloc(a, (spans.n ? spans.n : 1) * sizeof(AvpStr));
    AvpMap *seen = avp_map_new();
    int prev_end = 0;
    for (size_t s = 0; s < spans.n; s++) {
        int lo = spans.v[s].lo, hi = spans.v[s].hi;
        /* "\n".join(lines[lo - 1 : hi]) */
        size_t l0 = (size_t)(lo - 1), l1 = (size_t)hi;
        if (l1 > nlines) l1 = nlines;
        AvpStr seg = {"", 0};
        if (l0 < l1) {
            size_t b = starts.v[l0];
            size_t e = l1 < nlines ? starts.v[l1] - 1 : text.n;
            seg = avp_strn(text.p + b, e - b);
        }
        ShLex sl;
        sh_lex(a, seg, &sl);
        AvpStr h = avp_str(tok_hash(a, sl.toks.v, sl.toks.n));
        sh_lex_free(&sl);
        bool has = false;
        i64 addr = 0, v;
        if ((size_t)hi < nlines + 2)
            for (size_t q = 0; q < by_line[hi].n; q++) {
                const ShComment *cm = &lx.comments.v[by_line[hi].v[q]];
                if (sh_addr_in_comment(cm->text, &v) && cm->trailing) has = true, addr = v;
            }
        if (!has) {
            int stop = prev_end > lo - 12 ? prev_end : lo - 12;
            for (int ln = lo; ln > stop; ln--) {
                if (ln >= 0 && (size_t)ln < nlines + 2)
                    for (size_t q = 0; q < by_line[ln].n; q++) {
                        const ShComment *cm = &lx.comments.v[by_line[ln].v[q]];
                        if (sh_addr_in_comment(cm->text, &v)) has = true, addr = v;
                    }
                if (has) break;
            }
        }
        ShIdxFunc *f = &ix->funcs[ix->nfuncs++];
        f->name = spans.v[s].name;
        f->line = lo;
        f->end = hi;
        f->has_addr = has;
        f->addr = addr;
        f->hash = h;
        bool created;
        AvpVal *tv = avp_map_put(seen, h, &created);
        if (created) {
            tv->i = (i64)ix->ntexts;
            ix->text_hash[ix->ntexts] = h;
            ix->text[ix->ntexts++] = seg;
        } else {
            ix->text[tv->i] = seg; /* a dict keeps the last value */
        }
        prev_end = hi;
    }
    avp_map_free(seen);
    /* the rest: tokens and directives outside every function */
    u8 *inside = avp_alloc(a, nlines + 2);
    size_t ilen = nlines + 2;
    for (size_t s = 0; s < spans.n; s++)
        for (int ln = spans.v[s].lo; ln <= spans.v[s].hi; ln++) inside[(size_t)ln < ilen ? (size_t)ln : ilen - 1] = 1;
    AvpBuf rb;
    avp_buf_init(&rb, avp_scratch());
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    bool any = false;
    for (size_t i = 0; i < lx.toks.n; i++) {
        size_t ln = (size_t)lx.toks.v[i].line;
        if (inside[ln < ilen ? ln : ilen - 1]) continue;
        if (any) avp_buf_putc(&rb, '\x1f');
        avp_buf_put(&rb, lx.toks.v[i].t);
        any = true;
    }
    for (size_t i = 0; i < lx.directives.n; i++) {
        const ShDirective *d = &lx.directives.v[i];
        size_t ln = (size_t)d->line;
        if (inside[ln < ilen ? ln : ilen - 1]) continue;
        if (any) avp_buf_putc(&rb, '\x1f');
        avp_buf_putc(&rb, '#');
        avp_buf_put(&rb, d->name);
        avp_buf_putc(&rb, ' ');
        avp_buf_put(&rb, avp_str_strip(d->rest));
        any = true;
    }
    u8 dg[20];
    avp_sha1(rb.p ? rb.p : "", rb.n, dg);
    avp_arena_reset(avp_scratch(), mk);
    ix->rest_hash = avp_str(avp_hex(a, dg, 20));
    for (size_t i = 0; i < nlines + 2; i++) avp_vec_free(&by_line[i]);
    avp_vec_free(&found);
    avp_vec_free(&spans);
    avp_vec_free(&starts);
    sh_lex_free(&lx);
    build_lookups(ix);
    return ix;
}

AvpStr sh_index_to_json(AvpArena *a, const ShIndex *ix) {
    AvpBuf b;
    avp_buf_init(&b, a);
    AvpJsonW w;
    avp_jsonw_init(&w, &b, NULL);
    avp_jsonw_begin_object(&w);
    avp_jsonw_key(&w, AVP_S("funcs"));
    avp_jsonw_begin_array(&w);
    for (size_t i = 0; i < ix->nfuncs; i++) {
        const ShIdxFunc *f = &ix->funcs[i];
        avp_jsonw_begin_object(&w);
        avp_jsonw_kv_str(&w, "name", f->name);
        avp_jsonw_kv_i64(&w, "line", f->line);
        avp_jsonw_kv_i64(&w, "end", f->end);
        avp_jsonw_key(&w, AVP_S("addr"));
        if (f->has_addr) avp_jsonw_i64(&w, f->addr);
        else avp_jsonw_null(&w);
        avp_jsonw_kv_str(&w, "hash", f->hash);
        avp_jsonw_end_object(&w);
    }
    avp_jsonw_end_array(&w);
    avp_jsonw_kv_str(&w, "unit_hash", ix->unit_hash);
    avp_jsonw_kv_str(&w, "rest_hash", ix->rest_hash);
    avp_jsonw_kv_bool(&w, "parsed", ix->parsed);
    avp_jsonw_end_object(&w);
    avp_jsonw_finish(&w);
    return avp_buf_str(&b);
}

ShIndex *sh_index_from_json(AvpArena *a, AvpStr json) {
    AvpJsonError err;
    AvpJson *d = avp_json_parse(a, json, &err);
    if (!d) avp_throw(SH_EXC_OTHER, "json.decoder.JSONDecodeError: %s", avp_json_error_str(a, &err).p);
    ShIndex *ix = AVP_NEW(a, ShIndex);
    AvpJson *funcs = avp_json_getz(d, "funcs");
    size_t n = avp_json_len(funcs);
    ix->funcs = avp_alloc(a, (n ? n : 1) * sizeof(ShIdxFunc));
    for (size_t i = 0; i < n; i++) {
        AvpJson *f = avp_json_at(funcs, i);
        ShIdxFunc *o = &ix->funcs[ix->nfuncs++];
        o->name = avp_json_get_str(f, "name", AVP_S(""));
        o->line = (int)avp_json_get_i64(f, "line", 0);
        o->end = (int)avp_json_get_i64(f, "end", 0);
        AvpJson *ad = avp_json_getz(f, "addr");
        o->has_addr = ad && !avp_json_is_null(ad);
        o->addr = avp_json_i64(ad, 0);
        o->hash = avp_json_get_str(f, "hash", AVP_S(""));
    }
    ix->unit_hash = avp_json_get_str(d, "unit_hash", AVP_S(""));
    ix->rest_hash = avp_json_get_str(d, "rest_hash", AVP_S(""));
    ix->parsed = avp_json_get_bool(d, "parsed", false);
    build_lookups(ix);
    return ix;
}

/* ---- demangle ------------------------------------------------------------------------------------------------- */

static const char *const OPS[][2] = {
    {"__as", "operator="},  {"__eq", "operator=="},  {"__ne", "operator!="},  {"__vc", "operator[]"},
    {"__nw", "operator new"}, {"__dl", "operator delete"}, {"__nwa", "operator new[]"}, {"__dla", "operator delete[]"},
    {"__pl", "operator+"},  {"__mi", "operator-"},   {"__ml", "operator*"},   {"__dv", "operator/"},
    {"__apl", "operator+="}, {"__ami", "operator-="}, {"__aml", "operator*="}, {"__adv", "operator/="},
    {"__lt", "operator<"},  {"__gt", "operator>"},   {"__le", "operator<="},  {"__ge", "operator>="},
    {"__cl", "operator()"}, {"__rf", "operator->"},  {"__nt", "operator!"},   {"__ls", "operator<<"},
    {"__rs", "operator>>"}, {"__pp", "operator++"},  {"__mm", "operator--"},  {"__md", "operator%"},
    {"__ad", "operator&"},  {"__or", "operator|"},   {"__er", "operator^"},   {"__aa", "operator&&"},
    {"__oo", "operator||"},
};

static bool isdig(char c) { return c >= '0' && c <= '9'; }

/* _parse_len_name(s, pos): true with the name and the position after it */
static bool parse_len_name(AvpStr s, size_t pos, AvpStr *name, size_t *after) {
    size_t q = pos;
    while (q < s.n && isdig(s.p[q])) q++;
    if (q == pos) return false;
    i64 n = 0;
    for (size_t z = pos; z < q; z++) {
        n = n * 10 + (s.p[z] - '0');
        if (n > (i64)1 << 40) return false;
    }
    if (n <= 0 || q + (size_t)n > s.n) return false;
    *name = avp_strn(s.p + q, (size_t)n);
    *after = q + (size_t)n;
    return true;
}

static bool parse_qualifier(AvpStr rest, AvpStrVec *quals) {
    size_t pos = 0;
    if (rest.n && rest.p[0] == 'F') return true;
    AvpStr nm;
    if (rest.n > 2 && rest.p[0] == 'Q' && isdig(rest.p[1])) {
        int n = rest.p[1] - '0';
        pos = 2;
        for (int k = 0; k < n; k++) {
            if (!parse_len_name(rest, pos, &nm, &pos)) return false;
            avp_vec_push(quals, nm);
        }
    } else {
        if (!parse_len_name(rest, 0, &nm, &pos)) return false;
        avp_vec_push(quals, nm);
    }
    if (pos < rest.n && rest.p[pos] == 'C') pos++;
    if (!(pos < rest.n && rest.p[pos] == 'F')) return false;
    return true;
}

AvpStrVec sh_demangle(AvpArena *a, AvpStr sym, bool *sinit) {
    AvpStrVec cands = {0};
    *sinit = false;
    /* re.match(r"^@\d+@(.+)$", sym) */
    if (sym.n && sym.p[0] == '@') {
        size_t q = 1;
        while (q < sym.n && isdig(sym.p[q])) q++;
        if (q > 1 && q < sym.n && sym.p[q] == '@') {
            AvpStr rest = avp_strn(sym.p + q + 1, sym.n - q - 1);
            if (rest.n && rest.p[rest.n - 1] == '\n') rest.n--;
            if (rest.n && !memchr(rest.p, '\n', rest.n)) sym = rest;
        }
    }
    if (avp_str_startswith(sym, AVP_S("__sinit_"))) {
        *sinit = true;
        return cands;
    }
    i64 start = 1;
    for (;;) {
        i64 i = avp_str_find(sym, AVP_S("__"), start, AVP_END);
        if (i < 0) break;
        start = i + 1;
        AvpStr base = avp_strn(sym.p, (size_t)i), rest = avp_str_slice(sym, i + 2, AVP_END);
        AvpStrVec quals = {0};
        if (!parse_qualifier(rest, &quals)) {
            avp_vec_free(&quals);
            continue;
        }
        AvpStr name = base;
        if (avp_str_eqz(base, "__ct") && quals.n) {
            name = avp_vec_last(&quals);
        } else if (avp_str_eqz(base, "__dt") && quals.n) {
            name = avp_str_concat(a, AVP_S("~"), avp_vec_last(&quals));
        } else {
            for (size_t k = 0; k < sizeof OPS / sizeof *OPS; k++)
                if (avp_str_eqz(base, OPS[k][0])) name = avp_str(OPS[k][1]);
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t k = 0; k < quals.n; k++) {
            avp_buf_put(&b, quals.v[k]);
            avp_buf_putz(&b, "::");
        }
        avp_buf_put(&b, name);
        avp_vec_push(&cands, avp_buf_str(&b));
        if (quals.n > 1) {
            AvpBuf b2;
            avp_buf_init(&b2, a);
            avp_buf_put(&b2, avp_vec_last(&quals));
            avp_buf_putz(&b2, "::");
            avp_buf_put(&b2, name);
            avp_vec_push(&cands, avp_buf_str(&b2));
        }
        avp_vec_free(&quals);
        break;
    }
    avp_vec_push(&cands, sym);
    return cands;
}

const ShIdxFunc *sh_index_lookup(AvpArena *a, const ShIndex *ix, AvpStr symbol, i64 addr, const char **basis) {
    AvpVal *v = avp_imap_get(ix->by_addr, (u64)addr);
    if (v) {
        *basis = "addr";
        return &ix->funcs[v->i];
    }
    bool sinit;
    AvpStrVec dm = sh_demangle(a, symbol, &sinit);
    if (sinit) {
        *basis = "sinit";
        return NULL;
    }
    const ShIdxFunc *hit = NULL;
    for (size_t k = 0; k < dm.n && !hit; k++) {
        AvpVal *nv = avp_map_get(ix->by_name, dm.v[k]);
        if (nv && (nv->i >> 32) == 1) hit = &ix->funcs[nv->i & 0xFFFFFFFF];
    }
    avp_vec_free(&dm);
    *basis = hit ? "name" : "unit";
    return hit;
}
