/* cc.exe: a port of python tools/mwcc_trace/cc.py. Compile one C file with a unit's exact flags (build.ninja) and
 * print its functions' disassembly, or compare them with the retail listing (--cmp: resolved relocation targets via
 * the cc_codecmp library; --cmp --no-targets: the old name-blind positional comparison). The object goes to
 * build/tmp/mwcc_trace/<stem>.o. Exit 0, or 1 on a compiler failure (its output on stdout) or an error the Python
 * raised (SystemExit / a traceback: the message on stderr). */
#include "cctool.h"
#include "shsm_text.h"

static void out(AvpStr s) { fwrite(s.p, 1, s.n, stdout); }
static void outln(AvpStr s) {
    out(s);
    fputc('\n', stdout);
}

/* The Python died with an exception (traceback on stderr, exit 1). */
static int fail(const char *cls, AvpStr msg) {
    fflush(stdout);
    fprintf(stderr, "%s: %.*s\n", cls, AVP_SF(msg));
    return 1;
}

static AvpArgParser *make_parser(void) {
    AvpArgParser *ap = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = "cc.exe"});
    static const char *const none[] = {NULL};
    avp_arg(ap, &(AvpArgDesc){.names = "file"});
    avp_arg(ap, &(AvpArgDesc){.names = "-u", .dflt = AVP_DEF_STR("MSL_C/MSL_Common/Src/mem_funcs"),
                              .help = "the unit whose flags and retail listing are used: fmod/fmod or main/fmod/fmod"});
    avp_arg(ap, &(AvpArgDesc){.names = "-v"});
    avp_arg(ap, &(AvpArgDesc){.names = "-f", .action = AVP_ARG_APPEND, .dflt_list = none,
                              .help = "extra compiler flags appended to the unit's (shlex-split, repeatable): -f \"-O2\", "
                                      "-f=\"-O2 -inline off\" or -f -O2"});
    avp_arg(ap, &(AvpArgDesc){.names = "-o", .help = "keep the compiled object at PATH (default: "
                                                      "build/tmp/mwcc_trace/<stem>.o)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--file-macro", .metavar = "PATH",
                              .help = "compile with __FILE__ presented as the file name of PATH (MWCC's __FILE__ is the "
                                      "base name of the compiled file), so a probe compiled from a scratch file "
                                      "carries the unit's own string: FMOD's allocator tags embed __FILE__. The source "
                                      "is compiled from a copy of that name under build/tmp/mwcc_trace/file_macro"});
    avp_arg(ap, &(AvpArgDesc){.names = "-s", .action = AVP_ARG_APPEND, .dflt_list = none});
    avp_arg(ap, &(AvpArgDesc){.names = "--raw", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--cmp", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-targets", .action = AVP_ARG_STORE_TRUE,
                              .help = "--cmp: compare with every symbol name collapsed (the old behaviour)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--unresolved", .action = AVP_ARG_STORE_TRUE,
                              .help = "--cmp: list the undecidable targets"});
    avp_arg(ap, &(AvpArgDesc){.names = "--map", .action = AVP_ARG_APPEND, .dflt_list = none,
                              .help = "SYMBOL=RETAIL: compare compiled SYMBOL with retail RETAIL (repeatable)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--asm", .help = "retail asm file for --cmp (default: build/R5WEA4/asm/<UNIT>.s)"});
    return ap;
}

typedef struct {
    AvpArena *a;
    const char *root;
    const char *unit;
    const char *asm_path;
    AvpMap *names; /* --map: compiled -> AvpStr* retail */
    AvpSet *wanted;
    bool unresolved;
} Opts;

static const char *retail_path(const Opts *o) {
    if (o->asm_path) return o->asm_path;
    return avp_join(o->a, o->root, "build", "R5WEA4", "asm", avp_sprintf(o->a, "%s.s", o->unit));
}

/* compare_targets(a, obj). */
static int compare_targets(const Opts *o, const char *obj) {
    AvpArena *a = o->a;
    AvpStr err = {0};
    const char *rp = retail_path(o);
    AvpMap *retail = cc_load_asm(a, rp, &err);
    if (!retail) return fail(avp_io_err_class(), err);
    AvpMap *compiled = cc_load_object(a, obj, NULL, &err);
    if (!compiled) return fail("CalledProcessError", err);
    CcContext *ctx = cc_context_new(cc_project_new(o->root), avp_str(o->unit));
    AvpMapIter it = {0};
    while (avp_map_next(compiled, &it)) {
        AvpStr fn = it.key;
        if (avp_set_len(o->wanted) && !avp_set_has(o->wanted, fn)) continue;
        AvpVal *mv = avp_map_get(o->names, fn);
        AvpStr want;
        if (mv) {
            want = *(AvpStr *)mv->p;
        } else {
            AvpStr pre = cctool_fn_prefix(fn);
            if (avp_map_has(retail, fn)) want = fn;
            else if (pre.n && avp_map_has(retail, pre)) want = pre;
            else want = cc_retail_name_for(fn, retail);
        }
        if (!want.p) continue;
        AvpVal *rv = avp_map_get(retail, want);
        if (!rv) continue;
        CcFnResult *res = cc_compare_function(a, rv->p, it.val->p, ctx, true);
        /* aligned as match.exe aligns, so the counts agree with `match.exe --diff` */
        CcCompareResult *cr = cc_compare_result_new(a, fn, want, rv->p, it.val->p, res);
        outln(cctool_describe_aligned(a, fn, cr, o->unresolved, 40));
    }
    return 0;
}

