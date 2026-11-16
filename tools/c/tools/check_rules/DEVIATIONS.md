# check_rules: accepted differences from the Python

Proven equal to the retired Python tools/rules/check_rules.py by the differential harness, removed at retirement, commit 66ee84d3.

Stage 1 (lexer, parse model, facts dump): no entries. The facts dump is identical for every file of `src/` and
`include/` and for the synthetic corpus (PORTING.md, "Proof").

Stage 2 (the per-file checks): the findings dump is identical for the whole tree and the corpus (PORTING.md). The
entries below are inputs neither contains.

Not a deviation but a scope note: the command line of `tools/rules/check_rules.py` is not ported yet; the C
executable answers every argument set other than the hidden `--dump-facts` / `--dump-findings` / `--dump-final` with exit 2 and one
line on stderr.

## D-check_rules-1. Decimal literals of 64 bits or more in check_literals
- Behaviour: for a decimal literal at or above about 1.8e19 (where the unsigned 64-bit accumulation would overflow)
  in a bit context, the C skips R9's "decimal mask/flag" test; the Python tests the exact integer (a power of two or
  2^n-1). The other literal checks treat such a value as large, as the Python does.
- Reason: no source has such a literal; MWCC has no integer type that holds one.

## D-check_rules-2. configure.py tokens outside ASCII
- Behaviour: `_py_tokens` (parse_include_dirs) reads a non-ASCII letter outside strings and comments as an operator
  character; Python's `str.isalpha` makes it part of a name.
- Reason: configure.py is ASCII outside its comments and strings; names there are Python identifiers the reader
  compares with ASCII words only (`def`, `return`, `Object`, `extra_cflags`, `libs`).

## D-check_rules-3. Token reads past the end of the stream
- Behaviour: where the Python indexes past the last token and raises IndexError (the run fails), the C reads the
  test as false: `_is_delay_counter`'s body token after a `for` header that ends the stream, `check_function`'s
  `do { } while ( 0` with nothing after the `0`.
- Reason: both need a function body that is not closed, which the parser never records (a function needs its `}`).

Stage 3 (the rest of Checker.run: class model, R13 slot check, cross-file rules, RULE-EXCEPTION, allow list, units,
functions): the final dump is identical for the whole tree (with the real classes.json and without one), the
classes.json variants, 286 scratch trees and the corpus (PORTING.md). The entries below are inputs none contains.

## D-check_rules-4. Mangled names with non-ASCII digits or a `Q0` qualifier
- Behaviour: `_demangle_parts` reads the `Q<n>` count and the length prefixes as ASCII digits and slices bytes; the
  Python's `\d` and `int()` also accept other Unicode digits and slice code points. A `Q0` qualifier makes the C read
  the symbol as not a member; the Python raises IndexError (the run fails).
- Reason: symbols come from symbols.txt and classes.json, which are ASCII and never hold `Q0`.

