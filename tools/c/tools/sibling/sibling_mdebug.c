/* sibling_mdebug: ELF32 and ECOFF .mdebug reading (sibling_mdebug.h). */
#include "sibling_mdebug.h"

#include <string.h>

static u32 rd32(const u8 *p, bool be) {
    return be ? (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]
              : (u32)p[3] << 24 | (u32)p[2] << 16 | (u32)p[1] << 8 | p[0];
}
static u16 rd16(const u8 *p, bool be) { return be ? (u16)(p[0] << 8 | p[1]) : (u16)(p[1] << 8 | p[0]); }

static bool in_image(size_t size, u64 off, u64 n) { return off <= size && n <= size - off; }

static const char *cstr(const u8 *data, size_t size, u64 off) {
    if (off >= size) return NULL;
    const u8 *e = memchr(data + off, 0, size - off);
    return e ? (const char *)data + off : NULL;
}

bool sib_elf_parse(AvpArena *a, const u8 *data, size_t size, SibElf *out, const char **err) {
    memset(out, 0, sizeof *out);
    if (size < 52 || memcmp(data, "\x7f" "ELF", 4) != 0 || data[4] != 1) {
        *err = "not an ELF32 file";
        return false;
    }
    bool be = data[5] == 2;
    out->data = data;
    out->size = size;
    out->big_endian = be;
    out->machine = rd16(data + 18, be);
    u32 shoff = rd32(data + 32, be);
    u16 shentsize = rd16(data + 46, be), shnum = rd16(data + 48, be), shstrndx = rd16(data + 50, be);
    if (shentsize < 40 || !in_image(size, shoff, (u64)shentsize * shnum)) {
        *err = "section header table outside the file";
        return false;
    }
    u32 stroff = 0, strsize = 0;
    if (shstrndx < shnum) {
        const u8 *sh = data + shoff + (u64)shstrndx * shentsize;
        stroff = rd32(sh + 16, be);
        strsize = rd32(sh + 20, be);
    }
    for (u16 k = 0; k < shnum; k++) {
        const u8 *sh = data + shoff + (u64)k * shentsize;
        SibElfSec s = {0};
        u32 nm = rd32(sh, be);
        s.name = nm < strsize ? cstr(data, size, (u64)stroff + nm) : NULL;
        if (!s.name) s.name = "";
        s.type = rd32(sh + 4, be);
        s.flags = rd32(sh + 8, be);
        s.addr = rd32(sh + 12, be);
        s.offset = rd32(sh + 16, be);
        s.size = rd32(sh + 20, be);
        s.link = rd32(sh + 24, be);
        s.entsize = rd32(sh + 36, be);
        avp_vec_push(&out->secs, s);
    }
    const SibElfSec *st = sib_elf_section(out, ".symtab");
    if (st && st->link < out->secs.n && in_image(size, st->offset, st->size)) {
        const SibElfSec *ss = &out->secs.v[st->link];
        for (u64 o = 0; o + 16 <= st->size; o += 16) {
            const u8 *p = data + st->offset + o;
            SibElfSym y = {0};
            u32 nm = rd32(p, be);
            y.name = nm < ss->size ? cstr(data, size, (u64)ss->offset + nm) : NULL;
            if (!y.name) y.name = "";
            y.value = rd32(p + 4, be);
            y.size = rd32(p + 8, be);
            y.bind = p[12] >> 4;
            y.type = p[12] & 15;
            y.shndx = rd16(p + 14, be);
            avp_vec_push(&out->syms, y);
        }
    }
    (void)a;
    return true;
}

const SibElfSec *sib_elf_section(const SibElf *e, const char *name) {
    for (size_t k = 0; k < e->secs.n; k++)
        if (strcmp(e->secs.v[k].name, name) == 0) return &e->secs.v[k];
    return NULL;
}

/* ---- ECOFF ---- */



