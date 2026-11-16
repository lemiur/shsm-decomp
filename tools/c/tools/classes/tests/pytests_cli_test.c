/* pytests_cli_test.c: the Python unit tests of tools/test_classes.py that run the command line (CliTest and
 * test_output_does_not_depend_on_hash_seed), one C test per Python test with the same name. Where the Python calls
 * C.main(...) in process, this runs build/tools/classes.exe (the sibling of the tests folder) through avp_run, with
 * stdout and stderr captured apart, on a synthetic asm tree (build_rtti_texts) in a private temporary directory
 * and a private SHSM_CACHE_DIR, so the tests are parallel-safe and never touch build/R5WEA4 or config.
 *
 * Differences from the Python tests:
 *  - test_show: `show NoSuchClass` is a SystemExit whose message is the code in Python (`"class not found" in
 *    str(code)`); the exe exits 1 and prints the message on stderr, which is what is checked.
 *  - test_scan_records_input_stamp / test_scan_skip_and_cache / test_regions_from_splits: C.stamp_key,
 *    C.input_stamp, C.stale_reason and C.parse_data_regions are classes_stamp_key(_eq), classes_input_stamp,
 *    classes_stale_reason and classes_parse_data_regions; Analysis.region_of is not exported by the C library, so
 *    region_of below repeats its three lines over classes_region_of_library (the loop of the C stats code).
 *  - test_output_does_not_depend_on_hash_seed: the C tool has no hash seed; the Python's PYTHONHASHSEED is still set
 *    in the child's environment, and the four --no-cache scans must write identical bytes. */
#include "avp/test.h"
#include "classes.h"
#include "classes_prog.h"

#include <string.h>

static AvpStr S(const char *z) { return avp_str(z); }
#define SP(...) avp_sprintf(avp_arena(), __VA_ARGS__)

/* ---- the synthetic asm tree: build_rtti_texts, written as asm/revolution/hbm/lyt.s ----------------------------- */

#define PANE "Q36nw4hbm3lyt4Pane"
#define BND "Q36nw4hbm3lyt8Bounding"
#define COLOR "Q36nw4hbm2ut5Color"

static i64 g_addr = 0x80010000;

static void fn_begin(AvpBuf *b, const char *name) { avp_buf_printf(b, ".fn %s, global\n", name); }
static void I(AvpBuf *b, const char *ins) {
    unsigned a = (unsigned)g_addr;
    g_addr += 4;
    avp_buf_printf(b, "/* %08X %08X  00 00 00 00 */\t%s\n", a, a - 0x80000000u, ins);
}
static void fn_end(AvpBuf *b, const char *name) { avp_buf_printf(b, ".endfn %s\n\n", name); }
static void lis_addi(AvpBuf *b, const char *reg, const char *sym) {
    I(b, SP("lis %s, %s@ha", reg, sym));
    I(b, SP("addi %s, %s, %s@l", reg, reg, sym));
}
static void obj_n(AvpBuf *b, const char *name, unsigned addr, const char *const *w, size_t n) {
    avp_buf_printf(b, "# .data:0x0 | 0x%08X | size: 0x%X\n.obj %s, global\n", addr, (unsigned)(4 * n), name);
    for (size_t i = 0; i < n; i++) {
        if (w[i][0] == '0' && !w[i][1]) avp_buf_putz(b, "\t.4byte 0x00000000\n");
        else avp_buf_printf(b, "\t.4byte %s\n", w[i]);
    }
    avp_buf_printf(b, ".endobj %s\n\n", name);
}
#define OBJ(b, name, addr, ...)                                                  \
    do {                                                                         \
        const char *w_[] = {__VA_ARGS__};                                        \
        obj_n((b), (name), (addr), w_, sizeof w_ / sizeof *w_);                  \
    } while (0)

