/* Python's difflib (CPython 3.14.8), for what the tools use: SequenceMatcher (ratio, quick_ratio, real_quick_ratio,
 * find_longest_match, get_matching_blocks, get_opcodes, get_grouped_opcodes) and unified_diff. The algorithm is a
 * line-by-line port, so every result is the one Python gives, including the tie-breaking of find_longest_match,
 * the merge of adjacent matching blocks, the "popular element" pruning (autojunk: when len(b) >= 200 an element
 * that occurs more than len(b)//100 + 1 times is not used to start a match; Python's default is autojunk=True, the
 * tools pass autojunk=False almost everywhere) and the exact doubles of ratio() (2.0 * matches / length).
 *
 * Not ported because no tool uses them: Differ/ndiff, context_diff, get_close_matches, HtmlDiff, restore.
 *
 * A matcher compares two sequences. The elements can be
 *   - AvpStr (lines, tokens): equal when their bytes are equal;
 *   - u64 values (ids, code points, hashes);
 *   - the code points of two UTF-8 strings (Python str compared as a sequence of characters).
 * The constructors turn the elements into dense ids once (a hash table, O(la + lb)), then b2j is a CSR index (the
 * indices of every element of b in increasing order, like CPython's dict of lists) and find_longest_match works on
 * arrays instead of dicts. All memory comes from the arena given; nothing is freed by the matcher (use a scratch
 * arena with a mark/reset in loops that compare many pairs). The input arrays must stay valid while the matcher is
 * used (the AvpStr bytes are only read while the constructor runs).
 *
 *     AvpArenaMark mk = avp_arena_mark(avp_scratch());
 *     AvpSeqMatcher *m = avp_seqm_new_str(avp_scratch(), a, na, b, nb, NULL);   // SequenceMatcher(None, a, b)
 *     double r = avp_seqm_ratio(m);
 *     size_t n; const AvpOpcode *ops = avp_seqm_opcodes(m, &n);
 *     avp_arena_reset(avp_scratch(), mk);
 *
 * Differences from Python: indices and sizes are size_t; a matcher is built from both sequences at once (there is
 * no set_seq1/set_seq2 reuse); get_grouped_opcodes works on a copy of the opcodes (CPython edits the cached list
 * in place, which only shows if get_opcodes is called again afterwards). isjunk is available for u64 and code
 * point sequences (it receives the element value); the AvpStr constructor takes no isjunk. */
#pragma once
#include "str.h"

/* Options of SequenceMatcher(isjunk, a, b, autojunk). NULL means the defaults: no isjunk, autojunk True. */
typedef struct {
    bool no_autojunk;                     /* autojunk=False */
    bool (*isjunk)(void *ctx, u64 elem);  /* isjunk(elem); u64 and code point sequences only */
    void *ctx;
} AvpSeqOpts;
/* The tools' usual SequenceMatcher(None, a, b, autojunk=False). */
extern const AvpSeqOpts avp_seqm_no_autojunk;

typedef struct AvpSeqMatcher AvpSeqMatcher;

typedef struct {
    size_t a, b, size;
} AvpMatch; /* difflib.Match(a, b, size) */

typedef enum { AVP_OP_REPLACE, AVP_OP_DELETE, AVP_OP_INSERT, AVP_OP_EQUAL } AvpOpTag;
typedef struct {
    AvpOpTag tag;
    size_t i1, i2, j1, j2;
} AvpOpcode; /* ('replace' | 'delete' | 'insert' | 'equal', i1, i2, j1, j2) */
const char *avp_op_name(AvpOpTag tag); /* "replace", "delete", "insert", "equal" */

AvpSeqMatcher *avp_seqm_new_str(AvpArena *arena, const AvpStr *a, size_t na, const AvpStr *b, size_t nb,
                                const AvpSeqOpts *opts);
AvpSeqMatcher *avp_seqm_new_u64(AvpArena *arena, const u64 *a, size_t na, const u64 *b, size_t nb,
                                const AvpSeqOpts *opts);
AvpSeqMatcher *avp_seqm_new_u32(AvpArena *arena, const u32 *a, size_t na, const u32 *b, size_t nb,
                                const AvpSeqOpts *opts);
