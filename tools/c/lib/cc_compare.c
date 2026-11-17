/* tools/common/compare.py: the facade over cc_codecmp (see cc_compare.h). */
#include "cc_compare.h"

#include <stdlib.h>

static AvpStr S(const char *cz) { return avp_str(cz); }

static void set_err(CcError *err, CcErrKind kind, AvpStr msg) {
    if (err) {
        err->kind = kind;
        err->msg = msg;
    }
}

/* ---- repo paths ------------------------------------------------------------------------------------------------- */

AvpStr cc_unit_path(AvpArena *a, AvpStr unit) {
    AvpStr u = avp_str_replace(a, unit, AVP_S("\\"), AVP_S("/"), -1);
    if (avp_str_startswith(u, AVP_S("main/"))) u = avp_strn(u.p + 5, u.n - 5);
    AvpStr base, ext;
    char *uz = avp_cz(a, u);
    avp_path_splitext(uz, &base, &ext);
    static const char *const exts[] = {".c", ".cpp", ".cp", ".s", ".o", ".C", ".cc"};
    for (size_t k = 0; k < AVP_ARRAY_LEN(exts); k++)
        if (avp_str_eqz(ext, exts[k])) return avp_str(avp_cz(a, base));
    return avp_str(uz);
}

static char *joinv(AvpArena *a, const char *root, const char *const *parts, size_t n) {
    char *p = avp_strdup(a, root);
    for (size_t k = 0; k < n; k++) p = avp_path_join(a, p, parts[k]);
    return p;
}

char *cc_repo_asm_path(AvpArena *a, const CcRepo *r, AvpStr unit) {
    const char *parts[] = {"build", r->version, "asm"};
    char *p = joinv(a, r->root, parts, 3);
    AvpStrVec segs = avp_str_split(cc_unit_path(a, unit), AVP_S("/"), -1);
    for (size_t k = 0; k < segs.n; k++) p = avp_path_join(a, p, avp_cz(a, segs.v[k]));
    avp_vec_free(&segs);
    return avp_sprintf(a, "%s.s", p);
}

char *cc_repo_src_obj_dir(AvpArena *a, const CcRepo *r) {
    const char *parts[] = {"build", r->version, "src"};
    return joinv(a, r->root, parts, 3);
}

char *cc_repo_dtk(AvpArena *a, const CcRepo *r) {
    const char *parts[] = {"tools", "bin", "dtk.exe"};
    return joinv(a, r->root, parts, 3);
}

/* ---- pairs ------------------------------------------------------------------------------------------------------ */

const char *cc_pair_status_name(CcPairStatus s) {
    switch (s) {
    case CC_EQUAL: return "equal";
    case CC_REPLACE: return "replace";
    case CC_DELETE: return "delete";
    default: return "insert";
    }
}

bool cc_pair_differs(const CcPair *p) {
    return p->status != CC_EQUAL || (p->target && strcmp(p->target, "mismatch") == 0);
}

static const CcSide *side(AvpArena *a, size_t index, const CcRow *row) {
    CcSide *s = AVP_NEW(a, CcSide);
    s->index = index;
    s->address = row->addr;
    s->text = row->text;
    s->norm = row->norm;
    return s;
}

