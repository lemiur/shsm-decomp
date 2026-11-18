# Public decomp references by library

Index of public Wii and GameCube decompilation projects that already cover the third-party libraries this game
links, so a worker can start from existing code. It supports the "Reference first" ruling in `AGENTS.md` and the
"Reference policy" header-comment rule (`// Reference: <project> <path>`). Library ranges are in
`docs/library_map.md`. The vendor-source ruling still applies: nothing from unlicensed vendor source is
listed as usable here.

Checked as of 2026-10-23, from each project's repository tree, `configure.py` and README on GitHub (GitHub's
default branch at that date). Everything below is a pointer, not a verdict: re-read the file you intend to start
from, and compare its SDK build, compiler and flags with ours before trusting its bytes.

## How to read the tables

- **Status** is read from the project's `configure.py`: `M/N` = number of objects marked `Matching` / `NonMatching`
  in the directory named. Object counts, not function counts. An object marked for only some game versions
  (`MatchingFor(...)`) counts as `M`. A project's own progress page is the authority for percentages.
- **Compiler** is the project's `mw_version` for that library. This project builds the SDK with `Wii/1.0`
  (mwcceppc 4.3 build 145, banner `0x4302_145`). Most public Wii projects use `GC/3.0a5`, `GC/3.0a5.2` or `Wii/1.1`
  to `Wii/1.5` for the SDK, so a reference body may need flag or spelling changes before it matches here.
- **SDK build** is the banner string in the project's own source where one was seen. Ours: AI, AX, DVD, SC, THP
  `Feb 27 2009 (0x4302_145)`; KPAD `Jun 22 2009 (0x4302_145)`; HBM `Mar 3 2009 (0x4201_142)`.
- **Licence** is the repository's declared licence (GitHub licence field). Almost all dtk projects declare CC0-1.0
  for their own reconstruction; that says nothing about the underlying vendor code. "none" means no licence file.
- "Unverified" marks anything not confirmed by reading the file itself.

## Projects whose SDK build stamps equal ours

A GitHub code search for the exact banner stamps (`Feb 27 2009 10:01:36`, `10:05:17`, `10:01:59`, `0x4302_145`)
finds the repositories below. They are the best first stop for SDK code, because the library source revision is
the same drop.

| project | game | where the stamp appears | compiler | notes |
|---|---|---|---|---|
| xbret/xenoblade | Xenoblade Chronicles (JP) | `libs/RVL_SDK/src/revolution/ax/AX.c` (`Feb 27 2009 10:01:36`), `sc/scsystem.c` (`10:05:17`), `dvd/dvd.c` (`10:01:59`) | `Wii/1.1` linker; HBM `Wii/1.0a`; Dolphin-era libs `GC/3.0a5.2` | The only Wii project found whose SDK stamps equal ours across several libraries. 29 OS, 10 AX, 8 DVD, 3 SC, 3 ENC objects `Matching`; BTE listed (68 objects) but all `NonMatching`. CC0-1.0. |
| yannicksuter/mscharged-decomp | Mario Strikers Charged | `src/RVL_SDK/ax/AX.c` has two banners: `Feb 27 2009 10:01:36` and `Dec 18 2006`; `sc/scsystem.c` | `GC/3.0a5` | The Korean release `R4QK01` (2010, in progress) carries the 2009 SDK; the USA, EU and JP releases are complete and carry the 2006 SDK. The SDK stamp is per version, so check which `MatchingFor` set an object belongs to. CC0-1.0. |
| Tanline666/veryfat-dungeon | Wii Fit Plus | `src/revolution/SC/scsystem.c` and four more files (banner search hits; unverified per file) | linker `Wii/1.0`; some libs `Wii/1.0a` | Same compiler family as ours. OS 9/6, AX 9/4, DVD 6/6, GX 6/10, MEM 5/2, NAND 2/4; BTE source vendored (see BTE). CC0-1.0. |
| InusualZ/mhtri-decomp | Monster Hunter Tri | `src/AX/AX.c` and 16 more hits (banner search; unverified per file) | mostly `Wii/1.3`; BTE `GC/3.0a5.2`; `Wii/1.0` for a game lib | The `OS` library entry (SDK units including BTE) is 162/62; BTE stack files `Matching` (spot-checked). CC0-1.0. |
| SephDB/pokepark-wii-decomp | PokePark Wii | `src/sdk/RVL_SDK/src/revolution/os/OS.c` (banner search hit) | `Wii/1.1` | Small (66 source files). CC0-1.0. |
| Alice-2010/Decomp | Alice in Wonderland (2010) | `src/Revolution/DSP/dsp.c` (banner search hit) | `Wii/1.0RC1` | Few SDK units (OS, NAND, IPC, FS, GX bump). CC0-1.0. |
| ThePlayerRolo/LegoCloneWarsWii | Lego Star Wars III | `src/ai/ai.c` (banner search hit) | `Wii/1.5` | Later SDK; small. CC0-1.0. |
| DarkRTA/rb3 | Rock Band 3 | `src/sdk/RVL_SDK/src/os/OS.c` (hit); its `ax/AX.c` banner is `Dec 11 2009` | `Wii/1.3` | SDK mostly Dec 2009. No licence file. |
| ThePlayerRolo/sdk_2009-12-11 | Revolution SDK Dec 11 2009 (built from the Wii game SC5PGN) | all SDK units | `Wii/1.0` for SDK, `GC/3.0a5(.2)` for some BTE and Dolphin-era units | Not our SDK drop (Dec 2009; HBM Feb 24 2010) but the same compiler as ours with `-O4,p -ipa file -inline auto`. See the BTE and HBM tables. README states the BTE parts must be licensed Apache-2.0 as derivative works. |

