/* shutil (rmtree, copyfile, copy2, copystat, copytree, move, which) and tempfile (gettempdir, mkstemp, mkdtemp,
 * TemporaryDirectory cleanup) with CPython 3.14's behaviour on Windows. */
#include "fs_priv.h"

#include <bcrypt.h>
#include <stdio.h>

/* ---- rmtree ------------------------------------------------------------------------------------------------------ */

typedef struct {
    unsigned flags;
    bool failed;
} Rm;

static bool rm_ignore(const Rm *r) { return (r->flags & AVP_RMTREE_IGNORE_ERRORS) != 0; }

/* A failure of one step: ignored with IGNORE_ERRORS, else the walk stops (Python's default onexc re-raises). */
static void rm_fail(Rm *r) {
    if (!rm_ignore(r)) r->failed = true;
}

static bool clear_readonly(const char *path) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    wchar_t *w = avp_fs_wpath(sc, path);
    DWORD attr = GetFileAttributesW(w);
    bool ok = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY) &&
              SetFileAttributesW(w, attr & ~(DWORD)FILE_ATTRIBUTE_READONLY);
    fs_tmp_done(sc, mk);
    return ok;
}

static void rm_unlink(Rm *r, const char *path, bool dir) {
    if (r->failed) return;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (dir ? avp_rmdir(path) : avp_remove(path)) return;
        if (avp_io_is(ENOENT)) return;
        if (attempt == 0 && (r->flags & AVP_RMTREE_CLEAR_READONLY) && avp_io_is(EACCES) && clear_readonly(path)) continue;
        break;
    }
    rm_fail(r);
}

static void rm_dir(Rm *r, const char *path) {
    if (r->failed) return;
    AvpArena *sc = avp_arena_new(0);
    AvpDirEntries es;
    if (!avp_scandir(sc, path, &es)) {
        if (!avp_io_is(ENOENT)) rm_fail(r);
        avp_arena_free(sc);
        return;
    }
    /* os.walk(topdown=False, followlinks=_walk_symlinks_as_files): only real folders are entered; symlinks and
     * junctions to folders count as files and are removed as links. */
    for (size_t i = 0; i < es.n && !r->failed; i++) {
        const AvpDirEntry *e = &es.v[i];
        bool real_dir = (e->attrs & FILE_ATTRIBUTE_DIRECTORY) && !e->is_junction && !e->is_symlink;
        if (real_dir) rm_dir(r, e->path.p);
    }
    for (size_t i = 0; i < es.n && !r->failed; i++) {
        const AvpDirEntry *e = &es.v[i];
        if ((e->attrs & FILE_ATTRIBUTE_DIRECTORY) && !e->is_junction && !e->is_symlink) rm_unlink(r, e->path.p, true);
    }
    for (size_t i = 0; i < es.n && !r->failed; i++) {
        const AvpDirEntry *e = &es.v[i];
        bool real_dir = (e->attrs & FILE_ATTRIBUTE_DIRECTORY) && !e->is_junction && !e->is_symlink;
        if (!real_dir) rm_unlink(r, e->path.p, false);
    }
    avp_vec_free(&es);
    avp_arena_free(sc);
}

bool avp_rmtree(const char *path, unsigned flags) {
    Rm r = {flags, false};
    AvpStat st;
    if (!avp_lstat(path, &st)) {
        rm_fail(&r);
        return !r.failed;
    }
    if (AVP_S_ISLNK(st.mode) ||
        ((st.file_attributes & FILE_ATTRIBUTE_REPARSE_POINT) && st.reparse_tag == AVP_IO_REPARSE_TAG_MOUNT_POINT)) {
        avp_io_fail_msg(AVP_IOE_OS, 0, "Cannot call rmtree on a symbolic link");
        rm_fail(&r);
        return !r.failed;
    }
    rm_dir(&r, path);
    if (!r.failed) rm_unlink(&r, path, true);
    return !r.failed;
}

/* ---- copyfile / copy2 / copystat ------------------------------------------------------------------------------------- */