static AvpStr rtti_text(void) {
    AvpBuf t;
    avp_buf_init(&t, avp_arena());
    avp_buf_putz(&t, ".text\n");
    fn_begin(&t, "__ct__" PANE "Fv");
    lis_addi(&t, "r4", "__vt__" PANE);
    I(&t, "stw r4, 0x0(r3)"); I(&t, "blr");
    fn_end(&t, "__ct__" PANE "Fv");
    fn_begin(&t, "__dt__" PANE "Fv");
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r30, r3");
    lis_addi(&t, "r4", "__vt__" PANE);
    I(&t, "stw r4, 0x0(r30)"); I(&t, "bl __dl__FPv"); I(&t, "mr r3, r30"); I(&t, "blr");
    fn_end(&t, "__dt__" PANE "Fv");
    fn_begin(&t, "GetRuntimeTypeInfo__" PANE "CFv");
    lis_addi(&t, "r3", "__RTTI__" PANE);
    I(&t, "blr");
    fn_end(&t, "GetRuntimeTypeInfo__" PANE "CFv");
    fn_begin(&t, "__ct__" BND "Fv");
    I(&t, "mflr r0"); I(&t, "mr r31, r3"); I(&t, "bl __ct__" PANE "Fv");
    lis_addi(&t, "r4", "__vt__" BND);
    I(&t, "stw r4, 0x0(r31)"); I(&t, "addi r3, r31, 0x10"); I(&t, "bl __ct__" COLOR "Fv");
    I(&t, "mr r3, r31"); I(&t, "blr");
    fn_end(&t, "__ct__" BND "Fv");
    fn_begin(&t, "__dt__" BND "Fv");
    I(&t, "cmpwi r3, 0x0"); I(&t, "mr r30, r3");
    lis_addi(&t, "r4", "__vt__" BND);
    I(&t, "stw r4, 0x0(r30)"); I(&t, "addi r3, r30, 0x10"); I(&t, "li r4, 0x0"); I(&t, "bl __dt__" COLOR "Fv");
    I(&t, "mr r3, r30"); I(&t, "li r4, 0x0"); I(&t, "bl __dt__" PANE "Fv"); I(&t, "bl __dl__FPv"); I(&t, "blr");
    fn_end(&t, "__dt__" BND "Fv");
    fn_begin(&t, "GetRuntimeTypeInfo__" BND "CFv");
    lis_addi(&t, "r3", "__RTTI__" BND);
    I(&t, "blr");
    fn_end(&t, "GetRuntimeTypeInfo__" BND "CFv");
    avp_buf_putz(&t, ".data\n");
    OBJ(&t, "__vt__" PANE, 0x80320000, "__RTTI__" PANE, "0", "__dt__" PANE "Fv", "GetRuntimeTypeInfo__" PANE "CFv");
    OBJ(&t, "__RTTI__" PANE, 0x80320018, "0", "0");
    OBJ(&t, "__vt__" BND, 0x80320020, "__RTTI__" BND, "0", "__dt__" BND "Fv", "GetRuntimeTypeInfo__" BND "CFv");
    OBJ(&t, "__RTTI__" BND, 0x80320030, "0", "0");
    return avp_buf_str(&t);
}

/* ---- files and processes --------------------------------------------------------------------------------------- */

static void put(const char *path, AvpStr text) { AVP_ASSERT(avp_write_file(path, text.p, text.n)); }
static void putz(const char *path, const char *text) { put(path, S(text)); }
static AvpStr slurp(const char *path) {
    AvpBytes b = {0};
    AVP_ASSERT(avp_read_all(avp_arena(), path, &b));
    return (AvpStr){(const char *)b.v, b.n};
}
static void append(const char *path, const char *text) {
    put(path, avp_str_concat(avp_arena(), slurp(path), S(text)));
}

typedef struct {
    char *dir, *asm_dir, *lyt, *syms, *splits, *out, *cache, *exe;
} Ctx;

