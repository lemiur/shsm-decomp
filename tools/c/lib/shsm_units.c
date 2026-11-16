/* Translation units (tools/common/units.py). */
#include "shsm_units.h"

#include "avp/fs.h"
#include "avp/map.h"
#include "shsm_symbols.h"
#include "shsm_text.h"

#include <windows.h>

/* ---- configure.py ---- */

/* \s*,\s*"([^"]+)" at i: the offset after the closing quote, or 0 with no match. */
static size_t match_source_tail(AvpStr t, size_t i, AvpStr *src) {
    i = shsm_skip_ws(t, i);
    if (i >= t.n || t.p[i] != ',') return 0;
    i = shsm_skip_ws(t, i + 1);
    if (i >= t.n || t.p[i] != '"') return 0;
    size_t s = ++i;
    while (i < t.n && t.p[i] != '"') i++;
    if (i >= t.n || i == s) return 0;
    *src = avp_str_slice(t, (i64)s, (i64)i);
    return i + 1;
}

size_t shsm_parse_configure_status(AvpArena *a, AvpStr text, ShsmConfStatus **out) {
    AVP_VEC(ShsmConfStatus) v = {0};
    AvpMap *index = avp_map_new();
    i64 pos = 0;
    for (;;) {
        i64 at = avp_str_find(text, AVP_S("Object("), pos, AVP_END);
        if (at < 0) break;
        size_t i = shsm_skip_ws(text, (size_t)at + 7);
        AvpStr rest = avp_str_slice(text, (i64)i, AVP_END);
        const char *status = NULL;
        AvpStr src = {0};
        size_t end = 0;
        /* the alternation in order: Matching | NonMatching | Equivalent | MatchingFor\([^)]*\) */
        static const char *const plain[] = {"Matching", "NonMatching", "Equivalent"};
        for (size_t k = 0; k < 3 && !end; k++) {
            AvpStr w = avp_str(plain[k]);
            if (avp_str_startswith(rest, w) && (end = match_source_tail(text, i + w.n, &src)) != 0) status = plain[k];
        }
        if (!end && avp_str_startswith(rest, AVP_S("MatchingFor("))) {
            i64 close = avp_str_find(text, AVP_S(")"), (i64)(i + 12), AVP_END);
            if (close >= 0 && (end = match_source_tail(text, (size_t)close + 1, &src)) != 0) status = "MatchingFor";
        }
        if (!end) {
            pos = at + 1;
            continue;
        }
        pos = (i64)end;
        const char *source = avp_cz(a, src);
        const char *path = shsm_unit_path(a, source);
        bool created = false;
        AvpVal *slot = avp_map_put(index, avp_str(path), &created);
        if (created) {
            slot->i = (i64)v.n;
            avp_vec_push_zero(&v);
        }
        v.v[slot->i] = (ShsmConfStatus){path, status, source};
    }
    avp_map_free(index);
    *out = v.v;
    return v.n;
}

/* ---- build.ninja ---- */

AvpStr shsm_join_ninja_lines(AvpArena *a, AvpStr t) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t i = 0;
    while (i < t.n) {
        size_t d = i;
        if (t.p[i] == ' ' && i + 1 < t.n && t.p[i + 1] == '$') d = i + 1;
        if (t.p[d] == '$') {
            size_t k = d + 1;
            if (k < t.n && t.p[k] == '\r') k++;
            if (k < t.n && t.p[k] == '\n') {
                avp_buf_putc(&b, ' ');
                i = shsm_skip_ws(t, k + 1);
                continue;
            }
        }
        avp_buf_putc(&b, t.p[i]);
        i++;
    }
    return avp_buf_str(&b).p ? avp_buf_str(&b) : AVP_S("");
}

/* _ninja_unescape: re.sub(r"\$(.)", r"\1", s) */
static AvpStr ninja_unescape(AvpArena *a, AvpStr s) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] == '$' && i + 1 < s.n && s.p[i + 1] != '\n') i++;
        avp_buf_putc(&b, s.p[i]);
    }
    return b.n ? avp_buf_str(&b) : AVP_S("");
}

