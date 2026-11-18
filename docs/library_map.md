# Library map of `main.dol` (R5WEA4)

Rough layout of `.text` (0x800067A0-0x803D8360, 13,940 functions, 3,992,572 bytes) and the main data sections, by
library. **Every boundary below is approximate** (typically within a few functions where a named anchor exists,
within a few KB elsewhere). Nothing here is a split yet; the candidate translation-unit (TU) boundaries in
`ref/R5WEA4/tu_boundaries_strict.tsv` are what splits will be cut from.

Confidence: **high** = names, banners or source-file strings pin the range; **medium** = several independent
data/string clues agree but the edges are estimated; **low** = inferred from link order, data ordering or size.

## How this was built

- `build/tools/query.exe strings <unit>` lists the string literals each function of a unit addresses (it replaced the
  one-off `fn_strings.py`, which wrote one row per function: address, size, name, dtk unit, strings).
  Only 1,127 of 13,957 functions reference a recoverable string, and only ~360 carry a dtk signature name, so most
  evidence is indirect.
- Link-order ordering. The linker lays out `.sdata2` (float constants), `.sdata`, `.sbss`, `.bss`, `.rodata` and
  `.data` in the same TU order as `.text`, so the data an address range *owns* (referenced only from inside it) is a
  second, independent view of the same boundaries. The data columns in the table are that owned data (min-max of
  objects referenced only from the range; `n` objects).
- SDK banners (`<< RVL_SDK - X release build >>`, all `(0x4302_145)`, Feb 27 / Jun 22 2009 except HBM `0x4201_142`
  Mar 3 2009) are in `.data` in lib link order: THP, EXI, SI, OS, DVD, VI, AI, AX, DSP, GX, NAND, SC, (BTE), KPAD,
  WPAD, HBM. This fixed the SDK order.
- Source-file strings in the DOL: `rvl.cpp`, `OSReset.c`, `OSThread.c`, `OSStateTM.c`, `OSPlayTime.c`, `vi3in1.c`,
  `dvdfs.c`, `dvd.c`, `arc.c`, `TPL.c`, `WUD.c`, `fmod*.cpp` (see below). No RenderWare, MSL, libjpeg or
  Climax game `__FILE__` paths exist.
- Haiku subagents scanned the string/name rows in four address slices for library hints; their findings were
  cross-checked against the data-ownership view and against `fn_strings.tsv`.

## Section layout (from the DOL header)

| section | start | end | note |
|---|---|---|---|
| `.init` | 0x80004000 | 0x800066E0 | crt (`__start`, `memcpy`, `memset`, `__init_registers`...); MetroTRK vectors at 0x80004000 |
| `.text` | 0x800067A0 | 0x803D8360 | 13,940 functions |
| `.ctors` / `.dtors` | 0x803D8360 / 0x803D8760 | 0x803D8760 / 0x803D8780 | |
| `.rodata` | 0x803D8780 | 0x803DEE80 | 266 symbols; jump tables, JPEG message table (0x803DA500), HBM strings |
| `.data` | 0x803DEE80 | 0x80457440 | game strings first (0x803DEF20-0x80427280), then libs in link order |
| `.bss` | 0x80457480 | 0x80526720 | |
| `.sdata` / `.sbss` | 0x80526720 / 0x80529100 | 0x80529100 / 0x8052B5E0 | |
| `.sdata2` / `.sbss2` | 0x8052B5E0 / 0x80530080 | 0x80530080 / 0x805300B4 | |

## Range table

Start and end are the range's first and last-covered addresses (end exclusive). `fn` = functions starting inside the range.
Link order runs top to bottom: game code first, then platform glue, MetroTRK, MSL, then the Climax middleware block,
then the RVL SDK, then RenderWare, RW Physics, FMOD, HBM. Data columns give the owned `.data` / `.sdata2` ranges.

### Game and Climax code

