/* levers: scoring, the search, ranking, diffs and the report (levers.py classify, Scorer, search, ranked, make_diff,
 * apply_patch, safe_name, write_outputs, stats_line). */
#include "levers_search.h"

#include <string.h>

#include "cc_compare.h"
#include "levers_doc.h"
#include "neutral.h"

/* ---- text ---- */

char *lv_utf8(AvpArena *a, LvS s) {
    char *o = avp_alloc(a, s.n * 2 + 1);
    size_t k = 0;
    for (size_t i = 0; i < s.n; i++) {
        u8 c = (u8)s.p[i];
        if (c < 0x80)
            o[k++] = (char)c;
        else {
            o[k++] = (char)(0xC0 | (c >> 6));
            o[k++] = (char)(0x80 | (c & 0x3F));
        }
    }
    o[k] = 0;
    return o;
}

const char *lv_sha(AvpArena *a, LvS text) {
    char *u = lv_utf8(a, text);
    return avp_sha1_hex(a, u, strlen(u));
}

static size_t brk_len(LvS s, size_t i) {
    u8 c = (u8)s.p[i];
    if (c == '\r') return (i + 1 < s.n && s.p[i + 1] == '\n') ? 2 : 1;
    if (c == '\n' || c == 0x0b || c == 0x0c || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x85) return 1;
    return 0;
}

AvpStrVec lv_splitlines_keep(LvS s) {
    AvpStrVec out = {0};
    size_t i = 0;
    while (i < s.n) {
        size_t j = i, b = 0;
        while (j < s.n && !(b = brk_len(s, j))) j++;
        size_t end = j < s.n ? j + b : s.n;
        avp_vec_push(&out, ((AvpStr){s.p + i, end - i}));
        i = end;
    }
    return out;
}

/* ---- scores ---- */

static i64 key_cmp(const LvScore *x, const LvScore *y) {
    if (x->rows != y->rows) return x->rows < y->rows ? -1 : 1;
    if (x->tm != y->tm) return x->tm < y->tm ? -1 : 1;
    return 0;
}

static const LvOther *other_of(const LvScore *s, const char *name) {
    for (size_t i = 0; i < s->nothers; i++)
        if (!strcmp(s->others[i].name, name)) return &s->others[i];
    return NULL;
}

static void sort_cz(AvpCzVec *v) {
    for (size_t i = 1; i < v->n; i++)
        for (size_t j = i; j > 0 && strcmp(v->v[j - 1], v->v[j]) > 0; j--) {
            char *t = v->v[j];
            v->v[j] = v->v[j - 1];
            v->v[j - 1] = t;
        }
}

const char *lv_classify(AvpArena *a, const LvScore *base, const LvScore *sc, AvpCzVec *broken) {
    (void)a;
    *broken = (AvpCzVec){0};
    if (!sc->ok) return "error";
    for (size_t i = 0; i < base->nothers; i++) {
        const LvOther *b = &base->others[i], *c = other_of(sc, b->name);
        if (!c || (b->exact && !c->exact) || c->rows > b->rows || c->tm > b->tm)
            avp_vec_push(broken, (char *)b->name);
    }
    sort_cz(broken);
    if (sc->exact || key_cmp(sc, base) < 0) {
        if (broken->n) return "breaks";
        avp_vec_free(broken);
        *broken = (AvpCzVec){0};
        return "improved";
    }
    if (!key_cmp(sc, base)) return "neutral";
    return "worse";
}

/* ---- the search ---- */

static const char *const ORDER_DEFAULT[] = {"decl", "expr", "control", "temp", "type", "order", NULL};
static const char *const ORDER_REG[] = {"decl", "temp", "order", "expr", "type", "control", NULL};
static const char *const ORDER_CTRL[] = {"control", "expr", "decl", "temp", "order", "type", NULL};

const char *const *lv_family_order(const char *wall) {
    if (wall && !strcmp(wall, "register/schedule")) return ORDER_REG;
    if (wall && !strcmp(wall, "control flow")) return ORDER_CTRL;
    return ORDER_DEFAULT;
}

static void bump(AvpMap *stats, const char *k) { avp_map_put(stats, avp_str(k), NULL)->i += 1; }

