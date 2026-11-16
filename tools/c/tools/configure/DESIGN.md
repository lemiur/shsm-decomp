# Build plumbing in C: design

Ports `configure.py` (the project table and its argument parsing), `tools/project.py` (dtk-template's generator:
`build.ninja`, `objdiff.json`, `compile_commands.json`, the `progress` printer), `tools/ninja_syntax.py`,
`tools/decompctx.py` and `tools/download_tool.py` to C17, under the user's rulings "port all tools to C"
(2026-11-11) and "the build plumbing too, last, because the gate depends on it" (2026-11-13). Since the retirement
of the Python tools the C plumbing is the only route: `build/tools/configure.exe` replaces `python configure.py`.

## 1. Where the unit table lives: it stays in `configure.py` (option a)

`configure.py` stays the one source of the project table, unchanged in form, still runnable by Python. The C
generator reads it with a small interpreter for the subset of Python the file is written in.

Why not (b), a data file under `config/R5WEA4/` with a converter: about 40 readers know `configure.py` by name or
edit it as text, and would all change at once:

| reader | what it does with `configure.py` |
|---|---|
| `tools/common/units.py`, `tools/c/lib/shsm_units.c` (used by match, wall, eqcheck, status, pick, draft, flagsweep, ...) | `Object(<status>, "<path>")` statuses by regex |
| `tools/unit.py`, `tools/c/tools/unit` | `unit new` inserts an `Object(NonMatching, ...)` line in the right library; `flip`/`park` rewrite the status word |
| `tools/gate.py`, `tools/c/tools/gate` | `--pending-flip`: diffs `HEAD:configure.py` against the work tree (one status word changed); `--quick`: a `configure.py` change reaches every unit |
| `tools/merge_lane.py` | lists the lane's units from the `Object` lines of both revisions |
| `tools/ledger.py`, `tools/c/tools/ledger`, `tools/rules/check_rules.py`, `tools/c/tools/check_rules` | statuses (`Matching` units are gated; `[match]` findings there are `MATCH-PENDING`) |
| `tools/check_docs.py`, `tools/doctor.py` and their ports | the pins and the per-library flags quoted in docs/toolchain.md |
| `tools/neutral.py`, `tools/slot.py`, `tools/c/lib/shsm_repo.c` | repository root marker, refusal of flag changes |

Option (a) changes none of them, keeps the table human-editable (comments next to the entries carry the evidence for
each flag), and keeps the Python route working for the parity proof. The cost is an interpreter; it is bounded
because the file is declarative in practice.

### The subset (what `configure.exe` evaluates)

The whole file is executed top to bottom, as Python would, by `tools/c/tools/configure/` (`cfg_*.c`):

- statements: `import` / `from ... import` (names bound to built-ins, below), assignment to a name, an attribute
  (`config.x = ...`) or through `setattr`, expression statements (calls), `if` / `elif` / `else`, `for` over a
  list, tuple or `dict.items()` with tuple unpacking, `def` with positional, defaulted and `*args` parameters and a
  body of the same statements plus `return`, docstrings and comments, type annotations (parsed, ignored);
- expressions: `None`, `True`, `False`, integers, strings (single, double, triple-quoted, `r`, implicit
  concatenation), f-strings with `{expr}` fields (no format specs), lists, tuples, dicts (insertion-ordered), `*x`
  in list and call positions, names, attributes, calls with keyword arguments, `/` on paths, `==`, `!=`, `in`,
  `not in`, `is`, `is not`, `not`, `and`, `or`, conditional expressions;
