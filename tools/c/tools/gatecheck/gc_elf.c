/* gatecheck: shared helpers and the ELF32 big-endian reader (gatecheck.py Elf, ElfSection, ElfSym, ElfReloc). */
#include "gatecheck.h"

#include <stdarg.h>

void gc_raise(int exc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *msg = avp_vsprintf(avp_scratch(), fmt, ap);
    va_end(ap);
    avp_throw(exc, "%s", msg);
}

bool gc_is_gate(int code) { return code == GC_EXC_GATE || code == SHSM_EXC_VALUE || code == SHSM_EXC_OS; }

char *gc_hex(AvpArena *a, i64 v) {
    if (v < 0) return avp_sprintf(a, "-%llX", (unsigned long long)(-v));
    return avp_sprintf(a, "%llX", (unsigned long long)v);
}

char *gc_hx(AvpArena *a, i64 v, int width) {
    if (v < 0) {
        /* "%0*X" % (w, -v): the sign counts toward the width */
        char *digits = avp_sprintf(a, "%llX", (unsigned long long)(-v));
        int pad = width - 1 - (int)strlen(digits);
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "0x-");
        for (int i = 0; i < pad; i++) avp_buf_putc(&b, '0');
        avp_buf_putz(&b, digits);
        return avp_buf_cz(&b);
    }
    return avp_sprintf(a, "0x%0*llX", width, (unsigned long long)v);
}

AvpStr gc_cp_prefix(AvpStr s, size_t n) {
    size_t i = 0, cps = 0;
    while (i < s.n && cps < n) {
        i++;
        while (i < s.n && ((u8)s.p[i] & 0xC0) == 0x80) i++;
        cps++;
    }
    return avp_strn(s.p, i);
}

/* ---- ELF ------------------------------------------------------------------------------------------------------- */

static void need(const GcElf *e, i64 off, i64 size) {
    if (off + size > (i64)e->n)
        gc_raise(GC_EXC_STRUCT,
                 "unpack_from requires a buffer of at least %lld bytes for unpacking %lld bytes at offset %lld "
                 "(actual buffer size is %lld)",
                 (long long)(off + size), (long long)size, (long long)off, (long long)e->n);
}
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

/* _cstr: data[off:data.index(b"\0", off)].decode("latin-1") */
static const char *cstr(AvpArena *a, const GcElf *e, i64 off) {
    if (off > (i64)e->n) gc_raise(GC_EXC_VALUE, "subsection not found");
    const u8 *p = e->data + off;
    const u8 *z = memchr(p, 0, e->n - (size_t)off);
    if (!z) gc_raise(GC_EXC_VALUE, "subsection not found");
    size_t len = (size_t)(z - p), extra = 0;
    for (size_t i = 0; i < len; i++) extra += p[i] >= 0x80;
    if (!extra) return avp_cz(a, avp_strn((const char *)p, len));
    char *out = avp_alloc(a, len + extra + 1);
    size_t k = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] < 0x80) {
            out[k++] = (char)p[i];
        } else {
            out[k++] = (char)(0xC0 | p[i] >> 6);
            out[k++] = (char)(0x80 | (p[i] & 0x3F));
        }
    }
    return out;
}

GcElf *gc_elf_parse(AvpArena *a, const u8 *data, size_t n) {
    GcElf *e = AVP_NEW(a, GcElf);
    e->data = data;
    e->n = n;
    if (n < 52 || memcmp(data, "\x7f" "ELF", 4) != 0) gc_raise(GC_EXC_GATE, "not an ELF file");
    if (data[4] != 1 || data[5] != 2) gc_raise(GC_EXC_GATE, "expected ELF32 big-endian");
    need(e, 16, 36);
    i64 shoff = be32(data + 32);
    i64 shentsize = be16(data + 46), shnum = be16(data + 48), shstrndx = be16(data + 50);
    e->secs = AVP_NEWN(a, GcSec, (size_t)shnum + 1);
    for (i64 i = 0; i < shnum; i++) {
        i64 at = shoff + i * shentsize;
        need(e, at, 40);
        const u8 *p = data + at;
        GcSec *s = &e->secs[i];
        s->index = i;
        s->name_off = be32(p);
        s->type = be32(p + 4);
        s->flags = be32(p + 8);
        s->addr = be32(p + 12);
        s->off = be32(p + 16);
        s->size = be32(p + 20);
        s->link = be32(p + 24);
        s->info = be32(p + 28);
        s->align = be32(p + 32);
        s->entsize = be32(p + 36);
        s->name = "";
    }
    e->nsecs = (size_t)shnum;
    if (shnum && shstrndx < shnum) {
        i64 so = e->secs[shstrndx].off;
        for (i64 i = 0; i < shnum; i++) e->secs[i].name = cstr(a, e, so + e->secs[i].name_off);
    }
    return e;
}

