/* fragments.py source side: _local_names, source_bodies, source_rows, _single_call, _balanced, trim,
 * source_candidates, _source_text, _source_paths, cmd_source. Function bodies are read with the rule checker's lexer
 * and parser (tools/c/tools/check_rules: rules_lex, rules_parse), as the Python imports check_rules.Parsed. */
#include "frag.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/sort.h"
#include "rules_lex.h"
#include "rules_parse.h"
#include "shsm_re.h"

#include <string.h>

static const char *const STMT_END[] = {";", "{", "}", NULL};
static const char *const DECL_PREFIX[] = {"const", "static", "volatile", "unsigned", "signed", "struct", "class",
                                          "union", "enum", "register", "long", "short", NULL};
static const char *const KEYWORDS[] = {"if", "else", "for", "while", "do", "switch", "case", "default", "return",
                                       "break", "continue", "goto", "sizeof", "new", "delete", "this", "true", "false",
                                       "NULL", "operator", "static_cast", "reinterpret_cast", "const_cast",
                                       "dynamic_cast", NULL};

static bool in(AvpStr t, const char *const *set) {
    for (; *set; set++)
        if (avp_str_eqz(t, *set)) return true;
    return false;
}

static bool teq(const RlTok *t, const char *z) { return avp_str_eqz(t->t, z); }
static bool is_id(const RlTok *t) { return t->k == RL_ID; }
/* x in "([{" for a token text (a substring test in the Python; tokens are never empty) */
static bool in_chars(AvpStr t, const char *chars) { return t.n == 1 && strchr(chars, t.p[0]); }

/* _local_names(toks, lo, hi, params) */
static AvpSet *local_names(const RlTok *toks, i64 lo, i64 hi, const RlParams *params) {
    AvpSet *names = avp_set_new();
    for (size_t i = 0; i < params->n; i++)
        if (params->v[i].name.n) avp_set_add(names, params->v[i].name);
    bool start = true;
    for (i64 i = lo; i < hi; i++) {
        const RlTok *t = &toks[i];
        if (start && is_id(t) && !in(t->t, KEYWORDS)) {
            i64 j = i;
            while (j < hi && in(toks[j].t, DECL_PREFIX)) j++;
            if (j < hi && is_id(&toks[j]) && !in(toks[j].t, KEYWORDS)) {
                j++;
                while (j + 1 < hi && teq(&toks[j], "::") && is_id(&toks[j + 1])) j += 2;
                if (j < hi && teq(&toks[j], "<")) {
                    int depth = 0;
                    while (j < hi) {
                        if (teq(&toks[j], "<")) {
                            depth++;
                        } else if (teq(&toks[j], ">")) {
                            depth--;
                            if (depth == 0) {
                                j++;
                                break;
                            }
                        } else if (teq(&toks[j], ";") || teq(&toks[j], "{")) {
                            break;
                        }
                        j++;
                    }
                }
                while (j < hi && (teq(&toks[j], "*") || teq(&toks[j], "&") || teq(&toks[j], "const"))) j++;
                static const char *const AFTER[] = {"=", ";", ",", "[", ":", ")", NULL};
                if (j + 1 < hi && is_id(&toks[j]) && !in(toks[j].t, KEYWORDS) && in(toks[j + 1].t, AFTER)) {
                    avp_set_add(names, toks[j].t);
                    i64 k = j + 1;
                    int depth = 0;
                    while (k < hi && !(depth == 0 && (teq(&toks[k], ";") || teq(&toks[k], ")")))) {
                        if (in_chars(toks[k].t, "([{")) {
                            depth++;
                        } else if (in_chars(toks[k].t, ")]}")) {
                            depth--;
                        } else if (depth == 0 && teq(&toks[k], ",")) {
                            i64 m = k + 1;
                            while (m < hi && (teq(&toks[m], "*") || teq(&toks[m], "&"))) m++;
                            static const char *const AFTER2[] = {"=", ";", ",", "[", NULL};
                            if (m + 1 < hi && is_id(&toks[m]) && in(toks[m + 1].t, AFTER2)) avp_set_add(names, toks[m].t);
                        }
                        k++;
                    }
                }
            }
        }
        start = in(t->t, STMT_END) || teq(t, "(");
    }
    return names;
}

