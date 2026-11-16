/* transform_dep.exe: front end of tools/c/tools/transform_dep (a port of python tools/transform_dep.py).
 *
 * As the Python, it reads $HOME (or $WINEPREFIX) before it looks at its arguments, reads the input with the locale's
 * default encoding and universal newlines, and writes the output as UTF-8 with CRLF line ends. Failures the Python
 * reports as a traceback are reported the same way (traceback on stderr, exit code 1). */
#include "shsm_locale.h"
#include "transform_dep.h"

#define SCRIPT "transform_dep.exe"

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    char *winedevices = transform_dep_winedevices(a);
    if (!winedevices) shsm_py_traceback(SCRIPT, "KeyError", "'HOME'");

    AvpArgParser *p = avp_args_parser(a, &(AvpArgParserDesc){.prog = "transform_dep.exe",
                                                             .description = "Transform a .d file from Wine paths to normal paths"});
    avp_arg(p, &(AvpArgDesc){.names = "d_file", .help = "Dependency file in"});
    avp_arg(p, &(AvpArgDesc){.names = "d_file_out", .help = "Dependency file out"});
    AvpArgs *args = avp_args_parse(p, argc, argv);
    const char *in_file = avp_cz(a, avp_args_str(args, "d_file"));
    const char *out_file = avp_cz(a, avp_args_str(args, "d_file_out"));

    AvpStr text;
    if (!shsm_locale_read_text(a, in_file, &text)) shsm_py_traceback_io(SCRIPT);
    AvpTry t;
    AvpStr output = {0};
    if (AVP_TRY(&t)) {
        output = transform_dep_text(a, text, winedevices);
        avp_try_end(&t);
    } else {
        shsm_py_traceback(SCRIPT, "IndexError", "string index out of range");
    }
    if (!shsm_write_utf8_text(out_file, output)) shsm_py_traceback_io(SCRIPT);
    return 0;
}

AVP_MAIN(run, "transform_dep")
