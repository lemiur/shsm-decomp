// Reference: zeldaret/tp libs/revolution/src/ai/ai.c (the data declaration order is doldecomp/ogws src/revolution/AI/ai.c)
#include <revolution/ai.h>
#include <revolution/dsp.h>
#include <revolution/hardware.h>
#include <revolution/os.h>

/* __AIRegs word indices (invented names). */
#define AI_REG_CONTROL    0
#define AI_REG_VOLUME     1
#define AI_REG_SAMPLE_CNT 2
#define AI_REG_INT_TIMING 3

/* AI_REG_CONTROL bits (invented names). */
#define AI_CONTROL_PLAY             0x01 /* start/stop the stream */
#define AI_CONTROL_AFR              0x02 /* stream sample rate: set 48 kHz, clear 32 kHz */
#define AI_CONTROL_INT_MASK         0x04
#define AI_CONTROL_INT_VALID        0x10
#define AI_CONTROL_SAMPLE_CNT_RESET 0x20
#define AI_CONTROL_DSP_RATE         0x40 /* set: DSP samples at 32 kHz */

const char* __AIVersion = "<< RVL_SDK - AI \trelease build: Feb 27 2009 10:01:30 (0x4302_145) >>";

static AIDCallback __AID_Callback;
static u8* __CallbackStack;
static u8* __OldStack;
static OSTime bound_32KHz;
static OSTime bound_48KHz;
static OSTime min_wait;
static OSTime max_wait;
static OSTime buffer;
static BOOL __AID_Active;
static BOOL __AI_init_flag;

static void __AIDHandler(__OSInterrupt interrupt, OSContext* context);
static void __AICallbackStackSwitch(void* cb);
static void __AI_SRC_INIT(void);

/*
 * AIRegisterDMACallback
 *
 * Installs the callback run when the audio DMA buffer has been consumed and returns the previous one.
 */
AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    AIDCallback old_callback;
    BOOL old;

    old_callback = __AID_Callback;
    old = OSDisableInterrupts();
    __AID_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
} // 0x8022CA60

/*
 * AIInitDMA
 *
 * Programs the audio DMA start address and length (a multiple of 32 bytes).
 */
void AIInitDMA(u32 start_addr, u32 length) {
    BOOL old;

    old = OSDisableInterrupts();
    __DSPRegs[DSP_REG_AI_DMA_START_H] = (__DSPRegs[DSP_REG_AI_DMA_START_H] & ~0x1FFF) | (start_addr >> 16);
    __DSPRegs[DSP_REG_AI_DMA_START_L] = (__DSPRegs[DSP_REG_AI_DMA_START_L] & ~0xFFE0) | (start_addr & 0xFFFF);
    __DSPRegs[DSP_REG_AI_DMA_CTRL] =
        (__DSPRegs[DSP_REG_AI_DMA_CTRL] & ~DSP_AI_DMA_LENGTH_MASK) | (u16)((length / AI_DMA_BLOCK_SIZE) & 0xFFFF);
    OSRestoreInterrupts(old);
} // 0x8022CAB0

BOOL AIGetDMAEnableFlag(void) {
    return (__DSPRegs[DSP_REG_AI_DMA_CTRL] & DSP_AI_DMA_ENABLE) >> 15;
}

void AIStartDMA(void) {
    __DSPRegs[DSP_REG_AI_DMA_CTRL] = __DSPRegs[DSP_REG_AI_DMA_CTRL] | DSP_AI_DMA_ENABLE;
} // 0x8022CB30

void AIStopDMA(void) {
    __DSPRegs[DSP_REG_AI_DMA_CTRL] = __DSPRegs[DSP_REG_AI_DMA_CTRL] & ~DSP_AI_DMA_ENABLE;
} // 0x8022CB50

u32 AIGetDMABytesLeft(void) {
    return (__DSPRegs[DSP_REG_AI_DMA_LEFT] & DSP_AI_DMA_LENGTH_MASK) * AI_DMA_BLOCK_SIZE;
} // 0x8022CB70

u32 AIGetDMAStartAddr(void) {
    return ((__DSPRegs[DSP_REG_AI_DMA_START_H] & 0x1FFF) << 16) | (__DSPRegs[DSP_REG_AI_DMA_START_L] & 0xFFE0);
} // 0x8022CB80

u32 AIGetDMALength(void) {
    return (__DSPRegs[DSP_REG_AI_DMA_CTRL] & DSP_AI_DMA_LENGTH_MASK) * AI_DMA_BLOCK_SIZE;
} // 0x8022CBA0

BOOL AICheckInit(void) {
    return __AI_init_flag;
} // 0x8022CBB0

void AISetDSPSampleRate(u32 rate) {
    BOOL old;

    if (rate != AIGetDSPSampleRate()) {
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_DSP_RATE);
        if (rate == AI_SAMPLERATE_32KHZ) {
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            __AIRegs[AI_REG_CONTROL] |= AI_CONTROL_DSP_RATE;
            OSRestoreInterrupts(old);
        }
    }
}

u32 AIGetDSPSampleRate(void) {
    return ((__AIRegs[AI_REG_CONTROL] & AI_CONTROL_DSP_RATE) >> 6) ^ 1;
}

