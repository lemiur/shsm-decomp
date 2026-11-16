# shsm_asmindex: accepted differences from tools/common/asmindex.py

## D-shsm_asmindex-1. Threads instead of processes for parsing
- Behaviour: the Python parses changed listings in a process pool (`jobs` processes); the C uses the core thread pool
  (`jobs` threads). The work order (largest first when `jobs > 1` and more than four files, else listing order) and
  the order of the results are the same, so rows and file ids are the same.
- Reason: the table comparison on the real tree (171,175 rows, 0 differ) and the unit tests.

## D-shsm_asmindex-2. A larger page cache on the writer connection
- Behaviour: the C writer sets `PRAGMA cache_size=-262144` (256 MiB); the Python keeps sqlite's default (2 MiB). A cold
  build writes about 2.5 times faster (the index pages of the whole build stay in memory).
- Reason: the cache size is a connection setting, not stored in the file; the rows written are the same (table
  comparison above).

## D-shsm_asmindex-3. sqlite3 errors are one exception code
- Behaviour: `sqlite3.OperationalError` (and any other sqlite error) is `SHSM_EXC_SQLITE`, with sqlite's message or
  the Python's own text ("no index at ...", "read-only index is out of date; open it with refresh=True").
  `shsm_exc_class` does not know the code, so an uncaught one prints the message after `<prog>: error:` as any
  error does.
- Reason: the shared exception list of `shsm_cli.h` is not this module's to change; the CLI output (message, exit 2)
  is the same.

## D-shsm_asmindex-4. A parse failure in a worker
- Behaviour: when listings fail to parse, the Python re-raises the first failure `pool.map` yields; the C raises the
  failure of the lowest work index among those that ran (later work is skipped). Both write nothing.
- Reason: the same class of error and no partial index; a listing that cannot be read is a broken split.

## D-shsm_asmindex-5. function()/object() also take a SymRow
- Behaviour: `shsm_asmindex_function` / `_object` with `shsm_target_sym` resolve by the row's address; the Python's
  `_resolve` accepts only a name or an int (a SymRow raised inside sqlite's parameter binding).
- Reason: a superset; no Python call reaches it.
