/* objcache: entries, include probes, the stats log and the wrapper (tools/objcache.py "Include probes", "Entries",
 * "The wrapper"). */
#include "objcache.h"
#include "shsm_cache.h"
#include "shsm_cli.h"

#include <string.h>
#include <windows.h>

/* ================================================================================================= entries */

typedef struct {
    char kind;
    const char *path, *sha;
} Dep;
typedef struct {
    char kind;
    const char *parent, *base;
    bool exists;
} Probe;
typedef struct {
    Dep *deps;
    size_t ndeps;
    Probe *probes;
    size_t nprobes;
    const char *result;
} ManEntry;
typedef AVP_VEC(ManEntry) ManEntries;

static void put_dep(AvpCacheWriter *w, char kind, const char *path, const char *sha) {
    avp_cw_u8(w, (u8)kind);
    avp_cw_cz(w, path);
    avp_cw_cz(w, sha);
}

AvpStr oc_pack_result(AvpArena *a, const OcResult *r) {
    AvpCacheWriter w;
    avp_cw_init(&w);
    avp_cw_count(&w, r->argv.n);
    for (size_t i = 0; i < r->argv.n; i++) avp_cw_str(&w, r->argv.v[i]);
    avp_cw_cz(&w, r->src);
    avp_cw_cz(&w, r->src_sha);
    avp_cw_cz(&w, r->compiler);
    avp_cw_cz(&w, r->target_suffix);
    avp_cw_count(&w, r->dep_paths.n);
    for (size_t i = 0; i < r->dep_paths.n; i++)
        put_dep(&w, r->dep_kinds.v[i].p[0], r->dep_paths.v[i].p, r->dep_shas.v[i].p);
    avp_cw_cz(&w, r->obj_sha);
    avp_cw_bytes(&w, r->out_text.p, r->out_text.n);
    avp_cw_bytes(&w, r->err_text.p, r->err_text.n);
    avp_cw_cz(&w, r->cwd);
    avp_cw_f64(&w, r->created);
    avp_cw_cz(&w, r->manifest);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, OC_RESULT_MAGIC);
    u32 hn = (u32)w.b.n;
    u8 be[4] = {(u8)(hn >> 24), (u8)(hn >> 16), (u8)(hn >> 8), (u8)hn};
    avp_buf_putn(&b, be, 4);
    avp_buf_putn(&b, w.b.v, w.b.n);
    avp_buf_putn(&b, r->obj.p, r->obj.n);
    avp_cw_free(&w);
    return avp_buf_str(&b);
}

static const char *cr_cz(AvpArena *a, AvpCacheReader *r) {
    return avp_cz(a, avp_cr_str(r));
}

bool oc_unpack_result(AvpArena *a, AvpStr data, OcResult *r) {
    memset(r, 0, sizeof *r);
    size_t ml = sizeof(OC_RESULT_MAGIC) - 1;
    if (data.n < ml + 4 || memcmp(data.p, OC_RESULT_MAGIC, ml)) return false;
    const u8 *p = (const u8 *)data.p + ml;
    u32 hn = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    if (hn > data.n - ml - 4) return false;
    AvpCacheReader c;
    avp_cr_init(&c, p + 4, hn);
    u32 n = avp_cr_count(&c, 4);
    for (u32 i = 0; i < n && avp_cr_ok(&c); i++) avp_vec_push(&r->argv, avp_str(cr_cz(a, &c)));
    r->src = cr_cz(a, &c);
    r->src_sha = cr_cz(a, &c);
    r->compiler = cr_cz(a, &c);
    r->target_suffix = cr_cz(a, &c);
    n = avp_cr_count(&c, 9);
    for (u32 i = 0; i < n && avp_cr_ok(&c); i++) {
        char k[2] = {(char)avp_cr_u8(&c), 0};
        avp_vec_push(&r->dep_kinds, avp_str(avp_strdup(a, k)));
        avp_vec_push(&r->dep_paths, avp_str(cr_cz(a, &c)));
        avp_vec_push(&r->dep_shas, avp_str(cr_cz(a, &c)));
    }
    r->obj_sha = cr_cz(a, &c);
    size_t bn;
    const u8 *bp = avp_cr_bytes(&c, &bn);
    r->out_text = (AvpStr){avp_memdup(a, bp, bn), bn};
    bp = avp_cr_bytes(&c, &bn);
    r->err_text = (AvpStr){avp_memdup(a, bp, bn), bn};
    r->cwd = cr_cz(a, &c);
    r->created = avp_cr_f64(&c);
    r->manifest = cr_cz(a, &c);
    if (!avp_cr_ok(&c) || avp_cr_remaining(&c)) return false;
    r->obj = (AvpStr){(const char *)p + 4 + hn, data.n - ml - 4 - hn};
    if (strcmp(oc_sha_hex(a, r->obj.p, r->obj.n), r->obj_sha)) return false;
    return true;
}

