/* The PS2 executable of witness.py: ISO 9660 and SYSTEM.CNF, find_exe/load_exe, the ELF reader, cstring_at. */
#include "witness_ps2.h"

#include "shsm_cli.h"

#include <string.h>

#define SECTOR 2048

/* Reads beyond the file: Python's struct.unpack_from / indexing failure (D-witness-1). */
AVP_NORETURN static void out_of_file(const char *what, i64 off) {
    shsm_raise(SHSM_EXC_VALUE, "%s: offset %lld is outside the file", what, (long long)off);
}

static u32 u32_at(const u8 *d, size_t n, i64 o) {
    if (o < 0 || (u64)o + 4 > n) out_of_file("unpack_from", o);
    return (u32)d[o] | (u32)d[o + 1] << 8 | (u32)d[o + 2] << 16 | (u32)d[o + 3] << 24;
}

static u32 u16_at(const u8 *d, size_t n, i64 o) {
    if (o < 0 || (u64)o + 2 > n) out_of_file("unpack_from", o);
    return (u32)d[o] | (u32)d[o + 1] << 8;
}

/* ---- text helpers -------------------------------------------------------------------------------------------- */

/* bytes.decode("latin-1") as UTF-8. */
static char *latin1_to_utf8(AvpArena *a, const u8 *p, size_t n) {
    char *out = avp_alloc(a, 2 * n + 1), *q = out;
    for (size_t k = 0; k < n; k++) {
        u8 c = p[k];
        if (c < 0x80) {
            *q++ = (char)c;
        } else {
            *q++ = (char)(0xC0 | (c >> 6));
            *q++ = (char)(0x80 | (c & 0x3F));
        }
    }
    *q = 0;
    return out;
}

/* Python's strict UTF-8 decoder accepts exactly this. */
static bool utf8_valid(const u8 *p, size_t n) {
    size_t k = 0;
    while (k < n) {
        u8 c = p[k];
        if (c < 0x80) {
            k++;
            continue;
        }
        size_t len;
        u8 lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) {
            len = 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            len = 3;
            if (c == 0xE0) lo = 0xA0;
            if (c == 0xED) hi = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            len = 4;
            if (c == 0xF0) lo = 0x90;
            if (c == 0xF4) hi = 0x8F;
        } else {
            return false;
        }
        if (k + len > n) return false;
        if (p[k + 1] < lo || p[k + 1] > hi) return false;
        for (size_t m = 2; m < len; m++)
            if ((p[k + m] & 0xC0) != 0x80) return false;
        k += len;
    }
    return true;
}

size_t wt_pystr_len(const char *s) {
    size_t n = 0;
    for (; *s; s++)
        if (((u8)*s & 0xC0) != 0x80) n++;
    return n;
}

const char *wt_cstring_at(AvpArena *a, const u8 *data, size_t size, i64 off, i64 lo, i64 hi, i64 maxlen) {
    if (off > lo) {
        if (off - 1 < 0 || (u64)(off - 1) >= size) out_of_file("cstring", off - 1);
        if (data[off - 1] != 0) return NULL;
    }
    /* data.find(b"\0", off, min(hi, off + maxlen)) with slice bounds clipped to the data */
    i64 end = hi < off + maxlen ? hi : off + maxlen;
    if (end > (i64)size) end = (i64)size;
    i64 j = -1;
    for (i64 k = off < 0 ? 0 : off; k < end; k++)
        if (data[k] == 0) {
            j = k;
            break;
        }
    if (j <= off) return NULL;
    const u8 *b = data + off;
    size_t n = (size_t)(j - off), high = 0;
    for (size_t k = 0; k < n; k++) {
        u8 c = b[k];
        if ((c < 0x20 && c != 9 && c != 10 && c != 13) || c == 0x7F) return NULL;
        if (c >= 0x80) high++;
    }
    if (high * 4 > n) return NULL;
    if (utf8_valid(b, n)) {
        char *t = avp_alloc(a, n + 1);
        memcpy(t, b, n);
        t[n] = 0;
        return t;
    }
    return latin1_to_utf8(a, b, n);
}

