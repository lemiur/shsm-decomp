/* Shared fixture of the unit tests (header-only; every tests/*.c is its own test executable): the synthetic
 * repository of tools/test_unit.py (`Repo`, make_asm, make_dol, write_split_record, write_stale_listing,
 * add_bss_fixture, add_weak_fixture, the stub gate script) recreated byte for byte under
 * build/tmp/unit_tests/<pid>_<n>/, and `run_cli`, which runs unit.exe on it as a subprocess.
 *
 * The Python mocked `unit.regen`. Here `new --regen` runs <root>/tools/bin/ninja.exe for real: the test copies its
 * own executable there (repo_install_fake_ninja), and UNIT_TEST_MAIN makes that copy a stand-in for ninja that
 * counts its calls in <cwd>/ninja_calls.txt and, when <cwd>/fake_ninja.txt says "record", writes the split record
 * of the fixture (what the real `ninja build.ninja` leaves behind). */
#pragma once
#include "avp/fs.h"
#include "avp/proc.h"
#include "avp/test.h"
#include "unit_cmd.h"

#include <stdlib.h>
#include <string.h>

#define FX_BS "\\"

/* ---- the fixture text ---- */

#define FX_SYMBOLS                                                                                          \
    "_ctors = .ctors:0x803D8360; // type:label scope:global data:4byte\n"                                    \
    "_dtors = .dtors:0x803D8370; // type:label scope:global data:4byte\n"                                    \
    "fn_80000000 = .text:0x80000000; // type:function size:0x20\n"                                          \
    "__ct__8CFooTestFv = .text:0x80010000; // type:function size:0x10\n"                                    \
    "fn_80010010__8CFooTestFi = .text:0x80010010; // type:function size:0x10\n"                             \
    "fn_80010020__8CFooTestFRC8CBarTestPCc = .text:0x80010020; // type:function size:0x10\n"                \
    "__sinit_" FX_BS "FooTest_cpp = .text:0x80010030; // type:function size:0x10\n"                         \
    "fn_80010040 = .text:0x80010040; // type:function size:0x20\n"                                          \
    "fn_80010060 = .text:0x80010060; // type:function size:0xC\n"                                           \
    "fn_80010070 = .text:0x80010070; // type:function size:0x10\n"                                          \
    "__ct__12unk_801A8F6CFPCcPFRC19unk_AttributePacket_P17CAttributeHandlerUl = .text:0x80020000; "         \
    "// type:function size:0x10\n"                                                                          \
    "lbl_803E0000 = .data:0x803E0000; // type:object size:0x20\n"                                           \
    "lbl_803E0020 = .data:0x803E0020; // type:object size:0x8 data:string\n"                                \
    "lbl_80500000 = .sbss:0x80500000; // type:object size:0x4\n"                                            \
    "lbl_80500004 = .sbss:0x80500004; // type:object size:0x4\n"                                            \
    "lbl_80500008 = .sbss:0x80500008; // type:object size:0x4\n"                                            \
    "lbl_80500010 = .sbss:0x80500010; // type:object size:0x4\n"                                            \
    "lbl_80600000 = .sdata2:0x80600000; // type:object size:0x4 data:float\n"

#define FX_SPLITS                                                                                           \
    "Sections:\n"                                                                                           \
    "\t.init       type:code align:4\n"                                                                      \
    "\t.text       type:code align:8\n"                                                                      \
    "\t.ctors      type:rodata align:16\n"                                                                   \
    "\t.dtors      type:rodata align:16\n"                                                                   \
    "\t.data       type:data align:32\n"                                                                     \
    "\t.sbss       type:bss align:32\n"                                                                      \
    "\t.sdata2     type:rodata align:32\n"                                                                   \
    "\n"                                                                                                    \
    "game/Other.cpp:\n"                                                                                     \
    "\t.text       start:0x80000000 end:0x80000020\n"                                                        \
    "\t.ctors      start:0x803D8360 end:0x803D8364\n"                                                        \
    "\n"                                                                                                    \
    "game/After.cpp:\n"                                                                                     \
    "\t.text       start:0x80010040 end:0x80010060\n"

