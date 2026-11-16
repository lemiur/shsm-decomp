/* objdiff report reader (tools/common/report.py). */
#include "shsm_report.h"

#include "avp/fs.h"
#include "shsm_cli.h"

/* _num(v, 0) for an integer field: an int, or the int of a digit string (objdiff writes u64 as strings). */
static i64 num(const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return 0;
    if (v->kind == AVP_JSON_INT) return v->u.i;
    if (v->kind == AVP_JSON_FLOAT) return (i64)v->u.f;
    if (v->kind == AVP_JSON_BOOL) return v->b ? 1 : 0;
    if (v->kind == AVP_JSON_STRING) {
        i64 out = 0;
        if (avp_parse_int(v->u.str, 10, &out, NULL)) return out;
        double f = 0;
        if (avp_parse_float(v->u.str, &f)) return (i64)f;
        shsm_raise(SHSM_EXC_VALUE, "could not convert string to float: '%.*s'", AVP_SF(v->u.str));
    }
    return 0;
}

static double fnum(const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return 0.0;
    if (v->kind == AVP_JSON_STRING) {
        double f = 0;
        if (!avp_parse_float(v->u.str, &f))
            shsm_raise(SHSM_EXC_VALUE, "could not convert string to float: '%.*s'", AVP_SF(v->u.str));
        return f;
    }
    return avp_json_f64(v, 0.0);
}

static const char *str_or_null(AvpArena *a, const AvpJson *v) {
    return avp_json_is_str(v) ? avp_cz(a, v->u.str) : NULL;
}

static ShsmUnitReport *unit_report(AvpArena *a, AvpJson *d) {
    ShsmUnitReport *u = AVP_NEW(a, ShsmUnitReport);
    u->name = str_or_null(a, avp_json_getz(d, "name"));
    if (!u->name) shsm_raise(SHSM_EXC_KEY, "name");
    u->measures = avp_json_getz(d, "measures");
    u->fuzzy = fnum(avp_json_getz(u->measures, "fuzzy_match_percent"));
    u->metadata = avp_json_getz(d, "metadata");
    if (!avp_json_truthy(u->metadata)) u->metadata = avp_json_obj(a);
    u->complete = avp_json_truthy(avp_json_getz(u->metadata, "complete"));
    u->auto_generated = avp_json_truthy(avp_json_getz(u->metadata, "auto_generated"));
    u->source_path = str_or_null(a, avp_json_getz(u->metadata, "source_path"));
    AvpJson *fns = avp_json_getz(d, "functions");
    size_t n = avp_json_is_arr(fns) ? avp_json_len(fns) : 0;
    u->functions = AVP_NEWN(a, ShsmFunctionReport, n + 1);
    u->first_by_name = avp_map_new_cap(n);
    for (size_t i = 0; i < n; i++) {
        AvpJson *f = avp_json_at(fns, i);
        ShsmFunctionReport *fr = &u->functions[u->nfunctions];
        fr->name = str_or_null(a, avp_json_getz(f, "name"));
        if (!fr->name) shsm_raise(SHSM_EXC_KEY, "name");
        fr->unit = u->name;
        fr->size = num(avp_json_getz(f, "size"));
        AvpJson *fz = avp_json_getz(f, "fuzzy_match_percent");
        fr->scored = fz && fz->kind != AVP_JSON_NULL;
        fr->fuzzy = avp_json_truthy(fz) ? fnum(fz) : 0.0;
        fr->metadata = avp_json_getz(f, "metadata");
        if (!avp_json_truthy(fr->metadata)) fr->metadata = avp_json_obj(a);
        AvpJson *va = avp_json_getz(fr->metadata, "virtual_address");
        if (va && va->kind != AVP_JSON_NULL) {
            fr->has_address = true;
            fr->address = (u64)num(va);
        }
        AvpJson *off = avp_json_getz(f, "address");
        if (off && off->kind != AVP_JSON_NULL) {
            fr->has_offset = true;
            fr->offset = num(off);
        }
        fr->demangled = str_or_null(a, avp_json_getz(fr->metadata, "demangled_name"));
        bool created = false;
        AvpVal *slot = avp_map_put(u->first_by_name, avp_str(fr->name), &created);
        if (created) slot->i = (i64)u->nfunctions;
        u->nfunctions++;
    }
    return u;
}

ShsmReport *shsm_read_report(const char *path) {
    AvpArena *a = avp_arena();
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) shsm_raise_io();
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(a, text, &err);
    if (!doc) {
        AvpStr m = avp_json_error_str(a, &err);
        shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(m));
    }
    ShsmReport *rep = AVP_NEW(a, ShsmReport);
    rep->path = avp_strdup(a, path);
    rep->measures = avp_json_getz(doc, "measures");
    rep->index = avp_map_new();
    rep->fn_first = avp_map_new();
    AvpJson *units = avp_json_getz(doc, "units");
    size_t n = avp_json_is_arr(units) ? avp_json_len(units) : 0;
    rep->units = AVP_NEWN(a, ShsmUnitReport *, n + 1);
    for (size_t i = 0; i < n; i++) {
        ShsmUnitReport *u = unit_report(a, avp_json_at(units, i));
        bool created = false;
        AvpVal *slot = avp_map_put(rep->index, avp_str(u->name), &created);
        if (created) slot->i = (i64)rep->nunits++;
        rep->units[slot->i] = u;
    }
    for (size_t i = 0; i < rep->nunits; i++) {
        ShsmUnitReport *u = rep->units[i];
        AvpMapIter it = {0};
        while (avp_map_next(u->first_by_name, &it)) {
            bool created = false;
            AvpVal *slot = avp_map_put(rep->fn_first, it.key, &created);
            if (created) slot->p = &u->functions[it.val->i];
        }
    }
    return rep;
}

ShsmReport *shsm_load_report(const ShsmRepo *r) {
    if (!avp_exists(r->report_json))
        shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "no report at %s (build it with ninja first)", r->report_json);
    return shsm_read_report(r->report_json);
}

ShsmReport *shsm_load_baseline(const ShsmRepo *r) { return shsm_read_report(r->baseline_report_json); }

const ShsmUnitReport *shsm_report_unit(const ShsmReport *rep, const char *unit) {
    AvpVal *v = avp_map_get(rep->index, avp_str(shsm_unit_name(avp_arena(), unit)));
    return v ? rep->units[v->i] : NULL;
}

const ShsmFunctionReport *shsm_unit_report_function(const ShsmUnitReport *u, const char *name) {
    AvpVal *v = avp_map_get(u->first_by_name, avp_str(name));
    return v ? &u->functions[v->i] : NULL;
}

const ShsmFunctionReport *shsm_report_function(const ShsmReport *rep, const char *name, const char *unit) {
    if (unit) {
        const ShsmUnitReport *u = shsm_report_unit(rep, unit);
        return u ? shsm_unit_report_function(u, name) : NULL;
    }
    AvpVal *v = avp_map_get(rep->fn_first, avp_str(name));
    return v ? (const ShsmFunctionReport *)v->p : NULL;
}
