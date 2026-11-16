/* wdbg: the spec language (SPEC.md) on a fake host: parsing and its errors, Python's % formatting, the interpreter
 * (variables, registers, memory, hits, env, conditionals, handler errors) and the three shipped specs. The debugger
 * loop itself is checked by the differential harness (diff/cases.txt) against tools/mwcc_trace/wdbg.py. */
#include "avp/test.h"
#include "wdbg_spec.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* ---- fake host: 64 KiB of memory at 0x740000, the log as a list of lines ---------------------------------------- */

enum { MEM_BASE = 0x740000, MEM_SIZE = 0x10000 };
typedef struct {
    u8 mem[MEM_SIZE];
    AvpStrVec lines;
    AvpStrVec snaps;
    AvpArena *a;
} Fake;

static size_t f_read(void *ctx, u32 addr, void *buf, size_t n) {
    Fake *f = ctx;
    if (addr < MEM_BASE || addr >= MEM_BASE + MEM_SIZE) return 0;
    size_t off = addr - MEM_BASE, got = n < MEM_SIZE - off ? n : MEM_SIZE - off;
    memcpy(buf, f->mem + off, got);
    return got;
}
static void f_write(void *ctx, u32 addr, const void *p, size_t n) {
    Fake *f = ctx;
    if (addr >= MEM_BASE && addr + n <= MEM_BASE + MEM_SIZE) memcpy(f->mem + (addr - MEM_BASE), p, n);
}
static void f_out(void *ctx, AvpStr text) {
    Fake *f = ctx;
    avp_vec_push(&f->lines, avp_str_dup(f->a, text));
}
static bool f_snap(void *ctx, AvpStr name, i64 lo, i64 hi, const char **err) {
    Fake *f = ctx;
    (void)err;
    avp_vec_push(&f->snaps, avp_pyfmt(f->a, "%s %#x %#x", avp_cz(f->a, name), (long long)lo, (long long)hi));
    return true;
}
static AvpStr f_sym(void *ctx, AvpArena *a, i64 addr) {
    (void)ctx;
    return addr == 0x5b88f0 ? AVP_S("colorinstructions (Coloring.obj)") : avp_pyfmt(a, "%#x", (long long)addr);
}

/* the NB11 names of two compiler builds (the two the shipped iro_gate spec runs on); the build under test is h.build */
static const char *g_build = "Wii/1.0";
static bool f_lookup(void *ctx, AvpArena *a, AvpStr name, i64 *addr) {
    (void)ctx;
    (void)a;
    bool b1a = strcmp(g_build, "Wii/1.0a") == 0;
    if (avp_str_eq(name, AVP_S("IRO_Optimizer"))) *addr = b1a ? 0x58d730 : 0x591c60;
    else if (avp_str_eq(name, AVP_S("IRO_CanThisIRBeOptimized"))) *addr = b1a ? 0x5dcb50 : 0x5e1940;
    else return false;
    return true;
}

static Fake *fake_new(WdbgHost *h) {
    Fake *f = avp_xcalloc(1, sizeof *f);
    f->a = avp_arena();
    *h = (WdbgHost){.ctx = f, .read = f_read, .write = f_write, .out = f_out, .snap = f_snap, .sym = f_sym,
                  .build = g_build, .lookup = f_lookup};
    return f;
}
static void put32(Fake *f, u32 addr, u32 v) { memcpy(f->mem + (addr - MEM_BASE), &v, 4); }

typedef struct {
    u32 v[WDBG_NREGS];
    u32 *p[WDBG_NREGS];
} Regs;
static void regs_init(Regs *r) {
    memset(r->v, 0, sizeof r->v);
    for (int i = 0; i < WDBG_NREGS; i++) r->p[i] = &r->v[i];
}
static int reg(const char *name) {
    for (int i = 0; i < WDBG_NREGS; i++)
        if (strcmp(wdbg_reg_names[i], name) == 0) return i;
    return -1;
}