/* LF form; Repo writes it with CRLF */
#define FX_CONFIGURE                                                                                        \
    "import sys\n"                                                                                          \
    "\n"                                                                                                    \
    "config.libs = [\n"                                                                                     \
    "    {\n"                                                                                               \
    "        \"lib\": \"Runtime\",\n"                                                                       \
    "        \"objects\": [\n"                                                                              \
    "            Object(Matching, \"Runtime/a.c\"),\n"                                                      \
    "        ],\n"                                                                                          \
    "    },\n"                                                                                              \
    "    GameLib(\n"                                                                                        \
    "        \"game\",\n"                                                                                   \
    "        [\n"                                                                                           \
    "            Object(Matching, \"game/Other.cpp\"),\n"                                                   \
    "            # a comment that belongs to the next object\n"                                             \
    "            Object(\n"                                                                                 \
    "                NonMatching,\n"                                                                        \
    "                \"game/After.cpp\",\n"                                                                 \
    "                extra_cflags=[\"-O0\"],\n"                                                             \
    "            ),\n"                                                                                      \
    "        ],\n"                                                                                          \
    "    ),\n"                                                                                              \
    "]\n"                                                                                                   \
    "\n"                                                                                                    \
    "config.progress = True\n"

#define FX_CONFIG_YML                                                                                       \
    "object: sys/main.dol\n"                                                                                 \
    "force_active:\n"                                                                                       \
    "  - __init_cpp_exceptions_reference\n"                                                                  \
    "# registration objects\n"                                                                              \
    "  - lbl_80500FFF\n"                                                                                    \
    "\n"                                                                                                    \
    "mw_comment_version: 14\n"

#define FX_BSS_SYMBOLS                                                                                      \
    "__sinit_" FX_BS "BssTest_cpp = .text:0x80030000; // type:function size:0x30 scope:local\n"             \
    "fn_80030030 = .text:0x80030030; // type:function size:0x10\n"                                          \
    "fn_80040000 = .text:0x80040000; // type:function size:0x20\n"                                          \
    "fn_80050000 = .text:0x80050000; // type:function size:0x10\n"                                          \
    "lbl_80700000 = .bss:0x80700000; // type:object size:0x4\n"                                             \
    "lbl_80700008 = .bss:0x80700008; // type:object size:0x8\n"                                             \
    "lbl_80700010 = .bss:0x80700010; // type:object size:0x20\n"                                            \
    "lbl_80700034 = .bss:0x80700034; // type:object size:0x4\n"                                             \
    "lbl_80700040 = .bss:0x80700040; // type:object size:0x4\n"

#define FX_WEAK_SYMBOLS                                                                                     \
    "fn_80060000 = .text:0x80060000; // type:function size:0x10\n"                                          \
    "fn_80070000 = .text:0x80070000; // type:function size:0x10\n"                                          \
    "fn_80090000 = .text:0x80090000; // type:function size:0x10\n"

#define FX_SINIT_NAME "\"__sinit_" FX_BS "FooTest_cpp\""
#define FX_BSS_SINIT_NAME "\"__sinit_" FX_BS "BssTest_cpp\""

#define FX_SHA "6a3964eaf1ebe8225283363e4161a793a03683d3"

static const char *const FX_LISTINGS[] = {"auto_03_80000000_text", "auto_07_803E0000_data", "auto_10_80500000_sbss",
                                          "auto_11_80600000_sdata2"};

/* ---- files ---- */

static inline AvpArena *fx_a(void) { return avp_arena(); }

/* write(root, rel, text, eol): "\n" in text becomes eol; folders are created. */
static inline void fx_put(const char *root, const char *rel, const char *text, const char *eol) {
    AvpArena *a = fx_a();
    AvpBuf b;
    avp_buf_init(&b, a);
    for (const char *p = text; *p; p++) {
        if (*p == '\n') avp_buf_putz(&b, eol);
        else avp_buf_putc(&b, *p);
    }
    const char *path = avp_join(a, root, rel);
    AVP_ASSERT(avp_makedirs(avp_cz(a, avp_path_dirname(path)), true));
    AvpStr s = avp_buf_str(&b);
    AVP_ASSERT(avp_write_file(path, s.p ? s.p : "", s.n));
}

/* the file's bytes, unchanged */
static inline AvpStr fx_read(const char *root, const char *rel) {
    AvpArena *a = fx_a();
    AvpBytes bytes = {0};
    const char *path = avp_join(a, root, rel);
    AVP_ASSERT(avp_read_all(a, path, &bytes));
    return avp_strn((const char *)bytes.v, bytes.n);
}

/* the file with "\r\n" turned into "\n" (`.replace("\r\n", "\n")`) */
static inline AvpStr fx_read_lf(const char *root, const char *rel) {
    return avp_str_replace(fx_a(), fx_read(root, rel), AVP_S("\r\n"), AVP_S("\n"), -1);
}

static inline bool fx_exists(const char *root, const char *rel) { return avp_exists(avp_join(fx_a(), root, rel)); }

static inline AvpStr fx_lf(AvpStr s) { return avp_str_replace(fx_a(), s, AVP_S("\r\n"), AVP_S("\n"), -1); }

