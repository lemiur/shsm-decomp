/* brief.py World: what the briefing reads, each part loaded on first use. */
#include "brief.h"
#include "match.h"
#include "pick.h"
#include "shsm_mangle.h"
#include "shsm_re.h"
#include "twins.h"
#include "wall_walls.h"

#include <sqlite3.h>
#include <string.h>

static AvpArena *A(void) { return avp_arena(); }

BriefWorld *brief_world_new(const char *root) {
    AvpArena *a = A();
    BriefWorld *W = AVP_NEW(a, BriefWorld);
    W->a = a;
    W->repo = shsm_repo_find(root);
    W->root = W->repo->root;
    W->state_cache = avp_map_new();
    W->inputs_cache = avp_map_new();
    W->header_of_cache = avp_map_new();
    W->source_cache = avp_map_new();
    return W;
}

static QueryCtx *ctx_of(BriefWorld *W) {
    if (!W->ctx) W->ctx = query_ctx_for_root(W->a, W->root);
    return W->ctx;
}

void brief_world_close(BriefWorld *W) {
    if (W->twins_db) {
        sqlite3_close((sqlite3 *)W->twins_db);
        W->twins_db = NULL;
    }
    if (W->ctx) query_ctx_close(W->ctx);
}

ShsmAsmIndex *brief_ix(BriefWorld *W) { return query_ix(ctx_of(W)); }

static const ShsmUnits *units_of(BriefWorld *W) {
    if (!W->units_) W->units_ = shsm_load_units(W->repo);
    return W->units_;
}

const ShsmSplits *brief_splits(BriefWorld *W) {
    if (!W->splits_) W->splits_ = shsm_load_splits(W->repo);
    return W->splits_;
}

const ShsmReport *brief_report(BriefWorld *W) {
    if (W->report_loaded) return W->report_;
    for (int k = 0; k < 2 && !W->report_; k++) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            W->report_ = k == 0 ? shsm_load_report(W->repo) : shsm_load_baseline(W->repo);
            avp_try_end(&t);
        }
    }
    W->report_loaded = true;
    return W->report_;
}

const ShsmFunctionReport *brief_report_at(BriefWorld *W, i64 address) {
    if (!W->report_by_addr_) {
        W->report_by_addr_ = avp_imap_new();
        const ShsmReport *rp = brief_report(W);
        for (size_t u = 0; rp && u < rp->nunits; u++)
            for (size_t i = 0; i < rp->units[u]->nfunctions; i++) {
                ShsmFunctionReport *f = &rp->units[u]->functions[i];
                if (f->has_address) avp_imap_put(W->report_by_addr_, f->address, NULL)->p = f;
            }
    }
    AvpVal *v = avp_imap_get(W->report_by_addr_, (u64)address);
    return v ? (const ShsmFunctionReport *)v->p : NULL;
}

const char *brief_library(BriefWorld *W, i64 address) {
    const char *lib = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (!W->libs_loaded) {
            W->libs_loaded = true;
            W->libs_ = shsm_load_libraries(W->repo);
        }
        if (W->libs_) lib = shsm_libraries_lookup(W->a, W->libs_, address);
        avp_try_end(&t);
    }
    return lib;
}

const char *brief_reference(const char *lib, const char **section) {
    *section = NULL;
    if (!lib) return "?";
    return pick_reference_flag(lib, section);
}

const ShsmUnit *brief_unit(BriefWorld *W, const char *unit_name) {
    if (!unit_name || !*unit_name) return NULL;
    return shsm_units_lookup(units_of(W), unit_name);
}

const ShsmUnit *brief_source_unit(BriefWorld *W, const char *unit_name) {
    const ShsmUnit *u = brief_unit(W, unit_name);
    return u && u->source && *u->source ? u : NULL;
}

bool brief_matched(BriefWorld *W, i64 address, const char *unit_name) {
    const ShsmFunctionReport *f = brief_report_at(W, address);
    return f && f->fuzzy >= 100.0 && brief_source_unit(W, unit_name);
}

const AvpJson *brief_match_state(BriefWorld *W, const char *unit_name) {
    bool created;
    AvpVal *v = avp_map_put(W->state_cache, avp_str(unit_name), &created);
    if (created) {
        AvpJson *st = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            st = match_load_state(W->a, W->repo, unit_name);
            avp_try_end(&t);
        }
        avp_map_get(W->state_cache, avp_str(unit_name))->p = st;
        return st;
    }
    return (const AvpJson *)v->p;
}

