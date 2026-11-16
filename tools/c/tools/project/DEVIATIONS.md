# project: accepted differences from tools/project.py and tools/ninja_syntax.py

Proven equal to the retired Python tools/project.py and tools/ninja_syntax.py by the differential harness, removed at retirement, commit 66ee84d3.

The port is driven by `configure.exe` (tools/c/tools/configure).

## D-project-1. The order of the missing-compiler check
- Behaviour: when several compiler versions used by the objects lack `mwcceppc.exe` under `config.compilers_path`,
  the C port reports the first in the order the objects use them; Python iterates a `set` of strings, whose order
  depends on the per-process hash seed, so the reported version varies between Python runs.
- Reason: Python's own output is not reproducible; exit code (1) and stderr class are the same. The harness checks
  one missing compiler (`nocc`) and the missing linker (`nold`).

## D-project-2. `config.link_order_callback`
- Behaviour: a truthy `link_order_callback` stops the C port with a one-line error (exit 1) when `config.json` is
  loaded; Python calls the function.
- Reason: calling a configure.py function from the generator would need the interpreter's call machinery here;
  configure.py keeps the callback commented out. A loud failure, never a misreading.

## D-project-3. Exceptions are one-line messages
- Behaviour: what raises inside project.py in Python (a `KeyError` on a malformed `config.json` or `objdiff.json`,
  a `json.JSONDecodeError`, a `ValueError` of `Path.with_suffix` or `os.path.relpath` across drives, a `TypeError` on a
  wrongly typed configure.py value, an unreadable file) is one line `<ExceptionName>: <message>` on stderr with exit
  code 1 in the C port, where Python prints a traceback and exits with 1. `sys.exit("...")` messages are Python's,
  exactly.
- Reason: same exit code and stderr class; none of these occur with the project's configure.py and a dtk-written
  config.json.

## D-project-4. Windows only
- Behaviour: `is_windows()` is true, `use_wibo()` false (no wibo download edge), the non-Windows branches (the
  `transform_dep` suffix of the compile commands, `wine` as the default wrapper, `shlex.join` quoting) are absent.
  `config.wrapper` is honoured as on Windows.
- Reason: the C tools are Windows-native (MSVC x64), like the rest of tools/c.

## D-project-5. Progress values that are not numbers
- Behaviour: in `calculate_progress`, a measure that is a string of non-ASCII Unicode digits (which Python's
  `str.isdigit` / `int` accept) is refused with a one-line error, and a percentage measure that is a string or null
  gives a one-line `ValueError` message where Python raises from the format spec.
- Reason: objdiff writes ASCII digit strings and float percentages; the harness reports (`rep_real`, `rep_min`) are
  equal.

## D-project-6. `GITHUB_STEP_SUMMARY`
- Behaviour: an unwritable summary path gives `FileNotFoundError: <path>` (exit 1) rather than a traceback.
- Reason: as D-project-3. The summary text itself (the fences and the progress lines, CRLF) is Python's.

## D-project-7. Every tool step runs a C tool
- Behaviour: the generator writes project.py's graph with every Python step replaced by its C port from the directory
  of `configure.exe` (ProjectEnv.ctools, `<ctools>` below: absolute, backslashes). A command names the executable
  quoted (`"<ctools>\x.exe"`); an implicit input names it unquoted (ninja-escaped: `E$:\...`). No `$python` variable
  is written. Every other byte equals project.py's output, `objdiff.json` and `compile_commands.json` included. The
  differing lines, all in `build.ninja`:

  | project.py | C |
  |---|---|
  | `python = "<sys.executable>"` | (not written) |
  | rule `download_tool`: `$python tools\download_tool.py $tool $out --tag $tag` | `"<ctools>\download_tool.exe" $tool $out --tag $tag` |
  | the implicit `tools\download_tool.py` of every download edge | `<ctools>\download_tool.exe` |
  | rule `decompctx`: `$python tools\decompctx.py $in -o $out -d $out.d $includes $excludes $defines` | `"<ctools>\decompctx.exe" $in -o $out ...` (same arguments) |
  | the implicit `tools\decompctx.py` of every `.ctx` edge | `<ctools>\decompctx.exe` |
  | `--objcache`: the compile prefix `$python -S tools\objcache.py run -- ` (rules mwcc, mwcc_sjis, mwcc_extab, mwcc_sjis_extab) | `"<ctools>\objcache.exe" run -- ` |
  | `--objcache`: rule `objcache_prehash`: `$python tools\objcache.py prehash --compilers <c> --stamp $out` | `"<ctools>\objcache.exe" prehash --compilers <c> --stamp $out` |
  | rule `progress`: `$python configure.py $configure_args progress` | `"<ctools>\configure.exe" $configure_args progress` |
  | the `progress` edge's implicit `tools\project.py` | `<ctools>\configure.exe` |
  | rules `changes_fmt` / `changes_md`: `$python tools\changes_fmt.py ...` | `"<ctools>\changes_fmt.exe" ...` (same arguments) |
  | the implicit `tools\changes_fmt.py` of the `changes`, `changes_all` and `regressions.md` edges | `<ctools>\changes_fmt.exe` |
  | rule `configure`: `$python configure.py $configure_args` | `"<ctools>\configure.exe" $configure_args` |
  | the `configure` edge's implicit `tools\project.py tools\ninja_syntax.py` | `<ctools>\configure.exe` (after `configure.py`) |

  When the script is not `configure.py` in the current directory, the two `configure.exe` commands also pass
  `--script <script>`. Ninja's own line wrapping (at 78 columns) moves with the longer commands. `transform_dep` is
  not used: project.py runs it only when `os.name != "nt"` (D-project-4).
- Reason: the build plumbing is C; nothing in the graph runs Python.
