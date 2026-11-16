/* neutral: the parser and main (tools/neutral.py main). */
#include "neutral.h"

static int run(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    NeutralOpts o = {0};
    if (avp_args_has(args, "root") && !avp_args_is_none(args, "root")) o.root = avp_cz(a, avp_args_str(args, "root"));
    if (!avp_args_is_none(args, "since")) o.since = avp_cz(a, avp_args_str(args, "since"));
    o.paths = avp_args_strs(args, "paths");
    o.jobs = avp_args_is_none(args, "jobs") ? 0 : (int)avp_args_int(args, "jobs");
    o.no_cache = avp_args_bool(args, "no_cache");
    return neutral_run(&o, args);
}

int neutral_main(int argc, char **argv) {
    AvpArgParser *ap =
        shsm_cli_parser("neutral.exe", "Is a header or source edit byte-neutral? (see the module docstring)", true, true,
                        true);
    avp_arg(ap, &(AvpArgDesc){.names = "paths", .nargs = AVP_NARGS_STAR,
                              .help = "changed files (default: git diff against --since)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--since", .metavar = "REV", .dflt = AVP_DEF_NONE,
                              .help = "old side (default HEAD)"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE,
                              .help = "parallel compiles (default: CPU count)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-cache", .action = AVP_ARG_STORE_TRUE,
                              .help = "always compile the old side"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
