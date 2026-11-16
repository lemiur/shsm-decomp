/* levers: the commands (levers.py load_walls, _setup, run_one, cmd_search, cmd_show_apply, cmd_batch, run, main). */
#include "levers_search.h"

#include <string.h>

#include "levers_doc.h"
#include "match.h"
#include "shsm_cache.h"
#include "shsm_report.h"
#include "shsm_symbols.h"

/* ---- walls (tools/wall.py load_walls, match.inputs_staleness) ---- */

static bool num_is_one(const AvpJson *v) {
    if (!v) return false;
    if (v->kind == AVP_JSON_INT) return v->u.i == 1;
    if (v->kind == AVP_JSON_FLOAT) return v->u.f == 1.0;
    if (v->kind == AVP_JSON_BOOL) return v->b;
    return false;
}

/* digest of a file or NULL when it does not exist */
static const char *digest_or_none(AvpArena *a, const char *path) {
    return avp_exists(path) ? shsm_file_digest(a, path) : NULL;
}

static bool jstr_eq(const AvpJson *v, const char *s) {
    if (!s) return !v || v->kind == AVP_JSON_NULL;
    return v && v->kind == AVP_JSON_STRING && avp_str_eqz(v->u.str, s);
}

/* inputs_staleness(repo, unit, inp) != "" (a KeyError from get_unit: stale too): 1 stale, 0 current, -1 when the
 * Python raised (AttributeError, TypeError: load_walls then gives {}) */
static int unit_stale(AvpArena *a, const ShsmRepo *r, const AvpJson *unit, const AvpJson *inp) {
    if (!inp || !avp_json_truthy(inp)) return 1;
    if (inp->kind != AVP_JSON_OBJECT) return -1;
    if (!num_is_one(avp_json_getz(inp, "version"))) return 1;
    if (unit->kind != AVP_JSON_STRING) return -1;
    const ShsmUnit *u = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        u = shsm_get_unit(r, avp_cz(a, unit->u.str));
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_key(t.code)) avp_throw(t.code, "%s", t.msg);
        return 1;
    }
    if (!u->source) return 1;
    const char *src = avp_cz(a, avp_str_replace(a, avp_str(u->source), AVP_S("\\"), AVP_S("/"), -1));
    if (!jstr_eq(avp_json_getz(inp, "source"), src)) return 1;
    if (!jstr_eq(avp_json_getz(inp, "flags"), match_flags_key(a, u))) return 1;
    const AvpJson *files = avp_json_getz(inp, "files");
    if (files && files->kind != AVP_JSON_OBJECT) return -1;
    for (size_t i = 0; files && i < avp_json_len(files); i++) {
        const AvpJsonMember *mb = avp_json_member(files, i);
        const char *p = avp_cz(a, mb->key);
        const char *full = avp_path_isabs(p) ? p : avp_path_join(a, r->root, p);
        if (!avp_isfile(full)) return 1;
        if (!jstr_eq(mb->val, shsm_file_digest(a, full))) return 1;
    }
    if (!jstr_eq(avp_json_getz(inp, "retail"), digest_or_none(a, shsm_repo_asm_path(a, r, u->name)))) return 1;
    if (!jstr_eq(avp_json_getz(inp, "symbols"), digest_or_none(a, r->symbols_txt))) return 1;
    return 0;
}

/* a hashable JSON value as a set key (None, numbers and strings; the Python's set of unit names) */
static AvpStr key_of_value(AvpArena *a, const AvpJson *v) {
    AvpJsonDumpOpts o = {0};
    return v ? avp_json_dumps(a, v, &o) : AVP_S("null");
}

static bool hashable(const AvpJson *v) { return v->kind != AVP_JSON_ARRAY && v->kind != AVP_JSON_OBJECT; }

/* {function name: wall class} of the current units of build/walls.json (levers.load_walls over wall.load_walls:
 * {} when the file is missing, unreadable or malformed) */
