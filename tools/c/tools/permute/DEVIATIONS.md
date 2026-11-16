# permute.exe: allowed differences from `python tools/permute.py`

Proven equal to the retired Python tools/permute.py by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/permute.exe` (tools/c/tools/permute) gives the Python's stdout, exit codes and stderr class, and writes
the same files (base.c / base.cpp, target.s, target.o, compile.sh, settings.toml, the base's object and listing)
byte for byte. The source stripper (`permute_strip.h`) is its own module for the tools that
import it from permute.py.

**decomp-permuter is not ported.** `--run` starts `permuter_run.exe` (beside permute.exe), which runs the venv's Python in
`tools/permuter`, with the same command line and working directory as the Python tool (it is a third-party Python program
and stays one). As in Python, nothing is flushed before it starts: the permuter's output can come before this tool's
buffered lines when stdout is a pipe.

## Deviations

## D-permute-1. No tracebacks
- Behaviour: where the Python ends with an uncaught exception (no build.ninja or retail listing, a compiler or dtk
  that does not start or fails, an unreadable object, a brace the stripper cannot close), the C tool prints one line,
  the exception's text, on stderr. Both exit 1 with a non-empty stderr.
- Reason: there is no Python stack; the harness compares the stderr class.

## D-permute-2. Quoted function names (a reference defect, fixed here)
- Behaviour: dtk quotes a function name that is not an identifier, for the thunks MWCC emits for virtual functions
  (`.fn "@36@__dt__10FMVTriggerFv", global` in build/R5WEA4/asm/game/FMVTrigger.s; 39 units have such blocks). The
  Python's `^\.fn (\S+?),` keeps the quotes, so `permute.py game/FMVTrigger @36@__dt__10FMVTriggerFv` exits 1
  ("function ... not in the unit's retail asm"), and with the quotes typed it fails later, because `source_names`
  reads the `@36@` prefix as the name (`no definition of ['"@36@__dt'] ...`); `retail_fn` cannot find the quoted
  block either. The C tool unquotes the listing's names (and a quoted argument), finds the quoted `.fn`/`.endfn`
  block, and takes a thunk's source name from the symbol after its `@N@` prefix (`~FMVTrigger`), so the thunk is set
  up and scores exact.
- Reason: a defect of the reference (reported for the Python fix). Tests: `strip_test.c`
  `thunk_names_are_their_targets`, `permute_test.c` `resolve_quoted_thunk`. The harness generator skips quoted names
  until the Python has the same fix.

## D-permute-3. Compiler output is passed through as bytes
- Behaviour: the compiler's messages (a failed preprocess, compile or assembly) are written as the bytes the
  compiler printed, with `\r\n` and `\r` turned into `\n`; the Python decodes them as cp1252 and re-encodes them.
- Reason: the same bytes for every ASCII message (every source in the tree is ASCII); the Python would raise on
  the five bytes cp1252 leaves undefined.

## D-permute-4. Non-ASCII identifiers
- Behaviour: the stripper's scanners take every byte >= 0x80 as a word character (Python's `\w` decides per code
  point).
- Reason: preprocessed sources are ASCII (checked over src/ and include/).
