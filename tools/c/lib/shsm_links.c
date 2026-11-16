/* NTFS junctions and symbolic links (tools/slot.py: is_link, link_target, make_junction, remove_link, scan_links).
 * Reference for the link-first teardown rule: lib/worktree.c of another project of the author's at commit
 * 49e811503d7ba5c9cff92160557df057c50a8ab3 (is_link, remove_link); the code here is written against core's fs.h. */
#include "shsm_links.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_NOT_A_REPARSE_POINT_ 4390u /* winerror of "The file or directory is not a reparse point" */

const char *shsm_link_kind_name(ShsmLinkKind kind) {
    switch (kind) {
    case SHSM_LINK_NONE: return "none";
    case SHSM_LINK_JUNCTION: return "junction";
    case SHSM_LINK_SYMLINK_FILE: return "symlink-file";
    case SHSM_LINK_SYMLINK_DIR: return "symlink-dir";
    case SHSM_LINK_OTHER: return "other";
    }
    return "other";
}

ShsmLinkKind shsm_link_kind_from_attrs(u32 file_attributes, u32 reparse_tag) {
    if (!(file_attributes & AVP_FILE_ATTRIBUTE_REPARSE_POINT)) return SHSM_LINK_NONE;
    if (reparse_tag == AVP_IO_REPARSE_TAG_MOUNT_POINT) return SHSM_LINK_JUNCTION;
    if (reparse_tag == AVP_IO_REPARSE_TAG_SYMLINK)
        return (file_attributes & AVP_FILE_ATTRIBUTE_DIRECTORY) ? SHSM_LINK_SYMLINK_DIR : SHSM_LINK_SYMLINK_FILE;
    return SHSM_LINK_OTHER;
}

bool shsm_link_stat(const char *path, ShsmLinkKind *kind) {
    AvpStat st;
    *kind = SHSM_LINK_NONE;
    if (!avp_lstat(path, &st)) return false;
    *kind = shsm_link_kind_from_attrs(st.file_attributes, st.reparse_tag);
    return true;
}

ShsmLinkKind shsm_link_kind(const char *path) {
    ShsmLinkKind k;
    shsm_link_stat(path, &k);
    return k;
}

char *shsm_link_target(AvpArena *a, const char *path) {
    ShsmLinkKind k;
    if (!shsm_link_stat(path, &k)) return NULL;
    if (k == SHSM_LINK_NONE) {
        avp_io_fail_win(ERROR_NOT_A_REPARSE_POINT_, path, NULL);
        return NULL;
    }
    char *t = NULL;
    if (!avp_readlink(a, path, &t)) return NULL;
    if (strncmp(t, "\\\\?\\UNC\\", 8) == 0) return avp_sprintf(a, "\\\\%s", t + 8);
    if (strncmp(t, "\\\\?\\", 4) == 0 || strncmp(t, "\\??\\", 4) == 0) return t + 4;
    return t;
}

/* The shared refusal and parent-folder step of both creators. */
static bool prepare_link_path(const char *link) {
    if (avp_lexists(link)) {
        avp_io_fail_errno(EEXIST, link, NULL);
        return false;
    }
    AvpArena *a = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(a);
    AvpStr dir = avp_path_dirname(link);
    bool ok = true;
    if (dir.n) ok = avp_makedirs(avp_cz(a, dir), true);
    avp_arena_reset(a, mk);
    return ok;
}

bool shsm_link_create_junction(const char *link, const char *target) {
    if (!prepare_link_path(link)) return false;
    AvpArena *a = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(a);
    bool ok = avp_junction_create(link, avp_path_abspath(a, target));
    avp_arena_reset(a, mk);
    return ok;
}

bool shsm_link_create_symlink(const char *link, const char *target, bool is_dir) {
    if (!prepare_link_path(link)) return false;
    return avp_symlink(target, link, is_dir);
}

bool shsm_link_remove(const char *path) {
    AvpStat st;
    if (!avp_lstat(path, &st)) return false;
    if (!(st.file_attributes & AVP_FILE_ATTRIBUTE_REPARSE_POINT)) {
        avp_io_fail_win(ERROR_NOT_A_REPARSE_POINT_, path, NULL);
        return false;
    }
    if (st.file_attributes & AVP_FILE_ATTRIBUTE_DIRECTORY) return avp_rmdir(path);
    return avp_remove(path);
}

/* ---- walk ------------------------------------------------------------------------------------------------------- */

static int cmp_entries(const void *x, const void *y) {
    const AvpDirEntry *p = x, *q = y;
    size_t n = p->name.n < q->name.n ? p->name.n : q->name.n;
    int c = n ? memcmp(p->name.p, q->name.p, n) : 0; /* UTF-8 byte order is code point order */
    if (c) return c;
    return p->name.n < q->name.n ? -1 : p->name.n > q->name.n ? 1 : 0;
}

static bool walk_dir(AvpArena *a, ShsmLinkVec *out, const char *dir, const char *rel_prefix) {
    AvpDirEntries it = {0};
    if (!avp_scandir(a, dir, &it)) return false; /* the caller skips an unreadable subfolder */
    if (it.n > 1) qsort(it.v, it.n, sizeof it.v[0], cmp_entries);
    for (size_t i = 0; i < it.n; i++) {
        const AvpDirEntry *e = &it.v[i];
        const char *rel = avp_sprintf(a, "%s%.*s", rel_prefix, AVP_SF(e->name));
        ShsmLinkKind k = shsm_link_kind_from_attrs(e->attrs, e->reparse_tag);
        if (k != SHSM_LINK_NONE) {
            ShsmLink l = {rel, k, shsm_link_target(a, e->path.p)};
            avp_vec_push(out, l);
        } else if (e->attrs & AVP_FILE_ATTRIBUTE_DIRECTORY) {
            walk_dir(a, out, e->path.p, avp_sprintf(a, "%s/", rel));
        }
    }
    avp_vec_free(&it);
    return true;
}

bool shsm_links_walk(AvpArena *a, const char *root, ShsmLinkVec *out) {
    AvpStat st;
    if (!avp_lstat(root, &st)) return false;
    ShsmLinkKind k = shsm_link_kind_from_attrs(st.file_attributes, st.reparse_tag);
    if (k != SHSM_LINK_NONE) {
        ShsmLink l = {".", k, shsm_link_target(a, root)};
        avp_vec_push(out, l);
        return true;
    }
    return walk_dir(a, out, root, "");
}
