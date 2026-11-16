# check_rules C port: handover

Ports `tools/rules/check_rules.py` (4,838 lines) in stages. The port is complete and the Python tool, its tests and the
differential harness (`difftool.py`, `diff/`) were deleted at retirement (commit 66ee84d3); `git show 66ee84d3^:<path>`
recovers them. The `python ...` commands and `diff/` files named below are the historical record of how each stage was
proved, not commands to run now; the live proof is the CTest suite (`tools/build_c.cmd --test`, `ctest --preset tools -L check_rules`),
whose self-test holds the 215 cases of the former `test_check_rules.py`. `tools/c/tools/check_rules/README.md` documents the tool.

## Layout

| file | contents |
|---|---|
| `rules_lex.h/.c` | lexer: `RlTok`, `RlComment`, `RlDirective`, `rl_lex`, `rl_skip_quoted`, `rl_pair_brackets`, `rl_ptext`, the arena vector `RL_VEC`/`RL_PUSH`, `rl_read_source` |
| `rules_parse.h/.c` | parse model: `RlParsed` (`rl_parse`), `_Parser` (static `scope`, `decl`, `build_function`, `knr_definition`, `members`), records, derived facts and helpers |
| `rules_sources.h/.c` | `load_sources` and Checker.parse's project-wide storage macros |
| `rules_facts.c` | `rl_dump_facts`: the facts dump |
| `rules_check.h`, `rules_check.c` | the Checker's state and plumbing (sources, parses, include resolution, closures, status, `add`, run()'s prelude, `check_file`), Finding, shared helpers (`statements`, `parse_decl`, `_cast_info`) |
| `rules_config.c` | `parse_units`, `parse_include_dirs` (`_py_tokens`), `Allow.from_markdown` (all four sets), main()'s config reading |
| `rules_classes.h/.c` | the C++ class model (`parse_cpp_classes`, `SrcClass`, `SrcMethod`), demangling, `template_name`, `load_classes` with the input stamp, `RetailIndex`, `slot_implementation`, `retail_complete_slots` |
| `rules_vtables.c`, `rules_cross.c`, `rules_final.c` | R13 slot check; cross-file R1/R2; `run()`, RULE-EXCEPTION (R0), allow list, units, functions, exit code, the final dump |
| `rules_check_tokens.c`, `rules_check_function.c`, `rules_check_file.c` | the per-file checks (`rules_check.md` maps each to its Python) |
| `rules_check.md` | API of the stage-2 modules, Python-to-C table, the findings dump |
| `rules_lex.md` | API of the three headers, Python-to-C table, the dump's JSON shape |
| `rules_cli.h/.c` | the command line below argparse: `collect_files`, `units_to_files`, `changed_files` (git through `avp_run`), `summarize`, the text report and the `--json` document (stage 4) |
| `main.c` | front end: the Python's argparse command line (`run`), and the hidden `--dump-facts` / `--dump-facts-all` (stage 1), `--dump-findings` (stage 2) and `--dump-final` (stage 3), taken whenever one of them is on the line (`run_dump`) |
| `tests/lex_test.c`, `tests/parse_test.c`, `tests/check_test.c`, `tests/run_test.c`, `tests/cli_test.c` | unit tests (CTest label `check_rules`) |
| `tests/selftest_test.c`, `tests/selftest_tests.h`, `tests/selftest_cases.json`, `tests/gen_selftest.py` | the 215 tests of `tools/rules/test_check_rules.py` in CTest (stage 5): the C driver, the generated test list, the recorded cases, the generator |
| `tests/fixtures/` | hand-written edge cases for the dump comparison (`#if 0`, K&R, asm, C++, CRLF, invalid UTF-8, U+00A0, ...) |
| `diff/dump_facts.py` | the Python reference dump (imports check_rules.py by path) |
| `diff/compare_facts.py` | runs both dumps over all sources (or the files given) and compares them byte for byte |
| `diff/make_corpus.py` | writes the synthetic corpus (source-like strings of `tools/rules/test_check_rules.py`, as .c and .cpp) under `build/tmp/check_rules_corpus/` and lists it with the fixtures |
| `diff/dump_findings.py` | the Python reference findings dump (stage 2); `dump_facts.py --dump-findings` hands over to it |
| `diff/dump_final.py` | the Python reference of the whole run (stage 3); `dump_facts.py --dump-final` hands over to it |
| `diff/make_trees.py` | every Checker `tools/rules/test_check_rules.py` runs, written as a scratch tree under `build/tmp/check_rules_trees/` |
| `diff/cases.txt` | difftool cases of the command line against `python -I tools/rules/check_rules.py` (stage 4) |
| `diff/dump_cases.txt` | difftool cases of the three dumps (stages 1-3; `--cases tools/c/tools/check_rules/diff/dump_cases.txt`) |
| `diff/cases.py` | generators of both files: `@sources`, `@fixtures`, `@corpus`, `@trees`, `@classes`, and for stage 4 `@srcunits[:REGEX]`, `@revs`, `@abspath`, `@edits` |