static AvpMap *load_walls(AvpArena *a, const ShsmRepo *r) {
    AvpMap *out = avp_map_new(), *none = avp_map_new();
    AvpStr text;
    const char *path = avp_join(a, r->root, "build", "walls.json");
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT), &text)) return out;
    AvpJsonError jerr = {0};
    AvpJson *doc = avp_json_parse(a, text, &jerr);
    if (!doc || doc->kind != AVP_JSON_OBJECT) return out;
    AvpJson *inputs = avp_json_getz(doc, "inputs");
    if (inputs && !avp_json_truthy(inputs)) inputs = NULL;
    AvpSet *stale = avp_set_new();
    AvpJson *units = avp_json_getz(doc, "units");
    if (units && units->kind != AVP_JSON_ARRAY && !(units->kind == AVP_JSON_OBJECT && !avp_json_len(units)))
        return none;
    for (size_t i = 0; units && units->kind == AVP_JSON_ARRAY && i < avp_json_len(units); i++) {
        const AvpJson *u = avp_json_at(units, i);
        if (inputs && inputs->kind != AVP_JSON_OBJECT) return none;
        if (!hashable(u)) return none;
        const AvpJson *inp = inputs && u->kind == AVP_JSON_STRING ? avp_json_get(inputs, u->u.str) : NULL;
        int st = unit_stale(a, r, u, inp);
        if (st < 0) return none;
        if (st) avp_set_add(stale, key_of_value(a, u));
    }
    AvpJson *fns = avp_json_getz(doc, "functions");
    if (fns && fns->kind != AVP_JSON_ARRAY && !(fns->kind == AVP_JSON_OBJECT && !avp_json_len(fns))) return none;
    for (size_t i = 0; fns && fns->kind == AVP_JSON_ARRAY && i < avp_json_len(fns); i++)
        if (avp_json_at(fns, i)->kind != AVP_JSON_OBJECT) return none;
    for (size_t i = 0; fns && fns->kind == AVP_JSON_ARRAY && i < avp_json_len(fns); i++) {
        const AvpJson *f = avp_json_at(fns, i);
        const AvpJson *fu = avp_json_getz(f, "unit");
        if (fu && !hashable(fu)) return none;
        if (avp_set_has(stale, key_of_value(a, fu))) continue;
        const AvpJson *w = avp_json_getz(f, "wall");
        if (!w || !avp_json_truthy(w) || w->kind != AVP_JSON_STRING) continue;
        const char *keys[2] = {"retail_name", "name"};
        for (int k = 0; k < 2; k++) {
            const AvpJson *nm = avp_json_getz(f, keys[k]);
            if (nm && nm->kind == AVP_JSON_STRING && nm->u.str.n)
                avp_map_put(out, nm->u.str, NULL)->p = avp_cz(a, w->u.str);
        }
    }
    avp_set_free(stale);
    return out;
}

static const char *wall_of(AvpMap *walls, const char *symbol) {
    AvpVal *v = avp_map_get(walls, avp_str(symbol));
    return v ? v->p : NULL;
}

/* ---- commands ---- */

typedef struct {
    AvpArena *a;
    AvpArgs *args;
    const ShsmRepo *repo;
} Cmd;

static LvS read_bytes(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) shsm_raise_io();
    return (LvS){(const char *)b.v, b.n};
}

static const char *safe_rel(AvpArena *a, const ShsmRepo *r, const char *path) {
    const char *rel = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rel = shsm_repo_rel(a, r, path);
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_value(t.code)) avp_throw(t.code, "%s", t.msg);
        rel = NULL;
    }
    return rel;
}

static const char *path_join_split(AvpArena *a, const char *root, const char *sub, const char *unit_path,
                                   const char *last) {
    const char *p = avp_join(a, root, "build", sub);
    AvpStrVec parts = avp_str_split(avp_str(unit_path), AVP_S("/"), -1);
    for (size_t i = 0; i < parts.n; i++) p = avp_path_join(a, p, avp_cz(a, parts.v[i]));
    avp_vec_free(&parts);
    return avp_path_join(a, p, last);
}

static const char *out_dir(AvpArena *a, const ShsmRepo *r, const char *unit_path, const char *symbol) {
    return path_join_split(a, r->root, "levers", unit_path, lv_safe_name(a, symbol));
}

