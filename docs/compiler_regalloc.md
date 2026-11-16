# MWCC register allocation and scheduling: what is established

How `mwcceppc` (CodeWarrior for Wii, `Wii/1.0`, 4.3 build 145) turns virtual registers into physical ones, and
which source changes move the result. Use it to diagnose register-numbering walls before reaching for the
permuter. Addresses are virtual addresses in `tools/compilers/Wii/1.0/mwcceppc.exe` (image base 0x400000),
read with `build/tools/pe_disasm.exe`. Experiments are reproducible with `build/tools/cc.exe`.

Function names beside the addresses come from the compiler's own NB11 CodeView debug info (3,680 named functions
with their `.obj` module; file-static functions have no name and are written `static in <Module>.obj`). Look
addresses up with `build/tools/mwcc_symbols.exe --addr 0x5b88f0` (or `--name colorinstructions`; `--dump`
writes `build/mwcc_symbols/<version>.tsv`); `pe_disasm.exe` and `wdbg.exe` (`D.sym`) print the same names. Wii
1.0RC1, 1.0a, 1.0, 1.1, 1.3, 1.5, 1.6, 1.7 and 0x4201_127 carry NB11 (addresses differ per version); the GC builds
have no debug directory.

Reading reference: doldecomp/mweppc (CC0) has matching decompiled C for the GC 2.x versions of the back end
(`Coloring.c`, `Scheduler.c`, `InterferenceGraph.c`, `SpillCode.c`, `Peephole.c`, `RegisterInfo.c`, and others).
Its front end is 2.4.7 against our 4.3, so use it to read the allocator and scheduler logic, not as a byte-level
reference. The names of the file-local helpers there (simplify, select, coalesce and similar statics) are the
ones to check against the `static in <Module>.obj` entries above.

Status labels: **read** (seen in the disassembly), **measured** (compiler experiment, A/B),
**inferred** (consistent with both but not read directly).

## Pipeline of the global allocator (read)

Per register class (0 SPR, 1 CR field, 2 VR, 3 FPR, 4 GPR; class byte at `[0x794146]` (`coloring_class`)), the driver at `0x5b88f0` (colorinstructions, Coloring.obj)
repeats, at most 10 times (assert in `Coloring.c`):

1. build the interference graph (`0x63d720` (buildinterferencegraph, InterferenceGraph.obj), InterferenceGraph.c), with coalescing (`0x63dc60` (static in InterferenceGraph.obj)) until a pass
   merges nothing;
2. simplify (`0x5b8dc0` (static in Coloring.obj; simplify));
3. select (`0x5b8bf0` (static in Coloring.obj; select), with the register picker `0x592bf0` (pickcolor, RegisterInfo.obj));
4. on success rewrite the operands (`0x5b8a90` (static in Coloring.obj; operand rewrite)); on failure insert spill code (`0x63e9d0` (insertspillcode, Spill.obj), Spill.c) and retry.

Graph nodes are 32 bytes (`[0x77356c] + 32*index`): spill cost at +0x0c, colour at +0x12 (-1 none), flags at
+0x14, degree at +0x16, an edge tree at +0x1c. **Nodes 0..n-1 are the physical registers; virtual registers
follow in their creation order.** The node index is the only ordering the allocator uses.

### Coalescing (read)

- Copies are visited in program order, block by block. Both operands are resolved through a union-find array
  (`[0x7378f0]`).
- A copy is skipped when the two sides interfere (`0x63e780` (interferes, InterferenceGraph.obj)) or a pair check rejects them (`0x594640` (machinespecific_coalesce_restriction, RegisterInfo.obj)).
- On a merge **the lower index survives** (`alias[hi] = lo`). Physical registers have the lowest indices, so a
  variable coalesced with an argument register *becomes* that register (precoloured).
- After any merge the graph is rebuilt and coalescing repeats.

### Simplify (read)

- K is the number of allocatable registers of the class, volatile and nonvolatile together.
- Each sweep visits the virtual nodes in **ascending index order**. Every node with degree < K is pushed and
  its neighbours' degrees drop. Sweeps repeat until one pushes nothing.
