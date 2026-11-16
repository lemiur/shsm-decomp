/* Private helpers shared by the fs*.c and win*.c files of avpcore (not part of the public API). */
#pragma once
#include "avp/avp.h"
#include "avp/fs.h"
#include "avp/win.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <winioctl.h>

/* UTF-8 <-> UTF-16. u8_to_w gives a NUL-terminated string (invalid bytes become U+FFFD). */
wchar_t *avp_fs_u8_to_w(AvpArena *a, const char *s);
/* UTF-16 (n units, or n < 0 up to the NUL) to a NUL-terminated UTF-8 string of the arena. */
char *avp_fs_w_to_u8(AvpArena *a, const wchar_t *w, i64 n);
/* The path for a Win32 call: UTF-16, with the \\?\ prefix added (after GetFullPathNameW) when it would not fit in
 * MAX_PATH. */
wchar_t *avp_fs_wpath(AvpArena *a, const char *path);

/* Failure recording (see fs.h). The path arguments may be NULL. */
void avp_fs_fail_last(const char *path, const char *path2);              /* [WinError n] of GetLastError() */
void avp_fs_fail_open(unsigned winerror, const char *path);               /* open()'s [Errno n] from a Win32 error */
void avp_fs_fail_value(const char *fmt, ...);                             /* ValueError */
void avp_fs_fail_embedded_nul(void);

/* A minimal growing string builder over an arena (or the heap when a is NULL), so these files do not depend on
 * str.c. The content is NUL-terminated after fsb_cz(). */
typedef struct {
    char *p;
    size_t n, cap;
    AvpArena *a;
} FsBuf;
void fsb_init(FsBuf *b, AvpArena *a);
void fsb_reserve(FsBuf *b, size_t extra);
void fsb_put(FsBuf *b, const void *p, size_t n);
void fsb_putz(FsBuf *b, const char *z);
void fsb_putc(FsBuf *b, char c);
void fsb_putcp(FsBuf *b, u32 cp);
void fsb_printf(FsBuf *b, AVP_PRINTF_FMT const char *fmt, ...);
char *fsb_cz(FsBuf *b);   /* NUL-terminated content (never NULL) */
AvpStr fsb_str(FsBuf *b); /* NUL-terminated view of the content */

/* One UTF-8 code point (invalid bytes give U+FFFD, one byte each) and its encoding. */
u32 avp_fs_u8_next(const char **p, const char *end);
size_t avp_fs_u8_put(char out[4], u32 cp);

/* An AvpStr over a NUL-terminated string / a copy of n bytes with a NUL after them. */
static inline AvpStr fs_s(const char *z) { AvpStr s = {z, z ? strlen(z) : 0}; return s; }
AvpStr fs_dup(AvpArena *a, const char *p, size_t n);
static inline char *fs_cz(AvpArena *a, AvpStr s) { return (char *)fs_dup(a, s.p, s.n).p; }

/* Python's repr() of a str, appended to a builder. */
void avp_fs_repr(FsBuf *b, const char *utf8);

/* The text-codec core (fs_text.c). encode appends the file bytes of `text` (newline translation, then the encoding;
 * bom: put the utf-8-sig BOM first) to out. */
bool avp_fs_encode_text(FsBuf *out, AvpStr text, AvpTextMode m, bool bom);
bool avp_fs_decode_bytes(AvpArena *a, const u8 *p, size_t n, AvpTextMode m, AvpStr *out);

/* Opens a file the way the CRT's _wopen does for Python (share read+write, not inheritable). access is a
 * GENERIC_* mask, disposition a CREATE_* value. Returns INVALID_HANDLE_VALUE after recording an open()-style
 * failure. */
HANDLE avp_fs_open_handle(const char *path, DWORD access, DWORD disposition);

/* 64-bit helpers */
i64 avp_fs_filetime_to_ns(FILETIME ft);

/* GetFileInformationByHandle into an AvpStat (fs_os.c). */
bool avp_fs_stat_handle(HANDLE h, const char *path, AvpStat *st);

/* Temporary arenas. Every function of this module takes a private arena for its temporaries with fs_tmp() and gives
 * it back with fs_tmp_done(sc, mark) on every exit; the arena of a nested call is a different one, so a caller may
 * hand any arena (even avp_scratch()) to a function as the arena of its result. */
AvpArena *fs_tmp(void);
void fs_tmp_done(AvpArena *sc, AvpArenaMark mk);
