/* check_docs front end: options, the order of the checks, the report and its summary (run, emit, build_parser). */
#include "check_docs.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include <stdio.h>

#define DESCRIPTION "Check that AGENTS.md and the docs describe the tools and files that exist."
#define HELP_JOBS 4 /* help commands at once */

static AvpArena *A(void) { return avp_arena(); }

static bool has(const AvpStrVec *v, const char *s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], s)) return true;
    return false;
}

static int emit(AvpArgs *args, const CdReport *rep, const AvpStrVec *only) {
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    for (size_t i = 0; i < rep->items.n; i++) shsm_output_linez(out, cd_item_text(A(), &rep->items.v[i]));
    AvpStrVec names = {0};
    for (size_t i = 0; i < only->n; i++)
        if (!has(&names, avp_cz(A(), only->v[i]))) avp_vec_push(&names, only->v[i]);
    AvpJson *summary = avp_json_obj(A());
    AvpBuf lines;
    avp_buf_init(&lines, A());
    i64 total = 0;
    for (size_t c = 0; c < names.n; c++) {
        AvpStr up = avp_str_upper(A(), names.v[c]);
        i64 f = 0, n = 0;
        for (size_t i = 0; i < rep->items.n; i++)
            if (avp_str_eqz(up, rep->items.v[i].check)) {
                if (rep->items.v[i].note)
                    n++;
                else
                    f++;
            }
        AvpJson *s = avp_json_obj(A());
        avp_json_set(A(), s, AVP_S("findings"), avp_json_int(A(), f));
        avp_json_set(A(), s, AVP_S("notes"), avp_json_int(A(), n));
        avp_json_set(A(), summary, names.v[c], s);
        const char *status = f ? avp_sprintf(A(), "%lld finding%s", (long long)f, f == 1 ? "" : "s") : "ok";
        avp_buf_printf(&lines, "%.*s: %s, %lld note%s\n", AVP_SF(names.v[c]), status, (long long)n, n == 1 ? "" : "s");
        total += f;
    }
    AvpJson *findings = avp_json_arr(A());
    for (size_t i = 0; i < rep->items.n; i++) {
        const CdItem *it = &rep->items.v[i];
        AvpJson *o = avp_json_obj(A());
        avp_json_set(A(), o, AVP_S("check"), avp_json_str(A(), avp_str(it->check)));
        avp_json_set(A(), o, AVP_S("file"), avp_json_str(A(), avp_str(it->file)));
        avp_json_set(A(), o, AVP_S("line"), avp_json_int(A(), it->line));
        avp_json_set(A(), o, AVP_S("message"), avp_json_str(A(), avp_str(it->message)));
        avp_json_set(A(), o, AVP_S("note"), avp_json_bool(it->note));
        avp_json_arr_push(A(), findings, o);
    }
    shsm_output_data(out, "findings", findings);
    shsm_output_data(out, "summary", summary);
    shsm_output_close(out);
    if (!avp_args_bool(args, "json") && lines.n) fwrite(lines.p, 1, lines.n, stdout);
    return total ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpStr root_arg = avp_args_str(args, "root");
    const char *root = root_arg.p && root_arg.n ? avp_path_abspath(A(), avp_cz(A(), root_arg)) : shsm_find_root(NULL);
    AvpStrVec only = {0};
    for (size_t i = 0; CD_CHECKS[i]; i++) avp_vec_push(&only, avp_str(CD_CHECKS[i]));
    AvpStr only_arg = avp_args_str(args, "only");
    if (only_arg.p && only_arg.n) {
        only.n = 0;
        AvpStrVec parts = avp_str_split(only_arg, AVP_S(","), -1);
        for (size_t i = 0; i < parts.n; i++) {
            AvpStr c = avp_str_strip(parts.v[i]);
            if (c.n) avp_vec_push(&only, c);
        }
        AvpStrVec bad = {0};
        for (size_t i = 0; i < only.n; i++) {
            bool known = false;
            for (size_t k = 0; CD_CHECKS[k]; k++) known = known || avp_str_eqz(only.v[i], CD_CHECKS[k]);
            if (!known) avp_vec_push(&bad, only.v[i]);
        }
        if (bad.n) {
            AvpStr b = avp_str_join(A(), AVP_S(", "), bad.v, bad.n);
            shsm_raise(SHSM_EXC_USAGE, "--only: unknown check %.*s (choose from verbs,paths,pins,exceptions,text)", AVP_SF(b));
        }
    }
    if (!avp_isfile(avp_path_join(A(), root, "AGENTS.md"))) shsm_raise(SHSM_EXC_USAGE, "no AGENTS.md in %s", root);
    CdReport rep;
    cd_report_init(&rep, A());
    if (has(&only, "verbs")) {
        CdHelpEnv env = {cd_run_help, NULL, NULL, HELP_JOBS};
        cd_check_verbs(root, &rep, &env);
    }
    if (has(&only, "paths")) cd_check_paths(root, &rep);
    if (has(&only, "pins")) {
        CdPins pins = cd_doctor_pins();
        cd_check_pins(root, &rep, &pins);
    }
    if (has(&only, "exceptions")) cd_check_exceptions(root, &rep, NULL, NULL, NULL);
    if (has(&only, "text")) cd_check_text(root, &rep, NULL);
    return emit(args, &rep, &only);
}

int check_docs_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("check_docs.exe", DESCRIPTION, true, true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "--only", .metavar = "CHECKS",
                              .help = "comma-separated subset of verbs,paths,pins,exceptions,text"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
