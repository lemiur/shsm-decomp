/* The analysis (History) and the read commands: status and show. */
#include "sh_priv.h"

#include <string.h>

#define EPS 0.005

typedef struct {
    i64 id, time;
    AvpStr key, kind, subject, env, name;
} Snap;

typedef struct {
    i64 snap;
    AvpStr symbol;
    i64 addr;
    AvpStr unit, path;
    double score;
    i64 size;
    AvpStr hash, basis, unit_hash;
    i64 lo, hi;
    size_t pos;
} HRow;

typedef struct {
    i64 addr;
    HRow **rows;
    size_t n, cap;
} Group;

typedef struct {
    Snap *snaps;
    size_t nsnaps;     /* in ORDER BY time, id */
    AvpIMap *snap_pos; /* id -> position */
    Group *groups;     /* by_addr, in first-seen order */
    size_t ngroups;
    AvpIMap *by_addr; /* addr -> group index */
} History;

/* One analysed function (History.analyse). */
typedef struct {
    char address[24];
    const HRow *cur, *hist_row, *prev;
    double best, hist;
    bool old, lost;
    const char *regression;
    i64 snapshots;
} Fn;

static AvpStr col_str(AvpArena *a, sqlite3_stmt *st, int i) {
    const unsigned char *p = sqlite3_column_text(st, i);
    int n = sqlite3_column_bytes(st, i);
    return avp_str_dup(a, avp_strn(p ? (const char *)p : "", (size_t)n));
}

static AvpStr snap_name(AvpArena *a, const Snap *s) {
    if (avp_str_eqz(s->kind, "git")) return avp_str_slice(s->key, 0, 9);
    AvpDateTime dt;
    AvpStr t = AVP_S("?");
    if (avp_datetime_fromtimestamp((double)s->time, false, &dt)) avp_datetime_strftime(a, &dt, "%Y-%m-%d %H:%M", &t);
    return avp_str_concat(a, AVP_S("worktree@"), t);
}

static const Snap *snap_of(const History *h, i64 id, size_t *pos) {
    AvpVal *v = avp_imap_get(h->snap_pos, (u64)id);
    if (!v) avp_throw(SH_EXC_OTHER, "KeyError: %lld", id);
    if (pos) *pos = (size_t)v->i;
    return &h->snaps[v->i];
}

static int row_pos_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const HRow *a = *(HRow *const *)x, *b = *(HRow *const *)y;
    return a->pos < b->pos ? -1 : a->pos > b->pos ? 1 : 0;
}

