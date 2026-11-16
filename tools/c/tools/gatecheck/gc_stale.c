/* gatecheck layout: is a built object older than what it was built from? A layout verdict read from an object that
 * ninja has not rebuilt since a source, an included header, symbols.txt or splits.txt changed describes the old
 * build; the command stops with GC_EXIT_STALE instead. The headers are the ones ninja recorded for the object
 * (shsm_deps). */
#include "gatecheck.h"

#include "shsm_deps.h"

struct GcStaleCheck {
    const char *root;
    ShsmDeps *deps;
    const char *config[2]; /* splits.txt, symbols.txt (absolute) */
};

GcStaleCheck *gc_stale_open(AvpArena *a, const char *root, const char *splits, const char *symbols) {
    GcStaleCheck *c = AVP_NEW(a, GcStaleCheck);
    c->root = root;
    c->deps = shsm_deps_load(a, root);
    c->config[0] = splits;
    c->config[1] = symbols;
    return c;
}

const char *gc_stale_newer_input(AvpArena *a, GcStaleCheck *c, const char *obj_rel, const char *obj_path,
                                 const char *src_rel) {
    double built;
    if (!avp_getmtime(obj_path, &built)) return NULL;
    return shsm_deps_newer_input(a, c->deps, c->root, obj_rel, src_rel, c->config, 2, built);
}
