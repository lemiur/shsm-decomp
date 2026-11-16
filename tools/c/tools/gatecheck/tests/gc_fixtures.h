/* Builders for the synthetic inputs of tools/test_gatecheck.py: make_dol, make_elf, asm_text / data_asm, the SPLITS /
 * SYMBOLS / CONFIGURE texts, the layout fixtures (retail asm and blob, the built object), a temporary repository root
 * and a runner for the gatecheck executable. Header-only (static inline), included by every test file. */
#pragma once
#include "avp/test.h"
#include "gatecheck.h"
#include <string.h>

#define FX_N(a) (sizeof(a) / sizeof((a)[0]))
#define FX_START 0x80300000u

/* ---- bytes -------------------------------------------------------------------------------------------------------- */
static inline AvpStr fx_cat(AvpStr x, AvpStr y) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_buf_put(&b, x);
    avp_buf_put(&b, y);
    return avp_buf_str(&b);
}
static inline AvpStr fx_fill(u8 v, size_t n) {
    char *p = (char *)avp_alloc(avp_arena(), n + 1);
    memset(p, v, n);
    return avp_strn(p, n);
}
/* bytes(range(256)) * k, cut to n bytes */
static inline AvpStr fx_range(size_t n) {
    char *p = (char *)avp_alloc(avp_arena(), n + 1);
    for (size_t i = 0; i < n; i++) p[i] = (char)(i & 255);
    return avp_strn(p, n);
}
static inline AvpStr fx_copy(AvpStr s) { return avp_str_dup(avp_arena(), s); }
static inline void fx_be32(AvpBuf *b, u32 v) {
    avp_buf_putc(b, (char)(v >> 24));
    avp_buf_putc(b, (char)(v >> 16));
    avp_buf_putc(b, (char)(v >> 8));
    avp_buf_putc(b, (char)v);
}
static inline void fx_be16(AvpBuf *b, u32 v) {
    avp_buf_putc(b, (char)(v >> 8));
    avp_buf_putc(b, (char)v);
}
static inline void fx_put32(char *p, u32 v) {
    p[0] = (char)(v >> 24);
    p[1] = (char)(v >> 16);
    p[2] = (char)(v >> 8);
    p[3] = (char)v;
}
static inline void fx_put16(char *p, u32 v) {
    p[0] = (char)(v >> 8);
    p[1] = (char)v;
}
/* str.replace(from, to) over a C string */
static inline const char *fx_replace(const char *s, const char *from, const char *to) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    size_t fl = strlen(from);
    while (*s) {
        if (strncmp(s, from, fl) == 0) {
            avp_buf_putz(&b, to);
            s += fl;
        } else {
            avp_buf_putc(&b, *s++);
        }
    }
    return avp_buf_cz(&b);
}
static inline bool fx_contains(AvpStr hay, const char *needle) { return avp_str_contains(hay, avp_str(needle)); }

/* ---- make_dol ------------------------------------------------------------------------------------------------------ */
typedef struct {
    int idx;
    u32 addr;
    AvpStr data;
} FxDolSec;

static inline AvpStr fx_dol_ex(const FxDolSec *s, size_t n, u32 bss_addr, u32 bss_size, u32 entry) {
    AvpArena *a = avp_arena();
    u32 offs[18] = {0}, addrs[18] = {0}, sizes[18] = {0};
    AvpBuf body, out;
    avp_buf_init(&body, a);
    avp_buf_init(&out, a);
    for (size_t i = 0; i < n; i++) {
        offs[s[i].idx] = (u32)(0x100 + body.n);
        addrs[s[i].idx] = s[i].addr;
        sizes[s[i].idx] = (u32)s[i].data.n;
        avp_buf_put(&body, s[i].data);
    }
    for (int i = 0; i < 18; i++) fx_be32(&out, offs[i]);
    for (int i = 0; i < 18; i++) fx_be32(&out, addrs[i]);
    for (int i = 0; i < 18; i++) fx_be32(&out, sizes[i]);
    fx_be32(&out, bss_addr);
    fx_be32(&out, bss_size);
    fx_be32(&out, entry);
    while (out.n < 0x100) avp_buf_putc(&out, 0);
    avp_buf_put(&out, avp_buf_str(&body));
    return avp_buf_str(&out);
}
static inline AvpStr fx_dol(const FxDolSec *s, size_t n) { return fx_dol_ex(s, n, 0x80500000u, 0x100, 0x80004000u); }
static inline AvpStr fx_dol1(int idx, u32 addr, AvpStr data) {
    FxDolSec s = {idx, addr, data};
    return fx_dol(&s, 1);
}
static inline ShsmDol *fx_dol_parse(AvpStr bytes) { return shsm_dol_parse(avp_arena(), (const u8 *)bytes.p, bytes.n); }

