/* permute: the tool (see permute.h). Function names follow tools/permute.py. */
#include "permute.h"

#include "codecmp.h"
#include "permute_doc.h"
#include "permute_strip.h"
#include "shsm_text.h"

static const char *g_root; /* ROOT: --root, else the checkout (REPO) */

#define DOCTOR "build/tools/doctor.exe --fix"
#define USAGE                                                                                                         \
    "permute.exe <unit> <function> [--run] [-j N] [--stop-on-zero] [--score] [--file F] [--lang {auto,c,c++}] "         \
    "[--root DIR] [--out DIR] [--no-check] [--keep-all] [-- extra permuter args]"

/* tool_path(*parts): under ROOT when it exists there, else under the checkout. */
static const char *tool_path(AvpArena *a, const char *rel)
{
    if (!g_root) g_root = permute_repo();
    const char *p = permute_join(a, g_root, rel);
    return avp_exists(p) ? p : permute_join(a, permute_repo(), rel);
}

static void out_text(AvpStr s) { fwrite(s.p, 1, s.n, stdout); }
static void out_line(AvpStr s)
{
    out_text(s);
    fputc('\n', stdout);
}

static bool lit_at(AvpStr s, size_t i, AvpStr lit) { return i + lit.n <= s.n && memcmp(s.p + i, lit.p, lit.n) == 0; }

/* the start of the line after the one holding i, or s.n */
static size_t next_line(AvpStr s, size_t i)
{
    i64 nl = avp_str_find(s, AVP_S("\n"), (i64)i, (i64)s.n);
    return nl < 0 ? s.n : (size_t)nl + 1;
}

static AvpStr read_retail_asm(AvpArena *a, AvpStr unit)
{
    const char *path = permute_join(a, g_root, avp_sprintf(a, "build\\R5WEA4\\asm\\%.*s.s", AVP_SF(unit)));
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) avp_fatal("%s", avp_io_err_str(a).p);
    return text;
}

/* ---- symbols ---------------------------------------------------------------------------------------------------- */

/* re.findall(r"^\.fn (\S+?),", asm, re.M), with dtk's quotes removed from a quoted name ("@36@__dt__10FMVTriggerFv":
 * D-permute-2, a reference defect fixed here) */
static AvpStr unquote(AvpStr n)
{
    return n.n >= 2 && n.p[0] == '"' && n.p[n.n - 1] == '"' ? avp_strn(n.p + 1, n.n - 2) : n;
}

static AvpStrVec fn_names(AvpStr s)
{
    AvpStrVec out = {0};
    for (size_t line = 0; line < s.n; line = next_line(s, line)) {
        if (!lit_at(s, line, AVP_S(".fn "))) continue;
        size_t i = line + 4, e = i;
        while (e < s.n && s.p[e] != ',' && !shsm_ws_len(s, e)) e++;
        if (e < s.n && s.p[e] == ',' && e > i) avp_vec_push(&out, unquote(avp_strn(s.p + i, e - i)));
    }
    return out;
}

bool permute_resolve_symbol(AvpArena *a, AvpStr asm_text, AvpStr name, bool cxx, AvpStr *sym, AvpStr *error)
{
    AvpStrVec fns = fn_names(asm_text);
    name = unquote(name);
    for (size_t k = 0; k < fns.n; k++)
        if (avp_str_eq(fns.v[k], name)) return *sym = name, true;
    if (cxx) {
        AvpStrVec cands = {0};
        AvpStr prefix = avp_str_concat(a, name, AVP_S("__")), src;
        for (size_t k = 0; k < fns.n; k++)
            if (avp_str_startswith(fns.v[k], prefix) && permute_source_name(a, fns.v[k], &src) && avp_str_eq(src, name))
                avp_vec_push(&cands, fns.v[k]);
        if (cands.n == 1) return *sym = cands.v[0], true;
        if (cands.n) {
            *error = avp_str(avp_sprintf(a, "%.*s names %zu functions; give the full mangled symbol:\n  %s",
                                         AVP_SF(name), cands.n,
                                         avp_cz(a, avp_str_join(a, AVP_S("\n  "), cands.v, cands.n))));
            return false;
        }
    }
    *error = avp_str(avp_sprintf(a, "function %.*s not in the unit's retail asm", AVP_SF(name)));
    return false;
}

