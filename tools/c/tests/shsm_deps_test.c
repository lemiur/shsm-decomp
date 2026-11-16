/* shsm_deps: the inputs ninja recorded for an object and which of them is newer than it. tools/bin/ninja.exe of the
 * fixture root is a copy of the test child process, which answers `-t deps` with the root's ninja_deps.txt. */
#include "avp/proc.h"
#include "shsm_deps.h"
#include "shsm_test_util.h"

static const char DEPS_TEXT[] =
    "build/R5WEA4/config.json: #deps 1, deps mtime 5 (VALID)\n"
    "    config/R5WEA4/splits.txt\n"
    "\n"
    "build/R5WEA4/src/game/A.o: #deps 3, deps mtime 7 (VALID)\n"
    "    src/game/A.cpp\n"
    "    include/game/A.h\n"
    "    include/other.h\n"
    "\n";

static void set_mtime(const char *root, const char *rel, double secs) {
    const char *p = avp_path_join(avp_arena(), root, rel);
    i64 ns = (i64)(secs * 1e9);
    AVP_ASSERT(avp_utime_ns(p, ns, ns));
}

static const char *fixture(const char *name, bool with_ninja) {
    AvpArena *a = avp_arena();
    const char *root = fx_dir(name);
    fx_write(root, "src/game/A.cpp", "int a;\n");
    fx_write(root, "include/game/A.h", "// a\n");
    fx_write(root, "include/other.h", "// other\n");
    fx_write(root, "build/R5WEA4/src/game/A.o", "obj");
    fx_write(root, "ninja_deps.txt", DEPS_TEXT);
    if (with_ninja) {
        const char *child = avp_path_normpath(a, avp_join(a, avp_exe_dir(), "shsm_child.exe"));
        AVP_ASSERT(avp_makedirs(avp_join(a, root, "tools/bin"), true));
        AVP_ASSERT(avp_copyfile(child, avp_join(a, root, "tools/bin/ninja.exe")));
    }
    set_mtime(root, "build/R5WEA4/src/game/A.o", 2000000000.0);
    set_mtime(root, "src/game/A.cpp", 1999999000.0);
    set_mtime(root, "include/game/A.h", 1999999000.0);
    set_mtime(root, "include/other.h", 1999999000.0);
    return root;
}

AVP_TEST(newer_header_is_found_through_ninja_deps) {
    AvpArena *a = avp_arena();
    const char *root = fixture("deps with ninja", true);
    ShsmDeps *d = shsm_deps_load(a, root);
    const char *obj = "build/R5WEA4/src/game/A.o";
    CHECK(shsm_deps_newer_input(a, d, root, obj, "src/game/A.cpp", NULL, 0, 2000000000.0) == NULL);
    set_mtime(root, "include/other.h", 2000000100.0);
    const char *got = shsm_deps_newer_input(a, d, root, obj, "src/game/A.cpp", NULL, 0, 2000000000.0);
    REQUIRE(got != NULL);
    CHECK_CZ(got, "include/other.h");
    /* the source file is named first, a header of another object is not looked at */
    set_mtime(root, "src/game/A.cpp", 2000000200.0);
    CHECK_CZ(shsm_deps_newer_input(a, d, root, obj, "src/game/A.cpp", NULL, 0, 2000000000.0), "src/game/A.cpp");
    CHECK(shsm_deps_newer_input(a, d, root, "build/R5WEA4/src/game/B.o", NULL, NULL, 0, 2000000000.0) == NULL);
    /* an extra input (symbols.txt) counts */
    fx_write(root, "config/symbols.txt", "x");
    set_mtime(root, "config/symbols.txt", 2000000300.0);
    const char *extra[] = {NULL, "config/symbols.txt"};
    CHECK_CZ(shsm_deps_newer_input(a, d, root, "build/R5WEA4/src/game/B.o", NULL, extra, 2, 2000000000.0),
             "config/symbols.txt");
}

AVP_TEST(without_ninja_only_the_source_and_extras_are_known) {
    AvpArena *a = avp_arena();
    const char *root = fixture("deps without ninja", false);
    ShsmDeps *d = shsm_deps_load(a, root);
    set_mtime(root, "include/other.h", 2000000100.0);
    CHECK(shsm_deps_newer_input(a, d, root, "build/R5WEA4/src/game/A.o", "src/game/A.cpp", NULL, 0, 2000000000.0) == NULL);
    set_mtime(root, "src/game/A.cpp", 2000000100.0);
    CHECK_CZ(shsm_deps_newer_input(a, d, root, "build/R5WEA4/src/game/A.o", "src/game/A.cpp", NULL, 0, 2000000000.0),
             "src/game/A.cpp");
}

AVP_TEST_MAIN("shsm_deps")