/* ---- make_elf ------------------------------------------------------------------------------------------------------ */
typedef struct {
    const char *name;
    u32 type, flags, align;
    AvpStr data; /* section contents (not NOBITS) */
    u32 size;    /* NOBITS size */
    u32 addr;
} FxElfSec;
typedef struct {
    const char *name; /* NULL or "": no name */
    u32 value, size;
    int type, bind, shndx;
} FxElfSym;
typedef struct {
    int sec; /* section index (1-based), ascending in the array */
    u32 off, sym, type;
    i32 addend;
} FxElfRel;

static inline u32 fx_add_str(AvpBuf *tab, const char *s) {
    u32 pos = (u32)tab->n;
    avp_buf_putz(tab, s);
    avp_buf_putc(tab, 0);
    return pos;
}

static inline AvpStr fx_elf_ex(const FxElfSec *secs, size_t ns, const FxElfSym *syms, size_t nsym,
                               const FxElfRel *rels, size_t nrel, int etype) {
    AvpArena *a = avp_arena();
    typedef struct {
        u32 name_off, type, flags, addr, link, info, align, entsize, nobits;
        AvpStr data;
    } Ent;
    Ent ent[32];
    size_t ne = 0;
    AvpBuf shstr, strtab, symdata, out;
    avp_buf_init(&shstr, a);
    avp_buf_init(&strtab, a);
    avp_buf_init(&symdata, a);
    avp_buf_init(&out, a);
    avp_buf_putc(&shstr, 0);
    avp_buf_putc(&strtab, 0);
    for (size_t i = 0; i < ns; i++) {
        Ent e = {0};
        e.name_off = fx_add_str(&shstr, secs[i].name);
        e.type = secs[i].type;
        e.flags = secs[i].flags;
        e.addr = secs[i].addr;
        e.align = secs[i].align;
        e.data = secs[i].data;
        e.nobits = secs[i].size;
        ent[ne++] = e;
    }
    for (int i = 0; i < 16; i++) avp_buf_putc(&symdata, 0);
    for (size_t i = 0; i < nsym; i++) {
        u32 so = syms[i].name && syms[i].name[0] ? fx_add_str(&strtab, syms[i].name) : 0;
        fx_be32(&symdata, so);
        fx_be32(&symdata, syms[i].value);
        fx_be32(&symdata, syms[i].size);
        avp_buf_putc(&symdata, (char)((syms[i].bind << 4) | syms[i].type));
        avp_buf_putc(&symdata, 0);
        fx_be16(&symdata, (u32)syms[i].shndx);
    }
    for (size_t i = 0; i < nrel;) {
        int sidx = rels[i].sec;
        AvpBuf rd;
        avp_buf_init(&rd, a);
        for (; i < nrel && rels[i].sec == sidx; i++) {
            fx_be32(&rd, rels[i].off);
            fx_be32(&rd, (rels[i].sym << 8) | rels[i].type);
            fx_be32(&rd, (u32)rels[i].addend);
        }
        Ent e = {0};
        e.name_off = fx_add_str(&shstr, avp_sprintf(a, ".rela%s", secs[sidx - 1].name));
        e.type = GC_SHT_RELA;
        e.info = (u32)sidx;
        e.align = 4;
        e.entsize = 12;
        e.data = avp_buf_str(&rd);
        ent[ne++] = e;
    }
    u32 n_symtab = fx_add_str(&shstr, ".symtab");
    u32 n_strtab = fx_add_str(&shstr, ".strtab");
    u32 n_shstr = fx_add_str(&shstr, ".shstrtab");
    u32 symtab_idx = (u32)ne + 1, strtab_idx = (u32)ne + 2;
    Ent e = {0};
    e.name_off = n_symtab; e.type = GC_SHT_SYMTAB; e.link = strtab_idx; e.align = 4; e.entsize = 16;
    e.data = avp_buf_str(&symdata);
    ent[ne++] = e;
    e = (Ent){0};
    e.name_off = n_strtab; e.type = 3; e.align = 1;
    e.data = avp_buf_str(&strtab);
    ent[ne++] = e;
    e = (Ent){0};
    e.name_off = n_shstr; e.type = 3; e.align = 1;
    e.data = avp_buf_str(&shstr);
    ent[ne++] = e;
    for (size_t i = 0; i < ne; i++)
        if (ent[i].type == GC_SHT_RELA) ent[i].link = symtab_idx;

    for (int i = 0; i < 52; i++) avp_buf_putc(&out, 0);
    AvpBuf headers;
    avp_buf_init(&headers, a);
    for (int i = 0; i < 10; i++) fx_be32(&headers, 0);
    for (size_t i = 0; i < ne; i++) {
        u32 off, size;
        if (ent[i].type == GC_SHT_NOBITS) {
            off = (u32)out.n;
            size = ent[i].nobits;
        } else {
            while (out.n % 4) avp_buf_putc(&out, 0);
            off = (u32)out.n;
            avp_buf_put(&out, ent[i].data);
            size = (u32)ent[i].data.n;
        }
        fx_be32(&headers, ent[i].name_off);
        fx_be32(&headers, ent[i].type);
        fx_be32(&headers, ent[i].flags);
        fx_be32(&headers, ent[i].addr);
        fx_be32(&headers, off);
        fx_be32(&headers, size);
        fx_be32(&headers, ent[i].link);
        fx_be32(&headers, ent[i].info);
        fx_be32(&headers, ent[i].align);
        fx_be32(&headers, ent[i].entsize);
    }
    while (out.n % 4) avp_buf_putc(&out, 0);
    u32 shoff = (u32)out.n;
    u32 nheaders = (u32)ne + 1;
    avp_buf_put(&out, avp_buf_str(&headers));
    char *p = out.p;
    p[0] = 0x7f; p[1] = 'E'; p[2] = 'L'; p[3] = 'F'; p[4] = 1; p[5] = 2; p[6] = 1;
    fx_put16(p + 16, (u32)etype);
    fx_put16(p + 18, 20);
    fx_put32(p + 20, 1);
    fx_put32(p + 24, 0);
    fx_put32(p + 28, 0);
    fx_put32(p + 32, shoff);
    fx_put32(p + 36, 0);
    fx_put16(p + 40, 52);
    fx_put16(p + 42, 0);
    fx_put16(p + 44, 0);
    fx_put16(p + 46, 40);
    fx_put16(p + 48, nheaders);
    fx_put16(p + 50, nheaders - 1);
    return avp_buf_str(&out);
}
static inline AvpStr fx_elf(const FxElfSec *secs, size_t ns, const FxElfSym *syms, size_t nsym, const FxElfRel *rels,
                            size_t nrel) {
    return fx_elf_ex(secs, ns, syms, nsym, rels, nrel, 1);
}
static inline GcElf *fx_elf_parse(AvpStr bytes) { return gc_elf_parse(avp_arena(), (const u8 *)bytes.p, bytes.n); }

