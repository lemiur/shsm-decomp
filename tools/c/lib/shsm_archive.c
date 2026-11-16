/* The tar reader for `git archive` output and the extraction of its members. See shsm_archive.h; the member names, kinds and
 * sizes are those tarfile.open(fileobj=BytesIO(data)) yields (Lib/tarfile.py: TarInfo.frombuf, _proc_pax,
 * _proc_gnulong). Taken from another project of the author's tools/c/core/src/zip_tar.c at commit 49e811503d7ba5c9cff92160557df057c50a8ab3 (names avp_ -> shsm_);
 * shsm_tar_extract_files is this project's. */
#include "shsm_archive.h"

#include <stdio.h>
#include <wchar.h>
#include <windows.h>

#define BLOCK 512u

/* Python's nti(): an octal string, or a base-256 number when the first byte has its high bit set (0o200 positive,
 * 0o377 negative). Returns false for a field tarfile would call invalid. */
static bool tar_number(const u8 *f, size_t n, i64 *out) {
    if (f[0] == 0x80 || f[0] == 0xFF) {
        u64 v = 0;
        for (size_t i = 1; i < n; i++) {
            if (v >> 56) return false; /* beyond 64 bits */
            v = (v << 8) | f[i];
        }
        if (f[0] == 0x80) {
            if (v > (u64)INT64_MAX) return false;
            *out = (i64)v;
        } else {
            if (n - 1 > 7) return false;
            *out = (i64)v - ((i64)1 << (8 * (n - 1)));
        }
        return true;
    }
    /* nts + strip: the field up to the first NUL, surrounding whitespace ignored; empty is 0 */
    size_t len = 0;
    while (len < n && f[len]) len++;
    size_t b = 0, e = len;
    while (b < e && (f[b] == ' ' || (f[b] >= 9 && f[b] <= 13))) b++;
    while (e > b && (f[e - 1] == ' ' || (f[e - 1] >= 9 && f[e - 1] <= 13))) e--;
    if (b == e) {
        *out = 0;
        return true;
    }
    u64 v = 0;
    for (size_t i = b; i < e; i++) {
        if (f[i] < '0' || f[i] > '7') return false;
        v = (v << 3) | (u64)(f[i] - '0');
        if (v >> 62) return false;
    }
    *out = (i64)v;
    return true;
}

/* A NUL-terminated field (nts): the bytes up to the first NUL, at most n. */
static size_t field_len(const u8 *f, size_t n) {
    size_t len = 0;
    while (len < n && f[len]) len++;
    return len;
}

static bool checksum_ok(const u8 *h, i64 stored) {
    i64 usum = 0, ssum = 0;
    for (size_t i = 0; i < BLOCK; i++) {
        u8 c = (i >= 148 && i < 156) ? ' ' : h[i];
        usum += c;
        ssum += (signed char)c;
    }
    return stored == usum || stored == ssum;
}

void shsm_tar_open(ShsmTarReader *r, const void *p, size_t n) {
    memset(r, 0, sizeof *r);
    r->p = (const u8 *)p;
    r->n = n;
}

/* ---- pax records: "<len> <key>=<value>\n" ---------------------------------------------------------------------------- */

typedef struct {
    const u8 *path;
    size_t path_n;
    const u8 *linkpath;
    size_t linkpath_n;
    bool has_size;
    i64 size;
    bool has_mtime;
    i64 mtime;
} PaxInfo;

static i64 pax_int(const u8 *v, size_t n) {
    /* int(value) for size; float(value) truncated for mtime ("1700000000.25" -> 1700000000) */
    i64 x = 0;
    bool neg = false;
    size_t i = 0;
    if (i < n && v[i] == '-') {
        neg = true;
        i++;
    }
    for (; i < n && v[i] >= '0' && v[i] <= '9'; i++) x = x * 10 + (v[i] - '0');
    return neg ? -x : x;
}

/* Parses the records of a pax header body into info (later records override earlier ones). False on a malformed
 * record (tarfile: InvalidHeaderError). Values point into the body. */