/* SequenceMatcher(None, s, t) on two Python str values: UTF-8 decoded into code points (invalid bytes become
 * U+FFFD, one per byte, as avp_u8_next does). */
AvpSeqMatcher *avp_seqm_new_text(AvpArena *arena, AvpStr a, AvpStr b, const AvpSeqOpts *opts);

size_t avp_seqm_len_a(const AvpSeqMatcher *m);
size_t avp_seqm_len_b(const AvpSeqMatcher *m);

/* find_longest_match(alo, ahi, blo, bhi); ahi/bhi == AVP_SEQ_END mean len(a)/len(b) (Python's None). */
#define AVP_SEQ_END ((size_t)-1)
AvpMatch avp_seqm_find_longest_match(AvpSeqMatcher *m, size_t alo, size_t ahi, size_t blo, size_t bhi);
/* get_matching_blocks(): increasing in a and b, adjacent blocks merged, ending with the (len(a), len(b), 0)
 * sentinel; the array is owned by the matcher (valid as long as its arena). */
const AvpMatch *avp_seqm_matching_blocks(AvpSeqMatcher *m, size_t *count);
/* get_opcodes(). */
const AvpOpcode *avp_seqm_opcodes(AvpSeqMatcher *m, size_t *count);
/* get_grouped_opcodes(n): the groups of changes with n lines of context; *ngroups groups, group g is
 * groups[g].ops[0 .. groups[g].n). n may be negative like Python (nonsense but the same nonsense). */
typedef struct {
    const AvpOpcode *ops;
    size_t n;
} AvpOpGroup;
const AvpOpGroup *avp_seqm_grouped_opcodes(AvpSeqMatcher *m, i64 n, size_t *ngroups);

double avp_seqm_ratio(AvpSeqMatcher *m);            /* 2.0 * matches / (len(a) + len(b)); 1.0 when both empty */
double avp_seqm_quick_ratio(AvpSeqMatcher *m);      /* the multiset upper bound */
double avp_seqm_real_quick_ratio(AvpSeqMatcher *m); /* min(len(a), len(b)) upper bound */

/* One-shot ratios (SequenceMatcher(None, a, b, autojunk=...).ratio()) that use the arena's scratch space and give it
 * back: no arena needed. */
double avp_ratio_str(const AvpStr *a, size_t na, const AvpStr *b, size_t nb, bool autojunk);
double avp_ratio_u64(const u64 *a, size_t na, const u64 *b, size_t nb, bool autojunk);
double avp_ratio_text(AvpStr a, AvpStr b, bool autojunk); /* two Python str values */

/* unified_diff(a, b, fromfile, tofile, fromfiledate, tofiledate, n, lineterm). Like Python the sequences are
 * compared with SequenceMatcher(None, a, b) (autojunk on) and every output line is ' ' / '-' / '+' plus the input
 * line (no terminator is added to those), the header and range lines end with lineterm. Defaults are made by
 * AVP_UDIFF_DEFAULT: n = 3, lineterm = "\n", empty names and dates:
 *     AvpUdiff o = AVP_UDIFF_DEFAULT;  o.fromfile = AVP_S("a/x"); o.tofile = AVP_S("b/x"); o.n = 1;
 * An empty fromfiledate/tofiledate gives no tab-date part (Python tests the truth of the string). */
typedef struct {
    AvpStr fromfile, tofile, fromfiledate, tofiledate;
    i64 n;
    AvpStr lineterm;
} AvpUdiff;
#define AVP_UDIFF_DEFAULT ((AvpUdiff){{"", 0}, {"", 0}, {"", 0}, {"", 0}, 3, {"\n", 1}})

/* "".join(unified_diff(...)) appended to out (nothing is appended when a == b). */
void avp_unified_diff(AvpBuf *out, const AvpStr *a, size_t na, const AvpStr *b, size_t nb, const AvpUdiff *o);
/* list(unified_diff(...)): the lines, as views into one arena block. */
AvpStrVec avp_unified_diff_lines(AvpArena *arena, const AvpStr *a, size_t na, const AvpStr *b, size_t nb,
                                 const AvpUdiff *o);
