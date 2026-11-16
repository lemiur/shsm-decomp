/* unit_plan.load_classes and classes.mangled_class, on the classes tool's library (classes.h: the input stamp
 * comparison; classes_prog.h: the mangled-name reader). */
#include "unit_plan.h"

#include "classes.h"
#include "classes_prog.h"

const char *up_classes_json_path(AvpArena *a, const ShsmRepo *r) {
    return avp_path_join(a, r->version_build_dir, "classes.json");
}

const AvpJson *up_classes_load(AvpArena *a, const ShsmRepo *r, const char **note) {
    const char *path = up_classes_json_path(a, r);
    *note = NULL;
    if (!avp_exists(path)) {
        *note = avp_sprintf(a, "%s is absent (run `build/tools/classes.exe scan`): no class evidence", path);
        return NULL;
    }
    AvpBytes raw = {0};
    if (!avp_read_all(a, path, &raw)) {
        *note = avp_sprintf(a, "%s cannot be read (%s): no class evidence", path, avp_cz(a, avp_io_err_str(a)));
        return NULL;
    }
    AvpJsonError err = {0};
    AvpJson *db = avp_json_parse(a, avp_strn((const char *)raw.v, raw.n), &err);
    if (!db) {
        *note = avp_sprintf(a, "%s cannot be read (%s): no class evidence", path,
                            avp_cz(a, avp_json_error_str(a, &err)));
        return NULL;
    }
    const AvpJson *inputs = db->kind == AVP_JSON_OBJECT ? avp_json_get(db, AVP_S("inputs")) : NULL;
    const char *why = classes_stale_reason_inputs(a, inputs, r->asm_dir, r->symbols_txt);
    if (why) *note = avp_sprintf(a, "%s is stale: %s; run `build/tools/classes.exe scan`", path, why);
    return db;
}

/* ---- mangled_class ---- */

const char *up_mangled_class(AvpArena *a, const char *cname) {
    AvpStr cls = classes_mangled_class(a, avp_str(cname));
    return cls.p ? avp_cz(a, cls) : NULL;
}
