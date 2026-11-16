// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Processor/ppc/Generic/targimpl.c
//
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/targimpl.c (TRK_ppc_memcpy)
//
// NonMatching: TRKPPCAccessFPRegister 96.0% (schedule: retail keeps the parameter copies and `li r31`
// ahead of the first load of the NOP-array copy). All other functions are 100%.

#include "TRK/dolphin_trk.h"
#include "TRK/dsmsg.h"
#include "TRK/flush_cache.h"
#include "TRK/mem_TRK.h"
#include "TRK/mpc_7xx_603e.h"
#include "TRK/msgbuf.h"
#include "TRK/notify.h"
#include "TRK/nubevent.h"
#include "TRK/ppc_mem.h"
#include "TRK/support.h"
#include "TRK/targimpl.h"
#include "TRK/uart.h"
#include <revolution/os/OSThread.h>

typedef struct memRange {
    u8* start;
    u8* end;
    BOOL readable;
    BOOL writeable;
} memRange;

const memRange gTRKMemMap[1] = {{(u8*)0, (u8*)-1, TRUE, TRUE}};

typedef struct StopInfo_PPC {
    u32 PC;
    u32 PCInstruction;
    u16 exceptionID;
} StopInfo_PPC;

/* Head of a stopped or exception notification (0x40 bytes). */
typedef struct TRKStopInfo {
    u32 length;         /* 0x00 */
    u8 commandID;       /* 0x04 */
    u8 unused5[3];      /* 0x05 */
    u32 PC;             /* 0x08 */
    u32 instruction;    /* 0x0C */
    u32 exceptionID;    /* 0x10 */
    u32 threadCount;    /* 0x14 */
    u32 currentThread;  /* 0x18 */
    u8 unused[0x24];    /* 0x1C */
} TRKStopInfo;

typedef struct TRKExceptionStatus {
    StopInfo_PPC exceptionInfo;
    u8 inTRK;
    u8 exceptionDetected;
} TRKExceptionStatus;

typedef struct TRKStepStatus {
    BOOL active;                /* 0x00 */
    DSMessageStepOptions type;  /* 0x04 */
    u32 count;                  /* 0x08 */
    u32 rangeStart;             /* 0x0C */
    u32 rangeEnd;               /* 0x10 */
} TRKStepStatus;

ProcessorRestoreFlags_PPC gTRKRestoreFlags = {FALSE, FALSE};

static TRKExceptionStatus gTRKExceptionStatus = {{0, 0, 0}, TRUE, 0};

static TRKStepStatus gTRKStepStatus = {FALSE, DSSTEP_IntoCount, 0, 0};

static u16 TRK_saved_exceptionID = 0;
ProcessorState_PPC gTRKCPUState;
TRKState gTRKState;

typedef unsigned char u128[16];
u128 TRKvalue128_temp;

Default_PPC gTRKSaveState;

#define SPR_GQR0 912
#define SPR_HID2 920
#define SPR_FPECR 1022

#define MSR_FP 0x2000
#define MSR_EE 0x8000
#define MSR_SE 0x400

#define HID2_PSE_LSQE 0xA0000000

#define PPC_EXCEPTION_ID_MASK 0xFFFF
#define INSTR_TWUI 0x0FE00000
#define CACHED_BASE 0x80000000

#define PPC_Program 0x700
#define PPC_Trace 0xD00

#define INSTR_NOP 0x60000000
#define INSTR_BLR 0x4E800020
#define INSTR_PSQ_ST(psr, offset, rDest, w, gqr)     (0xF0000000 | ((psr) << 21) | ((rDest) << 16) | ((w) << 15) | ((gqr) << 12) | (offset))
#define INSTR_PSQ_L(psr, offset, rSrc, w, gqr)     (0xE0000000 | ((psr) << 21) | ((rSrc) << 16) | ((w) << 15) | ((gqr) << 12) | (offset))
#define INSTR_STW(rSrc, offset, rDest) (0x90000000 | ((rSrc) << 21) | ((rDest) << 16) | (offset))
#define INSTR_LWZ(rDest, offset, rSrc) (0x80000000 | ((rDest) << 21) | ((rSrc) << 16) | (offset))
#define INSTR_STFD(fprSrc, offset, rDest) (0xD8000000 | ((fprSrc) << 21) | ((rDest) << 16) | (offset))
#define INSTR_LFD(fprDest, offset, rSrc) (0xC8000000 | ((fprDest) << 21) | ((rSrc) << 16) | (offset))
#define INSTR_MFSPR(rDest, spr)     (0x7C000000 | ((rDest) << 21) | (((spr) & 0xFE0) << 6) | (((spr) & 0x1F) << 16) | 0x2A6)
#define INSTR_MTSPR(spr, rSrc)     (0x7C000000 | ((rSrc) << 21) | (((spr) & 0xFE0) << 6) | (((spr) & 0x1F) << 16) | 0x3A6)

#define DSFetch_u32(_p_) (*((u32*)(_p_)))
#define DSFetch_u64(_p_) (*((u64*)(_p_)))

