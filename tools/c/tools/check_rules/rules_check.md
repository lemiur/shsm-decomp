# rules_check: the Checker's per-file phase (stage 2)

Ports the part of `tools/rules/check_rules.py`'s `class Checker` that `check_file` runs for one parsed file, with the
project-wide state those checks read. Headers: `rules_check.h` (this API); the parse model is `rules_parse.h`
(`rules_lex.md`).

## Files

| file | Python |
|---|---|
| `rules_config.c` | `parse_units` L4422, `_py_tokens` L4440, `_match_bracket` L4479, `parse_include_dirs` L4493, `Allow.from_markdown` L1905 (the ORIGINAL-ASM and inline-asm-helper sets only), `_strip_main` L1891, main()'s reading of configure.py and docs/exceptions.md L4752-4770 |
| `rules_check.c` | `Checker.__init__`, `parse`, `tu_include_dirs`, `_every_library_dir`, `resolve_include`, `closure`, `status_of`, `add`, `tier_of`, `_set_header_languages`, `_volatile_notes`, `offset_macro_names`, `check_file` (L2030-2310, L2429), `Finding.text`/`to_dict` (L1874), `statements` L2887, `parse_decl` L2914, `_cast_info` L2359, `SCALAR_WORDS`, `DECL_KEYWORDS`, `STMT_KEYWORDS`, `VOLATILE_TYPES` |
| `rules_check_tokens.c` | `check_tokens` L2311 (R22), `check_identifier`/`flush_identifiers` (R7), `_skip_operand`, `_is_operand_end`, `_pointer_names`, `_operand_start`, `_pointer_term`, `check_offset_macro_uses`, `check_r4` (R4), `check_volatile`, `_is_delay_counter`, `_volatile_kind` (R17), `check_casts`, `_local_type` (R19, R26): L2329-2884 |
| `rules_check_function.c` | `check_function` (R14, R17), `_loop_block`, `_index_series`, `check_expressions`, `_stmt_ranges` (R17, R35), `_macro_bodies`, `_ids_in_used_macros`, `check_locals` (R2, R17, R25), `check_control`, `check_condition`, `_after_condition_declarator`, `_depth0_assign` (R35), `check_literals`, `_floaty`, `_explained` (R9, R11), `check_function_meta` (R15, R25, R33): L2978-3425 |
| `rules_check_file.c` | `check_directives` (R3, R22, R37), `check_define` (R4, R17, R31), `check_comments` (R17, R21, R37), `check_asm` with `unit_key` (R18), `check_declarations` (R2, R13, R17): L3428-3577, L1937 |

## API

- `RlConfig`: `unit_status` (path under src/ -> `AvpStr*`), `include_map` (path under src/ -> `AvpStrVec*`),
  `original_asm` / `asm_helpers` (sets of `"unit\x01function"`), `allow_source`. `rl_config_load(a, cfg, root,
  configure, allow)` reads them as main() does (NULL paths: `root/configure.py`, `root/docs/exceptions.md`; a missing
  file leaves its tables empty); `rl_parse_units`, `rl_parse_include_dirs`, `rl_allow_from_markdown` take texts.
