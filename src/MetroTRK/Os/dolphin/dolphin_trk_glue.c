// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk_glue.c
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/dolphin_trk_glue.c
// (the retail bodies are xenoblade's: NDEV only, no BBA or DDH connection)

#include "TRK/dolphin_trk_glue.h"
#include "TRK/GDEV_Stubs.h"
#include "TRK/mem_TRK.h"
#include "TRK/targimpl.h"
#include "TRK/uart.h"
#include <revolution/base/PPCArch.h>
#include <revolution/os/OSCache.h>
#include <revolution/os/OSError.h>
#include <revolution/os/OSThread.h>

u8 TRK_Use_BBA;
DBCommTable gDBCommTable;

// ORIGINAL-ASM: restores the interrupted context's registers and enters the nub's interrupt handler with the context's srr0/srr1 in r2/r4 and the vector in r3; an asm function with nofralloc in the public tp and xenoblade reconstructions
/* Re-enters the nub from an EXI interrupt as if the given exception had been taken in the interrupted context. */
asm void TRKLoadContext(OSContext* context, u32 exceptionVector) {
    nofralloc
    lwz r0, OSContext.gpr[0](r3)
    lwz r1, OSContext.gpr[1](r3)
    lwz r2, OSContext.gpr[2](r3)
    lhz r5, OSContext.state(r3)
    rlwinm. r6, r5, 0, 30, 30
    beq L_RestoreNonVolatile
    rlwinm r5, r5, 0, 31, 29
    sth r5, OSContext.state(r3)
    lmw r5, OSContext.gpr[5](r3)
    b L_Restored
L_RestoreNonVolatile:
    lmw r13, OSContext.gpr[13](r3)
L_Restored:
    mr r31, r3
    mr r3, r4
    lwz r4, OSContext.cr(r31)
    mtcrf 0xff, r4
    lwz r4, OSContext.lr(r31)
    mtlr r4
    lwz r4, OSContext.ctr(r31)
    mtctr r4
    lwz r4, OSContext.xer(r31)
    mtxer r4
    mfmsr r4
    rlwinm r4, r4, 0, 17, 15
    rlwinm r4, r4, 0, 31, 29
    mtmsr r4
    mtsprg 1, r2
    lwz r4, OSContext.gpr[3](r31)
    mtsprg 2, r4
    lwz r4, OSContext.gpr[4](r31)
    mtsprg 3, r4
    lwz r2, OSContext.srr0(r31)
    lwz r4, OSContext.srr1(r31)
    lwz r31, OSContext.gpr[31](r31)
    b TRKInterruptHandler
} // 0x80191904

/* EXI callback of the debugger connection: data arrived, so enter the nub as for an external interrupt. */
void TRKEXICallBack(s32 chan, OSContext* context) {
    OSEnableScheduler();
    TRKLoadContext(context, 0x500);
} // 0x8019198C

/* Installs the connection for the hardware the OS reports. Returns 1 when the ID is not one this build
 * supports. */
int InitMetroTRKCommTable(int hwId) {
    OSReport("Devkit set to : %ld\n", hwId);
    OSReport("MetroTRK : Sizeof Reply - %ld bytes\n", 0x40);
    TRK_Use_BBA = FALSE;

    if (hwId == HARDWARE_BBA) {
        return 0;
    } else if (hwId == HARDWARE_NDEV) {
        OSReport("MetroTRK : Set to NDEV hardware\n");
        gDBCommTable.initialize_func = gdev_cc_initialize;
        gDBCommTable.open_func = gdev_cc_open;
        gDBCommTable.close_func = gdev_cc_close;
        gDBCommTable.read_func = gdev_cc_read;
        gDBCommTable.write_func = gdev_cc_write;
        gDBCommTable.shutdown_func = gdev_cc_shutdown;
        gDBCommTable.peek_func = gdev_cc_peek;
        gDBCommTable.pre_continue_func = gdev_cc_pre_continue;
        gDBCommTable.post_stop_func = gdev_cc_post_stop;
        gDBCommTable.init_interrupts_func = gdev_cc_initinterrupts;
        return 0;
    } else {
        OSReport("MetroTRK : Set to UNKNOWN hardware. (%ld)\n", hwId);
        OSReport("MetroTRK : Invalid hardware ID passed from OS\n");
        OSReport("MetroTRK : Defaulting to GDEV Hardware\n");
        return 1;
    }
} // 0x801919C4

/* The connection is EXI-driven: nothing to do on a UART interrupt. */
void TRKUARTInterruptHandler(void) {
} // 0x80191AFC

/* Opens the connection, with TRKEXICallBack as its receive callback. */
DSError TRKInitializeIntDrivenUART(u32, u32, void** inputPendingPtr) {
    gDBCommTable.initialize_func(inputPendingPtr, TRKEXICallBack);
    gDBCommTable.open_func();
    return DS_NoError;
} // 0x80191B00

/* Enables the connection's interrupts (not for the broadband adapter). */
void EnableEXI2Interrupts(void) {
    if (!TRK_Use_BBA) {
        if (gDBCommTable.init_interrupts_func != NULL) {
            gDBCommTable.init_interrupts_func();
        }
    }
} // 0x80191B54

/* Polls the connection: the number of bytes received, 0 when none, negative on an error. */
int TRKPollUART(void) {
    return gDBCommTable.peek_func();
} // 0x80191B80

/* Reads length bytes; nonzero (-1) when the connection fails. */
UARTError TRKReadUARTN(void* bytes, u32 length) {
    int readErr = gDBCommTable.read_func(bytes, length);
    return (-readErr | readErr) >> 31;
} // 0x80191B94

/* Writes length bytes; nonzero (-1) when the connection fails. */
UARTError TRKWriteUARTN(const void* bytes, u32 length) {
    int writeErr = gDBCommTable.write_func(bytes, length);
    return (-writeErr | writeErr) >> 31;
} // 0x80191BD0

/* Takes the EXI2 port back for the debugger when the target stops. */
void ReserveEXI2Port(void) {
    gDBCommTable.post_stop_func();
} // 0x80191C0C

/* Releases the EXI2 port to the program before the target resumes. */
void UnreserveEXI2Port(void) {
    gDBCommTable.pre_continue_func();
} // 0x80191C20

/* Prints a nub message on the console. */
void TRK_board_display(const char* message) {
    OSReport("%s\n", message);
} // 0x80191C34

/* Plants the "\0END" marker word after the first instruction of PPCHalt, where the debugger recognises
 * the end of the program. */
void InitializeProgramEndTrap(void) {
    static const u32 EndofProgramInstruction = 0x00454E44;

    u8* endOfProgramInstructionBytes = (u8*)&EndofProgramInstruction;
    u8* ppcHaltPtr = (u8*)PPCHalt;

    TRK_memcpy(ppcHaltPtr + 4, endOfProgramInstructionBytes, 4);
    ICInvalidateRange(ppcHaltPtr + 4, 4);
    DCFlushRange(ppcHaltPtr + 4, 4);
} // 0x80191C4C
