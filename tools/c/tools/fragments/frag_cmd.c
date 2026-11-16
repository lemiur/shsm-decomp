/* fragments.py binary subcommands: _occ_summary, _compact, select, cmd_list, cmd_show, cmd_in, cmd_stats, cmd_known. */
#include "frag.h"
#include "fragments_known.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/sort.h"

#include <string.h>

/* _compact(pattern, width) */
AvpStr frag_compact(AvpArena *a, const AvpStrVec *pattern, size_t width) {
    AvpStr s = avp_str_join(a, AVP_S("; "), pattern->v, pattern->n);
    if (avp_u8_len(s) <= width) return s;
    AvpStr head = avp_u8_slice(s, 0, (i64)width - 3);
    return avp_pyfmt(a, "%S...", &head);
}

typedef struct {
    i64 nf, nu, nm;
} Summary;

/* _occ_summary(f, occ): distinct functions, their distinct units, the matched ones */
static Summary occ_summary(const FragOccRec *occ, size_t n) {
    Summary s = {0, 0, 0};
    AvpIMap *fns = avp_imap_new();
    AVP_VEC(const FragOccRec *) last = {0};
    for (size_t i = 0; i < n; i++) {
        bool created;
        AvpVal *v = avp_imap_put(fns, (u64)occ[i].fn_address, &created);
        if (created) {
            v->i = (i64)last.n;
            avp_vec_push(&last, &occ[i]);
        } else {
            last.v[v->i] = &occ[i];
        }
    }
    AvpSet *units = avp_set_new();
    bool none_unit = false;
    for (size_t i = 0; i < last.n; i++) {
        if (last.v[i]->unit) avp_set_add(units, avp_str(last.v[i]->unit));
        else none_unit = true;
        if (last.v[i]->matched) s.nm++;
    }
    s.nf = (i64)last.n;
    s.nu = (i64)avp_set_len(units) + (none_unit ? 1 : 0);
    avp_set_free(units);
    avp_vec_free(&last);
    avp_imap_free(fns);
    return s;
}

typedef struct {
    i64 score;
    Frag *f;
    FragOccRecs occ;
    Summary s;
} Selected;

static int cmp_selected(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Selected *a = x, *b = y;
    if (a->score != b->score) return a->score > b->score ? -1 : 1;
    if (a->f->rank != b->f->rank) return a->f->rank < b->f->rank ? -1 : 1;
    return 0;
}

int frag_cmd_list(const FragArgs *args, Frags *fr, ShsmOutput *out) {
    AvpArena *a = fr->arena;
    FragList all = frags_level(fr, args->level);
    AVP_VEC(Selected) rows = {0};
    for (size_t i = 0; i < all.n; i++) {
        Frag *f = all.v[i];
        if (f->length < args->min_len) continue;
        Selected x = {0};
        x.f = f;
        for (size_t k = 0; k < f->occ.n; k++)
            if (frag_lib_matches(a, f->occ.v[k].lib, args->lib)) avp_vec_push(&x.occ, f->occ.v[k]);
        x.s = occ_summary(x.occ.v, x.occ.n);
        if (x.s.nf < args->min_functions || (args->unmatched && x.s.nm)) {
            avp_vec_free(&x.occ);
            continue;
        }
        x.score = x.s.nf * f->length;
        avp_vec_push(&rows, x);
    }
    avp_sort(rows.v, rows.n, sizeof *rows.v, cmp_selected, NULL, false);
    AvpStr in_lib = args->lib && *args->lib ? avp_pyfmt(a, " in %s", args->lib) : AVP_S("");
    shsm_output_line(out, avp_pyfmt(a, "%d %s fragment(s)%S, min length %d, min functions %d (window %s, seeded at %s sites)",
                                    (i64)rows.n, args->level, &in_lib, args->min_len, args->min_functions,
                                    frags_meta(fr, "window"), frags_meta(fr, "sites")));
    shsm_output_linez(out, "id       len  fns units matched  score  sequence");
    size_t shown = args->all || !args->limit || (size_t)args->limit >= rows.n ? rows.n : (size_t)args->limit;
    AvpJson *data = avp_json_arr(a);
    for (size_t i = 0; i < shown; i++) {
        const Selected *x = &rows.v[i];
        AvpStr c = frag_compact(a, &x->f->pattern, 150);
        shsm_output_line(out, avp_pyfmt(a, "%-7s %4d %4d %5d %7d %6d  %S", x->f->id, x->f->length, x->s.nf, x->s.nu,
                                        x->s.nm, x->score, &c));
        AvpJson *d = frag_json(a, x->f, false);
        avp_json_set(a, d, AVP_S("functions"), avp_json_int(a, x->s.nf));
        avp_json_set(a, d, AVP_S("units"), avp_json_int(a, x->s.nu));
        avp_json_set(a, d, AVP_S("matched"), avp_json_int(a, x->s.nm));
        avp_json_set(a, d, AVP_S("score"), avp_json_int(a, x->score));
        avp_json_arr_push(a, data, d);
    }
    if (rows.n > shown) shsm_output_line(out, avp_pyfmt(a, "... %d more fragment(s), use --all", (i64)(rows.n - shown)));
    shsm_output_data(out, "level", avp_json_strz(a, args->level));
    shsm_output_data(out, "lib", args->lib ? avp_json_strz(a, args->lib) : avp_json_null());
    shsm_output_data(out, "count", avp_json_int(a, (i64)rows.n));
    shsm_output_data(out, "fragments", data);
    for (size_t i = 0; i < rows.n; i++) avp_vec_free(&rows.v[i].occ);
    avp_vec_free(&rows);
    return SHSM_EXIT_OK;
}

