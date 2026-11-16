# shsm_asmindex: the persistent index of the retail asm listings

Ports `tools/common/asmindex.py`: the sqlite index `build/index/asm.sqlite` of the current listings (`shsm_asm`), with
the same schema, schema version (2), meta rows (`schema`, `built`) and SQL text, so that an index written by either
language is read and refreshed by the other without a rebuild. sqlite is the vendored amalgamation
`third_party/sqlite` (3.50.4, the version CPython 3.14 links). Header `shsm_asmindex.h`.

## Opening and refreshing

`shsm_asmindex_open(&(ShsmAsmIndexOpts){...})` is `AsmIndex.open(repo, refresh, rebuild, jobs, db_path, asm_dir,
readonly)`; zero fields are the Python defaults (`no_refresh` false = refresh). As in the Python:
- the writer connection uses WAL, `synchronous=NORMAL`, a 120 s busy timeout; a missing or older schema is rebuilt
  once, in one `BEGIN IMMEDIATE` transaction (`ix->rebuilt`, `rebuilt_files`, `rebuilt_seconds`);
- refresh compares each current listing's size and `mtime_ns` with the `files` table, parses new and changed listings
  (a thread pool, largest first, results in work order, so file ids are assigned as the Python's process pool
  assigns them), replaces their rows and re-resolves cross-listing targets under the write lock; nothing is written
  when nothing changed;
- read-only opening (`readonly`) uses a read-only connection and lets a short-lived writer bring a missing, older or
  stale index up to date first; a read-only index that is out of date raises `SHSM_EXC_SQLITE` on refresh;
- `last_refresh` (`ShsmRefresh`: scanned, parsed, removed, parse_seconds, seconds, ignored, note) and
  `shsm_refresh_json` (the Python dict); `shsm_asmindex_clear`, `shsm_asmindex_stats` / `shsm_stats_json`,
  `shsm_asmindex_total_changes`;
- `shsm_asmindex_is_current` (new in C): true when no listing on disk is new, changed (size or mtime) or gone since
  the index was written; a caller that opened with `no_refresh` uses it to tell a current index from an out-of-date one
  without writing (a test of the real tree skips on an out-of-date index instead of failing on rows that no longer exist).

## Queries

A query target is `ShsmTarget`: `shsm_target_name` (every block so named), `shsm_target_addr` or `shsm_target_sym`
(exact), as the Python's str / int / SymRow arguments.

| Python | C |
|---|---|
| `symbols_named`, `symbol(name)`, `symbol(addr)`, `at`, `containing`, `unit_of` | `shsm_asmindex_symbols_named`, `_symbol`, `_symbol_at`, `_at`, `_containing`, `_unit_of` |
| `unit_symbols`, `functions`, `search` | `_unit_symbols`, `_functions`, `_search` (limit `SHSM_NONE`: none) |
| `function`, `object`, `read` | `_function`, `_object`, `_read` (an `ShsmAsmBlock` re-parsed from the listing; free its file) |
| `refs_to`, `refs_at`, `refs_from` (kinds) | `_refs_to`, `_refs_at`, `_refs_from` (kinds array, NULL for any) |
| `callers`, `callees`, `call_graph` | `_callers`, `_callees`, `_call_graph` (`ShsmCaller`, `ShsmCallee`, `ShsmCallEdge`) |
| `data_refs`, `data_targets` | `_data_refs`, `_data_targets` (`ShsmDataRef`) |
| `strings_of(follow_pointers)`, `find_strings`, `users_of_string`, `floats_of` | `_strings_of`, `_find_strings`, `_users_of_string`, `_floats_of` |
| `ctors`, `dtors`, `section_ranges(merge)`, `section_of` | `_ctors`, `_dtors`, `_section_ranges`, `_section_of` |
| `sql(query, params)` | `_sql(a, ix, query, params)`: params and rows as JSON |
| `SymRow.to_dict()` | `shsm_symrow_json` |

## Proofs (`tests/shsm_asmindex_real_test.c`, real tree; `tests/shsm_asmindex_real_dump.py` is the Python side)

- build: the Python and the C build the index into two files; every table compared row for row (rowids included):
  171,175 rows, 0 differ (meta `built` is a time stamp);
- queries: 7,947 queries over functions and objects sampled in every library of `libraries.tsv`, answered by the
  Python from the C-built file and by the C from the Python-built file, all equal;
- incremental: one listing's mtime moved and restored; each refresh parses that one file in both languages and the
  tables stay equal;
- concurrency: a Python reader during two C rebuilds and a C reader during a Python rebuild: no error, no
  inconsistent read.

Unit tests (`tests/shsm_asmindex_test.c`) port `TestIndex`, the index cases of `TestStaleListings` and
`TestIndexExact`. Deviations: `shsm_asmindex.DEVIATIONS.md`.
