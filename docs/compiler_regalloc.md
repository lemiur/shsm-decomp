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

Per register class (0 SPR, 1 CR field, 2 VR, 3 FPR, 4 GPR; class byte at `[0x794146]` (`coloring_class`)), the driver at
`0x5b88f0` (colorinstructions, Coloring.obj) skips a class whose used-register count `[0x778924 + 4*class]` equals its
physical count `[0x770b68 + 4*class]` (no virtual registers), then repeats, at most 10 times (attempt counter
`[0x770970]`, assert in `Coloring.c`):

1. build the interference graph (`0x63d720` (buildinterferencegraph, InterferenceGraph.obj)): the alias array
   (`[0x7378f0]`, one u16 per node, initialised to the identity), then in a loop the nodes (`0x63e3d0`, static), the
   edges (`0x63dfb0`, static) and coalescing (`0x63dc60`, static) until a pass merges nothing; then the variables
   (`0x63d930`, static), the coalescing marks (`0x63db20`, static) and the rematerialisation walk (`0x63d7e0`, static);
2. simplify (`0x5b8dc0` (static in Coloring.obj; simplify));
3. select (`0x5b8bf0` (static in Coloring.obj; select), with the register picker `0x592bf0` (pickcolor, RegisterInfo.obj));
4. on success rewrite the operands (`0x5b8a90` (static in Coloring.obj; operand rewrite)); on failure insert spill code
   (`0x63e9d0` (insertspillcode, Spill.obj)) and start again with attempt + 1.

`Wii/1.0a` (FMOD) has the same code, instruction for instruction: Coloring.obj at `-0x4950`, RegisterInfo.obj at
`-0x4530` and InterferenceGraph.obj at `-0x5030` from the `Wii/1.0` addresses (read); its data addresses differ (the
`RA_` keys of `tools/c/tools/mwtrace/mwt_build.c`). `mwtrace regalloc` (Tools, below) traces every step named here.

### Nodes (read; the degree copy measured)

32 bytes each at `[0x77356c] + 32*index`. **Nodes 0..n-1 are the physical registers of the class; virtual registers
follow in their creation order**, and the node index is the virtual register number.

