# check_docs: accepted differences from tools/check_docs.py

Proven equal to the retired Python tools/check_docs.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-check_docs-1. The pin table is the C doctor's, built in
- Behaviour: `pins` compares the Toolchain list of docs/toolchain.md with the pin table compiled into the C doctor
  (`tools/c/tools/doctor`, `doctor_pins`, `doctor_compilers_required`, `doctor_compilers_optional`); the Python imports
  `tools/doctor.py` from the `--root` checkout. Run against another checkout (`--root`), the C tool checks that
  checkout's docs/toolchain.md against its own build's table, the Python against the checkout's own `tools/doctor.py`.
- Reason: the C doctor is the port of tools/doctor.py and carries the same table (its own test checks the table
  against docs/toolchain.md); loading Python source is what the port removes. On this checkout both give the same lines
  (harness `--only pins` cases and the real-tree run). Where they differ: a checkout without `tools/doctor.py` (the
  harness fixture) gives the Python one finding, `tools/doctor.py:0: PINS cannot import: ...`, and the C the
  comparison against its built-in table. The C's messages name "doctor's pin table" and "doctor's compiler list" where
  the Python names `tools/doctor.py PINS`.

## D-check_docs-2. exceptions runs the C ledger
- Behaviour: the `exceptions` check runs `ledger.exe exceptions-check` from the folder of check_docs.exe (an
  isolated build such as `build/tools-B` runs its own ledger), else `<root>/build/tools/ledger.exe`, where the Python
  runs `python tools/ledger.py exceptions-check`. When neither exists it reports one finding,
  `build/tools/ledger.exe:0: EXCEPTIONS exceptions-check could not run: no ledger.exe beside check_docs.exe or in
  build/tools (build the tools with tools/build_c.cmd)`. Findings about the run itself (could not run, environment
  error) name `build/tools/ledger.exe` where the Python's name `tools/ledger.py`.
- Reason: the ledger port is proven by its own harness (`ledger exceptions-check` output equal byte for byte), so
  the lines folded in are the same; the Python ledger is retired.

## D-check_docs-3. Other projects' names: equal lengths in order of appearance
- Behaviour: the alternation built from docs/references.md's `owner/repo` names (`foreign_projects`) is ordered by
  length, longest first; names of equal length keep the order of their first appearance, where the Python's order
  is that of a set (string-hash order, varies with PYTHONHASHSEED).
- Reason: no output can show it: two different names of the same length cannot both match at one position, so the
  order among them never changes which text matches.

## D-check_docs-4. tab_indented_line: the tokenizer subset
- Behaviour: the Python ran `tokenize.tokenize` over each tracked `.py` file and took the first INDENT token whose
  text holds a tab (None when the tokenizer raised first). The C follows CPython 3.14's C tokenizer
  (Parser/lexer/lexer.c, cd_pytok.c) for everything that decides an INDENT or an error before one (encoding
  detection, indentation, strings, f/t-strings, numbers, continuations, control characters). Not reproduced: a coding
  cookie that names a codec other than UTF-8, Latin-1 or ASCII (such a file is decoded as UTF-8 here, where Python
  uses the named codec), and errors CPython raises only for token kinds that cannot occur before the first INDENT
  of a file the checker reads (deep nesting beyond the parser's limits, for example).
- Reason: the check only asks for the first tab-indented line; every `.py` file of the tree gives the same answer
  (harness `--only text` cases on the real tree and on the fixture with a tab-indented file).


## D-check_docs-5. verbs reads the C executables' help; Python commands are findings
- Behaviour: the adapter table's commands are `build/tools/<name>.exe ...`. For each, `verbs` runs `<name>.exe
  [sub] --help` from the folder of check_docs.exe (an isolated build such as `build/tools-B` checks its own
  executables), else `<root>/build/tools/<name>.exe`; when neither exists the row gets a finding that names
  `tools/build_c.cmd`. `tools/bin/<tool>[.exe]` commands are checked as before. A command whose first word is
  `tools/build_c.cmd`, `ctest`, `cmake`, `git`, `cp` or `rm` is accepted without a help check. A command in the
  Python form (`python tools/x.py`, `python -I tools/x.py`, `python -m tools.x`, `python configure.py`, a bare
  `tools/x.py`) is a finding (`... names a Python tool; the commands are build/tools/<name>.exe`), and the code spans
  of such a row that name no program are not checked against the row's other tools. `configure.exe` is run like any
  other tool, where the Python searched configure.py and tools/project.py for `python configure.py` instead of running
  it (and noted that per row); nothing reads tools/project.py. The notes on tools missing from the table list the
  executables the tree builds (each `tools/c/tools/<name>/main.c`, the note placed on that file, its mentions found as
  `<name>.exe` in the documents), where the Python listed the `tools/**/*.py` scripts with a `__main__` block.
- Reason: the Python tools are retired and the table of docs/tools.md documents the C executables, so the check reads their
  help. Until the table is rewritten in that form, each row still in the Python form is reported.

## D-check_docs-6. The document set and the text extensions
- Behaviour: the documents `verbs` and `paths` read are AGENTS.md, tools/rules/README.md and tools/c/README.md (the
  library contract) when present, then docs/*.md, where the Python reads tools/common/README.md in place of
  tools/c/README.md. The text check also covers `.cmd` files (tools/build_c.cmd).
  `verbs` takes its table from docs/tools.md and `pins` its Toolchain list from docs/toolchain.md (`CD_VERBS_DOC`,
  `CD_PINS_DOC`; both are docs/*.md files, so `paths` and `text` read them too); `pins` also looks values up in
  AGENTS.md, which keeps the DOL hash.
- Reason: tools/common is retired with the Python tools; tools/c/README.md is its counterpart.
