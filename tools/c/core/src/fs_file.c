/* Whole-file reads and writes, memory-mapped views, buffered open files and atomic replacement (Python's open(),
 * Path.read_bytes/read_text/write_bytes/write_text, os.replace). Files are opened the way the CRT's _wopen does
 * for CPython: share read and write, not inheritable. */
#include "fs_priv.h"

#include <stdio.h>

#define VIEW_MAP_MIN (64 * 1024)
#define WBUF_SIZE (64 * 1024)

HANDLE avp_fs_open_handle(const char *path, DWORD access, DWORD disposition) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    wchar_t *w = avp_fs_wpath(sc, path);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, FALSE};
    HANDLE h = CreateFileW(w, access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) avp_fs_fail_open(GetLastError(), path);
    fs_tmp_done(sc, mk);
    return h;
}

static bool read_fully(HANDLE h, u8 *p, size_t n, const char *path) {
    while (n) {
        DWORD chunk = n > (64u << 20) ? (64u << 20) : (DWORD)n, got = 0;
        if (!ReadFile(h, p, chunk, &got, NULL)) {
            avp_fs_fail_open(GetLastError(), path);
            return false;
        }
        if (got == 0) return true;
        p += got;
        n -= got;
    }
    return true;
}

/* ---- views ----------------------------------------------------------------------------------------------------- */

static const u8 k_empty[1] = {0};

bool avp_view_open(const char *path, AvpFileView *v) {
    memset(v, 0, sizeof *v);
    HANDLE h = avp_fs_open_handle(path, GENERIC_READ, OPEN_EXISTING);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) {
        avp_fs_fail_open(GetLastError(), path);
        CloseHandle(h);
        return false;
    }
    if (sz.QuadPart == 0) {
        CloseHandle(h);
        v->p = k_empty;
        return true;
    }
    if (sz.QuadPart < VIEW_MAP_MIN) {
        size_t n = (size_t)sz.QuadPart;
        u8 *buf = (u8 *)avp_xmalloc(n + 1);
        size_t got = 0;
        while (got < n) {
            DWORD g = 0;
            if (!ReadFile(h, buf + got, (DWORD)(n - got), &g, NULL)) {
                avp_fs_fail_open(GetLastError(), path);
                avp_xfree(buf);
                CloseHandle(h);
                return false;
            }
            if (g == 0) break;
            got += g;
        }
        buf[got] = 0;
        CloseHandle(h);
        v->p = buf;
        v->n = got;
        return true;
    }
    HANDLE map = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!map) {
        avp_fs_fail_open(GetLastError(), path);
        CloseHandle(h);
        return false;
    }
    const void *view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    if (!view) {
        avp_fs_fail_open(GetLastError(), path);
        CloseHandle(map);
        CloseHandle(h);
        return false;
    }
    v->p = (const u8 *)view;
    v->n = (size_t)sz.QuadPart;
    v->file = h;
    v->mapping = map;
    return true;
}

void avp_view_close(AvpFileView *v) {
    if (v->mapping) {
        UnmapViewOfFile(v->p);
        CloseHandle((HANDLE)v->mapping);
        CloseHandle((HANDLE)v->file);
    } else if (v->n) {
        avp_xfree((void *)v->p);
    }
    memset(v, 0, sizeof *v);
}

/* ---- whole-file reads ---------------------------------------------------------------------------------------- */

bool avp_read_all(AvpArena *a, const char *path, AvpBytes *out) {
    out->v = NULL;
    out->n = out->cap = 0;
    HANDLE h = avp_fs_open_handle(path, GENERIC_READ, OPEN_EXISTING);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    size_t cap = GetFileSizeEx(h, &sz) && sz.QuadPart > 0 ? (size_t)sz.QuadPart : 0;
    cap += 1;
    u8 *buf = a ? (u8 *)avp_alloc_noz(a, cap + 1) : (u8 *)avp_xmalloc(cap + 1);
    size_t n = 0;
    for (;;) {
        if (n == cap) {
            size_t ncap = cap * 2;
            if (a) {
                buf = (u8 *)avp_arena_realloc(a, buf, cap + 1, ncap + 1);
            } else buf = (u8 *)avp_xrealloc(buf, ncap + 1);
            cap = ncap;
        }
        size_t want = cap - n;
        DWORD chunk = want > (64u << 20) ? (64u << 20) : (DWORD)want, got = 0;
        if (!ReadFile(h, buf + n, chunk, &got, NULL)) {
            avp_fs_fail_open(GetLastError(), path);
            if (!a) avp_xfree(buf);
            CloseHandle(h);
            return false;
        }
        if (got == 0) break;
        n += got;
    }
    CloseHandle(h);
    buf[n] = 0;
    out->v = buf;
    out->n = n;
    out->cap = cap + 1;
    return true;
}

