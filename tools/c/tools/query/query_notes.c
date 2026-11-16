/* query.py notes: note_terms, term_regex, doc_passages, comment_blocks, source_hits, doc_notes and cmd_notes. */
#include "query_notes.h"

#include "query_b.h"

#define DOC_TERM_WORD "^[\\w@$:.~<>]+$"
#define DEF_RE "^[A-Za-z_][^;{}=]*?([A-Za-z_~][\\w:~<>]*)\\s*\\([^;]*$"

static bool re_match(const char *pat, int flags, AvpStr s) {
    ShsmReMatch m;
    return shsm_re_match(shsm_re(pat, flags), s, 0, AVP_END, &m);
}
static bool rx_search(const ShsmRe *rx, AvpStr s) {
    ShsmReMatch m;
    return shsm_re_search(rx, s, 0, AVP_END, &m);
}
static bool str_in(const AvpStrVec *v, AvpStr s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eq(v->v[i], s)) return true;
    return false;
}

/* ---- note_terms ---- */

AvpStrVec query_note_terms(QueryCtx *ctx, AvpStr term, const ShsmSymRow **sym_out) {
    AvpArena *a = ctx->a;
    AvpStrVec terms = {0};
    avp_vec_push(&terms, term);
    const ShsmSymRow *sym = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (query_parse_addr(term) != SHSM_NONE || shsm_sym_by_name(query_syms_split(ctx), term) ||
            avp_str_contains(term, AVP_S("::"))) {
            sym = query_resolve(ctx, term, NULL);
        } else if (re_match("^[A-Za-z_]\\w*$", 0, term)) {
            /* a plain member or function name: the mangled symbols it names (`fn_800067CC__8unk_RampFf`) */
            AvpJson *rows = query_sql(ctx, "SELECT DISTINCT name FROM symbols WHERE substr(name, 1, ?) = ? LIMIT 3", 2,
                                      avp_json_int(a, (i64)avp_u8_len(term) + 2),
                                      avp_json_str(a, avp_pyfmt(a, "%S__", &term)));
            AvpStr only = {NULL, 0};
            size_t n = 0;
            for (size_t i = 0; i < avp_json_len(rows); i++) {
                const AvpJson *c = avp_json_at(avp_json_at(rows, i), 0);
                AvpStr name = c && avp_json_is_str(c) ? c->u.str : AVP_S("");
                AvpStr base = query_mangle_base_name(a, name);
                if (base.p && avp_str_eq(query_mangle_base_name(a, name), term)) {
                    if (!n) only = name;
                    n++;
                }
            }
            if (n == 1) sym = query_sym(ctx, only);
        }
        avp_try_end(&t);
    } else {
        if (t.code != SHSM_EXC_USAGE && t.code != SHSM_EXC_FILE_NOT_FOUND) avp_throw(t.code, "%s", t.msg);
        sym = NULL;
    }
    if (!sym) {
        const ShsmSymbol *st = shsm_sym_by_name(query_syms_split(ctx), term);
        if (st) avp_vec_push(&terms, avp_pyfmt(a, "0x%08X", (i64)st->address));
    } else {
        avp_vec_push(&terms, sym->name);
        AvpStr base;
        const AvpStr *scope = NULL;
        size_t nscope = 0;
        if (query_mangle_parse_scope(a, sym->name, &base, &scope, &nscope) && !str_in(&terms, base) &&
            !avp_str_startswith(base, AVP_S("__"))) {
            if (nscope) {
                AvpStr q = avp_str_join(a, AVP_S("::"), scope, nscope);
                avp_vec_push(&terms, avp_pyfmt(a, "%S::%S", &q, &base));
            } else {
                avp_vec_push(&terms, base);
            }
            avp_vec_push(&terms, base);
        }
        avp_vec_push(&terms, avp_pyfmt(a, "0x%08X", sym->address));
    }
    AvpStrVec seen = {0};
    for (size_t i = 0; i < terms.n; i++)
        if (terms.v[i].n && !str_in(&seen, terms.v[i])) avp_vec_push(&seen, terms.v[i]);
    avp_vec_free(&terms);
    if (sym_out) *sym_out = sym;
    return seen;
}

