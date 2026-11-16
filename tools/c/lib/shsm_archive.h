/* Archives for the tools: the tar stream of `git archive` (replaces tarfile as merge_lane.py's extract_git_tree and
 * neutral.py's materialise_rev use it: tarfile.open(fileobj=BytesIO(data)), the regular files of a selection written
 * under a folder). No tool writes tar or zip archives; download_tool.py's zipfile.extractall of downloaded
 * toolchains has no C counterpart yet. Taken from another project of the author's tools/c/core/include/avp/zip.h (its tar part) at
 * commit 49e811503d7ba5c9cff92160557df057c50a8ab3, names avp_ -> shsm_; shsm_tar_extract_files added.
 * Paths are UTF-8; memory as stated per function. */
#pragma once
#include "avp/str.h"

/* ================================================================================================================
 * tar (git archive output)
 * ================================================================================================================
 * A reader over a whole tar held in memory (a `git archive` result, or a mapped file). It yields the members
 * tarfile.open(fileobj=BytesIO(data)) would yield, with the same TarInfo.name: ustar (prefix + "/" + name), pax
 * extended headers (path, linkpath, size, mtime; the global pax header git writes is read and not yielded), GNU
 * long names (L, K), a trailing "/" removed from directory names (every slash of a pax path, one of a GNU long name,
 * as tarfile does), base-256 numeric fields, the header checksum checked. Reading stops at the first all-zero block
 * or at the end of the data (as tarfile does). Errors follow tarfile too: a bad, truncated or empty first header, or
 * a bad header right after an extended header, is an error (-1); a bad or truncated header later in the archive just
 * ends the listing (0, with r->err telling why); member data that runs past the end of the buffer is an error (-1,
 * reported at that member: tarfile yields the member and raises at the next step, so a caller may see one member
 * fewer than Python's loop before the error).
 *
 * Member kinds: regular files (types '0', '\0', '7'), directories ('5'), symbolic links ('2'), hard links ('1') and
 * anything else as SHSM_TAR_OTHER (device nodes, fifos: tarfile yields them too). Data pointers point into the
 * caller's buffer (no copy). Names and link names are NUL-terminated strings from the arena given to
 * shsm_tar_next. */
typedef enum { SHSM_TAR_FILE, SHSM_TAR_DIR, SHSM_TAR_SYMLINK, SHSM_TAR_HARDLINK, SHSM_TAR_OTHER } ShsmTarKind;
typedef struct {
    const char *name;     /* TarInfo.name: UTF-8 bytes as stored, "/" separators, directories without a trailing "/" */
    const char *linkname; /* symbolic / hard links; "" otherwise */
    ShsmTarKind kind;
    char type;            /* the raw typeflag byte ('0' for the old NUL regular file) */
    u64 size;             /* regular files: the data length (0 for the other kinds) */
    const u8 *data;       /* regular files: points into the tar buffer */
    u32 mode;
    i64 mtime;            /* seconds since the epoch (a pax mtime with a fraction is truncated) */
} ShsmTarMember;
typedef struct {
    const u8 *p;
    size_t n;
    size_t pos;
    bool done;
    const char *err; /* why the last shsm_tar_next returned -1, or 0 on a bad header that ended the listing */
    /* A pax global header (type g) is applied to the member right after it, as tarfile does; its values are not kept
     * for later members (git writes only a "comment" there). */
} ShsmTarReader;
void shsm_tar_open(ShsmTarReader *r, const void *p, size_t n);
/* 1: *m filled; 0: end of the archive; -1: malformed (r->err says why: "bad checksum", "truncated header",
 * "unexpected end of data", "invalid header"). */
int shsm_tar_next(ShsmTarReader *r, AvpArena *a, ShsmTarMember *m);

/* TarFile.extractall(folder, filter="data") for the members the tools extract (Python 3.14's default filter): the
 * folder is created; directories and regular files are made under it (parent folders as needed, existing files
 * overwritten, modification times set from the archive as tarfile does for files and directories); a name that is
 * absolute or leaves the folder is an error ("... is outside the destination"); symbolic and hard links and special
 * files are not supported on Windows hosts without privileges: they are skipped and counted in *skipped (NULL ok).
 * Paths are UTF-8. False with *err set on the first failure (members before it are already extracted, as Python). */
bool shsm_tar_extract(const void *p, size_t n, const char *folder, size_t *skipped, const char **err);

/* The tools' own loop: `for m in tf.getmembers(): if m.isfile() and select(m.name): write tf.extractfile(m).read() to
 * os.path.join(folder, m.name)` (parent folders made as needed, existing files overwritten, the data written as is,
 * modification times left at the time of writing, as the Python's open(.., "wb").write leaves them). select NULL
 * takes every regular file; other member kinds are passed over (not counted). Unlike the Python loop, which joins
 * the name unchecked, a name that is absolute or climbs out of the folder is refused (an error, "... would be
 * extracted outside the destination"; git never writes one). *written (NULL ok) counts the files written. False
 * with *err (thread-local text, valid until the next call) on the first failure; files before it stay written. */
typedef bool (*ShsmTarSelect)(void *ctx, const char *name);
bool shsm_tar_extract_files(const void *p, size_t n, const char *folder, ShsmTarSelect select, void *ctx,
                            size_t *written, const char **err);
