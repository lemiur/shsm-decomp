/* pick.py build_parser() and run(): the sections of each mode, capped per section, and the --json document. */
#include "pick.h"

#include <string.h>

static const char DESCRIPTION[] =
    "Ranked work lists: lost matches, nearly complete units, near-exact functions, unstarted functions, "
    "MATCH-PENDING functions, mechanical drafts from matched twins.";
static const char *const MODES[] = {"lost", "close", "near", "fresh", "pending", "drafts", NULL};

static bool size_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    (void)ctx, (void)a, (void)msg;
    i64 v = 0;
    if (!avp_parse_int(in, 0, &v, NULL)) return false;
    out->kind = AVP_V_INT;
    out->i = v;
    return true;
}

typedef struct {
    ShsmOutput *out;
    bool summary, show_all;
    i64 limit;
} Sections;

typedef AvpStr (*FmtFn)(AvpArena *a, const AvpJson *it);

/* capped(items): {"count": n, "items": the shown items} */
static AvpJson *capped(AvpArena *a, const Sections *s, const PickItems *items) {
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("count"), avp_json_int(a, (i64)items->n));
    AvpJson *arr = avp_json_arr(a);
    size_t n = s->show_all || (size_t)s->limit >= items->n ? items->n : (size_t)s->limit;
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, items->v[i]);
    avp_json_set(a, d, AVP_S("items"), arr);
    return d;
}

static void section(AvpArena *a, const Sections *s, const char *title, const PickItems *items, FmtFn fmt,
                    const char *note) {
    if (s->summary) {
        AvpStr tail = note ? avp_pyfmt(a, " (%s)", note) : AVP_S("");
        shsm_output_line(s->out, avp_pyfmt(a, "== %s: %d%S", title, (i64)items->n, &tail));
    } else if (note) {
        shsm_output_line(s->out, avp_pyfmt(a, "%s: %s", title, note));
    }
    size_t shown = s->show_all || (size_t)s->limit >= items->n ? items->n : (size_t)s->limit;
    for (size_t i = 0; i < shown; i++) shsm_output_line(s->out, fmt(a, items->v[i]));
    if (items->n > shown) {
        if (s->summary) {
            AvpStr t = avp_str(title), first, rest;
            if (!avp_str_partition(t, AVP_S(" "), &first, &rest)) first = t;
            shsm_output_line(s->out, avp_pyfmt(a, "   ... %d more: build/tools/pick.exe %S", (i64)(items->n - shown), &first));
        } else {
            shsm_output_line(s->out, avp_pyfmt(a, "... %d more, use --all or --limit N", (i64)(items->n - shown)));
        }
    }
    if (s->summary) shsm_output_line(s->out, AVP_S(""));
}

static AvpJson *jnote(AvpArena *a, const char *note) { return note ? avp_json_strz(a, note) : avp_json_null(); }