typedef struct {
    LvScoreFn fn;
    void *ctx;
    double deadline;
} RunCtx;
typedef struct {
    LvScore *sc;
    int code;
    const char *msg;
} RunSlot;

static int run_one_cand(void *vctx, size_t i, const void *item, void *result) {
    (void)i;
    RunCtx *rc = vctx;
    const LvState *st = item;
    RunSlot *slot = result;
    if (avp_time() >= rc->deadline) return 0;
    AvpArena *a = avp_arena();
    AvpTry t;
    if (AVP_TRY(&t)) {
        slot->sc = rc->fn(rc->ctx, a, st->text);
        avp_try_end(&t);
    } else {
        slot->code = t.code ? t.code : -1;
        slot->msg = avp_strdup(a, t.msg);
    }
    return 0;
}

/* stable sort of idx[0..n) by cmp */
typedef int (*IdxCmp)(const void *ctx, size_t x, size_t y);
static void stable_idx(size_t *idx, size_t n, IdxCmp cmp, const void *ctx) {
    for (size_t i = 1; i < n; i++) {
        size_t v = idx[i], j = i;
        while (j > 0 && cmp(ctx, idx[j - 1], v) > 0) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = v;
    }
}

static int frontier_cmp(const void *ctx, size_t x, size_t y) {
    const LvResult *r = ctx;
    const LvScore *a = r[x].sc, *b = r[y].sc;
    if (a->exact != b->exact) return a->exact ? -1 : 1;
    return (int)key_cmp(a, b);
}

