/* The witness database: tools/witness.py db_path, lookup, _stamp, _TABLES, build, save, load, Witness.__init__. */
#include "witness_db.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/hash.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

const char WT_TABLES_SQL[] =
    "\n"
    "create table if not exists meta(key text primary key, value text);\n"
    "create table if not exists fn(side text, address integer, name text, unit text, lib text, size integer,\n"
    "    n integer, frame integer, back integer, primary key(side, address));\n"
    "create table if not exists calls(side text, caller integer, idx integer, site integer, callee integer);\n"
    "create table if not exists strs(side text, fn integer, text text);\n"
    "create table if not exists feat(side text, fn integer, kind text, value integer);\n"
    "create table if not exists runs(side text, run integer, pos integer, kind text, value text);\n"
    "create table if not exists pairs(wii integer primary key, ps2 integer, tier text, evidence text);\n"
    "create table if not exists inl(wii integer, helper integer, parent integer, evidence text);\n";

const char *wt_db_path(AvpArena *a, const ShsmRepo *r) { return avp_path_join(a, r->index_dir, "witness.sqlite"); }

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static void exec(sqlite3 *db, const char *sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) sql_fail(db);
}

static sqlite3_stmt *prep(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    return st;
}

static int step(sqlite3 *db, sqlite3_stmt *st) {
    int rc = sqlite3_step(st);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        sqlite3_finalize(st);
        sql_fail(db);
    }
    return rc;
}

static i64 col_int(sqlite3_stmt *st, int c) {
    return sqlite3_column_type(st, c) == SQLITE_NULL ? WT_NONE : sqlite3_column_int64(st, c);
}

static AvpStr col_text(sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return (AvpStr){NULL, 0};
    const char *p = (const char *)sqlite3_column_text(st, c);
    return avp_strn(p, (size_t)sqlite3_column_bytes(st, c));
}

static const char *col_cz(AvpArena *a, sqlite3_stmt *st, int c) {
    AvpStr s = col_text(st, c);
    return s.p ? avp_cz(a, s) : NULL;
}

/* ---- lookup ---- */

bool wt_lookup_order(AvpArena *a, const ShsmRepo *r, WtOrderRank **out, size_t *n) {
    *out = NULL;
    *n = 0;
    const char *path = wt_db_path(a, r);
    if (!avp_exists(path)) return false;
    sqlite3 *con = NULL;
    if (sqlite3_open_v2(path, &con, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(con);
        return false;
    }
    AVP_VEC(WtOrderRank) rows = {0};
    sqlite3_stmt *st = NULL;
    bool ok = sqlite3_prepare_v2(con,
                                 "select p.wii, o.rk from pairs p join "
                                 "(select address, row_number() over (order by address) - 1 as rk from fn "
                                 "where side = 'ps2') o on o.address = p.ps2 order by p.wii",
                                 -1, &st, NULL) == SQLITE_OK;
    if (ok) {
        int rc;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
            WtOrderRank row = {sqlite3_column_int64(st, 0), sqlite3_column_int64(st, 1)};
            avp_vec_push(&rows, row);
        }
        ok = rc == SQLITE_DONE;
    }
    sqlite3_finalize(st);
    sqlite3_close(con);
    if (ok && rows.n) {
        *out = AVP_NEWN(a, WtOrderRank, rows.n);
        memcpy(*out, rows.v, rows.n * sizeof *rows.v);
        *n = rows.n;
    }
    avp_vec_free(&rows);
    return ok;
}

