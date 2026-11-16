/* The tar reader of shsm_archive.h against Python's tarfile: member lists of tars written by tarfile in the pax, GNU and ustar
 * formats and of a real `git archive` (zip_tar_corpus.h, generator at its end), the cut-at-every-offset behaviour, and
 * extraction to a folder. */
#include "avp/avp.h"
#include "avp/hash.h"
#include "avp/test.h"
#include "shsm_archive.h"
#include "shsm_zlib.h"

#include <stdio.h>
#include <wchar.h>
#include <windows.h>

#include "shsm_archive_corpus.h"
#include "shsm_archive_git.h"

static const char *kind_name(ShsmTarKind k) {
    switch (k) {
    case SHSM_TAR_FILE: return "FILE";
    case SHSM_TAR_DIR: return "DIR";
    case SHSM_TAR_SYMLINK: return "SYMLINK";
    case SHSM_TAR_HARDLINK: return "HARDLINK";
    default: return "OTHER";
    }
}

/* Reads every member of the tar and compares it with the rows tarfile gave. */
static void check_rows(AvpArena *a, const char *label, const u8 *tar, size_t n, const TarRow *rows, size_t nrows) {
    ShsmTarReader r;
    shsm_tar_open(&r, tar, n);
    ShsmTarMember m;
    size_t i = 0;
    int rc;
    while ((rc = shsm_tar_next(&r, a, &m)) > 0) {
        if (i >= nrows) {
            CHECK_MSG(0, "%s: extra member \"%s\"", label, m.name);
            i++;
            continue;
        }
        const TarRow *w = &rows[i];
        CHECK_MSG(strcmp(m.name, w->name) == 0, "%s #%zu: name \"%s\", tarfile \"%s\"", label, i, m.name, w->name);
        CHECK_MSG(strcmp(kind_name(m.kind), w->kind) == 0, "%s #%zu %s: kind %s, tarfile %s", label, i, m.name, kind_name(m.kind), w->kind);
        CHECK_MSG(m.size == w->size, "%s #%zu %s: size %llu, tarfile %llu", label, i, m.name, (unsigned long long)m.size, w->size);
        CHECK_MSG(m.mtime == w->mtime, "%s #%zu %s: mtime %lld, tarfile %lld", label, i, m.name, (long long)m.mtime, w->mtime);
        CHECK_MSG(strcmp(m.linkname, w->link) == 0, "%s #%zu %s: link \"%s\", tarfile \"%s\"", label, i, m.name, m.linkname, w->link);
        if (m.kind == SHSM_TAR_FILE)
            CHECK_MSG(strcmp(avp_sha256_hex(a, m.data, (size_t)m.size), w->sha256) == 0, "%s #%zu %s: data differs", label, i, m.name);
        i++;
    }
    CHECK_MSG(rc == 0, "%s: ended with an error: %s", label, r.err ? r.err : "?");
    CHECK_MSG(i == nrows, "%s: %zu members, tarfile %zu", label, i, nrows);
}

AVP_TEST(member_lists_equal_tarfile) {
    AvpArena *a = avp_arena_new(0);
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_tar_cases); i++) {
        const TarCase *c = &k_tar_cases[i];
        size_t tn;
        ShsmZErr err;
        u8 *tar = shsm_zlib_decompress(a, c->z, c->zn, 15, c->tar_n, &tn, &err);
        REQUIRE(tar != NULL && tn == c->tar_n);
        check_rows(a, c->name, tar, tn, c->rows, c->nrows);
    }
    avp_arena_free(a);
}

/* `git archive --format=tar <commit> -- <path>` of a fixed commit of this repository: a real archive with git's
 * global pax header (shsm_archive_git.h). */