typedef struct {
    const char *path;
    AvpStr name;
    const RlTok *toks;
    size_t ntoks;
    AvpSet *names;
} Body;
typedef AVP_VEC(Body) Bodies;

/* source_bodies(paths): parses are kept alive in the arena a (token texts are views into the file text) */
static Bodies source_bodies(AvpArena *a, const AvpStrVec *paths) {
    Bodies out = {0};
    for (size_t i = 0; i < paths->n; i++) {
        const char *path = avp_cz(a, paths->v[i]);
        AvpStr text;
        if (!rl_read_source(a, path, &text)) continue;
        RlParsed *P = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            P = rl_parse(a, paths->v[i], text, NULL);
            avp_try_end(&t);
        } else {
            continue; /* a file the checker's parser cannot read is skipped, not fatal */
        }
        if (!P->ok) continue;
        for (size_t k = 0; k < P->p.funcs.n; k++) {
            const RlFunc *f = &P->p.funcs.v[k];
            if (f->is_asm || f->body < 0 || f->body_end <= f->body + 1) continue;
            i64 lo = f->body + 1, hi = f->body_end;
            Body b;
            b.path = path;
            b.name = f->name;
            b.toks = P->toks.v + lo;
            b.ntoks = (size_t)(hi - lo);
            b.names = local_names(P->toks.v, lo, hi, &f->params);
            avp_vec_push(&out, b);
        }
    }
    return out;
}

static bool re_match(const char *pat, AvpStr s) { return shsm_re_match(shsm_re(pat, 0), s, 0, AVP_END, NULL); }

bool frag_single_call(const AvpStr *texts, size_t n) {
    size_t lo = 0, hi = n;
    if (hi > lo && avp_str_eqz(texts[hi - 1], ";")) hi--;
    if (hi > lo && avp_str_eqz(texts[lo], "return")) lo++;
    if (hi - lo > 2 && avp_str_eqz(texts[lo + 1], "=")) lo += 2;
    const AvpStr *t = texts + lo;
    size_t m = hi - lo, i = 0;
    while (i < m && !avp_str_eqz(t[i], "(")) {
        if (!avp_str_eqz(t[i], "::") && !avp_str_eqz(t[i], ".") && !avp_str_eqz(t[i], "->") && !avp_str_eqz(t[i], "$") &&
            !re_match("[A-Za-z_]\\w*$", t[i]))
            return false;
        i++;
    }
    if (i == 0 || i >= m) return false;
    int depth = 0;
    for (size_t j = i; j < m; j++) {
        if (avp_str_eqz(t[j], "(")) {
            depth++;
        } else if (avp_str_eqz(t[j], ")")) {
            depth--;
            if (depth == 0) return j == m - 1;
        }
    }
    return false;
}

static bool balanced(const AvpStr *t, i64 s, i64 e) {
    int depth = 0;
    for (i64 i = s; i < e; i++) {
        if (avp_str_eqz(t[i], "(") || avp_str_eqz(t[i], "[") || avp_str_eqz(t[i], "{")) {
            depth++;
        } else if (avp_str_eqz(t[i], ")") || avp_str_eqz(t[i], "]") || avp_str_eqz(t[i], "}")) {
            if (--depth < 0) return false;
        }
    }
    return depth == 0;
}