bool wt_lookup(AvpArena *a, const ShsmRepo *r, i64 address, int limit, WtLookup *out) {
    memset(out, 0, sizeof *out);
    const char *path = wt_db_path(a, r);
    if (!avp_exists(path)) return false;
    sqlite3 *con = NULL;
    if (sqlite3_open_v2(path, &con, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(con);
        return false;
    }
    bool ok = true, have_pair = false;
    typedef struct {
        i64 helper, size;
        const char *name;
    } Row;
    AVP_VEC(Row) rows = {0};
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(con, "select ps2, tier, evidence from pairs where wii = ?", -1, &st, NULL) != SQLITE_OK) {
        ok = false;
    } else {
        sqlite3_bind_int64(st, 1, address);
        int rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            have_pair = true;
            out->ps2 = col_int(st, 0);
            out->tier = col_cz(a, st, 1);
            out->evidence = col_cz(a, st, 2);
        } else if (rc != SQLITE_DONE) {
            ok = false;
        }
    }
    sqlite3_finalize(st);
    st = NULL;
    if (ok) {
        if (sqlite3_prepare_v2(con,
                               "select distinct i.helper, f.size, w.name from inl i "
                               "left join fn f on f.side = 'ps2' and f.address = i.helper "
                               "left join pairs p on p.ps2 = i.helper "
                               "left join fn w on w.side = 'wii' and w.address = p.wii "
                               "where i.wii = ? order by f.size desc, i.helper",
                               -1, &st, NULL) != SQLITE_OK) {
            ok = false;
        } else {
            sqlite3_bind_int64(st, 1, address);
            int rc;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
                Row row = {col_int(st, 0), col_int(st, 1), col_cz(a, st, 2)};
                avp_vec_push(&rows, row);
            }
            if (rc != SQLITE_DONE) ok = false;
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(con);
    if (!ok || (!have_pair && !rows.n)) {
        avp_vec_free(&rows);
        memset(out, 0, sizeof *out);
        return false;
    }
    if (!have_pair) {
        out->ps2 = WT_NONE;
        out->tier = NULL;
        out->evidence = NULL;
    }
    /* rows[:limit] */
    i64 n = (i64)rows.n, stop = limit >= 0 ? (limit < n ? limit : n) : (n + limit > 0 ? n + limit : 0);
    for (i64 k = 0; k < stop; k++) {
        WtLookupHelper h = {rows.v[k].helper, rows.v[k].size, rows.v[k].name};
        avp_vec_push(&out->helpers, h);
    }
    out->more = n - limit > 0 ? n - limit : 0;
    avp_vec_free(&rows);
    return true;
}

/* ---- _stamp ---- */

static void repr_value(AvpArena *a, AvpBuf *b, sqlite3_stmt *st, int col) {
    switch (sqlite3_column_type(st, col)) {
    case SQLITE_NULL: avp_buf_putz(b, "None"); break;
    case SQLITE_INTEGER: avp_buf_printf(b, "%lld", (long long)sqlite3_column_int64(st, col)); break;
    default: avp_buf_put(b, avp_repr_str(a, col_text(st, col)));
    }
}

const char *wt_stamp(AvpArena *a, ShsmAsmIndex *ix, WtExe exe) {
    sqlite3_stmt *st = prep(ix->db, "select path, size, mtime_ns from files order by path");
    AvpStat sb;
    if (!avp_stat(exe.path, &sb)) {
        sqlite3_finalize(st);
        shsm_raise_io();
    }
    AvpBuf b = {0};
    avp_buf_printf(&b, "%s|%s|%s|%lld|%lld|[", WT_SCHEMA, exe.path, exe.member ? exe.member : "None",
                   (long long)sb.size, (long long)sb.mtime_ns);
    int n = 0;
    while (step(ix->db, st) == SQLITE_ROW) {
        if (n++) avp_buf_putz(&b, ", ");
        avp_buf_putc(&b, '(');
        for (int c = 0; c < 3; c++) {
            if (c) avp_buf_putz(&b, ", ");
            repr_value(a, &b, st, c);
        }
        avp_buf_putc(&b, ')');
    }
    sqlite3_finalize(st);
    avp_buf_putc(&b, ']');
    AvpStr s = avp_buf_str(&b);
    char *hex = avp_sha1_hex(a, s.p, s.n);
    avp_buf_free(&b);
    return hex;
}

/* ---- build ---- */