/* ---- collections.Counter over strings: most_common order (count descending, then first appearance) ---- */
typedef struct {
    AvpMap *m;
    AvpStrVec keys;
    AvpI64Vec counts;
} Counter;

static void counter_add(Counter *c, AvpStr k) {
    if (!c->m) c->m = avp_map_new();
    bool created;
    AvpVal *v = avp_map_put(c->m, k, &created);
    if (created) {
        v->i = (i64)c->keys.n;
        avp_vec_push(&c->keys, k);
        avp_vec_push(&c->counts, 0);
    }
    c->counts.v[v->i]++;
}

static int cmp_count(const void *x, const void *y, void *ctx) {
    const i64 *counts = ctx;
    size_t a = *(const size_t *)x, b = *(const size_t *)y;
    if (counts[a] != counts[b]) return counts[a] > counts[b] ? -1 : 1;
    return 0;
}

/* most_common(n): indices into keys/counts */
static size_t *counter_most_common(AvpArena *a, const Counter *c, size_t n, size_t *got) {
    size_t k = c->keys.n;
    size_t *idx = avp_alloc(a, (k ? k : 1) * sizeof *idx);
    for (size_t i = 0; i < k; i++) idx[i] = i;
    avp_sort(idx, k, sizeof *idx, cmp_count, c->counts.v, false);
    *got = k < n ? k : n;
    return idx;
}

static void counter_free(Counter *c) {
    if (c->m) avp_map_free(c->m);
    avp_vec_free(&c->keys);
    avp_vec_free(&c->counts);
}

static AvpStr renamed_one(AvpArena *a, const FragRow *row) {
    FragRenamed *r = frag_rename_rows(a, row, 1);
    return avp_pyfmt(a, "%S %S", &r[0].mn, &r[0].ops);
}

static AvpStr context_line(AvpArena *a, const Counter *c) {
    size_t n;
    size_t *idx = counter_most_common(a, c, 3, &n);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        AvpStr k = avp_str_strip(c->keys.v[idx[i]]);
        if (i) avp_buf_putz(&b, "; ");
        avp_buf_pyfmt(&b, "%S (%d)", &k, c->counts.v[idx[i]]);
    }
    return avp_buf_str(&b);
}

static AvpJson *context_json(AvpArena *a, const Counter *c) {
    size_t n;
    size_t *idx = counter_most_common(a, c, 5, &n);
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *pair = avp_json_arr(a);
        avp_json_arr_push(a, pair, avp_json_str(a, c->keys.v[idx[i]]));
        avp_json_arr_push(a, pair, avp_json_int(a, c->counts.v[idx[i]]));
        avp_json_arr_push(a, arr, pair);
    }
    return arr;
}

