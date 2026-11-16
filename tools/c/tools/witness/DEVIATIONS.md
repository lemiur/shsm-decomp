# witness: accepted differences from tools/witness.py

Proven equal to the retired Python tools/witness.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-witness-1. Reads beyond the end of a malformed executable
- Behaviour: when a truncated or malformed ELF or ISO image makes the reader index past the end of the file (a
  section, header or directory record that points outside the data), or function recovery reaches an undecoded
  address, the Python fails with an uncaught `struct.error`, `IndexError`, `ValueError` or `KeyError` (traceback,
  exit 1); the C tool raises a ValueError-class (KeyError-class for the undecoded address) error with its own
  message (`... offset N is outside the file`), reported as `witness: error: ...` with exit 2.
- Reason: only broken inputs reach it; the retail image and the synthetic test images never do. The Python's
  behaviour there is a crash, not a contract.

## D-witness-2. Order of the PS2 string sets
- Behaviour: a PS2 function's `strs` (the `strs` rows of the database) are iterated in insertion order; the
  Python's `set` of str iterates in an order that depends on `PYTHONHASHSEED` (differs between runs).
- Reason: the Python order is not reproducible itself; the rows are the same set per function (checked against the
  Python's database: 3,085 rows, same per-function sets, every other PS2 row identical in order).

## D-witness-3. Order of the string-anchor and constant vote keys (evidence texts)
- Behaviour: `Correspondence.string_anchors` iterates `wby.keys() & pby.keys()` (a set of str) and
  `kw.keys() & kp.keys()` (a set of (text, int) tuples), and `constant_candidates` iterates `uw.keys() & up.keys()`
  (a set of ('f' | 'i', int) tuples); the Python's order of these sets follows the randomised str hash
  (`PYTHONHASHSEED`). The C iterates them sorted: texts by code point, (text, int) by text then value, the 'f' keys
  before the 'i' keys, each by value. That order decides (a) which strings or constants an evidence text lists when
  more than two strings or three constants tie for the places, and in which order; (b) the "+ N shared callee(s)"
  count of a constant pair (N counts the callees already mapped when the pair is accepted, so it depends on which
  constant pairs were accepted earlier in the same pass); (c) the order of a function's string-inlined rows in
  `inl`, and of the pairs in insertion order, hence of the "call set" rows of `inl` (the `pairs` table itself is
  keyed and read back by `wii`). The pairs, their tiers, the validation figures and the set of inlined rows do not
  depend on it.
- Reason: the Python order is not reproducible itself. Evidence: Python databases built with `PYTHONHASHSEED` 1, 2,
  3 and 4 and the C database, compared with `diff/compare_db.py`: every (wii, ps2, tier) pair, every
  (wii, helper, parent) row and every other table identical; the evidence texts equal once normalised as the script
  documents (count, suffix, and the listed items as a set when all are listed). The Python seeds differ from each
  other in the same rows as from the C.

## D-witness-4. Order of the Wii string sets
- Behaviour: a Wii function's `strs` (string literals from the DOL and the asm index) are iterated in insertion order
  (the order of the index queries); the Python's `set` of str follows `PYTHONHASHSEED`, as in D-witness-2.
- Reason: as D-witness-2: the `strs` rows of each function are the same multiset (checked against the Python
  databases of four seeds), and the same (side, fn) sequence of rows.

## D-witness-5. Listing operands and index rows the retail tree never holds
- Behaviour: an immediate written with a leading zero (`010`) makes the Python's `int(text, 0)` raise (traceback);
  the C reads it as decimal. A `strings` row whose text is NULL gives the Python a ('s', None) run item stored as
  NULL; the C stores the empty text.
- Reason: dtk writes no such operand and the index no such row (the retail tree's database is identical, above).

## D-witness-6. Whitespace in listing lines (the known-helper site scan)
- Behaviour: `witness helpers` counts the known-helper sites with the fragments port's `known_sites`
  (`fragments_known_functions`, tools/c/tools/fragments; its D-fragments-1 is this entry). Its line patterns read `\s` as ASCII whitespace and the
  separators `\x1c`-`\x1f`; the Python's `re` (and `str.strip`) also take non-ASCII Unicode whitespace (U+0085, U+00A0,
  U+2000...) for `\s`.
- Reason: dtk writes listings with ASCII spaces and tabs only; the sites found on the retail tree are identical
  (harness: `helpers`, `helpers --json`, `helpers --lib game` and the synthetic listing with each helper shape, a
  quoted `.fn` name and a repeated name).

## D-witness-7. Timings
- Behaviour: the `load N s` figure of `stats` (Witness.load_seconds), `this run rebuilt in N s`, and the build seconds
  a build writes into meta (`build seconds: PS2 ..., Wii ..., mapping ..., validation ...`, and `seconds` under
  `--json`) are the C tool's own timings.
- Reason: wall-clock figures. The harness normalises `load`, `this run rebuilt in` everywhere and the build seconds
  only in the synthetic-checkout cases (each side builds its own database there; on the real tree both read the same
  database, so the figures are identical); the stale-stamp cases (`@fixture:stale*`) also drop the rebuild note,
  because the Python side runs first and leaves the C side a current database.

## D-witness-8. A Wii function without an address
- Behaviour: when the asm index yields a Wii function whose address or size is NULL (a listing `.fn` without dtk's
  `# .text:... | 0x... | size: ...` header), the Python fails in `resolve` and `_pair_line` with a TypeError
  (`'<=' not supported ...`, `%X format: an integer is required`, exit 2); the C tool reads the address as None
  (WT_NONE) and goes on.
- Reason: dtk writes the header before every function; the retail tree has none without it. Found with a hand-written
  fixture listing, which the harness now writes with the headers.

## Header for other tools

`witness_lookup.h` (static library `tool_witness`; link with `LIBS tool_witness`) stands alone (the core runtime and
`shsm_repo.h` only): `wt_lookup` (= `lookup(r, address, limit=3)`: read-only, never builds, refreshes or checks the
database), `WtLookup` / `WtLookupHelper`, `wt_db_path` and `WT_NONE`. The model (`witness.h`, `witness_db.h`,
`witness_map.h`, `witness_ps2.h`) and the subcommands (`witness_cmd.h`) are internal to the tool.

## fragments' known sites

`helpers` (without `--no-known`) needs tools/fragments.py `known_sites` and `KNOWN`: witness links the fragments
library (`tool_fragments`, `fragments_known.h`) for them, as the Python imports fragments.
