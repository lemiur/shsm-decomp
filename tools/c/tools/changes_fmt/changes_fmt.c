#include "changes_fmt.h"
#include <ctype.h>

static const char *const UNIT_KEYS[] = {"fuzzy_match_percent", "matched_code_percent", "matched_data_percent",
                                        "complete_code_percent", "complete_data_percent"};
static const char *const FUNCTION_KEYS[] = {"fuzzy_match_percent"};
#define COUNT(a) (sizeof(a) / sizeof *(a))

static const char *type_name(const AvpJson *v) {
    switch (v->kind) {
    case AVP_JSON_NULL: return "NoneType";
    case AVP_JSON_BOOL: return "bool";
    case AVP_JSON_INT: return "int";
    case AVP_JSON_FLOAT: return "float";
    case AVP_JSON_STRING: return "str";
    case AVP_JSON_ARRAY: return "list";
    default: return "dict";
    }
}

static const char *value_type(CfValue v) {
    switch (v.kind) {
    case CF_INT: return "int";
    case CF_FLOAT: return "float";
    case CF_STR: return "str";
    default: return v.s.p;
    }
}

CfValue cf_value_from_json(const AvpJson *v) {
    CfValue r = {CF_FLOAT, 0, 0.0, {"", 0}};
    if (!v) return r;
    switch (v->kind) {
    case AVP_JSON_BOOL:
        r.kind = CF_INT;
        r.i = v->b ? 1 : 0;
        break;
    case AVP_JSON_INT:
        if (v->big) {
            r.kind = CF_FLOAT;
            avp_parse_float(v->u.str, &r.f);
        } else {
            r.kind = CF_INT;
            r.i = v->u.i;
        }
        break;
    case AVP_JSON_FLOAT: r.f = v->u.f; break;
    case AVP_JSON_STRING:
        r.kind = CF_STR;
        r.s = v->u.str;
        break;
    default:
        r.kind = CF_OTHER;
        r.s = avp_str(type_name(v));
        break;
    }
    return r;
}

/* Compare an int with a float exactly, as Python does: -1, 0, 1 for i <, ==, > d; 2 for NaN. */
static int cmp_int_float(i64 i, double d) {
    if (d != d) return 2;
    if (d >= 9223372036854775808.0) return -1;
    if (d < -9223372036854775808.0) return 1;
    i64 t = (i64)d;
    if (i != t) return i < t ? -1 : 1;
    double frac = d - (double)t;
    return frac > 0 ? -1 : frac < 0 ? 1 : 0;
}

bool cf_greater(CfValue a, CfValue b) {
    if (a.kind == CF_INT && b.kind == CF_INT) return a.i > b.i;
    if (a.kind == CF_FLOAT && b.kind == CF_FLOAT) return a.f > b.f;
    if (a.kind == CF_INT && b.kind == CF_FLOAT) return cmp_int_float(a.i, b.f) == 1;
    if (a.kind == CF_FLOAT && b.kind == CF_INT) return cmp_int_float(b.i, a.f) == -1;
    if (a.kind == CF_STR && b.kind == CF_STR) return avp_str_cmp(a.s, b.s) > 0;
    avp_throw(AVP_ERR_OTHER, "TypeError: '>' not supported between instances of '%s' and '%s'", value_type(a),
              value_type(b));
}

AvpStr cf_format_float(AvpArena *a, CfValue v) {
    double d;
    if (v.kind == CF_INT) d = (double)v.i;
    else if (v.kind == CF_FLOAT) d = v.f;
    else avp_throw(AVP_ERR_OTHER, "TypeError: must be real number, not %s", value_type(v));
    if (d < 100.0 && d > 99.99) d = 99.99;
    return avp_pyfmt(a, "%6.2f", d);
}

/* ---- get_changes ---- */