static const char *work_dir(AvpArena *a, const ShsmRepo *r, const char *unit_path, const char *symbol) {
    return path_join_split(a, r->root, avp_path_join(a, "tmp", "levers"), unit_path, lv_safe_name(a, symbol));
}

typedef struct {
    AvpJson *doc;
    LvSearch res;
} RunOne;

static RunOne run_one(Cmd *c, const ShsmUnit *u, const char *src, const char *relpath, const char *symbol,
                      double minutes, i64 depth) {
    AvpArena *a = c->a;
    LvS text0 = read_bytes(a, src);
    {
        /* the symbol's address, for the `// 0xADDRESS` lookup when its name finds no definition */
        static ShsmSymbolTable *symtab;
        if (!symtab) symtab = shsm_load_symbols(c->repo, false);
        const ShsmSymbol *ss = shsm_sym_by_name(symtab, avp_str(symbol));
        lv_set_symbol_address(symbol, ss ? ss->address : 0);
    }
    const char *wd = work_dir(a, c->repo, u->path, symbol);
    const char *orig_dir = avp_cz(a, avp_path_dirname(avp_path_join(a, c->repo->root, u->source ? u->source : relpath)));
    const char **extra = avp_alloc(a, sizeof(char *) * 2);
    extra[0] = "-i";
    extra[1] = shsm_repo_rel(a, c->repo, orig_dir);
    LvScorer *sc = AVP_NEW(a, LvScorer);
    sc->repo = c->repo;
    sc->unit = u;
    sc->symbol = symbol;
    sc->basename = avp_cz(a, avp_path_basename(src));
    sc->workroot = wd;
    sc->extra = extra;
    sc->nextra = 2;
    AvpMap *walls = load_walls(a, c->repo);
    AvpStr fam = avp_args_str(c->args, "family");
    const char **fams = NULL;
    size_t nfam = 0;
    if (fam.n) {
        AvpStrVec parts = avp_str_split(fam, AVP_S(","), -1);
        fams = avp_alloc(a, sizeof(char *) * (parts.n + 1));
        for (size_t i = 0; i < parts.n; i++) {
            fams[nfam] = avp_cz(a, lv_strip(parts.v[i]));
            bool known = false;
            for (int k = 0; k < 6; k++)
                if (!strcmp(fams[nfam], LV_FAMILIES[k])) known = true;
            if (!known)
                shsm_raise(SHSM_EXC_USAGE, "unknown family in --family (known: decl, expr, control, temp, type, order)");
            nfam++;
        }
        avp_vec_free(&parts);
    }
    AvpTry t;
    if (AVP_TRY(&t)) {
        lv_ctx_new(a, text0, symbol);
        avp_try_end(&t);
    } else {
        if (t.code == LV_EXC_PARSE)
            shsm_raise(SHSM_EXC_USAGE, "cannot parse %s in %s: %s", symbol, relpath, lv_utf8(a, avp_str(t.msg)));
        avp_throw(t.code, "%s", t.msg);
    }
    LvSearchOpts o = {0};
    o.depth = depth ? depth : avp_args_int(c->args, "depth");
    o.frontier = avp_args_int(c->args, "frontier");
    o.jobs = avp_args_int(c->args, "jobs");
    o.deadline = avp_time() + minutes * 60;
    o.stop_on_exact = avp_args_bool(c->args, "stop_on_exact");
    o.families = fams;
    o.nfam = nfam;
    o.wall = wall_of(walls, symbol);
    RunOne r = {0};
    int code = 0;
    const char *msg = NULL;
    AvpTry t2;
    if (AVP_TRY(&t2)) {
        r.res = lv_search(a, text0, symbol, lv_scorer_score, sc, &o);
        avp_try_end(&t2);
    } else {
        code = t2.code;
        msg = avp_strdup(a, t2.msg);
    }
    avp_rmtree(wd, 1);
    if (code) avp_throw(code, "%s", msg);
    r.doc = lv_write_outputs(a, out_dir(a, c->repo, u->path, symbol), text0, relpath, src, symbol, u->name, &r.res);
    const char *w = wall_of(walls, symbol);
    avp_json_set(a, r.doc, AVP_S("wall"), w ? avp_json_strz(a, w) : avp_json_null());
    return r;
}

