/* doctor: the command line (doctor.main). */
#include "doctor.h"
#include "shsm_cli.h"
#include "shsm_repo.h"

#include <stdio.h>
#include <string.h>

#define A avp_arena()

static AvpArgParser *parser(void) {
    AvpArgParser *ap = avp_args_parser(
        NULL, &(AvpArgParserDesc){.prog = "doctor.exe", .description = "Environment self-check (see module docstring)."});
    avp_arg(ap, &(AvpArgDesc){.names = "--root"});
    avp_arg(ap, &(AvpArgDesc){.names = "--strict", .action = AVP_ARG_STORE_TRUE, .help = "optional checks fail too"});
    avp_arg(ap, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--fix", .nargs = AVP_NARGS_STAR, .metavar = "COMPONENT",
                              .help = "venv, permuter, binutils (default: all)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--install-hooks", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--uninstall-hooks", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE});
    return ap;
}

static bool is_component(AvpStr s) {
    for (size_t i = 0; doctor_fix_components[i]; i++)
        if (avp_str_eqz(s, doctor_fix_components[i])) return true;
    return false;
}

static int checks_output(Doctor *d, const DoctorChecks *checks, bool strict, bool as_json) {
    (void)d;
    size_t failed = 0;
    for (size_t i = 0; i < checks->n; i++) {
        const DoctorCheck *x = &checks->v[i];
        if (strcmp(x->status, "OK") != 0 && (x->required || strict) && !x->advisory) failed++;
    }
    if (as_json) {
        AvpJson *doc = avp_json_obj(A);
        avp_json_set(A, doc, AVP_S("ok"), avp_json_bool(failed == 0));
        AvpJson *arr = avp_json_arr(A);
        for (size_t i = 0; i < checks->n; i++) avp_json_arr_push(A, arr, doctor_check_json(A, &checks->v[i]));
        avp_json_set(A, doc, AVP_S("checks"), arr);
        AvpJsonDumpOpts o = shsm_json_opts();
        AvpStr s = avp_json_dumps(A, doc, &o);
        fwrite(s.p, 1, s.n, stdout);
        fputc('\n', stdout);
    } else {
        for (size_t i = 0; i < checks->n; i++) {
            fputs(doctor_check_line(A, &checks->v[i]), stdout);
            fputc('\n', stdout);
        }
        avp_pyprintf("doctor: %s (%d checks, %d failing)\n", failed ? "FAIL" : "OK", checks->n, failed);
    }
    return failed ? 1 : 0;
}

int doctor_main(int argc, char **argv) {
    AvpArgs *args = avp_args_parse(parser(), argc, argv);
    AvpStr root_arg = avp_args_str(args, "root");
    bool debug = avp_args_bool(args, "debug");
    AvpTry t;
    if (AVP_TRY(&t)) {
        /* --root, else the checkout this is run from (doctor.py: the checkout holding the script; D-doctor-1) */
        const char *root = root_arg.p ? avp_path_abspath(A, avp_cz(A, root_arg)) : shsm_find_root(NULL);
        int rc;
        if (avp_args_bool(args, "install_hooks")) {
            rc = doctor_install_hooks(root, NULL);
        } else if (avp_args_bool(args, "uninstall_hooks")) {
            rc = doctor_uninstall_hooks(root, NULL);
        } else if (!avp_args_is_none(args, "fix")) {
            AvpStrVec comps = avp_args_strs(args, "fix");
            const char **list;
            size_t n;
            if (comps.n) {
                list = AVP_NEWN(A, const char *, comps.n);
                for (size_t i = 0; i < comps.n; i++) list[i] = avp_cz(A, comps.v[i]);
                n = comps.n;
            } else {
                list = (const char **)doctor_fix_components;
                n = 3;
            }
            AvpBuf bad;
            avp_buf_init(&bad, A);
            for (size_t i = 0; i < n; i++) {
                if (is_component(avp_str(list[i]))) continue;
                if (bad.n) avp_buf_putz(&bad, ", ");
                avp_buf_putz(&bad, list[i]);
            }
            if (bad.n) {
                avp_try_end(&t);
                fflush(stdout);
                fprintf(stderr, "error: unknown component %s (choose from venv, permuter, binutils)\n", avp_buf_cz(&bad));
                return 2;
            }
            Doctor d = doctor_new(root);
            DoctorFixer f = {.d = &d, .runner = doctor_fix_run};
            rc = doctor_fix(&f, list, n) ? 0 : 1;
        } else {
            Doctor d = doctor_new(root);
            DoctorChecks checks = {0};
            doctor_check_all(&d, &checks);
            avp_try_end(&t);
            return checks_output(&d, &checks, avp_args_bool(args, "strict"), avp_args_bool(args, "json"));
        }
        avp_try_end(&t);
        return rc;
    }
    fflush(stdout);
    AvpStr msg = shsm_exc_str(A, t.code, t.msg);
    if (debug) { /* the Python re-raises: a traceback and exit 1 (D-doctor-5) */
        fprintf(stderr, "%s: %.*s\n", shsm_exc_class(t.code), AVP_SF(msg));
        return 1;
    }
    fprintf(stderr, "error: %.*s\n", AVP_SF(msg));
    return 2;
}
