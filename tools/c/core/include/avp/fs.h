/* Files, paths and directory walking with CPython 3.14-on-Windows behaviour: open() and its text modes, the os,
 * os.path (ntpath), glob, fnmatch, shutil and tempfile functions the tools call, returning what Python returns and
 * failing the way Python fails.
 *
 * Paths are UTF-8, NUL-terminated (`const char *`), with / or \ as separators, and are passed to Win32 as UTF-16
 * (a \\?\ prefix is added by the module when a path would exceed MAX_PATH, so long paths work as they do for Python).
 * Strings the module returns are allocated from the arena given; vectors (AvpStrVec, ...) are heap vectors whose
 * strings live in that arena, and every AvpStr in them is NUL-terminated (p[n] == 0) so `.p` is a C string.
 *
 * ---- errors ------------------------------------------------------------------------------------------------------
 * A function that can fail returns false (or NULL, or -1 for a count) and records the failure in a thread-local
 * AvpIoErr, which describes the exception Python would have raised: its class, errno, winerror and filename(s).
 * avp_io_err_str() is str(exception), e.g.
 *     [Errno 2] No such file or directory: 'x.txt'                       open()
 *     [WinError 2] The system cannot find the file specified: 'x.txt'    os.remove, os.stat, os.rename, ...
 *     [WinError 2] The system cannot find the file specified: 'a' -> 'b' os.replace, os.rename
 * (open() reports CRT errno values with strerror texts; the os.* functions report the Win32 error with the system's
 * FormatMessage text; both quote the path with repr(), so backslashes are doubled). The record of the last failure is
 * valid until the same thread's next failing call in this module; successful calls leave it alone. Python
 * "except FileNotFoundError" is avp_io_is(ENOENT); "except OSError" is any failure of kind AVP_IOE_OS.
 *
 * ---- text ------------------------------------------------------------------------------------------------------
 * open(path, encoding=E, errors=R, newline=N) is the AvpTextMode {E, R, N}: reading decodes the bytes and translates
 * newlines the way TextIOWrapper does, writing does the reverse. Text inside the program is always UTF-8.
 *   newline=None  (AVP_NL_DEFAULT): reading turns \r\n and \r into \n; writing turns \n into the platform's \r\n
 *   newline=""    (AVP_NL_EMPTY):   reading splits lines at \n, \r and \r\n but does not translate; writing as is
 *   newline="\n"  (AVP_NL_LF):      reading splits at \n only; writing as is
 *   newline="\r" / "\r\n"           (AVP_NL_CR / AVP_NL_CRLF): reading splits at that only; writing turns \n into it
 * Encodings: utf-8, utf-8-sig (reading drops one leading BOM, writing puts one at the start of the file), latin-1,
 * ascii and mbcs (the ANSI code page). Errors: strict (a bad byte fails with kind AVP_IOE_DECODE and Python's
 * UnicodeDecodeError text) or replace (U+FFFD, one per maximal invalid subpart exactly as CPython's utf-8 decoder).
 *
 * Environment variable names are case-insensitive as os.environ is on Windows (keys are upper-cased when listed).
 *
 * Deviations from Python (also in the report of the worker): temporary names are random (D1); the order of
 * pathlib's glob/rglob is not reproduced (callers sort, as the tools do); UnicodeDecodeError positions are those of
 * a whole-file read (a `for line in f` loop in Python reports them relative to its 8 KiB chunk).
 */
#pragma once
#include "arena.h"
#include "str.h"
#include "vec.h"

