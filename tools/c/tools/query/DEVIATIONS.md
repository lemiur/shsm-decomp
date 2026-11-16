# query: accepted differences from tools/query.py

Proven equal to the retired Python tools/query.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-query-1. Standard output encoding
- Behaviour: a non-ASCII character in text output (for example `notes "über"`) is written as UTF-8 by the C tool; the
  Python writes the console code page (cp1252) when its stdout is a pipe.
- Reason: the project-wide stdout convention (README "Output and exit-code conventions", D-ledger-2); `--json` output,
  which escapes non-ASCII, is identical (harness cases `notes "über" --json`, `notes "→" --json`).

## D-query-2. Integers wider than 64 bits
- Behaviour: `parse_int` reads at most 64-bit integers: `addr 0x123456789ABCDEF0123` fails with "not a number: ..."
  where the Python fails later with sqlite's "Python int too large to convert to SQLite INTEGER" (exit 2 and a
  one-line error on both sides; only the message differs). find-const reproduces its own "not a 32-bit value"
  message exactly.
- Reason: no address or constant of the binary needs more than 32 bits; the error class and exit code are the same.