/* ---- ELF ----------------------------------------------------------------------------------------------------- */

static int section_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = ((const WtSection *)x)->addr, b = ((const WtSection *)y)->addr;
    return a < b ? -1 : a > b;
}

WtElf *wt_elf_new(AvpArena *a, const u8 *data, size_t size) {
    if (size < 4 || memcmp(data, "\x7f" "ELF", 4) != 0 || size < 6 || data[4] != 1 || data[5] != 1)
        shsm_raise(SHSM_EXC_USAGE, "not a 32-bit little-endian ELF");
    WtElf *e = AVP_NEW(a, WtElf);
    e->data = data;
    e->size = size;
    e->machine = u16_at(data, size, 18);
    e->entry = u32_at(data, size, 24);
    i64 phoff = u32_at(data, size, 28), shoff = u32_at(data, size, 32);
    i64 phnum = u16_at(data, size, 44), shnum = u16_at(data, size, 48), shstrndx = u16_at(data, size, 50);
    if (shoff && shnum && shoff + 40 * shnum <= (i64)size) {
        bool has_stro = shstrndx < shnum;
        i64 stro = has_stro ? u32_at(data, size, shoff + 40 * shstrndx + 16) : 0;
        for (i64 k = 0; k < shnum; k++) {
            i64 h = shoff + 40 * k;
            i64 nm_off = u32_at(data, size, h), typ = u32_at(data, size, h + 4), flags = u32_at(data, size, h + 8);
            i64 addr = u32_at(data, size, h + 12), off = u32_at(data, size, h + 16), sz = u32_at(data, size, h + 20);
            if (!(flags & 2) || !addr || !sz) continue;
            const char *name = "";
            if (has_stro) {
                i64 p = stro + nm_off, q = p;
                while (q < (i64)size && data[q]) q++;
                if (q >= (i64)size) shsm_raise(SHSM_EXC_VALUE, "subsection not found");
                name = latin1_to_utf8(a, data + p, (size_t)(q - p));
            }
            WtSection s = {name, addr, off, sz, typ == 8 ? "bss" : ((flags & 4) && !strstr(name, "vu")) ? "text" : "data"};
            avp_vec_push(&e->sections, s);
        }
    }
    if (!e->sections.n) {
        for (i64 k = 0; k < phnum; k++) {
            i64 h = phoff + 32 * k;
            i64 typ = u32_at(data, size, h), off = u32_at(data, size, h + 4), vaddr = u32_at(data, size, h + 8);
            i64 filesz = u32_at(data, size, h + 16), memsz = u32_at(data, size, h + 20), flags = u32_at(data, size, h + 24);
            (void)u32_at(data, size, h + 28);
            if (typ != 1) continue;
            if (filesz) {
                WtSection s = {avp_sprintf(a, "load%lld", (long long)k), vaddr, off, filesz, (flags & 1) ? "text" : "data"};
                avp_vec_push(&e->sections, s);
            }
            if (memsz > filesz) {
                WtSection s = {avp_sprintf(a, "bss%lld", (long long)k), vaddr + filesz, 0, memsz - filesz, "bss"};
                avp_vec_push(&e->sections, s);
            }
        }
    }
    avp_sort(e->sections.v, e->sections.n, sizeof(WtSection), section_cmp, NULL, false);
    return e;
}

const WtSection *wt_elf_section_of(const WtElf *e, i64 addr) {
    /* bisect_right(starts, addr) - 1 */
    size_t lo = 0, hi = e->sections.n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (addr < e->sections.v[mid].addr)
            hi = mid;
        else
            lo = mid + 1;
    }
    if (lo == 0) return NULL;
    const WtSection *s = &e->sections.v[lo - 1];
    return addr < s->addr + s->size ? s : NULL;
}

