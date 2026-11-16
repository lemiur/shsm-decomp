# Matching decompilation: agent guide

Silent Hill: Shattered Memories (Wii, USA, game ID `R5WEA4`), built with dtk-template and decomp-toolkit.

Use Opus 5.5 agents for work requiring reasoning, Sonnet 5.5 High for tasks that are well bounded, and Haiku 5.5 Max for mechanical tasks like reading large files. Do not read large files into your own memory or write code yourself. Use agents for these tasks. Prefer a cheaper/faster model when applicable. Keep agent context for Haiku models below 100k at all times, and context for other models below 200k.
Please have agents and this session use Haiku 5.5 Max subagents for reading large files.

Reconstruct original source so the original compiler, with the original flags, emits object code that matches
the retail binary byte for byte, function by function. Correct structure outranks any score: classes, types,
ownership, storage, control flow, calling conventions and referents.

The point of the project is source a reader can understand. Matching bytes prove the source is faithful; they
don't make it readable. Every function must pass the reader test of the `decomp-code-rules` skill: would a
programmer on the original team have typed this? Source that matches through tricks (a fakematch) is not a
match.

The skills (`matcher`, `wall-identifier`, `helper-recovery`, `permute`, `orchestrator`) use the generic verbs
defined in the project adapter (`docs/tools.md`).

## Working one function

Run everything from the repository root. Every tool prints its options with `--help`; the full verb table (all tools,
all flags) is `docs/tools.md`; compiler flags and per-library profiles are `docs/toolchain.md` (read only your
library's row). The handful a matcher needs:

- `build/tools/brief.exe <symbol or address>`: the starting briefing (identity, status, twins, context, listing).
- `build/tools/match.exe <unit> [SYMBOL] [--changed] [--diff SYMBOL]`: compile the unit with its exact flags, score
  every function against retail, show an aligned diff. Units are `main/<path without extension>`.
- `build/tools/wall.exe <unit> <symbol>`: why a function does not match (referent, call set, control flow, schedule).
- `build/tools/query.exe fn|callers|xref|notes <symbol or term>`: retail assembly, callers, references, docs mentions.
- `build/tools/names.exe demangle|mangle|find|rename`: names and mangling; rename moves symbols and source together.
- `build/tools/witness.exe of|inlined <function>`: the PS2 build as a second compiler (what it kept out of line).
- `build/tools/check_rules.exe --units main/<unit>`: the rule checker; a `[match]` finding makes a function
  `MATCH-PENDING`.
- `build/tools/gate.exe`: the byte gate, before any commit or merge.

## Project rules and policies

**Byte gate.** `config/R5WEA4/build.sha1` holds the one hash contract
(`6a3964eaf1ebe8225283363e4161a793a03683d3`, the retail `orig/R5WEA4/sys/main.dol`; `config.yml` carries the same
hash so dtk refuses a wrong input DOL). The link takes `Matching` units from source and everything else from
dtk's split asm objects, so the gate is green at 0%; coverage comes from the objdiff report (`build/R5WEA4/report.json`,
"linked" figures). Gate only from a clean build: `build/tools/gate.exe` rebuilds from scratch in `build/gate/tree`.

**Score baseline.** `config/R5WEA4/baseline_report.json` is the objdiff report of the last merged state, copied
from `build/R5WEA4/report.json` after a green gate. `build/tools/gate.exe` compares against it (its regressions
step); by hand: `cp config/R5WEA4/baseline_report.json build/R5WEA4/baseline.json && tools/bin/ninja.exe changes`
(any regression is a defect). At each merge, refresh the baseline from the gate's report and regenerate the ledger
`docs/progress.md` (both numbers and the per-function status) and the exceptions record `docs/exceptions.md`
(`MATCH-PENDING`, `RULE-EXCEPTION`, `ORIGINAL-ASM`, listed helpers) in the same commit: the ledger with
`build/tools/ledger.exe progress --report config/R5WEA4/baseline_report.json`, the exceptions record by hand, checked
with `build/tools/ledger.exe exceptions-check`.

**Parking mechanism.** Per unit, in `configure.py` `config.libs`:
- `Object(NonMatching, "<path>")`: source is compiled and scored by objdiff but **not linked**; the retail asm is
  linked instead. This is where unfinished or `MATCH-PENDING` work lives.
- `Object(Matching, "<path>")`: linked into the gated DOL. Flip only after the unit is 100% and the gate passes.
- `Object(Equivalent, "<path>")`: linked only with `--non-matching` (functionally equivalent builds).

A unit is linked as a whole, so one unfinished function parks the unit. Every new unit also needs its address
ranges in `config/R5WEA4/splits.txt`.

