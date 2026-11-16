#include <revolution/base.h>
#include <revolution/db.h>
#include <revolution/os.h>

/*
 * L1, L2 and locked-cache control. The range operations work on 32-byte
 * cache blocks and are asm functions in the SDK.
 */

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void DCEnable(void) {
    nofralloc
    sync
    mfspr r3, HID0
    ori r3, r3, 0x4000      // HID0[DCE]
    mtspr HID0, r3
    blr
} // 0x80212F80

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void DCInvalidateRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi r5, addr, 27
    add nBytes, nBytes, r5
    addi nBytes, nBytes, 31
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbi r0, addr
    addi addr, addr, 32
    bdnz loop
    blr
} // 0x80212FA0

/* Ends with sc where the GameCube SDK has sync: the syscall flushes the bus. */
// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void DCFlushRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi r5, addr, 27
    add nBytes, nBytes, r5
    addi nBytes, nBytes, 31
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbf r0, addr
    addi addr, addr, 32
    bdnz loop
    sc
    blr
} // 0x80212FD0

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void DCStoreRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi r5, addr, 27
    add nBytes, nBytes, r5
    addi nBytes, nBytes, 31
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbst r0, addr
    addi addr, addr, 32
    bdnz loop
    sc
    blr
} // 0x80213000

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void DCFlushRangeNoSync(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi r5, addr, 27
    add nBytes, nBytes, r5
    addi nBytes, nBytes, 31
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbf r0, addr
    addi addr, addr, 32
    bdnz loop
    blr
} // 0x80213030

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void DCZeroRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi r5, addr, 27
    add nBytes, nBytes, r5
    addi nBytes, nBytes, 31
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbz r0, addr
    addi addr, addr, 32
    bdnz loop
    blr
} // 0x80213060

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void ICInvalidateRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi r5, addr, 27
    add nBytes, nBytes, r5
    addi nBytes, nBytes, 31
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    icbi r0, addr
    addi addr, addr, 32
    bdnz loop
    sync
    isync
    blr
} // 0x80213090

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void ICFlashInvalidate(void) {
    nofralloc
    mfspr r3, HID0
    ori r3, r3, 0x800       // HID0[ICFI]
    mtspr HID0, r3
    blr
} // 0x802130D0

// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void ICEnable(void) {
    nofralloc
    isync
    mfspr r3, HID0
    ori r3, r3, 0x8000      // HID0[ICE]
    mtspr HID0, r3
    blr
} // 0x802130E0

/*
 * __LCEnable
 *
 * Turns on the locked cache: flushes the first 32 KB of MEM1 out of the
 * data cache, sets HID2[LCE], maps it at 0xE0000000 through DBAT3 and
 * zeroes its 512 blocks.
 */
// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
static asm void __LCEnable(void) {
    nofralloc
    mfmsr r5
    ori r5, r5, 0x1000      // MSR[ME]
    mtmsr r5

    lis r3, 0x8000
    li r4, 0x400
    mtctr r4
flush:
    dcbt r0, r3
    dcbst r0, r3
    addi r3, r3, 32
    bdnz flush

    mfspr r4, SPR_HID2
    oris r4, r4, 0x100F     // HID2[LCE] and the DMA error enables
    mtspr SPR_HID2, r4

    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

    lis r3, 0xE000
    ori r3, r3, 0x0002
    mtdbatl 3, r3
    ori r3, r3, 0x01FE
    mtdbatu 3, r3
    isync

    lis r3, 0xE000
    li r6, 0x200
    mtctr r6
    li r6, 0
zero:
    dcbz_l r6, r3
    addi r3, r3, 32
    bdnz zero

    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    blr
} // 0x80213100

/*
 * LCEnable
 */
void LCEnable(void) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    __LCEnable();
    OSRestoreInterrupts(enabled);
} // 0x802131D0

/*
 * LCDisable
 *
 * Invalidates the locked cache's blocks and clears HID2[LCE].
 */
// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void LCDisable(void) {
    nofralloc
    lis r3, 0xE000
    li r4, 0x200
    mtctr r4
loop:
    dcbi r0, r3
    addi r3, r3, 32
    bdnz loop
    mfspr r4, SPR_HID2
    rlwinm r4, r4, 0, 4, 2  // clear HID2[LCE]
    mtspr SPR_HID2, r4
    blr
} // 0x80213210

/*
 * LCStoreBlocks
 *
 * Queues a DMA of numBlocks blocks (0 means 128) from the locked cache
 * address srcTag to main memory at destAddr.
 */
// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void LCStoreBlocks(register void* destAddr, register void* srcTag, register u32 numBlocks) {
    nofralloc
    rlwinm r6, numBlocks, 30, 27, 31
    clrlwi destAddr, destAddr, 3
    or r6, r6, destAddr
    mtspr DMA_U, r6
    rlwinm r6, numBlocks, 2, 28, 29
    or r6, r6, srcTag
    ori r6, r6, 0x2         // DMA_L[T]: store
    mtspr DMA_L, r6
    blr
} // 0x80213240

