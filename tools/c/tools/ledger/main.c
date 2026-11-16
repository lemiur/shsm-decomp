/* ledger.exe: the front end of tools/c/tools/ledger (a port of python tools/ledger.py). */
#include "ledger.h"
#include <stdio.h>

static int run(int argc, char **argv) {
    AvpBuf out, err;
    avp_buf_init(&out, avp_arena());
    avp_buf_init(&err, avp_arena());
    int code = ledger_main(argc, argv, &out, &err);
    AvpStr o = avp_buf_str(&out), e = avp_buf_str(&err);
    if (e.n) fwrite(e.p, 1, e.n, stderr);
    if (o.n) fwrite(o.p, 1, o.n, stdout);
    return code;
}

AVP_MAIN(run, "ledger")
