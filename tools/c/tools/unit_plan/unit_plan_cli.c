/* unit_plan.py main(), run(), cmd_check and cmd_plan. */
#include "unit_plan.h"

#include <string.h>

static const char DESCRIPTION[] = "Propose translation-unit boundaries with their evidence.";

typedef struct {
    bool detail;
    const char *target, *lib, *min_conf;
} UpArgs;

static AvpStr pct(AvpArena *a, i64 x, i64 y) {
    if (!y) return AVP_S("    - ");
    return avp_pyfmt(a, "%5.1f%%", 100.0 * (double)x / (double)y);
}

static int cmd_check(AvpArena *a, const UpArgs *ua, const UpState *s, size_t nunits, ShsmOutput *out) {
    const UpModel *m = s->m;
    UpEvaluation e = up_evaluate(a, m, &s->ev, s->lab);
    const UpCalib *calib = e.calib;
    UpBands b = up_bands(m, &s->ev, calib, s->lab, 0.6);
    i64 labelled = 0, vetoed = 0, vetoed_cuts = 0;
    for (size_t g = 0; g < s->ev.ngaps; g++) {
        if (s->lab[g] < 0) continue;
        labelled++;
        if (s->ev.tags[g] & UP_BIT(UP_T_INCLASS)) {
            vetoed++;
            vetoed_cuts += s->lab[g];
        }
    }
    shsm_output_line(out, avp_pyfmt(a,
                                    "%d labelled gaps (%d true cuts, %d inside units) from %d source units; confidence"
                                    " = in-sample precision of the tag combination",
                                    labelled, e.ncuts, labelled - e.ncuts, (i64)nunits));
    shsm_output_line(out, avp_pyfmt(a, "class evidence: %d classes; %d labelled gaps inside one, %d of them true cuts"
                                       " (wrongly vetoed)%s",
                                    (i64)m->nclasses, vetoed, vetoed_cuts,
                                    m->class_note ? avp_sprintf(a, "; %s", m->class_note) : ""));
    static const char *const BAND[3] = {"high", "medium+", "all"};
    for (int k = 0; k < 3; k++) {
        AvpStr p1 = pct(a, b.tp[k], b.tp[k] + b.fp[k]), p2 = pct(a, b.tp[k], b.tp[k] + b.missed[k]);
        shsm_output_line(out, avp_pyfmt(a, "cuts %-8s precision %S  recall %S  (right %d, wrong %d, missed %d)",
                                        BAND[k], &p1, &p2, b.tp[k], b.fp[k], b.missed[k]));
    }
    shsm_output_linez(out, "tag        predicted  right  precision  recall-of-true-cuts");
    for (int t = 0; t < UP_NPOSITIVE; t++) {
        UpCount c = calib->tag[t];
        AvpStr p1 = pct(a, c.hit, c.total), p2 = pct(a, e.recall[t], e.ncuts);
        shsm_output_line(out, avp_pyfmt(a, "%-10s %9d  %5d  %S     %S", UP_TAG_NAMES[t], c.total, c.hit, &p1, &p2));
    }
    shsm_output_linez(out, "PS2 order with other evidence  gaps  cuts  ratio (odds factor)");
    for (int k = 0; k < UP_NPS2; k++) {
        i64 with = calib->ps2_cut[k].hit + calib->ps2_other[k].hit;
        shsm_output_line(out, avp_pyfmt(a, "%-29s %5d %5d  %.2f", UP_TAG_NAMES[UP_PS2_TAG[k]], with,
                                        calib->ps2_cut[k].hit, up_ps2_ratio(calib, k)));
    }
    shsm_output_linez(out, "library (medium+ cuts)          cuts right wrong missed  precision  recall");
    size_t nl = b.libs.n;
    UpLibRow *rows = AVP_NEWN(a, UpLibRow, nl ? nl : 1);
    if (nl) memcpy(rows, b.libs.v, nl * sizeof *rows);
    for (size_t i = 1; i < nl; i++) { /* stable sort by -cuts */
        UpLibRow x = rows[i];
        size_t j = i;
        while (j && rows[j - 1].cuts < x.cuts) {
            rows[j] = rows[j - 1];
            j--;
        }
        rows[j] = x;
    }
    for (size_t i = 0; i < nl; i++) {
        const UpLibRow *r = &rows[i];
        AvpStr lib = avp_str(r->lib);
        if (avp_u8_len(lib) > 30) lib = avp_u8_slice(lib, 0, 30);
        AvpStr p1 = pct(a, r->tp, r->tp + r->fp), p2 = pct(a, r->tp, r->tp + r->missed);
        shsm_output_line(out, avp_pyfmt(a, "%-30S %5d %5d %5d %6d  %S     %S", &lib, r->cuts, r->tp, r->fp, r->missed,
                                        &p1, &p2));
    }
    if (ua->detail) {
        shsm_output_linez(out, "tag combination                     predicted  precision");
        size_t nc = calib->combo.n;
        size_t *ord = AVP_NEWN(a, size_t, nc ? nc : 1);
        for (size_t i = 0; i < nc; i++) ord[i] = i;
        for (size_t i = 1; i < nc; i++) {
            size_t x = ord[i], j = i;
            while (j && calib->combo.v[ord[j - 1]].total < calib->combo.v[x].total) {
                ord[j] = ord[j - 1];
                j--;
            }
            ord[j] = x;
        }
        for (size_t i = 0; i < nc; i++) {
            UpCount c = calib->combo.v[ord[i]];
            AvpStr p1 = pct(a, c.hit, c.total);
            shsm_output_line(out, avp_pyfmt(a, "%-35S %9d  %S", &calib->combo_keys.v[ord[i]], c.total, &p1));
        }
    }
    if (out->as_json) {
        shsm_output_data(out, "labelled", avp_json_int(a, labelled));
        shsm_output_data(out, "true_cuts", avp_json_int(a, e.ncuts));
        AvpJson *bands = avp_json_obj(a);
        for (int k = 0; k < 3; k++) {
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("tp"), avp_json_int(a, b.tp[k]));
            avp_json_set(a, o, AVP_S("fp"), avp_json_int(a, b.fp[k]));
            avp_json_set(a, o, AVP_S("missed"), avp_json_int(a, b.missed[k]));
            avp_json_set(a, bands, avp_str(BAND[k]), o);
        }
        shsm_output_data(out, "bands", bands);
        shsm_output_data(out, "tags", up_calib_tag_json(a, calib));
        AvpJson *combos = avp_json_obj(a);
        for (size_t i = 0; i < calib->combo.n; i++) {
            AvpJson *pair = avp_json_arr(a);
            avp_json_arr_push(a, pair, avp_json_int(a, calib->combo.v[i].hit));
            avp_json_arr_push(a, pair, avp_json_int(a, calib->combo.v[i].total));
            avp_json_set(a, combos, calib->combo_keys.v[i], pair);
        }
        shsm_output_data(out, "combos", combos);
        AvpJson *libs = avp_json_obj(a);
        for (size_t i = 0; i < nl; i++) {
            const UpLibRow *r = &b.libs.v[i];
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("cuts"), avp_json_int(a, r->cuts));
            avp_json_set(a, o, AVP_S("tp"), avp_json_int(a, r->tp));
            avp_json_set(a, o, AVP_S("fp"), avp_json_int(a, r->fp));
            avp_json_set(a, o, AVP_S("missed"), avp_json_int(a, r->missed));
            avp_json_set(a, libs, avp_str(r->lib), o);
        }
        shsm_output_data(out, "libraries", libs);
        AvpJson *rec = avp_json_obj(a);
        for (int t = 0; t < UP_NPOSITIVE; t++) avp_json_set(a, rec, avp_str(UP_TAG_NAMES[t]), avp_json_int(a, e.recall[t]));
        shsm_output_data(out, "tag_recall", rec);
    }
    return SHSM_EXIT_OK;
}

