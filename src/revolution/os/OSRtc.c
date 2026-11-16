// Reference: zeldaret/tp libs/revolution/src/os/OSRtc.c
#include <revolution/os.h>
#include <revolution/exi.h>

/* The SRAM on the RTC chip is 64 bytes; the chip is addressed in 64-byte units. */
#define SRAM_SIZE 64
#define SRAM_CHECKED_END 0x14

/* EXI channel and device of the RTC/SRAM chip. */
#define RTC_CHAN 0
#define RTC_DEV 1

/* RTC chip commands. */
#define RTC_CMD_WRITE 0xA0000000
#define RTC_CMD_SRAM 0x00000100
#define RTC_CMD_SRAM_READ 0x20000100
#define RTC_CMD_FLAGS_READ 0x21000800
#define RTC_CMD_FLAGS_WRITE 0xA1000800

/* GBS (Game Boy Advance link) mode values the SRAM may not hold: bits 10-14 equal 0x14, or bits 6-7 both set. */
#define GBS_MODE_INVALID(mode) ((((u32)(mode) & 0x7c00) == 0x5000) || (((u32)(mode) & 0xc0) == 0xc0))

typedef struct SramControl {
    u8 sram[SRAM_SIZE];
    u32 offset;   /* first byte that has changed and still has to be written back */
    BOOL enabled; /* interrupt state to restore on unlock */
    BOOL locked;
    BOOL sync;    /* TRUE when the last write-back (or read) succeeded */
    void (*callback)();
} SramControl;

static SramControl Scb __attribute__((aligned(32)));

static BOOL ReadSram(void* buffer);
static void WriteSramCallback(s32 chan, OSContext* context);
static BOOL WriteSram(void* buffer, u32 offset, u32 size);
static void* LockSram(u32 offset);
static BOOL UnlockSram(BOOL commit, u32 offset);

/*
 * ReadSram
 *
 * Reads the whole SRAM into buffer.
 */
