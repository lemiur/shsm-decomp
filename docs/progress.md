# Progress ledger

The score baseline of the merged state. Regenerate it at every merge from the gate's own output (the CHECK line
`build/R5WEA4/main.dol: OK` and `build/R5WEA4/report.json`); never edit the numbers by hand. Exceptions are
listed in [exceptions.md](exceptions.md). The saved objdiff report this ledger was taken from is
`config/R5WEA4/baseline_report.json` (see AGENTS.md, "The byte gate").

## Numbers

Counts cover functions linked from source (units marked `Matching` in `configure.py`). Functions that score
100% inside a `NonMatching` unit are not linked and are not counted.

<!-- ledger:begin numbers -->
| measure | functions | code bytes |
|---|---|---|
| **matched under the rules** (headline) | 5284 / 13922 | 1299940 / 3974484 |
| bytes matched (linked from source, excluding `ORIGINAL-ASM`) | 5289 / 13922 | 1301960 / 3974484 |
| `ORIGINAL-ASM` (linked, listed apart, outside both counts and totals) | 116 | 28536 |
| linked from source in total (objdiff "complete") | 5405 | 1330496 |
| `Matching` units | 589 / 1377 | |
<!-- ledger:end numbers -->

Totals exclude the `ORIGINAL-ASM` functions (<!-- ledger:begin gross -->14038 functions and 4003020 code bytes<!-- ledger:end gross --> in all, counting the dtk
alignment-padding blocks left between source splits: `pad_03_8021F8F8_text` and `pad_03_80224704_text` (20 bytes, between
the dvdfs, dvd and dvdqueue splits), `pad_03_802253A8_text` (8 bytes, after dvderror) and the twelve blocks between the
ipc, nand, sc, esp and arc splits, `pad_03_80241C68_text` (after ipcMain) to `pad_03_8024A788_text` (after arc), 84
bytes), and the seven blocks after the pad, ai and ax splits (`pad_03_8022CA5C_text` to `pad_03_8022F1C8_text`, 56 bytes), and the eight blocks after the i2c, vi3in1, KPAD, WPAD, WPADHIDParser, WPADMem, WUD and WUDHidHost splits (`pad_03_8022B2A4_text`, `pad_03_8022C9FC_text`, `pad_03_8027C098_text`, `pad_03_80284FD8_text`, `pad_03_802892A4_text`, `pad_03_8028A46C_text`, `pad_03_802908B4_text`, `pad_03_80290EA8_text`; 68 bytes), and the five blocks after the THPDec, THPAudio, AXSPB, AXVPB and dsp splits (`pad_03_8020D20C_text`, `pad_03_8020D778_text`, `pad_03_8022F60C_text`, `pad_03_80230988_text`, `pad_03_80236EAC_text`; 28 bytes), and the seven blocks after the AXFX splits (`pad_03_80230A74_text`, `pad_03_80230B64_text`, `pad_03_80231994_text`, `pad_03_80232884_text`, `pad_03_80232924_text`, `pad_03_8023346C_text`, `pad_03_802334B4_text`; 76 bytes), and the two blocks after the MIX splits (`pad_03_80236714_text`, `pad_03_80236C08_text`; 20 bytes). objdiff's own "matched" figure (1698 functions, 390420 bytes) also includes the 100% functions of the parked units, among them
the five assembly functions of the parked `revolution/os/OS` (`__OSFPRInit`, `__OSDBIntegrator`, `__OSDBJump`,
`OSExceptionVector`, `OSDefaultExceptionHandler`), which are not counted anywhere: they become `ORIGINAL-ASM` when
the unit is linked.
The parked `revolution/os/OSExec` likewise holds the assembly function `Run` (ORIGINAL-ASM when the unit is linked; not
counted anywhere).
The parked MetroTRK `targimpl` holds eight assembly functions (`__TRK_get_MSR`, `__TRK_set_MSR`, `TRKInterruptHandler`,
`TRKExceptionHandler`, `TRKSwapAndGo`, `TRKInterruptHandlerEnableInterrupts`, `ReadFPSCR`, `WriteFPSCR`; 928 bytes),
listed in exceptions.md and counted nowhere until the unit links.
The headline leaves out the four `MATCH-PENDING` functions of `revolution/os/OSPlayRecord` (1792 bytes; linked, bytes
identical, R17 steering: see exceptions.md); "bytes matched" includes them.
The parked `revolution/dsp/dsp_task` (5 functions at 100%, among them `__DSP_boot_task`, MATCH-PENDING: R17 volatile
local outside the user's delay-loop ruling) is not linked and not counted. `revolution/vi/vi` (26) and
`revolution/wud/debug_msg` (1) are linked and counted since the user's R17 ruling of 2026-10-18 (exceptions.md,
"sdk-av-2 rulings"). The five THP ORIGINAL-ASM functions (`__THPInverseDCTNoYPos`, `__THPInverseDCTY8`,
`__THPHuffDecodeDCTCompY`/`U`/`V`; 7364 bytes) are in the ORIGINAL-ASM row.
`PPCMthid4` is C with two listed `asm` statements and is counted (see exceptions.md).
The 72 `_savegpr_N`-style entry labels inside `runtime.c` are labels, not functions, and are not counted;
nor are the `entry` labels `__RAS_OSDisableInterrupts_begin`/`_end` and `__OSSystemCallVectorStart`/`End`.
Functions kept in source without an out-of-line copy in retail (inlined or dead-stripped: the OSCache L2 functions, the
OSThread and SIBios lists in exceptions.md, EXIBios `CompleteTransfer`, `EXIClearInterrupts`, `EXIProbeReset`,
`EXIProbe`, `__EXIAttach`, and the OSFont, OSReset and OSPlayRecord lists in exceptions.md, "OS unit rulings
(sdk-os-5)", and the dvdfs and dvdqueue
functions without a retail address: DVDGetCurrentDir, DVDConvertEntrynumToPath, isSame, myStrncpy and
PopWaitingQueuePrio inlined, DVDChangeDir, DVDReadPrio, DVDSeekPrio, the directory functions,
`__DVDIsBlockInWaitingQueue` and `DVDDumpWaitingQueue` dead-stripped; MetroTRK msgbuf's `TRKAppendBuffer1_ui8`,
`TRKReadBuffer1_ui8` and `TRKReadBuffer1_ui32`, inlined; libjpeg jdinput's `latch_quant_tables`, inlined into
`start_input_pass`; the GX functions kept from the references without a retail copy, such as `IsWriteGatherBufferEmpty`,
`GXProject`, `GXSetVtxDescv`, `GXTexModeSync`, and the inlined `EnableWriteGatherPipe`) have no address and are not counted.
Likewise the inlined static helpers of the sdk-nand units: mem_expHeap's block accessors (`GetMemPtrForMBlock_`,
`GetMBlockEndAddr_`, `InitMBlock_`, `InitFreeMBlock_` and the rest), mem_heapCommon's `FindListContainHeap_`, fs's
`_FSGetStatsCb` family (inlined into `_isfsFuncCb`), ipcclt's `ipcAllocReq`, `ipcFree`, `__ipcQueueRequest`,
`__ipcSendRequest`, `IpcAckHandler` and the `__ios_Ipc1`/`__ios_Open`/`__ios_Read`/`__ios_Write`/`__ios_Seek`/`__ios_Ioctl`
request builders, ipc memory's `__iosCoalesceChunk`, ipcProfile's `AddReqInfo`/`DelReqInfo`, and arc's `isSame`,
`myStrncpy`, `ARCConvertEntrynumToPath` and `ARCGetCurrentDir`.
Likewise the sdk-nand-2 units' functions without a retail copy: euart's `QueueLength`; nand's `nandCreate`,
`nandDelete`, `nandSeek`, `nandCreateDir` and `nandInspectPermission`; NANDOpenClose's `nandGetUniqueNumber` and
`nandCopy` (inlined) and `NANDSimpleSafeOpen`/`nandSafeOpen` (dead-stripped, kept because the `.data` string order
needs their strings); NANDCore's `nandIsRelativePath` and `nandReportErrorCode`; NANDCheck's `nandCalcUserUsage`,
`nandCheck` and `nandFree`; NANDLogging's `callbackRoutine` and `prepareLine`; and scsystem's `SetBgJobStatus`,
`__SCGetConfBuf`, `__SCGetConfBufSize`, `__SCIsDirty`, `__SCSetDirtyFlag`, `__SCClearDirtyFlag`, `ClearConfBuf`,
`ParseGetBEValue`, `FindItemByID`, `SCFindIntegerItem`, `SCReplaceIntegerItem`, `FinishFromFlush`, `ErrorFromFlush`
and `ErrorFromReload` (all inlined).
Likewise the sdk-av units' functions without a retail copy: ai's `AISetDSPSampleRate` and `AIGetDSPSampleRate` (inlined into
`AIInit`), `AIGetDMAEnableFlag` and `AIReset` (dead-stripped); AX's `AXInitEx` (inlined into `AXInit`); AXAlloc's
`__AXInitVoiceStacks`, `__AXPopFreeStack`, `__AXPopCallbackStack`, `__AXPushStackHead` and `__AXPopStackFromBottom` (inlined)
and `AXSetVoicePriority` (dead-stripped); AXAux's `AXGetAuxBCallback` and `AXGetAuxCCallback` (dead-stripped); AXCL's
`__AXWriteToCommandList` (inlined) and `AXSetMasterVolume` (dead-stripped); and AXOut's `AXRmtGetSamplesLeft` (inlined).
Likewise the sdk-vi-wpad units' functions without a retail copy: vi3in1's inlined or dead-stripped DAC helpers (the
functions whose banners say "inlined into" in `vi3in1.c`, among them `__VISetCGMSClear`) and the dead-stripped `VISetCGMS`
(kept for its `.data` string; its `OSPanic` line is an unknown placeholder).
Likewise the sdk-av-2 units' functions without a retail copy: AXVPB's `__AXDumpVPB` and `__AXGetSrcCycles`, dsp's
`DSPAssertInt`, dsp_task's `__DSP_add_task`, THPDec's `__THPSetupBuffers`, `__THPHuffGenerateSizeTable`,
`__THPHuffGenerateCodeTable`, `__THPHuffGenerateDecoderTables`, `__THPRestartDefinition` and the listed helpers
`__THPGQRSetup`, `__THPGQRRestore` and `__THPHuffDecodeTab`, and THPAudio's `__THPAudioGetNewSample` and
`__THPAudioInitialize` (inlined or dead-stripped).
Likewise the sdk-axfx units' dead-stripped `AXFXReverbHiExpGetMemSize`, `AXFXReverbHiExpGetMemSizeDpl2` and
`AXFXReverbStdExpGetMemSize` (kept because each one's 32000.0f heads its unit's `.sdata2` pool; exceptions.md, "sdk-axfx
rulings").

## Function status

Status values: `matched` (linked, bytes identical, every [match] rule passes); `MATCH-PENDING` (linked, bytes
identical, a [match] rule fails); `ORIGINAL-ASM`; `parked` (unit is `NonMatching`, with objdiff's score).
Naming lists placeholder names (R7), which are open work and do not affect the status.

<!-- ledger:begin status -->
| unit | function | address | size | status | naming |
|---|---|---|---|---|---|
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBMtrHandler` | 0x801900E8 | 76 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBIntrHandler` | 0x80190134 | 80 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBCheckMailBox` | 0x80190184 | 112 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBReadUSB_CSR` | 0x801901F4 | 48 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBInitComm` | 0x80190224 | 88 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBInitInterrupts` | 0x8019027C | 88 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBQueryData` | 0x801902D4 | 80 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBRead` | 0x80190324 | 124 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBWrite` | 0x801903A0 | 188 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBWaitForSendMail` | 0x8019045C | 60 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBOpen` | 0x80190498 | 4 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `DBClose` | 0x8019049C | 4 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `ODEMUGetSize` | 0x801904A0 | 8 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `ODEMUIsValidMail` | 0x801904A8 | 24 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `ODEMUGetPc2NngcOffset` | 0x801904C0 | 76 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `ODEMUGetPage` | 0x8019050C | 12 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `ODEMUGenMailData` | 0x80190518 | 20 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBReadMailbox` | 0x8019052C | 52 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBRead` | 0x80190560 | 64 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBWriteMailbox` | 0x801905A0 | 52 | matched |  |
| `revolution/NdevExi2AD/DebuggerDriver` | `__DBWrite` | 0x801905D4 | 68 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__EXI2Select` | 0x80190618 | 48 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__EXI2Deselect` | 0x80190648 | 44 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__EXI2Sync` | 0x80190674 | 28 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__EXI2Imm` | 0x80190690 | 216 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__DBEXIInit` | 0x80190768 | 96 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__DBEXIReadReg` | 0x801907C8 | 288 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__DBEXIWriteReg` | 0x801908E8 | 284 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__DBEXIReadRam` | 0x80190A04 | 228 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__DBEXIWriteRam` | 0x80190AE8 | 228 | matched |  |
| `revolution/NdevExi2AD/exi2` | `__EXISwap32` | 0x80190BCC | 32 | matched |  |
| `MSL_C/MSL_Common/Src/alloc` | `Block_subBlock` | 0x80195D28 | 516 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `Block_link` | 0x80195F2C | 336 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `SubBlock_merge_next` | 0x8019607C | 168 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `allocate_from_var_pools` | 0x80196124 | 576 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `soft_allocate_from_var_pools` | 0x80196364 | 220 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `FixBlock_construct` | 0x80196440 | 292 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `allocate_from_fixed_pools` | 0x80196564 | 520 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `deallocate_from_fixed_pools` | 0x8019676C | 492 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `__pool_resize_inplace` | 0x80196958 | 608 | matched | unit name unconfirmed; invented name |
| `MSL_C/MSL_Common/Src/alloc` | `malloc` | 0x80196BB8 | 160 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `free` | 0x80196C58 | 304 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `realloc` | 0x80196D88 | 792 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/alloc` | `calloc` | 0x801970A0 | 192 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_files` | `__close_all` | 0x80197160 | 164 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_files` | `__flush_all` | 0x80197204 | 108 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__ull2dec` | 0x80197270 | 220 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__timesdec` | 0x8019734C | 648 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__str2dec` | 0x801975D4 | 236 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__two_exp` | 0x801976C0 | 796 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__equals_dec` | 0x801979DC | 228 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__less_dec` | 0x80197AC0 | 256 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__minus_dec` | 0x80197BC0 | 1244 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__num2dec_internal` | 0x8019809C | 356 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__num2dec` | 0x80198200 | 420 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/ansi_fp` | `__dec2num` | 0x801983A4 | 1868 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/abs` | `abs` | 0x80198AF0 | 16 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/buffer_io` | `__prep_buffer` | 0x80198B00 | 40 | matched |  |
| `MSL_C/MSL_Common/Src/buffer_io` | `__flush_buffer` | 0x80198B28 | 184 | matched |  |
| `MSL_C/MSL_Common/Src/direct_io` | `__fwrite` | 0x80198BE0 | 776 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/file_io` | `fclose` | 0x80198EE8 | 188 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/file_io` | `fflush` | 0x80198FA4 | 308 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/FILE_POS` | `_ftell` | 0x801990D8 | 168 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/FILE_POS` | `ftell` | 0x80199180 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/FILE_POS` | `_fseek` | 0x80199184 | 452 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/mbstring` | `mbtowc` | 0x80199348 | 24 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/mbstring` | `__mbtowc_noconv` | 0x80199360 | 76 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/mbstring` | `__wctomb_noconv` | 0x801993AC | 28 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/mbstring` | `mbstowcs` | 0x801993C8 | 192 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/mbstring` | `wcstombs` | 0x80199488 | 184 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/mem` | `memmove` | 0x80199540 | 204 | matched |  |
| `MSL_C/MSL_Common/Src/mem` | `memchr` | 0x8019960C | 44 | matched |  |
| `MSL_C/MSL_Common/Src/mem` | `__memrchr` | 0x80199638 | 44 | matched |  |
| `MSL_C/MSL_Common/Src/mem` | `memcmp` | 0x80199664 | 76 | matched |  |
| `MSL_C/MSL_Common/Src/mem_funcs` | `__copy_longs_aligned` | 0x801996B0 | 188 | matched |  |
| `MSL_C/MSL_Common/Src/mem_funcs` | `__copy_longs_rev_aligned` | 0x8019976C | 168 | matched |  |
| `MSL_C/MSL_Common/Src/mem_funcs` | `__copy_longs_unaligned` | 0x80199814 | 192 | matched |  |
| `MSL_C/MSL_Common/Src/mem_funcs` | `__copy_longs_rev_unaligned` | 0x801998D4 | 172 | matched |  |
| `MSL_C/MSL_Common/Src/fpclassify` | `__signbitd` | 0x80199980 | 24 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/fpclassify` | `__fpclassifyd` | 0x80199998 | 124 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/misc_io` | `__stdio_atexit` | 0x80199A14 | 16 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `parse_format` | 0x80199A24 | 1468 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `long2str` | 0x80199FE0 | 576 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `longlong2str` | 0x8019A220 | 688 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `double2hex` | 0x8019A4D0 | 968 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `round_decimal` | 0x8019A898 | 296 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `float2str` | 0x8019A9C0 | 1948 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `__pformatter` | 0x8019B15C | 2220 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `__FileWrite` | 0x8019BA08 | 88 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `__StringWrite` | 0x8019BA60 | 108 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `printf` | 0x8019BACC | 204 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `vprintf` | 0x8019BB98 | 124 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `vsnprintf` | 0x8019BC14 | 136 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `vsprintf` | 0x8019BC9C | 132 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `snprintf` | 0x8019BD20 | 244 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/printf` | `sprintf` | 0x8019BE14 | 212 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/qsort` | `qsort` | 0x8019BEE8 | 368 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/rand` | `rand` | 0x8019C058 | 32 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/rand` | `srand` | 0x8019C078 | 8 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/scanf` | `parse_format` | 0x8019C080 | 1684 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/scanf` | `__sformatter` | 0x8019C714 | 3224 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/scanf` | `__StringRead` | 0x8019D3AC | 136 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/scanf` | `sscanf` | 0x8019D434 | 304 | matched | unit name unconfirmed; helper `__is_blank` invented |
| `MSL_C/MSL_Common/Src/string` | `strcpy` | 0x8019D564 | 192 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strncpy` | 0x8019D624 | 68 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strcat` | 0x8019D668 | 44 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strncat` | 0x8019D694 | 76 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strcmp` | 0x8019D6E0 | 284 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strncmp` | 0x8019D7FC | 64 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strchr` | 0x8019D83C | 48 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strtok` | 0x8019D86C | 292 | matched |  |
| `MSL_C/MSL_Common/Src/string` | `strstr` | 0x8019D990 | 108 | matched |  |
| `MSL_C/MSL_Common/Src/strtold` | `__strtold` | 0x8019D9FC | 4900 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/strtold` | `strtod` | 0x8019ED20 | 92 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/strtoul` | `__strtoul` | 0x8019ED7C | 1044 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/strtoul` | `__strtoull` | 0x8019F190 | 1192 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/strtoul` | `atoi` | 0x8019F638 | 196 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/strtoul` | `atol` | 0x8019F6FC | 196 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wmem` | `wmemcpy` | 0x8019F7C0 | 8 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wmem` | `wmemchr` | 0x8019F7C8 | 40 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `parse_format` | 0x8019F7F0 | 1424 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `long2str` | 0x8019FD80 | 580 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `longlong2str` | 0x8019FFC4 | 700 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `double2hex` | 0x801A0280 | 1108 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `round_decimal` | 0x801A06D4 | 296 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `float2str` | 0x801A07FC | 1936 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `__wpformatter` | 0x801A0F8C | 2444 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `__wStringWrite` | 0x801A1918 | 108 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `swprintf` | 0x801A1984 | 248 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wprintf` | `vswprintf` | 0x801A1A7C | 140 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wstring` | `wcslen` | 0x801A1B08 | 28 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wstring` | `wcscpy` | 0x801A1B24 | 28 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wstring` | `wcsncpy` | 0x801A1B40 | 68 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wstring` | `wcsncat` | 0x801A1B84 | 76 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wstring` | `wcscmp` | 0x801A1BD0 | 52 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wstring` | `wcschr` | 0x801A1C04 | 44 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/wchar_io` | `fwide` | 0x801A1C30 | 120 | matched |  |
| `MSL_C/MSL_Common/Src/environment` | `getenv` | 0x801A1CA8 | 8 | matched | unit name unconfirmed |
| `MSL_C/PPC_EABI/Src/uart_console_io_gcn` | `__write_console` | 0x801A1CB0 | 208 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/secure_error` | `__msl_runtime_constraint_violation_s` | 0x801A1D80 | 24 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Src/math_sun` | `scalbn` | 0x801A1D98 | 64 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_acos` | `__ieee754_acos` | 0x801A1DD8 | 716 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_asin` | `__ieee754_asin` | 0x801A20A4 | 664 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_atan2` | `__ieee754_atan2` | 0x801A233C | 600 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_exp` | `__ieee754_exp` | 0x801A2594 | 560 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_fmod` | `__ieee754_fmod` | 0x801A27C4 | 896 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_log` | `__ieee754_log` | 0x801A2B44 | 692 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_log10` | `__ieee754_log10` | 0x801A2DF8 | 276 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_pow` | `__ieee754_pow` | 0x801A2F0C | 2068 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_rem_pio2` | `__ieee754_rem_pio2` | 0x801A3720 | 900 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/k_cos` | `__kernel_cos` | 0x801A3AA4 | 272 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/k_rem_pio2` | `__kernel_rem_pio2` | 0x801A3BB4 | 5816 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/k_sin` | `__kernel_sin` | 0x801A526C | 192 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/k_tan` | `__kernel_tan` | 0x801A532C | 592 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_atan` | `atan` | 0x801A557C | 560 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_ceil` | `ceil` | 0x801A57AC | 332 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_copysign` | `copysign` | 0x801A58F8 | 44 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_cos` | `cos` | 0x801A5924 | 200 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_floor` | `floor` | 0x801A59EC | 336 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_frexp` | `frexp` | 0x801A5B3C | 136 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_ldexp` | `ldexp` | 0x801A5BC4 | 364 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_sin` | `sin` | 0x801A5D30 | 204 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/s_tan` | `tan` | 0x801A5DFC | 120 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_acos` | `acos` | 0x801A5E74 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_asin` | `asin` | 0x801A5E78 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_atan2` | `atan2` | 0x801A5E7C | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_exp` | `exp` | 0x801A5E80 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_fmod` | `fmod` | 0x801A5E84 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log` | `log` | 0x801A5E88 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log10` | `log10` | 0x801A5E8C | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_pow` | `pow` | 0x801A5E90 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/e_sqrt` | `__ieee754_sqrt` | 0x801A5E94 | 584 | matched | unit name unconfirmed |
| `MSL_C/PPC_EABI/Src/math_ppc` | `nan` | 0x801A60DC | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common_Embedded/Math/Double_precision/w_sqrt` | `sqrt` | 0x801A60E0 | 4 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/extras` | `strlwr` | 0x801A60E4 | 84 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/extras` | `stricmp` | 0x801A6138 | 160 | matched | unit name unconfirmed |
| `MSL_C/MSL_Common/Src/extras` | `strupr` | 0x801A61D8 | 84 | matched | unit name unconfirmed |
| `Runtime.PPCEABI.H/__mem` | `strlen` | 0x801A622C | 28 | matched |  |
| `Runtime.PPCEABI.H/__mem` | `memcpy` | 0x80005F34 | 668 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/__mem` | `__fill_mem` | 0x800061D0 | 180 | matched |  |
| `Runtime.PPCEABI.H/__mem` | `memset` | 0x80006284 | 48 | matched |  |
| `Runtime.PPCEABI.H/__va_arg` | `__va_arg` | 0x801A6248 | 200 | matched |  |
| `Runtime.PPCEABI.H/global_destructor_chain` | `__register_global_object` | 0x801A6310 | 24 | matched |  |
| `Runtime.PPCEABI.H/global_destructor_chain` | `__destroy_global_chain` | 0x801A6328 | 72 | matched |  |
| `Runtime.PPCEABI.H/NMWException` | `__construct_new_array` | 0x801A6370 | 260 | matched |  |
| `Runtime.PPCEABI.H/NMWException` | `__dt__26__partial_array_destructorFv` | 0x801A6474 | 188 | matched |  |
| `Runtime.PPCEABI.H/NMWException` | `__construct_array` | 0x801A6530 | 248 | matched |  |
| `Runtime.PPCEABI.H/NMWException` | `__destroy_arr` | 0x801A6628 | 120 | matched |  |
| `Runtime.PPCEABI.H/NMWException` | `__destroy_new_array` | 0x801A66A0 | 128 | matched |  |
| `Runtime.PPCEABI.H/ptmf` | `__ptmf_scall` | 0x801A6720 | 40 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__cvt_fp2unsigned` | 0x801A6748 | 92 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__save_fpr` | 0x801A67A4 | 76 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__restore_fpr` | 0x801A67F0 | 76 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__save_gpr` | 0x801A683C | 76 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__restore_gpr` | 0x801A6888 | 76 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__div2u` | 0x801A68D4 | 236 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__div2i` | 0x801A69C0 | 312 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__mod2u` | 0x801A6AF8 | 228 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__mod2i` | 0x801A6BDC | 268 | ORIGINAL-ASM |  |
| `Runtime.PPCEABI.H/runtime` | `__shl2i` | 0x801A6CE8 | 36 | ORIGINAL-ASM | named from behaviour and runtime.c order |
| `Runtime.PPCEABI.H/runtime` | `__cvt_sll_flt` | 0x801A6D0C | 180 | ORIGINAL-ASM | named from behaviour and runtime.c order |
| `Runtime.PPCEABI.H/runtime` | `__cvt_ull_flt` | 0x801A6DC0 | 160 | ORIGINAL-ASM | named from behaviour and runtime.c order |
| `Runtime.PPCEABI.H/runtime` | `__cvt_dbl_usll` | 0x801A6E60 | 204 | ORIGINAL-ASM | named from behaviour and runtime.c order |
| `Runtime.PPCEABI.H/runtime` | `__cvt_dbl_ull` | 0x801A6F2C | 168 | ORIGINAL-ASM | named from behaviour and runtime.c order |
| `Runtime.PPCEABI.H/__init_cpp_exceptions` | `__init_cpp_exceptions` | 0x801A6FD4 | 60 | matched |  |
| `Runtime.PPCEABI.H/__init_cpp_exceptions` | `__fini_cpp_exceptions` | 0x801A7010 | 52 | matched |  |
| `Runtime.PPCEABI.H/Gecko_ExceptionPPC` | `__register_fragment` | 0x801A7044 | 76 | parked, 93.7% |  |
| `Runtime.PPCEABI.H/Gecko_ExceptionPPC` | `__unregister_fragment` | 0x801A7090 | 40 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/__exception` | `pad_00_80004000_init` | 0x80004000 | 7988 | ORIGINAL-ASM |  |
| `MetroTRK/Processor/ppc/Export/targsupp` | `TRKAccessFile` | 0x80190BEC | 8 | ORIGINAL-ASM |  |
| `MetroTRK/Processor/ppc/Export/targsupp` | `TRKOpenFile` | 0x80190BF4 | 8 | ORIGINAL-ASM |  |
| `MetroTRK/Processor/ppc/Export/targsupp` | `TRKCloseFile` | 0x80190BFC | 8 | ORIGINAL-ASM |  |
| `MetroTRK/Processor/ppc/Export/targsupp` | `TRKPositionFile` | 0x80190C04 | 8 | ORIGINAL-ASM |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_initialize` | 0x80190C0C | 60 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_shutdown` | 0x80190C48 | 8 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_open` | 0x80190C50 | 36 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_close` | 0x80190C74 | 8 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_read` | 0x80190C7C | 180 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_write` | 0x80190D30 | 116 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_pre_continue` | 0x80190DA4 | 36 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_post_stop` | 0x80190DC8 | 36 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_peek` | 0x80190DEC | 116 | matched |  |
| `MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main` | `gdev_cc_initinterrupts` | 0x80190E60 | 36 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/gc/MWCriticalSection_gc` | `MWInitializeCriticalSection` | 0x80190E84 | 4 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/gc/MWCriticalSection_gc` | `MWEnterCriticalSection` | 0x80190E88 | 48 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/gc/MWCriticalSection_gc` | `MWExitCriticalSection` | 0x80190EB8 | 8 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer` | `CBGetBytesAvailableForRead` | 0x80190EC0 | 8 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer` | `CircleBufferInitialize` | 0x80190EC8 | 36 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer` | `CircleBufferWriteBytes` | 0x80190EEC | 264 | matched |  |
| `MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer` | `CircleBufferReadBytes` | 0x80190FF4 | 264 | matched |  |
| `MetroTRK/Processor/ppc/Generic/flush_cache` | `TRK_flush_cache` | 0x801910FC | 56 | ORIGINAL-ASM |  |
| `MetroTRK/Portable/main_TRK` | `TRK_main` | 0x80191134 | 60 | matched |  |
| `MetroTRK/Portable/mainloop` | `TRKNubMainLoop` | 0x80191170 | 236 | matched |  |
| `MetroTRK/Portable/mem_TRK` | `TRK_memcpy` | 0x8019125C | 316 | matched |  |
| `MetroTRK/Portable/mem_TRK` | `TRK_memset` | 0x80191398 | 296 | matched |  |
| `MetroTRK/Portable/dispatch` | `TRKDispatchMessage` | 0x801914C0 | 288 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `InitMetroTRK` | 0x801915E0 | 152 | ORIGINAL-ASM |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `InitMetroTRK_BBA` | 0x80191678 | 148 | ORIGINAL-ASM |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `EnableMetroTRKInterrupts` | 0x8019170C | 4 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `TRKTargetTranslate` | 0x80191710 | 104 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `__TRK_copy_vectors` | 0x80191778 | 308 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `TRKInitializeTarget` | 0x801918AC | 72 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk` | `__TRK_reset` | 0x801918F4 | 16 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKLoadContext` | 0x80191904 | 136 | ORIGINAL-ASM |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKEXICallBack` | 0x8019198C | 56 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `InitMetroTRKCommTable` | 0x801919C4 | 312 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKUARTInterruptHandler` | 0x80191AFC | 4 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKInitializeIntDrivenUART` | 0x80191B00 | 84 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `EnableEXI2Interrupts` | 0x80191B54 | 44 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKPollUART` | 0x80191B80 | 20 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKReadUARTN` | 0x80191B94 | 60 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRKWriteUARTN` | 0x80191BD0 | 60 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `ReserveEXI2Port` | 0x80191C0C | 20 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `UnreserveEXI2Port` | 0x80191C20 | 20 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `TRK_board_display` | 0x80191C34 | 24 | matched |  |
| `MetroTRK/Os/dolphin/dolphin_trk_glue` | `InitializeProgramEndTrap` | 0x80191C4C | 84 | matched |  |
| `MetroTRK/Portable/notify` | `TRKDoNotifyStopped` | 0x80191CA0 | 144 | matched |  |
| `MetroTRK/Portable/nubevent` | `TRKInitializeEventQueue` | 0x80191D30 | 36 | matched |  |
| `MetroTRK/Portable/nubevent` | `TRKGetNextEvent` | 0x80191D54 | 140 | matched |  |
| `MetroTRK/Portable/nubevent` | `TRKPostEvent` | 0x80191DE0 | 204 | matched |  |
| `MetroTRK/Portable/nubevent` | `TRKConstructEvent` | 0x80191EAC | 24 | matched |  |
| `MetroTRK/Portable/nubevent` | `TRKDestructEvent` | 0x80191EC4 | 8 | matched |  |
| `MetroTRK/Portable/nubinit` | `TRKInitializeNub` | 0x80191ECC | 172 | matched |  |
| `MetroTRK/Portable/nubinit` | `TRKTerminateNub` | 0x80191F78 | 36 | matched |  |
| `MetroTRK/Portable/nubinit` | `TRKNubWelcome` | 0x80191F9C | 12 | matched |  |
| `MetroTRK/Portable/nubinit` | `TRKInitializeEndian` | 0x80191FA8 | 116 | matched |  |
| `MetroTRK/Portable/serpoll` | `TRKTestForPacket` | 0x8019201C | 204 | matched |  |
| `MetroTRK/Portable/serpoll` | `TRKGetInput` | 0x801920E8 | 44 | matched |  |
| `MetroTRK/Portable/serpoll` | `TRKProcessInput` | 0x80192114 | 64 | matched |  |
| `MetroTRK/Portable/serpoll` | `TRKInitializeSerialHandler` | 0x80192154 | 8 | matched |  |
| `MetroTRK/Portable/serpoll` | `TRKTerminateSerialHandler` | 0x8019215C | 8 | matched |  |
| `MetroTRK/Portable/string_TRK` | `TRK_strlen` | 0x80192164 | 28 | matched |  |
| `MetroTRK/Portable/support` | `TRKSuppAccessFile` | 0x80192180 | 500 | matched |  |
| `MetroTRK/Portable/support` | `TRKRequestSend` | 0x80192374 | 296 | matched |  |
| `MetroTRK/Portable/support` | `HandleOpenFileSupportRequest` | 0x8019249C | 280 | matched |  |
| `MetroTRK/Portable/support` | `HandleCloseFileSupportRequest` | 0x801925B4 | 228 | matched |  |
| `MetroTRK/Portable/support` | `HandlePositionFileSupportRequest` | 0x80192698 | 268 | matched |  |
| `MetroTRK/Os/dolphin/targcont` | `TRKTargetContinue` | 0x801927A4 | 52 | matched |  |
| `MetroTRK/Processor/ppc/Generic/mpc_7xx_603e` | `TRKSaveExtended1Block` | 0x801927D8 | 452 | ORIGINAL-ASM |  |
| `MetroTRK/Processor/ppc/Generic/mpc_7xx_603e` | `TRKRestoreExtended1Block` | 0x8019299C | 356 | ORIGINAL-ASM |  |
| `MetroTRK/Portable/msg` | `TRKMessageSend` | 0x80192B00 | 100 | matched | invented sequence stamp field `CommandReply.sequence` |
| `MetroTRK/Portable/msgbuf` | `TRKInitializeMessageBuffers` | 0x80192B64 | 32 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKGetFreeBuffer` | 0x80192B84 | 156 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKGetBuffer` | 0x80192C20 | 36 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKReleaseBuffer` | 0x80192C44 | 40 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKResetBuffer` | 0x80192C6C | 40 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKSetBufferPosition` | 0x80192C94 | 48 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKAppendBuffer` | 0x80192CC4 | 164 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKReadBuffer` | 0x80192D68 | 144 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKAppendBuffer1_ui32` | 0x80192DF8 | 208 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKAppendBuffer1_ui64` | 0x80192EC8 | 244 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKAppendBuffer_ui8` | 0x80192FBC | 100 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKAppendBuffer_ui32` | 0x80193020 | 240 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKReadBuffer1_ui64` | 0x80193110 | 224 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKReadBuffer_ui8` | 0x801931F0 | 152 | matched |  |
| `MetroTRK/Portable/msgbuf` | `TRKReadBuffer_ui32` | 0x80193288 | 232 | matched |  |
| `MetroTRK/Portable/msghndlr` | `GetTRKConnected` | 0x80193370 | 8 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoConnect` | 0x80193378 | 112 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoDisconnect` | 0x801933E8 | 136 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoReset` | 0x80193470 | 108 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoOverride` | 0x801934DC | 108 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoReadMemory` | 0x80193548 | 564 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoWriteMemory` | 0x8019377C | 528 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoReadRegisters` | 0x8019398C | 508 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoWriteRegisters` | 0x80193B88 | 672 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoContinue` | 0x80193E28 | 196 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoStep` | 0x80193EEC | 644 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoStop` | 0x80194170 | 184 | matched |  |
| `MetroTRK/Portable/msghndlr` | `TRKDoSetOption` | 0x80194228 | 204 | matched |  |
| `MetroTRK/Export/mslsupp` | `__read_console` | 0x801942F4 | 104 | matched |  |
| `MetroTRK/Export/mslsupp` | `__TRK_write_console` | 0x8019435C | 104 | matched |  |
| `MetroTRK/Export/mslsupp` | `__close_console` | 0x801943C4 | 4 | matched |  |
| `MetroTRK/Export/mslsupp` | `__read_file` | 0x801943C8 | 8 | matched |  |
| `MetroTRK/Export/mslsupp` | `__write_file` | 0x801943D0 | 8 | matched |  |
| `MetroTRK/Export/mslsupp` | `__close_file` | 0x801943D8 | 112 | matched | inferred name (the MSL close hook beside `__read_file`/`__write_file`; no evidence string) |
| `MetroTRK/Export/mslsupp` | `__access_file` | 0x80194448 | 172 | matched |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `__TRK_get_MSR` | 0x801944F4 | 8 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `__TRK_set_MSR` | 0x801944FC | 8 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKValidMemory32` | 0x80194504 | 248 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRK_ppc_memcpy` | 0x801945FC | 204 | parked, 100% (C with an inline-asm block; listed helper when linked) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAccessMemory` | 0x801946C8 | 336 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAccessDefault` | 0x80194818 | 248 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAccessFP` | 0x80194910 | 316 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAccessExtended1` | 0x80194A4C | 356 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAccessExtended2` | 0x80194BB0 | 368 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKInterruptHandler` | 0x80194D20 | 404 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKExceptionHandler` | 0x80194EB4 | 156 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKPostInterruptEvent` | 0x80194F50 | 184 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKSwapAndGo` | 0x80195008 | 196 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKInterruptHandlerEnableInterrupts` | 0x801950CC | 84 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetInterrupt` | 0x80195120 | 92 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAddStopInfo` | 0x8019517C | 512 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetAddExceptionInfo` | 0x8019537C | 156 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetCheckStep` | 0x80195418 | 292 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetSingleStep` | 0x8019553C | 136 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetStepOutOfRange` | 0x801955C4 | 116 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetGetPC` | 0x80195638 | 16 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetSupportRequest` | 0x80195648 | 500 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetStopped` | 0x8019583C | 16 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetSetStopped` | 0x8019584C | 16 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetStop` | 0x8019585C | 24 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKPPCAccessSPR` | 0x80195874 | 220 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKPPCAccessPairedSingleRegister` | 0x80195950 | 168 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `ReadFPSCR` | 0x801959F8 | 36 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `WriteFPSCR` | 0x80195A1C | 36 | parked, 100% (ORIGINAL-ASM when linked; not counted) |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKPPCAccessFPRegister` | 0x80195A40 | 456 | parked, 96.0% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKPPCAccessSpecialReg` | 0x80195C08 | 104 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `TRKTargetSetInputPendingPtr` | 0x80195C70 | 16 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `ConvertAddress` | 0x80195C80 | 8 | parked, 100% |  |
| `MetroTRK/Processor/ppc/Generic/targimpl` | `GetThreadInfo` | 0x80195C88 | 144 | parked, 100% |  |
| `MetroTRK/Os/dolphin/target_options` | `SetUseSerialIO` | 0x80195D18 | 8 | matched |  |
| `MetroTRK/Os/dolphin/target_options` | `GetUseSerialIO` | 0x80195D20 | 8 | matched |  |
| `libjpeg/jcomapi` | `jpeg_abort` | 0x801B86C8 | 108 | matched |  |
| `libjpeg/jcomapi` | `jpeg_destroy` | 0x801B8734 | 76 | matched |  |
| `libjpeg/jcomapi` | `jpeg_alloc_quant_table` | 0x801B8780 | 60 | matched |  |
| `libjpeg/jcomapi` | `jpeg_alloc_huff_table` | 0x801B87BC | 60 | matched |  |
| `libjpeg/jdapimin` | `jpeg_CreateDecompress` | 0x801B87F8 | 320 | matched |  |
| `libjpeg/jdapimin` | `jpeg_destroy_decompress` | 0x801B8938 | 4 | matched |  |
| `libjpeg/jdapimin` | `default_decompress_parms` | 0x801B893C | 620 | matched |  |
| `libjpeg/jdapimin` | `jpeg_read_header` | 0x801B8BA8 | 200 | matched |  |
| `libjpeg/jdapimin` | `jpeg_consume_input` | 0x801B8C70 | 292 | matched |  |
| `libjpeg/jdapimin` | `jpeg_finish_decompress` | 0x801B8D94 | 308 | matched |  |
| `libjpeg/jdapistd` | `jpeg_start_decompress` | 0x801B8EC8 | 344 | matched |  |
| `libjpeg/jdapistd` | `output_pass_setup` | 0x801B9020 | 320 | matched |  |
| `libjpeg/jdapistd` | `jpeg_read_scanlines` | 0x801B9160 | 284 | matched |  |
| `libjpeg/jdcoefct` | `start_input_pass` | 0x801B927C | 96 | matched |  |
| `libjpeg/jdcoefct` | `start_output_pass` | 0x801B92DC | 124 | matched |  |
| `libjpeg/jdcoefct` | `decompress_onepass` | 0x801B9358 | 608 | matched |  |
| `libjpeg/jdcoefct` | `dummy_consume_data` | 0x801B95B8 | 8 | matched |  |
| `libjpeg/jdcoefct` | `consume_data` | 0x801B95C0 | 548 | matched |  |
| `libjpeg/jdcoefct` | `decompress_data` | 0x801B97E4 | 440 | matched |  |
| `libjpeg/jdcoefct` | `smoothing_ok` | 0x801B999C | 444 | matched |  |
| `libjpeg/jdcoefct` | `decompress_smooth_data` | 0x801B9B58 | 1744 | matched |  |
| `libjpeg/jdcoefct` | `jinit_d_coef_controller` | 0x801BA228 | 424 | matched |  |
| `libjpeg/jdcolor` | `ycc_rgb_convert` | 0x801BA3D0 | 392 | matched |  |
| `libjpeg/jdcolor` | `null_convert` | 0x801BA558 | 228 | matched |  |
| `libjpeg/jdcolor` | `grayscale_convert` | 0x801BA63C | 28 | matched |  |
| `libjpeg/jdcolor` | `gray_rgb_convert` | 0x801BA658 | 284 | matched |  |
| `libjpeg/jdcolor` | `ycck_cmyk_convert` | 0x801BA774 | 492 | matched |  |
| `libjpeg/jdcolor` | `start_pass_dcolor` | 0x801BA960 | 4 | matched |  |
| `libjpeg/jdcolor` | `jinit_color_deconverter` | 0x801BA964 | 2720 | matched |  |
| `libjpeg/jddctmgr` | `start_pass` | 0x801BB404 | 1428 | matched |  |
| `libjpeg/jddctmgr` | `jinit_inverse_dct` | 0x801BB998 | 172 | matched |  |
| `libjpeg/jdhuff` | `start_pass_huff_decoder` | 0x801BBA44 | 424 | matched |  |
| `libjpeg/jdhuff` | `jpeg_make_d_derived_tbl` | 0x801BBBEC | 1380 | matched |  |
| `libjpeg/jdhuff` | `jpeg_fill_bit_buffer` | 0x801BC150 | 384 | matched |  |
| `libjpeg/jdhuff` | `jpeg_huff_decode` | 0x801BC2D0 | 300 | matched |  |
| `libjpeg/jdhuff` | `decode_mcu` | 0x801BC3FC | 1328 | matched |  |
| `libjpeg/jdhuff` | `jinit_huff_decoder` | 0x801BC92C | 128 | matched |  |
| `libjpeg/jdinput` | `initial_setup` | 0x801BC9AC | 648 | matched |  |
| `libjpeg/jdinput` | `per_scan_setup` | 0x801BCC34 | 684 | matched |  |
| `libjpeg/jdinput` | `start_input_pass` | 0x801BCEE0 | 276 | matched |  |
| `libjpeg/jdinput` | `finish_input_pass` | 0x801BCFF4 | 20 | matched |  |
| `libjpeg/jdinput` | `consume_markers` | 0x801BD008 | 520 | matched |  |
| `libjpeg/jdinput` | `reset_input_controller` | 0x801BD210 | 124 | matched |  |
| `libjpeg/jdinput` | `jinit_input_controller` | 0x801BD28C | 136 | matched |  |
| `libjpeg/jdmainct` | `make_funny_pointers` | 0x801BD314 | 1056 | matched |  |
| `libjpeg/jdmainct` | `set_wraparound_pointers` | 0x801BD734 | 464 | matched |  |
| `libjpeg/jdmainct` | `set_bottom_pointers` | 0x801BD904 | 392 | matched |  |
| `libjpeg/jdmainct` | `start_pass_main` | 0x801BDA8C | 184 | matched |  |
| `libjpeg/jdmainct` | `process_data_simple_main` | 0x801BDB44 | 176 | matched |  |
| `libjpeg/jdmainct` | `process_data_context_main` | 0x801BDBF4 | 448 | matched |  |
| `libjpeg/jdmainct` | `process_data_crank_post` | 0x801BDDB4 | 40 | matched |  |
| `libjpeg/jdmainct` | `jinit_d_main_controller` | 0x801BDDDC | 488 | matched |  |
| `libjpeg/jdmarker` | `get_soi` | 0x801BDFC4 | 380 | matched |  |
| `libjpeg/jdmarker` | `get_sof` | 0x801BE140 | 1184 | matched |  |
| `libjpeg/jdmarker` | `get_sos` | 0x801BE5E0 | 992 | matched |  |
| `libjpeg/jdmarker` | `get_dht` | 0x801BE9C0 | 948 | matched |  |
| `libjpeg/jdmarker` | `get_dqt` | 0x801BED74 | 832 | matched |  |
| `libjpeg/jdmarker` | `get_dri` | 0x801BF0B4 | 396 | matched |  |
| `libjpeg/jdmarker` | `examine_app0` | 0x801BF240 | 860 | matched |  |
| `libjpeg/jdmarker` | `examine_app14` | 0x801BF59C | 284 | matched |  |
| `libjpeg/jdmarker` | `get_interesting_appn` | 0x801BF6B8 | 464 | matched |  |
| `libjpeg/jdmarker` | `skip_variable` | 0x801BF888 | 272 | matched |  |
| `libjpeg/jdmarker` | `next_marker` | 0x801BF998 | 416 | matched |  |
| `libjpeg/jdmarker` | `first_marker` | 0x801BFB38 | 244 | matched |  |
| `libjpeg/jdmarker` | `read_markers` | 0x801BFC2C | 900 | matched |  |
| `libjpeg/jdmarker` | `read_restart_marker` | 0x801BFFB0 | 220 | matched |  |
| `libjpeg/jdmarker` | `jpeg_resync_to_restart` | 0x801C008C | 364 | matched |  |
| `libjpeg/jdmarker` | `reset_marker_reader` | 0x801C01F8 | 40 | matched |  |
| `libjpeg/jdmarker` | `jinit_marker_reader` | 0x801C0220 | 292 | matched |  |
| `libjpeg/jdmaster` | `use_merged_upsample` | 0x801C0344 | 228 | matched |  |
| `libjpeg/jdmaster` | `jpeg_calc_output_dimensions` | 0x801C0428 | 692 | matched |  |
| `libjpeg/jdmaster` | `prepare_range_limit_table` | 0x801C06DC | 676 | matched |  |
| `libjpeg/jdmaster` | `master_selection` | 0x801C0980 | 820 | matched |  |
| `libjpeg/jdmaster` | `prepare_for_output_pass` | 0x801C0CB4 | 592 | matched |  |
| `libjpeg/jdmaster` | `finish_output_pass` | 0x801C0F04 | 80 | matched |  |
| `libjpeg/jdmaster` | `jinit_master_decompress` | 0x801C0F54 | 108 | matched |  |
| `libjpeg/jdmerge` | `start_pass_merged_upsample` | 0x801C0FC0 | 24 | matched |  |
| `libjpeg/jdmerge` | `merged_2v_upsample` | 0x801C0FD8 | 304 | matched |  |
| `libjpeg/jdmerge` | `merged_1v_upsample` | 0x801C1108 | 108 | matched |  |
| `libjpeg/jdmerge` | `h2v1_merged_upsample` | 0x801C1174 | 556 | matched |  |
| `libjpeg/jdmerge` | `h2v2_merged_upsample` | 0x801C13A0 | 440 | matched |  |
| `libjpeg/jdmerge` | `jinit_merged_upsampler` | 0x801C1558 | 1208 | matched |  |
| `libjpeg/jdphuff` | `start_pass_phuff_decoder` | 0x801C1A10 | 764 | matched |  |
| `libjpeg/jdphuff` | `decode_mcu_DC_first` | 0x801C1D0C | 776 | matched |  |
| `libjpeg/jdphuff` | `decode_mcu_AC_first` | 0x801C2014 | 792 | matched |  |
| `libjpeg/jdphuff` | `decode_mcu_DC_refine` | 0x801C232C | 428 | matched |  |
| `libjpeg/jdphuff` | `decode_mcu_AC_refine` | 0x801C24D8 | 1364 | matched |  |
| `libjpeg/jdphuff` | `jinit_phuff_decoder` | 0x801C2A2C | 308 | matched |  |
| `libjpeg/jdpostct` | `start_pass_dpost` | 0x801C2B60 | 312 | matched |  |
| `libjpeg/jdpostct` | `post_process_1pass` | 0x801C2C98 | 192 | matched |  |
| `libjpeg/jdpostct` | `post_process_prepass` | 0x801C2D58 | 260 | matched |  |
| `libjpeg/jdpostct` | `post_process_2pass` | 0x801C2E5C | 260 | matched |  |
| `libjpeg/jdpostct` | `jinit_d_post_controller` | 0x801C2F60 | 260 | matched |  |
| `libjpeg/jdsample` | `start_pass_upsample` | 0x801C3064 | 24 | matched |  |
| `libjpeg/jdsample` | `sep_upsample` | 0x801C307C | 332 | matched |  |
| `libjpeg/jdsample` | `fullsize_upsample` | 0x801C31C8 | 8 | matched |  |
| `libjpeg/jdsample` | `noop_upsample` | 0x801C31D0 | 12 | matched |  |
| `libjpeg/jdsample` | `int_upsample` | 0x801C31DC | 328 | matched |  |
| `libjpeg/jdsample` | `h2v1_upsample` | 0x801C3324 | 232 | matched |  |
| `libjpeg/jdsample` | `h2v2_upsample` | 0x801C340C | 304 | matched |  |
| `libjpeg/jdsample` | `h2v1_fancy_upsample` | 0x801C353C | 436 | matched |  |
| `libjpeg/jdsample` | `h2v2_fancy_upsample` | 0x801C36F0 | 488 | matched |  |
| `libjpeg/jdsample` | `jinit_upsampler` | 0x801C38D8 | 636 | matched |  |
| `libjpeg/jerror` | `error_exit` | 0x801C3B54 | 72 | matched |  |
| `libjpeg/jerror` | `output_message` | 0x801C3B9C | 48 | matched |  |
| `libjpeg/jerror` | `emit_message` | 0x801C3BCC | 124 | matched |  |
| `libjpeg/jerror` | `format_message` | 0x801C3C48 | 288 | matched |  |
| `libjpeg/jerror` | `reset_error_mgr` | 0x801C3D68 | 24 | matched |  |
| `libjpeg/jerror` | `jpeg_std_error` | 0x801C3D80 | 112 | matched |  |
| `libjpeg/jidctflt` | `jpeg_idct_float` | 0x801C3DF0 | 1184 | matched |  |
| `libjpeg/jidctfst` | `jpeg_idct_ifast` | 0x801C4290 | 936 | matched |  |
| `libjpeg/jidctint` | `jpeg_idct_islow` | 0x801C4638 | 1092 | matched |  |
| `libjpeg/jidctred` | `jpeg_idct_4x4` | 0x801C4A7C | 732 | matched |  |
| `libjpeg/jidctred` | `jpeg_idct_2x2` | 0x801C4D58 | 508 | matched |  |
| `libjpeg/jidctred` | `jpeg_idct_1x1` | 0x801C4F54 | 48 | matched |  |
| `libjpeg/jmemmgr` | `alloc_small` | 0x801C4F84 | 456 | parked, 93.9% |  |
| `libjpeg/jmemmgr` | `alloc_large` | 0x801C514C | 292 | parked, 100% |  |
| `libjpeg/jmemmgr` | `alloc_sarray` | 0x801C5270 | 356 | parked, 100% |  |
| `libjpeg/jmemmgr` | `alloc_barray` | 0x801C53D4 | 368 | parked, 100% |  |
| `libjpeg/jmemmgr` | `request_virt_sarray` | 0x801C5544 | 164 | parked, 100% |  |
| `libjpeg/jmemmgr` | `request_virt_barray` | 0x801C55E8 | 164 | parked, 100% |  |
| `libjpeg/jmemmgr` | `realize_virt_arrays` | 0x801C568C | 556 | parked, 100% |  |
| `libjpeg/jmemmgr` | `do_sarray_io` | 0x801C58B8 | 264 | parked, 100% |  |
| `libjpeg/jmemmgr` | `do_barray_io` | 0x801C59C0 | 268 | parked, 100% |  |
| `libjpeg/jmemmgr` | `access_virt_sarray` | 0x801C5ACC | 512 | parked, 100% |  |
| `libjpeg/jmemmgr` | `access_virt_barray` | 0x801C5CCC | 516 | parked, 100% |  |
| `libjpeg/jmemmgr` | `free_pool` | 0x801C5ED0 | 392 | parked, 100% |  |
| `libjpeg/jmemmgr` | `self_destruct` | 0x801C6058 | 104 | parked, 100% |  |
| `libjpeg/jmemmgr` | `jinit_memory_mgr` | 0x801C60C0 | 436 | parked, 100% |  |
| `libjpeg/jmemnobs` | `jpeg_get_small` | 0x801C6274 | 8 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_free_small` | 0x801C627C | 8 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_get_large` | 0x801C6284 | 8 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_free_large` | 0x801C628C | 8 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_mem_available` | 0x801C6294 | 8 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_open_backing_store` | 0x801C629C | 28 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_mem_init` | 0x801C62B8 | 8 | matched |  |
| `libjpeg/jmemnobs` | `jpeg_mem_term` | 0x801C62C0 | 4 | matched |  |
| `libjpeg/jquant1` | `select_ncolors` | 0x801C62C4 | 664 | matched |  |
| `libjpeg/jquant1` | `create_colormap` | 0x801C655C | 572 | matched |  |
| `libjpeg/jquant1` | `create_colorindex` | 0x801C6798 | 876 | matched |  |
| `libjpeg/jquant1` | `create_odither_tables` | 0x801C6B04 | 456 | matched |  |
| `libjpeg/jquant1` | `color_quantize` | 0x801C6CCC | 388 | matched |  |
| `libjpeg/jquant1` | `color_quantize3` | 0x801C6E50 | 328 | matched |  |
| `libjpeg/jquant1` | `quantize_ord_dither` | 0x801C6F98 | 448 | matched |  |
| `libjpeg/jquant1` | `quantize3_ord_dither` | 0x801C7158 | 404 | matched |  |
| `libjpeg/jquant1` | `quantize_fs_dither` | 0x801C72EC | 624 | matched |  |
| `libjpeg/jquant1` | `start_pass_1_quant` | 0x801C755C | 420 | matched |  |
| `libjpeg/jquant1` | `finish_pass_1_quant` | 0x801C7700 | 4 | matched |  |
| `libjpeg/jquant1` | `new_color_map_1_quant` | 0x801C7704 | 28 | matched |  |
| `libjpeg/jquant1` | `jinit_1pass_quantizer` | 0x801C7720 | 356 | matched |  |
| `libjpeg/jquant2` | `prescan_quantize` | 0x801C7884 | 144 | matched |  |
| `libjpeg/jquant2` | `update_box` | 0x801C7914 | 1104 | matched |  |
| `libjpeg/jquant2` | `median_cut` | 0x801C7D64 | 504 | matched |  |
| `libjpeg/jquant2` | `compute_color` | 0x801C7F5C | 316 | matched |  |
| `libjpeg/jquant2` | `select_colors` | 0x801C8098 | 256 | matched |  |
| `libjpeg/jquant2` | `find_nearby_colors` | 0x801C8198 | 580 | matched |  |
| `libjpeg/jquant2` | `find_best_colors` | 0x801C83DC | 464 | matched |  |
| `libjpeg/jquant2` | `fill_inverse_cmap` | 0x801C85AC | 340 | matched |  |
| `libjpeg/jquant2` | `pass2_no_dither` | 0x801C8700 | 204 | matched |  |
| `libjpeg/jquant2` | `pass2_fs_dither` | 0x801C87CC | 784 | matched |  |
| `libjpeg/jquant2` | `init_error_limit` | 0x801C8ADC | 916 | matched |  |
| `libjpeg/jquant2` | `finish_pass1` | 0x801C8E70 | 64 | matched |  |
| `libjpeg/jquant2` | `finish_pass2` | 0x801C8EB0 | 4 | matched |  |
| `libjpeg/jquant2` | `start_pass_2_quant` | 0x801C8EB4 | 456 | matched |  |
| `libjpeg/jquant2` | `new_color_map_2_quant` | 0x801C907C | 16 | matched |  |
| `libjpeg/jquant2` | `jinit_2pass_quantizer` | 0x801C908C | 528 | matched |  |
| `libjpeg/jutils` | `jdiv_round_up` | 0x801C929C | 16 | matched |  |
| `libjpeg/jutils` | `jround_up` | 0x801C92AC | 28 | matched |  |
| `libjpeg/jutils` | `jcopy_sample_rows` | 0x801C92C8 | 124 | matched |  |
| `libjpeg/jutils` | `jcopy_block_row` | 0x801C9344 | 20 | matched |  |
| `libjpeg/jutils` | `jzero_far` | 0x801C9358 | 12 | matched |  |
| `libjpeg/jdatasrc` | `init_source` | 0x801C9364 | 16 | matched | unit name unconfirmed |
| `libjpeg/jdatasrc` | `fill_input_buffer` | 0x801C9374 | 212 | matched | unit name unconfirmed |
| `libjpeg/jdatasrc` | `skip_input_data` | 0x801C9448 | 256 | matched | unit name unconfirmed |
| `libjpeg/jdatasrc` | `term_source` | 0x801C9548 | 4 | matched | unit name unconfirmed |
| `libjpeg/jdatasrc` | `jpeg_stdio_src` | 0x801C954C | 216 | matched | unit name unconfirmed |
| `revolution/thp/THPDec` | `THPVideoDecode` | 0x80209D80 | 712 | matched |  |
| `revolution/thp/THPDec` | `__THPReadFrameHeader` | 0x8020A050 | 308 | matched |  |
| `revolution/thp/THPDec` | `__THPReadScaneHeader` | 0x8020A190 | 276 | matched |  |
| `revolution/thp/THPDec` | `__THPReadQuantizationTable` | 0x8020A2B0 | 924 | matched |  |
| `revolution/thp/THPDec` | `__THPReadHuffmanTableSpecification` | 0x8020A650 | 964 | matched |  |
| `revolution/thp/THPDec` | `__THPPrepBitStream` | 0x8020AA20 | 588 | matched |  |
| `revolution/thp/THPDec` | `__THPDecompressYUV` | 0x8020AC70 | 260 | matched |  |
| `revolution/thp/THPDec` | `__THPDecompressiMCURow512x448` | 0x8020AD80 | 588 | matched |  |
| `revolution/thp/THPDec` | `__THPInverseDCTNoYPos` | 0x8020AFD0 | 1164 | ORIGINAL-ASM |  |
| `revolution/thp/THPDec` | `__THPInverseDCTY8` | 0x8020B460 | 1172 | ORIGINAL-ASM |  |
| `revolution/thp/THPDec` | `__THPDecompressiMCURow640x480` | 0x8020B900 | 592 | matched |  |
| `revolution/thp/THPDec` | `__THPDecompressiMCURowNxN` | 0x8020BB50 | 604 | matched |  |
| `revolution/thp/THPDec` | `__THPHuffDecodeDCTCompY` | 0x8020BDB0 | 1652 | ORIGINAL-ASM |  |
| `revolution/thp/THPDec` | `__THPHuffDecodeDCTCompU` | 0x8020C430 | 1688 | ORIGINAL-ASM |  |
| `revolution/thp/THPDec` | `__THPHuffDecodeDCTCompV` | 0x8020CAD0 | 1688 | ORIGINAL-ASM |  |
| `revolution/thp/THPDec` | `THPInit` | 0x8020D170 | 156 | matched |  |
| `revolution/thp/THPAudio` | `THPAudioDecode` | 0x8020D210 | 1384 | matched |  |
| `revolution/exi/EXIBios` | `SetExiInterruptMask` | 0x8020D780 | 232 | matched |  |
| `revolution/exi/EXIBios` | `EXIImm` | 0x8020D870 | 636 | matched |  |
| `revolution/exi/EXIBios` | `EXIImmEx` | 0x8020DAF0 | 164 | matched |  |
| `revolution/exi/EXIBios` | `EXIDma` | 0x8020DBA0 | 248 | matched |  |
| `revolution/exi/EXIBios` | `EXISync` | 0x8020DCA0 | 636 | matched |  |
| `revolution/exi/EXIBios` | `EXISetExiCallback` | 0x8020DF20 | 132 | matched |  |
| `revolution/exi/EXIBios` | `__EXIProbe` | 0x8020DFB0 | 384 | matched |  |
| `revolution/exi/EXIBios` | `EXIAttach` | 0x8020E130 | 280 | matched |  |
| `revolution/exi/EXIBios` | `EXIDetach` | 0x8020E250 | 184 | matched |  |
| `revolution/exi/EXIBios` | `EXISelect` | 0x8020E310 | 300 | matched |  |
| `revolution/exi/EXIBios` | `EXIDeselect` | 0x8020E440 | 252 | matched |  |
| `revolution/exi/EXIBios` | `EXIIntrruptHandler` | 0x8020E540 | 184 | matched |  |
| `revolution/exi/EXIBios` | `TCIntrruptHandler` | 0x8020E600 | 536 | matched |  |
| `revolution/exi/EXIBios` | `EXTIntrruptHandler` | 0x8020E820 | 208 | matched |  |
| `revolution/exi/EXIBios` | `EXIInit` | 0x8020E8F0 | 452 | matched |  |
| `revolution/exi/EXIBios` | `EXILock` | 0x8020EAC0 | 252 | matched |  |
| `revolution/exi/EXIBios` | `EXIUnlock` | 0x8020EBC0 | 216 | matched |  |
| `revolution/exi/EXIBios` | `UnlockedHandler` | 0x8020ECA0 | 40 | matched |  |
| `revolution/exi/EXIBios` | `EXIGetID` | 0x8020ECD0 | 936 | matched |  |
| `revolution/exi/EXIUart` | `ProbeBarnacle` | 0x8020F080 | 376 | matched |  |
| `revolution/exi/EXIUart` | `__OSEnableBarnacle` | 0x8020F200 | 444 | matched |  |
| `revolution/exi/EXICommon` | `EXIWriteReg` | 0x8020F3C0 | 392 | matched |  |
| `revolution/si/SIBios` | `CompleteTransfer` | 0x8020F550 | 756 | matched |  |
| `revolution/si/SIBios` | `SIInterruptHandler` | 0x8020F850 | 1004 | matched |  |
| `revolution/si/SIBios` | `SIInit` | 0x8020FC40 | 192 | matched |  |
| `revolution/si/SIBios` | `__SITransfer` | 0x8020FD00 | 432 | matched |  |
| `revolution/si/SIBios` | `SISetXY` | 0x8020FEB0 | 92 | matched |  |
| `revolution/si/SIBios` | `AlarmHandler` | 0x8020FF10 | 140 | matched |  |
| `revolution/si/SIBios` | `SITransfer` | 0x8020FFA0 | 364 | matched |  |
| `revolution/si/SIBios` | `GetTypeCallback` | 0x80210110 | 692 | matched |  |
| `revolution/si/SIBios` | `SIGetType` | 0x802103D0 | 436 | matched |  |
| `revolution/si/SISamplingRate` | `SISetSamplingRate` | 0x80210590 | 212 | matched |  |
| `revolution/si/SISamplingRate` | `SIRefreshSamplingRate` | 0x80210670 | 8 | matched |  |
| `revolution/base/PPCArch` | `PPCMfmsr` | 0x80210680 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtmsr` | 0x80210690 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMfhid0` | 0x802106A0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMthid0` | 0x802106B0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMfl2cr` | 0x802106C0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtl2cr` | 0x802106D0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtdec` | 0x802106E0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCSync` | 0x802106F0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCHalt` | 0x80210700 | 20 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtmmcr0` | 0x80210720 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtmmcr1` | 0x80210730 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtpmc1` | 0x80210740 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtpmc2` | 0x80210750 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtpmc3` | 0x80210760 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtpmc4` | 0x80210770 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMffpscr` | 0x80210780 | 32 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtfpscr` | 0x802107A0 | 40 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMfhid2` | 0x802107D0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMthid2` | 0x802107E0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMtwpar` | 0x802107F0 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCDisableSpeculation` | 0x80210800 | 40 | matched |  |
| `revolution/base/PPCArch` | `PPCSetFpNonIEEEMode` | 0x80210830 | 8 | ORIGINAL-ASM |  |
| `revolution/base/PPCArch` | `PPCMthid4` | 0x80210840 | 80 | matched |  |
| `revolution/os/OS` | `__OSFPRInit` | 0x80210890 | 296 | ORIGINAL-ASM |  |
| `revolution/os/OS` | `__OSGetIOSRev` | 0x802109C0 | 108 | matched |  |
| `revolution/os/OS` | `OSGetConsoleType` | 0x80210A30 | 596 | matched |  |
| `revolution/os/OS` | `ClearArena` | 0x80210C90 | 476 | matched |  |
| `revolution/os/OS` | `ClearMEM2Arena` | 0x80210E70 | 484 | matched |  |
| `revolution/os/OS` | `InquiryCallback` | 0x80211060 | 60 | matched |  |
| `revolution/os/OS` | `ReportOSInfo` | 0x802110A0 | 676 | matched |  |
| `revolution/os/OS` | `OSInit` | 0x80211350 | 1364 | matched |  |
| `revolution/os/OS` | `OSExceptionInit` | 0x802118B0 | 640 | matched |  |
| `revolution/os/OS` | `__OSDBIntegrator` | 0x80211B30 | 36 | ORIGINAL-ASM |  |
| `revolution/os/OS` | `__OSDBJump` | 0x80211B60 | 4 | ORIGINAL-ASM |  |
| `revolution/os/OS` | `__OSSetExceptionHandler` | 0x80211B70 | 20 | matched |  |
| `revolution/os/OS` | `__OSGetExceptionHandler` | 0x80211B90 | 16 | matched |  |
| `revolution/os/OS` | `OSExceptionVector` | 0x80211BA0 | 156 | ORIGINAL-ASM |  |
| `revolution/os/OS` | `OSDefaultExceptionHandler` | 0x80211C40 | 88 | ORIGINAL-ASM |  |
| `revolution/os/OS` | `__OSPSInit` | 0x80211CA0 | 84 | matched |  |
| `revolution/os/OS` | `__OSGetDIConfig` | 0x80211D00 | 16 | matched |  |
| `revolution/os/OS` | `OSRegisterVersion` | 0x80211D10 | 16 | matched |  |
| `revolution/os/OS` | `OSGetAppGamename` | 0x80211D20 | 132 | matched |  |
| `revolution/os/OS` | `OSGetAppType` | 0x80211DB0 | 32 | matched |  |
| `revolution/os/OSAlarm` | `__OSInitAlarm` | 0x80211DD0 | 88 | matched |  |
| `revolution/os/OSAlarm` | `OSCreateAlarm` | 0x80211E30 | 16 | matched |  |
| `revolution/os/OSAlarm` | `InsertAlarm` | 0x80211E40 | 592 | matched |  |
| `revolution/os/OSAlarm` | `OSSetAlarm` | 0x80212090 | 112 | matched |  |
| `revolution/os/OSAlarm` | `OSSetPeriodicAlarm` | 0x80212100 | 132 | matched |  |
| `revolution/os/OSAlarm` | `OSCancelAlarm` | 0x80212190 | 280 | matched |  |
| `revolution/os/OSAlarm` | `DecrementerExceptionCallback` | 0x802122B0 | 556 | matched |  |
| `revolution/os/OSAlarm` | `DecrementerExceptionHandler` | 0x802124E0 | 80 | ORIGINAL-ASM |  |
| `revolution/os/OSAlarm` | `OnReset` | 0x80212530 | 140 | matched |  |
| `revolution/os/OSAlarm` | `OSSetAlarmUserData` | 0x802125C0 | 8 | matched |  |
| `revolution/os/OSAlarm` | `OSGetAlarmUserData` | 0x802125D0 | 8 | matched |  |
| `revolution/os/OSAlarm` | `__OSSetInternalAlarmUserData` | 0x802125E0 | 16 | matched |  |
| `revolution/os/OSAlarm` | `__OSCancelInternalAlarms` | 0x802125F0 | 168 | matched | name from zeldaret/tp |
| `revolution/os/OSAlloc` | `DLInsert` | 0x802126A0 | 176 | matched |  |
| `revolution/os/OSAlloc` | `OSAllocFromHeap` | 0x80212750 | 248 | matched |  |
| `revolution/os/OSAlloc` | `OSFreeToHeap` | 0x80212850 | 120 | matched |  |
| `revolution/os/OSAlloc` | `OSSetCurrentHeap` | 0x802128D0 | 16 | matched |  |
| `revolution/os/OSAlloc` | `OSInitAlloc` | 0x802128E0 | 112 | matched |  |
| `revolution/os/OSAlloc` | `OSCreateHeap` | 0x80212950 | 112 | matched |  |
| `revolution/os/OSArena` | `OSGetMEM1ArenaHi` | 0x802129C0 | 8 | matched |  |
| `revolution/os/OSArena` | `OSGetMEM2ArenaHi` | 0x802129D0 | 8 | matched |  |
| `revolution/os/OSArena` | `OSGetArenaHi` | 0x802129E0 | 8 | matched |  |
| `revolution/os/OSArena` | `OSGetMEM1ArenaLo` | 0x802129F0 | 8 | matched |  |
| `revolution/os/OSArena` | `OSGetMEM2ArenaLo` | 0x80212A00 | 8 | matched |  |
| `revolution/os/OSArena` | `OSGetArenaLo` | 0x80212A10 | 8 | matched |  |
| `revolution/os/OSArena` | `OSSetMEM1ArenaHi` | 0x80212A20 | 8 | matched |  |
| `revolution/os/OSArena` | `OSSetMEM2ArenaHi` | 0x80212A30 | 8 | matched |  |
| `revolution/os/OSArena` | `OSSetArenaHi` | 0x80212A40 | 8 | matched |  |
| `revolution/os/OSArena` | `OSSetMEM1ArenaLo` | 0x80212A50 | 8 | matched |  |
| `revolution/os/OSArena` | `OSSetMEM2ArenaLo` | 0x80212A60 | 8 | matched |  |
| `revolution/os/OSArena` | `OSSetArenaLo` | 0x80212A70 | 8 | matched |  |
| `revolution/os/OSArena` | `OSAllocFromMEM1ArenaLo` | 0x80212A80 | 52 | matched |  |
| `revolution/os/OSAudioSystem` | `__AIClockInit` | 0x80212AC0 | 544 | matched |  |
| `revolution/os/OSAudioSystem` | `__OSInitAudioSystem` | 0x80212CE0 | 464 | matched |  |
| `revolution/os/OSAudioSystem` | `__OSStopAudioSystem` | 0x80212EB0 | 208 | matched |  |
| `revolution/os/OSCache` | `DCEnable` | 0x80212F80 | 20 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `DCInvalidateRange` | 0x80212FA0 | 44 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `DCFlushRange` | 0x80212FD0 | 48 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `DCStoreRange` | 0x80213000 | 48 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `DCFlushRangeNoSync` | 0x80213030 | 44 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `DCZeroRange` | 0x80213060 | 44 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `ICInvalidateRange` | 0x80213090 | 52 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `ICFlashInvalidate` | 0x802130D0 | 16 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `ICEnable` | 0x802130E0 | 20 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `__LCEnable` | 0x80213100 | 204 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `LCEnable` | 0x802131D0 | 56 | matched |  |
| `revolution/os/OSCache` | `LCDisable` | 0x80213210 | 40 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `LCStoreBlocks` | 0x80213240 | 36 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `LCStore` | 0x80213270 | 160 | matched |  |
| `revolution/os/OSCache` | `LCQueueWait` | 0x80213310 | 20 | ORIGINAL-ASM |  |
| `revolution/os/OSCache` | `DMAErrorHandler` | 0x80213330 | 328 | matched |  |
| `revolution/os/OSCache` | `__OSCacheInit` | 0x80213480 | 312 | matched |  |
| `revolution/os/OSContext` | `__OSLoadFPUContext` | 0x802135C0 | 292 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `__OSSaveFPUContext` | 0x802136F0 | 296 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSSaveFPUContext` | 0x80213820 | 8 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSSetCurrentContext` | 0x80213830 | 92 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSGetCurrentContext` | 0x80213890 | 12 | matched |  |
| `revolution/os/OSContext` | `OSSaveContext` | 0x802138A0 | 128 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSLoadContext` | 0x80213920 | 216 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSGetStackPointer` | 0x80213A00 | 8 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSSwitchFiber` | 0x80213A10 | 48 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSSwitchFiberEx` | 0x80213A40 | 48 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSClearContext` | 0x80213A70 | 36 | matched |  |
| `revolution/os/OSContext` | `OSInitContext` | 0x80213AA0 | 188 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `OSDumpContext` | 0x80213B60 | 608 | matched |  |
| `revolution/os/OSContext` | `OSSwitchFPUContext` | 0x80213DC0 | 132 | ORIGINAL-ASM |  |
| `revolution/os/OSContext` | `__OSContextInit` | 0x80213E50 | 72 | matched |  |
| `revolution/os/OSError` | `OSReport` | 0x80213EA0 | 128 | matched |  |
| `revolution/os/OSError` | `OSPanic` | 0x80213F20 | 300 | matched |  |
| `revolution/os/OSError` | `OSSetErrorHandler` | 0x80214050 | 652 | matched |  |
| `revolution/os/OSError` | `__OSUnhandledException` | 0x802142E0 | 764 | matched |  |
| `revolution/os/OSExec` | `PackArgs` | 0x802145E0 | 384 | matched |  |
| `revolution/os/OSExec` | `Utf16ToArg` | 0x80214760 | 344 | matched |  |
| `revolution/os/OSExec` | `PackInstallerArgs` | 0x802148C0 | 460 | matched |  |
| `revolution/os/OSExec` | `Run` | 0x80214A90 | 60 | ORIGINAL-ASM |  |
| `revolution/os/OSExec` | `Callback` | 0x80214AD0 | 12 | matched |  |
| `revolution/os/OSExec` | `__OSGetExecParams` | 0x80214AE0 | 36 | matched |  |
| `revolution/os/OSExec` | `callback` | 0x80214B10 | 8 | matched |  |
| `revolution/os/OSExec` | `__OSLaunchNextFirmware` | 0x80214B20 | 1668 | matched |  |
| `revolution/os/OSExec` | `__OSLaunchMenu` | 0x802151B0 | 200 | matched |  |
| `revolution/os/OSExec` | `__OSBootDolSimple` | 0x80215280 | 1876 | matched |  |
| `revolution/os/OSExec` | `__OSBootDol` | 0x802159E0 | 480 | matched |  |
| `revolution/os/OSFatal` | `ScreenReport` | 0x80215BC0 | 808 | matched |  |
| `revolution/os/OSFatal` | `ConfigureVideo` | 0x80215EF0 | 288 | matched |  |
| `revolution/os/OSFatal` | `OSFatal` | 0x80216010 | 488 | matched |  |
| `revolution/os/OSFatal` | `Halt` | 0x80216200 | 1512 | matched |  |
| `revolution/os/OSFont` | `GetFontCode` | 0x802167F0 | 312 | matched |  |
| `revolution/os/OSFont` | `Decode` | 0x80216930 | 424 | matched |  |
| `revolution/os/OSFont` | `OSSetFontEncode` | 0x80216AE0 | 120 | matched |  |
| `revolution/os/OSFont` | `ReadFont` | 0x80216B60 | 784 | matched |  |
| `revolution/os/OSFont` | `OSLoadFont` | 0x80216E70 | 280 | matched |  |
| `revolution/os/OSFont` | `ParseStringS` | 0x80216F90 | 248 | matched |  |
| `revolution/os/OSFont` | `ParseStringW` | 0x80217090 | 436 | matched |  |
| `revolution/os/OSFont` | `OSGetFontTexel` | 0x80217250 | 648 | matched |  |
| `revolution/os/OSInterrupt` | `OSDisableInterrupts` | 0x802174E0 | 20 | ORIGINAL-ASM |  |
| `revolution/os/OSInterrupt` | `OSEnableInterrupts` | 0x80217500 | 20 | ORIGINAL-ASM |  |
| `revolution/os/OSInterrupt` | `OSRestoreInterrupts` | 0x80217520 | 36 | ORIGINAL-ASM |  |
| `revolution/os/OSInterrupt` | `__OSSetInterruptHandler` | 0x80217550 | 20 | matched |  |
| `revolution/os/OSInterrupt` | `__OSGetInterruptHandler` | 0x80217570 | 16 | matched | SDK name from behaviour and order |
| `revolution/os/OSInterrupt` | `__OSInterruptInit` | 0x80217580 | 196 | matched |  |
| `revolution/os/OSInterrupt` | `SetInterruptMask` | 0x80217650 | 604 | matched |  |
| `revolution/os/OSInterrupt` | `__OSMaskInterrupts` | 0x802178B0 | 128 | matched |  |
| `revolution/os/OSInterrupt` | `__OSUnmaskInterrupts` | 0x80217930 | 128 | matched |  |
| `revolution/os/OSInterrupt` | `__OSDispatchInterrupt` | 0x802179B0 | 680 | matched | SDK name from behaviour and order |
| `revolution/os/OSInterrupt` | `ExternalInterruptHandler` | 0x80217C60 | 80 | ORIGINAL-ASM |  |
| `revolution/os/OSLink` | `__OSModuleInit` | 0x80217CB0 | 24 | matched |  |
| `revolution/os/OSMessage` | `OSInitMessageQueue` | 0x80217CD0 | 96 | matched | SDK name from behaviour and order |
| `revolution/os/OSMessage` | `OSSendMessage` | 0x80217D30 | 200 | matched | SDK name from behaviour and order |
| `revolution/os/OSMessage` | `OSReceiveMessage` | 0x80217E00 | 220 | matched | SDK name from behaviour and order |
| `revolution/os/OSMessage` | `OSJamMessage` | 0x80217EE0 | 208 | matched | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `OSGetPhysicalMem2Size` | 0x80217FB0 | 12 | matched |  |
| `revolution/os/OSMemory` | `OSGetConsoleSimulatedMem1Size` | 0x80217FC0 | 12 | matched |  |
| `revolution/os/OSMemory` | `OSGetConsoleSimulatedMem2Size` | 0x80217FD0 | 12 | matched |  |
| `revolution/os/OSMemory` | `OnShutdown` | 0x80217FE0 | 60 | matched | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `MEMIntrruptHandler` | 0x80218020 | 72 | matched |  |
| `revolution/os/OSMemory` | `ConfigMEM1_24MB` | 0x80218070 | 128 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `ConfigMEM2_52MB` | 0x802180F0 | 212 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `ConfigMEM2_56MB` | 0x802181D0 | 212 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `ConfigMEM2_64MB` | 0x802182B0 | 168 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `ConfigMEM2_112MB` | 0x80218360 | 212 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `ConfigMEM2_128MB` | 0x80218440 | 168 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `RealMode` | 0x802184F0 | 24 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSMemory` | `BATConfig` | 0x80218510 | 288 | matched |  |
| `revolution/os/OSMemory` | `__OSRestoreCodeExecOnMEM1` | 0x80218630 | 68 | matched |  |
| `revolution/os/OSMemory` | `__OSInitMemoryProtection` | 0x80218680 | 204 | matched |  |
| `revolution/os/OSMutex` | `OSInitMutex` | 0x80218750 | 56 | matched | SDK name from behaviour and order |
| `revolution/os/OSMutex` | `OSLockMutex` | 0x80218790 | 220 | matched | SDK name from behaviour and order |
| `revolution/os/OSMutex` | `OSUnlockMutex` | 0x80218870 | 200 | matched | SDK name from behaviour and order |
| `revolution/os/OSMutex` | `__OSUnlockAllMutex` | 0x80218940 | 108 | matched | SDK name from behaviour and order |
| `revolution/os/OSMutex` | `__OSCheckMutex` | 0x802189B0 | 252 | matched | SDK name from behaviour and order |
| `revolution/os/OSMutex` | `__OSCheckDeadLock` | 0x80218AB0 | 56 | matched | SDK name from behaviour and order |
| `revolution/os/OSMutex` | `__OSCheckMutexes` | 0x80218AF0 | 116 | matched | SDK name from behaviour and order |
| `revolution/os/OSReboot` | `__OSReboot` | 0x80218B70 | 108 | matched | SDK name from behaviour and order |
| `revolution/os/OSReboot` | `__OSGetSaveRegion` | 0x80218BE0 | 20 | matched | SDK name from behaviour and order |
| `revolution/os/OSReset` | `OSRegisterShutdownFunction` | 0x80218C00 | 136 | matched |  |
| `revolution/os/OSReset` | `__OSCallShutdownFunctions` | 0x80218C90 | 176 | matched |  |
| `revolution/os/OSReset` | `__OSShutdownDevices` | 0x80218D40 | 384 | matched |  |
| `revolution/os/OSReset` | `OSShutdownSystem` | 0x80218EC0 | 284 | matched |  |
| `revolution/os/OSReset` | `OSRestart` | 0x80218FE0 | 212 | matched |  |
| `revolution/os/OSReset` | `__OSReturnToMenu` | 0x802190C0 | 648 | matched |  |
| `revolution/os/OSReset` | `OSReturnToMenu` | 0x80219350 | 64 | matched |  |
| `revolution/os/OSReset` | `OSReturnToDataManager` | 0x80219390 | 64 | matched |  |
| `revolution/os/OSReset` | `__OSReturnToMenuForError` | 0x802193D0 | 164 | matched |  |
| `revolution/os/OSReset` | `__OSHotResetForError` | 0x80219480 | 88 | matched |  |
| `revolution/os/OSReset` | `OSGetResetCode` | 0x802194E0 | 48 | matched |  |
| `revolution/os/OSReset` | `OSResetSystem` | 0x80219510 | 28 | matched |  |
| `revolution/os/OSRtc` | `WriteSramCallback` | 0x80219530 | 312 | matched |  |
| `revolution/os/OSRtc` | `__OSInitSram` | 0x80219670 | 512 | matched |  |
| `revolution/os/OSRtc` | `UnlockSram` | 0x80219870 | 736 | matched |  |
| `revolution/os/OSRtc` | `__OSSyncSram` | 0x80219B50 | 16 | matched |  |
| `revolution/os/OSRtc` | `__OSReadROM` | 0x80219B60 | 292 | matched |  |
| `revolution/os/OSRtc` | `OSGetWirelessID` | 0x80219C90 | 120 | matched |  |
| `revolution/os/OSRtc` | `OSSetWirelessID` | 0x80219D10 | 156 | matched |  |
| `revolution/os/OSRtc` | `__OSGetRTCFlags` | 0x80219DB0 | 284 | matched |  |
| `revolution/os/OSRtc` | `__OSClearRTCFlags` | 0x80219ED0 | 272 | matched |  |
| `revolution/os/OSSemaphore` | `OSInitSemaphore` | 0x80219FE0 | 88 | matched | SDK name from behaviour and order |
| `revolution/os/OSSemaphore` | `OSWaitSemaphore` | 0x8021A040 | 112 | matched | SDK name from behaviour and order |
| `revolution/os/OSSemaphore` | `OSSignalSemaphore` | 0x8021A0B0 | 96 | matched | SDK name from behaviour and order |
| `revolution/os/OSSemaphore` | `OSGetSemaphoreCount` | 0x8021A110 | 8 | matched | SDK name from behaviour and order |
| `revolution/os/OSSync` | `SystemCallVector` | 0x8021A120 | 32 | ORIGINAL-ASM | SDK name from behaviour and order |
| `revolution/os/OSSync` | `__OSInitSystemCall` | 0x8021A140 | 96 | matched |  |
| `revolution/os/OSThread` | `DefaultSwitchThreadCallback` | 0x8021A1A0 | 4 | matched |  |
| `revolution/os/OSThread` | `__OSThreadInit` | 0x8021A1B0 | 644 | matched |  |
| `revolution/os/OSThread` | `OSInitThreadQueue` | 0x8021A440 | 16 | matched |  |
| `revolution/os/OSThread` | `OSGetCurrentThread` | 0x8021A450 | 12 | matched |  |
| `revolution/os/OSThread` | `OSDisableScheduler` | 0x8021A460 | 60 | matched |  |
| `revolution/os/OSThread` | `OSEnableScheduler` | 0x8021A4A0 | 60 | matched |  |
| `revolution/os/OSThread` | `UnsetRun` | 0x8021A4E0 | 104 | matched |  |
| `revolution/os/OSThread` | `__OSGetEffectivePriority` | 0x8021A550 | 64 | matched |  |
| `revolution/os/OSThread` | `SetEffectivePriority` | 0x8021A590 | 424 | matched |  |
| `revolution/os/OSThread` | `__OSPromoteThread` | 0x8021A740 | 80 | matched |  |
| `revolution/os/OSThread` | `SelectThread` | 0x8021A790 | 556 | matched |  |
| `revolution/os/OSThread` | `__OSReschedule` | 0x8021A9C0 | 24 | matched |  |
| `revolution/os/OSThread` | `OSYieldThread` | 0x8021A9E0 | 60 | matched |  |
| `revolution/os/OSThread` | `OSCreateThread` | 0x8021AA20 | 620 | matched |  |
| `revolution/os/OSThread` | `OSExitThread` | 0x8021AC90 | 228 | matched |  |
| `revolution/os/OSThread` | `OSCancelThread` | 0x8021AD80 | 468 | matched |  |
| `revolution/os/OSThread` | `OSJoinThread` | 0x8021AF60 | 324 | matched |  |
| `revolution/os/OSThread` | `OSResumeThread` | 0x8021B0B0 | 668 | matched |  |
| `revolution/os/OSThread` | `OSSuspendThread` | 0x8021B350 | 396 | matched |  |
| `revolution/os/OSThread` | `OSSleepThread` | 0x8021B4E0 | 240 | matched |  |
| `revolution/os/OSThread` | `OSWakeupThread` | 0x8021B5D0 | 248 | matched |  |
| `revolution/os/OSThread` | `CheckThreadQueue` | 0x8021B6D0 | 156 | matched |  |
| `revolution/os/OSThread` | `OSCheckActiveThreads` | 0x8021B770 | 1876 | matched |  |
| `revolution/os/OSThread` | `SleepAlarmHandler` | 0x8021BED0 | 128 | matched |  |
| `revolution/os/OSThread` | `OSSleepTicks` | 0x8021BF50 | 168 | matched |  |
| `revolution/os/OSTime` | `OSGetTime` | 0x8021C000 | 24 | ORIGINAL-ASM |  |
| `revolution/os/OSTime` | `OSGetTick` | 0x8021C020 | 8 | ORIGINAL-ASM |  |
| `revolution/os/OSTime` | `__OSGetSystemTime` | 0x8021C030 | 100 | matched |  |
| `revolution/os/OSTime` | `__OSTimeToSystemTime` | 0x8021C0A0 | 88 | matched |  |
| `revolution/os/OSTime` | `OSTicksToCalendarTime` | 0x8021C100 | 812 | matched |  |
| `revolution/os/OSUtf` | `OSUTF8to32` | 0x8021C430 | 276 | matched |  |
| `revolution/os/OSUtf` | `OSUTF16to32` | 0x8021C550 | 112 | matched |  |
| `revolution/os/OSUtf` | `OSUTF32toANSI` | 0x8021C5C0 | 124 | matched |  |
| `revolution/os/OSUtf` | `OSUTF32toSJIS` | 0x8021C640 | 64 | matched |  |
| `revolution/os/OSIpc` | `__OSGetIPCBufferHi` | 0x8021C680 | 8 | matched |  |
| `revolution/os/OSIpc` | `__OSGetIPCBufferLo` | 0x8021C690 | 8 | matched |  |
| `revolution/os/OSIpc` | `__OSInitIPCBuffer` | 0x8021C6A0 | 24 | matched |  |
| `revolution/os/OSStateTM` | `OSSetResetCallback` | 0x8021C6C0 | 236 | matched |  |
| `revolution/os/OSStateTM` | `OSSetPowerCallback` | 0x8021C7B0 | 236 | matched |  |
| `revolution/os/OSStateTM` | `__OSInitSTM` | 0x8021C8A0 | 280 | matched |  |
| `revolution/os/OSStateTM` | `__OSShutdownToSBY` | 0x8021C9C0 | 124 | matched |  |
| `revolution/os/OSStateTM` | `__OSHotReset` | 0x8021CA40 | 116 | matched |  |
| `revolution/os/OSStateTM` | `__OSSetVIForceDimming` | 0x8021CAC0 | 248 | matched |  |
| `revolution/os/OSStateTM` | `__OSUnRegisterStateEvent` | 0x8021CBC0 | 120 | matched |  |
| `revolution/os/OSStateTM` | `__OSVIDimReplyHandler` | 0x8021CC40 | 16 | matched |  |
| `revolution/os/OSStateTM` | `__OSDefaultResetCallback` | 0x8021CC50 | 4 | matched |  |
| `revolution/os/OSStateTM` | `__OSDefaultPowerCallback` | 0x8021CC60 | 4 | matched |  |
| `revolution/os/OSStateTM` | `__OSStateEventHandler` | 0x8021CC70 | 340 | matched |  |
| `revolution/os/OSPlayRecord` | `PlayRecordAlarmCallback` | 0x8021CDD0 | 12 | MATCH-PENDING (R17: RecordCheckSum reshaped to steer .bss order) |  |
| `revolution/os/OSPlayRecord` | `PlayRecordCallback` | 0x8021CDE0 | 1204 | MATCH-PENDING (R17: RecordCheckSum reshaped to steer .bss order) |  |
| `revolution/os/OSPlayRecord` | `__OSStartPlayRecord` | 0x8021D2A0 | 84 | MATCH-PENDING (R17: RecordCheckSum reshaped to steer .bss order) |  |
| `revolution/os/OSPlayRecord` | `__OSStopPlayRecord` | 0x8021D300 | 492 | MATCH-PENDING (R17: RecordCheckSum reshaped to steer .bss order) |  |
| `revolution/os/OSStateFlags` | `__OSWriteStateFlags` | 0x8021D4F0 | 228 | MATCH-PENDING (R17) |  |
| `revolution/os/OSStateFlags` | `__OSReadStateFlags` | 0x8021D5E0 | 296 | matched |  |
| `revolution/os/OSNet` | `__OSInitNet` | 0x8021D710 | 180 | parked, 100% |  |
| `revolution/os/OSNet` | `NWC24iPrepareShutdown` | 0x8021D7D0 | 184 | parked, 100% |  |
| `revolution/os/OSNet` | `NWC24iSynchronizeRtcCounter` | 0x8021D890 | 148 | parked, 100% |  |
| `revolution/os/OSNet` | `NWC24SuspendScheduler` | 0x8021D930 | 256 | parked, 100% |  |
| `revolution/os/OSNet` | `NWC24iRequestShutdown` | 0x8021DA30 | 108 | parked, 100% |  |
| `revolution/os/OSNet` | `NWC24Shutdown_` | 0x8021DAA0 | 184 | parked, 100% |  |
| `revolution/os/OSNet` | `NWC24iSetRtcCounter_` | 0x8021DB60 | 292 | parked, 100% |  |
| `revolution/os/OSNet` | `CallbackAsyncIpc` | 0x8021DC90 | 28 | parked, 100% |  |
| `revolution/os/OSNandbootInfo` | `__OSCreateNandbootInfo` | 0x8021DCB0 | 176 | matched |  |
| `revolution/os/OSNandbootInfo` | `__OSWriteNandbootInfo` | 0x8021DD60 | 344 | matched |  |
| `revolution/os/OSPlayTime` | `OSPlayTimeIsLimited` | 0x8021DEC0 | 24 | matched |  |
| `revolution/os/OSPlayTime` | `__OSPlayTimeFadeLastAIDCallback` | 0x8021DEE0 | 444 | matched |  |
| `revolution/os/OSPlayTime` | `__OSWriteExpiredFlag` | 0x8021E0A0 | 276 | matched |  |
| `revolution/os/OSPlayTime` | `__OSWriteExpiredFlagIfSet` | 0x8021E1C0 | 24 | matched |  |
| `revolution/os/OSPlayTime` | `__OSPlayTimeRebootThread` | 0x8021E1E0 | 236 | matched |  |
| `revolution/os/OSPlayTime` | `__OSPlayTimeAlarmExpired` | 0x8021E2D0 | 160 | matched |  |
| `revolution/os/OSPlayTime` | `__OSGetPlayTime` | 0x8021E370 | 460 | matched |  |
| `revolution/os/OSPlayTime` | `__OSInitPlayTime` | 0x8021E540 | 344 | matched |  |
| `revolution/os/OSLaunch` | `__OSRelaunchTitle` | 0x8021E6A0 | 516 | matched |  |
| `revolution/os/__start` | `__check_pad3` | 0x800062C0 | 40 | matched |  |
| `revolution/os/__start` | `__set_debug_bba` | 0x800062F0 | 12 | matched |  |
| `revolution/os/__start` | `__get_debug_bba` | 0x80006300 | 8 | matched |  |
| `revolution/os/__start` | `__start` | 0x80006310 | 364 | ORIGINAL-ASM |  |
| `revolution/os/__start` | `__init_registers` | 0x80006480 | 144 | ORIGINAL-ASM |  |
| `revolution/os/__start` | `__init_data` | 0x80006510 | 168 | matched |  |
| `revolution/os/__ppc_eabi_init` | `__init_user` | 0x8021E8B0 | 32 | ORIGINAL-ASM |  |
| `revolution/os/__ppc_eabi_init` | `__init_hardware` | 0x800065C0 | 36 | ORIGINAL-ASM |  |
| `revolution/os/__ppc_eabi_init` | `__init_cpp` | 0x8021E8D0 | 72 | matched |  |
| `revolution/os/__ppc_eabi_init` | `__flush_cache` | 0x800065F0 | 52 | ORIGINAL-ASM |  |
| `revolution/os/__ppc_eabi_init` | `exit` | 0x8021E920 | 76 | matched |  |
| `revolution/db/db` | `DBInit` | 0x8021E970 | 40 | matched |  |
| `revolution/db/db` | `__DBExceptionDestinationAux` | 0x8021E9A0 | 72 | matched |  |
| `revolution/db/db` | `__DBExceptionDestination` | 0x8021E9F0 | 16 | ORIGINAL-ASM |  |
| `revolution/db/db` | `__DBIsExceptionMarked` | 0x8021EA00 | 24 | matched |  |
| `revolution/db/db` | `DBPrintf` | 0x8021EA20 | 80 | matched |  |
| `revolution/dvd/dvdfs` | `__DVDFSInit` | 0x8021EFA0 | 48 | matched |  |
| `revolution/dvd/dvdfs` | `DVDConvertPathToEntrynum` | 0x8021EFD0 | 776 | matched |  |
| `revolution/dvd/dvdfs` | `DVDFastOpen` | 0x8021F2E0 | 104 | matched |  |
| `revolution/dvd/dvdfs` | `DVDOpen` | 0x8021F350 | 692 | matched |  |
| `revolution/dvd/dvdfs` | `DVDClose` | 0x8021F610 | 36 | matched |  |
| `revolution/dvd/dvdfs` | `entryToPath` | 0x8021F640 | 456 | matched |  |
| `revolution/dvd/dvdfs` | `DVDReadAsyncPrio` | 0x8021F810 | 208 | matched |  |
| `revolution/dvd/dvdfs` | `cbForReadAsync` | 0x8021F8E0 | 24 | matched |  |
| `revolution/dvd/dvd` | `StampCommand` | 0x8021F900 | 180 | matched | renamed from `fn_8021F900`: tp name by order and size |
| `revolution/dvd/dvd` | `defaultOptionalCommandChecker` | 0x8021F9C0 | 4 | matched | renamed from `fn_8021F9C0`: tp name by order and size |
| `revolution/dvd/dvd` | `DVDInit` | 0x8021F9D0 | 340 | matched |  |
| `revolution/dvd/dvd` | `stateReadingFST` | 0x8021FB30 | 376 | matched | renamed from `fn_8021FB30`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateReadingFST` | 0x8021FCB0 | 388 | matched | renamed from `fn_8021FCB0`: tp name by order and size |
| `revolution/dvd/dvd` | `FatalAlarmHandler` | 0x8021FE40 | 4 | matched | renamed from `fn_8021FE40`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateError` | 0x8021FE50 | 272 | matched | renamed from `fn_8021FE50`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStoreErrorCode1` | 0x8021FF60 | 20 | matched | renamed from `fn_8021FF60`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStoreErrorCode2` | 0x8021FF80 | 60 | matched | renamed from `fn_8021FF80`: tp name by order and size |
| `revolution/dvd/dvd` | `CategorizeError` | 0x8021FFC0 | 244 | matched | renamed from `fn_8021FFC0`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStoreErrorCode3` | 0x802200C0 | 196 | matched | renamed from `fn_802200C0`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateGettingError` | 0x80220190 | 1284 | matched | renamed from `fn_80220190`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForUnrecoveredError` | 0x802206A0 | 480 | matched | renamed from `fn_802206A0`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForUnrecoveredErrorRetry` | 0x80220880 | 212 | matched | renamed from `fn_80220880`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateGoToRetry` | 0x80220960 | 576 | matched | renamed from `fn_80220960`: tp name by order and size |
| `revolution/dvd/dvd` | `stateCheckID` | 0x80220BA0 | 824 | matched | renamed from `fn_80220BA0`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateReadingTOC` | 0x80220EE0 | 416 | matched | renamed from `fn_80220EE0`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateReadingPartitionInfo` | 0x80221080 | 1148 | matched | renamed from `fn_80221080`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateOpenPartition` | 0x80221500 | 392 | matched | renamed from `fn_80221500`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateOpenPartition2` | 0x80221690 | 356 | matched | renamed from `fn_80221690`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateCheckID1` | 0x80221800 | 456 | matched | renamed from `fn_80221800`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateCheckID2` | 0x802219D0 | 404 | matched | renamed from `fn_802219D0`: tp name by order and size |
| `revolution/dvd/dvd` | `stateCoverClosed` | 0x80221B70 | 196 | matched | renamed from `fn_80221B70`: tp name by order and size |
| `revolution/dvd/dvd` | `ResetAlarmHandler` | 0x80221C40 | 592 | matched | renamed from `fn_80221C40`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateReset` | 0x80221E90 | 228 | matched | renamed from `fn_80221E90`: tp name by order and size |
| `revolution/dvd/dvd` | `stateDownRotation` | 0x80221F80 | 200 | matched | renamed from `fn_80221F80`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateDownRotation` | 0x80222050 | 576 | matched | renamed from `fn_80222050`: tp name by order and size |
| `revolution/dvd/dvd` | `stateCoverClosed_CMD` | 0x80222290 | 376 | matched | renamed from `fn_80222290`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateCoverClosed` | 0x80222410 | 216 | matched | renamed from `fn_80222410`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForPrepareCoverRegister` | 0x802224F0 | 392 | matched | renamed from `fn_802224F0`: tp name by order and size |
| `revolution/dvd/dvd` | `CoverAlarmHandler` | 0x80222680 | 36 | matched | renamed from `fn_80222680`: tp name by order and size |
| `revolution/dvd/dvd` | `stateReady` | 0x802226B0 | 796 | matched | renamed from `fn_802226B0`: tp name by order and size |
| `revolution/dvd/dvd` | `stateBusy` | 0x802229D0 | 1456 | matched | renamed from `fn_802229D0`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForStateBusy` | 0x80222F80 | 2528 | matched | renamed from `fn_80222F80`: tp name by order and size |
| `revolution/dvd/dvd` | `DVDReadAbsAsyncPrio` | 0x80223960 | 228 | matched |  |
| `revolution/dvd/dvd` | `DVDInquiryAsync` | 0x80223A50 | 216 | matched |  |
| `revolution/dvd/dvd` | `DVDGetCommandBlockStatus` | 0x80223B30 | 200 | matched |  |
| `revolution/dvd/dvd` | `DVDGetDriveStatus` | 0x80223C00 | 156 | matched | renamed from `fn_80223C00`: tp name by order and size |
| `revolution/dvd/dvd` | `DVDSetAutoInvalidation` | 0x80223CA0 | 16 | matched |  |
| `revolution/dvd/dvd` | `DVDResume` | 0x80223CB0 | 80 | matched |  |
| `revolution/dvd/dvd` | `DVDCancelAsync` | 0x80223D00 | 868 | matched | renamed from `fn_80223D00`: tp name by order and size |
| `revolution/dvd/dvd` | `DVDCancel` | 0x80224070 | 192 | matched | renamed from `fn_80224070`: tp name by order and size |
| `revolution/dvd/dvd` | `cbForCancelSync` | 0x80224130 | 8 | matched | renamed from `fn_80224130`: tp name by order and size |
| `revolution/dvd/dvd` | `__BS2DVDLowCallback` | 0x80224140 | 8 | matched | renamed from `fn_80224140`: tp name by order and size |
| `revolution/dvd/dvd` | `__DVDGetCoverStatus` | 0x80224150 | 204 | matched |  |
| `revolution/dvd/dvd` | `DVDCheckDiskAsync` | 0x80224220 | 568 | matched | renamed from `fn_80224220`: tp name by order and size |
| `revolution/dvd/dvd` | `__DVDPrepareResetAsync` | 0x80224460 | 284 | matched |  |
| `revolution/dvd/dvd` | `Callback` | 0x80224580 | 12 | matched | renamed from `fn_80224580`: tp name by order and size |
| `revolution/dvd/dvd` | `__DVDPrepareReset` | 0x80224590 | 312 | matched |  |
| `revolution/dvd/dvd` | `__DVDTestAlarm` | 0x802246D0 | 32 | matched |  |
| `revolution/dvd/dvd` | `__DVDStopMotorAsync` | 0x802246F0 | 8 | matched | renamed from `fn_802246F0`: tp name by order and size |
| `revolution/dvd/dvd` | `__DVDRestartMotor` | 0x80224700 | 4 | matched | renamed from `fn_80224700`: tp name by order and size |
| `revolution/dvd/dvdqueue` | `__DVDClearWaitingQueue` | 0x80224710 | 56 | matched |  |
| `revolution/dvd/dvdqueue` | `__DVDPushWaitingQueue` | 0x80224750 | 104 | matched |  |
| `revolution/dvd/dvdqueue` | `__DVDPopWaitingQueue` | 0x802247C0 | 160 | matched |  |
| `revolution/dvd/dvdqueue` | `__DVDCheckWaitingQueue` | 0x80224860 | 88 | matched |  |
| `revolution/dvd/dvdqueue` | `__DVDGetNextWaitingQueue` | 0x802248C0 | 100 | matched |  |
| `revolution/dvd/dvdqueue` | `__DVDDequeueWaitingQueue` | 0x80224930 | 96 | matched |  |
| `revolution/dvd/dvderror` | `cbForNandClose` | 0x80224990 | 44 | matched | renamed from `fn_80224990`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandWrite` | 0x802249C0 | 92 | matched | renamed from `fn_802249C0`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandSeek` | 0x80224A20 | 252 | matched | renamed from `fn_80224A20`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandWrite0` | 0x80224B20 | 340 | matched | renamed from `fn_80224B20`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandSeek2` | 0x80224C80 | 188 | matched | renamed from `fn_80224C80`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandRead` | 0x80224D40 | 232 | matched | renamed from `fn_80224D40`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandSeek0` | 0x80224E30 | 160 | matched | renamed from `fn_80224E30`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandSeek1` | 0x80224ED0 | 256 | matched | renamed from `fn_80224ED0`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandOpen` | 0x80224FD0 | 284 | matched | renamed from `fn_80224FD0`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandCreate` | 0x802250F0 | 160 | matched | renamed from `fn_802250F0`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForNandCreateDir` | 0x80225190 | 148 | matched | renamed from `fn_80225190`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForPrepareControlRegister` | 0x80225230 | 144 | matched | renamed from `fn_80225230`: tp name by order and size |
| `revolution/dvd/dvderror` | `cbForPrepareStatusRegister` | 0x802252C0 | 120 | matched |  |
| `revolution/dvd/dvderror` | `__DVDStoreErrorCode` | 0x80225340 | 104 | matched |  |
| `revolution/dvd/dvdidutils` | `DVDCompareDiskID` | 0x802253B0 | 240 | matched |  |
| `revolution/dvd/dvdFatal` | `__DVDShowFatalMessage` | 0x802254A0 | 204 | matched |  |
| `revolution/dvd/dvdFatal` | `DVDSetAutoFatalMessaging` | 0x80225570 | 92 | matched |  |
| `revolution/dvd/dvdFatal` | `__DVDGetAutoFatalMessaging` | 0x802255D0 | 20 | matched |  |
| `revolution/dvd/dvdFatal` | `__DVDPrintFatalMessage` | 0x802255F0 | 24 | matched |  |
| `revolution/dvd/dvdDeviceError` | `lowCallback` | 0x80225610 | 16 | matched |  |
| `revolution/dvd/dvdDeviceError` | `__DVDCheckDevice` | 0x80225620 | 636 | matched |  |
| `revolution/dvd/dvd_broadway` | `doTransactionCallback` | 0x802258A0 | 184 | matched |  |
| `revolution/dvd/dvd_broadway` | `doPrepareCoverRegisterCallback` | 0x80225960 | 188 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowFinalize` | 0x80225A20 | 80 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowInit` | 0x80225A70 | 612 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowReadDiskID` | 0x80225CE0 | 396 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowOpenPartition` | 0x80225E70 | 620 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowOpenPartitionWithTmdAndTicketView` | 0x802260E0 | 612 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowGetNoDiscBufferSizes` | 0x80226350 | 528 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowGetNoDiscOpenPartitionParams` | 0x80226560 | 664 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowClosePartition` | 0x80226800 | 360 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowUnencryptedRead` | 0x80226970 | 408 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowStopMotor` | 0x80226B10 | 408 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowInquiry` | 0x80226CB0 | 364 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowRequestError` | 0x80226E20 | 360 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowSetSpinupFlag` | 0x80226F90 | 12 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowReset` | 0x80226FA0 | 380 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowAudioBufferConfig` | 0x80227120 | 408 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowReportKey` | 0x802272C0 | 408 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowSetMaximumRotation` | 0x80227460 | 388 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowRead` | 0x802275F0 | 432 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowSeek` | 0x802277A0 | 384 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowGetCoverRegister` | 0x80227920 | 16 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowGetStatusRegister` | 0x80227930 | 12 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowGetControlRegister` | 0x80227940 | 12 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowPrepareCoverRegister` | 0x80227950 | 364 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowPrepareStatusRegister` | 0x80227AC0 | 364 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowPrepareControlRegister` | 0x80227C30 | 364 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowGetImmBufferReg` | 0x80227DA0 | 12 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowUnmaskStatusInterrupts` | 0x80227DB0 | 8 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowMaskCoverInterrupt` | 0x80227DC0 | 8 | matched |  |
| `revolution/dvd/dvd_broadway` | `DVDLowClearCoverInterrupt` | 0x80227DD0 | 360 | matched |  |
| `revolution/dvd/dvd_broadway` | `__DVDLowTestAlarm` | 0x80227F40 | 8 | matched |  |
| `revolution/vi/vi` | `OnShutdown` | 0x80227F50 | 408 | matched |  |
| `revolution/vi/vi` | `__VIRetraceHandler` | 0x802280F0 | 2060 | matched |  |
| `revolution/vi/vi` | `VISetPreRetraceCallback` | 0x80228900 | 68 | matched |  |
| `revolution/vi/vi` | `VISetPostRetraceCallback` | 0x80228950 | 68 | matched |  |
| `revolution/vi/vi` | `getTiming` | 0x802289A0 | 280 | matched |  |
| `revolution/vi/vi` | `__VIInit` | 0x80228AC0 | 512 | matched |  |
| `revolution/vi/vi` | `VIInit` | 0x80228CC0 | 1352 | matched |  |
| `revolution/vi/vi` | `VIWaitForRetrace` | 0x80229210 | 84 | matched |  |
| `revolution/vi/vi` | `setFbbRegs` | 0x80229270 | 680 | matched |  |
| `revolution/vi/vi` | `setHorizontalRegs` | 0x80229520 | 224 | matched |  |
| `revolution/vi/vi` | `setVerticalRegs` | 0x80229600 | 404 | matched |  |
| `revolution/vi/vi` | `VIConfigure` | 0x802297A0 | 1740 | matched |  |
| `revolution/vi/vi` | `VIConfigurePan` | 0x80229E70 | 852 | matched |  |
| `revolution/vi/vi` | `VIFlush` | 0x8022A1D0 | 276 | matched |  |
| `revolution/vi/vi` | `VISetNextFrameBuffer` | 0x8022A2F0 | 108 | matched |  |
| `revolution/vi/vi` | `VISetBlack` | 0x8022A360 | 120 | matched |  |
| `revolution/vi/vi` | `VIGetRetraceCount` | 0x8022A3E0 | 8 | matched |  |
| `revolution/vi/vi` | `VIGetNextField` | 0x8022A3F0 | 156 | matched |  |
| `revolution/vi/vi` | `VIGetCurrentLine` | 0x8022A490 | 144 | matched |  |
| `revolution/vi/vi` | `VIGetTvFormat` | 0x8022A520 | 96 | matched |  |
| `revolution/vi/vi` | `VIGetScanMode` | 0x8022A580 | 96 | matched |  |
| `revolution/vi/vi` | `VIGetDTVStatus` | 0x8022A5E0 | 60 | matched |  |
| `revolution/vi/vi` | `__VIDisplayPositionToXY` | 0x8022A620 | 564 | matched |  |
| `revolution/vi/vi` | `VISetTimeToDimming` | 0x8022A860 | 248 | matched |  |
| `revolution/vi/vi` | `VIResetDimmingCount` | 0x8022A960 | 20 | matched |  |
| `revolution/vi/vi` | `__VIResetRFIdle` | 0x8022A980 | 16 | matched |  |
| `revolution/pad/Pad` | `__PADDisableRecalibration` | 0x8022CA00 | 92 | matched |  |
| `revolution/ai/ai` | `AIRegisterDMACallback` | 0x8022CA60 | 68 | matched |  |
| `revolution/ai/ai` | `AIInitDMA` | 0x8022CAB0 | 124 | matched |  |
| `revolution/ai/ai` | `AIStartDMA` | 0x8022CB30 | 20 | matched |  |
| `revolution/ai/ai` | `AIStopDMA` | 0x8022CB50 | 20 | matched |  |
| `revolution/ai/ai` | `AIGetDMABytesLeft` | 0x8022CB70 | 16 | matched |  |
| `revolution/ai/ai` | `AIGetDMAStartAddr` | 0x8022CB80 | 24 | matched |  |
| `revolution/ai/ai` | `AIGetDMALength` | 0x8022CBA0 | 16 | matched |  |
| `revolution/ai/ai` | `AICheckInit` | 0x8022CBB0 | 8 | matched |  |
| `revolution/ai/ai` | `AIInit` | 0x8022CBC0 | 384 | matched |  |
| `revolution/ai/ai` | `__AIDHandler` | 0x8022CD40 | 168 | matched |  |
| `revolution/ai/ai` | `__AICallbackStackSwitch` | 0x8022CDF0 | 100 | ORIGINAL-ASM |  |
| `revolution/ai/ai` | `__AI_SRC_INIT` | 0x8022CE60 | 460 | matched |  |
| `revolution/ax/AX` | `AXInit` | 0x8022D030 | 84 | matched |  |
| `revolution/ax/AX` | `AXQuit` | 0x8022D090 | 72 | matched |  |
| `revolution/ax/AX` | `AXIsInit` | 0x8022D0E0 | 8 | matched |  |
| `revolution/ax/AXAlloc` | `__AXGetStackHead` | 0x8022D0F0 | 20 | matched |  |
| `revolution/ax/AXAlloc` | `__AXServiceCallbackStack` | 0x8022D110 | 172 | matched |  |
| `revolution/ax/AXAlloc` | `__AXAllocInit` | 0x8022D1C0 | 176 | matched |  |
| `revolution/ax/AXAlloc` | `__AXAllocQuit` | 0x8022D270 | 176 | matched |  |
| `revolution/ax/AXAlloc` | `__AXPushFreeStack` | 0x8022D320 | 28 | matched |  |
| `revolution/ax/AXAlloc` | `__AXPushCallbackStack` | 0x8022D340 | 16 | matched |  |
| `revolution/ax/AXAlloc` | `__AXRemoveFromStack` | 0x8022D350 | 132 | matched |  |
| `revolution/ax/AXAlloc` | `AXFreeVoice` | 0x8022D3E0 | 124 | matched |  |
| `revolution/ax/AXAlloc` | `AXAcquireVoice` | 0x8022D460 | 388 | matched |  |
| `revolution/ax/AXAux` | `__AXAuxInit` | 0x8022D5F0 | 276 | matched |  |
| `revolution/ax/AXAux` | `__AXAuxQuit` | 0x8022D710 | 20 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxAInput` | 0x8022D730 | 52 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxAOutput` | 0x8022D770 | 28 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxAInputDpl2` | 0x8022D790 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxAOutputDpl2R` | 0x8022D7B0 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxAOutputDpl2Ls` | 0x8022D7D0 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxAOutputDpl2Rs` | 0x8022D7F0 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxBInput` | 0x8022D810 | 52 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxBOutput` | 0x8022D850 | 28 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxBInputDpl2` | 0x8022D870 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxBOutputDpl2R` | 0x8022D890 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxBOutputDpl2Ls` | 0x8022D8B0 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxBOutputDpl2Rs` | 0x8022D8D0 | 32 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxCInput` | 0x8022D8F0 | 52 | matched |  |
| `revolution/ax/AXAux` | `__AXGetAuxCOutput` | 0x8022D930 | 28 | matched |  |
| `revolution/ax/AXAux` | `__AXProcessAux` | 0x8022D950 | 940 | matched |  |
| `revolution/ax/AXAux` | `AXRegisterAuxACallback` | 0x8022DD00 | 100 | matched |  |
| `revolution/ax/AXAux` | `AXRegisterAuxBCallback` | 0x8022DD70 | 100 | matched |  |
| `revolution/ax/AXAux` | `AXRegisterAuxCCallback` | 0x8022DDE0 | 100 | matched |  |
| `revolution/ax/AXAux` | `AXGetAuxACallback` | 0x8022DE50 | 20 | matched |  |
| `revolution/ax/AXCL` | `__AXGetCommandListCycles` | 0x8022DE70 | 8 | matched |  |
| `revolution/ax/AXCL` | `__AXGetCommandListAddress` | 0x8022DE80 | 48 | matched |  |
| `revolution/ax/AXCL` | `__AXNextFrame` | 0x8022DEB0 | 2492 | matched |  |
| `revolution/ax/AXCL` | `__AXClInit` | 0x8022E870 | 80 | matched |  |
| `revolution/ax/AXCL` | `__AXClQuit` | 0x8022E8C0 | 4 | matched |  |
| `revolution/ax/AXCL` | `AXSetMode` | 0x8022E8D0 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXGetMode` | 0x8022E8E0 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXGetAuxAReturnVolume` | 0x8022E8F0 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXGetAuxBReturnVolume` | 0x8022E900 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXGetAuxCReturnVolume` | 0x8022E910 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXSetAuxAReturnVolume` | 0x8022E920 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXSetAuxBReturnVolume` | 0x8022E930 | 8 | matched |  |
| `revolution/ax/AXCL` | `AXSetAuxCReturnVolume` | 0x8022E940 | 8 | matched |  |
| `revolution/ax/AXOut` | `__AXOutNewFrame` | 0x8022E950 | 608 | matched |  |
| `revolution/ax/AXOut` | `__AXOutAiCallback` | 0x8022EBB0 | 176 | matched |  |
| `revolution/ax/AXOut` | `__AXDSPInitCallback` | 0x8022EC60 | 12 | matched |  |
| `revolution/ax/AXOut` | `__AXDSPResumeCallback` | 0x8022EC70 | 84 | matched |  |
| `revolution/ax/AXOut` | `__AXDSPDoneCallback` | 0x8022ECD0 | 16 | matched |  |
| `revolution/ax/AXOut` | `__AXDSPRequestCallback` | 0x8022ECE0 | 4 | matched |  |
| `revolution/ax/AXOut` | `__AXOutInitDSP` | 0x8022ECF0 | 236 | matched |  |
| `revolution/ax/AXOut` | `__AXOutInit` | 0x8022EDE0 | 504 | matched |  |
| `revolution/ax/AXOut` | `__AXOutQuit` | 0x8022EFE0 | 104 | matched |  |
| `revolution/ax/AXOut` | `AXRegisterCallback` | 0x8022F050 | 68 | matched |  |
| `revolution/ax/AXOut` | `AXRmtGetSamples` | 0x8022F0A0 | 196 | matched |  |
| `revolution/ax/AXOut` | `AXRmtAdvancePtr` | 0x8022F170 | 88 | matched |  |
| `revolution/ax/AXSPB` | `__AXGetStudio` | 0x8022F1D0 | 12 | matched |  |
| `revolution/ax/AXSPB` | `__AXDepopFadeMain` | 0x8022F1E0 | 108 | matched |  |
| `revolution/ax/AXSPB` | `__AXDepopFadeRmt` | 0x8022F250 | 108 | matched |  |
| `revolution/ax/AXSPB` | `__AXPrintStudio` | 0x8022F2C0 | 376 | matched |  |
| `revolution/ax/AXSPB` | `__AXSPBInit` | 0x8022F440 | 88 | matched |  |
| `revolution/ax/AXSPB` | `__AXSPBQuit` | 0x8022F4A0 | 4 | matched |  |
| `revolution/ax/AXSPB` | `__AXDepopVoice` | 0x8022F4B0 | 348 | matched |  |
| `revolution/ax/AXVPB` | `__AXGetNumVoices` | 0x8022F610 | 8 | matched |  |
| `revolution/ax/AXVPB` | `__AXServiceVPB` | 0x8022F620 | 1360 | matched |  |
| `revolution/ax/AXVPB` | `__AXSyncPBs` | 0x8022FB70 | 784 | matched |  |
| `revolution/ax/AXVPB` | `__AXGetPBs` | 0x8022FE80 | 8 | matched |  |
| `revolution/ax/AXVPB` | `__AXSetPBDefault` | 0x8022FE90 | 64 | matched |  |
| `revolution/ax/AXVPB` | `__AXVPBInit` | 0x8022FED0 | 48 | matched |  |
| `revolution/ax/AXVPB` | `__AXVPBInitCommon` | 0x8022FF00 | 652 | matched |  |
| `revolution/ax/AXVPB` | `__AXVPBQuit` | 0x80230190 | 24 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceSrcType` | 0x802301B0 | 192 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceState` | 0x80230270 | 112 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceType` | 0x802302E0 | 76 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceAddr` | 0x80230330 | 232 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceCurrentAddr` | 0x80230420 | 84 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceAdpcm` | 0x80230480 | 152 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceSrc` | 0x80230520 | 132 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceSrcRatio` | 0x802305B0 | 120 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceAdpcmLoop` | 0x80230630 | 96 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceLpf` | 0x80230690 | 104 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceLpfCoefs` | 0x80230700 | 92 | matched |  |
| `revolution/ax/AXVPB` | `AXGetLpfCoefs` | 0x80230760 | 188 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceBiquad` | 0x80230820 | 152 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceBiquadCoefs` | 0x802308C0 | 108 | matched |  |
| `revolution/ax/AXVPB` | `AXSetVoiceRmtOn` | 0x80230930 | 76 | matched |  |
| `revolution/ax/AXVPB` | `AXGetMaxVoices` | 0x80230980 | 8 | matched |  |
| `revolution/ax/AXProf` | `__AXGetCurrentProfile` | 0x80230990 | 64 | matched |  |
| `revolution/axfx/AXFXReverbHi` | `AXFXReverbHiInit` | 0x802309D0 | 100 | matched |  |
| `revolution/axfx/AXFXReverbHi` | `AXFXReverbHiShutdown` | 0x80230A40 | 36 | matched |  |
| `revolution/axfx/AXFXReverbHi` | `AXFXReverbHiCallback` | 0x80230A70 | 4 | matched |  |
| `revolution/axfx/AXFXReverbHiDpl2` | `AXFXReverbHiInitDpl2` | 0x80230A80 | 164 | matched |  |
| `revolution/axfx/AXFXReverbHiDpl2` | `AXFXReverbHiShutdownDpl2` | 0x80230B30 | 36 | matched |  |
| `revolution/axfx/AXFXReverbHiDpl2` | `AXFXReverbHiCallbackDpl2` | 0x80230B60 | 4 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `AXFXReverbHiExpInit` | 0x80230B70 | 412 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `AXFXReverbHiExpShutdown` | 0x80230D10 | 84 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `AXFXReverbHiExpCallback` | 0x80230D70 | 1316 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `__AllocDelayLine` | 0x802312A0 | 372 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `__BzeroDelayLines` | 0x80231420 | 292 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `__FreeDelayLine` | 0x80231550 | 276 | matched |  |
| `revolution/axfx/AXFXReverbHiExp` | `__InitParams` | 0x80231670 | 804 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `AXFXReverbHiExpInitDpl2` | 0x802319A0 | 448 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `AXFXReverbHiExpShutdownDpl2` | 0x80231B60 | 84 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `AXFXReverbHiExpCallbackDpl2` | 0x80231BC0 | 1460 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `__AllocDelayLine` | 0x80232180 | 372 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `__BzeroDelayLines` | 0x80232300 | 292 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `__FreeDelayLine` | 0x80232430 | 276 | matched |  |
| `revolution/axfx/AXFXReverbHiExpDpl2` | `__InitParams` | 0x80232550 | 820 | matched |  |
| `revolution/axfx/AXFXReverbStd` | `AXFXReverbStdInit` | 0x80232890 | 92 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStd` | `AXFXReverbStdShutdown` | 0x802328F0 | 36 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStd` | `AXFXReverbStdCallback` | 0x80232920 | 4 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `AXFXReverbStdExpInit` | 0x80232930 | 380 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `AXFXReverbStdExpShutdown` | 0x80232AB0 | 84 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `AXFXReverbStdExpCallback` | 0x80232B10 | 876 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `__AllocDelayLine` | 0x80232E80 | 324 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `__BzeroDelayLines` | 0x80232FD0 | 256 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `__FreeDelayLine` | 0x802330D0 | 240 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXReverbStdExp` | `__InitParams` | 0x802331C0 | 684 | matched | unit name unconfirmed; invented (Std names modelled on the Hi names) |
| `revolution/axfx/AXFXHooks` | `__AXFXAllocFunction` | 0x80233470 | 12 | matched |  |
| `revolution/axfx/AXFXHooks` | `__AXFXFreeFunction` | 0x80233480 | 12 | matched |  |
| `revolution/axfx/AXFXHooks` | `AXFXSetHooks` | 0x80233490 | 12 | matched |  |
| `revolution/axfx/AXFXHooks` | `AXFXGetHooks` | 0x802334A0 | 20 | matched |  |
| `revolution/mix/mix` | `__MIXSetPan` | 0x802334C0 | 180 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `__MIXGetVolume` | 0x80233580 | 60 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXInit` | 0x802335C0 | 276 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXQuit` | 0x802336E0 | 20 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetSoundMode` | 0x80233700 | 8 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXInitChannel` | 0x80233710 | 5828 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXReleaseChannel` | 0x80234DE0 | 24 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXResetControls` | 0x80234E00 | 176 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetInput` | 0x80234EB0 | 36 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetAuxA` | 0x80234EE0 | 36 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetAuxB` | 0x80234F10 | 36 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetAuxC` | 0x80234F40 | 48 | matched | unit name unconfirmed; invented (no public Wii MIX source or header) |
| `revolution/mix/mix` | `MIXSetPan` | 0x80234F70 | 108 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetSPan` | 0x80234FE0 | 108 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXSetFader` | 0x80235050 | 36 | matched | unit name unconfirmed |
| `revolution/mix/mix` | `MIXUpdateSettings` | 0x80235080 | 5780 | matched | unit name unconfirmed |
| `revolution/mix/mix_rmt` | `MIXRmtSetVolumes` | 0x80236720 | 76 | matched | unit name unconfirmed; invented (no public Wii MIX source or header) |
| `revolution/mix/mix_rmt` | `__MIXUpdateRmtChannel` | 0x80236770 | 1084 | matched | unit name unconfirmed; invented (no public Wii MIX source or header) |
| `revolution/mix/mix_rmt` | `__MIXResetRmtChannel` | 0x80236BB0 | 88 | matched | unit name unconfirmed; invented (no public Wii MIX source or header) |
| `revolution/dsp/dsp` | `DSPCheckMailToDSP` | 0x80236C10 | 16 | matched |  |
| `revolution/dsp/dsp` | `DSPCheckMailFromDSP` | 0x80236C20 | 16 | matched |  |
| `revolution/dsp/dsp` | `DSPReadMailFromDSP` | 0x80236C30 | 20 | matched |  |
| `revolution/dsp/dsp` | `DSPSendMailToDSP` | 0x80236C50 | 20 | matched |  |
| `revolution/dsp/dsp` | `DSPInit` | 0x80236C70 | 192 | matched |  |
| `revolution/dsp/dsp` | `DSPCheckInit` | 0x80236D30 | 8 | matched |  |
| `revolution/dsp/dsp` | `DSPAddTask` | 0x80236D40 | 112 | matched |  |
| `revolution/dsp/dsp` | `DSPCancelTask` | 0x80236DB0 | 64 | matched |  |
| `revolution/dsp/dsp` | `DSPAssertTask` | 0x80236DF0 | 188 | matched |  |
| `revolution/dsp/dsp_debug` | `__DSP_debug_printf` | 0x80236EB0 | 80 | matched |  |
| `revolution/dsp/dsp_task` | `__DSPHandler` | 0x80236F00 | 1068 | matched |  |
| `revolution/dsp/dsp_task` | `__DSP_exec_task` | 0x80237330 | 420 | matched |  |
| `revolution/dsp/dsp_task` | `__DSP_boot_task` | 0x802374E0 | 396 | matched |  |
| `revolution/dsp/dsp_task` | `__DSP_insert_task` | 0x80237670 | 160 | matched |  |
| `revolution/dsp/dsp_task` | `__DSP_remove_task` | 0x80237710 | 136 | matched |  |
| `revolution/gx/GXInit` | `__GXDefaultTexRegionCallback` | 0x802377A0 | 228 | matched |  |
| `revolution/gx/GXInit` | `__GXDefaultTlutRegionCallback` | 0x80237890 | 36 | matched |  |
| `revolution/gx/GXInit` | `__GXShutdown` | 0x802378C0 | 368 | matched |  |
| `revolution/gx/GXInit` | `__GXInitRevisionBits` | 0x80237A30 | 304 | matched |  |
| `revolution/gx/GXInit` | `GXInit` | 0x80237B60 | 1284 | matched |  |
| `revolution/gx/GXInit` | `__GXInitGX` | 0x80238070 | 2240 | matched |  |
| `revolution/gx/GXFifo` | `GXCPInterruptHandler` | 0x80238930 | 352 | matched |  |
| `revolution/gx/GXFifo` | `GXInitFifoBase` | 0x80238A90 | 140 | matched |  |
| `revolution/gx/GXFifo` | `CPGPLinkCheck` | 0x80238B20 | 248 | matched |  |
| `revolution/gx/GXFifo` | `GXSetCPUFifo` | 0x80238C20 | 480 | matched |  |
| `revolution/gx/GXFifo` | `GXSetGPFifo` | 0x80238E00 | 656 | matched |  |
| `revolution/gx/GXFifo` | `__GXSaveFifo` | 0x80239090 | 256 | matched |  |
| `revolution/gx/GXFifo` | `__GXIsGPFifoReady` | 0x80239190 | 8 | matched |  |
| `revolution/gx/GXFifo` | `GXGetGPStatus` | 0x802391A0 | 76 | matched |  |
| `revolution/gx/GXFifo` | `GXGetCPUFifo` | 0x802391F0 | 144 | matched |  |
| `revolution/gx/GXFifo` | `GXGetFifoPtrs` | 0x80239280 | 20 | matched |  |
| `revolution/gx/GXFifo` | `GXGetFifoBase` | 0x802392A0 | 8 | matched |  |
| `revolution/gx/GXFifo` | `GXSetBreakPtCallback` | 0x802392B0 | 68 | matched |  |
| `revolution/gx/GXFifo` | `GXEnableBreakPt` | 0x80239300 | 160 | matched |  |
| `revolution/gx/GXFifo` | `GXDisableBreakPt` | 0x802393A0 | 72 | matched |  |
| `revolution/gx/GXFifo` | `__GXFifoInit` | 0x802393F0 | 124 | matched |  |
| `revolution/gx/GXFifo` | `__GXCleanGPFifo` | 0x80239470 | 416 | matched |  |
| `revolution/gx/GXFifo` | `GXSetCurrentGXThread` | 0x80239610 | 76 | matched |  |
| `revolution/gx/GXAttr` | `GXSetVtxDesc` | 0x80239660 | 612 | matched |  |
| `revolution/gx/GXAttr` | `__GXSetVCD` | 0x802398D0 | 176 | matched |  |
| `revolution/gx/GXAttr` | `__GXCalculateVLim` | 0x80239980 | 300 | matched |  |
| `revolution/gx/GXAttr` | `GXClearVtxDesc` | 0x80239AB0 | 52 | matched |  |
| `revolution/gx/GXAttr` | `GXSetVtxAttrFmt` | 0x80239AF0 | 416 | matched |  |
| `revolution/gx/GXAttr` | `GXSetVtxAttrFmtv` | 0x80239C90 | 456 | matched |  |
| `revolution/gx/GXAttr` | `__GXSetVAT` | 0x80239E60 | 128 | matched |  |
| `revolution/gx/GXAttr` | `GXSetArray` | 0x80239EE0 | 64 | matched |  |
| `revolution/gx/GXAttr` | `GXInvalidateVtxCache` | 0x80239F20 | 16 | matched |  |
| `revolution/gx/GXAttr` | `GXSetTexCoordGen2` | 0x80239F30 | 580 | matched |  |
| `revolution/gx/GXAttr` | `GXSetNumTexGens` | 0x8023A180 | 36 | matched |  |
| `revolution/gx/GXMisc` | `GXSetMisc` | 0x8023A1B0 | 128 | matched |  |
| `revolution/gx/GXMisc` | `GXFlush` | 0x8023A230 | 92 | matched |  |
| `revolution/gx/GXMisc` | `__GXAbort` | 0x8023A290 | 356 | matched |  |
| `revolution/gx/GXMisc` | `GXAbortFrame` | 0x8023A400 | 436 | matched |  |
| `revolution/gx/GXMisc` | `GXSetDrawSync` | 0x8023A5C0 | 180 | matched |  |
| `revolution/gx/GXMisc` | `GXReadDrawSync` | 0x8023A680 | 12 | matched |  |
| `revolution/gx/GXMisc` | `GXDrawDone` | 0x8023A690 | 204 | matched |  |
| `revolution/gx/GXMisc` | `GXPixModeSync` | 0x8023A760 | 36 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeAlphaMode` | 0x8023A790 | 16 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeAlphaRead` | 0x8023A7A0 | 24 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeAlphaUpdate` | 0x8023A7C0 | 20 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeBlendMode` | 0x8023A7E0 | 92 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeColorUpdate` | 0x8023A840 | 20 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeDstAlpha` | 0x8023A860 | 24 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeDither` | 0x8023A880 | 20 | matched |  |
| `revolution/gx/GXMisc` | `GXPokeZMode` | 0x8023A8A0 | 28 | matched |  |
| `revolution/gx/GXMisc` | `GXSetDrawSyncCallback` | 0x8023A8C0 | 68 | matched |  |
| `revolution/gx/GXMisc` | `GXTokenInterruptHandler` | 0x8023A910 | 132 | matched |  |
| `revolution/gx/GXMisc` | `GXSetDrawDoneCallback` | 0x8023A9A0 | 68 | matched |  |
| `revolution/gx/GXMisc` | `GXFinishInterruptHandler` | 0x8023A9F0 | 128 | matched |  |
| `revolution/gx/GXMisc` | `__GXPEInit` | 0x8023AA70 | 100 | matched |  |
| `revolution/gx/GXGeometry` | `__GXSetDirtyState` | 0x8023AAE0 | 640 | matched |  |
| `revolution/gx/GXGeometry` | `GXBegin` | 0x8023AD60 | 332 | matched |  |
| `revolution/gx/GXGeometry` | `__GXSendFlushPrim` | 0x8023AEB0 | 216 | matched |  |
| `revolution/gx/GXGeometry` | `GXSetLineWidth` | 0x8023AF90 | 52 | matched |  |
| `revolution/gx/GXGeometry` | `GXSetPointSize` | 0x8023AFD0 | 52 | matched |  |
| `revolution/gx/GXGeometry` | `GXEnableTexOffsets` | 0x8023B010 | 60 | matched |  |
| `revolution/gx/GXGeometry` | `GXSetCullMode` | 0x8023B050 | 40 | matched |  |
| `revolution/gx/GXGeometry` | `GXSetCoPlanar` | 0x8023B080 | 60 | matched |  |
| `revolution/gx/GXGeometry` | `__GXSetGenMode` | 0x8023B0C0 | 36 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetDispCopySrc` | 0x8023B0F0 | 64 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetTexCopySrc` | 0x8023B130 | 64 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetDispCopyDst` | 0x8023B170 | 36 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetTexCopyDst` | 0x8023B1A0 | 268 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetDispCopyFrame2Field` | 0x8023B2B0 | 32 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetCopyClamp` | 0x8023B2D0 | 40 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetDispCopyYScale` | 0x8023B300 | 204 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetCopyClear` | 0x8023B3D0 | 120 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetCopyFilter` | 0x8023B450 | 484 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXSetDispCopyGamma` | 0x8023B640 | 20 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXCopyDisp` | 0x8023B660 | 316 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXCopyTex` | 0x8023B7A0 | 344 | matched |  |
| `revolution/gx/GXFrameBuf` | `GXClearBoundingBox` | 0x8023B900 | 56 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightAttn` | 0x8023B940 | 28 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightAttnA` | 0x8023B960 | 16 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightSpot` | 0x8023B970 | 412 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightDistAttn` | 0x8023BB10 | 196 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightPos` | 0x8023BBE0 | 16 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightDir` | 0x8023BBF0 | 28 | matched |  |
| `revolution/gx/GXLight` | `GXInitLightColor` | 0x8023BC10 | 12 | matched |  |
| `revolution/gx/GXLight` | `GXLoadLightObjImm` | 0x8023BC20 | 128 | matched |  |
| `revolution/gx/GXLight` | `GXSetChanAmbColor` | 0x8023BCA0 | 216 | matched |  |
| `revolution/gx/GXLight` | `GXSetChanMatColor` | 0x8023BD80 | 216 | matched |  |
| `revolution/gx/GXLight` | `GXSetNumChans` | 0x8023BE60 | 36 | matched |  |
| `revolution/gx/GXLight` | `GXSetChanCtrl` | 0x8023BE90 | 164 | matched |  |
| `revolution/gx/GXTexture` | `__GetImageTileCount` | 0x8023BF40 | 192 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexObj` | 0x8023C000 | 524 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexObjCI` | 0x8023C210 | 72 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexObjLOD` | 0x8023C260 | 260 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexObjWrapMode` | 0x8023C370 | 20 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexObjTlut` | 0x8023C390 | 8 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexObjUserData` | 0x8023C3A0 | 8 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjUserData` | 0x8023C3B0 | 8 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjWidth` | 0x8023C3C0 | 20 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjHeight` | 0x8023C3E0 | 20 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjFmt` | 0x8023C400 | 8 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjWrapS` | 0x8023C410 | 12 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjWrapT` | 0x8023C420 | 12 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjMipMap` | 0x8023C430 | 12 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjLODBias` | 0x8023C440 | 64 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjBiasClamp` | 0x8023C480 | 12 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjEdgeLOD` | 0x8023C490 | 20 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjMaxAniso` | 0x8023C4B0 | 12 | matched |  |
| `revolution/gx/GXTexture` | `GXGetTexObjTlut` | 0x8023C4C0 | 8 | matched |  |
| `revolution/gx/GXTexture` | `GXLoadTexObjPreLoaded` | 0x8023C4D0 | 356 | matched |  |
| `revolution/gx/GXTexture` | `GXLoadTexObj` | 0x8023C640 | 84 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTlutObj` | 0x8023C6A0 | 40 | matched |  |
| `revolution/gx/GXTexture` | `GXLoadTlut` | 0x8023C6D0 | 148 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTexCacheRegion` | 0x8023C770 | 168 | matched |  |
| `revolution/gx/GXTexture` | `GXInitTlutRegion` | 0x8023C820 | 32 | matched |  |
| `revolution/gx/GXTexture` | `GXInvalidateTexRegion` | 0x8023C840 | 284 | matched |  |
| `revolution/gx/GXTexture` | `GXInvalidateTexAll` | 0x8023C960 | 72 | matched |  |
| `revolution/gx/GXTexture` | `GXSetTexRegionCallback` | 0x8023C9B0 | 20 | matched |  |
| `revolution/gx/GXTexture` | `GXSetTlutRegionCallback` | 0x8023C9D0 | 20 | matched |  |
| `revolution/gx/GXTexture` | `GXSetTexCoordScaleManually` | 0x8023C9F0 | 124 | matched |  |
| `revolution/gx/GXTexture` | `GXSetTexCoordBias` | 0x8023CA70 | 100 | matched |  |
| `revolution/gx/GXTexture` | `__SetSURegs` | 0x8023CAE0 | 144 | matched |  |
| `revolution/gx/GXTexture` | `__GXSetSUTexRegs` | 0x8023CB70 | 356 | matched |  |
| `revolution/gx/GXTexture` | `__GXSetTmemConfig` | 0x8023CCE0 | 848 | matched |  |
| `revolution/gx/GXBump` | `GXSetTevIndirect` | 0x8023D030 | 104 | matched |  |
| `revolution/gx/GXBump` | `GXSetIndTexMtx` | 0x8023D0A0 | 320 | matched |  |
| `revolution/gx/GXBump` | `GXSetIndTexCoordScale` | 0x8023D1E0 | 256 | matched |  |
| `revolution/gx/GXBump` | `GXSetIndTexOrder` | 0x8023D2E0 | 200 | matched |  |
| `revolution/gx/GXBump` | `GXSetNumIndStages` | 0x8023D3B0 | 32 | matched |  |
| `revolution/gx/GXBump` | `GXSetTevDirect` | 0x8023D3D0 | 72 | matched |  |
| `revolution/gx/GXBump` | `GXSetTevIndWarp` | 0x8023D420 | 84 | matched |  |
| `revolution/gx/GXBump` | `__GXUpdateBPMask` | 0x8023D480 | 4 | matched |  |
| `revolution/gx/GXBump` | `__GXSetIndirectMask` | 0x8023D490 | 48 | matched |  |
| `revolution/gx/GXBump` | `__GXFlushTextureState` | 0x8023D4C0 | 36 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevOp` | 0x8023D4F0 | 148 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevColorIn` | 0x8023D590 | 64 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevAlphaIn` | 0x8023D5D0 | 64 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevColorOp` | 0x8023D610 | 88 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevAlphaOp` | 0x8023D670 | 88 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevColor` | 0x8023D6D0 | 96 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevColorS10` | 0x8023D730 | 100 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevKColor` | 0x8023D7A0 | 92 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevKColorSel` | 0x8023D800 | 80 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevKAlphaSel` | 0x8023D850 | 80 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevSwapMode` | 0x8023D8A0 | 60 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevSwapModeTable` | 0x8023D8E0 | 124 | matched |  |
| `revolution/gx/GXTev` | `GXSetAlphaCompare` | 0x8023D960 | 56 | matched |  |
| `revolution/gx/GXTev` | `GXSetZTexture` | 0x8023D9A0 | 132 | matched |  |
| `revolution/gx/GXTev` | `GXSetTevOrder` | 0x8023DA30 | 348 | matched |  |
| `revolution/gx/GXTev` | `GXSetNumTevStages` | 0x8023DB90 | 36 | matched |  |
| `revolution/gx/GXPixel` | `GXSetFog` | 0x8023DBC0 | 556 | matched |  |
| `revolution/gx/GXPixel` | `GXSetFogRangeAdj` | 0x8023DDF0 | 272 | matched |  |
| `revolution/gx/GXPixel` | `GXSetBlendMode` | 0x8023DF00 | 80 | matched |  |
| `revolution/gx/GXPixel` | `GXSetColorUpdate` | 0x8023DF50 | 44 | matched |  |
| `revolution/gx/GXPixel` | `GXSetAlphaUpdate` | 0x8023DF80 | 44 | matched |  |
| `revolution/gx/GXPixel` | `GXSetZMode` | 0x8023DFB0 | 52 | matched |  |
| `revolution/gx/GXPixel` | `GXSetZCompLoc` | 0x8023DFF0 | 48 | matched |  |
| `revolution/gx/GXPixel` | `GXSetPixelFmt` | 0x8023E020 | 168 | matched |  |
| `revolution/gx/GXPixel` | `GXSetDither` | 0x8023E0D0 | 44 | matched |  |
| `revolution/gx/GXPixel` | `GXSetDstAlpha` | 0x8023E100 | 48 | matched |  |
| `revolution/gx/GXPixel` | `GXSetFieldMask` | 0x8023E130 | 52 | matched |  |
| `revolution/gx/GXPixel` | `GXSetFieldMode` | 0x8023E170 | 112 | matched |  |
| `revolution/gx/GXDisplayList` | `GXCallDisplayList` | 0x8023E1E0 | 116 | matched |  |
| `revolution/gx/GXTransform` | `__GXSetProjection` | 0x8023E260 | 68 | matched |  |
| `revolution/gx/GXTransform` | `GXSetProjection` | 0x8023E2B0 | 100 | matched |  |
| `revolution/gx/GXTransform` | `GXSetProjectionv` | 0x8023E320 | 76 | matched |  |
| `revolution/gx/GXTransform` | `GXGetProjectionv` | 0x8023E370 | 64 | matched |  |
| `revolution/gx/GXTransform` | `GXLoadPosMtxImm` | 0x8023E3B0 | 80 | matched |  |
| `revolution/gx/GXTransform` | `GXLoadNrmMtxImm` | 0x8023E400 | 88 | matched |  |
| `revolution/gx/GXTransform` | `GXSetCurrentMtx` | 0x8023E460 | 32 | matched |  |
| `revolution/gx/GXTransform` | `GXLoadTexMtxImm` | 0x8023E480 | 180 | matched |  |
| `revolution/gx/GXTransform` | `__GXSetViewport` | 0x8023E540 | 144 | matched |  |
| `revolution/gx/GXTransform` | `GXSetViewportJitter` | 0x8023E5D0 | 60 | matched |  |
| `revolution/gx/GXTransform` | `GXSetViewport` | 0x8023E610 | 44 | matched |  |
| `revolution/gx/GXTransform` | `GXSetScissor` | 0x8023E640 | 104 | matched |  |
| `revolution/gx/GXTransform` | `GXSetScissorBoxOffset` | 0x8023E6B0 | 60 | matched |  |
| `revolution/gx/GXTransform` | `GXSetClipMode` | 0x8023E6F0 | 40 | matched |  |
| `revolution/gx/GXTransform` | `__GXSetMatrixIndex` | 0x8023E720 | 136 | matched |  |
| `revolution/gx/GXPerf` | `GXSetGPMetric` | 0x8023E7B0 | 2076 | matched |  |
| `revolution/gx/GXPerf` | `GXClearGPMetric` | 0x8023EFD0 | 16 | matched |  |
| `revolution/gx/GXPerf` | `GXInitXfRasMetric` | 0x8023EFE0 | 68 | matched |  |
| `revolution/gx/GXPerf` | `GXReadXfRasMetric` | 0x8023F030 | 164 | matched |  |
| `revolution/mem/mem_heapCommon` | `FindContainHeap_` | 0x8023F0E0 | 320 | matched |  |
| `revolution/mem/mem_heapCommon` | `MEMiInitHeapHead` | 0x8023F220 | 448 | matched |  |
| `revolution/mem/mem_heapCommon` | `MEMiFinalizeHeap` | 0x8023F3E0 | 364 | matched |  |
| `revolution/mem/mem_heapCommon` | `MEMFindContainHeap` | 0x8023F550 | 320 | matched |  |
| `revolution/mem/mem_expHeap` | `AllocUsedBlockFromFreeBlock_` | 0x8023F690 | 556 | matched |  |
| `revolution/mem/mem_expHeap` | `AllocFromHead_` | 0x8023F8C0 | 220 | matched |  |
| `revolution/mem/mem_expHeap` | `AllocFromTail_` | 0x8023F9A0 | 200 | matched |  |
| `revolution/mem/mem_expHeap` | `RecycleRegion_` | 0x8023FA70 | 360 | matched |  |
| `revolution/mem/mem_expHeap` | `MEMCreateExpHeapEx` | 0x8023FBE0 | 176 | matched |  |
| `revolution/mem/mem_expHeap` | `MEMDestroyExpHeap` | 0x8023FC90 | 48 | matched |  |
| `revolution/mem/mem_expHeap` | `MEMAllocFromExpHeapEx` | 0x8023FCC0 | 176 | matched |  |
| `revolution/mem/mem_expHeap` | `MEMResizeForMBlockExpHeap` | 0x8023FD70 | 508 | matched |  |
| `revolution/mem/mem_expHeap` | `MEMFreeToExpHeap` | 0x8023FF70 | 196 | matched |  |
| `revolution/mem/mem_expHeap` | `MEMGetSizeForMBlockExpHeap` | 0x80240040 | 8 | matched |  |
| `revolution/mem/mem_allocator` | `AllocatorAllocForExpHeap_` | 0x80240050 | 16 | matched |  |
| `revolution/mem/mem_allocator` | `AllocatorFreeForExpHeap_` | 0x80240060 | 8 | matched |  |
| `revolution/mem/mem_allocator` | `MEMAllocFromAllocator` | 0x80240070 | 16 | matched |  |
| `revolution/mem/mem_allocator` | `MEMFreeToAllocator` | 0x80240080 | 16 | matched |  |
| `revolution/mem/mem_allocator` | `MEMInitAllocatorForExpHeap` | 0x80240090 | 28 | matched |  |
| `revolution/mem/mem_list` | `MEMInitList` | 0x802400B0 | 24 | matched |  |
| `revolution/mem/mem_list` | `MEMAppendListObject` | 0x802400D0 | 112 | matched |  |
| `revolution/mem/mem_list` | `MEMRemoveListObject` | 0x80240140 | 108 | matched |  |
| `revolution/mem/mem_list` | `MEMGetNextListObject` | 0x802401B0 | 32 | matched |  |
| `revolution/euart/euart` | `EUARTInit` | 0x802401D0 | 252 | matched |  |
| `revolution/euart/euart` | `InitializeUART` | 0x802402D0 | 72 | matched |  |
| `revolution/euart/euart` | `WriteUARTN` | 0x80240320 | 560 | matched |  |
| `revolution/fs/fs` | `ISFS_OpenLib` | 0x80240550 | 296 | matched |  |
| `revolution/fs/fs` | `_isfsFuncCb` | 0x80240680 | 384 | matched |  |
| `revolution/fs/fs` | `ISFS_CreateDirAsync` | 0x80240800 | 244 | matched |  |
| `revolution/fs/fs` | `ISFS_ReadDir` | 0x80240900 | 340 | matched |  |
| `revolution/fs/fs` | `ISFS_ReadDirAsync` | 0x80240A60 | 328 | matched |  |
| `revolution/fs/fs` | `ISFS_GetAttr` | 0x80240BB0 | 344 | matched |  |
| `revolution/fs/fs` | `ISFS_GetAttrAsync` | 0x80240D10 | 308 | matched |  |
| `revolution/fs/fs` | `ISFS_Delete` | 0x80240E50 | 216 | matched |  |
| `revolution/fs/fs` | `ISFS_DeleteAsync` | 0x80240F30 | 208 | matched |  |
| `revolution/fs/fs` | `ISFS_Rename` | 0x80241000 | 260 | matched |  |
| `revolution/fs/fs` | `ISFS_RenameAsync` | 0x80241110 | 260 | matched |  |
| `revolution/fs/fs` | `ISFS_GetUsage` | 0x80241220 | 312 | matched |  |
| `revolution/fs/fs` | `ISFS_GetUsageAsync` | 0x80241360 | 292 | matched |  |
| `revolution/fs/fs` | `ISFS_CreateFile` | 0x80241490 | 244 | matched |  |
| `revolution/fs/fs` | `ISFS_CreateFileAsync` | 0x80241590 | 244 | matched |  |
| `revolution/fs/fs` | `ISFS_Open` | 0x80241690 | 200 | matched |  |
| `revolution/fs/fs` | `ISFS_OpenAsync` | 0x80241760 | 184 | matched |  |
| `revolution/fs/fs` | `ISFS_SeekAsync` | 0x80241820 | 140 | matched |  |
| `revolution/fs/fs` | `ISFS_Read` | 0x802418B0 | 32 | matched |  |
| `revolution/fs/fs` | `ISFS_ReadAsync` | 0x802418D0 | 164 | matched |  |
| `revolution/fs/fs` | `ISFS_Write` | 0x80241980 | 32 | matched |  |
| `revolution/fs/fs` | `ISFS_WriteAsync` | 0x802419A0 | 164 | matched |  |
| `revolution/fs/fs` | `ISFS_Close` | 0x80241A50 | 4 | matched |  |
| `revolution/fs/fs` | `ISFS_CloseAsync` | 0x80241A60 | 132 | matched |  |
| `revolution/fs/fs` | `ISFS_ShutdownAsync` | 0x80241AF0 | 144 | matched |  |
| `revolution/ipc/ipcMain` | `IPCInit` | 0x80241B80 | 76 | matched |  |
| `revolution/ipc/ipcMain` | `IPCReInit` | 0x80241BD0 | 72 | matched |  |
| `revolution/ipc/ipcMain` | `IPCReadReg` | 0x80241C20 | 16 | matched |  |
| `revolution/ipc/ipcMain` | `IPCWriteReg` | 0x80241C30 | 16 | matched |  |
| `revolution/ipc/ipcMain` | `IPCGetBufferHi` | 0x80241C40 | 8 | matched |  |
| `revolution/ipc/ipcMain` | `IPCGetBufferLo` | 0x80241C50 | 8 | matched |  |
| `revolution/ipc/ipcMain` | `IPCSetBufferLo` | 0x80241C60 | 8 | matched |  |
| `revolution/ipc/ipcclt` | `strnlen` | 0x80241C70 | 44 | matched |  |
| `revolution/ipc/ipcclt` | `IpcReplyHandler` | 0x80241CA0 | 600 | matched |  |
| `revolution/ipc/ipcclt` | `IPCInterruptHandler` | 0x80241F00 | 428 | matched |  |
| `revolution/ipc/ipcclt` | `IPCCltInit` | 0x802420B0 | 192 | matched |  |
| `revolution/ipc/ipcclt` | `IPCCltReInit` | 0x80242170 | 116 | matched |  |
| `revolution/ipc/ipcclt` | `__ios_Ipc2` | 0x802421F0 | 576 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_OpenAsync` | 0x80242430 | 284 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_Open` | 0x80242550 | 292 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_CloseAsync` | 0x80242680 | 192 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_Close` | 0x80242740 | 168 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_ReadAsync` | 0x802427F0 | 256 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_Read` | 0x802428F0 | 264 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_WriteAsync` | 0x80242A00 | 256 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_Write` | 0x80242B00 | 264 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_SeekAsync` | 0x80242C10 | 224 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_IoctlAsync` | 0x80242CF0 | 312 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_Ioctl` | 0x80242E30 | 304 | matched |  |
| `revolution/ipc/ipcclt` | `__ios_Ioctlv` | 0x80242F60 | 316 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_IoctlvAsync` | 0x802430A0 | 228 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_Ioctlv` | 0x80243190 | 220 | matched |  |
| `revolution/ipc/ipcclt` | `IOS_IoctlvReboot` | 0x80243270 | 764 | matched |  |
| `revolution/ipc/memory` | `iosCreateHeap` | 0x80243570 | 304 | matched |  |
| `revolution/ipc/memory` | `__iosAlloc` | 0x802436A0 | 508 | matched |  |
| `revolution/ipc/memory` | `iosAllocAligned` | 0x802438A0 | 4 | matched |  |
| `revolution/ipc/memory` | `iosFree` | 0x802438B0 | 488 | matched |  |
| `revolution/ipc/ipcProfile` | `IPCiProfInit` | 0x80243AA0 | 184 | matched |  |
| `revolution/ipc/ipcProfile` | `IPCiProfQueueReq` | 0x80243B60 | 132 | matched |  |
| `revolution/ipc/ipcProfile` | `IPCiProfAck` | 0x80243BF0 | 16 | matched |  |
| `revolution/ipc/ipcProfile` | `IPCiProfReply` | 0x80243C00 | 124 | matched |  |
| `revolution/nand/nand` | `NANDPrivateCreate` | 0x80243C80 | 240 | matched |  |
| `revolution/nand/nand` | `NANDCreateAsync` | 0x80243D70 | 280 | matched |  |
| `revolution/nand/nand` | `NANDPrivateCreateAsync` | 0x80243E90 | 256 | matched |  |
| `revolution/nand/nand` | `NANDDelete` | 0x80243F90 | 176 | matched |  |
| `revolution/nand/nand` | `NANDPrivateDelete` | 0x80244040 | 152 | matched |  |
| `revolution/nand/nand` | `NANDDeleteAsync` | 0x802440E0 | 216 | matched |  |
| `revolution/nand/nand` | `NANDPrivateDeleteAsync` | 0x802441C0 | 192 | matched |  |
| `revolution/nand/nand` | `NANDRead` | 0x80244280 | 104 | matched |  |
| `revolution/nand/nand` | `NANDReadAsync` | 0x802442F0 | 120 | matched |  |
| `revolution/nand/nand` | `NANDWrite` | 0x80244370 | 104 | matched |  |
| `revolution/nand/nand` | `NANDWriteAsync` | 0x802443E0 | 120 | matched |  |
| `revolution/nand/nand` | `NANDSeekAsync` | 0x80244460 | 168 | matched |  |
| `revolution/nand/nand` | `NANDCreateDirAsync` | 0x80244510 | 280 | matched |  |
| `revolution/nand/nand` | `NANDPrivateCreateDirAsync` | 0x80244630 | 256 | matched |  |
| `revolution/nand/nand` | `nandComposePerm` | 0x80244730 | 84 | matched |  |
| `revolution/nand/nand` | `nandSplitPerm` | 0x80244790 | 140 | matched |  |
| `revolution/nand/nand` | `nandGetStatus` | 0x80244820 | 340 | matched |  |
| `revolution/nand/nand` | `nandGetStatusCallback` | 0x80244980 | 116 | matched |  |
| `revolution/nand/nand` | `NANDPrivateGetStatus` | 0x80244A00 | 100 | matched |  |
| `revolution/nand/nand` | `NANDPrivateGetStatusAsync` | 0x80244A70 | 132 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandOpen` | 0x80244B00 | 272 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDOpen` | 0x80244C10 | 140 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDPrivateOpen` | 0x80244CA0 | 140 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDOpenAsync` | 0x80244D30 | 120 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDPrivateOpenAsync` | 0x80244DB0 | 120 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandOpenCallback` | 0x80244E30 | 120 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDClose` | 0x80244EB0 | 108 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDCloseAsync` | 0x80244F20 | 136 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDSimpleSafeClose` | 0x80244FB0 | 8 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandSafeClose` | 0x80244FC0 | 468 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDSimpleSafeOpenAsync` | 0x802451A0 | 44 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandSafeOpenAsync` | 0x802451D0 | 360 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandSafeOpenCallback` | 0x80245340 | 976 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandReadOpenCallback` | 0x80245710 | 152 | matched |  |
| `revolution/nand/NANDOpenClose` | `NANDSimpleSafeCloseAsync` | 0x802457B0 | 8 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandSafeCloseAsync` | 0x802457C0 | 252 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandSafeCloseCallback` | 0x802458C0 | 452 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandReadCloseCallback` | 0x80245A90 | 92 | matched |  |
| `revolution/nand/NANDOpenClose` | `nandCloseCallback` | 0x80245AF0 | 92 | matched |  |
| `revolution/nand/NANDCore` | `nandRemoveTailToken` | 0x80245B50 | 212 | matched |  |
| `revolution/nand/NANDCore` | `nandGetHeadToken` | 0x80245C30 | 212 | matched |  |
| `revolution/nand/NANDCore` | `nandGetRelativeName` | 0x80245D10 | 152 | matched |  |
| `revolution/nand/NANDCore` | `nandConvertPath` | 0x80245DB0 | 316 | matched |  |
| `revolution/nand/NANDCore` | `nandIsPrivatePath` | 0x80245EF0 | 52 | matched |  |
| `revolution/nand/NANDCore` | `nandIsUnderPrivatePath` | 0x80245F30 | 88 | matched |  |
| `revolution/nand/NANDCore` | `nandIsInitialized` | 0x80245F90 | 20 | matched |  |
| `revolution/nand/NANDCore` | `nandConvertErrorCode` | 0x80245FB0 | 360 | matched |  |
| `revolution/nand/NANDCore` | `nandGenerateAbsPath` | 0x80246120 | 200 | matched |  |
| `revolution/nand/NANDCore` | `nandGetParentDirectory` | 0x802461F0 | 164 | matched |  |
| `revolution/nand/NANDCore` | `NANDInit` | 0x802462A0 | 300 | matched |  |
| `revolution/nand/NANDCore` | `nandOnShutdown` | 0x802463D0 | 204 | matched |  |
| `revolution/nand/NANDCore` | `nandShutdownCallback` | 0x802464A0 | 12 | matched |  |
| `revolution/nand/NANDCore` | `nandCallback` | 0x802464B0 | 60 | matched |  |
| `revolution/nand/NANDCore` | `nandGetType` | 0x802464F0 | 724 | matched |  |
| `revolution/nand/NANDCore` | `NANDPrivateGetTypeAsync` | 0x802467D0 | 72 | matched |  |
| `revolution/nand/NANDCore` | `nandGetTypeCallback` | 0x80246820 | 120 | matched |  |
| `revolution/nand/NANDCore` | `nandGetHomeDir` | 0x802468A0 | 12 | matched |  |
| `revolution/nand/NANDCore` | `NANDInitBanner` | 0x802468B0 | 220 | matched |  |
| `revolution/nand/NANDCheck` | `nandCalcUsage` | 0x80246990 | 188 | matched |  |
| `revolution/nand/NANDCheck` | `NANDCheckAsync` | 0x80246A50 | 172 | matched |  |
| `revolution/nand/NANDCheck` | `nandUserAreaCallback` | 0x80246B00 | 564 | matched |  |
| `revolution/nand/NANDCheck` | `NANDGetFreeSpace` | 0x80246D40 | 284 | matched |  |
| `revolution/nand/NANDLogging` | `reserveFileDescriptor` | 0x80246E60 | 84 | matched |  |
| `revolution/nand/NANDLogging` | `NANDLoggingAddMessageAsync` | 0x80246EC0 | 248 | matched |  |
| `revolution/nand/NANDLogging` | `asyncRoutine` | 0x80246FC0 | 1156 | matched |  |
| `revolution/sc/scsystem` | `SCInit` | 0x80247450 | 160 | matched |  |
| `revolution/sc/scsystem` | `SCCheckStatus` | 0x802474F0 | 284 | matched |  |
| `revolution/sc/scsystem` | `SCReloadConfFileAsync` | 0x80247610 | 276 | matched |  |
| `revolution/sc/scsystem` | `OpenCallbackFromReload` | 0x80247730 | 208 | matched |  |
| `revolution/sc/scsystem` | `ReadCallbackFromReload` | 0x80247800 | 232 | matched |  |
| `revolution/sc/scsystem` | `CloseCallbackFromReload` | 0x802478F0 | 140 | matched |  |
| `revolution/sc/scsystem` | `FinishFromReload` | 0x80247980 | 292 | matched |  |
| `revolution/sc/scsystem` | `CloseCallbackFromReloadError` | 0x80247AB0 | 4 | matched |  |
| `revolution/sc/scsystem` | `ParseConfBuf` | 0x80247AC0 | 584 | matched |  |
| `revolution/sc/scsystem` | `UnpackItem` | 0x80247D10 | 384 | matched |  |
| `revolution/sc/scsystem` | `DeleteItemByID` | 0x80247E90 | 404 | matched |  |
| `revolution/sc/scsystem` | `CreateItemByID` | 0x80248030 | 632 | matched |  |
| `revolution/sc/scsystem` | `SCFindByteArrayItem` | 0x802482B0 | 224 | matched |  |
| `revolution/sc/scsystem` | `SCReplaceByteArrayItem` | 0x80248390 | 292 | matched |  |
| `revolution/sc/scsystem` | `SCFindU8Item` | 0x802484C0 | 212 | matched |  |
| `revolution/sc/scsystem` | `SCFindS8Item` | 0x802485A0 | 212 | matched |  |
| `revolution/sc/scsystem` | `SCFindU32Item` | 0x80248680 | 212 | matched |  |
| `revolution/sc/scsystem` | `SCReplaceU8Item` | 0x80248760 | 260 | matched |  |
| `revolution/sc/scsystem` | `__SCFlushSyncCallback` | 0x80248870 | 12 | matched |  |
| `revolution/sc/scsystem` | `SCFlushAsync` | 0x80248880 | 540 | matched |  |
| `revolution/sc/scsystem` | `MyNandCallback` | 0x80248AA0 | 824 | matched |  |
| `revolution/sc/scapi` | `SCGetAspectRatio` | 0x80248DE0 | 84 | matched |  |
| `revolution/sc/scapi` | `SCGetDisplayOffsetH` | 0x80248E40 | 116 | matched |  |
| `revolution/sc/scapi` | `SCGetIdleMode` | 0x80248EC0 | 12 | matched |  |
| `revolution/sc/scapi` | `SCGetLanguage` | 0x80248ED0 | 108 | matched |  |
| `revolution/sc/scapi` | `SCGetScreenSaverMode` | 0x80248F40 | 84 | matched |  |
| `revolution/sc/scapi` | `SCGetSoundMode` | 0x80248FA0 | 84 | matched |  |
| `revolution/sc/scapi` | `SCGetCounterBias` | 0x80249000 | 64 | matched |  |
| `revolution/sc/scapi` | `SCGetBtDeviceInfoArray` | 0x80249040 | 12 | matched |  |
| `revolution/sc/scapi` | `SCSetBtDeviceInfoArray` | 0x80249050 | 12 | matched |  |
| `revolution/sc/scapi` | `SCGetBtCmpDeviceInfoArray` | 0x80249060 | 12 | matched |  |
| `revolution/sc/scapi` | `SCSetBtCmpDeviceInfoArray` | 0x80249070 | 12 | matched |  |
| `revolution/sc/scapi` | `SCGetBtDpdSensibility` | 0x80249080 | 104 | matched |  |
| `revolution/sc/scapi` | `SCGetWpadMotorMode` | 0x802490F0 | 84 | matched |  |
| `revolution/sc/scapi` | `SCSetWpadMotorMode` | 0x80249150 | 8 | matched |  |
| `revolution/sc/scapi` | `SCGetWpadSensorBarPosition` | 0x80249160 | 84 | matched |  |
| `revolution/sc/scapi` | `SCGetWpadSpeakerVolume` | 0x802491C0 | 84 | matched |  |
| `revolution/sc/scapi` | `SCSetWpadSpeakerVolume` | 0x80249220 | 8 | matched |  |
| `revolution/sc/scapi_prdinfo` | `__SCF1` | 0x80249230 | 368 | matched |  |
| `revolution/sc/scapi_prdinfo` | `SCGetProductArea` | 0x802493A0 | 132 | matched |  |
| `revolution/sc/scapi_prdinfo` | `SCGetProductGameRegion` | 0x80249430 | 132 | matched |  |
| `revolution/esp/esp` | `ESP_InitLib` | 0x80249790 | 84 | matched |  |
| `revolution/esp/esp` | `ESP_CloseLib` | 0x802497F0 | 72 | matched |  |
| `revolution/esp/esp` | `ESP_LaunchTitle` | 0x80249840 | 148 | matched |  |
| `revolution/esp/esp` | `ESP_GetTicketViews` | 0x802498E0 | 276 | matched |  |
| `revolution/esp/esp` | `ESP_DiGetTicketView` | 0x80249A00 | 164 | matched |  |
| `revolution/esp/esp` | `ESP_DiGetTmd` | 0x80249AB0 | 236 | matched |  |
| `revolution/esp/esp` | `ESP_GetDataDir` | 0x80249BA0 | 152 | matched |  |
| `revolution/esp/esp` | `ESP_GetTitleId` | 0x80249C40 | 152 | matched |  |
| `revolution/esp/esp` | `ESP_GetConsumption` | 0x80249CE0 | 208 | matched |  |
| `revolution/arc/arc` | `ARCInitHandle` | 0x80249DB0 | 160 | matched |  |
| `revolution/arc/arc` | `ARCOpen` | 0x80249E50 | 684 | matched |  |
| `revolution/arc/arc` | `ARCFastOpen` | 0x8024A100 | 80 | matched |  |
| `revolution/arc/arc` | `ARCConvertPathToEntrynum` | 0x8024A150 | 636 | matched |  |
| `revolution/arc/arc` | `entryToPath` | 0x8024A3D0 | 464 | matched |  |
| `revolution/arc/arc` | `ARCGetStartAddrInMem` | 0x8024A5A0 | 20 | matched |  |
| `revolution/arc/arc` | `ARCGetLength` | 0x8024A5C0 | 8 | matched |  |
| `revolution/arc/arc` | `ARCClose` | 0x8024A5D0 | 8 | matched |  |
| `revolution/arc/arc` | `ARCChangeDir` | 0x8024A5E0 | 88 | matched |  |
| `revolution/arc/arc` | `ARCOpenDir` | 0x8024A640 | 124 | matched |  |
| `revolution/arc/arc` | `ARCReadDir` | 0x8024A6C0 | 188 | matched |  |
| `revolution/arc/arc` | `ARCCloseDir` | 0x8024A780 | 8 | matched |  |
| `revolution/bte/gki_buffer` | `gki_init_free_queue` | 0x8024A790 | 544 | matched |  |
| `revolution/bte/gki_buffer` | `gki_buffer_init` | 0x8024A9B0 | 684 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_init_q` | 0x8024AC60 | 20 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_getbuf` | 0x8024AC80 | 416 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_getpoolbuf` | 0x8024AE20 | 232 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_freebuf` | 0x8024AF10 | 368 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_get_buf_size` | 0x8024B080 | 68 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_send_msg` | 0x8024B0D0 | 444 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_read_mbox` | 0x8024B290 | 176 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_enqueue` | 0x8024B340 | 316 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_enqueue_head` | 0x8024B480 | 320 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_dequeue` | 0x8024B5C0 | 160 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_remove_from_queue` | 0x8024B660 | 196 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_getfirst` | 0x8024B730 | 8 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_getnext` | 0x8024B740 | 28 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_queue_is_empty` | 0x8024B760 | 16 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_create_pool` | 0x8024B770 | 840 | matched |  |
| `revolution/bte/gki_buffer` | `GKI_delete_pool` | 0x8024BAC0 | 360 | matched |  |
| `revolution/bte/gki_time` | `gki_timers_init` | 0x8024BC30 | 188 | matched |  |
| `revolution/bte/gki_time` | `GKI_get_tick_count` | 0x8024BCF0 | 20 | matched |  |
| `revolution/bte/gki_time` | `GKI_start_timer` | 0x8024BD10 | 332 | matched |  |
| `revolution/bte/gki_time` | `GKI_stop_timer` | 0x8024BE60 | 132 | matched |  |
| `revolution/bte/gki_time` | `GKI_init_timer_list` | 0x8024BEF0 | 20 | matched |  |
| `revolution/bte/gki_time` | `GKI_update_timer_list` | 0x8024BF10 | 144 | matched |  |
| `revolution/bte/gki_time` | `GKI_add_to_timer_list` | 0x8024BFA0 | 236 | matched |  |
| `revolution/bte/gki_time` | `GKI_remove_from_timer_list` | 0x8024C090 | 292 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_init` | 0x8024C1C0 | 144 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_shutdown` | 0x8024C250 | 168 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_run` | 0x8024C300 | 4 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_sched_lock` | 0x8024C310 | 4 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_sched_unlock` | 0x8024C320 | 4 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_delay` | 0x8024C330 | 4 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_send_event` | 0x8024C340 | 164 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_get_taskid` | 0x8024C3F0 | 8 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_enable` | 0x8024C400 | 36 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_disable` | 0x8024C430 | 68 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_exception` | 0x8024C480 | 4 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_os_malloc` | 0x8024C490 | 44 | matched |  |
| `revolution/bte/gki_ppc` | `GKI_os_free` | 0x8024C4C0 | 4 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_usb_cback` | 0x8024C4D0 | 20 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_receive_msg` | 0x8024C4F0 | 920 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_send_msg_now` | 0x8024C890 | 428 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_init` | 0x8024CA40 | 44 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_open` | 0x8024CA70 | 104 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_close` | 0x8024CAE0 | 52 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_send` | 0x8024CB20 | 48 | matched |  |
| `revolution/bte/hcisu_h2` | `hcisu_h2_handle_event` | 0x8024CB50 | 60 | matched |  |
| `revolution/bte/uusb_ppc` | `uusb_CloseDeviceCB` | 0x8024CB90 | 148 | matched |  |
| `revolution/bte/uusb_ppc` | `uusb_ReadIntrDataCB` | 0x8024CC30 | 424 | matched |  |
| `revolution/bte/uusb_ppc` | `uusb_ReadBulkDataCB` | 0x8024CDE0 | 412 | matched |  |
| `revolution/bte/uusb_ppc` | `uusb_WriteCtrlDataCB` | 0x8024CF80 | 236 | matched |  |
| `revolution/bte/uusb_ppc` | `uusb_WriteBulkDataCB` | 0x8024D070 | 224 | matched |  |
| `revolution/bte/uusb_ppc` | `UUSB_Register` | 0x8024D150 | 520 | matched |  |
| `revolution/bte/uusb_ppc` | `UUSB_Open` | 0x8024D360 | 396 | matched |  |
| `revolution/bte/uusb_ppc` | `UUSB_Read` | 0x8024D4F0 | 8 | matched |  |
| `revolution/bte/uusb_ppc` | `UUSB_Write` | 0x8024D500 | 524 | matched |  |
| `revolution/bte/uusb_ppc` | `UUSB_Close` | 0x8024D710 | 172 | matched |  |
| `revolution/bte/uusb_ppc` | `UUSB_Unregister` | 0x8024D7C0 | 68 | matched |  |
| `revolution/bte/bte_hcisu` | `bte_hcisu_send` | 0x8024D810 | 32 | matched |  |
| `revolution/bte/bte_hcisu` | `bte_hcisu_task` | 0x8024D830 | 100 | matched |  |
| `revolution/bte/bte_hcisu` | `bte_hcisu_close` | 0x8024D8A0 | 36 | matched |  |
| `revolution/bte/bte_hcisu` | `bta_ci_hci_msg_handler` | 0x8024D8D0 | 16 | matched |  |
| `revolution/bte/bte_init` | `BTE_InitStack` | 0x8024D8E0 | 44 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg` | 0x8024D910 | 180 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_0` | 0x8024D9D0 | 24 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_1` | 0x8024D9F0 | 24 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_2` | 0x8024DA10 | 24 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_3` | 0x8024DA30 | 24 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_4` | 0x8024DA50 | 24 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_5` | 0x8024DA70 | 24 | matched |  |
| `revolution/bte/bte_logmsg` | `LogMsg_6` | 0x8024DA90 | 24 | matched |  |
| `revolution/bte/bte_main` | `BTUInterruptHandler` | 0x8024DAB0 | 24 | matched |  |
| `revolution/bte/bte_main` | `BTA_Init` | 0x8024DAD0 | 220 | matched |  |
| `revolution/bte/bte_main` | `BTA_CleanUp` | 0x8024DBB0 | 52 | matched |  |
| `revolution/bte/bte_main` | `bta_usb_close_evt` | 0x8024DBF0 | 48 | matched |  |
| `revolution/bte/btu_task1` | `btu_task_init` | 0x8024DC20 | 64 | matched |  |
| `revolution/bte/btu_task1` | `btu_task_msg_handler` | 0x8024DC60 | 892 | matched |  |
| `revolution/bte/btu_task1` | `btu_start_timer` | 0x8024DFE0 | 112 | matched |  |
| `revolution/bte/btu_task1` | `btu_stop_timer` | 0x8024E050 | 20 | matched |  |
| `revolution/bte/bd` | `bdcpy` | 0x8024E064 | 52 | matched |  |
| `revolution/bte/bd` | `bdcmp` | 0x8024E098 | 160 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_rm_register` | 0x8024E138 | 16 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_compress_register` | 0x8024E148 | 16 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_pm_register` | 0x8024E158 | 16 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_conn_open` | 0x8024E168 | 196 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_conn_close` | 0x8024E22C | 196 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_sco_close` | 0x8024E2F0 | 52 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_idle` | 0x8024E324 | 52 | matched |  |
| `revolution/bte/bta_sys_conn` | `bta_sys_busy` | 0x8024E358 | 52 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_init` | 0x8024E38C | 100 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_event` | 0x8024E3F0 | 236 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_timer_update` | 0x8024E4DC | 32 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_register` | 0x8024E4FC | 20 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_sendmsg` | 0x8024E510 | 28 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_start_timer` | 0x8024E52C | 32 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_stop_timer` | 0x8024E54C | 20 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_disable` | 0x8024E560 | 104 | matched |  |
| `revolution/bte/bta_sys_main` | `bta_sys_set_trace_level` | 0x8024E5C8 | 8 | matched |  |
| `revolution/bte/ptim` | `ptim_init` | 0x8024E5D0 | 76 | matched |  |
| `revolution/bte/ptim` | `ptim_timer_update` | 0x8024E61C | 180 | matched |  |
| `revolution/bte/ptim` | `ptim_start_timer` | 0x8024E6D0 | 160 | matched |  |
| `revolution/bte/ptim` | `ptim_stop_timer` | 0x8024E770 | 64 | matched |  |
| `revolution/bte/utl` | `utl_freebuf` | 0x8024E7B0 | 68 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_enable` | 0x8024E7F4 | 272 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_disable` | 0x8024E904 | 176 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_disable_timer_cback` | 0x8024E9B4 | 188 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_set_dev_name` | 0x8024EA70 | 8 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_set_visibility` | 0x8024EA78 | 72 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_bond` | 0x8024EAC0 | 112 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_pin_reply` | 0x8024EB30 | 156 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_auth_reply` | 0x8024EBCC | 212 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_start` | 0x8024ECA0 | 96 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_cancel` | 0x8024ED00 | 132 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_discover` | 0x8024ED84 | 276 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_inq_cmpl` | 0x8024EE98 | 548 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_rmt_name` | 0x8024F0BC | 364 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_disc_rmt_name` | 0x8024F228 | 100 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_sdp_result` | 0x8024F28C | 616 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_cmpl` | 0x8024F4F4 | 24 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_disc_result` | 0x8024F50C | 80 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_result` | 0x8024F55C | 136 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_timer_cback` | 0x8024F5E4 | 76 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_free_sdp_db` | 0x8024F630 | 68 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_queue_search` | 0x8024F674 | 72 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_queue_disc` | 0x8024F6BC | 72 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_clear_queue` | 0x8024F704 | 68 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_cancel_cmpl` | 0x8024F748 | 68 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_cancel_transac_cmpl` | 0x8024F78C | 92 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_search_cancel_notify` | 0x8024F7E8 | 24 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_find_services` | 0x8024F800 | 416 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_discover_next_device` | 0x8024F9A0 | 224 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_sdp_callback` | 0x8024FA80 | 72 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_inq_results_cb` | 0x8024FAC8 | 144 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_inq_cmpl_cb` | 0x8024FB58 | 76 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_service_search_remname_cback` | 0x8024FBA4 | 64 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_remname_cback` | 0x8024FBE4 | 140 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_disc_remname_cback` | 0x8024FC70 | 136 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_cancel_rmt_name` | 0x8024FCF8 | 4 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_authorize_cback` | 0x8024FCFC | 200 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_pinname_cback` | 0x8024FDC4 | 192 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_pin_cback` | 0x8024FE84 | 304 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_link_key_request_cback` | 0x8024FFB4 | 8 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_new_link_key_cback` | 0x8024FFBC | 156 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_authentication_complete_cback` | 0x80250058 | 132 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_local_addr_cback` | 0x802500DC | 40 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_signal_strength` | 0x80250104 | 60 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_signal_strength_timer_cback` | 0x80250140 | 244 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_acl_change_cback` | 0x80250234 | 112 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_acl_change` | 0x802502A4 | 640 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_disable_conn_down_timer_cback` | 0x80250524 | 28 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_rssi_cback` | 0x80250540 | 108 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_link_quality_cback` | 0x802505AC | 108 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_l2cap_server_compress_cback` | 0x80250618 | 228 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_compress_cback` | 0x802506FC | 568 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_rm_cback` | 0x80250934 | 264 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_keep_acl` | 0x80250A3C | 132 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_immediate_disable` | 0x80250AC0 | 28 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_reset_complete` | 0x80250ADC | 4 | matched |  |
| `revolution/bte/bta_dm_act` | `bta_dm_send_hci_reset` | 0x80250AE0 | 80 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_EnableBluetooth` | 0x80250B30 | 136 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DisableBluetooth` | 0x80250BB8 | 56 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmIsDeviceUp` | 0x80250BF0 | 56 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmSetDeviceName` | 0x80250C28 | 100 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmSetVisibility` | 0x80250C8C | 88 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmSearch` | 0x80250CE4 | 132 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmSearchCancel` | 0x80250D68 | 56 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmPinReply` | 0x80250DA0 | 140 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmAddDevice` | 0x80250E2C | 240 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmRemoveDevice` | 0x80250F1C | 72 | matched |  |
| `revolution/bte/bta_dm_api` | `BTA_DmSendHciReset` | 0x80250F64 | 56 | matched |  |
| `revolution/bte/bta_dm_main` | `bta_dm_sm_execute` | 0x80250F9C | 152 | matched |  |
| `revolution/bte/bta_dm_main` | `bta_dm_search_sm_execute` | 0x80251034 | 200 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_init_pm` | 0x802510FC | 104 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_disable_pm` | 0x80251164 | 24 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_pm_cback` | 0x8025117C | 896 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_pm_set_mode` | 0x802514FC | 896 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_pm_btm_cback` | 0x8025187C | 120 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_pm_timer_cback` | 0x802518F4 | 248 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_pm_btm_status` | 0x802519EC | 288 | matched |  |
| `revolution/bte/bta_dm_pm` | `bta_dm_pm_timer` | 0x80251B0C | 12 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_api_enable` | 0x80251B18 | 372 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_api_disable` | 0x80251C8C | 260 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_disc_cmpl` | 0x80251D90 | 164 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_sdp_cback` | 0x80251E34 | 308 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_start_sdp` | 0x80251F68 | 404 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_sdp_cmpl` | 0x802520FC | 360 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_api_disc_act` | 0x80252264 | 104 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_open_cmpl_act` | 0x802522CC | 296 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_open_act` | 0x802523F4 | 184 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_data_act` | 0x802524AC | 88 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_handsk_act` | 0x80252504 | 616 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_ctrl_dat_act` | 0x8025276C | 504 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_close_act` | 0x80252964 | 512 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_get_dscp_act` | 0x80252B64 | 28 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_maint_dev_act` | 0x80252B80 | 356 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_get_acl_q_info` | 0x80252CE4 | 284 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_write_dev_act` | 0x80252E00 | 456 | matched |  |
| `revolution/bte/bta_hh_act` | `bta_hh_cback` | 0x80252FC8 | 664 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhEnable` | 0x80253260 | 136 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhDisable` | 0x802532E8 | 56 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhClose` | 0x80253320 | 100 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhOpen` | 0x80253384 | 180 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhSendData` | 0x80253438 | 144 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhAddDev` | 0x802534C8 | 168 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhRemoveDev` | 0x80253570 | 108 | matched |  |
| `revolution/bte/bta_hh_api` | `BTA_HhGetAclQueueInfo` | 0x802535DC | 84 | matched |  |
| `revolution/bte/bta_hh_main` | `bta_hh_sm_execute` | 0x80253630 | 852 | matched |  |
| `revolution/bte/bta_hh_main` | `bta_hh_hdl_event` | 0x80253984 | 332 | matched |  |
| `revolution/bte/bta_hh_main` | `bta_hh_evt_code` | 0x80253AD0 | 180 | matched |  |
| `revolution/bte/bta_hh_utils` | `bta_hh_find_cb` | 0x80253B84 | 316 | matched |  |
| `revolution/bte/bta_hh_utils` | `bta_hh_clean_up_kdev` | 0x80253CC0 | 128 | matched |  |
| `revolution/bte/bta_hh_utils` | `bta_hh_add_device_to_list` | 0x80253D40 | 188 | matched |  |
| `revolution/bte/bta_hh_utils` | `bta_hh_tod_spt` | 0x80253DFC | 84 | matched |  |
| `revolution/bte/bta_hh_utils` | `bta_hh_trace_dev_db` | 0x80253E50 | 224 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_init` | 0x80253F30 | 28 | matched |  |
| `revolution/bte/btm_acl` | `btm_handle_to_acl_index` | 0x80253F4C | 132 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_created` | 0x80253FD0 | 788 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_removed` | 0x802542E4 | 196 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_device_down` | 0x802543A8 | 100 | matched |  |
| `revolution/bte/btm_acl` | `BTM_SwitchRole` | 0x8025440C | 540 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_encrypt_change` | 0x80254628 | 368 | matched |  |
| `revolution/bte/btm_acl` | `BTM_SetLinkPolicy` | 0x80254798 | 496 | matched |  |
| `revolution/bte/btm_acl` | `BTM_SetDefaultLinkPolicy` | 0x80254988 | 16 | matched |  |
| `revolution/bte/btm_acl` | `btm_read_link_policy_complete` | 0x80254998 | 260 | matched |  |
| `revolution/bte/btm_acl` | `btm_read_remote_version_complete` | 0x80254A9C | 140 | matched |  |
| `revolution/bte/btm_acl` | `btm_read_remote_features_complete` | 0x80254B28 | 556 | matched |  |
| `revolution/bte/btm_acl` | `BTM_SetDefaultLinkSuperTout` | 0x80254D54 | 16 | matched |  |
| `revolution/bte/btm_acl` | `BTM_IsAclConnectionUp` | 0x80254D64 | 216 | matched |  |
| `revolution/bte/btm_acl` | `BTM_GetNumAclLinks` | 0x80254E3C | 92 | matched |  |
| `revolution/bte/btm_acl` | `btm_get_acl_disc_reason_code` | 0x80254E98 | 16 | matched |  |
| `revolution/bte/btm_acl` | `BTM_GetHCIConnHandle` | 0x80254EA8 | 156 | matched |  |
| `revolution/bte/btm_acl` | `btm_process_clk_off_comp_evt` | 0x80254F44 | 156 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_role_changed` | 0x80254FE0 | 500 | matched |  |
| `revolution/bte/btm_acl` | `btm_acl_timeout` | 0x802551D4 | 92 | matched |  |
| `revolution/bte/btm_acl` | `btm_get_max_packet_size` | 0x80255230 | 416 | matched |  |
| `revolution/bte/btm_acl` | `BTM_AclRegisterForChanges` | 0x802553D0 | 68 | matched |  |
| `revolution/bte/btm_acl` | `btm_qos_setup_complete` | 0x80255414 | 216 | matched |  |
| `revolution/bte/btm_acl` | `BTM_ReadRSSI` | 0x802554EC | 316 | matched |  |
| `revolution/bte/btm_acl` | `BTM_ReadLinkQuality` | 0x80255628 | 316 | matched |  |
| `revolution/bte/btm_acl` | `btm_read_rssi_complete` | 0x80255764 | 284 | matched |  |
| `revolution/bte/btm_acl` | `btm_read_link_quality_complete` | 0x80255880 | 280 | matched |  |
| `revolution/bte/btm_acl` | `btm_remove_acl` | 0x80255998 | 188 | matched |  |
| `revolution/bte/btm_acl` | `btm_chg_all_acl_pkt_types` | 0x80255A54 | 544 | matched |  |
| `revolution/bte/btm_dev` | `BTM_SecAddDevice` | 0x80255C74 | 392 | matched |  |
| `revolution/bte/btm_dev` | `BTM_SecDeleteDevice` | 0x80255DFC | 172 | matched |  |
| `revolution/bte/btm_dev` | `BTM_SecReadDevName` | 0x80255EA8 | 160 | matched |  |
| `revolution/bte/btm_dev` | `btm_sec_alloc_dev` | 0x80255F48 | 288 | matched |  |
| `revolution/bte/btm_dev` | `btm_find_dev_by_handle` | 0x80256068 | 184 | matched |  |
| `revolution/bte/btm_dev` | `btm_find_dev` | 0x80256120 | 136 | matched |  |
| `revolution/bte/btm_dev` | `btm_find_oldest_dev` | 0x802561A8 | 408 | matched |  |
| `revolution/bte/btm_devctl` | `btm_dev_init` | 0x80256340 | 160 | matched |  |
| `revolution/bte/btm_devctl` | `btm_db_reset` | 0x802563E0 | 176 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_DeviceReset` | 0x80256490 | 156 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_SendHciReset` | 0x8025652C | 168 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_IsDeviceUp` | 0x802565D4 | 28 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_SetAfhChannels` | 0x802565F0 | 240 | matched |  |
| `revolution/bte/btm_devctl` | `btm_dev_timeout` | 0x802566E0 | 664 | matched |  |
| `revolution/bte/btm_devctl` | `btm_reset_complete` | 0x80256978 | 452 | matched |  |
| `revolution/bte/btm_devctl` | `btm_read_hci_buf_size_complete` | 0x80256B3C | 200 | matched |  |
| `revolution/bte/btm_devctl` | `btm_read_local_version_complete` | 0x80256C04 | 188 | matched |  |
| `revolution/bte/btm_devctl` | `btm_read_local_features_complete` | 0x80256CC0 | 1276 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_SetLocalDeviceName` | 0x802571BC | 184 | matched |  |
| `revolution/bte/btm_devctl` | `btm_read_local_name_complete` | 0x80257274 | 140 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_ReadLocalDeviceAddr` | 0x80257300 | 64 | matched |  |
| `revolution/bte/btm_devctl` | `btm_read_local_addr_complete` | 0x80257340 | 72 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_ReadLocalVersion` | 0x80257388 | 76 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_SetDeviceClass` | 0x802573D4 | 128 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_ReadDeviceClass` | 0x80257454 | 16 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_ReadLocalFeatures` | 0x80257464 | 16 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_RegisterForDeviceStatusNotif` | 0x80257474 | 24 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_VendorSpecificCommand` | 0x8025748C | 252 | matched |  |
| `revolution/bte/btm_devctl` | `btm_vsc_complete` | 0x80257588 | 108 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_RegisterForVSEvents` | 0x802575F4 | 56 | matched |  |
| `revolution/bte/btm_devctl` | `btm_vendor_specific_evt` | 0x8025762C | 152 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_WritePageTimeout` | 0x802576C4 | 160 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_ReadStoredLinkKey` | 0x80257764 | 216 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_WriteStoredLinkKey` | 0x8025783C | 208 | matched |  |
| `revolution/bte/btm_devctl` | `BTM_DeleteStoredLinkKey` | 0x8025790C | 208 | matched |  |
| `revolution/bte/btm_devctl` | `btm_read_stored_link_key_complete` | 0x802579DC | 124 | matched |  |
| `revolution/bte/btm_devctl` | `btm_write_stored_link_key_complete` | 0x80257A58 | 92 | matched |  |
| `revolution/bte/btm_devctl` | `btm_delete_stored_link_key_complete` | 0x80257AB4 | 104 | matched |  |
| `revolution/bte/btm_devctl` | `btm_return_link_keys_evt` | 0x80257B1C | 396 | matched |  |
| `revolution/bte/btm_devctl` | `btm_report_device_status` | 0x80257CA8 | 32 | matched |  |
| `revolution/bte/btm_discovery` | `btm_discovery_db_init` | 0x80257CC8 | 120 | matched |  |
| `revolution/bte/btm_discovery` | `btm_discovery_db_reset` | 0x80257D40 | 184 | matched |  |
| `revolution/bte/btm_discovery` | `btm_discovery_timeout` | 0x80257DF8 | 4 | matched |  |
| `revolution/bte/btm_inq` | `BTM_SetDiscoverability` | 0x80257DFC | 644 | matched |  |
| `revolution/bte/btm_inq` | `BTM_SetInquiryScanType` | 0x80258080 | 176 | matched |  |
| `revolution/bte/btm_inq` | `BTM_SetPageScanType` | 0x80258130 | 176 | matched |  |
| `revolution/bte/btm_inq` | `BTM_SetInquiryMode` | 0x802581E0 | 152 | matched |  |
| `revolution/bte/btm_inq` | `BTM_SetConnectability` | 0x80258278 | 392 | matched |  |
| `revolution/bte/btm_inq` | `BTM_IsInquiryActive` | 0x80258400 | 16 | matched |  |
| `revolution/bte/btm_inq` | `BTM_CancelInquiry` | 0x80258410 | 264 | matched |  |
| `revolution/bte/btm_inq` | `BTM_StartInquiry` | 0x80258518 | 436 | matched |  |
| `revolution/bte/btm_inq` | `BTM_ReadRemoteDeviceName` | 0x802586CC | 232 | matched |  |
| `revolution/bte/btm_inq` | `BTM_CancelRemoteDeviceName` | 0x802587B4 | 120 | matched |  |
| `revolution/bte/btm_inq` | `BTM_InqDbRead` | 0x8025882C | 192 | matched |  |
| `revolution/bte/btm_inq` | `BTM_InqDbFirst` | 0x802588EC | 184 | matched |  |
| `revolution/bte/btm_inq` | `BTM_InqDbNext` | 0x802589A4 | 316 | matched |  |
| `revolution/bte/btm_inq` | `BTM_ClearInqDb` | 0x80258AE0 | 208 | matched |  |
| `revolution/bte/btm_inq` | `btm_inq_db_reset` | 0x80258BB0 | 408 | matched |  |
| `revolution/bte/btm_inq` | `btm_inq_db_init` | 0x80258D48 | 4 | matched |  |
| `revolution/bte/btm_inq` | `btm_inq_find_bdaddr` | 0x80258D4C | 220 | matched |  |
| `revolution/bte/btm_inq` | `btm_inq_db_new` | 0x80258E28 | 260 | matched |  |
| `revolution/bte/btm_inq` | `btm_set_inq_event_filter` | 0x80258F2C | 228 | matched |  |
| `revolution/bte/btm_inq` | `btm_event_filter_complete` | 0x80259010 | 544 | matched |  |
| `revolution/bte/btm_inq` | `btm_process_inq_results` | 0x80259230 | 632 | matched |  |
| `revolution/bte/btm_inq` | `btm_process_inq_complete` | 0x802594A8 | 260 | matched |  |
| `revolution/bte/btm_inq` | `btm_initiate_rem_name` | 0x802595AC | 336 | matched |  |
| `revolution/bte/btm_inq` | `btm_process_remote_name` | 0x802596FC | 344 | matched |  |
| `revolution/bte/btm_inq` | `btm_inq_rmt_name_failed` | 0x80259854 | 64 | matched |  |
| `revolution/bte/btm_main` | `btm_init` | 0x80259894 | 92 | matched |  |
| `revolution/bte/btm_pm` | `BTM_PmRegister` | 0x802598F0 | 188 | matched |  |
| `revolution/bte/btm_pm` | `BTM_SetPowerMode` | 0x802599AC | 464 | matched |  |
| `revolution/bte/btm_pm` | `BTM_ReadPowerMode` | 0x80259B7C | 184 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_reset` | 0x80259C34 | 120 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_sm_alloc` | 0x80259CAC | 80 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_compare_modes` | 0x80259CFC | 540 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_get_set_mode` | 0x80259F18 | 352 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_snd_md_req` | 0x8025A078 | 564 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_proc_cmd_status` | 0x8025A2AC | 176 | matched |  |
| `revolution/bte/btm_pm` | `btm_pm_proc_mode_change` | 0x8025A35C | 552 | matched |  |
| `revolution/bte/btm_sco` | `btm_sco_init` | 0x8025A584 | 68 | matched |  |
| `revolution/bte/btm_sco` | `btm_esco_conn_rsp` | 0x8025A5C8 | 476 | matched |  |
| `revolution/bte/btm_sco` | `btm_sco_chk_pend_unpark` | 0x8025A7A4 | 396 | matched |  |
| `revolution/bte/btm_sco` | `btm_sco_conn_req` | 0x8025A930 | 480 | matched |  |
| `revolution/bte/btm_sco` | `btm_sco_connected` | 0x8025AB10 | 416 | matched |  |
| `revolution/bte/btm_sco` | `BTM_RemoveSco` | 0x8025ACB0 | 184 | matched |  |
| `revolution/bte/btm_sco` | `btm_remove_sco_links` | 0x8025AD68 | 132 | matched |  |
| `revolution/bte/btm_sco` | `btm_sco_removed` | 0x8025ADEC | 276 | matched |  |
| `revolution/bte/btm_sco` | `btm_sco_acl_removed` | 0x8025AF00 | 180 | matched |  |
| `revolution/bte/btm_sco` | `btm_route_sco_data` | 0x8025AFB4 | 4 | matched |  |
| `revolution/bte/btm_sco` | `BTM_ChangeEScoLinkParms` | 0x8025AFB8 | 424 | matched |  |
| `revolution/bte/btm_sco` | `btm_esco_proc_conn_chg` | 0x8025B160 | 260 | matched |  |
| `revolution/bte/btm_sco` | `btm_is_sco_active` | 0x8025B264 | 112 | matched |  |
| `revolution/bte/btm_sco` | `btm_num_sco_links_active` | 0x8025B2D4 | 96 | matched |  |
| `revolution/bte/btm_sco` | `btm_is_sco_active_by_bdaddr` | 0x8025B334 | 136 | matched |  |
| `revolution/bte/btm_sec` | `BTM_SecRegister` | 0x8025B3BC | 120 | matched |  |
| `revolution/bte/btm_sec` | `BTM_SecAddRmtNameNotifyCallback` | 0x8025B434 | 80 | matched |  |
| `revolution/bte/btm_sec` | `BTM_SecDeleteRmtNameNotifyCallback` | 0x8025B484 | 84 | matched |  |
| `revolution/bte/btm_sec` | `BTM_SetPinType` | 0x8025B4D8 | 176 | matched |  |
| `revolution/bte/btm_sec` | `BTM_SetSecurityLevel` | 0x8025B588 | 552 | matched |  |
| `revolution/bte/btm_sec` | `BTM_PINCodeReply` | 0x8025B7B0 | 196 | matched |  |
| `revolution/bte/btm_sec` | `BTM_DeviceAuthorized` | 0x8025B874 | 476 | matched |  |
| `revolution/bte/btm_sec` | `BTM_SecBond` | 0x8025BA50 | 972 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_l2cap_access_req` | 0x8025BE1C | 1124 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_mx_access_request` | 0x8025C280 | 668 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_conn_req` | 0x8025C51C | 300 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_init` | 0x8025C648 | 28 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_dev_reset` | 0x8025C664 | 64 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_abort_access_req` | 0x8025C6A4 | 180 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_rmt_name_request_complete` | 0x8025C758 | 744 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_auth_complete` | 0x8025CA40 | 836 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_mkey_comp_event` | 0x8025CD84 | 252 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_encrypt_change` | 0x8025CE80 | 316 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_is_bonding` | 0x8025CFBC | 64 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_connected` | 0x8025CFFC | 1152 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_disconnect` | 0x8025D47C | 188 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_disconnected` | 0x8025D538 | 316 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_link_key_notification` | 0x8025D674 | 428 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_link_key_request` | 0x8025D820 | 304 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_pin_code_request_timeout` | 0x8025D950 | 116 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_pin_code_request` | 0x8025D9C4 | 860 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_update_clock_offset` | 0x8025DD20 | 80 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_execute_procedure` | 0x8025DD70 | 992 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_start_authorization` | 0x8025E150 | 284 | matched |  |
| `revolution/bte/btm_sec` | `btm_sec_collision_timeout` | 0x8025E26C | 224 | matched |  |
| `revolution/bte/btm_sec` | `btm_read_trusted_mask` | 0x8025E34C | 52 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_process_event` | 0x8025E380 | 1464 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_send_cmd` | 0x8025E938 | 396 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_connection_comp_evt` | 0x8025EAC4 | 220 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_connection_request_evt` | 0x8025EBA0 | 140 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_qos_setup_comp_evt` | 0x8025EC2C | 256 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_esco_connection_comp_evt` | 0x8025ED2C | 224 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_hdl_command_complete` | 0x8025EE0C | 344 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_command_complete_evt` | 0x8025EF64 | 280 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_hdl_command_status` | 0x8025F07C | 512 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_command_status_evt` | 0x8025F27C | 296 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_cmd_timeout` | 0x8025F3A4 | 344 | matched |  |
| `revolution/bte/btu_hcif` | `btu_hcif_link_key_notification_evt` | 0x8025F4FC | 224 | matched |  |
| `revolution/bte/btu_init` | `btu_init_core` | 0x8025F5DC | 44 | matched |  |
| `revolution/bte/btu_init` | `BTE_Init` | 0x8025F608 | 76 | matched |  |
| `revolution/bte/wbt_ext` | `WBT_ExtCreateRecord` | 0x8025F654 | 224 | matched |  |
| `revolution/bte/gap_api` | `GAP_Init` | 0x8025F734 | 96 | matched |  |
| `revolution/bte/gap_conn` | `gap_conn_init` | 0x8025F794 | 112 | matched |  |
| `revolution/bte/gap_conn` | `gap_connect_ind` | 0x8025F804 | 396 | matched |  |
| `revolution/bte/gap_conn` | `gap_connect_cfm` | 0x8025F990 | 476 | matched |  |
| `revolution/bte/gap_conn` | `gap_config_ind` | 0x8025FB6C | 316 | matched |  |
| `revolution/bte/gap_conn` | `gap_config_cfm` | 0x8025FCA8 | 492 | matched |  |
| `revolution/bte/gap_conn` | `gap_disconnect_ind` | 0x8025FE94 | 504 | matched |  |
| `revolution/bte/gap_conn` | `gap_data_ind` | 0x8026008C | 272 | matched |  |
| `revolution/bte/gap_conn` | `gap_congestion_ind` | 0x8026019C | 308 | matched |  |
| `revolution/bte/gap_utils` | `btm_cback` | 0x802602D0 | 472 | matched |  |
| `revolution/bte/gap_utils` | `gap_btm_cback0` | 0x802604A8 | 12 | matched |  |
| `revolution/bte/gap_utils` | `gap_btm_cback1` | 0x802604B4 | 12 | matched |  |
| `revolution/bte/gap_utils` | `gap_find_addr_name_cb` | 0x802604C0 | 536 | matched |  |
| `revolution/bte/gap_utils` | `gap_find_addr_inq_cb` | 0x802606D8 | 436 | matched |  |
| `revolution/bte/gap_utils` | `gap_convert_btm_status` | 0x8026088C | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_inquiry` | 0x802608EC | 168 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_inq_cancel` | 0x80260994 | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_per_inq_mode` | 0x802609F4 | 192 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_create_conn` | 0x80260AB4 | 224 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_disconnect` | 0x80260B94 | 136 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_add_SCO_conn` | 0x80260C1C | 140 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_accept_conn` | 0x80260CA8 | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_reject_conn` | 0x80260D08 | 92 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_link_key_req_reply` | 0x80260D64 | 416 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_link_key_neg_reply` | 0x80260F04 | 160 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_pin_code_req_reply` | 0x80260FA4 | 484 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_pin_code_neg_reply` | 0x80261188 | 160 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_change_conn_type` | 0x80261228 | 144 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_auth_request` | 0x802612B8 | 124 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_set_conn_encrypt` | 0x80261334 | 140 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_rmt_name_req` | 0x802613C0 | 216 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_rmt_name_req_cancel` | 0x80261498 | 160 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_rmt_features_req` | 0x80261538 | 124 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_rmt_ver_req` | 0x802615B4 | 124 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_rmt_clk_offset` | 0x80261630 | 124 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_setup_esco_conn` | 0x802616AC | 252 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_accept_esco_conn` | 0x802617A8 | 232 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_reject_esco_conn` | 0x80261890 | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_hold_mode` | 0x802618F0 | 180 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_sniff_mode` | 0x802619A4 | 204 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_exit_sniff_mode` | 0x80261A70 | 132 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_park_mode` | 0x80261AF4 | 180 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_exit_park_mode` | 0x80261BA8 | 132 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_switch_role` | 0x80261C2C | 176 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_policy_set` | 0x80261CDC | 148 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_reset` | 0x80261D70 | 92 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_set_event_filter` | 0x80261DCC | 444 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_pin_type` | 0x80261F88 | 116 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_stored_key` | 0x80261FFC | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_stored_key` | 0x8026205C | 448 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_delete_stored_key` | 0x8026221C | 176 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_change_name` | 0x802622CC | 320 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_page_tout` | 0x8026240C | 56 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_scan_enable` | 0x80262444 | 48 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_pagescan_cfg` | 0x80262474 | 68 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_inqscan_cfg` | 0x802624B8 | 68 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_auth_enable` | 0x802624FC | 116 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_encr_mode` | 0x80262570 | 116 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_dev_class` | 0x802625E4 | 68 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_auto_flush_tout` | 0x80262628 | 68 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_set_host_buf_size` | 0x8026266C | 188 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_link_super_tout` | 0x80262728 | 148 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_cur_iac_lap` | 0x802627BC | 112 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_local_ver` | 0x8026282C | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_local_features` | 0x8026288C | 92 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_buffer_size` | 0x802628E8 | 40 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_bd_addr` | 0x80262910 | 96 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_get_link_quality` | 0x80262970 | 124 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_read_rssi` | 0x802629EC | 120 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_set_afh_channels` | 0x80262A64 | 1056 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_inqscan_type` | 0x80262E84 | 48 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_inquiry_mode` | 0x80262EB4 | 48 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_write_pagescan_type` | 0x80262EE4 | 48 | matched |  |
| `revolution/bte/hcicmds` | `btsnd_hcic_vendor_spec_cmd` | 0x80262F14 | 244 | matched |  |
| `revolution/bte/hidd_api` | `HID_DevInit` | 0x80263008 | 104 | matched |  |
| `revolution/bte/hidd_conn` | `hidd_conn_initiate` | 0x80263070 | 192 | matched |  |
| `revolution/bte/hidd_mgmt` | `hidd_proc_repage_timeout` | 0x80263130 | 200 | matched |  |
| `revolution/bte/hidd_pm` | `hidd_pm_set_now` | 0x802631F8 | 472 | matched |  |
| `revolution/bte/hidd_pm` | `hidd_pm_proc_mode_change` | 0x802633D0 | 344 | matched |  |
| `revolution/bte/hidd_pm` | `hidd_pm_inact_timeout` | 0x80263528 | 76 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostGetSDPRecord` | 0x80263574 | 200 | matched |  |
| `revolution/bte/hidh_api` | `hidh_search_callback` | 0x8026363C | 1104 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostInit` | 0x80263A8C | 68 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostRegister` | 0x80263AD0 | 124 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostDeregister` | 0x80263B4C | 228 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostAddDev` | 0x80263C30 | 400 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostRemoveDev` | 0x80263DC0 | 200 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostOpenDev` | 0x80263E88 | 100 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostWriteDev` | 0x80263EEC | 320 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostCloseDev` | 0x8026402C | 160 | matched |  |
| `revolution/bte/hidh_api` | `HID_HostSetSecurityLevel` | 0x802640CC | 552 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_conn_reg` | 0x802642F4 | 344 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_conn_disconnect` | 0x8026444C | 164 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_sec_check_complete_term` | 0x802644F0 | 304 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_connect_ind` | 0x80264620 | 600 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_proc_repage_timeout` | 0x80264878 | 332 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_sec_check_complete_orig` | 0x802649C4 | 532 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_connect_cfm` | 0x80264BD8 | 840 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_config_ind` | 0x80264F20 | 680 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_config_cfm` | 0x802651C8 | 780 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_disconnect_ind` | 0x802654D4 | 708 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_disconnect_cfm` | 0x80265798 | 584 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_cong_ind` | 0x802659E0 | 504 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_l2cif_data_ind` | 0x80265BD8 | 920 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_conn_snd_data` | 0x80265F70 | 668 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_conn_initiate` | 0x8026620C | 252 | matched |  |
| `revolution/bte/hidh_conn` | `hidh_conn_dereg` | 0x80266308 | 44 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_Register` | 0x80266334 | 412 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_Deregister` | 0x802664D0 | 144 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_ConnectReq` | 0x80266560 | 564 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_ConnectRsp` | 0x80266794 | 400 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_ConfigReq` | 0x80266924 | 184 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_ConfigRsp` | 0x802669DC | 208 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_DisconnectReq` | 0x80266AAC | 164 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_DisconnectRsp` | 0x80266B50 | 164 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_DataWrite` | 0x80266BF4 | 292 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_SetIdleTimeout` | 0x80266D18 | 200 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_SetIdleTimeoutByBdAddr` | 0x80266DE0 | 256 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_SetTraceLevel` | 0x80266EE0 | 28 | matched |  |
| `revolution/bte/l2c_api` | `L2CA_RegisterCompression` | 0x80266EFC | 4 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_execute` | 0x80266F00 | 76 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_closed` | 0x80266F4C | 660 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_orig_w4_sec_comp` | 0x802671E0 | 368 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_term_w4_sec_comp` | 0x80267350 | 360 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_w4_l2cap_connect_rsp` | 0x802674B8 | 580 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_w4_l2ca_connect_rsp` | 0x802676FC | 472 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_config` | 0x802678D4 | 968 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_open` | 0x80267C9C | 604 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_w4_l2cap_disconnect_rsp` | 0x80267EF8 | 484 | matched |  |
| `revolution/bte/l2c_csm` | `l2c_csm_w4_l2ca_disconnect_rsp` | 0x802680DC | 348 | matched |  |
| `revolution/bte/l2c_csm` | `forward_peer_data` | 0x80268238 | 420 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_hci_conn_req` | 0x802683DC | 340 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_hci_conn_comp` | 0x80268530 | 568 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_sec_comp` | 0x80268768 | 180 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_hci_disc_comp` | 0x8026881C | 188 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_hci_qos_violation` | 0x802688D8 | 112 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_timeout` | 0x80268948 | 372 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_send_to_lower` | 0x80268ABC | 264 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_check_send_pkts` | 0x80268BC4 | 716 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_adjust_allocation` | 0x80268E90 | 408 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_process_num_completed_pkts` | 0x80269028 | 228 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_processs_num_bufs` | 0x8026910C | 20 | matched |  |
| `revolution/bte/l2c_link` | `l2cap_link_chk_pkt_start` | 0x80269120 | 464 | matched |  |
| `revolution/bte/l2c_link` | `l2cap_link_chk_pkt_end` | 0x802692F0 | 108 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_role_changed` | 0x8026935C | 128 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_role_change_failed` | 0x802693DC | 108 | matched |  |
| `revolution/bte/l2c_link` | `l2c_link_segments_xmitted` | 0x80269448 | 220 | matched |  |
| `revolution/bte/l2c_link` | `l2c_pin_code_request` | 0x80269524 | 68 | matched |  |
| `revolution/bte/l2c_main` | `l2c_init` | 0x80269568 | 276 | matched |  |
| `revolution/bte/l2c_main` | `l2c_rcv_acl_data` | 0x8026967C | 816 | matched |  |
| `revolution/bte/l2c_main` | `process_l2cap_cmd` | 0x802699AC | 2672 | matched |  |
| `revolution/bte/l2c_main` | `l2c_process_timeout` | 0x8026A41C | 80 | matched |  |
| `revolution/bte/l2c_main` | `l2c_process_held_packets` | 0x8026A46C | 300 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_allocate_lcb` | 0x8026A598 | 212 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_release_lcb` | 0x8026A66C | 256 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_find_lcb_by_bd_addr` | 0x8026A76C | 136 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_find_lcb_by_handle` | 0x8026A7F4 | 144 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_get_conn_role` | 0x8026A884 | 16 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_build_header` | 0x8026A894 | 196 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_cmd_reject` | 0x8026A958 | 252 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_connect_req` | 0x8026AA54 | 196 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_connect_rsp` | 0x8026AB18 | 216 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_reject_connection` | 0x8026ABF0 | 192 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_config_req` | 0x8026ACB0 | 760 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_config_rsp` | 0x8026AFA8 | 760 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_config_rej` | 0x8026B2A0 | 436 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_disc_req` | 0x8026B454 | 212 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_disc_rsp` | 0x8026B528 | 192 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_echo_req` | 0x8026B5E8 | 368 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_echo_rsp` | 0x8026B758 | 412 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_send_peer_info_rsp` | 0x8026B8F4 | 164 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_allocate_ccb` | 0x8026B998 | 268 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_release_ccb` | 0x8026BAA4 | 420 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_find_ccb_by_cid` | 0x8026BC48 | 92 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_allocate_rcb` | 0x8026BCA4 | 76 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_release_rcb` | 0x8026BCF0 | 16 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_find_rcb_by_psm` | 0x8026BD00 | 184 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_process_peer_cfg_req` | 0x8026BDB8 | 328 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_process_peer_cfg_rsp` | 0x8026BF00 | 64 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_process_our_cfg_req` | 0x8026BF40 | 272 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_process_our_cfg_rsp` | 0x8026C050 | 64 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_device_reset` | 0x8026C090 | 108 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_create_conn` | 0x8026C0FC | 280 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_create_conn_after_switch` | 0x8026C214 | 208 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_find_lcb_by_state` | 0x8026C2E4 | 144 | matched |  |
| `revolution/bte/l2c_utils` | `l2cu_lcb_disconnecting` | 0x8026C374 | 232 | matched |  |
| `revolution/bte/port_api` | `RFCOMM_Init` | 0x8026C45C | 80 | matched |  |
| `revolution/bte/port_rfc` | `PORT_StartCnf` | 0x8026C4AC | 352 | matched |  |
| `revolution/bte/port_rfc` | `PORT_StartInd` | 0x8026C60C | 156 | matched |  |
| `revolution/bte/port_rfc` | `PORT_ParNegInd` | 0x8026C6A8 | 432 | matched |  |
| `revolution/bte/port_rfc` | `PORT_ParNegCnf` | 0x8026C858 | 260 | matched |  |
| `revolution/bte/port_rfc` | `PORT_DlcEstablishInd` | 0x8026C95C | 320 | matched |  |
| `revolution/bte/port_rfc` | `PORT_DlcEstablishCnf` | 0x8026CA9C | 324 | matched |  |
| `revolution/bte/port_rfc` | `PORT_PortNegInd` | 0x8026CBE0 | 336 | matched |  |
| `revolution/bte/port_rfc` | `PORT_PortNegCnf` | 0x8026CD30 | 280 | matched |  |
| `revolution/bte/port_rfc` | `PORT_ControlInd` | 0x8026CE48 | 376 | matched |  |
| `revolution/bte/port_rfc` | `PORT_ControlCnf` | 0x8026CFC0 | 196 | matched |  |
| `revolution/bte/port_rfc` | `PORT_LineStatusInd` | 0x8026D084 | 200 | matched |  |
| `revolution/bte/port_rfc` | `PORT_DlcReleaseInd` | 0x8026D14C | 104 | matched |  |
| `revolution/bte/port_rfc` | `PORT_CloseInd` | 0x8026D1B4 | 160 | matched |  |
| `revolution/bte/port_rfc` | `Port_TimeOutCloseMux` | 0x8026D254 | 152 | matched |  |
| `revolution/bte/port_rfc` | `PORT_DataInd` | 0x8026D2EC | 512 | matched |  |
| `revolution/bte/port_rfc` | `PORT_FlowInd` | 0x8026D4EC | 304 | matched |  |
| `revolution/bte/port_rfc` | `port_rfc_send_tx_data` | 0x8026D61C | 248 | matched |  |
| `revolution/bte/port_rfc` | `port_rfc_closed` | 0x8026D714 | 404 | matched |  |
| `revolution/bte/port_utils` | `port_select_mtu` | 0x8026D8A8 | 380 | matched |  |
| `revolution/bte/port_utils` | `port_release_port` | 0x8026DA24 | 220 | matched |  |
| `revolution/bte/port_utils` | `port_find_mcb_dlci_port` | 0x8026DB00 | 84 | matched |  |
| `revolution/bte/port_utils` | `port_find_dlci_port` | 0x8026DB54 | 136 | matched |  |
| `revolution/bte/port_utils` | `port_flow_control_user` | 0x8026DBDC | 112 | matched |  |
| `revolution/bte/port_utils` | `port_get_signal_changes` | 0x8026DC4C | 104 | matched |  |
| `revolution/bte/port_utils` | `port_flow_control_peer` | 0x8026DCB4 | 460 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `rfcomm_l2cap_if_init` | 0x8026DE80 | 124 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_ConnectInd` | 0x8026DEFC | 140 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_ConnectCnf` | 0x8026DF88 | 260 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_ConfigInd` | 0x8026E08C | 264 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_ConfigCnf` | 0x8026E194 | 264 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_QoSViolationInd` | 0x8026E29C | 4 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_DisconnectInd` | 0x8026E2A0 | 292 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_BufDataInd` | 0x8026E3C4 | 672 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `RFCOMM_CongestionStatusInd` | 0x8026E664 | 308 | matched |  |
| `revolution/bte/rfc_l2cap_if` | `rfc_save_lcid_mcb` | 0x8026E798 | 28 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_execute` | 0x8026E7B4 | 68 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_state_idle` | 0x8026E7F8 | 532 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_state_wait_conn_cnf` | 0x8026EA0C | 444 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_state_configure` | 0x8026EBC8 | 296 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_sabme_wait_ua` | 0x8026ECF0 | 372 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_state_wait_sabme` | 0x8026EE64 | 288 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_state_connected` | 0x8026EF84 | 280 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_sm_state_disc_wait_ua` | 0x8026F09C | 376 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_conf_cnf` | 0x8026F214 | 244 | matched |  |
| `revolution/bte/rfc_mx_fsm` | `rfc_mx_conf_ind` | 0x8026F308 | 240 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_execute` | 0x8026F3F8 | 144 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_state_closed` | 0x8026F488 | 372 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_sabme_wait_ua` | 0x8026F5FC | 412 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_term_wait_sec_check` | 0x8026F798 | 444 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_orig_wait_sec_check` | 0x8026F954 | 336 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_opened` | 0x8026FAA4 | 492 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_port_sm_disc_wait_ua` | 0x8026FC90 | 284 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_pn` | 0x8026FDAC | 232 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_rpn` | 0x8026FE94 | 780 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_msc` | 0x802701A0 | 364 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_rls` | 0x8027030C | 140 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_nsc` | 0x80270398 | 4 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_test_rsp` | 0x8027039C | 8 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_fcon` | 0x802703A4 | 100 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_fcoff` | 0x80270408 | 100 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_process_l2cap_congestion` | 0x8027046C | 136 | matched |  |
| `revolution/bte/rfc_port_fsm` | `rfc_set_port_state` | 0x802704F4 | 164 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_StartRsp` | 0x80270598 | 44 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_DlcEstablishReq` | 0x802705C4 | 104 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_DlcEstablishRsp` | 0x8027062C | 112 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_ParNegReq` | 0x8027069C | 224 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_ParNegRsp` | 0x8027077C | 40 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_PortNegReq` | 0x802707A4 | 188 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_PortNegRsp` | 0x80270860 | 32 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_ControlReq` | 0x80270880 | 164 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_FlowReq` | 0x80270924 | 164 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_LineStatusReq` | 0x802709C8 | 152 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_DlcReleaseReq` | 0x80270A60 | 44 | matched |  |
| `revolution/bte/rfc_port_if` | `RFCOMM_DataReq` | 0x80270A8C | 56 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_sabme` | 0x80270AC4 | 176 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_ua` | 0x80270B74 | 176 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_dm` | 0x80270C24 | 196 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_disc` | 0x80270CE8 | 176 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_buf_uih` | 0x80270D98 | 408 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_pn` | 0x80270F30 | 232 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_fcon` | 0x80271018 | 132 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_fcoff` | 0x8027109C | 132 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_msc` | 0x80271120 | 288 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_rls` | 0x80271240 | 176 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_rpn` | 0x802712F0 | 284 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_test` | 0x8027140C | 168 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_send_credit` | 0x802714B4 | 192 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_parse_data` | 0x80271574 | 1396 | matched |  |
| `revolution/bte/rfc_ts_frames` | `rfc_process_mx_message` | 0x80271AE8 | 1572 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_calc_fcs` | 0x8027210C | 56 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_check_fcs` | 0x80272144 | 76 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_alloc_multiplexer_channel` | 0x80272190 | 344 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_release_multiplexer_channel` | 0x802722E8 | 136 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_timer_start` | 0x80272370 | 116 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_timer_stop` | 0x802723E4 | 88 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_port_timer_start` | 0x8027243C | 128 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_port_timer_stop` | 0x802724BC | 88 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_check_mcb_active` | 0x80272514 | 196 | matched |  |
| `revolution/bte/rfc_utils` | `rfcomm_process_timeout` | 0x802725D8 | 60 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_sec_check_complete` | 0x80272614 | 84 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_port_closed` | 0x80272668 | 344 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_inc_credit` | 0x802727C0 | 140 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_dec_credit` | 0x8027284C | 60 | matched |  |
| `revolution/bte/rfc_utils` | `rfc_check_send_cmd` | 0x80272888 | 100 | matched |  |
| `revolution/bte/sdp_api` | `SDP_InitDiscoveryDb` | 0x802728EC | 888 | matched |  |
| `revolution/bte/sdp_api` | `SDP_ServiceSearchRequest` | 0x80272C64 | 92 | matched |  |
| `revolution/bte/sdp_api` | `SDP_ServiceSearchAttributeRequest` | 0x80272CC0 | 100 | matched |  |
| `revolution/bte/sdp_api` | `SDP_FindAttributeInRec` | 0x80272D24 | 40 | matched |  |
| `revolution/bte/sdp_api` | `SDP_FindServiceInDb` | 0x80272D4C | 204 | matched |  |
| `revolution/bte/sdp_api` | `SDP_FindServiceUUIDInDb` | 0x80272E18 | 264 | matched |  |
| `revolution/bte/sdp_api` | `SDP_SetLocalDiRecord` | 0x80272F20 | 864 | matched |  |
| `revolution/bte/sdp_api` | `SDP_GetLocalDiRecord` | 0x80273280 | 1200 | matched |  |
| `revolution/bte/sdp_api` | `SDP_SetTraceLevel` | 0x80273730 | 36 | matched |  |
| `revolution/bte/sdp_db` | `sdp_db_service_search` | 0x80273754 | 316 | matched |  |
| `revolution/bte/sdp_db` | `find_uuid_in_seq` | 0x80273890 | 216 | matched |  |
| `revolution/bte/sdp_db` | `sdp_db_find_record` | 0x80273968 | 92 | matched |  |
| `revolution/bte/sdp_db` | `sdp_db_find_attr_in_rec` | 0x802739C4 | 64 | matched |  |
| `revolution/bte/sdp_db` | `SDP_CreateRecord` | 0x80273A04 | 224 | matched |  |
| `revolution/bte/sdp_db` | `SDP_DeleteRecord` | 0x80273AE4 | 276 | matched |  |
| `revolution/bte/sdp_db` | `SDP_AddAttribute` | 0x80273BF8 | 920 | matched |  |
| `revolution/bte/sdp_db` | `SDP_AddUuidSequence` | 0x80273F90 | 216 | matched |  |
| `revolution/bte/sdp_db` | `SDP_AddServiceClassIdList` | 0x80274068 | 380 | matched |  |
| `revolution/bte/sdp_db` | `SDP_DeleteAttribute` | 0x802741E4 | 572 | matched |  |
| `revolution/bte/sdp_discovery` | `sdpu_build_uuid_seq` | 0x80274420 | 236 | matched |  |
| `revolution/bte/sdp_discovery` | `sdp_snd_service_search_req` | 0x8027450C | 304 | matched |  |
| `revolution/bte/sdp_discovery` | `sdp_disc_connected` | 0x8027463C | 52 | matched |  |
| `revolution/bte/sdp_discovery` | `sdp_disc_server_rsp` | 0x80274670 | 320 | matched |  |
| `revolution/bte/sdp_discovery` | `process_service_search_rsp` | 0x802747B0 | 328 | matched |  |
| `revolution/bte/sdp_discovery` | `process_service_attr_rsp` | 0x802748F8 | 680 | matched |  |
| `revolution/bte/sdp_discovery` | `process_service_search_attr_rsp` | 0x80274BA0 | 724 | matched |  |
| `revolution/bte/sdp_discovery` | `save_attr_seq` | 0x80274E74 | 492 | matched |  |
| `revolution/bte/sdp_discovery` | `add_record` | 0x80275060 | 160 | matched |  |
| `revolution/bte/sdp_discovery` | `add_attr` | 0x80275100 | 1432 | matched |  |
| `revolution/bte/sdp_main` | `sdp_init` | 0x80275698 | 416 | matched |  |
| `revolution/bte/sdp_main` | `sdp_connect_ind` | 0x80275838 | 196 | matched |  |
| `revolution/bte/sdp_main` | `sdp_connect_cfm` | 0x802758FC | 400 | matched |  |
| `revolution/bte/sdp_main` | `sdp_config_ind` | 0x80275A8C | 316 | matched |  |
| `revolution/bte/sdp_main` | `sdp_config_cfm` | 0x80275BC8 | 380 | matched |  |
| `revolution/bte/sdp_main` | `sdp_disconnect_ind` | 0x80275D44 | 248 | matched |  |
| `revolution/bte/sdp_main` | `sdp_data_ind` | 0x80275E3C | 212 | matched |  |
| `revolution/bte/sdp_main` | `sdp_conn_originate` | 0x80275F10 | 276 | matched |  |
| `revolution/bte/sdp_main` | `sdp_disconnect` | 0x80276024 | 156 | matched |  |
| `revolution/bte/sdp_main` | `sdp_disconnect_cfm` | 0x802760C0 | 188 | matched |  |
| `revolution/bte/sdp_main` | `sdp_conn_timeout` | 0x8027617C | 136 | matched |  |
| `revolution/bte/sdp_server` | `sdp_server_handle_client_req` | 0x80276204 | 340 | matched |  |
| `revolution/bte/sdp_server` | `process_service_search` | 0x80276358 | 1040 | matched |  |
| `revolution/bte/sdp_server` | `process_service_attr_req` | 0x80276768 | 912 | matched |  |
| `revolution/bte/sdp_server` | `process_service_search_attr_req` | 0x80276AF8 | 1052 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_find_ccb_by_cid` | 0x80276F14 | 144 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_allocate_ccb` | 0x80276FA4 | 116 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_release_ccb` | 0x80277018 | 56 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_build_attrib_seq` | 0x80277050 | 464 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_build_attrib_entry` | 0x80277220 | 340 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_build_n_send_error` | 0x80277374 | 316 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_extract_uid_seq` | 0x802774B0 | 848 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_extract_attr_seq` | 0x80277800 | 592 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_get_len_from_type` | 0x80277A50 | 208 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_is_base_uuid` | 0x80277B20 | 224 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_compare_uuid_arrays` | 0x80277C00 | 628 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_compare_uuid_with_attr` | 0x80277E74 | 144 | matched |  |
| `revolution/bte/sdp_utils` | `sdpu_sort_attr_list` | 0x80277F04 | 80 | matched |  |
| `revolution/tpl/TPL` | `TPLBind` | 0x80277F60 | 280 | matched |  |
| `revolution/tpl/TPL` | `TPLGet` | 0x80278080 | 32 | matched |  |
| `revolution/vi/i2c` | `WaitMicroTime` | 0x8022A990 | 136 | matched |  |
| `revolution/vi/i2c` | `sendSlaveAddr` | 0x8022AA20 | 840 | matched |  |
| `revolution/vi/i2c` | `__VISendI2CData` | 0x8022AD70 | 1332 | matched |  |
| `revolution/vi/vi3in1` | `__VISetYUVSEL` | 0x8022B2B0 | 160 | matched |  |
| `revolution/vi/vi3in1` | `__VISetFilter4EURGB60` | 0x8022B350 | 64 | matched |  |
| `revolution/vi/vi3in1` | `__VISetCGMS` | 0x8022B390 | 88 | matched |  |
| `revolution/vi/vi3in1` | `__VISetWSS` | 0x8022B3F0 | 100 | matched |  |
| `revolution/vi/vi3in1` | `__VISetClosedCaption` | 0x8022B460 | 108 | matched |  |
| `revolution/vi/vi3in1` | `__VISetMacrovision` | 0x8022B4D0 | 3448 | matched |  |
| `revolution/vi/vi3in1` | `__VISetGammaImm` | 0x8022C250 | 352 | matched |  |
| `revolution/vi/vi3in1` | `__VISetGamma1_0` | 0x8022C3B0 | 16 | matched |  |
| `revolution/vi/vi3in1` | `__VISetGamma` | 0x8022C3C0 | 24 | matched |  |
| `revolution/vi/vi3in1` | `VISetGamma` | 0x8022C3E0 | 32 | matched |  |
| `revolution/vi/vi3in1` | `__VISetTrapFilter` | 0x8022C400 | 92 | matched |  |
| `revolution/vi/vi3in1` | `__VISetRGBOverDrive` | 0x8022C460 | 132 | matched |  |
| `revolution/vi/vi3in1` | `VISetRGBModeImm` | 0x8022C4F0 | 16 | matched |  |
| `revolution/vi/vi3in1` | `__VISetRGBModeImm` | 0x8022C500 | 72 | matched |  |
| `revolution/vi/vi3in1` | `__VISetRevolutionModeSimple` | 0x8022C550 | 1196 | matched |  |
| `revolution/kpad/KPAD` | `KPADSetPosParam` | 0x802780A0 | 28 | matched |  |
| `revolution/kpad/KPAD` | `reset_kpad` | 0x802780C0 | 464 | matched |  |
| `revolution/kpad/KPAD` | `fn_80278290` | 0x80278290 | 32 | matched | placeholder |
| `revolution/kpad/KPAD` | `calc_button_repeat` | 0x802782B0 | 416 | matched |  |
| `revolution/kpad/KPAD` | `read_kpad_button` | 0x80278450 | 160 | matched |  |
| `revolution/kpad/KPAD` | `calc_acc` | 0x802784F0 | 180 | matched |  |
| `revolution/kpad/KPAD` | `calc_acc_horizon` | 0x802785B0 | 404 | matched |  |
| `revolution/kpad/KPAD` | `calc_acc_vertical` | 0x80278750 | 292 | matched |  |
| `revolution/kpad/KPAD` | `read_kpad_acc` | 0x80278880 | 1120 | matched |  |
| `revolution/kpad/KPAD` | `select_2obj_first` | 0x80278CE0 | 488 | matched |  |
| `revolution/kpad/KPAD` | `select_2obj_continue` | 0x80278ED0 | 552 | matched |  |
| `revolution/kpad/KPAD` | `select_1obj_first` | 0x80279100 | 436 | matched |  |
| `revolution/kpad/KPAD` | `select_1obj_continue` | 0x802792C0 | 372 | matched |  |
| `revolution/kpad/KPAD` | `calc_dpd_variable` | 0x80279440 | 1484 | matched |  |
| `revolution/kpad/KPAD` | `read_kpad_dpd` | 0x80279A10 | 1148 | matched |  |
| `revolution/kpad/KPAD` | `clamp_stick_circle` | 0x80279E90 | 296 | matched |  |
| `revolution/kpad/KPAD` | `clamp_stick` | 0x80279FC0 | 548 | matched |  |
| `revolution/kpad/KPAD` | `read_kpad_stick` | 0x8027A1F0 | 2256 | matched |  |
| `revolution/kpad/KPAD` | `KPADRead` | 0x8027AAC0 | 12 | matched |  |
| `revolution/kpad/KPAD` | `KPADiRead` | 0x8027AAD0 | 1836 | matched |  |
| `revolution/kpad/KPAD` | `KPADInit` | 0x8027B200 | 12 | matched |  |
| `revolution/kpad/KPAD` | `KPADInitEx` | 0x8027B210 | 1092 | matched |  |
| `revolution/kpad/KPAD` | `KPADiConnectCallback` | 0x8027B660 | 356 | matched |  |
| `revolution/kpad/KPAD` | `KPADSetConnectCallback` | 0x8027B7D0 | 88 | matched |  |
| `revolution/kpad/KPAD` | `fn_8027B830` | 0x8027B830 | 24 | matched | placeholder |
| `revolution/kpad/KPAD` | `fn_8027B850` | 0x8027B850 | 24 | matched | placeholder |
| `revolution/kpad/KPAD` | `KPADiControlDpdCallback` | 0x8027B870 | 148 | matched |  |
| `revolution/kpad/KPAD` | `KPADiSamplingCallback` | 0x8027B910 | 1896 | matched |  |
| `revolution/kpad/KPAD` | `KPADSetPosPlayMode` | 0x8027C080 | 24 | matched |  |
| `revolution/usb/usb` | `USB_LOG` | 0x8027C0A0 | 168 | matched |  |
| `revolution/usb/usb` | `USB_ERR` | 0x8027C150 | 172 | matched |  |
| `revolution/usb/usb` | `IUSB_OpenLib` | 0x8027C200 | 248 | matched |  |
| `revolution/usb/usb` | `IUSB_CloseLib` | 0x8027C300 | 8 | matched |  |
| `revolution/usb/usb` | `_intrBlkCtrlIsoCb` | 0x8027C310 | 412 | matched |  |
| `revolution/usb/usb` | `IUSB_OpenDeviceIds` | 0x8027C4B0 | 324 | matched |  |
| `revolution/usb/usb` | `IUSB_CloseDeviceAsync` | 0x8027C600 | 292 | matched |  |
| `revolution/usb/usb` | `__LongBlkMsgInt` | 0x8027C730 | 852 | matched |  |
| `revolution/usb/usb` | `__IntrBlkMsgInt` | 0x8027CA90 | 856 | matched |  |
| `revolution/usb/usb` | `IUSB_ReadIntrMsgAsync` | 0x8027CDF0 | 116 | matched |  |
| `revolution/usb/usb` | `IUSB_ReadBlkMsgAsync` | 0x8027CE70 | 160 | matched |  |
| `revolution/usb/usb` | `IUSB_WriteBlkMsgAsync` | 0x8027CF10 | 160 | matched |  |
| `revolution/usb/usb` | `__CtrlMsgInt` | 0x8027CFB0 | 1444 | matched |  |
| `revolution/usb/usb` | `IUSB_WriteCtrlMsgAsync` | 0x8027D560 | 140 | matched |  |
| `revolution/wpad/WPAD` | `noAlloc` | 0x8027D5F0 | 8 | matched |  |
| `revolution/wpad/WPAD` | `noFree` | 0x8027D600 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsEnabledVSM` | 0x8027D610 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsEnabledTRN` | 0x8027D620 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsEnabledGTR` | 0x8027D630 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsEnabledDRM` | 0x8027D640 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WBCSetupCalibration` | 0x8027D650 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WBCGetCalibrationStatus` | 0x8027D660 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WBCGetBatteryLevel` | 0x8027D670 | 8 | matched |  |
| `revolution/wpad/WPAD` | `WBCReadDummy` | 0x8027D680 | 48 | matched |  |
| `revolution/wpad/WPAD` | `WBCSetZEROPointDummy` | 0x8027D6B0 | 48 | matched |  |
| `revolution/wpad/WPAD` | `WBCGetTGCWeightDummy` | 0x8027D6E0 | 48 | matched |  |
| `revolution/wpad/WPAD` | `OnShutdown` | 0x8027D710 | 456 | matched |  |
| `revolution/wpad/WPAD` | `__SendData` | 0x8027D8E0 | 468 | matched |  |
| `revolution/wpad/WPAD` | `WPADiRadioSensitivity` | 0x8027DAC0 | 272 | matched |  |
| `revolution/wpad/WPAD` | `IsControllerDataChanged` | 0x8027DBD0 | 2296 | matched |  |
| `revolution/wpad/WPAD` | `CheckButtonCombination` | 0x8027E4D0 | 188 | matched |  |
| `revolution/wpad/WPAD` | `WPADiCheckContInputs` | 0x8027E590 | 404 | matched |  |
| `revolution/wpad/WPAD` | `WPADiManageHandler` | 0x8027E730 | 2524 | matched |  |
| `revolution/wpad/WPAD` | `WPADiManageHandler0` | 0x8027F110 | 32 | matched |  |
| `revolution/wpad/WPAD` | `__ClearControlBlock` | 0x8027F130 | 628 | matched |  |
| `revolution/wpad/WPAD` | `WPADiInitSub` | 0x8027F3B0 | 604 | matched |  |
| `revolution/wpad/WPAD` | `WPADInit` | 0x8027F610 | 112 | matched |  |
| `revolution/wpad/WPAD` | `WPADStartFastSimpleSync` | 0x8027F680 | 4 | matched |  |
| `revolution/wpad/WPAD` | `WPADStopSimpleSync` | 0x8027F690 | 4 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetSimpleSyncCallback` | 0x8027F6A0 | 4 | matched |  |
| `revolution/wpad/WPAD` | `WPADRegisterAllocator` | 0x8027F6B0 | 4 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetStatus` | 0x8027F6C0 | 4 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetRadioSensitivity` | 0x8027F6D0 | 68 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetSensorBarPosition` | 0x8027F720 | 52 | matched |  |
| `revolution/wpad/WPAD` | `setupCallback` | 0x8027F760 | 204 | matched |  |
| `revolution/wpad/WPAD` | `abortConnCallback` | 0x8027F830 | 232 | matched |  |
| `revolution/wpad/WPAD` | `firmwareCheckCallback` | 0x8027F920 | 2292 | matched |  |
| `revolution/wpad/WPAD` | `WPADiRetrieveChannel` | 0x80280220 | 288 | matched |  |
| `revolution/wpad/WPAD` | `WPADiConnCallback` | 0x80280340 | 1204 | matched |  |
| `revolution/wpad/WPAD` | `WPADiRecvCallback` | 0x80280800 | 80 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetAccGravityUnit` | 0x80280850 | 212 | matched |  |
| `revolution/wpad/WPAD` | `DisconnectCallback` | 0x80280930 | 36 | matched |  |
| `revolution/wpad/WPAD` | `WPADDisconnect` | 0x80280960 | 192 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetAutoSleepTime` | 0x80280A20 | 52 | matched |  |
| `revolution/wpad/WPAD` | `fn_80280A60` | 0x80280A60 | 88 | matched | placeholder |
| `revolution/wpad/WPAD` | `WPADProbe` | 0x80280AC0 | 140 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetSamplingCallback` | 0x80280B50 | 136 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetConnectCallback` | 0x80280BE0 | 136 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetExtensionCallback` | 0x80280C70 | 92 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetDataFormat` | 0x80280CD0 | 68 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetDataFormat` | 0x80280D20 | 184 | matched |  |
| `revolution/wpad/WPAD` | `__infoCallback` | 0x80280DE0 | 84 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetInfoAsync` | 0x80280E40 | 544 | matched |  |
| `revolution/wpad/WPAD` | `WPADControlMotor` | 0x80281060 | 204 | matched |  |
| `revolution/wpad/WPAD` | `WPADEnableMotor` | 0x80281130 | 52 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsMotorEnabled` | 0x80281170 | 52 | matched |  |
| `revolution/wpad/WPAD` | `WPADControlLed` | 0x802811B0 | 488 | matched |  |
| `revolution/wpad/WPAD` | `WPADSaveConfig` | 0x802813A0 | 172 | matched |  |
| `revolution/wpad/WPAD` | `WPADRead` | 0x80281450 | 228 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetAutoSamplingBuf` | 0x80281540 | 428 | matched |  |
| `revolution/wpad/WPAD` | `WPADiExcludeButton` | 0x802816F0 | 276 | matched |  |
| `revolution/wpad/WPAD` | `WPADiCopyOut` | 0x80281810 | 320 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsSpeakerEnabled` | 0x80281950 | 68 | matched |  |
| `revolution/wpad/WPAD` | `WPADControlSpeaker` | 0x802819A0 | 5628 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetSpeakerVolume` | 0x80282FA0 | 52 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetSpeakerVolume` | 0x80282FE0 | 80 | matched |  |
| `revolution/wpad/WPAD` | `IsBusyStream` | 0x80283030 | 312 | matched |  |
| `revolution/wpad/WPAD` | `WPADCanSendStreamData` | 0x80283170 | 132 | matched |  |
| `revolution/wpad/WPAD` | `WPADSendStreamData` | 0x80283200 | 528 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetDpdSensitivity` | 0x80283410 | 52 | matched |  |
| `revolution/wpad/WPAD` | `WPADIsDpdEnabled` | 0x80283450 | 68 | matched |  |
| `revolution/wpad/WPAD` | `WPADGetDpdFormat` | 0x802834A0 | 88 | matched |  |
| `revolution/wpad/WPAD` | `__dpdCb` | 0x80283500 | 56 | matched |  |
| `revolution/wpad/WPAD` | `WPADControlDpd` | 0x80283540 | 4224 | matched |  |
| `revolution/wpad/WPAD` | `WPADControlBLC` | 0x802845C0 | 524 | matched |  |
| `revolution/wpad/WPAD` | `WPADiSendSetReportType` | 0x802847D0 | 616 | matched |  |
| `revolution/wpad/WPAD` | `WPADiSendWriteDataCmd` | 0x80284A40 | 436 | matched |  |
| `revolution/wpad/WPAD` | `WPADiSendWriteData` | 0x80284C00 | 440 | matched |  |
| `revolution/wpad/WPAD` | `WPADiSendReadData` | 0x80284DC0 | 428 | matched |  |
| `revolution/wpad/WPAD` | `WPADiClearQueue` | 0x80284F70 | 96 | matched |  |
| `revolution/wpad/WPAD` | `WPADSetCallbackByKPAD` | 0x80284FD0 | 8 | matched |  |
| `revolution/wpad/WPADHIDParser` | `abortInitExtension` | 0x80284FE0 | 348 | matched |  |
| `revolution/wpad/WPADHIDParser` | `getDevConfig` | 0x80285140 | 2792 | matched |  |
| `revolution/wpad/WPADHIDParser` | `getExtConfig` | 0x80285C30 | 1340 | matched |  |
| `revolution/wpad/WPADHIDParser` | `getExtType` | 0x80286170 | 1080 | matched |  |
| `revolution/wpad/WPADHIDParser` | `getGameInfo` | 0x802865B0 | 420 | matched |  |
| `revolution/wpad/WPADHIDParser` | `WPADiHIDParser` | 0x80286760 | 296 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_20_status_report` | 0x80286890 | 832 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_21_user_data` | 0x80286BD0 | 808 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_22_ack` | 0x80286F00 | 320 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__parse_dpd_data` | 0x80287040 | 1172 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__parse_dpdex_data` | 0x802874E0 | 632 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__parse_cl_data` | 0x80287760 | 1228 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__parse_vs_data` | 0x80287C30 | 444 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_30_data_type` | 0x80287DF0 | 92 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_31_data_type` | 0x80287E50 | 260 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_32_data_type` | 0x80287F60 | 792 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_33_data_type` | 0x80288280 | 332 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_34_data_type` | 0x802883D0 | 480 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_35_data_type` | 0x802885B0 | 928 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_36_data_type` | 0x80288950 | 268 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_37_data_type` | 0x80288A60 | 940 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_3d_data_type` | 0x80288E10 | 276 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_3e_data_type` | 0x80288F30 | 436 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_3f_data_type` | 0x802890F0 | 432 | matched |  |
| `revolution/wpad/WPADHIDParser` | `__a1_unused_report` | 0x802892A0 | 4 | matched |  |
| `revolution/wpad/WPADEncrypt` | `WPADiCreateKey` | 0x802892B0 | 1776 | matched |  |
| `revolution/wpad/WPADEncrypt` | `WPADiCreateKeyFor3rd` | 0x802899A0 | 1784 | matched |  |
| `revolution/wpad/WPADEncrypt` | `WPADiDecode` | 0x8028A0A0 | 720 | matched |  |
| `revolution/wpad/WPADMem` | `WPADiClearMemBlock` | 0x8028A370 | 44 | matched |  |
| `revolution/wpad/WPADMem` | `WPADWriteExtReg` | 0x8028A3A0 | 204 | matched |  |
| `revolution/wpad/wpad_debug_msg` | `DEBUGPrint` | 0x8028A470 | 80 | matched | unit name unconfirmed |
| `revolution/wud/WUD` | `WUDIsLinkedWBC` | 0x8028A4C0 | 8 | matched |  |
| `revolution/wud/WUD` | `App_MEMalloc` | 0x8028A4D0 | 80 | matched |  |
| `revolution/wud/WUD` | `App_MEMfree` | 0x8028A520 | 80 | matched |  |
| `revolution/wud/WUD` | `SyncFlushCallback` | 0x8028A570 | 120 | matched |  |
| `revolution/wud/WUD` | `WUDiIsSyncDisabled` | 0x8028A5F0 | 344 | matched |  |
| `revolution/wud/WUD` | `WUDiTryConnecting` | 0x8028A750 | 368 | matched |  |
| `revolution/wud/WUD` | `WUDiVirginStandardSync` | 0x8028A8C0 | 888 | matched |  |
| `revolution/wud/WUD` | `WUDiSaveDeviceToNand` | 0x8028AC40 | 308 | matched |  |
| `revolution/wud/WUD` | `OpenWiiFitCallback` | 0x8028AD80 | 124 | matched |  |
| `revolution/wud/WUD` | `SeekWiiFitCallback` | 0x8028AE00 | 132 | matched |  |
| `revolution/wud/WUD` | `UpdateWiiFitCallback` | 0x8028AE90 | 124 | matched |  |
| `revolution/wud/WUD` | `CloseWiiFitCallback` | 0x8028AF10 | 84 | matched |  |
| `revolution/wud/WUD` | `WUDiSyncDone` | 0x8028AF70 | 360 | matched |  |
| `revolution/wud/WUD` | `SyncHandler` | 0x8028B0E0 | 2788 | matched |  |
| `revolution/wud/WUD` | `SyncHandler0` | 0x8028BBD0 | 32 | matched |  |
| `revolution/wud/WUD` | `DeleteFlushCallback` | 0x8028BBF0 | 88 | matched |  |
| `revolution/wud/WUD` | `WUDiTerminateDevice` | 0x8028BC50 | 152 | matched |  |
| `revolution/wud/WUD` | `WUDiDeleteDevice` | 0x8028BCF0 | 516 | matched |  |
| `revolution/wud/WUD` | `DeleteAllHandler` | 0x8028BF00 | 848 | matched |  |
| `revolution/wud/WUD` | `DeleteAllHandler0` | 0x8028C250 | 32 | matched |  |
| `revolution/wud/WUD` | `WUDiClearUnregisteredDevice` | 0x8028C270 | 268 | matched |  |
| `revolution/wud/WUD` | `EnableStackHandler` | 0x8028C380 | 324 | matched |  |
| `revolution/wud/WUD` | `EnableStackHandler0` | 0x8028C4D0 | 32 | matched |  |
| `revolution/wud/WUD` | `InitFlushCallback` | 0x8028C4F0 | 76 | matched |  |
| `revolution/wud/WUD` | `WUDiGetRegisteredDevice` | 0x8028C540 | 1592 | matched |  |
| `revolution/wud/WUD` | `NandResultCallback` | 0x8028CB80 | 128 | matched |  |
| `revolution/wud/WUD` | `NandFlushCallback` | 0x8028CC00 | 24 | matched |  |
| `revolution/wud/WUD` | `WUDiGetWiiFitDevice` | 0x8028CC20 | 1328 | matched |  |
| `revolution/wud/WUD` | `InitHandler` | 0x8028D150 | 276 | matched |  |
| `revolution/wud/WUD` | `InitHandler0` | 0x8028D270 | 32 | matched |  |
| `revolution/wud/WUD` | `ShutFlushCallback` | 0x8028D290 | 76 | matched |  |
| `revolution/wud/WUD` | `ShutdownHandler` | 0x8028D2E0 | 180 | matched |  |
| `revolution/wud/WUD` | `ShutdownHandler0` | 0x8028D3A0 | 32 | matched |  |
| `revolution/wud/WUD` | `InitCore` | 0x8028D3C0 | 916 | matched |  |
| `revolution/wud/WUD` | `WUDInit` | 0x8028D760 | 284 | matched |  |
| `revolution/wud/WUD` | `WUDRegisterAllocator` | 0x8028D880 | 76 | matched |  |
| `revolution/wud/WUD` | `WUDShutdown` | 0x8028D8D0 | 544 | matched |  |
| `revolution/wud/WUD` | `WUDGetStatus` | 0x8028DAF0 | 60 | matched |  |
| `revolution/wud/WUD` | `WUDGetBufferStatus` | 0x8028DB30 | 72 | matched |  |
| `revolution/wud/WUD` | `WUDSetSniffMode` | 0x8028DB80 | 108 | matched |  |
| `revolution/wud/WUD` | `WUDSetSyncSimpleCallback` | 0x8028DBF0 | 92 | matched |  |
| `revolution/wud/WUD` | `StartSyncDevice` | 0x8028DC50 | 332 | matched |  |
| `revolution/wud/WUD` | `WUDStartFastSyncSimple` | 0x8028DDA0 | 20 | matched |  |
| `revolution/wud/WUD` | `WUDCancelSyncDevice` | 0x8028DDC0 | 144 | matched |  |
| `revolution/wud/WUD` | `WUDStopSyncSimple` | 0x8028DE50 | 136 | matched |  |
| `revolution/wud/WUD` | `WUDSetDisableChannel` | 0x8028DEE0 | 232 | matched |  |
| `revolution/wud/WUD` | `WUDSetHidRecvCallback` | 0x8028DFD0 | 92 | matched |  |
| `revolution/wud/WUD` | `WUDSetHidConnCallback` | 0x8028E030 | 92 | matched |  |
| `revolution/wud/WUD` | `WUDSetVisibility` | 0x8028E090 | 88 | matched |  |
| `revolution/wud/WUD` | `reset_again_cb` | 0x8028E0F0 | 48 | matched |  |
| `revolution/wud/WUD` | `install_patch_cb` | 0x8028E120 | 240 | matched |  |
| `revolution/wud/WUD` | `write_patch_cb` | 0x8028E210 | 596 | matched |  |
| `revolution/wud/WUD` | `remove_patch_cb` | 0x8028E470 | 436 | matched |  |
| `revolution/wud/WUD` | `SuperPeekPokeCallback` | 0x8028E630 | 92 | matched |  |
| `revolution/wud/WUD` | `WUDiAppendRuntimePatch` | 0x8028E690 | 252 | matched |  |
| `revolution/wud/WUD` | `WUDiInitSub` | 0x8028E790 | 368 | matched |  |
| `revolution/wud/WUD` | `WUDiRegisterDevice` | 0x8028E900 | 340 | matched |  |
| `revolution/wud/WUD` | `WUDiRemoveDevice` | 0x8028EA60 | 384 | matched |  |
| `revolution/wud/WUD` | `WUDiGetDevInfo` | 0x8028EBE0 | 216 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveTopSmpDevInfoPtr` | 0x8028ECC0 | 276 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveBottomSmpDevInfoPtr` | 0x8028EDE0 | 276 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveTopOfDisconnectedSmpDevice` | 0x8028EF00 | 360 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveTopStdDevInfoPtr` | 0x8028F070 | 276 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveBottomStdDevInfoPtr` | 0x8028F190 | 276 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveTopOfDisconnectedStdDevice` | 0x8028F2B0 | 400 | matched |  |
| `revolution/wud/WUD` | `WUDiMoveTopOfUnusedStdDevice` | 0x8028F440 | 408 | matched |  |
| `revolution/wud/WUD` | `WUDIsBusy` | 0x8028F5E0 | 116 | matched |  |
| `revolution/wud/WUD` | `CleanupCallback` | 0x8028F660 | 52 | matched |  |
| `revolution/wud/WUD` | `WUDSecurityCallback` | 0x8028F6A0 | 1364 | matched |  |
| `revolution/wud/WUD` | `WUDSearchCallback` | 0x8028FC00 | 520 | matched |  |
| `revolution/wud/WUD` | `WUDVendorSpecificCallback` | 0x8028FE10 | 680 | matched |  |
| `revolution/wud/WUD` | `WUDDeviceStatusCallback` | 0x802900C0 | 80 | matched |  |
| `revolution/wud/WUD` | `WUDStoredLinkKeyCallback` | 0x80290110 | 852 | matched |  |
| `revolution/wud/WUD` | `WUDPowerManagerCallback` | 0x80290470 | 300 | matched |  |
| `revolution/wud/WUD` | `_WUDGetDevAddr` | 0x802905A0 | 84 | matched |  |
| `revolution/wud/WUD` | `_WUDGetQueuedSize` | 0x80290600 | 88 | matched |  |
| `revolution/wud/WUD` | `_WUDGetNotAckedSize` | 0x80290660 | 88 | matched |  |
| `revolution/wud/WUD` | `_WUDGetLinkNumber` | 0x802906C0 | 60 | matched |  |
| `revolution/wud/WUD` | `WUDiGetDiscoverDevice` | 0x80290700 | 12 | matched |  |
| `revolution/wud/WUD` | `WUDSetDeviceHistory` | 0x80290710 | 116 | matched |  |
| `revolution/wud/WUD` | `WUDIsLatestDevice` | 0x80290790 | 84 | matched |  |
| `revolution/wud/WUD` | `WUDUpdateSCSetting` | 0x802907F0 | 76 | matched |  |
| `revolution/wud/WUD` | `WUDiSetDevAddrForHandle` | 0x80290840 | 20 | matched |  |
| `revolution/wud/WUD` | `WUDiGetDevAddrForHandle` | 0x80290860 | 20 | matched |  |
| `revolution/wud/WUD` | `WUDiSetQueueSizeForHandle` | 0x80290880 | 20 | matched |  |
| `revolution/wud/WUD` | `WUDiSetNotAckNumForHandle` | 0x802908A0 | 20 | matched |  |
| `revolution/wud/WUDHidHost` | `WUDHidHostCallback` | 0x802908C0 | 1408 | matched |  |
| `revolution/wud/WUDHidHost` | `bta_hh_co_data` | 0x80290E40 | 60 | matched |  |
| `revolution/wud/WUDHidHost` | `bta_hh_co_open` | 0x80290E80 | 16 | matched |  |
| `revolution/wud/WUDHidHost` | `bta_hh_co_close` | 0x80290E90 | 16 | matched |  |
| `revolution/wud/WUDHidHost` | `bta_dm_co_get_compress_memory` | 0x80290EA0 | 8 | matched |  |
| `revolution/wud/debug_msg` | `WUD_DEBUGPrint` | 0x80290EB0 | 80 | matched |  |
| `revolution/hbm/HBMFrameController` | `init__Q210homebutton15FrameControllerFifff` | 0x803B4C10 | 56 | matched |  |
| `revolution/hbm/HBMFrameController` | `initFrame__Q210homebutton15FrameControllerFv` | 0x803B4C50 | 32 | matched |  |
| `revolution/hbm/HBMFrameController` | `calc__Q210homebutton15FrameControllerFv` | 0x803B4C70 | 332 | matched |  |
| `revolution/hbm/HBMAnmController` | `__ct__Q210homebutton18GroupAnmControllerFv` | 0x803B4DC0 | 28 | matched |  |
| `revolution/hbm/HBMAnmController` | `__dt__Q210homebutton18GroupAnmControllerFv` | 0x803B4DE0 | 64 | matched |  |
| `revolution/hbm/HBMAnmController` | `do_calc__Q210homebutton18GroupAnmControllerFv` | 0x803B4E20 | 176 | matched |  |
| `revolution/hbm/HBMGUIManager` | `drawLine___Q210homebutton3guiFfffffUcR8_GXColor` | 0x803B4ED0 | 452 | matched |  |
| `revolution/hbm/HBMGUIManager` | `update__Q310homebutton3gui9ComponentFiffUlUlUlPv` | 0x803B50A0 | 520 | matched |  |
| `revolution/hbm/HBMGUIManager` | `isVisible__Q310homebutton3gui9ComponentFv` | 0x803B52B0 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `isPointed__Q310homebutton3gui9ComponentFi` | 0x803B52C0 | 12 | matched |  |
| `revolution/hbm/HBMGUIManager` | `onMove__Q310homebutton3gui9ComponentFff` | 0x803B52D0 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `onEvent__Q310homebutton3gui7ManagerFUlUlPv` | 0x803B52E0 | 32 | matched |  |
| `revolution/hbm/HBMGUIManager` | `onEvent__Q310homebutton3gui12EventHandlerFUlUlPv` | 0x803B5300 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `getID__Q310homebutton3gui9ComponentFv` | 0x803B5310 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setPointed__Q310homebutton3gui9ComponentFib` | 0x803B5320 | 12 | matched |  |
| `revolution/hbm/HBMGUIManager` | `onPoint__Q310homebutton3gui9ComponentFv` | 0x803B5330 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `offPoint__Q310homebutton3gui9ComponentFv` | 0x803B5340 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `__dt__Q310homebutton3gui7ManagerFv` | 0x803B5350 | 188 | matched |  |
| `revolution/hbm/HBMGUIManager` | `init__Q310homebutton3gui7ManagerFv` | 0x803B5410 | 104 | matched |  |
| `revolution/hbm/HBMGUIManager` | `init__Q310homebutton3gui9ComponentFv` | 0x803B5480 | 44 | matched |  |
| `revolution/hbm/HBMGUIManager` | `addComponent__Q310homebutton3gui7ManagerFPQ310homebutton3gui9Component` | 0x803B54B0 | 196 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setManager__Q310homebutton3gui9ComponentFPQ310homebutton3gui7Manager` | 0x803B5580 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `delComponent__Q310homebutton3gui7ManagerFPQ310homebutton3gui9Component` | 0x803B5590 | 160 | matched |  |
| `revolution/hbm/HBMGUIManager` | `getComponent__Q310homebutton3gui7ManagerFUl` | 0x803B5630 | 44 | matched |  |
| `revolution/hbm/HBMGUIManager` | `update__Q310homebutton3gui7ManagerFiffUlUlUlPv` | 0x803B5660 | 420 | matched |  |
| `revolution/hbm/HBMGUIManager` | `isTriggerTarger__Q310homebutton3gui9ComponentFv` | 0x803B5810 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `onTrig__Q310homebutton3gui9ComponentFUlR3Vec` | 0x803B5820 | 48 | matched |  |
| `revolution/hbm/HBMGUIManager` | `calc__Q310homebutton3gui7ManagerFv` | 0x803B5850 | 104 | matched |  |
| `revolution/hbm/HBMGUIManager` | `calc__Q310homebutton3gui9InterfaceFv` | 0x803B58C0 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `draw__Q310homebutton3gui7ManagerFv` | 0x803B58D0 | 104 | matched |  |
| `revolution/hbm/HBMGUIManager` | `draw__Q310homebutton3gui9InterfaceFv` | 0x803B5940 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setAllComponentTriggerTarget__Q310homebutton3gui7ManagerFb` | 0x803B5950 | 120 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setTriggerTarget__Q310homebutton3gui9ComponentFb` | 0x803B59D0 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `__dt__Q310homebutton3gui11PaneManagerFv` | 0x803B59E0 | 340 | matched |  |
| `revolution/hbm/HBMGUIManager` | `__dt__Q310homebutton3gui13PaneComponentFv` | 0x803B5B40 | 64 | matched |  |
| `revolution/hbm/HBMGUIManager` | `__dt__Q310homebutton3gui9ComponentFv` | 0x803B5B80 | 64 | matched |  |
| `revolution/hbm/HBMGUIManager` | `createLayoutScene__Q310homebutton3gui11PaneManagerFRCQ36nw4hbm3lyt6Layout` | 0x803B5BC0 | 36 | matched |  |
| `revolution/hbm/HBMGUIManager` | `addLayoutScene__Q310homebutton3gui11PaneManagerFRCQ36nw4hbm3lyt6Layout` | 0x803B5BF0 | 24 | matched |  |
| `revolution/hbm/HBMGUIManager` | `walkInChildren__Q310homebutton3gui11PaneManagerFRQ36nw4hbm2ut30LinkList<Q36nw4hbm3lyt4Pane,4>` | 0x803B5C10 | 664 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setPane__Q310homebutton3gui13PaneComponentFPQ36nw4hbm3lyt4Pane` | 0x803B5EB0 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `GetRuntimeTypeInfo__Q36nw4hbm3lyt4PaneCFv` | 0x803B5EC0 | 12 | matched |  |
| `revolution/hbm/HBMGUIManager` | `delLayoutScene__Q310homebutton3gui11PaneManagerFRCQ36nw4hbm3lyt6Layout` | 0x803B5ED0 | 24 | matched |  |
| `revolution/hbm/HBMGUIManager` | `walkInChildrenDel__Q310homebutton3gui11PaneManagerFRQ36nw4hbm2ut30LinkList<Q36nw4hbm3lyt4Pane,4>` | 0x803B5EF0 | 284 | matched |  |
| `revolution/hbm/HBMGUIManager` | `getPaneComponentByPane__Q310homebutton3gui11PaneManagerFPQ36nw4hbm3lyt4Pane` | 0x803B6010 | 120 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setAllBoundingBoxComponentTriggerTarget__Q310homebutton3gui11PaneManagerFb` | 0x803B6090 | 208 | matched |  |
| `revolution/hbm/HBMGUIManager` | `contain__Q310homebutton3gui13PaneComponentFff` | 0x803B6160 | 300 | matched |  |
| `revolution/hbm/HBMGUIManager` | `getDrawInfo__Q310homebutton3gui11PaneManagerFv` | 0x803B6290 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `draw__Q310homebutton3gui13PaneComponentFv` | 0x803B62A0 | 388 | matched |  |
| `revolution/hbm/HBMGUIManager` | `is_visible__Q210homebutton3guiFPQ36nw4hbm3lyt4Pane` | 0x803B6430 | 256 | matched |  |
| `revolution/hbm/HBMGUIManager` | `IsVisible__Q36nw4hbm3lyt4PaneCFv` | 0x803B6530 | 12 | matched |  |
| `revolution/hbm/HBMGUIManager` | `GetParent__Q36nw4hbm3lyt4PaneCFv` | 0x803B6540 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `isVisible__Q310homebutton3gui13PaneComponentFv` | 0x803B6550 | 212 | matched |  |
| `revolution/hbm/HBMGUIManager` | `draw__Q310homebutton3gui9InterfaceFRA3_A4_f` | 0x803B6630 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `create__Q310homebutton3gui9InterfaceFv` | 0x803B6640 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `update__Q310homebutton3gui9ComponentFiPC10KPADStatusffPv` | 0x803B6650 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setDraggingButton__Q310homebutton3gui9ComponentFUl` | 0x803B6660 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `onDrag__Q310homebutton3gui9ComponentFff` | 0x803B6670 | 4 | matched |  |
| `revolution/hbm/HBMGUIManager` | `getPane__Q310homebutton3gui13PaneComponentFv` | 0x803B6680 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setEventHandler__Q310homebutton3gui7ManagerFPQ310homebutton3gui12EventHandler` | 0x803B6690 | 44 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setManager__Q310homebutton3gui12EventHandlerFPQ310homebutton3gui7Manager` | 0x803B66C0 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `update__Q310homebutton3gui7ManagerFiPC10KPADStatusffPv` | 0x803B66D0 | 8 | matched |  |
| `revolution/hbm/HBMGUIManager` | `setDrawInfo__Q310homebutton3gui11PaneManagerFPCQ36nw4hbm3lyt8DrawInfo` | 0x803B66E0 | 8 | matched |  |
| `revolution/hbm/HBMController` | `wpadConnectCallback__Q210homebutton10ControllerFll` | 0x803B66F0 | 272 | matched |  |
| `revolution/hbm/HBMController` | `wpadExtensionCallback__Q210homebutton10ControllerFll` | 0x803B6800 | 268 | matched |  |
| `revolution/hbm/HBMController` | `soundOnCallback__Q210homebutton10ControllerFP7OSAlarmP9OSContext` | 0x803B6910 | 108 | matched |  |
| `revolution/hbm/HBMController` | `__ct__Q210homebutton10ControllerFiPQ210homebutton9RemoteSpk` | 0x803B6980 | 188 | matched |  |
| `revolution/hbm/HBMController` | `__dt__Q210homebutton10ControllerFv` | 0x803B6A40 | 128 | matched |  |
| `revolution/hbm/HBMController` | `initCallback__Q210homebutton10ControllerFv` | 0x803B6AC0 | 160 | matched |  |
| `revolution/hbm/HBMController` | `clearCallback__Q210homebutton10ControllerFv` | 0x803B6B60 | 116 | matched |  |
| `revolution/hbm/HBMController` | `setKpad__Q210homebutton10ControllerFPC11HBMKPadDatab` | 0x803B6BE0 | 388 | matched |  |
| `revolution/hbm/HBMController` | `clrKpadButton__Q210homebutton10ControllerFv` | 0x803B6D70 | 20 | matched |  |
| `revolution/hbm/HBMController` | `setInValidPos__Q210homebutton10ControllerFv` | 0x803B6D90 | 28 | matched |  |
| `revolution/hbm/HBMController` | `connect__Q210homebutton10ControllerFv` | 0x803B6DB0 | 16 | matched |  |
| `revolution/hbm/HBMController` | `disconnect__Q210homebutton10ControllerFv` | 0x803B6DC0 | 4 | matched |  |
| `revolution/hbm/HBMController` | `setSpeakerVol__Q210homebutton10ControllerFf` | 0x803B6DD0 | 8 | matched |  |
| `revolution/hbm/HBMController` | `playSound__Q210homebutton10ControllerFi` | 0x803B6DE0 | 156 | matched |  |
| `revolution/hbm/HBMController` | `isPlayingSoundId__Q210homebutton10ControllerCFi` | 0x803B6E80 | 96 | matched |  |
| `revolution/hbm/HBMController` | `initSound__Q210homebutton10ControllerFv` | 0x803B6EE0 | 16 | matched |  |
| `revolution/hbm/HBMController` | `updateSound__Q210homebutton10ControllerFv` | 0x803B6EF0 | 568 | matched |  |
| `revolution/hbm/HBMController` | `isPlayReady__Q210homebutton10ControllerCFv` | 0x803B7130 | 16 | matched |  |
| `revolution/hbm/HBMController` | `getController__Q210homebutton10ControllerFv` | 0x803B7140 | 4 | matched |  |
| `revolution/hbm/HBMController` | `startMotor__Q210homebutton10ControllerFv` | 0x803B7150 | 100 | matched |  |
| `revolution/hbm/HBMController` | `stopMotor__Q210homebutton10ControllerFv` | 0x803B71C0 | 48 | matched |  |
| `revolution/hbm/HBMController` | `getInfoAsync__Q210homebutton10ControllerFP8WPADInfo` | 0x803B71F0 | 132 | matched |  |
| `revolution/hbm/HBMController` | `ControllerCallback__Q210homebutton10ControllerFll` | 0x803B7280 | 36 | matched |  |
| `revolution/hbm/HBMController` | `getBatteryFlag__Q210homebutton10ControllerCFv` | 0x803B72B0 | 36 | matched |  |
| `revolution/hbm/HBMController` | `clrBatteryFlag__Q210homebutton10ControllerFv` | 0x803B72E0 | 32 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `UpdateSpeaker__Q210homebutton9RemoteSpkFP7OSAlarmP9OSContext` | 0x803B7300 | 620 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `ClearPcm__Q210homebutton9RemoteSpkFv` | 0x803B7570 | 44 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `__ct__Q210homebutton9RemoteSpkFPv` | 0x803B75A0 | 188 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `__dt__Q210homebutton9RemoteSpkFv` | 0x803B7660 | 164 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `Start__Q210homebutton9RemoteSpkFv` | 0x803B7710 | 212 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `Stop__Q210homebutton9RemoteSpkFv` | 0x803B77F0 | 84 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `DelaySpeakerOnCallback__Q210homebutton9RemoteSpkFP7OSAlarmP9OSContext` | 0x803B7850 | 48 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `SpeakerOnCallback__Q210homebutton9RemoteSpkFll` | 0x803B7880 | 216 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `DelaySpeakerPlayCallback__Q210homebutton9RemoteSpkFP7OSAlarmP9OSContext` | 0x803B7960 | 48 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `SpeakerPlayCallback__Q210homebutton9RemoteSpkFll` | 0x803B7990 | 228 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `Connect__Q210homebutton9RemoteSpkFl` | 0x803B7A80 | 140 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `Play__Q210homebutton9RemoteSpkFliSc` | 0x803B7B10 | 152 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `isPlaying__Q210homebutton9RemoteSpkCFl` | 0x803B7BB0 | 28 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `isPlayingId__Q210homebutton9RemoteSpkCFli` | 0x803B7BD0 | 48 | matched |  |
| `revolution/hbm/HBMRemoteSpk` | `isPlayReady__Q210homebutton9RemoteSpkCFl` | 0x803B7C00 | 28 | matched |  |
| `revolution/hbm/HBMAxSound` | `GetFreePlayer__24@unnamed@HBMAxSound_cpp@Fi` | 0x803B7C20 | 408 | matched |  |
| `revolution/hbm/HBMAxSound` | `AudioFrameCallback__24@unnamed@HBMAxSound_cpp@Fv` | 0x803B7DC0 | 100 | matched |  |
| `revolution/hbm/HBMAxSound` | `AudioSoundThreadProc__24@unnamed@HBMAxSound_cpp@FPv` | 0x803B7E30 | 576 | matched |  |
| `revolution/hbm/HBMAxSound` | `PlaySeq__10homebuttonFi` | 0x803B8070 | 316 | matched |  |
| `revolution/hbm/HBMAxSound` | `InitAxSound__10homebuttonFPCvPvUl` | 0x803B81B0 | 500 | matched |  |
| `revolution/hbm/HBMAxSound` | `ShutdownAxSound__10homebuttonFv` | 0x803B83B0 | 136 | matched |  |
| `revolution/hbm/HBMAxSound` | `AxSoundMain__10homebuttonFv` | 0x803B8440 | 4 | matched |  |
| `revolution/hbm/HBMAxSound` | `StopAllSeq__10homebuttonFv` | 0x803B8450 | 452 | matched |  |
| `revolution/hbm/HBMAxSound` | `SetVolumeAllSeq__10homebuttonFf` | 0x803B8620 | 288 | matched |  |
| `revolution/hbm/HBMAxSound` | `SetSoundMode__10homebuttonFUl` | 0x803B8740 | 4 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMAllocMem__FUl` | 0x803B8750 | 16 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMFreeMem__FPv` | 0x803B8760 | 16 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMCreate` | 0x803B8770 | 288 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMDelete` | 0x803B8890 | 112 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMInit` | 0x803B8900 | 48 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMCalc` | 0x803B8930 | 60 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMDraw` | 0x803B8970 | 36 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMGetSelectBtnNum` | 0x803B89A0 | 36 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMSetAdjustFlag` | 0x803B89D0 | 52 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMCreateSound` | 0x803B8A10 | 84 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMDeleteSound` | 0x803B8A70 | 4 | matched |  |
| `revolution/hbm/HBMCommon` | `HBMUpdateSound` | 0x803B8A80 | 36 | matched |  |
| `revolution/hbm/HBMBase` | `initgx__10homebuttonFv` | 0x803B8AB0 | 440 | matched |  |
| `revolution/hbm/HBMBase` | `__ct__Q210homebutton10HomeButtonFPC11HBMDataInfo` | 0x803B8C70 | 268 | matched |  |
| `revolution/hbm/HBMBase` | `__dt__Q210homebutton10HomeButtonFv` | 0x803B8D80 | 688 | matched |  |
| `revolution/hbm/HBMBase` | `__dt__Q36nw4hbm3lyt19ArcResourceAccessorFv` | 0x803B9030 | 108 | matched |  |
| `revolution/hbm/HBMBase` | `createInstance__Q210homebutton10HomeButtonFPC11HBMDataInfo` | 0x803B90A0 | 76 | matched |  |
| `revolution/hbm/HBMBase` | `deleteInstance__Q210homebutton10HomeButtonFv` | 0x803B90F0 | 68 | matched |  |
| `revolution/hbm/HBMBase` | `getInstance__Q210homebutton10HomeButtonFv` | 0x803B9140 | 12 | matched |  |
| `revolution/hbm/HBMBase` | `create__Q210homebutton10HomeButtonFv` | 0x803B9150 | 2144 | matched |  |
| `revolution/hbm/HBMBase` | `set_config__Q210homebutton10HomeButtonFv` | 0x803B99B0 | 368 | matched |  |
| `revolution/hbm/HBMBase` | `set_text__Q210homebutton10HomeButtonFv` | 0x803B9B20 | 132 | matched |  |
| `revolution/hbm/HBMBase` | `init__Q210homebutton10HomeButtonFv` | 0x803B9BB0 | 1184 | matched |  |
| `revolution/hbm/HBMBase` | `init_volume__Q210homebutton10HomeButtonFv` | 0x803BA050 | 512 | matched |  |
| `revolution/hbm/HBMBase` | `init_sound__Q210homebutton10HomeButtonFv` | 0x803BA250 | 304 | matched |  |
| `revolution/hbm/HBMBase` | `init_battery__Q210homebutton10HomeButtonFPC17HBMControllerData` | 0x803BA380 | 620 | matched |  |
| `revolution/hbm/HBMBase` | `calc__Q210homebutton10HomeButtonFPC17HBMControllerData` | 0x803BA5F0 | 7420 | matched |  |
| `revolution/hbm/HBMBase` | `calc_battery__Q210homebutton10HomeButtonFi` | 0x803BC2F0 | 516 | matched |  |
| `revolution/hbm/HBMBase` | `draw__Q210homebutton10HomeButtonFv` | 0x803BC500 | 344 | matched |  |
| `revolution/hbm/HBMBase` | `SpeakerCallback__10homebuttonFP7OSAlarmP9OSContext` | 0x803BC660 | 228 | matched |  |
| `revolution/hbm/HBMBase` | `MotorCallback__10homebuttonFP7OSAlarmP9OSContext` | 0x803BC750 | 36 | matched |  |
| `revolution/hbm/HBMBase` | `RetrySimpleSyncCallback__10homebuttonFP7OSAlarmP9OSContext` | 0x803BC780 | 204 | matched |  |
| `revolution/hbm/HBMBase` | `SimpleSyncCallback__10homebuttonFll` | 0x803BC850 | 56 | matched |  |
| `revolution/hbm/HBMBase` | `update__Q210homebutton10HomeButtonFPC17HBMControllerData` | 0x803BC890 | 2388 | matched |  |
| `revolution/hbm/HBMBase` | `update_controller__Q210homebutton10HomeButtonFi` | 0x803BD1F0 | 2212 | matched |  |
| `revolution/hbm/HBMBase` | `updateTrigPane__Q210homebutton10HomeButtonFv` | 0x803BDAA0 | 700 | matched |  |
| `revolution/hbm/HBMBase` | `startPointEvent__Q210homebutton10HomeButtonFPCQ36nw4hbm3lyt4PanePv` | 0x803BDD60 | 3660 | matched |  |
| `revolution/hbm/HBMBase` | `startLeftEvent__Q210homebutton10HomeButtonFPCQ36nw4hbm3lyt4Pane` | 0x803BEBB0 | 2764 | matched |  |
| `revolution/hbm/HBMBase` | `startTrigEvent__Q210homebutton10HomeButtonFPCQ36nw4hbm3lyt4Pane` | 0x803BF680 | 4696 | matched |  |
| `revolution/hbm/HBMBase` | `reset_btn__Q210homebutton10HomeButtonFv` | 0x803C08E0 | 620 | matched |  |
| `revolution/hbm/HBMBase` | `getPaneNo__Q210homebutton10HomeButtonFPCQ36nw4hbm3lyt4Pane` | 0x803C0B50 | 176 | matched |  |
| `revolution/hbm/HBMBase` | `getSelectBtnNum__Q210homebutton10HomeButtonFv` | 0x803C0C00 | 28 | matched |  |
| `revolution/hbm/HBMBase` | `setAdjustFlag__Q210homebutton10HomeButtonFi` | 0x803C0C20 | 292 | matched |  |
| `revolution/hbm/HBMBase` | `onEvent__Q210homebutton22HomeButtonEventHandlerFUlUlPv` | 0x803C0D50 | 184 | matched |  |
| `revolution/hbm/HBMBase` | `update_sound__Q210homebutton10HomeButtonFv` | 0x803C0E10 | 80 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `GetStepCurveValue__27@unnamed@lyt_animation_cpp@FfPCQ46nw4hbm3lyt3res7StepKeyUl` | 0x803C0E60 | 240 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `GetHermiteCurveValue__27@unnamed@lyt_animation_cpp@FfPCQ46nw4hbm3lyt3res10HermiteKeyUl` | 0x803C0F50 | 452 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `GetFrameSize__Q36nw4hbm3lyt13AnimTransformCFv` | 0x803C1120 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `__ct__Q36nw4hbm3lyt18AnimTransformBasicFv` | 0x803C1130 | 56 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `__dt__Q36nw4hbm3lyt18AnimTransformBasicFv` | 0x803C1170 | 140 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `SetResource__Q36nw4hbm3lyt18AnimTransformBasicFPCQ46nw4hbm3lyt3res14AnimationBlockPQ36nw4hbm3lyt16ResourceAccessor` | 0x803C1200 | 328 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `Bind__Q36nw4hbm3lyt18AnimTransformBasicFPQ36nw4hbm3lyt4Paneb` | 0x803C1350 | 256 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `Bind__Q36nw4hbm3lyt18AnimTransformBasicFPQ36nw4hbm3lyt8Material` | 0x803C1450 | 188 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `Animate__Q36nw4hbm3lyt18AnimTransformBasicFUlPQ36nw4hbm3lyt4Pane` | 0x803C1510 | 480 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `Animate__Q36nw4hbm3lyt18AnimTransformBasicFUlPQ36nw4hbm3lyt8Material` | 0x803C16F0 | 728 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_animation` | `FindAnimationLink__Q36nw4hbm3lyt6detailFPQ36nw4hbm2ut40LinkList<Q36nw4hbm3lyt13AnimationLink,0>PQ36nw4hbm3lyt13AnimTransform` | 0x803C19D0 | 56 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor` | `FindNameResource__37@unnamed@lyt_arcResourceAccessor_cpp@FP9ARCHandlePCc` | 0x803C1A10 | 220 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor` | `GetResourceSub__37@unnamed@lyt_arcResourceAccessor_cpp@FP9ARCHandlePCcUlPCcPUl` | 0x803C1AF0 | 328 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor` | `__ct__Q36nw4hbm3lyt19ArcResourceAccessorFv` | 0x803C1C40 | 84 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor` | `Attach__Q36nw4hbm3lyt19ArcResourceAccessorFPvPCc` | 0x803C1CA0 | 124 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor` | `GetResource__Q36nw4hbm3lyt19ArcResourceAccessorFUlPCcPUl` | 0x803C1D20 | 32 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor` | `GetFont__Q36nw4hbm3lyt19ArcResourceAccessorFPCc` | 0x803C1D40 | 112 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_bounding` | `__ct__Q36nw4hbm3lyt8BoundingFPCQ46nw4hbm3lyt3res8BoundingRCQ36nw4hbm3lyt11ResBlockSet` | 0x803C1DB0 | 60 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_bounding` | `__dt__Q36nw4hbm3lyt8BoundingFv` | 0x803C1DF0 | 88 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_bounding` | `DrawSelf__Q36nw4hbm3lyt8BoundingFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C1E50 | 112 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_bounding` | `__dt__Q36nw4hbm2ut5ColorFv` | 0x803C1EC0 | 64 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_bounding` | `GetRuntimeTypeInfo__Q36nw4hbm3lyt8BoundingCFv` | 0x803C1F00 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_bounding` | `__sinit_\lyt_bounding_cpp` | 0x803C1F10 | 20 | matched | placeholder (compiler-generated name) |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `EqualsPaneName__Q36nw4hbm3lyt6detailFPCcPCc` | 0x803C1F30 | 44 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `EqualsMaterialName__Q36nw4hbm3lyt6detailFPCcPCc` | 0x803C1F60 | 44 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `TestFileHeader__Q36nw4hbm3lyt6detailFRCQ46nw4hbm3lyt3res16BinaryFileHeader` | 0x803C1F90 | 40 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `TestFileHeader__Q36nw4hbm3lyt6detailFRCQ46nw4hbm3lyt3res16BinaryFileHeaderUl` | 0x803C1FC0 | 52 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `__ct__Q46nw4hbm3lyt6detail11TexCoordAryFv` | 0x803C2000 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `Free__Q46nw4hbm3lyt6detail11TexCoordAryFv` | 0x803C2020 | 80 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `Reserve__Q46nw4hbm3lyt6detail11TexCoordAryFUc` | 0x803C2070 | 136 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `SetSize__Q46nw4hbm3lyt6detail11TexCoordAryFUc` | 0x803C2100 | 248 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `Copy__Q46nw4hbm3lyt6detail11TexCoordAryFPCvUc` | 0x803C2200 | 148 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `IsModulateVertexColor__Q36nw4hbm3lyt6detailFPQ36nw4hbm2ut5ColorUc` | 0x803C22A0 | 104 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `MultipleAlpha__Q36nw4hbm3lyt6detailFQ36nw4hbm2ut5ColorUc` | 0x803C2310 | 60 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `SetVertexFormat__Q36nw4hbm3lyt6detailFbUc` | 0x803C2350 | 228 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `DrawQuad__Q36nw4hbm3lyt6detailFRCQ36nw4hbm4math4VEC2RCQ36nw4hbm3lyt4SizeUcPA4_CQ36nw4hbm4math4VEC2PCQ36nw4hbm2ut5Color` | 0x803C2440 | 1376 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `DrawQuad__Q36nw4hbm3lyt6detailFRCQ36nw4hbm4math4VEC2RCQ36nw4hbm3lyt4SizeUcPA4_CQ36nw4hbm4math4VEC2PCQ36nw4hbm2ut5ColorUc` | 0x803C29A0 | 308 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `DrawLine__Q36nw4hbm3lyt6detailFRCQ36nw4hbm4math4VEC2RCQ36nw4hbm3lyt4SizeQ36nw4hbm2ut5Color` | 0x803C2AE0 | 408 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_common` | `InitGXTexObjFromTPL__Q36nw4hbm3lyt6detailFP9_GXTexObjP10TPLPaletteUl` | 0x803C2C80 | 320 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_drawInfo` | `__ct__Q36nw4hbm3lyt8DrawInfoFv` | 0x803C2DC0 | 124 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_drawInfo` | `__dt__Q36nw4hbm3lyt8DrawInfoFv` | 0x803C2E40 | 64 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_group` | `__ct__Q36nw4hbm3lyt5GroupFPCQ46nw4hbm3lyt3res5GroupPQ36nw4hbm3lyt4Pane` | 0x803C2E80 | 260 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_group` | `__dt__Q36nw4hbm3lyt5GroupFv` | 0x803C2F90 | 168 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_group` | `__dt__Q36nw4hbm3lyt14GroupContainerFv` | 0x803C3040 | 200 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_group` | `AppendGroup__Q36nw4hbm3lyt14GroupContainerFPQ36nw4hbm3lyt5Group` | 0x803C3110 | 52 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_group` | `FindGroupByName__Q36nw4hbm3lyt14GroupContainerFPCc` | 0x803C3150 | 124 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `SetTagProcessorImpl__24@unnamed@lyt_layout_cpp@FPQ36nw4hbm3lyt4PanePQ36nw4hbm2ut19TagProcessorBase<w>` | 0x803C31D0 | 416 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `__ct__Q36nw4hbm3lyt6LayoutFv` | 0x803C3370 | 64 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `__dt__Q36nw4hbm3lyt6LayoutFv` | 0x803C33B0 | 296 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `Build__Q36nw4hbm3lyt6LayoutFPCvPQ36nw4hbm3lyt16ResourceAccessor` | 0x803C34E0 | 772 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `CreateAnimTransform__Q36nw4hbm3lyt6LayoutFPCvPQ36nw4hbm3lyt16ResourceAccessor` | 0x803C37F0 | 328 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `BindAnimation__Q36nw4hbm3lyt6LayoutFPQ36nw4hbm3lyt13AnimTransform` | 0x803C3940 | 36 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `UnbindAnimation__Q36nw4hbm3lyt6LayoutFPQ36nw4hbm3lyt13AnimTransform` | 0x803C3970 | 36 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `UnbindAllAnimation__Q36nw4hbm3lyt6LayoutFv` | 0x803C39A0 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `SetAnimationEnable__Q36nw4hbm3lyt6LayoutFPQ36nw4hbm3lyt13AnimTransformb` | 0x803C39C0 | 36 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `CalculateMtx__Q36nw4hbm3lyt6LayoutFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C39F0 | 32 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `Draw__Q36nw4hbm3lyt6LayoutFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C3A10 | 32 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `Animate__Q36nw4hbm3lyt6LayoutFUl` | 0x803C3A30 | 32 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `GetLayoutRect__Q36nw4hbm3lyt6LayoutCFv` | 0x803C3A50 | 108 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `SetTagProcessor__Q36nw4hbm3lyt6LayoutFPQ36nw4hbm2ut19TagProcessorBase<w>` | 0x803C3AC0 | 424 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_layout` | `BuildPaneObj__Q36nw4hbm3lyt6LayoutFlPCvRCQ36nw4hbm3lyt11ResBlockSet` | 0x803C3C70 | 508 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `CalcTextureMtx__26@unnamed@lyt_material_cpp@FPQ36nw4hbm4math5MTX34RCQ36nw4hbm3lyt6TexSRT` | 0x803C3E70 | 280 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetIndTexMtx__26@unnamed@lyt_material_cpp@F14_GXIndTexMtxIDPA3_Cf` | 0x803C3F90 | 520 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `__ct__Q36nw4hbm2ut5ColorFv` | 0x803C41A0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `__ct__Q36nw4hbm3lyt8MaterialFPCQ46nw4hbm3lyt3res8MaterialRCQ36nw4hbm3lyt11ResBlockSet` | 0x803C41B0 | 4484 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `__dt__Q36nw4hbm3lyt8MaterialFv` | 0x803C5340 | 176 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `ReserveGXMem__Q36nw4hbm3lyt8MaterialFUcUcUcUcbUcUcbbbb` | 0x803C53F0 | 1688 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `GetTexSRTAry__Q36nw4hbm3lyt8MaterialFv` | 0x803C5A90 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `GetTexCoordGenAry__Q36nw4hbm3lyt8MaterialFv` | 0x803C5AB0 | 32 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `GetIndTexSRTAry__Q36nw4hbm3lyt8MaterialFv` | 0x803C5AD0 | 88 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetTextureNum__Q36nw4hbm3lyt8MaterialFUc` | 0x803C5B30 | 144 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetTexCoordGenNum__Q36nw4hbm3lyt8MaterialFUc` | 0x803C5BC0 | 340 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetTextureNoWrap__Q36nw4hbm3lyt8MaterialFUcP10TPLPalette` | 0x803C5D20 | 132 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetTexture__Q36nw4hbm3lyt8MaterialFUcRC9_GXTexObj` | 0x803C5DB0 | 84 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetColorElement__Q36nw4hbm3lyt8MaterialFUls` | 0x803C5E10 | 436 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetupGX__Q36nw4hbm3lyt8MaterialFbUc` | 0x803C5FD0 | 3612 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `BindAnimation__Q36nw4hbm3lyt8MaterialFPQ36nw4hbm3lyt13AnimTransform` | 0x803C6DF0 | 28 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `UnbindAnimation__Q36nw4hbm3lyt8MaterialFPQ36nw4hbm3lyt13AnimTransform` | 0x803C6E10 | 128 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `UnbindAllAnimation__Q36nw4hbm3lyt8MaterialFv` | 0x803C6E90 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `Animate__Q36nw4hbm3lyt8MaterialFv` | 0x803C6EB0 | 120 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `AddAnimationLink__Q36nw4hbm3lyt8MaterialFPQ36nw4hbm3lyt13AnimationLink` | 0x803C6F30 | 52 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `FindAnimationLink__Q36nw4hbm3lyt8MaterialFPQ36nw4hbm3lyt13AnimTransform` | 0x803C6F70 | 8 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `SetAnimationEnable__Q36nw4hbm3lyt8MaterialFPQ36nw4hbm3lyt13AnimTransformb` | 0x803C6F80 | 76 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_material` | `GetTextureSize__Q36nw4hbm3lyt6detailFPQ36nw4hbm3lyt8MaterialUc` | 0x803C6FD0 | 272 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `__ct__Q36nw4hbm3lyt4PaneFPCQ46nw4hbm3lyt3res4Pane` | 0x803C70E0 | 280 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `__dt__Q36nw4hbm3lyt4PaneFv` | 0x803C7200 | 308 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `AppendChild__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt4Pane` | 0x803C7340 | 80 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `GetPaneRect__Q36nw4hbm3lyt4PaneCFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C7390 | 208 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `GetVtxColor__Q36nw4hbm3lyt4PaneCFUl` | 0x803C7460 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `SetVtxColor__Q36nw4hbm3lyt4PaneFUlQ36nw4hbm2ut5Color` | 0x803C7470 | 4 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `GetColorElement__Q36nw4hbm3lyt4PaneCFUl` | 0x803C7480 | 36 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `SetColorElement__Q36nw4hbm3lyt4PaneFUlUc` | 0x803C74B0 | 36 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `GetVtxColorElement__Q36nw4hbm3lyt4PaneCFUl` | 0x803C74E0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `SetVtxColorElement__Q36nw4hbm3lyt4PaneFUlUc` | 0x803C74F0 | 4 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `FindPaneByName__Q36nw4hbm3lyt4PaneFPCcb` | 0x803C7500 | 172 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `FindMaterialByName__Q36nw4hbm3lyt4PaneFPCcb` | 0x803C75B0 | 184 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `CalculateMtx__Q36nw4hbm3lyt4PaneFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C7670 | 768 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `Draw__Q36nw4hbm3lyt4PaneFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C7970 | 132 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `DrawSelf__Q36nw4hbm3lyt4PaneFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C7A00 | 124 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `Animate__Q36nw4hbm3lyt4PaneFUl` | 0x803C7A80 | 140 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `AnimateSelf__Q36nw4hbm3lyt4PaneFUl` | 0x803C7B10 | 180 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `BindAnimation__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt13AnimTransformb` | 0x803C7BD0 | 28 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `UnbindAnimation__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt13AnimTransformb` | 0x803C7BF0 | 144 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `UnbindAllAnimation__Q36nw4hbm3lyt4PaneFb` | 0x803C7C80 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `UnbindAnimationSelf__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt13AnimTransform` | 0x803C7CA0 | 160 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `AddAnimationLink__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt13AnimationLink` | 0x803C7D40 | 52 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `FindAnimationLink__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt13AnimTransform` | 0x803C7D80 | 120 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `SetAnimationEnable__Q36nw4hbm3lyt4PaneFPQ36nw4hbm3lyt13AnimTransformbb` | 0x803C7E00 | 184 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `LoadMtx__Q36nw4hbm3lyt4PaneFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C7EC0 | 264 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `GetVtxPos__Q36nw4hbm3lyt4PaneCFv` | 0x803C7FD0 | 224 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `GetMaterial__Q36nw4hbm3lyt4PaneCFv` | 0x803C80B0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_pane` | `__sinit_\lyt_pane_cpp` | 0x803C80C0 | 16 | matched | placeholder (compiler-generated name) |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `__ct__Q36nw4hbm3lyt7PictureFPCQ46nw4hbm3lyt3res7PictureRCQ36nw4hbm3lyt11ResBlockSet` | 0x803C80D0 | 296 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `__dt__Q36nw4hbm3lyt7PictureFv` | 0x803C8200 | 208 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `Append__Q36nw4hbm3lyt7PictureFP10TPLPalette` | 0x803C82D0 | 76 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `Append__Q36nw4hbm3lyt7PictureFRC9_GXTexObj` | 0x803C8320 | 368 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `GetVtxColor__Q36nw4hbm3lyt7PictureCFUl` | 0x803C8490 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `SetVtxColor__Q36nw4hbm3lyt7PictureFUlQ36nw4hbm2ut5Color` | 0x803C84B0 | 44 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `GetVtxColorElement__Q36nw4hbm3lyt7PictureCFUl` | 0x803C84E0 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `SetVtxColorElement__Q36nw4hbm3lyt7PictureFUlUc` | 0x803C8500 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `DrawSelf__Q36nw4hbm3lyt7PictureFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C8520 | 220 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `GetRuntimeTypeInfo__Q36nw4hbm3lyt7PictureCFv` | 0x803C8600 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_picture` | `__sinit_\lyt_picture_cpp` | 0x803C8610 | 20 | matched | placeholder (compiler-generated name) |
| `revolution/hbm/nw4hbm/lyt/lyt_resourceAccessor` | `__dt__Q36nw4hbm3lyt16ResourceAccessorFv` | 0x803C8630 | 64 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_resourceAccessor` | `__ct__Q36nw4hbm3lyt16ResourceAccessorFv` | 0x803C8670 | 16 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_resourceAccessor` | `GetFont__Q36nw4hbm3lyt16ResourceAccessorFPCc` | 0x803C8680 | 8 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `__ct__Q36nw4hbm3lyt7TextBoxFPCQ46nw4hbm3lyt3res7TextBoxRCQ36nw4hbm3lyt11ResBlockSet` | 0x803C8690 | 692 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `__dt__Q36nw4hbm3lyt7TextBoxFv` | 0x803C8950 | 400 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `GetVtxColor__Q36nw4hbm3lyt7TextBoxCFUl` | 0x803C8AE0 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `SetVtxColor__Q36nw4hbm3lyt7TextBoxFUlQ36nw4hbm2ut5Color` | 0x803C8B00 | 60 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `GetVtxColorElement__Q36nw4hbm3lyt7TextBoxCFUl` | 0x803C8B40 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `SetVtxColorElement__Q36nw4hbm3lyt7TextBoxFUlUc` | 0x803C8B60 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `DrawSelf__Q36nw4hbm3lyt7TextBoxFRCQ36nw4hbm3lyt8DrawInfo` | 0x803C8B80 | 1812 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `CalcLineRectImpl<w>__25@unnamed@lyt_textBox_cpp@FPQ36nw4hbm2ut4RectPQ36nw4hbm2ut17TextWriterBase<w>PCwifPb_i` | 0x803C92A0 | 1092 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `AllocStringBuffer__Q36nw4hbm3lyt7TextBoxFUs` | 0x803C96F0 | 124 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `FreeStringBuffer__Q36nw4hbm3lyt7TextBoxFv` | 0x803C9770 | 76 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `SetString__Q36nw4hbm3lyt7TextBoxFPCwUs` | 0x803C97C0 | 104 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `SetString__Q36nw4hbm3lyt7TextBoxFPCwUsUs` | 0x803C9830 | 200 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `__dt__Q36nw4hbm2ut4FontFv` | 0x803C9900 | 64 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `CalcStringRectImpl<w>__25@unnamed@lyt_textBox_cpp@FPQ36nw4hbm2ut4RectPQ36nw4hbm2ut17TextWriterBase<w>PCwif_v` | 0x803C9940 | 360 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `GetRuntimeTypeInfo__Q36nw4hbm3lyt7TextBoxCFv` | 0x803C9AB0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_textBox` | `__sinit_\lyt_textBox_cpp` | 0x803C9AC0 | 20 | matched | placeholder (compiler-generated name) |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `__ct__Q36nw4hbm3lyt6WindowFPCQ46nw4hbm3lyt3res6WindowRCQ36nw4hbm3lyt11ResBlockSet` | 0x803C9AE0 | 528 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `__dt__Q36nw4hbm3lyt6WindowFv` | 0x803C9CF0 | 324 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `FindMaterialByName__Q36nw4hbm3lyt6WindowFPCcb` | 0x803C9E40 | 252 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `FindAnimationLink__Q36nw4hbm3lyt6WindowFPQ36nw4hbm3lyt13AnimTransform` | 0x803C9F40 | 164 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `SetAnimationEnable__Q36nw4hbm3lyt6WindowFPQ36nw4hbm3lyt13AnimTransformbb` | 0x803C9FF0 | 148 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `GetVtxColor__Q36nw4hbm3lyt6WindowCFUl` | 0x803CA090 | 20 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `SetVtxColor__Q36nw4hbm3lyt6WindowFUlQ36nw4hbm2ut5Color` | 0x803CA0B0 | 44 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `GetVtxColorElement__Q36nw4hbm3lyt6WindowCFUl` | 0x803CA0E0 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `SetVtxColorElement__Q36nw4hbm3lyt6WindowFUlUc` | 0x803CA100 | 24 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `DrawSelf__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm3lyt8DrawInfo` | 0x803CA120 | 312 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `AnimateSelf__Q36nw4hbm3lyt6WindowFUl` | 0x803CA260 | 144 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `UnbindAnimationSelf__Q36nw4hbm3lyt6WindowFPQ36nw4hbm3lyt13AnimTransform` | 0x803CA2F0 | 144 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `DrawContent__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm4math4VEC2RCQ36nw4hbm3lyt15WindowFrameSizeUc` | 0x803CA380 | 268 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `DrawFrame__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm4math4VEC2RCQ46nw4hbm3lyt6Window5FrameRCQ36nw4hbm3lyt15WindowFrameSizeUc` | 0x803CA490 | 1500 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `DrawFrame4__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm4math4VEC2PCQ46nw4hbm3lyt6Window5FrameRCQ36nw4hbm3lyt15WindowFrameSizeUc` | 0x803CAA70 | 1736 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `DrawFrame8__Q36nw4hbm3lyt6WindowFRCQ36nw4hbm4math4VEC2PCQ46nw4hbm3lyt6Window5FrameRCQ36nw4hbm3lyt15WindowFrameSizeUc` | 0x803CB140 | 3376 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `GetFrameSize__Q36nw4hbm3lyt6WindowFUcPCQ46nw4hbm3lyt6Window5Frame` | 0x803CBE70 | 248 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `GetFrameMaterial__Q36nw4hbm3lyt6WindowCFUl` | 0x803CBF70 | 40 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `GetContentMaterial__Q36nw4hbm3lyt6WindowCFv` | 0x803CBFA0 | 16 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `GetRuntimeTypeInfo__Q36nw4hbm3lyt6WindowCFv` | 0x803CBFB0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/lyt/lyt_window` | `__sinit_\lyt_window_cpp` | 0x803CBFC0 | 20 | matched | placeholder (compiler-generated name) |
| `revolution/hbm/nw4hbm/math/math_triangular` | `SinFIdx__Q26nw4hbm4mathFf` | 0x803CBFE0 | 128 | matched |  |
| `revolution/hbm/nw4hbm/math/math_triangular` | `CosFIdx__Q26nw4hbm4mathFf` | 0x803CC060 | 108 | matched |  |
| `revolution/hbm/nw4hbm/math/math_triangular` | `Atan2FIdx__Q26nw4hbm4mathFff` | 0x803CC0D0 | 448 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_binaryFileFormat` | `IsValidBinaryFile__Q26nw4hbm2utFPCQ36nw4hbm2ut16BinaryFileHeaderUlUsUs` | 0x803CC290 | 116 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharStrmReader` | `ReadNextCharUTF8__Q36nw4hbm2ut14CharStrmReaderFv` | 0x803CC310 | 120 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharStrmReader` | `ReadNextCharUTF16__Q36nw4hbm2ut14CharStrmReaderFv` | 0x803CC390 | 28 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharStrmReader` | `ReadNextCharCP1252__Q36nw4hbm2ut14CharStrmReaderFv` | 0x803CC3B0 | 28 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharStrmReader` | `ReadNextCharSJIS__Q36nw4hbm2ut14CharStrmReaderFv` | 0x803CC3D0 | 96 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `__ct__Q36nw4hbm2ut10CharWriterFv` | 0x803CC430 | 908 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `__dt__Q36nw4hbm2ut10CharWriterFv` | 0x803CC7C0 | 64 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetFont__Q36nw4hbm2ut10CharWriterFRCQ36nw4hbm2ut4Font` | 0x803CC800 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetFont__Q36nw4hbm2ut10CharWriterCFv` | 0x803CC810 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetupGX__Q36nw4hbm2ut10CharWriterFv` | 0x803CC820 | 2384 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetColorMapping__Q36nw4hbm2ut10CharWriterFQ36nw4hbm2ut5ColorQ36nw4hbm2ut5Color` | 0x803CD170 | 68 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetGradationMode__Q36nw4hbm2ut10CharWriterFQ46nw4hbm2ut10CharWriter13GradationMode` | 0x803CD1C0 | 336 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetTextColor__Q36nw4hbm2ut10CharWriterFQ36nw4hbm2ut5ColorQ36nw4hbm2ut5Color` | 0x803CD310 | 384 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetScaleH__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD490 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetScaleV__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD4A0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetFontSize__Q36nw4hbm2ut10CharWriterFff` | 0x803CD4B0 | 208 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetFontWidth__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD580 | 100 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetFontHeight__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD5F0 | 100 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetFontAscent__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD660 | 100 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `IsWidthFixed__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD6D0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetFixedWidth__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD6E0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `Print__Q36nw4hbm2ut10CharWriterFUs` | 0x803CD6F0 | 316 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetCursor__Q36nw4hbm2ut10CharWriterFff` | 0x803CD830 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetCursorX__Q36nw4hbm2ut10CharWriterFf` | 0x803CD840 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetCursorY__Q36nw4hbm2ut10CharWriterFf` | 0x803CD850 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `MoveCursorX__Q36nw4hbm2ut10CharWriterFf` | 0x803CD860 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `MoveCursorY__Q36nw4hbm2ut10CharWriterFf` | 0x803CD870 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetCursorX__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD880 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `GetCursorY__Q36nw4hbm2ut10CharWriterCFv` | 0x803CD890 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `PrintGlyph__Q36nw4hbm2ut10CharWriterFfffRCQ36nw4hbm2ut5Glyph` | 0x803CD8A0 | 644 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_CharWriter` | `SetupGXWithColorMapping__Q36nw4hbm2ut10CharWriterFQ36nw4hbm2ut5ColorQ36nw4hbm2ut5Color` | 0x803CDB30 | 764 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_Font` | `InitReaderFunc__Q36nw4hbm2ut4FontFQ36nw4hbm2ut12FontEncoding` | 0x803CDE30 | 164 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_LinkList` | `__dt__Q46nw4hbm2ut6detail12LinkListImplFv` | 0x803CDEE0 | 132 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_LinkList` | `Erase__Q46nw4hbm2ut6detail12LinkListImplFQ56nw4hbm2ut6detail12LinkListImpl8Iterator` | 0x803CDF70 | 72 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_LinkList` | `Insert__Q46nw4hbm2ut6detail12LinkListImplFQ56nw4hbm2ut6detail12LinkListImpl8IteratorPQ36nw4hbm2ut12LinkListNode` | 0x803CDFC0 | 44 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_list` | `List_Init__Q26nw4hbm2utFPQ36nw4hbm2ut4ListUs` | 0x803CDFF0 | 24 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_list` | `List_Append__Q26nw4hbm2utFPQ36nw4hbm2ut4ListPv` | 0x803CE010 | 112 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_list` | `List_Remove__Q26nw4hbm2utFPQ36nw4hbm2ut4ListPv` | 0x803CE080 | 108 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_list` | `List_GetNext__Q26nw4hbm2utFPCQ36nw4hbm2ut4ListPCv` | 0x803CE0F0 | 32 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_list` | `List_GetNth__Q26nw4hbm2utFPCQ36nw4hbm2ut4ListUs` | 0x803CE110 | 80 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFont` | `__ct__Q36nw4hbm2ut7ResFontFv` | 0x803CE160 | 60 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFont` | `__dt__Q36nw4hbm2ut7ResFontFv` | 0x803CE1A0 | 88 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFont` | `SetResource__Q36nw4hbm2ut7ResFontFPv` | 0x803CE200 | 328 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFont` | `Rebuild__Q36nw4hbm2ut7ResFontFPQ36nw4hbm2ut16BinaryFileHeader` | 0x803CE350 | 268 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `__ct__Q46nw4hbm2ut6detail11ResFontBaseFv` | 0x803CE460 | 68 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `__dt__Q46nw4hbm2ut6detail11ResFontBaseFv` | 0x803CE4B0 | 64 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `SetResourceBuffer__Q46nw4hbm2ut6detail11ResFontBaseFPvPQ36nw4hbm2ut15FontInformation` | 0x803CE4F0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetWidth__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE500 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetHeight__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE510 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetAscent__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE520 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetDescent__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE530 | 20 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetBaselinePos__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE550 | 20 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetCellHeight__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE570 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetCellWidth__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE580 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetMaxCharWidth__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE590 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetType__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE5A0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetTextureFormat__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE5B0 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetLineFeed__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE5C0 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetDefaultCharWidths__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE5D0 | 24 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `SetDefaultCharWidths__Q46nw4hbm2ut6detail11ResFontBaseFRCQ36nw4hbm2ut10CharWidths` | 0x803CE5F0 | 32 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `SetAlternateChar__Q46nw4hbm2ut6detail11ResFontBaseFUs` | 0x803CE610 | 156 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `SetLineFeed__Q46nw4hbm2ut6detail11ResFontBaseFi` | 0x803CE6B0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetCharWidth__Q46nw4hbm2ut6detail11ResFontBaseCFUs` | 0x803CE6C0 | 68 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetCharWidths__Q46nw4hbm2ut6detail11ResFontBaseCFUs` | 0x803CE710 | 244 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetGlyph__Q46nw4hbm2ut6detail11ResFontBaseCFPQ36nw4hbm2ut5GlyphUs` | 0x803CE810 | 164 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetEncoding__Q46nw4hbm2ut6detail11ResFontBaseCFv` | 0x803CE8C0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `FindGlyphIndex__Q46nw4hbm2ut6detail11ResFontBaseCFPCQ36nw4hbm2ut11FontCodeMapUs` | 0x803CE8D0 | 204 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_ResFontBase` | `GetGlyphFromIndex__Q46nw4hbm2ut6detail11ResFontBaseCFPQ36nw4hbm2ut5GlyphUs` | 0x803CE9A0 | 264 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `__ct__Q36nw4hbm2ut19TagProcessorBase<c>Fv` | 0x803CEAB0 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `__dt__Q36nw4hbm2ut19TagProcessorBase<c>Fv` | 0x803CEAC0 | 64 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `Process__Q36nw4hbm2ut19TagProcessorBase<c>FUsPQ36nw4hbm2ut15PrintContext<c>` | 0x803CEB00 | 348 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `CalcRect__Q36nw4hbm2ut19TagProcessorBase<c>FPQ36nw4hbm2ut4RectUsPQ36nw4hbm2ut15PrintContext<c>` | 0x803CEC60 | 564 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `ProcessLinefeed__Q36nw4hbm2ut19TagProcessorBase<c>FPQ36nw4hbm2ut15PrintContext<c>` | 0x803CEEA0 | 112 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `ProcessTab__Q36nw4hbm2ut19TagProcessorBase<c>FPQ36nw4hbm2ut15PrintContext<c>` | 0x803CEF10 | 248 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `__ct__Q36nw4hbm2ut19TagProcessorBase<w>Fv` | 0x803CF010 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `__dt__Q36nw4hbm2ut19TagProcessorBase<w>Fv` | 0x803CF020 | 64 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `Process__Q36nw4hbm2ut19TagProcessorBase<w>FUsPQ36nw4hbm2ut15PrintContext<w>` | 0x803CF060 | 348 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `CalcRect__Q36nw4hbm2ut19TagProcessorBase<w>FPQ36nw4hbm2ut4RectUsPQ36nw4hbm2ut15PrintContext<w>` | 0x803CF1C0 | 564 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `ProcessLinefeed__Q36nw4hbm2ut19TagProcessorBase<w>FPQ36nw4hbm2ut15PrintContext<w>` | 0x803CF400 | 112 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TagProcessorBase` | `ProcessTab__Q36nw4hbm2ut19TagProcessorBase<w>FPQ36nw4hbm2ut15PrintContext<w>` | 0x803CF470 | 248 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `__ct__Q36nw4hbm2ut17TextWriterBase<c>Fv` | 0x803CF570 | 92 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `__dt__Q36nw4hbm2ut17TextWriterBase<c>Fv` | 0x803CF5D0 | 88 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetLineHeight__Q36nw4hbm2ut17TextWriterBase<c>Ff` | 0x803CF630 | 160 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetLineHeight__Q36nw4hbm2ut17TextWriterBase<c>CFv` | 0x803CF6D0 | 140 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetLineSpace__Q36nw4hbm2ut17TextWriterBase<c>Ff` | 0x803CF760 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetCharSpace__Q36nw4hbm2ut17TextWriterBase<c>Ff` | 0x803CF770 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetLineSpace__Q36nw4hbm2ut17TextWriterBase<c>CFv` | 0x803CF780 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetCharSpace__Q36nw4hbm2ut17TextWriterBase<c>CFv` | 0x803CF790 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetTabWidth__Q36nw4hbm2ut17TextWriterBase<c>Fi` | 0x803CF7A0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetTabWidth__Q36nw4hbm2ut17TextWriterBase<c>CFv` | 0x803CF7B0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetDrawFlag__Q36nw4hbm2ut17TextWriterBase<c>FUl` | 0x803CF7C0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetDrawFlag__Q36nw4hbm2ut17TextWriterBase<c>CFv` | 0x803CF7D0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetTagProcessor__Q36nw4hbm2ut17TextWriterBase<c>FPQ36nw4hbm2ut19TagProcessorBase<c>` | 0x803CF7E0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `ResetTagProcessor__Q36nw4hbm2ut17TextWriterBase<c>Fv` | 0x803CF7F0 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetTagProcessor__Q36nw4hbm2ut17TextWriterBase<c>CFv` | 0x803CF800 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcFormatStringWidth__Q36nw4hbm2ut17TextWriterBase<c>CFPCce` | 0x803CF810 | 504 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcFormatStringHeight__Q36nw4hbm2ut17TextWriterBase<c>CFPCce` | 0x803CFA10 | 504 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcFormatStringRect__Q36nw4hbm2ut17TextWriterBase<c>CFPQ36nw4hbm2ut4RectPCce` | 0x803CFC10 | 464 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcVStringRect__Q36nw4hbm2ut17TextWriterBase<c>CFPQ36nw4hbm2ut4RectPCcP16__va_list_struct` | 0x803CFDE0 | 368 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringWidth__Q36nw4hbm2ut17TextWriterBase<c>CFPCci` | 0x803CFF50 | 320 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringWidth__Q36nw4hbm2ut17TextWriterBase<c>CFPCc` | 0x803D0090 | 336 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringHeight__Q36nw4hbm2ut17TextWriterBase<c>CFPCci` | 0x803D01E0 | 320 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringHeight__Q36nw4hbm2ut17TextWriterBase<c>CFPCc` | 0x803D0320 | 336 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringRect__Q36nw4hbm2ut17TextWriterBase<c>CFPQ36nw4hbm2ut4RectPCci` | 0x803D0470 | 272 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringRect__Q36nw4hbm2ut17TextWriterBase<c>CFPQ36nw4hbm2ut4RectPCc` | 0x803D0580 | 304 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `Printf__Q36nw4hbm2ut17TextWriterBase<c>FPCce` | 0x803D06B0 | 552 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `VPrintf__Q36nw4hbm2ut17TextWriterBase<c>FPCcP16__va_list_struct` | 0x803D08E0 | 452 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `Print__Q36nw4hbm2ut17TextWriterBase<c>FPCci` | 0x803D0AB0 | 340 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `Print__Q36nw4hbm2ut17TextWriterBase<c>FPCc` | 0x803D0C10 | 368 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetBuffer__Q36nw4hbm2ut17TextWriterBase<c>FPcUl` | 0x803D0D80 | 28 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetBuffer__Q36nw4hbm2ut17TextWriterBase<c>FUl` | 0x803D0DA0 | 32 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetBufferSize__Q36nw4hbm2ut17TextWriterBase<c>Fv` | 0x803D0DC0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetBuffer__Q36nw4hbm2ut17TextWriterBase<c>Fv` | 0x803D0DD0 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `VSNPrintf__Q36nw4hbm2ut17TextWriterBase<c>FPcUlPCcP16__va_list_struct` | 0x803D0DE0 | 4 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `StrLen__Q36nw4hbm2ut17TextWriterBase<c>FPCc` | 0x803D0DF0 | 4 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcLineWidth__Q36nw4hbm2ut17TextWriterBase<c>FPCci` | 0x803D0E00 | 360 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcLineRectImpl__Q36nw4hbm2ut17TextWriterBase<c>FPQ36nw4hbm2ut4RectPCci` | 0x803D0F70 | 988 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringRectImpl__Q36nw4hbm2ut17TextWriterBase<c>FPQ36nw4hbm2ut4RectPCci` | 0x803D1350 | 276 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `PrintImpl__Q36nw4hbm2ut17TextWriterBase<c>FPCci` | 0x803D1470 | 1452 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `AdjustCursor__Q36nw4hbm2ut17TextWriterBase<c>FPfPfPCci` | 0x803D1A20 | 1260 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `IsDrawFlagSet__Q36nw4hbm2ut17TextWriterBase<c>CFUlUl` | 0x803D1F10 | 24 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `__ct__Q36nw4hbm2ut17TextWriterBase<w>Fv` | 0x803D1F30 | 92 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `__dt__Q36nw4hbm2ut17TextWriterBase<w>Fv` | 0x803D1F90 | 88 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetLineHeight__Q36nw4hbm2ut17TextWriterBase<w>Ff` | 0x803D1FF0 | 160 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetLineHeight__Q36nw4hbm2ut17TextWriterBase<w>CFv` | 0x803D2090 | 140 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetLineSpace__Q36nw4hbm2ut17TextWriterBase<w>Ff` | 0x803D2120 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetCharSpace__Q36nw4hbm2ut17TextWriterBase<w>Ff` | 0x803D2130 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetLineSpace__Q36nw4hbm2ut17TextWriterBase<w>CFv` | 0x803D2140 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetCharSpace__Q36nw4hbm2ut17TextWriterBase<w>CFv` | 0x803D2150 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetTabWidth__Q36nw4hbm2ut17TextWriterBase<w>Fi` | 0x803D2160 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetTabWidth__Q36nw4hbm2ut17TextWriterBase<w>CFv` | 0x803D2170 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetDrawFlag__Q36nw4hbm2ut17TextWriterBase<w>FUl` | 0x803D2180 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetDrawFlag__Q36nw4hbm2ut17TextWriterBase<w>CFv` | 0x803D2190 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetTagProcessor__Q36nw4hbm2ut17TextWriterBase<w>FPQ36nw4hbm2ut19TagProcessorBase<w>` | 0x803D21A0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `ResetTagProcessor__Q36nw4hbm2ut17TextWriterBase<w>Fv` | 0x803D21B0 | 16 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetTagProcessor__Q36nw4hbm2ut17TextWriterBase<w>CFv` | 0x803D21C0 | 8 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcFormatStringWidth__Q36nw4hbm2ut17TextWriterBase<w>CFPCwe` | 0x803D21D0 | 504 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcFormatStringHeight__Q36nw4hbm2ut17TextWriterBase<w>CFPCwe` | 0x803D23D0 | 504 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcFormatStringRect__Q36nw4hbm2ut17TextWriterBase<w>CFPQ36nw4hbm2ut4RectPCwe` | 0x803D25D0 | 464 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcVStringRect__Q36nw4hbm2ut17TextWriterBase<w>CFPQ36nw4hbm2ut4RectPCwP16__va_list_struct` | 0x803D27A0 | 368 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringWidth__Q36nw4hbm2ut17TextWriterBase<w>CFPCwi` | 0x803D2910 | 320 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringWidth__Q36nw4hbm2ut17TextWriterBase<w>CFPCw` | 0x803D2A50 | 336 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringHeight__Q36nw4hbm2ut17TextWriterBase<w>CFPCwi` | 0x803D2BA0 | 320 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringHeight__Q36nw4hbm2ut17TextWriterBase<w>CFPCw` | 0x803D2CE0 | 336 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringRect__Q36nw4hbm2ut17TextWriterBase<w>CFPQ36nw4hbm2ut4RectPCwi` | 0x803D2E30 | 272 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringRect__Q36nw4hbm2ut17TextWriterBase<w>CFPQ36nw4hbm2ut4RectPCw` | 0x803D2F40 | 304 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `Printf__Q36nw4hbm2ut17TextWriterBase<w>FPCwe` | 0x803D3070 | 552 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `VPrintf__Q36nw4hbm2ut17TextWriterBase<w>FPCwP16__va_list_struct` | 0x803D32A0 | 452 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `Print__Q36nw4hbm2ut17TextWriterBase<w>FPCwi` | 0x803D3470 | 340 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `Print__Q36nw4hbm2ut17TextWriterBase<w>FPCw` | 0x803D35D0 | 368 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetBuffer__Q36nw4hbm2ut17TextWriterBase<w>FPwUl` | 0x803D3740 | 28 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `SetBuffer__Q36nw4hbm2ut17TextWriterBase<w>FUl` | 0x803D3760 | 32 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetBufferSize__Q36nw4hbm2ut17TextWriterBase<w>Fv` | 0x803D3780 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `GetBuffer__Q36nw4hbm2ut17TextWriterBase<w>Fv` | 0x803D3790 | 12 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `VSNPrintf__Q36nw4hbm2ut17TextWriterBase<w>FPwUlPCwP16__va_list_struct` | 0x803D37A0 | 4 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `StrLen__Q36nw4hbm2ut17TextWriterBase<w>FPCw` | 0x803D37B0 | 4 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcLineWidth__Q36nw4hbm2ut17TextWriterBase<w>FPCwi` | 0x803D37C0 | 360 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcLineRectImpl__Q36nw4hbm2ut17TextWriterBase<w>FPQ36nw4hbm2ut4RectPCwi` | 0x803D3930 | 1012 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `CalcStringRectImpl__Q36nw4hbm2ut17TextWriterBase<w>FPQ36nw4hbm2ut4RectPCwi` | 0x803D3D30 | 280 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `PrintImpl__Q36nw4hbm2ut17TextWriterBase<w>FPCwi` | 0x803D3E50 | 1488 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `AdjustCursor__Q36nw4hbm2ut17TextWriterBase<w>FPfPfPCwi` | 0x803D4420 | 1260 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `IsDrawFlagSet__Q36nw4hbm2ut17TextWriterBase<w>CFUlUl` | 0x803D4910 | 24 | matched |  |
| `revolution/hbm/nw4hbm/ut/ut_TextWriterBase` | `__sinit_\ut_TextWriterBase_cpp` | 0x803D4930 | 164 | matched | placeholder (compiler-generated name) |
| `revolution/hbm/sound/mix` | `__HBMMIXSetPan__FP13HBMMIXChannel` | 0x803D49E0 | 160 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXInit` | 0x803D4A80 | 512 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXQuit` | 0x803D4C80 | 24 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXSetSoundMode` | 0x803D4CA0 | 12 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXInitChannel` | 0x803D4CB0 | 2376 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXReleaseChannel` | 0x803D5600 | 44 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXSetInput` | 0x803D5630 | 56 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXSetAuxA` | 0x803D5670 | 56 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXSetPan` | 0x803D56B0 | 128 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXSetFader` | 0x803D5730 | 56 | matched |  |
| `revolution/hbm/sound/mix` | `HBMMIXUpdateSettings` | 0x803D5770 | 2808 | matched |  |
| `revolution/hbm/sound/mix` | `HBMAllocIndex` | 0x803D6270 | 88 | matched |  |
| `revolution/hbm/sound/mix` | `HBMGetIndex` | 0x803D62D0 | 20 | matched |  |
| `revolution/hbm/sound/mix` | `HBMFreeIndex` | 0x803D62F0 | 40 | matched |  |
| `revolution/hbm/sound/mix` | `HBMFreeIndexByKey` | 0x803D6320 | 44 | matched |  |
| `revolution/hbm/sound/syn` | `__HBMSYNRemoveSynthFromList__FP11HBMSYNSYNTH` | 0x803D6350 | 144 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNInit` | 0x803D63E0 | 216 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNQuit` | 0x803D64C0 | 24 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNRunAudioFrame` | 0x803D64E0 | 108 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNInitSynth` | 0x803D6550 | 392 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNQuitSynth` | 0x803D66E0 | 168 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNMidiInput` | 0x803D6790 | 80 | matched |  |
| `revolution/hbm/sound/syn` | `HBMSYNSetMasterVolume` | 0x803D67E0 | 12 | matched |  |
| `revolution/hbm/sound/synctrl` | `__HBMSYNResetAllControllers` | 0x803D67F0 | 220 | matched |  |
| `revolution/hbm/sound/synctrl` | `__HBMSYNNoteOn__FP11HBMSYNSYNTHUcUcUc` | 0x803D68D0 | 556 | matched |  |
| `revolution/hbm/sound/synctrl` | `__HBMSYNMidiIn__FP11HBMSYNSYNTHPUc` | 0x803D6B00 | 284 | matched |  |
| `revolution/hbm/sound/synctrl` | `__HBMSYNRunInputBufferEvents` | 0x803D6C20 | 104 | matched |  |
| `revolution/hbm/sound/synenv` | `__HBMSYNSetupVolumeEnvelope` | 0x803D6C90 | 716 | matched |  |
| `revolution/hbm/sound/synenv` | `__HBMSYNRunVolumeEnvelope` | 0x803D6F60 | 268 | matched |  |
| `revolution/hbm/sound/synmix` | `__HBMSYNSetupVolume` | 0x803D7070 | 40 | matched |  |
| `revolution/hbm/sound/synmix` | `__HBMSYNSetupPan` | 0x803D70A0 | 24 | matched |  |
| `revolution/hbm/sound/synmix` | `__HBMSYNGetVoiceInput` | 0x803D70C0 | 20 | matched |  |
| `revolution/hbm/sound/synmix` | `__HBMSYNGetVoiceFader` | 0x803D70E0 | 36 | matched |  |
| `revolution/hbm/sound/synmix` | `__HBMSYNUpdateMix` | 0x803D7110 | 160 | matched |  |
| `revolution/hbm/sound/synpitch` | `__HBMSYNGetRelativePitch` | 0x803D71B0 | 272 | matched |  |
| `revolution/hbm/sound/synpitch` | `__HBMSYNSetupPitch` | 0x803D72C0 | 104 | matched |  |
| `revolution/hbm/sound/synpitch` | `__HBMSYNSetupSrc` | 0x803D7330 | 156 | matched |  |
| `revolution/hbm/sound/synpitch` | `__HBMSYNUpdateSrc` | 0x803D73D0 | 92 | matched |  |
| `revolution/hbm/sound/synsample` | `__HBMSYNSetupAdpcm__FP11HBMSYNVOICE` | 0x803D7430 | 532 | matched |  |
| `revolution/hbm/sound/synsample` | `__HBMSYNSetupPcm16__FP11HBMSYNVOICE` | 0x803D7650 | 272 | matched |  |
| `revolution/hbm/sound/synsample` | `__HBMSYNSetupPcm8__FP11HBMSYNVOICE` | 0x803D7760 | 272 | matched |  |
| `revolution/hbm/sound/synsample` | `__HBMSYNSetupSample` | 0x803D7870 | 52 | matched |  |
| `revolution/hbm/sound/synvoice` | `__HBMSYNClearVoiceReferences` | 0x803D78B0 | 164 | matched |  |
| `revolution/hbm/sound/synvoice` | `__HBMSYNSetVoiceToRelease` | 0x803D7960 | 12 | matched |  |
| `revolution/hbm/sound/synvoice` | `__HBMSYNServiceVoice` | 0x803D7970 | 240 | matched |  |
| `revolution/hbm/sound/seq` | `__HBMSEQRunEvent__FP11HBMSYNSYNTHP12_HBMSEQTRACK` | 0x803D7A60 | 708 | matched |  |
| `revolution/hbm/sound/seq` | `__HBMSEQInitTracks__FP15_HBMSEQSEQUENCEPUci` | 0x803D7D30 | 284 | matched |  |
| `revolution/hbm/sound/seq` | `__HBMSEQReadHeader__FP15_HBMSEQSEQUENCEPUc` | 0x803D7E50 | 136 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQInit` | 0x803D7EE0 | 40 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQQuit` | 0x803D7F10 | 24 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQRunAudioFrame` | 0x803D7F30 | 392 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQAddSequence` | 0x803D80C0 | 144 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQRemoveSequence` | 0x803D8150 | 152 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQSetState` | 0x803D81F0 | 324 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQGetState` | 0x803D8340 | 8 | matched |  |
| `revolution/hbm/sound/seq` | `HBMSEQSetVolume` | 0x803D8350 | 8 | matched |  |
| `zlib/adler32` | `adler32` | 0x801C9624 | 1056 | matched |  |
| `zlib/crc32` | `crc32` | 0x801C9A44 | 76 | matched |  |
| `zlib/crc32` | `crc32_little` | 0x801C9A90 | 936 | matched |  |
| `zlib/crc32` | `crc32_big` | 0x801C9E38 | 1048 | matched |  |
| `zlib/inffast` | `inflate_fast` | 0x801CA250 | 1300 | matched |  |
| `zlib/inflate` | `inflateReset` | 0x801CA764 | 128 | matched |  |
| `zlib/inflate` | `inflateInit2_` | 0x801CA7E4 | 336 | matched |  |
| `zlib/inflate` | `inflateInit_` | 0x801CA934 | 16 | matched |  |
| `zlib/inflate` | `updatewindow` | 0x801CA944 | 380 | matched |  |
| `zlib/inflate` | `inflate` | 0x801CAAC0 | 6024 | matched |  |
| `zlib/inflate` | `inflateEnd` | 0x801CC248 | 136 | matched |  |
| `zlib/inftrees` | `inflate_table` | 0x801CC2D0 | 1904 | matched |  |
| `zlib/zutil` | `zcalloc` | 0x801CCA40 | 8 | matched |  |
| `zlib/zutil` | `zcfree` | 0x801CCA48 | 48 | matched |  |
| `rwsdk/src/babbox` | `RwBBoxCalculate` | 0x80290F00 | 184 | matched |  |
| `rwsdk/src/babincam` | `RwCameraRegisterPluginStream` | 0x80290FB8 | 40 | matched |  |
| `rwsdk/src/babincam` | `RwCameraStreamRead` | 0x80290FE0 | 352 | matched |  |
| `rwsdk/src/babinfrm` | `RwFrameRegisterPluginStream` | 0x80291140 | 40 | matched |  |
| `rwsdk/src/babinfrm` | `_rwFrameListDeinitialize` | 0x80291168 | 76 | matched |  |
| `rwsdk/src/babinfrm` | `_rwFrameListStreamRead` | 0x802911B4 | 908 | matched |  |
| `rwsdk/src/babintex` | `RwTextureRegisterPluginStream` | 0x80291540 | 40 | matched |  |
| `rwsdk/src/babintex` | `RwTextureStreamGetSize` | 0x80291568 | 100 | matched |  |
| `rwsdk/src/babintex` | `RwTextureStreamWrite` | 0x802915CC | 384 | matched |  |
| `rwsdk/src/babintex` | `RwTextureStreamRead` | 0x8029174C | 584 | matched |  |
| `rwsdk/src/babintex` | `destroyTexture` | 0x80291994 | 48 | matched |  |
| `rwsdk/src/babintex` | `RwTexDictionaryStreamRead` | 0x802919C4 | 764 | matched |  |
| `rwsdk/src/bacamera` | `CameraUpdateZShiftScale` | 0x80291CC0 | 132 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraBuildPerspClipPlanes` | 0x80291D44 | 2344 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraBuildPerspViewMatrix` | 0x8029266C | 432 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraBuildParallelClipPlanes` | 0x8029281C | 1404 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraBuildParallelViewMatrix` | 0x80292D98 | 428 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraSync` | 0x80292F44 | 100 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraEndUpdate` | 0x80292FA8 | 100 | parked, 100% |  |
| `rwsdk/src/bacamera` | `CameraBeginUpdate` | 0x8029300C | 108 | parked, 100% |  |
| `rwsdk/src/bacamera` | `_rwCameraClose` | 0x80293078 | 100 | parked, 100% |  |
| `rwsdk/src/bacamera` | `_rwCameraOpen` | 0x802930DC | 144 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraEndUpdate` | 0x8029316C | 12 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraBeginUpdate` | 0x80293178 | 12 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraSetViewOffset` | 0x80293184 | 80 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraSetNearClipPlane` | 0x802931D4 | 68 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraSetFarClipPlane` | 0x80293218 | 68 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraFrustumTestSphere` | 0x8029325C | 332 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraClear` | 0x802933A8 | 76 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraShowRaster` | 0x802933F4 | 68 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraSetProjection` | 0x80293438 | 148 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraSetViewWindow` | 0x802934CC | 108 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraRegisterPlugin` | 0x80293538 | 48 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraDestroy` | 0x80293568 | 96 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraCreate` | 0x802935C8 | 260 | parked, 100% |  |
| `rwsdk/src/bacamera` | `RwCameraClone` | 0x802936CC | 224 | parked, 93.3% |  |
| `rwsdk/src/badevice` | `CorePluginAttach` | 0x802937AC | 624 | matched |  |
| `rwsdk/src/badevice` | `MallocWrapper` | 0x80293A1C | 20 | matched |  |
| `rwsdk/src/badevice` | `FreeWrapper` | 0x80293A30 | 64 | matched |  |
| `rwsdk/src/badevice` | `EngineOpen` | 0x80293A70 | 340 | matched |  |
| `rwsdk/src/badevice` | `_rwDeviceSystemRequest` | 0x80293BC4 | 300 | matched |  |
| `rwsdk/src/badevice` | `_rwGetNumEngineInstances` | 0x80293CF0 | 8 | matched |  |
| `rwsdk/src/badevice` | `RwEngineGetVersion` | 0x80293CF8 | 12 | matched |  |
| `rwsdk/src/badevice` | `RwEngineRegisterPlugin` | 0x80293D04 | 44 | matched |  |
| `rwsdk/src/badevice` | `RwEngineGetPluginOffset` | 0x80293D30 | 16 | matched |  |
| `rwsdk/src/badevice` | `RwEngineGetNumSubSystems` | 0x80293D40 | 68 | matched |  |
| `rwsdk/src/badevice` | `RwEngineGetSubSystemInfo` | 0x80293D84 | 84 | matched |  |
| `rwsdk/src/badevice` | `RwEngineSetSubSystem` | 0x80293DD8 | 68 | matched |  |
| `rwsdk/src/badevice` | `RwEngineGetNumVideoModes` | 0x80293E1C | 76 | matched |  |
| `rwsdk/src/badevice` | `RwEngineGetVideoModeInfo` | 0x80293E68 | 84 | matched |  |
| `rwsdk/src/badevice` | `RwEngineSetVideoMode` | 0x80293EBC | 68 | matched |  |
| `rwsdk/src/badevice` | `RwEngineStop` | 0x80293F00 | 128 | matched |  |
| `rwsdk/src/badevice` | `RwEngineStart` | 0x80293F80 | 184 | matched |  |
| `rwsdk/src/badevice` | `RwEngineClose` | 0x80294038 | 160 | matched |  |
| `rwsdk/src/badevice` | `RwEngineOpen` | 0x802940D8 | 244 | matched |  |
| `rwsdk/src/badevice` | `RwEngineTerm` | 0x802941CC | 76 | matched |  |
| `rwsdk/src/badevice` | `RwEngineInit` | 0x80294218 | 300 | matched |  |
| `rwsdk/src/baframe` | `RwFrameSetFreeListCreateParams` | 0x80294344 | 12 | matched |  |
| `rwsdk/src/baframe` | `_rwFrameOpen` | 0x80294350 | 164 | matched |  |
| `rwsdk/src/baframe` | `_rwFrameClose` | 0x802943F4 | 100 | matched |  |
| `rwsdk/src/baframe` | `rwSetHierarchyRoot` | 0x80294458 | 84 | matched |  |
| `rwsdk/src/baframe` | `rwFrameCloneRecurse` | 0x802944AC | 436 | matched |  |
| `rwsdk/src/baframe` | `_rwFrameCloneAndLinkClones` | 0x80294660 | 76 | matched |  |
| `rwsdk/src/baframe` | `_rwFramePurgeClone` | 0x802946AC | 76 | matched |  |
| `rwsdk/src/baframe` | `RwFrameDirty` | 0x802946F8 | 16 | matched |  |
| `rwsdk/src/baframe` | `_rwFrameInit` | 0x80294708 | 196 | matched |  |
| `rwsdk/src/baframe` | `RwFrameCreate` | 0x802947CC | 96 | matched |  |
| `rwsdk/src/baframe` | `_rwFrameDeInit` | 0x8029482C | 140 | matched |  |
| `rwsdk/src/baframe` | `RwFrameDestroy` | 0x802948B8 | 76 | matched |  |
| `rwsdk/src/baframe` | `FrameDestroyRecurseDeInitLeaf` | 0x80294904 | 92 | matched |  |
| `rwsdk/src/baframe` | `rwFrameDestroyRecurseDestroyLeaf` | 0x80294960 | 72 | matched |  |
| `rwsdk/src/baframe` | `rwFrameDestroyRecurse` | 0x802949A8 | 92 | matched |  |
| `rwsdk/src/baframe` | `RwFrameDestroyHierarchy` | 0x80294A04 | 36 | matched |  |
| `rwsdk/src/baframe` | `RwFrameUpdateObjects` | 0x80294A28 | 108 | matched |  |
| `rwsdk/src/baframe` | `RwFrameGetLTM` | 0x80294A94 | 68 | matched |  |
| `rwsdk/src/baframe` | `RwFrameGetRoot` | 0x80294AD8 | 8 | matched |  |
| `rwsdk/src/baframe` | `RwFrameAddChildNoUpdate` | 0x80294AE0 | 104 | matched |  |
| `rwsdk/src/baframe` | `RwFrameAddChild` | 0x80294B48 | 160 | matched |  |
| `rwsdk/src/baframe` | `RwFrameRemoveChild` | 0x80294BE8 | 132 | matched |  |
| `rwsdk/src/baframe` | `RwFrameForAllChildren` | 0x80294C6C | 132 | matched |  |
| `rwsdk/src/baframe` | `RwFrameTranslate` | 0x80294CF0 | 60 | matched |  |
| `rwsdk/src/baframe` | `RwFrameTransform` | 0x80294D2C | 60 | matched |  |
| `rwsdk/src/baframe` | `RwFrameRotate` | 0x80294D68 | 60 | matched |  |
| `rwsdk/src/baframe` | `RwFrameSetIdentity` | 0x80294DA4 | 120 | matched |  |
| `rwsdk/src/baframe` | `RwFrameRegisterPlugin` | 0x80294E1C | 48 | matched |  |
| `rwsdk/src/baframe` | `RwFrameGetPluginOffset` | 0x80294E4C | 16 | matched |  |
| `rwsdk/src/baimage` | `_rwImageOpen` | 0x80294E5C | 660 | matched |  |
| `rwsdk/src/baimage` | `_rwImageClose` | 0x802950F0 | 356 | matched |  |
| `rwsdk/src/baimage` | `RwImageCreate` | 0x80295254 | 172 | matched |  |
| `rwsdk/src/baimage` | `RwImageDestroy` | 0x80295300 | 144 | matched |  |
| `rwsdk/src/baimage` | `RwImageAllocatePixels` | 0x80295390 | 260 | matched |  |
| `rwsdk/src/baimage` | `RwImageFreePixels` | 0x80295494 | 88 | matched |  |
| `rwsdk/src/baimage` | `RwImageMakeMask` | 0x802954EC | 216 | matched |  |
| `rwsdk/src/baimage` | `RwImageApplyMask` | 0x802955C4 | 1248 | matched |  |
| `rwsdk/src/baimage` | `fn_80295AA4` | 0x80295AA4 | 240 | matched | placeholder |
| `rwsdk/src/baimage` | `fn_80295B94` | 0x80295B94 | 816 | matched | placeholder |
| `rwsdk/src/baimage` | `ImageAttempRead` | 0x80295EC4 | 104 | matched |  |
| `rwsdk/src/baimage` | `RwImageRead` | 0x80295F2C | 376 | matched |  |
| `rwsdk/src/baimage` | `ImageDetermineExtender` | 0x802960A4 | 232 | matched |  |
| `rwsdk/src/baimage` | `RwImageFindFileType` | 0x8029618C | 60 | matched |  |
| `rwsdk/src/baimage` | `RwImageRegisterImageFormat` | 0x802961C8 | 412 | matched |  |
| `rwsdk/src/baimage` | `RwImageReadMaskedImage` | 0x80296364 | 772 | matched |  |
| `rwsdk/src/baimage` | `RwRGBASetFromPixel` | 0x80296668 | 68 | matched |  |
| `rwsdk/src/baimage` | `fn_802966AC` | 0x802966AC | 576 | matched | placeholder |
| `rwsdk/src/baimage` | `RwImageCopy` | 0x802968EC | 236 | matched |  |
| `rwsdk/src/baimage` | `RwImageGammaCorrect` | 0x802969D8 | 876 | matched |  |
| `rwsdk/src/baimage` | `RwImageSetGamma` | 0x80296D44 | 376 | matched |  |
| `rwsdk/src/baimras` | `RwImageSetFromRaster` | 0x80296EBC | 116 | matched |  |
| `rwsdk/src/baimras` | `RwRasterSetFromImage` | 0x80296F30 | 116 | matched |  |
| `rwsdk/src/baimras` | `RwImageFindRasterFormat` | 0x80296FA4 | 148 | matched |  |
| `rwsdk/src/baraster` | `RwRasterUnlock` | 0x80297038 | 72 | matched |  |
| `rwsdk/src/baraster` | `RwRasterUnlockPalette` | 0x80297080 | 84 | matched |  |
| `rwsdk/src/baraster` | `RwRasterDestroy` | 0x802970D4 | 120 | matched |  |
| `rwsdk/src/baraster` | `RwRasterRegisterPlugin` | 0x8029714C | 48 | matched |  |
| `rwsdk/src/baraster` | `RwRasterLockPalette` | 0x8029717C | 80 | matched |  |
| `rwsdk/src/baraster` | `RwRasterGetNumLevels` | 0x802971CC | 96 | matched |  |
| `rwsdk/src/baraster` | `RwRasterShowRaster` | 0x8029722C | 128 | matched |  |
| `rwsdk/src/baraster` | `RwRasterCreate` | 0x802972AC | 248 | matched |  |
| `rwsdk/src/baraster` | `RwRasterLock` | 0x802973A4 | 84 | matched |  |
| `rwsdk/src/baraster` | `_rwRasterClose` | 0x802973F8 | 108 | matched |  |
| `rwsdk/src/baraster` | `_rwRasterOpen` | 0x80297464 | 332 | matched |  |
| `rwsdk/src/baresamp` | `ImageResampleGetSpan` | 0x802975B0 | 852 | matched |  |
| `rwsdk/src/baresamp` | `ImageResampleGetAvgPixel` | 0x80297904 | 660 | matched |  |
| `rwsdk/src/baresamp` | `RwImageResample` | 0x80297B98 | 484 | matched |  |
| `rwsdk/src/baresamp` | `RwImageCreateResample` | 0x80297D7C | 368 | matched |  |
| `rwsdk/src/basync` | `FrameSyncHierarchyRecurse` | 0x80297EEC | 168 | matched |  |
| `rwsdk/src/basync` | `FrameSyncHierarchyRecurseNoLTM` | 0x80297F94 | 140 | matched |  |
| `rwsdk/src/basync` | `FrameSyncHierarchy` | 0x80298020 | 316 | matched |  |
| `rwsdk/src/basync` | `_rwFrameSyncDirty` | 0x8029815C | 104 | matched |  |
| `rwsdk/src/basync` | `FrameSyncHierarchyLTMRecurse` | 0x802981C4 | 136 | matched |  |
| `rwsdk/src/basync` | `_rwFrameSyncHierarchyLTM` | 0x8029824C | 204 | matched |  |
| `rwsdk/src/batextur` | `TextureDefaultMipmapName` | 0x80298318 | 196 | matched |  |
| `rwsdk/src/batextur` | `PalettizeImage` | 0x802983DC | 240 | matched |  |
| `rwsdk/src/batextur` | `PalettizeMipmaps` | 0x802984CC | 460 | matched |  |
| `rwsdk/src/batextur` | `TextureImageReadAndSize` | 0x80298698 | 848 | matched |  |
| `rwsdk/src/batextur` | `TextureDefaultNormalRead` | 0x802989E8 | 1144 | matched |  |
| `rwsdk/src/batextur` | `TextureDefaultMipmapRead` | 0x80298E60 | 1908 | matched |  |
| `rwsdk/src/batextur` | `TextureDefaultRead` | 0x802995D4 | 32 | matched |  |
| `rwsdk/src/batextur` | `TextureRasterDefaultBuildMipmaps` | 0x802995F4 | 756 | matched |  |
| `rwsdk/src/batextur` | `StringCompare` | 0x802998E8 | 144 | matched |  |
| `rwsdk/src/batextur` | `TextureDefaultFind` | 0x80299978 | 232 | matched |  |
| `rwsdk/src/batextur` | `RwTextureSetMipmapping` | 0x80299A60 | 24 | matched |  |
| `rwsdk/src/batextur` | `RwTextureGetMipmapping` | 0x80299A78 | 20 | matched |  |
| `rwsdk/src/batextur` | `RwTextureSetAutoMipmapping` | 0x80299A8C | 24 | matched |  |
| `rwsdk/src/batextur` | `RwTextureGetAutoMipmapping` | 0x80299AA4 | 20 | matched |  |
| `rwsdk/src/batextur` | `RwTextureSetRaster` | 0x80299AB8 | 100 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryCreate` | 0x80299B1C | 216 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryDestroy` | 0x80299BF4 | 212 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryForAllTextures` | 0x80299CC8 | 104 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryForAllTexDictionaries` | 0x80299D30 | 132 | matched |  |
| `rwsdk/src/batextur` | `RwTextureCreate` | 0x80299DB4 | 152 | matched |  |
| `rwsdk/src/batextur` | `RwTextureDestroy` | 0x80299E4C | 196 | matched |  |
| `rwsdk/src/batextur` | `RwTextureSetName` | 0x80299F10 | 172 | matched |  |
| `rwsdk/src/batextur` | `RwTextureSetMaskName` | 0x80299FBC | 172 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryAddTexture` | 0x8029A068 | 80 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryRemoveTexture` | 0x8029A0B8 | 48 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryFindNamedTexture` | 0x8029A0E8 | 128 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionarySetCurrent` | 0x8029A168 | 20 | matched |  |
| `rwsdk/src/batextur` | `RwTexDictionaryGetCurrent` | 0x8029A17C | 20 | matched |  |
| `rwsdk/src/batextur` | `RwTextureRead` | 0x8029A190 | 336 | matched |  |
| `rwsdk/src/batextur` | `RwTextureRegisterPlugin` | 0x8029A2E0 | 48 | matched |  |
| `rwsdk/src/batextur` | `RwTextureRasterGenerateMipmaps` | 0x8029A310 | 64 | matched |  |
| `rwsdk/src/batextur` | `_rwTextureClose` | 0x8029A350 | 480 | matched |  |
| `rwsdk/src/batextur` | `_rwTextureOpen` | 0x8029A530 | 740 | matched |  |
| `rwsdk/src/batypehf` | `_rwObjectHasFrameSetFrame` | 0x8029A814 | 92 | matched |  |
| `rwsdk/src/batypehf` | `_rwObjectHasFrameReleaseFrame` | 0x8029A870 | 40 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `RwIm3DTransform` | 0x8029A898 | 224 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `RwIm3DEnd` | 0x8029A978 | 80 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `RwIm3DRenderIndexedPrimitive` | 0x8029A9C8 | 468 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `RwIm3DRenderPrimitive` | 0x8029AB9C | 440 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `RwIm3DSetTransformPipeline` | 0x8029AD54 | 80 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `RwIm3DSetRenderPipeline` | 0x8029ADA4 | 788 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `_rwIm3DClose` | 0x8029B0B8 | 100 | matched |  |
| `rwsdk/src/pipe/p2/baim3d` | `_rwIm3DOpen` | 0x8029B11C | 208 | matched |  |
| `rwsdk/src/pipe/p2/bapipe` | `_rwRenderPipelineOpen` | 0x8029B1EC | 68 | matched |  |
| `rwsdk/src/pipe/p2/bapipe` | `_rwRenderPipelineClose` | 0x8029B230 | 48 | matched |  |
| `rwsdk/src/pipe/p2/bapipe` | `_rwPipeAttach` | 0x8029B260 | 8 | matched |  |
| `rwsdk/src/pipe/p2/bapipe` | `_rwPipeInitForCamera` | 0x8029B268 | 4 | matched |  |
| `rwsdk/src/pipe/p2/p2altmdl` | `_rxPacketDestroy` | 0x8029B26C | 172 | matched |  |
| `rwsdk/src/pipe/p2/p2core` | `_rxPipelineClose` | 0x8029B318 | 100 | matched |  |
| `rwsdk/src/pipe/p2/p2core` | `_rxPipelineOpen` | 0x8029B37C | 244 | matched |  |
| `rwsdk/src/pipe/p2/p2core` | `fn_8029B470` | 0x8029B470 | 784 | matched | placeholder |
| `rwsdk/src/pipe/p2/p2core` | `RxHeapGetGlobalHeap` | 0x8029B780 | 8 | matched |  |
| `rwsdk/src/pipe/p2/p2core` | `RxPipelineExecute` | 0x8029B788 | 240 | matched |  |
| `rwsdk/src/pipe/p2/p2core` | `RxPipelineCreate` | 0x8029B878 | 152 | matched |  |
| `rwsdk/src/pipe/p2/p2core` | `_rxPipelineDestroy` | 0x8029B910 | 188 | matched |  |
| `rwsdk/src/pipe/p2/p2define` | `StalacTiteAlloc` | 0x8029B9CC | 112 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `StalacMiteAlloc` | 0x8029BA3C | 120 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `PipelineCalcNumUniqueClusters` | 0x8029BAB4 | 140 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `ReallocAndFixupSuperBlock` | 0x8029BB40 | 408 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `LockPipelineExpandData` | 0x8029BCD8 | 536 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `fn_8029BEF0` | 0x8029BEF0 | 408 | parked, 100% | placeholder |
| `rwsdk/src/pipe/p2/p2define` | `PipelineTallyInputs` | 0x8029C088 | 180 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `fn_8029C13C` | 0x8029C13C | 1004 | parked, 100% | placeholder |
| `rwsdk/src/pipe/p2/p2define` | `PipelineUnlockTopSort` | 0x8029C528 | 348 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `RxLockedPipeUnlock` | 0x8029C684 | 1504 | parked, 99.8% |  |
| `rwsdk/src/pipe/p2/p2define` | `RxPipelineLock` | 0x8029CC64 | 404 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `RxPipelineFindNodeByName` | 0x8029CDF8 | 256 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2define` | `RxLockedPipeAddFragment` | 0x8029CEF8 | 808 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `_ReqAddEntry` | 0x8029D220 | 168 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `_PropDownElimPath` | 0x8029D2C8 | 456 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `_ScopeTraceMerge` | 0x8029D490 | 112 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `fn_8029D500` | 0x8029D500 | 1000 | parked, 99.7% | placeholder |
| `rwsdk/src/pipe/p2/p2dep` | `_ForAllNodeReqsAddOutputClustersAndBuildContinuityBitfields` | 0x8029D8E8 | 620 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `_TraceClusterScopes` | 0x8029DB54 | 552 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `_AssignClusterSlots` | 0x8029DD7C | 188 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `fn_8029DE38` | 0x8029DE38 | 160 | parked, 100% | placeholder |
| `rwsdk/src/pipe/p2/p2dep` | `_MyEnumPipelineClustersCallBack` | 0x8029DED8 | 88 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2dep` | `fn_8029DF30` | 0x8029DF30 | 952 | parked, 100% | placeholder |
| `rwsdk/src/pipe/p2/p2dep` | `_rxChaseDependencies` | 0x8029E2E8 | 128 | parked, 100% |  |
| `rwsdk/src/pipe/p2/p2heap` | `HeapFreeBlocksNewEntry` | 0x8029E368 | 268 | matched |  |
| `rwsdk/src/pipe/p2/p2heap` | `HeapSuperBlockReset` | 0x8029E474 | 252 | matched |  |
| `rwsdk/src/pipe/p2/p2heap` | `RxHeapFree` | 0x8029E570 | 472 | matched |  |
| `rwsdk/src/pipe/p2/p2heap` | `_rxHeapReset` | 0x8029E748 | 204 | matched |  |
| `rwsdk/src/pipe/p2/p2heap` | `RxHeapDestroy` | 0x8029E814 | 160 | matched |  |
| `rwsdk/src/pipe/p2/p2heap` | `RxHeapCreate` | 0x8029E8B4 | 316 | matched |  |
| `rwsdk/src/pipe/p2/p2renderstate` | `RxRenderStateVectorSetDefaultRenderStateVector` | 0x8029E9F0 | 332 | matched |  |
| `rwsdk/src/pipe/p2/p2resort` | `_repartition` | 0x8029EB3C | 496 | matched |  |
| `rwsdk/src/pipe/p2/p2resort` | `_insertionsort` | 0x8029ED2C | 388 | matched |  |
| `rwsdk/src/pipe/p2/p2resort` | `_rx_rxRadixExchangeSort` | 0x8029EEB0 | 580 | matched |  |
| `rwsdk/src/pipe/p2/wii/im3dpipe` | `_rwIm3DCreatePlatformTransformPipeline` | 0x8029F0F4 | 168 | matched |  |
| `rwsdk/src/pipe/p2/wii/im3dpipe` | `_rwIm3DDestroyPlatformTransformPipeline` | 0x8029F19C | 72 | matched |  |
| `rwsdk/src/pipe/p2/wii/im3dpipe` | `_rwIm3DDestroyPlatformRenderPipelines` | 0x8029F1E4 | 140 | matched |  |
| `rwsdk/src/pipe/p2/wii/im3dpipe` | `_rwIm3DCreatePlatformRenderPipelines` | 0x8029F270 | 236 | matched |  |
| `rwsdk/src/plcore/babinary` | `ChunkIsComplex` | 0x8029FF00 | 188 | matched |  |
| `rwsdk/src/plcore/babinary` | `_rwStreamReadChunkHeader` | 0x8029FFBC | 384 | matched |  |
| `rwsdk/src/plcore/babinary` | `_rwStreamWriteVersionedChunkHeader` | 0x802A013C | 128 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamFindChunk` | 0x802A01BC | 316 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwMemLittleEndian32` | 0x802A02F8 | 196 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwMemLittleEndian16` | 0x802A03BC | 200 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwMemNative32` | 0x802A0484 | 196 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwMemNative16` | 0x802A0548 | 200 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamWriteReal` | 0x802A0610 | 352 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamWriteInt32` | 0x802A0770 | 352 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamWriteInt16` | 0x802A08D0 | 356 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamReadInt16` | 0x802A0A34 | 316 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamReadReal` | 0x802A0B70 | 312 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamReadInt32` | 0x802A0CA8 | 312 | matched |  |
| `rwsdk/src/plcore/babinary` | `RwStreamReadChunkHeaderInfo` | 0x802A0DE0 | 136 | matched |  |
| `rwsdk/src/plcore/bacolor` | `_rwColorOpen` | 0x802A0E68 | 20 | matched |  |
| `rwsdk/src/plcore/bacolor` | `_rwColorClose` | 0x802A0E7C | 20 | matched |  |
| `rwsdk/src/plcore/baerr` | `_rwErrorOpen` | 0x802A0E90 | 56 | matched |  |
| `rwsdk/src/plcore/baerr` | `_rwErrorClose` | 0x802A0EC8 | 20 | matched |  |
| `rwsdk/src/plcore/baerr` | `RwErrorSet` | 0x802A0EDC | 96 | matched |  |
| `rwsdk/src/plcore/baerr` | `RwErrorGet` | 0x802A0F3C | 64 | matched |  |
| `rwsdk/src/plcore/baerr` | `_rwerror` | 0x802A0F7C | 104 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListCreate` | 0x802A0FE4 | 280 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListGetNewEntry` | 0x802A10FC | 228 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListGetNewEntries` | 0x802A11E0 | 252 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListDestroyEndEntries` | 0x802A12DC | 16 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListDestroy` | 0x802A12EC | 120 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListGetNumEntries` | 0x802A1364 | 8 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListGetEntry` | 0x802A136C | 20 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListGetBegin` | 0x802A1380 | 8 | matched |  |
| `rwsdk/src/plcore/balist` | `_rwSListGetEnd` | 0x802A1388 | 24 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `MatrixMultiply` | 0x802A13A0 | 736 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `MatrixOrthoNormalize` | 0x802A1680 | 1216 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `MatrixInvertGeneric` | 0x802A1B40 | 560 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `MatrixQueryHalfTurnAxis` | 0x802A1D70 | 372 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `MatrixQueryRotate` | 0x802A1EE4 | 704 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `_rwMatrixDeterminant` | 0x802A21A4 | 116 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `_rwMatrixOrthogonalError` | 0x802A2218 | 140 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `_rwMatrixNormalError` | 0x802A22A4 | 156 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `_rwMatrixClose` | 0x802A2340 | 100 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `_rwMatrixOpen` | 0x802A23A4 | 224 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwEngineGetMatrixTolerances` | 0x802A2484 | 44 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixOptimize` | 0x802A24B0 | 628 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixUpdate` | 0x802A2724 | 24 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixMultiply` | 0x802A273C | 252 | ORIGINAL-ASM |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixOrthoNormalize` | 0x802A2838 | 4 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixRotateOneMinusCosineSine` | 0x802A283C | 808 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixRotate` | 0x802A2B64 | 232 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixInvert` | 0x802A2C4C | 460 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixScale` | 0x802A2E18 | 500 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixTranslate` | 0x802A300C | 412 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixTransform` | 0x802A31A8 | 556 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixQueryRotate` | 0x802A33D4 | 4 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixDestroy` | 0x802A33D8 | 60 | matched |  |
| `rwsdk/src/plcore/bamatrix` | `RwMatrixCreate` | 0x802A3414 | 144 | matched |  |
| `rwsdk/src/plcore/bamemory` | `_rwFreeListEnable` | 0x802A34A4 | 8 | matched |  |
| `rwsdk/src/plcore/bamemory` | `FreeListCreate` | 0x802A34AC | 552 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwFreeListCreate` | 0x802A36D4 | 16 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwFreeListCreateAndPreallocateSpace` | 0x802A36E4 | 4 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwFreeListDestroy` | 0x802A36E8 | 212 | matched |  |
| `rwsdk/src/plcore/bamemory` | `_rwFreeListAllocReal` | 0x802A37BC | 412 | matched |  |
| `rwsdk/src/plcore/bamemory` | `_rwFreeListFreeReal` | 0x802A3958 | 396 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwFreeListPurge` | 0x802A3AE4 | 340 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwFreeListForAllUsed` | 0x802A3C38 | 300 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwFreeListPurgeAllFreeLists` | 0x802A3D64 | 104 | matched |  |
| `rwsdk/src/plcore/bamemory` | `DefaultMalloc` | 0x802A3DCC | 4 | matched |  |
| `rwsdk/src/plcore/bamemory` | `DefaultRealloc` | 0x802A3DD0 | 4 | matched |  |
| `rwsdk/src/plcore/bamemory` | `DefaultCalloc` | 0x802A3DD4 | 4 | matched |  |
| `rwsdk/src/plcore/bamemory` | `_rwMemoryOpen` | 0x802A3DD8 | 284 | matched |  |
| `rwsdk/src/plcore/bamemory` | `_rwMemoryClose` | 0x802A3EF4 | 420 | matched |  |
| `rwsdk/src/plcore/bamemory` | `RwOsGetMemoryInterface` | 0x802A4098 | 12 | matched |  |
| `rwsdk/src/plcore/baresour` | `ResourcesInit` | 0x802A40A4 | 292 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/baresour` | `_rwResourcesOpen` | 0x802A41C8 | 96 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/baresour` | `_rwResourcesClose` | 0x802A4228 | 140 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/baresour` | `RwResourcesFreeResEntry` | 0x802A42B4 | 176 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/baresour` | `_rwResourcesPurge` | 0x802A4364 | 180 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/baresour` | `RwResourcesAllocateResEntry` | 0x802A4418 | 672 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/baresour` | `RwResourcesEmptyArena` | 0x802A46B8 | 508 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/bastream` | `_rwStreamModuleOpen` | 0x802A48B4 | 140 | matched |  |
| `rwsdk/src/plcore/bastream` | `_rwStreamModuleClose` | 0x802A4940 | 84 | matched |  |
| `rwsdk/src/plcore/bastream` | `StreamFileNameInitialize` | 0x802A4994 | 260 | matched |  |
| `rwsdk/src/plcore/bastream` | `StreamMemoryInitialize` | 0x802A4A98 | 188 | matched |  |
| `rwsdk/src/plcore/bastream` | `_rwStreamInitialize` | 0x802A4B54 | 264 | matched |  |
| `rwsdk/src/plcore/bastream` | `RwStreamRead` | 0x802A4C5C | 396 | matched |  |
| `rwsdk/src/plcore/bastream` | `RwStreamWrite` | 0x802A4DE8 | 560 | matched |  |
| `rwsdk/src/plcore/bastream` | `RwStreamSkip` | 0x802A5018 | 336 | matched |  |
| `rwsdk/src/plcore/bastream` | `RwStreamClose` | 0x802A5168 | 276 | matched |  |
| `rwsdk/src/plcore/bastream` | `RwStreamOpen` | 0x802A527C | 172 | matched |  |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryAddPluginStream` | 0x802A5328 | 68 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryAddPlgnStrmlwysCB` | 0x802A536C | 60 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryAddPlgnStrmRightsCB` | 0x802A53A8 | 60 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryReadDataChunks` | 0x802A53E4 | 440 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryInvokeRights` | 0x802A559C | 140 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryGetSize` | 0x802A5628 | 132 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistryWriteDataChunks` | 0x802A56AC | 332 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkbin` | `_rwPluginRegistrySkipDataChunks` | 0x802A57F8 | 196 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryOpen` | 0x802A58BC | 92 | matched |  |
| `rwsdk/src/plcore/batkreg` | `rwDestroyEntry` | 0x802A5918 | 72 | matched |  |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryClose` | 0x802A5960 | 284 | matched |  |
| `rwsdk/src/plcore/batkreg` | `PluginDefaultConstructor` | 0x802A5A7C | 4 | matched |  |
| `rwsdk/src/plcore/batkreg` | `PluginDefaultDestructor` | 0x802A5A80 | 4 | matched |  |
| `rwsdk/src/plcore/batkreg` | `PluginDefaultCopy` | 0x802A5A84 | 4 | matched |  |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryGetPluginOffset` | 0x802A5A88 | 48 | matched |  |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryAddPlugin` | 0x802A5AB8 | 696 | matched |  |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryInitObject` | 0x802A5D70 | 168 | matched |  |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryDeInitObject` | 0x802A5E18 | 108 | matched |  |
| `rwsdk/src/plcore/batkreg` | `_rwPluginRegistryCopyObject` | 0x802A5E84 | 124 | matched |  |
| `rwsdk/src/plcore/bavector` | `SqrtTableCreate` | 0x802A5F00 | 308 | matched |  |
| `rwsdk/src/plcore/bavector` | `InvSqrtTableCreate` | 0x802A6034 | 344 | matched |  |
| `rwsdk/src/plcore/bavector` | `VectorMultPoints` | 0x802A618C | 176 | matched |  |
| `rwsdk/src/plcore/bavector` | `VectorMultPoint` | 0x802A623C | 148 | matched |  |
| `rwsdk/src/plcore/bavector` | `VectorMultVectors` | 0x802A62D0 | 152 | matched |  |
| `rwsdk/src/plcore/bavector` | `VectorMultVector` | 0x802A6368 | 124 | matched |  |
| `rwsdk/src/plcore/bavector` | `_rwV3dNormalize` | 0x802A63E4 | 160 | matched |  |
| `rwsdk/src/plcore/bavector` | `RwV3dNormalize` | 0x802A6484 | 232 | matched |  |
| `rwsdk/src/plcore/bavector` | `RwV3dLength` | 0x802A656C | 116 | matched |  |
| `rwsdk/src/plcore/bavector` | `_rwSqrt` | 0x802A65E0 | 84 | matched |  |
| `rwsdk/src/plcore/bavector` | `_rwInvSqrt` | 0x802A6634 | 92 | matched |  |
| `rwsdk/src/plcore/bavector` | `RwV2dLength` | 0x802A6690 | 104 | matched |  |
| `rwsdk/src/plcore/bavector` | `RwV2dNormalize` | 0x802A66F8 | 292 | matched |  |
| `rwsdk/src/plcore/bavector` | `RwV3dTransformPoint` | 0x802A681C | 76 | ORIGINAL-ASM |  |
| `rwsdk/src/plcore/bavector` | `RwV3dTransformPoints` | 0x802A6868 | 96 | ORIGINAL-ASM |  |
| `rwsdk/src/plcore/bavector` | `RwV3dTransformVector` | 0x802A68C8 | 68 | ORIGINAL-ASM |  |
| `rwsdk/src/plcore/bavector` | `_rwVectorClose` | 0x802A690C | 164 | matched |  |
| `rwsdk/src/plcore/bavector` | `_rwVectorOpen` | 0x802A69B0 | 196 | matched |  |
| `rwsdk/src/plcore/resmem` | `ResHeapSplitBlock` | 0x802A6A74 | 140 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/resmem` | `_rwResHeapInit` | 0x802A6B00 | 80 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/resmem` | `_rwResHeapClose` | 0x802A6B50 | 8 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/resmem` | `_rwResHeapFree` | 0x802A6B58 | 184 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/resmem` | `_rwResHeapAlloc` | 0x802A6C10 | 212 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/rwchunkgroup` | `_rwChunkGroupOpen` | 0x802A6CE4 | 140 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/rwchunkgroup` | `_rwChunkGroupClose` | 0x802A6D70 | 84 | matched | unit name unconfirmed |
| `rwsdk/src/plcore/rwstring` | `StrICmp` | 0x802A6DC4 | 144 | matched |  |
| `rwsdk/src/plcore/rwstring` | `StrUpr` | 0x802A6E54 | 60 | matched |  |
| `rwsdk/src/plcore/rwstring` | `StrLwr` | 0x802A6E90 | 60 | matched |  |
| `rwsdk/src/plcore/rwstring` | `StrChr` | 0x802A6ECC | 52 | matched |  |
| `rwsdk/src/plcore/rwstring` | `StrRChr` | 0x802A6F00 | 48 | matched |  |
| `rwsdk/src/plcore/rwstring` | `_rwStringOpen` | 0x802A6F30 | 296 | matched |  |
| `rwsdk/src/plcore/rwstring` | `_rwStringClose` | 0x802A7058 | 4 | matched |  |
| `rwsdk/src/plcore/rwstring` | `_rwStringStreamGetSize` | 0x802A705C | 64 | matched |  |
| `rwsdk/src/plcore/rwstring` | `_rwStringStreamWrite` | 0x802A709C | 192 | matched |  |
| `rwsdk/src/plcore/rwstring` | `StringStreamRead` | 0x802A715C | 380 | matched |  |
| `rwsdk/src/plcore/rwstring` | `UnicodeStringStreamRead` | 0x802A72D8 | 452 | matched |  |
| `rwsdk/src/plcore/rwstring` | `_rwStringStreamFindAndRead` | 0x802A749C | 260 | matched |  |
| `rwsdk/src/driver/wii/dlsprite` | `fn_802B212C` | 0x802B212C | 368 | parked, 1.1% | placeholder |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterRender` | 0x802B229C | 12 | parked, 13.3% |  |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterRenderFast` | 0x802B22A8 | 12 | parked, 13.3% |  |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterRenderScaled` | 0x802B22B4 | 12 | parked, 13.3% |  |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterCamera_ZClearRectInit` | 0x802B22C0 | 812 | parked, 0.5% |  |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterCamera_ZClearRect` | 0x802B25EC | 680 | parked, 0.6% |  |
| `rwsdk/src/driver/wii/dlsprite` | `fn_802B2894` | 0x802B2894 | 968 | parked, 0.4% | placeholder |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterClearRect` | 0x802B2C5C | 8 | parked, 20.0% |  |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlRasterClear` | 0x802B2C64 | 68 | parked, 5.9% |  |
| `rwsdk/src/driver/wii/dlsprite` | `_rwDlSetRasterContext` | 0x802B2CA8 | 12 | parked, 33.3% |  |
| `rwsdk/src/driver/wii/dltexdic` | `_rwDlNativeTextureGetSize` | 0x802B2CB4 | 136 | matched |  |
| `rwsdk/src/driver/wii/dltexdic` | `_rwDlNativeTextureWrite` | 0x802B2D3C | 632 | matched |  |
| `rwsdk/src/driver/wii/dltexdic` | `_rwDlNativeTextureRead` | 0x802B2FB4 | 660 | matched |  |
| `rwsdk/src/driver/wii/dltoken` | `_rwDlTokenQueryDone` | 0x802B395C | 152 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionOpen` | 0x802B39F4 | 32 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionClose` | 0x802B3A14 | 16 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionDataDestructor` | 0x802B3A24 | 84 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionDataConstructor` | 0x802B3A78 | 12 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionDataStreamGetSize` | 0x802B3A84 | 56 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionDataStreamWrite` | 0x802B3ABC | 148 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionDataStreamRead` | 0x802B3B50 | 440 | matched |  |
| `rwsdk/plugin/collis/colldata` | `CollisionAtomicInit` | 0x802B3D08 | 16 | matched |  |
| `rwsdk/plugin/collis/colldata` | `RpCollisionPluginAttach` | 0x802B3D18 | 348 | matched |  |
| `rwsdk/plugin/collis/ctdata` | `_rpCollTreeCreate` | 0x802B8494 | 324 | matched |  |
| `rwsdk/plugin/collis/ctdata` | `_rpCollTreeDestroy` | 0x802B85D8 | 16 | matched |  |
| `rwsdk/plugin/collis/ctdata` | `_rpCollTreeStreamGetSize` | 0x802B85E8 | 44 | matched |  |
| `rwsdk/plugin/collis/ctdata` | `_rpCollTreeStreamWrite` | 0x802B8614 | 520 | matched |  |
| `rwsdk/plugin/collis/ctdata` | `_rpCollTreeStreamRead` | 0x802B881C | 456 | matched |  |
| `rwsdk/plugin/collis/ctdata` | `_rpCollTreeStreamReadPre36001` | 0x802B89E4 | 776 | matched |  |
| `rwsdk/plugin/dmorph/dmphanim` | `RpDMorphAnimationCreate` | 0x802B9080 | 312 | matched |  |
| `rwsdk/plugin/dmorph/dmphanim` | `RpDMorphAnimationDestroy` | 0x802B91B8 | 140 | matched |  |
| `rwsdk/plugin/dmorph/dmphanim` | `RpDMorphAnimationCreateFrames` | 0x802B9244 | 396 | matched |  |
| `rwsdk/plugin/dmorph/dmphanim` | `RpDMorphAnimationStreamRead` | 0x802B93D0 | 464 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `DMorphGeometryCtor` | 0x802B95A0 | 16 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `DMorphGeometryDtor` | 0x802B95B0 | 280 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `DMorphGeometryCopy` | 0x802B96C8 | 4 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `DMorphGeometryStreamRead` | 0x802B96CC | 932 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `DMorphGeometryStreamWrite` | 0x802B9A70 | 564 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `DMorphGeometryStreamSize` | 0x802B9CA4 | 236 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `_rpDMorphGeometryPluginAttach` | 0x802B9D90 | 124 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `RpDMorphGeometryCreateDMorphTargets` | 0x802B9E0C | 216 | matched |  |
| `rwsdk/plugin/dmorph/dmphgeom` | `RpDMorphGeometryGetNumDMorphTargets` | 0x802B9EE4 | 32 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `DMorphAtomicLinearBuild` | 0x802B9F04 | 1360 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `DMorphAtomicCtor` | 0x802BA454 | 24 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `DMorphAtomicDtor` | 0x802BA46C | 140 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `DMorphAtomicCopy` | 0x802BA4F8 | 276 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `DMorphAtomicAlways` | 0x802BA60C | 88 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `DMorphAtomicRights` | 0x802BA664 | 8 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `_rpDMorphAtomicPluginAttach` | 0x802BA66C | 120 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `RpDMorphAtomicGetDMorphValues` | 0x802BA6E4 | 184 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `RpDMorphAtomicSetAnimation` | 0x802BA79C | 664 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `fn_802BAA34` | 0x802BAA34 | 36 | matched | placeholder |
| `rwsdk/plugin/dmorph/dmphintp` | `RpDMorphAtomicAddTime` | 0x802BAA58 | 392 | matched |  |
| `rwsdk/plugin/dmorph/dmphintp` | `RpDMorphAtomicSetAnimLoopCallBack` | 0x802BABE0 | 28 | matched |  |
| `rwsdk/plugin/dmorph/rpdmorph` | `DMorphOpen` | 0x802BABFC | 20 | matched |  |
| `rwsdk/plugin/dmorph/rpdmorph` | `DMorphClose` | 0x802BAC10 | 20 | matched |  |
| `rwsdk/plugin/dmorph/rpdmorph` | `RpDMorphPluginAttach` | 0x802BAC24 | 108 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureOpen` | 0x802BAC90 | 64 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureClose` | 0x802BACD0 | 64 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureConstructor` | 0x802BAD10 | 12 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureDestructor` | 0x802BAD1C | 168 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureCopy` | 0x802BADC4 | 368 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureStreamGetSize` | 0x802BAF34 | 160 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureStreamWrite` | 0x802BAFD4 | 324 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `MultiTextureStreamRead` | 0x802BB118 | 932 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `_rpMultiTexturePluginAttach` | 0x802BB4BC | 108 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `_rpMaterialRegisterMultiTexturePlugin` | 0x802BB528 | 204 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `RpMultiTextureGetNumTextures` | 0x802BB5F4 | 8 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `RpMultiTextureGetEffect` | 0x802BB5FC | 8 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `RpMultiTextureGetTexture` | 0x802BB604 | 16 | matched |  |
| `rwsdk/plugin/matfx/multiTex` | `RpMaterialGetMultiTexture` | 0x802BB614 | 48 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcn` | `_rpMultiTexturePlatformPluginsAttach` | 0x802C1070 | 104 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `GameCubeMTEffectStreamGetSize` | 0x802C10D8 | 64 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `GameCubeMTEffectStreamWrite` | 0x802C1118 | 1140 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `GameCubeMTEffectStreamRead` | 0x802C158C | 1116 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `_rpGameCubeMTDataPluginAttach` | 0x802C19E8 | 36 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `RpGameCubeMTEffectCreate` | 0x802C1A0C | 368 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `RpGameCubeMTEffectGetConfig` | 0x802C1B7C | 8 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnData` | `RpGameCubeTevIndUnpack` | 0x802C1B84 | 80 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `GameCubeMTOpen` | 0x802C1BD4 | 68 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `GameCubeMTClose` | 0x802C1C18 | 4 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `GameCubeMTEffectSetupTevStage` | 0x802C1C1C | 320 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `GetTexFrameMatrix` | 0x802C1D5C | 628 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `_rpGameCubeMTEffectSend` | 0x802C1FD0 | 2300 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `_rpGameCubeMTEffectClean` | 0x802C28CC | 252 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `_rpGameCubeMTMeshRenderCallBack` | 0x802C29C8 | 540 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `_rpGameCubeMTPipePluginAttach` | 0x802C2BE4 | 68 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `RpMultiTextureSetGameCubeMTCallBacks` | 0x802C2C28 | 20 | matched |  |
| `rwsdk/plugin/matfx/gcn/multiTexGcnPipe` | `RpMultiTextureGetGameCubeMTCallBacks` | 0x802C2C3C | 56 | matched |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `CalcNBTSetup` | 0x802C2C74 | 244 | parked, 32.0% (MATCH-PENDING: R17, exceptions.md) |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `TriStripNBTDataSetup8` | 0x802C2D68 | 376 | parked, 88.2% |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `TriStripNBTDataSetup16` | 0x802C2EE0 | 376 | parked, 88.2% |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `TriListNBTDataSetup8` | 0x802C3058 | 324 | parked, 88.1% |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `TriListNBTDataSetup16` | 0x802C319C | 324 | parked, 88.1% |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `CalcMeshNBTs` | 0x802C32E0 | 2404 | parked, not written |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `_rpGameCubeMTPipeDataCalcNBTs` | 0x802C3C44 | 1004 | parked, 100% |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `_rpGameCubeMTPipeDataQueryNBTs` | 0x802C4030 | 224 | parked, 100% |  |
| `rwsdk/plugin/matfx/gcn/nbtGen` | `CalcNBT` | 0x802C4110 | 4 | parked, 100% |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimFramesLinkToHierarchy` | 0x802C4114 | 148 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimOpen` | 0x802C41A8 | 236 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimClose` | 0x802C4294 | 80 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimConstructor` | 0x802C42E4 | 28 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimDestructor` | 0x802C4300 | 272 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimCopy` | 0x802C4410 | 248 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimWrite` | 0x802C4508 | 444 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimRead` | 0x802C46C4 | 588 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimSize` | 0x802C4910 | 96 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimPluginAttach` | 0x802C4970 | 172 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimHierarchyCreate` | 0x802C4A1C | 348 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimHierarchyDestroy` | 0x802C4B78 | 196 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimHierarchyCreateSubHierarchy` | 0x802C4C3C | 396 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimHierarchyAttach` | 0x802C4DC8 | 148 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimHierarchyGetMatrixArray` | 0x802C4E5C | 8 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimFrameGetHierarchy` | 0x802C4E64 | 16 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimIDGetIndex` | 0x802C4E74 | 68 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `HAnimHierarchyUpdateFramesOnly` | 0x802C4EB8 | 2076 | matched |  |
| `rwsdk/plugin/hanim/rphanim` | `RpHAnimHierarchyUpdateMatrices` | 0x802C56D4 | 1796 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameApply` | 0x802C5DD8 | 372 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameInterpolate` | 0x802C5F4C | 1308 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameBlend` | 0x802C6468 | 1292 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameStreamRead` | 0x802C6974 | 180 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameStreamWrite` | 0x802C6A28 | 160 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameStreamGetSize` | 0x802C6AC8 | 16 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameMulRecip` | 0x802C6AD8 | 448 | matched |  |
| `rwsdk/plugin/hanim/stdkey` | `RpHAnimKeyFrameAdd` | 0x802C6C98 | 332 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSWorldSectorRenderCallBack` | 0x802C6DE4 | 256 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSDivideBox` | 0x802C6EE4 | 216 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSCalculateSectorBounds` | 0x802C6FBC | 356 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSCopyAndTagSectors` | 0x802C7120 | 516 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `GetEntireSector` | 0x802C7324 | 204 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `RpPVSSetViewPosition` | 0x802C73F0 | 124 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSWorldConstructor` | 0x802C746C | 268 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSWorldDestructor` | 0x802C7578 | 192 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSSectorConstructor` | 0x802C7638 | 56 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSSectorDestructor` | 0x802C7670 | 124 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `RpPVSHook` | 0x802C76EC | 124 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `RpPVSUnhook` | 0x802C7768 | 108 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSSectorGetChunkSize` | 0x802C77D4 | 44 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSSectorWriteChunk` | 0x802C7800 | 136 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSSectorReadChunk` | 0x802C7888 | 176 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSWorldGetChunkSize` | 0x802C7938 | 36 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSWorldWriteChunk` | 0x802C795C | 100 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `PVSWorldReadChunk` | 0x802C79C0 | 240 | matched |  |
| `rwsdk/plugin/pvs/rppvs` | `RpPVSPluginAttach` | 0x802C7AB0 | 244 | matched |  |
| `rwsdk/plugin/pvs/rppvsaux` | `_rpPVSDestroyPolyList` | 0x802C7BA4 | 84 | matched |  |
| `rwsdk/plugin/pvs/rppvsaux` | `_rpPVSDestroyPoly` | 0x802C7BF8 | 84 | matched |  |
| `rwsdk/plugin/pvs/rppvsaux` | `PVSGetNumSectors` | 0x802C7C4C | 100 | matched |  |
| `rwsdk/plugin/pvs/rppvsaux` | `_rpPVSWorldGetNumWorldSectors` | 0x802C7CB0 | 108 | matched |  |
| `rwsdk/plugin/random/mertwist` | `RpRandomMT` | 0x802C7D1C | 472 | matched |  |
| `rwsdk/plugin/random/rprandom` | `RandomClose` | 0x802C7EF4 | 92 | matched |  |
| `rwsdk/plugin/random/rprandom` | `RandomOpen` | 0x802C7F50 | 616 | matched |  |
| `rwsdk/plugin/random/rprandom` | `RpRandom` | 0x802C81B8 | 180 | matched |  |
| `rwsdk/plugin/random/rprandom` | `RpRandomPluginAttach` | 0x802C826C | 68 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataOpen` | 0x802CAFF0 | 20 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataClose` | 0x802CB004 | 20 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataArrayCopy` | 0x802CB018 | 464 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataStreamRead` | 0x802CB1E8 | 660 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataStreamWrite` | 0x802CB47C | 520 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataStreamGetSize` | 0x802CB684 | 256 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataListStreamWrite` | 0x802CB784 | 272 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataObjectConstruct` | 0x802CB894 | 16 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataObjectDestruct` | 0x802CB8A4 | 276 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataObjectCopy` | 0x802CB9B8 | 376 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataObjectStreamRead` | 0x802CBB30 | 168 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataObjectStreamWrite` | 0x802CBBD8 | 56 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `UserDataObjectGetSize` | 0x802CBC10 | 160 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpGeometryGetUserDataArrayCount` | 0x802CBCB0 | 72 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpGeometryGetUserDataArray` | 0x802CBCF8 | 40 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RwFrameGetUserDataArrayCount` | 0x802CBD20 | 72 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RwFrameGetUserDataArray` | 0x802CBD68 | 40 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpMaterialGetUserDataArrayCount` | 0x802CBD90 | 72 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpMaterialGetUserDataArray` | 0x802CBDD8 | 40 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpUserDataArrayGetName` | 0x802CBE00 | 8 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpUserDataArrayGetFormat` | 0x802CBE08 | 8 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpUserDataArrayGetInt` | 0x802CBE10 | 16 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpUserDataArrayGetString` | 0x802CBE20 | 16 | matched |  |
| `rwsdk/plugin/userdata/rpusrdat` | `RpUserDataPluginAttach` | 0x802CBE30 | 692 | matched |  |
| `rwsdk/plugin/uvanim/linear` | `RpUVAnimLinearKeyFrameApply` | 0x802CC0E4 | 80 | matched |  |
| `rwsdk/plugin/uvanim/linear` | `RpUVAnimLinearKeyFrameInterpolate` | 0x802CC134 | 168 | matched |  |
| `rwsdk/plugin/uvanim/linear` | `RpUVAnimLinearKeyFrameBlend` | 0x802CC1DC | 148 | matched |  |
| `rwsdk/plugin/uvanim/linear` | `RpUVAnimLinearKeyFrameMulRecip` | 0x802CC270 | 228 | matched |  |
| `rwsdk/plugin/uvanim/linear` | `RpUVAnimLinearKeyFrameAdd` | 0x802CC354 | 164 | matched |  |
| `rwsdk/plugin/uvanim/linear` | `RpUVAnimLinearKeyFrameDataInitFromMatrix` | 0x802CC3F8 | 52 | matched |  |
| `rwsdk/plugin/uvanim/param` | `RpUVAnimParamKeyFrameApply` | 0x802CC42C | 232 | matched |  |
| `rwsdk/plugin/uvanim/param` | `RpUVAnimParamKeyFrameInterpolate` | 0x802CC514 | 216 | matched |  |
| `rwsdk/plugin/uvanim/param` | `RpUVAnimParamKeyFrameBlend` | 0x802CC5EC | 196 | matched |  |
| `rwsdk/plugin/uvanim/param` | `RpUVAnimParamKeyFrameMulRecip` | 0x802CC6B0 | 4 | matched |  |
| `rwsdk/plugin/uvanim/param` | `RpUVAnimParamKeyFrameAdd` | 0x802CC6B4 | 4 | matched |  |
| `rwsdk/plugin/uvanim/param` | `RpUVAnimParamKeyFrameDataInitFromMatrix` | 0x802CC6B8 | 352 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimOpen` | 0x802CC818 | 148 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimClose` | 0x802CC8AC | 96 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpUVAnimPluginAttach` | 0x802CC90C | 172 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimConstructor` | 0x802CC9B8 | 64 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimDestructor` | 0x802CC9F8 | 212 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimCopy` | 0x802CCACC | 320 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `_rpUVAnimCreateConsistantMatrices` | 0x802CCC0C | 288 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimWrite` | 0x802CCD2C | 360 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimRead` | 0x802CCE94 | 1068 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `UVAnimSize` | 0x802CD2C0 | 328 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `_rpUVAnimCustomDataStreamRead` | 0x802CD408 | 172 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `_rpUVAnimCustomDataStreamWrite` | 0x802CD4B4 | 120 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `_rpUVAnimCustomDataStreamGetSize` | 0x802CD52C | 8 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpUVAnimDestroy` | 0x802CD534 | 116 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpUVAnimAddRef` | 0x802CD5A8 | 20 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpMaterialUVAnimApplyUpdate` | 0x802CD5BC | 752 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpUVAnimGetName` | 0x802CD8AC | 8 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpMaterialUVAnimGetInterpolator` | 0x802CD8B4 | 24 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpMaterialUVAnimAddAnimTime` | 0x802CD8CC | 120 | matched |  |
| `rwsdk/plugin/uvanim/rpuvanim` | `RpMaterialUVAnimExists` | 0x802CD944 | 176 | matched |  |
| `rwsdk/plugin/uvanim/stdkey` | `RpUVAnimKeyFrameInit` | 0x802CD9F4 | 172 | matched |  |
| `rwsdk/plugin/uvanim/stdkey` | `RpUVAnimKeyFrameStreamRead` | 0x802CDAA0 | 220 | matched |  |
| `rwsdk/plugin/uvanim/stdkey` | `RpUVAnimKeyFrameStreamWrite` | 0x802CDB7C | 236 | matched |  |
| `rwsdk/plugin/uvanim/stdkey` | `RpUVAnimKeyFrameStreamGetSize` | 0x802CDC68 | 64 | matched |  |
| `rwsdk/world/babinwor` | `_rpReadWorldRights` | 0x802CDCA8 | 124 | matched |  |
| `rwsdk/world/babinwor` | `_rpWriteWorldRights` | 0x802CDD24 | 124 | matched |  |
| `rwsdk/world/babinwor` | `_rpSizeWorldRights` | 0x802CDDA0 | 40 | matched |  |
| `rwsdk/world/babinwor` | `_rpReadSectRights` | 0x802CDDC8 | 124 | matched |  |
| `rwsdk/world/babinwor` | `_rpWriteSectRights` | 0x802CDE44 | 124 | matched |  |
| `rwsdk/world/babinwor` | `_rpSizeSectRights` | 0x802CDEC0 | 40 | matched |  |
| `rwsdk/world/babinwor` | `WorldSectorStreamRead` | 0x802CDEE8 | 1428 | matched |  |
| `rwsdk/world/babinwor` | `PlaneSectorStreamRead` | 0x802CE47C | 1032 | matched |  |
| `rwsdk/world/babinwor` | `RpWorldStreamRead` | 0x802CE884 | 1440 | matched |  |
| `rwsdk/world/babinwor` | `_rpBinaryWorldClose` | 0x802CEE24 | 20 | matched |  |
| `rwsdk/world/babinwor` | `_rpBinaryWorldOpen` | 0x802CEE38 | 20 | matched |  |
| `rwsdk/world/baclump` | `ClumpTidyDestroyClump` | 0x802CEE4C | 272 | matched |  |
| `rwsdk/world/baclump` | `ClumpTidyDestroyAtomic` | 0x802CEF5C | 180 | matched |  |
| `rwsdk/world/baclump` | `_rpReadAtomicRights` | 0x802CF010 | 124 | matched |  |
| `rwsdk/world/baclump` | `_rpWriteAtomicRights` | 0x802CF08C | 124 | matched |  |
| `rwsdk/world/baclump` | `_rpSizeAtomicRights` | 0x802CF108 | 40 | matched |  |
| `rwsdk/world/baclump` | `AtomicSync` | 0x802CF130 | 72 | matched |  |
| `rwsdk/world/baclump` | `AtomicDefaultRenderCallBack` | 0x802CF178 | 124 | matched |  |
| `rwsdk/world/baclump` | `ClumpAtomicCloneAndAttach` | 0x802CF1F4 | 392 | matched |  |
| `rwsdk/world/baclump` | `GeometryListStreamRead` | 0x802CF37C | 716 | matched |  |
| `rwsdk/world/baclump` | `ClumpInitCameraExt` | 0x802CF648 | 28 | matched |  |
| `rwsdk/world/baclump` | `ClumpDeInitCameraExt` | 0x802CF664 | 4 | matched |  |
| `rwsdk/world/baclump` | `ClumpInitLightExt` | 0x802CF668 | 28 | matched |  |
| `rwsdk/world/baclump` | `ClumpDeInitLightExt` | 0x802CF684 | 4 | matched |  |
| `rwsdk/world/baclump` | `ClumpDestroyLight` | 0x802CF688 | 96 | matched |  |
| `rwsdk/world/baclump` | `ClumpDestroyCamera` | 0x802CF6E8 | 96 | matched |  |
| `rwsdk/world/baclump` | `ClumpCallBack` | 0x802CF748 | 4 | matched |  |
| `rwsdk/world/baclump` | `ClumpDestroyAtomic` | 0x802CF74C | 184 | matched |  |
| `rwsdk/world/baclump` | `ClumpAtomicStreamRead` | 0x802CF804 | 1408 | matched |  |
| `rwsdk/world/baclump` | `_rpAtomicResyncInterpolatedSphere` | 0x802CFD84 | 344 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicGetWorldBoundingSphere` | 0x802CFEDC | 344 | matched |  |
| `rwsdk/world/baclump` | `_rpClumpClose` | 0x802D0034 | 188 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicSetFreeListCreateParams` | 0x802D00F0 | 12 | matched |  |
| `rwsdk/world/baclump` | `_rpClumpOpen` | 0x802D00FC | 252 | matched |  |
| `rwsdk/world/baclump` | `_rpClumpRegisterExtensions` | 0x802D01F8 | 124 | matched |  |
| `rwsdk/world/baclump` | `RpClumpRender` | 0x802D0274 | 144 | matched |  |
| `rwsdk/world/baclump` | `RpClumpForAllAtomics` | 0x802D0304 | 112 | matched |  |
| `rwsdk/world/baclump` | `RpClumpForAllCameras` | 0x802D0374 | 120 | matched |  |
| `rwsdk/world/baclump` | `RpClumpForAllLights` | 0x802D03EC | 120 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicCreate` | 0x802D0464 | 320 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicDestroy` | 0x802D05A4 | 184 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicClone` | 0x802D065C | 400 | matched |  |
| `rwsdk/world/baclump` | `RpClumpClone` | 0x802D07EC | 688 | matched |  |
| `rwsdk/world/baclump` | `RpClumpCreate` | 0x802D0A9C | 192 | matched |  |
| `rwsdk/world/baclump` | `RpClumpDestroy` | 0x802D0B5C | 276 | matched |  |
| `rwsdk/world/baclump` | `RpClumpAddAtomic` | 0x802D0C70 | 40 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicStreamRead` | 0x802D0C98 | 52 | matched |  |
| `rwsdk/world/baclump` | `RpClumpStreamRead` | 0x802D0CCC | 3468 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicRegisterPlugin` | 0x802D1A58 | 48 | matched |  |
| `rwsdk/world/baclump` | `RpClumpRegisterPlugin` | 0x802D1A88 | 48 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicRegisterPluginStream` | 0x802D1AB8 | 40 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicSetStreamAlwaysCallBack` | 0x802D1AE0 | 24 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicSetStreamRightsCallBack` | 0x802D1AF8 | 24 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicGetPluginOffset` | 0x802D1B10 | 16 | matched |  |
| `rwsdk/world/baclump` | `RpAtomicSetFrame` | 0x802D1B20 | 60 | matched |  |
| `rwsdk/world/bageomet` | `_rpGeometryOpen` | 0x802D1B5C | 24 | matched |  |
| `rwsdk/world/bageomet` | `_rpGeometryClose` | 0x802D1B74 | 20 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryAddMorphTargets` | 0x802D1B88 | 676 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryTriangleSetMaterial` | 0x802D1E2C | 144 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryForAllMaterials` | 0x802D1EBC | 124 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryLock` | 0x802D1F38 | 88 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryUnlock` | 0x802D1F90 | 664 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryForAllMeshes` | 0x802D2228 | 80 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryCreate` | 0x802D2278 | 1000 | matched |  |
| `rwsdk/world/bageomet` | `_rpGeometryAddRef` | 0x802D2660 | 16 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryDestroy` | 0x802D2670 | 236 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryRegisterPlugin` | 0x802D275C | 48 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryRegisterPluginStream` | 0x802D278C | 40 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryGetPluginOffset` | 0x802D27B4 | 16 | matched |  |
| `rwsdk/world/bageomet` | `RpGeometryStreamRead` | 0x802D27C4 | 3348 | matched |  |
| `rwsdk/world/balight` | `LightTidyDestroyLight` | 0x802D34D8 | 92 | matched |  |
| `rwsdk/world/balight` | `LightSync` | 0x802D3534 | 4 | matched |  |
| `rwsdk/world/balight` | `RpLightSetRadius` | 0x802D3538 | 68 | matched |  |
| `rwsdk/world/balight` | `RpLightSetColor` | 0x802D357C | 84 | matched |  |
| `rwsdk/world/balight` | `RpLightGetConeAngle` | 0x802D35D0 | 692 | matched |  |
| `rwsdk/world/balight` | `RpLightSetConeAngle` | 0x802D3884 | 92 | matched |  |
| `rwsdk/world/balight` | `RpLightRegisterPlugin` | 0x802D38E0 | 48 | matched |  |
| `rwsdk/world/balight` | `RpLightRegisterPluginStream` | 0x802D3910 | 40 | matched |  |
| `rwsdk/world/balight` | `RpLightStreamRead` | 0x802D3938 | 440 | matched |  |
| `rwsdk/world/balight` | `RpLightDestroy` | 0x802D3AF0 | 96 | matched |  |
| `rwsdk/world/balight` | `RpLightCreate` | 0x802D3B50 | 240 | matched |  |
| `rwsdk/world/balight` | `_rpLightClose` | 0x802D3C40 | 120 | matched |  |
| `rwsdk/world/balight` | `_rpLightOpen` | 0x802D3CB8 | 144 | matched |  |
| `rwsdk/world/bamateri` | `_rpReadMaterialRights` | 0x802D3D48 | 124 | matched |  |
| `rwsdk/world/bamateri` | `_rpWriteMaterialRights` | 0x802D3DC4 | 124 | matched |  |
| `rwsdk/world/bamateri` | `_rpSizeMaterialRights` | 0x802D3E40 | 40 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialSetFreeListCreateParams` | 0x802D3E68 | 12 | matched |  |
| `rwsdk/world/bamateri` | `_rpMaterialOpen` | 0x802D3E74 | 144 | matched |  |
| `rwsdk/world/bamateri` | `_rpMaterialClose` | 0x802D3F04 | 100 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialDestroy` | 0x802D3F68 | 136 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialClone` | 0x802D3FF0 | 672 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialSetTexture` | 0x802D4290 | 96 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialRegisterPlugin` | 0x802D42F0 | 48 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialRegisterPluginStream` | 0x802D4320 | 40 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialGetPluginOffset` | 0x802D4348 | 16 | matched |  |
| `rwsdk/world/bamateri` | `RpMaterialStreamRead` | 0x802D4358 | 912 | matched |  |
| `rwsdk/world/bamatlst` | `_rpMaterialListDeinitialize` | 0x802D46E8 | 144 | matched |  |
| `rwsdk/world/bamatlst` | `_rpMaterialListInitialize` | 0x802D4778 | 20 | matched |  |
| `rwsdk/world/bamatlst` | `_rpMaterialListGetMaterial` | 0x802D478C | 16 | matched |  |
| `rwsdk/world/bamatlst` | `_rpMaterialListAppendMaterial` | 0x802D479C | 312 | matched |  |
| `rwsdk/world/bamatlst` | `_rpMaterialListFindMaterialIndex` | 0x802D48D4 | 52 | matched |  |
| `rwsdk/world/bamatlst` | `_rpMaterialListStreamRead` | 0x802D4908 | 1284 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshHeaderCreate` | 0x802D4E0C | 24 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshClose` | 0x802D4E24 | 88 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshOpen` | 0x802D4E7C | 448 | matched |  |
| `rwsdk/world/bamesh` | `_rpBuildMeshCreate` | 0x802D503C | 296 | matched |  |
| `rwsdk/world/bamesh` | `_rpBuildMeshDestroy` | 0x802D5164 | 108 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshDestroy` | 0x802D51D0 | 108 | matched |  |
| `rwsdk/world/bamesh` | `_rpBuildMeshAddTriangle` | 0x802D523C | 412 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshHeaderForAllMeshes` | 0x802D53D8 | 128 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshWrite` | 0x802D5458 | 536 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshRead` | 0x802D5670 | 652 | matched |  |
| `rwsdk/world/bamesh` | `_rpMeshSize` | 0x802D58FC | 116 | matched |  |
| `rwsdk/world/bameshop` | `SortPolygons` | 0x802D5970 | 384 | matched |  |
| `rwsdk/world/bameshop` | `SortPolygonsInTriListMesh` | 0x802D5AF0 | 768 | matched |  |
| `rwsdk/world/bameshop` | `TriStripAddEdge` | 0x802D5DF0 | 228 | matched |  |
| `rwsdk/world/bameshop` | `TriStripBinEntryArrayDestroy` | 0x802D5ED4 | 204 | matched |  |
| `rwsdk/world/bameshop` | `TriStripBinEntryArrayCreate` | 0x802D5FA0 | 348 | matched |  |
| `rwsdk/world/bameshop` | `TriStripGetTriEdge` | 0x802D60FC | 172 | matched |  |
| `rwsdk/world/bameshop` | `TriStripMarkTriUsed` | 0x802D61A8 | 400 | matched |  |
| `rwsdk/world/bameshop` | `TriStripFollow` | 0x802D6338 | 1124 | matched |  |
| `rwsdk/world/bameshop` | `TriStripStripTris` | 0x802D679C | 3016 | matched |  |
| `rwsdk/world/bameshop` | `TriStripJoin` | 0x802D7364 | 988 | matched |  |
| `rwsdk/world/bameshop` | `TriStripMeshGenerate` | 0x802D7740 | 1588 | matched |  |
| `rwsdk/world/bameshop` | `RpBuildMeshGenerateDefaultTriStrip` | 0x802D7D74 | 12 | matched |  |
| `rwsdk/world/bameshop` | `_rpTriListMeshGenerate` | 0x802D7D80 | 664 | matched |  |
| `rwsdk/world/bameshop` | `_rpMeshOptimise` | 0x802D8018 | 168 | matched |  |
| `rwsdk/world/basector` | `_rpSectorOpen` | 0x802D80C0 | 20 | matched |  |
| `rwsdk/world/basector` | `_rpSectorClose` | 0x802D80D4 | 20 | matched |  |
| `rwsdk/world/basector` | `RpWorldSectorRender` | 0x802D80E8 | 20 | matched |  |
| `rwsdk/world/basector` | `RpWorldSectorRegisterPlugin` | 0x802D80FC | 48 | matched |  |
| `rwsdk/world/basector` | `RpWorldSectorRegisterPluginStream` | 0x802D812C | 40 | matched |  |
| `rwsdk/world/baworld` | `WorldBuildMeshAtomicSector` | 0x802D8154 | 636 | matched |  |
| `rwsdk/world/baworld` | `WorldSectorDeinstanceAll` | 0x802D83D0 | 236 | matched |  |
| `rwsdk/world/baworld` | `WorldSectorDestroyRecurse` | 0x802D84BC | 524 | matched |  |
| `rwsdk/world/baworld` | `WorldClose` | 0x802D86C8 | 104 | matched |  |
| `rwsdk/world/baworld` | `WorldOpen` | 0x802D8730 | 188 | matched |  |
| `rwsdk/world/baworld` | `_rpSectorDefaultRenderCallBack` | 0x802D87EC | 140 | matched |  |
| `rwsdk/world/baworld` | `_rpWorldSetupSectorBoundingBoxes` | 0x802D8878 | 352 | matched |  |
| `rwsdk/world/baworld` | `_rpWorldRegisterWorld` | 0x802D89D8 | 180 | matched |  |
| `rwsdk/world/baworld` | `RpWorldLock` | 0x802D8A8C | 152 | matched |  |
| `rwsdk/world/baworld` | `RpWorldUnlock` | 0x802D8B24 | 220 | matched |  |
| `rwsdk/world/baworld` | `RpWorldSectorGetWorld` | 0x802D8C00 | 232 | matched |  |
| `rwsdk/world/baworld` | `RpWorldDestroy` | 0x802D8CE8 | 280 | matched |  |
| `rwsdk/world/baworld` | `RpWorldSetSectorRenderCallBack` | 0x802D8E00 | 24 | matched |  |
| `rwsdk/world/baworld` | `RpWorldGetSectorRenderCallBack` | 0x802D8E18 | 8 | matched |  |
| `rwsdk/world/baworld` | `RpWorldCreate` | 0x802D8E20 | 1032 | matched |  |
| `rwsdk/world/baworld` | `RpWorldForAllClumps` | 0x802D9228 | 120 | matched |  |
| `rwsdk/world/baworld` | `RpWorldForAllMaterials` | 0x802D92A0 | 124 | matched |  |
| `rwsdk/world/baworld` | `RpWorldForAllWorldSectors` | 0x802D931C | 156 | matched |  |
| `rwsdk/world/baworld` | `RpWorldRegisterPlugin` | 0x802D93B8 | 48 | matched |  |
| `rwsdk/world/baworld` | `RpWorldRegisterPluginStream` | 0x802D93E8 | 40 | matched |  |
| `rwsdk/world/baworld` | `RpWorldPluginAttach` | 0x802D9410 | 360 | matched |  |
| `rwsdk/world/baworobj` | `RpLightTieSetFreeListCreateParams` | 0x802D9578 | 12 | matched |  |
| `rwsdk/world/baworobj` | `WorldObjectOpen` | 0x802D9584 | 280 | matched |  |
| `rwsdk/world/baworobj` | `WorldObjectClose` | 0x802D969C | 148 | matched |  |
| `rwsdk/world/baworobj` | `WorldCameraBeginUpdate` | 0x802D9730 | 48 | matched |  |
| `rwsdk/world/baworobj` | `WorldCameraEndUpdate` | 0x802D9760 | 32 | matched |  |
| `rwsdk/world/baworobj` | `WorldCameraSync` | 0x802D9780 | 164 | matched |  |
| `rwsdk/world/baworobj` | `WorldInitCameraExt` | 0x802D9824 | 92 | matched |  |
| `rwsdk/world/baworobj` | `WorldCopyCameraExt` | 0x802D9880 | 132 | matched |  |
| `rwsdk/world/baworobj` | `WorldDeInitCameraExt` | 0x802D9904 | 128 | matched |  |
| `rwsdk/world/baworobj` | `_rpLightTieDestroy` | 0x802D9984 | 112 | matched |  |
| `rwsdk/world/baworobj` | `_rpTieDestroy` | 0x802D99F4 | 132 | matched |  |
| `rwsdk/world/baworobj` | `WorldAttachAtomicSphere` | 0x802D9A78 | 424 | matched |  |
| `rwsdk/world/baworobj` | `WorldAtomicSync` | 0x802D9C20 | 252 | matched |  |
| `rwsdk/world/baworobj` | `WorldInitAtomicExt` | 0x802D9D1C | 56 | matched |  |
| `rwsdk/world/baworobj` | `WorldCopyAtomicExt` | 0x802D9D54 | 4 | matched |  |
| `rwsdk/world/baworobj` | `WorldDeInitAtomicExt` | 0x802D9D58 | 208 | matched |  |
| `rwsdk/world/baworobj` | `WorldInitClumpExt` | 0x802D9E28 | 40 | matched |  |
| `rwsdk/world/baworobj` | `WorldCopyClumpExt` | 0x802D9E50 | 244 | matched |  |
| `rwsdk/world/baworobj` | `WorldDeInitClumpExt` | 0x802D9F44 | 4 | matched |  |
| `rwsdk/world/baworobj` | `WorldLightSync` | 0x802D9F48 | 660 | matched |  |
| `rwsdk/world/baworobj` | `WorldInitLightExt` | 0x802DA1DC | 36 | matched |  |
| `rwsdk/world/baworobj` | `WorldCopyLightExt` | 0x802DA200 | 168 | matched |  |
| `rwsdk/world/baworobj` | `WorldDeInitLightExt` | 0x802DA2A8 | 164 | matched |  |
| `rwsdk/world/baworobj` | `WorldSyncCamera` | 0x802DA34C | 908 | matched |  |
| `rwsdk/world/baworobj` | `WorldAddClumpAtomic` | 0x802DA6D8 | 92 | matched |  |
| `rwsdk/world/baworobj` | `WorldRemoveClumpAtomic` | 0x802DA734 | 240 | matched |  |
| `rwsdk/world/baworobj` | `WorldAddClumpLight` | 0x802DA824 | 160 | matched |  |
| `rwsdk/world/baworobj` | `WorldRemoveClumpLight` | 0x802DA8C4 | 220 | matched |  |
| `rwsdk/world/baworobj` | `WorldAddClumpCamera` | 0x802DA9A0 | 96 | matched |  |
| `rwsdk/world/baworobj` | `WorldRemoveClumpCamera` | 0x802DAA00 | 48 | matched |  |
| `rwsdk/world/baworobj` | `writeGeometryMesh` | 0x802DAA30 | 24 | matched |  |
| `rwsdk/world/baworobj` | `readGeometryMesh` | 0x802DAA48 | 88 | matched |  |
| `rwsdk/world/baworobj` | `sizeGeometryMesh` | 0x802DAAA0 | 12 | matched |  |
| `rwsdk/world/baworobj` | `writeGeometryNative` | 0x802DAAAC | 8 | matched |  |
| `rwsdk/world/baworobj` | `readGeometryNative` | 0x802DAAB4 | 68 | matched |  |
| `rwsdk/world/baworobj` | `sizeGeometryNative` | 0x802DAAF8 | 4 | matched |  |
| `rwsdk/world/baworobj` | `writeWorldSectorNative` | 0x802DAAFC | 8 | matched |  |
| `rwsdk/world/baworobj` | `readWorldSectorNative` | 0x802DAB04 | 68 | matched |  |
| `rwsdk/world/baworobj` | `sizeWorldSectorNative` | 0x802DAB48 | 4 | matched |  |
| `rwsdk/world/baworobj` | `writeSectorMesh` | 0x802DAB4C | 80 | matched |  |
| `rwsdk/world/baworobj` | `readSectorMesh` | 0x802DAB9C | 100 | matched |  |
| `rwsdk/world/baworobj` | `sizeSectorMesh` | 0x802DAC00 | 56 | matched |  |
| `rwsdk/world/baworobj` | `_rpWorldObjRegisterExtensions` | 0x802DAC38 | 784 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldAddCamera` | 0x802DAF48 | 84 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldRemoveCamera` | 0x802DAF9C | 44 | matched |  |
| `rwsdk/world/baworobj` | `RwCameraGetWorld` | 0x802DAFC8 | 16 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldAddAtomic` | 0x802DAFD8 | 92 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldRemoveAtomic` | 0x802DB034 | 204 | matched |  |
| `rwsdk/world/baworobj` | `RpAtomicGetWorld` | 0x802DB100 | 12 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldAddClump` | 0x802DB10C | 240 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldRemoveClump` | 0x802DB1FC | 252 | matched |  |
| `rwsdk/world/baworobj` | `RwCameraForAllSectorsInFrustum` | 0x802DB2F8 | 120 | matched |  |
| `rwsdk/world/baworobj` | `RpClumpGetWorld` | 0x802DB370 | 12 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldAddLight` | 0x802DB37C | 160 | matched |  |
| `rwsdk/world/baworobj` | `RpWorldRemoveLight` | 0x802DB41C | 212 | matched |  |
| `rwsdk/world/baworobj` | `RpLightGetWorld` | 0x802DB4F0 | 12 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxSetReflectionOffset` | 0x802DB4FC | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxWorldRenderAtomics` | 0x802DB504 | 256 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxWorldSectorRender` | 0x802DB604 | 104 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxAtomicSetPass` | 0x802DB66C | 20 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxAtomicSetLightMask` | 0x802DB680 | 20 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxMaterialSetFlags` | 0x802DB694 | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxMaterialGetFlags` | 0x802DB69C | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxMaterialSetMask` | 0x802DB6A4 | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxMaterialGetMask` | 0x802DB6AC | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxAtomicGetPass` | 0x802DB6B4 | 12 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxAtomicGetLightMask` | 0x802DB6C0 | 12 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxLightSetFlags` | 0x802DB6CC | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxLightGetFlags` | 0x802DB6D4 | 8 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxSetPseudoHDR` | 0x802DB6DC | 12 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxAtomicRenderPass` | 0x802DB6E8 | 228 | matched |  |
| `rwsdk/world/climaxcommon` | `RwClimaxAtomicRender` | 0x802DB7CC | 104 | matched |  |
| `rwsdk/world/bapipew` | `RpWorldSetDefaultSectorPipeline` | 0x802DB834 | 60 | matched |  |
| `rwsdk/world/bapipew` | `RpAtomicSetDefaultPipeline` | 0x802DB870 | 60 | matched |  |
| `rwsdk/world/bapipew` | `_rpWorldPipelineClose` | 0x802DB8AC | 40 | matched |  |
| `rwsdk/world/bapipew` | `_rpWorldPipelineOpen` | 0x802DB8D4 | 236 | matched |  |
| `rwsdk/world/bapipew` | `_rpWorldPipeAttach` | 0x802DB9C0 | 8 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxWorldRender` | 0x802DB9C8 | 232 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxSetTexture` | 0x802DBAB0 | 16 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxRVLSetRenderCallbacks` | 0x802DBAC0 | 16 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxRVLSetVertexProcCallback` | 0x802DBAD0 | 8 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxRVLConvertMatrix` | 0x802DBAD8 | 100 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxRVLHDRPrelightApply` | 0x802DBB3C | 120 | matched |  |
| `rwsdk/world/climaxplatform` | `RwClimaxRVLHDRPrelightRemove` | 0x802DBBB4 | 120 | matched |  |
| `rwsdk/world/gclights` | `_rpGCHWLightingApplyDirectionalLight` | 0x802DBC2C | 480 | matched |  |
| `rwsdk/world/gclights` | `_rwGCLightsGlobalEnable` | 0x802DBE0C | 288 | matched |  |
| `rwsdk/world/gclights` | `_rwGCLightsLocalEnable` | 0x802DBF2C | 972 | matched |  |
| `rwsdk/world/gclights` | `_rwGCLightsListInit` | 0x802DC2F8 | 12 | matched |  |
| `rwsdk/world/gclights` | `_rwGCLightsListFlush` | 0x802DC304 | 256 | matched |  |
| `rwsdk/world/gclights` | `_rwGCLightsLocalQueue` | 0x802DC404 | 80 | matched |  |
| `rwsdk/world/gclights` | `rwDlLightExtCnst` | 0x802DC454 | 16 | matched |  |
| `rwsdk/world/gclights` | `rwDlLightExtDest` | 0x802DC464 | 4 | matched |  |
| `rwsdk/world/gclights` | `rwDlLightExtCopy` | 0x802DC468 | 4 | matched |  |
| `rwsdk/world/gclights` | `_rpDlLightPluginAttach` | 0x802DC46C | 76 | matched |  |
| `rwsdk/world/gcmorph` | `_rwDlV3dInterp` | 0x802DC670 | 4 | matched |  |
| `rwsdk/world/gcpipe` | `_rxGCResEntryWaitDone` | 0x802DC674 | 136 | matched |  |
| `rwsdk/world/native` | `_rpNativeRead` | 0x802DCAB0 | 764 | matched |  |
| `rwsdk/world/native` | `_rpNativeWrite` | 0x802DCDAC | 852 | matched |  |
| `rwsdk/world/native` | `_rpGeometryNativeSize` | 0x802DD100 | 44 | matched |  |
| `rwsdk/world/native` | `_rpGeometryNativeWrite` | 0x802DD12C | 72 | matched |  |
| `rwsdk/world/native` | `_rpGeometryNativeRead` | 0x802DD174 | 16 | matched |  |
| `rwsdk/world/native` | `_rpWorldSectorNativeSize` | 0x802DD184 | 84 | matched |  |
| `rwsdk/world/native` | `_rpWorldSectorNativeWrite` | 0x802DD1D8 | 96 | matched |  |
| `rwsdk/world/native` | `_rpWorldSectorNativeRead` | 0x802DD238 | 16 | matched |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rpDlAtomicPipelineCreate` | 0x802DD248 | 180 | parked, 100% |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rxGCAtomicDefaultLightingCallback` | 0x802DD2FC | 588 | parked, 99.0% |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rxGCDefaultReinstance` | 0x802DD548 | 632 | parked, 100% |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rxGCAtomicDefaultReinstanceCallback` | 0x802DD7C0 | 364 | parked, 100% |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rxGCAtomicDefaultInstanceCallback` | 0x802DD92C | 176 | parked, 100% |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rxGCAtomicAllInOneNode` | 0x802DD9DC | 648 | parked, 100% |  |
| `rwsdk/world/nodeGameCubeAtomicAllInOne` | `_rxGCAtomicAllInOnePipelineInit` | 0x802DDC64 | 60 | parked, 100% |  |
| `rwsdk/world/nodeGameCubeWorldSectorAllInOne` | `_rpDlSectorPipelineCreate` | 0x802DDCA0 | 180 | matched |  |
| `rwsdk/world/nodeGameCubeWorldSectorAllInOne` | `_rxGCSectorDefaultLightingCallback` | 0x802DDD54 | 340 | matched |  |
| `rwsdk/world/nodeGameCubeWorldSectorAllInOne` | `_rxGCSectorDefaultInstanceCallback` | 0x802DDEA8 | 156 | matched |  |
| `rwsdk/world/nodeGameCubeWorldSectorAllInOne` | `_rxGCWorldSectorAllInOneNode` | 0x802DDF44 | 496 | matched |  |
| `rwsdk/world/nodeGameCubeWorldSectorAllInOne` | `_rxGCWorldSectorAllInOnePipelineInit` | 0x802DE134 | 56 | matched |  |
| `rwsdk/world/setup` | `MatFuncDummy` | 0x802DE16C | 4 | matched |  |
| `rwsdk/world/setup` | `MatFunc1` | 0x802DE170 | 280 | matched |  |
| `rwsdk/world/setup` | `MatFunc2` | 0x802DE288 | 156 | matched |  |
| `rwsdk/world/setup` | `MatFunc3` | 0x802DE324 | 200 | matched |  |
| `rwsdk/world/setup` | `MatFunc4` | 0x802DE3EC | 176 | matched |  |
| `rwsdk/world/setup` | `MatFunc5` | 0x802DE49C | 48 | matched |  |
| `rwsdk/world/setup` | `MatFunc6` | 0x802DE4CC | 156 | matched |  |
| `rwsdk/world/vtxfmt` | `fn_802DEE68` | 0x802DEE68 | 628 | matched | placeholder |
| `rwsdk/world/vtxfmt` | `fn_802DF0DC` | 0x802DF0DC | 12 | matched | placeholder |
| `rwsdk/world/vtxfmt` | `_rxDlVertexFmtConst` | 0x802DF0E8 | 12 | matched |  |
| `rwsdk/world/vtxfmt` | `_rxDlVertexFmtDest` | 0x802DF0F4 | 100 | matched |  |
| `rwsdk/world/vtxfmt` | `_rpDlVtxFmtOpen` | 0x802DF158 | 80 | matched |  |
| `rwsdk/world/vtxfmt` | `_rpDlVtxFmtClose` | 0x802DF1A8 | 20 | matched |  |
| `rwsdk/world/vtxfmt` | `_rpDlVtxFmtPluginAttach` | 0x802DF1BC | 176 | matched |  |
| `rwsdk/world/vtxfmt` | `RpGameCubeVtxFmtSetNormal` | 0x802DF26C | 12 | matched |  |
| `rwsdk/world/vtxfmt` | `RpGameCubeVtxFmtInit` | 0x802DF278 | 112 | matched |  |
| `rwsdk/world/vtxfmt` | `RpGameCubeVtxFmtCreate` | 0x802DF2E8 | 76 | matched |  |
| `rwsdk/world/vtxfmt` | `RpGameCubeVtxFmtDestroy` | 0x802DF334 | 40 | matched |  |
| `rwsdk/world/vtxfmt` | `RpGameCubeGeometrySetVtxFmt` | 0x802DF35C | 132 | matched |  |
| `rwsdk/world/wrldpipe` | `_rxWorldDevicePluginAttach` | 0x802DF3E0 | 64 | matched |  |
| `rwsdk/world/wrldpipe` | `_rpCreatePlatformMaterialPipelines` | 0x802DF420 | 8 | matched |  |
| `rwsdk/world/wrldpipe` | `_rpDestroyPlatformMaterialPipelines` | 0x802DF428 | 4 | matched |  |
| `rwsdk/world/wrldpipe` | `_rpCreatePlatformWorldSectorPipelines` | 0x802DF42C | 108 | matched |  |
| `rwsdk/world/wrldpipe` | `_rpDestroyPlatformWorldSectorPipelines` | 0x802DF498 | 84 | matched |  |
| `rwsdk/world/wrldpipe` | `_rpCreatePlatformAtomicPipelines` | 0x802DF4EC | 112 | matched |  |
| `rwsdk/world/wrldpipe` | `_rpDestroyPlatformAtomicPipelines` | 0x802DF55C | 84 | matched |  |
| `rwsdk/world/geomcond` | `IndicesRemap` | 0x802DF5B0 | 116 | matched |  |
| `rwsdk/world/geomcond` | `IndexDataCreateRemapped` | 0x802DF624 | 396 | matched |  |
| `rwsdk/world/geomcond` | `VertexDataCreateRemapped` | 0x802DF7B0 | 688 | matched |  |
| `rwsdk/world/geomcond` | `VertexDataCreateMaps` | 0x802DFA60 | 1936 | matched |  |
| `rwsdk/world/ibuffer` | `_rwGCNDisplayListGetStride` | 0x802E475C | 976 | matched |  |
| `rwsdk/world/ibuffer` | `_rwGCNDisplayListGetSize` | 0x802E4B2C | 80 | matched |  |
| `rwsdk/world/ibuffer` | `_rwGCNDisplayListInitialize` | 0x802E4B7C | 20 | matched |  |
| `rwsdk/world/instancegeom` | `ReconditionVertexIndexData` | 0x802E4B90 | 1068 | matched |  |
| `rwsdk/world/instancegeom` | `VtxDescInitOptimized` | 0x802E4FBC | 528 | matched |  |
| `rwsdk/world/instancegeom` | `fn_802E51CC` | 0x802E51CC | 516 | matched | placeholder |
| `rwsdk/world/instancegeom` | `IndexDataSetupOptimized` | 0x802E53D0 | 408 | matched |  |
| `rwsdk/world/instancegeom` | `_rwDlGeometryInstanceOptimized` | 0x802E5568 | 996 | matched |  |
| `rwsdk/world/instancegeom` | `VtxDescInitFast` | 0x802E594C | 488 | matched |  |
| `rwsdk/world/instancegeom` | `VertexDataFastSetup` | 0x802E5B34 | 216 | matched |  |
| `rwsdk/world/instancegeom` | `_rwDlGeometryInstanceFast` | 0x802E5C0C | 852 | matched |  |
| `rwsdk/world/instanceworld` | `ReconditionVertexIndexData` | 0x802E5F60 | 1120 | matched |  |
| `rwsdk/world/instanceworld` | `VtxDescInitOptimized` | 0x802E63C0 | 528 | matched |  |
| `rwsdk/world/instanceworld` | `fn_802E65D0` | 0x802E65D0 | 516 | matched | placeholder |
| `rwsdk/world/instanceworld` | `IndexDataSetupOptimized` | 0x802E67D4 | 408 | matched |  |
| `rwsdk/world/instanceworld` | `_rwDlWorldSectorInstanceOptimized` | 0x802E696C | 992 | matched |  |
| `rwsdk/world/instanceworld` | `VtxDescInitFast` | 0x802E6D4C | 488 | matched |  |
| `rwsdk/world/instanceworld` | `VertexDataFastSetup` | 0x802E6F34 | 220 | matched |  |
| `rwsdk/world/instanceworld` | `_rwDlWorldSectorInstanceFast` | 0x802E7010 | 864 | matched |  |
| `rwsdk/world/itools` | `_rwGCNTriStripGetStats` | 0x802E7370 | 344 | matched |  |
| `rwsdk/world/itools` | `_rwGCNInstanceIndicesCopy` | 0x802E74C8 | 400 | matched |  |
| `rwsdk/world/itools` | `_rwGCNInstanceIndices` | 0x802E7658 | 556 | matched |  |
| `rwsdk/world/itools` | `WriteHeaders` | 0x802E7884 | 456 | matched |  |
| `rwsdk/world/itools` | `_rwGCNDisplayListFill` | 0x802E7A4C | 2188 | matched |  |
| `rwsdk/world/vbuffer` | `_rwGCNVertexBufferHeaderGetSize` | 0x802E82D8 | 20 | matched |  |
| `rwsdk/world/vbuffer` | `_rwGCNVertexBufferGetSize` | 0x802E82EC | 820 | matched |  |
| `rwsdk/world/vbuffer` | `_rwGCNVertexBufferInitialize` | 0x802E8620 | 1264 | matched |  |
| `rwsdk/world/vtxdesc` | `_rwVertexDescriptorInit` | 0x802E8FC4 | 44 | matched |  |
| `rwsdk/world/vtxdesc` | `_rwGCNVertexDescSetVAT` | 0x802E8FF0 | 8 | matched |  |
| `rwsdk/world/vtxdesc` | `_rwGCNVertexDescSetElementAttr` | 0x802E8FF8 | 552 | matched |  |
| `rwsdk/world/vtxdesc` | `_rwGCNVertexDescSetElementDesc` | 0x802E9220 | 1040 | matched |  |
| `rwsdk/world/vtxdesc` | `_rwGCNVertexDescSetNumIndexedAttr` | 0x802E9630 | 8 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinGetMeshBoneRemapIndices` | 0x802E9638 | 8 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinGetMeshBoneRLECount` | 0x802E9640 | 8 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinGetMeshBoneRLE` | 0x802E9648 | 8 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinSplitDataCreate` | 0x802E9650 | 232 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinSplitDataDestroy` | 0x802E9738 | 104 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinSplitDataStreamWrite` | 0x802E97A0 | 228 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinSplitDataStreamRead` | 0x802E9884 | 432 | matched |  |
| `rwsdk/plugin/skin2/bsplit` | `_rpSkinSplitDataStreamGetSize` | 0x802E9A34 | 52 | matched |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinFindNumUsedBones` | 0x802E9A68 | 204 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinCreateSkinData` | 0x802E9B34 | 460 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinOpen` | 0x802E9D00 | 280 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinClose` | 0x802E9E18 | 128 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinGeometryConstructor` | 0x802E9E98 | 24 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinGeometryDestructor` | 0x802E9EB0 | 168 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinGeometryCopy` | 0x802E9F58 | 4 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicConstructor` | 0x802E9F5C | 72 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicDestructor` | 0x802E9FA4 | 88 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicCopy` | 0x802E9FFC | 24 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicAlways` | 0x802EA014 | 240 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicRights` | 0x802EA104 | 188 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinGeometrySize` | 0x802EA1C0 | 144 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinGeometryWrite` | 0x802EA250 | 340 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinGeometryRead` | 0x802EA3A4 | 680 | parked, 99.7% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicRead` | 0x802EA64C | 700 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicWrite` | 0x802EA908 | 4 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `SkinAtomicGetSize` | 0x802EA90C | 8 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinPluginAttach` | 0x802EA914 | 256 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinAtomicSetHAnimHierarchy` | 0x802EAA14 | 20 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinAtomicGetHAnimHierarchy` | 0x802EAA28 | 20 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinGeometryGetSkin` | 0x802EAA3C | 20 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinGeometrySetSkin` | 0x802EAA50 | 140 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinGetNumBones` | 0x802EAADC | 8 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinGetVertexBoneWeights` | 0x802EAAE4 | 8 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinGetVertexBoneIndices` | 0x802EAAEC | 8 | parked, 100% |  |
| `rwsdk/plugin/skin2/rpskin` | `RpSkinGetSkinToBoneMatrices` | 0x802EAAF4 | 8 | parked, 100% |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_FlagUsed` | 0x802EAAFC | 140 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_UnRegisterSkinRes` | 0x802EAB88 | 220 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_RepEntryDestroyNotify` | 0x802EAC64 | 48 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_OrphanSkinRes` | 0x802EAC94 | 44 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_GetUsableResource` | 0x802EACC0 | 728 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_CheckResource` | 0x802EAF98 | 372 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_TokenStampResource` | 0x802EB10C | 76 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_ReAquireResource` | 0x802EB158 | 244 | matched |  |
| `rwsdk/plugin/skin2/rescache` | `_clSkinCache_AtomicDestruct` | 0x802EB24C | 44 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinMainResEntryCB` | 0x802EB278 | 272 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinResEntryWaitDone` | 0x802EB388 | 144 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinVertexBuffersUpdate` | 0x802EB418 | 1080 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinMatrixBlendUpdate` | 0x802EB850 | 496 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinBlendBodyP` | 0x802EBA40 | 388 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinBlendBodyPN` | 0x802EBBC4 | 552 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinInstanceCallback` | 0x802EBDEC | 280 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinAtomicReinstanceCallBack` | 0x802EBF04 | 996 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinLoadMatrix` | 0x802EC2E8 | 208 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinLoadMatrixPalette` | 0x802EC3B8 | 180 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinRenderCallback` | 0x802EC46C | 864 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinCreate` | 0x802EC7CC | 680 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinInitialize` | 0x802ECA74 | 160 | matched |  |
| `rwsdk/plugin/skin2/skingcn` | `_rpSkinDeinitialize` | 0x802ECB14 | 120 | matched |  |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate2WeightsP` | 0x802ECB8C | 280 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate2WeightsPN` | 0x802ECCA4 | 468 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate3WeightsP` | 0x802ECE78 | 368 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate3WeightsPN` | 0x802ECFE8 | 596 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate4WeightsP` | 0x802ED23C | 432 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skingcnasm` | `_rwDlSkinUpdate4WeightsPN` | 0x802ED3EC | 676 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skinmatrixblend` | `_rpSkinMatrixBlendUpdateASM` | 0x802EDBCC | 508 | ORIGINAL-ASM |  |
| `rwsdk/plugin/skin2/skinstream` | `_rpSkinGeometryNativeSize` | 0x802EDDC8 | 200 | matched |  |
| `rwsdk/plugin/skin2/skinstream` | `_rpSkinGeometryNativeWrite` | 0x802EDE90 | 420 | matched |  |
| `rwsdk/plugin/skin2/skinstream` | `_rpSkinGeometryNativeRead` | 0x802EE034 | 788 | matched |  |
| `rwsdk/tool/skinsplit/rtskinsp` | `_rtSkinSplitMeshBoneRLEValidate` | 0x802F0160 | 8 | matched |  |
| `rwsdk/tool/skinsplit/rtskinsp` | `_rtSkinSplitMeshSetBoneRLE` | 0x802F0168 | 296 | matched |  |
| `rwsdk/tool/skinsplit/rtskinsp` | `_rtSkinSplitCreateGeometryMesh` | 0x802F0290 | 420 | matched |  |
| `rwsdk/tool/skinsplit/rtskinsp` | `RtSkinSplitAtomicSplitGeometry` | 0x802F0434 | 992 | matched |  |
| `rwsdk/tool/slerp/rtslerp` | `RtSlerpCreate` | 0x802F1F44 | 204 | matched |  |
| `rwsdk/tool/slerp/rtslerp` | `RtSlerpInitialize` | 0x802F2010 | 492 | matched |  |
| `rwsdk/tool/slerp/rtslerp` | `RtSlerpDestroy` | 0x802F21FC | 100 | matched |  |
| `rwsdk/tool/slerp/rtslerp` | `RtSlerpGetMatrix` | 0x802F2260 | 1248 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RtFSManagerOpen` | 0x802F4310 | 48 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `_rtFSManagerIsNewFSRegistrable` | 0x802F4340 | 100 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RtFSManagerRegister` | 0x802F43A4 | 296 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RtFSManagerGetFileSystemFromName` | 0x802F44CC | 148 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `_rtFSManagerIsFSNameInUse` | 0x802F4560 | 196 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `_rtFSManagerFOpenOnFS` | 0x802F4624 | 380 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `_rtGetFileSystemFromDeviceName` | 0x802F47A0 | 116 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RtFSManagerSetDefaultFileSystem` | 0x802F4814 | 8 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFopen` | 0x802F481C | 504 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFclose` | 0x802F4A14 | 88 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFread` | 0x802F4A6C | 72 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFwrite` | 0x802F4AB4 | 72 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFseek` | 0x802F4AFC | 160 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFeof` | 0x802F4B9C | 16 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFexist` | 0x802F4BAC | 312 | matched |  |
| `rwsdk/tool/fsyst/rtfsmgr` | `RwFtell` | 0x802F4CE4 | 8 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `_rtFSInit` | 0x802F4CEC | 368 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `_rtGetFileNameFromDeviceName` | 0x802F4E5C | 132 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `_rtFSBuildFileName` | 0x802F4EE0 | 200 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `_rtFSFileExists` | 0x802F4FA8 | 88 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `_rtFSIsEOF` | 0x802F5000 | 76 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `_rtFileSystemTriggerCallBack` | 0x802F504C | 72 | matched |  |
| `rwsdk/tool/fsyst/rtfsyst` | `rtFSTriggerFileOutstandingOpCallback` | 0x802F5094 | 104 | matched |  |
| `rwsdk/tool/anim/rtanim` | `AnimOpen` | 0x802F5F10 | 96 | matched |  |
| `rwsdk/tool/anim/rtanim` | `AnimClose` | 0x802F5F70 | 84 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInitialize` | 0x802F5FC4 | 68 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimRegisterInterpolationScheme` | 0x802F6008 | 300 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimGetInterpolatorInfo` | 0x802F6134 | 136 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimAnimationCreate` | 0x802F61BC | 308 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimAnimationDestroy` | 0x802F62F0 | 48 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimAnimationStreamRead` | 0x802F6320 | 664 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimAnimationStreamWrite` | 0x802F65B8 | 292 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimAnimationStreamGetSize` | 0x802F66DC | 48 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimAnimationGetNumNodes` | 0x802F670C | 52 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorCreate` | 0x802F6740 | 176 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorDestroy` | 0x802F67F0 | 16 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorSetCurrentAnim` | 0x802F6800 | 308 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorSetKeyFrameCallBacks` | 0x802F6934 | 200 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorSetAnimLoopCallBack` | 0x802F69FC | 12 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorCopy` | 0x802F6A08 | 76 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorSubAnimTime` | 0x802F6A54 | 436 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorAddAnimTime` | 0x802F6C08 | 568 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorSetCurrentTime` | 0x802F6E40 | 68 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorBlend` | 0x802F6E84 | 196 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorAddTogether` | 0x802F6F48 | 160 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorCreateSubInterpolator` | 0x802F6FE8 | 92 | matched |  |
| `rwsdk/tool/anim/rtanim` | `RtAnimInterpolatorBlendSubInterpolator` | 0x802F7044 | 580 | matched |  |
| `rwsdk/tool/quat/rtquat` | `QuatFromXDiagDomMatrix` | 0x802F7288 | 168 | matched |  |
| `rwsdk/tool/quat/rtquat` | `QuatFromYDiagDomMatrix` | 0x802F7330 | 168 | matched |  |
| `rwsdk/tool/quat/rtquat` | `QuatFromZDiagDomMatrix` | 0x802F73D8 | 168 | matched |  |
| `rwsdk/tool/quat/rtquat` | `RtQuatConvertFromMatrix` | 0x802F7480 | 308 | matched |  |
| `rwsdk/tool/quat/rtquat` | `RtQuatRotate` | 0x802F75B4 | 992 | matched |  |
| `rwsdk/tool/quat/rtquat` | `RtQuatTransformVectors` | 0x802F7994 | 424 | matched |  |
| `rwsdk/tool/dict/rtdict` | `RtDictSchemaInit` | 0x802F7B3C | 88 | matched | unit name unconfirmed |
| `rwsdk/tool/dict/rtdict` | `RtDictSchemaDestruct` | 0x802F7B94 | 40 | matched | unit name unconfirmed |
| `rwsdk/tool/dict/rtdict` | `RtDictSchemaGetCurrentDict` | 0x802F7BBC | 8 | matched | unit name unconfirmed |
| `rwsdk/tool/dict/rtdict` | `RtDictSchemaSetCurrentDict` | 0x802F7BC4 | 8 | matched | unit name unconfirmed |
| `rwsdk/tool/dict/rtdict` | `RtDictFindNamedEntry` | 0x802F7BCC | 152 | matched | unit name unconfirmed |
| `rwsdk/tool/dict/rtdict` | `RtDictDestroy` | 0x802F7C64 | 272 | matched | unit name unconfirmed |
| `rwsdk/tool/dict/rtdict` | `RtDictSchemaStreamReadDict` | 0x802F7D74 | 792 | matched | unit name unconfirmed |
| `fmod/fmod` | `FMOD_Memory_Initialize` | 0x8032BA54 | 424 | matched |  |
| `fmod/fmod` | `FMOD_System_Create` | 0x8032BBFC | 352 | matched |  |
| `fmod/fmod` | `FMOD_Sound_GetSyncPoint` | 0x8032BD5C | 20 | matched |  |
| `fmod/fmod` | `FMOD_Sound_GetSyncPointInfo` | 0x8032BD70 | 20 | matched |  |
| `fmod/fmod` | `FMOD_Channel_GetCurrentSound` | 0x8032BD84 | 20 | matched |  |
| `fmod/fmod` | `FMOD_Channel_GetUserData` | 0x8032BD98 | 20 | matched |  |
| `fmod/fmod_async` | `asyncThreadFunc__4FMODFPv` | 0x8032BDAC | 4 | matched |  |
| `fmod/fmod_async` | `__dt__Q24FMOD6ThreadFv` | 0x8032BDB0 | 64 | matched |  |
| `fmod/fmod_async` | `shutDown__Q24FMOD11AsyncThreadFv` | 0x8032BDF0 | 116 | matched |  |
| `fmod/fmod_async` | `addCallback__Q24FMOD11AsyncThreadFPFv_11FMOD_RESULTPPQ24FMOD11AsyncThread` | 0x8032BE64 | 280 | matched |  |
| `fmod/fmod_async` | `removeCallback__Q24FMOD11AsyncThreadFPFv_11FMOD_RESULT` | 0x8032BF7C | 216 | matched |  |
| `fmod/fmod_async` | `reallyRelease__Q24FMOD11AsyncThreadFv` | 0x8032C054 | 256 | matched |  |
| `fmod/fmod_async` | `wakeupThread__Q24FMOD11AsyncThreadFv` | 0x8032C154 | 12 | matched |  |
| `fmod/fmod_async` | `update__Q24FMOD11AsyncThreadFv` | 0x8032C160 | 128 | matched |  |
| `fmod/fmod_async` | `threadFunc__Q24FMOD11AsyncThreadFv` | 0x8032C1E0 | 700 | matched |  |
| `fmod/fmod_async` | `getAsyncThread__Q24FMOD11AsyncThreadFPQ24FMOD6SoundI` | 0x8032C49C | 504 | matched |  |
| `fmod/fmod_async` | `__sinit_\fmod_async_cpp` | 0x8032C694 | 28 | matched | placeholder (compiler-generated name) |
| `fmod/fmod_channel_emulated` | `__ct__Q24FMOD15ChannelEmulatedFv` | 0x8032C6B0 | 88 | matched |  |
| `fmod/fmod_channel_emulated` | `__dt__Q24FMOD11ChannelRealFv` | 0x8032C708 | 64 | matched |  |
| `fmod/fmod_channel_emulated` | `init__Q24FMOD15ChannelEmulatedFiPQ24FMOD7SystemIPQ24FMOD6OutputPQ24FMOD4DSPI` | 0x8032C748 | 200 | matched |  |
| `fmod/fmod_channel_emulated` | `alloc__Q24FMOD15ChannelEmulatedFv` | 0x8032C810 | 140 | matched |  |
| `fmod/fmod_channel_emulated` | `stop__Q24FMOD15ChannelEmulatedFv` | 0x8032C89C | 216 | matched |  |
| `fmod/fmod_channel_emulated` | `isStream__Q24FMOD6SoundIFv` | 0x8032C974 | 8 | matched |  |
| `fmod/fmod_channel_emulated` | `close__Q24FMOD15ChannelEmulatedFv` | 0x8032C97C | 100 | matched |  |
| `fmod/fmod_channel_emulated` | `update__Q24FMOD15ChannelEmulatedFi` | 0x8032C9E0 | 672 | matched |  |
| `fmod/fmod_channel_emulated` | `isVirtual__Q24FMOD15ChannelEmulatedFPb` | 0x8032CC80 | 32 | matched |  |
| `fmod/fmod_channel_emulated` | `getDSPHead__Q24FMOD15ChannelEmulatedFPPQ24FMOD4DSPI` | 0x8032CCA0 | 32 | matched |  |
| `fmod/fmod_channel_emulated` | `setSpeakerLevels__Q24FMOD15ChannelEmulatedFiPfi` | 0x8032CCC0 | 8 | matched |  |
| `fmod/fmod_channel_emulated` | `setPositionEx__Q24FMOD11ChannelRealFUiUib` | 0x8032CCC8 | 16 | matched |  |
| `fmod/fmod_channel_emulated` | `moveChannelGroup__Q24FMOD11ChannelRealFPQ24FMOD13ChannelGroupIPQ24FMOD13ChannelGroupIb` | 0x8032CCD8 | 8 | matched |  |
| `fmod/fmod_channel_emulated` | `isStream__Q24FMOD11ChannelRealFv` | 0x8032CCE0 | 8 | matched |  |
| `fmod/fmod_channel_emulated` | `__dt__Q24FMOD15ChannelEmulatedFv` | 0x8032CCE8 | 64 | matched |  |
| `fmod/fmod_channel_real` | `__ct__Q24FMOD11ChannelRealFv` | 0x8032CD28 | 60 | matched |  |
| `fmod/fmod_channel_real` | `init__Q24FMOD11ChannelRealFiPQ24FMOD7SystemIPQ24FMOD6OutputPQ24FMOD4DSPI` | 0x8032CD64 | 84 | matched |  |
| `fmod/fmod_channel_real` | `close__Q24FMOD11ChannelRealFv` | 0x8032CDB8 | 16 | matched |  |
| `fmod/fmod_channel_real` | `alloc__Q24FMOD11ChannelRealFv` | 0x8032CDC8 | 40 | matched |  |
| `fmod/fmod_channel_real` | `alloc__Q24FMOD11ChannelRealFPQ24FMOD4DSPI` | 0x8032CDF0 | 40 | matched |  |
| `fmod/fmod_channel_real` | `set2DFreqVolumePanFor3D__Q24FMOD11ChannelRealFv` | 0x8032CE18 | 8 | matched |  |
| `fmod/fmod_channel_real` | `update__Q24FMOD11ChannelRealFi` | 0x8032CE20 | 8 | matched |  |
| `fmod/fmod_channel_real` | `updateStream__Q24FMOD11ChannelRealFv` | 0x8032CE28 | 8 | matched |  |
| `fmod/fmod_channel_real` | `start__Q24FMOD11ChannelRealFv` | 0x8032CE30 | 8 | matched |  |
| `fmod/fmod_channel_real` | `stop__Q24FMOD11ChannelRealFv` | 0x8032CE38 | 88 | matched |  |
| `fmod/fmod_channel_real` | `setPaused__Q24FMOD11ChannelRealFb` | 0x8032CE90 | 44 | matched |  |
| `fmod/fmod_channel_real` | `getPaused__Q24FMOD11ChannelRealFPb` | 0x8032CEBC | 36 | matched |  |
| `fmod/fmod_channel_real` | `setVolume__Q24FMOD11ChannelRealFf` | 0x8032CEE0 | 8 | matched |  |
| `fmod/fmod_channel_real` | `setFrequency__Q24FMOD11ChannelRealFf` | 0x8032CEE8 | 8 | matched |  |
| `fmod/fmod_channel_real` | `setPan__Q24FMOD11ChannelRealFff` | 0x8032CEF0 | 8 | matched |  |
| `fmod/fmod_channel_real` | `setDSPClockDelay__Q24FMOD11ChannelRealFv` | 0x8032CEF8 | 8 | matched |  |
| `fmod/fmod_channel_real` | `setSpeakerMix__Q24FMOD11ChannelRealFffffffff` | 0x8032CF00 | 1304 | matched |  |
| `fmod/fmod_channel_real` | `setSpeakerLevels__Q24FMOD11ChannelRealFiPfi` | 0x8032D418 | 276 | matched |  |
| `fmod/fmod_channel_real` | `updateSpeakerLevels__Q24FMOD11ChannelRealFf` | 0x8032D52C | 588 | matched |  |
| `fmod/fmod_channel_real` | `setPosition__Q24FMOD11ChannelRealFUiUi` | 0x8032D778 | 568 | matched |  |
| `fmod/fmod_channel_real` | `getPosition__Q24FMOD11ChannelRealFPUiUi` | 0x8032D9B0 | 872 | matched |  |
| `fmod/fmod_channel_real` | `setLoopPoints__Q24FMOD11ChannelRealFUiUi` | 0x8032DD18 | 76 | matched |  |
| `fmod/fmod_channel_real` | `setLoopCount__Q24FMOD11ChannelRealFi` | 0x8032DD64 | 12 | matched |  |
| `fmod/fmod_channel_real` | `setLowPassGain__Q24FMOD11ChannelRealFf` | 0x8032DD70 | 8 | matched |  |
| `fmod/fmod_channel_real` | `set3DAttributes__Q24FMOD11ChannelRealFv` | 0x8032DD78 | 8 | matched |  |
| `fmod/fmod_channel_real` | `set3DMinMaxDistance__Q24FMOD11ChannelRealFv` | 0x8032DD80 | 8 | matched |  |
| `fmod/fmod_channel_real` | `set3DOcclusion__Q24FMOD11ChannelRealFff` | 0x8032DD88 | 40 | matched |  |
| `fmod/fmod_channel_real` | `setReverbProperties__Q24FMOD11ChannelRealFPC29FMOD_REVERB_CHANNELPROPERTIES` | 0x8032DDB0 | 360 | matched |  |
| `fmod/fmod_channel_real` | `getReverbProperties__Q24FMOD11ChannelRealFP29FMOD_REVERB_CHANNELPROPERTIES` | 0x8032DF18 | 116 | matched |  |
| `fmod/fmod_channel_real` | `isPlaying__Q24FMOD11ChannelRealFPbb` | 0x8032DF8C | 64 | matched |  |
| `fmod/fmod_channel_real` | `isVirtual__Q24FMOD11ChannelRealFPb` | 0x8032DFCC | 32 | matched |  |
| `fmod/fmod_channel_real` | `getSpectrum__Q24FMOD11ChannelRealFPfii19FMOD_DSP_FFT_WINDOW` | 0x8032DFEC | 8 | matched |  |
| `fmod/fmod_channel_real` | `getWaveData__Q24FMOD11ChannelRealFPfii` | 0x8032DFF4 | 8 | matched |  |
| `fmod/fmod_channel_real` | `getDSPHead__Q24FMOD11ChannelRealFPPQ24FMOD4DSPI` | 0x8032DFFC | 16 | matched |  |
| `fmod/fmod_channel_real` | `setMode__Q24FMOD11ChannelRealFUi` | 0x8032E00C | 412 | matched |  |
| `fmod/fmod_channel_real` | `setLowPassFilter__Q24FMOD11ChannelRealFi` | 0x8032E1A8 | 12 | matched |  |
| `fmod/fmod_channel_real` | `getLowPassFilter__Q24FMOD11ChannelRealFPi` | 0x8032E1B4 | 16 | matched |  |
| `fmod/fmod_channel_real` | `setBiquadFilter__Q24FMOD11ChannelRealFbUsUsUsUsUs` | 0x8032E1C4 | 32 | matched |  |
| `fmod/fmod_channel_real` | `getBiquadFilter__Q24FMOD11ChannelRealFPbPUsPUsPUsPUsPUs` | 0x8032E1E4 | 56 | matched |  |
| `fmod/fmod_channel_real` | `setControllerSpeaker__Q24FMOD11ChannelRealFUii` | 0x8032E21C | 12 | matched |  |
| `fmod/fmod_channel_real` | `getControllerSpeaker__Q24FMOD11ChannelRealFPUi` | 0x8032E228 | 16 | matched |  |
| `fmod/fmod_channelgroup` | `release__Q24FMOD12ChannelGroupFv` | 0x80334D18 | 56 | matched |  |
| `fmod/fmod_channelgroup` | `setVolume__Q24FMOD12ChannelGroupFf` | 0x80334D50 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `getVolume__Q24FMOD12ChannelGroupFPf` | 0x80334D98 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `setPitch__Q24FMOD12ChannelGroupFf` | 0x80334DE0 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `setPaused__Q24FMOD12ChannelGroupFb` | 0x80334E28 | 76 | matched |  |
| `fmod/fmod_channelgroup` | `setMute__Q24FMOD12ChannelGroupFb` | 0x80334E74 | 76 | matched |  |
| `fmod/fmod_channelgroup` | `getMute__Q24FMOD12ChannelGroupFPb` | 0x80334EC0 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `stop__Q24FMOD12ChannelGroupFv` | 0x80334F08 | 56 | matched |  |
| `fmod/fmod_channelgroup` | `overrideReverbProperties__Q24FMOD12ChannelGroupFPC29FMOD_REVERB_CHANNELPROPERTIES` | 0x80334F40 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `overrideSpeakerMix__Q24FMOD12ChannelGroupFffffffff` | 0x80334F88 | 136 | matched |  |
| `fmod/fmod_channelgroup` | `addGroup__Q24FMOD12ChannelGroupFPQ24FMOD12ChannelGroup` | 0x80335010 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `getDSPHead__Q24FMOD12ChannelGroupFPPQ24FMOD3DSP` | 0x80335058 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `addDSP__Q24FMOD12ChannelGroupFPQ24FMOD3DSPPPQ24FMOD13DSPConnection` | 0x803350A0 | 88 | matched |  |
| `fmod/fmod_channelgroup` | `getNumChannels__Q24FMOD12ChannelGroupFPi` | 0x803350F8 | 72 | matched |  |
| `fmod/fmod_channelgroup` | `getChannel__Q24FMOD12ChannelGroupFiPPQ24FMOD7Channel` | 0x80335140 | 88 | matched |  |
| `fmod/fmod_channelgroupi` | `validate__Q24FMOD13ChannelGroupIFPQ24FMOD12ChannelGroupPPQ24FMOD13ChannelGroupI` | 0x80335198 | 44 | matched |  |
| `fmod/fmod_channelgroupi` | `release__Q24FMOD13ChannelGroupIFv` | 0x803351C4 | 40 | matched |  |
| `fmod/fmod_channelgroupi` | `releaseInternal__Q24FMOD13ChannelGroupIFb` | 0x803351EC | 592 | matched |  |
| `fmod/fmod_channelgroupi` | `setVolume__Q24FMOD13ChannelGroupIFf` | 0x8033543C | 84 | matched |  |
| `fmod/fmod_channelgroupi` | `setVolumeInternal__Q24FMOD13ChannelGroupIFv` | 0x80335490 | 224 | matched |  |
| `fmod/fmod_channelgroupi` | `getVolume__Q24FMOD13ChannelGroupIFPf` | 0x80335570 | 32 | matched |  |
| `fmod/fmod_channelgroupi` | `setPitch__Q24FMOD13ChannelGroupIFf` | 0x80335590 | 328 | matched |  |
| `fmod/fmod_channelgroupi` | `setPitchInternal__Q24FMOD13ChannelGroupIFv` | 0x803356D8 | 308 | matched |  |
| `fmod/fmod_channelgroupi` | `setPaused__Q24FMOD13ChannelGroupIFbb` | 0x8033580C | 228 | matched |  |
| `fmod/fmod_channelgroupi` | `getPaused__Q24FMOD13ChannelGroupIFPb` | 0x803358F0 | 32 | matched |  |
| `fmod/fmod_channelgroupi` | `setMute__Q24FMOD13ChannelGroupIFbb` | 0x80335910 | 228 | matched |  |
| `fmod/fmod_channelgroupi` | `getMute__Q24FMOD13ChannelGroupIFPb` | 0x803359F4 | 32 | matched |  |
| `fmod/fmod_channelgroupi` | `stop__Q24FMOD13ChannelGroupIFv` | 0x80335A14 | 268 | matched |  |
| `fmod/fmod_channelgroupi` | `overrideReverbProperties__Q24FMOD13ChannelGroupIFPC29FMOD_REVERB_CHANNELPROPERTIES` | 0x80335B20 | 308 | matched |  |
| `fmod/fmod_channelgroupi` | `overrideSpeakerMix__Q24FMOD13ChannelGroupIFffffffff` | 0x80335C54 | 364 | matched |  |
| `fmod/fmod_channelgroupi` | `updateChildMixTarget__Q24FMOD13ChannelGroupIFPQ24FMOD4DSPI` | 0x80335DC0 | 244 | matched |  |
| `fmod/fmod_channelgroupi` | `addGroup__Q24FMOD13ChannelGroupIFPQ24FMOD13ChannelGroupI` | 0x80335EB4 | 1024 | matched |  |
| `fmod/fmod_channelgroupi` | `getDSPHead__Q24FMOD13ChannelGroupIFPPQ24FMOD4DSPI` | 0x803362B4 | 56 | matched |  |
| `fmod/fmod_channelgroupi` | `addDSP__Q24FMOD13ChannelGroupIFPQ24FMOD4DSPIPPQ24FMOD14DSPConnectionI` | 0x803362EC | 448 | matched |  |
| `fmod/fmod_channelgroupi` | `getNumChannels__Q24FMOD13ChannelGroupIFPi` | 0x803364AC | 32 | matched |  |
| `fmod/fmod_channelgroupi` | `getChannel__Q24FMOD13ChannelGroupIFiPPQ24FMOD7Channel` | 0x803364CC | 92 | matched |  |
| `fmod/fmod_channelpool` | `__ct__Q24FMOD11ChannelPoolFv` | 0x8033CC4C | 28 | matched |  |
| `fmod/fmod_channelpool` | `init__Q24FMOD11ChannelPoolFPQ24FMOD7SystemIPQ24FMOD6Outputi` | 0x8033CC68 | 164 | matched |  |
| `fmod/fmod_channelpool` | `release__Q24FMOD11ChannelPoolFv` | 0x8033CD0C | 200 | matched |  |
| `fmod/fmod_channelpool` | `allocateChannel__Q24FMOD11ChannelPoolFPPQ24FMOD11ChannelRealiiPib` | 0x8033CDD4 | 568 | matched |  |
| `fmod/fmod_channelpool` | `getNumChannels__Q24FMOD11ChannelPoolFPi` | 0x8033D00C | 32 | matched |  |
| `fmod/fmod_channelpool` | `getChannelsUsed__Q24FMOD11ChannelPoolFPi` | 0x8033D02C | 32 | matched |  |
| `fmod/fmod_channelpool` | `setChannel__Q24FMOD11ChannelPoolFiPQ24FMOD11ChannelRealPQ24FMOD4DSPI` | 0x8033D04C | 104 | matched |  |
| `fmod/fmod_channelpool` | `getChannel__Q24FMOD11ChannelPoolFiPPQ24FMOD11ChannelReal` | 0x8033D0B4 | 60 | matched |  |
| `fmod/fmod_globals` | `__sinit_\fmod_globals_cpp` | 0x8033D0F0 | 192 | matched | placeholder (compiler-generated name) |
| `fmod/fmod_globals` | `__dt__Q24FMOD7SystemIFv` | 0x8033D1B0 | 108 | matched |  |
| `fmod/fmod_listener` | `__ct__Q24FMOD8ListenerFv` | 0x8033D21C | 84 | matched |  |
| `fmod/fmod_metadata` | `init__Q24FMOD7TagNodeF12FMOD_TAGTYPEPCcPvUi16FMOD_TAGDATATYPE` | 0x8033D270 | 220 | matched |  |
| `fmod/fmod_metadata` | `release__Q24FMOD8MetadataFv` | 0x8033D34C | 248 | matched |  |
| `fmod/fmod_metadata` | `getNumTags__Q24FMOD8MetadataFPiPi` | 0x8033D444 | 80 | matched |  |
| `fmod/fmod_metadata` | `getTag__Q24FMOD8MetadataFPCciP8FMOD_TAG` | 0x8033D494 | 380 | matched |  |
| `fmod/fmod_metadata` | `add__Q24FMOD8MetadataFPQ24FMOD8Metadata` | 0x8033D610 | 508 | matched |  |
| `fmod/fmod_metadata` | `addTag__Q24FMOD8MetadataF12FMOD_TAGTYPEPCcPvUi16FMOD_TAGDATATYPEb` | 0x8033D80C | 528 | matched |  |
| `fmod/fmod_music` | `play__Q24FMOD9MusicSongFb` | 0x8033DA1C | 688 | matched |  |
| `fmod/fmod_music` | `spawnNewVirtualChannel__Q24FMOD9MusicSongFPQ24FMOD12MusicChannelPQ24FMOD11MusicSamplePPQ24FMOD19MusicVirtualChannel` | 0x8033DCCC | 244 | matched |  |
| `fmod/fmod_music` | `setBPM__Q24FMOD9MusicSongFi` | 0x8033DDC0 | 148 | matched |  |
| `fmod/fmod_music` | `stop__Q24FMOD9MusicSongFv` | 0x8033DE54 | 344 | matched |  |
| `fmod/fmod_music` | `playSound__Q24FMOD9MusicSongFPQ24FMOD11MusicSamplePQ24FMOD19MusicVirtualChannelbPQ24FMOD13_SNDMIXPLUGIN` | 0x8033DFAC | 436 | matched |  |
| `fmod/fmod_music` | `updateStream__Q24FMOD12ChannelMusicFv` | 0x8033E160 | 104 | matched |  |
| `fmod/fmod_music` | `stop__Q24FMOD12ChannelMusicFv` | 0x8033E1C8 | 8 | matched |  |
| `fmod/fmod_music` | `start__Q24FMOD12ChannelMusicFv` | 0x8033E1D0 | 12 | matched |  |
| `fmod/fmod_music` | `setPaused__Q24FMOD12ChannelMusicFb` | 0x8033E1DC | 172 | matched |  |
| `fmod/fmod_music` | `setVolume__Q24FMOD12ChannelMusicFf` | 0x8033E288 | 52 | matched |  |
| `fmod/fmod_music` | `getLengthCallback__Q24FMOD9MusicSongFP16FMOD_CODEC_STATEPUiUi` | 0x8033E2BC | 116 | matched |  |
| `fmod/fmod_music` | `getPositionCallback__Q24FMOD9MusicSongFP16FMOD_CODEC_STATEPUiUi` | 0x8033E330 | 104 | matched |  |
| `fmod/fmod_music` | `getMusicNumChannelsCallback__Q24FMOD9MusicSongFP16FMOD_CODEC_STATEPi` | 0x8033E398 | 44 | matched |  |
| `fmod/fmod_music` | `setMusicChannelVolumeCallback__Q24FMOD9MusicSongFP16FMOD_CODEC_STATEif` | 0x8033E3C4 | 92 | matched |  |
| `fmod/fmod_music` | `getMusicChannelVolumeCallback__Q24FMOD9MusicSongFP16FMOD_CODEC_STATEiPf` | 0x8033E420 | 80 | matched |  |
| `fmod/fmod_music` | `getHardwareMusicChannelCallback__Q24FMOD9MusicSongFP16FMOD_CODEC_STATEPPQ24FMOD11ChannelReal` | 0x8033E470 | 144 | matched |  |
| `fmod/fmod_music` | `__dt__Q24FMOD12ChannelMusicFv` | 0x8033E500 | 64 | matched |  |
| `fmod/fmod_music` | `__sinit_\fmod_music_cpp` | 0x8033E540 | 680 | matched | placeholder (compiler-generated name) |
| `fmod/fmod_plugin` | `release__Q24FMOD6PluginFv` | 0x8033E7E8 | 64 | matched |  |
| `fmod/fmod_pluginfactory` | `defaultMetaData__Q24FMOD5CodecFP16FMOD_CODEC_STATE12FMOD_TAGTYPEPcPvUi16FMOD_TAGDATATYPEi` | 0x8033E828 | 28 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `defaultFileSeek__Q24FMOD5CodecFPvUiPv` | 0x8033E844 | 8 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `defaultFileRead__Q24FMOD5CodecFPvPvUiPUiPv` | 0x8033E84C | 16 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `__ct__Q24FMOD13PluginFactoryFv` | 0x8033E85C | 196 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `release__Q24FMOD13PluginFactoryFv` | 0x8033E920 | 592 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `setSystem__Q24FMOD13PluginFactoryFPQ24FMOD7SystemI` | 0x8033EB70 | 12 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `setPluginPath__Q24FMOD13PluginFactoryFPCc` | 0x8033EB7C | 96 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `unloadPlugin__Q24FMOD13PluginFactoryFUi` | 0x8033EBDC | 564 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `registerCodec__Q24FMOD13PluginFactoryFPQ24FMOD25FMOD_CODEC_DESCRIPTION_EXPUiUi` | 0x8033EE10 | 468 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `registerOutput__Q24FMOD13PluginFactoryFPQ24FMOD26FMOD_OUTPUT_DESCRIPTION_EXPUi` | 0x8033EFE4 | 536 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getNumCodecs__Q24FMOD13PluginFactoryFPi` | 0x8033F1FC | 60 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getNumDSPs__Q24FMOD13PluginFactoryFPi` | 0x8033F238 | 60 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getNumOutputs__Q24FMOD13PluginFactoryFPi` | 0x8033F274 | 60 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getCodecHandle__Q24FMOD13PluginFactoryFiPUi` | 0x8033F2B0 | 116 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getDSPHandle__Q24FMOD13PluginFactoryFiPUi` | 0x8033F324 | 116 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getOutputHandle__Q24FMOD13PluginFactoryFiPUi` | 0x8033F398 | 116 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getCodec__Q24FMOD13PluginFactoryFUiPPQ24FMOD25FMOD_CODEC_DESCRIPTION_EX` | 0x8033F40C | 104 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getDSP__Q24FMOD13PluginFactoryFUiPPQ24FMOD23FMOD_DSP_DESCRIPTION_EX` | 0x8033F474 | 104 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `getOutput__Q24FMOD13PluginFactoryFUiPPQ24FMOD26FMOD_OUTPUT_DESCRIPTION_EX` | 0x8033F4DC | 104 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `createCodec__Q24FMOD13PluginFactoryFPQ24FMOD25FMOD_CODEC_DESCRIPTION_EXPPQ24FMOD5Codec` | 0x8033F544 | 328 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `defaultGetWaveFormat__Q24FMOD5CodecFP16FMOD_CODEC_STATEiP21FMOD_CODEC_WAVEFORMAT` | 0x8033F68C | 120 | parked, 100% |  |
| `fmod/fmod_pluginfactory` | `createDSP__Q24FMOD13PluginFactoryFPQ24FMOD23FMOD_DSP_DESCRIPTION_EXPPQ24FMOD4DSPI` | 0x8033F704 | 988 | parked, not written |  |
| `fmod/fmod_pluginfactory` | `createOutput__Q24FMOD13PluginFactoryFPQ24FMOD26FMOD_OUTPUT_DESCRIPTION_EXPPQ24FMOD6Output` | 0x8033FAE0 | 308 | parked, 100% |  |
| `fmod/fmod_reverb` | `release__Q24FMOD6ReverbFv` | 0x8033FC14 | 60 | matched |  |
| `fmod/fmod_reverb` | `set3DAttributes__Q24FMOD6ReverbFPC11FMOD_VECTORff` | 0x8033FC50 | 104 | matched |  |
| `fmod/fmod_reverb` | `get3DAttributes__Q24FMOD6ReverbFP11FMOD_VECTORPfPf` | 0x8033FCB8 | 104 | matched |  |
| `fmod/fmod_reverb` | `setProperties__Q24FMOD6ReverbFPC22FMOD_REVERB_PROPERTIES` | 0x8033FD20 | 72 | matched |  |
| `fmod/fmod_reverb` | `getProperties__Q24FMOD6ReverbFP22FMOD_REVERB_PROPERTIES` | 0x8033FD68 | 72 | matched |  |
| `fmod/fmod_reverb` | `setActive__Q24FMOD6ReverbFb` | 0x8033FDB0 | 72 | matched |  |
| `fmod/fmod_reverb` | `getActive__Q24FMOD6ReverbFPb` | 0x8033FDF8 | 72 | matched |  |
| `fmod/fmod_reverbi` | `__ct__Q24FMOD7ReverbIFv` | 0x8033FE40 | 108 | matched |  |
| `fmod/fmod_reverbi` | `validate__Q24FMOD7ReverbIFPQ24FMOD6ReverbPPQ24FMOD7ReverbI` | 0x8033FEAC | 44 | matched |  |
| `fmod/fmod_reverbi` | `init__Q24FMOD7ReverbIFPQ24FMOD7SystemIbQ24FMOD16FMOD_REVERB_MODE` | 0x8033FED8 | 684 | matched |  |
| `fmod/fmod_reverbi` | `release__Q24FMOD7ReverbIFb` | 0x80340184 | 280 | matched |  |
| `fmod/fmod_reverbi` | `createDSP__Q24FMOD7ReverbIFi` | 0x8034029C | 792 | matched |  |
| `fmod/fmod_reverbi` | `getChanProperties__Q24FMOD7ReverbIFiiP29FMOD_REVERB_CHANNELPROPERTIESPPQ24FMOD14DSPConnectionI` | 0x803405B4 | 248 | matched |  |
| `fmod/fmod_reverbi` | `setChanProperties__Q24FMOD7ReverbIFiiPC29FMOD_REVERB_CHANNELPROPERTIESPQ24FMOD14DSPConnectionI` | 0x803406AC | 224 | matched |  |
| `fmod/fmod_reverbi` | `resetChanProperties__Q24FMOD7ReverbIFii` | 0x8034078C | 220 | matched |  |
| `fmod/fmod_reverbi` | `resetConnectionPointer__Q24FMOD7ReverbIFii` | 0x80340868 | 52 | matched |  |
| `fmod/fmod_reverbi` | `set3DAttributes__Q24FMOD7ReverbIFPC11FMOD_VECTORff` | 0x8034089C | 112 | matched |  |
| `fmod/fmod_reverbi` | `get3DAttributes__Q24FMOD7ReverbIFP11FMOD_VECTORPfPf` | 0x8034090C | 72 | matched |  |
| `fmod/fmod_reverbi` | `setProperties__Q24FMOD7ReverbIFPC22FMOD_REVERB_PROPERTIES` | 0x80340954 | 1300 | matched |  |
| `fmod/fmod_reverbi` | `getProperties__Q24FMOD7ReverbIFP22FMOD_REVERB_PROPERTIES` | 0x80340E68 | 308 | matched |  |
| `fmod/fmod_reverbi` | `setActive__Q24FMOD7ReverbIFb` | 0x80340F9C | 12 | matched |  |
| `fmod/fmod_reverbi` | `getActive__Q24FMOD7ReverbIFPb` | 0x80340FA8 | 32 | matched |  |
| `fmod/fmod_reverbi` | `setPresenceGain__Q24FMOD7ReverbIFiif` | 0x80340FC8 | 80 | matched |  |
| `fmod/fmod_sample_software` | `__ct__Q24FMOD14SampleSoftwareFv` | 0x80341018 | 76 | matched |  |
| `fmod/fmod_sample_software` | `release__Q24FMOD14SampleSoftwareFb` | 0x80341064 | 364 | matched |  |
| `fmod/fmod_sample_software` | `setMode__Q24FMOD14SampleSoftwareFUi` | 0x803411D0 | 84 | matched |  |
| `fmod/fmod_sample_software` | `lockInternal__Q24FMOD14SampleSoftwareFUiUiPPvPPvPUiPUi` | 0x80341224 | 952 | matched |  |
| `fmod/fmod_sample_software` | `unlockInternal__Q24FMOD14SampleSoftwareFPvPvUiUi` | 0x803415DC | 52 | matched |  |
| `fmod/fmod_sample_software` | `setBufferData__Q24FMOD14SampleSoftwareFPv` | 0x80341610 | 12 | matched |  |
| `fmod/fmod_sample_software` | `setLoopPointData__Q24FMOD14SampleSoftwareFv` | 0x8034161C | 1644 | matched |  |
| `fmod/fmod_sample_software` | `restoreLoopPointData__Q24FMOD14SampleSoftwareFv` | 0x80341C88 | 860 | matched |  |
| `fmod/fmod_sound_sample` | `__ct__Q24FMOD6SampleFv` | 0x80341FE4 | 76 | matched |  |
| `fmod/fmod_sound_sample` | `lock__Q24FMOD6SampleFUiUiPPvPPvPUiPUi` | 0x80342030 | 3776 | matched |  |
| `fmod/fmod_sound_sample` | `lockInternal__Q24FMOD6SampleFUiUiPPvPPvPUiPUi` | 0x80342EF0 | 8 | matched |  |
| `fmod/fmod_sound_sample` | `unlock__Q24FMOD6SampleFPvPvUiUi` | 0x80342EF8 | 3424 | matched |  |
| `fmod/fmod_sound_sample` | `unlockInternal__Q24FMOD6SampleFPvPvUiUi` | 0x80343C58 | 8 | matched |  |
| `fmod/fmod_sound_sample` | `release__Q24FMOD6SampleFb` | 0x80343C60 | 232 | matched |  |
| `fmod/fmod_sound_sample` | `setDefaults__Q24FMOD6SampleFfffi` | 0x80343D48 | 192 | matched |  |
| `fmod/fmod_sound_sample` | `setVariations__Q24FMOD6SampleFfff` | 0x80343E08 | 176 | matched |  |
| `fmod/fmod_sound_sample` | `set3DMinMaxDistance__Q24FMOD6SampleFff` | 0x80343EB8 | 160 | matched |  |
| `fmod/fmod_sound_sample` | `set3DConeSettings__Q24FMOD6SampleFfff` | 0x80343F58 | 176 | matched |  |
| `fmod/fmod_sound_sample` | `setMode__Q24FMOD6SampleFUi` | 0x80344008 | 144 | matched |  |
| `fmod/fmod_sound_sample` | `setLoopCount__Q24FMOD6SampleFi` | 0x80344098 | 144 | matched |  |
| `fmod/fmod_sound_sample` | `setLoopPoints__Q24FMOD6SampleFUiUiUiUi` | 0x80344128 | 144 | matched |  |
| `fmod/fmod_sound_sample` | `setBufferData__Q24FMOD6SampleFPv` | 0x803441B8 | 8 | matched |  |
| `fmod/fmod_sound_stream` | `__ct__Q24FMOD6StreamFv` | 0x803441C0 | 112 | matched |  |
| `fmod/fmod_sound_stream` | `fill__Q24FMOD6StreamFUiUiPUib` | 0x80344230 | 1824 | matched |  |
| `fmod/fmod_sound_stream` | `flush__Q24FMOD6StreamFv` | 0x80344950 | 168 | matched |  |
| `fmod/fmod_sound_stream` | `setPosition__Q24FMOD6StreamFUiUi` | 0x803449F8 | 808 | matched |  |
| `fmod/fmod_sound_stream` | `getPosition__Q24FMOD6StreamFPUiUi` | 0x80344D20 | 236 | matched |  |
| `fmod/fmod_sound_stream` | `setLoopCount__Q24FMOD6StreamFi` | 0x80344E0C | 44 | matched |  |
| `fmod/fmod_sound_stream` | `isStream__Q24FMOD6StreamFv` | 0x80344E38 | 8 | matched |  |
| `fmod/fmod_soundgroupi` | `__ct__Q24FMOD11SoundGroupIFv` | 0x80344E40 | 64 | matched |  |
| `fmod/fmod_soundgroupi` | `releaseInternal__Q24FMOD11SoundGroupIFv` | 0x80344E80 | 156 | matched |  |
| `fmod/fmod_soundgroupi` | `getNumPlaying__Q24FMOD11SoundGroupIFPi` | 0x80344F1C | 76 | matched |  |
| `fmod/fmod_speakerlevels_pool` | `__ct__Q24FMOD17SpeakerLevelsPoolFv` | 0x80349CFC | 12 | matched |  |
| `fmod/fmod_speakerlevels_pool` | `alloc__Q24FMOD17SpeakerLevelsPoolFPPf` | 0x80349D08 | 440 | matched |  |
| `fmod/fmod_speakerlevels_pool` | `free__Q24FMOD17SpeakerLevelsPoolFPf` | 0x80349EC0 | 92 | matched |  |
| `fmod/fmod_speakerlevels_pool` | `release__Q24FMOD17SpeakerLevelsPoolFv` | 0x80349F1C | 196 | matched |  |
| `fmod/fmod_codec_dsp` | `getDescriptionEx__Q24FMOD8CodecDSPFv` | 0x80356C38 | 168 | parked, 100% |  |
| `fmod/fmod_codec_dsp` | `openInternal__Q24FMOD8CodecDSPFUiP22FMOD_CREATESOUNDEXINFO` | 0x80356CE0 | 748 | parked, 100% |  |
| `fmod/fmod_codec_dsp` | `openCallback__Q24FMOD8CodecDSPFP16FMOD_CODEC_STATEUiP22FMOD_CREATESOUNDEXINFO` | 0x80356FCC | 16 | parked, 100% |  |
| `fmod/fmod_codec_dsp` | `closeCallback__Q24FMOD8CodecDSPFP16FMOD_CODEC_STATE` | 0x80356FDC | 8 | parked, 100% |  |
| `fmod/fmod_codec_dsp` | `readCallback__Q24FMOD8CodecDSPFP16FMOD_CODEC_STATEPvUiPUi` | 0x80356FE4 | 32 | parked, 100% |  |
| `fmod/fmod_codec_dsp` | `setPositionCallback__Q24FMOD8CodecDSPFP16FMOD_CODEC_STATEiUiUi` | 0x80357004 | 448 | parked, 96.2% |  |
| `fmod/fmod_codec_dsp` | `canPointCallback__Q24FMOD8CodecDSPFP16FMOD_CODEC_STATE` | 0x803571C4 | 8 | parked, 100% |  |
| `fmod/fmod_codec_dsp` | `__sinit_\fmod_codec_dsp_cpp` | 0x803571CC | 40 | parked, 100% | placeholder (compiler-generated name) |
| `fmod/fmod_dsp_connection_wii` | `FMOD_DSP_Connection_MixMonoToStereo_SIMD` | 0x803571F4 | 376 | parked, 60.7% (C with an inline-asm block; listed helper when linked) |  |
| `fmod/fmod_dsp_connection_wii` | `FMOD_DSP_Connection_MixStereoToStereo_SIMD` | 0x8035736C | 368 | parked, 1.1% |  |
| `fmod/fmod_dsp_connection_wii` | `FMOD_DSP_Connection_MixMonoTo5_1_SIMD` | 0x803574DC | 952 | parked, 0.4% |  |
| `fmod/fmod_dsp_connection_wii` | `FMOD_DSP_Connection_Mix5_1To5_1_SIMD` | 0x80357894 | 936 | parked, 0.4% |  |
| `fmod/fmod_dsp_convert_wii` | `FMOD_DSP_Convert_FloatToPCM16` | 0x80357C3C | 460 | parked, 92.0% |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Memory_Alloc__Fi` | 0x80357E08 | 20 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Memory_Realloc__FPvi` | 0x80357E1C | 152 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Memory_Free__FPv` | 0x80357EB4 | 12 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_File_Open__FPCcPciPUiPPv` | 0x80357EC0 | 276 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_File_Close__FPv` | 0x80357FD4 | 96 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_File_Cancel__FPv` | 0x80358034 | 52 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_File_AsyncRead__FP6GCFILEPviiPUi` | 0x80358068 | 376 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_File_Read__FPvPvUiPUi` | 0x803581E0 | 400 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_File_Seek__FPvUi` | 0x80358370 | 36 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Time_GetNs__FPUi` | 0x80358394 | 88 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Time_GetMs__FPUi` | 0x803583EC | 84 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Time_Sleep__FUi` | 0x80358440 | 92 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Thread_GetCurrentID__FPUi` | 0x8035849C | 52 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Thread_Create__FPCcPFPv_UiPv20FMOD_THREAD_PRIORITYPviPPv` | 0x803584D0 | 384 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Thread_Destroy__FPv` | 0x80358650 | 80 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_CriticalSection_Create__FPP23FMOD_OS_CRITICALSECTIONb` | 0x803586A0 | 152 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_CriticalSection_Free__FP23FMOD_OS_CRITICALSECTIONb` | 0x80358738 | 96 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_CriticalSection_Enter__FP23FMOD_OS_CRITICALSECTION` | 0x80358798 | 52 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_CriticalSection_Leave__FP23FMOD_OS_CRITICALSECTION` | 0x803587CC | 52 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Semaphore_Create__FPP17FMOD_OS_SEMAPHORE` | 0x80358800 | 116 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Semaphore_Free__FP17FMOD_OS_SEMAPHORE` | 0x80358874 | 80 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Semaphore_Wait__FP17FMOD_OS_SEMAPHORE` | 0x803588C4 | 52 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_Semaphore_Signal__FP17FMOD_OS_SEMAPHOREb` | 0x803588F8 | 52 | matched |  |
| `fmod/fmod_os_misc` | `FMOD_OS_CheckDriverList__FPb` | 0x8035892C | 8 | matched |  |
| `fmod/fmod_os_output` | `FMOD_OS_Output_Register__FPQ24FMOD13PluginFactory` | 0x80358934 | 64 | matched |  |
| `fmod/fmod_os_output` | `FMOD_OS_Output_GetDefault__FP15FMOD_OUTPUTTYPE` | 0x80358974 | 32 | matched |  |
| `fmod/fmod_sample_revolution` | `__ct__Q24FMOD16SampleRevolutionFv` | 0x8035AB74 | 84 | matched |  |
| `fmod/fmod_sample_revolution` | `release__Q24FMOD16SampleRevolutionFb` | 0x8035ABC8 | 244 | matched |  |
| `fmod/fmod_sample_revolution` | `lockInternal__Q24FMOD16SampleRevolutionFUiUiPPvPPvPUiPUi` | 0x8035ACBC | 572 | matched |  |
| `fmod/fmod_sample_revolution` | `unlockInternal__Q24FMOD16SampleRevolutionFPvPvUiUi` | 0x8035AEF8 | 196 | matched |  |
| `fmod/fmod_sample_revolution` | `setBufferData__Q24FMOD16SampleRevolutionFPv` | 0x8035AFBC | 32 | matched |  |
| `fmod/fmod_dsp_codec` | `release__Q24FMOD8DSPCodecFb` | 0x8035AFDC | 176 | matched |  |
| `fmod/fmod_dsp_connectioni` | `init__Q24FMOD14DSPConnectionIFRPfii` | 0x8035B0AC | 280 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `reset__Q24FMOD14DSPConnectionIFv` | 0x8035B1C4 | 128 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `setUnity__Q24FMOD14DSPConnectionIFv` | 0x8035B244 | 140 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `mix__Q24FMOD14DSPConnectionIFPfPfiiUi` | 0x8035B2D0 | 4504 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `mixAndRamp__Q24FMOD14DSPConnectionIFPfPfiiUi` | 0x8035C468 | 756 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `rampTo__Q24FMOD14DSPConnectionIFv` | 0x8035C75C | 492 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `checkUnity__Q24FMOD14DSPConnectionIFii` | 0x8035C948 | 192 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `setPan__Q24FMOD14DSPConnectionIFfii16FMOD_SPEAKERMODE` | 0x8035CA08 | 1888 | parked, 99.8% |  |
| `fmod/fmod_dsp_connectioni` | `setLevels__Q24FMOD14DSPConnectionIFPfi` | 0x8035D168 | 420 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `getLevels__Q24FMOD14DSPConnectionIFPfi` | 0x8035D30C | 144 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `setMix__Q24FMOD14DSPConnectionIFf` | 0x8035D39C | 64 | parked, 100% |  |
| `fmod/fmod_dsp_connectioni` | `copy__Q24FMOD14DSPConnectionIFPQ24FMOD14DSPConnectionI` | 0x8035D3DC | 152 | parked, 100% |  |
| `fmod/fmod_dsp_connectionpool` | `init__Q24FMOD17DSPConnectionPoolFPQ24FMOD7SystemIiii` | 0x8035D474 | 644 | matched |  |
| `fmod/fmod_dsp_connectionpool` | `close__Q24FMOD17DSPConnectionPoolFv` | 0x8035D6F8 | 220 | matched |  |
| `fmod/fmod_dsp_connectionpool` | `alloc__Q24FMOD17DSPConnectionPoolFPPQ24FMOD14DSPConnectionIb` | 0x8035D7D4 | 1064 | matched |  |
| `fmod/fmod_dsp_connectionpool` | `free__Q24FMOD17DSPConnectionPoolFPQ24FMOD14DSPConnectionIb` | 0x8035DBFC | 372 | matched |  |
| `fmod/fmod_dsp_filter` | `release__Q24FMOD9DSPFilterFb` | 0x8035ECC0 | 80 | matched |  |
| `fmod/fmod_dsp_filter` | `execute__Q24FMOD9DSPFilterFPfPPfPUiiPi16FMOD_SPEAKERMODEUi` | 0x8035ED10 | 1748 | matched |  |
| `fmod/fmod_dsp_filter` | `startBuffering__Q24FMOD9DSPFilterFUi` | 0x8035F3E4 | 376 | matched |  |
| `fmod/fmod_dsp_filter` | `getHistoryBuffer__Q24FMOD9DSPFilterFPPfPUiPUi` | 0x8035F55C | 56 | matched |  |
| `fmod/fmod_dsp_filter` | `stopBuffering__Q24FMOD9DSPFilterFv` | 0x8035F594 | 152 | matched |  |
| `fmod/fmod_dsp_resampler` | `__ct__Q24FMOD12DSPResamplerFv` | 0x8035F62C | 148 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `release__Q24FMOD12DSPResamplerFb` | 0x8035F6C0 | 152 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `alloc__Q24FMOD12DSPResamplerFPQ24FMOD23FMOD_DSP_DESCRIPTION_EX` | 0x8035F758 | 684 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `execute__Q24FMOD12DSPResamplerFPfPPfPUiiPi16FMOD_SPEAKERMODEUi` | 0x8035FA04 | 2408 | parked, 99.6% |  |
| `fmod/fmod_dsp_resampler` | `addInput__Q24FMOD12DSPResamplerFPQ24FMOD4DSPI` | 0x8036036C | 8 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `setFrequency__Q24FMOD12DSPResamplerFf` | 0x80360374 | 144 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `setPosition__Q24FMOD12DSPResamplerFUib` | 0x80360404 | 112 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `setFinished__Q24FMOD12DSPResamplerFbb` | 0x80360474 | 160 | parked, 100% |  |
| `fmod/fmod_dsp_resampler` | `getFinished__Q24FMOD12DSPResamplerFPb` | 0x80360514 | 100 | parked, 100% |  |
| `fmod/fmod_dsp_soundcard` | `alloc__Q24FMOD12DSPSoundCardFPQ24FMOD23FMOD_DSP_DESCRIPTION_EX` | 0x803649A4 | 192 | matched |  |
| `fmod/fmod_dsp_soundcard` | `release__Q24FMOD12DSPSoundCardFb` | 0x80364A64 | 120 | matched |  |
| `fmod/fmod_dsp_soundcard` | `execute__Q24FMOD12DSPSoundCardFPvPPvPUiiPi16FMOD_SPEAKERMODEUi` | 0x80364ADC | 528 | matched |  |
| `fmod/fmod_dsp_wavetable` | `alloc__Q24FMOD12DSPWaveTableFPQ24FMOD23FMOD_DSP_DESCRIPTION_EX` | 0x80364CEC | 164 | matched |  |
| `fmod/fmod_dsp_wavetable` | `execute__Q24FMOD12DSPWaveTableFPfPPfPUiiPi16FMOD_SPEAKERMODEUi` | 0x80364D90 | 2484 | matched |  |
| `fmod/fmod_dsp_wavetable` | `setFrequency__Q24FMOD12DSPWaveTableFf` | 0x80365744 | 172 | matched |  |
| `fmod/fmod_dsp_wavetable` | `setFinished__Q24FMOD12DSPWaveTableFbb` | 0x803657F0 | 144 | matched |  |
| `fmod/fmod_dsp_wavetable` | `getFinished__Q24FMOD12DSPWaveTableFPb` | 0x80365880 | 84 | matched |  |
| `fmod/fmod_dsp_wavetable` | `setPositionCallback__Q24FMOD12DSPWaveTableFP14FMOD_DSP_STATEUi` | 0x803658D4 | 60 | matched |  |
| `fmod/fmod_dsp_wavetable` | `setParameterCallback__Q24FMOD12DSPWaveTableFP14FMOD_DSP_STATEif` | 0x80365910 | 8 | matched |  |
| `fmod/fmod_dsp_wavetable` | `getParameterCallback__Q24FMOD12DSPWaveTableFP14FMOD_DSP_STATEiPfPc` | 0x80365918 | 8 | matched |  |
| `fmod/fmod_dsp_wavetable` | `resetCallback__Q24FMOD12DSPWaveTableFP14FMOD_DSP_STATE` | 0x80365920 | 28 | matched |  |
| `fmod/fmod_codec` | `release__Q24FMOD5CodecFv` | 0x80369770 | 228 | matched |  |
| `fmod/fmod_codec` | `read__Q24FMOD5CodecFPvUiPUi` | 0x80369854 | 568 | matched |  |
| `fmod/fmod_codec` | `getMetadataFromFile__Q24FMOD5CodecFv` | 0x80369A8C | 228 | matched |  |
| `fmod/fmod_codec` | `getMetadata__Q24FMOD4FileFPPQ24FMOD8Metadata` | 0x80369B70 | 8 | matched |  |
| `fmod/fmod_codec` | `getLength__Q24FMOD5CodecFPUiUi` | 0x80369B78 | 192 | matched |  |
| `fmod/fmod_codec` | `setPosition__Q24FMOD5CodecFiUiUi` | 0x80369C38 | 1840 | matched |  |
| `fmod/fmod_codec` | `getPosition__Q24FMOD5CodecFPUiUi` | 0x8036A368 | 228 | matched |  |
| `fmod/fmod_codec` | `metaData__Q24FMOD5CodecF12FMOD_TAGTYPEPCcPvUi16FMOD_TAGDATATYPEb` | 0x8036A44C | 232 | matched |  |
| `fmod/fmod_codec_raw` | `getDescriptionEx__Q24FMOD8CodecRawFv` | 0x8036D3B4 | 168 | matched |  |
| `fmod/fmod_codec_raw` | `openInternal__Q24FMOD8CodecRawFUiP22FMOD_CREATESOUNDEXINFO` | 0x8036D45C | 612 | matched |  |
| `fmod/fmod_codec_raw` | `openCallback__Q24FMOD8CodecRawFP16FMOD_CODEC_STATEUiP22FMOD_CREATESOUNDEXINFO` | 0x8036D6C0 | 16 | matched |  |
| `fmod/fmod_codec_raw` | `closeCallback__Q24FMOD8CodecRawFP16FMOD_CODEC_STATE` | 0x8036D6D0 | 8 | matched |  |
| `fmod/fmod_codec_raw` | `readCallback__Q24FMOD8CodecRawFP16FMOD_CODEC_STATEPvUiPUi` | 0x8036D6D8 | 124 | matched |  |
| `fmod/fmod_codec_raw` | `setPositionCallback__Q24FMOD8CodecRawFP16FMOD_CODEC_STATEiUiUi` | 0x8036D754 | 156 | matched |  |
| `fmod/fmod_codec_raw` | `canPointCallback__Q24FMOD8CodecRawFP16FMOD_CODEC_STATE` | 0x8036D7F0 | 8 | matched |  |
| `fmod/fmod_codec_raw` | `__sinit_\fmod_codec_raw_cpp` | 0x8036D7F8 | 40 | matched | placeholder (compiler-generated name) |
| `fmod/fmod_codec_user` | `getDescriptionEx__Q24FMOD9CodecUserFv` | 0x8036D820 | 156 | matched |  |
| `fmod/fmod_codec_user` | `openInternal__Q24FMOD9CodecUserFUiP22FMOD_CREATESOUNDEXINFO` | 0x8036D8BC | 876 | matched |  |
| `fmod/fmod_codec_user` | `openCallback__Q24FMOD9CodecUserFP16FMOD_CODEC_STATEUiP22FMOD_CREATESOUNDEXINFO` | 0x8036DC28 | 16 | matched |  |
| `fmod/fmod_codec_user` | `closeCallback__Q24FMOD9CodecUserFP16FMOD_CODEC_STATE` | 0x8036DC38 | 8 | matched |  |
| `fmod/fmod_codec_user` | `readCallback__Q24FMOD9CodecUserFP16FMOD_CODEC_STATEPvUiPUi` | 0x8036DC40 | 12 | matched |  |
| `fmod/fmod_codec_user` | `setPositionCallback__Q24FMOD9CodecUserFP16FMOD_CODEC_STATEiUiUi` | 0x8036DC4C | 8 | matched |  |
| `fmod/fmod_codec_user` | `__sinit_\fmod_codec_user_cpp` | 0x8036DC54 | 40 | matched | placeholder (compiler-generated name) |
| `fmod/fmod_output_emulated` | `__ct__Q24FMOD14OutputEmulatedFv` | 0x80372534 | 84 | matched |  |
| `fmod/fmod_output_emulated` | `init__Q24FMOD14OutputEmulatedFi` | 0x80372588 | 320 | matched |  |
| `fmod/fmod_output_emulated` | `release__Q24FMOD14OutputEmulatedFv` | 0x803726C8 | 124 | matched |  |
| `fmod/fmod_output_emulated` | `update__Q24FMOD14OutputEmulatedFv` | 0x80372744 | 8 | matched |  |
| `fmod/fmod_output_polled` | `__ct__Q24FMOD12OutputPolledFv` | 0x8037274C | 84 | matched |  |
| `fmod/fmod_output_polled` | `threadFunc__Q24FMOD12OutputPolledFv` | 0x803727A0 | 1572 | matched |  |
| `fmod/fmod_output_polled` | `start__Q24FMOD12OutputPolledFv` | 0x80372DC4 | 392 | matched |  |
| `fmod/fmod_output_polled` | `stop__Q24FMOD12OutputPolledFv` | 0x80372F4C | 92 | matched |  |
| `fmod/fmod_output_polled` | `__dt__Q24FMOD12OutputPolledFv` | 0x80372FA8 | 64 | matched |  |
| `fmod/fmod_output_polled` | `@240@__dt__Q24FMOD12OutputPolledFv` | 0x80372FE8 | 8 | matched | compiler-generated thunk |
| `fmod/fmod_output_polled` | `@240@threadFunc__Q24FMOD12OutputPolledFv` | 0x80372FF0 | 8 | matched | compiler-generated thunk |
| `fmod/fmod_output_software` | `__ct__Q24FMOD14OutputSoftwareFv` | 0x80372FF8 | 144 | matched |  |
| `fmod/fmod_output_software` | `init__Q24FMOD14OutputSoftwareFi` | 0x80373088 | 332 | matched |  |
| `fmod/fmod_output_software` | `release__Q24FMOD14OutputSoftwareFv` | 0x803731D4 | 124 | matched |  |
| `fmod/fmod_output_software` | `createSample__Q24FMOD14OutputSoftwareFUiP21FMOD_CODEC_WAVEFORMATPPQ24FMOD6Sample` | 0x80373250 | 1512 | matched |  |
| `fmod/fmod_output_software` | `getSampleMaxChannelsCallback__Q24FMOD14OutputSoftwareFP17FMOD_OUTPUT_STATEUi17FMOD_SOUND_FORMAT` | 0x80373838 | 8 | matched |  |
| `fmod/fmod_file` | `FMOD_File_SetDiskBusy` | 0x80373840 | 92 | parked, 100% |  |
| `fmod/fmod_file` | `fileThreadFunc__4FMODFPv` | 0x8037389C | 172 | parked, 100% |  |
| `fmod/fmod_file` | `getFileThread__Q24FMOD4FileFv` | 0x80373948 | 420 | parked, 100% |  |
| `fmod/fmod_file` | `__ct__Q24FMOD4FileFv` | 0x80373AEC | 84 | parked, 100% |  |
| `fmod/fmod_file` | `init__Q24FMOD4FileFUii` | 0x80373B40 | 152 | parked, 100% |  |
| `fmod/fmod_file` | `shutDown__Q24FMOD4FileFv` | 0x80373BD8 | 236 | parked, 100% |  |
| `fmod/fmod_file` | `open__Q24FMOD4FileFPCcUibPCc` | 0x80373CC4 | 600 | parked, 100% |  |
| `fmod/fmod_file` | `close__Q24FMOD4FileFv` | 0x80373F1C | 488 | parked, 100% |  |
| `fmod/fmod_file` | `cancel__Q24FMOD4FileFv` | 0x80374104 | 28 | parked, 100% |  |
| `fmod/fmod_file` | `reallyCancel__Q24FMOD4FileFv` | 0x80374120 | 8 | parked, 100% |  |
| `fmod/fmod_file` | `flip__Q24FMOD4FileFb` | 0x80374128 | 616 | parked, 100% |  |
| `fmod/fmod_file` | `seekAndReset__Q24FMOD4FileFv` | 0x80374390 | 236 | parked, 100% |  |
| `fmod/fmod_file` | `checkBufferedStatus__Q24FMOD4FileFv` | 0x8037447C | 808 | parked, 100% |  |
| `fmod/fmod_file` | `read__Q24FMOD4FileFPvUiUiPUi` | 0x803747A4 | 1656 | parked, 100% |  |
| `fmod/fmod_file` | `getByte__Q24FMOD4FileFPUc` | 0x80374E1C | 76 | parked, 100% |  |
| `fmod/fmod_file` | `getByte__Q24FMOD4FileFPi` | 0x80374E68 | 80 | parked, 100% |  |
| `fmod/fmod_file` | `getWord__Q24FMOD4FileFPUs` | 0x80374EB8 | 76 | parked, 100% |  |
| `fmod/fmod_file` | `getWord__Q24FMOD4FileFPUi` | 0x80374F04 | 76 | parked, 100% |  |
| `fmod/fmod_file` | `getWord__Q24FMOD4FileFPi` | 0x80374F50 | 76 | parked, 100% |  |
| `fmod/fmod_file` | `seek__Q24FMOD4FileFii` | 0x80374F9C | 464 | parked, 100% |  |
| `fmod/fmod_file` | `tell__Q24FMOD4FileFPUi` | 0x8037516C | 56 | parked, 100% |  |
| `fmod/fmod_file` | `setStartOffset__Q24FMOD4FileFUi` | 0x803751A4 | 44 | parked, 100% |  |
| `fmod/fmod_file` | `enableDoubleBuffer__Q24FMOD4FileFUiPv` | 0x803751D0 | 440 | parked, 91.1% |  |
| `fmod/fmod_file` | `setName__Q24FMOD4FileFPc` | 0x80375388 | 80 | parked, 100% |  |
| `fmod/fmod_file_disk` | `reallyOpen__Q24FMOD8DiskFileFPCcPUi` | 0x803753D8 | 204 | matched |  |
| `fmod/fmod_file_disk` | `reallyClose__Q24FMOD8DiskFileFv` | 0x803754A4 | 72 | matched |  |
| `fmod/fmod_file_disk` | `reallyRead__Q24FMOD8DiskFileFPvUiPUi` | 0x803754EC | 236 | matched |  |
| `fmod/fmod_file_disk` | `reallySeek__Q24FMOD8DiskFileFUi` | 0x803755D8 | 8 | matched |  |
| `fmod/fmod_file_disk` | `reallyCancel__Q24FMOD8DiskFileFv` | 0x803755E0 | 8 | matched |  |
| `fmod/fmod_file_memory` | `reallyOpen__Q24FMOD10MemoryFileFPCcPUi` | 0x803755E8 | 32 | matched |  |
| `fmod/fmod_file_memory` | `reallyClose__Q24FMOD10MemoryFileFv` | 0x80375608 | 8 | matched |  |
| `fmod/fmod_file_memory` | `reallyRead__Q24FMOD10MemoryFileFPvUiPUi` | 0x80375610 | 144 | matched |  |
| `fmod/fmod_file_memory` | `reallySeek__Q24FMOD10MemoryFileFUi` | 0x803756A0 | 28 | matched |  |
| `fmod/fmod_file_null` | `reallyOpen__Q24FMOD8NullFileFPCcPUi` | 0x803756BC | 20 | matched |  |
| `fmod/fmod_file_null` | `reallyClose__Q24FMOD8NullFileFv` | 0x803756D0 | 8 | matched |  |
| `fmod/fmod_file_null` | `reallyRead__Q24FMOD8NullFileFPvUiPUi` | 0x803756D8 | 56 | matched |  |
| `fmod/fmod_file_null` | `reallySeek__Q24FMOD8NullFileFUi` | 0x80375710 | 28 | matched |  |
| `fmod/fmod_file_user` | `reallyOpen__Q24FMOD8UserFileFPCcPUi` | 0x8037572C | 184 | matched |  |
| `fmod/fmod_file_user` | `reallyClose__Q24FMOD8UserFileFv` | 0x803757E4 | 96 | matched |  |
| `fmod/fmod_file_user` | `reallyRead__Q24FMOD8UserFileFPvUiPUi` | 0x80375844 | 108 | matched |  |
| `fmod/fmod_file_user` | `reallySeek__Q24FMOD8UserFileFUi` | 0x803758B0 | 116 | matched |  |
| `fmod/fmod_memory` | `alloc__Q24FMOD12MemSingletonFiPCci` | 0x80375924 | 92 | matched |  |
| `fmod/fmod_memory` | `free__Q24FMOD12MemSingletonFPCci` | 0x80375980 | 120 | matched |  |
| `fmod/fmod_memory` | `Memory_DefaultMalloc__4FMODFUiUi` | 0x803759F8 | 4 | matched |  |
| `fmod/fmod_memory` | `Memory_DefaultRealloc__4FMODFPvUiUi` | 0x803759FC | 4 | matched |  |
| `fmod/fmod_memory` | `Memory_DefaultFree__4FMODFPvUi` | 0x80375A00 | 4 | matched |  |
| `fmod/fmod_memory` | `__ct__Q24FMOD7MemPoolFv` | 0x80375A04 | 212 | matched |  |
| `fmod/fmod_memory` | `__dt__Q24FMOD7MemPoolFv` | 0x80375AD8 | 84 | matched |  |
| `fmod/fmod_memory` | `init__Q24FMOD7MemPoolFPvii` | 0x80375B2C | 356 | matched |  |
| `fmod/fmod_memory` | `close__Q24FMOD7MemPoolFv` | 0x80375C90 | 348 | matched |  |
| `fmod/fmod_memory` | `set__Q24FMOD7MemPoolFiii` | 0x80375DEC | 592 | matched |  |
| `fmod/fmod_memory` | `alloc__Q24FMOD7MemPoolFiPCciUib` | 0x8037603C | 936 | matched |  |
| `fmod/fmod_memory` | `free__Q24FMOD7MemPoolFPvPCciUi` | 0x803763E4 | 356 | matched |  |
| `fmod/fmod_memory` | `calloc__Q24FMOD7MemPoolFiPCciUi` | 0x80376548 | 8 | matched |  |
| `fmod/fmod_memory` | `realloc__Q24FMOD7MemPoolFPviPCciUi` | 0x80376550 | 1224 | matched |  |
| `fmod/fmod_string` | `FMOD_strlen` | 0x80376A18 | 32 | matched |  |
| `fmod/fmod_string` | `FMOD_strcpy` | 0x80376A38 | 36 | matched |  |
| `fmod/fmod_string` | `FMOD_strncpy` | 0x80376A5C | 48 | matched |  |
| `fmod/fmod_string` | `FMOD_strcat` | 0x80376A8C | 56 | matched |  |
| `fmod/fmod_string` | `FMOD_strncat` | 0x80376AC4 | 88 | matched |  |
| `fmod/fmod_string` | `FMOD_strupr` | 0x80376B1C | 60 | matched |  |
| `fmod/fmod_string` | `FMOD_strlwr` | 0x80376B58 | 60 | matched |  |
| `fmod/fmod_string` | `FMOD_strcmp` | 0x80376B94 | 44 | matched |  |
| `fmod/fmod_string` | `FMOD_strncmp` | 0x80376BC0 | 60 | matched |  |
| `fmod/fmod_string` | `FMOD_stricmp` | 0x80376BFC | 96 | matched |  |
| `fmod/fmod_string` | `FMOD_strnicmp` | 0x80376C5C | 112 | matched |  |
| `fmod/fmod_string` | `FMOD_memcmp` | 0x80376CCC | 56 | matched |  |
| `fmod/fmod_string` | `FMOD_memmove` | 0x80376D04 | 268 | matched |  |
| `fmod/fmod_string` | `FMOD_strdup` | 0x80376E10 | 140 | matched |  |
| `fmod/fmod_string` | `FMOD_atoi_hex` | 0x80376E9C | 144 | matched |  |
| `fmod/fmod_stringw` | `FMOD_strlenW` | 0x80376F2C | 44 | matched |  |
| `fmod/fmod_stringw` | `FMOD_strncpyW` | 0x80376F58 | 48 | matched |  |
| `fmod/fmod_stringw` | `FMOD_wtoa` | 0x80376F88 | 64 | matched |  |
| `fmod/fmod_thread` | `callback__Q24FMOD6ThreadFPv` | 0x80376FC8 | 372 | matched |  |
| `fmod/fmod_thread` | `threadFunc__Q24FMOD6ThreadFv` | 0x8037713C | 8 | matched |  |
| `fmod/fmod_thread` | `__ct__Q24FMOD6ThreadFv` | 0x80377144 | 52 | matched |  |
| `fmod/fmod_thread` | `initThread__Q24FMOD6ThreadFPCcPFPv_vPvQ34FMOD6Thread8PRIORITYPvibiPQ24FMOD7SystemI` | 0x80377178 | 376 | matched |  |
| `fmod/fmod_thread` | `closeThread__Q24FMOD6ThreadFv` | 0x803772F0 | 276 | matched |  |
| `fmod/fmod_thread` | `wakeupThread__Q24FMOD6ThreadFb` | 0x80377404 | 24 | matched |  |
| `fmod/fmod_time` | `__ct__Q24FMOD9TimeStampFv` | 0x8037741C | 48 | matched |  |
| `fmod/fmod_time` | `stampIn__Q24FMOD9TimeStampFv` | 0x8037744C | 56 | matched |  |
| `fmod/fmod_time` | `stampOut__Q24FMOD9TimeStampFi` | 0x80377484 | 328 | matched |  |
| `fmod/fmod_time` | `setPaused__Q24FMOD9TimeStampFb` | 0x803775CC | 180 | matched |  |
| `fmod/dlmalloc` | `init_mparams__4FMODFv` | 0x80377680 | 156 | matched |  |
| `fmod/dlmalloc` | `sys_trim__4FMODFPQ24FMOD12malloc_stateUl` | 0x8037771C | 100 | matched |  |
| `fmod/dlmalloc` | `tmalloc_large__4FMODFPQ24FMOD12malloc_stateUl` | 0x80377780 | 1372 | matched |  |
| `fmod/dlmalloc` | `tmalloc_small__4FMODFPQ24FMOD12malloc_stateUl` | 0x80377CDC | 704 | matched |  |
| `fmod/dlmalloc` | `internal_realloc__4FMODFPQ24FMOD12malloc_statePvUl` | 0x80377F9C | 448 | matched |  |
| `fmod/dlmalloc` | `create_mspace_with_base__4FMODFPvUli` | 0x8037815C | 552 | matched |  |
| `fmod/dlmalloc` | `mspace_malloc__4FMODFPvUl` | 0x80378384 | 844 | matched |  |
| `fmod/dlmalloc` | `mspace_free__4FMODFPvPv` | 0x803786D0 | 1756 | matched |  |
| `fmod/dlmalloc` | `mspace_realloc__4FMODFPvPvUl` | 0x80378DAC | 20 | matched |  |
| `fmod/fmod_channel` | `stop__Q24FMOD7ChannelFv` | 0x80378DC0 | 56 | matched |  |
| `fmod/fmod_channel` | `setPaused__Q24FMOD7ChannelFb` | 0x80378DF8 | 72 | matched |  |
| `fmod/fmod_channel` | `getPaused__Q24FMOD7ChannelFPb` | 0x80378E40 | 88 | matched |  |
| `fmod/fmod_channel` | `setVolume__Q24FMOD7ChannelFf` | 0x80378E98 | 76 | matched |  |
| `fmod/fmod_channel` | `getVolume__Q24FMOD7ChannelFPf` | 0x80378EE4 | 88 | matched |  |
| `fmod/fmod_channel` | `setFrequency__Q24FMOD7ChannelFf` | 0x80378F3C | 72 | matched |  |
| `fmod/fmod_channel` | `setPan__Q24FMOD7ChannelFf` | 0x80378F84 | 76 | matched |  |
| `fmod/fmod_channel` | `setDelay__Q24FMOD7ChannelF14FMOD_DELAYTYPEUiUi` | 0x80378FD0 | 104 | matched |  |
| `fmod/fmod_channel` | `getDelay__Q24FMOD7ChannelF14FMOD_DELAYTYPEPUiPUi` | 0x80379038 | 136 | matched |  |
| `fmod/fmod_channel` | `setSpeakerMix__Q24FMOD7ChannelFffffffff` | 0x803790C0 | 140 | matched |  |
| `fmod/fmod_channel` | `setSpeakerLevels__Q24FMOD7ChannelF12FMOD_SPEAKERPfi` | 0x8037914C | 108 | matched |  |
| `fmod/fmod_channel` | `setInputChannelMix__Q24FMOD7ChannelFPfi` | 0x803791B8 | 88 | matched |  |
| `fmod/fmod_channel` | `setMute__Q24FMOD7ChannelFb` | 0x80379210 | 72 | matched |  |
| `fmod/fmod_channel` | `setPriority__Q24FMOD7ChannelFi` | 0x80379258 | 72 | matched |  |
| `fmod/fmod_channel` | `setPosition__Q24FMOD7ChannelFUiUi` | 0x803792A0 | 88 | matched |  |
| `fmod/fmod_channel` | `getPosition__Q24FMOD7ChannelFPUiUi` | 0x803792F8 | 104 | matched |  |
| `fmod/fmod_channel` | `setReverbProperties__Q24FMOD7ChannelFPC29FMOD_REVERB_CHANNELPROPERTIES` | 0x80379360 | 72 | matched |  |
| `fmod/fmod_channel` | `getReverbProperties__Q24FMOD7ChannelFP29FMOD_REVERB_CHANNELPROPERTIES` | 0x803793A8 | 72 | matched |  |
| `fmod/fmod_channel` | `setChannelGroup__Q24FMOD7ChannelFPQ24FMOD12ChannelGroup` | 0x803793F0 | 72 | matched |  |
| `fmod/fmod_channel` | `setCallback__Q24FMOD7ChannelFPFP12FMOD_CHANNEL25FMOD_CHANNEL_CALLBACKTYPEPvPv_11FMOD_RESULT` | 0x80379438 | 72 | matched |  |
| `fmod/fmod_channel` | `set3DAttributes__Q24FMOD7ChannelFPC11FMOD_VECTORPC11FMOD_VECTOR` | 0x80379480 | 88 | matched |  |
| `fmod/fmod_channel` | `get3DAttributes__Q24FMOD7ChannelFP11FMOD_VECTORP11FMOD_VECTOR` | 0x803794D8 | 136 | matched |  |
| `fmod/fmod_channel` | `set3DMinMaxDistance__Q24FMOD7ChannelFff` | 0x80379560 | 88 | matched |  |
| `fmod/fmod_channel` | `set3DConeOrientation__Q24FMOD7ChannelFP11FMOD_VECTOR` | 0x803795B8 | 72 | matched |  |
| `fmod/fmod_channel` | `set3DOcclusion__Q24FMOD7ChannelFff` | 0x80379600 | 88 | matched |  |
| `fmod/fmod_channel` | `set3DSpread__Q24FMOD7ChannelFf` | 0x80379658 | 72 | matched |  |
| `fmod/fmod_channel` | `set3DPanLevel__Q24FMOD7ChannelFf` | 0x803796A0 | 72 | matched |  |
| `fmod/fmod_channel` | `set3DDopplerLevel__Q24FMOD7ChannelFf` | 0x803796E8 | 72 | matched |  |
| `fmod/fmod_channel` | `isPlaying__Q24FMOD7ChannelFPb` | 0x80379730 | 88 | matched |  |
| `fmod/fmod_channel` | `getAudibility__Q24FMOD7ChannelFPf` | 0x80379788 | 88 | matched |  |
| `fmod/fmod_channel` | `getCurrentSound__Q24FMOD7ChannelFPPQ24FMOD5Sound` | 0x803797E0 | 88 | matched |  |
| `fmod/fmod_channel` | `setMode__Q24FMOD7ChannelFUi` | 0x80379838 | 72 | matched |  |
| `fmod/fmod_channel` | `getMode__Q24FMOD7ChannelFPUi` | 0x80379880 | 88 | matched |  |
| `fmod/fmod_channel` | `setUserData__Q24FMOD7ChannelFPv` | 0x803798D8 | 72 | matched |  |
| `fmod/fmod_channel` | `getUserData__Q24FMOD7ChannelFPPv` | 0x80379920 | 88 | matched |  |
| `fmod/fmod_channel` | `getMemoryInfo__Q24FMOD7ChannelFUiUiPUiPUi` | 0x80379978 | 152 | matched |  |
| `fmod/fmod_dsp` | `release__Q24FMOD3DSPFv` | 0x80379A10 | 72 | matched |  |
| `fmod/fmod_dsp` | `disconnectFrom__Q24FMOD3DSPFPQ24FMOD3DSP` | 0x80379A58 | 76 | matched |  |
| `fmod/fmod_dsp` | `disconnectAll__Q24FMOD3DSPFbb` | 0x80379AA4 | 88 | matched |  |
| `fmod/fmod_dsp` | `setActive__Q24FMOD3DSPFb` | 0x80379AFC | 104 | matched |  |
| `fmod/fmod_dsp` | `setBypass__Q24FMOD3DSPFb` | 0x80379B64 | 104 | matched |  |
| `fmod/fmod_dsp` | `setParameter__Q24FMOD3DSPFif` | 0x80379BCC | 100 | matched |  |
| `fmod/fmod_dsp` | `getParameterInfo__Q24FMOD3DSPFiPcPcPciPfPf` | 0x80379C30 | 132 | matched |  |
| `fmod/fmod_dsp` | `getType__Q24FMOD3DSPFP13FMOD_DSP_TYPE` | 0x80379CB4 | 84 | matched |  |
| `fmod/fmod_dsp` | `setDefaults__Q24FMOD3DSPFfffi` | 0x80379D08 | 132 | matched |  |
| `fmod/fmod_dsp` | `getDefaults__Q24FMOD3DSPFPfPfPfPi` | 0x80379D8C | 132 | matched |  |
| `fmod/fmod_output` | `__ct__Q24FMOD6OutputFv` | 0x80379E10 | 120 | matched |  |
| `fmod/fmod_output` | `release__Q24FMOD6OutputFv` | 0x80379E88 | 132 | matched |  |
| `fmod/fmod_output` | `mix__Q24FMOD6OutputFPvUi` | 0x80379F0C | 1132 | matched |  |
| `fmod/fmod_output` | `getFreeChannel__Q24FMOD6OutputFUiPPQ24FMOD11ChannelRealiiPib` | 0x8037A378 | 160 | matched |  |
| `fmod/fmod_output` | `mixCallback__Q24FMOD6OutputFP17FMOD_OUTPUT_STATEPvUi` | 0x8037A418 | 16 | matched |  |
| `fmod/fmod_sound` | `release__Q24FMOD5SoundFv` | 0x8037A428 | 72 | matched |  |
| `fmod/fmod_sound` | `setDefaults__Q24FMOD5SoundFfffi` | 0x8037A470 | 160 | matched |  |
| `fmod/fmod_sound` | `getDefaults__Q24FMOD5SoundFPfPfPfPi` | 0x8037A510 | 160 | matched |  |
| `fmod/fmod_sound` | `setSubSound__Q24FMOD5SoundFiPQ24FMOD5Sound` | 0x8037A5B0 | 128 | matched |  |
| `fmod/fmod_sound` | `getSubSound__Q24FMOD5SoundFiPPQ24FMOD5Sound` | 0x8037A630 | 128 | matched |  |
| `fmod/fmod_sound` | `getLength__Q24FMOD5SoundFPUiUi` | 0x8037A6B0 | 128 | matched |  |
| `fmod/fmod_sound` | `getFormat__Q24FMOD5SoundFP15FMOD_SOUND_TYPEP17FMOD_SOUND_FORMATPiPi` | 0x8037A730 | 160 | matched |  |
| `fmod/fmod_sound` | `getNumSubSounds__Q24FMOD5SoundFPi` | 0x8037A7D0 | 112 | matched |  |
| `fmod/fmod_sound` | `getOpenState__Q24FMOD5SoundFP14FMOD_OPENSTATEPUiPb` | 0x8037A840 | 116 | matched |  |
| `fmod/fmod_sound` | `getNumSyncPoints__Q24FMOD5SoundFPi` | 0x8037A8B4 | 112 | matched |  |
| `fmod/fmod_sound` | `getSyncPoint__Q24FMOD5SoundFiPP14FMOD_SYNCPOINT` | 0x8037A924 | 128 | matched |  |
| `fmod/fmod_sound` | `getSyncPointInfo__Q24FMOD5SoundFP14FMOD_SYNCPOINTPciPUiUi` | 0x8037A9A4 | 144 | matched |  |
| `fmod/fmod_sound` | `setMode__Q24FMOD5SoundFUi` | 0x8037AA34 | 112 | matched |  |
| `fmod/fmod_sound` | `getMode__Q24FMOD5SoundFPUi` | 0x8037AAA4 | 112 | matched |  |
| `fmod/fmod_sound` | `setLoopCount__Q24FMOD5SoundFi` | 0x8037AB14 | 112 | matched |  |
| `fmod/fmod_sound` | `getMusicNumChannels__Q24FMOD5SoundFPi` | 0x8037AB84 | 112 | matched |  |
| `fmod/fmod_sound` | `setMusicChannelVolume__Q24FMOD5SoundFif` | 0x8037ABF4 | 128 | matched |  |
| `fmod/fmod_sound` | `getMemoryInfo__Q24FMOD5SoundFUiUiPUiPUi` | 0x8037AC74 | 160 | matched |  |
| `fmod/fmod_system` | `release__Q24FMOD6SystemFv` | 0x8037AD14 | 56 | matched |  |
| `fmod/fmod_system` | `getSoftwareFormat__Q24FMOD6SystemFPiP17FMOD_SOUND_FORMATPiPiP18FMOD_DSP_RESAMPLERPi` | 0x8037AD4C | 344 | matched |  |
| `fmod/fmod_system` | `getDSPBufferSize__Q24FMOD6SystemFPUiPi` | 0x8037AEA4 | 88 | matched |  |
| `fmod/fmod_system` | `setFileSystem__Q24FMOD6SystemFPFPCciPUiPPvPPv_11FMOD_RESULTPFPvPv_11FMOD_RESULTPFPvPvUiPUiPv_11FMOD_RESULTPFPvUiPv_11FMOD_RESULTi` | 0x8037AEFC | 104 | matched |  |
| `fmod/fmod_system` | `setAdvancedSettings__Q24FMOD6SystemFP21FMOD_ADVANCEDSETTINGS` | 0x8037AF64 | 72 | matched |  |
| `fmod/fmod_system` | `getAdvancedSettings__Q24FMOD6SystemFP21FMOD_ADVANCEDSETTINGS` | 0x8037AFAC | 72 | matched |  |
| `fmod/fmod_system` | `setSpeakerMode__Q24FMOD6SystemF16FMOD_SPEAKERMODE` | 0x8037AFF4 | 72 | matched |  |
| `fmod/fmod_system` | `getSpeakerMode__Q24FMOD6SystemFP16FMOD_SPEAKERMODE` | 0x8037B03C | 96 | matched |  |
| `fmod/fmod_system` | `getNumPlugins__Q24FMOD6SystemF15FMOD_PLUGINTYPEPi` | 0x8037B09C | 88 | matched |  |
| `fmod/fmod_system` | `getPluginHandle__Q24FMOD6SystemF15FMOD_PLUGINTYPEiPUi` | 0x8037B0F4 | 104 | matched |  |
| `fmod/fmod_system` | `getPluginInfo__Q24FMOD6SystemFUiP15FMOD_PLUGINTYPEPciPUi` | 0x8037B15C | 104 | matched |  |
| `fmod/fmod_system` | `createDSPByPlugin__Q24FMOD6SystemFUiPPQ24FMOD3DSP` | 0x8037B1C4 | 88 | matched |  |
| `fmod/fmod_system` | `init__Q24FMOD6SystemFiUiPv` | 0x8037B21C | 104 | matched |  |
| `fmod/fmod_system` | `update__Q24FMOD6SystemFv` | 0x8037B284 | 56 | matched |  |
| `fmod/fmod_system` | `set3DSettings__Q24FMOD6SystemFfff` | 0x8037B2BC | 104 | matched |  |
| `fmod/fmod_system` | `get3DSettings__Q24FMOD6SystemFPfPfPf` | 0x8037B324 | 104 | matched |  |
| `fmod/fmod_system` | `set3DNumListeners__Q24FMOD6SystemFi` | 0x8037B38C | 72 | matched |  |
| `fmod/fmod_system` | `get3DNumListeners__Q24FMOD6SystemFPi` | 0x8037B3D4 | 72 | matched |  |
| `fmod/fmod_system` | `set3DListenerAttributes__Q24FMOD6SystemFiPC11FMOD_VECTORPC11FMOD_VECTORPC11FMOD_VECTORPC11FMOD_VECTOR` | 0x8037B41C | 104 | matched |  |
| `fmod/fmod_system` | `get3DListenerAttributes__Q24FMOD6SystemFiP11FMOD_VECTORP11FMOD_VECTORP11FMOD_VECTORP11FMOD_VECTOR` | 0x8037B484 | 104 | matched |  |
| `fmod/fmod_system` | `getStreamBufferSize__Q24FMOD6SystemFPUiPUi` | 0x8037B4EC | 88 | matched |  |
| `fmod/fmod_system` | `getVersion__Q24FMOD6SystemFPUi` | 0x8037B544 | 72 | matched |  |
| `fmod/fmod_system` | `getHardwareChannels__Q24FMOD6SystemFPiPiPi` | 0x8037B58C | 104 | matched |  |
| `fmod/fmod_system` | `getSoundRAM__Q24FMOD6SystemFPiPiPi` | 0x8037B5F4 | 104 | matched |  |
| `fmod/fmod_system` | `createSound__Q24FMOD6SystemFPCcUiP22FMOD_CREATESOUNDEXINFOPPQ24FMOD5Sound` | 0x8037B65C | 120 | matched |  |
| `fmod/fmod_system` | `createStream__Q24FMOD6SystemFPCcUiP22FMOD_CREATESOUNDEXINFOPPQ24FMOD5Sound` | 0x8037B6D4 | 120 | matched |  |
| `fmod/fmod_system` | `createDSPByType__Q24FMOD6SystemF13FMOD_DSP_TYPEPPQ24FMOD3DSP` | 0x8037B74C | 88 | matched |  |
| `fmod/fmod_system` | `createChannelGroup__Q24FMOD6SystemFPCcPPQ24FMOD12ChannelGroup` | 0x8037B7A4 | 88 | matched |  |
| `fmod/fmod_system` | `createReverb__Q24FMOD6SystemFPPQ24FMOD6Reverb` | 0x8037B7FC | 72 | matched |  |
| `fmod/fmod_system` | `playSound__Q24FMOD6SystemF17FMOD_CHANNELINDEXPQ24FMOD5SoundbPPQ24FMOD7Channel` | 0x8037B844 | 120 | matched |  |
| `fmod/fmod_system` | `playDSP__Q24FMOD6SystemF17FMOD_CHANNELINDEXPQ24FMOD3DSPbPPQ24FMOD7Channel` | 0x8037B8BC | 120 | matched |  |
| `fmod/fmod_system` | `getMasterChannelGroup__Q24FMOD6SystemFPPQ24FMOD12ChannelGroup` | 0x8037B934 | 72 | matched |  |
| `fmod/fmod_system` | `getMasterSoundGroup__Q24FMOD6SystemFPPQ24FMOD10SoundGroup` | 0x8037B97C | 72 | matched |  |
| `fmod/fmod_system` | `setReverbProperties__Q24FMOD6SystemFPC22FMOD_REVERB_PROPERTIES` | 0x8037B9C4 | 76 | matched |  |
| `fmod/fmod_system` | `getReverbProperties__Q24FMOD6SystemFP22FMOD_REVERB_PROPERTIES` | 0x8037BA10 | 72 | matched |  |
| `fmod/fmod_system` | `setReverbAmbientProperties__Q24FMOD6SystemFP22FMOD_REVERB_PROPERTIES` | 0x8037BA58 | 56 | matched |  |
| `fmod/fmod_system` | `getReverbAmbientProperties__Q24FMOD6SystemFP22FMOD_REVERB_PROPERTIES` | 0x8037BA90 | 56 | matched |  |
| `fmod/fmod_system` | `getDSPClock__Q24FMOD6SystemFPUiPUi` | 0x8037BAC8 | 88 | matched |  |
| `fmod/fmod_system` | `setUserData__Q24FMOD6SystemFPv` | 0x8037BB20 | 72 | matched |  |
| `fmod/fmod_buckethash` | `__dt__Q24FMOD10BucketHashFv` | 0x8037BB68 | 128 | matched |  |
| `fmod/fmod_buckethash` | `init__Q24FMOD10BucketHashFUlPFPCvPv_bPFPCv_Ul` | 0x8037BBE8 | 188 | matched |  |
| `fmod/fmod_buckethash` | `release__Q24FMOD10BucketHashFv` | 0x8037BCA4 | 92 | matched |  |
| `fmod/fmod_buckethash` | `moveTo__Q24FMOD10BucketHashFPQ24FMOD10BucketHash` | 0x8037BD00 | 152 | matched |  |
| `fmod/fmod_buckethash` | `insert__Q24FMOD10BucketHashFPCvPv` | 0x8037BD98 | 176 | matched |  |
| `fmod/fmod_buckethash` | `find__Q24FMOD10BucketHashCFPCv` | 0x8037BE48 | 196 | matched |  |
| `fmod/fmod_buckethash` | `hash_uint__Q24FMOD10BucketHashFPCv` | 0x8037BF0C | 8 | matched |  |
| `fmod/fmod_compositionconditions` | `release__Q24FMOD17RefCountConditionFv` | 0x8037BF14 | 32 | matched |  |
| `fmod/fmod_compositionconditions` | `clone__Q24FMOD17RefCountConditionCFv` | 0x8037BF34 | 16 | matched |  |
| `fmod/fmod_compositionconditions` | `destroy__Q24FMOD17RefCountConditionFv` | 0x8037BF44 | 36 | matched |  |
| `fmod/fmod_compositionconditions` | `__ct__Q24FMOD18ParameterConditionFv` | 0x8037BF68 | 48 | matched |  |
| `fmod/fmod_compositionconditions` | `init__Q24FMOD18ParameterConditionFUi` | 0x8037BF98 | 12 | matched |  |
| `fmod/fmod_compositionconditions` | `evaluate__Q24FMOD18ParameterConditionCFRCQ24FMOD10MusicStateRb` | 0x8037BFA4 | 592 | matched |  |
| `fmod/fmod_compositionconditions` | `__ct__Q24FMOD19MusicStateConditionFv` | 0x8037C1F4 | 36 | matched |  |
| `fmod/fmod_compositionconditions` | `evaluate__Q24FMOD19MusicStateConditionCFRCQ24FMOD10MusicStateRb` | 0x8037C218 | 196 | matched |  |
| `fmod/fmod_compositionconditions` | `__ct__Q24FMOD17ConstantConditionFv` | 0x8037C2DC | 28 | matched |  |
| `fmod/fmod_compositionconditions` | `evaluate__Q24FMOD17ConstantConditionCFRCQ24FMOD10MusicStateRb` | 0x8037C2F8 | 16 | matched |  |
| `fmod/fmod_compositionconditions` | `type__Q24FMOD17ConstantConditionCFv` | 0x8037C308 | 8 | matched |  |
| `fmod/fmod_compositionconditions` | `type__Q24FMOD19MusicStateConditionCFv` | 0x8037C310 | 8 | matched |  |
| `fmod/fmod_compositionconditions` | `type__Q24FMOD18ParameterConditionCFv` | 0x8037C318 | 8 | matched |  |
| `fmod/fmod_compositionconditions` | `__dt__Q24FMOD18ParameterConditionFv` | 0x8037C320 | 64 | matched |  |
| `fmod/fmod_compositionconditions` | `__dt__Q24FMOD19MusicStateConditionFv` | 0x8037C360 | 64 | matched |  |
| `fmod/fmod_compositionconditions` | `__dt__Q24FMOD17ConstantConditionFv` | 0x8037C3A0 | 64 | matched |  |
| `fmod/fmod_event` | `FMOD_EventSystem_Create` | 0x80384A4C | 8 | matched |  |
| `fmod/fmod_eventcategoryi` | `__ct__Q24FMOD14EventCategoryIFv` | 0x80384A54 | 112 | matched |  |
| `fmod/fmod_eventcategoryi` | `__dt__Q24FMOD13EventCategoryFv` | 0x80384AC4 | 64 | matched |  |
| `fmod/fmod_eventcategoryi` | `init__Q24FMOD14EventCategoryIFv` | 0x80384B04 | 88 | matched |  |
| `fmod/fmod_eventcategoryi` | `release__Q24FMOD14EventCategoryIFv` | 0x80384B5C | 244 | matched |  |
| `fmod/fmod_eventcategoryi` | `load__Q24FMOD14EventCategoryIFPQ24FMOD4FileUiP19FMOD_EVENT_LOADINFOPQ24FMOD14EventCategoryIPCc` | 0x80384C50 | 1300 | matched |  |
| `fmod/fmod_eventcategoryi` | `mergeCategory__Q24FMOD14EventCategoryIFPQ24FMOD14EventCategoryIb` | 0x80385164 | 440 | matched |  |
| `fmod/fmod_eventcategoryi` | `createDSPNetwork__Q24FMOD14EventCategoryIFPQ24FMOD14EventCategoryI` | 0x8038531C | 444 | matched |  |
| `fmod/fmod_eventcategoryi` | `releaseDSPNetwork__Q24FMOD14EventCategoryIFv` | 0x803854D8 | 196 | matched |  |
| `fmod/fmod_eventcategoryi` | `addCategory__Q24FMOD14EventCategoryIFPQ24FMOD14EventCategoryI` | 0x8038559C | 636 | matched |  |
| `fmod/fmod_eventcategoryi` | `stealEvent__Q24FMOD14EventCategoryIFPQ24FMOD6EventI` | 0x80385818 | 492 | matched |  |
| `fmod/fmod_eventcategoryi` | `getInfo__Q24FMOD14EventCategoryIFPiPPc` | 0x80385A04 | 40 | matched |  |
| `fmod/fmod_eventcategoryi` | `getCategory__Q24FMOD14EventCategoryIFPCcPPQ24FMOD13EventCategory` | 0x80385A2C | 352 | matched |  |
| `fmod/fmod_eventcategoryi` | `getCategoryByIndex__Q24FMOD14EventCategoryIFiPPQ24FMOD13EventCategory` | 0x80385B8C | 120 | matched |  |
| `fmod/fmod_eventcategoryi` | `getNumCategories__Q24FMOD14EventCategoryIFPi` | 0x80385C04 | 76 | matched |  |
| `fmod/fmod_eventcategoryi` | `getEventByIndex__Q24FMOD14EventCategoryIFiUiPPQ24FMOD5Event` | 0x80385C50 | 196 | matched |  |
| `fmod/fmod_eventcategoryi` | `getNumEvents__Q24FMOD14EventCategoryIFPi` | 0x80385D14 | 60 | matched |  |
| `fmod/fmod_eventcategoryi` | `stopAllEvents__Q24FMOD14EventCategoryIFv` | 0x80385D50 | 388 | matched |  |
| `fmod/fmod_eventcategoryi` | `setVolume__Q24FMOD14EventCategoryIFf` | 0x80385ED4 | 104 | matched |  |
| `fmod/fmod_eventcategoryi` | `getVolume__Q24FMOD14EventCategoryIFPf` | 0x80385F3C | 32 | matched |  |
| `fmod/fmod_eventcategoryi` | `setPitch__Q24FMOD14EventCategoryIFf21FMOD_EVENT_PITCHUNITS` | 0x80385F5C | 160 | matched |  |
| `fmod/fmod_eventcategoryi` | `getPitch__Q24FMOD14EventCategoryIFPf21FMOD_EVENT_PITCHUNITS` | 0x80385FFC | 104 | matched |  |
| `fmod/fmod_eventcategoryi` | `setPaused__Q24FMOD14EventCategoryIFb` | 0x80386064 | 80 | matched |  |
| `fmod/fmod_eventcategoryi` | `getPaused__Q24FMOD14EventCategoryIFPb` | 0x803860B4 | 32 | matched |  |
| `fmod/fmod_eventcategoryi` | `setMute__Q24FMOD14EventCategoryIFb` | 0x803860D4 | 80 | matched |  |
| `fmod/fmod_eventcategoryi` | `getMute__Q24FMOD14EventCategoryIFPb` | 0x80386124 | 32 | matched |  |
| `fmod/fmod_eventcategoryi` | `getChannelGroup__Q24FMOD14EventCategoryIFPPQ24FMOD12ChannelGroup` | 0x80386144 | 32 | matched |  |
| `fmod/fmod_eventcategoryi` | `setUserData__Q24FMOD14EventCategoryIFPv` | 0x80386164 | 12 | matched |  |
| `fmod/fmod_eventcategoryi` | `getMemoryInfo__Q24FMOD14EventCategoryIFUiUiPUiPUi` | 0x80386170 | 8 | matched |  |
| `fmod/fmod_eventcategoryi` | `getUserData__Q24FMOD14EventCategoryIFPPv` | 0x80386178 | 32 | matched |  |
| `fmod/fmod_eventcategoryi` | `__dt__Q24FMOD14EventCategoryIFv` | 0x80386198 | 64 | matched |  |
| `fmod/fmod_eventenvelope` | `__ct__Q24FMOD18EventEnvelopePointFv` | 0x803861D8 | 40 | matched |  |
| `fmod/fmod_eventenvelope` | `__ct__Q24FMOD13EventEnvelopeFv` | 0x80386200 | 40 | matched |  |
| `fmod/fmod_eventenvelope` | `init__Q24FMOD13EventEnvelopeFPQ24FMOD16EventEnvelopeDef` | 0x80386228 | 32 | matched |  |
| `fmod/fmod_eventenvelope` | `release__Q24FMOD13EventEnvelopeFv` | 0x80386248 | 304 | matched |  |
| `fmod/fmod_eventenvelope` | `__ct__Q24FMOD16EventEnvelopeDefFv` | 0x80386378 | 84 | matched |  |
| `fmod/fmod_eventenvelope` | `init__Q24FMOD16EventEnvelopeDefFPQ24FMOD6System` | 0x803863CC | 72 | matched |  |
| `fmod/fmod_eventenvelope` | `getValue__Q24FMOD16EventEnvelopeDefFfPf` | 0x80386414 | 832 | matched |  |
| `fmod/fmod_eventgroupi` | `__ct__Q24FMOD11EventGroupIFv` | 0x80386754 | 112 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `__dt__Q24FMOD10EventGroupFv` | 0x803867C4 | 64 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `init__Q24FMOD11EventGroupIFv` | 0x80386804 | 72 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getEventPtr__Q24FMOD11EventGroupIFPCcPPQ24FMOD6EventIPPQ24FMOD11EventGroupI` | 0x8038684C | 588 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `addGroup__Q24FMOD11EventGroupIFPQ24FMOD11EventGroupI` | 0x80386A98 | 420 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `buildSoundBankList__Q24FMOD11EventGroupIFv` | 0x80386C3C | 876 | parked, 99.2% |  |
| `fmod/fmod_eventgroupi` | `loadEventData__Q24FMOD11EventGroupIF19FMOD_EVENT_RESOURCEUi` | 0x80386FA8 | 8 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `loadEventDataInternal__Q24FMOD11EventGroupIF19FMOD_EVENT_RESOURCEUiPQ24FMOD6EventI` | 0x80386FB0 | 992 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `adjustLoadCount__Q24FMOD11EventGroupIFi` | 0x80387390 | 52 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `freeEventData__Q24FMOD11EventGroupIFPQ24FMOD5Eventb` | 0x803873C4 | 140 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `freeEventDataInternal__Q24FMOD11EventGroupIFPQ24FMOD6EventIbb` | 0x80387450 | 532 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `freeInstanceData__Q24FMOD11EventGroupIFPQ24FMOD6EventIb` | 0x80387664 | 252 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `freeSampleData__Q24FMOD11EventGroupIFPQ24FMOD6EventI` | 0x80387760 | 252 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `release__Q24FMOD11EventGroupIFb` | 0x8038785C | 732 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getInfo__Q24FMOD11EventGroupIFPiPPc` | 0x80387B38 | 40 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getGroup__Q24FMOD11EventGroupIFPCcbPPQ24FMOD10EventGroup` | 0x80387B60 | 408 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getGroupByIndex__Q24FMOD11EventGroupIFibPPQ24FMOD10EventGroup` | 0x80387CF8 | 244 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getParentGroup__Q24FMOD11EventGroupIFPPQ24FMOD10EventGroup` | 0x80387DEC | 32 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getParentProject__Q24FMOD11EventGroupIFPPQ24FMOD12EventProject` | 0x80387E0C | 32 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getNumGroups__Q24FMOD11EventGroupIFPi` | 0x80387E2C | 76 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getEvent__Q24FMOD11EventGroupIFPCcUiPPQ24FMOD5Event` | 0x80387E78 | 168 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getEventByIndex__Q24FMOD11EventGroupIFiUiPPQ24FMOD5Event` | 0x80387F20 | 204 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getNumEvents__Q24FMOD11EventGroupIFPi` | 0x80387FEC | 60 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getProperty__Q24FMOD11EventGroupIFPCcPv` | 0x80388028 | 204 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getPropertyByIndex__Q24FMOD11EventGroupIFiPv` | 0x803880F4 | 172 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getNumProperties__Q24FMOD11EventGroupIFPi` | 0x803881A0 | 60 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `duplicateEvent__Q24FMOD11EventGroupIFPQ24FMOD6EventIPPQ24FMOD6EventI` | 0x803881DC | 464 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `createInstances__Q24FMOD11EventGroupIFPQ24FMOD6EventI` | 0x803883AC | 264 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `stealEventInstance__Q24FMOD11EventGroupIFPQ24FMOD6EventIPPQ24FMOD6EventIb` | 0x803884B4 | 1164 | parked, 95.8% |  |
| `fmod/fmod_eventgroupi` | `getEventInstance__Q24FMOD11EventGroupIFPQ24FMOD6EventIPPQ24FMOD5EventUi` | 0x80388940 | 832 | parked, 99.3% |  |
| `fmod/fmod_eventgroupi` | `load__Q24FMOD11EventGroupIFPQ24FMOD4FileUiP19FMOD_EVENT_LOADINFO` | 0x80388C80 | 4360 | parked, 98.5% |  |
| `fmod/fmod_eventgroupi` | `getState__Q24FMOD11EventGroupIFPUi` | 0x80389D88 | 284 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `setUserData__Q24FMOD11EventGroupIFPv` | 0x80389EA4 | 12 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getUserData__Q24FMOD11EventGroupIFPPv` | 0x80389EB0 | 32 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `getMemoryInfo__Q24FMOD11EventGroupIFUiUiPUiPUi` | 0x80389ED0 | 8 | parked, 100% |  |
| `fmod/fmod_eventgroupi` | `__dt__Q24FMOD11EventGroupIFv` | 0x80389ED8 | 64 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `__ct__Q24FMOD15EventImplSimpleFv` | 0x80399C30 | 32 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `release__Q24FMOD15EventImplSimpleFv` | 0x80399C50 | 100 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `releaseData__Q24FMOD15EventImplSimpleFv` | 0x80399CB4 | 72 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `unloadAllSounds__Q24FMOD15EventImplSimpleFv` | 0x80399CFC | 152 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `updateTimeParameters__Q24FMOD15EventImplSimpleFUi` | 0x80399D94 | 28 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `update__Q24FMOD15EventImplSimpleFUiUi` | 0x80399DB0 | 2452 | parked, 99.9% |  |
| `fmod/fmod_eventimpl_simple` | `updateSound__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundUi` | 0x8039A744 | 612 | parked, 99.9% |  |
| `fmod/fmod_eventimpl_simple` | `createDSPNetwork__Q24FMOD15EventImplSimpleFv` | 0x8039A9A8 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `releaseDSPNetwork__Q24FMOD15EventImplSimpleFv` | 0x8039A9B0 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `createStreams__Q24FMOD15EventImplSimpleFUi` | 0x8039A9B8 | 356 | parked, 99.9% |  |
| `fmod/fmod_eventimpl_simple` | `stop__Q24FMOD15EventImplSimpleFv` | 0x8039AB1C | 216 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `buildSoundBankList__Q24FMOD15EventImplSimpleFv` | 0x8039ABF4 | 2004 | parked, 98.7% |  |
| `fmod/fmod_eventimpl_simple` | `updateEnvelopes__Q24FMOD15EventImplSimpleFPv` | 0x8039B3C8 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getLengthMs__Q24FMOD15EventImplSimpleFPi` | 0x8039B3D0 | 160 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getPositionMs__Q24FMOD15EventImplSimpleFPUi` | 0x8039B470 | 260 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getChannelsPlaying__Q24FMOD15EventImplSimpleFPi` | 0x8039B574 | 108 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getAudibility__Q24FMOD15EventImplSimpleFPf` | 0x8039B5E0 | 244 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `estimateAudibility__Q24FMOD15EventImplSimpleFPf` | 0x8039B6D4 | 40 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `start__Q24FMOD15EventImplSimpleFv` | 0x8039B6FC | 112 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getParameter__Q24FMOD15EventImplSimpleFPCcPPQ24FMOD14EventParameter` | 0x8039B76C | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getParameterByIndex__Q24FMOD15EventImplSimpleFiPPQ24FMOD14EventParameter` | 0x8039B774 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getParameterWithDef__Q24FMOD15EventImplSimpleFPQ24FMOD17EventParameterDefPPQ24FMOD15EventParameterI` | 0x8039B77C | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getNumParameters__Q24FMOD15EventImplSimpleFPi` | 0x8039B784 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setMode__Q24FMOD15EventImplSimpleFv` | 0x8039B794 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setMinMaxDistance__Q24FMOD15EventImplSimpleFv` | 0x8039B7A4 | 20 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setReverbWetLevelDB__Q24FMOD15EventImplSimpleFv` | 0x8039B7B8 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setReverbDryLevelDB__Q24FMOD15EventImplSimpleFv` | 0x8039B7C8 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getProperty__Q24FMOD15EventImplSimpleFPCcPv` | 0x8039B7D8 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getPropertyByIndex__Q24FMOD15EventImplSimpleFiPv` | 0x8039B7E0 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setProperty__Q24FMOD15EventImplSimpleFPCcPv` | 0x8039B7E8 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setPropertyByIndex__Q24FMOD15EventImplSimpleFiPv` | 0x8039B7F0 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getNumProperties__Q24FMOD15EventImplSimpleFPi` | 0x8039B7F8 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getPropertyInfo__Q24FMOD15EventImplSimpleFPiPPcP23FMOD_EVENTPROPERTY_TYPE` | 0x8039B808 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `set3DAttributes__Q24FMOD15EventImplSimpleFv` | 0x8039B810 | 100 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setReverbProperties__Q24FMOD15EventImplSimpleFv` | 0x8039B874 | 508 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getState__Q24FMOD15EventImplSimpleFPUi` | 0x8039BA70 | 476 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `willTerminate__Q24FMOD15EventImplSimpleCFPi` | 0x8039BC4C | 40 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getParameterByIndexInternal__Q24FMOD15EventImplSimpleFiPPQ24FMOD15EventParameterI` | 0x8039BC74 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `duplicateEvent__Q24FMOD15EventImplSimpleFPQ24FMOD6EventIPQ24FMOD13SimpleMemPool` | 0x8039BC84 | 248 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `copyProperties__Q24FMOD15EventImplSimpleFPQ24FMOD6EventI` | 0x8039BD7C | 180 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `setupInstance__Q24FMOD15EventImplSimpleFv` | 0x8039BE30 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `load__Q24FMOD15EventImplSimpleFPQ24FMOD4FileUi` | 0x8039BE38 | 420 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `dereferenceSoundnames__Q24FMOD15EventImplSimpleFPQ24FMOD8SoundDefi` | 0x8039BFDC | 256 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `soundJustPlayed__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSound` | 0x8039C0DC | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `soundJustStopped__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSound` | 0x8039C0E4 | 8 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `useSoftware__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundPb` | 0x8039C0EC | 20 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getPriority__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundPi` | 0x8039C100 | 20 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getTimeOffset__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundPf` | 0x8039C114 | 20 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getChannelGroup__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundPPQ24FMOD12ChannelGroup` | 0x8039C128 | 40 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `hasPanEnvelope__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundPb` | 0x8039C150 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `hasSpeakerLevelEnvelope__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSoundPb` | 0x8039C160 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `getSound__Q24FMOD15EventImplSimpleFiiPPQ24FMOD10EventSound` | 0x8039C170 | 16 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `rescheduleSounds__Q24FMOD15EventImplSimpleFv` | 0x8039C180 | 88 | parked, 100% |  |
| `fmod/fmod_eventimpl_simple` | `clearPreviousSoundPointers__Q24FMOD15EventImplSimpleFPQ24FMOD10EventSound` | 0x8039C1D8 | 56 | parked, 100% |  |
| `fmod/fmod_eventinstancepool` | `__ct__Q24FMOD17EventInstancePoolFv` | 0x8039C210 | 24 | matched |  |
| `fmod/fmod_eventinstancepool` | `init__Q24FMOD17EventInstancePoolFUi` | 0x8039C228 | 460 | matched |  |
| `fmod/fmod_eventinstancepool` | `release__Q24FMOD17EventInstancePoolFv` | 0x8039C3F4 | 248 | matched |  |
| `fmod/fmod_eventinstancepool` | `getEventInstance__Q24FMOD17EventInstancePoolFPQ24FMOD6EventIPPQ24FMOD6EventI` | 0x8039C4EC | 196 | matched |  |
| `fmod/fmod_eventinstancepool` | `releaseEventInstance__Q24FMOD17EventInstancePoolFPQ24FMOD6EventIb` | 0x8039C5B0 | 296 | matched |  |
| `fmod/fmod_eventinstancepool` | `copyEventInstance__Q24FMOD17EventInstancePoolFPQ24FMOD6EventIPQ24FMOD6EventI` | 0x8039C6D8 | 80 | matched |  |
| `fmod/fmod_eventinstancepool` | `getEventInstanceIndex__Q24FMOD17EventInstancePoolFPQ24FMOD6EventIPi` | 0x8039C728 | 124 | matched |  |
| `fmod/fmod_eventinstancepool` | `getEventInstancePtr__Q24FMOD17EventInstancePoolFUiPPQ24FMOD6EventI` | 0x8039C7A4 | 56 | matched |  |
| `fmod/fmod_eventinstancepool` | `getNumInstancesInUse__Q24FMOD17EventInstancePoolFPQ24FMOD6EventIPi` | 0x8039C7DC | 188 | matched |  |
| `fmod/fmod_eventlayer` | `__ct__Q24FMOD10EventLayerFv` | 0x8039C898 | 96 | matched |  |
| `fmod/fmod_eventlayer` | `init__Q24FMOD10EventLayerFv` | 0x8039C8F8 | 56 | matched |  |
| `fmod/fmod_eventlayer` | `release__Q24FMOD10EventLayerFv` | 0x8039C930 | 392 | matched |  |
| `fmod/fmod_eventlayer` | `dereferenceParentEnvelopes__Q24FMOD10EventLayerFv` | 0x8039CAB8 | 148 | matched |  |
| `fmod/fmod_eventlayer` | `update__Q24FMOD10EventLayerFv` | 0x8039CB4C | 1352 | matched |  |
| `fmod/fmod_eventlayer` | `getLatestEndTime__Q24FMOD10EventLayerFPUxPPQ24FMOD10EventSound` | 0x8039D094 | 224 | matched |  |
| `fmod/fmod_eventlayer` | `rescheduleSounds__Q24FMOD10EventLayerFv` | 0x8039D174 | 168 | matched |  |
| `fmod/fmod_eventlayer` | `clearPreviousPointers__Q24FMOD10EventLayerFPQ24FMOD10EventSound` | 0x8039D21C | 108 | matched |  |
| `fmod/fmod_eventlayer` | `otherSoundsPlaying__Q24FMOD10EventLayerFPQ24FMOD10EventSound` | 0x8039D288 | 80 | matched |  |
| `fmod/fmod_eventlayer` | `willTerminate__Q24FMOD10EventLayerCFv` | 0x8039D2D8 | 108 | matched |  |
| `fmod/fmod_eventlayer` | `getEnvelope__Q24FMOD10EventLayerFUi` | 0x8039D344 | 56 | matched |  |
| `fmod/fmod_eventlayer` | `createDSPNetwork__Q24FMOD10EventLayerFPQ24FMOD12ChannelGroup` | 0x8039D37C | 1904 | matched |  |
| `fmod/fmod_eventlayer` | `releaseDSPNetwork__Q24FMOD10EventLayerFv` | 0x8039DAEC | 204 | matched |  |
| `fmod/fmod_eventlayer` | `startEffects__Q24FMOD10EventLayerFb` | 0x8039DBB8 | 1188 | matched |  |
| `fmod/fmod_eventprojecti` | `init__Q24FMOD6BundleFPPQ24FMOD6EventIi` | 0x8039FEE4 | 1384 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `release__Q24FMOD6BundleFv` | 0x803A044C | 324 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `duplicate__Q24FMOD6BundleFPPQ24FMOD6Bundle` | 0x803A0590 | 676 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `__ct__Q24FMOD13EventProjectIFv` | 0x803A0834 | 112 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `__dt__Q24FMOD12EventProjectFv` | 0x803A08A4 | 64 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `init__Q24FMOD13EventProjectIFv` | 0x803A08E4 | 108 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `createEventTable__Q24FMOD13EventProjectIFUiPQ24FMOD11EventGroupI` | 0x803A0950 | 364 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `createInstancePool__Q24FMOD13EventProjectIFUi` | 0x803A0ABC | 184 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `release__Q24FMOD13EventProjectIFv` | 0x803A0B74 | 828 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getInfo__Q24FMOD13EventProjectIFPiPPc` | 0x803A0EB0 | 96 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getGroup__Q24FMOD13EventProjectIFPCcbPPQ24FMOD10EventGroup` | 0x803A0F10 | 392 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getGroupByIndex__Q24FMOD13EventProjectIFibPPQ24FMOD10EventGroup` | 0x803A1098 | 240 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getNumGroups__Q24FMOD13EventProjectIFPi` | 0x803A1188 | 60 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getEvent__Q24FMOD13EventProjectIFPCcUiPPQ24FMOD5Event` | 0x803A11C4 | 300 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getEventByProjectID__Q24FMOD13EventProjectIFUiUiPPQ24FMOD5Event` | 0x803A12F0 | 92 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getNumEvents__Q24FMOD13EventProjectIFPi` | 0x803A134C | 32 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `addLSDGroup__Q24FMOD13EventProjectIFPPQ24FMOD11EventGroupIRiPQ24FMOD11EventGroupIPPQ24FMOD6EventIiRi` | 0x803A136C | 308 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `loadSampleData__Q24FMOD13EventProjectIFPiiPPciUi` | 0x803A14A0 | 2064 | parked, 92.2% |  |
| `fmod/fmod_eventprojecti` | `setUserData__Q24FMOD13EventProjectIFPv` | 0x803A1CB0 | 12 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `getUserData__Q24FMOD13EventProjectIFPPv` | 0x803A1CBC | 32 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `stopAllEvents__Q24FMOD13EventProjectIFb` | 0x803A1CDC | 168 | parked, 98.8% |  |
| `fmod/fmod_eventprojecti` | `getMemoryInfo__Q24FMOD13EventProjectIFUiUiPUiPUi` | 0x803A1D84 | 8 | parked, 100% |  |
| `fmod/fmod_eventprojecti` | `__dt__Q24FMOD13EventProjectIFv` | 0x803A1D8C | 64 | parked, 100% |  |
| `fmod/fmod_eventreverb` | `__ct__Q24FMOD9ReverbDefFv` | 0x803A1DCC | 140 | matched |  |
| `fmod/fmod_eventreverb` | `release__Q24FMOD9ReverbDefFv` | 0x803A1E58 | 156 | matched |  |
| `fmod/fmod_eventreverb` | `__ct__Q24FMOD12EventReverbIFv` | 0x803A1EF4 | 44 | matched |  |
| `fmod/fmod_eventreverb` | `init__Q24FMOD12EventReverbIFPQ24FMOD6Reverb` | 0x803A1F20 | 12 | matched |  |
| `fmod/fmod_eventreverb` | `release__Q24FMOD12EventReverbIFv` | 0x803A1F2C | 140 | matched |  |
| `fmod/fmod_eventreverb` | `setProperties__Q24FMOD12EventReverbIFPC22FMOD_REVERB_PROPERTIES` | 0x803A1FB8 | 24 | matched |  |
| `fmod/fmod_eventreverb` | `getProperties__Q24FMOD12EventReverbIFP22FMOD_REVERB_PROPERTIES` | 0x803A1FD0 | 24 | matched |  |
| `fmod/fmod_eventreverb` | `set3DAttributes__Q24FMOD12EventReverbIFPC11FMOD_VECTORff` | 0x803A1FE8 | 24 | matched |  |
| `fmod/fmod_eventreverb` | `get3DAttributes__Q24FMOD12EventReverbIFP11FMOD_VECTORPfPf` | 0x803A2000 | 24 | matched |  |
| `fmod/fmod_eventreverb` | `setActive__Q24FMOD12EventReverbIFb` | 0x803A2018 | 24 | matched |  |
| `fmod/fmod_eventreverb` | `getActive__Q24FMOD12EventReverbIFPb` | 0x803A2030 | 24 | matched |  |
| `fmod/fmod_eventreverb` | `setUserData__Q24FMOD12EventReverbIFPv` | 0x803A2048 | 12 | matched |  |
| `fmod/fmod_eventreverb` | `getUserData__Q24FMOD12EventReverbIFPPv` | 0x803A2054 | 32 | matched |  |
| `fmod/fmod_eventreverb` | `getMemoryInfo__Q24FMOD12EventReverbIFUiUiPUiPUi` | 0x803A2074 | 8 | matched |  |
| `fmod/fmod_eventreverb` | `__dt__Q24FMOD12EventReverbIFv` | 0x803A207C | 64 | matched |  |
| `fmod/fmod_eventuserproperty` | `__ct__Q24FMOD12UserPropertyFv` | 0x803A9EC0 | 32 | matched |  |
| `fmod/fmod_eventuserproperty` | `init__Q24FMOD12UserPropertyFPQ24FMOD12UserProperty` | 0x803A9EE0 | 160 | matched |  |
| `fmod/fmod_eventuserproperty` | `release__Q24FMOD12UserPropertyFv` | 0x803A9F80 | 172 | matched |  |
| `fmod/fmod_eventuserproperty` | `load__Q24FMOD12UserPropertyFPQ24FMOD4FileUi` | 0x803AA02C | 508 | matched |  |
| `fmod/fmod_musicsystemi` | `isActive__Q24FMOD12MusicPromptIFPb` | 0x803AC1EC | 32 | matched |  |
| `fmod/fmod_musicsystemi` | `getMemoryInfo__Q24FMOD12MusicPromptIFUiUiPUiPUi` | 0x803AC20C | 8 | matched |  |
| `fmod/fmod_musicsystemi` | `release__Q24FMOD9CuePromptFv` | 0x803AC214 | 64 | matched |  |
| `fmod/fmod_musicsystemi` | `begin__Q24FMOD9CuePromptFv` | 0x803AC254 | 248 | matched |  |
| `fmod/fmod_musicsystemi` | `end__Q24FMOD9CuePromptFv` | 0x803AC34C | 152 | matched |  |
| `fmod/fmod_musicsystemi` | `__ct__Q24FMOD13MusicSettingsFv` | 0x803AC3E4 | 256 | matched |  |
| `fmod/fmod_musicsystemi` | `setChannelGroup__Q24FMOD13MusicSettingsFPQ24FMOD12ChannelGroup` | 0x803AC4E4 | 120 | matched |  |
| `fmod/fmod_musicsystemi` | `applyReverbProperties__Q24FMOD13MusicSettingsFPQ24FMOD7Channel` | 0x803AC55C | 116 | matched |  |
| `fmod/fmod_musicsystemi` | `setReverbProperties__Q24FMOD13MusicSettingsFPC29FMOD_REVERB_CHANNELPROPERTIES` | 0x803AC5D0 | 356 | matched |  |
| `fmod/fmod_musicsystemi` | `getReverbProperties__Q24FMOD13MusicSettingsFP29FMOD_REVERB_CHANNELPROPERTIES` | 0x803AC734 | 192 | matched |  |
| `fmod/fmod_musicsystemi` | `setBaseVolume__Q24FMOD13MusicSettingsFf` | 0x803AC7F4 | 388 | matched |  |
| `fmod/fmod_musicsystemi` | `getBaseVolume__Q24FMOD13MusicSettingsCFPf` | 0x803AC978 | 16 | matched |  |
| `fmod/fmod_musicsystemi` | `setBaseReverbLevel__Q24FMOD13MusicSettingsFf` | 0x803AC988 | 268 | matched |  |
| `fmod/fmod_musicsystemi` | `getBaseReverbLevel__Q24FMOD13MusicSettingsCFPf` | 0x803ACA94 | 16 | matched |  |
| `fmod/fmod_musicsystemi` | `__ct__Q24FMOD12MusicSystemIFv` | 0x803ACAA4 | 32 | matched |  |
| `fmod/fmod_musicsystemi` | `init__Q24FMOD12MusicSystemIFPQ24FMOD6System` | 0x803ACAC4 | 12 | matched |  |
| `fmod/fmod_musicsystemi` | `initMusicEngine__Q24FMOD12MusicSystemIFPQ24FMOD12ChannelGroupPQ24FMOD13MusicSettings` | 0x803ACAD0 | 316 | matched |  |
| `fmod/fmod_musicsystemi` | `update__Q24FMOD12MusicSystemIFv` | 0x803ACC0C | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `release__Q24FMOD12MusicSystemIFv` | 0x803ACC48 | 172 | matched |  |
| `fmod/fmod_musicsystemi` | `ready__Q24FMOD12MusicSystemIFv` | 0x803ACCF4 | 20 | matched |  |
| `fmod/fmod_musicsystemi` | `reset__Q24FMOD12MusicSystemIFv` | 0x803ACD08 | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `setVolume__Q24FMOD12MusicSystemIFf` | 0x803ACD44 | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `getVolume__Q24FMOD12MusicSystemIFPf` | 0x803ACD80 | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `setReverbProperties__Q24FMOD12MusicSystemIFPC29FMOD_REVERB_CHANNELPROPERTIES` | 0x803ACDBC | 76 | matched |  |
| `fmod/fmod_musicsystemi` | `getReverbProperties__Q24FMOD12MusicSystemIFP29FMOD_REVERB_CHANNELPROPERTIES` | 0x803ACE08 | 76 | matched |  |
| `fmod/fmod_musicsystemi` | `setPaused__Q24FMOD12MusicSystemIFb` | 0x803ACE54 | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `getPaused__Q24FMOD12MusicSystemIFPb` | 0x803ACE90 | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `setMute__Q24FMOD12MusicSystemIFb` | 0x803ACECC | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `getMute__Q24FMOD12MusicSystemIFPb` | 0x803ACF08 | 60 | matched |  |
| `fmod/fmod_musicsystemi` | `getInfo__Q24FMOD12MusicSystemIFP15FMOD_MUSIC_INFO` | 0x803ACF44 | 72 | matched |  |
| `fmod/fmod_musicsystemi` | `promptCue__Q24FMOD12MusicSystemIFUi` | 0x803ACF8C | 184 | matched |  |
| `fmod/fmod_musicsystemi` | `prepareCue__Q24FMOD12MusicSystemIFUiPPQ24FMOD11MusicPrompt` | 0x803AD044 | 256 | matched |  |
| `fmod/fmod_musicsystemi` | `getParameterValue__Q24FMOD12MusicSystemIFUiPf` | 0x803AD144 | 160 | matched |  |
| `fmod/fmod_musicsystemi` | `setParameterValue__Q24FMOD12MusicSystemIFUif` | 0x803AD1E4 | 136 | matched |  |
| `fmod/fmod_musicsystemi` | `getCues__Q24FMOD12MusicSystemIFP19FMOD_MUSIC_ITERATORPCc` | 0x803AD26C | 156 | matched |  |
| `fmod/fmod_musicsystemi` | `getNextCue__Q24FMOD12MusicSystemIFP19FMOD_MUSIC_ITERATOR` | 0x803AD308 | 176 | matched |  |
| `fmod/fmod_musicsystemi` | `getParameters__Q24FMOD12MusicSystemIFP19FMOD_MUSIC_ITERATORPCc` | 0x803AD3B8 | 156 | matched |  |
| `fmod/fmod_musicsystemi` | `getNextParameter__Q24FMOD12MusicSystemIFP19FMOD_MUSIC_ITERATOR` | 0x803AD454 | 176 | matched |  |
| `fmod/fmod_musicsystemi` | `loadSoundData__Q24FMOD12MusicSystemIF19FMOD_EVENT_RESOURCEUi` | 0x803AD504 | 120 | matched |  |
| `fmod/fmod_musicsystemi` | `freeSoundData__Q24FMOD12MusicSystemIFb` | 0x803AD57C | 104 | matched |  |
| `fmod/fmod_musicsystemi` | `setCallback__Q24FMOD12MusicSystemIFPF23FMOD_MUSIC_CALLBACKTYPEPvPvPv_11FMOD_RESULTPv` | 0x803AD5E4 | 72 | matched |  |
| `fmod/fmod_musicsystemi` | `getMemoryInfo__Q24FMOD12MusicSystemIFUiUiPUiPUi` | 0x803AD62C | 8 | matched |  |
| `fmod/fmod_musicsystemi` | `__dt__Q24FMOD9CuePromptFv` | 0x803AD634 | 64 | matched |  |
| `fmod/fmod_musicsystemi` | `__dt__Q24FMOD12MusicSystemIFv` | 0x803AD674 | 64 | matched |  |
| `fmod/fmod_simplemempool` | `__ct__Q24FMOD13SimpleMemPoolFv` | 0x803B0204 | 20 | matched |  |
| `fmod/fmod_simplemempool` | `release__Q24FMOD13SimpleMemPoolFv` | 0x803B0218 | 112 | matched |  |
| `fmod/fmod_simplemempool` | `init__Q24FMOD13SimpleMemPoolFiPv` | 0x803B0288 | 120 | matched |  |
| `fmod/fmod_simplemempool` | `alloc__Q24FMOD13SimpleMemPoolFiPCci` | 0x803B0300 | 144 | matched |  |
| `fmod/fmod_simplemempool` | `calloc__Q24FMOD13SimpleMemPoolFiPCci` | 0x803B0390 | 228 | matched |  |
| `fmod/fmod_simplemempool` | `free__Q24FMOD13SimpleMemPoolFPvPCci` | 0x803B0474 | 40 | matched |  |
| `fmod/fmod_sounddef` | `__ct__Q24FMOD16SoundDefWaveformFv` | 0x803B3EC0 | 36 | matched |  |
| `fmod/fmod_sounddef` | `__ct__Q24FMOD18SoundDefOscillatorFv` | 0x803B3EE4 | 36 | matched |  |
| `fmod/fmod_sounddef` | `__ct__Q24FMOD12SoundDefNullFv` | 0x803B3F08 | 20 | matched |  |
| `fmod/fmod_sounddef` | `__ct__Q24FMOD18SoundDefProgrammerFv` | 0x803B3F1C | 20 | matched |  |
| `fmod/fmod_sounddef` | `__ct__Q24FMOD11SoundDefDefFv` | 0x803B3F30 | 52 | matched |  |
| `fmod/fmod_sounddef` | `init__Q24FMOD11SoundDefDefFPQ24FMOD13EventProjectI` | 0x803B3F64 | 80 | matched |  |
| `fmod/fmod_sounddef` | `validateSpawntimes__Q24FMOD11SoundDefDefFv` | 0x803B3FB4 | 40 | matched |  |
| `fmod/fmod_sounddef` | `setSpawnIntensity__Q24FMOD11SoundDefDefFf` | 0x803B3FDC | 88 | matched |  |
| `fmod/fmod_sounddef` | `getPlayMode__Q24FMOD11SoundDefDefFv` | 0x803B4034 | 12 | matched |  |
| `fmod/fmod_sounddef` | `getPitchRecalculateMode__Q24FMOD11SoundDefDefFv` | 0x803B4040 | 12 | matched |  |
| `fmod/fmod_sounddef` | `setPlayMode__Q24FMOD11SoundDefDefFi` | 0x803B404C | 24 | matched |  |
| `fmod/fmod_sounddef` | `setVolumeRandMethod__Q24FMOD11SoundDefDefF10RandMethod` | 0x803B4064 | 24 | matched |  |
| `fmod/fmod_sounddef` | `setPitchRandMethod__Q24FMOD11SoundDefDefF10RandMethod` | 0x803B407C | 24 | matched |  |
| `fmod/fmod_sounddef` | `setPitchRecalculateMode__Q24FMOD11SoundDefDefF15RecalculateMode` | 0x803B4094 | 20 | matched |  |
| `fmod/fmod_sounddef` | `__ct__Q24FMOD8SoundDefFv` | 0x803B40A8 | 52 | matched |  |
| `fmod/fmod_sounddef` | `release__Q24FMOD8SoundDefFb` | 0x803B40DC | 304 | matched |  |
| `fmod/fmod_sounddef` | `getEntry__Q24FMOD8SoundDefFPQ24FMOD10EventSoundPi` | 0x803B420C | 1384 | matched |  |
| `fmod/fmod_sounddef` | `getVolume__Q24FMOD8SoundDefFv` | 0x803B4774 | 388 | matched |  |
| `fmod/fmod_sounddef` | `getPitch__Q24FMOD8SoundDefFv` | 0x803B48F8 | 464 | matched |  |
| `fmod/fmod_sounddef` | `getOffset__Q24FMOD8SoundDefFv` | 0x803B4AC8 | 316 | matched |  |
<!-- ledger:end status -->

## Game code

Climax engine and game units (`GameLib`, `src/game/` and `src/climax/`). Names follow the placeholder ruling in
AGENTS.md (`unk_` classes, `fn_` functions with mangled names in `symbols.txt`, `lbl_` globals); unit names without
class-name evidence are unconfirmed (exceptions.md, open items).
The out-of-line copy of `unk_MapWaypoint`'s inline destructor (`__dt__15unk_MapWaypointFv`, emitted in CMapWaypoint's
object and dead-stripped by the linker) has no retail address and is not counted. Likewise CSplashScreenTrigger's
constructor (`__ct__20CSplashScreenTriggerFv`, inlined into its factory, out-of-line copy dead-stripped) and the weak
thunk `@36@fn_8009CB1C__12CBaseSpawnerFR7unk_Msg` that CGenericSpawner's object emits (the linker keeps the earlier
copy at 0x80053190, exceptions.md "Game code rulings (game-4)").

<!-- ledger:begin game -->
| unit | function | address | size | status | naming |
|---|---|---|---|---|---|
| `game/PadRumble` (unit name unconfirmed) | `fn_800067A0__Fv` | 0x800067A0 | 20 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_800067B4__Fv` | 0x800067B4 | 12 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_800067C0__Fv` | 0x800067C0 | 12 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_800067CC__8unk_RampFf` | 0x800067CC | 148 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_80006860__8unk_RampFfff` | 0x80006860 | 244 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_80006954__8unk_RampFv` | 0x80006954 | 132 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_800069D8__8unk_RampFv` | 0x800069D8 | 64 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_80006A18__Fb` | 0x80006A18 | 8 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `fn_80006A20__Fv` | 0x80006A20 | 8 | matched | placeholder |
| `game/PadRumble` (unit name unconfirmed) | `__sinit_\PadRumble_cpp` | 0x80006A28 | 596 | matched | placeholder (compiler-generated name) |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000A028__Fb` | 0x8000A028 | 4 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000A02C__Fv` | 0x8000A02C | 8 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000A034__Fb` | 0x8000A034 | 12 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `__ct__12unk_8000A040Fv` | 0x8000A040 | 848 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `__dt__12unk_8000A040Fv` | 0x8000A390 | 804 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000A6B4__12unk_8000A040FRf` | 0x8000A6B4 | 4 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000A6B8__Fffff` | 0x8000A6B8 | 180 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000A76C__12unk_8000A040Ff` | 0x8000A76C | 1812 | parked, 99.9% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000AE80__Fv` | 0x8000AE80 | 8 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000AE88__Ff` | 0x8000AE88 | 12 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000AE94__FP12unk_800E671C` | 0x8000AE94 | 116 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000AF08__12unk_8000A040Fi` | 0x8000AF08 | 1748 | parked, 90.6% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000B5DC__12unk_8000A040FiiP12unk_PadQueue` | 0x8000B5DC | 2232 | parked, 90.1% (MATCH-PENDING: R18, exceptions.md) | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000BE94__Fv` | 0x8000BE94 | 12 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000BEA0__Fv` | 0x8000BEA0 | 56 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000BED8__Fv` | 0x8000BED8 | 56 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000BF10__Fii` | 0x8000BF10 | 20 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8000BF24__12unk_8000A040Fif` | 0x8000BF24 | 168 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `fn_8009CB1C__12unk_8000A040FR7unk_Msg` | 0x8000BFCC | 808 | parked, 100% | placeholder |
| `game/unk_8000A040` (unit name unconfirmed) | `__sinit_\unk_8000A040_cpp` | 0x8000C2F4 | 56 | parked, 100% | placeholder (compiler-generated name) |
| `game/CCollisionCapsule` | `__ct__17CCollisionCapsuleFRC19unk_AttributePacket` | 0x8000C8A8 | 212 | matched | placeholder |
| `game/CCollisionCapsule` | `__dt__17CCollisionCapsuleFv` | 0x8000C97C | 196 | matched |  |
| `game/CCollisionCapsule` | `fn_8009CB1C__17CCollisionCapsuleFR7unk_Msg` | 0x8000CA40 | 328 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_801A73A8__17CCollisionCapsuleFRC19unk_AttributePacket` | 0x8000CB88 | 440 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CD40__17CCollisionCapsuleFUl` | 0x8000CD40 | 80 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CD90__17CCollisionCapsuleFP12unk_RwpSpace` | 0x8000CD90 | 296 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CEB8__17CCollisionCapsuleFP12unk_RwpSpace` | 0x8000CEB8 | 136 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CF40__17CCollisionCapsuleCFv` | 0x8000CF40 | 20 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CF54__17CCollisionCapsuleFb` | 0x8000CF54 | 84 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CFA8__17CAttributeHandlerFPUl` | 0x8000CFA8 | 16 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CFB8__17CAttributeHandlerFUlPUlPUl` | 0x8000CFB8 | 20 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CFCC__17CAttributeHandlerFv` | 0x8000CFCC | 4 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CFD0__17CAttributeHandlerFv` | 0x8000CFD0 | 4 | matched | placeholder |
| `game/CCollisionCapsule` | `fn_8000CFD4__17CAttributeHandlerFv` | 0x8000CFD4 | 4 | matched | placeholder |
| `game/CCollisionCapsule` | `__sinit_\CCollisionCapsule_cpp` | 0x8000CFD8 | 96 | matched | placeholder (compiler-generated name) |
| `game/CCollisionCapsule` | `fn_8000D038__17CCollisionCapsuleFRC19unk_AttributePacket` | 0x8000D038 | 72 | matched | placeholder |
| `game/CCollisionCapsule` | `@36@fn_8009CB1C__17CCollisionCapsuleFR7unk_Msg` | 0x8000D080 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCollisionCapsule` | `@36@__dt__17CCollisionCapsuleFv` | 0x8000D088 | 8 | matched | compiler-generated thunk |
| `game/CPreCache` | `__ct__9CPreCacheFRC19unk_AttributePacket` | 0x800288F4 | 136 | matched | placeholder |
| `game/CPreCache` | `__dt__9CPreCacheFv` | 0x8002897C | 196 | matched |  |
| `game/CPreCache` | `fn_801A73A8__9CPreCacheFRC19unk_AttributePacket` | 0x80028A40 | 368 | matched | placeholder |
| `game/CPreCache` | `fn_80028BB0__9CPreCacheFv` | 0x80028BB0 | 4 | matched | placeholder |
| `game/CPreCache` | `fn_8009CB1C__9CPreCacheFR7unk_Msg` | 0x80028BB4 | 344 | matched | placeholder |
| `game/CPreCache` | `fn_80028D0C__FiPCce` | 0x80028D0C | 80 | matched | placeholder |
| `game/CPreCache` | `fn_8000CFD0__9CPreCacheFv` | 0x80028D5C | 20 | matched | placeholder |
| `game/CPreCache` | `fn_8000CFCC__9CPreCacheFv` | 0x80028D70 | 200 | matched | placeholder |
| `game/CPreCache` | `fn_80028E38__9CPreCacheFv` | 0x80028E38 | 364 | matched | placeholder |
| `game/CPreCache` | `__sinit_\CPreCache_cpp` | 0x80028FA4 | 96 | matched | placeholder (compiler-generated name) |
| `game/CPreCache` | `fn_80029004__9CPreCacheFRC19unk_AttributePacket` | 0x80029004 | 72 | matched | placeholder |
| `game/CPreCache` | `@36@fn_8009CB1C__9CPreCacheFR7unk_Msg` | 0x8002904C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPreCache` | `@36@__dt__9CPreCacheFv` | 0x80029054 | 8 | matched | compiler-generated thunk |
| `game/CActorTest` | `__ct__10CActorTestFRC19unk_AttributePacket` | 0x8003BE3C | 272 | matched | placeholder |
| `game/CActorTest` | `__dt__10CActorTestFv` | 0x8003BF4C | 212 | matched |  |
| `game/CActorTest` | `fn_801A7A88__10CActorTestFv` | 0x8003C020 | 4 | matched | placeholder |
| `game/CActorTest` | `fn_801A73A8__10CActorTestFRC19unk_AttributePacket` | 0x8003C024 | 240 | matched | placeholder |
| `game/CActorTest` | `fn_8009CB1C__10CActorTestFR7unk_Msg` | 0x8003C114 | 528 | matched | placeholder |
| `game/CActorTest` | `__sinit_\CActorTest_cpp` | 0x8003C324 | 96 | matched | placeholder (compiler-generated name) |
| `game/CActorTest` | `fn_8003C384__10CActorTestFRC19unk_AttributePacket` | 0x8003C384 | 72 | matched | placeholder |
| `game/CActorTest` | `@36@fn_8009CB1C__10CActorTestFR7unk_Msg` | 0x8003C3CC | 8 | matched | compiler-generated thunk, placeholder |
| `game/CActorTest` | `@36@__dt__10CActorTestFv` | 0x8003C3D4 | 8 | matched | compiler-generated thunk |
| `game/CActorTest` | `@52@__dt__10CActorTestFv` | 0x8003C3DC | 8 | matched | compiler-generated thunk |
| `game/CActorTest` | `@60@__dt__10CActorTestFv` | 0x8003C3E4 | 8 | matched | compiler-generated thunk |
| `game/CActorTest` | `@60@fn_801A73A8__10CActorTestFRC19unk_AttributePacket` | 0x8003C3EC | 8 | matched | compiler-generated thunk, placeholder |
| `game/CActorTest` | `@60@fn_8009CB1C__10CActorTestFR7unk_Msg` | 0x8003C3F4 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPlayerCostume` | `__dt__14CPlayerCostumeFv` | 0x80073E3C | 148 | matched |  |
| `game/CPlayerCostume` | `__sinit_\CPlayerCostume_cpp` | 0x80073ED0 | 96 | matched | placeholder (compiler-generated name) |
| `game/CPlayerCostume` | `fn_80073F30__14CPlayerCostumeFRC19unk_AttributePacket` | 0x80073F30 | 128 | matched | placeholder |
| `game/CPlayerCostume` | `@36@__dt__14CPlayerCostumeFv` | 0x80073FB0 | 8 | matched | compiler-generated thunk |
| `game/CCopCar` (unit name unconfirmed) | `__dt__7CCopCarFv` | 0x80077A8C | 88 | matched |  |
| `game/CCopCar` (unit name unconfirmed) | `fn_801A73A8__7CCopCarFRC19unk_AttributePacket` | 0x80077AE4 | 4 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `fn_800180E4__7CCopCarFv` | 0x80077AE8 | 4 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `fn_80019F88__7CCopCarFf` | 0x80077AEC | 4 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `fn_8009CB1C__7CCopCarFR7unk_Msg` | 0x80077AF0 | 4 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `fn_800436B8__11CGenericCarFv` | 0x80077AF4 | 8 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `fn_80072D14__11CGenericCarFv` | 0x80077AFC | 8 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `__sinit_\CCopCar_cpp` | 0x80077B04 | 136 | matched | placeholder (compiler-generated name) |
| `game/CCopCar` (unit name unconfirmed) | `fn_80077B8C__7CCopCarFRC19unk_AttributePacket` | 0x80077B8C | 160 | matched | placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@36@fn_8009CB1C__7CCopCarFR7unk_Msg` | 0x80077C2C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@36@__dt__7CCopCarFv` | 0x80077C34 | 8 | matched | compiler-generated thunk |
| `game/CCopCar` (unit name unconfirmed) | `@52@__dt__7CCopCarFv` | 0x80077C3C | 8 | matched | compiler-generated thunk |
| `game/CCopCar` (unit name unconfirmed) | `@60@__dt__7CCopCarFv` | 0x80077C44 | 8 | matched | compiler-generated thunk |
| `game/CCopCar` (unit name unconfirmed) | `@60@fn_801A73A8__7CCopCarFRC19unk_AttributePacket` | 0x80077C4C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@60@fn_8009CB1C__7CCopCarFR7unk_Msg` | 0x80077C54 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@444@__dt__7CCopCarFv` | 0x80077C5C | 8 | matched | compiler-generated thunk |
| `game/CCopCar` (unit name unconfirmed) | `@1792@fn_801A73A8__7CCopCarFRC19unk_AttributePacket` | 0x80077C64 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@1792@__dt__7CCopCarFv` | 0x80077C6C | 8 | matched | compiler-generated thunk |
| `game/CCopCar` (unit name unconfirmed) | `@1928@fn_8007B3D0__11CGenericCarFv` | 0x80077C74 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@1928@fn_8007B8F0__11CGenericCarFv` | 0x80077C7C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@1928@fn_8007B4B0__11CGenericCarFv` | 0x80077C84 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@1928@fn_8007B3BC__11CGenericCarFv` | 0x80077C8C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCopCar` (unit name unconfirmed) | `@1928@__dt__7CCopCarFv` | 0x80077C94 | 8 | matched | compiler-generated thunk |
| `game/CHitEffectManager` | `__dt__17CHitEffectManagerFv` | 0x80087614 | 112 | matched |  |
| `game/CHitEffectManager` | `fn_8009CB1C__17CHitEffectManagerFR7unk_Msg` | 0x80087684 | 4 | matched | placeholder |
| `game/CHitEffectManager` | `fn_801A73A8__17CHitEffectManagerFRC19unk_AttributePacket` | 0x80087688 | 312 | matched | placeholder |
| `game/CHitEffectManager` | `__sinit_\CHitEffectManager_cpp` | 0x800877C0 | 96 | matched | placeholder (compiler-generated name) |
| `game/CHitEffectManager` | `fn_80087820__17CHitEffectManagerFRC19unk_AttributePacket` | 0x80087820 | 160 | matched | placeholder |
| `game/CHitEffectManager` | `@36@fn_8009CB1C__17CHitEffectManagerFR7unk_Msg` | 0x800878C0 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CHitEffectManager` | `@36@__dt__17CHitEffectManagerFv` | 0x800878C8 | 8 | matched | compiler-generated thunk |
| `game/CBasicMount` | `__ct__11CBasicMountFRC19unk_AttributePacket` | 0x8009CF20 | 224 | matched | placeholder |
| `game/CBasicMount` | `__dt__11CBasicMountFv` | 0x8009D000 | 148 | matched |  |
| `game/CBasicMount` | `fn_8009CB1C__11CBasicMountFR7unk_Msg` | 0x8009D094 | 252 | matched | placeholder |
| `game/CBasicMount` | `fn_801A73A8__11CBasicMountFRC19unk_AttributePacket` | 0x8009D190 | 156 | matched | placeholder |
| `game/CBasicMount` | `__sinit_\CBasicMount_cpp` | 0x8009D22C | 96 | matched | placeholder (compiler-generated name) |
| `game/CBasicMount` | `fn_8009D28C__11CBasicMountFRC19unk_AttributePacket` | 0x8009D28C | 72 | matched | placeholder |
| `game/CBasicMount` | `@36@fn_8009CB1C__11CBasicMountFR7unk_Msg` | 0x8009D2D4 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CBasicMount` | `@36@__dt__11CBasicMountFv` | 0x8009D2DC | 8 | matched | compiler-generated thunk |
| `game/CClumpSwap` | `__ct__10CClumpSwapFRC19unk_AttributePacket` | 0x8009ED10 | 88 | matched | placeholder |
| `game/CClumpSwap` | `__dt__10CClumpSwapFv` | 0x8009ED68 | 156 | matched |  |
| `game/CClumpSwap` | `fn_801A73A8__10CClumpSwapFRC19unk_AttributePacket` | 0x8009EE04 | 176 | matched | placeholder |
| `game/CClumpSwap` | `fn_801A7A88__10CClumpSwapFv` | 0x8009EEB4 | 20 | matched | placeholder |
| `game/CClumpSwap` | `__sinit_\CClumpSwap_cpp` | 0x8009EEC8 | 96 | matched | placeholder (compiler-generated name) |
| `game/CClumpSwap` | `fn_8009EF28__10CClumpSwapFRC19unk_AttributePacket` | 0x8009EF28 | 112 | matched | placeholder |
| `game/CClumpSwap` | `@36@__dt__10CClumpSwapFv` | 0x8009EF98 | 8 | matched | compiler-generated thunk |
| `game/CScalingObject` | `__ct__14CScalingObjectFRC19unk_AttributePacket` | 0x800A4F64 | 192 | matched | placeholder |
| `game/CScalingObject` | `__dt__14CScalingObjectFv` | 0x800A5024 | 168 | matched |  |
| `game/CScalingObject` | `fn_8000CFD0__14CScalingObjectFv` | 0x800A50CC | 136 | matched | placeholder |
| `game/CScalingObject` | `fn_801A73A8__14CScalingObjectFRC19unk_AttributePacket` | 0x800A5154 | 256 | matched | placeholder |
| `game/CScalingObject` | `fn_8009CB1C__14CScalingObjectFR7unk_Msg` | 0x800A5254 | 112 | matched | placeholder |
| `game/CScalingObject` | `fn_800A52C4__14CScalingObjectFfP7CPlayer` | 0x800A52C4 | 724 | matched | placeholder |
| `game/CScalingObject` | `__sinit_\CScalingObject_cpp` | 0x800A5598 | 96 | matched | placeholder (compiler-generated name) |
| `game/CScalingObject` | `fn_800A55F8__14CScalingObjectFRC19unk_AttributePacket` | 0x800A55F8 | 72 | matched | placeholder |
| `game/CScalingObject` | `@36@fn_8009CB1C__14CScalingObjectFR7unk_Msg` | 0x800A5640 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CScalingObject` | `@36@__dt__14CScalingObjectFv` | 0x800A5648 | 8 | matched | compiler-generated thunk |
| `game/CScalingObject` | `@52@__dt__14CScalingObjectFv` | 0x800A5650 | 8 | matched | compiler-generated thunk |
| `game/CScalingObject` | `@60@__dt__14CScalingObjectFv` | 0x800A5658 | 8 | matched | compiler-generated thunk |
| `game/CScalingObject` | `@60@fn_801A73A8__14CScalingObjectFRC19unk_AttributePacket` | 0x800A5660 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CScalingObject` | `@60@fn_8009CB1C__14CScalingObjectFR7unk_Msg` | 0x800A5668 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CSceneObject` | `__ct__12CSceneObjectFRC19unk_AttributePacket` | 0x800A5670 | 120 | matched | placeholder |
| `game/CSceneObject` | `__dt__12CSceneObjectFv` | 0x800A56E8 | 168 | matched |  |
| `game/CSceneObject` | `fn_8009CB1C__12CSceneObjectFR7unk_Msg` | 0x800A5790 | 112 | matched | placeholder |
| `game/CSceneObject` | `__sinit_\CSceneObject_cpp` | 0x800A5800 | 96 | matched | placeholder (compiler-generated name) |
| `game/CSceneObject` | `fn_800A5860__12CSceneObjectFRC19unk_AttributePacket` | 0x800A5860 | 160 | matched | placeholder |
| `game/CSceneObject` | `@36@fn_8009CB1C__12CSceneObjectFR7unk_Msg` | 0x800A5900 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CSceneObject` | `@36@__dt__12CSceneObjectFv` | 0x800A5908 | 8 | matched | compiler-generated thunk |
| `game/CSceneObject` | `@52@__dt__12CSceneObjectFv` | 0x800A5910 | 8 | matched | compiler-generated thunk |
| `game/CSceneObject` | `@60@__dt__12CSceneObjectFv` | 0x800A5918 | 8 | matched | compiler-generated thunk |
| `game/CSceneObject` | `@60@fn_8009CB1C__12CSceneObjectFR7unk_Msg` | 0x800A5920 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CIGCPlayerMover` | `__dt__15CIGCPlayerMoverFv` | 0x800AB27C | 160 | matched |  |
| `game/CIGCPlayerMover` | `fn_801A73A8__15CIGCPlayerMoverFRC19unk_AttributePacket` | 0x800AB31C | 156 | matched | placeholder |
| `game/CIGCPlayerMover` | `fn_8009CB1C__15CIGCPlayerMoverFR7unk_Msg` | 0x800AB3B8 | 124 | matched | placeholder |
| `game/CIGCPlayerMover` | `__sinit_\CIGCPlayerMover_cpp` | 0x800AB434 | 128 | matched | placeholder (compiler-generated name) |
| `game/CIGCPlayerMover` | `fn_800AB4B4__15CIGCPlayerMoverFRC19unk_AttributePacket` | 0x800AB4B4 | 128 | matched | placeholder |
| `game/CIGCPlayerMover` | `@36@fn_8009CB1C__15CIGCPlayerMoverFR7unk_Msg` | 0x800AB534 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CIGCPlayerMover` | `@36@__dt__15CIGCPlayerMoverFv` | 0x800AB53C | 8 | matched | compiler-generated thunk |
| `game/CAmbientInterferencePoint` | `__ct__25CAmbientInterferencePointFRC19unk_AttributePacket` | 0x800C1E48 | 232 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `__dt__25CAmbientInterferencePointFv` | 0x800C1F30 | 332 | parked, 100% |  |
| `game/CAmbientInterferencePoint` | `fn_8000CFCC__25CAmbientInterferencePointFv` | 0x800C207C | 24 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `fn_801A7A88__25CAmbientInterferencePointFv` | 0x800C2094 | 132 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `fn_8009CB1C__25CAmbientInterferencePointFR7unk_Msg` | 0x800C2118 | 492 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `fn_800C2304__25CAmbientInterferencePointFP14CBaseBehaviour` | 0x800C2304 | 744 | parked, 99.9% (MATCH-PENDING R18 when exact, exceptions.md) | placeholder |
| `game/CAmbientInterferencePoint` | `fn_800C25EC__25CAmbientInterferencePointFf` | 0x800C25EC | 420 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `fn_800C2790__25CAmbientInterferencePointFv` | 0x800C2790 | 224 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `fn_801A73A8__25CAmbientInterferencePointFRC19unk_AttributePacket` | 0x800C2870 | 412 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `fn_800C2A0C__25CAmbientInterferencePointFPCcb` | 0x800C2A0C | 344 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `__sinit_\CAmbientInterferencePoint_cpp` | 0x800C2B64 | 140 | parked, 100% | placeholder (compiler-generated name) |
| `game/CAmbientInterferencePoint` | `fn_800C2BF0__25CAmbientInterferencePointFRC19unk_AttributePacket` | 0x800C2BF0 | 72 | parked, 100% | placeholder |
| `game/CAmbientInterferencePoint` | `@36@fn_8009CB1C__25CAmbientInterferencePointFR7unk_Msg` | 0x800C2C38 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CAmbientInterferencePoint` | `@36@__dt__25CAmbientInterferencePointFv` | 0x800C2C40 | 8 | parked, 100% | compiler-generated thunk |
| `game/CAmbientTextMessage` | `__dt__19CAmbientTextMessageFv` | 0x800C2C48 | 220 | matched |  |
| `game/CAmbientTextMessage` | `fn_8009CB1C__19CAmbientTextMessageFR7unk_Msg` | 0x800C2D24 | 4 | matched | placeholder |
| `game/CAmbientTextMessage` | `fn_800C2790__19CAmbientTextMessageFv` | 0x800C2D28 | 140 | matched | placeholder |
| `game/CAmbientTextMessage` | `fn_801A73A8__19CAmbientTextMessageFRC19unk_AttributePacket` | 0x800C2DB4 | 276 | matched | placeholder |
| `game/CAmbientTextMessage` | `__sinit_\CAmbientTextMessage_cpp` | 0x800C2EC8 | 140 | matched | placeholder (compiler-generated name) |
| `game/CAmbientTextMessage` | `fn_800C2F54__19CAmbientTextMessageFRC19unk_AttributePacket` | 0x800C2F54 | 140 | matched | placeholder |
| `game/CAmbientTextMessage` | `@36@fn_8009CB1C__19CAmbientTextMessageFR7unk_Msg` | 0x800C2FE0 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CAmbientTextMessage` | `@36@__dt__19CAmbientTextMessageFv` | 0x800C2FE8 | 8 | matched | compiler-generated thunk |
| `game/CAmbientVoicemail` | `__dt__17CAmbientVoicemailFv` | 0x800C2FF0 | 220 | matched |  |
| `game/CAmbientVoicemail` | `fn_8009CB1C__17CAmbientVoicemailFR7unk_Msg` | 0x800C30CC | 4 | matched | placeholder |
| `game/CAmbientVoicemail` | `fn_800C2790__17CAmbientVoicemailFv` | 0x800C30D0 | 140 | matched | placeholder |
| `game/CAmbientVoicemail` | `fn_801A73A8__17CAmbientVoicemailFRC19unk_AttributePacket` | 0x800C315C | 276 | matched | placeholder |
| `game/CAmbientVoicemail` | `__sinit_\CAmbientVoicemail_cpp` | 0x800C3270 | 140 | matched | placeholder (compiler-generated name) |
| `game/CAmbientVoicemail` | `fn_800C32FC__17CAmbientVoicemailFRC19unk_AttributePacket` | 0x800C32FC | 140 | matched | placeholder |
| `game/CAmbientVoicemail` | `@36@fn_8009CB1C__17CAmbientVoicemailFR7unk_Msg` | 0x800C3388 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CAmbientVoicemail` | `@36@__dt__17CAmbientVoicemailFv` | 0x800C3390 | 8 | matched | compiler-generated thunk |
| `game/CPointOfInterest` | `__ct__16CPointOfInterestFRC19unk_AttributePacket` | 0x800C4F80 | 64 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `__dt__16CPointOfInterestFv` | 0x800C4FC0 | 144 | parked, 100% |  |
| `game/CPointOfInterest` | `fn_800C5050__16CPointOfInterestFRC19unk_AttributePacketb` | 0x800C5050 | 152 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `fn_801A73A8__16CPointOfInterestFRC19unk_AttributePacket` | 0x800C50E8 | 408 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `fn_800C5280__16CPointOfInterestFPC14CBaseBehaviour` | 0x800C5280 | 88 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `fn_800C52D8__16CPointOfInterestFPC14CBaseBehaviour` | 0x800C52D8 | 72 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `fn_800C5320__16CPointOfInterestFPC9unk_RwV3d` | 0x800C5320 | 212 | parked, 91.5% | placeholder |
| `game/CPointOfInterest` | `fn_800C53F4__16CPointOfInterestFPC9unk_RwV3dP9unk_RwV3d` | 0x800C53F4 | 240 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `fn_800C54E4__16CPointOfInterestFv` | 0x800C54E4 | 8 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `fn_800C54EC__16CPointOfInterestFv` | 0x800C54EC | 20 | parked, 100% | placeholder |
| `game/CPointOfInterest` | `__sinit_\CPointOfInterest_cpp` | 0x800C5500 | 44 | parked, 100% | placeholder (compiler-generated name) |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `__dt__29unk_List<P16CPointOfInterest>Fv` | 0x800C552C | 216 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5604__Fv` | 0x800C5604 | 160 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C56A4__26unk_PointOfInterestManagerFP16CPointOfInterest` | 0x800C56A4 | 96 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5704__26unk_PointOfInterestManagerFP16CPointOfInterest` | 0x800C5704 | 164 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C57A8__26unk_PointOfInterestManagerFPC9unk_RwV3dPC9unk_RwV3d` | 0x800C57A8 | 60 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C57E4__26unk_PointOfInterestManagerFPC16CPointOfInterestf` | 0x800C57E4 | 28 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5800__26unk_PointOfInterestManagerFP7CPlayer` | 0x800C5800 | 1376 | parked, 99.7% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5D60__26unk_PointOfInterestManagerFP12unk_8003C454` | 0x800C5D60 | 68 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5DA4__26unk_PointOfInterestManagerFP12unk_8003C454` | 0x800C5DA4 | 128 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5E24__26unk_PointOfInterestManagerFv` | 0x800C5E24 | 8 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5E2C__26unk_PointOfInterestManagerFP16CPointOfInterest` | 0x800C5E2C | 96 | parked, 100% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5E8C__26unk_PointOfInterestManagerFP16CPointOfInterest` | 0x800C5E8C | 192 | parked, 95.3% | placeholder |
| `game/unk_PointOfInterestManager` (unit name unconfirmed) | `fn_800C5F4C__26unk_PointOfInterestManagerFPCc` | 0x800C5F4C | 128 | parked, 100% | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `__ct__13CPointTriggerFRC19unk_AttributePacket` | 0x800C5FCC | 140 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `__dt__13CPointTriggerFv` | 0x800C6058 | 204 | matched |  |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_801A73A8__13CPointTriggerFRC19unk_AttributePacket` | 0x800C6124 | 336 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_800C6274__13CPointTriggerFP12unk_8003C454` | 0x800C6274 | 44 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_800C62A0__13CPointTriggerFP12unk_8003C454` | 0x800C62A0 | 16 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_800C62B0__13CPointTriggerFP12unk_8003C454` | 0x800C62B0 | 252 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_800C63AC__13CPointTriggerFv` | 0x800C63AC | 16 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_800C63BC__13CPointTriggerFv` | 0x800C63BC | 20 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `__sinit_\CPointTrigger_cpp` | 0x800C63D0 | 96 | matched | placeholder (compiler-generated name) |
| `game/CPointTrigger` (unit name unconfirmed) | `fn_800C6430__13CPointTriggerFRC19unk_AttributePacket` | 0x800C6430 | 192 | matched | placeholder |
| `game/CPointTrigger` (unit name unconfirmed) | `@104@__dt__13CPointTriggerFv` | 0x800C64F0 | 8 | matched | compiler-generated thunk |
| `game/CPointTrigger` (unit name unconfirmed) | `@68@__dt__13CPointTriggerFv` | 0x800C64F8 | 8 | matched | compiler-generated thunk |
| `game/CPointTrigger` (unit name unconfirmed) | `@68@fn_801A73A8__13CPointTriggerFRC19unk_AttributePacket` | 0x800C6500 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPsyPointOfInterest` | `__ct__19CPsyPointOfInterestFRC19unk_AttributePacket` | 0x800C6508 | 320 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `__dt__19CPsyPointOfInterestFv` | 0x800C6648 | 300 | parked, 100% |  |
| `game/CPsyPointOfInterest` | `fn_801A73A8__19CPsyPointOfInterestFRC19unk_AttributePacket` | 0x800C6774 | 600 | parked, 96.0% | placeholder |
| `game/CPsyPointOfInterest` | `fn_801A7A88__19CPsyPointOfInterestFv` | 0x800C69CC | 156 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_8009CB1C__19CPsyPointOfInterestFR7unk_Msg` | 0x800C6A68 | 592 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C4E8C__19CPsyPointOfInterestFv` | 0x800C6CB8 | 8 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C54EC__19CPsyPointOfInterestFv` | 0x800C6CC0 | 72 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C6D08__19CPsyPointOfInterestFPC14unk_ScreenRectP12unk_RwCamera` | 0x800C6D08 | 380 | parked, 99.5% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C6E84__19CPsyPointOfInterestFP12unk_80491B38` | 0x800C6E84 | 1508 | parked, 98.6% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C7468__19CPsyPointOfInterestFb` | 0x800C7468 | 376 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C6274__19CPsyPointOfInterestFP12unk_8003C454` | 0x800C75E0 | 4 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_8002CDC8__19CPsyPointOfInterestFv` | 0x800C75E4 | 8 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C4E94__19CPsyPointOfInterestFv` | 0x800C75EC | 8 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `fn_800C4E9C__19CPsyPointOfInterestFv` | 0x800C75F4 | 8 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `__sinit_\CPsyPointOfInterest_cpp` | 0x800C75FC | 140 | parked, 100% | placeholder (compiler-generated name) |
| `game/CPsyPointOfInterest` | `fn_800C7688__19CPsyPointOfInterestFRC19unk_AttributePacket` | 0x800C7688 | 84 | parked, 100% | placeholder |
| `game/CPsyPointOfInterest` | `@68@fn_801A7A88__19CPsyPointOfInterestFv` | 0x800C76DC | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CPsyPointOfInterest` | `@104@fn_8009CB1C__19CPsyPointOfInterestFR7unk_Msg` | 0x800C76E4 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CPsyPointOfInterest` | `@104@__dt__19CPsyPointOfInterestFv` | 0x800C76EC | 8 | parked, 100% | compiler-generated thunk |
| `game/CPsyPointOfInterest` | `@68@__dt__19CPsyPointOfInterestFv` | 0x800C76F4 | 8 | parked, 100% | compiler-generated thunk |
| `game/CPsyPointOfInterest` | `@68@fn_801A73A8__19CPsyPointOfInterestFRC19unk_AttributePacket` | 0x800C76FC | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CPsyPointOfInterest` | `@68@fn_8009CB1C__19CPsyPointOfInterestFR7unk_Msg` | 0x800C7704 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CGenericSpawner` | `__ct__15CGenericSpawnerFRC19unk_AttributePacket` | 0x800C8108 | 224 | matched | placeholder |
| `game/CGenericSpawner` | `__dt__15CGenericSpawnerFv` | 0x800C81E8 | 128 | matched |  |
| `game/CGenericSpawner` | `fn_800C8268__15CGenericSpawnerFUlUlUlUlUlP15CGenericSpawner` | 0x800C8268 | 136 | matched | placeholder |
| `game/CGenericSpawner` | `fn_8000CFD0__15CGenericSpawnerFv` | 0x800C82F0 | 160 | matched | placeholder |
| `game/CGenericSpawner` | `fn_8000CFCC__15CGenericSpawnerFv` | 0x800C8390 | 148 | matched | placeholder |
| `game/CGenericSpawner` | `fn_801A73A8__15CGenericSpawnerFRC19unk_AttributePacket` | 0x800C8424 | 164 | matched | placeholder |
| `game/CGenericSpawner` | `fn_800C84C8__15CGenericSpawnerFv` | 0x800C84C8 | 32 | matched | placeholder |
| `game/CGenericSpawner` | `__sinit_\CGenericSpawner_cpp` | 0x800C84E8 | 96 | matched | placeholder (compiler-generated name) |
| `game/CGenericSpawner` | `fn_800C8548__15CGenericSpawnerFRC19unk_AttributePacket` | 0x800C8548 | 72 | matched | placeholder |
| `game/CGenericSpawner` | `@36@__dt__15CGenericSpawnerFv` | 0x800C8590 | 8 | matched | compiler-generated thunk |
| `game/unk_800C8598` (unit name unconfirmed) | `__ct__12unk_800C8598FP12unk_801A92A8` | 0x800C8598 | 104 | matched | placeholder |
| `game/unk_800C8598` (unit name unconfirmed) | `__dt__12unk_800C8598Fv` | 0x800C8600 | 112 | matched | placeholder |
| `game/unk_800C8598` (unit name unconfirmed) | `fn_8009CB1C__12unk_800C8598FR7unk_Msg` | 0x800C8670 | 236 | matched | placeholder |
| `game/CColdTrigger` | `__dt__12CColdTriggerFv` | 0x800C875C | 148 | matched |  |
| `game/CColdTrigger` | `fn_801A73A8__12CColdTriggerFRC19unk_AttributePacket` | 0x800C87F0 | 232 | matched | placeholder |
| `game/CColdTrigger` | `fn_8009CB1C__12CColdTriggerFR7unk_Msg` | 0x800C88D8 | 80 | matched | placeholder |
| `game/CColdTrigger` | `__sinit_\CColdTrigger_cpp` | 0x800C8928 | 96 | matched | placeholder (compiler-generated name) |
| `game/CColdTrigger` | `fn_800C8988__12CColdTriggerFRC19unk_AttributePacket` | 0x800C8988 | 116 | matched | placeholder |
| `game/CColdTrigger` | `@36@fn_8009CB1C__12CColdTriggerFR7unk_Msg` | 0x800C89FC | 8 | matched | compiler-generated thunk, placeholder |
| `game/CColdTrigger` | `@36@__dt__12CColdTriggerFv` | 0x800C8A04 | 8 | matched | compiler-generated thunk |
| `game/CCombatZoneSetEnemyCountTrigger` | `__dt__31CCombatZoneSetEnemyCountTriggerFv` | 0x800C8A0C | 148 | matched |  |
| `game/CCombatZoneSetEnemyCountTrigger` | `fn_8009CB1C__31CCombatZoneSetEnemyCountTriggerFR7unk_Msg` | 0x800C8AA0 | 120 | matched | placeholder |
| `game/CCombatZoneSetEnemyCountTrigger` | `fn_801A73A8__31CCombatZoneSetEnemyCountTriggerFRC19unk_AttributePacket` | 0x800C8B18 | 180 | matched | placeholder |
| `game/CCombatZoneSetEnemyCountTrigger` | `__sinit_\CCombatZoneSetEnemyCountTrigger_cpp` | 0x800C8BCC | 96 | matched | placeholder (compiler-generated name) |
| `game/CCombatZoneSetEnemyCountTrigger` | `fn_800C8C2C__31CCombatZoneSetEnemyCountTriggerFRC19unk_AttributePacket` | 0x800C8C2C | 116 | matched | placeholder |
| `game/CCombatZoneSetEnemyCountTrigger` | `@36@fn_8009CB1C__31CCombatZoneSetEnemyCountTriggerFR7unk_Msg` | 0x800C8CA0 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCombatZoneSetEnemyCountTrigger` | `@36@__dt__31CCombatZoneSetEnemyCountTriggerFv` | 0x800C8CA8 | 8 | matched | compiler-generated thunk |
| `game/CCounter` | `__dt__8CCounterFv` | 0x800C8CB0 | 200 | matched |  |
| `game/CCounter` | `fn_8009CB1C__8CCounterFR7unk_Msg` | 0x800C8D78 | 344 | matched | placeholder |
| `game/CCounter` | `fn_801A73A8__8CCounterFRC19unk_AttributePacket` | 0x800C8ED0 | 260 | matched | placeholder |
| `game/CCounter` | `__sinit_\CCounter_cpp` | 0x800C8FD4 | 96 | matched | placeholder (compiler-generated name) |
| `game/CCounter` | `fn_800C9034__8CCounterFRC19unk_AttributePacket` | 0x800C9034 | 152 | matched | placeholder |
| `game/CCounter` | `@36@fn_8009CB1C__8CCounterFR7unk_Msg` | 0x800C90CC | 8 | matched | compiler-generated thunk, placeholder |
| `game/CCounter` | `@36@__dt__8CCounterFv` | 0x800C90D4 | 8 | matched | compiler-generated thunk |
| `game/DataTrigger` | `__dt__11DataTriggerFv` | 0x800C90DC | 168 | matched |  |
| `game/DataTrigger` | `fn_801A73A8__11DataTriggerFRC19unk_AttributePacket` | 0x800C9184 | 232 | matched | placeholder |
| `game/DataTrigger` | `fn_8009CB1C__11DataTriggerFR7unk_Msg` | 0x800C926C | 172 | matched | placeholder |
| `game/DataTrigger` | `__sinit_\DataTrigger_cpp` | 0x800C9318 | 96 | matched | placeholder (compiler-generated name) |
| `game/DataTrigger` | `fn_800C9378__11DataTriggerFRC19unk_AttributePacket` | 0x800C9378 | 120 | matched | placeholder |
| `game/DataTrigger` | `@36@fn_8009CB1C__11DataTriggerFR7unk_Msg` | 0x800C93F0 | 8 | matched | compiler-generated thunk, placeholder |
| `game/DataTrigger` | `@36@__dt__11DataTriggerFv` | 0x800C93F8 | 8 | matched | compiler-generated thunk |
| `game/CDesignerDebugMsg` | `__dt__17CDesignerDebugMsgFv` | 0x800C9400 | 200 | matched |  |
| `game/CDesignerDebugMsg` | `fn_8009CB1C__17CDesignerDebugMsgFR7unk_Msg` | 0x800C94C8 | 476 | matched | placeholder |
| `game/CDesignerDebugMsg` | `fn_801A73A8__17CDesignerDebugMsgFRC19unk_AttributePacket` | 0x800C96A4 | 188 | matched | placeholder |
| `game/CDesignerDebugMsg` | `__sinit_\CDesignerDebugMsg_cpp` | 0x800C9760 | 96 | matched | placeholder (compiler-generated name) |
| `game/CDesignerDebugMsg` | `fn_800C97C0__17CDesignerDebugMsgFRC19unk_AttributePacket` | 0x800C97C0 | 124 | matched | placeholder |
| `game/CDesignerDebugMsg` | `@36@fn_8009CB1C__17CDesignerDebugMsgFR7unk_Msg` | 0x800C983C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CDesignerDebugMsg` | `@36@__dt__17CDesignerDebugMsgFv` | 0x800C9844 | 8 | matched | compiler-generated thunk |
| `game/CFadeTrigger` | `__dt__12CFadeTriggerFv` | 0x800C984C | 192 | matched |  |
| `game/CFadeTrigger` | `fn_8009CB1C__12CFadeTriggerFR7unk_Msg` | 0x800C990C | 316 | matched | placeholder |
| `game/CFadeTrigger` | `fn_801A73A8__12CFadeTriggerFRC19unk_AttributePacket` | 0x800C9A48 | 344 | matched | placeholder |
| `game/CFadeTrigger` | `__sinit_\CFadeTrigger_cpp` | 0x800C9BA0 | 96 | matched | placeholder (compiler-generated name) |
| `game/CFadeTrigger` | `fn_800C9C00__12CFadeTriggerFRC19unk_AttributePacket` | 0x800C9C00 | 152 | matched | placeholder |
| `game/CFadeTrigger` | `@36@fn_8009CB1C__12CFadeTriggerFR7unk_Msg` | 0x800C9C98 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CFadeTrigger` | `@36@__dt__12CFadeTriggerFv` | 0x800C9CA0 | 8 | matched | compiler-generated thunk |
| `game/CFlickerTrigger` | `__dt__15CFlickerTriggerFv` | 0x800C9CA8 | 168 | matched |  |
| `game/CFlickerTrigger` | `fn_801A73A8__15CFlickerTriggerFRC19unk_AttributePacket` | 0x800C9D50 | 284 | matched | placeholder |
| `game/CFlickerTrigger` | `fn_8009CB1C__15CFlickerTriggerFR7unk_Msg` | 0x800C9E6C | 208 | matched | placeholder |
| `game/CFlickerTrigger` | `__sinit_\CFlickerTrigger_cpp` | 0x800C9F3C | 96 | matched | placeholder (compiler-generated name) |
| `game/CFlickerTrigger` | `fn_800C9F9C__15CFlickerTriggerFRC19unk_AttributePacket` | 0x800C9F9C | 140 | matched | placeholder |
| `game/CFlickerTrigger` | `@36@fn_8009CB1C__15CFlickerTriggerFR7unk_Msg` | 0x800CA028 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CFlickerTrigger` | `@36@__dt__15CFlickerTriggerFv` | 0x800CA030 | 8 | matched | compiler-generated thunk |
| `game/FMVTrigger` | `__dt__10FMVTriggerFv` | 0x800CA038 | 172 | matched |  |
| `game/FMVTrigger` | `fn_8009CB1C__10FMVTriggerFR7unk_Msg` | 0x800CA0E4 | 84 | matched | placeholder |
| `game/FMVTrigger` | `fn_801A73A8__10FMVTriggerFRC19unk_AttributePacket` | 0x800CA138 | 188 | matched | placeholder |
| `game/FMVTrigger` | `__sinit_\FMVTrigger_cpp` | 0x800CA1F4 | 140 | matched | placeholder (compiler-generated name) |
| `game/FMVTrigger` | `fn_800CA280__10FMVTriggerFRC19unk_AttributePacket` | 0x800CA280 | 116 | matched | placeholder |
| `game/FMVTrigger` | `@36@fn_8009CB1C__10FMVTriggerFR7unk_Msg` | 0x800CA2F4 | 8 | matched | compiler-generated thunk, placeholder |
| `game/FMVTrigger` | `@36@__dt__10FMVTriggerFv` | 0x800CA2FC | 8 | matched | compiler-generated thunk |
| `game/CKeyHUDTrigger` | `__dt__14CKeyHUDTriggerFv` | 0x800CB1C8 | 148 | matched |  |
| `game/CKeyHUDTrigger` | `fn_8009CB1C__14CKeyHUDTriggerFR7unk_Msg` | 0x800CB25C | 216 | matched | placeholder |
| `game/CKeyHUDTrigger` | `fn_801A73A8__14CKeyHUDTriggerFRC19unk_AttributePacket` | 0x800CB334 | 180 | matched | placeholder |
| `game/CKeyHUDTrigger` | `__sinit_\CKeyHUDTrigger_cpp` | 0x800CB3E8 | 140 | matched | placeholder (compiler-generated name) |
| `game/CKeyHUDTrigger` | `fn_800CB474__14CKeyHUDTriggerFRC19unk_AttributePacket` | 0x800CB474 | 116 | matched | placeholder |
| `game/CKeyHUDTrigger` | `@36@fn_8009CB1C__14CKeyHUDTriggerFR7unk_Msg` | 0x800CB4E8 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CKeyHUDTrigger` | `@36@__dt__14CKeyHUDTriggerFv` | 0x800CB4F0 | 8 | matched | compiler-generated thunk |
| `game/CMessageRelay` | `__ct__13CMessageRelayFRC19unk_AttributePacket` | 0x800CB4F8 | 204 | matched | placeholder |
| `game/CMessageRelay` | `__dt__13CMessageRelayFv` | 0x800CB5C4 | 356 | matched |  |
| `game/CMessageRelay` | `fn_8009CB1C__13CMessageRelayFR7unk_Msg` | 0x800CB728 | 548 | matched | placeholder |
| `game/CMessageRelay` | `fn_801A73A8__13CMessageRelayFRC19unk_AttributePacket` | 0x800CB94C | 804 | matched | placeholder |
| `game/CMessageRelay` | `fn_800CBC70__13CMessageRelayFv` | 0x800CBC70 | 464 | matched | placeholder |
| `game/CMessageRelay` | `__sinit_\CMessageRelay_cpp` | 0x800CBE40 | 96 | matched | placeholder (compiler-generated name) |
| `game/CMessageRelay` | `fn_800CBEA0__13CMessageRelayFRC19unk_AttributePacket` | 0x800CBEA0 | 72 | matched | placeholder |
| `game/CMessageRelay` | `@36@fn_8009CB1C__13CMessageRelayFR7unk_Msg` | 0x800CBEE8 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CMessageRelay` | `@36@__dt__13CMessageRelayFv` | 0x800CBEF0 | 8 | matched | compiler-generated thunk |
| `game/CModalTutorialMessageTrigger` | `fn_800CBEF8__Fv` | 0x800CBEF8 | 8 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `__dt__28CModalTutorialMessageTriggerFv` | 0x800CBF00 | 228 | matched |  |
| `game/CModalTutorialMessageTrigger` | `fn_8009CB1C__28CModalTutorialMessageTriggerFR7unk_Msg` | 0x800CBFE4 | 388 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `fn_801A73A8__28CModalTutorialMessageTriggerFRC19unk_AttributePacket` | 0x800CC168 | 244 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `fn_800CC25C__Fv` | 0x800CC25C | 104 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `__dt__22unk_TutorialMessageBoxFv` | 0x800CC2C4 | 160 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `fn_800CC364__22unk_TutorialMessageBoxFPviPCciUl` | 0x800CC364 | 104 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `__sinit_\CModalTutorialMessageTrigger_cpp` | 0x800CC3CC | 140 | matched | placeholder (compiler-generated name) |
| `game/CModalTutorialMessageTrigger` | `fn_800CC458__28CModalTutorialMessageTriggerFRC19unk_AttributePacket` | 0x800CC458 | 124 | matched | placeholder |
| `game/CModalTutorialMessageTrigger` | `@36@fn_8009CB1C__28CModalTutorialMessageTriggerFR7unk_Msg` | 0x800CC4D4 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CModalTutorialMessageTrigger` | `@36@__dt__28CModalTutorialMessageTriggerFv` | 0x800CC4DC | 8 | matched | compiler-generated thunk |
| `game/MusicTrigger` | `__dt__12MusicTriggerFv` | 0x800CC4E4 | 160 | matched |  |
| `game/MusicTrigger` | `fn_8000CFD0__12MusicTriggerFv` | 0x800CC584 | 132 | matched | placeholder |
| `game/MusicTrigger` | `fn_8000CFCC__12MusicTriggerFv` | 0x800CC608 | 120 | matched | placeholder |
| `game/MusicTrigger` | `fn_801A73A8__12MusicTriggerFRC19unk_AttributePacket` | 0x800CC680 | 208 | matched | placeholder |
| `game/MusicTrigger` | `fn_8009CB1C__12MusicTriggerFR7unk_Msg` | 0x800CC750 | 216 | matched | placeholder |
| `game/MusicTrigger` | `__sinit_\MusicTrigger_cpp` | 0x800CC828 | 96 | matched | placeholder (compiler-generated name) |
| `game/MusicTrigger` | `fn_800CC888__12MusicTriggerFRC19unk_AttributePacket` | 0x800CC888 | 124 | matched | placeholder |
| `game/MusicTrigger` | `@36@fn_8009CB1C__12MusicTriggerFR7unk_Msg` | 0x800CC904 | 8 | matched | compiler-generated thunk, placeholder |
| `game/MusicTrigger` | `@36@__dt__12MusicTriggerFv` | 0x800CC90C | 8 | matched | compiler-generated thunk |
| `game/COnScreenPromptTrigger` | `fn_80072D1C__25unk_OnScreenPromptRequestCFv` | 0x800CC914 | 156 | matched | placeholder |
| `game/COnScreenPromptTrigger` | `__dt__22COnScreenPromptTriggerFv` | 0x800CC9B0 | 332 | matched |  |
| `game/COnScreenPromptTrigger` | `fn_8009CB1C__22COnScreenPromptTriggerFR7unk_Msg` | 0x800CCAFC | 376 | matched | placeholder |
| `game/COnScreenPromptTrigger` | `fn_801A73A8__22COnScreenPromptTriggerFRC19unk_AttributePacket` | 0x800CCC74 | 296 | matched | placeholder |
| `game/COnScreenPromptTrigger` | `__dt__25unk_OnScreenPromptRequestFv` | 0x800CCD9C | 64 | matched | placeholder |
| `game/COnScreenPromptTrigger` | `__sinit_\COnScreenPromptTrigger_cpp` | 0x800CCDDC | 140 | matched | placeholder (compiler-generated name) |
| `game/COnScreenPromptTrigger` | `fn_800CCE68__22COnScreenPromptTriggerFRC19unk_AttributePacket` | 0x800CCE68 | 152 | matched | placeholder |
| `game/COnScreenPromptTrigger` | `@36@fn_8009CB1C__22COnScreenPromptTriggerFR7unk_Msg` | 0x800CCF00 | 8 | matched | compiler-generated thunk, placeholder |
| `game/COnScreenPromptTrigger` | `@36@__dt__22COnScreenPromptTriggerFv` | 0x800CCF08 | 8 | matched | compiler-generated thunk |
| `game/CPadRumbleTrigger` | `__dt__17CPadRumbleTriggerFv` | 0x800CCF10 | 172 | matched |  |
| `game/CPadRumbleTrigger` | `fn_8009CB1C__17CPadRumbleTriggerFR7unk_Msg` | 0x800CCFBC | 428 | matched | placeholder |
| `game/CPadRumbleTrigger` | `fn_801A73A8__17CPadRumbleTriggerFRC19unk_AttributePacket` | 0x800CD168 | 260 | matched | placeholder |
| `game/CPadRumbleTrigger` | `__sinit_\CPadRumbleTrigger_cpp` | 0x800CD26C | 96 | matched | placeholder (compiler-generated name) |
| `game/CPadRumbleTrigger` | `fn_800CD2CC__17CPadRumbleTriggerFRC19unk_AttributePacket` | 0x800CD2CC | 152 | matched | placeholder |
| `game/CPadRumbleTrigger` | `@36@fn_8009CB1C__17CPadRumbleTriggerFR7unk_Msg` | 0x800CD364 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPadRumbleTrigger` | `@36@__dt__17CPadRumbleTriggerFv` | 0x800CD36C | 8 | matched | compiler-generated thunk |
| `game/CPhoneCallTrigger` | `__ct__17CPhoneCallTriggerFRC19unk_AttributePacket` | 0x800CD374 | 164 | matched | placeholder |
| `game/CPhoneCallTrigger` | `__dt__17CPhoneCallTriggerFv` | 0x800CD418 | 364 | matched |  |
| `game/CPhoneCallTrigger` | `fn_8009CB1C__17CPhoneCallTriggerFR7unk_Msg` | 0x800CD584 | 244 | matched | placeholder |
| `game/CPhoneCallTrigger` | `fn_801A73A8__17CPhoneCallTriggerFRC19unk_AttributePacket` | 0x800CD678 | 440 | matched | placeholder |
| `game/CPhoneCallTrigger` | `fn_800CD830__17CPhoneCallTriggerFv` | 0x800CD830 | 132 | matched | placeholder |
| `game/CPhoneCallTrigger` | `__sinit_\CPhoneCallTrigger_cpp` | 0x800CD8B4 | 140 | matched | placeholder (compiler-generated name) |
| `game/CPhoneCallTrigger` | `fn_800CD940__17CPhoneCallTriggerFRC19unk_AttributePacket` | 0x800CD940 | 72 | matched | placeholder |
| `game/CPhoneCallTrigger` | `@36@fn_8009CB1C__17CPhoneCallTriggerFR7unk_Msg` | 0x800CD988 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPhoneCallTrigger` | `@36@__dt__17CPhoneCallTriggerFv` | 0x800CD990 | 8 | matched | compiler-generated thunk |
| `game/CPhoneMessageTrigger` | `__dt__20CPhoneMessageTriggerFv` | 0x800CD998 | 244 | matched |  |
| `game/CPhoneMessageTrigger` | `fn_8009CB1C__20CPhoneMessageTriggerFR7unk_Msg` | 0x800CDA8C | 128 | matched | placeholder |
| `game/CPhoneMessageTrigger` | `fn_801A73A8__20CPhoneMessageTriggerFRC19unk_AttributePacket` | 0x800CDB0C | 292 | matched | placeholder |
| `game/CPhoneMessageTrigger` | `__sinit_\CPhoneMessageTrigger_cpp` | 0x800CDC30 | 140 | matched | placeholder (compiler-generated name) |
| `game/CPhoneMessageTrigger` | `fn_800CDCBC__20CPhoneMessageTriggerFRC19unk_AttributePacket` | 0x800CDCBC | 132 | matched | placeholder |
| `game/CPhoneMessageTrigger` | `@36@fn_8009CB1C__20CPhoneMessageTriggerFR7unk_Msg` | 0x800CDD40 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPhoneMessageTrigger` | `@36@__dt__20CPhoneMessageTriggerFv` | 0x800CDD48 | 8 | matched | compiler-generated thunk |
| `game/PlaneTrigger` (unit name unconfirmed) | `__dt__12PlaneTriggerFv` | 0x800CDD50 | 256 | matched |  |
| `game/PlaneTrigger` (unit name unconfirmed) | `fn_8009CB1C__12PlaneTriggerFR7unk_Msg` | 0x800CDE50 | 1000 | matched | placeholder |
| `game/PlaneTrigger` (unit name unconfirmed) | `fn_800CE238__12PlaneTriggerFR7unk_Msg` | 0x800CE238 | 4 | matched | placeholder |
| `game/PlaneTrigger` (unit name unconfirmed) | `fn_801A73A8__12PlaneTriggerFRC19unk_AttributePacket` | 0x800CE23C | 812 | matched | placeholder |
| `game/PlaneTrigger` (unit name unconfirmed) | `__sinit_\PlaneTrigger_cpp` | 0x800CE568 | 96 | matched | placeholder (compiler-generated name) |
| `game/PlaneTrigger` (unit name unconfirmed) | `fn_800CE5C8__12PlaneTriggerFRC19unk_AttributePacket` | 0x800CE5C8 | 172 | matched | placeholder |
| `game/PlaneTrigger` (unit name unconfirmed) | `@36@fn_8009CB1C__12PlaneTriggerFR7unk_Msg` | 0x800CE674 | 8 | matched | compiler-generated thunk, placeholder |
| `game/PlaneTrigger` (unit name unconfirmed) | `@36@__dt__12PlaneTriggerFv` | 0x800CE67C | 8 | matched | compiler-generated thunk |
| `game/CPlayerStateTrigger` | `__dt__19CPlayerStateTriggerFv` | 0x800CE684 | 148 | matched |  |
| `game/CPlayerStateTrigger` | `fn_801A73A8__19CPlayerStateTriggerFRC19unk_AttributePacket` | 0x800CE718 | 232 | matched | placeholder |
| `game/CPlayerStateTrigger` | `fn_8009CB1C__19CPlayerStateTriggerFR7unk_Msg` | 0x800CE800 | 160 | matched | placeholder |
| `game/CPlayerStateTrigger` | `__sinit_\CPlayerStateTrigger_cpp` | 0x800CE8A0 | 96 | matched | placeholder (compiler-generated name) |
| `game/CPlayerStateTrigger` | `fn_800CE900__19CPlayerStateTriggerFRC19unk_AttributePacket` | 0x800CE900 | 120 | matched | placeholder |
| `game/CPlayerStateTrigger` | `@36@fn_8009CB1C__19CPlayerStateTriggerFR7unk_Msg` | 0x800CE978 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPlayerStateTrigger` | `@36@__dt__19CPlayerStateTriggerFv` | 0x800CE980 | 8 | matched | compiler-generated thunk |
| `game/CPsycologyZoneTrigger` | `__dt__21CPsycologyZoneTriggerFv` | 0x800CE988 | 160 | matched |  |
| `game/CPsycologyZoneTrigger` | `fn_8009CB1C__21CPsycologyZoneTriggerFR7unk_Msg` | 0x800CEA28 | 40 | matched | placeholder |
| `game/CPsycologyZoneTrigger` | `fn_801A73A8__21CPsycologyZoneTriggerFRC19unk_AttributePacket` | 0x800CEA50 | 188 | matched | placeholder |
| `game/CPsycologyZoneTrigger` | `__sinit_\CPsycologyZoneTrigger_cpp` | 0x800CEB0C | 96 | matched | placeholder (compiler-generated name) |
| `game/CPsycologyZoneTrigger` | `fn_800CEB6C__21CPsycologyZoneTriggerFRC19unk_AttributePacket` | 0x800CEB6C | 116 | matched | placeholder |
| `game/CPsycologyZoneTrigger` | `@36@fn_8009CB1C__21CPsycologyZoneTriggerFR7unk_Msg` | 0x800CEBE0 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPsycologyZoneTrigger` | `@36@__dt__21CPsycologyZoneTriggerFv` | 0x800CEBE8 | 8 | matched | compiler-generated thunk |
| `game/CPsyTrigger` | `__dt__11CPsyTriggerFv` | 0x800CEBF0 | 148 | matched |  |
| `game/CPsyTrigger` | `fn_801A73A8__11CPsyTriggerFRC19unk_AttributePacket` | 0x800CEC84 | 208 | matched | placeholder |
| `game/CPsyTrigger` | `fn_8009CB1C__11CPsyTriggerFR7unk_Msg` | 0x800CED54 | 100 | matched | placeholder |
| `game/CPsyTrigger` | `__sinit_\CPsyTrigger_cpp` | 0x800CEDB8 | 96 | matched | placeholder (compiler-generated name) |
| `game/CPsyTrigger` | `fn_800CEE18__11CPsyTriggerFRC19unk_AttributePacket` | 0x800CEE18 | 124 | matched | placeholder |
| `game/CPsyTrigger` | `@36@fn_8009CB1C__11CPsyTriggerFR7unk_Msg` | 0x800CEE94 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CPsyTrigger` | `@36@__dt__11CPsyTriggerFv` | 0x800CEE9C | 8 | matched | compiler-generated thunk |
| `game/RespotZoneTrigger` | `__dt__17RespotZoneTriggerFv` | 0x800CEEA4 | 148 | matched |  |
| `game/RespotZoneTrigger` | `fn_801A73A8__17RespotZoneTriggerFRC19unk_AttributePacket` | 0x800CEF38 | 220 | matched | placeholder |
| `game/RespotZoneTrigger` | `fn_8009CB1C__17RespotZoneTriggerFR7unk_Msg` | 0x800CF014 | 172 | matched | placeholder |
| `game/RespotZoneTrigger` | `__sinit_\RespotZoneTrigger_cpp` | 0x800CF0C0 | 96 | matched | placeholder (compiler-generated name) |
| `game/RespotZoneTrigger` | `fn_800CF120__17RespotZoneTriggerFRC19unk_AttributePacket` | 0x800CF120 | 124 | matched | placeholder |
| `game/RespotZoneTrigger` | `@36@fn_8009CB1C__17RespotZoneTriggerFR7unk_Msg` | 0x800CF19C | 8 | matched | compiler-generated thunk, placeholder |
| `game/RespotZoneTrigger` | `@36@__dt__17RespotZoneTriggerFv` | 0x800CF1A4 | 8 | matched | compiler-generated thunk |
| `game/CScreenShakeTrigger` | `__dt__19CScreenShakeTriggerFv` | 0x800CF1AC | 148 | matched |  |
| `game/CScreenShakeTrigger` | `fn_8009CB1C__19CScreenShakeTriggerFR7unk_Msg` | 0x800CF240 | 228 | matched | placeholder |
| `game/CScreenShakeTrigger` | `fn_801A73A8__19CScreenShakeTriggerFRC19unk_AttributePacket` | 0x800CF324 | 280 | matched | placeholder |
| `game/CScreenShakeTrigger` | `__sinit_\CScreenShakeTrigger_cpp` | 0x800CF43C | 96 | matched | placeholder (compiler-generated name) |
| `game/CScreenShakeTrigger` | `fn_800CF49C__19CScreenShakeTriggerFRC19unk_AttributePacket` | 0x800CF49C | 132 | matched | placeholder |
| `game/CScreenShakeTrigger` | `@36@fn_8009CB1C__19CScreenShakeTriggerFR7unk_Msg` | 0x800CF520 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CScreenShakeTrigger` | `@36@__dt__19CScreenShakeTriggerFv` | 0x800CF528 | 8 | matched | compiler-generated thunk |
| `game/CSlowMoTrigger` | `__dt__14CSlowMoTriggerFv` | 0x800CF530 | 192 | matched |  |
| `game/CSlowMoTrigger` | `fn_8009CB1C__14CSlowMoTriggerFR7unk_Msg` | 0x800CF5F0 | 324 | matched | placeholder |
| `game/CSlowMoTrigger` | `fn_801A73A8__14CSlowMoTriggerFRC19unk_AttributePacket` | 0x800CF734 | 292 | matched | placeholder |
| `game/CSlowMoTrigger` | `__sinit_\CSlowMoTrigger_cpp` | 0x800CF858 | 96 | matched | placeholder (compiler-generated name) |
| `game/CSlowMoTrigger` | `fn_800CF8B8__14CSlowMoTriggerFRC19unk_AttributePacket` | 0x800CF8B8 | 148 | matched | placeholder |
| `game/CSlowMoTrigger` | `@36@fn_8009CB1C__14CSlowMoTriggerFR7unk_Msg` | 0x800CF94C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CSlowMoTrigger` | `@36@__dt__14CSlowMoTriggerFv` | 0x800CF954 | 8 | matched | compiler-generated thunk |
| `game/CSplashScreenTrigger` | `__dt__20CSplashScreenTriggerFv` | 0x800CF95C | 180 | matched |  |
| `game/CSplashScreenTrigger` | `fn_801A73A8__20CSplashScreenTriggerFRC19unk_AttributePacket` | 0x800CFA10 | 256 | matched | placeholder |
| `game/CSplashScreenTrigger` | `fn_8009CB1C__20CSplashScreenTriggerFR7unk_Msg` | 0x800CFB10 | 304 | matched | placeholder |
| `game/CSplashScreenTrigger` | `fn_800CFC40__20CSplashScreenTriggerFf` | 0x800CFC40 | 1036 | matched | placeholder |
| `game/CSplashScreenTrigger` | `__sinit_\CSplashScreenTrigger_cpp` | 0x800D004C | 96 | matched | placeholder (compiler-generated name) |
| `game/CSplashScreenTrigger` | `fn_800D00AC__20CSplashScreenTriggerFRC19unk_AttributePacket` | 0x800D00AC | 136 | matched | placeholder |
| `game/CSplashScreenTrigger` | `@36@fn_8009CB1C__20CSplashScreenTriggerFR7unk_Msg` | 0x800D0134 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CSplashScreenTrigger` | `@36@__dt__20CSplashScreenTriggerFv` | 0x800D013C | 8 | matched | compiler-generated thunk |
| `game/CStaticInit` | `__dt__11CStaticInitFv` | 0x800D0144 | 148 | matched |  |
| `game/CStaticInit` | `fn_8009CB1C__11CStaticInitFR7unk_Msg` | 0x800D01D8 | 132 | matched | placeholder |
| `game/CStaticInit` | `fn_801A73A8__11CStaticInitFRC19unk_AttributePacket` | 0x800D025C | 152 | matched | placeholder |
| `game/CStaticInit` | `__sinit_\CStaticInit_cpp` | 0x800D02F4 | 96 | matched | placeholder (compiler-generated name) |
| `game/CStaticInit` | `fn_800D0354__11CStaticInitFRC19unk_AttributePacket` | 0x800D0354 | 136 | matched | placeholder |
| `game/CStaticInit` | `@36@fn_8009CB1C__11CStaticInitFR7unk_Msg` | 0x800D03DC | 8 | matched | compiler-generated thunk, placeholder |
| `game/CStaticInit` | `@36@__dt__11CStaticInitFv` | 0x800D03E4 | 8 | matched | compiler-generated thunk |
| `game/CSwimStaminaTrigger` | `__dt__19CSwimStaminaTriggerFv` | 0x800D03EC | 156 | matched |  |
| `game/CSwimStaminaTrigger` | `fn_801A73A8__19CSwimStaminaTriggerFRC19unk_AttributePacket` | 0x800D0488 | 220 | matched | placeholder |
| `game/CSwimStaminaTrigger` | `fn_8009CB1C__19CSwimStaminaTriggerFR7unk_Msg` | 0x800D0564 | 120 | matched | placeholder |
| `game/CSwimStaminaTrigger` | `__sinit_\CSwimStaminaTrigger_cpp` | 0x800D05DC | 128 | matched | placeholder (compiler-generated name) |
| `game/CSwimStaminaTrigger` | `fn_800D065C__19CSwimStaminaTriggerFRC19unk_AttributePacket` | 0x800D065C | 140 | matched | placeholder |
| `game/CSwimStaminaTrigger` | `@36@fn_8009CB1C__19CSwimStaminaTriggerFR7unk_Msg` | 0x800D06E8 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CSwimStaminaTrigger` | `@36@__dt__19CSwimStaminaTriggerFv` | 0x800D06F0 | 8 | matched | compiler-generated thunk |
| `game/CVoicemailMessageTrigger` | `__dt__24CVoicemailMessageTriggerFv` | 0x800D06F8 | 244 | matched |  |
| `game/CVoicemailMessageTrigger` | `fn_8009CB1C__24CVoicemailMessageTriggerFR7unk_Msg` | 0x800D07EC | 128 | matched | placeholder |
| `game/CVoicemailMessageTrigger` | `fn_801A73A8__24CVoicemailMessageTriggerFRC19unk_AttributePacket` | 0x800D086C | 292 | matched | placeholder |
| `game/CVoicemailMessageTrigger` | `__sinit_\CVoicemailMessageTrigger_cpp` | 0x800D0990 | 140 | matched | placeholder (compiler-generated name) |
| `game/CVoicemailMessageTrigger` | `fn_800D0A1C__24CVoicemailMessageTriggerFRC19unk_AttributePacket` | 0x800D0A1C | 132 | matched | placeholder |
| `game/CVoicemailMessageTrigger` | `@36@fn_8009CB1C__24CVoicemailMessageTriggerFR7unk_Msg` | 0x800D0AA0 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CVoicemailMessageTrigger` | `@36@__dt__24CVoicemailMessageTriggerFv` | 0x800D0AA8 | 8 | matched | compiler-generated thunk |
| `game/AreaTriggerBox` (unit name unconfirmed) | `__dt__14AreaTriggerBoxFv` | 0x800D1268 | 192 | matched |  |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_801A73A8__14AreaTriggerBoxFRC19unk_AttributePacket` | 0x800D1328 | 268 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_800D1434__14AreaTriggerBoxFPv` | 0x800D1434 | 188 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_800D14F0__14AreaTriggerBoxFPv` | 0x800D14F0 | 188 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `__dt__19AreaTriggerBoxLogicFv` | 0x800D15AC | 88 | matched |  |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_801A73A8__19AreaTriggerBoxLogicFRC19unk_AttributePacket` | 0x800D1604 | 4 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_800D1608__19AreaTriggerBoxLogicFPC8unk_Vec3` | 0x800D1608 | 128 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_800D1688__19AreaTriggerBoxLogicFPC8unk_Vec3` | 0x800D1688 | 400 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_800D1818__19AreaTriggerBoxLogicFv` | 0x800D1818 | 4 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `__sinit_\AreaTriggerBox_cpp` | 0x800D181C | 116 | matched | placeholder (compiler-generated name) |
| `game/AreaTriggerBox` (unit name unconfirmed) | `fn_800D1890__14AreaTriggerBoxFRC19unk_AttributePacket` | 0x800D1890 | 148 | matched | placeholder |
| `game/AreaTriggerBox` (unit name unconfirmed) | `@36@__dt__19AreaTriggerBoxLogicFv` | 0x800D1924 | 8 | matched | compiler-generated thunk |
| `game/AreaTriggerBox` (unit name unconfirmed) | `@36@__dt__14AreaTriggerBoxFv` | 0x800D192C | 8 | matched | compiler-generated thunk |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8011FB28` | 0x8011FB28 | 148 | parked, 2.7% | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8011FBBC` | 0x8011FBBC | 80 | parked, 5.0% | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8011FC0C` | 0x8011FC0C | 180 | parked, 2.2% | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8011FCC0` | 0x8011FCC0 | 624 | parked, 0.6% | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8011FF30` | 0x8011FF30 | 96 | parked, not written | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8011FF90` | 0x8011FF90 | 116 | parked, 3.4% | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_80120004` | 0x80120004 | 8 | parked, 20.0% | placeholder |
| `game/CAIFrozenLakeEnemyRespot` | `fn_8012000C` | 0x8012000C | 8 | parked, 20.0% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_801A73A8__13CButtonPuzzleFRC19unk_AttributePacket` | 0x8015A830 | 200 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_80164578__13CButtonPuzzleFv` | 0x8015A8F8 | 176 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015A9A8__12unk_80161958Ff` | 0x8015A9A8 | 100 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_80160584__12unk_80161958Ff` | 0x8015AA0C | 116 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8009CB1C__13CButtonPuzzleFR7unk_Msg` | 0x8015AA80 | 804 | parked, 99.6% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_80165B90__13CButtonPuzzleFb` | 0x8015ADA4 | 72 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015DBD4__13CButtonPuzzleFv` | 0x8015ADEC | 48 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015AE1C__13CButtonPuzzleFPUl` | 0x8015AE1C | 132 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015AEA0__12unk_80161958FP9unk_RwV3d` | 0x8015AEA0 | 32 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015AEC0__12unk_80161958Fv` | 0x8015AEC0 | 8 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015AEC8__12unk_80161958Fv` | 0x8015AEC8 | 8 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `__dt__13CButtonPuzzleFv` | 0x8015AED0 | 88 | parked, 100% |  |
| `game/CButtonPuzzle` (unit name unconfirmed) | `__sinit_\CButtonPuzzle_cpp` | 0x8015AF28 | 140 | parked, 100% | placeholder (compiler-generated name) |
| `game/CButtonPuzzle` (unit name unconfirmed) | `fn_8015AFB4__13CButtonPuzzleFRC19unk_AttributePacket` | 0x8015AFB4 | 156 | parked, 100% | placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@36@fn_8009CB1C__13CButtonPuzzleFR7unk_Msg` | 0x8015B050 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@36@__dt__13CButtonPuzzleFv` | 0x8015B058 | 8 | parked, 100% | compiler-generated thunk |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@52@__dt__13CButtonPuzzleFv` | 0x8015B060 | 8 | parked, 100% | compiler-generated thunk |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@60@__dt__13CButtonPuzzleFv` | 0x8015B068 | 8 | parked, 100% | compiler-generated thunk |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@60@fn_801A73A8__13CButtonPuzzleFRC19unk_AttributePacket` | 0x8015B070 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@60@fn_8009CB1C__13CButtonPuzzleFR7unk_Msg` | 0x8015B078 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CButtonPuzzle` (unit name unconfirmed) | `@324@fn_80160F08__12unk_8015F670Fv` | 0x8015B080 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CSequencePuzzleLogic` | `__ct__20CSequencePuzzleLogicFRC19unk_AttributePacket` | 0x8016FB50 | 308 | parked, 100% | placeholder |
| `game/CSequencePuzzleLogic` | `__dt__20CSequencePuzzleLogicFv` | 0x8016FC84 | 572 | parked, 100% |  |
| `game/CSequencePuzzleLogic` | `fn_8009CB1C__20CSequencePuzzleLogicFR7unk_Msg` | 0x8016FEC0 | 1572 | parked, 95.3% | placeholder |
| `game/CSequencePuzzleLogic` | `fn_801A73A8__20CSequencePuzzleLogicFRC19unk_AttributePacket` | 0x801704E4 | 648 | parked, 100% | placeholder |
| `game/CSequencePuzzleLogic` | `fn_8000CFD0__20CSequencePuzzleLogicFv` | 0x8017076C | 936 | parked, 96.6% | placeholder |
| `game/CSequencePuzzleLogic` | `fn_8000CFCC__20CSequencePuzzleLogicFv` | 0x80170B14 | 320 | parked, 98.4% | placeholder |
| `game/CSequencePuzzleLogic` | `fn_80170C54__20CSequencePuzzleLogicFv` | 0x80170C54 | 472 | parked, 71.0% | placeholder |
| `game/CSequencePuzzleLogic` | `__sinit_\CSequencePuzzleLogic_cpp` | 0x80170E2C | 96 | parked, 100% | placeholder (compiler-generated name) |
| `game/CSequencePuzzleLogic` | `fn_80170E8C__20CSequencePuzzleLogicFRC19unk_AttributePacket` | 0x80170E8C | 72 | parked, 100% | placeholder |
| `game/CSequencePuzzleLogic` | `@36@fn_8009CB1C__20CSequencePuzzleLogicFR7unk_Msg` | 0x80170ED4 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CSequencePuzzleLogic` | `@36@__dt__20CSequencePuzzleLogicFv` | 0x80170EDC | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `__ct__12CDrawingTestFRC19unk_AttributePacket` | 0x80177864 | 584 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `__dt__12CDrawingTestFv` | 0x80177AAC | 408 | parked, 100% |  |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_801A73A8__12CDrawingTestFRC19unk_AttributePacket` | 0x80177C44 | 424 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_8009CB1C__12CDrawingTestFR7unk_Msg` | 0x80177DEC | 1464 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_801A7A88__12CDrawingTestFv` | 0x801783A4 | 308 | parked, 65.2% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_8000CFCC__12CDrawingTestFv` | 0x801784D8 | 4 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_8000CFD0__12CDrawingTestFv` | 0x801784DC | 164 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_80178580__12CDrawingTestFP12unk_RpAtomic` | 0x80178580 | 52 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_801785B4__12CDrawingTestFv` | 0x801785B4 | 1196 | parked, 98.8% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_80178A60__12CDrawingTestFv` | 0x80178A60 | 2212 | parked, 100% (MATCH-PENDING: R18, exceptions.md) | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_80179304__12CDrawingTestFP12unk_RwCamera` | 0x80179304 | 1012 | parked, 74.4% (MATCH-PENDING: R18, exceptions.md) | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_801796F8__12CDrawingTestFP12unk_RwCamera` | 0x801796F8 | 1480 | parked, 99.5% (MATCH-PENDING: R18, exceptions.md) | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_80179CC0__12CDrawingTestFv` | 0x80179CC0 | 876 | parked, 96.2% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `__dt__7CCrayonFv` | 0x8017A02C | 88 | parked, 100% |  |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_801A73A8__7CCrayonFRC19unk_AttributePacket` | 0x8017A084 | 220 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_8009CB1C__7CCrayonFR7unk_Msg` | 0x8017A160 | 4 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_801A7A88__7CCrayonFv` | 0x8017A164 | 64 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `__sinit_\CDrawingTest_cpp` | 0x8017A1A4 | 204 | parked, 100% | placeholder (compiler-generated name) |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_8017A270__12CDrawingTestFRC19unk_AttributePacket` | 0x8017A270 | 72 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `fn_8017A2B8__7CCrayonFRC19unk_AttributePacket` | 0x8017A2B8 | 148 | parked, 100% | placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@36@fn_8009CB1C__7CCrayonFR7unk_Msg` | 0x8017A34C | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@36@__dt__7CCrayonFv` | 0x8017A354 | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `@52@__dt__7CCrayonFv` | 0x8017A35C | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `@60@__dt__7CCrayonFv` | 0x8017A364 | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `@60@fn_801A73A8__7CCrayonFRC19unk_AttributePacket` | 0x8017A36C | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@60@fn_8009CB1C__7CCrayonFR7unk_Msg` | 0x8017A374 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@36@fn_8009CB1C__12CDrawingTestFR7unk_Msg` | 0x8017A37C | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@36@__dt__12CDrawingTestFv` | 0x8017A384 | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `@52@__dt__12CDrawingTestFv` | 0x8017A38C | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `@60@__dt__12CDrawingTestFv` | 0x8017A394 | 8 | parked, 100% | compiler-generated thunk |
| `game/CDrawingTest` (unit name unconfirmed) | `@60@fn_801A73A8__12CDrawingTestFRC19unk_AttributePacket` | 0x8017A39C | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@60@fn_8009CB1C__12CDrawingTestFR7unk_Msg` | 0x8017A3A4 | 8 | parked, 100% | compiler-generated thunk, placeholder |
| `game/CDrawingTest` (unit name unconfirmed) | `@468@__dt__12CDrawingTestFv` | 0x8017A3AC | 8 | parked, 100% | compiler-generated thunk |
| `game/CMapLayer` | `__dt__9CMapLayerFv` | 0x8018AB64 | 148 | matched |  |
| `game/CMapLayer` | `fn_8009CB1C__9CMapLayerFR7unk_Msg` | 0x8018ABF8 | 116 | matched | placeholder |
| `game/CMapLayer` | `fn_801A73A8__9CMapLayerFRC19unk_AttributePacket` | 0x8018AC6C | 212 | matched | placeholder |
| `game/CMapLayer` | `__sinit_\CMapLayer_cpp` | 0x8018AD40 | 96 | matched | placeholder (compiler-generated name) |
| `game/CMapLayer` | `fn_8018ADA0__9CMapLayerFRC19unk_AttributePacket` | 0x8018ADA0 | 124 | matched | placeholder |
| `game/CMapLayer` | `@36@fn_8009CB1C__9CMapLayerFR7unk_Msg` | 0x8018AE1C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CMapLayer` | `@36@__dt__9CMapLayerFv` | 0x8018AE24 | 8 | matched | compiler-generated thunk |
| `game/CMapWaypoint` | `__dt__12CMapWaypointFv` | 0x8018D374 | 252 | matched |  |
| `game/CMapWaypoint` | `fn_8009CB1C__12CMapWaypointFR7unk_Msg` | 0x8018D470 | 84 | matched | placeholder |
| `game/CMapWaypoint` | `fn_801A73A8__12CMapWaypointFRC19unk_AttributePacket` | 0x8018D4C4 | 288 | matched | placeholder |
| `game/CMapWaypoint` | `__dt__18CRemoveMapWaypointFv` | 0x8018D5E4 | 148 | matched |  |
| `game/CMapWaypoint` | `fn_801A73A8__18CRemoveMapWaypointFRC19unk_AttributePacket` | 0x8018D678 | 180 | matched | placeholder |
| `game/CMapWaypoint` | `fn_8009CB1C__18CRemoveMapWaypointFR7unk_Msg` | 0x8018D72C | 128 | matched | placeholder |
| `game/CMapWaypoint` | `__sinit_\CMapWaypoint_cpp` | 0x8018D7AC | 168 | matched | placeholder (compiler-generated name) |
| `game/CMapWaypoint` | `fn_8018D854__12CMapWaypointFRC19unk_AttributePacket` | 0x8018D854 | 164 | matched | placeholder |
| `game/CMapWaypoint` | `fn_8018D8F8__18CRemoveMapWaypointFRC19unk_AttributePacket` | 0x8018D8F8 | 116 | matched | placeholder |
| `game/CMapWaypoint` | `@36@fn_8009CB1C__12CMapWaypointFR7unk_Msg` | 0x8018D96C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CMapWaypoint` | `@36@__dt__12CMapWaypointFv` | 0x8018D974 | 8 | matched | compiler-generated thunk |
| `game/CMapWaypoint` | `@36@fn_8009CB1C__18CRemoveMapWaypointFR7unk_Msg` | 0x8018D97C | 8 | matched | compiler-generated thunk, placeholder |
| `game/CMapWaypoint` | `@36@__dt__18CRemoveMapWaypointFv` | 0x8018D984 | 8 | matched | compiler-generated thunk |
| `game/CMapWaypointPurge` | `__dt__17CMapWaypointPurgeFv` | 0x8018D98C | 148 | matched |  |
| `game/CMapWaypointPurge` | `fn_8009CB1C__17CMapWaypointPurgeFR7unk_Msg` | 0x8018DA20 | 80 | matched | placeholder |
| `game/CMapWaypointPurge` | `fn_801A73A8__17CMapWaypointPurgeFRC19unk_AttributePacket` | 0x8018DA70 | 156 | matched | placeholder |
| `game/CMapWaypointPurge` | `__sinit_\CMapWaypointPurge_cpp` | 0x8018DB0C | 96 | matched | placeholder (compiler-generated name) |
| `game/CMapWaypointPurge` | `fn_8018DB6C__17CMapWaypointPurgeFRC19unk_AttributePacket` | 0x8018DB6C | 108 | matched | placeholder |
| `game/CMapWaypointPurge` | `@36@fn_8009CB1C__17CMapWaypointPurgeFR7unk_Msg` | 0x8018DBD8 | 8 | matched | compiler-generated thunk, placeholder |
| `game/CMapWaypointPurge` | `@36@__dt__17CMapWaypointPurgeFv` | 0x8018DBE0 | 8 | matched | compiler-generated thunk |
| `climax/cattributehandler` | `GetInstanceId__19unk_AttributePacketCFv` | 0x801A74FC | 120 | matched | placeholder |
| `climax/cattributehandler` | `ReprocessAttributePacket__19unk_AttributePacketFv` | 0x801A7574 | 160 | matched | placeholder |
| `climax/cattributehandler` | `__ct__28unk_AttributeCommandIteratorFRC19unk_AttributePacketUl` | 0x801A7614 | 60 | matched | placeholder |
| `climax/cattributehandler` | `SkipToNextAttributeCommand__28unk_AttributeCommandIteratorFb` | 0x801A7650 | 164 | matched | placeholder |
| `climax/cattributehandler` | `Clone__19unk_AttributePacketCFv` | 0x801A76F4 | 152 | matched | placeholder |
| `climax/cattributehandler` | `CreateEntity__19unk_AttributePacketCFP8RwStream` | 0x801A778C | 228 | matched | placeholder |
| `climax/cattributehandler` | `__dt__17CAttributeHandlerFv` | 0x801A7870 | 104 | matched |  |
| `climax/cattributehandler` | `RegisterForDelete__17CAttributeHandlerFv` | 0x801A78D8 | 92 | matched |  |
| `climax/cattributehandler` | `FindTexture__17CAttributeHandlerFPCci` | 0x801A7934 | 172 | matched |  |
| `climax/cattributehandler` | `FindRaster__17CAttributeHandlerFPCci` | 0x801A79E0 | 164 | matched |  |
| `climax/cattributehandler` | `fn_801A7A84__FPCc` | 0x801A7A84 | 4 | matched | placeholder |
| `climax/cattributehandler` | `fn_801A7A88__17CAttributeHandlerFv` | 0x801A7A88 | 152 | matched | placeholder |
| `climax/cattributehandler` | `fn_801A7B20__17CAttributeHandlerFv` | 0x801A7B20 | 220 | matched | placeholder |
| `climax/cattributehandler` | `fn_801A7BFC__17CAttributeHandlerFP15CZoneChangeInfo` | 0x801A7BFC | 12 | matched | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `__ct__12unk_801A92A8Fv` | 0x801A92A8 | 20 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `__dt__12unk_801A92A8Fv` | 0x801A92BC | 64 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `__as__Q23RWS8CEventIdFPCQ23RWS15CRegisteredMsgs` | 0x801A92FC | 20 | parked, not written |  |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9310__Fv` | 0x801A9310 | 164 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A93B4__FR12unk_801A92A8PCcPCc` | 0x801A93B4 | 1352 | parked, 99.9% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `__ct__15unk_EventRecordFv` | 0x801A98FC | 68 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9940__FR12unk_801A92A8` | 0x801A9940 | 364 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9AAC__13CEventHandlerFRC7unk_Msg` | 0x801A9AAC | 60 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9AE8__Fv` | 0x801A9AE8 | 332 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `__dt__13CEventHandlerFv` | 0x801A9C34 | 436 | parked, 100% |  |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9DE8__13CEventHandlerFR12unk_801A92A8PCcUs` | 0x801A9DE8 | 24 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9E00__13CEventHandlerFR12unk_801A92A8` | 0x801A9E00 | 216 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9ED8__FR12unk_801A92A8` | 0x801A9ED8 | 108 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9F44__13CEventHandlerFv` | 0x801A9F44 | 32 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801A9F64__13CEventHandlerFR12unk_801A92A8PCcUs` | 0x801A9F64 | 536 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801AA17C__13CEventHandlerFR12unk_801A92A8` | 0x801AA17C | 72 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801AA1C4__FR12unk_801A92A8PCcPCc` | 0x801AA1C4 | 84 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801AA218__13CEventHandlerFR12unk_801A92A8PCcPCc` | 0x801AA218 | 148 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801AA2AC__FR7unk_Msg` | 0x801AA2AC | 284 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801AA3C8__FPvll` | 0x801AA3C8 | 16 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `Plugin_Copy__Q33RWS22RpAtomicToEventHandler27@unnamed@ceventhandler_cpp@FPvPCvii` | 0x801AA3D8 | 16 | parked, not written |  |
| `climax/EventHandler` (unit name unconfirmed) | `fn_801AA3E8__Fv` | 0x801AA3E8 | 76 | parked, 100% | placeholder |
| `climax/EventHandler` (unit name unconfirmed) | `__sinit_\EventHandler_cpp` | 0x801AA434 | 284 | parked, 100% | placeholder (compiler-generated name) |
| `climax/EventHandler` (unit name unconfirmed) | `__ct__13unk_QueuedMsgFv` | 0x801AA550 | 4 | parked, 100% | placeholder |
| `climax/FullHeapAllocator` | `__ct__18CFullHeapAllocatorFv` | 0x801AC530 | 16 | matched |  |
| `climax/FullHeapAllocator` | `__ct__18CFullHeapAllocatorFPvi` | 0x801AC540 | 80 | matched |  |
| `climax/FullHeapAllocator` | `Alloc__18CFullHeapAllocatorFi` | 0x801AC590 | 184 | matched |  |
| `climax/FullHeapAllocator` | `ReAlloc__18CFullHeapAllocatorFPvi` | 0x801AC648 | 8 | matched |  |
| `climax/FullHeapAllocator` | `Free__18CFullHeapAllocatorFPv` | 0x801AC650 | 220 | matched |  |
| `climax/ZStream` | `CreateZStream__FP8RwStreamii` | 0x801B4464 | 292 | matched |  |
| `climax/ZStream` | `IF_ZAlloc__7ZStreamFPvUiUi` | 0x801B4588 | 12 | matched |  |
| `climax/ZStream` | `IF_ZFree__7ZStreamFPvPv` | 0x801B4594 | 8 | matched |  |
| `climax/ZStream` | `IFStreamClose__7ZStreamFPv` | 0x801B459C | 68 | matched |  |
| `climax/ZStream` | `IFStreamRead__7ZStreamFPvPvUi` | 0x801B45E0 | 756 | matched |  |
| `climax/ZStream` | `IFStreamWrite__7ZStreamFPvPCvUi` | 0x801B48D4 | 8 | matched |  |
| `climax/ZStream` | `IFStreamSkip__7ZStreamFPvUi` | 0x801B48DC | 748 | matched |  |
| `climax/Utils` (unit name unconfirmed) | `fn_801B67FC__FPCci` | 0x801B67FC | 116 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6870__FPCci` | 0x801B6870 | 116 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B68E4__FPcPCci` | 0x801B68E4 | 16 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B68F4__FPCwPc` | 0x801B68F4 | 160 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6994__FPCcPw` | 0x801B6994 | 156 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6A30__FPCc` | 0x801B6A30 | 68 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6A74__FP11unk_RwFramePv` | 0x801B6A74 | 100 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6AD8__FP11unk_RpClump` | 0x801B6AD8 | 60 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6B14__FP12unk_RpAtomicPv` | 0x801B6B14 | 48 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6B44__FP11unk_RpClumpP20unk_RpHAnimHierarchy` | 0x801B6B44 | 16 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6B54__FP9RwTexturePv` | 0x801B6B54 | 84 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6BA8__FP15RwTexDictionaryPv` | 0x801B6BA8 | 88 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6C00__FPCci` | 0x801B6C00 | 100 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6C64__FUlUl` | 0x801B6C64 | 20 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6C78__FUl` | 0x801B6C78 | 32 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6C98__Fs` | 0x801B6C98 | 20 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6CAC__Ff` | 0x801B6CAC | 56 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6CE4__FPPCc` | 0x801B6CE4 | 36 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6D08__FPCP11unk_XmlNodePCcPP11unk_XmlNode` | 0x801B6D08 | 72 | matched | placeholder |
| `climax/Utils` (unit name unconfirmed) | `fn_801B6D50__FPP11unk_XmlNodePCc` | 0x801B6D50 | 244 | matched | placeholder |
| `climax/ImageCodec` (unit name unconfirmed) | `fn_801B6E44__Fii` | 0x801B6E44 | 44 | matched | placeholder |
| `climax/ImageCodec` (unit name unconfirmed) | `fn_801B6E70__FPC7RwImage` | 0x801B6E70 | 88 | matched | placeholder |
| `climax/ImageCodec` (unit name unconfirmed) | `fn_801B6EC8__FP7RwImagePv` | 0x801B6EC8 | 3284 | matched | placeholder |
| `climax/ImageCodec` (unit name unconfirmed) | `fn_801B7B9C__FPv` | 0x801B7B9C | 2860 | matched | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D197C` | 0x801D197C | 700 | parked, 91.2% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_800CC364__15unk_StringTableFPviPCciUl` | 0x801D1C38 | 196 | parked, 100% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D1CFC__15unk_StringTableFPv` | 0x801D1CFC | 540 | parked, 77.8% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `__dt__11unk_List<i>Fv` | 0x801D1F18 | 216 | parked, 100% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D1FF0` | 0x801D1FF0 | 36 | parked, 100% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D2014` | 0x801D2014 | 44 | parked, 100% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D2040` | 0x801D2040 | 60 | parked, 100% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D207C__FPCcPcPCci` | 0x801D207C | 612 | parked, 97.6% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D22E0` | 0x801D22E0 | 628 | parked, 90.1% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D2554` | 0x801D2554 | 220 | parked, 89.7% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D2630` | 0x801D2630 | 176 | parked, 93.2% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `fn_801D26E0` | 0x801D26E0 | 380 | parked, 95.9% | placeholder |
| `climax/Localisation` (unit name unconfirmed) | `__sinit_\Localisation_cpp` | 0x801D285C | 56 | parked, 100% | placeholder (compiler-generated name) |
| `climax/Localisation` (unit name unconfirmed) | `__dt__28unk_List<P15unk_StringTable>Fv` | 0x801D2894 | 216 | parked, 100% | placeholder |
| `climax/GeometryData` | `RpGeometryGetData__FP10RpGeometry` | 0x801D2E00 | 184 | matched |  |
| `climax/GeometryData` | `PluginObjectConstructor__13cGeometryDataFPvii` | 0x801D2EB8 | 12 | matched |  |
| `climax/GeometryData` | `PluginObjectCopy__13cGeometryDataFPvPCvii` | 0x801D2EC4 | 32 | matched |  |
| `climax/GeometryData` | `PluginObjectDestructor__13cGeometryDataFPvii` | 0x801D2EE4 | 136 | matched |  |
| `climax/GeometryData` | `PluginDataChunkReadCallBack__13cGeometryDataFP8RwStreamiPvii` | 0x801D2F6C | 368 | matched |  |
| `climax/GeometryData` | `PluginDataChunkWriteCallBack__13cGeometryDataFP8RwStreamiPCvii` | 0x801D30DC | 232 | matched |  |
| `climax/GeometryData` | `PluginDataChunkGetSizeCallBack__13cGeometryDataFPCvii` | 0x801D31C4 | 36 | matched |  |
| `climax/GeometryData` | `GeometryDataPluginAttach__Fv` | 0x801D31E8 | 144 | matched |  |
| `climax/GeomTypes` | `GeomTypes_Start__Fv` | 0x801D5D68 | 100 | matched |  |
| `climax/GeomTypes` | `GeomTypes_End__Fv` | 0x801D5DCC | 52 | matched |  |
| `climax/GeomTypes` | `Inverse__Q210CPlaneList7StPlaneFv` | 0x801D5E00 | 56 | matched |  |
| `climax/GeomTypes` | `__ct__10CPlaneListFv` | 0x801D5E38 | 16 | matched |  |
| `climax/GeomTypes` | `__dt__10CPlaneListFv` | 0x801D5E48 | 64 | matched |  |
| `climax/GeomTypes` | `RefPlane__10CPlaneListFP5RwV3df` | 0x801D5E88 | 288 | matched |  |
| `climax/GeomTypes` | `GetCount__10CPlaneListFv` | 0x801D5FA8 | 8 | matched |  |
| `climax/GeomTypes` | `__ct__11CVertexListFv` | 0x801D5FB0 | 72 | matched |  |
| `climax/GeomTypes` | `__dt__11CVertexListFv` | 0x801D5FF8 | 64 | matched |  |
| `climax/GeomTypes` | `RefVertex__11CVertexListFP5RwV3d` | 0x801D6038 | 348 | matched |  |
| `climax/GeomTypes` | `GetVertex__11CVertexListFi` | 0x801D6194 | 12 | matched |  |
| `climax/GeomTypes` | `GetCount__11CVertexListFv` | 0x801D61A0 | 12 | matched |  |
| `climax/GeomTypes` | `Copy__Q214CcBSP_FaceList4FaceFv` | 0x801D61AC | 284 | matched |  |
| `climax/GeomTypes` | `CalcPlane__Q214CcBSP_FaceList4FaceFP10CPlaneList` | 0x801D62C8 | 804 | matched |  |
| `climax/GeomTypes` | `CalculateArea__Q214CcBSP_FaceList4FaceFv` | 0x801D65EC | 336 | matched |  |
| `climax/GeomTypes` | `InsertVertex__Q214CcBSP_FaceList4FaceFRC5RwV3di` | 0x801D673C | 200 | matched |  |
| `climax/GeomTypes` | `GetVertex__Q214CcBSP_FaceList4FaceFi` | 0x801D6804 | 108 | matched |  |
| `climax/GeomTypes` | `GetVertexID__Q214CcBSP_FaceList4FaceFi` | 0x801D6870 | 88 | matched |  |
| `climax/GeomTypes` | `ClasifyPlane__Q214CcBSP_FaceList4FaceFPCQ210CPlaneList7StPlane` | 0x801D68C8 | 448 | matched |  |
| `climax/GeomTypes` | `ClipByPlane__Q214CcBSP_FaceList4FaceFPCQ210CPlaneList7StPlane` | 0x801D6A88 | 756 | matched |  |
| `climax/GeomTypes` | `__ct__14CcBSP_FaceListFv` | 0x801D6D7C | 16 | matched |  |
| `climax/GeomTypes` | `__dt__14CcBSP_FaceListFv` | 0x801D6D8C | 124 | matched |  |
| `climax/GeomTypes` | `AddFace__14CcBSP_FaceListFPQ214CcBSP_FaceList4Face` | 0x801D6E08 | 48 | matched |  |
| `climax/GeomTypes` | `RemoveFace__14CcBSP_FaceListFPQ214CcBSP_FaceList4Face` | 0x801D6E38 | 152 | matched |  |
| `climax/GeomTypes` | `Copy__14CcBSP_FaceListFv` | 0x801D6ED0 | 152 | matched |  |
| `climax/GeomTypes` | `CountFaces__14CcBSP_FaceListFv` | 0x801D6F68 | 32 | matched |  |
| `climax/GeomTypes` | `Sort__14CcBSP_FaceListFv` | 0x801D6F88 | 124 | matched |  |
| `climax/GeomTypes` | `SortFromSize__14CcBSP_FaceListFv` | 0x801D7004 | 92 | matched |  |
| `climax/GeomTypes` | `SplitListByPlane__14CcBSP_FaceListFPCQ210CPlaneList7StPlaneRP14CcBSP_FaceListRP14CcBSP_FaceListb` | 0x801D7060 | 604 | matched |  |
| `climax/GeomTypes` | `NewFace__14CcBSP_FaceListFv` | 0x801D72BC | 72 | matched |  |
| `climax/GeomTypes` | `DelFace__14CcBSP_FaceListFPQ214CcBSP_FaceList4Face` | 0x801D7304 | 24 | matched |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataStreamRead` | 0x801D731C | 476 | parked, 100% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataDestroy` | 0x801D74F8 | 60 | parked, 100% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataGetSize` | 0x801D7534 | 108 | parked, 99.3% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataGetArraySize` | 0x801D75A0 | 20 | parked, 100% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataGetArrayI` | 0x801D75B4 | 24 | parked, 100% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataGetArrayF` | 0x801D75CC | 24 | parked, 100% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataGetValueI` | 0x801D75E4 | 24 | parked, 100% |  |
| `climax/ClimaxS1Lib` | `ClimaxS1DataAssignAttributeF` | 0x801D75FC | 48 | parked, 100% |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1EntryAddRef__FPv` | 0x801DD340 | 16 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1EntryDestroy__FPv` | 0x801DD350 | 108 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1EntryGetName__FPCv` | 0x801DD3BC | 8 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1EntryStreamGetSize__FPCv` | 0x801DD3C4 | 40 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1EntryStreamRead__FP8RwStream` | 0x801DD3EC | 220 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1EntryStreamWrite__FPCvP8RwStream` | 0x801DD4C8 | 168 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1DictionaryGlobalDataConstructor__FPvii` | 0x801DD570 | 108 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1DictionaryGlobalDataDestructor__FPvii` | 0x801DD5DC | 56 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1DictionaryPluginAttach` | 0x801DD614 | 96 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1DictionaryGetSchema` | 0x801DD674 | 16 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1DictionarySchemaGetAnimation` | 0x801DD684 | 76 | matched |  |
| `climax/ClimaxT1Dictionary` | `ClimaxT1DictionaryGetAnimation` | 0x801DD6D0 | 60 | matched |  |
| `climax/ClimaxT1Effect` | `ClimaxT1EffectPluginAttach` | 0x801DD70C | 164 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `AddUVAnimationMaterialToListLv0__FP10RpMaterialPv` | 0x801DD7B0 | 156 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `AddUVAnimationMaterialToListLv1__FP8RpAtomicPv` | 0x801DD84C | 76 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `AddUVAnimationMaterialToListLv2__FP7RpClumpPv` | 0x801DD898 | 60 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1EffectBuildUVAnimMaterialsList4W` | 0x801DD8D4 | 128 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1EffectAddUVAnimTime` | 0x801DD954 | 112 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1EffectUVAnimUpdate` | 0x801DD9C4 | 96 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `BoundsIntegrate__FP8wbCBDataR5RwV3d` | 0x801DDA24 | 228 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1AtomicGetWorldSpaceBounds__FP8RpAtomicP8wbCBData` | 0x801DDB08 | 488 | parked, 97.2% |  |
| `climax/ClimaxT1Effect` | `wbCB__FP8RpAtomicPv` | 0x801DDCF0 | 96 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1ClumpGetWorldSpaceBounds` | 0x801DDD50 | 76 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1AtomicRenderFCB__FP8RpAtomicPv` | 0x801DDD9C | 24 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1AtomicRenderTCB__FP8RpAtomicPv` | 0x801DDDB4 | 24 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1ClumpStartRendering` | 0x801DDDCC | 28 | parked, 100% |  |
| `climax/ClimaxT1Effect` | `ClimaxT1ClumpStopRendering` | 0x801DDDE8 | 28 | parked, 100% |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameDataConstructor__FPvii` | 0x801DDE04 | 80 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameDataDestructor__FPvii` | 0x801DDE54 | 4 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameDataCopier__FPvPCvii` | 0x801DDE58 | 72 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameDataReadStream__FP8RwStreamiPvii` | 0x801DDEA0 | 208 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameDataWriteStream__FP8RwStreamiPCvii` | 0x801DDF70 | 184 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameDataGetStreamSize__FPCvii` | 0x801DE028 | 8 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FramePluginAttach` | 0x801DE030 | 160 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameUpdateCB__FP7RwFramePv` | 0x801DE0D0 | 1680 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameUpdate` | 0x801DE760 | 2024 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameHasFacingFramesCB__FP7RwFramePv` | 0x801DEF48 | 96 | matched |  |
| `climax/ClimaxT1Frame` | `ClimaxT1FrameHasFacingFrames` | 0x801DEFA8 | 88 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryHasAnimatedMaterials` | 0x801DF000 | 12 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryDataConstructor__FPvii` | 0x801DF00C | 24 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryDataDestructor__FPvii` | 0x801DF024 | 4 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryDataCopier__FPvPCvii` | 0x801DF028 | 16 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryDataReadStream__FP8RwStreamiPvii` | 0x801DF038 | 96 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryDataWriteStream__FP8RwStreamiPCvii` | 0x801DF098 | 188 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryDataGetStreamSize__FPCvii` | 0x801DF154 | 8 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryUpdate` | 0x801DF15C | 4 | matched |  |
| `climax/ClimaxT1Geometry` | `ClimaxT1GeometryPluginAttach` | 0x801DF160 | 160 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameApply__FPvPv` | 0x801DF200 | 132 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameInterpolate__FPvPvPvfPv` | 0x801DF284 | 204 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameBlend__FPvPvPvf` | 0x801DF350 | 144 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameAdd__FPvPvPv` | 0x801DF3E0 | 68 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameMulRecipCB__FPvPv` | 0x801DF424 | 68 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameStreamReadCB__FP8RwStreamP15RtAnimAnimation` | 0x801DF468 | 340 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameStreamWriteCB__FPC15RtAnimAnimationP8RwStream` | 0x801DF5BC | 336 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameStreamGetSizeCB__FPC15RtAnimAnimation` | 0x801DF70C | 12 | matched |  |
| `climax/ClimaxT1KeyFrame` | `ClimaxT1KeyFrameRegisterInterpolationScheme` | 0x801DF718 | 164 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialDataConstructor__FPvii` | 0x801DF7BC | 76 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialDataDestructor__FPvii` | 0x801DF808 | 120 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialDataCopier__FPvPCvii` | 0x801DF880 | 216 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialGetIsDoubleSided` | 0x801DF958 | 12 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialGetFrameBlendMode` | 0x801DF964 | 16 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialGetAnimation` | 0x801DF974 | 16 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialGetUVMatrices` | 0x801DF984 | 380 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialDataReadStream__FP8RwStreamiPvii` | 0x801DFB00 | 300 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialDataWriteStream__FP8RwStreamiPCvii` | 0x801DFC2C | 152 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialDataGetStreamSize__FPCvii` | 0x801DFCC4 | 8 | matched |  |
| `climax/ClimaxT1Material` | `ClimaxT1MaterialPluginAttach` | 0x801DFCCC | 160 | matched |  |
| `climax/CompAnim` | `KeyFrameStreamWrite_F__FPC15RtAnimAnimationP8RwStream` | 0x801DFD6C | 720 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamRead_F__FP8RwStreamP15RtAnimAnimation` | 0x801E003C | 732 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamWrite_P__FPC15RtAnimAnimationP8RwStream` | 0x801E0318 | 256 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamRead_P__FP8RwStreamP15RtAnimAnimation` | 0x801E0418 | 268 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamWrite_V2__FPC15RtAnimAnimationP8RwStream` | 0x801E0524 | 188 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamRead_V2__FP8RwStreamP15RtAnimAnimation` | 0x801E05E0 | 420 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamGetSize__FPC15RtAnimAnimation` | 0x801E0784 | 192 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameStreamGetSize_P__FPC15RtAnimAnimation` | 0x801E0844 | 28 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameInterpolate_F_Std__FPvPvPvfPv` | 0x801E0860 | 632 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameInterpolate_P_Std__FPvPvPvfPv` | 0x801E0AD8 | 364 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameInterpolate_F_Full__FPvPvPvfPv` | 0x801E0C44 | 828 | parked, 100% |  |
| `climax/CompAnim` | `KeyFrameInterpolate_V2_Std__FPvPvPvfPv` | 0x801E0F80 | 1144 | parked, 100% |  |
| `climax/CompAnim` | `CompressedAnimRegister__Fv` | 0x801E13F8 | 600 | parked, 100% |  |
| `climax/scalekey` | `HAnimFullKeyFrameToMatrix__FPvPv` | 0x801E1700 | 424 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameBlend__FPvPvPvf` | 0x801E18A8 | 564 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameInterpolate__FPvPvPvfPv` | 0x801E1ADC | 24 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameStreamWrite__FPC15RtAnimAnimationP8RwStream` | 0x801E1AF4 | 152 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameStreamRead__FP8RwStreamP15RtAnimAnimation` | 0x801E1B8C | 152 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameStreamGetSize__FPC15RtAnimAnimation` | 0x801E1C24 | 12 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameMulRecip__FPvPv` | 0x801E1C30 | 512 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameAdd__FPvPvPv` | 0x801E1E30 | 380 | matched |  |
| `climax/scalekey` | `HAnimFullKeyFrameRegister` | 0x801E1FAC | 180 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameApply__FPvPv` | 0x801E295C | 20 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameInterpolate__FPvPvPvfPv` | 0x801E2970 | 52 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameBlend__FPvPvPvf` | 0x801E29A4 | 52 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameAdd__FPvPvPv` | 0x801E29D8 | 36 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameMulRecipCB__FPvPv` | 0x801E29FC | 36 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameStreamReadCB__FP8RwStreamP15RtAnimAnimation` | 0x801E2A20 | 276 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameStreamWriteCB__FPC15RtAnimAnimationP8RwStream` | 0x801E2B34 | 256 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameStreamGetSizeCB__FPC15RtAnimAnimation` | 0x801E2C34 | 12 | matched |  |
| `climax/ClimaxTrailKeyFrame` | `ClimaxTrailKeyFrameRegisterInterpolationScheme` | 0x801E2C40 | 164 | matched |  |
| `climax/RwPlugin` | `ClimaxP1AtomicDataConstructor__FPvii` | 0x801F35FC | 44 | matched |  |
| `climax/RwPlugin` | `ClimaxP1AtomicDataDestructor__FPvii` | 0x801F3628 | 72 | matched |  |
| `climax/RwPlugin` | `ClimaxP1AtomicDataCopier__FPvPCvii` | 0x801F3670 | 32 | matched |  |
| `climax/RwPlugin` | `ClimaxP1AtomicDataReadStream__FP8RwStreamiPvii` | 0x801F3690 | 108 | matched |  |
| `climax/RwPlugin` | `ClimaxP1AtomicDataWriteStream__FP8RwStreamiPCvii` | 0x801F36FC | 76 | matched |  |
| `climax/RwPlugin` | `ClimaxP1AtomicDataGetStreamSize__FPCvii` | 0x801F3748 | 8 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EntryAddRef__FPv` | 0x801F3750 | 16 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EntryDestroyRef__FPv` | 0x801F3760 | 64 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EntryGetName__FPCv` | 0x801F37A0 | 12 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EntryStreamGetSize__FPCv` | 0x801F37AC | 112 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EntryStreamRead__FP8RwStream` | 0x801F381C | 624 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EntryStreamWrite__FPCvP8RwStream` | 0x801F3A8C | 4 | matched |  |
| `climax/RwPlugin` | `ClimaxP1DictionaryGlobalDataConstructor__FPvii` | 0x801F3A90 | 108 | matched |  |
| `climax/RwPlugin` | `ClimaxP1DictionaryGlobalDataDestructor__FPvii` | 0x801F3AFC | 56 | matched |  |
| `climax/RwPlugin` | `ClimaxP1DictionaryGetSchema__Fv` | 0x801F3B34 | 16 | matched |  |
| `climax/RwPlugin` | `ClimaxP1EffectPluginAttach__Fv` | 0x801F3B44 | 256 | matched |  |
| `climax/TevCBMgr` | `__ct__Q211RenderState12cTevCBHandleFf` | 0x801F4FF8 | 456 | matched |  |
| `climax/TevCBMgr` | `__dt__Q211RenderState12cTevCBHandleFv` | 0x801F51C0 | 288 | matched |  |
| `climax/TevCBMgr` | `SetCallBacks__Q211RenderState12cTevCBHandleFPFP11RwMatrixTagiPA4_f_UiPFP14StTevSetupData_UiPFP14StTevSetupData_Ui` | 0x801F52E0 | 16 | matched |  |
| `climax/TevCBMgr` | `RebuildCallBacks__Q211RenderState12cTevCBHandleFv` | 0x801F52F0 | 160 | matched |  |
| `climax/TevCBMgr` | `Internal_XForm_CB__Q211RenderState12cTevCBHandleFP11RwMatrixTagiPA4_f` | 0x801F5390 | 124 | matched |  |
| `climax/TevCBMgr` | `Internal_PreTEV_CB__Q211RenderState12cTevCBHandleFP14StTevSetupData` | 0x801F540C | 124 | matched |  |
| `climax/TevCBMgr` | `Internal_PostTEV_CB__Q211RenderState12cTevCBHandleFP14StTevSetupData` | 0x801F5488 | 132 | matched |  |
| `climax/Audio/AsyncEventSource` | `__ct__Q25Audio15AsyncEventGroupFPCc` | 0x801F8284 | 92 | matched |  |
| `climax/Audio/AsyncEventSource` | `__dt__Q25Audio15AsyncEventGroupFv` | 0x801F82E0 | 252 | matched |  |
| `climax/Audio/AsyncEventSource` | `LoadEventSource__Q25Audio15AsyncEventGroupFPCcbPQ25Audio12ESLoadParams` | 0x801F83DC | 268 | matched |  |
| `climax/Audio/AsyncEventSource` | `Init__Q25Audio15AsyncEventGroupFPQ24FMOD11EventSystemPCcPCcibb` | 0x801F84E8 | 52 | matched |  |
| `climax/Audio/AsyncEventSource` | `__dt__Q25Audio16AsyncEventSourceFv` | 0x801F851C | 140 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetName__Q25Audio16AsyncEventSourceFv` | 0x801F85A8 | 8 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetName__Q25Audio16AsyncEventSourceFPPc` | 0x801F85B0 | 12 | matched |  |
| `climax/Audio/AsyncEventSource` | `Play__Q25Audio16AsyncEventSourceFQ35Audio7Channel4Typebb` | 0x801F85BC | 372 | matched |  |
| `climax/Audio/AsyncEventSource` | `Play__Q25Audio16AsyncEventSourceFQ35Audio7Channel4Typeb` | 0x801F8730 | 20 | matched |  |
| `climax/Audio/AsyncEventSource` | `IsReady__Q25Audio16AsyncEventSourceFv` | 0x801F8744 | 32 | matched |  |
| `climax/Audio/AsyncEventSource` | `Stop__Q25Audio16AsyncEventSourceFb` | 0x801F8764 | 220 | matched |  |
| `climax/Audio/AsyncEventSource` | `Pause__Q25Audio16AsyncEventSourceFb` | 0x801F8840 | 28 | matched |  |
| `climax/Audio/AsyncEventSource` | `IsPlaying__Q25Audio16AsyncEventSourceCFv` | 0x801F885C | 132 | matched |  |
| `climax/Audio/AsyncEventSource` | `IsPlayingOrInPlayingState__Q25Audio16AsyncEventSourceCFv` | 0x801F88E0 | 148 | matched |  |
| `climax/Audio/AsyncEventSource` | `IsPaused__Q25Audio16AsyncEventSourceCFv` | 0x801F8974 | 24 | matched |  |
| `climax/Audio/AsyncEventSource` | `SetVolume__Q25Audio16AsyncEventSourceFf` | 0x801F898C | 24 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetVolume__Q25Audio16AsyncEventSourceFv` | 0x801F89A4 | 24 | matched |  |
| `climax/Audio/AsyncEventSource` | `SetPosition__Q25Audio16AsyncEventSourceFPf` | 0x801F89BC | 120 | matched |  |
| `climax/Audio/AsyncEventSource` | `SetPosition__Q25Audio16AsyncEventSourceFf` | 0x801F8A34 | 20 | matched |  |
| `climax/Audio/AsyncEventSource` | `SetVelocity__Q25Audio16AsyncEventSourceFPf` | 0x801F8A48 | 20 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetPosition__Q25Audio16AsyncEventSourceCFv` | 0x801F8A5C | 24 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetPosition__Q25Audio16AsyncEventSourceFPf` | 0x801F8A74 | 48 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetVelocity__Q25Audio16AsyncEventSourceFPf` | 0x801F8AA4 | 20 | matched |  |
| `climax/Audio/AsyncEventSource` | `Clone__Q25Audio16AsyncEventSourceFb` | 0x801F8AB8 | 20 | matched |  |
| `climax/Audio/AsyncEventSource` | `Is3D__Q25Audio16AsyncEventSourceCFv` | 0x801F8ACC | 24 | matched |  |
| `climax/Audio/AsyncEventSource` | `GetSoundType__Q25Audio11EventSourceFv` | 0x801F8AE4 | 8 | matched |  |
| `climax/Audio/AsyncEventSource` | `IsAsync__Q25Audio16AsyncEventSourceFv` | 0x801F8AEC | 8 | matched |  |
| `climax/Audio/AsyncEventSource` | `SetEventErrorCode__Q25Audio16AsyncEventSourceFQ25Audio5Error` | 0x801F8AF4 | 8 | matched |  |
| `climax/Audio/AsyncEventSource` | `ReleaseRef__Q25Audio16RefCountedObjectFv` | 0x801F8AFC | 16 | matched |  |
| `climax/Audio/AsyncEventSource` | `AddRef__Q25Audio16RefCountedObjectFv` | 0x801F8B0C | 16 | matched |  |
| `climax/Audio/AsyncEventSource` | `SetEventErrorCode__Q25Audio10EventGroupFQ25Audio5Error` | 0x801F8B1C | 4 | matched |  |
| `climax/Audio/AsyncEventSource` | `IsAsync__Q25Audio15AsyncEventGroupFv` | 0x801F8B20 | 8 | matched |  |
| `climax/Audio/Helper` | `ReleaseRef__Q25Audio16DiskSoundProjectFv` | 0x801FD268 | 36 | matched |  |
| `climax/Audio/Helper` | `MemFileOpen__5AudioFPCcPUiPQ25Audio10FileHandle` | 0x801FD28C | 288 | matched |  |
| `climax/Audio/Helper` | `MemFileClose__5AudioFPv` | 0x801FD3AC | 44 | matched |  |
| `climax/Audio/Helper` | `MemFileRead__5AudioFPvPvUiPUi` | 0x801FD3D8 | 128 | matched |  |
| `climax/Audio/Helper` | `MemFileSeek__5AudioFPvUi` | 0x801FD458 | 12 | matched |  |
| `climax/Audio/Helper` | `MemFileGetPos__5AudioFPvRUi` | 0x801FD464 | 16 | matched |  |
| `climax/Audio/Helper` | `__dt__Q25Audio16DiskSoundProjectFv` | 0x801FD474 | 204 | matched |  |
| `climax/Audio/Helper` | `DiskFileOpen__5AudioFPCcPUiPQ25Audio10FileHandle` | 0x801FD540 | 600 | matched |  |
| `climax/Audio/Helper` | `DiskFileClose__5AudioFPv` | 0x801FD798 | 100 | matched |  |
| `climax/Audio/Helper` | `DiskFileRead__5AudioFPvPvUiPUi` | 0x801FD7FC | 196 | matched |  |
| `climax/Audio/Helper` | `DiskFileSeek__5AudioFPvUi` | 0x801FD8C0 | 12 | matched |  |
| `climax/Audio/Helper` | `DiskFileGetPos__5AudioFPvRUi` | 0x801FD8CC | 16 | matched |  |
| `climax/Audio/Helper` | `AsyncDiskFileClose__5AudioFPv` | 0x801FD8DC | 208 | matched |  |
| `climax/Audio/Helper` | `AsyncCallback__5AudioFPvPcii` | 0x801FD9AC | 124 | matched |  |
| `climax/Audio/Helper` | `AsyncDiskFileRead__5AudioFPvPvUiPUi` | 0x801FDA28 | 184 | matched |  |
| `climax/Audio/Helper` | `AsyncDiskFileSeek__5AudioFPvUi` | 0x801FDAE0 | 88 | matched |  |
| `climax/Audio/Helper` | `AsyncDiskFileGetPos__5AudioFPvRUi` | 0x801FDB38 | 16 | matched |  |
| `climax/Audio/Helper` | `IsLoaded__Q25Audio16DiskSoundProjectCFv` | 0x801FDB48 | 8 | matched |  |
| `climax/Audio/Listener` | `__ct__Q25Audio8ListenerFv` | 0x801FDB50 | 12 | matched |  |
| `climax/Audio/Listener` | `Create__Q25Audio8ListenerFi` | 0x801FDB5C | 12 | matched |  |
| `climax/Audio/Listener` | `Update__Q25Audio8ListenerFfPfPfPfPf` | 0x801FDB68 | 316 | matched |  |
| `climax/Audio/Source` | `__ct__Q25Audio6SourceFv` | 0x801FDCA4 | 36 | matched |  |
| `climax/Audio/Source` | `__dt__Q25Audio6SourceFv` | 0x801FDCC8 | 176 | matched |  |
| `climax/Audio/Source` | `LoadStream__Q25Audio6SourceFPCciPQ25Audio12SourceParams` | 0x801FDD78 | 128 | matched |  |
| `climax/Audio/Source` | `Play__Q25Audio6SourceFQ35Audio7Channel4Typeb` | 0x801FDDF8 | 104 | matched |  |
| `climax/Audio/Source` | `Pause__Q25Audio6SourceFb` | 0x801FDE60 | 8 | matched |  |
| `climax/Audio/Source` | `Stop__Q25Audio6SourceFb` | 0x801FDE68 | 80 | matched |  |
| `climax/Audio/Source` | `SetPosition__Q25Audio6SourceFPf` | 0x801FDEB8 | 12 | matched |  |
| `climax/Audio/Source` | `SetVelocity__Q25Audio6SourceFPf` | 0x801FDEC4 | 16 | matched |  |
| `climax/Audio/Source` | `GetPosition__Q25Audio6SourceFPf` | 0x801FDED4 | 12 | matched |  |
| `climax/Audio/Source` | `GetStats__Q25Audio6SourceFQ25Audio9StatsTypePQ25Audio5Stats` | 0x801FDEE0 | 152 | matched |  |
| `climax/Audio/Source` | `GetVelocity__Q25Audio6SourceFPf` | 0x801FDF78 | 16 | matched |  |
| `climax/Audio/Source` | `IsPlaying__Q25Audio6SourceCFv` | 0x801FDF88 | 68 | matched |  |
| `climax/Audio/Source` | `IsPaused__Q25Audio6SourceCFv` | 0x801FDFCC | 44 | matched |  |
| `climax/Audio/Source` | `SetVolume__Q25Audio6SourceFf` | 0x801FDFF8 | 44 | matched |  |
| `climax/Audio/Source` | `Clone__Q25Audio6SourceFb` | 0x801FE024 | 8 | matched |  |
| `climax/Audio/Source` | `GetSoundType__Q25Audio6SourceFv` | 0x801FE02C | 8 | matched |  |
| `climax/Audio/System` | `__dt__Q25Audio14AsyncQueueItemFv` | 0x801FE034 | 92 | matched |  |
| `climax/Audio/System` | `GetPlatformPostfix__5AudioFv` | 0x801FE090 | 8 | matched |  |
| `climax/Audio/System` | `SFileOpen__Q25Audio6SystemFPCciPUiPPvPPv` | 0x801FE098 | 220 | matched |  |
| `climax/Audio/System` | `SFileClose__Q25Audio6SystemFPvPv` | 0x801FE174 | 80 | matched |  |
| `climax/Audio/System` | `SFileRead__Q25Audio6SystemFPvPvUiPUiPv` | 0x801FE1C4 | 108 | matched |  |
| `climax/Audio/System` | `SFileSeek__Q25Audio6SystemFPvUiPv` | 0x801FE230 | 76 | matched |  |
| `climax/Audio/System` | `__ct__Q25Audio6SystemFiiPQ25Audio12MemoryParams` | 0x801FE27C | 408 | matched |  |
| `climax/Audio/System` | `__dt__Q29ListUtils32STLList<PQ25Audio12SoundProject>Fv` | 0x801FE414 | 216 | matched |  |
| `climax/Audio/System` | `__dt__Q29ListUtils36STLList<PQ25Audio16DiskSoundProject>Fv` | 0x801FE4EC | 216 | matched |  |
| `climax/Audio/System` | `__dt__Q29ListUtils25STLList<PQ25Audio6Source>Fv` | 0x801FE5C4 | 216 | matched |  |
| `climax/Audio/System` | `__dt__Q29ListUtils30STLList<PQ25Audio10EventGroup>Fv` | 0x801FE69C | 216 | matched |  |
| `climax/Audio/System` | `__dt__Q29ListUtils32STLList<PQ25Audio12ReverbPreset>Fv` | 0x801FE774 | 216 | matched |  |
| `climax/Audio/System` | `__dt__Q29ListUtils41STLList<PQ25Audio21AsyncDiskSoundProject>Fv` | 0x801FE84C | 216 | matched |  |
| `climax/Audio/System` | `Init__Q25Audio6SystemFiiPQ25Audio12MemoryParams` | 0x801FE924 | 108 | matched |  |
| `climax/Audio/System` | `LocalInit__Q25Audio6SystemFiiPQ25Audio12MemoryParams` | 0x801FE990 | 524 | matched |  |
| `climax/Audio/System` | `SetFileOpenCallback__Q25Audio6SystemFPFPCcPUiPQ25Audio10FileHandle_i` | 0x801FEB9C | 156 | matched |  |
| `climax/Audio/System` | `SetSynchronousDiskCallbacks__Q25Audio6SystemFPFPCcPUiPQ25Audio10FileHandle_iPFPv_iPFPvPvUiPUi_iPFPvUi_iPFPvRUi_i` | 0x801FEC38 | 28 | matched |  |
| `climax/Audio/System` | `SetAsynchronousDiskCallbacks__Q25Audio6SystemFPFPCcPUiPQ25Audio10FileHandleb_iPFPv_iPFPvPvUiPUiPFPvPcii_iPvb_iPFPvUi_iPFPvRUi_iPFPv_iPFPv_i` | 0x801FEC54 | 36 | matched |  |
| `climax/Audio/System` | `SetFileSystemCallbacks__Q25Audio6SystemFPFPCcPUiPQ25Audio10FileHandle_iPFPv_iPFPvPvUiPUi_iPFPvUi_ii` | 0x801FEC78 | 152 | matched |  |
| `climax/Audio/System` | `ChannelStart__Q25Audio6SystemFPQ24FMOD7Channel` | 0x801FED10 | 16 | matched |  |
| `climax/Audio/System` | `RemoveSource__Q25Audio6SystemFPQ25Audio6Sourceb` | 0x801FED20 | 240 | matched |  |
| `climax/Audio/System` | `LoadStreamSource__Q25Audio6SystemFPCciPQ25Audio12SourceParams` | 0x801FEE10 | 264 | matched |  |
| `climax/Audio/System` | `InterpolateReverb__Q25Audio6SystemFPQ25Audio12ReverbPresetPQ25Audio12ReverbPresetf` | 0x801FEF18 | 720 | matched |  |
| `climax/Audio/System` | `UpdatePendingDeleteEventGroups__Q25Audio6SystemFv` | 0x801FF1E8 | 876 | matched |  |
| `climax/Audio/System` | `SAsyncCallback__Q25Audio6SystemFPvPcii` | 0x801FF554 | 36 | matched |  |
| `climax/Audio/System` | `AsyncCallback__Q25Audio6SystemFPvPcii` | 0x801FF578 | 336 | matched |  |
| `climax/Audio/System` | `CancelAllAsyncStreams__Q25Audio6SystemFv` | 0x801FF6C8 | 108 | matched |  |
| `climax/Audio/System` | `AddAsyncStream__Q25Audio6SystemFPQ25Audio10DiskStream` | 0x801FF734 | 36 | matched |  |
| `climax/Audio/System` | `RemoveAsyncStream__Q25Audio6SystemFPQ25Audio10DiskStream` | 0x801FF758 | 192 | matched |  |
| `climax/Audio/System` | `CheckAsyncStream__Q25Audio6SystemFPQ25Audio10DiskStream` | 0x801FF818 | 52 | matched |  |
| `climax/Audio/System` | `CancelAsyncItemForEventSource__Q25Audio6SystemFPQ25Audio16AsyncEventSource` | 0x801FF84C | 336 | matched |  |
| `climax/Audio/System` | `QueueAsyncItem__Q25Audio6SystemFPQ25Audio14AsyncQueueItem` | 0x801FF99C | 12 | matched |  |
| `climax/Audio/System` | `UpdateAsync__Q25Audio6SystemFv` | 0x801FF9A8 | 736 | matched |  |
| `climax/Audio/System` | `FlushPendingAsyncGroups__Q25Audio6SystemFv` | 0x801FFC88 | 1416 | matched |  |
| `climax/Audio/System` | `AddUpdateSound__Q25Audio6SystemFPQ25Audio11EventSource` | 0x80200210 | 204 | matched |  |
| `climax/Audio/System` | `RemoveUpdateSound__Q25Audio6SystemFPQ25Audio11EventSource` | 0x802002DC | 148 | matched |  |
| `climax/Audio/System` | `AddPendingCloseAsyncStream__Q25Audio6SystemFPQ25Audio10DiskStream` | 0x80200370 | 80 | matched |  |
| `climax/Audio/System` | `Update__Q25Audio6SystemFf` | 0x802003C0 | 492 | matched |  |
| `climax/Audio/System` | `GetDefaultListener__Q25Audio6SystemFv` | 0x802005AC | 8 | matched |  |
| `climax/Audio/System` | `LoadFirstEventSourceFromAnyGroup__Q25Audio6SystemFPCc` | 0x802005B4 | 156 | matched |  |
| `climax/Audio/System` | `IsEventGroupLoaded__Q25Audio6SystemFPCc` | 0x80200650 | 220 | matched |  |
| `climax/Audio/System` | `LoadEventGroup__Q25Audio6SystemFPCcPCcibb` | 0x8020072C | 512 | matched |  |
| `climax/Audio/System` | `ForceDeleteEventGroup__Q25Audio6SystemFPQ25Audio10EventGroupb` | 0x8020092C | 260 | matched |  |
| `climax/Audio/System` | `FreeEventGroup__Q25Audio6SystemFPQ25Audio10EventGroupb` | 0x80200A30 | 564 | matched |  |
| `climax/Audio/System` | `LoadDiskSoundProject__Q25Audio6SystemFPQ25Audio10FileHandlePCc` | 0x80200C64 | 648 | matched |  |
| `climax/Audio/System` | `RemoveAsyncPlayback__Q25Audio6SystemFPQ25Audio16AsyncEventSource` | 0x80200EEC | 148 | matched |  |
| `climax/Audio/System` | `LoadAsyncDiskSoundProject__Q25Audio6SystemFPCc` | 0x80200F80 | 848 | matched |  |
| `climax/Audio/System` | `LoadAsyncEventGroup__Q25Audio6SystemFPCcPQ25Audio16DiskSoundProjectb` | 0x802012D0 | 400 | matched |  |
| `climax/Audio/System` | `LoadDiskSoundProject__Q25Audio6SystemFPCc` | 0x80201460 | 240 | matched |  |
| `climax/Audio/System` | `LoadSoundProject__Q25Audio6SystemFPvi` | 0x80201550 | 464 | matched |  |
| `climax/Audio/System` | `FindSoundProjectFile__Q25Audio6SystemFPCc` | 0x80201720 | 232 | matched |  |
| `climax/Audio/System` | `FindDiskSoundProjectFile__Q25Audio6SystemFPCcPPQ25Audio16DiskSoundProject` | 0x80201808 | 260 | matched |  |
| `climax/Audio/System` | `FreeDiskSoundProject__Q25Audio6SystemFPQ25Audio16DiskSoundProjectb` | 0x8020190C | 416 | matched |  |
| `climax/Audio/System` | `FreeSoundProject__Q25Audio6SystemFPQ25Audio12SoundProjectb` | 0x80201AAC | 236 | matched |  |
| `climax/Audio/System` | `__dt__Q25Audio12SoundProjectFv` | 0x80201B98 | 120 | matched |  |
| `climax/Audio/System` | `LoadEventGroup__Q25Audio6SystemFPQ25Audio12SoundProject` | 0x80201C10 | 28 | matched |  |
| `climax/Audio/System` | `FindReverbPreset__Q25Audio6SystemFPCc` | 0x80201C2C | 112 | matched |  |
| `climax/Audio/System` | `SetReverbPreset__Q25Audio6SystemFPQ25Audio12ReverbPresetb` | 0x80201C9C | 252 | matched |  |
| `climax/Audio/System` | `SetReverb__Q25Audio6SystemFPCc` | 0x80201D98 | 268 | matched |  |
| `climax/Audio/System` | `AddReverbPreset__Q25Audio6SystemFPQ25Audio12ReverbPreset` | 0x80201EA4 | 156 | matched |  |
| `climax/Audio/System` | `CanDeleteEventSource__Q25Audio6SystemFPQ25Audio11EventSourceb` | 0x80201F40 | 228 | matched |  |
| `climax/Audio/System` | `IsLoaded__Q25Audio21AsyncDiskSoundProjectCFv` | 0x80202024 | 40 | matched |  |
| `climax/Audio/System` | `IsAsync__Q25Audio21AsyncDiskSoundProjectCFv` | 0x8020204C | 8 | matched |  |
| `climax/Audio/System` | `__dt__Q25Audio21AsyncDiskSoundProjectFv` | 0x80202054 | 124 | matched |  |
| `climax/Time` | `Open__Q23RWS4TimeFv` | 0x802020D0 | 4 | matched |  |
| `climax/Time` | `GetTime__Q23RWS4TimeFv` | 0x802020D4 | 72 | matched |  |
| `climax/Time` | `GetSysTime__Q23RWS4TimeFv` | 0x8020211C | 4 | matched |  |
| `climax/Time` | `DifSysTimes__Q23RWS4TimeFxx` | 0x80202120 | 12 | matched |  |
| `climax/Time` | `SysTimeToSeconds__Q23RWS4TimeFx` | 0x8020212C | 80 | matched |  |
<!-- ledger:end game -->
