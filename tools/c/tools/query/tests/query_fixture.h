/* The synthetic checkout of tools/test_query.py: tools/common/_fixture.py's checkout (configure.py, symbols, splits,
 * the Alpha/Beta listings, objdiff.json, build.ninja, report and baseline) extended with a vtable listing (Gamma.s),
 * source, a header, docs/exceptions.md and a retail DOL, written at run time into a private temporary folder that is
 * removed at exit. Plus runners for query.exe (stdout, stderr, exit code; --json parsed). */
#pragma once
#include "avp/fs.h"
#include "avp/proc.h"
#include "query.h"
#include "../../../tests/shsm_asm_fixture.h"
#include "../../../tests/shsm_test_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const char QF_CONFIGURE[] =
    "\n"
    "DEFAULT_VERSION = 0\n"
    "VERSIONS = [\n"
    "    \"TESTV1\",  # 0: test\n"
    "]\n"
    "config.libs = [\n"
    "    {\n"
    "        \"lib\": \"game\",\n"
    "        \"objects\": [\n"
    "            Object(Matching, \"game/Alpha.cpp\"),\n"
    "            Object(\n"
    "                NonMatching,\n"
    "                \"game/Beta.c\",\n"
    "                extra_cflags=[\"-O0\"],\n"
    "            ),\n"
    "            Object(Equivalent, \"game/Gamma.c\"),\n"
    "        ],\n"
    "    },\n"
    "]\n";

/* _fixture.SYMBOLS plus the vtable row test_query.py appends. */
static const char QF_SYMBOLS[] =
    "fn_80001000__Fv = .text:0x80001000; // type:function size:0x10 scope:global\n"
    "helper = .text:0x80001010; // type:function size:0x8 scope:local align:4\n"
    "fn_80001018__8unk_RampFf = .text:0x80001018; // type:function size:0xC\n"
    "jumper = .text:0x80001100; // type:function size:0x8 scope:global\n"
    "lbl_80002000 = .data:0x80002000; // type:object size:0x6 scope:global data:string\n"
    "lbl_80002008 = .data:0x80002008; // type:object size:0xC scope:global\n"
    "lbl_80003000 = .sdata2:0x80003000; // type:object size:0x4 scope:local data:float\n"
    "lbl_80003008 = .sdata2:0x80003008; // type:object size:0x8 scope:local data:double hidden\n"
    "start_label = .text:0x80001000; // type:label scope:global\n"
    "__vt__8unk_Ramp = .data:0x80002100; // type:object size:0xC scope:global\n";

static const char QF_SPLITS[] =
    "Sections:\n"
    "\t.text       type:code align:8\n"
    "\t.data       type:data align:32\n"
    "\n"
    "game/Alpha.cpp:\n"
    "\t.text       start:0x80001000 end:0x80001024\n"
    "\t.data       start:0x80002000 end:0x80002014\n"
    "\t.sdata2     start:0x80003000 end:0x80003010\n"
    "\n"
    "game/Beta.c:\n"
    "\t.text       start:0x80001100 end:0x80001108\n";

static const char QF_OBJDIFF[] =
    "{\"units\": ["
    "{\"name\": \"main/game/Alpha\", \"target_path\": \"build/TESTV1/obj/game/Alpha.o\", "
    "\"base_path\": \"build/TESTV1/src/game/Alpha.o\", "
    "\"metadata\": {\"complete\": true, \"source_path\": \"src/game/Alpha.cpp\", \"progress_categories\": [\"game\"], "
    "\"auto_generated\": false}}, "
    "{\"name\": \"main/game/Beta\", \"target_path\": \"build/TESTV1/obj/game/Beta.o\", "
    "\"base_path\": \"build/TESTV1/src/game/Beta.o\", "
    "\"metadata\": {\"complete\": false, \"source_path\": \"src/game/Beta.c\", \"progress_categories\": [\"game\"]}}, "
    "{\"name\": \"main/auto_01_80005000_text\", \"target_path\": \"build/TESTV1/obj/auto_01_80005000_text.o\", "
    "\"metadata\": {\"auto_generated\": true}}]}";

