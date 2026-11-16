/* sibling: the foreign-architecture sibling (ELF32/ECOFF reading, stabs types, gnu-v2 names, the seq tier) on
 * synthetic fixtures. */
#include "avp/test.h"
#include "sibling_foreign.h"

#include <string.h>

/* ---- a tiny little-endian MIPS ELF with .text, .symtab, .strtab, optional .mdebug ---- */

typedef struct {
    u8 b[4096];
    size_t n;
} Img;
static void w32(Img *m, size_t o, u32 v) {
    m->b[o] = (u8)v;
    m->b[o + 1] = (u8)(v >> 8);
    m->b[o + 2] = (u8)(v >> 16);
    m->b[o + 3] = (u8)(v >> 24);
}
static void w16(Img *m, size_t o, u16 v) {
    m->b[o] = (u8)v;
    m->b[o + 1] = (u8)(v >> 8);
}
static size_t put(Img *m, const void *p, size_t n) {
    size_t o = m->n;
    memcpy(m->b + o, p, n);
    m->n += n;
    return o;
}
static size_t align(Img *m) {
    while (m->n & 3) m->b[m->n++] = 0;
    return m->n;
}

static const char *const STABS[] = {
    "int:t3=r3;-2147483648;2147483647;",
    "Base:T1=s4x:/23,0,32;;",
    "Thing:Tt2=s12!1,020,1;flag:/03,32,1;rest:/03,33,31;\\",
    "ptr:9=*10=xsLater:,64,32;Go::4=##3;:v;2A*0;2;;Get::11=##3;:;2B.;;~%2;",
    "Odd:T5=s4q:6=Z9,0,32;;",
};
#define NSTABS (sizeof STABS / sizeof *STABS)

