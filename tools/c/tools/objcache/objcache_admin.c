/* objcache: the maintenance side (tools/common/cache.py Cache.entries/prune, objcache.py _entries and
 * verify_entry). */
#include "objcache.h"
#include "shsm_cache.h"

#include <string.h>
#include <windows.h>

static bool starts(const u8 *p, size_t n, const char *magic) {
    size_t m = strlen(magic);
    return n >= m && !memcmp(p, magic, m);
}

static OcKind kind_of(const char *path, bool *c_result) {
    *c_result = false;
    FILE *f = NULL;
    if (_wfopen_s(&f, avp_to_wz(avp_scratch(), path), L"rb") || !f) return OC_KIND_OTHER;
    u8 head[16];
    size_t n = fread(head, 1, sizeof head, f);
    fclose(f);
    if (starts(head, n, OC_RESULT_MAGIC)) *c_result = true;
    if (starts(head, n, OC_PY_RESULT_PREFIX)) return OC_KIND_RESULT;
    if (starts(head, n, OC_PY_MANIFEST_PREFIX)) return OC_KIND_MANIFEST;
    return OC_KIND_OTHER;
}

static bool is_tmp(AvpStr n) {
    return n.n >= 5 && !memcmp(n.p, ".tmp-", 5);
}

OcEntries oc_entries(AvpArena *a, const char *root, bool with_kind) {
    OcEntries out = {0};
    if (!avp_isdir(root)) return out;
    AvpStrVec subs = {0};
    if (!avp_listdir(a, root, &subs)) return out;
    for (size_t i = 0; i < subs.n; i++) {
        AvpStr sub = subs.v[i];
        char *d = avp_path_join(a, root, avp_cz(a, sub));
        if (sub.n != 2 || !avp_isdir(d)) continue;
        AvpStrVec names = {0};
        if (!avp_listdir(a, d, &names)) continue;
        for (size_t j = 0; j < names.n; j++) {
            AvpStr nm = names.v[j];
            if (is_tmp(nm) || nm.n != 64 || memcmp(nm.p, sub.p, 2)) continue;
            char *p = avp_path_join(a, d, avp_cz(a, nm));
            AvpStat st;
            if (!avp_stat(p, &st)) continue;
            OcEntry e = {p, avp_cz(a, nm), st.size, st.mtime_ns, OC_KIND_OTHER, false};
            if (with_kind) e.kind = kind_of(p, &e.c_result);
            avp_vec_push(&out, e);
        }
        avp_vec_free(&names);
    }
    avp_vec_free(&subs);
    return out;
}

static int cmp_mtime(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 p = ((const OcEntry *)x)->mtime_ns, q = ((const OcEntry *)y)->mtime_ns;
    return p < q ? -1 : p > q;
}

void oc_prune(AvpArena *a, const char *root, i64 max_bytes, i64 *removed, i64 *freed) {
    *removed = *freed = 0;
    double now = avp_time();
    if (avp_isdir(root)) {
        AvpStrVec subs = {0};
        if (avp_listdir(a, root, &subs))
            for (size_t i = 0; i < subs.n; i++) {
                char *d = avp_path_join(a, root, avp_cz(a, subs.v[i]));
                if (!avp_isdir(d)) continue;
                AvpStrVec names = {0};
                if (avp_listdir(a, d, &names))
                    for (size_t j = 0; j < names.n; j++) {
                        if (!is_tmp(names.v[j])) continue;
                        char *p = avp_path_join(a, d, avp_cz(a, names.v[j]));
                        AvpStat st;
                        if (avp_stat(p, &st) && now - st.mtime > 3600) avp_remove(p);
                    }
                avp_vec_free(&names);
            }
        avp_vec_free(&subs);
    }
    OcEntries es = oc_entries(a, root, false);
    avp_sort(es.v, es.n, sizeof *es.v, cmp_mtime, NULL, false);
    i64 total = 0;
    for (size_t i = 0; i < es.n; i++) total += es.v[i].size;
    for (size_t i = 0; i < es.n; i++) {
        if (total <= max_bytes) break;
        if (avp_remove(es.v[i].path)) {
            total -= es.v[i].size;
            ++*removed;
            *freed += es.v[i].size;
        }
    }
    avp_vec_free(&es);
}

