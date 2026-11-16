# Lane protocol: matching at the lowest token cost

How the orchestrator sizes, routes, stops and integrates matching lanes. It adds to the `orchestrator` skill
and `AGENTS.md`; where they disagree, `AGENTS.md` wins.

## Unit of work

- A lane is **one translation unit**: a class (or a small group of classes) with every function in its `.text`
  range, taken from `build/tools/unit_plan.exe --lib <L> --min-conf high`. A unit ending at a static initialiser
  (`sinit`) is the most reliable cut (99% on the units already split). A function counts only when its whole unit
  is 100% and flipped, so lanes are never address ranges that cut through a unit.
- Units whose boundaries are only medium or low confidence wait, unless the worker can prove the boundary
  (static data ownership, a `.ctors` entry, a vtable's key function).
- For FMOD, RenderWare and Climax engine code, `build/tools/sibling.exe units --lib <L>` gives unit boundaries and
  file names from the Gormiti link map (the same developer, compiler and middleware, with a symbol and an object
  file for every function). It outranks `unit_plan` proposals; `brief.exe --unit START-END` says whether a range
  equals a clean sibling object span. A unit named from it carries a
  `// Unit name and boundary: Gormiti link map (<library> <object>)` comment instead of "unit name unconfirmed".
- Twin families: once one member of a family matches, `build/tools/draft.exe sweep` drafts the rest with no model.
  Solve one exemplar of an unmatched family before its other members.

## Worker prompt

The prompt holds only: slot and branch (`slot.exe new` takes `N` or `slotN`); the unit's range and its `unit.exe new`
command (the path carries its extension, `climax/Time.cpp`); the skill to start with
(`matcher`); what other lanes own; the stop rules below. For a function with a sibling counterpart (`brief` shows
it), `build/tools/sibling.exe fn <symbol>` gives the parameters, locals, their registers or frame slots and the
line table, and `build/tools/sibling.exe types <name>` the struct or class layout; `--build debug` shows the
unoptimised build's view of the same function. The worker starts from
`build/tools/brief.exe --unit START-END`, `build/tools/classes.exe header <class>` and `docs/toolchain.md` (its
library's row only). It does not read `docs/tools.md`, `docs/exceptions.md` or `docs/progress.md` whole.

## Using the sibling evidence

What the RenderWare lanes learned from Gormiti's DWARF and Debug build (`build/tools/sibling.exe`):

- What `wall.exe` calls a register/schedule difference is usually a wrong source shape: one local reused for two
  values, a temporary the original did not have, a block-scoped local declared at function scope (or the reverse),
  statement order. Take the locals, their scopes and their names from `sibling.exe fn <symbol>`.
- A static helper or setter that the optimised build inlines is a separate function in the Debug build
  (`sibling.exe fn <symbol> --build debug`, then a bounded `powerpc-eabi-objdump -d` of `GormitiDebug.elf`). It goes
  into the source as a function with `// no copy in retail`.
- A debug-only check (`RWASSERT`) can be the only reader of a variable retail still computes; the Debug build shows
  it. The assertion is written in the source and compiles to nothing.
- Gormiti's DWARF records no `const` and no `volatile`, and no parameters for K&R-style functions: try `const` on
  pointer parameters when loads refuse to hoist above stores; take such parameters from the retail assembly.
- `sibling.exe units` lists the span of an object's paired functions only. Static functions sit in the gaps before
  and after it: check each gap with `brief.exe --unit` and the object's entries in the Gormiti map before creating
  a unit. `sibling.exe units` can give a gap function to an object on size alone (`p2heap`'s trailing function
  belonged to `p2renderstate`): confirm it from its callers.
- Gormiti's debug information also covers the Climax engine classes that game code uses (`CEventId`,
  `CAttributeHandler`): query `sibling.exe types` for any engine class a game function touches.
- The unit planner's range for a game class can leave out the constructor and destructor just before it: check
  which functions read the class's vtable (`CPointOfInterest`'s unit was widened to take them).