static void TRKExceptionHandler(u16);
static void TRK_ppc_memcpy(void* dest, const void* src, int n, u32 storeMSR, u32 loadMSR);
static BOOL TRKTargetCheckStep(void);
static void GetThreadInfo(int*, int*);

// ORIGINAL-ASM: MSR read; an asm function in zeldaret/tp targimpl.c
asm u32 __TRK_get_MSR(void) {
    nofralloc
    mfmsr r3
    blr
} // 0x801944F4

// ORIGINAL-ASM: MSR write; an asm function in zeldaret/tp targimpl.c
asm void __TRK_set_MSR(register u32 msr) {
    nofralloc
    mtmsr msr
    blr
} // 0x801944FC

/* Checks that [addr, addr + length) lies inside gTRKMemMap with the requested access; ranges that spill
 * past the mapped region are checked recursively. */
DSError TRKValidMemory32(const void* addr, size_t length, ValidMemoryOptions readWriteable) {
    DSError err = DS_InvalidMemory;
    const u8* start;
    const u8* end;
    s32 i;

    start = (const u8*)addr;
    end = ((const u8*)addr + (length - 1));

    if (end < start) {
        return DS_InvalidMemory;
    }

    for (i = 0; i < (s32)(sizeof(gTRKMemMap) / sizeof(memRange)); i++) {
        if (start <= (const u8*)gTRKMemMap[i].end && end >= (const u8*)gTRKMemMap[i].start) {
            if ((readWriteable == VALIDMEM_Readable && !gTRKMemMap[i].readable) ||
                (readWriteable == VALIDMEM_Writeable && !gTRKMemMap[i].writeable)) {
                err = DS_InvalidMemory;
            } else {
                err = DS_NoError;

                if (start < (const u8*)gTRKMemMap[i].start) {
                    err = TRKValidMemory32(start, (const u8*)gTRKMemMap[i].start - start, readWriteable);
                }

                if (err == DS_NoError && end > (const u8*)gTRKMemMap[i].end) {
                    err = TRKValidMemory32((const u8*)gTRKMemMap[i].end, end - (const u8*)gTRKMemMap[i].end,
                                           readWriteable);
                }
            }

            break;
        }
    }

    return err;
} // 0x80194504

/* Copies n bytes a byte at a time through aligned word accesses, with the MSR switched to loadMSR for
 * the load and to storeMSR for the store. */
// ASM-HELPER: C function whose two syncs are asm statements in the public xenoblade targimpl.c (counted)
static void TRK_ppc_memcpy(void* dest, const void* src, int n, u32 storeMSR, u32 loadMSR) {
    u32 msr;
    const u8* srcTemp = (const u8*)src;
    u8* destTemp = (u8*)dest;

    msr = __TRK_get_MSR();

    while (n != 0) {
        u8 val;

        __TRK_set_MSR(loadMSR);
        val = ppc_readbyte1(srcTemp);
        asm { sync }

        __TRK_set_MSR(storeMSR);
        ppc_writebyte1(destTemp, val);
        asm { sync }

        srcTemp++;
        destTemp++;
        n--;
    }

    __TRK_set_MSR(msr);
} // 0x801945FC

