/* A port of tools/scripts/iso9660_extract.py (pure stdlib Python): ISO9660 --list, --extract and --boot-elf. */
#include "iso9660_extract.h"

#include "shsm_re.h"

#define SECTOR 2048

/* print(__doc__) of the Python module (the docstring ends in a newline, print adds another). */
static const char USAGE[] =
    "Minimal ISO9660 reader (pure stdlib).\n"
    "\n"
    "Usage:\n"
    "  build/tools/iso9660_extract.exe ISO --list [PATH]\n"
    "  build/tools/iso9660_extract.exe ISO --extract ISO_PATH OUT_FILE [--extract ...]\n"
    "  build/tools/iso9660_extract.exe ISO --boot-elf OUT_DIR   (extracts SYSTEM.CNF and BOOT2 ELF)\n"
    "\n";

typedef struct {
    u32 lba, size;
    bool dir;
    AvpStr name; /* UTF-8, for comparisons */
    AvpStr raw;  /* the latin-1 bytes Python's print sends to a cp1252 stdout */
} Rec;

typedef struct {
    AvpArena *a;
    AvpFile *f;
    Rec root;
} Iso;

AvpStr iso9660_latin1_utf8(AvpArena *a, const u8 *p, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) avp_buf_putcp(&b, p[i]);
    return avp_buf_str(&b);
}

/* the identifier without its version, still latin-1 */
static AvpStr clean_raw(AvpArena *a, const u8 *name, size_t n) {
    if (n == 1 && name[0] == 0) return avp_str(".");
    if (n == 1 && name[0] == 1) return avp_str("..");
    /* re.sub(r';\d+$', '', name): `$` also matches before a final newline */
    size_t end = n;
    if (end > 0 && name[end - 1] == '\n') end--;
    size_t k = end;
    while (k > 0 && name[k - 1] >= '0' && name[k - 1] <= '9') k--;
    if (k < end && k > 0 && name[k - 1] == ';')
        return avp_str_concat(a, avp_str_dup(a, avp_strn((const char *)name, k - 1)),
                              avp_str_dup(a, avp_strn((const char *)name + end, n - end)));
    return avp_str_dup(a, avp_strn((const char *)name, n));
}

AvpStr iso9660_clean_name(AvpArena *a, const u8 *name, size_t n) {
    AvpStr raw = clean_raw(a, name, n);
    return iso9660_latin1_utf8(a, (const u8 *)raw.p, raw.n);
}

static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

/* Iso.read(lba, count): fewer bytes at the end of the file. */
static size_t iso_read(Iso *iso, u64 lba, size_t count, u8 *out) {
    if (!avp_fseek(iso->f, (i64)(lba * SECTOR), 0)) return 0;
    i64 n = avp_fread(iso->f, out, count * SECTOR);
    return n < 0 ? 0 : (size_t)n;
}

static Rec parse_record(AvpArena *a, const u8 *b, size_t len) {
    /* a short record makes struct.unpack_from or b[25] raise in the Python: an uncaught exception */
    if (len < 33) avp_fatal("iso9660_extract.exe: error: IndexError: index out of range");
    Rec r;
    r.lba = le32(b + 2);
    r.size = le32(b + 10);
    r.dir = (b[25] & 2) != 0;
    size_t nlen = b[32];
    size_t avail = len - 33;
    r.raw = clean_raw(a, b + 33, nlen < avail ? nlen : avail);
    r.name = iso9660_latin1_utf8(a, (const u8 *)r.raw.p, r.raw.n);
    return r;
}

static Iso *iso_open(AvpArena *a, const char *path) {
    Iso *iso = avp_xcalloc(1, sizeof *iso);
    iso->a = a;
    iso->f = avp_fopen(path, "rb", AVP_TM_UTF8);
    if (!iso->f) avp_fatal("iso9660_extract.exe: error: FileNotFoundError: %s", path);
    u8 pvd[SECTOR] = {0};
    iso_read(iso, 16, 1, pvd);
    if (memcmp(pvd + 1, "CD001", 5) != 0) avp_fatal("not an ISO9660 image (no CD001 at sector 16)");
    iso->root = parse_record(a, pvd + 156, 34);
    return iso;
}

typedef struct {
    Rec *v;
    size_t n;
} Recs;

static Recs listdir(Iso *iso, Rec rec) {
    AvpArena *a = iso->a;
    size_t sectors = ((size_t)rec.size + SECTOR - 1) / SECTOR;
    u8 *buf = avp_xcalloc(sectors + 1, SECTOR);
    size_t got = iso_read(iso, rec.lba, sectors, buf);
    size_t len = got < rec.size ? got : rec.size; /* read(...)[:size] */
    Recs out = {avp_xcalloc(len / 33 + 1, sizeof(Rec)), 0};
    size_t pos = 0;
    while (pos < len) {
        size_t n = buf[pos];
        if (n == 0) {
            pos = (pos / SECTOR + 1) * SECTOR;
            continue;
        }
        size_t avail = len - pos;
        Rec r = parse_record(a, buf + pos, n < avail ? n : avail);
        pos += n;
        if (!avp_str_eqz(r.name, ".") && !avp_str_eqz(r.name, "..")) out.v[out.n++] = r;
    }
    avp_xfree(buf);
    return out;
}