static bool pax_parse(const u8 *buf, size_t n, PaxInfo *info) {
    size_t pos = 0;
    while (pos < n) {
        size_t i = pos;
        u64 len = 0;
        while (i < n && buf[i] >= '0' && buf[i] <= '9') len = len * 10 + (buf[i++] - '0');
        if (i == pos || i >= n || buf[i] != ' ') break; /* the regex stops matching: the rest is ignored */
        size_t kstart = ++i;
        while (i < n && buf[i] != '=') i++;
        if (i >= n || i == kstart) break;
        size_t kend = i;
        if (len == 0) return false;
        size_t vstart = i + 1;
        if (len > n - pos) len = n - pos; /* tarfile slices: a too-long length just takes what is there */
        size_t rec_end = pos + (size_t)len;
        size_t vend = rec_end > 0 ? rec_end - 1 : 0; /* drop the trailing newline */
        if (vend < vstart) vend = vstart;
        const u8 *key = buf + kstart, *val = buf + vstart;
        size_t kn = kend - kstart, vn = vend - vstart;
        if (kn == 4 && memcmp(key, "path", 4) == 0) {
            info->path = val;
            info->path_n = vn;
        } else if (kn == 8 && memcmp(key, "linkpath", 8) == 0) {
            info->linkpath = val;
            info->linkpath_n = vn;
        } else if (kn == 4 && memcmp(key, "size", 4) == 0) {
            info->has_size = true;
            info->size = pax_int(val, vn);
        } else if (kn == 5 && memcmp(key, "mtime", 5) == 0) {
            info->has_mtime = true;
            info->mtime = pax_int(val, vn);
        }
        pos += (size_t)len;
    }
    return true;
}

static const char *copy_cz(AvpArena *a, const u8 *p, size_t n) {
    char *s = (char *)avp_alloc_noz(a, n + 1);
    memcpy(s, p, n);
    s[n] = 0;
    return s;
}

static ShsmTarKind kind_of(char type) {
    switch (type) {
    case '0': case 0: case '7': return SHSM_TAR_FILE;
    case '5': return SHSM_TAR_DIR;
    case '2': return SHSM_TAR_SYMLINK;
    case '1': return SHSM_TAR_HARDLINK;
    default: return SHSM_TAR_OTHER;
    }
}

/* A bad header: tarfile raises ReadError when it is the archive's first header or follows an extended header (an
 * x, g, L or K record), and otherwise treats it as the end of the archive without an error. */
static int header_fail(ShsmTarReader *r, const char *msg, bool hard) {
    r->err = msg;
    r->done = true;
    return hard ? -1 : 0;
}

