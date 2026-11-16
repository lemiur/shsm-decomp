#include <revolution/hardware.h>
#include <revolution/os.h>

/* Low-memory words describing the memory sizes; the names are invented. */
#define OS_PHYSICAL_MEM1_SIZE_ADDR  0x3100
#define OS_SIMULATED_MEM1_SIZE_ADDR 0x3104
#define OS_PHYSICAL_MEM2_SIZE_ADDR  0x3118
#define OS_SIMULATED_MEM2_SIZE_ADDR 0x311C
#define OS_MEM2_END_ADDR            0x3120 /* end of the MEM2 the application may use */

#define MEM1_24MB 0x01800000
#define MEM2_64MB 0x04000000
#define MEM2_128MB 0x08000000

/* Highest MEM2 end each MEM2 mapping covers. */
#define MEM2_END_52MB  0x93400000
#define MEM2_END_56MB  0x93800000
#define MEM2_END_112MB 0x97000000

/* MEM interface registers (halfword indices); names invented. */
#define MEM_MARR_CONTROL 8  /* protected-region interrupt enables */
#define MEM_INT_STATUS   15
#define MEM_INT_CLEAR    16
#define MEM_INT_ADDR_LO  17
#define MEM_INT_ADDR_HI  18
#define MEM_SIZE_CONFIG  20

/* Magic value __OSInitMemoryProtection passes to BATConfig; any other spins forever. */
#define BAT_CONFIG_MAGIC 0xBA2CF

/*
 * OSGetPhysicalMem2Size
 *
 * Returns the size of the MEM2 fitted to the console.
 */
u32 OSGetPhysicalMem2Size(void) {
    return *(u32*)OSPhysicalToCached(OS_PHYSICAL_MEM2_SIZE_ADDR);
} // 0x80217FB0

/*
 * OSGetConsoleSimulatedMem1Size
 *
 * Returns the MEM1 size the console simulates.
 */
u32 OSGetConsoleSimulatedMem1Size(void) {
    return *(u32*)OSPhysicalToCached(OS_SIMULATED_MEM1_SIZE_ADDR);
} // 0x80217FC0

/*
 * OSGetConsoleSimulatedMem2Size
 *
 * Returns the MEM2 size the console simulates.
 */
u32 OSGetConsoleSimulatedMem2Size(void) {
    return *(u32*)OSPhysicalToCached(OS_SIMULATED_MEM2_SIZE_ADDR);
} // 0x80217FD0

/*
 * OnShutdown
 *
 * On the final shutdown pass, disables the protected-region interrupts.
 */
static BOOL OnShutdown(BOOL final, u32 event) {
    if (final) {
        __MEMRegs[MEM_MARR_CONTROL] = 0xFF;
        __OSMaskInterrupts(OS_INTERRUPTMASK_MEM_0 | OS_INTERRUPTMASK_MEM_1 | OS_INTERRUPTMASK_MEM_2 |
                           OS_INTERRUPTMASK_MEM_3);
    }
    return TRUE;
} // 0x80217FE0

static OSShutdownFunctionInfo ShutdownFunctionInfo = {OnShutdown, 127};

/*
 * MEMIntrruptHandler
 *
 * Reports a protected-region access to the OS_ERROR_PROTECTION handler,
 * with the cause and the faulting address.
 */
static void MEMIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    u32 cause;
    u32 addr;

    cause = __MEMRegs[MEM_INT_STATUS];
    addr = (((u32)__MEMRegs[MEM_INT_ADDR_HI] & 0x3FF) << 16) | __MEMRegs[MEM_INT_ADDR_LO];
    __MEMRegs[MEM_INT_CLEAR] = 0;

    if (__OSErrorTable[OS_ERROR_PROTECTION]) {
        __OSErrorTable[OS_ERROR_PROTECTION](OS_ERROR_PROTECTION, context, cause, addr);
        return;
    }

    __OSUnhandledException(OS_ERROR_PROTECTION, context, cause, addr);
} // 0x80218020

/*
 * ConfigMEM1_24MB
 *
 * Maps MEM1 as 24 MB (BAT0 16 MB, BAT2 8 MB above it), then returns to
 * the caller with translation on.
 * Runs in real mode, entered through RealMode.
 */
