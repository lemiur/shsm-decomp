# Deviations of the C tools from the Python tools

Every known behaviour difference between a C tool and the Python tool it replaces is listed here as a numbered
entry. A difference not listed here is a bug.

This file takes no new entries: a shared-library module's deviations go in `lib/<module>.DEVIATIONS.md`, a tool's in
`tools/<name>/DEVIATIONS.md` (README.md, "Deviations").

Format, one entry per difference:

    ## D<n>. <tool>: <short title>
    - Behaviour: what the C tool does differently (input, output, exit code, stderr).
    - Reason: why the difference is accepted.

(No entries yet.)

## D-C1. cc_* (tools/codecmp.py, tools/common/compare.py): malformed objects and listings
- Behaviour: where the Python raised an uncaught exception on malformed input (an ELF string-table or section index
  out of range, a truncated section-header, symbol or relocation table, a relocation symbol index past the symbol
  table in `Context.check`, `int(x, 0)` of a decimal immediate with leading zeros in `raw_operand_check`, a
  `names` override naming no retail function in `codecmp.compare_object`, a listing byte column holding tabs), the C
  code reads what is there (empty names, tables cut at the end of the data), treats the value proof as absent, or
  stops with an internal error (`AVP_ASSERT`).
- Reason: only a broken object or listing gives these inputs; mwcceppc and dtk never write them. Valid input gives
  the same results (differential: every unit under build/R5WEA4/src, tools/c/tests/diff/codecmp_diff.py).

## D-C2. cc_* : regular-expression character classes
- Behaviour: Python's `\s` and `\w` are Unicode classes; the C matchers use ASCII whitespace plus `\x1c`-`\x1f` for
  `\s` and count every byte >= 0x80 as a `\w` character.
- Reason: listings, symbols.txt and splits.txt are ASCII; the classes agree on ASCII.

## D-B1. shsm_units (tools/common/units.py): no build/index/units.json
- Behaviour: `load_units` neither reads nor writes the on-disk unit-table cache `build/index/units.json`; the C
  tools parse objdiff.json, configure.py and build.ninja themselves (cached in the process, keyed on their size and
  mtime as the Python's in-process cache). The unit table is the same; only the Python's side-file is not touched.
- Reason: parsing takes milliseconds in C; the file is a Python start-up optimisation, and leaving it alone keeps the
  Python's own cache (and its version key) under the Python's control. No output differs.

## D-B2. shsm_mangle, shsm_symbols: names are bytes
- Behaviour: demangling, `\S` / `\d` tests and length prefixes (`<len><name>`) count bytes and ASCII digits, where
  the Python counts code points and Unicode digits (`int('٣')` is 3 in Python).
- Reason: symbols.txt, splits.txt and the mangled names are ASCII (every name of the real symbols.txt demangles the
  same: tests/shsm_real_test.c). Whitespace tests do use Python's Unicode whitespace.

## D-B3. shsm_cli: --debug prints no traceback; huge --limit
- Behaviour: `shsm_cli_main` prints the same one-line error as cli.main but no traceback under `--debug` (there is
  none). A `--limit` beyond 64 bits is taken as no practical cap (the Python keeps the exact big int).
- Reason: a traceback is the Python interpreter's, not the tool's output; a line cap above 2**63 caps nothing in
  either.
