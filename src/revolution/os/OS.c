// Reference: zeldaret/tp libs/revolution/src/os/OS.c (rewritten against this tree's headers; retail is the Feb 27 2009 SDK, 0x4302_145)
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/os/OS.c (DriveInfo alignment, OSGetAppGamename)
#include <revolution/os.h>
#include <revolution/base.h>
#include <revolution/db.h>
#include <revolution/exi.h>
#include <revolution/hardware.h>
#include <revolution/ipc.h>
#include <revolution/pad.h>
#include <revolution/sc.h>
#include <revolution/si.h>
#include <PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h>
#include <string.h>
#include "TRK/dolphin_trk.h"

#define NOP 0x60000000

#define OS_CURRENTCONTEXT_PADDR 0x00C0
#define OS_CURRENTCONTEXT_VADDR 0x00D4
#define OS_EXCEPTIONTABLE_ADDR  0x3000
#define OS_DBJUMPPOINT_ADDR     0x60

#define BUILD_DATE "Feb 27 2009"
#define BUILD_TIME "10:04:29"

const char* __OSVersion = "<< RVL_SDK - OS \trelease build: " BUILD_DATE " " BUILD_TIME " (0x4302_145) >>";

static DVDCommandBlock DriveBlock;
static DVDDriveInfo DriveInfo ATTRIBUTE_ALIGN(32); // DMA target of DVDInquiryAsync
OSExecParams __OSRebootParams;

OSTime __OSStartTime;

static OSBootInfo* BootInfo;
static u32* BI2DebugFlag;
static u32 BI2DebugFlagHolder;

static char GameNameBuffer[5];

static f64 ZeroF;
static f32 ZeroPS[2];
static void (**OSExceptionTable)(__OSException, OSContext*);
static BOOL AreWeInitialized;
BOOL __OSIsGcam;
BOOL __OSInNandBoot;
BOOL __OSInIPL;

static void OSExceptionInit(void);

/*
 * __OSFPRInit
 *
 * Enables the FPU and clears every floating-point register (and the paired
 * singles, when HID2[PSE] is set) and the FPSCR.
 */
// ORIGINAL-ASM: an asm function in the public RVL SDK OS.c
asm void __OSFPRInit(void) {
    nofralloc

    mfmsr r3
    ori r3, r3, MSR_FP
    mtmsr r3

    mfspr r3, SPR_HID2
    rlwinm. r3, r3, 3, 0x1f, 0x1f
    beq skip_ps_init

    lis r3, ZeroPS@ha
    addi r3, r3, ZeroPS@l
    psq_l f0, 0(r3), 0, 0
    ps_mr f1, f0
    ps_mr f2, f0
    ps_mr f3, f0
    ps_mr f4, f0
    ps_mr f5, f0
    ps_mr f6, f0
    ps_mr f7, f0
    ps_mr f8, f0
    ps_mr f9, f0
    ps_mr f10, f0
    ps_mr f11, f0
    ps_mr f12, f0
    ps_mr f13, f0
    ps_mr f14, f0
    ps_mr f15, f0
    ps_mr f16, f0
    ps_mr f17, f0
    ps_mr f18, f0
    ps_mr f19, f0
    ps_mr f20, f0
    ps_mr f21, f0
    ps_mr f22, f0
    ps_mr f23, f0
    ps_mr f24, f0
    ps_mr f25, f0
    ps_mr f26, f0
    ps_mr f27, f0
    ps_mr f28, f0
    ps_mr f29, f0
    ps_mr f30, f0
    ps_mr f31, f0

skip_ps_init:
    lfd f0, ZeroF(r13)
    fmr f1, f0
    fmr f2, f0
    fmr f3, f0
    fmr f4, f0
    fmr f5, f0
    fmr f6, f0
    fmr f7, f0
    fmr f8, f0
    fmr f9, f0
    fmr f10, f0
    fmr f11, f0
    fmr f12, f0
    fmr f13, f0
    fmr f14, f0
    fmr f15, f0
    fmr f16, f0
    fmr f17, f0
    fmr f18, f0
    fmr f19, f0
    fmr f20, f0
    fmr f21, f0
    fmr f22, f0
    fmr f23, f0
    fmr f24, f0
    fmr f25, f0
    fmr f26, f0
    fmr f27, f0
    fmr f28, f0
    fmr f29, f0
    fmr f30, f0
    fmr f31, f0

    mtfsf 0xff, f0
    blr
} // 0x80210890