static void fail_shutil(int tag, const char *fmt, ...) {
    char text[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    avp_io_fail_msg(AVP_IOE_SHUTIL, tag, "%s", text);
}

bool avp_copyfile(const char *src, const char *dst) {
    if (avp_exists(src) && avp_exists(dst) && avp_path_samefile(src, dst)) {
        AvpArena *sc = fs_tmp();
        AvpArenaMark mk = avp_arena_mark(sc);
        FsBuf b;
        fsb_init(&b, sc);
        avp_fs_repr(&b, src);
        fsb_putz(&b, " and ");
        avp_fs_repr(&b, dst);
        fsb_putz(&b, " are the same file");
        fail_shutil(1, "%s", fsb_cz(&b));
        fs_tmp_done(sc, mk);
        return false;
    }
    HANDLE hs = avp_fs_open_handle(src, GENERIC_READ, OPEN_EXISTING);
    if (hs == INVALID_HANDLE_VALUE) return false;
    HANDLE hd = avp_fs_open_handle(dst, GENERIC_WRITE, CREATE_ALWAYS);
    if (hd == INVALID_HANDLE_VALUE) {
        CloseHandle(hs);
        return false;
    }
    const size_t BUF = 1024 * 1024;
    u8 *buf = (u8 *)avp_xmalloc(BUF);
    bool ok = true;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(hs, buf, (DWORD)BUF, &got, NULL)) {
            avp_fs_fail_open(GetLastError(), src);
            ok = false;
            break;
        }
        if (got == 0) break;
        DWORD off = 0;
        while (off < got) {
            DWORD put = 0;
            if (!WriteFile(hd, buf + off, got - off, &put, NULL)) {
                avp_fs_fail_open(GetLastError(), dst);
                ok = false;
                break;
            }
            off += put;
        }
        if (!ok) break;
    }
    avp_xfree(buf);
    CloseHandle(hs);
    if (!CloseHandle(hd) && ok) {
        avp_fs_fail_open(GetLastError(), dst);
        ok = false;
    }
    return ok;
}

bool avp_copystat(const char *src, const char *dst) {
    AvpStat st;
    if (!avp_stat(src, &st)) return false;
    if (!avp_utime_ns(dst, st.atime_ns, st.mtime_ns)) return false;
    return avp_chmod(dst, (int)(st.mode & 07777));
}

bool avp_copy2(const char *src, const char *dst0) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    const char *dst = dst0;
    if (avp_isdir(dst)) {
        AvpStr bn = avp_path_basename(src);
        dst = avp_path_join(sc, dst0, fs_cz(sc, bn));
    }
    COPYFILE2_EXTENDED_PARAMETERS params = {sizeof params, COPY_FILE_ALLOW_DECRYPTED_DESTINATION, NULL, NULL, NULL};
    HRESULT hr = CopyFile2(avp_fs_wpath(sc, src), avp_fs_wpath(sc, dst), &params);
    if (SUCCEEDED(hr)) {
        fs_tmp_done(sc, mk);
        return true;
    }
    unsigned code = ((hr & 0xFFFF0000) == 0x80070000) ? (unsigned)(hr & 0xFFFF) : (unsigned)hr;
    bool ok;
    if (code == ERROR_ACCESS_DENIED) {
        ok = avp_copyfile(src, dst) && avp_copystat(src, dst);
    } else {
        avp_io_fail_win(code, NULL, NULL);
        ok = false;
    }
    fs_tmp_done(sc, mk);
    return ok;
}

/* ---- copytree ---------------------------------------------------------------------------------------------------- */

typedef struct {
    const AvpCopytreeOpts *o;
    FsBuf errors; /* the items of the repr() of shutil.Error's list */
    size_t nerrors;
} Ct;

static void ct_error(Ct *c, const char *src, const char *dst, AvpStr why) {
    if (c->nerrors++) fsb_putz(&c->errors, ", ");
    fsb_putc(&c->errors, '(');
    avp_fs_repr(&c->errors, src);
    fsb_putz(&c->errors, ", ");
    avp_fs_repr(&c->errors, dst);
    fsb_putz(&c->errors, ", ");
    char *z = (char *)fs_dup(avp_arena(), why.p, why.n).p;
    avp_fs_repr(&c->errors, z);
    fsb_putc(&c->errors, ')');
}

static bool ct_ignored(const Ct *c, const char *name) {
    if (!c->o) return false;
    for (size_t i = 0; i < c->o->n_ignore; i++)
        if (avp_fnmatch(name, c->o->ignore[i])) return true;
    return false;
}