static void history_load(ShCtx *c, History *h) {
    AvpArena *a = c->a;
    memset(h, 0, sizeof *h);
    h->snap_pos = avp_imap_new();
    AVP_VEC(Snap) snaps = {0};
    sqlite3_stmt *st = sh_prepare(c, "SELECT id, key, kind, time, subject, env_sig FROM snapshots ORDER BY time, id");
    while (sqlite3_step(st) == SQLITE_ROW) {
        Snap s = {0};
        s.id = sqlite3_column_int64(st, 0);
        s.key = col_str(a, st, 1);
        s.kind = col_str(a, st, 2);
        s.time = sqlite3_column_int64(st, 3);
        s.subject = col_str(a, st, 4);
        s.env = col_str(a, st, 5);
        s.name = snap_name(a, &s);
        bool created;
        avp_imap_put(h->snap_pos, (u64)s.id, &created)->i = (i64)snaps.n;
        avp_vec_push(&snaps, s);
    }
    sqlite3_finalize(st);
    h->snaps = avp_alloc(a, (snaps.n ? snaps.n : 1) * sizeof(Snap));
    if (snaps.n) memcpy(h->snaps, snaps.v, snaps.n * sizeof(Snap));
    h->nsnaps = snaps.n;
    avp_vec_free(&snaps);
    h->by_addr = avp_imap_new();
    AVP_VEC(Group) groups = {0};
    st = sh_prepare(c, "SELECT snap, symbol, addr, unit, path, score, size, hash, basis, unit_hash, line_lo, line_hi FROM rows");
    /* interned strings: units, paths and hashes repeat across snapshots */
    AvpMap *intern = avp_map_new();
    while (sqlite3_step(st) == SQLITE_ROW) {
        HRow *r = AVP_NEW(a, HRow);
        r->snap = sqlite3_column_int64(st, 0);
        AvpStr *fields[] = {&r->symbol, NULL, &r->unit, &r->path, NULL, NULL, &r->hash, &r->basis, &r->unit_hash};
        for (int i = 1; i <= 9; i++) {
            if (!fields[i - 1]) continue;
            const unsigned char *p = sqlite3_column_text(st, i);
            AvpStr s = avp_strn(p ? (const char *)p : "", (size_t)sqlite3_column_bytes(st, i));
            AvpVal *v = avp_map_get(intern, s);
            if (!v) {
                /* the map keeps the key view: key it by the copy */
                AvpStr *d = AVP_NEW(a, AvpStr);
                *d = avp_str_dup(a, s);
                bool created;
                v = avp_map_put(intern, *d, &created);
                v->p = d;
            }
            *fields[i - 1] = *(AvpStr *)v->p;
        }
        r->addr = sqlite3_column_int64(st, 2);
        r->score = sqlite3_column_double(st, 5);
        r->size = sqlite3_column_int64(st, 6);
        r->lo = sqlite3_column_int64(st, 10);
        r->hi = sqlite3_column_int64(st, 11);
        snap_of(h, r->snap, &r->pos);
        bool created;
        AvpVal *gv = avp_imap_put(h->by_addr, (u64)r->addr, &created);
        if (created) {
            gv->i = (i64)groups.n;
            avp_vec_push(&groups, ((Group){r->addr, NULL, 0, 0}));
        }
        Group *g = &groups.v[gv->i];
        if (g->n == g->cap) {
            size_t nc = g->cap ? g->cap * 2 : 8;
            g->rows = avp_arena_realloc(a, g->rows, g->cap * sizeof(HRow *), nc * sizeof(HRow *));
            g->cap = nc;
        }
        g->rows[g->n++] = r;
    }
    sqlite3_finalize(st);
    avp_map_free(intern);
    h->groups = avp_alloc(a, (groups.n ? groups.n : 1) * sizeof(Group));
    if (groups.n) memcpy(h->groups, groups.v, groups.n * sizeof(Group));
    h->ngroups = groups.n;
    avp_vec_free(&groups);
    for (size_t i = 0; i < h->ngroups; i++)
        avp_sort(h->groups[i].rows, h->groups[i].n, sizeof(HRow *), row_pos_cmp, NULL, false);
}

static void exit_msg(AvpArena *a, AvpStr msg) { avp_throw(SH_EXC_EXIT, "%.*s", AVP_SF(msg)); (void)a; }

/* find_snap(spec): the position of the last snapshot whose key or name starts with spec */
static size_t find_snap(AvpArena *a, const History *h, const char *spec) {
    AvpStr sp = avp_str(spec);
    i64 hit = -1;
    for (size_t i = 0; i < h->nsnaps; i++)
        if (avp_str_startswith(h->snaps[i].key, sp) || avp_str_startswith(h->snaps[i].name, sp)) hit = (i64)i;
    if (hit < 0) exit_msg(a, avp_pyfmt(a, "no snapshot matches %r", spec));
    return (size_t)hit;
}

static AvpStr env_get(AvpStr env, AvpStr key, bool *found);

/* dict(kv.split("=", 1) for kv in env.split(";") if "=" in kv) equality */
static bool env_equal(AvpStr x, AvpStr y) {
    AvpStrVec px = avp_str_split(x, AVP_S(";"), -1), py = avp_str_split(y, AVP_S(";"), -1);
    /* both dicts: last value per key wins; compare key sets and values */
    bool eq = true;
    for (int pass = 0; pass < 2 && eq; pass++) {
        AvpStrVec *ps = pass ? &py : &px;
        AvpStr other = pass ? x : y, self = pass ? y : x;
        for (size_t i = 0; i < ps->n && eq; i++) {
            AvpStr k, v;
            if (!avp_str_partition(ps->v[i], AVP_S("="), &k, &v)) continue;
            bool f1, f2;
            AvpStr v1 = env_get(self, k, &f1), v2 = env_get(other, k, &f2);
            if (!f2 || !avp_str_eq(v1, v2)) eq = false;
        }
    }
    avp_vec_free(&px);
    avp_vec_free(&py);
    return eq;
}

