#ifndef REVOLUTION_OS_OSCACHE_H
#define REVOLUTION_OS_OSCACHE_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void DCEnable(void);
void DCInvalidateRange(void* addr, u32 nBytes);
void DCFlushRange(void* addr, u32 nBytes);
void DCStoreRange(void* addr, u32 nBytes);
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void DCZeroRange(void* addr, u32 nBytes);
void ICInvalidateRange(void* addr, u32 nBytes);
void ICFlashInvalidate(void);
void ICEnable(void);
void LCEnable(void);
void LCDisable(void);
void LCStoreBlocks(void* destAddr, void* srcTag, u32 numBlocks);
u32 LCStore(void* destAddr, void* srcAddr, u32 nBytes);
void LCQueueWait(u32 len);

void L2Enable(void);
void L2Disable(void);
void L2GlobalInvalidate(void);

#ifdef __cplusplus
}
#endif

#endif