- A function with no exact counterpart (`sibling.exe of` says so) may still be described by name
  (`sibling.exe fn '<name>' --sibling`), but Gormiti's copy can be another revision: useful for reading, not
  authoritative for retail.
- After a `symbols.txt` rename run `tools/bin/ninja.exe` before `match.exe`. `unit.exe flip` leaves `configure.py`
  edited: commit `configure.py` alone and leave the baseline and ledger to the merge.
- A deviation from the DWARF that the bytes force (a local declared in another position) is stated in the
  function's header comment.

## Walls

A unit that ends one or two functions short is worth a wall pass (an Opus lane over several parked units): closing
one function banks the whole unit. What `wall.exe` calls register/schedule has so far always been a fact about the
source or the build that the first pass did not look for. Check these before trying spellings:

- **A macro from the SDK header.** One line of the sibling's line table covering a whole loop body or several
  stores is a macro (`CLUSTERZERO` writes two 16-bit fields through one 32-bit store).
- **Statement boundaries.** The sibling's line table gives each statement its line: an expression the original
  computed in its own statement (`error_x = mx->x - 1.0f;`) compiles differently from the same expression folded
  into a sum.
- **The IR optimiser switched off.** Comparing a pointer of an aligned typedef (`RwMatrix*`) with `NULL` disables
  the IR optimiser for the function, and its loops then use an offset register where retail has a pointer
  induction variable; the original used a truth test (`if (*matrix)`). `build/tools/wdbg.exe` with the `iro_gate`
  spec shows which functions have it off.
- **Operand width and order.** A value widened before a shift (`(RwUInt32)frac << m`) moves the shift ahead of a
  load; compiling variants at `-O0` against the sibling's Debug build identifies the order.
- **A named local.** A named local is numbered before the compiler's temporaries, so it is coloured after an
  earlier load (`FMOD_MODE mode = mMode;` for a loop test). An empty lexical block in the sibling's DWARF and spare
  source lines between two statements are the evidence for one.