/* Reads or writes target memory with exception detection. */
DSError TRKTargetAccessMemory(void* data, u32 start, size_t* length, MemoryAccessOptions accessOptions, BOOL read) {
    DSError error;
    u32 msr;
    void* addr;
    u32 msrData;
    TRKExceptionStatus tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    addr = (void*)TRKTargetTranslate(start);
    error = TRKValidMemory32(addr, *length, read == FALSE);

    if (error != DS_NoError) {
        *length = 0;
    } else {
        msr = __TRK_get_MSR();
        msrData = msr | (gTRKCPUState.Extended1.MSR & 0x10);

        if (read) {
            TRK_ppc_memcpy(data, addr, *length, msr, msrData);
        } else {
            TRK_ppc_memcpy(addr, data, *length, msrData, msr);
            TRK_flush_cache((u32)addr, *length);
            if ((void*)start != addr) {
                TRK_flush_cache(start, *length);
            }
        }
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *length = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
} // 0x801946C8

/* Reads the instruction word at start. */
inline DSError TRKTargetReadInstruction(void* data, u32 start) {
    DSError error;
    size_t registersLength = 4;

    error = TRKTargetAccessMemory(data, start, &registersLength, MEMACCESS_UserMemory, TRUE);

    if (error == DS_NoError && registersLength != 4) {
        error = DS_InvalidMemory;
    }

    return error;
}

/* Reads or writes general registers [firstRegister, lastRegister] of the stopped target. */
DSError TRKTargetAccessDefault(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read) {
    DSError error;
    u32 count;
    u32* data;
    TRKExceptionStatus tempExceptionStatus;

    if (lastRegister > 36) {
        return DS_InvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    data = gTRKCPUState.Default.GPR + firstRegister;
    count = (lastRegister - firstRegister) + 1;
    *registersLengthPtr = count * sizeof(u32);

    if (read) {
        error = TRKAppendBuffer_ui32(b, data, count);
    } else {
        error = TRKReadBuffer_ui32(b, data, count);
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registersLengthPtr = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
} // 0x80194818

/* Reads or writes floating-point registers [firstRegister, lastRegister] one 64-bit value at a time. */
DSError TRKTargetAccessFP(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read) {
    u64 temp;
    DSError error;
    TRKExceptionStatus tempExceptionStatus;
    u32 current;

    if (lastRegister > 33) {
        return DS_InvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    __TRK_set_MSR(__TRK_get_MSR() | MSR_FP);

    *registersLengthPtr = 0;
    error = DS_NoError;

    for (current = firstRegister; current <= lastRegister && error == DS_NoError;
         current++, *registersLengthPtr += sizeof(f64)) {
        if (read) {
            TRKPPCAccessFPRegister(&temp, current, read);
            error = TRKAppendBuffer1_ui64(b, temp);
        } else {
            TRKReadBuffer1_ui64(b, &temp);
            error = TRKPPCAccessFPRegister(&temp, current, read);
        }
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registersLengthPtr = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
} // 0x80194910

/* Reads or writes supervisor registers [firstRegister, lastRegister]; a write that covers the time base or
 * the decrementer flags it to be restored on the next resume. */
DSError TRKTargetAccessExtended1(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read) {
    TRKExceptionStatus tempExceptionStatus;
    int error;
    u32* data;
    int count;

    if (lastRegister > 96) {
        return DS_InvalidRegister;
    }

    tempExceptionStatus = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    *registersLengthPtr = 0;

    if (firstRegister <= lastRegister) {
        data = (u32*)&gTRKCPUState.Extended1 + firstRegister;
        count = lastRegister - firstRegister + 1;
        *registersLengthPtr += count * sizeof(u32);

        if (read) {
            error = TRKAppendBuffer_ui32(b, data, count);
        } else {
            if (data <= &gTRKCPUState.Extended1.TBU && (data + count - 1) >= &gTRKCPUState.Extended1.TBL) {
                gTRKRestoreFlags.TBR = 1;
            }

            if (data <= &gTRKCPUState.Extended1.DEC && (data + count - 1) >= &gTRKCPUState.Extended1.DEC) {
                gTRKRestoreFlags.DEC = 1;
            }
            error = TRKReadBuffer_ui32(b, data, count);
        }
    }
    if (gTRKExceptionStatus.exceptionDetected) {
        *registersLengthPtr = 0;
        error = DS_CWDSException;
    }

    gTRKExceptionStatus = tempExceptionStatus;
    return error;
} // 0x80194A4C

/* Reads or writes the paired-single registers [firstRegister, lastRegister] with HID2[PSE,LSQE] and
 * GQR0 set up for plain 32-bit accesses. */
DSError TRKTargetAccessExtended2(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registerStorageSize, BOOL read) {
    TRKExceptionStatus savedException;
    u32 i;
    u32 value_buf0[1];
    u32 value_buf[2];
    DSError err;

    if (lastRegister > 31) {
        return DS_InvalidRegister;
    }

    savedException = gTRKExceptionStatus;
    gTRKExceptionStatus.exceptionDetected = FALSE;

    TRKPPCAccessSPR(value_buf0, SPR_HID2, TRUE);

    value_buf0[0] |= HID2_PSE_LSQE;
    TRKPPCAccessSPR(value_buf0, SPR_HID2, FALSE);

    value_buf0[0] = 0;
    TRKPPCAccessSPR(value_buf0, SPR_GQR0, FALSE);

    *registerStorageSize = 0;
    err = DS_NoError;

    for (i = firstRegister; i <= lastRegister && err == DS_NoError; i++) {
        if (read) {
            err = TRKPPCAccessPairedSingleRegister((u64*)value_buf, i, read);
            err = TRKAppendBuffer1_ui64(b, *(u64*)value_buf);
        } else {
            err = TRKReadBuffer1_ui64(b, (u64*)value_buf);
            err = TRKPPCAccessPairedSingleRegister((u64*)value_buf, i, read);
        }

        *registerStorageSize += sizeof(u64);
    }

    if (gTRKExceptionStatus.exceptionDetected) {
        *registerStorageSize = 0;
        err = DS_CWDSException;
    }

    gTRKExceptionStatus = savedException;

    return err;
} // 0x80194BB0

// ORIGINAL-ASM: first-level exception handler running on the target's registers; an asm function in zeldaret/tp targimpl.c
asm void TRKInterruptHandler(void) {
    nofralloc
    mtsrr0 r2
    mtsrr1 r4
    mfsprg r4, 3
    mfcr r2
    mtsprg 3, r2
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, TRKState_PPC.MSR(r2)
    ori r2, r2, 0x8002
    xori r2, r2, 0x8002
    sync
    mtmsr r2
    sync
    lis r2, TRK_saved_exceptionID@h
    ori r2, r2, TRK_saved_exceptionID@l
    sth r3, 0(r2)
    cmpwi r3, 0x500
    bne L_not_external
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    mflr r3
    stw r3, ProcessorState_PPC.transport_handler_saved_ra(r2)
    bl TRKUARTInterruptHandler
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    lwz r3, ProcessorState_PPC.transport_handler_saved_ra(r2)
    mtlr r3
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, TRKState_PPC.inputPendingPtr(r2)
    lbz r2, 0(r2)
    cmpwi r2, 0
    beq L_resume_target
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    lbz r2, TRKExceptionStatus.inTRK(r2)
    cmpwi r2, 1
    beq L_resume_target
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    li r3, 1
    stb r3, TRKState_PPC.inputActivated(r2)
    b L_not_external
L_resume_target:
    lis r2, gTRKSaveState@h
    ori r2, r2, gTRKSaveState@l
    lwz r3, Default_PPC.CR(r2)
    mtcrf 0xff, r3
    lwz r3, Default_PPC.GPR[3](r2)
    lwz r2, Default_PPC.GPR[2](r2)
    rfi
L_not_external:
    lis r2, TRK_saved_exceptionID@h
    ori r2, r2, TRK_saved_exceptionID@l
    lhz r3, 0(r2)
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    lbz r2, TRKExceptionStatus.inTRK(r2)
    cmpwi r2, 0
    bne TRKExceptionHandler
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    stw r0, ProcessorState_PPC.Default.GPR[0](r2)
    stw r1, ProcessorState_PPC.Default.GPR[1](r2)
    mfsprg r0, 1
    stw r0, ProcessorState_PPC.Default.GPR[2](r2)
    sth r3, ProcessorState_PPC.Extended1.exceptionID(r2)
    sth r3, (ProcessorState_PPC.Extended1.exceptionID + 2)(r2)
    mfsprg r0, 2
    stw r0, ProcessorState_PPC.Default.GPR[3](r2)
    stmw r4, ProcessorState_PPC.Default.GPR[4](r2)
    mfsrr0 r27
    mflr r28
    mfsprg r29, 3
    mfctr r30
    mfxer r31
    stmw r27, ProcessorState_PPC.Default.PC(r2)
    bl TRKSaveExtended1Block
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 1
    stb r3, TRKExceptionStatus.inTRK(r2)
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r0, TRKState_PPC.MSR(r2)
    sync
    mtmsr r0
    sync
    lwz r0, TRKState_PPC.LR(r2)
    mtlr r0
    lwz r0, TRKState_PPC.CTR(r2)
    mtctr r0
    lwz r0, TRKState_PPC.XER(r2)
    mtxer r0
    lwz r0, TRKState_PPC.DSISR(r2)
    mtdsisr r0
    lwz r0, TRKState_PPC.DAR(r2)
    mtdar r0
    lmw r3, TRKState_PPC.GPR[3](r2)
    lwz r0, TRKState_PPC.GPR[0](r2)
    lwz r1, TRKState_PPC.GPR[1](r2)
    lwz r2, TRKState_PPC.GPR[2](r2)
    b TRKPostInterruptEvent
} // 0x80194D20

// ORIGINAL-ASM: handler for exceptions raised while the nub runs; an asm function in zeldaret/tp targimpl.c
static asm void TRKExceptionHandler(u16) {
    nofralloc
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    sth r3, TRKExceptionStatus.exceptionInfo.exceptionID(r2)
    mfsrr0 r3
    stw r3, TRKExceptionStatus.exceptionInfo.PC(r2)
    lhz r3, TRKExceptionStatus.exceptionInfo.exceptionID(r2)
    cmpwi r3, 0x200
    beq L_skip_instruction
    cmpwi r3, 0x300
    beq L_skip_instruction
    cmpwi r3, 0x400
    beq L_skip_instruction
    cmpwi r3, 0x600
    beq L_skip_instruction
    cmpwi r3, 0x700
    beq L_skip_instruction
    cmpwi r3, 0x800
    beq L_skip_instruction
    cmpwi r3, 0x1000
    beq L_skip_instruction
    cmpwi r3, 0x1100
    beq L_skip_instruction
    cmpwi r3, 0x1200
    beq L_skip_instruction
    cmpwi r3, 0x1300
    beq L_skip_instruction
    b L_flag_exception
L_skip_instruction:
    mfsrr0 r3
    addi r3, r3, 0x4
    mtsrr0 r3
L_flag_exception:
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 0x1
    stb r3, TRKExceptionStatus.exceptionDetected(r2)
    mfsprg r3, 3
    mtcrf 0xff, r3
    mfsprg r2, 1
    mfsprg r3, 2
    rfi
} // 0x80194EB4

/* Queues the event for the interrupt that stopped the target: a support request, a breakpoint or an
 * exception. Input that arrived during the interrupt only clears its flag. */
void TRKPostInterruptEvent(void) {
    NubEventType eventType;
    int instruction;
    TRKEvent event;

    if (gTRKState.inputActivated) {
        gTRKState.inputActivated = FALSE;
    } else {
        switch ((int)(gTRKCPUState.Extended1.exceptionID & PPC_EXCEPTION_ID_MASK)) {
        case PPC_Program:
        case PPC_Trace:
            TRKTargetReadInstruction(&instruction, gTRKCPUState.Default.PC);

            if (instruction == INSTR_TWUI) {
                eventType = NUBEVENT_Support;
            } else {
                eventType = NUBEVENT_Breakpoint;
            }
            break;
        default:
            eventType = NUBEVENT_Exception;
            break;
        }

        TRKConstructEvent(&event, eventType);
        TRKPostEvent(&event);
    }
} // 0x80194F50

// ORIGINAL-ASM: swaps the nub state for the target state and resumes it with rfi; an asm function in zeldaret/tp targimpl.c
asm void TRKSwapAndGo(void) {
    nofralloc
    lis r3, gTRKState@h
    ori r3, r3, gTRKState@l
    stmw r0, TRKState_PPC.GPR[0](r3)
    mfmsr r0
    stw r0, TRKState_PPC.MSR(r3)
    mflr r0
    stw r0, TRKState_PPC.LR(r3)
    mfctr r0
    stw r0, TRKState_PPC.CTR(r3)
    mfxer r0
    stw r0, TRKState_PPC.XER(r3)
    mfdsisr r0
    stw r0, TRKState_PPC.DSISR(r3)
    mfdar r0
    stw r0, TRKState_PPC.DAR(r3)
    li r1, -0x7ffe
    nor r1, r1, r1
    mfmsr r3
    and r3, r3, r1
    mtmsr r3
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r2, TRKState_PPC.inputPendingPtr(r2)
    lbz r2, 0(r2)
    cmpwi r2, 0
    beq L_restore_target
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    li r3, 1
    stb r3, TRKState_PPC.inputActivated(r2)
    b TRKInterruptHandlerEnableInterrupts
L_restore_target:
    lis r2, gTRKExceptionStatus@h
    ori r2, r2, gTRKExceptionStatus@l
    li r3, 0
    stb r3, TRKExceptionStatus.inTRK(r2)
    bl TRKRestoreExtended1Block
    lis r2, gTRKCPUState@h
    ori r2, r2, gTRKCPUState@l
    lmw r27, ProcessorState_PPC.Default.PC(r2)
    mtsrr0 r27
    mtlr r28
    mtcrf 0xff, r29
    mtctr r30
    mtxer r31
    lmw r3, ProcessorState_PPC.Default.GPR[3](r2)
    lwz r0, ProcessorState_PPC.Default.GPR[0](r2)
    lwz r1, ProcessorState_PPC.Default.GPR[1](r2)
    lwz r2, ProcessorState_PPC.Default.GPR[2](r2)
    rfi
} // 0x80195008

// ORIGINAL-ASM: restores the nub state and enters TRKPostInterruptEvent; an asm function in zeldaret/tp targimpl.c
asm void TRKInterruptHandlerEnableInterrupts(void) {
    nofralloc
    lis r2, gTRKState@h
    ori r2, r2, gTRKState@l
    lwz r0, TRKState_PPC.MSR(r2)
    sync
    mtmsr r0
    sync
    lwz r0, TRKState_PPC.LR(r2)
    mtlr r0
    lwz r0, TRKState_PPC.CTR(r2)
    mtctr r0
    lwz r0, TRKState_PPC.XER(r2)
    mtxer r0
    lwz r0, TRKState_PPC.DSISR(r2)
    mtdsisr r0
    lwz r0, TRKState_PPC.DAR(r2)
    mtdar r0
    lmw r3, TRKState_PPC.GPR[3](r2)
    lwz r0, TRKState_PPC.GPR[0](r2)
    lwz r1, TRKState_PPC.GPR[1](r2)
    lwz r2, TRKState_PPC.GPR[2](r2)
    b TRKPostInterruptEvent
} // 0x801950CC

/* Handles a breakpoint or exception event: unless a step in progress continues, marks the target
 * stopped and notifies the host. */
DSError TRKTargetInterrupt(TRKEvent* event) {
    DSError error = DS_NoError;
    switch (event->eventType) {
    case NUBEVENT_Breakpoint:
    case NUBEVENT_Exception:
        if (TRKTargetCheckStep() == FALSE) {
            TRKTargetSetStopped(TRUE);
            error = TRKDoNotifyStopped(DSMSG_NotifyStopped);
        }
        break;
    default:
        break;
    }

    return error;
} // 0x80195120

/* Fills the stop notification: a 0x40-byte head, then the general registers, PC, LR, CR, CTR, XER,
 * the OS low-memory words 0xD4..0xE4 and the 0x400 bytes of memory at the 1 KiB block of the PC. */
DSError TRKTargetAddStopInfo(TRKBuffer* b) {
    DSError error;
    TRKStopInfo reply;
    int instruction;
    int threadCount;
    int currentIndex;
    size_t length;
    u8 memory[0x400];
    u32 value;
    int i;

    TRK_memset(&reply, 0, sizeof(reply));
    reply.length = 0x4E8;
    reply.commandID = DSMSG_NotifyStopped;
    reply.PC = gTRKCPUState.Default.PC;

    GetThreadInfo(&threadCount, &currentIndex);
    reply.threadCount = threadCount;
    reply.currentThread = *ConvertAddress(0xE4);

    TRKTargetReadInstruction(&instruction, gTRKCPUState.Default.PC);
    reply.instruction = instruction;
    reply.exceptionID = gTRKCPUState.Extended1.exceptionID & PPC_EXCEPTION_ID_MASK;
    error = TRKAppendBuffer_ui8(b, (u8*)&reply, sizeof(reply));

    if (error == DS_NoError) {
        for (i = 0; i < 32; i++) {
            TRKAppendBuffer1_ui32(b, gTRKCPUState.Default.GPR[i]);
        }
        TRKAppendBuffer1_ui32(b, gTRKCPUState.Default.PC);
        TRKAppendBuffer1_ui32(b, gTRKCPUState.Default.LR);
        TRKAppendBuffer1_ui32(b, gTRKCPUState.Default.CR);
        TRKAppendBuffer1_ui32(b, gTRKCPUState.Default.CTR);
        error = TRKAppendBuffer1_ui32(b, gTRKCPUState.Default.XER);
    }

    if (error == DS_NoError) {
        value = *ConvertAddress(0xD4);
        TRKAppendBuffer1_ui32(b, value);
        value = *ConvertAddress(0xD8);
        TRKAppendBuffer1_ui32(b, value);
        value = *ConvertAddress(0xDC);
        TRKAppendBuffer1_ui32(b, value);
        value = *ConvertAddress(0xE0);
        TRKAppendBuffer1_ui32(b, value);
        value = *ConvertAddress(0xE4);
        error = TRKAppendBuffer1_ui32(b, value);
    }

    if (error == DS_NoError) {
        length = sizeof(memory);
        error = TRKTargetAccessMemory(memory, gTRKCPUState.Default.PC & ~0x3FF, &length, MEMACCESS_UserMemory, TRUE);
        TRKAppendBuffer(b, memory, sizeof(memory));
    }

    return error;
} // 0x8019517C

/* Fills the exception notification: the PC and instruction of the exception and its ID. */
DSError TRKTargetAddExceptionInfo(TRKBuffer* b) {
    int instruction;
    TRKStopInfo reply;

    TRK_memset(&reply, 0, sizeof(reply));

    reply.length = 0x40;
    reply.commandID = DSMSG_NotifyException;
    reply.PC = gTRKExceptionStatus.exceptionInfo.PC;

    TRKTargetReadInstruction(&instruction, gTRKExceptionStatus.exceptionInfo.PC);

    reply.instruction = instruction;
    reply.exceptionID = gTRKExceptionStatus.exceptionInfo.exceptionID;

    return TRKAppendBuffer_ui8(b, (u8*)&reply, sizeof(reply));
} // 0x8019537C

/* Single-step tracing runs with external interrupts masked. */
inline DSError TRKTargetEnableTrace(BOOL val) {
    if (val) {
        gTRKCPUState.Extended1.MSR = (gTRKCPUState.Extended1.MSR | MSR_SE) & ~MSR_EE;
    } else {
        gTRKCPUState.Extended1.MSR = (gTRKCPUState.Extended1.MSR & ~MSR_SE) | MSR_EE;
    }
    return DS_NoError;
}

/* Tells whether the step in progress has finished at this trace exception. */
inline BOOL TRKTargetStepDone(void) {
    BOOL result = TRUE;

    if (gTRKStepStatus.active && ((u16)gTRKCPUState.Extended1.exceptionID) == PPC_Trace) {
        switch (gTRKStepStatus.type) {
        case DSSTEP_IntoCount:
            if (gTRKStepStatus.count > 0) {
                result = FALSE;
            }
            break;
        case DSSTEP_IntoRange:
            if (gTRKCPUState.Default.PC >= gTRKStepStatus.rangeStart &&
                gTRKCPUState.Default.PC <= gTRKStepStatus.rangeEnd) {
                result = FALSE;
            }
            break;
        default:
            break;
        }
    }

    return result;
}

/* Starts or continues a step: turns tracing on and lets the target run. */
inline DSError TRKTargetDoStep(void) {
    gTRKStepStatus.active = TRUE;
    TRKTargetEnableTrace(TRUE);

    if (gTRKStepStatus.type == DSSTEP_IntoCount || gTRKStepStatus.type == DSSTEP_OverCount) {
        gTRKStepStatus.count--;
    }

    TRKTargetSetStopped(FALSE);
    return DS_NoError;
}

/* Returns whether a step is still in progress after this stop: an unfinished step is resumed with
 * the trace bit set, a finished one is cleared. */
static BOOL TRKTargetCheckStep(void) {
    if (gTRKStepStatus.active) {
        TRKTargetEnableTrace(FALSE);

        if (TRKTargetStepDone()) {
            gTRKStepStatus.active = FALSE;
        } else {
            TRKTargetDoStep();
        }
    }

    return gTRKStepStatus.active;
} // 0x80195418

/* Steps count instructions; stepping over is not supported. */
DSError TRKTargetSingleStep(u32 count, BOOL stepOver) {
    DSError error = DS_NoError;

    if (stepOver) {
        error = DS_UnsupportedError;
    } else {
        gTRKStepStatus.type = DSSTEP_IntoCount;
        gTRKStepStatus.count = count;
        error = TRKTargetDoStep();
    }

    return error;
} // 0x8019553C

/* Steps until the PC leaves [rangeStart, rangeEnd]; stepping over is not supported. */
DSError TRKTargetStepOutOfRange(u32 rangeStart, u32 rangeEnd, BOOL stepOver) {
    DSError error = DS_NoError;

    if (stepOver) {
        error = DS_UnsupportedError;
    } else {
        gTRKStepStatus.type = DSSTEP_IntoRange;
        gTRKStepStatus.rangeStart = rangeStart;
        gTRKStepStatus.rangeEnd = rangeEnd;
        error = TRKTargetDoStep();
    }

    return error;
} // 0x801955C4

/* Returns the PC of the stopped target. */
u32 TRKTargetGetPC(void) {
    return gTRKCPUState.Default.PC;
} // 0x80195638

/* Services the file request the target made through the support trap: the request number is in r3
 * and its arguments in r4..r6; the result goes back in r3 and the PC moves past the trap. */
DSError TRKTargetSupportRequest(void) {
    DSIOResult ioResult;
    size_t* length;
    MessageCommandID commandId;
    DSError error;
    u32 position;
    TRKEvent event;

    commandId = gTRKCPUState.Default.GPR[3];
    if (commandId != DSMSG_ReadFile && commandId != DSMSG_WriteFile && commandId != DSMSG_OpenFile &&
        commandId != DSMSG_CloseFile && commandId != DSMSG_PositionFile) {
        TRKConstructEvent(&event, NUBEVENT_Exception);
        TRKPostEvent(&event);
        return DS_NoError;
    } else if (commandId == DSMSG_OpenFile) {
        error = HandleOpenFileSupportRequest((const char*)gTRKCPUState.Default.GPR[4],
                                             gTRKCPUState.Default.GPR[5] & 0xFF,
                                             (u32*)gTRKCPUState.Default.GPR[6], &ioResult);

        if (ioResult == DS_IONoError && error != DS_NoError) {
            ioResult = DS_IOError;
        }

        gTRKCPUState.Default.GPR[3] = ioResult;
    } else if (commandId == DSMSG_CloseFile) {
        error = HandleCloseFileSupportRequest(gTRKCPUState.Default.GPR[4], &ioResult);

        if (ioResult == DS_IONoError && error != DS_NoError) {
            ioResult = DS_IOError;
        }

        gTRKCPUState.Default.GPR[3] = ioResult;
    } else if (commandId == DSMSG_PositionFile) {
        position = *(u32*)gTRKCPUState.Default.GPR[5];
        error = HandlePositionFileSupportRequest(gTRKCPUState.Default.GPR[4], &position,
                                                 (u8)gTRKCPUState.Default.GPR[6], &ioResult);

        if (ioResult == DS_IONoError && error != DS_NoError) {
            ioResult = DS_IOError;
        }

        gTRKCPUState.Default.GPR[3] = ioResult;
        *(u32*)gTRKCPUState.Default.GPR[5] = position;
    } else {
        length = (size_t*)gTRKCPUState.Default.GPR[5];
        error = TRKSuppAccessFile(gTRKCPUState.Default.GPR[4], (u8*)gTRKCPUState.Default.GPR[6], length,
                                  &ioResult, TRUE, commandId == DSMSG_ReadFile);

        if (ioResult == DS_IONoError && error != DS_NoError) {
            ioResult = DS_IOError;
        }

        gTRKCPUState.Default.GPR[3] = ioResult;

        if (commandId == DSMSG_ReadFile) {
            TRK_flush_cache(gTRKCPUState.Default.GPR[6], *length);
        }
    }

    gTRKCPUState.Default.PC += 4;
    return error;
} // 0x80195648

BOOL TRKTargetStopped(void) {
    return gTRKState.isStopped;
} // 0x8019583C

void TRKTargetSetStopped(unsigned int stopped) {
    gTRKState.isStopped = stopped;
} // 0x8019584C

/* Marks the target stopped. */
DSError TRKTargetStop(void) {
    gTRKState.isStopped = TRUE;
    return DS_NoError;
} // 0x8019585C

/* Reads or writes a special-purpose register by running a generated mfspr/mtspr stub. */
DSError TRKPPCAccessSPR(void* value, u32 spr_register_num, BOOL read) {
    u32 access_func[10] = {INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP,
                           INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP};

    if (read) {
        access_func[0] = INSTR_MFSPR(4, spr_register_num);
        access_func[1] = (u32)INSTR_STW(4, 0, 3);
    } else {
        access_func[0] = (u32)INSTR_LWZ(4, 0, 3);
        access_func[1] = INSTR_MTSPR(spr_register_num, 4);
    }

    return TRKPPCAccessSpecialReg(value, access_func, read);
} // 0x80195874

/* Reads or writes a paired-single register by running a generated psq_st/psq_l stub. */
DSError TRKPPCAccessPairedSingleRegister(void* srcDestPtr, u32 psr, BOOL read) {
    u32 instructionData[] = {INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP,
                             INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP};

    if (read) {
        instructionData[0] = INSTR_PSQ_ST(psr, 0, 3, 0, 0);
    } else {
        instructionData[0] = INSTR_PSQ_L(psr, 0, 3, 0, 0);
    }

    return TRKPPCAccessSpecialReg(srcDestPtr, instructionData, read);
} // 0x80195950

// ORIGINAL-ASM: FPSCR read through mffs; an asm function in zeldaret/tp targimpl.c
asm void ReadFPSCR(register f64*) {
    nofralloc
    stwu r1, -0x40(r1)
    stfd f31, 0x10(r1)
    psq_st f31, 0x20(r1), 0, 0
    mffs f31
    stfd f31, 0x0(r3)
    psq_l f31, 0x20(r1), 0, 0
    lfd f31, 0x10(r1)
    addi r1, r1, 0x40
    blr
} // 0x801959F8

// ORIGINAL-ASM: FPSCR write through mtfsf; an asm function in zeldaret/tp targimpl.c
asm void WriteFPSCR(register f64*) {
    nofralloc
    stwu r1, -0x40(r1)
    stfd f31, 0x10(r1)
    psq_st f31, 0x20(r1), 0, 0
    lfd f31, 0(r3)
    mtfsf 0xff, f31
    psq_l f31, 0x20(r1), 0, 0
    lfd f31, 0x10(r1)
    addi r1, r1, 0x40
    blr
} // 0x80195A1C

// Not a const-parameter effect: the rows that move belong to the local copy of the static instruction template,
// not to accesses through srcDestPtr.
/* Reads or writes FPR 0..31, FPSCR (32) or FPECR (33). */
DSError TRKPPCAccessFPRegister(void* srcDestPtr, u32 fpr, BOOL read) {
    DSError error = DS_NoError;
    u32 instructionData1[] = {INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP,
                              INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP, INSTR_NOP};

    if (fpr < 32) {
        if (read) {
            instructionData1[0] = INSTR_STFD(fpr, 0, 3);
        } else {
            instructionData1[0] = INSTR_LFD(fpr, 0, 3);
        }

        error = TRKPPCAccessSpecialReg(srcDestPtr, instructionData1, read);
    } else if (fpr == 32) {
        if (read) {
            ReadFPSCR(srcDestPtr);
        } else {
            WriteFPSCR(srcDestPtr);
        }

        *(u64*)srcDestPtr &= 0xFFFFFFFF; /* FPSCR is 32 bits */
    } else if (fpr == 33) {
        if (!read) {
            *(u32*)srcDestPtr = *((u32*)(srcDestPtr) + 1);
        }

        error = TRKPPCAccessSPR(srcDestPtr, SPR_FPECR, read);
        if (read) {
            DSFetch_u64(srcDestPtr) = DSFetch_u32(srcDestPtr) & 0xffffffffLL;
        }
    }

    return error;
} // 0x80195A40

/* Runs a generated access stub: terminates it with blr, flushes the caches and calls it. */
DSError TRKPPCAccessSpecialReg(void* value, u32* access_func, BOOL read) {
    typedef void (*asm_access_type)(void*, void*);

    asm_access_type asm_access;

    access_func[9] = INSTR_BLR;

    asm_access = (asm_access_type)access_func;

    TRK_flush_cache((u32)access_func, 0x28);
    (*asm_access)((u32*)value, (void*)&TRKvalue128_temp);

    return DS_NoError;
} // 0x80195C08

void TRKTargetSetInputPendingPtr(void* ptr) {
    gTRKState.inputPendingPtr = ptr;
} // 0x80195C70

/* Maps a physical address to its cached alias. */
u32* ConvertAddress(u32 addr) {
    return (u32*)(addr | CACHED_BASE);
} // 0x80195C80

/* Counts the threads on the OS active-thread list and finds the index of the running one; the list
 * links are physical addresses, so each is converted to a cached one. */
static void GetThreadInfo(int* count, int* currentIndex) {
    int i;
    OSThread* thread;

    *count = 1;
    *currentIndex = 0;

    thread = __OSActiveThreadQueue.head;
    if (thread == (OSThread*)-1 || thread == NULL || thread == (OSThread*)CACHED_BASE) {
        return;
    }

    i = 0;
    while (thread != NULL) {
        if (thread == __OSCurrentThread) {
            *currentIndex = i;
        }
        thread = (OSThread*)ConvertAddress((u32)thread->linkActive.next);
        i++;
        if (thread == (OSThread*)-1 || thread == NULL || thread == (OSThread*)CACHED_BASE) {
            break;
        }
    }
    *count = i;
} // 0x80195C88