static WdbgSpec *parse(const char *text) {
    const char *err = NULL;
    WdbgSpec *s = wdbg_spec_parse(avp_arena(), "t.wdbg", avp_str(text), &err);
    CHECK_MSG(s != NULL, "parse: %s", err ? err : "?");
    return s;
}
static const char *parse_err(const char *text) {
    const char *err = NULL;
    WdbgSpec *s = wdbg_spec_parse(avp_arena(), "t.wdbg", avp_str(text), &err);
    CHECK(s == NULL);
    return s ? "" : err;
}

/* ---- % formatting --------------------------------------------------------------------------------------------- */

static const char *fmt(const char *f, const WdbgVal *args, size_t n) {
    AvpStr out;
    const char *err = NULL;
    if (!wdbg_format(avp_arena(), avp_str(f), args, n, &out, &err)) return err;
    return avp_cz(avp_arena(), out);
}
#define I(x) ((WdbgVal){.k = WDBG_INT, .i = (x)})
#define S(x) ((WdbgVal){.k = WDBG_STR, .s = AVP_S(x)})
#define B(x) ((WdbgVal){.k = WDBG_BYTES, .s = AVP_S(x)})

AVP_TEST(format_like_python_percent) {
    WdbgVal a[] = {I(3), I(-1), I(0x1f), I(0)};
    /* 'cmp op=%d/%d flags=%#x/%#x' % (3, -1, 0x1f, 0) */
    CHECK_CZ(fmt("cmp op=%d/%d flags=%#x/%#x", a, 4), "cmp op=3/-1 flags=0x1f/0x0");
    WdbgVal b[] = {I(5), I(255), I(0xbeef), S("q"), B("\0\0"), I(65)};
    CHECK_CZ(fmt("%5d|%-5x|%08X|%r|%s|%c", b, 6), "    5|ff   |0000BEEF|'q'|b'\\x00\\x00'|A");
    WdbgVal c[] = {I(7)};
    CHECK_CZ(fmt("100%% %i %u", (WdbgVal[]){I(1), I(2)}, 2), "100% 1 2");
    CHECK_CZ(fmt("%*d|%-*d|", (WdbgVal[]){I(4), I(7), I(3), I(7)}, 4), "   7|7  |");
    CHECK_CZ(fmt("%o %X %#o", (WdbgVal[]){I(8), I(-255), I(8)}, 3), "10 -FF 0o10");
    CHECK_CZ(fmt("%s %r", (WdbgVal[]){S("it's"), S("it's")}, 2), "it's \"it's\"");
    CHECK_CZ(fmt("%d", c, 1), "7");
}

AVP_TEST(format_errors_are_python_messages) {
    CHECK_CZ(fmt("%d %d", (WdbgVal[]){I(1)}, 1), "TypeError: not enough arguments for format string");
    CHECK_CZ(fmt("%d", (WdbgVal[]){I(1), I(2)}, 2), "TypeError: not all arguments converted during string formatting");
    CHECK_CZ(fmt("%d", (WdbgVal[]){S("x")}, 1), "TypeError: %d format: a real number is required, not str");
    CHECK_CZ(fmt("%x", (WdbgVal[]){S("x")}, 1), "TypeError: %x format: an integer is required, not str");
    CHECK_CZ(fmt("%y", (WdbgVal[]){I(1)}, 1), "ValueError: unsupported format character 'y' (0x79) at index 1");
    CHECK_CZ(fmt("abc %", (WdbgVal[]){I(1)}, 1), "ValueError: incomplete format");
}

/* ---- parsing -------------------------------------------------------------------------------------------------- */