static BOOL ReadSram(void* buffer) {
    int err;
    u32 cmd;

    DCInvalidateRange(buffer, SRAM_SIZE);
    if (!EXILock(RTC_CHAN, RTC_DEV, NULL)) {
        return FALSE;
    }
    if (!EXISelect(RTC_CHAN, RTC_DEV, EXI_FREQ_8M)) {
        EXIUnlock(RTC_CHAN);
        return FALSE;
    }
    cmd = RTC_CMD_SRAM_READ;
    err = 0;
    err |= !EXIImm(RTC_CHAN, &cmd, 4, EXI_WRITE, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIDma(RTC_CHAN, buffer, SRAM_SIZE, EXI_READ, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIDeselect(RTC_CHAN);
    EXIUnlock(RTC_CHAN);
    return !err;
}

/*
 * WriteSramCallback
 *
 * Called when the EXI channel becomes free again after a write-back found it
 * busy: writes back what is still pending.
 */
static void WriteSramCallback(s32 chan, OSContext* context) {
    Scb.sync = WriteSram(&Scb.sram[Scb.offset], Scb.offset, SRAM_SIZE - Scb.offset);
    if (Scb.sync != 0) {
        Scb.offset = SRAM_SIZE;
    }
} // 0x80219530

/*
 * WriteSram
 *
 * Writes size bytes of buffer to the SRAM at offset. If the channel is
 * busy, WriteSramCallback finishes the job later.
 */
static BOOL WriteSram(void* buffer, u32 offset, u32 size) {
    int err;
    u32 cmd;

    if (!EXILock(RTC_CHAN, RTC_DEV, WriteSramCallback)) {
        return FALSE;
    }
    if (!EXISelect(RTC_CHAN, RTC_DEV, EXI_FREQ_8M)) {
        EXIUnlock(RTC_CHAN);
        return FALSE;
    }
    offset <<= 6;
    cmd = ((offset + RTC_CMD_SRAM) | RTC_CMD_WRITE);
    err = 0;
    err |= !EXIImm(RTC_CHAN, &cmd, 4, EXI_WRITE, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIImmEx(RTC_CHAN, buffer, size, EXI_WRITE);
    err |= !EXIDeselect(RTC_CHAN);
    EXIUnlock(RTC_CHAN);
    return !err;
}

/*
 * __OSInitSram
 *
 * Reads the SRAM into the control block and normalises the GBS mode.
 */
void __OSInitSram(void) {
    Scb.locked = Scb.enabled = FALSE;
    Scb.sync = ReadSram(&Scb);
    Scb.offset = SRAM_SIZE;

    OSSetGbsMode(OSGetGbsMode());
} // 0x80219670

/*
 * LockSram
 *
 * Disables interrupts and returns the SRAM bytes from offset on, or NULL if
 * the SRAM is already locked.
 */
static void* LockSram(u32 offset) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    if (Scb.locked) {
        OSRestoreInterrupts(enabled);
        return NULL;
    }
    Scb.enabled = enabled;
    Scb.locked = TRUE;
    return &Scb.sram[offset];
}

/*
 * __OSLockSram
 *
 * Locks the SRAM and returns its first 20 bytes.
 */
OSSram* __OSLockSram(void) {
    return (OSSram*)LockSram(0);
}

/*
 * __OSLockSramEx
 *
 * Locks the SRAM and returns the extended part.
 */
OSSramEx* __OSLockSramEx(void) {
    return (OSSramEx*)LockSram(sizeof(OSSram));
}

/*
 * UnlockSram
 *
 * Unlocks the SRAM and restores interrupts. With commit set, refreshes the
 * checksum when the first 20 bytes were open and writes back everything from
 * the lowest changed offset on. Returns whether the SRAM is in sync.
 */
static BOOL UnlockSram(BOOL commit, u32 offset) {
    u16* p;

    if (commit != 0) {
        if (offset == 0) {
            OSSram* sram = (OSSram*)Scb.sram;
            if (2u < (sram->flags & 3)) {
                sram->flags &= ~3;
            }

            sram->checkSum = sram->checkSumInv = 0;
            for (p = (u16*)&sram->counterBias; p < ((u16*)&Scb.sram[SRAM_CHECKED_END]); p++) {
                sram->checkSum += *p;
                sram->checkSumInv += ~(*p);
            }
        }
        if (offset < Scb.offset) {
            Scb.offset = offset;
        }

        if (Scb.offset <= SRAM_CHECKED_END) {
            OSSramEx* sram = (OSSramEx*)(Scb.sram + sizeof(OSSram));
            if (GBS_MODE_INVALID(sram->gbs)) {
                sram->gbs = 0;
            }
        }

        Scb.sync = WriteSram(&Scb.sram[Scb.offset], Scb.offset, SRAM_SIZE - Scb.offset);
        if (Scb.sync != 0) {
            Scb.offset = SRAM_SIZE;
        }
    }
    Scb.locked = FALSE;
    OSRestoreInterrupts(Scb.enabled);
    return Scb.sync;
} // 0x80219870

/*
 * __OSUnlockSram
 */
BOOL __OSUnlockSram(BOOL commit) {
    return UnlockSram(commit, 0);
}

/*
 * __OSUnlockSramEx
 */
BOOL __OSUnlockSramEx(BOOL commit) {
    return UnlockSram(commit, sizeof(OSSram));
}

/*
 * __OSSyncSram
 *
 * TRUE when everything written to the SRAM has reached the chip.
 */
BOOL __OSSyncSram(void) {
    return Scb.sync;
} // 0x80219B50

/*
 * __OSReadROM
 *
 * Reads length bytes of the RTC chip's ROM, starting at offset, into buffer.
 */
BOOL __OSReadROM(void* buffer, s32 length, s32 offset) {
    int err;
    u32 cmd;

    DCInvalidateRange(buffer, length);
    if (EXILock(RTC_CHAN, RTC_DEV, NULL) == 0) {
        return FALSE;
    }
    if (EXISelect(RTC_CHAN, RTC_DEV, EXI_FREQ_8M) == 0) {
        EXIUnlock(RTC_CHAN);
        return FALSE;
    }
    cmd = offset << 6;
    err = 0;
    err |= !EXIImm(RTC_CHAN, &cmd, 4, EXI_WRITE, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIDma(RTC_CHAN, buffer, length, EXI_READ, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIDeselect(RTC_CHAN);
    EXIUnlock(RTC_CHAN);
    return !err;
} // 0x80219B60

/*
 * OSGetWirelessID
 *
 * Returns the ID of the wireless controller last paired with channel chan.
 */
u16 OSGetWirelessID(s32 chan) {
    OSSramEx* sram;
    u16 id;

    sram = __OSLockSramEx();
    id = sram->wirelessPadID[chan];
    __OSUnlockSramEx(FALSE);
    return id;
} // 0x80219C90

/*
 * OSSetWirelessID
 *
 * Records id as the wireless controller paired with channel chan.
 */
void OSSetWirelessID(s32 chan, u16 id) {
    OSSramEx* sram;

    sram = __OSLockSramEx();
    if (sram->wirelessPadID[chan] != id) {
        sram->wirelessPadID[chan] = id;
        __OSUnlockSramEx(TRUE);
        return;
    }

    __OSUnlockSramEx(FALSE);
} // 0x80219D10

/*
 * OSGetGbsMode
 */
u16 OSGetGbsMode(void) {
    OSSramEx* sram;
    u16 mode;

    sram = __OSLockSramEx();
    mode = sram->gbs;
    __OSUnlockSramEx(FALSE);
    return mode;
}

/*
 * OSSetGbsMode
 *
 * Invalid GBS modes are stored as 0.
 */
void OSSetGbsMode(u16 mode) {
    OSSramEx* sram;

    if (GBS_MODE_INVALID(mode)) {
        mode = 0;
    }

    sram = __OSLockSramEx();

    if (mode == sram->gbs) {
        __OSUnlockSramEx(FALSE);
        return;
    }
    sram->gbs = mode;

    __OSUnlockSramEx(TRUE);
}

/*
 * __OSGetRTCFlags
 *
 * Reads the RTC chip's flag word into *flags.
 */
BOOL __OSGetRTCFlags(u32* flags) {
    BOOL err;
    u32 cmd;

    if (!EXILock(RTC_CHAN, RTC_DEV, NULL)) {
        return FALSE;
    }

    if (!EXISelect(RTC_CHAN, RTC_DEV, EXI_FREQ_8M)) {
        EXIUnlock(RTC_CHAN);
        return FALSE;
    }

    cmd = RTC_CMD_FLAGS_READ;
    err = FALSE;
    err |= !EXIImm(RTC_CHAN, &cmd, 4, EXI_WRITE, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIImm(RTC_CHAN, &cmd, 4, EXI_READ, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIDeselect(RTC_CHAN);
    EXIUnlock(RTC_CHAN);
    *flags = cmd;
    return !err;
} // 0x80219DB0

/*
 * __OSClearRTCFlags
 *
 * Clears the RTC chip's flag word.
 */
BOOL __OSClearRTCFlags(void) {
    BOOL err;
    u32 cmd;
    u32 data = 0;

    if (!EXILock(RTC_CHAN, RTC_DEV, NULL)) {
        return FALSE;
    }

    if (!EXISelect(RTC_CHAN, RTC_DEV, EXI_FREQ_8M)) {
        EXIUnlock(RTC_CHAN);
        return FALSE;
    }

    cmd = RTC_CMD_FLAGS_WRITE;
    err = FALSE;
    err |= !EXIImm(RTC_CHAN, &cmd, 4, EXI_WRITE, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIImm(RTC_CHAN, &data, 4, EXI_WRITE, NULL);
    err |= !EXISync(RTC_CHAN);
    err |= !EXIDeselect(RTC_CHAN);
    EXIUnlock(RTC_CHAN);

    return !err;
} // 0x80219ED0
