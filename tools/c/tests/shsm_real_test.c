/* The shsm_* modules on the real tree: one real compile into a folder with spaces (when the compiler is there). */
#include "shsm_units.h"
#include "shsm_test_util.h"

/* A real compile of PadRumble into a private folder whose path holds spaces (output captured, depfile written). */
AVP_TEST(real_compile_with_spaces) {
    if (!avp_exists("configure.py") || !avp_exists("build.ninja")) {
        printf("skipped: not in the repository\n");
        return;
    }
    AvpArena *a = avp_arena();
    ShsmRepo *r = shsm_repo_find(NULL);
    const ShsmUnit *u = shsm_units_lookup(shsm_load_units(r), "main/game/PadRumble");
    if (!u || !u->has_cflags || !avp_exists(shsm_repo_compiler(a, r, u->mw_version, NULL))) {
        printf("skipped: no compiler or unit\n");
        return;
    }
    const char *work = fx_dir("compile dir with spaces");
    const char *src = avp_join(a, r->root, u->source);
    ShsmRun res = shsm_compile_source(a, r, u, src, work, &(ShsmCompileOpts){.sjis = -1, .depfile = true}, 300);
    CHECK_MSG(shsm_run_ok(&res), "compile failed: %.*s", AVP_SF(shsm_run_output(a, &res)));
    CHECK(avp_isfile(shsm_object_for(a, src, work)));
    CHECK(avp_isfile(shsm_depfile_for(a, src, work)));
}

AVP_TEST_MAIN("shsm_real")