bool avp_read_text(AvpArena *a, const char *path, AvpTextMode m, AvpStr *out) {
    AvpFileView v;
    if (!avp_view_open(path, &v)) return false;
    bool ok = avp_fs_decode_bytes(a, v.p, v.n, m, out);
    avp_view_close(&v);
    return ok;
}

bool avp_read_lines(AvpArena *a, const char *path, AvpTextMode m, AvpStrVec *out) {
    out->v = NULL;
    out->n = out->cap = 0;
    AvpStr text;
    if (!avp_read_text(a, path, m, &text)) return false;
    AvpFileLines it;
    AvpStr line;
    avp_file_lines_init(&it, text, m.newline);
    while (avp_file_lines_next(&it, &line)) avp_vec_push(out, line);
    return true;
}

/* ---- whole-file writes ------------------------------------------------------------------------------------------ */

static bool write_fully(HANDLE h, const void *p, size_t n, const char *path) {
    const u8 *b = (const u8 *)p;
    while (n) {
        DWORD chunk = n > (64u << 20) ? (64u << 20) : (DWORD)n, put = 0;
        if (!WriteFile(h, b, chunk, &put, NULL)) {
            avp_fs_fail_open(GetLastError(), path);
            return false;
        }
        b += put;
        n -= put;
    }
    return true;
}

static bool write_to(const char *path, DWORD access, DWORD disposition, bool seek_end, const void *p, size_t n) {
    HANDLE h = avp_fs_open_handle(path, access, disposition);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    if (seek_end) {
        LARGE_INTEGER z = {0};
        ok = SetFilePointerEx(h, z, NULL, FILE_END) != 0;
        if (!ok) avp_fs_fail_open(GetLastError(), path);
    }
    if (ok) ok = write_fully(h, p, n, path);
    if (!CloseHandle(h) && ok) {
        avp_fs_fail_open(GetLastError(), path);
        ok = false;
    }
    return ok;
}

bool avp_write_file(const char *path, const void *p, size_t n) {
    return write_to(path, GENERIC_WRITE, CREATE_ALWAYS, false, p, n);
}

bool avp_append_file(const char *path, const void *p, size_t n) {
    return write_to(path, FILE_APPEND_DATA, OPEN_ALWAYS, false, p, n);
}

bool avp_write_text(const char *path, AvpStr text, AvpTextMode m) {
    FsBuf b;
    fsb_init(&b, NULL);
    bool ok = avp_fs_encode_text(&b, text, m, true) && avp_write_file(path, b.p ? b.p : "", b.n);
    avp_xfree(b.p);
    return ok;
}

bool avp_append_text(const char *path, AvpStr text, AvpTextMode m) {
    HANDLE h = avp_fs_open_handle(path, FILE_APPEND_DATA | FILE_READ_ATTRIBUTES, OPEN_ALWAYS);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz = {0};
    GetFileSizeEx(h, &sz);
    FsBuf b;
    fsb_init(&b, NULL);
    bool ok = avp_fs_encode_text(&b, text, m, sz.QuadPart == 0) && write_fully(h, b.p ? b.p : "", b.n, path);
    avp_xfree(b.p);
    if (!CloseHandle(h) && ok) {
        avp_fs_fail_open(GetLastError(), path);
        ok = false;
    }
    return ok;
}

static volatile LONG g_tmp_seq;