bool frag_trim(const AvpStr *texts, size_t nt, bool prev_ok, i64 *bs, i64 *be, const char **kind) {
    i64 n = (i64)nt;
    bool have = false;
    i64 S = 0, E = 0;
    for (i64 s = 0; s < n; s++) {
        if (!((s == 0 && prev_ok) || (s > 0 && in(texts[s - 1], STMT_END)))) continue;
        for (i64 e = n; e >= 1; e--) {
            if (!(avp_str_eqz(texts[e - 1], ";") || avp_str_eqz(texts[e - 1], "}"))) continue;
            if (e - s <= 0 || (have && e - s <= E - S)) break;
            if (balanced(texts, s, e) && !avp_str_eqz(texts[s], "}") && !avp_str_eqz(texts[s], "else") &&
                !avp_str_eqz(texts[s], "break") && !avp_str_eqz(texts[s], "continue")) {
                have = true;
                S = s;
                E = e;
                break;
            }
        }
    }
    if (have) {
        *bs = S;
        *be = E;
        *kind = "statements";
        return true;
    }
    static const char *const EXPR_PREV[] = {"(", ",", "=", "return", ";", "{", "}", NULL};
    static const char *const EXPR_NEXT[] = {")", ",", ";", "]", NULL};
    for (i64 s = 0; s < n; s++) {
        if (s > 0 && !in(texts[s - 1], EXPR_PREV)) continue;
        for (i64 e = n; e > s; e--) {
            if (have && e - s <= E - S) break;
            if (e < n && !in(texts[e], EXPR_NEXT)) continue;
            if (e == n && !re_match("[\\w\")\\]]", texts[e - 1])) continue; /* the run stops inside an expression */
            bool spans = false;
            for (i64 x = s; x < e && !spans; x++)
                if (in(texts[x], STMT_END)) spans = true;
            if (spans) continue; /* an expression does not span statements */
            if (balanced(texts, s, e)) {
                have = true;
                S = s;
                E = e;
                break;
            }
        }
    }
    if (!have) return false;
    *bs = S;
    *be = E;
    *kind = "expression";
    return true;
}

/* ---- source_candidates ---- */

typedef struct {
    const char *path, *function;
    i64 line, end_line;
} Site;

typedef struct {
    i64 tokens, functions, score;
    const char *kind;
    AvpStr text;
    AVP_VEC(Site) sites;
    AVP_VEC(AvpStrVec) params;
} Cand;
typedef AVP_VEC(Cand) Cands;

typedef struct {
    FragCorpus *c;
    const Bodies *bodies;
    AvpStrVec vars; /* variable id -> its text */
} Src;

/* re.sub(r"\b(\w+) \(", ...): keywords keep their space before "(" */
static void call_paren(void *ctx, const ShsmReMatch *m, AvpBuf *b) {
    (void)ctx;
    AvpStr w = shsm_re_group(m, 1);
    if (in(w, KEYWORDS)) {
        avp_buf_put(b, shsm_re_group(m, 0));
    } else {
        avp_buf_put(b, w);
        avp_buf_putc(b, '(');
    }
}

/* _source_text(texts, corpus, occ0, s) */
static AvpStr source_text(AvpArena *a, const Src *S, const AvpStr *texts, size_t n, FragPos occ0, i64 s) {
    const FragFn *f = &S->c->fns.v[occ0.fi];
    AvpIMap *m = avp_imap_new();
    AvpStrVec out = {0};
    for (size_t k = 0; k < n; k++) {
        AvpStr x = texts[k];
        if (avp_str_eqz(x, "$")) {
            i64 row = occ0.p + s + (i64)k;
            i32 v = f->rpool[f->roff[row]];
            bool created;
            AvpVal *val = avp_imap_put(m, (u64)(u32)v, &created);
            if (created) val->i = (i64)avp_imap_len(m);
            x = avp_pyfmt(a, "P%d", val->i);
        }
        avp_vec_push(&out, x);
    }
    AvpStr r = avp_str_join(a, AVP_S(" "), out.v, out.n);
    avp_vec_free(&out);
    avp_imap_free(m);
    r = shsm_re_sub(a, shsm_re(" ?(->|\\.|::) ?", 0), AVP_S("\\1"), r, 0);
    r = shsm_re_sub(a, shsm_re(" ([,;)\\]])", 0), AVP_S("\\1"), r, 0);
    r = avp_str_replace(a, r, AVP_S("( "), AVP_S("("), -1);
    r = avp_str_replace(a, r, AVP_S("[ "), AVP_S("["), -1);
    return shsm_re_sub_fn(a, shsm_re("\\b(\\w+) \\(", 0), call_paren, NULL, r, 0, NULL);
}