- When no node has degree < K, the node with the smallest `degree / spill cost` is pushed optimistically
  (Briggs style). Ties go to the first node in the high-degree list.

### Select (read)

- Nodes are popped LIFO. In the common case where the first sweep pushes everything, that means **highest
  index coloured first**.
- The picker takes the **lowest-numbered free register** of the class's normal list. For GPRs that is the
  volatile set in ascending order: r0 (where the instruction allows it; not as a base register), then r3, r4,
  and so on up to r12. Neighbours' colours are excluded.
- When nothing in the normal list is free (interference with call-clobbered registers, inferred), the next
  register from the reserve list is added: **r31, then r30, and so on**. Added registers stay available for
  later nodes, and the lowest free one wins.
- There is no copy bias and no register hint: the picker never looks at a related node's colour.

## How source maps to node indices (measured)

Each line was checked with `cc.exe` on probes under `build/tmp/` (examples are given inline).

- **Locals get virtual registers in reverse declaration order**, after the parameters, so the last-declared
  local has the lowest index and the first-declared the highest.
  - Across a call, with `int a, b, c;` the first-declared `a` is coloured first and gets r31, then b r30,
    c r29, whatever the order of first use (`call_abc`, `call_cba`, `call_abc_usecba`).
  - With no call, a loop that keeps `a`..`f` live gives `a` = r6 up to `f` = r11. Reverse numbering pops `a`
    first, and lowest-free gives it the lowest free register.
- **Compiler temporaries** (expression results, call results) are created later and get higher indices, so
  they are coloured before the named locals and take the lowest registers (r0, r3, ...).
- **A local initialised from a call result** (`a = k(x)`) is coalesced with the result temporary. It then
  colours like a late node: `a` gets r31 whether it is declared before or after `b` (`tmpexpr`, `tmpexpr2`).
- **A parameter that is never reassigned** is coalesced with its argument register (`pnomod`: `p` stays in r3
  through a loop with `p++`).
- **A parameter whose first statement reassigns it from itself** (`p = p + n;`, also `p += n`, also through
  an lvalue cast `(char *)p = (char *)p + n`) is *not* coalesced. The entry copy from the argument register is
  propagated into the add and disappears, so the variable is an ordinary low-index node.
  - It is coloured late, typically getting the highest volatile register in use (e.g. `pmod`: `p` = r8 while
    the locals take r3, r6, r7).
  - A preceding cast self-assignment `(char *)p = (char *)p;` keeps the copy and makes `p` stay in its
    argument register (`pmod_castself`).
- **Statement order of independent definitions changes interference.** `d = d + n; s = s + n;` gives
  `d` = r6, `s` = r3, whereas `s = s + n; d = d + n;` gives `s` = r4, `d` = r3: a value defined while another
  argument register is still live cannot take that register (`p_mod`, `p_mod_ds`).
- `-opt nolifetimes` changes some scratch choices but not the parameter cases above.

Diagnosis recipe for a numbering wall:

1. List each variable's register in retail and in the build.
2. Sort them by register to recover the colouring order: lowest volatile = coloured first; r31 = first
   callee-saved.
3. Compare that order with the declaration order (reversed), the order temporaries are created, and parameter
   coalescing. Two swapped callee-saved registers usually mean two swapped declarations, or a local that is (or
   is not) initialised straight from a call result.

## Scheduler (read and measured)

Traced in `Wii/1.0` with a ctypes debugger that runs `mwcceppc` under breakpoints and dumps the scheduler's
nodes (`build/tmp/sched/dbg.py`, `tracer.py`; sample dumps `trace0.txt`, `trace1.txt`; scratch, not committed).