- built-ins: `argparse.ArgumentParser` (`add_argument` with `choices`, `type` (`str`, `str.upper`, `int`, `Path`),
  `default`, `help`, `nargs="?"`, `metavar`, `dest`, `action` `store_true` / `store_false` /
  `argparse.BooleanOptionalAction`; `parse_args` through the core's argparse emulation `avp/args.h`, so usage, help
  and errors are argparse's), `Path` (Windows `pathlib` semantics), `str`, `getattr`, `setattr`, `list.append`,
  `list.extend`, `list.index`, `dict.items`, `sys.exit`, and from `tools.project`: `Object`, `ProgressCategory`,
  `ProjectConfig`, `is_windows`, `generate_build`, `calculate_progress` (implemented by `tools/c/tools/project/`).

Anything else (a new statement form, an unknown built-in) is a hard error naming `configure.py:<line>`, never a
silent misreading: an edit to `configure.py` outside the subset fails `configure.exe` loudly, and the differential
harness (section 5) shows it before any switch. Values follow Python's object model where the generator can see the
difference (lists are shared by reference, so `cflags_runtime = [*cflags_base, ...]` copies and `"cflags":
cflags_base` aliases; dicts keep insertion order; `Object.resolve` merges options in Python's order).

## 2. Layout

| directory | ports | output |
|---|---|---|
| `tools/c/tools/project/` (library only, `NO_EXE`) | `tools/project.py`, `tools/ninja_syntax.py`: the value model (`pv_*`: Python values, Windows paths), `Object` / `ProjectConfig` / `ProgressCategory`, `generate_build` (`build.ninja`, `objdiff.json`, `compile_commands.json`), `calculate_progress` | `tool_project` |
| `tools/c/tools/configure/` | `configure.py`'s execution: lexer, parser, evaluator, the built-ins above, the front end | `build/tools/configure.exe` |
| `tools/c/tools/decompctx/` | `tools/decompctx.py` | `build/tools/decompctx.exe` |
| `tools/c/tools/download_tool/` | `tools/download_tool.py` (pinned URLs, sha256 checks kept) | `build/tools/download_tool.exe` |
| `tools/c/tools/transform_dep/` | `tools/transform_dep.py` (already ported and proven) | `build/tools/transform_dep.exe` |

## 3. Command line

    configure.exe [--script FILE] [--dump] [--] <configure.py arguments>

`configure.exe`'s own options come first and end at the first argument they do not know (or `--`); everything after
is handed, unchanged, to the script's own argument parser, so `configure_args` in `build.ninja` is exactly what
`python configure.py` wrote. `--script` defaults to `configure.py` in the current directory. `--emit c` is accepted
and ignored (build.ninja files written while a Python emit mode existed regenerate with it); `--emit python` and
`--python` are gone with the Python route.

The graph is project.py's with every Python step replaced by its C port, taken from the directory holding
`configure.exe` (written as the absolute, quoted path): `configure.exe $configure_args` (regeneration and
`progress`), `changes_fmt.exe`, `objcache.exe` (`run --` / `prehash`), `decompctx.exe`, `download_tool.exe`
(`transform_dep` is used only off Windows). No `$python` variable is written. The implicit inputs of the `configure`
edge are `config.json`, `configure.py` and `configure.exe`. The differences from project.py's output are listed in
`tools/c/tools/project/DEVIATIONS.md` (D-project-7).

### `configure.py`'s arguments (all supported, help and errors as argparse's)

`mode` (`configure` | `progress`), `-v/--version` (`R5WEA4`, upper-cased), `--build-dir DIR`, `--binutils`,
`--compilers`, `--map`, `--debug`, `--wrapper` (non-Windows only, so absent here), `--dtk`, `--objdiff`,
`--sjiswrap`, `--ninja`, `--verbose`, `--non-matching`, `--warn {all,off,error}`, `--no-progress`,
`--objcache` / `--no-objcache`. Local pins (`tools/bin/*.exe`, `tools/compilers`) fill unset tool paths.

## 4. Outputs and their consumers

| output | written by | consumed by |
|---|---|---|
| `build.ninja` | `configure` mode | `tools/bin/ninja.exe` (every build, `gate`, `merge_lane`, `unit new --regen`, `ninja changes`, `ninja build.ninja` regeneration); `units.py` / `shsm_units.c` parse its `# <source>: <lib> (linked True\|False)` comments and `mwcc*` edges (compile commands for match, neutral, flagsweep, permute, cc); `neutral` reads ninja's dependency records |
| `objdiff.json` | `configure` mode | `objdiff-cli report generate` (the `report` edge), the objdiff GUI, `units.py` / `shsm_units.c` (unit list, base/target objects, metadata) |
| `compile_commands.json` | `configure` mode | clangd only |
| progress text | `progress` mode (the `progress` edge after `report`) | the person running `ninja`; nothing parses it |
| `build/R5WEA4/report.json` | not configure: the `report` edge runs `objdiff-cli` | `gate` (coverage, regressions), `ledger`, `status`, `score_history`, the baseline |

Callers of `configure.exe`: `tools/c/tools/gate` (full gate, in `build/gate/tree`: `--compilers
<root>/tools/compilers`; `--quick`: no arguments), `tools/c/tools/merge_lane` (gate step, in the slot), the
`configure` edge of `build.ninja` itself, people. Each runs the `configure.exe` beside its own executable (section 6).

## 5. Proof

The harness and the Python route described in this section (`diff/`, `--emit python`, `python configure.py`) were removed at
retirement (commit 66ee84d3); the record of what they proved is `DEVIATIONS.md`. What remains is the real-tree check and the CTest suite.

- Differential harness `tools/c/tools/configure/diff/`: each case runs `python configure.py <args>` and
  `configure.exe --emit python --python <the harness's python> <args>` in two copies of a fixture tree (the real
  `configure.py`, `tools/project.py`, `config/`, `src/`, `include/`, `build/R5WEA4/config.json`, and variants:
  no `config.json` (bootstrap graph), `--non-matching`, `--debug`, `--map`, `--warn`, `--build-dir`, explicit tool
  paths, `--no-progress`, `--objcache`, `progress` with fixture reports, argument errors, `--help`) and compares
  stdout, exit code, the stderr class and the bytes of `build.ninja`, `objdiff.json` and `compile_commands.json`.
  `decompctx` and `download_tool` have their own harnesses (download: `file://` URLs and local fixture archives,
  never the network).
- Real tree, in the lane slot: `configure.exe --emit python` gives the same three files as `python configure.py`;
  `configure.exe --emit c` then `tools/bin/ninja.exe` gives `build/R5WEA4/main.dol: OK` and a `report.json` equal to
  the Python route's; the C gate with the opt-in option prints `GATE OK`.

## 6. The gate hook

The C gate (full and `--quick`) and the C merge tool run `configure.exe` from the folder of their own executable
(`build/tools/`); the gate's `--configure-exe PATH` overrides it.

## 7. `tools/run_tests.py` and `tools/build_c.cmd`

Status: as decided below, the test verb is now `ctest --preset tools` (or `tools/build_c.cmd --test`) and `tools/build_c.cmd`
is the ten-line wrapper; `run_tests.py` and `build_c.py` are gone and `difftool.py` was retired with the harness.

- `run_tests.py` runs the 43 Python test files (`tools/**/test_*.py`) as separate processes with selection
  (`--changed`, `-k`), sharding and timeouts. Every file it runs tests a Python tool, the Python half of the
  differential harness or a Python checker; in the C world those retire with the tools they test. CTest already
  provides what it adds (one process per test, `-j`, `--timeout`, `-L <tool>` / `-R <pattern>` selection,
  `--rerun-failed`), and each C tool's tests carry its CTest label. Decision: not ported. At retirement the `test`
  verb becomes `ctest --preset tools -j N [-L <tool>] [-R <pattern>]`, plus `difftool.py` while a Python reference
  remains (the harness is ported last). `--changed` maps to `-L` labels of the changed `tools/c/tools/<name>/`
  directories; if that selection is wanted, it is a small script in the retirement step, not a port.
- `build_c.cmd` is a convenience wrapper over the CMake presets (`tools`, `tools-build`), which are the contract; it
  adds only a job count, `MSBUILDDISABLENODEREUSE=1` and a fallback `cmake` path. The C tools cannot build
  themselves, so a C port would be circular. Decision: not ported; at retirement it is replaced by
  `cmake --workflow --preset tools-build` (or `tools` with the tests), wrapped if wanted by a ten-line
  `tools/build_c.cmd` setting `MSBUILDDISABLENODEREUSE=1` (a coordinated change outside `tools/c/tools/`).
