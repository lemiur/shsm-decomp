/* The remaining tests of tools/test_permute.py (the SymbolNames and StripCxx classes are in strip_test.c):
 * SymbolNames.test_resolve_symbol, CompileCxx (4), Settings (1), Args (3), MainFlow (5).
 *
 * CompileCxx drives the compiler and dtk as the Python test does (skipped, passing, when tools/compilers/Wii/1.0 or
 * tools/bin/dtk.exe is absent). preprocess_unit, compile_object, load_compiled and score_symbol are static in
 * permute.c; they are repeated here from the public pieces (the compiler through avp_run, cc_load_object, the
 * strippers of permute_strip.h, cc_compare_function).
 *
 * Args runs build/tools/permute.exe (tests run from the repository root): the Python calls parse_args directly, the
 * C parses inside permute_main. The documented command lines are checked by their reaching the unit lookup (exit 1,
 * "unit not found"), not by the parsed fields.
 *
 * MainFlow replaces the compiler and the asm with stubs (monkeypatching Python module functions); C has no such seam.
 * Left to the differential harness (tools/c/tools/permute/diff/cases.txt: setup, --score, the build/binutils note, the
 * --run requirements): test_setup_exits_0_and_prints_the_base_score, test_score_exits_0_only_when_exact and
 * test_binutils_note_names_doctor. The two --run tests are ported onto permute_run_requirements, whose message list is
 * what main prints; the closing "restores it" line is printed by permute_main and is not reachable from there. */
#include <stdlib.h>
#include "avp/avp.h"
#include "avp/test.h"
#include "permute.h"
#include "permute_strip.h"

#define N(a) (sizeof(a) / sizeof((a)[0]))

static const char UNIT[] =
    "namespace game {\n"
    "class Counter {\n"
    "public:\n"
    "    Counter(int start) : mCount(start) {}\n"
    "    virtual ~Counter() {}\n"
    "    inline int Get() const { return mCount; }\n"
    "    void Add(int n);\n"
    "    int Twice(int n) const;\n"
    "    int Total() const;\n"
    "    static int Clamp(int v);\n"
    "private:\n"
    "    int mCount;\n"
    "    int mLimit;\n"
    "};\n"
    "}\n"
    "\n"
    "static int Square(int v) { return v * v; }\n"
    "int Helper(int v) { return v + 1; }\n"
    "int Unrelated(int v) { return v - 1; }\n"
    "\n"
    "namespace game {\n"
    "int Counter::Clamp(int v) { return v > 100 ? 100 : v; }\n"
    "\n"
    "void Counter::Add(int n) {\n"
    "    mCount = Clamp(mCount + Square(n));\n"
    "}\n"
    "\n"
    "int Counter::Twice(int n) const {\n"
    "    return Helper(n) * 2 + Get();\n"
    "}\n"
    "\n"
    "int Counter::Total() const {\n"
    "    return Unrelated(mCount);\n"
    "}\n"
    "}\n";

static const char ADD[] = "Add__Q24game7CounterFi";
static const char TWICE[] = "Twice__Q24game7CounterCFi";

static bool has(AvpStr s, const char *needle) { return strstr(avp_cz(avp_arena(), s), needle) != NULL; }

/* ---- SymbolNames.test_resolve_symbol ----------------------------------------------------------------------------- */

static AvpStr resolve(AvpStr asm_text, const char *name, bool cxx, bool *ok, AvpStr *error) {
    AvpStr sym = {0};
    *error = (AvpStr){0};
    *ok = permute_resolve_symbol(avp_arena(), asm_text, avp_str(name), cxx, &sym, error);
    return sym;
}

AVP_TEST(resolve_symbol) {
    AvpArena *a = avp_arena();
    const char *names[] = {"Draw__Q24game7CounterFv", "DrawFrame__Q24game7CounterFi", "DrawFrame4__Q24game7CounterFi",
                           "Add__Q24game7CounterFi", "Add__Q24game7CounterFf", "OSInit"};
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t k = 0; k < N(names); k++)
        avp_buf_printf(&b, "%s.fn %s, global\n.endfn %s", k ? "\n" : "", names[k], names[k]);
    AvpStr asm_text = avp_buf_str(&b), error;
    bool ok;
    CHECK_STR(resolve(asm_text, "OSInit", true, &ok, &error), "OSInit");
    CHECK(ok);
    CHECK_STR(resolve(asm_text, "Draw", true, &ok, &error), "Draw__Q24game7CounterFv");
    CHECK_STR(resolve(asm_text, "DrawFrame", true, &ok, &error), "DrawFrame__Q24game7CounterFi");
    CHECK_STR(resolve(asm_text, "Add__Q24game7CounterFf", true, &ok, &error), "Add__Q24game7CounterFf");
    (void)resolve(asm_text, "Add", true, &ok, &error); /* overloads: the full symbol is required */
    CHECK(!ok);
    CHECK(has(error, "Add__Q24game7CounterFi"));
    (void)resolve(asm_text, "Missing", true, &ok, &error);
    CHECK(!ok);
    (void)resolve(asm_text, "Draw", false, &ok, &error); /* a C unit takes exact names only */
    CHECK(!ok);
}

