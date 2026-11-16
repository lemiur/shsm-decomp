# draft: accepted differences from tools/draft.py

Proven equal to the retired Python tools/draft.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: every case the same.

Every subcommand is ported (phase 1a: lexer, source scanning, substitution map, parser; phase 1b: World, brief's
infer_profile, the class evidence and the `twin` command; phase 2: `grade`, `reject`, `rejects`, `packet`; phase 3:
`select`, `sweep`, `promote`). Entries are numbered `D-draft-<n>`.

## D-draft-1. Text offsets are bytes
- Behaviour: the Python's token, definition and statement offsets (`levers.Tok.s/e`, `scan_source`'s
  `start/end/banner`, `top_level_statements`' spans, `class_block`'s span) count code points of a `str`; the C's
  count bytes of the UTF-8 text. Every use slices the same text with them, so the slices are the same strings; the
  numbers differ only after a non-ASCII character. `levers.match_brackets`' error message "unbalanced ')' at
  offset N" reports the byte offset. `scan_source`'s 40-character look-ahead for a trailing address comment counts
  code points, as the Python does. `top_level_statements` blanks comments with one space per code point, as the
  Python does (continuation bytes are dropped from the statement text).
- Reason: the offsets never reach the output (they only slice and order); every `src/` and `include/` file is
  ASCII today (cross-check: identical results on all 738 files).

## D-draft-2. A digit-like character that is not a decimal digit
- Behaviour: the Python's tokenizer starts a number at any `str.isdigit()` character (also a superscript such as
  `²`, or `.` followed by one) and then fails on `_NUM.match` returning None (AttributeError, a traceback). The C
  starts a number only where `\d` matches (Python's `\d`: Unicode decimal digits) and otherwise lexes the
  character as an operator token.
- Reason: the Python's outcome is a crash on input no source file holds; the C keeps scanning.

## D-draft-3. levers.ParseError is DRAFT_EXC_PARSE
- Behaviour: an unbalanced bracket raises `DRAFT_EXC_PARSE` with the Python's message (D-draft-1 for the offset).
  The Python's ParseError is a plain Exception draft.py does not catch, so cli.main prints `draft.py: error:
  <message>` and exits 2; shsm_cli_main does the same with the C's exception (a grade of a draft with an unbalanced
  bracket is a differential case).
- Reason: an exception class of its own, so no caller mistakes it for a usage or OS error.

## D-draft-5. The plan memo holds the whole unit_plan state
- Behaviour: `World.unit_of` asks `unit_plan.plan_for` for one plan per listing range (as the Python's `_plans`), and
  `memo_plan_model`'s effect, one load of unit_plan's model per process and cache stamp, is kept by holding
  `up_load_cached`'s state (model, evidence, truth labels, calibration) per (root, `up_cache_stamp`) for the process;
  the C reads unit_plan's own cache `build/index/unit_plan.cbin` where the Python unpickles `unit_plan.pickle`
  (D-unit_plan-3). The plans, and so every output, are the same.
- Reason: the C's `up_plan_for` loads and recomputes the evidence and calibration on every call; the memo keeps the
  Python's cost profile (select asks for about a hundred plans). `up_cache_stamp` is an additive export of
  unit_plan's cache stamp for this key.

## D-draft-7. The grader's rule check runs the C checker
- Behaviour: `Grader.rules` runs `python -I tools/rules/check_rules.py --json --no-summary --root <root> <paths>` and
  decodes its stdout; the C runs the checkout's `build/tools/check_rules.exe` with the same arguments (cwd the root,
  timeout 600 s) and decodes its stdout the same way: the match-tier findings, the R13 note when it says R13 was
  skipped, and, when stdout is not JSON, the first line of stderr (else stdout) as the rules error. R13: the C
  checker reads `build/R5WEA4/classes.json` as the Python checker does and treats a classes.json written by either
  scanner as current (the stamp question of gate's DEVIATIONS, "the scanners' stamps agree", concerns which scanner
  writes the file, not how the checker reads it), so R13 runs or is skipped on both sides alike.
- Reason: the C port of check_rules is proven equal on its own harness. An earlier version of this port ran the
  checker's library in process; a checker run holds the whole source tree and classes.json (about 850 MB at its
  peak), and the memory of each run stayed allocated, so a full sweep ran out of memory after a few dozen grades.
  A process per grade, as the Python has, returns it. `cli.first_error` is ported in the draft directory (the same
  port as neutral's `neutral_first_error`) rather than linking neutral's library for one function.

## D-draft-8. select's cache stamp lists data inputs only
- Behaviour: `select_inputs` stats the tables select reads (symbols, splits, libraries, rejects, configure.py, the
  reports, config.json, classes.json, sweep.json and the asm, twins, hashes and unit_plan caches); no script of the
  tool is stamped (the C has none).
- Reason: the cache is invalidated by its data; the executable is not an input.

## D-draft-9. promote runs the C tools
- Behaviour: the Python's Promoter ran its sibling scripts and ninja through its interpreter; the C runs the checkout's native executables `build/tools/unit.exe`, `names.exe`, `match.exe`,
  `gatecheck.exe`, `check_rules.exe`, `neutral.exe` with the same arguments, and `tools/bin/ninja.exe` by its
  absolute path from the checkout root (as `ninja_argv` does). The printed step names and plan text name the C tools
  (`unit.exe new --dry-run: ok`, `match.exe main/...`, `FAILED at gatecheck layout: ...`); the text after
  `FAILED:` is the last output line of the C tool, which equals the Python tool's where that tool's port is proven.
  The Promoter's `unit_of` and
  `get_unit` are function pointers of the C Promoter (the Python's tests patch `World.unit_of` and
  `units.get_unit`; tests/promote_test.c stubs them the same way).
- Reason: the user's ruling (tools in C): every one of these tools has a proven C port.

## D-draft-10. sweep -j N: one World per pool thread, the calling thread included
- Behaviour: the Python grades on `cli.pmap` threads, each opening its own World; the C grades with
  `avp_pool_map`, whose calling thread takes part and grades with the World that drafted the targets, the pool
  threads each with their own. Results keep the targets' order on both sides. A grade that raises is a FAIL with
  reason `grade: <str(e)>` and a draft that raises `draft: <first line of str(e)>`; str(e) is the C's message for
  the same exception class (shsm_exc_str), equal wherever the C raises the Python's text.
- Reason: the World's asm index connection is per thread in both; which thread grades does not change a grade.

## Harness: select cases compute on both sides
- `diff/cases_serial.txt` (run with `-j 1`) runs the Python through `diff/py_fresh_select.py`, which removes
  build/draft/select_cache.json before and after the Python's run, so the C computes every select case on no cache
  rather than reading the entry the Python just wrote, and both print `"cached": false`. The cache's sharing (the C
  reading the Python's entry and the reverse, `cached` true on the reading side, the rest of the document equal) is
  proven on the real tree. No output is normalised.

## D-draft-11. sweep's reason pattern keeps the Python's backspace
- Behaviour: cmd_sweep groups the reasons after `re.sub(PATTERN, "N", reason)`, where the Python source's raw string
  starts with a literal backspace character (U+0008, presumably a `\b` that reached the file as the control
  character). The placeholder alternative `(?:fn|lbl)_[0-9A-F]{8}\w*` therefore never matches a reason, and only
  `0x...` and decimal runs become `N` (`missing fn_801DF028` groups as `missing fn_NDFN`). The C uses the same
  pattern, backspace included, so the reason summary is the Python's.
- Reason: a Python defect reproduced: the summary lines and sweep.json's `reasons` are part of the compared output.
  The fix (a word boundary) belongs in both tools at once.