// ORIGINAL-ASM: BAT writes and rfi in real mode; an asm function in the public RVL SDK OSMemory.c
static asm void ConfigMEM1_24MB(void) {
    nofralloc
    li r7, 0x0
    lis r4, 0x0
    addi r4, r4, 0x2
    lis r3, 0x8000
    addi r3, r3, 0x1ff
    lis r6, 0x100
    addi r6, r6, 0x2
    lis r5, 0x8100
    addi r5, r5, 0xff
    isync
    mtspr DBAT0U, r7
    mtspr DBAT0L, r4
    mtspr DBAT0U, r3
    isync
    mtspr IBAT0U, r7
    mtspr IBAT0L, r4
    mtspr IBAT0U, r3
    isync
    mtspr DBAT2U, r7
    mtspr DBAT2L, r6
    mtspr DBAT2U, r5
    isync
    mtspr IBAT2U, r7
    mtspr IBAT2L, r6
    mtspr IBAT2U, r5
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
} // 0x80218070


/*
 * ConfigMEM2_52MB
 *
 * Maps MEM2 for a 52 MB arena (DBAT4-DBAT7, IBAT4-IBAT7) and returns with translation on.
 * Runs in real mode, entered through RealMode.
 */
// ORIGINAL-ASM: BAT writes and rfi in real mode; an asm function in the public RVL SDK OSMemory.c
static asm void ConfigMEM2_52MB(void) {
    nofralloc
    li r7, 0x0
    lis r4, 0x1000
    addi r4, r4, 0x2
    lis r3, 0x9000
    addi r3, r3, 0x3ff
    lis r6, 0x1000
    addi r6, r6, 0x2a
    lis r5, 0xd000
    addi r5, r5, 0x7ff
    isync
    mtspr 568, r7
    mtspr 569, r4
    mtspr 568, r3
    isync
    mtspr 560, r7
    mtspr 561, r7
    isync
    mtspr 570, r7
    mtspr 571, r6
    mtspr 570, r5
    isync
    mtspr 562, r7
    mtspr 563, r7
    isync
    lis r4, 0x1200
    addi r4, r4, 0x2
    lis r3, 0x9200
    addi r3, r3, 0x1ff
    lis r6, 0x1300
    addi r6, r6, 0x2
    lis r5, 0x9300
    addi r5, r5, 0x7f
    isync
    mtspr 572, r7
    mtspr 573, r4
    mtspr 572, r3
    isync
    mtspr 564, r7
    mtspr 565, r7
    isync
    mtspr 574, r7
    mtspr 575, r6
    mtspr 574, r5
    isync
    mtspr 566, r7
    mtspr 567, r7
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
} // 0x802180F0

/*
 * ConfigMEM2_56MB
 *
 * Maps MEM2 for a 56 MB arena and returns with translation on.
 * Runs in real mode, entered through RealMode.
 */
// ORIGINAL-ASM: BAT writes and rfi in real mode; an asm function in the public RVL SDK OSMemory.c
static asm void ConfigMEM2_56MB(void) {
    nofralloc
    li r7, 0x0
    lis r4, 0x1000
    addi r4, r4, 0x2
    lis r3, 0x9000
    addi r3, r3, 0x3ff
    lis r6, 0x1000
    addi r6, r6, 0x2a
    lis r5, 0xd000
    addi r5, r5, 0x7ff
    isync
    mtspr 568, r7
    mtspr 569, r4
    mtspr 568, r3
    isync
    mtspr 560, r7
    mtspr 561, r7
    isync
    mtspr 570, r7
    mtspr 571, r6
    mtspr 570, r5
    isync
    mtspr 562, r7
    mtspr 563, r7
    isync
    lis r4, 0x1200
    addi r4, r4, 0x2
    lis r3, 0x9200
    addi r3, r3, 0x1ff
    lis r6, 0x1300
    addi r6, r6, 0x2
    lis r5, 0x9300
    addi r5, r5, 0xff
    isync
    mtspr 572, r7
    mtspr 573, r4
    mtspr 572, r3
    isync
    mtspr 564, r7
    mtspr 565, r7
    isync
    mtspr 574, r7
    mtspr 575, r6
    mtspr 574, r5
    isync
    mtspr 566, r7
    mtspr 567, r7
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
} // 0x802181D0

/*
 * ConfigMEM2_64MB
 *
 * Maps MEM2 as 64 MB and returns with translation on.
 * Runs in real mode, entered through RealMode.
 */