/* The code: does this expression raise a GateError (gatecheck's catch of GateError)? */
static inline bool fx_dol_is_gate_error(AvpStr bytes) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        fx_dol_parse(bytes);
        avp_try_end(&t);
        return false;
    }
    return gc_is_gate(t.code);
}
static inline bool fx_elf_is_gate_error(AvpStr bytes) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        fx_elf_parse(bytes);
        avp_try_end(&t);
        return false;
    }
    return gc_is_gate(t.code);
}

/* ---- project metadata texts ---------------------------------------------------------------------------------------- */
static const char FX_SPLITS[] =
    "Sections:\n"
    "\t.init       type:code align:4\n"
    "\t.text       type:code align:8\n"
    "\n"
    "game/Foo.cpp:\n"
    "\t.text       start:0x80006000 end:0x80006100\n"
    "\t.data       start:0x80300000 end:0x80300040\n"
    "\t.ctors      start:0x803D8360 end:0x803D8364 rename:.ctors$10\n"
    "\n"
    "revolution/os/Bar.c:\n"
    "\t.text       start:0x80006100 end:0x80006200 align:16\n";

static const char FX_SYMBOLS[] =
    "fn_80006000__3Foo = .text:0x80006000; // type:function size:0x40 scope:global\n"
    "fn_80006040__3Foo = .text:0x80006040; // type:function size:0x40\n"
    "lbl_80300000 = .data:0x80300000; // type:object size:0x20\n"
    "BarInit = .text:0x80006100; // type:function size:0x80 scope:local\n";