LvSearch lv_search(AvpArena *a, LvS text0, const char *symbol, LvScoreFn fn, void *ctx, const LvSearchOpts *o) {
    double t0 = avp_time();
    double deadline = o->deadline ? o->deadline : t0 + 3600;
    LvSearch res = {0};
    res.stats = avp_map_new();
    LvScore *base = o->base ? o->base : fn(ctx, a, text0);
    res.base = base;
    if (!base->ok) shsm_raise(SHSM_EXC_USAGE, "the unmodified source does not compile: %s", base->error ? base->error : "None");
    const char *const *fo = lv_family_order(o->wall);
    const char *order[6];
    size_t norder = 0;
    for (size_t i = 0; fo[i]; i++) {
        bool want = !o->families || !o->nfam;
        for (size_t k = 0; !want && k < o->nfam; k++)
            if (!strcmp(o->families[k], fo[i])) want = true;
        if (want) order[norder++] = fo[i];
    }
    LvCtx *ctx0 = lv_ctx_new(a, text0, symbol);
    LvCounter orig_fn = lv_fake_forms(a, lv_slice(ctx0, ctx0->span0, ctx0->span1), symbol);
    AvpSet *seen = avp_set_new(), *seen_sig = avp_set_new();
    avp_set_add(seen, avp_str(lv_sha(a, text0)));
    if (base->sig) avp_set_add(seen_sig, avp_str(base->sig));
    AVP_VEC(LvState) states = {0};
    avp_vec_push(&states, ((LvState){text0, NULL, 0}));
    i64 depth = o->depth < LV_MAX_DEPTH ? o->depth : LV_MAX_DEPTH;
    if (depth < 1) depth = 1;
    for (i64 level = 1; level <= depth; level++) {
        if (avp_time() >= deadline || base->exact) break;
        res.levels = level;
        AVP_VEC(LvState) cands = {0};
        for (size_t si = 0; si < states.n; si++) {
            LvState *st = &states.v[si];
            LvCandidates gen = {0};
            AvpTry t;
            bool ok = true;
            if (AVP_TRY(&t)) {
                LvCtx *cx = lv_ctx_new(a, st->text, symbol);
                gen = lv_generate(cx, order, norder);
                avp_try_end(&t);
            } else {
                if (!lv_exc_caught(t.code)) avp_throw(t.code, "%s", t.msg);
                ok = false;
            }
            if (!ok) {
                bump(res.stats, "unparsable state");
                continue;
            }
            for (size_t ci = 0; ci < gen.n; ci++) {
                LvCandidate *cd = &gen.v[ci];
                bump(res.stats, "generated");
                const char *h = lv_sha(a, cd->text);
                if (!avp_set_add(seen, avp_str(h))) {
                    bump(res.stats, "duplicate");
                    continue;
                }
                LvS fn_text = {cd->text.p, 0};
                {
                    i64 s0 = cd->span0, s1 = cd->span1, n = (i64)cd->text.n;
                    if (s0 > n) s0 = n;
                    if (s1 > n) s1 = n;
                    if (s0 < 0) s0 = 0;
                    if (s1 < s0) s1 = s0;
                    fn_text = (LvS){cd->text.p + s0, (size_t)(s1 - s0)};
                }
                AvpCzVec why = lv_fake_reasons(a, &orig_fn, fn_text, symbol);
                bool fake = why.n > 0;
                avp_vec_free(&why);
                if (fake) {
                    bump(res.stats, "fake");
                    continue;
                }
                LvStep *path = avp_alloc(a, sizeof(LvStep) * (st->npath + 1));
                if (st->npath) memcpy(path, st->path, sizeof(LvStep) * st->npath);
                path[st->npath] = (LvStep){cd->family, cd->lever, lv_utf8(a, cd->desc), cd->line};
                avp_vec_push(&cands, ((LvState){cd->text, path, st->npath + 1}));
            }
            avp_vec_free(&gen);
        }
        /* cands.sort(key=order.index(family)): stable */
        AVP_VEC(LvState) sorted = {0};
        for (size_t f = 0; f < norder; f++)
            for (size_t i = 0; i < cands.n; i++)
                if (!strcmp(cands.v[i].path[cands.v[i].npath - 1].family, order[f])) avp_vec_push(&sorted, cands.v[i]);
        avp_vec_free(&cands);
        RunSlot *slots = avp_alloc(a, sizeof(RunSlot) * (sorted.n + 1));
        RunCtx rc = {fn, ctx, deadline};
        if (sorted.n)
            avp_pool_map(NULL, o->jobs < 1 ? 1 : (int)o->jobs, run_one_cand, &rc, sorted.v, sorted.n, sizeof(LvState),
                         slots, sizeof(RunSlot));
        for (size_t i = 0; i < sorted.n; i++)
            if (slots[i].code) avp_throw(slots[i].code, "%s", slots[i].msg);
        size_t lv0 = res.results.n;
        for (size_t i = 0; i < sorted.n; i++) {
            if (!slots[i].sc) {
                bump(res.stats, "skipped (budget)");
                continue;
            }
            LvResult r = {sorted.v[i], slots[i].sc, NULL, {0}};
            r.status = lv_classify(a, base, r.sc, &r.broken);
            bump(res.stats, !strcmp(r.status, "error") ? "failed to compile" : r.status);
            avp_vec_push(&res.results, r);
        }
        avp_vec_free(&sorted);
        LvResult *lres = res.results.v + lv0;
        size_t nl = res.results.n - lv0;
        if (o->stop_on_exact) {
            bool hit = false;
            for (size_t i = 0; i < nl; i++)
                if (lres[i].sc->exact && !strcmp(lres[i].status, "improved")) hit = true;
            if (hit) break;
        }
        size_t *idx = avp_alloc(a, sizeof(size_t) * (nl + 1));
        for (size_t i = 0; i < nl; i++) idx[i] = i;
        stable_idx(idx, nl, frontier_cmp, lres);
        AVP_VEC(LvState) nxt = {0};
        AvpSet *sigs = avp_set_new();
        AvpStrVec sigl = {0};
        for (size_t q = 0; q < nl; q++) {
            LvResult *r = &lres[idx[q]];
            if ((!strcmp(r->status, "improved") || !strcmp(r->status, "neutral")) && !r->broken.n && r->sc->sig &&
                !avp_set_has(seen_sig, avp_str(r->sc->sig)) && !avp_set_has(sigs, avp_str(r->sc->sig))) {
                avp_set_add(sigs, avp_str(r->sc->sig));
                avp_vec_push(&sigl, avp_str(r->sc->sig));
                avp_vec_push(&nxt, r->st);
            }
            if ((i64)nxt.n >= o->frontier) break;
        }
        for (size_t q = 0; q < sigl.n; q++) avp_set_add(seen_sig, sigl.v[q]);
        avp_vec_free(&sigl);
        avp_set_free(sigs);
        avp_vec_free(&states);
        states.v = nxt.v;
        states.n = nxt.n;
        states.cap = nxt.cap;
        if (!states.n) break;
    }
    avp_vec_free(&states);
    avp_set_free(seen);
    avp_set_free(seen_sig);
    res.seconds = avp_time() - t0;
    return res;
}