/* _NINJA_SRC = ^# (\S+): (\S+) \(linked (True|False)\)\s*$ */
static bool ninja_src_match(AvpStr s, AvpStr *source, AvpStr *lib, bool *linked) {
    if (!avp_str_startswith(s, AVP_S("# "))) return false;
    size_t i = 2, w1 = 2;
    while (i < s.n && !shsm_ws_len(s, i)) i++;
    if (i - w1 < 2 || s.p[i - 1] != ':' || i >= s.n || s.p[i] != ' ') return false;
    *source = avp_str_slice(s, (i64)w1, (i64)(i - 1));
    size_t w2 = ++i;
    while (i < s.n && !shsm_ws_len(s, i)) i++;
    if (i == w2) return false;
    *lib = avp_str_slice(s, (i64)w2, (i64)i);
    AvpStr rest = avp_str_slice(s, (i64)i, AVP_END);
    size_t k;
    if (avp_str_startswith(rest, AVP_S(" (linked True)"))) *linked = true, k = 14;
    else if (avp_str_startswith(rest, AVP_S(" (linked False)"))) *linked = false, k = 15;
    else return false;
    return shsm_skip_ws(rest, k) == rest.n;
}

size_t shsm_parse_ninja_compiles(AvpArena *a, AvpStr text, ShsmNinjaCompile **out) {
    AVP_VEC(ShsmNinjaCompile) v = {0};
    AvpMap *index = avp_map_new();
    AvpStrVec lines = avp_str_splitlines(shsm_join_ninja_lines(a, text), false);
    bool pending = false;
    AvpStr p_src = {0}, p_lib = {0};
    bool p_linked = false;
    AvpStrVec comment = {0};
    size_t i = 0;
    while (i < lines.n) {
        AvpStr line = lines.v[i];
        if (avp_str_startswith(line, AVP_S("# "))) {
            /* ninja_syntax wraps long comments at spaces: '# <source>: <lib> (linked\n# True)' */
            avp_vec_push(&comment, avp_str_strip(avp_str_slice(line, 2, AVP_END)));
            size_t from = comment.n > 3 ? comment.n - 3 : 0;
            for (size_t k = from; k < comment.n; k++) {
                AvpStr joined = avp_str_join(a, AVP_S(" "), comment.v + k, comment.n - k);
                AvpStr cand = avp_str_concat(a, AVP_S("# "), joined);
                AvpStr src, lib;
                bool linked;
                if (ninja_src_match(cand, &src, &lib, &linked)) {
                    pending = true;
                    p_src = src;
                    p_lib = lib;
                    p_linked = linked;
                    avp_vec_clear(&comment);
                    break;
                }
            }
            i++;
            continue;
        }
        avp_vec_clear(&comment);
        if (avp_str_startswith(line, AVP_S("build ")) && pending) {
            AvpStr head = avp_str_slice(line, 6, AVP_END), outs, rest;
            avp_str_partition(head, AVP_S(": "), &outs, &rest);
            AvpStrVec toks = avp_str_split(rest, AVP_S(""), -1);
            AvpStr rule = toks.n ? toks.v[0] : AVP_S("");
            avp_vec_free(&toks);
            if (avp_str_startswith(rule, AVP_S("mwcc"))) {
                ShsmNinjaCompile info = {0};
                info.source = avp_cz(a, p_src);
                info.library = avp_cz(a, p_lib);
                info.linked = p_linked;
                info.object = avp_cz(a, avp_str_replace(a, avp_str_strip(outs), AVP_S("\\"), AVP_S("/"), -1));
                info.rule = avp_cz(a, rule);
                size_t j = i + 1;
                while (j < lines.n && avp_str_startswith(lines.v[j], AVP_S("  "))) {
                    AvpStr k, val;
                    avp_str_partition(avp_str_strip(lines.v[j]), AVP_S(" = "), &k, &val);
                    if (avp_str_eqz(k, "mw_version")) {
                        info.mw_version =
                            avp_cz(a, avp_str_replace(a, ninja_unescape(a, val), AVP_S("\\"), AVP_S("/"), -1));
                    } else if (avp_str_eqz(k, "cflags")) {
                        const char *err = NULL;
                        AvpStrVec words = {0};
                        if (!shsm_shlex_split(a, ninja_unescape(a, val), &words, &err))
                            shsm_raise(SHSM_EXC_VALUE, "%s", err);
                        avp_vec_free(&info.cflags);
                        info.cflags = words;
                        info.has_cflags = true;
                    }
                    j++;
                }
                info.path = shsm_unit_path(a, info.source);
                bool created = false;
                AvpVal *slot = avp_map_put(index, avp_str(info.path), &created);
                if (created) {
                    slot->i = (i64)v.n;
                    avp_vec_push_zero(&v);
                }
                v.v[slot->i] = info;
                pending = false;
                i = j;
                continue;
            }
        }
        i++;
    }
    avp_vec_free(&comment);
    avp_vec_free(&lines);
    avp_map_free(index);
    *out = v.v;
    return v.n;
}

