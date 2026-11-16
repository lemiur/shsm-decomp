/* Unit tests of tools/codecmp.py's logic (ported from tools/test_codecmp.py) on synthetic ELF objects, listings and
 * fake projects (symbols.txt, splits.txt, main.dol) written to a temporary folder under build/tmp. The shared
 * library's own test, tools/c/tests/cc_codecmp_test.c, already covers parse_row operands, generated names, the
 * self-recursive call, a raw operand by value (and by wrong bytes), a wrong callee, a wrong global, a branch target
 * and a non-ELF file; those are not repeated here, except where the Python test asserts more than it does.
 *
 * Not ported: test_project_dol_closes_its_file. It checks that Python's Project.dol raises no ResourceWarning for an
 * unclosed file object; C has no such artefact (cc_project_dol reads the file in one call).
 *
 * test_not_an_elf is covered by cc_codecmp_test.c (not_an_elf). */
#include <stdlib.h>
#include "avp/avp.h"
#include "avp/test.h"
#include "cc_compare.h"
#include "codecmp.h"

#define N(a) (sizeof(a) / sizeof((a)[0]))

/* ---- synthetic ELF objects --------------------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    u32 type;
    const u8 *data;
    size_t n; /* for SHT_NOBITS the size only */
} TSec;
typedef struct {
    const char *name;
    u32 value, size;
    u8 bind, type;
    u16 shndx;
} TSym;
typedef struct {
    u32 sec, off, type, sym;
    i32 addend;
} TRel;

static void put32(AvpBuf *b, u32 v) {
    char c[4] = {(char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 4);
}
static void put16(AvpBuf *b, u16 v) {
    char c[2] = {(char)(v >> 8), (char)v};
    avp_buf_putn(b, c, 2);
}

/* A 32-bit big-endian relocatable ELF: sections 1..nsec as given, then one .rela.<name> per section that has
 * relocations (in section order), .symtab, .strtab, .shstrtab. Symbol indices are 1..nsyms (0 is the null symbol). */
static AvpStr build_elf(const TSec *secs, size_t nsec, const TSym *syms, size_t nsyms, const TRel *rels, size_t nrels) {
    AvpArena *a = avp_arena();
    AvpBuf strtab = {0}, symtab = {0}, shstr = {0}, body = {0}, hdrs = {0};
    avp_buf_putc(&strtab, 0);
    for (int k = 0; k < 16; k++) avp_buf_putc(&symtab, 0);
    for (size_t k = 0; k < nsyms; k++) {
        put32(&symtab, (u32)strtab.n);
        avp_buf_putn(&strtab, syms[k].name, strlen(syms[k].name) + 1);
        put32(&symtab, syms[k].value);
        put32(&symtab, syms[k].size);
        avp_buf_putc(&symtab, (char)((syms[k].bind << 4) | syms[k].type));
        avp_buf_putc(&symtab, 0);
        put16(&symtab, syms[k].shndx);
    }
    /* section list: user sections, rela sections, symtab, strtab, shstrtab */
    size_t nrela = 0;
    bool has[32] = {0};
    for (size_t k = 0; k < nrels; k++) has[rels[k].sec] = true;
    for (size_t s = 1; s <= nsec; s++) nrela += has[s];
    u32 symtab_idx = (u32)(1 + nsec + nrela);
    AvpBuf relabuf[32];
    memset(relabuf, 0, sizeof relabuf);
    avp_buf_putc(&shstr, 0);
    u32 pos = 52;
    for (int k = 0; k < 40; k++) avp_buf_putc(&hdrs, 0); /* the null section header */
    typedef struct {
        u32 name, type, off, size, link, info, ent;
    } H;
    H h[64];
    size_t nh = 0;
    for (size_t s = 0; s < nsec; s++) {
        h[nh].name = (u32)shstr.n;
        avp_buf_putn(&shstr, secs[s].name, strlen(secs[s].name) + 1);
        h[nh].type = secs[s].type;
        h[nh].off = pos;
        h[nh].size = (u32)secs[s].n;
        h[nh].link = h[nh].info = h[nh].ent = 0;
        if (secs[s].type != 8) {
            avp_buf_putn(&body, secs[s].data, secs[s].n);
            pos += (u32)secs[s].n;
        }
        nh++;
    }
    for (size_t s = 1; s <= nsec; s++) {
        if (!has[s]) continue;
        for (size_t k = 0; k < nrels; k++) {
            if (rels[k].sec != s) continue;
            put32(&relabuf[s], rels[k].off);
            put32(&relabuf[s], (rels[k].sym << 8) | rels[k].type);
            put32(&relabuf[s], (u32)rels[k].addend);
        }
        char *nm = avp_sprintf(a, ".rela%s", secs[s - 1].name);
        h[nh].name = (u32)shstr.n;
        avp_buf_putn(&shstr, nm, strlen(nm) + 1);
        h[nh].type = 4;
        h[nh].off = pos;
        h[nh].size = (u32)relabuf[s].n;
        h[nh].link = symtab_idx;
        h[nh].info = (u32)s;
        h[nh].ent = 12;
        avp_buf_putn(&body, relabuf[s].p, relabuf[s].n);
        pos += (u32)relabuf[s].n;
        nh++;
    }
    const struct {
        const char *name;
        u32 type;
        AvpBuf *b;
        u32 link, info, ent;
    } tail[3] = {{".symtab", 2, &symtab, symtab_idx + 1, 1, 16},
                 {".strtab", 3, &strtab, 0, 0, 0},
                 {".shstrtab", 3, &shstr, 0, 0, 0}};
    for (int t = 0; t < 3; t++) {
        h[nh].name = (u32)shstr.n;
        avp_buf_putn(&shstr, tail[t].name, strlen(tail[t].name) + 1);
        h[nh].type = tail[t].type;
        h[nh].off = pos;
        h[nh].link = tail[t].link;
        h[nh].info = tail[t].info;
        h[nh].ent = tail[t].ent;
        h[nh].size = (u32)tail[t].b->n; /* .shstrtab: fixed below (its own name is the last thing added) */
        if (t < 2) {
            avp_buf_putn(&body, tail[t].b->p, tail[t].b->n);
            pos += (u32)tail[t].b->n;
        }
        nh++;
    }
    /* .shstrtab's bytes are complete now */
    h[nh - 1].off = pos;
    h[nh - 1].size = (u32)shstr.n;
    avp_buf_putn(&body, shstr.p, shstr.n);
    pos += (u32)shstr.n;
    for (size_t k = 0; k < nh; k++) {
        put32(&hdrs, h[k].name);
        put32(&hdrs, h[k].type);
        put32(&hdrs, 0);
        put32(&hdrs, 0);
        put32(&hdrs, h[k].off);
        put32(&hdrs, h[k].size);
        put32(&hdrs, h[k].link);
        put32(&hdrs, h[k].info);
        put32(&hdrs, 4);
        put32(&hdrs, h[k].ent);
    }
    AvpBuf f = {0};
    for (int k = 0; k < 52; k++) avp_buf_putc(&f, 0);
    avp_buf_putn(&f, body.p, body.n);
    u32 shoff = (u32)f.n;
    avp_buf_putn(&f, hdrs.p, hdrs.n);
    u8 *p = (u8 *)f.p;
    memcpy(p, "\x7f" "ELF\x01\x02\x01", 7);
    p[17] = 1;  /* ET_REL */
    p[19] = 20; /* EM_PPC */
    p[32] = (u8)(shoff >> 24), p[33] = (u8)(shoff >> 16), p[34] = (u8)(shoff >> 8), p[35] = (u8)shoff;
    p[47] = 40;                /* shentsize */
    p[49] = (u8)(nh + 1);      /* shnum (the null header included) */
    p[51] = (u8)nh;            /* shstrndx: the last section */
    return avp_strn(f.p, f.n);
}

/* ---- listings ---------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *word; /* eight hex digits */
    const char *text;
} TI;

