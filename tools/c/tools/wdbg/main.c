/* wdbg.exe: a port of python tools/mwcc_trace/wdbg.py, the breakpoint debugger for the 32-bit mwcceppc.exe.
 *
 *     wdbg <src> <unit> <outdir> <spec> [extra flag words...]
 *
 * Compiles src with the unit's compiler version and cflags (cc.py's unit_flags, plus each extra word shlex-split)
 * into <outdir>/o.o under the Windows debug API, with the breakpoints of the spec (SPEC.md: the C spec language that
 * replaces the Python's spec files, D-wdbg-1). Nothing on stdout; the result is <outdir>/log.txt (and the spec's
 * snapshots <outdir>/<name>.bin). Exit 0; 1 where the Python dies with an uncaught exception or SystemExit (one line
 * on stderr: D-wdbg-2). */
#include "cctool.h"
#include "shsm_text.h"
#include "wdbg_debug.h"
#include "wdbg_iro.h"
#include "wdbg_run.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static int fail(const char *cls, AvpStr msg) {
    fflush(stdout);
    fprintf(stderr, "%s: %.*s\n", cls, AVP_SF(msg));
    return 1;
}

static int fail_io(AvpArena *a) { return fail(avp_io_err_class(), avp_io_err_str(a)); }

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    if (argc >= 2 && !strcmp(argv[1], "iro-scan")) return wdbg_iro_scan(a, avp_path_abspath(a, argv[0]), argc - 2, argv + 2);
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        puts("usage: wdbg.exe <src> <unit> <outdir> <spec> [extra flag words...]\n"
             "       wdbg.exe iro-scan [UNIT ...] [--status S[,S]] [--all-functions] [-j N] [--outdir DIR] [--spec FILE]\n"
             "\nThe first form compiles src with the unit's flags under the debugger and the breakpoints of a spec\n"
             "(SPEC.md); the result is <outdir>/log.txt. The second runs the iro_gate spec over units and prints, per\n"
             "non-exact function, whether the IR optimiser is on or off (see wdbg.exe iro-scan --help).");
        return 0;
    }
    /* src,unit,outdir,spec=sys.argv[1:5] */
    if (argc < 5)
        return fail("ValueError", avp_str(avp_sprintf(a, "not enough values to unpack (expected 4, got %d)", argc - 1)));
    const char *src = argv[1], *unit = argv[2], *outdir = argv[3], *spec_path = argv[4];
    if (!avp_makedirs(outdir, true)) return fail_io(a);

    /* cc.unit_flags(unit): cc.ROOT/build.ninja */
    const char *root = avp_path_normpath(a, avp_root());
    AvpStr ninja;
    if (!avp_read_text(a, avp_path_join(a, root, "build.ninja"), AVP_TM_UTF8, &ninja)) return fail_io(a);
    AvpStr ver;
    AvpStrVec cflags = {0};
    const char *err = NULL;
    int rc = cctool_unit_flags(a, ninja, unit, &ver, &cflags, &err);
    if (rc == 1) {
        fflush(stdout);
        fprintf(stderr, "unit not found in build.ninja: %s\n", unit);
        return 1;
    }
    if (rc == 2) return fail("AttributeError", AVP_S("'NoneType' object has no attribute 'group'"));
    if (rc == 3) return fail("ValueError", avp_str(err));
    for (int i = 5; i < argc; i++) {
        AvpStrVec words = {0};
        if (!shsm_shlex_split(a, avp_str(argv[i]), &words, &err)) return fail("ValueError", avp_str(err));
        for (size_t k = 0; k < words.n; k++) avp_vec_push(&cflags, words.v[k]);
        avp_vec_free(&words);
    }

    /* ROOT (forward slashes) + '/tools/compilers/' + ver + '/mwcceppc.exe'; args: cflags, -c, abspath(src), -o,
     * outdir + '/o.o'; the command line by subprocess.list2cmdline; cwd ROOT. */
    AvpStr root_fwd = avp_str_replace(a, avp_str(root), AVP_S("\\"), AVP_S("/"), -1);
    const char *exe = avp_sprintf(a, "%.*s/tools/compilers/%.*s/mwcceppc.exe", AVP_SF(root_fwd), AVP_SF(ver));
    const char **cargv = AVP_NEWN(a, const char *, cflags.n + 6);
    size_t n = 0;
    cargv[n++] = exe;
    for (size_t i = 0; i < cflags.n; i++) cargv[n++] = avp_cz(a, cflags.v[i]);
    cargv[n++] = "-c";
    cargv[n++] = avp_path_abspath(a, src);
    cargv[n++] = "-o";
    cargv[n++] = avp_sprintf(a, "%s/o.o", outdir);
    /* runpy.run_path(spec): here the spec language (SPEC.md) */
    AvpStr text;
    if (!avp_read_text(a, spec_path, AVP_TM_UTF8, &text)) return fail_io(a);
    /* the compiler runs in a kill-on-close job (wdbg_run.h); the command line by subprocess.list2cmdline; cwd ROOT */
    WdbgRun wr = {.exe = exe, .build = avp_cz(a, ver), .argv = cargv, .argc = n, .cwd = avp_cz(a, root_fwd),
                  .outdir = outdir, .spec_path = spec_path, .spec_text = text};
    const char *cls = NULL, *msg = NULL;
    i64 code = -1;
    if (!wdbg_run(a, &wr, &cls, &msg, &code)) {
        if (cls) return fail(cls, avp_str(msg));
        fflush(stdout);
        fprintf(stderr, "%s\n", msg); /* a spec load error: "Class: message" */
        return 1;
    }
    return 0;
}

AVP_MAIN(run, "wdbg")