static AvpStr cut(AvpStr s, i64 n) { return avp_u8_slice(s, 0, n); }

int frag_cmd_show(const FragArgs *args, Frags *fr, ShsmOutput *out) {
    AvpArena *a = fr->arena;
    Frag *f = frags_fragment(fr, args->id);
    Summary s = occ_summary(f->occ.v, f->occ.n);
    shsm_output_line(out, avp_pyfmt(a, "%s  %s level, %d rows, %d occurrence(s) in %d function(s), %d unit(s), %d matched",
                                    f->id, f->level, f->length, (i64)f->occ.n, s.nf, s.nu, s.nm));
    for (size_t i = 0; i < f->pattern.n; i++)
        shsm_output_line(out, avp_pyfmt(a, "  +%-3d %S", (i64)i, &f->pattern.v[i]));
    size_t nocc = f->occ.n, len = (size_t)f->length;
    FragRow *inner = avp_alloc(a, (nocc * len ? nocc * len : 1) * sizeof *inner);
    Counter prev = {0}, nxt = {0};
    for (size_t o = 0; o < nocc; o++) {
        size_t n;
        FragRow *rows = frags_occ_rows(fr, &f->occ.v[o], f->length, 1, 1, &n);
        if (len) memcpy(inner + o * len, rows + 1, len * sizeof *rows);
        counter_add(&prev, renamed_one(a, &rows[0]));
        counter_add(&nxt, renamed_one(a, &rows[n - 1]));
    }
    FragRender rr = frag_render(a, inner, nocc, len);
    AvpStr pl = context_line(a, &prev);
    shsm_output_line(out, avp_pyfmt(a, "preceded by: %S", &pl));
    AvpStr fl = context_line(a, &nxt);
    shsm_output_line(out, avp_pyfmt(a, "followed by: %S", &fl));
    AvpStr varying = AVP_S("");
    if (rr.cols.n) {
        AvpStrVec labels = {0};
        for (size_t i = 0; i < rr.cols.n && i < 8; i++) avp_vec_push(&labels, rr.cols.v[i].label);
        AvpStr j = avp_str_join(a, AVP_S(", "), labels.v, labels.n);
        varying = avp_pyfmt(a, "; varying operands: %S", &j);
        avp_vec_free(&labels);
    }
    shsm_output_line(out, avp_pyfmt(a, "occurrences (* matched; M Matching, N NonMatching, . asm)%S:", &varying));
    AvpStrVec lines = {0};
    for (size_t o = 0; o < nocc; o++) {
        const FragOccRec *x = &f->occ.v[o];
        const char *st = strcmp(x->status, "Matching") == 0 ? "M" : strcmp(x->status, "NonMatching") == 0 ? "N" : ".";
        AvpStr mark = avp_pyfmt(a, "%s%s", x->matched ? "*" : " ", st);
        AvpStr name = cut(avp_str(x->fn_name ? x->fn_name : ""), 40);
        AvpStr unit = cut(avp_str(x->unit ? x->unit : ""), 30);
        const AvpStrVec *vals = &rr.values[o];
        AvpStr v = avp_str_join(a, AVP_S(" | "), vals->v, vals->n < 8 ? vals->n : 8);
        avp_vec_push(&lines, avp_pyfmt(a, "  %S %08X +0x%-4X %-40S %-30S %S", &mark, x->address, x->address - x->fn_address,
                                       &name, &unit, &v));
    }
    shsm_output_lines(out, lines.v, lines.n);
    avp_vec_free(&lines);
    shsm_output_data(out, "fragment", frag_json(a, f, true));
    AvpJson *vj = avp_json_arr(a);
    for (size_t i = 0; i < rr.cols.n; i++) avp_json_arr_push(a, vj, avp_json_str(a, rr.cols.v[i].label));
    shsm_output_data(out, "varying", vj);
    AvpJson *values = avp_json_arr(a);
    for (size_t o = 0; o < nocc; o++) {
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("address"), avp_json_str(a, avp_pyfmt(a, "0x%08X", f->occ.v[o].address)));
        AvpJson *vv = avp_json_arr(a);
        for (size_t k = 0; k < rr.values[o].n; k++) avp_json_arr_push(a, vv, avp_json_str(a, rr.values[o].v[k]));
        avp_json_set(a, d, AVP_S("values"), vv);
        avp_json_arr_push(a, values, d);
    }
    shsm_output_data(out, "values", values);
    shsm_output_data(out, "preceded_by", context_json(a, &prev));
    shsm_output_data(out, "followed_by", context_json(a, &nxt));
    frag_render_free(&rr, nocc);
    counter_free(&prev);
    counter_free(&nxt);
    return SHSM_EXIT_OK;
}

