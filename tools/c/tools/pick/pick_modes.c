/* pick.py: the modes (lost, close, near, fresh, pending, drafts) and their text lines. */
#include "ledger.h"
#include "rules_cli.h"
#include "pick.h"
#include "shsm_asmindex.h"
#include "shsm_re.h"

#include <math.h>
#include <string.h>

/* ---- small JSON helpers ---- */

static void jset(AvpArena *a, AvpJson *o, const char *key, AvpJson *v) { avp_json_set(a, o, avp_str(key), v); }
static AvpJson *jstrz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
static AvpJson *jint(AvpArena *a, i64 v) { return avp_json_int(a, v); }
static AvpJson *jaddr(AvpArena *a, bool has, i64 v) { return has ? avp_json_int(a, v) : avp_json_null(); }
static const char *gs(AvpArena *a, const AvpJson *o, const char *key) {
    return avp_cz(a, ledger_py_str(a, avp_json_getz(o, key)));
}
static i64 gi(const AvpJson *o, const char *key) { return avp_json_get_i64(o, key, 0); }
static double gf(const AvpJson *o, const char *key) { return avp_json_get_f64(o, key, 0.0); }
/* pct() of a JSON number or null */
static const char *jpct(AvpArena *a, const AvpJson *v) {
    return pick_pct(a, !avp_json_is_null(v), avp_json_f64(v, 0.0));
}
static int cmp_f64(double x, double y) { return x < y ? -1 : x > y ? 1 : 0; }
static int cmp_i64(i64 x, i64 y) { return x < y ? -1 : x > y ? 1 : 0; }

/* type(v).__name__ of a JSON value */
static const char *py_type(const AvpJson *v) {
    static const char *const names[] = {"NoneType", "bool", "int", "float", "str", "list", "dict"};
    return v ? names[v->kind] : "NoneType";
}

/* ---- lost ---- */

static const char *recovery_of(AvpArena *a, const AvpJson *d) {
    const AvpJson *v = avp_json_getz(d, "recovery");
    if (avp_json_truthy(v)) return avp_cz(a, ledger_py_str(a, v));
    const AvpJson *kind = avp_json_getz(d, "historical_kind"), *key = avp_json_getz(d, "historical_key"),
                  *path = avp_json_getz(d, "historical_path");
    if (avp_json_is_str(kind) && avp_str_eq(kind->u.str, AVP_S("git")) && avp_json_truthy(key) &&
        avp_json_truthy(path)) {
        AvpStr k = avp_u8_slice(ledger_py_str(a, key), 0, 12), p = ledger_py_str(a, path);
        return avp_cz(a, avp_pyfmt(a, "git show %S:%S", &k, &p));
    }
    const AvpJson *h = avp_json_getz(d, "historical_hash");
    if (avp_json_truthy(h)) {
        AvpStr ad = ledger_py_str(a, avp_json_getz(d, "address")), hh = avp_u8_slice(ledger_py_str(a, h), 0, 10);
        return avp_cz(a, avp_pyfmt(a, "build/tools/score_history.exe show %S --source %S", &ad, &hh));
    }
    const AvpJson *s = avp_json_getz(d, "symbol");
    AvpStr which = ledger_py_str(a, avp_json_truthy(s) ? s : avp_json_getz(d, "address"));
    return avp_cz(a, avp_pyfmt(a, "build/tools/score_history.exe show %S", &which));
}

static int rank_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return cmp_f64(gf(*(AvpJson *const *)y, "rank"), gf(*(AvpJson *const *)x, "rank"));
}

