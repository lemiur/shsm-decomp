# wall: accepted differences from tools/wall.py

Proven equal to the retired Python tools/wall.py by the differential harness, removed at retirement, commit 66ee84d3.

Entries for the command-line half (single form, scan, notes, walls.json, the prover). The classifier's
entries (wall_classify.c, wall_rows.c, wall_cfg.c) are recorded by their files' authors below these.

## D-wall-2. scan -j runs threads in three phases, not a process pool
- Behaviour: the Python's `cli.pmap(scan_unit, ..., processes=True)` runs each unit's compile, comparison,
  classification and proofs in one worker process. The C tool compiles and compares the units on a thread pool
  (min(jobs, units) threads, each unit in its own build/tmp/wall/obj/<unit path>/ directory as the Python), then
  classifies every unit on the main thread in unit order, then runs the proofs on the pool (one task per unit, its
  proofs one after another on one CalleeLiveness, as the Python's worker ran them: its memo makes answers depend on
  the order of queries), and applies them in order. symbols.txt is loaded once, not once per unit. Output,
  walls.json and exit codes are the same; an exception from the classifier surfaces for the first unit in order, as
  `pool.map` raised it. The per-unit `seconds` the Python's workers measured is not kept (it never reaches the output
  or walls.json).
- Reason: the classifier is not required to be thread-safe; the expensive steps (compiles, dtk listings, proofs)
  still run in parallel.

## D-wall-3. A malformed compiler message line is skipped
- Behaviour: in `cli.first_error` (a failed compile's message), a `#` line after a block's Error marker that the
  message pattern `^#\s+...` does not match (a `#` followed directly by text) raised AttributeError in the Python
  (the compile error then became a crash, exit 2 with "'NoneType' object has no attribute 'group'"); the C tool skips
  the line and keeps reading the block.
- Reason: CodeWarrior always indents its message lines; the Python's behaviour is a defect, not a contract.

## D-wall-4. load_walls on a hand-made walls.json of the wrong shape
- Behaviour: a document whose `inputs` is not an object, or whose per-unit record's `files` is not an object, made
  the Python raise AttributeError from `.get` / `.items()`; the C reader treats the member as absent (the unit is
  then stale or current by its other members).
- Reason: wall scan never writes such a document; the reader must not crash the briefing tool that calls it.

## D-wall-5. A compiled float constant whose bytes run past its section
- Behaviour: in `describer`'s compiled side, a 4- or 8-byte `@` symbol in `.sdata2`/`.rodata` whose bytes lie
  (partly) outside its section's data made `struct.unpack` raise struct.error in the Python (the whole
  classification then crashed); the C describer keeps the kind (`float` / `double`) with the value null. Likewise a
  `_read_string` / `_looks_string` offset outside the section reads nothing instead of slicing with Python's
  negative-index rules.
- Reason: mwcceppc never writes such an object; the classifier must not crash on a referent description.

## Post-retirement changes (after the Python tools were retired)

## D-wall-asm-1. Assembly source units are left out
- Behaviour: a unit whose source is `src/<unit>.s` (`shsm_unit_is_asm`) is left out of the units the tool selects by status, library or change; named directly it fails with `unit <name> is an assembly source unit (...): assembled by binutils as, not compiled` (lib `D-shsm_units-1`).
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); MWCC never compiles such a unit, so there is nothing to compile and compare.