static AvpStr pack_manifest(AvpArena *a, const ManEntries *es) {
    AvpCacheWriter w;
    avp_cw_init(&w);
    avp_cw_count(&w, es->n);
    for (size_t i = 0; i < es->n; i++) {
        const ManEntry *e = &es->v[i];
        avp_cw_count(&w, e->ndeps);
        for (size_t j = 0; j < e->ndeps; j++) put_dep(&w, e->deps[j].kind, e->deps[j].path, e->deps[j].sha);
        avp_cw_count(&w, e->nprobes);
        for (size_t j = 0; j < e->nprobes; j++) {
            avp_cw_u8(&w, (u8)e->probes[j].kind);
            avp_cw_cz(&w, e->probes[j].parent);
            avp_cw_cz(&w, e->probes[j].base);
            avp_cw_bool(&w, e->probes[j].exists);
        }
        avp_cw_cz(&w, e->result);
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, OC_MANIFEST_MAGIC);
    avp_buf_putn(&b, w.b.v, w.b.n);
    avp_cw_free(&w);
    return avp_buf_str(&b);
}

static ManEntries unpack_manifest(AvpArena *a, AvpStr data, bool have) {
    ManEntries es = {0};
    size_t ml = sizeof(OC_MANIFEST_MAGIC) - 1;
    if (!have || data.n < ml || memcmp(data.p, OC_MANIFEST_MAGIC, ml)) return es;
    AvpCacheReader c;
    avp_cr_init(&c, data.p + ml, data.n - ml);
    u32 n = avp_cr_count(&c, 12);
    for (u32 i = 0; i < n && avp_cr_ok(&c); i++) {
        ManEntry e = {0};
        e.ndeps = avp_cr_count(&c, 9);
        e.deps = AVP_NEWN(a, Dep, e.ndeps + 1);
        for (size_t j = 0; j < e.ndeps && avp_cr_ok(&c); j++) {
            e.deps[j].kind = (char)avp_cr_u8(&c);
            e.deps[j].path = cr_cz(a, &c);
            e.deps[j].sha = cr_cz(a, &c);
        }
        e.nprobes = avp_cr_count(&c, 10);
        e.probes = AVP_NEWN(a, Probe, e.nprobes + 1);
        for (size_t j = 0; j < e.nprobes && avp_cr_ok(&c); j++) {
            e.probes[j].kind = (char)avp_cr_u8(&c);
            e.probes[j].parent = cr_cz(a, &c);
            e.probes[j].base = cr_cz(a, &c);
            e.probes[j].exists = avp_cr_bool(&c);
        }
        e.result = cr_cz(a, &c);
        avp_vec_push(&es, e);
    }
    if (!avp_cr_ok(&c) || avp_cr_remaining(&c)) es.n = 0; /* damaged: no dependency sets */
    return es;
}

/* ================================================================================================= probes */

typedef struct {
    AvpArena *a;
    AvpMap *dirs; /* norm(parent) -> AvpSet* of normcased names */
} Listing;

static bool listed(Listing *L, const char *parent, const char *base) {
    char *key = oc_norm(L->a, parent);
    AvpVal *v = avp_map_get(L->dirs, avp_str(key));
    AvpSet *names;
    if (v) {
        names = v->p;
    } else {
        names = avp_set_new();
        AvpStrVec ls = {0};
        if (avp_listdir(L->a, parent, &ls))
            for (size_t i = 0; i < ls.n; i++) avp_set_add(names, avp_str(avp_path_normcase(L->a, avp_cz(L->a, ls.v[i]))));
        avp_vec_free(&ls);
        avp_map_put(L->dirs, avp_str(key), NULL)->p = names;
    }
    return avp_set_has(names, avp_str(avp_path_normcase(L->a, base)));
}

