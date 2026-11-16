// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/dvd/dvd_broadway.c
// Reference: PrimeDecomp/sdk src/revolution/dvd/dvd_broadway.c (bodies of the dead-stripped functions)
// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/dvd/dvd_broadway.c
//
// DVDLow layer of the Wii DVD library: every drive command goes to the IOS
// driver /dev/di as an asynchronous ioctl (or ioctlv) whose completion runs
// one of the transaction callbacks below.
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>
#include <revolution/esp.h>
#include <revolution/ipc/ipcclt.h>
#include <revolution/ipc/ipcMain.h>

#include <string.h>

#define DVD_LOW_CTX_MAX 4
#define DVD_LOW_CMD_MAX 4
#define DVD_LOW_CTX_MAGIC 0xFEEBDAED

#define H3_HASH_TABLE_SIZE (96 * 1024)

#define IS_ALIGNED32(addr) (((u32)(addr) & 0x1F) == 0)

// /dev/di ioctl and ioctlv command numbers.
typedef enum DVDLowIoctl {
    DVD_IOCTL_INQUIRY = 0x12,
    DVD_IOCTL_READ_DISK_ID = 0x70,
    DVD_IOCTL_READ = 0x71,
    DVD_IOCTL_WAIT_FOR_COVER_CLOSE = 0x79,
    DVD_IOCTL_PREPARE_COVER_REGISTER = 0x7A,
    DVD_IOCTL_NOTIFY_RESET = 0x7E,
    DVD_IOCTL_READ_DVD_PHYSICAL = 0x80,
    DVD_IOCTL_READ_DVD_COPYRIGHT = 0x81,
    DVD_IOCTL_READ_DVD_DISC_KEY = 0x82,
    DVD_IOCTL_CLEAR_COVER_INTERRUPT = 0x86,
    DVD_IOCTL_GET_COVER_STATUS = 0x88,
    DVD_IOCTL_RESET = 0x8A,
    DVD_IOCTLV_OPEN_PARTITION = 0x8B,
    DVD_IOCTL_CLOSE_PARTITION = 0x8C,
    DVD_IOCTL_UNENCRYPTED_READ = 0x8D,
    DVD_IOCTL_ENABLE_DVD_VIDEO = 0x8E,
    DVD_IOCTLV_GET_NO_DISC_OPEN_PARTITION_PARAMS = 0x90,
    DVD_IOCTLV_NO_DISC_OPEN_PARTITION = 0x91,
    DVD_IOCTLV_GET_NO_DISC_BUFFER_SIZES = 0x92,
    DVD_IOCTLV_OPEN_PARTITION_TMD_TICKET = 0x93,
    DVD_IOCTLV_OPEN_PARTITION_TMD_TICKETVIEW = 0x94,
    DVD_IOCTL_PREPARE_STATUS_REGISTER = 0x95,
    DVD_IOCTL_PREPARE_CONTROL_REGISTER = 0x96,
    DVD_IOCTL_REPORT_KEY = 0xA4,
    DVD_IOCTL_SEEK = 0xAB,
    DVD_IOCTL_READ_DVD = 0xD0,
    DVD_IOCTL_READ_DVD_CONFIG = 0xD1,
    DVD_IOCTL_STOP_LASER = 0xD2,
    DVD_IOCTL_OFFSET = 0xD9,
    DVD_IOCTL_READ_DISK_BCA = 0xDA,
    DVD_IOCTL_REQUEST_DISC_STATUS = 0xDB,
    DVD_IOCTL_REQUEST_RETRY_NUMBER = 0xDC,
    DVD_IOCTL_SET_MAX_ROTATION = 0xDD,
    DVD_IOCTL_SER_MEAS_CONTROL = 0xDF,
    DVD_IOCTL_REQUEST_ERROR = 0xE0,
    DVD_IOCTL_STOP_MOTOR = 0xE3,
    DVD_IOCTL_AUDIO_BUFFER_CONFIG = 0xE4
} DVDLowIoctl;

// Which completion routine a context was handed to.
typedef enum callbackType {
    BOGUS_TYPE = 0,
    TRANSACTION_CB,
    COVER_CB,
    COVER_REG_CB
} callbackType_t;

// One outstanding request: the IOS callback argument.
typedef struct dvdContext {
    /* 0x00 */ DVDLowCallback callback;
    /* 0x04 */ callbackType_t callbackType;
    /* 0x08 */ u8 inUse;
    /* 0x0C */ u32 contextMagic; // DVD_LOW_CTX_MAGIC while intact
    /* 0x10 */ u32 contextNum;
    /* 0x14 */ u32 pad[3];       // pad: one context per 32-byte block
} dvdContext_t;

static volatile u8 requestInProgress = FALSE; // volatile: SDK declaration (xbret/xenoblade and PrimeDecomp/sdk dvd_broadway.c); cleared by the IOS completion callbacks
static u8 breakRequested;
static u8 callbackInProgress = FALSE;

static IOSFd DiFD = -1;

static int freeCommandBuf = 0;

static int freeDvdContext = 0;
static u8 dvdContextsInited = FALSE;
static u8 DVDLowInitCalled = FALSE;
static diCommand_t* diCommand; // DVD_LOW_CMD_MAX command blocks in IPC memory
static char* pathBuf;

