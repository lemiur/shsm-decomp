# MWCC inlining: what is established

How `mwcceppc` (CodeWarrior for Wii, 4.3 build 145) decides, call by call, whether a callee is expanded in place
or left as a call, and by which numbers. Use it with `build/tools/mwtrace.exe inline <unit>` (docs/tools.md), which
reads these decisions from a compile under the debugger.

Addresses are virtual addresses in `tools/compilers/Wii/1.0/mwcceppc.exe` (image base 0x400000), read with
`build/tools/pe_disasm.exe`; names come from the compiler's NB11 debug info (`build/tools/mwcc_symbols.exe`), and a
file-static function is written `static in CInline.obj`. `Wii/1.0a` (FMOD) is another compile of the same source:
the same code with other registers, other data addresses and two layout changes (an alias object's datatype is 6,
not 5; the inlinability kind sits at function data +0xda, not +0xf0). Its addresses are in
`tools/c/tools/mwtrace/mwt_build.c`, beside the `Wii/1.0` ones, under the same names; mwtrace uses them only on an
exe with the SHA-1 the table records.

Status labels, as in docs/compiler_regalloc.md: **read** (seen in the disassembly), **measured** (a compile under
`wdbg`/`mwtrace` with a control that changes the answer).

## The objects the inliner reads (read)

- An `Object` (a function): +2 datatype (3 `DFUNC`, 4 `DVFUNC`; 5 an alias, whose target is at +0x40; 6 in
  `Wii/1.0a`), +0xc its `HashNameNode` (name at +0xa), +0x10 its type, +0x14 its qualifiers (0x10: the `inline`
  keyword), +0x44 and +0x48 the link name `CMangler_GetLinkName` (0x56ea10) caches.
- Its type: +0 kind (7 a function type), +0x1a flags (0x800: auto-inline candidate, set by
  `CInline_CheckInlineability`; 0x20000: `__attribute__((never_inline))`, measured).
- Its function data, `CMid_GetFuncData(obj)` (0x481150): +0 the packed IR, +8 the cost, +0xf0 the inlinability
  kind. A function without a body in the unit (or not defined yet) has none.
- An `ENode`: +0 kind (0x39 a call, 0x3b an object reference), +8 flags, +0x10 the operand; a call's +0x10 is the
  function reference (its +8 bit 8: called directly, not through the vtable) and +0x14 the argument list.
- A `Statement`: +0 next, +4 kind, +0xa expression, +0x12 its destructor-object stack, +0x16 a file record (+0 â†’ a
  name node with the path at +0xa), +0x1a the source offset.

## When a function becomes a candidate (read and measured)

`CInline_CheckInlineability` (0x5615c0) runs once per defined function (from `CMid_DefineFunction`):

- qualifier 0x4 refuses it (read; what sets 0x4 is not established);
- the `inline` keyword makes it a candidate (read; measured);
- otherwise, when auto-inline is on (copts 0x779086; `-inline auto`, off with `-inline noauto`, measured) and
  `dont_inline` (0x7790ff) is off: the classifier below must not answer 0, and the size test (static at +0xe0,
  0x5616a0) must pass: the statements, not counting kinds 1 and 2 and expression statements whose expression kind
  is 0x40â€“0x42, at most `inline_max_auto_size` (0x779056; 15 in every traced unit, measured: 15 statements pass, 16
  do not), and the locals (the list at 0x7787cc, each type's size at +2) at most 0x400 bytes (read). It then sets
  type flag 0x800. A second path sets 0x800 without the size test when copts 0x77915b and 0x779112 are both set
  (read; 0x779112 is 0 in every traced unit, measured, so the path is never taken here).

## The inlinability kind (read and measured)

The classifier (static, 0x5641d0; called by `CheckInlineability` and by `CInline_ComputeInlineability`
(0x55f260), which stores it in the function data) answers:

- 0, not inlinable: the type has flag 0x20000 (`never_inline`, measured); the result is returned through a hidden
  pointer (`CMach_GetFunctionResultClass` 1) for a class whose destructor has side effects; another result class
  (2 and above); a `...` parameter (the marker type 0x770994; measured); a class parameter whose destructor has side
  effects; an expression statement of expression kind 0x3e (read; what 0x3e is is not established);
- 3, a body inlined as statements: a statement with a destructor-object stack (+0x12), or any statement but a
  final `return` (measured: a loop body is 3);
- 6, a body inlined as an expression (measured: `return x + 1;`). In `Wii/1.0`, copts 0x779169 turns 6 into 3;
  `Wii/1.0a` has no such test (read).

## The cost (read and measured)

The function data's +8 is the sum, over the statements, of a per-statement-kind cost function (table 0x770bc0)
that adds the per-node-kind cost of the expression (table 0x772438); the call node's cost function (0x564800,
static) adds the costs of the function reference and of each argument, and 1 for the node itself in the
ordinary case. It is a node count (measured: `return x + 1`
costs 2, a callee with 60 assignments 300). With `-ipa file` the cost is measured again after the function's own
inlining (`ComputeInlineability` in the visitor), so a caller sees its callee's inlined size (measured: a
self-recursive function expanded into itself goes from 4 to 22).

## Who runs the inliner, in which order (read and measured)

