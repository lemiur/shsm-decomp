/* configure.exe --dump: the final state as one JSON document, the encoding of diff/dump_config.py (its docstring):
 * {"args": <namespace>, "config": <ProjectConfig>}, json.dumps(indent=1), ensure_ascii. */
#include <string.h>

#include "avp/json.h"
#include "cfg.h"

static AvpJson *encode(AvpArena *a, const Pv *v) {
    switch (v->kind) {
    case PV_NONE: return avp_json_null();
    case PV_BOOL: return avp_json_bool(v->u.b);
    case PV_INT: return avp_json_int(a, v->u.i);
    case PV_STR: return avp_json_str(a, v->u.s);
    case PV_PATH: {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("$path"), avp_json_str(a, v->u.s));
        return o;
    }
    case PV_LIST: {
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < v->u.list->n; i++) avp_json_arr_push(a, arr, encode(a, v->u.list->v[i]));
        return arr;
    }
    case PV_TUPLE: {
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < v->u.list->n; i++) avp_json_arr_push(a, arr, encode(a, v->u.list->v[i]));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("$tuple"), arr);
        return o;
    }
    case PV_DICT: {
        AvpJson *o = avp_json_obj(a);
        for (size_t i = 0; i < v->u.dict->n; i++) avp_json_set(a, o, v->u.dict->keys[i], encode(a, v->u.dict->vals[i]));
        return o;
    }
    case PV_INSTANCE: {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("$class"), avp_json_strz(a, v->u.inst->cls));
        PvDict *d = v->u.inst->attrs;
        for (size_t i = 0; i < d->n; i++) avp_json_set(a, o, d->keys[i], encode(a, d->vals[i]));
        return o;
    }
    case PV_OPAQUE: {
        AvpJson *o = avp_json_obj(a);
        if (strcmp(v->u.opaque.tag, "function") == 0) {
            avp_json_set(a, o, AVP_S("$function"), avp_json_str(a, ((CfgFunction *)v->u.opaque.p)->name));
        } else {
            avp_json_set(a, o, AVP_S("$repr"), avp_json_str(a, pv_repr(a, v)));
        }
        return o;
    }
    }
    return avp_json_null();
}

AvpStr cfg_dump(AvpArena *a, Pv *args, Pv *config) {
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("args"), encode(a, args));
    avp_json_set(a, doc, AVP_S("config"), encode(a, config));
    AvpJsonDumpOpts opts = AVP_JSON_INDENT(1);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, avp_json_dumps(a, doc, &opts));
    avp_buf_putc(&b, '\n');
    return avp_buf_str(&b);
}