static bool copytree_impl(Ct *c, const char *src, const char *dst, bool top) {
    AvpArena *sc = avp_arena_new(0);
    AvpDirEntries es;
    if (!avp_scandir(sc, src, &es)) {
        avp_arena_free(sc);
        return false; /* the OSError of os.scandir propagates */
    }
    if (!avp_makedirs(dst, c->o && c->o->dirs_exist_ok)) {
        avp_arena_free(sc);
        return false;
    }
    for (size_t i = 0; i < es.n; i++) {
        const AvpDirEntry *e = &es.v[i];
        if (ct_ignored(c, e->name.p)) continue;
        AvpStr dstname = {avp_path_join(sc, dst, e->name.p), 0};
        dstname.n = strlen(dstname.p);
        bool ok = true;
        if (e->is_symlink) {
            char *linkto = NULL;
            if (!avp_readlink(sc, e->path.p, &linkto)) ok = false;
            else if (c->o && c->o->symlinks) {
                ok = avp_symlink(linkto, dstname.p, e->is_dir) && avp_copystat(e->path.p, dstname.p);
            } else if (e->is_dir) ok = copytree_impl(c, e->path.p, dstname.p, false);
            else ok = avp_copy2(e->path.p, dstname.p);
        } else if (e->is_dir) {
            ok = copytree_impl(c, e->path.p, dstname.p, false);
        } else ok = avp_copy2(e->path.p, dstname.p);
        if (!ok) {
            const AvpIoErr *ie = avp_io_err();
            if (ie->kind == AVP_IOE_SHUTIL && ie->err == 2) {
                /* a nested shutil.Error: its items are flattened into ours */
                if (c->nerrors++) fsb_putz(&c->errors, ", ");
                const char *m = ie->msg;
                size_t ml = strlen(m);
                if (ml >= 2) fsb_put(&c->errors, m + 1, ml - 2);
            } else {
                AvpStr why = avp_io_err_str(sc);
                ct_error(c, e->path.p, dstname.p, why);
            }
        }
    }
    avp_vec_free(&es);
    avp_arena_free(sc);
    if (!avp_copystat(src, dst)) {
        const AvpIoErr *ie = avp_io_err();
        if (ie->winerror == 0) {
            AvpArena *s2 = fs_tmp();
            AvpArenaMark mk = avp_arena_mark(s2);
            AvpStr why = avp_io_err_str(s2);
            ct_error(c, src, dst, why);
            fs_tmp_done(s2, mk);
        }
    }
    (void)top;
    return true;
}

bool avp_copytree(const char *src, const char *dst, const AvpCopytreeOpts *opts) {
    Ct c = {opts, {0}, 0};
    fsb_init(&c.errors, NULL);
    bool ok = copytree_impl(&c, src, dst, true);
    if (ok && c.nerrors) {
        avp_io_fail_msg(AVP_IOE_SHUTIL, 2, "[%s]", fsb_cz(&c.errors));
        ok = false;
    }
    avp_xfree(c.errors.p);
    return ok;
}

bool avp_move(const char *src, const char *dst) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    const char *real_dst = dst;
    bool ok = true;
    if (avp_isdir(dst)) {
        if (avp_path_samefile(src, dst) && !avp_islink(src)) {
            ok = avp_rename(src, dst);
            fs_tmp_done(sc, mk);
            return ok;
        }
        AvpStr bn = avp_path_basename(src);
        real_dst = avp_path_join(sc, dst, fs_cz(sc, bn));
        if (avp_exists(real_dst)) {
            fail_shutil(0, "Destination path '%s' already exists", real_dst);
            fs_tmp_done(sc, mk);
            return false;
        }
    }
    if (!avp_rename(src, real_dst)) {
        if (avp_islink(src)) {
            char *linkto;
            ok = avp_readlink(sc, src, &linkto) && avp_symlink(linkto, real_dst, avp_isdir(src)) && avp_remove(src);
        } else if (avp_isdir(src)) {
            AvpCopytreeOpts o = {NULL, 0, false, true};
            ok = avp_copytree(src, real_dst, &o) && avp_rmtree(src, 0);
        } else ok = avp_copy2(src, real_dst) && avp_remove(src);
    }
    fs_tmp_done(sc, mk);
    return ok;
}

/* ---- tempfile ---------------------------------------------------------------------------------------------------- */