- **Driver** `0x5bb420` (scheduleinstructions, Scheduler.obj) (assert string `Scheduler.c`). It runs once per basic block, twice per function: pass 1
  before register allocation (virtual registers; flag `[0x770c84]` (`coloring`) = 1) and pass 2 after it (physical registers).
  `-opt noschedule` disables both (that listing shows the scheduler's input order).
- **Machine model** is picked by the processor byte `[0x778ea4]` (`-proc`). `gekko`, `750` and `7400` use the table
  at `0x70335e` (data: machine750): two integer units and a 6-entry completion queue. `603e`, `604e`, `601`, `generic` and `403` use
  other tables and schedule differently.
- **Node build** `0x5bb600` (static in Scheduler.obj), per block: dependence edges (`0x5bb980` (static in Scheduler.obj), `0x5bbfc0` (static in Scheduler.obj), `0x5bbbd0` (static in Scheduler.obj)), then each node's
  height (longest latency path to the block end) and slack.
- **List scheduling** `0x5bb6f0` (static in Scheduler.obj)-`0x5bb7c3` (static in Scheduler.obj): forward, cycle by cycle; ready nodes are those whose predecessors
  have issued and whose operands are available.
- **Selection** `0x5bb7e0` (static in Scheduler.obj). The first ready node in program order is the incumbent. A later candidate replaces it
  only if it is strictly better by the first key that differs:
  1. critical (slack <= current cycle) beats non-critical;
  2. more successors whose only remaining predecessor is this node;
  3. greater height;
  4. pass 1 only: the smaller opcode-priority byte at `0x6f06cb + 22*opcode` (`li` 4, `lis` 4, `addi` 2,
     `mtctr` 1, `lwz` 3, `mr` 0, `b` 0);
  5. otherwise the incumbent stays (program order).
- In pass 2 ties are broken by program order alone.

Consequences for matching:

- Two independent instructions with equal height and fan-out are ordered by key 4 in pass 1, not by their
  source order: swapping the source order of `i = 0` and `p = table` does not swap `li` and `addi`.
- Anything that adds or removes a node in the block (even a dead copy that is deleted later) changes heights and
  fan-out counts and can reorder the survivors. See the first-function quirk below.

## Scheduler memory disambiguation (read and measured)

Whether the scheduler may move a load past a store (or two stores past each other) is decided per pair by
`may_alias` (`0x5c3460`, Alias.obj). The scheduler's dependence builders (statics at `0x5bbb56`, `0x5bbc06`,
`0x5bbc56` call sites, Scheduler.obj) ask it for every pair of memory instructions in a block; local CSE's
`killmemory` (LocalCSE.obj) asks it too, so the same answer decides whether a stored value is reused or reloaded
after a later store. `is_dependent` (`0x5bb390`) is the register test, not the memory one.

- `may_alias(a, b)` compares the two instructions' alias records (PCode `+0x1c`) with the static comparator at
  `0x5c3480`. Alias record: `+0x14` object, `+0x18` offset, `+0x1c` size, `+0x30` kind (0 object, 1 object + offset
  + size, 2 set with a bit vector at `+0x28`). Two locations in the same object alias only if their ranges overlap;
  different objects alias only through a set they both belong to. The worst-case set (`[0x7776c4]`) holds every
  object whose address may be taken through a pointer, globals included.
- Pointer parameters get their alias record when the incoming argument is copied to its home register
  (`move_assigned_argument` `0x46dc30`, CodeGen.obj, calls `make_distinct_virtual_alias` `0x5c45e0` for a pointer or
  reference parameter and attaches the record to that `mr`; `propagatealiasinfo` `0x66bf30` then carries it to every
  load and store based on that register, through casts and copies into locals). `make_distinct_virtual_alias`
  makes a fresh virtual object for the pointee and:
  1. `restrict` parameter (qualifier bit `0x200000`): the object stands alone;
  2. pointer to const (`CParser_IsReadOnly` on the pointee; a non-POD class pointee excepted): the object stands alone
     and is marked read-only (object `+0x14` = 1);
  3. any other pointer: the object is added to `create_alias_set(0, pointee type)`, which is the worst-case set unless
     type-based aliasing is on (flags `[0x779070]`/`[0x77906d]` with `[0x779071]`), so it may alias every global.
- Measured with `tools/c/tools/wdbg/specs/may_alias.wdbg` on `static S dst; void f(const S* p) { dst = *p; }` and the
  same with `S* p`: every `*p` / `dst` pair answers 0 for the const parameter and 1 for the plain one. The struct
  copy then comes out as all loads first, then all stores (const) versus load-load-store-store pairs (plain).
