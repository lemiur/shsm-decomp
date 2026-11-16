// Reference: zeldaret/tp libs/revolution/src/os/OSExec.c
// Reference: doldecomp/brawl src/revolution/OS/OSExec.c (the locals of __OSLaunchNextFirmware)
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>
#include <revolution/esp.h>
#include <revolution/hardware.h>
#include <revolution/ipc.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* Words the boot code and the apploader leave in low memory (names invented). */
#define OS_LOWMEM_WORD(addr) (*(u32*)OSPhysicalToCached(addr))
#define OS_BOOT_FLAG_TMD_TICKET 0x80
#define OS_MEM2_SIZE_ADDR 0x311C
#define OS_MEM2_ARENA_END_ADDR 0x3120
#define OS_IOS_BLOCK_ADDR 0x3100
#define OS_IOS_BLOCK_SIZE 0x100

/* Where the apploader and the old-style reboot code are loaded in MEM1. */
#define APPLOADER_LOAD_ADDR ((void*)0x81200000)
#define REBOOT_LOAD_ADDR ((void*)0x81330000)
#define DEFAULT_APPLOADER_POS 0x910

/* The block of arguments handed to the next program. */
#define ARGS_SIZE 0x2000

DVDDiskID id;

static volatile BOOL Prepared; // volatile: set by Callback while __OSBootDolSimple spins on it
BOOL __OSInReboot;
u32 __OSNextPartitionType = 0;

/* Header at the start of the apploader image. */
typedef struct AppLoaderStruct {
    char date[16];
    u32 entry;
    u32 size;
    u32 rebootSize;
    u32 reserved2;
} AppLoaderStruct;

typedef void (*AppInitCallback)(void (*report)(const char*, ...));
typedef int (*AppGetNextCallback)(void** addr, u32* length, u32* offset);
typedef void* (*AppGetEntryCallback)(void);
typedef void (*AppLoaderCallback)(AppInitCallback* init, AppGetNextCallback* next, AppGetEntryCallback* entry);

/*
 * PackArgs
 *
 * Lays the argument vector out at the end of the 8 KB block at addr: the
 * strings, then the pointer list (as offsets from addr), then argc.
 */
static BOOL PackArgs(void* addr, s32 argc, char** argv) {
    s32 numArgs;
    char* bootInfo2;
    char* ptr;
    char** list;
    u32 i;

    bootInfo2 = (char*)addr;
    memset(bootInfo2, 0, ARGS_SIZE);

    if (argc == 0) {
        *(u32*)(bootInfo2 + 8) = 0;
    } else {
        numArgs = argc;
        ptr = bootInfo2 + ARGS_SIZE;
        while (--argc >= 0) {
            ptr -= strlen(argv[argc]) + 1;
            strcpy(ptr, argv[argc]);
            argv[argc] = (char*)(ptr - bootInfo2);
        }

        ptr = bootInfo2 + ((ptr - bootInfo2) & ~0x3);
        ptr -= ((numArgs + 1) * 4);
        list = (char**)ptr;

        for (i = 0; i < numArgs + 1; i++) {
            list[i] = argv[i];
        }

        ptr -= 4;
        *(u32*)ptr = numArgs;

        *(u32*)(bootInfo2 + 8) = (ptr - bootInfo2);
    }

    return TRUE;
} // 0x802145E0

/*
 * Utf16ToArg
 *
 * Writes the bytes of a UTF-16 name as lower-case hex digits.
 */
