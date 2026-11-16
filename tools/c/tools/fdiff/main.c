/* fdiff.exe: a port of python tools/mwcc_trace/fdiff.py. Exit 0 exact, 1 differs, 2 environment error (printed as
 * "error: ..." on stdout) or usage error (argparse). */
#include "fdiff.h"

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgs *args = avp_args_parse(fdiff_parser(), argc, argv);
    AvpBuf out;
    avp_buf_init(&out, a);
    FdiffEnv env = {0};
    int rc = fdiff_run(&env, a, args, &out);
    AvpStr s = avp_buf_str(&out);
    fwrite(s.p, 1, s.n, stdout);
    return rc;
}

AVP_MAIN(run, "fdiff")