## D-check_rules-5. classes.json values of unexpected types
- Behaviour: where a message prints a classes.json value (`offset`, `symbol`, a record's `name`) that is a list or
  an object, the C prints it as compact JSON, the Python as its repr. (Stamp fields are compared with Python's
  equality since D-check_rules-11, so `2.0 == 2` holds in both.) Record names that are not strings sort as "" in the C where the Python raises TypeError on a mixed sort.
- Reason: tools/classes.py writes strings, integers and null in those places (the real file was checked).

## D-check_rules-6. The text of an unreadable classes.json
- Behaviour: for a classes.json that is not valid UTF-8 the note says `is unreadable (invalid UTF-8)`; the Python
  quotes the UnicodeDecodeError (`'utf-8' codec can't decode byte ...`). A JSON syntax error gives the same text on
  both sides (json.JSONDecodeError's message, line, column and character position).
- Reason: the file is written by tools/classes.py with json.dump (ASCII); only a damaged file can differ.

## D-check_rules-7. The default --root
- Behaviour: without `--root` the C tool's root is two folders above the executable (`<repo>/build/tools/`), made
  absolute (`os.path.abspath`); the Python's is two folders above the script (`<repo>/tools/rules/`). The `--json`
  document prints it as `root`.
- Reason: both name the checkout the tool belongs to; they are the same path whenever the executable is the build of
  the checkout it is run against (the harness runs both from the same checkout: equal in every case).

## D-check_rules-8. Non-ASCII text in the text report
- Behaviour: the C tool writes the text report as UTF-8. The Python's `print` writes through the locale encoding of a
  redirected stdout (cp1252 under `python -I`): a character outside cp1252 (U+FFFD from an undecodable byte, CJK, ...)
  ends the run with UnicodeEncodeError (a traceback, exit 1, no report), and characters inside it come out as cp1252
  bytes. On a console both print the same text. `--json` is ASCII on both sides (`ensure_ascii`).
- Reason: the Python's failure is a defect, not behaviour to keep; the tree (src/, include/) has no non-ASCII
  character (checked), so no harness case reaches it.

## D-check_rules-9. Tracebacks
- Behaviour: where the Python ends with an uncaught exception (the lexer's AttributeError on U+00A0 in code, a
  malformed classes.json the R13 check trips on, an unreadable scanned file, `os.path.relpath` across drives), the C
  tool writes `rules.exe: error:` and `<class>: <short reason>` on stderr and exits 1, as the Python
  does; the traceback's text differs.
- Reason: the harness compares the stderr class; the exit code and the empty stdout are equal.

## D-check_rules-10. --no-cache and SHSM_RULES_CACHE
- Behaviour: accepted and has no effect: the C tool keeps no per-file cache (PORTING.md, stage 6: its uncached whole-tree
  run, 1.6-1.7 s, is faster than the Python's warm-cache run, 3.6-7.4 s), so every run is the Python's uncached run, and
  it never reads or writes the Python's cache directory.
- Reason: the Python's output is the same with and without its cache (its own tests prove it; the harness runs the
  Python cached by default and with `--no-cache` in some cases: equal).

## D-check_rules-11. classes.json's input stamp is judged by the C reader
- Behaviour: whether the R13 slot checks run on a classes.json is decided by `classes_staleness` of the classes tool's
  library (`classes.h`, linked as `tool_classes`), the comparison classes.exe's readers make, on the paths the file
  records; the notes keep the Python checker's wording (`no input stamp`, `inputs ... are gone`, `cannot be read`,
  `the asm tree` / `the symbols file changed since the scan`). The five stamp values are compared with Python's
  equality, as `cur == old` does. Neither the scanner digest nor the base rule recorded in `inputs.scanner` is
  compared, so a classes.json written by classes.exe (either `--base-rule`) or by tools/classes.py is current for
  the R13 checks exactly when its inputs are unchanged. The Python checker keeps its own copy of the stamp code.
- Reason: one stamp comparison for the C tools (the owner's ruling, 2026-11-13); the verdicts and the texts are the
  Python's for every classes.json the scanners write.

## D-check_rules-12. `--changed` reads git's name lists whole
- Behaviour: `--changed REV` runs `git diff --name-only -z --diff-filter=ACMR REV --` and `git ls-files -z --others
  --exclude-standard` through the shared git module (`shsm_git`, with `core.quotepath=off`) and splits the answers on
  NUL. A changed or untracked source whose path contains a space, or a non-ASCII character, is checked. The Python
  splits git's text output on whitespace, so such a path is cut into pieces that name no file and is silently skipped
  (PORTING.md, "Python oddities"); git also quotes a non-ASCII path there. The error text for a failing git keeps the
  Python's form (`git failed for --changed REV: Command '[...]' returned non-zero exit status N.`), naming the
  Python's command without `-z`.
- Reason: a Python defect fixed. The tree has no source path with a space or a non-ASCII character, so the harness
  cases (`@revs --changed {}`) are equal.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `rules.exe: error: <Class>: <message>`, with the same exit code as before.

## Post-retirement changes (after the Python tools were retired)

## D-check_rules-asm-1. Assembly source files are never read
- Behaviour: `--units` leaves out a unit whose source is `src/<unit>.s` (no `no source file for unit` error), and a `.s` file named as a path is skipped; a directory walk already took only C/C++ sources and headers.
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); such a unit has no C to check.
