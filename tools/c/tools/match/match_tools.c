/* match: compiling with the dependency-keyed cache, the compiled listing (cached on the object's bytes), objdiff
 * percentages, the rule checker and gatecheck layout (tools/match.py compile_unit .. run_layout). */
#include "match.h"

#include "gatecheck.h"
#include "rules_cli.h"
#include "shsm_cache.h"

void match_raise_cc(const CcError *err) {
    int code = SHSM_EXC_USAGE;
    switch (err->kind) {
    case CC_ERR_KEY: code = SHSM_EXC_KEY; break;
    case CC_ERR_OS: code = SHSM_EXC_OS; break;
    case CC_ERR_VALUE: code = SHSM_EXC_VALUE; break;
    default: break;
    }
    shsm_raise(code, "%.*s", AVP_SF(err->msg));
}


static const char **cz_argv(AvpArena *a, const AvpStrVec *v) {
    const char **out = AVP_NEWN(a, const char *, v->n + 1);
    for (size_t i = 0; i < v->n; i++) out[i] = avp_cz(a, v->v[i]);
    return out;
}

/* ---- compiling ------------------------------------------------------------------------------------------------- */

static char *obj_key_of(AvpArena *a, const char *manifest_key, const MatchDeps *d) {
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, MATCH_CACHE_VERSION);
    shsm_key_strz(&k, "obj");
    shsm_key_strz(&k, manifest_key);
    shsm_key_repr(&k, shsm_repr_str_pairs(a, d->paths, d->digests, d->n));
    return shsm_key_hex(a, &k);
}

MatchCompiled match_compile_unit(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source,
                                 const char *workdir, const char *const *extra, size_t nextra, bool use_cache) {
    if (!avp_makedirs(workdir, true)) shsm_raise_io();
    const char *obj = shsm_object_for(a, source, workdir);
    ShsmCompileOpts opts = {0};
    opts.extra_flags = extra;
    opts.nextra = nextra;
    opts.sjis = -1;
    opts.depfile = true;
    AvpStrVec argv = shsm_compile_argv(a, r, u, source, obj, &opts);
    double t0 = avp_time();
    ShsmCache *cache = use_cache ? shsm_cache_new(NULL) : NULL;
    MatchCompiled c = {0};
    c.obj = obj;
    c.argv = argv;
    char *manifest_key = NULL;
    if (cache) {
        MatchDeps *hit = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            AvpStr absw = avp_str(avp_path_abspath(a, workdir));
            AvpStr *norm = AVP_NEWN(a, AvpStr, argv.n + 1);
            for (size_t i = 0; i < argv.n; i++) norm[i] = avp_str_replace(a, argv.v[i], absw, AVP_S("<WORK>"), -1);
            AvpStr tp[2], td[2];
            size_t nt = 0;
            for (size_t i = 0; i < argv.n && i < 2; i++) {
                char *p = avp_cz(a, argv.v[i]);
                if (avp_str_endswith(avp_str_lower(a, argv.v[i]), AVP_S(".exe")) && avp_exists(p)) {
                    tp[nt] = argv.v[i];
                    td[nt] = avp_str(shsm_file_digest(a, p));
                    nt++;
                }
            }
            ShsmKey k;
            shsm_key_init(&k);
            shsm_key_strz(&k, MATCH_CACHE_VERSION);
            shsm_key_strz(&k, "manifest");
            shsm_key_str_list(&k, norm, argv.n);
            shsm_key_repr(&k, shsm_repr_str_pairs(a, tp, td, nt));
            shsm_key_strz(&k, shsm_file_digest(a, source));
            manifest_key = shsm_key_hex(a, &k);
            AvpStr raw;
            if (shsm_cache_get(a, cache, manifest_key, &raw)) {
                if (!avp_u8_valid(raw)) shsm_raise(SHSM_EXC_VALUE, "'utf-8' codec can't decode the manifest");
                AvpJsonError jerr;
                AvpJson *deps = avp_json_parse(a, raw, &jerr);
                if (!deps) shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(avp_json_error_str(a, &jerr)));
                size_t n = avp_json_len(deps);
                bool all = avp_json_is_arr(deps);
                for (size_t i = 0; all && i < n; i++)
                    if (!avp_exists(avp_cz(a, avp_json_str_or(avp_json_at(deps, i), AVP_S(""))))) all = false;
                if (all) {
                    MatchDeps *d = AVP_NEW(a, MatchDeps);
                    d->paths = AVP_NEWN(a, AvpStr, n + 1);
                    d->digests = AVP_NEWN(a, AvpStr, n + 1);
                    for (size_t i = 0; i < n; i++) {
                        d->paths[i] = avp_json_str_or(avp_json_at(deps, i), AVP_S(""));
                        d->digests[i] = avp_str(shsm_file_digest(a, avp_cz(a, d->paths[i])));
                    }
                    d->n = n;
                    if (shsm_cache_restore(cache, obj_key_of(a, manifest_key, d), obj)) hit = d;
                }
            }
            avp_try_end(&t);
        } else {
            if (!shsm_exc_is_os(t.code) && !shsm_exc_is_value(t.code)) avp_throw(t.code, "%s", t.msg);
            manifest_key = NULL;
            hit = NULL;
        }
        if (hit) {
            c.ok = true;
            c.output = AVP_S("");
            c.seconds = avp_time() - t0;
            c.cached = true;
            c.deps = hit;
            return c;
        }
    }
    if (avp_exists(obj) && !avp_remove(obj)) shsm_raise_io();
    ShsmRun res = shsm_run(a, cz_argv(a, &argv), argv.n, r->root, 300);
    c.ok = shsm_run_ok(&res) && avp_exists(obj);
    c.deps = c.ok ? match_depfile_digests(a, r, source, workdir) : NULL;
    if (c.deps && cache && manifest_key) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            char *ok_key = obj_key_of(a, manifest_key, c.deps);
            AvpJson *list = avp_json_arr(a);
            for (size_t i = 0; i < c.deps->n; i++) avp_json_arr_push(a, list, avp_json_str(a, c.deps->paths[i]));
            AvpStr text = avp_json_dumps(a, list, NULL);
            shsm_cache_put(cache, manifest_key, text.p, text.n);
            shsm_cache_store(cache, ok_key, obj);
            avp_try_end(&t);
        } else if (!shsm_exc_is_os(t.code)) {
            avp_throw(t.code, "%s", t.msg);
        }
    }
    AvpStr output = avp_str_concat(a, res.out, res.err);
    if (res.timed_out) output = avp_str_concat(a, output, AVP_S("\n(compiler timed out)"));
    c.output = output;
    c.seconds = avp_time() - t0;
    c.cached = false;
    return c;
}

