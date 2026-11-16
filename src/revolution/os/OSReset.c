// Reference: zeldaret/tp libs/revolution/src/os/OSReset.c (the retail SDK is older: it also has OSReturnToDataManager and OSReturnToSetting)
#include <revolution/os.h>
#include <revolution/os/OSPlayTime.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>
#include <revolution/esp.h>
#include <revolution/hardware.h>
#include <revolution/pad.h>
#include <revolution/sc.h>
#include <revolution/vi.h>
#include <string.h>

/* The system menu is started with this menu mode (OSStateFlags.menuMode). */
#define MENU_MODE_SYSTEM_MENU 0
#define MENU_MODE_DATA_MANAGER 1

/* Shutdown events passed to __OSShutdownDevices and the shutdown functions. */
#define SD_EVENT_RESTART_FOR_ERROR 1
#define SD_EVENT_POWER_OFF 2
#define SD_EVENT_RESTART 4
#define SD_EVENT_RETURN_TO_MENU 5

/* OSStateFlags.lastShutdown values. */
#define LAST_SHUTDOWN_NORMAL 1
#define LAST_SHUTDOWN_RETURN_TO_MENU 3
#define LAST_SHUTDOWN_IDLE 5

/* OSStateFlags.lastDiscState values. */
#define DISC_STATE_EJECTED 3
#define DISC_STATE_INSERTED 2
#define DISC_STATE_UNCHANGED 1

/* AppType of a program started from a disc DOL and of one started by a channel title. */
#define APP_TYPE_DISC 0x80
#define APP_TYPE_CHANNEL 0x81

/* Bounds of the arena the system menu is launched in. */
#define MENU_ARENA_LO ((void*)0x81280000)
#define MENU_ARENA_HI ((void*)0x812F0000)

/* The reset code in the PI reset register sits in bits 3-31. */
#define PI_RESET_CODE_SHIFT 3

/* DVDGetCoverStatus value for a closed cover. */
#define DVD_COVER_CLOSED 2

#define ENQUEUE_INFO(info, queue)                       \
    do {                                                \
        OSShutdownFunctionInfo* __prev = (queue)->tail; \
        if (__prev == 0) {                              \
            (queue)->head = (info);                     \
        } else {                                        \
            __prev->next = (info);                      \
        }                                               \
        (info)->prev = __prev;                          \
        (info)->next = 0;                               \
        (queue)->tail = (info);                         \
    } while (0);

#define ENQUEUE_INFO_PRIO(info, queue)                                                   \
    do {                                                                                 \
        OSShutdownFunctionInfo* __prev;                                                  \
        OSShutdownFunctionInfo* __next;                                                  \
        for (__next = (queue)->head; __next && (__next->priority <= (info)->priority);   \
             __next = __next->next)                                                      \
            ;                                                                            \
                                                                                         \
        if (__next == 0) {                                                               \
            ENQUEUE_INFO(info, queue);                                                   \
        } else {                                                                         \
            (info)->next = __next;                                                       \
            __prev = __next->prev;                                                       \
            __next->prev = (info);                                                       \
            (info)->prev = __prev;                                                       \
            if (__prev == 0) {                                                           \
                (queue)->head = (info);                                                  \
            } else {                                                                     \
                __prev->next = (info);                                                   \
            }                                                                            \
        }                                                                                \
    } while (0);

static u32 bootThisDol = 0;
BOOL __OSIsReturnToIdle = FALSE;
static OSShutdownFunctionQueue ShutdownFunctionQueue;

static void KillThreads(void);
static void __OSReturnToMenu(u8 menuMode);

/*
 * OSRegisterShutdownFunction
 *
 * Inserts info into the shutdown queue, ordered by priority.
 */
void OSRegisterShutdownFunction(OSShutdownFunctionInfo* info) {
    ENQUEUE_INFO_PRIO(info, &ShutdownFunctionQueue);
} // 0x80218C00

