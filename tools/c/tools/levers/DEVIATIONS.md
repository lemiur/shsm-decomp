# levers: accepted differences from tools/levers.py

Proven equal to the retired Python `tools/levers.py` by the differential harness, removed at retirement, commit 66ee84d3. Last run: 37 cases, 0 differ.

## D-levers-1. Exception text of an unexpected failure inside the lever generators
- Behaviour: a Python exception other than ParseError, KeyError and IndexError raised while generating levers
  (a TypeError or AttributeError from an input the parsers do not expect, or an exception out of `mangle.parse`) is
  printed by `cli.main` as `levers.py: error: <str(e)>`. The C raises the same failure as its own exception class
  (`LV_EXC_TYPE`, `LV_EXC_ATTR`, `LV_EXC_DEMANGLE`) and prints `levers.py: error: <message>` with the C message,
  which need not be the Python's `str(e)` word for word.
- Reason: exit code (2) and stderr class are the same; the Python's message is CPython's own wording for an internal
  error (no source of the tree or of the harness reaches one: gen_check parses every real function on both sides
  without one).

## D-levers-2. Malformed build/walls.json
- Behaviour: `load_walls` gives `{}` for every walls.json the Python's `wall.load_walls` raises on (a non-dict
  function record, a list as `inputs` or as an input record, a list unit, `functions: null`, ...), as the Python does.
  Two shapes differ: a non-empty JSON object as `units` or `functions` (the Python iterates its keys; the C treats the
  file as malformed and gives `{}`), and a truthy `wall` that is not a string (the Python prints it, e.g. `1 wall`; the
  C ignores the record).
- Reason: the wall classifier writes lists and strings there; neither shape can come from it. The valid and the other
  malformed shapes were compared on both sides (13 walls.json variants x 3 functions, all equal).

## D-levers-3. Encodings of non-ASCII text outside the source
- Behaviour: `--show` writes the diff (read as latin-1) to stdout as UTF-8; the Python writes it in the stdout
  encoding, which is UTF-8 only under `PYTHONIOENCODING=utf-8` (the harness's setting) and the locale code page
  otherwise. A unit or `--file` path with characters outside latin-1 goes into the diff headers as UTF-8 bytes; the
  Python's latin-1 file write raises UnicodeEncodeError on it.
- Reason: the C tools write UTF-8 (AVP_MAIN); the tree's paths are ASCII. Source text itself is latin-1 bytes on both
  sides (one byte one character), so every candidate, diff and offset is the same.

## D-levers-4. Which candidates a time budget skips
- Behaviour: when `--minutes` runs out in the middle of a level, the candidates already started finish and the rest
  are counted as `skipped (budget)`; which ones depends on the scheduling of the compile threads, on both sides.
- Reason: timing, not behaviour. The harness's budget cases are the deterministic ones (the deadline passed before the
  first level, and the batch's `left <= 5` skip).

## Post-retirement changes (after the Python tools were retired)

## D-levers-asm-1. Assembly source units are left out
- Behaviour: a unit whose source is `src/<unit>.s` (`shsm_unit_is_asm`) is left out of the units the tool selects by status, library or change; named directly it fails with `unit <name> is an assembly source unit (...): assembled by binutils as, not compiled` (lib `D-shsm_units-1`).
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); MWCC never compiles such a unit, so there is nothing to compile and compare.