static int cmd_plan(AvpArena *a, const UpArgs *ua, const ShsmRepo *r, ShsmAsmIndex *ix, ShsmOutput *out) {
    i64 start, end;
    const char *title;
    if (ua->lib && *ua->lib) {
        UpLibRange lr = up_find_library(a, ua->lib, r, NULL);
        start = lr.start;
        end = lr.end;
        AvpStr joined = avp_str_join(a, AVP_S(" + "), lr.names.v, lr.names.n);
        title = avp_sprintf(a, "%s (0x%08llX-0x%08llX)", avp_cz(a, joined), (long long)start, (long long)end);
    } else {
        up_parse_range(a, ua->target, &start, &end);
        title = avp_sprintf(a, "0x%08llX-0x%08llX", (long long)start, (long long)end);
    }
    UpState *s = up_load_cached(a, r, ix, true);
    up_warn_classes(s->m);
    UpUnits res = up_plan(a, s->m, start, end, s->calib, &s->ev, up_min_p(ua->min_conf), s->lab);
    shsm_output_line(out, avp_pyfmt(a, "# unit plan for %s: %d proposed unit(s)", title, (i64)res.n));
    AvpStr header = up_accuracy_header(a, s->calib);
    shsm_output_line(out, avp_pyfmt(a, "# %S", &header));
    for (size_t i = 0; i < res.n; i++) {
        const UpUnit *u = &res.v[i];
        AvpStr tags = up_tag_key(a, u->end_tags);
        tags = avp_str_replace(a, tags, AVP_S("+"), AVP_S(","), -1);
        if (!tags.n) tags = AVP_S("(range end)");
        AvpStr known = {"", 0};
        if (u->known_units.n) {
            AvpStr j = avp_str_join(a, AVP_S(", "), u->known_units.v, u->known_units.n);
            known = avp_pyfmt(a, "  known: %S", &j);
        }
        shsm_output_line(out, avp_pyfmt(a, "%08X-%08X %4d fn  end:%-24S %-6s%s%S", u->start, u->end, u->functions,
                                        &tags, up_conf(u->has_conf, u->confidence), u->sinit ? "  sinit" : "", &known));
        if (u->names.n) {
            AvpStr j = avp_str_join(a, AVP_S(", "), u->names.v, u->names.n);
            shsm_output_line(out, avp_pyfmt(a, "    names: %S", &j));
        }
        shsm_output_line(out, avp_pyfmt(a, "    %s", u->command));
    }
    if (out->as_json) {
        AvpJson *rng = avp_json_arr(a);
        avp_json_arr_push(a, rng, avp_json_int(a, start));
        avp_json_arr_push(a, rng, avp_json_int(a, end));
        shsm_output_data(out, "range", rng);
        shsm_output_data(out, "accuracy", up_calib_tag_json(a, s->calib));
        AvpJson *units = avp_json_arr(a);
        for (size_t i = 0; i < res.n; i++) avp_json_arr_push(a, units, up_unit_json(a, &res.v[i]));
        shsm_output_data(out, "units", units);
    }
    return SHSM_EXIT_OK;
}