static const char FX_CONFIGURE[] =
    "\n"
    "            Object(Matching, \"game/Foo.cpp\"),\n"
    "            # Object(Matching, \"commented/Out.c\"),\n"
    "            Object(\n"
    "                NonMatching,\n"
    "                \"revolution/os/Bar.c\",\n"
    "                extra_cflags=[\"-x\"],\n"
    "            ),\n";

/* SymbolMap([(a, s, n) for a, s, n, ... in parse_symbols_txt(text)]) */
static inline GcSymbolMap *fx_symbol_map(const char *symbols_txt) {
    AvpArena *a = avp_arena();
    GcSymTxtVec sy = gc_parse_symbols_txt(a, avp_str(symbols_txt));
    GcEntry *e = AVP_NEWN(a, GcEntry, sy.n + 1);
    for (size_t i = 0; i < sy.n; i++) e[i] = (GcEntry){sy.v[i].addr, sy.v[i].size, sy.v[i].name};
    return gc_symbol_map(a, e, sy.n);
}
/* UnitMap(parse_splits(SPLITS), parse_configure(CONFIGURE)) */
static inline GcUnitMap *fx_unit_map(const char *splits, const char *configure) {
    AvpArena *a = avp_arena();
    GcSplitVec sp = gc_parse_splits(a, avp_str(splits));
    return gc_unit_map(a, &sp, gc_parse_configure(a, avp_str(configure)));
}

/* ---- asm text ------------------------------------------------------------------------------------------------------ */
typedef struct {
    const char *name;
    int off, size;
    const char *const *body; /* NULL-terminated lines */
} FxAsmObj;
#define FX_BODY(...) ((const char *const[]){__VA_ARGS__, NULL})

/* data_asm(secname, start, end, objs) */
static inline const char *fx_data_asm(const char *secname, u32 start, u32 end, const FxAsmObj *objs, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_buf_printf(&b, ".fn func, global\n.endfn func\n\n# 0x%08X..0x%08X | size: 0x%X\n", start, end, end - start);
    if (strcmp(secname, ".data") == 0 || strcmp(secname, ".rodata") == 0)
        avp_buf_printf(&b, "%s\n", secname);
    else
        avp_buf_printf(&b, ".section %s, \"wa\"\n", secname);
    avp_buf_printf(&b, ".balign 8\n");
    for (size_t i = 0; i < n; i++) {
        avp_buf_printf(&b, "\n# %s:0x%X | 0x%08X | size: 0x%X\n.obj %s, global\n", secname, objs[i].off,
                       start + (u32)objs[i].off, objs[i].size, objs[i].name);
        for (const char *const *l = objs[i].body; *l; l++) avp_buf_printf(&b, "\t%s\n", *l);
        avp_buf_printf(&b, ".endobj %s\n", objs[i].name);
    }
    return avp_buf_cz(&b);
}

/* code_asm(start, end, fns) */
typedef struct {
    const char *name;
    int size;
} FxFn;
static inline const char *fx_code_asm(u32 start, u32 end, const FxFn *fns, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_buf_printf(&b, "# 0x%08X..0x%08X | size: 0x%X\n.text\n.balign 4\n\n", start, end, end - start);
    u32 pos = start;
    for (size_t i = 0; i < n; i++) {
        avp_buf_printf(&b, "# .text:0x%X | 0x%08X | size: 0x%X\n.fn %s, global\n", pos - start, pos, fns[i].size,
                       fns[i].name);
        avp_buf_printf(&b, "/* %08X 00000000  4E 80 00 20 */\tblr\n.endfn %s\n\n", pos, fns[i].name);
        pos += (u32)fns[i].size;
    }
    return avp_buf_cz(&b);
}

/* ---- layout fixtures ----------------------------------------------------------------------------------------------- */
static inline const char *fx_retail_asm(void) {
    const FxAsmObj objs[] = {
        {"lbl_80300000", 0, 3, FX_BODY(".string \"ab\"")},
        {"gap_07_80300003_data", 3, 1, FX_BODY(".byte 0x00")},
        {"lbl_80300004", 4, 3, FX_BODY(".string \"cd\"")},
        {"gap_07_80300007_data", 7, 1, FX_BODY(".byte 0x00")},
        {"Table", 8, 16,
         FX_BODY(".4byte 0x00000001", ".4byte lbl_80300000", ".4byte 0x00000003", ".2byte 0x0004", ".2byte 0x0005")},
        {"gap_07_80300018_data", 0x18, 0x18, FX_BODY(".skip 0x18")},
    };
    return fx_data_asm(".data", FX_START, FX_START + 0x30, objs, FX_N(objs));
}
static inline AvpStr fx_retail_blob(void) {
    AvpBuf b;
    avp_buf_init(&b, avp_arena());
    avp_buf_putn(&b, "ab\0\0cd\0\0", 8);
    fx_be32(&b, 1);
    fx_be32(&b, FX_START);
    fx_be32(&b, 3);
    fx_be16(&b, 4);
    fx_be16(&b, 5);
    while (b.n < 0x30) avp_buf_putc(&b, 0);
    return avp_buf_str(&b);
}