/* CliTest.setUp */
static Ctx setup(void) {
    AvpArena *a = avp_arena();
    Ctx c;
    c.dir = avp_mkdtemp(a, "classes_pytests", NULL, NULL);
    AVP_ASSERT(c.dir != NULL);
    c.asm_dir = avp_path_join(a, c.dir, "asm");
    AVP_ASSERT(avp_makedirs(avp_join(a, c.asm_dir, "revolution", "hbm"), true));
    c.lyt = avp_join(a, c.asm_dir, "revolution", "hbm", "lyt.s");
    put(c.lyt, rtti_text());
    c.syms = avp_path_join(a, c.dir, "symbols.txt");
    putz(c.syms, "");
    c.splits = avp_path_join(a, c.dir, "splits.txt");
    putz(c.splits, "Sections:\n\t.data       type:data align:32\n\n"
                   "game/A.cpp:\n\t.text       start:0x80006000 end:0x80006010\n"
                   "\t.data       start:0x80300000 end:0x80300010\n\n"
                   "revolution/hbm/lyt.cpp:\n\t.data       start:0x80320020 end:0x80320038\n\n"
                   "climax/B.cpp:\n\t.data       start:0x80330000 end:0x80330010\n");
    c.out = avp_path_join(a, c.dir, "classes.json");
    c.cache = avp_path_join(a, c.dir, "cache");
    c.exe = avp_path_abspath(a, avp_path_join(a, avp_exe_dir(), "..\\classes.exe"));
    AVP_ASSERT(avp_exists(c.exe));
    AVP_ASSERT(avp_setenv("SHSM_CACHE_DIR", c.cache)); /* a private cache, never the machine-wide one */
    avp_unsetenv("SHSM_CLASSES_CACHE");
    return c;
}
static void teardown(Ctx *c) { avp_tempdir_cleanup(c->dir); }

typedef struct {
    i64 code;
    AvpStr out, err;
} Res;


/* CliTest.call(*args, asm=, syms=): the exe with the four path options and the given arguments (NULL-ended). */
static Res run_with(const Ctx *c, const char *asm_dir, const char *syms, const char *const *args, size_t nargs) {
    AvpArena *a = avp_arena();
    const char *argv[32];
    size_t n = 0;
    argv[n++] = c->exe;
    argv[n++] = "--asm"; argv[n++] = asm_dir;
    argv[n++] = "--symbols"; argv[n++] = syms;
    argv[n++] = "--out"; argv[n++] = c->out;
    argv[n++] = "--splits"; argv[n++] = c->splits;
    for (size_t i = 0; i < nargs; i++) argv[n++] = args[i];
    argv[n] = NULL;
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE, .text = true,
                                             .timeout = 300, .arena = a});
    if (r.status != AVP_PROC_OK) {
        avp_test__fail(__FILE__, __LINE__, "classes.exe did not run");
        return (Res){-1, S(""), S("")};
    }
    return (Res){r.returncode, r.out, r.err};
}
#define CALL(c, ...)                                                             \
    call_((c), (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))
static Res call_(const Ctx *c, const char *const *args, size_t n) { return run_with(c, c->asm_dir, c->syms, args, n); }

static AvpJson *parse(AvpStr text) {
    AvpJsonError e;
    AvpJson *j = avp_json_parse(avp_arena(), text, &e);
    CHECK_MSG(j != NULL, "not JSON: %.*s", AVP_SF(text));
    return j;
}
static AvpJson *scan(const Ctx *c) {
    Res r = CALL(c, "scan", "--json");
    CHECK_MSG(r.code == 0, "scan exit %lld: %.*s", (long long)r.code, AVP_SF(r.err));
    CHECK_MSG(r.err.n == 0, "scan stderr: %.*s", AVP_SF(r.err));
    return parse(r.out);
}
static bool has(AvpStr s, const char *sub) { return avp_str_contains(s, S(sub)); }

/* scan_how: ("kept" | "restored" | "scanned", the bytes of classes.json) from the last stdout line */
static const char *scan_how(const Ctx *c, const char *extra, AvpStr *data) {
    Res r = extra ? CALL(c, "scan", extra) : CALL(c, "scan");
    CHECK_MSG(r.code == 0, "scan exit %lld: %.*s", (long long)r.code, AVP_SF(r.err));
    if (data) *data = slurp(c->out);
    AvpStrVec lines = avp_str_splitlines(avp_str_strip(r.out), false);
    AvpStr last = lines.n ? lines.v[lines.n - 1] : S("");
    return has(last, "kept") ? "kept" : has(last, "restored from the cache") ? "restored" : "scanned";
}

