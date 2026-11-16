/* permuter_run.exe: a port of tools/mwcc_trace/permuter_run.py, the launcher of decomp-permuter (tools/permuter,
 * third-party Python) with its preprocessing step replaced by reading the already-preprocessed base.c.
 *
 *     permuter_run <permuter arguments...>
 *
 * Runs tools/venv/Scripts/python.exe -c <bootstrap> <root> <arguments...> (permuter_run.h) in the current directory
 * with this process's standard handles and returns its exit code. The arguments go to the permuter unchanged
 * (`--help` is the permuter's). Exit 2 with one line on stderr when the venv Python is missing or cannot be started
 * (D-permuter_run-2). */
#include "permuter_run.h"

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    const char *root = avp_path_normpath(a, avp_root());
    const char *python = permuter_run_python(a, root);
    if (!avp_isfile(python)) {
        fflush(stdout);
        fprintf(stderr, "permuter_run: error: %s not found (`build/tools/doctor.exe --fix venv` restores it)\n", python);
        return 2;
    }
    size_t n = 0;
    const char **child = permuter_run_argv(a, python, root, (const char *const *)argv + 1, (size_t)(argc - 1), &n);
    const char *err = NULL;
    i64 code = permuter_run_spawn(a, child, n, &err);
    if (code < 0) {
        fflush(stdout);
        fprintf(stderr, "permuter_run: error: cannot start %s: %s\n", python, err);
        return 2;
    }
    return (int)code;
}

AVP_MAIN(run, "permuter_run")
