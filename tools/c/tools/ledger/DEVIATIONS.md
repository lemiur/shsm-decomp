# ledger.exe: allowed differences from `python tools/ledger.py`

Proven equal to the retired Python `tools/ledger.py` by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/ledger.exe` (tools/c/tools/ledger) gives the Python's stdout, exit codes and stderr lines for
`progress` (every option: `--report --ledger --out --check --diff --diff-lines --add-markers --force
--checker-json --root`) and `exceptions-check` (`--report --checker-json --root --exceptions --strict`), and writes
the same bytes (line-ending style of the ledger kept). The unit tests (`tests/*.c`, label `ledger`) port all 37
tests of tools/test_ledger.py (helpers_test.c: the 6 TestHelpers tests and test_pure_order_when_no_table_exists;
progress_test.c: the other 17 TestProgress tests; exceptions_test.c: the 13 TestExceptionsCheck tests).

Timings on the committed tree (Windows 11, warm cache, best of three, seconds, Python then C):
`progress --check` 3.4 / 4.1, `exceptions-check` 5.2 / 3.9.

## Deviations

## D-ledger-1. Default repository root
- Behaviour: without `--root` the C tool takes the checkout that holds the current directory (`shsm_find_root`),
  else two folders above the executable; the Python took the checkout of `tools/ledger.py` itself.
- Reason: an executable built into another directory (`build/tools-cmake-*/build/tools/`) has no fixed place in a
  checkout. Run from inside a checkout (as every caller does) both name the same root, as the same absolute path
  (the `not found in <ledger>` and `<ledger> is up to date` lines print it); the harness runs every case that way.

## D-ledger-2. Standard output encoding
- Behaviour: the C tool writes stdout as UTF-8. The Python writes it in the console code page when stdout is a
  pipe (cp1252 here, no UTF-8 mode): a non-ASCII character in `progress --out -`, in a `--diff` line or in an
  `exceptions-check` finding (a token quoted from docs/exceptions.md) gives other bytes, and a character outside
  cp1252 makes the Python stop with UnicodeEncodeError (exit 1) where the C tool prints it.
- Reason: the project's tools print UTF-8 (`AVP_MAIN`); files written by both tools are UTF-8 and identical.
  docs/progress.md is ASCII and none of the 5 non-ASCII characters of docs/exceptions.md reaches the output of any
  harness case.

## D-ledger-3. No tracebacks; inputs that are not the tools' own formats
- Behaviour: where the Python leaves an exception uncaught (a missing or unreadable `--report`, `--checker-json` or
  `--ledger`, a file that is not JSON or starts with a BOM, a report path on another drive than the root), it
  prints a traceback and exits 1; the C tool prints only the traceback's last line (`FileNotFoundError: [Errno 2]
  No such file or directory: 'x'`, `json.decoder.JSONDecodeError: Expecting value: line 1 column 2 (char 1)`) and
  exits 1. The harness cuts the Python's traceback to that line (`normalize` in diff/cases.py) and compares the
  rest exactly. A message longer than about 1000 characters (a very long path) is cut (the try/throw buffer).
  JSON that parses but is not an objdiff report or check_rules output (a key missing, a list where an object
  belongs, a size that is not a number) made the Python fail with KeyError/TypeError/AttributeError (exit 1); the C
  tool treats a missing key or wrong type as absent (an empty string, no list) and carries on, except for sizes and
  addresses that are not integers (`ValueError: invalid literal for int() ...`, exit 1).
- Reason: there is no traceback to print in C; the error class, message and exit code are the Python's. The
  inputs are produced by objdiff and the rule checker, which always give the schema; `{}` as a report or checker
  output (both cases in the harness) agrees.

## D-ledger-5. Unicode digits in the scanned patterns
- Behaviour: the hand scanners that stand in for `\d` (the size column of ROW_RE, THUNK_RE `@\d+@`, the
  `(N functions, M bytes)` notes, `R\d+` rule ids, the inline `N functions and M code bytes` total) accept ASCII
  digits only; Python's `\d` also takes other Unicode decimal digits.
- Reason: every one of these fields is written by the tools or by hand in ASCII; no such digit occurs in the
  ledger or the exceptions record. `\w` of the source-word scan and `re.I` of UNCONFIRMED_RE are exact (Unicode
  alphanumerics; the dotted and dotless i).

## The rule checker runs in-process

- Without `--checker-json`, `progress` and `exceptions-check` run the C rule checker in-process (`tool_check_rules`,
  `rl_checker_json`: `check_rules --json --root <root>`). An exit status other than 0 and 1, and output that is not
  JSON, give `ledger: error:` lines. The reading of a failed run is the shared `rl_checker_json_of`, which status's
  unit test covers with a substituted run.

## Consolidation (hand scanners for regular expressions)

No regex engine exists yet (`shsm_re` is another lane's). Each pattern of ledger.py is a hand scanner in
`ledger_scan.c` that follows the pattern's backtracking order; move them to `shsm_re` when it lands:
- `unit_key`: `\.(c|cc|cp|cpp|h|hpp)$` with re.I;
- `parse_configure`: `Object\(\s*(Matching|NonMatching|Equivalent)\s*,\s*"([^"]+)"`;
- `parse_symbols`: `^(\S+) = (\S+?):0x([0-9A-Fa-f]+);(?: //(.*))?$` and `size:0x([0-9A-Fa-f]+)`;
- `ticks`: `` `([^`]+)` ``;
- `PLACEHOLDER_RE`, `THUNK_RE`, `UNCONFIRMED_RE` (re.I), `ROW_RE`;
- `parse_status_cell`: `^(.*?)(?: \((.*)\))?$`;
- `build_model`: `R\d+`;
- `splice`: the `<!-- ledger:begin NAME -->(.*?)<!-- ledger:end NAME -->` markers (re.S) and
  `\d+ functions and \d+ code bytes(?= in all)`;
- `source_words`: `[A-Za-z_@\\][\w\\@]*`;
- `cmd_exceptions_check`: `\((\d+) functions?, (\d+) bytes\)`.

## Post-retirement changes (after the Python tools were retired)

## D-ledger-asm-1. Assembly source units are ORIGINAL-ASM throughout
- Behaviour: unit keys drop a `.s` extension as they drop `.c`/`.cpp` (configure entries, exceptions.md rows, checker paths); `.s` files are read for source identifiers; in `progress` every function of a `.s` unit is ORIGINAL-ASM (linked) or `ORIGINAL-ASM when linked` (parked); `exceptions-check` needs no R18 hit for an ORIGINAL-ASM row of a `.s` unit and reports each function of a Matching `.s` unit missing from the ORIGINAL-ASM table; dtk's generated `pad_NN_XXXXXXXX_<section>` function of a `.s` unit (the TRK vector table) needs no `symbols.txt` entry.
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); the rule checker reads no `.s` file, so there is no R18 evidence to require.