static bool mtime_rel(BriefWorld *W, const char *rel, double *t) {
    return rel && avp_getmtime(avp_path_join(W->a, W->root, rel), t);
}

bool brief_inputs_current(BriefWorld *W, const char *unit_name, const AvpJson *inputs, double recorded_time) {
    if (inputs && avp_json_is_obj(inputs) && avp_json_len(inputs)) {
        const char *key = avp_sprintf(W->a, "%s|%p", unit_name, (const void *)inputs);
        AvpVal *v = avp_map_get(W->inputs_cache, avp_str(key));
        if (v) return v->i != 0;
        bool ok = false;
        AvpTry t;
        if (AVP_TRY(&t)) {
            ok = wall_inputs_staleness(W->a, W->repo, unit_name, inputs).n == 0;
            avp_try_end(&t);
        }
        avp_map_put(W->inputs_cache, avp_str(key), NULL)->i = ok;
        return ok;
    }
    const ShsmUnit *u = brief_source_unit(W, unit_name);
    double src_t;
    if (!u || !mtime_rel(W, u->source, &src_t)) return true;
    return recorded_time >= src_t;
}

static double num_or_zero(const AvpJson *v) {
    if (!v || !avp_json_is_num(v)) return 0.0;
    return avp_json_f64(v, 0.0);
}

const char *brief_unit_name_of(const ShsmSymRow *s) {
    if (!s->unit.p) return NULL;
    return shsm_symrow_unit_name(A(), s);
}

static bool json_eq_int(const AvpJson *v, i64 x) {
    if (!v) return false;
    if (v->kind == AVP_JSON_INT && !v->big) return v->u.i == x;
    if (v->kind == AVP_JSON_FLOAT) return v->u.f == (double)x;
    return false;
}

BriefScore brief_score(BriefWorld *W, const ShsmSymRow *s) {
    AvpArena *a = W->a;
    BriefScore r = {0};
    const ShsmUnit *u = brief_source_unit(W, brief_unit_name_of(s));
    if (!u) return r;
    double src_t = 0;
    bool has_src_t = mtime_rel(W, u->source, &src_t);
    const AvpJson *st = brief_match_state(W, u->name);
    if (brief_truthy(st) &&
        brief_inputs_current(W, u->name, avp_json_getz(st, "inputs"), num_or_zero(avp_json_getz(st, "time")))) {
        const AvpJson *fns = avp_json_getz(st, "functions");
        for (size_t i = 0; fns && avp_json_is_obj(fns) && i < avp_json_len(fns); i++) {
            const AvpJson *rec = avp_json_member(fns, i)->val;
            if (!avp_json_is_obj(rec)) continue;
            AvpStr rn = brief_jstr(rec, "retail_name");
            if (!(json_eq_int(avp_json_getz(rec, "address"), s->address) || (rn.p && avp_str_eq(rn, s->name))))
                continue;
            AvpTm tm;
            AvpStr when = {0};
            avp_localtime(avp_json_f64(avp_json_getz(st, "time"), 0.0), &tm);
            avp_strftime(a, "%Y-%m-%d %H:%M", &tm, &when);
            const AvpJson *status = avp_json_getz(rec, "status");
            bool exact = status && avp_json_is_str(status) && avp_str_eqz(status->u.str, "EXACT");
            const AvpJson *pct = avp_json_getz(rec, "percent");
            r.percent = exact ? avp_json_float(a, 100.0) : (pct ? (AvpJson *)pct : avp_json_null());
            r.origin = avp_cz(a, avp_pyfmt(a, "match.exe %s %S", status ? brief_py_strz(status) : "?", &when));
            r.exact = exact;
            return r;
        }
    }
    const ShsmFunctionReport *f = brief_report_at(W, s->address);
    if (!f) {
        r.percent = avp_json_null();
        r.origin = "not in the report";
        return r;
    }
    const char *origin = "objdiff report";
    const ShsmReport *rp = brief_report(W);
    double rp_t = 0;
    if (rp && rp->path && avp_getmtime(rp->path, &rp_t) && has_src_t && src_t && rp_t && rp_t < src_t)
        origin = "objdiff report, older than the source";
    r.percent = avp_json_float(a, f->fuzzy);
    r.origin = origin;
    r.exact = f->fuzzy >= 100.0;
    return r;
}