AVP_TEST(parse_errors_name_file_and_line) {
    CHECK_CZ(parse_err("bp 0x10\n    out 1\n"), "t.wdbg:3: expected 'end'");
    CHECK_CZ(parse_err("x = 1 $\n"), "t.wdbg:1: invalid character '$'");
    CHECK_CZ(parse_err("bp 1\n  bp 2\n  end\nend\n"), "t.wdbg:2: 'bp' inside a block");
    CHECK_CZ(parse_err("x = (1\n"), "t.wdbg:2: '(' was never closed");
    CHECK_CZ(parse_err("end\n"), "t.wdbg:1: unexpected 'end'");
    CHECK_CZ(parse_err("wr 1\n"), "t.wdbg:1: wr takes an address and a bytes value");
    CHECK_CZ(parse_err("x = 1 < 2 < 3\n"), "t.wdbg:1: chained comparisons are not supported");
    CHECK_CZ(parse_err("x = \"abc\n"), "t.wdbg:1: unterminated string literal");
    CHECK_CZ(parse_err("x = x\"0g\"\n"), "t.wdbg:1: bytes literal: expected pairs of hex digits");
}

AVP_TEST(breakpoints_in_first_order_last_body) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = parse("# comment\nbp 0x30\n out \"a\"\nend\nbp 0x10\nend\nbp 0x30\n out \"b\"\nend\n");
    REQUIRE(s);
    const char *err = NULL;
    REQUIRE(wdbg_spec_load(s, &h, &err));
    CHECK_EQ_U64(wdbg_spec_bp_count(s), 2);
    CHECK_EQ_U64(wdbg_spec_bp_addr(s, 0), 0x30);
    CHECK_EQ_U64(wdbg_spec_bp_addr(s, 1), 0x10);
    CHECK(wdbg_spec_has_bp(s, 0x10) && !wdbg_spec_has_bp(s, 0x20));
    Regs r;
    regs_init(&r);
    CHECK(wdbg_spec_hit(s, 0x30, r.p));
    CHECK_EQ_U64(f->lines.n, 1);
    CHECK_STR(f->lines.v[0], "b");
    CHECK_EQ_I64(wdbg_spec_hits(s, 0x30), 1);
    CHECK_EQ_I64(wdbg_spec_hits(s, 0x10), 0);
}

/* ---- interpreting --------------------------------------------------------------------------------------------- */

AVP_TEST(interpreter_values_registers_memory) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    put32(f, 0x740100, 0x12345678);
    WdbgSpec *s = parse("n = 0\n"
                        "bp 0x401000\n"
                        "    n += 1\n"
                        "    v = u32(Esp + 4)\n"
                        "    out \"n=%d hits=%d v=%#x u8=%d u16=%#x miss=%d\", n, hits(0x401000), v, u8(v), u16(v), u32(0x10)\n"
                        "    if n == 1\n"
                        "        out \"one\"\n"
                        "    elif n == 2\n"
                        "        out \"two \" + hex(-n) + \" \" + str(7 // -2) + \" \" + str(-7 % 3) + \" \" + repr(rd(v, 2))\n"
                        "    else\n"
                        "        out \"other %s %d %d\", \"y\" if not n else \"n\", 1 << 4 | 1, ~0 & 0xff\n"
                        "    end\n"
                        "    Eax = Eax + 2\n"
                        "    wr 0x740200, le32(n) + x\"aa bb\"\n"
                        "end\n");
    REQUIRE(s);
    const char *err = NULL;
    REQUIRE(wdbg_spec_load(s, &h, &err));
    put32(f, 0x740004 + 0, 0); /* nothing at Esp+4 in the fake memory for this test: the pointer itself */
    Regs r;
    regs_init(&r);
    r.v[reg("Esp")] = 0x740100 - 4;
    r.v[reg("Eax")] = 40;
    /* u32(Esp+4) = 0x12345678; reading at 0x12345678 misses: 0 */
    for (int i = 0; i < 3; i++) CHECK(wdbg_spec_hit(s, 0x401000, r.p));
    REQUIRE(f->lines.n == 6);
    CHECK_STR(f->lines.v[0], "n=1 hits=1 v=0x12345678 u8=0 u16=0x0 miss=0");
    CHECK_STR(f->lines.v[1], "one");
    CHECK_STR(f->lines.v[3], "two -0x2 -4 2 b''");
    CHECK_STR(f->lines.v[5], "other n 17 255");
    CHECK_EQ_U64(r.v[reg("Eax")], 46);
    CHECK_MEM(f->mem + 0x200, "\x03\0\0\0\xaa\xbb", 6);
}

