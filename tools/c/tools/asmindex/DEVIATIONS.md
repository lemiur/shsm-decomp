# asmindex: accepted differences from `python -m tools.common index`

Proven equal to the retired Python tools/common/asmindex.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-asmindex-1. A program of its own, not a subcommand
- Behaviour: the Python runs as `python -m tools.common index ...` (prog `python.exe -m tools.common index`, usage of
  the `index` subparser, `--debug` on the parent parser); the C runs as `asmindex ...` (prog `asmindex`, one parser
  with `-h`, `--debug` and the index options). Help and usage texts and the program name in error lines differ; the
  options, outputs, messages and exit codes are the same.
- Reason: one executable per tool (tools/c/README.md). The harness normalises the program name and drops the usage
  block (diff/cases.txt); 15 of 15 cases are the same.