static void listing_free(Listing *L) {
    AvpMapIter it = {0};
    while (avp_map_next(L->dirs, &it)) avp_set_free(it.val->p);
    avp_map_free(L->dirs);
}

static bool probes_hold(Listing *L, const char *cwd, const ManEntry *e) {
    for (size_t i = 0; i < e->nprobes; i++) {
        const Probe *p = &e->probes[i];
        if (listed(L, oc_join_dep(L->a, cwd, p->kind, p->parent), p->base) != p->exists) return false;
    }
    return true;
}

/* _rel_or_abs: ('r', path under cwd) or ('a', normalised absolute path) */
static void rel_or_abs(AvpArena *a, const char *cwd, const char *p, char *kind, const char **out) {
    char *nc = oc_norm(a, cwd);
    size_t l = strlen(nc);
    while (l && (nc[l - 1] == '\\' || nc[l - 1] == '/')) l--;
    char *prefix = avp_sprintf(a, "%.*s\\", (int)l, nc);
    char *n = avp_path_normpath(a, p);
    if (!strncmp(avp_path_normcase(a, n), prefix, strlen(prefix))) {
        *kind = 'r';
        *out = n + strlen(prefix);
    } else {
        *kind = 'a';
        *out = n;
    }
}

static bool build_probes(AvpArena *a, const OcCommand *c, const AvpStr *files, size_t nfiles, char **dep_paths,
                         size_t ndeps, Probe **out, size_t *nout, const char **why) {
    AvpSet *names = avp_set_new();
    for (size_t i = 0; i < nfiles; i++)
        if (!oc_include_names(a, files[i], names, why)) {
            avp_set_free(names);
            return false;
        }
    AvpPtrVec dirs = {0};
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < c->include_dirs.n; i++) {
        char *d = oc_cmd_path(a, c, avp_cz(a, c->include_dirs.v[i]));
        if (avp_set_add(seen, avp_str(oc_norm(a, d)))) avp_vec_push(&dirs, d);
    }
    {
        char *d = avp_cz(a, avp_path_dirname(oc_cmd_path(a, c, c->src)));
        if (avp_set_add(seen, avp_str(oc_norm(a, d)))) avp_vec_push(&dirs, d);
    }
    for (size_t i = 0; i < ndeps; i++) {
        char *d = avp_cz(a, avp_path_dirname(dep_paths[i]));
        if (avp_set_add(seen, avp_str(oc_norm(a, d)))) avp_vec_push(&dirs, d);
    }
    AvpSet *deps = avp_set_new();
    for (size_t i = 0; i < ndeps; i++) avp_set_add(deps, avp_str(oc_norm(a, dep_paths[i])));
    AvpStrVec sorted = avp_set_sorted(names);
    Listing L = {a, avp_map_new()};
    AvpMap *probes = avp_map_new(); /* kind + parent + '\n' + base -> Probe* */
    AVP_VEC(Probe *) order = {0};
    for (size_t i = 0; i < dirs.n; i++)
        for (size_t j = 0; j < sorted.n; j++) {
            char *p = avp_path_normpath(a, avp_path_join(a, dirs.v[i], avp_cz(a, sorted.v[j])));
            if (avp_set_has(deps, avp_str(oc_norm(a, p)))) continue;
            AvpStr head, tail;
            avp_path_split(p, &head, &tail);
            char *parent = avp_cz(a, head), *base = avp_cz(a, tail);
            Probe *pr = AVP_NEW(a, Probe);
            rel_or_abs(a, c->cwd, parent, &pr->kind, &pr->parent);
            pr->base = base;
            pr->exists = listed(&L, parent, base);
            bool created;
            AvpVal *v = avp_map_put(probes, avp_str(avp_sprintf(a, "%c%s\n%s", pr->kind, pr->parent, base)), &created);
            if (created) avp_vec_push(&order, pr);
            v->p = pr;
        }
    *out = AVP_NEWN(a, Probe, order.n + 1);
    for (size_t i = 0; i < order.n; i++) {
        AvpVal *v = avp_map_get(probes, avp_str(avp_sprintf(a, "%c%s\n%s", order.v[i]->kind, order.v[i]->parent,
                                                            order.v[i]->base)));
        (*out)[i] = *(Probe *)v->p;
    }
    *nout = order.n;
    avp_vec_free(&order);
    avp_map_free(probes);
    listing_free(&L);
    avp_vec_free(&sorted);
    avp_set_free(deps);
    avp_set_free(seen);
    avp_set_free(names);
    avp_vec_free(&dirs);
    return true;
}