static u8 *git_archive(AvpArena *a, const char *commit, const char *path, size_t *n) {
    /* avp_run: bounded by a timeout, the tree killed on expiry (no _popen: it could wait forever on a hung git) */
    const char *argv[] = {"git", "archive", "--format=tar", commit, "--", path, NULL};
    AvpProcSpec s;
    memset(&s, 0, sizeof s);
    s.argv = argv;
    s.in = AVP_IO_NULL;
    s.out = AVP_IO_CAPTURE;
    s.err = AVP_IO_NULL;
    s.timeout = 120;
    s.creation_flags = AVP_CREATE_NO_WINDOW;
    s.arena = a;
    AvpProcResult r = avp_run(&s);
    if (r.status != AVP_PROC_OK || r.returncode != 0 || !r.out.p || !r.out.n) return NULL;
    *n = r.out.n;
    return (u8 *)r.out.p;
}

AVP_TEST(git_archive_of_a_commit) {
    AvpArena *a = avp_arena_new(0);
    size_t n = 0;
    u8 *tar = git_archive(a, k_git_commit, k_git_path, &n);
    if (!tar) {
        printf("(git archive not available: skipped)\n");
        avp_arena_free(a);
        return;
    }
    check_rows(a, "git", tar, n, k_rows_git, AVP_ARRAY_LEN(k_rows_git));
    avp_arena_free(a);
}

AVP_TEST(cut_at_every_offset_like_tarfile) {
    AvpArena *a = avp_arena_new(0);
    const TarCase *c = NULL;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_tar_cases); i++)
        if (strcmp(k_tar_cases[i].name, "pax_ustar") == 0) c = &k_tar_cases[i];
    REQUIRE(c != NULL);
    size_t tn;
    u8 *tar = shsm_zlib_decompress(a, c->z, c->zn, 15, c->tar_n, &tn, NULL);
    REQUIRE(tar != NULL);
    REQUIRE(strlen(k_trunc_pax_ustar) == tn + 1);
    for (size_t cut = 0; cut <= tn; cut++) {
        char want = k_trunc_pax_ustar[cut];
        bool py_err = want >= 'a';
        size_t py_count = (size_t)(py_err ? want - 'a' : want - '0');
        ShsmTarReader r;
        shsm_tar_open(&r, tar, cut);
        ShsmTarMember m;
        size_t count = 0;
        int rc;
        AvpArenaMark mark = avp_arena_mark(a);
        while ((rc = shsm_tar_next(&r, a, &m)) > 0) {
            count++;
            /* a member's data lies inside the cut buffer, whatever the header says */
            CHECK(m.kind != SHSM_TAR_FILE || (m.data >= tar && m.data + m.size <= tar + cut));
        }
        avp_arena_reset(a, mark);
        if (!py_err) {
            CHECK_MSG(rc == 0 && count == py_count, "cut %zu: %zu members (rc %d), tarfile %zu", cut, count, rc, py_count);
        } else {
            /* tarfile yields a member whose data is cut off and raises on the next step; we refuse that member */
            CHECK_MSG(rc < 0 && (count == py_count || count + 1 == py_count), "cut %zu: %zu members (rc %d), tarfile %zu then an error", cut, count, rc, py_count);
        }
    }
    avp_arena_free(a);
}

AVP_TEST(damaged_headers) {
    AvpArena *a = avp_arena_new(0);
    const TarCase *c = &k_tar_cases[0];
    size_t tn;
    u8 *tar = shsm_zlib_decompress(a, c->z, c->zn, 15, c->tar_n, &tn, NULL);
    REQUIRE(tar != NULL);
    /* a flipped byte in the first header's name: the checksum no longer matches */
    u8 *m1 = (u8 *)avp_memdup(a, tar, tn);
    m1[1024 + 1] ^= 1; /* a name byte of the header after the global pax header and its data: the checksum fails */
    ShsmTarReader r;
    ShsmTarMember m;
    shsm_tar_open(&r, m1, tn);
    int rc = shsm_tar_next(&r, a, &m);
    CHECK(rc < 0);
    CHECK_CZ(r.err, "bad checksum");
    shsm_tar_open(&r, tar, 0);
    CHECK(shsm_tar_next(&r, a, &m) < 0);
    CHECK_CZ(r.err, "empty file");
    /* a tar of one zero block is an empty archive */
    u8 zeros[1024] = {0};
    shsm_tar_open(&r, zeros, sizeof zeros);
    CHECK(shsm_tar_next(&r, a, &m) == 0);
    /* garbage after a good first member ends the listing without an error, as tarfile does */
    const TarCase *u = &k_tar_cases[2];
    u8 *ustar = shsm_zlib_decompress(a, u->z, u->zn, 15, u->tar_n, &tn, NULL);
    REQUIRE(ustar != NULL);
    u8 *g = (u8 *)avp_memdup(a, ustar, tn);
    g[1024 + 100] ^= 1; /* some byte of the second header: whatever it is, the header cannot validate */
    shsm_tar_open(&r, g, tn);
    CHECK(shsm_tar_next(&r, a, &m) > 0);
    CHECK(shsm_tar_next(&r, a, &m) == 0);
    avp_arena_free(a);
}