// ORIGINAL-ASM: BAT writes and rfi in real mode; an asm function in the public RVL SDK OSMemory.c
static asm void ConfigMEM2_64MB(void) {
    nofralloc
    li r7, 0x0
    lis r4, 0x1000
    addi r4, r4, 0x2
    lis r3, 0x9000
    addi r3, r3, 0x7ff
    lis r6, 0x1000
    addi r6, r6, 0x2a
    lis r5, 0xd000
    addi r5, r5, 0x7ff
    isync
    mtspr 568, r7
    mtspr 569, r4
    mtspr 568, r3
    isync
    mtspr 560, r7
    mtspr 561, r7
    isync
    mtspr 570, r7
    mtspr 571, r6
    mtspr 570, r5
    isync
    mtspr 562, r7
    mtspr 563, r7
    isync
    mtspr 564, r7
    mtspr 565, r7
    isync
    mtspr 566, r7
    mtspr 567, r7
    isync
    mtspr 572, r7
    mtspr 573, r7
    isync
    mtspr 574, r7
    mtspr 575, r7
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
} // 0x802182B0

/*
 * ConfigMEM2_112MB
 *
 * Maps MEM2 for a 112 MB arena and returns with translation on.
 * Runs in real mode, entered through RealMode.
 */
// ORIGINAL-ASM: BAT writes and rfi in real mode; an asm function in the public RVL SDK OSMemory.c
static asm void ConfigMEM2_112MB(void) {
    nofralloc
    li r7, 0x0
    lis r4, 0x1000
    addi r4, r4, 0x2
    lis r3, 0x9000
    addi r3, r3, 0x7ff
    lis r6, 0x1000
    addi r6, r6, 0x2a
    lis r5, 0xd000
    addi r5, r5, 0xfff
    isync
    mtspr 568, r7
    mtspr 569, r4
    mtspr 568, r3
    isync
    mtspr 560, r7
    mtspr 561, r7
    isync
    mtspr 570, r7
    mtspr 571, r6
    mtspr 570, r5
    isync
    mtspr 562, r7
    mtspr 563, r7
    isync
    lis r4, 0x1400
    addi r4, r4, 0x2
    lis r3, 0x9400
    addi r3, r3, 0x3ff
    lis r6, 0x1600
    addi r6, r6, 0x2
    lis r5, 0x9600
    addi r5, r5, 0x1ff
    isync
    mtspr 572, r7
    mtspr 573, r4
    mtspr 572, r3
    isync
    mtspr 564, r7
    mtspr 565, r7
    isync
    mtspr 574, r7
    mtspr 575, r6
    mtspr 574, r5
    isync
    mtspr 566, r7
    mtspr 567, r7
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
} // 0x80218360

/*
 * ConfigMEM2_128MB
 *
 * Maps MEM2 as 128 MB and returns with translation on.
 * Runs in real mode, entered through RealMode.
 */
// ORIGINAL-ASM: BAT writes and rfi in real mode; an asm function in the public RVL SDK OSMemory.c
static asm void ConfigMEM2_128MB(void) {
    nofralloc
    li r7, 0x0
    lis r4, 0x1000
    addi r4, r4, 0x2
    lis r3, 0x9000
    addi r3, r3, 0xfff
    lis r6, 0x1000
    addi r6, r6, 0x2a
    lis r5, 0xd000
    addi r5, r5, 0xfff
    isync
    mtspr 568, r7
    mtspr 569, r4
    mtspr 568, r3
    isync
    mtspr 560, r7
    mtspr 561, r7
    isync
    mtspr 570, r7
    mtspr 571, r6
    mtspr 570, r5
    isync
    mtspr 562, r7
    mtspr 563, r7
    isync
    mtspr 564, r7
    mtspr 565, r7
    isync
    mtspr 566, r7
    mtspr 567, r7
    isync
    mtspr 572, r7
    mtspr 573, r7
    isync
    mtspr 574, r7
    mtspr 575, r7
    isync
    mfmsr r3
    ori r3, r3, 0x30
    mtsrr1 r3
    mflr r3
    mtsrr0 r3
    rfi
} // 0x80218440

/*
 * RealMode
 *
 * Calls config with address translation off; config returns to the
 * caller of RealMode with translation back on.
 */
