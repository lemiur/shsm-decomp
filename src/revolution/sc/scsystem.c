// Reference: zeldaret/tp libs/revolution/src/sc/scsystem.c
//
// The retail SC library differs from the reference in a few places: the item table has 37 entries (no
// MPLS.MOVIE), the ASSERTLINE checks and the item-type debug strings of UnpackItem are not in the release
// build, DeleteItemByID and CreateItemByID address the item-offset table at conf + 6 (the reference writes
// conf + 4 in DeleteItemByID), and ParseConfBuf's reference-table size counts the largest item id (36), not
// the item count.
#include <revolution/nand.h>
#include <revolution/os.h>
#include <revolution/sc.h>
#include <revolution/sc/scsystem.h>
#include <string.h>

const char* __SCVersion = "<< RVL_SDK - SC \trelease build: Feb 27 2009 10:05:17 (0x4302_145) >>";

/* Low-memory buffer that receives the product info text (setting.txt); its last byte is kept zero (names invented). */
#define SC_PRODUCT_INFO_ADDR 0x3800
#define SC_PRODUCT_INFO_SIZE 0x100

typedef struct NameAndID {
    char* name;
    SCItemID id;
} NameAndID;

/* State of the background reload/flush job, shared by both state machines. */
typedef struct SCControl {
    OSThreadQueue threadQueue;
    NANDFileInfo nandFileInfo;
    NANDCommandBlock nandCommandBlock;
    union {
        u8 nandType;
        NANDStatus nandStatus;
    } u;
    u8 nandStep;
    u8 nandNeedClose;
    u8 reloadFileCount;
    SCReloadConfFileCallback reloadCallback;
    s32 reloadResult;
    const char* reloadFileName[2];
    u8* reloadBufp[2];
    u32 reloadSizeExpected[2];
    u32 reloadedSize[2];
    SCFlushCallback flushCallback;
    u32 flushResult;
    u32 flushSize;
} SCControl;

static NameAndID NameAndIDTbl[SC_ITEM_ID_MAX_PLUS1] = {
    {"IPL.CB", SC_ITEM_ID_IPL_COUNTER_BIAS},
    {"IPL.AR", SC_ITEM_ID_IPL_ASPECT_RATIO},
    {"IPL.ARN", SC_ITEM_ID_IPL_AUTORUN_MODE},
    {"IPL.CD", SC_ITEM_ID_IPL_CONFIG_DONE},
    {"IPL.CD2", SC_ITEM_ID_IPL_CONFIG_DONE2},
    {"IPL.DH", SC_ITEM_ID_IPL_DISPLAY_OFFSET_H},
    {"IPL.E60", SC_ITEM_ID_IPL_EURGB60_MODE},
    {"IPL.EULA", SC_ITEM_ID_IPL_EULA},
    {"IPL.FRC", SC_ITEM_ID_IPL_FREE_CHANNEL_APP_COUNT},
    {"IPL.IDL", SC_ITEM_ID_IPL_IDLE_MODE},
    {"IPL.INC", SC_ITEM_ID_IPL_INSTALLED_CHANNEL_APP_COUNT},
    {"IPL.LNG", SC_ITEM_ID_IPL_LANGUAGE},
    {"IPL.NIK", SC_ITEM_ID_IPL_OWNER_NICKNAME},
    {"IPL.PC", SC_ITEM_ID_IPL_PARENTAL_CONTROL},
    {"IPL.PGS", SC_ITEM_ID_IPL_PROGRESSIVE_MODE},
    {"IPL.SSV", SC_ITEM_ID_IPL_SCREEN_SAVER_MODE},
    {"IPL.SADR", SC_ITEM_ID_IPL_SIMPLE_ADDRESS},
    {"IPL.SND", SC_ITEM_ID_IPL_SOUND_MODE},
    {"IPL.UPT", SC_ITEM_ID_IPL_UPDATE_TYPE},
    {"NET.CNF", SC_ITEM_ID_NET_CONFIG},
    {"NET.CTPC", SC_ITEM_ID_NET_CONTENT_RESTRICTIONS},
    {"NET.PROF", SC_ITEM_ID_NET_PROFILE},
    {"NET.WCPC", SC_ITEM_ID_NET_WC_RESTRICTION},
    {"NET.WCFG", SC_ITEM_ID_NET_WC_FLAGS},
    {"DEV.BTM", SC_ITEM_ID_DEV_BOOT_MODE},
    {"DEV.VIM", SC_ITEM_ID_DEV_VIDEO_MODE},
    {"DEV.CTC", SC_ITEM_ID_DEV_COUNTRY_CODE},
    {"DEV.DSM", SC_ITEM_ID_DEV_DRIVESAVING_MODE},
    {"BT.DINF", SC_ITEM_ID_BT_DEVICE_INFO},
    {"BT.CDIF", SC_ITEM_ID_BT_CMPDEV_INFO},
    {"BT.SENS", SC_ITEM_ID_BT_DPD_SENSIBILITY},
    {"BT.SPKV", SC_ITEM_ID_BT_SPEAKER_VOLUME},
    {"BT.MOT", SC_ITEM_ID_BT_MOTOR_MODE},
    {"BT.BAR", SC_ITEM_ID_BT_SENSOR_BAR_POSITION},
    {"DVD.CNF", SC_ITEM_ID_DVD_CONFIG},
    {"WWW.RST", SC_ITEM_ID_WWW_RESTRICTION},
    {"IPL.TID", SC_ITEM_ID_TEMPORARY_TITLE_ID},
};

