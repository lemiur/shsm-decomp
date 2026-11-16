/* twins.py: the Twins model (fingerprints, units, report, libraries) and the subcommands of, unmatched, families
 * and stats. */
#include "twins.h"

#include "avp/fmt.h"
#include "avp/sort.h"
#include "shsm_libraries.h"
#include "shsm_re.h"
#include "shsm_report.h"
#include "shsm_units.h"

#include <sqlite3.h>
#include <string.h>

/* ---- the model ---- */

Twins *twins_new(void) {
    AvpArena *a = avp_arena_new(0);
    Twins *tw = avp_alloc(a, sizeof *tw);
    tw->arena = a;
    tw->by_addr = avp_imap_new();
    for (int lv = 0; lv < TWINS_NLEVELS; lv++) tw->groups[lv] = avp_map_new();
    return tw;
}

TwinsFn *twins_add(Twins *tw, const TwinsFn *src) {
    TwinsFn *f = avp_alloc(tw->arena, sizeof *f);
    *f = *src;
    avp_vec_push(&tw->fns, f);
    avp_imap_put(tw->by_addr, (u64)f->address, NULL)->p = f;
    for (int lv = 0; lv < TWINS_NLEVELS; lv++) {
        bool created;
        AvpVal *v = avp_map_put(tw->groups[lv], avp_str(f->fp[lv]), &created);
        if (created) v->p = avp_alloc(tw->arena, sizeof(TwinsFnVec));
        avp_vec_push((TwinsFnVec *)v->p, f);
    }
    return f;
}

void twins_close(Twins *tw) {
    if (!tw) return;
    if (tw->ix) shsm_asmindex_close(tw->ix);
    for (int lv = 0; lv < TWINS_NLEVELS; lv++) {
        AvpMapIter it = {0};
        while (avp_map_next(tw->groups[lv], &it)) avp_vec_free((TwinsFnVec *)it.val->p);
        avp_map_free(tw->groups[lv]);
    }
    avp_imap_free(tw->by_addr);
    avp_vec_free(&tw->fns);
    avp_arena_free(tw->arena);
}

/* _matched_addresses: the exact functions of the report, else of the baseline; NULL when neither reads. */
static AvpISet *matched_addresses(const ShsmRepo *r) {
    for (int k = 0; k < 2; k++) {
        ShsmReport *rep = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            rep = k == 0 ? shsm_load_report(r) : shsm_load_baseline(r);
            avp_try_end(&t);
        } else {
            if (shsm_exc_is_os(t.code) || shsm_exc_is_value(t.code)) continue;
            avp_throw(t.code, "%s", t.msg);
        }
        AvpISet *s = avp_iset_new();
        for (size_t u = 0; u < rep->nunits; u++) {
            const ShsmUnitReport *ur = rep->units[u];
            for (size_t i = 0; i < ur->nfunctions; i++) {
                const ShsmFunctionReport *f = &ur->functions[i];
                if (f->fuzzy >= 100.0 && f->has_address) avp_iset_add(s, f->address);
            }
        }
        return s;
    }
    return NULL;
}

static const char *col_text(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return NULL;
    return avp_strdup(a, (const char *)sqlite3_column_text(st, c));
}

Twins *twins_load(const char *root, bool rebuild, int jobs) {
    const ShsmRepo *repo = shsm_repo_find(root);
    ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = repo, .readonly = true});
    sqlite3 *db = twins_open_db(repo, ix, rebuild, jobs);
    Twins *tw = twins_new();
    tw->repo = repo;
    tw->ix = ix;
    AvpArena *a = tw->arena;
    sqlite3_stmt *st;
    AVP_VEC(TwinsFn) rows = {0};
    if (sqlite3_prepare_v2(db, "select address, name, unit, size, nrows, exact, ref, shape from fingerprints"
                               " order by address", -1, &st, NULL) != SQLITE_OK)
        shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db));
    while (sqlite3_step(st) == SQLITE_ROW) {
        TwinsFn f = {0};
        f.address = sqlite3_column_int64(st, 0);
        f.name = col_text(a, st, 1);
        f.unit = col_text(a, st, 2);
        f.size = sqlite3_column_int64(st, 3);
        f.nrows = sqlite3_column_int64(st, 4);
        for (int lv = 0; lv < TWINS_NLEVELS; lv++) f.fp[lv] = col_text(a, st, 5 + lv);
        avp_vec_push(&rows, f);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    const ShsmUnits *us = shsm_load_units(repo);
    AvpISet *matched = matched_addresses(repo);
    const ShsmLibraries *libs = shsm_load_libraries(repo);
    for (size_t i = 0; i < rows.n; i++) {
        TwinsFn *f = &rows.v[i];
        const ShsmUnit *unit = f->unit ? shsm_units_lookup(us, shsm_unit_name(a, f->unit)) : NULL;
        f->status = unit ? unit->status : "Asm";
        f->source = unit && shsm_unit_has_source(unit) ? unit->source : NULL;
        if (!matched) f->matched = strcmp(f->status, "Matching") == 0;
        else f->matched = avp_iset_has(matched, (u64)f->address) && f->source;
        const char *lib = shsm_libraries_lookup(a, libs, f->address);
        f->lib = lib && *lib ? lib : "unknown";
        twins_add(tw, f);
    }
    avp_vec_free(&rows);
    if (matched) avp_iset_free(matched);
    return tw;
}

