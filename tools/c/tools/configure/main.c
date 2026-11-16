/* configure.exe: executes configure.py (DESIGN.md).
 *
 *     configure.exe [--script FILE] [--dump] [--] <configure.py arguments>
 *
 * The own options come first and end at the first argument they do not know (or `--`); the rest goes, unchanged, to
 * the script's argument parser. --dump replaces generate_build / calculate_progress by a JSON dump of the final state.
 * `--emit c` is still accepted (and ignored) for build.ninja files written while a Python emit mode existed. */
#include <stdio.h>
#include <string.h>

#include "avp/avp.h"
#include "cfg.h"

static int usage_error(const char *fmt, const char *arg) {
    fflush(stdout);
    fprintf(stderr, "configure: error: ");
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    return 2;
}

static char *backslashes(AvpArena *a, const char *p) {
    char *s = avp_strdup(a, p);
    for (char *c = s; *c; c++)
        if (*c == '/') *c = '\\';
    return s;
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    CfgOptions o = {0};
    o.script_path = "configure.py";
    int i = 1;
    while (i < argc) {
        const char *arg = argv[i];
        bool takes = strcmp(arg, "--emit") == 0 || strcmp(arg, "--script") == 0;
        if (takes && i + 1 >= argc) return usage_error("%s needs a value", arg);
        if (strcmp(arg, "--emit") == 0) {
            if (strcmp(argv[i + 1], "c") != 0) return usage_error("--emit: expected c, not '%s'", argv[i + 1]);
            i += 2;
        } else if (strcmp(arg, "--script") == 0) {
            o.script_path = argv[i + 1];
            i += 2;
        } else if (strcmp(arg, "--dump") == 0) {
            o.dump = true;
            i++;
        } else if (strcmp(arg, "--") == 0) {
            i++;
            break;
        } else {
            break;
        }
    }
    AvpStr base = avp_path_basename(o.script_path);
    o.prog = "configure.exe"; /* argparse prog: the program the reader runs */
    cfg_set_script_name(avp_cz(a, base));
    o.env.script = o.script_path;
    o.env.argv = (const char *const *)(argv + i);
    o.env.argc = (size_t)(argc - i);
    o.env.ctools = backslashes(a, avp_exe_dir());
    return cfg_run(a, &o);
}

AVP_MAIN(run, "configure")
