/* permuter_run: the child's argument vector and command line (arguments passed through unchanged), and the bootstrap
 * program's content. The launch itself is checked by the differential harness (diff/cases.txt). */
#include "avp/test.h"
#include "permuter_run.h"

AVP_TEST(argv_passes_arguments_unchanged) {
    AvpArena *a = avp_arena();
    const char *args[] = {"build/permute/fn", "-j", "4", "--stop-on-zero", "a b", "q\"uote", "", "back\\slash\\"};
    size_t n = 0;
    const char **v = permuter_run_argv(a, "C:\\r\\tools\\venv\\Scripts\\python.exe", "C:\\r", args, 8, &n);
    REQUIRE(n == 12);
    CHECK_CZ(v[0], "C:\\r\\tools\\venv\\Scripts\\python.exe");
    CHECK_CZ(v[1], "-c");
    CHECK(v[2] == PERMUTER_RUN_BOOTSTRAP);
    CHECK_CZ(v[3], "C:\\r");
    for (size_t i = 0; i < 8; i++) CHECK(strcmp(v[4 + i], args[i]) == 0);
    CHECK(v[12] == NULL);
    /* subprocess.list2cmdline of the tail */
    CHECK_CZ(avp_list2cmdline_n(a, v + 4, 8), "build/permute/fn -j 4 --stop-on-zero \"a b\" q\\\"uote \"\" back\\slash\\");
}

AVP_TEST(argv_without_arguments) {
    size_t n = 0;
    const char **v = permuter_run_argv(avp_arena(), "py", "R", NULL, 0, &n);
    CHECK_EQ_U64(n, 4);
    CHECK(v[4] == NULL);
}

AVP_TEST(python_path_and_bootstrap) {
    CHECK_CZ(permuter_run_python(avp_arena(), "X:\\root"), "X:\\root\\tools\\venv\\Scripts\\python.exe");
    CHECK(strstr(PERMUTER_RUN_BOOTSTRAP, "src.main.preprocess = _read_preprocessed\n") != NULL);
    CHECK(strstr(PERMUTER_RUN_BOOTSTRAP, "sys.path.insert(0, PERM)\n") != NULL);
    CHECK(strstr(PERMUTER_RUN_BOOTSTRAP, "open(filename, encoding='utf-8')") != NULL);
    size_t len = strlen(PERMUTER_RUN_BOOTSTRAP);
    CHECK(len > 0 && PERMUTER_RUN_BOOTSTRAP[len - 1] == '\n');
}

AVP_TEST_MAIN("permuter_run")