/*
 * __OSCallShutdownFunctions
 *
 * Calls every registered shutdown function in order. Stops after the priority
 * group in which one of them failed. Returns TRUE if all of them succeeded.
 */
BOOL __OSCallShutdownFunctions(BOOL final, u32 event) {
    OSShutdownFunctionInfo* info;
    int err;
    u32 priority;

    priority = 0;
    err = 0;

    for (info = ShutdownFunctionQueue.head; info != 0;) {
        if (err != 0 && priority != info->priority) {
            break;
        }
        err |= !info->func(final, event);
        priority = info->priority;
        info = info->next;
    }

    err |= !__OSSyncSram();
    return err ? FALSE : TRUE;
} // 0x80218C90

/*
 * __OSShutdownDevices
 *
 * Runs the shutdown functions for event (once before and once after interrupts
 * are disabled), then cancels every thread.
 */
void __OSShutdownDevices(u32 event) {
    BOOL disableRecalibration;
    BOOL doRecal;

    switch (event) {
    case 0:
    case 5:
    case 6:
        doRecal = FALSE;
        break;
    case 2:
    case 3:
    case 4:
    case 1:
    default:
        doRecal = TRUE;
        break;
    }

    __OSStopAudioSystem();

    if (!doRecal) {
        disableRecalibration = __PADDisableRecalibration(TRUE);
    }

    do {
    } while (!__OSCallShutdownFunctions(FALSE, event));
    do {
    } while (!__OSSyncSram());

    OSDisableInterrupts();

    __OSCallShutdownFunctions(TRUE, event);

    LCDisable();
    if (!doRecal) {
        __PADDisableRecalibration(disableRecalibration);
    }

    KillThreads();
} // 0x80218D40

/*
 * __OSGetDiscState
 *
 * Disc state to record in the state flags: ejected when the cover is open,
 * otherwise inserted, or unchanged when the RTC flags say nothing happened.
 */
static u8 __OSGetDiscState(u8 last) {
    u32 flags;

    if (__DVDGetCoverStatus() != DVD_COVER_CLOSED) {
        return DISC_STATE_EJECTED;
    } else {
        if ((last == DISC_STATE_UNCHANGED) && (__OSGetRTCFlags(&flags) && !flags)) {
            return DISC_STATE_UNCHANGED;
        } else {
            return DISC_STATE_INSERTED;
        }
    }
}

/*
 * KillThreads
 *
 * Cancels every thread that is sleeping or ready.
 */
static void KillThreads(void) {
    OSThread* thread;
    OSThread* next;

    for (thread = __OSActiveThreadQueue.head; thread; thread = next) {
        next = thread->linkActive.next;
        switch (thread->state) {
        case OS_THREAD_STATE_READY:
        case OS_THREAD_STATE_WAITING:
            OSCancelThread(thread);
            continue;
        default:
            continue;
        }
    }
}

/*
 * OSShutdownSystem
 *
 * Powers the console down: into standby, or back to the system menu when
 * WiiConnect24 wants the console to stay up.
 */
void OSShutdownSystem(void) {
    SCIdleModeInfo idleModeInfo;
    OSIOSRev iosRev;
    OSStateFlags state;

    memset(&idleModeInfo, 0, sizeof(SCIdleModeInfo));

    SCInit();
    while (SCCheckStatus() == 1) {
    }

    SCGetIdleMode(&idleModeInfo);
    __OSStopPlayRecord();
    __OSUnRegisterStateEvent();
    __DVDPrepareReset();
    __OSReadStateFlags(&state);
    state.lastDiscState = __OSGetDiscState(state.lastDiscState);

    if (idleModeInfo.wc24 == 1) {
        state.lastShutdown = LAST_SHUTDOWN_IDLE;
    } else {
        state.lastShutdown = LAST_SHUTDOWN_NORMAL;
    }

    __OSClearRTCFlags();
    __OSWriteStateFlags(&state);
    __OSGetIOSRev(&iosRev);

    if (idleModeInfo.wc24 == 1) {
        __OSIsReturnToIdle = TRUE;

        OSDisableScheduler();
        __OSShutdownDevices(SD_EVENT_RETURN_TO_MENU);
        OSEnableScheduler();
        __OSLaunchMenu();
    } else {
        OSDisableScheduler();
        __OSShutdownDevices(SD_EVENT_POWER_OFF);
        __OSShutdownToSBY();
    }
} // 0x80218EC0

