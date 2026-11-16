# iso9660_extract: accepted differences from tools/scripts/iso9660_extract.py

Proven equal to the retired Python `tools/scripts/iso9660_extract.py` by the differential harness, removed at retirement, commit 66ee84d3. Last run: 160 cases, 0 differ.

The real Ghost Rider disc (`--list`, `--list DATA`, `--extract`, `--boot-elf`) gives byte-identical listings and SHA-1-identical SYSTEM.CNF and SLES_543.17.

- Where the Python dies with an uncaught exception (`FileNotFoundError`, `IndexError` for a missing option value, a
  record shorter than 33 bytes) the C tool writes a two-line `iso9660_extract.exe: error:` message on stderr and
  exits 1; stderr is compared by class.
- Record names are printed as the latin-1 bytes (what Python sends to a cp1252 stdout). Names are compared with
  `str.upper()` on the UTF-8 form of the latin-1 text, which is Python's full case mapping.
- `extracted ... -> ...` echoes the argument as UTF-8, where Python encodes it with the console code page; the two
  agree for ASCII arguments.
- SYSTEM.CNF is read with the ANSI code page and `errors="replace"` (the Python's locale default); bytes outside ASCII
  in it are not exercised.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `iso9660_extract.exe: error: <Class>: <message>`, with the same exit code as before.