static Rec find(Iso *iso, const char *path) {
    AvpArena *a = iso->a;
    Rec rec = iso->root;
    AvpStr norm = avp_str_replace(a, avp_str(path), avp_str("\\"), avp_str("/"), -1);
    AvpStrVec parts = avp_str_split(norm, avp_str("/"), -1);
    for (size_t i = 0; i < parts.n; i++) {
        if (parts.v[i].n == 0) continue;
        AvpStr want = avp_str_upper(a, parts.v[i]);
        Recs rs = listdir(iso, rec);
        bool found = false;
        for (size_t j = 0; j < rs.n; j++) {
            if (avp_str_eq(avp_str_upper(a, rs.v[j].name), want)) {
                rec = rs.v[j];
                found = true;
                break;
            }
        }
        avp_xfree(rs.v);
        if (!found) avp_fatal("iso9660_extract.exe: error: FileNotFoundError: %s", path);
    }
    return rec;
}

static void extract(Iso *iso, Rec rec, const char *out_path) {
    AvpFile *o = avp_fopen(out_path, "wb", AVP_TM_UTF8);
    if (!o) avp_fatal("iso9660_extract.exe: error: OSError: cannot write %s", out_path);
    u64 remaining = rec.size;
    avp_fseek(iso->f, (i64)((u64)rec.lba * SECTOR), 0);
    enum { CHUNK = 1 << 20 };
    u8 *buf = avp_xmalloc(CHUNK);
    while (remaining) {
        i64 n = avp_fread(iso->f, buf, remaining < CHUNK ? (size_t)remaining : CHUNK);
        if (n <= 0) break;
        avp_fwrite(o, buf, (size_t)n);
        remaining -= (u64)n;
    }
    avp_xfree(buf);
    avp_fclose(o);
}

int iso9660_main(int argc, char **argv) {
    if (argc < 3) {
        fputs(USAGE, stdout);
        return 1;
    }
    AvpArena *a = avp_arena();
    Iso *iso = iso_open(a, argv[1]);
    int i = 2;
    while (i < argc) {
        const char *arg = argv[i];
        if (strcmp(arg, "--list") == 0) {
            const char *path = "/";
            if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) path = argv[i + 1];
            if (strcmp(path, "/") != 0) i++;
            Rec rec = strcmp(path, "/") != 0 ? find(iso, path) : iso->root;
            Recs rs = listdir(iso, rec);
            for (size_t k = 0; k < rs.n; k++)
                printf("%12u  %s%s  (lba %u)\n", rs.v[k].size, avp_cz(a, rs.v[k].raw), rs.v[k].dir ? "/" : "", rs.v[k].lba);
            avp_xfree(rs.v);
        } else if (strcmp(arg, "--extract") == 0) {
            if (i + 2 >= argc) avp_fatal("iso9660_extract.exe: error: IndexError: list index out of range");
            Rec rec = find(iso, argv[i + 1]);
            extract(iso, rec, argv[i + 2]);
            printf("extracted %s (%u bytes) -> %s\n", argv[i + 1], rec.size, argv[i + 2]);
            i += 2;
        } else if (strcmp(arg, "--boot-elf") == 0) {
            if (i + 1 >= argc) avp_fatal("iso9660_extract.exe: error: IndexError: list index out of range");
            const char *out = argv[i + 1];
            if (!avp_makedirs(out, true)) avp_fatal("iso9660_extract.exe: error: OSError: cannot create %s", out);
            Rec cnf = find(iso, "SYSTEM.CNF");
            const char *cnf_path = avp_join(a, out, "SYSTEM.CNF");
            extract(iso, cnf, cnf_path);
            AvpStr text;
            if (!avp_read_text(a, cnf_path, AVP_TEXT(AVP_ENC_MBCS, AVP_ERR_REPLACE, AVP_NL_DEFAULT), &text))
                avp_fatal("iso9660_extract.exe: error: OSError: %s", cnf_path);
            ShsmReMatch m;
            if (!shsm_re_search(shsm_re("BOOT2\\s*=\\s*cdrom0:[\\\\/]?([^;\\s]+)", SHSM_RE_I), text, 0, AVP_END, &m))
                avp_fatal("no BOOT2 line in SYSTEM.CNF");
            AvpStr name = avp_str_replace(a, shsm_re_group(&m, 1), avp_str("\\"), avp_str("/"), -1);
            const char *namez = avp_cz(a, name);
            Rec rec = find(iso, namez);
            const char *dst = avp_join(a, out, avp_cz(a, avp_path_basename(namez)));
            extract(iso, rec, dst);
            printf("SYSTEM.CNF:\n%s\n", avp_cz(a, text));
            printf("extracted %s (%u bytes) -> %s\n", namez, rec.size, dst);
            i += 1;
        } else {
            avp_fatal("unknown arg %s", arg);
        }
        i++;
    }
    return 0;
}