AvpStrVec match_first_error_lines(AvpArena *a, AvpStr output, i64 n) {
    AvpStrVec lines = avp_str_splitlines(output, false);
    AvpStrVec keep = {0};
    int skip = 0;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = lines.v[i];
        if (!avp_str_strip(ln).n) continue;
        ln = avp_str_rstrip(ln);
        /* MWCC's usage warning for the deprecated -multibyte spelling, which every compile prints */
        if (avp_str_contains(ln, AVP_S("Usage Warning"))) {
            skip = 2;
            continue;
        }
        if (skip && avp_str_startswith(ln, AVP_S("#"))) {
            skip--;
            continue;
        }
        skip = 0;
        avp_vec_push(&keep, ln);
    }
    AvpStrVec out = shsm_cap_lines(a, keep.v, keep.n, n, false, "more compiler output");
    avp_vec_free(&keep);
    avp_vec_free(&lines);
    return out;
}

/* ---- the compiled listing (compare.load_compiled with cache=True) ---------------------------------------------- */

typedef struct {
    AvpArena *a;
    const ShsmRepo *r;
    ShsmCache *cache;
    char *key;
} ListingCache;

static char *listing_key(AvpArena *a, const ShsmRepo *r, AvpStr data) {
    const char *dtk = shsm_repo_tool(a, r, "dtk");
    ShsmKey k;
    shsm_key_init(&k);
    shsm_key_strz(&k, "compare.listing");
    shsm_key_int(&k, 1); /* compare.LISTING_CACHE_VERSION */
    shsm_key_strz(&k, avp_exists(dtk) ? shsm_file_digest(a, dtk) : "");
    shsm_key_bytes(&k, data.p, data.n);
    return shsm_key_hex(a, &k);
}

