// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk.c
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/dolphin_trk.c
// (the retail bodies are the Wii variant of xenoblade: MEM2 translation, lc_base and __TRK_reset)

#include "TRK/dolphin_trk.h"
#include "TRK/dolphin_trk_glue.h"
#include "TRK/__exception.h"
#include "TRK/flush_cache.h"
#include "TRK/main_TRK.h"
#include "TRK/mem_TRK.h"
#include "TRK/mpc_7xx_603e.h"
#include "TRK/targimpl.h"
#include <revolution/os/OSReset.h>

/* PowerPC exception vector offsets. */
#define PPC_SYSTEMRESET             0x0100
#define PPC_MACHINECHECK            0x0200
#define PPC_DATAACCESSERROR         0x0300
#define PPC_INSTACCESSERROR         0x0400
#define PPC_EXTERNALINTERRUPT       0x0500
#define PPC_ALIGNMENTERROR          0x0600
#define PPC_PROGRAMERROR            0x0700
#define PPC_FPUNAVAILABLE           0x0800
#define PPC_DECREMENTERINTERRUPT    0x0900
#define PPC_SYSTEMCALL              0x0C00
#define PPC_TRACE                   0x0D00
#define PPC_PERFORMANCE_MONITOR     0x0F00
#define PPC_INSTR_ADDR_BREAK        0x1300
#define PPC_SYS_MANAGE              0x1400
#define PPC_THERMAL_MANAGE          0x1700

#define EXCEPTION_SIZE              0x100
#define NUM_EXCEPTIONS              15

/* Index of the external interrupt in TRK_ISR_OFFSETS: the OS keeps its own handler there. */
#define EXTERNAL_INTERRUPT_INDEX    4

/* Low-memory word whose bits select the vectors the debugger takes over. */
#define DB_EXCEPTION_MASK           0x44

/* Cached views of MEM1 and MEM2. */
#define MEM1_CACHED                 0x80000000
#define MEM2_CACHED                 0x90000000
#define PHYSICAL_MASK               0x3FFFFFFF

/* Locked-cache window: 16 KB at lc_base, reachable while DBAT3 maps it (the OS maps the locked cache with
 * DBAT3; DBAT3U & 3 is its Vs/Vp valid bits). */
#define LC_SIZE                     0x4000

static u32 TRK_ISR_OFFSETS[NUM_EXCEPTIONS] = {
    PPC_SYSTEMRESET,
    PPC_MACHINECHECK,
    PPC_DATAACCESSERROR,
    PPC_INSTACCESSERROR,
    PPC_EXTERNALINTERRUPT,
    PPC_ALIGNMENTERROR,
    PPC_PROGRAMERROR,
    PPC_FPUNAVAILABLE,
    PPC_DECREMENTERINTERRUPT,
    PPC_SYSTEMCALL,
    PPC_TRACE,
    PPC_PERFORMANCE_MONITOR,
    PPC_INSTR_ADDR_BREAK,
    PPC_SYS_MANAGE,
    PPC_THERMAL_MANAGE,
};

static u32 lc_base;

// ORIGINAL-ASM: saves the caller's registers into gTRKCPUState, moves to the debugger stack and jumps to TRK_main; an asm function with nofralloc in the public tp and xenoblade reconstructions
/* Debugger entry for the NDEV (r5: the hardware ID passed from the OS). If InitMetroTRKCommTable fails,
 * the return path reads LR and the registers through its return value instead of gTRKCPUState (the
 * original's bug, kept). */
asm void InitMetroTRK(void) {
    nofralloc
    subi r1, r1, 4
    stw r3, 0(r1)
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    stmw r0, ProcessorState_PPC.Default.GPR(r3)
    lwz r4, 0(r1)
    addi r1, r1, 4
    stw r1, ProcessorState_PPC.Default.GPR[1](r3)
    stw r4, ProcessorState_PPC.Default.GPR[3](r3)
    mflr r4
    stw r4, ProcessorState_PPC.Default.LR(r3)
    stw r4, ProcessorState_PPC.Default.PC(r3)
    mfcr r4
    stw r4, ProcessorState_PPC.Default.CR(r3)
    mfmsr r4
    ori r3, r4, 0x8000
    xori r3, r3, 0x8000
    mtmsr r3
    mtsrr1 r4
    bl TRKSaveExtended1Block
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    lmw r0, ProcessorState_PPC.Default.GPR(r3)
    li r0, 0
    mtspr IABR, r0
    mtspr DABR, r0
    lis r1, _db_stack_addr@h
    ori r1, r1, _db_stack_addr@l
    mr r3, r5
    bl InitMetroTRKCommTable
    cmpwi r3, 1
    bne initCommTableSuccess
    lwz r4, ProcessorState_PPC.Default.LR(r3)
    mtlr r4
    lmw r0, ProcessorState_PPC.Default.GPR(r3)
    blr
initCommTableSuccess:
    b TRK_main
    blr
} // 0x801915E0