static const char ConfDirName[] = "/shared2/sys";
static const char ConfFileName[] = "/shared2/sys/SYSCONF";
static const char ProductInfoFileName[] = "/title/00000001/00000002/data/setting.txt";

static u8 ConfBuf[0x4000] ATTRIBUTE_ALIGN(32);
static u8 ConfBufForFlush[0x4000] ATTRIBUTE_ALIGN(32);

static u8 Initialized;
static u8 DirtyFlag;
static u8 IsDevKit;
static vu8 BgJobStatus = 0; // volatile: SDK declaration (zeldaret/tp scsystem.c)
static u32 ItemIDOffsetTblOffset;
static u32 ItemIDMaxPlus1;
static u32 ItemNumTotal;
static u32 ItemRestSize;
static SCControl Control;

static void SetBgJobStatus(u32 status);
static u32 ParseConfBuf(u8* bufp, u32 bufSize);
static void ClearConfBuf(u8* bufp);
static BOOL ParseGetBEValue(const u8* bufp, const u8* bufEndp, u32* varp, u32 varSize);
static void OpenCallbackFromReload(s32 result, NANDCommandBlock* block);
static void ReadCallbackFromReload(s32 result, NANDCommandBlock* block);
static void CloseCallbackFromReloadError(s32 result, NANDCommandBlock* block);
static void CloseCallbackFromReload(s32 result, NANDCommandBlock* block);
static void ErrorFromReload(s32 result);
static void FinishFromReload(void);
static BOOL UnpackItem(const u8* bufp, SCItem* itemp);
static void MyNandCallback(s32 result, NANDCommandBlock* block);
static void FinishFromFlush(void);
static void ErrorFromFlush(void);

static BOOL __SCIsDirty(void);
static void __SCSetDirtyFlag(void);
static void __SCClearDirtyFlag(void);
static u8* __SCGetConfBuf(void);
static u32 __SCGetConfBufSize(void);

/*
 * SCInit
 *
 * Registers the library version, opens the NAND library and starts the first reload of the
 * configuration file; the job status stays at 1 (busy) until SCCheckStatus reports the result.
 */
void SCInit(void) {
    BOOL enabled = OSDisableInterrupts();

    if (Initialized) {
        OSRestoreInterrupts(enabled);
        return;
    }

    Initialized = TRUE;
    SetBgJobStatus(1);
    OSRestoreInterrupts(enabled);

    OSRegisterVersion(__SCVersion);
    OSInitThreadQueue(&Control.threadQueue);

    if (OSGetConsoleType() & 0x10000000) {
        IsDevKit = TRUE;
    }

    if (NANDInit() != 0 || SCReloadConfFileAsync(__SCGetConfBuf(), __SCGetConfBufSize(), NULL) != 0) {
        SetBgJobStatus(2);
    }
} // 0x80247450

/*
 * SetBgJobStatus
 *
 * Stores the background job status read by SCCheckStatus (0 idle, 1 busy, 2 error, 3 reload finished).
 */
static void SetBgJobStatus(u32 status) {
    BgJobStatus = (u8)status;
}

/*
 * SCCheckStatus
 *
 * Returns the background job status (0 idle, 1 busy, 2 error). A finished reload (3) is parsed here:
 * a good file becomes the live configuration, a bad one is replaced by an empty configuration.
 */
u32 SCCheckStatus(void) {
    BOOL enabled;
    u32 ret;

    enabled = OSDisableInterrupts();
    ret = BgJobStatus;

    if (ret == 3) {
        SetBgJobStatus(1);
        OSRestoreInterrupts(enabled);

        if (ParseConfBuf(Control.reloadBufp[0], Control.reloadedSize[0]) == 0) {
            enabled = OSDisableInterrupts();

            if (__SCGetConfBuf() != Control.reloadBufp[0]) {
                memcpy(__SCGetConfBuf(), Control.reloadBufp[0], __SCGetConfBufSize());
            }

            __SCClearDirtyFlag();
            OSRestoreInterrupts(enabled);
        } else {
            enabled = OSDisableInterrupts();
            ClearConfBuf(Control.reloadBufp[0]);
            __SCClearDirtyFlag();
            OSRestoreInterrupts(enabled);
        }

        ret = 0;
        SetBgJobStatus(ret);
    } else {
        OSRestoreInterrupts(enabled);
    }

    return ret;
} // 0x802474F0