/* D-permute-2: dtk quotes a thunk's name; the Python did not find it (or only with the quotes typed). */
AVP_TEST(resolve_quoted_thunk) {
    AvpArena *a = avp_arena();
    AvpStr asm_text = AVP_S(".fn \"@36@__dt__10FMVTriggerFv\", global\nblr\n.endfn \"@36@__dt__10FMVTriggerFv\"\n"),
           error;
    bool ok;
    CHECK_STR(resolve(asm_text, "@36@__dt__10FMVTriggerFv", true, &ok, &error), "@36@__dt__10FMVTriggerFv");
    CHECK(ok);
    CHECK_STR(resolve(asm_text, "\"@36@__dt__10FMVTriggerFv\"", true, &ok, &error), "@36@__dt__10FMVTriggerFv");
    CHECK_STR(permute_retail_fn(a, AVP_S("game/FMVTrigger"), AVP_S("@36@__dt__10FMVTriggerFv"), asm_text),
              ".fn \"@36@__dt__10FMVTriggerFv\", global\nblr\n.endfn \"@36@__dt__10FMVTriggerFv\"");
}

/* ---- CompileCxx -------------------------------------------------------------------------------------------------- */

static const char *const FLAGS[] = {"-nodefaults", "-proc", "gekko", "-align", "powerpc", "-enum", "int", "-fp",
                                    "hardware", "-Cpp_exceptions", "off", "-O4,p", "-inline", "auto", "-RTTI", "off",
                                    "-nosyspath", "-maxerrors", "1", "-lang=c++"};

static AvpStrVec flags_vec(bool drop_lang) {
    AvpStrVec v = {0};
    for (size_t k = 0; k < N(FLAGS); k++)
        if (!(drop_lang && strncmp(FLAGS[k], "-lang", 5) == 0)) avp_vec_push(&v, avp_str(FLAGS[k]));
    return v;
}

static const char *g_dir;

static void remove_dir(void) {
    if (g_dir) avp_tempdir_cleanup(g_dir);
}

/* The test's own folder: build/tmp/permute_test_<pid>, removed at exit. */
static const char *work_dir(void) {
    if (!g_dir) {
        g_dir = avp_sprintf(avp_arena(), "%s\\build\\tmp\\permute_test_%u", avp_root(), avp_getpid());
        AVP_ASSERT(avp_makedirs(g_dir, true));
        atexit(remove_dir);
    }
    return g_dir;
}

static const char *path_in(const char *name) { return avp_sprintf(avp_arena(), "%s\\%s", work_dir(), name); }

static const char *write_file(const char *name, AvpStr text) {
    const char *p = path_in(name);
    AVP_ASSERT(avp_write_text(p, text, AVP_TM_UTF8));
    return p;
}

static bool have_real_tools(void) {
    AvpArena *a = avp_arena();
    return avp_exists(avp_sprintf(a, "%s\\tools\\compilers\\Wii\\1.0\\mwcceppc.exe", avp_root())) &&
           avp_exists(avp_sprintf(a, "%s\\tools\\bin\\dtk.exe", avp_root()));
}

static const char *cc_path(void) {
    return avp_sprintf(avp_arena(), "%s\\tools\\compilers\\Wii\\1.0\\mwcceppc.exe", avp_root());
}

static AvpProcResult run_cc(const AvpStrVec *flags, const char *const *tail, size_t ntail) {
    AvpArena *a = avp_arena();
    const char **cmd = avp_alloc(a, (flags->n + ntail + 2) * sizeof *cmd);
    size_t n = 0;
    cmd[n++] = cc_path();
    for (size_t k = 0; k < flags->n; k++) cmd[n++] = avp_cz(a, flags->v[k]);
    for (size_t k = 0; k < ntail; k++) cmd[n++] = tail[k];
    cmd[n] = NULL;
    return avp_run(&(AvpProcSpec){.argv = cmd, .cwd = work_dir(), .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                  .timeout = 600, .arena = a});
}