AvpStr permute_retail_fn(AvpArena *a, AvpStr unit, AvpStr fn, AvpStr asm_text)
{
    /* ^\.fn FN,.*?^\.endfn FN$ (re.M | re.S) */
    /* dtk quotes a name that is not an identifier (D-permute-2): the quoted spelling is tried after the plain one */
    for (int quoted = 0; quoted < 2; quoted++) {
    const char *q = quoted ? "\"" : "";
    AvpStr open = avp_str(avp_sprintf(a, ".fn %s%.*s%s,", q, AVP_SF(fn), q));
    AvpStr close = avp_str(avp_sprintf(a, ".endfn %s%.*s%s", q, AVP_SF(fn), q));
    for (size_t s = 0; s < asm_text.n; s = next_line(asm_text, s)) {
        if (!lit_at(asm_text, s, open)) continue;
        for (size_t e = next_line(asm_text, s + open.n - 1); e < asm_text.n; e = next_line(asm_text, e)) {
            size_t end = e + close.n;
            if (lit_at(asm_text, e, close) && (end == asm_text.n || asm_text.p[end] == '\n'))
                return avp_strn(asm_text.p + s, end - s);
        }
    }
    }
    avp_fatal("function %.*s not in retail asm of %.*s", AVP_SF(fn), AVP_SF(unit));
}

AvpStrVec permute_compile_flags(const AvpStrVec *cflags, bool cxx)
{
    AvpStrVec out = {0};
    bool has_lang = false;
    for (size_t k = 0; k < cflags->n; k++) {
        bool lang = avp_str_startswith(cflags->v[k], AVP_S("-lang"));
        has_lang |= lang;
        if (cxx || !lang) avp_vec_push(&out, cflags->v[k]);
    }
    if (cxx && !has_lang) {
        avp_vec_push(&out, AVP_S("-lang"));
        avp_vec_push(&out, AVP_S("c++"));
    }
    return out;
}

AvpStr permute_settings_toml(AvpArena *a, AvpStr func_name, const char *objdump)
{
    AvpStr words[6] = {avp_str_replace(a, avp_str(objdump), AVP_S("\\"), AVP_S("/"), -1), AVP_S("-drz"), AVP_S("-m"),
                       AVP_S("powerpc"), AVP_S("-M"), AVP_S("broadway")};
    AvpStr cmd = permute_shlex_join(a, words, 6);
    cmd = avp_str_replace(a, cmd, AVP_S("\\"), AVP_S("\\\\"), -1);
    cmd = avp_str_replace(a, cmd, AVP_S("\""), AVP_S("\\\""), -1);
    return avp_str(avp_sprintf(a, "func_name = \"%.*s\"\ncompiler_type = \"mwcc\"\nobjdump_command = \"%.*s\"\n",
                               AVP_SF(func_name), AVP_SF(cmd)));
}

AvpStr permute_posix(AvpArena *a, const char *p)
{
    AvpStr s = avp_str_replace(a, avp_str(avp_path_abspath(a, p)), AVP_S("\\"), AVP_S("/"), -1);
    if (s.n >= 2 && s.p[1] == ':' && (((u8)s.p[0] | 0x20) >= 'a' && ((u8)s.p[0] | 0x20) <= 'z'))
        return avp_str(avp_sprintf(a, "/%c%.*s", s.p[0] | 0x20, (int)s.n - 2, s.p + 2));
    return s;
}

AvpStr permute_score_line(AvpArena *a, const CcFnResult *res, AvpStr error)
{
    if (!res) return avp_str_concat(a, AVP_S("error: "), error);
    if (cc_fn_result_exact(res)) return AVP_S("exact");
    return avp_str(avp_sprintf(a, "not exact (%zu of %zu rows differ, %zu target mismatches)", res->npositional,
                               res->rows, res->targets.n));
}

static const char *venv_python(AvpArena *a)
{
    const char *cands[] = {"tools\\venv\\Scripts\\python.exe", "tools\\venv\\bin\\python"};
    for (size_t k = 0; k < 2; k++) {
        const char *p = tool_path(a, cands[k]);
        if (avp_exists(p)) return p;
    }
    return NULL;
}