bool wt_elf_word(const WtElf *e, i64 addr, u32 *out) {
    const WtSection *s = wt_elf_section_of(e, addr);
    if (!s || strcmp(s->kind, "bss") == 0 || addr + 4 > s->addr + s->size) return false;
    *out = u32_at(e->data, e->size, s->offset + addr - s->addr);
    return true;
}

bool wt_elf_float_at(const WtElf *e, i64 addr, u32 *out) {
    u32 w;
    if (!wt_elf_word(e, addr, &w) || (addr & 3)) return false;
    *out = w;
    return true;
}

const char *wt_elf_cstring(AvpArena *a, const WtElf *e, i64 addr) {
    const WtSection *s = wt_elf_section_of(e, addr);
    if (!s || strcmp(s->kind, "data") != 0) return NULL;
    return wt_cstring_at(a, e->data, e->size, s->offset + addr - s->addr, s->offset, s->offset + s->size, 512);
}

/* ---- the executable: ISO 9660, SYSTEM.CNF, find_exe, load_exe -------------------------------------------------- */

/* _EXE_NAME.match(n): ^S[LC][UEPK]S_\d{3}\.\d{2}$ with re.I, on a UTF-8 str. Under re.I, s/S also match U+017F
 * and k/K also U+212A; `$` also matches before a final newline. */
static size_t ci_letter(const char *p, const char *letters) {
    u8 c = (u8)*p;
    if (c >= 'a' && c <= 'z') c = (u8)(c - 32);
    if (c < 0x80) return (c && strchr(letters, c)) ? 1 : 0;
    if (strchr(letters, 'S') && (u8)p[0] == 0xC5 && (u8)p[1] == 0xBF) return 2;
    if (strchr(letters, 'K') && (u8)p[0] == 0xE2 && (u8)p[1] == 0x84 && (u8)p[2] == 0xAA) return 3;
    return 0;
}

static bool exe_name_match(const char *n) {
    static const char *const PAT[] = {"S", "LC", "UEPK", "S"};
    const char *p = n;
    for (int k = 0; k < 4; k++) {
        size_t m = ci_letter(p, PAT[k]);
        if (!m) return false;
        p += m;
    }
    if (*p++ != '_') return false;
    for (int k = 0; k < 3; k++, p++)
        if (*p < '0' || *p > '9') return false;
    if (*p++ != '.') return false;
    for (int k = 0; k < 2; k++, p++)
        if (*p < '0' || *p > '9') return false;
    return *p == 0 || (p[0] == '\n' && p[1] == 0);
}

/* str.upper() of a latin-1 decoded name, as UTF-8. */
static char *latin1_upper(AvpArena *a, const u8 *p, size_t n) {
    char *out = avp_alloc(a, 2 * n + 1), *q = out;
    for (size_t k = 0; k < n; k++) {
        u32 c = p[k];
        if (c >= 'a' && c <= 'z') {
            *q++ = (char)(c - 32);
            continue;
        }
        if (c < 0x80) {
            *q++ = (char)c;
            continue;
        }
        if (c == 0xDF) { /* sharp s -> "SS" */
            *q++ = 'S';
            *q++ = 'S';
            continue;
        }
        if (c == 0xB5) c = 0x39C;                       /* micro sign -> GREEK CAPITAL MU */
        else if (c == 0xFF) c = 0x178;                  /* y diaeresis -> LATIN CAPITAL Y WITH DIAERESIS */
        else if (c >= 0xE0 && c <= 0xFE && c != 0xF7) c -= 0x20;
        *q++ = (char)(0xC0 | (c >> 6));
        *q++ = (char)(0x80 | (c & 0x3F));
    }
    *q = 0;
    return out;
}

typedef struct IsoFile {
    const char *name;
    i64 extent, size;
} IsoFile;
typedef AVP_VEC(IsoFile) IsoFiles;

static void read_at(AvpFile *fh, const char *path, i64 off, i64 n, AvpBytes *out) {
    (void)path;
    out->n = 0;
    if (!avp_fseek(fh, off, 0)) shsm_raise_io();
    if (n <= 0) return;
    avp_vec_reserve(out, (size_t)n);
    i64 got = avp_fread(fh, out->v, (size_t)n);
    if (got < 0) shsm_raise_io();
    out->n = (size_t)got; /* a short read gives fewer bytes, as fh.read */
}