CcPair *cc_align(AvpArena *a, const CcFn *rfn, const CcFn *cfn, size_t *npairs) {
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpStr *want = AVP_NEWN(avp_scratch(), AvpStr, rfn->nrows + 1), *got = AVP_NEWN(avp_scratch(), AvpStr, cfn->nrows + 1);
    cc_fn_norms(rfn, want);
    cc_fn_norms(cfn, got);
    AvpSeqMatcher *m = avp_seqm_new_str(avp_scratch(), want, rfn->nrows, got, cfn->nrows, &avp_seqm_no_autojunk);
    size_t n;
    const AvpOpcode *ops = avp_seqm_opcodes(m, &n);
    size_t total = 0;
    for (size_t k = 0; k < n; k++) total += AVP_MAX(ops[k].i2 - ops[k].i1, ops[k].j2 - ops[k].j1);
    CcPair *out = AVP_NEWN(a, CcPair, total ? total : 1);
    size_t q = 0;
    for (size_t k = 0; k < n; k++) {
        const AvpOpcode *o = &ops[k];
        if (o->tag == AVP_OP_EQUAL) {
            for (size_t d = 0; d < o->i2 - o->i1; d++) {
                CcPair *p = &out[q++];
                p->status = CC_EQUAL;
                p->retail = side(a, o->i1 + d, &rfn->rows[o->i1 + d]);
                p->compiled = side(a, o->j1 + d, &cfn->rows[o->j1 + d]);
            }
            continue;
        }
        size_t span = AVP_MAX(o->i2 - o->i1, o->j2 - o->j1);
        for (size_t d = 0; d < span; d++) {
            CcPair *p = &out[q++];
            p->retail = o->i1 + d < o->i2 ? side(a, o->i1 + d, &rfn->rows[o->i1 + d]) : NULL;
            p->compiled = o->j1 + d < o->j2 ? side(a, o->j1 + d, &cfn->rows[o->j1 + d]) : NULL;
            p->status = p->retail && p->compiled ? CC_REPLACE : p->retail ? CC_DELETE : CC_INSERT;
        }
    }
    avp_arena_reset(avp_scratch(), mk);
    *npairs = q;
    return out;
}

CcCompareResult *cc_compare_result_new(AvpArena *a, AvpStr name, AvpStr retail_name, const CcFn *rfn, const CcFn *cfn,
                                       const CcFnResult *res) {
    CcCompareResult *r = AVP_NEW(a, CcCompareResult);
    r->name = name;
    r->retail_name = retail_name;
    r->retail_fn = rfn;
    r->compiled_fn = cfn;
    r->raw = res;
    r->exact = cc_fn_result_exact(res);
    r->edit = res->edit;
    r->positional = res->positional;
    r->npositional = res->npositional;
    r->checked_targets = res->checked_targets;
    r->target_mismatches = &res->targets;
    r->unresolved = &res->unresolved;
    r->pairs = cc_align(a, rfn, cfn, &r->npairs);
    /* by_i: retail index -> equal pair */
    CcPair **by_i = AVP_NEWN(a, CcPair *, rfn->nrows + 1);
    for (size_t k = 0; k < r->npairs; k++)
        if (r->pairs[k].retail && r->pairs[k].status == CC_EQUAL) by_i[r->pairs[k].retail->index] = &r->pairs[k];
    const CcTdList *lists[2] = {&res->targets, &res->unresolved};
    const char *kinds[2] = {"mismatch", "unresolved"};
    for (int l = 0; l < 2; l++)
        for (size_t k = 0; k < lists[l]->n; k++) {
            const CcTargetDiff *t = &lists[l]->v[k];
            CcPair *p = t->i < rfn->nrows ? by_i[t->i] : NULL;
            if (p && !p->target) {
                p->target = kinds[l];
                p->detail = t->detail;
            }
        }
    CcCounts *c = &r->counts;
    for (size_t k = 0; k < r->npairs; k++) {
        switch (r->pairs[k].status) {
        case CC_EQUAL: c->equal++; break;
        case CC_REPLACE: c->replace++; break;
        case CC_DELETE: c->delete_++; break;
        default: c->insert++; break;
        }
    }
    c->retail_rows = rfn->nrows;
    c->compiled_rows = cfn->nrows;
    c->target_mismatches = res->targets.n;
    c->unresolved = res->unresolved.n;
    return r;
}

static AvpJson *jstr(AvpArena *a, AvpStr s) { return avp_json_str_ref(a, s); }
static AvpJson *jint(AvpArena *a, i64 v) { return avp_json_int(a, v); }