- With `-ipa file` (most units of this project): `CInline_CallGraphInlining` (0x55cb60) sets the bottom-up
  flag (0x77906b) and the inline depth (0x779066) to 0, so `-inline level=N` and `#pragma inline_depth` change
  nothing (measured), and visits the call graph with a static visitor (0x55ccb0): each function once, its callees
  first. A function none of whose callees (with a body) is a candidate is not visited at all (measured). After a
  caller's passes, `COpt_SimpleOptimizer` runs and `ComputeInlineability` measures the caller again; then, unless
  `always_inline` (0x779100) or copts 0x779101 is set, the growth test: when the new cost is above 2000 and above
  four times the old, or above 10000 and above old + old/4, the caller's IR is dropped and every expansion in it is
  undone (read; measured: 29 â†’ 2419 undoes ten expansions, the object keeps the ten calls).
- Without `-ipa file`: `CInline_Expand` (0x55cee0) runs per function at code generation, in source order, with the
  bottom-up flag as the options leave it (on in the traced unit, measured). A callee defined later in the file is no
  candidate yet when its caller is inlined (measured).

## The passes (read and measured)

The pass driver (static, 0x55cf90) does nothing when `dont_inline` is on or the inline depth is negative. In
bottom-up mode it first sets the working size limit (0x72b066) to `inline_max_size` (0x77905e) and halves it while
it is above 1 and the caller's estimated size (static 0x563e20) is above `inline_max_total_size` (0x779062). Each
pass walks every statement and every call node; the pass number is the level (0x72b192). After a pass the loop ends
when not bottom-up and no statement-level expansion was queued in the pass (0x72b194); else after level 3 (depth 0,
top-down), level 7 (depth 0, bottom-up) or level Nâˆ’1 (depth N) (read). Measured: a recursive callee is expanded at
levels 0 to 3 and its call at level 4 is never checked; a pass that expanded only expression bodies ends the loop,
so the calls inside those bodies are never checked either (they stay calls). Calls inside an expanded body are new
nodes, checked at the next level; a call left at one level is checked again at the next (measured: the same node).

Within one statement the walker does not reach the call nodes in source order (measured: in
`leaf(x) + kw(x) + big(x) + ... + ni(x) + stmtk(x)` the checks run `stmtk`, `ni`, ..., `big`, `leaf`, `kw`).
mwtrace numbers a caller's sites of one callee by level, then line, then this order.

## The decision for one call (read and measured)

The decision (static, 0x561e80) for a call node whose function reference is an object reference returns 1
(expand) or 0, each through its own return block, which is how mwtrace names the rule:

| rule | what it tests | block |
|---|---|---|
| `not-candidate` | the callee's type is a function, it has the `inline` keyword or flag 0x800, its datatype is 3, or 4 with the direct-call bit, and qualifier 0x4 is clear; otherwise 0 (measured) | 0x5622d0 |
| `always-inline` | `always_inline` (0x779100): 1 | 0x561f07 |
| `no-funcdata` | no function data for the callee: 0 | 0x562110 |
| `bottom-up-last` | bottom-up, no level left: 1 for an `inline` callee when copts 0x77906c is set, or a type with flag 0x80000000 (`Wii/1.0` only) | 0x561f7f |
| `bottom-up-size` | bottom-up: the callee's estimated size with its own inlines (static 0x564060, levels left) above the working limit: 0 (measured: cost 1, limit 512) | 0x56214b |
| `level1-cost` | top-down, depth 0, level â‰¥ 1: cost above 40: 0 (measured) | 0x562155 |
| `level2-cost` | level â‰¥ 2: cost above 28: 0 (measured) | 0x562169 |
| `level3-cost` | level â‰¥ 3: cost above 12: 0 (measured: cost 22 refused) | 0x561fcd |
| `pass` | none of the above: 1, unless copts 0x77915b and 0x779112 send it to the analysis below (measured) | 0x5622c4 |
| `opt-fit`, `ipa-analysis` | the analysis path: caller and callee costs against copts 0x77915c, a constant-argument estimate against 0x779160 (read; not reached in this project) | 0x562102, 0x56224f |

Level 0 has no size limit (read; measured: a callee of cost 300 is expanded at level 0).

A `pass` is not yet an expansion: the expansion function (static, 0x55e2c0) leaves a kind-0 callee (and kinds 1, 2,
4 and 5) as a call (measured: an `inline` varargs function), queues a kind-3 call at statement level (at most 64
per statement: 0x72b088, the list at 0x72b08a; read) for the statement expander (static 0x55d400 â†’ 0x55d610) after
the statement's walk, and expands a kind-6 body in place (measured).

## What follows the inliner (measured)

Calls the inliner leaves can still disappear: `COpt_SimpleOptimizer` runs between the last pass and the post-inline
measure (fmod_eventprojecti: a `__dl` call in a base destructor's inlined body), and later passes remove calls the
post-inline IR still has (fmod_eventprojecti `loadSampleData`: 15 `MemPool::free` calls after the inliner, 11 in the
object). mwtrace's self-check names the stage where the counts part.

## Line numbers (measured)

A statement's offset (+0x1a) counts as many line ends in its file as its line number; after `#line N` that number
is N's (a statement three lines after `#line 634` has an offset whose count is 637), and a number beyond the file's
own lines gives the offset of the end of the file. mwtrace maps the count back to the physical line through the
file's `#line` regions and gives no line where that is ambiguous or at the end of the file.

## Not established

- What qualifier 0x4 and expression kind 0x3e are; the meaning of copts 0x77915b, 0x779112, 0x779101, 0x77906c and
  0x779169 (their effects above are read, their option names are not).
- The analysis path's numbers (`opt-fit`, `ipa-analysis`): never reached under this project's flags.
- The statement-inline queue limit of 64 is read, not measured.