/* ---- errors ----------------------------------------------------------------------------------------------------- */
enum {
    AVP_IOE_NONE = 0,
    AVP_IOE_OS,       /* OSError and its subclasses (FileNotFoundError, PermissionError, ...) */
    AVP_IOE_DECODE,   /* UnicodeDecodeError */
    AVP_IOE_ENCODE,   /* UnicodeEncodeError */
    AVP_IOE_VALUE,    /* ValueError: "embedded null character", relpath's "no path specified", commonpath, ... */
    AVP_IOE_SHUTIL    /* shutil.Error / SameFileError / SpecialFileError (msg holds str(exception)) */
};
typedef struct AvpIoErr {
    int kind;            /* AVP_IOE_* */
    int err;             /* OSError.errno (the CRT <errno.h> numbers, as Python's errno module on Windows) */
    int winerror;        /* OSError.winerror; 0 when Python's text has the "[Errno n]" form */
    char path[1040];     /* OSError.filename ("" when none), UTF-8, as passed (longer names are cut) */
    char path2[1040];    /* OSError.filename2 */
    char msg[512];       /* the full str(exception) of non-OSError kinds */
} AvpIoErr;

const AvpIoErr *avp_io_err(void);               /* this thread's last failure */
bool avp_io_is(int err);                        /* last failure is an OSError with this errno (ENOENT, EEXIST, EACCES, ...) */
const char *avp_io_err_class(void);             /* type(exc).__name__: "FileNotFoundError", "PermissionError", "OSError", ... */
AvpStr avp_io_err_str(AvpArena *a);             /* str(exc) */
AvpStr avp_io_err_repr_path(AvpArena *a, const char *s); /* repr(str) as Python prints it ('it''s' -> "it's") */
void avp_io_clear(void);
/* For other modules that raise an OSError the way Python does (the fields are copied into the thread's record). */
void avp_io_fail_errno(int err, const char *path, const char *path2);     /* [Errno n] text: 'path' */
void avp_io_fail_win(unsigned winerror, const char *path, const char *path2); /* [WinError n] text: 'path' */
void avp_io_fail_msg(int kind, int err, const char *fmt, ...);            /* a preformatted text */

/* ---- text modes -------------------------------------------------------------------------------------------------- */
typedef enum { AVP_ENC_UTF8, AVP_ENC_UTF8_SIG, AVP_ENC_LATIN1, AVP_ENC_ASCII, AVP_ENC_MBCS } AvpEncoding;
typedef enum { AVP_ERR_STRICT, AVP_ERR_REPLACE } AvpErrorsMode;
typedef enum { AVP_NL_DEFAULT, AVP_NL_EMPTY, AVP_NL_LF, AVP_NL_CR, AVP_NL_CRLF } AvpNewline;
typedef struct {
    AvpEncoding enc;
    AvpErrorsMode errors;
    AvpNewline newline;
} AvpTextMode;
#define AVP_TEXT(e, r, n) ((AvpTextMode){(e), (r), (n)})
#define AVP_TM_UTF8 AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT)          /* open(p, encoding="utf-8") */
#define AVP_TM_UTF8_NL_EMPTY AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_EMPTY)   /* ..., newline="" */
#define AVP_TM_UTF8_NL_LF AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_LF)         /* ..., newline="\n" */
#define AVP_TM_UTF8_REPLACE AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_DEFAULT) /* ..., errors="replace" */
#define AVP_TM_UTF8_SIG AVP_TEXT(AVP_ENC_UTF8_SIG, AVP_ERR_STRICT, AVP_NL_DEFAULT)
#define AVP_TM_UTF8_SIG_REPLACE AVP_TEXT(AVP_ENC_UTF8_SIG, AVP_ERR_REPLACE, AVP_NL_DEFAULT)
#define AVP_TM_LATIN1 AVP_TEXT(AVP_ENC_LATIN1, AVP_ERR_STRICT, AVP_NL_DEFAULT)

/* bytes.decode() plus TextIOWrapper's newline handling, over a whole file's bytes. false: kind AVP_IOE_DECODE. */
bool avp_decode_text(AvpArena *a, AvpStr bytes, AvpTextMode m, AvpStr *out);
/* str.encode() plus the writer's newline translation (what open(p, "w", ...).write(text) puts in the file).
 * false: kind AVP_IOE_ENCODE (a character the encoding cannot hold). The result is allocated from `a`. */