/* ---- term_regex ---- */

ShsmRe *query_term_regex(AvpArena *a, const AvpStr *terms, size_t n) {
    AvpBuf b = {0};
    bool any_word = false;
    for (size_t i = 0; i < n; i++) {
        AvpStr esc = shsm_re_escape(a, terms[i]);
        bool word = re_match(DOC_TERM_WORD, 0, terms[i]);
        any_word = any_word || word;
        if (i) avp_buf_putz(&b, "|");
        if (word) {
            avp_buf_putz(&b, "(?<![\\w@$])");
            avp_buf_put(&b, esc);
            avp_buf_putz(&b, "(?![\\w@$])");
        } else {
            avp_buf_put(&b, esc);
        }
    }
    AvpStr pat = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return shsm_re_try_n(pat, any_word ? 0 : SHSM_RE_I);
}

/* ---- doc_passages ---- */

typedef struct {
    i64 level;
    AvpStr text;
} Head;
typedef AVP_VEC(Head) Heads;

static AvpStr heads_text(AvpArena *a, const Heads *h) {
    AvpBuf b = {0};
    for (size_t i = 0; i < h->n; i++) {
        if (i) avp_buf_putz(&b, " > ");
        avp_buf_put(&b, h->v[i].text);
    }
    AvpStr s = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return s;
}

static void flush(AvpArena *a, QueryPassages *out, const AvpStrVec *para, i64 start, const Heads *heads,
                  const ShsmRe *rx) {
    if (!para->n) return;
    AvpStr text = avp_str_join(a, AVP_S("\n"), para->v, para->n);
    if (rx_search(rx, text)) avp_vec_push(out, ((QueryPassage){start, heads_text(a, heads), text}));
}

QueryPassages query_doc_passages(AvpArena *a, const char *path, const ShsmRe *rx) {
    QueryPassages out = {0};
    AvpStr text = {0};
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) {
        if (shsm_io_exc_code() == SHSM_EXC_VALUE) shsm_raise_io();
        return out;
    }
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    Heads heads = {0};
    AvpStrVec para = {0};
    i64 start = 0;
    bool table_head = false, in_code = false;
    ShsmRe *head_re = shsm_re("^(#{1,6})\\s+(.*)$", 0);
    for (size_t k = 0; k < lines.n; k++) {
        AvpStr line = lines.v[k];
        i64 i = (i64)k + 1;
        if (avp_str_startswith(line, AVP_S("```"))) in_code = !in_code;
        ShsmReMatch m;
        if (!in_code && shsm_re_match(head_re, line, 0, AVP_END, &m)) {
            flush(a, &out, &para, start, &heads, rx);
            para.n = 0;
            i64 lvl = (i64)shsm_re_group(&m, 1).n;
            size_t keep = 0;
            for (size_t j = 0; j < heads.n; j++)
                if (heads.v[j].level < lvl) heads.v[keep++] = heads.v[j];
            heads.n = keep;
            avp_vec_push(&heads, ((Head){lvl, avp_str_strip(shsm_re_group(&m, 2))}));
            if (rx_search(rx, line)) avp_vec_push(&out, ((QueryPassage){i, heads_text(a, &heads), line}));
            table_head = false;
            continue;
        }
        if (avp_str_startswith(line, AVP_S("|"))) {
            flush(a, &out, &para, start, &heads, rx);
            para.n = 0;
            if (!table_head) table_head = true;
            else if (rx_search(rx, line)) avp_vec_push(&out, ((QueryPassage){i, heads_text(a, &heads), line}));
            continue;
        }
        table_head = false;
        bool blank = avp_str_strip(line).n == 0;
        if (blank || (para.n && re_match("^\\s*([-*]|\\d+\\.)\\s", 0, line))) {
            flush(a, &out, &para, start, &heads, rx);
            para.n = 0;
            if (!blank) avp_vec_push(&para, line);
            start = i;
            continue;
        }
        if (!para.n) start = i;
        avp_vec_push(&para, line);
    }
    flush(a, &out, &para, start, &heads, rx);
    avp_vec_free(&para);
    avp_vec_free(&heads);
    avp_vec_free(&lines);
    return out;
}