static bool listing_get(void *ctx, AvpStr obj_bytes, AvpStr *listing) {
    ListingCache *c = ctx;
    c->key = listing_key(c->a, c->r, obj_bytes);
    return shsm_cache_get(c->a, c->cache, c->key, listing);
}

static void listing_put(void *ctx, AvpStr obj_bytes, AvpStr listing) {
    ListingCache *c = ctx;
    if (!c->key) c->key = listing_key(c->a, c->r, obj_bytes);
    AvpTry t;
    if (AVP_TRY(&t)) {
        shsm_cache_put(c->cache, c->key, listing.p, listing.n);
        avp_try_end(&t);
    } else if (!shsm_exc_is_os(t.code)) {
        avp_throw(t.code, "%s", t.msg);
    }
}

AvpMap *match_load_listing(AvpArena *a, const ShsmRepo *r, const char *obj, bool use_cache, const char *out_dir) {
    CcRepo cr = {r->root, r->version};
    ListingCache lc = {a, r, use_cache ? shsm_cache_new(NULL) : NULL, NULL};
    CcListingCache seam = {&lc, listing_get, listing_put};
    CcError err = {0};
    AvpMap *m = cc_load_compiled(a, &cr, obj, out_dir, use_cache ? &seam : NULL, NULL, &err);
    if (!m) match_raise_cc(&err);
    return m;
}

/* ---- objdiff ----------------------------------------------------------------------------------------------------- */

AvpMap *match_objdiff_percents(const ShsmRepo *r, const char *target_obj, const char *base_obj, const char *workdir) {
    AvpArena *a = avp_arena_new(0);
    const char *exe = shsm_repo_tool(a, r, "objdiff-cli");
    if (!target_obj || !avp_exists(target_obj) || !avp_exists(exe)) {
        avp_arena_free(a);
        return NULL;
    }
    const char *out = avp_path_join(a, workdir, "objdiff.json");
    const char *argv[] = {exe, "diff", "-1", target_obj, "-2", base_obj, "-c", "functionRelocDiffs=none", "-o", out};
    ShsmRun res = shsm_run(a, argv, AVP_ARRAY_LEN(argv), r->root, 120);
    if (!shsm_run_ok(&res) || !avp_exists(out)) {
        avp_arena_free(a);
        return NULL;
    }
    AvpStr text;
    if (!avp_read_text(a, out, AVP_TM_UTF8, &text)) shsm_raise_io();
    AvpJsonError jerr;
    AvpJson *doc = avp_json_parse(a, text, &jerr);
    if (!doc) shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(avp_json_error_str(a, &jerr)));
    AvpMap *pct = avp_map_new();
    const AvpJson *syms = avp_json_getz(avp_json_getz(doc, "right"), "symbols");
    for (size_t i = 0; avp_json_is_arr(syms) && i < avp_json_len(syms); i++) {
        const AvpJson *s = avp_json_at(syms, i);
        AvpStr kind = avp_json_get_str(s, "kind", AVP_S(""));
        AvpStr name = avp_json_get_str(s, "name", AVP_S(""));
        if (!avp_str_eqz(kind, "SYMBOL_FUNCTION") || !name.n) continue;
        const AvpJson *mp = avp_json_getz(s, "match_percent");
        avp_map_put(pct, name, NULL)->f = avp_json_truthy(mp) ? avp_json_f64(mp, 0.0) : 0.0;
    }
    avp_arena_free(a);
    return pct;
}

/* ---- rules and data layout --------------------------------------------------------------------------------------- */

/* str(v) of a JSON value as Python's %s gives it. */
static AvpStr py_str(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    if (v->kind == AVP_JSON_STRING) return v->u.str;
    if (v->kind == AVP_JSON_BOOL) return v->b ? AVP_S("True") : AVP_S("False");
    if (v->kind == AVP_JSON_FLOAT) return avp_repr_f64(a, v->u.f);
    return avp_json_dumps(a, v, NULL);
}