/* compile(path, flags): the object's functions (compile_object + load_compiled). */
static AvpMap *compile(const char *path, const AvpStrVec *flags) {
    AvpArena *a = avp_arena();
    const char *obj = avp_sprintf(a, "%s.o", path);
    const char *tail[] = {"-c", path, "-o", obj};
    AvpProcResult r = run_cc(flags, tail, 4);
    CHECK_MSG(r.status == AVP_PROC_OK && r.returncode == 0, "compile %s: %.*s%.*s", path, AVP_SF(r.out), AVP_SF(r.err));
    AvpStr err = {0};
    AvpMap *m = r.returncode == 0 ? cc_load_object(a, obj, NULL, &err) : NULL;
    CHECK_MSG(m != NULL || r.returncode != 0, "load %s: %.*s", obj, AVP_SF(err));
    return m;
}

/* base(name, keep): the preprocessed unit with the other functions stripped, written as name. */
static const char *make_base(const char *name, const char *sym) {
    AvpArena *a = avp_arena();
    AvpStrVec flags = flags_vec(false);
    const char *src = path_in("unit.cpp");
    const char *tail[] = {"-E", "-c", src};
    AvpProcResult r = run_cc(&flags, tail, 3);
    AVP_ASSERT(r.returncode == 0);
    /* re.sub(r"^#(?!pragma).*$", "", text, flags=re.M); re.sub(r"\n{3,}", "\n\n", text) */
    AvpStrVec lines = avp_str_split(r.out, AVP_S("\n"), -1);
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t blank = 0;
    for (size_t k = 0; k < lines.n; k++) {
        AvpStr l = lines.v[k];
        if (l.n && l.p[0] == '#' && !avp_str_startswith(l, AVP_S("#pragma"))) l = (AvpStr){0};
        if (k) avp_buf_putc(&b, '\n');
        avp_buf_put(&b, l);
    }
    AvpStr text = avp_buf_str(&b), squeezed;
    AvpBuf q;
    avp_buf_init(&q, a);
    for (size_t i = 0; i < text.n; i++) {
        blank = text.p[i] == '\n' ? blank + 1 : 0;
        if (blank <= 2) avp_buf_putc(&q, text.p[i]);
    }
    squeezed = avp_buf_str(&q);
    AvpStr keep;
    AVP_ASSERT(permute_source_name(a, avp_str(sym), &keep));
    size_t kept = 0;
    AvpStrVec callees = {0};
    AvpStr body = permute_strip_other_fns_cxx(a, squeezed, &keep, 1, true, &kept, &callees);
    CHECK(kept > 0);
    return write_file(name, body);
}

static void write_unit(void) { write_file("unit.cpp", avp_str(UNIT)); }

static const CcFn *fn_of(const AvpMap *m, const char *name) {
    AvpVal *v = m ? avp_map_get(m, avp_str(name)) : NULL;
    return v ? v->p : NULL;
}

AVP_TEST(unit_has_the_mangled_names) {
    if (!have_real_tools()) return;
    write_unit();
    AvpStrVec flags = flags_vec(false);
    AvpStrVec cflags = permute_compile_flags(&flags, true);
    AvpMap *full = compile(path_in("unit.cpp"), &cflags);
    CHECK(fn_of(full, ADD) != NULL);
    CHECK(fn_of(full, TWICE) != NULL);
}

static bool same_norms(const CcFn *x, const CcFn *y) {
    if (!x || !y || x->nrows != y->nrows) return false;
    for (size_t k = 0; k < x->nrows; k++)
        if (!avp_str_eq(x->rows[k].norm, y->rows[k].norm)) return false;
    return true;
}

AVP_TEST(base_compiles_as_cxx_and_locates_the_member_by_mangled_name) {
    if (!have_real_tools()) return;
    write_unit();
    AvpStrVec flags = flags_vec(false), cflags = permute_compile_flags(&flags, true);
    const char *base = make_base("base.cpp", ADD);
    AvpMap *compiled = compile(base, &cflags);
    CHECK(fn_of(compiled, ADD) != NULL);
    CHECK(fn_of(compiled, TWICE) == NULL); /* stripped: its class declares it, no body here */
    /* the stripped base gives the function the code the whole unit gives it (callees stay for inlining) */
    AvpMap *full = compile(path_in("unit.cpp"), &cflags);
    CHECK(same_norms(fn_of(full, ADD), fn_of(compiled, ADD)));
}

