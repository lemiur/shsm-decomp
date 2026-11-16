/* The compiled object's ELF reader (tools/codecmp.py: Elf). */
#include "cc_elf.h"

#include <stdlib.h>

static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static u16 be16(const u8 *p) { return (u16)(((u32)p[0] << 8) | p[1]); }

/* data[off:off+n] with the clamping of a Python slice of non-negative bounds. */
static AvpStr slice(AvpStr d, u64 off, u64 n) {
    if (off >= d.n) return avp_strn(d.p + d.n, 0);
    u64 end = off + n;
    if (end > d.n) end = d.n;
    return avp_strn(d.p + off, (size_t)(end - off));
}

/* A NUL-terminated string at data[base:] decoded as Latin-1, kept as UTF-8. */
static AvpStr latin1_cstr(AvpArena *a, AvpStr d, u64 base) {
    if (base >= d.n) return avp_strn("", 0);
    const u8 *p = (const u8 *)d.p + base;
    size_t len = 0, extra = 0;
    while (base + len < d.n && p[len]) {
        if (p[len] >= 0x80) extra++;
        len++;
    }
    char *out = avp_alloc_noz(a, len + extra + 1);
    size_t k = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] < 0x80) {
            out[k++] = (char)p[i];
        } else {
            out[k++] = (char)(0xC0 | (p[i] >> 6));
            out[k++] = (char)(0x80 | (p[i] & 0x3F));
        }
    }
    out[k] = 0;
    return avp_strn(out, k);
}

static int cmp_reloc(const void *x, const void *y) {
    const CcReloc *a = x, *b = y;
    return a->offset < b->offset ? -1 : a->offset > b->offset;
}

CcElf *cc_elf_parse(AvpArena *a, const void *data, size_t n, const char **why) {
    const u8 *d = data;
    *why = NULL;
    if (n < 52 || memcmp(d, "\x7f" "ELF", 4) != 0 || d[4] != 1 || d[5] != 2) {
        *why = "not a 32-bit big-endian ELF object";
        return NULL;
    }
    CcElf *e = AVP_NEW(a, CcElf);
    e->bytes = avp_strn((const char *)d, n);
    u32 shoff = be32(d + 32);
    u16 shentsize = be16(d + 46), shnum = be16(d + 48), shstrndx = be16(d + 50);
    u32 (*raw)[10] = avp_alloc(a, sizeof(u32[10]) * (shnum ? shnum : 1));
    for (u32 i = 0; i < shnum; i++) {
        u64 at = (u64)shoff + (u64)i * shentsize;
        for (int k = 0; k < 10; k++) raw[i][k] = at + 4 * (u64)k + 4 <= n ? be32(d + at + 4 * k) : 0;
    }
    e->nsections = shnum;
    e->sections = AVP_NEWN(a, CcSection, shnum ? shnum : 1);
    for (u32 i = 0; i < shnum; i++) {
        CcSection *s = &e->sections[i];
        s->name = shstrndx < shnum ? latin1_cstr(a, e->bytes, (u64)raw[shstrndx][4] + raw[i][0]) : avp_strn("", 0);
        s->type = raw[i][1];
        s->flags = raw[i][2];
        s->offset = raw[i][4];
        s->size = raw[i][5];
        s->link = raw[i][6];
        s->info = raw[i][7];
        s->data = s->type == CC_SHT_NOBITS ? avp_strn("", 0) : slice(e->bytes, s->offset, s->size);
    }
    AVP_VEC(CcSym) syms = {0};
    for (u32 i = 0; i < shnum; i++) {
        CcSection *s = &e->sections[i];
        if (s->type != CC_SHT_SYMTAB) continue;
        u32 strtab = s->link;
        for (u32 k = 0; k < s->size / 16 && (u64)k * 16 + 16 <= s->data.n; k++) {
            const u8 *p = (const u8 *)s->data.p + (size_t)k * 16;
            CcSym sym;
            u32 nm = be32(p);
            sym.name = strtab < shnum ? latin1_cstr(a, e->bytes, (u64)raw[strtab][4] + nm) : avp_strn("", 0);
            sym.value = be32(p + 4);
            sym.size = be32(p + 8);
            sym.bind = p[12] >> 4;
            sym.type = p[12] & 15;
            sym.shndx = be16(p + 14);
            avp_vec_push(&syms, sym);
        }
    }
    e->nsymbols = syms.n;
    e->symbols = AVP_NEWN(a, CcSym, syms.n ? syms.n : 1);
    if (syms.n) memcpy(e->symbols, syms.v, syms.n * sizeof(CcSym));
    avp_vec_free(&syms);
    e->functions = avp_map_new();
    for (size_t k = 0; k < e->nsymbols; k++) {
        if (e->symbols[k].type != CC_STT_FUNC) continue;
        bool created;
        AvpVal *v = avp_map_put(e->functions, e->symbols[k].name, &created);
        if (created) v->i = (i64)k;
    }
    /* relocs[target section][offset] = Reloc, later entries replacing earlier ones */
    for (u32 t = 0; t < shnum; t++) {
        AvpIMap *table = NULL;
        for (u32 i = 0; i < shnum; i++) {
            CcSection *s = &e->sections[i];
            if (s->type != CC_SHT_RELA || s->info != t) continue;
            if (!table) table = avp_imap_new();
            for (u32 k = 0; k < s->size / 12 && (u64)k * 12 + 12 <= s->data.n; k++) {
                const u8 *p = (const u8 *)s->data.p + (size_t)k * 12;
                CcReloc *r = AVP_NEW(a, CcReloc);
                r->offset = be32(p);
                u32 info = be32(p + 4);
                r->type = info & 0xFF;
                r->sym = info >> 8;
                r->addend = (i32)be32(p + 8);
                avp_imap_put(table, r->offset, NULL)->p = r;
            }
        }
        if (!table) continue;
        size_t cnt = avp_imap_len(table), k = 0;
        CcReloc *arr = AVP_NEWN(a, CcReloc, cnt ? cnt : 1);
        AvpIMapIter it = {0};
        while (avp_imap_next(table, &it)) arr[k++] = *(const CcReloc *)it.val->p;
        qsort(arr, cnt, sizeof(CcReloc), cmp_reloc);
        e->sections[t].relocs = arr;
        e->sections[t].nrelocs = cnt;
        avp_imap_free(table);
    }
    return e;
}