/* ================================================================================================= stats log */

/* Append under a lock on the file's first byte (the Python wrapper locks the same byte with msvcrt.locking, so C and
 * Python wrappers serialise their appends); polled every millisecond, dropped after about two seconds. */
static void append_locked(AvpArena *a, const char *path, char event) {
    HANDLE h = CreateFileW(avp_to_wz(a, path), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov = {0};
    bool locked = false;
    for (int attempt = 0; attempt < 2000 && !locked; attempt++) {
        locked = LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov) != 0;
        if (!locked) Sleep(1);
    }
    if (locked) {
        LARGE_INTEGER zero = {0};
        DWORD wrote;
        if (SetFilePointerEx(h, zero, NULL, FILE_END)) WriteFile(h, &event, 1, &wrote, NULL);
        UnlockFileEx(h, 0, 1, 0, &ov);
    }
    CloseHandle(h);
}

static void log_event(AvpArena *a, const char *cwd, char event) {
    const char *dirs[2] = {oc_cache_dir(a), avp_path_join(a, cwd, "build")};
    for (int i = 0; i < 2; i++)
        if (avp_isdir(dirs[i])) append_locked(a, avp_path_join(a, dirs[i], OC_STATS_LOG), event);
}

/* ================================================================================================= the wrapper */

static void emit(AvpBuf *buf, DWORD std, AvpStr s) {
    if (!s.n) return;
    if (buf) {
        avp_buf_put(buf, s);
        return;
    }
    HANDLE h = GetStdHandle(std);
    size_t off = 0;
    while (off < s.n) {
        DWORD w = 0;
        DWORD chunk = (DWORD)((s.n - off) > (1u << 30) ? (1u << 30) : (s.n - off));
        if (!WriteFile(h, s.p + off, chunk, &w, NULL) || !w) break;
        off += w;
    }
}

static void emitz(AvpBuf *buf, DWORD std, const char *s) {
    emit(buf, std, avp_str(s));
}

typedef struct {
    AvpStr out, err;
    int rc;
} RunOut;

static RunOut run_plain(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, const OcEnv *env,
                        bool custom_env, const OcRunOpts *o) {
    const char **av = avp_alloc(a, (argc + 1) * sizeof(char *));
    for (size_t i = 0; i < argc; i++) av[i] = argv[i];
    AvpEnv *e = NULL;
    if (custom_env) {
        e = avp_env_new(a);
        for (size_t i = 0; i < env->n; i++) {
            const char *eq = strchr(env->vars[i], '=');
            if (eq && eq != env->vars[i])
                avp_env_set(e, avp_cz(a, (AvpStr){env->vars[i], (size_t)(eq - env->vars[i])}), eq + 1);
        }
    }
    AvpProcSpec spec = {.argv = av, .cwd = cwd, .env = e, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = a};
    AvpProcResult r = avp_run(&spec);
    RunOut ro = {{0}, {0}, 0};
    if (r.status == AVP_PROC_START_FAILED) {
        emitz(o->err, STD_ERROR_HANDLE,
              avp_sprintf(a, "objcache: cannot run %s: %s\n", argv[0], r.message ? r.message : "cannot start"));
        ro.rc = 127;
        return ro;
    }
    ro.out = r.out;
    ro.err = r.err;
    emit(o->out, STD_OUTPUT_HANDLE, r.out);
    emit(o->err, STD_ERROR_HANDLE, r.err);
    ro.rc = (int)r.returncode;
    return ro;
}

/* compiler output for storage: the working directory replaced by a marker (and back) */
static AvpStr replace_all(AvpArena *a, AvpStr s, AvpStr from, AvpStr to) {
    if (!from.n) return s;
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t i = 0;
    while (i < s.n) {
        if (s.n - i >= from.n && !memcmp(s.p + i, from.p, from.n)) {
            avp_buf_put(&b, to);
            i += from.n;
        } else {
            avp_buf_putc(&b, s.p[i++]);
        }
    }
    return (AvpStr){avp_buf_cz(&b), b.n};
}

