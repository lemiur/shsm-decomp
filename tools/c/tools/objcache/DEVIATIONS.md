# objcache: accepted differences from tools/objcache.py

Proven equal to the retired Python tools/objcache.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-objcache-1. Own entry format, under its own namespace of the same cache directory
- Behaviour: the C wrapper never reads an entry the Python wrapper wrote, and the Python never reads a C entry. Both
  live in `<SHSM_CACHE_DIR or %LOCALAPPDATA%\shsm-cache>\objcache` in the shared stamp format of
  tools/common/cache.py (`<key[:2]>\<key>`, payload plus the 48-byte `SHSMCACHE1` stamp), but the C keys are taken
  over the format word `objcache-c1` (the Python's `objcache-1`), so no key is shared, and the C payloads start with
  their own magics (`OCRES1-c1\n`, `OCMAN1-c1\n`) and hold length-prefixed binary records (core `cache.h` writer)
  where the Python's hold marshal data. A switch from one wrapper to the other is a cold cache. `prune` and `clear`
  act on every entry of the directory, C and Python alike (same stamp format, one size budget), as before.
  `stats` counts a result or manifest by its magic prefix (`OCRES1-` / `OCMAN1-`): the C tool counts the Python's
  entries of every Python version and its own, the Python counts only its own version's (a C entry is "other" to
  it, but its bytes are in the size). `verify` checks the C result entries only (the Python's likewise skips C
  entries). The C hash memo is its own file (`memo\<key>` with the key over `objcache-c1`) in its own format
  (`OCMEMO1-c1\n`, a 64-bit check of the payload, then the records; a damaged memo is ignored, as a damaged marshal
  memo is). The stats logs are shared: same files, same one-letter events, the same lock on the first byte.
- Reason: the owner's brief (the C tool need not read marshal data). The soundness rules are unchanged: the C key
  covers the same inputs (compiler and the DLLs beside it, each wrapper in front of it, the arguments with the `-o`
  value replaced, the MW* environment, the source path and bytes; each dependency set holds every header's sha256
  and the include probes), proven by the ported tests and by the clean-build comparison in the commit message.

## D-objcache-2. prehash reports the C memo
- Behaviour: `prehash` fills the C memo (D-objcache-1), so its counts are those of the C memo (a tree hashed by the
  Python prehash is hashed again by the C one), and `--json`'s `memo` names the C memo's path.
- Reason: follows from D-objcache-1; the line format and the other keys are the Python's (harness `prehash.txt`, with
  the memo path normalised).

## D-objcache-3. Debug lines
- Behaviour: with `SHSM_OBJCACHE_DEBUG=1` a hit also writes `objcache: hit (<n> files hashed, <t> ms)` on stderr (the
  number of files the process read and hashed, 0 to the compiler's own count when the memo holds, and the wrapper's
  wall time from its start to the restored output, in milliseconds with one decimal), and the reasons printed
  after `objcache: uncached:` / `not stored:` / `lookup failed:` are the C tool's wording (a Python exception text such
  as `list index out of range` for `-c` as the last argument is kept where one exists).
- Reason: debug output only (the variable is unset in every build); the extra line is how a hit's cost is shown.

## D-objcache-4. verify --sample draws another sample
- Behaviour: `verify --sample N` with fewer than N entries checks all of them, as the Python; with more it checks N
  entries chosen by the C tool's own generator, not `random.sample`'s.
- Reason: a random sample has no reproducible order to match; the harness compares `--all` and the empty cache.

## D-objcache-5. Stored compiler output markers
- Behaviour: the `-o` placeholder and the working-directory marker inside a C entry are `\x01OUT\x01` and
  `\x01CWD\x01` (the Python's use `\x00`); the replay is the same (the marker is replaced by the current working
  directory).
- Reason: internal to the C entry format (D-objcache-1); no output shows them.