static i64 jint(const AvpJson *o, const char *k) {
    const AvpJson *v = avp_json_getz(o, k);
    return v && v->kind == AVP_JSON_INT ? v->u.i : 0;
}

static bool jbool(const AvpJson *o, const char *k) {
    const AvpJson *v = avp_json_getz(o, k);
    return v && avp_json_truthy(v);
}

/* _steps_text of a steps list from the document */
static const char *steps_text_json(AvpArena *a, const AvpJson *steps) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < avp_json_len(steps); i++) {
        const AvpJson *s = avp_json_at(steps, i);
        if (i) avp_buf_put(&b, AVP_S(" + "));
        avp_buf_put(&b, avp_json_get_str(s, "lever", AVP_S("")));
        avp_buf_put(&b, avp_str(avp_sprintf(a, " L%lld", (long long)jint(s, "line"))));
    }
    return avp_buf_cz(&b);
}

static int cmd_show_apply(Cmd *c, const char *odir) {
    AvpArena *a = c->a;
    const char *path = avp_path_join(a, odir, "report.json");
    if (!avp_isfile(path))
        shsm_raise(SHSM_EXC_USAGE, "no report: run the search first (%s)", shsm_repo_rel(a, c->repo, path));
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT), &text)) shsm_raise_io();
    AvpJsonError jerr = {0};
    AvpJson *doc = avp_json_parse(a, text, &jerr);
    if (!doc) shsm_raise(SHSM_EXC_VALUE, "%s", avp_cz(a, avp_json_error_str(a, &jerr)));
    bool show = avp_args_given(c->args, "show") && !avp_args_is_none(c->args, "show");
    i64 n = show ? avp_args_int(c->args, "show") : avp_args_int(c->args, "apply");
    const AvpJson *imp = avp_json_getz(doc, "improving");
    const AvpJson *entry = NULL;
    for (size_t i = 0; imp && i < avp_json_len(imp) && !entry; i++)
        if (jint(avp_json_at(imp, i), "rank") == n) entry = avp_json_at(imp, i);
    if (!entry)
        shsm_raise(SHSM_EXC_USAGE, "no diff %lld (the report has %zu)", (long long)n, imp ? avp_json_len(imp) : 0);
    AvpStr dname = avp_json_get_str(entry, "diff", AVP_S(""));
    const char *dpath = avp_path_join(a, odir, avp_cz(a, dname));
    if (show) {
        LvS d = read_bytes(a, dpath);
        fputs(lv_utf8(a, d), stdout);
        return SHSM_EXIT_OK;
    }
    const char *src = avp_cz(a, avp_json_get_str(doc, "source_path", AVP_S("")));
    AvpStr srcrel = avp_json_get_str(doc, "source", AVP_S(""));
    LvS cur = read_bytes(a, src);
    if (!avp_str_eq(avp_str(lv_sha(a, cur)), avp_json_get_str(doc, "source_sha1", AVP_S(""))))
        shsm_raise(SHSM_EXC_USAGE, "%.*s changed since the search; run it again", AVP_SF(srcrel));
    LvS nw;
    const char *err = NULL;
    if (!lv_apply_patch(a, cur, read_bytes(a, dpath), &nw, &err))
        shsm_raise(SHSM_EXC_USAGE, "%.*s does not apply: %s", AVP_SF(dname), err);
    if (!avp_write_file(src, nw.p, nw.n)) shsm_raise_io();
    printf("applied %.*s to %.*s\n", AVP_SF(dname), AVP_SF(srcrel));
    printf("rule: %s\n", LV_RULE);
    return SHSM_EXIT_OK;
}

