/* Source stripping for a permuter base and MWCC symbol names: a port of the "Symbols" and "Stripping other function
 * bodies" sections of tools/permute.py (their docstrings are the spec). Other tools (the lever search, the draft
 * pipeline) link this through tool_permute: it has no dependency on the rest of permute.
 *
 * The Python's regular expressions are hand-coded scanners with the same results on the text they meet: Python's
 * \s is shsm_ws_len, \w is [A-Za-z0-9_] plus every byte >= 0x80 (a non-ASCII code point is taken as a letter; see
 * DEVIATIONS.md). Offsets are byte offsets into the UTF-8 text. Results are allocated in the arena given and are
 * NUL-terminated. Functions that index past the end where the Python raised IndexError (an unbalanced brace) return
 * false. */
#pragma once
#include "avp/avp.h"

/* ---- symbols ---------------------------------------------------------------------------------------------------- */

/* _class_of(enc): the innermost class name of an MWCC encoding (Q36nw4hbm3lyt6Window... -> Window); false for
 * None. */
bool permute_class_of(AvpStr enc, AvpStr *out);
/* source_names(sym): the unqualified name a definition of sym is written with ("DrawFrame", "Window", "~Window",
 * "operator"; the Python's one-element set); false when sym is not mangled (None). *out is a view into sym or a
 * string of a. */
bool permute_source_name(AvpArena *a, AvpStr sym, AvpStr *out);
/* is_mangled(sym) */
bool permute_is_mangled(AvpStr sym);
/* dir_name(sym): sym, or its first 48 characters + "_" + 8 hex digits of its sha1 when longer than 64. */
AvpStr permute_dir_name(AvpArena *a, AvpStr sym);

/* ---- C ---------------------------------------------------------------------------------------------------------- */

/* strip_other_fns(src, keep): every function body except keep's replaced with ";". */
bool permute_strip_other_fns(AvpArena *a, AvpStr src, AvpStr keep, AvpStr *out);
/* strip_extern_c(src): extern "C" { ... } wrappers dropped (contents kept), extern "C" <decl> -> extern <decl>. */
bool permute_strip_extern_c(AvpArena *a, AvpStr src, AvpStr *out);

/* ---- C++ -------------------------------------------------------------------------------------------------------- */

/* _fn_name(head): (qualifier without spaces, unqualified name or "operator", index of the parameter list's "(");
 * false for None. qual and name are views into head except a qualifier with spaces (copied into a). */
bool permute_fn_name(AvpArena *a, AvpStr head, AvpStr *qual, AvpStr *name, size_t *paren);
/* _decl_of(head, paren): the head as a declaration (parameter list, cv-qualifiers, no initialiser list) + ";". */
AvpStr permute_decl_of(AvpArena *a, AvpStr head, size_t paren);

/* One out-of-class function definition (_scan_cxx_defs' dict). */
typedef struct {
    AvpStr head;            /* one line, preprocessor lines left out */
    AvpStr qual, name;      /* _fn_name */
    size_t paren;           /* index of the parameter list in head */
    size_t start;           /* start of the head region in src */
    size_t body, end;       /* the body [body, end) in src */
    AvpStrVec pragmas;      /* the preprocessor lines inside the head region */
    bool forced;            /* template or inline */
} PermuteCxxDef;

typedef AVP_VEC(PermuteCxxDef) PermuteCxxDefs;

/* _scan_cxx_defs(src) */
PermuteCxxDefs permute_scan_cxx_defs(AvpArena *a, AvpStr src);

/* strip_other_fns_cxx(src, keep, keep_callees): keep holds the source names to keep. *kept: definitions named in
 * keep; *callees: names of the callees kept, sorted. */
AvpStr permute_strip_other_fns_cxx(AvpArena *a, AvpStr src, const AvpStr *keep, size_t nkeep, bool keep_callees,
                                   size_t *kept, AvpStrVec *callees);
