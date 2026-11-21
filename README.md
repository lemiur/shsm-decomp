# Silent Hill: Shattered Memories (Wii) decompilation

A work-in-progress matching decompilation of Silent Hill: Shattered Memories for the Wii (USA, game ID `R5WEA4`).
It is built with [dtk-template](https://github.com/encounter/dtk-template) and
[decomp-toolkit](https://github.com/encounter/decomp-toolkit), and compiles with the CodeWarrior C/C++ compiler the
game was built with.

The goal is source code that the original compiler, with the original flags, turns into the same object code as the
retail binary, function by function. "Matching" is checked against the whole program: the repository links the units
that have been reconstructed together with the original bytes of everything else, and the result must hash to the
retail `main.dol`. The source is meant to be readable, not only byte-identical: classes, types, ownership and control
flow are reconstructed as a programmer on the original team would have written them (see
[Verification](#how-work-is-verified)).

**This repository contains no game assets, no original code and no original binaries.** It holds reconstructed
source, headers, configuration and tooling. To build it you need your own legally obtained copy of the game.

The input is the game's executable:

| file | game | SHA-1 |
|---|---|---|
| `main.dol` (`sys/main.dol` on the disc) | Silent Hill: Shattered Memories, Wii, USA (`R5WEA4`) | `6a3964eaf1ebe8225283363e4161a793a03683d3` |

The hash is recorded in `config/R5WEA4/build.sha1` and in `config/R5WEA4/config.yml`; the build refuses any other
file.

## Status

As of 2026-11-20 (the live figures are in [`docs/progress.md`](docs/progress.md)):

| measure | functions | code bytes |
|---|---|---|
| matched under the rules (headline) | 5,294 of 13,922 (38.0%) | 1,301,500 of 3,974,484 (32.7%) |
| linked from source (byte-identical, excluding hand-written assembly) | 5,299 of 13,922 | 1,303,520 of 3,974,484 |
| hand-written assembly in the original, listed apart | 116 | 28,536 |

592 of 1,381 units are `Matching`, that is, linked from source into the gated build. Every other unit is linked
from the original bytes (see [Layout](#repository-layout) and `AGENTS.md`, "Parking mechanism").

Work so far, by library (functions matched, from `build/tools/status.exe` on 2026-11-20; the address ranges that
assign functions to libraries are approximate, see `docs/library_map.md`):

| library | what it is | functions matched |
|---|---|---|
| RVL SDK | Nintendo Revolution SDK, including the Bluetooth stack, Home Button Menu and nw4hbm | 2,539 of 2,701 (94.0%) |
| MSL C, Metrowerks runtime, MetroTRK | C library, compiler runtime, debugger kernel | 147 of 147; 10 of 27; 99 of 143 |
| zlib 1.2.3 | inflate side | 14 of 14 |
| IJG libjpeg 6b | JPEG decoder | 162 of 176 (92.0%) |
| RenderWare Graphics 3.7 | rendering middleware | 896 of 1,286 (69.7%) |
| RenderWare Physics | physics middleware | 40 of 525 (7.6%) |
| FMOD Ex 4.x | audio middleware, including its embedded dlmalloc | 645 of 1,879 (34.3%) |
| Climax engine | the developer's engine layer | 320 of 1,278 (25.0%) |
| Game code | Silent Hill: Shattered Memories itself | 421 of 5,842 (7.2%) |

## Building

Windows x64 is the supported host (the tooling is Windows-native; WSL is not needed). You supply the game
executable and the compilers; everything else is built or downloaded.

Prerequisites:

- Visual Studio 2022 (MSVC, x64) and CMake 4.x, for the project's own tools. They are written in C17 and built
  with `tools\build_c.cmd`. Python is not required.
- `orig/R5WEA4/sys/main.dol`: the executable from your copy of the game (SHA-1 above). The disc's data partition
  can be extracted into `orig/R5WEA4` with `dtk disc extract`. `orig/` is ignored by git.
- The CodeWarrior compilers, in `tools/compilers/` (not in the repository): this project uses `Wii/1.0`
  (mwcceppc 4.3 build 145) for most code and `Wii/1.0a` for FMOD and the Home Button Menu, plus `GC/3.0a5` for part
  of the Bluetooth stack (per-library profiles are in `docs/toolchain.md`). The compiler pack published by
  decomp.dev (tag `20251118` in `configure.py`) can be fetched with `build/tools/download_tool.exe --tag <tag>
  compilers <output>`; the build does not fetch it for you.
- Pinned binaries in `tools/bin/` (not in the repository): `dtk.exe` (decomp-toolkit v1.8.4), `objdiff-cli.exe`
  (objdiff v3.8.2) and `ninja.exe` (ninja v1.13.2). `docs/toolchain.md` gives each download URL and SHA-256.
  `gc-wii-binutils` and `sjiswrap` are downloaded by the build into `build/`.

Steps, from the repository root:

```
tools\build_c.cmd                  # build the C tools into build\tools\
build\tools\doctor.exe             # optional: checks the pins, compilers and the DOL hash
build\tools\configure.exe          # generate build.ninja
tools\bin\ninja.exe                # split, compile, link, check the SHA-1, write the objdiff report
build\tools\gate.exe               # clean rebuild in a private tree; the check that must stay green
```

The first `ninja` run analyses the DOL and writes the split assembly under `build/`. Progress is reported by
`build/tools/status.exe`; `build/tools/match.exe <unit>` compiles one unit and scores every function against
retail. The full list of tools, with their flags, is [`docs/tools.md`](docs/tools.md); every tool prints its options
with `--help`.

## Repository layout

| path | contents |
|---|---|
| `src/` | reconstructed source, one directory per library: `revolution/` (RVL SDK), `MSL_C/`, `Runtime.PPCEABI.H/`, `MetroTRK/`, `rwsdk/`, `fmod/`, `zlib/`, `libjpeg/`, `climax/`, `game/` |
| `include/` | headers, mirrored by library; engine and game headers are included as `<climax/X.h>` and `<game/X.h>` |
| `config/R5WEA4/` | dtk configuration: `config.yml`, `symbols.txt`, `splits.txt`, `build.sha1` (the hash contract), the baseline objdiff report |
| `configure.py` | the unit table: every unit, its library, compiler flags and `Matching`/`NonMatching` status (read by `build/tools/configure.exe`) |
| `docs/` | `progress.md` (ledger), `exceptions.md`, `toolchain.md`, `tools.md`, `references.md`, `library_map.md` and others |
| `tools/c/` | the project's tools (C17) and vendored third-party libraries; `tools/build_c.cmd` builds them |
| `orig/` | empty except `INVENTORY.md` and a placeholder; where you place your copy of the game (ignored by git) |
| `ref/` | small text-only reference notes (format notes, PS2 string lists, unit boundary candidates) |
| `AGENTS.md` | the project guide: policies, rulings, naming and layout by module |

## How work is verified

**The byte gate.** `build/tools/gate.exe` rebuilds from scratch in a private directory, links the `Matching` units
from source and everything else from the original bytes split out by dtk, and requires the result to hash to the
retail SHA-1. A function counts as done only when the whole binary linked with it compiled from source still hashes
identically. A second, independent check compares each function against retail with objdiff, and any disagreement
between the two is treated as a bug. The gate is green with nothing reconstructed, so coverage is reported
separately from the objdiff report.

**The rules.** Identical bytes alone are not a match. `build/tools/check_rules.exe` checks each matched function
against the project's code rules: each type declared once, each global defined in its owning unit, no fixed-offset
casts, real vtables, no fake code (probes, `volatile` carriers, dummy locals) and no assembly bodies. A function that
matches but breaks one of these rules is `MATCH-PENDING`: its source is kept, and it is reported and not counted.
Style rules that the bytes defeat are recorded as `RULE-EXCEPTION`s at the code.

**Assembly.** A function the evidence shows was hand-written assembly in the original is `ORIGINAL-ASM`: it is
written as an `asm` function (or in a `.s` unit where the original was an assembly file), listed apart, and counted
neither as matched nor as open.

`AGENTS.md` ("The byte gate", "Code rules") defines these terms, and [`docs/exceptions.md`](docs/exceptions.md)
lists every `MATCH-PENDING`, `RULE-EXCEPTION` and `ORIGINAL-ASM` entry with its reason.

## Sources, references and credits

This project rests on the work of others; this section lists the main sources and references. The trust order and the policies for using each source are in
`AGENTS.md` ("Reference sources", "Reference policy"); the index of public decompilations is
[`docs/references.md`](docs/references.md). A source file that started from another project's code carries a
`// Reference: <project> <path>` comment naming it; 558 files under `src/` and `include/` do.

### Public decompilations used as references

Where the header comment names a file, the file's reconstruction started from that project's code and was adapted to
this project's practices and to our binary (our SDK build and flags differ from most of these projects). Where it
names only `symbols.txt`, `splits.txt` or declarations, only function names, unit boundaries or declarations were
consulted. File counts are the files (sources and headers) whose header comment names the project, as of
2026-11-16. Licences are those `docs/references.md` records; "see that project" means the repository does not record one.

| project | files | where used | licence |
|---|---|---|---|
| [yannicksuter/mscharged-decomp](https://github.com/yannicksuter/mscharged-decomp) (Mario Strikers Charged) | 163 | RVL SDK: Bluetooth (BTE) stack (116 files), Home Button Menu, a few WPAD, WUD and GX units | CC0-1.0 (BTE parts are Broadcom Apache-2.0 source) |
| [zeldaret/tp](https://github.com/zeldaret/tp) (Twilight Princess) | 136 | RVL SDK (OS, GX, IPC, DVD, NAND, WPAD, MEM and others), MetroTRK, a few MSL and runtime files | CC0-1.0 |
| [xbret/xenoblade](https://github.com/xbret/xenoblade) (Xenoblade Chronicles) | 88 | Home Button Menu (53 files), MSL C, MetroTRK, a few OS, DVD and USB units, runtime | CC0-1.0 |
| [ThePlayerRolo/sdk_2009-12-11](https://github.com/ThePlayerRolo/sdk_2009-12-11) (Revolution SDK, Dec 2009) | 76 | Home Button Menu (nw4hbm, 72 files), a few SDK headers | no licence file; README notes Apache-2.0 for the Bluetooth parts |
| [tgsm/pbg](https://github.com/tgsm/pbg) (Piglet's Big Game, GameCube) | 59 | RenderWare core and headers (11 of the files for function names or unit boundaries only) | CC0-1.0 |
| [bfbbdecomp/bfbb](https://github.com/bfbbdecomp/bfbb) (SpongeBob SquarePants: Battle for Bikini Bottom) | 44 | RenderWare headers and names (14 of the files for names or declarations only) | CC0-1.0 |
| [zeldaret/ss](https://github.com/zeldaret/ss) (Skyward Sword) | 38 | MSL C math library | CC0-1.0 |
| [doldecomp/ogws](https://github.com/doldecomp/ogws) (Wii Sports) | 36 | RVL SDK: AX, AXFX, NdevExi2AD, MEM and a few others | CC0-1.0 |
| `doldecomp/mkdd`, `doldecomp/brawl`, `PrimeDecomp/sdk`, `zeldaret/oot-vc` | 2, 2, 3, 1 | THP decoder (mkdd), OS locals and semaphore (brawl), USB and DVD (PrimeDecomp/sdk), MSL `qsort` (oot-vc) | see that project |
| `aap/librw` (2 files, field layouts of RenderWare chunks) and `electronicarts/RenderWare3Docs` (3 files, documentation) | 5 | RenderWare names and layouts only | librw: MIT (per `docs/references.md`); see that project for the other |

Alice-2010/Decomp's zlib 1.2.3 inflate files were compared with ours and found to be the same text; no code was
taken from them.

### Libraries reconstructed from their public source

- **zlib 1.2.3** (inflate side: `src/zlib/`): upstream source, unmodified apart from address comments and evidence
  notes. zlib licence; see zlib.net.
- **IJG libjpeg 6b** (`src/libjpeg/`): reconstructed following the stock IJG 6b source, whose spellings and macros
  the code keeps. IJG licence; the tree records no URL.
- **dlmalloc 2.8.3** by Doug Lea (`src/fmod/dlmalloc.cpp`): the allocator FMOD embeds, written from the author's
  public-domain `malloc-2.8.3.c` with FMOD's configuration and edits, listed in the file's header comment.
- **fdlibm** (`e_acosf.c`, `k_sinf.c`): the constants and steps in two RenderWare math routines only.

### Public headers and documentation

- **FMOD Ex 4.x.** The public FMOD Ex headers and the `fmod_*.cpp` strings in the executable. No FMOD source is
  used.
- **MSL C and the Metrowerks runtime.** Names and unit layout follow the public Wii and GameCube decompilations above.

### Sibling binaries used as evidence

Several other programs share this game's engine, middleware or code lineage. They are **not in this repository**,
and no code or text is taken from them. They are read locally for names, object-file names and unit boundaries, and
struct or class layouts (from their debug information), and they never take part in the byte gate.

- *Gormiti: The Lords of Nature* (Wii, Climax, 2010): builds with debug information and link maps; same compiler
  family and the same FMOD and RenderWare builds. Function names, unit names and layouts are taken only where
  `build/tools/sibling.exe` pairs a function by identical normalised body; elsewhere retail decides.
- *Ghost Rider* (PS2, Climax): a 2006 preview build with symbols (member and class names, never offsets) and the
  retail build's string literals (class-name evidence, recorded in `ref/ghostrider_ps2/`).
- An earlier (May 2006) PS2 build of the Silent Hill Origins lineage: class and member names only.
- The PS2 release of this game: its stripped executable gives the PS2 build as a second compiler for inline and
  call-set questions, and its FMOD source paths.

### Community format documentation

- [RenderEclipse-Tools](https://github.com/IWILLCRAFT-M0d/RenderEclipse-Tools): community research on the data files
  of Climax's Silent Hill games (`FileFormats.md`). Trusted for file formats, not for code names; each claim used
  was read against the retail code (`docs/references.md`, `ref/shsm_formats/README.md`). It states no licence; no
  text or code of it is used.
- Climax_Game_Engine_Toolkit: community documentation of the same engine family, listed in `AGENTS.md` as a format
  reference. The tree records no URL.

### Build and analysis tools

| tool | use | licence |
|---|---|---|
| [dtk-template](https://github.com/encounter/dtk-template) (commit `95a941f`) | the project scaffold, its documentation is kept in `docs/dtk-template/` | CC0 1.0 (`docs/dtk-template/LICENSE`) |
| [decomp-toolkit](https://github.com/encounter/decomp-toolkit) v1.8.4 | splitting and linking the DOL | see that project |
| [objdiff](https://github.com/encounter/objdiff) v3.8.2 | per-function comparison and the progress report | see that project |
| [ninja](https://github.com/ninja-build/ninja) v1.13.2 | build driver | see that project |
| [gc-wii-binutils](https://github.com/encounter/gc-wii-binutils) 2.42-2, [sjiswrap](https://github.com/encounter/sjiswrap) v1.2.2 | assembler and Shift-JIS wrapper, downloaded by the build | see those projects |
| [wibo](https://github.com/decompals/wibo) 1.0.3 | optional wrapper for running the compiler off Windows; pinned in `configure.py`, unused on Windows | see that project |
| [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) (commit `059609d`) | optional, for register-allocation experiments | see that project |
| Metrowerks CodeWarrior for Wii (`mwcceppc`, `mwldeppc`) | the original compiler and linker; proprietary, not distributed here | see the vendor |

`tools/c/` holds this project's own tools. Their shared core and CMake structure come from the maintainers' earlier
decompilation tooling (`tools/c/README.md`, "Provenance"), and `tools/c/DEVIATIONS.md` records how each C tool
differs from the retired Python tool it replaced. Vendored under `tools/c/third_party/`, each with a `VERSION`, a
`NOTICE` and its licence file:

| library | version | licence file |
|---|---|---|
| [Capstone](https://github.com/capstone-engine/capstone) | 5.0.7 | `LICENSE.TXT` (BSD 3-Clause) and `LICENSE_LLVM.TXT` (NCSA) |
| [PCRE2](https://github.com/PCRE2Project/pcre2) | 10.44 | `LICENCE` (BSD 3-Clause with the PCRE2 exception) |
| [SQLite](https://www.sqlite.org/) | 3.50.4 | `PUBLIC-DOMAIN.txt` (public domain) |
| [zlib-ng](https://github.com/zlib-ng/zlib-ng) | 2.2.4 | `LICENSE.md` (zlib licence) |

## Contributing

The rules for the work are in `AGENTS.md`. In short: no original game files in the repository (`orig/` and disc
images are never committed) and no decompiler output pasted as source;
a decompiler may be used for reading only. Changes are checked with `build/tools/gate.exe` before they are merged.
Parts of this project were developed with the help of AI coding agents (Anthropic's Claude), which appear as
co-authors in the commit history.

## Licence

The project's own text, source and tools are dedicated to the public domain under
[CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) (see [`LICENSE`](LICENSE)). Material adapted
from other projects and third-party code keeps the terms of its source, as listed in the Sources, references and
credits section above. The game itself is not part of this repository and is not covered by this licence.