static i64 count_query(sqlite3 *con, const char *q, i64 addr, const char *level) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(con, q, -1, &st, NULL) != SQLITE_OK) frag_sql_fail(con);
    int k = 1;
    if (addr != SHSM_NONE) sqlite3_bind_int64(st, k++, addr);
    if (level) sqlite3_bind_text(st, k++, level, -1, SQLITE_TRANSIENT);
    i64 n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

int frag_cmd_in(const FragArgs *args, Frags *fr, ShsmOutput *out) {
    AvpArena *a = fr->arena;
    ShsmSymRow sr;
    frags_resolve(fr, args->function, &sr);
    FragAts cands = frag_fragments_at(a, fr->con, sr.address, args->level, args->min_functions, args->overlaps);
    i64 nocc = count_query(fr->con,
                           args->level ? "select count(*) from occurrences o join fragments f on f.id = o.fragment"
                                         " where o.fn_address = ? and f.level = ?"
                                       : "select count(*) from occurrences o join fragments f on f.id = o.fragment"
                                         " where o.fn_address = ?",
                           sr.address, args->level);
    const char *unit = sr.unit.p ? avp_cz(a, sr.unit) : NULL;
    FragFnInfo fi = frags_fninfo(fr, sr.address, unit);
    i64 nrows = sr.nrows == SHSM_NONE ? 0 : sr.nrows;
    shsm_output_line(out, avp_pyfmt(a, "%S  %08X  %d rows  unit %s (%s)  %s; %d fragment occurrence(s)", &sr.name,
                                    sr.address, nrows, unit, fi.status, fi.matched ? "matched" : "unmatched", nocc));
    shsm_output_linez(out, "range                 id      len  fns  sequence");
    AvpJson *data = avp_json_arr(a);
    AvpStrVec lines = {0};
    for (size_t i = 0; i < cands.n; i++) {
        const Frag *f = cands.v[i].f;
        i64 off = cands.v[i].address - sr.address;
        AvpStr c = frag_compact(a, &f->pattern, 110);
        avp_vec_push(&lines, avp_pyfmt(a, "+0x%04X..+0x%04X  %-7s %4d %4d  %S", off, off + 4 * f->length, f->id,
                                       f->length, f->nfuncs, &c));
        AvpJson *d = frag_json(a, f, false);
        avp_json_set(a, d, AVP_S("offset"), avp_json_int(a, off));
        avp_json_arr_push(a, data, d);
    }
    shsm_output_lines(out, lines.v, lines.n);
    avp_vec_free(&lines);
    shsm_output_data(out, "function", avp_json_str(a, sr.name));
    shsm_output_data(out, "address", avp_json_str(a, avp_pyfmt(a, "0x%08X", sr.address)));
    shsm_output_data(out, "fragments", data);
    avp_vec_free(&cands);
    return SHSM_EXIT_OK;
}

