# unit: accepted differences from tools/unit.py

Proven equal to the retired Python tools/unit.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 801 cases, 0 differ; 32 writing cases, 0 differ.

## D-unit-1. No asm index cache file
- Behaviour: unit.py pickles its asm index to `build/tmp/unittool/asm_index.pkl` (check and new write it); the C
  parses the current listings on every run and writes nothing.
- Reason: the C parse of the 60 MB of listings takes about 0.8 s, the same as reading a cache of the blocks in
  avp/cache.h format (tried: 0.85 s with the cache, 0.80 s without), so a cache only adds a file. Output does not
  depend on the cache (harness).

## D-unit-3. Order of repeated `--include-data` / `--exclude-data` names
- Behaviour: unit.py turns the names into `set`s, so with two or more names their processing order (the order of
  `other unit:` lines, the insertion order behind ties) follows Python's per-process string hashing; the C uses the
  command-line order with repeats dropped.
- Reason: Python's order is not reproducible between two runs of unit.py itself; with one name (every documented
  use) the outputs are equal.

## D-unit-4. Uncaught exceptions
- Behaviour: where unit.py dies with a traceback (a data-only unit whose shared objects reach `gap_hint` with no
  .text range: `TypeError`), the C prints `Traceback (most recent call last):`, a placeholder line and the
  exception line on stderr, exit 1; stdout is the same.
- Reason: the stack is Python's; the stderr class and exit code are kept.

## D-unit-5. Decoding of child output
- Behaviour: unit.py decodes `git status` and the gate/rules output with the locale encoding (`text=True`); the C
  decodes UTF-8 with replacement.
- Reason: the outputs are ASCII paths and gate lines; identical in every harness case.

## D-unit-6. ASCII `\w` in the hand-written asm scanners
- Behaviour: the asm block parser (`unit_asm.c`) matches unit.py's `TOK_RE`, `DECL_RE` scope word and `HDR_RE`
  section name with ASCII word characters, where Python's `\w` also takes Unicode letters.
- Reason: dtk's listings are ASCII; the `check` and `new --dry-run` sweeps over every split give identical output.
  Every other pattern goes through shsm_re (Python semantics).

## D-unit-7. Output order around `--regen`
- Behaviour: as in unit.py, ninja writes to the inherited stdout while the tool's own lines sit in its stdout buffer
  and follow at exit; the C buffer (AVP_MAIN) and Python's 8 KiB buffer differ in size, so an output longer than
  8 KiB before ninja runs would interleave differently.
- Reason: `new` prints far less than that before regen (writediff `new regen` cases equal).

## Post-retirement changes (after the Python tools were retired)

## D-unit-asm-1. dtk's `pad_` functions in an assembly source unit
- Behaviour: `check` (and so `flip`) leaves dtk's generated `pad_NN_XXXXXXXX_<section>` functions of a `.s` unit out of the `functions in the retail asm but not in symbols.txt` comparison and prints `[OK] assembly source unit: dtk's generated <names> cover(s) bytes the source assembles`; a C or C++ unit still reports them as missing.
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); dtk covers the TRK vector table's bytes with `pad_00_80004000_init`, which no `symbols.txt` entry can name (a `type:function` entry for the table is duplicated by dtk's label pass and breaks the link).
