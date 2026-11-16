// Reference: doldecomp/ogws src/revolution/AX/AXOut.c (AXRmtGetSamplesLeft is expanded into its callers, as in retail)
#include <revolution/ai.h>
#include <revolution/ax.h>
#include <revolution/ax/__ax.h>
#include <revolution/dsp.h>
#include <revolution/os.h>

// Output uses a ring buffer.
#define OUT_RING_MAX 3
// Each remote output buffer holds ten frames.
#define RMT_FRAME_MAX 10

// A command list holds up to 64 commands of two bytes each.
#define AX_CL_SIZE (64 * sizeof(u16))

// DSP mail that announces the size of the command list; the list address follows.
#define DSP_MAIL_CLSIZE(size) (0xBABE << 16 | (size))
// Where the DSP program keeps its data image in DSP data memory.
#define DRAM_IMAGE_DSP_ADDR 0x0CD2

// DSP cycles spent per audio sample, and the extra cycles budgeted when the output is not a ring buffer.
#define CYCLES_PER_SAMPLE 3797
#define SYNC_PBS_CYCLES 24400

// Copies the frame profile out one element at a time.
#define FUNNY_COPY(dst, src, type, n)                    \
    do {                                                 \
        type* __dst = (type*)(dst);                        \
        type* __src = (type*)(src);                        \
        int i;                                           \
                                                         \
        for (i = 0; i < (n) / sizeof(type); i++) {         \
            *__dst = *__src;                             \
            __dst++;                                     \
            __src++;                                     \
        }                                                \
    } while (0)

AXPROFILE __AXLocalProfile;

static volatile u32 __AXOutFrame; // volatile: SDK declaration (ogws AXOut.c); retail reloads it after the increment in __AXOutNewFrame
static u32 __AXAiDmaFrame;
static u32 __AXOutDspReady;
static AXOutCallback __AXUserFrameCallback;
static volatile BOOL __AXDSPInitFlag; // volatile: the wait loop in __AXOutInitDSP re-reads it (set by the DSP init callback)
static volatile BOOL __AXDSPDoneFlag; // volatile: SDK declaration (ogws AXOut.c); set by the DSP done callback
static BOOL __AXDebugSteppingMode;
static OSThreadQueue __AXOutThreadQueue;
static u32 __AXOutputBufferMode;
static s32 __AXRmtBuffLen;
static s32 __AXRmtDspPtr;
static s32 __AXRmtCpuPtr;
static AXExceedCallback __AXExceedCallback;

static s16 __AXRmtOutBuffer[AX_RMT_MAX][AX_SAMPLES_PER_FRAME_RMT * RMT_FRAME_MAX]
    __attribute__((aligned(32)));
static u8 __AXOutSBuffer[768] __attribute__((aligned(32)));
static u32 __AXOutBuffer[OUT_RING_MAX][AX_SAMPLES_PER_FRAME] __attribute__((aligned(32)));

static DSPTask __AXDSPTask __attribute__((aligned(32)));
static u8 __AXDramImage[64] __attribute__((aligned(32)));

static void __AXDSPInitCallback(void* task);
static void __AXDSPResumeCallback(void* task);
static void __AXDSPDoneCallback(void* task);
static void __AXDSPRequestCallback(void* task);

/*
 * __AXOutNewFrame
 *
 * Builds one audio frame: syncs the voices, hands the command list to the DSP, runs the aux and user callbacks,
 * and starts the next frame's mix. Returns the DSP cycles the frame has used so far.
 */