static AvpStr env_get(AvpStr env, AvpStr key, bool *found) {
    AvpStrVec parts = avp_str_split(env, AVP_S(";"), -1);
    AvpStr val = {"", 0};
    *found = false;
    for (size_t i = 0; i < parts.n; i++) {
        AvpStr k, v;
        if (!avp_str_partition(parts.v[i], AVP_S("="), &k, &v)) continue;
        if (avp_str_eq(k, key)) {
            val = v;
            *found = true;
        }
    }
    avp_vec_free(&parts);
    return val;
}

static const char *classify(const History *h, const HRow *prev, const HRow *cur) {
    if (!avp_str_eq(prev->hash, cur->hash)) {
        if (avp_str_eqz(cur->basis, "unit") || avp_str_eqz(cur->basis, "sinit")) return "unit changed (function not located)";
        return "source changed";
    }
    if (!avp_str_eq(prev->unit_hash, cur->unit_hash)) return "unit changed elsewhere";
    if (!env_equal(h->snaps[prev->pos].env, h->snaps[cur->pos].env)) return "headers/flags changed";
    return "unknown";
}

static int fn_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Fn *a = x, *b = y;
    int r = avp_str_cmp(a->cur->unit, b->cur->unit);
    return r ? r : strcmp(a->address, b->address);
}

static void fmt_address(char out[24], i64 addr) {
    AvpStr s = avp_pyfmt(avp_scratch(), "0x%08X", addr);
    memcpy(out, s.p, s.n < 23 ? s.n : 23);
    out[s.n < 23 ? s.n : 23] = 0;
}

/* analyse(vs): one entry per tracked function, sorted by (unit, address) */
static Fn *analyse(AvpArena *a, const History *h, const char *vs, size_t *nout) {
    i64 vs_pos = vs ? (i64)find_snap(a, h, vs) : -1;
    Fn *out = avp_alloc(a, (h->ngroups ? h->ngroups : 1) * sizeof(Fn));
    size_t latest = h->nsnaps ? h->nsnaps - 1 : 0;
    for (size_t g = 0; g < h->ngroups; g++) {
        HRow **lst = h->groups[g].rows;
        size_t n = h->groups[g].n;
        Fn *d = &out[g];
        const HRow *cur = lst[n - 1];
        double best = -1e308, hist = -1e308;
        bool bf = false;
        for (size_t i = 0; i < n; i++) {
            if (avp_str_eq(lst[i]->hash, cur->hash) && (!bf || lst[i]->score > best)) best = lst[i]->score, bf = true;
            if (i == 0 || lst[i]->score > hist) hist = lst[i]->score;
        }
        const HRow *hist_row = NULL;
        for (size_t i = 0; i < n; i++)
            if (lst[i]->score >= hist - EPS) hist_row = lst[i];
        const HRow *prev = NULL;
        if (vs_pos < 0) {
            prev = n > 1 ? lst[n - 2] : NULL;
        } else {
            for (size_t i = 0; i + 1 < n; i++)
                if ((i64)lst[i]->pos <= vs_pos) prev = lst[i];
        }
        fmt_address(d->address, h->groups[g].addr);
        d->cur = cur;
        d->hist_row = hist_row ? hist_row : cur;
        d->prev = prev;
        d->best = best;
        d->hist = hist;
        d->old = cur->pos != latest;
        d->lost = hist > best + EPS;
        d->snapshots = (i64)n;
        d->regression = NULL;
        if (prev && cur->score < prev->score - EPS) d->regression = classify(h, prev, cur);
    }
    avp_sort(out, h->ngroups, sizeof(Fn), fn_cmp, NULL, false);
    *nout = h->ngroups;
    return out;
}