- What decides is the declared type of the parameter itself. A const or restrict local initialised from a plain
  parameter (measured: the local is copy-propagated and gets the parameter's record, so only the parameter's
  type counts), a const cast at the use or `memcpy` do not change the answer for a plain parameter. Stores through a
  cast-away pointer to a const parameter keep the read-only object's record (inferred: `GXLoadTexObjPreLoaded` and
  `GXLoadTlut` write through it and match only with the const signature).
- `#pragma opt_pointer_analysis on` (IR pass `IRO_AnalyzePointers` `0x4ac490`, IroPointerAnalysis.obj, flag
  `[0x779121]`; modes `opt_pointer_analysis_mode addr|ansi|type`) is not the cause: it crashes `Wii/1.0` on any
  function that reads through a pointer into a global (`int g; void f(int* p) { g = *p; }`): `LocationSetSet_Find`
  passes a null type to `IRO_TypesEqual2` and `is_typeequal` faults, in every mode and at every optimisation level.

Case closed: the five parked RVL SDK GX walls (`GXSetCPUFifo`, `GXSetGPFifo`, `GXLoadTexObjPreLoaded`,
`GXLoadTlut`, `GXInvalidateTexRegion`), where retail moved accesses through the object parameter past stores to a
static, the FIFO port, another pointer or the stack. The 2009 SDK declares those parameters const (mscharged-decomp's
R4QK01 build and Petari do); with the const signatures all five are exact (docs/exceptions.md, "GX rulings").
Diagnosis rule: retail hoisting loads through a pointer parameter above stores to other memory where the build keeps
source order points at a const (or restrict) parameter in the original signature.

## Cross-function state

### The first-function quirk (read and measured)

In the first function of a translation unit that reaches the copy-propagation pass, the entry copy of the first
parameter (`mr r32, r3`, pcode node 0) is substituted into its uses but not deleted. The dead copy survives into
scheduler pass 1 and changes the selection in its block. The dead-instruction check after pass 1 (`0x5b8300` (peepholeoptimizepcode, Peephole.obj)) then
deletes it, so the final listing shows only the changed order.

Mechanism (read in the disassembly, confirmed by patching memory under the debugger):

- The O4 optimiser `0x5b7f70` (static in GlobalOptimizer.obj) runs copy propagation through the generic dataflow driver `0x6497c0(fn, 1)`
  (`0x66af80` (propagateinstructions, Propagation.obj); callsite `0x5b7ff3` (static in GlobalOptimizer.obj)/`0x5b8004` (static in GlobalOptimizer.obj)). The flag-1 descriptor at `0x6fe8a4` holds three callbacks:
  filter `0x649b00` (static in CopyPropagation.obj), per-use predicate `0x649840` (static in CopyPropagation.obj) and per-node action `0x649800` (static in CopyPropagation.obj).
- The predicate substitutes the copy's source into one use. At `0x6498c4` (static in CopyPropagation.obj) it compares the node index with the
  global `[0x747944]` ("node in progress"). Only if they differ does it store the node there and set
  `[0x747940]` = 1 ("substitution done", `0x6498ea` (static in CopyPropagation.obj)/`0x6498f0` (static in CopyPropagation.obj)).
- The action deletes the copy (`0x59fcb0` (deletepcode, PCode.obj)) only if `[0x747944]` == node and `[0x747940]` != 0. It then sets
  `[0x747944]` = -1 unconditionally (`0x649834` (static in CopyPropagation.obj)).
- Both globals are statically 0 in `.data`, and nothing else writes them (xrefs: only `0x649800`, `0x649840`,
  `0x649910` (static in CopyPropagation.obj)). So in the first function `[0x747944]` already equals node 0, `[0x747940]` stays 0, and the copy
  survives. The first action resets `[0x747944]` to -1, and the rest of the compiler process behaves normally.
- Patch tests: setting `[0x747944]` = -1 at the first function's entry (breakpoint `0x46bdac` (CodeGen_Generator+0xfc, CodeGen.obj)) gives retail's
  order for `__register_fragment`; setting it back to 0 at the entry of a second function brings the quirk back
  in that function.

What this means for source:

