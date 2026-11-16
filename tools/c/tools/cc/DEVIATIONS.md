# cc: accepted differences from tools/mwcc_trace/cc.py

Proven equal to the retired Python tools/mwcc_trace/cc.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-cc-1. Uncaught exceptions are one stderr line
- Behaviour: where the Python dies with an uncaught exception (traceback on stderr, exit 1), cc.exe writes one line
  `<Class>: <message>` on stderr and exits 1: the compiler or dtk cannot start (`-v` naming a compiler directory that
  does not exist: FileNotFoundError), `dtk elf disasm` fails (CalledProcessError; the C message names the dtk path
  only, the Python's names the whole command list), the retail listing cannot be read (`--asm` naming a missing
  file), a `--map` value without `=` (ValueError), a `-f` value or the unit's cflags that do not shlex-split
  (ValueError), a build edge without `mw_version`/`cflags` (AttributeError). stdout and the exit code are the
  Python's; the stderr class (non-empty) is the same.
- Reason: the traceback text is the interpreter's (file paths, line numbers), not the tool's output. A bad `--map`
  is reported after the compile, before the listings are read (the Python: after reading them); both orders end in
  exit 1 with nothing more on stdout.

## D-cc-2. Compiler output decoding
- Behaviour: the compiler's stdout/stderr (printed on a failed compile) are decoded as UTF-8 with replacement; the
  Python's `text=True` decodes with the locale code page, strictly.
- Reason: mwcceppc writes ASCII (its messages, and the source paths of this checkout); the harness's compile-failure
  cases are identical. Only a non-ASCII path or source line in an error message could differ.

## D-cc-3. build.ninja is parsed as the Python parses it, not through the unit table
- Behaviour: none (a note): `unit_flags` is reproduced (first `build` block whose head holds `: mwcc` and
  `src\<unit>.`, first `mw_version`/`cflags` line of that block) instead of `shsm_load_units`, so `-u` accepts exactly
  what the Python accepts (also a path that is not an objdiff unit). On the 350 units with source of the current
  build.ninja both give the same compiler version and cflags.
- Reason: identical results by construction.