PickItems pick_mode_lost(AvpArena *a, PickWorld *w, const PickArgs *o, const char **note) {
    PickItems items = {0};
    PickItems *rows = pick_history_rows(a, w, note);
    if (!rows) return items;
    for (size_t i = 0; i < rows->n; i++) {
        const AvpJson *d = rows->v[i];
        const AvpJson *lost = avp_json_getz(d, "lost");
        if (lost && !avp_json_truthy(lost)) continue;
        const AvpJson *uv = avp_json_getz(d, "unit");
        const char *unit = shsm_unit_path(a, uv ? avp_cz(a, ledger_py_str(a, uv)) : "");
        const AvpJson *sv = avp_json_getz(d, "symbol");
        const char *sym = avp_json_truthy(sv) ? avp_cz(a, ledger_py_str(a, sv)) : "";
        const ShsmFunctionReport *fr = shsm_report_function(w->report, sym, unit);
        if (!fr) fr = shsm_report_function(w->report, sym, NULL);
        i64 size = fr ? fr->size : 0;
        AvpJson *addr = avp_json_getz(d, "address");
        bool has_addr = false;
        i64 addr_i = 0;
        if (avp_json_is_str(addr)) {
            if (avp_parse_int(addr->u.str, 16, &addr_i, NULL)) {
                has_addr = true;
                addr = avp_json_int(a, addr_i);
            } else {
                addr = avp_json_null();
            }
        } else if (avp_json_is_int(addr) && !addr->big) {
            has_addr = true;
            addr_i = addr->u.i;
        }
        if (!addr) addr = avp_json_null();
        PickFn *f = has_addr ? pick_world_by_address(w, addr_i) : NULL;
        const char *lib = f ? f->library : NULL;
        if (!f) {
            lib = has_addr ? shsm_libraries_lookup(a, w->libraries, addr_i) : NULL;
            if (!lib) lib = "unknown";
        }
        PickFn probe = {.size = size, .library = lib, .unit = unit,
                        .unit_library = pick_world_unit_library(w, unit)};
        if (!pick_keep(a, &probe, o)) continue;
        AvpJson *cur = avp_json_getz(d, "current");
        if (!cur) cur = avp_json_null();
        AvpJson *hist = avp_json_getz(d, "historical");
        if (!avp_json_truthy(hist)) hist = avp_json_float(a, 0.0);
        AvpJson *best = avp_json_getz(d, "best");
        if (avp_json_is_null(best)) best = avp_json_truthy(cur) ? cur : avp_json_float(a, 0.0);
        i64 m = size > 1 ? size : 1;
        AvpJson *rank;
        if (avp_json_is_int(hist) && avp_json_is_int(best) && !hist->big && !best->big)
            rank = avp_json_int(a, (hist->u.i - best->u.i) * m);
        else
            rank = avp_json_float(a, (avp_json_f64(hist, 0.0) - avp_json_f64(best, 0.0)) * (double)m);
        AvpJson *it = avp_json_obj(a);
        jset(a, it, "unit", jstrz(a, unit));
        jset(a, it, "symbol", jstrz(a, sym));
        jset(a, it, "address", addr);
        jset(a, it, "size", jint(a, size));
        jset(a, it, "library", jstrz(a, lib));
        jset(a, it, "current", cur);
        jset(a, it, "best", best);
        jset(a, it, "historical", hist);
        jset(a, it, "rank", rank);
        jset(a, it, "command", jstrz(a, recovery_of(a, d)));
        avp_vec_push(&items, it);
    }
    avp_sort(items.v, items.n, sizeof(AvpJson *), rank_desc, NULL, false);
    return items;
}

AvpStr pick_fmt_lost(AvpArena *a, const AvpJson *it) {
    const AvpJson *cur = avp_json_getz(it, "current");
    const char *c = avp_json_is_null(cur) ? "-" : jpct(a, cur);
    const char *size = avp_cz(a, avp_pyfmt(a, "0x%X", gi(it, "size")));
    return avp_pyfmt(a, "%-40.40s %-48.48s %5s now %-6s best %-6s hist %-6s  %s", gs(a, it, "unit"),
                     gs(a, it, "symbol"), size, c, jpct(a, avp_json_getz(it, "best")),
                     jpct(a, avp_json_getz(it, "historical")), gs(a, it, "command"));
}

/* ---- close ---- */

typedef struct {
    const PickFn *f;
} FnRef;

static int remaining_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const PickFn *f = *(const PickFn *const *)x, *g = *(const PickFn *const *)y;
    int c = cmp_f64(-f->fuzzy, -g->fuzzy);
    return c ? c : cmp_i64(f->size, g->size);
}

static int work_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpJson *p = *(AvpJson *const *)x, *q = *(AvpJson *const *)y;
    int c = cmp_i64(gi(p, "left"), gi(q, "left"));
    if (!c) c = cmp_i64(gi(p, "bytes_left"), gi(q, "bytes_left"));
    if (!c) c = avp_str_cmp(avp_json_get_str(p, "unit", AVP_S("")), avp_json_get_str(q, "unit", AVP_S("")));
    return c;
}

static int cand_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpJson *p = *(AvpJson *const *)x, *q = *(AvpJson *const *)y;
    int c = cmp_i64(gi(q, "bytes"), gi(p, "bytes"));
    if (!c) c = avp_str_cmp(avp_json_get_str(p, "unit", AVP_S("")), avp_json_get_str(q, "unit", AVP_S("")));
    return c;
}

