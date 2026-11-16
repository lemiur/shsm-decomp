// Reference: zeldaret/tp libs/revolution/src/os/OSPlayTime.c
#include <revolution/os.h>
#include <revolution/ai.h>
#include <revolution/esp.h>
#include <revolution/nand.h>
#include <revolution/vi.h>
#include <string.h>

/* Low-memory word holding the top of the arena; the reboot thread is carved below it. */
#define OS_ARENA_HI_ADDR 0x3128

/* Stack of the reboot thread. */
#define REBOOT_STACK_SIZE 0x1000

/* Play time reported for a ticket without a limit. */
#define PLAYTIME_UNLIMITED 0xFFFFFFFF

/* The ES error for "no ticket" (-1017). */
#define ES_ERR_NO_TICKET (-1017)

/* Bytes of the title id record written to /shared2/expired. */
#define TITLE_ID_BYTES 32

/* Seconds between the play time running out and the forced return to the menu. */
#define PLAYTIME_GRACE_SECONDS 20

ATTRIBUTE_ALIGN(32) OSAlarm __OSExpireAlarm;
OSTime __OSExpireTime;
OSPlayTimeCallbackFunc __OSExpireCallback;
BOOL __OSExpireSetExpiredFlag;

static void* __OSPlayTimeRebootThread(void* args);

/* State of the audio fade-out that runs while the console returns to the menu. */
typedef struct __OSExpireAIFadeStruct {
    s16 new_ai_buffer[2][48 * 2 * 3];
    s32 idx_ai_buffer;
    s16* src_buffer;
    u32 frames;
    s16 aLeft;
    s16 aRight;
    AIDCallback gameAIDCallback;
} __OSExpireAIFadeStruct;

static __OSExpireAIFadeStruct* __OSExpireAIFade = NULL;

/*
 * OSPlayTimeIsLimited
 *
 * TRUE when the ticket puts a limit on the play time.
 */
BOOL OSPlayTimeIsLimited(void) {
    return __OSExpireTime != 0;
} // 0x8021DEC0

/*
 * __OSPlayTimeRebootCallback
 *
 * Alarm handler: stops every thread and starts the reboot thread on a stack
 * carved from the top of the arena.
 */
static void __OSPlayTimeRebootCallback(OSAlarm* alarm, OSContext* context) {
    void* arenaHi;
    u32 memSize;
    OSThread* thread;
    void* stack;

    for (thread = __OSActiveThreadQueue.head; thread; thread = thread->linkActive.next) {
        OSSuspendThread(thread);
    }

    memSize = OSRoundUp32B(sizeof(OSThread)) + REBOOT_STACK_SIZE;
    arenaHi = *(void**)OSPhysicalToCached(OS_ARENA_HI_ADDR);
    arenaHi = (void*)((u32)arenaHi - memSize);

    thread = (OSThread*)arenaHi;
    arenaHi = (void*)((u32)arenaHi + OSRoundUp32B(sizeof(OSThread)));
    stack = arenaHi;

    if (!OSCreateThread(thread, __OSPlayTimeRebootThread, NULL, (void*)((u32)stack + REBOOT_STACK_SIZE), REBOOT_STACK_SIZE, 0, 0)) {
        __OSHotResetForError();
    }

    OSResumeThread(thread);
}

/*
 * __OSPlayTimeFadeLastAIDCallback
 *
 * Audio DMA callback that repeats the last stereo sample of the game audio
 * with a decaying volume, so the sound fades out rather than cutting off.
 */
void __OSPlayTimeFadeLastAIDCallback(void) {
    if (__OSExpireAIFade->gameAIDCallback) {
        __OSExpireAIFade->gameAIDCallback();
    }

    if (__OSExpireAIFade->frames == 0) {
        __OSExpireAIFade->src_buffer = (s16*)OSPhysicalToCached(AIGetDMAStartAddr());
    }

    if (__OSExpireAIFade->frames == 1) {
        DCInvalidateRange(__OSExpireAIFade->src_buffer, 4);
        __OSExpireAIFade->aLeft = (s16)(*__OSExpireAIFade->src_buffer++);
        __OSExpireAIFade->aRight = (s16)(*__OSExpireAIFade->src_buffer);
    }

    if (__OSExpireAIFade->frames >= 1) {
        s16* buffer = __OSExpireAIFade->new_ai_buffer[__OSExpireAIFade->idx_ai_buffer];
        s16* dest = buffer;
        u32 bytes = AIGetDMALength();
        u32 i = bytes;
        f32 delta = 0.995f;

        while (i) {
            *dest++ = __OSExpireAIFade->aLeft;
            *dest++ = __OSExpireAIFade->aRight;

            __OSExpireAIFade->aLeft *= delta;
            __OSExpireAIFade->aRight *= delta;

            i -= 4;
        }

        DCFlushRange(buffer, bytes);
        AIInitDMA((u32)buffer, bytes);
        __OSExpireAIFade->idx_ai_buffer++;
        __OSExpireAIFade->idx_ai_buffer &= 1;
    }

    __OSExpireAIFade->frames++;
} // 0x8021DEE0