static u32 readLength;
static u32 spinUpValue;

static dvdContext_t dvdContexts[DVD_LOW_CTX_MAX] ATTRIBUTE_ALIGN(32);
static diRegVals_t diRegValCache ATTRIBUTE_ALIGN(32);
static u32 registerBuf[8] ATTRIBUTE_ALIGN(32);
static IOSIoVector ioVec[10] ATTRIBUTE_ALIGN(32);
static s32 lastTicketError[8] ATTRIBUTE_ALIGN(32);
static u32 coverStatus[8] ATTRIBUTE_ALIGN(32);
static u32 coverRegister[8] ATTRIBUTE_ALIGN(32);
static u32 statusRegister[8] ATTRIBUTE_ALIGN(32);
static u32 controlRegister[8] ATTRIBUTE_ALIGN(32);

/*
 * ddrAllocAligned32
 *
 * Takes size bytes from the bottom of the IPC buffer.
 */
static void* ddrAllocAligned32(const int size) {
    void* low;
    void* high;

    if (!IS_ALIGNED32(size)) {
        return 0;
    }

    low = IPCGetBufferLo();
    high = IPCGetBufferHi();

    // The SDK's bug: "& 31" keeps the remainder instead of rounding the pointer up.
    if (!IS_ALIGNED32(low)) {
        low = (void*)(((u32)low + 31) & 31);
    }

    if ((u32)low + size > (u32)high) {
        OSReport("(ddrAllocAligned32) Not enough space to allocate %d bytes\n", size);
    }

    IPCSetBufferLo((void*)((u32)low + size));
    return low;
} // no copy in retail (inlined)

/*
 * allocateStructures
 *
 * Allocates the command blocks and the path buffer in IPC memory.
 */
static BOOL allocateStructures(void) {
    if ((diCommand = ddrAllocAligned32(sizeof(diCommand_t) * DVD_LOW_CMD_MAX)) == 0) {
        OSReport("Allocation of diCommand blocks failed\n");
        return FALSE;
    }

    if ((pathBuf = ddrAllocAligned32(32)) == 0) {
        OSReport("Allocation of pathBuf failed\n");
        return FALSE;
    }

    return TRUE;
} // no copy in retail (inlined)

/*
 * initDvdContexts
 *
 * External in the SDK (PrimeDecomp/sdk): its out-of-line copy, compiled first and dead-stripped by the link, is the
 * first user of dvdContexts, which puts dvdContexts first in .bss.
 */
void initDvdContexts(void) {
    int i;

    for (i = 0; i < DVD_LOW_CTX_MAX; i++) {
        dvdContexts[i].callback = NULL;
        dvdContexts[i].callbackType = BOGUS_TYPE;
        dvdContexts[i].inUse = FALSE;
        dvdContexts[i].contextMagic = DVD_LOW_CTX_MAGIC;
        dvdContexts[i].contextNum = i;
    }

    freeDvdContext = 0;
} // no copy in retail (dead-stripped or inlined)

/*
 * newContext
 *
 * Claims the next context of the ring; spins forever if it is still in use or damaged.
 */
static inline dvdContext_t* newContext(const DVDLowCallback callback, const callbackType_t type) {
    int returnIndex;
    u8 use = dvdContexts[freeDvdContext].inUse != FALSE;

    if (use == TRUE) {
        OSReport("(newContext) ERROR: freeDvdContext.inUse (#%d) is true\n", freeDvdContext);
        OSReport("(newContext) Now spinning in infinite loop\n");

        while (TRUE) {
        }
    }

    if (dvdContexts[freeDvdContext].contextMagic != DVD_LOW_CTX_MAGIC) {
        OSReport("(newContext) Something overwrote the context magic - spinning \n");

        while (TRUE) {
        }
    }

    dvdContexts[freeDvdContext].callback = callback;
    dvdContexts[freeDvdContext].callbackType = type;
    dvdContexts[freeDvdContext].inUse = TRUE;
    returnIndex = freeDvdContext;
    freeDvdContext++;

    if (freeDvdContext >= DVD_LOW_CTX_MAX) {
        freeDvdContext = 0;
    }

    return &dvdContexts[returnIndex];
} // no copy in retail (inlined)

/*
 * nextCommandBuf
 *
 * Advances a command-block index around the ring.
 */
static inline void nextCommandBuf(int* bufNum) {
    (*bufNum)++;

    if (*bufNum >= DVD_LOW_CMD_MAX) {
        *bufNum = 0;
    }
} // no copy in retail (inlined)

/*
 * doTransactionCallback
 *
 * IOS completion of an ordinary request: reports a break, forwards the result to the caller's callback.
 */
static IOSError doTransactionCallback(IOSError intType, void* context) {
    dvdContext_t* dvdContext = (dvdContext_t*)context;

    if (dvdContext->contextMagic != DVD_LOW_CTX_MAGIC) {
        OSReport("(doTransactionCallback) Error - context mangled!\n");
        dvdContext->contextMagic = DVD_LOW_CTX_MAGIC;
    } else {
        requestInProgress = FALSE;

        if (dvdContext->callback != 0) {
            callbackInProgress = TRUE;

            if (breakRequested == TRUE) {
                breakRequested = FALSE;
                intType |= DVD_INTTYPE_BR;
            }

            if (intType & DVD_INTTYPE_TC) {
                readLength = 0;
            }

            dvdContext->callback(intType);
            callbackInProgress = FALSE;
        }
    }

    dvdContext->inUse = FALSE;
    return IOS_ERROR_OK;
} // 0x802258A0

