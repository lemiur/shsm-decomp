// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/ppc/Generic/targimpl.h

#ifndef TRK_TARGIMPL_H
#define TRK_TARGIMPL_H

#include <stddef.h>
#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/dsmsg.h"
#include "TRK/msgbuf.h"
#include "TRK/nubevent.h"

#ifdef __cplusplus
extern "C" {
#endif

/* General-purpose registers and the user-mode special registers. */
typedef struct Default_PPC {
    u32 GPR[32];
    u32 PC;
    u32 LR;
    u32 CR;
    u32 CTR;
    u32 XER;
} Default_PPC;

typedef struct Float_PPC {
    u64 FPR[32];
    u64 FPSCR;
    u64 FPECR;
} Float_PPC;

/* Supervisor registers of the 750CL (Broadway), in the order the debugger reads them. */
typedef struct Extended1_PPC_6xx_7xx {
    u32 SR[16];
    u32 TBL;
    u32 TBU;
    u32 HID0;
    u32 HID1;
    u32 MSR;
    u32 PVR;
    u32 IBAT0U;
    u32 IBAT0L;
    u32 IBAT1U;
    u32 IBAT1L;
    u32 IBAT2U;
    u32 IBAT2L;
    u32 IBAT3U;
    u32 IBAT3L;
    u32 DBAT0U;
    u32 DBAT0L;
    u32 DBAT1U;
    u32 DBAT1L;
    u32 DBAT2U;
    u32 DBAT2L;
    u32 DBAT3U;
    u32 DBAT3L;
    u32 DMISS;
    u32 DCMP;
    u32 HASH1;
    u32 HASH2;
    u32 IMISS;
    u32 ICMP;
    u32 RPA;
    u32 SDR1;
    u32 DAR;
    u32 DSISR;
    u32 SPRG0;
    u32 SPRG1;
    u32 SPRG2;
    u32 SPRG3;
    u32 DEC;
    u32 IABR;
    u32 EAR;
    u32 DABR;
    u32 PMC1;
    u32 PMC2;
    u32 PMC3;
    u32 PMC4;
    u32 SIA;
    u32 MMCR0;
    u32 MMCR1;
    u32 THRM1;
    u32 THRM2;
    u32 THRM3;
    u32 ICTC;
    u32 L2CR;
    u32 UMMCR2;
    u32 UBAMR;
    u32 UMMCR0;
    u32 UPMC1;
    u32 UPMC2;
    u32 USIA;
    u32 UMMCR1;
    u32 UPMC3;
    u32 UPMC4;
    u32 USDA;
    u32 MMCR2;
    u32 BAMR;
    u32 SDA;
    u32 MSSCR0;
    u32 MSSCR1;
    u32 PIR;
    u32 exceptionID;
    u32 GQR[8];
    u32 HID_G;
    u32 WPAR;
    u32 DMA_U;
    u32 DMA_L;
} Extended1_PPC_6xx_7xx;

/* Paired-single halves of the floating-point registers. */
typedef struct Extended2_PPC_6xx_7xx {
    u32 PSR[32][2];
} Extended2_PPC_6xx_7xx;

typedef struct ProcessorState_PPC_6xx_7xx {
    Default_PPC Default;
    Float_PPC Float;
    Extended1_PPC_6xx_7xx Extended1;
    Extended2_PPC_6xx_7xx Extended2;
    u32 transport_handler_saved_ra;
} ProcessorState_PPC_6xx_7xx;

typedef ProcessorState_PPC_6xx_7xx ProcessorState_PPC;

/* Target registers the nub saves while the target runs, and its stopped and input state. */
typedef struct TRKState_PPC {
    u32 GPR[32];            /* 0x00 */
    u32 LR;                 /* 0x80 */
    u32 CTR;                /* 0x84 */
    u32 XER;                /* 0x88 */
    u32 MSR;                /* 0x8C */
    u32 DAR;                /* 0x90 */
    u32 DSISR;              /* 0x94 */
    BOOL isStopped;         /* 0x98 */
    BOOL inputActivated;    /* 0x9C */
    u8* inputPendingPtr;    /* 0xA0 */
} TRKState_PPC;

typedef TRKState_PPC TRKState;

/* Extended1 registers the next resume must write back. */
typedef struct ProcessorRestoreFlags_PPC {
    u8 TBR;
    u8 DEC;
    u8 linker_padding[0x9 - 0x2];
} ProcessorRestoreFlags_PPC;

typedef enum ValidMemoryOptions {
    VALIDMEM_Readable,
    VALIDMEM_Writeable
} ValidMemoryOptions;

typedef enum MemoryAccessOptions {
    MEMACCESS_UserMemory,
    MEMACCESS_DebuggerMemory
} MemoryAccessOptions;

extern ProcessorState_PPC gTRKCPUState;
extern TRKState gTRKState;
extern Default_PPC gTRKSaveState;
extern ProcessorRestoreFlags_PPC gTRKRestoreFlags;

DSError TRKInitializeTarget(void);
DSError TRKTargetContinue(void);
DSError TRKTargetInterrupt(TRKEvent* event);
DSError TRKTargetSupportRequest(void);
BOOL TRKTargetStopped(void);
void TRKTargetSetInputPendingPtr(void* ptr);
void TRKTargetSetStopped(unsigned int stopped);
void TRKSwapAndGo(void);

u32 __TRK_get_MSR(void);
void __TRK_set_MSR(u32 msr);
DSError TRKValidMemory32(const void* addr, size_t length, ValidMemoryOptions readWriteable);
DSError TRKTargetAccessMemory(void* data, u32 start, size_t* length, MemoryAccessOptions accessOptions, BOOL read);
DSError TRKTargetAccessDefault(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read);
DSError TRKTargetAccessFP(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read);
DSError TRKTargetAccessExtended1(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registersLengthPtr, BOOL read);
DSError TRKTargetAccessExtended2(u32 firstRegister, u32 lastRegister, TRKBuffer* b, size_t* registerStorageSize, BOOL read);
void TRKInterruptHandler(void);
void TRKPostInterruptEvent(void);
void TRKInterruptHandlerEnableInterrupts(void);
DSError TRKTargetAddStopInfo(TRKBuffer* b);
DSError TRKTargetAddExceptionInfo(TRKBuffer* b);
DSError TRKTargetSingleStep(u32 count, BOOL stepOver);
DSError TRKTargetStepOutOfRange(u32 rangeStart, u32 rangeEnd, BOOL stepOver);
u32 TRKTargetGetPC(void);
DSError TRKTargetStop(void);
DSError TRKPPCAccessSPR(void* value, u32 spr_register_num, BOOL read);
DSError TRKPPCAccessPairedSingleRegister(void* srcDestPtr, u32 psr, BOOL read);
DSError TRKPPCAccessFPRegister(void* srcDestPtr, u32 fpr, BOOL read);
DSError TRKPPCAccessSpecialReg(void* value, u32* access_func, BOOL read);
void ReadFPSCR(f64* fpscr);
void WriteFPSCR(f64* fpscr);
u32* ConvertAddress(u32 addr);

#ifdef __cplusplus
}
#endif

#endif /* TRK_TARGIMPL_H */