static BOOL Utf16ToArg(char* dstArg, u16* srcName) {
    char* srcPtr;
    char* dstPtr;
    u8 i, mask, shift;

    if (srcName != 0) {
        srcPtr = (char*)srcName;
        dstPtr = dstArg;

        while (*srcPtr || *(srcPtr + 1)) {
            for (i = 0; i < 4; i++) {
                mask = (u8)((i & 0x1) ? 0xF : 0xF0);
                shift = (u8)((i & 0x1) ? 0 : 4);

                if (0 <= ((*srcPtr & mask) >> shift) && ((*srcPtr & mask) >> shift) < 0xA) {
                    *dstPtr = (char)(((*srcPtr & mask) >> shift) + 0x30);
                } else if (0xA <= ((*srcPtr & mask) >> shift) && ((*srcPtr & mask) >> shift) < 0x10) {
                    *dstPtr = (char)(((*srcPtr & mask) >> shift) + 0x57);
                } else {
                    return FALSE;
                }

                dstPtr++;

                if (i & 1) {
                    srcPtr++;
                }
            }
        }

        *dstPtr++ = 0;
        return TRUE;
    }

    return FALSE;
} // 0x80214760

/*
 * PackInstallerArgs
 *
 * PackArgs for the installer partition: arguments 2 and up at even
 * positions are UTF-16 names and are passed as hex strings.
 */
BOOL PackInstallerArgs(void* addr, s32 argc, char* argv[]) {
    s32 numArgs;
    char* bootInfo2;
    char* ptr;
    char** list;
    u32 i;

    bootInfo2 = (char*)addr;
    memset(bootInfo2, 0, ARGS_SIZE);

    if (argc == 0) {
        *(u32*)(bootInfo2 + 8) = 0;
    } else {
        numArgs = argc;
        ptr = bootInfo2 + ARGS_SIZE;

        while (--argc >= 0) {
            if (argc < 2 || (argc % 2)) {
                ptr -= strlen(argv[argc]) + 1;
                strcpy(ptr, argv[argc]);
                argv[argc] = (char*)(ptr - bootInfo2);
            } else {
                ptr -= (wcslen((wchar_t*)argv[argc]) * 4 + 1);
                Utf16ToArg(ptr, (u16*)argv[argc]);
                argv[argc] = (char*)(ptr - bootInfo2);
            }
        }

        ptr = bootInfo2 + ((ptr - bootInfo2) & ~0x3);
        ptr -= ((numArgs + 1) * 4);
        list = (char**)ptr;

        for (i = 0; i < numArgs + 1; i++) {
            list[i] = argv[i];
        }

        ptr -= 4;
        *(u32*)ptr = numArgs;

        *(u32*)(bootInfo2 + 8) = (ptr - bootInfo2);
    }

    return TRUE;
} // 0x802148C0

/*
 * Run
 *
 * Invalidates the instruction cache and jumps to entryPoint.
 */
// ORIGINAL-ASM: an asm function in the public RVL SDK OSExec.c
static asm void Run(register void* entryPoint) {
    fralloc

    bl ICFlashInvalidate
    sync
    isync
    mtctr entryPoint
    bctr

    frfree
    blr
} // 0x80214A90

/* Copies the restart parameters to addr and publishes them at __OSExecParams. */
static void __OSSetExecParams(const OSExecParams* params, OSExecParams* addr) {
    memcpy(addr, params, sizeof(OSExecParams));
    __OSExecParams = addr;
}

/* Publishes the restart parameters, then jumps to the DOL entry point with interrupts off. */
static void StartDol(const OSExecParams* params, void* entry) {
    OSExecParams* paramsWork = OSAllocFromMEM1ArenaLo(sizeof(OSExecParams), 1);

    __OSSetExecParams(params, paramsWork);
    __PIRegs[9] = 7;

    OSDisableInterrupts();
    Run(entry);
}

/* Reads length bytes from the disc at offset (in units of 4 bytes) into addr, waiting for completion. */
static void ReadDisc(void* addr, s32 length, s32 offset) {
    DVDCommandBlock block;

    DVDReadAbsAsyncPrio(&block, addr, length, offset, NULL, 0);

    while (DVDGetCommandBlockStatus(&block)) {
        if (DVDGetCommandBlockStatus(&block) > 2 || DVDGetCommandBlockStatus(&block) < 0) {
            __OSReturnToMenuForError();
        }
    }
}

static void Callback(s32 result, DVDCommandBlock* block) {
    Prepared = TRUE;
} // 0x80214AD0

/*
 * __OSGetExecParams
 *
 * Copies out the restart parameters left by the previous program, if any.
 */
