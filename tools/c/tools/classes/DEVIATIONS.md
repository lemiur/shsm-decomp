# classes: accepted differences from tools/classes.py

Proven equal to the retired Python tools/classes.py by the differential harness, removed at retirement, commit 66ee84d3.

The readers (`show`, `show --json`, `header`, `tree`, `tree --all`, `tree --json`), find_class's lookups, the stale
and unstamped warnings with and without `--strict`, the input errors and argparse's usage, errors and help print the
same stdout and exit codes as the Python. Under `--base-rule slots` (D-classes-2), `scan` prints the same stdout (with and without `--json`) and writes a classes.json
byte-identical to the Python's once the `inputs.scanner` value (D-classes-1) is set aside: on the real tree, after a
symbols.txt edit and after listing edits (a vtable store removed, a destructor renamed); the kept / restored /
rescanned messages follow the same sequence with each tool on its own out file.

## D-classes-1. The scanner digest and the cache key are the C tool's own
- Behaviour: the Python's scanner digest is sha256(classes.py's bytes + b"\0" + sys.version + b"\0" + the splits
  file); the C tool cannot know either, so its digest is sha256(b"classes-c\0" + avp_tools_stamp() + b"\0" + the
  splits file's bytes, when the file exists), and its content key starts with b"classes-scan-c1\0" instead of
  b"classes-scan-1\0" (the rest of the key is composed the same way, and the cache is the same machine-wide cache on
  the same disk format). The only difference in a classes.json the C tool writes is the value of
  `inputs.scanner`. A classes.json written by the other producer therefore never counts as current for `scan`
  (it is restored from the cache or rescanned and rewritten), and neither producer ever restores the other's cache
  entry.
- Reason: the digest exists to tell which program produced classes.json; the two programs are different producers.
  The readers' staleness check (`stale_reason`) compares only the input stamp, never the scanner, so either
  producer's file reads as current to either reader.

## D-classes-2. The base rule: `scan --base-rule {ctor,slots}`, ctor by default (owner's ruling, 2026-11-13)
- Behaviour: a deliberate deviation. The C scan keys a base by constructor and destructor evidence (`ctor`,
  `ClassesScanOpts.base_rule` 1, below) by default; the Python keys it by vtable-slot overlap, which names the wrong
  base, or no base, for 29 of the 627 classes of the real tree (17 of them move from `unk_800D2DE4` to
  `unk_803E2548`, among them unk_8003C454, CGhostPhotoTrigger, CZone and cPhone; `unk_800D2DE4` and `unk_800FEB28`
  themselves derive from `unk_803E2548`, ZoneLink from CTerrainTraversal). `--base-rule slots` selects the Python's rule, under which the scan's stdout
  and classes.json are the Python's (D-classes-1 aside). Without the option the rule is `SHSM_CLASSES_BASE_RULE`
  (`slots` or `ctor`) when set, else `ctor`; the readers' quiet scan of a missing classes.json uses the same default.
  `scan --help` lists the option; `--base-rule` on the Python is an argparse error (exit 2).
- Reason: the owner's ruling of 2026-11-13 makes the corrected rule the default; the Python is left as it is until it
  is retired. The harness (`diff/cases.txt`) runs the C side under `% env: SHSM_CLASSES_BASE_RULE=slots`, so every
  comparison with the Python is made under the Python's rule and byte identity holds there; `diff/cases_ctor.txt`
  covers the default: the Python's readers and the C readers on one classes.json written by a default `classes.exe
  scan` print the same (run with `--cases tools/c/tools/classes/diff/cases_ctor.txt`).
- The `ctor` rule (classes_baserule.c, whose header comment is the full statement): a subobject that the derived
  constructor builds by calling its constructor on this+off is a base when the derived constructor afterwards
  stores its own vtable into the subobject's vptr (a table the callee installs at +p, overwritten at this+off+p),
  and a member when it does not; a destructor call with r4 = 0 is a base, with r4 = -1 a member. The base's
  vtable is the table at the overwritten vptr (not the callee's lowest-installed "owner"), its offset is off, and
  its word in the derived vtable comes from the two stores' addends, so `complete_off` is not consulted. The vptr
  evidence (including the reference's inlined-constructor stores) outranks the destructor evidence, which outranks
  the reference's other candidates and slot overlap (still the fallback for groups no evidence places); a member
  verdict at an admitted base's offset is recorded on it as `conflict: ...`. Two refinements (classes_analysis.c,
  `trim_padding` and `inlined_parents`; unit tests `trailing_padding_of_a_base_table` and
  `inlined_parent_without_vptr_store` in tests/baserule_test.c): (1) a dtk-split base table (not a sized `__vt__`)
  whose trailing null words run past the end of a table that vptr evidence derives from it is cut at that end (the
  shortest such table): a derived table repeats every base slot, so those words are padding to the next object's
  alignment, not pure slots (`lbl_803E2548` is 0x20 bytes for five slots; uncut, its image in unk_8003C454's table
  covered unk_8003C454's first own slot at 0x140, and check_rules then reported a false missing override at CPlayer's
  slot 0x254, the override of `fn_8003AE98` that CPlayer.h declares); its own record lists the cut words.
  (2) when the vptr evidence names a grandparent because the parent's constructor and destructor were inlined with no
  vptr store (AreaTriggerBox calls AreaTriggerBase's constructor and destructor; `__vt__19AreaTriggerBoxLogic` is
  stored nowhere), a table that no function installs, descends from the proven base and has the best slot overlap
  with the derived table at that word replaces it, with `slot-overlap (N slots)` and the reference's
  `inlined-parent (grandparent ... reached directly)` note added to the evidence. New evidence strings (in the existing
  `evidence` lists; schema unchanged): `vptr-overwrite this+0xO (F installs V there)`, `dtor-call F (r4=0)`,
  `thunks this-0xOFF (N slots)`, `conflict: ...`; a member's tables go into `member_objects`. A `ctor` scan writes
  `inputs.scanner` as the digest followed by `+base-rule=ctor`, so neither rule's classes.json counts as current
  for the other's scan; the content key composes that value, so each rule has its own cache entries. The readers'
  staleness check (and check_rules.exe's, through `classes_staleness`) compares the input stamp only, so a file
  under either rule is current to them while its inputs are unchanged.

## D-classes-3. Tracebacks
- Behaviour: where the Python fails with an uncaught exception (render_header's TypeError for an override slot whose
  target is null, a KeyError for a derived key no class has, an unreadable or malformed classes.json), the C tool
  prints the same stdout up to that point, exits 1, and writes a short traceback whose last line names the
  exception (`TypeError: expected string or bytes-like object, got 'NoneType'`), not the Python's full stack.
- Reason: stderr is compared by class (non-empty); the frames of a Python stack have no C counterpart.

## D-classes-4. A malformed classes.json fails when it is read
- Behaviour: a classes.json whose members have the wrong type, or lack one the readers use (`key`, `name`,
  `name_source`, a vtable's `symbol`/`address`/`words`, a word's `index`/`offset`/`status`, a field's numbers, ...)
  is a traceback (exit 1) as soon as it is loaded; the Python fails only when it reaches the member, possibly after
  printing part of its output, and prints `None` for a few members the C tool treats as optional (`unit`,
  `size_source` when `size` is set: the Python raises KeyError there only when they are absent).
- Reason: classes.json is written by the scanner, never by hand; a well-formed file reads the same on both sides.

## D-classes-5. The checkout root
- Behaviour: the Python's ROOT is the checkout holding tools/classes.py; the C tool's is the checkout of the
  executable (`shsm_find_root` from the executable's folder), else the one around the current directory. The
  defaults of `--asm`, `--symbols`, `--out` and `--splits` and the `wrote <path>` line are relative to it.
- Reason: the same checkout whenever the executable is the checkout's own build/tools/classes.exe.

## D-classes-6. dtor_funcs iterates in insertion order, not a Python set's hash order
- Behaviour: Analysis._members_and_fields adds a vtable's slot and site destructors as members with
  `for f in self.dtor_funcs.get(vt, ())`, a Python set of strings, which iterates in per-process hash order; the
  C tool iterates in insertion order (the vtable's own dtor sites in site order, then the slot destructors in slot
  order). The order reaches the output only through ties: members are emitted sorted by address (stable), and the
  fields' Counter tie-breaks follow the member order, so two members at one address (or two without an address)
  could come out in another order than one particular Python run.
- Reason: the Python itself is not deterministic there (its output would change with PYTHONHASHSEED). In the real
  tree no class has two members at one address, and the Python's classes.json is byte-identical under
  PYTHONHASHSEED 1, 2 and 12345, and to the C tool's (scanner value aside).

## D-classes-7. Two edge cases of the analysis that the real tree never reaches
- Behaviour: (a) _spans sorts tuples whose third element may be None: CPython's timsort raises TypeError at the
  first None-versus-str comparison it makes, the C tool's stable sort may compare another pair first, so both raise
  (a traceback, exit 1) but on a different pair. (b) Where the Python would raise IndexError (a list index out of
  range in the vtable slot code), the C tool raises ValueError (shsm_cli.h has no IndexError class); both are a
  traceback with exit 1, only the class name on its last line differs. (The TypeError of _names' unused
  `sorted(factory addresses)` when one is None is reproduced, with its own message text.)
- Reason: neither occurs in a dtk listing (every span base is a vtable name; slot indices come from the table's own
  words); stderr is compared by class.

## D-classes-8. The parser's impossible operands and integer literals
- Behaviour: a register operand that is not a register (an operand dtk never writes where analyze_function expects
  `rN`) is treated as "no register", where the Python would crash with a traceback; an integer literal beyond 64
  bits wraps modulo 2**64 instead of growing.
- Reason: dtk writes neither: every register operand is `rN`/`fN`, and every literal fits in 32 bits.

## Header for other tools

`classes.h` (static library `tool_classes`; link with `LIBS tool_classes`): `classes_db_load` /
`classes_db_from_json` (a typed `ClassesDb` of `ClassesClass`: vtable words and groups, bases, derived keys,
members, member objects, fields, ctors/dtors/factories, registry calls, name candidates, each with its JSON object),
`classes_find` (find_class), `classes_by_key` / `_by_name` / `_by_vtable_symbol` / `_by_address`, `classes_method`,
`classes_hex`; the input stamp (`classes_default_paths`, `classes_input_stamp`, `classes_stamp_key`,
`classes_stale_reason`); the scan driver (`classes_check_inputs`, `classes_scanner_digest`, `classes_content_key`,
`classes_current`, `classes_run_scan`, `classes_dump_file_text`); the readers (`classes_show`, `classes_show_json`,
`classes_header`, `classes_tree`) and `classes_ctype`.