static int cmp_cand(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Cand *a = x, *b = y;
    if (a->score != b->score) return a->score > b->score ? -1 : 1;
    int c = avp_str_cmp(avp_str(a->sites.v[0].path), avp_str(b->sites.v[0].path));
    if (c) return c;
    if (a->sites.v[0].line != b->sites.v[0].line) return a->sites.v[0].line < b->sites.v[0].line ? -1 : 1;
    return 0;
}

static Cands source_candidates(AvpArena *a, const Bodies *bodies, i64 min_tokens, i64 min_sites) {
    Src S = {frag_corpus_new(1), bodies, {0}};
    AvpMap *varid = avp_map_new();
    for (size_t b = 0; b < bodies->n; b++) {
        const Body *body = &bodies->v[b];
        FragRowIn *rows = avp_alloc(a, (body->ntoks ? body->ntoks : 1) * sizeof *rows);
        for (size_t i = 0; i < body->ntoks; i++) {
            const RlTok *t = &body->toks[i];
            FragRowIn *r = &rows[i];
            r->disp = t->t;
            if (t->k == RL_STR || (t->k == RL_ID && avp_set_has(body->names, t->t))) {
                bool created;
                AvpVal *v = avp_map_put(varid, t->t, &created);
                if (created) {
                    v->i = (i64)S.vars.n;
                    avp_vec_push(&S.vars, t->t);
                }
                i32 *reg = avp_alloc(a, sizeof *reg);
                *reg = (i32)v->i;
                r->tok[0] = AVP_S("$");
                r->regs = reg;
                r->nregs = 1;
            } else {
                r->tok[0] = t->t;
            }
        }
        frag_corpus_add(S.c, rows, body->ntoks);
    }
    avp_map_free(varid);
    const AvpStrVec *rev = (const AvpStrVec *)&S.c->rev[0];
    Cands cands = {0};
    AvpSet *seen = avp_set_new();
    FragRepeats reps = frag_repeats(S.c, 0, (i32)min_tokens, (i32)min_sites);
    AvpBuf kb = {0};
    for (size_t ri = 0; ri < reps.n; ri++) {
        const FragRepeat *rp = &reps.v[ri];
        FragPos o0 = rp->occ.v[0];
        const FragFn *f0 = &S.c->fns.v[o0.fi];
        AvpStr *texts = avp_alloc(a, (size_t)rp->length * sizeof *texts);
        for (i32 k = 0; k < rp->length; k++) texts[k] = rev->v[f0->tok[0][o0.p + k]];
        bool prev_ok = true;
        for (size_t o = 0; o < rp->occ.n && prev_ok; o++) {
            FragPos q = rp->occ.v[o];
            if (q.p != 0 && !in(rev->v[S.c->fns.v[q.fi].tok[0][q.p - 1]], STMT_END)) prev_ok = false;
        }
        i64 s, e;
        const char *kind;
        if (!frag_trim(texts, (size_t)rp->length, prev_ok, &s, &e, &kind) || e - s < min_tokens) continue;
        if (frag_single_call(texts + s, (size_t)(e - s))) continue;
        avp_buf_clear(&kb);
        for (i64 k = s; k < e; k++) {
            avp_buf_put(&kb, texts[k]);
            avp_buf_putc(&kb, '\0');
        }
        avp_buf_putc(&kb, '\x01');
        for (size_t o = 0; o < rp->occ.n; o++) {
            i32 pos[2] = {rp->occ.v[o].fi, rp->occ.v[o].p + (i32)s};
            avp_buf_putn(&kb, (const char *)pos, sizeof pos);
        }
        if (!avp_set_add(seen, avp_buf_str(&kb))) continue;
        Cand c;
        memset(&c, 0, sizeof c);
        AvpISet *fis = avp_iset_new();
        for (size_t o = 0; o < rp->occ.n; o++) {
            FragPos q = rp->occ.v[o];
            avp_iset_add(fis, (u64)(u32)q.fi);
            const Body *body = &bodies->v[q.fi];
            const FragFn *f = &S.c->fns.v[q.fi];
            AvpIMap *m = avp_imap_new();
            size_t idx = 0;
            for (i64 k = q.p + s; k < q.p + e; k++) {
                for (u32 x = f->roff[k]; x < f->roff[k + 1]; x++) {
                    i32 v = f->rpool[x];
                    bool created;
                    avp_imap_put(m, (u64)(u32)v, &created);
                    if (!created) continue;
                    while (c.params.n <= idx) avp_vec_push_zero(&c.params);
                    avp_vec_push(&c.params.v[idx], S.vars.v[v]);
                    idx++;
                }
            }
            avp_imap_free(m);
            Site site = {body->path, avp_cz(a, body->name), body->toks[q.p + s].line, body->toks[q.p + e - 1].line};
            avp_vec_push(&c.sites, site);
        }
        c.functions = (i64)avp_iset_len(fis);
        avp_iset_free(fis);
        c.tokens = e - s;
        c.score = c.functions * c.tokens;
        c.kind = kind;
        c.text = source_text(a, &S, texts + s, (size_t)(e - s), o0, s);
        avp_vec_push(&cands, c);
    }
    avp_buf_free(&kb);
    avp_set_free(seen);
    frag_repeats_free(&reps);
    avp_sort(cands.v, cands.n, sizeof *cands.v, cmp_cand, NULL, false);
    /* a candidate whose sites all overlap a higher-ranked candidate's sites is the same code found again */
    Cands kept = {0};
    for (size_t i = 0; i < cands.n; i++) {
        const Cand *c = &cands.v[i];
        bool dup = false;
        for (size_t k = 0; k < kept.n && !dup; k++) {
            const Cand *K = &kept.v[k];
            bool all = true;
            for (size_t x = 0; x < c->sites.n && all; x++) {
                const Site *p = &c->sites.v[x];
                bool any = false;
                for (size_t y = 0; y < K->sites.n && !any; y++) {
                    const Site *q = &K->sites.v[y];
                    if (strcmp(p->path, q->path) == 0 && p->line <= q->end_line && q->line <= p->end_line) any = true;
                }
                if (!any) all = false;
            }
            if (all) dup = true;
        }
        if (!dup) avp_vec_push(&kept, *c);
    }
    avp_vec_free(&cands);
    avp_vec_free(&S.vars);
    frag_corpus_free(S.c);
    return kept;
}