/* ---- comment_blocks / source_hits ---- */

QueryBlocks query_comment_blocks(const AvpStr *lines, size_t n) {
    QueryBlocks blocks = {0};
    size_t i = 0;
    while (i < n) {
        AvpStr s = avp_str_lstrip(lines[i]);
        if (avp_str_startswith(s, AVP_S("//")) || avp_str_startswith(s, AVP_S("/*"))) {
            size_t j = i;
            while (j < n) {
                AvpStr t = avp_str_lstrip(lines[j]);
                if (avp_str_startswith(t, AVP_S("/*"))) {
                    while (j < n && !avp_str_contains(lines[j], AVP_S("*/"))) j++;
                    j++;
                } else if (avp_str_startswith(t, AVP_S("//"))) {
                    j++;
                } else {
                    break;
                }
            }
            avp_vec_push(&blocks, ((QueryBlock){(i64)i + 1, (i64)j}));
            i = j;
        } else {
            i++;
        }
    }
    return blocks;
}

static bool has_src_ext(AvpStr fn) {
    static const char *const ext[] = {".c", ".cpp", ".cp", ".h", ".hpp", ".inc"};
    for (size_t i = 0; i < sizeof ext / sizeof *ext; i++)
        if (avp_str_endswith(fn, avp_str(ext[i]))) return true;
    return false;
}

static int cmp_str(const void *x, const void *y, void *c) {
    (void)c;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

/* The definition line test of DEF_RE that the closing-comment walk uses: a match not indented. */
static bool def_line(AvpStr s) { return re_match(DEF_RE, 0, s); }

static void file_hits(QueryCtx *ctx, const char *path, const ShsmRe *rx, ShsmRe *const *addr_res, size_t naddr,
                      QueryNoteHits *hits) {
    AvpArena *a = ctx->a;
    AvpStr text = {0};
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) return;
    if (!rx_search(rx, text)) return;
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    AvpStr rel = avp_str(shsm_repo_rel(a, ctx->repo, path));
    AvpIMap *done = avp_imap_new();
    QueryBlocks bl = query_comment_blocks(lines.v, lines.n);
    for (size_t k = 0; k < bl.n; k++) {
        i64 first = bl.v[k].first, last = bl.v[k].last;
        i64 hi = last < (i64)lines.n ? last : (i64)lines.n;
        AvpStr body = hi > first - 1 ? avp_str_join(a, AVP_S("\n"), lines.v + first - 1, (size_t)(hi - first + 1))
                                     : AVP_S("");
        if (!rx_search(rx, body)) continue;
        AvpStr nxt = last < (i64)lines.n ? lines.v[last] : AVP_S("");
        if (def_line(nxt)) body = avp_pyfmt(a, "%S\n%S", &body, &nxt);
        avp_vec_push(hits, ((QueryNoteHit){rel, first, {NULL, 0}, body}));
        for (i64 x = first; x <= last; x++) avp_imap_put(done, (u64)x, NULL);
    }
    avp_vec_free(&bl);
    for (size_t ti = 0; ti < naddr; ti++) {
        for (size_t k = 0; k < lines.n; k++) {
            ShsmReMatch m;
            if (!shsm_re_match(addr_res[ti], lines.v[k], 0, AVP_END, &m)) continue;
            size_t j = k;
            while (j > 0 && !(def_line(lines.v[j]) && !avp_str_startswith(lines.v[j], AVP_S(" ")) &&
                              !avp_str_startswith(lines.v[j], AVP_S("\t"))))
                j--;
            QueryBlocks cb = query_comment_blocks(lines.v, j);
            i64 first = (i64)j + 1;
            for (size_t c = 0; c < cb.n; c++)
                if (cb.v[c].last == (i64)j) {
                    first = cb.v[c].first;
                    break;
                }
            avp_vec_free(&cb);
            if (avp_imap_get(done, (u64)first)) continue;
            AvpStr body = avp_str_join(a, AVP_S("\n"), lines.v + first - 1, j + 2 - (size_t)first);
            avp_vec_push(hits, ((QueryNoteHit){rel, first, {NULL, 0}, body}));
            avp_imap_put(done, (u64)first, NULL);
        }
    }
    avp_imap_free(done);
    avp_vec_free(&lines);
}