/* json.loads(data) without "inputs", as compact text */
static AvpStr without_inputs(AvpStr data) {
    AvpJson *db = parse(data);
    avp_json_del(db, S("inputs"));
    return avp_json_dumps(avp_arena(), db, &AVP_JSON_COMPACT);
}

/* touch_asm: the one asm file's mtime moves two seconds on (optionally appending text first) */
static void touch_asm(const Ctx *c, const char *text) {
    if (text) append(c->lyt, text);
    AvpStat st;
    AVP_ASSERT(avp_stat(c->lyt, &st));
    AVP_ASSERT(avp_utime_ns(c->lyt, st.atime_ns, st.mtime_ns + 2000000000));
}

/* ---- tests ----------------------------------------------------------------------------------------------------- */

AVP_TEST(test_output_does_not_depend_on_hash_seed) {
    AvpArena *a = avp_arena();
    Ctx c = setup();
    AvpStr first = {0};
    for (int seed = 1; seed <= 4; seed++) {
        AVP_ASSERT(avp_setenv("PYTHONHASHSEED", SP("%d", seed)));
        char *out = avp_path_join(a, c.dir, SP("classes%d.json", seed));
        const char *argv[] = {c.exe, "--asm", c.asm_dir, "--symbols", c.syms, "--out", out, "scan", "--no-cache", NULL};
        AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                                 .timeout = 300, .arena = a, .check = true});
        REQUIRE(r.status == AVP_PROC_OK);
        AvpStr data = slurp(out);
        if (seed == 1) first = data;
        else CHECK_MSG(avp_str_eq(data, first), "seed %d wrote other bytes", seed);
    }
    CHECK(first.n > 0);
    teardown(&c);
}

AVP_TEST(test_scan_records_input_stamp) {
    AvpArena *a = avp_arena();
    Ctx c = setup();
    scan(&c);
    AvpJson *db = parse(slurp(c.out));
    AvpJson *inputs = avp_json_getz(db, "inputs");
    CHECK_EQ_I64(avp_json_get_i64(avp_json_getz(inputs, "asm"), "files", -1), 1);
    CHECK_EQ_I64(avp_json_get_i64(avp_json_getz(inputs, "symbols"), "bytes", -1), 0);
    ClassesStampKey k1, k2;
    AvpJson *now = classes_input_stamp(a, c.asm_dir, c.syms);
    REQUIRE(classes_stamp_key(inputs, &k1) && classes_stamp_key(now, &k2));
    CHECK(classes_stamp_key_eq(&k1, &k2));
    teardown(&c);
}

AVP_TEST(test_scan_skip_and_cache) {
    AvpArena *a = avp_arena();
    Ctx c = setup();
    AvpStr first = {0}, data = {0}, plain = {0};
    CHECK_CZ(scan_how(&c, NULL, &first), "scanned");
    /* unchanged stamp and scanner digest: classes.json is current, nothing is rescanned */
    CHECK_CZ(scan_how(&c, NULL, &data), "kept");
    CHECK(avp_str_eq(data, first));
    /* a newer mtime with the same bytes: the stamp is stale, the content key hits */
    touch_asm(&c, NULL);
    CHECK_CZ(scan_how(&c, NULL, &data), "restored");
    CHECK(avp_str_eq(without_inputs(data), without_inputs(first)));
    ClassesStampKey k1, k2;
    REQUIRE(classes_stamp_key(avp_json_getz(parse(data), "inputs"), &k1));
    REQUIRE(classes_stamp_key(classes_input_stamp(a, c.asm_dir, c.syms), &k2));
    CHECK(classes_stamp_key_eq(&k1, &k2));
    ClassesDb *db = classes_db_from_json(a, parse(data));
    CHECK(classes_stale_reason(a, db, c.asm_dir, c.syms) == NULL);
    /* changed bytes: a real scan, and the same result as an uncached scan */
    touch_asm(&c, "\n# a comment line\n");
    CHECK_CZ(scan_how(&c, NULL, &data), "scanned");
    CHECK_CZ(scan_how(&c, "--no-cache", &plain), "scanned");
    CHECK(avp_str_eq(without_inputs(data), without_inputs(plain)));
    /* a changed splits file changes the scanner digest: not kept */
    append(c.splits, "\n");
    CHECK(strcmp(scan_how(&c, NULL, NULL), "kept") != 0);
    /* a damaged cache entry is a miss */
    AvpWalk *w = avp_walk_open(c.cache, true);
    REQUIRE(w != NULL);
    AvpWalkItem it;
    while (avp_walk_next(w, &it)) {
        for (size_t i = 0; i < it.files.n; i++) {
            const char *f = avp_path_join(a, avp_cz(a, it.dirpath), avp_cz(a, it.files.v[i]));
            AvpStr old = slurp(f);
            size_t n = old.n > 7 ? old.n : 7;
            char *buf = avp_alloc(a, n);
            memcpy(buf, old.p, old.n);
            memcpy(buf, "garbage", 7);
            AVP_ASSERT(avp_write_file(f, buf, n));
        }
    }
    avp_walk_close(w);
    touch_asm(&c, NULL);
    CHECK_CZ(scan_how(&c, NULL, NULL), "scanned");
    /* the bypasses always scan */
    CHECK_CZ(scan_how(&c, "--no-cache", NULL), "scanned");
    AVP_ASSERT(avp_setenv("SHSM_CLASSES_CACHE", "0"));
    CHECK_CZ(scan_how(&c, NULL, NULL), "scanned");
    avp_unsetenv("SHSM_CLASSES_CACHE");
    teardown(&c);
}