u32 __AXOutNewFrame(void) {
    AXPROFILE* current;
    u32 cycles;
    void* list;
    void* rmt[AX_RMT_MAX];
    s32 ptr;

    __AXLocalProfile.timeBegin = OSGetTime();

    cycles = (AX_SAMPLES_PER_FRAME - (AIGetDMABytesLeft() / AX_SAMPLE_DEPTH_BYTES)) * CYCLES_PER_SAMPLE;

    if (__AXOutputBufferMode == 1) {
        __AXSyncPBs(0);
    } else {
        __AXSyncPBs(SYNC_PBS_CYCLES);
    }

    __AXPrintStudio();

    list = __AXGetCommandListAddress();
    DSP_SEND_MAIL_SYNC(DSP_MAIL_CLSIZE(AX_CL_SIZE));
    DSP_SEND_MAIL_SYNC(list);

    __AXServiceCallbackStack();

    __AXLocalProfile.timeProcessAuxBegin = OSGetTime();
    __AXProcessAux();
    __AXLocalProfile.timeProcessAuxEnd = OSGetTime();

    __AXLocalProfile.timeUserFrameBegin = OSGetTime();
    // ogws writes `!= NULL` here and below; with retail's codegen only the bare tests give its register choice.
    if (__AXUserFrameCallback) {
        __AXUserFrameCallback();
    }
    __AXLocalProfile.timeUserFrameEnd = OSGetTime();

    rmt[0] = &__AXRmtOutBuffer[0][__AXRmtDspPtr];
    rmt[1] = &__AXRmtOutBuffer[1][__AXRmtDspPtr];
    rmt[2] = &__AXRmtOutBuffer[2][__AXRmtDspPtr];
    rmt[3] = &__AXRmtOutBuffer[3][__AXRmtDspPtr];

    ptr = __AXRmtDspPtr + AX_SAMPLES_PER_FRAME_RMT;
    if (ptr >= __AXRmtBuffLen) {
        ptr = 0;
    }

    if (__AXRmtCpuPtr >= __AXRmtDspPtr && __AXRmtCpuPtr < __AXRmtDspPtr + AX_SAMPLES_PER_FRAME_RMT) {
        __AXRmtCpuPtr = ptr;
    }

    __AXRmtDspPtr = ptr;
    __AXNextFrame(__AXOutSBuffer, __AXOutBuffer[__AXOutFrame], rmt);
    __AXOutFrame++;

    if (__AXOutputBufferMode == 1) {
        __AXOutFrame %= OUT_RING_MAX;
    } else {
        __AXOutFrame &= 1;
        AIInitDMA((u32)__AXOutBuffer[__AXOutFrame], AX_FRAME_SIZE);
    }

    __AXLocalProfile.timeEnd = OSGetTime();
    __AXLocalProfile.numVoices = __AXGetNumVoices();

    current = __AXGetCurrentProfile();
    if (current) {
        FUNNY_COPY(current, &__AXLocalProfile, u8, sizeof(AXPROFILE));
    }

    return cycles;
} // 0x8022E950

/*
 * __AXOutAiCallback
 *
 * Audio DMA callback: does nothing once the DSP program has finished; otherwise starts the next frame once the DSP is ready for it, otherwise asks the DSP to
 * interrupt its current work. In ring buffer mode it also queues the next buffer for the DMA.
 */
void __AXOutAiCallback(void) {
    u32 frame;

    if (__AXDSPDoneFlag == TRUE) {
        return;
    }

    if (__AXOutDspReady == 1) {
        __AXOutDspReady = 0;
        __AXOutNewFrame();
    } else {
        __AXOutDspReady = 2;
        DSPAssertTask(&__AXDSPTask);
    }

    if (__AXOutputBufferMode == 1) {
        AIInitDMA((u32)__AXOutBuffer[__AXAiDmaFrame], AX_FRAME_SIZE);

        frame = __AXAiDmaFrame + 1;
        frame %= OUT_RING_MAX;

        if (frame != __AXOutFrame) {
            __AXAiDmaFrame = frame;
        }
    }
} // 0x8022EBB0

/*
 * __AXDSPInitCallback
 */
static void __AXDSPInitCallback(void* task) {
    __AXDSPInitFlag = TRUE;
} // 0x8022EC60

/*
 * __AXDSPResumeCallback
 *
 * The DSP finished a frame: builds the next one if the audio DMA already asked for it, and reports a frame
 * that ran late to the exceed callback.
 */