static AvpJson *td_list_json(AvpArena *a, const CcTdList *l) {
    AvpJson *arr = avp_json_arr_cap(a, l->n);
    for (size_t k = 0; k < l->n; k++) {
        const CcTargetDiff *t = &l->v[k];
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("i"), jint(a, (i64)t->i));
        avp_json_set(a, o, AVP_S("j"), jint(a, (i64)t->j));
        avp_json_set(a, o, AVP_S("retail"), jstr(a, t->retail));
        avp_json_set(a, o, AVP_S("compiled"), jstr(a, t->compiled));
        avp_json_set(a, o, AVP_S("detail"), jstr(a, t->detail));
        avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, t->kind));
        avp_json_arr_push(a, arr, o);
    }
    return arr;
}

static AvpJson *side_json(AvpArena *a, const CcSide *s) {
    if (!s) return avp_json_null();
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("index"), jint(a, (i64)s->index));
    avp_json_set(a, o, AVP_S("address"), jint(a, s->address));
    avp_json_set(a, o, AVP_S("text"), jstr(a, s->text));
    avp_json_set(a, o, AVP_S("norm"), jstr(a, s->norm));
    return o;
}

AvpJson *cc_compare_result_to_json(AvpArena *a, const CcCompareResult *r, bool pairs) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("name"), jstr(a, r->name));
    avp_json_set(a, d, AVP_S("retail_name"), jstr(a, r->retail_name));
    avp_json_set(a, d, AVP_S("exact"), avp_json_bool(r->exact));
    avp_json_set(a, d, AVP_S("edit"), jint(a, (i64)r->edit));
    AvpJson *c = avp_json_obj(a);
    const CcCounts *k = &r->counts;
    avp_json_set(a, c, AVP_S("equal"), jint(a, (i64)k->equal));
    avp_json_set(a, c, AVP_S("replace"), jint(a, (i64)k->replace));
    avp_json_set(a, c, AVP_S("delete"), jint(a, (i64)k->delete_));
    avp_json_set(a, c, AVP_S("insert"), jint(a, (i64)k->insert));
    avp_json_set(a, c, AVP_S("retail_rows"), jint(a, (i64)k->retail_rows));
    avp_json_set(a, c, AVP_S("compiled_rows"), jint(a, (i64)k->compiled_rows));
    avp_json_set(a, c, AVP_S("target_mismatches"), jint(a, (i64)k->target_mismatches));
    avp_json_set(a, c, AVP_S("unresolved"), jint(a, (i64)k->unresolved));
    avp_json_set(a, d, AVP_S("counts"), c);
    avp_json_set(a, d, AVP_S("checked_targets"), jint(a, (i64)r->checked_targets));
    avp_json_set(a, d, AVP_S("target_mismatches"), td_list_json(a, r->target_mismatches));
    avp_json_set(a, d, AVP_S("unresolved"), td_list_json(a, r->unresolved));
    if (pairs) {
        AvpJson *arr = avp_json_arr_cap(a, r->npairs);
        for (size_t q = 0; q < r->npairs; q++) {
            const CcPair *p = &r->pairs[q];
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, cc_pair_status_name(p->status)));
            avp_json_set(a, o, AVP_S("retail"), side_json(a, p->retail));
            avp_json_set(a, o, AVP_S("compiled"), side_json(a, p->compiled));
            avp_json_set(a, o, AVP_S("target"), p->target ? avp_json_strz(a, p->target) : avp_json_null());
            avp_json_set(a, o, AVP_S("detail"), p->detail.p ? jstr(a, p->detail) : avp_json_null());
            avp_json_arr_push(a, arr, o);
        }
        avp_json_set(a, d, AVP_S("pairs"), arr);
    }
    return d;
}

