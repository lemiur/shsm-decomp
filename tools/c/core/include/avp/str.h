/* Strings. Text is UTF-8 everywhere (files, arguments, output). AvpStr is a view (pointer + byte length, not
 * necessarily NUL-terminated); AvpBuf builds strings. The functions follow the Python str methods they are named
 * after, including the edge cases (negative indices clamp as slices do, split() without a separator splits on runs of
 * whitespace and drops empty fields, splitlines() knows Python's line boundaries). Comparisons are by bytes, which for
 * UTF-8 is the code point order Python's str comparison uses.
 *
 * Python's whitespace (str.isspace / split() / strip()) is the Unicode one: ASCII \t \n \v \f \r space, \x1c-\x1f,
 * and U+0085, U+00A0, U+1680, U+2000-U+200A, U+2028, U+2029, U+202F, U+205F, U+3000; the functions below use that set.
 *
 * Results that are views (strip, slice, partition, split fields) point into the input. Results that are new
 * strings are allocated from the arena given. */
#pragma once
#include "arena.h"
#include "vec.h"

typedef struct {
    const char *p;
    size_t n;
} AvpStr;
typedef AVP_VEC(AvpStr) AvpStrVec;

#define AVP_S(lit) ((AvpStr){"" lit, sizeof(lit) - 1}) /* a string literal as an AvpStr */
#define AVP_SF(s) (int)(s).n, (s).p                    /* printf("%.*s", AVP_SF(s)) */

AvpStr avp_str(const char *cz); /* NULL gives the empty string */
AvpStr avp_strn(const char *p, size_t n);
char *avp_cz(AvpArena *a, AvpStr s); /* a NUL-terminated copy */
AvpStr avp_str_dup(AvpArena *a, AvpStr s);

bool avp_str_eq(AvpStr a, AvpStr b);
bool avp_str_eqz(AvpStr a, const char *cz);
int avp_str_cmp(AvpStr a, AvpStr b); /* <0, 0, >0: byte order (code point order) */
bool avp_str_startswith(AvpStr s, AvpStr prefix);
bool avp_str_endswith(AvpStr s, AvpStr suffix);
bool avp_str_contains(AvpStr hay, AvpStr needle);
/* find/rfind/count over [start, end) byte offsets with slice semantics (negative counts from the end; end ==
 * AVP_END for the end); find and rfind return the byte offset or -1. */
#define AVP_END ((i64)INT64_MAX)
i64 avp_str_find(AvpStr hay, AvpStr needle, i64 start, i64 end);
i64 avp_str_rfind(AvpStr hay, AvpStr needle, i64 start, i64 end);
size_t avp_str_count(AvpStr hay, AvpStr needle);
i64 avp_str_findc(AvpStr s, char c); /* first byte c or -1 */

/* s[i:j] on bytes, slice semantics (negative from the end, clamped; j == AVP_END for the end). */
AvpStr avp_str_slice(AvpStr s, i64 i, i64 j);

AvpStr avp_str_strip(AvpStr s);  /* Python whitespace */
AvpStr avp_str_lstrip(AvpStr s);
AvpStr avp_str_rstrip(AvpStr s);
AvpStr avp_str_strip_chars(AvpStr s, AvpStr chars); /* s.strip(chars): chars is a set of code points */
AvpStr avp_str_lstrip_chars(AvpStr s, AvpStr chars);
AvpStr avp_str_rstrip_chars(AvpStr s, AvpStr chars);
AvpStr avp_str_removeprefix(AvpStr s, AvpStr prefix);
AvpStr avp_str_removesuffix(AvpStr s, AvpStr suffix);

/* split: sep.n == 0 is split() / split(None) (whitespace runs, no empty fields); maxsplit < 0 is no limit. */
AvpStrVec avp_str_split(AvpStr s, AvpStr sep, int maxsplit);
AvpStrVec avp_str_rsplit(AvpStr s, AvpStr sep, int maxsplit);
AvpStrVec avp_str_splitlines(AvpStr s, bool keepends); /* \n \r \r\n \v \f \x1c \x1d \x1e U+0085 U+2028 U+2029 */
/* partition/rpartition: true when sep was found; *before, *after as Python. Not found: partition gives before = s,
 * after = empty; rpartition gives before = empty, after = s. sep must not be empty (Python raises ValueError). */
bool avp_str_partition(AvpStr s, AvpStr sep, AvpStr *before, AvpStr *after);
bool avp_str_rpartition(AvpStr s, AvpStr sep, AvpStr *before, AvpStr *after);