/*
 * doCoverCallback
 *
 * IOS completion of DVDLowWaitForCoverClose.
 */
static IOSError doCoverCallback(IOSError intType, void* context) {
    dvdContext_t* dvdContext = (dvdContext_t*)context;

    requestInProgress = FALSE;

    if (dvdContext->contextMagic != DVD_LOW_CTX_MAGIC) {
        OSReport("(doCoverCallback) Error - context mangled!\n");
        dvdContext->contextMagic = DVD_LOW_CTX_MAGIC;
    } else {
        if (dvdContext->callback != 0) {
            callbackInProgress = TRUE;

            if (breakRequested == TRUE) {
                breakRequested = FALSE;
                intType |= DVD_INTTYPE_BR;
            }

            dvdContext->callback(intType);
            callbackInProgress = FALSE;
        }
    }

    dvdContext->inUse = FALSE;
    return IOS_ERROR_OK;
} // no copy in retail (dead-stripped)

/*
 * doPrepareCoverRegisterCallback
 *
 * IOS completion of DVDLowPrepareCoverRegister: caches the cover register, then as doTransactionCallback.
 */
static IOSError doPrepareCoverRegisterCallback(IOSError intType, void* context) {
    dvdContext_t* dvdContext = (dvdContext_t*)context;

    requestInProgress = FALSE;
    diRegValCache.CoverRegVal = registerBuf[0];

    if (dvdContext->contextMagic != DVD_LOW_CTX_MAGIC) {
        // The SDK's message names doTransactionCallback here too.
        OSReport("(doTransactionCallback) Error - context mangled!\n");
        dvdContext->contextMagic = DVD_LOW_CTX_MAGIC;
    } else {
        if (dvdContext->callback != 0) {
            callbackInProgress = TRUE;

            if (breakRequested == TRUE) {
                breakRequested = FALSE;
                intType |= DVD_INTTYPE_BR;
            }

            dvdContext->callback(intType);
            callbackInProgress = FALSE;
        }
    }

    dvdContext->inUse = FALSE;
    return IOS_ERROR_OK;
} // 0x80225960

/*
 * DVDLowFinalize
 */
BOOL DVDLowFinalize(void) {
    IOSError rv = IOS_Close(DiFD);

    if (rv != IOS_ERROR_OK) {
        OSReport("(DVDLowFinish) Error: IOS_Close failed\n");
        return FALSE;
    }

    DVDLowInitCalled = FALSE;
    return TRUE;
} // 0x80225A20

/*
 * DVDLowInit
 *
 * First call: starts the IPC client and allocates the command blocks and contexts. Every call opens /dev/di.
 */
BOOL DVDLowInit(void) {
    IOSError rv;

    if (!DVDLowInitCalled) {
        DVDLowInitCalled = TRUE;

        rv = IPCCltInit();
        if (rv != IOS_ERROR_OK) {
            OSReport("IPCCltInit returned error: %d\n", rv);
            return FALSE;
        }

        if (!allocateStructures()) {
            return FALSE;
        }

        if (!dvdContextsInited) {
            initDvdContexts();
            dvdContextsInited = TRUE;
        }
    }

    strncpy(pathBuf, "/dev/di", 32);
    DiFD = IOS_Open(pathBuf, 0);

    if (DiFD >= 0) {
        return TRUE;
    }

    switch (DiFD) {
    case IOS_ERROR_NOEXISTS:
        OSReport("(DVDLowInit) Error: IOS_Open failed - pathname '/dev/di' does not exist\n");
        return FALSE;
    case IOS_ERROR_ACCESS:
        OSReport("(DVDLowInit) Error: IOS_Open failed - calling thread lacks permission\n");
        return FALSE;
    case IOS_ERROR_MAX:
        OSReport("(DVDLowInit) Error: IOS_Open failed - connection limit has been reached\n");
        return FALSE;
    default:
        OSReport("(DVDLowInit) IOS_Open failed, errorcode = %d\n", DiFD);
        return FALSE;
    }
} // 0x80225A70

/*
 * DVDLowReadDiskID
 */
BOOL DVDLowReadDiskID(DVDDiskID* diskID, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    if (diskID == 0) {
        OSReport("@@@@@@ WARNING - Calling DVDLowReadDiskId with NULL ptr\n");
    }

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DISK_ID;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DISK_ID, &diCommand[freeCommandBuf], sizeof(diCommand_t), diskID,
                        sizeof(DVDDiskID), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDiskID) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80225CE0

/*
 * DVDLowOpenPartition
 */
