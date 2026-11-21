# Exceptions record

Functions whose status is not plain `matched`, and the review rulings that let a function count. Every entry
names the function, the rule and the deviation. The future rule checker (`build/tools/check_rules.exe`) takes its
allow list from this file. The numbers are in [progress.md](progress.md).

## MATCH-PENDING

| unit | function | rule | deviation |
|---|---|---|---|
| `revolution/os/OSPlayRecord` | `PlayRecordAlarmCallback`, `PlayRecordCallback`, `__OSStartPlayRecord`, `__OSStopPlayRecord` (4 functions, 1792 bytes) | R17 (steering) | `RecordCheckSum` was changed from the reference's `static u32 RecordCheckSum(OSPlayRecord* record)` (zeldaret/tp and doldecomp/ogws both have it static, with the record as parameter) to a public, parameterless `u32 RecordCheckSum(void)` that reads `PlayRecord` directly, declared in `OSPlayRecord.h`. Retail has no out-of-line copy of it; the reshaping exists only to make the compiler touch `PlayRecord` first and so give it the first `.bss` address (with the reference shape the data order differs and two functions score 99.9%). A signature and linkage chosen to steer layout is a Section F lever, not a recovered original. Linked (bytes identical, gate green), kept as the best-known state, not counted. ogws reaches the same order with `DECOMP_FORCEACTIVE(OSPlayRecord_c, PlayRecord)`, also a tool-side device. What would make it authentic: the real declaration order or storage of the statics that gives retail's order with the reference's `RecordCheckSum`. `-pool off` is refuted (ruling "DVD rulings (sdk-io-3)"): retail addresses `PlayRecordAlarm` as `PlayRecord`'s base register + 0x80, which is pooled `.bss` (the default `-pool on`), and the reference's `RecordCheckSum` with `-pool off` scores `PlayRecordCallback` and `__OSStopPlayRecord` 83%. |
| `revolution/os/OSStateFlags` | `__OSWriteStateFlags` (0x8021D4F0, 228 bytes) | R17 (steering) | zeldaret/tp's `__OSWriteStateFlags` body, with `#pragma global_optimizer off` around the function. Retail keeps the last file-close test as a compare and branch (`cmpwi`/`beq`, separate `FALSE` and `TRUE` return blocks); with the global optimizer on, the compiler folds the two constant returns into one boolean value (`cntlzw`/`srwi`), which is the only difference in the function (92.5%). No source spelling tested moves it: `if (result)`/`if (result != 0)`, the assignment-in-condition form, a `BOOL` local, `goto`, `if (result == 0) return TRUE; return FALSE;`, the call inline in the condition, `u32`/`BOOL` casts, and every optimizer level and facet (`-opt level=0..4`, `no{dead,deadcode,deadstore,lifetimes,prop,cse,loop,strength,autoinline,peephole,schedule}`), `-O4` without `speed`, `-ipa` off and `-inline deferred` (some of these keep the branches but lose the inlining or the register allocation). The pragma is the same Section F lever as OS.c's (ruling "OS unit rulings (sdk-os-4)" below); a library-wide `-prefix` file carrying it was tested and breaks 91 units, so it is not a build setting. Linked (bytes identical, gate green), kept as the best-known state, not counted. |
| `game/CAmbientInterferencePoint` | `fn_800C2304__25CAmbientInterferencePointFP14CBaseBehaviour` (0x800C2304, 744 bytes; 99.9%) | R18 | The range-and-facing test normalises and dots two vectors with paired-single code scheduled into the C around it, so the helpers were inline asm in the original too; the source uses invented `asm` inline helpers (`V3dLength` and `V3dNormalize` from `include/game/VectorPS.h`, the unit-local `NormalizeFacing` and `FacingDot`; retail's two normalisations are two different helpers) whose original header is not identified. Residue 2 of 186 rows (the order of the two dot-product address computations) besides the unit's `.sdata2` constant order. Unit parked. |
| `game/unk_8000A040` | `fn_8000B5DC__12unk_8000A040FiiP12unk_PadQueue` (0x8000B5DC, 2232 bytes; 90.1%) | R18 | The motion-sample queue update sums and divides vectors with paired-single code (`V3dAdd`, `V3dDivide`, inline asm in the unit, as CAmbientInterferencePoint's vector helpers); their original header is not identified. Below 100% as well (register allocation, iterator evaluation order). The unit stays parked. |
| `game/CDrawingTest` | `fn_80178A60__12CDrawingTestFv` (0x80178A60, 2212 bytes; exact), `fn_80179304__12CDrawingTestFP12unk_RwCamera` (0x80179304; 74.4%), `fn_801796F8__12CDrawingTestFP12unk_RwCamera` (0x801796F8; 99.5%) | R18 | The drawing renderer and the crayon placement scale, add and normalise vectors with paired-single code scheduled into the C around it, as CAmbientInterferencePoint and unk_8000A040 do; the source uses invented `asm` inline helpers (a unit-local `V3dScale` on `unk_Vec3`, and `V3dAdd`/`V3dScale`/`V3dNormalize`/`V3dDot` on `unk_Vector3` in `include/game/VectorPS.h`). The two helper sets and the two vector types (`unk_Vec3`, `unk_Vector3`) are not yet unified. Unit parked (match/crayon ruling "crayon-1"). |

Apart from these, every function linked from source passed a hand review against the [match] rules (R1, R2, R4, R13, R17,
R18) and the Section F fakematch catalogue. The reader-test rulings on the MSL stdio, multibyte and wide-string
units, the OS interrupt, memory and mutex units and the EXI/SI units are listed under "Reader-test rulings" below.
The 18 functions of `revolution/os/OSUtf`, `OSIpc` and `OSStateTM` (lane sdk-os-4) pass the checker and the hand
review; their rulings are under "Reader-test rulings" and "OS unit rulings (sdk-os-4)" below.
The 27 functions of the nine MetroTRK units (lane msl-trk-4: `main_TRK`, `mainloop`, `nubevent`, `nubinit`,
`CircleBuffer`, `serpoll`, `string_TRK`, `support`, `targcont`) pass the checker and the hand review; their rulings
are under "MetroTRK rulings (msl-trk-4)" below.
The 18 counted functions of `revolution/db/db` (4; `__DBExceptionDestination` is ORIGINAL-ASM), `revolution/dvd/dvdfs`
(8) and `revolution/dvd/dvdqueue` (6) (lane sdk-io-2) pass the checker and the hand review; their rulings are under
"DVD and DB rulings (sdk-io-2)" below.
The 16 functions of MetroTRK `msg` (`TRKMessageSend`) and `msgbuf` (15) (lane msl-trk-5) pass the checker and the
hand review; their rulings are under "MetroTRK rulings (msl-trk-5)" below.
The 36 counted functions of MetroTRK `msghndlr`, `dispatch`, `mslsupp`, `MWCriticalSection_gc`, `EXI2_GDEV_GCN/main` and
`target_options` (lane msl-trk-6) pass the checker and the hand review; their rulings are under "MetroTRK rulings
(msl-trk-6)" below.
The 44 counted functions of `revolution/os/OSFatal`, `OSFont`, `OSReset`, `OSRtc`, `OSNandbootInfo`, `OSPlayTime` and
`OSLaunch` (lane sdk-os-5) pass the checker and the hand review; their rulings are under "OS unit rulings (sdk-os-5)"
below.
The 103 functions of the ten linked RVL SDK GX units (`GXInit`, `GXAttr`, `GXMisc`, `GXGeometry`, `GXFrameBuf`,
`GXBump`, `GXTev`, `GXPixel`, `GXDisplayList`, `GXPerf`; lane sdk-gx) pass the checker and the hand review; their
rulings are under "GX rulings (sdk-gx)" below.
The 81 newly counted functions of lane sdk-av-2 (AXSPB, AXVPB, AXProf, dsp, dsp_debug, THPDec, THPAudio, and vi.c and
WUD debug_msg after the user's R17 ruling of 2026-10-18) pass the checker and the hand review; their rulings are under
"sdk-av-2 rulings" below.
The 27 functions of lane sdk-axfx (`AXFXReverbHi` 3, `AXFXReverbHiDpl2` 3, `AXFXReverbHiExp` 7, `AXFXReverbHiExpDpl2`
7, `AXFXReverbStd` 3, `AXFXHooks` 4) pass the checker and the hand review; their rulings are under "sdk-axfx rulings"
below.
The 19 functions of lane sdk-axart (`mix` 16, `mix_rmt` 3) pass the checker and the hand review; their rulings are
under "sdk-mix rulings" below.

The 24 linked libjpeg units pass the checker. The 34 [match] findings first reported on them (R17 `* 1`/`+ 0`,
R1 on same-named private types, R2 on macro bodies) flagged IJG 6b's own spellings; they were checker bugs, fixed
in the rule checker (`tools/c/tools/check_rules`) with regression tests, not allow-list entries:
- R17: `inptr[DCTSIZE*1]` (jidctint, jidctfst, jidctflt, jidctred) and `errorptr[dir3+0]` (jquant2 `pass2_fs_dither`)
  are IJG 6b's own spellings, one row of an index series beside `[DCTSIZE*2]`... and `[dir3+1]`, `[dir3+2]`. The
  checker no longer fires on a whole subscript `[X * 1]`/`[X + 0]` when the same function also subscripts
  `[X * n]`/`[X + n]` with another literal.
- R1: `my_upsampler` (jdsample/jdmerge), `savable_state` (jdhuff/jdphuff) and `my_cquantizer` (jquant1/jquant2) are
  separate types private to their C units, as in IJG 6b; no header declares them and no body copies a prefix of the
  other. R1 is about one type defined twice. The checker now skips a name whose definitions are all in C source
  files, with different bodies, none a prefix copy and no defining file including another.
- R2: `jpeg_common_fields` (jpeglib.h), `BITREAD_STATE_VARS` (jdhuff.h) and `INPUT_VARS` (jdmarker.c) are macro
  bodies; main's macro-body fix (191c7ba) already keeps them out of the parsed code, and a regression test covers it.
- The checker also reads IJG's `GLOBAL(type)`/`LOCAL(type)`/`METHODDEF(type)`/`EXTERN(type)` as the return type, so
  libjpeg functions are reported under their own names (they were all reported as `GLOBAL`/`METHODDEF`/`LOCAL`).

## RULE-EXCEPTION

| location | rule | deviation |
|---|---|---|
| `include/TRK/trk_glue.h` (`OSReport`; used by `TRKPostEvent`, `TRKGetFreeBuffer`, `TRKMessageSend`) | R6, R25 | `void OSReport();`, an old-style declaration without a prototype, beside the SDK's variadic `void OSReport(const char* msg, ...)` in `revolution/os/OSError.h`. The calls from these units take one and two arguments and none sets up CR6; only an unprototyped declaration compiles both that way. Ruling under "MetroTRK rulings (msl-trk-5)" (it replaces msl-trk-4's fixed `void OSReport(char* fmt);`). Counted. |
| `src/game/unk_8000A040.cpp` (`fn_8000A6B8__Fffff`, 0x8000A6B8) | R30 | The parameter `x` is transformed in place: signed input, magnitude, curve value, signed result (`x *= sign; ... x = pow(...); x *= sign; return x;`). With a separate result local the allocator keeps it out of f1 and the function no longer matches. Reviewed in the game-6 merge: an in-place transform of one value, a common idiom, not a merged pair of unrelated meanings (Section F). Counted when `game/unk_8000A040` links (unit parked). |
| `src/rwsdk/world/baworobj.c` (`WorldSyncCamera`) | R30 | `spSect` is declared last; Gormiti's DWARF lists it earlier. Declared last it gets retail's register numbering; a pure declaration move (user's ruling, 2026-11-15, "Rulings of 2026-11-15"). Counted. |
| `src/rwsdk/src/plcore/bamemory.c` (`_rwFreeListFreeReal`) | R30 | `heap` is declared at function scope; Gormiti's DWARF has it in the block. At function scope it gets retail's register colouring; a pure declaration move (same ruling). Counted. |
| `src/rwsdk/plugin/skin2/skinstream.c` (`_rpSkinGeometryNativeWrite`) | R30 | `maxWeights` is declared before `numVertices` and `size`; Gormiti's DWARF lists it last. Declared first, retail's register numbering of `maxWeights` and `numVertices` follows; a pure declaration move (same ruling). Counted. |
| `src/rwsdk/world/instancegeom.c` (`_rwDlGeometryInstanceOptimized`) | R30 | `displayLists` is declared before `size` and `i` after `displayListSize`; Gormiti's DWARF lists `i` first and `displayLists` after `vbHeader`. Pure declaration moves for retail's register numbering (same ruling). Counted. |
| `src/rwsdk/world/instancegeom.c` (`_rwDlGeometryInstanceFast`) | R30 | `displayLists` is declared before `size` (and `displayListSize` after `vbHeader`); Gormiti's DWARF lists `displayLists` after `vbHeader` and `displayListSize` before `vBufferSize`. Pure declaration moves for retail's register numbering (same ruling). Counted. |
| `src/rwsdk/world/instanceworld.c` (`_rwDlWorldSectorInstanceOptimized`) | R30 | `displayLists` is declared before `size` and `i` after `displayListSize`; Gormiti's DWARF lists `i` first and `displayLists` after `vbHeader`. Pure declaration moves for retail's register numbering (same ruling). Counted. |
| `src/rwsdk/world/instanceworld.c` (`_rwDlWorldSectorInstanceFast`) | R30 | `displayLists` is declared before `size` (and `displayListSize` after `vbHeader`); Gormiti's DWARF lists `displayLists` after `vbHeader` and `displayListSize` before `vBufferSize`. Pure declaration moves for retail's register numbering (same ruling). Counted. |
| `src/game/unk_Trail.cpp` (`fn_80081858`) | R30 | `atomicFrame` is declared before the first call, ahead of its first use. Declared at its first use, the allocator gives it another register than retail (r31); the declaration here gives retail numbering. Pure declaration move, counted. |
| `src/game/unk_80077C9C.cpp` (`fn_8007886C`, `fn_80078914`, `fn_800789BC`, `fn_80078A64`, `fn_80078B0C`, `fn_80078BB4`) | R30 | `key` and `value` are declared before `record`, and `key` is loaded after the record check. Declared in that order, retail's register numbering (r30 record, r31 key, the value in the key's register) follows; a pure declaration move, noted in each header. Counted. |
| `src/game/unk_80077C9C.cpp` (`fn_80077C9C`, `fn_80077D44`, `fn_80077DEC`, `fn_80077E94`, `fn_80077F3C`, `fn_80077FE4`, `fn_8007808C`, `fn_80078134`, `fn_800781DC`, `fn_80078284`, `fn_8007832C`, `fn_800783D4`, `fn_8007847C`, `fn_80078524`, `fn_800785CC`, `fn_80078674`, `fn_8007871C`, `fn_800787C4`, `fn_8007886C`, `fn_80078914`, `fn_800789BC`, `fn_80078A64`, `fn_80078B0C`, `fn_80078BB4`, `fn_80078C5C`, `fn_80078D04`, `fn_80078DAC`, `fn_80078E54`) | R30 | `key` and `value` are declared before `record`, and `key` is loaded after the record check. Declared in that order, retail's register numbering (r30 record, r31 key, the value in the key's register) follows; a pure declaration move, noted in each header. Counted. |

## ORIGINAL-ASM

Written in assembly in the original SDK or Metrowerks runtime (`asm` functions with `nofralloc` in the public
RVL SDK and runtime sources). Listed apart and left out of both counts and totals.

| unit | function | evidence |
|---|---|---|
| `Runtime.PPCEABI.H/__mem` | `memcpy` (0x29C bytes, in .init) | the Wii runtime's hand-written copy: overlap test, `dcbt` touch, double-word (`lfd`/`stfd`) block loop, word and byte tails, forward and backward; an `asm` function in xbret/xenoblade `__mem.c` ("Handwritten", Matching there), and the retail body is the same. Source marker at the function. |
| `Runtime.PPCEABI.H/ptmf` | `__ptmf_scall` | takes its PTMF record in r12 (no C calling convention can) and tail-jumps through `bctr`; an `asm` function in the Metrowerks runtime |
| `Runtime.PPCEABI.H/runtime` | `__cvt_fp2unsigned`, `__save_fpr`, `__restore_fpr`, `__save_gpr`, `__restore_gpr`, `__div2u`, `__div2i`, `__mod2u`, `__mod2i`, `__shl2i`, `__cvt_sll_flt`, `__cvt_ull_flt`, `__cvt_dbl_usll`, `__cvt_dbl_ull` (14 functions, 2188 bytes) | compiler support routines with fixed register contracts: the save/restore routines store below r11 and have 72 extra entry points (`_savefpr_N`, `_restfpr_N`, `_savegpr_N`, `_restgpr_N`, `type:label` in `symbols.txt`), which C cannot express; the 64-bit routines take r3:r4/r5:r6 pairs and use r5-r10 without a frame or saves. All are `asm` functions with `nofralloc` in the Metrowerks `runtime.c`. |
| `revolution/os/OSAlarm` | `DecrementerExceptionHandler` | first-level exception stub: saves registers into the context and branches to the C callback; an `asm` function in the SDK |
| `revolution/os/OSTime` | `OSGetTime` | `mftbu`/`mftb` retry loop; an `asm` function in the SDK |
| `revolution/os/OSTime` | `OSGetTick` | `mftb`; an `asm` function in the SDK |
| `revolution/os/__ppc_eabi_init` | `__init_hardware`, `__flush_cache`, `__init_user` (3 functions, 120 bytes) | `asm` functions in the SDK `__ppc_eabi_init.c` (the public ogws reconstruction has the same bodies). `__init_hardware` and `__flush_cache` sit in `.init`, run before the stack is usable and use `nofralloc`; `__init_user` is `fralloc`/`bl __init_cpp`/`frfree`, which keeps the frame and the `bl` that every Wii compiler turns into a tail branch from a C body. |
| `revolution/os/__start` | `__start`, `__init_registers` (2 functions, 508 bytes, in .init) | the entry point runs before there is a stack or small-data registers: `__init_registers` clears the GPRs and loads r1/r2/r13 from the linker's `_stack_addr`, `_SDA2_BASE_` and `_SDA_BASE_` (`lis`/`ori`); `__start` builds the first frame by hand, keeps argc/argv in r14/r15 across calls, calls `InitMetroTRK` through `mtlr`/`blrl` and ends in `b exit`. `asm` functions with `nofralloc` in xbret/xenoblade's and zeldaret/tp's `__start.c` (the same text), and the retail bodies are the same. |
| `revolution/base/PPCArch` | `PPCMfmsr`, `PPCMtmsr`, `PPCMfhid0`, `PPCMthid0`, `PPCMfl2cr`, `PPCMtl2cr`, `PPCMtdec`, `PPCSync`, `PPCHalt`, `PPCMtmmcr0`, `PPCMtmmcr1`, `PPCMtpmc1`, `PPCMtpmc2`, `PPCMtpmc3`, `PPCMtpmc4`, `PPCMffpscr`, `PPCMtfpscr`, `PPCMfhid2`, `PPCMthid2`, `PPCMtwpar`, `PPCSetFpNonIEEEMode` (21 functions, 236 bytes) | MSR/SPR/FPSCR accessors that C cannot express (`mfmsr`, `mtspr`, `mffs`/`mtfsf` through a hand-built frame, `mtfsb1`, `sc`, the `PPCHalt` spin loop); `asm` functions with `nofralloc` in the RVL SDK `PPCArch.c`. `PPCMthid4` is not here: it is C (listed helper below). |
| `revolution/os/OSCache` | `DCEnable`, `DCInvalidateRange`, `DCFlushRange`, `DCStoreRange`, `DCFlushRangeNoSync`, `DCZeroRange`, `ICInvalidateRange`, `ICFlashInvalidate`, `ICEnable`, `__LCEnable`, `LCDisable`, `LCStoreBlocks`, `LCQueueWait` (13 functions, 636 bytes) | cache-block loops (`dcbi`/`dcbf`/`dcbst`/`dcbz`/`icbi` over `ctr`, `blelr` on an empty range), HID0/HID2 bit sets, DBAT3 and `dcbz_l` for the locked cache, `DMA_U`/`DMA_L` writes; `asm` functions with `nofralloc` in the RVL SDK `OSCache.c`. |
| `revolution/os/OSContext` | `__OSLoadFPUContext`, `__OSSaveFPUContext`, `OSSaveFPUContext`, `OSSetCurrentContext`, `OSSaveContext`, `OSLoadContext`, `OSGetStackPointer`, `OSSwitchFiber`, `OSSwitchFiberEx`, `OSInitContext`, `OSSwitchFPUContext` (11 functions, 1456 bytes) | register save/restore (`stmw`/`lmw`, FPRs, PS1 halves, GQRs), `rfi` exits, stack switches through `blrl`, a tail branch to `OSClearContext`; `asm` functions with `nofralloc` in the RVL SDK `OSContext.c`. |
| `revolution/os/OSInterrupt` | `OSDisableInterrupts`, `OSEnableInterrupts`, `OSRestoreInterrupts` (3 functions, 76 bytes) | `mfmsr`/`mtmsr` on MSR[EE], which C cannot express; `OSDisableInterrupts` also carries the `entry __RAS_OSDisableInterrupts_begin`/`_end` labels that `OSLoadContext` compares SRR0 against (`type:label` in `symbols.txt`). `asm` functions with `nofralloc` in the RVL SDK `OSInterrupt.c`. |
| `revolution/os/OSInterrupt` | `ExternalInterruptHandler` | first-level exception stub like `DecrementerExceptionHandler`: stores r0-r2, `stmw r6` and GQR1-GQR7 into the context and tail-branches to `__OSDispatchInterrupt` under a hand-made 8-byte frame; an `asm` function with `nofralloc` in the SDK. |
| `revolution/os/OSMemory` | `ConfigMEM1_24MB`, `ConfigMEM2_52MB`, `ConfigMEM2_56MB`, `ConfigMEM2_64MB`, `ConfigMEM2_112MB`, `ConfigMEM2_128MB`, `RealMode` (7 functions, 1124 bytes) | IBAT/DBAT writes (`mtspr` to the BAT SPRs, `isync`) run in real mode and left through `mtsrr0`/`mtsrr1`/`rfi`; `RealMode` enters real mode by clearing MSR[IR,DR] through `rfi`. `asm` functions with `nofralloc` in the RVL SDK `OSMemory.c`. |
| `revolution/db/db` | `__DBExceptionDestination` | the debugger's exception hook: runs with the MMU off and no stack, sets MSR[IR,DR] (`mfmsr`/`ori`/`mtmsr`) and tail-branches to `__DBExceptionDestinationAux`; an `asm` function with `nofralloc` in the SDK `db.c` (the public ogws reconstruction has the same body). |
| `revolution/ai/ai` | `__AICallbackStackSwitch` | runs the AI DMA callback on `__CallbackStack`: saves r1 into `__OldStack`, loads r1 from `__CallbackStack` (minus 8), calls the callback through `mtlr`/`blrl` and restores r1, inside `fralloc`/`frfree`. C cannot switch the stack pointer. `static asm` in zeldaret/tp `ai.c` (the same body, `fralloc` form) and in doldecomp/ogws `AI/ai.c` (`mr r31` form); the retail body is tp's. |
| `revolution/thp/THPDec` | `__THPInverseDCTNoYPos`, `__THPInverseDCTY8`, `__THPHuffDecodeDCTCompY`, `__THPHuffDecodeDCTCompU`, `__THPHuffDecodeDCTCompV` (5 functions, 7364 bytes) | hand-scheduled paired-single IDCTs (`psq_l`/`ps_madd`/`psq_st` over the locked-cache work rows) and the Huffman bit-reader loops written as `asm` blocks that make up the bodies of these functions in doldecomp/mkdd `libs/dolphin/thp/THPDec.c` (the same structure: register parameters, asm blocks around a few C statements). Not C code under the rules; listed apart, not counted (ruling "sdk-av-2 rulings"). |
| `revolution/os/OSSync` | `SystemCallVector` | exception-vector body that `__OSInitSystemCall` copies to 0x0C00 from between its `entry __OSSystemCallVectorStart`/`End` labels: HID0[ABE] toggle, `isync`/`sync`, `rfi`; an `asm` function with `nofralloc` in the RVL SDK `OSSync.c`. |
| `MetroTRK/Processor/ppc/Generic/flush_cache` | `TRK_flush_cache` | cache-line loop (`dcbst`/`dcbf`/`sync`/`icbi` over the rounded range); an `asm` function with `nofralloc` in the public zeldaret/tp MetroTRK `flush_cache.c`, and the retail body is the same. |
| `MetroTRK/Processor/ppc/Generic/mpc_7xx_603e` | `TRKSaveExtended1Block`, `TRKRestoreExtended1Block` (2 functions, 808 bytes) | bulk `mfsr`/`mfspr`/BAT reads into `gTRKCPUState.Extended1` (`stmw`) and the reverse (`lmw`, `mtsr`/`mtspr`, the time base only when `gTRKRestoreFlags` says so); `asm` functions with `nofralloc` in tp's `mpc_7xx_603e.c`. Retail is the Gekko/Broadway variant (GQR0-7, HID2, WPAR, DMA_U/DMA_L, the performance counters in place of tp's 7xx extras), written from the retail bytes in tp's structure; ruling "MetroTRK rulings (msl-trk-6)". |
| `MetroTRK/Os/dolphin/dolphin_trk` | `InitMetroTRK`, `InitMetroTRK_BBA` (2 functions, 300 bytes) | debugger entry points: save every GPR into `gTRKCPUState` (`stmw r0`), copy LR/CR/MSR, clear IABR/DABR, load r1 from the linker's `_db_stack_addr` and jump to `TRK_main`, which no C body can do; `asm` functions with `nofralloc` in tp's and xenoblade's `dolphin_trk.c` (the same text). Ruling "MetroTRK rulings (match-trk)". |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKLoadContext` | reloads an `OSContext`'s registers (`lmw`, `mtcrf`/`mtctr`/`mtxer`, MSR[EE,RI] off, SPRG1-3) and branches to `TRKInterruptHandler` with the vector in r3; an `asm` function with `nofralloc` in tp's and xenoblade's `dolphin_trk_glue.c`. |
| `MetroTRK/Processor/ppc/Generic/__exception` | `pad_00_80004000_init` (1 function, 7988 bytes, in .init) | the bytes of `gTRKInterruptVectorTable`, which dtk labels and covers with its generated `pad_` function: MetroTRK's exception vector table, the `"Metrowerks Target Resident Kernel for PowerPC"` banner, then one 0x100-byte slot per exception (a branch to `__TRK_reset` at 0x100; the others save r2-r4 in SPRG1-3 and `rfi` into `TRKInterruptHandler` with the vector number in r3), which `__TRK_copy_vectors` copies to low memory. An assembly file in the original (`__exception.s`, as in xbret/xenoblade), assembled here by the binutils `as` rule of dtk-template; an assembly source unit is ORIGINAL-ASM throughout (user's ruling, 2026-11-14). |
| `MetroTRK/Processor/ppc/Export/targsupp` | `TRKAccessFile`, `TRKOpenFile`, `TRKCloseFile`, `TRKPositionFile` (4 functions, 32 bytes) | `twui r0, 0; blr` trap stubs the host debugger services; tp's and xenoblade's `targsupp.s` is an assembly file. Written as `asm` functions in a `.c` unit with `twi 31, r0, 0` (MWCC's inline assembler has no `twui` mnemonic; the same encoding). |
| `rwsdk/plugin/skin2/skinmatrixblend` | `_rpSkinMatrixBlendUpdateASM` (1 function, 508 bytes) | RpSkin2's matrix palette blend, `dstMat[b] = matA[b] * matB[b] * matC` over the used-bone list: a hand-written frame (`stwu -0x98` and 18 `stfd` for f14-f31, no `mflr`, no calls), `matC` held in f0-f7, then `psq_l`/`ps_muls0`/`ps_madds0`/`ps_madds1` scheduled by hand around an `mtctr`/`bdnz` loop of 64-byte matrices; the Gormiti DWARF line table (`skinmatrixblend.c`, RpSkin2.a `skinmatrixblend.obj`) has one source line per instruction (127 lines for 127 instructions) and the same name and 0x1FC size. Written as an `asm` function with `nofralloc` in `skinmatrixblend.c`, Gormiti's C compile unit (user's ruling, 2026-11-15). |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate2WeightsP`, `_rwDlSkinUpdate2WeightsPN`, `_rwDlSkinUpdate3WeightsP`, `_rwDlSkinUpdate3WeightsPN`, `_rwDlSkinUpdate4WeightsP`, `_rwDlSkinUpdate4WeightsPN` (6 functions, 2820 bytes) | RpSkin2's GameCube vertex skinning loops, one per count of weights per vertex (2, 3, 4) and per arrays skinned (positions "P", positions and normals "PN"): `dst = sum(w[i] * (M[bone[i]] * src))` over the vertices of a `SkinAsmData` job, matrices from the palette at 64 bytes apiece. Each has a hand-written frame (a bare `stwu`, then `stw r27..r31` and/or `stfd f24..f31` upward from offset 8, no `mflr`, no calls), `psq_lux`/`psq_stux` walks through GQR6/GQR7, `psq_l`/`ps_madds*` bodies software-pipelined by hand around an `mtctr`/`bdnz` loop; the Gormiti DWARF line table (`skingcnasm.c`, RpSkin2.a `skingcnasm.obj`) has one source line per instruction (70, 117, 92, 149, 108 and 169 lines for the same counts of instructions) and the same names and sizes. Written as `asm` functions with `nofralloc` in `skingcnasm.c`, Gormiti's C compile unit (user's ruling, 2026-11-15). |

ORIGINAL-ASM bodies in parked units (listed for the checker; counted nowhere until the unit links):

| unit | function | evidence |
|---|---|---|
| `rwsdk/src/plcore/bamatrix` | `RwMatrixMultiply` (1 function, 252 bytes) | the Wii driver's paired-single matrix product: `psq_l` row pairs with `ps_muls0`/`ps_madds0`/`ps_madds1`, f30/f31 saved with `psq_st` in a hand-written frame, the flags ANDed in between; no C body compiles to it. The module's C product `MatrixMultiply` is only installed in the globals' `multMatrix`; every caller, the module's own included, calls the asm function. bamatrix is parked (ORIGINAL-ASM when it links). |
| `rwsdk/src/plcore/bavector` | `RwV3dTransformPoint`, `RwV3dTransformPoints`, `RwV3dTransformVector` (3 functions, 240 bytes) | the Wii driver's paired-single point and vector transforms: `psq_l` row pairs with `ps_madds0`/`ps_madds1` (`ps_muls0` for the vector), the array form a `psq_lu`/`psq_stu` loop counted in ctr with no zero test; no C body compiles to them. The array transform is `RwV3dTransformPoints` (tgsm/pbg defines it at this place, before `_rwVectorClose`, and its bacamera.c calls it at the same site); the other two names are unknown (game code calls them directly; the module's own C transforms go through the vector globals). bavector is parked (ORIGINAL-ASM when it links). |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `__TRK_get_MSR`, `__TRK_set_MSR`, `TRKInterruptHandler`, `TRKExceptionHandler`, `TRKSwapAndGo`, `TRKInterruptHandlerEnableInterrupts`, `ReadFPSCR`, `WriteFPSCR` (8 functions, 928 bytes) | `mfmsr`/`mtmsr`, `mffs`/`mtfsf` through memory, and the first-level exception, swap and `rfi` paths that run on the target's registers with no C frame; all `asm` functions in zeldaret/tp `targimpl.c`. targimpl is parked (31/33; ORIGINAL-ASM when it links). |
| `revolution/os/OS` | `__OSFPRInit`, `__OSDBIntegrator`, `__OSDBJump`, `OSExceptionVector`, `OSDefaultExceptionHandler` (5 functions, 580 bytes) | `mtmsr`/`mfspr HID2` and the floating-point and paired-single register clears, the debugger integrator and jump stubs, and the first-level exception vector and default handler that run with no C frame; all `asm` functions in the public RVL SDK `OS.c` (source marker at each). OS is parked (ORIGINAL-ASM when it links). |
| `revolution/os/OSExec` | `Run` (1 function, 60 bytes) | `ICFlashInvalidate`, `sync`/`isync`, then `mtctr`/`bctr` to the entry point; an `asm` function in the public RVL SDK `OSExec.c` (source marker at the function). OSExec is parked (ORIGINAL-ASM when it links). |

### Header-level asm inlines (ORIGINAL-ASM)

Header-level `asm` inlines of the original (user's ruling, 2026-11-15, "Rulings of 2026-11-15" below): listed once, by
header path and name; counted neither as matched nor as open, and their callers are not R18 findings. Call sites
count the reconstructed callers; "retail fragment" is `build/tools/fragments.exe in <function>` (the number of retail
functions holding the same instruction sequence).

| unit | function | evidence |
|---|---|---|
| `include/game/VectorPS.h` | `V3dNormalize` | 3 call sites in 2 units (CAmbientInterferencePoint, CDrawingTest); retail fragment `frsqrte`/`fnmsubs`/`ps_muls0` (the Newton step and scale) in 73 functions. |
| `include/game/VectorPS.h` | `V3dLength` | 2 call sites in 2 units (CAmbientInterferencePoint, CScalingObject); retail fragment `ps_madd`/`ps_sum0`/`frsqrte` in 25 functions. |
| `include/game/VectorPS.h` | `V3dDot` | 2 call sites (CDrawingTest). Several-site evidence across units open (open items). |
| `include/game/VectorPS.h` | `V3dSub` | 1 call site (CScalingObject). Several-site evidence open (open items). |
| `include/game/VectorPS.h` | `V3dScale` | 1 call site (CDrawingTest). Several-site evidence open (open items). |
| `include/game/VectorPS.h` | `V3dAdd` | no caller of the header copy yet (unk_8000A040 keeps a unit-local `V3dAdd`, the same body); several-site evidence open (open items). |
| `include/revolution/os/OSFastCast.h` | `OSInitFastCast`, `__OSu16tof32`, `__OSf32tou8`, `__OSf32tou16`, `__OSf32tos16` | the SDK header `OSFastCast.h` (ogws, xbret/xenoblade): GQR set-up and quantised `psq_l`/`psq_st` casts; callers in THPDec, math_triangular and lyt_animation (rows under "Listed inline-asm helpers"). |
| `include/revolution/hbm/nw4hbm/math/arithmetic.h` | `FSelect` | sdk_2009's nw4hbm `arithmetic.h` helper (one `fsel`); called through `Rect::Normalize` from both `CalcRect` instances. |

## Listed inline-asm helpers (R18 original-helper exception; counted)

| unit | function | helper | evidence |
|---|---|---|---|
| `Runtime.PPCEABI.H/__init_cpp_exceptions` | `__init_cpp_exceptions` | `static inline char* GetR2(void)` with `asm { mr toc, r2 }` | Metrowerks runtime idiom from the MSL/runtime source: C cannot name r2, and the original reads the TOC base through this helper to pass it to `__register_fragment`. One instruction, inside an otherwise-C function. |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRK_ppc_memcpy` | two `asm { sync }` statements, with xenoblade's `ppc_readbyte1`/`ppc_writebyte1` (`include/TRK/ppc_mem.h`) | xenoblade's `targimpl.c` text: a `sync` after the load and after the store, written as `asm` statements. The `__sync()` intrinsic gives the same `sync` but lets the compiler share the store lane's shift (retail computes it twice: the asm statements limit optimisation of the function), so the function is 95.3% with the intrinsic and exact with the statements. Unit parked; counts when it is linked. Ruling "MetroTRK rulings (match-trk)". |
| `revolution/base/PPCArch` | `PPCMthid4` | two `asm { mtspr SPR_HID4, newhid4 }` statements | The RVL SDK `PPCArch.c` writes `PPCMthid4` as a C function (the H4A test, the `OSReport` errata message and the bit set are C) with the HID4 write as an `asm` statement in each branch; C cannot name HID4. Counted as C. |
| `revolution/gx/GXTransform` | `__GXSetProjection` | `static void WriteProjPS(const register f32 proj[6], register volatile void* dest)` | zeldaret/tp `GXTransform.c` has the helper with the same name, parameters and six paired-single moves (`psq_l` x3 from the matrix, `psq_st` x3 to the write-gather port); ogws has it too (`inline`, swapped parameters). C cannot name paired-single moves. Written `asm volatile` (ruling "GX rulings (sdk-gx)"). Unit parked; counts when it is linked. |
| `revolution/gx/GXTransform` | `GXSetProjectionv`, `GXGetViewportv` | `static void Copy6Floats(const register f32 src[6], register volatile f32* dest)` | tp's and ogws's helper: three paired-single copies. Same notes as `WriteProjPS`. |
| `revolution/gx/GXTransform` | `GXGetProjectionv` | `static void LoadProjPS(register f32* dest)` | PROVISIONAL. Only ogws has it, marked there "Fake inline": tp's `GXGetProjectionv` copies the six floats in C. Retail's release `GXGetProjectionv` does the copy with three `psq_l`/`psq_st` pairs off `__GXData`, which C does not compile to, so the paired-single code is the SDK's; the helper's name and shape are ogws's guess. Listed as the SDK's asm; the wrapper shape is an open item. |
| `revolution/gx/GXTransform` | `GXLoadPosMtxImm`, `GXLoadTexMtxImm` | `static void WriteMTXPS4x3(const register f32 mtx[3][4], register volatile f32* dest)` | tp's and ogws's helper (six `psq_l`, six `psq_st` to the port). |
| `revolution/gx/GXTransform` | `GXLoadNrmMtxImm` | `static void WriteMTXPS3x3from3x4(const register f32 mtx[3][4], register volatile f32* dest)` | tp's helper (ogws lacks it): the 3x3 part of a 3x4 matrix as `psq_l`/`lfs` then `psq_st`/`stfs`. tp has no `const` on `mtx` and casts at the call; the `const` here is byte-neutral. |
| `revolution/gx/GXTransform` | `GXLoadNrmMtxImm3x3` | `static void WriteMTXPS3x3(const register f32 mtx[3][3], register volatile f32* dest)` | tp's and ogws's helper. |
| `revolution/gx/GXTransform` | `GXLoadTexMtxImm` | `static void WriteMTXPS4x2(const register f32 mtx[2][4], register volatile f32* dest)` | tp's and ogws's helper. |
| `revolution/gx/GXLight` | `GXLoadLightObjImm` | `static inline void PushLight(const register GXLightObj* lt_obj, register volatile void* dest)` | tp's text verbatim (`static inline`, register locals, three zero words, the colour, six paired singles), but written `asm volatile`. ogws's equivalent `WriteLightObj` hard-codes r6 and is marked a fake match there; this version names every register. Unit parked. |
| `revolution/gx/GXInit` | `IsWriteGatherBufferEmpty` | `asm BOOL IsWriteGatherBufferEmpty(void)` | tp's whole `asm` function verbatim (`sync`, `mfspr r3, WPAR`, `andi.`); WPAR has no C spelling. Dead-stripped in retail: no address, counted nowhere (it would be ORIGINAL-ASM if a copy existed). Source marker `// ASM-HELPER` at the function. |
| `revolution/thp/THPDec` | `THPInit` | `static inline void OSInitFastCast(void)` (`include/revolution/os/OSFastCast.h`) | The SDK header `OSFastCast.h` (doldecomp/ogws `include/revolution/OS/OSFastCast.h`, the same four `li`/`oris`/`mtspr` triples for GQR2..GQR5); a GQR has no C spelling. Moved from a local copy in THPDec.c to the OS header in the SDK form (byte-neutral, clean gate). Source marker `// ASM-HELPER` at the function. |
| `revolution/hbm/nw4hbm/math/math_triangular` | `SinFIdx`, `CosFIdx`, `Atan2FIdx` | `static inline f32 __OSu16tof32(register const u16*)`, `static inline u16 __OSf32tou16(register f32)` (`include/revolution/os/OSFastCast.h`), through nw4hbm `U16ToF32`/`F32ToU16` | The SDK header `OSFastCast.h` (xbret/xenoblade `libs/RVL_SDK/include/revolution/os/OSFastCast.h`): one `psq_l`/`psq_st` through GQR3 (u16); a quantised load or store has no C spelling. The SDK wrappers `OSu16tof32`/`OSf32tou16` declare their out parameter `volatile`, as here (SDK form, not a carrier). Source marker `// ASM-HELPER` at the functions. |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `Animate__Q36nw4hbm3lyt18AnimTransformBasicFUlPQ36nw4hbm3lyt4Pane`, `Animate__Q36nw4hbm3lyt18AnimTransformBasicFUlPQ36nw4hbm3lyt8Material` | `static inline u8 __OSf32tou8(register f32)`, `static inline s16 __OSf32tos16(register f32)` (`include/revolution/os/OSFastCast.h`), through `OSf32tou8`/`OSf32tos16` | The SDK header `OSFastCast.h` (xbret/xenoblade `libs/RVL_SDK/include/revolution/os/OSFastCast.h`): one `psq_st` through GQR2 (u8) or GQR5 (s16); a quantised store has no C spelling. The wrappers keep the SDK's `volatile` out parameter, as for `OSf32tou16`. Source marker `// ASM-HELPER` at the functions. |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `CalcRect` (char and wchar_t instances) | `inline f32 FSelect(register f32 cond, register f32 ifPos, register f32 ifNeg)` (`include/revolution/hbm/nw4hbm/math/arithmetic.h`), through `Rect::Normalize` | sdk_2009's `nw4hbm/math/arithmetic.h` helper verbatim (one `asm { fsel }` on `register` floats). Wii/1.0a has no single-precision `fsel` intrinsic (`__fsels` is undefined) and `__fsel` returns double: each of the four results gains an `frsp` (CalcRect 91.9%). Source marker `// ASM-HELPER` at the function. |
| `revolution/thp/THPDec` | `__THPDecompressYUV` | `static inline void __THPGQRSetup(void)`, `static inline void __THPGQRRestore(void)` | mkdd `THPDec.c` has both with the same names and bodies (`mfspr`/`mtspr` of GQR5/GQR6 around C stores of the saved values). Source markers at the functions. |
| `revolution/thp/THPDec` | `__THPHuffDecodeDCTCompY`, `__THPHuffDecodeDCTCompU`, `__THPHuffDecodeDCTCompV` (ORIGINAL-ASM) | `inline s32 __THPHuffDecodeTab(register THPFileInfo* info, register THPHuffmanTab* h)` | mkdd's helper (asm blocks around the table walk). Its callers are ORIGINAL-ASM, so it adds nothing to the counts. Source marker at the function. |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinBlendBodyP`, `_rpSkinBlendBodyPN` | `asm { mfspr tmp, GQR5 }`, `asm { mtspr GQR5, weightGQR; mtspr GQR6, posGQR }` (PN: also `mtspr GQR7, normGQR`) and `asm { mtspr GQR5, tmp }` | Special-purpose register moves only (user's ruling, 2026-11-16): GQR5 to GQR7 have no C spelling. The Gormiti DWARF (`skin2/gcn/skingcn.c`, Debug and Final) shows C functions with the locals `gqr5value`, `gqr5address`, `tmp`, `weightGQR`, `posGQR` (PN: `normGQR`), modelled under those names; the save and restore of GQR5 through `gqr5address` are C statements (the Debug build stores and reloads through it on lines of their own; Final optimises `gqr5value` away, as retail does). The asm operands are `register` locals (MWCC error 34001 otherwise). Source marker `// ASM-HELPER` at both functions. Counted as C. |

## Review rulings (counted)

| function | rule | ruling |
|---|---|---|
| `__destroy_new_array` | R4 | `*(size_t*)((char*)block - ARRAY_HEADER_SIZE)` reads the `new[]` array header in front of the block. R4 allows byte-pointer arithmetic on allocator headers; the expression is the original Metrowerks source. |
| `OSAllocFromHeap`, `OSFreeToHeap` | R4 | `Cell` header arithmetic (`(char*)cell + HEADERSIZE`): allocator headers, allowed by R4; original SDK form. |
| `OSSetCurrentHeap`, `OSInitAlloc` | R17 | `__OSCurrHeap` is `volatile` as in the SDK header; the declaration carries a `// volatile:` evidence note. |
| `IPCReadReg`, `IPCWriteReg` | R17 | `__IPCRegs` is a hardware register block (`vu32`); evidence note at the declaration. |
| `make_crc_table` | R17 | zlib 1.2.3 crc32.c: `static volatile int first` is upstream's guard against concurrent table builds, in the `DYNAMIC_CRC_TABLE` branch this build does not compile (the function is not in the binary); evidence note at the declaration. |
| `OSSetErrorHandler` | R4 | `*(u64*)&thread->context.fpr[i] = 0xFFFFFFFFFFFFFFFFULL` (and `psf[i]`) stores an all-ones bit pattern into a `f64` member through a type pun. Not offset arithmetic: the member is named and typed; the pun writes a NaN pattern C has no literal for. Original SDK form. |
| `__OSInitAudioSystem` | R4, R17 | `*(vu32*)&__DSPRegs[16]` (`DSP_DMA_MMADDR`, `_ARADDR`, `_CNT`) writes a 32-bit DMA register pair of the 16-bit DSP block in one store. An indexed element of the typed register block, not a byte offset; `volatile` because it is a hardware register (evidence note on each macro). Original SDK form. |
| `__AIClockInit`, `__OSStopAudioSystem`, `__OSUnhandledException` | R17 | `__DSPRegs`, `__DIRegs` and `__ACRRegs` are hardware register blocks (`vu16`/`vu32`); evidence notes at the declarations in `revolution/hardware.h`. |
| `NWC24iOpenResourceManager_`, `NWC24iCloseResourceManager_`, `NWC24iIoctlResourceManager_`, `NWC24iIoctlResourceManagerAsync_`, `CheckCallingStatus`, `NWC24ResumeScheduler` | R17 | Each resource-manager helper takes `callerName` and reads it with one `(void)callerName;` statement at the place of the reference's debug report, with the comment `// Debug build: report at this point (zeldaret/tp libs/revolution/src/os/OSNet.c)`. That read is what materialises the caller's `__FUNCTION__` literal in retail `.data` (the sibling's `OSNet.o` names the objects `__FUNCTION__$NNN`, one per caller, and its Release build has the same `.data` order and sizes); without it the compiler drops the literals. User's ruling of 2026-11-16 (AGENTS.md, "Release-compiled-out debug checks"): a parameter that only a compiled-out debug report reads. `NWC24ResumeScheduler` is the dead-stripped twin that owns the fifth literal (user's ruling of 2026-11-16, dead-stripped functions without reference text). Evidence: `orig/_ref/gormiti_wii/Exe/MAP/GormitiRelease.MAP`, `os.a OSNet.o`. |

## Open items that do not block a match
- **`game/unk_800ABDE4` unit name unconfirmed (R7):** `src/game/unk_800ABDE4.cpp` (0x800ABDE4-0x800ABE6C, one function, fn_800ABDE4: the name lookup over the list at `lbl_8046EA70`, control-flow wall). Its `.bss` (lbl_8046EA70, shared with the unsplit neighbours fn_800AB754, fn_800ABD5C and fn_800ABDA0) is taken by this unit. Stays NonMatching.
- **Game unit name unconfirmed (0x800AEF7C):** `game/unk_800AEF7C.cpp` (0x800AEF7C-0x800AEFE4, one compiler-generated
  static initialiser, `fn_800AEF7C`). It is the `__sinit` of the singleton `lbl_8046EA70` (sub-object at +0x118 of the
  block `lbl_8046E958`; semaphore at +0x100, registration node at +0x10C). The methods of that singleton lie in the
  unsplit range 0x800AB6C8-0x800AEF7C (`auto_03`), so the owning translation unit is not yet recovered. The unit stays
  NonMatching as a stub.
- **`game/unk_800B28E0` unit name unconfirmed (R12, R7; R13 open):** `src/game/unk_800B28E0.cpp` (0x800B28E0-0x800B290C, four virtual accessors, all exact). The name is invented from the address. The accessors belong to the placeholder classes `unk_800B1754` (vtable lbl_803EA524) and `unk_800B2E58` (lbl_803EA5E8), declared partially in `include/game/unk_800B1754.h` and `include/game/unk_800B2E58.h`. R13 stays open at `unk_800B1754` slots 0x68 and 0x6C, the overrides of its second base (the vptr at 0x4), which is not modelled. The unit stays NonMatching until those slots and the base are declared by the class owner.
- **Game unit name unconfirmed (fn_800AF0B4):** `game/unk_800AF0B4.cpp` (0x800AF0B4-0x800AF100, one function, exact: `fn_800AF0B4(obj, limit)` returns `fn_801B0F5C(lbl_8052A5C0, obj) >= limit` as an unsigned compare). Named after its address; the boundary is the unit plan at confidence 0.16 with no sibling object. The callee `fn_801B0F5C` (returns the word at 0x18 of its second argument) is declared in `include/game/unk_ResourceManager.h` and defined in its unit (not yet reconstructed). The parameter `obj` is `void*`: its type is known only from that read.
- **Game unit name unconfirmed (0x800AF050):** `game/unk_800AF050.cpp` (0x800AF050-0x800AF064, one function, `fn_800AF050`:
  a five-instruction tail call to `fn_801B0B8C` with the resource manager `lbl_8052A5C0`, the handle and flags 1, exact). It
  owns the eight-byte `.sbss` object `lbl_80529888` (declared in `include/game/unk_800AF050.h`, layout not reconstructed),
  which `fn_800A93DC` also passes as an owner. The file name and the parameter name `handle` are invented; no class or
  string evidence names them.
- **Game unit name unconfirmed (0x800AF100):** `game/unk_800AF100.cpp` (0x800AF100-0x800AF188, one function,
  `fn_800AF100`, matched exactly). It reads up to `size` bytes from the async stream through `fn_801B10AC`, a
  ring-buffer read in the unit at 0x801B0120 (not reconstructed; declared in `include/game/unk_800AF100.h` with
  `lbl_8052A5C0` as its first argument), then byte-swaps the words read. `unk_AsyncStream` is an invented name for
  that stream object; no string names the file.
- **`game/unk_800865DC` unit name unconfirmed (R12, R7):** `src/game/unk_800865DC.cpp` (0x800865DC-0x80087614, nineteen
  functions) carries the "unit name unconfirmed" comment; no string or class name names it. The unit stays NonMatching
  and its unreconstructed bodies are stubs. Its end is 0x80087614, where CHitEffectManager starts.
  The two behaviour vtables `lbl_803E7F90` and `lbl_803E7F38` (22 slots each, classes
  unnamed) sit in its `.data`.
- **Game unit name unconfirmed (0x800B0C64):** `game/unk_800B0C64.cpp` (0x800B0C64-0x800B0DEC, one compiler-generated static initialiser, `__sinit_\unk_800B0C64_cpp`, matched exactly). It defines the fourteen message ids `lbl_805298A0`-`lbl_80529908` (placeholder class `unk_801A92A8`, declared in `include/game/unk_800B0C64.h` because many units read them) and owns the compiler's registration nodes `lbl_8046EA98`. Placeholder names for the ids; rename the unit and the ids when evidence appears.
- **Game unit name unconfirmed (0x8009996C):** `game/unk_8009996C.cpp` (0x8009996C-0x800999A8, one compiler-generated
  static initialiser, `__sinit_\unk_8009996C_cpp`, matched exactly). It constructs the message id `lbl_80529758`
  (`unk_801A92A8`, declared in `include/game/unk_8009996C.h` because the code before it in `.text` uses it) and registers
  the destructor of the compiler's `lbl_8046A738` node. The owner file of the code before it is not split yet.
- **Game unit name unconfirmed (RestoreGXState__Fv):** `game/unk_80099EB8.cpp` (0x80099EB8-0x8009A024, one function: the GX state restore, 91 instructions, exact). The Gormiti sibling holds this function in `ROMFontText.cpp` with its font routines, which does not confirm this unit's boundary. Its neighbour fn_80099E74 (0x80099E74-0x80099EB8, a message-queue receive on `lbl_8046BAE8`, shared with `AudioDecode`) is not in this unit; it belongs with the audio decode unit.
- **Unit name unconfirmed (THP, lane L0213):** `revolution/thp/THPAudioDecode.c` (0x80099C64-0x80099CC8, two functions: `fn_80099C64`, `AudioDecoder`). The file name is the Gormiti (Debug) link map's object `THPAudioDecode.obj`; the two functions do not pair with it by body (`AudioDecoder` is 0x28 bytes here, 0x2C there), so the name is a reading aid. Both bodies match. `lbl_8046A7B0` (the decode thread) and `lbl_80529770` (its flag) are declared in `include/revolution/thp/__thp.h` and owned by the unit that holds `CreateAudioDecodeThread`; `fn_8009C1A4`, `AudioDecode` and `fn_8009C220` are declared there too, with opaque buffer pointers, pending their owners.
- **Game unit name unconfirmed (fn_8009B840):** `game/unk_MovieFrame.cpp` (0x8009B840-0x8009B97C, one function, exact): the movie-frame draw that waits up to ten 10 ms sleeps for the first frame in `lbl_8046BB40`, then calls `fn_8009A024`, `fn_8009A4CC` and `RestoreGXState`. The file name is invented; the boundary is the unit plan at confidence 0.16, with no sibling object. `lbl_8046BB40` (0x200 bytes, owner not yet known) is declared extern in `include/game/unk_MovieFrame.h`: the fields it reads are named, the rest are unknown runs. The parameter types of `fn_8009A4CC` and of the first argument of `fn_8009A024` are placeholders.
- **Game unit name unconfirmed (fn_8009C308):** `game/unk_8009C308.cpp` (0x8009C308-0x8009C324, one function: resumes the video decode thread when its start flag `lbl_805297A0` is set; caller THPPlayerPrepare). Named after its address; the boundary is the unit plan at confidence 0.16. The function's role is invented. The globals `lbl_805297A0` and `lbl_8046D4D0` are declared in `include/game/unk_8009C308.h` and owned by the unsplit unit that holds CreateVideoDecodeThread.
- **Game unit name unconfirmed (unk_800A292C):** `game/unk_800A292C.cpp` (0x800A292C-0x800A2B78, three functions: the cached lookup `fn_800A292C` and the table find `fn_800A2A18`, which `fn_800A292C` inlines as `FindEntry`, and the append `fn_800A2AB8`; names invented, no string evidence, no sibling object). `fn_800A2A18` and `fn_800A2AB8` are exact. `fn_800A292C` is parked at 99.49%: the inlined `FindEntry` swaps the id and offset registers (register/schedule residue). The table types live in `include/game/unk_8005549C.h`, where this unit added `index` at 0x04 and `capacity` at 0x08 from its asm.
- **Game unit name unconfirmed (unk_800A4098):** `game/unk_800A4098.cpp` (0x800A4098-0x800A40B4, one function: the
  RpClumpForAllAtomics callback fn_800A4098 that the constructor of `unk_800A40B4` passes; names invented). The unit
  boundary is the unit plan's gap between two neighbouring units, with no sibling object. Exact.
  `unk_800A40B4` is declared here with only the members the callback writes; its constructor lane owns the rest.

- **SDK unit name unconfirmed (THPRead, 0x8009C014):** `revolution/thp/THPRead.c` (0x8009C014-0x8009C030, `fn_8009C014` only so far: the flag-guarded resume of the THP read thread) is named without string evidence; its boundary is the unit plan at confidence 0.16; the unit belongs to the RVL THP library. The read thread's objects are shared with `CreateReadThread` and the functions around it: `lbl_8046C1B8` (the OSThread and its stack, declared in `include/revolution/thp/__thp.h`) is the referent here, while `CreateReadThread` reaches the same address as `lbl_8046C0E0` plus 0xD8. The one-object or two-object model needs a single decision before those functions are reconstructed.
- **Climax unit name unconfirmed (0x8002039C):** `climax/unk_InterpCache.cpp` (0x8002039C-0x80020564, one function: the
- **Game unit name unconfirmed (fn_80099E60):** `game/unk_80099E60.cpp` (0x80099E60-0x80099E74, one function: a five-instruction wrapper that posts its message to the OSMessageQueue at lbl_8046BAC8 with OS_MESSAGE_NOBLOCK). Named after its address; the sibling pairs no counterpart. The queue owner is the AudioDecode unit (not reconstructed), so the global is declared in `include/game/unk_80099E60.h` only. The range sits in the THP player and audio area (THPPlayer* dtk names at 0x8009A850 onward), so the library and file name (`thp` or `revolution/thp/THPPlayer.c`) may replace it once that unit is reconstructed; the lane file lib `game/G3` is not a configure.py library.
- **Game unit name unconfirmed (0x800AF210):** `game/unk_800AF210.cpp` (0x800AF210-0x800AF298, one function: a resource read through `fn_801B10AC` on `lbl_8052A5C0`, then a float byte-swap loop over the result via `fn_801B6CAC`; the Gormiti sibling pairs no counterpart). The unit stays NonMatching at 97.06%: the one unexplained row is retail's dead copy of the third argument into r6 before the first call (wall class: other). The callers in `auto_03_800AB6C8` and `auto_03_800B5E94` are not reconstructed.
- **Game unit name unconfirmed (0x800AF03C):** `game/unk_800AF03C.cpp` (0x800AF03C-0x800AF050, one function: a five-instruction tail call to fn_801B0A88 with the resource manager `lbl_8052A5C0`, the caller's handle, the 8-byte object `lbl_80529888` and flag 1). Named after its address; no string or class-name evidence names it. The callee (unit 0x801B0120, not reconstructed) does not read its third argument, so `include/game/unk_ResourceManager.h` declares it unused. `lbl_80529888` is declared as `u8[8]` in `include/game/unk_800AF03C.h` until its owning unit is reconstructed.
- **Game unit name unconfirmed (0x8009A024):** `game/unk_8009A024.cpp` (0x8009A024-0x8009A730, two functions, no class or string evidence). Parked NonMatching: `fn_8009A024` at 97.8% (the colour-word stack slots and the width/height register binding), `fn_8009A4CC` at 91.6% (parameter-to-register binding). The `C_MTXOrtho` prototype was added to `include/revolution/mtx.h`.
  destructor of an interpolator-cache class `unk_InterpCache`, names invented; the constructor is not in this unit). The
  vtable `lbl_803E0D80` is shared with other units (not renamed here). Parked NonMatching at 99.12%: retail repeats the
  this-null test (0x800203C4, dead) which is not reproduced; the vptr relocation names differ. `FindInterpNode` is an
  invented static helper for the search loop.
- **THP player unit name unconfirmed (0x80099B78):** `revolution/thp/THPPlayer.c` (0x80099B78-0x80099C48, one function,
  `CreateAudioDecodeThread`, parked NonMatching at 85.25%). The file name is inferred from the THP player symbols
  (`THPPlayerQuit`, `THPPlayerClose`), not from a string. The block `THPAudioDecodeBlock` (`lbl_8046A7B0`, name invented,
  layout from the function) and `lbl_80529770` are owned here; the decoder functions `AudioDecoder` and
  `AudioDecoderForOnMemory` are declared in `include/revolution/thp/__thp.h`. The residue: retail's stack-top fold and the
  priority copy's position (wall: other, register/schedule).

- **Game unit name unconfirmed (unk_8003C3FC):** `game/unk_8003C3FC.cpp` (0x8003C3FC-0x8003C454, four callbacks: no class, no string evidence; the boundary is the unit plan at confidence 0.87, with no sibling object). All four are exact. The second argument's type `unk_8003C454` and the record type `unk_ActorSlot` (field 0x220) are inferred from the callbacks' reads; the record's source type `unk_ActorSlotSource` is a placeholder layout (its field at 0x38 is unnamed).

- **Game unit name unconfirmed (fn_800ABADC):** `game/unk_800ABADC.cpp` (0x800ABADC-0x800ABBDC, one function: appends a named
  entry to the registry `lbl_8046EA70`, a 0x10-byte list whose owner unit is not yet fixed; this unit defines it, and the
  other units that read it declare it extern). The registry, entry and node names are invented; the entry layout (name
  0x100, handle, refCount, release) is retail's. No string or class evidence names the file.

- **Game unit name unconfirmed (fn_80043658):** `game/unk_80043658.cpp` (0x80043658-0x800436D8, seven virtual slots of `unk_8003C454`) is named after its address; no string or class evidence names the file, and the boundary is the unit plan at confidence 0.16 with no sibling object. `fn_801B6188` (the case-insensitive prefix compare that fn_80043658 calls with "IGC") is declared in `include/climax/Utils.h` and not yet owned by a unit. The slot `field_0x220` of `unk_8003C454` is typed `unk_8000F80C**` from fn_80043658 alone.

- **Game unit name unconfirmed (fn_801A7BFC, unk_80055174):** `game/unk_80055310.cpp` (0x80055310-0x80055358, one function: the zone-change override of unk_80055174, which calls `fn_80030B68` with the respot manager `lbl_80529260` and then the base override `fn_80042F18`) is named after its address; the boundary is the unit plan at confidence 0.52, with no sibling object. Both callees are declared extern "C" in the unit file and defined in their own units (not yet reconstructed).

- **Game unit name unconfirmed (fn_800ABD5C):** `game/unk_800ABD5C.cpp` (0x800ABD5C-0x800ABDA0, one function: the registry lookup-and-bump, exact). The lookup is the inline `unk_FindRegistryEntry` in `include/game/unk_800ABD5C.h`, and its decrement twin fn_800ABDA0 is outside this unit. The registry global `lbl_8046EA70` (a list head at 0x8) has no owner unit yet: it is shared with fn_800AB754 and its .bss object is left to the asm object, so the header declares it `extern` only. The boundary is the unit plan at confidence 0.16 (one function), with no sibling object. The type names `unk_RegistryEntry`, `unk_RegistryNode` and `unk_RegistryHead` are invented.

- **Game unit name unconfirmed (fn_80053C40):** `game/unk_80053C40.cpp` (0x80053C40-0x80055174, 25 functions). The first function is a constructor that stores the vtable at `lbl_803E38F0`, so the file is named after the constructor address (class `unk_80053C40`, invented). Scaffolded with `--data none` (the data hull overlaps game/CPreCache.cpp and game/CActorTest.cpp); it carries the `.ctors` entry of fn_80055084. Its `.data`, `.sdata`, `.sbss` and `.sdata2` objects are not yet owned by this unit.
- **Game unit name unconfirmed (unk_Trail):** `game/unk_Trail.cpp` (0x800817C0-0x80081928, two functions: the trail placeholder callback `fn_800817C0` and the trail clump builder `fn_80081858`) is named after its role; no string names the file. The struct `unk_Trail` is invented, and the owner is not confirmed: symbols.txt names Effects::Trail beside this code (`AddSpan`, `FadeToKill`), but these two functions do not prove membership. Its caller `fn_8007F514` is in `main/auto_03_8007EFE4_text`. The declaration `fn_801D9074` (in `main/auto_03_801D7D54_text`) and the string `lbl_803E7A38` ("trail_placeholder") are in this unit's .data range.
- **Game unit name unconfirmed (fn_80077C9C):** `game/unk_80077C9C.cpp` (0x80077C9C-0x80078EFC, 56 functions: lookup functions over `unk_Lookup`, one cache and one record per lookup, and empty leaves) is named after its address; no string or class evidence names it. The `unk_Lookup` layout is invented from the lookups' offsets in `include/game/unk_80077C9C.h`. The 28 functions with the R30 declaration move are listed in the RULE-EXCEPTION table.
- **Game unit name unconfirmed (fn_80095030):** `game/unk_80095030.cpp` (0x80095030-0x80095108, one function: a render pass that clears its camera, sets the TEV callbacks of its clump and flushes the camera image) is named after its address; no string or class evidence names the file or the class. Its class `unk_RenderPass` (name invented) has its layout read from the function and its caller `fn_80094E6C` only: the fields at 0x10, 0x28, 0x48, 0x58 and 0x6C are typed, the rest are placeholders. `lbl_8046A6E8` (the TEV handle the function registers callbacks on) has no owner unit yet, so `include/game/unk_80095030.h` declares it without a definition.
- **Game unit name unconfirmed (fn_8009C030):** `game/unk_8009C030.cpp` (0x8009C030-0x8009C06C, one function: it cancels the video read thread when it was created and clears its flag; `CreateReadThread` at 0x8009BF74, unassigned, is its counterpart). The boundary is the unit plan at confidence 0.16 with no sibling object, and the lane's `game/G3` is not a configure library (the game library is `game`). The owned data is the read thread `lbl_8046C1B8` (an `OSThread` followed by its 0x1000-byte stack, type `unk_ReadThread` invented) and the flag `lbl_80529798`, both declared in `include/game/unk_8009C030.h`; `CreateReadThread` and the other readers are not yet assigned a unit. `fn_8009C030` keeps its address name (no evidence). Exact.
- **Game unit name unconfirmed (fn_8009EFA0, unk_ClumpSwapList):** `game/unk_ClumpSwapList.cpp` (0x8009EFA0-0x8009F34C, four functions: the list's constructor-factory, its add, remove and lookup). The class `unk_ClumpSwapList` is invented (role name); no string or class-name evidence names the file, and the boundary is the unit plan at confidence 0.16 with no sibling object. The three member functions are not yet exact (register residue of the index loop; see the function banners); the unit stays NonMatching.
- **Game unit name unconfirmed (fn_800AEFE4):** `game/unk_800AEFE4.cpp` (0x800AEFE4-0x800AF03C, one function: a resource request under the owner key `lbl_80529888`, with priority 0.0f; exact). Named after its address; the boundary is the unit plan at confidence 0.16, with no sibling object and no string evidence. `lbl_80529888` (8 bytes of .sbss, also passed as the owner by the functions at 0x800A8F88 and 0x800A93DC) is defined in this unit and declared in `include/game/unk_800AEFE4.h`. The second argument is passed as 1 by the caller fn_800AC7AC and is not read. The 0.0f literal is the compiler's `lbl_8052C440` constant. The comment in `include/game/unk_800C8598.h` that names this unit as a writer of `lbl_80527508` does not match the retail code here (the function writes no such global).
- **Game unit name unconfirmed (fn_80015F8C):** `game/unk_80015F8C.cpp` (0x80015F8C-0x80016074, one function: a
  name lookup in a 0x3c-byte entry table, then the binding of the entry) is named after its address; no string or class
  evidence names the file, and its table types (`unk_NameTable`, `unk_NameEntry`, `unk_NameDesc`) are invented. The
  caller `fn_80014CA8` is in `main/auto_03_800129E0_text`. `fn_8001590C` (called here) and the global `lbl_8052A5C0`
  (`.sbss`, shared with `fn_8001590C`) are declared in `include/game/unk_80015F8C.h` and not yet owned by a unit.
- **Game unit name unconfirmed (fn_800AF098):** `game/unk_800AF098.cpp` (0x800AF098-0x800AF0B4, one function: a three-argument forward to `fn_801B10AC` with the resource manager `lbl_8052A5C0` as its first argument) is named after its address; no string or class evidence names it. The argument names and types are invented (the callers, in `main/auto_03_800AB6C8_text` and `main/auto_03_800B5E94_text`, pass a pointer, a pointer offset from it and a size). `fn_801B10AC` is declared in `include/game/unk_800AF098.h`, not yet reconstructed. The boundary is the unit plan at confidence 0.16, with no sibling object.
- **Game unit name unconfirmed (fn_8001590C):** `game/unk_8001590C.cpp` (0x8001590C-0x80015F8C, four functions; the boundary is the unit plan at confidence 0.16, with no sibling object). fn_80015E98, fn_80015C10 and fn_80015D2C are reconstructed with register residue only (98.6%, 94.5%, 94.8%); fn_8001590C is open (stub). The registry type `unk_EntryRegistry` and its fields are placeholders, and the callees fn_800149C8, fn_80012E18 and fn_8001397C are declared in `include/game/unk_8001590C.h`.
- **Game unit name unconfirmed (fn_800438A8):** `game/unk_800438A8.cpp` (0x800438A8-0x80043950, two functions: the
  cached lookup `fn_800438A8` and the empty `fn_8004394C`) is named after its address; no string, class-name or sibling
  evidence (the Gormiti pairing finds neither function). The placeholder classes `unk_LookupCacheOwner`, `unk_FlagHolder` and
  `unk_LookupRecord` (`include/game/unk_800438A8.h`) carry offset-derived field names; the owning class is unknown.
- **Game unit name unconfirmed (fn_8007F044):** `game/unk_8007F044.cpp` (0x8007F044-0x8007F20C, one function: the quad batch draw that fn_8007F20C calls for each list node; the boundary is the unit plan at confidence 0.16, with no sibling object). fn_8007F044 is NonMatching at the register/schedule wall (79.96%, control flow isomorphic). `unk_Quad` and `unk_QuadBatch` (include/game/unk_8007F044.h) and the two unused parameters are invented; the list head lbl_80529608 is unnamed.
- **Game unit name unconfirmed (fn_80016644):** `game/unk_80016644.cpp` (0x80016644-0x80016678, one function) is named after
  its address. `fn_80016644` is slot 13 of the vtable `lbl_803E07F0` (constructor `fn_80016200`, class not yet
  reconstructed), so the true form is a virtual member of that class; it is written as a free `extern "C"` dispatcher
  with untyped `self` and message parameters until the class is recovered. Its `.sbss` words `lbl_805291D0` and
  `lbl_805291D4` are owned by the unit of `fn_80016634`. The rule checker reports only R7 names.
- **Game unit name unconfirmed (unk_800174B8):** `game/unk_800174B8.cpp` (0x800174B8-0x80017570, 19 slot functions of
  the unk_8009D2E4, unk_803E8E64 and unk_80024158 interfaces) is named after its address; no string or class evidence
  names the file. `fn_8009ECB0` is an identity open item (its second argument is a 16-byte out vector; the position
  accessor it calls, `fn_8009CD14`, is a C-linkage copy of CBaseBehaviour's `fn_8009CD08`).

- **Game unit name unconfirmed (fn_8001A5D4):** `game/unk_NamedEntryTable.cpp` (0x8001A5D4-0x8001A848, five functions:
  the lookups of a table of named entries, by index and by case-folded name). No string or class evidence names the
  file or the table; the struct `unk_NamedEntryTable` and `unk_NamedEntry` in `include/game/unk_NamedEntryTable.h` are
  invented. Its callers (fn_800191B4 of unk_8001777C, fn_80113540, fn_801134E8) are not reconstructed yet. `fn_8001A638`
  and `fn_8001A674` are register/schedule walls (see their header comments); the other three are exact.
- **Game unit name unconfirmed (fn_80020564):** `game/unk_8004BC94.cpp` (0x80020564-0x80020730, one destructor, `__dt__12unk_8004BC94Fv`; not matched). The class is named after its constructor fn_8004BC94 (the vtable lbl_803E0C60 stores it); the base vtable lbl_803E0D80 is stored by the body and is not declared (base class not reconstructed), and the duplicate guards and register numbering of the inlined base destructor are not reproduced. The unit stays NonMatching.
- **Game unit name unconfirmed (__sinit_\unk_80095108_cpp):** `game/unk_80095108.cpp` (0x80095108-0x80095154, one compiler-generated start-up function) is named after its address; no string or class evidence names the file. Its only source is the definition of the global handle `lbl_8046A6E8` (a `RenderState::cTevCBHandle` built at priority 1.5, its destructor registered with `lbl_8046A6D8` as the registration node), declared in `include/game/unk_80095108.h`. The global is used by `fn_80095030` (`main/auto_03_800929F0_text`), which is not yet reconstructed. The unit stays NonMatching: its flip is red because the built .bss ends 8 bytes short of retail at 0x8046A6F8 (retail puts lbl_8046A700 at 0x8046A700), which shifts every later .bss reference by 8; no evidence names those 8 bytes.

- **Game unit name unconfirmed (fn_800221F8):** `game/unk_800221F8.cpp` (0x800221F8-0x80022200, one function) is named
  after its address; no string or class evidence names the file. `fn_800221F8` is the callback slot of a morph controller
  (stored at +0x118 of the 0x120-byte object built at 0x80022900 and called as (context, atomic, deltaTime)) and forwards
  to `RpDMorphAtomicAddTime`. The owning class is not yet reconstructed.
- **Unit name unconfirmed (game lane L0096, unk_8002A9A4):** `game/unk_8002A9A4.cpp` (0x8002A9A4-0x8002AF94, four functions) is named after its address; no string or class evidence names the file. `fn_8002AD78`, `fn_8002AE2C` and `fn_8002AEF0` match; `fn_8002A9A4`  (the counted-pointer object pair, 66% in match) is open. Its constructor `fn_8012BDB0` is declared extern "C" in  `include/game/unk_8002A9A4.h` and defined in unit auto_03_8012BD58 (not yet reconstructed); the class `unk_8012BDB0`  is named after that address. The lookups and list types are placeholders (`unk_NameIndex`, `unk_RefList`).
- **Unit name unconfirmed (fn_8002C8D0; lane L0109):** `game/unk_8002C8D0.cpp` (0x8002C8D0-0x8002C9DC, one virtual of
  unk_8002BC7C) is named after its address; no string or class-name evidence names the file. The class is named after its
  constructor fn_8002BC7C (vtable lbl_803E1788); only the members the function reads are declared in
  `include/game/unk_8002C8D0.h`. The function is NonMatching (register/schedule wall, see its header comment).
- **Game unit name unconfirmed (fn_8002AF94):** `game/unk_8002AF94.cpp` (0x8002AF94-0x8002B368, one function) is named
  after its address; no string or class evidence names the file. Its body is exact and the unit is parked NonMatching:
  the gate reports the object's 8-aligned `.sdata2` and an out-of-line copy of `unk_RefPtr<unk_80120188>::~unk_RefPtr`.
  The classes `unk_80120188` (constructor fn_80120188, a 0x28-byte timed object whose base vtable 0x803F6490 is not yet
  reconstructed) and the list types `unk_EndList`/`unk_EndNode` are invented; `unk_RefPtr` gained an owning copy
  constructor in `include/game/unk_HUDRequest.h`.
- **`game/unk_8002D384.cpp` (unit name unconfirmed, NonMatching):** the file name is the address (no class-name evidence). Its one function `fn_8002D384` (0x8002D384-0x8002D480, unit_plan heuristic boundary, low confidence) is a free function with an unread second argument. The wall is the owner: retail converts `lbl_80529260` (`unk_RespotManager*`) to `unk_SpawnerResource*` through a null-checked +8 base conversion, so the base relation is not yet recovered and the body uses `reinterpret_cast`. Also open: the name table `lbl_803E1A90`, `lbl_80526A1C`/`lbl_80526A20`/`lbl_80529268` (owners in the unit around 0x8002E364), and `unk_RespotManager` members before 0x64 (`field_0x0` gap).

- **`game/unk_8002D158.cpp` (unit name unconfirmed):** the file name is the first function's address (no class-name or
  string evidence). It holds `fn_8002D158` (a float constant getter) and `fn_8002D160` (a flag test on the object's
  field at 0x394 and a call to `fn_80183114`), both matched. Their object is `unk_8002BC7C` (the constructor's address),
  a placeholder in `include/game/unk_8002BC7C.h` whose vtable `lbl_803E1788` is not declared yet, so the class holds only
  the bytes the two functions read. `fn_80183114` is declared extern "C" with the class pointer until its unit is named.

- **Game unit name unconfirmed (fn_80031F54; lane L0122):** `game/unk_80031F54.cpp` (0x80031F54-0x80032298, three
  functions in the render pass of an object whose class is not yet reconstructed; its `this` is the placeholder
  `unk_RenderObject`). Named after its address; no string or class evidence names the file. `fn_80032240` and
  `fn_80032244` match exactly. `fn_80031F54` is 99.47%: retail passes a third argument (0) to `RwClimaxWorldRender`,
  which the declared prototype in `rwsdk/world/climaxplatform.h` does not take. `lbl_80529638` is declared extern in
  `include/game/unk_80031F54.h`; its owning unit is not yet identified.
- **Game unit name unconfirmed (fn_80009EC8):** `game/unk_80009EC8.cpp` (0x80009EC8-0x80009F60, one function: the WPAD
  and KPAD start-up of the pad input) is named after its address; no string or class evidence names the file. Its
  callees `fn_80009CCC` and `fn_80009CD4` (the WPAD allocator and free callbacks) and `fn_80009CF8` (the KPAD connect
  callback) are declared extern "C" in `include/game/unk_80009EC8.h`, defined in their own units (not yet reconstructed).
- **Game unit name unconfirmed (fn_80098F2C):** `game/unk_80098F2C.cpp` (0x80098F2C-0x80098F34, one function: the setter
  of the byte at `lbl_80529E4A`) is named after its address. No string or class evidence names the file. The global
  `lbl_80529E4A` (6 bytes, .sbss) is declared extern in `include/game/unk_80098F2C.h` and defined in the unreconstructed
  .sbss unit `auto_10_80529C40_sbss`. The parameter type `u8` comes from the `stb` store, not from a caller's type.
- **Unit name unconfirmed (R7; game lane L0115):** `game/unk_8002D148.cpp` (0x8002D148-0x8002D158, two leaves) is named after
  its address. `fn_8002D148` returns the float 1.6f and `fn_8002D150` returns 0; both are slots of the vtable `lbl_803E1788`
  (a CPointOfInterest-derived class, not reconstructed). The int return of `fn_8002D150` is a guess; the function is
  declared extern "C" in `include/game/unk_8002D148.h`.
- **Unit name unconfirmed (R12, R7; game lane L0003):** `game/unk_80009CF8.cpp` holds one function, `fn_80009CF8`; the
  file name is an address placeholder. Its shape is the WPAD connect callback `(chan, result)` (`WPADConnectCallback`),
  but no caller in the tree names it, so the name and the role are invented. `Audio::Manager::InitWiiController` is
  declared in `include/climax/Audio/Manager.h` from its dtk signature name alone (static, one `long`).

- **Game unit name unconfirmed (fn_8002D2DC):** `game/unk_8002D2DC.cpp` (0x8002D2DC-0x8002D384, one function: the load
  callback that 0x8002D4BC passes to fn_801B05C0) is named after its address. Parked NonMatching, identity TODO at the
  code: the owner `lbl_80529260` is converted to its `unk_SpawnerResource` base at offset 8 with a null check; that
  needs `unk_RespotManager` to have an 8-byte non-empty base before the spawner base (checked by a disposable
  experiment: the function is exact with such a base, which is not identified yet). `lbl_80529264` is declared in
  `include/game/unk_8002D2DC.h`, defined in the unit of 0x8002E364 and 0x8002ECE0 (not yet reconstructed).
- **Node-list destructor unit (R12, R7):** `src/game/unk_8000EC94.cpp` holds `__dt__12unk_NodeListFv`
  (0x8000EC94), exact, but the class is a unit-local copy of the `unk_List<T>` shape in `climax/unk_List.h` for an 8-byte
  element (the destructor is that template's `~unk_List` with the same `Remove` test). The owner of the instance is the
  unit that builds it (the constructor inlined in fn_8000EE44's region, with `__vt__12unk_NodeList` at 0x803DFC44), not
  this one-function unit. Parked NonMatching until the owner is reconstructed and instantiates the template; then delete
  the copy.
- **`game/unk_8000F71C.cpp` (unit name unconfirmed):** the file name is the address (no class-name evidence). It holds
  four functions of the physics manager's region: `unk_PhysicsManager::fn_8000F71C` (matched), `fn_8000F804` (matched,
  owner unknown), `fn_8000F7F8` (matched as a free function: its manager argument is untyped and its object's class is
  unnamed) and `fn_8000F78C` (stub: it constructs a 0x10C-byte object with `fn_8000D090` and passes the manager's
  member at 0xC, whose type is not yet known). `fn_8000D70C` is declared in `include/game/unk_PhysicsManager.h` as an
  identity TODO. `lbl_80529178` is declared there and defined in the unit of `fn_8000F5E4`.
- **`game/unk_PhysicsManager` unit name (unit name unconfirmed; R7):** `src/game/unk_PhysicsManager.cpp` takes its name
  from the invented class `unk_PhysicsManager` (`include/game/unk_PhysicsManager.h`). Its one function,
  `fn_8000F648`, is a member whose instance is unused and whose body is identical to `fn_8000F5E4` (not yet in
  this unit). The counter `lbl_80529178` is declared extern in the header and is owned by the unreconstructed unit
  that holds 0x80529178. Rename the unit and class when evidence appears.
- **`game/unk_80015234` unit name unconfirmed (R12, R7):** `src/game/unk_80015234.cpp` carries the "unit name
  unconfirmed" comment; no string or class name names it. Its only function is the compiler-generated static
  initialiser `__sinit_\unk_80015234_cpp`, which sets the display-height pair (`game/DisplayHeight.h`) and constructs
  the two message ids `lbl_80529188` and `lbl_80529190` (placeholder names, many referrers; `unk_801A92A8` is the
  placeholder class of `include/climax/unk_801A92A8.h`).

- **Unit name unconfirmed (R12, R7):** `game/unk_80015348.cpp` (`fn_80015348`, the accessor of the registry table
  `lbl_805291BC`) carries the "unit name unconfirmed" comment. It returns the registry record (`SClassInfo`) at the index;
  `lbl_805291BC` has no owner unit yet (the table is filled by `fn_8001539C` in the auto_03 region).
- **Unit name unconfirmed (R12):** `game/unk_800152C0.cpp` carries the "unit name unconfirmed"
  comment: `fn_800152C0` (the registry lookup of a class record by name, `SClassInfo`) has no class or string
  evidence for its file or name. The registry globals `lbl_805291BC` (table) and `lbl_805291C0` (count) are declared
  in `climax/AttributeHandler.h` and still defined by the unreconstructed `.sbss` unit `auto_10_80529170_sbss`.
  `SClassInfo` past its name and factory is padding (`field_0x14`), not yet reconstructed.

- **Game unit name unconfirmed (fn_8001A848):** `game/unk_8001A848.cpp` (0x8001A848-0x8001A8E8, two functions) is named
  after its address; no string or class evidence names the file. `fn_8001A848` (a table lookup that returns the float of
  a target) is exact. `fn_8001A8C0` is a stub: its retail body is a virtual call on the second argument (slot 21), the
  loop callback installed by the RtAnimInterpolator setters at 0x8001B85C and 0x8001BAE8; the class is not recovered.
- **`fmod/fmod_memory` helpers (R7, R30; invented):** `FindFreeBlocks` and `FindThreadSlot` are invented inline
  helpers (name and shape marked at the code; the sibling's line table puts each on one line of its callers, so the
  original was probably a macro). The declaration order of their locals gives retail's register numbering in `alloc`
  and `realloc`; review it against the reader test (R30) when the original spelling is evidenced.
- **MIX unit split and invented names (R12, R7; sdk-axart):** `revolution/mix/mix_rmt.c` carries the "unit name
  unconfirmed" comment (no public Wii MIX source). The split into `mix.c` and `mix_rmt.c` is PROVISIONAL: a single
  `mix.c` with `#pragma dont_inline on`/`reset` around the three remote functions compiles to the same bytes (see
  "sdk-mix rulings"). Invented: `MIXRmtSetVolumes`, `__MIXUpdateRmtChannel`, `__MIXResetRmtChannel`, the type
  `MIXRmtChannel` and its fields (`mode`, `main0..3`, `aux0..3`, `vMain*`, `vAux*`), `MIXSetAuxC` and the `MIXChannel`
  `auxC`/`vC*` fields (modelled on auxA/auxB), the `MIX_MODE_*`, `MIX_FLAG_*`, `MIX_CTRL_*`, `MIX_RMT_MODE_*` and
  `MIX_MAX_VOICES` macros (tp writes the bits as raw hex), `__MIXRmtChannel`, `__s_MIXRmtChannel`, and the statics
  tp lacks: `__s_MIXChannel`, `__MIXMaxVoices`, `__MIXInitialized` (the Wii build's 96-voice table and init flag).
  Checked against tp at review: `__MIXVolumeTable`, `__MIXPanTable`, `__MIX_DPL2_front`/`rear`, `__MIXChannel`,
  `__MIXSoundMode`, `__MIXSetPan`, `__MIXResetChannel`, `__MIXClampPan` and `__MIXGetVolume` (including its
  `int db = db_;` parameter copy) are tp's. The invented names are recorded here but not yet marked one by one at the
  code (mix.h's banner covers the remote names only): add the markers in the next MIX touch.
- **Invented names (R7, sdk-vi-wpad):** `VISetTimeToDimming` (0x8022A860), `VIDisplayState` (tp `SomeVIStruct`), vi.c's `VI_DIM_*`, `VI_DVD_STOP_*`, `VI_DI_*`, `VI_VERT_COUNT`/`VI_HORIZ_COUNT`, `U32_MAX`, `VI_FBB_*` macros; KPAD's `KPAD_FMT_BTN_ACC_DPD_EXTENDED`, `KPAD_RING_SAMPLES` and the `KPADInsideStatus` members listed at the struct. All marked at the code.
- **Unit names and Std names unconfirmed (R12, R7; sdk-axfx):** `revolution/axfx/AXFXReverbStd.c` and
  `AXFXReverbStdExp.c` carry the "unit name unconfirmed" comment (no public source or header for the standard reverb).
  Invented: `AXFXReverbStdInit`/`Shutdown`/`Callback`, `AXFXReverbStdExpInit`/`Shutdown`/`Callback`/`GetMemSize`, the
  types `AXFX_REVERBSTD` and `AXFX_REVERBSTD_EXP` (layout derived from the asm) and `AXFX_BUS_DPL2` (the DPL2 bus
  pointer type of `AXFX_REVERBHI_EXP_DPL2`). Modelled on the Hi names; rename when evidence appears.
- **Unit name unconfirmed (R12):** `revolution/wpad/wpad_debug_msg.c` (tp's file name; ogws `WPAD/debug_msg.c`). `VISetCGMS`'s `OSPanic` line (1) is an unknown placeholder.
- **Unit name unconfirmed (R12, R7; game/AtomicTags):** `src/game/AtomicTags.cpp` holds `fn_8000C32C`, the reader of the atomic's `atomicType` user-data integer (callers: `AtomicTags::FindAtomicByTypeCB`, `AtomicTags::FindAllAtomicsByTypeCB`, `fn_8009E3AC`). The file name follows the class in the callers' symbol names; no string names the file. The name `fn_8000C32C` is a placeholder for a free function (`extern "C"`).
- **Invented macro name (R7):** `ALARM_TAG_INTERNAL` in `revolution/os/OSAlarm`, the tag that
  `__OSSetInternalAlarmUserData` (0x802125E0, named from the Gormiti link map, os.a OSAlarm.o) stores in the alarm.
  Its partner at 0x802125F0 is `__OSCancelInternalAlarms`, the name zeldaret/tp `libs/revolution/src/os/OSThread.c`
  calls from `OSCancelThread` (as here), with the same behaviour (cancel every alarm tagged internal whose user data
  is the thread).
- **Invented constant name (R7):** `OS_SYSTEM_TIME_ADJUST` (low-memory offset 0x30D8) in `revolution/os/OSTime`.
- **`__EXIFreq` owner open (sdk-nand-2):** euart reads the EXI clock selection `__EXIFreq` (.sdata2 0x8052EAD8,
  value 4, `EXI_FREQ_16M`). The datum sits ahead of OS.c's `.sdata2` (0x8052EAE0), so it is not euart's own (euart's
  `.text` follows OS's). It stays in the auto `.sdata2` range, declared once as `extern const u32 __EXIFreq;` in
  `revolution/euart.h`. When its owner unit is found, define it there and move the declaration to that unit's header.
- **OS header request (sdk-nand-2):** NANDCore's shutdown handler tests the power-off event, which OSReset.c keeps
  private (`SD_EVENT_POWER_OFF`); NANDCore spells it `NAND_SHUTDOWN_EVENT 2` (name invented). The OS owner should
  export the `OS_SD_EVENT_*` shutdown event constants in `revolution/os/OSReset.h` (tp's public names), after which
  NANDCore uses them.
- **Invented names (R7, sdk-nand-2):** `NANDCreateAsync`, `NANDDeleteAsync`, `NANDCreateDirAsync` (after the
  `NANDPrivate*Async` pairs in tp), `NANDSimpleSafeOpenAsync`, `NANDSimpleSafeCloseAsync` (after `NANDSimpleSafeClose`),
  `NANDCheckAsync` (after `NANDCheck`) and `NANDGetFreeSpace` are absent from tp `nand.c`, `NANDOpenClose.c` and
  `NANDCheck.c`; the macros `NAND_ISFS_ERROR_BASE`, `NAND_SHUTDOWN_EVENT` (NANDCore), `NAND_STATE_CHECK`,
  `NAND_STATE_FREE_SPACE`, `NAND_HOME_MAX_*`, `NAND_USER_MAX_*`, `NAND_USAGE_UNKNOWN` (NANDCheck),
  `NAND_SIMPLE_SAFE_BLOCK_SIZE` (NANDOpenClose) and `SC_PRODUCT_INFO_ADDR`/`_SIZE` (scsystem) are invented;
  `SCGetBtCmpDeviceInfoArray`/`SCSetBtCmpDeviceInfoArray` are unverified (in neither tp nor ogws). Each is marked
  at the code. Verified: `NANDPrivateDeleteAsync`, `NANDPrivateGetStatusAsync` (tp `nand.c`); `BUILD_DATE`/`BUILD_TIME`
  (tp `NANDCore.c`); `SCGetBtDeviceInfoArray`/`SCSetBtDeviceInfoArray` and the other scapi accessors (doldecomp/ogws
  `src/revolution/SC/scapi.c`).
- **Unit names unconfirmed (R12):** `MSL_C/MSL_Common/Src/abs.c` (`abs`) and `MSL_C/MSL_Common/Src/fpclassify.c`
  (`__signbitd`, `__fpclassifyd`) are guessed filenames, marked at the top of each file. Candidates to check
  against a public Wii MSL decomp: `arith.c` and `math_api.c`. The header name `math_float.h` (fdlibm `__HI`/`__LO`
  and the `FP_*` classes) is likewise invented.
- **Runtime names from behaviour (R7):** `__shl2i`, `__cvt_sll_flt`, `__cvt_ull_flt`, `__cvt_dbl_usll` and
  `__cvt_dbl_ull` (ORIGINAL-ASM, `Runtime.PPCEABI.H/runtime`) have no name evidence in this binary. Kept: each
  body does what the Metrowerks routine of that name does (64-bit shift left of r3:r4 by r5; signed and unsigned
  64-bit to float with round-to-nearest-even; double to signed 64-bit, saturating; double to unsigned 64-bit, 0
  below zero and all ones on overflow), and they follow `__mod2i` in the Metrowerks `runtime.c` order.
- **Invented names in `revolution/os/OSAudioSystem` (R7):** `__ACRRegs` (the Hollywood register block at
  0xCD800000, declared in `revolution/hardware.h`), the word indices `ACR_AUDIO_CLOCK`, `ACR_AI_PLL_LO`,
  `ACR_AI_PLL_HI`, the halfword indices `DSP_MAILBOX_IN_HI`, `DSP_MAILBOX_OUT_HI`, `DSP_MAILBOX_OUT_LO`,
  `DSP_CONTROL_STATUS`, `DSP_AR_SIZE`, `DSP_AR_DMA_CNT_H`, the register-pair macros `DSP_DMA_MMADDR`,
  `DSP_DMA_ARADDR`, `DSP_DMA_CNT`, the buffer macro `__DSPWorkBuffer`, and the macros `SET_FIELD` and
  `WAIT_MICROSECONDS`. Behaviour-derived, not evidenced; rename when an SDK name is found.
- **Invented header name (R7/R12):** `revolution/hardware.h` (holds `__DSPRegs`, `__DIRegs`, `__ACRRegs`).
- **SDK names from behaviour and order (R7), accepted:** `PPCMthid0`, `PPCSync` (`PPCArch.c` order
  `PPCMfhid0`/`PPCMthid0`/.../`PPCMtdec`/`PPCSync`/`PPCHalt`); `DCStoreRange` (`dcbst` + `sc` between
  `DCFlushRange` and `DCFlushRangeNoSync`); `__LCEnable`, `LCEnable`, `LCStoreBlocks`, `LCStore`, `LCQueueWait`
  (`OSCache.c` locked-cache order, `LCEnable` wraps `__LCEnable` in interrupt disable/restore);
  `OSGetCurrentContext`, `OSSaveContext`, `OSSwitchFiber`, `OSSwitchFiberEx`, `OSInitContext` (`OSContext.c`
  order and bodies); `__OSFpscrEnableBits` (the `.sdata` word `OSSetErrorHandler` masks with `FPSCR_ENABLE`). No
  string in the binary names them; they are the public SDK names for these bodies at these positions.
- **Dead-stripped functions kept in source:** `L2Enable`, `L2Disable`, `L2GlobalInvalidate` and `static L2Init` in
  `revolution/os/OSCache` are inlined into `__OSCacheInit` and their out-of-line copies stripped by the linker.
  Kept: the `">>> L2 INVALIDATE : SHOULD NEVER HAPPEN"` string sits in the unit's pool ahead of the
  `DMAErrorHandler` strings, in the SDK function order. They have no address and are not counted.
- **`include/stdio.h` (ownership):** owned by the MSL lane since the stdio units were reconstructed (it now
  includes `MSL_C/MSL_Common/ansi_files.h` for `FILE` and declares the stdio entry points). Extend it there.
- **MSL unit names unconfirmed (R12):** `MSL_C/MSL_Common/Src/ansi_files.c`, `direct_io.c`, `file_io.c`,
  `FILE_POS.c`, `mbstring.c`, `misc_io.c`, `scanf.c` and `wstring.c` are the Metrowerks filenames as recalled, not
  confirmed by a DOL string or a checked public decomp; each file carries the "unit name unconfirmed" comment.
- **MSL names recalled, not confirmed (R7):** the enums and constants of `MSL_C/MSL_Common/ansi_files.h`
  (`__file_kinds`, `__open_modes`, `__io_modes`, `__file_orientations`, `__io_states`, `__buffer_modes` and their
  members, `__no_io_error`/`__io_error`/`__io_EOF`, `__ungetc_buffer_size`), the `FILE` member names, the
  `struct __locale`/`_loc_ctype_cmpt` member names, `__console_buffer_size` and the console buffers
  `stdin_buff`/`stdout_buff`/`stderr_buff`, and `__InStrCtrl` with `__GetAChar`/`__UngetAChar`/`__TestForError`.
  The layouts are proven by use (`__locale` is 0x48 bytes with `ctype_cmpt_ptr` at +0x38, as `mbtowc` and
  `wcstombs` read it; `file_orientation:2` sits between `file_kind:3` and `binary_io:1`, as `fwide` and `__fwrite`
  read it); the spellings are from memory of the Metrowerks headers.
- **MSL names from behaviour, accepted (R7):** `free`, `errno`, `_current_locale`, `mbtowc`, the `wcs*` functions,
  `__StringRead`, `sscanf`, and the console procedures `__read_console`, `__write_console`, `__close_console`
  (their bodies and their slots in the `__files` initialiser match the Metrowerks functions). `__sformatter` is
  named from its call signature in `sscanf` only; its body is not reconstructed.
- **Invented helper (R7):** `static int __is_blank(const char*)` in `scanf.c` (the all-white-space test `sscanf`
  inlines). The loop is in the binary; the name and the split into a helper are ours. Candidate original form: the
  Metrowerks `vsscanf` as an inline function called by `sscanf`; try it when `scanf.c` is completed.
- **No owning unit yet (R2):** `__stdio_exit` (declared in `MSL_C/MSL_Common/abort_exit.h`, written by
  `__stdio_atexit`) and `errno` (declared in `errno.h`; `symbols.txt` gives it 8 bytes in `.sbss`) are referenced
  by matched units but defined in no source unit; they stay in the asm split until their owners
  (`abort_exit.c`, `errno.c`) are reconstructed.
- **No out-of-line `wctomb`:** retail has no `wctomb` body, so `wcstombs` calls
  `_current_locale.ctype_cmpt_ptr->encode_wc` directly (the body of the Metrowerks `wctomb`). R16 candidate: define
  `wctomb` in `mbstring.c` and let `-inline auto` inline it (the out-of-line copy would be dead-stripped); check
  that it keeps the bytes before adopting it.
- **`set_error` not restored (R16):** `__fwrite`, `fflush` and `_fseek` write `file->file_state.error = 1;
  file->buffer_len = 0;` in the order of the Metrowerks `set_error` macro; restoring the macro is a [checked]
  readability fix.
- **Partial units:** `MSL_C/MSL_Common/Src/scanf.c` holds `__StringRead` and `sscanf` only; `__sformatter`
  (0x8019C714) and the rest of the original `scanf.c` stay in the asm split.
- **Invented data name (R7):** the static `__cvt_fp2unsigned_limits` in `runtime.c` is `lbl_803DA2A8` in
  `symbols.txt`; the Metrowerks source's own name is not yet confirmed.
- **SDK names from behaviour and order (R7), accepted (OS interrupt, message, memory, mutex, reboot, sync,
  semaphore units):** `__OSGetInterruptHandler`, `__OSDispatchInterrupt`, `InterruptPrioTable`;
  `OSInitMessageQueue`, `OSSendMessage`, `OSReceiveMessage`, `OSJamMessage`; `OnShutdown`, `ConfigMEM1_24MB`,
  `ConfigMEM2_52MB`, `ConfigMEM2_56MB`, `ConfigMEM2_64MB`, `ConfigMEM2_112MB`, `ConfigMEM2_128MB`, `RealMode`;
  `OSInitMutex`, `OSLockMutex`, `OSUnlockMutex`, `__OSUnlockAllMutex`, `__OSCheckMutex`, `__OSCheckDeadLock`,
  `__OSCheckMutexes`; `__OSReboot`, `__OSGetSaveRegion`, `SaveStart`, `SaveEnd`, `__OSBootDol`;
  `OSInitSemaphore`, `OSWaitSemaphore`, `OSSignalSemaphore`, `OSGetSemaphoreCount`; `SystemCallVector`;
  `OSGetCurrentThread`, `__OSGetEffectivePriority`, `__OSPromoteThread`, `OSWakeupThread` (now in the linked
  `revolution/os/OSThread`, whose reconstruction used zeldaret/tp as a reference). Each body does what
  the public SDK function of that name does, at that position in the SDK unit order; no string names them.
- **Invented names in the OS units (R7):** behaviour-derived, not evidenced; rename when an SDK name is found.
  - `OSInterrupt.c`: the low-memory macros `OS_INTERRUPTMASK_ADDR` (0x00C4), `OS_USER_INTERRUPTMASK_ADDR`
    (0x00C8), `OS_INTERRUPT_TABLE_ADDR` (0x3040); the Hollywood index and bit `ACR_PPC_INTMSK`, `ACR_INTMSK_IPC`;
    the PI register indices and bits `PI_INTSR`, `PI_INTMSK`, `PI_INT_*`, `PI_INTSR_RSWST`.
  - `OSInterrupt.h`: `OSInterruptMaskOf`; the SDK spells this macro `OS_INTERRUPTMASK(interrupt)` as recalled
    (check against a public RVL decomp before renaming). The `__OS_INTERRUPT_*` and `OS_INTERRUPTMASK_*` names
    are the SDK's.
  - `revolution/hardware.h`: `__ACRRestrictedRegs` (the 0xCD000000 mapping). `__PIRegs`, `__MEMRegs`, `__EXIRegs`
    and `__AIRegs` are the SDK names.
  - `OSMemory.c`: `OS_PHYSICAL_MEM1_SIZE_ADDR`, `OS_SIMULATED_MEM1_SIZE_ADDR`, `OS_PHYSICAL_MEM2_SIZE_ADDR`,
    `OS_SIMULATED_MEM2_SIZE_ADDR`, `OS_MEM2_END_ADDR`; `MEM1_24MB`, `MEM2_64MB`, `MEM2_128MB`, `MEM2_END_52MB`,
    `MEM2_END_56MB`, `MEM2_END_112MB`; the MEM register indices `MEM_MARR_CONTROL`, `MEM_INT_STATUS`,
    `MEM_INT_CLEAR`, `MEM_INT_ADDR_LO`, `MEM_INT_ADDR_HI`, `MEM_SIZE_CONFIG`; `BAT_CONFIG_MAGIC` and the `magic`
    parameter of `BATConfig`.
  - `OSReboot.c`: `REBOOT_ARENA_LO`, `REBOOT_ARENA_HI`, `OS_REBOOT_PARAM_ADDR` (0x3194), and `OS_RESET_RESTART`
    for the 0x80000000 flag (the SDK's `OS_RESET_RESTART` is a reset code, 0, not this flag: rename).
  - `OSSync.c`: `OS_SYSTEM_CALL_VECTOR_ADDR`, `OS_EXCEPTION_VECTOR_SIZE`.
- **`__OSNextPartitionType` in the asm split:** defined in the parked `OSExec.c`; it stays in the asm split until
  OSExec is linked.
- **`OS_EXCEPTION_SAVE_GPRS` not restored (R16):** `ExternalInterruptHandler` and `DecrementerExceptionHandler`
  open-code the register stores that the SDK writes as the `OS_EXCEPTION_SAVE_GPRS(context)` macro. Restoring the
  macro in `OSContext.h` is a [checked] readability fix (both are ORIGINAL-ASM, so no count changes).

- **Game unit name unconfirmed (R7 invented class, R12):** `src/game/unk_80012330.cpp` (its one function `fn_80012330`,
  a reference release: the last reference frees the block's RenderWare data and the block) carries the "unit name
  unconfirmed" comment; the class `unk_RefBlock` (`include/game/unk_80012330.h`) is a role name invented from that
  function alone. The caller `fn_80187470` (game/G7) passes its owner as the second argument, which this path never
  reads. Gormiti has no counterpart.
- **Game unit name unconfirmed (R7 invented classes, R12):** `src/game/unk_80029444.cpp` (`fn_80029444`, `fn_800294A4`,
  `fn_8002981C`) carries the "unit name unconfirmed" comment. The chunk table (`unk_ChunkTable` and its entry and record
  structs, `include/game/unk_80029444.h`) is a role name invented from the loader. `fn_80029444` takes a
  `CZoneChangeInfo` (flags at 0x95 and 0x97) and reads the byte at 0x10 of `lbl_8052A5C0` through the partial
  `unk_ResourceManager` (`include/game/unk_ResourceManager.h`, layout not reconstructed). Gormiti has no counterpart.
- **RenderWare unit name unconfirmed (R12):** `rwsdk/src/plcore/batkbin.c` (the plugin-registry stream functions
  `_rwPluginRegistryAddPluginStream` .. `_rwPluginRegistrySkipDataChunks`; tgsm/pbg has no such file, the name
  follows RenderWare's own `ba*` naming) carries the "unit name unconfirmed" comment.
- **RenderWare toolkit unit name unconfirmed (R12):** `rwsdk/tool/dict/rtdict.c` (named after the SDK header
  `rtdict.h`; no reference split names the file) carries the "unit name unconfirmed" comment.
- **RenderWare `RwSplitBits` (R17 note):** `include/rwsdk/plcore/bavector.h` declares `RwSplitBits`'s integer
  members `volatile`, as bfbbdecomp/bfbb `include/rwsdk/rwplcore.h` (CC0) does (the SDK header's text, checked at
  the rw-pilot merge review; original text, not a carrier): retail reloads every integer access of the square-root lookups from the
  stack (`_rwSqrt`, `_rwInvSqrt`, `RwV3dNormalize`, `RwV3dLength`, `RwV2dLength`, `RwV2dNormalize` match with it).
  The lookup macros `rwSqrtMacro`/`rwInvSqrtMacro` live in `bavector.c` with the module's globals (spelling
  reconstructed from retail).
- **RenderWare name provenance (rw-pilot merge review).** Every RenderWare identifier the lane introduced was
  checked against tgsm/pbg and bfbbdecomp/bfbb (`include/rwsdk`, `src/rwsdk`; both CC0), aap/librw (MIT, naming
  aid) and the RW 3.7 user guides (electronicarts/RenderWare3Docs). Confirmed and kept: `RwSplitBits` and its
  volatile members, `RwStringFunctions` with the seventeen `vec*` members and typedefs (bfbb), `_rwColorOpen`/
  `_rwColorClose`, `rwSqrtMacro`/`rwInvSqrtMacro`, `RwErrorCodePlugin_errcore` (pbg; the lane's `RwErrorCodeCore`
  was renamed to it), `RwChunkGroup`, `RwMemoryHintDuration` and `rwMEMHINTDUR_*`, `rwALPHATESTFUNCTION*`,
  `rwRENDERSTATEALPHATESTFUNCTION(REF)`, `RwFreeListAlloc`, `RwStreamWriteChunkHeader`, `MAKECHUNKID` (user
  guide), `RWERROR` (librw). Found in none of them and marked "name invented" at the declaration: `rwID_VECTORMODULE`
  .. `rwID_CHUNKGROUPMODULE` and their enum `RwCriterionCoreID`, `RwAlphaTestFunction` (the enum name),
  `rwMEMHINTFLAG_RESIZABLE`, `RwFreeListFree` (unconfirmed counterpart of the documented `RwFreeListAlloc`),
  `rwCHUNKHEADERSIZE`, `RWBUILDNUMBER`, `rwPLUGIN_ID`, `rwERRORCOMMONFLAG`, the `*FORCEENUMSIZEINT` sentinels, the
  `RW*GLOBAL` macros and `rw*Globals` types, `StreamFileInitialize`, `colorModule`, `chunkGroupModule`, the
  square-root table names, `_rwSListDestroyEndEntries`/`_rwSListGetBegin`/`_rwSListGetEnd`,
  `_rwResHeapInit`/`Close`/`Alloc`/`Free`, `rwCHUNKGROUPMAXNAME`, `_rwChunkGroupOpen`/`Close`. None was replaced:
  each names a role the bytes prove, in the module's evidenced naming form; rename them when a public source names
  them. Most of these names are now confirmed or renamed from
  the SDK headers: "RenderWare header provenance (rw-headers)".
- **RenderWare names invented (R7):** `E_RW_ZEROLENGTH` (core error 25, raised by `RwV2dNormalize`), and the static
  transforms `VectorMultPoint`/`VectorMultPoints`/`VectorMultVector`/`VectorMultVectors` (pbg names only one pair).
  `_rwGetNumEngineInstances`, `RwFreeListForAllUsed`, `_rwFreeListAllocReal` and the batkreg statics take tgsm/pbg's
  names.
- **RenderWare plcore matrix, memory and resources (rw-core-b).** Units `bamatrix.c` (0x802A13A0-0x802A34A4),
  `bamemory.c` (0x802A34A4-0x802A40A4) and `baresour.c` (0x802A40A4-0x802A48B4), cut by tgsm/pbg's plcore link
  order (balist, bamatrix, bamemory, baresour, bastream) and by module data (`matrixModule` and the matrix free list
  are read only by 0x802A13A0-0x802A34A4; `freeListsEnabled` only from 0x802A34A4). bfbbdecomp/bfbb's `splits.txt`
  names `bamatrix.c`; `bamemory.c` and `baresour.c` keep the "unit name unconfirmed" comment (pbg lists them, no
  source). Names: functions and data from bfbb `config/GQPE78/symbols.txt` (bamatrix: `MatrixMultiply`,
  `MatrixOrthoNormalize`, `_rwMatrixDeterminant`/`OrthogonalError`/`NormalError`, `_rwMatrixOpen`/`Close`, the
  public `RwMatrix*` set, `_rwMatrixFreeList`, `_rwMatrixFreeListBlockSize`, `_rwMatrixFreeListPreallocBlocks`,
  `matrixModule`) and tgsm/pbg `symbols.txt`/headers (`_rwFreeListEnable`, `RwFreeListCreate`, `_rwFreeListFreeReal`,
  `_rwMemoryOpen`/`Close`, `RwOsGetMemoryInterface`; `ResourcesInit`, `_rwResourcesOpen`/`Close`,
  `RwResourcesFreeResEntry`, `_rwResourcesPurge`, `RwResourcesAllocateResEntry`, `RwResourcesEmptyArena`,
  `resourcesModule`; `RwResEntry` from bfbb `rwplcore.h`). RW 3.7 API names in no CC0 source:
  `RwEngineGetMatrixTolerances`, `RwMatrixQueryRotate`. Invented (marked at the declaration): `MatrixInvertGeneric`
  (librw's `invertGeneral` as aid), `MatrixQueryHalfTurnAxis`, `MatrixQueryRotate`, `rwMatrixGlobals`/`RWMATRIXGLOBAL`
  and members, `rwRADIANSPERDEGREE`, `RwRealAbs`; `FreeListCreate`, `RwFreeListPurge`, `RwFreeListPurgeAllFreeLists`,
  `DefaultMalloc`/`DefaultRealloc`/`DefaultCalloc`, `FreeListRelease`, `FreeListModuleOpen`, `freeListsEnabled`,
  `freeListModuleOpen`, `freeListFreeList`, `freeListList`, `freeListFreeListSpace`, `rwMEMHINTDUR_MASK`,
  `rwFREELISTFLAG_STATIC`/`FREEBLOCKS`; `rwResourcesGlobals`/`RWRESOURCESGLOBAL` and members,
  `rwID_RESOURCESMODULE` (0x40B), `E_RW_INSUFFICIENTRESOURCES` (12).
- **`RwFreeList` is 0x24 bytes (rw-core-b).** Retail allocates the structures from a free list of 0x24-byte entries;
  the 0x28-byte static free lists in each module's `.bss` are 0x24 objects padded by the next unit's alignment. The
  invented `field_0x24` is dropped and the symbols' sizes in `symbols.txt` corrected (DOL unchanged).
  The rw-core merge applied the same correction to that lane's static free lists `_rwCameraFreeList`,
  `frameFreeList`, `_rwRasterFreeList` and to `lbl_804F6F98` (the immediate-mode module's in-place free list,
  0x409 in its `RwFreeListCreateAndPreallocateSpace` hint, followed by bamatrix's `.bss`): all now 0x24.
- **Declarations moved home (rw-core merge):** `RwGlobals` has `resArenaInitSize` (0x128), so baresour reads
  `RWSRCGLOBAL(resArenaInitSize)`; the offset macro `rwRESARENAINITSIZE` is gone and baresour is linked
  (`Matching`, 7/7). `rwID_RESOURCESMODULE` is rwtypes.h's `RwCriterionCoreID` member, `rwID_UNICODESTRING` and
  `rwID_MATRIX` babinary.h's `RwCorePluginID` members (the macro copies in rwstring.h and bamatrix.h clashed with
  them), `rwLinkListGetLastLLLink` is in bamemory.h, and bamatrix.c's local vector macros gave way to bavector.h's.
  Still outside its home: `E_RW_INSUFFICIENTRESOURCES` (baresour.h; belongs in baerr.h). `rwPI` is in
  `plcore/batypes.h` (rw-headers merge).
- **`RwMatrix` alignment and `switch` lowering (rw-core-b; resolved by the header evidence, rw-headers).**
  RwMatrixScale, RwMatrixTranslate, RwMatrixTransform and RwMatrixRotateOneMinusCosineSine lower
  `switch (combineOp)` as a compare tree in retail. Traced in mwcceppc Wii/1.0: `IRO_OptimizeSwitches` turns a
  switch of fewer than 8 compares into an if-chain at -O1 and above unless the IR optimiser is off for the
  function, which it is when the function compares a pointer to an aligned type or reads a member through one
  (docs/compiler_regalloc.md, "IR optimiser gate"). The SDK headers declare the three types through the platform alignment macro: `bamatrix.h`
  `typedef struct RwMatrixTag RWALIGN(RwMatrix, rwMATRIXALIGNMENT);`, `bacamera.h`
  `typedef struct RwCamera RWALIGN(RwCamera, rwMATRIXALIGNMENT);` and `baframe.h`
  `typedef struct RwFrame RWALIGN(RwFrame, rwFRAMEALIGNMENT);` (also `RwV4d`, not modelled here). `RWALIGN` and the
  alignments are defined per OS in `ostypes.h` (in its Windows build `RWALIGN` is empty and both
  alignments are `sizeof(RwUInt32)`). `include/rwsdk/rwtypes.h` (our ostypes role) spells the GameCube expansion
  `type __attribute__((aligned(x)))` with both alignments `sizeof(RwUInt32)`: the expansion is inferred from
  retail's switch lowering (no allowed source shows the Metrowerks one; INFERRED, open for the user's
  confirmation), the values follow the Windows build and
  are bounded by retail (16 breaks MatrixQueryRotate, RwMatrixRotateOneMinusCosineSine and RwMatrixTransform; 4 and
  8 compile alike). With the typedefs in their SDK homes the header layout follows the SDK too: `RwV2d`, `RwV3d`,
  `RwSplitBits`, `RwSphere` and `RwPlane` moved to the new `plcore/batypes.h`, `bamatrix.h` includes it and holds
  the `RwMatrix` typedef, and `bavector.h` includes `batypes.h` and `bamatrix.h` (as the SDK's bavector.h does).
  Result (`neutral.exe`): 13 functions of bamatrix, bacamera, baframe and babinfrm change, all toward retail
  (bamatrix 17 to 21 of 24 exact; bacamera's `RwCameraSetProjection` and baframe's `RwFrameRemoveChild` exact); no
  other unit changes, game units included. `#pragma opt_optimizecontrolflow off` is no longer needed anywhere.
- **Original quirks (R36):** RwMatrixScale and RwMatrixTranslate dereference a NULL matrix in the flags update
  after an invalid combine op; RwFreeListPurge returns blocks freed times `entrySize`; MatrixQueryRotate copies
  `mInverse`'s flags into `mOneMinus`. Labelled at the code. `RwEngineSetMatrixTolerances` has no copy in retail.
- **RenderWare core module IDs and module entry points (rw-core).** `RwCriterionCoreID` gains `rwID_MATRIXMODULE`,
  `rwID_FRAMEMODULE`, `rwID_CAMERAMODULE`, `rwID_IMAGEMODULE`, `rwID_RASTERMODULE`, `rwID_TEXTUREMODULE`,
  `rwID_PIPEMODULE`, `rwID_IMMEDIATEMODULE` and `rwID_RESOURCESMODULE` (0x402-0x40B, the IDs CorePluginAttach
  registers): invented in the rwID_ form, after aap/librw's `ID_FRAMEMODULE`/`IMAGE`/`RASTER`/`TEXTURE` and its
  list of the other modules by role. The entry points badevice registers and calls take tgsm/pbg's names, matched by
  pbg's GC symbol order and sizes and by their call sites: `_rwMatrixOpen`/`Close`, `_rwRenderPipelineOpen`/`Close`,
  `_rwPipeAttach`, `_rwPipeInitForCamera`, `_rwIm3DOpen`/`Close`, `_rwResourcesOpen`/`Close`/`Purge`,
  `_rwDeviceRegisterPlugin`, `_rwDeviceGetHandle`, `_rwFreeListEnable`, `_rwFreeListFreeReal`, `_rwMemoryOpen`/`Close`,
  `_rwObjectHasFrameSetFrame`/`ReleaseFrame` (bfbb rwcore.h). They are declared in headers named after pbg's units
  (`pipe/p2/bapipe.h`, `pipe/p2/baim3d.h`, `plcore/baresour.h`, `driver/gcn/dldevice.h`, `plcore/bamatrix.h`,
  `plcore/bamemory.h`); those units are not yet reconstructed, and the Wii driver directory keeps pbg's `gcn` name
  (no evidence of the Wii build's own). `rwsdk/src/batypehf.c` takes pbg's unit name (pbg has no source for it).
- **RenderWare `_rwVectorSetMultFn` (bavector, dead-stripped helper).** `_rwVectorOpen` installs the four default
  transforms through `_rwVectorSetMultFn(NULL, NULL, NULL, NULL)`, inlined (`-ipa file`): the open function is
  exact only with that call, whose inlined parameter copies give retail's early address loads. The name is tgsm/pbg's
  (its GameCube copy takes two transforms); the four-transform signature, in globals order, is reconstructed. Retail
  has no standalone copy (nothing calls it), so its body carries `// no copy in retail (dead-stripped or inlined)`.
  bavector's table builders (closed): `SqrtTableCreate`/`InvSqrtTableCreate` take the Gormiti DWARF's shape, one `spIn` and one
  `spOut` of the function written by both loops (no per-iteration locals), with the two half-tables as pointers.
  The four C transforms and `_rwV3dNormalize` were register/schedule walls; bavector is exact (the three paired-single
  transforms are ORIGINAL-ASM).
- **RenderWare file functions, names unconfirmed (R7):** the streams call `RwFopen` (fopen role) through
  `RwFtell` (ftell role) directly, not through `RwGlobals` file functions; declared in
  `include/rwsdk/plcore/bafsys.h`, unit not yet reconstructed.
- **RenderWare chunk group module, names unconfirmed (R12, R7):** `rwsdk/src/plcore/rwchunkgroup.c`
  (0x802A6CE4-0x802A6DC4) is the core module registered with ID 0x412 and four bytes of globals; its open function
  builds a free list of 33-byte entries that nothing in this build allocates from. `rwID_CHUNKGROUPMODULE`
  (0x12), `RwChunkGroup` (`RwChar name[rwCHUNKGROUPMAXNAME + 1]`, 33 bytes; the user guide's "fixed length string") is
  documented; the module ID, `rwCHUNKGROUPMAXNAME` and `_rwChunkGroupOpen`/`Close` are invented (provenance above). The file name is invented
  to sort between `resmem.c` and `rwstring.c` (the plcore units are linked in file-name order); the module could
  also be the head of `rwstring.c`. Its static names follow bastream's.
- **RenderWare resource heap, names unconfirmed (R12, R7):** `rwsdk/src/plcore/resmem.c` (0x802A6A74-0x802A6CE4)
  takes the file name tgsm/pbg lists at this place in the plcore link order (pbg has no source for it).
  `_rwResHeapInit`/`Close`/`Alloc`/`Free` are invented (in no reachable public source);
  `ResHeapSplitBlock`, `rwResHeap`, `rwResHeapBlock`, `rwRESHEAPALIGN` and `rwRESHEAPBLOCKUSED` are invented. The
  callers (0x802A40A4-0x802A46B8, the resource arena) are not yet reconstructed. Retail tests a free block as
  `~flags & rwRESHEAPBLOCKUSED` (`nor`, `clrlwi.`); `!(flags & ...)` and `(flags & ...) == 0` give one `clrlwi.`.
- **RenderWare `RwSList` names (R7):** `balist.c` takes `_rwSListCreate`, `_rwSListGetNewEntry`,
  `_rwSListGetNewEntries`, `_rwSListDestroy`, `_rwSListGetNumEntries`, `_rwSListGetEntry` and the `RwSList` members
  from tgsm/pbg; `_rwSListDestroyEndEntries`, `_rwSListGetBegin`, `_rwSListGetEnd` (not in pbg) and `SLIST_STARTSIZE`
  are the SDK header's (rw-headers). Retail's
  `_rwSListCreate` takes a memory hint (pbg's does not) and has no `_rwSListEmpty`, `_rwSListGetArray` or
  `_rwSListToArray`.
- **RenderWare colour module names (R7):** `bacolor.c` holds only the open/close pair registered with ID 0x40D;
  `_rwColorOpen`/`_rwColorClose` are pbg's (its module table); `colorModule` is invented.
- **RenderWare string table names (R7):** `RwStringFunctions` and its `vec*` members/typedefs are bfbbdecomp/bfbb `rwplcore.h`'s
  (same seventeen members in the same order); the stores of `_rwStringOpen` prove the order and the 17
  members at `RwGlobals` 0xC4-0x108 (`strncmp` included, which tgsm/pbg lacks). `StrICmp`, `StrUpr`, `StrLwr`,
  `StrChr`, `StrRChr` take tgsm/pbg's names.
- **RenderWare vector functions declared twice (R1/R2 open item):** `include/climax/Frame.h` declares
  `RwV3dNormalize`/`RwV3dLength` on the game's `unk_RwV3d`, `include/rwsdk/plcore/bavector.h` on `RwV3d` (both C
  linkage, same layout). The game headers should take `RwV3d` from `rwsdk` and drop their copies; that retypes the
  game's `unk_RwV3d` uses (mangled member names change), so it is left to a game-header lane. The rule checker
  raises no [match] finding for it. The same holds for the matrix functions (rw-core-b renamed them in game code):
  `Frame.h` declares `RwMatrixUpdate` (returning `void`; `bamatrix.h` returns `RwMatrix*`), `RwMatrixRotate`,
  `RwMatrixInvert` and `RwMatrixScale` on `unk_RwMatrix`/`unk_RwV3d`, `include/rwsdk/plcore/bamatrix.h` on
  `RwMatrix`/`RwV3d`; Frame.h's "unit not yet reconstructed" comments on them are now stale (bamatrix is parked).
- **RenderWare string streams in `rwstring.c` (R12, PROVISIONAL; rw-core-b):** `_rwStringStreamGetSize`,
  `_rwStringStreamWrite`, the statics `StringStreamRead`/`UnicodeStringStreamRead` and `_rwStringStreamFindAndRead`
  (0x802A705C-0x802A75A0, names from tgsm/pbg `symbols.txt`) and their `nullString` (`.sbss2` 0x805300B0, pbg's
  name; a one-byte zero const, which Wii/1.0 puts in `.sbss2`) are linked as the tail of `rwstring.c`: they directly
  follow `_rwStringClose`, read the string table rwstring.c installs, and no plcore file name sorts between. Counter-
  evidence: pbg's GameCube split puts them at the head of `rwsdk/src/babintex.c` (beside the texture stream code).
  Here their callers (0x80291568, 0x802915CC) sit 0x15000 bytes away, so they are not in that unit; a separate file
  sorting after `rwstring.c` is not excluded (the bytes are the same either way). `0x802A75A0` (a path test:
  drive letter or leading backslash) starts the next, unidentified unit. bfbbdecomp/bfbb's GQPE78 split agrees
  with pbg (`_rwStringStreamGetSize` 0x80234ED4 opens `rwsdk/src/babintex.c`, after `babinfrm.c`, while its
  `rwsdk/src/plcore/rwstring.c` ends at 0x80234798, followed by `os/gcn/osintf.c`, `babbox.c`, `babincam.c`,
  `babinfrm.c`): in both GameCube builds the stream functions sit in the core (`src/`) range. Here they sit in the
  plcore range after `rwstring.c`, where bfbb has `osintf.c` next, so in this RW build they left the core object;
  rwstring.c is the nearest evidenced owner, kept PROVISIONAL (merge review: accepted).
- **RenderWare string readers' 64-byte buffer (rw-core-b; merge review: accepted, not steering):** both readers
  align their stack buffer with `__attribute__((aligned(64)))`: retail aligns the frame to 64 bytes
  (`clrlwi r11,r1,26; stwux`), which MWCC emits only for a local declared with alignment above 8, and the buffer is
  the only aggregate local; 32 misses by 7 rows. The attribute states a property the retail frame proves, so it is
  not R17 fake code. Neither CC0 reference has source for the readers (tgsm/pbg `rwstring.c` stops at the string
  table; bfbb has no RW plcore source), so the original spelling (an SDK alignment macro such as `RWALIGN`, or the
  raw attribute) is open; comment at the code.
- **RenderWare binary stream names (R7):** `babinary.c` takes its function names from tgsm/pbg (`ChunkIsComplex`,
  `RwMemLittleEndian32/16`, `RwMemNative32/16`, `RwStreamWriteReal/Int32/Int16`, `RwStreamReadInt16/Real/Int32`,
  `RwStreamReadChunkHeaderInfo`; order and sizes as pbg's GC symbol table) and its prototypes and chunk IDs from
  bfbbdecomp/bfbb `rwplcore.h`; `rwChunkHeader` (the 12-byte header as streamed) and `rwSTREAMCONVERTBUFFERSIZE`
  are invented. Retail has no `RwMemRealToFloat32`/`RwMemFloat32ToReal` (pbg's GC build does): `RwStreamWriteReal`
  and `RwStreamReadReal` are byte for byte the Int32 bodies, so the reals are only byte-swapped here.
- **RenderWare string streams, unit unassigned (R12):** `fn_802A705C` (strlen through the string table, rounded
  to 4: a string chunk's size), `fn_802A709C` (writes an `rwID_STRING` chunk) and the readers from `fn_802A715C`
  follow `_rwStringClose` and are left in the asm split; they may belong to `rwstring.c`, in which case its
  `.text` range grows to take them.
- **RenderWare frame names (R7, rw-core).** `baframe.c`/`babinfrm.c` take tgsm/pbg's names (`_rwFrameOpen`/`Close`,
  `rwSetHierarchyRoot`, `rwFrameCloneRecurse`, `_rwFrameCloneAndLinkClones`, `_rwFramePurgeClone`, `_rwFrameInit`,
  the three destroy-recursion statics, `frameTKList`, `frameModule`, `_rwFrameFreeListBlockSize`/`PreallocBlocks`,
  `_rwFrameSyncHierarchyLTM`, `rwFrameList`, `_rwFrameListDeinitialize`/`StreamRead`) and bfbbdecomp/bfbb's
  (`RwFrameGetRoot`, `RwOpCombineType`, `RwMatrixType`, `rwMATRIXINTERNALIDENTITY`, the `rwObject*` and
  `rwLinkList*` macros, `rwID_FRAMELIST`). Invented, marked at the declaration: `RwFrameSetFreeListCreateParams`
  (after the user guide's `RwTextureSetFreeListCreateParams`), `_rwFrameDeInit` (retail's out-of-line first half of
  `RwFrameDestroy`), `RwFrameAddChildNoUpdate` (`RwFrameAddChild` without the dirty-list and update steps, used by
  the frame list reader), `RwFrameSetIdentity` and `RwFrameGetPluginOffset` (after aap/librw's `Frame::setIdentity`
  and `getPluginOffset`), `rwFRAME`, the `rwFRAMEPRIVATE*` flags (librw's `HIERARCHYSYNC*`/`SUBTREESYNC*` values),
  `rwFrameGlobals`/`RWFRAMEGLOBAL`, `frameFreeList`, `rwStreamFrame` and its `parentIndex`/`data` members,
  `_rwFrameListAutoUpdate` (the `.sdata` flag choosing `RwFrameAddChild` or the no-update link) and the two
  matrix-error bounds. Retail has no copy of `RwFrameScale`, `RwFrameCount`, `RwFrameSetStreamAlwaysCallBack` or
  pbg's frame list builders and writers.
- **RenderWare `baframe.c` inlining (rw-core ruling, PROVISIONAL).** Retail `baframe.c` inlines no call (`RwFrameUpdateObjects`
  is called from eight frame functions, `rwSetHierarchyRoot` recurses out of line), while `babbox`, `bastream` and
  `batkbin` need the library's `-inline auto`. flagsweep: `-inline noauto` or `-inline off` take baframe from 12 to 28
  of 29 exact and leave layout clean (`#pragma auto_inline off` at the top of the file gives the same bytes);
  `-inline deferred` changes nothing. Ruling: per-object `-inline noauto` for `rwsdk/src/baframe.c` in
  `configure.py` (the project's form for a per-file codegen difference; the source stays free of pragmas). Whether
  the original file carried a pragma instead cannot be told from the bytes.
- **RenderWare one-function walls (rw-core, traced).** These units were parked on one function each; the walls were
  traced (the compiler-trace tools, `build/tools/wdbg.exe` and friends), not searched by spelling.
  - `baframe` `RwFrameRemoveChild` (1 row, CSE): closed by the SDK's aligned `RwFrame` typedef (rw-headers,
    "RwMatrix alignment"); the plain reads match, no const accessor needed. baframe is `Matching`.
  - `babinfrm` `_rwFrameListStreamRead` (register allocation): with the aligned typedefs only the frame/matrix
    pair (r30/r29) still differed; it is closed by declaring `frame` before `matrix` (rw-headers). The
    `-opt nostrength` trace above it no longer applies.
  - `batextur` `_rwTextureClose` (closed): the Gormiti DWARF (Debug build) shows the original recomputes the list head,
    terminator and a `texDictsExistOnShutdown` flag after destroying the dummy dictionary, and asserts the flag
    after the loop (`RWASSERT`, an empty macro in a release build). The assigned flag keeps the globals pointer live on
    the break edge, which is retail's join placement.
- **RenderWare engine names (R7, rw-core badevice).** `badevice.c` takes tgsm/pbg's names: `CorePluginAttach`,
  `MallocWrapper`, `FreeWrapper`, `EngineOpen`, `engineTKList`, `staticGlobals`, `engineInstancesOpened`,
  `RwCoreDeviceSystemFn` and `RwSubSystemInfo`; `RwMetrics`, `RwEngineStatus`, `RwEngineInitFlag`,
  `RwEngineOpenParams`, `RwVideoMode`(`Flag`), `rwstrcpy`, the `RwEngine*` signatures and the `RwGlobals` members
  `stdFunc`, `dirtyFrameList`, `metrics`, `engineStatus`, `resArenaInitSize` are bfbbdecomp/bfbb `rwplcore.h`'s.
  Invented: the standard function index enum `RwStandardFunction` and its `rwSTANDARD*` members (no reachable
  public source spells them; each is named after the driver function tgsm/pbg's `dldevice.c` installs at that
  index; slot 22 is `rwSTANDARDHINTRENDERF2B` in the SDK header, pbg installs nothing there), `rwSTANDARDNUMOFSTANDARD` (29, the count
  `EngineOpen` passes). Retail differs from pbg: `RwEngineInit`/`RwEngineTerm` open and close no file system,
  `CorePluginAttach` also registers the chunk group module (0x412) before `_rwPipeAttach`, the version is 0x37002,
  `RwGlobals` is 0x12C bytes (no file function table; the string table has 17 entries), and
  `RwEngineGetCurrentSubSystem`, `RwEngineGetCurrentVideoMode`, `RwEngineGetTextureMemorySize` and
  `RwEngineGetMaxTextureSize` have no copy in retail (not declared). The device error raised by
  `_rwDeviceSystemRequest` (core code 24) stays a number, as in pbg. Its one string literal (retail `.data`)
  leaves `-str` unproven: `noreuse`, `reuse`, `reuse,readonly` and `reuse,pool` give the same code (flagsweep).
- **RenderWare texture names (R7, rw-core batextur).** `batextur.c` takes tgsm/pbg's names (`TextureDefaultMipmapName`,
  `PalettizeImage`, `PalettizeMipmaps`, `TextureImageReadAndSize`, `TextureDefaultNormalRead`/`MipmapRead`/`Read`,
  `TextureRasterDefaultBuildMipmaps`, `StringCompare`, the `RwTexture*`/`RwTexDictionary*` API, `textureTKList`,
  `texDictTKList`, `textureModule`, `dummyTexDict`, the free lists and their block sizes, the members
  `textureFreeList`, `texDictFreeList`, `textureReadCB`, `mipmapping`, `autoMipmapping`, `rasterBuildMipmapsCB`,
  `mipmapNameCB`) and bfbbdecomp/bfbb `rwcore.h`'s (`RwTexDictionaryRemoveTexture`, the `filterAddressing` masks and
  macros, `rwLLLinkGetData`/`GetConstData`/`GetNext`, `rwLinkListGetFirstLLLink`/`GetTerminator`, `rwstrncpy`,
  `rwstrcat`, `rwstrlen`). Invented, marked at the declaration: `rwTextureGlobals`/`RWTEXTUREGLOBAL`, `texDictList`,
  `currentTexDict`, `textureFindCB` (aap/librw's `findCB`), `TextureDefaultFind` (librw's `defaultFindCB`, the
  find callback retail installs at module offset 0x18; tgsm/pbg's GameCube module has none), `RwTextureCallBackFind`,
  `RwTexDictionarySetCurrent` (librw's `TexDictionary::setCurrent`), `rwTEXDICTIONARY` (librw's ID 6) and
  `rwTEXTUREBASENAMELENGTH`; `field_0x24`/`field_0x28` (a heap block and a 16-bit count `_rwTextureClose` frees and
  clears; nothing in the unit fills them) keep placeholder names. `RwTextureRead`'s error (core code 22, the texture
  and mask that could not be read) stays a number, and the texture-dictionary free list's hint names the module ID
  0x408 as a number (`rwID_TEXTUREMODULE` is not in `rwtypes.h`). Retail differs from pbg: a find callback before the
  read callback, `RwTexDictionaryRemoveTexture`/`SetCurrent` present, `RwTextureGenerateMipmapName` without a copy,
  and `RwTexDictionaryForAllTextures` takes and returns a `const RwTexDictionary*` (the SDK signature, bfbb): with it
  the inlined copies in `RwTexDictionaryDestroy` and `_rwTextureClose` keep the next link apart from the current one,
  as retail does.
  The image readers call the image, resample and palette-quantiser APIs, declared for their units (not yet
  reconstructed) under tgsm/pbg's names, identified by pbg's GameCube symbol order and sizes and by their use here:
  `RwImageCreate`, `RwImageAllocatePixels`, `RwImageFindFileType`, `RwImageReadMaskedImage`, `RwImageCopy`,
  `RwImageGammaCorrect` (`baimage.h`), `RwImageResample`, `RwImageCreateResample` (new `baresamp.h`), `RwPalQuant` and
  `_rwPalQuantInit`/`AddImage`/`ResolvePalette`/`MatchImage`/`Term` (new `driver/common/palquant.h`). Retail differs
  from pbg: `_rwPalQuantAddImage` takes `(pq, image, weight)` (pbg's `(weight, pq, image)` evaluates the weight first,
  4 rows in `PalettizeImage`), and the Wii `RwPalQuant` is 0x4008 bytes (the callers' frames; `_rwPalQuantInit` and
  `_rwPalQuantTerm` use only its last two words, the octree root and its node free list, kept as `field_0x4000`/
  `field_0x4004`; the 0x4000 bytes before them keep a placeholder array). Invented: `rwTEXTUREIMAGENAMELENGTH` (256),
  `rwTEXTUREPALETTESIZE` (256) and `rwTEXTUREMAXMIPLEVELS` (16), and the quantiser's parameter names beyond pbg's.
  `TextureImageReadAndSize`'s image-format error stays the number 0x80000009 (`baerr.h` has no name for it). Kept
  quirks (R36): `PalettizeMipmaps` ends its shared-palette search by setting the level index to 64,
  `TextureRasterDefaultBuildMipmaps` returns from inside its clean-up loop when palettising fails (only the top level
  is destroyed), and `PalettizeImage` returns without `_rwPalQuantTerm` when the image cannot be created.
- **RenderWare `-str noreuse` (rw-core batextur evidence).** Retail batextur keeps two separate 1-byte `""` literals in
  `.sdata` (0x80528EB0, 0x80528EB4: the mask names `TextureDefaultNormalRead` and `TextureDefaultMipmapRead` pass to
  the inlined `RwTextureSetMaskName`); `-str reuse` merges identical literals of a file into one, so RW was built
  with `noreuse`. `"(null)"` (`RwTextureRead`) sits in writable `.sdata` and is addressed `sda21` off r13:
  `-str reuse,readonly`, `reuse,pool` and `noreuse,readonly` each change `RwTextureRead` (3 rows, flagsweep), so
  neither `readonly` nor `pool`. With both readers reconstructed, `-str noreuse` gives the retail `.sdata` (`""` at +0
  and +4, `"(null)"` at +8) and `-str reuse` makes `TextureDefaultMipmapRead` address `TextureDefaultNormalRead`'s
  `""` (cc.py `--raw`); the code sweep alone cannot tell them apart. `gatecheck layout` reports retail
  `lbl_80528EB4` MISSING: its cover pass pairs retail's 4-byte `lbl_80528EB0` with both built 1-byte `""` objects
  (a tool artefact; the built offsets equal retail's).
- **RenderWare `RwFreeList` is 0x24 bytes (rw-core batextur).** Retail batextur's two free lists sit 0x24 apart in
  `.bss` (0x804F6F38, 0x804F6F5C), as tgsm/pbg's `RwFreeList` (size 0x24). The 0x28 sizes of the other units' free
  lists in `symbols.txt` are the 8-byte alignment of the next unit's `.bss` (MWCC emits `.bss` aligned to 8); the
  former `field_0x24` member was that padding and is removed. neutral.py: only the `.bss` sizes of baframe,
  bastream, batkreg and rwchunkgroup change ("layout ok on both sides"); no code changes.
- **RenderWare raster and image names (R7, rw-core baraster/baimras).** `baraster.c` takes tgsm/pbg's names
  (`RwRasterUnlock` .. `_rwRasterOpen`, `rasterTKList`, `rasterModule`, `_rwRasterFreeList`,
  `_rwRasterFreeListBlockSize`/`PreallocBlocks`, the globals member `rasterFreeList`; order and sizes as pbg's GC
  symbol table) and bfbbdecomp/bfbb `rwcore.h`'s (`RwRaster`, the raster enums, `RwRasterGet*` macros, the API
  signatures); `baimras.c` and `baimage.h` take pbg's and bfbb's (`RwImageSetFromRaster`, `RwRasterSetFromImage`,
  `RwImageFindRasterFormat`, `RwImage`, `RwImageFlag`, `_rwImageOpen`/`Close`, `RwImageDestroy`, `RwImageSetGamma`;
  the last four are declared for `baimage.c`, not yet reconstructed). pbg has no `baimras.c` source: its bodies
  are reconstructed from retail. Invented, marked at the declaration: `rwRasterGlobals`, `RWRASTERGLOBAL`,
  `rasterStack`, `rasterSP`, `dummyRaster`, `rwRASTERCONTEXTSTACKSIZE` (pbg calls the 0x28 bytes before the dummy
  raster `rasterPtrMaybe` and padding; the open function points entry 0 at the dummy raster and zeroes the word at
  0x28, read here as the raster context stack and its index), and the local `RASTERMODULEID` (module 0x407; its
  `rwID_RASTERMODULE` belongs in `rwtypes.h`). Retail has no copy of `RwRasterRender`, `RwRasterRenderFast`,
  `RwRasterRead` or `RwRasterReadMaskedRaster`. `RwImage.palette` is a `struct RwRGBA*` until `RwRGBA` has an
  `rwsdk` header. `include/game/CSplashScreenTrigger.h` now takes `RwImage`/`RwRaster` and the three functions
  from `<rwsdk/baimras.h>` (its `unk_RwImage` is gone; the game unit's code is unchanged, neutral.py: only the
  renamed relocation targets differ).
- **RenderWare camera names (R7, rw-core bacamera).** `bacamera.c` takes tgsm/pbg's names (the eight `Camera*`
  statics, `_rwCameraOpen`/`Close`, the `RwCamera*` API, `cameraTKList`, `cameraModule`, `_rwCameraFreeList`,
  `_rwCameraFreeListBlockSize`/`PreallocBlocks`; pbg's GC symbol order lines up one to one) and bfbbdecomp/bfbb
  `rwcore.h`'s (`RwCameraClone`, `RwCameraClearMode`, `RwFrustumTestResult`, `RwFrustumPlane`, `RwSphere`, `RwPlane`,
  `RwMatrixTolerance`, the `RwV3d*Macro` arithmetic). Callees renamed by pbg's order: `RwMatrixOptimize`
  (0x802A24B0), `_rwFrameSyncDirty` (0x8029815C), `_rwPipeInitForCamera` (0x8029B268). Invented, marked at the
  declaration: `rwCameraGlobals`/`RWCAMERAGLOBAL`, `rwCAMERA` (object type 4), `RwRealClosestSide` (the sign-bit
  read that sets a frustum plane's closest-side bytes, `*(const RwInt32*)&` as in pbg), and the frustum test's
  `.sbss` statics `numSpheresOutside`, `numSpheresTooSmall`, `numSpheresVisible`, `minSphereSize` (retail-only:
  pbg's test has no counters and no size cull; `minSphereSize` is read and never written in the DOL). Retail differs
  from pbg: `RwCamera` has no pad word before the frustum planes (0x184 bytes), plane 0 of both frustums is the
  LTM's at (the perspective builder normalises a cross product instead when the frame's modelling matrix lacks
  `rwMATRIXTYPENORMAL`), the perspective corners are built without a transform call, and the parallel corners go
  through `RwV3dTransformPoints` (the paired-single array transform).
- **RenderWare `bacamera.c` flags (rw-core, PROVISIONAL, for the lane lead).** As in `baframe.c`, retail inlines no
  call (`CameraUpdateZShiftScale` is called out of line five times): `-inline noauto` takes the unit from 12 to 16
  exact under flagsweep, nothing worse. `-opt nolifetimes` lowers the view matrix and parallel clip-plane residues
  (55/60/240 rows to 38/47/188) and changes no other RW unit except bavector (140 to 161 rows); unproven. The one
  string literal (retail `.data`) does not settle `-str`. The switch in `CameraUpdateZShiftScale` and
  `RwCameraSetProjection` compiles in retail to an unmerged compare tree (`cmpwi 3; bge; cmpwi 1; bge; b`); Wii/1.0
  through 1.7 fold the cases into `subi; cmplwi` under every spelling tried (switch with or without default, case
  order, `if` forms, `-O1`..`-O4`, `-opt` suboptions), while GC/3.0a3..3.0a5.2 reproduce `RwCameraSetProjection`
  exactly but break `RwCameraSetViewWindow` and `RwCameraCreate` (y before x scheduling). Ruling: `-inline noauto`
  adopted per object (configure.py). Switch cause traced in Wii/1.0 mwcceppc: `IRO_OptimizeSwitches` rewrites a
  switch of fewer than eight compares as ifs, which `IRO_SimplifyBoundedIntervalConditionals` folds to
  `subi; cmplwi`; retail's trees are the back end's own `switchstatement` lowering, so retail's two switches never
  went through that pass. The pass is gated by the internal pragma `opt_optimizecontrolflow` (on at every -O level);
  `#pragma opt_optimizecontrolflow off` over the unit makes `RwCameraSetProjection` exact and changes nothing else
  (`CameraUpdateZShiftScale` keeps 13 float-register rows). The same compiler folds `_rwDeviceSystemRequest`'s switch
  in matched badevice, and a version sweep keeps Wii/1.0 (GC/3.0a5.2 breaks babinary, badevice, batextur, baframe,
  bastream). Open: whether the original file was built with that pragma or flag (no RW evidence); not adopted.
- **libjpeg unit name unconfirmed (R12):** `libjpeg/jdatasrc.c`, the game's copy of IJG's stdio source manager
  (linked after jutils), carries the "unit name unconfirmed" comment.
- **libjpeg game hooks:** the file read is RenderWare's `RwStreamRead` (declared in
  `include/rwsdk/plcore/bastream.h`, which `fill_input_buffer` calls where IJG calls `JFREAD`/`fread`; `infile`
  stays `void *`, an `RwStream *` in fact) and `rw_malloc`/`rw_free` (the game allocator entry points that jmemnobs calls
  for `malloc`/`free`), named from the Gormiti link map (`jpeg.obj`) and declared in `include/libjpeg/jinclude.h`;
  their owners are Climax code outside the libjpeg range.
- **libjpeg configuration:** the game's `jmorecfg.h` sets `RGB_PIXELSIZE 4` (jdcolor and jdmerge advance 4 bytes
  per pixel and leave the fourth unwritten); IJG's default is 3. A game configuration change, kept with a note.
- **libjpeg library declarations (R6/R25):** `sprintf`, `sscanf` (`<stdio.h>`), `exit`, `getenv` and
  `EXIT_FAILURE` (`<stdlib.h>`) come from the MSL headers; `include/libjpeg/jinclude.h` no longer declares them.
  IJG's `JPP((...))` prototype wrapper is written out as plain ANSI prototypes (byte-neutral).
- **libjpeg flags:** `-O3 -fp_contract off -use_lmw_stmw on -func_align 4` is the first flag profile outside the
  SDK/runtime/MSL family. The flags of the game's own (Climax) code are still unknown; do not assume this profile
  for them.
- **libjpeg range:** the library starts at 0x801B86C8 (`jpeg_abort`); 0x801B67FC-0x801B86C8 is Climax code.
- **`block_relocations` (config.yml), reviewed:** `.data:0x80401810` (value 0x801BF188) and `.data:0x80424184`
  (0x801C0000) sit inside `lbl_803FD240` (0xDE30 bytes) and `lbl_8041D240` (0xA000 bytes). Both blobs start with the
  string `EMBEDDED` and are otherwise high-entropy words. Both values land mid-function (inside `get_dri` at
  0x801BF0B4 and `read_restart_marker` at 0x801BFFB0), never at a function start, so they are not code pointers.
  No other asm file names either value. `.rodata:0x803DA708` is `base_dither_matrix` + 0x10, row 1 (128, 64, 176,
  112 = 80 40 B0 70). Blocking all three hides no real pointer. The real address-taken use of `read_restart_marker`
  is jinit_marker_reader's method store, a code relocation, which is unaffected.

- **MSL unit names unconfirmed (R12):** `MSL_C/MSL_Common/Src/environment.c` (`getenv`, a `return NULL` stub) and
  `MSL_C/MSL_Common/Src/alloc.c` (the pool allocator) are the Metrowerks filenames as recalled; each file carries
  the "unit name unconfirmed" comment.
- **MSL alloc names (R7):** the structure, member, function and macro names of `alloc.c` (`Block`, `SubBlock`,
  `FixBlock`, `FixSubBlock`, `FixStart`, `__mem_pool_obj`, `Block_subBlock`, `Block_link`, `SubBlock_merge_next`,
  `allocate_from_var_pools`, `soft_allocate_from_var_pools`, `FixBlock_construct`, `allocate_from_fixed_pools`,
  `deallocate_from_fixed_pools`, the `SubBlock_*`/`Block_*` helpers) and the system-heap hooks `__sys_alloc` and
  `__sys_free` (`include/MSL_C/MSL_Common/alloc.h`) are recalled from the Metrowerks MSL, not confirmed by this
  binary. `__pool_resize_inplace` (0x80196958) is invented: it has no recalled counterpart. The size and flag
  constants (`SUBBLOCK_IN_USE`, `SIZE_MASK`, `MIN_SUBBLOCK_SIZE`, `MAX_FIX_CLIENT_SIZE`, ...) are behaviour-derived.
  The unit starts at 0x80195D28; `SetUseSerialIO` and `GetUseSerialIO` in front of it are MetroTRK `target_options` (split in msl-trk-6).

- **Dead-stripped functions kept in source (`revolution/si/SIBios`):** `SIBusy`, `SIEnablePollingInterrupt`,
  `SIRegisterPollingHandler`, `SIUnregisterPollingHandler`, `SISync`, `SIGetStatus`, `SIGetCommand`,
  `SIEnablePolling`, `SIDisablePolling`, `SIGetResponse`, `SIDecodeType` and `SIProbe` compile out of line but have
  no copy in retail; `SIIsChanBusy`, `SIClearTCInterrupt`, `SITransferNext`, `SISetCommand`, `SITransferCommands`,
  `SIGetResponseRaw`, `CallTypeAndStatusCallback`, `SIGetTypeAsync` and `SIGetTypeString` have none either. Kept in
  zeldaret/tp's order (all are in tp's `SIBios.c`), the OSCache/OSThread precedent: their uses of `RDSTHandler`,
  `InputBuffer`, `InputBufferValid`, `InputBufferVcount` and `TypeCallback` place those statics in `.bss`. They
  have no address and are not counted. SIBios scores 83.5% matched data in objdiff with the gate green: the
  unmatched data is exactly the 13 `SIGetTypeString` literals (11 in `.data`, `"Busy"` and `"Unknown"` in `.sdata`),
  which the linker drops with the function (the linked DOL hashes to retail). Every retail SIBios data object
  (`Si`, `Type`, `__SIVersion`, the `.bss`/`.sbss` statics, `__PADFixBits`) is 100%.
- **Inlined EXIBios functions:** `CompleteTransfer`, `EXIClearInterrupts`, `EXIProbeReset`, `EXIProbe` and
  `__EXIAttach` have no out-of-line copy in retail (inlined into their callers, stripped); they are in tp's
  `EXIBios.c` and are not counted.
- **Pending OS declarations (R2, ownership):** `include/revolution/exi.h` declares `ExiProbeStartTime[2]`
  (0x800030C0), `__OSDeviceCode` (0x800030E6), `OSGetConsoleType`, `__OSGetDIConfig` and `OSRegisterVersion`;
  `include/revolution/si.h` declares `__SIRegs`, `OSMicrosecondsToTicks`/`OSMillisecondsToTicks` (behind `#ifndef`)
  and `OSGetWirelessID`/`OSSetWirelessID`; `include/revolution/vi.h` declares `__VIRegs`. None is declared anywhere
  else on main (checked at merge). They belong in the OS headers (`revolution/os.h`, `revolution/hardware.h`,
  `OSTime.h`, an `OSRtc` header); the OS lane moves them and deletes these copies in the same change, or the second
  declaration is an R2 duplicate.
- **Invented name (R7):** `ExiProbeStartTime[2]` (low memory 0x800030C0, the channel 0/1 card-probe start times
  read and written by `__EXIProbe`, `EXIProbeReset` and `EXIGetID`). No string or checked public header names it;
  rename when an SDK name is found.
- **Names outside the EXI/SI units, accepted (R7):** `__OSGetDIConfig` (0x80211D00, 16 bytes, before
  `OSRegisterVersion`), called from `EXISync`; `OSGetWirelessID`/`OSSetWirelessID` (0x80219C90/0x80219D10, after
  `__OSSyncSram` in the OSRtc range), called from `GetTypeCallback`; `VIGetCurrentLine` (0x8022A490), called from
  `SIInterruptHandler`. Each is the callee tp (and ogws, for `EXISync`) names at that call site, and the matched
  callers' relocations land on these addresses.
- **EXI/SI names from the reference, accepted (R7):** `EXIImmEx`, `EXISetExiCallback`, `EXIAttach`, `EXIDetach`,
  `ProbeBarnacle`, `EXIWriteReg`, `CompleteTransfer` (SIBios), `__SITransfer`, `AlarmHandler`,
  `SIRefreshSamplingRate`; data `XYPAL`, `XferTime`, `Alarm`, `InputBuffer`, `InputBufferValid`,
  `InputBufferVcount`, `RDSTHandler`, `TypeCallback`, `cmdFixDevice`, `cmdTypeAndStatus`, `Enabled`,
  `BarnacleEnabled`, `Dev`, `Chan`, `__PADFixBits`. tp names at tp positions with matching bodies and sizes (data
  splits: `lbl_8042B748` = `XYPAL` + the `SISetSamplingRate` warning literal; `lbl_804920D0` = `Alarm` through
  `TypeCallback`, 0x170 bytes). The `BUILD_TIME` strings (`10:02:03` EXI, `10:04:44` SI) are the retail banners.
  The SDK's misspellings `EXIIntrruptHandler`/`TCIntrruptHandler`/`EXTIntrruptHandler` are kept (dtk names).
- **EXI/SI readability, [checked] or [free] (byte-neutral, verified at review):** `EXIDma`'s `(u32)exi->tcCallback`
  cast and `EXIImmEx`'s cast-as-lvalue can take the plain forms; `ProbeBarnacle`'s `sizeof(revision)` should read
  `sizeof(*revision)` or `sizeof(u32)` (check the bytes); unnamed EXI/SI register bits and device constants (R11:
  EXI CPR bits 0x800/0x1000/0x2000/0x405/0x7F5, the device IDs 0x07010000, 0x04020100, 0x04020300, 0x04220000 and
  0x800000xx, the SI status and type masks in `GetTypeCallback` and `CompleteTransfer`); `EXISelect(chan, dev, 4)`
  in `EXIWriteReg` (`EXI_FREQ_16M`). Function header comments (R33 [free]) are only the address on the closing brace.

- **Pending declarations in `include/revolution/os/OS.h` (sdk-os-4):** each block carries a `Pending:` comment
  naming its owner. Whoever defines one of these in its owning header must delete the copy in `OS.h` in the same
  commit (otherwise R1/R2 fire on main):
  - DVD: resolved in sdk-io-2. The block is gone; `OS.h` includes `revolution/dvd.h`, which declares them.
  - Other libraries: `__PADSpec` (PAD), `IPCCltInit` (IPC client, `ipc/ipcclt.h`), `EnableMetroTRKInterrupts`
    (MetroTRK).
  - The OS-unit items moved to their own headers (`OSExec.h`, `OSRtc.h`, `OSNet.h`, `OSFatal.h`, `OSReset.h`,
    `OSPlayTime.h`) in sdk-os-5.
  At merge there is no second declaration of any of them on main; the checker passes on all of `src/`.
- **Pending declarations for other libraries (sdk-os-5):** declared where the OS units need them, each block marked
  `Pending:` with its owner. Whoever defines one in its owning header deletes the copy in the same commit:
  - VI (sdk-io-2): the `include/revolution/vi.h` pending block: `__VISetRGBModeImm`, `VIWaitForRetrace`, `VIInit`,
    `VIFlush`, `VIConfigure`, `VIConfigurePan`, `VISetNextFrameBuffer`, `VISetBlack`, `VIGetRetraceCount`,
    `VIRetraceCallback`, `VISetPreRetraceCallback`, `VISetPostRetraceCallback` (moved there from `OSFatal.h` at
    review). The VI library has no lane yet, so the block stays; `VIResetDimmingCount` moved from `OSStateTM.h` into
    the non-pending part of `vi.h` in sdk-io-2 (`OSStateTM.h` includes `vi.h`).
  - DVD: resolved at the sdk-io-2 merge. `__DVDPrepareReset`, `__DVDGetCoverStatus` and `__DVDPrepareResetAsync`
    are declared once, in `dvd/__dvd.h` (OSReset and OSExec include it); `include/revolution/dvdlow.h` was removed:
    its types and prototypes are the SDK's in `dvd.h` (`DVDPartitionInfo.gamePartition` and
    `DVDGameTOC.partitionInfos` are pointers, so OSExec casts them to `u32` as tp's OSExec.c does), and
    `__DVDLayoutFormat` is the `vu32` in `__dvd.h`.
  - Minimal headers, to be completed rather than duplicated: `ai.h` (DMA entry points), `gx.h` (`GXRenderModeObj`,
    `GX_TF_I4`/`GX_TF_IA4`, `GXAbortFrame`), `sc.h` (`SCGetCounterBias`, `SCIdleModeInfo`, `SCGetIdleMode`),
    `nand.h` (`NANDStatus`, the `NANDPrivate*` calls, result codes), `ipc/ipcclt.h` (`IPCCltReInit`, `IOS_Close`),
    `pad.h`, `esp.h` (ES proxy; merged with sdk-io-2's copy at its merge, see "DVD and DB rulings (sdk-io-2)"),
    `stdio.h` (`snprintf`), `string.h` (`strncmp`).
  At merge none is declared a second time on main; the checker passes on all of `src/`.
- **Partial layouts and invented names (sdk-os-5, R27/R7):** `ESTitleMeta` and `DVDGameTOC` are now the SDK
  structures from tp (sdk-io-2 merge; see "DVD and DB rulings (sdk-io-2)"). Invented names: `OSGetSettingPagePath` (OSReset), the ES limit codes
  `ES_LIMIT_*`, the macros `NANDBOOT_*`, `STATE_LAST_SHUTDOWN_RELAUNCH`, `ES_ERR_NO_TICKET_VIEW` (OSLaunch),
  `PLAYTIME_*`, `ES_ERR_NO_TICKET`, `TITLE_ID_BYTES` (OSPlayTime), `OS_FATAL_*` (OSFatal); `ESTitleIdAligned`.
- **Minimal headers for libraries no lane owns (sdk-os-4):** `revolution/gx.h` (only `GXColor`) declares what the OS
  units call. It is to be completed, not duplicated, when GX's header work is done (`revolution/ipc/ipcclt.h`,
  `revolution/nand.h` and `revolution/sc.h` are now completed by their own units). `__ArenaLo`/`__ArenaHi` were added to `__ppc_eabi_linker.h`
  (linker-script symbols, beside `_stack_addr`).
- **Accepted renames (sdk-os-4):** `IOS_Ioctl` (0x80242E30), `VIResetDimmingCount` (0x8022A960), `NANDRead`,
  `NANDReadAsync`, `NANDWrite`, `NANDWriteAsync`, `NANDSeekAsync`, `NANDOpen`, `NANDOpenAsync`, `NANDClose`,
  `NANDCloseAsync`, and `NANDDelete` (0x80243F90). `NANDDelete` was the lane's least certain; the retail body settles
  it: `nandIsInitialized` (else -128, `NAND_RESULT_FATAL_ERROR`), a zeroed 64-byte path, `nandGenerateAbsPath`,
  `nandIsPrivatePath` (else -102, `ISFS_ERROR_ACCESS`: the unprivileged path, so not `NANDPrivateDelete`), the
  synchronous `fn_80240E50` (`ISFS_Delete`), then `nandConvertErrorCode`. That is `NANDDelete` with `nandDelete`
  inlined, as in tp and ogws `nand.c`; neither has a public `NANDDeleteAsync`.
- **Invented names (R7), sdk-os-4:** the STM ioctl and event macros in `OSStateTM` (`STM_IOCTL_*`, `STM_EVENT_*`,
  `STM_VIDIM_MASK_*`, `PI_INTSR_RSTSWST`), the Unicode/ANSI range macros in `OSUtf`, the low-memory macros in
  `OSIpc`, and the play-record state descriptions in the parked `OSPlayRecord`. `__VIRegs[1]` in
  `__OSShutdownToSBY`/`__OSHotReset` is the VI display-control register (ogws `VI_DCR`), unnamed here (R11, [free]).
- **`OSUTF32toANSI` dead range test (R36, open):** `utf32 > 0xFF` returns first, so the following
  `0x152 <= utf32 && utf32 <= 0x2122` table search can never run and code points such as U+20AC never map to
  their Windows-1252 bytes. The test and loop are in the retail bytes and in tp's text: an SDK bug, kept. It
  wants an R36 quirk comment at the code (byte-neutral).
- **GX `__LINE__` line arguments (free tier):** restore tp's literal line numbers in place of the remaining
  `__LINE__` arguments of the no-op register and assert macros (GXFrameBuf, GXTexture, GXPixel, GXTransform, GXPerf,
  GXLight, GXFifo, GXInit); convention under "GX rulings (sdk-gx)". Byte-neutral.
- **`LoadProjPS` shape (GXTransform, provisional listing):** find the SDK's real form of the paired-single copy in
  `GXGetProjectionv` (ogws marks its inline fake; tp copies in C, which does not give retail's bytes).
- **MSL `ansi_fp.c` (msl-trk-6):** unit name unconfirmed (no DOL string names the file); its per-object
  `-str reuse,pool,readonly` is provisional ("MetroTRK rulings (msl-trk-6)").
- **MSL unit names unconfirmed (msl-2):** `MSL_C/PPC_EABI/Src/uart_console_io_gcn.c` (`__write_console`) and
  `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_copysign.c`, `s_frexp.c`, `s_ldexp.c` take the paths of the
  zeldaret/tp and ss decomps; no DOL string names the files. Each carries the "unit name unconfirmed" comment.
- **Game unit name unconfirmed (game-bootstrap):** `game/PadRumble.cpp` (0x800067A0-0x80006C7C) is named after
  the pad rumble messages it defines (`"MsgStartBigRumble"`...); no string names the file. It carries the "unit
  name unconfirmed" comment; rename it when a class-name string evidences the unit.
- **Game unit name unconfirmed (unk_8007BAD4):** `game/unk_8007BAD4.cpp` (0x8007BAD4-0x8007BB78, two functions: the
  constructor and destructor of a 0x14-byte element class named after its constructor address `unk_8007BAD4`, the
  two-element array at 0xA030 that `fn_8007C8A4` constructs in its CSUV object). The boundary is the unit
  plan at confidence 0.16, with no sibling object. The element holds a pointer to a heap record whose only known member is
  an `unk_801A92A8` at 0xB4 (the record name `unk_SlotRecord` is invented). It carries the "unit name unconfirmed" comment.
- **Game unit name unconfirmed (lane L0265_800A26D8):** `game/unk_800A26D8.cpp` (0x800A26D8-0x800A275C) holds one
  function, `fn_800A26D8`, which walks the system attribute commands of a packet (`lbl_805297E8`) and returns the payload
  of the first with id 0, or NULL. No string or class-name evidence names the file, and the boundary is the unit plan at
  confidence 0.16 with no sibling object. The file name is the address. It carries the "unit name unconfirmed" comment;
  rename it when evidence appears.
- **Game unit name unconfirmed (lane L0000_80006C7C):** `game/unk_80006C7C.cpp` (0x80006C7C-0x80008828) is named after
  its placeholder class `unk_80006C7C` (the constructor address); no string names the class or the file. It carries the
  "unit name unconfirmed" comment; rename it when class-name evidence appears.
- **Game unit name unconfirmed (lane L0300_800AF064):** `game/unk_800AF064.cpp` (0x800AF064-0x800AF098) is named after its
  first function, `fn_800AF064`; no string or class name names the unit, and no sibling object covers it (unit plan at
  confidence 0.16). Its three parameter types are taken from the call sites only, the functions keep their retail
  symbols with `extern "C"`, and the bodies of the three stubs return 0. It carries the "unit name unconfirmed" comment.
- **Game unit name unconfirmed (lane L0076_800202D0):** `game/unk_803E0D28.cpp` (0x800202D0-0x8002039C) is named after
  its placeholder class `unk_803E0D28` (the vtable address `__vt__12unk_803E0D28`, 20 slots; the factory is fn_8001ECC0,
  the constructor is not identified). It carries the "unit name unconfirmed" comment. The unit is NonMatching: four of
  six functions are exact; the destructor is register/schedule residue only (r0/r4 for retail's r4/r5). `fn_80020350` is
  a virtual of unk_8004C25C (vtable lbl_803E0BA0, slot 0x50) placed here by address; it is a stub until that class's unit
  exists. The class's other 19 slots are placeholders in `include/game/unk_803E0D28.h`.
- **Game unit name unconfirmed (Localisation):** `climax/Localisation.cpp` (the languages and regions, the Config.xml
  reader, the "BootStrings" and "Strings" table loaders and the text lookup) is named after what it does; no string
  names the file. It carries the "unit name unconfirmed" comment; rename it when a class-name string evidences the unit.
- **Game unit name unconfirmed (unk_80009F60):** `game/unk_80009F60.cpp` (0x80009F60-0x8000A028, one function) is named
  after its first address; no string names the file. It carries the "unit name unconfirmed" comment; rename it when evidence appears.
- **Game placeholder names (game-bootstrap, R7):** every class, member function and global of `game/PadRumble`
  except the `Msg*` message objects is a placeholder under the placeholder-naming ruling (AGENTS.md): the classes
  `unk_Ramp`, `unk_Tweaks`, `unk_TweakEntry` (names invented) and `unk_801A92A8` (the 8-byte message identifier,
  constructor 0x801A92A8; perhaps RenderWare Studio's `RWS::CEventId`, unconfirmed), the `fn_` functions, the `lbl_`
  globals and the enum `TWEAK_FLOAT`/`TWEAK_BOOL` (invented).
- **`fn_800C770C__Fi` prototype (game-bootstrap, R25):** declared in `src/game/PadRumble.cpp` because its owning unit
  (0x800C770C) has no header yet; move the prototype into that unit's header when it is reconstructed. Its
  signature (`int` in, tested result out, non-member) comes from the PadRumble call site only.
- **`MIN`/`MAX` macros (game-bootstrap):** defined at the top of `game/PadRumble.cpp`; move them into a shared
  engine header when a second unit needs them.
- **GameLib flags (game-bootstrap, game-2):** `-use_lmw_stmw on` is PROVISIONAL (about a third of the game functions
  that save GPRs without FPRs use `_savegpr`, which needs a per-object `-use_lmw_stmw off`). `-O3`, `-ipa file` and
  `-str noreuse` are library-wide ("Game code rulings (game-2)").
- **Game unit names unconfirmed (game-2):** `game/unk_8000A040.cpp` (pad manager; placeholder) and
  `climax/EventHandler.cpp` (named after `CEventHandler`, the payload type string of `iMsgOnDeleteEntity`); the framework
  header names (`AttributeHandler.h`, `Memory.h`, `Display.h`, `unk_MessageInbox.h`) are invented. game-3 adds
  `game/unk_800C8598.cpp` (the listener before CColdTrigger; placeholder) and the invented header names `Map.h`,
  `Vec3.h` and `Messages.h`. The creator of the physics manager `lbl_80529170` is `game/unk_8000EE44.cpp` (fn_8000EE44,
  placeholder unit name; the class is `unk_PhysicsManager` in `include/game/unk_PhysicsManager.h`, name invented).
- **Climax helper units unconfirmed (climax-strings):** `climax/Utils.cpp` (0x801B67FC-0x801B6E44: string copies,
  RenderWare hierarchy and texture searches, byte swaps, the XML file loader) and `climax/ImageCodec.cpp`
  (0x801B6E44-0x801B86C8: the size, encoder and decoder of an 8x8-block image codec over `RwImage`) carry the "unit
  name unconfirmed" comment; the cut at 0x801B6E44 follows the callers (the codec's game callers use 0x801B6E44 to
  0x801B7B9C and nothing below), the PS2 build keeps both ranges together. The header names `Utils.h`
  and `ImageCodec.h` and the names `unk_TextureSearch`, `unk_XmlNode`, `unk_RwTexture`,
  `unk_RwTexDictionary`, `unk_RpAtomic`, `unk_RpHAnimHierarchy`, `unk_RwObject` and `RpClumpGetFrame` (RW 3.7 API
  names standing in for the SDK header) are invented or provisional. The RenderWare callees (`fn_80294C6C`,
  `RpHAnimFrameGetHierarchy`, `RpSkinAtomicSetHAnimHierarchy`, `RpClumpForAllAtomics`, `fn_80299CC8`, `fn_80299D30`) are identified by use only.
- **Unconfirmed game headers and invented names (game-4):** the header names `game/Time.h` (invented),
  `game/CBaseSpawner.h` (class name evidenced, unit 0x800C7688-0x800C8108 not reconstructed), `game/unk_NamedEntryPlayer.h`
  and `game/unk_80491B38.h` (placeholders) are unconfirmed. Invented names: `fn_80202384`'s parameters (`scale`,
  `blendTime`), `unk_ResourceManager`, `unk_SpawnerResource`, `unk_LoadCallback`, `unk_Im2DVertex`,
  `unk_File`, the render-state helpers `SetCullMode`/`SetDepthState`/`SetAlphaBlend`, CSplashScreenTrigger's
  `LoadRaster`, the attribute-command enums and the member names of the eight game-4 classes and CBaseSpawner.
  The CSplashScreenTrigger `.sdata2` mechanism (an out-of-line, dead-stripped constructor compiled first) is a lead for
  CAmbientInterferencePoint's constant order (below).
- **Display-height header and names (game-sinit-trace):** `game/DisplayHeight.h` (header name invented, "unit name
  unconfirmed"), `GetDisplayHeight`, `displayHeightF` and `displayHeight` are invented names. The bytes do not say
  which of the two reads the original routed through the const view, nor how the original spelled it.
- **CAmbientInterferencePoint data residues (game-3):** the unit's `.sdata2` constants come out in another order (the
  9.0f constant), and it reads the `.sbss2` float `lbl_80530080`, which cannot be defined in the unit without being
  folded; with the sinit and `fn_800C2304` (MATCH-PENDING above) the unit stays parked.
- **CVoicemailMessageTrigger display-height names:** `config/R5WEA4/symbols.txt` still names the unit's two
  display-height words `lbl_80529BF8` / `lbl_80529BFC`, where the display-height ruling calls for `scope:local`
  `displayHeightF` / `displayHeight`. Until they are renamed, `build/tools/match.exe --status Matching` reports
  `__sinit_\CVoicemailMessageTrigger_cpp` as differing by relocation target.
- **"Class-name hash" comments are inaccurate:** the header comments that call variables such as `lbl_80529C30` a
  "class-name hash" describe an interned name: `fn_801A7A84` tail-calls the string pool `fn_801B61E4`, which returns
  the pooled copy's address, not a hash. `build/tools/hashes.exe vars` lists the variable and string pairs.
- **`.text` size differences in parked units:** `build/tools/gatecheck.exe layout` reports a `.text` code-size
  difference as the only layout finding of the parked units `MetroTRK/Portable/mem_TRK.c`, `game/PlaneTrigger.cpp`,
  `revolution/NdevExi2AD/DebuggerDriver.c`, `revolution/NdevExi2AD/exi2.c`, `revolution/gx/GXTransform.c` and
  `revolution/os/OSExec.c`.
- **Unit name unconfirmed (`climax/unk_80008828.cpp`):** holds `fn_80008828`, the blend-mode setter (modes 0 to 12; mode 0 re-applies the current mode). No class or string evidence names the file or the function, so the file name is invented and its top comment says so. The two jump tables are compiler output owned by this unit. The blend and fog caches are declared in `include/climax/RenderState.h` (names invented there).

- **Unit name unconfirmed (game/unk_80009DD0.cpp):** the KPAD sampling step `fn_80009DD0` of the pad channel `lbl_804579C0[0]` (the `0x80006C7C` unit, split off as its own unit) carries the "unit name unconfirmed" comment; `fn_80009DD0` is parked at 64% on a register/schedule wall (retail keeps the constant 16 and the callback address in registers from entry).

- **Unit name unconfirmed (game/unk_ClumpBounds.cpp):** holds `fn_80025674`, the bounds maker of the clump (one function of the unnamed range 0x80025460-0x8002598C; the other functions of that range are in other units). Its class is invented as `unk_ClumpBounds` in `include/game/unk_ClumpBounds.h`; `symbols.txt` names the class `CBoundsUpdater` (`CheckAtomic__14CBoundsUpdaterFP8RpAtomic`), so the rename is open. The function is parked at 97.8% on a register/schedule wall (the loop test's load of `nodes`).
- **Game unit name unconfirmed (fn_80099AB4):** `game/unk_80099AB4.cpp` (0x80099AB4-0x80099B78, one function) is named
  after its address. `fn_80099AB4` opens a THP movie from a localised path, allocates its decode buffer and starts the
  player; it returns the buffer, or NULL after a failed open. It calls the operator `new[]` directly: the array-new
  expression adds a size multiply the retail code does not have. The THP info records are placeholder 16-byte types in
  `include/revolution/thp.h`. `lbl_80529768` is also read by `fn_80099A7C`; this unit defines it.

- **Game unit name unconfirmed (game/unk_8005549C.cpp):** 0x8005549C-0x800558B4, thirteen functions: six lookup
  callbacks (`fn_8005549C`, `fn_80055544`, `fn_800555EC`, `fn_80055694`, `fn_8005573C`, `fn_800557E4`) that resolve a
  named entry through `fn_800A2A18` with a cached result, plus their six empty partners and `fn_8005588C`. No string
  or class evidence names the file; the owner type `unk_LookupOwner` and its fields are invented from the accesses
  alone. The registering factory is `fn_80068D68` (`main/auto_03_80055464_text`). `fn_8005588C` is at 98% (register
  residue); the unit stays NonMatching until it closes.
- **Game unit name unconfirmed (game/unk_800B290C.cpp):** `src/game/unk_800B290C.cpp` (0x800B290C-0x800B2AA4, eight functions) is named after its first function; no string or class evidence names it, and the boundary is the unit plan at confidence 0.52, with no sibling object. Its class `unk_800B2E58` (name invented) is the cursor of fn_800B2940, and its constructor is not in this unit. Five functions are exact. fn_800B2924 and fn_800B292C are compiler-generated secondary-base thunks of the class that holds fn_800B2638 and fn_800B25BC, so they are not reconstructed here (missing from the object). fn_800B2940 is at 94% (register residue and a loop-block difference). The unit stays NonMatching.

- **Game unit name unconfirmed (fn_8009C220):** `game/unk_8009C220.cpp` (0x8009C220-0x8009C234, one function) is named
  after its address; no string or class evidence names the file, and the boundary is the unit plan at confidence 0.52
  with no sibling object. `fn_8009C220` (`OSSendMessage( &lbl_8046C120, msg, OS_MESSAGE_BLOCK )`, returns its BOOL) is
  exact. The message queue `lbl_8046C120` (a 0x98-byte object whose first 0x20 bytes are an `OSMessageQueue`) is declared
  extern in `include/game/unk_8009C220.h`; its owner is unreconstructed (fn_8009C1EC receives from it, in the asm split).

## EXIBios `-schedule off` (ruling)

`revolution/exi/EXIBios.c` is built with `extra_cflags=["-schedule off"]`; `EXIUart`, `EXICommon` and the SI units
use the plain SDK profile. Accepted as an original per-file build setting, not a lever:
- With scheduling on, 18 of the 19 EXIBios functions break (47.7% to 77.5%, `SetExiInterruptMask` 89.4%; only
  `EXIImmEx` stays 100%), including the one-statement `UnlockedHandler` (`EXIGetID(chan, 0, &id)`, 74%): retail keeps `stw r0, 0x14(r1)`
  before `li r4, 0`, the scheduler hoists the `li`. No source spelling moves a prologue store against a parameter
  move in a one-call body, so the difference is the unit's build setting, not its source.
- doldecomp/ogws builds the same file the same way: `Object(Matching, "revolution/EXI/EXIBios.c",
  extra_cflags=["-schedule off"])`, the only per-file flag among its EXI/SI units. zeldaret/tp has its revolution
  EXI units `NonMatching` with no extra flag (its Dolphin `EXIBios.c` is also built apart from its library, with
  `-O3,p`).
- The flag is recorded in the docs/toolchain.md library profile table.

## MSL `-ipa file` and alloc.c (ruling, msl-2)

- **`-ipa file` is library-wide for MSL C** (`cflags_msl`). It closes alloc `free` and `realloc` and ansi_fp
  `__num2dec` (inlined locals numbered as in retail, docs/compiler_regalloc.md "Inlined locals") and changes no
  other MSL function (clean gate green, `ninja changes` against the baseline shows only those gains). alloc.c's
  former per-object `-inline noauto` is dropped: under `-ipa file` the unit matches without it.
- **alloc.c pool handle.** `free`, `realloc`, `malloc` and `calloc` pass the opaque `__mem_pool*` that
  `get_malloc_pool()` returns, and each inline pool helper (`__pool_alloc`, `__pool_free`, `__pool_realloc`) casts it to
  its own `__mem_pool_obj* pool_obj` local, as in zeldaret/tp's `__pool_free`. That local, not the call result, carries
  the pool through the expansion, which gives retail's register order. `__pool_realloc` keeps the name EWL uses for
  realloc's worker; its body (with `__pool_resize_inplace`, an invented name) has no public reference.
- **ansi_fp `__count_trailing_zero`** follows xbret/xenoblade's form (the low word in its own local, early return
  for a zero low word); tp's loop form of `__count_trailing_zerol` is not this build's.
- **`__TRK_write_console`** is declared once, in `MSL_C/MSL_Common/console_io.h`, with MSL's console-hook
  signature; MetroTRK's `mslsupp.c` defines it with its own `u32`/`DSIOResult` spellings (same ABI).

## MSL string flags (ruling)

`cflags_msl` builds MSL C with `-str reuse -pragma "readonly_strings on"` instead of the runtime's
`-str reuse,pool,readonly`. Retail keeps MSL's small literals unpooled in `.sdata2` (`strtok`'s `""` at
0x8052DE50, `"."`, `"AM|PM"`, `"%T"`, `"NAN("`). Every `-str` spelling with `readonly` (`reuse,readonly`,
`readonly`, `reuse,nopool,readonly`, `nopool,readonly`) pools them into `.rodata` under Wii 1.0 and 1.7; the
`readonly_strings` pragma alone puts them in `.sdata2`, and every MSL unit that was already `Matching` stays 100%
with it. Accepted as the only flag form consistent with the evidence. The exact original option is unproven: the
pragma may have come from a prefix file or the build's command line; no pragma is written in source.

- **Invented queue macros in `revolution/os/OSThread` (R7):** `AddTail`, `AddPrio`, `RemoveItem` and `RemoveHead`
  are behaviour-derived names. The reference (zeldaret/tp `libs/revolution/src/os/OSThread.c`) spells the same
  macros `ENQUEUE_THREAD`, `ENQUEUE_THREAD_PRIO`, `DEQUEUE_THREAD` and `DEQUEUE_HEAD` (argument order thread,
  queue, link), but those are tp's reconstruction, not evidence of the SDK's names (ogws open-codes the queue
  operations), so neither spelling is proven. `IsSuspended` and `ASSERTREPORT` likewise; `ASSERTREPORT` has the
  same definition in tp.
- **Dead-stripped functions kept in source (`revolution/os/OSThread`):** `OSSetSwitchThreadCallback`,
  `OSIsThreadSuspended`, `OSIsThreadTerminated`, `OSDetachThread`, `OSSetThreadPriority`, `OSGetThreadPriority`,
  `OSSetIdleFunction`, `OSGetIdleFunction`, `OSClearStack`, `OSSetThreadSpecific` and `OSGetThreadSpecific` have no
  out-of-line copy in retail (inlined or stripped by the linker). Kept as in the SDK unit, the OSCache L2 precedent:
  `OSSetIdleFunction` is what keeps the static `IdleThread` alive, and without it the `.bss` layout (ending at
  `IdleThread` + 0x318 = 0x80493050) breaks. They have no address and are not counted.

## OS unit rulings (sdk-os-4)

**`#pragma global_optimizer off` in `revolution/os/OS.c` (ruling: a lever; not counted if linked).** The parked
OS.c wraps `InquiryCallback`, `OSInit` and `OSGetAppGamename` in `#pragma global_optimizer off` / `reset`, inferred
from the bytes only. Without it they score 92.7%, 90.6% and 90.6%. Neither public reference has it: zeldaret/tp
`libs/revolution/src/os/OS.c` has no `#pragma` at all, and doldecomp/ogws `OS.c` has only `#pragma unused(result)` in
`InquiryCallback`. A per-function optimiser switch with no source or build evidence is a compiler-state lever
(Section F), the same class as a steering block. Ruling: the pragma may stay in the parked draft as a record of
what the bytes need, but if OS.c is linked with it, the three functions are `MATCH-PENDING` (R17, steering) and are
not counted. What would make it authentic: evidence that the SDK built OS.c (or these functions) with another
optimiser setting, ideally a per-object flag that matches the whole unit with no pragma in source (as in the
EXIBios `-schedule off` ruling), or the same pragma in a public reference.

**The same pragma in `revolution/os/OSStateFlags` (ruling: the same lever; linked, `__OSWriteStateFlags`
`MATCH-PENDING`).** The pragma around `__OSWriteStateFlags` is the only form found that reproduces its bytes (see
the MATCH-PENDING table). It is not a library-wide build setting: `-prefix` with the pragma, applied to the whole
SDK library, changes 91 units and fails the gate (figure unverified, to be rechecked at merge), so the original
cannot have compiled the library that way. The function-scoped pragma is the minimal form tested: the rest of the
unit is 100% with or without it, and only this function needs the pragma. That leaves the source-level pragma as
the only explanation, and that explanation unproven, exactly as for OS.c. The unit is linked with the pragma
(bytes identical, gate green) and the function is not counted. The rest of the unit (`__OSReadStateFlags`, the
statics) is plain reconstructed source and counts.

**Compiler-generated `@NNN` names in `symbols.txt` (ruling: only names dtk produced; never guessed).** The `@NNN`
names in `symbols.txt` come from dtk's signature analysis (the 33 in the seeded file, e.g. OSError's `@641`), which
takes them from symbol-bearing SDK objects. A number cannot be derived from a stripped DOL. The lane's `@985`,
`@986`, `@990` and `@1013` for four `OSStateTM` strings were extrapolated from dtk's `@980`/`@981`; our own build
names the same literals `@476`, `@477`, `@481` and `@504`. Ruling: a guessed `@NNN` is not allowed; such literals
keep their `lbl_` names until dtk or another symbol source names them (no descriptive name either: a compiler
literal has no source name). The four were reverted to `lbl_80439918`, `lbl_80439924`, `lbl_8043995C` and
`lbl_80439994` at review; objdiff still pairs them (OSStateTM 100% code and data) and the gate is green, so the
guessed names were never needed.

## OS unit rulings (sdk-os-5)

Lane sdk-os-5 linked `OSFatal`, `OSFont`, `OSReset`, `OSRtc`, `OSNandbootInfo`, `OSPlayTime`, `OSLaunch` (44 counted
functions) and `OSPlayRecord` (4 functions, `MATCH-PENDING` above). Reference: zeldaret/tp `libs/revolution/src/os/`,
with doldecomp/ogws `src/revolution/OS/` as a second opinion.

**`OSPlayRecord`'s `RecordCheckSum` (ruling: steering; the unit's four functions are `MATCH-PENDING`).** See the
table above. Both references keep `RecordCheckSum` static with an `OSPlayRecord*` parameter, as `OSNandbootInfo`'s
`CheckSum` is here. Reverting to that shape gives 99.9% on `PlayRecordCallback` and `__OSStopPlayRecord`.

**`OSPlayTime` error branches (ruling: authentic once spelled as the SDK assertion; counted).** The lane wrote an
invented `ES_FAILED(rv)` macro expanding to `((void)(rv))` in the five ES-failure branches, a stand-in for tp's
`ASSERTMSG1LINE(n, FALSE, "Failed: %d\n", rv); rv = rv;`. An invented empty carrier is the Section F empty-statement
pattern; the SDK's own release-build assertion is its human form. At review the five sites became
`ASSERTMSG1(FALSE, "Failed: %d\n", rv);` (the SDK macro in `revolution/os/OSError.h`, `((void)0)` outside debug
builds) and the invented macro was deleted: same bytes, gate green. The statement is load-bearing (removing it drops
`__OSGetPlayTime` to 98.2% and `__OSInitPlayTime` to 95.0%: the failure branches merge), which is what a debug-only
assertion at that point explains: the original branch held a statement that compiles to nothing in release. The
sites are in `__OSPlayTimeGetConsumption` and `__OSGetPlayTimeCurrent` (static, inlined, no address) and in
`__OSInitPlayTime`. tp's extra `rv = rv;` is not needed and not written (R17).

**Stripped functions kept for data order (`OSReset`, `OSFont`; not counted).** `OSReset.c` keeps `OSSetBootDol` and an
invented `OSGetSettingPagePath` (the settings-page path switch; the panic text names its SDK caller
`OSReturnToSetting`, whose body is unknown) because their strings remain in retail `.data` while the code was
dead-stripped. Their `OSPanic` line numbers (1204, 985) are invented: the code is stripped, so no retail byte holds
them, and the gate is green whatever they are. They are marked as unknown placeholders at the code (review). Neither
reference has these functions. `OSFont.c` keeps `ExpandFontSheet`, `OSInitFont`, `OSGetFontTexture`,
`OSGetFontWidth` and `OSSetFontWidth` (tp's text; stripped) and `OSGetFontEncode`, `GetFontSize`, `ReadROM`,
`IsSjisLeadByte`, `IsSjisTrailByte` (inlined); none has an address. `OSPlayRecord`'s `RecordCheckSum` is inlined too.
Three `Zenkaku2Code` entries tp lacks were restored from the retail table (data, verified by the gate).

**Per-object flags.** `OSFatal.c` is built with `-fp_contract off` (ruling: authentic build setting; recorded in the
docs/toolchain.md profile table). Evidence: `RGB2YUV` (inlined into `Halt`), the unit's only float code, has no fused
multiply-adds in retail. Both references build the whole RVL SDK with `-fp_contract off` (tp
`cflags_revolution_base`, ogws `cflags_rvl`); this project builds the SDK with the base `-fp_contract on`, and no
other matched SDK unit has yet shown a difference. Open item: try `-fp_contract off` library-wide (it would replace
the per-object flag if every SDK unit keeps its score). `OSNet.c` (parked) carries `-inline noauto` per object; neither
reference uses it (ogws and tp build OSNet with their library flags). With the library's flags alone three of
OSNet's eight functions are exact, with `-ipa function` or `-ipa off` in its place four, with it all eight. It is not
a flag of the `os` library: built with the flag on the whole library (`RVLSDKLib("os", ...)` with `lib_cflags`, regenerated before measuring),
20 of the 34 `os` units lose functions: static helpers that retail inlines (`SetTimer` in OSAlarm, for one) come out of
line. The user's ruling of 2026-11-16: the flag on OSNet.c alone is a lever, as in the `global_optimizer` ruling
above (this unit is its only evidence), and the unit stays parked until evidence for the setting appears.
`#pragma auto_inline off` at the top of OSNet.c gives the same bytes without the flag and is not used either. It affects no count while OSNet
is parked.

**`goto` in `ScreenReport` (OSFatal; ruling: authentic, R14 [checked]).** The line-break and wrap paths jump back to
the top of the function to start the next row. tp has the same `goto loop_1`, ogws `goto new_line`; the bytes need
the backward branch to the row setup. The label was renamed to ogws's `new_line` at review (byte-neutral).

**Bound-first range tests in `OSFont` (ruling: authentic, as the OSUtf ruling).** `GetFontCode`, `OSSetFontEncode`,
`IsSjisLeadByte` and `IsSjisTrailByte` write `lo <= x && x <= hi`. Substituting tp's `x >= lo && x <= hi` at review
drops `GetFontCode` to 90.2%, `OSSetFontEncode` to 73.1%, `ParseStringS` to 90.8% and `ParseStringW` to 95.4%: retail
was written with the bound first. Operand order only; nothing added.

**Renames outside the OS units (ruling: accepted; R7).** `snprintf` (0x8019BD20), `DVDReadAbsAsyncPrio`,
`DVDGetCommandBlockStatus`, `DVDSetAutoInvalidation`, `DVDResume`, `__DVDPrepareResetAsync`, `DVDLowFinalize`,
`DVDLowReadDiskID`, `DVDLowOpenPartition`, `DVDLowOpenPartitionWithTmdAndTicketView`, `DVDLowClosePartition`,
`VISetPreRetraceCallback`, `VISetPostRetraceCallback`, `VIInit`, `VIWaitForRetrace`, `VIConfigure`, `VIConfigurePan`,
`VIFlush`, `VISetNextFrameBuffer`, `VISetBlack`, `VIGetRetraceCount`, `AIRegisterDMACallback`, `AIInitDMA`,
`AIGetDMAStartAddr`, `AIGetDMALength`, `GXAbortFrame`, `IPCCltReInit`, `IOS_Close`, `NANDPrivateCreate`,
`NANDPrivateDelete`, `NANDPrivateGetStatus`, `NANDPrivateOpen`, `SCGetIdleMode`, `SCGetCounterBias`,
`ESP_LaunchTitle`, `ESP_GetTicketViews`, `ESP_GetConsumption`. Each is the callee tp's OS sources name at that call
site (checked at review: every name occurs in tp's `OSExec.c`, `OSFatal.c`, `OSPlayTime.c`, `OSNandbootInfo.c`,
`OSReset.c`, `OSNet.c` or `OSLaunch.c`), and in the linked units the callers' relocations land on these addresses
(gate green). The DVD, VI and AI names fall in the territory of the running lane sdk-io-2, which must reconcile them
on rebase (its branch also adds `include/revolution/esp.h`, an add/add conflict with this lane's `esp.h`, and edits
`dvd.h`, `OS.h`, `vi.h` and `symbols.txt`).

**`__OSNextPartitionType` and `ESTicketView` (ruling: verified).** `lbl_8052AAA0` (.sbss, 4 bytes) is renamed
`__OSNextPartitionType`, tp's name for the word `__OSReboot` stores from low memory 0x3194 and `OSExec` compares
partition types against; defined in the parked `OSExec.c`, declared in `OSExec.h`. `ESTicketView` in `esp.h` is the
ES ticket view under `#pragma pack(push, 4)`: `view` 0, `ticketID` 4, `deviceType` 0xC, `titleID` 0x10, `accessMask`
0x18, `reserved[60]` 0x1A, `cidxMask[64]` 0x56, `padding` 0x96 (`// pad:` note, the ES name), `limits[8]` 0x98, size
0xD8, the IOS layout.

**Reader-test sample (counted).** `OSFatal`, `Halt`, `OSLoadFont`, `OSGetFontTexel`, `OSShutdownSystem`, `OSRestart`,
`__OSSyncSram`, `__OSRelaunchTitle` and `__OSWriteNandbootInfo` were read against the reader test and Section F: tp's
text with named sizes and modes, no fake code. Notes:
- `OSFatal` and `Halt` spin with `do {} while (...)` on the retrace count and the EXI register: empty-bodied wait
  loops, genuine (the loads are in the binary).
- `OSShutdownSystem` reads the IOS revision into a local that is not read again: the call is in the binary and tp has
  the same local (an out-parameter, not a dummy).
- `GetFontSize` reads the decoded size at +4 of the ROM font image through a byte pointer; a `// byteptr: file format`
  note was added at review (R4 allows file-format reads).
- Readability, [free]: `OSLoadFont` switches on the literal encodings 0..5 rather than `OS_FONT_ENCODE_*`;
  `ConfigureVideo` uses raw TV modes (2, 0, 22, 20, 4) and `__VIRegs[54]`; `OSInitFont` and `GetFontCode` carry
  unnamed sizes and code bounds (R11); `__OSRelaunchTitle` sets the arena to raw 0x81280000/0x812F0000;
  `__OSWriteNandbootInfo` opens with access mode 2 (`NAND_ACCESS_WRITE`).

**Tooling settings (accepted).** `splits.txt` gives `OSStateFlags`' `.bss` split `align:32` and ends `OSPlayTime`'s
`.bss` at 0x80493430; `ninja changes` lists the old `auto_08_80493440_bss` as going from 100% to 0% because it is now
`auto_08_80493430_bss`: not a regression. `OSRtc`'s `Scb` is aligned to 32 in source (`__attribute__((aligned(32)))`),
as in tp and ogws.

## MetroTRK rulings (msl-trk-4)

**Non-variadic `OSReport` prototype in `include/TRK/trk_glue.h` (ruling: authentic original declaration; counted;
the RULE-EXCEPTION marker stays as the record of an original quirk). Refined in msl-trk-5: the declaration is now
unprototyped, see "MetroTRK rulings (msl-trk-5)"; the CR6 evidence below stands.** Retail calls `OSReport` (the SDK function)
from two MetroTRK units with different call shapes: `TRKPostEvent` (nubevent) passes its string with no `crclr cr1eq`
before the `bl`, while `TRKRequestSend` (support) sets it before both of its calls. A call through a variadic
prototype always gets the CR6 setup, so the translation unit that built nubevent saw a non-variadic declaration,
and support.c saw the SDK's variadic one. The binary itself is the evidence; zeldaret/tp's MetroTRK `usr_put.c`
(`extern void OSReport(char* fmt);`, with the note that the variadic form adds a `crclr`) is a reconstruction that
reached the same conclusion, not original source, so the exact spelling (`char*`, the header it lives in) is
unproven. It is not a lever: no other spelling of the call produces those bytes, and nothing in the declaration
steers the compiler beyond the one fact retail shows. It bends R6 the way the original did (R36: the original's
quirk kept and labelled). Constraint: no translation unit may include both `TRK/trk_glue.h` and
`revolution/os/OSError.h` (the two prototypes conflict). At review `trk_glue.h` is included by nubevent, nubinit and
targcont only, `OSError.h` by support only, and no `TRK/` or `amc/` header pulls in `OSError.h`
(`revolution/os/OS.h` does not include it).

**`cflags_trk` string flags (ruling: consistent with the evidence; unconfirmed).** The MetroTRK units are built
with the runtime profile but `-str reuse,pool` (no `readonly`). support.c's two `OSReport` strings sit in `.data`
as one pooled object, `@stringBase0` at 0x80427468 (0x41 bytes), the second string addressed as
`@stringBase0+0x1F`; that is pooling into a writable section, which `-str reuse,pool` gives and the runtime's
`-str reuse,pool,readonly` would move to `.rodata`. nubevent's and nubinit's single strings stay in `.data` at
0x80427428 and 0x80427448 (`lbl_` names, as dtk left them). objdiff reports 100% data for all nine units and the
gate (byte-exact `.data`) is green. The exact original option set (pooling from a flag, a pragma or a prefix file;
`-gccinc`, `-common off`, `-inline auto`) is unproven, as for the MSL string flags.

**`.data` split ends padded in `splits.txt` (ruling: tooling constraint, accepted).** nubevent's `.data` split ends
at 0x80427448 and nubinit's at 0x80427468, past the last string byte (0x80427445 and 0x80427465): dtk rejects a
`.data` split bound that is not aligned, and the compiler pads each unit's `.data` to its alignment anyway (both
ends are 4- and 8-byte aligned here). The gate is green with these bounds. The old auto unit
`auto_07_80427448_data`, which `ninja changes` lists as going from 100% to 0%, was absorbed into nubinit's split;
it is not a regression.

**Names (ruling: accepted with open R7 items).**
- The TRK function names, unit paths and globals (`gTRKEventQueue`, `gTRKBigEndian`, `gTRKInputPendingPtr`,
  `TRK_mainError`) follow zeldaret/tp `libs/TRK_MINNOW_DOLPHIN`, the module's reference by the user's ruling.
- The DB layer (`DBInitComm`, `DBInitInterrupts`, `DBQueryData`, `DBRead`, `DBWrite`, `DBOpen`, `DBClose`), the
  Metrowerks critical section (`MWInitializeCriticalSection`, `MWEnterCriticalSection`, `MWExitCriticalSection`)
  and `TRK_flush_cache` are tp's names for the functions at the same place in the call graph.
- `TRKGetFreeBuffer`, `TRKSetBufferPosition` and `TRKAppendBuffer_ui8` were tp names assigned from call shape;
  confirmed by the msgbuf reconstruction in msl-trk-5 (tp's bodies match).
- Invented (open R7): the `struct TRKBuffer` members (`inUse`, `length`, `position`, `data`); the `CommandReply`
  members and unions (`replyError`, `arg`, `param`, `replyLength`, `replyPosition`, `unused`); the enums
  `DSMsgCommand`, `DSIOResult` and `DSReplyError`; the `DSError` enumerators `DS_EventQueueFull`,
  `DS_MessageBufferReadError` and `DS_Error800` (values from the bytes); `UARTError`; the gdev locals and the
  `GDEV_BUF_SIZE` macro; the macros `TRK_ENDIAN_PROBE_BIG`/`_LITTLE` (nubinit) and `TRK_FIRST_EVENT_ID` (nubevent).
- Placeholders `fn_80192B00` and `fn_801931F0` became `TRKMessageSend` and `TRKReadBuffer_ui8` in msl-trk-5.

**Reader-test sample (counted).** `TRKNubWelcome`, `TRKPostEvent`, `TRKGetNextEvent`, `CircleBufferReadBytes`,
`CircleBufferWriteBytes`, `TRKTestForPacket`, `TRKProcessInput`, `TRKSuppAccessFile` and `TRKRequestSend` were
read against the reader test and Section F: tp's text with named constants and documented structs, no fake code.
- `CircleBufferReadBytes` tests `size < availSize`, `CircleBufferWriteBytes` `availSize >= size`: tp's text, the
  original's asymmetry.
- `TRKTestForPacket` assigns `TRKGetFreeBuffer`'s status to `result` and, when the head cannot be read, releases
  that status as a buffer ID: an original bug (tp's text; the bytes release the status). Labelled at the code at
  review (R36).
- `TRKTestForPacket` reads the packet length as `((u32*)packetBuf)[0]` and support.c reads replies through
  `(CommandReply*)buffer->data`: views of a byte buffer at offset 0 through a typed head, not offset arithmetic
  (R4 passes). `TRKSuppAccessFile` and `HandleOpenFileSupportRequest` read `replyBuffer`/`tempBuffer` even when
  `TRKRequestSend` failed (uninitialised on that path): tp's text and the original's bug; the label is open work.
- `TRKInitializeEndian` reads the probe bytes through `*(u32*)bendian`: tp's text, the point of the function.

## DVD and DB rulings (sdk-io-2)

**`esp.h` merge (ruling: one header; SDK types).** main (sdk-os-5) and the lane each added `include/revolution/esp.h`.
The merged header keeps main's `ESTicketView` (0xD8 bytes, `#pragma pack(4)`, the field layout OSLaunch, OSPlayTime
and OSReset were matched with; tp's spelling of the same structure has the same offsets), `ESLpEntry`,
`ESTitleId`, `ESSysVersion`, `ESTitleIdAligned`, the `ES_LIMIT_*` codes and main's `ESP_*` prototypes, and adds
from the lane (tp `esp.h`) `ESTicket` (0x2A4 bytes) with its field types for `dvd.h`'s `DVDGamePartition` and
`DVDPartitionParams`. The IOS crypto types (`IOSCHash`, `IOSCSigRsa2048`, ...) moved to a new
`include/revolution/ios/iostypes.h` (tp `private/iostypes.h` subset), which `esp.h` includes. One departure from
"keep main's types": main's `ESTitleMeta` was a 0x4A00-byte placeholder (`reserved0`/`sysVersion`/`reserved1`), and
`DVDPartitionParams.padding3[OSRoundUp32B(sizeof(ESTitleMeta)) - sizeof(ESTitleMeta)]` is then a zero-length
array that MWCC rejects in every unit (OS.h now includes dvd.h). The merged header has tp's `ESTitleMeta`
(signature, `ESTitleMetaHeader`, 512 `ESContentMeta`; 0x49E4 bytes, rounded to the 0x4A00 OSExec allocates), with
`sysVersion` at 0x184 as before; OSExec reads `meta->head.sysVersion` (tp's text). The lane's unused `ESTmdView`
family and `ESP_GetTmdView`/`ESP_GetDataDir` were dropped.

**Pending-declaration dedupe (ruling: done at merge).** The lane removed the DVD pending block from `OS.h` (OS.h
includes `dvd.h`) and moved `VIResetDimmingCount` from `OSStateTM.h` to `vi.h`; main had moved the VI prototypes into
`vi.h`'s pending block and added `include/revolution/dvdlow.h` and a `__dvd.h` pending block in `dvd.h`. Merged: one
declaration of each (the lists are under "Open items"); `dvdlow.h` is gone. Checker clean on all of `src/`.

**R25 false positive on `ATTRIBUTE_ALIGN` (ruling: checker bug, fixed in the rule checker (`tools/c/tools/check_rules`) with a regression test).** The
prototype scan took the first `(` of a declaration as a parameter list, so `static u32 x ATTRIBUTE_ALIGN(32);` was a
"prototype of `ATTRIBUTE_ALIGN`" and `static u8 buf[OSRoundUp32B(n)] ...;` one of `OSRoundUp32B` (7 hits: 6 in dvd.c,
1 in OSPlayTime.c). The scan now skips `(` inside array bounds and strips `ATTRIBUTE_ALIGN(...)` like
`__attribute__(...)`. The other 22 R25 hits in dvd.c are real `()` parameter lists, not false positives (the
lane's "28 hits" counted them together).

**`__DVDLayoutFormat` is `vu32` (ruling: evidenced).** Dropping the volatile leaves DVDOpen and DVDFastOpen at
100% but takes dvd.c `stateReadingFST` to 93.9% (its four reads in two statements merge). The evidence note in
`__dvd.h` named the wrong functions; corrected at review. The `(volatile DVDCommandBlock*)` casts in dvdfs
`DVDReadPrio`/`DVDSeekPrio` sit in dead-stripped functions (no retail address, not counted); review them with dvd.c's.

**Header requests (ruling: resolved where clean, the rest open).**
- `ATTRIBUTE_ALIGN`: main already defines it in `revolution/types.h`; `DECL_WEAK` (`__declspec(weak)`) moved there
  too. The guarded copies in `dvd/__dvd.h` were removed.
- `OSCachedToPhysical`: moved from db.c into `revolution/os.h` beside `OSPhysicalToCached`.
- `db.h` now includes `db/DBInterface.h`; the second `__DBIsExceptionMarked` prototype (in DBInterface.h, with a
  `u8` parameter) was dropped, db.h's `__OSException` one kept.
- Open: `__OSLaunchPartitionType` and `__OSLockedFlag` are declared in `dvd/__dvd.h` but owned by OS
  (`__OSLockedFlag` as the absolute-address object `u8 __OSLockedFlag : 0x80003187;`, a definition in a header that
  every DVD unit includes; types unconfirmed). Move them to the OS headers when an OS lane touches them.
- Open: `static inline int tolower(int c)` (and `isspace`) in `include/ctype.h`, added for dvdfs `isSame`. The logic
  is MSL's (range guard, then the locale's lower map), but the spelling differs from MSL's as recalled (`inline` without `static`,
  `0x100`, an `(int)` cast) and the header has no `// Reference:` line. Byte-neutral for dvdfs; replace it with the
  MSL text when the MSL ctype unit is reconstructed.

**Library map.** 0x8021EA70-0x8021EFA0 (11 functions) is not DB but the SDK MTX library: the Gormiti link map
holds all eleven in `mtx.a` (`mtx.o` 0x8021EA70-0x8021EE98, `mtxvec.o` `PSMTXMultVec`, `mtx44.o` `C_MTXOrtho` at
0x8021EF00); `docs/library_map.md` ends DB at 0x8021EA70 and lists the range separately.

**Reader-test sample (counted).** `DBInit`, `__DBIsExceptionMarked`, `DVDConvertPathToEntrynum`, `DVDOpen`,
`entryToPath` and all six dvdqueue functions were read against the reader test and Section F: the reference text
(tp/ogws), no dummy locals, no steering, no fixed offsets. Notes:
- `DVDConvertPathToEntrynum`'s `goto next_hier` after the not-found return is the SDK's own shape (tp's text).
  `ptr = pathPtr;` before the name scan reuses the end cursor for the component start; byte-neutral (100% either
  way), so it is not steering; left in the reference's form.
- `DVDOpen` (0x2B4 bytes) inlines `DVDGetCurrentDir`, `DVDConvertEntrynumToPath`; `entryToPath` inlines
  `myStrncpy`. dvdfs.c's header said `DVDGetCurrentDir` was dead-stripped; corrected at review.
- Style items left open ([free]): `0 > entry` and `DVDGetCurrentDir(currentDir, 128)` in `DVDOpen`, the literal
  `1` for `DVD_COMMAND_READ` in `DVDDumpWaitingQueue`, the misspelt `extention` locals (tp's names).

## MetroTRK rulings (msl-trk-5)

**`OSReport` declared without a prototype (ruling: authentic; counted under a RULE-EXCEPTION for R6 and R25).**
`TRKMessageSend` calls the SDK's `OSReport` with two arguments (format and the UART error) and `TRKGetFreeBuffer`
and `TRKPostEvent` with one; none of the three calls has the `crclr cr6` a call through a variadic prototype gets.
msl-trk-4's `void OSReport(char* fmt);` cannot take the two-argument call, and no fixed parameter list fits both
arities, so the translation units that built nubevent, msgbuf and msg saw no prototype: an old-style declaration
`void OSReport();` (or an implicit declaration, which C89 allowed and which compiles the same calls; the bytes
cannot tell them apart). The old-style declaration is the closer authentic spelling that keeps a visible
declaration for the reader. tp's GameCube MetroTRK has no such call (its `TRKMessageSend` has no report), so there
is no public text to copy. Against the rules: R25 ("every function has a prototype in scope") is the rule it
breaks, a [checked] rule the bytes defeat; R6 is bent the same way msl-trk-4 recorded. Neither is a [match] rule, so
the functions count; `// RULE-EXCEPTION: R6/R25, ...` sits at the declaration and the checker reports the R25 finding
as excepted. Checked at merge: nubevent, nubinit, targcont and support stay 100% (support.c still includes
`OSError.h`'s variadic prototype and keeps its `crclr`); the constraint that no unit includes both `TRK/trk_glue.h`
and `revolution/os/OSError.h` stands (an unprototyped declaration is not compatible with a variadic prototype).

**`TRKMessageSend`'s sequence stamp (ruling: authentic shape; rewritten at review to the plain spelling).** The DOL's
string `MetroTRK - TRK_WriteUARTN returned %ld` and the `lhz`/`sth` on the 2-byte `.sbss` word at 0x8052A4C0 show
that this Wii MetroTRK stamps a 16-bit sequence number at offset 6 of every outgoing message, skipping 0; tp's
GameCube `msg.c` has neither (it only traces the write result), so the body has no public reference. The lane's
form (`if (gTRKMsgSeq == 0) gTRKMsgSeq = 1;`, then `seq = gTRKMsgSeq;` stored and `gTRKMsgSeq = seq + 1;`) was not
steering, but the lane's note that only it reached 100% was wrong: `((CommandReply*)msg->data)->sequence =
gTRKMsgSeq++;` after the same zero test compiles to identical bytes (retail keeps the 1 in a register and never
stores it: the store is dead before the increment's store). The review adopted that spelling, which a programmer
would type. Names: `gTRKMsgSeq` (static), `CommandReply.unused5` and `CommandReply.sequence` are invented (open R7).

**Names and dead-stripped functions.** The msgbuf names follow tp's `msgbuf.c` (`TRKResetBuffer`, `TRKAppendBuffer`,
`TRKReadBuffer`, `TRKAppendBuffer1_ui32`/`_ui64`, `TRKAppendBuffer_ui32`, `TRKReadBuffer1_ui64`, `TRKReadBuffer_ui8`/
`_ui32`) and `fn_80191BD0` became tp's `TRKWriteUARTN`; the matched bodies confirm them. `TRKAppendBuffer1_ui8`,
`TRKReadBuffer1_ui8` and `TRKReadBuffer1_ui32` stay in source in tp's order and have no out-of-line copy (inlined
into the `_ui8`/`_ui32` array functions); not counted. `gTRKMsgBufs` (tp's name) is the 3 x 0x88C-byte pool.
`dserror.h` now has tp's full `DSError` enumeration.

**Reader-test sample (counted).** `TRKMessageSend`, `TRKGetFreeBuffer`, `TRKAppendBuffer1_ui64` and
`TRKReadBuffer_ui32` were read against the reader test and Section F: tp's text (msgbuf) and plain C (msg), no
dummy locals, volatile or steering. `TRKGetFreeBuffer` leaves its loop with `i = TRK_MSGBUF_COUNT;` (tp's text, the
original's way of breaking). `TRKReadBuffer1_ui32` sizes its swap buffer `sizeof(data)` on a `u32*` (4 bytes, the
pointer size: tp's text; the original's slip, harmless here). Header comments and address trailers were added at
review to the ten msgbuf functions that lacked them.

## Reader-test rulings (counted; no allow-list effect)

Review of the MSL stdio, multibyte and wide-string units. Each spelling below was reported as needed for the
bytes; each was judged against the reader test and Section F.

| function | spelling | ruling |
|---|---|---|
| `__fwrite` | local declaration order; `bytes_to_go -= num_bytes` before `file->buffer_ptr += num_bytes` | authentic. Declaration order is a legitimate lever, and the statement order is the Metrowerks order (advance the source pointer and the counts, then the buffer pointer and length). No local is invented: `always_buffer` (text or fully buffered) and `buffered` (any buffering) each carry a meaning and are read. |
| `_ftell` | `unsigned char kind = file->file_mode.file_kind;` and `!(kind == __disk_file \|\| kind == __console_file)` | authentic. The file kind is read once and tested twice; public GC/Wii MSL reconstructions use the same local, as recalled (`tmp_kind`). The `!(a \|\| b)` test is the Metrowerks wording. |
| `_fseek` | `error = 1` before `buffer_len = 0` | authentic: the expansion order of the Metrowerks `set_error` macro (open item above). |
| `fflush` | `if (file->file_mode.file_kind != __disk_file) position = 0; else position = ftell(file);` | authentic: the Metrowerks `fflush` wording. |
| `wcstombs` | working pointer `pwc` over `pwcs`; direct `encode_wc` call | authentic: the Metrowerks `wcstombs` walks a copy of `pwcs`. The direct call is `wctomb`'s body, not an invented temporary (R16 open item above). |
| `fwide` | `int orientation` switch variable | authentic: the Metrowerks `fwide` switches on an `int orientation` local. The parameter `mode` doubles as the result (R30 [checked], noted, not fake). |
| `sscanf` | all-blank loop in `static __is_blank` | not fake code: the loop is in the binary. Name and helper invented (open item above). |
| `strcmp` | separate `c1`/`c2` locals in the byte loop after `bytecmp`, beside `a`/`b` | authentic. Both are read and each holds one byte of the tail comparison (R30 [checked]; not the Section F dummy-local pattern: no unread or meaningless temporary). The body is the Metrowerks word-at-a-time `strcmp` with its `register` locals, `K1`/`K2` constants and `bytecmp`/`adjust` labels. |
| `strtok` | `static char* empty = ""` stored back into `next` at the end of the string | authentic: one `""` object in `.sdata2`, referenced by both statics' initialisers and the reset, as retail. |
| `__OSDispatchInterrupt` | `intmr = __PIRegs[PI_INTMSK];` read into a local before `if (intsr == 0 \|\| (intsr & intmr) == 0)` | authentic. Retail loads the PI mask (`lwz r0, 0x3004(r3)`) unconditionally, before the `intsr == 0` branch. `__PIRegs` is volatile, so `(intsr & __PIRegs[1])` inside the condition would load it only on the second test; the read order is semantic, not a register lever. No meaningless temporary: the local holds the interrupt mask register. |
| `__OSDispatchInterrupt` | `handler` declared before `interrupt` | authentic: declaration order is a legitimate lever (ruling on `__fwrite`); both locals carry a meaning and are read. |
| `OSInterrupt.h` | `__RAS_OSDisableInterrupts_begin`/`_end` declared `void f(void)` instead of `u8[]` | correct. The `entry` directives in the `OSDisableInterrupts` asm body define them as code labels in that unit; the SDK declares them as functions. `OSContext` (`OSLoadContext` takes their `@ha`/`@l`) stays 100% in the gate. |
| `SetRun`, `UnsetRun` (OSThread) | `ASSERT(...)` statements, `((void)0)` in release (`include/revolution/os/OSError.h`) | authentic. The SDK's own assertion form (`ASSERT`/`ASSERTMSG`/`ASSERTMSG1`, `OSPanic(__FILE__, __LINE__, ...)` in debug builds, `((void)0)` otherwise). `UnsetRun` stays out of line only with its three asserts present, and zeldaret/tp's revolution `OSThread.c` has three assertions at exactly these points of `UnsetRun`; ogws instead forces it out of line with a `DECOMP_DONT_INLINE` attribute, which is the lever this replaces. |
| `OSCheckActiveThreads` | `ASSERTREPORT(1522, ...)` ... `ASSERTREPORT(1597, ...)` with literal line numbers | authentic. The numbers are the retail immediates passed to `OSReport`/`OSPanic`, i.e. the original `__LINE__` values of `OSThread.c`; a literal line keeps the bytes without padding the file. Same macro and form as zeldaret/tp. |
| `RunQueueBits`, `RunQueueHint`, `Reschedule` (OSThread) | `static volatile` with `// volatile:` notes | accepted as SDK form: ogws declares all three `volatile`; zeldaret/tp has `RunQueueBits`/`RunQueueHint` volatile and `Reschedule` plain. `SelectThread` spins on `RunQueueBits` with interrupts enabled, which needs `volatile`; the gate is green with all three. |
| `OSCreateThread` | `*(u64*)&thread->context.fpr[i] = 0xFFFFFFFFFFFFFFFFULL` (and `psf[i]`) | as the `OSSetErrorHandler` ruling: a NaN bit pattern through a typed member, original SDK form. |
| `__OSThreadInit` | `(u8*)_stack_addr`, `(u32*)_stack_end` | boundary casts from the linker-defined `char[]` symbols (`__ppc_eabi_linker.h`) to the `OSThread` member types (R19 allows casts at real boundaries). |
| `OSLockMutex`, `OSUnlockMutex`, `__OSUnlockAllMutex` | the `PushTail`, `PopHead`, `PopItem` queue macros with `do { } while (0)` bodies | authentic: the SDK `OSMutex.c` queue macros; `do { } while (0)` occurs only in the macro definitions, not in a function body (R17 passes). |
| `BATConfig`, `__OSRestoreCodeExecOnMEM1` | `while (magic != BAT_CONFIG_MAGIC) { }` | not fake code: the compare against 0xBA2CF and the self-loop are in the binary. `__OSRestoreCodeExecOnMEM1` is named from the Gormiti link map and `BATConfig` agrees with it; the constant `BAT_CONFIG_MAGIC` and the `magic` parameter name are invented (open item above). |
| `decode_mcu_DC_refine`, `decode_mcu_AC_refine` (jdphuff) | `(JCOEF) p1` / `(JCOEF) m1` at three sites | not in IJG 6b's text (6b writes `\|= p1`, `+= p1`, `+= m1`), but required by the bytes: retail narrows p1/m1 with `extsh` before the OR/add, and removing the casts drops both functions to 98.9%/98.7%. A real arithmetic cast with an effect on the bytes, the plain spelling of a narrowing the shipped library made, not steering; commented at the code. |
| `fill_input_buffer` (jdatasrc) | direct call `RwStreamRead(src->infile, src->buffer, INPUT_BUF_SIZE)` instead of `JFREAD(...)` | explained by the binary (the game replaced `fread`); a direct call rather than a `JFREAD` macro onto an address name (AGENTS.md source rules). Otherwise IJG 6b verbatim. |
| `consume_markers`, `start_input_pass`, `latch_quant_tables` (jdinput) | IJG 6b text | verbatim IJG 6b, unchanged; the former register-numbering residue was a build setting (library-wide `-ipa file`, ruling "Walls rulings" below), not a source deviation. |
| `__copy_longs_rev_unaligned` (mem_funcs) | two `cps = ((unsigned char*)lps); cpd = ((unsigned char*)lpd);` resyncs | authentic. Verbatim the Wii branch (`#if !PLATFORM_GCN`) of zeldaret/ss `mem_funcs.c` (fetched at review, ss `main` d3d8e3c, and compared line by line); in ss they keep tab-aligned Metrowerks formatting that differs from the file's clang-formatted rest, the mark of copied original MSL text. With `cps`/`cpd`/`lps`/`lpd` being the MSL cast macros they compile to nothing but keep the entry copy of `src` live (docs/compiler_regalloc.md). Original lineage text, not the permuter's invented self-assignment; the unit is linked and counted. |
| `jpeg_idct_islow`, `pass2_fs_dither`, `format_message`, `jinit_upsampler`, `ycc_rgb_convert` | IJG 6b text | verbatim IJG 6b (including `register` locals and the `SHIFT_TEMPS` macro); `NO_ZERO_ROW_TEST` conditional dropped as it models no build here (R37). |
| `ProbeBarnacle` (EXIUart) | `err = FALSE;` before `err \|= !EXIImm(...)` | authentic: zeldaret/tp's text. Not a lever: ogws's `err = !EXIImm(...)` (no reset) gives the same bytes; only dropping the reset while keeping `\|=` changes them (98.9%). |
| `ProbeBarnacle` (EXIUart) | `EXIImm(chan, revision, sizeof(revision), EXI_READ, NULL)` | `sizeof` of a pointer (4), tp's text; ogws writes `sizeof(u32)`. Same value on this target, byte-neutral; readability open item. |
| `EXIDma` | `if ((u32)exi->tcCallback)` | tp's text (EXIImm has the plain test); dropping the cast is byte-neutral. Not fake (no effect on the bytes); readability open item. |
| `EXIImmEx` | `((u8*)buf) += xLen;` | tp's text, a CodeWarrior cast-as-lvalue extension; `buf = (u8*)buf + xLen` (ogws) is byte-identical. Readability open item. |
| `EXIImm`, `EXIDma`, `__EXIProbe`, `EXIInit`, `EXISync` | tp's text, with `REG(chan, idx)` for `__EXIRegs[chan * 5 + idx]` | authentic; the macro names the five-word channel stride. `EXISync`'s long condition is tp's (and ogws's) logic. |
| `SIInterruptHandler`, `SIGetType` | function-scope `static u32 cmdTypeAndStatus` (two objects) | authentic: retail has both `@LOCAL@SIInterruptHandler...@cmdTypeAndStatus` and `@LOCAL@SIGetType...@cmdTypeAndStatus` in `.sbss`. `SIGetType`'s two `BUSY` branches (`Type[chan] = ...` versus `type = Type[chan] = ...`) are tp's text verbatim. |
| `__SITransfer` | `SIComCSR` bitfield union written field by field | authentic SDK register form; the reserved fields carry `// pad:` notes. |
| `SITransfer`, `AlarmHandler` | tp's text; `chan = alarm - Alarm` | authentic. |
| `OSUTF8to32`, `OSUTF16to32`, `OSUTF32toANSI` | range tests written bound-first (`lo <= x && x <= hi`, `x < lo \|\| hi < x`) | authentic. tp writes `x >= lo && x <= hi` and `x < lo \|\| x > hi`; substituting tp's spelling at review drops the three functions to 98.2%, 88.9% and 84.5% (the compiler folds the pair into one subtract-and-compare), so retail was written with the bound first. Number-line order is a common way to type a range test; nothing is added, and operand order is the only change. tp's spelling is a reconstruction, not evidence against this. |
| `OSUTF32toSJIS` | `if (UNICODE_SUPPLEMENTARY_BASE <= utf32) return 0;` then the per-page table lookup | authentic: tp's text, constant-first as in tp. |
| `__OSInitIPCBuffer` | `IpcBufferLo = (void*)*(u32*)OSPhysicalToCached(OS_IPC_BUFFER_LO_ADDR);` | authentic: tp's text with the low-memory offsets 0x3130/0x3134 named. A read of the low-memory block, not offset arithmetic on an object (R4 does not apply). |
| `__OSShutdownToSBY` | tp's text without tp's unused `res` local | authentic, and cleaner than the reference: the ioctl result is never read, so the call is a statement (R17: no dummy local). `OSPanic(__FILE__, 348, ...)` keeps the retail line number, as in the `ASSERTREPORT` ruling. |
| `__OSVIDimReplyHandler` | `StmVdInUse = 0; return 0;` | authentic: tp's and ogws's body. |
| `OSSetResetCallback` | `NULL` installs `__OSDefaultResetCallback`; returns `NULL` when the previous callback was the default | authentic: the text of tp's `OSSetPowerCallback`, which sits beside it in retail with the same 236 bytes. ogws's shorter version (no default handling) does not fit those bytes. |
| `OSStateTM` statics | tentative `ResetCallback`, `PowerCallback` declared after the zero-initialised `int`s; buffers in the order Eh, Im, Vd | authentic layout, verified by the gate. Tentative statics are emitted in reverse declaration order, so `ResetCallback` is declared first of the two to land last (`PowerCallback` at .sbss+0x18, `ResetCallback` at +0x1C). With the other order objdiff still reported 100% while the DOL hash failed: objdiff does not check static layout (docs/toolchain.md). |

## Walls rulings (walls lane)

- **libjpeg `-ipa file` (library-wide).** jdinput `consume_markers` matches only when `latch_quant_tables` is
  expanded into `start_input_pass` before `start_input_pass` is expanded into `consume_markers`, which `-ipa file`
  does (callee expansion order, docs/compiler_regalloc.md). Tested at review as a `JPEGLib` cflag on all 26 units
  from a clean gate: `main.dol: OK`, every libjpeg unit's per-function scores unchanged (25 at 100%, the parked
  jmemmgr at 90.6% either way). Adopted library-wide (as the RVL SDK profile) rather than per file: a per-library
  build setting is more plausible than one file built differently. The source is IJG 6b unchanged.
- **Permuter-use record.** Per the user's ruling, compiler and linker tracing came first on every wall; the permuter
  was run only on traced register/schedule residues, and no permuter output is committed. `__copy_longs_rev_unaligned`:
  a compiler probe (`pmod_castself`, docs/compiler_regalloc.md) showed that a cast self-assignment keeps the entry
  copy; the resync lines were then found verbatim in the zeldaret/ss Wii branch, and the committed text is the
  reference's (ruling above). `alloc_small` (jmemmgr): `(long)` cast state rejected
  (not IJG). `Block_link`: closed by the ss macro forms, no permuter text. Caveat for future runs: decomp-permuter's
  `extract_fn` prototypes static callees that MWCC would inline, so check that its base score equals
  `build/tools/cc.exe` before trusting a run.
- **mem_funcs and jdinput unparked.** Both units are `Matching` (4/4 and 7/7, clean gate green).

## GX rulings (sdk-gx)

The RVL SDK GX library, 0x802377A0-0x8023F0D4, 14 units (references zeldaret/tp `libs/revolution/src/gx/`, with
doldecomp/ogws `src/revolution/GX/` second). Built with `cflags_gx` (SDK profile + `-fp_contract off`, as tp and ogws
build the whole SDK; no fused multiply-adds in the GX float code). Thirteen units are `Matching`; `GXTransform` is
parked (below).

- **`GX_XF_LOAD_REGS(size, addr)` (XF block-load header).** ogws's macro (`include/revolution/GX/GXHardware.h`, also in
  xenoblade): `cmd = 0; cmd |= addr; cmd |= size << 16;` then the opcode byte and the word. Retail computes the slot
  address after the opcode byte store, which the single `addr | count << 16` expression of tp does not give
  (`GXLoadNrmMtxImm` 81% and `GXLoadLightObjImm` 97%, the latter the r4/r5 swap, both exact with the macro).
  Defined in `revolution/gx/__gx.h` and used by every immediate matrix or light load (`GXLoadPosMtxImm`,
  `GXLoadNrmMtxImm`, `GXLoadNrmMtxImm3x3`, `GXLoadTexMtxImm`, `GXLoadLightObjImm`); `GXLight` is `Matching` with it.
- **Const object parameters (2009 SDK).** `GXSetCPUFifo`, `GXSetGPFifo` (`const GXFifoObj*`), `GXLoadTexObjPreLoaded`
  (`const GXTexObj*`, `const GXTexRegion*`), `GXLoadTlut` (`const GXTlutObj*`) and `GXInvalidateTexRegion`
  (`const GXTexRegion*`) take const pointers, as in mscharged-decomp's R4QK01 (2009) build and Petari; tp, ogws and
  xenoblade have the older non-const signatures. The bytes prove it: MWCC gives a pointer-to-const (or restrict)
  parameter its own alias object, so its loads move past stores to globals, the FIFO port and other pointers
  (docs/compiler_regalloc.md, "Scheduler memory disambiguation"). With the non-const signatures the five functions
  stopped at 41-89%; with const they are exact. `GXLoadTexObjPreLoaded` and `GXLoadTlut` still write through the
  cast-away pointer, as the references do. `GXInvalidateTexRegion` has no reference copy; const is taken from the
  same family. `GXLoadTexObj` and `GXTexRegionCallback` keep the non-const types (mscharged has const there too;
  untested, no byte depends on it in this unit).
- **Missing macros.** `ASSERTMSGLINE` and `OSUncachedToPhysical` were undefined, so their uses compiled as implicit
  calls; both are now defined in `revolution/gx/__gx.h` (the assert is `((void)0)` in release, as the SDK's).
- **R18, the SDK's inline-asm helpers** (listed in "Listed inline-asm helpers" above). `WriteProjPS`, `Copy6Floats`,
  `WriteMTXPS4x3`, `WriteMTXPS3x3from3x4`, `WriteMTXPS3x3`, `WriteMTXPS4x2` (GXTransform), `PushLight` (GXLight) and
  `IsWriteGatherBufferEmpty` (GXInit) are in tp under those names with the same instructions (checked at review against
  tp main). `LoadProjPS` is ogws's alone and ogws calls its inline fake: listed as PROVISIONAL (the paired-single copy
  is in retail; its helper shape is not proven). All counted once their units link.
- **`asm volatile`: confirmed as an accepted spelling.** tp writes the helpers with plain `asm { }`; ogws writes every
  one as `ASM_VOLATILE (...)`, which its `decomp/compat.h` expands to `asm volatile` for CodeWarrior, and closes six
  GXTransform functions with it. The qualifier is a CodeWarrior language feature that keeps the block's instructions
  in the order written; it is a property of the asm statement, not a `volatile` object or a carrier (R17 covers
  variables and casts). Retail keeps the helpers' loads-then-stores order, which plain `asm` lets the scheduler
  interleave. Whether the original wrote `asm volatile` or got the same order another way (a scheduling pragma)
  cannot be told from the bytes; `asm volatile` is the minimal spelling that gives it and is accepted. The rules
  checker reads `asm volatile` as an asm qualifier (lane change, with a test).
- **`volatile void* wgpipe = &GXWGFifo;` in `__GXSetProjection`: not accepted as authentic; MATCH-PENDING when
  GXTransform links.** tp passes the port straight to the helper (`WriteProjPS(__GXData->projMtx, (volatile
  void*)GXFIFO_ADDR)`) and has no local; ogws has the local with the comment "Temp required to match", its own
  admission that the temporary exists for the bytes. A local that only holds a constant address so that the
  allocator treats it differently is the Section F temporary pattern (R17). `__GXSetProjection` is 100% in the parked
  unit; it does not count until a form without the temporary (or evidence for it) is found.
- **`math.h` `cosf`.** Retail `GXInitLightSpot` calls `cos` (0x801A5924) and rounds with `frsp`, which is what tp's and
  ogws's `cosf(r)` compile to through MSL's inline wrapper. ogws's MSL `math_double.h` has `inline float cosf(float x)
  { return cos(x); }` (no cast) with `double cos(double x);` in `fdlibm_public.h`. Adopted in that form (`inline`, not
  the lane's `static inline`; byte-identical, no out-of-line `cosf` emitted) in the minimal project `include/math.h`,
  which MSL owns; extend it from MSL's real header when MSL math is reconstructed.
- **`cos` at 0x801A5924 (MSL math range): verified.** The body is fdlibm `s_cos.c`: `|hi| <= 0x3FE921FB` calls
  `__kernel_cos(x, 0.0)`; `>= 0x7FF00000` returns `x - x`; otherwise `__ieee754_rem_pio2`
  and a switch on `n & 3`: `__kernel_cos(y0, y1)`, `-__kernel_sin(y0, y1, 1)`, `-__kernel_cos`,
  `__kernel_sin(y0, y1, 1)`. `sin` calls `__kernel_sin` first. The three callees are named and matched with the MSL math units ("MSL rulings (msl-3)").
- **Render-mode objects: verified.** The seven retail `GXRenderModeObj`s (0x80440B78-0x80440CE0, 0x3C each) decode to
  the SDK tables they are named after: `GXNtsc480IntDf` (TV mode 0, 640x480, xfb 480, origin 40/0, double field,
  vfilter 8,8,10,12,10,8,8), `GXNtsc480Prog` (mode 2, single field, vfilter 0,0,21,22,21,0,0), `GXMpal480IntDf`
  (mode 8), `GXMpal480Prog` (mode 10), `GXPal528IntDf` (mode 4, 640x528, y origin 23), `GXEurgb60Hz480IntDf` (mode 20),
  `GXEurgb60Hz480Prog` (mode 22; its 0x40 size includes 4 alignment bytes). `__GXInitGX` selects four of them by
  `VIGetTvFormat`, as in tp. The other SDK modes defined in `GXFrameBuf.c` are dead-stripped.
- **Line arguments of no-op asserts (convention).** `SET_REG_FIELD`, the generated `SC_*_SET_*` register macros,
  `ASSERTMSGLINE` and `CHECK_*` take a line number that the release build discards (`((void)0)`), so every spelling is
  byte-neutral. Convention: write the reference's literal line number (the debug SDK's `__LINE__` value, which tp
  carries, e.g. `SET_REG_FIELD(2147, reg, 24, 0, phyAddr >> 5)` in `GXCopyDisp`), or `0` where
  the reference has `0`; never `__LINE__`, which would record this file's layout instead of the original's (same
  reasoning as the `ASSERTREPORT` ruling). The remaining `__LINE__` uses (GXFrameBuf 104, GXTexture 86, GXPixel 48,
  GXTransform 17, GXPerf 10, GXLight 3, GXFifo 2, GXInit 1, from an older tp text) are a free-tier open item, not a
  blocker; GXBump, GXTev, GXAttr and GXGeometry already follow the convention.
- **Reader-test sample (counted).** `GXInit`, `__GXInitGX`, `GXSetVtxDesc`, `GXSetVtxAttrFmt`, `__GXSetDirtyState`,
  `GXSetCullMode`, `GXSetTevColorIn`, `GXSetBlendMode`, `GXCopyDisp`, `GXPixModeSync` and `GXCallDisplayList` read as
  tp's text with named registers and macros; no dummy locals, steering or carriers. Notes: the `= NULL` register-block
  pointers (`__piReg`..`__memReg`, `volatile` with evidence notes) and the function-local statics (`shutdownFuncRegistered`;
  `peCount`, `time`, `calledOnce` in `__GXShutdown`) are tp's declarations and give retail's `.sbss` order (gate green).
  `GXInit`'s inner `{ u32 reg = ...; }` block shadowing the outer `reg` is tp's text. `GXCallDisplayList`'s
  `*(u32*)&__GXData->vNumNot == 0` tests the adjacent `vNumNot`/`bpSentNot` halfwords in one load; it is tp's text
  with tp's comment, a pun on a named member rather than offset arithmetic (R4 does not apply; same standing as the
  `OSSetErrorHandler` ruling). `GXPixModeSync` (0x8023A760) resends `peCtrl`, which is tp's `GXPixModeSync`; the
  former `GXTexModeSync` name belongs to the BP 0x63 write, kept dead-stripped. The `static inline` texgen,
  channel and ambient helpers in GXGeometry use the `inline` keyword, an original spelling, not a carrier. At review,
  the address trailers written after closing braces (`} // 0x...`, 71 functions in GXBump, GXDisplayList, GXFrameBuf,
  GXGeometry, GXLight, GXPixel, GXTev) were moved to the end of the header comments (byte-neutral, gate green).

## DVD rulings (sdk-io-3)

**dvderror.c (ruling: Matching, 14 functions counted).** Reference zeldaret/tp `dvderror.c`. Split `.text`
0x80224990-0x802253A8 (dtk leaves the 8-byte `pad_03_802253A8_text` before the next 16-byte-aligned function),
`.data`, `.bss` and `.sbss`. Renames by order, size and use against tp: the twelve `cbForNand*`/`cbFor*Register`
callbacks, `NandInfo`, `NandCb` (size corrected from 0xD4 to 0xBC, `sizeof(NANDCommandBlock)`; the rest was
alignment before `__ErrorInfo`), `ExistFlag`, `NextOffset`, the new `__FirstErrorInfo` (0x80 bytes at 0x80498300,
the header record), and the callees `DVDLowGetStatusRegister`, `DVDLowGetControlRegister`,
`DVDLowPrepareControlRegister` (dvdlow) and `NANDPrivateCreateAsync`, `NANDPrivateCreateDirAsync` (nand). Each callee
is the one tp calls at that site with the same arguments (the two NAND entry points take the path, `NAND_PERM_RWALL`
and attribute 0; the two register getters are 12-byte loads). Three NAND prototypes added to `nand.h` with tp's
signatures. `DVDErrorInfo.unk_0x14` is now `control`, an invented name marked at the field: it holds the DVD control
register that `cbForPrepareControlRegister` stores. Reader test (counted): `cbForNandOpen`, `cbForNandWrite`,
`cbForPrepareStatusRegister`, `__DVDStoreErrorCode` and the rest of the file are tp's text with named sizes
(`sizeof(DVDErrorInfo)`), no dummy locals and no steering. Open ([free]): the literal access mode 3 in
`NANDPrivateOpenAsync` and the callback results 1/2 (R11); `__FirstErrorInfo` has no header declaration (only
dvderror.c names it).

**`static volatile BOOL ResetRequired` in dvd.c (ruling: authentic, accepted with its evidence note).** tp declares it
plain (`static BOOL ResetRequired;`), but tp's dvd.c is `NonMatching`, so its declaration carries no byte evidence.
ogws declares it plain too (`= FALSE`) and its dvd.c matches, but it is built with GC/3.0a5.2 against an older SDK, so
its scheduling says nothing about this build. Here plain `BOOL` gives `cbForStateReset` 96.5% (the `li r0, 0` for
the stores is scheduled before `stw LastResetEnd+4` instead of after it) and `cbForStateBusy` 99.97% (the store to
the non-volatile `executing` moves above the `ResetRequired` store). Retail keeps the `ResetRequired` store in program
order among the volatile `LastResetEnd`/`ResumeFromHere` stores in both functions, which is what volatile does; the
other accesses (`cbForStoreErrorCode2`, `cbForStateGoToRetry`, the second store in `cbForStateBusy`,
`DVDResetRequired`) match either way, so the qualifier distorts nothing. The flag is set from interrupt callbacks and
polled through `DVDResetRequired`, the same use as the SDK's volatile `ResumeFromHere`, `Canceling` and `PausingFlag`
beside it. A qualifier on the file-scope declaration with a codegen reason and a plausible SDK-revision change (this
SDK is from 2009, tp's from 2006), not a carrier local. dvd.c is Matching, so the declaration is
linked.

**dvd.c R17 cleanup (ruling: two authentic, one corrected at review).**
- `DVDInit`: `if (bootInfo->magic != 0xE5207C22 && bootInfo->magic != 0x0D15EA5E) FirstTimeInBootrom = TRUE;`
  replaces tp's empty `if`/`else if` chain. It is ogws's text (matching there, with `OS_BOOT_MAGIC_JTAG` and
  `OS_BOOT_MAGIC_BOOTROM`) and byte-identical here: authentic.
- The unread locals `old` (`__DVDSetOptionalCommandChecker`) and `idle` (`__DVDAudioBufferConfig`) were removed:
  byte-neutral, accepted.
- `DVDReset`: the lane wrote `status = __DIRegs[1]; __DIRegs[1] = status;`, an invented temporary. The SDK form is
  tp's `__DIRegs[1] = __DIRegs[1];` (the status bits are written back to acknowledge them), restored at review.
  `DVDReset` has no retail copy (not scored). The checker's R17 self-assignment test now passes `X = X;` when `X` is
  based on a name declared volatile with a `// volatile:` note (a hardware register really is read and written;
  `__DIRegs` has its note in `hardware.h`), with a regression test; a plain or unnoted self-assignment still fires.
- `stateBusy`: `DVDLowAudioBufferConfig(u32 enable, ...)` (was `u8`). `StampIntType` is `inline` (retail has no
  out-of-line copy).

**`-pool off` (ruling: rejected for the RVL SDK, per object and library-wide).** A MetroTRK worker (lane
msl-trk-6-G; merged for MetroTRK only, "MetroTRK rulings (msl-trk-6)") found that under MWCC's default `-pool on` the uninitialised `.bss` statics of a unit are
pooled and laid out in order of first use in the code, while `-pool off` lays them out in reverse declaration order.
Tested on OSPlayRecord with tp's `static u32 RecordCheckSum(OSPlayRecord* record)` restored and `-pool off` on the
object: `PlayRecordCallback` 83.3%, `__OSStopPlayRecord` 83.0%. Retail loads `PlayRecord`'s address once and reaches
`PlayRecordAlarm` as `addi r3, r29, 0x80`, which is pooled addressing: OSPlayRecord was built with pooling on, its
`.bss` order is a first-use order, and its MATCH-PENDING entry stays. Library-wide (`RVLSDKLib` cflags plus
`-pool off`): 31 functions in 14 SDK units lose their match (dvd, dvderror, GXInit, GXTev, OS, OSCache, OSContext,
OSError, OSNet, OSPlayRecord, OSReset, OSThread, SIBios, SISamplingRate), none gains, and the DOL hash fails.
Neither zeldaret/tp nor doldecomp/ogws passes `-pool` for the SDK. The SDK keeps the default.

## DVD rulings (sdk-dvd-broadway)

**dvd_broadway.c (ruling requested: dead-stripped bodies, 32 functions exact).** Unit name from xbret/xenoblade,
PrimeDecomp/sdk, ogws and mscharged (all `dvd_broadway.c`). Live functions follow xbret/xenoblade (same SDK build,
Matching there); the pointer tests are written against `0` as PrimeDecomp/sdk writes them (`== NULL` with this
project's C `NULL`, `((void*)0)`, gives `li r0, 0; cmplw` where retail has `cmpwi`), the alignment tests of
`DVDLowOpenPartition`, `...WithTmdAndTicketView` and `DVDLowGetNoDiscBufferSizes` as `((u32)p & 0x1F)` and the
allocation tests as `(diCommand = ddrAllocAligned32(...)) == 0`, all PrimeDecomp/sdk's spellings. The DI command and
register blocks use `dvd.h`'s `diCommand_t`/`diRegVals_t` (mscharged and PrimeDecomp/sdk use the same types).
- Dead-stripped functions. Retail `.data` keeps the strings of 19 functions the link dropped, in source order
  (`doCoverCallback`, `DVDLowOpenPartitionWithTmdAndTicket`, `DVDLowNoDiscOpenPartition`, `DVDLowWaitForCoverClose`,
  `DVDLowNotifyReset`, `DVDLowGetCoverStatus`, the five `DVDLowReadDvd*`, `DVDLowOffset`, `DVDLowStopLaser`,
  `DVDLowReadDiskBca`, `DVDLowSerMeasControl`, `DVDLowRequestDiscStatus`, `DVDLowRequestRetryNumber`,
  `DVDLowGetCoverReg`, `DVDLowEnableDvdVideo`). xbret places the strings with
  `DECOMP_FORCEACTIVE` and empty stubs; here each has a real body marked `// no copy in retail (dead-stripped)`:
  xbret's where it has one, otherwise PrimeDecomp/sdk's (also mscharged's). The built `.data` is byte-identical to
  retail (0xF06 bytes plus the section's alignment). PrimeDecomp/sdk's `DVDLowReadDvdPhysical` size check
  (`"Size of DVDVideoPhysical is not a multiple of 32!"`, a constant-false test) is left out: retail has no such
  string. The five functions xbret stubs that own no string and that no reference gives a body (`DVDLowBreak`,
  `DVDLowClearCallback`, `DVDLowGetLength`, `DVDLowUnmaskCoverInterrupt`, `DVDLowGetLastEticketError`) are not
  written: nothing in retail evidences their text.
- `initDvdContexts` is external, as in PrimeDecomp/sdk: its out-of-line copy (dropped by the link) is the first user
  of `dvdContexts`, which is what puts `dvdContexts` before `diRegValCache` and `registerBuf` in `.bss` (with it
  `static`, the order is the first-use order of `doPrepareCoverRegisterCallback`). Inlined into `DVDLowInit`.
- `static volatile u8 requestInProgress` carries a `// volatile:` note: every reference declares it volatile; the
  bytes are the same either way.
- `symbols.txt`: `fn_80225960` is `doPrepareCoverRegisterCallback` (it copies `registerBuf` into
  `diRegValCache.CoverRegVal` and is passed by `DVDLowPrepareCoverRegister`; `doCoverCallback` has no retail copy);
  dtk's 0x2E/0xEA8 `.data` blocks `lbl_8043AB4A`/`lbl_8043AB78` are split into the 69 string literals they hold, so
  `gatecheck layout` can pair them with the compiled literals.
- `dvd.h` additions: `DVD_INTTYPE_BR`, `DVDLowStopMotor` (called by dvd.c, previously undeclared) and the prototypes
  and buffer types (`DVDVideoPhysical`, `DVDVideoDiscKey`, `DVDDiskBca`, `DVDLowDriveSer`, PrimeDecomp/sdk layouts) of
  the dead-stripped API. The extra declarations renumber MWCC's `@NNN` literal labels in units that include `dvd.h`
  (`neutral` reports relocation-name changes, no byte change; those units stay exact).

**dvdidutils.c, dvdFatal.c, dvdDeviceError.c (sdk-dvd).** Unit names from xbret/xenoblade, ogws and mscharged.
Bodies from xbret/xenoblade (dvdidutils also zeldaret/tp); all seven retail functions exact.
- `DVDGenerateDiskID` keeps tp's body, marked `// no copy in retail (dead-stripped)`; tp's debug-only `strnlen`
  helper is left out with the asserts that use it.
- dvdDeviceError: `__DVDShowDeviceErrorMessage` is `static` and inlined into `__DVDCheckDevice` (retail has no copy;
  the message table is referenced from `__DVDCheckDevice`). The two discarded `DVDLowGetImmBufferReg()` calls are
  retail's: xbret stores them in locals nothing reads, which the checker rejects as R17 dummy locals, so the calls
  stand alone. xbret's dead `break`s after `goto` are dropped. `lowDone`/`lowIntType` are `volatile` with a
  `// volatile:` note: the interrupt callback sets them while `__DVDCheckDevice` spins.
- Invented names (R7): `__OSDiscLayers` (low-memory byte 0x8000319C, in `dvd/__dvd.h`), dvdDeviceError's
  `DVD_DEVERR_*` and `DVD_REPORTKEY_RPC_STATE`. `SC_PRODUCT_GAME_REGION_*` in `sc.h` are koopthekoopa/wii-ipl's.
- `symbols.txt`: `lbl_803DD580`, `lbl_803DD59C`, `lbl_80528A88` are `__DVDErrorMessageDefault`,
  `__DVDErrorMessageEurope`, `__DVDErrorMessageChinaKorea` (xbret's names); dvd.c's `.sdata` split ends at 0x80528A88
  (the third table belongs to dvdFatal). The Chinese/Korean table has two entries, so a language from German to Dutch
  indexes past its end (SDK quirk, kept, R36).

**dvd.c `stateBusy` (sdk-dvd).** The `StampCommand` switch had cases 7 to 12 that retail lacks (its jump table sends
them to `default`); objdiff scored it 100% because it does not compare jump-table contents. The case set is now
retail's (xbret's eleven commands).

## MetroTRK rulings (msl-trk-6)

**Units (ruling: counted).** Linked from source and counted: `msghndlr` (13), `dispatch` (1), `mslsupp` (7),
`MWCriticalSection_gc` (3), `EXI2_GDEV_GCN/main` (10, unparked) and `target_options` (2); `flush_cache` (1) and
`mpc_7xx_603e` (2) are ORIGINAL-ASM. Parked: `targimpl` (31/33), MSL `ansi_fp` (8/10), NdevExi2AD `DebuggerDriver`
(20/21) and `exi2` (9/10). All linked units pass the checker; the hand review found no fakematch. Clean gate
`main.dol: OK` with every Matching unit at 100%.

**`-pool off` in `cflags_trk` (ruling: accepted for MetroTRK as a library build setting; unconfirmed as the original
option).** `EXI2_GDEV_GCN/main` declares `gRecvCB` (0x1C bytes) before `gRecvBuf` (0x500 bytes); retail puts
`gRecvBuf` at 0x8047A230 and `gRecvCB` at 0x8047A730, reverse declaration order, which is what `-pool off` gives for
large uninitialised `.bss` statics (under the default `-pool on` they come out in first-use order, `gRecvCB` first).
Neutrality, measured at merge on all 21 MetroTRK units: objdiff scores are identical with and without the flag;
with the flag and gdev Matching the gate is green; without it and gdev Matching the gate fails; without it and gdev
parked the gate is green. So no other TRK unit's bytes or layout depend on it, and gdev's layout needs it. No
MetroTRK unit addresses `.bss` through a pooled base register (the pooled-addressing evidence that refuted
`-pool off` for the SDK, "DVD rulings (sdk-io-3)", has no counterpart here). It is a real MWCC option, and a
per-library setting is how a separately built library (MetroTRK ships as its own archive) would differ; whether the
original makefile spelled it `-pool off`, or reached the same layout another way, is unproven. Alternative
rejected: declaring `gRecvBuf` before `gRecvCB` under the default would also link, but it reverses tp's order to
steer layout (a Section F lever) in one unit, where a library flag that changes nothing else is the plainer
hypothesis. MetroTRK only; the SDK keeps the default. The parked-list item about `DBInitComm`'s `int*` callback is
closed: `AmcExi2Comm.h` now takes the DB layer from `revolution/NdevExi2AD/DebuggerDriver.h`, and
`gdev_cc_initialize` passes its `EXICallback` uncast (the same type as `DBMtrCallback`).

**Gekko `TRKSaveExtended1Block`/`TRKRestoreExtended1Block` as ORIGINAL-ASM (ruling: accepted, listed apart).** The
reference (tp `mpc_7xx_603e.c`) has both as `asm` functions with `nofralloc`; C cannot name the SRs, BATs and SPRs
they move. tp's text is the GameCube 7xx variant; this Wii build's bodies move GQR0-7, HID2, WPAR, DMA_U/DMA_L and
the performance counters instead, and no public Wii text exists, so the lane wrote the instructions from the retail
bytes inside tp's structure (same order of blocks, the `gTRKCPUState`/`gTRKRestoreFlags` layout through
`targimpl.h`'s typed declarations). That is the nature of any ORIGINAL-ASM body: assembly matches by construction,
which is why it is counted in neither number. The ruling is that the functions were assembly in the original (the
reference says so and the bodies have no C shape), not that the transcribed text is Nintendo's. `TRK_flush_cache` is
tp's `asm` text unchanged.

**ORIGINAL-ASM table.** E's three functions (864 bytes) are listed and linked; D's eight `targimpl` functions (928
bytes) are listed in the parked table under "ORIGINAL-ASM" and counted nowhere until `targimpl` links.

**MSL `ansi_fp.c` per-object string flags (ruling: provisional; parked unit).** The MSL profile keeps small
literals unpooled in `.sdata2` (`-str reuse` plus the `readonly_strings` pragma, "MSL string flags"). ansi_fp's
`__two_exp` reads its long digit strings from one pooled `.rodata` block (0x803D9310-0x803D93F0, one base plus
offsets), which only `-str reuse,pool,readonly` gives, so the object carries that flag (`extra_cflags`). Two MSL
units with different pooling mean the library was not built with one string setting, or that ansi_fp.c selects
pooling itself (a pooling pragma in the file would be the authentic form if a reference showed one). The flag is
the smallest change that matches; provisional, to revisit when ansi_fp links.

**NdevExi2AD `-O0` (ruling: accepted for the library; provisional until a unit links).** Every function of the range
spills its parameters to the frame on entry, keeps block locals in a frame-pointer frame and calls the ODEMU and
mailbox helpers out of line: unoptimised code, which no `-O1`..`-O4` setting reproduces. doldecomp/ogws builds the
library the same way. Both units are parked (`DBWrite` 99.0%, `__EXI2Imm` 94.0%, register numbering), so the flag
has no gate effect yet. The library sits at 0x801900E8-0x80190BEC; the four `twui` stubs `TRKAccessFile`..
`TRKPositionFile` (0x80190BEC-0x80190C0C) were taken out of the exi2 split at merge: they are MetroTRK's `targsupp`
(tp writes them as `asm` stubs) and stay dtk asm until that unit is reconstructed (open item). The four static
mailbox/RAM helpers moved from `DebuggerDriver.h` (ogws keeps them in the header) into `DebuggerDriver.c`, their only
user, so that gdev can include the header (DebuggerDriver unchanged at 20/21).

**Header unification at merge.** `TRK/dsmsg.h` holds `CommandRequest`, `CommandReply` (moved from `support.h`) and
`DSFileHandle`. `targimpl.h` is the typed header (processor state, `MemoryAccessOptions`, `ValidMemoryOptions`) and
took msghndlr's prototypes: `TRKTargetAccessMemory` takes `MemoryAccessOptions` (msghndlr passes
`MEMACCESS_UserMemory`/`MEMACCESS_DebuggerMemory`), and `TRKTargetStop` returns `DSError` as in tp (byte-neutral).
`mpc_7xx_603e.h` takes `gTRKCPUState`/`gTRKRestoreFlags` from `targimpl.h` instead of byte-array externs and alone
declares the save/restore pair. `target_options.h`, `dolphin_trk.h` (`__TRK_reset`, `__TRK_copy_vectors`,
`TRKTargetTranslate`) and `flush_cache.h` are single headers merged from the lanes' copies. `include/math.h` is one
MSL header: main's `sqrtf`/`sinf`/`cos` and `inline float cosf`, plus ansi_fp's `copysign`, `frexp`, `ldexp`, `pow`,
`HUGE_VALF` and the classification macros. `nubevent.h` now has tp's order (`NUBEVENT_Breakpoint = 3`,
`NUBEVENT_Exception = 4`; msl-trk-4 had them swapped, invisible to mainloop's shared case).

**Names (ruling: accepted with open R7 items).**
- tp names (`TRK_MINNOW_DOLPHIN`, by order, size and call graph): the twelve `TRKDo*` handlers, `GetTRKConnected`,
  `IsTRKConnected`, `TRKDoNotifyStopped`, `__TRK_reset`, `__TRK_copy_vectors`, `TRKTargetTranslate`, the targimpl
  functions and data (`gTRKMemMap`, `gTRKStepStatus`, `TRKvalue128_temp`), the mslsupp hooks `__read_console`,
  `__TRK_write_console`, `__close_console`, `__read_file`, `__write_file`, `__access_file`, the four `TRK*File`
  stubs, `GetUseSerialIO`/`SetUseSerialIO`/`useSerialIO`.
- ogws names (a reconstruction, by order and call graph): the NdevExi2AD functions and statics (`__DBMtrHandler`,
  `__DBIntrHandler`, `__DBCheckMailBox`, `__DBReadUSB_CSR`, `__DBWaitForSendMail`, the `ODEMU*` helpers,
  `__DBReadMailbox`, `__DBRead`, `__DBWriteMailbox`, `__DBWrite`, `__EXI2Select`/`Deselect`/`Sync`/`Imm`,
  `__DBEXIInit`, `__DBEXIReadReg`/`WriteReg`/`ReadRam`/`WriteRam`, `__EXISwap32`, `__DBMtrCallback`,
  `__DBDbgCallback`, `__DBEXIInputFlag`, `__DBRecvMail`, `__DBRecvDataSize`).
- MSL names by behaviour: `copysign`, `frexp`, `ldexp`, `pow`, `__float_nan`, `__float_huge`, `__double_huge`, and
  the ansi_fp functions (tp's `ansi_fp.c`).
- Invented (open R7): `gTRKReplySerial` (the reply serial at 0x8052A4C8; tp's GameCube nub has no such counter) and
  `CommandReply.arg.serial`; `CommandRequest` and its members (`option`, the `arg`/`range` unions, `rangeEnd`);
  `TRKStopInfo` and its members (targimpl.c); `DSFileHandle` (`DS_Stdin`/`DS_Stdout`/`DS_Stderr`); `__close_file`
  (inferred: the close hook beside `__read_file`/`__write_file`, no evidence string); the macros
  `TRK_REPLY_HEAD_SIZE`, `PPC_EXCEPTION_ID_MASK`, `HID2_PSE_LSQE`, `CACHED_BASE` and ansi_fp's `DBL_*` masks.

**Reader-test sample (counted).** `TRKDoReadMemory`, `TRKDispatchMessage`, `__access_file`, `MWEnterCriticalSection`,
`gdev_cc_initialize` (linked) and `TRKTargetAccessMemory`, `TRKTargetAddStopInfo`, `__DBEXIInit` (parked) were read
against the reader test and Section F: tp's or ogws's text with named enumerators and typed request/reply heads, no
dummy locals, `volatile` carriers or steering.
- `TRKDoReadMemory` tests `result == DS_NoError` right after setting it; that and the 32-byte-aligned `buf` are tp's
  text (the original's dead test, kept). The request is read through `(CommandRequest*)buffer->data`, a typed view
  of the message buffer at offset 0 (R4 passes, as for `CommandReply` in msl-trk-4).
- `__access_file` and `__close_file` switch on `(u8)result`: the IO result is a byte in a 32-bit return (tp's text).
- `TRKTargetAccessMemory` saves and restores `gTRKExceptionStatus` by value around the access: tp's text.
- `TRKTargetAddStopInfo` sizes the notification with the literals `0x4E8`/`0x40` and reads the OS low-memory words
  through `ConvertAddress(0xD4..0xE4)`: open R11 items (unit parked).
- `__DBEXIInit` polls `EXI2_CR & EXI_CR_TSTART` in an empty `while`: ogws's text, a hardware wait.
- Open ([free]): targimpl's header carries
  score notes (R21); the NdevExi2AD units' register and mail constants are unnamed (R11).

## MetroTRK rulings (match-trk)

**Units.** The last MetroTRK range in dtk asm is split in tp's layout: `Processor/ppc/Export/targsupp` (4, all
ORIGINAL-ASM), `Os/dolphin/dolphin_trk` (7: 5 counted, `InitMetroTRK`/`InitMetroTRK_BBA` ORIGINAL-ASM),
`Os/dolphin/dolphin_trk_glue` (13: 12 counted, `TRKLoadContext` ORIGINAL-ASM) and `Portable/notify` (1). The retail
bodies are xenoblade's Wii text (MEM2 translation, `lc_base`, `__TRK_reset`, NDEV-only comm table), which both
files name in a `// Reference:` line beside tp's.

**`TRKOpenFile` and `TRKPositionFile` under `force_active`.** Nothing in retail calls either stub (`mslsupp` calls
only `TRKAccessFile` and `TRKCloseFile`), yet both are in the DOL: the original `targsupp.s` was an assembly object the
linker kept whole. Built from C, MWLD strips the two, so `config.yml` lists them under `force_active`.

**mem_TRK is xenoblade's word-access version; 0x80191398 is `TRK_memset` (ruling: renamed).** Retail moves each
byte through the aligned word that holds it, which is xenoblade's `ppc_readbyte1`/`ppc_writebyte1` text (tp's
byte and word stores are another version). The function at 0x80191398, called as `TRK_fill_mem` until now, keeps
`dst` in a second register for the whole body although nothing reads it: it is xenoblade's `TRK_memset`, with
`TRK_fill_mem` inlined into it and the out-of-line `TRK_fill_mem` stripped (no copy in retail). Every caller
(msgbuf, msghndlr, support, targimpl) calls `TRK_memset`, as in xenoblade; the call sites compile the same. The
size parameters are `int`, as xenoblade's: the `for (i = 0; i != size; i++)` loops test the count with `cmpwi`.

**`TRK_ppc_memcpy` (ruling: accepted, listed helper).** The function is xenoblade's: the byte goes through
`ppc_readbyte1`/`ppc_writebyte1`, now shared by mem_TRK and targimpl in `TRK/ppc_mem.h` (xenoblade's header), and
each access is followed by `asm { sync }`. With `__sync()` the compiler shares the lane shift that retail
computes twice; the earlier `(int)` cast that steered this is gone.

**`InitMetroTRK` is 0x98 bytes (ruling: symbols.txt corrected).** dtk sized it 0x94 and left its last word, a
`blr` after `b TRK_main`, as a gap. Both references end the function with that unreachable `blr`, as
`InitMetroTRK_BBA` (0x94) does in retail, so the word is the function's own.

**`DBAT3U & 3` in `TRKTargetTranslate` and `__TRK_copy_vectors` (ruling: accepted).** Retail tests the
`Extended1` word at 0x238, which `TRKSaveExtended1Block` fills from `mfdbatu 3`: DBAT3U in the layout targimpl and
mpc_7xx_603e already use. xenoblade spells it `DBAT2L` in a struct with an extra `HID2` word before the IBATs; that
layout does not fit our save routine's offsets nor targimpl's, so the field keeps the name the saved register
gives it. It also reads right: the OS maps the locked cache with DBAT3 (`__LCEnable`), and `& 3` is its Vs/Vp
valid bits.

**`0x1C000000 > addr` (ruling: original spelling).** The MEM2 test is written as xenoblade writes it; the
`addr < 0x1C000000` order folds the two compares into a `subis`/`cmplw` range check that retail lacks.

**`TRK_copy_vector` (static, inlined).** Retail inlines it (with `TRKTargetTranslate`) into
`__TRK_copy_vectors`; the compiler's out-of-line copy is unreferenced and the linker drops it, so it has no
address (an EXTRA symbol in the object, nothing in the DOL).

**Names and headers.** `fn_80191B54` is `EnableEXI2Interrupts` (tp and xenoblade). `EnableMetroTRKInterrupts`
moves from `revolution/os/OS.h` (its pending note) to `TRK/dolphin_trk.h`, which `OS.c` includes. The glue's
declarations move from `TRK/trk_glue.h` to the new `TRK/dolphin_trk_glue.h` (tp's header), which `trk_glue.h`
includes. `DBCommTable.initialize_func` spells its callback parameter as `void (*)(s32, OSContext*)` (the
`EXICallback` signature) rather than including `revolution/exi.h`: that header brings the SDK's variadic
`OSReport`, which clashes with the unprototyped declaration the portable units need (msl-trk-5).
`gDBCommTable` and `TRK_Use_BBA` are globals (tp's storage, `scope:global`); xenoblade's unused statics
(`gReadBuf`, `gWriteBuf`, the read and write counters) have no retail storage and are not declared.

## Game code rulings (game-bootstrap)

**Unit (ruling: counted).** `game/PadRumble` (9 functions and `__sinit_\PadRumble_cpp`, 1244 bytes) is linked from
source and counted; the checker finds no [match] finding, and the hand review found no fakematch. Clean gate
`main.dol: OK` with every Matching unit at 100%. The first game unit; GameLib profile in docs/toolchain.md.

**Reader test (ruling: passes).** `unk_Ramp::fn_80006860` clamps once, `this->target = MIN( 1.0f, MAX( 0.0f, target ) )`,
and then reads the stored member for the rate and the immediate case; that is how the clamp would be written once and
reused, not a steering device. The else branch (`value = this->target; rate = 0.0f;`) stores in source order. `MIN`/`MAX`
are the ordinary fully parenthesised macros (R31 clean). The parameters shadow the members `target` and `lifetime`
and are disambiguated with `this->`, a plain spelling. `unk_Tweaks`'s inline constructor leaves six members unset as
retail does; the banner says so (R36).

**Placeholder names (ruling: conform to the 2026-10-17 placeholder ruling).** `unk_801A92A8` is named after its
constructor; `unk_Ramp`, `unk_Tweaks`, `unk_TweakEntry` are role names, each marked "name invented" in its comment
(the review added the mark to `unk_Tweaks` and `unk_TweakEntry`). Member functions keep `fn_<address>` with mangled
names in `symbols.txt`; unknown globals stay `lbl_`; the `Msg*` objects take their names from the registration strings.

**Renames outside the unit (ruling: accepted).** `fn_801A92A8`/`fn_801A92BC` become `__ct__12unk_801A92A8Fv` and
`__dt__12unk_801A92A8Fv`: the constructor and destructor of the placeholder class, under their real special names, as
the ruling says. `fn_800C770C` becomes `fn_800C770C__Fi`: a free function keeps its placeholder with the mangled form.
Both are provisional on the callers' evidence; the signature of `fn_800C770C` is an open item.

**`()` prototypes in headers (ruling: checker bug, fixed).** R25 treated every `.h` as C, so the C++ game header
`include/game/PadRumble.h` drew four `'()' prototype in C` findings. The checker now takes a header's language from its
includers (C++ when only C++ sources include it, C when any C source does) and, for a header no source includes, from
the convention that `include/game/` and `include/climax/` are C++ (`CPP_HEADER_DIRS`); self-test added. No finding
outside the game headers changed. The checker also reports `unk_<Role>` class names as R7 placeholders.

**`__sinit_\PadRumble_cpp` at 99.73% in a single-symbol objdiff diff (ruling: bytes proven by the gate).** The report
scores it 100%; the one-shot diff shows nine `addi r5, r30, 0xNN` instructions as argument mismatches with identical
text. They are objdiff's inferred pool references on the r30 base: retail resolves them into `lbl_80457480` (the 0x70
block of nine destructor-chain records, kept as one `lbl_` because dtk supplied no `@NNN` names), the compiled object
into its own `@40`-style records. Same bytes; the unit is linked and the clean gate is green, which proves them.

## Game code rulings (game-2)

**Units.** `game/CHitEffectManager` (0x80087614-0x800878D0, 7 functions) and `game/CMapWaypointPurge`
(0x8018D98C-0x8018DBE8, 7 functions) are linked; clean gate `main.dol: OK`, checker clean. Parked:
`game/CAmbientTextMessage` (0x800C2C48-0x800C2FF0) and `game/CAmbientVoicemail` (0x800C2FF0-0x800C3398) on their
sinits (MATCH-PENDING above), `game/unk_8000A040` (0x8000A028-0x8000C32C, the pad manager; every function written, `fn_8000A76C` 99.9%, `fn_8000AF08` 90.6%, `fn_8000B5DC` 90.1%)
and `climax/EventHandler` (0x801A92A8-0x801AA554; six functions at 95-99%, register and schedule residues). Every
behaviour unit emits its inline factory (address taken) and its two this-adjust thunks (`@36@`, for `CEventHandler` at
0x24) after its `__sinit`, so the unit ends after the thunks, not at the sinit.

**GameLib flags (ruling: library-wide `-O3 -ipa file -str noreuse`).**
- `-str noreuse`: the pad manager constructor registers four messages with the payload type `"RumbleStartData*"`, and
  retail has four separate copies of the string addressed in one function (0x8000A1A8-0x8000A1E4: `r31+0x7B0`,
  `0x7C4`, `0x7D8`, `0x7EC`); `CGenericCar::fn_80017E88` (0x8007A060) uses fourteen copies of `"%s%s%s"`. `-str reuse` merges identical literals
  across a whole unit (measured). The alternative, a class-name string returned by an inline function, is refuted:
  MWCC emits such a string once per unit as `@STRING@<function>`, which cannot give four or fourteen copies in one
  function. The two copies of the class name in every behaviour sinit follow from the flag, and the clean gate with
  CHitEffectManager and CMapWaypointPurge linked proves their layout. PadRumble has no duplicate literal (neutral).
- `-O3`: `fn_801A9940` (EventHandler) reaches 100% and the pad manager destructor goes from 81% to 95%; no other game
  function changes. `-ipa file`: `fn_801A93B4` goes from 91.6% to 96.9%; no other change. PadRumble stays 100% under
  every combination.

**Shared framework headers (R1).** The framework types the workers had copied are declared once:
`climax/AttributeHandler.h` (`unk_AttributeCommand`, `unk_AttributePacket`, `unk_AttributeCommandIterator`,
`CAttributeHandler`, the class registry `unk_801A8F6C`, the class-id hash `fn_801A7A84`), `climax/EventHandler.h`
(`CEventHandler`, `unk_Msg`, the message API, by reference), `climax/Memory.h` (`operator new`/`new[]` with heap
arguments, `GAME_HEAP`/`GAME_HEAP_FLAGS`, `fn_801B67FC`), `game/CBaseBehaviour.h` (`CBaseBehaviour`,
`unk_BehaviourBase`), `game/unk_MessageInbox.h` (the phone inbox types) and `game/Display.h` (`lbl_8052861C`). Header
names invented. The pad manager's `unk_MsgReceiver` was `CEventHandler` (vtable lbl_80428260, destructor 0x801A9C34);
its override `fn_8000BFCC` takes the base slot's name `fn_8009CB1C`.

**Attribute iterator (ruling: one iterator; reader test passes).** `operator->` returns `const unk_AttributeCommand*
const&`, so each use reads `current` again, as retail does in every walker (a by-value `operator->` drops
CHitEffectManager, both ambient units and CMapWaypointPurge to 84-96%). String and value payloads are read through
inline accessors (`GetString()`, `GetU32()`, names invented): in CMapWaypointPurge, `fn_801AA218( purgeMsg,
it->GetString(), 0 )` computes the payload address before the other arguments, as an inlined call does, which
`it->data` does not (99% to 100%); the accessors leave the other units unchanged.

**`CHitEffectManager` constructor (ruling: counted; reader test passes).** `lbl_80529690 = this; lbl_80529690->Reset();`
with an inline `Reset()` (name invented) that clears the effects. Retail stores `this` to the singleton before the loop and
then clears through two induction pointers derived from a pointer other than the constructor's `this` (`r27`/`r28` from a
copy of `r30`); the inlined call on `this` (`Reset()`), a hand-written loop on `this`, and clearing before the store all
keep `this` live and index off it (40 of 46 rows differ). So the source must make an inlined call through a second
pointer, and the only one in reach is the singleton the constructor has just set: "become the instance, then initialise
the instance" is an ordinary singleton idiom, every token carries meaning, and no storage or local is invented
(`GetInstance()->Reset()` through a static inline accessor gives the same bytes; the spelling of the access is not
recoverable). It replaces an R17 named copy of `this`. The memset-then-assign double clear of each element is retail's
own (R36), not steering.

**Display-height pair in behaviour sinits (ruling: MATCH-PENDING stays). Superseded by the display-height accessor ruling (game-sinit-trace).** The template form (`template<class T> T
f(const T&)`) has no corroboration: no other construct in the binary is known to read a global through a reference-taking
inline, and plain inline accessors (by value or returning `const int&`), a struct with a `const int&` constructor,
`static`/`const`/`static const` pairs in either order and explicit casts all give one load. It is an invented steering
helper (Section F, "a missing inline helper" row) until such evidence appears. Lead for the real form: the same pair
(`lwz lbl_8052861C` twice, an int-to-float conversion and two `.sbss` stores) opens about 76 static initialisers across
the game, so it is a pair of per-unit statics defined in a shared header that every behaviour unit includes; the source
keeps them as per-unit `lbl_` globals until the header is identified.

**Linker roots (ruling: `force_active` is the mechanism; rule recorded in docs/toolchain.md).** Nothing outside a behaviour unit
names any of its symbols: in the retail asm, no symbol of `game/CHitEffectManager` or `game/CMapWaypointPurge` (functions,
vtable, strings, the class id, the registration object, even the singleton `lbl_80529690`) is referenced from any other
file. The class is reached only by name through the registry that its static initialiser fills, so retail keeps these
objects through their `.ctors` entries alone, whatever the unit boundaries; this is not a symptom of wrong splits. MWLD
keeps an object's `.ctors` only when another section of it is live, and the local `__sinit_` cannot be force-activated,
so each linked behaviour unit lists its class registration object under `force_active` in `config.yml` (`lbl_8052968C`,
`lbl_8052A40C`).

**Library flags spot-check (review).** Old GameLib flags against `-O3 -ipa file -str noreuse`, per function, every
`game/` and `climax/` unit (parked ones included): 10 functions improve (EventHandler `fn_801A93B4` 91.6 to 96.9,
`fn_801A9940` 98.7 to 100, `fn_801A9AE8` 95.3 to 96.1; the four behaviour sinits 63-69 to 83-100; CMapWaypointPurge
`fn_801A73A8` 99.0 to 100; pad manager constructor 95.2 to 100, destructor 81.3 to 95.0), none regresses, PadRumble 10/10.

**Reader-test sample (review, passes).** CHitEffectManager `fn_801A73A8` (iterator walk, kind lookup by name, guarded
store), the CMapWaypointPurge destructor (unlinks and releases its message), the attribute iterator (a reference-returning
`operator->` is an ordinary accessor shape; no dummy state), `GetString`/`GetU32` (payload accessors at the typed payload
boundary), `unk_MessageInbox.h` (offset-named placeholder bytes before the one known member, R27) and the
`CBaseBehaviour` bases: the binary puts `CBaseBehaviour`'s own vptr at 0x2C, which MWCC does only when the first base is
not polymorphic, so an empty non-polymorphic first base is the least hypothesis; its name is invented and marked, and
it adds no vtable slot (R13). Placeholder and invented names follow the user's ruling (`unk_` classes with "name
invented", `fn_` members with mangled names in `symbols.txt`, `lbl_` globals, `__sinit_\<Unit>_cpp`); `CBaseBehaviour`,
`CHitEffectManager`, `CMapWaypointPurge` and `CAmbient*` are DOL strings, `CAttributeHandler` a Ghost Rider literal.

## Game code rulings (game-3)

**Units.** `game/CMapWaypoint` (0x8018D374-0x8018D98C: CMapWaypoint and CRemoveMapWaypoint, 13 functions),
`game/unk_800C8598` (0x800C8598-0x800C875C, 3 functions) and `game/CColdTrigger` (0x800C875C-0x800C8A0C, 7 functions)
are linked; clean gate `main.dol: OK`, checker clean. `game/CAmbientInterferencePoint` (0x800C1E48-0x800C2C48, the base
of the two parked ambient units) is parked (MATCH-PENDING and open items above). Its `.data` starts at 0x803EB2B0: the
three save-data strings that its `.sdata` pointers name come first. In the retail asm no file outside each linked unit
names its symbols, except `__ct__12unk_800C8598FP12unk_801A92A8` (called by the unit at 0x80055464, which embeds the
listener) and `lbl_80527508` (written by the unit at 0x800AEFE4). `force_active`: `lbl_80529AD0`, `lbl_8052A3FC`,
`lbl_8052A404`.

**`unk_800C8598` is its own unit (ruling).** As one unit with CColdTrigger, every function matched but the layout did
not: MWCC emitted CColdTrigger's vtable before the listener's, and the listener's flag `lbl_80527508` (one byte) was
followed by the clamp limits at +4, where retail has them at +8. Both differences are what a unit boundary gives:
the listener's vtable (0x10 bytes) and flag each end an 8-byte aligned block, and CColdTrigger's `.data` and `.sdata`
start on the next 8-byte boundary (0x803EBEA8, 0x80527510). The listener's unit name is unconfirmed.

**CColdTrigger clamps with literal limits through a reference-taking template (ruling: authentic; reader test
passes).** `coldLevel = Clamp< int >( coldLevel, 0, 3 )` with a file-local `template< class T > const T& Clamp( const
T& v, const T& lo, const T& hi )` (name invented). The evidence is the data: `lbl_80527510` (0) and `lbl_80527514` (3)
are `.sdata` words that the code both loads and takes the address of, and a zero in `.sdata` is not a defined global
(MWCC puts zero-initialised globals in `.sbss`, measured). They are the temporaries MWCC materialises when a literal is
bound to a `const int&` parameter: the literal call gives exactly these two words, in this order, with direct
`@sda21` addressing. Named globals land in `.sbss`/`.sdata` at the wrong offsets, and a struct or array of the two
limits (8-byte aligned, so the layout fits) makes MWCC hoist the base address into r31 (96%). The pad manager's
`unk_Fill( ..., ( T* )NULL )` binds a literal the same way (its zero `.sdata` words at 0x80526724 onwards).

**Display-height pair in behaviour sinits (game-3 review: MATCH-PENDING stays). Superseded by the display-height accessor ruling (game-sinit-trace).** CColdTrigger's `Clamp` proves that
Climax code passes values through reference-taking template helpers, which retires the game-2 objection that no such
construct was known. It does not give the sinit's form: the double load needs exactly one of the two initialisers to
read `lbl_8052861C` through a `const int&` parameter and produce the bare value, and a clamp, minimum or maximum of a
variable emits compares that the retail sinits lack. No identity- or conversion-style reference helper is evidenced, so
adopting one in `Display.h` would still be an invented steering helper. CAmbientTextMessage, CAmbientVoicemail, the pad
manager and CAmbientInterferencePoint keep the plain form.

**Display-height pair: the mechanism (game-sinit-trace).** The second load is not an inlining-order effect. MWCC's IR
common-subexpression pass (`IRO_CommonSubs`, through the comparator behind `IRO_ExprsSameSemantically`) treats two
loads as one expression only when their qualifier flags agree: a read through a `const int` lvalue carries the const
flag and a plain read does not (traced; docs/compiler_regalloc.md, "Common subexpressions"). An inline returning
`int&` merges; one returning `const int&` does not. So the header made exactly one of the two reads const, the float
one or the int one (both measure 100%, declared float first). Candidate spellings, none evidenced: an accessor
returning `const int&` (an ordinary C++ getter shape) used for one initialiser, a `const T&` conversion or identity
helper, or a `const int` view of the global. The retail asm gives no further evidence: `lbl_8052861C` is written once
(0x80203348, with the width `lbl_80528618` just before it, from the two arguments of `fn_80203318`), the width is read
once (0x8018F954), the height 352 times in 174 functions, always `@sda21` (a lone word, not a member of an
addressed struct), and outside the 76 static initialisers no block reloads it without an intervening store or
call. No DOL string names the display globals. The user's ruling below picks the spelling.

**Display-height accessor (user's ruling, 2026-10-23).** Adopted: an inline accessor returning `const int&` reads one
of the two copies. `game/Display.h` declares `lbl_8052861C` and `inline const int& GetDisplayHeight()` (name invented);
`game/DisplayHeight.h` (header name invented, "unit name unconfirmed") defines the pair as statics, `static float
displayHeightF = GetDisplayHeight(); static int displayHeight = lbl_8052861C;` (names invented; the float read takes
the accessor, the int read is plain; either way round measures 100%). The pair is internal to each unit, so
`symbols.txt` carries each unit's two words as `scope:local` `displayHeightF`/`displayHeight`. Every game unit whose
static initialiser opens with the pair includes `game/DisplayHeight.h` (directly, in the `.cpp`, where the pair's
place in the initialiser order puts it); units without the pair (CDesignerDebugMsg, CSplashScreenTrigger) include only
`game/Display.h`. Applied to CAmbientInterferencePoint, CAmbientTextMessage, CAmbientVoicemail and the pad manager:
their four static initialisers are 100% and checker clean. CAmbientTextMessage and CAmbientVoicemail are linked
(`force_active`: `lbl_80529A54`, `lbl_80529A64`); CAmbientInterferencePoint (R18, `fn_800C2304`) and the pad manager
(R18 `fn_8000B5DC`, unwritten functions) stay parked. `build/tools/unit.exe check` confirms that nothing outside either
linked unit references it, so each unit's registration object is listed under `force_active`.

**Shared headers (R1/R2).** `game/Map.h`: `unk_Map` (now with `fn_8018C524` and `fn_8018CD44` as members),
`unk_MapWaypoint` and `lbl_8052A3F0`, used by CMapWaypoint and CMapWaypointPurge. `game/Vec3.h`: `unk_Vec3` (from the
pad manager header; replaces CAmbientInterferencePoint's local `RwV3d`). `game/Messages.h`: `lbl_8052A780`,
`lbl_8052A788`, `lbl_8052A798`, `lbl_80529F18` (added by CCombatZoneSetEnemyCountTrigger, game-4). `CBaseBehaviour::fn_8009CD08()`/`fn_8009CD78()` (`const` members returning the position
and the facing vector of the frame's modelling matrix, 0x40 and 0x30 into the frame at `field_0x30`),
`unk_InboxOwner::field_0xA8`, `unk_AttributeCommand::GetFloat()`, and `PostMsg` (send when a handler is linked; name
invented) in `climax/EventHandler.h`, used by the listener and CAmbientInterferencePoint. `fn_801A5E74` is MSL's `acos`:
the eight one-instruction wrappers ending at `pow` (0x801A5E90) are w_acos, w_asin, w_atan2, w_exp, w_fmod, w_log,
w_log10 and w_pow in MSL order; declared in `math.h`. Every existing unit keeps its bytes (clean gate and
`ninja changes`).

**CMapWaypoint destructor frees with scalar delete (R36, counted).** The body deletes the waypoint's two names with
`delete` and sets them to NULL, then `unk_MapWaypoint`'s inline destructor runs with `delete[]` and finds them NULL;
retail calls `__dl__FPv` twice, then `__dla__FPv` twice. The names come from `fn_801B67FC` (`new[]`), so the scalar
delete is the original's mismatch, kept and labelled. The out-of-line `__dt__15unk_MapWaypointFv` the object carries is
dead-stripped at link.

**Rule checker (tools).** R1 counted `class T` in a template parameter list as a type definition (CColdTrigger's
`Clamp` against the pad manager's `unk_Fill`); parameters inside `template < ... >` are now skipped, with two self-tests.

**Reader-test sample (review, passes).** The listener's handler (guards, payload fill, `PostMsg`), CColdTrigger's
`fn_801A73A8` (the clamp through an int temporary, labelled), CRemoveMapWaypoint's handler (empty-name guard before the
map call), CMapWaypoint's attribute walk, and the `unk_Map`/`unk_MapWaypoint` declarations (placeholder fields named
by offset, invented names marked).

## Game code rulings (game-4)

**Units.** Linked (clean gate `main.dol: OK`, checker clean, every function 100%): `game/CGenericSpawner`
(0x800C8108-0x800C8598, 10 functions), `game/CCombatZoneSetEnemyCountTrigger` (0x800C8A0C-0x800C8CB0, 7),
`game/CPadRumbleTrigger` (0x800CCF10-0x800CD374, 7), `game/CPsycologyZoneTrigger` (0x800CE988-0x800CEBF0, 7),
`game/CPsyTrigger` (0x800CEBF0-0x800CEEA4, 7), `game/CSlowMoTrigger` (0x800CF530-0x800CF95C, 7),
`game/CSplashScreenTrigger` (0x800CF95C-0x800D0144, 8) and `game/CSwimStaminaTrigger` (0x800D03EC-0x800D06F8, 7).
In the retail asm no file outside each unit names its symbols, except CSwimStaminaTrigger's message `lbl_80529BE8`
and its name pointer `lbl_8052C8C0`, read by the unit at 0x80151A64 (five and one references), so that object is live
without a `force_active` entry (checked). `force_active`: `lbl_80529AC4`, `lbl_80529ADC`, `lbl_80529B7C`,
`lbl_80529BB4`, `lbl_80529BBC`, `lbl_80529BD4`, `lbl_80529BDC`. CGenericSpawner's `.text` starts at 0x800C8108:
0x800C8100 is CBaseSpawner's `@36@` destructor thunk, the end of the CBaseSpawner unit.

**CGenericSpawner's CBaseSpawner thunk is a deduplicated weak copy (ruling: authentic).** CGenericSpawner does not
override `fn_8009CB1C`, so its object emits the weak thunk `@36@fn_8009CB1C__12CBaseSpawnerFR7unk_Msg` for its
`CEventHandler` vtable (8 bytes, before its own destructor thunk). Retail has one copy of that thunk, `fn_80053190`
(`subi r3, r3, 0x24; b fn_800C7CEC`), in the unit at 0x80052F3C, whose spawner vtable `lbl_803E30D0` (another
CBaseSpawner subclass that does not override the handler) uses it too; CBaseSpawner's and CGenericSpawner's vtables
point at it and neither of their units carries a copy. That is MWLD keeping the first weak definition in link order and
dropping the later ones. `symbols.txt` names `fn_80053190` as that thunk with `scope:weak`, so the linker drops this
object's copy as it did in retail. Checked both ways: with the old global name `fn_80053190` the gate is red (the copy
stays, 8 bytes); with the weak name it is green. The copy has no retail address and is not counted.

**CSplashScreenTrigger's `.sdata2` order comes from an out-of-line constructor (ruling: authentic).** Retail's pool is
0.0f, 1.0f, 0.1f, 20.0f, 640.0f, 255.0f, then the int-to-float double; with the constructor inline in the class
MWCC creates the constants in code order (the attribute handler's 0.1f and 20.0f first, the factory's 0.0f and 1.0f
last: 0.1, 20, 1, 640, 255, 0; objdiff `.sdata2` 75%). Defined in the .cpp, ahead of the destructor, the constructor is
compiled first and creates 0.0f and 1.0f first (its `fade = 0.0f; duration = 1.0f;`); `-inline auto` still inlines it
into the factory, and the linker dead-strips the unreferenced out-of-line copy, so retail has no address for it. Same
mechanism as the axfx `GetMemSize` ruling (a stripped function still leaves its literals in the unit's `.sdata2`). No
MSL inline helper is involved: the unit calls no math function. The constructor is labelled at the code.

**CPadRumbleTrigger's constructor stores the start pad word twice (R36, counted).** Retail stores 0 to 0x40 among the
member initialisers and again, with 0x50, after them. The source sets `startPad` in the initialiser list and both pad
words in the body; the redundancy is the original's, labelled at the code. Reader test passes.

**CGenericSpawner registers its load callback through a function-pointer cast (ruling: authentic).** The static
`fn_800C8268` takes the spawner as `CGenericSpawner*` and is passed as the manager's `unk_LoadCallback` (last parameter
`void*`) with a C-style cast: a cast at a real API boundary, an idiom of the era (R19 allows it there). The other exact
form (a `void*` parameter cast inside) is no more evidenced.

**CSplashScreenTrigger clamps its duration with plain ternaries.** `duration = ( 0.1f > duration ) ? 0.1f : duration;`
then the same against 20.0f. A `const T&` Min/Max template (as CColdTrigger's `Clamp`) gives other code (worker's
probe), so the plain form, which reads as a MIN/MAX macro expansion, stays. Reader test passes.

**Shared headers (R1/R2).** `game/Time.h` (header name invented): `fn_80202384( float scale, float blendTime )`
(signature and name invented; CScreenShakeTrigger calls it too), renamed `fn_80202384__Fff`. `game/CBaseSpawner.h`
(header name unconfirmed): CBaseSpawner, `fn_800C7E84` and `fn_800C7F54`; CBaseSpawner's functions take their compiled
names (`__ct__12CBaseSpawnerFv` 0x800C7B38, `__dt__12CBaseSpawnerFv` 0x800C7C58, `fn_8000CFD0__12CBaseSpawnerFv`
0x800C7BA4, `fn_800C7C04__12CBaseSpawnerFv`, `fn_8009CB1C__12CBaseSpawnerFR7unk_Msg` 0x800C7CEC,
`fn_801A73A8__12CBaseSpawnerFRC19unk_AttributePacket` 0x800C7D60, `fn_801A7BFC__12CBaseSpawnerFv` 0x800C7E80, read from
the slots of its vtable `lbl_803EBD2C`). `game/unk_80491B38.h`: the leading padding is split to name `field_0x4`, the
unscaled frame time (CSlowMoTrigger). The RenderWare globals, device, render
states and `RwIm2DVertex` CSplashScreenTrigger uses come from `include/rwsdk/badevice.h` (the game's former
`unk_RwGlobals`/`unk_Im2DVertex` views were folded into it, byte-neutral by `build/tools/neutral.exe`); the game calls them
through the SDK's macros (`RwRenderStateSet`, `RwIm2DRenderPrimitive`, `RwIm2DGetNearScreenZ`, `RwFreeListAlloc`).

**Reader-test review (passes).** The four new units and the four committed ones were read again (the double pad
store, the callback cast and the clamp above included). [free] fixes: CPsyTrigger's message member `msg` is renamed
`triggerMsg` (it was shadowed by the handler's `unk_Msg& msg` parameter and reached as `this->msg`), and the trigger
headers' class comments now mark their member names as invented. Doc fixes: CColdTrigger's vtable is 0x58 bytes (its
header said 0x40), `game/unk_8000A040` has four unwritten functions (not five), and `Messages.h` also holds
`lbl_80529F18`. No `MATCH-PENDING` changes.

## sdk-nand rulings (MEM, ISFS, IPC client, ESP, ARC)

- **`-char unsigned` withdrawn from `revolution/ipc/ipcMain`.** The one piece of evidence was `strnlen`, whose
  test of its character compiles to `lbz; cmpwi` without sign extension. `strnlen` is ipcclt's own function
  (zeldaret/tp `ipcclt.c`, over `const u8*`), so the unsigned test comes from its parameter type, not from a flag.
  `strnlen` moved to `revolution/ipc/ipcclt` (`.text` from 0x80241C70; ipcMain ends at 0x80241C68). ipcMain
  compiles with the plain SDK profile and stays 100%; no IPC unit has a per-object flag. `strnlen` is declared once,
  in `ipc/ipcclt.h`; fs.c's callers pass `const u8*` names, as in tp.
- **`IPCCltInit` declaration (R2).** The provisional `void IPCCltInit(void);` in `os/OS.h` conflicted with the IPC
  client's `IOSError IPCCltInit(void)` and is removed; `ipc/ipcclt.h` is the single declaration (OS.c includes
  `revolution/ipc.h`).
- **`FSTEntry` (R1).** dvdfs.c and arc.c each defined the 12-byte FST entry; disc FSTs and U8 archives share the
  format. One definition now lives in `revolution/dvd.h`, which arc.c includes. The access macros (`entryIsDir`,
  `fileLength` ...) stay unit-local, as in both references: dvdfs's read the static `FstStart`, arc's take the
  handle's table as a parameter. Both units stay 100%, gate green.
- **`ipc/memory.h`** no longer redeclares `IOSHeapId`/`IOSError` (compiler warning 10122); it includes
  `ios/iostypes.h`.
- **`ISFS_FREE` (reader test passed).** Every fs.c exit path tests `blCtxt != 0` and then frees through a macro that
  tests again; retail performs both tests. zeldaret/tp open-codes the nested pair with the note that the original
  had a macro that already did the NULL check. The macro is the original spelling of that double test, not a lever.
- **Invented names (R7):** `ISFS_GetUsageAsync` (retail function at 0x80241360, absent from tp; named after the
  `ISFS_xxxAsync` pairs, its body the GetUsage request with the `_isfsFuncCb` case 4), the `ES_IOCTL_*` ioctlv command
  constants in esp.c and `ES_EINVAL` (-1017) in `esp.h` (tp writes the literals). Marked as invented at the code.
- **`ARCOpen` (reader test passed).** Not in tp; rebuilt from the DVDOpen shape (path to entry number, the warning
  with the current directory on a miss, refusal of directories, start offset and length from the entry). Its
  warning string and the inlined `ARCGetCurrentDir` are retail evidence. 100%, honest.
- **Reader-test sample (all pass):** MEMAllocFromExpHeapEx, RecycleRegion_, IPCCltInit, IOS_Open, `__ios_Ioctlv`,
  ISFS_OpenAsync, `_isfsFuncCb`, ESP_LaunchTitle, ARCInitHandle, iosAllocAligned. ipcclt's `ROUNDUP` rounds to
  `IPC_BUF_CNT / 2` and `ESP_LaunchTitle` carves its ioctl vector from `buf + 208` of a 256-byte aligned scratch
  buffer; both are the tp spellings and pass. The `func` request kinds in fs.c (0 to 5) are bare numbers as in tp
  (R11 advisory, open).
- **Counts:** MEM 23 functions (four units, 0x8023F0E0-0x802401D0), fs 25, ipcclt 21 (with `strnlen`), memory 4,
  ipcProfile 4, esp 9, arc 12 (ends 0x8024A788): 98 functions linked from source, 97 new to the ledger. No
  MATCH-PENDING, no ORIGINAL-ASM. Checker clean on all of `src/`.

## sdk-nand-2 rulings (euart, NAND, SC)

- **`nandReportErrorCode` (R17, Section F; reader test passed).** The static is inlined twice into
  `nandConvertErrorCode`, and at both sites retail evaluates the range test `err < -23 && err > -100` (`cmpwi; bge;
  cmpwi`) and discards it. tp has no form (its `nandReportErrorCode` is an empty stub marked NONMATCHING, so the
  test is missing there). An `if` with an empty body drops the compares (95.4%), so only an expression statement
  produces them; the source is `(void)(err < IOS_ERROR_INVALID_SIZE && err > NAND_ISFS_ERROR_BASE);` with a comment
  that the release build keeps only the test of the debug report. No storage, no volatile, no steering: it is
  the dead code retail contains, written once, not a carrier. Counted. If a public SDK spelling appears (a
  release-mode report macro that still evaluates its argument), adopt it.
- **`ParseConfBuf` reference-table size (R36, kept).** Retail sizes the table `2 * (SC_ITEM_ID_MAX_PLUS1 - 1)`
  bytes below `bufEndp` (immediate 72 for 37 ids), where tp writes `2 * SC_ITEM_ID_MAX_PLUS1`. The id table is
  written as `runtimeRefp[-id]` from `bufEndp - 2`, so id 36 lands one slot below the cleared and bounds-checked
  region: a genuine off-by-one of the shipped SDK, kept and commented at the code.
- **`SCGetBtDeviceInfoArray` / `SCGetBtCmpDeviceInfoArray` return `void`** as in ogws (a tail call: the bytes do
  not fix the return type, and the WPAD callers ignore `r3`). Bytes unchanged.
- **NANDOpenClose dead-stripped functions.** `NANDSimpleSafeOpen` and `nandSafeOpen` (tp sync functions, no retail
  copy) are kept because their strings occupy the unit's `.data` in that order; the gate proves the layout.
- **NANDSecret removed.** The stub unit's range was the tails of NANDCheck (`nandCalcUsage`, `NANDCheckAsync`,
  `nandUserAreaCallback`, `NANDGetFreeSpace`) and NANDCore; no unit-name guess remains in NAND.
- **`fn_802494B4`** (0x802494B4-0x80249790) between scapi_prdinfo and esp is game audio code, left unassigned in the
  auto range.
- **Data scores.** NANDCore `.data` scores 56% and NANDLogging 92% in objdiff while code is 100% and the gate is
  green: retail's string literals are `lbl_`/`gap_` blocks covering several strings, the compiler's are one `@NNN`
  per string, so symbol-level data pairing fails. Section bytes match (the gate). String-pool naming only.
- **`UARTError` defined once.** `revolution/euart.h` owns the enum (`kUARTNoError`, ...); `TRK/uart.h` includes it.
  MetroTRK `msg.c` and `serpoll.c` now use `kUARTNoError` (was `UART_NoError`, same value); both stay 100%,
  gate green. `strcmp` is declared in `string.h` and `atoi` in `stdlib.h`; `fn_8019F638` is renamed `atoi`.
- **Reader-test sample (all pass):** NANDInit, nandConvertPath, NANDOpenAsync, nandSafeOpenCallback,
  NANDCheckAsync, NANDLoggingAddMessageAsync, SCInit, ParseConfBuf, SCFlushAsync, SCGetProductArea, EUARTInit. The
  tp spellings' bare numbers (safe-open `state` steps, the console-type mask `0x10000000`, the EUART register
  bytes) remain as in the reference (R11, open).
- **Counts:** euart 3, nand 20, NANDOpenClose 19, NANDCore 19, NANDCheck 4, NANDLogging 3, scsystem 21, scapi 17,
  scapi_prdinfo 3: 109 functions in nine `Matching` units. No MATCH-PENDING, no ORIGINAL-ASM. Checker: 0 blocking.

## sdk-av rulings (PAD, AI, AX)

- **`__AICallbackStackSwitch` is ORIGINAL-ASM (accepted, listed above).** tp and ogws both write it as a `static asm`
  function; the source is tp's body. Not counted (100 bytes).
- **Invented names (recorded; marked at the code).** `__AXAllocQuit`, `__AXAuxQuit`, `__AXClQuit`, `__AXOutQuit`
  (functions of the units built here) and `__AXVPBQuit` (0x80230190), `__AXSPBQuit` (0x8022F4A0) (renamed in
  `symbols.txt` only; units not built): neither ogws (no `AXQuit`) nor tp (no AX.c) has a Quit chain, so the names
  follow the Init ones; the identities come from `AXQuit`'s call order mirroring `AXInitEx`. `__AXCompressorTableAddress`
  and `__AXCompressorReleaseFrames` (AXCL statics; ogws has `THRESHOLD`/`RELEASE_FRAMES` constants with the note that
  later AX versions make them static variables). The `AI_REG_*`/`AI_CONTROL_*` macros in ai.c and the `DSP_REG_*`,
  `DSP_CSR_*`, `DSP_AI_DMA_*` macros in dsp.h are invented (tp uses bare indices; ogws has its own invented
  `AI_AICR`/`DSP_AI_DMA_CSR` names). `AXPROFILE.unk_0x34` is a placeholder (ogws `WORD_0x34`; tp has no member there).
- **Renames verified.** `fn_80236C10`..`fn_80236DF0` -> `DSPCheckMailToDSP`, `DSPCheckMailFromDSP`, `DSPReadMailFromDSP`,
  `DSPSendMailToDSP`, `DSPInit`, `DSPCheckInit`, `DSPAddTask`, `DSPCancelTask`, `DSPAssertTask`: tp's dsp.c order with
  `DSPReadCPUToDSPMbox`, `DSPAssertInt`, `DSPReset`, `DSPHalt`, `DSPUnhalt`, `DSPGetDMAStatus` stripped (ogws has the same
  order without `DSPCancelTask`, which retail keeps for `__AXOutQuit`); the sizes fit and the AXOut call sites agree.
  The AX callees (`__AXVPBInit`, `__AXSPBInit`, `__AXClInit`, `__AXOutInit`, `__AXSetPBDefault`, `__AXGetStudio`,
  `__AXPrintStudio`, `__AXGetNumVoices`, `__AXSyncPBs`, `__AXGetPBs`, `__AXGetCurrentProfile`) follow ogws's call sites
  in AXInitEx, AXAlloc, AXCL and AXOut.
- **Bare pointer tests.** ai.c's `if (__AID_Callback && ...)` / `if (__CallbackStack)` are tp's text (the `!= NULL`
  spelling gives the same bytes, so tp's spelling stays). AXOut's `if (__AXUserFrameCallback)` and `if (current)` differ
  from ogws's `!= NULL`, which drops `__AXOutNewFrame` to 98.9% (each); kept for the bytes, commented at the code.
  AXAux's `!= 0` callback tests likewise (comment at the top of the file). No [match] issue.
- **Retail differs from ogws** (later SDK, written from the bytes): the `__init` guard in `AXInitEx` and `AXQuit`,
  `__AXOutAiCallback`'s early return when `__AXDSPDoneFlag` is set, and `__AXOutQuit`. `__AXOutFrame`,
  `__AXDSPInitFlag` and `__AXDSPDoneFlag` are `volatile` as in ogws.
- **Headers.** `OSNanosecondsToTicks` moved into `os/OSTime.h` in tp's form (ai.c's local copy dropped); `__PADSpec`
  is declared in `revolution/pad.h` (removed from the pending block of `os/OS.h`; OS.c includes pad.h). The
  `DSP_SEND_MAIL_SYNC` macro (ogws `dsp_task.h`) and AXCL's `LIST_WRITE_16`/`LIST_WRITE_32`, which had been collapsed onto one line each, are back on continuation lines. All byte-neutral (clean gate).
- **Functions kept without a retail copy.** ai: `AISetDSPSampleRate`, `AIGetDSPSampleRate` (inlined into `AIInit`),
  `AIGetDMAEnableFlag`, `AIReset` (dead-stripped). AX: `AXInitEx` (inlined). AXAlloc: `__AXInitVoiceStacks`,
  `__AXPopFreeStack`, `__AXPopCallbackStack`, `__AXPushStackHead`, `__AXPopStackFromBottom` (inlined),
  `AXSetVoicePriority` (dead-stripped). AXAux: `AXGetAuxBCallback`, `AXGetAuxCCallback` (dead-stripped). AXCL:
  `__AXWriteToCommandList` (inlined), `AXSetMasterVolume` (dead-stripped). AXOut: `AXRmtGetSamplesLeft` (inlined).
- **Reader-test sample (all pass):** AIInit, `__AIDHandler`, AIStartDMA, AXInit, AXAcquireVoice (AXAlloc has no
  `__AXAllocVoice`), AXRegisterAuxACallback, `__AXNextFrame`, `__AXOutInit`. Open (R11): bare mode numbers
  (`AXInitEx(0)`, `__AXOutputBufferMode == 1`, `__AXOutDspReady` states) as in ogws.
- **Counts:** Pad 1, ai 11 (+1 ORIGINAL-ASM), AX 3, AXAlloc 9, AXAux 21, AXCL 13, AXOut 12: 70 matched functions and
  one ORIGINAL-ASM in seven `Matching` units. No MATCH-PENDING. Checker: 0 blocking.

## sdk-vi-wpad rulings (VI, KPAD, WPAD, WUD)

- **Counts (recounted from the gate's report).** vi 26, i2c 3, vi3in1 15: 44 VI functions at 100%, not 56. i2c and
  vi3in1 are `Matching` (18 counted). `vi.c` is parked because of `__VIInit` (MATCH-PENDING above), so its 26 are
  not counted. `wpad_debug_msg` (`DEBUGPrint`) is `Matching` and counted. WUD `debug_msg` (`WUD_DEBUGPrint`) is parked
  (MATCH-PENDING above). Of KPAD's 29 functions, 22 are at 100% (parked).
- **R17 cases: a rule question for the user, not ruled here.** Both are reference text: tp's `volatile u32 a` delay
  loop in `__VIInit`, and tp's unused `va_list list` in `WUD_DEBUGPrint`. R17 as written (a volatile local always
  fires; a local that is never read fires unless an R36/BUG/NOTE comment sits within two lines) blocks both. They
  stay MATCH-PENDING. The proposed rule change is in the merge report for the user. `WUD_DEBUGPrint` has a clean
  alternative, ogws's spelling (see the table above). **Resolved at the sdk-av-2 merge:** the user ruled on 2026-10-18 that R17 accepts the noted SDK delay-loop counter (AGENTS.md, project profile), so `__VIInit` carries `// volatile: delay loop, zeldaret/tp libs/revolution/src/vi/vi.c` and `vi.c` is `Matching` (26 counted); `WUD_DEBUGPrint` takes ogws's spelling (`#pragma unused(fmt)`, no local; 100%) and WUD `debug_msg` is `Matching`.
- **`VISetCGMS` line number (ruling: unknown placeholder, as in "OS unit rulings (sdk-os-5)").** The function was
  dead-stripped; only its report string and `__FILE__` remain in `vi3in1.c`'s `.data`. `OSPanic(__FILE__, 1, ...)`
  reaches no retail byte, and the gate is green whatever the line is. It is marked as an unknown placeholder at the
  code.
- **`block_relocations` 0x8043C0E8 (accepted).** These are the `gammaSet[10].yout[5..6]` halfwords (0x8000, 0xA000)
  in vi3in1's gamma table, not a pointer. dtk took the word for an address into `.text`. Gate green with the entry.
- **GX's local VI constants (accepted).** `GXFrameBuf.c`'s private `VI_TVMODE_*`/`VI_XFBMODE_*` macros were removed
  in favour of `vi.h`'s `VITVMode`/`VIXFBMode` (tp names). GXFrameBuf stays 100%, 13/13, and the gate is green.
  `GXPal528Prog`, `GXPal528ProgSoft` and `GXPal524ProgAa` are declared in `gx/GXFrameBuffer.h` and defined in `vi.c`,
  as in tp.
- **Cross-lane edits (checked).**
  - `dvd/__dvd.h`: declares `__DVDStopMotorAsync`, `__DVDRestartMotor` and `__DVDStopMotorCommandBlock` (defined in
    `dvd.c`). The `dvd.c` stub signature `__DVDStopMotorAsync(DVDCommandBlock*, DVDCBCallback)` stays at 100%.
  - `mtx.h`: `Vec2` and `PSMTXMultVec` (SDK names).
  - `math.h`: adds `double sin(double)` and `double sqrt(double)` in MSL's form, next to main's `fabs`.
  - `symbols.txt` renames outside the lane's ranges: `fn_801A5D30` -> `sin` (fdlibm `s_sin` shape, `lis 0x3fe9`
    pi/4 test), `fn_801A60E0` -> `sqrt` (a 4-byte `b` to `__ieee754_sqrt`: MSL `w_sqrt.c`), `fn_8021EEA0` ->
    `PSMTXMultVec` (the paired-single body, 0x54 bytes). The WPAD API renames are inside the WPAD ranges. No other
    hunk of `configure.py`, `splits.txt` or `symbols.txt` touches anything outside VI, KPAD, WPAD, WUD and these
    callees.
  - `lbl_8052B113` resized from 0x5 to 0x1 (`WPADEncrypt` `.sbss` ends at 0x8052B114; byte accesses only).
  - Rebase fix: main's `game/unk_8000A040.h` had declared `fn_8027C080`/`fn_802780A0` itself, and main's
    `symbols.txt` had mangled them (`__Fii`/`__Fiff`). The header now includes `revolution/kpad.h` and calls
    `KPADSetPosPlayMode(0, KPAD_PLAY_MODE_LOOSE)` and `KPADSetPosParam`. The unit is parked; its score is unchanged.
- **Invented names (recorded; marked at the code).** `VISetTimeToDimming` (0x8022A860; tp has no setter of
  that kind), `VIDisplayState` (tp: `SomeVIStruct`), and vi.c's macros `VI_DIM_*`, `VI_DVD_STOP_*`, `VI_DI_*`,
  `VI_VERT_COUNT`/`VI_HORIZ_COUNT`, `U32_MAX` and `VI_FBB_*`. In KPAD: `KPAD_FMT_BTN_ACC_DPD_EXTENDED`,
  `KPAD_RING_SAMPLES`, and the `KPADInsideStatus` members that tp/ogws lack (listed at the struct). The unit name
  `wpad_debug_msg.c` is tp's; ogws calls the file `debug_msg.c`. It is unconfirmed for this binary and marked at the
  top of the file.
- **Library map.** VI 0x80227F50-0x8022CA00. TPL 0x80277F60-0x802780A0. KPAD 0x802780A0-0x8027C098. A USB/IOS HID
  client 0x8027C0A0-0x8027D660 (string `USB: `; reference ogws `src/revolution/USB`), unowned. Two 8-byte stubs at
  0x8027D660 and 0x8027D670 have no known owner. WPAD 0x8027D680-0x8028A4C0 (five units) and WUD
  0x8028A4C0-0x80290F00 (three units). RenderWare starts at 0x80290F00 (`RwBBoxCalculate` is a bounding-box
  accumulate), so 0x80290F00-0x80293078 is not WUD. See `library_map.md`.
- **Reader-test sample (no [match] finding, open readability work recorded).** VISetNextFrameBuffer passes.
  `__VISetRevolutionModeSimple` passes with notes (bare `__VISetVolume(0x8E, 0x8E)`). Open (R16/R11/R30, not
  blocking): VIConfigure open-codes `OLD_SET_REG_FIELD` expansions at two sites (one with a no-op `| 0`) next to calls
  of the macro, so restore the macro there. VIInit, `__VIInit` and VIConfigure unpack `VI_TVMODE` by hand
  (`& 3`, `>> 2`) and compare `tv == 3` instead of `VI_DEBUG`. `__VIRetraceHandler` reuses `regIndex` (a bit count,
  then a mask) and reads `__VIRegs[0x1E]` into an unread `reg` (needs a why-comment). `__VISendI2CData` writes the
  `__i2c_ident_flag` SDA swap out six times (a helper candidate) and calls `WaitMicroTime(2)` twice in a row. KPAD
  (parked): `KPADiRead`'s `param_3`/`param_4` and `calc_acc_horizon`'s reused `f1`/`ax`/`ay`/`vx`/`vy` are
  placeholders (R7/R30).

## sdk-wpad-2 rulings (KPAD, WPAD, WPADMem)

- **KPAD link trial.** Flipped to `Matching`, clean gate `main.dol: FAILED`; attributed by a DOL compare: only the two
  register residues differ (75 bytes in `read_kpad_dpd`, 12 in `KPADInitEx`). The suspected `.sdata2` duplicate
  constants and `.bss` gap are not walls. KPAD stays parked (see the parked list). No permuter or allocator trace run.
- **Invented names (R7; recorded).** `WPADiClearMemBlock` and `WPADWriteExtReg` (WPADMem; neither tp nor ogws has
  them; marked at the code). KPAD's `KPADInsideStatus` placeholders (`field_0x55C`, `field_0x55D`) and the `lbl_8052B08x`
  balance-board flags stay placeholders. The lane's other WPAD/WUD renames (42 in `symbols.txt`) are reference names:
  40 found in ogws/tp/WUD, in ogws's and WUD.c's definition order with no inversions.
- **`WUDStartSyncSimple` (unconfirmed shape).** The name is the public WUD API (WUD.c in the reference), but retail
  is a four-argument tail call (`li r3,1; li r4,-1; li r5,0; li r6,1; b fn_8028DC50`), not the reference body; the
  name stands, the body is another revision's.
- **`WPAD_FMT_BTN_ACC_DPD_EXTENDED` (9).** Not ruled here (review cut short by the context budget); it stays as the
  lane wrote it, an open item to settle against the format switch in `WPADSetDataFormat` and the reference enums.
- **`symbols.txt` (checked).** `lbl_8052EE08` 4 -> 6 bytes is KPADiSamplingCallback's `{1, 1, 1}` s16 initialiser
  (`lwz` +0, `lhz` +4); `lbl_8052EE0C` was only its +4 half (removed); `lbl_804F1388` 0x38 -> 0x20 is `f64[4]`, the 0x18
  bytes after it are unreferenced (dtk gap) and the gate trial shows them identical. Other data renames
  (`fs_acc_rotation`, `_wpdcb`, `_dev_handle_index`/`_wpd`, `kp_fs_acc_rot_deg`, `kpad_initialized`, `_callbackByKPAD`,
  `_speakerVolume`, `_rumble`, `_sensorBarPos`, `_dpdSensitivity`, `_sleepTime`) follow ogws. `read_kpad_stick` and
  `IsBusyStream` are static in source without `scope:local` in `symbols.txt` (bytes unaffected).
- **sc.h (header request).** `SCStatus` (`SC_STATUS_OK`, `SC_STATUS_BUSY`, `SC_STATUS_FATAL`; 0-2, ogws names) and
  `SCMotorMode` (`SC_MOTOR_OFF`, `SC_MOTOR_ON`) added to `sc.h`, replacing WPAD.c's local macros (byte-neutral; gate green).
- **Reader test (no [match] finding; open readability work).** WPADiClearMemBlock passes. WPADWriteExtReg passes with
  notes: `status` holds the controller status, then the queue result (R30); the header should say the callback also
  fires for WPAD_ERR_NO_CONTROLLER. WPADiRecvCallback passes with notes (`length` unread in retail too; the `u8 chan`
  makes the table's -1 255, which is the rejection: comment it). WPADSetDataFormat and WPADRead lack header comments
  (R33); WPADRead's `(u8)(cb->rxBufIndex == 0)` is retail's byte truncation (tp has it) and wants a why-comment, as
  does its error-size override. File level: WPAD.c uses `__attribute__((aligned(32)))` where `ATTRIBUTE_ALIGN(32)`
  exists; `_callbackByKPAD` is `static` without `scope:local` evidence. KPADiRead, read_kpad_stick and KPADInitEx were
  not reviewed in this pass (context budget).

## sdk-av-2 rulings (AXSPB, AXVPB, AXProf, AXComp, DSPCode, DSP, THP)

- **R17 ruling of the user (2026-10-18), implemented here.** R17 accepts a `volatile` local used only as the counter of
  an empty-bodied busy-wait loop when its declaration carries `// volatile: delay loop, <reference project and file>`
  and the reference has the same form (AGENTS.md, project profile). `build/tools/check_rules.exe` skips the volatile-local
  finding only when that note is present and the variable is a plain scalar declared without an initialiser whose
  every other use is in the header of one `for (v = A; <test of v>; v++)` loop with a `{}` or `;` body
  (`_is_delay_counter`). The self-tests cover the noted shape (trailing and line-above note, `;` body), the shape
  without the note or with another note, and uses outside the loop (after it, in the body, an initialiser, a mail
  local). Applied: `__VIInit` (tp's `volatile u32 a; for (a = 0; a < 1000; a++) {}`) carries the note and `vi.c` is
  `Matching` (26 counted). `WUD_DEBUGPrint` takes ogws's `#pragma unused(fmt)` form (100%, no rule change needed) and
  WUD `debug_msg` is `Matching`.
- **User's ruling (2026-10-18): `__DSP_boot_task` stays MATCH-PENDING; R17 is not widened further (the assert-only
  volatile-local variant is declined).** Superseded by the user's ruling of 2026-11-15 ("Rulings of 2026-11-15"): the
  assert-only local is accepted and `__DSP_boot_task` passes the checker.
- **`__DSP_boot_task` is not the delay shape (MATCH-PENDING, table above).** tp's
  `volatile u32 mail` is assigned once from `DSPReadMailFromDSP()` after the wait loop and read only by the
  release-disabled `ASSERTMSG1`; retail keeps its store (`stw r3, 0x8(r1)`) and never reloads it. No loop header
  uses it, so neither the counter shape nor a mail-wait shape covers it, and the checker was not widened. If the user
  wants it accepted, the narrow form would be: a volatile scalar local, written once from a call, read only inside an
  assert macro argument, with a `// volatile:` note naming the reference. `dsp_task.c` is parked meanwhile (its other
  4 functions are at 100%). The lane's note said retail "stores and reloads" the local; it only stores. The lane's
  assert string held a raw newline (MWCC warning 10449); fixed to `\n`.
- **`#pragma dont_inline on`/`reset` around `__AXDepopFadeMain` and `__AXDepopFadeRmt` (accepted, PROVISIONAL).**
  Retail calls the two fades out of line, 20 times from `__AXPrintStudio`; with the SDK's `-inline auto` and no
  control, all 20 calls inline. The control is a build decision about the inline set, not code: it changes only
  whether the two functions are inlined and is scoped to exactly the functions retail keeps out of line. The review
  got the same bytes with a per-object `-inline noauto` and no pragma (all 7 functions 100%). ogws makes the same
  decision with its `DECOMP_DONT_INLINE` attribute on these two functions. Ruled like EXIBios's `-schedule off`: an
  original per-unit or per-function build setting, not a lever. Which spelling the SDK used (pragma or file flag) is
  unproven; the pragma stays because it names the two functions at the code.
- **`AXSetVoiceAdpcm`'s ten hand-unrolled word copies (passes the reader test).** ogws writes the sibling
  `AXSetVoiceAddr` the same way (`*dst = *src; dst++; src++;` four times). The structure assignment
  `vpb->pb.adpcm = *adpcm` scores 71.9% and `FUNNY_COPY(..., u32, ...)` 15.3% (review probes, reverted), so the copy is
  the SDK's own unrolled form, not a steering device. It has a comment at the code.
- **`AXGetLpfCoefs`'s `(f32)cos(...)` (passes the reader test).** With ogws's `cosf` the function still scores 100%,
  but the gate goes red: the `cosf` inline instantiation emits the `.sdata2` constants in another order (review probe,
  reverted). `(f32)cos(x)` is exactly what the inline expands to, and `(f32)sqrt` is needed anyway because retail
  calls `sqrt` (MSL `w_sqrt.c`, 0x801A60E0). Comment at the code.
- **DSPCode's microcode bytes from the retail DOL (accepted; data, not code).** `axDspSlave` is the DSP slave
  program image: `.data` that the PowerPC copies to the DSP and never executes, and that no compiler produces from C.
  It is a byte array (R24 generated material), not an assembly body, so R18 does not apply. The bytes differ from
  ogws's (another AX version) and were taken from the retail image; the vectors and length follow ogws's layout.
- **THP ORIGINAL-ASM (accepted, listed above).** `__THPInverseDCTNoYPos`, `__THPInverseDCTY8` and the three
  `__THPHuffDecodeDCTComp*` functions are asm blocks in register-parameter functions in mkdd's THPDec.c, as in
  retail. Listed apart and counted nowhere (5 functions, 7364 bytes). The `ASM-HELPER` helpers are in the helpers
  table: `__THPGQRSetup`/`__THPGQRRestore` (mkdd), `__THPHuffDecodeTab` (mkdd; called only from the ORIGINAL-ASM
  functions) and `OSInitFastCast`.
- **Headers (byte-neutral, clean gate).** `OSInitFastCast` moved from THPDec.c into a new
  `include/revolution/os/OSFastCast.h` in the SDK form (ogws's `OSFastCast.h`, with the `OS_GQR_TYPE_*` names), which
  `revolution/os.h` includes as the SDK does. `HID2_LCE` moved into `base/PPCArch.h`'s HID2 block. THPDec.c called
  `PPCMfhid2` with no prototype in scope; it now includes `base/PPCArch.h`. `math.h`: main already had
  `double sqrt(double)` from the vi-wpad merge, so the lane's duplicate line was dropped; `fn_801A60E0` -> `sqrt` was
  already in main's `symbols.txt`. `DSP_TASK_FLAG_CLEARALL`/`_ATTACHED`/`_CANCEL` added to `dsp.h` (tp names).
- **Readability fixes in review (byte-neutral).** `__DSPHandler` tested `flags & (1 << (31 - 0x1E))` and
  `(mail + 0x232F0000) == 2` (tp's text, m2c-style). It now reads `(flags & DSP_TASK_FLAG_CANCEL) && mail ==
  DSP_MAIL_FROM_YIELD`, with a comment (a task cancelled while it runs finishes at its next yield). `DSPInit`'s
  `__DSPRegs[DSP_REG_CSR] = tmp = tmp & ~0xAC;` is now two statements, like the pair above it. AXVPB's copied ogws
  developer comment on `FUNNY_COPY` was replaced by a one-line description.
- **`-fp_contract off` on AXVPB (accepted).** `AXGetLpfCoefs` (`rf31 * rf31 - 1.0f`) has no `fmsubs` in retail; ogws
  and tp build the RVL SDK with `-fp_contract off`. Per object, as for OSFatal and KPAD.
- **Reader-test sample (no [match] finding).** `__AXServiceVPB` passes (ogws's form; the `*(u32*)&...AddressHi`
  stores move a Hi/Lo halfword pair as one word through named members, as in the `OSSetErrorHandler` R4 ruling, not
  offset arithmetic). `AXSetVoiceSrc`, `__AXSPBInit` (retail's reverse store order), `DSPInit` and `__DSPHandler`
  (after the fixes above) pass. `THPVideoDecode` passes (mkdd's `goto` error exits with meaningful labels and one
  shared exit, commented). `THPAudioDecode` passes (named `THP_PCM_*`/`THP_MIXDOWN_*` constants, the mono mixdown
  commented). Open (R16, not blocking): AXVPB and AXOut define two different `FUNNY_COPY` macros (`u16` and `int`
  index), and `AXITD` is a placeholder type local to AXVPB.c.
- **Library map.** THP 0x80209D80-0x8020D778 (`fn_80209D54` is game code); AXSPB 0x8022F1D0-0x8022F60C, AXVPB
  0x8022F610-0x80230988, AXProf 0x80230990-0x802309D0; DSP 0x80236C10-0x80237798 (GX at 0x802377A0). The unnamed
  block 0x802309D0-0x80236C10 (53 functions, 24,692 bytes; AXFX candidates) is left for a future lane. New dtk
  alignment pads: `pad_03_8020D20C_text`, `pad_03_8020D778_text`, `pad_03_8022F60C_text`, `pad_03_80230988_text`,
  `pad_03_80236EAC_text`.
- **Counts:** AXSPB 7, AXVPB 24, AXProf 1, dsp 9, dsp_debug 1, THPDec 11 (+5 ORIGINAL-ASM), THPAudio 1, plus vi 26
  and WUD debug_msg 1 from the ruling: 81 functions newly counted. AXComp and DSPCode are data units. dsp_task (5 at
  100%) is parked. Checker: 0 blocking.

## sdk-axfx rulings (AXFX reverbs and hooks)

- **`AXFX*GetMemSize` in the Exp units (accepted as authentic, body PROVISIONAL).** `AXFXReverbHiExpGetMemSize`,
  `AXFXReverbHiExpGetMemSizeDpl2` and `AXFXReverbStdExpGetMemSize` have no retail copy. The first two are real SDK
  functions: doldecomp/ogws declares `u32 AXFXReverbHiExpGetMemSize(const AXFX_REVERBHI_EXP*)` and
  `u32 AXFXReverbHiExpGetMemSizeDpl2(const AXFX_REVERBHI_EXP_DPL2*)` in `include/revolution/AXFX/AXFXReverbHiExp.h` /
  `AXFXReverbHiExpDpl2.h`, and its `AXFXReverbHi.c`/`AXFXReverbHiDpl2.c` define `AXFXReverbHiGetMemSize[Dpl2]` first in
  the file, calling them. The game calls none of them, so the linker stripped them (retail's `AXFXReverbHi.c` likewise
  starts at Init; `AXFXReverbHiExpSettings[Dpl2]` are stripped too). A stripped function still leaves its literals in
  the unit's `.sdata2`: retail's pool starts with 32000.0f, which Init (first use 0.0f) cannot put first. zeldaret/tp
  covers the same slot with an invented `f32 dummy_0() { return 32000.0f; }` (and `dummy_1() { return -3.0f; }` before
  `__InitParams`), which is a steering stub; the lane's function is the SDK's own, so it is not R17 steering. No public
  body exists: the body is reconstructed from Init's allocation sizes (only its first float literal is visible in
  retail), so it is PROVISIONAL. The Std one has no public declaration and is modelled on the Hi one (invented name).
  Without the function the pool starts 0.0f, 32000.0f (review probe).
- **The red Exp gate was `pow`, not GetMemSize (fixed with `powf`).** Byte attribution of the lane's state (HiExp alone
  linked): the first differences are in `AXFXReverbHiExp`'s `.sdata2` (0x8052EB98-0x8052EBD8), and everything after it
  shifts by 8 bytes (`.text` sda21 offsets from 0x80231863 on, `.sdata2` from 0x8052EBB8). The 32000.0f was already
  first. Retail's pool is `32000f, 0f, 1f, 0.6f, 0.5f, pad, 4330000080000000, -3.0f, pad, 10.0 (double), 0.95f, pad,
  4330000000000000` (0x40 bytes); the build had the 10.0 double ahead of -3.0f and no padding (0x38 bytes). The
  comb coefficient `pow(10.0f, -3.0f * len / (time * 32000))` (tp's text) creates the double 10.0 before -3.0f. With
  MSL's inline float wrapper `powf` (`inline float powf(float x, float y) { return pow(x, y); }`, added to
  `include/math.h` beside `cosf`) the constants are created in retail's order and all three Exp pools are byte-identical
  to retail, with every function still 100%. Same mechanism as the AXGetLpfCoefs `cosf` ruling (sdk-av-2), here in
  the other direction. tp's `dummy_1` stub was covering this. `AXFXReverbHiExp` and `AXFXReverbHiExpDpl2` are
  `Matching` (clean gate green); so is `AXFXReverbStdExp` (ruling below).
- **`AXFXReverbStdExpCallback` (sdk-axfx-2).** No public source exists; the body follows the HiExp Callback (tp)
  with two combs and two allpasses written out instead of loops. Three source facts carry the match, each checked
  against retail's register and load order: the six delay line positions are read into locals at the top of each
  sample (the field reads are not hoisted out of the channel loop otherwise); the position updates after the channel
  loop are six written-out `if (++pos >= length)` statements (the `for (i < 2)` loops of the Hi text load the length
  after the store); the coefficients are read in structure order and the mix is `earlyOut * earlyGain + fusedOut *
  fusedGain`. The remaining GPR residue (three loop temporaries rotated through r24-r26) was traced with
  `build/tools/wdbg.exe` (simplify push order at `0x5b8e90` (static in Coloring.obj; simplify push), colours at `0x5b8a90` (static in Coloring.obj; operand rewrite)): the reused `combLine`
  local was a low-index node coloured last, so r24 joined the reserve set too late for the early-reflection
  temporaries. Indexing the comb lines through `reverb->combLine[ch][n]` directly, while the two allpass stages keep
  sharing one `allpass` pointer as in the HiExp text, gives retail's numbering; a comb line local or one local per
  allpass stage does not (99.93%). No permuter used. Unit `Matching`, clean gate green.
- **`-fp_contract off` objects:** `AXFXReverbHiExp.c`, `AXFXReverbHiExpDpl2.c`, `AXFXReverbStdExp.c` (per object, as
  AXVPB, OSFatal and KPAD; tp and ogws build the RVL SDK with it). The three thin units and AXFXHooks have no float
  arithmetic.
- **ogws Dpl2 offsets corrected.** `AXFX_REVERBHI_DPL2`'s simple parameters sit at 0x178-0x18C after the 0x178-byte
  `AXFX_REVERBHI_EXP_DPL2` (ogws's header comments are off); the layout is taken from retail's `__ParamConvert`.
- **Reader-test sample (no [match] finding).** `AXFXReverbHiInit` (ogws/tp form: `__ParamConvert` then the Exp
  Init), `AXFXReverbHiCallback` (ogws's `void*` callback signature with the casts to the Exp types at the one
  boundary), `AXFXReverbStdInit` (same shape as Hi; names invented, recorded in open items), `AXFXSetHooks` and
  `__AXFXAllocFunction` (ogws, `OSAllocFromHeap(__OSCurrHeap, size)`) pass. Open ([free], not blocking): R33 banners
  missing on most AXFX functions, R35 brace-less `if`s and R11 bare 32000 (the sample rate) in the Exp units (tp text).
- **Library map.** AXFX 0x802309D0-0x802334B4 (34 functions; no Chorus or Delay in retail); AXART 0x802334C0-0x80236C10
  (19 functions; `.data` 0x8043FA70-0x80440600, `.bss` 0x804AE000-0x804B2380, `.sbss` 0x8052AEF8-0x8052AF10) is
  untouched, a lane candidate with zeldaret/tp `libs/dolphin/src/axart/*.c` as reference.
- **Counts:** 27 functions newly counted (13 claimed by the lane plus the 14 of the two Exp units unparked at review).
  Checker: 0 blocking.

## sdk-mix rulings (MIX mixer, the range once listed as AXART)

- **Identity.** 0x802334C0-0x80236C10 is the SDK MIX mixer, not AXART: tp's GameCube `mix.c` gives the same tables
  (volume, pan, DPL2 front/rear), the same `MIXChannel` layout and the same function order; the Wii build adds the auxC
  bus, DPL2 rear slots and a per-voice remote-speaker table. No AXART function is identified anywhere in the DOL.
- **The mix/mix_rmt split (accepted, PROVISIONAL; unit name unconfirmed).** Retail calls `__MIXResetRmtChannel`
  (from MIXInit, MIXInitChannel, MIXResetControls) and `__MIXUpdateRmtChannel` (from MIXUpdateSettings) out of line,
  and `__MIXUpdateRmtChannel` calls `__MIXGetVolume` out of line 12 times, while every mix.c use of `__MIXGetVolume`
  is inlined. In one unit, the SDK's `-inline auto -ipa file` inlines all of them. Two explanations give identical
  bytes: (a) the lane's separate unit `mix_rmt.c` (no build control needed; `__MIXGetVolume` global); (b) one `mix.c`
  with `#pragma dont_inline on`/`reset` around the three remote functions (`__MIXGetVolume` static). Review probe: (b)
  compiled with `build/tools/cc.exe -u revolution/mix/mix` matches every one of the 19 functions row for row
  (the only difference, two `__MIXSetPan` table relocation rows, is the probe's `...data.0` naming and occurs for the
  lane's own mix.c too). The data and the 16-byte padding at 0x80236714 are the same either way. (a) is kept: it needs
  no pragma, and unlike AXSPB (where ogws's `DECOMP_DONT_INLINE` supports the pragma) no reference supports (b). Open
  item: rename or merge when a Wii MIX source, header or map appears.
- **`lbl_8052AF08` resized 8 to 4 (`__MIXRmtChannel`).** The second word, 0x8052AF0C-0x8052AF10, is `.sbss` alignment
  padding at the end of mix.c's `.sbss`; clean gate green with the resize.
- **Reader-test sample (no [match] finding).** `MIXInit` (tp form plus the remote reset per voice), `MIXInitChannel`
  and `MIXUpdateSettings` (tp's long per-mode volume tables and `if ((*p++ = ...))` writes into the voice's mix block,
  extended to auxC), `MIXSetPan` (tp), `__MIXGetVolume` (tp text, parameter copy included) and
  `MIXRmtSetVolumes` (plain field stores; ORs the caller's mode into the existing mode, as retail) pass. Open ([free],
  not blocking): R33 banners missing on most MIX functions, R35 brace-less `if`s with assignment (tp text), R11 bare
  mixer-control bits in `__MIXUpdateRmtChannel`, R7 tp's `v1` field names.

## zlib rulings (zlib-1)

- **Identity.** 0x801C9624-0x801CCA78 is zlib 1.2.3's inflate side in alphabetical link order: `adler32.c`
  (`adler32`, 1.2.3's `len == 1` fast path), `crc32.c` (`crc32`, `crc32_little`, `crc32_big`; `crc_table`
  `.rodata` 0x803DA948, 0x2000 bytes, BYFOUR), `inffast.c`, `inflate.c` (`inflateReset`, `inflateInit2_`,
  `inflateInit_`, `updatewindow`, `inflate`, `inflateEnd`; `fixedtables` inlined), `inftrees.c` and `zutil.c`
  (`zcalloc`, `zcfree`). Unreferenced functions and data (`inflateSync`, `zError`, `inflate_copyright`...) are
  dead-stripped. Unit names are zlib's. `GetParent` onward is another library (called from game code).
- **No deflate.** The cluster at 0x801B4354 once listed as "zlib deflate side" is not zlib: it is Climax engine
  C++ (an `__sinit` registering a global object with a vtable at 0x80428424, and a stream class whose
  0x801B4464 calls `inflateInit_(strm, "1.2.3", 0x38)`, then `inflate`/`inflateEnd`). No deflate code is linked.
- **Source.** Upstream zlib 1.2.3, unmodified (K&R definitions kept). Public references checked:
  Alice-2010/Decomp (Wii, zlib 1.2.3; its inffast.c/inftrees.c are upstream text and match there; its
  Wii/1.0RC1 `-opt nopeephole -str reuse,readonly -DNO_MEMCPY` profile does not apply here:
  `-opt nopeephole` breaks inflate (2/6 exact), inffast and inftrees; retail calls `memcpy`, so not
  `NO_MEMCPY`; retail keeps duplicate strings, so not `-str reuse`), yannicksuter/mscharged-decomp (zlib 1.2.2,
  GC/3.0a5: older adler32, not used).
- **Flags (`cflags_zlib` = the game's `cflags_game` + `-i include/zlib`).** `-O3` (`-O4,p`: inflate 135 rows,
  inflate_table 1 row; `-O2` and `-O4,s` worse), `-str noreuse` (inflate's two `"unknown compression method"` and
  `"invalid bit length repeat"` copies; dropping it breaks inflate), `-use_lmw_stmw on` (dropping it breaks
  inflate, inffast, inftrees). `-ipa file` and `-fp_contract off` change nothing (no float code). Wii/1.0RC1
  differs from Wii/1.0 on `inflate` (157 rows): 1.0 kept.
- **Data.** Retail's message strings are separate 4-aligned objects (split per string in symbols.txt); the
  function-scope tables take the compiler's names (`@LOCAL@fixedtables__Fe@lenfix`, `@LOCAL@inflate__Fe@order`,
  `@LOCAL@inflate_table__Fe@lbase`...). `config.yml` adds the `lenfix` relocation pair (high half spilled to the
  stack, which dtk does not pair) and blocks `crc_table`'s words as pointers.
- **`crc32.c` and `limits.h`.** zlib's `crc32.c` takes the four-table BYFOUR path only when `<limits.h>`
  defines `UINT_MAX`/`ULONG_MAX`/`USHRT_MAX`; the project's MSL `include/limits.h` carries the standard limits
  (xbret/xenoblade's MSL text, `CHAR_MIN`/`CHAR_MAX` signed), so `crc32`, `crc32_little` and `crc_table` match.

## HBM rulings (sdk-hbm-1)

- **Compiler `Wii/1.0a`.** The HBM banner `Mar 3 2009 (0x4201_142)` is CodeWarrior 4.2 build 142. `mwcceppc -version`
  reports 4.2 build 142 for `tools/compilers/Wii/1.0a` and also for `Wii/0x4201_127` (a different binary despite the
  directory name), 4.2 build 140 for `1.0RC1`, 4.3 build 145 for `1.0`. xbret/xenoblade builds this same drop
  (JP, `sdk_hbm_version = 20090303`) with `Wii/1.0a`. flagsweep on the four ut units: `0x4201_127`, `1.0RC1`,
  `1.0a`, `1.0`, `1.1` all exact; `GC/3.0a5.2` fails (`ut_list` 2/5, `ut_LinkList` 2/3, `ut_CharStrmReader` 0/4).
  The choice rests on the banner; separating `1.0a` from the `0x4201_127` binary needs a larger unit (open).
- **Flags (`cflags_hbm`, `HBMLib` in configure.py).** Base + `-ipa file -fp_contract off -sdata 0 -sdata2 0
  -RTTI on -i include/revolution/hbm`. `-ipa file` is required: without it `ut_LinkList` is 1/3 (the destructor's
  inlined `Clear`/`Erase` chain). `-O4,p` is required: `-O4,s` gives `ut_LinkList` 1/3 and `ut_list` 3/5, `-O3`
  breaks every unit. `-fp_contract off`, `-sdata 0`/`-sdata2 0` (HBM has no small-data sections) and `-RTTI on` are
  xenoblade's and untested here (no float code, data or vtables in the matched units): PROVISIONAL.
  Under `1.0a` the base `-multibyte` prints a deprecation warning; harmless.
- **Units and names.** Unit boundaries from xenoblade's JP split (docs/library_map.md, "HBM units"); file and
  header names from ThePlayerRolo/sdk_2009-12-11, whose headers carry the original member names from a debug
  library's DWARF (`ut_list.o`...). Headers live in `include/revolution/hbm/nw4hbm/ut/` (`list.h`, `LinkList.h`,
  `inlines.h`, `binaryFileFormat.h`, `CharStrmReader.h`), included as `"nw4hbm/ut/..."`. Function names are
  the nw4hbm mangled names, renamed in symbols.txt.
- **Dead-stripped functions.** Only functions present in retail are defined, plus the inline-only helpers they
  expand (`SetFirstObject`, `LinkListImpl::Erase(Iterator, Iterator)`, `Erase(LinkListNode*)`, `Clear`, which the
  linker strips like the original). `List_Prepend`, `List_Insert`, `List_GetPrev` are declared, not defined
  (no retail bytes); `IsReverseEndianBinaryFile`, `GetNextBinaryBlockHeader` and `LinkListImpl::Reverse` are
  omitted (xenoblade's `return false` bodies for the first two are stubs, not source).
- **`ObjectToLink_`** (ut_list) adds the list's run-time `offset` to an object pointer: the intrusive-list design
  (as `GetLink` in `revolution/mem/mem_list.c`), not a fixed-offset cast (R4).
- **Flip order.** `ut_binaryFileFormat`'s `.text` ends on 16-byte padding (0x803CC304-0x803CC310), accepted only
  while `ut_CharStrmReader` (the next unit) is linked from source; flipped alone the gate is red.
- **Open items.** Sound units: C (`mix.c`, xenoblade) or C++ (`sound/mix.cpp`, sdk_2009) - settled as C++ with
  C-linkage exports in "HBM rulings (sdk-hbm-16)". Five small dtk pieces
  with no xenoblade counterpart (0x803B89A0, 0x803C1F24, 0x803C8624, 0x803C9AD4, 0x803CBFD4) and
  `Pane::GetColorElement`/`SetColorElement` sized 0x20 + 0x4 here against 0x24 + 0x24 in xenoblade.


## HBM rulings (sdk-hbm-2)

- **Flags confirmed.** `-fp_contract off`: math_triangular has `a + r * delta` in all three functions; flagsweep
  `-fp_contract on` gives 10 differing rows (`fmadds`) under every Wii version, `off` is exact under `0x4201_127`,
  `1.0RC1`, `1.0a`, `1.0`, `1.1` (`GC/3.0a5.2` fails either way). `-sdata2 0`: retail loads float constants with
  `lis`/`lfs @l` from `.rodata`; `-sdata2 8` breaks `DrawInfo::DrawInfo` (9 rows, `sda21`). `-sdata 0`: with
  `-sdata 8` the 8-byte `__RTTI__Q36nw4hbm3lyt8DrawInfo` moves to `.sdata`; retail has it in `.data`. `-RTTI on`:
  retail vtables start with a pointer to the class's `__RTTI__` object, followed by the type-name string
  (`"nw4hbm::lyt::DrawInfo"` at 0x8045475C); `-RTTI off` gives a bare 0xC vtable. The four HBM-sdk-1 ut units
  plus these five still cannot separate `Wii/1.0a` from the `0x4201_127` binary or `1.0RC1`/`1.0`/`1.1` (open).
- **RTTI objects and weak dedupe.** `__RTTI__` objects are weak (MWCC). A class's RTTI object and name string are
  kept from the first unit in link order that emits them: `ResourceAccessor`'s sit in `lyt_arcResourceAccessor`'s
  `.data` (0x80454640/0x80454660), so `lyt_resourceAccessor`'s retail `.data` is only its vtable (0x18). The source
  unit still emits its own copies; MWLD drops them as the original link did (gate green with the asm
  `lyt_arcResourceAccessor`, its `__RTTI__` named `scope:weak` in symbols.txt). objdiff shows the unit's `.data`
  below 100% for these extra objects; the gate decides. `FrameController` is never instantiated: its vtable is
  dead-stripped (GroupAnmController's vtable at 0x804531F0 carries its `calc`), and the source unit's copy is
  stripped the same way.
- **Literal pools.** dtk lumped float literals and PTMF constants into one object (`lbl_803DEDAC` 0x24,
  `lbl_80455C50` 0x30); split in symbols.txt into the compiler's 4-byte and 0xC objects (`lbl_` names kept) so
  that relocation targets compare. A unit's trailing `.rodata` pad (0x803DEDCC, 0x803DE924) is not a symbol.
  HBMFrameController's `.rodata` shows 66.7% on 8 bytes in objdiff: retail's `lbl_803DE920` is `1.0f, 0.0f` (dtk's 8-byte
  float object); only the `1.0f` (`getLastFrame`) is loaded, nothing reads 0x803DE924, and the source emits just the 4-byte
  `@120`. The linker's alignment pad before the next unit (0x803DE928, `.balign 8`) supplies the four zero bytes, equal to
  `0.0f`, so the gate is green; objdiff compares section sizes (8 vs 4) and cannot see the pad.
- **Dead-stripped `AtanFIdx` defined.** math_triangular's `.rodata` holds 1.0f and -1.0f between `CosFIdx`'s and
  `Atan2FIdx`'s constants, used only by `AtanFIdx` (no retail copy). The function is defined (sdk_2009 body) and
  fixes the pool order; `SinCosFIdx` (no constants of its own, no evidence in retail) is declared only, per the
  sdk-hbm-1 dead-strip rule.
- **`FAbs` is `__fabsf`.** sdk_2009 writes `asm { fabs }`; the `__fabs` intrinsic adds an `frsp` (double result),
  `__fabsf` gives retail's single `fabs` without an asm body.
- **Names.** `FrameController::eAnmType_Alternate` from sdk_2009's DWARF-derived header (xenoblade:
  `eAnmType_Oscillate`). `PSMTXIdentity` (0x8021EA70, the paired-single identity) named in symbols.txt and
  declared in `revolution/mtx.h`.
- **Open.** The five small dtk pieces of sdk-hbm-1 remain; 0x803C8624 (before `lyt_resourceAccessor`) and
  0x803CBFD4 (before `math_triangular`) belong to the preceding asm units.

## sdk-wpad-3 rulings (WPAD, WPADHIDParser, WPADEncrypt, WUD, WUDHidHost)

All five units are `Matching` (each flipped with `build/tools/unit.exe flip`, clean gate `main.dol: OK`). Reference:
yannicksuter/mscharged-decomp, its `R4QK01` sources (Korean Mario Strikers Charged, the 2009 SDK drop, `Wii/1.0`,
all `Matching` there; `WPADEncrypt.c` is version-independent), adapted to this project's headers and names.

- **`WPADHIDParser.c` takes `-fp_contract off` (per object).** Without it only `__parse_dpd_data` differs: `fmsubs`/
  `fmadds` where retail has `fmuls` + `fsubs`/`fadds`. Same per-object flag as AXVPB, KPAD and OSFatal. The
  reference's `-fp off` for WPAD.c is not needed here: the `WPADCommand` GPR-pair copies come out of the SDK profile.
- **`WPAD_CESUCCESS (WPAD_ERR_OK + 0)` and `WPAD_CETRANSFER (WPAD_ERR_TRANSFER + 0)` (wpad.h).** Spelled as in the
  reference header. The `+ 0` makes the macros plain `int` expressions; with bare enumerators `(err == 0) ? A : B`
  compiles to a branch instead of retail's branchless form. Recorded as the reference's spelling, not a lever.
- **`WPADControlBlock` is aligned on the type** (`__attribute__((aligned(32)))` on the typedef): retail `_wpd` is
  32-byte aligned; the same attribute on the variable changes the loop unrolling of `WPADiInitSub`.
- **WPAD.c split starts at 0x8027D5F0** (`noAlloc`, `noFree`, `WPADIsEnabledVSM/TRN/GTR/DRM`, the weak
  `WBCSetupCalibration`, `WBCGetCalibrationStatus`, `WBCGetBatteryLevel`, then the WBC dummies at 0x8027D680), in the
  reference's order; the auto asm region before it ends at 0x8027D5EC.
- **`block_relocations` .data:0x80448084.** WPADEncrypt `tb[0xA4..0xA7]` (bytes `80 1D 0E B0`) is table data that
  dtk took for a pointer into the game's text.
- **WUD.c dead functions.** The source keeps the reference functions that have no retail copy (dead-stripped by the
  linker or inlined; e.g. `WUDStartSyncDevice`, `WUDStartClearDevice`, `WUDRegisterDevice`), each marked
  `// no copy in retail (dead-stripped or inlined)`, under the dead-stripped functions ruling (AGENTS.md). The retail
  copy at 0x8028DDA0 is `StartSyncSimple(TRUE)`, which R4QK01's `symbols.txt` names `WUDStartFastSyncSimple` (its
  caller at 0x8027F680 is `WPADStartFastSimpleSync`); both were renamed from `WUDStartSyncSimple`/`WPADStartSimpleSync`,
  which have no retail copy and keep the reference bodies (`StartSyncSimple(FALSE)`).
- **Renames outside the lane's units, from call-site evidence:** MSL `atan` (0x801A557C) and `tan` (0x801A5DFC)
  (fdlibm sizes, alphabetical MSL math order, called by `getDevConfig`; they had to be named for the link);
  BTE callees named at their WUD/WPAD call sites (BTA_*, BTM_*, `GKI_getbuf`, `L2CA_SetTraceLevel`,
  `SDP_SetTraceLevel`, `btm_remove_acl`, `bta_sys_set_trace_level`).
- **Names now confirmed:** `WPADiClearMemBlock` and `WPADWriteExtReg` (recorded as invented in sdk-wpad-2) are the
  names in the R4QK01 `WPADMem.c`.
- **Open items.** `WPADResetAutoSleepTimeCount` (WPAD.c; stamps the channel's last-input time) has no reference name. The WUD
  functions without a retail copy carry R4QK01 names that this binary cannot confirm. `WPADVSStatus` keeps the
  reference's placeholder member names.
- **KPAD closed from the R4QK01 `KPAD.c`.** `calc_obj_horizon` (inlined into `read_kpad_dpd`) keeps the square root
  and its reciprocal in two locals and stores `sec_nrm` in separate statements (fixes the FPR swap); `KPADInitEx`
  walks the channels with `do { ... } while (++i < WPAD_MAX_CONTROLLERS)` (fixes the order of the hoisted callback
  addresses). Both are the reference's spellings; the unit is `Matching`.

## HBM rulings (sdk-hbm-3)

- **lyt_arcResourceAccessor (Matching).** From xenoblade's body with sdk_2009 names. `FindNameResource` and
  `GetResourceSub` are file-local helpers in an anonymous namespace (`scope:local` in symbols.txt); `detail::FindFont` (iterating with the reference's `NW4HBM_RANGE_FOR`)
  is a `static` inline. The unit's `.data` runs to 0x80454668: retail places `ResourceAccessor`'s name string and weak
  `__RTTI__` (0x80454640, 0x80454660) here, the first unit in link order that emits them. The inline
  `~ArcResourceAccessor` is compiled here too, but retail keeps the earlier weak copy (0x803B9030, now
  `__dt__Q36nw4hbm3lyt19ArcResourceAccessorFv`); ours is dropped at link. `Detach`, `RegistFont` and `UnregistFont`
  have no copy in retail and are declared only (dead-stripped functions ruling). `stricmp` (0x801A6138, MSL, unsplit)
  is named and declared in `include/extras.h`. `ut::LinkList<T, I>` gained its iterator and `PushBack`/`Erase`
  members.
- **HBMAnmController (Matching).** xenoblade's bodies. New headers `HBMAnmController.h`, `nw4hbm/lyt/pane.h`,
  `group.h`, `animation.h`, `types.h`, `common.h`, `nw4hbm/ut/Color.h`. Vtables checked against retail by hand
  and by the R13 checker, which matches the named vtables of `Pane`, `Group` and `AnimTransformBasic` with no slot
  finding (`AnimTransform` and `detail::PaneBase` have no vtable of their own): `Pane` has 23 slots (the Bounding vtable 0x80454668 overrides
  only `DrawSelf`; `SetAnimationEnable` is slot 20, offset 0x58 in `do_calc`), `AnimTransform` 6 (from
  `AnimTransformBasic` 0x80454568; no `AnimTransform` vtable of its own in retail), `Group` 1 (0x80454780).
  `res::Pane`/`res::Group` and the resource constructors are omitted until a unit needs them.
- **FrameController's name string and weak `__RTTI__` belong to `HBMFrameController`.** The retail pair at 0x804531C8-0x804531F0 is the copy MWLD keeps from the first unit in link order that emits it, `HBMFrameController` (whose own vtable and `__dt__` are dead-stripped). The split gave it to `HBMAnmController`, whose own copy is the duplicate the link drops, so `gatecheck layout` saw the pair out of order. `splits.txt` now gives that `.data` range to `HBMFrameController` and `HBMAnmController` starts at its vtable (0x804531F0); layout matches for both units.

## HBM rulings (sdk-hbm-4)

- **lyt_bounding (Matching).** sdk_2009's bodies: xenoblade's `Bounding::DrawSelf` is empty, but retail keeps the
  debug outline (`IsDebugDrawMode`, `LoadMtx`, `detail::DrawLine(GetVtxPos(), mSize, 0x00FF00FF)`). The unit also
  emits the weak `ut::Color::~Color` (0x803C1EC0, the first copy in link order; later units pass it to the array
  constructors) and `__sinit_\lyt_bounding_cpp` (`Bounding::typeInfo`). Its `.data` runs to 0x80454750: after
  Bounding's vtable, name string and `__RTTI__`, retail places the name strings, base lists and weak `__RTTI__` of
  `Pane` and `detail::PaneBase`, which Bounding's RTTI is the first in link order to emit (`unit.exe new` stopped at
  0x80454700; the range was extended by hand). `.bss` 0x80525AD8-0x80525AE0: `typeInfo` is 4 bytes (symbols.txt
  size corrected from dtk's 8; the rest is the 8-byte alignment gap). New headers `nw4hbm/lyt/bounding.h`,
  `nw4hbm/ut/RuntimeTypeInfo.h`; `pane.h` gained `res::Pane` and `Pane(const res::Pane*)`, `common.h`
  `res::DataBlockHeader` and `detail::DrawLine`. The 20 inherited Pane slots of the Bounding vtable were named in
  symbols.txt from the slot order (`CalculateMtx` 0x803C7670 ... `LoadMtx` 0x803C7EC0), so the vtable's relocations
  compare by name. The checker's R25 note on the `Bounding::typeInfo` definition is a false positive (a static
  member definition, not a prototype). Likewise R35 on `Group::AppendPane`'s `if (void* pMem = ...)`: a condition declaration, not an
  assignment inside a condition.
- **lyt_group (Matching).** sdk_2009's bodies (same as xenoblade), with its `NW4HBM_RANGE_FOR_NO_AUTO_INC` macro
  (added to `ut/LinkList.h` with `Iterator::operator++(int)`) and `(int)NAME_LENGTH * i` kept from the reference.
  `Group::Init` and `Group::AppendPane` are out-of-line members whose bodies retail inlines; their own copies are
  dead-stripped. `Group()` has no copy in retail and is declared only. New `nw4hbm/lyt/layout.h` declares only
  `Layout`'s allocator statics (`mspAllocator` 0x80525B08, `AllocMemory`/`FreeMemory`); the Layout unit must add the
  class's real vtable and members there. `detail::EqualsPaneName` (0x803C1F30, lyt_common) and
  `detail::ConvertOffsToPtr` are declared in `common.h`/`types.h`.
- **Shared headers touched.** `include/new` (placement `operator new`/`delete`, MSL style) is new and the single
  definition: `climax/Memory.h` includes `<new>` instead of defining its own inline placement new (no function in any
  unit changes). `revolution/hardware.h` puts its absolute-address register arrays
  under `#ifndef __cplusplus`: C++ cannot declare `vu32 x[] : addr`, and the HBM reaches the header through
  `mem/allocator.h` -> `os.h` -> `vi.h`. C units are unchanged.
- **R13.** `classes.exe scan` finds the namespaced nw4hbm vtables and the rule checker's R13 slot check runs on them:
  the Bounding (25 entries: RTTI header and 23 Pane slots, overriding the dtor, `GetRuntimeTypeInfo` and `DrawSelf`)
  and Group (dtor only) vtables match their source classes with no finding, and compare at 100% with named
  relocations.

## sdk-bte-1 rulings (BTE map, bte_init, bd, utl)

- **BTE names and units from mscharged-decomp R4QK01.** All 731 functions of 0x8024A790-0x80277F60 align with the
  reference's BTE functions by size, order and padding, so `symbols.txt` takes the reference's names (692 renamed;
  the 39 already named agreed). Unit table in docs/library_map.md, "BTE units". The reference marks 12 of them
  `scope:local` (`uusb_*CB`, `BTUInterruptHandler`, `gap_*_ind`/`_cfm`); `symbols.txt` keeps them global until
  their units are split.
- **Compilers** (docs/toolchain.md profile row, `BTELib`): `GC/3.0a5` for the stack, `Wii/1.0` per object for the glue.
  bte_init, bd and utl compile identically under both; their compiler follows the unit's alignment (`bte_init`
  starts a 16-byte-aligned unit between Wii units, `bd` and `utl` start on 4-byte boundaries) and sdk_2009.
- **`bd_addr_null`** is `.sbss2:0x805300A8` (dtk had `lbl_805300A8`, size 8; read by `bta_hh_find_cb`), owned by
  `bd.c` (`.sbss2` split 0x805300A8-0x805300B0), as in R4QK01. `bd_addr_any`, `bdcmpany`, `bdsetany` and the
  utl string helpers and `utl_set_device_class` have no retail copy and keep the reference bodies (dead-stripped
  functions ruling). `bta_dm_pm_cfg` (3 bytes, before it) is owned by `bta_dm_cfg.c` (sdk-bte-6 rulings).
- **Headers.** `include/revolution/bte/` mirrors the Broadcom header names (subsets so far); the base types,
  `BD_ADDR` family, `BT_HDR`, `tBTM_STATUS` and `GKI_getbuf` moved there from `revolution/bte.h`, which includes
  them. A named parameter in a new prototype renumbers the compiler's `@N` labels in every unit including
  `bte.h` (object files differ, DOL bytes do not); parameters are named as in the reference.
- **Sources** carry the Broadcom copyright and Apache-2.0 notice and the reference's "Original source" line; each
  file's `// Reference:` line names the licence.
- **Open: WUD device states.** `WUDDevInfo.status` (0, 1, 2, 3, 8, 9, 12) and `sync_type` (0 to 5) stay bare
  numbers in WUD.c and WUDHidHost.c: no reference or header names them (they are not BTA constants), and naming
  them would be invented.

## sdk-bte-2 rulings (bte_hcisu, wbt_ext, btm_main, btu_init, port_api)

- Five units from the R4QK01 reference, each source unchanged in substance (tabs to 4 spaces, includes as
  `<revolution/bte/...>`, the `bd.c` header format). `bte_hcisu.c` (`Wii/1.0`, glue) and `wbt_ext.c` (`GC/3.0a5`) are
  Matching (clean gate green). `btm_main.c`, `btu_init.c` and `port_api.c` (`GC/3.0a5`) are 100% and green in the
  gate when linked; they were MATCH-PENDING on the checker's R4 offset-macro pattern until the checker fix (see
  "sdk-bte-3 rulings") and are now Matching.
- Open item (checker, R4): `BTM_CLOCK_OFFSET_VALID`, `RFCOMM_MIN_OFFSET`, `RFCOMM_PF_OFFSET`, `L2CAP_MIN_OFFSET` and
  `L2CAP_SEND_CMD_OFFSET` are Broadcom protocol constants (a flag bit, frame field positions, payload headroom in a
  `BT_HDR` buffer), not struct-offset macros. They are kept in the reference's exact form; renaming or wrapping them
  to dodge the pattern would be distortion. The checker needs a way to admit a header macro (its Review rulings
  match functions only); the three units flip once it has one. Every later BTE stack unit reaches these headers.
  Resolved: the checker no longer reads these protocol constants as offset macros (main 69bd643).
- Data: `btm_cb` (`.bss` 0x804E7A58, 0x27C4) is owned by `btm_main.c`; `BT_BD_ANY` (`.sdata2` 0x8052EDA8, 6 bytes)
  by `btu_init.c`; `p_hcisu_if` (0x8052B038) and `p_hcisu_cfg` (0x8052B03C) (`.sbss`, reference declaration order,
  `gatecheck layout` matches) by `bte_hcisu.c`. `btu_cb` (0x804E7530, 0x84, owner `btu_task1.c`) and `rfc_cb`
  (0x804EB308, 0x418, owner not yet split) are named in `symbols.txt` and declared `extern` only. The old symbol
  sizes of `btm_cb` and `btu_cb` included alignment padding. All are global in R4QK01.
- Dead-stripped, reference body kept (`// no copy in retail`): `WBT_ExtAddPinCode`, `wbt_sdp_show_ext`,
  `BTU_AclPktSize`.
- Headers: new `bt_target.h`, `gki_target.h`, `bt_trace.h`, `hcidefs.h`, `hci.h`, `btu.h`, `btm_int.h`, `l2c_api.h`,
  `l2c_int.h`, `rfcdefs.h`, `rfc_int.h`, `port_int.h`, `profiles_api.h`, `sdp_api.h`, `sdp_int.h`, `sdpdefs.h`,
  `wbt_api.h`; `bt_types.h`, `gki.h`, `btm_api.h` and `port_api.h` grown to the reference's full content. The
  reference's `btm_int.h` declares about 78 functions twice (an old and a newer block); one declaration of each is
  kept. The BTM block of `revolution/bte.h` moved to `btm_api.h`/`hcidefs.h`; `bte.h` keeps `btm_remove_acl` (as the
  reference's public header does; `btm_int.h` includes `bte.h` instead of declaring it again) and now carries the
  `bte_hcisu` externs and prototypes. Review: the reference's old-and-newer duplicate prototypes (`sdp_api.h`,
  `btu.h`, `rfc_int.h`, and `SDP_SetTraceLevel` repeated in `bte.h`, which now includes `sdp_api.h`; `L2CA_SetTraceLevel` stays in
  `bte.h` only, since including `l2c_api.h` there would expose `L2CAP_MIN_OFFSET` to every WPAD/WUD unit) are reduced
  to one declaration each.
- `wbt_ext.c` writes `sizeof temp_u16` where the reference writes `ARRAY_LENGTH(temp_u16)`. That macro is the
  mscharged-decomp tooling header `include/decomp.h`'s (next to `ASM`, `DECOMP_FORCEACTIVE`, `CLAMP`), not Broadcom or
  SDK source, so it is no evidence of the original spelling; the arrays are `UINT8`, so the value is the same. `wbt_ext.c` and `bte_hcisu.c` have no "Original source" block:
  the reference names none. `bte_hcisu_task`'s `param_1` is the reference's name (sdk_2009-12-11 has no better one).

## HBM rulings (sdk-hbm-5)

- **lyt_pane (Matching).** sdk_2009's bodies (retail's `Pane` layout is that drop's: `mName[16]` at 0xB4, no
  `ExtUserDataList`, one `GetMaterial` slot; xenoblade's `Pane` is a later drop). `SetName`, `SetUserData`, `Init`,
  `InsertChild(Iterator, Pane*)`, `CalculateMtxChild` and the `PaneBase` constructor and destructor are kept as
  out-of-line bodies marked `// no copy in retail (inlined)`; `Pane()`, `PrependChild`, `InsertChild(Pane*, Pane*)`
  and `RemoveChild` have no copy and no inline use here, so they are declared only. The two `.rodata` float pools were
  split in symbols.txt into the compiler's objects (`lbl_803DED50`..`lbl_803DED60` and `lbl_803DED68`/`6C`;
  `lbl_803DED6C` is `CalculateMtx`'s function-local `const f32 invAlpha`). `.data` is the vtable only; the weak
  `__RTTI__` objects deduplicate against lyt_bounding's (gatecheck layout reports them as DEDUP).
- **Named from the call sites.** `detail::EqualsMaterialName` (0x803C1F60) and `detail::FindAnimationLink`
  (0x803C19D0) from the sdk_2009 declarations; the MTX library entries `PSMTXCopy` (0x8021EAA0), `PSMTXConcat`
  (0x8021EAE0), `PSMTXRotRad` (0x8021ECB0), `PSMTXTransApply` (0x8021EE20) and `PSMTXScale` (0x8021EE70) from the
  order and use in `CalculateMtx`/`LoadMtx`. `mtx.h` declares them with the SDK's `MTX*` macros.
- **Headers.** New `nw4hbm/lyt/material.h` (the `Material` interface Pane uses; members as sdk_2009, no GX
  accessors yet) and `nw4hbm/math/constants.h`; `LinkList::GetIteratorFromPointer(T*)` added; `Pane` gained the
  `BIT_*` flag enum and `IsInfluencedAlpha`/`IsLocationAdjust`. R13 by hand: the Pane vtable (25 entries) compares at
  100% with named relocations. R35 on `if (Pane* p = ...)` and R17 on `CalculateMtx`'s two scope blocks (lexical blocks
  in the SDK's debug info) are kept from the reference.
- **lyt_picture (Matching).** sdk_2009's bodies (xenoblade's `Picture` takes a `TexMap` and is a later drop; retail
  has `Append(TPLPalette*)` and `Append(const GXTexObj&)`). Retail's copies are the `res::Picture` constructor,
  destructor, both `Append`s, the four vertex-colour virtuals, `DrawSelf`, `GetRuntimeTypeInfo` and the `__sinit`. The
  other three constructors, `GetTexCoordNum`, `GetTexCoord` and `SetTexCoord` have no copy and no inline use, so they
  are declared only; `Init`, `ReserveTexCoord` and `SetTexCoordNum` are inlined into the constructor and `Append` and
  keep their bodies marked `// no copy in retail (inlined)`. The `.rodata` object `lbl_803DED70` (8 bytes) is
  `Append`'s `Size(0.0f, 0.0f)` constant plus 4 bytes of alignment padding (`gatecheck layout` reports PADSIZE);
  `.data` is the vtable, the name string and base list, and the weak `__RTTI__` (the `Pane` and `PaneBase` ones
  deduplicate against lyt_bounding's, DEDUP in `gatecheck layout`).
- **ut::Color alignment.** Retail's `Picture` members start at 0xD4, while `Pane` ends with the one-byte
  `mbUserAllocated` at 0xD0 (data size 0xD1). The cause is `ut::Color`, which is 4-byte aligned in the SDK (sdk_2009
  declares `struct Color : public GXColor { ... } alignas(4);`, noting that MWCC honours the attribute only in that
  position, so `Color.h` spells it `ATTRIBUTE_ALIGN(4)` (`__attribute__((aligned))`) after the closing brace). `Picture::mVtxColors` is
  the first `Picture` member and aligns to 4, which puts it at 0xD4; `Pane` has no member at 0xD1 (sdk_2009's
  DWARF-derived `pane.h` shows only "3 bytes padding"). This replaces an earlier `mPadding[3]` in `Pane`; every
  lyt_pane/lyt_picture byte and data score is unchanged.
- **Named from the call sites** (symbols.txt): `TexCoordAry::TexCoordAry`, `Free`, `Reserve`, `SetSize`, `Copy`
  (0x803C2000..0x803C2200), `detail::IsModulateVertexColor`, `SetVertexFormat`, `DrawQuad`, `InitGXTexObjFromTPL`,
  `ut::Color::Color()` (weak, 0x803C41A0), `Material::Material(const res::Material*, const ResBlockSet&)`,
  `GetTexCoordGenAry`, `SetTextureNum`, `SetTexCoordGenNum`, `SetTexture(u8, const GXTexObj&)` and
  `detail::GetTextureSize`, from the sdk_2009 declarations and the argument registers; each mangled name is the
  compiler's own for the declaration in the headers (relocation targets compared at 100%).
- **Headers.** New `nw4hbm/lyt/picture.h` and SDK `revolution/tpl.h` (types only; the TPL functions are not
  declared until a unit calls them); `common.h` gained `TexCoordAry` and the draw helpers, `layout.h` the `res`
  block lists and `ResBlockSet`, `material.h` `res::Material`, `TexCoordGen` and the texture accessors, `inlines.h`
  `ut::Min`. R13 by hand: the Picture vtable (25 slots after the RTTI pair) compares at 100% with named
  relocations. Checker: R25 on `Picture::typeInfo` (a static member definition, as for Bounding) and R35 on
  `if (Material* p = ...)` (a C++ condition declaration; fixed in the main checker by 09593ae) are false positives.

## HBM rulings (sdk-hbm-6)

- **lyt_material (Matching).** sdk_2009's bodies (xenoblade's `Material` takes `TexMap` objects and is a later
  drop). Retail keeps 23 functions: the two local helpers `CalcTextureMtx` and `SetIndTexMtx`, the weak
  `ut::Color::Color()`, the `res::Material` constructor, the destructor, `ReserveGXMem`, the three non-const
  accessors `GetTexSRTAry`, `GetTexCoordGenAry` and `GetIndTexSRTAry`, `SetTextureNum`, `SetTexCoordGenNum`, both
  `SetTexture`s, `SetTextureNoWrap`, `SetColorElement`, `SetupGX`, the animation members and `detail::GetTextureSize`.
- **Dead-stripped bodies** (ruling "Dead-stripped functions"). Functions with no copy and no inline use in retail are
  declared only (`GetColorComponentValue`, the const accessors other than `GetTexMapAry`, `GetColorElement`, the
  `GXTexObj` `SetTextureNoWrap`). Bodies retail inlines keep their source, marked `// no copy in retail (inlined)`:
  the local helpers (`GXColorS10` `==`/`!=`, `CalcIndTexMtx`, `SetColorComponentValue`, `InitTexSRT`, the
  `CalcOffset*Ary` chain, `CopyGXTexObj`), `Init`, `InitBitGXNums`, the other non-const `Get*Ary` accessors,
  `SetName`, `SetTevStageNum`, `SetIndStageNum`, `SetTexture(u8, TPLPalette*)`, and `GetTexture` with the const
  `GetTexMapAry` (both inlined into `detail::GetTextureSize`). The default `Material::Material()` has no copy
  but is kept: compiled, it is the first function to need the weak `ut::Color::Color()` (for its `__construct_array`
  calls), which puts that constructor at 0x803C41A0, ahead of the `res::Material` constructor, as in retail. Without
  it the weak constructor follows the `res::Material` constructor and the gate fails (`gatecheck attribute`: the whole
  constructor moves by 0x10).
- **`CalcOffsetTexMapAry`** is restored as the first link of the offset chain (a function returning 0; sdk_2009
  spells it as a `0ul` macro). Byte-neutral either way; operand order inside the chain is also neutral.
- **`ConvertOffsToPtr` adds on the integer address** (`reinterpret_cast<unsigned int>(pBase) + offset`, with an
  `unsigned int` offset), as sdk_2009 and mscharged-decomp spell it; the earlier `u8*` arithmetic left the constructor
  at 99.85%. Mechanism (traced with a cut-down probe and `#pragma opt_common_subs off`): the constructor reads
  `GetTexCoordGenAry()` into a local and then inlines `SetTexCoordGenNum`, which reads it again. With `u8*` arithmetic
  the IR common-subexpression pass merges the two reads into a temporary for `mpGXMem + (texMap * 32 + texSRT * 20)`,
  which keeps that grouping; with integer arithmetic it does not, the reads are merged later by propagation, and the
  sum is regrouped as `(mpGXMem + texSRT * 20) + texMap * 32`, retail's shape. lyt_group and lyt_picture (the other
  users) are unchanged.
- **`T* const` locals**, as xenoblade spells them, hold the `Get*Ary()` results across the unit (sdk_2009's plain
  `T*` drops three functions), except in the constructor: its `texCoordGens` is a plain `TexCoordGen*`, as in
  both references (with `const` there, the pointer and the copy loop's counter swap r3 and r4), and its `texSRTs` is
  a plain `TexSRT*`, as xenoblade spells it there (byte-neutral either way).
- **Symbols.** The `.rodata` pools at 0x803DED20 (0x14) and 0x803DED3C (0xC) are split into the compiler's objects
  (two 8-byte 2-byte-element tables and the float constants); the `Material` vtable is `__vt__Q36nw4hbm3lyt8Material`
  and its RTTI object `__RTTI__Q36nw4hbm3lyt8Material` (weak, owned here). `fn_803C2310` in lyt_common is
  `detail::MultipleAlpha(ut::Color, u8)` (sdk_2009 declaration; `SetupGX` calls it with the material colour and the
  alpha). `gatecheck layout` matches.

## HBM rulings (sdk-hbm-7)

- **lyt_window.** sdk_2009's bodies (xenoblade's `Window` checks `GetTextureNum()` before each frame
  part, has `InitContent`/`InitFrame` and `GetMaterialNum`/`GetMaterial(u32)`, and draws the eight-part frame with
  negated y offsets: a later drop that retail does not follow). 20 of 21 functions matched and `DrawFrame` sat at
  92.5%, a register/schedule residue in its first (LT) quad only (retail numbers `-1`, `coords[0][ix]` and
  `coords[0][iy]` r0/r5/r11 where the build used r5/r9/r0); calls, CFG, stack layout and the three later quads
  matched (resolved by sdk-hbm-17, below). Tried without
  effect: `do { } while (0)` macro bodies, `texCds[0]` for `*texCds`, a literal `1` for `texCoordNum`, `const`/plain
  `texSize` and a `const Size&` binding, `const bool bUseVtxCol`, every order of the four locals after `texSize`,
  `vtxColors` before `texSize` or at the top, `SetVertexFormat` after `GetTextureSize`, non-const `tSz` and a
  non-const `GetTexutreFlipInfo` result, and compilers Wii/1.0 to 1.7 and GC 3.0a3 to 3.0a5.2 (`flagsweep`).
  Traced with `wdbg.exe` (Wii/1.0a: `assign_register_to_variable` 0x5b17c0, `pickcolor` caller 0x5b43d1/0x5b43d6,
  node base `[0x752f90]`): the residue is the numbering of the LT quad's inlined `ix`/`iy` objects. In the build
  LT's `ix` gets the higher index (coloured first: `ix` r6, `iy` r7), in RT, RB and LB the lower (`ix` r8, `iy` r7);
  retail's LT follows the other quads (`ix` r9, `iy` r8), which also moves `-1` into r0. The declaration order of
  `ix`/`iy` in `GetLTTexCoord` does not change the DrawFrame numbering (the objects are not the declared locals'
  order). Also tried without effect on DrawFrame: `ut::Color()` as `operator=(0xffffffff)` (sdk_2009) or
  `ToU32ref() = WHITE`, `const` `vtxColors` (mscharged), the helpers forward-declared, defined after the
  `Window` members, or `GetTexutreFlipInfo` defined last, `TextureFlipInfo` at file scope (sdk_2009), `int` flip
  parameters, a `const` `flipInfos` table, `(diff) * tSz[ix]` operand order, `#pragma schedule once`.
  `GetLTTexCoord` and `GetLTFrameSize` are pinned by `DrawFrame4`/`DrawFrame8` (any body change there breaks them):
  `u8` locals for `coords[0][ix]`/`coords[0][iy]` in `GetLTTexCoord` give DrawFrame 99.87% (one r10/r11 swap) but
  drop DrawFrame4/8 to 98%/96%; a trial that copied mscharged's whole reference function instead of only its LT
  block measured 88.7% here; that copy also brought the reference's `ix`-before-`iy` ordering and its other
  bodies, so the figure measured those differences rather than the LT block shape. Permuter not run.
  Resolved by sdk-hbm-17: the LT part is expanded in `DrawFrame` with those two inline helpers (mscharged R4QK01,
  this drop's own source), which gives retail's numbering and takes the unit to 21 of 21.
- **Dead-stripped and inlined.** The unnamed-namespace helpers `GetTexutreFlipInfo` (the SDK's misspelling), the
  `Get{LT,RT,LB,RB}{FrameSize,TexCoord}` pairs and `SetFrameTexOrigin`/`SetFrameTexExtent` have no copy in retail;
  every caller inlines them, so they keep their bodies, marked `// no copy in retail (inlined)`. sdk_2009 declares
  no other `Window` member.
- **Constructor.** `matOffsTbl` is `const u32* const`, as xenoblade spells it (sdk_2009's plain `const u32*` swaps
  it with `pResContent`, r27/r28, 99.2%). sdk_2009's lexical block around the content setup and its assignment
  inside the `mFrames` condition are byte-neutral; the body is written flat, with the assignment before the test.
- **Header.** New `nw4hbm/lyt/window.h`: `InflationLRTB`, `WindowFrameSize`, `res::Window`, `res::WindowFrame`,
  `res::WindowContent` and `Window` with the nested `Content` and `Frame`, offsets as in sdk_2009 (size 0x104).
  `VERTEXCOLOR_MAX` is a class enum, as in `Picture`. The block signature `SIGNATURE_WINDOW_BLOCK` is left out
  (no user in this unit). The local `TextureFlipInfo` stays in the unit's unnamed namespace (one definition, used
  only here).
- **Symbols and data.** The 21 functions take their mangled names; `fn_803CBFC0` is `__sinit_\lyt_window_cpp`
  (stores `&Pane::typeInfo` into `typeInfo__Q36nw4hbm3lyt6Window` at 0x80525B28, `.bss` split 0x80525B28-0x80525B30
  added by hand: `unit.exe new` does not infer it). The 0xC after it (0x803CBFD4) is alignment padding before
  math_triangular. The vtable is `__vt__Q36nw4hbm3lyt6Window` (0x7C, 6 slots after Pane's), the RTTI object
  `__RTTI__Q36nw4hbm3lyt6Window` (weak, owned here); `lbl_80454A50` (0x3C) is the function-local static
  `flipInfos` table, `lbl_80454B08` the type-name string, `lbl_80454B1C` the base list; the three `.rodata` values
  are compiler constants and keep `lbl_`. Pane's and PaneBase's RTTI copies are DEDUP (owned by lyt_bounding); the
  `texCoordNum` constants of the three `DrawFrame`s are DEADSTRIP. `gatecheck layout` matches.
- **Checker notes.** R13: `Window` slots compare clean.

## HBM rulings (sdk-hbm-8)

- **ut_ResFont (Matching), ut_ResFontBase (Matching).** Both units start from sdk_2009's `ut_ResFont.cpp` and
  `ut_ResFontBase.cpp` and match at 100% with named relocations under `cflags_hbm`, no per-object flags; the clean
  gate printed `build/R5WEA4/main.dol: OK` after each flip. ResFontBase was flipped first, so ResFont's `.text`
  (ending on 4 bytes of padding at 0x803CE45C) is followed by a source unit; ResFontBase's `.text` ends at its last
  instruction (0x803CEAA8), before the asm ut_TagProcessorBase.
- **Headers.** The resource blocks (`FontMapMethod`, `FontCodeMap`, `FontTextureGlyph`, `FontWidth`,
  `FontInformation`) join `CharWidths` and `Glyph` in `nw4hbm/ut/Font.h`, as in sdk_2009. `FontInformation` keeps
  sdk_2009's trailing `padding_[1]` (a DWARF member, `byte1_t padding_[1]`), marked `// pad:` like the other SDK
  resource padding; size 0x18 either way, gate and layout unchanged.
  New `nw4hbm/ut/ResFontBase.h` (`detail::ResFontBase`, size 0x18) and `nw4hbm/ut/ResFont.h` (signatures as
  multi-character constants, as `ResourceAccessor` spells them; `FILE_VERSION` 0x0104). `CMapScanEntry` and
  `CMapInfoScan` stay local to ut_ResFontBase (one definition, used only there).
- **Offsets.** `ResolveOffset` and the block step `AddOffset` (unnamed namespace, ut_ResFont) add on the integer
  address, as `lyt::detail::ConvertOffsToPtr` does; `ResolveOffset` writes through
  `*reinterpret_cast<u32*>(&ptr)` as sdk_2009 does (same bytes as a plain assignment; the checker reports no R4
  finding). The FINF/TGLP/CWDH/CMAP payload is `blockHeader + 1`.
- **Dead-stripped bodies** (ruling "Dead-stripped functions"). `ResFontBase::RemoveResourceBuffer` and
  `ResFont::RemoveResource` have no copy and no inline use in retail: declared only. `GetGlyphIndex`,
  `FindGlyphIndex(u16)` and both `GetCharWidthsFromIndex` have no copy but are inlined into `SetAlternateChar`,
  `GetCharWidths`, `GetGlyph` and `GetGlyphFromIndex`: bodies kept, marked `// no copy in retail (inlined)`.
- **Symbols and data.** The 28 functions take their mangled names (align.txt). ut_ResFont owns `.data`
  0x80455C80-0x80455D60 (`unit.exe new` stopped at 0x80455D08; extended by hand): `__vt__Q36nw4hbm2ut7ResFont`, the
  type-name strings and base lists of ResFont, ResFontBase and Font, and the three weak RTTI objects, all first
  emitted here in link order. ut_ResFontBase owns 0x80455D60-0x80455E20: `lbl_80455D60` (the
  `&CharStrmReader::ReadNextCharCP1252` member-pointer constant of the inlined `Font()`; compiler data, keeps
  `lbl_`), `__vt__Q46nw4hbm2ut6detail11ResFontBase` and the weak `__vt__Q36nw4hbm2ut4Font` (pure slots null). Its
  copies of the RTTI objects, strings and base lists are DEDUP. `fn_803C9900` (lyt_window range) is the weak
  `__dt__Q36nw4hbm2ut4FontFv`, first emitted there; ut_ResFontBase's copy is dropped. Strings and base lists keep
  `lbl_`. `gatecheck layout` matches both units (PADSIZE only).
- **Checker notes.** R13 [advisory] reports Font's 19 pure virtuals as added slots of `__vt__Q36nw4hbm2ut4Font`:
  the retail slots are null (pure), which the checker reads as no slot. ResFont and ResFontBase compare clean.
  Tool problem, reported; no source change.

## HBM rulings (sdk-hbm-9)

ut_TagProcessorBase and ut_CharWriter (both Matching).

- **Sources.** Both start from sdk_2009's `ut_TagProcessorBase.cpp` and `ut_CharWriter.cpp`. Every function is 100%
  with named relocations under `cflags_hbm`, no per-object flags. The clean gate printed `build/R5WEA4/main.dol: OK`
  after each flip (`unit.exe flip`).
- **Headers.** New `nw4hbm/ut/TagProcessorBase.h` (`Operation`, `PrintContext<CharT>`, `TagProcessorBase<CharT>`),
  `nw4hbm/ut/CharWriter.h` (nested `ColorMapping`, `VertexColor`, `TextColor`, `TextureFilter`, `LoadingTexture`;
  size 0x4C; sdk_2009's `padding_[2]` at 0x40 kept, marked `// pad:`) and `nw4hbm/ut/TextWriterBase.h` (declarations
  for the asm unit ut_TextWriterBase; its `VSNPrintf`/`StrLen` inlines need the `vsnprintf` and `vswprintf`
  declarations added to `stdio.h` and `wchar.h`). sdk_2009's `char16_t` is spelled `u16` (mangled `Us`). sdk_2009's
  `extern template class` declarations are left out. `Rect::Normalize` joins `nw4hbm/ut/Rect.h`.
- **`FSelect` keeps sdk_2009's `asm { fsel }`.** Wii/1.0a has no `__fsels`; `__fsel` returns double, so each of
  `Normalize`'s four results gains an `frsp` (CalcRect 91.9%). Listed under "Listed inline-asm helpers", marker
  `// ASM-HELPER` at the function (unlike `FAbs`, whose `__fabsf` intrinsic gives retail's single `fabs`).
- **Templates.** The two explicit instantiations (`template class TagProcessorBase<char>;` then `<wchar_t>`) emit
  every member, the vtables and the RTTI as weak symbols; `symbols.txt` marks the 12 functions,
  `__vt__Q36nw4hbm2ut19TagProcessorBase<w>`/`<c>` and `__RTTI__...<w>`/`<c>` `scope:weak`. They are the only retail
  copies (ut_TagProcessorBase owns `.data` 0x80455E20-0x80455EA8, the wchar_t objects first, as the compiler emits
  them); the two type-name strings and the `.rodata` int-to-float constant keep `lbl_`. `.text` ends at its last
  instruction (0x803CF568) before asm ut_TextWriterBase. The callees in ut_TextWriterBase (`GetLineHeight`,
  `GetTabWidth`, both instances) take their mangled names from align.txt.
- **CharWriter inlined helpers.** The file-static `SetupGXCommon` and `ResetColorMapping`, `ResetTextureCache`,
  `SetTextColor(Color)`, `SetScale(f32, f32)`, `EnableLinearFilter`, `SetCursor(f32, f32, f32)`,
  `UpdateVertexColor`, `LoadTexture`, `SetupVertexFormat`, `SetupGXDefault`, `SetupGXForI` and `SetupGXForRGBA` have
  no retail copy but are expanded into the constructor (0x38C), `SetupGX` (0x950), `SetGradationMode`,
  `SetTextColor(Color, Color)`, `PrintGlyph` and `SetupGXWithColorMapping`: bodies kept, marked
  `// no copy in retail (inlined)`. `SetupGXCommon`'s static `fog` and its guard (`.bss` 0x80525B34, 0x80525B30) are
  read by both `SetupGX` and `SetupGXWithColorMapping`.
- **CharWriter dead-stripped (declared only).** `GetColorMappingMin`/`Max`, `SetAlpha`, `GetAlpha`,
  `GetGradationMode`, `GetTextColor`, `GetGradationStartColor`/`EndColor`, `SetScale(f32)`, `SetFontSize(f32)`,
  `GetFontDescent`, `IsLinearFilterEnableAtSmall`/`AtLarge`, `EnableFixedWidth`, `SetFixedWidth`, `DrawGlyph`, both
  `MoveCursor`, `SetCursorZ`, `MoveCursorZ`, `GetCursorZ`.
- **CharWriter data.** `mLoadingTexture__Q36nw4hbm2ut10CharWriter` (`.bss` 0x80525B38, 0x10), defined after
  `SetupGXCommon` as in sdk_2009 (gives the retail `.bss` order); the guard, `fog` and the five `.rodata` literals
  (0x803DEDD0-0x803DEDE8) keep `lbl_` (no dtk number). `0xFFFFFFFF` colours are written `Color::WHITE` (R11) and GX
  arguments by their enum names; byte-neutral. `gatecheck layout` matches both units.
- **R13.** The checker finds no retail vtable for the class template `TagProcessorBase` (advisory: it does not map
  `<c>`/`<w>` instances); the two vtables compare 100% in objdiff. Tool limitation, reported; no source change.
  CharWriter has no vtable.

## HBM rulings (sdk-hbm-10)

ut_TextWriterBase (Matching).

- **Source.** Adapted from sdk_2009's `ut_TextWriterBase.cpp` (xenoblade JP second reference). All 83 functions
  (both the `<c>` and `<w>` instances of every member, including the out-of-line header inlines `VSNPrintf`,
  `StrLen` and `IsDrawFlagSet`, plus `__sinit_\ut_TextWriterBase_cpp`) and the six named static data objects are
  100% with named relocations under `cflags_hbm`, no per-object flags. The clean gate printed
  `build/R5WEA4/main.dol: OK` at the flip (`unit.exe flip`); `gatecheck layout` matches. No header change was needed.
- **Deviations from the reference text.** Definitions are written in retail (declaration) order; `byte4_t` is
  `u32`, `std::va_list` is `va_list`. sdk_2009's `Printf` ends with `va_end(vlist)`, a name the function does not
  declare (the macro discards its argument); written `va_end(vargs)`, byte-neutral. Review ruling: `vlist` is a leftover of the reference's local name (every other
  function there declares `vargs`) and cannot be read as an original quirk, so the corrected spelling stands without an R36 label.
- **`PrintImpl`'s `xCursorAdj` (R36).** The reference assigns `xCursorAdj = orgCursorX - GetCursorX()` and never
  reads it (`ATTR_UNUSED` in sdk_2009). Retail keeps that `GetCursorX` call (dropping the local gives 99.4% in both
  instances), so the dead store is the original's: kept with an `// R36:` note, which the checker accepts. The
  reference's `{ }` block around `pFont`/`adj` stays (R17 advisory only).
- **Templates and data.** The explicit instantiations `template class TextWriterBase<char>;` then `<wchar_t>` emit
  every member weak; `symbols.txt` names the 82 members from align.txt with `scope:weak`, the sinit `scope:local`.
  Static members `mFormatBufferSize` (`.data` 0x80455EA8 `<c>`, 0x80455EAC `<w>`, `= 0x100`), `mFormatBuffer` and
  `mDefaultTagProcessor` (`.bss` 0x80525B60-0x80525B6C, `<c>` pair first) are weak and named; the two
  destructor-chain nodes (0x80525B48, 0x80525B54), the two instantiation guards (0x80525B70, 0x80525B71) and the
  three `.rodata` constants (0x803DEDF8-0x803DEE0C) keep `lbl_`. `.text` ends at its last instruction (0x803D49D4)
  before asm `mix.c`.
- **`vswprintf`.** `fn_801A1A7C` (MSL, 0x8C) is renamed `vswprintf` in `symbols.txt`: `TextWriterBase<w>::VSNPrintf`
  is a tail call to it, as sdk_2009's inline is `vswprintf(...)`, and its body is the `__wpformatter` wrapper shape
  (`vsnprintf`'s wide twin).

## HBM rulings (sdk-hbm-11)

lyt_textBox (Matching).

- **Source.** Adapted from sdk_2009's `lyt_textBox.cpp` (xenoblade JP second reference). All 16 retail functions
  (the TextBox constructor from a data block, destructor, the four vertex-colour accessors, `DrawSelf`,
  `AllocStringBuffer`, `FreeStringBuffer`, both `SetString`, `GetRuntimeTypeInfo`, the weak `ut::Font` destructor,
  the unnamed-namespace `CalcLineRectImpl<w>` and `CalcStringRectImpl<w>`, `__sinit_\lyt_textBox_cpp`) are
  byte-identical under `cflags_hbm`, no per-object flags. The clean gate printed `build/R5WEA4/main.dol: OK` with the
  unit `Matching`; `gatecheck layout` matches; the rule checker reports no [match] finding.
- **Destructor at 99.99% in objdiff (tool limitation).** The inlined `SetFont(NULL)` keeps its dead font-size branch,
  whose int-to-float constant is loaded `lfd f1, lbl_803DED80@l(r3)` with no paired `@ha` (r3 holds `GetHeight`'s
  result there; sdk_2009 notes the same unpaired relocation). dtk cannot rebuild a lone low-half relocation from the
  DOL, so the retail side shows a raw `-0x1280` and objdiff scores the row as different; the linked bytes are
  identical (gate green). Commented at the code as the compiler's slip (R36).
- **Deviations from the reference text.** `char16_t code` is `u16`; sdk_2009's `(int)code < L' '` cast is kept
  (without it the test is `cmplwi`, retail `cmpwi`). `bool bSuccess ATTR_UNUSED = pResFont->SetResource(fontRes);` is
  written as the bare call (byte-neutral). Merge-review ruling: the reference's local exists to feed an assertion that
  release builds compile out (`ATTR_UNUSED` is the decomp author's warning suppression, not Nintendo text), and this tree
  has no assertion text to restore, so a dummy local would be R17 fake code; the call as a statement is kept, as for
  `__OSShutdownToSBY`. `ARRAY_LENGTH(mTextColors)` is spelled with `sizeof`.
- **Inlined, bodies kept (`// no copy in retail (inlined)`).** `GetColor`, `ClampColor`, `CalcLineStrNum`,
  `CalcStringRect`, `TextBox::Init`, `GetStringBufferLength`, `SetFont`, `GetTextDrawRect(TextWriterBase<w>*)`,
  `GetTextMagH`, `GetTextMagV`. **Dead-stripped, declared only:** the three `TextBox(u16 ...)` constructors,
  `GetFont`, `GetTextDrawRect(const DrawInfo&)`.
- **Headers.** New `nw4hbm/lyt/textBox.h` (`res::Font`, `res::TextBox`, size 0x74, and `TextBox`, size 0x100; the
  reference's untagged `mTextBoxFlag` bit-field struct kept); `detail::Get/SetHorizontalPosition` and
  `Get/SetVerticalPosition` join `nw4hbm/lyt/common.h`, `Rect::MoveTo` joins `nw4hbm/ut/Rect.h` (both sdk_2009).
- **Symbols and data.** The 16 functions take their align.txt names; the two templates keep the `@unnamed@` mangling,
  global as dtk wrote them. `__vt__Q36nw4hbm3lyt7TextBox`, `__RTTI__Q36nw4hbm3lyt7TextBox` (weak) and
  `typeInfo__Q36nw4hbm3lyt7TextBox` (`.bss` 0x80525B20, size 0x4) are named; the type-name string, base list and
  `L"
"` keep `lbl_`. dtk's 0x10-byte `lbl_803DED80` is split into the int-to-float double and the `0.5f`/`1.0f`
  literals at 0x803DED88/0x803DED8C (`GetTextMagH`/`V` read them off one base register; one object made objdiff
  mis-attribute the offsets). `.text` ends at its last instruction (0x803C9AD4) before asm lyt_window (with
  0x803C9AE0 the gate was red: the asm unit is 4-aligned).
- **R15 (open, [checked]).** The templates are defined before the TextBox members, as in sdk_2009; the compiler
  emits their instances after their first users, so the checker reports the order. Kept in the reference's order.
- **R13.** The checker reports an advisory for the overloaded virtual `SetString` (slots not checked); the vtable
  compares 100% in objdiff.

## HBM rulings (sdk-hbm-12)

lyt_animation (Matching).

- **Source.** Adapted from sdk_2009's `lyt_animation.cpp` (xenoblade JP second reference). All 11 retail functions
  (the unnamed-namespace `GetStepCurveValue` and `GetHermiteCurveValue`, `AnimTransform::GetFrameSize`, the
  AnimTransformBasic constructor and destructor, `SetResource`, both `Bind` and both `Animate`,
  `detail::FindAnimationLink`) are byte-identical under `cflags_hbm`, no per-object flags. The clean gate printed
  `build/R5WEA4/main.dol: OK` with the unit `Matching`; `gatecheck layout` matches; the rule checker reports no
  [match] finding.
- **Deviations from the reference text.** None that reach the compiler: `nullptr` is `NULL`, `std::memset` is
  `memset` without the reference's `reinterpret_cast<void*>` (a clangd silencer), the vertex-colour argument
  `*static_cast<u8*>(&u8Val)` (marked FAKE by the reference's author) is `u8Val`, and the author's comment on the
  Hermite expression is replaced by one naming the basis. All byte-neutral.
- **Inlined, bodies kept (`// no copy in retail (inlined)`).** `RIsSame`, the seven unnamed-namespace `Animate*`
  helpers (`AnimatePainSRT`, `AnimateVisibility`, `AnimateVertexColor`, `AnimateMaterialColor`, `AnimateTextureSRT`,
  `AnimateTexturePattern`, `AnimateIndTexSRT`), `AnimTransform::AnimTransform` and `AnimTransform::~AnimTransform`
  (inlined into the AnimTransformBasic constructor and destructor; the base vtable store is dead). **Dead-stripped,
  declared only:** `AnimTransform::IsLoopData`. The link also drops `__vt__Q36nw4hbm3lyt13AnimTransform` (its only
  user, the AnimTransform constructor, has no copy).
- **Headers.** `nw4hbm/lyt/animation.h` gains `res::AnimationBlock`, `res::AnimationContent`, `res::AnimationInfo`
  (the seven `RL**` signatures), `res::AnimationTarget`, `res::HermiteKey`, `res::StepKey` and `AnimTransformBasic`
  (size 0x20); `detail::GetStrTableStr` joins `nw4hbm/lyt/common.h`, `Pane::SetSRTElement` joins
  `nw4hbm/lyt/pane.h` (the reference's array view of the translate/rotate/scale/size members, as
  `Material::SetTexSRTElement`), `OSf32tou8`/`OSf32tos16` join `revolution/os/OSFastCast.h` (xenoblade form; listed
  in the helpers table).
- **Symbols and data.** The 11 functions take their align.txt names; the two curve functions keep the `@unnamed@`
  mangling, global as dtk wrote them. `__vt__Q36nw4hbm3lyt18AnimTransformBasic` and the two `__RTTI__` objects
  (AnimTransformBasic, AnimTransform; weak, as compiled) are named; the type-name strings and the base list keep
  `lbl_`. dtk's 0x14-byte `lbl_803DECE4` is split into the five float literals at 0x803DECE4..0x803DECF4
  (`GetHermiteCurveValue` reads them off one base register; one object made objdiff mis-attribute the offsets, 99.8%).
  `__RTTI__Q36nw4hbm3lyt13AnimTransform` scores 66.7% in objdiff: in the unlinked object the dead AnimTransform vtable
  still sits before its type-name string, so the string's offset differs from retail; the linked bytes are identical
  (`gatecheck layout` reports the stripped vtable, gate green).
- **R13.** The checker reports advisories for the overloaded virtuals `Bind`/`Animate` (slots not checked) and for
  AnimTransform (no retail vtable); the AnimTransformBasic vtable compares 100% in objdiff.

lyt_common (Matching).

- **Source.** Adapted from sdk_2009's `lyt_common.cpp` (xenoblade JP second reference). All 16 retail functions
  (`EqualsPaneName(const char*, const char*)`, `EqualsMaterialName`, both `TestFileHeader`, the TexCoordAry
  constructor, `Free`, `Reserve`, `SetSize` and `Copy`, `IsModulateVertexColor`, `MultipleAlpha(Color, u8)`,
  `SetVertexFormat`, the two `DrawQuad(basePt, size, ...)` overloads, `DrawLine`, `InitGXTexObjFromTPL`) are
  byte-identical under `cflags_hbm`, no per-object flags. The clean gate printed `build/R5WEA4/main.dol: OK` with the
  unit `Matching`; `gatecheck layout` matches; the rule checker reports no [match] finding.
- **Deviations from the reference text.** None that reach the compiler: `nullptr` is `NULL`, `std::strncmp` is
  `strncmp`, `ARRAY_LENGTH` is spelled with `sizeof`, `BOOLIFY_TERNARY_TYPE(GXBool, ...)` (the decomp author's macro)
  is the plain `? GX_TRUE : GX_FALSE`, the byte-order mark and opaque white are `NW4HBM_BYTEORDER_BIG` and
  `ut::Color::WHITE`, the `0x80000000` test in `InitGXTexObjFromTPL` carries a comment (an unbound palette still holds
  file offsets) in place of the author's TODO, and `byte_t`/`byte4_t` are `u8`/`u32`. All byte-neutral.
- **Inlined, body kept (`// no copy in retail (inlined)`).** `MultipleAlpha(Color*, const Color*, u8)` (inlined into
  the alpha `DrawQuad`). **Dead-stripped, declared only** (in `nw4hbm/lyt/common.h`): `EqualsPaneName(const u8*,
  const u8*)`, `TexCoordAry::GetCoord`, `TexCoordAry::SetCoord`, `DrawQuad(polLT, polRB, texLT, texRB, vtxColors)`.
- **Headers.** `nw4hbm/lyt/common.h` gains `res::BinaryFileHeader` (size 0x10; the layout files' own header type,
  distinct from `ut::BinaryFileHeader` as in sdk_2009, which mangles `lyt::res`), `GetSignatureInt`, the two
  TexCoordAry coordinate accessors and the missing detail declarations (`TestFileHeader`, the second
  `EqualsPaneName`, `MultipleAlpha` and two `DrawQuad` overloads); each is declared once. `TPLBind` and `TPLGet` join
  `revolution/tpl.h` (sdk_2009 signatures).
- **Symbols and data.** The three `fn_` functions take their align.txt names (both `TestFileHeader`, the five-argument
  `DrawQuad`). `fn_80277F60`/`fn_80278080` are renamed `TPLBind`/`TPLGet` (the TPL unit's `TPL.c` string, the SDK
  sizes 0x118/0x20 and these call sites). The `SetSize` local static (`texCoords`, `.bss` 0x80525AE8, size 0x20) and
  its guard (0x80525AE0), and the three `.rodata` literals (`0.0f`, `1.0f`, the int-to-float double) keep `lbl_`:
  they are compiler-named (`texCoords$NNN`, `@GUARD@...`, `@NNN`). `Layout::mspAllocator` (`.bss` 0x80525B08) is
  not taken: it belongs to lyt_layout.

Merge-review rulings (sdk-hbm-12).

- **New `ASM-HELPER`s** (`__OSf32tou8`, `__OSf32tos16` in `revolution/os/OSFastCast.h`) qualify: the public SDK form
  (xenoblade, ogws, tp `OSFastCast.h`), and retail's two Animate functions show the bare quantised store
  (`psq_st f0, 0(r31), 1, qr2` / `qr5`) with no conversion code. A quantised store has no C spelling. Listed in the
  helpers table.
- **The vertex-colour argument** the reference author marked FAKE (`*static_cast<u8*>(&u8Val)`) is written as `u8Val`:
  the bytes are identical and nothing in the source steers; the plain spelling is the natural one and the marker is the
  reference author's own admission of a cast that does nothing.
- **Macro substitutions.** `BOOLIFY_TERNARY_TYPE`, `ARRAY_LENGTH` and `byte_t`/`byte4_t` are the decomp-project
  helper macros and typedefs of the reference trees (the tooling `decomp.h`), not Nintendo text; the plain ternary,
  `sizeof` and `u8`/`u32` are byte-neutral and consistent with the sdk-bte-2 `ARRAY_LENGTH` ruling.
- **`TPLBind` / `TPLGet`** renames are proven: `TPLBind` loads the `"TPL.c"` `.sdata` string into its `OSPanic` call,
  both sizes (0x118, 0x20) are the SDK's, and `InitGXTexObjFromTPL` calls them; the two declarations in
  `revolution/tpl.h` duplicate nothing.
- **`__RTTI__Q36nw4hbm3lyt13AnimTransform` at 66.7%** is an objdiff-only artefact of the unlinked object (the dead
  AnimTransform vtable still precedes the type-name string there); the linked bytes and `gatecheck layout` are
  identical, the gate is green.

## HBM rulings (sdk-hbm-13)

lyt_layout (Matching).

- **Source.** Adapted from sdk_2009's `lyt_layout.cpp` (xenoblade JP second reference; mscharged-decomp's for
  `BuildPaneObj`). All 15 retail functions (the unnamed-namespace `SetTagProcessorImpl`, the Layout constructor and
  destructor, `Build`, `CreateAnimTransform`, `BindAnimation`, `UnbindAnimation`, `UnbindAllAnimation`,
  `SetAnimationEnable`, `CalculateMtx`, `Draw`, `Animate`, `GetLayoutRect`, `SetTagProcessor`, `BuildPaneObj`) are
  byte-identical under `cflags_hbm`, no per-object flags. The clean gate printed `build/R5WEA4/main.dol: OK` with the
  unit `Matching`; `gatecheck layout` matches; the rule checker reports no [match] finding.
- **`BuildPaneObj` stays out of line through its single exit.** With sdk_2009's text (a `return` in every case)
  MWCC inlines `BuildPaneObj` into `Build` (36% there). sdk_2009 keeps it out of line with four `(void)0` statements
  (its author's "stripped assert" theory) and xenoblade with `DECOMP_DONT_INLINE`; both are steering. mscharged's
  spelling, one `pPane` assigned in each case and returned after the switch, matches both functions with nothing
  added. mscharged also has a debug-only `NW4HBMPanicMessage_Line(503, "unknown data type")` in the default case; it
  is `((void)0)` in release and byte-neutral (A/B compiled both ways), so it is left out, as the earlier HBM units
  leave out the reference trees' stripped asserts. No nw4hbm assert header is added.
- **Deviations from the reference text.** None that reach the compiler: `nullptr` is `NULL`, `decltype(it)` is
  `__typeof__(it)` (as in lyt_group), `POINTER_ADD` is `detail::ConvertOffsToPtr<void>`, the group block's
  `reinterpret_cast` of the block header is a `static_cast` of `dataPtr` (the same address), and `NW4HBM_FOUR_CHAR` /
  `byte4_t` signatures are `u32` multi-character literals (the existing `resourceAccessor.h` / `animation.h` form).
- **Inlined, bodies kept (`// no copy in retail (inlined)`).** The three unnamed-namespace `CreateObject` templates.
  `SetTagProcessor` holds one expanded level of the recursive `SetTagProcessorImpl` (0x1A8 bytes against 0x1A0), as
  compiled. No dead-stripped functions (align.txt lists none for the unit).
- **Headers.** `nw4hbm/lyt/layout.h` gains the full `Layout` class (sdk_2009 layout: vtable with the destructor and
  ten virtuals, `mAnimTransList` 0x04, `mpRootPane` 0x10, `mpGroupContainer` 0x14, `mLayoutSize` 0x18,
  `mOriginType` 0x20, size 0x24; `OriginType_et`), `res::Layout` (size 0x14) and the `RLYT`/`lyt1`/`txl1`/`fnl1`/
  `mat1` signatures. The block signatures join their owners' headers (`pan1`/`pas1`/`pae1` pane.h, `pic1`
  picture.h, `txt1` textBox.h, `wnd1` window.h, `bnd1` bounding.h, `grp1`/`grs1`/`gre1` group.h, `RLAN`/`pai1`
  animation.h). `ut::DynamicCast` and `detail::GetTypeInfoFromPtr_` join `nw4hbm/ut/RuntimeTypeInfo.h` (sdk_2009 text).
- **Symbols and data.** The 15 functions take their align.txt names. `__vt__Q36nw4hbm3lyt6Layout` (0x34) and
  `__RTTI__Q36nw4hbm3lyt6Layout` (weak, as compiled) are named; the type-name string keeps `lbl_`, and the two
  `.rodata` floats (`0.0f`, `2.0f`) keep `lbl_` (compiler-named `@NNN`). `Layout::mspAllocator` (`.bss`
  0x80525B08) is defined here (taken with `unit.exe new --include-data`); its `symbols.txt` size is 0x4, the next 4
  bytes being alignment (as `typeInfo__Q36nw4hbm3lyt7TextBox`).
- **R13.** The Layout vtable compares 100% in objdiff; the checker's advisories are the existing ones (AnimTransform,
  PaneBase, overloaded virtuals).

## HBM rulings (sdk-hbm-14)

HBMRemoteSpk, HBMAxSound and HBMCommon (all Matching).

- **Source.** yannicksuter/mscharged-decomp builds this exact drop (`R4QK01`, banner `Mar  3 2009 14:00:03
  (0x4201_142)`), so its text is the first reference for all three units (its `VERSION_R4QK01` branches where it
  has them); sdk_2009 (2010 drop) and xenoblade are second. All 37 functions (15 + 10 + 12) are byte-identical under
  `cflags_hbm`, no per-object flags; each unit's clean gate printed `build/R5WEA4/main.dol: OK`, `gatecheck layout`
  matches, and the rule checker reports no [match] finding.
- **HBMCommon.** The 0x24 function at 0x803B89A0 (no name in the align list) is `HBMGetSelectBtnNum` (same body as
  the reference's). `HBMStartBlackOut` and `HBMIsReassignedControllers` have no copy in retail and are declared only
  (`<revolution/hbm.h>`). `HBMCreateSound` calls the inline `HomeButton::createSound` (mscharged R4QK01), which
  explains the otherwise unused `getInstance()` call; sdk_2009's unused `ATTR_UNUSED` local is not adopted. The
  reference's asserts are release no-ops and are left out. `getAllocatorType` is inlined (`// no copy in retail
  (inlined)`). Data: the banner string (keeps `lbl_`), `__HBMVersion`, `spAllocator` (C linkage, `.data`) and the
  local `sAllocator` (`.bss`).
- **HBMAxSound.** The HBM sound library (`seq.c`, `syn.c`, `mix.c`) is C (unmangled names): `sound/seq.h`,
  `sound/syn.h` (partial: `HBMSYNVOICE` forward-declared, owned by `synvoice`) and `sound/mix.h` declare it in
  `extern "C"`, layouts proven from use (`HBMSEQSEQUENCE` 0x2E1C, `HBMSYNSYNTH` 0x2408). The five anonymous-namespace
  helpers retail inlines keep their bodies (`(inlined)`). `SetSoundMode` is a tail call to `HBMMIXSetSoundMode`;
  `AxSoundMain` is empty. Invented names (marked at the code): `DEFAULT_PLAYER_NUM`, `FOCUS_PLAYER_NUM`,
  `SOUND_THREAD_STACK_SIZE`, `SOUND_WORK_AREA_SIZE`, `HBMSEQ_STATE_STOP`/`_RUN`. `HBM_MEM_SIZE_SOUND` (0x18700, name
  from sdk_2009's asserts) joins `<revolution/hbm.h>`. `fn_801A5E8C` is named `log10` (MSL wrapper beside `pow`;
  declared in `include/math.h`). Data: the two `.rodata` filename statics, three float constants (`lbl_`), the 28
  MIDI name literals and the function-local `SOUND_FILENAME` table (`lbl_804537B8`, its `$NNN` unknown), `sWork`.
- **HBMRemoteSpk.** The align list's names for 0x803B7960, 0x803B7990 and 0x803B7A80 were wrong (sdk_2009's 2010
  Off/Disconnect set); the bodies are this drop's `DelaySpeakerPlayCallback`, `SpeakerPlayCallback` and `Connect`
  (mscharged/xenoblade), renamed in `symbols.txt`. The class is polymorphic (`virtual ~RemoteSpk()`, vptr at 0x1F0,
  size 0x1F8); its vtable and `__RTTI__` are defined here (the key function is here, so the RTTI object is global).
  The reference's unread `result` locals (sdk_2009 DWARF records them) are dropped: same bytes, and an unread local
  fails the reader test. Reviewed at merge: kept out, as in the sdk-bte-8 ruling (a result never read is a bare call; R17 blocks the unread local, and retail leaves no trace of it). Inlined-only bodies kept: `SetInstance`, `GetInstance`, `GetPCMFromSeID`, `MakeVolumeData`.
- **SDK headers (merge note for the sdk-bte lane).** `include/revolution/wpad.h` gains one line, `typedef s32
  WPADResult;`. New `include/revolution/wenc.h` (`WENCInfo`, `WENC_FLAG_USER_INFO`, `WENCGetEncodeData`;
  `fn_802494B4` named `WENCGetEncodeData`).
- **HBM headers.** New public `<revolution/hbm.h>` (types and API; xenoblade's `HBMTypes.h` layout, offsets checked
  against HBMCreate). `HBMBase.h` declares only what HBMCommon uses of `homebutton::HomeButton` (`mSequence`,
  `mpHBInfo` and the member functions it calls); the HBMBase unit must add the rest of the class there.
  `HBMCommon.h`, `HBMAxSound.h`, `HBMRemoteSpk.h` are the units' own headers.

## HBM rulings (sdk-hbm-15)

HBMController, HBMGUIManager and HBMBase (all Matching).

- **Sources.** yannicksuter/mscharged-decomp's R4QK01 sources are this drop; its
  `src/RVL_SDK/hbm/R4QK01/HBMBase.cpp` (not the generic 2010 `HBMBase.cpp`, which defines the sound-archive
  interface this drop does not have) and the generic `HBMController.cpp`/`HBMGUIManager.cpp` with their
  `VERSION_R4QK01` branches selected. Each unit's clean gate printed `build/R5WEA4/main.dol: OK` (`unit.exe flip`),
  `gatecheck layout` matches, and the rule checker reports no [match] finding. objdiff's remaining <100% rows are
  compile-time data names only (the retail split labels the classes' tables and constants `lbl_`/`@NNN` where the
  source emits named symbols; the linked bytes and `gatecheck layout` agree).
- **HBMController (25 functions, 0x803B66F0-0x803B7300, `HBMController.h`).** `RemoteSpk*` sits at 0x20 (this drop
  has no `nw4hbm::snd::SoundHandle`; the R4QK01 layout), so `mHBController` is 0x20, the callbacks 0x24/0x28,
  `mPlaySoundTime`/`mStopSoundTime` 0x30/0x38 (8-byte aligned) and the flags 0x40-0x44 (object size 0x48).
  `soundOn`, `soundOff`, `getChan`, `getSpeakerVol` and `isPlayingSound` have no out-of-line copy in retail (every
  caller expands them): their definitions carry `inline` and `// no copy in retail (inlined)`. `wpadConnectCallback`
  takes `WPADSetExtensionCallback`'s old value only in the 2010 drop; retail stores the extension callback it
  replaces (0x28), as the R4QK01 branch does. The deleting-destructor shape of
  `__dt__Q210homebutton10ControllerFv` (null check, alarm cancels, `delete this`) is MWCC's codegen for a plain
  non-virtual destructor; the class has no vtable. `initCallback`'s device variable is `WPADDeviceType` (the 2009
  SDK type); `WPADInfo` stays opaque in the header (forward declaration) and `revolution/wpad/__wpad.h` is included
  only by the unit's `.cpp`, which calls `WPADGetInfoAsync`.
- **HBMGUIManager (56 functions, 0x803B4ED0-0x803B66E8, `HBMGUIManager.h`).** The retail `.data`
  0x80453238-0x804534A8 is the four kept vtables with their type-name strings, base lists and the `__RTTI__`
  objects, then the `Interface` type-name string and RTTI object (0x80453480/0x804534A0): the `Interface` vtable is
  never instantiated and is dead-stripped, and so are the `Interface` init/dtor copies our object emits (weak,
  unreferenced). The vtable slots fix the class layouts: `Interface` 0x04, `EventHandler` 0x08, `Component` 0x2C,
  `Manager` 0x18, `PaneManager` 0x2C, `PaneComponent` 0x30; `Manager::delComponent` is virtual in this drop
  (`getComponent` follows it). The four vtables' `.data` names are the `__vt__Q310homebutton3gui*` symbols (the
  HBMBase asm referred to them by the retail labels; on the unit's data rename the labels resolve).
  `PaneManager::muPadding` is the reference's `byte2_t muPadding` (alignment, `// pad:` note).
- **`setAllBoundingBoxComponentTriggerTarget`'s pragma is an UNPROVEN codegen control.** With precedence
  propagation on, the compiler duplicates the loop's first test; turning propagation off gives retail's shared test
  at the loop bottom. mscharged's R4QK01 `HBMGUIManager.cpp` does **not** carry the pragma. The only source that
  does is sdk_2009-12-11, wrapped there in `#if defined(NDEBUG)` / `TODO(FAKE)`, with the note "also works as
  `#pragma opt_propagation off`, but I think it's probably this quirk". Both spellings produce identical bytes (A/B
  compiled); the source keeps `#pragma opt_propagation off` because it is the narrower switch that reference's
  author named as the equivalent form, and the alternative `#pragma ppc_iro_level 0` (that reference's own
  spelling) is noted at the code. Recorded as a `RULE-EXCEPTION` at the pragma (R17 lever) and open: by the OS.c
  ruling's standard this is a compiler-state lever, not evidence; what would make it authentic is evidence that the
  SDK built this unit with that setting (a per-object flag matching the whole unit with no pragma in source, or the
  pragma in a reference not marked FAKE). The function is byte-identical either way, so it does not change what the
  gate proves; the open item is the *attribution*.
- **HBMBase (33 functions, 0x803B8AB0-0x803C0E60, `HBMBase.h`).** The R4QK01 class: `HomeButton` 0x7A0, no snd
  sound-archive/sound-handle members, `createSound` inlines `InitAxSound`, `update_sound` is the retail tail of the
  unit. 28 helper definitions are marked `inline` so no out-of-line copies are emitted (retail has none) while
  staying in the reference's order: `init_msg` (already `inline` in the reference), the `reset_*` group,
  `init_vib`, `calc_fadeoutAnm`, the speaker/simple-sync alarm setters and callbacks, `update_posController`,
  `isActive`/`isUpBarActive`/`isDownBarActive`, `findAnimator`/`findGroupAnimator`, `setForcusSE`,
  `setVolume`/`getVolume`, `setVibFlag`/`getVibFlag`, `startBlackOut`, `play_sound`/`fadeout_sound`, and the four
  `BlackFader` members. Two static helpers have no retail copy either and no address comment: `drawBlackPlate` and
  `get_comma_length` (both expanded into their single callers; `// no copy in retail (inlined)`).
  **`WPAD_ECOMM` in the references is `WPAD_ERR_BUSY` (-2) in this drop**: `HomeButton::update` compares both
  `wpad_err` and the `WPADProbe` result with `cmpwi -2` (0x803BCAAC/0x803BCAB4). The unit's `.data` starts at
  0x80453878: `scCursorLytName`'s initializer owns the four layout-name literals `"P1_Def.brlyt"`..`"P4_Def.brlyt"`
  first (0x80453878, 0x80453888, 0x80453898 and, pooled into the array's own storage, 0x804538A8) and the
  `scCursorLytName` pointer array follows at 0x804538B8; dtk's data inference had put the unit's start at
  0x804538C8 (the first object after the array), which left the literals and array in the gap. The built object
  emits the four literals separately; the linker's layout still matches (gatecheck layout). HBMBase's header changes
  (additive): new `nw4hbm/lyt.h` umbrella, `group.h` gains `PaneLinkList`, `pane.h`
  `SetTranslate(const math::VEC2&)`, `math/triangular.h` `AtanDeg`/`AtanRad`/`Atan2Deg`/`Atan2Rad`, and
  `HBMFrameController.h` the accessors the class's users need — of which only `setAnimType` is called (the rest are
  the reference header's own inline members). `SpeakerCallback` etc. keep the reference's `static` (file-local)
  linkage.
- **SDK header changes (shared, all additive).** `revolution/wpad.h`: the older result-code aliases
  (`WPAD_ESUCCESS`, `WPAD_ENODEV`, `WPAD_EBUSY`, `WPAD_ETRANSFER`, `WPAD_EINVAL`, `WPAD_CENODEV`), `WPADResult`,
  `WPADDeviceType` and the `WPADProbe(chan, WPADDeviceType*)` prototype, as the 2009 SDK's own wpad.h.
  `revolution/os/OSTime.h`: `OSDiffTick` in the SDK's 32-bit form (`(OSTick)(t1) - (OSTick)(t0)`); retail's
  `Controller::updateSound` timeout tests are 32-bit divides, which only that form gives (the 64-bit
  `(OSTime)t1 - (OSTime)t0` produces `__divu64` calls). `revolution/os/OSAlarm.h`:
  `OSGetAlarmUserDataAny`/`OSSetAlarmUserDataAny`, the SDK's own access forms. `revolution/mtx.h`:
  `PSMTXTrans` and `PSMTXInverse`; `PSMTXInverse` returns `u32` (0 for a singular matrix, 1 on success —
  0x8021EBB0's body and the SDK declaration; the HBM callers ignore the result, so the retype is byte-inert).
  `string.h`: `strcat`/`strncat`.
- **SDK callees named from these units' call sites (symbols.txt):** `__nw__FUl` (0x801AD014, the `operator
  new(size_t)` wrapper the source's `new` expressions call; 0xC/0x10 shapes in the MSL allocator family),
  `PSMTXInverse` (0x8021EBB0) and `PSMTXTrans` (0x8021EDE0), from the MTX library's function order and the units'
  calls.
- **Array counts are spelled `sizeof(array) / sizeof(array[0])`.** The reference's 20 `ARRAY_COUNT(x)` sites in
  HBMBase are yannicksuter/mscharged-decomp's tooling macro (`tools/decomp.h`), not Nintendo text; the project
  already rules that substitution byte-neutral (sdk-hbm-12, `ARRAY_LENGTH`), so no `ARRAY_COUNT` helper is added to
  the tree.
- **Evidence comments were re-derived, not trusted by order.** Every function's trailing `// 0x8XXXXXXX` was
  checked against `symbols.txt` by *name*: the symbol at the commented address must have the function's own name,
  and for a member its mangled encoding must name the same class (all 32 HBMBase and 23 HBMGUIManager definitions
  with retail copies bind; HBMController's 25/25 bind after `disconnect` was written in block form). The two
  HBMBase statics with no retail copy carry no address. `build/tools/unit.exe check` only verifies that a trailing comment
  names *some* function of the unit, not the enclosing one — it accepted 36 wrong assignments (the review's
  finding); it needs an enclosing-function check (open, tools).
- **Open.** `HBMBase::init_volume`/`init_sound`'s `AXFX` calls still leave objdiff rows at 99.8% from `.rodata`
  symbol names only (retail's `lbl_803Dxxxx` compiler constants stay `lbl_`); `HBMGUIManager`'s `Interface` weak
  copies and `HBMBase`'s weak header helpers are dead-stripped as in retail (right side only). Reference-inherited
  codegen shapes kept with `// R36` notes: `(void)mpPane->GetTranslate();` (unread inline accessor) and
  `is_visible(...) ? true : false` (the ternary retail's codegen comes from). Raw event codes stay numeric:
  HBMGUIManager passes 0/1/2/3/5 (sdk_2009 names them `EventHandler::TrigEvent`/`PointEvent`/`LeftEvent`/
  `Event3`/`ReleaseEvent`; our `EventHandler` mirrors mscharged's, which has no enum) and HBMBase passes
  82/86/94/214/221, whose enum no reference names. HBMBase.cpp keeps the R4QK01 reference's Allman brace layout;
  the other HBM units are K&R (style only, no byte effect; reflowing is open work).
  `build/R5WEA4/classes.json` was missing in the lane's first pass, so the checker's R13 slot checks were skipped
  then; after `build/tools/classes.exe scan` they run: classes=24 matched=12 unmatched=5 not-modelled=7 added=0 missing=0
  order=0 override=0, 12 advisories (overloaded virtuals / classes with no retail vtable by name), no finding. A
  missing `classes.json` should be reported by the gate/checker rather than silently skipping R13 (open, tools).

## HBM rulings (sdk-hbm-16)

The nine HBM sound units, `revolution/hbm/sound/{mix,syn,synctrl,synenv,synmix,synpitch,synsample,synvoice,seq}.cpp`
(0x803D49E0-0x803D8358, 56 functions). Reference: yannicksuter/mscharged-decomp
`src/RVL_SDK/hbm/sound/*.cpp` and the same drop's headers (`libs/RVL_SDK/include/revolution/hbm/sound/*.h`), i.e.
the HBM 20090303 drop our banner records. 55 of the 56 functions are byte-identical under `cflags_hbm` with no
per-object flags; eight units are `Matching` (clean gate: `build/R5WEA4/main.dol: OK`, `gatecheck attribute`
identical, `gatecheck layout` matches), `synctrl` stays `NonMatching` on one schedule residue (see below). The
rule checker reports no [match] finding in the nine units.

- **C or C++ (the documentation's open item).** The units are C++ (`.cpp`) with C-linkage exports. Evidence:
  (1) every exported function is unmangled, i.e. `extern "C"`: the references' `sound/*.h` wrap the API in
  `extern "C"`, and a definition that follows such a declaration inherits C linkage; (2) none of the nine ranges
  holds a `__sinit`, a `.ctors` entry or an `__RTTI__` object, and none of the units has a dynamic initializer
  (their statics are constant-initialized tables and zeroed state), so target-file naming is the same either way
  and the binary cannot distinguish the front end by itself; (3) the drop's source is C++-only as written -
  mwcceppc `-lang=c` rejects it (mix.cpp `HBMMIXInit` declares `int i;` after `assert(AXIsInit())`, and
  `HBMMIXInitChannel` declares `HBMMIXIndex index` mid-block: "(10141) expression syntax error"), so the shipped
  source was C99 or C++; (4) the unit names are `sound/mix.cpp` and so on in sdk_2009 (2010 drop) and in
  mscharged (R4QK01, our banner), and only xenoblade spells them `mix.c`. The units are compiled as `.cpp`; the
  byte gate does not depend on the choice because all exported code has C linkage.
- **Inlining and linkage forced by retail's function lists.** A `static` body inlined at every call site leaves no
  copy, an external definition always leaves one, so retail's 56-function list fixes the linkage of the helpers the
  references declare without `static`:
  - mix (15): `__HBMMIXSetPan` out of line; `__HBMMIXResetChannel`, `__HBMMIXClampPan` and `__HBMMIXGetVolume`
    fully inlined (`// no copy in retail (inlined)`).
  - syn (8): `__HBMSYNRemoveSynthFromList` out of line, `__HBMSYNAddSynthToList` inlined.
  - synctrl (4): only `__HBMSYNResetAllControllers`, `__HBMSYNNoteOn`, `__HBMSYNMidiIn` and
    `__HBMSYNRunInputBufferEvents` have copies. `__HBMSYNSetController` and `__HBMSYNGetWavetableData`, which the
    reference's `synctrl.h` declares with external linkage, therefore had internal linkage in retail: both are
    `static` here and dropped from the header (an external definition would add the copies retail does not have).
    `__HBMSYNProgramChange`, `__HBMSYNResetController` and `__HBMSYNNoteOff` are internal and fully inlined.
  - synenv (2): `__HBMSYNGetEnvelopeTime` internal, inlined into `__HBMSYNSetupVolumeEnvelope` (dropped from
    `synenv.h`); synsample's `__HBMSYNGetNibbleAddress` likewise.
- **Data.** Each unit owns its statics and names them in `symbols.txt` at their retail addresses, which splits
  dtk's lumped `lbl_` labels; the addresses come from the units' own references (offsets 0x790/0x990/0xA90 in mix's
  volumen/pan/DPL2 tables, 0x190 into the volume-attenuation table, 0x18/0x50/0x10/0x2E strides in the wavetable
  record sizes). mix: `__HBMMIXVolumeTable` 0x80455EB0 (965 u16 = 0x78A, then 6 alignment bytes),
  `__HBMMIXPanTable` 0x80456640, `__HBMMIX_DPL2_front` 0x80456840, `__HBMMIX_DPL2_rear` 0x80456940 (`.data`
  0xB90); `sIndex2IdTable` 0x80525B78 (16 s8), `sId2IndexTable` 0x80525B88 (`AX_VOICE_MAX` = 96 s8),
  `__HBMMIXSoundMode`, `__HBMMIXChannel`, `__s_HBMMIXChannel` (16 x 0x64) and `__init` in `.bss`
  0x80525B78-0x80526234. syn: `__HBMSYNSynthList`, `__HBMSYNVoice`, `__s_HBMSYNVoice` (16 x 0x4C) and `__init`
  (`.bss` 0x80526238-0x80526704). synctrl `__HBMSYNn128` 0x80456A40; synmix `__HBMSYNVolumeAttenuation`
  0x80456C40 and `__HBMSYNAttackAttnTable` 0x80456E40; synpitch the four pitch tables at 0x80456FD0-0x804573C0;
  seq `__HBMSEQMidiEventLength` 0x804573C0 and its list head and `__init` (`.bss` 0x80526708). The float literal
  pools keep the compiler's own `@NNN` names in `symbols.txt` (`.rodata` 0x803DEE10-0x803DEE6C): the stripped DOL
  cannot name them, and they are the compiler's form, not a guess. `HBMSYNVOICE` is 0x4C bytes in
  `sound/synvoice.h`, confirmed by retail's voice stride 0x4C and `synth` at 0x08.
- **Headers.** New `sound/sound.h` (the `ARRAY_LENGTH` helper), `sound/synctrl.h`, `synenv.h`, `synmix.h`,
  `synpitch.h`, `synsample.h`, `synvoice.h`;
  `sound/syn.h` gains one extern (`__HBMSYNVoice`); `include/assert.h` (MSL assert, a no-op under the build's
  `-DNDEBUG=1`); `include/revolution/ax.h` gains the `AXMIXERCTRL` and `AXMIXERCTRLRMT` enums (the mixer-control
  bits the units OR into `pb.mixerCtrl`). All additive; `sound/mix.h`, `seq.h` and `syn.h` keep their existing
  declarations and order.
- **`ARRAY_LENGTH`.** The drop sizes the loop bounds over its fixed tables with `ARRAY_LENGTH` (mix's channel and
  index tables, syn's voice pool). The reference project keeps that helper in its `decomp.h`, which this project
  has no equivalent of, so the nine units share it from the new `sound/sound.h` and the sources keep the drop's
  spelling (`ARRAY_LENGTH(sIndex2IdTable)`, `ARRAY_LENGTH(__s_HBMSYNVoice)`, ...); the array declarations keep the
  sizes the drop writes (`[16]`, `[AX_VOICE_MAX]`). No name in the nine units is invented.
- **Open items (recorded, no code change).** (C5) the `@NNN` literal-pool entries carry a plain `type:object`
  annotation while the surrounding retail entries use `data:float`/`data:4byte`; the addresses and sizes are exact,
  only the annotation style is inconsistent. (C6) the data externs in `sound/synctrl.h` (`__HBMSYNn128`) and
  `sound/synmix.h` (the attenuation and attack tables) sit outside the `extern "C"` block while `sound/syn.h`'s
  `__HBMSYNVoice` is inside it; there is no symbol effect (MWCC emits the same unmangled name either way and the
  gated objects are identical), but the placement should be normalised when the headers are next touched.

### `synctrl` `__HBMSYNResetAllControllers` (0x803D67F0)

90.55%: 57 rows on both sides, same instruction multiset, same register-to-value mapping, same body, but the
four-instruction loop preheader is scheduled differently. Retail hoists `lwz r7, __HBMSYNVolumeAttenuation@l(r5)`
and `lwz r6, 0x190(r4)` ahead of `li r9, 0` and `li r4, 0x40`; the build emits `li r9, 0`, `lwz r6`, `li r4, 0x40`,
`lwz r7`. All six orders of the unit's three effective `__HBMSYNSetController` reset calls (volume, pan, reverb
send) were compiled: the reference order (volume, pan, reverb) keeps the body's store order and the register
mapping identical to retail, while reverb/pan/volume reproduces retail's preheader but reverses the body's stores
(99.53%), contradicting retail's body, so the reference order is kept. Wall class: register/schedule (4). No
permuter run (the unit is C++; the residue is a scheduler placement, not a spelling). `build/tools/flagsweep.exe` over
Wii/1.0a, 1.0RC1, 1.0, 1.1, 1.3 and 0x4201_127 gives the same six differing rows in every build, so it is not a
compiler-version choice. The other three functions are exact, so the unit stays `NonMatching` until the
preheader closes.

## HBM rulings (sdk-hbm-17)

lyt_window (Matching).

- **Source.** yannicksuter/mscharged-decomp builds this exact drop (`R4QK01`), and its
  `src/RVL_SDK/hbm/nw4hbm/lyt/lyt_window.cpp` closes the open item sdk-hbm-7 left: the original spells the
  top-left (LT) part out inside `Window::DrawFrame` instead of calling `GetLTTexCoord`. The origin coordinate is
  read from the flip table (`iy = flipInfo.idx[1]`, then `ix = flipInfo.idx[0]`), then the inline
  `SetFrameTexOrigin` writes it to the two neighbouring corners and `SetFrameTexExtent` writes the covered
  extent; the RT, RB and LB parts stay on the shared `Get*TexCoord` helpers and their macro. With that block the
  unit is 21 of 21 functions at 100% (`DrawFrame` 0x803CA490, 0x5DC), `unit.exe flip` printed
  `build/R5WEA4/main.dol: OK` (every Matching unit at 100%), `gatecheck layout` matches and the rule checker
  reports no finding (its R13 slot checks were skipped on that run for lack of `build/R5WEA4/classes.json`; re-run
  after `build/tools/classes.exe scan`: classes=7 matched=4 unmatched=2 not-modelled=1 added=0 missing=0 order=0
  override=0, 3 advisories, no finding). sdk_2009-12-11's single-macro `DrawFrame` (the unit's previous source)
  gives the LT quad's inlined `ix`/`iy` the reverse numbering (`ix` r6, `iy` r7, `-1` in r5) and sits at 92.5%;
  retail numbers them like the other quads (`ix` r9, `iy` r8, `-1` in r0), which the separate LT expansion
  reproduces. The sdk-hbm-7 `wdbg` trace (the LT objects are not the declared locals of `GetLTTexCoord`) is
  explained by this: the LT quad's objects come from its own expansion, not from that helper.
- **Layout.** The helpers are `inline` and marked `// no copy in retail (inlined)` (retail has no `SetFrameTex*`
  symbol; every use is expanded). The block's locals are at function scope: mscharged's `{ }` around them is
  byte-identical (A/B compiled) and is dropped; the function declares its other locals flat, so the extra scope
  buys nothing (style only, no byte effect). `GetLTTexCoord` and `GetLTFrameSize` keep their bodies unchanged:
  `DrawFrame4` and `DrawFrame8` inline them for their own LT part and stay at 100% (sdk-hbm-7's attempted `u8`
  locals in that helper broke both). The unit's data is unchanged (`flipInfos` at `lbl_80454A50`, the three
  `.rodata` constants, the vtable/RTTI), so the pass is a `.text` change only; the three weak inline copies our
  object emits with no retail copy (`ut::Color`'s ctor/dtor, `Window::Content`'s dtor) remain right-side-only, as
  in the other HBM units.
- **Header.** `src/.../lyt_window.cpp` adds the reference line for mscharged's R4QK01 file; the unit's own
  header is unchanged.

## Parked (NonMatching), with reasons
- `revolution/hbm/sound/synctrl` (sdk-hbm-16): 3 of 4 functions 100%; `__HBMSYNResetAllControllers`
  90.55%, a loop-preheader schedule residue (ruling above).

- `revolution/dsp/dsp_task` (sdk-av-2): 5 of 5 at 100%; parked only because `__DSP_boot_task` is MATCH-PENDING (R17, table above). Linked, the gate is green.
- `MetroTRK/Processor/ppc/Generic/targimpl` (msl-trk-6, match-trk): 32 of 33 functions 100% (8 of them
  ORIGINAL-ASM, listed). `TRKPPCAccessFPRegister` 96.0%: retail schedules the parameter copies and `li r31` ahead
  of the first load of the NOP-array copy and stores its first word at once (schedule residue; declaration order,
  a separate `error = DS_NoError` and xenoblade's FPECR expression change nothing).
- `revolution/NdevExi2AD/DebuggerDriver` (21 of 21) and `revolution/NdevExi2AD/exi2` (10 of 10) are `Matching`
  (see "Branch integrate/antigravity review"). Both built `-O0`.

- `Runtime.PPCEABI.H/Gecko_ExceptionPPC`: `__register_fragment` 93.7%, a schedule residue (`li r6` before
  `addi r5`; retail has them the other way round). Traced cause (walls lane, docs/compiler_regalloc.md, "The
  first-function quirk"): in the first function of a translation unit the copy-propagation state globals
  `[0x747944]` (node in progress, initially 0) and `[0x747940]` (substitution done) leave the entry copy of the first
  parameter undeleted until after scheduler pass 1, which changes the order; patching `[0x747944]` to -1 under the
  debugger gives retail's order. So the original file compiled something before `__register_fragment` that no
  reference shows; inventing a function there would be a fakematch (the Wii references cover it with
  `#pragma schedule once`, not adopted). Same in all nine Wii compiler builds. `__unregister_fragment` is 100%.
  Ruled out as well: every spelling of the table walk's setup (`for (i = 0, fragment = fragmentinfo; …)`, the pointer
  assigned before the loop as a statement or in its declaration, `i = 0` as a separate statement, the two swapped in
  and out of the `for` init) compiles to the same `li r6` placement, and a per-object `-schedule off` moves eight rows
  of this function and five of `__unregister_fragment` the wrong way, so neither is the original's build setting.
- `revolution/os/OS` (sdk-os-4): 19 of 20 functions 100% in objdiff (five of them assembly, see the ledger),
  `OSInit` 99.9% (store order). Three functions reach their score only under `#pragma global_optimizer off`
  (ruling above: `MATCH-PENDING` if linked so); the pragma is function-scoped, not a build setting (the
  library-wide `-prefix` test under OSStateFlags, ruling above). `OSInit`'s remaining rows, for the next pass:
  two operands relocate to `gTRKInterruptVectorTable` in retail where the build materialises literals: the
  `lis r4, 0x8000` that is the high half of the 0x310C low-memory read (retail's `gTRKInterruptVectorTable@ha`,
  the same 0x8000), and `arenaLo = (void*)0x80004000`, which retail writes as
  `(void*)gTRKInterruptVectorTable` (MetroTRK owns the 0x80004000 symbol). Five base+offset accesses into
  `.data` (0x1f0/0x240/0x258/0x2b0/0x310) carry a different pool-base relocation (`@1` in retail, the section
  symbol here; the objects they reach are `@3163`/`@3165`/`@3166`/`@3167`/`@3168`), six `@sda21` instruction
  relocations name five constant objects (`@1854`/`@1856` in `.sdata` vs `@3162`/`@3164`, `@1009`/`@1011` in
  `.sdata2` vs `@2344`/`@2346`) although the data objects themselves pair at 100%, and the two locals of the
  `__OSInitIPCBuffer` call are stored to 0x8/0xC in the other order. In-source `ORIGINAL-ASM`
  markers: `__OSFPRInit`, `__OSDBIntegrator`, `__OSDBJump`, `OSExceptionVector`, `OSDefaultExceptionHandler`
  (asm functions) and the GQR block of `__OSPSInit` (inline asm in C). Move them to the `ORIGINAL-ASM` and
  listed-helper tables when the unit is linked.
- `revolution/os/OSStateFlags` (sdk-os-4): linked. `__OSWriteStateFlags` is `MATCH-PENDING` (the pragma lever, table
  above); `__OSReadStateFlags` and the statics are counted and the clean gate is green.
- `revolution/os/OSExec` (sdk-os-5): 10 of 11 functions 100%; `__OSLaunchNextFirmware` 95.3% (96.5% before the
  sdk-io-2 merge, which replaced the `vu32 numGamePartitions` of the pending `dvdlow.h` with the SDK's plain `u32`
  in `dvd.h`: the volatile was a lever in a function that does not match either way). The reload class is the same
  one OSSemaphore rejects: `volatile u32 numGamePartitions` restores both loop reloads but leaves the register
  allocation (96.4%; retail keeps the ES `rc` in a callee-saved register across the ESP calls, the build in r3),
  and `#pragma global_optimizer off` around the function makes it worse (93.8%). `Run` (60 bytes) is an assembly
  function (ORIGINAL-ASM when the unit is linked; not counted while parked).
- `revolution/dvd/dvd` (sdk-io-2, sdk-io-3, sdk-dvd): Matching, all 52 functions exact. `cbForStateReadingPartitionInfo`
  matches since sdk-dvd reads the launch partition from the low-memory words `__OSLaunchPartitionType` (0x80003194)
  and `__OSLaunchPartitionOffset` (0x80003198, name invented) in `dvd/__dvd.h`, as xbret does (retail reloads 0x3194
  in the partition loop). The R17 findings of the sdk-io-2 review are
  cleared (rulings "DVD rulings (sdk-io-3)"); the checker reports no [match] finding in the unit. Open ([checked]/[free]):
  22 R25 `()` parameter lists in C (tp's spelling of the state functions), the bare boot magics in `DVDInit` (R11). 42 of the unit's functions were
  renamed from `fn_` by order and size against tp's `dvd.c`, and the lane renamed the `dvdlow.c` functions around
  it the same way (spot-checked at review: `DVDLowRead`, `DVDLowSeek`, `DVDLowInquiry`, `DVDLowStopMotor`,
  `DVDLowAudioBufferConfig`, `DVDLowSetMaximumRotation` and `DVDLowGetNoDiscBufferSizes` issue the IOS DI ioctls
  0x71, 0xAB, 0x12, 0xE3, 0xE4, 0xDD and 0x92, each the one its tp name sends).
- `revolution/os/OSNet` (sdk-os-5): all eight functions exact and the DOL hash holds with the unit linked from source;
  the five caller-name `.data` literals are MWCC `__FUNCTION__` objects (the sibling's `OSNet.o` names them
  `__FUNCTION__$NNN`, one per caller), emitted because each resource-manager helper reads `callerName` where the
  reference reports a failure (R17 row above; `NWC24ResumeScheduler`, dead-stripped in retail, owns the fifth). Parked
  only on `-inline noauto` (passage above): this unit alone evidences it and it breaks other `os` units, so the user's
  ruling of 2026-11-16 keeps the unit parked until evidence for the setting appears.
- `revolution/os/OSSemaphore`: `OSWaitSemaphore` 89.1%. Retail reloads `sem->count` after the wait loop for
  the decrement (`lwz r4, 0x0(r29)`); the source keeps the count from the loop test. `volatile s32 count` closes it
  but is rejected as a fakematch: the SDK's `OSSemaphore` declares a plain `s32 count` (zeldaret/tp
  `libs/revolution/include/revolution/os/OSSemaphore.h`, `s32 count;`). The form was tested by editing
  `include/revolution/os/OSSemaphore.h`, which is unchanged; the live note is in `src/revolution/os/OSSemaphore.c`
  (the unit source), not in the header. `while (sem->count <= 0) ...; count = sem->count--;` and library-wide
  `-ipa file` do not change it; neither do `sem->count = sem->count - 1`, `-= 1`, `OSGetSemaphoreCount(sem)` in
  the test or in the decrement, `-opt no{cse,prop,lifetimes}`, `-ipa` off with `-inline deferred`, every installed
  Wii compiler, or the `global_optimizer` pragma. The other three functions are 100%.
- `libjpeg/jmemmgr`: `alloc_small` 93.9%: retail recomputes `min_request + slop` at each use, the build keeps it in
  one more saved register. Library-wide `-ipa file` and removing the functions after it do not change it. A permuter
  run (about 44k iterations) reached 2/114 rows with a `(long)` cast on one operand of one sum; IJG 6b has no cast
  there, so the state was rejected and not committed (permuter record below). The function is the stock IJG 6b text (checked against the jpegsr6b release: identical but for whitespace); unprototyped `jpeg_get_small`, `unsigned int` locals, every Wii/GC compiler, `-O2`..`-O4,s`, `-ipa function`, `-inline deferred/smart/all` and the `-opt no*` switches do not change the merge. `first_pool_slop`/`extra_pool_slop` are named in symbols.txt. The other 13 functions are 100%, `jinit_memory_mgr`
  included (its `getenv` call now targets `getenv` in `MSL_C/MSL_Common/Src/environment`).
  The three-term spelling of the pool-size sum is ruled out by the user's ruling of 2026-11-16 (AGENTS.md, "Surviving
  source against a lever"): retail's two adds at 0x801C50E4 and 0x801C50F0 are IJG's own `+= min_request + slop`, so
  the unit stays parked on IJG's text.
- `revolution/gx/GXTransform` (sdk-gx): 14 of 15 functions 100%. `GXLoadTexMtxImm` 89.6%: the build hoists the cheap
  else-arm of each selection (slot address, word count) above its compare where retail keeps two-armed branches (unit
  `.text` 8 bytes short). ogws and xenoblade match only with `(u64)` casts on those arms (a fakematch); the spellings
  and flags tried are listed at the function. When the unit links, `__GXSetProjection` is MATCH-PENDING (the `wgpipe` temporary, ruling
  "GX rulings (sdk-gx)") and `LoadProjPS` stays a provisional listing.
- `rwsdk/src/pipe/p2/p2define`: 12 of 13 functions exact; `RxLockedPipeUnlock` 99.79% (13 rows), wall
  register/schedule, in the inlined `CalcUnlockPersistentMemSize`: retail keeps `depChaseBlockSize` in r0 across the
  loop (the loop's temporaries in r4/r5, `numClusters` reused for the tail) where the source gives it r5. Statement
  order follows Gormiti's Debug line table (another revision: its `CalcUnlockPersistentMemSize` adds 7 per
  private-data block, retail does not). Tried: splitting the partial sum into another size variable, `x = x + f()` and
  `f() + x`, folding `unlockStartBlockSize` into `topSortBlockSize`, the tail expression's operand order, `levers.exe`
  (136 single edits).
- `rwsdk/world/native`: 7 of 8 exact; `_rpNativeWrite` 98.22% (5 rows), wall register/schedule, in the inlined
  `_rpNativePointer2Offset`: the loop over the attribute arrays takes two induction pointers in retail (`lwzu` on the
  load, a separate store pointer from `&attr[0]`) where the source gets one; the identical loop of the inlined
  `_rpNativeOffset2Pointer` has one in retail too. Retail's revision differs from Gormiti's (`_rpNativeWrite` pairs
  by size only). Tried: pointer, cast-lvalue and `-=` spellings, operand order, a local for the array or the record,
  a dead re-read of the field (the Debug build has an address-only expression after the store), `levers.exe` (21
  edits).
- `fmod/fmod_codec_dsp`: 7 of 8 exact; `CodecDSP::setPositionCallback` 96.15% (25 rows, 2 target mismatches), wall
  register/schedule. The format test of the inlined `setPositionInternal` written as `||` on `waveformat->format` gives
  retail's blocks (two signed compares sharing one `FMOD_ERR_FORMAT` block), but the source reloads the format for
  the inlined `SoundI::getBytesFromSamples` where retail keeps the first load in r9. Every spelling that holds the
  format in a variable (a local, an `int` local, an inline helper's parameter by value or through the pointer; `||`,
  `&&`, `!`, `if`/`else if`, statement-split returns, `?:`) is merged into one unsigned compare (`cmplwi 10`).
- `game/CPointOfInterest`: 10 of 11 functions exact; `fn_800C5320` 91.51% (12 rows), wall register/schedule: one
  consistent renaming (retail holds the matrix in r5 and the address of `toPoint` in r7, the build the reverse) and
  the place of the `stwu`. Call set and control flow are retail's. Tried: scalar and operator forms of `centre` and
  `toPoint`, the declaration order of the three locals, an early return, a named matrix local.
- `game/CButtonPuzzle`: 20 of 21 functions exact; `fn_8009CB1C` (the message handler) 99.60% (15 rows), wall
  register/schedule: `tweaks` and `record` take r30 and r29 where retail has r29 and r30 (MWCC's Simplify tie-break).
  The only matching state wraps the body after the base call in an inline helper holding the state, `tweaks` and
  `record`; it is rejected as fake code (R17) and the handler is left unwrapped. Also ruled out: every declaration
  order and form of the locals, `record` as a pointer or taken at each use, `record` bound to `lbl_80491B38`
  directly, the event-id conversion in the message test (neutral here), `-ipa file` dropped, `-O4,p`.
- `game/CAmbientInterferencePoint`: 13 of 14 functions exact; `fn_800C2304` 99.94% (2 rows), also MATCH-PENDING under
  R18 (table above). Residue: the order of `FacingDot`'s two address computations (retail computes `&delta` first;
  pointer or reference parameters, register locals in either order, `V3dDot( &facing, &delta )` and a
  reference-parameter wrapper do not move it). The unit's `.sdata2` constant order also differs ("CAmbientInterferencePoint
  data residues (game-3)").
- `fmod/fmod_eventimpl_simple`: 50 of 54 functions exact, four open (walls in the functions' headers). `update` 99.95% (5 rows, register/schedule): in the
  `EVENTI_FLAG_0x40000` update retail numbers the `oneshotsplaying` mask before the `m_eventi` load of the clearing
  statement; `flags = mask | flags` and ternary spellings leave it or lose more. `updateSound` 99.93% (2 rows,
  register/schedule): the first volume product lands in f1 where retail uses f0; every association and statement split
  of the product, and a ternary for the mute test, leave it. `createStreams` 99.87% (3 rows, control flow): retail
  materialises the software test as 1, then 0 when both tests fail, the source sets 0 first for every expression form
  and in every installed compiler build; a named, twice-assigned `bool` gives retail's order but renumbers every
  callee-saved register (97.7%). `buildSoundBankList` 98.67% (87 rows, register/schedule, no CFG or call difference):
  the two `NULL` terminator stores take their constant before the address chain in retail, the reference-count merge
  loads `sounddefrefcnt_array[0]` before `bankrefcnt_array[j]`, and the unrolled bank-name loop colours the
  `m_soundbank` and `m_soundbankname` bases differently.
- `fmod/fmod_eventgroupi`: 32 of 36 functions exact, four open (walls in the functions' headers).
  `buildSoundBankList` 99.25% (25 rows, register/schedule): the merge loop colours its values differently (retail:
  entry pointer r3, `event_node` r7, `eventbankindex` r6); inner pointers in the other order, the increment moved and
  the indices declared at the top were tried. `stealEventInstance` 95.77% (95 rows, 1 target mismatch, struct temporary
  and register order): retail copies `m_initialdspclock` to one stack temporary per read; `getStartClock` by value gives
  two, and only a same-type functional cast `FMOD_UINT64P( ... ).mHi` gives one (97.9%, rejected as a carrier); the
  named `instance` local changes the colouring of the instance pointer, which Gormiti's DWARF does not list.
  `getEventInstance` 99.33% (8 rows, register/schedule): retail keeps the chosen instance in r4 for the callback test
  and reloads it for the call; a nested test, const accessors, a ternary and a named result were tried. `load` 98.46%
  (75 rows, register/schedule, no call-set or CFG difference): the subgroup loop colours its pointer, the new group and
  the count differently, and the three category-list insertions compute the list head before reading its `prev` link;
  Gormiti's copy is another revision (0x148 bytes longer).
- `climax/ClimaxT1Effect`: 14 of 15 functions exact; `ClimaxT1AtomicGetWorldSpaceBounds` 97.20% (25 rows), wall
  register/schedule (control flow isomorphic): retail holds `skinMatrix`, `GeomData` and the bone offset in r31, r30 and
  r29 and loads the bone's radius address before its bone index. Ruled out: all 120 declaration orders of the five outer
  locals, the order of the Radius and BoneID reads and of the Min and Max statements, `BoneID` inlined into the call,
  const locals, `++Ind`, the Debug build's dead `int first = 1;`. `RpGeometryGetData`'s retail copy is another revision
  than Gormiti's.
- `climax/ClimaxS1Lib`: 7 of 8 functions exact; `ClimaxS1DataGetSize` 99.26% (2 rows), wall register/schedule: retail
  adds the array's byte count first, then `size`. The current spelling reproduces Gormiti's `-O0` code; every
  accumulation spelling, the accessors in the Debug form, the accessors' definition order and every compiler version
  (`flagsweep`) are ruled out.
- `fmod/fmod_dsp_resampler`: 8 of 9 functions exact; `DSPResampler::execute` 99.56% (11 rows), wall register/schedule: in
  the two position-wrap tails retail reloads `mNoDMA` after the loop-flag test. Reading `mNoDMA` through a pointer to
  const `DSPResampler` in either tail makes it exact (a const read is its own expression to the common-subexpression
  pass, as in the display-height accessor ruling), but no spelling for it is evidenced (Gormiti's DWARF lists no inline
  member of `DSPResampler`). The user's ruling of 2026-11-16 (AGENTS.md, "Const-qualified reads") rules the const read
  a lever here, since no const accessor is evidenced: the function stays open. The body carries two spellings that
  Gormiti's DWARF does not list: a `loopstart` local on the backward path and a `readpos` local in the fill loop.
- `fmod/fmod_dsp_connectioni`: 11 of 12 functions exact; `DSPConnectionI::setPan` 99.77% (20 rows, 0 target mismatches),
  register only: in the QUAD/SURROUND/5POINT1/7POINT1 and PROLOGIC two-channel arms retail holds `1.0f` in f2 and the
  pan factor `l` in f3 where the build has f3 and f2 (the 1-in-channel and `l`/`r` swap rows). Retail's switch carries a
  case label 1000 (`cmpwi 0x3E8`, same arm as STEREO) that Gormiti's `FMOD_SPEAKERMODE` DWARF does not list:
  `FMOD_SPEAKERMODE_STEREO_LINEAR` is an invented name for it, marked at the code. `DSPConnectionI::checkMono` is a
  header inline (Gormiti's DWARF shows its locals `count` and `count2` in `mix`); the `FMOD_DSP_Connection_*_SIMD`
  prototypes are declared in `fmod_dsp_connectioni.h` and defined in `fmod_dsp_connection_wii.cpp`.
- `rwsdk/src/pipe/p2/p2dep`: 10 of 11 functions exact; `fn_8029D500` (Gormiti's `_PropagateDependenciesAndKillDeadPaths`,
  unpaired) 99.66% (14 rows, 0 target mismatches), register only: in the first loop retail numbers the volatile registers
  r5/r6/r7/r8 for the duplicate-cluster scan one step differently from the build. The unit has eight inline statics
  that retail does not carry (`_ReqCreate`, `_ReqSearch4Cluster`, `_ReqDeleteEntry`, `_IoSpecSearch4Cluster`,
  `_ScopeTraceCreate`, `_ScopeTraceAddEntry`, `_EnumPipelineClusters`, `_CountHeadNodeRqdsAndOpts`), each marked
  `// no copy in retail (inlined)`; `ReqGetEntry` is a macro of this unit.
- `rwsdk/plugin/skin2/rpskin`: 26 of 27 functions exact; `SkinGeometryRead` is open, register wall: the inlined
  `SkinFindMaxWeights` takes its two loop counters in swapped registers. `SkinAtomicRead`'s copy of the same inline is
  exact; declaring `j` before `i` in the helper closes `SkinGeometryRead` and opens `SkinAtomicRead`'s, so neither
  form is kept.
- `climax/Localisation`: 7 of 14 functions exact; `fn_801D197C`, `unk_StringTable::fn_801D1CFC`, `fn_801D207C`,
  `fn_801D22E0`, `fn_801D2554`, `fn_801D2630` and `fn_801D26E0` are open (93.0% to 98.6%); reason not yet recorded
  (no function header in the source gives one). The source carries spellings that move scores but are not yet
  justified by evidence and must be checked against the reader test before any of them counts: the `bool loaded`
  temporary in `fn_801D26E0`, the `hdr` copy of `header` and the late `swapped`/`i` declarations in
  `unk_StringTable::fn_801D1CFC`, the late `strings` declaration in `fn_801D197C`, `header = NULL` written at the two
  call sites instead of in the constructor, and the dropped `text = NULL` before the search loop of `FindString`.
- `fmod/fmod_musicengine`: scaffold only (0 of 38 functions); unit name and boundary from the Gormiti link map
  (`fmod_event.a fmod_musicengine.obj`, 0x803AA228-0x803AC1EC), parked and linked from retail asm.
- Invented names of the `integrate/antigravity2` units (marked at the code or the declaration): `CHANNELI_FLAG_0x1`,
  `CHANNELI_FLAG_0x2`, `SOUNDI_CHANNELMASK_ALLMONO`/`_ALLSTEREO`, `EVENTCATEGORYI_MAXPLAYBACKS_*`,
  `EVENTCATEGORYI_PATH_LENGTH`, `EVENTCATEGORYI_FILEVERSION_MAXPLAYBACKS`, `EVENT_CATEGORY_MASTER`/`_MUSIC`,
  `EventCategoryI::getCategoryPtr`, `FMOD_SPEAKERMODE_STEREO_LINEAR` (value 1000 is retail's), `CHANNELREAL_FLAG_0x80`.
- `fmod/fmod_dsp_convert_wii`: 0 of 1 exact; `FMOD_DSP_Convert_FloatToPCM16` 92.00% (68 rows, 2 target mismatches):
  the structure (unrolled by four, remainder loop, clamp shape, spill slots) is retail's; the strided source and
  destination pointers land in other registers. Parameter and local names other than `destptr`, `srcptr` and `len`
  are invented.
- `fmod/fmod_os_misc`: 24 of 24 functions exact, not linked: `gatecheck layout` reports a `.data` difference on the run
  of `__FILE__` literals (retail's object at 0x8044E3B8, 0x3C bytes, has no built counterpart, and three built literals
  have no retail counterpart). Open until the cause is found.
- `game/unk_8000A040`: 17 of 20 functions exact, all three open functions complete. `fn_8000A76C` 99.86% (10 rows): the
  constant pool needs the x line before the y line (320.0 before 0.5); written x-first only `f28`/`f29` and one stack
  slot differ (99.77%). `fn_8000AF08` 90.58%: retail keeps the table base in the row register and folds the group
  offset into the loads where the build materialises the row. `fn_8000B5DC` is under R18 (table above).
- `game/CSequencePuzzleLogic`: `fn_8000CFCC` 98.41% (24 rows): retail loads `end()` before `begin()` but keeps the stack
  slots of `end() - begin()`; the other open functions state their walls in their header comments.
- `game/CDrawingTest`: `fn_801796F8` 99.47% (11 rows): only the floating-point registers of the texture-coordinate block
  remain (retail loads 0.2666 and 0.2 before the subtraction and adds 0.5 as the first operand).
- `game/unk_8001205C` (unit name unconfirmed: no string names the unit or its class; the start-node group, its boundary from the unit plan only): 2 of 4 functions exact (`fn_800122A4`, `fn_800122C0`); `fn_8001205C` 98.15% and `fn_800121A0` 98.15%, the path-A counter loads of both (retail hoists the this copy among them; register/schedule), and `fn_8001205C`s start-index store base register. Not linked.

## Game code rulings (game-5)

**Units.** Linked (clean gate `main.dol: OK`, checker clean, every function 100%): `game/CCounter`
(0x800C8CB0-0x800C90DC, 7 functions), `game/DataTrigger` (0x800C90DC-0x800C9400, 7; the class is registered without a
C prefix), `game/CFadeTrigger` (0x800C984C-0x800C9CA8, 7), `game/CFlickerTrigger` (0x800C9CA8-0x800CA038, 7) and
`game/CScreenShakeTrigger` (0x800CF1AC-0x800CF530, 7). `force_active`: `lbl_80529AE4`, `lbl_80529AEC`, `lbl_80529B04`,
`lbl_80529B0C`, `lbl_80529BCC` (class registration objects nothing else names). Matched under the rules +35.

**CCounter's debug strings come from inline helpers, emitted in reverse definition order (ruling: authentic).** The
four strings in `.data` are the `@STRING@<helper>` pools of four inline member functions (`Reset`, `Count`,
`CountReached`, `Send`; names invented), and MWCC lays them out in the reverse of the order the helpers are defined in
the source. The definition order in `CCounter.cpp` therefore follows the strings, not the call graph; `symbols.txt` names
the pools `@STRING@<helper>`. The `NULL` passed for each `%s` is retail's (`li r4, 0`): the owner-name argument of the
debug print, whose source is unknown. Each helper is called from the handler only.

**CDesignerDebugMsg matches (`game/CDesignerDebugMsg`, `Matching`, clean gate green).** `fn_8009CB1C` (0x800C94C8) matches with each fade channel scaled by `(u8)(int)fade` (the `int` step keeps retail's `clrlwi` after each conversion; `(u8)fade` alone drops it), the channels scaled red, blue, green, alpha, and the colour declared after the text box (retail loads its constant word after the box values). Labelled original bug (R36, at the code): `0.04f * framesLeft` is below 1 throughout the fade, so the integer factor is 0 and the text vanishes when the fade should start. Registration object `lbl_80529AF8` is under `force_active`.

**Reader-test review (passes).** CFlickerTrigger's `fn_8009CB1C` computes `triggerName` and `sendName` into locals; they
are the two string arguments of the debug print, named values, kept. CFadeTrigger's `field_0x4C` is set by the packet
and never read by the unit (a member of the object; the header labels it as never read by the unit). CScreenShakeTrigger fills `unk_ScreenShakeData` in retail's store order (4, 0, 8, 0x10, 0xC);
the payload struct is used as a whole. CFadeTrigger's `fn_8009CB1C` does not call the base handler (as retail).

**Shared headers (R1/R2).** `game/Log.h` (header name invented) now declares `fn_80028D0C`, `fn_8002438C` (moved from
`CGenericSpawner.h`) and `fn_800A65B0__FPCce`. `Messages.h` gains `lbl_8052A7A0` and `lbl_80529E70` and holds
`lbl_8052A828` (moved from `CSplashScreenTrigger.h`). `climax/AttributeHandler.h` gains `unk_AttributeCommand::GetRGBA`.
New headers `climax/Colour.h`, `climax/Font.h` and `climax/Fade.h`: names invented. Every existing unit keeps its bytes
(clean gate and `ninja changes`: no edited function fell).

**Open items (game-5).** Invented names: the headers `game/Log.h`, `climax/Colour.h`, `climax/Font.h`,
`climax/Fade.h`; `unk_RGBA`, `unk_Font`, `unk_TextBox`, `unk_FlickerParams` (member names from the debug format string),
`unk_ScreenShakeData`; CCounter's helpers `Reset`/`Count`/`CountReached`/`Send`; CDesignerDebugMsg's `Hide`/`Show`/`Draw`;
the attribute-command enums and member names of the six classes. Unit names evidenced by registration strings
(none unconfirmed).

## Game code rulings (game-7)

**Units.** Linked (clean gate `main.dol: OK`, checker clean with R13 slot checks, every function 100%):
`game/CPlayerStateTrigger` (0x800CE684-0x800CE988, 7 functions) and `game/RespotZoneTrigger` (0x800CEEA4-0x800CF1AC, 7;
registered without a C prefix, like DataTrigger). `force_active`: `lbl_80529BAC`, `lbl_80529BC4`. Parked:
`game/CPhoneMessageTrigger` (0x800CD998-0x800CDD50; see "Parked"). Unit names evidenced by registration strings. The
docs/game_units.md rows put the behaviour units' boundaries one row late: each unit is its class functions, then its
static initialiser, then the inline factory and the two `@36@` thunks, so CPlayerStateTrigger starts at 0x800CE684
(not 0x800CE8A0) and CPhoneMessageTrigger at 0x800CD998.

**CPhoneCallTrigger not split (0x800CD374-0x800CD998).** Its `fn_800CD830` (called from 0x800F494C) makes a virtual call
through slot 0x1B4 of the player (`lbl_80491B38.field_0x14`), which needs the player class's vtable declared first (R13).
The split was made and withdrawn in this lane.

**CPlayerStateTrigger's flag (ruling: authentic).** `field_0x40` is set with `if ( it->GetU32() ) { ... = true; } else
{ ... = false; }`: retail branches and stores constants held in r30/r31; `= it->GetU32() != 0` (CFadeTrigger's spelling)
and `? true : false` give the branch-free `neg/or/srwi` form.

**Shared headers (R1/R2).** `Messages.h` gains `lbl_805294E0` (CPlayerStateTrigger posts its state payload to it). New
header `game/unk_RespotManager.h`: `unk_RespotManager` (name invented, from RespotZoneTrigger's format string) with
`fn_80030F34` (renamed `fn_80030F34__17unk_RespotManagerFPCcPCcPCc` in `symbols.txt`) and `lbl_80529260`.

**Open items (game-7).** Invented names: `unk_RespotManager`, the header `unk_RespotManager.h`, and the attribute-command
enums and member names of the three classes. CPhoneMessageTrigger's `fn_8009CB1C` register residue (closed in game-8).

## Game code rulings (game-8)

**Units.** Linked (clean gate `main.dol: OK`, checker clean with R13 slot checks, every function 100%):
`game/CPhoneMessageTrigger` (0x800CD998-0x800CDD50, 7 functions; `force_active` `lbl_80529B9C`) and
`game/CVoicemailMessageTrigger` (0x800D06F8-0x800D0AB0, 7 functions; `force_active` `lbl_80529C04`; class name from the
registration strings `lbl_803ED1E8`/`lbl_803ED204`; vtable `__vt__24CVoicemailMessageTrigger`). docs/game_units.md rows
corrected (the voicemail unit was listed as an unnamed 800D06F8-800D0990 row plus a one-function name row).

**Phone triggers send through an inline member with a named local (ruling: authentic).** Retail numbers the five
inlined `unk_MessageInfo` constructor parameters in reverse in both phone triggers (0x4C r5 .. 0x40 r8), while
CAmbientTextMessage/CAmbientVoicemail number them forward. The form that gives retail's registers is an inline member
defined in the class that builds a named `unk_MessageInfo info( ... )` and passes it to the inbox (`SendTextMessage()`,
`SendVoicemail()`; names invented), called from the message handler. It matches both units on the first compile. Not
equivalent: the same body inlined directly in the handler, a named local in the handler, a static helper taking the five
values (forward order), and a helper building the temporary in the call without a named local (reversed, but the first
word moves out of r0). Mechanism in docs/compiler_regalloc.md, "Inlined locals". CPhoneCallTrigger's nine-word call
(0x800CD5B4) uses the same shape (`StartCall()`, a named nine-word `unk_CallInfo`).

**Open items (game-8).** Invented names `SendTextMessage`, `SendVoicemail` and CVoicemailMessageTrigger's attribute-command
enum and member names; CPhoneCallTrigger's below.

**CPhoneCallTrigger (linked; 0x800CD374-0x800CD998, 9 functions).** The game-7 blocker is cleared by declaring the
player: `lbl_80491B38.field_0x14` is `CPlayer*` (registered as "CPlayer"; vtable `lbl_803E5F38`, 0x25C bytes; size
0x2368), with its chain `CBaseBehaviour` <- `unk_8009D2E4` <- `unk_8001777C` <- `unk_8003C454` <- `CPlayer`, every retail
slot declared (placeholder names; pure slot 0x160 `fn_80072D14() = 0`) and data as padding only. Slot 0x1B4 is
`unk_8003C454::fn_80040D4C( float, int )` (signature from the call site and the callee's stores). R13: 14 classes
matched, 0 added, 0 missing; `unk_CPlayerBase700` and `unk_CPlayerBase704` (CPlayer's interface bases at 0x700/0x704,
no standalone vtable, names invented) are reported unmatched, not blocking. No `force_active`: `fn_800F4824` (linked
asm) references `fn_800CD830`. Quirks kept: `unk_CallInfo`'s constructor takes and assigns its three texts in the
order 0x54, 0x5C, 0x58, and the destructor deletes them in that order (any other order leaves the handler at 99.8%);
the inbox is null-checked through a local pointer (`if ( inbox )`), as retail tests it; the constructor takes the
attribute packet unused and is defined out of line (inline, it folds into the factory).
Open items: invented names `unk_CPlayerBase700`, `unk_CPlayerBase704`, `unk_CallInfo`, `StartCall` and the class's
member names; the intermediate classes' padding sizes (`unk_8009D2E4` 0x144, `unk_8001777C` 0x1BC, `unk_800D2DE4`
0x3B4, `unk_8003C454` 0x700) are only as exact as the offsets they force; `fn_80040D4C`'s parameter types.

## sdk-bte-3 rulings

- `btm_main.c`, `btu_init.c` and `port_api.c` are Matching (user's approval, 2026-10-23; clean gate green for each
  flip). Their MATCH-PENDING rows were removed: the rule-checker fix of main 69bd643 resolved the R4 findings on the
  Broadcom protocol constants (`BTM_CLOCK_OFFSET_VALID`, `RFCOMM_MIN_OFFSET`, `RFCOMM_PF_OFFSET`, `L2CAP_MIN_OFFSET`,
  `L2CAP_SEND_CMD_OFFSET`), and the three units have no [match] findings.
- `gap_api.c` (`GC/3.0a5`, R4QK01 reference) is Matching (clean gate green). `GAP_Init` is the one retail function;
  the other 24 keep the reference body (`// no copy in retail`). `gap_cb` (`.bss` 0x804EA220, 0x3AC; the old size
  0x3B0 included alignment padding) is named and declared `extern` in the new `gap_int.h`; its owner in the
  reference, `gap_utils.c`, defines it (sdk-bte-4). `gap_api.h` grew to the reference's types and declares every `GAP_*`
  function once (the reference header declares only `GAP_Init`); the reference's self-referential
  `#define GAP_EVT_CONN_CONGESTED (GAP_EVT_CONN_CONGESTED + 0)` lines (and `GAP_EVT_CONN_UNCONGESTED`'s) are kept inside the enum, as in `sdp_api.h` and `btm_api.h`: Broadcom's own
  spelling, unused by any function, no checker finding, no byte change.
  The local `extern BTM_StartDiscovery` in the reference's `gap_api.c` is dropped (`btm_api.h` declares it).
  `BCM_STRNCPY_S` (Broadcom's, from the reference's `data_types.h`) added to `data_types.h`.
- `hidd_api.c` and `hidd_mgmt.c` (`GC/3.0a5`, R4QK01 reference) are Matching (clean gate green for each flip).
  Retail keeps one function of each (`HID_DevInit`, `hidd_proc_repage_timeout`); 10 and 6 functions keep the
  reference body (`// no copy in retail`). `hd_cb` (`.bss` 0x804EA5D0, 0x144; the old size 0x148 included alignment
  padding) is defined in `hidd_mgmt.c`, its reference owner, and declared `extern` in `hidd_int.h`. New headers
  `hiddefs.h`, `hid_conn.h`, `hidd_int.h`, `wcassert.h` (all the reference's); `hidd_api.h` grew to the reference's
  content. The self-referential `#define HID_SEC_CHN (HID_SEC_CHN + 0)` / `HID_NOSEC_CHN` lines of `hid_conn.h`
  are kept inside the enum (the reference's spelling, as above); the reference's `decomp.h` `ARRAY_LENGTH(x)`, which is
  `sizeof(x) / sizeof(x[0])`, is written out that way (two uses, both in dead functions).
- Data scores: `hidd_proc_repage_timeout`'s trace string is `lbl_80443E38` in retail and `@NNN` in the build. objdiff
  pairs `.data` strings by position and the build also holds the dead `hidd_mgmt_process_evt` string ahead of it,
  so the one-shot diff shows 99.8% (the report's code score is 100%, `.data` 91%) while the bytes and the gate
  match. String-pool naming only, as in the NAND "Data scores" ruling; the reference's function order is kept.
- Resolved (dead-stripped body): `HID_DevRegister` (no copy in retail) keeps the reference's local `conn_able`, set
  in both branches of the host-address test and never read. The user ruled (2026-10-23) that R17 does not apply
  inside a `// no copy in retail` body (the reference's text, not matched code); the checker skips R17 there. It
  changes no retail byte.

### Game code rulings (game-9)

Quirks labelled at the code (R36, comments only, bytes unchanged): CPhoneCallTrigger's texts are taken, stored
(`unk_CallInfo`) and deleted in the order 0x54, 0x5C, 0x58; its inbox is null-checked through a local pointer although
a member's address cannot be null; its constructor takes the attribute packet unused. RespotZoneTrigger's two log
calls pass a literal NULL for the first `%s` (retail `li r5, 0` before each `fn_80028D0C`).

Units split by the game-7 cut rule (class functions, sinit, inline factory, `@36@` thunks), names from the registration
strings: `game/CKeyHUDTrigger` (0x800CB1C8-0x800CB4F8, 7 functions, linked; force_active `lbl_80529B3C`),
`game/CModalTutorialMessageTrigger` (0x800CBEF8-0x800CC4E4, 11 functions, linked; no force_active: `fn_800CBEF8` and
`fn_800CC25C` are called from outside) and `game/CMessageRelay` (0x800CB4F8-0x800CBEF8, 9 functions, linked; force_active `lbl_80529B44`).
CGhostPhotoTrigger's unit is 0x800CA304-0x800CB1C8: its factory 0x800CB168 is followed by three thunks (0x800CB1C0 is a
second `b` to its destructor), and 0x800CA280-0x800CA304 (a factory and two thunks) closes the unit of the sinit at
0x800CA1F4 (docs/game_units.md rows corrected).

- Shared headers: new `game/SaveData.h` (name invented, after the "SaveData" group string) takes the save-data
  declarations out of `CAmbientInterferencePoint.h`; `fn_80205354`'s value is `const void*` with a type code and a byte
  size (CAmbientInterferencePoint writes type 1 with one byte, CKeyHUDTrigger type 2 with four), symbol renamed to
  `fn_80205354__FPvPCciPCvi`. New `game/unk_HUDController.h` (names invented): the object at 0x6C4 of
  `*lbl_80529D20` (`unk_InboxOwner::field_0x6C4`) with `fn_800E4148`, `fn_800E588C` and `fn_800E611C`; the float's
  place among `fn_800E4148`'s parameters is invented (the mangled name follows it). `lbl_803E6400` ("TransitionIN",
  shared with `fn_80065914`) is declared there, defined elsewhere. New `climax/unk_FileLoadCallback.h`, `game/unk_Settings.h`
  (`fn_800C770C`, moved out of PadRumble.cpp); all names invented. (`game/Random.h` was retired: the generator is MSL
  `rand` in `MSL_C/MSL_Common/Src/rand.c`, declared in `stdlib.h`; "MSL rulings (msl-3)".)
- CKeyHUDTrigger copies its flag into a local before passing its address to `fn_80205354` (retail stores the field at
  0x8(r1)), as CAmbientInterferencePoint does.
- The class registry `unk_801A8F6C` is an empty class: CModalTutorialMessageTrigger's static flag `lbl_80529B59` sits one
  byte after its registry object `lbl_80529B58`. The 28 registry objects in `symbols.txt` are `size:0x1` accordingly.
- CModalTutorialMessageTrigger: `unk_TutorialText` (three owned texts, inline destructor and copying `operator=`) is
  shared at 0x3C of the trigger and 0xC of the singleton `unk_TutorialMessageBox` (`lbl_80529B50`, size 0x18, bases
  `unk_BehaviourBase` and `unk_FileLoadCallback`). `unk_FileLoadCallback::fn_800CC364` is declared non-pure: with all
  its virtuals pure MWCC emits a weak base vtable into this unit's `.data`, which retail does not have. The box's R13
  findings are a checker limitation, admitted by review ruling (review-rulings table).
  `CPlayer` gains `field_0x2366` (nonzero: modal messages are not shown) inside its padding; size unchanged.
- CMessageRelay: after its `fn_80206348( ..., "SaveData" )` test `fn_8009CB1C` compares `field_0xC4` with zero and
  then tests a constant true (`li r0, 1; cmpwi r0, 0`); the same sequence occurs at 0x800C62DC, 0x800D1468, 0x800D1524
  and 0x8016FF64. It is the release-compiled-out debug check `CheckSaveDataKey` (game-11 ruling); the `switch` spelling
  (`switch ( key != NULL ) { case 0: return true; default: return true; }`) is a fakematch and is not used. Quirks kept
  at the code: four flag commands fall through
  without `break`; the constructor ignores its packet and leaves `enabled` uninitialised; "Recieve" as in retail.
  Spellings the bytes need: the shuffle loop `for ( int i = 0; i != count; i++ )` with `u32 count` (retail `bne`), and
  the flag commands' `if ( it->GetU32() != 0 ) { x = true; } else { x = false; }` (as CPlayerStateTrigger, game-7).
  `lbl_803EC5A0` (0x98) is five strings, split in `symbols.txt`; one is `@STRING@Retrigger__13CMessageRelayFv`, a
  function-local literal proving an inline `Retrigger`.

Open items: recover the always-true save-condition helper across its five sites (CMessageRelay's blocker); invented
names `unk_HUDController`, `unk_FileLoadCallback`, `unk_TutorialText`, `unk_TutorialMessageBox`, `Random.h`,
`SaveData.h`, `unk_Settings.h`, `Retrigger`, `Shuffle`, `Swap` and the three classes' member and command names; the
meaning of setting index 11, player `field_0xC` and `field_0x2366`, and the int parameters of `fn_800CC364` and the
HUD controller's functions; `classes.exe` does not model a base without a vtable of its own (the box's R13 reading).

### Game code rulings (game-10)

- The always-true save condition (CMessageRelay's blocker, five sites listed in docs/game_units.md, "Always-true save
  condition") is a member function of each class: CPointTrigger keeps it out of line at `fn_800C63AC`
  (`lwz r0, 0x88(r3); li r3, 1; cmpwi r0, 0; blr`, called from CFlarePickup `fn_80148BD0`), next to the out-of-line
  save-data test `fn_800C63BC`. The inlined sites are that body.
- Mechanism (probes with `build/tools/cc.exe`, unit flags; `-O0` keeps the branch, `-O1` removes if-forms, `-O2`
  merges the switch's arms): the optimiser's if-then-else simplification removes a test whose arms are already equal
  (`if ( k ) return true; return true;`, `k ? f() : true`, `k == NULL || f()` with an inline `f` returning true, every
  two-armed assignment) before code generation, so no compare survives. The compare survives only when the non-null arm
  becomes the constant in a later pass: then code generation emits `li 1` in both arms, the arms are merged and the
  branch dropped, and the compare stays dead. Forms that do this standalone: a two-way `switch` on `k != NULL`;
  `if ( k == NULL ) return true; bool ok = true; ok = ok && f( k ); return ok;` (`f` inline, returning true);
  `ok = ok && true`; a counter (`n += 1; return n > 0`); an empty `for` loop whose exit value is returned. A single-arm
  form (`bool ok = true; if ( k ) ok = f( k ); return ok;`) keeps a second `li` in the arm.
- None of these one-level forms is adopted: each reproduces the shape only through code no programmer would write in
  that form (an always-true stub, a self-and, a dummy counter). In CMessageRelay every one-level form swaps the key and
  the constant between `r3` and `r0`. The adopted form is two levels deep (game-11).
- `fn_802057FC`'s third parameter is the type code, not a count: the reader passes it to `fn_80205878` where the writer
  `fn_80205354` passes its type to `fn_80205510`, and the fifth (`size`) sits where the writer's byte size does.

Open items: the names of CPointTrigger's, AreaTriggerBox's and the 8016FAD0 class's units.

### Game code rulings (game-11)

- CPointTrigger is 0x800C5FCC-0x800C6508 (linked; CFlarePickup and others call it, no force_active): its class functions, `__sinit_\CPointTrigger_cpp`, the inline factory
  `fn_800C6430` and three this-adjust thunks (`@104@` and `@68@` destructor, `@68@fn_801A73A8`). 0x800C552C-0x800C5FCC
  is a separate unit, the point manager `lbl_80529A88` (size 0x430, created by `fn_800C5604`, called from ten units;
  its `.data` is the list vtable 0x803EBAE8 and its `.sdata2` the floats 0x8052C718-0x8052C738, none used by
  CPointTrigger): `game/unk_PointOfInterestManager`, name invented, stubs only.
- CPointTrigger's first base is CPointOfInterest (`include/game/CPointOfInterest.h`, vtable lbl_803EBA7C): the base's
  attribute decoder (0x800C50E8) uses the class id lbl_80529A80, which the 0x800C5500 static initialiser hashes from
  "CPointOfInterest". CBaseBehaviour is the second base, at 0x44. Both bases' slot 0x18 is `fn_801A73A8`, so one
  override (0x800C6124) fills both. CPointTrigger's inline overrides (slots 0xC, 0x10, 0x14, 0x20) are emitted by
  CLookTrigger's unit (0x800C4E8C-0x800C4E9C, 0x800C4B04), which links first; symbols.txt carries their mangled names.
- The save-data test `fn_800C63BC` and the base's `fn_800C54EC` return `int` (a tail call, no bool conversion).
  `fn_800C63AC` and `fn_800C63BC` are ordinary members defined in the unit after `fn_800C62B0`: `-ipa file` inlines
  them there, and they stay out of line because CFlarePickup `fn_80148BD0` calls both.

- The constructor takes the frame's matrix through `RwFrameGetMatrix` (`include/climax/Frame.h`), an inline function
  taking the frame: written as the plain expression `&frame->modelling` (the SDK's release macro), or through a member
  accessor or a local, the matrix pointer and the zero swap registers. `fn_800C62B0` reads both message names into
  locals before the log call (retail computes msgA's first), as CFlickerTrigger does.
- The always-true check (game-10) is two levels deep. A member that tests the key and passes it to an inline helper,
  which tests the key again and returns the same constant either way, reproduces all three sites exactly: the
  out-of-line `fn_800C63AC`, its inlined copy in `fn_800C62B0` and CMessageRelay `fn_8009CB1C` (in-class or `inline`
  member, so no out-of-line copy there), including the inlined sites' key in `r3` and constant in `r0` that every
  one-level form swaps. Shape: `if ( key != NULL ) return Helper( key ); return true;` (or `if ( key == NULL ) return
  true; return Helper( key ) ? true : false;`), with `Helper` any of `if ( key != NULL ) return true; return true;`,
  `key != NULL ? true : true`, an enum or `static const` constant in both arms, or a defaulted parameter returned
  either way.
- Ruling (user's ruling, 2026-10-23): the two-level form is accepted as a release-compiled-out debug check, the inner
  helper true on both arms. The helper is `inline bool CheckSaveDataKey( const char* key )` in
  `include/game/SaveData.h` (the check always follows the save-data test `fn_80206348` declared there); the outer test
  is CPointTrigger's out-of-line `fn_800C63AC` and an in-class private `CheckKey()` in CMessageRelay and AreaTriggerBox
  (`fn_80206348( ... ) && CheckKey()`). Names invented. The caller passes the key field itself. Every site of the shape
  uses it, including CSequencePuzzleLogic's (0x8016FF64, key 0xDC, in its message handler, key in `r3`, constant in
  `r0`; game-12 rulings).
  Forms that do not give the shape (probes with `build/tools/cc.exe`, unit flags): one level with a
  late constant other than game-10's; the helper without the outer test; a helper without a parameter, or given a
  local copy of the key (folds); tautologies (`k || !k`, `k != NULL || k == NULL`, `k == NULL || true`); `const`,
  `static const`, enum, struct and array constants; distinct named states compared or switched on (`GetState( k ) !=
  STATE_BLOCKED`, no range propagation); int and bool return types alike.

- AreaTriggerBox is 0x800D1268-0x800D1934 (`game/AreaTriggerBox`, linked; force_active `lbl_80529C34`): two classes, `__sinit_\AreaTriggerBox_cpp`, the
  inline factory `fn_800D1890` and the two `@36@` destructor thunks. 0x800D0AB0-0x800D1268 is the base's unit,
  AreaTriggerBase (the 0x800D11B0 static initialiser hashes "AreaTriggerBase" into the class id of its attribute
  commands): its class functions, its `__sinit`, then its two `@36@` thunks and no factory (the class is abstract;
  `include/game/AreaTriggerBase.h`, stubs not yet written). AreaTriggerBox's `.data` starts at 0x803ED318 ("SaveData").
- AreaTriggerBox derives from an intermediate class that implements the box tests (slots 0x58-0x60) and leaves the enter
  and exit handlers (0x64, 0x68) pure: vtable 0x803ED3E8, destructor 0x800D15AC, never constructed. It is named
  AreaTriggerBoxLogic after the unit's second class-name hash (lbl_80529C38, which nothing reads); that the hash names
  this class is inferred. Its functions follow AreaTriggerBox's in `.text` and its hash follows AreaTriggerBox's
  registration in the static initialiser, so its definitions follow AreaTriggerBox's in the file; `-ipa file` inlines
  its attribute handler and destructor into AreaTriggerBox's.
- `RwV3dTransformPoints` is declared `extern "C"` in `include/climax/Frame.h`, RenderWare being C;
  the box's face table is a local `unk_RwPlane` array initialised from the compiler's `.rodata` copy (lbl_803D8A20),
  which needs a constructor-free vector (`unk_RwV3d`) to be brace-initialised.
- The two-level check (above) also gives both AreaTriggerBox sites (0x800D1468 and 0x800D1524, key 0xE8).
- `fn_800D1688` tests each face through an inline plane test returning `int` (RenderWare's RwBool is an int), with
  `int` locals: `int inside0 = InsidePlane( &faces[i], &local[0] );` (name invented). Inlining that helper gives the
  two face pointers (normals and distances) and the `r3`/`r0` order; with `bool` the two results swap registers, and
  open-coding the test gives one pointer. The project has no RenderWare 3.7 headers yet, so the return type is
  spelled `int`; it becomes `RwBool` (rwplcore.h, `typedef RwInt32 RwBool`) when they are added, as `unk_RwMatrix`,
  `unk_RwPlane` and `unk_RwV3d` become `RwMatrix`, `RwPlane` and `RwV3d` (same layouts, pad1..3 included).
- `lbl_80529C34` (AreaTriggerBox's registration object, the empty class `unk_801A8F6C`) is 1 byte in symbols.txt,
  not dtk's 0x4 (which ran to `lbl_80529C38`): with the padded size `gatecheck layout` paired it with the next object.
  The built objects sit at the retail addresses (clean gate green); layout matches with the corrected size.

Open items: the identity of `fn_80068614`; the point manager's reconstruction and
its `.sbss` pointer lbl_80529A88 (not yet in its split); the unit names `game/CPointTrigger` and
`game/unk_PointOfInterestManager`; the name AreaTriggerBoxLogic; the unit names `game/AreaTriggerBox` and AreaTriggerBase; the AreaTriggerBase unit's
reconstruction and its listeners (constructors 0x800D1934, 0x800D19EC, 0x800D1B7C, 0x800D1CC4).

### Game code rulings (game-12)

- CSequencePuzzleLogic is 0x8016FB50-0x80170EE4 (`game/CSequencePuzzleLogic`, NonMatching; class name from the registration
  and log strings): its class functions, `__sinit_\CSequencePuzzleLogic_cpp`, the inline factory `fn_80170E8C` and the two
  `@36@` thunks. 0x8016FAD0-0x8016FB50 (factory `fn_8016FAD0`, `new` 0x778 + CProjectorPuzzle's constructor, and six
  this-adjust thunks into 0x8016EE9C-0x8016F154) belongs to the CProjectorPuzzle unit before it, not split.
  The fifth always-true save-check site (0x8016FF64, key 0xDC) uses `CheckKey()`/`CheckSaveDataKey` as the other classes.
- Helper structure from the PS2 build (SLUS_218.99, same TU at 0x25F2D8-0x2607D0): `OnInput` 0x25FBB8, `Solve` 0x25FF40,
  `Save` 0x260028, `SetHistory` 0x260170 and the sequence check 0x25F9F8 are out-of-line .cpp functions there; the
  load and save slots (0x2605E0, 0x260600), `Load` 0x2606D0, `Update` 0x260620 (the delay count), `UnlinkInputs`
  0x260588 (called from the destructor), `CheckKey` and the save-data test are emitted after the static initialiser
  (inline). On the Wii: `OnInput`, `Solve`, `Load`, `Save` are .cpp members after the save slot (every call inlined,
  dead copies stripped); `SetHistory`, `UnlinkInputs`, `Update` are in-class. Evidence: the destructor matches only
  with `UnlinkInputs`; the load slot keeps `text` in a register only with `SetHistory` as its own inline level
  (0x3A8 bytes, as retail; the loop written out gives 0x3CC); the save slot writes its terminator through the
  running pointer, as PS2 `Save` does. `Load` cannot be in-class on the Wii: an in-class inline's literal is one
  shared `@STRING@` object emitted at the end of `.data`, and retail's load-slot literal follows the jump table.
- Literal mechanics (MWCC 4.3, `-str noreuse -ipa file`), measured: every inlined instance of a .cpp function gets its
  own copy of each literal, in the caller's pool; the function's own out-of-line copy gets another at its definition.
  The linker dead-strips unreferenced `.sdata` literals but keeps unreferenced `.data` literals (a dead "XYZ" in
  `.sdata` vanished from the DOL; the dead "Sends %s" in `.data` stayed). Retail's `.sdata` zero word, `.sdata2`
  second zero and the `.data` zero word before the vtable are padding (`gatecheck layout`: PADSIZE).
- The history is `unk_Deque< u8, 64 >` (`include/game/unk_Deque.h`, shared with unk_8000A040). The destructor is
  `DestroyAll(); FreeMap();` and `Free()` is the same two calls (the SGI STL split: the deque's destructor destroys the
  elements, its base frees the nodes and map); unk_8000A040's destructor stays 100% and the duplicated `Free` body
  with its RULE-EXCEPTION is gone. `push_back`, `unk_Find`, `operator*`, `operator==` were added for this class (names
  invented).
- `fn_802057FC` reads into `void*` (renamed `fn_802057FC__FPvPCciPvi`; the load passes a `const char**` with type 5, a
  string). `fn_800264AC` (a getter returning the word at 0x94 of the object at an attribute handler's field_0xC) is
  `fn_800264AC__FUl`, declared in `include/game/unk_800264AC.h`, taking the u32 handle until the class is known.
- Open (blocks the flip): retail `.data` holds five unreferenced literals after the save slot's "CSequencePuzzleLogic"
  (0x803FACDC..0x803FAD6C): "Sends %s" twice and "CSequencePuzzleLogic" three times. The dead copies of `OnInput`,
  `Solve`, `Save` and `Load` give four of them, in order; one "CSequencePuzzleLogic" is still missing (a fifth dead
  function holding the save key, or the key reached through one more out-of-line level on the load or save side). The
  PS2 build has no further user of the key (its only references: `Save`, `Load`, the registration), so the Wii save
  code differs from the PS2 port there; not invented.
- The parked functions' remaining differences are stated in their header comments (`src/game/CSequencePuzzleLogic.cpp`).

### Game code rulings (game-13)

- Three G4 trigger units, cut by the behaviour-unit rule (class functions, `__sinit`, inline factory, two `@36@`
  thunks), names from their registration and log strings: MusicTrigger 0x800CC4E4-0x800CC914 (linked; force_active
  `lbl_80529B64`), COnScreenPromptTrigger 0x800CC914-0x800CCF10 (linked; force_active `lbl_80529B74`), PlaneTrigger
  0x800CDD50-0x800CE684 (NonMatching). The registration objects are 1 byte in symbols.txt (AreaTriggerBox ruling).
- `unk_SpawnerResource` (`include/game/CGenericSpawner.h`) is an empty owner base. MusicTrigger passes
  `this ? this+0x34 : NULL` as the resource-request owner of `fn_801B05C0`/`fn_801B0B8C`, and 0x34 is its own first
  member; CGenericSpawner's owner starts at its first member too. Only an empty base, which MWCC places at the start
  of the derived class's members, gives both, so its former members moved into CGenericSpawner at the same offsets
  (CGenericSpawner unchanged, gate green). `fn_801B05C0` never reads the argument.
- MusicTrigger's `.sdata2` pools 1.0f before 0.0f: the constructor is defined out of line ahead of the destructor (as
  CSplashScreenTrigger's), so it creates 1.0f first; its out-of-line copy is dead-stripped. It assigns `track` and
  `field_0x3C` in its body, because retail stores them after the `playMsg` constructor call.
- COnScreenPromptTrigger holds a reference-counted HUD request: the base `unk_HUDRequest`
  (`include/game/unk_HUDRequest.h`; vtable 0x803E6194, the first 0x14 bytes of dtk's merged object, built at
  0x8006E660 by the player unit, whose out-of-line copies `fn_80072D1C__14unk_HUDRequestCFv`,
  `fn_80072D38__14unk_HUDRequestFf` and `__dt__14unk_HUDRequestFv` the weak inline copies resolve to) and
  `unk_OnScreenPromptRequest`, held through the intrusive pointer `unk_RefPtr<T>` (NULL-default constructor, whose
  dead `stw r0,0x4(r3)` the factory keeps). The done test is a `const` member: non-const schedules the `timeLeft`
  load after the prologue stores (5 of 39 rows). All names invented; the unit name of `unk_HUDRequest` is
  unconfirmed. objdiff scores `__sinit` 99.43% only because the dead weak base vtable shifts the string offsets it
  pairs by; cc.exe and the gate agree it is exact.
- RenderWare vectors are the plain struct `unk_RwV3d` (RwV3d, no constructors): `unk_RwMatrix`'s vectors, the
  RwV3dNormalize/RwV3dLength stand-ins `RwV3dNormalize`/`RwV3dLength`, the CBaseBehaviour frame accessors
  `fn_8009CD08`/`CD20`/`CD4C`/`CD78`, `unk_ColdMsgData::pos`, `unk_CameraInfo::direction` and PlaneTrigger's plane
  vectors. Evidence: PlaneTrigger copy-initialises the payload position word by word (`lwz`/`stw`), which only a
  constructor-free struct gives; unk_800C8598's `data.pos = *pos` emits `lfs`/`stfs` for both types. `unk_Vec3` stays
  the game's own vector; `RwV3dTransformPoints` keeps it until AreaTriggerBox's virtual signatures are revisited (open).
- `unk_PadEvent` (unk_8000A040) is `Vec acc; float value; float speed;`, a copy of KPADStatus's `acc`, `acc_value`,
  `acc_speed` (no byte change). Its former flat x/y/z made the R1 layout-view check pair it with `unk_RwV3d`.
- CSystemCommands: the class id `lbl_805297E8` (hashed from "CSystemCommands" at 0x800A2900) is declared in
  `include/game/CSystemCommands.h` (moved from CHitEffectManager.h); its command 1 is the frame placement
  (`SYSCMD_SetMatrix`, name invented), on which PlaneTrigger rebuilds its planes. AreaTriggerBase.h declares the four
  trigger messages its static initialiser registers (`lbl_80529C08`..`lbl_80529C20`).
- PlaneTrigger's handler reads the trigger's record through the inline `GetRecord()` (`unk_801A92A8.h`, name
  invented): retail loads it before `msg.record`, which every plain spelling loads first. The distance tests use one
  inline `DistanceAlong(point, origin, axis)` (name invented), which gives retail's four stack temporaries.
- Open (blocks PlaneTrigger's flip): `fn_801A73A8__12PlaneTriggerFRC19unk_AttributePacket` 98.18%, a CSE residue at the
  first attribute loop's `++sys` (retail reuses the command pointer loaded for `sys->id`; the shared `operator++` in
  `include/climax/AttributeHandler.h` reloads it). An `operator++` reading its base through `operator->` closes this
  loop but breaks the unit's second loop and the sibling handlers (CHitEffectManager, AreaTriggerBox, CColdTrigger),
  so the shared iterator is kept; the loop's original spelling is unknown.
- Rule checker note (tools not edited): R1's layout-view test reads a parameter list `( T* name` as a cast
  (`_cast_to`), so any struct whose members are a prefix of another struct's and that appears as a first parameter
  type is reported.

Open items: the unit names `game/MusicTrigger`, `game/PlaneTrigger` (strings without the C prefix) and the owner of
`unk_HUDRequest`; the invented names above (`unk_MusicPlayer`, `unk_HUDRequest`, `unk_RefPtr`,
`unk_OnScreenPromptRequest`, members and `CMD_*`); PlaneTrigger's attribute loop.

### Game code rulings (game-14)

- **Release-empty level log as a header inline (provisional, pending the user's confirmation).** `fn_80028D0C`
  (level, format, ...) is `inline` with an empty body in `include/game/Log.h`. MWCC does not inline a variadic
  function, so every unit that calls it emits a weak copy (`neutral`: 11 Matching or parked game units gain the
  0x50-byte copy, no other symbol changes) and MWLD keeps the first in link order. Evidence: the body is only the
  variadic register spill; CPreCache (0x80028BB4) is the lowest-address caller of all 40, and the copy sits inside
  that unit. The two other empty variadic loggers, `fn_8002438C` (first caller 0x8002420C) and `fn_800A65B0` (first
  caller 0x800A605C), each sit likewise just after their earliest caller, so they are expected to take the same form
  when their units are reconstructed. Same family as the release-compiled-out save-data checks (game-11) and the
  deduplicated weak thunk (CGenericSpawner ruling). Name kept as a placeholder (no string names the function).
- **CPreCache `UnCache` (name invented)** is a plain member that MWCC inlines into `fn_8009CB1C` and `fn_8000CFCC`;
  its out-of-line copy is dead-stripped, but its two `-str noreuse` literals survive: retail has three pairs of
  `"PreCache[%s]: UnCaching %s"` (0x803E156C/88, 0x803E15A4/C0, and the unreferenced 0x803E1618/34 after
  `fn_80028E38`'s strings).
- **CAIFrozenLakeEnemyRespot** (0x8011FB28-0x80120014) is a parked stub unit: its respot search `fn_8011FCC0` uses
  paired-single code (R18, as CAmbientInterferencePoint) and waits for the ruling on paired-single game helpers.
  When it is linked, its registration object `lbl_80529EDC` needs `force_active`.

### Game code rulings (game-15)

- **CAttributeHandler's defaults are header inlines, kept as weak copies (provisional, pending the user's
  confirmation).** The five defaults `fn_8000CFD4`, `fn_8000CFD0`, `fn_8000CFCC` (empty), `fn_8000CFB8` (clears two
  out parameters, returns 0) and `fn_8000CFA8` (clears one, returns 0) are defined in the class in
  `include/climax/AttributeHandler.h`, with an inline constructor that zeroes the eight words (CCollisionCapsule's
  constructor and CBaseBehaviour's at 0x8009C700 both open-code it). Evidence: retail holds one copy of each, inside
  the CCollisionCapsule unit (0x8000CFA8-0x8000CFD8, after its last member and before its static initialiser, in
  reverse slot order, where MWCC emits a unit's weak copies); CCollisionCapsule's vtable `__vt__17CCollisionCapsule`
  (0x803DFBB0) is the lowest-addressed of the 200 vtables that point at them, so its unit is the first in link order
  to need them. `symbols.txt` marks the five `scope:weak` (as the CGenericSpawner thunk) so the link drops every
  later copy. `neutral`: 40 game units gain the copies and nothing else changes; their vtable relocations now name
  the renamed `fn_8000CFA8__17CAttributeHandlerFPUl` and `fn_8000CFB8__17CAttributeHandlerFUlPUlPUl`. The parameter
  types of those two are unknown (word stores; no override or caller is known).
- **CCollisionCapsule** (0x8000C8A8-0x8000D090, class name from its registration strings) is not a CBaseBehaviour:
  its constructor open-codes the CAttributeHandler and CEventHandler constructors and stores its own vtable at 0x2C,
  so it derives from `unk_BehaviourBase`, `CAttributeHandler` and `CEventHandler` directly, with its own slots
  (destructor, handler, packet, `fn_8000CD90`, `fn_8000CEB8`). Its RW Physics callees are placeholders in
  `include/climax/Physics.h` (C linkage, unit name unconfirmed) and the physics manager `lbl_80529170` is
  `unk_PhysicsManager` (`include/game/unk_PhysicsManager.h`, role name invented). `fn_8009CB1C` compares the message's record through the event id's conversion operator (`msg.record == lbl_8052A790`,
  "Event-id conversion operator" under "Wall-pass round rulings"). The unit is `Matching`. Nothing outside needs
  `force_active`: the actor unit at 0x8003C384 constructs it directly.

## sdk-bte-4 rulings

- `hidd_conn.c` and `hidd_pm.c` (`GC/3.0a5`, R4QK01 reference) are Matching (clean gate green for each flip). Retail
  keeps one function of `hidd_conn.c` (`hidd_conn_initiate`) and three of `hidd_pm.c` (`hidd_pm_set_now`,
  `hidd_pm_proc_mode_change`, `hidd_pm_inact_timeout`); 15 and 5 functions keep the reference body
  (`// no copy in retail`). No dummy locals: no R17 finding, nothing removed from the reference bodies.
- Data: `hidd_conn.c` owns the two trace strings of `hidd_conn_initiate` (`lbl_80443E08`, `lbl_80443E20`, `.data`,
  plus one byte of padding); the dead functions' strings and the `static const` `reg_info` (`.rodata`) are
  compiled and dropped by the link (`gatecheck layout` matches). As with `hidd_proc_repage_timeout` (sdk-bte-3 "Data
  scores"), the build names the two strings `@NNN` and objdiff pairs `.data` strings by position behind the dead
  ones, so the one-shot diff shows the `lis`/`addi` pairs as argument mismatches while the bytes and the gate match.
  `hidd_pm.c` has no retail data (`pwr_modes`, `static const`, is read only by the dead `hidd_pm_init`).
- Headers: no new headers. `revolution/bte.h` (the WPAD-facing header) defined its own `tHID_DEV_DSCP_INFO`, a
  second definition of `hiddefs.h`'s `struct desc_info` (R1) that collided once a unit included both (`hidd_conn.c`
  through `btm_int.h`); `bte.h` now includes `hiddefs.h` and keeps only `tBTA_HH_DEV_DESCR`. Same layout, no byte
  change (clean gate green).
- The reference's `ARRAY_LENGTH(hd_cb.pm_params)` (tooling `decomp.h`) in the dead `hidd_pm_init` is written
  `sizeof hd_cb.pm_params / sizeof hd_cb.pm_params[0]`, as in `hidd_api.c`.
- Neither reference file has an "Original source" block (bluedroid has no HID device files), so none is given.
  Resolved: the bluedroid "Original source" blocks of `hidd_api.c` and `hidd_mgmt.c` (the reference has none; bluedroid has
  no such files) are removed, comment-only; their Broadcom licence notices stay.
- `gap_utils.c` (`GC/3.0a5`, R4QK01 reference) is Matching (clean gate green). Retail keeps six functions
  (`btm_cback`, `gap_btm_cback0`, `gap_btm_cback1`, `gap_find_addr_name_cb`, `gap_find_addr_inq_cb`,
  `gap_convert_btm_status`); five keep the reference body (`// no copy in retail`: `gap_inq_results_cb`,
  `gap_find_local_addr_by_name`, `gap_allocate_cb`, `gap_free_cb`, `gap_is_service_busy`). Under the user's
  ruling, R17 does not apply inside no-copy-in-retail bodies (main's checker change); this unit has no R17
  finding either way and nothing is removed from the reference bodies.
- Data owners: `gap_utils.c` defines `gap_cb` (`.bss` 0x804EA220-0x804EA5CC, split added; `extern` in `gap_int.h`)
  and owns `.data` 0x80443B18-0x80443E08: the trace strings of the retained functions, the five switch
  jumptables, and `lbl_80443C08` (0x88), the two strings of the dead `gap_inq_results_cb`, which the link keeps.
  Retail's `lbl_80443B18` is one label over `btm_cback`'s four strings (addressed off one base register); the
  build emits one `@NNN` per string, so the one-shot diff shows three `addi` argument mismatches in `btm_cback`
  while the bytes and the gate match. String-pool naming only, as in the NAND "Data scores" ruling.
- Header: `gap_int.h` declares `gap_find_addr_name_cb` (bluedroid's `gap_int.h` declares it; the reference header
  omits it). No other header change.
- Tool problem (open item): `gatecheck layout revolution/bte/gap_utils` reports `gap_cb` as EXTRA with "retail 0
  objs (1 gaps)": the retail-split parser treats every object whose name starts with `gap_` as a dtk gap filler,
  so the Broadcom GAP data name is misread. The gate (clean, green) proves the `.bss` layout. `unit.exe check` also
  warns that five of the six functions have no asm block (they are in `build/R5WEA4/asm/revolution/bte/gap_utils.s`).
- `bta_dm_main.c` (`GC/3.0a5`, R4QK01 reference) is Matching (clean gate green). Both reference functions are in
  retail (`bta_dm_sm_execute`, `bta_dm_search_sm_execute`); nothing is dead-stripped.
- Data owners: `bta_dm_main.c` owns `.rodata` 0x803DD918-0x803DDA40 (`bta_dm_action`, `bta_dm_st_table`,
  `bta_dm_search_action`, the four `bta_dm_search_*_st_table` rows and `bta_dm_search_st_tbl`, renamed from
  `lbl_` in `symbols.txt`) and `.bss` 0x804E7678-0x804E77F8 (`bta_dm_search_cb` 0x7C, then `bta_dm_cb` 0x104;
  split added). Both control blocks are `extern` in `bta_dm_int.h`. The reference declares `bta_dm_cb` first;
  the compiler places `bta_dm_search_cb` first, as in retail and R4QK01. The retail state-table sizes include trailing
  padding (`gatecheck layout` PADSIZE, layout matches).
- The reference's `#if 0`/`#if 1` alternatives (the array state-table typedefs, the second `BTA_DM_API_BOND_EVT`
  row position, each marked with the decomp author's "not over here?" notes) are resolved to the compiled branch; the
  retail table bytes decide the row order. Restoring the uncompiled branches would add R37 (`#if 0`) findings
  for no byte or reading gain. Its note on the
  non-array typedefs is kept, reworded.
- Headers: new `bta_api.h`, `bta_sys.h` and `bta_dm_int.h` mirror the reference headers (its `<decomp.h>` include
  dropped; the empty `#if 0` enum placeholders in `bta_api.h` removed). The BTA device-manager types and
  functions, `tBTA_SYS_ID` and `bta_sys_set_trace_level` that `<revolution/bte.h>` declared are removed there;
  `bte.h` includes `bta_api.h` and `bta_sys.h` instead (identical declarations, R1/R2; full build unchanged).
  Open item: the HID host part of `bte.h` (`tBTA_HH_*`, `BTA_Hh*`, `bta_hh_co_*`) and
  `bta_dm_co_get_compress_memory` belong in `bta_hh_api.h`, `bta_hh_co.h` and `bta_dm_co.h` once those units are split. Resolved in sdk-bte-6.

## sdk-bte-5 rulings

- Five units from the R4QK01 reference match unchanged apart from this project's layout (includes as
  `<revolution/bte/...>`, 4-space indent, Reference line, Broadcom Apache-2.0 notice): `bta_sys_cfg.c` (data
  only), `bta_sys_conn.c` (8 functions), `bta_sys_main.c` (9), `ptim.c` (4), `btm_discovery.c` (3), all under
  `GC/3.0a5` (`BTELib` default); clean gate green for each flip.
- Dead-stripped reference functions keep their bodies, marked `} // no copy in retail (dead-stripped)`:
  `bta_sys_app_open`, `bta_sys_app_close`, `bta_sys_sco_open` (bta_sys_conn), `bta_sys_disable_timers`
  (bta_sys_main), and `btm_disc_compl_cb`, `BTM_SetDiscoveryParams`, `BTM_StartDiscovery`, `BTM_FindAttribute`,
  `BTM_FindService` (btm_discovery). Retail has no copy of any of them and no caller inlines them.
- Data: `bta_sys_main.c` owns `bta_sys_cb` (`.bss` 0x804E75B8, 0x8C) and `appl_trace_level` (`.sbss` 0x8052B050,
  1 byte) and the two `bta_sys_event` trace strings (`.data` 0x80441748-0x80441780); `bta_sys_cfg.c` owns
  `p_bta_sys_cfg` (`.sdata` 0x80528D38) and `bta_sys_cfg` (`.sdata2` 0x8052ED68, 6 bytes). `symbols.txt` names the
  four from the reference (were `lbl_`); sizes exclude padding. `unit.exe new` attributed the two `bta_sys_cfg`
  objects to `bta_sys_main.c` (their only reader); the reference splits put them in `bta_sys_cfg.c`, and retail's
  section order is consistent with that (the gate is green with them there). objdiff shows
  `bta_sys_main` `.data` at 97% (string pairing by position, as in sdk-bte-4); `gatecheck layout` and the gate match.
- Headers: new `ptim.h` and `bta_sys_int.h` mirror the reference (`tPTIM_CB`, `tBTA_SYS_CB`, `extern bta_sys_cb`,
  `extern p_bta_sys_cfg`). Every function the new units define was already declared once in `bta_sys.h`,
  `btm_api.h` or `btm_int.h`; nothing is declared twice.
- Open items carried: the HID host part of `bte.h` (sdk-bte-4) stays until the `bta_hh_*` units split (not split
  in this lane); `bta_dm_cfg.c` and `bta_hh_cfg.c` (data only) remain unsplit. Both resolved in sdk-bte-6.

## sdk-bte-6 rulings

- Five units from the R4QK01 reference are Matching (clean gate green for each flip), unchanged apart from this
  project's layout (includes as `<revolution/bte/...>`, 4-space indent, Reference line, Broadcom Apache-2.0 notice),
  all `GC/3.0a5` (`BTELib` default): `bta_dm_cfg.c` and `bta_hh_cfg.c` (data only), `bta_hh_utils.c` (5 retail
  functions), `bta_hh_main.c` (3), `bta_hh_api.c` (8).
- Dead-stripped reference functions keep their bodies, marked `} // no copy in retail (dead-stripped)`:
  `BTA_HhSetReport`, `BTA_HhGetReport`, `BTA_HhSetProtoMode`, `BTA_HhGetProtoMode`, `BTA_HhSetIdle`,
  `BTA_HhGetIdle`, `BTA_HhSendCtrl`, `BTA_HhGetDscpInfo`, `BTA_HhParseBootRpt` (bta_hh_api) and
  `bta_hh_parse_keybd_rpt`, `bta_hh_parse_mice_rpt` (bta_hh_utils; their tables `vir_key_tbl_1`/`vir_key_tbl_2` are
  compiled and dropped by the link, `gatecheck layout` DEADSTRIP). The static `bta_hh_snd_write_dev` (bta_hh_api)
  and `bta_hh_state_code` (bta_hh_main) have no out-of-line copy: the compiler expands them into their retail
  callers, so they carry `(dead-stripped or inlined)`.
- Data owners (names from the reference, were `lbl_`; sizes exclude padding):
  - `bta_dm_cfg.c`: `.rodata` 0x803DD7D0-0x803DD85C (`bta_dm_cfg`, `bta_dm_rm_cfg`, `bta_dm_compress_cfg`,
    `bta_dm_pm_spec`, `bta_dm_pm_md`), `.sdata` 0x80528D10-0x80528D24 (the five `p_bta_dm_*` pointers) and `.sbss2`
    0x805300A0 (`bta_dm_pm_cfg`, 3 bytes, all zero, so `.sbss2` as in R4QK01). This confirms the sdk-bte-1 guess for
    `lbl_805300A0`. The reference's duplicate `BTA_DM_LINK_TIMEOUT` define is kept verbatim (the second value, 5000,
    is the compiled one).
  - `bta_hh_cfg.c`: `.rodata` 0x803DD860 (`bta_hh_cfg`), `.sdata` 0x80528D28-0x80528D34 (`p_devt_list`,
    `p_bta_hh_cfg`).
  - `bta_hh_main.c`: `.rodata` 0x803DDA40-0x803DDAC8 (`bta_hh_action`, `bta_hh_st_idle`, `bta_hh_st_w4_conn`,
    `bta_hh_st_connected`, `bta_hh_st_tbl`), `.data` 0x80441E38-0x80442118 (strings and the jumptable), `.bss`
    0x804E7828 (`bta_hh_cb`, 0x230; split added by hand, `unit.exe new` did not attribute it).
  - `bta_hh_api.c`: `.data` 0x80441E08 (the `BTA_HhOpen` string) and `.sdata2` 0x8052ED88 (`bta_hh_reg`, static).
  - `bta_hh_utils.c`: `.data` 0x80442118-0x804423D8 (strings only). `bd_addr_null` stays in `bd.c` (sdk-bte-3).
  - As in sdk-bte-4, objdiff pairs `.data` strings by position, so `bta_hh_sm_execute` (99.53%),
    `bta_hh_evt_code` (98.11%), `bta_hh_find_cb` and `bta_hh_trace_dev_db` show `@NNN`/`lbl_` argument mismatches
    while the bytes, `gatecheck layout` and the gate match.
- The reference's `#if 0` array state-table typedefs in `bta_hh_main.c` are resolved to the compiled branch, as in
  `bta_dm_main.c` (sdk-bte-4).
- The reference's "Original source" path for `bta_hh_utils.c` (`bt/hh/...`) is corrected to bluedroid's
  `bta/hh/bta_hh_utils.c` (comment only).
- Headers: new `bta_hh_api.h`, `bta_hh_co.h`, `bta_dm_co.h` and `bta_hh_int.h` mirror the reference headers.
  The HID host declarations of `<revolution/bte.h>` (`tBTA_HH_*`, `BTA_Hh*`, `bta_hh_co_*`) and
  `bta_dm_co_get_compress_memory` are removed there; `bte.h` includes the new headers instead. `bte.h`'s partial
  `tBTA_HH` union and sequential status enum are replaced by the reference's (the values WUD uses, `BTA_HH_OK` and
  `BTA_HH_ERR_AUTH_FAILED`, are unchanged). `WUDHidHost.c` now defines `bta_dm_co_get_compress_memory` with the
  header's `BOOLEAN` return (was `BOOL`); `ninja changes` against the baseline reports no function change.

## sdk-bte-7 rulings

- Three units from the R4QK01 reference are Matching, unchanged apart from this project's layout (includes as
  `<revolution/bte/...>`, 4-space indent, Reference line, Broadcom Apache-2.0 notice): `bta_hh_act.c` (18
  functions, `GC/3.0a5`, `BTELib` default), and the glue units `bte_logmsg.c` (8) and `bte_main.c` (4), both
  `Wii/1.0` per object as the BTE row requires. `bta_hh_act.c` and `bte_main.c` gated green alone; `bte_logmsg.c`
  gates green only with `bte_main.c` linked from source: its `.bss` (`tmp`, 0x7D0 compiled) ends 0x10 short of
  `__BTUInterruptHandlerStack`, which `bte_main.c` aligns to 32 (`ATTRIBUTE_ALIGN(32)`) and the asm fallback did not.
  So the padding before the stack is the next unit's alignment, not logmsg's.
- No dead-stripped functions. The statics `bta_hh_get_trans_status`, `bta_hh_get_w4_event`,
  `bta_hh_hid_event_name` (bta_hh_act) and `BTE_InitSys`, `bta_register_os_callbacks`, `BTE_BringUpStack`
  (bte_main) have no retail copy because the compiler expands them into their callers; they are kept unmarked.
  The reference's `#if !defined(NDEBUG)` debug code in `bte_main.c` (`__btu_interrupt_handler_debug_msg`,
  `enable_dbg_msg`, the timing code in `BTUInterruptHandler`) is kept and compiles out under `-DNDEBUG`.
- `symbols.txt` names from the reference (were `lbl_`/`fn_`): `hh_cb` (`.bss` 0x804EA718), `l2cb` (0x804EAB20),
  `@LOCAL@LogMsg__FUlPCce@tmp` (0x804E5D20, the compiler's own name for `LogMsg`'s static), `__BTUInterruptHandlerStack`
  (0x804E6500), `_bte_alarm`, `bte_hcisu_h2_cfg`, `bte_target_mode`, `_bte_app_info`, `hcisu_h2` (0x803DD7B8),
  `wait4hci` (0x80528D00), and the MSL function `vsprintf` (0x8019BC9C, between `vsnprintf` and `snprintf`, called
  by `LogMsg`). Three coarse `.data` blobs of bta_hh_act's trace strings (`lbl_80441A54`, `lbl_80441BB8`,
  `lbl_80441C94`) are split into one `data:string` symbol per string from the retail bytes, so the relocation
  targets of `bta_hh_handsk_act`, `bta_hh_ctrl_dat_act` and `bta_hh_cback` resolve per string.
- Splits: `bta_hh_act.c` owns `.data` 0x80441910-0x80441E08 and `.sdata` 0x80528D58; `bte_logmsg.c` `.text` ends at
  its last instruction (0x8024DAA8) and owns `.bss` 0x804E5D20-0x804E6500 and `.sdata` 0x80528D40 (`"%s
"`);
  `bte_main.c` owns `.bss` 0x804E6500-0x804E7530, `.sdata` 0x80528D48 and `.sbss` 0x8052B040-0x8052B048
  (`bte_target_mode` is defined there and only read by logmsg; `hcisu_h2` belongs to a later unit).
- Headers: `hidh_api.h` becomes the full reference header (was a subset); new `hidh_int.h` (`tHID_HOST_CTB`,
  `extern hh_cb`, `hidh_conn_*`) and `uusb.h` (a subset: `extern wait4hci`). `bte.h` gains `tBTE_TARGET_MODE`,
  `bte_target_mode`, `p_hcisu_if`, `p_hcisu_cfg` and the `bte_hcisu_*`, `bta_ci_hci_msg_handler`,
  `bta_usb_close_evt` prototypes. `wait4hci` and `bte_target_mode` are `volatile` globals as in the reference
  headers (`// volatile:` notes; storage qualifiers, not R17 carriers). `bte_target_mode` is defined with the
  header's `tBTE_TARGET_MODE` type (reference: `UINT8`; same bytes). `OSSwitchFiber` takes `u32` here, so
  `BTUInterruptHandler` casts its two arguments.
- Open items: `__BTUInterruptHandlerStack` is a global with no header declaration (as in the reference; only
  `bte_main.c` uses it; the rule checker does not flag it). `hh_cb`'s symbol size (0x408) includes 4 bytes of
  padding after `tHID_HOST_CTB` (0x404); `tmp` (0x7E0 vs 0x7D0) and `bte_hcisu_h2_cfg` (8 vs 4) likewise; sizes
  left as dtk inferred them.

## sdk-bte-8 rulings

- Five units from the R4QK01 reference are Matching, each gating green on its own clean gate: the glue units
  `btu_task1.c` (4 functions), `gki_ppc.c` (13) and `gki_time.c` (8), `Wii/1.0` per object as the BTE row
  requires, and the stack units `btm_dev.c` (7) and `port_utils.c` (7), `BTELib` default `GC/3.0a5`. Layout changes
  against the reference are as in sdk-bte-6 (includes as `<revolution/bte/...>`, 4-space indent, Reference line,
  Broadcom Apache-2.0 notice); `gki_ppc.c` and `gki_time.c` also brace every single-statement body and add function
  header comments; `port_utils.c` keeps the reference's brace style. None needed a neighbour linked from source (each
  passed a Matching-link probe before the flip). `gki_ppc.c`'s `.text` split ends at its last instruction
  (0x8024C4C4); the 12-byte pad stays with the asm unit `hcisu_h2.c`.
- `btu_task1.c`: the reference's `btu_task_msg_handler` had two dummy locals (`sp10 = OSGetTime()`, `sp08 = btu_count`,
  never read; R17). Retail calls `OSGetTime` and discards the result, so the source is a bare `OSGetTime();`
  statement and the `btu_count` copy is dropped; the bytes are unchanged. The reference's register-style locals
  (`r30`, `sp09`, ...) are renamed to bluedroid-style names (`p_tle`, `p_msg`, `i`, `mask`, `handled`,
  `event_range`, `timer_cb`; `done` is invented). `bt_types.h` gains `BT_EVT_MASK` (0xff00) and
  `BT_EVT_TO_BTU_HCI_CMD` (22 << 8) from bluedroid. Dead-stripped (bodies kept, marked `(dead-stripped)`):
  `btu_register_timer`, `btu_deregister_timer`, `btu_register_event_range`, `btu_deregister_event_range`
  (prototypes added to `btu.h`). `btu_enable_dbg` and `btu_dbg_flag` exist only under `#if !defined(NDEBUG)`.
  Data: `btu_cb` (`.bss` 0x804E7530, 0x84 plus 4 bytes of padding) is taken with `--include-data`; `symbols.txt`
  renames `lbl_80528D50` to `btu_count`, `lbl_80528D54` to `execute_btu`, `lbl_8052B048` to `_btu_g_count` and
  `lbl_8052B04C` to `_btu_last_timer_tick` (local).
- `btm_dev.c`: no data. `btm_sec_free_dev` is expanded into `BTM_SecDeleteDevice` and marked
  `(dead-stripped or inlined)`.
- `gki_ppc.c`: `gki_cb` (`.bss` 0x804BB1A0, size 0x28AE0 = `sizeof(tGKI_CB)`, the retail `memset` length) replaces
  dtk's three objects `lbl_804BB1A0`, `lbl_804D6A08` and `lbl_804DE017` (the interior symbols are deleted). New
  headers `gki_common.h` and `gki_int.h` come from the reference's private/bte headers. `OSError.h` gains
  `ASSERTLINE(exp, line)` (from the reference's `os.h`; release form `((void)0)`, the debug form is untested).
  `wud.h` gains the `App_MEMalloc` and `App_MEMfree` prototypes (defined in `WUD.c`, previously undeclared).
  Dead-stripped: `GKI_create_task`, `GKI_suspend_task`, `GKI_resume_task`, `GKI_wait`, `GKI_isend_event`,
  `GKI_map_taskname` (its "BAD" `.sdata` literal is DEADSTRIP in `gatecheck layout`), `GKI_get_time_stamp`,
  `GKI_register_mempool`. `GKI_exit_task` is inlined into `GKI_shutdown` and marked `(dead-stripped or inlined)`;
  the static `GKI_init_btu_task` is expanded into `GKI_init` and left unmarked.
- `gki_time.c`: no data, no header changes. Dead-stripped: `GKI_ready_to_sleep`, `GKI_timer_update`,
  `GKI_init_timer_list_entry`; `gki_adjust_timer_count` is expanded into `GKI_start_timer` and marked
  `(dead-stripped or inlined)`. The reference local `temp_ticks` keeps its name (R7, naming open).
- `port_utils.c`: new `l2cdefs.h` (the reference header: `L2CAP_PKT_OVERHEAD` and related constants; no duplicate
  definitions in the tree). `.data` 0x80445CA8-0x80445DF0: the coarse `lbl_80445CA8` blob (0x100) is split into five
  `data:string` symbols (`port_select_mtu`'s format strings, as sdk-bte-7 did for bta_hh_act), plus `lbl_80445DA8`
  and `lbl_80445DC0`. Dead-stripped: `port_allocate_port`, `port_set_defaults`, `port_find_mcb`, `port_find_port`;
  their static const `default_port_pars` is compiled and dropped by the link (DEADSTRIP). `rfc_cb` stays outside the
  unit.
- Open items: `btu_task1.c`'s `done` local name is invented; the `ASSERTLINE` debug form is untested.

## sdk-bte-9 rulings

- Six units from the R4QK01 reference are Matching: the stack units `bta_dm_api.c` (11 functions), `sdp_server.c` (4),
  `l2c_main.c` (5) and `gap_conn.c` (8), `BTELib` default `GC/3.0a5`, each gating green on its own clean gate; and the
  glue units `hcisu_h2.c` (8) and `uusb_ppc.c` (11), `Wii/1.0` per object, flipped together. Layout changes against the
  reference are as in sdk-bte-6/8 (includes as `<revolution/bte/...>`, 4-space indent, Reference line, Broadcom
  Apache-2.0 notice); the glue units also brace every single-statement body and add function header comments.
- `hcisu_h2.c` gates red alone: `.bss` comes out 0x20 short, because retail reserves 0x804E3C80-0x804E3CC0 for
  `hcisu_h2_cb` (0x24 compiled) and the padding is the next unit's alignment (`uusb_ppc.c`'s `__uusb_ppc_stack1` and
  `__uusb_ppc_stack2` are `ATTRIBUTE_ALIGN(32)`; the asm fallback did not align). With both linked from source the clean
  gate is green (the sdk-bte-7 bte_logmsg/bte_main case). `hcisu_h2.c`'s `.text` split starts at 0x8024C4D0 and ends at
  its last instruction 0x8024CB8C; the pads before and after are left to alignment.
- `bta_dm_api.c`: `symbols.txt` renames `lbl_8052ED78` to `bta_dm_reg` and `lbl_8052ED80` to `bta_dm_search_reg`
  (local, the unit's `.sdata2` statics) and `lbl_803DD8A0` to `bta_service_id_to_btm_srv_id_lkup_tbl` (read by
  `BTA_DmAddDevice`; owned by the asm unit `bta_dm_act.c`, declared in `bta_dm_int.h`). Dead-stripped:
  `BTA_DmDiscover`, `BTA_DmBond`, `BTA_DmAuthorizeReply`, `BTA_DmSignalStrength`, `BTA_KeepACLLinksOnShutDown`.
- `sdp_server.c`: `lbl_804EB720` is renamed `sdp_cb` (owned by a later sdp unit, declared in `sdp_int.h`). The
  reference's `__memcpy` of the attribute sequence is spelled as struct assignment (`attr_seq_sav = attr_seq;`), which
  gives retail's inline copy (the `memcpy` form scores 90.6% in `process_service_search_attr_req`). The reference's
  `(void)&len_to_send; (void)&p_buf;` and two set-never-read `is_cont` locals are dropped (R17; bytes unchanged). The
  literal 54 is spelled `DATA_ELE_SEQ_DESC_TYPE << 3 | SIZE_IN_NEXT_WORD`; `L2CAP_MIN_OFFSET` in the response pointer is
  read back as `p_buf->offset` (assigned the line before; same bytes, as the file computes `p_req`).
- `l2c_main.c` defines `l2cb` (as the reference; `.bss` taken with `--include-data`). The `.data` blob `lbl_80445590`
  is split into 16 `data:string` symbols. New header `hcimsgs.h` (the reference header; `HCID_GET_EVENT` and
  `HCID_GET_HANDLE`, without which those compile as implicit calls). The reference's `#if 0` placeholder in the timeout
  switch is a comment (R37). `l2c_process_compress_setup_timeout` (empty) is marked `(dead-stripped or inlined)`.
- `hcisu_h2.c`: defines `hcisu_h2` (`.rodata`, `tHCI_IF const`); `symbols.txt` renames `lbl_804E3C80` to `hcisu_h2_cb`,
  `lbl_8052ED58` to `hcisu_preamble_table`, `lbl_8052ED60` to `hcisu_msg_evt_table`. `uusb.h` becomes the reference
  header except the unspecified-signature prototypes `UUSB_ReadBuf()`/`UUSB_WriteBuf()` (R25; nothing calls them).
  The set-never-read `bytes_sent`/`bytes_read` locals become bare calls (sdk-bte-8 ruling); the unread `type` and
  `(void)cb;` are dropped; the `packet_type` literals 4, 2, 3 are spelled `H4_TYPE_EVENT`, `H4_TYPE_ACL_DATA`,
  `H4_TYPE_SCO_DATA`; `hcisu_h2_open`'s `_p_cfg` parameter copy is a plain parameter. The reference's local
  `STREAM_TO_UINT16` redefinition (a `(UINT16)` cast on the high byte) is kept: retail's extra mask in
  `hcisu_h2_send_msg_now` needs it (99.44% with the shared `bt_types.h` form). `hcisu_h2_send_msg` is marked
  `(dead-stripped or inlined)`.
- `uusb_ppc.c`: defines `wait4hci` (`--include-data`), `usb`, the two stacks and the `__ntd_*` statics; `symbols.txt`
  names `usb`, `__uusb_ppc_stack1/2`, `__ntd_ios_file_descriptor`, the seven `.sbss` statics, and the eight IUSB
  functions it calls (`fn_8027C200` `IUSB_OpenLib`, C300 `IUSB_CloseLib`, C4B0 `IUSB_OpenDeviceIds`, C600
  `IUSB_CloseDeviceAsync`, CDF0 `IUSB_ReadIntrMsgAsync`, CE70 `IUSB_ReadBlkMsgAsync`, CF10 `IUSB_WriteBlkMsgAsync`,
  D560 `IUSB_WriteCtrlMsgAsync`; their offsets from `IUSB_OpenLib` equal the R4QK01 `symbols.txt`'s). New header
  `include/revolution/usb/usb.h` (those prototypes, `IOSError` for the reference's `IPCResult`); `bte.h` gains
  `__ntd_set_ohci`, `__ntd_set_pid_vid`, `__ntd_get_usb_file_descriptor` (the reference's `misc.h`). The trace code is
  keyed on `#if !defined(NDEBUG)` as in bte_main. The reference's `#define NULL ((void *)(0))` is kept (retail's
  `li r0,0; cmplw` null tests need it). Callbacks take `s32` (the `USBCallback` typedef). Dead-stripped:
  `__ntd_set_ohci`, `__ntd_set_pid_vid`, `__ntd_get_usb_file_descriptor`, `__ntd_get_allocated_mem_size`,
  `uusb_get_trace_state`; `uusb_set_trace_state` is `(dead-stripped or inlined)`. Invented names (marked):
  `p_hci_buf`, `intrEndpoint2`, `bulk_writes_pending`, `ctrl_writes_pending`; `hcisu_h2_init`'s `option`.
- `gap_conn.c`: reference text unchanged apart from layout; `gap_cb` stays in `gap_utils.c`. Dead-stripped: the 11
  `GAP_Conn*` API functions; their six trace literals are DEADSTRIP in `gatecheck layout`. objdiff scores
  `gap_connect_ind` 99.80%, `gap_disconnect_ind` 99.92% and `gap_congestion_ind` 99.87% only because it pairs the
  strings by position (retail `lbl_80443A48`, ours `@2119`, shifted by the dead-stripped literals); with the
  `GAP_Conn*` bodies removed as a probe all three are 100%, and the clean gate is green (the NANDCore case).
- Open items: `uusb_ReadIntrDataCB` keeps the reference's `int len = 0;` and routes the length through it
  (`len = ret; p_recv_buf->len = len;`); without the local the function is 99.9%, with the reference's unused local it
  trips R17. The routed spelling is a judgement, not proof (needs review). `__ntd_ohci`, `__ntd_ohci_init_flag` and the
  uusb stacks are globals with no header declaration (as the reference; the `__BTUInterruptHandlerStack` precedent).
  PADSIZE notes: `hcisu_h2_cb` 0x24/0x40, `hcisu_preamble_table` 4/8, `hcisu_h2` 0x14/0x18, `usb` 0x4C/0x60 (sizes as
  dtk inferred). `hcimsgs.h` is not yet included by `l2c_link.c` (resolved in sdk-bte-14: it is, as in the
  reference). `bta_service_id_to_btm_srv_id_lkup_tbl` is defined by no source until `bta_dm_act.c` is added
  (resolved by sdk-bte-12: `bta_dm_act.c` defines it).
- Review rulings (sdk-bte-9 merge): `uusb_ReadIntrDataCB`'s `len` is accepted as original code, not steering: the
  reference declares the local, an unused declaration in a decomp reference marks a variable the original used, and
  `len = ret; p_recv_buf->len = len;` is an ordinary length local (the unrouted form is 99.9%). The open item above is
  resolved. `sdp_server.c`'s `attr_seq_sav = attr_seq;` and `attr_seq = attr_seq_sav;` are struct assignments
  (retail's inline copy; the reference's `memcpy` is a decompiler rendering of the same). The three `gap_conn.c`
  functions that a one-shot objdiff diff scores 99.80 to 99.92% are 100% in the report and count as matched (string
  pairing by position, as the NAND "Data scores" and `hidd_proc_repage_timeout` rulings). The IUSB names rest on the
  reference's offsets from `IUSB_OpenLib` plus the call shapes in `uusb_ppc.c` (`"oh0"`/`"oh1"` device ids,
  interrupt/bulk/control callbacks, the 0x20 control request type), all 100% with these relocation targets.

## sdk-bte-10 rulings

- Five stack units from the R4QK01 reference are Matching, `BTELib` default `GC/3.0a5`, each gating green on its own
  clean gate with no neighbour linked from source: `bta_dm_pm.c` (8 functions), `btm_pm.c` (10), `sdp_api.c` (9),
  `hidh_api.c` (11) and `rfc_port_if.c` (12). Layout changes against the reference are as in sdk-bte-6/8/9; the stubs'
  "unit name unconfirmed" comments are dropped (the reference names each file). None needed a header change.
- `bta_dm_pm.c`: defines `bta_dm_conn_srvcs` (`symbols.txt` renames `lbl_804E77F8`; `.bss` taken with
  `--include-data`; read by the asm unit `bta_dm_act.c`). `bta_dm_pm_set_mode`'s tail keeps the reference's (Broadcom's) spelling
  `if (pm_action == BTA_DM_PM_NO_ACTION) { /* ... */ } else if (PARK) ... else if (SNIFF) ... else if (ACTIVE)`: the
  empty `NO_ACTION` branch is what retail compiles (dropping it gives 99.53%; the earlier nested
  `if (pm_action != NO_ACTION)` form also reached 100% but deviated from the original text). Resolved by the user's
  ruling of 2026-10-23: R17 accepts an empty `{ }` branch of an `if`/`else if` that is followed by `else if`/`else`,
  with or without a comment inside (a bare block, a lone empty `if`, the last branch of a chain and an empty `else`
  block still fire; tools/c/tools/check_rules/README.md). Android bluedroid `bta/dm/bta_dm_pm.c` has the same empty-branch head. The static helpers `bta_dm_pm_park`, `_sniff` and
  `_active` are expanded into `bta_dm_pm_set_mode` and unmarked.
- `btm_pm.c`: `symbols.txt` renames `lbl_803DDAC8` to `btm_pm_md_comp_matrix`, `lbl_8052ED98` to `btm_pm_mode_off`,
  `lbl_8052ED9C` to `btm_pm_mode_msk`; these three tables are non-static definitions with no header declaration, as
  the reference (used only in this unit; the rule checker raises no R2). Dead-stripped: `BTM_IsPowerManagerOn`;
  `btm_pm_find_acl_ind` is a static expanded into its callers. `btm_cb` stays in `btm_main.c`.
- `sdp_api.c`: reference text unchanged apart from layout. Dead-stripped: `SDP_FindAttributeInDb`,
  `sdp_fill_proto_elem` (static; both callers stripped), `SDP_FindProtocolListElemInRec`,
  `SDP_FindAddProtoListsElemInRec`, `SDP_FindProfileVersionInRec`, `SDP_DiDiscover`, `SDP_GetNumDiRecords`,
  `SDP_GetDiRecord`. `sdp_cb` stays with the later sdp unit.
- `hidh_api.c`: defines `hh_cb` (`--include-data`; PADSIZE 0x404/0x408). The `.data` blob `lbl_80443E58` is split
  into 9 `data:string` symbols. `hidh_get_str_attr` (non-static, no retail copy, expanded into
  `hidh_search_callback`) is marked `(dead-stripped or inlined)`; its off-by-one terminator (`str[max_len]` after
  copying `max_len - 1` bytes) is labelled, not fixed (R36).
- `rfc_port_if.c`: defines `rfc_cb` (`.bss` 0x804EB308-0x804EB720 with `--include-data`; declared in `rfc_int.h`).
  This replaces the sdk-bte-8 note that `rfc_cb` is owned outside the matched units. Dead-stripped:
  `RFCOMM_StartReq`. The function banners' one-line descriptions are written for this project (the reference has
  none).
- Open items: `bta_service_id_to_btm_srv_id_lkup_tbl` was defined by no source until `bta_dm_act.c` was added.
  Resolved by sdk-bte-12.

## sdk-bte-11 rulings

- Four stack units from the R4QK01 reference are Matching, `BTELib` default `GC/3.0a5`, each flipped on its own clean
  gate: `rfc_utils.c` (15 functions), `rfc_l2cap_if.c` (10), `rfc_mx_fsm.c` (10) and `sdp_db.c` (10). Reference text
  unchanged apart from layout (sdk-bte-6/8/9); no locals renamed, none invented; no header changes. The function
  banners' descriptions are written for this project.
- R17 and empty head branches (user's ruling, 2026-10-23): an empty branch heading an `if`/`else if` chain is not fake
  code (checker change "tools: R17 accepts an empty branch heading an else-if chain", cherry-picked). `bta_dm_pm_set_mode`
  is restored to the reference's `if (pm_action == BTA_DM_PM_NO_ACTION) { /* ... */ } else if ...` (100%, gate green);
  this closes the sdk-bte-10 open item on its nesting.
- `rfc_utils.c`: `symbols.txt` splits the `.data` blob `lbl_8044663C` (0x24) into two `data:string` objects
  (`lbl_8044663C` 0x14, `lbl_80446650` 0x10; sdk-bte-7 precedent: without it `rfc_port_closed` has a wrong relocation
  target). The static `rfc_crctable` (`.rodata`) keeps its `lbl_` name. `rfc_timer_start`/`_stop` also expand into
  their callers but keep out-of-line copies.
- `rfc_l2cap_if.c`: `lbl_80445DF0` (0x140, nine strings) is split into nine `data:string` objects (sizes include
  padding; `gatecheck layout` PADSIZE notes only). `rfc_find_lcid_mcb` has no retail copy (expanded into its five
  callers): marked `(dead-stripped or inlined)`.
- `rfc_mx_fsm.c`: `lbl_80445F4C` (0x54) is split into three strings and `lbl_80445FFC` (0x4C) into two.
  `rfc_mx_send_config_req` has no retail copy: `(dead-stripped or inlined)`. The reference's file-local
  `#define L2CAP_SUCCESS 0` is kept (no bte header defines an L2CAP result code yet).
- `sdp_db.c`: `.data` 0x804466C8-0x80446760 (three strings, already one symbol each). Dead-stripped:
  `sdp_compose_proto_list` (static), `SDP_AddSequence`, `SDP_AddProtocolList`, `SDP_AddAdditionProtoLists`,
  `SDP_AddProfileDescriptorList`, `SDP_AddLanguageBaseAttrIDList`. Data scores as sdk-bte-3: `SDP_AddUuidSequence`
  (99.81%) and `SDP_DeleteAttribute` (99.93%) differ only in objdiff's positional pairing of `.data` strings behind
  the dead `SDP_AddSequence` literals (`lbl_80446704`/`lbl_80446738` vs `@NNN`); bytes, `gatecheck layout` (3
  DEADSTRIP objects) and the gate match. `sdp_cb` stays with `sdp_main.c`.
- Open items: `L2CAP_SUCCESS` should move to `l2c_api.h` (bluedroid spells the L2CAP result codes there) when
  `l2c_api.c` is added (resolved in sdk-bte-14: neither reference's `l2c_api.h` defines the L2CAP result
  codes, so `rfc_mx_fsm.c` keeps its file-local `#define L2CAP_SUCCESS 0`). Resolved by sdk-bte-12:
  `bta_dm_act.c` defines `bta_service_id_to_btm_srv_id_lkup_tbl`.
## sdk-bte-12 rulings

- Four stack units from the R4QK01 reference are Matching, `BTELib` default `GC/3.0a5`, one `flip:` commit per
  unit: `bta_dm_act.c` (56 functions), `btu_hcif.c` (12), `hcicmds.c` (59) and `hidh_conn.c` (16). The four were
  validated by a clean gate with all four linked from source (`build/R5WEA4/main.dol: OK`), and the final cleanup
  commit was gated again on its own; the objdiff report scores all four at 100% code and 56/56, 12/12, 59/59 and
  16/16 functions.
  Layout changes against the reference are as in sdk-bte-6/8/9 (includes as `<revolution/bte/...>`, 4-space indent,
  Reference line, Broadcom Apache-2.0 notice); no locals renamed, none invented, no shared header changed.
- `btu_hcif.c`: the reference's never-read `p_cc` local is dropped (R17; the assignment is dead and the bytes are
  unchanged). `btu_hcif_send_host_rdy_for_data` has no retail copy (dead-stripped). The 12 retail functions match
  from the reference text unchanged: `btu_hcif_process_event` (1464 bytes, the handlers the compiler inlined into
  it), `btu_hcif_send_cmd`, `btu_hcif_cmd_timeout`, and the nine static handlers that keep out-of-line copies
  (`connection_comp_evt`, `connection_request_evt`, `qos_setup_comp_evt`, `esco_connection_comp_evt`,
  `hdl_command_complete`, `command_complete_evt`, `hdl_command_status`, `command_status_evt`,
  `link_key_notification_evt`).
- `hcicmds.c`: 66 of the reference's command builders have no retail copy (dead-stripped: the Wii stack never sends
  them). `ARRAY_LENGTH` is the mscharged-decomp tooling header's macro, not Broadcom source, so
  `btsnd_hcic_set_afh_channels` loops to `sizeof(channels)` (the sdk-bte-2 ruling; `channels` is `UINT8`, the value
  is the same).
- `hidh_conn.c`: `symbols.txt` splits three coarse retail `.data` blobs (`lbl_80443FB4` 0x120, `lbl_804440D4` 0xB8,
  `lbl_804441B8` 0x58) at the built string boundaries so the per-function string-offset relocations resolve to the
  same symbols; each part's size runs to the next part's start, so it includes the retail padding (`gatecheck
  layout` PADSIZE notes only). This is the sdk-bte-11 (`rfc_l2cap_if.c`) precedent, needed because the compiler
  emits one pool object per string while dtk can only name the addresses the DOL references.
  `hidh_l2cif_config_ind`'s unread `p_dev` local and its assignment are dropped (R17) and the reference's
  `# if 0` block before `hidh_conn_snd_data` (an old register-annotated prototype of it) is removed (R37:
  preprocessed-out text is not source); both are byte-neutral (16/16 afterwards). `hidh_conn_retry` has no retail
  copy and no callers at all: body kept, marked `// no copy in retail (dead-stripped)`.
- `bta_dm_act.c`: `symbols.txt` merges dtk's `.sdata2` pair `lbl_8052ED70` (0x2) and `lbl_8052ED72` (0x6) into the
  one 0x4 object the compiler emits for `bta_dm_find_services`' `attr_list` template (`0x0001`,
  `0x8001` = `ATTR_ID_SERVICE_CLASS_ID_LIST`, `ATTR_ID_EXT_BRCM_VERSION`); the four zero bytes after it
  (0x8052ED74-0x8052ED78) are section padding before `bta_dm_reg` (0x8052ED78, 8-aligned). The two `sda21` loads
  now both address `lbl_8052ED70` and the function is 100%. `bta_service_id_to_btm_srv_id_lkup_tbl` (.rodata,
  retail size 0x60 including 4 bytes of trailing padding) is this unit's, closing the sdk-bte-9/10/11 open item.
  `bta_sys_check_compress` and `bta_dm_adjust_roles` have no retail copy (dead-stripped, bodies kept and marked
  `// no copy in retail (dead-stripped)`); `bta_sys_check_compress`'s trace literal
  `"bta_sys_check_compress OK, BTA ID %d"` is compiled between the live `.data` strings, so in the *object* the
  strings after it sit 0x25 bytes higher than in retail's object. `bta_dm_find_services` is 100% after the
  `.sdata2` merge; `bta_dm_l2cap_server_compress_cback` (99.82%) and `bta_dm_compress_cback` (99.86%) differ only in
  objdiff's positional pairing of those strings, with byte-identical instructions, `gatecheck layout` matching
  (DEADSTRIP on the dead literal) and the report scoring both 100% (the sdk-bte-9 `gap_conn.c` precedent).
- Open items: the two one-shot objdiff scores above are pairing artifacts, not source differences; the four units
  have no unfinished logic. Naming (R7): `bta_sys_check_compress` and `bta_dm_l2cap_server_compress_cback` keep the
  reference's `param_1`/`param_2`/`param_7` parameter names (no header or reference names them). Address comments:
  no live function carries a `// 0x8XXXXXXX` line, because the four units keep the reference's text, which has no
  function banners (unit.exe's check warns for all four; project-wide and not byte-related). The reference's
  decompiler notes stay inside the matched bodies as the surviving-reference principle requires (`// ???`,
  `// Why 79, specifically`, `// Yes, they are defined like this.`, `// casting away constness`, and the
  `HID_HOST_MAX_CONN_RETRY` note).

## sdk-bte-13 rulings

- Five stack units from the R4QK01 reference are Matching, `BTELib` default `GC/3.0a5`, each flipped on its own
  clean gate: `btm_acl.c` (29 functions), `btm_devctl.c` (33), `btm_inq.c` (25), `btm_sco.c` (15) and
  `btm_sec.c` (31). Reference text unchanged apart from layout (sdk-bte-6/8/9: includes as
  `<revolution/bte/...>`, 4-space indent, Reference line, Broadcom Apache-2.0 notice); no locals renamed, none
  invented. No function banners yet: neither reference file carries one, and sdk-bte-10/11 wrote them
  where the reference had none, so that is open readability work here. One shared-header addition:
  `hcimsgs.h` defines `HCIC_PARAM_SIZE_READ_NAME` (0) for the `BTM_ReadLocalDeviceName` call site, which
  both references use without defining it. Its value cannot reach the byte stream (`HCI_GET_CMD_BUF` in
  `bt_target.h` drops its argument), and the command's own sender `btsnd_hcic_read_name` spells the same
  size as `HCIC_PARAM_SIZE_READ_CMD` (0) in both references.
- No copy in retail (the dead-stripped ruling): 15 reference functions in `btm_acl.c`, 14 in `btm_devctl.c`,
  14 in `btm_inq.c`, 17 in `btm_sco.c` and 20 in `btm_sec.c`. Bodies kept and marked
  `// no copy in retail (dead-stripped)`, except the helpers the compiler inlined into called code (no
  out-of-line copy in the object), marked `(dead-stripped or inlined)`: `btm_establish_continue`;
  `btm_dev_reset`, `btm_continue_reset`, `btm_after_reset_hold_complete`; `btm_clr_inq_result_flt`,
  `btm_inq_db_find`, `btm_initiate_inquiry`; `btm_send_connect_request`; `btm_sec_start_get_name`,
  `btm_sec_start_authentication`, `btm_sec_start_encryption`, `btm_sec_are_all_trusted`,
  `btm_sec_find_first_serv`, `btm_sec_find_next_serv`, `btm_sec_find_mx_serv`, `btm_restore_mode` and
  `btm_clr_client_scn`. `btm_continue_reset`'s only caller is `btm_after_reset_hold_complete`, itself inlined.
- Data: each unit's `.data` is the compiler's string literals (base `-str reuse`). A blob holding several
  literals is split into one `data:string` object per emitted literal (sdk-bte-11 precedent; sizes are dtk's
  analysis after the first build): `btm_acl.c` `lbl_80442438` (6), `lbl_804425F4` (2), `lbl_80442724` (4);
  `btm_devctl.c` `lbl_80442828` (3), `lbl_80442958` (3); `btm_sco.c` `lbl_80442CA8` (3), `lbl_80442DD4` (7);
  `btm_sec.c` `lbl_80442FA8` (4), `lbl_80443040` (4), `lbl_804431A8` (7), `lbl_804432A4` (10),
  `lbl_80443740` (13). `btm_inq.c`'s nine `.data` blobs hold one literal each.
- Named data from the source (dtk had `lbl_` names): `btm_sco.c`'s `.rodata` `btm_esco_defaults`
  (0x803DDAD8, 0x10) and `btm_inq.c`'s `.sdata2` LAPs `general_inq_lap` / `limited_inq_lap`
  (0x8052ED90 / 0x8052ED94); retail's two sizes include one padding byte each (`gatecheck layout` PADSIZE
  note), ours is 3 bytes and the link pads.
- `btm_inq.c`'s two dead literals (`BTM_SetPeriodicInquiryMode`, `BTM_CancelPeriodicInquiry`) are DEADSTRIP in
  `gatecheck layout`; with them dropped the `.data` span is retail's 0x210. `btm_sec.c`'s `.sdata2` is one
  6-byte initializer (the local `bd_addr[BD_ADDR_LEN] = {0xff, ...}`) against dtk's six 1-byte retail objects;
  bytes, order and the gate agree (COARSE note), the granularity is dtk's analysis of the retail `.sdata2`.
- objdiff: all five units report 100% matched code and 100% complete data. `btm_inq.c` is the one unit with
  no `matched_data` at all: its `.data` section scores 89.69% and `.sdata2` 93.33%, objdiff's positional
  object pairing behind the two dead literals (`BTM_SetPeriodicInquiryMode`, `BTM_CancelPeriodicInquiry`) and
  the two LAP objects, while its data COMPLETE is 100%. The one-shot `objdiff-cli diff` likewise still scores
  `btm_inq.c`'s eight string-emitting API functions 99.67-99.93% and `btm_sec_mkey_comp_event` 99.60% (plus
  the section symbols and the two LAP objects), for the same pairing reason and the six-way `.sdata2` split in
  `btm_sec.c`; bytes, `gatecheck layout` and the gate match (the sdk-bte-3/9/11 note).
- `btm_inq_db_init` is a live retail function whose body is a bare `blr` (4 bytes). The reference's
  `/* ... */` placeholder is replaced by an empty body with an evidence comment (no byte change).
- Open items: the dead functions `btm_inq_check_ageing` and `BTM_ContinueReset` keep the reference's
  `/* ... */` placeholder bodies under their dead-stripped markers (no retail copy to reconstruct against);
  the five units carry no function banners (201 R33 [free] findings) because neither reference has them, left
  for a later readability pass.

## sdk-bte-14 rulings

- Eight units from the R4QK01 reference are Matching, one clean gate for the batch (`build/R5WEA4/main.dol: OK`,
  all eight `Matching` in the same link), each 100% functions and code in the gate's report: `gki_buffer.c` (18
  functions, `Wii/1.0`), `l2c_api.c` (13), `l2c_csm.c` (11), `l2c_link.c` (17), `l2c_utils.c` (33),
  `sdp_discovery.c` (10), `sdp_main.c` (11) and `sdp_utils.c` (13), the last seven `BTELib` default `GC/3.0a5`.
  Each unit is the reference's text adapted to this project's layout (includes as `<revolution/bte/...>`, 4-space
  indent, Reference line, Broadcom Apache-2.0 notice, sdk-bte-6/8/9); apart from that layout only the six
  `p_buf->offset` read-backs and the `#if 0` placeholder below differ, and the function banners' descriptions are
  written for this project. No header changes: every declaration these units need was already in
  `include/revolution/bte/`.
- `gki_buffer.c` is a glue unit: `Wii/1.0` per object. Under the `BTELib` default `GC/3.0a5` it leaves
  `gki_init_free_queue` at 87.1%, `gki_buffer_init` at 94.8%, `GKI_getbuf` 98.1%, `GKI_getpoolbuf` 96.6%,
  `GKI_freebuf` 99.8%, `GKI_send_msg` 99.9%, `GKI_read_mbox` 89.4% and `GKI_delete_pool` 90.8%; with `Wii/1.0` all
  18 are 100%. This confirms the unit's `Wii/1.0` entry in the library map rather than changing the profile.
- Two groups of reference functions the link drops. Out-of-line dead-stripped bodies keep the reference's text,
  marked `// no copy in retail (dead-stripped)`, each confirmed against the compiled object's out-of-line
  symbols: `gki_buffer`: `GKI_igetpoolbuf`, `GKI_poolfreecount`, `GKI_change_buf_owner`, `GKI_find_buf_start`;
  `l2c_api`: `L2CA_Ping`, `L2CA_SetDesireRole`, `L2CA_SetAclPriority`, `L2CA_SetCompression`, `L2CA_Flush`,
  `L2CA_GetNumQueuedBufs`; `l2c_link`: `l2c_link_get_decompressed_pkt`, `l2c_link_pkts_rcvd` (Broadcom's stub
  body, always no packets); `l2c_utils`: `l2cu_send_peer_info_req`, `l2cu_find_ccb_by_remote_cid`,
  `l2cu_send_feature_req`, `l2cu_check_feature_req`, `l2cu_check_feature_rsp`; `sdp_main`:
  `sdp_set_max_attr_list_size`; `sdp_utils`: `sdpu_compare_bt_uuids`.
  Four bodies reach retail inlined instead, marked `// no copy in retail (dead-stripped or inlined)`, with the
  retail caller's inline copy read in the asm: `GKI_set_pool_permission` (into `GKI_create_pool`, whose pool mask
  update is inline at 0x8024BA5C-0x8024BA7C with no call), `gki_chk_buf_damage` (into `GKI_freebuf`,
  `GKI_send_msg`, `GKI_enqueue` and `GKI_enqueue_head`; `GKI_freebuf` has the 0xddbbddba magic compare inline at
  0x8024AF84-0x8024AF9C with no `bl`), `gki_add_to_pool_list` (into `GKI_create_pool`) and
  `gki_remove_from_pool_list` (into `GKI_delete_pool`). The two statics have no out-of-line copy in the compiled
  object either; the first two do have one, and the linker drops it.
  `gatecheck layout` reports DEADSTRIP only in `l2c_utils` `.data` (two string objects); every other unit's
  sections match.
- **R4 and the L2CAP offset constants.** `L2CAP_SEND_CMD_OFFSET` (`l2c_int.h`, 0) and `L2CAP_MIN_OFFSET`
  (`l2c_api.h`, 9) offset the payload pointer of a `BT_HDR` buffer, so the checker's offset-macro rule reads them
  as pointer offsets. Where the buffer's `offset` was assigned on the line before, the pointer reads it back,
  `(UINT8 *)(p_buf + 1) + p_buf->offset` — the `sdp_server.c` precedent (sdk-bte-9); the compiler keeps the stored
  value, so the bytes are unchanged. Six sites: `l2c_utils` `l2cu_build_header` and `l2cu_send_peer_config_rej`,
  `sdp_discovery`'s three send paths (`p_cmd`/`p_msg`) and `sdp_utils` `sdpu_build_n_send_error`. The other twelve
  `l2c_utils` sites keep the reference's exact expression: their buffer comes back from `l2cu_build_header`, so
  `p_buf->offset` is not known at the site and a read-back would emit a load retail does not have. They carry
  `// byteptr: file format, HCI/L2CAP headers after BT_HDR; offset would load`: `(p_buf + 1)` skips the `BT_HDR`
  block, after which come the HCI data preamble, the L2CAP packet header and the command header.
  The six read-backs are the only code-level source changes here; no code changed for a finding.
- **Data scores.** Two units report a low unit-level `matched_data_percent` while every function is 100%; neither
  is a layout difference (`gatecheck layout` reports "layout matches" for all eight and the gate is green).
  - `sdp_main`: `.data` is exactly matched (864/864, the unit's whole 0x360 `.data`); the unit's 4.59% is the
    `.bss` `sdp_cb`, whose retail size 0x4638 includes 4 bytes of trailing padding over the built 0x4634, so
    objdiff cannot pair the two objects (the sdk-bte-4/6 PADSIZE case).
  - `l2c_utils`: the 6.45% is the 16-byte `.sdata` (the only exactly matched data); `.data` is fuzzy-matched but
    not exact because the link drops the two DEADSTRIP string objects of the dead functions (`@2990`, `@3000`),
    which shifts objdiff's positional pairing of the remaining `@NNN` strings (the sdk-bte-9 `gap_conn` case).
- `L2CA_SetCompression`'s body in the reference is an `#if 0` placeholder; the branch keeps the reference's
  `/* ... */` comment (R37). `L2CA_RegisterCompression` is empty in retail too (`} // 0x80266EFC`).
- `l2c_link.c` includes `hcimsgs.h`, as the reference does (the sdk-bte-9 open item). `rfc_mx_fsm.c` keeps its
  file-local `#define L2CAP_SUCCESS 0`: neither reference's `l2c_api.h` defines the L2CAP result codes (the
  sdk-bte-11 open item).
- Open items: `l2c_link.c` keeps the reference's two naming TODOs on `btsnd_hcic_reject_conn`'s reason arguments
  (0x14/0x15): `hcidefs.h` names 13/15 `HCI_ERR_HOST_REJECT_RESOURCES`/`_DEVICE`, while retail sends 0x14/0x15
  (the Bluetooth "low resources"/"power off" codes), so the literals stay (R7 naming open). `l2c_utils` and
  `sdp_main` keep their `lbl_` `.data` names (compiler-pooled strings, sdk-bte-3 data-score ruling). `sdp_utils`'
  `.text` split ends at its last instruction (0x80277F54); the 12-byte pad before 0x80277F60 stays with the
  following dtk asm unit (the sdk-bte-8 `gki_ppc.c` case). `l2c_api.c` keeps the reference's decompiler
  placeholder parameters `param_6` (`L2CA_SetCompression`) and `param_1`/`param_2` (`L2CA_RegisterCompression`) as
  R7 open naming work; the other R7 hits (`local_cid`, `local_id`, `local_addr`, `local_features`, `param_len`,
  `param_buf`) are the reference's own named parameters, not placeholders.

## sdk-bte-15 rulings

- Three stack units from the R4QK01 reference are Matching, `BTELib` default `GC/3.0a5`, one `flip:` commit each
  after its own clean gate (`build/R5WEA4/main.dol: OK`): `port_rfc.c` (18 functions), `rfc_port_fsm.c` (17) and
  `rfc_ts_frames.c` (15). This closes the BTE range: 68 of 68 units, all 731 functions, and the gate's report
  scores the three 100% code, 100% data and 100% functions (18/18, 17/17, 15/15). Reference text adapted only in
  layout (includes as `<revolution/bte/...>`, 4-space indent, Reference line, Broadcom Apache-2.0 notice,
  sdk-bte-6/8/9); no shared header changed, because every declaration these units need was already in
  `include/revolution/bte/`.
- Function banners are this project's: neither reference carries one for these three files, so they are on every
  live function, ending in the function's address (sdk-bte-14 wrote the same for its units). This closes the
  sdk-bte-13 open item for `port_rfc.c`, `rfc_port_fsm.c` and `rfc_ts_frames.c` only; `btm_acl.c`, `btm_devctl.c`,
  `btm_inq.c`, `btm_sco.c` and `btm_sec.c` still have no function banners (still open).
- No copy in retail (the dead-stripped ruling): `port_rfc.c` `port_open_continue`, `port_start_control`,
  `port_start_par_neg`, `port_start_close`. Bodies kept, marked `// no copy in retail (dead-stripped)`. Three
  helpers reach retail only inlined, marked `(dead-stripped or inlined)`: `rfc_send_nsc` (`rfc_ts_frames.c`, whose
  build is inlined into `rfc_process_mx_message` at 0x8027208C-0x802720E8 with no `bl rfc_send_nsc`), and
  `port_get_credits` (`port_rfc.c`, expanded into `PORT_ParNegInd` and `PORT_ParNegCnf`) and
  `rfc_port_uplink_data` (`rfc_port_fsm.c`, expanded into `rfc_port_sm_opened`). `port_get_credits` and
  `rfc_send_nsc` have an out-of-line copy in the compiled object, which the linker drops; `rfc_port_uplink_data`
  is fully inlined and has no copy in the object either (the object has 17 functions, one fewer than the source).
- `rfc_process_nsc` (`rfc_port_fsm.c`) is retail's 4-byte body (`blr`, 0x80270398). The reference's `/* ... */`
  placeholder is replaced by an empty body with an evidence comment, as sdk-bte-13 did for `btm_inq_db_init` (no
  byte change).
- Data: each unit's `.data` is the compiler's string literals (base `-str reuse`), one object per emitted literal.
  The three literals that MWCC addresses off the section-base register (one `lis`/`addi` pair into a base register,
  then `addi rN, base, offset` for each literal of that function) needed their retail blob split first, or the
  one-shot objdiff left them unmatched (99.8-99.95% on `PORT_PortNegCnf`,
  `rfc_port_sm_term_wait_sec_check` and `rfc_port_sm_orig_wait_sec_check`): `port_rfc.c` `lbl_80445978` (0x38)
  into 0x14/0x24 and `lbl_80445AB8` (0x60) into 0x10/0x10/0x18/0x28; `rfc_port_fsm.c` `lbl_804462E4` (0x5C)
  into 0x2C/0x30 and `lbl_80446380` (0x60) into 0x30/0x30; `rfc_ts_frames.c` `lbl_80446510` (0xD8) into twelve
  parts of exact size (0x10, 0x12, 0xA, 0x9, 0x17, 0xE, 0x13, 0x1E, 0xD, 0xD, 0xE, 0xE). The first two units'
  parts carry the retail padding inside the part before the next literal (sdk-bte-11 precedent: the sizes run
  to the next part's start); `rfc_ts_frames.c`'s exact sizes let dtk name the padding as gaps and make the
  comparison exact. `gatecheck layout` reports "layout matches" for all three units and all their sections.
  The retail `.data` also holds the dead functions' trace literals (port_rfc's first two literals belong to
  `port_open_continue`), which is why the split follows the compiler's literal order and not only live
  references. The one-shot `objdiff-cli diff` still scores the three units' `.data` 99.5-99.9% (99.69 `port_rfc`,
  99.91 `rfc_port_fsm`, 99.53 `rfc_ts_frames`) and `rfc_ts_frames.c`'s `.sdata` 96.77% for the remaining object
  pairing; the gate's report scores both code and data 100% for all three.
- `rfc_ts_frames.c`'s two small error literals, `"Bad UA"` and `"Bad DM"` (7 bytes each), sit in `.sdata`
  (0x80528D90 / 0x80528D98), as retail has them: the default `-sdata 8` places short literals there. Retail's two
  objects are followed by one padding byte each (span 0x10), the built section is 0xF with 8-byte alignment;
  contents, order and the gate agree (a COARSE note at most, no finding).
- **R4 and the frame-header offset (sdk-bte-14 precedent).** The four RFCOMM frame builders (`rfc_send_sabme`,
  `rfc_send_ua`, `rfc_send_dm`, `rfc_send_disc`) and `rfc_send_test` offset a payload pointer by
  `L2CAP_MIN_OFFSET` (and `RFCOMM_MIN_OFFSET` in `rfc_send_test`), which the checker reads as pointer arithmetic on
  an offset macro. Where the buffer's `offset` field is known at the site, the sdk-bte-14 remedy would be to read
  it back, but that was measured here: `(UINT8 *)(p_buf + 1) + p_buf->offset` makes MWCC emit the load and drops
  the four builders from 100% to 43.9% (`rfc_send_dm` 74.9%), so the reference's expression stays and each site
  carries `// byteptr: file format, frame header after the BT_HDR block; offset would load`. Nine sites: two each
  in `rfc_send_sabme`, `rfc_send_ua`, `rfc_send_dm` and `rfc_send_disc` (the payload pointer and the FCS argument),
  one in `rfc_send_test` (the shifted destination pointer). Joining the builders' two-line
  `*p_data = RFCOMM_*_FCS(...)` statement onto one line puts the note directly above the flagged expression; the
  text is byte-neutral.
- `rfc_port_fsm.c` keeps the reference's functional comment on the ignored SABME retransmission
  (`/* Ignore SABME retransmission if client dares to do so */`); the reference author's personal note above it is
  dropped (no byte change), as R21 asks for comments that explain the code.
- Two Broadcom quirks in `port_rfc.c` are labelled, not fixed (R36, no byte change): `PORT_PortNegInd`'s
  not-found branch stores the peer parameters through the port pointer its own lookup just returned as NULL, so
  retail faults at the store and the `RFCOMM_PortNegRsp` after it is never reached (the store is at
  0x8026CC64-0x8026CCA0, the call at 0x8026CCA8); and `PORT_ParNegCnf`'s first flow-control arm tests
  `PORT_FC_DEFAULT == PORT_FC_TS710`, which is constant-false because `bt_target.h` sets `PORT_FC_DEFAULT` to
  `PORT_FC_CREDIT` (so a peer asking for credit always gets it).
- Open items: R7 naming — `rfc_process_mx_message`'s RPN `param_mask` field and the `local_ctrl` port member keep
  the reference's names (the checker lists both as `[checked]` placeholder names; `rfc_int.h` and `port_int.h` are
  where they would be renamed). The three units' source text is otherwise the reference's. No permuter was used
  for any of the three.

## MSL rulings (msl-3)

- **Library-wide `-fp_contract off` for MSL C.** The fdlibm range (0x801A1DD8-0x801A6248) has no `fmadd`/`fmsub`
  against about 300 `fmul`; `-fp_contract on` fuses fdlibm's polynomial evaluations (`z*(S2+z*(S3+...))`). The flag
  is in `cflags_msl`, as xbret/xenoblade builds its MSL; every previously matched MSL unit keeps 100% with it.
- **fdlibm units follow zeldaret/ss, with the newer MSL forms.** `e_acos`/`e_asin` set `errno = EDOM` before
  returning `NAN` (ss's `PLATFORM_SHIELD` arm); `e_fmod` has the `diff` early-out of the newer MSL (ss's
  `PLATFORM_SHIELD` arm; the GameCube and Wii arms leave 88.9%); `e_rem_pio2` takes ss's `PLATFORM_WII` arm;
  `e_pow` and `k_rem_pio2` scale with `scalbn` (fdlibm's own call, which ss replaced with `ldexp` in k_rem_pio2;
  retail calls `scalbn` at 0x801A1D98). The `PLATFORM_*` conditionals are resolved in the source, not carried.
- **`nan` (`math_ppc.c`, 0x801A60DC) is an empty body** (a lone `blr`): MSL's PowerPC `nan` never builds a NaN.
  Kept as the original's quirk (R36), as in ss and xenoblade.
- **Unit names unconfirmed (R12):** `MSL_C/MSL_Common/Src/secure_error.c`, `extras.c`,
  `MSL_C/MSL_Common_Embedded/Src/math_sun.c`, `MSL_C/PPC_EABI/Src/math_ppc.c` and the fdlibm units
  `MSL_C/MSL_Common_Embedded/Math/Double_precision/{e_acos,e_asin,e_atan2,e_exp,e_fmod,e_log,e_log10,e_pow,
  e_rem_pio2,e_sqrt,k_cos,k_rem_pio2,k_sin,k_tan,s_atan,s_ceil,s_cos,s_floor,s_sin,s_tan,w_acos,w_asin,w_atan2,
  w_exp,w_fmod,w_log,w_log10,w_pow,w_sqrt}.c` take the paths of the ss and xenoblade decomps; no DOL string names
  the files. Each carries the "unit name unconfirmed" comment. `e_exp`/`w_exp` (absent from xenoblade's build) sit
  between `e_atan2` and `e_fmod` and between `w_atan2` and `w_fmod`, by size and by the wrappers' branch targets.
- **`strlwr`/`strupr` (extras.c) named from behaviour (R7):** the two in-place case converters around `stricmp`
  read the locale's lower map (0x10) and upper map (0x0C); MSL's extras.c defines `strlwr` before and `strupr`
  after `stricmp`. `toupper` was added to `include/ctype.h` beside `tolower`.
- **`rand`/`srand` (rand.c, 0x8019C058-0x8019C080):** MSL's generator with its `.sdata` seed `random_next` (initial 1);
  the game's former `fn_8019C058__Fv` in `game/Random.h` was this C function, now called through `<stdlib.h>`.
- **`strlen` (0x801A622C) stays in dtk asm:** it follows extras.c; ss has it in `string.c`, xenoblade in
  `Runtime/__mem.c`, and our `string.c` (0x8019D564-0x8019D9FC) is already split without it. Its owner is open.
- **printf.c (0x80199A24-0x8019BEE8) follows xenoblade, with two zeldaret/ss Wii forms.** `double2hex` casts the
  borrowed nibble and the digit offsets to `unsigned char` (`working_byte |= (unsigned char)(...)`,
  `working_byte += (unsigned char)'0'`) and `float2str` adds `(short)(dec.sig.length - 1)` to the exponent: retail has
  the `clrlwi`/`extsh` of ss's Wii arm, which xenoblade's Wii/1.1 build lacks. `fn_8019A898` is `round_decimal` and
  `fn_8019BACC` is `printf` (variadic, `fwide(stdout, -1)`, `__pformatter(&__FileWrite, stdout, ...)`), by size, order
  and body against xenoblade. `lbl_80427820` is `__lconv` (locale.c's `struct lconv`, `"."` first; declared in
  `MSL_C/MSL_Common/locale.h`, defined by its unsplit owner). Per-object `-str reuse,pool,readonly`: the eight
  infinity/NaN spellings and `""` are one pooled `.rodata` `@stringBase0` (0x25 bytes), as in ansi_fp.c. The
  `long2str` test `num != 0x8000000000000000L` on a 32-bit `long` is always true (MSL's quirk, kept, R36).
  `intmax_t` lives in a new MSL `<stdint.h>`. Unit name unconfirmed (R12), the decomps' path.
- **qsort.c (0x8019BEE8-0x8019C058, `fn_8019BEE8` = `qsort`):** MSL's heapsort, from zeldaret/oot-vc `src/libc/qsort.c`
  unchanged (its `table_ptr`/`swap` macros); it sits between printf.c and rand.c, as MSL's alphabetical link order
  puts it, and its two game callers pass a table, count, size and compare function. `_compare_function` and the
  prototype are in `<stdlib.h>`. Unit name unconfirmed (R12).
- **scanf.c whole (0x8019C080-0x8019D564):** the unit takes back `parse_format` (`fn_8019C080`, 0x694) and
  `__sformatter` (0xC98) with their `.rodata` (the `scan_format` initialiser) and `.data` (four switch tables), from
  xenoblade's scanf.c. `set_char_map` narrows the bit to `unsigned char` (`|= (unsigned char)(1 << (ch & 7))`): retail
  has the `clrlwi` that xenoblade's Wii/1.1 build lacks, the same Wii/1.0 difference as printf.c. Its size-modifier
  enum is untagged: scanf orders the modifiers differently from printf's `enum argument_options` (R1). The callees
  `fn_8019D9FC`, `fn_8019ED7C` and `fn_8019F190` are `__strtold`, `__strtoul` and `__strtoull` (sizes 0x1324, 0x414,
  0x4A8 and their order as in xenoblade's strtold.c/strtoul.c; argument lists as `__sformatter` passes them), declared
  in `MSL_C/MSL_Common/strtold.h` and `strtoul.h`; their units stay in asm.
- **strtoul.c (0x8019ED7C-0x8019F7C0):** `__strtoul`, `__strtoull`, `atoi` and `atol` (`fn_8019F6FC`, the same body as
  `atoi`: `strtol(str, NULL, 10)` inlined) from xenoblade's strtoul.c. `strtol` is defined (MSL's text, inlined into
  both callers; no copy in retail); `strtoul` and `strtoll`/`strtoull`/`atoll` have no copy in retail and are not
  written. `isalpha` was added to `<ctype.h>` beside `isdigit`. Unit name unconfirmed (R12).
- **wmem.c (0x8019F7C0-0x8019F7F0, `fn_8019F7C0` = `wmemcpy`, `fn_8019F7C8` = `wmemchr`):** by size, order and body
  against xenoblade's wmem.c; both are called from wprintf.c. `wmemchr` takes an `int` count, as in xenoblade: retail
  tests it with `cmpwi` (a `size_t` count gives `cmplwi`). `wmemcpy` returns `memcpy`'s result (xenoblade's text has
  no `return`; the tail call is the same). Unit name unconfirmed (R12).
- **strtold.c (0x8019D9FC-0x8019ED7C, `fn_8019ED20` = `strtod`):** xenoblade's strtold.c, with the Wii/1.0 narrowing
  casts that the later build lacks, as in printf.c: the mantissa mask `ui &= (unsigned char)(0xff << ...)` and the
  exponent bytes `dbl_bits[n] |= (unsigned char)(exponent >> ...)` (retail `clrlwi` and `extrwi`). `strtod` never
  stores `*end` (MSL's quirk, kept, R36). `__double_huge` (`.sdata` 0x80528678) belongs to MSL's float.c with
  `__float_nan`/`__float_huge` and is left to that unsplit owner; `HUGE_VAL`/`INFINITY`, `isxdigit`, `DBL_MIN` and
  `DBL_MAX_EXP` (with their `LDBL_` aliases) were added to `<math.h>`, `<ctype.h>` and `<float.h>`. The scanner state
  enums of strtold.c and strtoul.c are untagged: each file has its own states under the same names (R1). Unit name
  unconfirmed (R12).
- **wprintf.c (0x8019F7F0-0x801A1B08):** xenoblade's wprintf.c; the placeholders are `parse_format`, `long2str`,
  `longlong2str`, `double2hex`, `round_decimal`, `float2str`, `__wpformatter` (local, as `__pformatter`),
  `__wStringWrite`, `swprintf` (`fn_801A1984`, called by game code) and `vswprintf`, by size, order and body; the six
  local names repeat printf.c's and were entered in `symbols.txt` by hand. The Wii/1.0 narrowing casts of printf.c
  apply here too (`double2hex`'s `(unsigned char)` nibble arithmetic, `float2str`'s `(short)` exponent add).
  Per-object `-str reuse,pool,readonly`, as printf.c: retail pools the twelve wide infinity/NaN/hex-prefix spellings
  and `L""` into one `.data` `@wstringBase0` (0x6E; MWCC keeps wide literals writable under this option) and the
  narrow `""` into a `.rodata` `@stringBase0`; the library default (`-str reuse` + `readonly_strings`) puts the wide
  literals in `.rodata` (`double2hex` 79.8%). The enums `justification_options`, `sign_options` and
  `argument_options` moved from printf.c into `MSL_C/MSL_Common/printf.h` (shared, R1); the wide conversion record is
  `wprint_format` (name invented: MSL calls both records `print_format`, R1), and `__wOutStrCtrl`, `iswdigit`/`iswupper`
  (new `<wctype.h>`), `wint_t` and `swprintf` were declared. The narrow `%s` branch with `#` reads its length byte
  through the wide `buff_ptr` (MSL's bug, kept, R36). Unit name unconfirmed (R12).

## sdk-usb-1 rulings

- `revolution/usb/usb.c` (0x8027C0A0-0x8027D5F0, 14 functions) follows xbret/xenoblade (the same 2009 SDK build) for
  the live functions, written in zeldaret/tp's and PrimeDecomp/sdk's spelling (`IOSError`, `iusb_ctxt`,
  `__usb.h`). Every pointer test is written against `0` (`(rv = iosAllocAligned(...)) == 0`, `0 == vector`,
  `data == 0`): with this project's C `NULL`, `((void*)0)`, they compile to `li r0, 0; cmplw` where retail has
  `cmpwi` (the dvd_broadway precedent). The request context's union holds typed members (`USB_DevDescr des`,
  `IntBlkCtrlReq reqBuf`) where the references cast a `char` buffer (R4); byte-neutral.
- Dead-stripped functions. Retail `.data` keeps the strings of the functions the link dropped, in source order.
  They have real bodies marked `// no copy in retail (dead-stripped)`: PrimeDecomp/sdk's (also SMGCommunity/Petari's)
  for `_usb_enable_log`, `_usb_disable_log`, `IUSB_OpenDeviceIdsAsync`, `IUSB_CloseDevice`, `__openDevice`,
  `IUSB_GetDeviceList`, the sync and async interrupt and control wrappers, `unicode2ascii`, `_GetStrCb`,
  `IUSB_GetAsciiStr` (without PrimeDecomp's `USB_LOG("GetStr\n")`: retail has no such string), `IUSB_GetAsciiStrAsync`,
  `_GetDescrCb`, `IUSB_GetDevDescr`, `IUSB_GetDevDescrAsync`, the suspend/resume/reset and root-hub functions,
  `IUSB_DeviceRemovalNotifyAsync`, `__checkIsoArgs`, `IUSB_IsoMsgAsync`, `IUSB_DeviceInsertionNotifyAsync` and
  `IUSB_DeviceClassInsertionNotifyAsync`. The built `.data`, `.sdata` and `.sbss` match retail (`gatecheck layout`).
  The functions xbret stubs that own no string and that no reference gives a body
  (`IUSB_DeviceInsertionNotifyAsyncWithTimeout`, `IUSB_SetIsoAlt`, `IUSB_CancelInsertionNotify`) are not written.
- **Bodies without a reference (user's ruling, 2026-11-16, AGENTS.md "Dead-stripped functions").** Five retail strings
  belong to 2009-only functions that no reference writes out (xbret and mscharged place them with
  `DECOMP_FORCEACTIVE`, tp with `dummy_str_*` functions): `"calling short blk transfer fn: ..."` and `"calling long blk
  transfer fn: ..."` between `__IntrBlkMsgInt` and `__CtrlMsgInt`, and `"IUSB_RegisterInsertionNotifyWithIdAsync"` (its
  `__FUNCTION__`), `"%s: Not enough memory\n"` and `"Failed to open %s: %d\n"` at the end. The sibling's link maps
  (Release, Final and Debug, `usb.a usb.o`) list `IUSB_ReadBlkMsg`, `IUSB_WriteBlkMsg` and
  `IUSB_RegisterInsertionNotifyWithIdAsync` as `UNUSED` in the same object, and retail `.data` holds strings only they
  own, so each keeps a body marked `// no copy in retail (dead-stripped)`, modelled on its live twin
  (`IUSB_ReadBlkMsgAsync`, `IUSB_WriteBlkMsgAsync`, `IUSB_DeviceInsertionNotifyAsync`) with a line at the code naming the
  twin and what is unconfirmed (signatures, log placement, the ioctl command, allocation order, clean-up paths). The
  bodies are never counted. The unit is `Matching`.

## crt-1 rulings (match/usb-crt)

- `Runtime.PPCEABI.H/__mem.c` owns `.init` 0x80005F34-0x800062B4 (`memcpy`, `__fill_mem`, `memset`) and `.text`
  0x801A622C-0x801A6248 (`strlen`). `strlen` (weak) sits between MSL `extras.c` and Runtime `__va_arg.c`, the same
  place xbret/xenoblade's `__mem.c` `.text` holds it (just before its `__va_arg.c`); zeldaret/ss puts it in MSL
  `string.c`, but our `string.c` range does not reach it. The adjacency to `__va_arg` decides for `__mem.c`.
- `memcpy` is xbret's `asm` body (ORIGINAL-ASM, table above). `__fill_mem` and `memset` are zeldaret/tp's Metrowerks
  text, including its cast-as-lvalue pointer steps (`((unsigned long*)dst) = ...`, an MWCC extension the runtime
  source uses); `strlen` is xbret's. Each `.init` function carries `__declspec(section ".init")` as in tp:
  `#pragma section code_type ".init"` moved only `__fill_mem`. The `.init` split ends at `memset`'s last
  instruction (0x800062B4): the 12 bytes after it are `__start.c`'s 16-byte function alignment.

## crt-2 rulings (match/crt2)

- `revolution/os/__start.c` owns `.init` 0x800062C0-0x800065B8 (`__check_pad3`, `__set_debug_bba`, `__get_debug_bba`,
  `__start`, `__init_registers`, `__init_data`) and `.sbss` 0x8052AB38 (`Debug_BBA`). The `.sbss` byte sits between
  `OSStateTM` and `OSPlayRecord`, the order xbret/xenoblade's configure lists the three objects in, so the unit is
  the SDK `os` library's (as in both references), not the runtime's. The `.init` split ends at `__init_data`'s last
  instruction; the 8 bytes after it are `__ppc_eabi_init.c`'s 16-byte alignment.
- The C bodies are xenoblade's text (`__copy_rom_section` and `__init_bss_section` inlined into `__init_data`, no copy
  in retail), with tp's linker-table types (`__rom_copy_info { rom, addr, size }`, `__bss_init_info`), now declared in
  `__ppc_eabi_linker.h` beside `_SDA_BASE_`/`_SDA2_BASE_`. The pad-3 word at 0x800030E4 is declared in `OS.h` as
  `__OSGCPad3Button` (name invented), the project's form for low-memory words; the asm keeps the references' low-memory
  addresses as named constants in the unit. `__init_hardware`, `__flush_cache` and `__init_user` are declared once in
  the new `revolution/os/__ppc_eabi_init.h` (xenoblade's header), which `__ppc_eabi_init.c` now includes.
- `MSL_C/MSL_Common/Src/float.c` is a data-only unit: `.sdata` 0x80528670-0x80528680 (`__float_nan`, `__float_huge`,
  `__double_huge`), zeldaret/tp's Wii `float.c` (the same three objects; xenoblade's copy adds `__double_max`,
  `__extended_*`, `__float_max` and `__float_epsilon`, which have no copy in our retail and are left out). Both public
  decomps name the file, so it carries no "unit name unconfirmed" comment. The arrays are `unsigned long`, as
  `math.h` declares them.
- `MetroTRK/Processor/ppc/Generic/__exception.s` owns `.init` 0x80004000-0x80005F34, an assembly source object (the
  first in the project): configure's `as` rule assembles it with `build/binutils` (downloaded by ninja), and
  objdiff pairs the built bytes with dtk's `pad_00_80004000_init` (the table has no function symbol: dtk's
  `FindTRKInterruptVectorTable` pass always adds `gTRKInterruptVectorTable` and `gTRKInterruptVectorTableEnd` as
  labels, and a `type:function` entry under the same name in `symbols.txt` is then duplicated, which the link rejects).
  The text is xenoblade's `__exception.s` with retail's target names (`__TRK_reset`, `TRKInterruptHandler` for
  xenoblade's `__TRKreset`, `TRK_InterruptHandler`) and without xenoblade's 0xC-byte tail fill: our table ends at the
  last slot's `rfi` (0x1F34, dtk's `TRK_TABLE_SIZE`), where `memcpy` begins. Path in the TRK_MINNOW_DOLPHIN layout
  beside `flush_cache.c`; the rule checker does not read `.s` files.
  **Ruling (user, 2026-11-14): `.s` source units are allowed in the tools.** The unit is assembled with binutils `as`
  and every function in it is ORIGINAL-ASM: `gatecheck layout` counts its label-only `.init` section as one object and
  compares its bytes with retail (relocated words masked), `unit check`/`flip` accept dtk's `pad_00_80004000_init`,
  the ledger keys the unit without `.s` and needs no R18 hit for it, and `check_rules` never reads the file. Linked
  from source (`Matching`).

## RenderWare rulings (rw-pilot)

- **Profile (`cflags_rw`, PROVISIONAL).** RenderWare Graphics 3.7 is C, built `Wii/1.0` with base + `-func_align 4
  -fp_contract off -use_lmw_stmw on -str noreuse -O3 -ipa file` (the game profile). Evidence: functions in
  0x80290F00-0x802F5000 are packed at 4 bytes (`-O4,p` alone gives 16); the range has no `fmadds`/`fmsubs`/`fnmsubs`
  against 2,292 `fmuls`; GPR saves are `stmw` (198) except where FPRs are saved too (`_savegpr` 67). `-O3` is
  required: `-O4,p` (pbg's `cflags_rw`) breaks `_rwStreamModuleClose`, `RwStreamRead`, `RwStreamWrite` and
  `RwStreamSkip`; `-O2` gives the same code in bastream. `-use_lmw_stmw on` is proven by rwstring (next item).
  `-str noreuse` is proven by batextur (next items, "RenderWare `-str noreuse`"); `-ipa file`/`function`,
  `-inline deferred` and the compiler version (Wii 1.0 to 1.7, GC) change no function of the RW units so far, so
  they are unproven. No unit needs a per-object flag
  (pbg builds `batkreg.c` with `-lang c++`; ours is plain C).
- **`-use_lmw_stmw on` proven (rw-pilot, plcore batch 2).** `_rwStringOpen` (rwstring.c) saves r27-r31 with
  `stmw`; `-use_lmw_stmw off` changes 39 of its rows (flagsweep). `-ipa function` and `-inline deferred` change no
  function of rwstring, resmem, balist, rwchunkgroup or bacolor; `-O4,p` breaks one function each of resmem and
  rwchunkgroup, as it does bastream. None of these units has a string literal, so
  `-str` stays unproven.
- **References.** Units start from tgsm/pbg `src/rwsdk/src` (CC0) and carry `// Reference:` lines; file names and
  unit order follow the GameCube RW decomps (pbg, bfbbdecomp).

## crayon-1 rulings (match/crayon)

- `game/CDrawingTest` holds CDrawingTest and CCrayon (both registered by its one static initialiser 0x8017A1A4).
  Unit name unconfirmed: the file is named after the first registration string. Open item.
- `unk_80039E40` (constructor 0x80039E40, destructor 0x80039EF8, vptr at 0x5C) is the common base of `unk_800FEB28`
  and `unk_800D2DE4`: both constructors call 0x80039E40 and neither calls the other's; CDrawingTest has it as its
  second base at 0x1D4. Its slots were renamed from `unk_800FEB28`/`unk_800D2DE4` to `unk_80039E40` (byte-neutral, DOL OK).
- CBasicAnimatedObject (registry string, unit 0x80020B04.. not yet reconstructed) is modelled in
  `include/game/CBasicAnimatedObject.h` with the overrides its vtable proves; its fields are typed from use only.
- R17 verdict (merge review): `IsRegistered` in `include/climax/EventHandler.h` binds the registration test to a
  named `bool` so MWCC keeps it as a value (`subf/subf/or/srwi.`) in CDrawingTest's message posts, where `PostMsg`'s
  plain test compiles to a compare and branch. Not accepted as the original form: a one-line predicate whose named
  result exists only to steer codegen is a carrier. `PostRegisteredMsg` (name invented) now returns
  `fn_801AA2AC`'s result from inside its test, which leaves retail's second branch where the expansion ends a
  then-branch; it has two users, `CDrawingTest::fn_801796F8` (99.47%, NonMatching) and `CScalingObject` (Matching,
  14/14 exact, the checker reports no finding). The verdict on the named `bool` of `IsRegistered` is not revisited
  by this change; a reviewer who finds it a carrier makes both users MATCH-PENDING (R17). Retail's second `field_0x4B8` test in
  `fn_801796F8` is kept as an original quirk (`// quirk: tests the flag again`).
- Open for review (reader test): `fn_801785B4` declares `unk_NamedEntryPlayer* player` at the top of the function
  (C-style) and assigns it only before the five result reports; declaring it at its assignment costs 6 rows.
- `CSplashScreenTrigger`'s unit-local render-state helpers moved to `include/climax/RenderState.h` (name invented),
  shared with CDrawingTest; the unit stays 8/8 exact with its data layout unchanged.
- Rebased onto the rw-pilot merge: `include/climax/RenderState.h` uses `rwsdk/badevice.h` (the `RwRenderStateSet`
  macro and the SDK's `rw*` enumerators) in place of its own state enum and engine-global view; `badevice.h` gains
  `RwTextureFilterMode`, `RwTextureAddressMode` and the `RwRenderState`, `RwBlendFunction` and `RwAlphaTestFunction`
  members the helpers use. CSplashScreenTrigger 8/8 and CDrawingTest 28/33 unchanged.
- `unk_8003C454`'s base at 0x1BC is `unk_80039E40` itself, not `unk_800D2DE4`: its constructor calls 0x80039E40 at
  +0x1BC, stores its second vptr at 0x218 and writes its own fields from 0x21C (0x21C, 0x220, 0x2C4). The only
  retail caller of `unk_800D2DE4`'s constructor is 0x800E671C. Byte-neutral (`neutral.exe`).
- `CVoicemailMessageTrigger`'s display-height pair (0x80529BF8/0x80529BFC) is `displayHeightF`/`displayHeight`
  (Display-height ruling).
- Open item (R1): three vector types for one 12-byte float triple: `unk_Vec3` (`game/Vec3.h`), `unk_Vector3`
  (`game/VectorPS.h`, deriving `unk_RwV3d`) and `unk_RwV3d` (`climax/Frame.h`, beside the SDK's `RwV3d` in
  `rwsdk/`), with two `V3dScale` helpers (CDrawingTest's unit-local in-place one on `unk_Vec3`, VectorPS.h's on
  `unk_RwV3d`). Both helper sets are paired-single inline asm (R18, MATCH-PENDING with the paired-single ruling);
  consolidate them once that ruling and the RW type lane settle which type is the game's.
- Open item: the RenderWare calls CDrawingTest makes partly keep game-side `unk_Rw*` types (`unk_RwFrame`,
  `unk_RwMatrix`, `unk_RpAtomic`, `unk_RwCamera`; `RwV3dTransformPoint`, `RwIm3DTransform`, `RwIm3DRenderIndexedPrimitive` keep placeholder
  names); they move to the `rwsdk/` headers as the RenderWare lane reaches those units. Its image, raster and
  texture calls use rwsdk's `RwImage`/`RwRaster`/`RwTexture` and the SDK names since the rw-core merge.

## RenderWare rw-core merge review

- **Game-side RW copies retired (R1).** `climax/Image.h`'s `struct RwRaster` duplicated rwsdk's `RwRaster` (same
  name, two definitions); it and the copies `unk_RwImage`, `unk_RwTexture`, `unk_RwTexDictionary` and the game's
  RW raster, image and texture declarations are gone: climax/Image.h, ImageCodec, Utils, AttributeHandler,
  CDrawingTest and CSplashScreenTrigger use `rwsdk/baraster.h`, `baimage.h`, `baimras.h`, `batextur.h` and
  `plcore/babinary.h` (`RwMemNative16`). Mangled names that change with the types: `fn_801B6E70__FPC7RwImage`,
  `fn_801B6EC8__FP7RwImagePv`, `fn_801B6B54__FP9RwTexturePv`, `fn_801B6BA8__FP15RwTexDictionaryPv`. `badevice.h`
  takes `RwTextureFilterMode`/`RwTextureAddressMode` from `batextur.h` (both headers defined them; C++ rejects the
  repeat). Still duplicated, left to a game-header lane (no clash while no unit includes both): Frame.h's
  `unk_RwFrame`, `unk_RwMatrix`, `unk_RwV3d`, `unk_RwPlane` (rwsdk `RwPlane` in bavector.h) and its declarations of
  `RwFrameUpdateObjects`, `RwFrameGetLTM`, `RwFrameForAllChildren`, `RwV3dTransformPoints`, `RwV3dNormalize`,
  `RwV3dLength` and the matrix functions on those types.
- **batextur `.sdata` `""` literals (gatecheck finding): tool artefact.** Retail holds the two `""` at
  0x80528EB0/0x80528EB4, each one zero byte padded to 4 (`lbl_80528EB0`, `lbl_80528EB4`, 4 bytes each); the built
  object holds them at `.sdata+0x0` and `+0x4` (`@1795`, `@1885`, size 1), with the same bytes and offsets and the
  same section size. `gatecheck layout` counts the zero-size section symbol at `+0x0` as a second object
  (`COARSE ... covers 2 built objects`) and then pairs the rest one off (`MISSING lbl_80528EB4`). Not a layout
  defect; the gate decides when batextur links. Tooling item: gatecheck should skip zero-size section symbols.
- **bavector `_rwVectorSetMultFn` "no copy in retail" (open item for the user).** The dead-stripped ruling
  (2026-10-23) names SDK C units and C++ libraries; RenderWare is a C library outside the SDK, so whether it follows
  the SDK rule (keep the reference's body, marked) is unruled. Here the body is not a reference's text: tgsm/pbg
  names the function but its GameCube copy takes two transforms; the four-transform body is reconstructed because
  `_rwVectorOpen` is exact only with it inlined. bavector is parked, so this blocks nothing yet.
- **Per-object `-inline noauto` (baframe, bacamera): PROVISIONAL**, on parked units only; accepted for merge.

## RenderWare header provenance (rw-headers)

The names, layouts and macro forms below are the RenderWare SDK's. Core and plcore only (`include/rwsdk/world/` belongs to the world lane).

- **Confirmed, "name invented" withdrawn:** `RwCriterionCoreID` and every `rwID_*MODULE` member we use
  (`batype.h`), `rwFRAME`, the `rwFRAMEPRIVATE*` flag names and values (#defines in the header; our
  enum name stays invented), `RwFrameSetFreeListCreateParams`, `_rwFrameDeInit`, `RwFrameSetIdentity`,
  `RwFrameGetPluginOffset`; the module globals types `rwFrameGlobals` (member `frames`), `rwCameraGlobals`,
  `rwRasterGlobals` (all members) and `rwRASTERCONTEXTSTACKSIZE`, `rwStreamGlobals`, `rwVectorGlobals`,
  `rwMatrixGlobals` (all four members), `rwTextureGlobals`, `rwChunkGroupGlobals`; `rwTEXDICTIONARY`,
  `rwTEXTUREBASENAMELENGTH`, `RwTextureCallBackFind`; `RwAlphaTestFunction` (`baimmedi.h`);
  `_rwSListDestroyEndEntries`/`GetBegin`/`GetEnd`; `RwFreeListPurge`, `RwFreeListPurgeAllFreeLists`,
  `rwFREELISTFLAG_STATIC`/`FREEBLOCKS`, `rwMEMHINTFLAG_RESIZABLE` (enum `RwMemoryHintFlag`, now so declared),
  `rwMEMHINTDUR_MASK` (a `RwMemoryHintDuration` member, now so declared), `rwMEMHINTDURFORCEENUMSIZEINT`;
  `rwCHUNKGROUPMAXNAME`, `_rwChunkGroupOpen`/`Close` (`rwgrp.h`); `_rwResHeapInit`/`Close`/`Alloc`/`Free`
  (`resmem.h`); `rwCHUNKHEADERSIZE`, `RWBUILDNUMBER` (101) and `rwLIBRARYBASEVERSION` (`rwversion.h`); the 29
  standard-function slot names and values (`badevice.h`); `RwGlobals`, `RwFreeList`, `RwStringFunctions` and
  `RwMemoryFunctions` member names and order.
- **Renamed to the header's name:** `_rwFrameAddChildNoUpdate` to `RwFrameAddChildNoUpdate` (symbols.txt, source;
  `names.exe rename`); `rwSLISTINITIALSIZE` to `SLIST_STARTSIZE` (the header defines it in balist.h; kept in
  balist.c, its only user); `rwSTANDARDUNK22` to `rwSTANDARDHINTRENDERF2B`, and the invented enum `RwStandardFunction` to the
  header's `#define`s; `rwPLUGIN_ID` (core errors' plugin) to `rwID_COREPLUGIN` of `enum RwCriterionPluginID`
  (rwtypes.h); the error
  sentinels to `E_RW_LASTERROR` and `rwLASTERROR_errcore`; `RwGlobals` `field_0xC` to `pad`; members
  `rwCameraGlobals::cameras` to `cameraFreeList`, `rwChunkGroupGlobals::groupFreeList` to `groupFList`, and the
  texture globals `textureReadCB`/`textureFindCB`/`mipmapping`/`autoMipmapping`/`field_0x24`/`field_0x28`/
  `rasterBuildMipmapsCB`/`mipmapNameCB` to `textureReadFunc`/`textureFindFunc`/`haveTexMipmaps`/
  `generateTexMipmaps`/`scratchMem` (`RwUInt8*`)/`scratchMemSize` (`RwInt16`, with the header's `pad`)/
  `mipmapCallback`/`mipmapNameCallback`. The resource arena state is the header's `rwResources`, nested in
  `rwResourcesGlobals` as `res`; retail's arena has one more word (`requestedSize`, 0x28) than the SDK's.
  `include/rwsdk/rpcriter.h` (file and `rwID_USERDATAPLUGIN` confirmed by `rpcriter.h`) takes the
  header's `#define` form; its invented enum `RwCriterionPluginID` clashed with the header's core/world/tool enum of
  that name (`batype.h`, now in rwtypes.h). `neutral.exe`: no unit's code or data changes.
- **Error macros and plugin IDs (rw-headers merge with rw-world).** The SDK has two `RWERROR`s: the core's
  (`badebug.h`) stores `rwID_COREPLUGIN`, the plugins' (`rpdbgerr.h`) stores
  `rwPLUGIN_ID`, which each plugin's `rpplugin.h` defines (`world/rpplugin.h`: `#define rwPLUGIN_ID 0x02`). The tree
  now follows that: `plcore/badebug.h` and `plcore/rpdbgerr.h` hold the two macros (moved out of `baerr.h`, which
  keeps the error types and functions), the core units include `badebug.h`, and the world units include
  `world/rpplugin.h` first and `rpdbgerr.h` in place of their per-file `#define rwPLUGIN_ID 2` (and the `#ifndef`
  default in `baerr.h` is gone). `RwTexCoords` and `rwPI`/`rwPIOVER2` (with the `#ifndef rwPI` guard, which is the
  SDK header's own text) moved to `plcore/batypes.h`, their SDK home; bamatrix.c's copy of `rwPI` and rwtypes.h's
  are dropped. `neutral.exe`: no unit's code or data changes.
- **Resolved (match/rw-baclump): the `RwFrame` alignment against baclump.** With `baframe.h`'s
  `typedef struct RwFrame RWALIGN(RwFrame, rwFRAMEALIGNMENT)` eight baclump functions inlining
  `RpAtomicSetGeometry` lost their fold of `RpAtomicSetGeometry(atomic, NULL, 0)`. Traced (docs/compiler_regalloc.md,
  "IR optimiser gate"): `IRO_CanThisIRBeOptimized` turns the whole IR optimiser off for a function, after inlining,
  when one expression compares a pointer to an aligned typedef (`frame != NULL`) or reads a member through one
  (`frame->root`); naming, passing, casting or truth-testing the pointer does not. baclump's frame tests were spelled
  `!= NULL`/`== NULL`; written as truth tests (`if (frame)`, `if (!RpClumpGetFrame(...))`), the spelling of the SDK
  header's own macros (`world/baclump.h` `RpInterpolatorSet*Macro`: `(RpAtomicGetFrame(_atomic)) ? ...`), baclump is
  44/44 with the aligned `RwFrame`, `ClumpAtomicCloneAndAttach` included (it stays unoptimised through
  `frame->root`, as retail shows). Rule for RenderWare units: test aligned-type pointers (`RwFrame*`, `RwMatrix*`,
  `RwCamera*`) for truth, as the SDK does; a `!= NULL` comparison of one is a codegen change, not a style choice.
- **Still invented (no header evidence):** the error codes' names and numbers (the header enums `RwErrorCodeCommon`
  and `RwErrorCodePlugin_errcore` are built from `.def` files: `E_RW_ZEROLENGTH`,
  `E_RW_INSUFFICIENTRESOURCES`, the image-format code 0x80000009 stay as they are), `rwERRORCOMMONFLAG`, the
  `RW*GLOBAL` accessor macros other than `RWRESOURCESGLOBAL` (only that one is in a header), the module-info and
  free-list statics, the square-root table macros, `rwRADIANSPERDEGREE`, the bamatrix and bamemory static helpers,
  `rwResHeap`/`rwResHeapBlock`/`rwRESHEAPALIGN`/`rwRESHEAPBLOCKUSED`/`ResHeapSplitBlock`, the stream and texture
  file-local constants, `rwStreamFrame`.
- **Open (unit name):** the chunk group module's header is `rwgrp.h` in the SDK, which points to `rwgrp.c` for our
  `rwchunkgroup.c` (whose file name is marked invented); renaming the unit is left to a split pass. The SDK's
  `badevice.h` is in `plcore/`; ours is `rwsdk/badevice.h` (header placement, no code effect).

## RenderWare plugin rulings (rw-plugins)

- **Plugins present (plugin IDs registered in retail).** Collision (`0x11D`, attach 0x802B3D18), delta morph
  (`0x122`), material effects (`0x120`) with multitexture (`0x12C`), `0x129` (0x802C1070, 0x802C2BE4), HAnim
  (`0x11E`, attach 0x802C4970), user data (`0x11F`, attach 0x802CBE30), UV animation (`0x135`, attach 0x802CC90C)
  and skin (`0x116`, attach 0x802EA914). No patch ID (`0x123`) or PTank ID is registered.
- **`rwsdk/plugin/userdata/rpusrdat.c`** (0x802CAFF0-0x802CC0E4): unit name and function names from
  bfbbdecomp/bfbb's splits and symbols (`UserDataOpen` .. `UserDataObjectGetSize`, `RpGeometryGetUserDataArray*`,
  `RpUserDataPluginAttach`); the public accessors (`RwFrameGetUserDataArray*`, `RpMaterialGetUserDataArray*`,
  `RpUserDataArrayGet*`) follow the RW 3.7 API naming of those. Names invented: `UserDataArrayCopy`,
  `UserDataStreamGetSize`, `UserDataListStreamWrite` (retail functions bfbb does not have), the inlined helpers
  `UserDataArrayDestroy`, `UserDataListDestroy`, `UserDataListStreamRead`, `UserDataListStreamGetSize`,
  `UserDataListGetCount`, `UserDataFormatSize`, the macro `rwstrdup`, the type `RpUserDataList` and the
  `userData*Offset` statics. Evidence for the helper structure: every `UserDataObject*` callback and count accessor
  is exact only with its list loop in an inlined list helper (inlined code takes retail's register colouring; the
  same loops written in the callback colour the strength-reduced offset first), and `UserDataArrayCopy` and
  `UserDataStreamWrite` are exact only with a non-`const` array parameter (with `const`, MWCC hoists the source
  array's loads over the stores to the destination). The attach checks the world sector registrations against the
  geometry offsets (retail's slip, kept and commented).
- **Plugin IDs.** `include/rwsdk/rpcriter.h` (header and `rwID_USERDATAPLUGIN` confirmed by the SDK
  header, rw-headers; values from retail's registrations) and `rwVENDORID_CRITERIONTK` in `rwtypes.h`
  (bfbbdecomp/bfbb rwplcore.h).
- **Merge note (world lane).** `include/rwsdk/world/bageomet.h`, `basector.h`, `balight.h` and `bamateri.h` are
  new here with only an opaque typedef and the `Rp*RegisterPlugin`/`RegisterPluginStream` declarations the plugins
  call (bfbbdecomp/bfbb rpworld.h); `symbols.txt` names those eight functions (0x802D275C .. 0x802D812C). The world
  lane owns these headers: on merge keep its contents and add any declaration missing. `babincam.h` and
  `babintex.h` (core) declare `RwCameraRegisterPluginStream` and `RwTextureRegisterPluginStream`.

## RenderWare toolkit rulings (rw-plugins2)

- **`rwsdk/tool/anim/rtanim.c`** (0x802F5F10-0x802F7288): unit name and function names from tgsm/pbg's splits and
  symbols (`AnimOpen` .. `RtAnimInterpolatorBlendSubInterpolator`); `RtAnimGetInterpolatorInfo`,
  `RtAnimAnimationGetNumNodes` and `RtAnimInterpolatorSetAnimLoopCallBack` (retail functions pbg lacks) and every
  type, macro and prototype from the RenderWare SDK header `rtanim.h`; `RtAnimInterpolatorInfoBlock`/`...NumEntries` from bfbbdecomp/bfbb. Names invented:
  the four `_rtAnimAnimationFreeList*` statics, `rtANIMMAXINTERPOLATIONSCHEMES` and the error enum `RtAnimError`
  (`E_RT_ANIM_*`; the real names are in the unpublished `rtanim.rpe`). Retail deviations from the 3.7.0.2 header:
  `RtAnimInterpolatorInfo` is 0x34 bytes (one word at 0x30 beyond `customDataSize`, only ever copied:
  placeholder `field_0x30`); the animation chunk header carries build number 0x55 where the core writes 0x65, so
  the unit redefines `RWBUILDNUMBER` (the toolkit was built from another SDK build); the animation free list is
  created at open but `RtAnimAnimationCreate` allocates with `RwMalloc` (kept as retail shows).
- **`rwsdk/tool/dict/rtdict.c`** (0x802F7B3C-0x802F808C): unit name unconfirmed (named after the header
  `rtdict.h`; open item). Names, types and the error ID `rwID_DICTTOOLKIT` (0xBF) from the RenderWare SDK
  headers `rtdict.h` and `rpcriter.h`. `RtDictSchemaCreateDict`, `RtDictSchemaAddDict`,
  `RtDictSchemaRemoveDict`, `RtDictAddEntry`, `_rtDictSchemaInitDict` and `_rtDictDestruct` have no copy in retail
  (inlined into `RtDictDestroy` and `RtDictSchemaStreamReadDict`); their bodies are kept and marked.
- **Plugin IDs.** `include/rwsdk/rpcriter.h` gains `rwID_ANIMTOOLKIT` (0xB7, RtAnim's plugin registration) and
  `rwID_DICTTOOLKIT` (0xBF, RtDict's error ID), from the SDK header.

## RenderWare world rulings (rw-world)

- **Units and names (R12, R7).** The eleven `rpworld` units `rwsdk/world/babinwor.c` .. `baworobj.c`
  (0x802CDCA8-0x802DB4FC) follow bfbbdecomp/bfbb `splits.txt` (file names and link order) and are cut where the
  world plugin attach (`RpWorldPluginAttach`, IDs 0x501-0x50B) and each module's data put them; the file names
  are evidenced, so no "unit name unconfirmed" comment. Function and data names are bfbb/tgsm/pbg `symbols.txt`
  names (GameCube RW 3.x; matched by order, calls, data and behaviour, not size alone). The world headers
  `include/rwsdk/world/*.h` take their types from bfbb `include/rwsdk/rpworld.h` (CC0), checked against retail
  registry/free-list sizes and field offsets. Deviations from bfbb's GC layout, proven by retail and confirmed by
  the RenderWare SDK world headers: `RpWorldSector` is 0x88
  (no `noCollAtomicsInWorldSector` list) and holds `RpTriangle* triangles` / `numTriangles` (bfbb's `RpPolygon`
  has the material index first; the 3.7 `RpTriangle` has it last, at +6); `RpTriangle.matIndex` is `RwUInt16`
  (retail stores 0xFFFF); `RpAtomic` +0x62 is the header's `pad` (create and clone store 0x101). `RxPipeline`
  (bapipe.h) takes bfbb rwcore.h's struct. `baworobj.c` ends at 0x802DB4FC: the light's
  world getter 0x802DB4F0 reads the unit's static `lightExtOffset`, so it is the unit's last function.
- **Names confirmed by the SDK headers** (formerly invented): the module globals types and members
  `rpClumpGlobals` (`atomicFreeList`, `clumpFreeList`), `rpLightGlobals` (`lightFreeList`), `rpMaterialGlobals`
  (`matFreeList`), `rpMeshGlobals` with `RPMESHGLOBAL` (formerly `RWMESHGLOBAL`), `rpGeometryGlobals`
  (`geomFreeList`, now sizing the world attach's registrations); `RwCriterionWorldID` and its `rwID_` members
  (completed from baworld.h); the stream chunk structs `RpLightChunkInfo`, `RpMaterialChunkInfo`,
  `RpClumpChunkInfo`, `RpGeometryChunkInfo`, `RpWorldChunkInfo`, `RpPlaneSectorChunkInfo`, `RpWorldSectorChunkInfo`
  (formerly `rpLightChunkInfo`, `rpMaterialChunkInfo`, `rpClumpBinary`, `rpBinaryGeometry`, `rpBinaryWorld`,
  `rpBinaryPlaneSector`, `rpBinaryWorldSector`), moved into the world headers; `RpTie` and `RpLightTie` with their
  members; `rpATOMICPRIVATEWORLDBOUNDDIRTY` (`enum RpAtomicPrivateFlag`, typedef `rpAtomicPrivateFlag`); the
  functions `RpAtomicSetFreeListCreateParams`, `RpMaterialSetFreeListCreateParams`,
  `RpLightTieSetFreeListCreateParams`, `RpGeometryGetPluginOffset`, `RpMaterialGetPluginOffset`, `RpMaterialClone`,
  `RpClumpGetWorld`, `RpLightGetWorld`, `RpWorldLock` (formerly `fn_802D8A8C`, the pair of `RpWorldUnlock`); the
  macros `RpMaterialGetPipelineMacro`/`RpMaterialSetPipelineMacro` (world/pipe/p2/bapipew.h). The headers also
  give `RpGeometryForAllMeshes` a const geometry (which makes it exact) and `_rpWorldPipelineClose` a void return.
- **Names invented (R7), marked at their declarations**: the module
  globals macros `RWCLUMPGLOBAL`, `RWLIGHTGLOBAL`, `RWMATERIALGLOBAL`, the world module's
  `rpWorldGlobals`/`RWWORLDGLOBAL` and `rpWorldObjGlobals`/`RWWORLDOBJGLOBAL` with their members,
  `rxPipelineGlobals`/`RXPIPELINEGLOBAL` (its use in bapipew.h confirms only the macro's name); the stream chunk
  struct `rpAtomicBinary`; `rpWorldListEntry`, `rpWorldFindSectorData`, `rpClumpObjectExt`, `rpClumpCloneData`,
  `rpGeometryList`, the world-object extensions `rpWorldObjCameraExt`/`AtomicExt`/`LightExt`/`ClumpExt`, the
  tristripper types `TriStripTri`, `TriStripEdge`, `TriStripFreeLists`, `TriStripListEntry`, `TriStripMaterialMesh`,
  `rpMeshopStatic` (this build's tristripper predates the 3.7 headers' tunnel stripper, whose `RpTriStrip*` types do
  not fit); the functions `ClumpDestroyLight`/`Camera`/`Atomic`, and the inlined statics `LightACos`,
  `MaterialListSetSize`, `GeometryListDeinitialize`, `WorldMemoryTake`, `BuildMeshFreeListCreate`/`Destroy`,
  `MeshObjectHasIndices`, `TriStripBinRemove`, `WorldAtomicDestroyTies`, `WorldLightDestroyTies`,
  `WorldAtomicTieSector`, `WorldLightTieSector`;
  data `buildMeshFreeList`, `lastSeenSectExtraData`/`lastSeenSectRightsPluginId`; constants and macros
  `rwLIGHTCONEVERSION`, `rpMATERIALRIGHTSEXTRASIZE`, `rpMATERIALLISTGRANULARITY`, `rpBUILDMESHFREELISTBLOCKSIZE`,
  `rpMESHSTREAMINDICES`, `rpMESHHEADERPRIMTYPE7`/`rwPRIMTYPE7`, `rwSECTORBUILD`, `rpWORLDTEXCOORDSETSMASK`,
  `rpWORLDPOLYGONMATINDEXLASTVERSION`, `rpWORLDOBJFRUSTUMSECTORSGROW`, `GETCOORD`, `RwV3dGetSectorCoord`,
  the `WORLD*EXT`/`CLUMP*EXT*` accessors, the `TriStripEdge*` macros,
  `TRISTRIPFINALPASS`, `RasterHasAlpha`, `RwRealMax2`/`RwRealMax3`, `E_RP_WORLD_CLUMPNOTINWORLD` (4),
  `E_RP_WORLD_ATOMICNOFRAME` (5) (the error codes live in `rperror.def`, which is not a header and stays unread).
  Placeholders left: `_rxWorldDevicePluginAttach` (the pipeline attach the world attach calls; `_rpWorldPipeAttach` is
  0x802DB9C0 by bfbb's order; bapipew.h also declares `_rxWorldDevicePluginAttach`, unconfirmed),
  world-object globals `field_0x8`, clump extension `field_0x4`, `lbl_8052B408` (the atomic being rendered; never read).
- **Original quirks (R36), labelled at the code:** `RpLightCreate` sets `minusCosAngle` to 0.0f; `RpLightStreamRead`
  keeps the light when its plugin chunks fail; light chunks before 0x30300 store the cone as a tangent;
  `RpMaterialCreate` (inlined, no copy) sets `pad = 1`; `_rpClumpOpen` creates the atomic free list with the clump
  memory hint and the clump free list with the atomic one; `_rpMeshRead` sizes 16 bytes per mesh; this build's mesh
  flags carry a seventh primitive type (0x20); `_rpMeshOptimise` returns a static empty header for a build mesh with
  no triangles; the tristripper sizes its 2-byte index buffers at 4 bytes each and checks only its first `RwMalloc`;
  `RpWorldCreate` reports `sizeof(RpWorldSector*)` in its NOMEM error; `WorldLightSync` raises `E_RW_NULLP` and
  still returns the object.
- **Inlined public functions with no copy in retail**, kept and marked `// no copy in retail (dead-stripped or
  inlined)` as babbox.c does: `RpMaterialCreate`, `RpAtomicSetGeometry`, `RpClumpAddLight`, `RpClumpAddCamera`,
  `RpClumpRemoveLight`, `RpClumpRemoveCamera`; `_rpWorldUnregisterWorld` and `WorldFindSector` are inlined statics.
  `_rpMaterialListStreamGetSize`/`StreamWrite` and the GC extras without a Wii copy are not written.
- **`rwPLUGIN_ID`.** Each world unit includes `world/rpplugin.h` (`rwPLUGIN_ID` 0x02, the world plugin's error ID)
  and `plcore/rpdbgerr.h` (the plugins' `RWERROR`), as the SDK headers do (rw-headers merge).
- **Reference outside the RW list:** `balight.c`'s inlined arc cosine follows fdlibm `e_acosf.c` (freely
  redistributable Sun licence) for its constants and steps, with `_rwSqrt` and a clamp in place of NaN.
- **Reader-test items for review (not [match] findings):** `RpClumpCreate` calls `RpClumpSetFrame(clump, NULL)`
  right after `rwObjectInitialize`; it changes no byte of `RpClumpCreate` but keeps it out of line in `RpClumpClone`
  as retail has it (inline budget; R17 ruling wanted). `TriStripFollow` spells its booleans
  `deadEnd = TRUE; if (... == FALSE) deadEnd = FALSE;`; `TriStripStripTris` repeats the "add strip and break" tail
  in both arms; `_rpMaterialListAppendMaterial` repeats its append tail in both arms; `RpGeometryForAllMeshes`/
  `RpWorldForAllClumps` loop guards; some readers' local declaration orders were found by search (comments at the
  code). Merge review verdicts: the duplicated tails (`_rpMaterialListAppendMaterial`, `TriStripStripTris`) and the
  searched declaration orders pass the reader test (two copies in retail need two in the source; no dummy locals).
  `TriStripFollow`'s `deadEnd`/`canSwap` spellings sit in parked bameshop: they read as the expansion of a statement
  macro taking the `used`/`testUsed` member (as `TriStripEdgeGetFreeTri` does), to be recovered as one before
  bameshop flips. `RpClumpSetFrame(clump, NULL)` in baclump's `RpClumpCreate` (match/rw-baclump): retail's
  `RpAtomicCreate` makes the same redundant reset after `rwObjectInitialize` as a real call
  (`_rwObjectHasFrameSetFrame(atomic, NULL)` at 0x802D04E0, the inlined `RpAtomicSetFrame(atomic, NULL)`), so the
  creators share the idiom and the statement is the original's, not a steering line; it passes R17 on that evidence.
- **Closed walls (match/rw-world-walls, Gormiti DWARF):** baworobj `WorldSyncCamera` (frustum test written out in the
  loop, locals in the DWARF order with `spSect` declared last; `const` plane pointer), bameshop `SortPolygons` (the
  triangles are read through the qsort element pointers, no triangle locals) and bageomet `RpGeometryStreamRead`
  (each size and index local in its own block). baworld `RpWorldSectorGetWorld` closed (match/rw-walls): the list entries are read through the SDK
  macro `rwLLLinkGetConstData` (the function changes no entry); with const reads of `entry->world` the flag and
  range tests share one load and the inlined `RpWorldForAllWorldSectors` loads it again, as retail does
  (docs/compiler_regalloc.md, "Common subexpressions"). baworld is `Matching`.
- **bageomet names invented (R7):** `E_RP_WORLD_TOOMANYVERTICES` (6), `rpGEOMETRYTEXCOORDSETSMASK`, the stream structs
  `rpBinaryTriangle`, `rpBinaryMorphTarget` (the header's `RpGeometryChunkInfo` replaces `rpBinaryGeometry`), the
  inlined static `GeometryDestroy` (the body
  `RpGeometryDestroy` and every `RpGeometryStreamRead` failure exit share). Quirks (R36): `RpGeometryCreate` leaks its
  block when `_rpMaterialListInitialize` fails; `RpGeometryStreamRead` returns NULL without destroying the geometry
  when the material list chunk is missing.

## RenderWare core rulings (rw-core-rest)

- **Units and names.** `babincam`, `babintex` and `basync` are cut and named by tgsm/pbg `config/GPLE9G/splits.txt`
  (babincam after babbox, babintex after babinfrm, basync after baresamp) and take pbg's `symbols.txt` names
  (`RwTextureStreamGetSize`, `RwTextureStreamWrite`, `RwTexDictionaryStreamRead`, the local `destroyTexture`,
  `FrameSyncHierarchyRecurse`, `FrameSyncHierarchyRecurseNoLTM`, `FrameSyncHierarchy`,
  `FrameSyncHierarchyLTMRecurse`). pbg's babintex also holds the string stream functions; here they sit in rwstring.c.
- **Per-object `-inline noauto` (basync): PROVISIONAL.** Under the library's `-inline auto` the compiler inlines
  `FrameSyncHierarchy` into `_rwFrameSyncDirty` and unrolls the recursive syncs into themselves (0/6); retail calls
  each out of line. With `-inline noauto` all six functions match, as for baframe and bacamera (rw-pilot ruling).
- **basync `rwFrameSyncObjects` (name invented).** The object sync loop of the three hierarchy walkers is a statement
  macro guarded by `rwLinkListEmpty`: retail tests the list twice before the loop (once for the guard, once at the
  loop head), and only a block expanded in the caller colours the walker's flags before the loop's link (a `static
  __inline` helper gives the same instructions with two registers swapped in `FrameSyncHierarchyRecurse`).
  `_rwFrameSyncDirty` writes its own loop without the guard, as retail has it.
- **babintex names invented (R7):** `rwStreamTexture` (`filterAndAddress`), `rwStreamTexDictionary` (`deviceId`,
  `numTextures`, in big-endian order) and the `rwSTREAMTEXTURE*` field constants. `RwTextureStreamFlags` and
  `rwDEVICESYSTEMGETID` are header evidence. Quirk (R36): `RwCameraStreamRead` (babincam) returns NULL without
  destroying the camera when its plugin data fails to read.
- **Declaration orders found by search:** `RwTextureStreamRead` (`texture`, `filtering`, `mipmapState`,
  `autoMipmapState`, `flags`, `addressingU`), as for the world readers.

## SDK wall rulings (sdk-walls2)

Eight parked units closed to 100% under `match.exe` (137 of 137 functions); the earlier "Parked" notes for them in
this file (the NdevExi2AD `-O0` ruling's 99.0%/94.0%, the `global_optimizer` ruling for OS.c, the OSSemaphore,
`__OSLaunchNextFirmware`, `GXLoadTexMtxImm` and dvd residues) are superseded by the entries below.

- **NdevExi2AD (`DebuggerDriver.c`, `exi2.c`): `GC/3.0a5` with `-O0 -inline off -func_align 4` per object.** `DBWrite` and
  `__EXI2Imm` differed only in register numbers. Unoptimised MWCC gives registers to the most-used locals first and
  breaks ties by declaration order; `Wii/1.0` and `GC/3.0a5.2` count a local's declaration as a use, which reorders
  those numbers, while the `GC/3.0a3` to `GC/3.0a5` builds do not. `build/tools/flagsweep.exe main/revolution/NdevExi2AD/exi2 -v all -s __EXI2Imm`
  scores `__EXI2Imm` 1/1 for `GC/3.0a3.x`/`3.0a5` and 0/1 (10 rows) for every `Wii/*` build and `GC/3.0a5.2`; `GC/3.0a5`
  is kept as the build the BTE protocol stack uses. `-func_align 4` (per object): retail packs the library's functions at 4 bytes, where the build otherwise aligns them to 16 (0x74 and 0x20 bytes longer; `gatecheck layout` matches with it). `DBWrite` declares `ofs` and `mail` before `enabled`. `-inline off`
  states what `-O0` already implies for the helpers. Provisional, as the library's other flags: no second unit decides
  between the `3.0a` builds. Both units stay `NonMatching` until the gate runs.
- **dvd `cbForStateOpenPartition`, `cbForStateCheckID2`: `BB2` is 32-byte aligned** (`ATTRIBUTE_ALIGN(32)`, as in
  doldecomp/ogws). An aligned object referenced from a function, an inlined body included, turns the IR optimiser off
  for that function, which keeps the inlined `StampIntType`'s `lastCommand[CommandInfoCounter - 1]` unfolded as in
  retail. Retail's `.bss` gap before `BB2` agrees with the alignment.
- **GXTransform `GXLoadTexMtxImm`: `#pragma optimizewithasm off`** after the includes, with the `asm volatile` blocks
  written as plain `asm` and the `volatile void* wgpipe` local of `__GXSetProjection` removed. The pragma and its
  scope are in SMGCommunity/Petari and koopthekoopa/wii-ipl `GXTransform.c`; functions that expand the inline-asm
  matrix helpers are then not IR-optimised, so the two-armed selections stay branches. This replaces the earlier
  `opt_generateconditionalassignments off` experiment, which no reference used.
- **hbm `synctrl` `__HBMSYNResetAllControllers`: `__HBMSYNSetController` and `__HBMSYNGetWavetableData` are external
  again**, declared in `synctrl.h` as in the reference (sdk-hbm-16 made them `static` on the argument that an external
  definition adds copies retail lacks). External linkage changes which calls MWCC inlines and in what order, and
  closes the schedule residue; the linker then drops the two unreferenced copies, so the DOL has no copy of either
  (`// no copy in retail (dead-stripped or inlined)`). The units compiled object does carry the two extra symbols
  (`neutral`: "no retail counterpart"). This departs from the library-family rule "C++ libraries declare only" because
  the bodies are needed at the inlined call sites; whether the gate accepts it is decided by the flip of `synctrl`.
- **os `OSInit`, `OSGetAppGamename`, `InquiryCallback`: the three `#pragma global_optimizer off` regions are removed.**
  Retail's `DriveInfo` is a 32-byte-aligned DMA target (xbret/xenoblade `OS.c`), which turns the IR optimiser off
  for the functions that touch it (the same mechanism as dvd `BB2`; inferred from the bytes, `OSInit` and `InquiryCallback` match without the pragmas). `OSGetAppGamename` takes
  xenoblade's loop spelling. This resolves the R17 lever ruling of "OS unit rulings (sdk-os-4)" for OS.c.
- **os `__OSLaunchNextFirmware`: locals as in doldecomp/brawl** (`u8 i` at function scope, `info` last), with the IOS
  version as `ESTitleIdAligned` (32-byte aligned like every ES title ID, closing the register shift the earlier
  `volatile numGamePartitions` experiment could not).
- **os `OSWaitSemaphore`: `OSSemaphore.count` is `volatile s32`**, the declaration of the OpenRVL header in
  doldecomp/brawl (`include/OpenRVL/include/revolution/OS/OSSemaphore.h`, checked); it supersedes the earlier ruling
  that tp's plain `s32` is the SDK's.
- **Flipped (gate `OK-PENDING`):** OSSemaphore, OS, OSExec, GXTransform, synctrl. **Open, stay `NonMatching` (gate red, source
  unchanged):** dvd (`.bss` is 0x30 short: retail's `__DVDStopMotorCommandBlock` slot is 0x60, one 0x30 object after the
  command block has no definition; code differs in 23 symbols from the shifted `.bss` addresses); `DebuggerDriver`
  (retail emits the public functions first and the static ODEMU helpers after `DBClose`, in reverse header order,
  then the mailbox statics; the build emits the statics first, so every function sits at the wrong address); `exi2`
  (the unit alone differs in 7 bytes of the asm-linked `DebuggerDriver`, cause not isolated).

## FMOD rulings (fmod-1)

- **Profile.** FMOD Ex is built with the game's flags (`cflags_fmod` = `cflags_game` + `-i include/fmod`,
  `Wii/1.0`); evidence and what is provisional are in the toolchain row (docs/toolchain.md). Proven on fmod/fmod:
  `-O3` (each other `-O` level breaks a function), auto inlining, `-str noreuse` (two `"fmod.cpp"` objects in
  `.data`). Proven range-wide from retail: 4-byte function packing, no fused multiply-adds, null RTTI words.
  Provisional: `-ipa file` (neutral here), the compiler build (Wii 0x4201_127 to 1.7 give identical code), and
  the 53 functions that save GPRs with `_savegpr` but no FPRs.
- **Unit boundaries.** `fmod.cpp` is 0x8032BA54-0x8032BDAC: the start of the FMOD range (the two thunks before it
  branch into the preceding library), `"fmod.cpp"` tags in `FMOD_System_Create`, and its end at the first function
  of `fmod_async.cpp` (0x8032BDAC, a thread-entry thunk whose address only the `fmod_async.cpp` function
  0x8032C49C takes; `"fmod_async.cpp"` tags from 0x8032BE64 to 0x8032C49C; the unit ends with its `.ctors` entry
  0x8032C694 at 0x8032C6B0). The weak `FMOD::Thread`-like destructor 0x8032BDB0 (vtable lbl_8044FB30) is placed
  with `fmod_async.cpp`.
- **Line numbers are bytes.** FMOD's allocator calls take `__FILE__` and `__LINE__` (`FMOD_Memory_Calloc`,
  `FMOD_Memory_Free` in include/fmod/fmod_memory.h). fmod.cpp's C functions that retail does not call were
  stripped, so `FMOD_System_Create` restores the original lines (251, 276) with `#line` directives at the two
  tagged statements. This is the first use of `#line` in the project: PROVISIONAL pending a user ruling (the
  alternative is blank filler lines, which reads worse).
- **Names.** The C entry points and the public C++ methods they forward to are named from the public FMOD Ex API by
  signature and use: the event-system callback 0x80391FB4 gets a channel's user data and current sound, then a
  sync point and its info with a 0x100-byte name buffer and `FMOD_TIMEUNIT_PCM` (2); `Sound::getSyncPoint`'s body
  checks the open state for READY (0) or SETPOSITION (7). Internal names (`SystemI`, `LinkedListNode`, `MemPool`,
  `Global`, `setCallbacks`) follow FMOD's conventions and are invented; internal functions keep `fn_` names with
  mangled forms in symbols.txt. FMOD's global pointer stays `lbl_80529020` (its owning unit, in the `.sdata` gap
  before rtanim, is not split yet). Open item: the callback typedefs (with `FMOD_MEMORY_TYPE`) fix the mangled
  names of the default callbacks 0x803759F8-0x80375A00; the FMOD 4.x minor version is not established.
- **Layout fact.** MWCC places a class's vtable pointer where its first virtual function is declared among the
  data members: `SystemI` (vptr at 0x9AEC of 0x9AF0) declares its data members before `~SystemI`.

## RenderWare wall rulings (rw-walls-2)

Five of the six parked RenderWare walls closed and their units flipped (`p2altmdl`, `vtxdesc`, `bamatrix`,
`rpuvanim`, `bsplit`, each `GATE OK-PENDING` on its flip). Each fix is a source shape the evidence names; no lever or
permuter text is committed.

- **`_rxPacketDestroy` (p2altmdl): the `CLUSTERTERMINATE`/`CLUSTERZERO` macros.** Retail clears `flags` and `stride`
  with one `stw`; so does Gormiti's Debug build, so the source stores a word there. The RenderWare SDK header
  `p2altmdl.h` does it as `*(RwUInt32 *) &(cl)->flags = 0U;` in
  `CLUSTERZERO`, and Gormiti's line table gives the whole loop body one line (a macro). The macros are declared in our
  `include/rwsdk/pipe/p2/p2altmdl.h`. The pun at offset 0 is the SDK's own text, not an offset cast (R4); the rule
  checker reports nothing.
- **`_rwGCNVertexDescSetElementAttr` (vtxdesc): `(RwUInt32)frac << n`.** Each field value is written count, type,
  fraction (Gormiti's Debug build evaluates POS as `(cnt | fmt << 1) | frac << 4`), and the `unsigned char` fraction is
  widened to `RwUInt32` before its shift at every site. The widening moves the fraction's shift ahead of the register
  load in the POS and TEX1 cases (the residue was the colour of that shift and of the load).
- **`RwMatrixOptimize` (bamatrix): the identity error in two statements per axis.** Gormiti's Final line table gives
  `error_x = mx->x - 1.0f;` its own line before the sum (lines 841/842, 844/845, 847/848), and its DWARF puts
  `error_x`/`error_y`/`error_z` in the registers of those differences. Gormiti's out-of-line copy of the static
  `_rwMatrixIdentityError` matches the inlined body with that shape. Reusing the error variable for the difference is
  the original's (R30 not applied: the bytes and the DWARF decide).
- **`UVAnimDestructor`, `_rpUVAnimCreateConsistantMatrices` (rpuvanim): truth tests on `RwMatrix*`.** Comparing a
  pointer to the aligned `RwMatrix` typedef with NULL turned the IR optimiser off for both functions
  (`tools/c/tools/wdbg/specs/iro_gate.wdbg` named exactly these two among the unit's functions), and without it the
  loops index with an offset register instead of retail's pointer induction variables. `if (*matrix)` / `!*matrix`
  (the SDK's truth-test spelling, as in baclump) keeps the optimiser. The unit's `.sbss` is defined in reverse of its
  layout (`RpUVAnimMaterialGlobals`, `_rpUVAnimCustomDataFreeList`, `rpUVAnimModule`): MWCC lays uninitialised
  globals out in reverse definition order; the first flip attempt was red on exactly those three addresses.
- **`_rpSkinSplitDataStreamRead` (bsplit): `_rpSkinSplitDataCreate` calls `_rpSkinSplitDataDestroy`.** Gormiti's
  Debug build calls Destroy from Create (line 100), and the Final line table gives the free-and-clear one line. Written
  so, the compiler inlines Destroy into Create and Create into the stream read, keeping Create's out-of-line copy,
  as retail.
- **`RwCameraClone` (bacamera): stays parked, fake exact state rejected.** Retail colours the parameter `camera` r31
  and the new camera r30; the source gives the reverse (a local set from a call result is a late node, coloured
  first). Permuter record: one run (`build/tools/permute.exe rwsdk/src/bacamera RwCameraClone --run -j 8
  --stop-on-zero --keep-all`, about ten minutes) reached score 0 by reading the copied fields through
  `RwCamera** new_var = &camera`. Reduced by hand to a pointer to the parameter (`RwCamera** source = &camera;`, every
  field read through `*source`), exact on its own: taking the parameter's address gives it a late node. No RenderWare
  programmer writes that; not committed. A `const RwCamera*` copy of the parameter does not move it. Levers depth 2
  (74 candidates) and a flag and compiler sweep found nothing.

## Rulings of 2026-11-15

Nine rulings of the user, recorded in AGENTS.md; the evidence and the checker's side are here.

- **Rank of the sibling binaries (AGENTS.md, Reference sources, item 7).** Gormiti ranks directly after the DOL itself,
  for functions `sibling.exe` pairs by identical normalised bodies only, and only for function names, unit names and
  boundaries, and struct and class layouts. An unpaired function, or a same-named one of another size, is another
  revision: Gormiti is a reading aid there and the retail assembly decides. The Ghost Rider preview and the May 2006
  Origins build rank with item 5 (names and member names, never offsets); both are imported
  (`sibling.exe index|pair|types … --name <name>`), their cross-architecture pairing trusted only in its sequence tier.
- **`#line` for embedded line numbers (AGENTS.md, Policies, "Embedded line numbers", and the project profile row on
  source lines).** One `#line N` directly before the statement whose retail code carries line N; N from the retail
  constant, confirmed by the sibling's line table where the function pairs. Nothing between the directive and the
  statement; no other use; no blank-line padding. The checker does not police the placement (open item).
- **Locals against the sibling's DWARF (AGENTS.md, Policies).** Instances, as now noted at the code:

  | function | file | kind | note |
  |---|---|---|---|
  | `WorldSyncCamera` | `src/rwsdk/world/baworobj.c` | RULE-EXCEPTION R30 | `spSect` declared last (DWARF: earlier) for retail's register numbering |
  | `_rwFreeListFreeReal` | `src/rwsdk/src/plcore/bamemory.c` | RULE-EXCEPTION R30 | `heap` at function scope (DWARF: in the block) for retail's register colouring |
  | `RpHAnimHierarchyUpdateMatrices` | `src/rwsdk/plugin/hanim/rphanim.c` | evidence note | `nodeMatrix` a `RwMatrix` in the loop block; the DWARF has a `RwMatrix*` at function scope (another revision) |
  | `AsyncThread::getAsyncThread` | `src/fmod/fmod_async.cpp` | evidence note (already in form) | `current` local; Gormiti's copy is another revision |
  | `ChannelEmulated::update` | `src/fmod/fmod_channel_emulated.cpp` | evidence note | the mode local; the DWARF's block lists no variable (eliminated local) |
  | `EventGroupI::release` | `src/fmod/fmod_eventgroupi.cpp` | evidence note | the next-node local; Gormiti's code keeps the same register, so the DWARF dropped an eliminated local |
  | `EventImplSimple::duplicateEvent` | `src/fmod/fmod_eventimpl_simple.cpp` | evidence note | the new-sound local (r31) the DWARF does not list |
  | `_rpSkinGeometryNativeWrite` | `src/rwsdk/plugin/skin2/skinstream.c` | RULE-EXCEPTION R30 | `maxWeights` declared before `numVertices` and `size` (DWARF: last) for retail's register numbering |
  | `_rpSkinGeometryNativeSize` | `src/rwsdk/plugin/skin2/skinstream.c` | evidence note | the `flags` local (`geometry->flags`, read before the stores that follow, as retail does); the DWARF does not list it (eliminated local) |
  | `ChannelReal::updateSpeakerLevels` | `src/fmod/fmod_channel_real.cpp` | evidence note | the `system` (`mSystem`) and `levels` (`mParent->mLevels`) locals (names invented) the DWARF does not list; retail and Gormiti keep both in r31 and r30 across the `sqrt` call, so the DWARF dropped eliminated locals. Without `system` the function reaches 96.4% and without both 93.7% |
  | `VertexDataSetupOptimized` family (`fn_802E51CC`, `IndexDataSetupOptimized`, `VertexDataFastSetup`, `IndexDataFastSetup`, `fn_802E65D0` and the instanceworld twins) | `src/rwsdk/world/instancegeom.c`, `src/rwsdk/world/instanceworld.c` | evidence note | the unused `vtxFmt` local with its default-format call: Gormiti's Debug build lists it (`sibling.exe fn VertexDataSetupOptimized --build debug`); retail keeps the call, the Final DWARF drops the eliminated local |

  R30 is a [checked] rule: the two RULE-EXCEPTIONs do not change any function's status.
- **Debug-only assertions (AGENTS.md, "Release-compiled-out debug checks").** `RWASSERT` (`include/rwsdk/plcore/badebug.h`,
  a release no-op) is written only where Gormiti's Debug build shows the assertion, with a `Debug build:` comment.
  The Climax engine's debug warning (user's ruling, 2026-11-16) is `RWS_WARN( condition, text )` in `include/climax/Debug.h`, a release no-op;
  its name is invented, after the debug stream `RWS::cdbg()` that Gormiti's Debug build writes to (no macro name survives in a binary).
  Evidence: the Debug build's `cdbg` output `WARNING: cattributehandler.cpp(778): InstanceID alraedy exists.` in
  `RWS::CAttributePacket::CreateEntity`; Gormiti's Final build and retail have none of it. Used only at such a site, with
  `// Debug build: warning at this point (Gormiti Debug)`. Done: `CreateEntity` (`climax/cattributehandler`), whose local
  `existing` the warning reads; check_rules counts a read in a release no-op macro's arguments.
- **Assembly functions in C sources (AGENTS.md, Parking mechanism).** Such an object is its C file with `asm` functions;
  `.s` only for an assembly original (`__exception.s`). Done: `rwsdk/plugin/skin2/skinmatrixblend.c` and
  `rwsdk/plugin/skin2/skingcnasm.c` (Gormiti's compile units), each `asm` functions with `nofralloc`, all ORIGINAL-ASM.
  CodeWarrior inline-assembly forms that differ from binutils `as`: the GQR operand of `psq_l`/`psq_st`/`psq_lux`/`psq_stux`
  is the bare number (`0`, `6`), not `qr6`; local labels are plain identifiers (`vertexLoop:`), not `.L_...`; `register`
  parameters may be named in the instructions (`mtctr numUsedBones`) or the raw `rN` used; `rotlwi`, `clrlslwi`, `subi`,
  `ps_cmpo1 cr5, ...` and `ble cr5, label` are accepted as dtk prints them.
  Open: the six compiled functions at 0x802ED690-0x802EDBCC (`SkinOpen`'s pipeline create/destroy/attach and the second
  pipeline's three callbacks) are not assembly; they are most likely the Wii build of RpSkin2's `skingcng.c` (Gormiti's three pipeline functions
  sit between `skingcnasm.obj` and `skinmatrixblend.obj`), unit not yet created.
- **FMOD's compiler (docs/toolchain.md, FMOD row).** The 4.2 series: `Wii/1.0RC1` or `Wii/1.0a`, no unit separates them;
  `Wii/1.0a` chosen. `-ipa file` and the `_savegpr` remark stay provisional.
- **`volatile` (AGENTS.md, project profile, R17).** (a) A file-scope or global volatile reloaded at every use and
  written from interrupt or callback context, with `// volatile: <evidence>`: the checker already accepted every noted
  non-local volatile, so nothing changed there (`_RwDlTokenCurrent`, `_RwDlTokenLastSeen` in
  `src/rwsdk/src/driver/wii/dltoken.c` carry their notes). (b) The assert-only local: `check_rules` accepts a scalar
  declared without an initialiser, assigned once by a statement that does not read it, every other use inside the
  arguments of a macro with a release definition that expands to nothing, with
  `// volatile: read by a release-disabled assertion, <reference>`. Self-tests cover that shape and the nearest
  rejected ones (no note, another note, an initialiser, a read outside the assertion, a second store, no read, a macro
  that is not a no-op); a `static volatile` local still fires. `__DSP_boot_task` carries the note; the sdk-av-2
  decision that kept it MATCH-PENDING is superseded.
- **R18 and inline-assembly helpers (AGENTS.md, Code rules).** `check_rules` reports R18 on the function whose own body
  holds the assembly: an `asm` function, an `asm { }` block, or a helper defined in the same file and called once (the
  finding moves to the call, in the caller). A header-level `asm` inline is allowed when listed under
  "Header-level asm inlines (ORIGINAL-ASM)" (above) and is otherwise "helper not listed as ORIGINAL-ASM"; a source marker no longer
  excuses a header-level one. Every such helper in `include/` is listed there. Effects: `CScalingObject`'s and the
  other callers of `VectorPS.h` no longer carry a header finding; `CAmbientInterferencePoint::fn_800C2304` and
  `CDrawingTest::fn_80178A60` now carry R18 in their own name (their unit-local `NormalizeFacing`, `FacingDot` and
  `V3dScale` are called once: function-specific blocks); unk_8000A040's `V3dAdd` and `V3dDivide` (two call sites
  each, one caller) keep their findings on the helpers. Open: the several-site evidence of `V3dAdd`, `V3dSub`,
  `V3dScale` and `V3dDot` in `VectorPS.h` (fragments over retail).
- **Compound literal in `MusicSettings::MusicSettings` (AGENTS.md, Policies).** Accepted with its comment.
- **Dead-stripped C++ bodies (AGENTS.md, "Dead-stripped functions").** A function the sibling's link map lists as
  `UNUSED` in the same object, when the unit's emitted layout needs it, is kept as `// no copy in retail
  (dead-stripped)` with the evidence noted.

## RenderWare and FMOD small-unit rulings

Names, constants and bodies the code marks as invented, unconfirmed or dead-stripped in the twelve small units
(`rwsdk/plugin/{collis/colldata,dmorph/rpdmorph,skin2/skinstream}`, `rwsdk/src/pipe/p2/p2resort`,
`rwsdk/tool/fsyst/rtfsyst`, `rwsdk/world/nodeGameCubeWorldSectorAllInOne`, `fmod/fmod_{channelgroup,codec_raw,
codec_user,sample_revolution,string,thread}`). Parked functions are under "Parked (NonMatching), with reasons",
the R30 exception and the locals notes in the tables above. `rpdmorph` has nothing open.

- **Dead-stripped bodies kept (AGENTS.md, "Dead-stripped functions").**
  - `RtFileSystemSetDeviceName` (`rtfsyst.c`, marked `// no copy in retail (dead-stripped)`): retail inlines it into
    `_rtFSInit`, so the body is the source of that inlined code.
  - `RxNodeDefinitionGetGameCubeWorldSectorAllInOne` (`nodeGameCubeWorldSectorAllInOne.c`, marked `(dead-stripped or
    inlined)`): it owns the static node definition `nodeGameCubeWorldSectorAllInOneCSL`, which retail keeps.
  - `FMOD_tolower` (`fmod_string.cpp`, marked `(dead-stripped or inlined)`): `FMOD_stricmp` and `FMOD_strnicmp` inline
    it; Gormiti's map lists the out-of-line copy as `UNUSED` (0x1C bytes).
- **Weak `File::getSize` copies discarded at the link (`fmod_codec_raw.cpp`, `fmod_codec_user.cpp`).** The inline
  virtual is emitted weak in each object that calls it out of line; retail keeps the one copy of `fmod_systemi.obj`
  (0x8034F048), so the others are discarded. Nothing to add to the sources.
- **Invented names (R7), all marked at the code.**
  - `fmod_thread.cpp`: the static inlines `RegisterThreadSlot` and `UnregisterThreadSlot` (retail inlines the code
    into `Thread::callback`; the by-value id of the first, copied to a stack slot, is what the bytes show);
    `MemPool::MAX_THREADS` (32, `include/fmod/fmod_memory.h`).
  - `fmod_codec_user.cpp`: the static inline `getBlockAlign`. `include/fmod/fmod_codeci.h`: `CODEC_FLAG_USERLENGTH`.
    `include/fmod/fmod_linkedlist.h`: `SortedLinkedListNode::NODE_PRIORITY_NONE` (the constructor stores -1).
  - `include/fmod/fmod_soundi.h`: the `roundup` parameter of `SoundI::getBytesFromSamples` (default true: a count of
    compressed frames rounds up to the frame holding the last sample, a stream's read passes false and rounds down;
    XMA and MPEG report the count unchanged). `SOUNDI_FLAG_PRELOADED` and `SOUNDI_FLAG_PROGRAMMERSOUND`, named for the
    errors `release` returns when they are set.
  - `include/fmod/fmod_channelgroupi.h`: the second `bool` of `ChannelGroupI::setPaused` and `setMute` is
    `storeState`. Gormiti's copy takes one `bool`, so retail's is another revision; the group keeps the new state
    itself when the flag is set, and calls its child groups with false.
  - `rwsdk/plugin/collis/colldata.h`: `rwID_COLLTREE` (value `MAKECHUNKID(rwVENDORID_CORE, 0x2C)` from retail; the RW
    3.7 name is unconfirmed).
- **Shared sample-count helper (closed).** The sample-count to byte-length conversion is `SoundI::getBytesFromSamples`
  (`include/fmod/fmod_soundi.h`, a static form taking channels and format and an instance form), used by
  `fmod_sample_revolution`, `fmod_sample_software`, `fmod_sound_sample`, `fmod_sound_stream` and `fmod_codec_dsp`;
  the unit-local `GetLockBytes` copies are gone. The `getPosition` functions of `ChannelReal`, `ChannelSoftware` and
  `ChannelStream` are not reconstructed yet.
- **Unconfirmed names (open).**
  - `ChannelGroupI::setPitch`, `overrideSpeakerMix` and `addGroup` (0x80335590, 0x80335C54, 0x80335EB4) are named from
    the public `ChannelGroup` wrappers that call them (0x80334DE0, 0x80334F88, 0x80335010); unconfirmed until their
    bodies are reconstructed.
  - `rtfsyst.c`: the allocation hint `rwMEMHINTDUR_GLOBAL | 0x1BE` in `RtFileSystemSetDeviceName` is the module id
    of the allocation; the name of the 0x1BE constant is unknown.
- **Identity TODO (`skinstream.c`, `_rpSkinGeometryNativeSize`).** Retail clears the word at 0x10 of the block that
  `((RwResEntry**)vbHeader->attr[0].array)[-1]` points to, written `->ownerRef = NULL` after an `RwResEntry` field at
  that offset. Nothing in the unit or its neighbours says what the instancer stores before the array; marked
  `identity-TODO` at the code.

## Units from the other tool's branch (baimage, p2renderstate, p2define, colldata, rtfsyst, fmod sample and stream)

Seven units are `Matching` (`fmod/fmod_sample_software` 8/8, `fmod/fmod_sound_sample` 14/14, `fmod/fmod_sound_stream`
7/7, `rwsdk/src/baimage` 21/21, `rwsdk/src/pipe/p2/p2renderstate` 1/1, `rwsdk/plugin/collis/colldata` 9/9,
`rwsdk/tool/fsyst/rtfsyst` 7/7); `rwsdk/src/pipe/p2/p2define` is `NonMatching`. The two former walls are closed:
`_rtGetFileNameFromDeviceName` (`fPath = fileName` and indexing) and `RpCollisionPluginAttach` (the two static
helpers `CollisionGeometryPluginAttach` and `CollisionWorldSectorPluginAttach`, which Gormiti's Debug map and DWARF
list as statics with the locals `result` and `offset`; the `= 0` on the five globals is what the `.sbss` order needs).

- **`rwsdk/src/pipe/p2/p2define` stays `NonMatching`: 12 of 13 functions exact.** The one open function,
  `RxLockedPipeUnlock`, is under "Parked (NonMatching), with reasons".
- **Inlined statics kept, marked `// no copy in retail (inlined)`.** Names from Gormiti's Debug map (absent from, or
  `UNUSED` in, the Final map): `baimage.c` `ImageGetScratchMem`, `_rwImageGammaCorrectArrayOfRGBA`,
  `ImageStraightCopy`; `p2define.c` `CalcNodesOutputsCompactedMemSize`, `CalcUnlockPersistentMemSize`,
  `PipelineNode2Index`, `RxPipelineNodeFindOutputByIndex`, `RxPipelineNodeFindInput`, `RxLockedPipeAddPath`;
  `colldata.c` `CollisionGeometryPluginAttach`, `CollisionWorldSectorPluginAttach`.
- **Retail quirk kept (R36).** `ImagePathForAllFullNames` (`fn_80295B94` in `baimage.c`) calls `rwstrchr(pathElement, ';')`
  twice, in the declaration's initialiser and again in the `if`; one evaluation gives 97.06%.
- **Invented names (R7).** The `roundup` parameter of `SoundI::getBytesFromSamples` (see
  "Invented names" under "RenderWare and FMOD small-unit rulings"), `rxPipelineGlobals::defaultRenderState`
  (the 0x2C bytes at 0x4 are a `RxRenderStateVector`, whose layout is Gormiti's), `FMOD_LOADSECONDARYRAM` and
  `FMOD_MEMORY_SECONDARY` placed in `include/fmod/fmod_sample_software.h` (public FMOD flags; they belong in `fmod.h`).
- **Error-code names (`baerr.h`).** `E_RW_INVIMAGEFORMAT`, `E_RW_PLUGINNOTINIT` and the `E_RX_*` names are entries of
  Gormiti's Debug error table. The `E_RX_*` values are retail's; each name is the table entry at the value's position
  in the table's order (28 `CYCLICPIPELINE`, 34 `FRAGMENTEDPIPELINE`, 36 `INVALIDENTRYPOINT`, 40
  `NODETOOMANYCLUSTERSOFINTEREST`, 41 `NODETOOMANYOUTPUTS`, 42 `PIPELINETOOMANYNODES`, 52 `UNLOCKEDPIPE`), not read
  from a value table: unconfirmed until the table's numbering is read.
- **Placeholder function names kept (not renamed).** `sibling.exe of` finds no counterpart for `RwImageSetPath`
  (Gormiti `RwImageSetPath`), `fn_80295B94` (`ImagePathForAllFullNames`), `fn_802966AC` (`ImageConvertDepth`) and
  `fn_8029C13C` (`PipelineTopSort`), and only a seq-size pair for `fn_8029BEF0` (`_NodeCreate`), so they are not
  renamed; size and order in `baimage.obj` and `p2define.obj` agree with the names given in the comments at the code.
- **Open declarations.** `fn_802A75A0` (a path test) is declared in `baimage.h` because its unit is unidentified;
  `RwFexist` is in `bafsys.h`, whose range (0x802F481C-0x802F4D24) holds it. `p2define.c` keeps `field_0x38` of the
  pipeline globals (the maximum node count; name open) and `lbl_8052B1B8` (Gormiti `P2MemoryLimits`, name of the
  variable unknown).

## Wall-pass round rulings (p2heap, p2resort, native, rtskinsp, rescache, dlmalloc, fmod_dsp, game headers)

Parked functions of this round are under "Parked (NonMatching), with reasons". `game/CActorTest` is `Matching`,
13 of 13 functions exact.

- **`fmod/dlmalloc` is public source (`src/fmod/dlmalloc.cpp`).** The text is Doug Lea's dlmalloc 2.8.3
  (malloc-2.8.3.c, public domain), compiled as C++ inside `namespace FMOD`; the file carries
  `// Reference: Doug Lea's malloc (dlmalloc) 2.8.3, public domain`. FMOD's changes are listed in the file header
  (`ONLY_MSPACES`, no mmap or `MORECORE`, no locks, `mparams` a pointer set by `init_mparams`, the `sys_alloc` body
  and the stats and create/destroy routines removed). The routines nothing calls (`mspace_calloc`, `mspace_memalign`, ...) are kept as in the public text:
  Gormiti's map lists them `UNUSED`, they are the 13 functions the unit builds that retail does not contain (9 of 9
  retail functions exact). They are not marked individually with `// no copy in retail (dead-stripped)`; the file
  header states it for all of them. The unit's name and boundary are Gormiti's link map's (`fmodex.a dlmalloc.obj`).
- **`rwsdk/src/pipe/p2/p2heap.c` and `p2heap.h`.** The inlined statics `HeapFreeBlocksDeleteEntry`,
  `HeapSuperBlockCreate` and `HeapSuperBlockDestroy` are restored from Gormiti's Debug map (absent from the Final map)
  and marked `// no copy in retail (inlined)`. `RxHeapReset` is the macro `(!((heap)->dirty) ? TRUE : _rxHeapReset(heap))`
  of `p2heap.h` (the Debug build's `RxHeapCreate` tests `dirty` around the call), which is where retail's dead
  branch comes from. `RWERROR((E_RW_NOMEM, size))` in `HeapFreeBlocksNewEntry` passes the size as its second
  argument. Invented: `rxHEAPGROWBLOCKS` (0x20, the growth step of the free-block array). The header's layouts are
  Gormiti's DWARF (`rxHeapBlockHeader::pad` is its member). The unit was split from `p2renderstate` at 0x8029E9F0.
- **`rwsdk/src/pipe/p2/p2resort.c`.** `RX_SWAP_ELEMENTS` is an invented macro name (Gormiti's line table puts the
  exchange on one line at every use); its locals are declared in the order the DWARF implies. `_insertionsort` steps
  each pointer in its loop's test as a comma expression (the Debug build stores each test's value to a stack slot and
  reloads it).
- **`rwsdk/world/native.c`.** `_rpNativeSize` is restored as a static (a function of Gormiti's Debug map, absent from
  the Final map), marked `// no copy in retail (inlined)`; its `RWASSERT`s carry the Debug-build assertions and
  compile to nothing. `include/rwsdk/plcore/rpdbgerr.h` gains the release no-op `RWASSERT`, which
  `include/rwsdk/plcore/badebug.h` defines in the same guarded form: the macro has two definitions. Open (R1 shape):
  one shared definition in `badebug.h`.
- **`rwsdk/tool/skinsplit/rtskinsp.c`.** `_rtSkinSplitMeshBoneRLEValidate` is rebuilt from Gormiti's Debug build,
  names, locals and statement order from the DWARF, with its checks written as `RWASSERT`s. In a release build only
  the return of `matTriList` remains, and the function stays out of line in retail. The Debug build's per-function
  name probe (its line 103) is not written. `_rtSkinSplitGeomResetTriMat` is marked `// no copy in retail (inlined)`.
- **`rwsdk/plugin/skin2/rescache.c`.** `_clSkinCache_UnRegisterSkinRes` is another revision than Gormiti's (Gormiti's
  copy is 0x18 bytes shorter), so retail's bytes decide its body; it carries an `identity-TODO`: retail clears the
  word at 0xC of the vertex buffer header, written `VBOwner->attr[0].array = NULL`, and what the word is, is open.
  `_RwSysFrameID` (`.sbss` 0x8052B234, declared in `include/rwsdk/driver/wii/dldevice.h`) is named on moderate
  evidence only: the cache stamps its entries' `lastFrameID` with it and compares it, and its writer is not
  reconstructed.
- **`fmod/fmod_dsp.cpp`, `include/fmod/fmod_dspi.h`.** `DSP::setActive` and `DSP::setBypass` call the inline setters
  `DSPI::setActive` and `DSPI::setBypass` (names invented after the public setters; retail has no out-of-line copy),
  which set `mFlags` bits 0x2 and 0x4 as bare numbers: the bits have no names, and their meanings (active, bypass)
  follow the setter names.
- **`include/climax/AttributeHandler.h`.** `CAttributeHandler::GetZone() const` is Gormiti's DWARF name
  (`RWS::CAttributeHandler::GetZone()`); the DWARF calls the member it returns `mZone`, but the field keeps its
  placeholder `field_0xC`. Used by `CScalingObject` and `CAmbientInterferencePoint` in comparisons of the form
  `GetZone() == other->GetZone()`.
- **Event-id conversion operator (`include/climax/unk_801A92A8.h`).** The message id class (Gormiti's `CEventId`; the
  class keeps its placeholder name here) has `operator unk_EventRecord*()` and `operator*()`, both Gormiti DWARF
  members. The game message handlers compare through the conversion (`msg.record == lbl_X`), which gives retail's
  operand order in `CCollisionCapsule::fn_8009CB1C` and `CActorTest`; `CAmbientInterferencePoint` and others still
  write `.record == lbl.record`.
- **`game/CPointOfInterest` symbols edited by hand.** `fn_800C5280`, `fn_800C5320` and `fn_800C53F4` in
  `config/R5WEA4/symbols.txt` changed their mangled parameter lists (`fn_800C5280` lost a first `CPointOfInterest*` parameter; `unk_Vec3` became
  `unk_RwV3d` in the other two);
  `names.exe rename` refuses signature changes, so the three names were edited in `symbols.txt` and the source together.
  The unit was widened to 0x800C4F80 to include the constructor and destructor that own the vtable.
- **`force_active` registration objects (`config/R5WEA4/config.yml`).** `lbl_80529328` (`CActorTest`) and
  `lbl_8052980C` (`CScalingObject`) join the list of class registration objects: each unit is reached only through
  its `.ctors` entry, so without the entry the linker strips it.

## Branch integrate/antigravity review (NdevExi2AD, six FMOD units, RpPVS)

Nine units are `Matching` and exact: `revolution/NdevExi2AD/DebuggerDriver` 21/21, `revolution/NdevExi2AD/exi2` 10/10,
`fmod/fmod_dsp_codec` 1/1, `fmod/fmod_dsp_filter` 5/5, `fmod/fmod_codec` 8/8, `fmod/fmod_output` 5/5,
`fmod/fmod_sound` 18/18, `fmod/fmod_system` 39/39 and `rwsdk/plugin/pvs/rppvsaux` 4/4. `rwsdk/plugin/pvs/rppvs` is
`Matching` as well, 19 of 19 functions exact.

- **`rwsdk/plugin/pvs/rppvs` closed walls.** `PVSCopyAndTagSectors` keeps two quirks of the Debug build: `v` is computed
  (`RwV3dSubMacro`) and never read, and the diagonal shift is worked out again from the box. `RpPVSSetViewPosition`
  keeps the search under `if ( lbl_80517780.InSector == -1 )`, a test the compiler folds because `InSector` was just
  set to -1, and reads `PVSCache` only in the closing assertion. The two sector-render counters are defined in
  reverse of their address order, because uninitialised small data is laid out in reverse order of definition.
- **`rppvs.c` bodies with no copy in retail (inlined), marked at the code.** `PVSFindDiagonalSizeOfBBox`,
  `PVSInitializeSectors`, `PVSDeinitialize` and `RpPVSWorldSectorVisible` (names from Gormiti's DWARF; each is inlined
  into its callers in retail). `RpPVSWorldSectorVisible` is non-static in the header, so the unit builds one extra
  function.
- **`rppvs.c` names left as placeholders (R7).** The plugin's global state is `lbl_80517780` (`RpPVSGlobalVars`; the
  layout and member names are Gormiti's DWARF; its owning unit's variable name is open). `lbl_8052B348` and
  `lbl_8052B34C` are two sector-render counters that retail increments in `PVSWorldSectorRenderCallBack` and Gormiti's
  copy does not have; their names and purpose beyond "accepted" and "rejected" sector counts are open.
  `rwID_PVSPLUGIN` (0x2A, `include/rwsdk/rpcriter.h`) is the value of retail's plugin registrations.
- **`rwsdk/plugin/pvs/rppvsaux` and `rppvs` unit names** are Gormiti's link map (`RpPvs.a rppvsaux.obj`, `rppvs.obj`).
- **FMOD names from the public API, no counterpart in Gormiti.** `Sound::getMusicNumChannels` and
  `Sound::setMusicChannelVolume` (0x8037AB84, 0x8037ABF4): Gormiti's link map has no copy; the names are the FMOD Ex
  public API's, and the bodies follow the pattern of the neighbouring `Sound` methods (validate the handle, require
  READY or SETPOSITION, forward to the `SoundI` virtual of the same name).
- **The seven `FMOD::SystemI` renames** (`release`, `getNumPlugins`, `getPluginInfo`, `init`, `createSound`,
  `playSound`, `setReverbProperties`) are the mangled names of Gormiti's `fmodex.a fmod_systemi.obj` functions of the
  same size. The first four are another revision than Gormiti's (no identical-body pair); `createSound`, `playSound`
  and `setReverbProperties` pair by order and size.
- **`DSPFilter::execute` local `historybuffer`.** Gormiti's copy of this function is another revision (0x6DC bytes
  against 0x6D4) and its DWARF lists no such local; retail holds `mHistoryBuffer` in a callee-saved register across the
  `memcpy` calls, which a local of its own gives (locals ruling of 2026-11-15; the function's header comment says so).
- **Unnamed flags and constants.** `DSPI_FLAG_0x10` and `DSPI_FLAG_0x100` (`include/fmod/fmod_dspi.h`) are bits of
  `DSPI::mFlags` whose meaning is open; `DSPI_FLAG_ACTIVE` (0x2) and `DSPI_FLAG_BYPASS` (0x4) are named after
  `DSP::setActive` and `DSP::setBypass` (the `fmod_dsp.cpp` entry above, which still writes them as bare numbers).
  `FMOD_MEMORY_XBOX360_PHYSICAL` (0x00100000) is the public FMOD Ex memory flag, placed in `fmod.h`.
- **`Plugin::Plugin()`** (`include/fmod/fmod_dspi.h`) is an inline constructor that sets `mGlobal` from
  `lbl_80529020`, FMOD's global pointer (see "FMOD rulings (fmod-1)"); `DSPCodec::release` and `Output::mix` read the
  same pointer, so they keep the `lbl_80529020` name.
- **`NdevExi2AD`.** Names, unit name and layout are doldecomp/ogws's (`// Reference:` at the top of each file). The
  `ODEMU*` helpers and the four mailbox and RAM helpers sit after their callers, in retail's order, and are
  declared `static` ahead of use (`ODEMU.h` holds the declarations).

## FMOD, RenderWare and Climax units of the 2026-11-16 lanes

Parked functions of these units are under "Parked (NonMatching), with reasons". `fmod/fmod_channel`,
`fmod/fmod_reverbi`, `fmod/fmod_music`, `rwsdk/plugin/matfx/multiTex`, `rwsdk/plugin/dmorph/dmphgeom`,
`rwsdk/plugin/pvs/rppvs`, `climax/TevCBMgr`, `climax/scalekey` and `climax/GeomTypes` are `Matching`.

- **Names from Gormiti's sequence-and-size tier only (`fmod/fmod_channel`).** `Channel::setSpeakerMix` (0x803790C0) pairs
  with the 9th function of Gormiti's `fmod_channel.obj` by order and size (0x8C), with no identical-body pair. The
  `ChannelI` symbols `setVolume(float, bool)` (0x80339588), `setSpeakerLevels` (0x80339EAC), `setPosition` (0x8033B59C)
  and `setMode` (0x8033BF68) were renamed in `symbols.txt` by the same tier and are unconfirmed. The trailing `bool`
  of `ChannelI::setVolume`, `setPan`, `setSpeakerMix` and `setSpeakerLevels` is named `flag` (name invented; retail's
  call sites pass false from `Channel::setVolume` and true from the other three).
- **`fmod/fmod_reverbi`.** `FMOD_REVERB_MAXINSTANCES` (4, `include/fmod/fmod_reverbi.h`) is invented, after the public
  API's instance numbers 0 to 3. `PluginFactory::createDSP( FMOD_DSP_DESCRIPTION_EX*, DSPI** )` (0x8033F704, 0x3DC bytes)
  is named from another revision: the same-named function in Gormiti's `fmod_pluginfactory.obj` is 0x3F4 bytes and has no
  identical-body pair. `ReverbI::releaseDSP` is `inline`, no copy in retail (inlined into `release`; Gormiti's map lists
  it `UNUSED`).
- **`fmod/fmod_music`.** The `*Internal` members of `MusicSong` (`getLengthInternal`, `getPositionInternal`,
  `getMusicNumChannelsInternal`, `setMusicChannelVolumeInternal`, `getMusicChannelVolumeInternal`),
  `getHardwareMusicChannel` and `MusicVirtualChannel::cleanUp` are inline, no copy in retail, each marked at the code.
  `MusicSong::playSound` is another revision than Gormiti's (same size, other code), so retail's bytes decide its body.
  `gDummyChannel__4FMOD` (`.bss` 0x80523CB0, size 0x2B8) is a symbol line added to `config/R5WEA4/symbols.txt` by hand:
  `names.exe` has no verb that adds a symbol.
- **`rwsdk/plugin/matfx/multiTex`.** `RpMultiTextureSetEffect`, `RpMultiTextureSetTexture`, `RpMultiTextureSetCoords`
  and `RpMultiTextureGetCoords` are `// no copy in retail (inlined)`. `RpMultiTextureGetEffect` takes
  `const RpMultiTexture*` (Gormiti's DWARF records no `const`): that is what makes the inlined `SetEffect` reload the
  effect it has just stored, as retail does. `_rpMTEffectOpen`, `_rpMTEffectClose`, `RpMTEffectFind` and
  `RpMTEffectAddRef` (symbols of the unsplit `multiTexEffect` range) are named from Gormiti's same-named functions of
  `multiTexEffect.obj` (same size for three, 0x1B0 against 0x1C0 for `RpMTEffectFind`) and their callers, with no
  identical-body pair. The header names `multitex.h` and `multitexgcn.h` are invented.
- **`rwsdk/plugin/dmorph/dmphgeom`.** `RpDMorphGeometryDestroyDMorphTargets` is `inline`, no copy in retail (inlined into
  `DMorphGeometryDtor`; a function of Gormiti's Debug build). `DMorphGeometryStreamWrite` passes each write's byte
  count through one `size` local, as the Debug build does.
- **`rwsdk/plugin/pvs/rppvs`.** See the closed walls under "Branch integrate/antigravity review".
- **`climax/RwPlugin`.** `RenderCallBack` (the render callback of the first atomic read) is an invented name.
  `fn_801EDA30` and `fn_801EDABC` (`include/climax/ParticleX.h`) are placeholders for the functions of the
  unreconstructed RwInterface unit, and `lbl_80528858` (`include/climax/RwInterface.h`, the offset of the plugin's data
  in an atomic) is declared there until that unit is written.
- **`climax/TevCBMgr`.** Retail has one handle list and one count (`sHandleList[10]`, `sHandleCount`); Gormiti's DWARF
  lists each twice (a second, static pair), which this unit does not follow.
- **`climax/scalekey`.** `HAnimFullKeyFrameToMatrix` and `HAnimFullKeyFrameBlend` are another revision than Gormiti's
  (retail's `ToMatrix` converts the rotation once and multiplies it by the scale matrix); both are static, and their
  identity rests on `Register` storing them as the scheme's callbacks and `Interpolate` calling `Blend`. `HANIMFULLKEYFRAME_TYPE_ID` is an
  invented name.

- **game/unk_AnimController.cpp (unit name unconfirmed).** The table at unk_8001777C field_0x148 (`unk_AnimController`) and its entry (`unk_803E0A48`, vtable `__vt__12unk_803E0A48`) have no string or class-name evidence; both names are invented. Open: fn_8001A100 6
- **game/unk_AnimController.cpp (unit name unconfirmed).** The table at unk_8001777C field_0x148 (`unk_AnimController`) and its entry (`unk_803E0A48`, vtable `__vt__12unk_803E0A48`) have no string or class-name evidence; both names are invented. Open: fn_8001A100 6 percent, fn_8001A428 76 percent, fn_8001A520 94 percent; the unit stays NonMatching.


## Game unit rulings (unk_80074138)

- **`game/unk_80074138.cpp` (unit name unconfirmed):** the file name is the constructor address (no class-name
  evidence). It holds the constructor `__ct__12unk_80074138Fii` (0x80074138) and the destructor
  `__dt__12unk_80074138Fv` (0x800744B0) of the placeholder class `unk_80074138` (names invented), and two instances of the
  shared `unk_List<T>` destructor (0x80074300 for `unk_List<unk_PendingItem*>`, 0x800743D8 for `unk_List<unk_RpClump*>`),
  which match exactly. `RpClumpGetWorld` and `RpWorldRemoveClump` are declared in the unit header with the C linkage of
  RenderWare (0x802DB370, 0x802DB1FC, not yet reconstructed). `RWS::AssetTracker::CAssetList` is declared with
  `DecRefCount` only (`climax/AttributeHandler.h`). Open: the constructor and the destructor are not yet exact (register
  and control-flow walls, see the function comments); retail's first-list vtable lbl_803E65C0 is 0x10 bytes while the
  template instance's is 0xC, so that list's second virtual slot is not yet explained. The unit stays NonMatching.

## Game unit rulings (unk_800A8100)

- **`game/unk_800A8100.cpp` (unit name unconfirmed):** the file and the class `unk_800A8100` are named after the constructor fn_800A8100 (`__ct__12unk_800A8100FPCcP11unk_RpClumpP11unk_RpWorldPv`), which matches exactly. Its vtable is `__vt__12unk_800A8100` (the symbol was lbl_803E9B40 in the data unit auto_07_803E9620_data); the constructor stores the table start, and slot 0 is fn_800A8640 (`fn_800A8640__12unk_800A8100Fv`, a bare return, name unknown). The table word at +0xC is zero and is not declared as a slot. The class destructor at 0x800A8208 is outside this unit. The unit boundary is the unit plan at confidence 0.16 with no sibling object: open item for a class-name or string evidence.