`.s` source units are allowed (user's ruling, 2026-11-14): assembled with binutils `as`, every function in them is
`ORIGINAL-ASM` (listed in docs/exceptions.md; the rule checker does not read them, the ledger needs no R18 hit).
**Assembly functions in C sources (user's ruling, 2026-11-15).** An object whose original source was a C or C++ file
of hand-written assembly functions (the sibling's compile-unit path is a `.c`/`.cpp`, its line table has one line per
instruction, the frames are not the compiler's) is reconstructed as that C file with `asm` functions (`nofralloc`
where the frame is hand-written); a `.s` source unit is used only where the original was an assembly file
(MetroTRK's `__exception.s`). Either way every such function is `ORIGINAL-ASM`, listed in docs/exceptions.md and
counted neither as matched nor as open.

**Annotations.** Function identity is the symbol name in `config/R5WEA4/symbols.txt` (address, size, scope);
source functions match by that name. Renaming a function means renaming it in `symbols.txt` and the source together.
Each reconstructed function's header comment ends with its address, `// 0x8XXXXXXX`. Stubs: the unit stays
`NonMatching`. Data: defined in its owning unit under its `symbols.txt` name; compiler-generated data
(`@stringBase0`, `@NNN` float constants, `lbl_` string literals) keeps the compiler's form.
An `@NNN` name in `symbols.txt` is used only where dtk's analysis supplied it; never guess a number (a stripped DOL
cannot give one). Unnamed literals stay `lbl_` (ruling in docs/exceptions.md, "OS unit rulings").

**Reference sources** (trust order):
1. The DOL itself: dtk signature names (reliable), string literals (FMOD `fmod_*.cpp` source names, RenderWare
   `rwID_*` names, Climax class-registry names and `.xaml` UI names), RVL SDK banners.
2. Public Wii decomps for RVL SDK, MSL, the Metrowerks runtime and MetroTRK names and unit layout.
3. RenderWare Graphics 3.7 SDK headers and documentation (Rw/Rp/Rt API names).
4. FMOD Ex 4.x headers (public API, `FMOD::` classes); internal `*I` classes follow FMOD's naming.
5. Climax engine evidence: class names from string literals in this DOL (the class registry and its
   `.xaml`/name strings), cross-checked against the class-name string literals recovered from Ghost Rider (PS2,
   Climax, same engine family) in `ref/ghostrider_ps2/` (`classes.txt`; 27 of those names also occur in this DOL).
   The Ghost Rider ELF is stripped: it provides string literals only, no symbols. Also the community
   Climax_Game_Engine_Toolkit docs and RenderEclipse-Tools `FileFormats.md` on GitHub (file formats, chunk IDs),
   trusted for formats, not for code names.
6. The PS2 release of SHSM (`orig/_iso/`, stripped MIPS ELF) for its `z:/SH1R/Code/game/libs/FMOD/src/*.cpp` paths.
7. Sibling binaries with debug information (rank: user's ruling, 2026-11-15, below). They give names, translation
   units and layouts, never source text, and are never gate input; a name or unit taken from one says so at the code.
   - Gormiti: The Lords of Nature (Wii, Climax, 2010), `orig/_ref/gormiti_wii/`: Debug, Release and Final ELFs with
     DWARF 2 and link maps. Same compiler family, the same FMOD Ex and RenderWare builds: `build/tools/sibling.exe`
     pairs functions by identical normalised bodies (1,796 of 1,880 FMOD functions, 1,060 of 1,286 RenderWare
     functions, about 480 Climax engine functions; name agreement 98% on 2,888 functions that already had names)
     and reads its DWARF (`sibling.exe types`, `fn`, `cu`). **It ranks directly after item 1**, but only for the
     functions `sibling.exe` pairs with it by identical normalised bodies, and only for function names, unit
     (object-file) names and boundaries, and struct and class layouts. Where retail's copy differs from Gormiti's
     (unpaired, or a same-named function of another size: another revision), Gormiti is a reading aid and the retail
     assembly decides.
   - Ghost Rider preview (PS2, 2006-07-20), `orig/_ref/ghostrider_ps2_preview/`, and the 17 May 2006 PS2 build
     labelled Silent Hill Origins, `orig/_ref/origins_may17_2006/` (an earlier build of the same lineage): gcc
     ELFs with `.symtab` and a stabs `.mdebug`: 303 `Rwp*` RW Physics functions and 70 physics source files, and
     member layouts for about 30 Climax classes this DOL shares. **They rank with item 5**: names and member names,
     never offsets (another compiler). Imported by `sibling.exe index|pair|types … --name <name>`; their
     cross-architecture pairing is trusted only in its sequence tier.

**Policies.** Running the game in an emulator for observation is allowed. A decompiler (Ghidra etc.) may be used
for reading; never paste its output as source. Generated output goes in `build/` (ignored). Original game files
in `orig/` and the root disc images and archives are never committed. Commit messages name the change
(`match: reconstruct OSInit`, `split: add RVL SDK os units`). A unit path no evidence names (a guessed TU
filename) is allowed: mark it "unit name unconfirmed" in a comment at the top of the file and list it under open
items in `docs/exceptions.md`; rename it when evidence appears.

**Reference policy (user's ruling).** For third-party libraries (RVL SDK, MSL, the Metrowerks runtime, MetroTRK
and similar public libraries), workers may use other public decomp projects' reconstructed source (for example
zeldaret/tp, ogws) as a reference. Every source file that used such a reference carries a header comment naming
the project and the file, e.g. `// Reference: zeldaret/tp libs/revolution/src/os/OSThread.c`. Game (Climax) code
has no such reference.

**Reference first (user's ruling, 2026-10-23).** Before working on any library, check whether a public Wii (or
GameCube, for shared libraries) decomp project has already decompiled it (`docs/references.md` indexes the known
coverage). If one has, start from that project's code rather than reconstructing from scratch, and bring it to this
project's practices and quality standards: our unit layout, headers and naming per module, the reader test and every
[match] rule, the `// Reference:` header comment, and the byte gate on our binary. A reference body that matches only
through a fakematch, or that differs from our retail build, is adapted, not copied as-is. The vendor-source ruling
below still applies.

**Vendor source (user's ruling, 2026-10-18).** Vendor `.c` source re-hosted without a licence is not used, not even for reading, and no vendor text is copied: declarations are written in this project's headers, in this project's form.

**Dead-stripped functions (user's ruling, 2026-10-23).** A function the reference defines but the original link
dropped (no copy in retail) is handled per library family. SDK C units (RVL SDK, including WPAD/WUD/KPAD and BTE)
keep the reference's body, marked `// no copy in retail` at the code. nw4hbm and other C++ libraries declare it
only, unless retail data proves the body was compiled (for example constants or strings it alone owns, as with
`AtanFIdx` in `math_triangular`), in which case the body is kept and the evidence noted. That proof includes
(user's ruling, 2026-11-15) a function the sibling's link map lists as `UNUSED` in the same object when the unit's
emitted layout needs it (it is what gives retail's weak destructor, vtable order or constant-pool order); such a body
is kept, marked `// no copy in retail (dead-stripped)`, with the evidence noted.
R17 does not apply inside a `// no copy in retail` body: it is the reference's text, not matched code (user's ruling,
2026-10-23). The rule checker skips R17 in bodies whose closing line carries the plain marker, `(dead-stripped)` or
`(dead-stripped or inlined)` (the last confirmed by the user's ruling, 2026-10-23); an `(inlined)`-only body does not
exempt, because that code reaches retail through its callers.

**Embedded line numbers (user's ruling, 2026-11-15).** Where retail's bytes embed a source line number (FMOD's
allocator and debug calls pass `__LINE__`), the project form is one `#line N` directly before the statement whose
retail code carries line N, with N taken from the retail constant and, where the function pairs, confirmed by the
sibling's line table. Nothing else sits between the directive and that statement, and `#line` moves no other code.
Padding a file with blank lines to reach a line is fake layout and is not used.

**Locals against the sibling's DWARF (user's ruling, 2026-11-15).** A local the sibling's DWARF does not list, or lists
in another scope or position, is accepted with a note in the function's header comment when it has a meaning of its
own (not a carrier) and either retail is shown to be another revision than the sibling or the DWARF could not record
it (MWCC's DWARF drops locals the optimiser eliminates and records no `const`/`volatile`). A deviation that is only a
declaration moved to get retail's register numbering (`WorldSyncCamera`'s `spSect`, `_rwFreeListFreeReal`'s `heap`)
is a `RULE-EXCEPTION` under "the bytes win": `// RULE-EXCEPTION: R30, <reason>` at the code, listed in
docs/exceptions.md; the function counts.

**Compound literals in C++ (user's ruling, 2026-11-15).** The compound literal in FMOD's `MusicSettings::MusicSettings`
(an MWCC extension in C++) is accepted with its comment: the sibling's line table puts the copy and the `memcpy` on
one line and names only `cprop`.

## Objective and scores

- The goal is an exact match for every function: identical instructions, constants and *ordered relocation
  targets*. Fuzzy and aggregate percentages are navigation only.
- If the project tracks per-function history, keep three numbers per function:
  - **current**: the score at the latest build;
  - **best**: the best score of the function's current source. It resets when the source changes;
  - **historical**: the best score ever, across all source revisions.

  `historical > best` means an earlier source matched better: a lost match to recover from version control
  before inventing anything new.
- A current-score dip in an *unchanged* function, caused by codegen shifting elsewhere in its translation unit,
  is not a regression.
- Masked bytes are not enough. A matching body that calls the wrong function, or names the wrong global, is a
  defect.

## Workflow

1. **Pick work** (`pick`, then `brief`). Choose the lowest-scoring or never-matched functions, lost matches, or a
   unit close to complete. Check existing notes and verdicts (source comments, history) first. `docs/library_map.md`
   gives the rough library layout of `.text`.
2. **Classify the gap** (`wall-identifier`; `wall`): referent, then inline/call set, then control flow, then
   register/schedule. Fix the earliest class that still differs. For an inline/call-set question, `witness` shows
   which helpers the PS2 build kept out of line.
3. **Reconstruct** with `matcher` (an unstarted function with a matched twin starts from a mechanical `draft`);
   recover hidden helpers with `helper-recovery`. Use `permute` only for a
   diagnosed register/schedule residue, and only after compiler and linker tracing (the compiler-trace executables,
   `docs/compiler_regalloc.md`) has failed to name the cause (user's ruling). Permuter output is never committed
   unless the resulting source passes the reader test; record each permuter use in `docs/exceptions.md`.
4. **Iterate** with `match <unit>` (`build/tools/match.exe`). Run `build` when a change spans units, for example a
   shared-header edit.
5. **Gate** (`gate`) before committing or merging. The gates must stay green.
6. **Stop.** Leave a complete body whose remaining difference is bounded by evidence (`eqcheck`, `witness`) in a
   `NonMatching` unit, with a one-line reason in its header comment.

## The byte gate

A function is done when **the whole binary, linked with that function compiled from source, still hashes to the
retail binary's hash**. Nothing else counts: not "looks equivalent", not a passing test, not a local diff of zero,
not a closeness score, not an agent's verdict. Every other signal is a filter feeding this gate. That is what makes
automation safe: a weak model or a buggy tool can waste time, but a wrong answer cannot land.

- **The gate has two states**, identical or failed; there is no third. It needs:
  - one hash contract per binary;
  - a build that links matched objects together with fallback bytes from the original for everything not yet
    matched;
  - a check that exits non-zero on any mismatch.

  Unmatched or non-matching source never enters the gated build.
- **Build it first.** A link made entirely from the original's bytes must reproduce the hash before progress is
  measured against it. (Done: the all-asm link of R5WEA4 passes.)
- **What the gate is blind to, and the rule that covers each blind spot:**

| blind to | rule |
|---|---|
| **coverage**: the gate is green at 0%, because the fallback supplies the original bytes | report how much of the image came from source, counted from the link's own record of where each byte came from; keep a second, independent check (per-function match) and treat any disagreement between the two as a bug |
| **which hash** | the gate verifies that its contract belongs to the binary it builds, and that the configured original still has that hash |
| **whether the build ran**: a failed link leaves the old binary in place | delete the output first, and judge from the linker's and the build's exit codes, never from the file |
| **incremental state** | gate only from a clean rebuild into a fresh directory |
| **whether the body is source**: pasted assembly (`__asm`, emitted bytes) matches trivially | inline-asm bodies are "verbatim", never counted as matched source; audit them (functions that were assembly in the original are `ORIGINAL-ASM`, listed apart) |
| **whether the source is honest**: fake code (dummy locals, `volatile` carriers, offset casts) matches too | the rule checker (`rules`) runs over every matched function; [match] findings make it `MATCH-PENDING` |
| **what was committed**: a gate that substitutes drafts can destroy uncommitted work | commit matched work the moment it exists, and never gate on a dirty tree (other than tool-database churn) |

- **"Banked" is a printed line, not an intention.** Write "matched" or "banked" only from the gate's printed success
  line. A helper must refuse an empty work list. Count from the source, never from a ledger.
- **A red gate is a hash, not a diagnosis.** Turn it into a named symbol: attribute the differing bytes to the
  function and unit that hold them, or substitute the one draft into a copy of its unit and read the compiler's first
  conflict or redefinition error.
- **Batch, then bisect.** One clean rebuild verifies a batch of drafts. On red, bisect, or drop the units the byte
  attribution names, so that one bad draft cannot sink the rest. When parallel drafters declare a shared callee
  differently, the failure is the clash, not either draft: normalise the declarations, gate one at a time, and re-gate.
- **A slow gate is a bug.** Measure it before you fear it; run builds in parallel, and give the expensive binary its
  own lane rather than excluding it. (A clean all-asm gate takes about 10 seconds.)

## Code rules

The full catalogue is the user-level `decomp-code-rules` skill (`references/rules.md`). Two laws apply always:

1. **The bytes win.** A change made for style must not change what the compiler emits. When a rule and the bytes
   disagree, keep the bytes and write `// RULE-EXCEPTION: <rule>, <reason the bytes give>` at the code.
2. **Identical bytes alone are not a match.** A function counts as matched only when its bytes match *and* it
   passes every **[match]** rule:
   - R1: each type declared once, in a shared header;
   - R2: each global declared once, as `extern` in a header, and defined in its owning unit;
   - R4: no fixed-offset casts, and no offset arithmetic on `this`;
   - R13: real vtables: every slot in the binary's vtable declared, none added (a slot of unknown purpose keeps
     a placeholder name);
   - R17: no fake code (probes, `volatile` carriers, dummy locals, steering blocks);
   - R18: no assembly bodies.

   A function that fails is `MATCH-PENDING`: keep its source, report it, and don't count it. A function the
   evidence shows was originally assembly is `ORIGINAL-ASM`, listed apart. Naming (R7) is **not** required for a
   match; placeholder names are open work.

**R18 and inline-assembly helpers (user's ruling, 2026-11-15).** A small inline helper whose body is hand-written
assembly (the paired-single vector helpers of `include/game/VectorPS.h`, the SDK's `OSFastCast.h` casts and the like)
is `ORIGINAL-ASM`: listed once, by name and header path, in docs/exceptions.md, counted neither as matched nor as open.
A function that calls or inlines such a helper is not itself an R18 finding: its own body is C++ and it counts when it
matches and passes the other [match] rules. The helper must be a real unit of the original (the same instruction
sequence at several call sites, `fragments.exe in <function>` / `twins.exe`); a function-specific assembly block
wrapped in a helper to dodge R18 is still a fakematch. The checker reports R18 on the function whose own body holds
assembly (an `asm` function, an `asm { }` block, or a helper defined in the same file and called once), and a
header-level `asm` inline that is not listed as "helper not listed as ORIGINAL-ASM".

Progress reports two numbers: **matched under the rules** (the headline) and **bytes matched**.

### Project profile

| item | this project |
|---|---|
| compiler dialect: what it accepts and rejects (R22) | CodeWarrior C/C++ for Wii (mwcceppc 4.3 build 145; C99-ish C with `//` comments and mixed declarations, C++98 with templates). Accepts `asm` functions and `__declspec(section)`, `#pragma` controls. C++ exceptions are likely **off** (extab and extabindex are only 0x60 bytes each, 5 `@etb`/`@eti` entries), so `-Cpp_exceptions off` is the default; units with extab entries need `-Cpp_exceptions on`. RTTI: PROVISIONAL `-RTTI off`, not yet checked (look for type_info objects referenced from vtables). Middleware and game C++ language level is not yet known per library. |
| naming-style table, one row per module (R8) | See "Naming and layout by module" below (the user's ruling, 2026-10-17). |
| layout style: braces, indent, line limit, banners (R23, R32, R33) | Per module, below. Engine and game code default to the `style-defaults.md` Table 2 layout (id Tech 4) where no evidence exists. |
| placeholder patterns (R7) | `fn_[0-9A-F]{8}`, `lbl_[0-9A-F]{8}`, `unk_[0-9A-Fa-f]+` and `unk_<Role>` (game-code placeholder classes), `field_0x[0-9A-Fa-f]+`, plus dtk's `dtor_`, `sinit_`, `jumptable_`, `gap_` and `@stringBase` generated names. |
| evidence sources for names (R7) | dtk signature names; DOL strings (FMOD `fmod_*.cpp`, RenderWare `rwID_*`, Climax class-registry and `.xaml` name strings, SDK banners, error messages); public Wii decomps (SDK/MSL/runtime/TRK); RenderWare 3.7 SDK; FMOD Ex headers; Ghost Rider (PS2) class-name string literals in `ref/ghostrider_ps2/` (stripped ELF: no symbols); Climax_Game_Engine_Toolkit docs and RenderEclipse-Tools `FileFormats.md` (GitHub) for formats. See "Reference sources" above. |
| does the binary record source lines? | No: the retail DOL is stripped (no map, no DWARF, no RELs). Layout changes are [free] unless the unit uses `__LINE__`, `assert`, or `__FILE__`-style debug macros (FMOD passes source file names to its debug calls: check for `__LINE__` constants next to `fmod_*.cpp` string references before reflowing FMOD code). Where retail bytes embed a line number, the project form is `#line N` directly before the statement whose code carries line N (user's ruling, 2026-11-15; see "Embedded line numbers" below). |
| parking mechanism (exempt from R37) | dtk-template unit status in `configure.py`: `Object(NonMatching, ...)` (compiled, scored, not linked) versus `Object(Matching, ...)` (linked, gated); `Equivalent` for `--non-matching` builds. No `#ifdef NON_MATCHING` bodies in Matching units. |
| rules this project tightens or relaxes | **R17, SDK delay loops (the user's ruling, 2026-10-18).** R17 accepts a `volatile` local used only as the counter of an empty-bodied busy-wait loop (the SDK's own delay idiom, e.g. zeldaret/tp's `volatile u32 a; for (a = 0; a < 1000; a++) {}` in `__VIInit`) when its declaration carries `// volatile: delay loop, <reference project and file>` and the same form appears in the public reference. The checker accepts exactly that shape: a plain scalar declared without an initialiser, every other use in the header of one `for (v = A; <test of v>; v++)` loop whose body is `{}` or `;`. **R17, `volatile` (the user's ruling, 2026-11-15)** accepts two more shapes, and nothing wider. (a) A file-scope or global variable that retail reloads at every use and that is written from interrupt or callback context, with `// volatile: <evidence>` on its declaration (the RenderWare display-list tokens `_RwDlTokenCurrent`, `_RwDlTokenLastSeen`, written by the GX draw-sync token callback). (b) A local read only by a release-disabled assertion, as tp's `volatile u32 mail` in `__DSP_boot_task` (the public reference's own form; its only reader is a debug assertion, as in "Release-compiled-out debug checks"): a scalar declared without an initialiser, assigned once by a statement that does not read it, every other use inside the arguments of a macro that compiles to nothing in release (`ASSERTMSG1`, `ASSERTLINE` and the like in `revolution/os/OSError.h`), its declaration carrying `// volatile: read by a release-disabled assertion, <reference project and file>`. Any other volatile local is still a carrier. |

### Naming and layout by module

The per-module evidence style below is the user's ruling (2026-10-17).

| module | names | layout |
|---|---|---|
| RVL SDK (`revolution/`), MSL C, Metrowerks runtime (`Runtime.PPCEABI.H`), MetroTRK | The Nintendo and Metrowerks names and the unit layout used by existing public Wii decomps (`OSInit`, `__OSInitAlarm`, `DVDLowInit`, `GXInit`, `TRKInitializeNub`, `__destroy_global_chain`); file paths as in those decomps (`revolution/os/OSAlarm.c`, `Runtime.PPCEABI.H/global_destructor_chain.c`). | As in those decomps (C, K&R-ish braces, 4-space indent). |
| RenderWare Graphics 3.7 | RW 3.7 SDK conventions, in C: `Rw` core (`RwFrameCreate`, `RwRaster`), `Rp` plugins (`RpClump`, `RpWorld`), `Rt` toolkits (`RtAnim`); `rwID_*` chunk constants; struct types as in the SDK headers. | RW SDK source style (C). |
| FMOD Ex + Event system | FMOD conventions: `FMOD::` namespace, public classes (`FMOD::System`, `FMOD::Channel`, `FMOD::EventSystem`) wrapping internal `*I` classes (`SystemI`, `ChannelI`, `EventSystemI`); units named after the `fmod_*.cpp` strings in the DOL. | FMOD source style (C++). |
| IJG libjpeg | IJG names (`jpeg_read_header`, `jinit_*`, `j*.c` units). | IJG style (C). |
| Climax engine and game code | Names and namespaces evidenced by string literals in this DOL (class registry, `.xaml` and name strings, e.g. `ClimaxT1Entry`), cross-checked against the Ghost Rider class-name literals in `ref/ghostrider_ps2/classes.txt` (`C`-prefixed classes such as `CAnimBehaviour`; namespaces such as `RWS::` appear only in Ghost Rider literals so far). Unknown members, locals and helpers use the `decomp-code-rules` `style-defaults.md` Table 2 fallback (`PascalCase` methods, `lowerCamel` members, `UPPER_SNAKE` constants), keeping the evidenced `C` class prefix. | `style-defaults.md` Table 2 layout (id Tech 4 banners, K&R braces), until evidence says otherwise. |

**Game code layout (user's ruling, 2026-10-17).** Game-specific code goes in `src/game/<Unit>.cpp` and Climax
engine code in `src/climax/`, with their headers mirrored in `include/game/` and `include/climax/` and included as
`<game/X.h>` / `<climax/X.h>`. File names come from class-name evidence (string literals); a file named without
that evidence carries the "unit name unconfirmed" comment and an open item in `docs/exceptions.md` (see Policies).

**Placeholder naming for game code without string evidence (user's ruling, 2026-10-17).**
- Classes are `unk_<constructor address>` (`unk_801A92A8`), or `unk_<Role>` (`unk_Ramp`) with "name invented" in
  the class comment.
- Member functions keep `fn_<address>`; `symbols.txt` carries the mangled form (`fn_800067CC__8unk_RampFf`), as
  it does for free functions (`fn_800C770C__Fi`). Constructors and destructors take their real special names
  (`__ct__12unk_801A92A8Fv`).
- Unknown globals stay `lbl_<address>`.
- A unit's static initialiser is `__sinit_\<Unit>_cpp` (`__sinit_\PadRumble_cpp`).
- Real names replace the placeholders only when strings or other evidence appear.

**Display-height accessor (user's ruling, 2026-10-23).** About 76 game static initialisers open with a pair of
per-unit statics, a float and an int, both read from the display height `lbl_8052861C`, which retail loads twice
(MWCC's common-subexpression pass keeps two loads only when exactly one read is const-qualified). The pair lives in
`include/game/DisplayHeight.h`: `static float displayHeightF = GetDisplayHeight(); static int displayHeight =
lbl_8052861C;`, with `inline const int& GetDisplayHeight()` in `game/Display.h` (all names invented). A game unit
whose static initialiser opens with the pair includes `<game/DisplayHeight.h>` in its `.cpp` and defines no copy of
its own; its two `.sbss` words are `scope:local` `displayHeightF`/`displayHeight` in `symbols.txt`. A unit without
the pair includes only `<game/Display.h>`. Details in docs/exceptions.md, "Display-height accessor".

**Release-compiled-out debug checks (user's ruling, 2026-10-23).** Several triggers test a third save-data key after
the save-data test `fn_80206348` with a dead compare and a constant true. That is a debug check compiled out in
release, two levels deep: a member of each class tests its key field and passes the field itself to the shared inline
`CheckSaveDataKey( const char* key )` in `include/game/SaveData.h`, which tests it again and returns true on both arms
(`if ( key != NULL ) return CheckSaveDataKey( key ); return true;`; names invented). The member is out of line where
another unit calls it (CPointTrigger `fn_800C63AC`) and in-class otherwise (CMessageRelay, AreaTriggerBox `CheckKey`).
The form is accepted as source, not a fakematch; it applies to every site of the shape. Details in docs/exceptions.md,
"Game code rulings (game-11)". The same case covers debug-only assertions (user's ruling, 2026-11-15): RenderWare's
own `RWASSERT`, defined as a release no-op (`include/rwsdk/plcore/badebug.h`), with the assertion written in the source
where the sibling's Debug build shows it, and only there; the code carries `// Debug build: assertion at this point
(Gormiti Debug)` or a comment already in that form (`/* Debug build: ... */`). The user's ruling of 2026-11-16 extends the case to the Climax engine's
debug-warning macro `RWS_WARN( condition, text )` (`include/climax/Debug.h`), a release no-op named (name invented)
after the debug stream `RWS::cdbg()` that Gormiti's Debug build writes to;
it is written only where that build shows a file-and-line warning, with the comment `// Debug build: warning at this point (Gormiti Debug)` and the text that build prints, spelling included.

## Matching rules that are easy to get wrong

- **Levers are disposable experiments.** Never keep probes, unused declarations, fake locals, `volatile`
  carriers, padding or distorted source to move a score. A trick that closes a gap is a clue to the real source
  (`decomp-code-rules`, Section F), not the answer.
- **A dip is not a rejection.** If a change moves codegen toward retail's shape, keep it and compose the next
  lever on top. The gate decides only what is committed.
- **Prefer original spellings.** An inline function or macro is more likely the original spelling than a
  hand-expanded body; prefer it as the base unless the evidence says otherwise.
- **Trust surviving source.** Surviving original or lineage source is presumed authentic; record the decision
  to adopt it where the project keeps such decisions.
- **Bank exacts reached by experiment.** If unchanged source reaches exact under a disposable translation-unit
  experiment, record the exact state, then remove the experiment.
- **Keep every exact match, and count it honestly.** Keep a 100% match even when its source breaks a rule.
  - **A style rule the bytes defeat:** write a `RULE-EXCEPTION` at the code. The function counts.
  - **A [match] rule:** the function is `MATCH-PENDING`. Keep its source as the best-known state, and list it
    as open work.

  Record either case (function, rule, deviation) where the project keeps exceptions, and admit it through the
  gate's allow list.
- **Compiler version is a hypothesis** until ruled: FMOD's is ruled (user's ruling, 2026-11-15) the 4.2 series,
  `Wii/1.0a` (see its row in `docs/toolchain.md`). `Wii/1.0` is chosen from the SDK banner. Middleware (FMOD, RenderWare,
  libjpeg) may have been prebuilt with another CodeWarrior version; if a library resists matching with plausible
  source, try the neighbouring versions in `tools/compilers/Wii/` before distorting source.

## Source rules

- **One class, one definition**, in a shared header. No unit-local copies, layout views or placeholder shells.
- **Each function and global lives in its evidence-backed owner** unit and header. No scattered `extern`s and no
  macro aliases onto address names.
- **Access goes through typed members.** No raw offset casts, offset macros or offset arithmetic on `this`. Use named casts
  only at real boundaries. A cast is usually a symptom: retype the member until it disappears.
- **Unclear identity: chase the evidence** (callers, storage, callees, mangling, vptr stores, RTTI, offsets). If
  it is still unclear, leave an identity-TODO marker. Never fabricate.
- **Names are semantic.** No address-derived names, compiler ordinals or `local_10`-style names in finished
  code. Take names from evidence first: error strings that start with a function name, assert text, symbols.
  A name you invent is marked as invented wherever the project records provenance. Follow the module's own
  naming style.
- **Readable by default.** Recovered source should read like the original team's code:
  - one local per meaning (R30);
  - named constants and flag groups (R11, R29);
  - documented structs (R27);
  - a header comment per function (R33);
  - the original's bugs kept and labelled, not fixed (R36).
- **Use the era's real headers** (RVL SDK, RenderWare 3.7, FMOD Ex) instead of hand-rolled declarations. Mirror
  the original precompiled-header or prelude structure when the evidence shows one.
- **Typed enums for proven numeric domains.** Remember that retyping a parameter changes its mangled name.
- **Aggregates are proven from whole-object use** (`Vec`, `Mtx`, `RwV3d`, `RwMatrix`). Do not split one object into
  overlapping globals, and never model an interior address as separate storage: refine the owner.
- **Vtables are recovered mechanically:**
  - one vtable per polymorphic base;
  - inherited slots are not redeclared;
  - overrides are marked (a comment, or a repeated `virtual`, in C++98);
  - new slots are `virtual`;
  - there are never dummy virtuals: a padded vtable can truncate the real one and crash. A real slot of unknown
    purpose keeps a placeholder name.
- **No fake code**, storage, labels, aliases or padding to improve a score or the final layout.

## Data

- A datum gets a real definition under its `symbols.txt` name. Compiler-generated data (string pools, FP
  constants, dynamic initialisers) is recorded as such, not given an invented owner.
- Aggregate data percentages do not prove that `.data` and `.bss` are correct. The compiler and linker choose
  layout; a field read through a separately declared global gets its own storage, so a link of the source alone
  can behave differently even when every function matches.

## Tests and verification

- Matching work runs no test suites: comparing against retail is the verification, and the gate runs before
  commit or merge.
- Tooling changes run the touched tool's own CTest label (`ctest --preset tools -L <tool>` in `tools/c`, or `tools/build_c.cmd --test`).
- Do not add tests that re-check a function's bytes against retail; the build already does that.
- Claim "matched" or "banked" only from the tool's own printed result (see "The byte gate"), never from
  memory or a ledger.

## Repository hygiene

- Tool inputs go in `config/`, generated output in an ignored `build/`, history in version control. Docs describe
  current usage and contracts only: no diaries, score snapshots or campaign logs.
- Never commit: `orig/` game files (only `orig/INVENTORY.md` and `.gitkeep` placeholders), the root disc images
  and archives, `tools/compilers/`, `tools/bin/`, `tools/src/`, `build/`, or binary blobs under `ref/`.
- Source comments explain the code (`decomp-code-rules` R21):
  - markers;
  - ABI or codegen constraints;
  - explanations of unsafe seams;
  - `RULE-EXCEPTION`s;
  - function header banners and section banners in the project's style (R32, R33);
  - why a block exists;
  - the original's quirks (R36).

  No history, scores, dates or campaign notes.
- **Never run `git worktree remove` on a nested worktree whose directories are NTFS junctions into a parent
  checkout.** The removal follows the junctions and deletes their targets (this destroyed files in a lane slot once).
  Remove each junction first with `cmd /c rmdir <junction>`, which deletes only the link, then remove the worktree.
  `build/tools/slot.exe remove <slot>` performs that procedure.
- Preserve concurrent changes: stage only your own unit of work, and never commit build state. Commit messages
  name the change (`match: reconstruct CThing::Method`, `tools: verify relocation targets`).
