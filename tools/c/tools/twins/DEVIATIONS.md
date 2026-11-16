# twins: accepted differences from tools/twins.py

Proven equal to the retired Python tools/twins.py by the differential harness, removed at retirement, commit 66ee84d3.

No observable differences. The fingerprint database each language writes is equal row for row to the other's
(schema text, `fingerprints` and `meta`, stamp included).

Internal differences that cannot be seen from outside:
- The fingerprints are computed on a thread pool (the Python: a process pool); results are inserted in work order, as
  the Python's ordered `pmap` does, so `insert or replace` keeps the same row for a repeated address.
- `families --min-count 0` (or less) with an empty family: the Python's sort key reads `ms[0]` and fails with
  IndexError; the C raises the same message (`list index out of range`, exit 2) from an internal ValueError code;
  the printed line is the same (cli.main prints `str(e)` either way).
- A listing that fails to parse during a rebuild raises in the worker; the C reports the first failing file's error
  (in work order) after the pool finishes, the Python whichever its pool re-raises first.

## Header for other tools

`twins.h` (static library `tool_twins`; link with `LIBS tool_twins`): `twins_open_db` (twins.open_db),
`twins_load` / `Twins` / `TwinsFn` (the Twins model: groups per level, `twins_of`, `twins_resolve`,
`twins_operand_diffs`, `twins_lib_matches`, `twins_fn_json`), `twins_fingerprint`, `twins_normalise_row`,
`twins_compute_stats`, and the subcommands writing into a `TwinsOut` (an open cli.Output, or collected lines and data).