static const char *opt_str(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    UpArgs ua = {0};
    ua.target = opt_str(a, args, "target");
    ua.lib = opt_str(a, args, "lib");
    ua.min_conf = opt_str(a, args, "min_conf");
    ua.detail = avp_args_bool(args, "detail");
    const ShsmRepo *r = shsm_repo_find(opt_str(a, args, "root"));
    bool is_check = ua.target && strcmp(ua.target, "check") == 0;
    if (!is_check && !(ua.target && *ua.target) && !(ua.lib && *ua.lib))
        shsm_raise(SHSM_EXC_USAGE, "give START-END, --lib L or check");
    ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = r, .readonly = true});
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    int rc;
    if (is_check) {
        UpState *s = up_load_cached(a, r, ix, true);
        up_warn_classes(s->m);
        size_t nunits = 0;
        (void)up_truth_splits(a, s->m, r, &nunits);
        rc = cmd_check(a, &ua, s, nunits, out);
    } else {
        rc = cmd_plan(a, &ua, r, ix, out);
    }
    shsm_output_close(out);
    shsm_asmindex_close(ix);
    return rc;
}

int unit_plan_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("unit_plan.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "target", .nargs = AVP_NARGS_OPT, .dflt = AVP_DEF_NONE,
                              .help = "START-END (hex) or `check`"});
    avp_arg(ap, &(AvpArgDesc){.names = "--lib", .dflt = AVP_DEF_NONE,
                              .help = "a libraries.tsv library, its last component or its group"});
    static const char *const confs[] = {"low", "medium", "high", NULL};
    avp_arg(ap, &(AvpArgDesc){.names = "--min-conf", .choices = confs, .dflt = AVP_DEF_STR("low")});
    avp_arg(ap, &(AvpArgDesc){.names = "--detail", .action = AVP_ARG_STORE_TRUE,
                              .help = "check: precision per tag combination"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
