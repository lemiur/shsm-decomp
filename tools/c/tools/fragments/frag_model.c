/* fragments.py the model: Frag, Occ, _matched_addresses, lib_matches, Fragments, frag_of, fragments_at, in_function. */
#include "frag.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/sort.h"
#include "shsm_re.h"
#include "shsm_report.h"

#include <string.h>

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

bool frag_lib_matches(AvpArena *a, const char *lib, const char *want) {
    if (!want || !*want) return true;
    AvpStr w = avp_str_strip_chars(avp_str_lower(a, avp_str(want)), AVP_S("/"));
    const char *name = avp_cz(a, avp_str_lower(a, avp_str(lib && *lib ? lib : "unknown")));
    AvpStr nm = avp_str(name);
    if (avp_str_eq(w, nm)) return true;
    i64 slash = avp_str_rfind(nm, AVP_S("/"), 0, AVP_END);
    AvpStr last = slash >= 0 ? avp_strn(nm.p + slash + 1, nm.n - (size_t)slash - 1) : nm;
    if (avp_str_eq(w, last)) return true;
    return avp_str_eq(w, shsm_library_group_of(name));
}

static const char *col_text(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return NULL;
    return avp_cz(a, avp_strn((const char *)sqlite3_column_text(st, c), (size_t)sqlite3_column_bytes(st, c)));
}

Frag *frag_of_stmt(AvpArena *a, sqlite3_stmt *st, int c) {
    Frag *f = AVP_NEW(a, Frag);
    f->id = col_text(a, st, c);
    f->level = col_text(a, st, c + 1);
    f->rank = sqlite3_column_int64(st, c + 2);
    f->length = sqlite3_column_int64(st, c + 3);
    f->nfuncs = sqlite3_column_int64(st, c + 4);
    f->nunits = sqlite3_column_int64(st, c + 5);
    f->nocc = sqlite3_column_int64(st, c + 6);
    f->score = sqlite3_column_int64(st, c + 7);
    const char *pat = col_text(a, st, c + 8);
    AvpJson *j = avp_json_parse(a, avp_str(pat ? pat : "[]"), NULL);
    for (size_t i = 0; i < avp_json_len(j); i++) avp_vec_push(&f->pattern, avp_json_str_or(avp_json_at(j, i), AVP_S("")));
    return f;
}

AvpJson *frag_occ_json(AvpArena *a, const FragOccRec *o) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("function"), o->fn_name ? avp_json_strz(a, o->fn_name) : avp_json_null());
    avp_json_set(a, d, AVP_S("fn_address"), avp_json_str(a, avp_pyfmt(a, "0x%08X", o->fn_address)));
    avp_json_set(a, d, AVP_S("address"), avp_json_str(a, avp_pyfmt(a, "0x%08X", o->address)));
    avp_json_set(a, d, AVP_S("unit"), o->unit ? avp_json_strz(a, o->unit) : avp_json_null());
    avp_json_set(a, d, AVP_S("matched"), avp_json_bool(o->matched));
    avp_json_set(a, d, AVP_S("status"), avp_json_strz(a, o->status));
    avp_json_set(a, d, AVP_S("source"), o->source ? avp_json_strz(a, o->source) : avp_json_null());
    avp_json_set(a, d, AVP_S("lib"), avp_json_strz(a, o->lib));
    return d;
}