static const char *diff_cmd(AvpArena *a, const char *unit, const char *symbol) {
    return avp_cz(a, avp_pyfmt(a, "build/tools/match.exe %s --diff %s", unit, symbol));
}

void pick_mode_close(AvpArena *a, PickWorld *w, const PickArgs *o, PickItems *work, PickItems *candidates) {
    memset(work, 0, sizeof *work);
    memset(candidates, 0, sizeof *candidates);
    AvpMapIter iter = {0};
    while (avp_map_next(w->by_unit, &iter)) {
        const char *path = avp_cz(a, iter.key);
        const PickFnVec *fl = iter.val->p;
        const char *status = pick_world_unit_status(w, path);
        if (strcmp(status, "NonMatching") != 0 && strcmp(status, "Equivalent") != 0) continue;
        const char *lib = pick_world_unit_lib(w, path);
        if (!pick_lib_matches(a, lib, o->lib, pick_world_unit_library(w, path)) || !pick_unit_matches(a, path, o->unit))
            continue;
        size_t nleft = 0;
        AVP_VEC(const PickFn *) below = {0};
        i64 bytes_left = 0, bytes = 0;
        for (size_t i = 0; i < fl->n; i++) {
            const PickFn *f = fl->v[i];
            bytes += f->size;
            if (f->fuzzy >= 100.0) continue;
            avp_vec_push(&below, f);
            bytes_left += f->size;
            if ((!o->has_min_size || f->size >= o->min_size) && (!o->has_max_size || f->size <= o->max_size)) nleft++;
        }
        if ((o->has_min_size || o->has_max_size) && below.n && !nleft) {
            avp_vec_free(&below);
            continue;
        }
        avp_sort(below.v, below.n, sizeof(const PickFn *), remaining_cmp, NULL, false);
        AvpJson *it = avp_json_obj(a);
        jset(a, it, "unit", jstrz(a, path));
        jset(a, it, "status", jstrz(a, status));
        jset(a, it, "library", jstrz(a, lib));
        jset(a, it, "functions", jint(a, (i64)fl->n));
        jset(a, it, "exact", jint(a, (i64)(fl->n - below.n)));
        jset(a, it, "left", jint(a, (i64)below.n));
        jset(a, it, "bytes_left", jint(a, bytes_left));
        jset(a, it, "bytes", jint(a, bytes));
        AvpJson *rem = avp_json_arr(a);
        for (size_t i = 0; i < below.n; i++) {
            AvpJson *r = avp_json_obj(a);
            jset(a, r, "symbol", jstrz(a, below.v[i]->name));
            jset(a, r, "percent", below.v[i]->scored ? avp_json_float(a, below.v[i]->fuzzy) : avp_json_null());
            jset(a, r, "size", jint(a, below.v[i]->size));
            avp_json_arr_push(a, rem, r);
        }
        jset(a, it, "remaining", rem);
        if (below.n) {
            jset(a, it, "command", jstrz(a, diff_cmd(a, path, below.v[0]->name)));
            avp_vec_push(work, it);
        } else {
            jset(a, it, "command", jstrz(a, avp_cz(a, avp_pyfmt(a, "build/tools/unit.exe flip %s", path))));
            avp_vec_push(candidates, it);
        }
        avp_vec_free(&below);
    }
    avp_sort(work->v, work->n, sizeof(AvpJson *), work_cmp, NULL, false);
    avp_sort(candidates->v, candidates->n, sizeof(AvpJson *), cand_cmp, NULL, false);
}

AvpStr pick_fmt_close(AvpArena *a, const AvpJson *it) {
    const AvpJson *rem = avp_json_getz(it, "remaining");
    size_t n = avp_json_len(rem);
    AvpBuf b = {.a = a};
    for (size_t i = 0; i < n && i < 3; i++) {
        const AvpJson *r = avp_json_at(rem, i);
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_pyfmt(&b, "%.40s %s 0x%X", gs(a, r, "symbol"), jpct(a, avp_json_getz(r, "percent")), gi(r, "size"));
    }
    if (n > 3) avp_buf_pyfmt(&b, ", +%d more", (i64)(n - 3));
    return avp_pyfmt(a, "%-42.42s %d/%d exact, %d left (0x%X bytes): %s  %s", gs(a, it, "unit"), gi(it, "exact"),
                     gi(it, "functions"), gi(it, "left"), gi(it, "bytes_left"), avp_buf_cz(&b), gs(a, it, "command"));
}

AvpStr pick_fmt_candidate(AvpArena *a, const AvpJson *it) {
    return avp_pyfmt(a, "%-42.42s %d/%d exact, 0x%X bytes, needs gate/rules  %s", gs(a, it, "unit"), gi(it, "exact"),
                     gi(it, "functions"), gi(it, "bytes"), gs(a, it, "command"));
}