static const char QF_NINJA[] =
    "# Source files\n"
    "# game/Alpha.cpp: game (linked True)\n"
    "build build\\TESTV1\\src\\game\\Alpha.o: mwcc_sjis src\\game\\Alpha.cpp | $\n"
    "    tools\\compilers\\$mw_version\\mwcceppc.exe || pre-compile\n"
    "  mw_version = Wii\\1.0\n"
    "  cflags = -nodefaults -proc gekko -pragma \"cats off\" -O4,p $\n"
    "      -i include -lang=c++\n"
    "  basedir = build\\TESTV1\\src\\game\n"
    "build build\\TESTV1\\src\\game\\Alpha.ctx: decompctx src\\game\\Alpha.cpp\n"
    "\n"
    "# game/Beta.c: game (linked\n"
    "# False)\n"
    "build $\n"
    "    build\\TESTV1\\src\\game\\Beta.o: $\n"
    "    mwcc $\n"
    "    src\\game\\Beta.c\n"
    "  mw_version = GC\\3.0a5\n"
    "  cflags = -O4,p -O0\n";

/* _fixture.REPORT; the baseline differs only in jumper's percent (50.0). */
#define QF_REPORT_FMT                                                                                              \
    "{\"measures\": {\"fuzzy_match_percent\": 50.0, \"total_code\": \"36\", \"matched_code\": \"16\", "            \
    "\"total_functions\": 4, \"matched_functions\": 2}, "                                                          \
    "\"categories\": [{\"id\": \"game\", \"name\": \"Game Code\", \"measures\": {\"fuzzy_match_percent\": 50.0}}], " \
    "\"units\": ["                                                                                                 \
    "{\"name\": \"main/game/Alpha\", \"measures\": {\"fuzzy_match_percent\": 100.0, \"total_code\": \"36\"}, "    \
    "\"metadata\": {\"complete\": true, \"source_path\": \"src/game/Alpha.cpp\", "                                 \
    "\"progress_categories\": [\"game\"]}, "                                                                       \
    "\"sections\": [{\"name\": \".text\", \"size\": \"36\", \"fuzzy_match_percent\": 100.0, "                      \
    "\"metadata\": {\"virtual_address\": \"2147487744\"}}], "                                                      \
    "\"functions\": ["                                                                                             \
    "{\"name\": \"fn_80001000__Fv\", \"size\": \"16\", \"fuzzy_match_percent\": 100.0, "                           \
    "\"metadata\": {\"demangled_name\": \"fn_80001000()\", \"virtual_address\": \"2147487744\"}, \"address\": \"0\"}, " \
    "{\"name\": \"helper\", \"size\": \"8\", \"metadata\": {\"virtual_address\": \"2147487760\"}, \"address\": \"16\"}]}, " \
    "{\"name\": \"main/game/Beta\", \"measures\": {}, \"metadata\": {\"complete\": false}, "                       \
    "\"functions\": [{\"name\": \"jumper\", \"size\": \"8\", \"fuzzy_match_percent\": %s}]}]}"

static const char QF_GAMMA_S[] =
    ".include \"macros.inc\"\n"
    ".file \"Gamma.c\"\n"
    "\n"
    "# 0x80002100..0x80002110 | size: 0x10\n"
    ".data\n"
    ".balign 8\n"
    "\n"
    "# .data:0x0 | 0x80002100 | size: 0xC\n"
    "# unk_Ramp::__vtable\n"
    ".obj __vt__8unk_Ramp, global\n"
    "\t.4byte 0x00000000\n"
    "\t.4byte 0x00000000\n"
    "\t.4byte fn_80001018__8unk_RampFf\n"
    ".endobj __vt__8unk_Ramp\n";

static const char QF_ALPHA_CPP[] =
    "#include <game/Alpha.h>\n"
    "\n"
    "/*\n"
    "====================\n"
    "unk_Ramp::fn_80001018\n"
    "\n"
    "Advances the ramp.\n"
    "====================\n"
    "*/\n"
    "// 0x80001018\n"
    "void unk_Ramp::fn_80001018( float dt ) {\n"
    "    helper();\n"
    "}\n"
    "\n"
    "void plain( void ) {\n"
    "    helper();\n"
    "} // 0x80001000\n";

static const char QF_EXCEPTIONS_MD[] =
    "# Exceptions record\n"
    "\n"
    "## MATCH-PENDING\n"
    "\n"
    "| unit | function | rule |\n"
    "|---|---|---|\n"
    "| `game/Alpha` | `fn_80001018__8unk_RampFf` | R17 (steering) |\n"
    "| `game/Beta` | `jumper` | R4 |\n"
    "\n"
    "## Rulings\n"
    "\n"
    "### Alpha rulings\n"
    "\n"
    "The ramp member `fn_80001018` keeps its float argument;\n"
    "this paragraph spans two lines.\n"
    "\n"
    "Unrelated paragraph.\n";