int frag_cmd_stats(const FragArgs *args, Frags *fr, ShsmOutput *out) {
    (void)args;
    AvpArena *a = fr->arena;
    const char *rel = shsm_repo_rel(a, fr->repo, frag_db_path(a, fr->repo));
    shsm_output_line(out, avp_pyfmt(a, "fragments at %s: %s functions, %s rows; window %s rows, seeded at %s sites; scan %s s,"
                                       " search %s s",
                                    rel, frags_meta(fr, "functions"), frags_meta(fr, "rows"), frags_meta(fr, "window"),
                                    frags_meta(fr, "sites"), frags_meta(fr, "scan_seconds"),
                                    frags_meta(fr, "search_seconds")));
    AvpJson *levels = avp_json_obj(a);
    for (int lv = 0; lv < FRAG_NLEVELS; lv++) {
        const char *level = FRAG_LEVELS[lv];
        i64 n = count_query(fr->con, "select count(*) from fragments where level = ?", SHSM_NONE, level);
        i64 covered = count_query(fr->con, "select count(distinct fn_address) from occurrences o join fragments f"
                                           " on f.id = o.fragment where f.level = ?", SHSM_NONE, level);
        sqlite3_stmt *st;
        if (sqlite3_prepare_v2(fr->con, "select case when length < 8 then '<8' when length < 12 then '8-11'"
                                        " when length < 20 then '12-19' else '20+' end, count(*) from fragments"
                                        " where level = ? group by 1", -1, &st, NULL) != SQLITE_OK)
            frag_sql_fail(fr->con);
        sqlite3_bind_text(st, 1, level, -1, SQLITE_TRANSIENT);
        AvpStrVec keys = {0};
        AvpI64Vec counts = {0};
        AvpJson *hist = avp_json_obj(a);
        while (sqlite3_step(st) == SQLITE_ROW) {
            AvpStr k = avp_str_dup(a, avp_strn((const char *)sqlite3_column_text(st, 0), (size_t)sqlite3_column_bytes(st, 0)));
            i64 c = sqlite3_column_int64(st, 1);
            avp_vec_push(&keys, k);
            avp_vec_push(&counts, c);
            avp_json_set(a, hist, k, avp_json_int(a, c));
        }
        sqlite3_finalize(st);
        /* sorted(hist): by the key text (keys are distinct) */
        size_t *idx = avp_alloc(a, (keys.n ? keys.n : 1) * sizeof *idx);
        for (size_t i = 0; i < keys.n; i++) idx[i] = i;
        for (size_t i = 1; i < keys.n; i++)
            for (size_t j = i; j > 0 && avp_str_cmp(keys.v[idx[j - 1]], keys.v[idx[j]]) > 0; j--) {
                size_t t = idx[j];
                idx[j] = idx[j - 1];
                idx[j - 1] = t;
            }
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < keys.n; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_pyfmt(&b, "%S: %d", &keys.v[idx[i]], counts.v[idx[i]]);
        }
        AvpStr h = avp_buf_str(&b);
        shsm_output_line(out, avp_pyfmt(a, "%-6s %6d fragments, %6d functions contain one; by length: %S", level, n,
                                        covered, &h));
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("fragments"), avp_json_int(a, n));
        avp_json_set(a, d, AVP_S("functions"), avp_json_int(a, covered));
        avp_json_set(a, d, AVP_S("by_length"), hist);
        avp_json_set(a, levels, avp_str(level), d);
        avp_vec_free(&keys);
        avp_vec_free(&counts);
    }
    shsm_output_data(out, "meta", fr->meta);
    shsm_output_data(out, "levels", levels);
    return SHSM_EXIT_OK;
}

/* ---- known ---- */

typedef struct {
    i64 s, e;
} KSpan;

typedef struct {
    const char *fid;
    i64 rank, length, nfuncs;
    AVP_VEC(KSpan) spans;
} KFrag;