static int ranked_cmp(const void *ctx, size_t x, size_t y) {
    const LvResult *r = ctx;
    const LvScore *a = r[x].sc, *b = r[y].sc;
    if (a->exact != b->exact) return a->exact ? -1 : 1;
    i64 k = key_cmp(a, b);
    if (k) return (int)k;
    if (r[x].st.npath != r[y].st.npath) return r[x].st.npath < r[y].st.npath ? -1 : 1;
    return 0;
}

void lv_ranked(AvpArena *a, const LvResults *res, LvResults *imp, LvResults *brk) {
    *imp = (LvResults){0};
    *brk = (LvResults){0};
    size_t n = res->n;
    size_t *idx = avp_alloc(a, sizeof(size_t) * (n + 1));
    for (size_t i = 0; i < n; i++) idx[i] = i;
    stable_idx(idx, n, ranked_cmp, res->v);
    AvpSet *sigs = avp_set_new();
    for (size_t q = 0; q < n; q++) {
        const LvResult *r = &res->v[idx[q]];
        if (!strcmp(r->status, "improved") && !(r->sc->sig && avp_set_has(sigs, avp_str(r->sc->sig)))) {
            if (r->sc->sig) avp_set_add(sigs, avp_str(r->sc->sig));
            avp_vec_push(imp, *r);
        } else if (!strcmp(r->status, "breaks"))
            avp_vec_push(brk, *r);
    }
    avp_set_free(sigs);
}

/* ---- the scorer of the unit ---- */

static const char *trunc_chars(AvpArena *a, AvpStr s, size_t n) {
    size_t i = 0, k = 0;
    while (i < s.n && k < n) {
        u8 c = (u8)s.p[i];
        size_t l = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        i += l;
        k++;
    }
    if (i > s.n) i = s.n;
    return avp_cz(a, (AvpStr){s.p, i});
}

static LvScore *fail(AvpArena *a, const char *err) {
    LvScore *sc = AVP_NEW(a, LvScore);
    sc->ok = false;
    sc->error = err;
    return sc;
}