/*
 * AIInit
 *
 * Resets the audio interface, selects the 32 kHz DSP rate and hooks the audio DMA interrupt. `stack` is the
 * optional 8-byte-aligned stack the DMA callback runs on.
 */
void AIInit(u8* stack) {
    u32 reg;

    if (__AI_init_flag != TRUE) {
        OSRegisterVersion(__AIVersion);

        bound_32KHz = OSNanosecondsToTicks(31524);
        bound_48KHz = OSNanosecondsToTicks(42024);
        min_wait = OSNanosecondsToTicks(42000);
        max_wait = OSNanosecondsToTicks(63000);
        buffer = OSNanosecondsToTicks(3000);

        reg = __AIRegs[AI_REG_CONTROL];
        reg &= ~(AI_CONTROL_PLAY | AI_CONTROL_INT_MASK | AI_CONTROL_INT_VALID);
        __AIRegs[AI_REG_CONTROL] = reg;
        __AIRegs[AI_REG_VOLUME] = 0;
        __AIRegs[AI_REG_INT_TIMING] = 0;
        __AIRegs[AI_REG_CONTROL] =
            (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_SAMPLE_CNT_RESET) | AI_CONTROL_SAMPLE_CNT_RESET;

        AISetDSPSampleRate(AI_SAMPLERATE_32KHZ);

        __AID_Callback = NULL;
        __CallbackStack = stack;

        __OSSetInterruptHandler(__OS_INTERRUPT_DSP_AI, __AIDHandler);
        __OSUnmaskInterrupts(OS_INTERRUPTMASK_DSP_AI);
        __AI_init_flag = TRUE;
    }
} // 0x8022CBC0

void AIReset(void) {
    __AI_init_flag = FALSE;
}

/*
 * __AIDHandler
 *
 * Audio DMA interrupt: acknowledges it and runs the registered callback, on the callback stack when one was given.
 */
static void __AIDHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    u16 tmp;

    tmp = __DSPRegs[DSP_REG_CSR];
    tmp = (tmp & ~(DSP_CSR_DSP_INT | DSP_CSR_ARAM_INT)) | DSP_CSR_AI_DMA_INT;
    __DSPRegs[DSP_REG_CSR] = tmp;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AID_Callback && !__AID_Active) {
        __AID_Active = TRUE;
        if (__CallbackStack) {
            __AICallbackStackSwitch(__AID_Callback);
        } else {
            __AID_Callback();
        }
        __AID_Active = FALSE;
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
} // 0x8022CD40

/*
 * __AICallbackStackSwitch
 *
 * Calls `cb` with the stack pointer moved to the top of __CallbackStack, then restores the old stack.
 */
static asm void __AICallbackStackSwitch(register void* cb) {
    fralloc
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    stw r1, 0x0(r5)
    lis r5, __CallbackStack@ha
    addi r5, r5, __CallbackStack@l
    lwz r1, 0x0(r5)
    subi r1, r1, 0x8
    mtlr cb
    blrl
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    lwz r1, 0x0(r5)
    frfree
    blr
} // 0x8022CDF0

/*
 * __AI_SRC_INIT
 *
 * Calibrates the sample-rate converter: times the interval between sample counter changes at both rates and
 * waits out the settling time that interval selects.
 */
static void __AI_SRC_INIT(void) {
    OSTime rising_32khz = 0;
    OSTime rising_48khz = 0;
    OSTime diff = 0;
    OSTime temp;
    u32 temp0;
    u32 temp1;
    u32 done = 0;

    temp = 0;

    while (!done) {
        __AIRegs[AI_REG_CONTROL] =
            (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_SAMPLE_CNT_RESET) | AI_CONTROL_SAMPLE_CNT_RESET;
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_AFR) | 0;
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_PLAY) | AI_CONTROL_PLAY;
        temp0 = __AIRegs[AI_REG_SAMPLE_CNT] & 0x7FFFFFFF;
        while (temp0 == (__AIRegs[AI_REG_SAMPLE_CNT] & 0x7FFFFFFF)) {
        }
        rising_32khz = OSGetTime();
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_AFR) | AI_CONTROL_AFR;
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_PLAY) | AI_CONTROL_PLAY;
        temp1 = __AIRegs[AI_REG_SAMPLE_CNT] & 0x7FFFFFFF;
        while (temp1 == (__AIRegs[AI_REG_SAMPLE_CNT] & 0x7FFFFFFF)) {
        }
        rising_48khz = OSGetTime();
        diff = rising_48khz - rising_32khz;
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_AFR) | 0;
        __AIRegs[AI_REG_CONTROL] = (__AIRegs[AI_REG_CONTROL] & ~AI_CONTROL_PLAY) | 0;
        if (diff < bound_32KHz - buffer) {
            temp = min_wait;
            done = 1;
        } else if (diff >= bound_32KHz + buffer && diff < bound_48KHz - buffer) {
            temp = max_wait;
            done = 1;
        } else {
            done = 0;
        }
    }
    while (rising_48khz + temp > OSGetTime()) {
    }
} // 0x8022CE60
