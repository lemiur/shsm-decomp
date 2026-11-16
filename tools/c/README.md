# tools/c: the project's tools in C (porting recipe)

All of the project's tooling is being ported to C17 (owner's ruling): one native executable per tool, the same stdout,
exit code and stderr class as the Python tool it replaces, proven by the differential harness (retired; see
the DEVIATIONS.md files). The Python tools are deleted. This file is the contract a porting agent reads instead of the source.

## Layout

| path | contents |
|---|---|
| `core/include/avp/`, `core/src/` | runtime library `shsmcore` (from another project of the author's; `avp_` API names kept) |
| `core/tests/*.c` | its unit tests (CTest label `core`) |
| `lib/*.c/.h` | shared tool logic, library `shsmtools` (`shsm_*` shared modules, `cc_*` comparison) |
| `lib/<module>.md`, `lib/<module>.DEVIATIONS.md` | a lib module's API documentation, and its accepted differences from the Python (one pair per module) |
| `tests/*.c` | unit tests of `lib/` (CTest label `lib`) |
| `tools/<name>/` | one tool per directory (below) |
| `third_party/<name>/` | one vendored library per directory (below, "Vendoring a library") |
| `cmake/` | `shsm_tool.cmake` (`shsm_add_tool`), `shsm_third_party.cmake` (`shsm_register_third_party`), `tools_stamp.cmake` (generates `avp_tools_stamp()`, sha256 of `core/`, `lib/*.c,*.h` and `tools/**/*.c,*.h`), `entry_points.cmake` (CTest check: every executable enters through `AVP_MAIN`) |
| `DEVIATIONS.md` | accepted differences of the shared library recorded before the per-module files (`D-B*`, `D-C*`) |

One tool, `tools/<name>/` (originally ported from `tools/<name>.py`, since retired):

| file | role |
|---|---|
| `main.c` | front end: argument parsing and output, ends with `AVP_MAIN(run, "<name>")` -> `build/tools/<name>.exe` |
| `*.c`, `*.h` (not `main.c`) | the tool's logic, static library `tool_<name>` (linked by the exe and the tests; the directory is on its include path) |
| `tests/*.c` | unit tests: `tests/foo_test.c` -> `build/tools/tests/test_<name>_foo_test.exe`, CTest test `<name>_foo_test`, label `<name>` |
| `DEVIATIONS.md` | this tool's accepted differences from the Python |
| `CMakeLists.txt` | optional: replaces the default rule (see below) |

The top-level `CMakeLists.txt` globs `tools/*/` (with `CONFIGURE_DEPENDS`; the build re-runs CMake when a directory or
file appears). **Adding a tool edits no shared file**: no registry, no test list, no shared DEVIATIONS entry. If a
tool needs a change to an existing `lib/` module or to `core/`, that is a separate, coordinated change; its deviations
go in `lib/<module>.DEVIATIONS.md`.

**Files a porting agent does not edit** (several agents port tools in parallel worktrees; these files are shared):
`tools/c/README.md`, the top-level `tools/c/CMakeLists.txt`, `tools/c/DEVIATIONS.md`, `tools/c/CMakePresets.json` and
everything under `tools/c/cmake/`. Everything an agent adds lives in files it owns: `tools/<name>/` (its tool),
`lib/<module>.c/.h/.md` plus `lib/<module>.DEVIATIONS.md` (a new shared module, documented in its own `.md`; a
shared-library deviation goes in that module's `.DEVIATIONS.md`, never in `tools/c/DEVIATIONS.md`),
`tests/<module>_test.c`, and `third_party/<name>/` (a vendored library).

Unusual tools: a `tools/<name>/CMakeLists.txt` is `add_subdirectory`'d instead of the default rule and calls
`shsm_add_tool(<name> [DIR d] [SOURCES ...] [MAIN main.c] [TESTS ...] [LIBS targets...] [INCLUDES dirs...] [NO_EXE])`
(defaults = the default rule's globs); also available there: `shsm_target_flags(<target>)` (the project's compile
flags), `shsm_add_test(<label> <src> [LIBS ...])`, and the variables `SHSM_REPO`, `SHSM_TOOLS_OUT`, `SHSM_C_FLAGS`,
`SHSM_C_DEFS`, `SHSM_TEST_TIMEOUT`. `cmake/shsm_tool.cmake` documents them. A tool may link another tool's library
(`LIBS tool_other`), but shared code belongs in `lib/`.

## Adding a tool (recipe)

1. Read the tool's specification (or, for a port of a retired Python tool, `git show 66ee84d3^:tools/<name>.py`) and find the C counterparts of the shared modules it needs in the API reference below.
   Missing shared pieces: write them in your tool directory first; move to `lib/` only by coordinated change.
2. `tools/c/tools/<name>/main.c`: parser with `shsm_cli_parser` (or `avp_args_parser` for unusual CLIs), body through
   `shsm_cli_main`, `AVP_MAIN(run, "<name>")`. Never define `main`/`wmain` (the `entry_points` test fails).
3. Logic in `<name>.c/.h` (or several files), prefixed by the tool name.
4. Tests in `tests/<topic>_test.c` with `avp/test.h` (`AVP_TEST(x) { CHECK(...); }`, `AVP_TEST_MAIN("<name>")`); they
   run from the repository root.
5. (Retired with the harness.) Tests covering every option and error path.
6. `DEVIATIONS.md` (below), even if it only says "No entries".
7. Build, `ctest -L <name>`, record any difference from the retired Python as a deviation.

## Porting a Python tool that imports other Python tools

Many tools import another tool's module (`import gatecheck`, `from rules import check_rules`) or run it as a
subprocess. The C tool links the C library for that logic (`lib/`, or the other tool's `tool_<other>` library through
`LIBS`) and **never shells out to a Python tool**. If the logic has no C counterpart yet, port the piece it needs
(into `lib/` as a new module, or your tool directory), not a bridge.

There are no bridges left: `match --data` and `match --rules` link the layout check and the rule checker.

A bridge to a Python tool is a bug. Running the project's native tools (`tools/bin/*.exe`, the compilers, `git`)
through `avp_run` is not a bridge.

The Python tools (`tools/<name>.py`), their tests and the differential harness were deleted at retirement; the
DEVIATIONS.md files are the proof record.

## Vendoring a library

A third-party library goes in `third_party/<name>/`, nowhere else, and edits no shared file (the top-level
`CMakeLists.txt` adds every `third_party/*/CMakeLists.txt` by glob). The directory holds:
- the library's sources, unchanged where possible (changes listed in `VERSION`);
- its licence file as shipped (`LICENSE`, `COPYING`, ...) and a `NOTICE` with the attribution the licence requires;
- `VERSION`: the release, the source path (the upstream subdirectory or archive member taken), the source repository
  and its commit;
- its own `CMakeLists.txt`, which defines one static target, publishes its include directories and registers it:

      add_library(<target> STATIC <sources>...)
      target_include_directories(<target> PUBLIC <include dirs>)
      shsm_register_third_party(<target>)

Registration (`cmake/shsm_third_party.cmake`) compiles the library with warnings off and without `/WX` (it is not our
code; the zero-warnings rule covers our code only) and links it into `shsmcore`, so every library, tool and test links
it and sees its `PUBLIC` include directories. The `entry_points` check skips `third_party/`.

## Build and test

    cmake --workflow --preset tools-build     # in tools/c: configure + build, Release
    cmake --workflow --preset tools           # configure + build + every unit test
    tools/build_c.cmd [--test] [--parallel N]   # the same from any directory (configure, build, ctest steps)
    ctest --preset tools -L <name>            # in tools/c: one tool's tests (after a build)

Machine load: `build_c.cmd --parallel N` (default: the number of processors) sets the build's jobs
(`cmake --build --preset tools --parallel N -- /nodeReuse:false`) and CTest's (`ctest --preset tools -j N`), and runs
every child with `MSBUILDDISABLENODEREUSE=1`, so no MSBuild node outlives the build. The presets carry the same for
direct cmake use: build preset `jobs: 4`, `nativeToolOptions: ["/nodeReuse:false"]` and that environment variable;
test preset `execution.jobs: 4`, timeout 600 s. A test killed on its timeout takes its children with it: those started
through `avp_run` sit in a kill-on-close job object (core/proc.c), and CTest's own job object ends the rest when CTest
exits.

Output: executables `build/tools/<name>.exe`, tests `build/tools/tests/`, CMake tree `build/tools-cmake/`. Visual Studio
2022 (C++ workload) and cmake 3.25+; MSVC x64 only. A second build next to a busy one (another agent's):
`cmake -S tools/c -B <dir> -G "Visual Studio 17 2022" -A x64 -DSHSM_TOOLS_OUT=<dir>/build/tools` then
`cmake --build <dir> --config Release` and `ctest --test-dir <dir> -C Release -L <name>` (core's `base_test` expects
`build/tools` in the executable path, hence the suffix).

Worktrees: nothing here names an absolute path. The preset's CMake tree and executables are under the `build/` of the
checkout the preset is run from (`${sourceDir}/../../build/...`), so two worktrees never share a CMake tree;
`build_c.cmd` takes the repository root from its own location, and the tools find
their root from the current checkout (`avp_root`). Test fixtures that need an absolute path build it at run time.

**Zero warnings**: C17, `/W4 /WX /utf-8 /Zc:preprocessor`, MSVC 2022; every target of ours gets these flags
(`shsm_target_flags`; vendored `third_party/` targets are built with warnings off). A warning is a build failure; do not disable warnings with pragmas.

## Differential harness (retired)

The harness (`difftool.py`, the per-tool `diff/` case files and generators) and the Python tools it compared against
were removed at retirement. What it proved is recorded in each tool's `DEVIATIONS.md` (its first line names the retired
Python file and the last run); the D-entries there are every accepted difference. A difference not listed is a bug.
No harness code remains under `tools/c`.

## Deviations

Every behaviour difference from the Python is an entry; an unlisted difference is a bug. A tool's entries go in
`tools/c/tools/<name>/DEVIATIONS.md`, numbered `D-<name>-<n>`; a shared-library module's in
`tools/c/lib/<module>.DEVIATIONS.md`, numbered `D-<module>-<n>`. `tools/c/DEVIATIONS.md` keeps the entries recorded
before the per-module files (`D-B<n>` shsm_*, `D-C<n>` cc_*) and takes no new ones. Entry format:

    ## D-<name>-<n>. <short title>
    - Behaviour: what the C tool does differently (input, output, exit code, stderr).
    - Reason: why the difference is accepted (and the evidence: harness run, input that cannot occur).

## Output and exit-code conventions

As the retired Python command-line module did, implemented in `lib/shsm_cli.h`: exit 0 clean, 1 finding (`SHSM_EXC_FINDING`), 2 usage or
environment error; errors are one line `<prog>: error: <message>` on stderr (`shsm_cli_main` maps the `SHSM_EXC_*`
exceptions raised with `shsm_raise`); `--json` prints one document identical to `json.dumps(..., indent=1)`
(`shsm_output_*`, `shsm_json_opts`; `avp/json.h` reproduces json.dumps byte for byte); text output capped at
`--limit` (default 40) with the Python footer (`... N more lines, <hint>`), `--all` lifts the cap. Python number and
string formatting through `avp/fmt.h` (`avp_pyfmt`, `avp_float_repr`), never printf for user-visible floats.
stdout is UTF-8 and buffered (`AVP_MAIN`); the C tool writes the same line endings as the Python (check with the harness).

## API reference: lib/ (shsmtools)

Each lib module documents itself in [`lib/<module>.md`](lib/) (its API, the Python it ports); a new module adds its
`.md` there and does not edit this file. The modules below predate that convention and are documented here.

Allocation goes into an `AvpArena *a` (first parameter where present). Exceptions are `avp_throw` codes
(`SHSM_EXC_*`), caught with `AVP_TRY`.

**`shsm_cli.h`** (tools/common/cli.py): exit codes `SHSM_EXIT_OK/FINDING/ERROR`, `SHSM_EXC_*`, `SHSM_DEFAULT_LIMIT`.
- `shsm_raise(exc, fmt, ...)` raise; `shsm_raise_io()` re-raise the last fs.h failure as FileNotFoundError/OSError/ValueError; `shsm_io_exc_code()` its class
- `shsm_exc_class` `type(e).__name__`; `shsm_exc_is_key` / `_is_os` / `_is_value` class tests; `shsm_exc_str` `str(e)`; `shsm_exc_main_text` text after `prog: error: `
- `shsm_cli_parser(prog, description, root, json_switch, limit)` parser with `--root --json --all --limit --debug`; `shsm_limit_type` parse one `--limit`
- `shsm_cli_main(ap, argc, argv, fn, ctx)` parse, call, map exceptions to exit 1/2
- `shsm_cap_lines` lines cut to the limit plus footer; `shsm_output_open` / `_line` / `_linez` / `_lines` / `_data` / `_close` capped text or `--json` output (cli.Output); `shsm_json_opts` json.dumps(indent=1) options
- `shsm_run(a, argv, argc, cwd, timeout)` run without a shell, capture text; `shsm_run_ok`, `shsm_run_output` (stdout+stderr), `shsm_run_check` raise USAGE unless it succeeded; `shsm_default_jobs` os.cpu_count()

**`shsm_repo.h`** (tools/common/repo.py), type `ShsmRepo` (every standard path):
`shsm_find_root(start)` checkout root; `shsm_read_version(root)`; `shsm_retail_dol_path`; `shsm_repo_new(root, version)`;
`shsm_repo_find(root)` cached repo; `shsm_repo_tool` tools/bin/<name>.exe; `shsm_repo_compiler` mwcceppc path;
`shsm_repo_asm_path` build/<ver>/asm/<unit>.s; `shsm_repo_rel` root-relative forward-slash path; `shsm_unit_path`
(`main/x` or `x.cpp` -> `x`); `shsm_unit_name` (`main/` + path).

**`shsm_symbols.h`** (tools/common/symbols.py), types `ShsmSymbol`, `ShsmSymbolTable`, `ShsmSplits`, `ShsmSplitUnit`, `ShsmSpan`:
- `shsm_parse_symbol_line`, `shsm_parse_symbols`, `shsm_load_symbols(r, with_splits)`; `shsm_symbol_end`, `shsm_symbol_attr`
- lookups: `shsm_sym_by_name`, `shsm_sym_all_by_name`, `shsm_sym_at`, `shsm_sym_first_at`, `shsm_sym_containing`, `shsm_sym_describe` (`name+0xN`), `shsm_sym_in_section`, `shsm_sym_functions`, `shsm_sym_in_range`, `shsm_sym_unit_of`
- splits: `shsm_parse_splits`, `shsm_load_splits`, `shsm_splits_unit`, `shsm_splits_range_of`, `shsm_splits_unit_of`

**`shsm_units.h`** (tools/common/units.py), types `ShsmUnit`, `ShsmUnits`, `ShsmCompileOpts`:
`shsm_parse_configure_status`, `shsm_join_ninja_lines`, `shsm_parse_ninja_compiles`, `shsm_load_units(r)` (objdiff.json +
configure.py + build.ninja), `shsm_units_lookup` (NULL when unknown), `shsm_get_unit` (raises KEY / UNIT_NOT_BUILT),
`shsm_unit_has_source`, `shsm_object_for`, `shsm_depfile_for`, `shsm_compile_argv` (exact ninja argv),
`shsm_compile_source` (compile into a private dir).

**`shsm_mangle.h`** (tools/common/mangle.py, demangling only): `shsm_demangle`, `shsm_demangle_or_name`, `shsm_qualified` (`Class::fn`).

**`shsm_cache.h`** (tools/common/cache.py, same disk format and keys), types `ShsmKey`, `ShsmCache`:
- key: `shsm_key_init`, `shsm_key_bytes` / `_str` / `_strz` / `_repr` / `_none` / `_int` / `_str_list`, `shsm_key_hex`; `shsm_repr_str_tuple`, `shsm_repr_str_pairs`, `shsm_file_digest`
- cache: `shsm_cache_default_root`, `shsm_cache_new`, `shsm_cache_path`, `shsm_cache_get`, `shsm_cache_put`, `shsm_cache_store`, `shsm_cache_restore`, `shsm_cache_contains`, `shsm_cache_remove`

**`shsm_report.h`** (tools/common/report.py), types `ShsmReport`, `ShsmUnitReport`, `ShsmFunctionReport`:
`shsm_read_report(path)`, `shsm_load_report(r)`, `shsm_load_baseline(r)`, `shsm_report_unit`, `shsm_report_function`, `shsm_unit_report_function`.

**`shsm_text.h`**: `shsm_ws_len` / `shsm_skip_ws` (Python `\s`), `shsm_shlex_split` (POSIX shlex.split).

**`cc_elf.h`** (codecmp.py `Elf`), types `CcElf`, `CcSection`, `CcSym`, `CcReloc`, `CC_R_*`:
`cc_elf_parse`, `cc_reloc_type_name`, `cc_elf_section_name`, `cc_elf_reloc_exact`, `cc_elf_reloc_at`, `cc_elf_function`, `cc_elf_find_symbol`.

**`cc_listing.h`** (codecmp.py dtk listing rows), types `CcRow`, `CcRef`, `CcFn`:
`cc_parse_row`, `cc_decode_branch`, `cc_fn_new`, `cc_fn_from_norms`, `cc_fn_norms`, `cc_parse_listing`, `cc_load_asm`
(retail .s), `cc_load_object` (runs `dtk elf disasm`), `cc_is_space`.

**`cc_codecmp.h`** (codecmp.py Project, Dol, Context, comparison), types `CcProject`, `CcDol`, `CcContext`, `CcFnResult`, `CcStatus`:
- project: `cc_project_new`, `cc_project_root`, `cc_project_addresses`, `cc_project_spans`, `cc_project_dol`, `cc_project_unit_sections`, `cc_project_describe`
- DOL: `cc_dol_read`, `cc_dol_in_bss`; parsing: `cc_parse_symbols`, `cc_parse_splits`; names: `cc_embedded_address`, `cc_is_compiler_local`, `cc_retail_name_for`
- compare: `cc_context_new`, `cc_context_check`, `cc_context_raw_operand_check`, `cc_status_name`, `cc_fn_result_exact`, `cc_edit_count`, `cc_compare_function`, `cc_codecmp_object`

**`cc_compare.h`** (tools/common/compare.py facade), types `CcRepo`, `CcError`, `CcPair`, `CcCompareResult`, `CcObjectComparison`, `CcListingCache`:
`cc_unit_path`, `cc_repo_asm_path`, `cc_repo_src_obj_dir`, `cc_repo_dtk`, `cc_pair_status_name`, `cc_pair_differs`,
`cc_align` (difflib, autojunk off), `cc_compare_result_new`, `cc_compare_result_to_json` (Python key order),
`cc_compare_result_render` (side by side), `cc_is_gap`, `cc_load_retail`, `cc_load_compiled`, `cc_repo_context`,
`cc_compare_function_unit`, `cc_compare_object`.

## API reference: core/ (shsmcore, `#include "avp/<x>.h"`, or `avp/avp.h` for all but test.h)

| header | provides (Python equivalent) and key functions |
|---|---|
| `base.h` | types `i8..u64`; `AVP_MAIN(run, "tool")` (UTF-8 argv and console, buffered stdout); `AVP_ASSERT` (always on); `avp_fatal` / `avp_exit` / `avp_warn` / `avp_panic`; `avp_root`, `avp_set_root`, `avp_exe_dir`, `avp_tool_name`, `avp_tools_stamp`; `avp_xmalloc` family; try/throw: `AVP_TRY(t)`, `avp_try_end`, `avp_throw(code, fmt, ...)`, codes `AVP_ERR_IO/FORMAT/REGEX/PROC/OTHER` |
| `arena.h` | bump arenas: `avp_arena()` (thread's long-lived), `avp_scratch()`, `avp_arena_mark/reset`, `avp_alloc` (zeroed), `avp_strdup`, `avp_memdup`, `avp_sprintf` (C rules), `AVP_NEW`, `AVP_NEWN` |
| `str.h` | `AvpStr` view, `AvpStrVec`, `AvpBuf` builder; str methods (`avp_str_split/strip/partition/replace/join/startswith/find/splitlines/lower/casefold/ljust`...), `avp_parse_int` / `avp_parse_float` (int(), float()), UTF-8 and wide helpers (`avp_to_w`, `avp_from_w`), `AVP_S(lit)` |
| `vec.h` | typed growable arrays: `AVP_VEC(T)`, `avp_vec_push/pop/insert/remove/extend/free`; `AvpBytes`, `AvpI64Vec`, `AvpPtrVec`, ... |
| `map.h` | insertion-ordered dict/set: `avp_map_*` (string keys), `avp_imap_*` (int keys), `avp_set_*`, `avp_iset_*`, `avp_pyintset_*` (CPython set-of-int order) |
| `sort.h` | stable `avp_sort` (list.sort), `avp_sort_strs`, `avp_bisect_*`, `avp_heap_push/pop`, `avp_heapify` |
| `json.h` | json byte for byte: `avp_json_parse(_ex)`, builders `avp_json_obj/arr/str/int/float`, `avp_json_set/get/at/len`, `avp_json_dumps` / `avp_json_dump_file` with `AVP_JSON_INDENT(n)` / `AVP_JSON_COMPACT`, streaming `avp_jsonw_*` |
| `args.h` | argparse clone (CPython 3.14 help and error text): `avp_args_parser`, `avp_arg`, groups, mutex, subparsers, `avp_args_parse`, getters `avp_args_str/int/bool/strs/given` |
| `proc.h` | subprocess: `avp_run(spec)` (cwd, env, capture, text, timeout, check), `avp_run_text`, Popen `avp_proc_start/wait/communicate/kill_tree`, `avp_list2cmdline`, `avp_which`, `avp_env_*`; children in a kill-on-close job; timeout kills the tree; ceiling `AVP3_PROC_CEILING` |
| `fs.h` | files, os, os.path (ntpath), glob, fnmatch, shutil, tempfile, pathlib: `avp_read_text/all/lines`, `avp_write_text/atomic`, `avp_path_join/abspath/relpath/normpath/splitext`, `avp_exists/isfile/isdir/stat/getmtime`, `avp_makedirs`, `avp_scandir`, `avp_walk_*`, `avp_glob`, `avp_fnmatch`, `avp_rmtree`, `avp_copy2`, `avp_mkdtemp`, `avp_pl_*`; failures as Python exceptions (`avp_io_err`, `AVP_IOE_*`) |
| `hash.h` | hashlib via CNG: `avp_sha256(_hex)`, `avp_md5`, `avp_sha1`, `avp_hasher_*` (incremental), `avp_file_digest(_hex)`, `avp_crc32` (zlib.crc32), `avp_hash64` (non-crypto) |
| `pool.h` | concurrent.futures and threading: `avp_pool_new`, `avp_pool_map` (input order), `avp_pool_for(_range)`, `avp_pool_submit`, futures, `avp_completions_*` (as_completed), `avp_procpool_run` (capped process fan-out), mutex/cond/event/sem/once, atomics, `avp_cpu_count` |
| `clock.h` | time and datetime: `avp_time`, `avp_perf_counter`, `avp_monotonic`, `avp_sleep`, `avp_strftime`, `avp_localtime`, date/datetime/timedelta (`avp_datetime_now`, `avp_datetime_isoformat`, `avp_timedelta_str`), `avp_stopwatch_*` |
| `difflib.h` | SequenceMatcher (`avp_seqm_new_*`, `avp_seqm_opcodes`, `avp_seqm_ratio`, `avp_seqm_no_autojunk`), `avp_unified_diff(_lines)` |
| `csv.h` | csv reader/DictReader/writer/DictWriter (excel, excel-tab, unix), `avp_lines_*` line iteration, `avp_tsv_split` |
| `fmt.h` | Python formatting: `avp_float_repr`, `%` operator `avp_pyfmt` / `avp_pyprintf`, format specs `avp_format_i64/f64/str`, `avp_py_round`, `avp_floordiv_i64` / `avp_mod_i64`, `avp_repr_str` and container reprs, `avp_math_fsum` |
| `cache.h` | binary cache files replacing pickle: writer `avp_cw_*`, reader `avp_cr_*`, `avp_cache_write` (atomic) / `avp_cache_open`, append-only logs `avp_log_*` |
| `win.h` | ctypes/winreg/msvcrt calls: `avp_reg_get`, named mutex, `avp_filelock_try`, `avp_proc_list`, `avp_pid_alive`, `avp_isatty`, `avp_console_width`, `avp_undecorate_symbol`, `avp_win_error_str` |
| `test.h` | unit tests (test files only): `AVP_TEST(name)`, `AVP_TEST_MAIN("suite")`, `CHECK`, `CHECK_MSG`, `CHECK_EQ_I64/U64/F64`, `CHECK_STR`, `CHECK_CZ`, `CHECK_MEM`, `REQUIRE`; runner `--filter`, `-v` |

## Not there yet

No regular-expression engine (pcre2 not vendored; `cc_*` and `shsm_text` hand-match their few patterns), no sqlite,
no capstone disassembler (compiled listings come from `dtk elf disasm`), no zlib (crc32 only), no sockets/http, no git
helpers (spawn `git` with `avp_run`), no C/C++ lexer, no DOL reader outside `cc_codecmp` (`cc_project_dol`,
`cc_dol_read`), no asmindex (tools/common/asmindex.py), no mangler (only `shsm_demangle`), no general YAML reader
(`shsm_repo` reads the few config.yml fields it needs). Add a missing piece inside your tool, or as a new `lib/`
module with its own `.md`; a third-party library goes in `third_party/` ("Vendoring a library").

## Provenance

`core/`, `cmake/tools_stamp.cmake`, `cmake/entry_points.cmake`, and the structure of `CMakeLists.txt` and
`CMakePresets.json` are taken from the `tools/c` directory of another project of the author's at commit `49e811503d7ba5c9cff92160557df057c50a8ab3`
(that project's C tools). Its vendored libraries were not taken; this project's own go in `third_party/`
(each with its `VERSION`, licence and `NOTICE`).

Taken unchanged:
- `core/include/avp/`: `arena.h args.h base.h cache.h clock.h csv.h difflib.h fmt.h fs.h hash.h json.h map.h pool.h
  proc.h sort.h str.h test.h vec.h win.h` (`avp.h` and `hash.h` edited, below).
- `core/src/`: `arena.c args.c args_format.c args_internal.h avpcore.c base.c cache.c clock.c csv.c csv_tsv.c
  difflib.c fmt_*.c fs_*.c fs_priv.h json*.c json_internal.h map.c map_pyintset.c pool.c proc.c sort.c str*.c str*.h
  vec.c win.c win_err.c` (`hash.c` edited, below).
- `core/tests/`: the tests and corpora of the kept modules: `arena base cache clock csv difflib fmt fs_dir fs_io
  fs_path fs_text hash hypot json map map_pyintset_dict map_pyintset_or pool proc sort str str_base64 str_casefold
  struct_cond win` (`*_test.c` with their `*_corpus.h`/`*_expect.h`, and `args_test.c`/`args_corpus.h`).
- `cmake/tools_stamp.cmake`.

Left out (they need capstone, pcre2, sqlite or zlib, or are specific to that project): `disasm` (capstone), `re` (pcre2), `zip*`
(zlib), `http`, `mcp*`, `pe*`, `pyrandom`, with their headers and tests, plus `core/tests/proc_debug_test.c`
(debugger-mode process tests), `debuggee/`, `third_party/`, `cmake/help_no_py.cmake`, the `tools-debug` presets, and all
of that project's `lib/`, `tools/` and `tests/`.

Changes to copied code:
1. `core/include/avp/avp.h`: removed the `#include` lines of the dropped headers `zip.h`, `pyrandom.h`, `pe.h`,
   `disasm.h`, `re.h`.
2. `core/src/hash.c`: removed `#include <zlib.h>`; `avp_crc32` (was a loop over zlib's `crc32`) is now a
   table-driven CRC-32 (polynomial 0xEDB88320, table built once with `InitOnceExecuteOnce`), the same values
   (`hash_test` and `cache_test` check them).
3. `core/include/avp/hash.h`: the comment line naming zlib-ng's crc32 now names the table CRC-32.
4. `cmake/entry_points.cmake`: dropped the `debuggee/` exclusion (no such directory here).
5. `CMakeLists.txt`: targets renamed (`avpcore` -> `shsmcore`, `avptools` -> `shsmtools`, project `shsmtools`,
   variables `AVP3_*`/`AVP_*` -> `SHSM_*`, cache variable `SHSM_TOOLS_OUT`); `third_party` subdirectory and its
   libraries, the 32-bit debuggee ExternalProject, the `mcp_link_all_test` and `hk_selftest_modes_test` special
   cases and the `help_no_py` test removed; `shsmcore` links only `bcrypt shlwapi advapi32 dbghelp`;
   `CMAKE_VS_GLOBALS VcpkgEnabled=false` added (a user-wide vcpkg integration otherwise runs `pwsh.exe` after every
   link); an empty `lib/` gives an INTERFACE `shsmtools` that still carries the `lib` include directory.
6. `CMakePresets.json`: generator "Visual Studio 17 2022" (another project of the author's: "Visual Studio 18 2026"); only the Release `tools`
   presets and the `tools`/`tools-build` workflows.
7. `CMakeLists.txt`: that project's `tools/*_main.c` rule replaced by one directory per tool (`tools/<name>/`,
   `cmake/shsm_tool.cmake`, new); CTest labels `core`, `lib`, `<name>`; `cmake/tools_stamp.cmake` also hashes the
   `.c`/`.h` files under `tools/`.

Names kept from another project of the author's on purpose: the `avp_`/`Avp` API, the environment variables `AVP3_ROOT` (repository root,
`avp_root()`) and `AVP3_PROC_CEILING` (process time ceiling, `proc.h`). MSVC 2022 (14.44) raised no new warnings
on the copied code.