/* A dtk-style listing block of one function; the addresses start at base and advance by four. */
static AvpStr listing(const char *name, i64 base, const TI *ins, size_t n) {
    AvpBuf b = {0};
    avp_buf_printf(&b, ".fn %s, global\n", name);
    for (size_t k = 0; k < n; k++) {
        i64 addr = base + 4 * (i64)k;
        const char *w = ins[k].word;
        avp_buf_printf(&b, "/* %08X %08X  %c%c %c%c %c%c %c%c */\t%s\n", (u32)addr, (u32)(addr + 0x40), w[0], w[1],
                       w[2], w[3], w[4], w[5], w[6], w[7], ins[k].text);
    }
    avp_buf_printf(&b, ".endfn %s\n", name);
    return avp_strn(b.p, b.n);
}

static const CcFn *one(const AvpMap *fns, const char *name) {
    AvpVal *v = avp_map_get(fns, avp_str(name));
    AVP_ASSERT(v);
    return v->p;
}

static CcFn *retail_fn(const char *name, const TI *ins, size_t n) {
    AvpArena *a = avp_arena();
    AvpMap *m = cc_parse_listing(a, listing(name, 0x80001000, ins, n), NULL);
    return (CcFn *)one(m, name);
}

typedef struct {
    const TI *ins;
    size_t nins;
    const TSym *syms; /* after the function's own symbol, which is index 1 */
    size_t nsyms;
    const TRel *rels;
    size_t nrels;
    const u8 *data; /* the .data section (section 2); NULL: sixteen zero bytes */
    size_t ndata;
    const TSec *extra; /* sections 3... */
    size_t nextra;
} CSpec;

/* compiled_fn: a compiled function placed at .text offset 0, with .data as section 2. */
static CcFn *compiled_fn(const char *name, const CSpec *s) {
    AvpArena *a = avp_arena();
    static const u8 zero16[16] = {0};
    size_t n = 4 * s->nins;
    u8 *text = avp_alloc(a, n ? n : 1);
    memset(text, 0, n);
    TSec secs[8];
    AVP_ASSERT(s->nextra + 2 <= N(secs));
    secs[0] = (TSec){".text", 1, text, n};
    secs[1] = (TSec){".data", 1, s->data ? s->data : zero16, s->data ? s->ndata : 16};
    for (size_t k = 0; k < s->nextra; k++) secs[2 + k] = s->extra[k];
    TSym syms[16];
    AVP_ASSERT(s->nsyms + 1 <= N(syms));
    syms[0] = (TSym){name, 0, (u32)n, 1, CC_STT_FUNC, 1};
    for (size_t k = 0; k < s->nsyms; k++) syms[1 + k] = s->syms[k];
    AvpStr bytes = build_elf(secs, 2 + s->nextra, syms, 1 + s->nsyms, s->rels, s->nrels);
    const char *why;
    CcElf *elf = cc_elf_parse(a, bytes.p, bytes.n, &why);
    AVP_ASSERT(elf);
    AvpMap *m = cc_parse_listing(a, listing(name, 0, s->ins, s->nins), elf);
    return (CcFn *)one(m, name);
}

#define INS(arr) .ins = (arr), .nins = N(arr)
#define SYMS(arr) .syms = (arr), .nsyms = N(arr)
#define RELS(arr) .rels = (arr), .nrels = N(arr)

/* ---- fake projects ----------------------------------------------------------------------------------------------- */

static const char *g_root;
static int g_project_no;

static void remove_root(void) {
    if (g_root) avp_tempdir_cleanup(g_root);
}

static const char *SYMBOLS =
    "OSDisableInterrupts = .text:0x80001000; // type:function size:0x14 scope:global\n"
    "OSEnableInterrupts = .text:0x80001020; // type:function size:0x14 scope:global\n"
    "fn_80002000__Fv = .text:0x80002000; // type:function size:0x10\n"
    "Callback = .text:0x80003000; // type:function size:0xC scope:local\n"
    "Callback = .text:0x80004000; // type:function size:0xC scope:local\n"
    "lbl_80500010 = .sbss:0x80500010; // type:object size:0x4 scope:global\n"
    "lbl_80500014 = .sbss:0x80500014; // type:object size:0x4 scope:global\n";

static const char *SPLITS_HEAD = "Sections:\n\t.text type:code align:8\n\n";

static void W32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24), p[1] = (u8)(v >> 16), p[2] = (u8)(v >> 8), p[3] = (u8)v;
}

/* fake_dol(addr, payload, bss=(0x80400000, 0x100)): one initialised section at addr holding the payload. */
static AvpStr fake_dol(u32 addr, const u8 *payload, size_t n) {
    u8 hdr[0x100] = {0};
    W32(hdr + 4 * 7, 0x100);
    W32(hdr + 72 + 4 * 7, addr);
    W32(hdr + 144 + 4 * 7, (u32)n);
    W32(hdr + 216, 0x80400000u);
    W32(hdr + 220, 0x100);
    AvpBuf b = {0};
    avp_buf_putn(&b, hdr, sizeof hdr);
    avp_buf_putn(&b, payload, n);
    return avp_strn(b.p, b.n);
}