/* ---- output ---------------------------------------------------------------------------------------------------- */

static void json_fn(AvpJsonW *w, const History *h, const Fn *d) {
    const Snap *hs = &h->snaps[d->hist_row->pos];
    avp_jsonw_begin_object(w);
    avp_jsonw_kv_str(w, "address", avp_str(d->address));
    avp_jsonw_kv_str(w, "symbol", d->cur->symbol);
    avp_jsonw_kv_str(w, "unit", d->cur->unit);
    avp_jsonw_kv_str(w, "path", d->cur->path);
    avp_jsonw_kv_str(w, "basis", d->cur->basis);
    avp_jsonw_kv_i64(w, "size", d->cur->size);
    avp_jsonw_kv_f64(w, "current", d->cur->score);
    avp_jsonw_kv_f64(w, "best", d->best);
    avp_jsonw_kv_f64(w, "historical", d->hist);
    avp_jsonw_kv_str(w, "historical_rev", hs->name);
    avp_jsonw_kv_str(w, "historical_key", hs->key);
    avp_jsonw_kv_str(w, "historical_kind", hs->kind);
    avp_jsonw_kv_str(w, "historical_path", d->hist_row->path);
    avp_jsonw_key(w, AVP_S("historical_lines"));
    avp_jsonw_begin_array(w);
    avp_jsonw_i64(w, d->hist_row->lo);
    avp_jsonw_i64(w, d->hist_row->hi);
    avp_jsonw_end_array(w);
    avp_jsonw_kv_str(w, "historical_hash", d->hist_row->hash);
    avp_jsonw_kv_str(w, "historical_basis", d->hist_row->basis);
    avp_jsonw_kv_str(w, "snapshot", h->snaps[d->cur->pos].name);
    avp_jsonw_kv_bool(w, "old", d->old);
    avp_jsonw_kv_bool(w, "lost", d->lost);
    avp_jsonw_key(w, AVP_S("previous"));
    if (d->prev) avp_jsonw_f64(w, d->prev->score);
    else avp_jsonw_null(w);
    avp_jsonw_key(w, AVP_S("previous_snapshot"));
    if (d->prev) avp_jsonw_str(w, h->snaps[d->prev->pos].name);
    else avp_jsonw_null(w);
    avp_jsonw_key(w, AVP_S("regression"));
    if (d->regression) avp_jsonw_strz(w, d->regression);
    else avp_jsonw_null(w);
    avp_jsonw_kv_i64(w, "snapshots", d->snapshots);
    avp_jsonw_end_object(w);
}

static void fmt_score(AvpBuf *b, const double *x) {
    if (!x) avp_buf_putz(b, "   -  ");
    else avp_buf_pyfmt(b, "%6.2f", *x);
}

static void recovery_lines(AvpBuf *o, const History *h, const Fn *d) {
    const Snap *hs = &h->snaps[d->hist_row->pos];
    if (avp_str_eqz(hs->kind, "git")) {
        AvpStr k = avp_str_slice(hs->key, 0, 12);
        avp_buf_pyfmt(o, "    recover: git show %S:%S", &k, &d->hist_row->path);
        if (d->hist_row->lo) avp_buf_pyfmt(o, "   (lines %d-%d)", d->hist_row->lo, d->hist_row->hi);
        avp_buf_putc(o, '\n');
        return;
    }
    AvpStr hh = avp_str_slice(d->hist_row->hash, 0, 10);
    avp_buf_pyfmt(o, "    uncommitted version (%S): build/tools/score_history.exe show %s --source %S\n", &hs->name,
                  d->address, &hh);
}