static const IsoFile *iso_get(const IsoFiles *f, const char *name) {
    for (size_t k = 0; k < f->n; k++)
        if (strcmp(f->v[k].name, name) == 0) return &f->v[k];
    return NULL;
}

/* iso_files(fh): {NAME: (extent, size)} (a later duplicate replaces the earlier value in place). */
static void iso_files(AvpArena *a, AvpFile *fh, const char *path, IsoFiles *out) {
    AvpBytes pvd = {0}, d = {0};
    read_at(fh, path, 16 * SECTOR, SECTOR, &pvd);
    if (pvd.n < 6 || memcmp(pvd.v + 1, "CD001", 5) != 0)
        shsm_raise(SHSM_EXC_USAGE, "not an ISO 9660 image (no primary volume descriptor)");
    const u8 *root = pvd.v + 156;
    if (pvd.n < 190) out_of_file("unpack_from", 156);
    read_at(fh, path, (i64)u32_at(root, 34, 2) * SECTOR, u32_at(root, 34, 10), &d);
    size_t i = 0;
    while (i < d.n) {
        size_t ln = d.v[i];
        if (ln == 0) {
            i = (i / SECTOR + 1) * SECTOR;
            continue;
        }
        if (i + 32 >= d.n) out_of_file("directory record", (i64)i + 32);
        size_t nl = d.v[i + 32], s0 = i + 33, s1 = s0 + nl;
        if (s0 > d.n) s0 = d.n;
        if (s1 > d.n) s1 = d.n;
        const u8 *semi = memchr(d.v + s0, ';', s1 - s0);
        size_t len = semi ? (size_t)(semi - (d.v + s0)) : s1 - s0;
        if (i + 25 >= d.n) out_of_file("directory record", (i64)i + 25);
        if (!(d.v[i + 25] & 2) && len) {
            IsoFile f = {latin1_upper(a, d.v + s0, len), u32_at(d.v, d.n, (i64)i + 2), u32_at(d.v, d.n, (i64)i + 10)};
            bool found = false;
            for (size_t k = 0; k < out->n; k++)
                if (strcmp(out->v[k].name, f.name) == 0) {
                    out->v[k] = f;
                    found = true;
                }
            if (!found) avp_vec_push(out, f);
        }
        i += ln;
    }
    avp_vec_free(&pvd);
    avp_vec_free(&d);
}

/* Python str \s for a latin-1 character. */
static bool py_space(u8 c) {
    return c == ' ' || (c >= 9 && c <= 13) || (c >= 0x1C && c <= 0x1F) || c == 0x85 || c == 0xA0;
}

/* re.search(r"BOOT2\s*=\s*cdrom0?:\\?([^;\s]+)", text): the group as bytes [*g0, *g1), false when no match. */
static bool boot2_search(const u8 *t, size_t n, size_t *g0, size_t *g1) {
    for (size_t s = 0; s + 5 <= n; s++) {
        if (memcmp(t + s, "BOOT2", 5) != 0) continue;
        size_t p = s + 5;
        while (p < n && py_space(t[p])) p++;
        if (p >= n || t[p] != '=') continue;
        p++;
        while (p < n && py_space(t[p])) p++;
        if (p + 5 > n || memcmp(t + p, "cdrom", 5) != 0) continue;
        p += 5;
        if (p < n && t[p] == '0' && p + 1 < n && t[p + 1] == ':')
            p += 2;
        else if (p < n && t[p] == ':')
            p += 1;
        else
            continue;
        /* \\? then [^;\s]+ (greedy; backtracks over the backslash when nothing follows it) */
        size_t q = p;
        if (q < n && t[q] == '\\' && q + 1 < n && t[q + 1] != ';' && !py_space(t[q + 1])) q++;
        size_t r = q;
        while (r < n && t[r] != ';' && !py_space(t[r])) r++;
        if (r == q) continue;
        *g0 = q;
        *g1 = r;
        return true;
    }
    return false;
}