/* A project in its own folder: symbols (NULL: SYMBOLS), the splits.txt blocks after the header (NULL: none), the DOL
 * (NULL: no main.dol). */
static CcProject *new_project(const char *symbols, const char *splits_blocks, const AvpStr *dol) {
    AvpArena *a = avp_arena();
    if (!g_root) {
        g_root = avp_sprintf(a, "%s\\build\\tmp\\codecmp_test_%u", avp_root(), avp_getpid());
        AVP_ASSERT(avp_makedirs(g_root, true));
        atexit(remove_root);
    }
    char *root = avp_sprintf(a, "%s\\p%d", g_root, ++g_project_no);
    AVP_ASSERT(avp_makedirs(avp_sprintf(a, "%s\\config\\R5WEA4", root), true));
    AVP_ASSERT(avp_makedirs(avp_sprintf(a, "%s\\orig\\R5WEA4\\sys", root), true));
    AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\config\\R5WEA4\\symbols.txt", root), avp_str(symbols ? symbols : SYMBOLS),
                              AVP_TM_UTF8));
    AVP_ASSERT(avp_write_text(avp_sprintf(a, "%s\\config\\R5WEA4\\splits.txt", root),
                              avp_str(avp_sprintf(a, "%s%s", SPLITS_HEAD, splits_blocks ? splits_blocks : "")),
                              AVP_TM_UTF8));
    if (dol) AVP_ASSERT(avp_write_file(avp_sprintf(a, "%s\\orig\\R5WEA4\\sys\\main.dol", root), dol->p, dol->n));
    return cc_project_new(root);
}

/* TargetTests.ctx: Context("game/Foo", fake_project(SYMBOLS, dol=..., splits=...)). */
static CcContext *ctx_new(const AvpStr *dol, const char *splits_blocks) {
    return cc_context_new(new_project(NULL, splits_blocks, dol), AVP_S("game/Foo"));
}

static CcFnResult *cmp(const CcFn *r, CcFn *c, CcContext *ctx) {
    return cc_compare_function(avp_arena(), r, c, ctx, true);
}

static bool has(AvpStr s, const char *needle) {
    return strstr(avp_cz(avp_arena(), s), needle) != NULL;
}

static const CcSecRange *sec_of(CcUnitSecs u, const char *name) {
    for (size_t k = 0; k < u.n; k++)
        if (avp_str_eqz(u.v[k].name, name)) return &u.v[k];
    return NULL;
}

/* ---- ElfTests ---------------------------------------------------------------------------------------------------- */

AVP_TEST(elf_parse_sections_symbols_relocs) {
    AvpArena *a = avp_arena();
    static const u8 text[12] = {0x48, 0, 0, 1, 0x38, 0x60, 0, 0, 0x3C, 0x60, 0, 0};
    static const u8 data[8] = {0};
    TSec secs[] = {{".text", 1, text, 12}, {".data", 1, data, 8}};
    TSym syms[] = {{"Foo", 0, 12, 1, CC_STT_FUNC, 1}, {"ext", 0, 0, 1, 0, 0}};
    TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}, {1, 10, CC_R_ADDR16_HA, 1, 4}};
    AvpStr bytes = build_elf(secs, 2, syms, 2, rels, 2);
    const char *why;
    CcElf *elf = cc_elf_parse(a, bytes.p, bytes.n, &why);
    REQUIRE(elf);
    CHECK_STR(elf->sections[0].name, "");
    CHECK_STR(elf->sections[1].name, ".text");
    CHECK_STR(elf->sections[2].name, ".data");
    CHECK_STR(elf->symbols[1].name, "Foo");
    CHECK_EQ_I64(elf->symbols[1].type, CC_STT_FUNC);
    CHECK_EQ_I64(elf->symbols[2].shndx, 0);
    const CcReloc *r = cc_elf_reloc_at(elf, 1, 0);
    REQUIRE(r);
    CHECK_CZ(cc_reloc_type_name(a, r->type), "REL24");
    CHECK_STR(elf->symbols[r->sym].name, "ext");
    CHECK_EQ_I64(r->addend, 0);
    r = cc_elf_reloc_at(elf, 1, 8); /* a 16-bit half sits at instruction offset + 2 */
    REQUIRE(r);
    CHECK_CZ(cc_reloc_type_name(a, r->type), "ADDR16_HA");
    CHECK_STR(elf->symbols[r->sym].name, "Foo");
    CHECK_EQ_I64(r->addend, 4);
    CHECK(cc_elf_reloc_at(elf, 1, 4) == NULL);
}

/* ---- ProjectDataTests -------------------------------------------------------------------------------------------- */

AVP_TEST(project_parse_symbols) {
    AvpArena *a = avp_arena();
    AvpMap *names;
    CcSpan *spans;
    size_t nspans;
    cc_parse_symbols(a, avp_str(SYMBOLS), &names, &spans, &nspans);
    AvpVal *v = avp_map_get(names, AVP_S("OSDisableInterrupts"));
    REQUIRE(v);
    const AvpI64Vec *vec = v->p;
    REQUIRE(vec->n == 1);
    CHECK_EQ_I64(vec->v[0], 0x80001000);
    v = avp_map_get(names, AVP_S("Callback"));
    REQUIRE(v);
    vec = v->p;
    REQUIRE(vec->n == 2);
    CHECK_EQ_I64(vec->v[0], 0x80003000);
    CHECK_EQ_I64(vec->v[1], 0x80004000);
    REQUIRE(nspans >= 1);
    CHECK_EQ_I64(spans[0].addr, 0x80001000);
    CHECK_EQ_I64(spans[0].size, 0x14);
    CHECK_STR(spans[0].name, "OSDisableInterrupts");
}

AVP_TEST(project_describe) {
    AvpArena *a = avp_arena();
    CcProject *p = new_project(NULL, NULL, NULL);
    CHECK_STR(cc_project_describe(a, p, 0x80001000), "OSDisableInterrupts");
    CHECK_STR(cc_project_describe(a, p, 0x80001004), "OSDisableInterrupts+0x4");
    CHECK_STR(cc_project_describe(a, p, 0x70000000), "0x70000000");
}