static AvpJson *facts_json(AvpArena *a, const WtFacts *f) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("machine"), avp_json_int(a, f->machine));
    avp_json_set(a, o, AVP_S("entry"), avp_json_int(a, f->entry));
    avp_json_set(a, o, AVP_S("gp"), f->gp == WT_NONE ? avp_json_null() : avp_json_int(a, f->gp));
    avp_json_set(a, o, AVP_S("words"), avp_json_int(a, f->words));
    AvpJson *secs = avp_json_arr(a);
    for (size_t k = 0; k < f->sections.n; k++) {
        const WtSection *s = &f->sections.v[k];
        AvpJson *t = avp_json_arr(a);
        avp_json_arr_push(a, t, avp_json_strz(a, s->name));
        avp_json_arr_push(a, t, avp_json_int(a, s->addr));
        avp_json_arr_push(a, t, avp_json_int(a, s->size));
        avp_json_arr_push(a, t, avp_json_strz(a, s->kind));
        avp_json_arr_push(a, secs, t);
    }
    avp_json_set(a, o, AVP_S("sections"), secs);
    return o;
}

void wt_build(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix, WtExe exe, int jobs, WtBuilt *out) {
    memset(out, 0, sizeof *out);
    double t0 = avp_time();
    AvpBytes data = {0};
    const char *label = wt_load_exe(a, exe, &data);
    WtFacts facts;
    wt_build_ps2(a, data.v, data.n, &out->ps2, &out->pruns, &facts);
    double t1 = avp_time();
    wt_build_wii(a, r, ix, jobs, &out->wii, &out->wruns);
    double t2 = avp_time();
    out->c = wt_corr_run(wt_corr_new(&out->wii, &out->ps2, &out->wruns, &out->pruns, NULL), 30);
    double t3 = avp_time();
    wt_validate(&out->wii, &out->ps2, &out->wruns, &out->pruns, out->c->anchor_of, &out->val);
    double t4 = avp_time();
    out->inl = wt_corr_inlined_rows(out->c);
    AvpJson *meta = avp_json_obj(a);
    avp_json_set(a, meta, AVP_S("exe"), avp_json_strz(a, label));
    avp_json_set(a, meta, AVP_S("exe_size"), avp_json_int(a, (i64)data.n));
    avp_json_set(a, meta, AVP_S("facts"), facts_json(a, &facts));
    avp_json_set(a, meta, AVP_S("validation"), wt_validation_json(a, &out->val));
    avp_json_set(a, meta, AVP_S("rounds"), avp_json_int(a, out->c->rounds));
    AvpJson *secs = avp_json_obj(a);
    avp_json_set(a, secs, AVP_S("ps2"), avp_json_float(a, avp_py_round(t1 - t0, 2)));
    avp_json_set(a, secs, AVP_S("wii"), avp_json_float(a, avp_py_round(t2 - t1, 2)));
    avp_json_set(a, secs, AVP_S("map"), avp_json_float(a, avp_py_round(t3 - t2, 2)));
    avp_json_set(a, secs, AVP_S("validate"), avp_json_float(a, avp_py_round(t4 - t3, 2)));
    avp_json_set(a, meta, AVP_S("seconds"), secs);
    out->meta = meta;
}

/* ---- save ---- */

static void bind_opt_int(sqlite3_stmt *st, int k, i64 v) {
    if (v == WT_NONE)
        sqlite3_bind_null(st, k);
    else
        sqlite3_bind_int64(st, k, v);
}

