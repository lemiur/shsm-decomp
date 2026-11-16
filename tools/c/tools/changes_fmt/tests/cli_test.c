/* changes_fmt.exe as a program: the report file is the one named on the command line, whatever the working directory is
 * (a slot's build.ninja runs the main checkout's changes_fmt.exe from the slot). */
#include "avp/avp.h"
#include "avp/test.h"
#include "shsm_cli.h"

static const char DOC[] =
    "{\"from\":{},\"to\":{},\"units\":[{\"name\":\"main/a\",\"from\":{\"fuzzy_match_percent\":90.0},"
    "\"to\":{\"fuzzy_match_percent\":80.0},\"functions\":[]}]}";

AVP_TEST(report_is_read_from_any_working_directory) {
    AvpArena *a = avp_arena();
    char *dir = avp_mkdtemp(a, "cf", "", NULL);
    REQUIRE(dir);
    const char *file = avp_path_join(a, dir, "changes.json");
    REQUIRE(avp_write_file(file, DOC, sizeof DOC - 1));
    const char *argv[] = {avp_join(a, avp_exe_dir(), "..", "changes_fmt.exe"), file, NULL};
    /* the working directory is neither the tool's root nor the report's folder's parent */
    ShsmRun r = shsm_run(a, argv, 2, avp_cz(a, avp_path_dirname(dir)), 60);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK_MSG(avp_str_contains(r.out, AVP_S("main/a")), "stdout: %.*s stderr: %.*s", AVP_SF(r.out), AVP_SF(r.err));
    /* a relative name is relative to the working directory */
    const char *argv2[] = {argv[0], "changes.json", NULL};
    r = shsm_run(a, argv2, 2, dir, 60);
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(avp_str_contains(r.out, AVP_S("main/a")));
    avp_tempdir_cleanup(dir);
}

AVP_TEST_MAIN("changes_fmt_cli")