static inline AvpStr fx_replace(AvpStr s, const char *old, const char *new_) {
    return avp_str_replace(fx_a(), s, avp_str(old), avp_str(new_), -1);
}

static inline AvpStr fx_replace1(AvpStr s, const char *old, const char *new_) {
    return avp_str_replace(fx_a(), s, avp_str(old), avp_str(new_), 1);
}

/* str.index(sub): the offset, -1 when absent (a failed CHECK in the caller) */
static inline i64 fx_pos(AvpStr s, const char *sub) { return avp_str_find(s, avp_str(sub), 0, AVP_END); }

/* ---- assertions on text ---- */

#define CHECK_IN(text, cz)                                                                                  \
    do {                                                                                                    \
        AvpStr in_t_ = (text);                                                                              \
        const char *in_c_ = (cz);                                                                           \
        CHECK_MSG(avp_str_contains(in_t_, avp_str(in_c_)), "expected \"%s\" in:\n%.*s", in_c_, AVP_SF(in_t_)); \
    } while (0)
#define CHECK_NOT_IN(text, cz)                                                                              \
    do {                                                                                                    \
        AvpStr in_t_ = (text);                                                                              \
        const char *in_c_ = (cz);                                                                           \
        CHECK_MSG(!avp_str_contains(in_t_, avp_str(in_c_)), "unexpected \"%s\" in:\n%.*s", in_c_, AVP_SF(in_t_)); \
    } while (0)
/* s.index(a) < s.index(b), both present */
#define CHECK_BEFORE(text, a_cz, b_cz)                                                                      \
    do {                                                                                                    \
        AvpStr bt_ = (text);                                                                                \
        i64 bp_a_ = fx_pos(bt_, (a_cz)), bp_b_ = fx_pos(bt_, (b_cz));                                       \
        CHECK_MSG(bp_a_ >= 0 && bp_b_ >= 0 && bp_a_ < bp_b_, "\"%s\" (%lld) is not before \"%s\" (%lld)",   \
                  (a_cz), (long long)bp_a_, (b_cz), (long long)bp_b_);                                      \
    } while (0)