/* object.get(name): NULL when absent; AttributeError when `object` is not a dict. */
static const AvpJson *dict_get(const AvpJson *object, const char *name) {
    if (object->kind != AVP_JSON_OBJECT)
        avp_throw(AVP_ERR_OTHER, "AttributeError: '%s' object has no attribute 'get'", type_name(object));
    return avp_json_getz(object, name);
}

/* object["name"] */
static const AvpJson *dict_item(const AvpJson *object, const char *name) {
    if (object->kind == AVP_JSON_STRING || object->kind == AVP_JSON_ARRAY)
        avp_throw(AVP_ERR_OTHER, "TypeError: %s indices must be integers or slices, not str", type_name(object));
    if (object->kind != AVP_JSON_OBJECT)
        avp_throw(AVP_ERR_OTHER, "TypeError: '%s' object is not subscriptable", type_name(object));
    const AvpJson *v = avp_json_getz(object, name);
    if (!v) avp_throw(AVP_ERR_OTHER, "KeyError: '%s'", name);
    return v;
}

/* `for item in value` where each item is then subscripted with a str: a list yields its items; an empty dict or str
 * iterates to nothing; any other value raises at the first item (or at iter()). */
static size_t items_of(const AvpJson *v) {
    if (v->kind == AVP_JSON_ARRAY) return avp_json_len(v);
    if (v->kind == AVP_JSON_OBJECT || v->kind == AVP_JSON_STRING) {
        if (avp_json_len(v) == 0) return 0;
        avp_throw(AVP_ERR_OTHER, "TypeError: string indices must be integers, not 'str'");
    }
    avp_throw(AVP_ERR_OTHER, "TypeError: '%s' object is not iterable", type_name(v));
}

static void diff_key(CfChanges *reg, CfChanges *prog, const AvpJson *name, const AvpJson *object, const char *key) {
    const AvpJson *from = dict_get(object, "from");
    const AvpJson *to = dict_get(object, "to");
    CfValue fv = cf_value_from_json(from ? dict_get(from, key) : NULL);
    CfValue tv = cf_value_from_json(to ? dict_get(to, key) : NULL);
    CfChange c = {0};
    if (name && name->kind == AVP_JSON_STRING) {
        c.has_name = true;
        c.name = name->u.str;
    } else if (name && name->kind != AVP_JSON_NULL) {
        c.bad_type = type_name(name);
    }
    c.key = avp_str_removesuffix(avp_str(key), AVP_S("_percent"));
    c.from = fv;
    c.to = tv;
    if (cf_greater(fv, tv)) avp_vec_push(reg, c);
    else if (cf_greater(tv, fv)) avp_vec_push(prog, c);
}

void cf_get_changes(const AvpJson *root, CfChanges *reg, CfChanges *prog) {
    for (size_t k = 0; k < COUNT(UNIT_KEYS); k++) diff_key(reg, prog, NULL, root, UNIT_KEYS[k]);
    const AvpJson *units = dict_get(root, "units");
    if (!units) return;
    size_t nu = items_of(units);
    for (size_t u = 0; u < nu; u++) {
        const AvpJson *unit = avp_json_at(units, u);
        const AvpJson *unit_name = dict_item(unit, "name");
        for (size_t k = 0; k < COUNT(UNIT_KEYS); k++) diff_key(reg, prog, unit_name, unit, UNIT_KEYS[k]);
        const AvpJson *funcs = dict_get(unit, "functions");
        if (!funcs) continue;
        size_t nf = items_of(funcs);
        for (size_t f = 0; f < nf; f++) {
            const AvpJson *func = avp_json_at(funcs, f);
            const AvpJson *func_name = dict_item(func, "name");
            for (size_t k = 0; k < COUNT(FUNCTION_KEYS); k++) diff_key(reg, prog, func_name, func, FUNCTION_KEYS[k]);
        }
    }
}

/* ---- text ---- */

static void check_name(const CfChange *c) {
    if (c->bad_type) avp_throw(AVP_ERR_OTHER, "TypeError: object of type '%s' has no len()", c->bad_type);
}