static Img build(bool with_md) {
    Img m = {0};
    m.n = 0x100;
    /* .text: 16 words at file 0x100, address 0x100000 */
    size_t text = m.n;
    for (int k = 0; k < 16; k++) put(&m, "\0\0\0\0", 4);
    /* .strtab */
    size_t strtab = m.n;
    put(&m, "\0func_a\0", 8);
    /* .symtab: null + func_a */
    size_t symtab = align(&m);
    m.n += 32;
    w32(&m, symtab + 16, 1);
    w32(&m, symtab + 20, 0x100000);
    w32(&m, symtab + 24, 0x20);
    m.b[symtab + 28] = 0x12; /* GLOBAL FUNC */
    w16(&m, symtab + 30, 1);
    size_t md = 0, mdlen = 0;
    if (with_md) {
        md = align(&m);
        m.n += 0x60;
        /* local strings */
        size_t ss = m.n;
        u32 iss_file = (u32)(put(&m, "a.c", 4) - ss), iss_a = (u32)(put(&m, "func_a", 7) - ss);
        u32 iss_b = (u32)(put(&m, "static_b", 9) - ss);
        u32 iss_st[NSTABS];
        for (size_t k = 0; k < NSTABS; k++) iss_st[k] = (u32)(put(&m, STABS[k], strlen(STABS[k]) + 1) - ss);
        u32 cbss = (u32)(m.n - ss);
        /* local symbols */
        size_t syms = align(&m);
        u32 nsym = 3 + (u32)NSTABS;
        m.n += 12 * nsym;
        w32(&m, syms + 0, iss_file);
        w32(&m, syms + 8, 11); /* stFile */
        w32(&m, syms + 12, iss_a);
        w32(&m, syms + 16, 0x100000);
        w32(&m, syms + 20, 6); /* stProc */
        w32(&m, syms + 24, iss_b);
        w32(&m, syms + 28, 0x100020);
        w32(&m, syms + 32, 14); /* stStaticProc */
        for (u32 k = 0; k < NSTABS; k++) {
            w32(&m, syms + 36 + 12 * k, iss_st[k]);
            w32(&m, syms + 44 + 12 * k, (SIB_STAB_MARK + 0x80) << 12); /* N_LSYM stab */
        }
        /* FDR */
        size_t fdr = m.n;
        m.n += 72;
        w32(&m, fdr + 0, 0x100000);
        w32(&m, fdr + 4, iss_file);
        w32(&m, fdr + 12, cbss);
        w32(&m, fdr + 20, nsym);
        w16(&m, fdr + 42, 2);
        /* PDRs */
        size_t pdr = m.n;
        m.n += 104;
        w32(&m, pdr + 0, 0);
        w32(&m, pdr + 4, 1);
        w32(&m, pdr + 52, 0x20);
        w32(&m, pdr + 56, 2);
        mdlen = m.n - md;
        w16(&m, md, 0x7009);
        w32(&m, md + 24, 2);             /* ipdMax */
        w32(&m, md + 28, (u32)pdr);      /* cbPdOffset */
        w32(&m, md + 32, nsym);          /* isymMax */
        w32(&m, md + 36, (u32)syms);     /* cbSymOffset */
        w32(&m, md + 56, cbss);          /* issMax */
        w32(&m, md + 60, (u32)ss);       /* cbSsOffset */
        w32(&m, md + 72, 1);             /* ifdMax */
        w32(&m, md + 76, (u32)fdr);      /* cbFdOffset */
    }
    size_t shstr = m.n;
    put(&m, "\0.text\0.symtab\0.strtab\0.mdebug\0.shstrtab\0", 41);
    size_t sh = align(&m);
    int nsec = 6;
    m.n += 40 * (size_t)nsec;
    struct {
        u32 name, type, flags, addr, off, size, link, ent;
    } secs[6] = {{0, 0, 0, 0, 0, 0, 0, 0},
                 {1, 1, 6, 0x100000, (u32)text, 64, 0, 0},
                 {7, 2, 0, 0, (u32)symtab, 32, 3, 16},
                 {15, 3, 0, 0, (u32)strtab, 8, 0, 0},
                 {23, 0x70000005u, 0, 0, (u32)md, (u32)mdlen, 0, 0},
                 {31, 3, 0, 0, (u32)shstr, 41, 0, 0}};
    if (!with_md) { /* an empty PROGBITS section named "shstrtab" stands in for .mdebug */
        secs[4].name = 32;
        secs[4].type = 1;
        secs[4].off = (u32)shstr;
        secs[4].size = 0;
    }
    for (int k = 0; k < nsec; k++) {
        size_t o = sh + 40 * (size_t)k;
        w32(&m, o, secs[k].name);
        w32(&m, o + 4, secs[k].type);
        w32(&m, o + 8, secs[k].flags);
        w32(&m, o + 12, secs[k].addr);
        w32(&m, o + 16, secs[k].off);
        w32(&m, o + 20, secs[k].size);
        w32(&m, o + 24, secs[k].link);
        w32(&m, o + 36, secs[k].ent);
    }
    memcpy(m.b, "\x7f" "ELF\x01\x01\x01", 7);
    w16(&m, 16, 2);
    w16(&m, 18, SIB_EM_MIPS);
    w32(&m, 20, 1);
    w32(&m, 32, (u32)sh);
    w16(&m, 40, 52);
    w16(&m, 46, 40);
    w16(&m, 48, (u16)nsec);
    w16(&m, 50, 5);
    return m;
}

AVP_TEST(foreign_elf_and_mdebug) {
    AvpArena *a = avp_arena();
    static Img m;
    m = build(true);
    SibElf e;
    const char *err = NULL;
    REQUIRE(sib_elf_parse(a, m.b, m.n, &e, &err));
    CHECK(e.machine == SIB_EM_MIPS && !e.big_endian);
    REQUIRE(e.syms.n == 2);
    CHECK_CZ(e.syms.v[1].name, "func_a");
    const SibElfSec *s = sib_elf_section(&e, ".mdebug");
    REQUIRE(s != NULL);
    SibMdebug md;
    REQUIRE(sib_md_parse(a, m.b, m.n, s->offset, s->size, &md, &err));
    REQUIRE(md.fds.n == 1 && md.pds.n == 2);
    CHECK_CZ(md.fds.v[0].name, "a.c");
    CHECK_CZ(md.pds.v[1].name, "static_b");
    CHECK(md.pds.v[1].adr == 0x100020);
    SibStabVec v = {0};
    sib_md_stabs(a, &md, 0, &v);
    REQUIRE(v.n == NSTABS - 1); /* the continuation joined */
    CHECK(v.v[0].type == 0x80);
    CHECK(strstr(v.v[2].str, "rest:/03,33,31;ptr:") != NULL);
    SibXFnVec f = sib_foreign_functions(a, &e, &md);
    REQUIRE(f.n == 2);
    CHECK(f.v[0].from_symtab && f.v[0].file == 0 && f.v[0].order == 0 && f.v[0].size == 0x20);
    CHECK(!f.v[1].from_symtab && f.v[1].file == 0 && f.v[1].order == 1 && f.v[1].size == 0x20);
    avp_vec_free(&v);
}