#define CHECK_BYTES_EQ(a, b) \
    CHECK_MSG(avp_str_eq((a), (b)), "%s and %s differ", #a, #b)

/* ---- running the tool ---- */

typedef struct {
    i64 rc;
    AvpStr out, err;
} Cli;

/* assertEqual(rc, want, out + err) */
#define CHECK_RC(r, want) \
    CHECK_MSG((r).rc == (want), "exit code %lld, expected %d\nstdout:\n%.*s\nstderr:\n%.*s", (long long)(r).rc, \
              (int)(want), AVP_SF((r).out), AVP_SF((r).err))

static inline const char *fx_unit_exe(void) {
    AvpArena *a = fx_a();
    return avp_path_normpath(a, avp_join(a, avp_exe_dir(), "..", "unit.exe"));
}

static inline Cli fx_run(const char *cwd, const char *const *argv_in, size_t skip_exe) {
    AvpArena *a = fx_a();
    (void)skip_exe;
    AvpProcResult pr = avp_run(&(AvpProcSpec){.argv = argv_in, .cwd = cwd, .out = AVP_IO_CAPTURE,
                                              .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                              .timeout = 600, .arena = a});
    if (pr.status != AVP_PROC_OK)
        fprintf(stderr, "cannot run %s: status %d %s\n", argv_in[0], (int)pr.status, pr.message ? pr.message : "");
    AVP_ASSERT(pr.status == AVP_PROC_OK);
    Cli c = {pr.returncode, pr.out, pr.err};
    return c;
}

typedef struct {
    const char *root;
} Repo;

/* run_cli(root, *args): unit.exe --root <root> <args>; args NULL-terminated */
static inline Cli run_cli_a(const Repo *r, const char *const *args) {
    AvpArena *a = fx_a();
    size_t n = 0;
    while (args[n]) n++;
    const char **argv = AVP_NEWN(a, const char *, n + 4);
    argv[0] = fx_unit_exe();
    argv[1] = "--root";
    argv[2] = r->root;
    for (size_t i = 0; i < n; i++) argv[3 + i] = args[i];
    argv[3 + n] = NULL;
    return fx_run(NULL, argv, 0);
}
#define RUN(r, ...) run_cli_a(&(r), (const char *const[]){__VA_ARGS__, NULL})

/* git(root, *args), which must succeed */
static inline Cli fx_git_a(const char *root, const char *const *args) {
    AvpArena *a = fx_a();
    size_t n = 0;
    while (args[n]) n++;
    const char **argv = AVP_NEWN(a, const char *, n + 8);
    size_t k = 0;
    argv[k++] = "git";
    argv[k++] = "-c";
    argv[k++] = "user.name=t";
    argv[k++] = "-c";
    argv[k++] = "user.email=t@t";
    argv[k++] = "-c";
    argv[k++] = "commit.gpgsign=false";
    for (size_t i = 0; i < n; i++) argv[k++] = args[i];
    argv[k] = NULL;
    Cli c = fx_run(root, argv, 0);
    if (c.rc != 0) fprintf(stderr, "git %s failed in %s:\n%.*s%.*s\n", args[0], root, AVP_SF(c.out), AVP_SF(c.err));
    AVP_ASSERT(c.rc == 0);
    return c;
}
#define GIT(root, ...) fx_git_a((root), (const char *const[]){__VA_ARGS__, NULL})

/* ---- asm text ---- */

static inline void fx_block(AvpBuf *b, u32 addr, const char *name, u32 size, const char *const *body, size_t n) {
    avp_buf_printf(b, "# .text:0x0 | 0x%08X | size: 0x%X\n.fn %s, global\n", addr, size, name);
    for (size_t i = 0; i < n; i++)
        avp_buf_printf(b, "/* %08X 00000000  00 00 00 00 */\t%s\n", addr + 4 * (u32)i, body[i]);
    avp_buf_printf(b, ".endfn %s\n\n", name);
}
#define FX_BLOCK(b, addr, name, size, ...)                                                                  \
    fx_block((b), (addr), (name), (size), (const char *const[]){__VA_ARGS__},                               \
             sizeof((const char *const[]){__VA_ARGS__}) / sizeof(const char *))

#define FX_TEXT_HEAD(file) ".include \"macros.inc\"\n.file \"" file "\"\n\n.text\n.balign 4\n\n"

static inline void fx_put_buf(const char *root, const char *rel, AvpBuf *b) {
    fx_put(root, rel, avp_buf_cz(b), "\n");
}

static inline void make_asm(const char *root) {
    AvpArena *a = fx_a();
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, FX_TEXT_HEAD("auto_03_x_text"));
    FX_BLOCK(&t, 0x80000000, "fn_80000000", 0x20, "lwz r0, lbl_80500010@sda21(r0)", "blr");
    FX_BLOCK(&t, 0x80010000, "__ct__8CFooTestFv", 0x10, "lis r3, lbl_803E0000@ha", "addi r3, r3, lbl_803E0000@l",
             "lwz r0, lbl_80500010@sda21(r0)", "blr");
    FX_BLOCK(&t, 0x80010010, "fn_80010010__8CFooTestFi", 0x10, "lfs f0, lbl_80600000@sda21(r0)",
             "lwz r4, lbl_80500010@sda21(r0)", "bl fn_80010060", "blr");
    FX_BLOCK(&t, 0x80010020, "fn_80010020__8CFooTestFRC8CBarTestPCc", 0x10, "lwz r0, lbl_80500004@sda21(r0)", "blr");
    FX_BLOCK(&t, 0x80010030, FX_SINIT_NAME, 0x10, "li r3, lbl_80500008@sda21", "lis r4, lbl_803E0020@ha",
             "li r3, lbl_80500000@sda21",
             "bl __ct__12unk_801A8F6CFPCcPFRC19unk_AttributePacket_P17CAttributeHandlerUl", "blr");
    FX_BLOCK(&t, 0x80010040, "fn_80010040", 0x20, "lwz r0, lbl_80500004@sda21(r0)", "blr");
    FX_BLOCK(&t, 0x80010060, "fn_80010060", 0xC, "blr");
    FX_BLOCK(&t, 0x80010070, "fn_80010070", 0x10, "blr");
    FX_BLOCK(&t, 0x80020000, "__ct__12unk_801A8F6CFPCcPFRC19unk_AttributePacket_P17CAttributeHandlerUl", 0x10, "blr");
    fx_put_buf(root, "build/R5WEA4/asm/auto_03_80000000_text.s", &t);

    AvpBuf d;
    avp_buf_init(&d, a);
    avp_buf_putz(&d, ".include \"macros.inc\"\n.file \"auto_07_803E0000_data\"\n\n# 0x803E0000..0x803E0030 | size: 0x30\n"
                     ".data\n.balign 8\n\n");
    avp_buf_putz(&d, "# .data:0x0 | 0x803E0000 | size: 0x20\n.obj lbl_803E0000, global\n\t.4byte 0x00000000\n"
                     "\t.4byte fn_80010010__8CFooTestFi\n\t.4byte 0x00000000\n.endobj lbl_803E0000\n\n");
    avp_buf_putz(&d, "# .data:0x20 | 0x803E0020 | size: 0x8\n.obj lbl_803E0020, global\n\t.string \"CFooTest\"\n"
                     ".endobj lbl_803E0020\n\n");
    avp_buf_putz(&d, "# .data:0x28 | 0x803E0028 | size: 0x4\n.obj gap_07_803E0028_data, global\n"
                     ".hidden gap_07_803E0028_data\n\t.4byte 0x00000000\n.endobj gap_07_803E0028_data\n\n");
    fx_put_buf(root, "build/R5WEA4/asm/auto_07_803E0000_data.s", &d);

    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, ".include \"macros.inc\"\n.file \"auto_10_80500000_sbss\"\n\n# 0x80500000..0x80500014 | size: 0x14\n"
                     ".section .sbss, \"wa\", @nobits\n.balign 8\n\n");
    static const u32 offs[] = {0, 4, 8, 0x10};
    for (size_t i = 0; i < 4; i++)
        avp_buf_printf(&b,
                       "# .sbss:0x%X | 0x%08X | size: 0x4\n.obj lbl_%08X, global\n\t.skip 0x4\n.endobj lbl_%08X\n\n",
                       offs[i], 0x80500000u + offs[i], 0x80500000u + offs[i], 0x80500000u + offs[i]);
    fx_put_buf(root, "build/R5WEA4/asm/auto_10_80500000_sbss.s", &b);

    AvpBuf c;
    avp_buf_init(&c, a);
    avp_buf_putz(&c, ".include \"macros.inc\"\n.file \"auto_11_80600000_sdata2\"\n\n# 0x80600000..0x80600004 | size: 0x4\n"
                     ".section .sdata2, \"a\"\n.balign 8\n\n");
    avp_buf_putz(&c, "# .sdata2:0x0 | 0x80600000 | size: 0x4\n.obj lbl_80600000, global\n\t.float 1.0\n"
                     ".endobj lbl_80600000\n");
    fx_put_buf(root, "build/R5WEA4/asm/auto_11_80600000_sdata2.s", &c);
}

