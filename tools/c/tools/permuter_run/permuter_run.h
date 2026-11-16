/* permuter_run: the launcher of decomp-permuter (third-party Python, tools/permuter) that replaces its `cpp -P`
 * preprocessing step by reading the already-preprocessed base.c, as tools/mwcc_trace/permuter_run.py does.
 * decomp-permuter stays Python: the C tool starts tools/venv/Scripts/python.exe with an inline bootstrap
 * (PERMUTER_RUN_BOOTSTRAP) equivalent to the script, the checkout root and the arguments unchanged. */
#pragma once
#include "avp/avp.h"

/* The `python -c` program: argv[1] is the checkout root (removed before the permuter parses its arguments);
 * sys.argv[0] becomes "permuter_run.py" (argparse's prog, as under the script), sys.path[0] the script's directory
 * <root>/tools/mwcc_trace, then <root>/tools/permuter is put first; src.main.preprocess is replaced by a function
 * reading the file as UTF-8, and src.main.main() runs. */
extern const char PERMUTER_RUN_BOOTSTRAP[];

/* <root>/tools/venv/Scripts/python.exe (backslashes, as os.path.join builds it). */
const char *permuter_run_python(AvpArena *a, const char *root);

/* The child's argument vector: python, "-c", the bootstrap, root, then args[0..n) unchanged. NULL-terminated; *argc_out
 * receives the count. */
const char **permuter_run_argv(AvpArena *a, const char *python, const char *root, const char *const *args, size_t n,
                               size_t *argc_out);

/* Starts the child with this process's standard handles and current directory, in a kill-on-close job (it ends with
 * this process), ignores Ctrl+C/Ctrl+Break here (the child, in the same console, gets them and decides), waits and
 * returns the child's exit code. -1 when the child cannot be started (*err: the Windows error text). */
i64 permuter_run_spawn(AvpArena *a, const char *const *argv, size_t argc, const char **err);
