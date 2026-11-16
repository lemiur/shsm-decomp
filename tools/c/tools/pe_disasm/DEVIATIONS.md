# pe_disasm: accepted differences from tools/mwcc_trace/pe_disasm.py

Proven equal to the retired Python tools/mwcc_trace/pe_disasm.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-pe_disasm-1. Its own index cache file
- Behaviour: the index (calls, refs, function starts) is cached in `build/tmp/pe/<exe base name>.idx.bin`
  (`avp/cache.h` format: kind `pe_disasm_idx`, schema 1, the exe's `os.path.getmtime` as the stamp, written
  atomically) instead of `build/tmp/pe/<exe base name>.idx.pickle`. The rule is the Python's: the cache is used
  when the stored mtime equals the exe's, else the index is rebuilt and the file replaced. A damaged or truncated
  file is rebuilt (the Python's `except Exception: pass`).
- Reason: pickle has no C reader. The two caches never read each other's file, and the index they hold is the same
  (harness: every index command over every compiler). The key is still the base name, as the Python's (every
  compiler is `mwcceppc.exe`, so alternating compilers rebuild it: under a second in C).

## D-pe_disasm-2. Uncaught exceptions are a two-line traceback
- Behaviour: where the Python dies with an uncaught exception (a missing or unreadable exe, a truncated header:
  struct.error, a PE without sections: ValueError from `min()`, a regex `re.compile` refuses, a VA `int(arg, 16)`
  refuses, `--max` without a value or with a bad one, `callees` at an address with no decodable instruction:
  IndexError), pe_disasm.exe writes `pe_disasm.exe: error:` and `<Class>: <message>` on stderr and
  exits 1, after the same stdout lines.
- Reason: the rest of a traceback is the interpreter's. stdout, the exit code and the stderr class are the Python's
  (harness: the error cases and the `@bad` inputs).

## D-pe_disasm-3. Values beyond 64 bits
- Behaviour: a VA or `--max` whose value does not fit in a signed 64-bit integer ends the run with an
  OverflowError traceback (exit 1); the Python goes on (finding nothing at that address).
- Reason: no address of a 32-bit image is that large.

## D-pe_disasm-4. NB11 names are loaded only when a command can print them
- Behaviour: the Python loads the compiler's NB11 table (symbols.py) before every command, `strings` included, and
  swallows any failure; the C tool loads it (through lib/shsm_pe) only for xref, func, callers and callees.
- Reason: `strings` never prints a name and a load failure is silent in both, so the output is the same.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `pe_disasm.exe: error: <Class>: <message>`, with the same exit code as before.