/* ---- _source_paths, cmd_source ---- */

static bool src_ext(AvpStr f) {
    return avp_str_endswith(f, AVP_S(".c")) || avp_str_endswith(f, AVP_S(".cpp")) ||
           avp_str_endswith(f, AVP_S(".cp")) || avp_str_endswith(f, AVP_S(".cc"));
}

static AvpStrVec source_paths(AvpArena *a, const ShsmRepo *r, const FragArgs *args) {
    AvpStrVec files = {0};
    if (args->paths.n) {
        for (size_t i = 0; i < args->paths.n; i++) {
            const char *p = avp_cz(a, args->paths.v[i]);
            if (!avp_path_isabs(p)) p = avp_path_join(a, r->root, p);
            if (avp_isdir(p)) {
                AvpWalk *w = avp_walk_open(p, true);
                AvpWalkItem it;
                memset(&it, 0, sizeof it);
                while (avp_walk_next(w, &it))
                    for (size_t k = 0; k < it.files.n; k++)
                        if (src_ext(it.files.v[k]))
                            avp_vec_push(&files, avp_str(avp_path_join(a, avp_cz(a, it.dirpath), avp_cz(a, it.files.v[k]))));
                avp_walk_close(w);
            } else if (avp_isfile(p)) {
                avp_vec_push(&files, avp_str(p));
            } else {
                shsm_raise(SHSM_EXC_USAGE, "no such file or directory: %s", p);
            }
        }
        avp_sort_strs(files.v, files.n);
        return files;
    }
    const ShsmUnits *us = shsm_load_units(r);
    AvpSet *set = avp_set_new();
    for (size_t i = 0; i < us->n; i++) {
        const ShsmUnit *u = us->v[i];
        if (!u->source) continue;
        if (args->lib && *args->lib) {
            AvpStr pre = avp_str_rstrip_chars(avp_str_lower(a, avp_str(args->lib)), AVP_S("/"));
            AvpStr prefix = avp_pyfmt(a, "%S/", &pre);
            if (!frag_lib_matches(a, u->library, args->lib) &&
                !avp_str_startswith(avp_str_lower(a, avp_str(u->path)), prefix))
                continue;
        }
        const char *p = avp_path_join(a, r->root, u->source);
        if (avp_isfile(p) && avp_set_add(set, avp_str(p))) avp_vec_push(&files, avp_str(p));
    }
    avp_set_free(set);
    avp_sort_strs(files.v, files.n);
    return files;
}