void __OSGetExecParams(OSExecParams* params) {
    if (0x80000000 <= (u32)__OSExecParams) {
        memcpy(params, __OSExecParams, sizeof(OSExecParams));
    } else {
        params->valid = FALSE;
    }
} // 0x80214AE0

static volatile u32 DVDLowIntType; // volatile: set by the DVDLow callback while the caller spins on it

static void callback(u32 intType) {
    DVDLowIntType = intType;
} // 0x80214B10

/* Waits for the DVDLow command started above to finish and leaves for the system menu if it failed. */
#define DVDLOW_WAIT(line)                                                           \
    do {                                                                            \
        while (DVDLowIntType == 0) {                                                \
        }                                                                           \
        if (DVDLowIntType != 1) {                                                   \
            OSReport("\nOSExec(): Failed to exec %d in %d\n", DVDLowIntType, line); \
            __OSReturnToMenuForError();                                             \
        }                                                                           \
    } while (0)

/* Reports a failed ES call and resets. */
#define ES_CHECK(rc, line)                                           \
    if (rc) {                                                        \
        OSReport("\nOSExec(): Failed to exec %d in %d\n", rc, line); \
        __OSHotResetForError();                                      \
    }

/*
 * __OSLaunchNextFirmware
 *
 * Opens the next game partition on the disc, then launches the IOS the
 * partition's title wants and re-opens the partition under it.
 */