bool avp_encode_text(AvpArena *a, AvpStr text, AvpTextMode m, AvpStr *out);

/* `for line in f` over decoded text: lines keep their terminator; the last one may have none. In AVP_NL_DEFAULT
 * mode the text was already translated, so lines end at \n. */
typedef struct {
    AvpStr rest;
    AvpNewline newline;
} AvpFileLines;
void avp_file_lines_init(AvpFileLines *it, AvpStr text, AvpNewline newline);
bool avp_file_lines_next(AvpFileLines *it, AvpStr *line);

/* ---- whole files ------------------------------------------------------------------------------------------------ */
/* A read-only view of a whole file: memory-mapped above 64 KiB, read into memory below. An empty file gives
 * p != NULL and n == 0. file/mapping are the Win32 handles (NULL for a file read into memory). */
typedef struct {
    const u8 *p;
    size_t n;
    void *file, *mapping;
} AvpFileView;
bool avp_view_open(const char *path, AvpFileView *v);
void avp_view_close(AvpFileView *v);
/* open(path, "rb").read() into `a` (a == NULL: heap memory, release with avp_vec_free). */
bool avp_read_all(AvpArena *a, const char *path, AvpBytes *out);
/* open(path, "r", encoding=..., errors=..., newline=...).read(): the text as UTF-8 in `a`. */
bool avp_read_text(AvpArena *a, const char *path, AvpTextMode m, AvpStr *out);
/* list(open(path, ...)): the lines (terminators kept) as NUL-terminated strings of `a`, in a heap vector. */
bool avp_read_lines(AvpArena *a, const char *path, AvpTextMode m, AvpStrVec *out);
/* Path.read_bytes()/read_text() are the same calls. */

/* open(path, "wb").write(p) / open(path, "w", ...).write(text) / open(path, "a", ...).write(text) */
bool avp_write_file(const char *path, const void *p, size_t n);
bool avp_write_text(const char *path, AvpStr text, AvpTextMode m);
bool avp_append_text(const char *path, AvpStr text, AvpTextMode m);
bool avp_append_file(const char *path, const void *p, size_t n);
/* A temporary file in the same folder, then MoveFileExW(MOVEFILE_REPLACE_EXISTING): a reader never sees half a file.
 * The temporary name is path + ".<pid>.<tid>.<n>.tmp" (D1). */
bool avp_write_atomic(const char *path, const void *p, size_t n);
bool avp_write_text_atomic(const char *path, AvpStr text, AvpTextMode m);

/* ---- open files (a buffered handle: open(), .read/.write/.seek/.flush, os.fsync, close) ---------------------------
 * mode as Python's open(): "r" "rb" "w" "wb" "a" "ab" "x" "xb" "r+" "r+b" "w+" "w+b" "a+" "a+b". In a text mode ("w", "a",
 * "x", "w+", "a+") avp_fwrite_text encodes and translates newlines per `tm`; avp_fwrite always writes the bytes as
 * given. Reading through an AvpFile is binary (read whole text files with avp_read_text). */
typedef struct AvpFile AvpFile;
AvpFile *avp_fopen(const char *path, const char *mode, AvpTextMode tm);
bool avp_fwrite(AvpFile *f, const void *p, size_t n);
bool avp_fwrite_text(AvpFile *f, AvpStr text);
bool avp_fprintf(AvpFile *f, AVP_PRINTF_FMT const char *fmt, ...); /* C printf rules, then avp_fwrite_text */
i64 avp_fread(AvpFile *f, void *p, size_t n);                      /* bytes read (0 at the end), -1 on error */
bool avp_fseek(AvpFile *f, i64 offset, int whence);                /* whence: 0 start, 1 current, 2 end */
i64 avp_ftell(AvpFile *f);
i64 avp_fsize(AvpFile *f);
bool avp_ftruncate(AvpFile *f, i64 size);
bool avp_fflush(AvpFile *f);
bool avp_fsync(AvpFile *f);                                        /* flush, then FlushFileBuffers (os.fsync) */
void *avp_fhandle(AvpFile *f);                                     /* the Win32 HANDLE (for byte-range locks in win.h) */
bool avp_fclose(AvpFile *f);                                       /* false when the final flush failed */