int shsm_tar_next(ShsmTarReader *r, AvpArena *a, ShsmTarMember *m) {
    if (r->done) return 0;
    bool hard = r->pos == 0, ext = false;
    if (r->n == 0) return header_fail(r, "empty file", true);
    PaxInfo pax_global;
    /* The global pax header (type g) only carries information for the member after it; 'x' headers start from a copy
     * of the globals, as tarfile does. */
    memset(&pax_global, 0, sizeof pax_global);
    PaxInfo pax;
    memset(&pax, 0, sizeof pax);
    const u8 *longname = NULL, *longlink = NULL;
    size_t longname_n = 0, longlink_n = 0;
    for (;;) {
        if (r->pos >= r->n) {
            if (ext) return header_fail(r, "missing header after an extended header", true);
            r->done = true;
            return 0;
        }
        if (r->n - r->pos < BLOCK) return header_fail(r, "truncated header", hard);
        const u8 *h = r->p + r->pos;
        /* an all-zero block ends the archive */
        size_t z = 0;
        while (z < BLOCK && h[z] == 0) z++;
        if (z == BLOCK) {
            r->done = true;
            return 0;
        }
        i64 chk, size, mode = 0, mtime = 0;
        if (!tar_number(h + 148, 8, &chk) || !checksum_ok(h, chk)) return header_fail(r, "bad checksum", hard);
        if (!tar_number(h + 100, 8, &mode) || !tar_number(h + 124, 12, &size) || !tar_number(h + 136, 12, &mtime) ||
            size < 0)
            return header_fail(r, "invalid header", hard);
        char type = (char)h[156];
        size_t name_n = field_len(h, 100);
        size_t link_n = field_len(h + 157, 100);
        size_t prefix_n = field_len(h + 345, 155);
        const u8 *data = h + BLOCK;
        size_t data_off = r->pos + BLOCK;

        bool is_reg = type == '0' || type == 0 || type == '7' || type == 'S';
        bool supported = type == '0' || type == 0 || type == '1' || type == '2' || type == '5' || type == '6' ||
                         type == '3' || type == '4' || type == '7' || type == 'L' || type == 'K' || type == 'S';
        /* the pax size overrides the header's size for the member that follows */
        if (type != 'x' && type != 'g' && type != 'X' && pax.has_size && pax.size >= 0) size = pax.size;
        size_t skip = 0;
        if (type == 'x' || type == 'g' || type == 'X' || type == 'L' || type == 'K' || is_reg || !supported) {
            /* tarfile reads the byte before the next header: that byte must exist, so the data and its padding up
             * to the block must be in the archive */
            if ((u64)size > (u64)(r->n - data_off) || (u64)(r->n - data_off) < ((u64)size + BLOCK - 1) / BLOCK * BLOCK) {
                r->err = "unexpected end of data";
                r->done = true;
                return -1;
            }
            skip = (size_t)(((u64)size + BLOCK - 1) / BLOCK * BLOCK);
        }
        r->pos = data_off + skip;

        if (type == 'x' || type == 'X') {
            hard = ext = true;
            pax = pax_global; /* copy of the globals */
            if (!pax_parse(data, (size_t)size, &pax)) {
                r->err = "invalid header";
                r->done = true;
                return -1;
            }
            continue;
        }
        if (type == 'g') {
            if (!pax_parse(data, (size_t)size, &pax_global)) {
                r->err = "invalid header";
                r->done = true;
                return -1;
            }
            hard = ext = true;
            pax = pax_global;
            continue;
        }
        if (type == 'L') {
            hard = ext = true;
            longname = data;
            longname_n = field_len(data, (size_t)size);
            continue;
        }
        if (type == 'K') {
            hard = ext = true;
            longlink = data;
            longlink_n = field_len(data, (size_t)size);
            continue;
        }

        /* a real member */
        const u8 *np = h;
        const u8 *lp = h + 157;
        ShsmTarKind kind = kind_of(type);
        char outtype = type == 0 ? '0' : type;
        /* old V7 directories: a regular file whose name ends in "/" */
        if (type == 0 && name_n > 0 && h[name_n - 1] == '/') {
            kind = SHSM_TAR_DIR;
            outtype = '5';
        }
        if (kind == SHSM_TAR_DIR) {
            while (name_n > 0 && np[name_n - 1] == '/') name_n--; /* remove redundant slashes */
        }
        /* ustar long names: prefix + "/" + name; GNU and pax names replace the whole thing */
        const char *name;
        if (pax.path) {
            size_t pn = pax.path_n;
            if (kind == SHSM_TAR_DIR)
                while (pn > 0 && pax.path[pn - 1] == '/') pn--; /* tarfile strips every trailing "/" of a pax path */
            name = copy_cz(a, pax.path, pn);
        } else if (longname) {
            size_t ln = longname_n;
            if (kind == SHSM_TAR_DIR && ln > 0 && longname[ln - 1] == '/') ln--; /* removesuffix("/"): one slash */
            name = copy_cz(a, longname, ln);
        } else if (prefix_n) {
            char *s = (char *)avp_alloc_noz(a, prefix_n + 1 + name_n + 1);
            memcpy(s, h + 345, prefix_n);
            s[prefix_n] = '/';
            memcpy(s + prefix_n + 1, np, name_n);
            s[prefix_n + 1 + name_n] = 0;
            name = s;
        } else {
            name = copy_cz(a, np, name_n);
        }
        const char *linkname;
        if (pax.linkpath) linkname = copy_cz(a, pax.linkpath, pax.linkpath_n);
        else if (longlink) linkname = copy_cz(a, longlink, longlink_n);
        else linkname = copy_cz(a, lp, link_n);

        m->name = name;
        m->linkname = linkname;
        m->kind = kind;
        m->type = outtype;
        m->size = kind == SHSM_TAR_FILE ? (u64)size : 0;
        m->data = kind == SHSM_TAR_FILE ? data : NULL;
        m->mode = (u32)mode;
        m->mtime = pax.has_mtime ? pax.mtime : mtime;
        return 1;
    }
}

/* ---- extraction ------------------------------------------------------------------------------------------------------- */