TwinsFnVec twins_of(AvpArena *a, const Twins *tw, const TwinsFn *f, int level) {
    (void)a;
    TwinsFnVec out = {0};
    AvpVal *v = avp_map_get(tw->groups[level], avp_str(f->fp[level]));
    if (!v) return out;
    const TwinsFnVec *g = v->p;
    for (size_t i = 0; i < g->n; i++)
        if (g->v[i] != f) avp_vec_push(&out, g->v[i]);
    return out;
}

static TwinsFn *by_addr(const Twins *tw, i64 address) {
    if (address == SHSM_NONE) return NULL;
    AvpVal *v = avp_imap_get(tw->by_addr, (u64)address);
    return v ? v->p : NULL;
}

/* int(text, 16): false on a ValueError */
static bool hex_int(AvpStr text, i64 *v) {
    bool overflow = false;
    return avp_parse_int(text, 16, v, &overflow) && !overflow;
}

TwinsFn *twins_resolve(Twins *tw, const char *text) {
    AvpArena *a = tw->arena;
    AvpStr t = avp_str(text);
    ShsmSymRow sr = {0};
    bool have = false;
    ShsmReMatch m;
    i64 addr;
    if (shsm_re_fullmatch(shsm_re("(0x)?[0-9A-Fa-f]{8}", 0), t, 0, AVP_END, &m)) {
        if (hex_int(t, &addr)) have = shsm_asmindex_symbol_at(a, tw->ix, addr, &sr);
    } else {
        ShsmSymRows all = shsm_asmindex_symbols_named(a, tw->ix, t);
        ShsmSymRows fns = {0};
        for (size_t i = 0; i < all.n; i++)
            if (by_addr(tw, all.v[i].address)) avp_vec_push(&fns, all.v[i]);
        if (fns.n > 1) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t i = 0; i < fns.n; i++) avp_buf_pyfmt(&b, i ? ", %08X" : "%08X", fns.v[i].address);
            AvpStr list = avp_buf_str(&b);
            i64 nf = (i64)fns.n;
            avp_vec_free(&fns);
            shsm_raise(SHSM_EXC_USAGE, "%s",
                       avp_cz(a, avp_pyfmt(a, "%r names %d functions (%S): give an address", text, nf, &list)));
        }
        if (fns.n) {
            sr = fns.v[0];
            have = true;
        } else {
            have = shsm_asmindex_symbol(a, tw->ix, t, &sr);
        }
        avp_vec_free(&fns);
    }
    if (!have && hex_int(t, &addr)) have = shsm_asmindex_containing(a, tw->ix, addr, &sr);
    TwinsFn *f = have ? by_addr(tw, sr.address) : NULL;
    if (!f) shsm_raise(SHSM_EXC_USAGE, "%s", avp_cz(a, avp_pyfmt(a, "no function %r in the asm index", text)));
    return f;
}