BOOL DVDLowOpenPartition(const u32 partitionWordOffset, const ESTicket* const eTicket, const u32 numCertBytes,
                         const u8* const certificates, ESTitleMeta* tmd, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (eTicket != 0 && ((u32)eTicket & 0x1F)) {
        OSReport("(DVDLowOpenPartition) eTicket memory is unaligned\n");
        return FALSE;
    }

    if (certificates != 0 && ((u32)certificates & 0x1F)) {
        OSReport("(DVDLowOpenPartition) certificates memory is unaligned\n");
        return FALSE;
    }

    if (tmd != 0 && ((u32)tmd & 0x1F)) {
        // The SDK's message names the certificates for the tmd check too.
        OSReport("(DVDLowOpenPartition) certificates memory is unaligned\n");
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTLV_OPEN_PARTITION;
    diCommand[freeCommandBuf].arg[0] = partitionWordOffset;

    ioVec[0].base = (u8*)&diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(diCommand_t);

    ioVec[1].base = (u8*)eTicket;
    if (eTicket == 0) {
        ioVec[1].length = 0;
    } else {
        ioVec[1].length = sizeof(ESTicket);
    }

    ioVec[2].base = (u8*)certificates;
    if (certificates == 0) {
        ioVec[2].length = 0;
    } else {
        ioVec[2].length = numCertBytes;
    }

    ioVec[3].base = (u8*)tmd;
    ioVec[3].length = sizeof(ESTitleMeta);

    ioVec[4].base = (u8*)lastTicketError;
    ioVec[4].length = sizeof(lastTicketError);

    rv = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_OPEN_PARTITION, 3, 2, ioVec, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowOpenPartition) IOS_IoctlvAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80225E70

/*
 * DVDLowOpenPartitionWithTmdAndTicket
 */
BOOL DVDLowOpenPartitionWithTmdAndTicket(const u32 partitionWordOffset, const ESTicket* const eTicket,
                                         const u32 numTmdBytes, const ESTitleMeta* const tmd, const u32 numCertBytes,
                                         const u8* const certificates, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (eTicket != 0 && !IS_ALIGNED32(eTicket)) {
        OSReport("(%s) eTicket memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    if (certificates != 0 && !IS_ALIGNED32(certificates)) {
        return FALSE;
    }

    if (tmd == 0) {
        OSReport("(%s) tmd parameter cannot be NULL\n", __FUNCTION__);
        return FALSE;
    } else if (!IS_ALIGNED32(tmd)) {
        OSReport("(%s) tmd memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    if (eTicket == 0) {
        OSReport("(%s) eTicket parameter cannot be NULL\n", __FUNCTION__);
        return FALSE;
    } else if (!IS_ALIGNED32(eTicket)) {
        OSReport("(%s) eTicket memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTLV_OPEN_PARTITION_TMD_TICKET;
    diCommand[freeCommandBuf].arg[0] = partitionWordOffset;

    ioVec[0].base = (u8*)&diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(diCommand_t);

    ioVec[1].base = (u8*)eTicket;
    ioVec[1].length = sizeof(ESTicket);

    ioVec[2].base = (u8*)tmd;
    ioVec[2].length = numTmdBytes;

    ioVec[3].base = (u8*)certificates;
    if (certificates == 0) {
        ioVec[3].length = 0;
    } else {
        ioVec[3].length = numCertBytes;
    }

    ioVec[4].base = (u8*)lastTicketError;
    ioVec[4].length = sizeof(lastTicketError);

    rv = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_OPEN_PARTITION_TMD_TICKET, 4, 1, ioVec, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowOpenPartition) IOS_IoctlvAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowOpenPartitionWithTmdAndTicketView
 */
BOOL DVDLowOpenPartitionWithTmdAndTicketView(const u32 partitionWordOffset, const ESTicketView* const eTicketView,
                                             const u32 numTmdBytes, const ESTitleMeta* const tmd,
                                             const u32 numCertBytes, const u8* const certificates,
                                             DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (certificates != 0 && ((u32)certificates & 0x1F)) {
        return FALSE;
    }

    if (tmd == 0) {
        OSReport("(%s) tmd parameter cannot be NULL\n", __FUNCTION__);
        return FALSE;
    } else if (((u32)tmd & 0x1F)) {
        OSReport("(%s) tmd memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    if (eTicketView == 0) {
        OSReport("(%s) eTicketView parameter cannot be NULL\n", __FUNCTION__);
        return FALSE;
    } else if (((u32)eTicketView & 0x1F)) {
        OSReport("(%s) eTicketView memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);
    nextCommandBuf(&freeCommandBuf);

    diCommand[freeCommandBuf].theCommand = DVD_IOCTLV_OPEN_PARTITION_TMD_TICKETVIEW;
    diCommand[freeCommandBuf].arg[0] = partitionWordOffset;

    ioVec[0].base = (u8*)&diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(diCommand_t);

    ioVec[1].base = (u8*)eTicketView;
    ioVec[1].length = sizeof(ESTicketView);

    ioVec[2].base = (u8*)tmd;
    ioVec[2].length = numTmdBytes;

    ioVec[3].base = (u8*)certificates;
    if (certificates == 0) {
        ioVec[3].length = 0;
    } else {
        ioVec[3].length = numCertBytes;
    }

    ioVec[4].base = (u8*)lastTicketError;
    ioVec[4].length = sizeof(lastTicketError);

    rv = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_OPEN_PARTITION_TMD_TICKETVIEW, 4, 1, ioVec, doTransactionCallback,
                         dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowOpenPartition) IOS_IoctlvAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x802260E0

/*
 * DVDLowGetNoDiscBufferSizes
 */
BOOL DVDLowGetNoDiscBufferSizes(const u32 partitionWordOffset, u32* numTmdBytes, u32* numCertBytes,
                                DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (numTmdBytes == 0 || numCertBytes == 0) {
        OSReport("(%s) Error: NULL pointer argument\n", __FUNCTION__);
        return FALSE;
    }

    if (((u32)numTmdBytes & 0x1F)) {
        OSReport("(%s) numTmdBytes memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    if (((u32)numCertBytes & 0x1F)) {
        OSReport("(%s) certificates memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);
    nextCommandBuf(&freeCommandBuf);

    diCommand[freeCommandBuf].theCommand = DVD_IOCTLV_GET_NO_DISC_BUFFER_SIZES;
    diCommand[freeCommandBuf].arg[0] = partitionWordOffset;

    ioVec[0].base = (u8*)&diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(diCommand_t);

    ioVec[1].base = (u8*)numTmdBytes;
    ioVec[1].length = sizeof(u32);

    ioVec[2].base = (u8*)numCertBytes;
    ioVec[2].length = sizeof(u32);

    rv = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_GET_NO_DISC_BUFFER_SIZES, 1, 2, ioVec, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (%s) IOS_IoctlvAsync returned error: %d\n", __FUNCTION__, rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226350

/*
 * DVDLowGetNoDiscOpenPartitionParams
 */
BOOL DVDLowGetNoDiscOpenPartitionParams(const u32 partitionWordOffset, ESTicket* eTicket, u32* numTmdBytes,
                                        ESTitleMeta* tmd, u32* numCertBytes, u8* certificates, u32* dataWordOffset,
                                        u8* h3HashPtr, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (eTicket == 0 || numTmdBytes == 0 || tmd == 0 || numCertBytes == 0 || certificates == 0 ||
        dataWordOffset == 0 || h3HashPtr == 0) {
        OSReport("(%s) Error: NULL pointer argument\n", __FUNCTION__);
        return FALSE;
    }

    if (!IS_ALIGNED32(eTicket) || !IS_ALIGNED32(numTmdBytes) || !IS_ALIGNED32(tmd) || !IS_ALIGNED32(numCertBytes) ||
        !IS_ALIGNED32(certificates) || !IS_ALIGNED32(dataWordOffset) || !IS_ALIGNED32(h3HashPtr)) {
        OSReport("(%s) pointer argument is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);
    nextCommandBuf(&freeCommandBuf);

    diCommand[freeCommandBuf].theCommand = DVD_IOCTLV_GET_NO_DISC_OPEN_PARTITION_PARAMS;
    diCommand[freeCommandBuf].arg[0] = partitionWordOffset;

    // In: the command, the tmd size, the certificate size.
    ioVec[0].base = (u8*)&diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(diCommand_t);

    ioVec[1].base = (u8*)numTmdBytes;
    ioVec[1].length = sizeof(u32);

    ioVec[2].base = (u8*)numCertBytes;
    ioVec[2].length = sizeof(u32);

    // Out: ticket, tmd size, tmd, certificate size, certificates, data offset, H3 hash table.
    ioVec[3].base = (u8*)eTicket;
    ioVec[3].length = sizeof(ESTicket);

    ioVec[4].base = (u8*)numTmdBytes;
    ioVec[4].length = sizeof(u32);

    ioVec[5].base = (u8*)tmd;
    ioVec[5].length = *numTmdBytes;

    ioVec[6].base = (u8*)numCertBytes;
    ioVec[6].length = sizeof(u32);

    ioVec[7].base = certificates;
    ioVec[7].length = *numCertBytes;

    ioVec[8].base = (u8*)dataWordOffset;
    ioVec[8].length = sizeof(u32);

    ioVec[9].base = h3HashPtr;
    ioVec[9].length = H3_HASH_TABLE_SIZE;

    rv = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_GET_NO_DISC_OPEN_PARTITION_PARAMS, 3, 7, ioVec, doTransactionCallback,
                         dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (%s) IOS_IoctlvAsync returned error: %d\n", __FUNCTION__, rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226560

/*
 * DVDLowNoDiscOpenPartition
 */
BOOL DVDLowNoDiscOpenPartition(const ESTicket* const eTicket, const u32 numTmdBytes, const ESTitleMeta* const tmd,
                               const u32 numCertBytes, const u8* const certificates, const u32 dataWordOffset,
                               const u8* const h3HashPtr, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (eTicket == 0 || tmd == 0 || certificates == 0) {
        OSReport("(%s) Error: NULL pointer argument\n", __FUNCTION__);
        return FALSE;
    }

    if (!IS_ALIGNED32(eTicket) || !IS_ALIGNED32(tmd) || !IS_ALIGNED32(certificates) ||
        !IS_ALIGNED32(dataWordOffset) || !IS_ALIGNED32(h3HashPtr)) {
        OSReport("(%s) pointer argument is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTLV_NO_DISC_OPEN_PARTITION;
    diCommand[freeCommandBuf].arg[0] = dataWordOffset;

    ioVec[0].base = (u8*)&diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(diCommand_t);

    ioVec[1].base = (u8*)eTicket;
    ioVec[1].length = sizeof(ESTicket);

    ioVec[2].base = (u8*)tmd;
    ioVec[2].length = numTmdBytes;

    ioVec[3].base = (u8*)certificates;
    ioVec[3].length = numCertBytes;

    ioVec[4].base = (u8*)h3HashPtr;
    ioVec[4].length = H3_HASH_TABLE_SIZE;

    ioVec[5].base = (u8*)lastTicketError;
    ioVec[5].length = sizeof(u32);

    rv = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_NO_DISC_OPEN_PARTITION, 5, 1, ioVec, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (%s) IOS_IoctlvAsync returned error: %d\n", __FUNCTION__, rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowClosePartition
 */
BOOL DVDLowClosePartition(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_CLOSE_PARTITION;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_CLOSE_PARTITION, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL, 0,
                        doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowClosePartition) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226800

/*
 * DVDLowUnencryptedRead
 */
BOOL DVDLowUnencryptedRead(void* destAddr, u32 length, u32 wordOffset, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    readLength = length;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_UNENCRYPTED_READ;
    diCommand[freeCommandBuf].arg[0] = length;
    diCommand[freeCommandBuf].arg[1] = wordOffset;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_UNENCRYPTED_READ, &diCommand[freeCommandBuf], sizeof(diCommand_t), destAddr,
                        length, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowUnencryptedRead) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226970

/*
 * DVDLowStopMotor
 */
BOOL DVDLowStopMotor(BOOL eject, BOOL kill, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_STOP_MOTOR;
    diCommand[freeCommandBuf].arg[0] = eject;
    diCommand[freeCommandBuf].arg[1] = kill;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_STOP_MOTOR, &diCommand[freeCommandBuf], sizeof(diCommand_t), &diRegValCache,
                        sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowStopMotor) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226B10

/*
 * DVDLowWaitForCoverClose
 */
BOOL DVDLowWaitForCoverClose(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, COVER_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_WAIT_FOR_COVER_CLOSE;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_WAIT_FOR_COVER_CLOSE, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL,
                        0, doCoverCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowWaitForCoverClose) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowInquiry
 */
BOOL DVDLowInquiry(DVDDriveInfo* info, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_INQUIRY;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_INQUIRY, &diCommand[freeCommandBuf], sizeof(diCommand_t), info,
                        sizeof(DVDDriveInfo), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowInquiry) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226CB0

/*
 * DVDLowRequestError
 */
BOOL DVDLowRequestError(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_REQUEST_ERROR;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_REQUEST_ERROR, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        &diRegValCache, sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowRequestError) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226E20

/*
 * DVDLowSetSpinupFlag
 *
 * Records the spin-up flag that DVDLowReset passes to the drive.
 */
BOOL DVDLowSetSpinupFlag(u32 spinUp) {
    spinUpValue = spinUp;
    return TRUE;
} // 0x80226F90

/*
 * DVDLowNotifyReset
 *
 * Synchronous: refused from inside a DVDLow callback.
 */
BOOL DVDLowNotifyReset(void) {
    IOSError rv;

    if (callbackInProgress == TRUE) {
        // The SDK's message names DVDLowSetSpinupFlag here.
        OSReport("(DVDLowSetSpinupFlag): Synch functions can't be called in callbacks\n");
        return FALSE;
    }

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_NOTIFY_RESET;

    rv = IOS_Ioctl(DiFD, DVD_IOCTL_NOTIFY_RESET, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL, 0);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowNotifyReset) IOS_IoctlAsync returned error: %d\n", rv);
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReset
 */
BOOL DVDLowReset(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_RESET;
    diCommand[freeCommandBuf].arg[0] = spinUpValue;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_RESET, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL, 0,
                        doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReset) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80226FA0

/*
 * DVDLowAudioBufferConfig
 */
BOOL DVDLowAudioBufferConfig(u32 enable, u32 size, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_AUDIO_BUFFER_CONFIG;
    diCommand[freeCommandBuf].arg[0] = enable;
    diCommand[freeCommandBuf].arg[1] = size;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_AUDIO_BUFFER_CONFIG, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        &diRegValCache, sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowAudioBufferConfig) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80227120

/*
 * DVDLowGetCoverStatus
 *
 * Synchronous: refused from inside a DVDLow callback.
 */
u32 DVDLowGetCoverStatus(void) {
    IOSError rv;

    if (callbackInProgress == TRUE) {
        OSReport("(DVDLowGetCoverStatus): Synch functions can't be called in callbacks\n");
        return 0;
    }

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_GET_COVER_STATUS;

    rv = IOS_Ioctl(DiFD, DVD_IOCTL_GET_COVER_STATUS, &diCommand[freeCommandBuf], sizeof(diCommand_t), coverStatus,
                   sizeof(coverStatus));

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowGetCoverStatus) IOS_Ioctl returned error: %d\n", rv);
        return 0xDEADDEAD; // the SDK's marker for a failed status read
    }

    return coverStatus[0];
} // no copy in retail (dead-stripped)

/*
 * DVDLowReadDvd
 */
BOOL DVDLowReadDvd(u32 strm, u32 retry, void* destAddr, u32 lengthInSectors, u32 lsn, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DVD;
    if (strm == 0) {
        diCommand[freeCommandBuf].arg[0] = 0;
    } else {
        diCommand[freeCommandBuf].arg[0] = 1;
    }
    if (retry == 0) {
        diCommand[freeCommandBuf].arg[1] = 0;
    } else {
        diCommand[freeCommandBuf].arg[1] = 1;
    }
    diCommand[freeCommandBuf].arg[2] = lengthInSectors;
    diCommand[freeCommandBuf].arg[3] = lsn;
    readLength = lengthInSectors * 2048;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DVD, &diCommand[freeCommandBuf], sizeof(diCommand_t), destAddr,
                        lengthInSectors * 2048, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDVD) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReadDvdConfig
 */
BOOL DVDLowReadDvdConfig(u8 set, u32 type, u32 config, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DVD_CONFIG;
    diCommand[freeCommandBuf].arg[0] = set;
    diCommand[freeCommandBuf].arg[1] = type;
    diCommand[freeCommandBuf].arg[2] = config;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DVD_CONFIG, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        &diRegValCache, sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDVDConfig) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReadDvdCopyright
 */
BOOL DVDLowReadDvdCopyright(u32 layer, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DVD_COPYRIGHT;
    diCommand[freeCommandBuf].arg[0] = layer;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DVD_COPYRIGHT, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        &diRegValCache, sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDvdCopyright) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReadDvdPhysical
 */
BOOL DVDLowReadDvdPhysical(DVDVideoPhysical* physical, u32 layer, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DVD_PHYSICAL;
    diCommand[freeCommandBuf].arg[0] = layer;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DVD_PHYSICAL, &diCommand[freeCommandBuf], sizeof(diCommand_t), physical,
                        sizeof(DVDVideoPhysical), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDvdPhysical) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReadDvdDiscKey
 */
BOOL DVDLowReadDvdDiscKey(DVDVideoDiscKey* discKey, u32 layer, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DVD_DISC_KEY;
    diCommand[freeCommandBuf].arg[0] = layer;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DVD_DISC_KEY, &diCommand[freeCommandBuf], sizeof(diCommand_t), discKey,
                        sizeof(DVDVideoDiscKey), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDvdDiscKey) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReportKey
 */
BOOL DVDLowReportKey(DVDVideoReportKey* reportKey, u32 format, u32 lsn, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_REPORT_KEY;
    diCommand[freeCommandBuf].arg[0] = format >> 16;
    diCommand[freeCommandBuf].arg[1] = lsn;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_REPORT_KEY, &diCommand[freeCommandBuf], sizeof(diCommand_t), reportKey,
                        sizeof(DVDVideoReportKey), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReportKey) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x802272C0

/*
 * DVDLowOffset
 */
BOOL DVDLowOffset(u32 subcmd, u32 wordOffset, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_OFFSET;
    if (subcmd == 0) {
        diCommand[freeCommandBuf].arg[0] = 0;
    } else {
        diCommand[freeCommandBuf].arg[0] = 1;
    }
    diCommand[freeCommandBuf].arg[1] = wordOffset;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_OFFSET, &diCommand[freeCommandBuf], sizeof(diCommand_t), &diRegValCache,
                        sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowOffset) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowStopLaser
 */
BOOL DVDLowStopLaser(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_STOP_LASER;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_STOP_LASER, &diCommand[freeCommandBuf], sizeof(diCommand_t), &diRegValCache,
                        sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowStopLaser) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowReadDiskBca
 */
BOOL DVDLowReadDiskBca(DVDDiskBca* diskBca, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ_DISK_BCA;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DISK_BCA, &diCommand[freeCommandBuf], sizeof(diCommand_t), diskBca,
                        sizeof(DVDDiskBca), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowReadDiskBca) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowSerMeasControl
 */
BOOL DVDLowSerMeasControl(DVDLowDriveSer* ser, u8 clear, u8 enable, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_SER_MEAS_CONTROL;
    diCommand[freeCommandBuf].arg[0] = clear;
    diCommand[freeCommandBuf].arg[1] = enable;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_SER_MEAS_CONTROL, &diCommand[freeCommandBuf], sizeof(diCommand_t), ser,
                        sizeof(DVDLowDriveSer), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowSerMeasControl) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowRequestDiscStatus
 */
BOOL DVDLowRequestDiscStatus(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_REQUEST_DISC_STATUS;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_REQUEST_DISC_STATUS, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        &diRegValCache, sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowRequestDiscStatus) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowRequestRetryNumber
 */
BOOL DVDLowRequestRetryNumber(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_REQUEST_RETRY_NUMBER;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_REQUEST_RETRY_NUMBER, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        &diRegValCache, sizeof(diRegVals_t), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowRequestRetryNumber) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)

/*
 * DVDLowSetMaximumRotation
 */
BOOL DVDLowSetMaximumRotation(u32 subcmd, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_SET_MAX_ROTATION;
    diCommand[freeCommandBuf].arg[0] = (subcmd >> 16) & 3;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_SET_MAX_ROTATION, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL, 0,
                        doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowSetMaxRotation) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80227460

/*
 * DVDLowRead
 */
BOOL DVDLowRead(void* destAddr, u32 length, u32 wordOffset, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    if (!IS_ALIGNED32(destAddr)) {
        OSReport("(DVDLowRead): ERROR - destAddr buffer is not 32 byte aligned\n");
        return FALSE;
    }

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    readLength = length;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_READ;
    diCommand[freeCommandBuf].arg[0] = length;
    diCommand[freeCommandBuf].arg[1] = wordOffset;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ, &diCommand[freeCommandBuf], sizeof(diCommand_t), destAddr, length,
                        doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowRead) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x802275F0

/*
 * DVDLowSeek
 */
BOOL DVDLowSeek(u32 wordOffset, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_SEEK;
    diCommand[freeCommandBuf].arg[0] = wordOffset;

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_SEEK, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL, 0,
                        doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowSeek) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x802277A0

/*
 * DVDLowGetCoverReg
 *
 * Synchronous: refused from inside a DVDLow callback.
 */
u32 DVDLowGetCoverReg(void) {
    IOSError rv;

    if (callbackInProgress == TRUE) {
        OSReport("(DVDLowGetCoverReg): Synch functions can't be called in callbacks\n");
        return 0;
    }

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_PREPARE_COVER_REGISTER;

    rv = IOS_Ioctl(DiFD, DVD_IOCTL_PREPARE_COVER_REGISTER, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                   coverRegister, sizeof(coverRegister));

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowGetCoverReg) IOS_Ioctl returned error: %d\n", rv);
        return 0;
    }

    return coverRegister[0];
} // no copy in retail (dead-stripped)

/*
 * DVDLowGetCoverRegister
 *
 * The cover register as last cached by DVDLowPrepareCoverRegister.
 */
u32 DVDLowGetCoverRegister(void) {
    return diRegValCache.CoverRegVal;
} // 0x80227920

/*
 * DVDLowGetStatusRegister
 */
u32 DVDLowGetStatusRegister(void) {
    return statusRegister[0];
} // 0x80227930

/*
 * DVDLowGetControlRegister
 */
u32 DVDLowGetControlRegister(void) {
    return controlRegister[0];
} // 0x80227940

/*
 * DVDLowPrepareCoverRegister
 */
BOOL DVDLowPrepareCoverRegister(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_PREPARE_COVER_REGISTER;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_PREPARE_COVER_REGISTER, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        registerBuf, sizeof(registerBuf), doPrepareCoverRegisterCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        // "Regsiter" is the SDK's spelling.
        OSReport("@@@ (DVDLowPrepareCoverRegsiter) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80227950

/*
 * DVDLowPrepareStatusRegister
 */
BOOL DVDLowPrepareStatusRegister(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_PREPARE_STATUS_REGISTER;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_PREPARE_STATUS_REGISTER, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        statusRegister, sizeof(statusRegister), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowPrepareStatusRegsiter) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80227AC0

/*
 * DVDLowPrepareControlRegister
 */
BOOL DVDLowPrepareControlRegister(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_PREPARE_CONTROL_REGISTER;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_PREPARE_CONTROL_REGISTER, &diCommand[freeCommandBuf], sizeof(diCommand_t),
                        controlRegister, sizeof(controlRegister), doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowPrepareControlRegister) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80227C30

/*
 * DVDLowGetImmBufferReg
 */
u32 DVDLowGetImmBufferReg(void) {
    return diRegValCache.ImmRegVal;
} // 0x80227DA0

/*
 * DVDLowUnmaskStatusInterrupts
 */
BOOL DVDLowUnmaskStatusInterrupts(void) {
    return TRUE;
} // 0x80227DB0

/*
 * DVDLowMaskCoverInterrupt
 */
BOOL DVDLowMaskCoverInterrupt(void) {
    return TRUE;
} // 0x80227DC0

/*
 * DVDLowClearCoverInterrupt
 */
BOOL DVDLowClearCoverInterrupt(DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_CLEAR_COVER_INTERRUPT;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_CLEAR_COVER_INTERRUPT, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL,
                        0, doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowClearCoverInterrupt) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // 0x80227DD0

/*
 * __DVDLowTestAlarm
 *
 * The IOS driver keeps no DVDLow alarm: no alarm is ever ours.
 */
BOOL __DVDLowTestAlarm(const OSAlarm* alarm) {
#pragma unused(alarm)
    return FALSE;
} // 0x80227F40

/*
 * DVDLowEnableDvdVideo
 */
BOOL DVDLowEnableDvdVideo(const u8 enable, DVDLowCallback callback) {
    dvdContext_t* dvdContext;
    IOSError rv;

    nextCommandBuf(&freeCommandBuf);
    diCommand[freeCommandBuf].theCommand = DVD_IOCTL_ENABLE_DVD_VIDEO;
    diCommand[freeCommandBuf].arg[0] = enable;

    requestInProgress = TRUE;
    dvdContext = newContext(callback, TRANSACTION_CB);

    rv = IOS_IoctlAsync(DiFD, DVD_IOCTL_ENABLE_DVD_VIDEO, &diCommand[freeCommandBuf], sizeof(diCommand_t), NULL, 0,
                        doTransactionCallback, dvdContext);

    if (rv != IOS_ERROR_OK) {
        OSReport("@@@ (DVDLowEnableDvdVideo) IOS_IoctlAsync returned error: %d\n", rv);
        dvdContext->inUse = FALSE;
        return FALSE;
    }

    return TRUE;
} // no copy in retail (dead-stripped)