GcElf *gc_elf_load(AvpArena *a, const char *path) {
    AvpBytes bytes = {0};
    if (!avp_read_all(a, path, &bytes)) gc_raise(GC_EXC_GATE, "cannot read %s: %s", path, avp_io_err_str(a).p);
    return gc_elf_parse(a, bytes.v ? bytes.v : (const u8 *)"", bytes.n);
}

void gc_elf_symbols(AvpArena *a, GcElf *e) {
    if (e->syms_loaded) return;
    AVP_VEC(GcSym) out = {0};
    for (size_t k = 0; k < e->nsecs; k++) {
        const GcSec *s = &e->secs[k];
        if (s->type != GC_SHT_SYMTAB) continue;
        if (s->link >= (i64)e->nsecs) gc_raise(GC_EXC_INDEX, "list index out of range");
        i64 strtab = e->secs[s->link].off;
        for (i64 i = 0; i < s->size / 16; i++) {
            i64 at = s->off + i * 16;
            need(e, at, 16);
            const u8 *p = e->data + at;
            GcSym y;
            i64 nm = be32(p);
            y.index = i;
            y.value = be32(p + 4);
            y.size = be32(p + 8);
            y.bind = p[12] >> 4;
            y.type = p[12] & 15;
            y.shndx = be16(p + 14);
            y.name = nm ? cstr(a, e, strtab + nm) : "";
            avp_vec_push(&out, y);
        }
        break;
    }
    e->syms = out.n ? avp_memdup(a, out.v, out.n * sizeof(GcSym)) : NULL;
    e->nsyms = out.n;
    avp_vec_free(&out);
    e->syms_loaded = true;
}

AvpStr gc_elf_section_data(const GcElf *e, const GcSec *s) {
    if (s->type == GC_SHT_NOBITS) return AVP_S("");
    i64 lo = s->off < (i64)e->n ? s->off : (i64)e->n;
    i64 hi = s->off + s->size < (i64)e->n ? s->off + s->size : (i64)e->n;
    return avp_strn((const char *)e->data + lo, (size_t)(hi - lo));
}

void gc_elf_relocs_of(const GcElf *e, const GcSec *s, GcRelVec *out) {
    if (s->type == GC_SHT_RELA) {
        for (i64 i = 0; i < s->size / 12; i++) {
            i64 at = s->off + i * 12;
            need(e, at, 12);
            const u8 *p = e->data + at;
            u32 info = be32(p + 4);
            GcRel r = {be32(p), info >> 8, info & 0xFF, (i32)be32(p + 8)};
            avp_vec_push(out, r);
        }
    } else {
        for (i64 i = 0; i < s->size / 8; i++) {
            i64 at = s->off + i * 8;
            need(e, at, 8);
            const u8 *p = e->data + at;
            u32 info = be32(p + 4);
            GcRel r = {be32(p), info >> 8, info & 0xFF, 0};
            avp_vec_push(out, r);
        }
    }
}

void gc_elf_relocs_for(const GcElf *e, i64 sec_index, GcRelVec *out) {
    for (size_t k = 0; k < e->nsecs; k++) {
        const GcSec *s = &e->secs[k];
        if ((s->type != GC_SHT_RELA && s->type != GC_SHT_REL) || s->info != sec_index) continue;
        gc_elf_relocs_of(e, s, out);
    }
}
