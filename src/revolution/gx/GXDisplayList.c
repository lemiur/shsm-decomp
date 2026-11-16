#include <string.h>

#include <revolution/gx.h>
#include <revolution/os.h>

#include <revolution/gx/__gx.h>

static __GXFifoObj DisplayListFifo;
static __GXFifoObj OldCPUFifo;
static GXData __savedGXdata;

// Starts recording GX commands into the list buffer.
void GXBeginDisplayList(void* list, u32 size) {
    CHECK_GXBEGIN("GXBeginDisplayList");
    ASSERTMSG(!__GXData->inDispList, "GXBeginDisplayList: display list already in progress");
    ASSERTMSG((size & 0x1F) == 0, "GXBeginDisplayList: size is not 32 byte aligned");
    ASSERTMSG(((u32)list & 0x1F) == 0, "GXBeginDisplayList: list is not 32 byte aligned");

    if (__GXData->dirtyState != 0) {
        __GXSetDirtyState();
    }

    if (__GXData->dlSaveContext != 0) {
        memcpy(&__savedGXdata, __GXData, sizeof(__savedGXdata));
    }

    DisplayListFifo.base = (u8*)list;
    DisplayListFifo.top = (u8*)list + size - 4;
    DisplayListFifo.size = size;
    DisplayListFifo.count = 0;
    DisplayListFifo.rdPtr = list;
    DisplayListFifo.wrPtr = list;
    __GXData->inDispList = 1;
    GXGetCPUFifo((GXFifoObj*)&OldCPUFifo);
    GXSetCPUFifo((GXFifoObj*)&DisplayListFifo);
    GXResetWriteGatherPipe();
}

// Ends recording; returns the list size in bytes, or 0 if the buffer overflowed.
u32 GXEndDisplayList(void) {
    u32 ov;
    u32 reg;
    BOOL enabled;
    u32 cpenable;

    CHECK_GXBEGIN("GXEndDisplayList");
    ASSERTMSG(__GXData->inDispList == TRUE, "GXEndDisplayList: no display list in progress");
    GXFlush();
    reg = GX_GET_PI_REG(5);
    ov = (reg >> 26) & 1;
    GXGetCPUFifo((GXFifoObj*)&DisplayListFifo);
    ASSERTMSG(!ov, "GXEndDisplayList: display list commands overflowed buffer");
    GXSetCPUFifo((GXFifoObj*)&OldCPUFifo);

    if (__GXData->dlSaveContext != 0) {
        enabled = OSDisableInterrupts();
        cpenable = __GXData->cpEnable;
        memcpy(__GXData, &__savedGXdata, sizeof(*__GXData));
        __GXData->cpEnable = cpenable;
        OSRestoreInterrupts(enabled);
    }

    __GXData->inDispList = 0;
    if (!ov) {
        return DisplayListFifo.count;
    }

    return 0;
}

// Sends a recorded display list to the GP.
// 0x8023E1E0
void GXCallDisplayList(void* list, u32 nbytes) {
    CHECK_GXBEGIN("GXCallDisplayList");
    ASSERTMSG(!__GXData->inDispList, "GXCallDisplayList: display list already in progress");
    ASSERTMSG((nbytes & 0x1F) == 0, "GXCallDisplayList: nbytes is not 32 byte aligned");
    ASSERTMSG(((u32)list & 0x1F) == 0, "GXCallDisplayList: list is not 32 byte aligned");

    if (__GXData->dirtyState != 0) {
        __GXSetDirtyState();
    }

#if DEBUG
    __GXShadowDispList(list, nbytes);
#endif

    if (*(u32*)&__GXData->vNumNot == 0) {  // checks both vNum and bpSent
        __GXSendFlushPrim();
    }

    GX_WRITE_U8(GX_CMD_CALL_DL);
    GX_WRITE_U32(list);
    GX_WRITE_U32(nbytes);
}