/* permuter_run.exe beside this executable: the launcher of decomp-permuter in the venv Python. */
static const char *permuter_run_exe(AvpArena *a) { return avp_join(a, avp_exe_dir(), "permuter_run.exe"); }

AvpStrVec permute_run_requirements(AvpArena *a, const char *root, bool have_binutils)
{
    AvpStrVec missing = {0};
    if (!avp_exists(permuter_run_exe(a)))
        avp_vec_push(&missing, avp_str(avp_sprintf(a, "permuter_run.exe is missing (looked for %s)", permuter_run_exe(a))));
    const char *permuter = tool_path(a, "tools\\permuter\\permuter.py");
    if (!avp_exists(permuter))
        avp_vec_push(&missing, avp_str(avp_sprintf(
                                   a, "tools/permuter is missing (decomp-permuter checkout; looked for %s)", permuter)));
    if (!venv_python(a))
        avp_vec_push(&missing, avp_str(avp_sprintf(a,
                                                   "tools/venv is missing (the permuter's Python with toml and "
                                                   "Levenshtein; looked for %s)",
                                                   tool_path(a, "tools\\venv"))));
    if (!have_binutils)
        avp_vec_push(&missing, avp_str(avp_sprintf(a,
                                                   "build/binutils is missing (powerpc-eabi-as/objdump for target.o; "
                                                   "looked in %s)",
                                                   permute_join(a, root, "build\\binutils"))));
    return missing;
}

/* ---- compile and score ------------------------------------------------------------------------------------------ */

static AvpStr cat_out(AvpArena *a, AvpStr out, AvpStr err) { return avp_str_concat(a, out, err); }

static const char **argv_of(AvpArena *a, const char *first, const AvpStrVec *flags, const char *const *tail,
                            size_t ntail)
{
    const char **v = avp_alloc(a, (flags->n + ntail + 2) * sizeof *v);
    size_t n = 0;
    v[n++] = first;
    for (size_t k = 0; k < flags->n; k++) v[n++] = avp_cz(a, flags->v[k]);
    for (size_t k = 0; k < ntail; k++) v[n++] = tail[k];
    v[n] = NULL;
    return v;
}

static void run_or_die(AvpArena *a, const char *const *argv, const char *cwd, i64 *rc, AvpStr *out, AvpStr *err)
{
    const char *msg = NULL;
    if (!permute_run_capture(a, argv, cwd, rc, out, err, &msg)) avp_fatal("FileNotFoundError: %s", msg);
}

/* compile_object: the compiler's output when it fails, else .p == NULL */
static AvpStr compile_object(AvpArena *a, const char *cc, const AvpStrVec *flags, const char *src, const char *obj,
                             const char *root)
{
    const char *tail[] = {"-c", avp_path_abspath(a, src), "-o", obj};
    i64 rc;
    AvpStr out, err;
    run_or_die(a, argv_of(a, cc, flags, tail, 4), root, &rc, &out, &err);
    return rc == 0 ? (AvpStr){0} : cat_out(a, out, err);
}

/* load_compiled(obj): dtk runs inside the object's directory. */
static AvpMap *load_compiled(AvpArena *a, const char *obj)
{
    const char *lst = avp_sprintf(a, "%s.s", obj);
    const char *argv[] = {tool_path(a, "tools\\bin\\dtk.exe"), "elf", "disasm", avp_cz(a, avp_path_basename(obj)),
                          avp_cz(a, avp_path_basename(lst)), NULL};
    i64 rc;
    AvpStr out, err;
    run_or_die(a, argv, avp_cz(a, avp_path_dirname(avp_path_abspath(a, obj))), &rc, &out, &err);
    if (rc) avp_fatal("subprocess.CalledProcessError: Command '%s' returned non-zero exit status %lld.",
                      avp_list2cmdline(a, argv), (long long)rc);
    AvpBytes data;
    if (!avp_read_all(a, obj, &data)) avp_fatal("%s", avp_io_err_str(a).p);
    const char *why = NULL;
    CcElf *elf = cc_elf_parse(a, data.v, data.n, &why);
    if (!elf) avp_fatal("ValueError: %s", why);
    AvpStr text;
    if (!avp_read_text(a, lst, AVP_TM_UTF8, &text)) avp_fatal("%s", avp_io_err_str(a).p);
    return cc_parse_listing(a, text, elf);
}

