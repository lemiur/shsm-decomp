# merge_lane: accepted differences from tools/merge_lane.py

Proven equal to the retired Python `tools/merge_lane.py` by the differential harness, removed at retirement, commit 66ee84d3. Last run: 111 stub cases, 0 differ; 23 plain cases, 0 differ.

Unit tests: the 83 tests of tools/test_merge_lane.py are ported as CTest tests (`ctest -L merge_lane`).

## D-merge_lane-1. configure runs configure.exe
- Behaviour: the gate step's `configure` command runs `configure.exe` beside `merge_lane.exe` in the slot (it
  executes the slot's `configure.py`, tools/c/tools/configure/DESIGN.md); the Python ran `python configure.py`. Its
  log `gate-configure.log` starts with the executable's command line, and a missing executable gives
  `failed to start: ...` and exit 127.
- Reason: the build plumbing is C; as the gate's D-gate-7.

## D-merge_lane-2. gatecheck and ledger run as the C executables
- Behaviour: the `attribute` and `layout` commands run `gatecheck.exe` and the `ledger_progress` /
  `ledger_exceptions` commands run `ledger.exe`, each found beside `merge_lane.exe` and given `--root <slot>`; the
  Python ran `python tools/gatecheck.py` / `python tools/ledger.py` from the slot. Their logs
  (`gate-attribute.log`, `layout.log`, `ledger-*.log`) therefore start with the executable's command line instead of
  the Python command (`$ <dir>\gatecheck.exe --root <slot> layout ...`), and a missing executable gives
  `failed to start: ...` and exit 127 as a missing Python command did.
- Reason: both are proven ports (their own DEVIATIONS.md); as D-gate-3. The commands' output and exit codes, which
  the steps read, are the ported tools'.

## D-merge_lane-3. check_rules and the class scan are linked
- Behaviour: `rules_changed`, `rules_tree` (both trees) and `classes_scan` call `tool_check_rules` and `tool_classes`
  in process instead of running `python -I tools/rules/check_rules.py --json ...` and `python tools/classes.py scan`.
  The logs hold the JSON document (`rules-changed.json`, `rules-tree-*.json`) as before, without the `$ <command>`
  first line, and `rules-classes.log` holds one line, `classes scan (linked, base rule ctor): <path>`, instead of the
  Python scanner's output. The scan always uses the C scanner's default constructor/destructor base rule (the owner's
  ruling of 2026-11-13; `SHSM_CLASSES_BASE_RULE` is ignored), as the gate's rules step does (D-gate-8); the Python
  scanner used the slot-overlap rule. An internal failure of the linked checker or scanner (a file it cannot read, a lexer
  failure, a path on another drive) is reported as a short `merge_lane.exe: error:` text with the
  exception class and message and exit 1, where the Python process printed its full traceback; the step reads it the
  same way ("no JSON (exit 1): no JSON in output", followed by the text's tail).
- Reason: a C tool does not shell out to a ported tool; the checker is proven by check_rules' harness, the scanner by
  classes'.

## D-merge_lane-5. score_history's index is linked
- Behaviour: the changes step classifies a fall with score_history's linked C index (`sh_index_source`). The Python
  imported `tools/score_history.py` from the slot (the lane's copy) or, failing that, from its own folder, and failed
  the step with "tools/score_history.py not found ..." when neither existed; the C tool needs neither file and never
  runs a lane's own copy.
- Reason: `sh_index_source` is proven equal to the Python's `index_source` on every source file of the tree
  (D-score_history-1); a lane that changes score_history.py goes through the C tool's own review.

## D-merge_lane-6. The default --repo is the checkout holding the executable
- Behaviour: without `--repo`, the repository is found by walking up from the executable's folder to the first
  folder holding both `.git` and `tools` (for `build/tools/merge_lane.exe`, the checkout); the Python took the parent
  of its own folder (`tools/..`).
- Reason: the executable lives in `build/tools/`, not `tools/`; both name the same checkout. `--help` prints the same
  text.

## D-merge_lane-7. Ctrl-C
- Behaviour: an interrupt ends the process with the console's interrupt status; the Python caught
  KeyboardInterrupt, printed `interrupted` and returned 130.
- Reason: as D-gate-5; nothing is left half-written that the Python's handler cleaned up (a rebase interrupted
  mid-way is in progress on both sides, and the next run's preflight refuses it either way).

## D-merge_lane-8. The gate step keeps build/tools and build/tools-cmake
- Behaviour: the clean rebuild empties the slot's `build/` but keeps `build/tools` (the C tools, configure.exe
  among them, and this program when it runs from the slot) and `build/tools-cmake` (their CMake tree); its log
  line is `emptied <build> (kept tools, tools-cmake)`. The Python deleted the whole `build/`.
- Reason: the Python tools lived in `tools/`; the C tools are build outputs of `tools/build_c.cmd` under `build/`,
  and deleting them removes the configure.exe the next command runs (found at retirement: the dry run stopped
  with `could not delete <slot>\build` after removing half of `build/tools`). Test: `gate_keeps_the_c_tools`.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `merge_lane.exe: error: <Class>: <message>`, with the same exit code as before.