AvpStr avp_str_replace(AvpArena *a, AvpStr s, AvpStr old, AvpStr new_, int count); /* count < 0: all */
AvpStr avp_str_join(AvpArena *a, AvpStr sep, const AvpStr *items, size_t count);
AvpStr avp_str_concat(AvpArena *a, AvpStr x, AvpStr y);
AvpStr avp_str_repeat(AvpArena *a, AvpStr s, size_t times);
/* str.lower() / str.upper() over all of Unicode, as CPython 3.14 (full mappings: multi-character results and the final
 * sigma included). */
AvpStr avp_str_lower(AvpArena *a, AvpStr s);
AvpStr avp_str_upper(AvpArena *a, AvpStr s);
/* str.casefold(): full Unicode case folding per code point (U+00DF gives "ss", U+03C2 gives U+03C3). */
AvpStr avp_str_casefold(AvpArena *a, AvpStr s);
AvpStr avp_str_expandtabs(AvpArena *a, AvpStr s, int tabsize);
/* ljust/rjust/center/zfill pad to a width counted in code points, as Python does. */
AvpStr avp_str_ljust(AvpArena *a, AvpStr s, size_t width, u32 fill);
AvpStr avp_str_rjust(AvpArena *a, AvpStr s, size_t width, u32 fill);
AvpStr avp_str_center(AvpArena *a, AvpStr s, size_t width, u32 fill);
AvpStr avp_str_zfill(AvpArena *a, AvpStr s, size_t width);

/* Python's str predicates (false for the empty string, as Python). */
bool avp_str_isdigit(AvpStr s);   /* ASCII digits and the Unicode Nd/No digits Python counts */
bool avp_str_isdecimal(AvpStr s);
bool avp_str_isalpha(AvpStr s);
bool avp_str_isalnum(AvpStr s);
bool avp_str_isspace(AvpStr s);
bool avp_str_isupper(AvpStr s);
bool avp_str_islower(AvpStr s);
bool avp_str_isidentifier(AvpStr s);
bool avp_str_isascii(AvpStr s);   /* true for the empty string, as Python */
bool avp_cp_isspace(u32 cp);

/* Python int(s, base) and float(s): surrounding whitespace allowed, underscores between digits, base 0 reads the
 * 0x/0o/0b prefixes, float accepts inf/nan/infinity in any case. False when Python would raise ValueError, or when
 * an int does not fit in 64 bits (*overflow set when non-NULL). */
bool avp_parse_int(AvpStr s, int base, i64 *out, bool *overflow);
bool avp_parse_uint(AvpStr s, int base, u64 *out);
bool avp_parse_float(AvpStr s, double *out);

/* ---- UTF-8 ---- */
size_t avp_u8_len(AvpStr s);                      /* code points (len() of the Python str) */
AvpStr avp_u8_slice(AvpStr s, i64 i, i64 j);      /* s[i:j] counted in code points, slice semantics */
u32 avp_u8_next(const char **p, const char *end); /* decodes one code point; invalid bytes give U+FFFD, one byte each */
size_t avp_u8_put(char out[4], u32 cp);           /* encodes; returns the byte count */
bool avp_u8_valid(AvpStr s);
/* UTF-16 for the Win32 W APIs (NUL-terminated), and back (n < 0: up to the NUL). Unpaired surrogates become
 * U+FFFD. */
wchar_t *avp_to_w(AvpArena *a, AvpStr s);
wchar_t *avp_to_wz(AvpArena *a, const char *cz);
AvpStr avp_from_w(AvpArena *a, const wchar_t *w, i64 n);

/* base64.b64encode(data): the standard alphabet with '=' padding, no line breaks. */
AvpStr avp_base64_encode(AvpArena *a, const void *data, size_t n);

/* ---- builder ----
 * buf.a == NULL: heap memory (avp_buf_free); else the arena's. A zeroed AvpBuf is a valid empty heap builder.
 * The content is always NUL-terminated (buf.p[buf.n] == 0) once anything was added. */
typedef struct {
    char *p;
    size_t n;
    size_t cap;
    AvpArena *a;
} AvpBuf;
void avp_buf_init(AvpBuf *b, AvpArena *a);
void avp_buf_put(AvpBuf *b, AvpStr s);
void avp_buf_putz(AvpBuf *b, const char *cz);
void avp_buf_putn(AvpBuf *b, const void *p, size_t n);
void avp_buf_putc(AvpBuf *b, char c);
void avp_buf_putcp(AvpBuf *b, u32 cp); /* one code point as UTF-8 */
void avp_buf_printf(AvpBuf *b, AVP_PRINTF_FMT const char *fmt, ...); /* C printf rules */
void avp_buf_vprintf(AvpBuf *b, const char *fmt, va_list ap);
void avp_buf_reserve(AvpBuf *b, size_t extra);
void avp_buf_clear(AvpBuf *b);
AvpStr avp_buf_str(const AvpBuf *b);
char *avp_buf_cz(AvpBuf *b); /* the NUL-terminated content ("" when empty) */
void avp_buf_free(AvpBuf *b);