const char *cc_reloc_type_name(AvpArena *a, u32 type) {
    switch (type) {
    case 1: return "ADDR32";
    case 4: return "ADDR16_LO";
    case 5: return "ADDR16_HI";
    case 6: return "ADDR16_HA";
    case 10: return "REL24";
    case 11: return "REL14";
    case 109: return "EMB_SDA21";
    case 107: return "EMB_SDA2I16";
    case 108: return "EMB_SDA2REL";
    case 32: return "SDAREL16";
    default: return avp_sprintf(a, "R_PPC_%u", type);
    }
}

AvpStr cc_elf_section_name(const CcElf *e, u32 shndx) {
    return 0 < shndx && shndx < e->nsections ? e->sections[shndx].name : avp_strn("", 0);
}

const CcReloc *cc_elf_reloc_exact(const CcElf *e, u32 shndx, i64 offset) {
    if (shndx >= e->nsections || offset < 0 || offset > 0xFFFFFFFFll) return NULL;
    const CcSection *s = &e->sections[shndx];
    size_t lo = 0, hi = s->nrelocs;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (s->relocs[mid].offset < (u64)offset) lo = mid + 1;
        else hi = mid;
    }
    return lo < s->nrelocs && s->relocs[lo].offset == (u64)offset ? &s->relocs[lo] : NULL;
}

const CcReloc *cc_elf_reloc_at(const CcElf *e, u32 shndx, i64 offset) {
    const CcReloc *r = cc_elf_reloc_exact(e, shndx, offset);
    return r ? r : cc_elf_reloc_exact(e, shndx, offset + 2);
}

const CcSym *cc_elf_function(const CcElf *e, AvpStr name) {
    AvpVal *v = avp_map_get(e->functions, name);
    return v ? &e->symbols[v->i] : NULL;
}

const CcSym *cc_elf_find_symbol(const CcElf *e, AvpStr name, i64 shndx) {
    for (size_t k = 0; k < e->nsymbols; k++)
        if (avp_str_eq(e->symbols[k].name, name) && (shndx < 0 || e->symbols[k].shndx == shndx)) return &e->symbols[k];
    return NULL;
}