- `RlChecker`: `rl_checker_new(a, root, extra, cfg)` loads `load_sources(root)` and appends each extra path not
  loaded (main()'s scanned paths; NULL when one cannot be read); `rl_checker_from(a, paths, texts, cfg)` takes
  in-memory sources (a Python `{path: text}`; tests). `rl_checker_parse` is `Checker.parse` (one arena per file; a
  parse the lexer rejects sets `ck->failed`, the Python run's AttributeError); `rl_checker_closure` returns
  `sorted(closure(path))` (every caller sorts or ignores order); `rl_checker_status_of`.
- `rl_checker_prepare(ck)`: run()'s prelude: `_set_header_languages()` (sets `P->is_cpp` of every `.h`/`.inc`
  header), `noted_volatile = _volatile_notes()`, and `offset_macro_names()` (memoized on first use in the Python;
  both read only parses).
- `rl_check_file(ck, P)`: `check_file(P)`; findings are appended to `ck->findings` (`RlFinding`: path, line, rule,
  tier, msg, func (`p == NULL` is None), status, reason). `check_function_meta` sets `RlFunc.has_addr`/`addr`.
- `rl_add` is `Checker.add` (drops R17 inside a `no copy in retail` body; returns the index or -1);
  `rl_finding_text` is `Finding.text()`, `rl_finding_json` writes `to_dict()` without `units` (always `[]` in this
  phase).
- Shared helpers: `rl_statements` (statements are contiguous token ranges), `rl_parse_decl` (declarators with the
  name token's index relative to the statement), `rl_cast_info` (close index or -1), `rl_strip_ws`
  (`re.sub(r"\s+", "", s)`), `rl_fmt`.

Python dicts keyed by tuples are string maps keyed `"a\x01b"`; `check_identifier`'s `seen` keeps insertion order in
`ph_v`; `_pointer_names`' memo (keyed by path, line and name) and `_macro_bodies`' (keyed by path) live per file,
since each path is checked once. `rev` in the R4 helpers is `match` itself (`pair_brackets` maps both ways).

## The findings dump (stage-2 proof)

    check_rules --dump-findings [FILE ...] [--dump-list LIST] [--dump-rules R4,R17] [--root DIR]

The Checker built as main() builds it with no cache, run()'s prelude, then `check_file` of each scanned path: every
source of `load_sources` in sorted order without FILE, else the FILEs (and the lines of LIST) in the order given,
read on top of the sources. One compact JSON object per finding (`to_dict()` without `units`), in emission order,
optionally only the listed rules; a run the Python cannot complete prints `{"error":"AttributeError"}`. The reference
is `diff/dump_findings.py` (also reached through `diff/dump_facts.py --dump-findings`, the harness's command).

## Stage 3: the rest of Checker.run

| file | Python |
|---|---|
| `rules_classes.h/.c` | `demangle_member` L1376, `mangled_params`, `_demangle_parts`, `SrcMethod`, `_param_types`, `SrcClass`, `_member_function`, `_class_members`, `parse_cpp_classes` (`Parsed.cpp_classes`, memoized in `P->cpp_classes_memo`), `load_classes`, `input_stamp`, `_stamp_key`, `classes_stale`, `template_name`, `RetailIndex`, `slot_implementation`, `retail_complete_slots` (L1366-1853) |
| `rules_vtables.c` | `_src_classes`, `_resolve_base`, `_virtual_names`, `_virtual_sigs`, `_peers`, `_pick_overridden`, `_model`, `_model_from`, `check_vtables`, `_r13`, `_compare_subtables`, `_params_agree`, `_compare_vtable` (L3580-4048; R13) |
| `rules_cross.c` | `_unit_private`, `_definition_guards`, `_defined_at_first_include`, `_compiled_out_with`, `check_cross_file`, `_cast_to` (L4050-4258; R1, R2) |
| `rules_final.c` | `run` L2205, `apply_exceptions` with `_exception_cover` (R0), `apply_allow`, `compute_units`, `blocking`, `build_functions`, `exit_code` (L4261-4414), and the final dump |
| `rules_config.c` | `Allow.from_markdown` now with `rulings` and `pending` |

API additions (`rules_check.h`): `RlChecker.scan` (set before `rl_checker_run`), `classes_data`/`classes_note`
(main()'s `load_classes` result: `rl_load_classes`), and what run() leaves: `checked`/`checked_set`
(`checked_paths`), `r13` (`RlR13`), `exceptions` (`RlException`, `cover` as `lo`/`hi`, `refused`), `functions`
(`RlFunctionRec`, counts in insertion order), `RlFinding.units`; `error` names the exception class when the Python
run would raise (a malformed classes.json address or vtable group), `failed` the lexer's AttributeError as before.
`rl_exit_code`, `rl_blocking`, `rl_closure_has`, `rl_unit_key`, `rl_resolve_include`, `rl_is_source_path`.
JSON values of classes.json are `AvpJson` (identity = pointer, as the Python's `is`); `rl_py_str` is `"%s" % v`.

## The final dump (stage-3 proof)

    check_rules --dump-final [FILE ...] [--dump-list LIST] [--root DIR] [--classes P] [--configure P] [--allow P]

The Checker built as main() builds it (configure.py, docs/exceptions.md, `load_classes` of `--classes` or
`os.path.join(root, "build/R5WEA4/classes.json")`, no cache) over the scan (default: every source in sorted order),
`run()`, then compact JSON lines: `{"checked": [...]}`, every finding's `to_dict()` (with `units`) in findings order,
`{"exception": {...}}` per RULE-EXCEPTION record (all keys, `cover` as a list, `refused` when set), `{"function":
{...}}` per `build_functions` record, `{"r13": {...}}`, `{"exit": N}`. A run the Python cannot complete prints
`{"error": "<class>"}`. The reference is `diff/dump_final.py` (also reached through `diff/dump_facts.py`).
`diff/make_trees.py` turns every Checker that `tools/rules/test_check_rules.py` runs into a scratch tree (sources,
configure.py, docs/exceptions.md, a freshly stamped classes.json, scan.txt) for the `@trees` cases.

## Command line (stage 4: `rules_cli.h/.c`, `main.c`)

| Python | C |
|---|---|
| `UsageError` | a `const char *err` out-parameter; main writes `check_rules: <err>` and returns 2 |
| `collect_files` L4629 | `rl_collect_files` (avp_walk, dirs and files sorted; `err == NULL` on failure is relpath's ValueError) |
| `units_to_files` L4646 | `rl_units_to_files` (over `RlSources.paths`, load_sources order) |
| `changed_files` L4658 | `rl_changed_files` (two `git` runs through `avp_run`, cwd root, check; CalledProcessError's text rebuilt) |
| `summarize` L4690 | `rl_summarize` (`ck._all_functions` is its `all_functions` argument) |
| main's shown list | `rl_shown_findings` (the `--tier` set, stable sort by path, line, rule) |
| main's text block | `rl_report_text` (the joined lines plus print's newline) |
| main's `doc` | `rl_report_json` (`json.dumps(doc, indent=2) + "\n"`; `HAND_REVIEWED` sorted by number) |
| `main` L4733 | `main.c` `run`: argparse (`avp_args_parser`, prog `check_rules.py`), the `--configure`/`--allow` checks, load_sources, config, scan, the scanned files not loaded, `rl_checker_from`, `rl_load_classes`, `rl_checker_run`, the report |

`rl_json_exception`, `rl_json_function` and `rl_json_r13` (rules_final.c) write the records both the final dump and
the `--json` document print. A run the Python ended with a traceback prints `rules.exe: error: <class>: <reason>` on stderr, exit 1 (D-check_rules-9).
