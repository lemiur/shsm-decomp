/* Python str() / repr() / '%d' of JSON-loaded values, for the format strings of tools/wall.py. */
#include "wall.h"

static AvpStr big_or_int(AvpArena *a, const AvpJson *v) {
    return v->big ? avp_strn(v->u.str.p, v->u.str.n) : avp_repr_i64(a, v->u.i);
}

AvpStr wall_py_repr(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    switch (v->kind) {
    case AVP_JSON_BOOL: return avp_repr_bool(v->b);
    case AVP_JSON_INT: return big_or_int(a, v);
    case AVP_JSON_FLOAT: return avp_repr_f64(a, v->u.f);
    case AVP_JSON_STRING: return avp_repr_str(a, v->u.str);
    case AVP_JSON_ARRAY: {
        AvpStr *items = AVP_NEWN(a, AvpStr, v->u.arr.n + 1);
        for (size_t i = 0; i < v->u.arr.n; i++) items[i] = wall_py_repr(a, v->u.arr.v[i]);
        return avp_repr_list(a, items, v->u.arr.n);
    }
    default: {
        AvpStr *keys = AVP_NEWN(a, AvpStr, v->u.obj.n + 1), *vals = AVP_NEWN(a, AvpStr, v->u.obj.n + 1);
        for (size_t i = 0; i < v->u.obj.n; i++) {
            keys[i] = avp_repr_str(a, v->u.obj.v[i].key);
            vals[i] = wall_py_repr(a, v->u.obj.v[i].val);
        }
        return avp_repr_dict(a, keys, vals, v->u.obj.n);
    }
    }
}

AvpStr wall_py_str(AvpArena *a, const AvpJson *v) {
    if (v && v->kind == AVP_JSON_STRING) return v->u.str;
    return wall_py_repr(a, v);
}

i64 wall_py_int(const AvpJson *v) {
    if (!v) return 0;
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? 1 : 0;
    case AVP_JSON_INT: return v->u.i;
    case AVP_JSON_FLOAT: return (i64)v->u.f;
    default: return 0;
    }
}

AvpStr wall_py_hex(AvpArena *a, i64 v) { return avp_pyfmt(a, "%#x", v); }

static bool is_none(const AvpJson *v) { return !v || v->kind == AVP_JSON_NULL; }
static bool is_number(const AvpJson *v) {
    return v->kind == AVP_JSON_INT || v->kind == AVP_JSON_FLOAT || v->kind == AVP_JSON_BOOL;
}
static double num_of(const AvpJson *v) {
    if (v->kind == AVP_JSON_BOOL) return v->b ? 1.0 : 0.0;
    if (v->kind == AVP_JSON_INT) return (double)v->u.i;
    return v->u.f;
}

bool wall_py_eq(const AvpJson *x, const AvpJson *y) {
    if (is_none(x) || is_none(y)) return is_none(x) && is_none(y);
    if (is_number(x) || is_number(y)) {
        if (!(is_number(x) && is_number(y))) return false;
        if (x->kind != AVP_JSON_FLOAT && y->kind != AVP_JSON_FLOAT) {
            if (x->big || y->big) return x->big && y->big && avp_str_eq(x->u.str, y->u.str);
            return wall_py_int(x) == wall_py_int(y);
        }
        return num_of(x) == num_of(y);
    }
    if (x->kind != y->kind) return false;
    switch (x->kind) {
    case AVP_JSON_STRING: return avp_str_eq(x->u.str, y->u.str);
    case AVP_JSON_ARRAY:
        if (x->u.arr.n != y->u.arr.n) return false;
        for (size_t i = 0; i < x->u.arr.n; i++)
            if (!wall_py_eq(x->u.arr.v[i], y->u.arr.v[i])) return false;
        return true;
    default:
        if (x->u.obj.n != y->u.obj.n) return false;
        for (size_t i = 0; i < x->u.obj.n; i++) {
            const AvpJson *o = avp_json_get(y, x->u.obj.v[i].key);
            if (!o || !wall_py_eq(x->u.obj.v[i].val, o)) return false;
        }
        return true;
    }
}

AvpStr wall_jstr(const AvpJson *obj, const char *key) {
    const AvpJson *v = avp_json_getz(obj, key);
    return avp_json_is_str(v) ? v->u.str : AVP_S("");
}

AvpJson *wall_jstrz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