const AvpJson *brief_classes(BriefWorld *W) {
    if (!W->classes_loaded) {
        W->classes_loaded = true;
        const char *path = avp_path_join(W->a, W->repo->version_build_dir, "classes.json");
        if (!avp_isfile(path)) {
            W->classes_exc = SHSM_EXC_FILE_NOT_FOUND;
            W->classes_msg = avp_sprintf(W->a, "no %s (build/tools/classes.exe scan)", shsm_repo_rel(W->a, W->repo, path));
        } else {
            AvpStr text;
            if (!avp_read_text(W->a, path, AVP_TM_UTF8, &text)) {
                W->classes_exc = shsm_io_exc_code();
                W->classes_msg = avp_cz(W->a, avp_io_err_str(W->a));
            } else {
                AvpJsonError err;
                W->classes_ = avp_json_parse(W->a, text, &err);
                if (!W->classes_) {
                    W->classes_exc = SHSM_EXC_VALUE;
                    W->classes_msg = avp_cz(W->a, avp_json_error_str(W->a, &err));
                }
            }
        }
    }
    if (W->classes_exc) shsm_raise(W->classes_exc, "%s", W->classes_msg);
    return W->classes_;
}

static AvpJson *load_json_file(BriefWorld *W, const char *path, double *mtime) {
    AvpStr text;
    if (!avp_read_text(W->a, path, AVP_TM_UTF8, &text)) return NULL;
    AvpJson *d = avp_json_parse(W->a, text, NULL);
    if (!d || !avp_getmtime(path, mtime)) return NULL;
    return d;
}

static bool str_in_list(const AvpJson *list, const char *s) {
    for (size_t i = 0; list && avp_json_is_arr(list) && i < avp_json_len(list); i++) {
        const AvpJson *v = avp_json_at(list, i);
        if (avp_json_is_str(v) && avp_str_eqz(v->u.str, s)) return true;
    }
    return false;
}

const AvpJson *brief_wall_entry(BriefWorld *W, const char *unit_name, AvpStr name) {
    if (!W->walls_loaded) {
        W->walls_loaded = true;
        W->walls_ = load_json_file(W, avp_path_join(W->a, W->repo->build_dir, "walls.json"), &W->walls_mtime);
    }
    const AvpJson *w = W->walls_;
    const ShsmUnit *u = brief_source_unit(W, unit_name);
    if (!w || !avp_json_is_obj(w) || !u || !str_in_list(avp_json_getz(w, "units"), unit_name)) return NULL;
    const AvpJson *inputs = avp_json_getz(w, "inputs");
    const AvpJson *inp = brief_truthy(inputs) && avp_json_is_obj(inputs) ? avp_json_getz(inputs, unit_name) : NULL;
    if (!brief_inputs_current(W, unit_name, inp, W->walls_mtime)) return NULL;
    const AvpJson *fns = avp_json_getz(w, "functions");
    for (size_t i = 0; fns && i < avp_json_len(fns); i++) {
        const AvpJson *f = avp_json_at(fns, i);
        AvpStr fu = brief_jstr(f, "unit");
        if (!fu.p || !avp_str_eqz(fu, unit_name)) continue;
        AvpStr rn = brief_jstr(f, "retail_name"), n = brief_jstr(f, "name");
        if ((rn.p && avp_str_eq(rn, name)) || (n.p && avp_str_eq(n, name))) return f;
    }
    AvpJson *ex = avp_json_obj(W->a);
    brief_set(ex, "exact", avp_json_bool(true));
    brief_set(ex, "verdict", avp_json_strz(W->a, "EXACT"));
    return ex;
}

const AvpJson *brief_eqcheck_entry(BriefWorld *W, const char *unit_name, AvpStr name) {
    if (!W->eq_loaded) {
        W->eq_loaded = true;
        W->eq_ = load_json_file(W, avp_path_join(W->a, W->repo->build_dir, "eqcheck.json"), &W->eq_mtime);
    }
    if (!W->eq_ || !brief_inputs_current(W, unit_name, NULL, W->eq_mtime)) return NULL;
    const AvpJson *fns = avp_json_is_obj(W->eq_) ? avp_json_getz(W->eq_, "functions") : NULL;
    for (size_t i = 0; fns && i < avp_json_len(fns); i++) {
        const AvpJson *f = avp_json_at(fns, i);
        AvpStr fu = brief_jstr(f, "unit"), fn = brief_jstr(f, "function");
        if (fu.p && avp_str_eqz(fu, unit_name) && fn.p && avp_str_eq(fn, name)) return f;
    }
    return NULL;
}