/*
 * __OSWriteExpiredFlag
 *
 * Records the running title id in /shared2/expired so the system menu knows
 * the play time ran out. Returns TRUE on success.
 */
BOOL __OSWriteExpiredFlag(void) {
    s32 rv = 0;
    NANDFileInfo nInfo;
    BOOL openNInfo = FALSE;
    u8 titleId[TITLE_ID_BYTES] ATTRIBUTE_ALIGN(32);

    rv = NANDPrivateCreate("/shared2/expired", NAND_PERM_RWALL, 0);

    if (rv != NAND_RESULT_OK && rv != NAND_RESULT_EXISTS) {
        goto out;
    }

    rv = NANDPrivateOpen("/shared2/expired", &nInfo, 2);

    if (rv != NAND_RESULT_OK) {
        goto out;
    }

    openNInfo = TRUE;

    rv = ESP_InitLib();
    if (rv != 0) {
        goto out;
    }

    memset(titleId, 0, TITLE_ID_BYTES);
    rv = ESP_GetTitleId((u64*)titleId);

    if (rv != 0) {
        goto out;
    }

    rv = NANDWrite(&nInfo, titleId, TITLE_ID_BYTES);
    if (rv < 0) {
        goto out;
    } else if (rv != TITLE_ID_BYTES) {
        rv = NAND_RESULT_INVALID;
        goto out;
    } else {
        rv = 0;
    }

out:
    if (openNInfo) {
        NANDClose(&nInfo);
    }

    return rv == 0 ? TRUE : FALSE;
} // 0x8021E0A0

/*
 * __OSWriteExpiredFlagIfSet
 *
 * Writes the expired flag unless the application turned that off.
 */
BOOL __OSWriteExpiredFlagIfSet(void) {
    if (__OSExpireSetExpiredFlag) {
        return __OSWriteExpiredFlag();
    }

    return FALSE;
} // 0x8021E1C0

/*
 * __OSPlayTimeRebootThread
 *
 * Fades the screen and the sound out, writes the expired flag and returns to
 * the system menu.
 */
static void* __OSPlayTimeRebootThread(void* args) {
    BOOL enabled;
    u32 frames, fadeShift = 1;
    ATTRIBUTE_ALIGN(32) __OSExpireAIFadeStruct aiFade;

    __OSExpireAIFade = &aiFade;
    memset(__OSExpireAIFade, 0, sizeof(__OSExpireAIFadeStruct));
    __OSExpireAIFade->gameAIDCallback = AIRegisterDMACallback(__OSPlayTimeFadeLastAIDCallback);

    for (frames = 0; frames < 20; frames++) {
        fadeShift = (frames / 5) + 1;
        if (fadeShift > 7) {
            fadeShift = 7;
        }

        VIWaitForRetrace();
        __OSSetVIForceDimming(TRUE, fadeShift, fadeShift);
    }

    AIRegisterDMACallback(NULL);

    VISetBlack(TRUE);
    VIFlush();

    enabled = OSDisableInterrupts();

    __OSWriteExpiredFlagIfSet();
    OSRestoreInterrupts(enabled);
    OSReturnToMenu();
    return NULL;
} // 0x8021E1E0

/*
 * __OSPlayTimeAlarmExpired
 *
 * Alarm handler for the end of the play time: calls the application callback
 * if there is one, else starts the forced return to the menu.
 */
static void __OSPlayTimeAlarmExpired(OSAlarm* alarm, OSContext* context) {
    if (__OSExpireCallback != 0) {
        __OSExpireCallback();
    } else {
        __OSPlayTimeRebootCallback(alarm, context);
    }
} // 0x8021E2D0

/*
 * __OSPlayTimeGetConsumption
 *
 * Fetches the play time already used for the ticket (the entries only when
 * there are any).
 */
static s32 __OSPlayTimeGetConsumption(ESTicketView* tk, ESLpEntry* lpEntry, u32* nEntries) {
    s32 rv = ESP_GetConsumption(tk->ticketID, NULL, nEntries);

    if (rv > 0) {
        goto out;
    } else if (rv != 0) {
        ASSERTMSG1(FALSE, "Failed: %d\n", rv);
        goto out;
    }

    if (*nEntries != 0) {
        rv = ESP_GetConsumption(tk->ticketID, lpEntry, nEntries);
        if (rv != 0) {
            ASSERTMSG1(FALSE, "Failed: %d\n", rv);
            goto out;
        }
    }

out:
    return rv;
}