static int cmd_search(Cmd *c) {
    AvpArena *a = c->a;
    AvpStr root = avp_args_str(c->args, "root");
    c->repo = shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
    AvpStr tu = avp_args_str(c->args, "target_unit"), fn = avp_args_str(c->args, "function");
    if (!tu.n || !fn.n) shsm_raise(SHSM_EXC_USAGE, "give <unit> <function>, or --batch");
    const char *function = avp_cz(a, fn);
    const ShsmUnit *u = shsm_get_unit(c->repo, avp_cz(a, tu));
    AvpStr file = avp_args_str(c->args, "file");
    if (!u->source && !file.n) shsm_raise(SHSM_EXC_USAGE, "unit %s has no source file", u->name);
    const char *src = file.n ? avp_path_abspath(a, avp_cz(a, file)) : avp_path_join(a, c->repo->root, u->source);
    if (!avp_isfile(src)) shsm_raise(SHSM_EXC_USAGE, "no such source file: %s", src);
    const char *relpath = safe_rel(a, c->repo, src);
    if (!relpath || !strncmp(relpath, "..", 2))
        shsm_raise(SHSM_EXC_USAGE, "the source file must be inside the checkout: %s", src);
    const char *odir = out_dir(a, c->repo, u->path, function);
    if ((avp_args_given(c->args, "show") && !avp_args_is_none(c->args, "show")) ||
        (avp_args_given(c->args, "apply") && !avp_args_is_none(c->args, "apply")))
        return cmd_show_apply(c, odir);
    RunOne r = run_one(c, u, src, relpath, function, avp_args_float(c->args, "minutes"), 0);
    AvpJson *doc = r.doc;
    const AvpJson *base = avp_json_getz(doc, "base");
    const char *wall = avp_json_get_str(doc, "wall", AVP_S("")).p ? avp_cz(a, avp_json_get_str(doc, "wall", AVP_S("")))
                                                                  : NULL;
    ShsmOutput *out = shsm_output_open(c->args, -1, -1, -1);
    shsm_output_linez(out, avp_sprintf(a, "levers: %s %s (source %s, %s wall)", u->name, function, relpath,
                                       wall && *wall ? wall : "unknown"));
    shsm_output_linez(out, avp_sprintf(a, "rule: %s", LV_RULE));
    shsm_output_linez(out, avp_sprintf(a, "base: %lld differing rows, %lld target mismatches (%lld retail rows)%s",
                                       (long long)jint(base, "rows"), (long long)jint(base, "target_mismatches"),
                                       (long long)jint(base, "retail_rows"),
                                       jbool(base, "exact") ? ", already exact" : ""));
    const AvpJson *imp = avp_json_getz(doc, "improving");
    if (avp_json_len(imp)) {
        shsm_output_linez(out, avp_sprintf(a, "%4s %5s %3s %6s  %s", "rank", "rows", "tm", "delta", "lever / source line"));
        for (size_t i = 0; i < avp_json_len(imp); i++) {
            const AvpJson *e = avp_json_at(imp, i);
            const AvpJson *steps = avp_json_getz(e, "steps");
            const AvpJson *last = avp_json_at(steps, avp_json_len(steps) - 1);
            AvpStr desc = avp_json_get_str(last, "desc", AVP_S(""));
            shsm_output_linez(out, avp_sprintf(a, "%4lld %5lld %3lld %+6lld  %s%s: %.*s", (long long)jint(e, "rank"),
                                               (long long)jint(e, "rows"), (long long)jint(e, "target_mismatches"),
                                               (long long)jint(e, "delta"), jbool(e, "exact") ? "EXACT " : "",
                                               steps_text_json(a, steps), AVP_SF(desc)));
        }
    }
    const AvpJson *brk = avp_json_getz(doc, "breaks");
    for (size_t i = 0; i < avp_json_len(brk) && i < 5; i++) {
        const AvpJson *b = avp_json_at(brk, i);
        const AvpJson *names = avp_json_getz(b, "breaks");
        AvpBuf nb;
        avp_buf_init(&nb, a);
        for (size_t k = 0; k < avp_json_len(names) && k < 3; k++) {
            if (k) avp_buf_put(&nb, AVP_S(", "));
            avp_buf_put(&nb, avp_json_at(names, k)->u.str);
        }
        const char *what = jbool(b, "exact") ? "exact" : avp_sprintf(a, "improves (%lld rows)", (long long)jint(b, "rows"));
        shsm_output_linez(out, avp_sprintf(a, "%s but breaks %s: %s", what, avp_buf_cz(&nb),
                                           steps_text_json(a, avp_json_getz(b, "steps"))));
    }
    shsm_output_linez(out, avp_sprintf(a, "candidates: %s; %.1f s, %lld level(s), -j %lld", lv_stats_line(a, r.res.stats),
                                       r.res.seconds, (long long)r.res.levels,
                                       (long long)avp_args_int(c->args, "jobs")));
    shsm_output_linez(out, avp_sprintf(a, "wrote %s (%zu diff(s)); --show N prints one, --apply N applies one",
                                       shsm_repo_rel(a, c->repo, avp_path_join(a, odir, "report.json")),
                                       avp_json_len(imp)));
    for (size_t i = 0; i < avp_json_len(doc); i++)
        shsm_output_data(out, avp_cz(a, avp_json_member(doc, i)->key), avp_json_member(doc, i)->val);
    shsm_output_close(out);
    return SHSM_EXIT_OK;
}