/*
 * SCReloadConfFileAsync
 *
 * Starts the chain of NAND callbacks that reads SYSCONF into bufp and the product info file into
 * the low-memory buffer at 0x3800.
 */
s32 SCReloadConfFileAsync(u8* bufp, u32 bufSize, SCReloadConfFileCallback callback) {
    u32 i;

    if (bufSize < __SCGetConfBufSize()) {
        return -128;
    }

    SetBgJobStatus(1);
    Control.reloadCallback = callback;
    Control.reloadResult = 0;
    Control.reloadFileCount = 0;

    for (i = 0; i < 2; i++) {
        Control.reloadedSize[i] = 0;
    }

    Control.reloadFileName[0] = ConfFileName;
    Control.reloadFileName[1] = ProductInfoFileName;
    Control.reloadBufp[0] = bufp;
    Control.reloadBufp[1] = (u8*)OSPhysicalToCached(SC_PRODUCT_INFO_ADDR);
    Control.reloadSizeExpected[0] = __SCGetConfBufSize();
    Control.reloadSizeExpected[1] = SC_PRODUCT_INFO_SIZE;
    ClearConfBuf(bufp);
    ItemIDOffsetTblOffset = 0;
    ItemNumTotal = 0;
    ItemRestSize = 0;
    Control.nandNeedClose = FALSE;
    return NANDPrivateOpenAsync(Control.reloadFileName[Control.reloadFileCount],
                                &Control.nandFileInfo, NAND_ACCESS_READ, OpenCallbackFromReload,
                                &Control.nandCommandBlock);
} // 0x80247610

/*
 * OpenCallbackFromReload
 *
 * Reload step 1: the file is open, start reading it.
 */
static void OpenCallbackFromReload(s32 result, NANDCommandBlock* block) {
    if (result == 0) {
        Control.nandNeedClose = TRUE;

        if (NANDReadAsync(&Control.nandFileInfo, Control.reloadBufp[Control.reloadFileCount],
                          Control.reloadSizeExpected[Control.reloadFileCount],
                          ReadCallbackFromReload, &Control.nandCommandBlock) == 0)
        {
            return;
        }
    }

    ErrorFromReload(result);
} // 0x80247730

/*
 * ReadCallbackFromReload
 *
 * Reload step 2: a complete read closes the file; a short read is an error.
 */
static void ReadCallbackFromReload(s32 result, NANDCommandBlock* block) {
    if (result == Control.reloadSizeExpected[Control.reloadFileCount]) {
        Control.reloadedSize[Control.reloadFileCount] = (u32)result;
        Control.nandNeedClose = FALSE;

        if (NANDCloseAsync(&Control.nandFileInfo, CloseCallbackFromReload,
                           &Control.nandCommandBlock) == 0)
        {
            return;
        }
    }

    ErrorFromReload((s32)((result == 0) ? -128 : result));
} // 0x80247800

/*
 * CloseCallbackFromReload
 *
 * Reload step 3: the file is closed, go on to the next one.
 */
static void CloseCallbackFromReload(s32 result, NANDCommandBlock* block) {
    if (result == 0) {
        FinishFromReload();
        return;
    }

    ErrorFromReload(result);
} // 0x802478F0

/*
 * FinishFromReload
 *
 * Moves on to the next file; after the last one it publishes the status, empties the buffer when
 * the configuration file was missing and terminates the product info text.
 */
static void FinishFromReload(void) {
    u32 status;

nextFile:
    Control.reloadFileCount++;

    if (Control.reloadFileCount < 2) {
        Control.nandNeedClose = FALSE;

        if (NANDPrivateOpenAsync(Control.reloadFileName[Control.reloadFileCount],
                                 &Control.nandFileInfo, NAND_ACCESS_READ, OpenCallbackFromReload,
                                 &Control.nandCommandBlock) == 0)
        {
            return;
        }

        goto nextFile;
    }

    switch (Control.reloadResult) {
    case 0:
        status = 3;
        break;
    default:
    case -12:
        ClearConfBuf(Control.reloadBufp[0]);
        Control.reloadedSize[0] = Control.reloadSizeExpected[0];
        status = 3;
        break;
    }

    ((char*)OSPhysicalToCached(SC_PRODUCT_INFO_ADDR))[SC_PRODUCT_INFO_SIZE - 1] = '\0';

    if (Control.reloadCallback) {
        Control.reloadCallback(Control.reloadResult);
        Control.reloadCallback = NULL;
    }

    SetBgJobStatus(status);
} // 0x80247980

/*
 * ErrorFromReload
 *
 * Records the first file's error, closes the file if it is still open and finishes the reload.
 */