/*
 * LCStore
 *
 * Queues DMAs of nBytes from the locked cache to main memory, 128 blocks
 * at a time, and returns the number of DMA transactions queued.
 */
u32 LCStore(void* destAddr, void* srcAddr, u32 nBytes) {
    u32 numBlocks;
    u32 numTransactions;

    numBlocks = (nBytes + 31) / 32;
    numTransactions = (numBlocks + 128 - 1) / 128;

    while (numBlocks) {
        if (numBlocks < 128) {
            LCStoreBlocks(destAddr, srcAddr, numBlocks);
            numBlocks = 0;
        } else {
            LCStoreBlocks(destAddr, srcAddr, 0);
            numBlocks -= 128;
            destAddr = (u8*)destAddr + 4096;
            srcAddr = (u8*)srcAddr + 4096;
        }
    }

    return numTransactions;
} // 0x80213270

/*
 * LCQueueWait
 *
 * Waits until at most len DMA transactions are queued (HID2[DMAQL]).
 */
// ORIGINAL-ASM: cache-block/SPR instructions; an asm function in the public RVL SDK OSCache.c
asm void LCQueueWait(register u32 len) {
    nofralloc
    mfspr r4, SPR_HID2
    rlwinm r4, r4, 8, 28, 31
    cmpw r4, len
    bgt LCQueueWait
    blr
} // 0x80213310

/*
 * L2Enable, L2Disable, L2GlobalInvalidate
 *
 * Only called from __OSCacheInit, where they are inlined; the linker
 * strips the out-of-line copies.
 */
void L2Enable(void) {
    PPCMtl2cr((PPCMfl2cr() | L2CR_L2E) & ~L2CR_L2I);
}

void L2Disable(void) {
    __sync();
    PPCMtl2cr(PPCMfl2cr() & ~L2CR_L2E);
    __sync();
}

void L2GlobalInvalidate(void) {
    L2Disable();
    PPCMtl2cr(PPCMfl2cr() | L2CR_L2I);
    while (PPCMfl2cr() & L2CR_L2IP) {
    }
    PPCMtl2cr(PPCMfl2cr() & ~L2CR_L2I);
    while (PPCMfl2cr() & L2CR_L2IP) {
        DBPrintf(">>> L2 INVALIDATE : SHOULD NEVER HAPPEN\n");
    }
}

/*
 * L2Init
 *
 * Disables and invalidates the L2 cache with address translation on.
 */
static void L2Init(void) {
    u32 oldMSR;

    oldMSR = PPCMfmsr();
    __sync();
    PPCMtmsr(MSR_IR | MSR_DR);
    __sync();
    L2Disable();
    L2GlobalInvalidate();
    PPCMtmsr(oldMSR);
}

/*
 * DMAErrorHandler
 *
 * Machine-check handler: reports and clears locked-cache DMA errors, and
 * halts on any other machine check.
 */
static void DMAErrorHandler(OSError error, OSContext* context, ...) {
    u32 hid2;

    hid2 = PPCMfhid2();

    OSReport("Machine check received\n");
    OSReport("HID2 = 0x%x   SRR1 = 0x%x\n", hid2, context->srr1);
    if (!(hid2 & (HID2_DCHERR | HID2_DNCERR | HID2_DCMERR | HID2_DQOERR)) || !(context->srr1 & SRR1_DMA_BIT)) {
        OSReport("Machine check was not DMA/locked cache related\n");
        OSDumpContext(context);
        PPCHalt();
    }

    OSReport("DMAErrorHandler(): An error occurred while processing DMA.\n");
    OSReport("The following errors have been detected and cleared :\n");
    if (hid2 & HID2_DCHERR) {
        OSReport("\t- Requested a locked cache tag that was already in the cache\n");
    }
    if (hid2 & HID2_DNCERR) {
        OSReport("\t- DMA attempted to access normal cache\n");
    }
    if (hid2 & HID2_DCMERR) {
        OSReport("\t- DMA missed in data cache\n");
    }
    if (hid2 & HID2_DQOERR) {
        OSReport("\t- DMA queue overflowed\n");
    }

    /* The error bits are write-one-to-clear. */
    PPCMthid2(hid2);
} // 0x80213330

/*
 * __OSCacheInit
 *
 * Enables the L1 caches and the L2 cache if the boot code left them off,
 * and installs the locked-cache machine-check handler.
 */
void __OSCacheInit(void) {
    if (!(PPCMfhid0() & HID0_ICE)) {
        ICEnable();
        DBPrintf("L1 i-caches initialized\n");
    }
    if (!(PPCMfhid0() & HID0_DCE)) {
        DCEnable();
        DBPrintf("L1 d-caches initialized\n");
    }

    if (!(PPCMfl2cr() & L2CR_L2E)) {
        L2Init();
        L2Enable();
        DBPrintf("L2 cache initialized\n");
    }

    OSSetErrorHandler(OS_ERROR_MACHINE_CHECK, DMAErrorHandler);
    DBPrintf("Locked cache machine check handler installed\n");
} // 0x80213480
