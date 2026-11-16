/* Taken from another project of the author's tools/c/core/include/avp/re.h at commit 49e811503d7ba5c9cff92160557df057c50a8ab3 (names avp_re -> shsm_re).
 * Python's `re` module on top of the vendored PCRE2 10.44 (8-bit, JIT). The API is shaped like the module and
 * reproduces its observable behaviour: search / match / fullmatch with pos and endpos, finditer, findall (the
 * three result shapes), split (maxsplit, groups in the result, None for an unmatched group), sub / subn with a
 * Python replacement template or a callback, escape, group numbers and names, groupdict order, lastindex,
 * unmatched groups (-1 spans, like Python's None), and the empty-match rules of Python 3.7+ (an empty match right
 * after a non-empty one is allowed; after an empty match the next search starts at the same position but must not
 * give an empty match there). The scan loops are written here, not taken from pcre2_substitute, so they are
 * Python's.
 *
 * Offsets. Subjects are UTF-8 (AvpStr) and every offset (pos, endpos, spans, ShsmReMatch accessors) is a BYTE
 * offset, because that is what the callers slice with. A tool that prints or compares Python's code-point index
 * converts with avp_u8_len on the prefix:
 *     size_t cp = avp_u8_len(avp_strn(subject.p, (size_t)shsm_re_start(&m, 0)));
 * A str pattern matches code points (PCRE2_UTF); a subject that is not valid UTF-8 is an error (AVP_ERR_FORMAT):
 * Python could not have held it as a str. With SHSM_RE_BYTES the pattern is a Python bytes pattern (rb"..."): the
 * subject is bytes, no UTF, no UCP, \w \d \s \b and case folding are ASCII only (what Python does for bytes).
 *
 * Python semantics reproduced by a translation step before compiling (the Python pattern is parsed and rewritten
 * for PCRE2; the result is cached with the compiled code):
 *   - str patterns are Unicode-aware (PCRE2_UTF | PCRE2_UCP) unless SHSM_RE_A or SHSM_RE_BYTES;
 *   - \Z is PCRE2 \z; {,n} is {0,n}; a { that does not start a Python quantifier is a literal; ^ and $ in
 *     MULTILINE mode (Python's ^ also matches after a trailing newline); `.` and DOTALL; VERBOSE mode (whitespace
 *     and # comments dropped outside character classes, kept inside, as Python) also as a scoped (?x:...);
 *   - scoped inline flags (?i:...) (?-i:...) (?s:...) (?a:...) and the global flags that must come first;
 *   - \xhh (exactly two digits), \uXXXX, \UXXXXXXXX, octal escapes, \1..\99 group references (always a
 *     reference, never an octal escape, as Python), (?P<name>...), (?P=name), (?(1)..|..), (?>...) and
 *     possessive quantifiers; `[` inside a character class is a literal (never a POSIX class);
 *   - the syntax PCRE2 accepts and Python rejects is rejected with Python's re.error message: unknown escapes of
 *     ASCII letters ("bad escape \h"), nothing to repeat, multiple repeat, unbalanced parenthesis, unterminated
 *     character set, unknown group name, bad character range, global flags not at the start, ...
 *   The message is Python's str(re.error): "<msg> at position N" plus " (line L, column C)" when the pattern
 *   holds a newline; N counts code points of a str pattern.
 *
 * \w, \s, \d, \b follow Python's definitions (a Unicode \w is letters, numbers and underscore, not PCRE2 10.43+'s
 * wider set with combining marks; \s is str.isspace without PCRE2's U+180E); they are spelled out for PCRE2.
 *
 * Not reproduced (the translation or PCRE2 differs): \N{NAME} (no Unicode name database: an error), the locale
 * flag (?L), re.DEBUG output, Python's dotless-i equivalence under IGNORECASE (other case folding is PCRE2's
 * Unicode simple case folding), a negated shorthand inside a character class of a pattern that mixes ASCII and
 * Unicode scopes, the Unicode data version (PCRE2 10.44's is older than CPython 3.14's: code points assigned
 * since, e.g. U+1C89 or U+2EBF0, are not word characters or digits), lookbehind width rules (PCRE2 accepts more
 * than Python), patterns whose compiled form exceeds 64 KiB (the PCRE2 build's link size), more than
 * SHSM_RE_MAX_GROUPS capturing groups.
 *
 * Thread safety. A compiled ShsmRe is immutable and shared by all threads (shsm_re keeps a process-wide cache keyed
 * by pattern and flags; every pattern is compiled once, JIT included, and never freed). Match data, the JIT stack
 * and the match context are per thread. An ShsmReMatch / ShsmReIter belongs to the thread that uses it. */