- **Declaration order of a block's locals.** The sibling's DWARF lists a block's locals in reverse declaration order.
  Declaring a macro's or a block's locals in the true order (`_l`, `_r`, `_elementSize`, then `t0`, `t1` in
  `p2resort.c`'s swap macro) made `_repartition` exact and moved a second function onto retail's registers.
- **A hand-unrolled copy loop.** It is usually a mis-spelled plain loop: `FMOD_memmove`'s 8-byte counter-register
  blocks come from two plain `while ( count-- )` loops that the compiler unrolls itself.
- **Inlining chains.** A callee is auto-inlined only when it is small in the compiler's measure: the Debug build
  shows what it called (Create clearing through Destroy), and that inner inline is what makes the outer one fit
  while its out-of-line copy is kept.
- **A const-qualified read.** A load retail repeats where the build shares it (a list node's next link read
  before `removeNode`, which reads it again) is a read through a pointer to const: a const read is its own
  expression to the common-subexpression pass (`docs/compiler_regalloc.md`). `EventEnvelopeDef::release` reads
  the link through `const EventEnvelopePoint *point`. `DSPResampler::execute` shows the same shape: retail reloads
  `mNoDMA` after the loop-flag test in its position-wrap tails, and reading it through a pointer to const
  `DSPResampler` gives that, but no spelling for such a read is evidenced, so the function stays parked until ruled.
- **An inline helper that tests its own argument.** A load hoisted above a null test (`mSystem->mDownmix` before
  `if (outchannels)`) is the `this` of an inlined member that does the test itself
  (`Downmix::getOutputChannels(outchannels)`); the sibling's line table puts load, test and store on one line.
- **A named local the sibling lacks.** Two callee-saved registers swapped between a flag and a list read
  (`getAsyncThread`) closed with a named `current` local for the list's first node.
- **The compiler version.** `build/tools/flagsweep.exe <unit> -v all` on a parked unit: FMOD needed `Wii/1.0a`
  (an aggregate copy kept inside a loop, where 4.3 hoists it), which the units matched until then could not tell.
- **A missing argument to an error macro.** `RWERROR((E_RW_NOMEM, size))` takes the size as a second argument; the
  call without it shows as two swapped registers (`p2heap.c`).
- **A static the Final map lacks.** A function in Gormiti's Debug map that the Final map does not list was inlined
  in retail: restore it as a static with `// no copy in retail (inlined)` instead of open-coding it
  (`p2heap.c`, `native.c`'s `_rpNativeSize`).
- **A loop test that stores and reloads.** In the Debug build a test whose value goes to a stack slot and is
  reloaded is a comma expression (`while (p -= elementSize, *key(p) > keyToPlace)` in `_insertionsort`).
- **A dead branch is real source folded away.** A macro of the form `(!cond ? TRUE : f(x))` (`RxHeapReset`) or a
  debug-only check leaves no branch in retail but one in the source. A tiny function retail does not inline is a
  routine whose body is assertions and loops that compile to nothing (`_rtSkinSplitMeshBoneRLEValidate`).
- **A `||` range test.** On a value held in a variable it becomes one unsigned compare; it stays two signed
  compares only when both operands are memory loads (`CodecDSP::setPositionCallback`).
- **Game code: ids and accessors.** A comparison against an event id goes through the id's conversion operator
  (`msg.record == lbl_X`, not `.record == X.record`), and a field compared on both sides through an inline accessor
  (`GetZone() == player->GetZone()`) gives retail's load order.
- **Things outside the function.** Vtables are emitted in reverse order of class declaration; uninitialised small
  data is laid out in reverse order of definition (`rppvs.c` defines its two counters the higher address first); an
  out-of-line copy of a function retail only has inlined is in the sibling's map as `UNUSED` and must exist in the
  unit; a float constant's position in the pool follows the order of first use, so a pool that differs from retail's
  (`gatecheck.exe layout <unit> --section .sdata2` shows it) points at the order of the source statements
  (`unk_8000A040::fn_8000A76C` needs the x line before the y line). Run `build/tools/gatecheck.exe layout <unit>`
  before a flip.
- **An inline that returns from inside its test.** An inline helper that returns its callee's result from within its
  own test leaves a second branch where its expansion ends a then-branch (`PostRegisteredMsg`, which `CDrawingTest`
  and `CScalingObject` call). A redundant `else if ( flag )` after an `if ( !flag )` is an original quirk that
  gives retail's `beq cr1`; keep it and label it (`CDrawingTest::fn_801796F8`).
- **A debug-only guard or computation the optimiser folds away.** The Debug build's statements shape the code even
  when nothing of them survives: the search in `RpPVSSetViewPosition` runs under `if ( InSector == -1 )`, a value is
  computed and never read (`RwV3dSubMacro` into `v` in `PVSCopyAndTagSectors`, which also works the diagonal shift out
  again from the box), a RenderWare stream write passes every write's byte count through one block local
  (`RwUInt32 size`, the data arrays included: `DMorphGeometryStreamWrite`), and a pointer's type and statement order
  together fix the code (`ClimaxS1DataStreamRead` walks the block as `char* data` and `temp`, stepping `temp` past the
  header before `imax` is set). Write the Debug build's form, labelled as the original's quirk.
- **A clamp inside a call's argument.** `memcpy( dst, src, a > b ? b : a )` compiles differently from the same clamp
  written as a statement before the call (`FMOD_OS_Memory_Realloc`).
- **A static that an unwritten neighbour references.** A variable that a function still linked from retail's assembly
  uses must stay global until that function is written (`FMOD_OS_GC_TempBufMem`, freed by the Revolution output
  unit), or the link breaks.
- **A forward declaration with the wrong keyword.** Generated stub headers declare RenderWare types as `class X;`
  where RenderWare declares `struct X`, an R1 finding that turns a flip red (`scalekey.h`, `RwStream`): include the
  real header (`rwsdk/plcore/bastream.h`) or use the right keyword.

A state that matches only through a carrier (reading a parameter through a pointer to itself, a dummy local, a
pointless assignment) is rejected and recorded in the function's header comment and `docs/exceptions.md`: it shows
what the compiler is sensitive to, and the real cause is still open. The permuter runs only after the checks above
and compiler tracing have failed (AGENTS.md, Workflow step 3).

## Routing

| work | model |
|---|---|
| scripted steps (flips of all-exact units, merges, sweeps, index refresh) | none: the orchestrator runs the tool |
| functions under 0x40, twin drafts that grade PARTIAL | Sonnet 5.5 High (Haiku 5.5 Max only for reading large files) |
| a unit of ordinary functions (0x40 to 0x3FF) | Sonnet 5.5 High |
| a library's first unit (unknown compiler profile), functions of 0x400 and over, a diagnosed wall | Opus 5.5 |
| a unit under 2 KB with sibling evidence, made of near-identical thin wrappers | Haiku 5.5 Max alone: the orchestrator pre-creates the unit, the worker solves one wrapper first and writes the rest in its form; 90k context budget (`fmod_channel`: 36 wrappers, all exact, linked) |
| a unit whose original is public-domain or otherwise public source (dlmalloc) | Sonnet 5.5 High, with that source as the starting text |
| any other unit under 2 KB with sibling evidence | Sonnet 5.5 High directly; a Haiku draft followed by a Sonnet pass is not used, because the Sonnet pass costs as much as a Sonnet-only lane |

Escalate a function to the next model only after it is parked with a wall class recorded. Parked units one or two
functions short go to an Opus 5.5 wall pass before new small units are started. A wall pass pays most on third-party
units, where Gormiti's Debug build names the original's quirks (the Walls checklist above); on game code, which has no
sibling, a pass has closed few functions, so it is kept to units with a recorded wall class.

## Stop rules

- At most eight compile-and-compare iterations on one function, then classify it with `build/tools/wall.exe` and
  park it with the class in its header comment.
- A register/schedule wall is parked at once. No permuter during bulk matching; parked walls are revisited in one
  pass, `build/tools/levers.exe --batch` first.
- A worker stops at a clean commit when its context passes about 120k tokens (150k for Opus) and reports what is
  left.
- A lane that stopped mid-work is taken over by a new agent from the slot's state: the uncommitted files are committed
  as `wip: lane state at stop`, the branch is rebased onto `main`, and the new agent gets the original brief plus
  what changed on `main` since. The takeover finishes the unit's work in most cases.
- A lane does not delete the generated placeholder of a function it has not written.
- A unit that is 100% but red at the gate is parked with the gate's attribution in `docs/exceptions.md`; the
  worker does not distort source to move data.

## Integration

- The orchestrator runs the slot's own `build/tools/merge_lane.exe --slot <slot> --branch <lane> --onto main`,
  (its own `merge.log` under `E:/shsm-wt/merge_lane_logs/<slot>/` keeps the whole output; redirect nothing into the slot's `build/`, the clean gate empties it), then
  `--update --merge --reviewed` after the reader-test review. Its changes step (and the gate's regressions step) blocks
  only an edited function whose score fell: a function whose address left its unit's range through the lane's resplit
  (not exact in the baseline) and a function whose generated placeholder the lane removed (it was not yet written) are
  listed in their own groups and do not block.
- Several lanes are merged in one sitting; after the last one, in the main checkout: `git merge --ff-only main`,
  `build/tools/configure.exe`, `tools/bin/ninja.exe`, `build/tools/classes.exe scan`,
  `build/tools/draft.exe sweep`, then `build/tools/pick.exe` for the next lanes.
- Stale `.s` files left in `build/R5WEA4/asm` by an older split corrupt the asm index and break
  `classes.exe scan`. If the index counts more `.s` files than `configure.py` has units, remove `build/R5WEA4`
  (never `build/tools`) and rebuild.

## Cost record

One row per finished worker in `build/cost/lanes.tsv` (not committed): date, lane, kind, model, effort, tokens
(from the worker's completion notice), tool uses, minutes, functions banked, bytes banked, walls parked, note.
Lane order follows tokens per banked byte, measured per library and model; game code is the default when the
figures are close.