typedef struct {
    AvpStr s0, s1;
    u32 w[5];
    int order[3];
    int table_size;
    bool ref_second;
} FxBuilt;
static inline FxBuilt fx_built_defaults(void) {
    FxBuilt o = {0};
    o.s0 = AVP_S("ab\0");
    o.s1 = AVP_S("cd\0");
    u32 w[5] = {1, 0, 3, 4, 5};
    memcpy(o.w, w, sizeof w);
    o.order[0] = 0; o.order[1] = 1; o.order[2] = 2;
    o.table_size = 16;
    o.ref_second = true;
    return o;
}
/* TestCase.built: a relocatable object with .data (two strings and a table whose word 1 is relocated to @1) and a
 * .text function referencing them. */
static inline AvpStr fx_built(const FxBuilt *o) {
    AvpArena *a = avp_arena();
    AvpBuf tab;
    avp_buf_init(&tab, a);
    fx_be32(&tab, o->w[0]);
    fx_be32(&tab, o->w[1]);
    fx_be32(&tab, o->w[2]);
    fx_be16(&tab, o->w[3]);
    fx_be16(&tab, o->w[4]);
    while ((int)tab.n < o->table_size) avp_buf_putc(&tab, 0);
    AvpStr data[3], pad0, pad1;
    pad0 = fx_cat(o->s0, fx_fill(0, 4 > o->s0.n ? 4 - o->s0.n : 0));
    pad1 = fx_cat(o->s1, fx_fill(0, 4 > o->s1.n ? 4 - o->s1.n : 0));
    data[0] = pad0;
    data[1] = pad1;
    data[2] = avp_buf_str(&tab);
    const char *nm[3] = {"@1", "@2", "Table"};
    int sz[3] = {(int)o->s0.n, (int)o->s1.n, o->table_size};
    FxElfSym syms[8];
    size_t nsym = 0;
    u32 idx[3] = {0}, offs[3] = {0};
    syms[nsym++] = (FxElfSym){"x.c", 0, 0, GC_STT_FILE, 0, GC_SHN_ABS};
    AvpBuf blob;
    avp_buf_init(&blob, a);
    for (int k = 0; k < 3; k++) {
        int i = o->order[k];
        syms[nsym++] = (FxElfSym){nm[i], (u32)blob.n, (u32)sz[i], GC_STT_OBJECT, 0, 1};
        idx[i] = (u32)nsym;
        offs[i] = (u32)blob.n;
        avp_buf_put(&blob, data[i]);
    }
    syms[nsym++] = (FxElfSym){"func", 0, 16, GC_STT_FUNC, 1, 2};
    FxElfRel rels[5];
    size_t nrel = 0;
    rels[nrel++] = (FxElfRel){1, offs[2] + 4, idx[0], 1, 0};
    rels[nrel++] = (FxElfRel){2, 0, idx[2], 4, 0};
    rels[nrel++] = (FxElfRel){2, 4, idx[0], 4, 0};
    if (o->ref_second) rels[nrel++] = (FxElfRel){2, 8, idx[1], 4, 0};
    FxElfSec secs[2] = {{".data", 1, 3, 8, avp_buf_str(&blob), 0, 0},
                        {".text", 1, 6, 4, AVP_S("`\0\0\0`\0\0\0`\0\0\0`\0\0\0"), 0, 0}};
    return fx_elf(secs, 2, syms, nsym, rels, nrel);
}

/* TestCase.layout: parse_asm_data, built_sections, compare_unit */
static inline GcLayoutResult *fx_result_new(void) {
    GcLayoutResult *res = AVP_NEW(avp_arena(), GcLayoutResult);
    res->unit = "u";
    res->status = "Matching";
    res->sections = avp_map_new();
    return res;
}
static inline GcLayoutResult *fx_layout(AvpStr elf_bytes, const char *asm_text, AvpStr blob, const GcDedupeVec *dedupe) {
    AvpArena *a = avp_arena();
    ShsmDol *d = fx_dol_parse(fx_dol1(7, FX_START, blob));
    GcAsmUnit *au = gc_parse_asm_data(a, avp_str(asm_text), d);
    GcElf *e = fx_elf_parse(elf_bytes);
    GcBuiltSections *bs = gc_built_sections(a, e, au->names, dedupe);
    GcLayoutResult *res = fx_result_new();
    gc_compare_unit(a, res, au, bs, NULL);
    return res;
}

