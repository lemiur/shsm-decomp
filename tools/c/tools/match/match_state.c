/* match: paths, the state files, state inputs, dependency files and JSON helpers (tools/match.py "Paths" and
 * "Compiling": parse_depfile, depfile_digests). */
#include "match.h"

#include "shsm_cache.h"
#include "shsm_text.h"

/* ---- JSON helpers ---------------------------------------------------------------------------------------------- */

AvpJson *match_jstr(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
AvpJson *match_jf(AvpArena *a, double v) { return avp_json_float(a, v); }
AvpJson *match_ji(AvpArena *a, i64 v) { return avp_json_int(a, v); }

AvpJson *match_json_copy(AvpArena *a, const AvpJson *v) {
    if (!v) return avp_json_null();
    switch ((AvpJsonKind)v->kind) {
    case AVP_JSON_NULL: return avp_json_null();
    case AVP_JSON_BOOL: return avp_json_bool(v->b);
    case AVP_JSON_INT: return v->big ? avp_json_int_digits(a, v->u.str) : avp_json_int(a, v->u.i);
    case AVP_JSON_FLOAT: return avp_json_float(a, v->u.f);
    case AVP_JSON_STRING: return avp_json_str(a, v->u.str);
    case AVP_JSON_ARRAY: {
        AvpJson *out = avp_json_arr_cap(a, v->u.arr.n);
        for (size_t i = 0; i < v->u.arr.n; i++) avp_json_arr_push(a, out, match_json_copy(a, v->u.arr.v[i]));
        return out;
    }
    case AVP_JSON_OBJECT: {
        AvpJson *out = avp_json_obj(a);
        for (size_t i = 0; i < v->u.obj.n; i++)
            avp_json_set(a, out, v->u.obj.v[i].key, match_json_copy(a, v->u.obj.v[i].val));
        return out;
    }
    }
    return avp_json_null();
}

static bool is_none(const AvpJson *v) { return !v || v->kind == AVP_JSON_NULL; }

bool match_json_eq(const AvpJson *x, const AvpJson *y) {
    if (is_none(x) || is_none(y)) return is_none(x) && is_none(y);
    bool xn = x->kind == AVP_JSON_INT || x->kind == AVP_JSON_FLOAT || x->kind == AVP_JSON_BOOL;
    bool yn = y->kind == AVP_JSON_INT || y->kind == AVP_JSON_FLOAT || y->kind == AVP_JSON_BOOL;
    if (xn || yn) {
        if (!(xn && yn)) return false;
        if (x->kind == AVP_JSON_INT && y->kind == AVP_JSON_INT && (x->big || y->big))
            return x->big && y->big && avp_str_eq(x->u.str, y->u.str);
        double a = x->kind == AVP_JSON_BOOL ? (double)x->b : avp_json_f64(x, 0.0);
        double b = y->kind == AVP_JSON_BOOL ? (double)y->b : avp_json_f64(y, 0.0);
        if (x->kind == AVP_JSON_INT && y->kind == AVP_JSON_INT) return x->u.i == y->u.i;
        return a == b;
    }
    if (x->kind != y->kind) return false;
    switch ((AvpJsonKind)x->kind) {
    case AVP_JSON_STRING: return avp_str_eq(x->u.str, y->u.str);
    case AVP_JSON_ARRAY:
        if (x->u.arr.n != y->u.arr.n) return false;
        for (size_t i = 0; i < x->u.arr.n; i++)
            if (!match_json_eq(x->u.arr.v[i], y->u.arr.v[i])) return false;
        return true;
    case AVP_JSON_OBJECT:
        if (x->u.obj.n != y->u.obj.n) return false;
        for (size_t i = 0; i < x->u.obj.n; i++) {
            const AvpJson *other = avp_json_get(y, x->u.obj.v[i].key);
            if (!other || !match_json_eq(x->u.obj.v[i].val, other)) return false;
        }
        return true;
    default: return false;
    }
}

const char *match_jget_cz(AvpArena *a, const AvpJson *obj, const char *key) {
    const AvpJson *v = avp_json_getz(obj, key);
    if (!v || v->kind != AVP_JSON_STRING) return NULL;
    return avp_cz(a, v->u.str);
}

/* ---- paths and state ------------------------------------------------------------------------------------------- */

const char *match_dir(AvpArena *a, const ShsmRepo *r) { return avp_join(a, r->root, "build", "tmp", "match"); }

const char *match_state_path(AvpArena *a, const ShsmRepo *r, const char *unit) {
    AvpStr p = avp_str_replace(a, avp_str(shsm_unit_path(a, unit)), AVP_S("/"), AVP_S("__"), -1);
    return avp_join(a, match_dir(a, r), "state", avp_sprintf(a, "%.*s.json", AVP_SF(p)));
}

AvpJson *match_load_state(AvpArena *a, const ShsmRepo *r, const char *unit) {
    AvpStr text;
    if (!avp_read_text(a, match_state_path(a, r, unit), AVP_TM_UTF8, &text)) {
        avp_io_clear();
        return NULL;
    }
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(a, text, &err);
    if (!doc || doc->kind == AVP_JSON_NULL) return NULL;
    return doc;
}

static const char *slashes(AvpArena *a, const char *p) {
    return avp_cz(a, avp_str_replace(a, avp_str(p), AVP_S("\\"), AVP_S("/"), -1));
}

const char *match_rel_or_abs(AvpArena *a, const ShsmRepo *r, const char *path) {
    char *rel = NULL;
    if (!avp_path_relpath(a, path, r->root, &rel)) { /* another drive */
        avp_io_clear();
        return slashes(a, avp_path_abspath(a, path));
    }
    if (avp_str_startswith(avp_str(rel), AVP_S(".."))) return slashes(a, avp_path_abspath(a, path));
    return slashes(a, rel);
}

const char *match_flags_key(AvpArena *a, const ShsmUnit *u) {
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, u->mw_version);
    if (u->has_cflags) shsm_key_str_list(&k, u->cflags.v, u->cflags.n);
    else shsm_key_none(&k);
    shsm_key_strz(&k, u->rule);
    return shsm_key_hex(a, &k);
}