/* score_symbol(retail_fns, compiled_fns, sym): a result, or *error. */
static CcFnResult *score_symbol(const AvpMap *retail, const AvpMap *compiled, const char *sym, AvpStr *error) {
    AvpArena *a = avp_arena();
    const CcFn *c = fn_of(compiled, sym), *r = fn_of(retail, sym);
    if (!c) {
        AvpStrVec names = avp_map_keys(compiled);
        avp_sort_strs(names.v, names.n);
        *error = avp_str(avp_sprintf(a, "symbol %s not in the compiled object (it has: %s)", sym,
                                     avp_cz(a, avp_str_join(a, AVP_S(", "), names.v, names.n < 8 ? names.n : 8))));
        return NULL;
    }
    if (!r) {
        *error = avp_str(avp_sprintf(a, "symbol %s not in the retail asm", sym));
        return NULL;
    }
    return cc_compare_function(a, r, (CcFn *)c, NULL, false);
}

AVP_TEST(score_symbol_exact_and_not_exact) {
    if (!have_real_tools()) return;
    AvpArena *a = avp_arena();
    write_unit();
    AvpStrVec flags = flags_vec(false), cflags = permute_compile_flags(&flags, true);
    AvpMap *full = compile(path_in("unit.cpp"), &cflags);
    AvpMap *base = compile(make_base("base2.cpp", TWICE), &cflags);
    AvpStr error = {0};
    CcFnResult *res = score_symbol(full, base, TWICE, &error);
    REQUIRE(res != NULL);
    CHECK(cc_fn_result_exact(res));
    res = score_symbol(full, base, ADD, &error); /* Add is stripped from this base */
    CHECK(res == NULL);
    CHECK(has(error, ADD));

    const char *edited = make_base("base3.cpp", TWICE);
    AvpStr text;
    REQUIRE(avp_read_text(a, edited, AVP_TM_UTF8, &text));
    AVP_ASSERT(avp_write_text(edited, avp_str_replace(a, text, AVP_S("Helper(n) * 2 + Get()"),
                                                      AVP_S("Helper(n) * 3 + Get()"), -1), AVP_TM_UTF8));
    res = score_symbol(full, compile(edited, &cflags), TWICE, &error);
    REQUIRE(res != NULL);
    CHECK(!cc_fn_result_exact(res));
}

AVP_TEST(compile_flags_make_any_file_name_compile_as_cxx) {
    if (!have_real_tools()) return;
    AvpArena *a = avp_arena();
    write_unit();
    /* the permuter hands compile.sh an input.c: -lang must carry the language, not the extension */
    AvpStrVec flags = flags_vec(true), cx = permute_compile_flags(&flags, true);
    REQUIRE(cx.n >= 2);
    CHECK_STR(cx.v[cx.n - 2], "-lang");
    CHECK_STR(cx.v[cx.n - 1], "c++");
    AvpStrVec all = flags_vec(false), same = permute_compile_flags(&all, true);
    REQUIRE(same.n == all.n);
    for (size_t k = 0; k < all.n; k++) CHECK(avp_str_eq(same.v[k], all.v[k]));
    AvpStrVec c = permute_compile_flags(&all, false);
    for (size_t k = 0; k < c.n; k++) CHECK(!avp_str_startswith(c.v[k], AVP_S("-lang")));
    AvpStr text;
    REQUIRE(avp_read_text(a, make_base("base4.cpp", ADD), AVP_TM_UTF8, &text));
    const char *as_c = write_file("input.c", text);
    CHECK(fn_of(compile(as_c, &cx), ADD) != NULL);
}

/* ---- Settings ---------------------------------------------------------------------------------------------------- */

AVP_TEST(objdump_path_with_spaces) {
    /* The Python parses the TOML and shlex.splits objdump_command. C has no TOML reader here: the text is compared
     * (the value is a basic string; the path's spaces are protected by shlex.quote's single quotes). */
    const char *exe = "X:\\Game Project Checkout Dir\\build\\binutils\\powerpc-eabi-objdump.exe";
    AvpStr s = permute_settings_toml(avp_arena(), AVP_S("fn_80001234"), exe);
    CHECK_STR(s, "func_name = \"fn_80001234\"\n"
                 "compiler_type = \"mwcc\"\n"
                 "objdump_command = \"'X:/Game Project Checkout Dir/build/binutils/powerpc-eabi-objdump.exe' "
                 "-drz -m powerpc -M broadway\"\n");
}

/* ---- Args (build/tools/permute.exe) ------------------------------------------------------------------------------ */

