#ifndef _REVOLUTION_GX_GXFIFO_H_
#define _REVOLUTION_GX_GXFIFO_H_

#include <revolution/gx/GXEnum.h>
#include <revolution/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u8 pad[128];  // pad: opaque SDK GXFifoObj, 128 bytes
} GXFifoObj;

typedef void (*GXBreakPtCallback)(void);

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size);
void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr);
void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark);
void GXSetCPUFifo(const GXFifoObj* fifo);
void GXSetGPFifo(const GXFifoObj* fifo);
void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt);
void GXGetFifoStatus(GXFifoObj* fifo, GXBool* overhi, GXBool* underflow, u32* fifoCount, GXBool* cpuWrite, GXBool* gpRead, GXBool* fifowrap);
void GXGetFifoPtrs(GXFifoObj* fifo, void** readPtr, void** writePtr);
void* GXGetFifoBase(const GXFifoObj* fifo);
u32 GXGetFifoSize(const GXFifoObj* fifo);
void GXGetFifoLimits(const GXFifoObj* fifo, u32* hi, u32* lo);
GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb);
void GXEnableBreakPt(void* break_pt);
void GXDisableBreakPt(void);
OSThread* GXSetCurrentGXThread(void);
OSThread* GXGetCurrentGXThread(void);
BOOL GXGetCPUFifo(GXFifoObj* fifo);
BOOL GXGetGPFifo(GXFifoObj* fifo);
u32 GXGetOverflowCount(void);
u32 GXResetOverflowCount(void);
// volatile: the write-gather pipe port is a hardware address
volatile void* GXRedirectWriteGatherPipe(void* ptr);
void GXRestoreWriteGatherPipe(void);

#ifdef __cplusplus
}
#endif

#endif