AvpJson *match_state_inputs(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source,
                            const MatchDeps *deps) {
    if (!deps) return avp_json_null();
    AvpJson *doc = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        AvpJson *files = avp_json_obj(a);
        avp_json_set(a, files, avp_str(match_rel_or_abs(a, r, source)), avp_json_strz(a, shsm_file_digest(a, source)));
        for (size_t i = 0; i < deps->n; i++)
            avp_json_set(a, files, avp_str(match_rel_or_abs(a, r, avp_cz(a, deps->paths[i]))),
                         avp_json_str(a, deps->digests[i]));
        const char *asm_path = shsm_repo_asm_path(a, r, u->name);
        doc = avp_json_obj(a);
        avp_json_set(a, doc, AVP_S("version"), match_ji(a, MATCH_STATE_INPUTS_VERSION));
        avp_json_set(a, doc, AVP_S("source"), avp_json_strz(a, match_rel_or_abs(a, r, source)));
        avp_json_set(a, doc, AVP_S("files"), files);
        avp_json_set(a, doc, AVP_S("flags"), avp_json_strz(a, match_flags_key(a, u)));
        avp_json_set(a, doc, AVP_S("retail"),
                     avp_exists(asm_path) ? avp_json_strz(a, shsm_file_digest(a, asm_path)) : avp_json_null());
        avp_json_set(a, doc, AVP_S("symbols"),
                     avp_exists(r->symbols_txt) ? avp_json_strz(a, shsm_file_digest(a, r->symbols_txt))
                                                : avp_json_null());
        avp_try_end(&t);
        return doc;
    }
    if (shsm_exc_is_os(t.code)) return avp_json_null();
    avp_throw(t.code, "%s", t.msg);
}

void match_save_state(AvpArena *a, const ShsmRepo *r, const char *unit, const AvpJson *result) {
    AvpJson *old = match_load_state(a, r, unit);
    const AvpJson *recs = avp_json_getz(result, "functions");
    size_t nrecs = avp_json_is_arr(recs) ? avp_json_len(recs) : 0;
    AvpJson *funcs = avp_json_obj(a);
    const AvpJson *ofuncs = old ? avp_json_getz(old, "functions") : NULL;
    if (avp_json_is_obj(ofuncs))
        for (size_t i = 0; i < ofuncs->u.obj.n; i++) avp_json_set(a, funcs, ofuncs->u.obj.v[i].key, ofuncs->u.obj.v[i].val);
    for (size_t i = 0; i < nrecs; i++) {
        AvpJson *rec = avp_json_at(recs, i);
        avp_json_set(a, funcs, avp_json_get_str(rec, "name", AVP_S("")), rec);
    }
    AvpJson *inputs = avp_json_getz(result, "inputs");
    if (!inputs) inputs = avp_json_null();
    if (!avp_json_truthy(avp_json_getz(result, "restricted"))) {
        funcs = avp_json_obj(a);
        for (size_t i = 0; i < nrecs; i++) {
            AvpJson *rec = avp_json_at(recs, i);
            avp_json_set(a, funcs, avp_json_get_str(rec, "name", AVP_S("")), rec);
        }
    } else if (!match_json_eq(old ? avp_json_getz(old, "inputs") : NULL, inputs)) {
        inputs = avp_json_null(); /* the kept entries were scored on other inputs */
    }
    const char *path = match_state_path(a, r, unit);
    AvpStr dir = avp_path_dirname(path);
    char *dirz = avp_cz(a, dir);
    if (!avp_makedirs(dirz, true)) shsm_raise_io();
    AvpJson *doc = avp_json_obj(a);
    AvpJson *unit_v = avp_json_getz(result, "unit");
    avp_json_set(a, doc, AVP_S("unit"), unit_v ? unit_v : avp_json_null());
    AvpJson *src = avp_json_getz(result, "source");
    avp_json_set(a, doc, AVP_S("source"), src ? src : avp_json_null());
    avp_json_set(a, doc, AVP_S("time"), match_jf(a, avp_time()));
    avp_json_set(a, doc, AVP_S("inputs"), inputs);
    avp_json_set(a, doc, AVP_S("functions"), funcs);
    AvpJsonDumpOpts opts = shsm_json_opts();
    AvpStr text = avp_json_dumps(a, doc, &opts), bytes;
    if (!avp_encode_text(a, text, AVP_TM_UTF8, &bytes)) shsm_raise_io();
    char *tmp = NULL;
    AvpFile *f = avp_mkstemp(a, "tmp", ".tmp", dirz, &tmp);
    if (!f) shsm_raise_io();
    bool ok = avp_fwrite(f, bytes.p, bytes.n);
    ok = avp_fclose(f) && ok;
    if (!ok || !avp_replace(tmp, path)) shsm_raise_io();
}