static inline void fx_be32(unsigned char *p, u32 v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* a DOL with a 0x10-byte .ctors table (data section 7 at 0x803D8360) holding two words */
static inline void make_dol(const char *root) {
    AvpArena *a = fx_a();
    unsigned char buf[0x110];
    memset(buf, 0, sizeof buf);
    fx_be32(buf + 7 * 4, 0x100);
    fx_be32(buf + 18 * 4 + 7 * 4, 0x803D8360);
    fx_be32(buf + 36 * 4 + 7 * 4, 0x10);
    fx_be32(buf + 0x100, 0x80000000);
    fx_be32(buf + 0x104, 0);
    fx_be32(buf + 0x108, 0x80010030);
    fx_be32(buf + 0x10C, 0);
    const char *dir = avp_join(a, root, "orig", "R5WEA4", "sys");
    AVP_ASSERT(avp_makedirs(dir, true));
    AVP_ASSERT(avp_write_file(avp_join(a, dir, "main.dol"), buf, sizeof buf));
}

/* build/R5WEA4/config.json as `dtk dol split` writes it: the units of the current split. */
static inline void write_split_record(const char *root) {
    AvpArena *a = fx_a();
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "{\"version\": \"1.8.4\", \"name\": \"main\", \"units\": [");
    for (size_t i = 0; i < sizeof FX_LISTINGS / sizeof *FX_LISTINGS; i++)
        avp_buf_printf(&b, "%s{\"object\": \"build/R5WEA4/obj/%s.o\", \"name\": \"%s\", \"autogenerated\": true}",
                       i ? ", " : "", FX_LISTINGS[i], FX_LISTINGS[i]);
    avp_buf_putz(&b, "], \"modules\": []}");
    fx_put_buf(root, "build/R5WEA4/config.json", &b);
}

/* A listing an earlier split left behind: the same code under a name symbols.txt no longer has. */
static inline void write_stale_listing_at(const char *root, const char *rel) {
    AvpArena *a = fx_a();
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, ".include \"macros.inc\"\n.file \"old\"\n\n.text\n.balign 4\n\n");
    FX_BLOCK(&t, 0x80010000, "fn_80010000_old", 0x10, "blr");
    fx_put_buf(root, avp_sprintf(a, "build/R5WEA4/asm/%s", rel), &t);
}
static inline void write_stale_listing(const char *root) { write_stale_listing_at(root, "auto_03_80010000_text.s"); }