/* The old comparison (--cmp --no-targets). */
static int compare_positional(const Opts *o, AvpStr text) {
    AvpArena *a = o->a;
    AvpStr retail;
    if (!avp_read_text(a, retail_path(o), AVP_TM_UTF8, &retail)) return fail(avp_io_err_class(), avp_io_err_str(a));
    AvpMap *fns = cctool_split_fns(a, text);
    AvpMapIter it = {0};
    while (avp_map_next(fns, &it)) {
        AvpStr fn = it.key;
        AvpVal *mv = avp_map_get(o->names, fn);
        AvpStr want = mv ? *(AvpStr *)mv->p : fn;
        if (avp_str_eq(want, fn) && !cctool_retail_has(retail, fn)) {
            AvpStr pre = cctool_fn_prefix(fn);
            want = pre.n ? pre : fn;
        }
        AvpStr body;
        if (!cctool_retail_block(retail, want, &body)) continue;
        outln(cctool_positional(a, fn, it.val->p, body));
    }
    return 0;
}

/* -u accepts the unit as build.ninja names it (fmod/fmod) or as the tools do (main/fmod/fmod, either slash). */
static const char *unit_arg(AvpArena *a, AvpStr u) {
    AvpStr s = avp_str_replace(a, u, AVP_S("\\"), AVP_S("/"), -1);
    s = avp_str_removeprefix(s, AVP_S("main/"));
    return avp_cz(a, s);
}