| offset | field |
|---|---|
| +0x00 | link of simplify's stack and of its high-degree list |
| +0x04 | the `Object` the node stands for (set by `0x63d930` from each parameter's and local's VarInfo), 0 for a temporary |
| +0x08 | the single defining instruction when the value can be rematerialised (`0x63d7e0`, `is_location_independent`), else 0 |
| +0x0c | spill cost (0 until estimatespillcosts runs) |
| +0x10 | the node's own index |
| +0x12 | colour (-1 none; the physical root's index for a node coalesced into a physical register) |
| +0x14 | flags: 0x1 spilled (select), 0x2 removed by simplify, 0x4 coalesced into another node, 0x8 a coalescing root, 0x10 and 0x20 the two halves of a register-pair variable, 0x40 more than one definition (no rematerialisation), 0x80 and 0x400 set by estimatespillcosts from operand flags 0x40 and 0x10 (the node's spill ratio is then the constant at `0x6e88f4`, `0x7f7fffff` (FLT_MAX), or at `0x6e88f0`, `0x7effffff`), 0x200 a pair node (two colours; its edges count 2), 0x800 kept from rematerialisation (meaning not established), 0x4000 the VLE picker's restriction |
| +0x16 | degree (simplify decrements it) |
| +0x18 | equal to +0x16 when the graph is complete and not decremented by simplify (measured; what reads it is not established) |
| +0x1a | the sort key of `machine_specific_sort_graph` (VLE only) |
| +0x1c | the node's edge list: edge records with the endpoints at +0x0c (lower) and +0x0e (higher), the next edge of the lower endpoint at +0x04 and of the higher at +0x08 |

The variables walk (`0x63d930`) reads, per parameter (list `[0x772358]`) and local (list `[0x7787cc]`), the VarInfo
(`Registers_GetVarInfo` `0x594f00`: +0x0e flags, 0x2 in a register, 0x4 a register pair; +0x0f class; +0x10 virtual
register; +0x12 the pair's second register) and stores the Object at node +0x04; the objects of `[0x7722e4]` are walked
the same way.

### Coalescing (read)

- Copies (instruction flag 0x10, class byte +0x3b, operand registers +0x30 and +0x3e) are visited in program order,
  block by block. Both operands are resolved to their roots through the alias array.
- Equal roots: the copy is deleted. Otherwise the copy is kept when the roots interfere (`0x63e780` (interferes,
  InterferenceGraph.obj)), when `0x594640` (machinespecific_coalesce_restriction, RegisterInfo.obj) refuses the pair,
  or when the higher root is a member of a sparse set (sparse array `[0x737900]`, dense `[0x7378fc]`, count
  `[0x7378f8]`; who fills it and what it stands for is not established).
- A merge stores `alias[hi] = lo` (`0x63ddd5`): **the lower index survives**. The higher node's edges are added to the
  lower (`makeinterfere` `0x63e810`) and the copy is deleted. Physical registers have the lowest indices, so a variable
  coalesced with an argument register *becomes* that register (colour = the register, flag 0x4, set by `0x63db20`).
- After any merge the graph is rebuilt and coalescing repeats.
- `0x63db20` then marks every coalesced node (0x4) and its root (0x8); a root without an Object takes the coalesced
  node's (`0x63dc43`), so a temporary root can carry a variable's Object.

### Simplify (read; the choice measured)

- K = `available_registers(class)` (`0x5b6030`): GPR 29, FPR 32, CR 7 (measured).
- Each sweep visits the virtual nodes not yet removed or coalesced (flags 0x6) in **ascending index order**. A node
  with degree < K is pushed and its neighbours' degrees drop by one (two for a pair). The drop is immediate, so a
  higher-index node can still be pushed in the same sweep. A node with degree >= K goes on the high-degree list (at its
  head: the list runs from the highest index down). Sweeps repeat until one pushes nothing.
- `machine_specific_sort_graph` (`0x592b00`) re-sorts the stack by node +0x1a only for GPRs with the VLE byte
  `[0x778e9b]` set; the byte is 0 in every traced compile (measured), so the stack is the push order.
- When the high-degree list is not empty, `estimatespillcosts` (`0x63f3a0`, Spill.obj) runs once: for each operand of
  the class in a block of weight w (block +0x24; 1 when `[0x779112]` is set), a use adds 2w (w when the node can be
  rematerialised or `[0x779112]` is set) and a definition adds w (subtracts w when the node can be rematerialised or
  the instruction's flags hold 0x8002).
- Then the node with the **smallest spill cost / degree** is pushed optimistically (read: `fild degree; fild cost;
  fdivrp`; measured on `DSPResampler::execute`, where node 36 at 3/31 was chosen over node 32 at 6641/31). A tie keeps
  the first candidate of the list, the highest index (measured, the same case: 36 before 34, both 3/31). Sweeps then
  resume.

### Select (read)

- Nodes are popped LIFO: the last pushed first. When the first sweep pushes everything, that means **highest index
  coloured first**; the nodes of a later sweep are coloured before every node of an earlier one.
- The start mask is `volatile_registers(class)` (`0x5b5fe0`): GPR r0 r3-r12, FPR f0-f13, CR cr0-cr1 cr6-cr7
  (measured). The colour of every neighbour that has one (a physical register's index, a coloured node's colour; two
  bits for a pair) is cleared from it.
- The picker takes the **lowest set bit** (`0x592bf0`; the VLE restriction applies only with `[0x778e9b]`). r0 is not
  taken where an instruction cannot use it: such a node has an edge to physical node 0 (measured: base-register
  temporaries interfere with r0).
- When nothing is free, `obtain_nonvolatile_register` (`0x5b5e40`) hands out the class's next nonvolatile register
  (the per-class list at `0x76f910`, counter `[0x7786f8 + 4*class]`, skipping reserved ones in `[0x76fbd0]`): **r31,
  then r30, and so on**. It joins the mask for the rest of select and the node is tried again. The nonvolatile registers
  handed out are the ones the prologue saves (measured: the self-check of every traced case).
- When none is left, the node is marked spilled (0x1), select returns 0 and spill code follows (`DSPResampler::execute`:
  one GPR spill, colouring at attempt 1).
- There is no copy bias and no register hint: the picker never looks at a related node's colour.

### Operand rewrite and the -sym on DWARF (read and measured)

`0x5b8a90` gives every register operand of the class its node's colour, then, per node with an Object, not spilled,
whose `[Object +0x40] +0x09` byte is set, writes the colour into the VarInfo (+0x10; +0x12 for a pair's second half).
`-sym on` writes the VarInfo register into the DWARF. A variable whose byte is clear keeps its virtual register number,
which the DWARF records as a DWARF register number (`DSPResampler::execute`: `outbuffer`, virtual 34, is printed `f2`,
`outchannels`, 36, `f4`). The byte was set when the variables were walked and is clear at the rewrite; what clears it
is not established. A variable whose node is dead (degree 0: its uses were copy-propagated into temporaries) is
coloured r0 or f0, and the DWARF says so.

## How source maps to node indices (measured)

Each line was checked with `cc.exe` probes under `build/tmp/` and with `mwtrace regalloc` (cases named inline).

- **Locals get virtual registers in reverse declaration order**, after the parameters, so the last-declared
  local has the lowest index and the first-declared the highest (the numbering walk `0x46f140`, below).
  - Across a call, with `int a, b, c;` the first-declared `a` is coloured first and gets r31, then b r30,
    c r29, whatever the order of first use (`call_abc`, `call_cba`, `call_abc_usecba`); the parameter `x` (a lower
    index) gets r28.
  - `WorldSyncCamera`: plpPlanes, cameraExt, goBackToFront, nStack, position, spSect (declaration order) are nodes
    45..40 and get r31..r26; declared after `spaStack`, spSect is node 45 and r31, and the other five move one down.
  - With no call, a loop that keeps `a`..`f` live gives `a` = r6 up to `f` = r11. Reverse numbering pops `a`
    first, and lowest-free gives it the lowest free register (`_rwFreeListFreeReal`: heap at function scope is node
    40, coloured first, r5; declared in the while block it is node 36, coloured third, r7).
- **That order decides only among the nodes simplify removes in the same sweep.** Select colours the nodes of a later
  sweep first, so a node whose degree stays >= K through the first sweep is coloured before every node removed in it,
  whatever its index or declaration:
  - `CPsyPointOfInterest::fn_801A73A8`: `this` (node 32, the lowest index) has degree 49, is pushed in sweep 2 and is
    coloured first: r31. Every other node is removed in sweep 1.
  - `MemPool::set`: the four parameters and three of the four locals (nodes 32-36, 38, 39; degrees 37 to 72) are
    pushed in sweep 2 and take r25-r31 by index; the local `count` (node 37, degree 13) is removed in sweep 1 and gets
    r0, and the compiler-made node 40 (degree 24, sweep 1, live across a call) gets r24.
  - Which nodes reach a later sweep follows from their degree (how many values are live at the same time) and from the
    drops the earlier pushes of the sweep give, so one more live value can move a node between sweeps.
- **Compiler temporaries** (expression results, call results) are created later and get higher indices, so
  in the same sweep they are coloured before the named locals and take the lowest registers (r0, r3, ...).
- **A value copy-propagated out of a variable lives in a temporary**: the variable's own node is dead (degree 0) and a
  temporary carries the value and its register. A probe of the `tmpexpr` shape (`int a = k(x)` used across later calls): `a` is node 33,
  degree 0, r0 (also in its DWARF); the value is temporary 37, r31. This is what was described as "a local initialised
  from a call result is coalesced with the result temporary" (`tmpexpr`, `tmpexpr2`): no merge happens, the variable
  is replaced.
- **Inlined callees' objects and compiler-made objects** are locals too. The inliner renames a callee's locals and
  parameter objects to `@NNN` (`Wii/1.0` `0x55ed18` and `0x55eef4`, `Wii/1.0a` `0x55aba8` and `0x55ad43`: the result of
  CParser_GetUniqueName stored at Object +0x0c), so only the trace tells where they come from. Other `@NNN` locals come
  from CFunc (newlabel, `create_temp_object`, the statics after `CFunc_CompoundStatement` and after
  `CFunc_GenerateLoop`; 414, 53, 133 and 92 of them in one compile of `fmod_memory.cpp`) and have no source name. In
  `MemPool::set` the named locals (36-39) precede 13 such objects (40-52), which precede the temporaries.
- **A parameter that is never reassigned** is coalesced with its argument register (`pnomod`: `p` stays in r3
  through a loop with `p++`; `loop6`: `p`, node 32, is coalesced into r3).
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

1. `build/tools/mwtrace.exe regalloc <unit> --function SYM` gives each node's variable, degree, sweep, select
   position and register; put retail's registers beside them (`match.exe --diff`).
2. Find the first variable whose register differs and the node that has its register in retail: their sweeps,
   degrees and indices say which of the rules above decides.
3. Try a source change with `--against FILE2`: it prints the nodes the edit adds or changes, the first simplify and
   select position that differs and every named variable whose register moves.

## Scheduler (read and measured)

`build/tools/mwtrace.exe sched <unit> --function SYM` (Tools, below) traces every step named here: the nodes, the
edges with their kinds, each selection with the key that decided, checked against the object. Addresses are `Wii/1.0`;
`Wii/1.0a` has the block scheduler and everything after it (0x5bb600 on) instruction for instruction at -0x4980, its own
shorter driver at `0x5b6ad0` and its own data addresses (read; the `SC_` keys of `tools/c/tools/mwtrace/mwt_build.c`).

- **Driver** `0x5bb420` (scheduleinstructions, Scheduler.obj; assert string `Scheduler.c`) (read). It sets the machine
  model `[0x731548]` from the processor byte `[0x778ea4]` (`-proc`; `gekko`, byte 4, takes the table at `0x70335e`), calls
  its initialiser (+0x28) and `gather_alias_info`, then walks the block list `[0x7722fc]` and schedules a block when it
  has **more than 2 instructions** (block +0x28) and either the argument is nonzero or the block's flags (+0x2a) & 3 are
  clear; it then sets flag 8. Blocks of 1 or 2 instructions keep their input order (measured: listed by `mwtrace sched`
  as not scheduled; their instructions reach the object in that order). What flags 1 and 2 mean is not established; the
  entry and exit blocks of pass 1 carry them (0x5, 0x6) and are empty (measured).
- **Two passes**, both called from CodeGen_Generator (read): pass 1 at `0x46cc74` with argument 0, only when the schedule
  mode byte `[0x778ea5]` is 2, before `colorinstructions` (virtual registers; `[0x770c84]` (`coloring`) = 1, measured); pass
  2 at `0x46cbc5` with argument 1 when the byte is nonzero, after register allocation, `generate_prologue`,
  `generate_epilogue` and `peepholeoptimizepcode` (physical registers; coloring 0, measured). **Pass 2 schedules the
  prologue and epilogue with the body** (measured: its first block holds `stwu`, `mflr`, the saves and the body's first
  instructions). The mode byte is 2 in every traced compile. `Wii/1.0a` calls the driver from `0x46b325` and `0x46b3d4`
  (in the static `0x46b230`). `-opt noschedule` was not checked by the trace.
- **Machine model** (`gekko`, measured): issue width (+0) 2, +4 = 1 (so register output and anti dependences get latency
  0), +8 = 0 (extra latency into a branch). Its callbacks: +0x0c latency of an instruction, +0x10 reset, +0x14 can-issue,
  +0x18 issue (reserve), +0x1c end of cycle, +0x20 serialises (read). Can-issue refuses a second load or store in a
  cycle (measured: `fn_8000A76C`, every load ready at cycle 0 slot 1 answers 0). `603e`, `604e`, `601`, `generic` and `403`
  use other tables (read).
- **Nodes** (block scheduler `0x5bb600`, static, read). The block is walked from its last instruction (block +0x18,
  pcode +4 the previous one); each node is pushed on the list's head, so the list is in program order. 26 bytes: +0 next,
  +4 previous, +8 successor edges, +0x0c the pcode, +0x10 latency (machine +0x0c), +0x12 the earliest issue cycle, +0x14
  slack, +0x16 height (the latency at creation, raised by each edge), +0x18 predecessors not yet issued. Once the block
  is built, slack = the block's greatest height `[0x73154c]` - height (`0x5bb697`).
- **Edges** (read; every kind measured). The edge adder `0x5bc0b0` (static) takes (earlier, later, flag): latency = the
  earlier's latency when the flag is set, else 0; one less (not below 0) when the later is a store (pcode flags 0x40004)
  whose first operand `is_same_operand` the earlier's; plus machine +8 when the later is a branch (pcode flags 0x1); an
  edge already there keeps the greater latency; the earlier's height becomes at least latency + the later's height. The
  dependence builder `0x5bb980` (static), per node, with per-class, per-register lists of the later definitions
  (`[0x731562]`) and uses (`[0x731576]`):
  - register operands (kind 0) through `0x5bbfc0` (static): a definition gets an edge to every later use (`raw`, flag 1)
    and every later definition (`waw`); a use to every later definition (`war`); `waw` and `war` take flag 1 only for
    class 0 (SPR), else machine +4 == 0, so latency 0 on gekko. An operand that is both used and defined (a `bl`'s r3) is
    taken as a definition only (`and eax, 2`, read);
  - memory: a load (pcode flags 0x20002) against every later store, a store against every later load and store, each
    pair asked of `may_alias` (next section; never load against load), an edge (flag 1) when it answers 1;
  - instructions with pcode flag 0x80 are chained among themselves (`[0x731552]`, `ordered`);
  - a serialising instruction (pcode flags & 9 with flags2 & 0x800000, or flags & 0x100, or the machine's +0x20 callback)
    gets an edge to every later instruction (`serial`), and every instruction gets one, latency 0, to each later
    serialising one (`[0x731556]`, `to-serial`; measured: a `bl` is one);
  - a node with no successor gets an edge, latency 0, to the block's branch (`[0x73154e]`, the last instruction with pcode
    flag 0x1; `branch`).
- **List scheduling** `0x5bb6f0`-`0x5bb7c3` (read). Cycle by cycle from 0, up to machine +0 selections a cycle; a
  selection that finds nothing ends the cycle. An issued node's successors each lose a predecessor and get earliest =
  max(earliest, cycle + edge latency); the instruction is appended to the block (the new order), the machine's +0x18
  reserves it, the node leaves the list. Machine +0x1c ends the cycle.
- **Selection** `0x5bb7e0` (static) (read; every key measured by the trace's replay of the logged numbers). Ready: no
  predecessor left, earliest <= cycle, and can-issue nonzero. The first ready node in program order is the incumbent; each
  later ready node replaces it only if strictly better by the first key that differs:
  1. critical (slack <= current cycle) beats non-critical;
  2. more successors whose only remaining predecessor is this node;
  3. greater height;
  4. only while `[0x770c84]` (coloring) is set, that is in pass 1: the smaller opcode-priority byte at
     `0x6f06cb + 22*opcode` (measured: `LI` 4, `LIS` 4, `MFLR` 4, `LWZ` 3, `LFS` 3, `LFD` 3, `ADDI` 2, `STWU` 2,
     `FMUL` 2, `STW` 1, `MR` 0);
  5. otherwise the incumbent stays (program order).

  In pass 2 keys 1 to 3 apply as in pass 1; only key 4 is off. When `[0x779112]` is set an extra loop calls can-issue
  for the ready nodes after the incumbent and ignores the answers (read; the byte is 0 in every traced compile, its
  meaning is not established).
- **After the scheduler** (read and measured). After pass 2 come only `peepholeoptimizepcode` (when the VLE byte
  `[0x778e9b]` is set) and `assemblefunction` (`0x5a0ad0`: `optimizefinalbranches`, `0x5a0f00` (static), which deletes and
  adds instructions, then `assemblepcode` per instruction at `0x5a0cf8`). In every function traced (`mwtrace sched`'s
  self-check: the game, RenderWare and FMOD cases below) each pass-2 block's output order is the object's order, and no
  instruction was deleted from or added inside a scheduled block. Between the passes the order of pass 1 is the one
  register allocation numbers and colours; pass 2 then reorders physical-register code.
- **regalloc's instruction numbers** (`#N` in `mwtrace regalloc`, the `ra` column of `mwtrace sched`) count the
  rematerialisation walk `0x63d7e0`: blocks forward, **each block from its last instruction** (read: block +0x18,
  pcode +4), so within a block a later instruction has a lower number (measured: `fn_8000A76C`'s LFS #300 is issued
  before the LFD #298 of the same block).

Consequences for matching:

- Two independent instructions with equal criticality, successor count and height are ordered by key 4 in pass 1, by
  program order in pass 2. Measured with `tools/c/tools/mwtrace/tests/fixtures/sched_key4_a.c` and `_b.c` (`--against`):
  swapping the source order of `i = 0` and `p = table` ahead of `g(i, p)` does not swap `li` and `addi`, but not because
  a key overrides it: both texts give the scheduler the same input (the values are generated where they are used), and
  in both pass 1 issues `addi` over `li` by priority 2 < 4 at cycle 1.
- Anything that adds or removes a node in the block (even a dead copy that is deleted later) changes heights and
  successor counts and can reorder the survivors. See the first-function quirk below.
- Diagnosis: for a row whose instruction retail has at another position, `mwtrace sched ... --pair @A,@B` (the two
  object rows) names the selection and the key, or the edge that held one of them; an order difference in pass 2 with
  the same pass-1 order is decided by keys 1-3 on physical-register code.

Cases (measured with `mwtrace sched`, self-check clean):

- `TriListNBTDataSetup8` (RenderWare `nbtGen`): its first differing rows are an order difference: retail emits
  `clrlslwi r30, r5, 24, 2` before the two `clrlslwi` of r0, the build after them. In pass 2 (block 4) the r5 one has
  height 9 and both r0 ones 14: kept behind by key 3; in pass 1 the three are equal and program order decides.
  `TriStripNBTDataSetup8`'s first differing rows are a register difference (the same instructions in the same order).
- `FMOD::File::enableDoubleBuffer` (`Wii/1.0a`): the first differing rows are a register difference (the build saves one
  nonvolatile more); in the build, pass 2 keeps `mr r28, r4` behind `stw r28` by a `war` edge and `divwu` behind
  `lwz r4` by its `raw` latency 2.
- `fn_8000A76C` (`game/unk_8000A040`): pass 1 issues the LFS (virtual f85, regalloc #300) at cycle 0, before the LFD
  (f89, #298), by key 1 (slack 0 against 4): the LFS's temporary is the one defined first.

## Scheduler memory disambiguation (read and measured)

Whether the scheduler may move a load past a store (or two stores past each other) is decided per pair by
`may_alias` (`0x5c3460`, Alias.obj). The scheduler's dependence builders (statics at `0x5bbb56`, `0x5bbc06`,
`0x5bbc56` call sites, Scheduler.obj) ask it, for each load, about every later store, and for each store, about every
later load and store of the block (never two loads; read, and measured: `mwtrace sched` lists each pair with the answer
under "memory pairs"); an answer of 1 is an edge of the earlier's latency (`mem-load-store`, `mem-store-load`,
`mem-store-store`). A precheck (`0x5bbcc0`, static) runs first when `[0x778f2b]` is set, and a nonzero answer skips the
pair (read; the byte is 0 in every traced compile, its meaning is not established). Local CSE's
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
  `mwtrace sched` on the same pair (`tools/c/tools/mwtrace/tests/fixtures/sched_alias.c`, `copy_plain` and
  `copy_const`) shows the edges: with the plain parameter each store has a `mem-load-store` edge from every earlier
  load and each later load a `mem-store-load` edge from every earlier store (latency 2), while stores to different
  words of `dst` answer 0; with the const parameter every pair answers 0 and pass 1 issues the four loads before the
  stores, by key 2 (each load has a store waiting only on it) and program order (measured).
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
scheduler pass 1 and changes the selection in its block. It is deleted before register allocation's instruction walk
(measured with `mwtrace sched`), so the final listing shows only the changed order. Between pass 1 (`0x46cc74`) and
`colorinstructions` the only pass called is `eliminatedeadcode` (`0x5b8870`, at `0x46ca10` when `[0x779111]` >= 1);
`peepholeoptimizepcode` (`0x5b8300`) runs only after the prologue and epilogue (read in CodeGen_Generator's calls). Which
call deletes the copy was not traced.

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
- `mwtrace sched` shows the copy as a node (measured, `tools/c/tools/mwtrace/tests/fixtures/sched_first.c` against
  `sched_second.c`, `void probe(int n) { gi = n; gp = table; }` first in its file or after a function that takes the
  quirk): pass 1's block has one node more, `#0 MR r32= r3`, with no edge at all (its value is used nowhere), ready from
  cycle 0; it is compared at every selection (there it loses by height and criticality) and fills the second slot of
  cycle 1, which the other text leaves empty. The copy has no regalloc number and is not emitted: it is gone before
  register allocation's instruction walk. In this probe the survivors' order is the same in both texts; where the copy
  has a successor, its height and successor count enter the keys as described above.

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
- `build/tools/mwtrace.exe regalloc <unit> --function SYM [--against FILE2]` traces one function's allocation: per class
  and attempt the nodes in select order with their variable, degree, sweep, spill cost, free registers and register,
  the simplify order, the optimistic choices and the coalescing, self-checked against the -sym on DWARF and the
  prologue; `--against` compares two source texts (docs/tools.md).
- `build/tools/wdbg.exe <src> <unit> <outdir> <spec.wdbg>` runs mwcceppc under a
  debugger with the unit's flags. The spec file (`tools/c/tools/wdbg/SPEC.md` gives the format) maps breakpoint addresses to handlers that read or patch the
  compiler's memory; output in `<outdir>/log.txt`. Specs for the first-function
  quirk are in `tools/c/tools/wdbg/specs/`; `may_alias.wdbg` logs every memory-alias answer with both alias records;
  `iro_gate.wdbg` logs, per function, whether the IR optimiser runs and the first node that turned it off.
  `build/tools/wdbg.exe iro-scan` runs it over the non-exact functions of every NonMatching unit (or the named units) and flags
  a function whose IR optimiser is off while its siblings have it on; a spec carries a `build` line and refuses a compiler
  build whose addresses it does not know (FMOD builds with `Wii/1.0a`, the rest with `Wii/1.0`).
  A compiler crash is logged with its address, name and stack.
- `build/tools/mwtrace.exe sched <unit> --function SYM [--against FILE2] [--pair A,B]` traces one function's scheduling:
  per pass and block the nodes, the edges with their kinds, the memory pairs, each selection with the key that decided,
  self-checked by replaying the selections and against the object; `--pair` answers what ordered two instructions
  (docs/tools.md).

The partial mwcceppc decompilation the user supplied (Wii 1.1; CABI, CMangler and the MS-DOS OS layer only) has
no back-end code. The allocator was located through its `Coloring.c`, `InterferenceGraph.c` and `Spill.c` assert
strings instead. The `dumpir`, `dumpphases` and similar pragma strings exist in the binary but produce no
output in this release build.
