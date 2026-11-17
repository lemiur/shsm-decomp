/* solver_score: scratch copies, compiles with a changed flag set, the comparison with retail, levers' Score of a
 * function, and the controls 1 to 3 (the solver checks its own measurements). Everything is written under
 * build/tmp/solver/<unit path>/. */
#include "solver.h"
#include "shsm_text.h"
#include "wall.h"

void sv_scorer_init(AvpArena *a, SvScorer *s, const ShsmRepo *r, const ShsmUnit *u) {
    memset(s, 0, sizeof *s);
    s->r = r;
    s->u = u;
    s->basename = u->source ? avp_cz(a, avp_path_basename(u->source)) : "source.c";
    s->workdir = avp_path_join(a, avp_join(a, r->root, "build", "tmp", "solver"), shsm_unit_path(a, u->name));
}

AvpStrVec sv_flag_words(AvpArena *a, const char *flag) {
    AvpStrVec w = {0};
    const char *err = NULL;
    if (!shsm_shlex_split(a, avp_str(flag), &w, &err)) w = avp_str_split(avp_str(flag), AVP_S(""), -1);
    return w;
}

/* Removes the last consecutive occurrence of each group's words from argv. */
static void drop_groups(AvpStrVec *argv, const AvpStrVec *groups, size_t ngroups) {
    for (size_t g = 0; g < ngroups; g++) {
        const AvpStrVec *w = &groups[g];
        if (!w->n || w->n > argv->n) continue;
        for (size_t i = argv->n - w->n + 1; i-- > 0;) {
            bool eq = true;
            for (size_t k = 0; k < w->n && eq; k++) eq = avp_str_eq(argv->v[i + k], w->v[k]);
            if (!eq) continue;
            memmove(&argv->v[i], &argv->v[i + w->n], (argv->n - i - w->n) * sizeof(AvpStr));
            argv->n -= w->n;
            break;
        }
    }
}

static SvBuild *compile_at(SvScorer *s, AvpArena *a, const char *src, const char *dir, const SvFlags *fl) {
    SvBuild *b = AVP_NEW(a, SvBuild);
    b->dir = dir;
    b->src = src;
    b->obj = shsm_object_for(a, src, dir);
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmCompileOpts o = {0};
        o.sjis = -1;
        AvpPtrVec extra = {0};
        if (fl)
            for (size_t i = 0; i < fl->nextra; i++) avp_vec_push(&extra, (void *)fl->extra[i]);
        if (fl && fl->preprocess) avp_vec_push(&extra, (void *)"-EP");
        o.extra_flags = (const char *const *)extra.v;
        o.nextra = extra.n;
        if (fl) {
            o.drop_flags = fl->drop;
            o.ndrop = fl->ndrop;
        }
        AvpStrVec argv = shsm_compile_argv(a, s->r, s->u, src, b->obj, &o);
        avp_vec_free(&extra);
        if (fl && fl->ngroups) drop_groups(&argv, fl->groups, fl->ngroups);
        const char **av = AVP_NEWN(a, const char *, argv.n + 1);
        for (size_t i = 0; i < argv.n; i++) av[i] = avp_cz(a, argv.v[i]);
        avp_vec_free(&argv);
        bool pp = fl && fl->preprocess;
        AvpProcResult pr = avp_run(&(AvpProcSpec){.argv = av, .cwd = s->r->root, .out = AVP_IO_CAPTURE,
                                                  .err = AVP_IO_CAPTURE, .text = !pp, .errors = AVP_ERRORS_REPLACE,
                                                  .timeout = 300, .creation_flags = AVP_BELOW_NORMAL_PRIORITY_CLASS,
                                                  .arena = a});
        if (pr.status == AVP_PROC_START_FAILED)
            shsm_raise(SHSM_EXC_USAGE, "cannot run %s: %s", av[0], pr.message ? pr.message : "cannot start");
        if (pr.status != AVP_PROC_OK || pr.returncode != 0) {
            AvpStr output = avp_str_concat(a, pr.out, pr.err);
            AvpStr why = wall_first_error(a, output);
            if (!why.p || !why.n) {
                AvpStrVec ls = avp_str_splitlines(output, false);
                why = AVP_S("compile failed");
                for (size_t i = 0; i < ls.n; i++)
                    if (avp_str_strip(ls.v[i]).n) {
                        why = ls.v[i];
                        break;
                    }
                avp_vec_free(&ls);
            }
            b->error = avp_sprintf(a, "compile failed: %.*s", AVP_SF(why));
        } else if (pp) {
            /* MWCC writes -EP output to the -o file when -o names a file, else to stdout */
            AvpBytes pb = {0};
            if (avp_exists(b->obj) && avp_read_all(a, b->obj, &pb) && pb.n)
                b->preprocessed = avp_strn((const char *)pb.v, pb.n);
            else
                b->preprocessed = pr.out;
            b->ok = true;
        } else {
            CcRepo cr = {s->r->root, s->r->version};
            CcError cerr = {0};
            b->oc = cc_compare_object(a, &cr, avp_str(s->u->name), b->obj, NULL, true, NULL, NULL, NULL, dir, NULL,
                                      &cerr);
            if (!b->oc)
                b->error = avp_sprintf(a, "comparison failed: %.*s", AVP_SF(cerr.msg));
            else
                b->ok = true;
        }
        avp_try_end(&t);
    } else {
        b->ok = false;
        b->error = avp_strdup(a, t.msg ? t.msg : "error");
    }
    return b;
}