QueryNoteHits query_source_hits(QueryCtx *ctx, const ShsmRe *rx, const AvpStr *addr_terms, size_t n) {
    AvpArena *a = ctx->a;
    QueryNoteHits hits = {0};
    ShsmRe **addr_res = AVP_NEWN(a, ShsmRe *, n + 1);
    for (size_t i = 0; i < n; i++) {
        AvpStr esc = shsm_re_escape(a, addr_terms[i]);
        addr_res[i] = shsm_re_try_n(avp_pyfmt(a, "^\\}\\s*//\\s*%S\\b", &esc), SHSM_RE_I);
    }
    const char *tops[] = {ctx->repo->src_dir, ctx->repo->include_dir};
    for (size_t ti = 0; ti < 2; ti++) {
        AvpWalk *w = avp_walk_open(tops[ti], true);
        AvpWalkItem item = {0};
        while (avp_walk_next(w, &item)) {
            AvpStrVec fns = {0};
            for (size_t i = 0; i < item.files.n; i++) avp_vec_push(&fns, item.files.v[i]);
            avp_sort(fns.v, fns.n, sizeof *fns.v, cmp_str, NULL, false);
            const char *dp = avp_cz(a, item.dirpath);
            for (size_t i = 0; i < fns.n; i++) {
                if (!has_src_ext(fns.v[i])) continue;
                file_hits(ctx, avp_path_join(a, dp, avp_cz(a, fns.v[i])), rx, addr_res, n, &hits);
            }
            avp_vec_free(&fns);
        }
        avp_walk_close(w);
    }
    return hits;
}

/* ---- doc_notes ---- */

QueryNoteHits query_doc_notes(QueryCtx *ctx, AvpStr term, const QueryNotesOpts *opts, AvpStrVec *terms_out) {
    AvpArena *a = ctx->a;
    QueryNotesOpts def = {.docs_only = true};
    if (!opts) opts = &def;
    AvpStrVec terms = {0};
    if (opts->no_expand) avp_vec_push(&terms, term);
    else terms = query_note_terms(ctx, term, NULL);
    ShsmRe *rx = query_term_regex(a, terms.v, terms.n);
    QueryNoteHits hits = {0};
    if (!opts->source_only) {
        AvpWalk *w = avp_walk_open(avp_path_join(a, ctx->repo->root, "docs"), true);
        AvpWalkItem item = {0};
        while (avp_walk_next(w, &item)) {
            AvpStrVec fns = {0};
            for (size_t i = 0; i < item.files.n; i++) avp_vec_push(&fns, item.files.v[i]);
            avp_sort(fns.v, fns.n, sizeof *fns.v, cmp_str, NULL, false);
            const char *dp = avp_cz(a, item.dirpath);
            for (size_t i = 0; i < fns.n; i++) {
                if (!avp_str_endswith(fns.v[i], AVP_S(".md"))) continue;
                bool skip = false;
                for (size_t e = 0; e < opts->nexclude; e++) skip = skip || avp_str_eqz(fns.v[i], opts->exclude[e]);
                if (skip) continue;
                const char *p = avp_path_join(a, dp, avp_cz(a, fns.v[i]));
                QueryPassages ps = query_doc_passages(a, p, rx);
                AvpStr rel = avp_str(shsm_repo_rel(a, ctx->repo, p));
                for (size_t k = 0; k < ps.n; k++)
                    avp_vec_push(&hits, ((QueryNoteHit){rel, ps.v[k].line, ps.v[k].heading, ps.v[k].text}));
                avp_vec_free(&ps);
            }
            avp_vec_free(&fns);
        }
        avp_walk_close(w);
    }
    if (!opts->docs_only) {
        AvpStrVec addr = {0};
        for (size_t i = 0; i < terms.n; i++)
            if (avp_str_startswith(terms.v[i], AVP_S("0x"))) avp_vec_push(&addr, terms.v[i]);
        QueryNoteHits sh = query_source_hits(ctx, rx, addr.v, addr.n);
        for (size_t i = 0; i < sh.n; i++) avp_vec_push(&hits, sh.v[i]);
        avp_vec_free(&sh);
        avp_vec_free(&addr);
    }
    if (terms_out) *terms_out = terms;
    else avp_vec_free(&terms);
    return hits;
}

