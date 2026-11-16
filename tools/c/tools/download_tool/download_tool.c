#include "download_tool.h"

#include <stdlib.h>
#include <string.h>

#include "shsm_zlib.h"

/* ---- URLs ---------------------------------------------------------------------------------------------------- */

const char *dt_machine(AvpArena *a) {
    const char *m = getenv("PROCESSOR_ARCHITEW6432");
    if (!m || !*m) m = getenv("PROCESSOR_ARCHITECTURE");
    if (!m || !*m) m = "amd64";
    return avp_cz(a, avp_str_lower(a, avp_str(m)));
}

/* arch as the Python normalises it: "amd64" is "x86_64". */
static const char *norm_arch(const char *machine) { return strcmp(machine, "amd64") == 0 ? "x86_64" : machine; }

char *dt_tool_url(AvpArena *a, const char *tool, const char *tag, const char *system, const char *machine) {
    bool darwin = strcmp(system, "darwin") == 0;
    bool windows = strcmp(system, "windows") == 0;
    const char *sys = darwin ? "macos" : system;
    const char *suffix = windows ? ".exe" : "";
    if (strcmp(tool, "binutils") == 0) {
        const char *arch = darwin ? "universal" : norm_arch(machine);
        return avp_sprintf(a, "https://github.com/encounter/gc-wii-binutils/releases/download/%s/%s-%s.zip", tag, sys,
                           arch);
    }
    if (strcmp(tool, "compilers") == 0) return avp_sprintf(a, "https://files.decomp.dev/compilers_%s.zip", tag);
    if (strcmp(tool, "dtk") == 0)
        return avp_sprintf(a, "https://github.com/encounter/decomp-toolkit/releases/download/%s/dtk-%s-%s%s", tag, sys,
                           norm_arch(machine), suffix);
    if (strcmp(tool, "objdiff-cli") == 0)
        return avp_sprintf(a, "https://github.com/encounter/objdiff/releases/download/%s/objdiff-cli-%s-%s%s", tag, sys,
                           norm_arch(machine), suffix);
    if (strcmp(tool, "sjiswrap") == 0)
        return avp_sprintf(a, "https://github.com/encounter/sjiswrap/releases/download/%s/sjiswrap-windows-x86.exe", tag);
    if (strcmp(tool, "wibo") == 0)
        return avp_sprintf(a, "https://github.com/decompals/wibo/releases/download/%s/wibo-%s", tag,
                           darwin ? "macos" : machine);
    return NULL;
}

/* ---- zip ----------------------------------------------------------------------------------------------------- */

static void err_set(DtErr *e, const char *cls, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    e->cls = cls;
    vsnprintf(e->text, sizeof e->text, fmt, ap);
    va_end(ap);
}

static void err_io(DtErr *e) {
    err_set(e, avp_io_err_class(), "%s", avp_cz(avp_scratch(), avp_io_err_str(avp_scratch())));
}

static u32 rd16(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8; }
static u32 rd32(const u8 *p) { return rd16(p) | rd16(p + 2) << 16; }

static const wchar_t CP437[129] =
    L"ÇüéâäàåçêëèïîìÄÅ"
    L"ÉæÆôöòûùÿÖÜ¢£¥₧ƒ"
    L"áíóúñÑªº¿⌐¬½¼¡«»"
    L"░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
    L"└┴┬├─┼╞╟╚╔╩╦╠═╬╧"
    L"╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀"
    L"αßΓπΣσµτΦΘΩδ∞φε∩"
    L"≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";

/* ZipInfo.filename: UTF-8 when flag bit 11 is set, else cp437. */
static AvpStr member_name(AvpArena *a, const u8 *raw, size_t n, u32 flags) {
    if (flags & 0x800) return avp_str_dup(a, avp_strn((const char *)raw, n));
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        if (raw[i] < 0x80)
            avp_buf_putc(&b, (char)raw[i]);
        else
            avp_buf_putcp(&b, (u32)CP437[raw[i] - 0x80]);
    }
    return avp_buf_str(&b);
}