static inline void qf_be32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

/* _row_bytes: every "/ * 8XXXXXXX off  B0 B1 B2 B3 * /" row of a listing, placed into text (based at 0x80001000). */
static inline void qf_place_rows(u8 *text, size_t size, const char *listing) {
    for (const char *line = listing; *line;) {
        const char *nl = strchr(line, '\n');
        unsigned addr, b[4];
        if (strncmp(line, "/* 8", 4) == 0 &&
            sscanf(line, "/* %8x %*8x %2x %2x %2x %2x", &addr, &b[0], &b[1], &b[2], &b[3]) == 5) {
            size_t at = (size_t)addr - 0x80001000u;
            AVP_ASSERT(at + 4 <= size);
            for (int i = 0; i < 4; i++) text[at + (size_t)i] = (u8)b[i];
        }
        if (!nl) break;
        line = nl + 1;
    }
}

/* make_dol: .text 0x80001000..0x80001108 (the fixture's rows), .data 0x80002000..0x80002018, .sdata2 0x80003000
 * (1.5f, pad, 0.25), bss 0x80004000 size 0x100, entry 0x80001000. */
static inline void qf_make_dol(const char *path) {
    enum { TEXT = 0x108, DATA = 0x18, SDATA2 = 0x10, HDR = 0x100 };
    static u8 dol[HDR + TEXT + DATA + SDATA2];
    memset(dol, 0, sizeof dol);
    u8 *text = dol + HDR, *data = text + TEXT, *sdata2 = data + DATA;
    qf_place_rows(text, TEXT, ALPHA_S);
    qf_place_rows(text, TEXT, BETA_S);
    memcpy(data, "a\"b\n\0\0", 6);
    qf_be32(data + 8, 0x80001000u);  /* .4byte fn_80001000__Fv */
    qf_be32(data + 12, 0x80002002u); /* .4byte lbl_80002000+0x2 */
    qf_be32(sdata2, 0x3FC00000u);    /* 1.5f */
    qf_be32(sdata2 + 8, 0x3FD00000u); /* 0.25 (high word; low word 0) */
    const struct {
        int i;
        u32 addr, off, size;
    } secs[] = {{0, 0x80001000u, HDR, TEXT}, {7, 0x80002000u, HDR + TEXT, DATA},
                {8, 0x80003000u, HDR + TEXT + DATA, SDATA2}};
    for (size_t k = 0; k < sizeof secs / sizeof secs[0]; k++) {
        qf_be32(dol + 4 * secs[k].i, secs[k].off);
        qf_be32(dol + 72 + 4 * secs[k].i, secs[k].addr);
        qf_be32(dol + 144 + 4 * secs[k].i, secs[k].size);
    }
    qf_be32(dol + 216, 0x80004000u);
    qf_be32(dol + 220, 0x100u);
    qf_be32(dol + 224, 0x80001000u);
    AvpArena *a = avp_arena();
    avp_makedirs(avp_cz(a, avp_path_dirname(path)), true);
    AVP_ASSERT(avp_write_file(path, dol, sizeof dol));
}

static const char *qf_root_;

static inline void qf_cleanup(void) {
    if (qf_root_) avp_tempdir_cleanup(qf_root_);
    qf_root_ = NULL;
}

/* test_query.make_checkout(): the checkout, made once per test executable in a new temporary folder. */
static inline const char *qf_root(void) {
    if (qf_root_) return qf_root_;
    AvpArena *a = avp_arena();
    char *root = avp_mkdtemp(a, "shsm-query-", "", NULL);
    AVP_ASSERT(root);
    qf_root_ = root;
    atexit(qf_cleanup);
    fx_write(root, "configure.py", QF_CONFIGURE);
    fx_write(root, "tools/.keep", "");
    fx_write(root, "config/TESTV1/config.yml", "object_base: orig/TESTV1\nobject: sys/main.dol\n");
    fx_write(root, "config/TESTV1/symbols.txt", QF_SYMBOLS);
    fx_write(root, "config/TESTV1/splits.txt", QF_SPLITS);
    fx_write(root, "build/TESTV1/asm/game/Alpha.s", ALPHA_S);
    fx_write(root, "build/TESTV1/asm/game/Beta.s", BETA_S);
    fx_write(root, "objdiff.json", QF_OBJDIFF);
    fx_write(root, "build.ninja", QF_NINJA);
    fx_write(root, "build/TESTV1/report.json", avp_sprintf(a, QF_REPORT_FMT, "12.5"));
    fx_write(root, "config/TESTV1/baseline_report.json", avp_sprintf(a, QF_REPORT_FMT, "50.0"));
    fx_write(root, "build/TESTV1/asm/game/Gamma.s", QF_GAMMA_S);
    fx_write(root, "src/game/Alpha.cpp", QF_ALPHA_CPP);
    fx_write(root, "include/game/Alpha.h", "class unk_Ramp {\n    void fn_80001018( float dt );\n};\n");
    fx_write(root, "docs/exceptions.md", QF_EXCEPTIONS_MD);
    qf_make_dol(avp_join(a, root, "orig", "TESTV1", "sys", "main.dol"));
    return root;
}