AVP_TEST(project_embedded_address) {
    i64 v;
    CHECK(cc_embedded_address(AVP_S("fn_8009CB1C__12CBaseSpawnerFR7unk_Msg"), &v) && v == 0x8009CB1C);
    CHECK(cc_embedded_address(AVP_S("lbl_80500010"), &v) && v == 0x80500010);
    CHECK(cc_embedded_address(AVP_S("jumptable_803D8780"), &v) && v == 0x803D8780);
    CHECK(cc_embedded_address(AVP_S("@stringBase0_803D98F8"), &v) && v == 0x803D98F8);
    CHECK(cc_embedded_address(AVP_S("gap_03_80211E28_text"), &v) && v == 0x80211E28);
    CHECK(!cc_embedded_address(AVP_S("__vt__12unk_801A92A8"), &v)); /* a class's constructor address */
    CHECK(!cc_embedded_address(AVP_S("OSDisableInterrupts"), &v));
}

AVP_TEST(project_parse_splits) {
    AvpArena *a = avp_arena();
    AvpMap *sp = cc_parse_splits(a, AVP_S("Sections:\n\t.text type:code align:8\n\ngame/Foo.cpp:\n"
                                          "\t.text       start:0x800067A0 end:0x80006C7C\n"
                                          "\t.bss start:0x80457480 end:0x80457588\n"));
    AvpVal *v = avp_map_get(sp, AVP_S("game/Foo.cpp"));
    REQUIRE(v);
    const CcUnitSecs *u = v->p;
    const CcSecRange *t = sec_of(*u, ".text");
    REQUIRE(t);
    CHECK_EQ_I64(t->start, 0x800067A0);
    CHECK_EQ_I64(t->end, 0x80006C7C);
    CcProject *p = new_project(NULL, "game/Foo.cpp:\n\t.text       start:0x800067A0 end:0x80006C7C\n"
                                     "\t.bss start:0x80457480 end:0x80457588\n", NULL);
    const CcSecRange *b = sec_of(cc_project_unit_sections(p, AVP_S("main/game/Foo")), ".bss");
    REQUIRE(b);
    CHECK_EQ_I64(b->start, 0x80457480);
    CHECK_EQ_I64(b->end, 0x80457588);
    CHECK_EQ_U64(cc_project_unit_sections(p, AVP_S("game/Nope")).n, 0);
}

AVP_TEST(project_dol_read) {
    static const u8 payload[5] = {1, 2, 3, 4, 5};
    AvpStr dol = fake_dol(0x80300000, payload, sizeof payload);
    const CcDol *d = cc_project_dol(new_project(NULL, NULL, &dol));
    REQUIRE(d);
    AvpStr out;
    REQUIRE(cc_dol_read(d, 0x80300001, 3, &out));
    CHECK_EQ_U64(out.n, 3);
    CHECK_MEM(out.p, "\x02\x03\x04", 3);
    CHECK(!cc_dol_read(d, 0x80300003, 4, &out)); /* runs past the section */
    CHECK(!cc_dol_read(d, 0x80400000, 4, &out)); /* bss: no bytes */
    CHECK(cc_dol_in_bss(d, 0x80400010));
}

/* ---- ListingTests ------------------------------------------------------------------------------------------------ */

AVP_TEST(listing_retail_operands) {
    const char *text =
        ".fn alpha, global\n"
        "/* 80001000 80001040  38 60 00 01 */\tli r3, 1\n"
        ".L_80001004:\n"
        "/* 80001004 80001044  4B FF FF FC */\tb .L_80001004\n"
        "/* 80001008 80001048  48 00 0F F9 */\tbl fn_80002000__Fv\n"
        "/* 8000100C 8000104C  3C 60 80 50 */\tlis r3, lbl_80500010@ha\n"
        "/* 80001010 80001050  80 03 00 04 */\tlwz r0, lbl_80500010+0x4@sda21(r0)\n"
        "/* 80001014 80001054  80 03 00 00 */\tlwz r0, \"@1790\"@sda21(r0)\n"
        "/* 80001018 80001058  4E 80 00 20 */\tblr\n"
        ".endfn alpha\n";
    AvpMap *m = cc_parse_listing(avp_arena(), avp_str(text), NULL);
    const CcFn *fn = one(m, "alpha");
    REQUIRE(fn->nrows == 7);
    const char *want[] = {"li r3, 1", "b L", "bl SYM", "lis r3, SYM@ha", "lwz r0, SYM@sda21(r0)",
                          "lwz r0, SYM@sda21(r0)", "blr"};
    for (size_t k = 0; k < 7; k++) CHECK_STR(fn->rows[k].norm, want[k]);
    CHECK(fn->rows[1].has_label);
    CHECK_EQ_I64(fn->rows[1].label, 0x80001004);
    CHECK_EQ_I64(fn->rows[1].bidx, 1);
    REQUIRE(fn->rows[2].has_ref);
    CHECK_STR(fn->rows[2].ref.name, "fn_80002000__Fv");
    CHECK_STR(fn->rows[2].ref.kind, "rel24");
    REQUIRE(fn->rows[4].has_ref);
    CHECK_STR(fn->rows[4].ref.name, "lbl_80500010");
    CHECK_EQ_I64(fn->rows[4].ref.off, 4);
    CHECK_STR(fn->rows[4].ref.kind, "sda21");
    REQUIRE(fn->rows[5].has_ref);
    CHECK_STR(fn->rows[5].ref.name, "@1790");
}

AVP_TEST(listing_quoted_function_names_and_data_lines) {
    const char *text = ".fn \"@36@fn_8009CB1C__12CBaseSpawnerFR7unk_Msg\", weak\n"
                       "/* 800878C0 000838C0  38 63 FF F8 */\taddi r3, r3, -0x8\n"
                       "/* 800878C4 000838C4  00 00 00 00 */\t.4byte 0x00000000 /* invalid */\n"
                       ".endfn x\n";
    AvpMap *m = cc_parse_listing(avp_arena(), avp_str(text), NULL);
    CHECK_EQ_U64(avp_map_len(m), 1);
    AvpMapIter it = {0};
    REQUIRE(avp_map_next(m, &it));
    CHECK_STR(it.key, "@36@fn_8009CB1C__12CBaseSpawnerFR7unk_Msg");
    const CcFn *fn = one(m, "@36@fn_8009CB1C__12CBaseSpawnerFR7unk_Msg");
    CHECK_EQ_U64(fn->nrows, 1);
}