static inline void add_bss_fixture(const char *root) {
    AvpArena *a = fx_a();
    AvpStr old = fx_lf(fx_read(root, "config/R5WEA4/symbols.txt"));
    fx_put(root, "config/R5WEA4/symbols.txt", avp_cz(a, avp_str_concat(a, old, AVP_S(FX_BSS_SYMBOLS))), "\n");
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, FX_TEXT_HEAD("auto_03_80030000_text"));
    FX_BLOCK(&t, 0x80030000, FX_BSS_SINIT_NAME, 0x30,
             "lis r3, lbl_80700000@ha",  /* stored to through the symbol */
             "lis r4, lbl_80700034@ha",
             "addi r4, r4, lbl_80700034@l", /* only its address is copied: not defined here */
             "stw r4, lbl_80700000@l(r3)",
             "lis r3, lbl_80700008@ha", /* constructed: the `this` of a __ct__ call */
             "addi r3, r3, lbl_80700008@l",
             "bl __ct__12unk_801A8F6CFPCcPFRC19unk_AttributePacket_P17CAttributeHandlerUl",
             "lis r5, lbl_80700010@ha", /* several stores through a base register */
             "addi r5, r5, lbl_80700010@l", "li r0, 0", "stw r0, 0(r5)", "stw r0, 4(r5)", "blr");
    FX_BLOCK(&t, 0x80030030, "fn_80030030", 0x10, "blr");
    FX_BLOCK(&t, 0x80040000, "fn_80040000", 0x20, "lis r3, lbl_80700000@ha", "lwz r0, lbl_80700000@l(r3)",
             "lis r3, lbl_80700008@ha", "lwz r0, lbl_80700008@l(r3)", "lis r3, lbl_80700010@ha",
             "lwz r0, lbl_80700010@l(r3)", "lis r3, lbl_80700034@ha", "lwz r0, lbl_80700034@l(r3)",
             "lis r3, lbl_80700040@ha", "lwz r0, lbl_80700040@l(r3)", "blr");
    FX_BLOCK(&t, 0x80050000, "fn_80050000", 0x10, "lis r3, lbl_80700040@ha", "lwz r0, lbl_80700040@l(r3)", "blr");
    fx_put_buf(root, "build/R5WEA4/asm/auto_03_80030000_text.s", &t);

    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, ".include \"macros.inc\"\n.file \"auto_08_80700000_bss\"\n\n# 0x80700000..0x80700044 | size: 0x44\n"
                     ".section .bss, \"wa\", @nobits\n.balign 8\n\n");
    static const struct {
        u32 off, size;
    } objs[] = {{0, 4}, {4, 4}, {8, 8}, {0x10, 0x20}, {0x30, 4}, {0x34, 4}, {0x38, 8}, {0x40, 4}};
    for (size_t i = 0; i < sizeof objs / sizeof *objs; i++) {
        u32 off = objs[i].off;
        const char *name;
        if (off == 0 || off == 8 || off == 0x10 || off == 0x34 || off == 0x40)
            name = avp_sprintf(a, "lbl_%08X", 0x80700000u + off);
        else name = avp_sprintf(a, "gap_08_%08X_bss", 0x80700000u + off);
        avp_buf_printf(&b, "# .bss:0x%X | 0x%08X | size: 0x%X\n.obj %s, global\n\t.skip 0x%X\n.endobj %s\n\n", off,
                       0x80700000u + off, objs[i].size, name, objs[i].size, name);
    }
    fx_put_buf(root, "build/R5WEA4/asm/auto_08_80700000_bss.s", &b);
}