// ORIGINAL-ASM: rfi into real mode; an asm function in the public RVL SDK OSMemory.c
static asm void RealMode(register u32 config) {
    nofralloc
    clrlwi config, config, 2
    mtsrr0 config
    mfmsr config
    rlwinm config, config, 0, 28, 25
    mtsrr1 config
    rfi
} // 0x802184F0

/*
 * BATConfig
 *
 * Shrinks MEM1 to 24 MB when the console simulates that, and maps MEM2 to
 * the size the application may use.
 */
static void BATConfig(u32 magic) {
    u32 mem1;
    u32 mem2;
    u32 mem2End;

    mem1 = *(u32*)OSPhysicalToCached(OS_SIMULATED_MEM1_SIZE_ADDR);
    if (mem1 < *(u32*)OSPhysicalToCached(OS_PHYSICAL_MEM1_SIZE_ADDR) && mem1 == MEM1_24MB) {
        DCInvalidateRange(OSPhysicalToCached(MEM1_24MB), MEM1_24MB);
        __MEMRegs[MEM_SIZE_CONFIG] = 2;
    }

    if (mem1 <= MEM1_24MB) {
        RealMode((u32)ConfigMEM1_24MB);
    }

    mem2 = *(u32*)OSPhysicalToCached(OS_SIMULATED_MEM2_SIZE_ADDR);
    mem2End = *(u32*)OSPhysicalToCached(OS_MEM2_END_ADDR);
    if (mem2 <= MEM2_64MB) {
        if (mem2End <= MEM2_END_52MB) {
            RealMode((u32)ConfigMEM2_52MB);
        } else if (mem2End <= MEM2_END_56MB) {
            RealMode((u32)ConfigMEM2_56MB);
        } else {
            RealMode((u32)ConfigMEM2_64MB);
        }
    } else if (mem2 <= MEM2_128MB) {
        if (mem2End <= MEM2_END_112MB) {
            RealMode((u32)ConfigMEM2_112MB);
        } else {
            RealMode((u32)ConfigMEM2_128MB);
        }
    }

    while (magic != BAT_CONFIG_MAGIC) {
    }
} // 0x80218510

/*
 * __OSRestoreCodeExecOnMEM1
 *
 * Restores the 24 MB MEM1 mapping; spins forever unless passed the magic
 * value. Called from OSExec.
 *
 * Name: Gormiti link map (os.a OSMemory.o).
 */
void __OSRestoreCodeExecOnMEM1(u32 magic) {
    RealMode((u32)ConfigMEM1_24MB);
    while (magic != BAT_CONFIG_MAGIC) {
    }
} // 0x80218630

/*
 * __OSInitMemoryProtection
 *
 * Once per boot: clears and masks the protected-region interrupts, installs
 * their handler and the shutdown hook, configures the BATs and unmasks the
 * address error interrupt.
 */
void __OSInitMemoryProtection(void) {
    static BOOL initialized = FALSE;
    BOOL enabled;

    if (!initialized) {
        enabled = OSDisableInterrupts();

        __MEMRegs[MEM_INT_CLEAR] = 0;
        __MEMRegs[MEM_MARR_CONTROL] = 0xFF;
        __OSMaskInterrupts(OS_INTERRUPTMASK_MEM_0 | OS_INTERRUPTMASK_MEM_1 | OS_INTERRUPTMASK_MEM_2 |
                           OS_INTERRUPTMASK_MEM_3);

        __OSSetInterruptHandler(__OS_INTERRUPT_MEM_0, MEMIntrruptHandler);
        __OSSetInterruptHandler(__OS_INTERRUPT_MEM_1, MEMIntrruptHandler);
        __OSSetInterruptHandler(__OS_INTERRUPT_MEM_2, MEMIntrruptHandler);
        __OSSetInterruptHandler(__OS_INTERRUPT_MEM_3, MEMIntrruptHandler);
        __OSSetInterruptHandler(__OS_INTERRUPT_MEM_ADDRESS, MEMIntrruptHandler);
        OSRegisterShutdownFunction(&ShutdownFunctionInfo);

        BATConfig(BAT_CONFIG_MAGIC);

        __OSUnmaskInterrupts(OS_INTERRUPTMASK_MEM_ADDRESS);
        initialized = TRUE;
        OSRestoreInterrupts(enabled);
    }
} // 0x80218680
