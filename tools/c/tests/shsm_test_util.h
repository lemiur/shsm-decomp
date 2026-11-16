/* Helpers of the shsm_* unit tests (header-only): fixture folders under build/tmp/cpilot/B (paths with spaces),
 * file writing. */
#pragma once
#include "avp/fs.h"
#include "avp/test.h"
#include "shsm_cli.h"

#include <stdio.h>

/* A fresh fixture folder build/tmp/cpilot/B/<name> (absolute; the name may hold spaces). */
static inline const char *fx_dir(const char *name) {
    AvpArena *a = avp_arena();
    const char *d = avp_path_abspath(a, avp_join(a, "build", "tmp", "cpilot", "B", name));
    if (avp_isdir(d)) avp_rmtree(d, 0);
    AVP_ASSERT(avp_makedirs(d, true));
    return d;
}

/* Writes text to dir/rel (creating folders); returns the path. */
static inline const char *fx_write(const char *dir, const char *rel, const char *text) {
    AvpArena *a = avp_arena();
    const char *p = avp_join(a, dir, rel);
    avp_makedirs(avp_cz(a, avp_path_dirname(p)), true);
    AVP_ASSERT(avp_write_file(p, text, strlen(text)));
    return p;
}

static inline void fx_check_raised(const char *file, int line, const char *what, int code, const char *msg, int exc,
                                   const char *want) {
    if (code != exc) avp_test__fail(file, line, "%s raised %d (%s), expected %d", what, code, msg, exc);
    else if (want && strcmp(msg, want) != 0) avp_test__fail(file, line, "%s message: %s", what, msg);
}

/* Expects stmt to raise exc with the message msg (msg NULL: any message). */
#define CHECK_RAISES(stmt, exc, want)                                            \
    do {                                                                               \
        AvpTry t_;                                                                     \
        if (AVP_TRY(&t_)) {                                                            \
            stmt;                                                                      \
            avp_try_end(&t_);                                                          \
            avp_test__fail(__FILE__, __LINE__, "%s did not raise", #stmt);             \
        } else {                                                                       \
            fx_check_raised(__FILE__, __LINE__, #stmt, t_.code, t_.msg, (exc), (want)); \
        }                                                                              \
    } while (0)