AvpJson *query_note_hit_json(AvpArena *a, const QueryNoteHit *h) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("file"), avp_json_str(a, h->file));
    avp_json_set(a, o, AVP_S("line"), avp_json_int(a, h->line));
    avp_json_set(a, o, AVP_S("heading"), shsm_json_opt_str(a, h->heading));
    avp_json_set(a, o, AVP_S("text"), avp_json_str(a, h->text));
    return o;
}

/* ---- cmd_notes ---- */

int query_cmd_notes(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr term = avp_args_str(args, "term");
    QueryNotesOpts o = {.docs_only = avp_args_bool(args, "docs_only"),
                        .source_only = avp_args_bool(args, "source_only")};
    AvpStrVec terms = {0};
    QueryNoteHits hits = query_doc_notes(ctx, term, &o, &terms);
    bool all = avp_args_bool(args, "all");
    AvpStr joined = avp_str_join(a, AVP_S(", "), terms.v, terms.n);
    shsm_output_line(out, avp_pyfmt(a, "searching for: %S", &joined));
    AvpJson *jterms = avp_json_arr(a), *jhits = avp_json_arr(a);
    for (size_t i = 0; i < terms.n; i++) avp_json_arr_push(a, jterms, avp_json_str(a, terms.v[i]));
    for (size_t i = 0; i < hits.n; i++) {
        const QueryNoteHit *h = &hits.v[i];
        avp_json_arr_push(a, jhits, query_note_hit_json(a, h));
        AvpStr hd = h->heading.p && h->heading.n ? avp_pyfmt(a, "  [%S]", &h->heading) : AVP_S("");
        shsm_output_line(out, avp_pyfmt(a, "%S:%d%S", &h->file, h->line, &hd));
        AvpStr t = avp_str_strip_chars(h->text, AVP_S("\n"));
        if (!all && avp_u8_len(t) > 700) {
            AvpStr cut = avp_u8_slice(t, 0, 700);
            t = avp_pyfmt(a, "%S ...", &cut);
        }
        AvpStrVec ls = avp_str_split(t, AVP_S("\n"), -1);
        for (size_t k = 0; k < ls.n; k++) shsm_output_line(out, avp_pyfmt(a, "    %S", &ls.v[k]));
        avp_vec_free(&ls);
    }
    shsm_output_data(out, "terms", jterms);
    shsm_output_data(out, "hits", jhits);
    int rc = SHSM_EXIT_OK;
    if (!hits.n) {
        shsm_output_line(out, avp_pyfmt(a, "no notes mention %S", &term));
        rc = SHSM_EXIT_FINDING;
    }
    avp_vec_free(&hits);
    avp_vec_free(&terms);
    return rc;
}