AvpStrVec twins_operand_diffs(AvpArena *a, Twins *tw, const TwinsFn *x, const TwinsFn *y, size_t cap, i64 *n) {
    ShsmAsmFile *fa = NULL, *fb = NULL;
    ShsmAsmBlock *ba = shsm_asmindex_function(a, tw->ix, shsm_target_addr(x->address), &fa);
    ShsmAsmBlock *bb = shsm_asmindex_function(a, tw->ix, shsm_target_addr(y->address), &fb);
    AvpStrVec diffs = {0};
    size_t nr = ba && bb ? (ba->rows.n < bb->rows.n ? ba->rows.n : bb->rows.n) : 0;
    for (size_t i = 0; i < nr; i++) {
        const ShsmAsmRow *rx = &ba->rows.v[i], *ry = &bb->rows.v[i];
        AvpStr nx[3], ny[3];
        twins_normalise_row(a, rx, x->address, x->name, nx);
        twins_normalise_row(a, ry, y->address, y->name, ny);
        AvpStr ex = nx[0], ey = ny[0];
        i64 sx = avp_str_findc(ex, ' '), sy = avp_str_findc(ey, ' ');
        ex = avp_strn(ex.p + sx + 1, ex.n - (size_t)sx - 1);
        ey = avp_strn(ey.p + sy + 1, ey.n - (size_t)sy - 1);
        if (avp_str_eq(ex, ey)) continue;
        AvpStrVec tx = avp_str_split(ex, AVP_S(", "), -1), ty = avp_str_split(ey, AVP_S(", "), -1);
        AvpBuf b;
        avp_buf_init(&b, a);
        bool first = true;
        if (tx.n == ty.n) {
            for (size_t k = 0; k < tx.n; k++) {
                if (avp_str_eq(tx.v[k], ty.v[k])) continue;
                avp_buf_pyfmt(&b, first ? "%S->%S" : " %S->%S", &tx.v[k], &ty.v[k]);
                first = false;
            }
        } else {
            avp_buf_pyfmt(&b, "%S->%S", &ex, &ey);
        }
        avp_vec_free(&tx);
        avp_vec_free(&ty);
        AvpStr pairs = avp_buf_str(&b);
        AvpStr mn = rx->mnemonic;
        avp_vec_push(&diffs, avp_pyfmt(a, "+0x%X %S %S", rx->address - x->address, &mn, &pairs));
    }
    if (fa) shsm_asm_free(fa);
    if (fb) shsm_asm_free(fb);
    *n = (i64)diffs.n;
    if (diffs.n > cap) {
        size_t more = diffs.n - cap;
        diffs.n = cap;
        avp_vec_push(&diffs, avp_pyfmt(a, "+%d more", (i64)more));
    }
    return diffs;
}

bool twins_lib_matches(AvpArena *a, const TwinsFn *f, const char *lib) {
    if (!lib || !*lib) return true;
    AvpStr l = avp_str_strip_chars(avp_str_lower(a, avp_str(lib)), AVP_S("/"));
    const char *name = avp_cz(a, avp_str_lower(a, avp_str(f->lib && *f->lib ? f->lib : "unknown")));
    AvpStr nm = avp_str(name);
    if (avp_str_eq(l, nm)) return true;
    i64 slash = avp_str_rfind(nm, AVP_S("/"), 0, AVP_END);
    AvpStr last = slash >= 0 ? avp_strn(nm.p + slash + 1, nm.n - (size_t)slash - 1) : nm;
    if (avp_str_eq(l, last)) return true;
    return avp_str_eq(l, shsm_library_group_of(name));
}

AvpJson *twins_fn_json(AvpArena *a, const TwinsFn *f) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("address"), avp_json_str(a, avp_pyfmt(a, "0x%08X", f->address)));
    avp_json_set(a, o, AVP_S("name"), f->name ? avp_json_strz(a, f->name) : avp_json_null());
    avp_json_set(a, o, AVP_S("unit"), f->unit ? avp_json_strz(a, f->unit) : avp_json_null());
    avp_json_set(a, o, AVP_S("size"), avp_json_int(a, f->size));
    avp_json_set(a, o, AVP_S("matched"), avp_json_bool(f->matched));
    avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, f->status));
    avp_json_set(a, o, AVP_S("source"), f->source ? avp_json_strz(a, f->source) : avp_json_null());
    avp_json_set(a, o, AVP_S("lib"), f->lib ? avp_json_strz(a, f->lib) : avp_json_null());
    return o;
}

/* ---- stats ---- */