/* ---- os.path (ntpath) -------------------------------------------------------------------------------------------- */
/* Pure string functions, exactly ntpath's results (checked against os.path on thousands of paths). */
char *avp_path_normcase(AvpArena *a, const char *p);   /* LCMapStringEx lower case (invariant locale) and / -> \ */
bool avp_path_isabs(const char *p);                    /* 3.13+: "\x" and "c:x" are not absolute */
char *avp_path_join(AvpArena *a, const char *first, const char *second);
char *avp_path_joinv(AvpArena *a, size_t n, const char *const *parts); /* join(parts[0], parts[1:]...); n >= 1 */
#define avp_join(a, ...) \
    avp_path_joinv((a), sizeof((const char *const[]){__VA_ARGS__}) / sizeof(const char *), (const char *const[]){__VA_ARGS__})
/* splitroot -> (drive, root, tail): views into p. splitdrive -> (drive, rest). */
void avp_path_splitroot(const char *p, AvpStr *drive, AvpStr *root, AvpStr *tail);
void avp_path_splitdrive(const char *p, AvpStr *drive, AvpStr *rest);
void avp_path_split(const char *p, AvpStr *head, AvpStr *tail);   /* head keeps no trailing separator unless it is the root */
void avp_path_splitext(const char *p, AvpStr *root, AvpStr *ext);  /* leading dots of the name are not an extension */
AvpStr avp_path_basename(const char *p);                           /* views into p */
AvpStr avp_path_dirname(const char *p);
char *avp_path_normpath(AvpArena *a, const char *p);
char *avp_path_abspath(AvpArena *a, const char *p);                /* GetFullPathNameW, fallbacks as ntpath */
char *avp_path_realpath(AvpArena *a, const char *p);               /* strict=False; resolves symlinks and junctions */
/* relpath(path, start): false with kind AVP_IOE_VALUE ("no path specified", "path is on mount 'C:', start on mount
 * 'D:'"). start == NULL is the current directory. */
bool avp_path_relpath(AvpArena *a, const char *p, const char *start, char **out);
/* commonpath(paths): false with kind AVP_IOE_VALUE (empty list, "Paths don't have the same drive", "Can't mix
 * absolute and relative paths", "Can't mix rooted and not-rooted paths"). */
bool avp_path_commonpath(AvpArena *a, const char *const *paths, size_t n, char **out);
char *avp_path_expanduser(AvpArena *a, const char *p);             /* ~ via USERPROFILE / HOMEDRIVE+HOMEPATH */
bool avp_path_samefile(const char *a, const char *b);              /* os.path.samefile; false (and an error) when either fails */
/* Backslash form to forward slashes: Path.as_posix(), str.replace("\\", "/"). */
char *avp_path_as_posix(AvpArena *a, const char *p);

/* Queries: exist, isfile, isdir follow links and are false on any error, as in Python. */
bool avp_exists(const char *p);
bool avp_lexists(const char *p);       /* os.path.lexists: a dangling link exists */
bool avp_isfile(const char *p);
bool avp_isdir(const char *p);
bool avp_islink(const char *p);        /* symbolic links only (a junction is not a link for os.path.islink) */
bool avp_isjunction(const char *p);
bool avp_getsize(const char *p, i64 *size);            /* os.path.getsize; false with OSError */
bool avp_getmtime(const char *p, double *seconds);     /* os.path.getmtime (float seconds since 1970) */