typedef struct {
    wchar_t *w; /* the destination folder, absolute, extended-length (\\?\) form, no trailing backslash */
    size_t n;
} Dest;

static wchar_t *to_wide(const char *s, size_t n) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s, (int)n, NULL, 0);
    wchar_t *w = (wchar_t *)avp_xmalloc(((size_t)len + 1) * sizeof(wchar_t));
    if (len > 0) MultiByteToWideChar(CP_UTF8, 0, s, (int)n, w, len);
    w[len] = 0;
    return w;
}

/* Extended-length absolute form of a folder. */
static bool dest_init(Dest *d, const char *folder) {
    wchar_t *w = to_wide(folder, strlen(folder));
    for (wchar_t *c = w; *c; c++)
        if (*c == L'/') *c = L'\\';
    DWORD need = GetFullPathNameW(w, 0, NULL, NULL);
    if (!need) {
        avp_xfree(w);
        return false;
    }
    wchar_t *full = (wchar_t *)avp_xmalloc((need + 8) * sizeof(wchar_t));
    DWORD got = GetFullPathNameW(w, need, full, NULL);
    avp_xfree(w);
    if (!got || got >= need) {
        avp_xfree(full);
        return false;
    }
    while (got > 3 && full[got - 1] == L'\\') full[--got] = 0;
    wchar_t *ext = (wchar_t *)avp_xmalloc((got + 8) * sizeof(wchar_t));
    if (full[0] == L'\\' && full[1] == L'\\' && full[2] == L'?') {
        wcscpy(ext, full);
    } else if (full[0] == L'\\' && full[1] == L'\\') {
        wcscpy(ext, L"\\\\?\\UNC");
        wcscat(ext, full + 1);
    } else {
        wcscpy(ext, L"\\\\?\\");
        wcscat(ext, full);
    }
    avp_xfree(full);
    d->w = ext;
    d->n = wcslen(ext);
    return true;
}

/* A member name made safe the way tarfile's "data" filter does: leading "/" and "\" dropped; an absolute path (drive
 * letter) or one that climbs out of the folder is refused. Returns the wide path under the destination or NULL. */
static wchar_t *member_path(const Dest *d, const char *name, bool *escapes) {
    *escapes = false;
    while (*name == '/' || *name == '\\') name++;
    wchar_t *rel = to_wide(name, strlen(name));
    for (wchar_t *c = rel; *c; c++)
        if (*c == L'/') *c = L'\\';
    if (rel[0] && rel[1] == L':') { /* "C:..." */
        *escapes = true;
        avp_xfree(rel);
        return NULL;
    }
    /* normalise "." and ".." components logically */
    size_t rn = wcslen(rel);
    wchar_t *out = (wchar_t *)avp_xmalloc((d->n + rn + 2) * sizeof(wchar_t));
    memcpy(out, d->w, d->n * sizeof(wchar_t));
    size_t len = d->n;
    size_t base_len = len;
    for (size_t i = 0; i < rn;) {
        size_t j = i;
        while (j < rn && rel[j] != L'\\') j++;
        size_t cn = j - i;
        if (cn == 0 || (cn == 1 && rel[i] == L'.')) {
            /* skip */
        } else if (cn == 2 && rel[i] == L'.' && rel[i + 1] == L'.') {
            if (len <= base_len) {
                *escapes = true;
                avp_xfree(rel);
                avp_xfree(out);
                return NULL;
            }
            while (len > base_len && out[len - 1] != L'\\') len--;
            if (len > base_len) len--;
        } else {
            out[len++] = L'\\';
            memcpy(out + len, rel + i, cn * sizeof(wchar_t));
            len += cn;
        }
        i = j + 1;
    }
    out[len] = 0;
    avp_xfree(rel);
    return out;
}

static bool make_dirs(wchar_t *path, size_t floor_len) {
    /* path is "<dest>\a\b\c"; create each missing level below the destination */
    size_t n = wcslen(path);
    for (size_t i = floor_len + 1; i <= n; i++) {
        if (path[i] == L'\\' || path[i] == 0) {
            wchar_t save = path[i];
            path[i] = 0;
            if (!CreateDirectoryW(path, NULL)) {
                DWORD e = GetLastError();
                if (e != ERROR_ALREADY_EXISTS) {
                    DWORD attr = GetFileAttributesW(path);
                    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                        path[i] = save;
                        return false;
                    }
                }
            }
            path[i] = save;
        }
    }
    return true;
}

