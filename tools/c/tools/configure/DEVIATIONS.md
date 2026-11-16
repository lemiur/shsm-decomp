# configure: accepted differences from `python configure.py`

Proven equal to the retired Python configure.py plus tools/project.py by the differential harness, removed at retirement, commit 66ee84d3.

The generator's own deviations and the definition of `--emit c` are in `tools/c/tools/project/DEVIATIONS.md`.

## D-configure-1. Constructs outside the subset are refused
- Behaviour: a statement, expression, built-in or module outside DESIGN.md section 1 (class, while, try, with,
  lambda, comprehensions, float literals, augmented assignment, set displays, slices, f-string format specs,
  decorators, global, keyword-only parameters, `import os`, an unknown name such as `print`, ...) stops configure.exe
  with `configure: error: <script>:<line>: <what>` and exit code 2; Python runs it. The whole file is parsed before
  anything runs, so a syntax form outside the subset fails before any output.
- Reason: the design rule (DESIGN.md section 1): an edit to configure.py outside the subset must fail loudly, never be
  misread. `diff/cases_unsupported.txt` shows each of 16 forms refused at the line that holds it.

## D-configure-2. Python exceptions are reported as subset errors
- Behaviour: what raises an exception in Python (NameError, AttributeError, TypeError, KeyError, IndexError, the
  ValueError of `list.index` or `Path.with_suffix`, a SyntaxError) is `configure: error: <script>:<line>: <message>`
  with exit 2 in configure.exe, where Python prints a traceback and exits with 1.
- Reason: configure.py raises none of them on any argument set; the stderr class (non-empty) is the same, and the
  message names the line. `sys.exit` itself is Python's: the message on stderr and exit 1, an int as the code, None 0
  (the sys.exit cases of the table fixture).

## D-configure-3. `type=str.upper` with `choices`
- Behaviour: the core argparse (`avp/args.h`) accepts choices on string types only, so for a `type=str.upper`
  argument with choices configure.exe gives the converter the choices (it checks them and raises argparse's own
  `invalid choice: 'X' (choose from ...)` message) and passes the metavar argparse would format from them (`{R5WEA4}`).
  Help, usage and errors are argparse's text (harness cases `-h`, `-v XX`, `-v r5wea4`). The one difference: argparse
  converts a string default but does not check it against the choices; the C converter would reject a default outside
  the choices.
- Reason: configure.py's default is `VERSIONS[DEFAULT_VERSION]`, always one of the choices. No change to the shared
  core was needed.

## D-configure-4. No Python warnings
- Behaviour: Python's compile-time warnings (SyntaxWarning: `is` with a literal, invalid escape sequences) are not
  printed.
- Reason: warnings are not part of the result; configure.py raises none (the table fixture was written without them).

## D-configure-5. `--dump` and the own options
- Behaviour: `--script FILE`, `--dump`, `--emit c` (accepted and ignored) and `--` before the script's arguments
  belong to configure.exe (DESIGN.md section 3); `--dump` replaces `generate_build` / `calculate_progress` by the JSON
  document `diff/dump_config.py` defines. Another `--emit` value or a missing option value is `configure: error: ...`
  with exit 2.
- Reason: the front end of the design; `python configure.py` has no such options. The first argument that is not one
  of them (even `-h`) starts the script's arguments, so `configure.exe -h` is `python configure.py -h`.

## D-configure-6. A missing script
- Behaviour: `configure: error: cannot read <script>` and exit 2; Python prints `can't open file '<abs path>': [Errno
  2] No such file or directory`, also exit 2.
- Reason: same exit code and stderr class; the message is not parsed by anything.

## D-configure-8. Lexer limits inside f-strings
- Behaviour: an f-string replacement field that contains the f-string's own quote character (legal since Python 3.12,
  PEP 701, e.g. `f"{d["k"]}"`) ends the string at that quote, which then fails as a subset error (D-configure-1), and
  `\N{...}` escapes are refused.
- Reason: configure.py uses neither; the failure is loud, never a misreading.

## D-configure-9. Object identity and iteration
- Behaviour: `is` / `is not` compare the interpreter's objects (None, True and False are shared; ints from -5 to 256
  compare by value, as CPython caches them; equal strings are never identical unless they are the same object, where
  CPython interns some); `for` iterates a snapshot of a list (appends inside the loop are not visited);
  `dict.items()` / `.keys()` / `.values()` return lists; `str()` of an instance is refused (Python prints its address).
- Reason: configure.py uses `is` only with None and never changes a list it iterates; the dumped state is equal on
  every harness case.

## Real-tree proof of the C plumbing

With no Python on PATH: `build/tools/configure.exe` then `tools/bin/ninja.exe` ends with `build/R5WEA4/main.dol: OK`
(the regeneration after `dtk dol split` runs `configure.exe`, and no edge names a Python file), and the C gate, which
runs the `configure.exe` beside it, passes every step (400 units).