static const ManEntry *lookup(AvpArena *a, ShsmCache *cache, const OcCommand *c, const char *mkey, OcMemo *memo,
                              OcResult *res) {
    AvpStr data;
    bool have = shsm_cache_get(a, cache, mkey, &data);
    ManEntries es = unpack_manifest(a, data, have);
    AvpMap *hashes = avp_map_new();
    Listing L = {a, avp_map_new()};
    const ManEntry *found = NULL;
    for (size_t i = 0; i < es.n && !found; i++) {
        const ManEntry *e = &es.v[i];
        bool ok = true;
        for (size_t j = 0; j < e->ndeps && ok; j++) {
            char *path = oc_join_dep(a, c->cwd, e->deps[j].kind, e->deps[j].path);
            AvpVal *v = avp_map_get(hashes, avp_str(path));
            const char *got;
            if (v) {
                got = v->p;
            } else {
                got = oc_memo_sha(a, memo, path);
                if (!got) got = "";
                avp_map_put(hashes, avp_str(path), NULL)->p = (void *)got;
            }
            ok = !strcmp(got, e->deps[j].sha);
        }
        if (!ok || !probes_hold(&L, c->cwd, e)) continue;
        AvpStr rd;
        if (!shsm_cache_get(a, cache, e->result, &rd) || !oc_unpack_result(a, rd, res)) {
            shsm_cache_remove(cache, e->result);
            continue;
        }
        found = e;
    }
    listing_free(&L);
    avp_map_free(hashes);
    avp_vec_free(&es);
    return found;
}

static bool write_atomic(AvpArena *a, const char *path, AvpStr data) {
    avp_makedirs(avp_cz(a, avp_path_dirname(avp_path_abspath(a, path))), true);
    char *tmp = avp_sprintf(a, "%s.objcache-%lu.tmp", path, (unsigned long)GetCurrentProcessId());
    FILE *f = NULL;
    if (_wfopen_s(&f, avp_to_wz(a, tmp), L"wb") || !f) return false;
    bool ok = fwrite(data.p, 1, data.n, f) == data.n;
    ok = fclose(f) == 0 && ok;
    if (!ok || !avp_replace(tmp, path)) {
        avp_remove(tmp);
        return false;
    }
    return true;
}

static bool restore(AvpArena *a, const OcCommand *c, const OcResult *r) {
    char *target = avp_sprintf(a, "%s%s", c->out, r->target_suffix);
    const char **deps = avp_alloc(a, (r->dep_paths.n + 1) * sizeof(char *));
    for (size_t i = 0; i < r->dep_paths.n; i++) deps[i] = oc_join_dep(a, c->cwd, r->dep_kinds.v[i].p[0], r->dep_paths.v[i].p);
    if (!write_atomic(a, oc_cmd_path(a, c, c->obj), r->obj)) return false;
    return write_atomic(a, oc_cmd_path(a, c, c->dep), oc_render_depfile(a, target, c->src, deps, r->dep_paths.n));
}

static bool read_file(AvpArena *a, const char *path, AvpStr *out) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) return false;
    *out = (AvpStr){(const char *)b.v, b.n};
    return true;
}