| range name | start | end | fn | bytes | confidence | evidence | owned data |
|---|---|---|---|---|---|---|---|
| G1 core, collision, anim, zones | 0x800067A0 | 0x80033D34 | 727 | 185,748 | medium (content), low (edges) | `CCollisionCapsule` 0x8000CFD8, `CAnimBehaviour` 0x80017570, `CAnimSkeleton` 0x8001A0A4, `CZone*` 0x8002598C, `SaveData` 0x80012E18, `banner.bin` 0x80016074, NAND_07/08 | `.data` 0x803DEF20-0x803F5F10; `.sdata2` 0x8052B5E0-0x8052B8FC |
| G2 audio, characters, enemies | 0x80033D34 | 0x80073554 | 722 | 260,128 | medium / low | `Audio.arc` 0x800341C0, `%s.snd`, `AFX_MixerSettings.csv`, `CCharacter` 0x800436D8, `CEnemy` 0x80052E58, `IGC` 0x80043658 | `.data` 0x803E0AF0-0x803E6248; `.sdata2` 0x8052B900-0x8052E9BC |
| G3 player, fx, scene, IGC | 0x80073554 | 0x800C5000 | 1,467 | 334,588 | medium / low | `CPlayer` 0x80073554, `cIce` 0x80087D88, `cSnow`, `CFogConfig` 0x80092990, `CSceneGraphContainer*` 0x8009F4B4, `CPhysicsObject` 0x800A4E8C, `CIGCBorderControl` 0x800A88B4, `THP` string 0x8009AA00 | `.data` 0x803E5EE0-0x803EBA7C; `.sdata2` 0x8052BDF0-0x8052C700 |
| G4 triggers, UI, save, photo | 0x800C5000 | 0x80100000 | 966 | 241,624 | medium / low | `CPointTrigger` 0x800C62B0, `CMessageRelay` 0x800CB728, `GameSave` 0x800DB344, `COnScreenText` 0x800D7888, `UI_MobileMainMenu.xaml` 0x800E70DC, `Jpg` photo strings 0x800CBFE4-0x800F31A8, `WiiMote` 0x800B3310 | `.data` 0x803EBA44-0x803F2314; `.sdata2` 0x8052C708-0x8052D910 |
| G5 XAML UI, camera, AI | 0x80100000 | 0x80144000 | 870 | 278,672 | medium / low | `Storyboard`, `Window.Resources`, `Path=` parser 0x801021C8, `CAnimatedCamera` 0x801136A0, `CSimpleStaticCamera` 0x8011CD90, `CAIWorldBehaviour` 0x8012BC44, NavMesh text 0x80137D60 | `.data` 0x803F2418-0x803F67C0; `.sdata2` 0x8052CE18-0x8052D4E8 |
| G6 fx, psych, puzzles | 0x80144000 | 0x80179000 | 744 | 217,676 | medium / low | `CarJourney` 0x801447C0, `CFlare` 0x80148380, `AFX_UI*` 0x8014E178, `Psych.arc`, `TensionBehaviour` 0x801520A4, `CWheelchair` 0x80158B78, `CButtonPuzzle` 0x8015AF28, `CTwistPuzzle` 0x80177760, `CSUV` 0x80173400 | `.data` 0x803F6800-0x803FB9F8; `.sdata2` 0x8052D4F0-0x8052DC10 |
| G7 therapy, traversal, map | 0x80179000 | 0x8018F8C4 | 328 | 91,584 | medium / low | `CPlayerResponse` 0x8017A944, `CQuestionnaire` 0x8017B2F4, `CObstacle` 0x8017F718, `TT_SyncGroup`, `FUSEBD` 0x80187504, `CMapLayer` 0x8018AD40, `UI_Map.rws`, `HomeMenu*` strings 0x8018E0B4-0x8018ED24 (game-side home menu glue, not HBM) | `.data` 0x803FBB10-0x80427xxx; `.sdata2` 0x8052DC14-0x8052DDE8 |
| platform glue (`rvl.cpp`) | 0x8018F8C4 | 0x801900E8 | 6 | 2,084 | high | `main` 0x8018F8C4 (dtk), string `rvl.cpp`, `dvd:` 0x8018FC3C, `opening.bnr` 0x8018FEA8 | |
| Climax core: RWS, streaming, handlers | 0x801A70B8 | 0x801B4354 | 241 | 53,916 | low | `UnRegisterd Class` 0x801A7574, `CEventHandler*` 0x801A9310, `rwID_RWS/CLUMP/ATOMIC/TEXDICTIONARY` 0x801AB4C4-0x801AEFEC, `Stream Still Caching` 0x801B1E3C, `Async.Arc`, `DATA.ARC`, `IGC.ARC` | `.data` 0x80428158-0x80428418; `.sdata2` 0x8052E408-0x8052E438 |
| Climax XML, config, typed assets, movies, rw tables | 0x801CCD24 | 0x80209D54 | 959 | 249,904 | low | XML parse errors 0x801CCD6C ("Expected '=' ..."), `BootStrings`/`CONFIG` 0x801D197C-0x801D26E0, `ClimaxT1Entry` 0x801DD3EC / `ClimaxP1Entry` 0x801EE23C, `iMsg*` and `movies\` 0x80203008-0x8020382C, `rwID_*`/`rwpID_*` chunk-ID registration 0x80209044-0x80209D3C | `.data` 0x80429A18-0x8042B600; `.sdata2` 0x8052B770-0x8052EAB0 (mixed) |

Notes: the G-range edges are cuts at subsystem clusters chosen to give roughly equal lanes; the subsystem order is
real (first-seen addresses above), but the true edges are only known per TU. The Climax names in the middle block
(0x801A70B8-0x80209D54) share one link-order position with libjpeg/zlib below, so these may be several engine
libraries (stream, XML, config, asset tables) linked after MSL rather than part of the game proper.

### Toolchain runtime, MetroTRK, MSL C

| range name | start | end | fn | bytes | confidence | evidence | owned data |
|---|---|---|---|---|---|---|---|
| crt in `.init` | 0x80004000 | 0x800066E0 | 11 + vectors | | high | `__start`, `__init_registers`, `__init_data`, `__flush_cache`, `memcpy`, `memset`, `__fill_mem` (dtk names) | |
| RVL SDK `NdevExi2AD` (debugger EXI2 driver) | 0x801900E8 | 0x80190BEC | 31 | 2,820 | high | split units `revolution/NdevExi2AD/DebuggerDriver.c` (0x801900E8-0x80190618: `__DBMtrHandler` ... `DBClose`, the ODEMU and mailbox/RAM helpers) and `exi2.c` (0x80190618-0x80190BEC: `__EXI2Select` ... `__EXISwap32`); names and layout from doldecomp/ogws; built with `-O0` (exceptions.md, "MetroTRK rulings (msl-trk-6)"). Linked between the platform glue and MetroTRK, whose gdev layer calls its `DB*` API | `.sdata` 0x80528650-0x80528658; `.sbss` 0x8052A478-0x8052A490 |
| MetroTRK (incl. `gdev_cc_*`) | 0x80190BEC | 0x80195D28 | 143 | 20,796 | high | `TRKAccessFile`..`TRKPositionFile` (four `twui` stubs, tp `targsupp`) 0x80190BEC; `gdev_cc_*` 0x80190C0C; `TRK_main` 0x80191134 ... `TRKTargetSetInputPendingPtr` 0x80195C70; `SetUseSerialIO`/`GetUseSerialIO` (target_options) end at 0x80195D28, where MSL `alloc.c` starts; strings `MetroTRK : ...`, `MetroTRK for Revolution v0.4` (0x80427340-0x80427568); EXI hooks `TRKEXICallBack` 0x8019198C. Unit map below | `.data` 0x80427290-0x80427578; `.rodata` 0x803D9270-0x803D92F8 |
| MSL C | 0x80195D28 | 0x801A6248 | 147 | 66,848 | high | `alloc.c` from 0x80195D28 (split), `__close_all` 0x80197160, `ansi_fp.c` 0x80197270-0x80198AF0 (split), `__prep_buffer`/`__flush_buffer`/`__fwrite` 0x80198B00-0x80198BE0, `_fseek`, `wcstombs`, `memmove`/`memchr`, printf core (`parse_format`, `long2str`, `double2hex`, `float2str`, `__pformatter` 0x8019B15C), `vsnprintf`, `sprintf`, `strcpy`, `strchr`, `fwide`, `strlen` 0x801A622C | `.data` 0x80427578-0x804280E8; `.rodata` 0x803D9310-0x803DA210 |
| Metrowerks runtime (`Runtime.PPCEABI.H`) | 0x801A6248 | 0x801A70B8 | 27 | 3,696 | high | `__va_arg`, `__register_global_object`, `__destroy_global_chain`, `__save_fpr/__restore_fpr/__save_gpr/__restore_gpr`, `__div2u`, `__mod2u`, `__init_cpp_exceptions`, `__fini_cpp_exceptions`, `__register_fragment`, `__unregister_fragment`; string `Metrowerks CW runtime library initializing default heap` 0x803DA2F6 (functions 0x801A70B8, 0x801A7170) | `.rodata` 0x803DA2A8; `.sbss` 0x8052A518 |

MetroTRK unit map (`.text`; units and paths from zeldaret/tp `TRK_MINNOW_DOLPHIN`; status as in `configure.py`):

| unit | start | end | status |
|---|---|---|---|
| (unsplit) tp `Os/dolphin/targsupp` (`TRKAccessFile`, `TRKOpenFile`, `TRKCloseFile`, `TRKPositionFile`) | 0x80190BEC | 0x80190C0C | dtk asm |
| `gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main.c` | 0x80190C0C | 0x80190E84 | Matching |
| `gamedev/cust_connection/utils/gc/MWCriticalSection_gc.c` | 0x80190E84 | 0x80190EC0 | Matching |
| `gamedev/cust_connection/utils/common/CircleBuffer.c` | 0x80190EC0 | 0x801910FC | Matching |
| `Processor/ppc/Generic/flush_cache.c` | 0x801910FC | 0x80191134 | Matching (ORIGINAL-ASM) |
| `Portable/main_TRK.c` | 0x80191134 | 0x80191170 | Matching |
| `Portable/mainloop.c` | 0x80191170 | 0x8019125C | Matching |
| `Portable/mem_TRK.c` | 0x8019125C | 0x801914C0 | NonMatching |
| `Portable/dispatch.c` | 0x801914C0 | 0x801915E0 | Matching |
| (unsplit) tp `Os/dolphin/dolphin_trk.c` (`InitMetroTRK` ... `InitMetroTRKCommTable`), `dolphin_trk_glue.c` (`TRKUARTInterruptHandler` ... `InitializeProgramEndTrap`), `Portable/notify.c` (`TRKDoNotifyStopped`) | 0x801915E0 | 0x80191D30 | dtk asm |
| `Portable/nubevent.c` | 0x80191D30 | 0x80191ECC | Matching |
| `Portable/nubinit.c` | 0x80191ECC | 0x8019201C | Matching |
| `Portable/serpoll.c` | 0x8019201C | 0x80192164 | Matching |
| `Portable/string_TRK.c` | 0x80192164 | 0x80192180 | Matching |
| `Portable/support.c` | 0x80192180 | 0x801927A4 | Matching |
| `Os/dolphin/targcont.c` | 0x801927A4 | 0x801927D8 | Matching |
| `Processor/ppc/Generic/mpc_7xx_603e.c` | 0x801927D8 | 0x80192B00 | Matching (ORIGINAL-ASM) |
| `Portable/msg.c` | 0x80192B00 | 0x80192B64 | Matching |
| `Portable/msgbuf.c` | 0x80192B64 | 0x80193370 | Matching |
| `Portable/msghndlr.c` | 0x80193370 | 0x801942F4 | Matching |
| `Export/mslsupp.c` | 0x801942F4 | 0x801944F4 | Matching |
| `Processor/ppc/Generic/targimpl.c` | 0x801944F4 | 0x80195D18 | NonMatching (31/33) |
| `Os/dolphin/target_options.c` | 0x80195D18 | 0x80195D28 | Matching |

Already split (config/R5WEA4/splits.txt): `Runtime.PPCEABI.H/global_destructor_chain.c` (0x801A6310-0x801A6370) and
`Runtime.PPCEABI.H/__init_cpp_exceptions.cpp` (0x801A6FD4-0x801A7044).

### Compression and imaging middleware (inside the Climax middleware block)

| range name | start | end | fn | bytes | confidence | evidence | owned data |
|---|---|---|---|---|---|---|---|
| Climax engine: compressed-stream class | 0x801B4354 | 0x801B67FC | 54 | 9,384 | medium | an `__sinit` registering a global object (vtable `.data` 0x80428424) and a stream class whose 0x801B4464 calls `inflateInit_(strm, "1.2.3", 0x38)`, then `inflate`/`inflateEnd`; C++, not zlib (no deflate is linked) | `.data` 0x80428424-0x80428478 |
| Climax engine: unidentified C++ helpers | 0x801B67FC | 0x801B86C8 | 24 | 7,884 | low | not IJG: `fn_801B67FC(const char*, int)` copies a string into `operator new[]` storage, `fn_801B6D50` frees with `operator delete[]`; callers are game code (`unk_MessageInbox`, triggers); the two large functions `fn_801B6EC8`/`fn_801B7B9C` call only `fn_802A0548` (RenderWare range) | |
| IJG libjpeg | 0x801B86C8 | 0x801C9624 | 176 | 69,468 | high (first unit `jcomapi.c`, `jpeg_abort`) | `jpeg_std_message_table` pointer array at `.rodata` 0x803DA500, referenced from `jerror` 0x801C3D80; message strings 0x804284A8-0x80429650 ("Bogus Huffman table definition", "Premature end of JPEG file"...); `JPEGMEM` getenv 0x801C60C0 (jmemansi/jmemname); 10 rodata objects (quant/huffman/zigzag tables) 0x803DA340-0x803DA808 | `.rodata` 0x803DA340-0x803DA808 |
| zlib 1.2.3, inflate side | 0x801C9624 | 0x801CCA78 | 14 | 13,396 | high | units `adler32.c`, `crc32.c`, `inffast.c`, `inflate.c`, `inftrees.c`, `zutil.c` in alphabetical link order, all Matching (lane zlib-1): `inflate` 0x801CAAC0 ("incorrect header check", "invalid block type"...), `inflate_fast` 0x801CA250, `inflateInit2_` 0x801CA7E4 (`"1.2.3"`), `crc_table` and `inftrees` tables `.rodata` 0x803DA948-0x803DD2F0 | `.data` 0x80429750-0x804299A0 |
| unidentified library | 0x801CCA78 | 0x801CCD24 | 6 | 684 | low | `GetParent`-`fn_801CCC58`, called from game code; not zlib | |
| THP player | 0x80209D80 | 0x8020D780 | 17 | 14,848 | high | `THPDec.c` 0x80209D80-0x8020D20C (11 C functions + 5 ORIGINAL-ASM: the paired-single IDCTs and the Huffman DCT decoders) and `THPAudio.c` 0x8020D210-0x8020D778 (`THPAudioDecode`), reconstructed from doldecomp/mkdd `libs/dolphin/thp` (lane sdk-av-2); banner `RVL_SDK - THP` (0x8042B618) sits immediately before the EXI banner in `.data`. `fn_80209D54` (0x20 bytes, 0x80209D54-0x80209D74) is game code, not THP. Alignment pads `pad_03_8020D20C_text` (4) and `pad_03_8020D778_text` (8) | `.rodata` 0x803DD4F0-0x803DD580; `.data` 0x8042B618-0x8042B660; `.bss` 0x80491E00-0x80491F50 |

Only zlib's inflate side is linked; the game reaches it through the Climax stream class at 0x801B4354.

### RVL SDK

Order from banners and dtk-named anchors. The lowest-level anchors (`OSInit` 0x80211350, `GXInit` 0x80237B60, ...) are
dtk signature names; the sub-library edges are estimates.

| range name | start | end | fn | bytes | confidence | evidence | owned data |
|---|---|---|---|---|---|---|---|
| EXI | 0x8020D780 | 0x8020F550 | 22 | 7,484 | high start | `SetExiInterruptMask`, `EXIImm`, `EXIDma`, `EXISync`, `EXIInit`, `EXILock`, `EXIGetID`, `__OSEnableBarnacle` 0x8020F200; banner EXI | `.sbss` 0x8052AA08-0x8052AA1C |
| SI | 0x8020F550 | 0x80210680 | 11 | 4,328 | high | `SIInterruptHandler` 0x8020F850, `SIInit`, `SITransfer`, `SIGetType`, `SISetSamplingRate`; banner SI | `.data` 0x8042B6F0-0x8042B718 |
| OS (incl. PPCArch) | 0x80210680 | 0x8021E8B0 | 277 | 56,188 | high | `PPCMfmsr`..`PPCMthid4` 0x80210680-0x80210890, `OSInit` 0x80211350, `OSExceptionInit`, `__OSInitAlarm`, arena/cache/context/error/exec/fatal/font/interrupt/memory/reset/rtc/thread/time/IPC/state/net/playtime functions; strings `OSReset.c`, `OSThread.c`, `OSStateTM.c`, `OSPlayTime.c`; banner OS | `.data` 0x8042B7B0-0x80439BD4; `.sdata2` 0x8052EAE0-0x8052EB38 |
| `__ppc_eabi_init` + DB | 0x8021E8B0 | 0x8021EA70 | 8 | 448 | high | `__init_user`, `__init_cpp`, `exit` 0x8021E8B0-0x8021E970; `db.c` 0x8021E970-0x8021EA70 (`DBInit`..`DBPrintf`, 5 functions, reconstructed) | |
| MTX (SDK `mtx.a`) | 0x8021EA70 | 0x8021EFA0 | 11 | 1,328 | high | The Gormiti link map holds all 11 functions in `mtx.a`: `mtx.o` (`PSMTXIdentity` 0x8021EA70 .. `PSMTXScale` 0x8021EE70, 9 functions), `mtxvec.o` (`PSMTXMultVec` 0x8021EEA0, called from KPAD and WPADHIDParser) and `mtx44.o` (`C_MTXOrtho` 0x8021EF00, 0x98 bytes, plain C, no paired-single instructions). The `PSMTX*` bodies are `psq_l`/`psq_st`/`ps_merge` matrix and vector code, also called from game code (e.g. 0x80097600, 0x8009A0E4). Linked between DB and DVD. | |
| DVD | 0x8021EFA0 | 0x80227F50 | 118 | 36,040 | high start, medium end | `__DVDFSInit`, `DVDInit` 0x8021F9D0, `DVDLowInit`, `DVDLowUnencryptedRead`...; strings `dvdfs.c`, `dvd.c`; banner DVD | `.data` 0x80439C08-0x8043AB78; `.bss` 0x80493440-0x80498540 |
| VI | 0x80227F50 | 0x8022CA00 | 44 | 19,120 | high | three units: `vi.c` 0x80227F50-0x8022A990 (26 functions; `OnShutdown` first, `VIInit` 0x802297A0), `i2c.c` 0x8022A990-0x8022B2A4 (`WaitMicroTime`, `sendSlaveAddr`, `__VISendI2CData`), `vi3in1.c` 0x8022B2B0-0x8022C9FC (15 functions, `__VISetRGBModeImm` 0x8022C500); strings `vi.c`, `vi3in1.c`; banner VI. All reconstructed; `vi.c` parked (`__VIInit` MATCH-PENDING), i2c and vi3in1 Matching | `.data` 0x8043BA20-0x8043C588; `.bss` 0x80498560-0x804986F0; `.sdata` 0x80528AA8-0x80528AE0; `.sbss` 0x8052ACA0-0x8052AD70 |
| PAD | 0x8022CA00 | 0x8022CA60 | 1 | 92 | high | `revolution/pad/Pad.c` (reconstructed): only `__PADDisableRecalibration`, plus `__PADSpec` (`.sbss` 0x8052AD70); the rest of the PAD library is not linked | `.sbss` 0x8052AD70-0x8052AD74 |
| AI | 0x8022CA60 | 0x8022D030 | 12 | 1,484 | high | `revolution/ai/ai.c` (reconstructed): `AIRegisterDMACallback`..`__AI_SRC_INIT`; banner AI 0x8043C588 (`__AIVersion`) | `.data` 0x8043C588-0x8043C5D0; `.sbss` 0x8052AD78-0x8052ADB8 |
| AX | 0x8022D030 | 0x802309D0 | 90 | 14,752 | high start, medium end | `AX.c`, `AXAlloc.c`, `AXAux.c`, `AXCL.c`, `AXOut.c` (0x8022D030-0x8022F1C8, banner AX 0x8043C5D0), then `AXSPB.c` 0x8022F1D0-0x8022F60C (7), `AXVPB.c` 0x8022F610-0x80230988 (24, per-object `-fp_contract off`) and `AXProf.c` 0x80230990-0x802309D0 (1), all reconstructed from doldecomp/ogws (lane sdk-av-2), with the data units `AXComp.c` (`__AXCompressorTable` 0x8043C6C0) and `DSPCode.c` (`axDspSlave` 0x8043D680, the DSP microcode image, plus its vectors). The rest of the range is AXFX (0x802309D0-0x802334B4) and MIX (0x802334C0-0x80236C10; once listed as AXART), the next two rows | `.data` 0x8043C5D0-0x80440680; `.bss` 0x804986F0-0x804AE000 |
| AXFX | 0x802309D0 | 0x802334B4 | 34 | 10,980 | confirmed (ogws/tp names; the Std units modelled) | lane sdk-axfx: `AXFXReverbHi.c` 0x802309D0-0x80230A74 (3), `AXFXReverbHiDpl2.c` 0x80230A80-0x80230B64 (3), `AXFXReverbHiExp.c` 0x80230B70-0x80231994 (7), `AXFXReverbHiExpDpl2.c` 0x802319A0-0x80232884 (7), `AXFXReverbStd.c` 0x80232890-0x80232924 (3, unit name unconfirmed), `AXFXReverbStdExp.c` 0x80232930-0x8023346C (7, unit name unconfirmed; parked), `AXFXHooks.c` 0x80233470-0x802334B4 (4). No Chorus or Delay in retail (the linker stripped every AXFX function the game does not reach, among them all `*GetMemSize` and `*Settings`). The three Exp units are built `-fp_contract off` | `.data` 0x8043F680-0x8043FA70; `.sdata` 0x80528AF8-0x80528B00 (hooks); `.sdata2` 0x8052EB88-0x8052EC60 |
| MIX | 0x802334C0 | 0x80236C10 | 19 | 14,160 | high (tp `libs/dolphin/src/mix/mix.c`; the remote half has no public source) | lane sdk-axart: `mix.c` 0x802334C0-0x80236714 (16; tp's GameCube MIX plus the Wii auxC bus, DPL2 rear slots and the remote-speaker channel table) and `mix_rmt.c` 0x80236720-0x80236C08 (3: `MIXRmtSetVolumes`, `__MIXUpdateRmtChannel`, `__MIXResetRmtChannel`; unit name unconfirmed, names invented). The range was listed as AXART, but tp's `axart` is absent from retail (no AXART function is identified anywhere in the DOL, and every function of this range is MIX); this is the MIX mixer | `.data` 0x8043FA70-0x80440600 (volume, pan and DPL2 tables); `.bss` 0x804AE000-0x804B2380 (channel tables); `.sbss` 0x8052AEF8-0x8052AF10 |
| DSP | 0x80236C10 | 0x802377A0 | 15 | 2,896 | high | `dsp.c` 0x80236C10-0x80236EAC (9; tp order with `DSPCancelTask`), `dsp_debug.c` 0x80236EB0-0x80236F00 (`__DSP_debug_printf`) and `dsp_task.c` 0x80236F00-0x80237798 (5; parked: `__DSP_boot_task` MATCH-PENDING R17), reconstructed from zeldaret/tp (lane sdk-av-2); GX begins at 0x802377A0 (`__GXDefaultTexRegionCallback`, first function of `GXInit.c`) | `.data` 0x80440600-0x804407C0 |
| GX | 0x802377A0 | 0x8023F0D4 | 181 | 29,924 | confirmed | 14 units reconstructed from zeldaret/tp (GXInit..GXPerf, `config/R5WEA4/splits.txt`); `GXInit` 0x80237B60, `__GXInitGX` 0x80238070 (banner) | `.data` 0x804407C0-0x80440F40 |
| RVL SDK MEM | 0x8023F0E0 | 0x802401D0 | 23 | 4,260 | confirmed | split units `revolution/mem/mem_heapCommon.c`, `mem_expHeap.c`, `mem_allocator.c`, `mem_list.c` (zeldaret/tp, ogws); `MEMiInitHeapHead` ... `MEMGetNextListObject` 0x802401B0 | |
| RVL SDK EXI UART debug print (`euart`) | 0x802401D0 | 0x80240550 | 3 | 884 | confirmed | split unit `revolution/euart/euart.c` 0x802401D0-0x80240550 (zeldaret/tp `libs/revolution/src/euart/euart.c`): `EUARTInit` 0x802401D0, `InitializeUART` 0x802402D0, `WriteUARTN` 0x80240320; matched. Reads `__EXIFreq` (.sdata2 0x8052EAD8, owner open) | `.sbss` 0x8052AF98-0x8052AFA8 |
| IPC / ISFS / IOS | 0x80240550 | 0x80243C80 | 61 | 13,740 | confirmed | split units `revolution/fs/fs.c` (0x80240550, `ISFS_OpenLib` ... `ISFS_ShutdownAsync`), `ipc/ipcMain.c` (0x80241B80-0x80241C68), `ipc/ipcclt.c` (0x80241C70, `strnlen`, `IPCCltInit`, `IOS_*`), `ipc/memory.c` (0x80243570, `iosCreateHeap` ...), `ipc/ipcProfile.c` (0x80243AA0-0x80243C7C) | |
| NAND | 0x80243C80 | 0x80247450 | 65 | 13,832 | confirmed | split units, matched (zeldaret/tp): `revolution/nand/nand.c` 0x80243C80-0x80244AF4, `NANDOpenClose.c` 0x80244B00-0x80245B4C (`nandOpen`, `NANDPrivateOpenAsync`), `NANDCore.c` 0x80245B50-0x8024698C (`NANDInit` 0x802462A0), `NANDCheck.c` 0x80246990-0x80246E5C, `NANDLogging.c` 0x80246E60-0x80247444 (`NANDLoggingAddMessageAsync`); string `Failed to set home directory.`; banner NAND | `.data` 0x80440FF8-0x804412DC |
| SC + ESP + ARC | 0x80247450 | 0x8024A790 | 63 | 12,680 | confirmed | SC split units, matched: `revolution/sc/scsystem.c` 0x80247450-0x80248DD8 (`SCInit`), `scapi.c` 0x80248DE0-0x80249228 (from `SCGetAspectRatio`), `scapi_prdinfo.c` 0x80249230-0x802494B4; then game audio code `fn_802494B4` (0x802494B4-0x80249790, unassigned) between SC and ESP; `revolution/esp/esp.c` 0x80249790 (`ESP_InitLib` ...) and `revolution/arc/arc.c` 0x80249DB0-0x8024A788 (string `arc.c`) matched; banner SC | `.data` 0x804412E0-0x8044175C |
| Bluetooth stack (BTE: bta, btm, hid, l2cap, rfcomm, sdp, gap, btu) | ~0x8024A790 | 0x80277F60 | 731 | 185,792 | high | starts after arc with the GKI buffer functions (`fn_8024A790` ...); ~230 debug-trace strings: `bta_dm_*`, `bta_hh_*`, `BTM_*`, `Security Manager:`, `L2CAP -`, `PORT_*`, `rfc_*`, `SDP -`, `HID -`, `GAP_CONN` | `.data` 0x80441780-0x80446D38; `.rodata` 0x803DD870-0x803DDC20 |
| TPL | 0x80277F60 | 0x802780A0 | 2 | 320 | high | one unit `revolution/tpl/TPL.c` (xbret/xenoblade, ThePlayerRolo/sdk_2009-12-11): `TPLBind` 0x80277F60, `TPLGet` 0x80278080; string `TPL.c` (`invalid version number for texture palette`) | `.data` 0x80446D58-0x80446D88; `.sdata` 0x80528DA0-0x80528DA8 |
| KPAD | 0x802780A0 | 0x8027C098 | 29 | 16,376 | high | one unit `revolution/kpad/KPAD.c` (parked, 22 of 29 functions at 100%; per-object `-fp_contract off`): `KPADSetPosParam` 0x802780A0 .. `KPADSetPosPlayMode` 0x8027C080; banner KPAD 0x80446D88; `WARNING: Overwritten the callback needed by KPAD` is in WPAD | `.data` 0x80446D88-0x80446DE8; `.bss` 0x804EFD58-0x804F13C0; `.sdata` 0x80528DA8-0x80528E28; `.sbss` 0x8052B058-0x8052B090; `.sdata2` 0x8052EDB0-0x8052EE20 |
| USB (IOS USB client) | 0x8027C0A0 | 0x8027D5F0 | 14 | 5,456 | high | one unit `revolution/usb/usb.c` (xbret/xenoblade, zeldaret/tp, PrimeDecomp/sdk): `USB_LOG` 0x8027C0A0 .. `IUSB_WriteCtrlMsgAsync` 0x8027D560; string `USB: ` (`.sdata` 0x80528E30). Followed by WPAD's two 8-byte stubs `noAlloc`/`noFree` | `.data` 0x80446DE8-0x80447618; `.sdata` 0x80528E28-0x80528E38; `.sbss` 0x8052B090-0x8052B0A0 |
| WPAD | 0x8027D680 | 0x8028A4C0 | 99 | 52,800 | high | five units: `WPAD.c` 0x8027D680-0x80284FD8 (`WBCReadDummy` first, `WPADInit`), `WPADHIDParser.c` 0x80284FE0-0x802892A4, `WPADEncrypt.c` 0x802892B0-0x8028A370, `WPADMem.c` 0x8028A370-0x8028A46C, `wpad_debug_msg.c` 0x8028A470-0x8028A4C0 (`DEBUGPrint`, Matching; unit name unconfirmed); banner WPAD, `NDEV` firmware message | `.rodata` 0x803DDC30-0x803DDC88; `.data` 0x80447618-0x804492D8; `.bss` 0x804F13C0-0x804F4BA0; `.sdata` 0x80528E38-0x80528E68; `.sbss` 0x8052B0A0-0x8052B114; `.sdata2` 0x8052EE20-0x8052EE88 |
| WUD | 0x8028A4C0 | 0x80290F00 | 92 | 27,200 | high | three units: `WUD.c` 0x8028A4C0-0x802908B4, `WUDHidHost.c` 0x802908C0-0x80290EA8 (ends with `bta_hh_co_open/close`, `bta_dm_co_get_compress_memory`), `debug_msg.c` 0x80290EB0-0x80290F00 (`WUD_DEBUGPrint`, parked: MATCH-PENDING); string `WUD.c` x3, `__wud*`, `BTA_Init()`, `WUDShutdown()` | `.data` 0x804492D8-0x8044A718; `.bss` 0x804F4BA0-0x804F6D48; `.sdata` 0x80528E68-0x80528E80; `.sbss` 0x8052B118-0x8052B150; `.sdata2` 0x8052EE88-0x8052EE90 |

### RenderWare, RW Physics, FMOD, HBM

| range name | start | end | fn | bytes | confidence | evidence | owned data |
|---|---|---|---|---|---|---|---|
| RenderWare 3.7 core, Rp/Rt, GameCube/Wii driver | 0x80290F00 | 0x802F5000 | 1,274 | 409,856 | medium-high content, low edges | starts with `RwBBoxCalculate` (RwBBoxCalculate shape: running min/max of a point array into a 0x18-byte box), so 0x80290F00-0x80293078 is RenderWare, not WUD; `Invalid projection type specified` (RwCamera) 0x80293438, `Only rendering sub system`, `_rw3DRenderPrimitiveInit()` 0x8029F378, `Invalid combination type` (Rt/Rp matfx) x5, `rwRASTERFORMATLUM8 not yet supported` 0x802AB6EC, `Conversion from 8/16/32bit rasters...` 0x802ABCFC, Climax GX wrapper `cxGXCallDisplayList` 0x802AEDC8, `Failed to create surface for texture` 0x802B06AC, `_rxGCChannelLightingSetup` 0x802BEEA8, `control point out of range` 0x802CA6F4 (patch/bezier), `_rwDlObjectRenderSetup` 0x802DE568 and `_rwDlVtxFmtSetup` (display-list pipeline). No `Rp*/Rt*` name strings, no `rwID_` assert paths. Bluetooth/WPAD data (0x8044A9A0) ends right before this range's `.data` (0x8044A730) | `.data` 0x8044A730-0x8044BAA8; `.bss` 0x804F6D48-0x805199D8; `.rodata` 0x803DDC88-0x803DDDC8 |
| RW Physics (Rwp) | 0x802F5000 | 0x8032BA54 | 525 | 223,828 | medium (identity high, start low) | `.data` tables `RwpAggregate`, `RwpCapsule`, `RwpConvex`, `RwpCylinder`, `RwpPlane`, `RwpSphere`, `RwpTriangle`, `RwpTrilist`, `rwpOBJTYPEBODY/GENERIC/RAGDOLL` (0x8044BBD0-0x8044BDB4), `.sdata` `RwpBox`/`RwpGrid`/`RwpNull` (0x80528FF8-0x80529010). Objtype vtables at 0x8032A484 (generic) and 0x8032B238 (ragdoll) sit immediately before `fmod.cpp`. Start estimated from the first function referencing the Rwp `.data`/`.sdata` blocks (0x802F52A4) | `.data` 0x8044BAB8-0x8044BD84; `.sdata` 0x80528FE8-0x80529018; `.sdata2` 0x8052F370-0x8052F868 |
| FMOD Ex 4.x + Event system | 0x8032BA54 | 0x803B4C10 | 1,880 | 561,572 | high | `fmod.cpp` 0x8032BBFC ... `fmod_sounddef.cpp` 0x803B420C: 47 distinct `fmod_*.cpp` names (table below); `FMOD XM Codec`/`OggS` 0x80370374; DSP units (`FMOD Resampler Unit`, `FMOD WaveTable Unit`); Revolution back ends `fmod_output_revolution.cpp`, `fmod_channel_revolution.cpp`, `fmod_sample_revolution.cpp` | `.data` 0x8044BDA8-0x804531B4; `.bss` 0x80519D58-0x805258CC; `.sdata2` 0x8052F870-0x8053006C |
| HBM (Home Button Menu, nw4r lyt/ut) | 0x803B4C10 | 0x803D8360 | 544 | 145,232 | high | banner `RVL_SDK - HBM` (0x80453828, build `0x4201_142`), `wt\HomeButtonSe.wt` / `.pcm`, `P1_Def.brlyt` (0x803B9150+). 536 of the function sizes in order equal xbret/xenoblade JP's HBM (same `20090303` drop); starts with `HBMFrameController.cpp` (16-byte aligned, FMOD is 4-aligned). Unit table below | `.data` 0x804531F0-0x80457440; `.rodata` 0x803DE928-0x803DEE6C; `.bss` 0x805258F8-0x80526710 |

#### HBM units

Unit boundaries come from xbret/xenoblade's JP split (`config/jp/splits.txt`, HBM 20090303, the same drop as
ours): aligning the two function-size sequences, 536 of our 544 functions have the same size in the same order.
The 8 others: xenoblade's `Pane::GetColorElement`/`SetColorElement` (0x24 each) are 0x20 + 0x4 here
(0x803C7480-0x803C74D0), and dtk lists five 0xC/0x24 pieces xenoblade does not (0x803B89A0, 0x803C1F24,
0x803C8624, 0x803C9AD4, 0x803CBFD4: dtk split artefacts at unit edges or code xenoblade folds into a neighbour;
check when the unit is opened). Unit order is the HBM link order of xenoblade and of
ThePlayerRolo/sdk_2009-12-11. Functions the game never calls are dead-stripped (`ut_list` keeps 5 of 8). The
sound units are the drop's `sound/*.cpp` (C++ with C-linkage exports; xenoblade names them `mix.c`): see
docs/exceptions.md, "HBM rulings (sdk-hbm-16)".
Of the 41 units below, 40 are `Matching` (linked into the gated DOL) and one is parked:
`sound/synctrl` (3 of 4).

| unit (`src/revolution/hbm/...`) | start | end | fn | status |
|---|---|---|---|---|
| `HBMFrameController.cpp` | 0x803B4C10 | 0x803B4DBC | 3 | Matching |
| `HBMAnmController.cpp` | 0x803B4DC0 | 0x803B4ED0 | 3 | Matching |
| `HBMGUIManager.cpp` | 0x803B4ED0 | 0x803B66E8 | 56 | Matching |
| `HBMController.cpp` | 0x803B66F0 | 0x803B7300 | 25 | Matching |
| `HBMRemoteSpk.cpp` | 0x803B7300 | 0x803B7C1C | 15 | Matching |
| `HBMAxSound.cpp` | 0x803B7C20 | 0x803B8744 | 10 | Matching |
| `HBMCommon.cpp` | 0x803B8750 | 0x803B8AA4 | 12 | Matching |
| `HBMBase.cpp` | 0x803B8AB0 | 0x803C0E60 | 33 | Matching |
| `nw4hbm/lyt/lyt_animation.cpp` | 0x803C0E60 | 0x803C1A10 | 11 | Matching |
| `nw4hbm/lyt/lyt_arcResourceAccessor.cpp` | 0x803C1A10 | 0x803C1DB0 | 6 | Matching |
| `nw4hbm/lyt/lyt_bounding.cpp` | 0x803C1DB0 | 0x803C1F30 | 6 | Matching |
| `nw4hbm/lyt/lyt_common.cpp` | 0x803C1F30 | 0x803C2DC0 | 16 | Matching |
| `nw4hbm/lyt/lyt_drawInfo.cpp` | 0x803C2DC0 | 0x803C2E80 | 2 | Matching |
| `nw4hbm/lyt/lyt_group.cpp` | 0x803C2E80 | 0x803C31D0 | 5 | Matching |
| `nw4hbm/lyt/lyt_layout.cpp` | 0x803C31D0 | 0x803C3E70 | 15 | Matching |
| `nw4hbm/lyt/lyt_material.cpp` | 0x803C3E70 | 0x803C70E0 | 23 | Matching |
| `nw4hbm/lyt/lyt_pane.cpp` | 0x803C70E0 | 0x803C80D0 | 28 | Matching |
| `nw4hbm/lyt/lyt_picture.cpp` | 0x803C80D0 | 0x803C8630 | 12 | Matching |
| `nw4hbm/lyt/lyt_resourceAccessor.cpp` | 0x803C8630 | 0x803C8688 | 3 | Matching |
| `nw4hbm/lyt/lyt_textBox.cpp` | 0x803C8690 | 0x803C9AE0 | 17 | Matching |
| `nw4hbm/lyt/lyt_window.cpp` | 0x803C9AE0 | 0x803CBFE0 | 21 | Matching |
| `nw4hbm/math/math_triangular.cpp` | 0x803CBFE0 | 0x803CC290 | 3 | Matching |
| `nw4hbm/ut/ut_binaryFileFormat.cpp` | 0x803CC290 | 0x803CC310 | 1 | Matching |
| `nw4hbm/ut/ut_CharStrmReader.cpp` | 0x803CC310 | 0x803CC430 | 4 | Matching |
| `nw4hbm/ut/ut_CharWriter.cpp` | 0x803CC430 | 0x803CDE30 | 26 | Matching |
| `nw4hbm/ut/ut_Font.cpp` | 0x803CDE30 | 0x803CDEE0 | 1 | Matching |
| `nw4hbm/ut/ut_LinkList.cpp` | 0x803CDEE0 | 0x803CDFF0 | 3 | Matching |
| `nw4hbm/ut/ut_list.cpp` | 0x803CDFF0 | 0x803CE160 | 5 | Matching |
| `nw4hbm/ut/ut_ResFont.cpp` | 0x803CE160 | 0x803CE460 | 4 | Matching |
| `nw4hbm/ut/ut_ResFontBase.cpp` | 0x803CE460 | 0x803CEAB0 | 24 | Matching |
| `nw4hbm/ut/ut_TagProcessorBase.cpp` | 0x803CEAB0 | 0x803CF570 | 12 | Matching |
| `nw4hbm/ut/ut_TextWriterBase.cpp` | 0x803CF570 | 0x803D49E0 | 83 | Matching |
| `sound/mix.cpp` | 0x803D49E0 | 0x803D6350 | 15 | Matching |
| `sound/syn.cpp` | 0x803D6350 | 0x803D67F0 | 8 | Matching |
| `sound/synctrl.cpp` | 0x803D67F0 | 0x803D6C90 | 4 | NonMatching (3 of 4; ResetAllControllers schedule residue) |
| `sound/synenv.cpp` | 0x803D6C90 | 0x803D7070 | 2 | Matching |
| `sound/synmix.cpp` | 0x803D7070 | 0x803D71B0 | 5 | Matching |
| `sound/synpitch.cpp` | 0x803D71B0 | 0x803D7430 | 4 | Matching |
| `sound/synsample.cpp` | 0x803D7430 | 0x803D78B0 | 4 | Matching |
| `sound/synvoice.cpp` | 0x803D78B0 | 0x803D7A60 | 3 | Matching |
| `sound/seq.cpp` | 0x803D7A60 | 0x803D8360 | 11 | Matching |

#### BTE units

Unit boundaries come from yannicksuter/mscharged-decomp's `R4QK01` split (`config/R4QK01/splits.txt` and
`symbols.txt`; Korean Mario Strikers Charged, the same 2009 SDK, every BTE object `Matching` there): all 731 of our
functions in 0x8024A790-0x80277F60 have the reference's size in the reference's order, with the same padding
between functions, so the function names in `symbols.txt` are the reference's. In the reference the range sits
between `wud/debug_msg.c` and `usb/usb.c`; here it follows `arc.c` and ends with 12 bytes of padding before
0x80277F60. Unit order is the reference's link order. Sources live in `src/revolution/bte/<file>` (flat, as in
the reference and ThePlayerRolo/sdk_2009-12-11). Compiler per unit: the Wii glue units are `Wii/1.0` and 16-byte
aligned, the protocol stack `GC/3.0a5` and packed at 4 bytes (`BTELib` in `configure.py`; docs/toolchain.md profile).
The three `*_cfg.c` units hold only data. The reference functions that retail lacks were dead-stripped (sdk_2009
lists more objects, such as `bta_*_cfg` for unused profiles, `goep_*`, `xml_*`; none has code here).

| unit (`src/revolution/bte/...`) | start | end | fn | compiler | status |
|---|---|---|---|---|---|
| `gki_buffer.c` | 0x8024A790 | 0x8024BC30 | 18 | Wii/1.0 | Matching |
| `gki_time.c` | 0x8024BC30 | 0x8024C1C0 | 8 | Wii/1.0 | Matching |
| `gki_ppc.c` | 0x8024C1C0 | 0x8024C4C4 | 13 | Wii/1.0 | Matching |
| `hcisu_h2.c` | 0x8024C4D0 | 0x8024CB90 | 8 | Wii/1.0 | Matching |
| `uusb_ppc.c` | 0x8024CB90 | 0x8024D810 | 11 | Wii/1.0 | Matching |
| `bta_dm_cfg.c` | - | - | 0 | data only (GC/3.0a5) | Matching |
| `bta_hh_cfg.c` | - | - | 0 | data only (GC/3.0a5) | Matching |
| `bta_sys_cfg.c` | - | - | 0 | data only (GC/3.0a5) | Matching |
| `bte_hcisu.c` | 0x8024D810 | 0x8024D8E0 | 4 | Wii/1.0 | Matching |
| `bte_init.c` | 0x8024D8E0 | 0x8024D910 | 1 | Wii/1.0 | Matching |
| `bte_logmsg.c` | 0x8024D910 | 0x8024DAA8 | 8 | Wii/1.0 | Matching |
| `bte_main.c` | 0x8024DAB0 | 0x8024DC20 | 4 | Wii/1.0 | Matching |
| `btu_task1.c` | 0x8024DC20 | 0x8024E064 | 4 | Wii/1.0 | Matching |
| `bd.c` | 0x8024E064 | 0x8024E138 | 2 | GC/3.0a5 | Matching |
| `bta_sys_conn.c` | 0x8024E138 | 0x8024E38C | 8 | GC/3.0a5 | Matching |
| `bta_sys_main.c` | 0x8024E38C | 0x8024E5D0 | 9 | GC/3.0a5 | Matching |
| `ptim.c` | 0x8024E5D0 | 0x8024E7B0 | 4 | GC/3.0a5 | Matching |
| `utl.c` | 0x8024E7B0 | 0x8024E7F4 | 1 | GC/3.0a5 | Matching |
| `bta_dm_act.c` | 0x8024E7F4 | 0x80250B30 | 56 | GC/3.0a5 | Matching |
| `bta_dm_api.c` | 0x80250B30 | 0x80250F9C | 11 | GC/3.0a5 | Matching |
| `bta_dm_main.c` | 0x80250F9C | 0x802510FC | 2 | GC/3.0a5 | Matching |
| `bta_dm_pm.c` | 0x802510FC | 0x80251B18 | 8 | GC/3.0a5 | Matching |
| `bta_hh_act.c` | 0x80251B18 | 0x80253260 | 18 | GC/3.0a5 | Matching |
| `bta_hh_api.c` | 0x80253260 | 0x80253630 | 8 | GC/3.0a5 | Matching |
| `bta_hh_main.c` | 0x80253630 | 0x80253B84 | 3 | GC/3.0a5 | Matching |
| `bta_hh_utils.c` | 0x80253B84 | 0x80253F30 | 5 | GC/3.0a5 | Matching |
| `btm_acl.c` | 0x80253F30 | 0x80255C74 | 29 | GC/3.0a5 | Matching |
| `btm_dev.c` | 0x80255C74 | 0x80256340 | 7 | GC/3.0a5 | Matching |
| `btm_devctl.c` | 0x80256340 | 0x80257CC8 | 33 | GC/3.0a5 | Matching |
| `btm_discovery.c` | 0x80257CC8 | 0x80257DFC | 3 | GC/3.0a5 | Matching |
| `btm_inq.c` | 0x80257DFC | 0x80259894 | 25 | GC/3.0a5 | Matching |
| `btm_main.c` | 0x80259894 | 0x802598F0 | 1 | GC/3.0a5 | Matching |
| `btm_pm.c` | 0x802598F0 | 0x8025A584 | 10 | GC/3.0a5 | Matching |
| `btm_sco.c` | 0x8025A584 | 0x8025B3BC | 15 | GC/3.0a5 | Matching |
| `btm_sec.c` | 0x8025B3BC | 0x8025E380 | 31 | GC/3.0a5 | Matching |
| `btu_hcif.c` | 0x8025E380 | 0x8025F5DC | 12 | GC/3.0a5 | Matching |
| `btu_init.c` | 0x8025F5DC | 0x8025F654 | 2 | GC/3.0a5 | Matching |
| `wbt_ext.c` | 0x8025F654 | 0x8025F734 | 1 | GC/3.0a5 | Matching |
| `gap_api.c` | 0x8025F734 | 0x8025F794 | 1 | GC/3.0a5 | Matching |
| `gap_conn.c` | 0x8025F794 | 0x802602D0 | 8 | GC/3.0a5 | Matching |
| `gap_utils.c` | 0x802602D0 | 0x802608EC | 6 | GC/3.0a5 | Matching |
| `hcicmds.c` | 0x802608EC | 0x80263008 | 59 | GC/3.0a5 | Matching |
| `hidd_api.c` | 0x80263008 | 0x80263070 | 1 | GC/3.0a5 | Matching |
| `hidd_conn.c` | 0x80263070 | 0x80263130 | 1 | GC/3.0a5 | Matching |
| `hidd_mgmt.c` | 0x80263130 | 0x802631F8 | 1 | GC/3.0a5 | Matching |
| `hidd_pm.c` | 0x802631F8 | 0x80263574 | 3 | GC/3.0a5 | Matching |
| `hidh_api.c` | 0x80263574 | 0x802642F4 | 11 | GC/3.0a5 | Matching |
| `hidh_conn.c` | 0x802642F4 | 0x80266334 | 16 | GC/3.0a5 | Matching |
| `l2c_api.c` | 0x80266334 | 0x80266F00 | 13 | GC/3.0a5 | Matching |
| `l2c_csm.c` | 0x80266F00 | 0x802683DC | 11 | GC/3.0a5 | Matching |
| `l2c_link.c` | 0x802683DC | 0x80269568 | 17 | GC/3.0a5 | Matching |
| `l2c_main.c` | 0x80269568 | 0x8026A598 | 5 | GC/3.0a5 | Matching |
| `l2c_utils.c` | 0x8026A598 | 0x8026C45C | 33 | GC/3.0a5 | Matching |
| `port_api.c` | 0x8026C45C | 0x8026C4AC | 1 | GC/3.0a5 | Matching |
| `port_rfc.c` | 0x8026C4AC | 0x8026D8A8 | 18 | GC/3.0a5 | Matching |
| `port_utils.c` | 0x8026D8A8 | 0x8026DE80 | 7 | GC/3.0a5 | Matching |
| `rfc_l2cap_if.c` | 0x8026DE80 | 0x8026E7B4 | 10 | GC/3.0a5 | Matching |
| `rfc_mx_fsm.c` | 0x8026E7B4 | 0x8026F3F8 | 10 | GC/3.0a5 | Matching |
| `rfc_port_fsm.c` | 0x8026F3F8 | 0x80270598 | 17 | GC/3.0a5 | Matching |
| `rfc_port_if.c` | 0x80270598 | 0x80270AC4 | 12 | GC/3.0a5 | Matching |
| `rfc_ts_frames.c` | 0x80270AC4 | 0x8027210C | 15 | GC/3.0a5 | Matching |
| `rfc_utils.c` | 0x8027210C | 0x802728EC | 15 | GC/3.0a5 | Matching |
| `sdp_api.c` | 0x802728EC | 0x80273754 | 9 | GC/3.0a5 | Matching |
| `sdp_db.c` | 0x80273754 | 0x80274420 | 10 | GC/3.0a5 | Matching |
| `sdp_discovery.c` | 0x80274420 | 0x80275698 | 10 | GC/3.0a5 | Matching |
| `sdp_main.c` | 0x80275698 | 0x80276204 | 11 | GC/3.0a5 | Matching |
| `sdp_server.c` | 0x80276204 | 0x80276F14 | 4 | GC/3.0a5 | Matching |
| `sdp_utils.c` | 0x80276F14 | 0x80277F60 | 13 | GC/3.0a5 | Matching |

Not found: any audio engine other than FMOD, libpng, Lua. IJG libjpeg sits in the Climax block, not
adjacent to the SDK. The RW Physics identification is stronger than its edges. The `rwpID_*` strings at 0x801AB878
and 0x8020925C are chunk IDs registered by Climax asset code, not the Rwp library.

### FMOD source-file clusters (candidate TU boundaries)

First and last function referencing each `__FILE__` string, and the count (clusters can interleave within a file
because template and inline code from headers is attributed elsewhere). Order is link order.

| file | first | last | fn refs |
|---|---|---|---|
| fmod.cpp | 0x8032BBFC | 0x8032BBFC | 1 |
| fmod_async.cpp | 0x8032BE64 | 0x8032C49C | 4 |
| fmod_channelgroupi.cpp | 0x80335EB4 | 0x80335EB4 | 1 |
| fmod_channelpool.cpp | 0x8033CC68 | 0x8033CD0C | 2 |
| fmod_plugin.cpp | 0x8033E7E8 | 0x8033E7E8 | 1 |
| fmod_pluginfactory.cpp | 0x8033FAE0 | 0x8033FAE0 | 1 |
| fmod_reverbi.cpp | 0x8033FED8 | 0x8034029C | 3 |
| fmod_soundgroupi.cpp | 0x80344E80 | 0x80344E80 | 1 |
| fmod_soundi.cpp | 0x8034715C | 0x80348CC8 | 2 |
| fmod_speakerlevels_pool.cpp | 0x80349D08 | 0x80349F1C | 2 |
| fmod_systemi.cpp | 0x8034FCE0 | 0x80352A30 | 4 |
| fmod_channel_revolution.cpp | 0x803547BC | 0x803547BC | 1 |
| fmod_os_misc.cpp | 0x80357FD4 | 0x80358874 | 6 |
| fmod_output_revolution.cpp | 0x80358AA8 | 0x803593FC | 2 |
| fmod_sample_revolution.cpp | 0x8035ABC8 | 0x8035ABC8 | 1 |
| fmod_dsp_codec.cpp | 0x8035AFDC | 0x8035AFDC | 1 |
| fmod_dsp_filter.cpp | 0x8035F3E4 | 0x8035F594 | 2 |
| fmod_dsp_resampler.cpp | 0x8035F6C0 | 0x8035F758 | 2 |
| fmod_dsp_soundcard.cpp | 0x803649A4 | 0x80364A64 | 2 |
| fmod_dspi.cpp | 0x803679A0 | 0x8036838C | 4 |
| fmod_codec.cpp | 0x80369770 | 0x8036A44C | 4 |
| fmod_output_emulated.cpp | 0x80372588 | 0x803726C8 | 2 |
| fmod_output_software.cpp | 0x80373088 | 0x80373088 | 1 |
| fmod_file.cpp | 0x80373948 | 0x803751D0 | 5 |
| fmod_memory.cpp | 0x80375C90 | 0x803763E4 | 3 |
| fmod_string.cpp | 0x80376E10 | 0x80376E10 | 1 |
| fmod_thread.cpp | 0x803772F0 | 0x803772F0 | 1 |
| fmod_output.cpp | 0x80379E88 | 0x80379E88 | 1 |
| fmod_buckethash.cpp | 0x8037BB68 | 0x8037BD00 | 4 |
| fmod_compositionconditions.cpp | 0x8037BF14 | 0x8037BF14 | 1 |
| fmod_compositioncore.cpp | 0x8037C844 | 0x8037D140 | 3 |
| fmod_compositionentities.cpp | 0x8037F178 | 0x80383F3C | 19 |
| fmod_eventcategoryi.cpp | 0x80384B5C | 0x8038559C | 3 |
| fmod_eventi.cpp | 0x8038AEC8 | 0x80391E60 | 3 |
| fmod_eventimpl_complex.cpp | 0x80395CF0 | 0x80395CF0 | 1 |
| fmod_eventimpl_simple.cpp | 0x8039BC84 | 0x8039BE38 | 2 |
| fmod_eventinstancepool.cpp | 0x8039C3F4 | 0x8039C3F4 | 1 |
| fmod_eventprojecti.cpp | 0x803A0950 | 0x803A0950 | 1 |
| fmod_memory.h | 0x803A14A0 | 0x803A14A0 | 1 |
| fmod_eventreverb.cpp | 0x803A1E58 | 0x803A1F2C | 2 |
| fmod_eventsound.cpp | 0x803A2EF0 | 0x803A2EF0 | 1 |
| fmod_eventsystemi.cpp | 0x803A4EEC | 0x803A9BB8 | 6 |
| fmod_eventuserproperty.cpp | 0x803AA02C | 0x803AA02C | 1 |
| fmod_musicengine.cpp | 0x803AACDC | 0x803AB950 | 2 |
| fmod_musicsystemi.cpp | 0x803AC214 | 0x803AD044 | 4 |
| fmod_segmentplayer.cpp | 0x803AF698 | 0x803AF80C | 2 |
| fmod_simplemempool.cpp | 0x803B0218 | 0x803B0474 | 5 |
| fmod_soundbank.cpp | 0x803B049C | 0x803B3764 | 8 |
| fmod_sounddef.cpp | 0x803B420C | 0x803B420C | 1 |

A file name with a single function (such as `fmod_sounddef.cpp`) marks one function that *references the string*;
its TU extends to neighbours that share its float-constant pool: `fmod_sounddef.cpp`'s pool (`.sdata2` 0x80530038+)
is used from 0x803B3EE4 to 0x803B4AC8, so that TU is about 0x803B3EC0-0x803B4C00. The FMOD `__LINE__` constants next
to these strings (see AGENTS.md) mean FMOD source must not be reflowed.

## Candidate TU boundaries

**1,197 strict candidates** in `ref/R5WEA4/tu_boundaries_strict.tsv` (1,119 in the looser variant, `--loose`).
Method (the `nocross` evidence of `build/tools/unit_plan.exe`, which replaced the one-off `tu_boundaries.py`): a `.sdata2` float constant or a string literal is emitted once per TU and is
never merged by the linker, so all functions referencing one lie in the same TU. A gap between neighbours that no such
constant's span crosses is a candidate boundary. Constants with spans over 300 functions are treated as extern
const globals and ignored. Only 3,158 of 13,951 functions reference any such constant (anchored); a boundary
between anchored functions A and B can lie anywhere among the `slack` unanchored functions between them (column
3). 26% of the cuts (317) have slack 0, i.e. are exact function-level boundaries.

Candidates per range (strict): G1 68, G2 24, G3 132, G4 121, G5 98, G6 81, G7 38, platform 3, MetroTRK 4, MSL 25,
Climax core 18, Climax stream class 2, libjpeg 6, Climax XML/config/assets 96, THP 2, OS 13, DB 3, DVD 6, VI 1, AX/DSP 8,
GX 3, NAND 5, SC/ESP/ARC 14, BTE 108, TPL 1, KPAD 2, WPAD 10, WUD 13, RW 74, Rwp 52, FMOD 164, HBM 2.
(EXI, SI, PAD/AI, IPC and the runtime have none because their functions reference no private
constants or strings, so no cut can be proven there.)

### Candidate TUs from dtk-named functions (SDK, MSL, MetroTRK)

File names are the ones public Wii decomps use for these functions; the addresses are this DOL's. Treat the
names as hypotheses and the boundaries as approximate.

| expected file | first function | notes |
|---|---|---|
| (MetroTRK) `cc_gdev.c` | 0x80190C0C `gdev_cc_initialize` | 10 functions to 0x80190E84 |
| `main_TRK.c` / nub files | 0x80191134 `TRK_main`, `TRKNubMainLoop` 0x80191170 | |
| `dispatch.c` | 0x801914C0 `TRKDispatchMessage` | |
| `dolphin_trk.c` | 0x801915E0 `InitMetroTRK` ... `InitMetroTRKCommTable` 0x801919C4 | |
| `nubevent.c` | 0x80191D30 `TRKInitializeEventQueue` | |
| `nubinit.c` | 0x80191ECC `TRKInitializeNub` | |
| `msgbuf.c` | 0x80192B64 `TRKInitializeMessageBuffers`, `TRKGetBuffer` | |
| `targimpl.c` / `excpt` | 0x80194D20 `TRKInterruptHandler`, 0x80195648 `TRKTargetSupportRequest` | |
| (MSL) `ansi_files.c` | 0x80197160 `__close_all` | |
| `buffer_io.c` | 0x80198B00 `__prep_buffer`, `__flush_buffer` | |
| `file_io.c` | 0x80198BE0 `__fwrite` | |
| `file_pos.c` | 0x80199184 `_fseek` | |
| `wchar_io.c` / `mbstring.c` | 0x80199488 `wcstombs` | |
| `mem.c` | 0x80199540 `memmove`, `memchr`, `__memrchr` | |
| `printf.c` | 0x80199A14 `__stdio_atexit` ... 0x8019BE14 `sprintf` | `parse_format`, `long2str`, `double2hex`, `float2str`, `__pformatter` |
| `string.c` | 0x8019D564 `strcpy`, `strchr`; `strlen` 0x801A622C | |
| `wide_io.c` | 0x801A1C30 `fwide` | |
| (runtime) `__va_arg.c` | 0x801A6248 | |
| `global_destructor_chain.c` | 0x801A6310 | already a split unit |
| `runtime.c` | 0x801A67A4 `__save_fpr` ... `__mod2u` 0x801A6AF8 | |
| `__init_cpp_exceptions.cpp` | 0x801A6FD4 | already a split unit; `__register_fragment` 0x801A7044 follows |
| (SDK) `EXIBios.c`/`EXIUart.c` | 0x8020D780 `SetExiInterruptMask` ... `EXIGetID` 0x8020ECD0 | `__OSEnableBarnacle` 0x8020F200 |
| `SIBios.c`, `SISamplingRate.c` | 0x8020F850 `SIInterruptHandler` ... 0x80210590 | |
| `PPCArch.c` | 0x80210680 `PPCMfmsr` ... 0x80210890 | |
| `OS.c` | 0x80210890 `__OSFPRInit` ... `OSExceptionInit`, `__OSDBIntegrator` | |
| `OSAlarm.c` | 0x80211DD0 `__OSInitAlarm` ... 0x802124E0 | |
| `OSArena.c` | 0x802129C0 `OSGetMEM1ArenaHi` ... 0x80212A80 | |
| `OSAudioSystem.c` | 0x80212AC0 `__AIClockInit`, 0x80212CE0 | |
| `OSCache.c` | 0x80212F80 `DCEnable` ... `__OSCacheInit` 0x80213480 | |
| `OSContext.c` | 0x802135C0 `__OSLoadFPUContext` ... 0x80213E50 | |
| `OSError.c` | 0x80213EA0 `OSReport`, `OSPanic`, `OSSetErrorHandler`, `__OSUnhandledException` | |
| `OSExec.c` / `OSFatal.c` / `OSFont.c` | 0x80214AE0 / 0x80216010 / 0x80216AE0 | |
| `OSInterrupt.c` | 0x802174E0 `OSDisableInterrupts` ... 0x80217C60 | |
| `OSLink.c`, `OSMemory.c` | 0x80217CB0, 0x80217FB0 ... `__OSInitMemoryProtection` 0x80218680 | |
| `OSReset.c` | 0x80218C00 `OSRegisterShutdownFunction` ... `OSResetSystem` 0x80219510 | string `OSReset.c` |
| `OSRtc.c` | 0x80219670 `__OSInitSram` ... 0x80219ED0 | |
| `OSSync.c`, `OSThread.c` | 0x8021A140 `__OSInitSystemCall`, 0x8021A1B0 `__OSThreadInit` ... 0x8021B4E0 | string `OSThread.c` |
| `OSTime.c`, `OSIpc.c`, `OSStateTM.c` | 0x8021C000, 0x8021C6A0, 0x8021C8A0 ... 0x8021CC70 | string `OSStateTM.c` |
| `OSPlayRecord.c`, `OSStateFlags.c`, `OSNet.c` | 0x8021CDE0, 0x8021D4F0, 0x8021D710 | |
| `OSPlayTime.c` | 0x8021DEC0 ... `__OSInitPlayTime` 0x8021E540 | string `OSPlayTime.c` |
| `__ppc_eabi_init.cpp`, `db.c` | 0x8021E8B0, 0x8021E970 | |
| `dvdfs.c`, `dvd.c`, `dvdlow.c`, ... | 0x8021EFA0, 0x8021F9D0, 0x80225A70 `DVDLowInit` | strings `dvdfs.c`, `dvd.c` |
| `vi.c`, `i2c.c`, `vi3in1.c` | 0x80227F50, 0x8022A990, 0x8022B2B0 ... 0x8022C9FC | strings `vi.c`, `vi3in1.c`; split, i2c and vi3in1 Matching |
| `GXInit.c`, `GXFifo.c`, `GXMisc.c`, `GXTexture.c` | 0x80237B60, 0x80238A90 `GXInitFifoBase`, 0x8023A1B0 `GXSetMisc`, 0x8023C770 | |
| `fs.c`, `ipcMain.c`, `ipcclt.c`, `memory.c`, `ipcProfile.c` | 0x80240550 `ISFS_OpenLib` ... 0x80243C7C | split, Matching |
| `nand.c` ... `NANDLogging.c` (six units) | 0x80243C80 ... 0x80247444; 0x802462A0 `NANDInit` | split, stubs |
| `scsystem.c`, `scapi.c`, `scapi_prdinfo.c`, `esp.c`, `arc.c` | 0x80247450 `SCInit`, 0x80249790 `ESP_InitLib`, 0x80249DB0 ... 0x8024A788 | string `arc.c`; esp and arc Matching |

## Cross-checks and open questions

- THP: the `THP` string at 0x8009AA00 is game-side (a chunk type). The THP *library* starts at 0x80209D80
  (`THPVideoDecode`); `fn_80209D54` before it is game code.
- IJG/zlib split (see above): libjpeg's data comes before inflate's in `.rodata`/`.data`, matching the text order,
  so libjpeg then inflate is a consistent pair.
- No RenderWare `Rp*`/`Rt*` identifier strings exist, so RW sub-library edges (Rw core, Rp world/atomic/matfx,
  Rt anim/bezier, the Wii driver and `cx*` Climax additions) must come from the assert/`Invalid combination type`
  clusters and from the RW 3.7 SDK layout. The split into RW_A/RW_B in the lanes below is by size only.
- The SC/ESP/ARC range is the weakest SDK edge. (0x80290F00-0x80293078, once unclear, is RenderWare.)
- `main`'s neighbours: `rvl.cpp` is the game's platform layer (it owns `main`, DVD init and the banner), so it is a
  game unit rather than part of the SDK.

## Lane candidates

Disjoint lanes by library and size (bytes of `.text`; fn = function count). Ordered roughly by how well the source is
known and how small the unit is, so early lanes build confidence in the toolchain flags. Each lane's address
range is approximate; start workers on a single named TU, not the whole lane.

| lane | range | fn | bytes | why this lane |
|---|---|---|---|---|
| L1 MetroTRK + MSL + runtime | 0x80190C0C-0x801A70B8 | 313 | 91,304 | known sources in public decomps; dtk names; pins compiler flags (MSL flags differ from SDK) |
| L2 SDK OS core: EXI, SI, OS, DB | 0x8020D780-0x8021EFA0 | 329 | 69,664 | dtk-named, `OS*.c` strings, banners give exact flags |
| L3 SDK DVD, VI, PAD/AI, AX/DSP | 0x8021EFA0-0x802377A0 | 334 | 98,208 | known decompiled sources; mid-size. Pad, ai, AX, AXAlloc, AXAux, AXCL, AXOut, AXSPB, AXVPB, AXProf, AXComp, DSPCode, dsp and dsp_debug matched (0x8022CA00-0x802309D0, 0x80236C10-0x80236F00); vi.c matched. AXFX matched except `AXFXReverbStdExp.c` (parked, 0x802309D0-0x802334B4). MIX (`mix.c`, `mix_rmt.c`; the range once listed as AXART) matched (0x802334C0-0x80236C10). Open: dsp_task.c (parked, `__DSP_boot_task` R17) and DVD |
| L4 SDK GX, MEM, euart, IPC/ISFS, NAND, SC/ESP/ARC | 0x802377A0-0x8024A790 | 396 | 75,320 | GX, MEM, euart, IPC/ISFS, NAND, SC, ESP and ARC matched (`fn_802494B4`, game audio, unassigned) |
| L5 SDK Bluetooth stack (BTE) | 0x8024A790-0x80277F60 | 731 | 185,792 | Broadcom BTE; 68 units mapped from mscharged-decomp R4QK01 (see "BTE units"); 68 of 68 units matched (the "BTE units" table gives each unit's status) |
| L6 SDK WPAD, KPAD, WUD, TPL, USB | 0x80277F60-0x80290F00 | | 102,304 | WPAD/KPAD/WUD have partial public sources (tp, ogws); KPAD, WPAD and WUD split and partly reconstructed; USB unowned |
| L7 THP, zlib, libjpeg | 0x801B4354-0x801CCD24 and 0x80209D80-0x8020D780 | 292 | 115,568 | THP matched (lane sdk-av-2); public upstream sources (zlib 1.2.3, IJG 6b); high match odds with correct flags |
| L8 FMOD core (system, channel, sound, DSP setup) | 0x8032BA54-0x80357FD4 | 537 | 181,632 | file names via `fmod_*.cpp`; FMOD 4.x headers |
| L9 FMOD platform, DSP, codecs, file/memory | 0x80357FD4-0x8037BB68 | 436 | 146,324 | `fmod_*_revolution.cpp`, `fmod_dsp*`, codecs |
| L10 FMOD events, music, soundbank | 0x8037BB68-0x803B4C10 | 907 | 233,616 | many small `fmod_event*.cpp`; `__LINE__` constants constrain edits |
| L11 HBM (nw4r lyt/ut + home button) | 0x803B4C10-0x803D8360 | 544 | 145,232 | one SDK library, strings `P1_Def.brlyt` |
| L12 RW core A | 0x80293078-0x802C8000 | 711 | 217,408 | RW 3.7 public headers; raster/camera/driver |
| L13 RW core B | 0x802C8000-0x802F5000 | 555 | 183,880 | pipelines, display lists, patches |
| L14 RW Physics | 0x802F5000-0x8032BA54 | 525 | 223,828 | Rwp class names in data; vtables for object types |
| L15 Climax core + XML/config/assets | 0x801A70B8-0x801B4354 and 0x801CCD24-0x80209D54 | 1,200 | 303,820 | engine-side, string-rich (`ClimaxT1Entry` registry) |
| L16 game G1 core, collision, anim, zones | 0x800067A0-0x80033D34 | 727 | 185,748 | foundational classes first; helper-recovery targets |
| L17 game G2 audio, characters, enemies | 0x80033D34-0x80073554 | 722 | 260,128 | |
| L18 game G3 player, fx, scene, IGC | 0x80073554-0x800C5000 | 1,467 | 334,588 | split further per TU once splits exist |
| L19 game G4 triggers, UI, save, photo | 0x800C5000-0x80100000 | 966 | 241,624 | |
| L20 game G5 XAML UI, camera, AI | 0x80100000-0x80144000 | 870 | 278,672 | |
| L21 game G6 fx, psych, puzzles | 0x80144000-0x80179000 | 744 | 217,676 | |
| L22 game G7 therapy, traversal, map + platform | 0x80179000-0x80190C0C | 369 | 96,520 | includes `rvl.cpp` (`main`) |

Suggested order for parallel workers (disjoint by construction): L1, L2, L7, L11 first (small or upstream-known),
then L3-L6 and L8-L10 together, then the RW and Rwp lanes, then the game lanes after `splits.txt` carries the
candidate boundaries for them. Lane edges should be re-derived from `ref/R5WEA4/tu_boundaries_strict.tsv` when a
worker adds splits.
