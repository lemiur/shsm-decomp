# fragments: deviations from tools/fragments.py

Proven equal to the retired Python tools/fragments.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 618 cases, 0 differ.

The entries
below are differences on inputs that do not occur in this project's listings or sources.

## D-fragments-1. Whitespace in listing lines (the known-helper site scan)
- Behaviour: `known` finds the known-helper sites with the line patterns of witness's port (`frag_known.c`, the same
  code as `tools/c/tools/witness/witness_known.c`): `\s` and `str.strip` read ASCII whitespace and the separators
  `\x1c`-`\x1f`; the Python's `re` and `str.strip` also take non-ASCII Unicode whitespace (U+0085, U+00A0, U+2000...).
- Reason: dtk's listings separate fields with ASCII spaces and tabs only (the same ruling as D-witness-6).

## D-fragments-2. Registers per row
- Behaviour: a listing row naming more than eight `r`/`f` registers stops the C tool with an internal error
  (`norm_row` keeps a row's register ids in a fixed array of eight); the Python has no limit.
- Reason: no PowerPC instruction names more than four registers.

## D-fragments-3. Seed grouping without Python's hash()
- Behaviour: none visible. The Python counts seed windows by `hash(tuple)` before grouping them by their exact tokens
  and register renaming; the C groups by the exact key directly.
- Reason: a hash collision in the Python only adds a candidate group that the distinct-function test then drops, so
  both give the same seeds (the identical databases above).

## D-fragments-4. Scanning listings in threads
- Behaviour: none visible. The Python parses the listings in a process pool (`cli.pmap(..., processes=True)`), the C in
  a thread pool; both add the functions to the corpus in work order, so function indices, ranks and ids agree.
- Reason: speed (real tree: scan 0.9 s against 19 s; the whole build 17 s against 221 s).