typedef struct {
    AvpStr compile_error; /* .p != NULL: the file does not compile (the compiler's output) */
    CcFnResult *res;      /* the comparison, or NULL with error */
    AvpStr error;
} PermuteScore;

static PermuteScore score_file(AvpArena *a, const char *root, AvpStr unit, const char *cc, const AvpStrVec *flags,
                               const char *srcfile, AvpStr sym)
{
    PermuteScore sc = {0};
    AvpStr stem, ext;
    avp_path_splitext(srcfile, &stem, &ext);
    const char *obj = avp_sprintf(a, "%.*s.o", AVP_SF(stem));
    sc.compile_error = compile_object(a, cc, flags, srcfile, obj, root);
    if (sc.compile_error.p) return sc;
    AvpMap *compiled = load_compiled(a, obj);
    AvpStr err = {0};
    AvpMap *retail = cc_load_asm(a, permute_join(a, root, avp_sprintf(a, "build\\R5WEA4\\asm\\%.*s.s", AVP_SF(unit))),
                                 &err);
    if (!retail) avp_fatal("%.*s", AVP_SF(err));
    CcContext *ctx = cc_context_new(cc_project_new(root), unit);
    AvpVal *c = avp_map_get(compiled, sym), *r = avp_map_get(retail, sym);
    if (!c) {
        AvpStrVec names = avp_map_keys(compiled);
        avp_sort_strs(names.v, names.n);
        sc.error = avp_str(avp_sprintf(a, "symbol %.*s not in the compiled object (it has: %s)", AVP_SF(sym),
                                       avp_cz(a, avp_str_join(a, AVP_S(", "), names.v, names.n < 8 ? names.n : 8))));
    } else if (!r) {
        sc.error = avp_str(avp_sprintf(a, "symbol %.*s not in the retail asm", AVP_SF(sym)));
    } else {
        sc.res = cc_compare_function(a, r->p, c->p, ctx, true);
    }
    return sc;
}

/* report(res): 0 only when the function is exact */
static int report(AvpArena *a, const PermuteScore *sc)
{
    if (!sc->res) {
        out_line(permute_score_line(a, NULL, sc->error));
        return 1;
    }
    out_line(codecmp_describe_result(a, sc->res->name, sc->res, false, 40));
    out_line(permute_score_line(a, sc->res, (AvpStr){0}));
    return cc_fn_result_exact(sc->res) ? 0 : 1;
}

/* preprocess_unit: mwcceppc -E with the unit's flags, line markers dropped, blank runs squeezed */
static AvpStr preprocess_unit(AvpArena *a, const char *cc, const AvpStrVec *cflags, const char *src, const char *root)
{
    const char *tail[] = {"-E", "-c", src};
    i64 rc;
    AvpStr out, err;
    run_or_die(a, argv_of(a, cc, cflags, tail, 3), root, &rc, &out, &err);
    if (rc) {
        out_text(cat_out(a, out, err));
        avp_exit(1);
    }
    /* re.sub(r"^#(?!pragma).*$", "", text, flags=re.M) */
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t line = 0; line < out.n;) {
        size_t end = next_line(out, line);
        bool drop = out.p[line] == '#' && !lit_at(out, line + 1, AVP_S("pragma"));
        if (drop) {
            if (end > line && out.p[end - 1] == '\n') avp_buf_putc(&b, '\n');
        } else {
            avp_buf_put(&b, avp_strn(out.p + line, end - line));
        }
        line = end;
    }
    /* re.sub(r"\n{3,}", "\n\n", text) */
    AvpStr t = avp_buf_str(&b);
    AvpBuf r;
    avp_buf_init(&r, a);
    for (size_t i = 0; i < t.n;) {
        if (t.p[i] == '\n') {
            size_t j = i;
            while (j < t.n && t.p[j] == '\n') j++;
            avp_buf_putn(&r, "\n\n", j - i >= 3 ? 2 : j - i);
            i = j;
        } else {
            avp_buf_putc(&r, t.p[i++]);
        }
    }
    return avp_buf_str(&r);
}

