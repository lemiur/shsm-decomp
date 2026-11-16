/* NTFS junctions and symbolic links (tools/slot.py: is_link, link_target, make_junction, remove_link, scan_links).
 * Built on core's fs.h (avp_lstat, avp_readlink, avp_junction_create, avp_symlink, avp_rmdir, avp_remove,
 * avp_scandir); nothing here follows a link. API notes in shsm_links.md.
 *
 * Errors: a function that can fail returns false (NULL for a string) and leaves an OSError in the thread's fs.h
 * record (avp_io_err); `shsm_raise_io()` (shsm_cli.h) re-raises it as the Python exception. A refusal is recorded
 * the same way (EEXIST for a link path that exists, winerror 4390 "not a reparse point" for removing a non-link). */
#pragma once
#include "avp/fs.h"

typedef enum {
    SHSM_LINK_NONE = 0,     /* not a reparse point (or the path cannot be examined) */
    SHSM_LINK_JUNCTION,     /* IO_REPARSE_TAG_MOUNT_POINT */
    SHSM_LINK_SYMLINK_FILE, /* IO_REPARSE_TAG_SYMLINK, a file link */
    SHSM_LINK_SYMLINK_DIR,  /* IO_REPARSE_TAG_SYMLINK, a directory link */
    SHSM_LINK_OTHER         /* any other reparse tag (cloud placeholder, WSL, dedup, ...) */
} ShsmLinkKind;

/* "none", "junction", "symlink-file", "symlink-dir", "other". */
const char *shsm_link_kind_name(ShsmLinkKind kind);

/* The kind of a directory entry or stat result: its dwFileAttributes and reparse tag (0 when none). */
ShsmLinkKind shsm_link_kind_from_attrs(u32 file_attributes, u32 reparse_tag);

/* lstat-style, never follows. false with the OSError when the path cannot be examined (missing, access denied);
 * then *kind is SHSM_LINK_NONE. */
bool shsm_link_stat(const char *path, ShsmLinkKind *kind);
/* The kind, SHSM_LINK_NONE for an ordinary file or folder and for any path that cannot be examined
 * (slot.is_link is shsm_link_kind(path) != SHSM_LINK_NONE). */
ShsmLinkKind shsm_link_kind(const char *path);

/* The target of a link as text, allocated from `a`, or NULL (os.readlink failed or the path is not a link; the
 * OSError is recorded). Junction: the substitute name with the \??\ prefix stripped ("C:\x\y"). Symbolic link: the
 * substitute name as os.readlink gives it, with a leading \\?\ (\\?\UNC\ becomes \\) stripped; a relative symbolic
 * link target is returned as stored (relative to the link's folder). Never follows the link. */
char *shsm_link_target(AvpArena *a, const char *path);

/* Creates the directory junction `link` -> `target` (make_junction): refuses, with EEXIST and nothing changed, when
 * `link` exists (a dangling link counts); makes the target absolute against the current directory; creates the
 * link's missing parent folders. The target need not exist. */
bool shsm_link_create_junction(const char *link, const char *target);
/* Creates a symbolic link (directory link when is_dir). The target is stored as given (relative targets are
 * relative to the link's folder). Same refusal and parent folders; false when the process may not create symbolic
 * links (no privilege / developer mode: ERROR_PRIVILEGE_NOT_HELD). */
bool shsm_link_create_symlink(const char *link, const char *target, bool is_dir);

/* Removes the link itself, never what it points to (remove_link): refuses, with nothing deleted, a path that is not
 * a reparse point (winerror 4390) or does not exist. A reparse point with the directory attribute (junction,
 * directory symbolic link, dangling ones too) goes through RemoveDirectoryW on the link, which cannot remove a
 * non-empty real folder and never enters the target; any other reparse point goes through DeleteFileW. */
bool shsm_link_remove(const char *path);

/* One reparse point found by the walk. */
typedef struct {
    const char *rel;      /* path relative to the walk root, forward slashes ("." when the root itself is the link) */
    ShsmLinkKind kind;
    const char *target;   /* shsm_link_target, or NULL when it cannot be read */
} ShsmLink;
typedef AVP_VEC(ShsmLink) ShsmLinkVec;

/* Every reparse point under root (scan_links), without descending through any of them. Ordinary folders are
 * descended; within a folder the entries are visited in order of name (code point order, as sorted(os.scandir)),
 * each folder's contents directly after its own name. A root that is itself a link gives that one entry with rel
 * ".". false when root cannot be listed; a subfolder that cannot be listed is skipped. `out` is a heap vector (free
 * with avp_vec_free); the strings live in `a`. */
bool shsm_links_walk(AvpArena *a, const char *root, ShsmLinkVec *out);