/* ---- stat ------------------------------------------------------------------------------------------------------ */
#define AVP_S_IFMT 0170000
#define AVP_S_IFDIR 0040000
#define AVP_S_IFREG 0100000
#define AVP_S_IFLNK 0120000
#define AVP_S_ISDIR(m) (((m) & AVP_S_IFMT) == AVP_S_IFDIR)
#define AVP_S_ISREG(m) (((m) & AVP_S_IFMT) == AVP_S_IFREG)
#define AVP_S_ISLNK(m) (((m) & AVP_S_IFMT) == AVP_S_IFLNK)
#define AVP_FILE_ATTRIBUTE_DIRECTORY 0x10
#define AVP_FILE_ATTRIBUTE_REPARSE_POINT 0x400
#define AVP_IO_REPARSE_TAG_MOUNT_POINT 0xA0000003u
#define AVP_IO_REPARSE_TAG_SYMLINK 0xA000000Cu
typedef struct {
    u32 mode;                       /* st_mode */
    u32 nlink;
    u64 dev, ino;                   /* volume serial number, file index (as CPython reports them) */
    u64 ino_hi;                     /* the high half of a 128-bit file id (ReFS); 0 where ids are 64 bits (NTFS) */
    i64 size;
    i64 atime_ns, mtime_ns, ctime_ns; /* st_*time_ns (ctime is the creation time on Windows) */
    double atime, mtime, ctime;     /* st_*time: sec + nsec * 1e-9 computed as CPython does */
    u32 file_attributes;            /* st_file_attributes */
    u32 reparse_tag;                /* st_reparse_tag (0 when not a reparse point) */
} AvpStat;
bool avp_stat(const char *p, AvpStat *st);   /* os.stat: follows links */
bool avp_lstat(const char *p, AvpStat *st);  /* os.lstat */
bool avp_fstat(AvpFile *f, AvpStat *st);     /* os.fstat */

/* ---- os: files, folders, environment ---------------------------------------------------------------------------- */
bool avp_mkdir(const char *p);
bool avp_makedirs(const char *p, bool exist_ok);   /* os.makedirs(p, exist_ok=...) incl. the error cases */
bool avp_remove(const char *p);                    /* os.remove / os.unlink (a directory link is removed with RemoveDirectory) */
bool avp_rmdir(const char *p);
bool avp_rename(const char *src, const char *dst); /* fails when dst exists */
bool avp_replace(const char *src, const char *dst);/* MoveFileExW(MOVEFILE_REPLACE_EXISTING) */
char *avp_getcwd(AvpArena *a);
bool avp_chdir(const char *p);
/* os.utime(p, None) sets both times to now; with times: (atime_ns, mtime_ns). */
bool avp_utime_now(const char *p);
bool avp_utime_ns(const char *p, i64 atime_ns, i64 mtime_ns);
/* os.chmod(p, mode): on Windows only the S_IWRITE bit counts (clear = read-only attribute). */
bool avp_chmod(const char *p, int mode);
bool avp_symlink(const char *target, const char *link, bool target_is_dir);
bool avp_junction_create(const char *link, const char *target);   /* mklink /J */
bool avp_link(const char *existing, const char *newlink);          /* os.link (hard link) */
bool avp_readlink(AvpArena *a, const char *p, char **out);         /* os.readlink, with the \\?\ prefix as Python gives it */

/* Environment: getenv returns NULL when unset; the value is allocated from `a`. */
char *avp_getenv(AvpArena *a, const char *name);
const char *avp_getenv_or(AvpArena *a, const char *name, const char *dflt);
bool avp_setenv(const char *name, const char *value);   /* os.environ[name] = value (also for child processes) */
bool avp_unsetenv(const char *name);
typedef struct {
    AvpStr name;    /* upper-cased as os.environ's keys are */
    AvpStr value;
} AvpEnvEntry;
typedef AVP_VEC(AvpEnvEntry) AvpEnvVec;
AvpEnvVec avp_environ(AvpArena *a);                      /* dict(os.environ), in the environment block's order */