char *dt_zip_arcname(AvpArena *a, AvpStr filename) {
    char *name = avp_cz(a, avp_str_replace(a, filename, AVP_S("/"), AVP_S("\\"), -1));
    AvpStr drive, rest;
    avp_path_splitdrive(name, &drive, &rest);
    AvpStrVec parts = avp_str_split(rest, AVP_S("\\"), -1);
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
    for (size_t i = 0; i < parts.n; i++) {
        AvpStr x = parts.v[i];
        if (x.n == 0 || avp_str_eqz(x, ".") || avp_str_eqz(x, "..")) continue;
        if (!first) avp_buf_putc(&b, '\\');
        first = false;
        avp_buf_put(&b, x);
    }
    /* _sanitize_windows_name: illegal characters to "_", trailing dots and spaces off each part, empty parts dropped */
    AvpStr joined = avp_buf_str(&b);
    AvpBuf t;
    avp_buf_init(&t, a);
    for (size_t i = 0; i < joined.n; i++) {
        char c = joined.p[i];
        avp_buf_putc(&t, strchr(":<>|\"?*", c) && c ? '_' : c);
    }
    AvpStrVec segs = avp_str_split(avp_buf_str(&t), AVP_S("\\"), -1);
    AvpBuf out;
    avp_buf_init(&out, a);
    first = true;
    for (size_t i = 0; i < segs.n; i++) {
        AvpStr x = segs.v[i];
        while (x.n > 0 && (x.p[x.n - 1] == '.' || x.p[x.n - 1] == ' ')) x.n--;
        if (x.n == 0) continue;
        if (!first) avp_buf_putc(&out, '\\');
        first = false;
        avp_buf_put(&out, x);
    }
    return avp_buf_cz(&out);
}

typedef struct {
    AvpStr name; /* ZipInfo.filename */
    const u8 *raw_name;
    size_t raw_name_n;
    u32 flags, method, crc, csize, usize, header_off;
} ZipEntry;

static bool find_ecd(const u8 *p, size_t n, size_t *pos) {
    if (n >= 22 && rd32(p + n - 22) == 0x06054b50 && rd16(p + n - 2) == 0) {
        *pos = n - 22;
        return true;
    }
    size_t start = n > 65535 + 22 ? n - 65535 - 22 : 0;
    for (size_t i = n >= 4 ? n - 4 + 1 : 0; i-- > start;) {
        if (rd32(p + i) == 0x06054b50) {
            if (i + 22 > n) return false;
            *pos = i;
            return true;
        }
    }
    return false;
}

static bool extract_entry(AvpArena *a, const u8 *p, size_t n, const ZipEntry *e, const char *dest, DtErr *err) {
    char *arc = dt_zip_arcname(a, e->name);
    bool is_dir = e->name.n > 0 && e->name.p[e->name.n - 1] == '/';
    if (!arc[0] && !is_dir) {
        err_set(err, "ValueError", "Empty filename.");
        return false;
    }
    char *target = avp_path_normpath(a, avp_path_join(a, dest, arc));
    AvpStr upper = avp_path_dirname(target);
    if (upper.n > 0) {
        char *u = avp_cz(a, upper);
        if (!avp_exists(u) && !avp_makedirs(u, true)) {
            err_io(err);
            return false;
        }
    }
    if (is_dir) {
        if (!avp_isdir(target) && !avp_makedirs(target, true)) {
            err_io(err);
            return false;
        }
        return true;
    }
    /* ZipFile.open: the local header must agree with the central directory */
    size_t lo = e->header_off;
    if (lo + 30 > n || rd32(p + lo) != 0x04034b50) {
        err_set(err, "zipfile.BadZipFile", "Bad magic number for file header");
        return false;
    }
    size_t fn = rd16(p + lo + 26), ex = rd16(p + lo + 28);
    if (lo + 30 + fn > n || fn != e->raw_name_n || memcmp(p + lo + 30, e->raw_name, fn) != 0) {
        err_set(err, "zipfile.BadZipFile", "File name in directory %s and header differ.",
                avp_cz(a, avp_repr_str(a, e->name)));
        return false;
    }
    if (e->flags & 1) {
        err_set(err, "RuntimeError", "File %s is encrypted, password required for extraction",
                avp_cz(a, avp_repr_str(a, e->name)));
        return false;
    }
    if (e->method != 0 && e->method != 8) {
        err_set(err, "NotImplementedError", "That compression method is not supported");
        return false;
    }
    size_t data_off = lo + 30 + fn + ex;
    if (data_off > n || (size_t)e->csize > n - data_off) {
        err_set(err, "EOFError", "Compressed file ended before the end-of-stream marker was reached");
        return false;
    }
    AvpFile *f = avp_fopen(target, "wb", AVP_TM_UTF8);
    if (!f) {
        err_io(err);
        return false;
    }
    const u8 *out = p + data_off;
    size_t out_n = e->csize;
    if (e->method == 8) {
        ShsmZErr ze;
        size_t got = 0;
        out = shsm_zlib_decompress(a, p + data_off, e->csize, -15, e->usize, &got, &ze);
        if (!out) {
            avp_fclose(f);
            err_set(err, "zlib.error", "%s", ze.msg);
            return false;
        }
        out_n = got;
    }
    bool good = shsm_zlib_crc32(out, out_n, 0) == e->crc;
    if (good) {
        if (out_n > 0 && !avp_fwrite(f, out, out_n)) {
            err_io(err);
            avp_fclose(f);
            return false;
        }
    } else {
        err_set(err, "zipfile.BadZipFile", "Bad CRC-32 for file %s", avp_cz(a, avp_repr_str(a, e->name)));
    }
    if (!avp_fclose(f) && good) {
        err_io(err);
        return false;
    }
    return good;
}