void twins_compute_stats(const Twins *tw, i64 min_size, TwinsStats out[TWINS_NLEVELS]) {
    for (int lv = 0; lv < TWINS_NLEVELS; lv++) {
        TwinsStats *st = &out[lv];
        memset(st, 0, sizeof *st);
        AvpMapIter it = {0};
        while (avp_map_next(tw->groups[lv], &it)) {
            const TwinsFnVec *g = it.val->p;
            i64 n = 0, bytes = 0;
            bool anym = false;
            for (size_t i = 0; i < g->n; i++) {
                if (g->v[i]->size < min_size) continue;
                n++;
                bytes += g->v[i]->size;
                anym = anym || g->v[i]->matched;
            }
            if (!n) continue;
            st->functions += n;
            st->bytes += bytes;
            if (n < 2) continue;
            st->groups++;
            st->with_twin += n;
            st->with_twin_bytes += bytes;
            for (size_t i = 0; i < g->n; i++) {
                const TwinsFn *m = g->v[i];
                if (m->size < min_size) continue;
                if (m->matched) st->matched_with_twin++;
                else if (anym) {
                    st->unmatched_with_matched_twin++;
                    st->unmatched_with_matched_twin_bytes += m->size;
                } else {
                    st->unmatched_in_families++;
                    st->unmatched_in_families_bytes += m->size;
                }
            }
        }
    }
}

AvpJson *twins_stats_json(AvpArena *a, const TwinsStats *s) {
    AvpJson *o = avp_json_obj(a);
#define F(k) avp_json_set(a, o, AVP_S(#k), avp_json_int(a, s->k))
    F(functions);
    F(bytes);
    F(with_twin);
    F(with_twin_bytes);
    F(groups);
    F(unmatched_with_matched_twin);
    F(unmatched_with_matched_twin_bytes);
    F(unmatched_in_families);
    F(unmatched_in_families_bytes);
    F(matched_with_twin);
#undef F
    return o;
}

/* ---- output helpers ---- */

static void put_line(TwinsOut *o, AvpStr s) {
    if (o->out) shsm_output_line(o->out, s);
    else avp_vec_push(&o->lines, avp_str_dup(avp_arena(), s));
}

static void put_data(TwinsOut *o, const char *key, AvpJson *v) {
    if (o->out) {
        shsm_output_data(o->out, key, v);
        return;
    }
    if (!o->data) o->data = avp_json_obj(avp_arena());
    avp_json_set(avp_arena(), o->data, avp_str(key), v);
}

/* _more: the capped list's footer */
static void more(AvpArena *a, TwinsOut *o, size_t total, size_t shown, const char *what) {
    if (total > shown) put_line(o, avp_pyfmt(a, "... %d more %s, use --all", (i64)(total - shown), what));
}

static size_t cap(const TwinsArgs *args, size_t n) {
    if (args->all || !args->limit) return n;
    return (size_t)args->limit < n ? (size_t)args->limit : n;
}

static const char *mark(AvpArena *a, const TwinsFn *f) {
    const char *st = strcmp(f->status, "Matching") == 0 ? "M" : strcmp(f->status, "NonMatching") == 0 ? "N" : ".";
    return avp_sprintf(a, "%s%s", f->matched ? "*" : " ", st);
}

static AvpStr fmt_fn(AvpArena *a, const TwinsFn *f) {
    AvpStr name = avp_u8_slice(avp_str(f->name), 0, 44), unit = avp_u8_slice(avp_str(f->unit), 0, 34);
    return avp_pyfmt(a, "%s %08X %-44S %5d  %-34S", mark(a, f), f->address, &name, f->size, &unit);
}

static AvpJson *fns_json(AvpArena *a, TwinsFn *const *v, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, twins_fn_json(a, v[i]));
    return arr;
}

static AvpStr join(AvpArena *a, const AvpStrVec *v, const char *sep) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < v->n; i++) {
        if (i) avp_buf_putz(&b, sep);
        avp_buf_put(&b, v->v[i]);
    }
    return avp_buf_str(&b);
}

/* ---- of ---- */

static int cmp_unmatched_first(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const TwinsFn *a = *(TwinsFn *const *)x, *b = *(TwinsFn *const *)y;
    if (a->matched != b->matched) return a->matched ? -1 : 1; /* key (not matched, address) */
    return a->address < b->address ? -1 : a->address > b->address;
}

