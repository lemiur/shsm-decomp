# score_history: accepted differences from the Python

Proven equal to the retired Python tools/score_history.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-score_history-1. A private copy of the rule checker's lexer and function parser (temporary)
- Behaviour: `sh_lex.c` holds a C copy of exactly the pieces of `tools/rules/check_rules.py` that score_history uses:
  `lex`, `pair_brackets`, and the function list of `Parsed` (the `_Parser.scope` walk with K&R definitions, aggregate
  bodies, `extern "C"` and namespaces, declaration macros, and the two-pass asm masking). It collects only what
  decides `Parsed.funcs` (name, first line, last line): variables, prototypes, typedefs, member lists, asm sites and
  the rule facts are not built. `rough_functions`, `index_source` and `demangle` are score_history's own and are
  ported in `sh_index.c`.
- Reason: the rule checker is ported by another lane; until `tool_check_rules` (or a lib module) exists, this copy is
  what the port can link. It is replaced by linking the rule checker port once that lands, with
  `diff/index_corpus.py` re-run to show the hashes stay the same. Evidence of equality today: every one of the 740
  C/C++ files under `src/` and `include/` gives the same index (functions, lines, addresses, hashes, unit and rest
  hashes, stored texts), and the full backfill (every committed revision of every source file) gives the same
  `blobs` and `sources` tables.

## D-score_history-2. Uncaught exceptions print a short traceback
- Behaviour: where the Python died with a traceback (an unknown `--rev`, a missing `--report` file, a report that is
  not JSON, a database that cannot be opened, a git failure outside `backfill`'s own handler), the C tool prints
  `Traceback (most recent call last):`, a placeholder frame line and `<Class>: <message>` on stderr and exits 1. The
  message text is the C tool's own (for git failures: the git command, without git's stderr).
- Reason: the exit code and the stderr class (non-empty) are the same; a Python traceback's frames cannot be
  reproduced and nothing reads them. `SystemExit` messages (`no snapshot matches 'x'`, `x: no such function`,
  `x: ambiguous (N matches)`, `no version with hash prefix x`, `no stored source text for ...`) are the same bytes.

## D-score_history-3. `backfill` and `record` print their output when the command ends
- Behaviour: the lines of `backfill` (`recorded <rev>: N rows`, `have ...`, `skip ...`) are collected and written when
  the command returns (`score_history_run` fills a buffer); the Python wrote each line as it went.
- Reason: the bytes and their order are the same; only their timing differs. The in-process interface
  (`score_history_run`, used by the pick port) needs the whole output in a buffer.

## D-score_history-4. A revision `backfill` skips keeps its partial index rows only as the Python did
- Behaviour: `backfill` runs every recorded revision in one transaction (`BEGIN` .. `COMMIT`); a revision that fails
  with a `RuntimeError`-class error (`<rev> has no <baseline>`, a git failure) leaves the transaction open, so the
  source indexes it already cached are committed with the next recorded revision, or dropped when the database is
  closed after the last one. `record` commits once at the end.
- Reason: this is what Python's implicit `sqlite3` transaction did (the failed revision's `INSERT`s stay pending
  until the next `commit()`; `close()` without a commit drops them), so the database contents are the same. The
  explicit transactions also make a run that dies half-way leave the database as it was before that revision.

## D-score_history-5. Non-ASCII corner cases of the lexer and the demangler
- Behaviour: the lexer works on UTF-8 bytes with Python's Unicode classes where they matter (whitespace `\s`, word
  `\w` for directive names, decimal `\d` for numbers, one operator token per non-ASCII code point); a Unicode
  whitespace character outside a comment or string (which made the Python lexer fail with an `AttributeError`) raises
  the same class of failure (exit 1). `demangle` reads ASCII digits only in length prefixes and `Q<n>` qualifiers,
  where the Python's `str.isdigit()` / `\d` also accepted other Unicode digits (and then failed in `int()` for some).
  The `_skip_quoted` backslash skips two bytes where the Python skipped two code points (equivalent: the bytes
  skipped or scanned next are never ASCII quote, backslash or newline).
- Reason: the project's sources and symbols are ASCII (and UTF-8 in comments); `diff/index_corpus.py` shows the same
  results on every file of the tree. Decoding is `errors="replace"` exactly (one U+FFFD per maximal invalid subpart).

## D-score_history-6. The default `--repo` is the checkout the tool runs in
- Behaviour: without `--repo`, the repository is `shsm_find_root()` (the nearest checkout at or above the current
  directory, else the executable's), where the Python took the checkout of `tools/score_history.py` itself.
- Reason: both are the checkout the tool belongs to when run from it, which is how the harness, the pick port and the
  project run it.

## Post-retirement changes (after the Python tools were retired)

## D-score_history-asm-1. A unit's source path may be `.s`
- Behaviour: when the report gives no `source_path`, the path guess tries `src/<unit>.s` after `.cpp`, `.c`, `.cp` and `.cc`; the file is then hashed like any other source (it holds no C functions, so its functions take the unit-basis hash).
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule).