/* ---- extraction ---- */

static void remove_tree(const wchar_t *dir) {
    wchar_t pattern[MAX_PATH];
    swprintf(pattern, MAX_PATH, L"%ls\\*", dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            wchar_t path[MAX_PATH];
            swprintf(path, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) remove_tree(path);
            else DeleteFileW(path);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir);
}

static i64 file_mtime(const char *path, bool dir) {
    wchar_t w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH);
    HANDLE h = CreateFileW(w, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           dir ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    FILETIME ft;
    bool ok = GetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    if (!ok) return -1;
    u64 v = ((u64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (i64)(v / 10000000ull) - 11644473600ll;
}

AVP_TEST(extract_like_extractall) {
    AvpArena *a = avp_arena_new(0);
    char tmp[MAX_PATH], root[MAX_PATH + 64];
    GetTempPathA(sizeof tmp, tmp);
    snprintf(root, sizeof root, "%savp_tar_test_%lu", tmp, GetCurrentProcessId());
    wchar_t wroot[MAX_PATH + 64];
    MultiByteToWideChar(CP_UTF8, 0, root, -1, wroot, MAX_PATH + 64);
    remove_tree(wroot);
    const TarCase *c = &k_tar_cases[0]; /* pax */
    size_t tn;
    u8 *tar = shsm_zlib_decompress(a, c->z, c->zn, 15, c->tar_n, &tn, NULL);
    REQUIRE(tar != NULL);
    size_t skipped = 99;
    const char *err = NULL;
    char dest[MAX_PATH + 80];
    snprintf(dest, sizeof dest, "%s/out/nested", root); /* the folder (and its parents) are created */
    CHECK_MSG(shsm_tar_extract(tar, tn, dest, &skipped, &err), "extract: %s", err ? err : "?");
    CHECK_EQ_U64(skipped, 2); /* the symbolic and the hard link */
    for (size_t i = 0; i < c->nrows; i++) {
        const TarRow *w = &c->rows[i];
        char path[2 * MAX_PATH];
        snprintf(path, sizeof path, "%s/%s", dest, w->name);
        DWORD attr;
        wchar_t wp[2 * MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 2 * MAX_PATH);
        if (strcmp(w->kind, "FILE") == 0) {
            attr = GetFileAttributesW(wp);
            /* long paths: retry in the extended-length form */
            if (attr == INVALID_FILE_ATTRIBUTES) {
                wchar_t ext[2 * MAX_PATH + 8];
                swprintf(ext, 2 * MAX_PATH + 8, L"\\\\?\\%ls", wp);
                for (wchar_t *p = ext; *p; p++) if (*p == L'/') *p = L'\\';
                attr = GetFileAttributesW(ext);
            }
            CHECK_MSG(attr != INVALID_FILE_ATTRIBUTES, "missing file %s", w->name);
            if (strlen(path) < 250) {
                CHECK_CZ(avp_file_digest_hex(a, AVP_HASH_SHA256, path), w->sha256);
                CHECK_MSG(file_mtime(path, false) == w->mtime, "%s: mtime %lld, archive %lld", w->name, (long long)file_mtime(path, false), w->mtime);
            }
        } else if (strcmp(w->kind, "DIR") == 0 && strlen(path) < 250) {
            attr = GetFileAttributesW(wp);
            CHECK_MSG(attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY), "missing folder %s", w->name);
            CHECK_MSG(file_mtime(path, true) == w->mtime, "folder %s: mtime %lld, archive %lld", w->name, (long long)file_mtime(path, true), w->mtime);
        }
    }
    /* extracting again over the existing files works */
    CHECK(shsm_tar_extract(tar, tn, dest, NULL, &err));
    remove_tree(wroot);
    avp_arena_free(a);
}

AVP_TEST(extract_refuses_paths_outside_the_folder) {
    AvpArena *a = avp_arena_new(0);
    size_t tn;
    u8 *tar = shsm_zlib_decompress(a, k_tar_evil, sizeof k_tar_evil, 15, 0, &tn, NULL);
    REQUIRE(tar != NULL);
    char tmp[MAX_PATH], root[MAX_PATH + 64];
    GetTempPathA(sizeof tmp, tmp);
    snprintf(root, sizeof root, "%savp_tar_evil_%lu", tmp, GetCurrentProcessId());
    wchar_t wroot[MAX_PATH + 64];
    MultiByteToWideChar(CP_UTF8, 0, root, -1, wroot, MAX_PATH + 64);
    remove_tree(wroot);
    const char *err = NULL;
    char dest[MAX_PATH + 80];
    snprintf(dest, sizeof dest, "%s/dest", root);
    CHECK(!shsm_tar_extract(tar, tn, dest, NULL, &err));
    CHECK(err && strstr(err, "outside the destination"));
    /* the member before it was extracted, as tarfile does; the evil one went nowhere */
    char ok[MAX_PATH + 100], evil[MAX_PATH + 100];
    snprintf(ok, sizeof ok, "%s/ok.txt", dest);
    snprintf(evil, sizeof evil, "%s/evil.txt", root);
    wchar_t w[MAX_PATH + 100];
    MultiByteToWideChar(CP_UTF8, 0, ok, -1, w, MAX_PATH + 100);
    CHECK(GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES);
    MultiByteToWideChar(CP_UTF8, 0, evil, -1, w, MAX_PATH + 100);
    CHECK(GetFileAttributesW(w) == INVALID_FILE_ATTRIBUTES);
    remove_tree(wroot);
    avp_arena_free(a);
}

/* ---- the tools' selective extraction (merge_lane.py extract_git_tree, neutral.py materialise_rev) ---- */

static bool select_prefix(void *ctx, const char *name) {
    const char *prefix = (const char *)ctx;
    return strncmp(name, prefix, strlen(prefix)) == 0;
}

static void temp_root(char *root, size_t cap, wchar_t *wroot, const char *tag) {
    char tmp[MAX_PATH];
    GetTempPathA(sizeof tmp, tmp);
    snprintf(root, cap, "%sshsm_archive_%s_%lu", tmp, tag, GetCurrentProcessId());
    MultiByteToWideChar(CP_UTF8, 0, root, -1, wroot, MAX_PATH + 64);
    remove_tree(wroot);
}

AVP_TEST(extract_files_of_a_git_archive_like_neutral_py) {
    AvpArena *a = avp_arena_new(0);
    size_t n = 0;
    u8 *tar = git_archive(a, k_git_commit, k_git_path, &n);
    if (!tar) {
        printf("(git archive not available: skipped)\n");
        avp_arena_free(a);
        return;
    }
    CHECK_EQ_U64(n, k_git_tar_len);
    char root[MAX_PATH + 64];
    wchar_t wroot[MAX_PATH + 64];
    temp_root(root, sizeof root, wroot, "git");
    char dest[MAX_PATH + 80];
    snprintf(dest, sizeof dest, "%s/tree-0123456789abcdef", root);
    size_t written = 0;
    const char *err = NULL;
    CHECK_MSG(shsm_tar_extract_files(tar, n, dest, NULL, NULL, &written, &err), "extract: %s", err ? err : "?");
    size_t files = 0;
    for (size_t i = 0; i < AVP_ARRAY_LEN(k_rows_git); i++) {
        const TarRow *w = &k_rows_git[i];
        if (strcmp(w->kind, "FILE") != 0) continue;
        files++;
        char path[2 * MAX_PATH];
        snprintf(path, sizeof path, "%s/%s", dest, w->name);
        CHECK_CZ(avp_file_digest_hex(a, AVP_HASH_SHA256, path), w->sha256);
    }
    CHECK_EQ_U64(written, files);
    /* again over the existing tree: the files are overwritten */
    CHECK(shsm_tar_extract_files(tar, n, dest, NULL, NULL, &written, &err));
    CHECK_EQ_U64(written, files);
    remove_tree(wroot);
    avp_arena_free(a);
}

AVP_TEST(extract_files_selects_regular_files_like_merge_lane_py) {
    AvpArena *a = avp_arena_new(0);
    const TarCase *c = &k_tar_cases[0]; /* pax: files, folders, links, a long name, a non-ASCII name */
    size_t tn;
    u8 *tar = shsm_zlib_decompress(a, c->z, c->zn, 15, c->tar_n, &tn, NULL);
    REQUIRE(tar != NULL);
    char root[MAX_PATH + 64];
    wchar_t wroot[MAX_PATH + 64];
    temp_root(root, sizeof root, wroot, "sel");
    const char *err = NULL;
    size_t written = 0;
    char dest[MAX_PATH + 80];
    snprintf(dest, sizeof dest, "%s/d", root);
    CHECK_MSG(shsm_tar_extract_files(tar, tn, dest, select_prefix, (void *)"dir/", &written, &err), "%s", err ? err : "?");
    size_t want = 0;
    for (size_t i = 0; i < c->nrows; i++) {
        const TarRow *w = &c->rows[i];
        char path[2 * MAX_PATH];
        snprintf(path, sizeof path, "%s/%s", dest, w->name);
        wchar_t wp[2 * MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 2 * MAX_PATH);
        DWORD attr = GetFileAttributesW(wp);
        bool sel = strcmp(w->kind, "FILE") == 0 && strncmp(w->name, "dir/", 4) == 0;
        if (sel) {
            want++;
            CHECK_CZ(avp_file_digest_hex(a, AVP_HASH_SHA256, path), w->sha256);
        } else if (strcmp(w->kind, "DIR") != 0 && strlen(path) < 250) {
            CHECK_MSG(attr == INVALID_FILE_ATTRIBUTES, "%s was not selected but exists", w->name);
        }
    }
    CHECK_EQ_U64(written, want);
    remove_tree(wroot);
    avp_arena_free(a);
}

AVP_TEST(extract_files_refuses_paths_outside_the_folder) {
    AvpArena *a = avp_arena_new(0);
    size_t tn;
    u8 *tar = shsm_zlib_decompress(a, k_tar_evil, sizeof k_tar_evil, 15, 0, &tn, NULL);
    REQUIRE(tar != NULL);
    char root[MAX_PATH + 64];
    wchar_t wroot[MAX_PATH + 64];
    temp_root(root, sizeof root, wroot, "evil2");
    char dest[MAX_PATH + 80];
    snprintf(dest, sizeof dest, "%s/dest", root);
    const char *err = NULL;
    size_t written = 9;
    CHECK(!shsm_tar_extract_files(tar, tn, dest, NULL, NULL, &written, &err));
    CHECK(err && strstr(err, "outside the destination"));
    CHECK_EQ_U64(written, 1); /* ok.txt, before the bad member */
    char evil[MAX_PATH + 100];
    snprintf(evil, sizeof evil, "%s/evil.txt", root);
    wchar_t w[MAX_PATH + 100];
    MultiByteToWideChar(CP_UTF8, 0, evil, -1, w, MAX_PATH + 100);
    CHECK(GetFileAttributesW(w) == INVALID_FILE_ATTRIBUTES);
    remove_tree(wroot);
    avp_arena_free(a);
}

AVP_TEST_MAIN("shsm_archive")