static void bind_opt_text(sqlite3_stmt *st, int k, const char *s) {
    if (s)
        sqlite3_bind_text(st, k, s, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(st, k);
}

static void run_insert(sqlite3 *db, sqlite3_stmt *st) {
    if (sqlite3_step(st) != SQLITE_DONE) sql_fail(db);
    sqlite3_reset(st);
    sqlite3_clear_bindings(st);
}

static void save_side(sqlite3 *db, const char *side, const WtSide *fns, sqlite3_stmt *sfn, sqlite3_stmt *scalls,
                      sqlite3_stmt *sstrs, sqlite3_stmt *sfeat) {
    for (size_t k = 0; k < fns->v.n; k++) {
        const WtFn *f = fns->v.v[k];
        sqlite3_bind_text(sfn, 1, side, -1, SQLITE_STATIC);
        sqlite3_bind_int64(sfn, 2, f->addr);
        bind_opt_text(sfn, 3, f->name);
        bind_opt_text(sfn, 4, f->unit);
        bind_opt_text(sfn, 5, f->lib);
        sqlite3_bind_int64(sfn, 6, f->size);
        sqlite3_bind_int64(sfn, 7, f->n);
        sqlite3_bind_int64(sfn, 8, f->frame);
        sqlite3_bind_int64(sfn, 9, f->back);
        run_insert(db, sfn);
    }
    for (size_t k = 0; k < fns->v.n; k++) {
        const WtFn *f = fns->v.v[k];
        size_t n = f->sites.n < f->calls.n ? f->sites.n : f->calls.n;
        for (size_t j = 0; j < n; j++) {
            sqlite3_bind_text(scalls, 1, side, -1, SQLITE_STATIC);
            sqlite3_bind_int64(scalls, 2, f->addr);
            sqlite3_bind_int64(scalls, 3, (i64)j);
            bind_opt_int(scalls, 4, f->sites.v[j]);
            bind_opt_int(scalls, 5, f->calls.v[j]);
            run_insert(db, scalls);
        }
    }
    for (size_t k = 0; k < fns->v.n; k++) {
        const WtFn *f = fns->v.v[k];
        size_t pos = 0;
        AvpStr t;
        while (avp_set_next(f->strs, &pos, &t)) {
            sqlite3_bind_text(sstrs, 1, side, -1, SQLITE_STATIC);
            sqlite3_bind_int64(sstrs, 2, f->addr);
            sqlite3_bind_text(sstrs, 3, t.p ? t.p : "", (int)t.n, SQLITE_TRANSIENT);
            run_insert(db, sstrs);
        }
    }
    for (size_t k = 0; k < fns->v.n; k++) {
        const WtFn *f = fns->v.v[k];
        const AvpPyIntSet *sets[3] = {f->floats, f->ints, f->offs};
        static const char *const kinds[3] = {"f", "i", "o"};
        for (int s = 0; s < 3; s++) {
            size_t pos = 0;
            i64 v;
            while (avp_pyintset_next(sets[s], &pos, &v)) {
                sqlite3_bind_text(sfeat, 1, side, -1, SQLITE_STATIC);
                sqlite3_bind_int64(sfeat, 2, f->addr);
                sqlite3_bind_text(sfeat, 3, kinds[s], -1, SQLITE_STATIC);
                sqlite3_bind_int64(sfeat, 4, v);
                run_insert(db, sfeat);
            }
        }
    }
}

static void save_runs(sqlite3 *db, const char *side, const WtRuns *runs, sqlite3_stmt *st) {
    char kind[2] = {0, 0}, num[32];
    for (size_t k = 0; k < runs->n; k++)
        for (size_t i = 0; i < runs->v[k].n; i++) {
            const WtRunItem *it = &runs->v[k].v[i];
            kind[0] = it->kind;
            sqlite3_bind_text(st, 1, side, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, (i64)k);
            sqlite3_bind_int64(st, 3, (i64)i);
            sqlite3_bind_text(st, 4, kind, 1, SQLITE_TRANSIENT);
            if (it->kind == 'f') {
                snprintf(num, sizeof num, "%lld", (long long)it->fn);
                sqlite3_bind_text(st, 5, num, -1, SQLITE_TRANSIENT);
            } else if (it->kind == 's' && it->text) {
                sqlite3_bind_text(st, 5, it->text, -1, SQLITE_STATIC);
            } else {
                sqlite3_bind_null(st, 5);
            }
            run_insert(db, st);
        }
}

void wt_save(sqlite3 *db, const char *stamp, const WtBuilt *b) {
    AvpArena *a = avp_arena_new(0);
    exec(db, "BEGIN");
    AvpTry t;
    if (AVP_TRY(&t)) {
        static const char *const tables[] = {"meta", "fn", "calls", "strs", "feat", "runs", "pairs", "inl"};
        for (size_t k = 0; k < 8; k++) exec(db, avp_sprintf(a, "delete from %s", tables[k]));
        sqlite3_stmt *sfn = prep(db, "insert into fn values (?,?,?,?,?,?,?,?,?)");
        sqlite3_stmt *scalls = prep(db, "insert into calls values (?,?,?,?,?)");
        sqlite3_stmt *sstrs = prep(db, "insert into strs values (?,?,?)");
        sqlite3_stmt *sfeat = prep(db, "insert into feat values (?,?,?,?)");
        save_side(db, "wii", &b->wii, sfn, scalls, sstrs, sfeat);
        save_side(db, "ps2", &b->ps2, sfn, scalls, sstrs, sfeat);
        sqlite3_finalize(sfn);
        sqlite3_finalize(scalls);
        sqlite3_finalize(sstrs);
        sqlite3_finalize(sfeat);
        sqlite3_stmt *sruns = prep(db, "insert into runs values (?,?,?,?,?)");
        save_runs(db, "wii", &b->wruns, sruns);
        save_runs(db, "ps2", &b->pruns, sruns);
        sqlite3_finalize(sruns);
        sqlite3_stmt *sp = prep(db, "insert into pairs values (?,?,?,?)");
        for (size_t k = 0; k < b->c->pairs.n; k++) {
            const WtPairEntry *e = &b->c->pairs.v[k];
            sqlite3_bind_int64(sp, 1, e->wii);
            sqlite3_bind_int64(sp, 2, e->p.ps2);
            bind_opt_text(sp, 3, e->p.tier);
            bind_opt_text(sp, 4, e->p.ev);
            run_insert(db, sp);
        }
        sqlite3_finalize(sp);
        sqlite3_stmt *si = prep(db, "insert into inl values (?,?,?,?)");
        for (size_t k = 0; k < b->inl.n; k++) {
            const WtInl *e = &b->inl.v[k];
            bind_opt_int(si, 1, e->wii);
            bind_opt_int(si, 2, e->helper);
            bind_opt_int(si, 3, e->parent);
            bind_opt_text(si, 4, e->ev);
            run_insert(db, si);
        }
        sqlite3_finalize(si);
        sqlite3_stmt *sm = prep(db, "insert into meta values ('meta', ?)");
        AvpStr js = avp_json_dumps(a, b->meta, NULL);
        sqlite3_bind_text(sm, 1, js.p, (int)js.n, SQLITE_TRANSIENT);
        run_insert(db, sm);
        sqlite3_finalize(sm);
        sm = prep(db, "insert into meta values ('stamp', ?)");
        sqlite3_bind_text(sm, 1, stamp, -1, SQLITE_TRANSIENT);
        run_insert(db, sm);
        sqlite3_finalize(sm);
        exec(db, "COMMIT");
        avp_try_end(&t);
    } else {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        avp_arena_free(a);
        avp_throw(t.code, "%s", t.msg);
    }
    avp_arena_free(a);
}

/* ---- load ---- */

static WtSide *side_of(WtLoaded *m, AvpStr side) {
    if (avp_str_eqz(side, "wii")) return &m->wii;
    if (avp_str_eqz(side, "ps2")) return &m->ps2;
    shsm_raise(SHSM_EXC_KEY, "%.*s", (int)side.n, side.p ? side.p : "");
}

static WtFn *fn_of(WtSide *s, i64 a) {
    WtFn *f = wt_side_get(s, a);
    if (!f) shsm_raise(SHSM_EXC_KEY, "%lld", (long long)a);
    return f;
}

void wt_load(AvpArena *a, sqlite3 *db, WtLoaded *out) {
    memset(out, 0, sizeof *out);
    wt_side_init(&out->wii);
    wt_side_init(&out->ps2);
    sqlite3_stmt *st = prep(db, "select * from fn");
    while (step(db, st) == SQLITE_ROW) {
        WtFn *f = wt_fn_new(col_int(st, 1));
        f->name = col_cz(a, st, 2);
        f->unit = col_cz(a, st, 3);
        f->lib = col_cz(a, st, 4);
        f->size = col_int(st, 5);
        f->n = col_int(st, 6);
        f->frame = col_int(st, 7);
        f->back = col_int(st, 8);
        wt_side_put(side_of(out, col_text(st, 0)), f);
    }
    sqlite3_finalize(st);
    st = prep(db, "select * from calls order by side, caller, idx");
    while (step(db, st) == SQLITE_ROW) {
        WtFn *f = fn_of(side_of(out, col_text(st, 0)), col_int(st, 1));
        avp_vec_push(&f->calls, col_int(st, 4));
        avp_vec_push(&f->sites, col_int(st, 3));
    }
    sqlite3_finalize(st);
    st = prep(db, "select * from strs");
    while (step(db, st) == SQLITE_ROW) {
        WtFn *f = fn_of(side_of(out, col_text(st, 0)), col_int(st, 1));
        AvpStr t = col_text(st, 2);
        avp_set_add(f->strs, t.p ? t : AVP_S(""));
    }
    sqlite3_finalize(st);
    st = prep(db, "select * from feat");
    while (step(db, st) == SQLITE_ROW) {
        WtFn *f = fn_of(side_of(out, col_text(st, 0)), col_int(st, 1));
        AvpStr kind = col_text(st, 2);
        AvpPyIntSet *set = avp_str_eqz(kind, "f") ? f->floats : avp_str_eqz(kind, "i") ? f->ints
                                                     : avp_str_eqz(kind, "o")  ? f->offs
                                                                               : NULL;
        if (!set) shsm_raise(SHSM_EXC_KEY, "%.*s", (int)kind.n, kind.p ? kind.p : "");
        avp_pyintset_add(set, col_int(st, 3));
    }
    sqlite3_finalize(st);
    /* runs[side] is a defaultdict(list) keyed by run number: values in first-seen order */
    AvpIMap *ridx[2] = {avp_imap_new(), avp_imap_new()};
    st = prep(db, "select * from runs order by side, run, pos");
    while (step(db, st) == SQLITE_ROW) {
        AvpStr side = col_text(st, 0);
        int s = avp_str_eqz(side, "wii") ? 0 : avp_str_eqz(side, "ps2") ? 1 : -1;
        if (s < 0) shsm_raise(SHSM_EXC_KEY, "%.*s", (int)side.n, side.p ? side.p : "");
        WtRuns *runs = s ? &out->pruns : &out->wruns;
        bool created;
        AvpVal *v = avp_imap_put(ridx[s], (u64)col_int(st, 1), &created);
        if (created) {
            v->i = (i64)runs->n;
            WtRun empty = {0};
            avp_vec_push(runs, empty);
        }
        AvpStr kind = col_text(st, 3), val = col_text(st, 4);
        WtRunItem it = {kind.n ? kind.p[0] : 'o', 0, NULL};
        if (it.kind == 'f') {
            i64 x;
            if (!val.p || !avp_parse_int(avp_str_strip(val), 10, &x, NULL))
                shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 10");
            it.fn = x;
        } else if (val.p) {
            it.text = avp_cz(a, val);
        }
        avp_vec_push(&runs->v[v->i], it);
    }
    sqlite3_finalize(st);
    avp_imap_free(ridx[0]);
    avp_imap_free(ridx[1]);
    WtCorr *c = wt_corr_new(&out->wii, &out->ps2, &out->wruns, &out->pruns, NULL);
    st = prep(db, "select * from pairs");
    while (step(db, st) == SQLITE_ROW) {
        i64 w = col_int(st, 0), p = col_int(st, 1);
        const char *tier = col_cz(c->arena, st, 2), *ev = col_cz(c->arena, st, 3);
        bool created;
        AvpVal *v = avp_imap_put(c->pair_idx, (u64)w, &created);
        WtPairEntry e = {w, {p, tier, ev}};
        if (created) {
            v->i = (i64)c->pairs.n;
            avp_vec_push(&c->pairs, e);
        } else {
            c->pairs.v[v->i] = e;
        }
        avp_imap_put(c->rev, (u64)p, &created)->i = w;
    }
    sqlite3_finalize(st);
    st = prep(db, "select * from inl");
    while (step(db, st) == SQLITE_ROW) {
        WtInl r = {col_int(st, 0), col_int(st, 1), col_int(st, 2), col_cz(a, st, 3)};
        avp_vec_push(&out->inl, r);
        if (!(r.ev && strcmp(r.ev, "call set") == 0) && r.ev) avp_vec_push(&c->inl, r);
    }
    sqlite3_finalize(st);
    out->c = c;
    st = prep(db, "select value from meta where key = 'meta'");
    if (step(db, st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        shsm_raise(SHSM_EXC_TYPE, "'NoneType' object is not subscriptable");
    }
    AvpStr js = col_text(st, 0);
    AvpJsonError err;
    out->meta = avp_json_parse(a, js.p ? js : AVP_S(""), &err);
    sqlite3_finalize(st);
    if (!out->meta) shsm_raise(SHSM_EXC_VALUE, "%.*s", (int)err.msg.n, err.msg.p ? err.msg.p : "");
}

/* ---- Witness ---- */

WtWitness *wt_witness_open(const char *root, const char *exe, bool rebuild, int jobs) {
    WtWitness *w = avp_xcalloc(1, sizeof *w);
    w->arena = avp_arena_new(0);
    AvpArena *a = w->arena;
    w->repo = shsm_repo_find(root);
    w->exe = wt_find_exe(w->repo, exe);
    w->ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = w->repo, .readonly = true});
    if (!avp_makedirs(w->repo->index_dir, true)) shsm_raise_io();
    sqlite3 *con = NULL;
    const char *path = wt_db_path(a, w->repo);
    if (sqlite3_open_v2(path, &con, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        const char *msg = con ? sqlite3_errmsg(con) : "unable to open database file";
        shsm_raise(SHSM_EXC_SQLITE, "%s", avp_strdup(a, msg));
    }
    AvpTry t;
    if (AVP_TRY(&t)) {
        sqlite3_busy_timeout(con, 5000);
        exec(con, WT_TABLES_SQL);
        const char *stamp = wt_stamp(a, w->ix, w->exe);
        sqlite3_stmt *st = prep(con, "select value from meta where key = 'stamp'");
        const char *have = NULL;
        if (step(con, st) == SQLITE_ROW) have = col_cz(a, st, 0);
        sqlite3_finalize(st);
        if (rebuild || !have || strcmp(have, stamp) != 0) {
            double t0 = avp_time();
            WtBuilt b;
            wt_build(a, w->repo, w->ix, w->exe, jobs, &b);
            wt_save(con, stamp, &b);
            w->has_built = true;
            w->built = avp_py_round(avp_time() - t0, 1);
            wt_corr_free(b.c);
            avp_vec_free(&b.inl);
            avp_vec_free(&b.val.by_tier);
        }
        double t1 = avp_time();
        wt_load(a, con, &w->m);
        w->load_seconds = avp_py_round(avp_time() - t1, 2);
        avp_try_end(&t);
    } else {
        sqlite3_close(con);
        avp_throw(t.code, "%s", t.msg);
    }
    sqlite3_close(con);
    for (size_t k = 0; k < w->m.ps2.v.n; k++) avp_vec_push(&w->ps2_order, w->m.ps2.v.v[k]->addr);
    avp_sort_i64(w->ps2_order.v, w->ps2_order.n);
    w->ps2_rank = avp_imap_new();
    for (size_t k = 0; k < w->ps2_order.n; k++) {
        bool created;
        avp_imap_put(w->ps2_rank, (u64)w->ps2_order.v[k], &created)->i = (i64)k;
    }
    return w;
}

void wt_witness_close(WtWitness *w) {
    if (!w) return;
    if (w->ix) shsm_asmindex_close(w->ix);
    wt_corr_free(w->m.c);
    avp_vec_free(&w->m.inl);
    avp_vec_free(&w->ps2_order);
    avp_imap_free(w->ps2_rank);
    avp_arena_free(w->arena);
    avp_xfree(w);
}