static void ErrorFromReload(s32 result) {
    if (Control.reloadFileCount == 0) {
        Control.reloadResult = result;
    }

    Control.reloadedSize[Control.reloadFileCount] = 0;

    if (Control.nandNeedClose) {
        if (NANDCloseAsync(&Control.nandFileInfo, CloseCallbackFromReloadError,
                           &Control.nandCommandBlock) == 0)
        {
            return;
        }
    }

    FinishFromReload();
}

static void CloseCallbackFromReloadError(s32 result, NANDCommandBlock* block) {
    FinishFromReload();
} // 0x80247AB0

/*
 * ClearConfBuf
 *
 * Writes an empty configuration file image: header "SCv0", zero items, trailer "SCed".
 */
static void ClearConfBuf(u8* bufp) {
    u32 size = __SCGetConfBufSize();
    memset(bufp, 0, size);

    if (size > 12) {
        memcpy(bufp, "SCv0", 4);
        memcpy(bufp + (size - 4), "SCed", 4);
        *((u16*)bufp + 3) = 8;
    }
}

/*
 * ParseConfBuf
 *
 * Checks the file image (header, item offsets, trailer), then builds the runtime table at the end of the
 * buffer that maps an item id to the slot of its offset. Returns 0 on success, 2 for a damaged file.
 */
static u32 ParseConfBuf(u8* bufp, u32 bufSize) {
    u8 *bufTop, *bufEndp;
    u32 numItems, loopItem;
    SCItem item;
    u32 itemOffset;
    u16* itemOfsp;
    u16* runtimeRefp;
    u32 restSize;
    NameAndID* tblp = NameAndIDTbl;
    NameAndID* tblEndp;
    char* name;

    if ((bufSize < 12) || (bufSize > __SCGetConfBufSize())) {
        goto error;
    }

    bufTop = bufp;
    bufEndp = bufp + bufSize - 4;
    ItemIDMaxPlus1 = SC_ITEM_ID_MAX_PLUS1;

    if (memcmp(bufp, "SCv0", 4) || memcmp(bufEndp, "SCed", 4)) {
        goto error;
    }

    bufp += 4;

    if (bufSize < __SCGetConfBufSize()) {
        u32 expandSize = __SCGetConfBufSize() - bufSize;
        memset(bufEndp, 0, expandSize);
        bufEndp += expandSize;
        memcpy(bufEndp, "SCed", 4);
    }

    if (ParseGetBEValue(bufp, bufEndp, &numItems, 2) == FALSE) {
        goto error;
    }

    bufp += 2;
    itemOfsp = (u16*)bufp;
    itemOffset = ((u8*)(itemOfsp + numItems + 1) - bufTop);

    for (loopItem = 0; loopItem < numItems; loopItem++) {
        if (itemOffset > bufSize || ((u8*)&itemOfsp[loopItem] - bufTop) > bufSize) {
            goto error;
        }

        if (itemOffset != itemOfsp[loopItem] || UnpackItem(bufTop + itemOffset, &item) == FALSE) {
            goto error;
        }

        itemOffset += item.packedSize;
    }

    if (itemOffset > bufSize) {
        goto error;
    }

    if (itemOffset != itemOfsp[loopItem]) {
        goto error;
    }

    // The reference table is sized for ids 1..36 (one slot short of the id range 0..36); the original's count.
    runtimeRefp = (u16*)(bufEndp - 2 * (SC_ITEM_ID_MAX_PLUS1 - 1));

    if ((u8*)(bufTop + itemOffset) > (u8*)runtimeRefp) {
        goto error;
    }

    restSize = (u32)((u8*)runtimeRefp - (u8*)(bufTop + itemOffset));
    memset(runtimeRefp, 0, (u32)(bufEndp - (u8*)runtimeRefp));
    runtimeRefp = (u16*)(bufEndp - 2);
    tblEndp = tblp + ItemIDMaxPlus1;

    while (tblp < tblEndp && (name = tblp->name) != NULL) {
        u32 nameLen;
        u8* p;

        nameLen = strlen(name);

        for (loopItem = 0; loopItem < numItems; loopItem++) {
            p = bufTop + itemOfsp[loopItem];

            if (nameLen == (u32)(((*p) & 0x1F) + 1) &&
                memcmp(name, p + sizeof(SCType), nameLen) == 0)
            {
                runtimeRefp[-tblp->id] = (u16)((u8*)(&itemOfsp[loopItem]) - bufTop);
                break;
            }
        }

        tblp++;
    }

    ItemIDOffsetTblOffset = (u32)((u8*)runtimeRefp - bufTop);
    ItemNumTotal = numItems;
    ItemRestSize = restSize;
    return 0;

error:
    return 2;
} // 0x80247AC0

/*
 * ParseGetBEValue
 *
 * Reads a big-endian integer of varSize bytes; FALSE when it would run past the buffer.
 */