AvpStr cc_compare_result_render(AvpArena *a, const CcCompareResult *r, bool only_diff, int context, int width) {
    size_t n = r->npairs;
    bool *keep = AVP_NEWN(a, bool, n + 1);
    for (size_t i = 0; i < n; i++) keep[i] = !only_diff;
    if (only_diff)
        for (size_t i = 0; i < n; i++)
            if (cc_pair_differs(&r->pairs[i]) || r->pairs[i].target)
                for (i64 j = (i64)i - context; j <= (i64)i + context; j++)
                    if (j >= 0 && (size_t)j < n) keep[j] = true;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%s: %s, edit %zu, %zu/%zu rows, %zu target mismatches, %zu unresolved", r->name.p,
                   r->exact ? "exact" : "differs", r->edit, r->counts.retail_rows, r->counts.compiled_rows,
                   r->target_mismatches->n, r->unresolved->n);
    i64 last = -1;
    for (size_t i = 0; i < n; i++) {
        if (!keep[i]) continue;
        const CcPair *p = &r->pairs[i];
        if (only_diff && last >= 0 && (i64)i != last + 1) avp_buf_putz(&b, "\n  ...");
        last = (i64)i;
        const char *m = p->status == CC_REPLACE ? "!" : p->status == CC_DELETE ? "-" : p->status == CC_INSERT ? "+"
                      : p->target && strcmp(p->target, "mismatch") == 0 ? "T" : p->target ? "?" : " ";
        size_t idx = p->retail ? p->retail->index : p->compiled->index;
        AvpStr left = avp_str_ljust(a, p->retail ? p->retail->text : AVP_S(""), width > 0 ? (size_t)width : 0, ' ');
        avp_buf_printf(&b, "\n%s %3zu %.*s | %.*s", m, idx, AVP_SF(left), AVP_SF(p->compiled ? p->compiled->text : AVP_S("")));
        if (p->target && strcmp(p->target, "mismatch") == 0)
            avp_buf_printf(&b, "\n        -> %.*s", AVP_SF(p->detail.p ? p->detail : AVP_S("")));
    }
    return avp_strn(avp_buf_cz(&b), b.n);
}

/* ---- loading ---------------------------------------------------------------------------------------------------- */

bool cc_is_gap(AvpStr s) {
    /* ^gap_[0-9A-Fa-f]+_[0-9A-Fa-f]{8}_ */
    if (!avp_str_startswith(s, AVP_S("gap_"))) return false;
    size_t p = 4;
#define HX(c) (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
    while (p < s.n && HX(s.p[p])) p++;
    if (p == 4 || p >= s.n || s.p[p] != '_') return false;
    for (size_t k = p + 1; k < p + 9; k++)
        if (k >= s.n || !HX(s.p[k])) return false;
#undef HX
    return p + 9 < s.n && s.p[p + 9] == '_';
}

typedef struct {
    i64 size, mtime_ns;
    AvpMap *all, *nogap;
} RetailEntry;

static AvpMutex g_lock;
static AvpArena *g_arena;
static AvpMap *g_retail, *g_projects;

static AvpArena *g_arena_get(void) {
    if (!g_arena) {
        g_arena = avp_arena_new(0);
        g_retail = avp_map_new();
        g_projects = avp_map_new();
    }
    return g_arena;
}