/* ---- running query.exe ---- */
typedef struct {
    int rc;
    const char *out, *err;
} QfRun;

/* run_query(root, *argv): query.exe argv... --root root (root NULL: no --root). */
static inline QfRun qf_runv(const char *root, const char *const *args) {
    AvpArena *a = avp_arena();
    const char *argv[40];
    size_t n = 0;
    argv[n++] = avp_path_join(a, avp_cz(a, avp_path_dirname(avp_exe_dir())), "query.exe");
    for (; *args; args++) {
        AVP_ASSERT(n < 36);
        argv[n++] = *args;
    }
    if (root) {
        argv[n++] = "--root";
        argv[n++] = root;
    }
    ShsmRun r = shsm_run(a, argv, n, NULL, 300);
    return (QfRun){(int)r.returncode, avp_cz(a, r.out), avp_cz(a, r.err)};
}
#define QF_RUN(...) qf_runv(qf_root(), (const char *const[]){__VA_ARGS__, NULL})

/* q(*argv, rc=): stdout, checking the exit code. */
static inline const char *qf_q(const char *file, int line, int rc, const char *const *args) {
    QfRun r = qf_runv(qf_root(), args);
    if (r.rc != rc) avp_test__fail(file, line, "exit %d, expected %d: %s%s", r.rc, rc, r.out, r.err);
    return r.out;
}
#define QF_Q(...) qf_q(__FILE__, __LINE__, 0, (const char *const[]){__VA_ARGS__, NULL})

/* j(*argv): the --json document (an empty object when it does not parse). */
static inline AvpJson *qf_j(const char *file, int line, const char *const *args) {
    const char *argv[36];
    size_t n = 0;
    for (; *args; args++) argv[n++] = *args;
    argv[n++] = "--json";
    argv[n] = NULL;
    const char *out = qf_q(file, line, 0, argv);
    AvpJsonError e;
    AvpJson *d = avp_json_parse(avp_arena(), avp_str(out), &e);
    if (!d) {
        avp_test__fail(file, line, "not JSON: %s", out);
        return avp_json_obj(avp_arena());
    }
    return d;
}
#define QF_J(...) qf_j(__FILE__, __LINE__, (const char *const[]){__VA_ARGS__, NULL})

/* ---- JSON helpers ---- */
static inline AvpJson *jg(const AvpJson *o, const char *key) { return avp_json_getz(o, key); }
/* a string value as a C string; NULL for null or another kind */
static inline const char *js(const AvpJson *v) { return avp_json_is_str(v) ? avp_cz(avp_arena(), v->u.str) : NULL; }
/* [d[key] for d in arr] joined by ","; "-" for a value that is not a string */
static inline const char *jcol(const AvpJson *arr, const char *key) {
    AvpArena *a = avp_arena();
    const char *s = "";
    for (size_t i = 0; i < avp_json_len(arr); i++) {
        const char *v = js(jg(avp_json_at(arr, i), key));
        s = avp_sprintf(a, "%s%s%s", s, i ? "," : "", v ? v : "-");
    }
    return s;
}
/* the first item of arr whose key equals val, or NULL */
static inline AvpJson *jfind(const AvpJson *arr, const char *key, const char *val) {
    for (size_t i = 0; i < avp_json_len(arr); i++) {
        const char *v = js(jg(avp_json_at(arr, i), key));
        if (v && strcmp(v, val) == 0) return avp_json_at(arr, i);
    }
    return NULL;
}
static inline bool has(const char *hay, const char *needle) { return hay && strstr(hay, needle) != NULL; }
/* the i-th line (0-based) of text, "" past the end */
static inline const char *line_at(const char *text, size_t i) {
    AvpStrVec v = avp_str_splitlines(avp_str(text), false);
    return i < v.n ? avp_cz(avp_arena(), v.v[i]) : "";
}