/* ---- near ---- */

static int near_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpJson *p = *(AvpJson *const *)x, *q = *(AvpJson *const *)y;
    int c = cmp_f64(gf(q, "rank"), gf(p, "rank"));
    return c ? c : cmp_i64(gi(p, "address"), gi(q, "address"));
}

PickItems pick_mode_near(AvpArena *a, PickWorld *w, const PickArgs *o) {
    PickItems items = {0};
    for (size_t i = 0; i < w->fns.n; i++) {
        const PickFn *f = w->fns.v[i];
        if (strcmp(f->status, "Asm") == 0 || f->linked || !f->scored || !(o->threshold <= f->fuzzy && f->fuzzy < 100.0) ||
            !pick_keep(a, f, o))
            continue;
        AvpJson *it = avp_json_obj(a);
        jset(a, it, "unit", jstrz(a, f->unit));
        jset(a, it, "symbol", jstrz(a, f->name));
        jset(a, it, "address", jaddr(a, f->has_address, f->address));
        jset(a, it, "size", jint(a, f->size));
        jset(a, it, "percent", avp_json_float(a, f->fuzzy));
        jset(a, it, "library", jstrz(a, f->library));
        jset(a, it, "rank", avp_json_float(a, (double)f->size * pow(f->fuzzy / 100.0, 2.0)));
        jset(a, it, "command", jstrz(a, diff_cmd(a, f->unit, f->name)));
        avp_vec_push(&items, it);
    }
    avp_sort(items.v, items.n, sizeof(AvpJson *), near_cmp, NULL, false);
    return items;
}

AvpStr pick_fmt_near(AvpArena *a, const AvpJson *it) {
    return avp_pyfmt(a, "%-40.40s %-48.48s %6s 0x%-5X  %s", gs(a, it, "unit"), gs(a, it, "symbol"),
                     jpct(a, avp_json_getz(it, "percent")), gi(it, "size"), gs(a, it, "command"));
}

/* ---- fresh ---- */

static int addr_none_last(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 p = *(const i64 *)x, q = *(const i64 *)y;
    bool pn = p == SHSM_NONE, qn = q == SHSM_NONE;
    if (pn != qn) return pn ? 1 : -1;
    return cmp_i64(pn ? 0 : p, qn ? 0 : q);
}

static int small_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpJson *p = *(AvpJson *const *)x, *q = *(AvpJson *const *)y;
    int c = cmp_i64(gi(p, "size"), gi(q, "size"));
    return c ? c : cmp_i64(gi(p, "address"), gi(q, "address"));
}

