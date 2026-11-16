# pre_commit: deviations from tools/hooks/pre_commit.py

Proven equal to the retired Python tools/hooks/pre_commit.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-pre_commit-1. A staged set past the Windows command-line limit
- Behaviour: the C hook splits `git ls-files -s -z -- <names>` into batches that each stay under the 32 K-character
  command-line limit and checks every staged file; the Python passes every name in one command and, past the limit,
  fails with `pre-commit: error: [WinError 206] The filename or extension is too long` and exit 2 (the commit is
  blocked). hookdiff case "staged set past the command-line limit" (700 names): Python exit 2, C exit 0.
- Reason: a defect in the Python (a large commit, such as a split adding hundreds of units, cannot pass the hook); the
  C gives the answer the Python gives for any set that fits in one command line (unit test `batches`).

## D-pre_commit-2. The rule-checker warning runs the C checker in-process
- Behaviour: the hook links the rule checker (`tool_check_rules`) and runs `check_rules <paths>` in-process with the
  checkout (the toplevel) as root. A checkout without configure.py has no Matching unit, so the checker exits 0 and
  nothing is printed. An exception inside the checker prints `pre_commit.exe: error:` and its last line
  among the warning lines.
- Reason: the C tools link each other rather than shell out.

## D-pre_commit-3. Standard output is UTF-8
- Behaviour: the C hook writes UTF-8 (every C tool does: `AVP_MAIN`); a staged path outside ASCII (`src/café.o`) is printed as UTF-8.
- Reason: git and the repository store paths in UTF-8; UTF-8 output is the project's convention for the C tools
  (tools/c/README.md, "Output and exit-code conventions").

## D-pre_commit-4. An error the Python does not catch
- Behaviour: an unexpected failure while reading the index (a malformed `git ls-files` record, a non-numeric
  `git cat-file -s`) prints one line `<ExceptionClass>: <message>` on stderr and exits 1; the Python prints a traceback
  (also exit 1).
- Reason: a C tool has no traceback; exit code and stderr class are the same. The input cannot come from a working git.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `pre_commit.exe: error: <Class>: <message>`, with the same exit code as before.