void __OSLaunchNextFirmware(void) {
    u8 i;
    int rc = -1;
    u32 ticketCnt = 1;
    ESTicketView* views;
    ESTicketView* ticket;
    ESTitleIdAligned version; // the IOS title the TMD asks for, kept 32-byte aligned like every ES title ID
    u32 mem2Size;
    u32 mem2ArenaEnd;
    DVDGameTOC* gameTOC;
    DVDPartitionInfo* partitionInfo;
    DVDPartitionInfo* current;
    ESTitleMeta* meta;
    u32 tmd = 0;
    DVDPartitionInfo* info;

    gameTOC = OSAllocFromMEM1ArenaLo(0x20, 0x20);
    partitionInfo = OSAllocFromMEM1ArenaLo(0x800, 0x20);
    meta = OSAllocFromMEM1ArenaLo(0x4A00, 0x40);
    ticket = OSAllocFromMEM1ArenaLo(0xE0, 0x20);

    if (__OSNextPartitionType == __OSLaunchPartitionType && __OSLaunchPartitionOffset != 0) {
        rc = ESP_InitLib();
        if (!rc) {
            rc = ESP_DiGetTicketView(NULL, ticket);
        }

        if (!rc) {
            rc = ESP_DiGetTmd(NULL, &tmd);
        }

        if (!rc) {
            rc = ESP_DiGetTmd(meta, &tmd);
        }

        ESP_CloseLib();

        if (OSPlayTimeIsLimited()) {
            __OSPlayTimeType type = OSPLAYTIME_PERMANENT;
            u32 playTime = -1;
            __OSGetPlayTime(ticket, &type, &playTime);
            if (playTime == 0) {
                __OSWriteExpiredFlag();
                __OSReturnToMenuForError();
            }
        }
    }

    if (!rc) {
        current = partitionInfo;
        current->type = __OSLaunchPartitionType;
        current->gamePartition = (DVDGamePartition*)__OSLaunchPartitionOffset;
    } else {
        DVDLowIntType = 0;
        DVDLowClosePartition(callback);
        DVDLOW_WAIT(920);

        DVDLowIntType = 0;
        DVDLowUnencryptedRead((void*)gameTOC, 0x20, 0x10000, callback);
        DVDLOW_WAIT(930);

        DVDLowIntType = 0;
        DVDLowUnencryptedRead(partitionInfo, 0x800, (u32)gameTOC->partitionInfos, callback);
        DVDLOW_WAIT(942);

        current = NULL;
        info = partitionInfo;

        for (i = 0; i < gameTOC->numGamePartitions; i++) {
            if (info->type == __OSNextPartitionType) {
                current = info;
            }

            info++;
        }

        if (!current) {
            gameTOC++;
            info = partitionInfo + 4;

            for (i = 0; i < gameTOC->numGamePartitions; i++) {
                if (info->type == __OSNextPartitionType) {
                    current = info;
                }

                info++;
            }
        }

        if (!current) {
            OSReport("\nOSExec(): The specified game doesn't exist in the disc\n");
            __OSReturnToMenuForError();
        }

        __OSLaunchPartitionType = current->type;
        __OSLaunchPartitionOffset = (u32)current->gamePartition;

        DVDLowIntType = 0;

        if (__OSLockedFlag == OS_BOOT_FLAG_TMD_TICKET) {
            DVDLowOpenPartitionWithTmdAndTicketView((u32)current->gamePartition, ticket, tmd, meta, 0, 0, callback);
        } else {
            DVDLowOpenPartition((u32)current->gamePartition, NULL, 0, NULL, meta, callback);
        }

        DVDLOW_WAIT(1003);

        DVDLowIntType = 0;
        DVDLowClosePartition(callback);
        DVDLOW_WAIT(1013);
    }

    version = meta->head.sysVersion;

    rc = ESP_InitLib();
    ES_CHECK(rc, 1024);

    rc = ESP_GetTicketViews(version, NULL, &ticketCnt);
    if (ticketCnt != 1 || rc) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", rc, 1033);
        __OSHotResetForError();
    }

    views = OSAllocFromMEM1ArenaLo(OSRoundUp32B(ticketCnt * sizeof(ESTicketView)), 0x20);
    rc = ESP_GetTicketViews(version, views, &ticketCnt);
    ES_CHECK(rc, 1042);

    DVDLowFinalize();

    mem2Size = OS_LOWMEM_WORD(OS_MEM2_SIZE_ADDR);
    mem2ArenaEnd = OS_LOWMEM_WORD(OS_MEM2_ARENA_END_ADDR);
    DCStoreRange(OSPhysicalToCached(OS_IOS_BLOCK_ADDR), OS_IOS_BLOCK_SIZE);

    rc = ESP_LaunchTitle(version, views);
    ES_CHECK(rc, 1058);

    ESP_CloseLib();
    DCInvalidateRange(OSPhysicalToCached(OS_IOS_BLOCK_ADDR), OS_IOS_BLOCK_SIZE);

    if (mem2Size < OS_LOWMEM_WORD(OS_MEM2_SIZE_ADDR)) {
        OS_LOWMEM_WORD(0x3120) = mem2Size - (OS_LOWMEM_WORD(0x311C) - OS_LOWMEM_WORD(0x3120));
        OS_LOWMEM_WORD(0x3128) = mem2Size - (OS_LOWMEM_WORD(0x311C) - OS_LOWMEM_WORD(0x3128));
        OS_LOWMEM_WORD(0x3130) = mem2Size - (OS_LOWMEM_WORD(0x311C) - OS_LOWMEM_WORD(0x3130));
        OS_LOWMEM_WORD(0x3134) = mem2Size - (OS_LOWMEM_WORD(0x311C) - OS_LOWMEM_WORD(0x3134));
        OS_LOWMEM_WORD(0x311C) = mem2Size;
    }

    if (mem2ArenaEnd < OS_LOWMEM_WORD(OS_MEM2_ARENA_END_ADDR)) {
        __OSInitMemoryProtection();
    }

    __OSInitIPCBuffer();
    IPCReInit();
    IPCCltReInit();

    DVDLowInit();
    DVDLowIntType = 0;
    DVDLowReadDiskID(&id, callback);
    DVDLOW_WAIT(1102);

    DVDLowIntType = 0;

    if (__OSLockedFlag == OS_BOOT_FLAG_TMD_TICKET) {
        DVDLowOpenPartitionWithTmdAndTicketView((u32)current->gamePartition, ticket, tmd, meta, 0, 0, callback);
    } else {
        DVDLowOpenPartition((u32)current->gamePartition, NULL, 0, NULL, meta, callback);
    }

    DVDLOW_WAIT(1125);
} // 0x80214B20