static void read_symr(const u8 *p, bool be, SibSymr *o) {
    o->iss = (i32)rd32(p, be);
    o->value = (i32)rd32(p + 4, be);
    u32 w = rd32(p + 8, be);
    if (be) {
        o->st = (u8)(w >> 26);
        o->sc = (u8)((w >> 21) & 31);
        o->index = w & 0xFFFFF;
    } else {
        o->st = (u8)(w & 63);
        o->sc = (u8)((w >> 6) & 31);
        o->index = w >> 12;
    }
}

bool sib_md_parse(AvpArena *a, const u8 *data, size_t size, u32 off, u32 len, SibMdebug *m, const char **err) {
    memset(m, 0, sizeof *m);
    if (!in_image(size, off, len) || len < 0x60) {
        *err = ".mdebug outside the file";
        return false;
    }
    const u8 *h = data + off;
    bool be = h[0] == 0x70 && h[1] == 0x09;
    if (!be && !(h[0] == 0x09 && h[1] == 0x70)) {
        *err = ".mdebug: not an ECOFF symbolic header (magic 0x7009)";
        return false;
    }
    m->be = be;
    m->data = data;
    m->size = size;
    m->magic = rd16(h, be);
    m->vstamp = rd16(h + 2, be);
#define F(o) ((i32)rd32(h + (o), be))
    m->ilineMax = F(4);
    m->ipdMax = F(24);
    m->isymMax = F(32);
    m->ioptMax = F(40);
    m->iauxMax = F(48);
    m->issMax = F(56);
    m->issExtMax = F(64);
    m->ifdMax = F(72);
    m->crfd = F(80);
    m->iextMax = F(88);
    u32 offs[6] = {(u32)F(36), (u32)F(60), (u32)F(68), (u32)F(76), (u32)F(28), (u32)F(92)};
#undef F
    /* file offsets, or (outside the section) section-relative */
    for (int k = 0; k < 6; k++)
        if (offs[k] && !(offs[k] >= off && offs[k] < (u64)off + len)) offs[k] += off;
    m->cbSymOffset = offs[0];
    m->cbSsOffset = offs[1];
    m->cbSsExtOffset = offs[2];
    m->cbFdOffset = offs[3];
    m->cbPdOffset = offs[4];
    m->cbExtOffset = offs[5];
    if (m->ifdMax > 0 && in_image(size, m->cbFdOffset, (u64)m->ifdMax * 72)) {
        for (i32 k = 0; k < m->ifdMax; k++) {
            const u8 *p = data + m->cbFdOffset + (u64)k * 72;
            SibFdr f = {0};
            f.adr = rd32(p, be);
            f.rss = (i32)rd32(p + 4, be);
            f.issBase = (i32)rd32(p + 8, be);
            f.cbSs = (i32)rd32(p + 12, be);
            f.isymBase = (i32)rd32(p + 16, be);
            f.csym = (i32)rd32(p + 20, be);
            f.ipdFirst = rd16(p + 40, be);
            f.cpd = (i16)rd16(p + 42, be);
            f.iauxBase = (i32)rd32(p + 44, be);
            f.caux = (i32)rd32(p + 48, be);
            u32 bits = rd32(p + 60, be);
            f.lang = be ? (u8)(bits >> 27) : (u8)(bits & 31);
            avp_vec_push(&m->fds, f);
        }
        for (i32 k = 0; k < m->ifdMax; k++) {
            SibFdr *f = &m->fds.v[k];
            f->name = sib_md_lstr(m, k, f->rss);
            if (!f->name) f->name = "";
        }
    }
    if (m->ipdMax > 0 && in_image(size, m->cbPdOffset, (u64)m->ipdMax * 52)) {
        for (i32 fd = 0; fd < (i32)m->fds.n; fd++) {
            const SibFdr *f = &m->fds.v[fd];
            for (i32 j = 0; j < f->cpd; j++) {
                i32 ip = f->ipdFirst + j;
                if (ip < 0 || ip >= m->ipdMax) break;
                const u8 *p = data + m->cbPdOffset + (u64)ip * 52;
                SibPdr d = {0};
                d.fd = fd;
                d.isym = (i32)rd32(p + 4, be);
                d.adr = f->adr + rd32(p, be);
                SibSymr y;
                if (sib_md_sym(m, fd, d.isym, &y)) {
                    d.name = sib_md_lstr(m, fd, y.iss);
                    if (y.st == SIB_ST_PROC || y.st == SIB_ST_STATICPROC) d.adr = (u32)y.value;
                }
                if (!d.name) d.name = "";
                avp_vec_push(&m->pds, d);
            }
        }
    }
    if (m->iextMax > 0 && in_image(size, m->cbExtOffset, (u64)m->iextMax * 16)) {
        for (i32 k = 0; k < m->iextMax; k++) {
            const u8 *p = data + m->cbExtOffset + (u64)k * 16;
            SibExtr x = {0};
            x.ifd = (i16)rd16(p + 2, be);
            SibSymr y;
            read_symr(p + 4, be, &y);
            x.value = y.value;
            x.st = y.st;
            x.sc = y.sc;
            x.name = (y.iss >= 0 && y.iss < m->issExtMax) ? cstr(data, size, (u64)m->cbSsExtOffset + (u32)y.iss) : NULL;
            if (!x.name) x.name = "";
            avp_vec_push(&m->exts, x);
        }
    }
    (void)a;
    return true;
}