static void print_table(AvpBuf *o, const History *h, Fn *const *rows, size_t n) {
    avp_buf_pyfmt(o, "%-44s %-12s %6s %6s %6s  %s\n", "unit", "address", "cur", "best", "hist", "symbol / flags");
    for (size_t i = 0; i < n; i++) {
        const Fn *d = rows[i];
        AvpBuf fl;
        avp_buf_init(&fl, avp_scratch());
        int nf = 0;
#define FLAG(...)                                    \
    do {                                             \
        if (nf++) avp_buf_putz(&fl, ", ");            \
        avp_buf_pyfmt(&fl, __VA_ARGS__);              \
    } while (0)
        if (d->lost) FLAG("LOST(%S)", &h->snaps[d->hist_row->pos].name);
        if (d->regression) FLAG("REG:%s", d->regression);
        if (d->old) FLAG("%s", "old");
        if (avp_str_eqz(d->cur->basis, "unit") || avp_str_eqz(d->cur->basis, "sinit")) FLAG("hash:%S", &d->cur->basis);
#undef FLAG
        AvpStr unit = avp_u8_slice(d->cur->unit, 0, 44);
        avp_buf_pyfmt(o, "%-44S %-12s ", &unit, d->address);
        fmt_score(o, &d->cur->score);
        avp_buf_putc(o, ' ');
        fmt_score(o, &d->best);
        avp_buf_putc(o, ' ');
        fmt_score(o, &d->hist);
        avp_buf_putz(o, "  ");
        avp_buf_put(o, d->cur->symbol);
        if (nf) {
            avp_buf_putz(o, "  [");
            avp_buf_put(o, avp_buf_str(&fl));
            avp_buf_putc(o, ']');
        }
        avp_buf_putc(o, '\n');
    }
}

static bool unit_matches(AvpStr unit, AvpStr u, AvpStr mu) {
    return avp_str_startswith(unit, u) || avp_str_startswith(unit, mu) || avp_str_endswith(unit, u);
}

typedef struct {
    AvpStr key;
    i64 n;
} Count;

static int count_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(((const Count *)x)->key, ((const Count *)y)->key);
}

/* Counter(keys) sorted by key */
static Count *counted(AvpArena *a, const AvpStr *keys, size_t n, size_t *nout) {
    AvpMap *m = avp_map_new();
    Count *out = avp_alloc(a, (n ? n : 1) * sizeof(Count));
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        bool created;
        AvpVal *v = avp_map_put(m, keys[i], &created);
        if (created) {
            v->i = (i64)k;
            out[k++] = (Count){keys[i], 0};
        }
        out[v->i].n++;
    }
    avp_map_free(m);
    avp_sort(out, k, sizeof(Count), count_cmp, NULL, false);
    *nout = k;
    return out;
}