// ORIGINAL-ASM: the same entry for the broadband adapter, with external interrupts left enabled; an asm function with nofralloc in the public tp and xenoblade reconstructions
/* Debugger entry for the broadband adapter (hardware ID 2). */
asm void InitMetroTRK_BBA(void) {
    nofralloc
    subi r1, r1, 4
    stw r3, 0(r1)
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    stmw r0, ProcessorState_PPC.Default.GPR(r3)
    lwz r4, 0(r1)
    addi r1, r1, 4
    stw r1, ProcessorState_PPC.Default.GPR[1](r3)
    stw r4, ProcessorState_PPC.Default.GPR[3](r3)
    mflr r4
    stw r4, ProcessorState_PPC.Default.LR(r3)
    stw r4, ProcessorState_PPC.Default.PC(r3)
    mfcr r4
    stw r4, ProcessorState_PPC.Default.CR(r3)
    mfmsr r4
    ori r3, r4, 0x8000
    mtmsr r3
    mtsrr1 r4
    bl TRKSaveExtended1Block
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    lmw r0, ProcessorState_PPC.Default.GPR(r3)
    li r0, 0
    mtspr IABR, r0
    mtspr DABR, r0
    lis r1, _db_stack_addr@h
    ori r1, r1, _db_stack_addr@l
    li r3, 2
    bl InitMetroTRKCommTable
    cmpwi r3, 1
    bne initCommTableSuccess
    lwz r4, ProcessorState_PPC.Default.LR(r3)
    mtlr r4
    lmw r0, ProcessorState_PPC.Default.GPR(r3)
    blr
initCommTableSuccess:
    b TRK_main
    blr
} // 0x80191678

/* Called by OSInit once the OS is up. */
void EnableMetroTRKInterrupts(void) {
    EnableEXI2Interrupts();
} // 0x8019170C

/* Maps a target address to the address the nub accesses: the locked-cache window as is while DBAT3
 * maps it, physical MEM1 and MEM2 addresses through their cached views, anything else unchanged. */
u32 TRKTargetTranslate(u32 addr) {
    if (addr >= lc_base && addr < lc_base + LC_SIZE) {
        if (gTRKCPUState.Extended1.DBAT3U & 3) {
            return addr;
        }
    }

    if (addr < 0x03000000) {
        return (addr & PHYSICAL_MASK) | MEM1_CACHED;
    }

    if (addr >= 0x10000000 && 0x1C000000 > addr) {
        return (addr & PHYSICAL_MASK) | MEM2_CACHED;
    }

    return addr;
} // 0x80191710

/* Copies the nub's handler for one vector into place. */
static void TRK_copy_vector(u32 offset) {
    void* dest = (void*)TRKTargetTranslate(offset);
    TRK_memcpy(dest, gTRKInterruptVectorTable + offset, EXCEPTION_SIZE);
    TRK_flush_cache((u32)dest, EXCEPTION_SIZE);
}

/* Installs the nub's handler on every vector the debugger mask selects, except the external interrupt. */
void __TRK_copy_vectors(void) {
    u32* maskPtr;
    u32* isrOffsetPtr;
    int i;
    u32 mask;

    if (lc_base <= DB_EXCEPTION_MASK && lc_base + LC_SIZE > DB_EXCEPTION_MASK
        && (gTRKCPUState.Extended1.DBAT3U & 3)) {
        maskPtr = (u32*)DB_EXCEPTION_MASK;
    } else {
        maskPtr = (u32*)(MEM1_CACHED + DB_EXCEPTION_MASK);
    }

    isrOffsetPtr = TRK_ISR_OFFSETS;
    i = 0;
    mask = *maskPtr;

    do {
        if ((mask & (1 << i)) && i != EXTERNAL_INTERRUPT_INDEX) {
            TRK_copy_vector(*isrOffsetPtr);
        }

        i++;
        isrOffsetPtr++;
    } while (i <= NUM_EXCEPTIONS - 1);
} // 0x80191778

/* Starts the target stopped, records its MSR and places the locked-cache window. */
DSError TRKInitializeTarget(void) {
    gTRKState.isStopped = TRUE;
    gTRKState.MSR = __TRK_get_MSR();
    lc_base = 0xE0000000;
    return DS_NoError;
} // 0x801918AC

/* Resets the console on the host's reset request. */
void __TRK_reset(void) {
    OSResetSystem(FALSE, 0, FALSE);
} // 0x801918F4
