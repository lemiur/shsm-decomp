/* score_history.exe: tools/score_history.py (per-function current / best / historical scores). */
#include "score_history.h"

#include "avp/avp.h"

#include <stdio.h>

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpBuf out, err;
    avp_buf_init(&out, NULL);
    avp_buf_init(&err, NULL);
    int rc = score_history_run(a, argc - 1, (const char *const *)(argv + 1), &out, &err);
    if (out.n) fwrite(out.p, 1, out.n, stdout);
    fflush(stdout);
    if (err.n) fwrite(err.p, 1, err.n, stderr);
    fflush(stderr);
    avp_buf_free(&out);
    avp_buf_free(&err);
    return rc;
}

AVP_MAIN(run, "score_history")