LvScore *lv_scorer_score(void *ctx, AvpArena *a, LvS text) {
    LvScorer *s = ctx;
    i64 n = avp_atomic_add64(&s->counter, 1) - 1;
    const char *d = avp_path_join(a, s->workroot, avp_sprintf(a, "c%05lld", (long long)n));
    LvScore *out = NULL;
    AvpTry t;
    int code = 0;
    const char *msg = NULL;
    if (AVP_TRY(&t)) {
        if (!avp_makedirs(d, true)) shsm_raise_io();
        const char *src = avp_path_join(a, d, s->basename);
        if (!avp_write_file(src, text.p, text.n)) shsm_raise_io();
        const char *obj = shsm_object_for(a, src, d);
        ShsmCompileOpts opts = {0};
        opts.extra_flags = s->extra;
        opts.nextra = s->nextra;
        opts.sjis = -1;
        AvpStrVec argv = shsm_compile_argv(a, s->repo, s->unit, src, obj, &opts);
        const char **cargv = avp_alloc(a, sizeof(char *) * (argv.n + 1));
        for (size_t i = 0; i < argv.n; i++) cargv[i] = avp_cz(a, argv.v[i]);
        AvpProcResult r = avp_run(&(AvpProcSpec){.argv = cargv, .cwd = s->repo->root, .out = AVP_IO_NULL,
                                                 .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                                 .timeout = 300, .creation_flags = AVP_BELOW_NORMAL_PRIORITY_CLASS,
                                                 .arena = a});
        if (r.status == AVP_PROC_START_FAILED) shsm_raise(SHSM_EXC_USAGE, "cannot run %s: %s", cargv[0], r.message);
        if (r.status != AVP_PROC_OK || r.returncode != 0) {
            r = avp_run(&(AvpProcSpec){.argv = cargv, .cwd = s->repo->root, .out = AVP_IO_CAPTURE,
                                       .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                       .timeout = 300, .creation_flags = AVP_BELOW_NORMAL_PRIORITY_CLASS, .arena = a});
            AvpStr output = avp_str_concat(a, r.out, r.err);
            AvpStr err = neutral_first_error(a, output);
            if (!err.p) {
                err = AVP_S("compile failed");
                AvpStrVec lines = avp_str_splitlines(output, false);
                for (size_t i = 0; i < lines.n; i++)
                    if (avp_str_strip(lines.v[i]).n) {
                        err = lines.v[i];
                        break;
                    }
                avp_vec_free(&lines);
            }
            out = fail(a, trunc_chars(a, err, 200));
        } else {
            CcRepo cr = {s->repo->root, s->repo->version};
            CcError cerr = {0};
            CcObjectComparison *oc = cc_compare_object(a, &cr, avp_str(s->unit->name), obj, NULL, true, NULL, NULL,
                                                       NULL, NULL, NULL, &cerr);
            if (!oc) {
                int ex = cerr.kind == CC_ERR_KEY ? SHSM_EXC_KEY
                         : cerr.kind == CC_ERR_OS ? SHSM_EXC_OS
                         : cerr.kind == CC_ERR_VALUE ? SHSM_EXC_VALUE
                                                     : SHSM_EXC_USAGE;
                shsm_raise(ex, "%.*s", AVP_SF(cerr.msg));
            }
            const CcCompareResult *tgt = NULL;
            AvpVal *v = avp_map_get(oc->results, avp_str(s->symbol));
            if (v) tgt = v->p;
            AvpMapIter it = {0};
            while (!tgt && avp_map_next(oc->results, &it)) {
                const CcCompareResult *c = it.val->p;
                if (avp_str_eqz(c->retail_name, s->symbol)) tgt = c;
            }
            if (!tgt)
                out = fail(a, "function not in the compiled object");
            else {
                out = AVP_NEW(a, LvScore);
                out->ok = true;
                size_t no = avp_map_len(oc->results);
                out->others = avp_alloc(a, sizeof(LvOther) * (no + 1));
                AvpBuf sig;
                avp_buf_init(&sig, a);
                bool first = true;
                for (size_t i = 0; i < tgt->npairs; i++) {
                    const CcPair *p = &tgt->pairs[i];
                    if (p->compiled) {
                        if (!first) avp_buf_putc(&sig, '\n');
                        avp_buf_put(&sig, p->compiled->norm);
                        first = false;
                    }
                    if (cc_pair_differs(p)) out->rows++;
                }
                out->sig = avp_sha1_hex(a, sig.p ? sig.p : "", sig.n);
                out->tm = (i64)tgt->target_mismatches->n;
                out->exact = tgt->exact;
                out->nrows = (i64)tgt->counts.retail_rows;
                it = (AvpMapIter){0};
                while (avp_map_next(oc->results, &it)) {
                    const CcCompareResult *c = it.val->p;
                    if (c == tgt) continue;
                    i64 rows = 0;
                    for (size_t i = 0; i < c->npairs; i++)
                        if (cc_pair_differs(&c->pairs[i])) rows++;
                    out->others[out->nothers++] =
                        (LvOther){avp_cz(a, it.key), rows, (i64)c->target_mismatches->n, c->exact};
                }
            }
        }
        avp_try_end(&t);
    } else {
        code = t.code;
        msg = avp_strdup(a, t.msg);
    }
    avp_rmtree(d, 1);
    if (code) avp_throw(code, "%s", msg);
    return out;
}

/* ---- output ---- */