int sh_cmd_status(ShCtx *c, const ShStatusOpts *o) {
    AvpArena *a = c->a;
    AvpBuf *out = c->out;
    History h;
    history_load(c, &h);
    if (!h.nsnaps) {
        avp_buf_putz(out, "no snapshots recorded (run backfill / record)\n");
        return 0;
    }
    size_t nall;
    Fn *all = analyse(a, &h, o->vs, &nall);
    Fn **rows = avp_alloc(a, (nall ? nall : 1) * sizeof(Fn *));
    size_t n = 0;
    for (size_t i = 0; i < nall; i++) rows[n++] = &all[i];
    if (o->unit) {
        AvpStr u = avp_str(o->unit), mu = avp_str(avp_sprintf(a, "main/%s", o->unit));
        size_t m = 0;
        for (size_t i = 0; i < n; i++)
            if (avp_str_eq(rows[i]->cur->unit, u) || avp_str_eq(rows[i]->cur->unit, mu)) rows[m++] = rows[i];
        if (m == 0)
            for (size_t i = 0; i < n; i++)
                if (unit_matches(rows[i]->cur->unit, u, mu)) rows[m++] = rows[i];
        n = m;
    }
    if (o->lost) {
        size_t m = 0;
        for (size_t i = 0; i < n; i++)
            if (rows[i]->lost) rows[m++] = rows[i];
        n = m;
    }
    if (o->regressed) {
        size_t m = 0;
        for (size_t i = 0; i < n; i++)
            if (rows[i]->regression) rows[m++] = rows[i];
        n = m;
    }
    if (o->json) {
        AvpJsonDumpOpts jo = AVP_JSON_INDENT(1);
        AvpJsonW w;
        avp_jsonw_init(&w, out, &jo);
        avp_jsonw_begin_array(&w);
        for (size_t i = 0; i < n; i++) json_fn(&w, &h, rows[i]);
        avp_jsonw_end_array(&w);
        avp_jsonw_finish(&w);
        avp_buf_putc(out, '\n');
        return 0;
    }
    if (o->lost) {
        avp_buf_pyfmt(out, "%d lost match(es): historical > best (current source never reached it)\n", (i64)n);
        print_table(out, &h, rows, n);
        for (size_t i = 0; i < n; i++) {
            const Fn *d = rows[i];
            const Snap *hs = &h.snaps[d->hist_row->pos];
            avp_buf_pyfmt(out, "\n%S %S: best %.2f, historical %.2f at %S (basis %S)\n", &d->cur->unit, &d->cur->symbol,
                          d->best, d->hist, &hs->name, &d->hist_row->basis);
            recovery_lines(out, &h, d);
        }
        return 0;
    }
    if (o->regressed) {
        static const char *const kinds[] = {"changed source", "unchanged source", "unlocated"};
        static const char *const notes[] = {
            "the function's own text differs",
            "same function text; not a regression when caused by codegen elsewhere in the unit (AGENTS.md)",
            "function not found in the source (hashed by the whole file) and the file changed"};
        avp_buf_pyfmt(out, "%d function(s) scoring below the previous snapshot", (i64)n);
        if (o->vs) avp_buf_pyfmt(out, " (vs %s)", o->vs);
        avp_buf_putc(out, '\n');
        Fn **grp = avp_alloc(a, (n ? n : 1) * sizeof(Fn *));
        for (int k = 0; k < 3; k++) {
            size_t m = 0;
            for (size_t i = 0; i < n; i++) {
                const char *r = rows[i]->regression;
                int kind = !strcmp(r, "source changed") ? 0 : !strncmp(r, "unit changed (function", 22) ? 2 : 1;
                if (kind == k) grp[m++] = rows[i];
            }
            avp_buf_pyfmt(out, "\n== %s: %d (%s)\n", kinds[k], (i64)m, notes[k]);
            print_table(out, &h, grp, m);
            for (size_t i = 0; i < m; i++) {
                const Fn *d = grp[i];
                avp_buf_pyfmt(out, "    %S: %.2f -> %.2f  (%S -> %S)\n", &d->cur->symbol, d->prev->score, d->cur->score,
                              &h.snaps[d->prev->pos].name, &h.snaps[d->cur->pos].name);
            }
        }
        return 0;
    }
    if (o->unit || o->all) {
        print_table(out, &h, rows, n);
        return 0;
    }
    /* the summary */
    AvpStr *kk = avp_alloc(a, h.nsnaps * sizeof(AvpStr));
    for (size_t i = 0; i < h.nsnaps; i++) kk[i] = h.snaps[i].kind;
    size_t nk;
    Count *kinds = counted(a, kk, h.nsnaps, &nk);
    avp_buf_pyfmt(out, "snapshots: %d (", (i64)h.nsnaps);
    for (size_t i = 0; i < nk; i++) {
        if (i) avp_buf_putz(out, ", ");
        avp_buf_pyfmt(out, "%d %S", kinds[i].n, &kinds[i].key);
    }
    avp_buf_pyfmt(out, "), %S .. %S\n", &h.snaps[0].name, &h.snaps[h.nsnaps - 1].name);
    i64 full = 0, zero = 0, nlost = 0, nreg = 0, nchanged = 0;
    AvpStr *lost_units = avp_alloc(a, (n ? n : 1) * sizeof(AvpStr));
    AvpStr *reg_units = avp_alloc(a, (n ? n : 1) * sizeof(AvpStr));
    for (size_t i = 0; i < n; i++) {
        const Fn *d = rows[i];
        if (d->cur->score >= 100 - EPS) full++;
        if (d->cur->score <= EPS) zero++;
        if (d->lost) lost_units[nlost++] = d->cur->unit;
        if (d->regression) {
            reg_units[nreg++] = d->cur->unit;
            if (!strcmp(d->regression, "source changed")) nchanged++;
        }
    }
    avp_buf_pyfmt(out, "functions tracked: %d, current 100%%: %d, current 0%%: %d\n", (i64)n, full, zero);
    avp_buf_pyfmt(out, "lost matches (historical > best): %d\n", nlost);
    avp_buf_pyfmt(out, "below previous snapshot: %d (changed source %d, unchanged source %d)\n", nreg, nchanged,
                  nreg - nchanged);
    const char *titles[] = {"lost", "regressed"};
    AvpStr *lists[] = {lost_units, reg_units};
    i64 counts[] = {nlost, nreg};
    for (int t = 0; t < 2; t++) {
        size_t np;
        Count *per = counted(a, lists[t], (size_t)counts[t], &np);
        for (size_t i = 0; i < np; i++) avp_buf_pyfmt(out, "  %-9s %-60S %d\n", titles[t], &per[i].key, per[i].n);
    }
    return 0;
}