AVP_TEST(handler_errors_are_logged_and_counted) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = parse("bp 0x10\n out \"before\"\n out \"%d\", nope\n out \"after\"\nend\n"
                        "bp 0x20\n out \"%d\", hits(0x99)\nend\n"
                        "bp 0x30\n out \"%d\", 1 // 0\nend\n");
    REQUIRE(s);
    const char *err = NULL;
    REQUIRE(wdbg_spec_load(s, &h, &err));
    Regs r;
    regs_init(&r);
    CHECK(!wdbg_spec_hit(s, 0x10, r.p));
    CHECK(!wdbg_spec_hit(s, 0x20, r.p));
    CHECK(!wdbg_spec_hit(s, 0x30, r.p));
    REQUIRE(f->lines.n == 4);
    CHECK_STR(f->lines.v[0], "before");
    CHECK_STR(f->lines.v[1], "HANDLER ERR NameError: name 'nope' is not defined (t.wdbg:3, bp 0x10; this handler is stopped)");
    CHECK_STR(f->lines.v[2], "HANDLER ERR KeyError: 153 (t.wdbg:7, bp 0x20; this handler is stopped)");
    CHECK_STR(f->lines.v[3], "HANDLER ERR ZeroDivisionError: integer division or modulo by zero (t.wdbg:10, bp 0x30; this handler is stopped)");
    CHECK_EQ_I64(wdbg_spec_hits(s, 0x10), 1);
}

AVP_TEST(load_errors_and_snap_sym_env) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    const char *err = NULL;
    WdbgSpec *bad = parse("x = undefined_name\n");
    REQUIRE(bad);
    CHECK(!wdbg_spec_load(bad, &h, &err));
    CHECK_CZ(err, "NameError: name 'undefined_name' is not defined (t.wdbg:1)");
    SetEnvironmentVariableW(L"WDBG_TEST_VAR", L"42");
    SetEnvironmentVariableW(L"WDBG_TEST_EMPTY", L"");
    SetEnvironmentVariableW(L"WDBG_TEST_UNSET", NULL);
    WdbgSpec *s = parse("e = int(env(\"WDBG_TEST_VAR\")) + 1\n"
                        "bp 0x10\n"
                        "    snap \"s1\", 0x687000, 0x689000\n"
                        "    snap \"s2\"\n"
                        "    out sym(0x5b88f0)\n"
                        "    out sym(16) + \"|\" + env(\"WDBG_TEST_EMPTY\", \"d\") + \"|\" + env(\"WDBG_TEST_UNSET\", \"d\")\n"
                        "    out \"%d %d %d\", e, len(\"ab\"), len(x\"00 01 02\")\n"
                        "    out env(\"WDBG_TEST_UNSET\")\n"
                        "end\n");
    REQUIRE(s);
    REQUIRE(wdbg_spec_load(s, &h, &err));
    Regs r;
    regs_init(&r);
    CHECK(!wdbg_spec_hit(s, 0x10, r.p));
    REQUIRE(f->snaps.n == 2);
    CHECK_STR(f->snaps.v[0], "s1 0x687000 0x689000");
    CHECK_STR(f->snaps.v[1], "s2 0x687000 0x7c8000");
    REQUIRE(f->lines.n == 4);
    CHECK_STR(f->lines.v[0], "colorinstructions (Coloring.obj)");
    CHECK_STR(f->lines.v[1], "0x10||d");
    CHECK_STR(f->lines.v[2], "43 2 3");
    CHECK_STR(f->lines.v[3], "HANDLER ERR KeyError: 'WDBG_TEST_UNSET' (t.wdbg:8, bp 0x10; this handler is stopped)");
}