static AvpJson *rules_failure(AvpArena *a, AvpStr error) {
    AvpJson *rr = avp_json_obj(a);
    avp_json_set(a, rr, AVP_S("ok"), avp_json_bool(false));
    avp_json_set(a, rr, AVP_S("pending"), avp_json_obj(a));
    avp_json_set(a, rr, AVP_S("findings"), avp_json_arr(a));
    avp_json_set(a, rr, AVP_S("error"), avp_json_str(a, error));
    return rr;
}

AvpJson *match_run_rules(AvpArena *a, const ShsmRepo *r, const char *unit, const char *file) {
    /* check_rules --json (--units main/<unit> | <file>), run in-process (tool_check_rules) */
    RlRunOpts o = {0};
    o.root = r->root;
    o.json = true;
    if (file) avp_vec_push(&o.paths, avp_str(avp_path_abspath(a, file)));
    else avp_vec_push(&o.units, avp_str(shsm_unit_name(a, unit)));
    AvpBuf out, err;
    avp_buf_init(&out, a);
    avp_buf_init(&err, a);
    rl_run(a, &o, &out, &err);
    AvpJsonError jerr;
    AvpJson *doc = avp_json_parse(a, avp_buf_str(&out), &jerr);
    if (!doc) {
        AvpStr all = avp_str_concat(a, avp_buf_str(&out), avp_buf_str(&err));
        AvpStrVec first = match_first_error_lines(a, all, 3);
        return rules_failure(a, first.n ? first.v[0] : AVP_S(""));
    }
    AvpStr target;
    if (file) {
        char *rel = NULL;
        if (!avp_path_relpath(a, avp_path_abspath(a, file), r->root, &rel)) shsm_raise_io();
        target = avp_str_replace(a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1);
    } else {
        const char *src = shsm_get_unit(r, unit)->source;
        target = avp_str_replace(a, avp_str(src ? src : ""), AVP_S("\\"), AVP_S("/"), -1);
    }
    AvpJson *pending = avp_json_obj(a), *findings = avp_json_arr(a);
    const AvpJson *fns = avp_json_getz(doc, "functions");
    for (size_t i = 0; avp_json_is_arr(fns) && i < avp_json_len(fns); i++) {
        const AvpJson *fn = avp_json_at(fns, i);
        const AvpJson *mr = avp_json_getz(fn, "match_rules");
        AvpStr path = avp_str_replace(a, avp_json_get_str(fn, "path", AVP_S("")), AVP_S("\\"), AVP_S("/"), -1);
        if (!avp_json_truthy(mr) || !avp_str_eq(path, target)) continue;
        const AvpJson *nm = avp_json_getz(fn, "name");
        AvpStr key = nm && nm->kind == AVP_JSON_STRING ? nm->u.str : AVP_S("null");
        AvpJson *list = avp_json_get(pending, key);
        if (!list) {
            list = avp_json_arr(a);
            avp_json_set(a, pending, key, list);
        }
        for (size_t k = 0; avp_json_is_arr(mr) && k < avp_json_len(mr); k++)
            avp_json_arr_push(a, list, match_json_copy(a, avp_json_at(mr, k)));
    }
    const AvpJson *fds = avp_json_getz(doc, "findings");
    for (size_t i = 0; avp_json_is_arr(fds) && i < avp_json_len(fds); i++) {
        const AvpJson *f = avp_json_at(fds, i);
        if (!avp_str_eqz(avp_json_get_str(f, "tier", AVP_S("")), "match")) continue;
        const AvpJson *st = avp_json_getz(f, "status");
        if (st && st->kind == AVP_JSON_STRING && avp_str_eqz(st->u.str, "allowed")) continue;
        const AvpJson *func = avp_json_getz(f, "function");
        AvpStr fpart = AVP_S("");
        if (avp_json_truthy(func)) {
            AvpStr fs = py_str(a, func);
            fpart = avp_pyfmt(a, " (%S)", &fs);
        }
        AvpStr p = py_str(a, avp_json_getz(f, "path")), l = py_str(a, avp_json_getz(f, "line"));
        AvpStr ru = py_str(a, avp_json_getz(f, "rule")), m = py_str(a, avp_json_getz(f, "message"));
        avp_json_arr_push(a, findings, avp_json_str(a, avp_pyfmt(a, "%S:%S: %S [match] %S%S", &p, &l, &ru, &m, &fpart)));
    }
    /* r13_note: the checker's reason for skipping its R13 vtable checks, or None */
    const AvpJson *note = avp_json_getz(avp_json_getz(doc, "r13"), "note");
    AvpJson *r13 = avp_json_null();
    if (avp_json_truthy(note) && note->kind == AVP_JSON_STRING && avp_str_contains(note->u.str, AVP_S("skipped")))
        r13 = avp_json_str(a, note->u.str);
    AvpJson *rr = avp_json_obj(a);
    avp_json_set(a, rr, AVP_S("ok"), avp_json_bool(true));
    avp_json_set(a, rr, AVP_S("pending"), pending);
    avp_json_set(a, rr, AVP_S("findings"), findings);
    avp_json_set(a, rr, AVP_S("error"), avp_json_null());
    avp_json_set(a, rr, AVP_S("r13_note"), r13);
    return rr;
}