/*
 * __OSLaunchMenu
 *
 * Launches the system menu (title 00000001-00000002).
 */
void __OSLaunchMenu(void) {
    s32 rc;
    u32 ticketCnt = 1;
    ESTicketView* views;
    ESTitleId titleId = 0x0000000100000002ull;

    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812F0000);
    rc = ESP_InitLib();

    if (rc != 0) {
        return;
    }

    rc = ESP_GetTicketViews(titleId, NULL, &ticketCnt);

    if (ticketCnt != 1 || rc != 0) {
        return;
    }

    views = (ESTicketView*)OSAllocFromMEM1ArenaLo(OSRoundUp32B(sizeof(ESTicketView) * ticketCnt), 32);
    rc = ESP_GetTicketViews(titleId, views, &ticketCnt);

    if (rc != 0) {
        return;
    }

    rc = ESP_LaunchTitle(titleId, views);

    if (rc != 0) {
        return;
    }

    while (1) {
    }
} // 0x802151B0

static s32 GetApploaderPosition(void) {
    static s32 apploaderPosition;

    if (apploaderPosition != 0) {
        return apploaderPosition;
    }

    if (__OSAppLoaderOffset != 0) {
        u32* tgcHeader;
        s32 apploaderOffsetInTGC;

        tgcHeader = OSAllocFromMEM1ArenaLo(0x40, 0x20);
        ReadDisc(tgcHeader, 0x40, __OSAppLoaderOffset >> 2);
        apploaderOffsetInTGC = tgcHeader[14];

        apploaderPosition = (__OSAppLoaderOffset + apploaderOffsetInTGC) >> 2;
    } else {
        apploaderPosition = DEFAULT_APPLOADER_POS;
    }

    return apploaderPosition;
}

static AppLoaderStruct* LoadApploader(void) {
    AppLoaderStruct* header;

    header = (AppLoaderStruct*)OSAllocFromMEM1ArenaLo(sizeof(AppLoaderStruct), 0x20);
    ReadDisc(header, sizeof(AppLoaderStruct), GetApploaderPosition());

    ReadDisc(APPLOADER_LOAD_ADDR, OSRoundUp32B(header->size), GetApploaderPosition() + 0x8);
    ICInvalidateRange(APPLOADER_LOAD_ADDR, OSRoundUp32B(header->size));
    return header;
}

static void* LoadDol(const OSExecParams* params, AppLoaderCallback getInterface) {
    AppInitCallback appInit;
    AppGetNextCallback appGetNext;
    AppGetEntryCallback appGetEntry;
    void* addr;
    u32 length;
    u32 offset;
    OSExecParams* paramsWork;

    getInterface(&appInit, &appGetNext, &appGetEntry);
    paramsWork = (OSExecParams*)OSAllocFromMEM1ArenaLo(sizeof(OSExecParams), 1);
    __OSSetExecParams(params, paramsWork);
    appInit((void (*)(const char*, ...))OSReport);
    OSSetArenaLo(paramsWork);

    while (appGetNext(&addr, &length, &offset) != 0) {
        ReadDisc(addr, length, offset >> __DVDLayoutFormat);
    }

    return appGetEntry();
}