static AvpJson *site_json(AvpArena *a, const ShsmRepo *r, const Site *s) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("path"), avp_json_strz(a, shsm_repo_rel(a, r, s->path)));
    avp_json_set(a, d, AVP_S("line"), avp_json_int(a, s->line));
    avp_json_set(a, d, AVP_S("end_line"), avp_json_int(a, s->end_line));
    avp_json_set(a, d, AVP_S("function"), avp_json_strz(a, s->function));
    return d;
}

static AvpJson *cand_json(AvpArena *a, const ShsmRepo *r, const Cand *c) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("tokens"), avp_json_int(a, c->tokens));
    AvpJson *sites = avp_json_arr(a);
    for (size_t i = 0; i < c->sites.n; i++) avp_json_arr_push(a, sites, site_json(a, r, &c->sites.v[i]));
    avp_json_set(a, d, AVP_S("sites"), sites);
    avp_json_set(a, d, AVP_S("functions"), avp_json_int(a, c->functions));
    avp_json_set(a, d, AVP_S("score"), avp_json_int(a, c->score));
    avp_json_set(a, d, AVP_S("kind"), avp_json_strz(a, c->kind));
    avp_json_set(a, d, AVP_S("text"), avp_json_str(a, c->text));
    AvpJson *params = avp_json_arr(a);
    for (size_t i = 0; i < c->params.n; i++) {
        AvpJson *p = avp_json_arr(a);
        for (size_t k = 0; k < c->params.v[i].n; k++) avp_json_arr_push(a, p, avp_json_str(a, c->params.v[i].v[k]));
        avp_json_arr_push(a, params, p);
    }
    avp_json_set(a, d, AVP_S("params"), params);
    return d;
}

/* Counter(vals).most_common(4) as "v (n) | ..." */
static AvpStr most_common4(AvpArena *a, const AvpStrVec *vals) {
    AvpMap *m = avp_map_new();
    AvpStrVec keys = {0};
    AvpI64Vec counts = {0};
    for (size_t i = 0; i < vals->n; i++) {
        bool created;
        AvpVal *v = avp_map_put(m, vals->v[i], &created);
        if (created) {
            v->i = (i64)keys.n;
            avp_vec_push(&keys, vals->v[i]);
            avp_vec_push(&counts, 0);
        }
        counts.v[v->i]++;
    }
    size_t k = keys.n;
    size_t *idx = avp_alloc(a, (k ? k : 1) * sizeof *idx);
    for (size_t i = 0; i < k; i++) idx[i] = i;
    for (size_t i = 1; i < k; i++)
        for (size_t j = i; j > 0 && counts.v[idx[j - 1]] < counts.v[idx[j]]; j--) {
            size_t t = idx[j];
            idx[j] = idx[j - 1];
            idx[j - 1] = t;
        }
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < k && i < 4; i++) {
        if (i) avp_buf_putz(&b, " | ");
        avp_buf_pyfmt(&b, "%S (%d)", &keys.v[idx[i]], counts.v[idx[i]]);
    }
    avp_vec_free(&keys);
    avp_vec_free(&counts);
    avp_map_free(m);
    return avp_buf_str(&b);
}