AvpMap *cc_load_retail(const CcRepo *r, AvpStr unit, const char *asm_path, bool include_gaps, CcError *err) {
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    const char *path = asm_path ? asm_path : cc_repo_asm_path(avp_scratch(), r, unit);
    AvpStat st;
    if (!avp_stat(path, &st)) {
        set_err(err, CC_ERR_OS, avp_io_err_str(avp_arena()));
        avp_arena_reset(avp_scratch(), mk);
        return NULL;
    }
    AvpStr key = avp_str(avp_path_normcase(avp_scratch(), avp_path_abspath(avp_scratch(), path)));
    avp_mutex_lock(&g_lock);
    AvpArena *ga = g_arena_get();
    AvpVal *v = avp_map_get(g_retail, key);
    RetailEntry *e = v ? v->p : NULL;
    AvpMap *out = NULL;
    if (!e || e->size != st.size || e->mtime_ns != st.mtime_ns) {
        AvpStr why;
        AvpMap *allfns = cc_load_asm(ga, path, &why);
        if (!allfns) {
            avp_mutex_unlock(&g_lock);
            set_err(err, CC_ERR_OS, avp_str_dup(avp_arena(), why));
            avp_arena_reset(avp_scratch(), mk);
            return NULL;
        }
        e = AVP_NEW(ga, RetailEntry);
        e->size = st.size;
        e->mtime_ns = st.mtime_ns;
        e->all = allfns;
        e->nogap = avp_map_new();
        AvpMapIter it = {0};
        while (avp_map_next(allfns, &it))
            if (!cc_is_gap(it.key)) avp_map_put(e->nogap, it.key, NULL)->p = it.val->p;
        avp_map_put(g_retail, key, NULL)->p = e;
    }
    out = include_gaps ? e->all : e->nogap;
    avp_mutex_unlock(&g_lock);
    avp_arena_reset(avp_scratch(), mk);
    return out;
}

CcContext *cc_repo_context(const CcRepo *r, AvpStr unit) {
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpStr key = avp_str(avp_path_normcase(avp_scratch(), r->root));
    avp_mutex_lock(&g_lock);
    g_arena_get();
    bool created;
    AvpVal *v = avp_map_put(g_projects, key, &created);
    if (created) v->p = cc_project_new(r->root);
    CcProject *p = v->p;
    avp_mutex_unlock(&g_lock);
    CcContext *c = cc_context_new(p, cc_unit_path(avp_scratch(), unit));
    avp_arena_reset(avp_scratch(), mk);
    return c;
}

/* os.path.commonpath([path, root]) == root, both normcase(abspath()); false across drives */
static bool under(AvpArena *a, const char *path, const char *root) {
    char *p = avp_path_normcase(a, avp_path_abspath(a, path));
    char *q = avp_path_normcase(a, avp_path_abspath(a, root));
    const char *paths[2] = {p, q};
    char *common;
    if (!avp_path_commonpath(a, paths, 2, &common)) {
        avp_io_clear();
        return false;
    }
    return strcmp(common, q) == 0;
}