bool sib_md_sym(const SibMdebug *m, i32 fd, i32 i, SibSymr *out) {
    if (fd < 0 || (size_t)fd >= m->fds.n) return false;
    const SibFdr *f = &m->fds.v[fd];
    if (i < 0 || i >= f->csym) return false;
    i64 k = (i64)f->isymBase + i;
    if (k < 0 || k >= m->isymMax) return false;
    u64 o = (u64)m->cbSymOffset + (u64)k * 12;
    if (!in_image(m->size, o, 12)) return false;
    read_symr(m->data + o, m->be, out);
    return true;
}

const char *sib_md_lstr(const SibMdebug *m, i32 fd, i32 iss) {
    if (fd < 0 || (size_t)fd >= m->fds.n) return NULL;
    const SibFdr *f = &m->fds.v[fd];
    if (iss < 0 || iss >= f->cbSs) return NULL;
    i64 k = (i64)f->issBase + iss;
    if (k < 0 || k >= m->issMax) return NULL;
    return cstr(m->data, m->size, (u64)m->cbSsOffset + (u64)k);
}

void sib_md_stabs(AvpArena *a, SibMdebug *m, i32 fd, SibStabVec *out) {
    if (fd < 0 || (size_t)fd >= m->fds.n) return;
    const SibFdr *f = &m->fds.v[fd];
    AvpBuf join = {0};
    bool joining = false;
    SibStab cur = {0};
    for (i32 i = 0; i < f->csym; i++) {
        SibSymr y;
        if (!sib_md_sym(m, fd, i, &y)) break;
        if ((y.index & 0xFFF00u) != SIB_STAB_MARK) continue;
        const char *s = sib_md_lstr(m, fd, y.iss);
        if (!s) s = "";
        size_t n = strlen(s);
        bool cont = n > 0 && s[n - 1] == '\\';
        if (!joining) {
            cur.type = (int)(y.index - SIB_STAB_MARK);
            cur.value = y.value;
            cur.str = s;
        }
        if (cont) {
            if (!joining) avp_buf_clear(&join);
            avp_buf_putn(&join, s, n - 1);
            joining = true;
            continue;
        }
        if (joining) {
            avp_buf_putn(&join, s, n);
            cur.str = avp_strdup(a, avp_buf_cz(&join));
            joining = false;
        }
        avp_vec_push(out, cur);
        m->stab_records++;
    }
    if (joining) {
        cur.str = avp_strdup(a, avp_buf_cz(&join));
        avp_vec_push(out, cur);
    }
    avp_buf_free(&join);
}
