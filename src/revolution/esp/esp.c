// Reference: zeldaret/tp libs/revolution/src/esp/esp.c
#include <revolution/esp.h>
#include <revolution/ipc/ipcclt.h>

#include <stddef.h>

/* ES ioctlv commands (numbers as in the IOS ES interface; names invented: tp writes the literals). */
#define ES_IOCTL_LAUNCH_TITLE 8
#define ES_IOCTL_GET_NUM_TICKET_VIEWS 18
#define ES_IOCTL_GET_TICKET_VIEWS 19
#define ES_IOCTL_GET_CONSUMPTION 22
#define ES_IOCTL_DI_GET_TICKET_VIEW 27
#define ES_IOCTL_GET_DATA_DIR 29
#define ES_IOCTL_GET_TITLE_ID 32
#define ES_IOCTL_DI_GET_TMD_SIZE 57
#define ES_IOCTL_DI_GET_TMD 58

static s32 __esFd = -1;

/*
 * ESP_InitLib
 *
 * Opens /dev/es on first use.
 */
s32 ESP_InitLib(void) {
    s32 ret = 0;

    if (__esFd < 0) {
        __esFd = IOS_Open("/dev/es", 0);

        if (__esFd < 0) {
            ret = __esFd;
        }
    }

    return ret;
} // 0x80249790

/*
 * ESP_CloseLib
 *
 * Closes /dev/es and forgets the descriptor.
 */
s32 ESP_CloseLib(void) {
    s32 ret = 0;

    if (__esFd >= 0) {
        ret = IOS_Close(__esFd);
        if (ret == 0) {
            __esFd = -1;
        }
    }

    return ret;
} // 0x802497F0

/*
 * ESP_LaunchTitle
 *
 * Reboots into a title; the ticket view must be 32-byte aligned.
 */
s32 ESP_LaunchTitle(ESTitleId titleID, ESTicketView* pTicketView) {
    s32 ret = 0;
    u8 buf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* vec = (IOSIoVector*)(buf + 208);
    u64* id = (u64*)buf;

    if (__esFd < 0) {
        ret = ES_EINVAL;
        goto out;
    }

    // check if ptr is aligned to 0x20
    if ((u32)pTicketView & 31) {
        ret = ES_EINVAL;
        goto out;
    }

    *id = titleID;
    vec[0].base = (u8*)id;
    vec[0].length = 8;
    vec[1].base = (u8*)pTicketView;
    vec[1].length = 0xD8;

    ret = IOS_IoctlvReboot(__esFd, ES_IOCTL_LAUNCH_TITLE, 2, 0, vec);
    __esFd = -1;

out:
    return ret;
} // 0x80249840

/*
 * ESP_GetTicketViews
 *
 * Counts (list == NULL) or fetches the ticket views of a title.
 */
s32 ESP_GetTicketViews(ESTitleId titleId, ESTicketView* ticketViewList, u32* ticketViewCnt) {
    s32 rv = 0;
    u8 __esBuf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* v = (IOSIoVector*)(__esBuf + 256 - 6 * sizeof(IOSIoVector));
    ESTitleId* p1 = (ESTitleId*)__esBuf;
    u32* p2 = (u32*)(__esBuf + 32);

    if (__esFd < 0 || ticketViewCnt == NULL) {
        rv = ES_EINVAL;
        goto out;
    }

    if (((u32)ticketViewList & 31)) {
        rv = ES_EINVAL;
        goto out;
    }

    *p1 = titleId;
    if (ticketViewList == NULL) {
        v[0].base = (u8*)p1;
        v[0].length = sizeof(ESTitleId);
        v[1].base = (u8*)p2;
        v[1].length = 4;
        rv = IOS_Ioctlv(__esFd, ES_IOCTL_GET_NUM_TICKET_VIEWS, 1, 1, v);

        if (rv == 0) {
            *ticketViewCnt = *p2;
        }
    } else {
        if (*ticketViewCnt == 0) {
            rv = ES_EINVAL;
            goto out;
        }

        *p2 = *ticketViewCnt;
        v[0].base = (u8*)p1;
        v[0].length = sizeof(ESTitleId);
        v[1].base = (u8*)p2;
        v[1].length = 4;
        v[2].base = (u8*)ticketViewList;
        v[2].length = sizeof(ESTicketView) * (*ticketViewCnt);
        rv = IOS_Ioctlv(__esFd, ES_IOCTL_GET_TICKET_VIEWS, 2, 1, v);
    }

out:
    return rv;
} // 0x802498E0

/*
 * ESP_DiGetTicketView
 *
 * Builds a ticket view from a disc ticket (NULL ticket: the current one).
 */
