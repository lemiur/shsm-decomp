# flagsweep.exe: allowed differences from `python tools/flagsweep.py`

Proven equal to the retired Python tools/flagsweep.py by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/flagsweep.exe` (tools/c/tools/flagsweep) gives the Python's stdout (matrix, ranking, `--json`), exit
codes and stderr class. It links tool_permute for `cc.unit_flags` (a private port in permute_cc.c until the
compiler-trace lane's `cc` port exists), `permute.find_source` and shlex.

## Deviations

## D-flagsweep-1. Timings
- Behaviour: the seconds in the summary line (`... listing)   2.4s`) and every `"seconds"` of `--json` differ.
- Reason: clocks. The harness normalises them.

## D-flagsweep-2. Scoring runs in the compile pool
- Behaviour: each variant is scored on the thread that compiled it, with its own comparison context (the Python
  scored all variants afterwards on one thread). Results and output are the same.
- Reason: speed; a CcContext belongs to one thread, the project data is shared and locked.

## D-flagsweep-3. No tracebacks; disassembly failures
- Behaviour: an uncaught Python exception (unreadable build.ninja or listing, a shlex quoting error in `--vary`,
  `--set` or `--drop`) is one line on stderr, exit 1, as the Python's traceback. A failed `dtk elf disasm` is
  reported in the variant's error as `disassembly failed: <the library's message>`, not Python's
  `CalledProcessError` text.
- Reason: no Python stack; the harness compares the stderr class; dtk does not fail on objects the compiler wrote.