int frag_cmd_source(const FragArgs *args, const ShsmRepo *r, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    AvpStrVec files = source_paths(a, r, args);
    if (!files.n) shsm_raise(SHSM_EXC_USAGE, "no source files selected");
    double t = avp_time();
    Bodies bodies = source_bodies(a, &files);
    Cands cands = source_candidates(a, &bodies, args->min_tokens, args->min_sites);
    shsm_output_line(out, avp_pyfmt(a, "%d candidate(s) in %d function bodies of %d file(s), min %d tokens at %d sites"
                                       " (%.1f s)",
                                    (i64)cands.n, (i64)bodies.n, (i64)files.n, args->min_tokens, args->min_sites,
                                    avp_time() - t));
    i64 cap = args->limit / 6 > 1 ? args->limit / 6 : 1;
    size_t shown = args->all || (size_t)cap >= cands.n ? cands.n : (size_t)cap;
    for (size_t i = 0; i < shown; i++) {
        const Cand *c = &cands.v[i];
        shsm_output_linez(out, "");
        shsm_output_line(out, avp_pyfmt(a, "#%d  %d tokens x %d function(s) = %d  (%s)", (i64)i + 1, c->tokens,
                                        c->functions, c->score, c->kind));
        AvpStr text = c->text;
        if (avp_u8_len(text) > 200) {
            AvpStr head = avp_u8_slice(text, 0, 197);
            text = avp_pyfmt(a, "%S...", &head);
        }
        shsm_output_line(out, avp_pyfmt(a, "  %S", &text));
        if (c->params.n) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t k = 0; k < c->params.n; k++) avp_buf_pyfmt(&b, k ? ", P%d" : "P%d", (i64)k + 1);
            AvpStr sig = avp_buf_str(&b);
            shsm_output_line(out, avp_pyfmt(a, "  helper shape: Helper( %S )", &sig));
            for (size_t k = 0; k < c->params.n; k++) {
                AvpStr mc = most_common4(a, &c->params.v[k]);
                shsm_output_line(out, avp_pyfmt(a, "    P%d: %S", (i64)k + 1, &mc));
            }
        } else {
            shsm_output_linez(out, "  helper shape: Helper() (no varying names)");
        }
        for (size_t k = 0; k < c->sites.n && k < 8; k++)
            shsm_output_line(out, avp_pyfmt(a, "  %s:%d  %s", shsm_repo_rel(a, r, c->sites.v[k].path),
                                            c->sites.v[k].line, c->sites.v[k].function));
        if (c->sites.n > 8) shsm_output_line(out, avp_pyfmt(a, "  ... %d more site(s)", (i64)(c->sites.n - 8)));
    }
    shsm_output_data(out, "files", avp_json_int(a, (i64)files.n));
    shsm_output_data(out, "bodies", avp_json_int(a, (i64)bodies.n));
    AvpJson *list = avp_json_arr(a);
    size_t nj = args->all ? cands.n : (args->limit < (i64)cands.n ? (size_t)args->limit : cands.n);
    if (out->as_json)
        for (size_t i = 0; i < nj; i++) avp_json_arr_push(a, list, cand_json(a, r, &cands.v[i]));
    shsm_output_data(out, "candidates", list);
    return SHSM_EXIT_OK;
}