AVP_TEST(listing_unrelocated_loop_to_own_start_becomes_a_local_branch) {
    /* `bne <own name>` (retail) and `bne .text` (compiled) are the same loop branch: displacement -4 */
    const TI ins[] = {{"38600001", "li r3, 1"}, {"4082FFFC", "bne f"}, {"4E800020", "blr"}};
    const CcFn *fn = retail_fn("f", ins, N(ins));
    CHECK_STR(fn->rows[1].norm, "bne L");
    CHECK(fn->rows[1].has_label);
    CHECK_EQ_I64(fn->rows[1].label, 0x80001000);
    CHECK_EQ_I64(fn->rows[1].bidx, 0);
}

AVP_TEST(listing_decode_branch) {
    i64 t;
    CHECK(cc_decode_branch(0x4BFFFFFC, 0x100, &t) && t == 0xFC); /* b -4 */
    CHECK(cc_decode_branch(0x48000008, 0x100, &t) && t == 0x108);
    CHECK(!cc_decode_branch(0x48000002, 0x100, &t));              /* absolute */
    CHECK(cc_decode_branch(0x4182FFF8, 0x100, &t) && t == 0xF8); /* beq -8 */
    CHECK(!cc_decode_branch(0x4E800020, 0x100, &t));              /* blr */
}

/* ---- TargetTests ------------------------------------------------------------------------------------------------- */

AVP_TEST(same_function_by_name_and_by_embedded_address) {
    const TI ri[] = {{"48001001", "bl fn_80002000__Fv"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    /* the compiled call carries another mangling of the same address */
    const TI ci[] = {{"48000001", "bl fn_80002000__FRi"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"fn_80002000__FRi", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK(cc_fn_result_exact(res));
    CHECK_EQ_U64(res->checked_targets, 1);
    CHECK_EQ_U64(res->unresolved.n, 0);
}

AVP_TEST(self_recursive_call_is_the_same_on_both_sides) {
    /* stateReady (revolution/dvd/dvd) calls itself: retail `bl stateReady` carries no relocation (the assembler
     * resolved it, word 4BFFFF75 = bl -0x8C back to the start); the compiled object keeps an R_PPC_REL24 against its
     * own symbol. Both are the same local branch to row 0. */
    const TI ri[] = {{"9421FFF0", "stwu r1, -0x10(r1)"}, {"7C0802A6", "mflr r0"}, {"4BFFFFF9", "bl stateReady"},
                     {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("stateReady", ri, N(ri));
    const TI ci[] = {ri[0], ri[1], {"48000001", "bl stateReady"}, ri[3]};
    const TRel rels[] = {{1, 8, CC_R_REL24, 1, 0}};
    CcFn *comp = compiled_fn("stateReady", &(CSpec){INS(ci), RELS(rels)});
    CHECK_STR(retail->rows[2].norm, "bl L");
    CHECK_STR(comp->rows[2].norm, "bl L");
    CHECK_EQ_I64(comp->rows[2].bidx, 0);
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK(cc_fn_result_exact(res));
    CHECK_EQ_U64(res->edit, 0);
    /* a tail branch relocated against the .text section symbol at the function's own offset is the same */
    const TI i2[] = {{"38600001", "li r3, 1"}, {"48000000", "b .text"}};
    const TSym s2[] = {{".text", 0, 0, 0, CC_STT_SECTION, 1}};
    const TRel r2[] = {{1, 4, CC_R_REL24, 2, 0}};
    CcFn *comp2 = compiled_fn("g", &(CSpec){INS(i2), SYMS(s2), RELS(r2)});
    CHECK_STR(comp2->rows[1].norm, "b L");
    /* a call to another function of the same section stays a call */
    const TI i3[] = {{"48000001", "bl other"}, {"4E800020", "blr"}};
    const TSym s3[] = {{"other", 0x40, 8, 1, CC_STT_FUNC, 1}};
    const TRel r3[] = {{1, 0, CC_R_REL24, 2, 0}};
    CcFn *comp3 = compiled_fn("h", &(CSpec){INS(i3), SYMS(s3), RELS(r3)});
    CHECK_STR(comp3->rows[0].norm, "bl SYM");
    CHECK(comp3->rows[0].has_ref);
}

AVP_TEST(wrong_callee_is_flagged_although_the_text_is_equal) {
    const TI ri[] = {{"48001001", "bl OSDisableInterrupts"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"48000001", "bl OSEnableInterrupts"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"OSEnableInterrupts", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK_EQ_U64(res->edit, 0);
    CHECK_EQ_U64(res->npositional, 0);
    CHECK(!cc_fn_result_exact(res));
    REQUIRE(res->targets.n == 1);
    CHECK(has(res->targets.v[0].detail, "0x80001000"));
    CHECK(has(res->targets.v[0].detail, "0x80001020"));
    /* without a context (the old behaviour) the pair is exact */
    CHECK(cc_fn_result_exact(cc_compare_function(avp_arena(), retail, comp, NULL, true)));
    CHECK(cc_fn_result_exact(cc_compare_function(avp_arena(), retail, comp, ctx_new(NULL, NULL), false)));
}

AVP_TEST(wrong_global_and_wrong_offset) {
    const TI ri[] = {{"3C600000", "lis r3, lbl_80500010+0x4@ha"}, {"80030000", "lwz r0, lbl_80500010+0x4@l(r3)"},
                     {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"3C600000", "lis r3, g@ha"}, {"80030000", "lwz r0, g@l(r3)"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"lbl_80500014", 0, 4, 1, 1, 0}};
    /* same global, same offset (a different spelling: lbl_80500014 + 0) -> match */
    const TRel same[] = {{1, 2, CC_R_ADDR16_HA, 2, 0}, {1, 6, CC_R_ADDR16_LO, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(same)});
    CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(NULL, NULL))));
    /* other global */
    const TRel other[] = {{1, 2, CC_R_ADDR16_HA, 2, 8}, {1, 6, CC_R_ADDR16_LO, 2, 8}};
    comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(other)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK_EQ_U64(res->targets.n, 2);
}

AVP_TEST(same_name_is_the_same_symbol_even_when_symbols_txt_lacks_it) {
    const TI ri[] = {{"3C600000", "lis r3, _stack_addr@ha"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TSym syms[] = {{"_stack_addr", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 2, CC_R_ADDR16_HA, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ri), SYMS(syms), RELS(rels)});
    CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(NULL, NULL))));
}

AVP_TEST(unknown_names_are_unresolved_not_matched) {
    const TI ri[] = {{"48001001", "bl fn_80009999"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"48000001", "bl Renamed"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"Renamed", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK_EQ_U64(res->targets.n, 0);
    REQUIRE(res->unresolved.n == 1);
    CHECK(has(res->unresolved.v[0].detail, "Renamed"));
    /* the retail name nobody knows is unresolved too */
    const TI ri2[] = {{"48001001", "bl Mystery"}, {"4E800020", "blr"}};
    retail = retail_fn("f", ri2, N(ri2));
    const TI ci2[] = {{"48000001", "bl OSDisableInterrupts"}, {"4E800020", "blr"}};
    const TSym s2[] = {{"OSDisableInterrupts", 0, 0, 1, 0, 0}};
    comp = compiled_fn("f", &(CSpec){INS(ci2), SYMS(s2), RELS(rels)});
    res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK_EQ_U64(res->targets.n, 0);
    REQUIRE(res->unresolved.n == 1);
    CHECK(has(res->unresolved.v[0].detail, "Mystery"));
}

AVP_TEST(duplicate_retail_names_resolve_inside_the_unit) {
    const char *sp = "game/Foo.cpp:\n\t.text start:0x80004000 end:0x80004100\n";
    const TI ri[] = {{"48001001", "bl Callback"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"48000001", "bl fn_80004000"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"fn_80004000", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels)});
    CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(NULL, sp))));
    const TI ci2[] = {{"48000001", "bl fn_80003000"}, {"4E800020", "blr"}};
    const TSym s2[] = {{"fn_80003000", 0, 0, 1, 0, 0}};
    comp = compiled_fn("f", &(CSpec){INS(ci2), SYMS(s2), RELS(rels)});
    CHECK_EQ_U64(cmp(retail, comp, ctx_new(NULL, sp))->targets.n, 1);
}

AVP_TEST(float_constant_by_content) {
    static const u8 consts[8] = {0x3F, 0xC0, 0, 0, 0x40, 0x20, 0, 0}; /* 1.5f, 2.5f */
    AvpStr dol = fake_dol(0x80300000, consts, sizeof consts);
    const TI ri[] = {{"C0030000", "lfs f0, lbl_80300004@sda21(r0)"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"C0030000", "lfs f0, \"@150\"@sda21(r0)"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"@150", 0, 4, 0, CC_STT_OBJECT, 3}};
    const TRel rels[] = {{1, 0, CC_R_EMB_SDA21, 2, 0}};
    static const u8 f25[4] = {0x40, 0x20, 0, 0}, f15[4] = {0x3F, 0xC0, 0, 0};
    for (int pass = 0; pass < 2; pass++) {
        bool expect = pass == 0; /* 2.5 matches, 1.5 does not */
        TSec extra[] = {{".sdata2", 1, expect ? f25 : f15, 4}};
        CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels), .extra = extra, .nextra = 1});
        CcFnResult *res = cmp(retail, comp, ctx_new(&dol, NULL));
        CHECK(cc_fn_result_exact(res) == expect);
        if (!expect) {
            REQUIRE(res->targets.n >= 1);
            CHECK(has(res->targets.v[0].detail, "differs"));
        }
    }
}