#pragma once
#include "avp/str.h"

/* Python's re flag values, so a flag expression can be translated one to one. */
enum {
    SHSM_RE_I = 2,        /* IGNORECASE */
    SHSM_RE_L = 4,        /* LOCALE: refused */
    SHSM_RE_M = 8,        /* MULTILINE */
    SHSM_RE_S = 16,       /* DOTALL */
    SHSM_RE_U = 32,       /* UNICODE: the default for str patterns, an error with SHSM_RE_BYTES */
    SHSM_RE_X = 64,       /* VERBOSE */
    SHSM_RE_DEBUG = 128,  /* accepted and ignored (Python prints a dump of the parsed pattern) */
    SHSM_RE_A = 256,      /* ASCII */
    SHSM_RE_BYTES = 1 << 16 /* not a Python flag: the pattern is a bytes pattern and subjects are bytes */
};

typedef struct ShsmRe ShsmRe;

/* ---- compiling ---------------------------------------------------------------------------------------------
 * shsm_re: for literal patterns known to be valid; compiled once and cached (thread-safe), so it may be called
 * at every use. An invalid pattern is an internal error (avp_panic). shsm_re_try: for patterns built at run time
 * (a user's --select option): throws AVP_ERR_REGEX with Python's re.error text (see base.h AVP_TRY); the cache is
 * used as well. shsm_re_compile: the same without a throw: false and the message in err[errcap]. */
ShsmRe *shsm_re(const char *pattern, int flags);
ShsmRe *shsm_re_try(const char *pattern, int flags);
ShsmRe *shsm_re_try_n(AvpStr pattern, int flags); /* a pattern that is not NUL-terminated or holds NUL bytes */
bool shsm_re_compile(AvpStr pattern, int flags, ShsmRe **out, char *err, size_t errcap);

/* Pattern properties: .pattern, .flags (as given), .groups, .groupindex. */
AvpStr shsm_re_pattern(const ShsmRe *re);
int shsm_re_flags(const ShsmRe *re);
int shsm_re_groups(const ShsmRe *re);
/* The PCRE2 pattern the Python pattern was translated to (diagnostics and tests). */
AvpStr shsm_re_translated(const ShsmRe *re);
/* Group number of a name, or -1 (groupindex.get). The name of group g (1..groups), or NULL for an unnamed
 * group; groupdict() order is the group number order. */
int shsm_re_group_index(const ShsmRe *re, const char *name);
const char *shsm_re_group_name(const ShsmRe *re, int g);

/* ---- matches ---------------------------------------------------------------------------------------------- */
/* A pattern may have at most SHSM_RE_MAX_GROUPS capturing groups (shsm_re refuses more: the match structure holds
 * the spans inline, so a match needs no allocation and no free). */
#define SHSM_RE_MAX_GROUPS 31

typedef struct ShsmReMatch {
    const ShsmRe *re;
    AvpStr string; /* the whole subject (match.string) */
    i64 pos, endpos;
    int lastindex; /* match.lastindex, -1 for None */
    i64 span[2 * (SHSM_RE_MAX_GROUPS + 1)]; /* start, end of group 0..groups; -1, -1 when it did not take part */
} ShsmReMatch;

/* Group g (0 = the whole match) of a match. start/end give -1 for a group that did not take part (Python's
 * span() of an unmatched group); shsm_re_group gives {NULL, 0} for it (Python None) and a view into the subject
 * (p != NULL, possibly empty) otherwise. Out-of-range g is an internal error (Python raises IndexError). */
i64 shsm_re_start(const ShsmReMatch *m, int g);
i64 shsm_re_end(const ShsmReMatch *m, int g);
bool shsm_re_matched(const ShsmReMatch *m, int g);
AvpStr shsm_re_group(const ShsmReMatch *m, int g);
/* m.group(name): the name must exist (else an internal error); AvpStr as shsm_re_group. */
AvpStr shsm_re_group_named(const ShsmReMatch *m, const char *name);
/* m.lastgroup: the name of the group of lastindex, or NULL. */
const char *shsm_re_lastgroup(const ShsmReMatch *m);
/* m.expand(template): appends the expansion to out. */
void shsm_re_expand(AvpBuf *out, const ShsmReMatch *m, AvpStr tmpl);