AvpJson *frag_json(AvpArena *a, const Frag *f, bool with_occ) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("id"), avp_json_strz(a, f->id));
    avp_json_set(a, d, AVP_S("level"), avp_json_strz(a, f->level));
    avp_json_set(a, d, AVP_S("rank"), avp_json_int(a, f->rank));
    avp_json_set(a, d, AVP_S("length"), avp_json_int(a, f->length));
    avp_json_set(a, d, AVP_S("nfuncs"), avp_json_int(a, f->nfuncs));
    avp_json_set(a, d, AVP_S("nunits"), avp_json_int(a, f->nunits));
    avp_json_set(a, d, AVP_S("nocc"), avp_json_int(a, f->nocc));
    avp_json_set(a, d, AVP_S("score"), avp_json_int(a, f->score));
    AvpJson *p = avp_json_arr(a);
    for (size_t i = 0; i < f->pattern.n; i++) avp_json_arr_push(a, p, avp_json_str(a, f->pattern.v[i]));
    avp_json_set(a, d, AVP_S("pattern"), p);
    if (with_occ) {
        AvpJson *o = avp_json_arr(a);
        for (size_t i = 0; i < f->occ.n; i++) avp_json_arr_push(a, o, frag_occ_json(a, &f->occ.v[i]));
        avp_json_set(a, d, AVP_S("occurrences"), o);
    }
    return d;
}

/* ---- Fragments ---- */

Frags *frags_open(const char *root, bool rebuild, int jobs, i64 window, i64 sites, FragLogFn log) {
    AvpArena *a = avp_arena();
    Frags *fr = AVP_NEW(a, Frags);
    fr->arena = a;
    fr->repo = shsm_repo_find(root);
    fr->ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = fr->repo, .readonly = true});
    fr->con = frag_open_db(fr->repo, fr->ix, rebuild, jobs, window, sites, log);
    fr->meta = avp_json_obj(a);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(fr->con, "select key, value from meta", -1, &st, NULL) != SQLITE_OK) frag_sql_fail(fr->con);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *k = col_text(a, st, 0), *v = col_text(a, st, 1);
        avp_json_set(a, fr->meta, avp_str(k ? k : ""), v ? avp_json_strz(a, v) : avp_json_null());
    }
    sqlite3_finalize(st);
    fr->units = shsm_load_units(fr->repo);
    fr->matched = matched_addresses(fr->repo);
    fr->libs = shsm_load_libraries(fr->repo);
    fr->fncache = avp_imap_new();
    return fr;
}

void frags_close(Frags *fr) {
    if (!fr) return;
    if (fr->con) sqlite3_close(fr->con);
    if (fr->ix) shsm_asmindex_close(fr->ix);
    if (fr->matched) avp_iset_free(fr->matched);
    avp_imap_free(fr->fncache);
    fr->con = NULL;
    fr->ix = NULL;
}

const char *frags_meta(const Frags *fr, const char *key) {
    AvpJson *v = avp_json_getz(fr->meta, key);
    if (!v || avp_json_is_null(v)) return NULL;
    AvpStr s = avp_json_str_or(v, AVP_S(""));
    return avp_cz(fr->arena, s);
}

FragFnInfo frags_fninfo(Frags *fr, i64 addr, const char *unit) {
    AvpVal *v = avp_imap_get(fr->fncache, (u64)addr);
    if (v) return *(FragFnInfo *)v->p;
    AvpArena *a = fr->arena;
    FragFnInfo *got = AVP_NEW(a, FragFnInfo);
    const ShsmUnit *u = unit && *unit ? shsm_units_lookup(fr->units, shsm_unit_name(a, unit)) : NULL;
    got->status = u ? u->status : "Asm";
    got->source = u && shsm_unit_has_source(u) ? u->source : NULL;
    if (!fr->matched) got->matched = strcmp(got->status, "Matching") == 0;
    else got->matched = avp_iset_has(fr->matched, (u64)addr) && got->source;
    const char *lib = shsm_libraries_lookup(a, fr->libs, addr);
    got->lib = lib && *lib ? lib : "unknown";
    bool created;
    avp_imap_put(fr->fncache, (u64)addr, &created)->p = got;
    return *got;
}

static FragOccRec occ_of(Frags *fr, sqlite3_stmt *st, int c) {
    FragOccRec o;
    memset(&o, 0, sizeof o);
    o.fn_address = sqlite3_column_int64(st, c);
    o.fn_name = col_text(fr->arena, st, c + 1);
    o.unit = col_text(fr->arena, st, c + 2);
    o.row = sqlite3_column_int64(st, c + 3);
    o.address = sqlite3_column_int64(st, c + 4);
    FragFnInfo i = frags_fninfo(fr, o.fn_address, o.unit);
    o.matched = i.matched;
    o.status = i.status;
    o.source = i.source;
    o.lib = i.lib;
    return o;
}

