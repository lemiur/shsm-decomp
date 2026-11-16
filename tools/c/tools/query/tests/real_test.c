/* QueryRealTreeTest of tools/test_query.py: query.exe --root <the checkout CTest runs in>, reading only. Skipped as
 * the Python skips (skip_unless_real_tree): when SHSM_NO_REAL_TREE is set or there is no build/R5WEA4/asm; also
 * without build/index/asm.sqlite, so that the test never builds the index of a real tree. */
#include "query_fixture.h"

#include <stdlib.h>

static const char *real_root(void) {
    const char *env = getenv("SHSM_NO_REAL_TREE");
    if (env && *env) {
        printf("skipped: SHSM_NO_REAL_TREE is set\n");
        return NULL;
    }
    if (!avp_exists("configure.py") || !avp_isdir("build/R5WEA4/asm") || !avp_exists("build/index/asm.sqlite")) {
        printf("skipped: no build/R5WEA4/asm or build/index/asm.sqlite (run configure.py and ninja)\n");
        return NULL;
    }
    return avp_path_abspath(avp_arena(), ".");
}

static AvpJson *real_q(const char *file, int line, const char *root, const char *const *args) {
    const char *argv[16];
    size_t n = 0;
    for (; *args; args++) argv[n++] = *args;
    argv[n++] = "--json";
    argv[n] = NULL;
    QfRun r = qf_runv(root, argv);
    if (r.rc != 0) avp_test__fail(file, line, "exit %d: %s", r.rc, r.err);
    AvpJson *d = avp_json_parse(avp_arena(), avp_str(r.out), NULL);
    if (!d) {
        avp_test__fail(file, line, "not JSON: %.200s", r.out);
        return avp_json_obj(avp_arena());
    }
    return d;
}
#define REAL_Q(root, ...) real_q(__FILE__, __LINE__, root, (const char *const[]){__VA_ARGS__, NULL})

AVP_TEST(real_fn_and_callers) {
    const char *root = real_root();
    if (!root) return;
    AvpJson *d = jg(REAL_Q(root, "fn", "fn_800067CC", "--summary"), "function");
    CHECK_CZ(js(jg(d, "name")), "fn_800067CC__8unk_RampFf");
    CHECK_CZ(js(jg(d, "unit")), "main/game/PadRumble");
    d = REAL_Q(root, "callers", "memset");
    CHECK(avp_json_len(jg(d, "callers")) > 100);
}

AVP_TEST(real_strings_and_unit) {
    const char *root = real_root();
    if (!root) return;
    AvpJson *d = REAL_Q(root, "find-string", "MsgStartBigRumble");
    CHECK(avp_json_len(jg(d, "strings")) > 0);
    d = REAL_Q(root, "unit", "game/PadRumble");
    CHECK_CZ(js(jg(d, "status")), "Matching");
}

AVP_TEST_MAIN("query")
