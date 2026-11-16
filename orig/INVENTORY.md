# Inventory: Silent Hill: Shattered Memories (Wii, USA)

## Disc
- Game ID `R5WEA4`, region USA (NTSC-U, 'E'), disc number 0, revision/version 0. Title "Silent Hill: Shattered Memories".
- Source: your own dump of the USA disc, `<your dump>.rvz` or `<your dump>.iso` (the reference RVZ was zstd level 19, chunk 128 KiB; the ISO is 4,699,979,776 bytes).
- Partitions: type 1 (UPDATE) at 0x50000, type 0 (DATA) at 0xF800000. Only DATA was extracted.
- First extracted with a stop-gap RVZ reader (retired), to `orig/R5WEA4/{sys,files}` (dtk-style layout). `dtk disc extract <rvz> orig/R5WEA4`
  is the replacement: it verifies against Redump (`dtk disc verify`: CRC32 23e2a1ff, SHA-1 f8ad64c9737a5bdd16758e5ffdb3f5b356f9b701).
  `sys/*` from the stop-gap are byte-identical to dtk's; six `files/*` (MOVIES/CREDITS, IGC49b, NOISE, OUTRO, data.arc, igc.arc)
  differ from dtk's output of the same sizes, so re-extract with dtk if those files are needed.
- A PS2 disc image (`<your PS2 dump>.iso`) is not this game's Wii disc: it is the PlayStation 2 release (`SLUS_218.99`, MIPS ELF, ISO9660 with DATA.ARC/IGC.ARC/MOVIES/MODULES).
  In the author's copy its first sectors are overwritten with 0x5D filler. Extracted to `orig/_iso/` (scratch, unused). PS2 ELF is stripped (no .symtab) but has `z:/SH1R/Code/game/libs/FMOD/src/*.cpp` path strings (list: `orig/_iso/ps2_source_paths.txt`).

## sys/
| file | size |
|---|---|
| boot.bin | 1,088 (0x440) |
| bi2.bin | 8,192 |
| apploader.bin | 241,412 |
| fst.bin | 476 |
| main.dol | 4,565,440 (0x45A9C0), sha1 6a3964eaf1ebe8225283363e4161a793a03683d3 |

No `.rel`, `.elf`, `.map`, `.sel`, `.str`, `.sym`, `.dbg` files on disc (FST has only 24 entries). Only one DOL.

## Filesystem layout (files/, total 3,726,348,008 bytes)
- `data.arc` 1,500,508,160 (custom archive: LE header magic 0xFA10, 1995 entries, align 0x8000, entries = hash,offset,csize,usize; no names, compressed)
- `igc.arc` 1,376,569,344 (same format, 289 entries)
- `opening.bnr` 339,112
- `MOVIES/` (20 THP videos, ~0.85 GB): CREDITS, HM1PLY, HM1RWD, HM2PLY, HM2RWD, HM3PLY, HM3RWD, IGC49a, IGC49b, IGC49c0, IGC49c1, IGC49c2, IGC49d, INTRO, LOOP, MONTAGE, NOISE, OUTRO, PUBDEV, UFO

## main.dol (equivalent of `dtk dol info`)
```
text0 off=000100 addr=80004000 size=0x26e0   (init)
text1 off=0027e0 addr=800067a0 size=0x3d1bc0 (end 803d8360)
data0 addr=800066e0 size=0x60 | data1 addr=80006740 size=0x60 | data2 addr=803d8360 size=0x400
data3 addr=803d8760 size=0x20 | data4 addr=803d8780 size=0x6700 | data5 addr=803dee80 size=0x785c0
data6 addr=80526720 size=0x29e0 | data7 addr=8052b5e0 size=0x4aa0
bss   addr=80457480 size=0xd8c34 (end 805300b4)   entry=0x80006310
```
(Note: data6/data7 sit above bss, i.e. sdata/sdata2; the usual dtk section naming applies.)

## Toolchain hints
- SDK strings: `<< RVL_SDK - {THP,EXI,SI,OS,DVD,VI,AI,AX,DSP,GX,NAND,SC,KPAD,WPAD,HBM} >>`, build tag `release build: Feb 27 2009 ... (0x4302_145)` => RVL SDK 4.3.2 build 145 (CW Wii 4.3 build 145 == `compilers/Wii/1.0`), `GCN_Mem_Alloc.c`, MetroTRK ("Metrowerks Target Resident Kernel for PowerPC" at 0x100), "Metrowerks CW runtime library initializing default heap". No MSL/"CodeWarrior"/"mwcc" strings.
- Middleware: FMOD Ex + Event System (`fmod_*.cpp`, 81 unique source-file name strings in the DOL, no version number string), RenderWare (`rwID_CLUMP`, `rwID_WORLD`, ...), IJG libjpeg, Climax engine names (`ClimaxT1Entry`, "Fuse Binary Format", `rwID_FUSESTATE`, NavMesh), `.xaml` UI names. No NW4R, Havok, Bink, CRI.
- No symbol map, no debug ELF, no linker map in DOL or ARCs (name-pattern scan of data.arc/igc.arc/DOL/apploader found only random binary hits).

## Tools
- The project's own tools are built with `tools/build_c.cmd` into `build/tools/`; dtk, objdiff, the binutils and the compilers are fetched or extracted as `docs/toolchain.md` describes (`tools/bin/`, `tools/compilers/` and `build/` are not committed).
