# dolphin.exe: allowed differences from `python tools/dolphin.py`

Proven equal to the retired Python tools/dolphin.py by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/dolphin.exe` (tools/c/tools/dolphin) gives the Python's stdout, exit codes, stderr lines, `--json`
documents and session files (`build/dolphin/session.json`, `journal.jsonl`, `broker.json`, `dolphin.json`, same keys
and order). The shared modules' deviations are in
`tools/c/lib/shsm_net.DEVIATIONS.md`, `shsm_gdb.DEVIATIONS.md`, `shsm_gdbfake.DEVIATIONS.md` and, for the common
command line (`--debug` prints no traceback), `tools/c/DEVIATIONS.md` D-B3.

Values that come from the clock or the operating system (process ids, ports, creation stamps, the `started` and
journal times, measured seconds) differ from run to run in either language and are not deviations.

## D-dolphin-1. launch starts the C broker
- Behaviour: `launch` spawns `dolphin.exe _broker ...` (the running executable) instead of
  `python tools/dolphin.py _broker ...`; session.json's `broker_exe` is therefore the path of dolphin.exe, and the
  broker's log lines in `build/dolphin/broker.log` come from the C broker.
- Reason: the C tool must not shell out to a Python tool (tools/c/README.md). The two brokers speak the same
  protocol and write the same `broker.json`: the stub cases pass with the Python broker serving both command lines
  and again with the C broker serving both (97 of 97 cases each), so a session started by either tool is driven by
  the other.