static inline void add_weak_fixture(const char *root) {
    AvpArena *a = fx_a();
    AvpStr old = fx_lf(fx_read(root, "config/R5WEA4/symbols.txt"));
    fx_put(root, "config/R5WEA4/symbols.txt", avp_cz(a, avp_str_concat(a, old, AVP_S(FX_WEAK_SYMBOLS))), "\n");
    AvpBuf t;
    avp_buf_init(&t, a);
    avp_buf_putz(&t, FX_TEXT_HEAD("auto_03_80060000_text"));
    FX_BLOCK(&t, 0x80060000, "fn_80060000", 0x10, "lis r3, __vt__4CWkA@ha", "addi r3, r3, __vt__4CWkA@l", "blr");
    FX_BLOCK(&t, 0x80070000, "fn_80070000", 0x10, "lis r3, __vt__4CWkB@ha", "addi r3, r3, __vt__4CWkB@l", "blr");
    fx_put_buf(root, "build/R5WEA4/asm/auto_03_80060000_text.s", &t);

    static const struct {
        u32 off, size;
        const char *name, *scope, *l1, *l2;
    } objs[] = {
        {0x00, 0x8, "__vt__4CWkA", "global", "\t.4byte __RTTI__4CWkA", "\t.4byte fn_80060000"},
        {0x08, 0x8, "__RTTI__4CWkA", "weak", "\t.4byte lbl_80710010", "\t.4byte lbl_80710018"},
        {0x10, 0x5, "lbl_80710010", "global", "\t.string \"CWkA\"", NULL},
        {0x15, 0x3, "gap_07_80710015_data", "global", "\t.byte 0x00, 0x00, 0x00", NULL},
        {0x18, 0x8, "lbl_80710018", "global", "\t.4byte __RTTI__4CWkB", "\t.4byte 0x00000000"},
        {0x20, 0x5, "lbl_80710020", "global", "\t.string \"CWkB\"", NULL},
        {0x25, 0x3, "gap_07_80710025_data", "global", "\t.byte 0x00, 0x00, 0x00", NULL},
        {0x28, 0x8, "__RTTI__4CWkB", "weak", "\t.4byte lbl_80710020", "\t.4byte 0x00000000"},
        {0x30, 0x8, "__vt__4CWkB", "global", "\t.4byte __RTTI__4CWkB", "\t.4byte fn_80070000"},
    };
    AvpBuf d;
    avp_buf_init(&d, a);
    avp_buf_putz(&d, ".include \"macros.inc\"\n.file \"auto_07_80710000_data\"\n\n# 0x80710000..0x80710038 | size: 0x38\n"
                     ".data\n.balign 8\n\n");
    for (size_t i = 0; i < sizeof objs / sizeof *objs; i++) {
        avp_buf_printf(&d, "# .data:0x%X | 0x%08X | size: 0x%X\n.obj %s, %s\n", objs[i].off, 0x80710000u + objs[i].off,
                       objs[i].size, objs[i].name, objs[i].scope);
        if (!strncmp(objs[i].name, "gap_", 4)) avp_buf_printf(&d, ".hidden %s\n", objs[i].name);
        avp_buf_printf(&d, "%s\n", objs[i].l1);
        if (objs[i].l2) avp_buf_printf(&d, "%s\n", objs[i].l2);
        avp_buf_printf(&d, ".endobj %s\n\n", objs[i].name);
    }
    fx_put_buf(root, "build/R5WEA4/asm/auto_07_80710000_data.s", &d);
}

/* A stub gate (a batch file, run through cmd.exe); the report lists the unit under test and the fixture's other
 * Matching units. The percentages are written as given ("100.0", "87.5", "99.0"). */
static inline const char *fx_gate_script(bool ok, const char *report_pct, const char *other_pct, bool report) {
    AvpArena *a = fx_a();
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "@echo off\nif not exist build\\R5WEA4 mkdir build\\R5WEA4\n");
    if (report)
        avp_buf_printf(&b,
                       ">build\\R5WEA4\\report.json echo {\"units\": [{\"name\": \"main/game/CFooTest\", \"measures\": "
                       "{\"matched_code_percent\": %s}}, {\"name\": \"main/Runtime/a\", \"measures\": "
                       "{\"matched_code_percent\": %s}}, {\"name\": \"main/game/Other\", \"measures\": "
                       "{\"matched_code_percent\": 100.0}}]}\n",
                       report_pct, other_pct);
    avp_buf_printf(&b, "echo %s\n", ok ? "build/R5WEA4/main.dol: OK" : "nothing to see");
    return avp_buf_cz(&b);
}

/* ---- the repository ---- */

/* an empty fixture folder build/tmp/unit_tests/<pid>_<n> */
static inline Repo repo_new_root(void) {
    static int counter;
    AvpArena *a = fx_a();
    Repo r;
    r.root = avp_path_abspath(
        a, avp_join(a, "build", "tmp", "unit_tests", avp_sprintf(a, "%u_%d", (unsigned)avp_getpid(), counter++)));
    if (avp_isdir(r.root)) avp_rmtree(r.root, AVP_RMTREE_CLEAR_READONLY);
    AVP_ASSERT(avp_makedirs(r.root, true));
    return r;
}

static inline Repo repo_new(bool with_git) {
    Repo r = repo_new_root();
    fx_put(r.root, "config/R5WEA4/symbols.txt", FX_SYMBOLS, "\n");
    fx_put(r.root, "config/R5WEA4/splits.txt", FX_SPLITS, "\n");
    fx_put(r.root, "config/R5WEA4/config.yml", FX_CONFIG_YML, "\r\n");
    fx_put(r.root, "configure.py", FX_CONFIGURE, "\r\n");
    fx_put(r.root, "src/game/Other.cpp", "// other\n", "\r\n");
    make_asm(r.root);
    make_dol(r.root);
    if (with_git) {
        fx_put(r.root, ".gitignore", "build/\n", "\n");
        fx_put(r.root, "stub_gate.cmd", fx_gate_script(true, "100.0", "100.0", true), "\r\n");
        GIT(r.root, "init", "-q");
        GIT(r.root, "config", "core.autocrlf", "false");
        GIT(r.root, "add", "-A");
        GIT(r.root, "commit", "-q", "-m", "init");
    }
    return r;
}