static bool write_atomic(const char *path, const void *p, size_t n) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    size_t pl = strlen(path);
    char *tmp = (char *)avp_alloc_noz(sc, pl + 64);
    memcpy(tmp, path, pl);
    snprintf(tmp + pl, 64, ".%lu.%lu.%ld.tmp", GetCurrentProcessId(), GetCurrentThreadId(), InterlockedIncrement(&g_tmp_seq));
    bool ok = false;
    HANDLE h = avp_fs_open_handle(tmp, GENERIC_WRITE, CREATE_NEW);
    if (h != INVALID_HANDLE_VALUE) {
        bool wrote = write_fully(h, p, n, tmp);
        if (!CloseHandle(h) && wrote) {
            avp_fs_fail_open(GetLastError(), tmp);
            wrote = false;
        }
        if (wrote) {
            wchar_t *wt = avp_fs_wpath(sc, tmp), *wp = avp_fs_wpath(sc, path);
            if (MoveFileExW(wt, wp, MOVEFILE_REPLACE_EXISTING)) ok = true;
            else {
                DWORD e = GetLastError();
                DeleteFileW(wt);
                SetLastError(e);
                avp_fs_fail_last(tmp, path);
            }
        } else {
            AvpIoErr keep = *avp_io_err();
            DeleteFileW(avp_fs_wpath(sc, tmp));
            avp_io_fail_errno(keep.err, keep.path, NULL);
        }
    }
    fs_tmp_done(sc, mk);
    return ok;
}

bool avp_write_atomic(const char *path, const void *p, size_t n) { return write_atomic(path, p, n); }

bool avp_write_text_atomic(const char *path, AvpStr text, AvpTextMode m) {
    FsBuf b;
    fsb_init(&b, NULL);
    bool ok = avp_fs_encode_text(&b, text, m, true) && write_atomic(path, b.p ? b.p : "", b.n);
    avp_xfree(b.p);
    return ok;
}

/* ---- open files ---------------------------------------------------------------------------------------------- */

struct AvpFile {
    HANDLE h;
    char path[520];
    bool readable, writable, append, text, bom_pending;
    AvpTextMode tm;
    u8 *wbuf;
    size_t wn;
    bool failed;
};

static bool flush_buf(AvpFile *f) {
    if (f->wn == 0) return true;
    bool ok = write_fully(f->h, f->wbuf, f->wn, f->path);
    f->wn = 0;
    if (!ok) f->failed = true;
    return ok;
}

static AvpFile *file_from_handle(HANDLE h, const char *path, bool readable, bool writable, bool append, bool text,
                                 AvpTextMode tm) {
    AvpFile *f = (AvpFile *)avp_xcalloc(1, sizeof *f);
    f->h = h;
    snprintf(f->path, sizeof f->path, "%s", path);
    f->readable = readable;
    f->writable = writable;
    f->append = append;
    f->text = text;
    f->tm = tm;
    if (writable) f->wbuf = (u8 *)avp_xmalloc(WBUF_SIZE);
    if (text && writable && tm.enc == AVP_ENC_UTF8_SIG) {
        LARGE_INTEGER sz = {0};
        GetFileSizeEx(h, &sz);
        f->bom_pending = !append || sz.QuadPart == 0;
    }
    return f;
}

AvpFile *avp_fopen(const char *path, const char *mode, AvpTextMode tm) {
    bool plus = strchr(mode, '+') != NULL, bin = strchr(mode, 'b') != NULL;
    char kind = mode[0];
    DWORD access = 0, disp = 0;
    bool readable = false, writable = false, append = false;
    switch (kind) {
    case 'r':
        readable = true;
        writable = plus;
        access = GENERIC_READ | (plus ? GENERIC_WRITE : 0);
        disp = OPEN_EXISTING;
        break;
    case 'w':
        writable = true;
        readable = plus;
        access = GENERIC_WRITE | (plus ? GENERIC_READ : 0);
        disp = CREATE_ALWAYS;
        break;
    case 'x':
        writable = true;
        readable = plus;
        access = GENERIC_WRITE | (plus ? GENERIC_READ : 0);
        disp = CREATE_NEW;
        break;
    case 'a':
        writable = true;
        append = true;
        readable = plus;
        access = FILE_APPEND_DATA | FILE_READ_ATTRIBUTES | (plus ? GENERIC_READ : 0);
        disp = OPEN_ALWAYS;
        break;
    default:
        avp_fs_fail_value("invalid mode: '%s'", mode);
        return NULL;
    }
    HANDLE h = avp_fs_open_handle(path, access, disp);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    return file_from_handle(h, path, readable, writable, append, !bin, tm);
}