s32 ESP_DiGetTicketView(const void* ticket, ESTicketView* ticketView) {
    s32 rv = 0;
    u8 __esBuf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* v = (IOSIoVector*)(__esBuf + 256 - 6 * sizeof(IOSIoVector));

    if (__esFd < 0 || ticketView == NULL) {
        rv = ES_EINVAL;
        goto out;
    }

    if (((u32)ticket & 31) || ((u32)ticketView & 31)) {
        rv = ES_EINVAL;
        goto out;
    }

    v[0].base = (u8*)ticket;

    if (ticket == NULL) {
        v[0].length = 0;
    } else {
        v[0].length = sizeof(ESTicket);
    }

    v[1].base = (u8*)ticketView;
    v[1].length = sizeof(ESTicketView);
    rv = IOS_Ioctlv(__esFd, ES_IOCTL_DI_GET_TICKET_VIEW, 1, 1, v);

out:
    return rv;
} // 0x80249A00

/*
 * ESP_DiGetTmd
 *
 * Gets the size (tmd == NULL) or the contents of the disc title metadata.
 */
s32 ESP_DiGetTmd(ESTitleMeta* tmd, u32* tmdSize) {
    s32 rv = 0;
    u8 __esBuf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* v = (IOSIoVector*)(__esBuf + 256 - 6 * sizeof(IOSIoVector));
    u32* p1 = (u32*)__esBuf;

    if (__esFd < 0 || tmdSize == NULL) {
        rv = ES_EINVAL;
        goto out;
    }

    if (((u32)tmd & 31)) {
        rv = ES_EINVAL;
        goto out;
    }

    if (tmd == NULL) {
        v[0].base = (u8*)p1;
        v[0].length = 4;
        rv = IOS_Ioctlv(__esFd, ES_IOCTL_DI_GET_TMD_SIZE, 0, 1, v);

        if (rv == 0) {
            *tmdSize = *p1;
        }
    } else {
        if (*tmdSize == 0) {
            rv = ES_EINVAL;
            goto out;
        }

        *p1 = *tmdSize;
        v[0].base = (u8*)p1;
        v[0].length = 4;
        v[1].base = (u8*)tmd;
        v[1].length = *tmdSize;
        rv = IOS_Ioctlv(__esFd, ES_IOCTL_DI_GET_TMD, 1, 1, v);
    }

out:
    return rv;
} // 0x80249AB0

/*
 * ESP_GetDataDir
 *
 * Gets the NAND data directory of a title.
 */
s32 ESP_GetDataDir(ESTitleId titleId, char* dataDir) {
    s32 rv = 0;
    u8 __esBuf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* v = (IOSIoVector*)(__esBuf + 256 - 6 * sizeof(IOSIoVector));
    ESTitleId* p1 = (ESTitleId*)__esBuf;

    if (__esFd < 0 || dataDir == NULL) {
        rv = ES_EINVAL;
        goto out;
    }

    if (((u32)dataDir & 31)) {
        rv = ES_EINVAL;
        goto out;
    }

    *p1 = titleId;
    v[0].base = (u8*)p1;
    v[0].length = sizeof(ESTitleId);
    v[1].base = (u8*)dataDir;
    v[1].length = 0x1E;
    rv = IOS_Ioctlv(__esFd, ES_IOCTL_GET_DATA_DIR, 1, 1, v);

out:
    return rv;
} // 0x80249BA0

/*
 * ESP_GetTitleId
 *
 * Gets the ID of the running title.
 */
s32 ESP_GetTitleId(ESTitleId* titleId) {
    s32 rv = 0;
    u8 __esBuf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* v = (IOSIoVector*)(__esBuf + 256 - 6 * sizeof(IOSIoVector));

    if (__esFd < 0 || titleId == NULL) {
        rv = ES_EINVAL;
        goto out;
    }

    v[0].base = __esBuf;
    v[0].length = sizeof(ESTitleId);

    rv = IOS_Ioctlv(__esFd, ES_IOCTL_GET_TITLE_ID, 0, 1, v);

    if (rv == 0) {
        *titleId = *(ESTitleId*)__esBuf;
    }

out:
    return rv;
} // 0x80249C40

/*
 * ESP_GetConsumption
 *
 * Gets the limited-play consumption entries of a ticket.
 */
s32 ESP_GetConsumption(ESTicketId ticketId, ESLpEntry* entries, u32* nEntries) {
    s32 rv = 0;
    u8 __esBuf[256] ATTRIBUTE_ALIGN(32);
    IOSIoVector* v = (IOSIoVector*)(__esBuf + 256 - 6 * sizeof(IOSIoVector));
    ESTicketId* p1 = (ESTicketId*)__esBuf;
    u32* p2 = (u32*)(__esBuf + 32);

    if (__esFd < 0) {
        rv = ES_EINVAL;
        goto out;
    }

    if (((u32)entries & 31)) {
        rv = ES_EINVAL;
        goto out;
    }

    *p1 = ticketId;
    v[0].base = (u8*)p1;
    v[0].length = sizeof(ESTicketId);

    if (entries == NULL) {
        v[1].base = NULL;
        v[1].length = 0;
    } else {
        v[1].base = (u8*)entries;
        v[1].length = sizeof(ESLpEntry) * (*nEntries);
        *p2 = *nEntries;
    }

    v[2].base = (u8*)p2;
    v[2].length = 4;
    rv = IOS_Ioctlv(__esFd, ES_IOCTL_GET_CONSUMPTION, 1, 2, v);
    *nEntries = *p2;
out:
    return rv;
} // 0x80249CE0