/* ---- the shipped specs ---------------------------------------------------------------------------------------- */

static WdbgSpec *load_file(const char *name, WdbgHost *h) {
    AvpArena *a = avp_arena();
    const char *path = avp_sprintf(a, "tools/c/tools/wdbg/specs/%s", name);
    AvpStr text;
    bool read = avp_read_text(a, path, AVP_TM_UTF8, &text);
    CHECK_MSG(read, "cannot read %s", path);
    if (!read) return NULL;
    const char *err = NULL;
    WdbgSpec *s = wdbg_spec_parse(a, path, text, &err);
    CHECK_MSG(s != NULL, "%s", err ? err : "?");
    if (s) CHECK_MSG(wdbg_spec_load(s, h, &err), "%s", err ? err : "?");
    return s;
}

AVP_TEST(shipped_cse_quals) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = load_file("cse_quals.wdbg", &h);
    REQUIRE(s);
    CHECK_EQ_U64(wdbg_spec_bp_count(s), 1);
    CHECK_EQ_U64(wdbg_spec_bp_addr(s, 0), 0x5d4460);
    /* two ENodes: a at 0x741000 (op 5, flags 1, operand at 0x741100 op 9), b at 0x742000 (op 6, flags 0, operand 0) */
    put32(f, 0x740004, 0x741000);
    put32(f, 0x740008, 0x742000);
    f->mem[0x1000] = 5;
    put32(f, 0x741006, 1);
    put32(f, 0x74102a, 0x741100);
    f->mem[0x1100] = 9;
    f->mem[0x2000] = 6;
    Regs r;
    regs_init(&r);
    r.v[reg("Esp")] = 0x740000;
    CHECK(wdbg_spec_hit(s, 0x5d4460, r.p));
    REQUIRE(f->lines.n == 1);
    CHECK_STR(f->lines.v[0], "cmp op=5/6 flags=0x1/0x0  operand op=9/-1");
}

AVP_TEST(shipped_may_alias) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = load_file("may_alias.wdbg", &h);
    REQUIRE(s);
    CHECK_EQ_U64(wdbg_spec_bp_count(s), 2);
    CHECK_EQ_U64(wdbg_spec_bp_addr(s, 0), 0x5c3460);
    CHECK_EQ_U64(wdbg_spec_bp_addr(s, 1), 0x5c3473);
    /* a: alias 0x741200 (kind 1, offset 8, size 4) of object 0x741300 (read-only, name node 0x741400 "foo"); b: none */
    put32(f, 0x740004, 0x741000);
    put32(f, 0x740008, 0x741800);
    put32(f, 0x74101c, 0x741200);
    put32(f, 0x741214, 0x741300);
    put32(f, 0x741218, 8);
    put32(f, 0x74121c, 4);
    f->mem[0x1230] = 1;
    put32(f, 0x74130c, 0x741400);
    f->mem[0x1314] = 1;
    memcpy(f->mem + 0x140a, "foo", 4);
    Regs r;
    regs_init(&r);
    r.v[reg("Esp")] = 0x740000;
    CHECK(wdbg_spec_hit(s, 0x5c3460, r.p));
    r.v[reg("Eax")] = 0x1ff;
    CHECK(wdbg_spec_hit(s, 0x5c3473, r.p));
    REQUIRE(f->lines.n == 1);
    CHECK_STR(f->lines.v[0], "may_alias loc:foo+0x8/4(ro)            none                         -> 255");
}

