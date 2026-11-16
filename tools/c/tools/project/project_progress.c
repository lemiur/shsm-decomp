/* project.py: calculate_progress (the `progress` mode): the progress lines from build/<version>/report.json. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "avp/fmt.h"
#include "project_int.h"

/* convert_numbers: a str value made of digits becomes an int */
static void convert_numbers(AvpArena *a, AvpJson *measures) {
    if (!avp_json_is_obj(measures)) return;
    for (size_t i = 0; i < avp_json_len(measures); i++) {
        const AvpJsonMember *m = avp_json_member(measures, i);
        if (avp_json_is_str(m->val) && m->val->u.str.n && avp_str_isdigit(m->val->u.str)) {
            bool ascii = true;
            for (size_t k = 0; k < m->val->u.str.n; k++)
                if (m->val->u.str.p[k] < '0' || m->val->u.str.p[k] > '9') ascii = false;
            if (!ascii) proj_exit("configure: error: non-ASCII digits in report.json are not supported");
            avp_json_set(a, measures, m->key, avp_json_int_digits(a, m->val->u.str));
        }
    }
}

typedef struct {
    FILE *summary;
} Out;

static void progress_print(Out *o, const char *s) {
    printf("%s\n", s);
    if (o->summary) fprintf(o->summary, "%s\n", s);
}

static AvpJson *get0(AvpArena *a, AvpJson *measures, const char *key) {
    AvpJson *v = avp_json_get(measures, avp_str(key));
    return v ? v : avp_json_int(a, 0);
}

/* f"{v}" */
static const char *fmt_value(AvpArena *a, const AvpJson *v) {
    switch (v->kind) {
    case AVP_JSON_NULL: return "None";
    case AVP_JSON_BOOL: return v->b ? "True" : "False";
    case AVP_JSON_STRING: return avp_cz(a, v->u.str);
    default: return avp_cz(a, avp_json_dumps(a, v, NULL));
    }
}

/* f"{v:.2f}" */
static const char *fmt_pct(AvpArena *a, const AvpJson *v) {
    if (!avp_json_is_num(v) || v->kind == AVP_JSON_BOOL)
        proj_exit("ValueError: Unknown format code 'f' for object of type '%s'", v->kind == AVP_JSON_STRING ? "str" : "NoneType");
    return avp_cz(a, avp_format_f64(a, avp_json_f64(v, 0), ".2f"));
}

static void print_category(AvpArena *a, Out *o, const char *name, AvpJson *m) {
    progress_print(o, avp_sprintf(a, "  %s: %s%% fuzzy, %s%% matched, %s%% linked (%s / %s files)", name,
                                  fmt_pct(a, get0(a, m, "fuzzy_match_percent")), fmt_pct(a, get0(a, m, "matched_code_percent")),
                                  fmt_pct(a, get0(a, m, "complete_code_percent")), fmt_value(a, get0(a, m, "complete_units")),
                                  fmt_value(a, get0(a, m, "total_units"))));
    progress_print(o, avp_sprintf(a, "    Code: %s / %s bytes (%s / %s functions)", fmt_value(a, get0(a, m, "matched_code")),
                                  fmt_value(a, get0(a, m, "total_code")), fmt_value(a, get0(a, m, "matched_functions")),
                                  fmt_value(a, get0(a, m, "total_functions"))));
    progress_print(o, avp_sprintf(a, "    Data: %s / %s bytes (%s%%)", fmt_value(a, get0(a, m, "matched_data")),
                                  fmt_value(a, get0(a, m, "total_data")), fmt_pct(a, get0(a, m, "matched_data_percent"))));
}

static double num(const AvpJson *v) { return avp_json_f64(v, 0); }

static bool is_zero(const AvpJson *v) { return avp_json_is_num(v) && v->kind != AVP_JSON_BOOL ? num(v) == 0 : (v->kind == AVP_JSON_BOOL && !v->b); }

int project_calculate_progress(AvpArena *a, Pv *config, const ProjectEnv *env) {
    PCtx ctx = {a, config, env, NULL, NULL};
    PCtx *c = &ctx;
    proj_validate(c);
    const char *report_path = proj_join(a, proj_out_path(c), "report.json");
    if (!avp_isfile(report_path)) proj_exit("Report file %s does not exist", report_path);
    AvpJson *report = proj_load_json(a, report_path);
    AvpJson *measures = avp_json_get(report, AVP_S("measures"));
    if (!measures) proj_exit("KeyError: 'measures'");
    convert_numbers(a, measures);
    AvpJson *categories = avp_json_get(report, AVP_S("categories"));
    for (size_t i = 0; i < avp_json_len(categories); i++) convert_numbers(a, avp_json_get(avp_json_at(categories, i), AVP_S("measures")));

    Out o = {0};
    char *summary_path = avp_getenv(a, "GITHUB_STEP_SUMMARY");
    if (summary_path && *summary_path) {
        o.summary = fopen(summary_path, "a");
        if (!o.summary) proj_exit("FileNotFoundError: %s", summary_path);
        fputs("```\n", o.summary);
    }
    progress_print(&o, "Progress:");
    print_category(a, &o, "All", measures);
    Pv *ppc = proj_attr(c, "print_progress_categories");
    for (size_t i = 0; i < avp_json_len(categories); i++) {
        AvpJson *cat = avp_json_at(categories, i);
        bool show = ppc->kind == PV_BOOL && ppc->u.b;
        if (ppc->kind == PV_LIST) {
            AvpJson *id = avp_json_get(cat, AVP_S("id"));
            for (size_t k = 0; k < ppc->u.list->n && !show; k++) {
                Pv *x = ppc->u.list->v[k];
                if (x->kind == PV_STR && avp_json_is_str(id) && avp_str_eq(x->u.s, id->u.str)) show = true;
            }
        }
        if (show)
            print_category(a, &o, fmt_value(a, avp_json_get(cat, AVP_S("name"))), avp_json_get(cat, AVP_S("measures")));
    }
    if (pv_truthy(proj_attr(c, "progress_use_fancy"))) {
        AvpJson *total_code = get0(a, measures, "total_code"), *total_data = get0(a, measures, "total_data");
        if (is_zero(total_code) || is_zero(total_data)) {
            if (o.summary) fclose(o.summary); /* the Python returns here without the closing fence */
            return 0;
        }
        double code_frac = num(get0(a, measures, "complete_code")) / num(total_code);
        double data_frac = num(get0(a, measures, "complete_data")) / num(total_data);
        Pv *cf = proj_attr(c, "progress_code_fancy_frac"), *df = proj_attr(c, "progress_data_fancy_frac");
        progress_print(&o, avp_sprintf(a, "\nYou have %lld out of %s %s and %lld out of %s %s.",
                                       (long long)floor(code_frac * (double)cf->u.i), proj_pv_str(a, cf),
                                       proj_pv_str(a, proj_attr(c, "progress_code_fancy_item")),
                                       (long long)floor(data_frac * (double)df->u.i), proj_pv_str(a, df),
                                       proj_pv_str(a, proj_attr(c, "progress_data_fancy_item"))));
    }
    if (o.summary) {
        fputs("```\n", o.summary);
        fclose(o.summary);
    }
    return 0;
}