static BOOL ParseGetBEValue(const u8* bufp, const u8* bufEndp, u32* varp, u32 varSize) {
    u32 value = 0;

    if ((bufp + varSize) > bufEndp) {
        return FALSE;
    }

    while (varSize) {
        value = (value << 8) | *bufp;
        bufp++;
        varSize--;
    }

    *varp = value;
    return TRUE;
}

/*
 * UnpackItem
 *
 * Decodes the item at bufp (type/name-length byte, name, size prefix for arrays, data) into itemp.
 * Returns FALSE for an unknown type.
 */
static BOOL UnpackItem(const u8* bufp, SCItem* itemp) {
    SCType type;

    memset(itemp, 0, sizeof(SCItem));
    type = (u8)(*bufp & 0xE0);
    itemp->name = (char*)(bufp + sizeof(SCType));
    itemp->nameLen = (u32)((*bufp & 0x1F) + 1);
    itemp->data = (u8*)(bufp + sizeof(SCType) + itemp->nameLen);

    switch (type) {
    case 0x60:
    case 0xE0:
        itemp->dataSize = sizeof(u8);
        break;
    case 0x80:
        itemp->dataSize = sizeof(u16);
        break;
    case 0xA0:
        itemp->dataSize = sizeof(u32);
        break;
    case 0xC0:
        itemp->dataSize = sizeof(u64);
        break;
    case 0x40:
        itemp->dataSize = (u32)(*(itemp->data) + 1);
        itemp->data++;
        itemp->packedSize++;
        break;
    case 0x20:
        itemp->dataSize = (u32)((((*(itemp->data)) << 8) | (*(itemp->data + 1))) + 1);
        itemp->data += 2;
        itemp->packedSize += 2;
        break;
    default:
        goto err;
    }

    if (type == 0x40 || type == 0x20) {
        itemp->typeByteArray = 0x40;
    } else {
        itemp->typeInteger = type;
        memcpy(&itemp->integer, itemp->data, itemp->dataSize);
    }

    itemp->packedSize += sizeof(SCType) + itemp->nameLen + itemp->dataSize;

err:
    return (itemp->dataSize != 0);
} // 0x80247D10

/*
 * FindItemByID
 *
 * Unpacks the item of the given id through the runtime reference table.
 */
static BOOL FindItemByID(SCItemID id, SCItem* itemp) {
    u8* conf = __SCGetConfBuf();
    u16* refp;

    if (id < ItemIDMaxPlus1 && ItemIDOffsetTblOffset != 0) {
        refp = (u16*)(conf + ItemIDOffsetTblOffset);
        if (refp[-id] != 0) {
            return UnpackItem(conf + *(u16*)(conf + refp[-id]), itemp);
        }
    }

    return FALSE;
}

/*
 * DeleteItemByID
 *
 * Removes an item: closes the gap in the offset table and in the item data, fixes the offsets and
 * the runtime reference table, and gives the space back to the free pool.
 */
void DeleteItemByID(SCItemID id) {
    u8* conf = __SCGetConfBuf();
    u32 targetRef, initialTopOfFreeSpace, moveSize, shrinkSize, i;
    u16 *refp, *itemOfsTop, *itemOfsTargetp, *itemOfsEndp, *itemOfsp;

    if (id < ItemIDMaxPlus1 && ItemIDOffsetTblOffset != 0) {
        refp = (u16*)(conf + ItemIDOffsetTblOffset);
        targetRef = refp[-id];

        if (targetRef != 0 && ItemNumTotal != 0) {
            itemOfsTop = (u16*)(conf + 6);
            itemOfsTargetp = (u16*)(conf + targetRef);
            itemOfsEndp = itemOfsTop + ItemNumTotal;
            initialTopOfFreeSpace = *itemOfsEndp;
            shrinkSize = 2 + (itemOfsTargetp[1] - itemOfsTargetp[0]);

            moveSize = *itemOfsTargetp - (targetRef + 2);
            memmove(conf + targetRef, conf + targetRef + 2, moveSize);

            for (itemOfsp = itemOfsEndp - 1; itemOfsp >= itemOfsTop; itemOfsp--) {
                if (itemOfsp < itemOfsTargetp) {
                    *itemOfsp -= 2;
                } else {
                    *itemOfsp -= shrinkSize;
                }
            }

            memmove(conf + itemOfsTargetp[0], conf + (itemOfsTargetp[0] + shrinkSize),
                    initialTopOfFreeSpace - (itemOfsTargetp[0] + shrinkSize));
            memset(conf + (initialTopOfFreeSpace - shrinkSize), 0, shrinkSize);

            for (i = 0; i < ItemIDMaxPlus1; i++) {
                if (refp[-i] >= targetRef) {
                    if (refp[-i] > targetRef) {
                        refp[-i] -= 2;
                    } else {
                        refp[-i] = 0;
                    }
                }
            }
            ItemRestSize += shrinkSize;
            ItemNumTotal--;
            *(u16*)(conf + 4) = (u16)ItemNumTotal;
            __SCSetDirtyFlag();
        }
    }
} // 0x80247E90

