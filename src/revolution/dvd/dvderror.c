// Reference: zeldaret/tp libs/revolution/src/dvd/dvderror.c
//
// Writes the last DVD error to /shared2/test2/dvderror.dat through a chain of asynchronous NAND callbacks.
// The file holds a header record (__FirstErrorInfo, whose nextOffset names the slot to overwrite) followed by
// seven rotating error records. Any failure ends the chain by calling the user callback with 2; success is 1.
#include <revolution/os.h>
#include <revolution/nand.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>

/* Register value stored when the register read did not complete. */
#define DVD_REGISTER_UNKNOWN 0xFFFFFFFF

static BOOL ExistFlag = FALSE;
static NANDCommandBlock NandCb;
static NANDFileInfo NandInfo;
static DVDCBCallback Callback;
static u32 NextOffset;
DVDErrorInfo __ErrorInfo ATTRIBUTE_ALIGN(32);
DVDErrorInfo __FirstErrorInfo ATTRIBUTE_ALIGN(32);

/*
 * cbForNandClose
 *
 * Last link of the chain: reports the outcome of closing the file.
 */
static void cbForNandClose(s32 result, NANDCommandBlock* block) {
    if (Callback) {
        Callback((result == 0) ? 1 : 2, NULL);
    }
} // 0x80224990

/*
 * cbForNandWrite
 *
 * Closes the file once the error record is written.
 */
