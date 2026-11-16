# shsm_mangle (tools/common/mangle.py)

CodeWarrior C++ name demangling and mangling in cwdemangle's output style: `lib/shsm_mangle.c` (the demangler, `parse`
and the type formatting), `lib/shsm_mangle_encode.c` (the mangler), `lib/shsm_mangle.h` (the API),
`lib/shsm_mangle_internal.h` (shared by the two `.c` files, not API). Accepted differences from the Python:
[`shsm_mangle.DEVIATIONS.md`](shsm_mangle.DEVIATIONS.md) (and `D-B2` in `tools/c/DEVIATIONS.md`).

Strings are NUL-terminated and allocated in the `AvpArena *a` passed first. Where the Python returns `None` the C
function returns `NULL`. Where the Python raises, the C function raises with `avp_throw` (catch with `AVP_TRY`):
`ValueError` is `SHSM_EXC_VALUE` and `RecursionError` is `SHSM_EXC_RECURSION`, with the Python's `str(e)` as the
message (`shsm_exc_class(code)` gives the class name). `DemangleError` is internal: the Python catches it exactly where
the C catches it, so it never escapes.

## Demangling

| C | Python | notes |
|---|---|---|
| `shsm_demangle(a, mangled)` | `demangle` | `'unk_Ramp::fn_800067CC(float)'`; `NULL` when it is not a mangled name |
| `shsm_demangle_or_name(a, name)` | `demangle(name) or name` | |
| `shsm_qualified(a, name)` | `match._qualified` | the demangled name up to its parameter list |
| `shsm_mangle_parse(a, mangled)` | `parse` | a `ShsmDemangled *`, `NULL` for `None` |
| `shsm_demangled_text(a, d)` | `Demangled.text()` | what `shsm_demangle` returns |
| `shsm_demangled_to_json(a, d)` | `Demangled.to_dict()` | an `AvpJson` object, keys in the Python's order: `kind scope name raw_name params const volatile ret thunk local text` |
| `shsm_demangle_template_name(a, raw)` | `demangle_template_name` | `'TextWriterBase<w>'` -> `'TextWriterBase<wchar_t>'` |

Raised out of the demangler as in the Python: `SHSM_EXC_VALUE "invalid literal for int() with base 10: 'x'"` (a `Q`
followed by a non-digit) and `SHSM_EXC_RECURSION "maximum recursion depth exceeded"` (a type cut off at the end of the
name, an empty template argument).

`ShsmDemangled` (`mangle.Demangled`):

| field | meaning |
|---|---|
| `kind`, `is_data` | `"function"` or `"data"` (`__vt__...`, `@LOCAL@` statics) |
| `scope`, `nscope` | qualifier parts, demangled (`nw4hbm`, `ut`, `TextWriterBase<char>`) |
| `name` | the base name as written in source (`~Foo`, `operator=`, `fn_800067CC`, `__vtable` for a vtable) |
| `raw_name` | the mangled base (`__dt`, `__as`, `__opi`); `""` for an `@LOCAL@` static |
| `params`, `nparams` | C spellings, none for `()` |
| `is_const`, `is_volatile` | method qualifiers |
| `ret` | return type text, `NULL` for `None` (template functions: `...F...x_v`) |
| `thunk` | `"@36@"` style prefix or `""` |
| `local` | for `@LOCAL@f@x`: the demangled enclosing function, `NULL` for `None` |

## Mangling

| C | Python | notes |
|---|---|---|
| `shsm_mangle(a, name, scope, params, nparams, is_const, is_volatile)` | `mangle` | `scope` is `"A::B"` or `NULL` (None); `""` is the one-part list `[""]`, as in the Python |
| `shsm_mangle_parts(a, name, scope, nscope, params, nparams, is_const, is_volatile)` | `mangle` with a list scope | `nscope == 0` is `None` |
| `shsm_mangle_type(a, text)` | `mangle_type` | `'const char*'` -> `'PCc'`; `ValueError` for arrays and function types ("unsupported type for mangling: <text>") and for `unsigned Foo` ("signed/unsigned on a non-builtin: <text>") |
| `shsm_mangle_scope(a, "A::B")`, `shsm_mangle_scope_parts(a, parts, n)` | `mangle_scope` (string, list) | `'Q21A1B'`; empty parts dropped; more than 9 parts: `ValueError "scope too deep"` |
| `shsm_mangle_template_name(a, name)` | `mangle_template_name` | `'TextWriterBase<wchar_t>'` -> `'TextWriterBase<w>'` |
| `shsm_split_scope(a, text, &n)` | `_split_scope` | cuts at `::` outside `<...>` |

`mangle` mangles the operator type (`operator const char*` -> `__opPCc`) before the parameters, and the parameters before
the scope, so the first error the Python hits is the first one the C raises.

## Python behaviour kept as it is

These are properties of `mangle.py` that the C reproduces, since the ports must agree with the reference; each is
covered by a test (`mangle_type_cases`, `mangle_special_names`, `symbols_roundtrip_vs_python`).

- `mangle_type("A<B>::C")`: the word split `[^\s<]+(?:<.*>)?|\S+` ends the first word at the template arguments and
  starts a second at `::C`; the words are joined with a space, so a qualified type with template arguments before the
  last `::` comes out as `Q25A<B> 1C` (arguments unmangled, a space in the name). Types without a qualifier after
  the template arguments are right (`nw4hbm::ut::TextWriterBase<char>` -> `Q36nw4hbm2ut17TextWriterBase<c>`).
- `mangle(name, "A<B::C>::D")` splits its scope string with `split("::")`, not `_split_scope`, so `::` inside
  `<...>` also cuts (`f__Q33A<B2C>1DFv`); `mangle_scope("A<B::C>::D")` is right (`Q29A<Q21B1C>1D`). Pass a list
  (`shsm_mangle_parts`) to avoid it.
- A function-template base name is written as given (`CalcStringRectImpl<wchar_t>`, not `<w>`), and there is no
  return type: a name with `_ret` does not round-trip.
- `demangle_template_name` leaves a space in front of an argument (`A<Ui, Sc>` -> `A<unsigned int,  Sc>`).

## Tests

`tests/shsm_mangle_test.c` (label `lib`): the Python test tables (`TestMangle.CASES`, `NOT_MANGLED`, `MANGLE`) as
literal data, field checks of `parse`, the encoder's cases and errors, and two differential tests that run
`tests/shsm_mangle_dump.py` (skipped without Python or outside the repository):

- `mangle_vs_python`: about 270 synthetic calls of every function above (types, scopes, template names, operators,
  constructors, error cases, 100 and 300 levels of template nesting), result or exception class and message compared.
- `symbols_roundtrip_vs_python`: every symbol of `config/<ver>/symbols.txt` (26,634). The parse of each is compared
  field for field with `Demangled.to_dict()`. A function that parses to a function (no data symbol, thunk or `@LOCAL@`
  static) is re-encoded as `mangle(d.name, d.scope, d.params, const=d.const, volatile=d.volatile)`; the C result (or
  the same `ValueError`) is compared with the Python's, and the round trip must give the symbol back except for the
  documented limits (unsupported parameter types, a return type, a template base name). Last run: 26,634 of 26,634 parse
  results agree; 860 symbols re-encode and all 860 agree; of the 855 functions, 846 round-trip exactly, 7 have an
  array or function-pointer parameter (`ValueError`), 2 have a return type and a template base name.
