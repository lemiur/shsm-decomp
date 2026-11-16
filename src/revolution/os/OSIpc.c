// Reference: zeldaret/tp libs/revolution/src/os/OSIpc.c
#include <revolution/os.h>

/* Low-memory block where the boot code publishes the IPC buffer bounds. */
#define OS_IPC_BUFFER_LO_ADDR 0x3130
#define OS_IPC_BUFFER_HI_ADDR 0x3134

/* Bounds of the MEM2 area reserved for IPC with Starlet, read from low memory by __OSInitIPCBuffer. */
static void* IpcBufferHi;
static void* IpcBufferLo = (void*)0xFFFFFFFF;

/*
 * __OSGetIPCBufferHi
 *
 * Returns the end of the IPC buffer area.
 */
void* __OSGetIPCBufferHi(void) {
    return IpcBufferHi;
} // 0x8021C680

/*
 * __OSGetIPCBufferLo
 *
 * Returns the start of the IPC buffer area (0xFFFFFFFF until initialised).
 */
void* __OSGetIPCBufferLo(void) {
    return IpcBufferLo;
} // 0x8021C690

/*
 * __OSInitIPCBuffer
 *
 * Reads the IPC buffer bounds the boot code left in low memory.
 */
void __OSInitIPCBuffer(void) {
    IpcBufferLo = (void*)*(u32*)OSPhysicalToCached(OS_IPC_BUFFER_LO_ADDR);
    IpcBufferHi = (void*)*(u32*)OSPhysicalToCached(OS_IPC_BUFFER_HI_ADDR);
} // 0x8021C6A0