static inline void repo_cleanup(Repo *r) {
    if (r->root && avp_isdir(r->root)) avp_rmtree(r->root, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}


/* ---- templates: a repository that is expensive to make (git init, add, commit, `new`) is made once per test
 * executable and copied for every test (the Python made it in setUp; the copy is the same tree, git state included). */

static inline void fx_copy_tree(const char *src, const char *dst) {
    AvpArena *a = fx_a();
    size_t sl = strlen(src);
    AvpWalk *w = avp_walk_open(src, true);
    AVP_ASSERT(w != NULL);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        const char *dir = avp_cz(a, it.dirpath);
        const char *rel = dir + sl;
        while (*rel == '\\' || *rel == '/') rel++;
        const char *to = *rel ? avp_join(a, dst, rel) : dst;
        AVP_ASSERT(avp_makedirs(to, true));
        for (size_t i = 0; i < it.dirs.n; i++)
            AVP_ASSERT(avp_makedirs(avp_join(a, to, avp_cz(a, it.dirs.v[i])), true));
        for (size_t i = 0; i < it.files.n; i++) {
            const char *name = avp_cz(a, it.files.v[i]);
            AVP_ASSERT(avp_copyfile(avp_join(a, dir, name), avp_join(a, to, name)));
        }
    }
    avp_walk_close(w);
}

static const char *fx_template_roots[4];
static int fx_template_n;

static inline void fx_templates_cleanup(void) {
    for (int i = 0; i < fx_template_n; i++)
        if (avp_isdir(fx_template_roots[i]))
            avp_rmtree(fx_template_roots[i], AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS);
}

/* A new repository that is a copy of *tmpl, which make() builds on first use. */
static inline Repo repo_from_template(Repo *tmpl, Repo (*make)(void)) {
    if (!tmpl->root) {
        *tmpl = make();
        AVP_ASSERT(fx_template_n < 4);
        if (fx_template_n == 0) atexit(fx_templates_cleanup);
        fx_template_roots[fx_template_n++] = tmpl->root;
    }
    Repo r = repo_new_root();
    fx_copy_tree(tmpl->root, r.root);
    return r;
}

/* ---- the stand-in for ninja ---- */

static const char *fx_self_exe;

static inline bool fx_is_fake_ninja(const char *argv0) {
    AvpStr base = avp_path_basename(argv0);
    return avp_str_eqz(avp_str_lower(fx_a(), base), "ninja.exe");
}

static inline int fx_fake_ninja(void) {
    AvpArena *a = fx_a();
    AVP_ASSERT(avp_append_file("ninja_calls.txt", "build.ninja\n", 12));
    AvpStr mode = {0};
    if (avp_exists("fake_ninja.txt") && avp_read_text(a, "fake_ninja.txt", AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT), &mode) &&
        avp_str_startswith(avp_str_strip(mode), AVP_S("record")))
        write_split_record(".");
    return 0;
}

/* tools/bin/ninja.exe of the repository: a copy of this executable; `mode` "record" makes it write the split
 * record, "" only counts. */
static inline void repo_install_fake_ninja(const Repo *r, const char *mode) {
    AvpArena *a = fx_a();
    AVP_ASSERT(fx_self_exe != NULL);
    const char *dir = avp_join(a, r->root, "tools", "bin");
    AVP_ASSERT(avp_makedirs(dir, true));
    AVP_ASSERT(avp_copyfile(fx_self_exe, avp_join(a, dir, "ninja.exe")));
    fx_put(r->root, "fake_ninja.txt", mode, "\n");
}

static inline int fx_ninja_calls(const Repo *r) {
    if (!fx_exists(r->root, "ninja_calls.txt")) return 0;
    return (int)avp_str_count(fx_read(r->root, "ninja_calls.txt"), AVP_S("build.ninja"));
}

/* AVP_TEST_MAIN, plus the stand-in for ninja. */
#define UNIT_TEST_MAIN(suite_name)                                                  \
    static int unit_test__main(int argc, char **argv) {                             \
        if (argc > 0 && fx_is_fake_ninja(argv[0])) return fx_fake_ninja();           \
        fx_self_exe = avp_path_abspath(fx_a(), argv[0]);                            \
        return avp_test__run(argc, argv);                                           \
    }                                                                               \
    AVP_MAIN(unit_test__main, "test_" suite_name)
