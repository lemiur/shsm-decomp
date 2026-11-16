# hashes: accepted differences from tools/hashes.py

Proven equal to the retired Python `tools/hashes.py` by the differential harness, removed at retirement, commit 66ee84d3.

## D-hashes-1. Order of the format-derived rows of `corpus` and `hashvals`
- Behaviour: the names a kept format derives ("%s.snd" applied to every identifier-like corpus string) are inserted
  into `corpus` and `hashvals` in byte order. The Python inserts them in the iteration order of a set of bytes, which
  hash randomisation changes from run to run.
- Reason: the Python's order is not reproducible by itself (two Python rebuilds of the same inputs differ in 19,866
  rows of these two tables); the C order is deterministic. The contents are equal: `compare_db.py py.sqlite c.sqlite
  --unordered corpus hashvals` reports 0 differ, every other table equal row for row, the same schema text (the stamp
  excepted). No output depends on the order (two
  derived names would need the same 32-bit hash value).

## D-hashes-2. The database is rebuilt in one transaction
- Behaviour: the C build runs from `DROP TABLE` to the stamp row inside one `BEGIN IMMEDIATE ... COMMIT`. The Python
  commits every statement of its two `executescript` blocks on its own and the inserts in between, so a reader or a
  second build running at the same time can see a half-built database.
- Reason: the final contents are identical (the schema text is the same statement text). The Python's behaviour is a
  defect: during a harness run with a concurrent rebuild, a Python `stats` counted 301,604 constant sites (twice the
  real 150,802) and a second Python build failed with `index hv_value already exists`; the C reader sees the old or
  the new database, never a mixture. The single transaction also makes the C build about three times faster (each
  commit is a disk flush).

## D-hashes-3. `find --words`: words tried in file order
- Behaviour: the words of `--words` files are matched in the order they first appear in the files. The Python
  iterates a set of bytes (random order per run).
- Reason: the order shows only when several words of the files have the same hash value under one function; the
  Python's own output then varies between runs.

## D-hashes-4. Inputs the retail listings do not contain
- Behaviour: (a) a class-registry call whose r3 holds a value loaded through a pointer (`lwz`) gives no pair; the
  Python would try to store the tuple and stop with sqlite3.ProgrammingError. (b) A pooled-string offset (`addi rX,
  rY, N` without relocation) beyond 64 bits is not read; the Python would add the big integer. (c) The class of a
  function comes from its demangled qualified name (the part before the name, without a return type), where the
  Python reads mangle.parse's scope list; a name whose demangling raises gives no class, where the Python's `vars`
  stopped with the error.
- Reason: none of these occurs in the retail listings: the harness runs `vars --by-class --all`, `--class` and the
  JSON pairs (every function name with a pair, with its class) with no difference, and the databases' `vars` tables
  are equal row for row.