/*
 * CreateItemByID
 *
 * Appends an item with the table name of id: opens a slot in the offset table, writes the type/name
 * byte, the name, the length prefix for arrays and the data. Returns FALSE when it does not fit.
 */
BOOL CreateItemByID(SCItemID id, SCType type, const u8* data, u32 size) {
    u8* conf = __SCGetConfBuf();
    u8* p;
    u32 nameLen;
    u32 packedSize = sizeof(SCType);
    NameAndID* tblp = NameAndIDTbl;
    char* name;
    u32 topOfFreeSpace;
    u16* refp;
    u16* itemOfsTop;
    u16* itemOfsEndp;
    u16* itemOfsp;

    if (id < ItemIDMaxPlus1 && data != NULL && ItemNumTotal < 0xFFFF && ItemIDOffsetTblOffset != 0) {
        switch (type) {
        case 0xE0:
        case 0x60:
            size = 1;
            break;
        case 0x80:
            size = 2;
            break;
        case 0xA0:
            size = 4;
            break;
        case 0xC0:
            size = 8;
            break;
        case 0x40:
            if (size == 0 || size > 65536) {
                goto error;
            }
            if (size > 256) {
                packedSize += 2;
                type = 0x20;
            } else {
                packedSize += 1;
            }
            break;

        default:
            goto error;
        }

        packedSize += size;

        while ((name = tblp->name) != NULL) {
            if (tblp->id == id) {
                break;
            }

            tblp++;
        }

        if (name == NULL) {
            goto error;
        }

        nameLen = strlen(name);
        if (nameLen > 32) {
            goto error;
        }

        packedSize += nameLen;
        if (ItemRestSize < (2 + packedSize)) {
            goto error;
        }

        itemOfsTop = (u16*)(conf + 6);
        itemOfsEndp = itemOfsTop + ItemNumTotal;
        topOfFreeSpace = *itemOfsEndp;
        memmove(conf + (itemOfsTop[0] + 2), conf + itemOfsTop[0], topOfFreeSpace - itemOfsTop[0]);

        itemOfsp = itemOfsTop;
        do {
            *itemOfsp += 2;
            itemOfsp++;
        } while (itemOfsp <= itemOfsEndp);

        topOfFreeSpace = *itemOfsEndp;
        p = (u8*)(conf + topOfFreeSpace);

        *p = (SCType)(type | (nameLen - 1));
        memcpy(p + sizeof(SCType), name, nameLen);
        p += sizeof(SCType) + nameLen;
        if (type == 0x40) {
            *p++ = (u8)(size - 1);
        } else if (type == 0x20) {
            *p++ = (u8)((size - 1) >> 8);
            *p++ = (u8)(size - 1);
        }
        memcpy(p, data, size);
        p += size;

        refp = (u16*)(conf + ItemIDOffsetTblOffset);
        refp[-id] = (u16)((u8*)itemOfsEndp - conf);
        itemOfsEndp[1] = (u16)(itemOfsEndp[0] + packedSize);
        ItemRestSize -= 2 + packedSize;
        ItemNumTotal++;
        *(u16*)(conf + 4) = (u16)ItemNumTotal;
        __SCSetDirtyFlag();

        return TRUE;
    }

error:
    return FALSE;
} // 0x80248030

/*
 * SCFindByteArrayItem
 *
 * Copies a byte-array item of exactly `size` bytes to data.
 */
BOOL SCFindByteArrayItem(void* data, u32 size, SCItemID id) {
    SCItem item;
    BOOL result = FALSE;
    BOOL enabled = OSDisableInterrupts();

    if (data != NULL && FindItemByID(id, &item) && item.typeByteArray != 0 && item.dataSize == size) {
        memcpy(data, item.data, size);
        result = TRUE;
    }

    OSRestoreInterrupts(enabled);
    return result;
} // 0x802482B0

/*
 * SCReplaceByteArrayItem
 *
 * Overwrites a byte-array item of the same size in place, or deletes a mismatching one and creates it again.
 */
BOOL SCReplaceByteArrayItem(const void* data, u32 size, SCItemID id) {
    SCItem item;
    BOOL result = FALSE;
    BOOL enabled = OSDisableInterrupts();

    if (data != NULL) {
        if (FindItemByID(id, &item)) {
            if (item.typeByteArray != 0 && item.dataSize == size) {
                if (memcmp(item.data, data, size) != 0) {
                    memcpy(item.data, data, size);
                    __SCSetDirtyFlag();
                }

                result = TRUE;
                goto finish;
            } else {
                DeleteItemByID(id);
            }
        }
        result = CreateItemByID(id, 0x40, data, size);
    }

finish:
    OSRestoreInterrupts(enabled);
    return result;
} // 0x80248390