LvS lv_make_diff(AvpArena *a, LvS old, LvS nw, const char *relpath) {
    AvpStrVec A = lv_splitlines_keep(old), B = lv_splitlines_keep(nw);
    AvpUdiff o = AVP_UDIFF_DEFAULT;
    o.fromfile = avp_str(avp_sprintf(a, "a/%s", relpath));
    o.tofile = avp_str(avp_sprintf(a, "b/%s", relpath));
    o.n = 3;
    AvpStrVec lines = avp_unified_diff_lines(a, A.v, A.n, B.v, B.n, &o);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = lines.v[i];
        if (avp_str_startswith(ln, AVP_S("---")) || avp_str_startswith(ln, AVP_S("+++"))) {
            while (ln.n && (ln.p[ln.n - 1] == '\r' || ln.p[ln.n - 1] == '\n')) ln.n--;
            avp_buf_put(&b, ln);
            avp_buf_putc(&b, '\n');
            continue;
        }
        avp_buf_put(&b, ln);
        if (!ln.n || ln.p[ln.n - 1] != '\n') avp_buf_put(&b, AVP_S("\n\\ No newline at end of file\n"));
    }
    avp_vec_free(&A);
    avp_vec_free(&B);
    avp_vec_free(&lines);
    char *z = avp_buf_cz(&b);
    return (LvS){z, b.n};
}

static bool parse_digits(LvS s, size_t *i, i64 *v) {
    size_t k = *i;
    i64 x = 0;
    while (k < s.n && s.p[k] >= '0' && s.p[k] <= '9') x = x * 10 + (s.p[k++] - '0');
    if (k == *i) return false;
    *v = x;
    *i = k;
    return true;
}

/* _HUNK.match(line): the old start and whether the old count was the text "0" */
static bool hunk_header(LvS ln, i64 *start, bool *zero) {
    size_t i = 0;
    i64 v, w;
    if (ln.n < 4 || memcmp(ln.p, "@@ -", 4)) return false;
    i = 4;
    if (!parse_digits(ln, &i, &v)) return false;
    *zero = false;
    if (i < ln.n && ln.p[i] == ',') {
        size_t j = i + 1;
        if (parse_digits(ln, &j, &w)) {
            *zero = j - (i + 1) == 1 && ln.p[i + 1] == '0';
            i = j;
        }
    }
    if (i + 2 > ln.n || memcmp(ln.p + i, " +", 2)) return false;
    i += 2;
    if (!parse_digits(ln, &i, &w)) return false;
    if (i < ln.n && ln.p[i] == ',') {
        size_t j = i + 1;
        if (parse_digits(ln, &j, &w)) i = j;
    }
    if (i + 3 > ln.n || memcmp(ln.p + i, " @@", 3)) return false;
    *start = v;
    return true;
}

bool lv_apply_patch(AvpArena *a, LvS text, LvS diff, LvS *out, const char **err) {
    AvpStrVec old = lv_splitlines_keep(text), lines = lv_splitlines_keep(diff);
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t pos = 0, i = 0;
    bool ok = true;
    while (i < lines.n && !avp_str_startswith(lines.v[i], AVP_S("@@"))) i++;
    while (ok && i < lines.n) {
        i64 st;
        bool zero;
        if (!hunk_header(lines.v[i], &st, &zero)) {
            *err = avp_sprintf(a, "bad hunk header: %s", lv_utf8(a, avp_str(lv_repr(a, lines.v[i]))));
            ok = false;
            break;
        }
        i64 start = st - (zero ? 0 : 1);
        if (start < (i64)pos) {
            *err = "overlapping hunks";
            ok = false;
            break;
        }
        for (size_t k = pos; k < (size_t)start && k < old.n; k++) avp_buf_put(&b, old.v[k]);
        pos = (size_t)start;
        i++;
        while (i < lines.n && !avp_str_startswith(lines.v[i], AVP_S("@@"))) {
            AvpStr ln = lines.v[i];
            AvpStr body = ln.n ? (AvpStr){ln.p + 1, ln.n - 1} : ln;
            if (i + 1 < lines.n && avp_str_startswith(lines.v[i + 1], AVP_S("\\"))) {
                if (body.n) body.n--;
                i++;
            }
            char c0 = ln.n ? ln.p[0] : 0;
            if (c0 == ' ' || c0 == '-') {
                if (pos >= old.n || !avp_str_eq(old.v[pos], body)) {
                    *err = avp_sprintf(a, "hunk does not match at line %zu", pos + 1);
                    ok = false;
                    break;
                }
                pos++;
                if (c0 == ' ') avp_buf_put(&b, body);
            } else if (c0 == '+')
                avp_buf_put(&b, body);
            else if (c0 != '\\') {
                *err = avp_sprintf(a, "bad patch line: %s", lv_utf8(a, avp_str(lv_repr(a, ln))));
                ok = false;
                break;
            }
            i++;
        }
    }
    if (ok) {
        for (size_t k = pos; k < old.n; k++) avp_buf_put(&b, old.v[k]);
        char *z = avp_buf_cz(&b);
        *out = (LvS){z, b.n};
    }
    avp_vec_free(&old);
    avp_vec_free(&lines);
    return ok;
}