/* ---- dependency files ------------------------------------------------------------------------------------------ */

AvpStrVec match_parse_depfile(AvpArena *a, AvpStr text0, const char *cwd) {
    AvpStr text = avp_str_replace(a, text0, AVP_S("\\\r\n"), AVP_S(" "), -1);
    text = avp_str_replace(a, text, AVP_S("\\\n"), AVP_S(" "), -1);
    AvpStrVec words = {0};
    AvpBuf cur;
    avp_buf_init(&cur, a);
    size_t i = 0;
    while (i < text.n) {
        char ch = text.p[i];
        if (ch == '\\' && i + 1 < text.n && text.p[i + 1] == ' ') {
            avp_buf_putc(&cur, ' ');
            i += 2;
            continue;
        }
        size_t ws = shsm_ws_len(text, i);
        if (ws) {
            if (cur.n) {
                avp_vec_push(&words, avp_str_dup(a, avp_buf_str(&cur)));
                avp_buf_clear(&cur);
            }
            i += ws;
            continue;
        }
        avp_buf_putc(&cur, ch);
        i++;
    }
    if (cur.n) avp_vec_push(&words, avp_str_dup(a, avp_buf_str(&cur)));
    AvpStrVec deps = {0};
    AvpSet *seen = avp_set_new();
    bool seen_target = false;
    for (size_t k = 0; k < words.n; k++) {
        AvpStr w = words.v[k];
        if (!seen_target) {
            if (avp_str_endswith(w, AVP_S(":"))) seen_target = true;
            continue;
        }
        char *wz = avp_cz(a, w);
        char *p = avp_path_normpath(a, avp_path_isabs(wz) ? wz : avp_path_join(a, cwd, wz));
        if (avp_set_add(seen, avp_str(p))) avp_vec_push(&deps, avp_str(p));
    }
    avp_set_free(seen);
    avp_vec_free(&words);
    return deps;
}

MatchDeps *match_depfile_digests(AvpArena *a, const ShsmRepo *r, const char *source, const char *out_dir) {
    AvpStr text;
    if (!avp_read_text(a, shsm_depfile_for(a, source, out_dir), AVP_TM_UTF8_REPLACE, &text)) {
        avp_io_clear();
        return NULL;
    }
    AvpStrVec all = match_parse_depfile(a, text, r->root);
    const char *self = avp_path_normcase(a, avp_path_abspath(a, source));
    MatchDeps *d = AVP_NEW(a, MatchDeps);
    d->paths = AVP_NEWN(a, AvpStr, all.n + 1);
    d->digests = AVP_NEWN(a, AvpStr, all.n + 1);
    AvpTry t;
    if (AVP_TRY(&t)) {
        for (size_t i = 0; i < all.n; i++) {
            char *p = avp_cz(a, all.v[i]);
            if (strcmp(avp_path_normcase(a, p), self) == 0) continue;
            d->paths[d->n] = all.v[i];
            d->digests[d->n] = avp_str(shsm_file_digest(a, p));
            d->n++;
        }
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_os(t.code)) avp_throw(t.code, "%s", t.msg);
        d = NULL;
    }
    avp_vec_free(&all);
    return d;
}

/* ---- stale split asm ----------------------------------------------------------------------------------------------- */

const char *match_stale_asm_line(AvpArena *a, const ShsmRepo *r, const char *unit) {
    const char *asm_path = shsm_repo_asm_path(a, r, unit);
    double am = 0, sm = 0;
    if (!avp_exists(asm_path) || !avp_getmtime(asm_path, &am)) return NULL;
    const char *newer = NULL;
    if (r->symbols_txt && avp_getmtime(r->symbols_txt, &sm) && sm > am) newer = "symbols.txt";
    else if (r->splits_txt && avp_getmtime(r->splits_txt, &sm) && sm > am) newer = "splits.txt";
    if (!newer) return NULL;
    return avp_sprintf(a,
                       "warning: the split asm %s is older than config/%s/%s: retail names and MISSING/EXTRA below may be "
                       "stale; run `tools/bin/ninja.exe` to re-split, then match again",
                       shsm_repo_rel(a, r, asm_path), r->version, newer);
}