static const char *tag_dir(SvScorer *s, AvpArena *a, const char *tag) {
    if (!tag) tag = avp_sprintf(a, "c%05lld", (long long)(avp_atomic_add64(&s->counter, 1) - 1));
    return avp_path_join(a, s->workdir, tag);
}

SvBuild *sv_build(SvScorer *s, AvpArena *a, AvpStr text, const char *tag, const SvFlags *fl) {
    const char *dir = tag_dir(s, a, tag);
    if (avp_isdir(dir)) avp_rmtree(dir, 1);
    if (!avp_makedirs(dir, true) || !avp_write_file(avp_path_join(a, dir, s->basename), text.p, text.n)) {
        SvBuild *b = AVP_NEW(a, SvBuild);
        b->dir = dir;
        b->error = avp_sprintf(a, "cannot write the scratch copy in %s", dir);
        return b;
    }
    return compile_at(s, a, avp_path_join(a, dir, s->basename), dir, fl);
}

SvBuild *sv_build_file(SvScorer *s, AvpArena *a, const char *src, const char *tag, const SvFlags *fl) {
    const char *dir = tag_dir(s, a, tag);
    if (avp_isdir(dir)) avp_rmtree(dir, 1);
    if (!avp_makedirs(dir, true)) {
        SvBuild *b = AVP_NEW(a, SvBuild);
        b->dir = dir;
        b->error = avp_sprintf(a, "cannot make %s", dir);
        return b;
    }
    return compile_at(s, a, src, dir, fl);
}

const CcCompareResult *sv_result(const SvBuild *b, const char *symbol) {
    if (!b || !b->oc) return NULL;
    AvpVal *v = avp_map_get(b->oc->results, avp_str(symbol));
    if (v) return v->p;
    AvpMapIter it = {0};
    while (avp_map_next(b->oc->results, &it)) {
        const CcCompareResult *c = it.val->p;
        if (avp_str_eqz(c->retail_name, symbol)) return c;
    }
    return NULL;
}

static i64 differing_rows(const CcCompareResult *c) {
    i64 rows = 0;
    for (size_t i = 0; i < c->npairs; i++)
        if (cc_pair_differs(&c->pairs[i])) rows++;
    return rows;
}

static const char *signature(AvpArena *a, const CcCompareResult *c) {
    AvpBuf sig;
    avp_buf_init(&sig, a);
    bool first = true;
    for (size_t i = 0; i < c->npairs; i++) {
        const CcPair *p = &c->pairs[i];
        if (!p->compiled) continue;
        if (!first) avp_buf_putc(&sig, '\n');
        avp_buf_put(&sig, p->compiled->norm);
        first = false;
    }
    return avp_sha1_hex(a, sig.p ? sig.p : "", sig.n);
}

LvScore *sv_lvscore(AvpArena *a, const SvBuild *b, const char *symbol) {
    LvScore *out = AVP_NEW(a, LvScore);
    if (!b || !b->ok || !b->oc) {
        out->error = b && b->error ? b->error : "no build";
        return out;
    }
    const CcCompareResult *tgt = sv_result(b, symbol);
    if (!tgt) {
        out->error = "function not in the compiled object";
        return out;
    }
    out->ok = true;
    out->rows = differing_rows(tgt);
    out->sig = signature(a, tgt);
    out->tm = (i64)tgt->target_mismatches->n;
    out->exact = tgt->exact;
    out->nrows = (i64)tgt->counts.retail_rows;
    out->others = AVP_NEWN(a, LvOther, avp_map_len(b->oc->results) + 1);
    AvpMapIter it = {0};
    while (avp_map_next(b->oc->results, &it)) {
        const CcCompareResult *c = it.val->p;
        if (c == tgt) continue;
        out->others[out->nothers++] =
            (LvOther){avp_cz(a, it.key), differing_rows(c), (i64)c->target_mismatches->n, c->exact};
    }
    return out;
}