/* resolve_symbol: the group index */
static size_t resolve_symbol(AvpArena *a, const History *h, const char *spec) {
    AvpStr s = avp_str(spec);
    /* ^(?:0x)?([0-9A-Fa-f]{8})$ */
    AvpStr body = s;
    if (body.n && body.p[body.n - 1] == '\n') body.n--;
    if (avp_str_startswith(body, AVP_S("0x")) && body.n == 10) body = avp_str_slice(body, 2, AVP_END);
    if (body.n == 8) {
        bool hex = true;
        u64 v = 0;
        for (size_t i = 0; i < 8; i++) {
            char ch = body.p[i];
            int d = (ch >= '0' && ch <= '9') ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10
                    : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
            if (d < 0) hex = false;
            else v = v * 16 + (u64)d;
        }
        if (hex) {
            AvpVal *g = avp_imap_get(h->by_addr, v);
            if (g) return (size_t)g->i;
        }
    }
    AVP_VEC(size_t) hits = {0};
    for (size_t g = 0; g < h->ngroups; g++)
        for (size_t i = 0; i < h->groups[g].n; i++)
            if (avp_str_eq(h->groups[g].rows[i]->symbol, s)) {
                avp_vec_push(&hits, g);
                break;
            }
    if (!hits.n)
        for (size_t g = 0; g < h->ngroups; g++)
            if (avp_str_startswith(h->groups[g].rows[h->groups[g].n - 1]->symbol, s)) avp_vec_push(&hits, g);
    if (hits.n != 1) {
        AvpStr why = hits.n ? avp_pyfmt(a, "ambiguous (%d matches)", (i64)hits.n) : AVP_S("no such function");
        exit_msg(a, avp_pyfmt(a, "%s: %S", spec, &why));
    }
    size_t r = hits.v[0];
    avp_vec_free(&hits);
    return r;
}

