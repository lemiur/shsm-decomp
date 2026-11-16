# Climax data formats confirmed against the Wii executable

Advisory reading aid for the Climax archive, string-table and XML code (`R5WEA4`, USA). The formats come from the
community repository RenderEclipse-Tools (`docs/references.md`, "RenderEclipse-Tools"); every fact below was read
in the retail code. Addresses are functions or data of the DOL under their current `symbols.txt` names, which are
placeholders: the repository names no function. Never gate input.

## String table (`Strings`, `BootStrings`)

- Layout: word 0 (the repository says magic `0x02000000`; the loader swaps it and never compares it), word 1 the
  entry count, then `count` entries of two words, then UTF-16 text. Words are stored little-endian and swapped in
  place on load.
- An entry is an identifier and an offset. The offset is counted in 16-bit units from the end of the entry array;
  the loader turns it into the text's address. Entries that share an offset share a text, swapped once.
- Loader: `unk_StringTable::fn_801D1CFC` (0x801D1CFC, source in `src/climax/Localisation.cpp`). Lookups:
  `fn_801D2554` (0x801D2554, from a name) and `fn_801D2630` (0x801D2630, from an identifier): a linear scan of
  the entries of every loaded table (`lbl_804908B8`).
- Identifier: hash of the name, seed 5381, `hash = hash * 33 + (c & 0xDF)` over the bytes up to the NUL, 32 bits.
  The bytes are signed after the mask. Callers also hash names inline (0x800C6960, 0x800D8098, 0x800D8144).
  Other hashes on the same seed: `fn_801B6020` (no mask, bounded length), `fn_801B605C` (mask, bounded length),
  both in the string-cache code (`climax/stream` region), and the interning and name-list routines
  `fn_801B63CC` and `fn_801B651C`.

## `.ARC` archive

- Header, four little-endian words: `10 FA 00 00` (compared as `0xFA10` after the swap), file count (+8 of the
  archive object), start of the data, an unknown word. The 16-byte entries follow: file-name hash, position, size,
  uncompressed size (0 when not compressed, per the repository; compression not examined here).
- Archive open: `fn_801B2A68` (0x801B2A68) checks the magic and keeps the entry-table pointer at +0x14.
  Lookup: `fn_801B2C7C` (0x801B2C7C) hashes the part of the name after the last `/` or `\`, folding `A`-`Z` by
  adding 0x20, with seed 0 and `hash = hash * 33 ^ c`, and scans the entries for the hash. Users: `fn_801B32C8`,
  `fn_801B3538`, `fn_801B365C` (size) and `fn_801B3F58` (position and sizes).
- The executable embeds the tables of three archives in `.data`, each behind a 0x38-byte descriptor (name at
  +0x18, byte count at +0x14, little-endian; the next descriptor follows the data): `Boot.arc` (0x803FD240, the
  whole 0x5200-byte file, 6 entries), `data.arc` (0x8041D240, header and 1995 entries), `igc.arc` (0x80424F38,
  header and 289 entries). `fn_8018EE98` (0x8018EE98) finds a descriptor by name; `fn_8018FC3C` (0x8018FC3C)
  registers them as the `dvd:` file system.

## `.XML` / `.XAML` user-interface files

- Header: a word, the string-start offset, the bytes `FBXF`, the file size; records follow at +0x14. The
  executable accepts `FBXF` (a file still in file byte order) and `FBXM` (already relocated in memory); any other
  buffer is read as text XML.
- `fn_801CDEA8` (0x801CDEA8) tests the four bytes one at a time (bytes 8 to 11) and calls
  `FbXML::RootElement::RelocateForMem` (0x801CE334), which swaps every word, turns offsets into pointers and
  writes `M` into byte 11. The record kind is bits 1 and 2 of a record's swapped first word: 0 is 0x14 bytes, 1 and
  2 are 8 bytes, 3 is 12 bytes (marker, name, value). The repository's five marker constants all decode to kind 3.
- Game code hashes the text of an XML attribute with the string-table hash (0x800D8098, 0x800D8144); what the
  result is looked up in was not examined.

## Cheat addresses (Wii, NTSC) and what they are

- 0x80528842: one-byte `.sdata` flag, written by `fn_801E7A10`, read by `fn_801E74A8` (post-process pass,
  strength `lbl_80528848` clamped to 1 to 16). The repository calls it the noise filter.
- 0x8052C3BC and 0x8052C3C0: the shared float constants 1.0f and -1.0f. `fn_800A86A8`, `fn_800A88B4` and
  `fn_800A89E8` (the `CIGCBorderControl` handlers) load them as the border direction.
- 0x803E1BC4 and 0x803E1BD4: the literals "IT_CrashIntro" and "ITCI_LCT1" in the pool `lbl_803E1B38`. No code
  reference to either was found by relocation.