static void __AXDSPResumeCallback(void* task) {
    u32 cycles;

    if (__AXOutDspReady == 2) {
        __AXOutDspReady = 0;

        cycles = __AXOutNewFrame();

        if (__AXExceedCallback != NULL) {
            __AXExceedCallback(cycles);
        }
    } else {
        __AXOutDspReady = 1;
    }
} // 0x8022EC70

/*
 * __AXDSPDoneCallback
 */
static void __AXDSPDoneCallback(void* task) {
    __AXDSPDoneFlag = TRUE;
    OSWakeupThread(&__AXOutThreadQueue);
} // 0x8022ECD0

/*
 * __AXDSPRequestCallback
 */
static void __AXDSPRequestCallback(void* task) {
} // 0x8022ECE0

/*
 * __AXOutInitDSP
 *
 * Describes the AX DSP program to the DSP driver, adds the task and waits for the DSP to start it.
 */
void __AXOutInitDSP(void) {
    __AXDSPTask.iram_mmem_addr = axDspSlave;
    __AXDSPTask.iram_length = axDspSlaveLength;
    __AXDSPTask.iram_addr = 0;

    __AXDSPTask.dram_mmem_addr = __AXDramImage;
    __AXDSPTask.dram_length = sizeof(__AXDramImage);
    __AXDSPTask.dram_addr = DRAM_IMAGE_DSP_ADDR;

    __AXDSPTask.dsp_init_vector = axDspInitVector;
    __AXDSPTask.dsp_resume_vector = axDspResumeVector;

    __AXDSPTask.init_cb = __AXDSPInitCallback;
    __AXDSPTask.res_cb = __AXDSPResumeCallback;
    __AXDSPTask.done_cb = __AXDSPDoneCallback;
    __AXDSPTask.req_cb = __AXDSPRequestCallback;

    __AXDSPTask.priority = 0;

    __AXDSPInitFlag = FALSE;
    __AXDSPDoneFlag = FALSE;

    OSInitThreadQueue(&__AXOutThreadQueue);

    if (!DSPCheckInit()) {
        DSPInit();
    }

    DSPAddTask(&__AXDSPTask);
    while (!__AXDSPInitFlag) {
        ;
    }
} // 0x8022ECF0

/*
 * __AXOutInit
 *
 * Clears the output buffers, starts the DSP program, hooks the audio DMA and primes the first frames.
 */
void __AXOutInit(u32 mode) {
    int i;
    u32* dst;
    u32* dst2;
    void* rmt[AX_RMT_MAX];

    __AXOutFrame = 0;
    __AXAiDmaFrame = 0;
    __AXOutputBufferMode = mode;
    __AXDebugSteppingMode = 0;

    for (dst = (u32*)__AXOutBuffer, i = 0; i < sizeof(__AXOutBuffer) / sizeof(u32); i++) {
        *dst++ = 0;
    }
    DCFlushRange(__AXOutBuffer, sizeof(__AXOutBuffer));

    for (dst2 = (u32*)__AXOutSBuffer, i = 0; i < sizeof(__AXOutSBuffer) / sizeof(u32); i++) {
        *dst2++ = 0;
    }
    DCFlushRange(__AXOutSBuffer, sizeof(__AXOutSBuffer));

    for (dst2 = (u32*)__AXRmtOutBuffer, i = 0; i < sizeof(__AXRmtOutBuffer) / sizeof(u32); i++) {
        *dst2++ = 0;
    }
    DCFlushRange(__AXRmtOutBuffer, sizeof(__AXRmtOutBuffer));

    __AXOutInitDSP();
    AIRegisterDMACallback(__AXOutAiCallback);

    rmt[0] = __AXRmtOutBuffer[0];
    rmt[1] = __AXRmtOutBuffer[1];
    rmt[2] = __AXRmtOutBuffer[2];
    rmt[3] = __AXRmtOutBuffer[3];

    __AXRmtCpuPtr = AX_SAMPLES_PER_FRAME_RMT;
    __AXRmtDspPtr = AX_SAMPLES_PER_FRAME_RMT;
    __AXRmtBuffLen = AX_SAMPLES_PER_FRAME_RMT * RMT_FRAME_MAX;

    if (__AXOutputBufferMode == 1) {
        __AXNextFrame(__AXOutSBuffer, __AXOutBuffer[2], rmt);
    } else {
        __AXNextFrame(__AXOutSBuffer, __AXOutBuffer[1], rmt);
    }

    __AXOutDspReady = 1;
    __AXUserFrameCallback = NULL;

    if (__AXOutputBufferMode == 1) {
        AIInitDMA((u32)__AXOutBuffer[__AXAiDmaFrame], AX_FRAME_SIZE);
        __AXAiDmaFrame++;
    } else {
        AIInitDMA((u32)__AXOutBuffer[__AXOutFrame], AX_FRAME_SIZE);
    }

    AIStartDMA();
    __AXExceedCallback = NULL;
} // 0x8022EDE0