static int str_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp(*(const char *const *)x, *(const char *const *)y); /* UTF-8 byte order = code point order */
}

/* read_iso_exe(path): (member name, bytes). */
static const char *read_iso_exe(AvpArena *a, const char *path, AvpBytes *data) {
    AvpFile *fh = avp_fopen(path, "rb", AVP_TM_UTF8);
    if (!fh) shsm_raise_io();
    IsoFiles files = {0};
    iso_files(a, fh, path, &files);
    const char *name = NULL;
    const IsoFile *cnf = iso_get(&files, "SYSTEM.CNF");
    if (cnf) {
        AvpBytes t = {0};
        read_at(fh, path, cnf->extent * SECTOR, cnf->size, &t);
        size_t g0, g1;
        if (boot2_search(t.v, t.n, &g0, &g1)) name = latin1_upper(a, t.v + g0, g1 - g0);
        avp_vec_free(&t);
    }
    if (!name || !iso_get(&files, name)) {
        const char **names = avp_alloc(a, (files.n + 1) * sizeof *names);
        for (size_t k = 0; k < files.n; k++) names[k] = files.v[k].name;
        avp_sort(names, files.n, sizeof *names, str_cmp, NULL, false);
        name = NULL;
        for (size_t k = 0; k < files.n && !name; k++)
            if (exe_name_match(names[k])) name = names[k];
    }
    if (!name) {
        avp_fclose(fh);
        shsm_raise(SHSM_EXC_USAGE, "%s: no boot executable in the root directory", path);
    }
    const IsoFile *f = iso_get(&files, name);
    read_at(fh, path, f->extent * SECTOR, f->size, data);
    avp_fclose(fh);
    avp_vec_free(&files);
    return name;
}

WtExe wt_find_exe(const ShsmRepo *r, const char *exe) {
    AvpArena *a = avp_arena();
    if (exe && *exe) {
        if (!avp_isfile(exe)) shsm_raise(SHSM_EXC_USAGE, "--exe %s: no such file", exe);
        AvpFile *fh = avp_fopen(exe, "rb", AVP_TM_UTF8);
        if (!fh) shsm_raise_io();
        u8 head[4];
        i64 got = avp_fread(fh, head, 4);
        avp_fclose(fh);
        bool elf = got == 4 && memcmp(head, "\x7f" "ELF", 4) == 0;
        return (WtExe){avp_strdup(a, exe), elf ? NULL : "boot"};
    }
    const char *d = shsm_repo_shared_path(a, r, "orig/_iso");
    if (avp_isdir(d)) {
        AvpStrVec names = {0};
        if (!avp_listdir(a, d, &names)) shsm_raise_io();
        avp_sort_strs(names.v, names.n);
        for (size_t k = 0; k < names.n; k++) {
            const char *n = avp_cz(a, names.v[k]);
            const char *p = avp_path_join(a, d, n);
            if (exe_name_match(n) && avp_isfile(p)) return (WtExe){p, NULL};
        }
        for (size_t k = 0; k < names.n; k++) {
            AvpStr n = names.v[k];
            if (n.n >= 4 && _strnicmp(n.p + n.n - 4, ".iso", 4) == 0)
                return (WtExe){avp_path_join(a, d, avp_cz(a, n)), "boot"};
        }
    }
    shsm_raise(SHSM_EXC_USAGE, "no PS2 executable: orig/_iso/ holds neither a SLUS_* ELF nor an ISO image (--exe PATH)");
}

const char *wt_load_exe(AvpArena *a, WtExe exe, AvpBytes *data) {
    AvpStr base = avp_path_basename(exe.path);
    if (exe.member) {
        const char *name = read_iso_exe(a, exe.path, data);
        return avp_sprintf(a, "%.*s:%s", (int)base.n, base.p, name);
    }
    if (!avp_read_all(NULL, exe.path, data)) shsm_raise_io();
    return avp_cz(a, base);
}
