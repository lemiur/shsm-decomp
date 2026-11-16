/* cc.exe: a port of python tools/mwcc_trace/cc.py (compile one file with a unit's exact flags, print or compare its
 * functions). The pieces here are the Python module's functions; main.c is its main(). Prefix cctool_ (the lib's
 * cc_* names belong to the comparison library). */
#pragma once
#include "avp/avp.h"
#include "cc_codecmp.h"
#include "cc_compare.h"

/* unit_flags(unit) on the text of build.ninja (read with universal newlines): the first `build` block whose head
 * holds ": mwcc" and "src\<unit with backslashes>." (a substring test, as the Python's). Returns 0 with *ver
 * ("Wii/1.0", backslashes made forward) and *cflags; 1 when no block names the unit (SystemExit); 2 when the block
 * has no mw_version or cflags line (AttributeError); 3 when the cflags do not shlex-split (ValueError, *err). */
int cctool_unit_flags(AvpArena *a, AvpStr ninja, const char *unit, AvpStr *ver, AvpStrVec *cflags, const char **err);

/* The listing prefix "/\* ADDR OFF  BYTES *\/" (cc.py's regular expression): its length, 0 when the line does not
 * start with one. */
size_t cctool_prefix_len(AvpStr line);

/* disasm(obj, symbols, raw) on the listing text dtk wrote: the selected functions as "name:" plus their lines.
 * symbols NULL or empty: all. */
AvpStr cctool_filter_listing(AvpArena *a, AvpStr listing, const AvpSet *symbols, bool raw);

/* norm(line): "" for a line that is not an instruction. */
AvpStr cctool_norm(AvpArena *a, AvpStr line);

/* split_fns(text): {name: AvpStrVec* of body lines} (val.p) in first-seen order (a repeated name: last body). */
AvpMap *cctool_split_fns(AvpArena *a, AvpStr text);

/* The body of `.fn <name>,` in the retail text (the Python's ^\.fn NAME,.*?$(.*?)^\.endfn): false when none. */
bool cctool_retail_block(AvpStr retail, AvpStr name, AvpStr *body);
/* re.search(r"^\.fn NAME,", retail, re.M). */
bool cctool_retail_has(AvpStr retail, AvpStr name);

/* The --no-targets comparison of one function: the cc.py text block (heading and up to 40 rows), lines joined by
 * "\n" without a final newline. */
AvpStr cctool_positional(AvpArena *a, AvpStr fn, const AvpStrVec *body, AvpStr retail_body);

/* codecmp.describe_result(name, r, show_unresolved, limit). */
AvpStr cctool_describe_result(AvpArena *a, AvpStr name, const CcFnResult *r, bool show_unresolved, size_t limit);

/* The mangled-placeholder prefix fn_<8 upper hex> of a name (re.match), empty when none. */
AvpStr cctool_fn_prefix(AvpStr name);

/* The --cmp text over cc_compare_result_new's aligned pairs (the alignment match.exe --diff uses): the heading with
 * match.exe's row counts, then up to limit differing pairs marked ! (replaced), - (retail only), + (compiled only), T
 * (target mismatch). */
AvpStr cctool_describe_aligned(AvpArena *a, AvpStr name, const CcCompareResult *r, bool show_unresolved, size_t limit);