static bool lib_matches(const ShsmUnit *u, const char *lib) {
    if (u->library) {
        if (!strcmp(lib, u->library)) return true;
        AvpStrVec parts = avp_str_split(avp_str(u->library), AVP_S("/"), -1);
        bool hit = false;
        for (size_t i = 0; i < parts.n; i++)
            if (avp_str_eqz(parts.v[i], lib)) hit = true;
        avp_vec_free(&parts);
        if (hit) return true;
    }
    AvpStrVec parts = avp_str_split(avp_str(u->path), AVP_S("/"), -1);
    bool hit = false;
    for (size_t i = 0; i < parts.n; i++)
        if (avp_str_eqz(parts.v[i], lib)) hit = true;
    avp_vec_free(&parts);
    return hit;
}

typedef struct {
    const ShsmUnit *u;
    const ShsmFunctionReport *f;
} Work;

static int cmd_batch(Cmd *c) {
    AvpArena *a = c->a;
    AvpStr root = avp_args_str(c->args, "root");
    c->repo = shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
    ShsmReport *rep = shsm_load_report(c->repo);
    const ShsmUnits *table = shsm_load_units(c->repo);
    AvpStr st = avp_args_str(c->args, "status"), unit = avp_args_str(c->args, "unit"), lib = avp_args_str(c->args, "lib");
    AvpSet *statuses = NULL;
    if (st.n) {
        statuses = avp_set_new();
        AvpStrVec parts = avp_str_split(st, AVP_S(","), -1);
        for (size_t i = 0; i < parts.n; i++) avp_set_add(statuses, avp_str_strip(parts.v[i]));
        avp_vec_free(&parts);
    } else if (!unit.n && !lib.n) {
        statuses = avp_set_new();
        avp_set_add(statuses, AVP_S("NonMatching"));
        avp_set_add(statuses, AVP_S("Equivalent"));
    }
    double minp = avp_args_float(c->args, "min_percent");
    AVP_VEC(Work) work = {0};
    for (size_t i = 0; i < table->n; i++) {
        const ShsmUnit *u = table->v[i];
        if (!u->source || !strcmp(u->status, "Asm") || shsm_unit_is_asm(u)) continue;
        if (statuses && avp_set_len(statuses) && !avp_set_has(statuses, avp_str(u->status))) continue;
        if (unit.n && !strstr(u->path, avp_cz(a, unit))) continue;
        if (lib.n && !lib_matches(u, avp_cz(a, lib))) continue;
        const ShsmUnitReport *ur = shsm_report_unit(rep, u->name);
        if (!ur) continue;
        for (size_t k = 0; k < ur->nfunctions; k++) {
            const ShsmFunctionReport *f = &ur->functions[k];
            AvpVal *first = avp_map_get(ur->first_by_name, avp_str(f->name));
            if (first && (size_t)first->i != k) continue;
            /* a repeated name: the dict keeps the first place and the last entry */
            for (size_t q = k + 1; q < ur->nfunctions; q++)
                if (!strcmp(ur->functions[q].name, f->name)) f = &ur->functions[q];
            if (minp <= f->fuzzy && f->fuzzy < 100) avp_vec_push(&work, ((Work){u, f}));
        }
    }
    if (!work.n)
        shsm_raise(SHSM_EXC_USAGE, "no function at or above %s%% and below 100%% in the selected units",
                   avp_sprintf(a, "%.1f", minp));
    double t_end = avp_time() + avp_args_float(c->args, "minutes") * 60;
    AvpJson *rows = avp_json_arr(a);
    ShsmOutput *out = shsm_output_open(c->args, -1, -1, -1);
    shsm_output_linez(out, avp_sprintf(a, "rule: %s", LV_RULE));
    for (size_t i = 0; i < work.n; i++) {
        const ShsmUnit *u = work.v[i].u;
        const ShsmFunctionReport *f = work.v[i].f;
        double left = t_end - avp_time();
        if (left <= 5) {
            shsm_output_linez(out, avp_sprintf(a, "%s %s: skipped (budget)", u->path, f->name));
            continue;
        }
        double per = left / (double)(work.n - i), cap = left < 30.0 ? left : 30.0;
        double share = (per > cap ? per : cap) / 60.0;
        double mins = share < left / 60.0 ? share : left / 60.0;
        RunOne r = {0};
        AvpTry t;
        bool ok = true;
        const char *emsg = NULL;
        if (AVP_TRY(&t)) {
            const char *src = avp_path_join(a, c->repo->root, u->source);
            r = run_one(c, u, src, shsm_repo_rel(a, c->repo, src), f->name, mins, 1);
            avp_try_end(&t);
        } else {
            int code = t.code;
            if (code == SHSM_EXC_USAGE || code == LV_EXC_PARSE)
                emsg = lv_utf8(a, avp_str(t.msg));
            else if (code == LV_EXC_KEY || shsm_exc_is_key(code))
                emsg = avp_cz(a, shsm_exc_str(a, SHSM_EXC_KEY, t.msg));
            else
                avp_throw(code, "%s", t.msg);
            ok = false;
        }
        if (!ok) {
            AvpStrVec ls = avp_str_splitlines(avp_str(emsg), false);
            AvpStr first = ls.n ? ls.v[0] : AVP_S("");
            if (first.n > 100) first.n = 100;
            shsm_output_linez(out, avp_sprintf(a, "%s %s %.1f%%: error: %.*s", u->path, f->name, f->fuzzy, AVP_SF(first)));
            avp_vec_free(&ls);
            continue;
        }
        const AvpJson *imp = avp_json_getz(r.doc, "improving");
        const AvpJson *best = avp_json_len(imp) ? avp_json_at(imp, 0) : NULL;
        const char *verdict;
        if (best && jbool(best, "exact"))
            verdict = avp_sprintf(a, "EXACT found: %s", steps_text_json(a, avp_json_getz(best, "steps")));
        else if (best) {
            const AvpJson *base = avp_json_getz(r.doc, "base");
            verdict = avp_sprintf(a, "improved %lld -> %lld rows: %s",
                                  (long long)(jint(base, "rows") + jint(base, "target_mismatches")),
                                  (long long)(jint(best, "rows") + jint(best, "target_mismatches")),
                                  steps_text_json(a, avp_json_getz(best, "steps")));
        } else {
            AvpVal *g = avp_map_get(r.res.stats, AVP_S("generated"));
            verdict = avp_sprintf(a, "nothing (%lld candidates)", (long long)(g ? g->i : 0));
        }
        shsm_output_linez(out, avp_sprintf(a, "%s %s %.1f%%: %s", u->path, f->name, f->fuzzy, verdict));
        AvpJson *row = avp_json_obj(a);
        avp_json_set(a, row, AVP_S("unit"), avp_json_strz(a, u->name));
        avp_json_set(a, row, AVP_S("function"), avp_json_strz(a, f->name));
        avp_json_set(a, row, AVP_S("percent"), avp_json_float(a, f->fuzzy));
        avp_json_set(a, row, AVP_S("verdict"), avp_json_strz(a, verdict));
        avp_json_arr_push(a, rows, row);
    }
    shsm_output_data(out, "functions", rows);
    shsm_output_close(out);
    avp_vec_free(&work);
    return SHSM_EXIT_OK;
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    Cmd c = {avp_arena(), args, NULL};
    i64 depth = avp_args_int(args, "depth");
    if (depth < 1 || depth > LV_MAX_DEPTH) shsm_raise(SHSM_EXC_USAGE, "--depth must be 1..%d", LV_MAX_DEPTH);
    int rc = 0;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rc = avp_args_bool(args, "batch") ? cmd_batch(&c) : cmd_search(&c);
        avp_try_end(&t);
    } else {
        /* the levers' own exception classes, as cli.main reports them */
        switch (t.code) {
        case LV_EXC_KEY: shsm_raise(SHSM_EXC_KEY, "%s", t.msg);
        case LV_EXC_PARSE:
        case LV_EXC_INDEX:
        case LV_EXC_TYPE:
        case LV_EXC_ATTR:
        case LV_EXC_DEMANGLE: shsm_raise(SHSM_EXC_VALUE, "%s", lv_utf8(c.a, avp_str(t.msg)));
        default: avp_throw(t.code, "%s", t.msg);
        }
    }
    return rc;
}

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx;
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