PickItems pick_mode_fresh(AvpArena *a, PickWorld *w, const PickArgs *o, i64 limit) {
    PickItems items = {0};
    PickFnVec cand = {0};
    for (size_t i = 0; i < w->fns.n; i++) {
        PickFn *f = w->fns.v[i];
        if (strcmp(f->status, "Asm") == 0 && strncmp(f->name, "pad_", 4) != 0 && pick_keep(a, f, o))
            avp_vec_push(&cand, f);
    }
    if (!cand.n) return items;
    ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = w->repo, .readonly = true});
    /* call_graph(ix) */
    AvpIMap *nrows_of = avp_imap_new();
    ShsmSymRows fns = shsm_asmindex_functions(a, ix);
    for (size_t i = 0; i < fns.n; i++) avp_imap_put(nrows_of, (u64)fns.v[i].address, NULL)->i = fns.v[i].nrows;
    avp_vec_free(&fns);
    AvpIMap *callees = avp_imap_new(), *callers = avp_imap_new();
    ShsmCallEdges edges = shsm_asmindex_call_graph(a, ix, NULL, 0);
    for (size_t i = 0; i < edges.n; i++) {
        const ShsmCallEdge *e = &edges.v[i];
        if (e->caller_addr == SHSM_NONE) continue;
        bool created = false;
        AvpVal *v = avp_imap_put(callees, (u64)e->caller_addr, &created);
        if (created) v->p = AVP_NEW(a, AvpI64Vec);
        avp_vec_push((AvpI64Vec *)v->p, e->callee_addr);
        if (e->callee_addr != SHSM_NONE) avp_imap_put(callers, (u64)e->callee_addr, NULL)->i += 1;
    }
    avp_vec_free(&edges);
    for (size_t i = 0; i < cand.n; i++) {
        const PickFn *f = cand.v[i];
        AvpVal *nv = f->has_address ? avp_imap_get(nrows_of, (u64)f->address) : NULL;
        bool has_rows = nv && nv->i != SHSM_NONE;
        i64 nrows = has_rows ? nv->i : 0;
        AvpI64Vec uniq = {0};
        AvpVal *cv = f->has_address ? avp_imap_get(callees, (u64)f->address) : NULL;
        if (cv) {
            const AvpI64Vec *outs = cv->p;
            for (size_t k = 0; k < outs->n; k++) {
                i64 x = outs->v[k];
                if (x == f->address) continue;
                bool dup = false;
                for (size_t j = 0; j < uniq.n && !dup; j++) dup = uniq.v[j] == x;
                if (!dup) avp_vec_push(&uniq, x);
            }
        }
        avp_sort(uniq.v, uniq.n, sizeof(i64), addr_none_last, NULL, false);
        i64 matched = 0;
        for (size_t k = 0; k < uniq.n; k++) {
            if (uniq.v[k] == SHSM_NONE) continue;
            PickFn *g = pick_world_by_address(w, uniq.v[k]);
            if (g && pick_world_matched(g)) matched++;
        }
        bool leaf = uniq.n == 0;
        i64 ncallees = (i64)uniq.n;
        avp_vec_free(&uniq);
        if (o->leaf && matched < ncallees) continue;
        bool tiny = leaf && has_rows && nrows < o->tiny;
        AvpVal *callers_v = f->has_address ? avp_imap_get(callers, (u64)f->address) : NULL;
        AvpJson *it = avp_json_obj(a);
        jset(a, it, "unit", jstrz(a, f->unit));
        jset(a, it, "symbol", jstrz(a, f->name));
        jset(a, it, "address", jaddr(a, f->has_address, f->address));
        jset(a, it, "size", jint(a, f->size));
        jset(a, it, "rows", has_rows ? jint(a, nrows) : avp_json_null());
        jset(a, it, "library", jstrz(a, f->library));
        jset(a, it, "reference", jstrz(a, pick_reference_flag(f->library, NULL)));
        jset(a, it, "callers", jint(a, callers_v ? callers_v->i : 0));
        jset(a, it, "callees", jint(a, ncallees));
        jset(a, it, "callees_matched", jint(a, matched));
        jset(a, it, "leaf", avp_json_bool(leaf));
        jset(a, it, "tiny", avp_json_bool(tiny));
        jset(a, it, "command", jstrz(a, avp_cz(a, avp_pyfmt(a, "build/tools/brief.exe %s", f->name))));
        jset(a, it, "unit_command",
             jstrz(a, avp_cz(a, avp_pyfmt(a, "build/tools/unit.exe new <path> --text 0x%08X-0x%08X --dry-run",
                                          f->address, f->address + f->size))));
        avp_vec_push(&items, it);
    }
    avp_vec_free(&cand);
    avp_imap_free(nrows_of);
    avp_imap_free(callers);
    avp_imap_free(callees);
    if (o->small) avp_sort(items.v, items.n, sizeof(AvpJson *), small_cmp, NULL, false);
    i64 cap = o->all ? 0 : (limit ? limit : o->limit);
    size_t shown = cap && (size_t)cap < items.n ? (size_t)cap : items.n;
    for (size_t i = 0; i < items.n; i++) {
        if (i >= shown) {
            jset(a, items.v[i], "strings", avp_json_null());
            continue;
        }
        AvpJson *strs = avp_json_arr(a);
        ShsmStrRows rows = shsm_asmindex_strings_of(a, ix, shsm_target_addr(gi(items.v[i], "address")), false);
        AvpSet *seen = avp_set_new();
        for (size_t k = 0; k < rows.n && avp_json_len(strs) < 4; k++)
            if (avp_set_add(seen, rows.v[k].text)) avp_json_arr_push(a, strs, avp_json_str(a, rows.v[k].text));
        avp_set_free(seen);
        avp_vec_free(&rows);
        jset(a, items.v[i], "strings", strs);
    }
    shsm_asmindex_close(ix);
    return items;
}