/* ---- twins ---- */

void *brief_twins_db(BriefWorld *W) {
    if (!W->twins_db) W->twins_db = twins_open_db(W->repo, brief_ix(W), false, 0);
    return W->twins_db;
}

static AvpStr col_text(sqlite3_stmt *st, int i) {
    const unsigned char *p = sqlite3_column_text(st, i);
    if (!p) return (AvpStr){0};
    return avp_str_dup(A(), avp_strn((const char *)p, (size_t)sqlite3_column_bytes(st, i)));
}

static AVP_NORETURN void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

bool brief_twins_fps(BriefWorld *W, i64 address, const char *fp[3]) {
    sqlite3 *db = (sqlite3 *)brief_twins_db(W);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "select exact, ref, shape from fingerprints where address = ?", -1, &st, NULL) !=
        SQLITE_OK)
        sql_fail(db);
    sqlite3_bind_int64(st, 1, address);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    if (found)
        for (int i = 0; i < 3; i++) {
            AvpStr t = col_text(st, i);
            fp[i] = t.p ? avp_cz(A(), t) : NULL;
        }
    sqlite3_finalize(st);
    return found;
}

BriefTwinRows brief_twins_rows(BriefWorld *W, int level, const char *fp, bool ordered, i64 not_address) {
    sqlite3 *db = (sqlite3 *)brief_twins_db(W);
    BriefTwinRows out = {0};
    const char *lv = TWINS_LEVELS[level];
    const char *sql =
        ordered ? avp_sprintf(A(), "select address, name, unit, size from fingerprints where %s = ? order by address", lv)
                : avp_sprintf(A(), "select address, name, unit, size from fingerprints where %s = ? and address != ?",
                              lv);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    if (fp)
        sqlite3_bind_text(st, 1, fp, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(st, 1);
    if (!ordered) sqlite3_bind_int64(st, 2, not_address);
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        BriefTwinRow r;
        r.address = sqlite3_column_int64(st, 0);
        AvpStr n = col_text(st, 1), u = col_text(st, 2);
        r.name = n.p ? avp_cz(A(), n) : NULL;
        r.unit = u.p ? avp_cz(A(), u) : NULL;
        r.size = sqlite3_column_type(st, 3) == SQLITE_NULL ? SHSM_NONE : sqlite3_column_int64(st, 3);
        avp_vec_push(&out, r);
    }
    if (rc != SQLITE_DONE) sql_fail(db);
    sqlite3_finalize(st);
    return out;
}

/* ---- headers and source lines ---- */

static AvpStr read_replace(const char *path, bool *ok) {
    AvpStr text = {0};
    *ok = avp_read_text(A(), path, AVP_TM_UTF8_REPLACE, &text);
    return text;
}

static void load_headers(BriefWorld *W) {
    if (W->headers_loaded) return;
    W->headers_loaded = true;
    const char *top = W->repo->include_dir;
    AvpWalk *wk = avp_walk_open(top, true);
    if (!wk) return;
    AvpWalkItem it = {0};
    while (avp_walk_next(wk, &it)) {
        for (size_t i = 0; i < it.files.n; i++) {
            AvpStr fn = it.files.v[i];
            if (!(avp_str_endswith(fn, AVP_S(".h")) || avp_str_endswith(fn, AVP_S(".hpp")) ||
                  avp_str_endswith(fn, AVP_S(".inc"))))
                continue;
            const char *p = avp_path_join(W->a, avp_cz(W->a, it.dirpath), avp_cz(W->a, fn));
            bool ok;
            AvpStr text = read_replace(p, &ok);
            if (!ok) continue;
            char *rel = NULL;
            if (!avp_path_relpath(W->a, p, top, &rel)) continue;
            AvpStr r = avp_str_replace(W->a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1);
            avp_vec_push(&W->header_paths, r);
            avp_vec_push(&W->header_texts, text);
        }
    }
    avp_walk_close(wk);
}

/* mangle.parse(name): its base name and last scope part (*cls p NULL without a scope); false: None. */
static bool parse_name(const char *name, const char **base, const char **cls) {
    ShsmDemangled *d = shsm_mangle_parse(A(), name);
    *cls = NULL;
    if (!d) return false;
    *base = d->name;
    if (d->nscope) *cls = d->scope[d->nscope - 1];
    return true;
}