AvpMap *cc_load_compiled(AvpArena *a, const CcRepo *r, const char *obj_path0, const char *out_dir,
                         const CcListingCache *cache, AvpStr *listing, CcError *err) {
    char *obj_path = avp_path_abspath(a, obj_path0);
    AvpBytes bytes = {0};
    if (!avp_read_all(a, obj_path, &bytes)) {
        set_err(err, CC_ERR_OS, avp_io_err_str(a));
        return NULL;
    }
    AvpStr data = avp_strn((const char *)bytes.v, bytes.n);
    const char *why;
    const CcElf *elf = cc_elf_parse(a, data.p, data.n, &why);
    if (!elf) {
        set_err(err, CC_ERR_VALUE, avp_str(why));
        return NULL;
    }
    AvpStr text = {0};
    if (cache && cache->get && cache->get(cache->ctx, data, &text)) {
        if (out_dir) {
            avp_makedirs(out_dir, true);
            AvpStr base = avp_path_basename(obj_path);
            char *dst = avp_path_join(a, out_dir, avp_sprintf(a, "%.*s.s", AVP_SF(base)));
            avp_write_file(dst, text.p, text.n);
        }
        if (listing) *listing = text;
        return cc_parse_listing(a, text, elf);
    }
    AvpStr head, tail;
    avp_path_split(obj_path, &head, &tail);
    char *obj_dir = avp_cz(a, head), *base = avp_cz(a, tail);
    char *tmp = NULL;
    if (!out_dir && under(a, obj_path, cc_repo_src_obj_dir(a, r))) out_dir = tmp = avp_mkdtemp(a, "shsm-listing-", "", NULL);
    char *work = out_dir ? avp_path_abspath(a, out_dir) : obj_dir;
    bool copied = strcmp(avp_path_normcase(a, work), avp_path_normcase(a, obj_dir)) != 0;
    bool ok = true;
    if (copied) {
        if (!avp_makedirs(work, true) || !avp_copyfile(obj_path, avp_path_join(a, work, base))) {
            set_err(err, CC_ERR_OS, avp_io_err_str(a));
            ok = false;
        }
    }
    if (ok) {
        char *dtk = cc_repo_dtk(a, r);
        char *lst = avp_sprintf(a, "%s.s", base);
        const char *argv[] = {dtk, "elf", "disasm", base, lst, NULL};
        AvpProcSpec spec = {0};
        spec.argv = argv;
        spec.cwd = work;
        spec.out = AVP_IO_CAPTURE;
        spec.err = AVP_IO_CAPTURE;
        spec.text = true;
        spec.errors = AVP_ERRORS_REPLACE;
        spec.timeout = 120;
        spec.arena = a;
        AvpProcResult pr = avp_run(&spec);
        if (pr.status == AVP_PROC_START_FAILED) {
            set_err(err, CC_ERR_USAGE, S(avp_sprintf(a, "cannot run %s: %s", dtk, pr.message ? pr.message : "")));
            ok = false;
        } else if (pr.status == AVP_PROC_TIMEOUT || pr.returncode != 0) {
            AvpStr outp = avp_str_strip(avp_str_concat(a, pr.out, pr.err));
            AvpStrVec ls = avp_str_splitlines(outp, false);
            AvpStr lastl = ls.n ? ls.v[ls.n - 1] : AVP_S("");
            set_err(err, CC_ERR_USAGE, S(avp_sprintf(a, "dtk elf disasm %s failed: %.*s", obj_path, AVP_SF(lastl))));
            avp_vec_free(&ls);
            ok = false;
        } else if (!avp_read_text(a, avp_path_join(a, work, lst), AVP_TM_UTF8_NL_EMPTY, &text)) {
            set_err(err, CC_ERR_OS, avp_io_err_str(a));
            ok = false;
        }
    }
    if (tmp) avp_rmtree(tmp, 1);
    else if (copied) {
        avp_remove(avp_path_join(a, work, base));
        avp_io_clear();
    }
    if (!ok) return NULL;
    if (cache && cache->put) cache->put(cache->ctx, data, text);
    if (listing) *listing = text;
    return cc_parse_listing(a, text, elf);
}

/* ---- comparing -------------------------------------------------------------------------------------------------- */

CcCompareResult *cc_compare_function_unit(AvpArena *a, const CcRepo *r, AvpStr unit, const char *obj_path,
                                          AvpStr symbol, AvpStr retail_name, bool targets, const AvpMap *compiled,
                                          const char *asm_path, CcError *err) {
    const AvpMap *retail = cc_load_retail(r, unit, asm_path, false, err);
    if (!retail) return NULL;
    const AvpMap *comp = compiled ? compiled : cc_load_compiled(a, r, obj_path, NULL, NULL, NULL, err);
    if (!comp) return NULL;
    AvpVal *cv = avp_map_get(comp, symbol);
    if (!cv) {
        set_err(err, CC_ERR_KEY, S(avp_sprintf(a, "%.*s is not in %s", AVP_SF(symbol), obj_path)));
        return NULL;
    }
    AvpStr rname = retail_name.n ? retail_name : cc_retail_name_for(symbol, retail);
    AvpVal *rv = rname.p ? avp_map_get(retail, rname) : NULL;
    if (!rv) {
        set_err(err, CC_ERR_KEY, S(avp_sprintf(a, "no retail function for %.*s in %.*s", AVP_SF(symbol), AVP_SF(unit))));
        return NULL;
    }
    CcFnResult *res = cc_compare_function(a, rv->p, cv->p, targets ? cc_repo_context(r, unit) : NULL, targets);
    return cc_compare_result_new(a, symbol, rname, rv->p, cv->p, res);
}

