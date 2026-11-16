#ifndef REVOLUTION_EXI_H
#define REVOLUTION_EXI_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OS.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Device IDs read back by EXIGetID. */
#define EXI_MEMORY_CARD_59   0x00000004
#define EXI_MEMORY_CARD_123  0x00000008
#define EXI_MEMORY_CARD_251  0x00000010
#define EXI_MEMORY_CARD_507  0x00000020
#define EXI_USB_ADAPTER      0x01010000
#define EXI_NPDP_GDEV        0x01020000
#define EXI_MODEM            0x02020000
#define EXI_MARLIN           0x03010000
#define EXI_ETHER            0x04020200
#define EXI_RS232C           0x04040404
#define EXI_MIC              0x04060000
#define EXI_AD16             0x04120000
#define EXI_STREAM_HANGER    0x04130000
#define EXI_ETHER_VIEWER     0x04220001
#define EXI_IS_VIEWER        0x05070000

/* EXIImm and EXIDma transfer types. */
#define EXI_READ   0
#define EXI_WRITE  1

/* Clock selections of EXISelect. */
#define EXI_FREQ_1M  0
#define EXI_FREQ_2M  1
#define EXI_FREQ_4M  2
#define EXI_FREQ_8M  3
#define EXI_FREQ_16M 4
#define EXI_FREQ_32M 5

typedef void (*EXICallback)(s32 chan, OSContext* context);

typedef struct EXIControl {
    EXICallback exiCallback;
    EXICallback tcCallback;
    EXICallback extCallback;
    volatile u32 state; // volatile: updated from the EXI interrupt handlers
    int immLen;
    u8* immBuf;
    u32 dev;
    u32 id;
    s32 idTime;
    int items;
    struct {
        u32 dev;
        EXICallback callback;
    } queue[3];
} EXIControl;

void EXIInit(void);
BOOL EXILock(s32 chan, u32 dev, EXICallback unlockedCallback);
BOOL EXIUnlock(s32 chan);
BOOL EXISelect(s32 chan, u32 dev, u32 freq);
BOOL EXIDeselect(s32 chan);
BOOL EXIImm(s32 chan, void* buf, s32 len, u32 type, EXICallback callback);
BOOL EXIImmEx(s32 chan, void* buf, s32 len, u32 type);
BOOL EXIDma(s32 chan, void* buf, s32 len, u32 type, EXICallback callback);
BOOL EXISync(s32 chan);
BOOL EXIProbe(s32 chan);
BOOL EXIAttach(s32 chan, EXICallback extCallback);
BOOL EXIDetach(s32 chan);
u32 EXIClearInterrupts(s32 chan, int exi, int tc, int ext);
EXICallback EXISetExiCallback(s32 chan, EXICallback exiCallback);
s32 EXIGetID(s32 chan, u32 dev, u32* id);
void EXIProbeReset(void);

/* EXIUart.c */
void __OSEnableBarnacle(s32 chan, u32 dev);

/* EXICommon.c */
BOOL EXIWriteReg(s32 chan, u32 dev, u32 exiCmd, void* reg, s32 size);

#ifdef __cplusplus
}
#endif

#endif