static FILETIME unix_to_filetime(i64 t) {
    u64 v = (u64)((t + 11644473600ll) * 10000000ll);
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)v;
    ft.dwHighDateTime = (DWORD)(v >> 32);
    return ft;
}

static bool write_file(const wchar_t *path, const u8 *data, size_t n, i64 mtime) {
    HANDLE f = CreateFileW(path, GENERIC_WRITE | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    while (n && ok) {
        DWORD k = n > 0x40000000u ? 0x40000000u : (DWORD)n, w = 0;
        ok = WriteFile(f, data, k, &w, NULL) && w == k;
        data += k;
        n -= k;
    }
    if (ok) {
        FILETIME ft = unix_to_filetime(mtime);
        SetFileTime(f, NULL, &ft, &ft);
    }
    CloseHandle(f);
    return ok;
}

static void set_dir_time(const wchar_t *path, i64 mtime) {
    HANDLE f = CreateFileW(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    FILETIME ft = unix_to_filetime(mtime);
    SetFileTime(f, NULL, &ft, &ft);
    CloseHandle(f);
}

typedef struct {
    wchar_t *path;
    i64 mtime;
} DirTime;

bool shsm_tar_extract(const void *p, size_t n, const char *folder, size_t *skipped, const char **err) {
    const char *dummy;
    if (!err) err = &dummy;
    *err = NULL;
    if (skipped) *skipped = 0;
    Dest d;
    if (!dest_init(&d, folder)) {
        *err = "cannot resolve the destination folder";
        return false;
    }
    /* the destination itself (and its parents) */
    {
        wchar_t *tmp = (wchar_t *)avp_xmalloc((d.n + 2) * sizeof(wchar_t));
        memcpy(tmp, d.w, (d.n + 1) * sizeof(wchar_t));
        /* base_len: after the \\?\X: prefix (or \\?\UNC\server\share) */
        size_t base_len = 6; /* "\\?\C:" */
        if (wcsncmp(tmp, L"\\\\?\\UNC", 7) == 0) base_len = 7;
        bool ok = make_dirs(tmp, base_len);
        avp_xfree(tmp);
        if (!ok) {
            avp_xfree(d.w);
            *err = "cannot create the destination folder";
            return false;
        }
    }
    AvpArena *a = avp_arena_new(0);
    ShsmTarReader r;
    shsm_tar_open(&r, p, n);
    typedef AVP_VEC(DirTime) DirTimes;
    DirTimes dirs = {0};
    bool ok = true;
    ShsmTarMember m;
    int rc;
    wchar_t *last_parent = NULL;
    size_t last_parent_n = 0;
    while ((rc = shsm_tar_next(&r, a, &m)) > 0) {
        if (m.kind == SHSM_TAR_SYMLINK || m.kind == SHSM_TAR_HARDLINK || m.kind == SHSM_TAR_OTHER) {
            if (skipped) (*skipped)++;
            continue;
        }
        bool escapes;
        wchar_t *target = member_path(&d, m.name, &escapes);
        if (!target) {
            *err = avp_sprintf(a, "\"%s\" would be extracted outside the destination", m.name);
            ok = false;
            break;
        }
        size_t tn = wcslen(target);
        if (tn == d.n) { /* the folder itself (name "." or "") */
            avp_xfree(target);
            continue;
        }
        if (m.kind == SHSM_TAR_DIR) {
            if (!make_dirs(target, d.n)) {
                *err = avp_sprintf(a, "cannot create the folder \"%s\"", m.name);
                avp_xfree(target);
                ok = false;
                break;
            }
            DirTime *dt = avp_vec_push_zero(&dirs);
            dt->path = target;
            dt->mtime = m.mtime;
            continue;
        }
        /* a regular file: its parent folders first (remembering the last one made) */
        size_t pn = tn;
        while (pn > d.n && target[pn - 1] != L'\\') pn--;
        pn--; /* the backslash before the file name */
        if (!(last_parent && last_parent_n == pn && wmemcmp(last_parent, target, pn) == 0)) {
            wchar_t save = target[pn];
            target[pn] = 0;
            bool made = make_dirs(target, d.n);
            target[pn] = save;
            if (!made) {
                *err = avp_sprintf(a, "cannot create the folder of \"%s\"", m.name);
                avp_xfree(target);
                ok = false;
                break;
            }
            avp_xfree(last_parent);
            last_parent = (wchar_t *)avp_xmalloc((pn + 1) * sizeof(wchar_t));
            memcpy(last_parent, target, pn * sizeof(wchar_t));
            last_parent[pn] = 0;
            last_parent_n = pn;
        }
        if (!write_file(target, m.data, (size_t)m.size, m.mtime)) {
            *err = avp_sprintf(a, "cannot write \"%s\"", m.name);
            avp_xfree(target);
            ok = false;
            break;
        }
        avp_xfree(target);
    }
    if (ok && rc < 0) {
        *err = r.err;
        ok = false;
    }
    /* directory times last and deepest first (tarfile sorts them in reverse), since writing inside a folder changes it */
    for (size_t i = dirs.n; i-- > 0;) {
        set_dir_time(dirs.v[i].path, dirs.v[i].mtime);
        avp_xfree(dirs.v[i].path);
    }
    avp_vec_free(&dirs);
    avp_xfree(last_parent);
    avp_xfree(d.w);
    if (!ok && *err) {
        /* keep the message alive past the arena */
        static AVP_THREAD_LOCAL char msg[512];
        snprintf(msg, sizeof msg, "%s", *err);
        *err = msg;
    }
    avp_arena_free(a);
    return ok;
}

/* ---- the tools' selective extraction ------------------------------------------------------------------------------ */

static AVP_THREAD_LOCAL char g_extract_err[512];

static bool extract_fail(const char **err, const char *fmt, const char *name) {
    snprintf(g_extract_err, sizeof g_extract_err, fmt, name);
    *err = g_extract_err;
    return false;
}

bool shsm_tar_extract_files(const void *p, size_t n, const char *folder, ShsmTarSelect select, void *ctx,
                            size_t *written, const char **err) {
    const char *dummy;
    if (!err) err = &dummy;
    *err = NULL;
    if (written) *written = 0;
    Dest d;
    if (!dest_init(&d, folder)) return extract_fail(err, "cannot resolve the destination folder%s", "");
    AvpArena *a = avp_arena_new(0);
    ShsmTarReader r;
    shsm_tar_open(&r, p, n);
    bool ok = true;
    ShsmTarMember m;
    int rc;
    while ((rc = shsm_tar_next(&r, a, &m)) > 0) {
        if (m.kind != SHSM_TAR_FILE || (select && !select(ctx, m.name))) continue;
        bool escapes;
        wchar_t *target = member_path(&d, m.name, &escapes);
        size_t tn = target ? wcslen(target) : 0;
        if (!target || tn == d.n) {
            avp_xfree(target);
            ok = extract_fail(err, "\"%s\" would be extracted outside the destination", m.name);
            break;
        }
        /* os.makedirs(os.path.dirname(target), exist_ok=True): from the drive down, as the Python does */
        size_t pn = tn;
        while (pn > d.n && target[pn - 1] != L'\\') pn--;
        pn--;
        wchar_t save = target[pn];
        target[pn] = 0;
        size_t floor_len = wcsncmp(target, L"\\\\?\\UNC", 7) == 0 ? 7 : 6;
        bool made = make_dirs(target, floor_len);
        target[pn] = save;
        if (!made) {
            avp_xfree(target);
            ok = extract_fail(err, "cannot create the folder of \"%s\"", m.name);
            break;
        }
        HANDLE f = CreateFileW(target, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        avp_xfree(target);
        bool wrote = f != INVALID_HANDLE_VALUE;
        const u8 *data = m.data;
        u64 left = m.size;
        while (wrote && left) {
            DWORD k = left > 0x40000000u ? 0x40000000u : (DWORD)left, w = 0;
            wrote = WriteFile(f, data, k, &w, NULL) && w == k;
            data += k;
            left -= k;
        }
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        if (!wrote) {
            ok = extract_fail(err, "cannot write \"%s\"", m.name);
            break;
        }
        if (written) (*written)++;
    }
    if (ok && rc < 0) ok = extract_fail(err, "%s", r.err ? r.err : "invalid header");
    avp_xfree(d.w);
    avp_arena_free(a);
    return ok;
}