const char *lv_safe_name(AvpArena *a, const char *name) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t n = strlen(name);
    for (size_t i = 0; i < n;) {
        u8 c = (u8)name[i];
        size_t l = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        bool keep = l == 1 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                               c == '_' || c == '.' || c == '$' || c == '-');
        avp_buf_putc(&b, keep ? (char)c : '_');
        i += l;
    }
    char *s = avp_buf_cz(&b);
    if (strcmp(s, name) || b.n > 80) {
        char *h = avp_sha1_hex(a, name, n);
        s = avp_sprintf(a, "%.70s_%.8s", s, h);
    }
    return s;
}

const char *lv_steps_text(AvpArena *a, const LvStep *path, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_put(&b, AVP_S(" + "));
        avp_buf_put(&b, avp_str(avp_sprintf(a, "%s L%lld", path[i].lever, (long long)path[i].line)));
    }
    return avp_buf_cz(&b);
}

const char *lv_stats_line(AvpArena *a, const AvpMap *stats) {
    static const char *const keys[] = {"generated", "duplicate", "fake", "failed to compile", "neutral", "worse",
                                       "improved", "breaks", "skipped (budget)", "unparsable state"};
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
    for (int k = 0; k < 10; k++) {
        AvpVal *v = avp_map_get(stats, avp_str(keys[k]));
        i64 n = v ? v->i : 0;
        if (!n && k >= 8) continue;
        const char *name = !strcmp(keys[k], "fake") ? "filtered as fake"
                           : !strcmp(keys[k], "breaks") ? "break another function"
                                                        : keys[k];
        if (!first) avp_buf_put(&b, AVP_S(", "));
        first = false;
        avp_buf_put(&b, avp_str(avp_sprintf(a, "%lld %s", (long long)n, name)));
    }
    return avp_buf_cz(&b);
}

AvpJson *lv_steps_json(AvpArena *a, const LvStep *path, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("family"), avp_json_strz(a, path[i].family));
        avp_json_set(a, o, AVP_S("lever"), avp_json_strz(a, path[i].lever));
        avp_json_set(a, o, AVP_S("line"), avp_json_int(a, path[i].line));
        avp_json_set(a, o, AVP_S("desc"), avp_json_strz(a, path[i].desc));
        avp_json_arr_push(a, arr, o);
    }
    return arr;
}