AVP_TEST(shipped_first_fn_copyprop) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = load_file("first_fn_copyprop.wdbg", &h);
    REQUIRE(s);
    static const u32 want[] = {0x6497c0, 0x46bdac, 0x649840, 0x649910, 0x6498ba, 0x6498d4, 0x649800, 0x649930};
    CHECK_EQ_U64(wdbg_spec_bp_count(s), 8);
    for (size_t i = 0; i < 8; i++) CHECK_EQ_U64(wdbg_spec_bp_addr(s, i), want[i]);
    Regs r;
    regs_init(&r);
    r.v[reg("Esp")] = 0x740000;
    r.v[reg("Ebx")] = 77;
    put32(f, 0x740008, 1);
    put32(f, 0x747940, 3);
    CHECK(wdbg_spec_hit(s, 0x46bdac, r.p));
    CHECK(wdbg_spec_hit(s, 0x6497c0, r.p));
    CHECK(wdbg_spec_hit(s, 0x6497c0, r.p));
    CHECK(wdbg_spec_hit(s, 0x649910, r.p));
    CHECK(wdbg_spec_hit(s, 0x649840, r.p));
    CHECK(wdbg_spec_hit(s, 0x6498d4, r.p));
    REQUIRE(f->lines.n == 6);
    CHECK_STR(f->lines.v[0], "######## fn entry #1");
    CHECK_STR(f->lines.v[1], "=== 6497c0 call#1 flag=1");
    CHECK_STR(f->lines.v[2], "=== 6497c0 call#2 flag=1");
    CHECK_STR(f->lines.v[3], "    -> 6499f0 said NO (node=77)");
    CHECK_STR(f->lines.v[4], "  pred1 node=0 use=1  [747940]=3 [747944]=0");
    CHECK_STR(f->lines.v[5], "    -> SUBSTITUTED operand (747940=1,747944=node)");
}

static void patch_run(const wchar_t *mode, const wchar_t *fn, int hits, const char *line, u32 word) {
    SetEnvironmentVariableW(L"FIXMODE", mode);
    SetEnvironmentVariableW(L"FIXFN", fn);
    WdbgHost h;
    Fake *f = fake_new(&h);
    put32(f, 0x747944, 0x5a5a5a5a);
    WdbgSpec *s = load_file("first_fn_patch.wdbg", &h);
    REQUIRE(s);
    CHECK_EQ_U64(wdbg_spec_bp_count(s), 1);
    Regs r;
    regs_init(&r);
    for (int i = 0; i < hits; i++) CHECK(wdbg_spec_hit(s, 0x46bdac, r.p));
    if (line) {
        REQUIRE(f->lines.n == 1);
        CHECK_STR(f->lines.v[0], line);
    } else {
        CHECK_EQ_U64(f->lines.n, 0);
    }
    u32 got;
    memcpy(&got, f->mem + 0x7944, 4);
    CHECK_EQ_U64(got, word);
}

AVP_TEST(shipped_first_fn_patch_env) {
    patch_run(NULL, NULL, 3, "patched [747944] at fn entry #1 mode=fix", 0xffffffff);
    patch_run(L"break", NULL, 3, "patched [747944] at fn entry #1 mode=break", 0);
    patch_run(L"fix", L"2", 3, "patched [747944] at fn entry #2 mode=fix", 0xffffffff);
    patch_run(L"other", L"2", 2, "patched [747944] at fn entry #2 mode=other", 0);
    patch_run(L"fix", L"5", 3, NULL, 0x5a5a5a5a);
    SetEnvironmentVariableW(L"FIXMODE", NULL);
    SetEnvironmentVariableW(L"FIXFN", NULL);
}

/* ---- compiler builds ---------------------------------------------------------------------------------------- */

