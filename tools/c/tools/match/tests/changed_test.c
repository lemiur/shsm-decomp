/* match.exe --changed with no unit: the units whose source or recorded header is newer than their last run or build.
 * Runs the tool on a fixture checkout whose tools/bin/ninja.exe is the test child process answering `-t deps`. */
#include "avp/proc.h"
#include "avp/test.h"
#include "match.h"

#include <string.h>

static const char CONFIGURE[] =
    "DEFAULT_VERSION = 0\nVERSIONS = [\n    \"TEST01\",\n]\nlibs = [\n    Object(NonMatching, \"game/A.cpp\"),\n]\n";
static const char NINJA[] =
    "rule mwcc\n  command = x\n# game/A.cpp: game (linked False)\n"
    "build build\\TEST01\\src\\game\\A.o: mwcc src\\game\\A.cpp\n  mw_version = Wii/1.0\n  cflags = -O3\n";
static const char OBJDIFF[] =
    "{\"units\": [{\"name\": \"main/game/A\", \"target_path\": \"build/TEST01/obj/game/A.o\", \"base_path\": "
    "\"build/TEST01/src/game/A.o\", \"metadata\": {\"source_path\": \"src/game/A.cpp\"}}]}\n";
static const char DEPS[] = "build/TEST01/src/game/A.o: #deps 2, deps mtime 1 (VALID)\n    src/game/A.cpp\n    include/game/A.h\n\n";

static AvpArena *A(void) { return avp_arena(); }

static void put(const char *root, const char *rel, const char *text) {
    const char *p = avp_path_join(A(), root, rel);
    AVP_ASSERT(avp_makedirs(avp_cz(A(), avp_path_dirname(p)), true));
    AVP_ASSERT(avp_write_file(p, text, strlen(text)));
}

static void set_mtime(const char *root, const char *rel, double secs) {
    const char *p = avp_path_join(A(), root, rel);
    i64 ns = (i64)(secs * 1e9);
    AVP_ASSERT(avp_utime_ns(p, ns, ns));
}

static const char *fixture(double *now) {
    const char *root = avp_path_abspath(A(), avp_join(A(), "build", "tmp", "cpilot", "B", "match changed"));
    if (avp_isdir(root)) AVP_ASSERT(avp_rmtree(root, AVP_RMTREE_CLEAR_READONLY));
    put(root, "configure.py", CONFIGURE);
    put(root, "build.ninja", NINJA);
    put(root, "objdiff.json", OBJDIFF);
    put(root, "config/TEST01/config.yml", "object: orig/TEST01/sys/main.dol\nobject_base: \".\"\n");
    put(root, "config/TEST01/splits.txt", "game/A.cpp:\n\t.text start:0x10 end:0x20\n");
    put(root, "tools/README", "x\n");
    put(root, "build/tools/sjiswrap.exe", "");
    put(root, "src/game/A.cpp", "int a;\n");
    put(root, "include/game/A.h", "// a\n");
    put(root, "build/TEST01/src/game/A.o", "obj");
    put(root, "ninja_deps.txt", DEPS);
    const char *child = avp_path_normpath(A(), avp_join(A(), avp_exe_dir(), "shsm_child.exe"));
    AVP_ASSERT(avp_makedirs(avp_join(A(), root, "tools/bin"), true));
    AVP_ASSERT(avp_copyfile(child, avp_join(A(), root, "tools/bin/ninja.exe")));
    AVP_ASSERT(avp_getmtime(avp_path_join(A(), root, "src/game/A.cpp"), now));
    set_mtime(root, "build/TEST01/src/game/A.o", *now - 1000);
    set_mtime(root, "src/game/A.cpp", *now - 2000);
    set_mtime(root, "include/game/A.h", *now - 2000);
    return root;
}

typedef struct {
    i64 rc;
    AvpStr out;
} Run;

static Run match_changed(const char *root) {
    const char *exe = avp_path_normpath(A(), avp_join(A(), avp_exe_dir(), "..", "match.exe"));
    const char *argv[] = {exe, "--root", root, "--changed", NULL};
    AvpProcSpec spec = {0};
    spec.argv = argv;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    AvpProcResult r = avp_run(&spec);
    AVP_ASSERT(r.status == AVP_PROC_OK);
    return (Run){r.returncode, r.out};
}

static bool has(AvpStr t, const char *needle) { return avp_str_contains(t, avp_str(needle)); }

AVP_TEST(changed_without_a_unit_takes_the_units_a_header_edit_touched) {
    double now;
    const char *root = fixture(&now);
    Run r = match_changed(root);
    CHECK_MSG(r.rc == 0 && has(r.out, "--changed: 0 units"), "rc %lld\n%.*s", (long long)r.rc, AVP_SF(r.out));
    /* an edited source file */
    set_mtime(root, "src/game/A.cpp", now - 500);
    r = match_changed(root);
    CHECK_MSG(has(r.out, "--changed: 1 units") && has(r.out, "main/game/A"), "rc %lld\n%.*s", (long long)r.rc, AVP_SF(r.out));
    /* it was compared by that run: no longer changed. A header the build recorded is the other kind of edit. */
    r = match_changed(root);
    CHECK_MSG(has(r.out, "--changed: 0 units"), "%.*s", AVP_SF(r.out));
    set_mtime(root, "include/game/A.h", now + 500);
    r = match_changed(root);
    CHECK_MSG(has(r.out, "--changed: 1 units") && has(r.out, "main/game/A"), "%.*s", AVP_SF(r.out));
}

AVP_TEST_MAIN("match_changed")
