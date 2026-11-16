#include <revolution/ipc.h>
#include <revolution/os.h>

static void* IPCBufferHi;
static void* IPCBufferLo;
static void* IPCCurrentBufferHi;
static void* IPCCurrentBufferLo;
static u8 Initialized;

/*
 * IPCInit
 *
 * Takes the IPC buffer bounds from OS on first use.
 */
void IPCInit(void) {
    if (!Initialized) {
        IPCBufferHi = __OSGetIPCBufferHi();
        IPCBufferLo = __OSGetIPCBufferLo();
        IPCCurrentBufferHi = IPCBufferHi;
        IPCCurrentBufferLo = IPCBufferLo;
        Initialized = TRUE;
    }
} // 0x80241B80

/*
 * IPCReInit
 *
 * Unconditionally resets the IPC buffer to its full OS bounds.
 */
void IPCReInit(void) {
    Initialized = FALSE;
    IPCBufferHi = __OSGetIPCBufferHi();
    IPCBufferLo = __OSGetIPCBufferLo();
    IPCCurrentBufferHi = IPCBufferHi;
    IPCCurrentBufferLo = IPCBufferLo;
    Initialized = TRUE;
} // 0x80241BD0

u32 IPCReadReg(u32 regIdx) {
    return __IPCRegs[regIdx];
} // 0x80241C20

void IPCWriteReg(u32 regIdx, u32 data) {
    __IPCRegs[regIdx] = data;
} // 0x80241C30

void* IPCGetBufferHi(void) {
    return IPCCurrentBufferHi;
} // 0x80241C40

void* IPCGetBufferLo(void) {
    return IPCCurrentBufferLo;
} // 0x80241C50

void IPCSetBufferLo(void* newLo) {
    IPCCurrentBufferLo = newLo;
} // 0x80241C60