int frag_cmd_known(const FragArgs *args, Frags *fr, ShsmOutput *out) {
    (void)args;
    AvpArena *a = fr->arena;
    double t = avp_time();
    FragmentsKnownSites sites[FRAGMENTS_NKNOWN];
    fragments_known_sites(a, fr->repo, fr->ix, sites);
    double scan = avp_time() - t;
    shsm_output_line(out, avp_pyfmt(a, "independent site scan of every listing: %.1f s", scan));
    AvpJson *data = avp_json_arr(a);
    bool found_all = true;
    for (int h = 0; h < FRAGMENTS_NKNOWN; h++) {
        const FragmentsKnown *K = &FRAGMENTS_KNOWN[h];
        const FragmentsKnownSites *S = &sites[h];
        i64 early = 0;
        AvpSet *fnset = avp_set_new();
        for (size_t i = 0; i < S->n; i++) {
            if (S->v[i].offset < 0x20) early++;
            avp_set_add(fnset, S->v[i].fn);
        }
        i64 nfn = (i64)avp_set_len(fnset);
        avp_set_free(fnset);
        AvpJson *res = avp_json_obj(a);
        avp_json_set(a, res, AVP_S("helper"), avp_json_strz(a, K->name));
        avp_json_set(a, res, AVP_S("note"), avp_json_strz(a, K->note));
        avp_json_set(a, res, AVP_S("stated"), K->stated < 0 ? avp_json_null() : avp_json_int(a, K->stated));
        avp_json_set(a, res, AVP_S("scanned_sites"), avp_json_int(a, (i64)S->n));
        avp_json_set(a, res, AVP_S("scanned_functions"), avp_json_int(a, nfn));
        avp_json_set(a, res, AVP_S("in_first_8_rows"), avp_json_int(a, early));
        bool have_best = false;
        i64 best_hits = 0, best_rank = 0, b_length = 0, b_nfuncs = 0, b_nocc = 0;
        const char *b_fid = NULL;
        int b_lv = 0;
        AvpISet *anywhere = avp_iset_new();
        for (int lv = 0; lv < FRAG_NLEVELS; lv++) {
            sqlite3_stmt *st;
            if (sqlite3_prepare_v2(fr->con, "select f.id, f.rank, f.length, f.nfuncs, o.address, o.row from occurrences o"
                                            " join fragments f on f.id = o.fragment where f.level = ?", -1, &st,
                                   NULL) != SQLITE_OK)
                frag_sql_fail(fr->con);
            sqlite3_bind_text(st, 1, FRAG_LEVELS[lv], -1, SQLITE_TRANSIENT);
            AvpMap *byid = avp_map_new();
            AVP_VEC(KFrag) kf = {0};
            while (sqlite3_step(st) == SQLITE_ROW) {
                AvpStr id = avp_strn((const char *)sqlite3_column_text(st, 0), (size_t)sqlite3_column_bytes(st, 0));
                bool created;
                AvpVal *v = avp_map_put(byid, id, &created);
                if (created) {
                    v->i = (i64)kf.n;
                    avp_vec_push_zero(&kf);
                    kf.v[v->i].fid = avp_cz(a, id);
                }
                KFrag *k = &kf.v[v->i];
                k->rank = sqlite3_column_int64(st, 1);
                k->length = sqlite3_column_int64(st, 2);
                k->nfuncs = sqlite3_column_int64(st, 3);
                i64 addr = sqlite3_column_int64(st, 4);
                avp_vec_push(&k->spans, ((KSpan){addr, addr + 4 * k->length}));
            }
            sqlite3_finalize(st);
            for (size_t i = 0; i < kf.n; i++) {
                KFrag *k = &kf.v[i];
                AvpISet *hits = avp_iset_new();
                for (size_t x = 0; x < S->n; x++) {
                    i64 an = S->v[x].address;
                    for (size_t y = 0; y < k->spans.n; y++)
                        if (k->spans.v[y].s <= an && an < k->spans.v[y].e) {
                            avp_iset_add(hits, (u64)an);
                            break;
                        }
                }
                i64 nh = (i64)avp_iset_len(hits);
                if (nh) {
                    size_t pos = 0;
                    u64 m;
                    while (avp_iset_next(hits, &pos, &m)) avp_iset_add(anywhere, m);
                    /* key (len(hits), -rank) > best key */
                    if (!have_best || nh > best_hits || (nh == best_hits && -k->rank > -best_rank)) {
                        have_best = true;
                        best_hits = nh;
                        best_rank = k->rank;
                        b_fid = k->fid;
                        b_lv = lv;
                        b_length = k->length;
                        b_nfuncs = k->nfuncs;
                        b_nocc = (i64)k->spans.n;
                    }
                }
                avp_iset_free(hits);
                avp_vec_free(&k->spans);
            }
            avp_vec_free(&kf);
            avp_map_free(byid);
        }
        shsm_output_linez(out, "");
        AvpStr stated = K->stated < 0 ? AVP_S("") : avp_pyfmt(a, ", %d stated", K->stated);
        shsm_output_line(out, avp_pyfmt(a, "%s: %d site(s) in %d function(s) by scan (%d in the first 8 rows)%S  [%s]",
                                        K->name, (i64)S->n, nfn, early, &stated, K->note));
        if (!have_best) {
            found_all = false;
            shsm_output_linez(out, "  NOT FOUND: no fragment covers any scanned site");
        } else {
            const char *lv = FRAG_LEVELS[b_lv];
            i64 total = count_query(fr->con, "select count(*) from fragments where level = ?", SHSM_NONE, lv);
            Frag *f = frags_fragment(fr, b_fid);
            shsm_output_line(out, avp_pyfmt(a, "  best fragment %s (%s rank %d of %d): %d rows, %d occurrence(s) in %d"
                                               " function(s); covers %d of %d scanned site(s)",
                                            b_fid, lv, best_rank, total, b_length, b_nocc, b_nfuncs, best_hits, (i64)S->n));
            AvpStr c = frag_compact(a, &f->pattern, 140);
            shsm_output_line(out, avp_pyfmt(a, "  %S", &c));
            avp_json_set(a, res, AVP_S("fragment"), avp_json_strz(a, b_fid));
            avp_json_set(a, res, AVP_S("level"), avp_json_strz(a, lv));
            avp_json_set(a, res, AVP_S("rank"), avp_json_int(a, best_rank));
            avp_json_set(a, res, AVP_S("of"), avp_json_int(a, total));
            avp_json_set(a, res, AVP_S("length"), avp_json_int(a, b_length));
            avp_json_set(a, res, AVP_S("occurrences"), avp_json_int(a, b_nocc));
            avp_json_set(a, res, AVP_S("functions"), avp_json_int(a, b_nfuncs));
            avp_json_set(a, res, AVP_S("covered"), avp_json_int(a, best_hits));
            AvpI64Vec missed = {0};
            for (size_t x = 0; x < S->n; x++) {
                i64 an = S->v[x].address;
                bool in = false;
                for (size_t o = 0; o < f->occ.n && !in; o++)
                    if (f->occ.v[o].address <= an && an < f->occ.v[o].address + 4 * b_length) in = true;
                if (!in) avp_vec_push(&missed, an);
            }
            i64 extra = (i64)f->occ.n - best_hits;
            if (missed.n) {
                AvpBuf b;
                avp_buf_init(&b, a);
                avp_buf_putz(&b, "  missed by it: ");
                for (size_t i = 0; i < missed.n && i < 8; i++) avp_buf_pyfmt(&b, i ? ", %08X" : "%08X", missed.v[i]);
                if (missed.n > 8) avp_buf_putz(&b, " ...");
                shsm_output_line(out, avp_buf_str(&b));
            }
            avp_vec_free(&missed);
            if (extra > 0)
                shsm_output_line(out, avp_pyfmt(a, "  %d occurrence(s) outside the scanned sites (other spellings or"
                                                   " coincidence)", extra));
            shsm_output_line(out, avp_pyfmt(a, "  any fragment (either level) covers %d of %d scanned site(s)",
                                            (i64)avp_iset_len(anywhere), (i64)S->n));
            avp_json_set(a, res, AVP_S("covered_by_any"), avp_json_int(a, (i64)avp_iset_len(anywhere)));
        }
        avp_iset_free(anywhere);
        avp_json_arr_push(a, data, res);
    }
    for (int h = 0; h < FRAGMENTS_NKNOWN; h++) avp_vec_free(&sites[h]);
    shsm_output_data(out, "known", data);
    shsm_output_data(out, "scan_seconds", avp_json_float(a, avp_py_round(scan, 2)));
    return found_all ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
}