bool dt_zip_extract_all(AvpArena *a, const u8 *p, size_t n, const char *dest, DtErr *err) {
    size_t ecd;
    if (!find_ecd(p, n, &ecd)) {
        err_set(err, "zipfile.BadZipFile", "File is not a zip file");
        return false;
    }
    u32 total = rd16(p + ecd + 10), size_cd = rd32(p + ecd + 12), off_cd = rd32(p + ecd + 16);
    if (rd16(p + ecd + 4) == 0xFFFF || rd16(p + ecd + 8) == 0xFFFF || total == 0xFFFF || size_cd == 0xFFFFFFFFu ||
        off_cd == 0xFFFFFFFFu) {
        err_set(err, "NotImplementedError", "zip64 archives are not supported");
        return false;
    }
    i64 concat = (i64)ecd - (i64)size_cd - (i64)off_cd;
    i64 start = (i64)off_cd + concat;
    if (start < 0) {
        err_set(err, "zipfile.BadZipFile", "Bad offset for central directory");
        return false;
    }
    size_t pos = (size_t)start, end = pos + size_cd;
    if (end > n) end = n;
    AVP_VEC(ZipEntry) entries = {0};
    while (pos + 46 <= end) {
        const u8 *c = p + pos;
        if (rd32(c) != 0x02014b50) {
            err_set(err, "zipfile.BadZipFile", "Bad magic number for central directory");
            return false;
        }
        size_t fn = rd16(c + 28), ex = rd16(c + 30), cm = rd16(c + 32);
        if (pos + 46 + fn > n) {
            err_set(err, "zipfile.BadZipFile", "Truncated central directory");
            return false;
        }
        ZipEntry e;
        e.flags = rd16(c + 8);
        e.method = rd16(c + 10);
        e.crc = rd32(c + 16);
        e.csize = rd32(c + 20);
        e.usize = rd32(c + 24);
        e.header_off = (u32)((i64)rd32(c + 42) + concat);
        e.raw_name = c + 46;
        e.raw_name_n = fn;
        e.name = member_name(a, c + 46, fn, e.flags);
        if (e.csize == 0xFFFFFFFFu || e.usize == 0xFFFFFFFFu || rd32(c + 42) == 0xFFFFFFFFu) {
            err_set(err, "NotImplementedError", "zip64 archives are not supported");
            return false;
        }
        avp_vec_push(&entries, e);
        pos += 46 + fn + ex + cm;
    }
    for (size_t i = 0; i < entries.n; i++)
        if (!extract_entry(a, p, n, &entries.v[i], dest, err)) return false;
    return true;
}
