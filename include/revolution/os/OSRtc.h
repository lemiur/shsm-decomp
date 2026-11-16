#ifndef REVOLUTION_OS_OSRTC_H
#define REVOLUTION_OS_OSRTC_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The first 20 bytes of the console's SRAM (the part the IPL maintains). */
typedef struct OSSram {
    u16 checkSum;
    u16 checkSumInv;
    u32 ead0;
    u32 ead1;
    u32 counterBias;
    s8 displayOffsetH;
    u8 ntd;
    u8 language;
    u8 flags;
} OSSram;

/* The extended SRAM that follows OSSram. */
typedef struct OSSramEx {
    u8 flashID[2][12];
    u32 wirelessKeyboardID;
    u16 wirelessPadID[4];
    u8 dvdErrorCode;
    u8 padding0; // pad: SDK OSSramEx.padding0
    u8 flashIDCheckSum[2];
    u16 gbs;
    u8 padding1[2]; // pad: SDK OSSramEx.padding1
} OSSramEx;

void __OSInitSram(void);
OSSram* __OSLockSram(void);
OSSramEx* __OSLockSramEx(void);
BOOL __OSUnlockSram(BOOL commit);
BOOL __OSUnlockSramEx(BOOL commit);
BOOL __OSSyncSram(void);
BOOL __OSReadROM(void* buffer, s32 length, s32 offset);

u16 OSGetGbsMode(void);
void OSSetGbsMode(u16 mode);

/* Per-channel wireless controller IDs kept in the extended SRAM. */
u16 OSGetWirelessID(s32 chan);
void OSSetWirelessID(s32 chan, u16 id);

BOOL __OSGetRTCFlags(u32* flags);
BOOL __OSClearRTCFlags(void);

#ifdef __cplusplus
}
#endif

#endif
