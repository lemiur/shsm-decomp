/* sibling: the sibling ELF's symbol table and __vt__ objects, and the pairing by retail address (sibling_vtables.h). */
#include "sibling_vtables.h"

#include "avp/fs.h"
#include "shsm_mangle.h"

#include <string.h>

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

typedef struct {
    u32 type, addr, off, size, link, entsize;
} Shdr;

/* The file bytes at a virtual address inside an allocated PROGBITS section; NULL when none holds [addr, addr+n). */
static const u8 *at_vaddr(const u8 *b, const Shdr *sh, u32 nsh, u32 addr, u32 n) {
    for (u32 i = 0; i < nsh; i++)
        if (sh[i].type == 1 && sh[i].addr && addr >= sh[i].addr && (u64)addr + n <= (u64)sh[i].addr + sh[i].size)
            return b + sh[i].off + (addr - sh[i].addr);
    return NULL;
}

SibElf *sibling_elf_parse(AvpArena *a, AvpStr elf) {
    const u8 *b = (const u8 *)elf.p;
    if (elf.n < 52 || memcmp(b, "\x7f" "ELF", 4) != 0 || b[4] != 1 || b[5] != 2) return NULL;
    u32 shoff = be32(b + 32), shentsize = be16(b + 46), shnum = be16(b + 48);
    if (shentsize < 40 || shoff > elf.n || (u64)shnum * shentsize > elf.n - shoff) return NULL;
    Shdr *sh = AVP_NEWN(a, Shdr, shnum + 1);
    for (u32 i = 0; i < shnum; i++) {
        const u8 *h = b + shoff + (size_t)i * shentsize;
        sh[i] = (Shdr){be32(h + 4), be32(h + 12), be32(h + 16), be32(h + 20), be32(h + 24), be32(h + 36)};
        if (sh[i].off > elf.n || sh[i].size > elf.n - sh[i].off) sh[i].type = 0; /* NOBITS or broken: never read */
    }
    SibElf *e = AVP_NEW(a, SibElf);
    e->by_name = avp_map_new();
    e->fn_at = avp_imap_new();
    for (u32 i = 0; i < shnum; i++) {
        if (sh[i].type != 2 || sh[i].link >= shnum) continue; /* SHT_SYMTAB */
        const Shdr *st = &sh[sh[i].link];
        size_t n = sh[i].size / 16;
        e->syms = AVP_NEWN(a, SibElfSym, n + 1);
        for (size_t k = 1; k < n; k++) {
            const u8 *s = b + sh[i].off + k * 16;
            u32 name = be32(s);
            if (name >= st->size) continue;
            const char *nm = (const char *)b + st->off + name;
            if (!memchr(nm, 0, st->size - name) || !*nm) continue;
            SibElfSym *y = &e->syms[e->nsyms];
            *y = (SibElfSym){avp_strdup(a, nm), be32(s + 4), be32(s + 8), (u8)(s[12] >> 4), (u8)(s[12] & 15)};
            if (be16(s + 14) == 0) continue; /* undefined */
            size_t idx = e->nsyms++;
            bool created;
            AvpVal *v = avp_map_put(e->by_name, avp_str(y->name), &created);
            if (created || (y->bind != 0 && e->syms[v->i - 1].bind == 0)) v->i = (i64)idx + 1;
            if (y->type == 2) {
                AvpVal *f = avp_imap_put(e->fn_at, y->address, &created);
                if (created || (y->bind != 0 && e->syms[f->i - 1].bind == 0)) f->i = (i64)idx + 1;
            }
        }
        break;
    }
    AVP_VEC(SibVtable) vts = {0};
    for (size_t k = 0; k < e->nsyms; k++) {
        const SibElfSym *y = &e->syms[k];
        if (strncmp(y->name, "__vt__", 6) != 0 || y->size < 8 || (y->size & 3)) continue;
        const u8 *p = at_vaddr(b, sh, shnum, y->address, y->size);
        if (!p) continue;
        SibVtable v = {.symbol = y->name, .address = y->address, .size = y->size, .weak = y->bind == 2};
        AvpTry t;
        ShsmDemangled *d = NULL;
        if (AVP_TRY(&t)) {
            d = shsm_mangle_parse(a, y->name);
            avp_try_end(&t);
        }
        if (d && d->nscope) {
            AvpBuf q;
            avp_buf_init(&q, a);
            for (size_t s = 0; s < d->nscope; s++) avp_buf_printf(&q, "%s%s", s ? "::" : "", d->scope[s]);
            v.cls = avp_cz(a, avp_buf_str(&q));
        } else {
            v.cls = y->name + 6;
        }
        v.n = (y->size - 8) / 4;
        v.slots = AVP_NEWN(a, const char *, v.n + 1);
        v.slot_addr = AVP_NEWN(a, u32, v.n + 1);
        for (size_t s = 0; s < v.n; s++) {
            u32 w = be32(p + 8 + 4 * s);
            v.slot_addr[s] = w;
            const char *fn = w ? sibling_elf_fn_at(e, w) : NULL;
            v.slots[s] = w ? (fn ? fn : "") : NULL;
        }
        avp_vec_push(&vts, v);
    }
    e->vt = vts.v;
    e->nvt = vts.n;
    return e;
}

const char *sibling_elf_path(AvpArena *a, Sibling *sb) {
    sqlite3_stmt *st = NULL;
    const char *v = NULL;
    if (sb->db && sqlite3_prepare_v2(sb->db, "select value from meta where key='elf'", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
            v = avp_strdup(a, (const char *)sqlite3_column_text(st, 0));
        sqlite3_finalize(st);
    }
    if (!v) v = shsm_repo_shared_path(a, sb->repo, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf");
    return v;
}

const SibElfSym *sibling_elf_sym(const SibElf *e, const char *name) {
    AvpVal *v = avp_map_get(e->by_name, avp_str(name));
    return v ? &e->syms[v->i - 1] : NULL;
}

const char *sibling_elf_fn_at(const SibElf *e, u32 address) {
    AvpVal *v = avp_imap_get(e->fn_at, address);
    return v ? e->syms[v->i - 1].name : NULL;
}

const SibVtable *sibling_elf_vtable(const SibElf *e, const char *q) {
    for (size_t i = 0; i < e->nvt; i++)
        if (strcmp(e->vt[i].cls, q) == 0 || strcmp(e->vt[i].symbol, q) == 0) return &e->vt[i];
    return NULL;
}

AvpIMap *sibling_peer_map(AvpArena *a, Sibling *sb) {
    AvpIMap *m = avp_imap_new();
    sqlite3_stmt *st = NULL;
    if (!sb->db || sqlite3_prepare_v2(sb->db,
                                      "select s.address, f.name, f.address, s.tier from shsm s join functions f on"
                                      " f.address = s.peer where s.peer is not null",
                                      -1, &st, NULL) != SQLITE_OK)
        return m;
    while (sqlite3_step(st) == SQLITE_ROW) {
        SibPeer *p = AVP_NEW(a, SibPeer);
        p->name = avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
        p->address = (u32)sqlite3_column_int64(st, 2);
        int tier = sqlite3_column_int(st, 3);
        p->tier = tier > 0 && tier < SIB_NTIERS ? SIBLING_TIERS[tier] : "";
        avp_imap_put(m, (u64)sqlite3_column_int64(st, 0), NULL)->p = p;
    }
    sqlite3_finalize(st);
    return m;
}