/* ---- the unit table ---- */

typedef struct {
    bool ok[3];
    i64 size[3], mtime_ns[3];
} Stamp;

static Stamp stamp_of(const ShsmRepo *r) {
    Stamp s;
    memset(&s, 0, sizeof s);
    const char *paths[3] = {r->objdiff_json, r->configure_py, r->build_ninja};
    for (int i = 0; i < 3; i++) {
        AvpStat st;
        if (avp_stat(paths[i], &st)) {
            s.ok[i] = true;
            s.size[i] = st.size;
            s.mtime_ns[i] = st.mtime_ns;
        }
    }
    return s;
}

typedef struct {
    Stamp stamp;
    ShsmUnits *units;
} Loaded;

static SRWLOCK g_units_lock = SRWLOCK_INIT;
static AvpMap *g_loaded; /* normcase(root) -> Loaded * */

static const char *absp(AvpArena *a, const ShsmRepo *r, AvpStr p) {
    if (!p.n) return NULL;
    return avp_path_normpath(a, avp_join(a, r->root, avp_cz(a, p)));
}

static AvpStr read_text_or_raise(AvpArena *a, const char *path) {
    AvpStr t;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &t)) shsm_raise_io();
    return t;
}

static void units_add(ShsmUnits *us, AvpMap *index, ShsmUnit *u) {
    bool created = false;
    AvpVal *slot = avp_map_put(index, avp_str(u->name), &created);
    if (created) {
        slot->i = (i64)us->n;
        avp_vec_push(us, u);
    } else {
        us->v[slot->i] = u;
    }
}

