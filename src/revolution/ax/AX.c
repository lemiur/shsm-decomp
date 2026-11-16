// Reference: doldecomp/ogws src/revolution/AX/AX.c (the init guard and AXQuit follow the later SDK, as in retail)
#include <revolution/ax.h>
#include <revolution/ax/__ax.h>
#include <revolution/os.h>

const char* __AXVersion = "<< RVL_SDK - AX \trelease build: Feb 27 2009 10:01:36 (0x4302_145) >>";

static BOOL __init = FALSE;

/*
 * AXInit
 *
 * Starts the AX library in the default output mode.
 */
void AXInit(void) {
    AXInitEx(0);
} // 0x8022D030

/*
 * AXInitEx
 *
 * Starts the AX library once: registers the version, then initialises the voice allocator, the voice and
 * sub-parameter blocks, the aux buses, the command list and the output.
 */
void AXInitEx(u32 mode) {
    if (!__init) {
        OSRegisterVersion(__AXVersion);

        __AXAllocInit();
        __AXVPBInit();
        __AXSPBInit();
        __AXAuxInit();
        __AXClInit();
        __AXOutInit(mode);

        __init = TRUE;
    }
}

/*
 * AXQuit
 *
 * Shuts the AX library down, in the reverse of the initialisation order.
 */
void AXQuit(void) {
    if (__init) {
        __AXOutQuit();
        __AXAllocQuit();
        __AXVPBQuit();
        __AXSPBQuit();
        __AXAuxQuit();
        __AXClQuit();

        __init = FALSE;
    }
} // 0x8022D090

/*
 * AXIsInit
 *
 * Returns whether the AX library is running.
 */
BOOL AXIsInit(void) {
    return __init;
} // 0x8022D0E0