/*
 * __AXOutQuit (invented name)
 *
 * Stops the AX DSP program and the audio DMA, and clears the callbacks.
 */
void __AXOutQuit(void) {
    BOOL enabled = OSDisableInterrupts();

    __AXUserFrameCallback = NULL;

    DSPCancelTask(&__AXDSPTask);
    OSSleepThread(&__AXOutThreadQueue);

    AIRegisterDMACallback(NULL);
    AIStopDMA();

    __AXExceedCallback = NULL;
    OSRestoreInterrupts(enabled);
} // 0x8022EFE0

/*
 * AXRegisterCallback
 *
 * Sets the callback run once per audio frame; returns the previous one.
 */
AXOutCallback AXRegisterCallback(AXOutCallback callback) {
    AXOutCallback old = __AXUserFrameCallback;
    BOOL enabled = OSDisableInterrupts();

    __AXUserFrameCallback = callback;

    OSRestoreInterrupts(enabled);
    return old;
} // 0x8022F050

/*
 * AXRmtGetSamplesLeft
 *
 * Number of remote output samples the DSP has produced that the CPU has not read yet.
 */
s32 AXRmtGetSamplesLeft(void) {
    s32 samples = __AXRmtDspPtr - (AX_SAMPLES_PER_FRAME_RMT * 2);

    if (samples < 0) {
        samples += __AXRmtBuffLen;
    }

    samples -= __AXRmtCpuPtr;

    if (samples < 0) {
        samples += __AXRmtBuffLen;
    }

    return samples;
}

/*
 * AXRmtGetSamples
 *
 * Copies up to num samples of a remote channel to out; returns the number copied.
 */
s32 AXRmtGetSamples(s32 chan, s16* out, s32 num) {
    s32 avail = AXRmtGetSamplesLeft();
    s32 actual = num > avail ? avail : num;
    s32 ptr = __AXRmtCpuPtr;
    int i;

    for (i = 0; i < actual; i++) {
        *out++ = __AXRmtOutBuffer[chan][ptr];

        if (++ptr >= __AXRmtBuffLen) {
            ptr = 0;
        }
    }

    DCInvalidateRange(__AXRmtOutBuffer[chan], __AXRmtBuffLen * sizeof(s16));
    return actual;
} // 0x8022F0A0

/*
 * AXRmtAdvancePtr
 *
 * Skips num samples of the remote output (at most those available); returns the number skipped.
 */
s32 AXRmtAdvancePtr(s32 num) {
    s32 avail = AXRmtGetSamplesLeft();
    s32 actual = num > avail ? avail : num;

    __AXRmtCpuPtr += actual;

    if (__AXRmtCpuPtr >= __AXRmtBuffLen) {
        __AXRmtCpuPtr -= __AXRmtBuffLen;
    }

    return actual;
} // 0x8022F170