static AvpProcResult run_permute(const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **cmd = avp_alloc(a, (n + 2) * sizeof *cmd);
    cmd[0] = avp_sprintf(a, "%s\\build\\tools\\permute.exe", avp_root());
    for (size_t k = 0; k < n; k++) cmd[k + 1] = args[k];
    cmd[n + 1] = NULL;
    return avp_run(&(AvpProcSpec){.argv = cmd, .cwd = avp_root(), .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                  .timeout = 120, .arena = a});
}

#define RUN(...) run_permute((const char *const[]){__VA_ARGS__}, N(((const char *const[]){__VA_ARGS__})))

static bool have_permute_exe(void) {
    return avp_exists(avp_sprintf(avp_arena(), "%s\\build\\tools\\permute.exe", avp_root()));
}

AVP_TEST(documented_command_lines) {
    if (!have_permute_exe()) return;
    /* Each line parses (not exit 2) and reaches the unit lookup, which fails for this unit. */
    AvpProcResult r = RUN("nosuch/unit", "fn", "--run", "-j", "8", "--stop-on-zero", "--lang", "c++", "--root", ".",
                          "--out", "O", "--keep-all");
    CHECK_EQ_I64(r.returncode, 1);
    CHECK(has(r.err, "unit not found in build.ninja: nosuch/unit"));
    r = RUN("nosuch/unit", "fn", "--score", "--file", "F", "--lang", "c", "--no-check");
    CHECK_EQ_I64(r.returncode, 1);
    CHECK(has(r.err, "unit not found in build.ninja: nosuch/unit"));
    r = RUN("nosuch/unit", "fn", "--run", "--", "--best-only", "--seed", "3");
    CHECK_EQ_I64(r.returncode, 1);
    CHECK(has(r.err, "unit not found in build.ninja: nosuch/unit"));
}

AVP_TEST(unknown_flag_exits_2) {
    if (!have_permute_exe()) return;
    CHECK_EQ_I64(RUN("u", "fn", "--frobnicate").returncode, 2);
    CHECK_EQ_I64(RUN("u", "fn", "--lang", "rust").returncode, 2);
    CHECK_EQ_I64(RUN("u", "fn", "-j", "many").returncode, 2);
    CHECK_EQ_I64(RUN("u").returncode, 2);
}

AVP_TEST(help_exits_0) {
    if (!have_permute_exe()) return;
    AvpProcResult r = RUN("--help");
    CHECK_EQ_I64(r.returncode, 0);
    CHECK(has(r.out, "usage: permute.exe"));
}

/* ---- MainFlow (the --run requirements) --------------------------------------------------------------------------- */

/* The Python points ROOT and REPO at an empty folder; here the checkout stays the REPO (tool_path falls back to it). */

static bool listed(const AvpStrVec *v, const char *needle) {
    for (size_t k = 0; k < v->n; k++)
        if (has(v->v[k], needle)) return true;
    return false;
}

AVP_TEST(run_names_each_missing_requirement) {
    /* The Python points ROOT and REPO at an empty folder, so all three are missing. permute_run_requirements looks
     * for the permuter and the venv through the checkout (permute_repo, or --root when set inside permute_main), so
     * here they are missing only where the real tree lacks them; build/binutils is looked up under the root given. */
    AvpArena *a = avp_arena();
    const char *root = work_dir();
    AvpStrVec m = permute_run_requirements(a, root, false);
    CHECK(listed(&m, "build/binutils is missing"));
    CHECK(listed(&m, avp_sprintf(a, "%s\\build\\binutils", root)));
    bool permuter = avp_exists(avp_sprintf(a, "%s\\tools\\permuter\\permuter.py", permute_repo()));
    CHECK(listed(&m, "tools/permuter is missing") == !permuter);
    bool venv = avp_exists(avp_sprintf(a, "%s\\tools\\venv\\Scripts\\python.exe", permute_repo())) ||
                avp_exists(avp_sprintf(a, "%s\\tools\\venv\\bin\\python", permute_repo()));
    CHECK(listed(&m, "tools/venv is missing") == !venv);
}

AVP_TEST(run_names_only_what_is_missing) {
    AvpArena *a = avp_arena();
    AvpStrVec m = permute_run_requirements(a, work_dir(), true);
    CHECK(!listed(&m, "build/binutils is missing"));
    m = permute_run_requirements(a, work_dir(), false);
    CHECK(listed(&m, "build/binutils is missing"));
}

AVP_TEST_MAIN("permute")