/* ---- search / match / fullmatch ----------------------------------------------------------------------------
 * pos and endpos are Python's: bytes offsets, clamped to [0, len]; pass 0 and AVP_END for the whole subject.
 * ^ does not match at pos (only at the real start); endpos behaves as if the subject ended there. m may be NULL
 * when only the answer matters. A single call validates the subject as UTF-8 (cost linear in the subject); loops
 * over one subject use shsm_re_iter, or call shsm_re_assume_valid first. */
bool shsm_re_search(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m);
bool shsm_re_match(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m);
bool shsm_re_fullmatch(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos, ShsmReMatch *m);
/* Declares that the bytes of s are valid UTF-8 for the calling thread's next calls on this subject (any
 * sub-range of it), so those calls skip validation. The caller vouches for it (text read by fs.h is validated). */
void shsm_re_assume_valid(AvpStr s);

/* ---- finditer ----------------------------------------------------------------------------------------------
 *     ShsmReIter it; ShsmReMatch m;
 *     shsm_re_iter_init(&it, re, text, 0, AVP_END);
 *     while (shsm_re_iter_next(&it, &m)) { ... }
 * Python's rules for empty matches; the subject is validated once. */
typedef struct ShsmReIter {
    const ShsmRe *re;
    AvpStr s;
    i64 pos, endpos;
    bool must_advance;
    bool done;
} ShsmReIter;
void shsm_re_iter_init(ShsmReIter *it, const ShsmRe *re, AvpStr s, i64 pos, i64 endpos);
bool shsm_re_iter_next(ShsmReIter *it, ShsmReMatch *m);

/* ---- findall -----------------------------------------------------------------------------------------------
 * Python's result shapes: no groups: the whole match; one group: that group; more: a tuple of all the groups.
 * An unmatched group gives "" (p != NULL, n 0). width is 1 for the first two shapes, the group count for tuples;
 * item i, field j is v.v[i * width + j]. Strings are views into the subject. Free with shsm_re_findall_free. */
typedef struct {
    AvpStrVec v;
    size_t count; /* number of items (matches) */
    int width;
} ShsmReFindall;
ShsmReFindall shsm_re_findall(const ShsmRe *re, AvpStr s, i64 pos, i64 endpos);
AvpStr shsm_re_findall_at(const ShsmReFindall *r, size_t i, int j);
void shsm_re_findall_free(ShsmReFindall *r);

/* ---- split -------------------------------------------------------------------------------------------------
 * re.split(pattern, s, maxsplit): the pieces and, after each, the groups of its separator. A group that did not
 * take part is represented distinctly: .p == NULL (Python None); an empty string has p != NULL. maxsplit 0 is "no
 * limit" (Python); a negative maxsplit splits nothing, as Python. The pieces are views into the subject. */
AvpStrVec shsm_re_split(const ShsmRe *re, AvpStr s, int maxsplit);

/* ---- sub / subn --------------------------------------------------------------------------------------------
 * repl is a Python replacement template: \1..\99, \g<n>, \g<name>, \g<0>, the escapes \a \b \f \n \r \t \v \\,
 * \0 and octal escapes, other backslash pairs of non-letters kept as written; an unmatched group gives "". Bad
 * templates throw AVP_ERR_REGEX with Python's message ("invalid group reference 3 at position 1", "bad escape \q
 * at position 0", "unknown group name 'x'", ...). A template without a backslash is used as is. Templates are
 * parsed once per (pattern, template) and cached. count 0 replaces every match (Python). When nothing is replaced
 * the result is s itself (not a copy). The result is allocated from a; *n (optional) gets the number of
 * replacements (subn). */
AvpStr shsm_re_sub(AvpArena *a, const ShsmRe *re, AvpStr repl, AvpStr s, int count);
AvpStr shsm_re_subn(AvpArena *a, const ShsmRe *re, AvpStr repl, AvpStr s, int count, size_t *n);
/* The callback form: fn appends the replacement text for match m to out (Python's callable returns a str). */
typedef void (*ShsmReSubFn)(void *ctx, const ShsmReMatch *m, AvpBuf *out);
AvpStr shsm_re_sub_fn(AvpArena *a, const ShsmRe *re, ShsmReSubFn fn, void *ctx, AvpStr s, int count, size_t *n);

/* ---- escape ------------------------------------------------------------------------------------------------
 * re.escape (3.7+): only the characters  ()[]{}?*+-|^$\.&~# and the ASCII whitespace \t \n \r \v \f and space are
 * backslash-escaped. The result is allocated from a. */
AvpStr shsm_re_escape(AvpArena *a, AvpStr s);