static AvpJson *jstr_or_null(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

static bool is_diff_name(const char *f) {
    return strlen(f) == 8 && f[0] >= '0' && f[0] <= '9' && f[1] >= '0' && f[1] <= '9' && f[2] >= '0' &&
           f[2] <= '9' && !strcmp(f + 3, ".diff");
}

AvpJson *lv_write_outputs(AvpArena *a, const char *odir, LvS text0, const char *relpath, const char *srcpath,
                          const char *symbol, const char *unit_name, const LvSearch *res) {
    if (!avp_makedirs(odir, true)) shsm_raise_io();
    AvpStrVec names = {0};
    if (!avp_listdir(a, odir, &names)) shsm_raise_io();
    for (size_t i = 0; i < names.n; i++) {
        const char *f = avp_cz(a, names.v[i]);
        if (is_diff_name(f) && !avp_remove(avp_path_join(a, odir, f))) shsm_raise_io();
    }
    LvResults imp, brk;
    lv_ranked(a, &res->results, &imp, &brk);
    const LvScore *base = res->base;
    AvpJson *entries = avp_json_arr(a);
    for (size_t i = 0; i < imp.n; i++) {
        const LvResult *r = &imp.v[i];
        const char *name = avp_sprintf(a, "%03zu.diff", i + 1);
        LvS d = lv_make_diff(a, text0, r->st.text, relpath);
        if (!avp_write_file(avp_path_join(a, odir, name), d.p, d.n)) shsm_raise_io();
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("rank"), avp_json_int(a, (i64)i + 1));
        avp_json_set(a, e, AVP_S("diff"), avp_json_strz(a, name));
        avp_json_set(a, e, AVP_S("rows"), avp_json_int(a, r->sc->rows));
        avp_json_set(a, e, AVP_S("target_mismatches"), avp_json_int(a, r->sc->tm));
        avp_json_set(a, e, AVP_S("exact"), avp_json_bool(r->sc->exact));
        avp_json_set(a, e, AVP_S("delta"), avp_json_int(a, (r->sc->rows + r->sc->tm) - (base->rows + base->tm)));
        avp_json_set(a, e, AVP_S("steps"), lv_steps_json(a, r->st.path, r->st.npath));
        avp_json_arr_push(a, entries, e);
    }
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("unit"), avp_json_strz(a, unit_name));
    avp_json_set(a, doc, AVP_S("function"), avp_json_strz(a, symbol));
    avp_json_set(a, doc, AVP_S("source"), avp_json_strz(a, relpath));
    avp_json_set(a, doc, AVP_S("source_path"), avp_json_strz(a, srcpath));
    avp_json_set(a, doc, AVP_S("source_sha1"), avp_json_strz(a, lv_sha(a, text0)));
    avp_json_set(a, doc, AVP_S("rule"), avp_json_strz(a, LV_RULE));
    AvpJson *bj = avp_json_obj(a);
    avp_json_set(a, bj, AVP_S("rows"), avp_json_int(a, base->rows));
    avp_json_set(a, bj, AVP_S("target_mismatches"), avp_json_int(a, base->tm));
    avp_json_set(a, bj, AVP_S("exact"), avp_json_bool(base->exact));
    avp_json_set(a, bj, AVP_S("retail_rows"), avp_json_int(a, base->nrows));
    avp_json_set(a, doc, AVP_S("base"), bj);
    avp_json_set(a, doc, AVP_S("improving"), entries);
    AvpJson *bl = avp_json_arr(a);
    for (size_t i = 0; i < brk.n; i++) {
        const LvResult *r = &brk.v[i];
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("rows"), avp_json_int(a, r->sc->rows));
        avp_json_set(a, e, AVP_S("target_mismatches"), avp_json_int(a, r->sc->tm));
        avp_json_set(a, e, AVP_S("exact"), avp_json_bool(r->sc->exact));
        AvpJson *bn = avp_json_arr(a);
        for (size_t k = 0; k < r->broken.n; k++) avp_json_arr_push(a, bn, avp_json_strz(a, r->broken.v[k]));
        avp_json_set(a, e, AVP_S("breaks"), bn);
        avp_json_set(a, e, AVP_S("steps"), lv_steps_json(a, r->st.path, r->st.npath));
        avp_json_arr_push(a, bl, e);
    }
    avp_json_set(a, doc, AVP_S("breaks"), bl);
    AvpJson *fl = avp_json_arr(a);
    size_t nf = 0;
    for (size_t i = 0; i < res->results.n && nf < 20; i++) {
        const LvResult *r = &res->results.v[i];
        if (strcmp(r->status, "error")) continue;
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("error"), jstr_or_null(a, r->sc->error));
        avp_json_set(a, e, AVP_S("steps"), lv_steps_json(a, r->st.path, r->st.npath));
        avp_json_arr_push(a, fl, e);
        nf++;
    }
    avp_json_set(a, doc, AVP_S("failed"), fl);
    AvpJson *sj = avp_json_obj(a);
    AvpMapIter it = {0};
    while (avp_map_next(res->stats, &it)) avp_json_set(a, sj, it.key, avp_json_int(a, it.val->i));
    avp_json_set(a, doc, AVP_S("stats"), sj);
    avp_json_set(a, doc, AVP_S("seconds"), avp_json_float(a, avp_py_round(res->seconds, 1)));
    avp_json_set(a, doc, AVP_S("levels"), avp_json_int(a, res->levels));
    AvpJsonDumpOpts jo = AVP_JSON_INDENT(1);
    AvpStr text = avp_json_dumps(a, doc, &jo);
    /* json.dump into a text-mode file: newlines written as os.linesep */
    if (!avp_write_text(avp_path_join(a, odir, "report.json"), text, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT)))
        shsm_raise_io();
    return doc;
}