/*
 * SCFindIntegerItem
 *
 * Copies an integer item of the given type to data.
 */
static BOOL SCFindIntegerItem(void* data, SCItemID id, SCType type) {
    SCItem item;
    BOOL result = FALSE;
    BOOL enabled = OSDisableInterrupts();

    if (FindItemByID(id, &item) && item.typeInteger == type) {
        memcpy(data, item.data, item.dataSize);
        result = TRUE;
    }

    OSRestoreInterrupts(enabled);
    return result;
}

/*
 * SCReplaceIntegerItem
 *
 * Overwrites an integer item of the given type, or deletes a mismatching one and creates it again.
 */
static BOOL SCReplaceIntegerItem(const void* data, SCItemID id, SCType type) {
    SCItem item;
    BOOL result = FALSE;
    BOOL enabled = OSDisableInterrupts();

    if (FindItemByID(id, &item)) {
        if (item.typeInteger == type) {
            if (memcmp(item.data, data, item.dataSize) != 0) {
                memcpy(item.data, data, item.dataSize);
                __SCSetDirtyFlag();
            }

            result = TRUE;
            goto finish;
        } else {
            DeleteItemByID(id);
        }
    }

    result = CreateItemByID(id, type, data, 0);

finish:
    OSRestoreInterrupts(enabled);
    return result;
}

/*
 * SCFindU8Item
 *
 * Reads a one-byte integer item.
 */
BOOL SCFindU8Item(u8* data, SCItemID id) {
    return SCFindIntegerItem(data, id, 0x60);
} // 0x802484C0

/*
 * SCFindS8Item
 *
 * Reads a one-byte integer item (stored with the same type as the unsigned one).
 */
BOOL SCFindS8Item(s8* data, SCItemID id) {
    return SCFindIntegerItem(data, id, 0x60);
} // 0x802485A0

/*
 * SCFindU32Item
 *
 * Reads a four-byte integer item.
 */
BOOL SCFindU32Item(u32* data, SCItemID id) {
    return SCFindIntegerItem(data, id, 0xA0);
} // 0x80248680

/*
 * SCReplaceU8Item
 *
 * Writes a one-byte integer item.
 */
BOOL SCReplaceU8Item(u8 data, SCItemID id) {
    return SCReplaceIntegerItem(&data, id, 0x60);
} // 0x80248760

/*
 * __SCFlushSyncCallback
 *
 * Completion callback of a flush started without one: wakes the threads waiting on the control block.
 */
static void __SCFlushSyncCallback(u32 status) {
    OSWakeupThread(&Control.threadQueue);
} // 0x80248870

/*
 * SCFlushAsync
 *
 * Writes the configuration back to SYSCONF when it is dirty (state machine in MyNandCallback). With no
 * callback the completion wakes the threads waiting on the control block.
 */
void SCFlushAsync(SCFlushCallback callback) {
    SCControl* ctrl;
    BOOL enabled;
    u8 status;

    ctrl = &Control;
    enabled = OSDisableInterrupts();
    status = BgJobStatus;

    if (status == 0) {
        SetBgJobStatus(1);

        if (callback == NULL) {
            callback = __SCFlushSyncCallback;
        }

        ctrl->flushCallback = callback;
        ctrl->flushResult = 0;
        ctrl->nandNeedClose = FALSE;
        ctrl->flushSize = __SCGetConfBufSize();

        if (!__SCIsDirty()) {
            OSRestoreInterrupts(enabled);
            FinishFromFlush();
        } else {
            __SCClearDirtyFlag();
            memcpy(ConfBufForFlush, __SCGetConfBuf(), __SCGetConfBufSize());

            OSRestoreInterrupts(enabled);
            ctrl->nandStep = 0;

            if (NANDPrivateGetTypeAsync(ConfFileName, &ctrl->u.nandType, MyNandCallback,
                                        &ctrl->nandCommandBlock) != NAND_RESULT_OK)
            {
                ErrorFromFlush();
            }
        }
    } else {
        if (callback) {
            callback(status == 1 ? status : 2);
        }

        OSRestoreInterrupts(enabled);
    }
} // 0x80248880

/*
 * MyNandCallback
 *
 * Flush state machine: 0 type of SYSCONF, 1 its permissions, 2 delete, 3 type of the directory,
 * 4 create directory, 5 create file, 6 open, 7 write, 8 close, 9 close after an error.
 */
