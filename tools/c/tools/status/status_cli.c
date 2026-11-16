/* status.py build_parser() and run(). */
#include "status.h"

static const char DESCRIPTION[] = "Per-library progress, per-unit status, changes since a baseline, SVG chart.";

static const char *opt_cz(AvpArena *a, const AvpArgs *args, const char *dest) {
    if (!avp_args_has(args, dest) || avp_args_is_none(args, dest)) return NULL;
    return avp_cz(a, avp_args_str(args, dest));
}

static AvpJson *jstr_or_null(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

static void out_lines(ShsmOutput *out, const AvpStrVec *v) { shsm_output_lines(out, v->v, v->n); }

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    const char *lib = opt_cz(a, args, "lib");
    const char *since = opt_cz(a, args, "since");
    const char *svg = opt_cz(a, args, "svg");
    const char *report = opt_cz(a, args, "report");
    const char *checker_json = opt_cz(a, args, "checker_json");
    bool detail = avp_args_bool(args, "detail"), units = avp_args_bool(args, "units");
    bool no_checker = avp_args_bool(args, "no_checker");

    PickWorld *w = pick_world_new(a, opt_cz(a, args, "root"), report);
    const char *report_path = report && *report ? report : w->report_path;
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    const char *stale = pick_stale_note(a, w->repo, report_path, true);
    if (stale) shsm_output_linez(out, stale);
    if (since && *since) {
        const char *name = NULL;
        ShsmReport *old = status_load_old_report(a, w, since, &name);
        AvpJson *res = status_since(a, w, old, lib);
        AvpStrVec lines = status_fmt_since(a, res, name);
        out_lines(out, &lines);
        shsm_output_data(out, "since", avp_json_strz(a, name));
        shsm_output_data(out, "changes", res);
        shsm_output_close(out);
        return SHSM_EXIT_OK;
    }
    if (units) {
        StatusJsonVec rows = status_unit_rows(a, w, lib, status_pending_by_unit(a, w));
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < rows.n; i++) {
            shsm_output_line(out, status_fmt_unit(a, rows.v[i]));
            avp_json_arr_push(a, arr, rows.v[i]);
        }
        shsm_output_data(out, "units", arr);
        shsm_output_close(out);
        return SHSM_EXIT_OK;
    }
    StatusRows rows;
    StatusRow *total;
    status_library_table(a, w, lib, detail, &rows, &total);
    StatusHeadline *h = NULL;
    if (!(lib && *lib)) h = status_headline(a, w, report_path, checker_json, no_checker);
    if (svg && *svg) {
        const char *text = NULL;
        if (h) {
            const i64 *c = h->rules;
            text = avp_cz(a, avp_pyfmt(a, "%.2f%% matched under the rules (%d / %d bytes, %d / %d functions)",
                                       status_pc(c[2], c[3]), c[2], c[3], c[0], c[1]));
        }
        AvpStr body = status_render_svg(a, &rows, total, text);
        if (!avp_write_text(svg, body, AVP_TM_UTF8_NL_LF)) shsm_raise_io();
    }
    /* the table and headline are short: never cap them */
    out->limit = 0;
    AvpStrVec table = status_fmt_table(a, &rows, total);
    out_lines(out, &table);
    if (h) {
        shsm_output_line(out, AVP_S(""));
        AvpStrVec hl = status_fmt_headline(a, h);
        out_lines(out, &hl);
        /* the table and the ledger take ORIGINAL-ASM from the same sources; a difference is a bug to report */
        if (total->asm_n != h->asm_[0] || total->asm_b != h->asm_[1])
            shsm_output_line(out, avp_pyfmt(a, "note: the table's ORIGINAL-ASM (%d fns, %d bytes) differs from the "
                                               "ledger's (%d fns, %d bytes): the ledger counts rule-checker R18 "
                                               "findings too",
                                            total->asm_n, total->asm_b, h->asm_[0], h->asm_[1]));
    }
    if (svg && *svg) shsm_output_line(out, avp_pyfmt(a, "wrote %s", svg));
    AvpJson *libs = avp_json_arr(a);
    for (size_t i = 0; i < rows.n; i++) avp_json_arr_push(a, libs, status_row_json(a, rows.v[i]));
    shsm_output_data(out, "report", avp_json_strz(a, report_path));
    shsm_output_data(out, "libraries", libs);
    shsm_output_data(out, "total", status_row_json(a, total));
    shsm_output_data(out, "headline", h ? status_headline_json(a, h) : avp_json_null());
    shsm_output_data(out, "svg", jstr_or_null(a, svg));
    shsm_output_close(out);
    return SHSM_EXIT_OK;
}

int status_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("status.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "--lib",
                              .help = "only this library (full name, group or component); splits it into libraries"});
    avp_arg(ap, &(AvpArgDesc){.names = "--detail", .action = AVP_ARG_STORE_TRUE,
                              .help = "one row per library instead of per group"});
    avp_arg(ap, &(AvpArgDesc){.names = "--units", .action = AVP_ARG_STORE_TRUE, .help = "one line per source unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "--since", .metavar = "REF",
                              .help = "changes since a report file or a git rev of the baseline report"});
    avp_arg(ap, &(AvpArgDesc){.names = "--svg", .metavar = "PATH", .help = "write a stacked-bar SVG chart to PATH"});
    avp_arg(ap, &(AvpArgDesc){.names = "--report", .metavar = "PATH",
                              .help = "objdiff report (default build/<ver>/report.json)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--checker-json", .metavar = "PATH",
                              .help = "saved `check_rules.exe --json` output for the headline"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-checker", .action = AVP_ARG_STORE_TRUE,
                              .help = "headline without running the rule checker"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