/*
 * DisableWriteGatherPipe
 *
 * Clears HID2[WPE].
 */
static void DisableWriteGatherPipe(void) {
    u32 hid2;

    hid2 = PPCMfhid2();
    hid2 &= ~0x40000000;
    PPCMthid2(hid2);
}

/* Revision of the Hollywood chip, read from low memory. */
static u32 __OSGetHollywoodRev(void) {
    return *(u32*)OSPhysicalToCached(0x3138);
}

/*
 * __OSGetIOSRev
 *
 * Decodes the IOS version and build date the IOS leaves in low memory.
 */
void __OSGetIOSRev(OSIOSRev* rev) {
    u32 iosVer;
    u32 iosBuild;

    iosVer = *(u32*)OSPhysicalToUncached(0x3140);
    iosBuild = *(u32*)OSPhysicalToUncached(0x3144);

    rev->reserved = (iosVer >> 24) & 0xFF;
    rev->major = (iosVer >> 16) & 0xFF;
    rev->minor = (iosVer >> 8) & 0xFF;
    rev->micro = iosVer & 0xFF;

    rev->month = ((iosBuild >> 20) & 0xF) * 10 + ((iosBuild >> 16) & 0xF);
    rev->date = ((iosBuild >> 12) & 0xF) * 10 + ((iosBuild >> 8) & 0xF);
    rev->year = ((iosBuild >> 4) & 0xF) * 10 + (iosBuild & 0xF) + 2000;
} // 0x802109C0

/*
 * OSGetConsoleType
 *
 * Returns the console type code: derived from the drive-reported device
 * code when there is one, otherwise from the Hollywood revision and the
 * MEM2 size; the boot information's value for an unrecognised revision.
 */
u32 OSGetConsoleType(void) {
    u32 hwRev;
    u32 gddrSize;

    if (BootInfo == NULL || BootInfo->consoleType == 0) {
        return 0x10000002;
    }

    hwRev = __OSGetHollywoodRev();

    if (__OSDeviceCode & 0x8000) {
        switch (__OSDeviceCode & ~0x8000) {
        case 2:
        case 3:
        case 0x203:
            switch (hwRev) {
            case 0:
                return 0x10;
            case 1:
                return 0x11;
            case 2:
                return 0x12;
            case 0x10:
                return 0x20;
            case 0x11:
                return 0x21;
            }

            if (hwRev > 0x11) {
                return 0x21;
            }

        case 0x202:
        case 0x201:
            switch (hwRev) {
            case 0:
                return 0x10000010;
            case 1:
                return 0x10000011;
            case 2:
                return 0x10000012;
            case 0x10:
                return 0x10000020;
            case 0x11:
                return 0x10000021;
            }

            if (hwRev > 0x11) {
                return 0x10000021;
            }

        case 0x300:
            return 0x100;

        default:
            break;
        }
    }

    gddrSize = OSGetPhysicalMem2Size();

    switch (hwRev) {
    case 0:
        if (gddrSize == 0x4000000) {
            return 0x10;
        } else {
            return 0x10000010;
        }

    case 1:
        if (gddrSize == 0x4000000) {
            return 0x11;
        } else {
            return 0x10000011;
        }

    case 2:
        if (gddrSize == 0x4000000) {
            return 0x12;
        } else {
            return 0x10000012;
        }

    case 0x10:
        if (gddrSize == 0x4000000) {
            return 0x20;
        } else {
            return 0x10000020;
        }

    case 0x11:
        if (gddrSize == 0x4000000) {
            return 0x21;
        } else {
            return 0x10000021;
        }
    }

    if (hwRev > 0x11) {
        if (gddrSize == 0x4000000) {
            return 0x21;
        } else {
            return 0x10000021;
        }
    }

    return BootInfo->consoleType;
} // 0x80210A30

/* Zeroes [base, base + size) and flushes its last 256 KiB out of the cache. */
static void MemClear(void* base, u32 size) {
    void* lastBase = (size > 0x40000) ? (void*)((u32)base + size - 0x40000) : base;

    DCZeroRange(base, size);
    DCFlushRange(lastBase, 0x40000);
}