/* kinds(res, difference): how many findings of that kind (NULL: any kind) */
static inline size_t fx_count(const GcLayoutResult *r, const char *kind, bool difference) {
    size_t n = 0;
    for (size_t i = 0; i < r->findings.n; i++) {
        const GcFinding *f = &r->findings.v[i];
        if (f->difference == difference && (!kind || strcmp(f->kind, kind) == 0)) n++;
    }
    return n;
}
static inline bool fx_has(const GcLayoutResult *r, const char *kind, bool difference) {
    return fx_count(r, kind, difference) > 0;
}
/* assertEqual(kinds(res), [], texts) */
static inline void fx_check_no_differences(const GcLayoutResult *r) {
    for (size_t i = 0; i < r->findings.n; i++)
        if (r->findings.v[i].difference)
            avp_test__fail(__FILE__, __LINE__, "unexpected difference %s %s: %s", r->findings.v[i].kind,
                           r->findings.v[i].section, r->findings.v[i].text);
}
static inline const GcFinding *fx_find(const GcLayoutResult *r, const char *kind) {
    for (size_t i = 0; i < r->findings.n; i++)
        if (strcmp(r->findings.v[i].kind, kind) == 0) return &r->findings.v[i];
    return NULL;
}

/* ---- temporary repository root and the gatecheck executable --------------------------------------------------------- */
static inline const char *fx_tmp_root(void) {
    char *p = avp_mkdtemp(avp_arena(), "gc_test_", "", NULL);
    if (!p) avp_panic(__FILE__, __LINE__, "cannot create a temporary directory");
    return p;
}
static inline void fx_write(const char *root, const char *rel, const void *p, size_t n) {
    AvpArena *a = avp_arena();
    char *path = avp_path_join(a, root, rel);
    AvpStr dir = avp_path_dirname(path);
    if (!avp_makedirs(avp_cz(a, dir), true)) avp_panic(__FILE__, __LINE__, "makedirs %s", path);
    if (!avp_write_file(path, p, n)) avp_panic(__FILE__, __LINE__, "cannot write %s", path);
}
static inline void fx_write_text(const char *root, const char *rel, const char *text) {
    fx_write(root, rel, text, strlen(text));
}
static inline void fx_write_bytes(const char *root, const char *rel, AvpStr b) { fx_write(root, rel, b.p, b.n); }
static inline void fx_mkdirs(const char *root, const char *rel) {
    avp_makedirs(avp_path_join(avp_arena(), root, rel), true);
}
/* The three metadata files every attribute run reads. */
static inline void fx_write_meta(const char *root) {
    fx_write_text(root, "config/R5WEA4/splits.txt", FX_SPLITS);
    fx_write_text(root, "config/R5WEA4/symbols.txt", FX_SYMBOLS);
    fx_write_text(root, "configure.py", FX_CONFIGURE);
}

typedef struct {
    i64 rc;
    AvpStr out, err;
} FxRun;
/* gatecheck --root <root> <args...> (args NULL-terminated); the test executable sits in build/tools/tests, the tool
 * in build/tools. */
static inline FxRun fx_gatecheck(const char *root, const char *const *args) {
    AvpArena *a = avp_arena();
    const char *exe = avp_path_normpath(a, avp_sprintf(a, "%s/../gatecheck.exe", avp_exe_dir()));
    const char *argv[32];
    size_t n = 0;
    argv[n++] = exe;
    argv[n++] = "--root";
    argv[n++] = root;
    for (; *args; args++) argv[n++] = *args;
    argv[n] = NULL;
    AvpProcSpec spec = {0};
    spec.argv = argv;
    spec.out = AVP_IO_CAPTURE;
    spec.err = AVP_IO_CAPTURE;
    spec.text = true;
    AvpProcResult r = avp_run(&spec);
    if (r.status != AVP_PROC_OK) avp_panic(__FILE__, __LINE__, "cannot run %s: %s", exe, r.message ? r.message : "?");
    return (FxRun){r.returncode, r.out, r.err};
}
#define FX_ARGS(...) ((const char *const[]){__VA_ARGS__, NULL})