int sh_cmd_show(ShCtx *c, const char *symbol, bool source_given, const char *source, bool json) {
    AvpArena *a = c->a;
    AvpBuf *out = c->out;
    History h;
    history_load(c, &h);
    size_t g = resolve_symbol(a, &h, symbol);
    HRow **lst = h.groups[g].rows;
    size_t n = h.groups[g].n;
    i64 addr = h.groups[g].addr;
    if (source_given) {
        AvpStr hash;
        if (!*source) {
            hash = lst[n - 1]->hash;
        } else {
            const HRow *last = NULL;
            for (size_t i = 0; i < n; i++)
                if (avp_str_startswith(lst[i]->hash, avp_str(source))) last = lst[i];
            if (!last) exit_msg(a, avp_pyfmt(a, "no version with hash prefix %s", source));
            hash = last->hash;
        }
        sqlite3_stmt *st = sh_prepare(c, "SELECT text FROM sources WHERE hash=?");
        sqlite3_bind_text(st, 1, hash.p, (int)hash.n, SQLITE_TRANSIENT);
        if (sqlite3_step(st) != SQLITE_ROW) {
            sqlite3_finalize(st);
            AvpStr h10 = avp_str_slice(hash, 0, 10);
            exit_msg(a, avp_pyfmt(a, "no stored source text for %S (basis unit/sinit hashes the whole file)", &h10));
        }
        AvpStr text = col_str(a, st, 0);
        sqlite3_finalize(st);
        avp_buf_put(out, text);
        avp_buf_putc(out, '\n');
        return 0;
    }
    size_t nall;
    Fn *all = analyse(a, &h, NULL, &nall);
    char want[24];
    fmt_address(want, addr);
    const Fn *d = NULL;
    for (size_t i = 0; i < nall && !d; i++)
        if (!strcmp(all[i].address, want)) d = &all[i];
    if (!d)
        exit_msg(a, avp_pyfmt(a, "%s: no current / best / historical analysis for %s (no row in any snapshot)", symbol, want));
    if (json) {
        AvpJsonDumpOpts jo = AVP_JSON_INDENT(1);
        AvpJsonW w;
        avp_jsonw_init(&w, out, &jo);
        avp_jsonw_begin_object(&w);
        avp_jsonw_key(&w, AVP_S("function"));
        json_fn(&w, &h, d);
        avp_jsonw_key(&w, AVP_S("timeline"));
        avp_jsonw_begin_array(&w);
        for (size_t i = 0; i < n; i++) {
            const HRow *r = lst[i];
            const Snap *s = &h.snaps[r->pos];
            avp_jsonw_begin_object(&w);
            avp_jsonw_kv_i64(&w, "snap", r->snap);
            avp_jsonw_kv_str(&w, "symbol", r->symbol);
            avp_jsonw_kv_i64(&w, "addr", r->addr);
            avp_jsonw_kv_str(&w, "unit", r->unit);
            avp_jsonw_kv_str(&w, "path", r->path);
            avp_jsonw_kv_f64(&w, "score", r->score);
            avp_jsonw_kv_i64(&w, "size", r->size);
            avp_jsonw_kv_str(&w, "hash", r->hash);
            avp_jsonw_kv_str(&w, "basis", r->basis);
            avp_jsonw_kv_str(&w, "unit_hash", r->unit_hash);
            avp_jsonw_kv_i64(&w, "line_lo", r->lo);
            avp_jsonw_kv_i64(&w, "line_hi", r->hi);
            avp_jsonw_kv_str(&w, "snapshot", s->name);
            avp_jsonw_kv_str(&w, "subject", s->subject);
            avp_jsonw_end_object(&w);
        }
        avp_jsonw_end_array(&w);
        avp_jsonw_end_object(&w);
        avp_jsonw_finish(&w);
        avp_buf_putc(out, '\n');
        return 0;
    }
    avp_buf_pyfmt(out, "%s  %S\n", want, &lst[n - 1]->symbol);
    avp_buf_pyfmt(out, "%-26s %-10s %6s %-10s %-6s  %s\n", "snapshot", "date", "score", "src hash", "basis",
                  "symbol / subject");
    const HRow *prev = NULL;
    for (size_t i = 0; i < n; i++) {
        const HRow *r = lst[i];
        const Snap *s = &h.snaps[r->pos];
        const char *mark = (!prev || avp_str_eq(prev->hash, r->hash)) ? " " : "*";
        AvpStr nm = avp_u8_slice(s->name, 0, 26), hh = avp_str_slice(r->hash, 0, 10),
               subj = avp_u8_slice(s->subject, 0, 60);
        char date[16] = "?";
        AvpDateTime dt;
        if (avp_datetime_fromtimestamp((double)s->time, false, &dt)) avp_date_isoformat(avp_datetime_date(&dt), date);
        avp_buf_pyfmt(out, "%-26S %-10s ", &nm, date);
        fmt_score(out, &r->score);
        avp_buf_pyfmt(out, " %-10S%s %-6S  %S  %S\n", &hh, mark, &r->basis, &r->symbol, &subj);
        prev = r;
    }
    avp_buf_putz(out, "(* = function source differs from the row above)\n");
    avp_buf_pyfmt(out, "current %.2f, best %.2f, historical %.2f at %S\n", d->cur->score, d->best, d->hist,
                  &h.snaps[d->hist_row->pos].name);
    if (d->lost) recovery_lines(out, &h, d);
    return 0;
}