## RVL SDK (os, exi, si, db, dvd, vi, pad, ai, mem, euart, nand, sc, esp, arc, tpl, usb, ipc, fs, ...)

Most projects use the same unit layout (`revolution/<lib>/<File>.c`, or `RVL_SDK/<lib>/`), so a path found in one
maps onto ours. Best matches by compiler and SDK build are listed first.

| project | path | SDK / compiler | status (M/N objects) | notes |
|---|---|---|---|---|
| xbret/xenoblade | `libs/RVL_SDK/src/revolution/` | Feb 2009 stamps; `Wii/1.1` | os 29/5, ax 10/0, dvd 8/0, sc 3/0, ai 1/0, arc 1/0, db 1/0, dsp 3/0, esp 1/0, euart 1/0, fs 1/0, ipc 2/2, mem 4/1, mtx 5/0, pad 1/0, tpl 1/0, usb 1/0, exi 2/1, si 1/1, gx 6/8, nand 0/5, vi 0/3, mix 0/2, wpad 1/4, wud 1/2, kpad 0/1 | Closest SDK build. Also `libs/NdevExi2A`, `libs/PowerPC_EABI_Support` (MSL, runtime, MetroTRK). CC0-1.0. |
| yannicksuter/mscharged-decomp | `src/RVL_SDK/<lib>/` | 2006 and 2009 builds per version; `GC/3.0a5`, SDK flags `-O4,p -inline auto -ipa file -fp_contract off` | all `Matching`: os 33, ax 10, axfx 7, dvd 8, gx 14, nand 5, mem 6, mix 2, ipc 4, exi 3, si 2, vi 3, ai 1, arc 1, db 1, dsp 3, esp 1, euart 1, kpad 1, sc 3, tpl 1, usb 1, wpad 5, wud 3, thp 3, ndev 2 | Widest complete SDK set found (USA, EU, JP complete). Also DWC, GameSpy, NHTTP, NWC24, VF, RFL. CC0-1.0. |
| SMGCommunity/Petari | `src/RVL_SDK/<lib>/` | Super Mario Galaxy (2007) SDK; `GC/3.0a5`, `GC/3.0a3`, `GC/3.0a5.2`, `Wii/1.0` for one lib (unverified which) | all `Matching`: os 34, dvd 8, gx 14, nand 6, mem 4, ipc 4, exi 3, ax 4, axfx 3, dsp 3, sc 3, wpad 5, wud 3, thp 2, kpad 1, tpl 1, usb 1, euart 1, esp 1, arc 1, fs 1 | CC0-1.0. |
| doldecomp/forecast | `src/revolution/<LIB>/` | Wii Forecast Channel (USA v7); `GC/3.0a5.2` linker; HBM banner `May 16 2007` | os 23/8, dsp 3/0, sc 3/0, ax 8/2, gx 6/9, dvd 3/4, nand 0/3, vi 0/3, wpad 5/0, wud 3/0, kpad 1/0 | In progress. CC0-1.0. |
| doldecomp/ogws | `src/revolution/<LIB>/` | Wii Sports (2006 SDK); `GC/3.0a5`, `GC/3.0a5.2`, MSL `GC/3.0a3` | nearly all `Matching`: OS 28/1, AX 10/0, DVD 6/0, EXI 3/0, IPC 4/0, NAND 4/0, SC 2/0, MEM 4/1, GX 8/7, DSP 3/0, PAD, ARC, ESP, EUART, FS, USB, TPL, WENC; `OSFatal.c`, `OSNet.c`, `OSStateTM.c`, `OSPlayRecord.c` present; VI 0/3, SI 1/1, KPAD 0/1, WPAD 1/4 | Also the NWC24 and VF libraries. CC0-1.0. |
| zeldaret/tp | `libs/revolution/src/<lib>/` | Twilight Princess Wii: `-DSDK_SEP2006`, `GC/3.0a3` (patched `GC/3.0a3p1`), linker `GC/3.0a5`. Shield port: `-DSDK_AUG2010`, `Wii/1.0` | os 27/8, ipc 4/0, mem 4/0, ax 2/0, axfx 3/0, arc 1/0, pad 2/0, tpl 1/0, usb 1/0, wud 2/1; dvd 0/8, exi 0/3, gx 0/20, nand 0/6, sc 0/3, si 0/2, vi 0/3, wpad 0/4, kpad 0/1, euart 0/1, esp 0/1, fs 0/1, dsp 0/3 | Largest reference for GC Dolphin libs (`libs/dolphin/src/`: ax, axart, axfx, mix, dsp, gx, os, card, mtx, gd). The Wii `revolution/` side is mostly `NonMatching` objects. This repo's current SDK work already cites it. CC0-1.0. |
| ThePlayerRolo/sdk_2009-12-11 | `source/<lib>/` | SDK Dec 11 2009, `Wii/1.0`, debug and release builds | os 0/39, ax 0/11, axart 0/7, axfx 0/19, dvd 0/8, gx 0/20, nand 0/7, mem 0/6, ipc 0/4, thp 0/3, mix 0/2, syn 0/9, kbd, midi, enc 7/0, cx 4/0, arc 1/0, tpl 1/0 | A decompilation, not a vendor tree: dtk-style `configure.py` with Debug/Release `Matching`/`NonMatching` objects, `orig/` empty (built from the user's SC5PGN copy), names from that game's debug DWARF. Same compiler and `-O4,p -ipa file` flags as ours, different SDK date; units are mostly laid out but unmatched. Useful for unit layout, file names and library coverage. GitHub detects no licence file (README notes Apache-2.0 for the BTE parts). |
| zeldaret/ss | `src/PowerPC_EABI_Support/`; SDK headers `include/rvl/` | Skyward Sword (2011), `Wii/1.5` | SDK `.c` units not in the tree (headers only) | Reference for headers only. CC0-1.0. |
| doldecomp/mkw | `lib/rvl/` (ax 2/13, kpad 0/2, nand 0/2, tpl, bte) | Mario Kart Wii; `Wii/0x4201_127` | mostly `NonMatching` | Uses the earlier 4.2 compiler (0x4201_127) for its SDK. CC0-1.0. |
| doldecomp/dolsdk2001, doldecomp/dolsdk2004, doldecomp/ttyd (`libs/dolsdk2004`) | GameCube Dolphin SDK (not Revolution) | dolsdk2001 (May 2001), dolsdk2004 (Apr 2004) | GC SDK subset per tree scan (os, gx, ax, axfx or axart) | Only useful for libraries shared with the Wii SDK (ax, axfx, gx, os). dolsdk2001 and dolsdk2004 declare no licence; ttyd declares MIT. |

The ax, axfx and mix units are already reconstructed here from zeldaret/tp and ogws (see the unit headers); the
remote-speaker half of `mix.c` has no public source found.

## Bluetooth BTE stack (bta, btm, hid, l2cap, rfcomm, sdp, gap, btu, gki)

The Wii's BTE stack is Broadcom's, later open-sourced as part of Android's Bluedroid. This is the largest third-party
block in the DOL after FMOD (731 functions in `docs/library_map.md`).

| project | path | version / compiler | status | notes |
|---|---|---|---|---|
| doldecomp/ogws | `src/revolution/BTE/` (214 `.c` files, `NOTICE`, `README`, `MODULE_LICENSE_APACHE2`), headers `include/revolution/BTE/` | Broadcom source, 2002-2012 copyright headers, with Revolution changes under `#ifdef REVOLUTION` and a "NOTICE OF CHANGES" header per modified file; compiled with `-DREVOLUTION` | reference source only: configure builds one `bte_unsplit.o`, `NonMatching` | Apache-2.0 licensed source with the Wii changes marked. This is a reconstruction aid rather than a matching unit; it is a copy of the upstream layout (version differences against the Wii-era build are unverified). |
| Swiftshine/kdc, Swiftshine/krtdl, ThePlayerRolo/Super-Mario-All-Star-Decomp, Tanline666/veryfat-dungeon | `src/revolution/BTE/` (same vendored tree, 188 to 222 files) | same as ogws | per project; Wii Fit Plus `Matching` 2 of the listed objects | Copies of the same vendored tree. CC0-1.0 declared (BTE parts are Apache-2.0). |
| ThePlayerRolo/sdk_2009-12-11 | `source/bte/` | Dec 2009 BTE; `Wii/1.0` for config files, `GC/3.0a5` for most stack files | 97 objects `Matching` | The most complete matched BTE found; different SDK drop than ours. README: Bluetooth code must be Apache-2.0, not CC0. |
| SMGCommunity/Petari | `src/RVL_SDK/bte/` | 2007; `GC/3.0a5` | 68 `Matching` | Same stack at an older drop. CC0-1.0 declared. |
| yannicksuter/mscharged-decomp | `src/RVL_SDK/bte/` | `GC/3.0a5`, `-i libs/RVL_SDK/include/private/bte` | 68 `Matching` | BTE flagged in configure as "vendored Broadcom middleware". |
| doldecomp/forecast | `src/revolution/BTE/` | `GC/3.0a5.2` | 67/2 | Matches `uusb_ppc.c` and the stack. |
| InusualZ/mhtri-decomp, TheSoraHD/TheLastStoryDecomp (`libs/smg2/src/RVL_SDK/bte`), Etrainquandt17/Whippy, hotlandsoftware/wii-news-channel | `src/BTE`, `src/RVL_SDK/bte` | `GC/3.0a5.2` (mhtri) | present (66 to 97 files each); status not counted | Unverified. |
| xbret/xenoblade | `libs/RVL_SDK/src/revolution/bte/` | `Wii/1.1` | 68 listed, 0 matching | Layout only. |
| DarkRTA/rb3 | `doc/src-old/libs/bt/` (108 files) | Rock Band 3 | reference only (under `doc/`) | Unverified. |

Starting a BTE unit from one of the matched sets, then diffing against our DOL, is the route; file paths follow the
`bta/<sub>`, `stack/<sub>`, `gki`, `hci`, `main` split.

## Home Button Menu (HBM) and nw4hbm

Our HBM banner is `Mar 3 2009 (0x4201_142)` (CodeWarrior 4.2 build 142, `Wii/1.0a`). xbret/xenoblade's JP
version carries that exact drop (`sdk_hbm_version = 20090303`, built with `Wii/1.0a`): its JP split and symbols
give our unit map (docs/library_map.md, "HBM units"). Our HBM uses the nw4hbm lyt/ut/math subset and the
AX-based `mix`/`syn`/`seq` sound units, not nw4hbm db/snd (those are the 2007 drop in mscharged).
mscharged-decomp's Korean release `R4QK01` also builds this exact drop (banner `Mar  3 2009 14:00:03
(0x4201_142)`; its `VERSION_R4QK01` branches in `HBMCommon.cpp`, `HBMAxSound.cpp`, `HBMRemoteSpk.cpp`).
ThePlayerRolo/sdk_2009-12-11's headers carry the original struct and member names (from the DWARF of a debug
library), so they are the naming reference.

| project | path | version / compiler | status | notes |
|---|---|---|---|---|
| yannicksuter/mscharged-decomp | `src/RVL_SDK/hbm/` (nw4hbm lyt, ut, db, snd; 105 `.c`/`.cpp`), flag `-DHBM_ASSERT` | `GC/3.0a5` | 103 `Matching` | Most complete matched HBM found; Mario Strikers Charged (2007). The `R4QK01` (2010) `HBMCommon`, `HBMAxSound` and `HBMRemoteSpk` are our exact drop (`Mar 3 2009`). |
| ThePlayerRolo/sdk_2009-12-11 | `source/homebuttonLib/` (47 files) | HBM Feb 24 2010; `Wii/1.0` | 47 `Matching` | Same compiler as ours; different drop. |
| Swiftshine/key | `src/hbm/` (49 files) | banner `Dec 10 2009 13:25:50` (`__CWCC__`/`__CWBUILD__`) | per project | Kirby Epic Yarn. CC0-1.0. |
| Etrainquandt17/Whippy | `src/homebuttonLib/` | banner `May 19 2010` / `Dec 10 2009` | per project (39 files) | Unverified. |
| xbret/xenoblade | `libs/RVL_SDK/src/revolution/hbm/` | JP: HBM `20090303` (our drop), `Wii/1.0a`; EU/US: `20100224`, `Wii/1.1` | 14/27 | Closest drop; flags `-ipa file -fp_contract off -sdata 0 -sdata2 0 -RTTI on`. Its unused-function bodies (`IsReverseEndianBinaryFile` returning `false`) are stubs, not source. |
| zeldaret/tp | `libs/revolution/src/homebuttonLib/` (nw4hbm db, lyt, ut, plus HBM*.cpp) | 2006; `GC/3.0a3` | 1/38 | Mostly `NonMatching`. |
| doldecomp/forecast, hotlandsoftware/wii-news-channel | `src/revolution/HBM/` | banner `May 16 2007`, `0x4199_60726` | small (3 to 6 files) | Older drop. |
| doldecomp/ogws | `src/homebuttonMiniLib/` (6 files) | mini HBM | 6/0 | |

## nw4r (lyt, ut, snd, g3d, math, ef, db)

Not part of this game's library map except the HBM subset above; listed because the HBM code reuses its naming and
shapes. Full nw4r: doldecomp/ogws (`src/nw4r/`, nearly all `Matching`, `GC/3.0a5`), zeldaret/ss (`src/nw4r/`,
`Wii/1.5`), xbret/xenoblade (`libs/nw4r/`), SMGCommunity/Petari (`src/nw4r/lyt`, `ut`, `math`, `db`),
doldecomp/forecast, hotlandsoftware/wii-news-channel.

## KPAD, WPAD, WUD, USB, TPL

| project | path | status | notes |
|---|---|---|---|
| mscharged-decomp, Petari, forecast | `RVL_SDK/wpad` 5, `wud` 3, `kpad` 1 | all `Matching` | KPAD at the 2007-era drop; ours is `Jun 22 2009`. |
| doldecomp/ogws | `src/revolution/WUD` 3/0, `WPAD` 1/4, `USB` 1/0, `TPL` 1/0, `KPAD` 0/1 | | |
| ThePlayerRolo/sdk_2009-12-11 | `source/wpad/` 7/1, `source/kpad/` 3/0 | `Wii/1.0` | Closest compiler. |
| xbret/xenoblade | `wpad` 1/4, `wud` 1/2, `kpad` 0/1, `usb` 1/0, `tpl` 1/0 | | |
| zeldaret/tp | `wpad` 0/4, `wud` 2/1, `kpad` 0/1, `usb` 1/0, `tpl` 1/0 | | `KPAD.c` here is already parked per the 2009 stamp. |

## THP

| project | path | compiler | status | notes |
|---|---|---|---|---|
| SMGCommunity/Petari | `src/RVL_SDK/thp/` (2 files) | `GC/3.0a5` | `Matching` | Wii THP. |
| yannicksuter/mscharged-decomp | `src/RVL_SDK/thp/` (3 files, one per-version) | `GC/3.0a5` | `Matching` | |
| doldecomp/sms, projectPiki/pikmin2, doldecomp/mkdd | `libs/THPPlayer`, `src/Dolphin/thp`, `libs/dolphin/thp` | GameCube THP | present | GC THP; the Wii THP here is already reconstructed in `THPDec.c`/`THPAudio.c`. |

## NdevExi2AD

zeldaret/tp (`src/NdevExi2A/`, 0/2), doldecomp/ogws (`src/revolution/NdevExi2AD/`, `exi2.c` matching,
`DebuggerDriver.c` not), SMGCommunity/Petari (`src/NDEV`, 2/0), mscharged-decomp (2/0), forecast (2/0),
xbret/xenoblade (`libs/NdevExi2A`, 2/1).

## MSL C, Metrowerks runtime, MetroTRK

All three are in nearly every dtk project; the unit names above match the ones used here
(`PowerPC_EABI_Support/MSL/MSL_C/MSL_Common`, `Runtime/Src`, `TRK_MINNOW_DOLPHIN`).

| project | paths | compiler | status | notes |
|---|---|---|---|---|
| zeldaret/tp | `libs/PowerPC_EABI_Support/MSL/MSL_C` (MSL_Common, MSL_Common_Embedded/Math), `Runtime/Src`, `libs/TRK_MINNOW_DOLPHIN` | `GC/2.7` (GZ2), `GC/3.0a3` (Wii) | MSL 63/9, runtime 10/0, TRK 25/1 (debugger) and 5/1 (gamedev) | Public reference used by the existing units. |
| xbret/xenoblade | `libs/PowerPC_EABI_Support/src/MSL_C`, `MetroTRK`, `Runtime` | `Wii/1.1` | MSL 71/0, TRK 26/0 | Wii-era MSL on a 4.3 compiler. |
| doldecomp/forecast | `src/MSL_C`, `src/MetroTRK`, `src/Runtime.PPCEABI.H` | `GC/3.0a5.2` | MSL 70/0, TRK 29/0 | |
| SMGCommunity/Petari | `src/MSL_C`, `src/MetroTRK`, `src/Runtime` | `GC/3.0a3`, `GC/3.0a5` | MSL 63/0, TRK 29/0 | |
| yannicksuter/mscharged-decomp | `src/MSL`, `src/MetroTRK`, `src/Runtime` | `GC/2.7` (TRK), `GC/3.0a5(.2)` | MSL 61/0, TRK 29/0 | |
| zeldaret/ss | `src/PowerPC_EABI_Support/` | `Wii/1.5` | `Wii/1.x` MSL and runtime; counts not taken | |
| Tanline666/veryfat-dungeon, ThePlayerRolo/sdk_2009-12-11 | `src/MSL/Math`, `src/MetroTRK/debugger`, `src/runtime` | `Wii/1.0` linker | MSL 5/3 (Wii Fit Plus) | Same compiler as ours. |
| doldecomp/mweppc | decompiled CodeWarrior toolchain | n/a | compiler, not game code | Useful for flag behaviour; not a library source. |
| Others with MSL/TRK/runtime | projectPiki/pikmin2, doldecomp/sms, doldecomp/melee, doldecomp/mkdd, SwareJonge/IEStrikers, zeldaret/oot-gc (`src/metrotrk`, `src/runtime`), Cuyler36/ac-decomp (now ACreTeam/ac-decomp), SeekyCt/spm-decomp (headers only) | GC compilers | present | GameCube builds. |

## zlib 1.2.3

| project | path | version | compiler / flags | status | notes |
|---|---|---|---|---|---|
| Alice-2010/Decomp | `src/Libraries/zlib/` (inffast, inflate, inftrees, zutil and headers) | 1.2.3 (header `ZLIB_VERSION "1.2.3"`) | `Wii/1.0RC1`; `-lang=c99 -func_align 4 -use_lmw_stmw on -opt nopeephole -str reuse,readonly -DNO_MEMCPY` | `inffast.c` and `inftrees.c` `Matching` for the PAL version; `inflate.c` and `zutil.c` listed but not matching (`MatchingFor()` with no versions) | Inflate side only; same version as ours. CC0-1.0. |
| yannicksuter/mscharged-decomp | `src/zlib/` (adler32, crc32, inffast, inflate, inftrees, zutil) | 1.2.2 (header) | `GC/3.0a5`, `-O4,p -inline auto -pool off -use_lmw_stmw on` | 6 `Matching` (USA, EU, JP, KR) | Inflate side only; one patch release older than ours. CC0-1.0. |
| DarkRTA/rb3 | `src/system/zlib/` (full stock tree incl. `deflate.c`, `trees.c`, `gzio.c`) | 1.2.1 | not compiled in `configure.py` | vendored upstream source only | Stock zlib, not a match. |

No public decomp covers zlib's deflate side (`deflate.c`, `trees.c`, `compress.c`) built with CodeWarrior. The stock
zlib 1.2.3 source (zlib licence) is freely available upstream. In our DOL the deflate side lies before libjpeg and the
inflate side after it (`docs/library_map.md`, low confidence for the deflate range).

## IJG libjpeg 6b

No Wii or GameCube decomp project found with libjpeg (searched `jpeg`, `jdapimin`, `jerror`, `jpeg_std_message_table`
in the decomp repositories listed here and via GitHub code search; hits were non-decomp projects). The stock IJG 6b
source is public (IJG licence). hotlandsoftware/wii-news-channel has `src/revolution/TMCC_JPEG/` (a different
decoder, `jpgd_*`, from the Wii News Channel), not libjpeg.

## RenderWare Graphics (3.7) and RW Physics

No project found decompiles RenderWare 3.7 for Wii. GameCube projects built on RenderWare 3.x, all dtk-based:

| project | path | RW version | compiler | status | notes |
|---|---|---|---|---|---|
| bfbbdecomp/bfbb | `include/rwsdk/`, `src/rwsdk/` (only `rtslerp.c`, `ctbsp.c` in tree); configure lists `rwsdk/src` (plcore, pipe/p2), `driver/gcn`, `world`, `plugin/collis`, `hanim`, `matfx`, `ptank`, `skin2`, `userdata`, `tool/anim`, `intsec`, `slerp` | RW 3.x GC (minor version not checked) | `GC/1.3.2`, `cflags_renderware` (`-lang=c`, `-fp_contract off`, `-char signed`, `-str reuse`, `-common off`, `-O4,p`) | 120 objects, all `NonMatching`; source mostly absent | Gives GC RenderWare file names and unit boundaries. Headers `include/rwsdk/*.h` are reconstructed SDK headers (compare with the public RW 3.7 headers before use). CC0-1.0. |
| bfbbdecomp/tssm, seilc/incredibles, seilc/rotu, seilc/rat | same `rwsdk` layout | RW 3.x GC | `GC/1.3.2` | 123 to 139 RW objects each, all `NonMatching` | Same split layout. CC0-1.0. |
| tgsm/pbg | `src/rwsdk/src`, `driver`, `tool` (incl. bundled libpng 1.0.12), `src/rwa/` (RenderWare Audio) | RW 3.x GC | `GC/1.3.2` | 41 RW objects `Matching`, 205 `NonMatching` | Piglet's Big Game. Largest matched RW set found; RW Audio (`rwa`) is not used by this game's library map. CC0-1.0. |
| ShulkMaster/mk-deception | RW objects (9 `Matching`/47) | RW 3.x GC | `GC/1.3.2` | Unverified paths. | |
| LivewireCB/RW-3.5_Decomp | `src/rwsdk/` (stub sources), `ratsgc_d_dwarf/` (listing derived from a debug build's DWARF) | RW 3.5 GC | `GC/1.3.2`, `GC/2.6` | 6 `Matching`, 114 `NonMatching`; source files are empty stubs | Provenance of the DWARF listing unverified; treat as a boundary and naming aid at most. |
| origami-ltd/gamecube-reVC | `src/fakerw/` | n/a | n/a | reimplementation of the RW API for a port | Not a CodeWarrior match. |

RW 3.7 is newer than the GC projects' RW; the retail Wii build has a different driver (`rwsdk/driver/gcn`, Wii
variant) and the PS2/PC-only plugins are absent. Use these for boundaries and the stable core (babinary, baframe,
bacamera, bagetopt etc.) only, with our own `rwID_*` and symbol evidence deciding.

Not matching decomps: `aap/librw` (MIT re-implementation of the RW API, useful only to understand behaviour) and `electronicarts/RenderWare3Docs` (documentation).

No public decomp, source or symbol list for RW Physics (`Rwp*`) was found.

## FMOD Ex 4.x and the event system

No Wii or GameCube decomp project contains FMOD (the `fmod` hits in the scanned projects are the MSL math file
`e_fmod.c`). FMOD source trees exist on GitHub (for example inside leaked or modified game trees) without a licence
to redistribute; they are not listed and fall under the vendor-source ruling. Reconstruction rests on the FMOD Ex
public headers and the `fmod_*.cpp` strings in the DOL.

One FMOD unit has public source: the allocator FMOD embeds is Doug Lea's malloc, release 2.8.3 (`malloc-2.8.3.c`,
public domain, from the author's distribution at gee.cs.oswego.edu). `src/fmod/dlmalloc.cpp` is that text with FMOD's
configuration and edits, listed in its header comment.

## RenderEclipse-Tools (Climax file formats)

`github.com/IWILLCRAFT-M0d/RenderEclipse-Tools` is a community repository that reverse-engineers the data files of
Climax's Silent Hill games (Origins, both studio versions, and Shattered Memories; PS2, PSP and Wii files). It holds
an archive manager for `*.ARC`, a splitter for RenderWare stream and `*.snd` files, a reader for the obfuscated
`*.XML`/`*.XAML` files, a filename hash-table tool, a QuickBMS script, a Noesis texture plugin and two documents:
`FileFormats.md` and `MiscResearch.md`. It states no licence. No text or code of it is used here.

It is trusted for file formats, not for code names (`AGENTS.md`, reference source 5). `FileFormats.md` documents
magic values, field orders and two name hashes (the string-table hash and the `.ARC` file-name hash). It does **not**
document RenderWare or Climax chunk IDs, and it names no function, class or symbol of the executable.
`MiscResearch.md` gives, per platform and region, Gecko cheat addresses for the noise filter, the cutscene black
borders and the cutscene tables. Each claim that touches the Wii executable was read against `R5WEA4` (USA, the NTSC
cheats); the result is below, and `ref/shsm_formats/README.md` restates the confirmed formats with the code that
implements them. Sibling names (`build/tools/sibling.exe`, Gormiti) are given where a function pairs.

| claim | DOL evidence | status |
|---|---|---|
| String table: magic `0x02000000`, then `stringsNumber`, then two words per string (hashed name, position), then UTF-16 text | `unk_StringTable::fn_801D1CFC` (0x801D1CFC) byte-swaps word 0 and the count, takes the entries at +8, puts the text base at entries plus count times 8, and sets each entry's text to the base plus its second word counted in 16-bit units; it swaps each shared text once. The loader swaps word 0 and never compares it | partly (layout confirmed; the magic value is not tested anywhere found) |
| String-table name hash: seed 5381, `hash = hash * 0x21 + (c & 0xDF)` | `fn_801D2554` (0x801D2554, `climax/Localisation`) hashes a name to its NUL, then scans the 8-byte entries of every table in `lbl_804908B8` for an equal first word and returns the second (or NULL, or "NO STRING"); `fn_801D2630` is the same scan from a ready hash. The same loop is open-coded at 0x800C6960 (unit 0x800C6508: hashes a formatted name and asks `fn_801D2630` whether it exists) and at 0x800D8098 and 0x800D8144 (unit 0x800D7C00: hash the text of an XML attribute). The document's example `igc02_doctork_01a` has decimal 484287472 = `0x1CDDA3F0` by this formula; the hex printed in the document (`0xf0a3dd1c`) is that value's bytes in file order | confirmed |
| `.ARC` magic `0x10FA0000`, header magic / `fileCount` / `startDataPos` / unknown | `fn_801B2A68` (0x801B2A68) copies the first 0x10 bytes of the archive, swaps them and compares the first word with `0xFA10` (0x801B2AF0 and 0x801B2B34; the file's bytes `10 FA 00 00`, little-endian); the count is at +8 and the entry-table pointer is kept at +0x14 of the archive object. The constant `0x10FA0000` itself occurs only as data | confirmed (the document's value is the bytes in file order) |
| `.ARC` file-name hash: lowercase the name, seed 0, `hash = hash * 33 ^ c` | `FindEntry` (0x801B2C7C; Gormiti `RWS::FileSystem::CArchive::FindEntry`) takes the part of the path after the last `/` or `\`, adds 0x20 to `A`-`Z`, hashes with `slwi 5` plus `add` then `xor`, and scans 16-byte entries for an equal first word. The document's `fontjap` example gives `0xF63BD6A8` by this formula | confirmed (the code hashes the name without its directories, which the document does not say) |
| `.ARC` entries: hashed name, `fileDataPos`, `fileSize`, `fileSizeUncompressed` (16 bytes) | the stride of `FindEntry` is 16 bytes; the three embedded tables below read as hash, position, size, uncompressed size; `fn_801B3F58` (Gormiti `GetArchiveFileInfo`) and `fn_801B365C` (Gormiti `Size`) call the lookup | confirmed |
| `.ARC` file table embedded in the executable | three images in `.data`, each behind a 0x38-byte descriptor (name at +0x18, little-endian size at +0x14): `Boot.arc` at 0x803FD240 (size 0x5200, header at 0x803FD278, 6 entries; the whole archive is embedded), `data.arc` at 0x8041D240 (size 0x7CC0, header at 0x8041D278, 1995 entries; header and table only), `igc.arc` at 0x80424F38 (size 0x1220, header at 0x80424F70, 289 entries; header and table only). `fn_8018EE98` (0x8018EE98) finds a block by case-insensitive name; `fn_8018FC3C` (0x8018FC3C) registers the blocks with the `dvd:` file system | confirmed |
| XML header `unknown0`, `startStringsPos`, `"FBXF"`, `fileSize` | `fn_801CDEA8` (0x801CDEA8, Gormiti `FbXML::RootElement::Create`) tests bytes 8 to 10 for `F`, `B`, `X` and byte 11 for `F` or `M`, then calls `FbXML::RootElement::RelocateForMem` (0x801CE334), which swaps and relocates the blob: word 1 becomes a pointer (the string start), word 3 is swapped, and byte 11 is then set to `M`. A buffer without the signature is parsed as text XML by the same function. The test is bytewise (no 32-bit constant), in the Climax XML block | confirmed (the `M` form, the in-memory result, is not in the document) |
| Attribute markers `0x06800100`, `0x1E800100`, `0x0E800100`, `0x1E000000`, `0x0E000000` | no marker constant is in the code. `RelocateForMem` decodes each record by two bits of its swapped first word (`(w >> 1) & 3`): 0 is a 0x14-byte record, 1 and 2 are 8 bytes, 3 is 12 bytes with a relocated third word. All five markers, read little-endian, give 3, which fits "marker, name position, value position" | partly (consistent; not read marker by marker) |
| Noise filter `00528842 00000000` (8-bit write at 0x80528842) | 0x80528842 is `lbl_80528842`, a one-byte flag in `.sdata`. `fn_801E7A10` (0x801E7A10) is its setter and `fn_801E74A8` (0x801E74A8) its only reader: with the flag set it clamps the strength `lbl_80528848` to 1 to 16 and calls `fn_801E7C44` (not examined) | confirmed (the flag exists, with one writer and one reader; that it is the noise filter rests on the document) |
| No black borders `0452c3bc`, `0452c3c0`, `04528842` (32-bit writes) | 0x8052C3BC and 0x8052C3C0 are the `.sdata2` constants 1.0f and -1.0f, read by `fn_800A86A8` (0x800A86A8), `fn_800A88B4` (0x800A88B4) and `fn_800A89E8` (0x800A89E8) (`game/G3`; the strings "CIGCBorderControl ON/OFF" are used in `fn_800A88B4`). `fn_800A86A8` stores one of them as the border's direction in a controller field and notifies an `unk_InboxOwner`; zeroing both keeps the field from becoming 1 or -1. The third code is a 32-bit write at 0x80528842: it clears the flag above, the byte `lbl_80528843` and two bytes of the float `lbl_80528844` | confirmed for the two constants (shared float data, not code as the document says); the third line's wider reach is not in the document |
| Intro and ending cutscene tables `043E1BC4` to `043E1BE0` | 0x803E1BC4 and 0x803E1BD4 are the literals "IT_CrashIntro" and "ITCI_LCT1" inside the string pool `lbl_803E1B38` (0x148 bytes, `.data`); the codes overwrite their text with `igc0a` / `igc0a_igc0b` and `igc49a` / `igc49d_igc49a`. The ending code's last word at 0x803E1BE0 lies in the next literal of the pool. No relocation in the listings points at either literal, so the code that reads them is not identified | partly (bytes confirmed; the reader is not found) |

Six functions load the seed `0x1505` (5381) and they are not one routine: `GetStringHash` (0x801B6020, Gormiti
`SHUtils::GetStringHash`) hashes up to a length or a NUL without case folding; `GetStringHashForceLower` (0x801B605C, Gormiti
`GetStringHashForceLower`) is the same with `& 0xDF`; `fn_801D2554` is the `& 0xDF` hash to a NUL followed by the
string-table lookup; `fn_800D838C` (0x800D838C) hashes a name with `& 0xDF` and searches a game object's array of
12-byte records by first word, falling back to `fn_801D2630` when it is absent; `fn_801B63CC` (0x801B63CC) hashes
without folding and uses the low byte as the bucket of a 256-entry table of interned strings (`lbl_8048E088`), then
compares text; `fn_801B651C` (0x801B651C) finds or appends a name in a linked list of name, hash and next, folding the
hash and the compare unless the list's flag is set. Another copy of the unfolded hash sits in the function around
0x801B6270 (the interning side of `fn_801B63CC`).

The `& 0xDF` is applied to a signed byte (`rlwinm` then `extsb`), so a byte of 0x80 or more is sign-extended; the
document's `ord(c) & 0xDF` agrees for ASCII. Nothing in the binary contradicts the repository's documents; the
differences are the byte order of printed hex values (the documents show stored bytes) and the omissions noted above.

## No public decomp found

- FMOD Ex 4.x (core, DSP units, event system).
- RenderWare 3.7 Wii driver and plugins; RenderWare Physics (`Rwp*`).
- IJG libjpeg 6b built with CodeWarrior.
- zlib 1.2.3 deflate side (`deflate.c`, `trees.c`, `compress.c`); the inflate side has Alice-2010 (1.2.3) and
  mscharged-decomp (1.2.2).
- The Climax engine and game code (no public project; Ghost Rider PS2 class-name literals only, see AGENTS.md).
  The PS2 release keeps debug attribute names that the Wii build compiled out (`ref/shsm_ps2/debug_strings.txt`,
  19 strings, none in the Wii DOL). No PS2 instruction or data word references them: the attribute handlers
  dispatch on numeric ids, and the names are unreferenced pool literals. `ref/shsm_ps2/debug_strings.md`
  therefore attributes each string to a handler or unit from the referenced strings beside it in the string pool
  and pairs that with its Wii function, with a confidence word per string. A PS2 attribute handler has extra
  cases for these attributes that its Wii twin lacks.
- Remote-speaker half of the AX `mix.c`.

## Not matching decomps

- libogc / devkitPro: an independent open-source library, not a recompilation of Nintendo's SDK; its function
  boundaries and struct layouts differ. Useful for hardware behaviour only.
- doldecomp/brawl (`include/lib/BrawlHeaders/OpenRVL`, `nw4r`): headers only for the SDK side.
- decomp.dev: its project index is disabled; use the GitHub repositories above.

## Other Wii and GameCube projects seen (SDK content not examined)

doldecomp/mkdd, doldecomp/melee, doldecomp/sms, projectPiki/pikmin, projectPiki/pikmin2, zeldaret/oot-gc,
ACreTeam/ac-decomp, SeekyCt/spm-decomp, NSMBW-Community/NSMBW-Decomp, mkwcat/nsmbw-project, yannicksuter/smstrikers-decomp,
zcanann/SFA-Decomp, TeamOrre/xd-decomp, roeming/SA2B, ThePlayerRolo/lm-decomp, Jovinull/sonicheroes,
SwareJonge/IEStrikers, KooShnoo/pokemon-rumble, yomcube/cmwp-decomp, conhlee/rhf (Rhythm Heaven Fever; DSP banner
`Jul 2 2010`), pbr-ret/pbr-decomp. They carry GameCube-era or later Wii SDK drops and the same shared MSL, runtime
and MetroTRK files.
