/* shsm_deps: see shsm_deps.h. */
#include "shsm_deps.h"

#include "avp/fs.h"
#include "shsm_cli.h"

struct ShsmDeps {
    AvpMap *by_obj; /* object path -> AvpStrVec * */
};

ShsmDeps *shsm_deps_load(AvpArena *a, const char *root) {
    ShsmDeps *d = AVP_NEW(a, ShsmDeps);
    d->by_obj = avp_map_new();
    const char *ninja = avp_path_join(a, root, "tools/bin/ninja.exe");
    if (!avp_isfile(ninja)) return d;
    const char *argv[] = {ninja, "-t", "deps"};
    AvpTry t;
    ShsmRun run;
    if (AVP_TRY(&t)) {
        run = shsm_run(a, argv, 3, root, 60);
        avp_try_end(&t);
    } else {
        return d;
    }
    if (!shsm_run_ok(&run)) return d;
    AvpStrVec lines = avp_str_splitlines(run.out, false);
    AvpStrVec *cur = NULL;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr l = lines.v[i];
        if (!l.n) continue;
        if (l.p[0] == ' ' || l.p[0] == '\t') {
            if (cur) avp_vec_push(cur, avp_str_strip(l));
            continue;
        }
        i64 at = avp_str_find(l, AVP_S(": #deps"), 0, AVP_END);
        cur = NULL;
        if (at < 0) continue;
        cur = AVP_NEW(a, AvpStrVec);
        avp_map_put(d->by_obj, avp_str_dup(a, (AvpStr){l.p, (size_t)at}), NULL)->p = cur;
    }
    return d;
}

static bool newer_than(const char *path, double than) {
    double m;
    return avp_exists(path) && avp_getmtime(path, &m) && m > than;
}

static const char *full_path(AvpArena *a, const char *root, const char *p) {
    return avp_path_isabs(p) ? p : avp_path_join(a, root, p);
}

const char *shsm_deps_newer_input(AvpArena *a, const ShsmDeps *d, const char *root, const char *obj_rel,
                                  const char *src_rel, const char *const *extra, size_t nextra, double than) {
    if (src_rel && newer_than(full_path(a, root, src_rel), than)) return src_rel;
    for (size_t i = 0; i < nextra; i++)
        if (extra[i] && newer_than(full_path(a, root, extra[i]), than)) return extra[i];
    AvpVal *v = d ? avp_map_get(d->by_obj, avp_str(obj_rel)) : NULL;
    if (v && v->p) {
        const AvpStrVec *l = v->p;
        for (size_t i = 0; i < l->n; i++) {
            const char *p = avp_cz(a, l->v[i]);
            if (newer_than(full_path(a, root, p), than)) return p;
        }
    }
    return NULL;
}