AvpStr match_rules_verdict(AvpArena *a, const AvpJson *rr) {
    if (!avp_json_truthy(avp_json_getz(rr, "ok"))) return AVP_S("rules error");
    AvpStr v = avp_pyfmt(a, "rules %d [match]", (i64)avp_json_len(avp_json_getz(rr, "findings")));
    if (avp_json_truthy(avp_json_getz(rr, "r13_note"))) v = avp_str_concat(a, v, AVP_S(", R13 NOT CHECKED"));
    return v;
}

AvpJson *match_run_layout(AvpArena *a, const ShsmRepo *r, const char *unit, const char *obj) {
    /* gatecheck layout <unit> --object <obj>, run in-process (tool_gatecheck); stdout's lines, then stderr's */
    AvpStrVec out = {0}, err = {0};
    int rc = gc_layout_lines(a, r->root, shsm_unit_path(a, unit), avp_path_abspath(a, obj), &out, &err);
    AvpJson *lines = avp_json_arr(a);
    for (int pass = 0; pass < 2; pass++) {
        const AvpStrVec *v = pass ? &err : &out;
        for (size_t i = 0; i < v->n; i++) {
            AvpStrVec sub = avp_str_splitlines(v->v[i], false);
            for (size_t k = 0; k < sub.n; k++)
                if (avp_str_strip(sub.v[k]).n) avp_json_arr_push(a, lines, avp_json_str(a, avp_str_rstrip(sub.v[k])));
            avp_vec_free(&sub);
        }
    }
    avp_vec_free(&out);
    avp_vec_free(&err);
    const char *status = rc == 0 ? "ok" : rc == 1 ? "differs" : "error";
    AvpJson *lay = avp_json_obj(a);
    avp_json_set(a, lay, AVP_S("status"), avp_json_strz(a, status));
    avp_json_set(a, lay, AVP_S("lines"), lines);
    return lay;
}

/* ---- assembly units ---------------------------------------------------------------------------------------------- */

/* An assembly source unit (`.s`) is assembled by build.ninja's `as` rule (binutils as, then dtk elf fixup), never by
 * MWCC: ask ninja for the unit's object, which is the object `--built` reads. Ninja keeps it current from the source
 * and its macros.inc, so nothing is cached here. ok is false with the output when the assembler fails. */
MatchCompiled match_assemble_unit(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u) {
    MatchCompiled c = {0};
    if (!u->base_obj) shsm_raise(SHSM_EXC_USAGE, "%s has no object in build.ninja (regenerate it with `%s`)", u->name, SHSM_REGEN_COMMAND);
    const char *ninja = shsm_repo_tool(a, r, "ninja");
    if (!avp_exists(ninja)) shsm_raise(SHSM_EXC_USAGE, "%s is missing: it assembles %s", ninja, u->source);
    const char *argv[3] = {ninja, shsm_repo_rel(a, r, u->base_obj), NULL};
    double t0 = avp_time();
    ShsmRun res = shsm_run(a, argv, 2, r->root, 300);
    c.ok = shsm_run_ok(&res) && avp_exists(u->base_obj);
    c.obj = u->base_obj;
    c.output = avp_str_concat(a, res.out, res.err);
    if (res.timed_out) c.output = avp_str_concat(a, c.output, AVP_S("\n(assembler timed out)"));
    c.seconds = avp_time() - t0;
    return c;
}
