# shsm_units: accepted differences from tools/common/units.py

The module predates the per-module files (its earlier entries are `D-B*` in `tools/c/DEVIATIONS.md`). Changes made
after the Python tools were retired are listed here.

## Post-retirement changes

## D-shsm_units-1. Assembly source units
- Behaviour: `shsm_unit_is_asm(u)` (new) is true for a unit whose source is `src/<unit>.s`. `shsm_compile_argv` (and so
  `shsm_compile_source`) raises `unit <name> is an assembly source unit (<source>): assembled by binutils as, not
  compiled` for such a unit, where it raised "has no compile command in build.ninja".
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's `as` rule, never by
  MWCC); match, wall, eqcheck, levers and neutral leave them out of their batches with this test.
