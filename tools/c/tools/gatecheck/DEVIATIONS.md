# gatecheck: accepted differences from tools/gatecheck.py

Proven equal to the retired Python `tools/gatecheck.py` by the differential harness, removed at retirement, commit 66ee84d3. Last run: 872 cases, 0 differ.

The C tool prints `gatecheck.exe: error:` and the exception line, no frames, exit status 1, where the Python crashes with an uncaught exception.

Timings (best of five, this machine, built tree): `attribute` on the green build: Python 790 ms, C 222 ms.
`layout --all`: Python 2995 ms, C 984 ms. Most of the C time is process start-up (`gatecheck.exe -h` alone takes
200-500 ms here); the layout work for all 350 units takes about 0.3 s.

Unit tests: the 48 tests of tools/test_gatecheck.py are ported one to one (48 C tests in `tests/*.c`, each file
starting with its `Python test -> C test` map). The CLI tests run `build/tools/gatecheck.exe` on fixtures in a temporary
root; `test_shift_hint` uses a fixed LCG for its 1024 random bytes instead of `random.Random(1)`.

## Consolidation (not deviations: same behaviour)

No regular-expression engine exists yet; gatecheck's regexes are hand scanners, to be replaced by `shsm_re` later:
- gc_meta.c: `parse_splits` (unit line `^(.+?):(?:\s.*)?$`, range line `^\s+(\S+)\s+start:(0x..)\s+end:(0x..)`),
  `parse_symbols_txt` (`^(.+?) = (\S+?):0x(..);(.*)$`, `size:0x`, `type:(\w+)`, `scope:(\w+)`), `parse_configure`
  (`Object\(\s*(Matching|NonMatching|Equivalent|MatchingFor\([^)]*\))\s*,\s*"([^"]+)"`).
- gc_asm.c: `ANON_RE`, `GAP_RE`, `LOOSE_RE`, the section directive regex, the `.obj` / `.section` / `.rel` /
  `.4byte` / `.string` / `.ascii` / `.string16` patterns, the two dtk address comments, and `int(x, 0)` for `.skip`.
- `\w` is ASCII letters, digits, underscore and every non-ASCII byte (Python: Unicode word characters); the project's
  inputs are ASCII.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `gatecheck.exe: error: <Class>: <message>`, with the same exit code as before.

## Post-retirement changes (after the Python tools were retired)

## D-gatecheck-asm-1. A code section with no sized symbol is one whole object
- Behaviour: `layout`'s code-size check counts an `.init`/`.text` section of the built object that has no sized symbol
  (an assembler section holding only labels, as an assembly source unit's) as one object of the section's size,
  where it counted 0 bytes; the section summary line says `N whole section(s)`. With the retail DOL at hand the
  section's bytes are compared with retail's at the same place in the split, each relocated word masked on both
  sides: equal bytes add a note, the first differing byte is a `CONTENT` difference.
- Reason: the user's ruling of 2026-11-14 allows `.s` source units; MetroTRK's `__exception.s` vector table has no
  function symbol (dtk adds `gTRKInterruptVectorTable` as a label), so the old count read a correct table as empty.

## D-gatecheck-post-2. A coarse retail object covers only built objects inside its span
- Behaviour: `layout` pairs a larger retail object with a run of smaller built objects (`COARSE`) only when the run, from the first object's start to the last one's end, fits inside the retail object; it used to sum the sizes alone, so rwsdk batextur's 4-byte retail `.sdata` literal `lbl_80528EB0` "covered" both built 1-byte `""` literals (+0 and +4) and its neighbour `lbl_80528EB4` was reported MISSING. Each literal now pairs with its own object (a padding size note). Zero-size symbols (the section symbol at +0) were already left out of the built objects.
- Reason: a fix found while adding assembly source units; the run of objects must lie in the span it is said to cover.

## D-gatecheck-post-3. Equal-scoring alignments prefer the nearer offsets
- Behaviour: `layout`'s object alignment (`gc_align_objects`) breaks a tie between alignments of equal total score by the sum of the paired objects' offset distances, where it took the earliest pairing. A run of equal-looking objects (the same `__FILE__` literal repeated, 0x11 bytes each) on the built side is paired with the retail copies at their own offsets, not with the first copies of the run: fmod_os_misc's lone literal at +0x3C was paired with the built literal at +0, which raised `OFFSET`, `CONTENT`, `MISSING` and `EXTRA` findings for a `.data` that equals retail's.
- Reason: a false difference that blocked flipping `fmod/fmod_os_misc.cpp`; the pairing is a presentation of the same bytes, so no verdict on a section whose objects are all different changes.

## D-gatecheck-post-4. Address-named objects paired by position must carry the same name
- Behaviour: `layout` reports `ORDER` when a retail object `lbl_<hex>` and the built object paired with it at its place are both address-named and differ in name. Such objects are anonymous to the pairing (size and content only), so two zero-filled counters of equal size defined in the wrong order (MWCC puts uninitialised small data out in reverse order of definition) paired by position and passed; rppvs.c's `lbl_8052B348`/`lbl_8052B34C` `.sbss` counters flipped red at the link hash. The names are the source's own (symbols.txt), so a swap is a real difference.
- Reason: a layout verdict that is green on a layout the link rejects.