/* ---- directories ------------------------------------------------------------------------------------------------ */
typedef struct {
    AvpStr name;          /* DirEntry.name */
    AvpStr path;          /* DirEntry.path: join(scandir argument, name) */
    u32 attrs;            /* dwFileAttributes */
    u32 reparse_tag;      /* the reparse tag when the entry is a reparse point, else 0 */
    bool is_dir;          /* DirEntry.is_dir(): follows symlinks and junctions (false when the target is gone) */
    bool is_file;         /* DirEntry.is_file() */
    bool is_symlink;      /* DirEntry.is_symlink(): symbolic links only */
    bool is_junction;     /* DirEntry.is_junction() */
    i64 size;             /* DirEntry.stat(follow_symlinks=False) data from the listing itself */
    i64 atime_ns, mtime_ns, ctime_ns;
} AvpDirEntry;
typedef AVP_VEC(AvpDirEntry) AvpDirEntries;
/* list(os.scandir(p)): the entries in the order the file system returns them (what FindFirstFile gives, "." and ".."
 * left out). An empty p lists the current directory. */
bool avp_scandir(AvpArena *a, const char *p, AvpDirEntries *out);
/* os.listdir(p): the names in the same order. */
bool avp_listdir(AvpArena *a, const char *p, AvpStrVec *out);

/* os.walk(top, topdown, followlinks=False). Each avp_walk_next yields (dirpath, dirnames, filenames); topdown:
 * the caller may drop entries from item.dirs (compact the vector in place and lower n) to prune the walk, as
 * `dirnames[:] = ...` does. A folder that cannot be listed is skipped silently (os.walk's default onerror=None).
 * Strings are valid until the next avp_walk_next or avp_walk_close; the item struct passed in must stay alive (and
 * be the same one) until then, because the walk reads the pruned dirs back from it. */
typedef struct {
    AvpStr dirpath;
    AvpStrVec dirs;
    AvpStrVec files;
} AvpWalkItem;
typedef struct AvpWalk AvpWalk;
AvpWalk *avp_walk_open(const char *top, bool topdown);
bool avp_walk_next(AvpWalk *w, AvpWalkItem *item);
void avp_walk_close(AvpWalk *w);

/* ---- fnmatch and glob -------------------------------------------------------------------------------------------- */
/* fnmatch.fnmatch: both sides normcase'd (case-insensitive on Windows); fnmatchcase: exact. Patterns: * ? [seq]
 * [!seq] with Python 3.14's rules for ranges and for an unclosed [. */
bool avp_fnmatch(const char *name, const char *pat);
bool avp_fnmatchcase(const char *name, const char *pat);
/* fnmatch.filter(names, pat): the names that match, in order (views of the input strings). */
AvpStrVec avp_fnmatch_filter(const AvpStrVec *names, const char *pat);
bool avp_glob_has_magic(const char *s);               /* glob.has_magic: any of * ? [ */
char *avp_glob_escape(AvpArena *a, const char *s);    /* glob.escape */
typedef struct {
    const char *root_dir;   /* glob(root_dir=...): NULL for none */
    bool recursive;         /* recursive=True: "**" matches any folders */
    bool include_hidden;    /* include_hidden=True: wildcards match names starting with '.' */
} AvpGlobOpts;
/* glob.glob(pattern, ...): the list in glob's order (scandir order per level, "**" parent first), the strings with
 * the separators glob.glob produces (those of the pattern, and "\" where it joins). */
AvpStrVec avp_glob_ex(AvpArena *a, const char *pattern, const AvpGlobOpts *opts);
AvpStrVec avp_glob(AvpArena *a, const char *pattern, bool recursive);

/* ---- shutil ---------------------------------------------------------------------------------------------------- */
enum { AVP_RMTREE_IGNORE_ERRORS = 1, AVP_RMTREE_CLEAR_READONLY = 2 };
/* shutil.rmtree(path, ignore_errors=...): does not follow links; a junction or symlink is removed, never its
 * target; the first failure stops it (false, errors as Python) unless IGNORE_ERRORS. AVP_RMTREE_CLEAR_READONLY
 * additionally clears the read-only attribute and retries (the `onexc=chmod` idiom; tempfile's cleanup). */
