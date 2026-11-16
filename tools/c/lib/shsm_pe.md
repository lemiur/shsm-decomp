# shsm_pe: PE headers and the Metrowerks CodeView NB11 reader

`#include "shsm_pe.h"`. Ports `tools/mwcc_trace/symbols.py` (the library half: `_pe_sections`, `_find_nb11`,
`parse`, `load`, `SymbolTable`, `_strip`, `version_of`) and the section-header reading of
`tools/mwcc_trace/pe_disasm.py` (`PE.__init__`). The command line of `symbols.py` is `tools/mwcc_symbols`; the
explorer is `tools/pe_disasm`; `tools/wdbg` names compiler addresses through `shsm_pe_symbols_load` and
`shsm_pe_symbols_describe`.

| Python | C |
|---|---|
| `_pe_sections(d)` / `PE.__init__` header reads | `shsm_pe_headers(a, d, n, SHSM_PE_SYMBOLS \| SHSM_PE_DISASM, &h)`: the two readers read different fields in different orders and bounds (symbols checks the `PE\0\0` signature, reads `NumberOfRvaAndSizes` and 24 bytes per section header; pe_disasm reads 40); `ShsmPeHeaders` with `ShsmPeSection` (name, vsize, va, rsize, raw, ch) |
| `struct.unpack_from('<I' / '<H', d, off)` | `shsm_pe_u32` / `shsm_pe_u16` (throw `SHSM_PE_EXC_STRUCT` with CPython's text) |
| `symbols.parse(d)` / `symbols.load(path)` | `shsm_pe_symbols_parse(a, d, n)` / `shsm_pe_symbols_load(a, exe)` -> `ShsmPeSymbols *` |
| `SymbolTable.funcs` / `.ranges` / `.data` | `t->funcs` (`ShsmPeFunc`: va, size, name, module), `t->ranges` (`ShsmPeRange`: lo, hi, module), `t->data` (`ShsmPeDatum`: va, name) |
| `t.lookup(va)` | `shsm_pe_symbols_lookup(t, va)` (NULL for None) |
| `t.find(text)` | `shsm_pe_symbols_find(a, t, text, &hits)` (count; exact lowered matches, else substrings) |
| `t.module_at(va)` | `shsm_pe_symbols_module_at(t, va)` (`.p == NULL` for None) |
| `t.describe(va)` | `shsm_pe_symbols_describe(a, t, va)` (`.n == 0` for None) |
| `t.describe_data(va)` | `shsm_pe_symbols_describe_data(t, va)` (`.n == 0` for None) |
| `_strip(name)` | `shsm_pe_strip(name)` (the `re.match(r'(.*)@[0-9]+$')` rule written out, newline cases included) |
| `version_of(path)` | `shsm_pe_version_of(a, root, path)` (root: the Python's `ROOT`, the checkout with backslashes) |
| `NoDebugInfo` / `struct.error` / `IndexError` / `OSError` | `avp_throw` codes `SHSM_PE_EXC_NO_DEBUG_INFO`, `SHSM_PE_EXC_STRUCT`, `SHSM_PE_EXC_INDEX`, `AVP_ERR_IO` (message = `str(e)`); `shsm_pe_exc_class(code)` names them |

Kept from the Python on purpose: the dictionary orders (`named` built from the public records, later duplicates
replacing the value in place, then `setdefault` per procedure record in first-insertion order; `exact` keyed by VA,
last record wins), sorting by `(va, name)` and `(lo, hi, module)` tuples (names compared in code point order: UTF-8
byte order), function sizes as the distance to the next named symbol capped at the end of the first module range
holding the start, exact sizes from procedure records, `module_at(va) or '?'` (an empty module name also becomes
`?`), Python's negative list index for segment 0 (`secs[-1]`), slices clamped and single-byte reads raising
IndexError, the last section holding the debug directory winning. Names are decoded as latin-1 and held as UTF-8.

Provenance: written for this project. that project's `core/src/pe.c` / `core/include/avp/pe.h` (commit
`49e811503d7ba5c9cff92160557df057c50a8ab3`) were reviewed and not taken: they clone `pefile` (lazy parsing,
pefile's errors and clamping), while these two scripts read the headers with raw `struct.unpack_from` calls whose
bounds and order decide the error a damaged file gives.

## Tests

`tests/shsm_pe_test.c` (CTest `shsm_pe_test`, label `lib`): the port of `tools/mwcc_trace/test_symbols.py` (five
known Wii/1.0 functions through lookup/describe/find, an interior offset, a static, the function count 3680, the
NoDebugInfo of a non-PE image; skipped without the compiler or under `SHSM_NO_REAL_TREE`), plus `_strip` and
`version_of` cases checked against the Python, a short file (struct.error text) and a missing file (OSError text).
The differential harnesses of `mwcc_symbols` and `pe_disasm` run the module over every compiler in
`tools/compilers`.