static void MyNandCallback(s32 result, NANDCommandBlock* block) {
    SCControl* ctrl = &Control;

    switch (ctrl->nandStep) {
    case 0:
        if (result == NAND_RESULT_OK && ctrl->u.nandType == 1) {
            ctrl->nandStep = 1;

            if (NANDPrivateGetStatusAsync(ConfFileName, &ctrl->u.nandStatus, MyNandCallback,
                                          &ctrl->nandCommandBlock) != NAND_RESULT_OK)
            {
                break;
            }
        } else {
            goto _case_1_lbl;
        }
        return;
    case 1:
        if (result == NAND_RESULT_OK && ctrl->u.nandStatus.permission == NAND_PERM_RWALL) {
            goto _case_5_lbl;
        }
    _case_1_lbl:
        ctrl->nandStep = 2;

        if (NANDPrivateDeleteAsync(ConfFileName, MyNandCallback, &ctrl->nandCommandBlock) != NAND_RESULT_OK) {
            break;
        }
        return;
    case 2:
        ctrl->nandStep = 3;

        if (NANDPrivateGetTypeAsync(ConfDirName, &ctrl->u.nandType, MyNandCallback,
                                    &ctrl->nandCommandBlock) != NAND_RESULT_OK)
        {
            break;
        }
        return;
    case 3:
        if (result == NAND_RESULT_OK && ctrl->u.nandType == 2) {
            goto _case_4_lbl;
        }

        ctrl->nandStep = 4;

        if (NANDPrivateCreateDirAsync(ConfDirName, NAND_PERM_RWALL, 0, MyNandCallback,
                                      &ctrl->nandCommandBlock) != NAND_RESULT_OK)
        {
            break;
        }
        return;
    case 4:
    _case_4_lbl:
        ctrl->nandStep = 5;
        if (NANDPrivateCreateAsync(ConfFileName, NAND_PERM_RWALL, 0, MyNandCallback,
                                   &ctrl->nandCommandBlock) != NAND_RESULT_OK)
        {
            break;
        }
        return;
    case 5:
    _case_5_lbl:
        ctrl->nandStep = 6;
        if (NANDPrivateOpenAsync(ConfFileName, &ctrl->nandFileInfo, NAND_ACCESS_WRITE, MyNandCallback,
                                 &ctrl->nandCommandBlock) != NAND_RESULT_OK)
        {
            break;
        }
        return;
    case 6:
        if (result != NAND_RESULT_OK) {
            break;
        }

        ctrl->nandNeedClose = TRUE;
        ctrl->nandStep = 7;

        if (NANDWriteAsync(&ctrl->nandFileInfo, ConfBufForFlush, ctrl->flushSize, MyNandCallback,
                           &ctrl->nandCommandBlock) != NAND_RESULT_OK)
        {
            break;
        }
        return;
    case 7:
        if (result != ctrl->flushSize) {
            break;
        }

        ctrl->nandNeedClose = FALSE;
        ctrl->nandStep = 8;

        if (NANDCloseAsync(&ctrl->nandFileInfo, MyNandCallback, &ctrl->nandCommandBlock) != NAND_RESULT_OK) {
            break;
        }
        return;
    case 8:
        if (result != NAND_RESULT_OK) {
            break;
        }
    case 9:
        FinishFromFlush();
    default:
        return;
    }

    ErrorFromFlush();
} // 0x80248AA0

/*
 * FinishFromFlush
 *
 * Marks the configuration dirty again after a failed write, runs the callback and publishes the result.
 */
static void FinishFromFlush(void) {
    SCControl* ctrl;
    SCFlushCallback callback;

    ctrl = &Control;
    if (ctrl->flushResult != 0) {
        __SCSetDirtyFlag();
    }

    callback = ctrl->flushCallback;
    if (callback) {
        ctrl->flushCallback = NULL;
        callback(ctrl->flushResult);

        if (ctrl->threadQueue.head != NULL) {
            OSWakeupThread(&ctrl->threadQueue);
        }
    }

    SetBgJobStatus(ctrl->flushResult);
}

/*
 * ErrorFromFlush
 *
 * Flags the flush as failed; closes the file first if it is open.
 */
static void ErrorFromFlush(void) {
    SCControl* ctrl;

    ctrl = &Control;
    ctrl->flushResult = 2;

    if (ctrl->nandNeedClose) {
        ctrl->nandStep = 9;

        if (NANDCloseAsync(&ctrl->nandFileInfo, MyNandCallback, &ctrl->nandCommandBlock) == NAND_RESULT_OK) {
            return;
        }
    }

    FinishFromFlush();
}

/*
 * __SCIsDirty
 *
 * Accessors of the dirty flag and the configuration buffer.
 */
static BOOL __SCIsDirty(void) {
    return DirtyFlag ? TRUE : FALSE;
}

static void __SCSetDirtyFlag(void) {
    DirtyFlag = TRUE;
}

static void __SCClearDirtyFlag(void) {
    DirtyFlag = FALSE;
}

static u8* __SCGetConfBuf(void) {
    return ConfBuf;
}

static u32 __SCGetConfBufSize(void) {
    return sizeof(ConfBuf);
}
