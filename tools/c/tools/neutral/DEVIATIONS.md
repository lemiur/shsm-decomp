# neutral: deviations from tools/neutral.py

Proven equal to the retired Python tools/neutral.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 22 cases, 22 same.

Unit tests: `tests/neutral_test.c` (the 9 tests of `tools/test_neutral.py`, plus `first_error` and `safe_name` for two C helpers).

## D-neutral-2. --debug prints no traceback
- Behaviour: with `--debug`, an unexpected exception is still `neutral: error: <Class>: <message>` and exit 2; the
  Python re-raised it (a traceback and exit 1).
- Reason: there is no traceback in C (`shsm_cli.h`, as D-gate-4).

## Post-retirement changes (after the Python tools were retired)

## D-neutral-asm-1. Assembly source units are left out
- Behaviour: a unit whose source is `src/<unit>.s` (`shsm_unit_is_asm`) is left out of the units the tool selects by status, library or change; named directly it fails with `unit <name> is an assembly source unit (...): assembled by binutils as, not compiled` (lib `D-shsm_units-1`).
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); MWCC never compiles such a unit, so there is nothing to compile and compare.