AVP_TEST(foreign_no_mdebug) {
    AvpArena *a = avp_arena();
    static Img m;
    m = build(false);
    SibElf e;
    const char *err = NULL;
    REQUIRE(sib_elf_parse(a, m.b, m.n, &e, &err));
    CHECK(sib_elf_section(&e, ".mdebug") == NULL);
    SibXFnVec f = sib_foreign_functions(a, &e, NULL);
    REQUIRE(f.n == 1);
    CHECK(f.v[0].file == -1 && f.v[0].from_symtab);
    /* a damaged header is refused, not read */
    SibMdebug md;
    CHECK(!sib_md_parse(a, m.b, m.n, 0, 0x60, &md, &err));
}

AVP_TEST(foreign_stabs_types) {
    AvpArena *a = avp_arena();
    const char *joined[] = {STABS[0], STABS[1],
                            avp_sprintf(a, "%.*s%s", (int)strlen(STABS[2]) - 1, STABS[2], STABS[3]), STABS[4]};
    i64 unknown = 0;
    const char *t = sib_stabs_render(a, joined, 4, "Thing", false, &unknown);
    REQUIRE(t != NULL);
    CHECK(strstr(t, "class Thing : public Base /* 0x00 */ { // size 0xC, vptr from this class") != NULL);
    CHECK(strstr(t, "int flag : 1;") != NULL);
    CHECK(strstr(t, "bit 1") != NULL);
    CHECK(strstr(t, "Later *ptr;") != NULL);
    CHECK(strstr(t, "virtual int Go(/* v */); // vtable index 0") != NULL);
    CHECK(strstr(t, "int Get(/*  */) const;") != NULL);
    const char *b = sib_stabs_render(a, joined, 4, "Base", false, NULL);
    REQUIRE(b != NULL);
    CHECK(strstr(b, "struct Base { // size 0x4") != NULL);
    CHECK(strstr(b, "int x;") != NULL);
    const char *o = sib_stabs_render(a, joined, 4, "Odd", false, &unknown);
    REQUIRE(o != NULL);
    CHECK(strstr(o, "<unknown stabs: Z9>") != NULL);
    CHECK(unknown >= 1);
    CHECK(sib_stabs_render(a, joined, 4, "Missing", false, NULL) == NULL);
}

AVP_TEST(foreign_gnu_demangle) {
    AvpArena *a = avp_arena();
    CHECK_CZ(sib_gnu_demangle(a, "AddSpawn__5CZonePQ33RWS12AssetTracker10CAssetList"),
             "CZone::AddSpawn(RWS::AssetTracker::CAssetList *)");
    CHECK_CZ(sib_gnu_qualified(a, "SetZone__Q23RWS17CAttributeHandlerP5CZone"), "RWS::CAttributeHandler::SetZone");
    CHECK_CZ(sib_gnu_demangle(a, "_$_5CZone"), "CZone::~CZone()");
    CHECK_CZ(sib_gnu_demangle(a, "__5CZoneRCQ23RWS16CAttributePacket"),
             "CZone::CZone(const RWS::CAttributePacket &)");
    CHECK_CZ(sib_gnu_demangle(a, "__as__5CZoneRC5CZone"), "CZone::operator=(const CZone &)");
    CHECK_CZ(sib_gnu_demangle(a, "GetName__C5CZone"), "CZone::GetName() const");
    CHECK_CZ(sib_gnu_demangle(a, "Lerp__FfffT0"), "Lerp(float, float, float, float)");
    CHECK_CZ(sib_gnu_demangle(a, "Update__5CZonef"), "CZone::Update(float)");
    CHECK_CZ(sib_gnu_demangle(a, "f__FUiPCcN21"), "f(unsigned int, const char *, const char *, const char *)");
    CHECK(sib_gnu_demangle(a, "RwpBodyCreate") == NULL);
    CHECK(sib_gnu_demangle(a, "foo__t4List1Zi") == NULL); /* templates: not covered */
}

