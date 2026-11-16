// Reference: zeldaret/tp libs/revolution/src/os/OSLaunch.c
#include <revolution/esp.h>
#include <revolution/os.h>
#include <revolution/os/OSStateFlags.h>
#include <revolution/os/OSNandbootInfo.h>
#include <string.h>

/* Size of the buffer the nandboot record is written from; the record sits at its end. */
#define NANDBOOT_BUFFER_SIZE 0x2000

/* Return type of the nandboot record: come back to the title that launched us. */
#define NANDBOOT_RETURN_TO_LAST_TITLE 1

/* Error from ES when the disc game has no ticket view. */
#define ES_ERR_NO_TICKET_VIEW (-1017)

/* lastShutdown value for a relaunch. */
#define STATE_LAST_SHUTDOWN_RELAUNCH 3

/*
 * __OSRelaunchTitle
 *
 * Records the running title as the one to return to, then launches it again
 * with resetCode as its argument.
 */
void __OSRelaunchTitle(u32 resetCode) {
    s32 rc = 0;
    u32 ticketCnt = 1;
    ESTicketView* tik = NULL;
    ESTitleIdAligned titleId;
    u8* bi2 = NULL;
    OSNandbootInfo* info = NULL;
    OSStateFlags state;

    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812F0000);

    rc = ESP_InitLib();
    if (rc != 0) {
        __OSReturnToMenuForError();
    }

    rc = ESP_GetTitleId(&titleId);
    if (rc != 0) {
        __OSReturnToMenuForError();
    }

    tik = (ESTicketView*)OSAllocFromMEM1ArenaLo(OSRoundUp32B(sizeof(ESTicketView)), 32);
    if (tik == NULL) {
        __OSReturnToMenuForError();
    }

    memset(tik, 0, OSRoundUp32B(sizeof(ESTicketView)));
    rc = ESP_DiGetTicketView(NULL, tik);
    if (rc == ES_ERR_NO_TICKET_VIEW) {
        ESTicketView* tmp = NULL;
        rc = ESP_GetTicketViews(titleId, NULL, &ticketCnt);
        if (rc != 0) {
            __OSReturnToMenuForError();
        }

        tmp = (ESTicketView*)OSAllocFromMEM1ArenaLo(OSRoundUp32B(ticketCnt * sizeof(ESTicketView)), 32);
        if (tmp == NULL) {
            __OSReturnToMenuForError();
        }

        rc = ESP_GetTicketViews(titleId, tmp, &ticketCnt);
        if (rc != 0) {
            __OSReturnToMenuForError();
        }

        memcpy(tik, tmp, sizeof(ESTicketView));
    } else if (rc != 0) {
        __OSReturnToMenuForError();
    } else {
        if (OSPlayTimeIsLimited()) {
            __OSPlayTimeType type = OSPLAYTIME_PERMANENT;
            u32 remain = 0xFFFFFFFF;
            __OSGetPlayTime(tik, &type, &remain);
            if (remain == 0) {
                __OSWriteExpiredFlag();
                __OSReturnToMenuForError();
            }
        }
    }

    bi2 = (u8*)OSAllocFromMEM1ArenaLo(NANDBOOT_BUFFER_SIZE, 64);
    info = (OSNandbootInfo*)((u32)bi2 + (NANDBOOT_BUFFER_SIZE - sizeof(OSNandbootInfo)));
    memset(bi2, 0, NANDBOOT_BUFFER_SIZE);
    info->LastTitleId = titleId;
    info->LastAppType = OSGetAppType();
    info->ReturnType = NANDBOOT_RETURN_TO_LAST_TITLE;
    info->ArgValue = resetCode | 0x80000000;

    __OSCreateNandbootInfo();
    __OSWriteNandbootInfo(info);
    __OSReadStateFlags(&state);
    state.lastShutdown = STATE_LAST_SHUTDOWN_RELAUNCH;
    __OSWriteStateFlags(&state);
    rc = ESP_LaunchTitle(titleId, tik);

    if (rc != 0) {
        __OSReturnToMenuForError();
    }

    while (1) {
    }
} // 0x8021E6A0