bool avp_fwrite(AvpFile *f, const void *p, size_t n) {
    if (!f->writable) {
        avp_io_fail_msg(AVP_IOE_OS, EBADF, "not writable");
        return false;
    }
    if (n >= WBUF_SIZE) {
        if (!flush_buf(f)) return false;
        bool ok = write_fully(f->h, p, n, f->path);
        if (!ok) f->failed = true;
        return ok;
    }
    if (f->wn + n > WBUF_SIZE && !flush_buf(f)) return false;
    memcpy(f->wbuf + f->wn, p, n);
    f->wn += n;
    return true;
}

bool avp_fwrite_text(AvpFile *f, AvpStr text) {
    FsBuf b;
    fsb_init(&b, NULL);
    bool ok = avp_fs_encode_text(&b, text, f->tm, f->bom_pending);
    if (ok) {
        f->bom_pending = false;
        ok = avp_fwrite(f, b.p ? b.p : "", b.n);
    }
    avp_xfree(b.p);
    return ok;
}

bool avp_fprintf(AvpFile *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return false;
    char *buf = (char *)avp_xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    AvpStr s = {buf, (size_t)n};
    bool ok = avp_fwrite_text(f, s);
    avp_xfree(buf);
    return ok;
}

i64 avp_fread(AvpFile *f, void *p, size_t n) {
    if (!flush_buf(f)) return -1;
    size_t total = 0;
    u8 *b = (u8 *)p;
    while (total < n) {
        DWORD want = (n - total) > (64u << 20) ? (64u << 20) : (DWORD)(n - total), got = 0;
        if (!ReadFile(f->h, b + total, want, &got, NULL)) {
            avp_fs_fail_open(GetLastError(), f->path);
            return -1;
        }
        if (got == 0) break;
        total += got;
    }
    return (i64)total;
}

bool avp_fseek(AvpFile *f, i64 offset, int whence) {
    if (!flush_buf(f)) return false;
    LARGE_INTEGER o;
    o.QuadPart = offset;
    DWORD m = whence == 0 ? FILE_BEGIN : whence == 1 ? FILE_CURRENT : FILE_END;
    if (!SetFilePointerEx(f->h, o, NULL, m)) {
        avp_fs_fail_open(GetLastError(), f->path);
        return false;
    }
    return true;
}

i64 avp_ftell(AvpFile *f) {
    if (!flush_buf(f)) return -1;
    LARGE_INTEGER z = {0}, r;
    if (!SetFilePointerEx(f->h, z, &r, FILE_CURRENT)) {
        avp_fs_fail_open(GetLastError(), f->path);
        return -1;
    }
    return r.QuadPart;
}

i64 avp_fsize(AvpFile *f) {
    if (!flush_buf(f)) return -1;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(f->h, &sz)) {
        avp_fs_fail_open(GetLastError(), f->path);
        return -1;
    }
    return sz.QuadPart;
}

bool avp_ftruncate(AvpFile *f, i64 size) {
    if (!flush_buf(f)) return false;
    FILE_END_OF_FILE_INFO info;
    info.EndOfFile.QuadPart = size;
    if (!SetFileInformationByHandle(f->h, FileEndOfFileInfo, &info, sizeof info)) {
        avp_fs_fail_open(GetLastError(), f->path);
        return false;
    }
    return true;
}

bool avp_fflush(AvpFile *f) { return flush_buf(f); }

bool avp_fsync(AvpFile *f) {
    if (!flush_buf(f)) return false;
    if (!FlushFileBuffers(f->h)) {
        avp_fs_fail_open(GetLastError(), f->path);
        return false;
    }
    return true;
}

void *avp_fhandle(AvpFile *f) { return f->h; }

bool avp_fclose(AvpFile *f) {
    bool ok = flush_buf(f);
    if (!CloseHandle(f->h) && ok) {
        avp_fs_fail_open(GetLastError(), f->path);
        ok = false;
    }
    avp_xfree(f->wbuf);
    avp_xfree(f);
    return ok;
}

bool avp_fstat(AvpFile *f, AvpStat *st) {
    flush_buf(f);
    return avp_fs_stat_handle(f->h, f->path, st);
}