static const char *opt_cz(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static bool want(const char *mode, const char *m) { return !mode || strcmp(mode, m) == 0; }

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    PickArgs o;
    pick_args_default(&o);
    o.mode = opt_cz(a, args, "mode");
    o.lib = opt_cz(a, args, "lib");
    o.unit = opt_cz(a, args, "unit");
    if ((o.has_min_size = !avp_args_is_none(args, "min_size"))) o.min_size = avp_args_int(args, "min_size");
    if ((o.has_max_size = !avp_args_is_none(args, "max_size"))) o.max_size = avp_args_int(args, "max_size");
    o.threshold = avp_args_float(args, "threshold");
    o.small = avp_args_bool(args, "small");
    o.leaf = avp_args_bool(args, "leaf");
    o.tiny = avp_args_int(args, "tiny");
    o.checker = avp_args_bool(args, "checker");
    o.all = avp_args_bool(args, "all");
    o.limit = avp_args_int(args, "limit");

    PickWorld *w = pick_world_new(a, opt_cz(a, args, "root"), opt_cz(a, args, "report"));
    Sections s = {0};
    s.summary = o.mode == NULL;
    s.limit = s.summary ? PICK_SECTION_LIMIT : o.limit;
    s.show_all = o.all && !s.summary;
    s.out = shsm_output_open(args, 0, -1, -1);
    const char *stale = pick_stale_note(a, w->repo, w->report_path, true);
    if (stale) shsm_output_linez(s.out, stale);
    AvpJson *data = avp_json_obj(a);
    avp_json_set(a, data, AVP_S("report"), avp_json_strz(a, w->report_path));
    avp_json_set(a, data, AVP_S("stale_report"), jnote(a, stale));
    AvpJson *filters = avp_json_obj(a);
    avp_json_set(a, filters, AVP_S("lib"), jnote(a, o.lib));
    avp_json_set(a, filters, AVP_S("unit"), jnote(a, o.unit));
    avp_json_set(a, filters, AVP_S("min_size"), o.has_min_size ? avp_json_int(a, o.min_size) : avp_json_null());
    avp_json_set(a, filters, AVP_S("max_size"), o.has_max_size ? avp_json_int(a, o.max_size) : avp_json_null());
    avp_json_set(a, data, AVP_S("filters"), filters);

    if (want(o.mode, "lost")) {
        const char *note = NULL;
        PickItems items = pick_mode_lost(a, w, &o, &note);
        AvpJson *d = capped(a, &s, &items);
        avp_json_set(a, d, AVP_S("note"), jnote(a, note));
        avp_json_set(a, data, AVP_S("lost"), d);
        section(a, &s, "lost", &items, pick_fmt_lost, note);
    }
    if (want(o.mode, "close")) {
        PickItems work, cands;
        pick_mode_close(a, w, &o, &work, &cands);
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("units"), capped(a, &s, &work));
        avp_json_set(a, d, AVP_S("candidates"), capped(a, &s, &cands));
        avp_json_set(a, data, AVP_S("close"), d);
        section(a, &s, "close", &work, pick_fmt_close, NULL);
        section(a, &s, "close candidates (every function 100%)", &cands, pick_fmt_candidate, NULL);
    }
    if (want(o.mode, "near")) {
        PickItems items = pick_mode_near(a, w, &o);
        AvpJson *d = capped(a, &s, &items);
        avp_json_set(a, d, AVP_S("threshold"), avp_json_float(a, o.threshold));
        avp_json_set(a, data, AVP_S("near"), d);
        section(a, &s, avp_cz(a, avp_pyfmt(a, "near (>= %g%%)", o.threshold)), &items, pick_fmt_near, NULL);
    }
    if (want(o.mode, "fresh")) {
        PickItems items = {0};
        const char *note = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            items = pick_mode_fresh(a, w, &o, s.limit);
            avp_try_end(&t);
        } else {
            if (t.code != SHSM_EXC_FILE_NOT_FOUND) avp_throw(t.code, "%s", t.msg);
            AvpStr e = shsm_exc_str(a, t.code, t.msg);
            if (!s.summary) shsm_raise(SHSM_EXC_USAGE, "%.*s", AVP_SF(e));
            note = avp_cz(a, avp_pyfmt(a, "asm index unavailable: %S", &e));
        }
        AvpSet *libset = avp_set_new();
        for (size_t i = 0; i < items.n; i++) avp_set_add(libset, avp_json_get_str(items.v[i], "library", AVP_S("")));
        AvpStrVec libs = avp_set_sorted(libset);
        avp_set_free(libset);
        AvpJson *refs = avp_json_obj(a);
        for (size_t i = 0; i < libs.n; i++)
            avp_json_set(a, refs, libs.v[i], avp_json_strz(a, pick_reference_flag(avp_cz(a, libs.v[i]), NULL)));
        AvpJson *d = capped(a, &s, &items);
        avp_json_set(a, d, AVP_S("references"), refs);
        avp_json_set(a, d, AVP_S("note"), jnote(a, note));
        avp_json_set(a, data, AVP_S("fresh"), d);
        if (o.lib && *o.lib && libs.n && !s.summary) {
            AvpBuf b = {.a = a};
            for (size_t i = 0; i < libs.n && i < 8; i++) {
                if (i) avp_buf_putz(&b, ", ");
                avp_buf_pyfmt(&b, "%S %s", &libs.v[i], pick_reference_flag(avp_cz(a, libs.v[i]), NULL));
            }
            shsm_output_line(s.out, avp_pyfmt(a, "fresh %s: public reference: %s", o.lib, avp_buf_cz(&b)));
        }
        avp_vec_free(&libs);
        section(a, &s, "fresh", &items, pick_fmt_fresh, note);
    }
    if (want(o.mode, "pending")) {
        PickItems items = pick_mode_pending(a, w, &o);
        avp_json_set(a, data, AVP_S("pending"), capped(a, &s, &items));
        section(a, &s, "pending", &items, pick_fmt_pending, NULL);
    }
    if (want(o.mode, "drafts")) {
        const char *note = NULL;
        PickItems items = pick_mode_drafts(a, w, &o, &note);
        AvpJson *d = capped(a, &s, &items);
        avp_json_set(a, d, AVP_S("note"), jnote(a, note));
        avp_json_set(a, data, AVP_S("drafts"), d);
        section(a, &s, "drafts (grade MATCH from a matched twin)", &items, pick_fmt_drafts, note);
    }
    for (size_t i = 0; i < avp_json_len(data); i++) {
        const AvpJsonMember *m = avp_json_member(data, i);
        shsm_output_data(s.out, avp_cz(a, m->key), m->val);
    }
    shsm_output_close(s.out);
    return SHSM_EXIT_OK;
}

int pick_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("pick.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "mode", .nargs = AVP_NARGS_OPT, .choices = MODES,
                              .help = "one list (default: a short section of each)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--lib",
                              .help = "library of config/<ver>/libraries.tsv (full name, group or component)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--unit", .help = "unit path (prefix or substring)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--min-size", .type = AVP_T_CUSTOM, .conv = size_conv, .type_name = "size_arg",
                              .metavar = "N", .help = "smallest function size in bytes"});
    avp_arg(ap, &(AvpArgDesc){.names = "--max-size", .type = AVP_T_CUSTOM, .conv = size_conv, .type_name = "size_arg",
                              .metavar = "N", .help = "largest function size in bytes"});
    avp_arg(ap, &(AvpArgDesc){.names = "--report", .metavar = "PATH",
                              .help = "objdiff report (default build/<ver>/report.json)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--threshold", .type = AVP_T_FLOAT, .dflt = AVP_DEF_FLOAT(90.0),
                              .help = "near: lowest percent (default 90)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--small", .action = AVP_ARG_STORE_TRUE, .help = "fresh: smallest first"});
    avp_arg(ap, &(AvpArgDesc){.names = "--leaf", .action = AVP_ARG_STORE_TRUE,
                              .help = "fresh: only functions whose callees are all matched"});
    avp_arg(ap, &(AvpArgDesc){.names = "--tiny", .type = AVP_T_INT, .dflt = AVP_DEF_INT(16), .metavar = "N",
                              .help = "fresh: 'tiny' = no calls, under N instructions"});
    avp_arg(ap, &(AvpArgDesc){.names = "--checker", .action = AVP_ARG_STORE_TRUE,
                              .help = "pending: also run the rule checker"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