static ShsmUnits *parse_units(const ShsmRepo *r) {
    AvpArena *a = avp_arena();
    ShsmConfStatus *status = NULL;
    size_t nstatus = 0;
    if (avp_exists(r->configure_py))
        nstatus = shsm_parse_configure_status(a, read_text_or_raise(a, r->configure_py), &status);
    ShsmNinjaCompile *ninja = NULL;
    size_t nninja = 0;
    if (avp_exists(r->build_ninja)) nninja = shsm_parse_ninja_compiles(a, read_text_or_raise(a, r->build_ninja), &ninja);
    AvpJson *od_units = NULL;
    if (avp_exists(r->objdiff_json)) {
        AvpJson *doc = avp_json_parse(a, read_text_or_raise(a, r->objdiff_json), NULL);
        od_units = avp_json_getz(doc, "units");
    }
    AvpMap *st_index = avp_map_new(), *nj_index = avp_map_new();
    for (size_t i = 0; i < nstatus; i++) avp_map_put(st_index, avp_str(status[i].path), NULL)->i = (i64)i;
    for (size_t i = 0; i < nninja; i++) avp_map_put(nj_index, avp_str(ninja[i].path), NULL)->i = (i64)i;

    ShsmUnits *us = AVP_NEW(a, ShsmUnits);
    AvpMap *index = avp_map_new();
    us->index = index;
    size_t nod = avp_json_is_arr(od_units) ? avp_json_len(od_units) : 0;
    for (size_t k = 0; k < nod; k++) {
        AvpJson *e = avp_json_at(od_units, k);
        AvpJson *jn = avp_json_getz(e, "name");
        if (!avp_json_is_str(jn)) shsm_raise(SHSM_EXC_KEY, "name");
        AvpJson *meta = avp_json_getz(e, "metadata");
        ShsmUnit *u = AVP_NEW(a, ShsmUnit);
        u->name = avp_cz(a, jn->u.str);
        u->path = shsm_unit_path(a, u->name);
        AvpVal *sv = avp_map_get(st_index, avp_str(u->path));
        const ShsmConfStatus *st = sv ? &status[sv->i] : NULL;
        AvpVal *nv = avp_map_get(nj_index, avp_str(u->path));
        const ShsmNinjaCompile *nj = nv ? &ninja[nv->i] : NULL;
        AvpJson *sp = avp_json_getz(meta, "source_path");
        if (avp_json_truthy(sp) && avp_json_is_str(sp)) u->source = avp_cz(a, sp->u.str);
        else if (st) u->source = avp_sprintf(a, "src/%s", st->source);
        u->status = (u->source && st) ? st->status : "Asm";
        u->library = nj ? nj->library : NULL;
        u->linked = nj ? nj->linked : false;
        u->complete = avp_json_truthy(avp_json_getz(meta, "complete"));
        u->auto_generated = avp_json_truthy(avp_json_getz(meta, "auto_generated"));
        AvpJson *pc = avp_json_getz(meta, "progress_categories");
        for (size_t i = 0; avp_json_is_arr(pc) && i < avp_json_len(pc); i++) {
            AvpJson *c = avp_json_at(pc, i);
            if (avp_json_is_str(c)) avp_vec_push(&u->progress_categories, c->u.str);
        }
        u->target_obj = absp(a, r, avp_json_str_or(avp_json_getz(e, "target_path"), AVP_S("")));
        u->base_obj = absp(a, r, avp_json_str_or(avp_json_getz(e, "base_path"), AVP_S("")));
        u->mw_version = nj ? nj->mw_version : NULL;
        if (nj && nj->has_cflags) {
            u->has_cflags = true;
            u->cflags = nj->cflags;
        }
        u->rule = nj ? nj->rule : NULL;
        u->objdiff = e;
        units_add(us, index, u);
    }
    for (size_t i = 0; i < nstatus; i++) {
        const ShsmConfStatus *st = &status[i];
        const char *n = shsm_unit_name(a, st->path);
        if (avp_map_has(index, avp_str(n))) continue;
        AvpVal *nv = avp_map_get(nj_index, avp_str(st->path));
        const ShsmNinjaCompile *nj = nv ? &ninja[nv->i] : NULL;
        ShsmUnit *u = AVP_NEW(a, ShsmUnit);
        u->name = n;
        u->path = shsm_unit_path(a, n);
        u->source = avp_sprintf(a, "src/%s", st->source);
        u->status = st->status;
        u->library = nj ? nj->library : NULL;
        u->linked = nj ? nj->linked : false;
        u->base_obj = nj ? absp(a, r, avp_str(nj->object)) : NULL;
        /* os.path.join(obj_dir, *p.split("/")) + ".o" */
        AvpStrVec parts = avp_str_split(avp_str(st->path), AVP_S("/"), -1);
        const char **pv = AVP_NEWN(a, const char *, parts.n + 1);
        pv[0] = r->obj_dir;
        for (size_t k = 0; k < parts.n; k++) pv[k + 1] = avp_cz(a, parts.v[k]);
        u->target_obj = avp_sprintf(a, "%s.o", avp_path_joinv(a, parts.n + 1, pv));
        avp_vec_free(&parts);
        u->mw_version = nj ? nj->mw_version : NULL;
        if (nj && nj->has_cflags) {
            u->has_cflags = true;
            u->cflags = nj->cflags;
        }
        u->rule = nj ? nj->rule : NULL;
        units_add(us, index, u);
    }
    avp_map_free(st_index);
    avp_map_free(nj_index);
    return us;
}