/* ---- the command line ------------------------------------------------------------------------------------------- */

static bool positive_int(AvpStr s)
{
    if (!s.n || s.p[0] < '1' || s.p[0] > '9') return false;
    for (size_t i = 1; i < s.n; i++)
        if (s.p[i] < '0' || s.p[i] > '9') return false;
    return true;
}

static void write_or_die(const char *path, AvpStr text, AvpTextMode m)
{
    if (!avp_write_text(path, text, m)) avp_fatal("%s", avp_io_err_str(avp_arena()).p);
}

int permute_main(int argc, char **argv)
{
    AvpArena *a = avp_arena();
    int nargs = argc;
    AvpStrVec extra = {0};
    for (int k = 1; k < argc; k++)
        if (strcmp(argv[k], "--") == 0) {
            nargs = k;
            for (int j = k + 1; j < argc; j++) avp_vec_push(&extra, avp_str(argv[j]));
            break;
        }
    static const char *const langs[] = {"auto", "c", "c++", NULL};
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){.prog = "permute.exe", .usage = USAGE,
                                                               .description = PERMUTE_DESCRIPTION,
                                                               .epilog = PERMUTE_EPILOG,
                                                               .formatter = AVP_FORMAT_RAW_DESCRIPTION});
    avp_arg(ap, &(AvpArgDesc){.names = "unit", .help = "unit as in objdiff.json (\"main/\" optional)"});
    avp_arg(ap, &(AvpArgDesc){.names = "function",
                              .help = "symbol name (mangled for C++), or a source name naming one function"});
    avp_arg(ap, &(AvpArgDesc){.names = "--run", .action = AVP_ARG_STORE_TRUE,
                              .help = "start decomp-permuter on the directory (C bases only)"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j", .dflt = AVP_DEF_STR("4"), .metavar = "N",
                              .help = "permuter worker count for --run (default 4)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--stop-on-zero", .action = AVP_ARG_STORE_TRUE,
                              .help = "--run: stop at the first exact state"});
    avp_arg(ap, &(AvpArgDesc){.names = "--score", .action = AVP_ARG_STORE_TRUE,
                              .help = "only compile and compare; exit 0 only when exact"});
    avp_arg(ap, &(AvpArgDesc){.names = "--file", .metavar = "F",
                              .help = "--score: the file to compile (default: the directory's base)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--lang", .choices = langs, .dflt = AVP_DEF_STR("auto"),
                              .help = "base language (default auto)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                              .help = "take build, asm, compilers and tools from another checkout"});
    avp_arg(ap, &(AvpArgDesc){.names = "--out", .metavar = "DIR",
                              .help = "the directory (default <root>/build/permute/<function>)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-check", .action = AVP_ARG_STORE_TRUE,
                              .help = "do not compile and score the base after setup"});
    avp_arg(ap, &(AvpArgDesc){.names = "--keep-all", .action = AVP_ARG_STORE_TRUE,
                              .help = "keep every function body in the base"});
    AvpArgs *args = avp_args_parse(ap, nargs, argv);
    AvpStr jobs = avp_args_str(args, "j");
    if (!positive_int(jobs))
        avp_args_error(ap, "-j needs a positive integer, not %s", avp_cz(a, avp_repr_str(a, jobs)));

    bool run = avp_args_bool(args, "run"), stop0 = avp_args_bool(args, "stop_on_zero");
    bool score_only = avp_args_bool(args, "score"), check = !avp_args_bool(args, "no_check");
    AvpStr unit = avp_args_str(args, "unit"), fn = avp_args_str(args, "function");
    if (avp_str_startswith(unit, AVP_S("main/"))) unit = avp_str_slice(unit, 5, (i64)unit.n);
    AvpStr root_opt = avp_args_str(args, "root");
    g_root = root_opt.n ? avp_path_abspath(a, avp_cz(a, root_opt)) : permute_repo();

    const char *ver;
    AvpStrVec cflags;
    permute_unit_flags(a, g_root, unit, &ver, &cflags);
    const char *cc = tool_path(
        a, avp_sprintf(a, "tools\\compilers\\%s\\mwcceppc.exe",
                       avp_cz(a, avp_str_replace(a, avp_str(ver), AVP_S("/"), AVP_S("\\"), -1))));
    const char *src = permute_find_source(a, g_root, unit);
    AvpStr asm_text = read_retail_asm(a, unit);
    bool unit_is_cxx = !avp_str_endswith(avp_str(src), AVP_S(".c"));
    AvpStr sym, error;
    if (!permute_resolve_symbol(a, asm_text, fn, unit_is_cxx, &sym, &error)) avp_fatal("%s", avp_cz(a, error));
    AvpStr lang = avp_args_str(args, "lang");
    bool cxx;
    if (avp_str_eqz(lang, "auto")) {
        cxx = unit_is_cxx && permute_is_mangled(sym);
    } else {
        cxx = avp_str_eqz(lang, "c++");
        if (cxx && !unit_is_cxx)
            avp_fatal("--lang c++ needs a C++ unit (this one is %.*s)", AVP_SF(avp_path_basename(src)));
    }
    const char *base_name = cxx ? "base.cpp" : "base.c";
    AvpStr out_opt = avp_args_str(args, "out");
    const char *d = out_opt.n ? avp_path_abspath(a, avp_cz(a, out_opt))
                              : permute_join(a, permute_join(a, g_root, "build\\permute"),
                                             avp_cz(a, permute_dir_name(a, sym)));
    if (!avp_makedirs(d, true)) avp_fatal("%s", avp_io_err_str(a).p);
    AvpStrVec flags = permute_compile_flags(&cflags, cxx);

    if (score_only) {
        AvpStr file_opt = avp_args_str(args, "file");
        const char *base = file_opt.n ? avp_cz(a, file_opt)
                                      : permute_join(a, d, avp_exists(permute_join(a, d, base_name)) ? base_name
                                                                                                     : "base.c");
        if (!avp_exists(base)) avp_fatal("no %s; run without --score first", base);
        bool base_cxx = avp_str_endswith(avp_str(base), AVP_S(".cpp")) || avp_str_endswith(avp_str(base), AVP_S(".cp"));
        flags = permute_compile_flags(&cflags, base_cxx);
        PermuteScore sc = score_file(a, g_root, unit, cc, &flags, base, sym);
        if (sc.compile_error.p) {
            out_text(sc.compile_error);
            return 1;
        }
        return report(a, &sc);
    }

    /* base: preprocess with the unit's flags, strip other bodies */
    AvpStr text = preprocess_unit(a, cc, &cflags, src, g_root), body;
    if (cxx) {
        AvpStr name;
        permute_source_name(a, sym, &name);
        size_t kept;
        AvpStrVec callees = {0};
        if (avp_args_bool(args, "keep_all")) {
            body = text;
            kept = 1;
        } else {
            body = permute_strip_other_fns_cxx(a, text, &name, 1, true, &kept, &callees);
        }
        if (!kept) avp_fatal("no definition of %s found in the preprocessed unit", avp_cz(a, avp_repr_list(a, &name, 1)));
        printf("C++ base: %zu definition(s) named %.*s kept, callees kept (inlining candidates): %s\n", kept,
               AVP_SF(name), callees.n ? avp_cz(a, avp_str_join(a, AVP_S(", "), callees.v, callees.n)) : "none");
    } else {
        if (unit_is_cxx && !permute_strip_extern_c(a, text, &text)) avp_fatal("IndexError: string index out of range");
        if (!permute_strip_other_fns(a, text, fn, &body)) avp_fatal("IndexError: string index out of range");
    }
    const char *base = permute_join(a, d, base_name);
    write_or_die(base, body, AVP_TM_UTF8);

    /* target.o from the retail asm block */
    const char *tgt_s = permute_join(a, d, "target.s");
    write_or_die(tgt_s,
                 avp_str(avp_sprintf(a, ".include \"macros.inc\"\n.text\n.balign 4\n%s\n",
                                     avp_cz(a, permute_retail_fn(a, unit, sym, asm_text)))),
                 AVP_TM_UTF8);
    const char *binu = permute_join(a, g_root, "build\\binutils");
    const char *asm_exe = permute_join(a, binu, "powerpc-eabi-as.exe");
    bool have_binutils = avp_exists(asm_exe);
    if (have_binutils) {
        const char *argv_as[] = {asm_exe, "-mgekko", "-I", permute_join(a, g_root, "build\\R5WEA4\\include"), tgt_s,
                                 "-o", permute_join(a, d, "target.o"), NULL};
        i64 rc;
        AvpStr out, err;
        run_or_die(a, argv_as, NULL, &rc, &out, &err);
        if (rc) {
            out_text(cat_out(a, out, err));
            return 1;
        }
    } else {
        printf("note: target.o not built: build/binutils is missing (%s); `%s` restores it (or tools/bin/ninja.exe "
               "build/binutils). --score does not need it, --run does\n",
               binu, DOCTOR);
    }

    /* compile.sh: invoked as ./compile.sh input.c -o output.o */
    AvpStr qflags = permute_shlex_join(a, flags.v, flags.n);
    write_or_die(permute_join(a, d, "compile.sh"),
                 avp_str(avp_sprintf(a,
                                     "#!/bin/bash\n# generated by permute.exe for %.*s (%s)\n"
                                     "in=$(realpath \"$1\"); out=$(realpath -m \"$3\")\ncd %s\n"
                                     "exec %s %s -c \"$in\" -o \"$out\"\n",
                                     AVP_SF(unit), ver, avp_cz(a, permute_shlex_quote(a, permute_posix(a, g_root))),
                                     avp_cz(a, permute_shlex_quote(a, permute_posix(a, cc))), avp_cz(a, qflags))),
                 AVP_TM_UTF8_NL_LF);
    write_or_die(permute_join(a, d, "settings.toml"),
                 permute_settings_toml(a, cxx ? sym : fn, permute_join(a, binu, "powerpc-eabi-objdump.exe")),
                 AVP_TM_UTF8_NL_LF);

    printf("permuter directory: %s\n", d);
    int status = 0;
    if (check) {
        PermuteScore sc = score_file(a, g_root, unit, cc, &flags, base, sym);
        if (sc.compile_error.p) {
            out_text(sc.compile_error);
            printf("error: %s does not compile\n", base_name);
            return 1;
        }
        /* a successful setup exits 0 whatever the base scores; the score is the line below */
        status = sc.res ? 0 : 1;
        report(a, &sc);
        printf("base score: %s\n", avp_cz(a, permute_score_line(a, sc.res, sc.error)));
    }
    if (!run) return status;
    if (cxx) {
        printf("--run: refused. decomp-permuter parses base.c with pycparser, a C parser; it cannot read the C++ in "
               "%s. Edit %s by hand and re-score it with --score.\n",
               base_name, base_name);
        return 2;
    }
    AvpStrVec missing = permute_run_requirements(a, g_root, have_binutils);
    if (missing.n) {
        for (size_t k = 0; k < missing.n; k++) printf("--run: %.*s\n", AVP_SF(missing.v[k]));
        printf("--run: `%s` restores %s\n", DOCTOR, missing.n > 1 ? "them" : "it");
        return 1;
    }
    AvpStrVec cmd = {0};
    avp_vec_push(&cmd, avp_str(permuter_run_exe(a)));
    avp_vec_push(&cmd, avp_str(d));
    avp_vec_push(&cmd, AVP_S("-j"));
    avp_vec_push(&cmd, jobs);
    if (stop0) avp_vec_push(&cmd, AVP_S("--stop-on-zero"));
    for (size_t k = 0; k < extra.n; k++) avp_vec_push(&cmd, extra.v[k]);
    const char **cv = avp_alloc(a, (cmd.n + 1) * sizeof *cv);
    for (size_t k = 0; k < cmd.n; k++) cv[k] = avp_cz(a, cmd.v[k]);
    const char *permuter_dir = avp_cz(a, avp_path_dirname(tool_path(a, "tools\\permuter\\permuter.py")));
    /* subprocess.call: the child shares this process's stdout and stderr; nothing is flushed first (as Python) */
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = cv, .cwd = permuter_dir});
    if (r.status == AVP_PROC_START_FAILED) avp_fatal("FileNotFoundError: %s", r.message);
    return (int)r.returncode;
}
