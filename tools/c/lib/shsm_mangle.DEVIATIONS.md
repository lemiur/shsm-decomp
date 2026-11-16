# shsm_mangle: accepted differences from tools/common/mangle.py

The demangler's byte-versus-code-point behaviour is `D-B2` in `tools/c/DEVIATIONS.md` (recorded before the per-module
files); the entries below are the encoder's and the ones added with it.

## D-shsm_mangle-1. Unicode digits in template arguments
- Behaviour: a template argument that is an integer literal is recognised as `-?` followed by ASCII digits. The Python's
  `re.fullmatch(r"-?\d+", a)` also accepts other Unicode decimal digits, so `A<٣>` is kept as written by the Python
  (`mangle_template_name`, and `demangle_template_name`) and treated as a type name by the C.
- Reason: the same cause as `D-B2`; no name of `config/<ver>/symbols.txt` holds a non-ASCII character (all 26,634
  symbols agree in the differential test), and the project's types and template arguments are ASCII.

## D-shsm_mangle-2. Template nesting depth
- Behaviour: the Python recurses once per template nesting level of a type (`mangle_type` -> `mangle_scope` ->
  `mangle_template_name` -> `mangle_type`) and raises `RecursionError` when the interpreter's stack limit is reached.
  The C raises `SHSM_EXC_RECURSION "maximum recursion depth exceeded"` from nesting level 249, the limit measured for
  CPython 3.14 when `mangle_type` is called from the top level. The Python's limit depends on how deep its caller
  already is (called through one wrapper function it fails at 248 levels), so the two can differ by a few levels
  around that depth.
- Reason: a bounded C stack needs a limit; no symbol nests templates anywhere near that deep, and the differential
  test checks 100 levels (both succeed) and 300 levels (both raise).

## D-shsm_mangle-3. Interfaces that cannot carry the Python's other argument types
- Behaviour: names and types are `const char *` (no embedded NUL); a scope is either a `"A::B"` string or a list of
  strings (the two forms `mangle` and `mangle_scope` take); `None` is `NULL`. The Python's `TypeError`s for other
  argument types have no counterpart.
- Reason: the callers (`names.py`, `brief.py`, `query.py`) pass only strings, lists of strings and `None`.