- It is one-shot per TU (one compiler process per file). It does not accumulate, and it does not depend on
  virtual-register numbering or any table. Functions that are not first are never affected.
- A preceding function cures it only if it is really compiled, goes through the optimiser, and has a copy whose
  use is substituted: `int f(int a) { return a + 1; }`, an unused `static` function with a used parameter, an
  inline function whose address is taken, an explicit template instantiation, or a class whose vtable (and so
  its inline virtual members) is emitted. Functions without parameters, with an unused parameter or a constant
  return, `asm` functions, unused `inline` functions and static data initialisers do not (measured with
  `cc.exe --cmp`; probes in `build/tmp/w2trace/variants.py`).
- Later functions in the file cannot help: code generation follows source order. `-inline deferred` also cures it,
  but only because it generates code in reverse order (and emits the functions reversed). Every matched runtime
  unit is emitted in source order, so it is not the original setting.
- All nine Wii compiler versions in `tools/compilers/Wii/` (0x4201_127 to 1.7) behave the same.

Case: `__register_fragment` (`Gecko_ExceptionPPC.cp`, retail `0x801A7044`) is the first function of its file. With
the quirk the build emits `li r6,0` before `addi r5,r5,fragmentinfo@l`; retail has them the other way round.
Fourteen public decomps of this file (zeldaret/tp, ss and tww, DarkRTA/rb3, projectPiki/pikmin2, zeldaret/oot-vc
and others; copies in `build/ref/gecko/`) put `__register_fragment` first and all of the exception-handling
machinery (`ExPPC_*`, `__throw`, `__end__catch`) after `__unregister_fragment`, where it cannot affect it. The Wii
ones meet the same swap and cover it with `#pragma schedule once` (rb3, key, pokepark, xenoblade, rhf). The
original file therefore had something compiled before `__register_fragment` that no reference shows (a header,
a prefix file, a dead-stripped function). Until that construct is identified the unit stays parked: adding an
invented function would be a fakematch.

### Inlined locals: numbering depends on when the callee is expanded (read and measured)

Traced with `wdbg.exe` (specs `build/tmp/inl/dump_push.py`, `dump_ra.py`; scratch).

- Virtual registers are numbered by the loop at `0x46f140` (static in CodeGen.obj). It walks the parameter list `[0x772358]` (`arguments`), then the locals
  list `[0x7787cc]` (`locals`) from head to tail, creating each register with `0x5b6110` (assign_register_to_variable, Registers.obj)/`0x5951e0` (assign_register_by_type, RegisterInfo.obj) (RegisterInfo.c).
- `0x513f10` (CFunc_AppendLocal, CFunc.obj) pushes locals on the head of that list (callers `0x457ef0` (CParser_NewLocalDataObject+0x20, CParser.obj), `0x55eab0` (static in CInline.obj), `0x55ee10` (static in CInline.obj)), so the newest
  object gets the lowest index. Inliner-made objects are renamed `@NNN` from a global counter: a higher `@` number
  means a lower index.
- **Without `-ipa`**, inline expansion happens at code generation (`CInline.c`: driver `0x55d3e0` (static in CInline.obj), body walk
  `0x55d610` (static in CInline.obj), argument binder `0x55eab0`, statement walker `0x55df70` (static in CInline.obj)). An expansion first creates the return-value
  temporary (`0x457ed0` (CParser_NewLocalDataObject, CParser.obj)), then pushes the callee's locals (`0x55ed10` (static in CInline.obj)-`0x55ed23` (static in CInline.obj)), walking the callee's list from head
  to tail. That reverses declaration order, so the callee's first-declared local gets the lowest index. Nested callees
  are expanded lazily, from the original tree, at every call site.
- **With `-ipa file`**, inlining happens at parse time with callees that are already expanded. A callee that was itself
  expanded into another function is reversed twice, so its locals keep the normal order. Dead static functions that
  were inlined everywhere are not emitted.
- `-inline bottomup`/`nobottomup`, `#pragma inline_bottom_up` and `#pragma inline_depth` change which calls are
  inlined, not the numbering.

