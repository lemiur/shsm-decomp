#ifndef REVOLUTION_AI_H
#define REVOLUTION_AI_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audio interface. */

/* Audio DMA block size: the DMA length is programmed in these units. */
#define AI_DMA_BLOCK_SIZE 32

/* DSP sample rates for AISetDSPSampleRate. */
#define AI_SAMPLERATE_48KHZ 1
#define AI_SAMPLERATE_32KHZ 0

typedef void (*AIDCallback)(void);

AIDCallback AIRegisterDMACallback(AIDCallback callback);
void AIInitDMA(u32 addr, u32 length);
BOOL AIGetDMAEnableFlag(void);
void AIStartDMA(void);
void AIStopDMA(void);
u32 AIGetDMABytesLeft(void);
u32 AIGetDMAStartAddr(void);
u32 AIGetDMALength(void);
BOOL AICheckInit(void);
void AISetDSPSampleRate(u32 rate);
u32 AIGetDSPSampleRate(void);
void AIInit(u8* stack);
void AIReset(void);

#ifdef __cplusplus
}
#endif

#endif