bool avp_rmtree(const char *path, unsigned flags);
bool avp_copyfile(const char *src, const char *dst);  /* contents only; SameFileError text for the same file */
bool avp_copy2(const char *src, const char *dst);     /* contents and metadata (CopyFile2); dst may be a folder */
bool avp_copystat(const char *src, const char *dst);  /* times and the read-only bit */
typedef struct {
    const char *const *ignore;  /* shutil.ignore_patterns(*ignore): fnmatch patterns on names, NULL for none */
    size_t n_ignore;
    bool dirs_exist_ok;
    bool symlinks;
} AvpCopytreeOpts;
bool avp_copytree(const char *src, const char *dst, const AvpCopytreeOpts *opts);
bool avp_move(const char *src, const char *dst);      /* shutil.move: rename, else copy and delete */
/* shutil.which is avp_which in proc.h. */

/* ---- tempfile -------------------------------------------------------------------------------------------------- */
char *avp_gettempdir(AvpArena *a);                    /* tempfile.gettempdir() (TMP, TEMP, ... then abspath) */
/* mkstemp: creates and opens (mode "w+b", not inherited) a new file prefix + 8 random chars + suffix in dir
 * (NULL: gettempdir()); *path_out receives the absolute path. NULL on failure. */
AvpFile *avp_mkstemp(AvpArena *a, const char *prefix, const char *suffix, const char *dir, char **path_out);
/* mkdtemp: creates the folder, returns its absolute path (NULL on failure). TemporaryDirectory is
 * avp_mkdtemp(a, prefix "tmp", ...) and avp_tempdir_cleanup(path) at the end. */
char *avp_mkdtemp(AvpArena *a, const char *prefix, const char *suffix, const char *dir);
/* TemporaryDirectory.cleanup(): rmtree that clears read-only files and retries; errors are not reported. */
void avp_tempdir_cleanup(const char *path);

/* ---- pathlib ---------------------------------------------------------------------------------------------------- */
/* str(PureWindowsPath(p)): separators to \, repeated separators and "." components dropped, a trailing separator
 * dropped; ".." is not resolved. */
char *avp_pl_str(AvpArena *a, const char *p);
AvpStr avp_pl_name(const char *p);                    /* Path.name (views into p, which must be avp_pl_str form) */
AvpStr avp_pl_suffix(const char *p);                  /* Path.suffix */
AvpStr avp_pl_stem(const char *p);                    /* Path.stem */
char *avp_pl_parent(AvpArena *a, const char *p);      /* str(Path.parent) */
char *avp_pl_with_suffix(AvpArena *a, const char *p, const char *suffix); /* NULL: ValueError (kind AVP_IOE_VALUE) */
char *avp_pl_with_name(AvpArena *a, const char *p, const char *name);     /* NULL: ValueError */
/* Path.relative_to(base) (3.12+ rules, walk_up False): false with kind AVP_IOE_VALUE:
 * "'x' is not in the subpath of 'y'". is_relative_to is the same test. */
bool avp_pl_relative_to(AvpArena *a, const char *p, const char *base, char **out);
bool avp_pl_is_relative_to(const char *p, const char *base);
AvpStrVec avp_pl_parts(AvpArena *a, const char *p);   /* Path.parts: "C:\\" then the names */
/* Path ordering (case-insensitive, compared by components): the comparison sorted(paths) uses. */
int avp_pl_cmp(const char *x, const char *y);
/* Path.resolve() (strict=False): realpath; Path.expanduser(); Path.iterdir() is avp_listdir (same order). */
char *avp_pl_resolve(AvpArena *a, const char *p);
/* Path.rglob(pattern) / Path.glob(pattern): every match under root (hidden names included; case-insensitive), in an
 * unspecified order. */
AvpStrVec avp_pl_glob(AvpArena *a, const char *root, const char *pattern);
AvpStrVec avp_pl_rglob(AvpArena *a, const char *root, const char *pattern);