int twins_cmd_of(const TwinsArgs *args, Twins *tw, TwinsOut *out) {
    AvpArena *a = avp_arena();
    TwinsFn *f = twins_resolve(tw, args->function);
    put_line(out, avp_pyfmt(a, "%s  %08X  %d bytes  unit %s (%s)  %s", f->name, f->address, f->size, f->unit,
                            f->status, f->matched ? "matched" : "unmatched"));
    AvpJson *levels = avp_json_obj(a);
    AvpISet *seen = avp_iset_new();
    for (int lv = 0; lv < TWINS_NLEVELS; lv++) {
        TwinsFnVec all = twins_of(a, tw, f, lv), ts = {0};
        for (size_t i = 0; i < all.n; i++)
            if (!avp_iset_has(seen, (u64)all.v[i]->address)) avp_vec_push(&ts, all.v[i]);
        avp_vec_free(&all);
        for (size_t i = 0; i < ts.n; i++) avp_iset_add(seen, (u64)ts.v[i]->address);
        avp_json_set(a, levels, avp_str(TWINS_LEVELS[lv]), fns_json(a, ts.v, ts.n));
        put_line(out, avp_pyfmt(a, "%s: %d new twin(s)%s", TWINS_LEVELS[lv], (i64)ts.n,
                                lv == TWINS_EXACT ? "" : " (beyond the levels above)"));
        avp_sort(ts.v, ts.n, sizeof *ts.v, cmp_unmatched_first, NULL, false);
        size_t shown = cap(args, ts.n);
        for (size_t i = 0; i < shown; i++) {
            const TwinsFn *t = ts.v[i];
            AvpStr fl = fmt_fn(a, t);
            AvpStr line = avp_pyfmt(a, "  %S", &fl);
            if (t->matched) line = avp_pyfmt(a, "%S  src %s", &line, t->source);
            put_line(out, line);
            if (lv == TWINS_SHAPE) {
                i64 n;
                AvpStrVec d = twins_operand_diffs(a, tw, f, t, 6, &n);
                AvpStr j = join(a, &d, "; ");
                put_line(out, avp_pyfmt(a, "      %S", &j));
                avp_vec_free(&d);
            }
        }
        more(a, out, ts.n, shown, "twin(s)");
        avp_vec_free(&ts);
    }
    avp_iset_free(seen);
    put_data(out, "function", twins_fn_json(a, f));
    put_data(out, "levels", levels);
    return SHSM_EXIT_OK;
}

/* ---- unmatched ---- */

typedef struct {
    TwinsFn *f;
    TwinsFnVec ms;
} UnRow;

static int cmp_matching_first(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const TwinsFn *a = *(TwinsFn *const *)x, *b = *(TwinsFn *const *)y;
    int ka = strcmp(a->status, "Matching") != 0, kb = strcmp(b->status, "Matching") != 0;
    if (ka != kb) return ka - kb;
    return a->address < b->address ? -1 : a->address > b->address;
}

static int cmp_size_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const TwinsFn *a = ((const UnRow *)x)->f, *b = ((const UnRow *)y)->f;
    if (a->size != b->size) return a->size > b->size ? -1 : 1;
    return a->address < b->address ? -1 : a->address > b->address;
}