static int cmp_str(const void *x, const void *y) { return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y); }

static CcNames freeze_sorted(AvpArena *a, AvpStrVec *v) {
    CcNames n = {AVP_NEWN(a, AvpStr, v->n ? v->n : 1), v->n};
    if (v->n) memcpy(n.v, v->v, v->n * sizeof(AvpStr));
    qsort(n.v, n.n, sizeof(AvpStr), cmp_str);
    avp_vec_free(v);
    return n;
}

CcObjectComparison *cc_compare_object(AvpArena *a, const CcRepo *r, AvpStr unit, const char *obj_path,
                                      const AvpSet *symbols, bool targets, const AvpMap *names, const char *asm_path,
                                      const AvpMap *compiled, const char *out_dir, const CcListingCache *cache,
                                      CcError *err) {
    const AvpMap *retail = cc_load_retail(r, unit, asm_path, false, err);
    if (!retail) return NULL;
    const AvpMap *comp = compiled ? compiled : cc_load_compiled(a, r, obj_path, out_dir, cache, NULL, err);
    if (!comp) return NULL;
    CcContext *ctx = targets ? cc_repo_context(r, unit) : NULL;
    CcObjectComparison *oc = AVP_NEW(a, CcObjectComparison);
    oc->results = avp_map_new();
    oc->retail = retail;
    oc->compiled = comp;
    AvpSet *paired = avp_set_new();
    AvpStrVec compiled_only = {0}, retail_only = {0};
    bool filter = symbols && avp_set_len(symbols) > 0;
    /* One compiled function per retail function: the one of the same name, else the first in the object. The
     * shared fn_<addr> prefix pairs a second function (a weak stub carrying another class's mangling) with the same
     * retail name; that one is compiled-only (match reports it as EXTRA), not a second result for the retail
     * function. */
    AvpMap *owner = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(comp, &it)) {
        AvpStr rname = {NULL, 0};
        AvpVal *ov = names ? avp_map_get(names, it.key) : NULL;
        if (ov && ov->p && ((AvpStr *)ov->p)->n) rname = *(AvpStr *)ov->p;
        else rname = cc_retail_name_for(it.key, retail);
        if (!rname.p || !avp_map_get(retail, rname)) continue;
        if (avp_map_has(owner, rname) && !avp_str_eq(it.key, rname)) continue;
        AvpStr *o = AVP_NEW(a, AvpStr);
        *o = it.key;
        avp_map_put(owner, rname, NULL)->p = o;
    }
    it.pos = 0;
    while (avp_map_next(comp, &it)) {
        AvpStr rname = {NULL, 0};
        AvpVal *ov = names ? avp_map_get(names, it.key) : NULL;
        if (ov && ov->p && ((AvpStr *)ov->p)->n) rname = *(AvpStr *)ov->p;
        else rname = cc_retail_name_for(it.key, retail);
        AvpVal *rv = rname.p ? avp_map_get(retail, rname) : NULL;
        if (!rv || !avp_str_eq(*(const AvpStr *)avp_map_get(owner, rname)->p, it.key)) {
            avp_vec_push(&compiled_only, it.key);
            continue;
        }
        avp_set_add(paired, rname);
        if (filter && !avp_set_has(symbols, it.key)) continue;
        CcFnResult *res = cc_compare_function(a, rv->p, it.val->p, ctx, targets);
        AvpStr cname = avp_str_dup(a, it.key);
        avp_map_put(oc->results, cname, NULL)->p = cc_compare_result_new(a, cname, avp_str_dup(a, rname), rv->p, it.val->p, res);
    }
    it.pos = 0;
    while (avp_map_next(retail, &it))
        if (!avp_set_has(paired, it.key)) avp_vec_push(&retail_only, it.key);
    oc->compiled_only = freeze_sorted(a, &compiled_only);
    oc->retail_only = freeze_sorted(a, &retail_only);
    avp_set_free(paired);
    avp_map_free(owner);
    return oc;
}