static BOOL IsNewApploader(AppLoaderStruct* header) {
    if (strncmp(header->date, "2004/02/01", 10) > 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 * __OSBootDolSimple
 *
 * Boots the DOL at doloffset on the disc through the apploader.
 */
void __OSBootDolSimple(u32 doloffset, u32 restartCode, void* regionStart, void* regionEnd, BOOL argsUseDefault, s32 argc, char** argv) {
    OSExecParams* params;
    void* dolEntry;
    OSBootInfo* bootInfo;
    char* titleArg;
    AppLoaderStruct* header;

    OSDisableInterrupts();

    if (__OSInReboot) {
        __OSNextPartitionType = __OSLaunchPartitionType;
    }

    __OSRestoreCodeExecOnMEM1(0xBA2CF);

    params = (OSExecParams*)OSAllocFromMEM1ArenaLo(sizeof(OSExecParams), 1);
    params->valid = TRUE;
    params->restartCode = restartCode;
    params->regionStart = regionStart;
    params->regionEnd = regionEnd;
    params->argsUseDefault = argsUseDefault;

    if (!argsUseDefault) {
        params->argsAddr = OSAllocFromMEM1ArenaLo(ARGS_SIZE, 1);

        if (__OSNextPartitionType == 2 && !__OSInReboot) {
            PackInstallerArgs(params->argsAddr, argc, argv);
        } else {
            PackArgs(params->argsAddr, argc, argv);
        }
    }

    DVDInit();
    DVDSetAutoInvalidation(TRUE);
    DVDResume();

    Prepared = FALSE;

    __DVDPrepareResetAsync(Callback);
    __OSMaskInterrupts(0xFFFFFFF0);
    __OSUnmaskInterrupts(0x10);
    OSEnableInterrupts();

    while (Prepared != TRUE) {
    }

    __OSLaunchNextFirmware();

    if (restartCode == 0xA0000000 && !__OSInReboot) {
        ESTitleIdAligned titleID;
        int rc;

        rc = ESP_InitLib();
        if (rc) {
            return;
        }

        rc = ESP_GetTitleId(&titleID);
        if (rc) {
            return;
        }

        rc = ESP_CloseLib();
        if (rc) {
            return;
        }

        titleArg = (char*)((u32)params->argsAddr + (u32)argv[1]);
        snprintf(titleArg, 17, "%016llx", titleID);
    }

    header = LoadApploader();
    if (IsNewApploader(header)) {
        if (doloffset == 0xFFFFFFFF) {
            doloffset = GetApploaderPosition() + ((header->size + 0x20) >> 2);
        }

        params->bootDol = doloffset;
        dolEntry = LoadDol(params, (AppLoaderCallback)header->entry);

        bootInfo = OSPhysicalToCached(0);
        OS_LOWMEM_WORD(0x3180) = *(u32*)bootInfo->DVDDiskID.gameName;
        *(u8*)OSPhysicalToCached(0x3184) = 0x80;

        StartDol(params, dolEntry);
    } else {
        *(volatile u32*)0x812FDFF0 = (u32)regionStart; // volatile: read by the reboot code loaded below
        *(volatile u32*)0x812FDFEC = (u32)regionEnd; // volatile: read by the reboot code loaded below
        *(volatile u8*)0x800030E2 = 1; // volatile: flag read by the reboot code loaded below

        ReadDisc(REBOOT_LOAD_ADDR, OSRoundUp32B(header->rebootSize), GetApploaderPosition() + ((header->size + 0x20) >> 2));
        ICInvalidateRange(REBOOT_LOAD_ADDR, OSRoundUp32B(header->rebootSize));
        OSDisableInterrupts();
        ICFlashInvalidate();
        Run(REBOOT_LOAD_ADDR);
    }
} // 0x80215280

/*
 * __OSBootDol
 *
 * Boots the DOL at doloffset with argv, passing the offset itself as the
 * first argument.
 */
void __OSBootDol(u32 doloffset, u32 restartCode, const char** argv) {
    char doloffInString[20];
    s32 argvlen;
    char** argvToPass;
    s32 i;
    void* saveStart;
    void* saveEnd;

    __OSGetSaveRegion(&saveStart, &saveEnd);
    sprintf(doloffInString, "%d", doloffset);
    argvlen = 0;

    if (argv != 0) {
        while (argv[argvlen] != 0) {
            argvlen++;
        }
    }

    argvlen++;
    argvToPass = OSAllocFromMEM1ArenaLo((argvlen + 1) * 4, 1);
    *argvToPass = doloffInString;

    for (i = 1; i < argvlen; i++) {
        argvToPass[i] = (char*)argv[i - 1];
    }

    __OSBootDolSimple(-1, restartCode, saveStart, saveEnd, FALSE, argvlen, argvToPass);
} // 0x802159E0
