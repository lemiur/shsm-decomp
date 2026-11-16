// Reference: zeldaret/tp libs/revolution/src/os/OSPlayRecord.c
#include <revolution/os.h>
#include <revolution/nand.h>

/*
 * Play record state machine. The state is advanced by PlayRecordCallback, which is the completion callback of
 * every asynchronous NAND call. States, as observed in the code (names invented; the SDK uses bare numbers):
 *   0 start      1 opening play_rec.dat   2 reading the record   3 seeking to the start
 *   4 waiting for the next save (60 s alarm)   5 writing the record   6 closing the file
 *   7 failed     8 stop timed out         9 not running
 */

static BOOL PlayRecordGet = FALSE;
static OSPlayRecord PlayRecord __attribute__((aligned(32)));
static NANDFileInfo FileInfo;
static NANDCommandBlock Block;
static s32 PlayRecordState = 9;
static OSAlarm PlayRecordAlarm;
static BOOL PlayRecordError = FALSE;
// volatile: written by __OSStopPlayRecord, read from the NAND completion callback
static volatile BOOL PlayRecordTerminate = FALSE;
// volatile: written by the NAND completion callback, polled by __OSStopPlayRecord
static volatile BOOL PlayRecordTerminated = FALSE;
static BOOL PlayRecordRetry = FALSE;
static OSTime PlayRecordLastCloseTime;
static s32 PlayRecordLastError = NAND_RESULT_OK;

static void PlayRecordCallback(s32, NANDCommandBlock *);

/*
 * RecordCheckSum
 *
 * Sums the words of PlayRecord that follow the checksum field. Both callers expand it inline and retail has no
 * out-of-line copy (the linker stripped it); being public, the compiler translates it where it is defined, which is
 * what gives PlayRecord the first .bss address (statics are laid out in order of first use).
 */
u32 RecordCheckSum(void) {
    u32 *ptr, i, sum;
    ptr = (u32*)PlayRecord.titleName;
    sum = 0;

    for (i = 0; i < (sizeof(PlayRecord) - 4) / 4; i++) {
        sum = sum + *ptr;
        ptr++;
    }

    return sum;
}

/*
 * PlayRecordAlarmCallback
 *
 * Alarm handler: resumes the state machine after a wait or a retry delay.
 */
void PlayRecordAlarmCallback(OSAlarm* alarm, OSContext* context) {
    PlayRecordCallback(NAND_RESULT_OK, NULL);
} // 0x8021CDD0

/*
 * PlayRecordCallback
 *
 * Handles the result of the NAND call for the current state, picks the next state and starts its NAND call.
 * A busy NAND (or a full file table when opening) is retried from an alarm one second later.
 */