AvpStr pick_fmt_fresh(AvpArena *a, const AvpJson *it) {
    const char *flags = "";
    if (avp_json_get_bool(it, "tiny", false)) flags = " tiny";
    else if (avp_json_get_bool(it, "leaf", false)) flags = " leaf";
    AvpBuf strs = {.a = a};
    const AvpJson *sv = avp_json_getz(it, "strings");
    if (avp_json_truthy(sv)) {
        avp_buf_putz(&strs, " strings: ");
        for (size_t i = 0; i < avp_json_len(sv) && i < 2; i++) {
            if (i) avp_buf_putz(&strs, "; ");
            avp_json_put_string(&strs, avp_u8_slice(avp_json_str_or(avp_json_at(sv, i), AVP_S("")), 0, 40), false);
        }
    }
    const AvpJson *rows = avp_json_getz(it, "rows");
    const char *rs = avp_json_is_null(rows) ? "?" : avp_cz(a, avp_pyfmt(a, "%d", rows->u.i));
    const char *cmd = avp_cz(a, avp_pyfmt(a, "%s  then  %s", gs(a, it, "command"), gs(a, it, "unit_command")));
    return avp_pyfmt(a, "0x%08X %-40.40s %-14.14s ref:%-7s 0x%-5X %4s ins, %d callers, callees %d/%d%s%s  %s",
                     gi(it, "address"), gs(a, it, "symbol"), gs(a, it, "library"), gs(a, it, "reference"),
                     gi(it, "size"), rs, gi(it, "callers"), gi(it, "callees_matched"), gi(it, "callees"), flags,
                     avp_buf_cz(&strs), cmd);
}

/* ---- pending ---- */

/* ledger.read_text(path)[0]: utf-8 with errors="replace", CRLF to LF. */
static AvpStr read_exceptions(AvpArena *a, const char *path) {
    AvpStr raw = {0};
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &raw)) shsm_raise_io();
    return avp_str_replace(a, raw, AVP_S("\r\n"), AVP_S("\n"), -1);
}

/* ledger.run_checker(root): the rule checker, run in-process (tool_check_rules). */
static AvpJson *run_checker(AvpArena *a, const char *root) {
    const char *why = NULL;
    AvpJson *doc = rl_checker_json(a, root, NULL, &why);
    if (!doc) shsm_raise(SHSM_EXC_USAGE, "%s", why);
    return doc;
}

static bool seen_add(AvpArena *a, AvpSet *seen, AvpStr u, AvpStr fn) {
    return avp_set_add(seen, avp_pyfmt(a, "%S%c%S", &u, 0, &fn));
}

PickItems pick_pending_records(AvpArena *a, PickWorld *w, bool checker) {
    PickItems out = {0};
    AvpSet *seen = avp_set_new();
    const char *path = avp_join(a, w->repo->root, "docs", "exceptions.md");
    ShsmRe *rule_re = shsm_re("R\\d+", 0);
    if (avp_isfile(path)) {
        LedgerExcRows rows = ledger_parse_exceptions(a, read_exceptions(a, path));
        for (size_t i = 0; i < rows.n; i++) {
            const LedgerExcRow *r = rows.v[i];
            if (strcmp(r->table, "MATCH-PENDING") != 0) continue;
            AvpJson *rules = avp_json_arr(a);
            if (r->cells.n >= 3) {
                ShsmReFindall fa = shsm_re_findall(rule_re, r->cells.v[2], 0, AVP_END);
                for (size_t k = 0; k < fa.count; k++) avp_json_arr_push(a, rules, avp_json_str(a, shsm_re_findall_at(&fa, k, 0)));
                shsm_re_findall_free(&fa);
            }
            AvpStr note = r->cells.n >= 4 ? r->cells.v[3] : AVP_S("");
            for (size_t ui = 0; ui < r->units.n; ui++) {
                const char *u = avp_cz(a, r->units.v[ui]);
                for (size_t fi = 0; fi < r->functions.n; fi++) {
                    const char *fn = avp_cz(a, r->functions.v[fi]);
                    if (!shsm_report_function(w->report, fn, u)) continue; /* a token of the prose */
                    if (!seen_add(a, seen, r->units.v[ui], r->functions.v[fi])) continue;
                    AvpJson *d = avp_json_obj(a);
                    jset(a, d, "unit", jstrz(a, u));
                    jset(a, d, "symbol", jstrz(a, fn));
                    jset(a, d, "rules", rules);
                    jset(a, d, "note", avp_json_str(a, note));
                    jset(a, d, "source", jstrz(a, "exceptions.md"));
                    avp_vec_push(&out, d);
                }
            }
        }
    }
    if (checker) {
        AvpJson *data = run_checker(a, w->repo->root);
        AvpJson *fl = avp_json_getz(data, "functions");
        for (size_t i = 0; i < avp_json_len(fl); i++) {
            const AvpJson *f = avp_json_at(fl, i);
            AvpStr st = avp_json_get_str(f, "status", AVP_S("")), fp = avp_json_get_str(f, "path", AVP_S(""));
            if (!avp_str_eq(st, AVP_S("MATCH-PENDING")) || !avp_str_startswith(fp, AVP_S("src/"))) continue;
            AvpStr u = ledger_unit_key(a, fp), name = avp_json_get_str(f, "name", AVP_S(""));
            if (!seen_add(a, seen, u, name)) continue;
            AvpJson *rules = avp_json_arr(a);
            const AvpJson *mr = avp_json_getz(f, "match_rules");
            for (size_t k = 0; k < avp_json_len(mr) && avp_json_is_arr(mr); k++) avp_json_arr_push(a, rules, avp_json_at(mr, k));
            AvpJson *d = avp_json_obj(a);
            jset(a, d, "unit", avp_json_str(a, u));
            jset(a, d, "symbol", avp_json_str(a, name));
            jset(a, d, "rules", rules);
            jset(a, d, "note", jstrz(a, ""));
            jset(a, d, "source", jstrz(a, "checker"));
            avp_vec_push(&out, d);
        }
    }
    avp_set_free(seen);
    return out;
}

