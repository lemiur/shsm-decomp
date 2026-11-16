# pick.exe: allowed differences from `python tools/pick.py`

Proven equal to the retired Python tools/pick.py by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/pick.exe` (tools/c/tools/pick) gives the Python's stdout, exit codes and stderr lines for every mode
(`lost`, `close`, `near`, `fresh`, `pending`, `drafts` and the summary) with every option (`--lib --unit --min-size
--max-size --report --threshold --small --leaf --tiny --checker --root --json --all --limit --debug`). The unit tests
(`tests/pick_test.c`, label `pick`) port all 23 tests of tools/test_pick.py (plus a `pct` case) on the same synthetic
checkout (`tests/fixture_data.h`, generated from tools/common/_fixture.py and tools/test_pick.py).

Timings on the real tree (Windows 11, warm cache, best of two, seconds, Python then C): summary 3.00 / 0.50, `lost`
2.39 / 0.37, `close` 0.54 / 0.04, `near` 0.32 / 0.05, `fresh` 0.75 / 0.25, `pending` 0.33 / 0.04, `drafts`
0.34 / 0.04, `pending --checker` 2.31 / 1.81.

The library `tool_pick` (`pick.h`) also carries what tools/status.py imports from pick: the world model
(`pick_world_new`, `PickFn`), `pick_lib_matches`, `pick_reference_flag`, `pick_pct` and `pick_pending_records`.

## Deviations

## D-pick-1. Score history in process
- Behaviour: the lost section calls score_history's C port in process (`score_history_run`, library
  `tool_score_history`) with the argument lists the Python passed to `tools/score_history.py` (`--repo R --db D
  status --lost --json`, then the text form). The Python's note `history unavailable (tools/score_history.py
  missing)` has no counterpart (the port is linked, never missing), nor has the 300 s timeout or the
  `history unavailable (cannot run ...)` note of a Python that cannot start.
- Reason: a C tool never shells out to a Python tool (tools/c/README.md). The JSON-then-text fallback and its
  `history unavailable (score_history exit N: <first line>)` note are kept (tests `json_then_text_fallback`).

## D-pick-2. Standard output encoding
- Behaviour: the C tool writes stdout as UTF-8. The Python writes it in the console code page when stdout is a
  pipe and PYTHONIOENCODING is unset: a non-ASCII character of a `pending` note (copied from docs/exceptions.md)
  gives other bytes there.
- Reason: as D-ledger-2. The harness runs the Python with `PYTHONIOENCODING=utf-8`; every other text pick prints is
  ASCII (`fresh` strings go through json.dumps, which escapes).

## D-pick-3. Inputs that are not the tools' own formats
- Behaviour: where malformed inputs made the Python fail with a TypeError/AttributeError (a sweep.json whose
  `results` is a string or an object, a score_history row whose `symbol` or `unit` is not a string, a rule checker
  `functions` entry that is not an object), the C tool skips the value or prints its `str()`. A sweep result that is
  not an object raises `'<type>' object has no attribute 'get'` (exit 2) in both. A `--min-size`/`--max-size` beyond
  64 bits is an argparse error in C (`invalid size_arg value`); Python accepts it.
- Reason: draft.py, score_history and check_rules always write their schemas; the harness covers a sweep.json that
  is not JSON and one that is a JSON list (both agree).

## D-pick-4. An index folder that cannot be created
- Behaviour: when `build/index` exists as a file, the modes that open the asm index (`fresh` and the summary) stop
  with `pick.py: error: unable to open database file` (exit 2); the Python stops in `os.makedirs` with
  `pick.py: error: [WinError 183] Cannot create a file when that file already exists: '<root>\\build\\index'`
  (exit 2). Same exit code and error class; the message is shsm_asmindex's (lib/shsm_asmindex), not pick's.
- Reason: a broken checkout, not a state the tools produce; the difference belongs to the asm index module and is
  left to it. The harness keeps `close` on such a checkout (no index opened, equal) and leaves out the two modes.

Not a deviation (checked by hand, recorded because the harness cannot show it): on a checkout with no
`build/<ver>/asm` and no index, the first `fresh` (or summary) raises `no retail asm at ...` in both languages (exit
2, or the summary's `asm index unavailable: ...` note) and leaves a schema-only index behind; every later run of
either language then lists the functions with `? ins` and no callers. The harness runs the Python side first, so its
`noasm-*` checkouts start from that schema-only state (diff/cases.py).