/*
 * __OSGetPlayTime
 *
 * Works out from the ticket limits and the recorded consumption what kind of
 * limit applies (*type) and how much play time is left (*playTime).
 */
s32 __OSGetPlayTime(ESTicketView* ticket, __OSPlayTimeType* type, u32* playTime) {
    s32 rv;
    u32 i;
    ESLpEntry lpEntry[ES_MAX_LIMITED_PLAY] ATTRIBUTE_ALIGN(32);
    u32 numCc = 0, seenOther = 0;
    ESTicketView ticketAligned ATTRIBUTE_ALIGN(32);

    if ((u32)ticket & 31) {
        memcpy(&ticketAligned, ticket, sizeof(ESTicketView));
        ticket = &ticketAligned;
    }

    rv = __OSPlayTimeGetConsumption(ticket, lpEntry, &numCc);
    if (rv != 0) {
        goto out;
    }

    for (i = 0; i < ES_MAX_LIMITED_PLAY; i++) {
        if (ticket->limits[i].code == ES_LIMIT_PLAYTIME) {
            *type = OSPLAYTIME_LIMITED;

            if (numCc == 0) {
                *playTime = ticket->limits[i].limit;
            } else {
                if (lpEntry[i].limit >= ticket->limits[i].limit) {
                    *playTime = 0;
                } else {
                    *playTime = ticket->limits[i].limit - lpEntry[i].limit;
                }
            }
            goto out;
        } else if (ticket->limits[i].code != ES_LIMIT_NONE) {
            seenOther = i + 1;
        }
    }

    if (!seenOther) {
        *type = OSPLAYTIME_PERMANENT;
        *playTime = PLAYTIME_UNLIMITED;
    } else {
        seenOther--;

        if (ticket->limits[seenOther].code == ES_LIMIT_LAUNCHES) {
            *type = OSPLAYTIME_LAUNCHES;
            *playTime = ticket->limits[seenOther].limit;

            if (numCc > 0) {
                *playTime -= lpEntry[seenOther].limit;
            }
        } else {
            *type = OSPLAYTIME_OTHER;
        }
    }

out:
    return rv;
} // 0x8021E370

/*
 * __OSGetPlayTimeCurrent
 *
 * __OSGetPlayTime for the ticket of the running title.
 */
s32 __OSGetPlayTimeCurrent(__OSPlayTimeType* type, u32* playTime) {
    s32 rv;
    ATTRIBUTE_ALIGN(32) ESTicketView ticket;

    rv = ESP_DiGetTicketView(NULL, &ticket);

    if (rv == ES_ERR_NO_TICKET) {
        goto out;
    }

    if (rv != 0) {
        ASSERTMSG1(FALSE, "Failed: %d\n", rv);
        goto out;
    }

    rv = __OSGetPlayTime(&ticket, type, playTime);

out:
    return rv;
}

/*
 * __OSInitPlayTime
 *
 * Reads the ticket play-time limit and, if there is one, arms the alarm that
 * ends the session 20 seconds after the limit.
 */
void __OSInitPlayTime(void) {
    s32 rv;
    u32 limit;
    __OSPlayTimeType type;

    __OSExpireTime = 0;
    __OSExpireCallback = NULL;
    __OSExpireSetExpiredFlag = TRUE;

    rv = ESP_InitLib();
    if (rv != 0) {
        ASSERTMSG1(FALSE, "Failed: %d\n", rv);
        goto out;
    }

    rv = __OSGetPlayTimeCurrent(&type, &limit);
    if (rv != 0) {
        if (rv != ES_ERR_NO_TICKET) {
            ASSERTMSG1(FALSE, "Failed: %d\n", rv);
        }

        goto out;
    }

    if (type == OSPLAYTIME_PERMANENT) {
        goto out;
    } else if (type == OSPLAYTIME_LIMITED) {
        if (limit == 0) {
            OSPanic(__FILE__, 737, "Expired");
        }

        OSCreateAlarm(&__OSExpireAlarm);
        OSSetAlarm(&__OSExpireAlarm, OSSecondsToTicks((OSTime)limit + PLAYTIME_GRACE_SECONDS), __OSPlayTimeAlarmExpired);
        __OSExpireTime = __OSExpireAlarm.fire;
        OSReport("PlayTime: %d seconds left\n", limit);
    }

out:
    ESP_CloseLib();
} // 0x8021E540