## Stage 1: lexer and per-file facts (done)

Covers check_rules.py lines 96-195 (the constants the lexer and parser use; the rule tables and placeholder regexes
are for later stages), 196-406 (lexer), 408-1365 (parse model, `_Parser` in full), `Parsed`'s derived facts
`cpp_syntax`, `offset_macros`, `volatile_noted` (with `Checker._volatile_decl_name`, `OFFSET_MACRO_NAME`,
`HEADER_CONST`), the helpers `comments_on`, `has_note`, `func_at`, `no_copy_in_retail`, and `load_sources`.
Not covered: `Parsed.cpp_classes()` (`parse_cpp_classes`, L1591: stage 3), `facts()`/`from_facts()`/`__getattr__`
(the per-file cache: port with the cache).

Proof (0 differences):

    python -I tools/c/tools/check_rules/diff/compare_facts.py          # all of src/ and include/: 740 files, 740 same
    python -I tools/c/tools/check_rules/diff/make_corpus.py > build/tmp/check_rules_corpus.txt
    python -I tools/c/tools/check_rules/diff/compare_facts.py $(tr -d '\r' < build/tmp/check_rules_corpus.txt)
                                                                       # 519 files (504 test strings + 15 fixtures), 519 same
    python tools/c/tests/diff/difftool.py check_rules -j 4             # 759 cases (per file, fixtures, errors), 759 same

The dumps are 22.8 MB per side for the tree. The C dump of the whole tree takes about 2 s, the Python one about 9 s.

## Design

- One arena per parse (`rl_parse(a, ...)`): every record, string and vector of a parse is in it (`RL_VEC` grows with
  `avp_arena_realloc`), so a caller frees a file with `avp_arena_free`; `rl_parsed_free` releases the few heap maps,
  sets and `AvpStrVec`s. Temporary arrays (statement lists, `in_tparams`, K&R parts) use `avp_scratch()` with a
  mark/reset around each use.
- Tokens are stored by value (`RlTok`, 24 bytes) in `RlToks`; their texts are views into the file text (placeholders
  `__ASM_BODY__`, `;`, `{}` are static strings). Python's token identity (`stmt.index(tok)`, `body.index(tok)`,
  `id(body)` keys of `asm_ids`) is pointer identity or the token's index.
- Python lists of tokens built by filtering (`stmt`, `body`, `tail`) are `RlTokPtrs` (pointer arrays); slices of the
  stream are (pointer, count). `pair_brackets`' dict is an `i32` array (-1 = absent).
- Python dicts and sets: `storage_macros` is two `AvpMap`s (the argument and the file's own, own first), `func_names`
  and the derived name sets are `AvpSet`s, `_comments_by_line` is a CSR table (`cbl_start`/`cbl_idx`), K&R's `params`
  dict is a list searched from the end (last assignment wins). Sets are only ever dumped sorted.
- The nested class parser of the Python (`sub = _Parser(...)` sharing the seven lists) is a recursive `scope` call
  on the same `Ps` state. The aggregate record is re-fetched by index after the recursion (its vector may move).
- Regexes that need Python's Unicode semantics use `shsm_re` (directive name, `_IF_ZERO`, `_STORAGE_DEFINE`, the
  include pattern, the note regexes, `OFFSET_MACRO_NAME`, `HEADER_CONST`, `NO_COPY_RE`); `_NUM`, `_ID`, `_OP`,
  `VOLATILE_TYPES` and the all-caps test are hand-matched.