static bool store(AvpArena *a, ShsmCache *cache, const OcCommand *c, const char *mkey, const char *compiler_id,
                  AvpStr source, double start, AvpStr so, AvpStr se, bool auto_prune, const char **why) {
    AvpStr now, obj, dep;
    if (!read_file(a, oc_cmd_path(a, c, c->src), &now)) return *why = "cannot read the source", false;
    if (!avp_str_eq(now, source)) return *why = "source changed during the compile", false;
    if (!read_file(a, oc_cmd_path(a, c, c->obj), &obj)) return *why = "cannot read the object", false;
    if (!read_file(a, oc_cmd_path(a, c, c->dep), &dep)) return *why = "cannot read the dependency file", false;
    const char *target;
    AvpStrVec printed = {0};
    if (!oc_parse_depfile(a, dep, c->src, &target, &printed, why)) return false;
    if (strcmp(target, c->obj))
        return *why = "dependency target is not the -o directory joined with the object name", false;
    AvpStr cw = {c->cwd, strlen(c->cwd)};
    while (cw.n && (cw.p[cw.n - 1] == '\\' || cw.p[cw.n - 1] == '/')) cw.n--;
    char *prefix = avp_sprintf(a, "%.*s\\", (int)cw.n, cw.p);
    size_t pl = strlen(prefix), nd = printed.n;
    OcResult r = {0};
    char **paths = AVP_NEWN(a, char *, nd + 1);
    AvpStr *files = AVP_NEWN(a, AvpStr, nd + 1);
    files[0] = source;
    ShsmKey rk;
    shsm_key_init(&rk);
    shsm_key_strz(&rk, OC_FORMAT);
    shsm_key_strz(&rk, "result");
    shsm_key_strz(&rk, mkey);
    shsm_key_int(&rk, (i64)nd);
    Dep *deps = AVP_NEWN(a, Dep, nd + 1);
    for (size_t i = 0; i < nd; i++) {
        const char *p = printed.v[i].p;
        bool rel = !strncmp(p, prefix, pl);
        deps[i].kind = rel ? 'r' : 'a';
        deps[i].path = rel ? p + pl : p;
        paths[i] = oc_join_dep(a, c->cwd, deps[i].kind, deps[i].path);
        AvpStat st;
        if (!avp_stat(paths[i], &st)) return *why = avp_sprintf(a, "cannot stat %s", paths[i]), false;
        if (st.mtime > start - OC_MTIME_SLACK)
            return *why = avp_sprintf(a, "header modified during the compile: %s", paths[i]), false;
        if (!read_file(a, paths[i], &files[i + 1])) return *why = avp_sprintf(a, "cannot read %s", paths[i]), false;
        deps[i].sha = oc_sha_hex(a, files[i + 1].p, files[i + 1].n);
        char k[2] = {deps[i].kind, 0};
        avp_vec_push(&r.dep_kinds, avp_str(avp_strdup(a, k)));
        avp_vec_push(&r.dep_paths, avp_str(deps[i].path));
        avp_vec_push(&r.dep_shas, avp_str(deps[i].sha));
        shsm_key_strz(&rk, k);
        shsm_key_strz(&rk, deps[i].path);
        shsm_key_strz(&rk, deps[i].sha);
    }
    AvpStat sst;
    if (!avp_stat(oc_cmd_path(a, c, c->src), &sst)) return *why = "cannot stat the source", false;
    if (sst.mtime > start - OC_MTIME_SLACK) return *why = "source modified during the compile", false;
    ManEntry entry = {deps, nd, NULL, 0, NULL};
    if (!build_probes(a, c, files, nd + 1, paths, nd, &entry.probes, &entry.nprobes, why)) return false;
    const char *rkey = shsm_key_hex(a, &rk);
    entry.result = rkey;
    for (size_t i = 0; i < c->argc; i++) avp_vec_push(&r.argv, avp_str(i == c->oi ? OC_OUT : c->argv[i]));
    r.src = c->src;
    r.src_sha = oc_sha_hex(a, source.p, source.n);
    r.compiler = compiler_id;
    r.target_suffix = target + strlen(c->out);
    r.obj_sha = oc_sha_hex(a, obj.p, obj.n);
    r.out_text = replace_all(a, so, avp_str(c->cwd), AVP_S(OC_CWD_MARK));
    r.err_text = replace_all(a, se, avp_str(c->cwd), AVP_S(OC_CWD_MARK));
    r.cwd = c->cwd;
    r.created = start;
    r.manifest = mkey;
    r.obj = obj;
    AvpStr packed = oc_pack_result(a, &r);
    shsm_cache_put(cache, rkey, packed.p, packed.n);
    AvpStr old;
    bool have = shsm_cache_get(a, cache, mkey, &old);
    ManEntries olds = unpack_manifest(a, old, have);
    ManEntries es = {0};
    avp_vec_push(&es, entry);
    for (size_t i = 0; i < olds.n && es.n < OC_MAX_DEPSETS; i++)
        if (strcmp(olds.v[i].result, rkey)) avp_vec_push(&es, olds.v[i]);
    AvpStr man = pack_manifest(a, &es);
    shsm_cache_put(cache, mkey, man.p, man.n);
    avp_vec_free(&es);
    avp_vec_free(&olds);
    if (auto_prune) {
        LARGE_INTEGER pc;
        QueryPerformanceCounter(&pc);
        u64 x = (u64)pc.QuadPart * 0x9E3779B97F4A7C15ull ^ GetCurrentProcessId();
        if ((x >> 17) % OC_AUTO_PRUNE_ONE_IN == 0) {
            i64 removed, freed;
            oc_prune(a, cache->root, (i64)(OC_DEFAULT_MAX_GB * (double)(1LL << 30)), &removed, &freed);
        }
    }
    return true;
}