AvpStr cf_plaintext(AvpArena *a, const CfChange *changes, size_t n) {
    if (n == 0) return AVP_S("");
    const i64 table_total_width = 136, percents_max_len = 7 + 4 + 7;
    i64 key_max_len = 0, name_max_len = 0;
    for (size_t i = 0; i < n; i++) {
        check_name(&changes[i]);
        i64 kl = (i64)avp_u8_len(changes[i].key);
        /* len(name or "Total"): an empty name counts as "Total" */
        i64 nl = changes[i].has_name && changes[i].name.n ? (i64)avp_u8_len(changes[i].name) : 5;
        if (kl > key_max_len) key_max_len = kl;
        if (nl > name_max_len) name_max_len = nl;
    }
    i64 max_width_for_name_col = table_total_width - 3 - key_max_len - 3 - percents_max_len;
    if (max_width_for_name_col < name_max_len) name_max_len = max_width_for_name_col;
    AvpBuf out;
    avp_buf_init(&out, a);
    for (size_t i = 0; i < n; i++) {
        const CfChange *c = &changes[i];
        AvpStr name = c->has_name ? c->name : AVP_S("Total");
        if ((i64)avp_u8_len(name) > name_max_len)
            name = avp_str_concat(a, avp_u8_slice(name, 0, name_max_len - 5), AVP_S("[...]"));
        if (i) avp_buf_putc(&out, '\n');
        avp_buf_put(&out, avp_str_rjust(a, name, (size_t)name_max_len, ' '));
        avp_buf_putz(&out, " | ");
        avp_buf_put(&out, avp_str_ljust(a, c->key, (size_t)key_max_len, ' '));
        avp_buf_putz(&out, " | ");
        avp_buf_put(&out, cf_format_float(a, c->from));
        avp_buf_putz(&out, "% -> ");
        avp_buf_put(&out, cf_format_float(a, c->to));
        avp_buf_putc(&out, '%');
    }
    return avp_buf_str(&out);
}

AvpStr cf_markdown(AvpArena *a, const CfChange *changes, size_t n, const char *description) {
    if (n == 0) return AVP_S("");
    const i64 name_max_len = 100;
    AvpBuf out;
    avp_buf_init(&out, a);
    avp_buf_putz(&out, "<details>\n");
    avp_buf_printf(&out, "<summary>Detected %zu %s compared to the base:</summary>\n", n, description);
    avp_buf_putz(&out, "\n| Name | Type | Before | After |\n| ---- | ---- | ------ | ----- |\n");
    for (size_t i = 0; i < n; i++) {
        const CfChange *c = &changes[i];
        AvpStr name = AVP_S("Total");
        if (c->bad_type) check_name(c);
        if (c->has_name) {
            name = c->name;
            if ((i64)avp_u8_len(name) > name_max_len)
                name = avp_str_concat(a, avp_u8_slice(name, 0, name_max_len - 3), AVP_S("..."));
            name = avp_str_concat(a, avp_str_concat(a, AVP_S("`"), name), AVP_S("`"));
        }
        /* key.replace("_", " ").capitalize(): the keys are ASCII */
        char *kz = avp_cz(a, avp_str_replace(a, c->key, AVP_S("_"), AVP_S(" "), -1));
        for (size_t j = 0; kz[j]; j++)
            kz[j] = (char)(j == 0 ? toupper((unsigned char)kz[j]) : tolower((unsigned char)kz[j]));
        avp_buf_putz(&out, "| ");
        avp_buf_put(&out, name);
        avp_buf_putz(&out, " | ");
        avp_buf_putz(&out, kz);
        avp_buf_putz(&out, " | ");
        avp_buf_put(&out, cf_format_float(a, c->from));
        avp_buf_putz(&out, "% | ");
        avp_buf_put(&out, cf_format_float(a, c->to));
        avp_buf_putz(&out, "% |\n");
    }
    avp_buf_putz(&out, "</details>");
    return avp_buf_str(&out);
}
