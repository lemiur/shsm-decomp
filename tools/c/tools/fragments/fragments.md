# fragments: repeated parts of functions (port of tools/fragments.py)

`build/tools/fragments.exe` is the former `python tools/fragments.py`: the same subcommands (`list`, `show`, `in`, `known`,
`stats`, `source`), options, text and `--json` output, exit codes, and the same fragment database
`build/index/fragments.sqlite` (schema 4: tables `meta`, `fragments`, `occurrences`, rebuilt when the asm index's file
stamps or `--window`/`--sites` change, or on `--rebuild`), so a database written by either language is read by the
other without a rebuild.

## Files

| file | Python |
|---|---|
| `frag_norm.c` | `frame_flags`, `norm_row`, `_letters`, `rename_rows`, `_fmt_varies`, `render` |
| `frag_corpus.c` | `Corpus` (`add`, `canon`, `seeds`, `extend`, `repeats`), `boilerplate`, `drop_contained` |
| `frag_db.c` | `_stamp`, `db_path`, `_scan_file`, `build_corpus` (a thread pool, results in file order), `_fragment_records`, `open_db` |
| `frag_model.c` | `Frag`, `Occ`, `_matched_addresses`, `lib_matches`, `Fragments`, `frag_of`, `fragments_at`, `in_function` |
| `frag_cmd.c` | `_occ_summary`, `_compact`, `select`, `cmd_list`, `cmd_show`, `cmd_in`, `cmd_stats`, `cmd_known` |
| `frag_known.c` | `scan_listing`, `_site_hash`, `_site_display`, `_site_savekey`, `KNOWN`, `known_sites` |
| `frag_source.c` | `_local_names`, `source_bodies`, `source_rows`, `_single_call`, `_balanced`, `trim`, `source_candidates`, `_source_text`, `_source_paths`, `cmd_source` |
| `frag_cli.c`, `main.c` | `main`, `run` |

`source` reads function bodies with the rule checker's lexer and parser (`tool_check_rules`: `rules_lex.h`,
`rules_parse.h`), as the Python imports `check_rules.Parsed`.

## Public headers (link `tool_fragments`)

- `fragments.h`: `fragments_in_function(a, root, address, min_functions, level, &out)`, the Python's
  `in_function(address, repo, min_functions, level)`: the fragments inside one function as the `in` subcommand selects
  them, read from the database as it stands (never built); false (None) when the database is absent or unreadable.
- `fragments_known.h`: `FRAGMENTS_KNOWN` (`KNOWN`: name, stated count, note), `fragments_known_sites` (`known_sites`:
  every site as function, anchor address, offset) and `fragments_known_functions` (the sites reduced to the set of
  function names per helper). `witness helpers` uses a private copy of this scan
  (`tools/c/tools/witness/witness_known.c`, `wt_known_sites`); `wt_known_sites(a, r, ix, out)` is
  `fragments_known_functions(a, r, ix, out)` with `WT_NKNOWN == FRAGMENTS_NKNOWN` and the names in the same order, so
  witness can drop its copy and link `tool_fragments` (its CMake `LIBS`) without a behaviour change.

`frag.h` is internal (the model, corpus and command entry points the unit tests use).

## Proof

Differential cases in `diff/cases.txt` (generators in `diff/cases.py`: the real tree, a synthetic checkout with
repeated helpers and source files, and checkouts each side rebuilds). `diff/compare_db.py A B` compares two fragment
databases table by table (timings excluded); the real-tree database built by each language compared equal. Unit
tests in `tests/` port `tools/test_fragments.py`'s row, repeat, render and source tests. Deviations: `DEVIATIONS.md`.