static void cbForNandWrite(s32 result, NANDCommandBlock* block) {
    if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != 0) {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x802249C0

/*
 * cbForNandSeek
 *
 * Positioned at the error slot: stores the next slot index in the record when this is slot 0, then writes it.
 */
static void cbForNandSeek(s32 result, NANDCommandBlock* block) {
    if (result == sizeof(DVDErrorInfo) * (1 + NextOffset)) {
        if (NextOffset == 0) {
            __ErrorInfo.nextOffset = (1 + NextOffset) % 7;
        }

        DCFlushRange((void*)&__ErrorInfo, sizeof(__ErrorInfo));

        if (NANDWriteAsync(&NandInfo, (void*)&__ErrorInfo, sizeof(__ErrorInfo), cbForNandWrite, &NandCb) != 0) {
            cbForNandWrite(-1, NULL);
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80224A20

/*
 * cbForNandWrite0
 *
 * The header record is written: seeks to the error slot named by NextOffset.
 */
static void cbForNandWrite0(s32 result, NANDCommandBlock* block) {
    if (result == sizeof(__FirstErrorInfo)) {
        if (NANDSeekAsync(&NandInfo, (s32)(sizeof(DVDErrorInfo) * (1 + NextOffset)), 0, cbForNandSeek, &NandCb) != 0) {
            cbForNandSeek(-1, NULL);
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80224B20

/*
 * cbForNandSeek2
 *
 * Positioned after the header: advances the stored slot index modulo 7 and rewrites the header.
 */
static void cbForNandSeek2(s32 result, NANDCommandBlock* block) {
    if (result == sizeof(DVDErrorInfo)) {
        __FirstErrorInfo.nextOffset = (__FirstErrorInfo.nextOffset + 1) % 7;

        if (NANDWriteAsync(&NandInfo, (void*)&__FirstErrorInfo, sizeof(__FirstErrorInfo), cbForNandWrite0, &NandCb) != 0) {
            cbForNandWrite0(-1, NULL);
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80224C80

/*
 * cbForNandRead
 *
 * The header record is read: takes NextOffset from it and seeks back to rewrite it. A failed read starts the log over at slot 1.
 */
static void cbForNandRead(s32 result, NANDCommandBlock* block) {
    if (result == sizeof(DVDErrorInfo)) {
        NextOffset = __FirstErrorInfo.nextOffset;

        if (NANDSeekAsync(&NandInfo, sizeof(DVDErrorInfo), 0, cbForNandSeek2, &NandCb) != 0) {
            cbForNandSeek2(-1, NULL);
        }
    } else {
        __ErrorInfo.nextOffset = 1;
        if (NANDWriteAsync(&NandInfo, (void*)&__ErrorInfo, sizeof(__ErrorInfo), cbForNandWrite, &NandCb) != 0) {
            cbForNandWrite(-1, NULL);
        }
    }
} // 0x80224D40

/*
 * cbForNandSeek0
 *
 * Positioned at the file start: a new log begins at slot 0 with a fresh header.
 */
static void cbForNandSeek0(s32 result, NANDCommandBlock* block) {
    if (result == 0) {
        NextOffset = 0;
        __ErrorInfo.nextOffset = 1;

        if (NANDWriteAsync(&NandInfo, (void*)&__FirstErrorInfo, sizeof(__FirstErrorInfo), cbForNandWrite0, &NandCb) != 0) {
            cbForNandWrite0(-1, NULL);
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80224E30

/*
 * cbForNandSeek1
 *
 * Positioned at the header: reads it, or starts a new log when the seek failed.
 */
static void cbForNandSeek1(s32 result, NANDCommandBlock* block) {
    if (result == sizeof(DVDErrorInfo)) {
        if (NANDReadAsync(&NandInfo, (void*)&__FirstErrorInfo, sizeof(__FirstErrorInfo), cbForNandRead, &NandCb) != 0) {
            cbForNandRead(-1, NULL);
        }
    } else {
        if (NANDSeekAsync(&NandInfo, 0, 0, cbForNandSeek0, &NandCb) != 0) {
            cbForNandSeek0(-1, NULL);
        }
    }
} // 0x80224ED0

/*
 * cbForNandOpen
 *
 * The file is open: an existing log is read back, a new one gets a fresh header.
 */
static void cbForNandOpen(s32 result, NANDCommandBlock* block) {
    if (result == 0) {
        if (ExistFlag) {
            if (NANDSeekAsync(&NandInfo, sizeof(DVDErrorInfo), 0, cbForNandSeek1, &NandCb) != 0) {
                cbForNandSeek1(-1, NULL);
            }
        } else {
            NextOffset = 0;
            __ErrorInfo.nextOffset = 1;
            if (NANDWriteAsync(&NandInfo, (void*)&__FirstErrorInfo, sizeof(__FirstErrorInfo), cbForNandWrite0, &NandCb) != 0) {
                cbForNandWrite0(-1, NULL);
            }
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80224FD0

/*
 * cbForNandCreate
 *
 * The file exists or was created: remembers which, then opens it.
 */
static void cbForNandCreate(s32 result, NANDCommandBlock* block) {
    if (result == 0 || result == NAND_RESULT_EXISTS) {
        if (result == NAND_RESULT_EXISTS) {
            ExistFlag = TRUE;
        }

        if (NANDPrivateOpenAsync("/shared2/test2/dvderror.dat", &NandInfo, 3, cbForNandOpen, &NandCb) != 0) {
            if (Callback) {
                Callback(2, NULL);
            }
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x802250F0

/*
 * cbForNandCreateDir
 *
 * The directory exists or was created: creates the log file.
 */
static void cbForNandCreateDir(s32 result, NANDCommandBlock* block) {
    if (result == 0 || result == NAND_RESULT_EXISTS) {
        if (NANDPrivateCreateAsync("/shared2/test2/dvderror.dat", NAND_PERM_RWALL, 0, cbForNandCreate, &NandCb) != 0) {
            if (Callback) {
                Callback(2, NULL);
            }
        }
    } else {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80225190

/*
 * cbForPrepareControlRegister
 *
 * Stores the DVD control register in the record (all ones when the read failed), then creates the directory.
 */
static void cbForPrepareControlRegister(u32 intType) {
    if (intType == 1) {
        __ErrorInfo.control = DVDLowGetControlRegister();
    } else {
        __ErrorInfo.control = DVD_REGISTER_UNKNOWN;
    }

    if (NANDPrivateCreateDirAsync("/shared2/test2", NAND_PERM_RWALL, 0, cbForNandCreateDir, &NandCb) != 0) {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x80225230

/*
 * cbForPrepareStatusRegister
 *
 * Stores the DVD status register in the record (all ones when the read failed), then reads the control register.
 */
static void cbForPrepareStatusRegister(u32 intType) {
    if (intType == 1) {
        __ErrorInfo.status = DVDLowGetStatusRegister();
    } else {
        __ErrorInfo.status = DVD_REGISTER_UNKNOWN;
    }

    if (DVDLowPrepareControlRegister(cbForPrepareControlRegister) == 0) {
        if (Callback) {
            Callback(2, NULL);
        }
    }
} // 0x802252C0

/*
 * __DVDStoreErrorCode
 *
 * Starts the chain: records the error and its time, then reads the status and control registers before the NAND
 * file is touched.
 */
void __DVDStoreErrorCode(u32 error, DVDCBCallback callback) {
    __ErrorInfo.error = error;
    __ErrorInfo.dateTime = (u32)OSTicksToSeconds(OSGetTime());
    Callback = callback;
    DVDLowPrepareStatusRegister(cbForPrepareStatusRegister);
} // 0x80225340