const char *sv_score_variant(SvScorer *s, AvpArena *a, const SvBuild *base, AvpStr text, const char *symbol,
                             const SvFlags *fl, i64 *rows_before, i64 *rows_after, bool *exact, AvpCzVec *broken) {
    LvScore *b0 = sv_lvscore(a, base, symbol);
    SvBuild *v = sv_build(s, a, text, NULL, fl);
    LvScore *b1 = sv_lvscore(a, v, symbol);
    *rows_before = b0->ok ? b0->rows : -1;
    *rows_after = b1->ok ? b1->rows : -1;
    *exact = b1->ok && b1->exact;
    if (!b0->ok) {
        *broken = (AvpCzVec){0};
        return "error";
    }
    return lv_classify(a, b0, b1, broken);
}

bool sv_objects_equal(const char *x, const char *y) {
    AvpArena *a = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(a);
    AvpBytes p = {0}, q = {0};
    bool eq = avp_read_all(a, x, &p) && avp_read_all(a, y, &q) && p.n == q.n && memcmp(p.v, q.v, p.n) == 0;
    avp_arena_reset(a, mk);
    return eq;
}

/* ---- controls -------------------------------------------------------------------------------------------------- */

AvpStr sv_with_control_fn(AvpArena *a, AvpStr text) {
    return avp_str_concat(a, text, AVP_S("\n\nint " SV_CONTROL_FN "(int x) { return x * 7 + 3; }\n"));
}

bool sv_build_has_control_fn(const SvBuild *b) {
    if (!b || !b->oc || !b->oc->compiled) return false;
    AvpMapIter it = {0};
    while (avp_map_next(b->oc->compiled, &it))
        if (avp_str_startswith(it.key, AVP_S(SV_CONTROL_FN))) return true;
    return false;
}

bool sv_control_pipeline(SvScorer *s, AvpArena *a, AvpStr text, const char **why) {
    SvBuild *b = sv_build(s, a, sv_with_control_fn(a, text), "pipeline", NULL);
    *why = NULL;
    if (!b->ok) {
        *why = b->error;
        return false;
    }
    if (!sv_build_has_control_fn(b)) {
        *why = SV_CONTROL_FN " not in the object compiled from the scratch copy";
        return false;
    }
    return true;
}

bool sv_control_baseline(AvpArena *a, const SvBuild *ref, const SvBuild *base, const char *symbol, const char **why) {
    *why = NULL;
    if (!ref || !ref->ok || !base || !base->ok) {
        *why = "a build failed";
        return false;
    }
    const CcCompareResult *x = sv_result(ref, symbol), *y = sv_result(base, symbol);
    if (!x && !y) return true;
    if (!x || !y) {
        *why = avp_sprintf(a, "%s is emitted by one build only", symbol);
        return false;
    }
    i64 rx = differing_rows(x), ry = differing_rows(y);
    const char *sx = signature(a, x), *sy = signature(a, y);
    if (rx != ry || strcmp(sx, sy) != 0) {
        *why = avp_sprintf(a, "the scratch copy gives %lld differing rows (signature %.10s), the stop %lld (%.10s)",
                           (long long)ry, sy, (long long)rx, sx);
        return false;
    }
    return true;
}

static const char FLAG_CONTROL_SRC[] =
    "static int solver_flag_callee(int x) { return x * 3 + 1; }\n"
    "int solver_flag_caller(int x) { return solver_flag_callee(x) + solver_flag_callee(x + 2); }\n";