AVP_TEST(shipped_iro_gate_follows_the_build) {
    static const struct {
        const char *build;
        u32 bp[5];
    } builds[2] = {{"Wii/1.0", {0x591c60, 0x591ccc, 0x5e1b4f, 0x5e1a70, 0x5e1aab}},
                   {"Wii/1.0a", {0x58d730, 0x58d79c, 0x5dcd5f, 0x5dcc80, 0x5dccbb}}};
    for (int b = 0; b < 2; b++) {
        g_build = builds[b].build;
        WdbgHost h;
        fake_new(&h);
        WdbgSpec *s = load_file("iro_gate.wdbg", &h);
        REQUIRE(s);
        REQUIRE(wdbg_spec_bp_count(s) == 5);
        for (size_t i = 0; i < 5; i++) CHECK_MSG(wdbg_spec_bp_addr(s, i) == builds[b].bp[i], "%s bp %zu", g_build, i);
    }
    g_build = "Wii/1.0";
}

AVP_TEST(shipped_specs_refuse_another_build) {
    static const char *const specs[] = {"cse_quals.wdbg", "first_fn_copyprop.wdbg", "first_fn_patch.wdbg",
                                        "iro_gate.wdbg", "may_alias.wdbg"};
    g_build = "Wii/1.7";
    for (size_t i = 0; i < 5; i++) {
        AvpArena *a = avp_arena();
        const char *path = avp_sprintf(a, "tools/c/tools/wdbg/specs/%s", specs[i]);
        AvpStr text;
        REQUIRE(avp_read_text(a, path, AVP_TM_UTF8, &text));
        const char *err = NULL;
        WdbgSpec *s = wdbg_spec_parse(a, path, text, &err);
        REQUIRE(s);
        WdbgHost h;
        fake_new(&h);
        CHECK(!wdbg_spec_load(s, &h, &err));
        CHECK_MSG(err && strstr(err, "BuildError: this spec's addresses are for compiler build Wii/1.0") &&
                      strstr(err, "this run compiles with Wii/1.7") && strstr(err, specs[i]),
                  "%s: %s", specs[i], err ? err : "(loaded)");
    }
    g_build = "Wii/1.0";
}

AVP_TEST(addr_and_compiler_in_the_language) {
    g_build = "Wii/1.0a";
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = parse("out compiler()\nout hex(addr(\"IRO_Optimizer\") + 0x6c)\n"
                        "bp addr(\"IRO_Optimizer\")\nend\n");
    const char *err = NULL;
    REQUIRE(wdbg_spec_load(s, &h, &err));
    REQUIRE(f->lines.n == 2);
    CHECK_STR(f->lines.v[0], "Wii/1.0a");
    CHECK_STR(f->lines.v[1], "0x58d79c");
    CHECK(wdbg_spec_has_bp(s, 0x58d730));
    s = parse("x = addr(\"NoSuchFunction\")\n");
    CHECK(!wdbg_spec_load(s, &h, &err));
    CHECK(strstr(err, "LookupError: compiler build Wii/1.0a has no function named 'NoSuchFunction'") &&
          strstr(err, "t.wdbg:1"));
    g_build = "Wii/1.0";
}

AVP_TEST(a_failing_handler_reports_once_and_stops) {
    WdbgHost h;
    Fake *f = fake_new(&h);
    WdbgSpec *s = parse("bp 0x10\n out \"hit\"\n out \"%d\", missing\nend\n");
    const char *err = NULL;
    REQUIRE(wdbg_spec_load(s, &h, &err));
    Regs r;
    regs_init(&r);
    CHECK(!wdbg_spec_hit(s, 0x10, r.p));
    CHECK(!wdbg_spec_hit(s, 0x10, r.p));
    CHECK(!wdbg_spec_hit(s, 0x10, r.p));
    REQUIRE(f->lines.n == 2);
    CHECK_STR(f->lines.v[0], "hit");
    CHECK_STR(f->lines.v[1], "HANDLER ERR NameError: name 'missing' is not defined (t.wdbg:3, bp 0x10; this handler is stopped)");
    CHECK_EQ_I64(wdbg_spec_hits(s, 0x10), 3);
}

AVP_TEST_MAIN("wdbg")