AVP_TEST(raw_retail_operand_against_a_literal_by_value) {
    /* nw4hbm::lyt::TextBox::~TextBox: dtk has no symbol at 0x803DED80, so retail reads `lfd f1, -0x1280(r3)` where
     * the object relocates `"@4555"@l` (the int-to-double constant at .data + 8) */
    static const u8 conv[8] = {0x43, 0x30, 0, 0, 0x80, 0, 0, 0};
    u8 data16[16] = {0}, zeros16[16] = {0};
    memcpy(data16 + 8, conv, 8);
    const char *sp = "game/Foo.cpp:\n\t.data start:0x803DED78 end:0x803DED90\n";
    const TI ri[] = {{"C823ED80", "lfd f1, -0x1280(r3)"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"C8230000", "lfd f1, \"@4555\"@l(r3)"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"@4555", 8, 8, 0, CC_STT_OBJECT, 2}};
    const TRel rels[] = {{1, 2, CC_R_ADDR16_LO, 2, 0}};
    CSpec spec = {INS(ci), SYMS(syms), RELS(rels), .data = data16, .ndata = 16};
    AvpStr dol = fake_dol(0x803DED78, data16, 16);
    CcFn *comp = compiled_fn("f", &spec);
    CcFnResult *res = cmp(retail, comp, ctx_new(&dol, sp));
    CHECK(cc_fn_result_exact(res));
    REQUIRE(res->notes.n == 1);
    CHECK(strcmp(res->notes.v[0].kind, "value") == 0);
    CHECK(has(res->notes.v[0].detail, "0x803DED80"));
    /* comparing the same compiled function again gives the same verdict (the proof is re-derived) */
    CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(&dol, sp))));
    /* other bytes at the address, another layout, or no target check: the row still differs */
    AvpStr dol_zero = fake_dol(0x803DED78, zeros16, 16);
    res = cmp(retail, compiled_fn("f", &spec), ctx_new(&dol_zero, sp));
    CHECK(!cc_fn_result_exact(res));
    CHECK_EQ_U64(res->notes.n, 0);
    res = cmp(retail, compiled_fn("f", &spec),
              ctx_new(&dol, "game/Foo.cpp:\n\t.data start:0x803DED70 end:0x0\n"));
    CHECK(!cc_fn_result_exact(res));
    CHECK_EQ_U64(res->notes.n, 0);
    CHECK(!cc_fn_result_exact(cc_compare_function(avp_arena(), retail, compiled_fn("f", &spec), NULL, true)));
}

AVP_TEST(string_by_content_with_addend) {
    static const u8 image[13] = {'f', 'i', 'r', 's', 't', 0, 's', 'e', 'c', 'o', 'n', 'd', 0};
    AvpStr dol = fake_dol(0x80300000, image, sizeof image);
    const TI ri[] = {{"38630000", "addi r3, r3, lbl_80300006@l"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"38630000", "addi r3, r3, \"@stringBase0\"@l"}, {"4E800020", "blr"}};
    static const u8 pool_a[16] = {'x', 'x', 0, 'f', 'i', 'r', 's', 't', 0, 's', 'e', 'c', 'o', 'n', 'd', 0};
    static const u8 pool_b[12] = {'f', 'i', 'r', 's', 't', 0, 't', 'h', 'i', 'r', 'd', 0};
    for (int pass = 0; pass < 2; pass++) {
        bool expect = pass == 0;
        const u8 *pool = expect ? pool_a : pool_b;
        size_t n = expect ? sizeof pool_a : sizeof pool_b;
        i32 off = expect ? (i32)(n - 7) : 6;
        TSym syms[] = {{"@stringBase0", 0, (u32)n, 0, CC_STT_OBJECT, 2}};
        TRel rels[] = {{1, 2, CC_R_ADDR16_LO, 2, off}};
        CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels), .data = pool, .ndata = n});
        CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(&dol, NULL))) == expect);
    }
}

