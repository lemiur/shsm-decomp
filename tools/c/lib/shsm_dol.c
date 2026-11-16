/* The DOL reader (shsm_dol.h): gatecheck.py Dol/Segment and codecmp.py Dol. */
#include "shsm_dol.h"

#include "avp/fs.h"
#include "shsm_cli.h"

#include <string.h>

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

ShsmDol *shsm_dol_parse(AvpArena *a, const u8 *data, size_t size) {
    if (size < SHSM_DOL_HEADER_SIZE) shsm_raise(SHSM_EXC_VALUE, "DOL too short (%zu bytes)", size);
    ShsmDol *d = AVP_NEW(a, ShsmDol);
    d->data = data;
    d->size = size;
    for (int i = 0; i < SHSM_DOL_SECTIONS; i++) {
        d->offs[i] = be32(data + 4 * i);
        d->addrs[i] = be32(data + 72 + 4 * i);
        d->sizes[i] = be32(data + 144 + 4 * i);
    }
    d->bss_addr = be32(data + 0xD8);
    d->bss_size = be32(data + 0xDC);
    d->entry = be32(data + 0xE0);
    for (int i = 0; i < SHSM_DOL_SECTIONS; i++) {
        if (d->sizes[i] == 0) continue;
        if ((u64)d->offs[i] + d->sizes[i] > size)
            shsm_raise(SHSM_EXC_VALUE, "DOL section %d runs past the end of the file", i);
        ShsmDolSegment *s = &d->segs[d->nsegs++];
        s->index = i;
        s->off = d->offs[i];
        s->addr = d->addrs[i];
        s->size = d->sizes[i];
        s->data = data + d->offs[i];
    }
    return d;
}

ShsmDol *shsm_dol_load(AvpArena *a, const char *path) {
    AvpBytes bytes = {0};
    if (!avp_read_all(a, path, &bytes)) shsm_raise(SHSM_EXC_OS, "cannot read %s: %s", path, avp_io_err_str(a).p);
    return shsm_dol_parse(a, bytes.v ? bytes.v : (const u8 *)"", bytes.n);
}

const char *shsm_dol_seg_name(const ShsmDolSegment *s, char buf[4]) {
    int k = s->index < SHSM_DOL_TEXT_SECTIONS ? s->index : s->index - SHSM_DOL_TEXT_SECTIONS;
    buf[0] = s->index < SHSM_DOL_TEXT_SECTIONS ? 'T' : 'D';
    if (k >= 10) {
        buf[1] = '1';
        buf[2] = (char)('0' + k - 10);
        buf[3] = 0;
    } else {
        buf[1] = (char)('0' + k);
        buf[2] = 0;
    }
    return buf;
}

const ShsmDolSegment *shsm_dol_segment_at(const ShsmDol *d, i64 addr) {
    for (size_t k = 0; k < d->nsegs; k++)
        if (d->segs[k].addr <= addr && addr < shsm_dol_seg_end(&d->segs[k])) return &d->segs[k];
    return NULL;
}

i64 shsm_dol_file_offset(const ShsmDol *d, i64 addr) {
    const ShsmDolSegment *s = shsm_dol_segment_at(d, addr);
    return s ? s->off + (addr - s->addr) : -1;
}

bool shsm_dol_read(AvpArena *a, const ShsmDol *d, i64 addr, i64 n, AvpStr *out) {
    if (n <= 0) {
        *out = avp_strn("", 0);
        return true;
    }
    char *buf = avp_alloc(a, (size_t)n + 1);
    i64 pos = addr, end = addr + n;
    while (pos < end) {
        const ShsmDolSegment *s = shsm_dol_segment_at(d, pos);
        if (!s) return false;
        i64 stop = end < shsm_dol_seg_end(s) ? end : shsm_dol_seg_end(s);
        memcpy(buf + (pos - addr), s->data + (pos - s->addr), (size_t)(stop - pos));
        pos = stop;
    }
    *out = avp_strn(buf, (size_t)n);
    return true;
}

bool shsm_dol_read_within(const ShsmDol *d, i64 addr, i64 n, AvpStr *out) {
    for (size_t k = 0; k < d->nsegs; k++) {
        const ShsmDolSegment *s = &d->segs[k];
        if (s->addr <= addr && addr + n <= shsm_dol_seg_end(s)) {
            *out = avp_strn((const char *)s->data + (addr - s->addr), (size_t)n);
            return true;
        }
    }
    return false;
}

bool shsm_dol_in_bss(const ShsmDol *d, i64 addr) {
    return d->bss_addr <= addr && addr < (i64)d->bss_addr + d->bss_size;
}