/* The compiled functions of the control source as "name\nnorm\nnorm..." blocks, NULL on failure. */
static const char *control_listing(SvScorer *s, AvpArena *a, const char *tag, const SvFlags *fl, const char **why) {
    AvpStr ext = {0}, root = {0};
    avp_path_splitext(s->basename, &root, &ext);
    const char *dir = avp_path_join(a, s->workdir, tag);
    if (avp_isdir(dir)) avp_rmtree(dir, 1);
    const char *src = avp_path_join(a, dir, avp_sprintf(a, "solver_flag_control%.*s", AVP_SF(ext)));
    if (!avp_makedirs(dir, true) || !avp_write_file(src, FLAG_CONTROL_SRC, sizeof FLAG_CONTROL_SRC - 1)) {
        *why = "cannot write the control source";
        return NULL;
    }
    /* compiled and listed without a comparison: the control functions have no retail counterpart */
    AvpTry t;
    const char *listing = NULL;
    if (AVP_TRY(&t)) {
        ShsmCompileOpts o = {0};
        o.sjis = -1;
        if (fl) {
            o.extra_flags = fl->extra;
            o.nextra = fl->nextra;
            o.drop_flags = fl->drop;
            o.ndrop = fl->ndrop;
        }
        const char *obj = shsm_object_for(a, src, dir);
        AvpStrVec argv = shsm_compile_argv(a, s->r, s->u, src, obj, &o);
        if (fl && fl->ngroups) drop_groups(&argv, fl->groups, fl->ngroups);
        const char **av = AVP_NEWN(a, const char *, argv.n + 1);
        for (size_t i = 0; i < argv.n; i++) av[i] = avp_cz(a, argv.v[i]);
        avp_vec_free(&argv);
        AvpProcResult pr = avp_run(&(AvpProcSpec){.argv = av, .cwd = s->r->root, .out = AVP_IO_CAPTURE,
                                                  .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                                  .timeout = 300, .arena = a});
        if (pr.status != AVP_PROC_OK || pr.returncode != 0) {
            *why = avp_sprintf(a, "control compile failed: %.*s", AVP_SF(wall_first_error(a, avp_str_concat(a, pr.out, pr.err))));
        } else {
            CcRepo cr = {s->r->root, s->r->version};
            CcError cerr = {0};
            AvpMap *fns = cc_load_compiled(a, &cr, obj, dir, NULL, NULL, &cerr);
            if (!fns) {
                *why = avp_sprintf(a, "control listing failed: %.*s", AVP_SF(cerr.msg));
            } else {
                AvpBuf b;
                avp_buf_init(&b, a);
                AvpMapIter it = {0};
                while (avp_map_next(fns, &it)) {
                    const CcFn *fn = it.val->p;
                    avp_buf_put(&b, it.key);
                    for (size_t i = 0; i < fn->nrows; i++) {
                        avp_buf_putc(&b, '\n');
                        avp_buf_put(&b, fn->rows[i].norm);
                    }
                    avp_buf_putc(&b, '\n');
                }
                listing = b.p ? b.p : "";
            }
        }
        avp_try_end(&t);
    } else {
        *why = avp_strdup(a, t.msg ? t.msg : "error");
    }
    return listing;
}

int sv_control_flag(SvScorer *s, AvpArena *a, const SvFlags *without, const SvFlags *with, const char **why) {
    *why = NULL;
    const char *x = control_listing(s, a, "flagctl-without", without, why);
    if (!x) return -1;
    const char *y = control_listing(s, a, "flagctl-with", with, why);
    if (!y) return -1;
    return strcmp(x, y) != 0 ? 1 : 0;
}

int sv_compile_snippet(SvScorer *s, AvpArena *a, const char *tag, const char *ext, AvpStr text, const char **why) {
    *why = NULL;
    const char *dir = avp_path_join(a, s->workdir, tag);
    if (avp_isdir(dir)) avp_rmtree(dir, 1);
    const char *src = avp_path_join(a, dir, avp_sprintf(a, "solver_snippet%s", ext));
    if (!avp_makedirs(dir, true) || !avp_write_file(src, text.p, text.n)) {
        *why = "cannot write the snippet";
        return -1;
    }
    int ok = -1;
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmCompileOpts o = {0};
        o.sjis = -1;
        const char *obj = shsm_object_for(a, src, dir);
        AvpStrVec argv = shsm_compile_argv(a, s->r, s->u, src, obj, &o);
        const char **av = AVP_NEWN(a, const char *, argv.n + 1);
        for (size_t i = 0; i < argv.n; i++) av[i] = avp_cz(a, argv.v[i]);
        size_t n = argv.n;
        avp_vec_free(&argv);
        AvpProcResult pr = avp_run(&(AvpProcSpec){.argv = av, .cwd = s->r->root, .out = AVP_IO_CAPTURE,
                                                  .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                                  .timeout = 300, .arena = a});
        AVP_UNUSED(n);
        if (pr.status != AVP_PROC_OK || pr.returncode != 0) {
            AvpStr e = wall_first_error(a, avp_str_concat(a, pr.out, pr.err));
            *why = avp_sprintf(a, "%.*s", AVP_SF(e.p ? e : AVP_S("compile failed")));
            ok = 0;
        } else {
            ok = 1;
        }
        avp_try_end(&t);
    } else {
        *why = avp_strdup(a, t.msg ? t.msg : "error");
    }
    return ok;
}