const ShsmUnits *shsm_load_units(const ShsmRepo *r) {
    Stamp stamp = stamp_of(r);
    AvpStr key = avp_str(avp_path_normcase(avp_arena(), r->root));
    AcquireSRWLockExclusive(&g_units_lock);
    if (!g_loaded) g_loaded = avp_map_new();
    AvpVal *v = avp_map_get(g_loaded, key);
    Loaded *hit = v ? (Loaded *)v->p : NULL;
    ShsmUnits *us = (hit && memcmp(&hit->stamp, &stamp, sizeof stamp) == 0) ? hit->units : NULL;
    ReleaseSRWLockExclusive(&g_units_lock);
    if (us) return us;
    us = parse_units(r); /* may raise: outside the lock */
    Loaded *l = AVP_NEW(avp_arena(), Loaded);
    l->stamp = stamp;
    l->units = us;
    AcquireSRWLockExclusive(&g_units_lock);
    avp_map_put(g_loaded, key, NULL)->p = l;
    ReleaseSRWLockExclusive(&g_units_lock);
    return us;
}

const ShsmUnit *shsm_units_lookup(const ShsmUnits *us, const char *unit) {
    AvpVal *v = avp_map_get((AvpMap *)us->index, avp_str(shsm_unit_name(avp_arena(), unit)));
    return v ? us->v[v->i] : NULL;
}

/* _in_splits: path in load_splits(repo).units; an OSError or ValueError gives false. */
static bool in_splits(const ShsmRepo *r, const char *path) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmSplits *s = shsm_load_splits(r);
        bool found = shsm_splits_unit(s, path) != NULL;
        avp_try_end(&t);
        return found;
    }
    if (shsm_exc_is_os(t.code) || shsm_exc_is_value(t.code)) return false;
    avp_throw(t.code, "%s", t.msg);
}

const ShsmUnit *shsm_get_unit(const ShsmRepo *r, const char *unit) {
    const ShsmUnits *us = shsm_load_units(r);
    const ShsmUnit *u = shsm_units_lookup(us, unit);
    static const char *const fmt =
        "unit %s is in %s but not in build.ninja (added since build.ninja was generated); regenerate it with `%s`";
    if (!u) {
        if (avp_exists(r->build_ninja) && in_splits(r, shsm_unit_path(avp_arena(), unit)))
            shsm_raise(SHSM_EXC_UNIT_NOT_BUILT, fmt, unit, "splits.txt", SHSM_REGEN_COMMAND);
        shsm_raise(SHSM_EXC_KEY, "unknown unit %s (not in objdiff.json, configure.py or splits.txt)", unit);
    }
    bool objdiff_empty = !u->objdiff || (avp_json_is_obj(u->objdiff) && avp_json_len(u->objdiff) == 0);
    if (objdiff_empty && !u->has_cflags && u->source && *u->source && avp_exists(r->build_ninja))
        shsm_raise(SHSM_EXC_UNIT_NOT_BUILT, fmt, unit, "configure.py", SHSM_REGEN_COMMAND);
    return u;
}

/* ---- compiling ---- */

static AvpStr source_stem(const char *source) {
    AvpStr root, ext;
    avp_path_splitext(avp_cz(avp_arena(), avp_path_basename(source)), &root, &ext);
    return root;
}

const char *shsm_object_for(AvpArena *a, const char *source, const char *out_dir) {
    AvpStr stem = source_stem(source);
    return avp_join(a, avp_path_abspath(a, out_dir), avp_sprintf(a, "%.*s.o", AVP_SF(stem)));
}

const char *shsm_depfile_for(AvpArena *a, const char *source, const char *out_dir) {
    AvpStr stem = source_stem(source);
    return avp_join(a, avp_path_abspath(a, out_dir), avp_sprintf(a, "%.*s.d", AVP_SF(stem)));
}