AVP_TEST(test_missing_inputs_exit_2) {
    AvpArena *a = avp_arena();
    Ctx c = setup();
    const char *const scan_arg[] = {"scan"};
    Res r = run_with(&c, avp_path_join(a, c.asm_dir, "none"), c.syms, scan_arg, 1);
    CHECK_EQ_I64(r.code, 2);
    CHECK(has(r.err, "asm directory not found"));
    r = run_with(&c, c.asm_dir, SP("%s.none", c.syms), scan_arg, 1);
    CHECK_EQ_I64(r.code, 2);
    CHECK(has(r.err, "symbols file not found"));
    char *empty = avp_path_join(a, c.dir, "empty");
    AVP_ASSERT(avp_makedirs(empty, true));
    r = run_with(&c, empty, c.syms, scan_arg, 1);
    CHECK_EQ_I64(r.code, 2);
    CHECK(has(r.err, "no .s files"));
    CHECK(!avp_exists(c.out));
    teardown(&c);
}

AVP_TEST(test_show) {
    Ctx c = setup();
    scan(&c);
    Res r = CALL(&c, "show", "nw4hbm::lyt::Bounding");
    CHECK_EQ_I64(r.code, 0);
    CHECK(has(r.out, "nw4hbm::lyt::Pane"));
    CHECK_EQ_U64(r.err.n, 0);
    r = CALL(&c, "show", "__vt__Q36nw4hbm3lyt8Bounding", "--json");
    CHECK_STR(avp_json_get_str(parse(r.out), "name", S("")), "nw4hbm::lyt::Bounding");
    r = CALL(&c, "show", "NoSuchClass");
    CHECK(r.code != 0);
    CHECK(has(r.err, "class not found"));
    teardown(&c);
}

AVP_TEST(test_tree) {
    Ctx c = setup();
    scan(&c);
    Res r = CALL(&c, "tree");
    CHECK_EQ_I64(r.code, 0);
    AvpStrVec lines = avp_str_splitlines(r.out, false);
    size_t i = 0;
    while (i < lines.n && !has(lines.v[i], "nw4hbm::lyt::Pane")) i++;
    REQUIRE(i + 1 < lines.n);
    CHECK(has(lines.v[i + 1], "nw4hbm::lyt::Bounding"));
    CHECK(lines.v[i + 1].n > 0 && lines.v[i + 1].p[0] == ' ');
    r = CALL(&c, "tree", "nw4hbm::lyt::Pane", "--json");
    CHECK_EQ_U64(avp_json_len(parse(r.out)), 1);
    teardown(&c);
}

