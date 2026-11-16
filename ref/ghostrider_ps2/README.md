# Ghost Rider (Europe, PS2) reference extraction

## Provenance

- Disc image: `<your Ghost Rider dump>.iso` (the European PS2 release, 2,643,460,096 bytes, ISO9660 PS2 DVD), kept in the project root (ignored by `.gitignore`).
- `SYSTEM.CNF`: `BOOT2 = cdrom0:\SLES_543.17;1`, `VER = 1.00`, `VMODE = PAL`.
- `SLES_543.17`: boot ELF, 4,937,236 bytes, sha1 `ca2f5fa8b1b7214d67c4e87343cc36474318b94a`.
  ELF32 little-endian MIPS (EE), entry 0x0025b858, 26 sections.

## IMPORTANT finding

This ELF is STRIPPED. It has no `.symtab`, `.strtab`, `.mdebug`, `.debug_*`, `.line` or `.stab`
sections and no RTTI/mangled names. It is NOT the unstripped ~14k-symbol build that was expected.
Only a few hundred identifier-like string literals survive (see `classes.txt`, `source_files.txt`).
Sections present: `.text .vutext .reginfo .DVP.* (overlay stubs) .data .vudata .rodata
.gcc_except_table .lit4 .sdata .sbss .bss .vubss .shstrtab` (see `sections.txt`).

## Tools (`build/tools/iso9660_extract.exe`, `build/tools/elf_symbols.exe`; C ports of the retired `tools/scripts/*.py`)

- `iso9660_extract.exe` - ISO9660 lister/extractor.
- `elf_symbols.exe` - ELF32 LE section/symbol dumper, GNU v2 demangler (self-tested at startup; Itanium not implemented),
  and a string-mining fallback used when `.symtab` is missing.

## Regenerate

```
cd <repository root>
build/tools/iso9660_extract.exe "<your Ghost Rider dump>.iso" --list
build/tools/iso9660_extract.exe "<your Ghost Rider dump>.iso" --boot-elf ref/ghostrider_ps2
build/tools/elf_symbols.exe ref/ghostrider_ps2/SLES_543.17 ref/ghostrider_ps2
```

## Files

- `SYSTEM.CNF`, `SLES_543.17` - extracted from the disc.
- `sections.txt` - section table and a debug-info presence report (none).
- `symbols.txt` - `address size type bind section raw_name demangled_name`; contains only header comments (0 symbols).
- `source_files.txt` - no STT_FILE entries exist; lists source paths found as string literals
  (RenderWare SDK `c:/daily/rwsdk/plugin/pds/sky2/...` and RenderWare Physics `//Physics/Rwp37Active/src/...`, 23 entries; none are Climax game files).
- `classes.txt` - heuristic. `Namespace::member` strings (e.g. `Multiplayer::Msg::msg*`, `OF_Patrol::CState*`)
  and a list of class-name-like literals (`CBikeCamera`, `CEnemyBehaviour`, ...).
  Method counts are only meaningful if a symbol table is ever supplied.

## Cross-check against the SHSM Wii DOL

27 of the 119 class-name literals in `classes.txt` also occur verbatim as strings in `orig/R5WEA4/sys/main.dol`
(e.g. CBaseCamera, CCameraDirector, CSceneObject, CPhysicsObject, CSequenceObject, CSimpleAnimation, CBaseSpawner,
CGenericSpawner, CMessageRelay, CCounter, CZoneCollision, CSkyDome, CBaseLight, CColorLight, CBloomCfg,
CMotionBlurCfg, CHitEffectManager, CStaticInit, CSystemCommands). This is a name-level hint only (no addresses).