int lv_main(int argc, char **argv) {
    AvpArgParser *p = avp_args_parser(
        NULL, &(AvpArgParserDesc){.prog = "levers.exe",
                                  .description = "Lever search: honest single source edits to one function, scored "
                                                 "against retail. " LV_RULE,
                                  .epilog = LV_EPILOG, .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    avp_arg(p, &(AvpArgDesc){.names = "target_unit", .metavar = "unit", .nargs = AVP_NARGS_OPT,
                             .help = "unit (any spelling: game/PadRumble)"});
    avp_arg(p, &(AvpArgDesc){.names = "function", .nargs = AVP_NARGS_OPT, .help = "function symbol (symbols.txt name)"});
    avp_arg(p, &(AvpArgDesc){.names = "--file", .help = "search from this copy of the source instead of the unit's file"});
    avp_arg(p, &(AvpArgDesc){.names = "--depth", .type = AVP_T_INT, .dflt = AVP_DEF_INT(1),
                             .help = "lever combinations to chain (1..4, default 1)"});
    avp_arg(p, &(AvpArgDesc){.names = "--frontier", .type = AVP_T_INT, .dflt = AVP_DEF_INT(6),
                             .help = "distinct states continued per level (default 6)"});
    avp_arg(p, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_INT(shsm_default_jobs()),
                             .help = "parallel compiles"});
    avp_arg(p, &(AvpArgDesc){.names = "--minutes", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(5.0),
                             .help = "hard time budget (default 5)"});
    avp_arg(p, &(AvpArgDesc){.names = "--stop-on-exact", .action = AVP_ARG_STORE_TRUE,
                             .help = "stop at the first exact state"});
    avp_arg(p, &(AvpArgDesc){.names = "--family",
                             .help = "only these lever families (comma list: decl,expr,control,temp,type,order)"});
    avp_arg(p, &(AvpArgDesc){.names = "--show", .type = AVP_T_INT, .metavar = "N", .help = "print diff N of the last run"});
    avp_arg(p, &(AvpArgDesc){.names = "--apply", .type = AVP_T_INT, .metavar = "N",
                             .help = "apply diff N of the last run to the source file"});
    avp_arg(p, &(AvpArgDesc){.names = "--batch", .action = AVP_ARG_STORE_TRUE,
                             .help = "level 1 over every near-matched function"});
    avp_arg(p, &(AvpArgDesc){.names = "--unit", .help = "batch: unit path substring"});
    avp_arg(p, &(AvpArgDesc){.names = "--lib", .help = "batch: library name or path component"});
    avp_arg(p, &(AvpArgDesc){.names = "--status", .help = "batch: unit status list (default NonMatching,Equivalent)"});
    avp_arg(p, &(AvpArgDesc){.names = "--min-percent", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(90.0),
                             .help = "batch: lowest objdiff percent (default 90)"});
    return shsm_cli_main(p, argc, argv, run, NULL);
}