Case closed by this: `consume_markers` (libjpeg `jdinput.c`) inlines `start_input_pass`, which inlines
`latch_quant_tables`. Retail's standalone `start_input_pass` numbers the latch locals in one order and the copy
inside `consume_markers` in the other. Only `-ipa file` reproduces both, and it also drops the standalone
`latch_quant_tables`, which retail lacks. With the flag the whole unit is 100% from verbatim IJG source; it is neutral
on the other 25 libjpeg units (`cc.exe --cmp`). The RVL SDK units already need `-ipa file`; libjpeg now takes it library-wide (JPEGLib cflags).

Diagnosis rule: when a function that inlines static helpers has the helpers' locals coloured in the wrong order (or a
caller-side temporary such as an inlined call's result coloured before them), and the same helper matches standalone,
try `-ipa file` on the unit before touching the source. Check that the flag does not regress the unit's other
functions.

Case closed: MSL `alloc.c` `free` and `realloc`. With `__pool_free(get_malloc_pool(), ptr)` the pool pointer (the
return temporary of `get_malloc_pool()`, created before the inlined helper's locals) was coloured first; retail
colours it after them. zeldaret/tp's form, where the helper takes the opaque `__mem_pool*` and casts it to its own
`pool_obj` local, compiled with `-ipa file` (now MSL-wide), matches all 13 functions: the helper's local carries the
pool through the expansion.

### What is not cross-function state

TU composition was tested on the other walls: the functions the references have but retail lacks (MSL `alloc.c`:
`Block_construct`, `__msize`, `__pool_alloc_clear`, `__pool_free_all`, `__malloc_free_all`), removing the functions
after the wall function (`jdinput.c`, `jmemmgr.c`), and reordering definitions. None changes a wall. Apart from the
first-function quirk, no state carried from one function to the next has been found.

### Other results on these walls

- MSL `mem_funcs.c` `__copy_longs_rev_unaligned` (lost entry copy of `src`) was not cross-function state. It closed
  with the Wii branch (`#if !PLATFORM_GCN`) of the zeldaret/ss reference: two `cps = (unsigned char*)lps;
  cpd = (unsigned char*)lpd;` resyncs whose lvalue-cast self-assignments keep the entry copy (see `pmod_castself`
  above). Check a reference's Wii-only branches before assuming a GameCube text is the original.
- MSL `alloc.c` `Block_link` closed with the ss macro forms of `SubBlock_size` and `SubBlock_set_free` (the macro
  declares its own `this_size`).
- libjpeg `jmemmgr.c` `alloc_small` (open): retail recomputes `min_request + slop` at its second use. Ours merges the
  two identical sums into one saved register. A permuter run (about 44k iterations) found that a cast on one operand
  of one sum (`(long) min_request + slop`) blocks the merge (2/114 rows left, symbol names only). IJG has no cast
  there, so the state is evidence only.

Permuter caveat: decomp-permuter's `extract_fn` turns every non-target function without `inline` into a prototype,
so static callees that MWCC auto-inlines become real calls and the scores are meaningless. Keep their bodies (the
scratch `build/tmp/permuter_run_keep.py` sets `KEEP_FNS`) and check that the permuter's base score equals `cc.exe`.

## Common subexpressions: qualifiers are part of the expression (read and measured)

Two reads of the same global in one block become one load only if the IR common-subexpression pass finds them equal.

- Pass order in the IR optimiser driver (`0x5921f0`, IRO_Optimizer+0x590, IrOptimizer.obj): `IRO_UseDef`,
  `IRO_GenerateTopLevelExprsForSubableOperands`, then `IRO_CommonSubs` (`0x633d90`, IroCSE.obj; it runs once),
  and only later `IRO_CopyAndConstantPropagation` (`0x610090`) and `IRO_ExpressionPropagation` (`0x60f8d0`).
- `IRO_CommonSubs` compares candidates with `IRO_ExprsSameSemantically` (`0x5d4440`), a thunk to the static
  comparator at `0x5d4460` (IroUtil.obj). Like `IRO_ExprsSame` (`0x5d4be0`) and `IRO_ExprsSameValue`, it first
  requires the same operator byte (`+0`), then the same flags dword (`+6`) under the mask `0xffffe7ff`, then
  `IRO_TypesEqual` on the result types (`+0x12`). The flags dword carries the expression's qualifiers, and the mask
  keeps `0x1` (const).
- Traced with `tools/c/tools/wdbg/specs/cse_quals.wdbg` on `extern int g; float a = (const int&)g; int b = g;`: the
  two load nodes compare with flags `0x0` and `0x1` (same operator, same operand) and are not merged.

So a read through a `const int` lvalue and a plain read of the same `int` are two expressions, and both loads
survive. Measured with `cc.exe` (one float and one int static initialised from `extern int g`):

| reads | loads |
|---|---|
| both plain, any order, `static`/`const`/class statics, casts, by-value inline accessors | 1 |
| one through `(const int&)g`, `*(const int*)&g`, an inline returning `const int&`, or a `const T&` parameter; the other plain | 2 |
| both through the same `const int&` accessor | 1 |
| one through an inline returning `int&` or `int*` (non-const) | 1 |
| `extern const int g` (every read const) | 1 |

Inlining order does not separate the reads: an `int&` accessor is inlined the same way and merges; only the const
qualifier does.

Case: the display-height pair that opens about 76 game static initialisers (`lwz lbl_8052861C` twice, first load
converted to float). Any one const-qualified read gives it exactly; the bytes cannot tell which spelling made the read
const. The source reads the float copy through the `const int&` accessor `GetDisplayHeight` (AGENTS.md,
"Display-height accessor"; docs/exceptions.md).

Case closed: the phone triggers' `unk_MessageInfo` (game `CPhoneMessageTrigger`, `CVoicemailMessageTrigger`).
Traced with `wdbg.exe` (breakpoints on `0x513f10` (CFunc_AppendLocal) with the caller's return address, and on `0x55eab0`).
The inlined constructor's five parameter objects are made by `0x55ee10` (return `0x55ef05`) in argument order, each
pushed on the head of the locals list, so the last parameter gets the lowest index. Called directly in the handler
(`fn( unk_MessageInfo( a, b, c, d, e ) )`, CAmbientTextMessage), that is the only push: forward registers (a r0, b r5 ..
e r8). In retail's form the constructor is first expanded at parse time (`-ipa file`) into an inline member,
`void SendTextMessage() { unk_MessageInfo info( a, b, c, d, e ); inbox.fn( info ); }`; expanding that member into the
handler copies its locals (`info` and the five parameter objects) with the loop at `0x55ed10`-`0x55ed23`, head to tail,
pushing each on the head again. The parameters' order is reversed once more: b r8 .. e r5 (a stays in r0). The named
local matters: the same member passing the temporary straight to the call also reverses the five, but moves `a` out of r0.
Diagnosis rule: inlined constructor or helper parameters numbered in reverse of a matched sibling's call site, with the
same loads and stores, point at one more inline layer (an inline member or function wrapping the construction).

## IR optimiser gate: aligned types (read and measured)

`IRO_Optimizer` (`0x591c60`, IrOptimizer.obj) first asks `IRO_CanThisIRBeOptimized` (`0x5e1940`) about the function's
statement list, after inlining, and stores the negated answer at `[0x793ee0]`; the IR passes (constant and copy
propagation, dead stores, `IRO_OptimizeSwitches` and the rest) run only when it is 0. `IRO_CanThisIRBeOptimized`
walks every statement's expression with the static checker `0x5e1a30`, which refuses the function when one node's
type qualifiers carry an alignment bit (`& 0x1f000000`; `__attribute__((aligned(4)))` gives `0x3000000`). The
qualifiers are `CParser_GetTypeQualifiers(node type, node qualifiers & 0x1f200003)` (`0x455b10`): for a pointer or
member-pointer type the pointer type's own qualifier field, otherwise the node's. An object reference is tested the
same way with the object's type and qualifiers (`0x5e1a55`).

With `typedef struct RwFrame __attribute__((aligned(4))) RwFrame;` (`include/rwsdk/baframe.h`), measured with
`tools/c/tools/wdbg/specs/iro_gate.wdbg` (one line per function: optimised or not, and the first node that refused):

| source form (`RwFrame* f`) | IR optimiser |
|---|---|
| `f != NULL`, `f == NULL`, `f < e`, `RpAtomicGetFrame(a) != NULL`, `gf() != NULL` (the comparison node, int-typed, carries the operand's alignment qualifier) | skipped |
| `f->root`, `f->child` (a pointer member read through `f`: the member's pointer type carries it) | skipped |
| an int member read through `f` (`RwFrameDirty(f)`: the node's qualifiers carry it) | skipped |
| `if (f)`, `if (!f)`, `RpAtomicGetFrame(a) ? ... : 0`, `if (RpAtomicGetFrame(a))` | runs |
| passing, assigning or returning `f`, a cast to `RwFrame*`, `RwFrameUpdateObjects(f)`, `&f->modelling` | runs |
| the same comparisons on `struct RwFrame*` (the tag, not the typedef) or on `rwObjectGetParent(a)` (`void*`) | runs |

So a function loses its IR optimiser by comparing a pointer to an aligned typedef or by reading a member through
one, never by merely naming, passing or truth-testing it. Inlined bodies count (the check runs after inlining):
baclump's `RpAtomicSetGeometry` written with `frame != NULL` turned the optimiser off in every function that inlines
it (`RpAtomicDestroy` and seven more), where retail folds `RpAtomicSetGeometry(atomic, NULL, 0)`; written
`if (frame && ...)`, the RenderWare SDK's spelling (its header macros test `RpAtomicGetFrame(_atomic)` for truth),
all 44 baclump functions match. `ClumpAtomicCloneAndAttach` stays unoptimised, as in retail, through `frame->root`.
Diagnosis rule: when one function of a unit that uses an aligned type keeps a fold its siblings make (or the
reverse), look for a comparison or member read of the aligned type in it or in what it inlines.

## Tools

- `build/tools/cc.exe <file> -u <unit> [-v Wii/1.x] [-f "<flag>"] [-s <fn>] [--cmp]` compiles a probe or a
  variant with the unit's exact flags (read from `build.ninja`). `--cmp` diffs it against the retail block.
- `build/tools/fdiff.exe <unit> <fn> [--all]` rebuilds a unit and shows retail and compiled side by side.
- `build/tools/pe_disasm.exe strings|xref|func|callers|callees <exe> <arg>` does static x86 disassembly of
  the compiler (`pe_disasm.exe`).
- `build/tools/permute.exe <unit> <fn> [--run]` sets up decomp-permuter (see docs/tools.md and docs/toolchain.md). For a C++ unit it strips the
  `extern "C"` wrappers and compiles base.c as C, which keeps the symbol of an `extern "C"` function.
- `build/tools/wdbg.exe <src> <unit> <outdir> <spec.wdbg>` runs mwcceppc under a
  debugger with the unit's flags. The spec file (`tools/c/tools/wdbg/SPEC.md` gives the format) maps breakpoint addresses to handlers that read or patch the
  compiler's memory; output in `<outdir>/log.txt`. Specs for the first-function
  quirk are in `tools/c/tools/wdbg/specs/`; `may_alias.wdbg` logs every memory-alias answer with both alias records;
  `iro_gate.wdbg` logs, per function, whether the IR optimiser runs and the first node that turned it off.
  `build/tools/wdbg.exe iro-scan` runs it over the non-exact functions of every NonMatching unit (or the named units) and flags
  a function whose IR optimiser is off while its siblings have it on; a spec carries a `build` line and refuses a compiler
  build whose addresses it does not know (FMOD builds with `Wii/1.0a`, the rest with `Wii/1.0`).
  A compiler crash is logged with its address, name and stack. The scheduler dumps came from the older
  `build/tmp/sched/dbg.py` (scratch).

The partial mwcceppc decompilation the user supplied (Wii 1.1; CABI, CMangler and the MS-DOS OS layer only) has
no back-end code. The allocator was located through its `Coloring.c`, `InterferenceGraph.c` and `Spill.c` assert
strings instead. The `dumpir`, `dumpphases` and similar pragma strings exist in the binary but produce no
output in this release build.
