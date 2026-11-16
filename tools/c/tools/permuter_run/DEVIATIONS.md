# permuter_run: accepted differences from tools/mwcc_trace/permuter_run.py

Proven equal to the retired Python tools/mwcc_trace/permuter_run.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-permuter_run-1. A native launcher of a third-party Python program
- Behaviour: `permuter_run.exe` does not run decomp-permuter itself: it starts `<root>/tools/venv/Scripts/python.exe
  -c <bootstrap> <root> <arguments...>` (permuter_run.h, `PERMUTER_RUN_BOOTSTRAP`) in the current directory with its
  own standard handles, waits, and exits with the child's exit code. The bootstrap is the script statement for
  statement: `sys.path[0]` = `<root>/tools/mwcc_trace` (the script's directory), `<root>/tools/permuter` inserted
  first, `src.main.preprocess` replaced by a UTF-8 file read, `src.main.main()`; `sys.argv[0]` is set to
  `permuter_run.py` so argparse prints the same `usage: permuter_run.py ...` and error lines. The Python script
  ran in whatever interpreter started it (permute --run used the venv's); the C tool always uses the venv
  of the checkout it runs in.
- Reason: decomp-permuter is a third-party Python program (tools/permuter, pinned commit, not ours to port); this is
  not a bridge to one of the project's Python tools (README, "never shell out to a Python tool"). Evidence:
  diff/cases.txt (help, usage errors, missing directories, real runs on build/permute/abs that compile base.c and stop
  at score 0) all same.

## D-permuter_run-2. A missing venv is a one-line error
- Behaviour: when `<root>/tools/venv/Scripts/python.exe` does not exist the tool prints
  `permuter_run: error: <path> not found (...)` on stderr and exits 2; when it cannot be started,
  `permuter_run: error: cannot start <path>: <Windows error>`, exit 2. The script has no such case (it is already
  running in some Python); permute.exe --run checks the venv before starting it.
- Reason: the C tool must find its interpreter; exit 2 is the project's environment-error code.

## D-permuter_run-3. Process tree and Ctrl+C
- Behaviour: the Python child runs in a kill-on-close job (it and the permuter's worker processes end when
  permuter_run.exe ends, e.g. when a caller kills it on a timeout); Ctrl+C and Ctrl+Break are left to the child (the
  launcher's handler only declines to die, so the permuter's KeyboardInterrupt handling prints its summary and the
  launcher returns its exit code).
- Reason: a launcher must not orphan the program it starts nor exit before it; under the script there is one process
  and no such question.