Frag *frags_fragment(Frags *fr, const char *fid) {
    AvpArena *a = fr->arena;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(fr->con, "select * from fragments where id = ?", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(fr->con);
    AvpStr up = avp_str_upper(a, avp_str(fid));
    sqlite3_bind_text(st, 1, up.p, (int)up.n, SQLITE_TRANSIENT);
    Frag *f = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) f = frag_of_stmt(a, st, 0);
    sqlite3_finalize(st);
    if (!f) {
        AvpStr r = avp_repr_str(a, avp_str(fid));
        shsm_raise(SHSM_EXC_USAGE, "no fragment %.*s (ids are S<n> and L<n>, see `list`)", (int)r.n, r.p);
    }
    if (sqlite3_prepare_v2(fr->con, "select fn_address, fn_name, unit, row, address from occurrences where fragment = ?"
                                    " order by address", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(fr->con);
    sqlite3_bind_text(st, 1, f->id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) avp_vec_push(&f->occ, occ_of(fr, st, 0));
    sqlite3_finalize(st);
    return f;
}

FragList frags_level(Frags *fr, const char *level) {
    AvpArena *a = fr->arena;
    FragList out = {0};
    AvpMap *byid = avp_map_new();
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(fr->con, "select * from fragments where level = ? order by rank", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(fr->con);
    sqlite3_bind_text(st, 1, level, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        Frag *f = frag_of_stmt(a, st, 0);
        bool created;
        AvpVal *v = avp_map_put(byid, avp_str(f->id), &created);
        if (created) {
            v->i = (i64)out.n;
            avp_vec_push(&out, f);
        } else {
            out.v[v->i] = f;
        }
    }
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(fr->con, "select o.fragment, o.fn_address, o.fn_name, o.unit, o.row, o.address"
                                    " from occurrences o join fragments f on f.id = o.fragment where f.level = ?"
                                    " order by o.address", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(fr->con);
    sqlite3_bind_text(st, 1, level, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) {
        AvpStr id = avp_strn((const char *)sqlite3_column_text(st, 0), (size_t)sqlite3_column_bytes(st, 0));
        AvpVal *v = avp_map_get(byid, id);
        if (!v) shsm_raise(SHSM_EXC_KEY, "%.*s", (int)id.n, id.p);
        avp_vec_push(&out.v[v->i]->occ, occ_of(fr, st, 1));
    }
    sqlite3_finalize(st);
    avp_map_free(byid);
    return out;
}

static bool is_hex8(AvpStr t) {
    return shsm_re_fullmatch(shsm_re("(0x)?[0-9A-Fa-f]{8}", 0), t, 0, AVP_END, NULL);
}

static i64 hexval(AvpStr t) {
    if (avp_str_startswith(t, AVP_S("0x"))) t = avp_strn(t.p + 2, t.n - 2);
    u64 v = 0;
    avp_parse_uint(t, 16, &v);
    return (i64)v;
}

void frags_resolve(Frags *fr, const char *text, ShsmSymRow *out) {
    AvpArena *a = fr->arena;
    AvpStr t = avp_str(text);
    bool have = false;
    if (is_hex8(t)) {
        have = shsm_asmindex_symbol_at(a, fr->ix, hexval(t), out);
    } else {
        ShsmSymRows all = shsm_asmindex_symbols_named(a, fr->ix, t);
        AVP_VEC(ShsmSymRow) fns = {0};
        for (size_t i = 0; i < all.n; i++)
            if (avp_str_eqz(all.v[i].kind, "function")) avp_vec_push(&fns, all.v[i]);
        if (fns.n > 1) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t i = 0; i < fns.n; i++) avp_buf_pyfmt(&b, i ? ", %08X" : "%08X", fns.v[i].address);
            AvpStr r = avp_repr_str(a, t), l = avp_buf_str(&b);
            size_t n = fns.n;
            avp_vec_free(&fns);
            shsm_raise(SHSM_EXC_USAGE, "%.*s names %zu functions (%.*s): give an address", (int)r.n, r.p, n, (int)l.n,
                       l.p);
        }
        if (fns.n) {
            *out = fns.v[0];
            have = true;
        } else {
            have = shsm_asmindex_symbol(a, fr->ix, t, out);
        }
        avp_vec_free(&fns);
    }
    if (!have && is_hex8(t)) have = shsm_asmindex_containing(a, fr->ix, hexval(t), out);
    if (!have || !avp_str_eqz(out->kind, "function")) {
        AvpStr r = avp_repr_str(a, t);
        shsm_raise(SHSM_EXC_USAGE, "no function %.*s in the asm index", (int)r.n, r.p);
    }
}

FragRow *frags_occ_rows(Frags *fr, const FragOccRec *o, i64 length, int before, int after, size_t *n) {
    AvpArena *a = fr->arena;
    ShsmAsmFile *file = NULL;
    ShsmAsmBlock *b = shsm_asmindex_function(a, fr->ix, shsm_target_addr(o->fn_address), &file);
    if (!b) shsm_raise(SHSM_EXC_TYPE, "'NoneType' object has no attribute 'rows'");
    size_t nr = b->rows.n;
    bool *flags = avp_alloc(a, (nr ? nr : 1) * sizeof *flags);
    frag_frame_flags(b->rows.v, nr, flags);
    i64 lo = o->row - before, hi = o->row + length + after;
    FragRow *out = avp_alloc(a, (size_t)(hi - lo) * sizeof *out);
    size_t k = 0;
    AvpStr name = avp_str_dup(a, b->name);
    for (i64 i = lo; i < hi; i++, k++) {
        if (i >= 0 && i < (i64)nr) {
            if (flags[i] && !(o->row <= i && i < o->row + length)) {
                out[k].disp = avp_pyfmt(a, "|%S", &b->rows.v[i].mnemonic);
                continue;
            }
            FragNorm x;
            frag_norm_row(a, &b->rows.v[i], b->address, name, &x);
            out[k].regs = avp_memdup(a, x.regs, sizeof x.regs);
            out[k].nregs = x.nregs;
            out[k].disp = x.disp;
        } else {
            out[k].disp = AVP_S("|");
        }
    }
    if (file) shsm_asm_free(file);
    *n = k;
    return out;
}

/* ---- fragments_at, in_function ---- */

typedef struct {
    i64 a, e, l;
} Picked;

static int cmp_score_addr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const FragAt *p = x, *q = y;
    if (p->f->score != q->f->score) return p->f->score > q->f->score ? -1 : 1;
    if (p->address != q->address) return p->address < q->address ? -1 : 1;
    return 0;
}

/* raises nothing: false on any sqlite error (out left empty) */
static bool fragments_at_raw(AvpArena *a, sqlite3 *con, i64 address, const char *level, FragAts *out) {
    const char *q = level ? "select o.address, o.row, f.* from occurrences o join fragments f on f.id = o.fragment"
                            " where o.fn_address = ? and f.level = ? order by o.address, f.length desc, f.nfuncs desc"
                          : "select o.address, o.row, f.* from occurrences o join fragments f on f.id = o.fragment"
                            " where o.fn_address = ? order by o.address, f.length desc, f.nfuncs desc";
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(con, q, -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int64(st, 1, address);
    if (level) sqlite3_bind_text(st, 2, level, -1, SQLITE_TRANSIENT);
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        FragAt x;
        x.address = sqlite3_column_int64(st, 0);
        x.f = frag_of_stmt(a, st, 2);
        avp_vec_push(out, x);
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE;
}

static FragAts select_at(AvpArena *a, FragAts rows, i64 min_functions, bool overlaps) {
    FragAts cands = {0};
    for (size_t i = 0; i < rows.n; i++)
        if (rows.v[i].f->nfuncs >= min_functions) avp_vec_push(&cands, rows.v[i]);
    avp_vec_free(&rows);
    if (overlaps) return cands;
    FragAt *sorted = avp_alloc(a, (cands.n ? cands.n : 1) * sizeof *sorted);
    if (cands.n) memcpy(sorted, cands.v, cands.n * sizeof *sorted);
    avp_sort(sorted, cands.n, sizeof *sorted, cmp_score_addr, NULL, false);
    AVP_VEC(Picked) picked = {0};
    for (size_t i = 0; i < cands.n; i++) {
        i64 s = sorted[i].address, l = sorted[i].f->length, e = s + 4 * l;
        bool ok = true;
        for (size_t k = 0; k < picked.n && ok; k++) {
            i64 lo = s > picked.v[k].a ? s : picked.v[k].a, hi = e < picked.v[k].e ? e : picked.v[k].e;
            i64 ml = l < picked.v[k].l ? l : picked.v[k].l;
            if (!(hi - lo < 2 * ml)) ok = false;
        }
        if (ok) avp_vec_push(&picked, ((Picked){s, e, l}));
    }
    FragAts kept = {0};
    AVP_VEC(Picked) seen = {0};
    for (size_t i = 0; i < cands.n; i++) {
        i64 s = cands.v[i].address, l = cands.v[i].f->length;
        bool keep = false;
        for (size_t k = 0; k < picked.n && !keep; k++)
            if (picked.v[k].a == s && picked.v[k].a + 4 * picked.v[k].l == s + 4 * l) keep = true;
        if (!keep) continue;
        bool dup = false;
        for (size_t k = 0; k < seen.n && !dup; k++)
            if (seen.v[k].a == s && seen.v[k].l == l) dup = true;
        if (dup) continue;
        avp_vec_push(&seen, ((Picked){s, 0, l}));
        avp_vec_push(&kept, cands.v[i]);
    }
    avp_vec_free(&picked);
    avp_vec_free(&seen);
    avp_vec_free(&cands);
    return kept;
}

FragAts frag_fragments_at(AvpArena *a, sqlite3 *con, i64 address, const char *level, i64 min_functions, bool overlaps) {
    FragAts rows = {0};
    if (!fragments_at_raw(a, con, address, level, &rows)) frag_sql_fail(con);
    return select_at(a, rows, min_functions, overlaps);
}

bool fragments_in_function(AvpArena *a, const char *root, long long address, long long min_functions,
                           const char *level, FragmentsInFunction *out) {
    memset(out, 0, sizeof *out);
    const ShsmRepo *r = shsm_repo_find(root);
    const char *path = frag_db_path(a, r);
    if (!avp_isfile(path)) return false;
    sqlite3 *con = NULL;
    if (sqlite3_open_v2(path, &con, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        sqlite3_close(con);
        return false;
    }
    sqlite3_busy_timeout(con, 5000);
    FragAts rows = {0};
    bool ok = fragments_at_raw(a, con, (i64)address, level, &rows);
    sqlite3_close(con);
    if (!ok) {
        avp_vec_free(&rows);
        return false;
    }
    FragAts found = select_at(a, rows, (i64)min_functions, false);
    for (size_t i = 0; i < found.n; i++) {
        const Frag *f = found.v[i].f;
        FragmentInFunction x;
        memset(&x, 0, sizeof x);
        x.id = f->id;
        x.level = f->level;
        x.rank = f->rank;
        x.length = f->length;
        x.nfuncs = f->nfuncs;
        x.nunits = f->nunits;
        x.nocc = f->nocc;
        x.score = f->score;
        x.pattern = f->pattern;
        x.offset = found.v[i].address - (i64)address;
        avp_vec_push(out, x);
    }
    avp_vec_free(&found);
    return true;
}