/*
 * OSRestart
 *
 * Restarts the program with resetCode: through the channel launcher or by
 * rebooting the disc DOL, or with a hot reset if neither applies.
 */
void OSRestart(u32 resetCode) {
    u8 type = OSGetAppType();
    __OSStopPlayRecord();
    __OSUnRegisterStateEvent();

    if (type == APP_TYPE_CHANNEL) {
        OSDisableScheduler();
        __OSShutdownDevices(SD_EVENT_RESTART);
        OSEnableScheduler();
        __OSRelaunchTitle(resetCode);
    } else if (type == APP_TYPE_DISC) {
        OSDisableScheduler();
        __OSShutdownDevices(SD_EVENT_RESTART);
        OSEnableScheduler();
        __OSReboot(resetCode, bootThisDol);
    }

    OSDisableScheduler();
    __OSShutdownDevices(SD_EVENT_RESTART_FOR_ERROR);
    __OSHotResetForError();
} // 0x80218FE0

/*
 * __OSReturnToMenu
 *
 * Records menuMode in the state flags and launches the system menu. Falls back
 * to __OSReturnToMenuForError if ES cannot be set up or no memory is left.
 */
static void __OSReturnToMenu(u8 menuMode) {
    OSStateFlags state;
    ESTicketView* ticketView = NULL;
    int rc;

    __OSStopPlayRecord();
    __OSUnRegisterStateEvent();
    __DVDPrepareReset();
    __OSReadStateFlags(&state);

    state.lastDiscState = __OSGetDiscState(state.lastDiscState);
    state.lastShutdown = LAST_SHUTDOWN_RETURN_TO_MENU;
    state.menuMode = menuMode;

    __OSClearRTCFlags();
    __OSWriteStateFlags(&state);

    OSSetArenaLo(MENU_ARENA_LO);
    OSSetArenaHi(MENU_ARENA_HI);

    rc = ESP_InitLib();
    if (rc) {
        __OSReturnToMenuForError();
    }

    ticketView = OSAllocFromMEM1ArenaLo(0xE0, 0x20);
    if (ticketView == 0) {
        __OSReturnToMenuForError();
    }

    memset(ticketView, 0, 0xE0);

    rc = ESP_DiGetTicketView(NULL, ticketView);
    if (!rc && OSPlayTimeIsLimited()) {
        __OSPlayTimeType type = OSPLAYTIME_PERMANENT;
        u32 playTime = -1;
        __OSGetPlayTime(ticketView, &type, &playTime);
        if (playTime == 0) {
            __OSWriteExpiredFlagIfSet();
        }
    }

    OSDisableScheduler();
    __OSShutdownDevices(SD_EVENT_RETURN_TO_MENU);
    OSEnableScheduler();

    __OSLaunchMenu();
    OSDisableScheduler();

    __VISetRGBModeImm();
    __OSHotResetForError();
}

/*
 * OSReturnToMenu
 *
 * Quits the program and returns to the system menu.
 */
void OSReturnToMenu(void) {
    __OSReturnToMenu(MENU_MODE_SYSTEM_MENU);
    OSPanic(__FILE__, 895, "OSReturnToMenu(): Falied to boot system menu.\n");
} // 0x80219350

/*
 * OSReturnToDataManager
 *
 * Quits the program and returns to the system menu's data management screen.
 */