static const char *depfile_out_dir(AvpArena *a, const char *source, const char *out) {
    char *o = avp_path_abspath(a, out);
    AvpStr os_ = avp_str(o), root, ext;
    avp_path_splitext(o, &root, &ext);
    if (avp_isdir(o) || avp_str_endswith(os_, AVP_S("/")) || avp_str_endswith(os_, AVP_S("\\")) || ext.n == 0) {
        AvpStr s = avp_str_rstrip_chars(os_, AVP_S("/\\"));
        return s.n ? avp_cz(a, s) : o;
    }
    AvpStr stem = source_stem(source);
    const char *want = avp_sprintf(a, "%.*s.o", AVP_SF(stem));
    const char *have = avp_cz(a, avp_path_basename(o));
    if (strcmp(avp_path_normcase(a, have), avp_path_normcase(a, want)) != 0)
        shsm_raise(SHSM_EXC_VALUE, "with depfile=True MWCC names the object after the source: %s must be <dir>/%.*s.o",
                   o, AVP_SF(stem));
    return avp_cz(a, avp_path_dirname(o));
}

AvpStrVec shsm_compile_argv(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source, const char *out,
                            const ShsmCompileOpts *opts) {
    ShsmCompileOpts none = {0};
    none.sjis = -1;
    if (!opts) opts = &none;
    if (shsm_unit_is_asm(u))
        shsm_raise(SHSM_EXC_VALUE, "unit %s is an assembly source unit (%s): assembled by binutils as, not compiled",
                   u->name, u->source);
    if (!u->has_cflags)
        shsm_raise(SHSM_EXC_VALUE,
                   "unit %s has no compile command in build.ninja (asm unit, or regenerate it with `%s`)", u->name,
                   SHSM_REGEN_COMMAND);
    AvpStrVec argv = {0};
    avp_vec_push(&argv, avp_str(shsm_repo_compiler(a, r, opts->mw_version ? opts->mw_version : u->mw_version, NULL)));
    for (size_t i = 0; i < u->cflags.n; i++) {
        bool drop = false;
        for (size_t k = 0; k < opts->ndrop && !drop; k++) drop = avp_str_eqz(u->cflags.v[i], opts->drop_flags[k]);
        if (!drop) avp_vec_push(&argv, u->cflags.v[i]);
    }
    for (size_t k = 0; k < opts->nextra; k++) avp_vec_push(&argv, avp_str(opts->extra_flags[k]));
    const char *out_arg;
    if (opts->depfile) {
        avp_vec_push(&argv, AVP_S("-MMD"));
        out_arg = depfile_out_dir(a, source, out);
    } else {
        out_arg = avp_path_abspath(a, out);
    }
    avp_vec_push(&argv, AVP_S("-c"));
    avp_vec_push(&argv, avp_str(avp_path_abspath(a, source)));
    avp_vec_push(&argv, AVP_S("-o"));
    avp_vec_push(&argv, avp_str(out_arg));
    bool sjis = opts->sjis < 0 ? (u->rule && strncmp(u->rule, "mwcc_sjis", 9) == 0 && avp_exists(r->sjiswrap))
                               : opts->sjis != 0;
    if (sjis) avp_vec_insert(&argv, 0, avp_str(r->sjiswrap));
    return argv;
}

ShsmRun shsm_compile_source(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source, const char *out,
                            const ShsmCompileOpts *opts, double timeout) {
    const char *dir = (opts && opts->depfile) ? depfile_out_dir(a, source, out)
                                              : avp_cz(a, avp_path_dirname(avp_path_abspath(a, out)));
    if (!avp_makedirs(dir, true)) shsm_raise_io();
    AvpStrVec argv = shsm_compile_argv(a, r, u, source, out, opts);
    const char **av = AVP_NEWN(a, const char *, argv.n + 1);
    for (size_t i = 0; i < argv.n; i++) av[i] = avp_cz(a, argv.v[i]);
    ShsmRun res = shsm_run(a, av, argv.n, r->root, timeout);
    avp_vec_free(&argv);
    return res;
}