const char *brief_header_of(BriefWorld *W, const char *name) {
    AvpVal *cached = avp_map_get(W->header_of_cache, avp_str(name));
    if (cached) return (const char *)cached->p;
    AvpArena *a = W->a;
    const char *base = name, *cls = NULL;
    parse_name(name, &base, &cls);
    const char *pat;
    if (cls) {
        AvpStr c = avp_str(cls);
        AvpStr before, after;
        avp_str_partition(c, AVP_S("<"), &before, &after);
        AvpStr esc = shsm_re_escape(a, before);
        pat = avp_sprintf(a, "^\\s*(?:class|struct)\\s+%.*s\\b[^;]*$", AVP_SF(esc));
    } else {
        AvpStr esc = shsm_re_escape(a, avp_str(base));
        pat = avp_sprintf(a, "^[^\\n;{}()#/]*\\b%.*s\\s*\\([^;{]*\\)\\s*(?:const\\s*)?[;{]", AVP_SF(esc));
    }
    ShsmRe *rx = shsm_re_try(pat, SHSM_RE_M);
    load_headers(W);
    const char *best = NULL;
    size_t best_len = 0;
    AvpStr b = avp_str(base);
    for (size_t i = 0; i < W->header_paths.n; i++) {
        AvpStr text = W->header_texts.v[i];
        if (avp_str_find(text, b, 0, AVP_END) < 0) continue;
        ShsmReMatch m;
        if (!shsm_re_search(rx, text, 0, (i64)text.n, &m)) continue;
        size_t len = avp_u8_len(W->header_paths.v[i]);
        if (!best || len < best_len) {
            best = avp_cz(a, W->header_paths.v[i]);
            best_len = len;
        }
    }
    avp_map_put(W->header_of_cache, avp_str(name), NULL)->p = (void *)best;
    return best;
}

static const AvpStrVec *source_lines(BriefWorld *W, const char *source) {
    bool created;
    AvpVal *v = avp_map_put(W->source_cache, avp_str(source), &created);
    if (!created) return (const AvpStrVec *)v->p;
    bool ok;
    AvpStr text = read_replace(avp_path_join(W->a, W->root, source), &ok);
    AvpStrVec *lines = NULL;
    if (ok) {
        lines = AVP_NEW(W->a, AvpStrVec);
        *lines = avp_str_split(text, AVP_S("\n"), -1);
    }
    avp_map_get(W->source_cache, avp_str(source))->p = lines;
    return lines;
}

static bool first_is_alpha(AvpStr s) {
    if (!s.n) return false;
    return avp_str_isalpha(avp_u8_slice(s, 0, 1));
}

i64 brief_source_line(BriefWorld *W, const char *source, i64 address, const char *name) {
    AvpArena *a = W->a;
    const AvpStrVec *lines = source_lines(W, source);
    if (!lines) return SHSM_NONE;
    AvpStr tag = avp_pyfmt(a, "0x%08X", address);
    for (size_t k = 0; k < lines->n; k++) {
        AvpStr ln = lines->v[k];
        if (avp_str_find(ln, AVP_S("//"), 0, AVP_END) < 0) continue;
        AvpStr up = avp_str_replace(a, avp_str_upper(a, ln), AVP_S("0X"), AVP_S("0x"), -1);
        if (avp_str_find(up, tag, 0, AVP_END) < 0) continue;
        if (avp_str_startswith(avp_str_lstrip(ln), AVP_S("}"))) {
            size_t j = k;
            while (j > 0 && !(first_is_alpha(lines->v[j]) && avp_str_find(lines->v[j], AVP_S("("), 0, AVP_END) >= 0))
                j--;
            return (i64)j + 1;
        }
        return (i64)k + 1;
    }
    const char *base = name, *cls;
    parse_name(name, &base, &cls);
    AvpStr esc = shsm_re_escape(a, avp_str(base));
    ShsmRe *rx = shsm_re_try(avp_sprintf(a, "\\b%.*s\\s*\\(", AVP_SF(esc)), 0);
    for (size_t k = 0; k < lines->n; k++) {
        AvpStr ln = lines->v[k];
        ShsmReMatch m;
        if (shsm_re_search(rx, ln, 0, (i64)ln.n, &m) && !avp_str_startswith(ln, AVP_S(" ")) &&
            !avp_str_startswith(ln, AVP_S("\t")))
            return (i64)k + 1;
    }
    return SHSM_NONE;
}