AVP_TEST(foreign_seq_fill) {
    /* retail: P u u P | u P ; foreign file 0: P u u P, file 1 in between */
    SibXFn gx[6] = {{.address = 0, .file = 0, .order = 0}, {.address = 4, .file = 0, .order = 1},
                    {.address = 8, .file = 1, .order = 0}, {.address = 12, .file = 0, .order = 2},
                    {.address = 16, .file = 0, .order = 3}, {.address = 20, .file = 0, .order = 4}};
    SibSeqNode g[6], s[6];
    for (int k = 0; k < 6; k++) g[k] = (SibSeqNode){.address = k * 4, .size = 32, .peer = -1, .unit = ""};
    for (int k = 0; k < 6; k++) s[k] = (SibSeqNode){.address = k * 4, .size = 40, .peer = -1, .unit = "u1"};
    s[0].peer = 0, s[0].tier = 1, g[0].peer = 0, g[0].tier = 1;
    s[3].peer = 4, s[3].tier = 1, g[4].peer = 3, g[4].tier = 1;
    s[5].peer = 5, s[5].tier = 1, g[5].peer = 5, g[5].tier = 1; /* gap of 1 retail vs 0 foreign: not filled */
    sib_foreign_group(gx, 6);
    CHECK(sib_seq_fill(s, 6, g, gx, 6, 7, 0) == 2);
    CHECK(s[1].peer == 1 && s[1].tier == 7);
    CHECK(s[2].peer == 3 && g[3].peer == 2); /* file 1's function skipped */
    CHECK(s[4].peer == -1);
    CHECK(g[2].peer == -1);
    /* a size mismatch refuses the whole gap */
    SibSeqNode s2[3] = {{.size = 40, .peer = 0, .tier = 1, .unit = "u"}, {.size = 4000, .peer = -1, .unit = "u"},
                        {.size = 40, .peer = 2, .tier = 1, .unit = "u"}};
    SibSeqNode g2[3] = {{.size = 40, .peer = 0}, {.size = 40, .peer = -1}, {.size = 40, .peer = 2}};
    SibXFn gx2[3] = {{.file = 0}, {.file = 0}, {.file = 0}};
    sib_foreign_group(gx2, 3);
    CHECK(sib_seq_fill(s2, 3, g2, gx2, 3, 7, 8) == 0);
    /* seq-edge: the tail of file 0 and the head of file 1 between two pairs */
    SibXFn gx3[4] = {{.address = 0, .file = 0}, {.address = 4, .file = 0}, {.address = 8, .file = 1}, {.address = 12, .file = 1}};
    sib_foreign_group(gx3, 4);
    SibSeqNode s3[4], g3[4];
    for (int k = 0; k < 4; k++) s3[k] = (SibSeqNode){.size = 40, .peer = -1, .unit = "u"}, g3[k] = s3[k];
    s3[0].peer = 0, s3[0].tier = 1, g3[0].peer = 0, g3[0].tier = 1;
    s3[3].peer = 3, s3[3].tier = 1, g3[3].peer = 3, g3[3].tier = 1;
    CHECK(sib_seq_fill(s3, 4, g3, gx3, 4, 7, 0) == 0);
    CHECK(sib_seq_fill(s3, 4, g3, gx3, 4, 7, 8) == 2);
    CHECK(s3[1].peer == 1 && s3[1].tier == 8 && s3[2].peer == 2);
}

AVP_TEST_MAIN("sibling")
