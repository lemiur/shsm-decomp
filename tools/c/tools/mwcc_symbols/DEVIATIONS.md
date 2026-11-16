# mwcc_symbols: accepted differences from tools/mwcc_trace/symbols.py

Proven equal to the retired Python `tools/mwcc_trace/symbols.py` by the differential harness, removed at retirement, commit 66ee84d3.

## D-mwcc_symbols-1. Uncaught exceptions are a two-line traceback
- Behaviour: where the Python dies with an uncaught exception (a truncated or damaged exe: struct.error or
  IndexError; an `--addr` that `int(s, 0)` refuses: ValueError; an empty function table with `--addr`: IndexError;
  `--check-all` meeting a damaged exe; `--dump` failing to create or write its file), mwcc_symbols.exe writes
  `mwcc_symbols.exe: error:` and `<Class>: <message>` on stderr and exits 1, after the same stdout lines.
- Reason: the rest of a traceback is the interpreter's (file paths, line numbers). stdout, the exit code and the
  stderr class are the Python's (harness: the `@bad` inputs and the refused `--addr` literals).

## D-mwcc_symbols-2. --addr values beyond 64 bits
- Behaviour: an `--addr` literal whose value does not fit in a signed 64-bit integer (`0x10000000000000000`) ends
  the run with an OverflowError traceback (exit 1); the Python prints `0x...  no symbol` and goes on.
- Reason: no address of a 32-bit image is that large; the C table keeps addresses as `i64`.

## D-mwcc_symbols-3. Output encoding of non-ASCII names
- Behaviour: stdout is UTF-8; the Python writes to a pipe in the locale's code page. The `--dump` file is written in
  the ANSI code page by both, so it is identical.
- Reason: no compiler under tools/compilers has a non-ASCII byte in a module or function name (checked on the
  nine NB11 dumps); the harness compares every name of every NB11 compiler through `--dump`.

## D-mwcc_symbols-4. The checkout root
- Behaviour: the C tool takes `ROOT` (default `--exe`, `--check-all`'s tools/compilers, the `--dump` folder and the
  `wrote` line) from `avp_root()` (`AVP3_ROOT`, else the checkout above the executable), made absolute with
  backslashes; the Python takes it from its own file's location.
- Reason: both name the same checkout when run from it (harness: the `wrote E:\...\build\mwcc_symbols\<v>.tsv`
  lines are identical).

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `mwcc_symbols.exe: error: <Class>: <message>`, with the same exit code as before.
