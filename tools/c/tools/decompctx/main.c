/* decompctx.exe: front end of tools/c/tools/decompctx (a port of python tools/decompctx.py). */
#include <stdio.h>

#include "decompctx.h"
#include "shsm_locale.h"

static void collect(AvpArena *a, const AvpArgs *args, const char *dest, AvpStrVec *out) {
    AvpStrVec v = avp_args_strs(args, dest);
    for (size_t i = 0; i < v.n; i++) avp_vec_push(out, avp_str_dup(a, v.v[i]));
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgParser *p = avp_args_parser(
        a, &(AvpArgParserDesc){.prog = "decompctx.exe",
                               .description = "Create a context file which can be used for decomp.me"});
    avp_arg(p, &(AvpArgDesc){.names = "c_file", .help = "File from which to create context"});
    avp_arg(p, &(AvpArgDesc){.names = "-o --output", .help = "Output file", .dflt = AVP_DEF_STR("ctx.c")});
    avp_arg(p, &(AvpArgDesc){.names = "-d --depfile", .help = "Dependency file"});
    avp_arg(p, &(AvpArgDesc){.names = "-I --include", .action = AVP_ARG_APPEND, .help = "Include directory"});
    avp_arg(p, &(AvpArgDesc){.names = "-x --exclude", .action = AVP_ARG_APPEND, .help = "Excluded file name glob"});
    avp_arg(p, &(AvpArgDesc){.names = "-D --define", .action = AVP_ARG_APPEND, .help = "Macro definition"});
    AvpArgs *args = avp_args_parse(p, argc, argv);

    if (!avp_args_given(args, "include")) avp_fatal("No include directories specified");

    DecompctxState st;
    decompctx_init(&st, a, avp_path_realpath(a, avp_root()));
    collect(a, args, "include", &st.include_dirs);
    collect(a, args, "exclude", &st.exclude_globs);
    AvpStrVec defs = {0};
    collect(a, args, "define", &defs);

    AvpStr prelude = decompctx_prelude(a, &defs);
    AvpBuf total;
    avp_buf_init(&total, a);
    avp_buf_put(&total, prelude);
    if (!decompctx_import_c_file(&st, avp_cz(a, avp_args_str(args, "c_file")), &total))
        shsm_py_traceback(st.script, st.err_class, "%s", st.err_text);

    AvpStr output = avp_args_str(args, "output");
    char *out_path = avp_path_join(a, st.root_dir, avp_cz(a, output));
    if (!shsm_write_utf8_text(out_path, avp_buf_str(&total))) shsm_py_traceback_io(st.script);

    if (avp_args_given(args, "depfile")) {
        AvpBuf d;
        avp_buf_init(&d, a);
        avp_buf_put(&d, decompctx_sanitize_path(a, output));
        avp_buf_putc(&d, ':');
        for (size_t i = 0; i < st.deps.n; i++) {
            avp_buf_putz(&d, " \\\n\t");
            avp_buf_put(&d, decompctx_sanitize_path(a, st.deps.v[i]));
        }
        char *dep_path = avp_path_join(a, st.root_dir, avp_cz(a, avp_args_str(args, "depfile")));
        if (!shsm_write_utf8_text(dep_path, avp_buf_str(&d))) shsm_py_traceback_io(st.script);
    }
    return 0;
}

AVP_MAIN(run, "decompctx")
