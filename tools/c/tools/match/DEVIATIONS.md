# match.exe: allowed differences from `python tools/match.py`

Proven equal to the retired Python `tools/match.py` by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/match.exe` (tools/c/tools/match) gives the Python's stdout, exit codes, stderr lines, `--json` documents,
state files (`build/tmp/match/state/*.json`, same keys, order, number formatting and CRLF line ends) and cache
entries (same keys: the two tools share the compile and listing cache). The shared library's own deviations are in
`tools/c/DEVIATIONS.md`.

## Deviations

- **D-M1 timings.** Every figure measured with a clock differs: the `compile/compare/total` seconds of the summary
  line and of `--json`, `COMPILE FAILED (N s)`, the `N s with -j N` of a multi-unit run, the top-level `seconds`
  of a multi-unit `--json` document, and the `time` field of a state file. The harness normalises them.
- **D-M2 cached flag.** Whether a compile came from the cache depends on which tool ran first (they share the
  cache): ` (cached)` in the summary line and `"cached"` in `--json` can differ between two otherwise identical
  runs. The harness normalises them.
- **D-M3 work directory names.** `--keep` keeps `build/tmp/match/work/<unit>-<random>`; the random part differs (as
  it does between two Python runs).
- **D-M4 threads for processes.** A multi-unit run analyses units on a thread pool of `min(-j, units)` threads
  (the Python used a process pool of that size); each unit's analysis runs on its own arena, freed once its result
  is copied. Results, their order and the output are the same.
- **D-M5 no tracebacks.** `--debug` prints no traceback (there is none); the error line and exit code are the
  Python's (shared-library deviation).

## Not ported

- The `hooks` / `workdir` test seams of `analyse_unit`.

## Post-retirement changes (after the Python tools were retired)

## D-match-asm-1. Assembly source units are left out
- Behaviour: a unit whose source is `src/<unit>.s` (`shsm_unit_is_asm`) is left out of the units the tool selects by status, library or change; named directly it fails with `unit <name> is an assembly source unit (...): assembled by binutils as, not compiled` (lib `D-shsm_units-1`).
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); MWCC never compiles such a unit, so there is nothing to compile and compare.