static int pending_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpJson *p = *(AvpJson *const *)x, *q = *(AvpJson *const *)y;
    int c = (int)(gf(p, "percent") < 100.0) - (int)(gf(q, "percent") < 100.0);
    return c ? c : cmp_i64(gi(q, "size"), gi(p, "size"));
}

PickItems pick_mode_pending(AvpArena *a, PickWorld *w, const PickArgs *o) {
    PickItems items = {0};
    PickItems recs = pick_pending_records(a, w, o->checker);
    for (size_t i = 0; i < recs.n; i++) {
        AvpJson *rec = recs.v[i];
        const char *unit = gs(a, rec, "unit"), *sym = gs(a, rec, "symbol");
        const PickFnVec *ul = pick_world_unit_fns(w, unit);
        const ShsmFunctionReport *fr = shsm_report_function(w->report, sym, unit);
        const char *frname = fr ? fr->name : NULL;
        if (!fr) {
            for (size_t k = 0; ul && k < ul->n && !frname; k++) {
                AvpStr dm = avp_str(ul->v[k]->demangled ? ul->v[k]->demangled : "");
                AvpStr head, tail;
                if (!avp_str_partition(dm, AVP_S("("), &head, &tail)) head = dm;
                if (avp_str_endswith(head, avp_str(sym))) frname = ul->v[k]->name;
            }
            if (!frname) continue;
        }
        const PickFn *f = NULL;
        for (size_t k = 0; ul && k < ul->n && !f; k++)
            if (strcmp(ul->v[k]->name, frname) == 0) f = ul->v[k];
        if (!f || !pick_keep(a, f, o)) continue;
        AvpJson *it = avp_json_obj(a);
        for (size_t k = 0; k < avp_json_len(rec); k++) {
            const AvpJsonMember *m = avp_json_member(rec, k);
            avp_json_set(a, it, m->key, m->val);
        }
        jset(a, it, "symbol", jstrz(a, f->name));
        jset(a, it, "address", jaddr(a, f->has_address, f->address));
        jset(a, it, "size", jint(a, f->size));
        jset(a, it, "percent", avp_json_float(a, f->fuzzy));
        jset(a, it, "linked", avp_json_bool(f->linked));
        jset(a, it, "library", jstrz(a, f->library));
        jset(a, it, "command",
             jstrz(a, avp_cz(a, avp_pyfmt(a, "build/tools/check_rules.exe --units main/%s", f->unit))));
        avp_vec_push(&items, it);
    }
    avp_sort(items.v, items.n, sizeof(AvpJson *), pending_cmp, NULL, false);
    return items;
}

AvpStr pick_fmt_pending(AvpArena *a, const AvpJson *it) {
    const AvpJson *rules = avp_json_getz(it, "rules");
    AvpBuf rb = {.a = a};
    for (size_t i = 0; i < avp_json_len(rules); i++) {
        if (i) avp_buf_putz(&rb, ",");
        avp_buf_put(&rb, ledger_py_str(a, avp_json_at(rules, i)));
    }
    const char *rs = rb.n ? avp_buf_cz(&rb) : "-";
    AvpStr note = avp_json_get_str(it, "note", AVP_S(""));
    AvpStr nt = avp_u8_len(note) > 50 ? avp_pyfmt(a, "%.50S...", &note) : note;
    return avp_pyfmt(a, "%-36.36s %-48.48s %6s 0x%-5X %-8s %-10s %S  %s", gs(a, it, "unit"), gs(a, it, "symbol"),
                     jpct(a, avp_json_getz(it, "percent")), gi(it, "size"),
                     avp_json_get_bool(it, "linked", false) ? "linked" : "parked", rs, &nt, gs(a, it, "command"));
}