static void PlayRecordCallback(s32 result, NANDCommandBlock* block) {
    s32 ret = 0;
    PlayRecordLastError = result;

    if (PlayRecordTerminate) {
        PlayRecordTerminated = TRUE;
        return;
    }

    if (!PlayRecordRetry) {
        switch (PlayRecordState) {
        case 0:
            PlayRecordState = 1;
            break;
        case 1:
            if (result == NAND_RESULT_MAXFD) {
                PlayRecordRetry = TRUE;
                OSCreateAlarm(&PlayRecordAlarm);
                OSSetAlarm(&PlayRecordAlarm, (OSTime)1 * OSSecondsToTicks(1), PlayRecordAlarmCallback);
                return;
            }
            else if (result == NAND_RESULT_OK) {
                if (!PlayRecordGet) {
                    PlayRecordState = 2;
                }
                else {
                    PlayRecordState = 4;
                }
                break;
            }
            else {
                PlayRecordError = TRUE;
                PlayRecordState = 7;
                return;
            }
        case 2:
            if (result == sizeof(OSPlayRecord)) {
                PlayRecordGet = TRUE;
                PlayRecordLastCloseTime = PlayRecord.playTime;
                PlayRecordState = 3;
            }
            else {
                PlayRecordError = TRUE;
                PlayRecordState = 6;
            }
            break;
        case 3:
            if (result == 0) {
                PlayRecordState = 4;
            }
            else {
                PlayRecordError = TRUE;
                PlayRecordState = 6;
            }
            break;
        case 4:
            PlayRecordState = 5;
            break;
        case 5:
            if (result == sizeof(OSPlayRecord)) {
                if (OSGetTime() - PlayRecordLastCloseTime > (OSTime)300 * OSSecondsToTicks(1)) {
                    PlayRecordState = 6;
                }
                else {
                    PlayRecordState = 3;
                }
            }
            else {
                PlayRecordError = TRUE;
                PlayRecordState = 6;
            }
            break;
        case 6:
            if (PlayRecordError) {
                PlayRecordState = 7;
                return;
            }
            else {
                if (result == NAND_RESULT_OK) {
                    PlayRecordLastCloseTime = PlayRecord.playTime;
                    PlayRecordState = 1;
                    break;
                }
                else {
                    PlayRecordState = 7;
                    PlayRecordError = TRUE;
                    return;
                }
            }
        default:
            PlayRecordState = 7;
            PlayRecordError = TRUE;
            return;
        }
    }

    PlayRecordRetry = FALSE;

    switch (PlayRecordState) {
    case 1:
        ret = NANDOpenAsync("/title/00000001/00000002/data/play_rec.dat", &FileInfo, 3, PlayRecordCallback, &Block);
        break;
    case 2:
        ret = NANDReadAsync(&FileInfo, &PlayRecord, sizeof(OSPlayRecord), PlayRecordCallback, &Block);
        break;
    case 3:
        ret = NANDSeekAsync(&FileInfo, 0, 0, PlayRecordCallback, &Block);
        break;
    case 4:
        OSCreateAlarm(&PlayRecordAlarm);
        OSSetAlarm(&PlayRecordAlarm, (OSTime)60 * OSSecondsToTicks(1), PlayRecordAlarmCallback);
        break;
    case 5:
        PlayRecord.playTime = OSGetTime();
        PlayRecord.checkSum = RecordCheckSum();
        ret = NANDWriteAsync(&FileInfo, &PlayRecord, sizeof(OSPlayRecord), PlayRecordCallback, &Block);
        break;
    case 6:
        ret = NANDCloseAsync(&FileInfo, PlayRecordCallback, &Block);
        break;
    }

    if (ret != NAND_RESULT_OK) {
        if (ret == NAND_RESULT_BUSY) {
            OSCreateAlarm(&PlayRecordAlarm);
            OSSetAlarm(&PlayRecordAlarm, (OSTime)1 * OSSecondsToTicks(1), PlayRecordAlarmCallback);
            PlayRecordRetry = TRUE;
        }
        else {
            PlayRecordError = TRUE;
            switch (PlayRecordState) {
            case 2:
            case 3:
            case 5:
                PlayRecordState = 6;
                ret = NANDCloseAsync(&FileInfo, PlayRecordCallback, &Block);

                if (ret == NAND_RESULT_BUSY) {
                    PlayRecordRetry = TRUE;
                    OSCreateAlarm(&PlayRecordAlarm);
                    OSSetAlarm(&PlayRecordAlarm, 1 * OSSecondsToTicks(1), PlayRecordAlarmCallback);
                }
                break;
            default:
                PlayRecordState = 7;
            }
        }
    }

    PlayRecordLastError = ret;
} // 0x8021CDE0

/*
 * __OSStartPlayRecord
 *
 * Starts the play record updater, which saves the play time to play_rec.dat once a minute.
 */
void __OSStartPlayRecord(void) {
    if (NANDInit() == NAND_RESULT_OK) {
        PlayRecordGet = FALSE;
        PlayRecordState = 0;
        PlayRecordError = FALSE;
        PlayRecordRetry = FALSE;
        PlayRecordTerminate = FALSE;
        PlayRecordTerminated = FALSE;
        PlayRecordLastError = NAND_RESULT_OK;
        PlayRecordCallback(NAND_RESULT_OK, NULL);
    }
} // 0x8021D2A0

/*
 * __OSStopPlayRecord
 *
 * Stops the updater, waiting up to 500 ms for a call in flight, and closes the file.
 */
void __OSStopPlayRecord(void) {
    BOOL old;
    OSTime start;

    old = OSDisableInterrupts();

    PlayRecordTerminate = TRUE;
    
    if (PlayRecordState == 7 || PlayRecordState == 0 || 
        PlayRecordState == 9 || PlayRecordState == 8)
    {
        OSRestoreInterrupts(old);
    }
    else if (PlayRecordState == 4) {
        OSCancelAlarm(&PlayRecordAlarm);
        OSRestoreInterrupts(old);
        PlayRecord.playTime = OSGetTime();
        PlayRecord.checkSum = RecordCheckSum();
        NANDWrite(&FileInfo, &PlayRecord, sizeof(PlayRecord));
        NANDClose(&FileInfo);
    } else {
        if (PlayRecordRetry) {
            OSCancelAlarm(&PlayRecordAlarm);
            OSRestoreInterrupts(old);
        } else {
            OSRestoreInterrupts(old);
            start = OSGetTime();
            while (1) {
                if (PlayRecordTerminated) {
                    break;
                }
                if ((OSGetTime() - start) > OSMillisecondsToTicks(500)) {
                    PlayRecordState = 8;
                    return;
                }
            }
        }

        switch (PlayRecordState) {
        case 2:
        case 3:
        case 5:
            NANDClose(&FileInfo);
            break;
        case 1:
            if (PlayRecordLastError == NAND_RESULT_OK && !PlayRecordRetry) {
                NANDClose(&FileInfo);
            }
            break;
        case 6:
            if (PlayRecordRetry) {
                NANDClose(&FileInfo);
            }
            break;
        }
    }
    
    PlayRecordState = 9;
} // 0x8021D300