static bool read_file(AvpArena *a, const char *path, AvpStr *out) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) return false;
    *out = (AvpStr){(const char *)b.v, b.n};
    return true;
}

static AvpStr unmark(AvpArena *a, AvpStr s, const char *cwd) {
    AvpBuf b;
    avp_buf_init(&b, a);
    AvpStr m = AVP_S(OC_CWD_MARK);
    for (size_t i = 0; i < s.n;) {
        if (s.n - i >= m.n && !memcmp(s.p + i, m.p, m.n)) {
            avp_buf_putz(&b, cwd);
            i += m.n;
        } else {
            avp_buf_putc(&b, s.p[i++]);
        }
    }
    return (AvpStr){avp_buf_cz(&b), b.n};
}

const char *oc_verify_entry(AvpArena *a, const char *root, const char *cache_root, const char *key,
                            const char *scratch) {
    ShsmCache *cache = shsm_cache_new(cache_root);
    AvpStr data;
    OcResult r;
    if (!shsm_cache_get(a, cache, key, &data) || !oc_unpack_result(a, data, &r)) return "bad: damaged entry";
    size_t n = r.argv.n, oi = n;
    for (size_t i = 0; i < n; i++)
        if (avp_str_eqz(r.argv.v[i], OC_OUT)) oi = i;
    if (oi == n) return "bad: damaged entry";
    const char **av = avp_alloc(a, (n + 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) av[i] = i == oi ? "verify" : avp_cz(a, r.argv.v[i]);
    OcCommand c;
    const char *why;
    if (!oc_command_parse(a, av, n, root, &c, &why)) return "stale";
    const char *id = oc_compiler_identity(a, &c, NULL, &why);
    if (!id || strcmp(id, r.compiler)) return "stale";
    char *sha = oc_file_sha(a, oc_cmd_path(a, &c, c.src));
    if (!sha || strcmp(sha, r.src_sha)) return "stale";
    for (size_t i = 0; i < r.dep_paths.n; i++) {
        char *h = oc_file_sha(a, oc_join_dep(a, root, r.dep_kinds.v[i].p[0], r.dep_paths.v[i].p));
        if (!h || strcmp(h, r.dep_shas.v[i].p)) return "stale";
    }
    char *tmpdir = avp_mkdtemp(a, "v-", NULL, scratch);
    char *tmp = NULL;
    if (!tmpdir || !avp_path_relpath(a, tmpdir, root, &tmp)) return "bad: the fresh compile failed";
    const char *result = "ok";
    av[oi] = tmp;
    AvpProcSpec spec = {.argv = av, .cwd = root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .arena = a};
    AvpProcResult pr = avp_run(&spec);
    AvpStr fresh_obj, fresh_dep;
    if (pr.status != AVP_PROC_OK || pr.returncode != 0 || !oc_command_parse(a, av, n, root, &c, &why) ||
        !read_file(a, oc_cmd_path(a, &c, c.obj), &fresh_obj) || !read_file(a, oc_cmd_path(a, &c, c.dep), &fresh_dep)) {
        result = "bad: the fresh compile failed";
    } else {
        const char **deps = avp_alloc(a, (r.dep_paths.n + 1) * sizeof(char *));
        for (size_t i = 0; i < r.dep_paths.n; i++)
            deps[i] = oc_join_dep(a, root, r.dep_kinds.v[i].p[0], r.dep_paths.v[i].p);
        AvpStr want_dep =
            oc_render_depfile(a, avp_sprintf(a, "%s%s", tmp, r.target_suffix), c.src, deps, r.dep_paths.n);
        if (!avp_str_eq(fresh_obj, r.obj))
            result = "bad: object differs";
        else if (!avp_str_eq(fresh_dep, want_dep))
            result = "bad: dependency file differs";
        else if (!avp_str_eq(pr.out, unmark(a, r.out_text, root)) || !avp_str_eq(pr.err, unmark(a, r.err_text, root)))
            result = "bad: compiler output differs";
    }
    avp_rmtree(avp_path_join(a, root, tmp), AVP_RMTREE_IGNORE_ERRORS);
    return result;
}
