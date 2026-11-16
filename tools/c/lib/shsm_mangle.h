/* CodeWarrior C++ name demangling and mangling as tools/common/mangle.py does it (cwdemangle's output style); see
 * shsm_mangle.md. The demangler treats names as bytes (the Python treats code points: the two differ only for
 * non-ASCII names, which symbols.txt does not hold); the mangler counts code points in <len> as the Python does. */
#pragma once
#include "avp/json.h"
#include "avp/str.h"

/* mangle.demangle(mangled): 'unk_Ramp::fn_800067CC(float)' for 'fn_800067CC__8unk_RampFf', NULL when it does not
 * parse as a mangled name. Raises where the Python raised out of demangle: SHSM_EXC_VALUE "invalid literal for int()
 * with base 10: 'x'" (Q followed by a non-digit) and SHSM_EXC_RECURSION "maximum recursion depth exceeded" (a type
 * cut off at the end of the name, e.g. 'x__FP', or an empty template argument). */
const char *shsm_demangle(AvpArena *a, const char *mangled);
/* demangle(name) or name. */
const char *shsm_demangle_or_name(AvpArena *a, const char *name);
/* match._qualified(name): the demangled name up to its parameter list ('unk_Ramp::fn_800067CC'), name itself when
 * it does not demangle (or demangles to ""). */
const char *shsm_qualified(AvpArena *a, const char *name);

/* ---- parse(): the structured form ---- */

/* mangle.Demangled. Strings are NUL-terminated and live in the arena. */
typedef struct {
    const char *kind;        /* "function" or "data" */
    bool is_data;            /* kind == "data" */
    const char **scope;      /* qualifier parts, demangled ("nw4hbm", "ut", "TextWriterBase<char>") */
    size_t nscope;
    const char *name;        /* the base name as written in source: "~Foo", "operator=", "fn_800067CC" */
    const char *raw_name;    /* the mangled base: "__dt", "__as"; "" for an @LOCAL@ static */
    const char **params;     /* C spellings; nparams 0 for "()" */
    size_t nparams;
    bool is_const, is_volatile; /* method qualifiers */
    const char *ret;         /* return type text, NULL when None */
    const char *thunk;       /* "@36@" style prefix or "" */
    const char *local;       /* @LOCAL@f@x: the demangled enclosing function text, NULL when None */
} ShsmDemangled;

/* mangle.parse(mangled): NULL where the Python returned None. Raises as shsm_demangle does. */
ShsmDemangled *shsm_mangle_parse(AvpArena *a, const char *mangled);
/* Demangled.text() (shsm_demangle(x) is shsm_demangled_text(shsm_mangle_parse(x))). */
const char *shsm_demangled_text(AvpArena *a, const ShsmDemangled *d);
/* Demangled.to_dict(): {kind, scope, name, raw_name, params, const, volatile, ret, thunk, local, text}. */
AvpJson *shsm_demangled_to_json(AvpArena *a, const ShsmDemangled *d);
/* mangle.demangle_template_name(raw): 'TextWriterBase<w>' -> 'TextWriterBase<wchar_t>'. Raises as shsm_demangle. */
const char *shsm_demangle_template_name(AvpArena *a, const char *raw);

/* ---- mangling ---- */

/* mangle._split_scope(text): 'A::B<C::D>::E' -> {"A", "B<C::D>", "E"} (:: inside <> kept). */
const char **shsm_split_scope(AvpArena *a, const char *text, size_t *n);
/* mangle.mangle_template_name(name): 'TextWriterBase<wchar_t>' -> 'TextWriterBase<w>'. */
const char *shsm_mangle_template_name(AvpArena *a, const char *name);
/* mangle.mangle_scope(scope): 'A::B' -> 'Q21A1B', one part -> '<len><name>'; the _parts form takes the list. Empty
 * parts are dropped; more than 9 parts raise ValueError "scope too deep". */
const char *shsm_mangle_scope(AvpArena *a, const char *scope);
const char *shsm_mangle_scope_parts(AvpArena *a, const char *const *parts, size_t n);
/* mangle.mangle_type(text): 'const char*' -> 'PCc'. ValueError (SHSM_EXC_VALUE) for arrays, function types and a
 * signed/unsigned non-builtin, with the Python message. */
const char *shsm_mangle_type(AvpArena *a, const char *text);
/* mangle.mangle(name, scope, params, const, volatile): scope is a 'A::B' string (NULL: None), or the _parts form's
 * list. shsm_mangle("fn_800067CC", "unk_Ramp", {"float"}, 1, false, false) == "fn_800067CC__8unk_RampFf". */
const char *shsm_mangle(AvpArena *a, const char *name, const char *scope, const char *const *params, size_t nparams,
                        bool is_const, bool is_volatile);
const char *shsm_mangle_parts(AvpArena *a, const char *name, const char *const *scope, size_t nscope,
                              const char *const *params, size_t nparams, bool is_const, bool is_volatile);