/* ---- drafts ---- */

AvpJson *pick_sweep_state(AvpArena *a, const ShsmRepo *repo, const char **note) {
    *note = NULL;
    const char *path = avp_join(a, repo->build_dir, "draft", "sweep.json");
    static const char ABSENT[] = "no current sweep (build/draft/sweep.json absent): run " PICK_SWEEP_COMMAND;
    AvpStr text = {0};
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) {
        *note = ABSENT;
        return NULL;
    }
    AvpJsonError je;
    AvpJson *doc = avp_json_parse(a, text, &je);
    if (!doc || !avp_json_is_obj(doc)) {
        *note = ABSENT;
        return NULL;
    }
    AvpJson *res = avp_json_getz(doc, "results");
    if (!avp_json_truthy(res)) res = avp_json_arr(a);
    double t = 0, ix_t = 0;
    if (!avp_getmtime(path, &t)) {
        *note = ABSENT;
        return NULL;
    }
    if (avp_getmtime(avp_join(a, repo->index_dir, "asm.sqlite"), &ix_t) && t < ix_t)
        *note = "the sweep is older than the asm index: rerun " PICK_SWEEP_COMMAND;
    return res;
}

PickItems pick_mode_drafts(AvpArena *a, PickWorld *w, const PickArgs *o, const char **note) {
    PickItems items = {0};
    AvpJson *res = pick_sweep_state(a, w->repo, note);
    if (!res) return items;
    AvpSet *started = avp_set_new();
    for (size_t i = 0; i < w->fns.n; i++) {
        const char *st = pick_world_unit_status(w, w->fns.v[i]->unit);
        if (st && strcmp(st, "Asm") != 0) avp_set_add(started, avp_str(w->fns.v[i]->name));
    }
    for (size_t i = 0; i < avp_json_len(res) && avp_json_is_arr(res); i++) {
        const AvpJson *r = avp_json_at(res, i);
        if (!avp_json_is_obj(r)) shsm_raise(SHSM_EXC_VALUE, "'%s' object has no attribute 'get'", py_type(r));
        const AvpJson *verdict = avp_json_getz(r, "verdict"), *sym = avp_json_getz(r, "symbol");
        if (!avp_json_is_str(verdict) || !avp_str_eq(verdict->u.str, AVP_S("MATCH"))) continue;
        if (avp_json_is_str(sym) && avp_set_has(started, sym->u.str)) continue;
        const AvpJson *lib = avp_json_getz(r, "lib");
        if (o->lib && *o->lib &&
            !pick_lib_matches(a, avp_json_truthy(lib) ? avp_cz(a, ledger_py_str(a, lib)) : NULL, o->lib, NULL))
            continue;
        const AvpJson *up = avp_json_getz(r, "unit_path");
        if (o->unit && *o->unit &&
            !pick_unit_matches(a, avp_json_truthy(up) ? avp_cz(a, ledger_py_str(a, up)) : "", o->unit))
            continue;
        if (!sym) shsm_raise(SHSM_EXC_KEY, "symbol");
        const AvpJson *twin = avp_json_getz(r, "twin");
        AvpJson *it = avp_json_obj(a);
        jset(a, it, "symbol", (AvpJson *)sym);
        jset(a, it, "library", lib ? (AvpJson *)lib : avp_json_null());
        jset(a, it, "twin", twin ? (AvpJson *)twin : avp_json_null());
        jset(a, it, "unit_path", up ? (AvpJson *)up : avp_json_null());
        AvpStr s = ledger_py_str(a, sym);
        jset(a, it, "command", avp_json_str(a, avp_pyfmt(a, "build/tools/draft.exe twin %S", &s)));
        avp_vec_push(&items, it);
    }
    avp_set_free(started);
    return items;
}

AvpStr pick_fmt_drafts(AvpArena *a, const AvpJson *it) {
    const AvpJson *lib = avp_json_getz(it, "library"), *twin = avp_json_getz(it, "twin");
    const char *l = avp_json_truthy(lib) ? avp_cz(a, ledger_py_str(a, lib)) : "?";
    const char *t = avp_json_truthy(twin) ? avp_cz(a, ledger_py_str(a, twin)) : "?";
    return avp_pyfmt(a, "%-40.40s %-12.12s twin %-40.40s %s", gs(a, it, "symbol"), l, t, gs(a, it, "command"));
}