/*
 * ClearArena
 *
 * Zeroes the MEM1 arena, except the region a restart asked to keep.
 */
static void ClearArena(void) {
    if (!OSIsRestart()) {
        MemClear(OSGetArenaLo(), (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
        return;
    }

    if (__OSRebootParams.regionStart == NULL || !OSIsMEM1Region(__OSRebootParams.regionStart)) {
        MemClear(OSGetArenaLo(), (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
        return;
    }

    ASSERT(__OSRebootParams.regionEnd != NULL);

    if ((u32)OSGetArenaLo() < (u32)__OSRebootParams.regionStart) {
        if ((u32)OSGetArenaHi() <= (u32)__OSRebootParams.regionStart) {
            MemClear(OSGetArenaLo(), (u32)OSGetArenaHi() - (u32)OSGetArenaLo());
        } else {
            MemClear(OSGetArenaLo(), (u32)__OSRebootParams.regionStart - (u32)OSGetArenaLo());

            if ((u32)OSGetArenaHi() > (u32)__OSRebootParams.regionEnd) {
                MemClear(__OSRebootParams.regionEnd,
                         (u32)OSGetArenaHi() - (u32)__OSRebootParams.regionEnd);
            }
        }
    }
} // 0x80210C90

/*
 * ClearMEM2Arena
 *
 * Zeroes the MEM2 arena, except the region a restart asked to keep.
 */
static void ClearMEM2Arena(void) {
    if (!OSIsRestart()) {
        MemClear(OSGetMEM2ArenaLo(), (u32)OSGetMEM2ArenaHi() - (u32)OSGetMEM2ArenaLo());
        return;
    }

    if (__OSRebootParams.regionStart == NULL || !OSIsMEM2Region(__OSRebootParams.regionStart)) {
        MemClear(OSGetMEM2ArenaLo(), (u32)OSGetMEM2ArenaHi() - (u32)OSGetMEM2ArenaLo());
        return;
    }

    ASSERT(__OSRebootParams.regionEnd != NULL);

    if ((u32)OSGetMEM2ArenaLo() < (u32)__OSRebootParams.regionStart) {
        if ((u32)OSGetMEM2ArenaHi() <= (u32)__OSRebootParams.regionStart) {
            MemClear(OSGetMEM2ArenaLo(), (u32)OSGetMEM2ArenaHi() - (u32)OSGetMEM2ArenaLo());
        } else {
            MemClear(OSGetMEM2ArenaLo(), (u32)__OSRebootParams.regionStart - (u32)OSGetMEM2ArenaLo());

            if ((u32)OSGetMEM2ArenaHi() > (u32)__OSRebootParams.regionEnd) {
                MemClear(__OSRebootParams.regionEnd,
                         (u32)OSGetMEM2ArenaHi() - (u32)__OSRebootParams.regionEnd);
            }
        }
    }
} // 0x80210E70

/*
 * InquiryCallback
 *
 * Completion of the drive inquiry started by OSInit: publishes the device
 * code, or 1 when the inquiry failed.
 */
static void InquiryCallback(s32 result, DVDCommandBlock* block) {
    switch (block->state) {
    case 0:
        __OSDeviceCode = (u16)(0x8000 | DriveInfo.deviceCode);
        return;
    default:
        __OSDeviceCode = 1;
        return;
    }
} // 0x80211060

/*
 * CheckTargets
 *
 * Panics when the boot program or the apploader was built for another
 * target than RVL. The line numbers passed to OSPanic are retail's __LINE__
 * values of the SDK's OS.c.
 */
static void CheckTargets(void) {
    switch (*(u8*)OSPhysicalToCached(0x315C)) {
    case 0x80:
        break;
    case 0x81:
        OSReport("OS ERROR: boot program is not for RVL target. Please use correct boot program.\n");
        OSPanic(__FILE__, 1153, "Failed to run app");
        break;
    default:
        break;
    }

    switch (*(u8*)OSPhysicalToCached(0x315D)) {
    case 0x80:
        break;
    case 0x81:
        OSReport("OS ERROR: apploader[D].img is not for RVL target. Please use correct apploader[D].img.\n");
        OSPanic(__FILE__, 1171, "Failed to run app");
        break;
    default:
        break;
    }
}

/*
 * CheckFirmare
 *
 * Shows the fatal error screen when the running IOS is older than the one
 * the title was built against (recorded in low memory).
 */
static void CheckFirmare(void) {
    OSIOSRev iosRev;
    u32 firmware;
    GXColor bg = {0x00, 0x00, 0xFF, 0x00};
    GXColor fg = {0xFF, 0xFF, 0xFF, 0x00};

    __OSGetIOSRev(&iosRev);
    firmware = iosRev.major << 0x10;
    firmware |= iosRev.minor << 0x8;
    firmware |= iosRev.micro;

    if (iosRev.major != (*(u32*)0x80003188 >> 0x10) ||
        (iosRev.major == (*(u32*)0x80003188 >> 0x10) && firmware < *(u32*)0x80003188)) {
        OSReport("OS ERROR: This firmware is an improper version for this SDK. Please use a correct Firmware.\n");
        OSFatal(fg, bg,
                "\n\nERROR #002\nAn error has occurred.\nPress the Eject Button, remove the\nGame Disc, and turn off the power to \nthe console. \nPlease read the Wii Operations Manual \nfor further instructions.\n");
        OSPanic(__FILE__, 1236, "Failed to run app");
    }
}

/*
 * ReportOSInfo
 *
 * Prints the OS banner: kernel build, console type, IOS revision and the
 * memory layout.
 */
static void ReportOSInfo(void) {
    u32 consoleType;
    u32 sysMemSize;
    OSIOSRev ios;

    OSReport("\nRevolution OS\n");
    OSReport("Kernel built : %s %s\n", BUILD_DATE, BUILD_TIME);
    OSReport("Console Type : ");

    consoleType = OSGetConsoleType();

    switch (consoleType & 0xF0000000) {
    case 0:
        switch (consoleType) {
        case 0x11:
            OSReport("Pre-production board 1\n");
            break;
        case 0x12:
            OSReport("Pre-production board 2-1\n");
            break;
        case 0x20:
            OSReport("Pre-production board 2-2\n");
            break;
        case 0x100:
            OSReport("RVA 1\n");
            break;
        default:
            OSReport("Retail %d\n", consoleType);
            break;
        }
        break;
    case 0x10000000:
        switch (consoleType) {
        case 0x10000021:
            OSReport("NDEV 2.1\n");
            break;
        case 0x10000020:
            OSReport("NDEV 2.0\n");
            break;
        case 0x10000012:
            OSReport("NDEV 1.2\n");
            break;
        case 0x10000011:
            OSReport("NDEV 1.1\n");
            break;
        case 0x10000010:
            OSReport("NDEV 1.0\n");
            break;
        case 0x10000008:
            OSReport("Revolution Emulator\n");
            break;
        default:
            OSReport("Emulation platform (%08x)\n", consoleType);
            break;
        }
        break;
    case 0x20000000:
        OSReport("TDEV-based emulation HW%d\n", (consoleType & ~0xF0000000) - 3);
        break;
    default:
        OSReport("%08x\n", consoleType);
        break;
    }

    __OSGetIOSRev(&ios);
    OSReport("Firmware     : %d.%d.%d ", ios.major, ios.minor, ios.micro);
    OSReport("(%d/%d/%d)\n", ios.month, ios.date, ios.year);
    sysMemSize = OSGetConsoleSimulatedMem1Size() + OSGetConsoleSimulatedMem2Size();
    OSReport("Memory %d MB\n", sysMemSize / (1024 * 1024));

    OSReport("MEM1 Arena : 0x%x - 0x%x\n", OSGetMEM1ArenaLo(), OSGetMEM1ArenaHi());
    OSReport("MEM2 Arena : 0x%x - 0x%x\n", OSGetMEM2ArenaLo(), OSGetMEM2ArenaHi());
} // 0x802110A0

/*
 * OSInit
 *
 * Brings up the OS: arenas, exception vectors, interrupts, cache, EXI/SI,
 * threads, audio, IPC and the drive. Runs once.
 */
void OSInit(void) {
    void* bi2StartAddr;
    void* arenaLo;

    if (AreWeInitialized == FALSE) {
        AreWeInitialized = TRUE;

        __OSStartTime = __OSGetSystemTime();
        OSDisableInterrupts();

        __OSGetExecParams(&__OSRebootParams);
        PPCMtmmcr0(0);
        PPCMtmmcr1(0);
        PPCMtpmc1(0);
        PPCMtpmc2(0);
        PPCMtpmc3(0);
        PPCMtpmc4(0);
        PPCMthid4(0x83900000);
        PPCDisableSpeculation();
        PPCSetFpNonIEEEMode();

        BootInfo = (OSBootInfo*)OSPhysicalToCached(0);
        BI2DebugFlag = 0;
        __DVDLongFileNameFlag = 0;

        bi2StartAddr = (void*)(*(u32*)OSPhysicalToCached(0xF4));
        if (bi2StartAddr) {
            BI2DebugFlag = (void*)((char*)bi2StartAddr + 0xC);
            __PADSpec = ((u32*)bi2StartAddr)[9];
            *(u8*)OSPhysicalToCached(0x30E8) = *BI2DebugFlag;
            *(u8*)OSPhysicalToCached(0x30E9) = __PADSpec;
        } else if (BootInfo->arenaHi) {
            BI2DebugFlagHolder = *(u8*)OSPhysicalToCached(0x30E8);
            BI2DebugFlag = &BI2DebugFlagHolder;
            __PADSpec = *(u8*)OSPhysicalToCached(0x30E9);
        }

        __DVDLongFileNameFlag = 1;

        arenaLo = (void*)*(u32*)OSPhysicalToCached(0x310C);
        if (arenaLo == 0) {
            if (OSIsMEM1Region(__ArenaLo)) {
                arenaLo = !BootInfo->arenaLo ? __ArenaLo : BootInfo->arenaLo;

                if (!BootInfo->arenaLo && (BI2DebugFlag && (*BI2DebugFlag < 2))) {
                    arenaLo = (void*)OSRoundUp32B(_stack_addr);
                }
            } else {
                arenaLo = (void*)0x80004000;
            }
        }

        OSSetMEM1ArenaLo(arenaLo);

        arenaLo = (void*)*(u32*)OSPhysicalToCached(0x3110);
        if (arenaLo == 0) {
            arenaLo = !BootInfo->arenaHi ? (void*)__ArenaHi : BootInfo->arenaHi;
        }

        OSSetMEM1ArenaHi(arenaLo);

        arenaLo = (void*)*(u32*)OSPhysicalToCached(0x3124);
        if (arenaLo != 0) {
            if (OSIsMEM2Region(__ArenaLo)) {
                arenaLo = __ArenaLo;

                if (BI2DebugFlag && (*BI2DebugFlag < 2)) {
                    arenaLo = (void*)OSRoundUp32B(_stack_addr);
                }
            } else {
                if ((u32)arenaLo >= 0x90000000 && (u32)arenaLo < 0x90000800) {
                    arenaLo = (void*)0x90000800;
                }
            }

            OSSetMEM2ArenaLo(arenaLo);
        }

        arenaLo = (void*)*(u32*)OSPhysicalToCached(0x3128);
        if (arenaLo != 0) {
            OSSetMEM2ArenaHi(arenaLo);
        }

        __OSInitIPCBuffer();
        OSExceptionInit();
        __OSInitSystemCall();
        __OSInitAlarm();
        __OSModuleInit();
        __OSInterruptInit();
        __OSContextInit();
        __OSCacheInit();
        EXIInit();
        SIInit();
        __OSInitSram();
        __OSThreadInit();
        __OSInitAudioSystem();

        DisableWriteGatherPipe();

        if (!__OSInIPL) {
            __OSInitMemoryProtection();
        }

        ReportOSInfo();
        OSRegisterVersion(__OSVersion);

        if (BI2DebugFlag && (*BI2DebugFlag >= 2)) {
            EnableMetroTRKInterrupts();
        }

        if (!__OSInNandBoot && !__OSInReboot) {
            ClearArena();
            ClearMEM2Arena();
        }

        OSEnableInterrupts();
        IPCCltInit();

        if (!__OSInNandBoot && !__OSInReboot) {
            __OSInitSTM();

            SCInit();

            /* Wait until SC is no longer busy. */
            while (SCCheckStatus() == 1) {
            }

            __OSInitNet();
        }

        if (!__OSInIPL) {
            CheckTargets();
            CheckFirmare();
            DVDInit();

            if (__OSIsGcam) {
                __OSDeviceCode = 0x9000;
            } else if (!__OSDeviceCode) {
                DCInvalidateRange(&DriveInfo, sizeof(DriveInfo));
                DVDInquiryAsync(&DriveBlock, &DriveInfo, InquiryCallback);
            }

            if (OSGetAppType() == 0x80 && !__OSInReboot) {
                if (!__DVDCheckDevice()) {
                    OSReturnToMenu();
                }
            }
        }

        if (!__OSInIPL && !__OSInNandBoot) {
            __OSInitPlayTime();
        }

        if (!__OSInIPL && !__OSInNandBoot && !__OSInReboot) {
            __OSStartPlayRecord();
        }
    }
} // 0x80211350

/* Low-memory address of the first-level vector of each exception (indexed by exception number). */
static u32 __OSExceptionLocations[] = {
    0x00000100, 0x00000200, 0x00000300, 0x00000400, 0x00000500, 0x00000600, 0x00000700, 0x00000800,
    0x00000900, 0x00000C00, 0x00000D00, 0x00000F00, 0x00001300, 0x00001400, 0x00001700,
};

/*
 * OSExceptionInit
 *
 * Copies OSExceptionVector to every exception vector (patching in the
 * exception number), installs the DB integrator the first time, and points
 * every exception at OSDefaultExceptionHandler.
 */
static void OSExceptionInit(void) {
    __OSException exception;
    void* destAddr;

    /* The exception number is patched into this instruction of the vector. */
    u32* opCodeAddr;
    u32 oldOpCode;

    /* Address range of the vector code to copy. */
    u8* handlerStart;
    u32 handlerSize;

    opCodeAddr = (u32*)__OSEVSetNumber;
    oldOpCode = *opCodeAddr;
    handlerStart = (u8*)__OSEVStart;
    handlerSize = (u32)((u8*)__OSEVEnd - (u8*)__OSEVStart);

    /* Install the DB integrator, only if we are the first OSInit to be run. */
    destAddr = (void*)OSPhysicalToCached(OS_DBJUMPPOINT_ADDR);
    if (*(u32*)destAddr == 0) {
        DBPrintf("Installing OSDBIntegrator\n");
        memcpy(destAddr, (void*)__OSDBINTSTART, (u32)__OSDBINTEND - (u32)__OSDBINTSTART);
        DCFlushRangeNoSync(destAddr, (u32)__OSDBINTEND - (u32)__OSDBINTSTART);
        __sync();
        ICInvalidateRange(destAddr, (u32)__OSDBINTEND - (u32)__OSDBINTSTART);
    }

    for (exception = 0; exception < __OS_EXCEPTION_MAX; exception++) {
        if (BI2DebugFlag && (*BI2DebugFlag >= 2)) {
            if (__DBIsExceptionMarked(exception)) {
                DBPrintf(">>> OSINIT: exception %d commandeered by TRK\n", exception);
                continue;
            }
        }

        /* Patch the exception number into the copy of the vector. */
        *opCodeAddr = oldOpCode | exception;

        /* Patch the DB vector: a jump to the debugger, or nops. */
        if (__DBIsExceptionMarked(exception)) {
            DBPrintf(">>> OSINIT: exception %d vectored to debugger\n", exception);
            memcpy((void*)__DBVECTOR, (void*)__OSDBJUMPSTART, (u32)__OSDBJUMPEND - (u32)__OSDBJUMPSTART);
        } else {
            u32* ops = (u32*)__DBVECTOR;
            int cb;

            for (cb = 0; cb < (u32)__OSDBJUMPEND - (u32)__OSDBJUMPSTART; cb += sizeof(u32)) {
                *ops++ = NOP;
            }
        }

        destAddr = (void*)OSPhysicalToCached(__OSExceptionLocations[(u32)exception]);
        memcpy(destAddr, handlerStart, handlerSize);
        DCFlushRangeNoSync(destAddr, handlerSize);
        __sync();
        ICInvalidateRange(destAddr, handlerSize);
    }

    OSExceptionTable = (void*)OSPhysicalToCached(OS_EXCEPTIONTABLE_ADDR);

    for (exception = 0; exception < __OS_EXCEPTION_MAX; exception++) {
        __OSSetExceptionHandler(exception, OSDefaultExceptionHandler);
    }

    /* Restore the patched instruction so the vector can be installed again after a restart. */
    *opCodeAddr = oldOpCode;

    DBPrintf("Exceptions initialized...\n");
} // 0x802118B0

/*
 * __OSDBIntegrator
 *
 * Debugger entry stub, copied to low memory 0x60: saves the return address
 * in the DB interface, turns on address translation and jumps to the
 * debugger's handler.
 */
// ORIGINAL-ASM: an asm function in the public RVL SDK OS.c
static asm void __OSDBIntegrator(void) {
    nofralloc
entry __OSDBINTSTART
    li r5, OS_DBINTERFACE_ADDR
    mflr r3
    stw r3, 0xC(r5) /* exception return address */
    lwz r3, 0x8(r5) /* exception destination */
    oris r3, r3, OS_CACHED_REGION_PREFIX
    mtlr r3
    li r3, 0x30 /* MSR_IR | MSR_DR: turn on memory addressing */
    mtmsr r3
    blr
entry __OSDBINTEND
} // 0x80211B30

/*
 * __OSDBJump
 *
 * The call into the DB integrator that replaces the NOP at __DBVECTOR when
 * an exception is vectored to the debugger.
 */
// ORIGINAL-ASM: an asm function in the public RVL SDK OS.c
static asm void __OSDBJump(void) {
    nofralloc
entry __OSDBJUMPSTART
    bla OS_DBJUMPPOINT_ADDR
entry __OSDBJUMPEND
} // 0x80211B60

/*
 * __OSSetExceptionHandler
 *
 * Installs handler for exception and returns the previous one.
 */
__OSExceptionHandler __OSSetExceptionHandler(__OSException exception, __OSExceptionHandler handler) {
    __OSExceptionHandler oldHandler;

    oldHandler = OSExceptionTable[exception];
    OSExceptionTable[exception] = handler;
    return oldHandler;
} // 0x80211B70

/*
 * __OSGetExceptionHandler
 *
 * Returns the handler installed for exception.
 */
__OSExceptionHandler __OSGetExceptionHandler(__OSException exception) {
    return OSExceptionTable[exception];
} // 0x80211B90

/*
 * OSExceptionVector
 *
 * First-level exception vector, copied to every exception address by
 * OSExceptionInit with the exception number patched in at __OSEVSetNumber:
 * saves r3-r5 and the special registers into the current context, then
 * jumps to the exception's handler with address translation on.
 */
// ORIGINAL-ASM: an asm function in the public RVL SDK OS.c
static asm void OSExceptionVector(void) {
    nofralloc
entry __OSEVStart
    /* Save r4 into SPRG0. */
    mtsprg 0, r4

    /* Load the physical address of the current context into r4. */
    lwz r4, OS_CURRENTCONTEXT_PADDR

    /* Save r3-r5 into the current context. */
    stw r3, OS_CONTEXT_R3(r4)
    mfsprg r3, 0
    stw r3, OS_CONTEXT_R4(r4)
    stw r5, OS_CONTEXT_R5(r4)

    lhz r3, OS_CONTEXT_STATE(r4)
    ori r3, r3, OS_CONTEXT_STATE_EXC
    sth r3, OS_CONTEXT_STATE(r4)

    /* Save the special registers. */
    mfcr r3
    stw r3, OS_CONTEXT_CR(r4)
    mflr r3
    stw r3, OS_CONTEXT_LR(r4)
    mfctr r3
    stw r3, OS_CONTEXT_CTR(r4)
    mfxer r3
    stw r3, OS_CONTEXT_XER(r4)
    mfsrr0 r3
    stw r3, OS_CONTEXT_SRR0(r4)
    mfsrr1 r3
    stw r3, OS_CONTEXT_SRR1(r4)
    mr r5, r3

entry __DBVECTOR
    nop

    /* Turn on address translation at the next rfi. */
    mfmsr r3
    ori r3, r3, MSR_IR | MSR_DR
    mtsrr1 r3

    /* The exception number, patched by OSExceptionInit. */
entry __OSEVSetNumber
    addi r3, 0, 0x0000

    /* Load the virtual address of the current context into r4. */
    lwz r4, OS_CURRENTCONTEXT_VADDR

    /* A non-recoverable exception goes straight to the default handler. */
    rlwinm. r5, r5, 0, 30, 30 /* MSR[RI] */
    bne recoverable
    addis r5, 0, OSDefaultExceptionHandler@ha
    addi r5, r5, OSDefaultExceptionHandler@l
    mtsrr0 r5
    rfi

recoverable:
    /* Locate the exception handler. */
    rlwinm r5, r3, 2, 22, 29
    lwz r5, OS_EXCEPTIONTABLE_ADDR(r5)
    mtsrr0 r5

    /* r3 = exception number, r4 = context, srr0 = handler; the handler restores the rest. */
    rfi

entry __OSEVEnd
    nop
} // 0x80211BA0

/*
 * OSDefaultExceptionHandler
 *
 * Saves the registers the vector left live into context and reports the
 * exception through __OSUnhandledException.
 */
// ORIGINAL-ASM: an asm function in the public RVL SDK OS.c
asm void OSDefaultExceptionHandler(register __OSException exception, register OSContext* context) {
    nofralloc
    stw r0, OS_CONTEXT_R0(context)
    stw r1, OS_CONTEXT_R1(context)
    stw r2, OS_CONTEXT_R2(context)
    stmw r6, OS_CONTEXT_R6(context)
    mfspr r0, GQR1
    stw r0, OS_CONTEXT_GQR1(context)
    mfspr r0, GQR2
    stw r0, OS_CONTEXT_GQR2(context)
    mfspr r0, GQR3
    stw r0, OS_CONTEXT_GQR3(context)
    mfspr r0, GQR4
    stw r0, OS_CONTEXT_GQR4(context)
    mfspr r0, GQR5
    stw r0, OS_CONTEXT_GQR5(context)
    mfspr r0, GQR6
    stw r0, OS_CONTEXT_GQR6(context)
    mfspr r0, GQR7
    stw r0, OS_CONTEXT_GQR7(context)
    mfdsisr r5
    mfdar r6

    stwu r1, -8(r1)
    b __OSUnhandledException
} // 0x80211C40

/*
 * __OSPSInit
 *
 * Enables paired singles and the graphics quantization registers.
 */
// ORIGINAL-ASM: the GQR-clearing block is inline asm in the public RVL SDK OS.c
void __OSPSInit(void) {
    PPCMthid2(PPCMfhid2() | 0x80000000 | 0x20000000);
    ICFlashInvalidate();
    __sync();

    asm {
        li r3, 0
        mtspr GQR0, r3
        mtspr GQR1, r3
        mtspr GQR2, r3
        mtspr GQR3, r3
        mtspr GQR4, r3
        mtspr GQR5, r3
        mtspr GQR6, r3
        mtspr GQR7, r3
    }
} // 0x80211CA0

/*
 * __OSGetDIConfig
 *
 * Returns the low byte of the DVD interface configuration register.
 */
u32 __OSGetDIConfig(void) {
    return __DIRegs[9] & 0xFF;
} // 0x80211D00

/*
 * OSRegisterVersion
 *
 * Prints a library version banner.
 */
void OSRegisterVersion(const char* version) {
    OSReport("%s\n", version);
} // 0x80211D10

/* Game code "HAEA" reported while the system menu is the running program. */
static const char* AppGameNameForSysMenu = "HAEA";

/*
 * OSGetAppGamename
 *
 * Returns the four-character game code of the running program, in a static
 * buffer.
 */
const char* OSGetAppGamename(void) {
    int i;
    const char* temp = (const char*)OSPhysicalToCached(0x3194);
    const char* name = temp;

    if (__OSInIPL) {
        name = AppGameNameForSysMenu;
    } else if ('0' > *temp || ('9' < *temp && *temp < 'A') || *temp > 'Z') {
        name = (const char*)OSPhysicalToCached(0x3180);
    }

    for (i = 0; i < 4; i++) {
        GameNameBuffer[i] = name[i];
    }
    GameNameBuffer[4] = '\0';

    return GameNameBuffer;
} // 0x80211D20

/*
 * OSGetAppType
 *
 * Returns the application type byte from low memory; the system menu
 * reports 0x40.
 */
u8 OSGetAppType(void) {
    if (__OSInIPL) {
        return 0x40;
    }

    return *((u8*)OSPhysicalToCached(0x3184)) & 0xFF;
} // 0x80211DB0
