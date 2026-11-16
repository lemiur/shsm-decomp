# slot: accepted differences from tools/slot.py

Proven equal to the retired Python tools/slot.py by the differential harness, removed at retirement, commit 66ee84d3.

Lock files are the same JSON document (keys
`owner, lane, slot, time, epoch, pid, host, ttl_hours` in that order, one line, `json.dump` separators, `pid` the
caller's process), so the two tools honour each other's locks.

## D-slot-1. Tracebacks
- Behaviour: where the Python lets an exception escape (a lock file holding a JSON list, string or number; an
  `epoch` or `ttl_hours` that `float()` rejects; a non-string `lane`; any error under `--debug`), the C tool prints
  `Traceback (most recent call last):`, a line saying the port prints no stack, and the exception line
  (`TypeError: ...`, `ValueError: ...`, `ToolError: ...`) on stderr, with the Python's exit code 1. The Python prints
  its full stack.
- Reason: a stack of Python frames has no C counterpart. stdout, the exit code and the stderr class are the same
  (slotdiff scenario sc_corrupt_locks; cases.txt `--debug` cases).

## D-slot-2. Default --repo
- Behaviour: without `--repo`, the C tool uses the checkout that holds its executable (`<checkout>/build/tools/`),
  else the checkout holding the current folder, else the current folder.
- Reason: an executable has no source file location; for the normal layout (`build/tools/slot.exe` of a checkout, or
  a build tree elsewhere run from a checkout) both name the same checkout (every cases.txt case relies on it).