void OSReturnToDataManager(void) {
    __OSReturnToMenu(MENU_MODE_DATA_MANAGER);
    OSPanic(__FILE__, 913, "OSReturnToDataManager(): Falied to boot system menu.\n");
} // 0x80219390

/*
 * OSGetSettingPagePath
 *
 * Path of the settings manual page for page. The linker dropped this code and
 * its caller (the SDK has an OSReturnToSetting, named by the panic text) from
 * retail, but the page strings remain in .data; the caller's body is unknown.
 * The name is invented. The panic's line number is unknown (the code is
 * stripped): 985 is a placeholder that reaches no retail byte.
 */
const char* OSGetSettingPagePath(u32 page) {
    switch (page) {
    case 0:
        return "Calendar/Calendar_index.html";
    case 1:
        return "Display/Display_index.html";
    case 2:
        return "Sound/Sound_index.html";
    case 3:
        return "Parental_Control/Parental_Control_index.html";
    case 4:
        return "Internet/Internet_index.html";
    case 5:
        return "WiiConnect24/Wiiconnect24_index.html";
    case 6:
        return "Update/Update_index.html";
    default:
        OSPanic(__FILE__, 985, "OSReturnToSetting(): You can't specify %d.  \n", page);
        return NULL;
    }
}

/*
 * __OSReturnToMenuForError
 *
 * Last resort when the system menu cannot be started normally: records an
 * inserted disc and an ordinary return to the menu, launches the menu, and
 * resets the console if that fails.
 */
void __OSReturnToMenuForError(void) {
    OSStateFlags state;
    __OSReadStateFlags(&state);

    state.lastDiscState = DISC_STATE_INSERTED;
    state.lastShutdown = LAST_SHUTDOWN_RETURN_TO_MENU;

    __OSClearRTCFlags();
    __OSWriteStateFlags(&state);

    __OSLaunchMenu();
    OSDisableScheduler();

    __VISetRGBModeImm();
    __OSHotResetForError();

    OSPanic(__FILE__, 1010, "__OSReturnToMenu(): Falied to boot system menu.\n");
} // 0x802193D0

/*
 * __OSHotResetForError
 *
 * Hot-resets the console. Restores the STM interface first if the program was
 * started from a NAND channel or through a reboot.
 */
void __OSHotResetForError(void) {
    if (__OSInNandBoot || __OSInReboot) {
        __OSInitSTM();
    }

    __OSHotReset();

    OSPanic(__FILE__, 1034, "__OSHotReset(): Falied to reset system.\n");
} // 0x80219480

/*
 * OSGetResetCode
 *
 * Returns the reset code: the restart code with bit 31 set if the program was
 * started by a restart, otherwise the reset code the hardware latched.
 */
u32 OSGetResetCode(void) {
    u32 resetCode;
    if (__OSRebootParams.valid) {
        resetCode = OS_RESET_RESTART | __OSRebootParams.restartCode;
    } else {
        resetCode = (__PIRegs[9] & ~((1 << PI_RESET_CODE_SHIFT) - 1)) / (1 << PI_RESET_CODE_SHIFT);
    }

    return resetCode;
} // 0x802194E0

/*
 * OSResetSystem
 *
 * Obsolete: the reset parameters are ignored and the call always panics.
 */
void OSResetSystem(int reset, u32 resetCode, int forceMenu) {
    OSPanic(__FILE__, 1185, "OSResetSystem() is obsoleted. It doesn't work any longer.\n");
} // 0x80219510

/*
 * OSSetBootDol
 *
 * Obsolete: bootThisDol stays zero, so OSRestart always reboots the first DOL.
 * Dead in retail (the linker dropped it, its string remains). The panic's line
 * number is unknown: 1204 is a placeholder that reaches no retail byte.
 */
void OSSetBootDol(u32 dolOffset) {
    OSPanic(__FILE__, 1204, "OSSetBootDol() is obsoleted. It doesn't work any longer.\n");
}
