# codecmp.exe: allowed differences from `python tools/codecmp.py`

Proven equal to the retired Python tools/codecmp.py by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/codecmp.exe` (tools/c/tools/codecmp) is the command line of tools/codecmp.py over the shared comparison
library (`cc_codecmp`, `cc_listing`, `cc_elf`; their deviations are in `tools/c/DEVIATIONS.md`, `D-C*`). It prints
the Python's stdout byte for byte and exits with its codes.

## Deviations

## D-codecmp-1. No tracebacks
- Behaviour: where the Python ends with an uncaught exception (a missing retail listing: `FileNotFoundError`; a
  missing or unreadable object: `subprocess.CalledProcessError` from `dtk elf disasm`), the C tool prints one line,
  the exception text, on stderr. Both exit with code 1 and a non-empty stderr.
- Reason: there is no Python stack to print; the harness compares the stderr class.

## The retail DOL is read with the shared reader (shsm_dol)
- Behaviour: `Project.dol` parses orig/R5WEA4/sys/main.dol with `shsm_dol_parse` and reads it with
  `shsm_dol_read_within`. A file shorter than 224 bytes stops with `<path>: not a DOL` as before; a file of 224 to 255
  bytes, or one whose section runs past the end of the file, now stops with `<path>: not a DOL (<reason>)`, where
  codecmp.py read the short header or returned the truncated bytes.
- Reason: one DOL reader for the C tools; only a damaged DOL behaves differently, and the hash contract rules that
  out for the retail one (harness 1062/1062).
