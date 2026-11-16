# gate: accepted differences from tools/gate.py

Proven equal to the retired Python `tools/gate.py` by the differential harness, removed at retirement, commit 66ee84d3. Last run: 143 stub cases, 0 differ; 9 plain cases, 0 differ.

Unit tests: the 46 tests of tools/test_gate.py are ported (`tests/driver_test.c` 12, `pending_test.c` 6,
`steps_test.c` 19, `helpers_test.c` 9, each file starting with its `Python test -> C test` map), plus 4 tests of the
merge_lane / score_history helpers in `gate_regress.h`; 50 tests, all passing (`ctest --preset tools -L gate`).
Differences in how they test, none in what:
- `test_main_exit_codes`: the C test reads the final line from `build/gate/last.json` instead of captured stdout.
- `test_check_tools_is_doctor_check_required`: `tools/doctor.py` is a stand-in file (D-gate-1).
- `test_remove_tree_refuses_links`: a junction instead of a symlink (no symlink privilege needed; the gate refuses
  both).
- `test_quick_*`: `load_units`, `classify_falls` and `load_module` are mocked through `GateQuick`'s hooks and a
  stand-in `tools/merge_lane.py`.
No test was left out.

## D-gate-1. The pin table is the C doctor's
- Behaviour: the preflight's tool check calls the C doctor's `doctor_check_required` (linked, `tool_doctor`) with the
  gate's runner. The Python loaded `tools/doctor.py` from the checkout and called its `check_required`; the C gate
  neither reads nor requires any `.py` file for it.
- Reason: the doctor port is the one pin table of the C tools (its own harness proves it equal).

## D-gate-3. gatecheck and ledger run as the C executables; the regression helpers are linked
- Behaviour: the Python ran `tools/gatecheck.py` and `tools/ledger.py` from the tree copy (`build/gate/tree/tools`)
  and imported the tree's `tools/merge_lane.py`, `tools/score_history.py` and the checkout's `tools/gatecheck.py`
  (selftest). The C gate runs `gatecheck.exe` and `ledger.exe` beside it (with `--root <tree>`, as the Python did)
  and links the ported functions; it requires none of those `.py` files.
- Reason: the ports are the logic; the files were the Python's implementation detail.

## D-gate-4. --debug prints no traceback
- Behaviour: with `--debug`, an unexpected exception is still `gate: error: <Class>: <message>` and exit 2; the
  Python re-raised it (a traceback and exit 1).
- Reason: there is no traceback in C (cli.main's `--debug` has the same note in `shsm_cli.h`).

## D-gate-5. Ctrl-C
- Behaviour: an interrupt ends the process with the console's interrupt status; the Python caught
  KeyboardInterrupt and returned 130 (after its `finally` removed the lock, which the C gate then leaves behind; the
  next gate refuses it for 4 hours, the Python's stale-lock rule, with the message naming the file to delete).
- Reason: an interactive interrupt is not part of the gate's contract; the lock refusal says what to do.

## D-gate-7. configure.exe instead of `python configure.py`
- Behaviour: the build step runs `configure.exe --compilers <checkout>/tools/compilers` in the tree and `--quick`
  runs `configure.exe` in the checkout, where the Python ran `python configure.py` with the same arguments.
  `configure.exe` is the one beside the gate (`build/tools/configure.exe`), or `--configure-exe PATH`; it executes the
  tree's `configure.py` (tools/c/tools/configure/DESIGN.md) and writes a `build.ninja` whose tool steps run the C
  tools beside it. The failure line names the command `configure` (`configure exited N`, the Python:
  `configure.py exited N`). `--quick` treats a changed `configure.py` as reaching every unit (the Python also counted
  `tools/project.py`, which no longer exists).
- Reason: the build plumbing is C; `configure.py` stays the unit table.

## D-gate-8. The rules step links the class scanner and the rule checker
- Behaviour: the rules step runs the class scan of `tool_classes` (always the constructor/destructor base rule, the
  scanner's default by the owner's ruling of 2026-11-13; `SHSM_CLASSES_BASE_RULE` is ignored) and `tool_check_rules`'
  `rl_run` (`check_rules --json` with `--root <tree>`) in process; `--quick` runs the same checker with
  `--no-summary <changed src files>` in the checkout. The Python ran `python tools/classes.py scan` (the slot-overlap
  rule) and `python -I tools/rules/check_rules.py --json`. Under the ctor rule the checker can report R13 findings the
  slot-overlap rule does not (on the real tree of 2026-11-14: 44 R13 findings against 49, the five `AreaTriggerBase:
  fn_800D... cannot be placed` findings exist only under the slot-overlap rule; no blocking difference). A failure of
  the scan is `classes scan exited N` (the Python: `classes.py scan exited N`); an internal error of either tool is a
  short `gate.exe: error:` text with the exception class and message and exit 1, read as a
  failed run, as a crashed Python process was.
- Reason: a C tool links the C ports instead of running them; the ruling makes the ctor rule the intended result.

## D-gate-9. score_history's index is linked
- Behaviour: the regression classification indexes the lane and onto sources with score_history's linked
  `sh_index_source` (`gate_index_linked`); the Python imported the tree's `tools/score_history.py` and called its
  `index_source`. An exception inside the index gives `not classified: <message>` on both sides.
- Reason: `sh_index_source` is proven equal to the Python's `index_source` on every source file of the tree
  (D-score_history-1); merge_lane uses the same index (D-merge_lane-5).

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `gate.exe: error: <Class>: <message>`, with the same exit code as before.

## Post-retirement changes (after the Python tools were retired)

## D-gate-asm-1. A unit's source path may be `.s`
- Behaviour: when the report gives no `source_path`, the regression step's path guess tries `src/<unit>.s` after `.cpp`, `.c`, `.cp` and `.cc`.
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule).