AVP_TEST(test_header) {
    Ctx c = setup();
    scan(&c);
    Res r = CALL(&c, "header", "nw4hbm::lyt::Bounding");
    CHECK_EQ_I64(r.code, 0);
    CHECK(has(r.out, "class"));
    CHECK(has(r.out, "Bounding"));
    CHECK(has(r.out, "Pane"));
    teardown(&c);
}

AVP_TEST(test_stale_warning_and_strict) {
    AvpArena *a = avp_arena();
    Ctx c = setup();
    scan(&c);
    Res r = CALL(&c, "show", "nw4hbm::lyt::Pane");
    CHECK_EQ_U64(r.err.n, 0);
    putz(c.syms, "fn_X = .text:0x80001000; // type:function size:0x4\n");
    r = CALL(&c, "show", "nw4hbm::lyt::Pane");
    CHECK_EQ_I64(r.code, 0);
    CHECK(has(r.err, "is stale"));
    CHECK(has(r.err, "symbols file"));
    CHECK(has(r.err, "build/tools/classes.exe scan"));
    r = CALL(&c, "--strict", "tree");
    CHECK_EQ_I64(r.code, 1);
    CHECK_EQ_U64(r.out.n, 0);
    /* a file without a stamp is stale too */
    AvpJson *db = parse(slurp(c.out));
    CHECK(avp_json_del(db, S("inputs")));
    put(c.out, avp_json_dumps(a, db, &AVP_JSON_COMPACT));
    r = CALL(&c, "header", "nw4hbm::lyt::Pane");
    CHECK(has(r.err, "no input stamp"));
    teardown(&c);
}

/* Analysis.region_of(a, regions) */
static const char *region_of(i64 addr, const ClassesRegions *regions) {
    AvpStr lib = {0};
    for (size_t k = 0; k < regions->n; k++) {
        if (regions->v[k].start > addr) break;
        lib = regions->v[k].library;
    }
    AvpStr r = CLASSES_NONE(lib) ? (AvpStr){0} : classes_region_of_library(lib);
    return CLASSES_NONE(r) ? "other" : avp_cz(avp_arena(), r);
}

AVP_TEST(test_regions_from_splits) {
    AvpArena *a = avp_arena();
    Ctx c = setup();
    AvpStr text = {0};
    AVP_ASSERT(avp_read_text(a, c.splits, AVP_TM_UTF8, &text));
    ClassesRegions regions = classes_parse_data_regions(a, text);
    REQUIRE(regions.n == 3);
    const i64 st[3] = {0x80300000, 0x80320020, 0x80330000}, en[3] = {0x80300010, 0x80320038, 0x80330010};
    const char *lib[3] = {"game", "revolution", "climax"};
    for (int i = 0; i < 3; i++) {
        CHECK_EQ_I64(regions.v[i].start, st[i]);
        CHECK_EQ_I64(regions.v[i].end, en[i]);
        CHECK_STR(regions.v[i].library, lib[i]);
    }
    CHECK_CZ(region_of(0x80300004, &regions), "game");
    CHECK_CZ(region_of(0x80310000, &regions), "game"); /* unsplit, after a game unit */
    CHECK_CZ(region_of(0x80320020, &regions), "other");
    CHECK_CZ(region_of(0x80340000, &regions), "climax-core");
    CHECK_CZ(region_of(0x80000000, &regions), "other");
    AvpJson *stats = scan(&c);
    /* Pane (0x80320000) is unsplit data after the game unit; Bounding sits in the hbm split */
    AvpJson *by = avp_json_getz(stats, "vtable_classes_by_region");
    REQUIRE(by != NULL && avp_json_len(by) == 2);
    AvpJson *g = avp_json_getz(by, "game"), *o = avp_json_getz(by, "other");
    REQUIRE(g && o);
    CHECK_EQ_I64(avp_json_get_i64(g, "named", -1), 1);
    CHECK_EQ_I64(avp_json_get_i64(g, "unk", -1), 0);
    CHECK_EQ_I64(avp_json_get_i64(o, "named", -1), 1);
    CHECK_EQ_I64(avp_json_get_i64(o, "unk", -1), 0);
    teardown(&c);
}

AVP_TEST_MAIN("classes_pytests_cli")