int twins_cmd_unmatched(const TwinsArgs *args, Twins *tw, TwinsOut *out) {
    AvpArena *a = avp_arena();
    int lv = twins_level_index(args->level);
    AVP_VEC(UnRow) rows = {0};
    for (size_t i = 0; i < tw->fns.n; i++) {
        TwinsFn *f = tw->fns.v[i];
        if (f->matched || f->size < args->min_size || !twins_lib_matches(a, f, args->lib)) continue;
        if (lv == TWINS_SHAPE) {
            TwinsFnVec ex = twins_of(a, tw, f, TWINS_EXACT);
            bool any = false;
            for (size_t k = 0; k < ex.n && !any; k++) any = ex.v[k]->matched;
            avp_vec_free(&ex);
            if (any) continue; /* listed at the exact level */
        }
        TwinsFnVec all = twins_of(a, tw, f, lv), ms = {0};
        for (size_t k = 0; k < all.n; k++)
            if (all.v[k]->matched) avp_vec_push(&ms, all.v[k]);
        avp_vec_free(&all);
        if (ms.n) {
            avp_sort(ms.v, ms.n, sizeof *ms.v, cmp_matching_first, NULL, false);
            avp_vec_push(&rows, ((UnRow){f, ms}));
        }
    }
    avp_sort(rows.v, rows.n, sizeof *rows.v, cmp_size_desc, NULL, false);
    i64 bytes = 0;
    for (size_t i = 0; i < rows.n; i++) bytes += rows.v[i].f->size;
    put_line(out, avp_pyfmt(a, "%d unmatched function(s), %d bytes, with a matched %s twin (min size %d)", (i64)rows.n,
                            bytes, args->level, args->min_size));
    size_t shown = cap(args, rows.n);
    AvpJson *data = avp_json_arr(a);
    for (size_t i = 0; i < shown; i++) {
        const UnRow *r = &rows.v[i];
        const TwinsFn *t = r->ms.v[0];
        AvpStr fl = fmt_fn(a, r->f);
        put_line(out, avp_pyfmt(a, "%S <- %s %s", &fl, t->name, t->source));
        AvpJson *rec = avp_json_obj(a);
        avp_json_set(a, rec, AVP_S("function"), twins_fn_json(a, r->f));
        avp_json_set(a, rec, AVP_S("twins"), fns_json(a, r->ms.v, r->ms.n));
        if (lv == TWINS_SHAPE) {
            i64 n;
            AvpStrVec d = twins_operand_diffs(a, tw, t, r->f, 6, &n);
            avp_json_set(a, rec, AVP_S("diffs"), avp_json_int(a, n));
            AvpStr j = join(a, &d, "; ");
            put_line(out, avp_pyfmt(a, "      %d row(s) differ: %S", n, &j));
            avp_vec_free(&d);
        }
        avp_json_arr_push(a, data, rec);
    }
    more(a, out, rows.n, shown, "function(s)");
    put_data(out, "level", avp_json_strz(a, args->level));
    put_data(out, "count", avp_json_int(a, (i64)rows.n));
    put_data(out, "bytes", avp_json_int(a, bytes));
    put_data(out, "functions", data);
    for (size_t i = 0; i < rows.n; i++) avp_vec_free(&rows.v[i].ms);
    avp_vec_free(&rows);
    return SHSM_EXIT_OK;
}

/* ---- families ---- */

typedef struct {
    TwinsFnVec ms;
    i64 bytes;
} Fam;

static int cmp_fam(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Fam *a = x, *b = y;
    if (a->bytes != b->bytes) return a->bytes > b->bytes ? -1 : 1;
    i64 aa = a->ms.v[0]->address, ba = b->ms.v[0]->address;
    return aa < ba ? -1 : aa > ba;
}