AVP_TEST(bss_by_layout) {
    const char *sp = "game/Foo.cpp:\n\t.bss start:0x80400000 end:0x80400100\n";
    const TI ri[] = {{"3C600000", "lis r3, lbl_80400020@ha"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"3C600000", "lis r3, ...bss.0@ha"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"...bss.0", 0, 0, 0, CC_STT_OBJECT, 3}};
    static const u8 zeros[0x100] = {0};
    const TSec extra[] = {{".bss", 8, zeros, 0x100}};
    CcFn *comp = NULL;
    for (int pass = 0; pass < 2; pass++) {
        bool expect = pass == 0;
        TRel rels[] = {{1, 2, CC_R_ADDR16_HA, 2, expect ? 0x20 : 0x24}};
        comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels), .extra = extra, .nextra = 1});
        CcFnResult *res = cmp(retail, comp, ctx_new(NULL, sp));
        CHECK(cc_fn_result_exact(res) == expect);
        if (!expect) {
            REQUIRE(res->targets.n >= 1);
            CHECK(has(res->targets.v[0].detail, "layout"));
        }
    }
    /* no record of the section in splits.txt: undecidable */
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, "game/Foo.cpp:\n"));
    CHECK_EQ_U64(res->targets.n, 0);
    CHECK_EQ_U64(res->unresolved.n, 1);
}

AVP_TEST(bss_retail_target_cannot_be_content_checked) {
    static const u8 zeros8[8] = {0}, zeros4[4] = {0};
    AvpStr dol = fake_dol(0x80300000, zeros8, sizeof zeros8);
    const TI ri[] = {{"38630000", "addi r3, r3, lbl_80400020@l"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"38630000", "addi r3, r3, \"@5\"@l"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"@5", 0, 4, 0, CC_STT_OBJECT, 2}};
    const TRel rels[] = {{1, 2, CC_R_ADDR16_LO, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels), .data = zeros4, .ndata = 4});
    CcFnResult *res = cmp(retail, comp, ctx_new(&dol, NULL));
    CHECK_EQ_U64(res->targets.n, 0);
    CHECK_EQ_U64(res->unresolved.n, 1);
}

AVP_TEST(jump_table_entries_are_function_relative) {
    static const u8 table[8] = {0x80, 0, 0x10, 0x08, 0x80, 0, 0x10, 0x0C};
    AvpStr dol = fake_dol(0x80300000, table, sizeof table);
    const TI ri[] = {{"3C800000", "lis r4, jumptable_80300000@ha"}, {"48000001", "nop"}, {"38600001", "li r3, 1"},
                     {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"3C800000", "lis r4, \"@12\"@ha"}, {"48000001", "nop"}, {"38600001", "li r3, 1"},
                     {"4E800020", "blr"}};
    const TSym syms[] = {{"@12", 0, 8, 0, CC_STT_OBJECT, 2}};
    static const u8 zeros8[8] = {0};
    const TRel good[] = {{1, 2, CC_R_ADDR16_HA, 2, 0}, {2, 0, CC_R_ADDR32, 1, 8}, {2, 4, CC_R_ADDR32, 1, 12}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(good), .data = zeros8, .ndata = 8});
    CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(&dol, NULL))));
    const TRel bad[] = {{1, 2, CC_R_ADDR16_HA, 2, 0}, {2, 0, CC_R_ADDR32, 1, 8}, {2, 4, CC_R_ADDR32, 1, 8}};
    comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(bad), .data = zeros8, .ndata = 8});
    CcFnResult *res = cmp(retail, comp, ctx_new(&dol, NULL));
    REQUIRE(res->targets.n == 1);
    CHECK(has(res->targets.v[0].detail, "pointer at +0x4"));
}

AVP_TEST(pointer_to_a_named_function_inside_data) {
    /* a table of one pointer to OSEnableInterrupts; retail holds the address of OSDisableInterrupts */
    static const u8 table[4] = {0x80, 0, 0x10, 0};
    AvpStr dol = fake_dol(0x80300000, table, sizeof table);
    const TI ri[] = {{"3C600000", "lis r3, lbl_80300000@ha"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"3C600000", "lis r3, \"@7\"@ha"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"@7", 0, 4, 0, CC_STT_OBJECT, 2}, {"OSEnableInterrupts", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 2, CC_R_ADDR16_HA, 2, 0}, {2, 0, CC_R_ADDR32, 3, 0}};
    static const u8 zeros4[4] = {0};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels), .data = zeros4, .ndata = 4});
    CcFnResult *res = cmp(retail, comp, ctx_new(&dol, NULL));
    REQUIRE(res->targets.n == 1);
    CHECK(has(res->targets.v[0].detail, "OSEnableInterrupts"));
}