## Python oddities found (kept)

- `decl()` names a virtual member prototype after the token before `(`: `virtual ~B();` is recorded as `B::B`.
- `_find_asm_sites` builds `func_bodies` and `decl()` computes `inner` and `_index_of` results that are never used;
  `masked_funcs` and `func_lookup` are always empty; `VarDecl.in_function` is never set; `_decl_name`'s `[` branch
  is unreachable (the `in "(["` test comes first).
- The agg tail keeps a bracket group's closer, not the group, so `} a[4];` reads `a` then a stray `]` (depth -1).
- A Unicode whitespace character in code (U+00A0) makes `lex` raise AttributeError: the whole run fails.

## Stage 2: the per-file checks (done)

Covers everything `Checker.check_file` runs (L2298-3577: check_tokens, check_identifier/flush_identifiers,
check_r4 with the offset-macro helpers, check_volatile, check_casts, check_directives, check_define, check_comments,
check_asm, check_declarations, check_function with check_expressions, check_locals, check_control, check_condition,
check_literals, and check_function_meta) and the state they read: the source index with main()'s extra paths, the
storage macros, `parse`, include resolution and `closure` (`tu_include_dirs`, `_every_library_dir`,
`resolve_include`), `status_of`, `add`, `tier_of`, `_set_header_languages`, `_volatile_notes`,
`offset_macro_names`, `_macro_bodies`, Finding's `text()`/`to_dict()`, and main()'s readers of configure.py
(`parse_units`, `parse_include_dirs`) and docs/exceptions.md (`Allow.from_markdown`: the two asm sets; `rulings` and
`pending` were ported in stage 3). Rules proven: R2, R3, R4, R7, R9, R11, R13 (the dummy
virtual check), R14, R15, R17, R18 (with check_asm's allow status), R19, R21, R22, R25, R26, R31, R33, R35, R37.

Proof (0 differences; the boundary is run()'s per-file loop: no RULE-EXCEPTION, allow-list, cross-file or R13 slot
pass):

    python -I tools/c/tools/check_rules/diff/dump_findings.py --dump-findings > py.jsonl   # about 13 s
    build/tools/check_rules.exe --dump-findings > c.jsonl                                  # about 2 s
                                       # whole tree, 740 files: 8,278 findings, byte-identical (R3 7, R4 1, R7 3,382,
                                       # R9 37, R11 694, R14 245, R17 286, R18 159 (154 allowed), R19 5, R21 118,
                                       # R25 45, R26 19, R31 324, R33 1,069, R35 1,884, R37 3)
    ... --dump-findings --dump-list <corpus list without nbsp.c>
                                       # the synthetic corpus in one run, 517 files: 539 findings, byte-identical,
                                       # every ported rule present (R2 25, R13 4, R15 2, R22 1, R4 26, ...)
    python tools/c/tests/diff/difftool.py check_rules -j 4
                                       # 770 cases: the 759 stage-1 cases plus 11 stage-2 cases (the tree, four rule
                                       # groups, single files, the corpus twice, the lexer failure, a usage error)

`tests/check_test.c` covers each rule group on small sources, the config readers and Finding. Deviations
D-check_rules-1 to -3 (inputs neither proof contains).

## Python oddities found in stage 2 (kept)

- `check_offset_macro_uses` builds `rev = {c: o for o, c in match.items()}`, which equals `match` (pair_brackets maps
  both ways).
- Dead code: `_cast_info`'s `if nxt.t in ("(",) and False`, `check_condition`'s `if prev == "(" and ...: pass`,
  `check_function`'s unused `status` and `body`, `check_locals`' `if "static" in ty.split(): pass`, `parse_decl`'s
  `qual_words`, `check_r4`'s `base_line`.
- `_is_delay_counter` and `check_function`'s `do { } while ( 0 )` test can index past the token stream (IndexError)
  on unclosed input (D-check_rules-3).
- A self-assignment after a label (`L1: a = a;`) is not reported: the label joins the statement's left side.

## Stage 3: the rest of Checker.run (done)

Covers everything `Checker.run` does after the per-file loop and what it needs: the C++ class model (L1366-1647,
`Parsed.cpp_classes`), `load_classes` with `input_stamp`/`_stamp_key`/`classes_stale` (same notes, including the
missing, unreadable, classless and stale cases), `template_name`, `RetailIndex`, `slot_implementation`,
`retail_complete_slots`, the R13 slot check (`check_vtables` and every helper, L3580-4048), `check_cross_file` with
`_unit_private`, the definition guards, `_compiled_out_with` and `_cast_to` (R1, R2; L4050-4258), `apply_exceptions`
and `_exception_cover` (R0), `Allow.from_markdown`'s `rulings` and `pending`, `apply_allow`, `compute_units`,
`blocking`, `build_functions`, `exit_code`, and `run()` itself (checked_paths with the closure headers). Unit statuses
(`parse_units`, `status_of`, `is_matching_path`) were already ported in stage 2.

Proof (0 differences; `--dump-final` prints checked_paths, every finding with its final status and units, the
exception records, the function records, `r13` and the exit code):

    python -I tools/c/tools/check_rules/diff/dump_final.py --dump-final > py.jsonl     # about 12 s
    build/tools/check_rules.exe --dump-final > c.jsonl                                 # about 5 s
                                       # whole tree with the real (fresh) classes.json: 13,281 lines, byte-identical:
                                       # 8,321 findings (283 allowed, 1 excepted; R13 43 advisory, 585 retail
                                       # vtables), 5 exception records, 4,952 function records, exit 0
    python tools/c/tests/diff/difftool.py check_rules -j 4
                                       # 1,070 cases, 1,070 same: the 770 stage-1/2 cases plus 300 stage-3 cases: the
                                       # whole tree with and without classes.json, a scan whose headers run() adds,
                                       # 286 scratch trees (@trees: every Checker of test_check_rules.py with its
                                       # sources, configure.py, exceptions.md and a freshly stamped classes.json; R13
                                       # added 17, missing 7, order 7, override 5, R1 26, R2 13, R0 6, excepted 4,
                                       # allowed 11, exit 1 in 119), 9 classes.json variants (@classes: stale asm tree,
                                       # stale mtime, stale symbols, both, no stamp, inputs gone, no class list, broken
                                       # JSON, missing; copies under build/tmp/check_rules_classes/, the real file is
                                       # only read), the synthetic corpus in one run (R1 203, R2 85, R13 22, R0 3)

`tests/run_test.c` covers demangling, `template_name`, the class model, R13 slots, `load_classes`' notes, R1/R2,
RULE-EXCEPTION and review rulings. Deviations D-check_rules-4 to -6 (inputs no proof contains).

## Python oddities found in stage 3 (kept)

- `_virtual_sigs` decides which own methods are virtual against the inherited signatures only, then merges; the
  `extend(s for s in lst if s not in sigs.get(name, []))` idiom also drops duplicates inside `lst` (the generator sees
  the list it extends).
- `_compare_subtables`' "none" pair is the accumulated result itself, so every return gives what was gathered so far.
- `_model`'s memo is keyed by class name, not by definition: two definitions of one name share a model.
- `_r13` counts a non-advisory finding under its kind, an advisory one under "advisory" whatever the kind.
- `build_functions` computes `orig_asm` and never uses it; `Allow.pending` is read by no check.
- An exception record gains a `refused` key only when it refused a [match] finding (the JSON shape varies).
- `_compare_subtables` would raise (TypeError) on a vtable group whose `this_offset` is null when the class has own
  vptrs; the real classes.json has null offsets only in classes without own vptrs (239 checked), so it never runs.

## Stage 4: the command line (done)

Covers check_rules.py L4625-4838: `UsageError`, `collect_files`, `units_to_files`, `changed_files`, `summarize`,
`main` (every option: paths, `--units`, `--changed`, `--json`, `--root`, `--tier`, `--no-summary`, `--functions`,
`--allow`, `--configure`, `--classes`, `--no-cache`; argparse's help and errors through `avp_args_parser` with prog
`check_rules.py`; the `--configure`/`--allow` checks; the text report with the exception block, the summary, the
`# MATCH-PENDING ...` lines and the result line; the `--json` document with `hand_reviewed`; exit codes 0/1/2, and 1
with a traceback where the Python raises). `--changed` spawns `git diff` / `git ls-files` itself (`rl_changed_files`,
`avp_run`); it moves to `shsm_git` when that lane lands. `--no-cache` and `SHSM_RULES_CACHE` are accepted and change
nothing (no cache yet: D-check_rules-10); the C tool never touches the Python's cache directory.

Proof (0 differences):

    python tools/c/tests/diff/difftool.py check_rules -j 4
        # 1,074 cases, 1,074 same: the whole tree in text and --json with every report option (--tier, --functions,
        # --no-summary, --no-cache, an abbreviation, --root .); paths (directories, files, a header, include/, files
        # outside src/, a non-source file, duplicates, absolute paths, missing paths, the lexer failure); --units for
        # each of the 350 units with a source file (@srcunits) and the game units again with --json, several units,
        # extensions, missing units; --changed against HEAD, HEAD~1, HEAD~5, main, the commit before tools/rules
        # existed, the root commit, a range and a missing revision, text and --json, and the empty value;
        # --configure/--allow explicit, missing, empty and wrong-file; classes.json real, the 9 variants of
        # @classes (text and --json), missing and a directory; 5 scratch trees with one edit each (@edits: a source,
        # a header, the allow list, a unit's status, classes.json) three ways; the 286 @trees through the command
        # line, text and --json; 10 usage errors and -h/--help
    python tools/c/tests/diff/difftool.py check_rules --cases tools/c/tools/check_rules/diff/dump_cases.txt -j 4
        # 1,070 cases, 1,070 same (the stage 1-3 dumps, after the refactor of rules_final.c's record writers)

Whole tree, text output, this machine (idle): C 1.8-2.1 s (`--json` 1.8-2.2 s); Python `--no-cache` 12.2-13.5 s,
Python with a warm cache 3.6-3.8 s. One unit (`--units main/game/PadRumble`): C 1.2-1.8 s, Python warm 1.4-1.7 s:
the C's floor is reading and macro-scanning every source of src/ and include/ (load_sources), which stage 6 weighs.
`tests/cli_test.c` covers units_to_files, collect_files (sorted walk, plain files, absolute paths, the error), the
text report (summary lines, `--tier`, `--no-summary`) and the JSON document's shape. Deviations D-check_rules-7
(the default root: the executable's checkout) to -10.

## Python oddities and defects found in stage 4

- Under `python -I` a redirected stdout is cp1252: a finding or path with a character outside cp1252 makes the text
  report fail with UnicodeEncodeError (D-check_rules-8; the tree has no non-ASCII text, so it does not occur today).
- `changed_files` splits git's output on whitespace (a path with a space is cut) and mixes repository-relative
  (`git diff`) with cwd-relative (`ls-files`) names; both agree only because root is the repository top.
- `--units` wins silently over `--changed`, which wins over paths; `--changed ''` falls back to the paths.
- The reference command can only be run from the repository root by the harness (its script path is relative), so
  there is no `[cwd=...]` case.

## Stage 5: the self-test in CTest (done)

`tools/rules/test_check_rules.py` stays the reference and is not touched. `tests/gen_selftest.py` imports it by path, runs
all 215 tests under recording wrappers (they still pass in the Python) and writes `tests/selftest_cases.json` (about 800 KB:
what each test fed the Python and what the Python produced) and `tests/selftest_tests.h` (one `SELFTEST(<Class>_<test>, "<Class>.<test>")`
per test). `tests/selftest_test.c` defines the 215 tests (CTest `check_rules_selftest_test`; `--filter Class_test` runs one) and replays the
recorded data through the C library, never through hand-written copies of the assertions:

- **run** (275 recorded `Checker.run` calls): an `RlChecker` built from the recorded sources, scan, unit-status and include-directory
  tables, allow-list sets, classes.json data and note; `rl_checker_run`; the `--dump-final` text (checked paths, every finding with
  status and units, exception and function records, R13 state, exit code) compared byte for byte with the Python's, plus the probe the
  assertions read: `Finding.text()`, `blocking`, per source `is_cpp`, functions, variables and include closure, and the `resolve_include`
  calls the test made. This is a superset of every `hits(...)`, `ck.r13[...]`, `ck.exit_code()` and `ck.functions` assertion.
- **main** (83 recorded `main(argv)` calls, the Cli, R13CommandLine and PerFileCache tests): the recorded tree is written under a scratch
  root, `build/tools/check_rules.exe` is run with the recorded arguments, and its exit code, stdout and stderr are compared with the
  Python's (the root replaced by `@ROOT@` in its plain, slash and JSON-escaped forms). classes.json input stamps are re-made on the replay
  tree and moved the way the Python's stood against its tree (asm tree and/or symbols file differ, or inputs gone).
- **call** (44 direct helper calls): `parse_units`, `parse_include_dirs`, `Allow.from_markdown` (all four sets), `demangle_member`,
  `template_name`, `lex` (tokens with kind and line, comments, directives), `uses_cpp_syntax`, `parse_cpp_classes` (names, parameters, const).

Count: **215 of 215 tests run and pass in CTest.**

- 204 are ported with every assertion covered.
- 6 (all PerFileCache) replay every `main` output (cached and uncached runs print the same, and the C tool has no cache) but not what is
  about the Python cache itself: its stored entries, damaged and emptied entries, invalidation by a changed checker, and
  `RulesCache` in `test_facts_hold_no_tokens`. Their ids: `PerFileCache.test_checker_change_invalidates`,
  `test_cold_warm_and_every_kind_of_edit`, `test_damaged_entries_are_misses`, `test_facts_hold_no_tokens`,
  `test_no_cache_and_env_bypass_write_nothing`, `test_other_checkout_path_hits`.
- 3 are mapped:
  `Config.test_parse_include_dirs_of_the_real_configure` (the C parses the checkout's configure.py: same unit set in both tables, every
  list starts with `include`); `RealSource.test_current_tree` (the smoke test, always on in C: no blocking finding in the live `src/`, the
  library headers among the checked paths; the Python runs it only with `--smoke`); `Style.test_rule_table_is_truthful` (the C tier table
  equals the Python `RULES`; the README and source scan stay Python).
- 2 are skipped, the reason printed by the test: `SelfTestHelp.test_help_names_the_smoke_switch` (tests the Python test script's own
  `--help`) and `R13CommandLine.test_stamp_matches_classes_py` (compares two Python implementations of the stamp; the C stamp is proven by
  the 9 `@classes` harness cases, where stamps made by the Python are read as fresh or stale exactly as the Python reads them).

The Python's `Allow()` carries source `""`, `Allow.from_markdown` the default `docs/exceptions.md`; the recorded `allow_source` keeps both.

    python -I tools/c/tools/check_rules/tests/gen_selftest.py           # regenerate the data (about 10 s); --check prints current or STALE
    build/tools/tests/test_check_rules_selftest_test.exe                 # from the repository root, about 25 s; or ctest -R check_rules_selftest

## Stage 6: the cache decision (done: no cache)

Whole tree, text output, this machine under other sessions' load: C 1.6-1.7 s; Python `--no-cache` 11.5 s; Python with a warm cache
4.2-7.4 s (a cold cached run, which fills the cache, 29 s). The C's uncached run beats the Python's warm run, so the C tool keeps no
cache: `RulesCache`, `_check_file_cached`, `_findings_key`, `store_facts` and `Parsed.facts()/from_facts()` are not ported. `--no-cache`
and `SHSM_RULES_CACHE` are accepted and have no effect (D-check_rules-10); the C tool never reads or writes the Python's cache directory.

## Final state

Every stage is done; nothing remains for the port itself. Outside this tool: `match --rules` can link `tool_check_rules`
(tools/c/README.md) instead of spawning the Python; that is the `match` port's change.

| stage | proof |
|---|---|
| 1 lexer and facts | 740 tree files and 519 corpus files, 0 differ; 759 difftool cases |
| 2 per-file checks | 8,278 findings on the tree and 539 on the corpus, byte-identical |
| 3 run() and the rest | `--dump-final`: 13,281 lines on the tree; 286 scratch trees, 9 classes.json variants |
| 4 command line | 1,074 difftool cases (text and `--json`, every option, `--units`, `--changed`, usage errors) |
| 5 self-test | 215 of 215 CTest tests (above) |
| 6 cache | decided: none |
| final run | CTest 66 of 66 pass (warning-free build, check_rules_selftest_test 215 of 215); difftool check_rules 1,074 cases 1,074 same, 0 differ; dump cases 1,070 same, 0 differ |

Rerun the live proof from the repository root (the Python-reference lines below it were retired with the harness):

    tools/build_c.cmd --test --parallel 6                    # warning-free build (/W4 /WX), then all CTest tests, the self-test among them
    python tools/c/tests/diff/difftool.py check_rules -j 4         # the 1,074 command-line cases
    python tools/c/tests/diff/difftool.py check_rules --cases tools/c/tools/check_rules/diff/dump_cases.txt -j 4    # the 1,070 dump cases
    python -I tools/c/tools/check_rules/diff/compare_facts.py      # stage 1 over src/ and include/ (the corpus: see stage 1)
    python -I tools/c/tools/check_rules/tests/gen_selftest.py --check

Headers other tools link (library `tool_check_rules`, include directory `tools/c/tools/check_rules`): `rules_lex.h` (the lexer: `rl_lex`,
`RlTok`, `RlComment`, `RlDirective`, `rl_pair_brackets`, `rl_read_source`, the `RL_VEC` arena vector), `rules_parse.h` (`rl_parse` and the parse
model: functions, variables, aggregates, prototypes, `rl_uses_cpp_syntax`, derived facts), `rules_sources.h` (`load_sources`), `rules_check.h`
(the Checker: `rl_checker_new`, `rl_checker_from`, `rl_checker_run`, findings, `rl_exit_code`, `rl_blocking`, `rl_dump_final`), `rules_classes.h`
(the class model, demangling, `rl_load_classes`) and `rules_cli.h` (file selection, `rl_report_text`, `rl_report_json`). `rules_lex.md` and
`rules_check.md` document them.

## Python defects found, all kept (the C reproduces them)

- Stage 1: a virtual member prototype is named after the token before `(`, so `virtual ~B();` is `B::B`; dead results in `_find_asm_sites`
  and `decl()`; the aggregate tail keeps a bracket group's closer; a Unicode space (U+00A0) in code makes `lex` raise AttributeError.
- Stage 2: `rev` in `check_offset_macro_uses` equals `match`; dead branches in `_cast_info`, `check_condition`, `check_function`,
  `check_locals`, `parse_decl` and `check_r4`; `_is_delay_counter` and the `do { } while ( 0 )` test can index past the token stream
  (IndexError); a self-assignment after a label is not reported.
- Stage 3: `_virtual_sigs` judges virtuality against inherited signatures only and its merge drops duplicates inside a list;
  `_compare_subtables`' "none" pair is the accumulated result; `_model`'s memo is keyed by class name; `_r13` counts advisory findings under
  "advisory"; `build_functions` computes `orig_asm` unused and `Allow.pending` is read by nothing; an exception record gains `refused` only
  sometimes; `_compare_subtables` would raise on a null `this_offset` with own vptrs (never in the real data).
- Stage 4: under `python -I` a redirected stdout is cp1252 (a finding with a character outside it fails the text report); `changed_files`
  splits git's output on whitespace and mixes repository- and cwd-relative names; `--units` wins silently over `--changed`, which wins over
  paths; `--changed ''` falls back to the paths.
- Stage 5: `Allow()` and `Allow.from_markdown()` default to different `source` texts (`""` and `docs/exceptions.md`), visible in the messages
  of allowed findings; the test module reads `RULES_SMOKE` once at import, so `--smoke` works only when it runs as a script.