static int cmp_strz(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

int twins_cmd_families(const TwinsArgs *args, Twins *tw, TwinsOut *out) {
    AvpArena *a = avp_arena();
    int lv = twins_level_index(args->level);
    AVP_VEC(Fam) fams = {0};
    bool empty = false;
    AvpMapIter it = {0};
    while (avp_map_next(tw->groups[lv], &it)) {
        const TwinsFnVec *g = it.val->p;
        bool any = false;
        for (size_t i = 0; i < g->n && !any; i++) any = g->v[i]->matched;
        if (any) continue;
        Fam fm = {0};
        for (size_t i = 0; i < g->n; i++)
            if (g->v[i]->size >= args->min_size && twins_lib_matches(a, g->v[i], args->lib)) {
                avp_vec_push(&fm.ms, g->v[i]);
                fm.bytes += g->v[i]->size;
            }
        if ((i64)fm.ms.n >= args->min_count) {
            if (!fm.ms.n) empty = true;
            avp_vec_push(&fams, fm);
        } else {
            avp_vec_free(&fm.ms);
        }
    }
    /* the Python's sort key reads ms[0]: an empty family (--min-count 0 or less) is its IndexError */
    if (empty) shsm_raise(SHSM_EXC_VALUE, "list index out of range"); /* IndexError: printed the same way */
    if (fams.n > 1) avp_sort(fams.v, fams.n, sizeof *fams.v, cmp_fam, NULL, false);
    i64 total = 0;
    for (size_t i = 0; i < fams.n; i++) total += fams.v[i].bytes;
    put_line(out, avp_pyfmt(a, "%d %s-level famil(ies) of unmatched functions with no matched member, %d bytes "
                               "(min count %d, min size %d)",
                            (i64)fams.n, args->level, total, args->min_count, args->min_size));
    size_t shown = cap(args, fams.n);
    AvpJson *data = avp_json_arr(a);
    for (size_t i = 0; i < shown; i++) {
        const Fam *fm = &fams.v[i];
        AVP_VEC(const char *) us = {0};
        for (size_t k = 0; k < fm->ms.n; k++) avp_vec_push(&us, fm->ms.v[k]->unit ? fm->ms.v[k]->unit : "");
        avp_sort(us.v, us.n, sizeof *us.v, cmp_strz, NULL, false);
        size_t nu = 0;
        for (size_t k = 0; k < us.n; k++)
            if (!k || strcmp(us.v[k], us.v[k - 1]) != 0) nu++;
        avp_vec_free(&us);
        AvpBuf nb;
        avp_buf_init(&nb, a);
        for (size_t k = 0; k < fm->ms.n && k < 4; k++) {
            if (k) avp_buf_putz(&nb, ", ");
            avp_buf_putz(&nb, fm->ms.v[k]->name);
        }
        if (fm->ms.n > 4) avp_buf_pyfmt(&nb, ", ... +%d", (i64)(fm->ms.n - 4));
        AvpStr names = avp_buf_str(&nb);
        AvpStr lib = avp_u8_slice(avp_str(fm->ms.v[0]->lib), 0, 20);
        put_line(out, avp_pyfmt(a, "%3d x %5d = %6d bytes  %S  in %d unit(s): %S", (i64)fm->ms.n, fm->ms.v[0]->size,
                                fm->bytes, &lib, (i64)nu, &names));
        AvpJson *rec = avp_json_obj(a);
        avp_json_set(a, rec, AVP_S("count"), avp_json_int(a, (i64)fm->ms.n));
        avp_json_set(a, rec, AVP_S("bytes"), avp_json_int(a, fm->bytes));
        avp_json_set(a, rec, AVP_S("members"), fns_json(a, fm->ms.v, fm->ms.n < 50 ? fm->ms.n : 50));
        avp_json_arr_push(a, data, rec);
    }
    more(a, out, fams.n, shown, "famil(ies)");
    put_data(out, "level", avp_json_strz(a, args->level));
    put_data(out, "families", data);
    for (size_t i = 0; i < fams.n; i++) avp_vec_free(&fams.v[i].ms);
    avp_vec_free(&fams);
    return SHSM_EXIT_OK;
}

/* ---- stats ---- */

int twins_cmd_stats(const TwinsArgs *args, Twins *tw, TwinsOut *out) {
    AvpArena *a = avp_arena();
    TwinsStats res[TWINS_NLEVELS];
    twins_compute_stats(tw, args->min_size, res);
    i64 nm = 0, ns = 0, sb = 0;
    for (size_t i = 0; i < tw->fns.n; i++) {
        nm += tw->fns.v[i]->matched;
        if (tw->fns.v[i]->size < args->min_size) {
            ns++;
            sb += tw->fns.v[i]->size;
        }
    }
    const char *at = tw->repo ? shsm_repo_rel(a, tw->repo, twins_db_path(a, tw->repo)) : "";
    put_line(out, avp_pyfmt(a, "%d functions (%d matched); fingerprints at %s; min size %d bytes", (i64)tw->fns.n, nm,
                            at, args->min_size));
    put_line(out, AVP_S("level  functions  with-twin (bytes)        groups  matched-w/twin  unmatched+matched-twin "
                        "(bytes)  unmatched-families-only (bytes)"));
    AvpJson *levels = avp_json_obj(a);
    for (int lv = 0; lv < TWINS_NLEVELS; lv++) {
        const TwinsStats *s = &res[lv];
        put_line(out, avp_pyfmt(a, "%-6s %9d  %6d (%9d)  %6d  %14d  %8d (%9d)          %8d (%9d)", TWINS_LEVELS[lv],
                                s->functions, s->with_twin, s->with_twin_bytes, s->groups, s->matched_with_twin,
                                s->unmatched_with_matched_twin, s->unmatched_with_matched_twin_bytes,
                                s->unmatched_in_families, s->unmatched_in_families_bytes));
        avp_json_set(a, levels, avp_str(TWINS_LEVELS[lv]), twins_stats_json(a, s));
    }
    put_line(out, avp_pyfmt(a, "below the size threshold: %d functions, %d bytes (ignored)", ns, sb));
    put_data(out, "min_size", avp_json_int(a, args->min_size));
    put_data(out, "functions", avp_json_int(a, (i64)tw->fns.n));
    put_data(out, "matched", avp_json_int(a, nm));
    put_data(out, "levels", levels);
    return SHSM_EXIT_OK;
}