AVP_TEST(branch_targets_compare_by_aligned_row) {
    const TI ri[] = {{"38600001", "li r3, 1"},        {"2C030000", "cmpwi r3, 0"}, {"41820008", "beq .L_80001010"},
                     {"38600002", "li r3, 2"},        {"38600003", "li r3, 3"},    {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI same[] = {{"38600001", "li r3, 1"},        {"2C030000", "cmpwi r3, 0"}, {"41820008", "beq .L_00000010"},
                       {"38600002", "li r3, 2"},        {"38600003", "li r3, 3"},    {"4E800020", "blr"}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(same)});
    CHECK(cc_fn_result_exact(cmp(retail, comp, ctx_new(NULL, NULL))));
    const TI other[] = {{"38600001", "li r3, 1"},        {"2C030000", "cmpwi r3, 0"}, {"4182000C", "beq .L_00000014"},
                        {"38600002", "li r3, 2"},        {"38600003", "li r3, 3"},    {"4E800020", "blr"}};
    comp = compiled_fn("f", &(CSpec){INS(other)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK_EQ_U64(res->edit, 0);
    REQUIRE(res->targets.n == 1);
    CHECK(strcmp(res->targets.v[0].kind, "branch") == 0);
}

AVP_TEST(branch_to_an_inserted_row_is_not_judged) {
    const TI ri[] = {{"2C030000", "cmpwi r3, 0"}, {"41820008", "beq .L_80001008"}, {"38600002", "li r3, 2"},
                     {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"2C030000", "cmpwi r3, 0"}, {"41820004", "beq .L_00000008"}, {"38600009", "li r3, 9"},
                     {"38600002", "li r3, 2"},    {"4E800020", "blr"}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK(res->edit > 0);
    CHECK_EQ_U64(res->targets.n, 0);
}

AVP_TEST(instruction_difference_is_still_reported) {
    const TI ri[] = {{"38600001", "li r3, 1"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"38600002", "li r3, 2"}, {"4E800020", "blr"}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci)});
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    CHECK_EQ_U64(res->edit, 1);
    CHECK_EQ_U64(res->npositional, 1);
    CHECK(has(codecmp_describe_result(avp_arena(), AVP_S("f"), res, false, 40), "1/2 rows differ"));
}

AVP_TEST(describe_result_format) {
    const TI ri[] = {{"48001001", "bl OSDisableInterrupts"}, {"4E800020", "blr"}};
    const CcFn *retail = retail_fn("f", ri, N(ri));
    const TI ci[] = {{"48000001", "bl OSEnableInterrupts"}, {"4E800020", "blr"}};
    const TSym syms[] = {{"OSEnableInterrupts", 0, 0, 1, 0, 0}};
    const TRel rels[] = {{1, 0, CC_R_REL24, 2, 0}};
    CcFn *comp = compiled_fn("f", &(CSpec){INS(ci), SYMS(syms), RELS(rels)});
    AvpArena *a = avp_arena();
    CcFnResult *res = cmp(retail, comp, ctx_new(NULL, NULL));
    char *text = avp_cz(a, codecmp_describe_result(a, AVP_S("f"), res, false, 40));
    char *l1 = strchr(text, '\n');
    REQUIRE(l1);
    *l1++ = 0;
    char *l2 = strchr(l1, '\n');
    REQUIRE(l2);
    *l2 = 0;
    CHECK_CZ(text, "f: 0/2 rows differ, 1 target mismatches");
    CHECK(strncmp(l1, "    0 bl OSDisableInterrupts", 28) == 0);
    const char *tail = "| bl OSEnableInterrupts            [target]";
    size_t lt = strlen(tail), ll = strlen(l1);
    CHECK(ll >= lt && strcmp(l1 + ll - lt, tail) == 0);
}

/* ---- HelperTests ------------------------------------------------------------------------------------------------- */

AVP_TEST(helper_edit_count) {
    AvpStr a4[] = {AVP_S("a"), AVP_S("b"), AVP_S("c"), AVP_S("d")};
    AvpStr x1[] = {AVP_S("a"), AVP_S("X"), AVP_S("c"), AVP_S("d")};
    AvpStr i1[] = {AVP_S("a"), AVP_S("b"), AVP_S("I"), AVP_S("c"), AVP_S("d")};
    CHECK_EQ_U64(cc_edit_count(a4, 4, a4, 4), 0);
    CHECK_EQ_U64(cc_edit_count(a4, 4, x1, 4), 1);
    CHECK_EQ_U64(cc_edit_count(a4, 4, i1, 5), 1);
    CHECK_EQ_U64(cc_edit_count(a4, 4, NULL, 0), 4);
}

AVP_TEST(helper_retail_name_for) {
    AvpMap *retail = avp_map_new();
    avp_map_put(retail, AVP_S("alpha"), NULL);
    avp_map_put(retail, AVP_S("fn_80002000__Fv"), NULL);
    avp_map_put(retail, AVP_S("fn_80003000"), NULL);
    CHECK_STR(cc_retail_name_for(AVP_S("fn_80002000__FRi"), retail), "fn_80002000__Fv");
    CHECK_STR(cc_retail_name_for(AVP_S("fn_80003000__8unk_RampFv"), retail), "fn_80003000");
    CHECK(cc_retail_name_for(AVP_S("beta"), retail).p == NULL);
}

/* ---- RealDataTest ------------------------------------------------------------------------------------------------ */

/* Compares the real compiled OSAlarm object with its retail listing. Runs only where the built object, the retail
 * listing, main.dol and dtk exist. */
AVP_TEST(os_alarm_is_exact_with_no_target_diffs) {
    AvpArena *a = avp_arena();
    const char *root = avp_root();
    char *obj_src = avp_sprintf(a, "%s\\build\\R5WEA4\\src\\revolution\\os\\OSAlarm.o", root);
    char *asm_path = avp_sprintf(a, "%s\\build\\R5WEA4\\asm\\revolution\\os\\OSAlarm.s", root);
    char *dol = avp_sprintf(a, "%s\\orig\\R5WEA4\\sys\\main.dol", root);
    char *dtk = avp_sprintf(a, "%s\\tools\\bin\\dtk.exe", root);
    if (!avp_isfile(obj_src) || !avp_isfile(asm_path) || !avp_isfile(dol) || !avp_isfile(dtk)) return;
    char *work = avp_sprintf(a, "%s\\build\\tmp\\codecmp\\_test_%u", root, avp_getpid());
    REQUIRE(avp_makedirs(work, true));
    char *obj = avp_sprintf(a, "%s\\OSAlarm.o", work);
    REQUIRE(avp_copyfile(obj_src, obj));
    AvpStr err = {0};
    AvpMap *retail = cc_load_asm(a, asm_path, &err);
    AvpMap *compiled = retail ? cc_load_object(a, obj, NULL, &err) : NULL;
    if (retail && compiled) {
        CcContext *ctx = cc_context_new(cc_project_new(root), AVP_S("revolution/os/OSAlarm"));
        AvpMap *res = cc_codecmp_object(a, retail, compiled, ctx, true, NULL);
        CHECK(avp_map_len(res) >= 10);
        size_t checked = 0;
        AvpMapIter it = {0};
        while (avp_map_next(res, &it)) {
            const CcFnResult *r = it.val->p;
            CHECK_MSG(cc_fn_result_exact(r), "%.*s is not exact", AVP_SF(it.key));
            CHECK_MSG(r->unresolved.n == 0, "%.*s has unresolved targets", AVP_SF(it.key));
            checked += r->checked_targets;
        }
        CHECK(checked > 100);
    } else {
        CHECK_MSG(false, "loading the retail listing or the object failed: %.*s", AVP_SF(err));
    }
    avp_rmtree(work, AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST_MAIN("codecmp")