static int run(int argc, char **argv) {
    AvpArena *a = avp_arena();
    /* `-f -O2`: a flag value that starts with - would be read as an option; join it to its -f as `-f=-O2` */
    char **argv2 = AVP_NEWN(a, char *, (size_t)argc + 1);
    int argc2 = 0;
    for (int i = 0; i < argc; i++) {
        if (i + 1 < argc && !strcmp(argv[i], "-f") && argv[i + 1][0] == '-' && argv[i + 1][1]) {
            argv2[argc2++] = avp_sprintf(a, "-f=%s", argv[i + 1]);
            i++;
        } else {
            argv2[argc2++] = argv[i];
        }
    }
    argv2[argc2] = NULL;
    AvpArgs *args = avp_args_parse(make_parser(), argc2, argv2);
    Opts o = {0};
    o.a = a;
    o.root = avp_path_normpath(a, avp_root()); /* the Python ROOT: backslashes */
    o.unit = unit_arg(a, avp_args_str(args, "u"));
    o.asm_path = avp_args_str(args, "asm").p;
    o.unresolved = avp_args_bool(args, "unresolved");
    o.names = avp_map_new();
    o.wanted = avp_set_new();
    AvpStrVec maps = avp_args_strs(args, "map");
    AvpStrVec syms = avp_args_strs(args, "s");
    AvpStrVec fs = avp_args_strs(args, "f");
    const char *file = avp_args_str(args, "file").p;

    AvpStr ninja;
    const char *ninja_path = avp_path_join(a, o.root, "build.ninja");
    if (!avp_read_text(a, ninja_path, AVP_TM_UTF8, &ninja)) return fail(avp_io_err_class(), avp_io_err_str(a));
    AvpStr ver;
    AvpStrVec cflags = {0};
    const char *err = NULL;
    int rc = cctool_unit_flags(a, ninja, o.unit, &ver, &cflags, &err);
    if (rc == 1) {
        fflush(stdout);
        fprintf(stderr, "unit not found in build.ninja: %s\n", o.unit);
        return 1;
    }
    if (rc == 2) return fail("AttributeError", AVP_S("'NoneType' object has no attribute 'group'"));
    if (rc == 3) return fail("ValueError", avp_str(err));
    AvpStr v = avp_args_str(args, "v");
    if (v.n) ver = v;
    for (size_t i = 0; i < fs.n; i++) {
        AvpStrVec words = {0};
        if (!shsm_shlex_split(a, fs.v[i], &words, &err)) return fail("ValueError", avp_str(err));
        for (size_t k = 0; k < words.n; k++) avp_vec_push(&cflags, words.v[k]);
        avp_vec_free(&words);
    }
    for (size_t i = 0; i < syms.n; i++) avp_set_add(o.wanted, syms.v[i]);

    const char *outdir = avp_join(a, o.root, "build", "tmp", "mwcc_trace");
    if (!avp_makedirs(outdir, true)) return fail(avp_io_err_class(), avp_io_err_str(a));
    AvpStr stem, ext;
    AvpStr base = avp_path_basename(file);
    avp_path_splitext(avp_str_dup(a, base).p, &stem, &ext);
    const char *obj = avp_path_join(a, outdir, avp_sprintf(a, "%.*s.o", AVP_SF(stem)));
    const char *keep = avp_args_str(args, "o").p;
    if (keep) {
        obj = avp_path_abspath(a, keep);
        if (!avp_makedirs(avp_cz(a, avp_path_dirname(obj)), true)) return fail(avp_io_err_class(), avp_io_err_str(a));
    }
    /* --file-macro: MWCC's __FILE__ is the base name of the file it compiles (`#line` does not change it), so the
     * source is copied under the unit's file name into a folder of its own and the original's folder is added to the
     * include path, as the copy lives beside the object. */
    const char *src_path = avp_path_abspath(a, file);
    const char *file_macro = avp_args_str(args, "file_macro").p;
    const char *src_dir_flag = NULL;
    if (file_macro) {
        AvpStr text;
        if (!avp_read_text(a, src_path, AVP_TM_UTF8, &text)) return fail(avp_io_err_class(), avp_io_err_str(a));
        AvpStr want = avp_path_basename(avp_cz(a, avp_str_replace(a, avp_str(file_macro), AVP_S("\\"), AVP_S("/"), -1)));
        if (!want.n) return fail("ValueError", AVP_S("--file-macro needs a file name"));
        const char *copy_dir = avp_path_join(a, outdir, "file_macro");
        if (!avp_makedirs(copy_dir, true)) return fail(avp_io_err_class(), avp_io_err_str(a));
        src_dir_flag = avp_cz(a, avp_path_dirname(src_path));
        src_path = avp_path_join(a, copy_dir, avp_cz(a, want));
        if (!avp_write_text(src_path, text, AVP_TM_UTF8_NL_LF)) return fail(avp_io_err_class(), avp_io_err_str(a));
    }
    AvpStrVec vparts = avp_str_split(ver, AVP_S("/"), -1);
    const char *cc = avp_join(a, o.root, "tools", "compilers");
    for (size_t i = 0; i < vparts.n; i++) cc = avp_path_join(a, cc, avp_str_dup(a, vparts.v[i]).p);
    cc = avp_path_join(a, cc, "mwcceppc.exe");

    AvpPtrVec cargv = {0};
    avp_vec_push(&cargv, (void *)cc);
    for (size_t i = 0; i < cflags.n; i++) avp_vec_push(&cargv, (void *)avp_str_dup(a, cflags.v[i]).p);
    if (src_dir_flag) {
        avp_vec_push(&cargv, (void *)"-i");
        avp_vec_push(&cargv, (void *)src_dir_flag);
    }
    avp_vec_push(&cargv, (void *)"-c");
    avp_vec_push(&cargv, (void *)src_path);
    avp_vec_push(&cargv, (void *)"-o");
    avp_vec_push(&cargv, (void *)obj);
    avp_vec_push(&cargv, NULL);
    AvpProcSpec spec = {0};
    spec.argv = (const char *const *)cargv.v;
    spec.cwd = o.root;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    spec.errors = AVP_ERRORS_REPLACE;
    spec.arena = a;
    AvpProcResult r = avp_run(&spec);
    if (r.status == AVP_PROC_START_FAILED)
        return fail("FileNotFoundError", avp_str(r.message ? r.message : "cannot start the compiler"));
    if (r.returncode) {
        out(r.out);
        out(r.err);
        return 1;
    }
    bool cmp = avp_args_bool(args, "cmp");
    for (size_t i = 0; cmp && i < maps.n; i++) {
        AvpStr k, val;
        if (!avp_str_partition(maps.v[i], AVP_S("="), &k, &val))
            return fail("ValueError", avp_pyfmt(a, "dictionary update sequence element #%d has length 1; 2 is required", i));
        AvpStr *p = AVP_NEW(a, AvpStr);
        *p = val;
        avp_map_put(o.names, k, NULL)->p = p;
    }
    if (cmp && !avp_args_bool(args, "no_targets")) return compare_targets(&o, obj);

    /* disasm(obj, set(a.s), a.raw) */
    const char *lst = avp_sprintf(a, "%s.s", obj);
    const char *dtk = avp_join(a, o.root, "tools", "bin", "dtk.exe");
    const char *dargv[] = {dtk, "elf", "disasm", obj, lst, NULL};
    AvpProcSpec ds = {0};
    ds.argv = dargv;
    ds.out = AVP_IO_CAPTURE;
    ds.err = AVP_IO_CAPTURE;
    ds.arena = a;
    AvpProcResult dr = avp_run(&ds);
    if (dr.status == AVP_PROC_START_FAILED)
        return fail("FileNotFoundError", avp_str(dr.message ? dr.message : "cannot start dtk"));
    if (dr.returncode)
        return fail("subprocess.CalledProcessError",
                    avp_pyfmt(a, "Command '%s' returned non-zero exit status %d.", dtk, dr.returncode));
    AvpStr listing;
    if (!avp_read_text(a, lst, AVP_TM_UTF8, &listing)) return fail(avp_io_err_class(), avp_io_err_str(a));
    AvpStr text = cctool_filter_listing(a, listing, o.wanted, avp_args_bool(args, "raw"));
    if (!cmp) {
        outln(text);
        return 0;
    }
    return compare_positional(&o, text);
}

AVP_MAIN(run, "cc")