static void random_name(char out[9]) {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
    u8 r[8];
    if (BCryptGenRandom(NULL, r, sizeof r, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        for (int i = 0; i < 8; i++) r[i] = (u8)(t.QuadPart >> (i * 3)) ^ (u8)(GetCurrentThreadId() + i * 31);
    }
    for (int i = 0; i < 8; i++) out[i] = chars[r[i] % 37];
    out[8] = 0;
}

static INIT_ONCE g_tmp_once = INIT_ONCE_STATIC_INIT;
static char *g_tmpdir;

static BOOL CALLBACK find_tmpdir(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    (void)once;
    (void)param;
    (void)ctx;
    AvpArena *sc = avp_arena_new(0);
    const char *cands[12];
    size_t n = 0;
    const char *envs[] = {"TMPDIR", "TEMP", "TMP"};
    for (size_t i = 0; i < 3; i++) {
        char *v = avp_getenv(sc, envs[i]);
        if (v) cands[n++] = v;
    }
    cands[n++] = avp_path_expanduser(sc, "~\\AppData\\Local\\Temp");
    char *sr = avp_getenv(sc, "SYSTEMROOT");
    cands[n++] = sr ? avp_path_join(sc, sr, "Temp") : "%SYSTEMROOT%\\Temp";
    cands[n++] = "c:\\temp";
    cands[n++] = "c:\\tmp";
    cands[n++] = "\\temp";
    cands[n++] = "\\tmp";
    cands[n++] = avp_getcwd(sc);
    for (size_t i = 0; i < n; i++) {
        const char *dir = cands[i];
        if (strcmp(dir, ".") != 0) dir = avp_path_abspath(sc, dir);
        for (int tries = 0; tries < 100; tries++) {
            char nm[9];
            random_name(nm);
            char leaf[16] = "tmp";
            memcpy(leaf + 3, nm, 9);
            const char *file = avp_path_join(sc, dir, leaf);
            HANDLE h = avp_fs_open_handle(file, GENERIC_WRITE, CREATE_NEW);
            if (h != INVALID_HANDLE_VALUE) {
                DWORD put;
                WriteFile(h, "blat", 4, &put, NULL);
                CloseHandle(h);
                avp_remove(file);
                g_tmpdir = _strdup(dir);
                avp_arena_free(sc);
                return TRUE;
            }
            if (!avp_io_is(EEXIST)) break;
        }
    }
    g_tmpdir = _strdup(".");
    avp_arena_free(sc);
    return TRUE;
}

char *avp_gettempdir(AvpArena *a) {
    InitOnceExecuteOnce(&g_tmp_once, find_tmpdir, NULL, NULL);
    return avp_strdup(a, g_tmpdir);
}

static char *temp_name(AvpArena *a, const char *prefix, const char *suffix, const char *dir) {
    char nm[9];
    random_name(nm);
    size_t pl = strlen(prefix), sl = strlen(suffix);
    char *leaf = (char *)avp_alloc_noz(a, pl + 8 + sl + 1);
    memcpy(leaf, prefix, pl);
    memcpy(leaf + pl, nm, 8);
    memcpy(leaf + pl + 8, suffix, sl + 1);
    return avp_path_join(a, dir, leaf);
}

AvpFile *avp_mkstemp(AvpArena *a, const char *prefix, const char *suffix, const char *dir, char **path_out) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    if (!prefix) prefix = "tmp";
    if (!suffix) suffix = "";
    char *d = dir ? avp_path_abspath(sc, dir) : avp_gettempdir(sc);
    AvpFile *f = NULL;
    for (int seq = 0; seq < 10000 && !f; seq++) {
        AvpArenaMark m2 = avp_arena_mark(sc);
        char *file = temp_name(sc, prefix, suffix, d);
        f = avp_fopen(file, "x+b", AVP_TM_UTF8);
        if (f) {
            *path_out = avp_path_abspath(a, file);
            break;
        }
        avp_arena_reset(sc, m2);
        if (avp_io_is(EEXIST)) continue;
        if (avp_io_is(EACCES) && avp_isdir(d)) continue; /* a race with a deleted name on Windows */
        break;
    }
    fs_tmp_done(sc, mk);
    if (!f && !avp_io_err()->kind) avp_io_fail_errno(EEXIST, NULL, NULL);
    return f;
}

char *avp_mkdtemp(AvpArena *a, const char *prefix, const char *suffix, const char *dir) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    if (!prefix) prefix = "tmp";
    if (!suffix) suffix = "";
    char *d = dir ? avp_path_abspath(sc, dir) : avp_gettempdir(sc);
    char *res = NULL;
    for (int seq = 0; seq < 10000 && !res; seq++) {
        AvpArenaMark m2 = avp_arena_mark(sc);
        char *file = temp_name(sc, prefix, suffix, d);
        if (avp_mkdir(file)) {
            res = avp_path_abspath(a, file);
            break;
        }
        avp_arena_reset(sc, m2);
        if (avp_io_is(EEXIST)) continue;
        if (avp_io_is(EACCES) && avp_isdir(d)) continue;
        break;
    }
    fs_tmp_done(sc, mk);
    return res;
}

void avp_tempdir_cleanup(const char *path) { avp_rmtree(path, AVP_RMTREE_CLEAR_READONLY | AVP_RMTREE_IGNORE_ERRORS); }