int oc_run_wrapped(AvpArena *a, const char *const *argv, size_t argc, const char *cwd, const OcEnv *env_in,
                   const OcRunOpts *o) {
    double t0 = avp_perf_counter();
    OcRunOpts dflt = {0};
    if (!o) o = &dflt;
    if (!cwd) cwd = avp_getcwd(a);
    bool custom_env = env_in != NULL;
    OcEnv env = custom_env ? *env_in : oc_env_current(a);
    if (!argc) {
        emitz(o->err, STD_ERROR_HANDLE, "objcache: no command\n");
        return 2;
    }
    if (!oc_enabled_value(oc_env_get(&env, "SHSM_OBJCACHE")))
        return run_plain(a, argv, argc, cwd, &env, custom_env, o).rc;
    const char *dbg = oc_env_get(&env, "SHSM_OBJCACHE_DEBUG");
    bool debug = dbg && !strcmp(dbg, "1");
    if (custom_env) { /* the cache root follows the given environment */
        const char *cd = oc_env_get(&env, "SHSM_CACHE_DIR");
        SetEnvironmentVariableW(L"SHSM_CACHE_DIR", cd ? avp_to_wz(a, cd) : NULL);
    }
    ShsmCache *cache = shsm_cache_new(oc_cache_dir(a));
    OcCommand c;
    const char *why = NULL, *compiler_id = NULL, *mkey = NULL;
    AvpStr source = {0};
    OcMemo *memo = NULL;
    bool ok = oc_command_parse(a, argv, argc, cwd, &c, &why);
    if (ok) {
        memo = oc_memo_load(a, cwd);
        compiler_id = oc_compiler_identity(a, &c, memo, &why);
        ok = compiler_id != NULL;
    }
    if (ok && !read_file(a, oc_cmd_path(a, &c, c.src), &source)) {
        why = avp_sprintf(a, "cannot read %s", oc_cmd_path(a, &c, c.src));
        ok = false;
    }
    if (!ok) {
        if (debug) emitz(o->err, STD_ERROR_HANDLE, avp_sprintf(a, "objcache: uncached: %s\n", why));
        log_event(a, cwd, 'u');
        return run_plain(a, argv, argc, cwd, &env, custom_env, o).rc;
    }
    mkey = oc_manifest_key(a, &c, compiler_id, source, &env);
    AvpTry t;
    if (AVP_TRY(&t)) {
        OcResult res;
        const ManEntry *hit = lookup(a, cache, &c, mkey, memo, &res);
        bool restored = hit && restore(a, &c, &res);
        avp_try_end(&t);
        if (restored) {
            emit(o->out, STD_OUTPUT_HANDLE, replace_all(a, res.out_text, AVP_S(OC_CWD_MARK), avp_str(c.cwd)));
            emit(o->err, STD_ERROR_HANDLE, replace_all(a, res.err_text, AVP_S(OC_CWD_MARK), avp_str(c.cwd)));
            if (debug) {
                double ms = (avp_perf_counter() - t0) * 1000;
                emitz(o->err, STD_ERROR_HANDLE,
                      avp_sprintf(a, "objcache: hit (%ld files hashed, %.1f ms)\n", (long)oc_files_hashed, ms));
            }
            log_event(a, cwd, 'h');
            return 0;
        }
        if (hit && debug) emitz(o->err, STD_ERROR_HANDLE, "objcache: lookup failed: cannot restore the outputs\n");
    } else if (debug) {
        emitz(o->err, STD_ERROR_HANDLE, avp_sprintf(a, "objcache: lookup failed: %s\n", t.msg));
    }
    double start = avp_time();
    RunOut ro = run_plain(a, argv, argc, cwd, &env, custom_env, o);
    if (ro.rc != 0) {
        log_event(a, cwd, 'n');
        return ro.rc;
    }
    volatile bool stored = false;
    if (AVP_TRY(&t)) {
        stored = store(a, cache, &c, mkey, compiler_id, source, start, ro.out, ro.err, !o->no_auto_prune, &why);
        avp_try_end(&t);
    } else {
        why = avp_strdup(a, t.msg);
    }
    if (!stored && debug) emitz(o->err, STD_ERROR_HANDLE, avp_sprintf(a, "objcache: not stored: %s\n", why));
    log_event(a, cwd, stored ? 'm' : 'n');
    return ro.rc;
}
