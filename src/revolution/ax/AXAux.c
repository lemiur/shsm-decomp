// Reference: doldecomp/ogws src/revolution/AX/AXAux.c (__AXAuxQuit follows the later SDK, as in retail)
// The aux callback tests compare against 0: with the NULL macro ((void*)0) the compiler loads the pointer into a different register.
#include <revolution/ax.h>
#include <revolution/ax/__ax.h>
#include <revolution/os.h>
#include <string.h>

/**
 * Ring buffers have three entries:
 * 1. For DSP write
 * 2. For DSP read
 * 3. For CPU read/write
 */
#define AUX_RING_MAX 3

/**
 * 96 samples per frame @ 32-bit depth
 */
#define AUX_CHAN_SIZE (AX_SAMPLE_DEPTH_BYTES * AX_SAMPLES_PER_FRAME)

/**
 * Stereo buffer size: (96 samples/frame, 3 channels)
 * DPL2 buffer size:   (96 samples/frame, 4 channels)
 */
#define STEREO_FRAME_SIZE (AUX_CHAN_SIZE * AX_STEREO_MAX)
#define DPL2_FRAME_SIZE (AUX_CHAN_SIZE * AX_DPL2_MAX)

/**
 * Sample indices for stereo
 */
#define STEREO_SAMPLE_L_BEGIN (AUX_CHAN_SIZE * 0)
#define STEREO_SAMPLE_R_BEGIN (AUX_CHAN_SIZE * 1)
#define STEREO_SAMPLE_S_BEGIN (AUX_CHAN_SIZE * 2)

/**
 * Sample indices for DPL2
 */
#define DPL2_SAMPLE_I_BEGIN (AUX_CHAN_SIZE * 3)
#define DPL2_SAMPLE_L_BEGIN (AUX_CHAN_SIZE * 0)
#define DPL2_SAMPLE_R_BEGIN (AUX_CHAN_SIZE * 1)
#define DPL2_SAMPLE_LS_BEGIN (AUX_CHAN_SIZE * 2)
#define DPL2_SAMPLE_RS_BEGIN (AUX_CHAN_SIZE * 3)

static u8 __AXBufferAuxA[AUX_RING_MAX][DPL2_FRAME_SIZE] ATTRIBUTE_ALIGN(32);
static u8 __AXBufferAuxB[AUX_RING_MAX][DPL2_FRAME_SIZE] ATTRIBUTE_ALIGN(32);
static u8 __AXBufferAuxC[AUX_RING_MAX][STEREO_FRAME_SIZE] ATTRIBUTE_ALIGN(32);

static u8 __clearAuxA[AUX_RING_MAX];
static u8 __clearAuxB[AUX_RING_MAX];
static u8 __clearAuxC[AUX_RING_MAX];

static AXAuxCallback __AXCallbackAuxA;
static AXAuxCallback __AXCallbackAuxB;
static AXAuxCallback __AXCallbackAuxC;

static void* __AXContextAuxA;
static void* __AXContextAuxB;
static void* __AXContextAuxC;

static void* __AXAuxADspWrite;
static void* __AXAuxADspRead;

static void* __AXAuxBDspWrite;
static void* __AXAuxBDspRead;

static void* __AXAuxCDspWrite;
static void* __AXAuxCDspRead;

static u32 __AXAuxDspWritePosition;
static u32 __AXAuxDspReadPosition;
static u32 __AXAuxCpuReadWritePosition;

/*
 * __AXAuxInit
 *
 * Clears the aux callbacks, contexts and ring buffers and sets the initial ring positions.
 */
void __AXAuxInit(void) {
    int i;
    u32* bufferA = (u32*)__AXBufferAuxA;
    u32* bufferB = (u32*)__AXBufferAuxB;
    u32* bufferC = (u32*)__AXBufferAuxC;

    __AXCallbackAuxA = NULL;
    __AXCallbackAuxB = NULL;
    __AXCallbackAuxC = NULL;

    __AXContextAuxA = NULL;
    __AXContextAuxB = NULL;
    __AXContextAuxC = NULL;

    __AXAuxDspWritePosition = 0;
    __AXAuxDspReadPosition = 1;
    __AXAuxCpuReadWritePosition = 2;

    for (i = 0; i < STEREO_FRAME_SIZE / sizeof(u32); i++) {
        *bufferA++ = 0;
        *bufferB++ = 0;
        *bufferC++ = 0;
    }

    memset(__clearAuxA, FALSE, AUX_RING_MAX);
    memset(__clearAuxB, FALSE, AUX_RING_MAX);
    memset(__clearAuxC, FALSE, AUX_RING_MAX);
} // 0x8022D5F0

/*
 * __AXAuxQuit
 *
 * Detaches the aux callbacks (invented name).
 */
void __AXAuxQuit(void) {
    __AXCallbackAuxA = NULL;
    __AXCallbackAuxB = NULL;
    __AXCallbackAuxC = NULL;
} // 0x8022D710

/*
 * __AXGetAuxAInput
 *
 * Returns the buffer the DSP writes aux bus A into, or NULL when no callback is registered.
 */
void __AXGetAuxAInput(void** out) {
    if (__AXCallbackAuxA != 0) {
        *out = __AXBufferAuxA[__AXAuxDspWritePosition];
    } else {
        *out = NULL;
    }
} // 0x8022D730

/*
 * __AXGetAuxAOutput
 *
 * Returns the buffer the DSP reads aux bus A from.
 */
void __AXGetAuxAOutput(void** out) {
    *out = __AXBufferAuxA[__AXAuxDspReadPosition];
} // 0x8022D770

/*
 * __AXGetAuxAInputDpl2
 *
 * Returns the Dolby Pro Logic II input channel of aux bus A.
 */
void __AXGetAuxAInputDpl2(void** out) {
    *out = &__AXBufferAuxA[__AXAuxDspWritePosition][DPL2_SAMPLE_I_BEGIN];
} // 0x8022D790

/*
 * __AXGetAuxAOutputDpl2R
 *
 * Returns the right channel the DSP reads from aux bus A in Dolby Pro Logic II mode.
 */
void __AXGetAuxAOutputDpl2R(void** out) {
    *out = &__AXBufferAuxA[__AXAuxDspReadPosition][DPL2_SAMPLE_R_BEGIN];
} // 0x8022D7B0

/*
 * __AXGetAuxAOutputDpl2Ls
 *
 * Returns the left surround channel the DSP reads from aux bus A in Dolby Pro Logic II mode.
 */
void __AXGetAuxAOutputDpl2Ls(void** out) {
    *out = &__AXBufferAuxA[__AXAuxDspReadPosition][DPL2_SAMPLE_LS_BEGIN];
} // 0x8022D7D0

/*
 * __AXGetAuxAOutputDpl2Rs
 *
 * Returns the right surround channel the DSP reads from aux bus A in Dolby Pro Logic II mode.
 */
void __AXGetAuxAOutputDpl2Rs(void** out) {
    *out = &__AXBufferAuxA[__AXAuxDspReadPosition][DPL2_SAMPLE_RS_BEGIN];
} // 0x8022D7F0

/*
 * __AXGetAuxBInput
 *
 * Returns the buffer the DSP writes aux bus B into, or NULL when no callback is registered.
 */
void __AXGetAuxBInput(void** out) {
    if (__AXCallbackAuxB != 0) {
        *out = __AXBufferAuxB[__AXAuxDspWritePosition];
    } else {
        *out = NULL;
    }
} // 0x8022D810

/*
 * __AXGetAuxBOutput
 *
 * Returns the buffer the DSP reads aux bus B from.
 */
void __AXGetAuxBOutput(void** out) {
    *out = __AXBufferAuxB[__AXAuxDspReadPosition];
} // 0x8022D850

/*
 * __AXGetAuxBInputDpl2
 *
 * Returns the Dolby Pro Logic II input channel of aux bus B.
 */
void __AXGetAuxBInputDpl2(void** out) {
    *out = &__AXBufferAuxB[__AXAuxDspWritePosition][DPL2_SAMPLE_I_BEGIN];
} // 0x8022D870

/*
 * __AXGetAuxBOutputDpl2R
 *
 * Returns the right channel the DSP reads from aux bus B in Dolby Pro Logic II mode.
 */
void __AXGetAuxBOutputDpl2R(void** out) {
    *out = &__AXBufferAuxB[__AXAuxDspReadPosition][DPL2_SAMPLE_R_BEGIN];
} // 0x8022D890

/*
 * __AXGetAuxBOutputDpl2Ls
 *
 * Returns the left surround channel the DSP reads from aux bus B in Dolby Pro Logic II mode.
 */
void __AXGetAuxBOutputDpl2Ls(void** out) {
    *out = &__AXBufferAuxB[__AXAuxDspReadPosition][DPL2_SAMPLE_LS_BEGIN];
} // 0x8022D8B0

/*
 * __AXGetAuxBOutputDpl2Rs
 *
 * Returns the right surround channel the DSP reads from aux bus B in Dolby Pro Logic II mode.
 */
void __AXGetAuxBOutputDpl2Rs(void** out) {
    *out = &__AXBufferAuxB[__AXAuxDspReadPosition][DPL2_SAMPLE_RS_BEGIN];
} // 0x8022D8D0

/*
 * __AXGetAuxCInput
 *
 * Returns the buffer the DSP writes aux bus C into, or NULL when no callback is registered.
 */
void __AXGetAuxCInput(void** out) {
    if (__AXCallbackAuxC != 0) {
        *out = __AXBufferAuxC[__AXAuxDspWritePosition];
    } else {
        *out = NULL;
    }
} // 0x8022D8F0

/*
 * __AXGetAuxCOutput
 *
 * Returns the buffer the DSP reads aux bus C from.
 */
void __AXGetAuxCOutput(void** out) {
    *out = __AXBufferAuxC[__AXAuxDspReadPosition];
} // 0x8022D930

/*
 * __AXProcessAux
 *
 * Once per frame: publishes the DSP buffers of each aux bus, runs the registered aux callbacks on the CPU buffer (or clears it) and advances the ring.
 */
void __AXProcessAux(void) {
    __AXAuxADspWrite = __AXBufferAuxA[__AXAuxDspWritePosition];
    __AXAuxADspRead = __AXBufferAuxA[__AXAuxDspReadPosition];

    __AXAuxBDspWrite = __AXBufferAuxB[__AXAuxDspWritePosition];
    __AXAuxBDspRead = __AXBufferAuxB[__AXAuxDspReadPosition];

    __AXAuxCDspWrite = __AXBufferAuxC[__AXAuxDspWritePosition];
    __AXAuxCDspRead = __AXBufferAuxC[__AXAuxDspReadPosition];

    if (__AXCallbackAuxA != 0) {
        if (__AXClMode == AX_OUTPUT_DPL2) {
            void* chans[AX_DPL2_MAX];
            // clang-format off
            chans[AX_DPL2_L]  = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_L_BEGIN];
            chans[AX_DPL2_R]  = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_R_BEGIN];
            chans[AX_DPL2_LS] = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_LS_BEGIN];
            chans[AX_DPL2_RS] = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_RS_BEGIN];
            // clang-format on

            DCInvalidateRange(chans[0], DPL2_FRAME_SIZE);
            __AXCallbackAuxA(chans, __AXContextAuxA);
            DCFlushRangeNoSync(chans[0], DPL2_FRAME_SIZE);
        } else {
            void* chans[AX_STEREO_MAX];
            // clang-format off
            chans[AX_STEREO_L] = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_L_BEGIN];
            chans[AX_STEREO_R] = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_R_BEGIN];
            chans[AX_STEREO_S] = &__AXBufferAuxA[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_S_BEGIN];
            // clang-format on

            DCInvalidateRange(chans[0], STEREO_FRAME_SIZE);
            __AXCallbackAuxA(chans, __AXContextAuxA);
            DCFlushRangeNoSync(chans[0], STEREO_FRAME_SIZE);
        }
    } else if (__clearAuxA[__AXAuxCpuReadWritePosition]) {
        void* chan = __AXBufferAuxA[__AXAuxCpuReadWritePosition];
        memset(chan, 0, DPL2_FRAME_SIZE);
        DCFlushRange(chan, DPL2_FRAME_SIZE);
        __clearAuxA[__AXAuxCpuReadWritePosition] = FALSE;
    }

    if (__AXCallbackAuxB != 0) {
        if (__AXClMode == AX_OUTPUT_DPL2) {
            void* chans[AX_DPL2_MAX];
            // clang-format off
            chans[AX_DPL2_L]  = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_L_BEGIN];
            chans[AX_DPL2_R]  = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_R_BEGIN];
            chans[AX_DPL2_LS] = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_LS_BEGIN];
            chans[AX_DPL2_RS] = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][DPL2_SAMPLE_RS_BEGIN];
            // clang-format on

            DCInvalidateRange(chans[0], DPL2_FRAME_SIZE);
            __AXCallbackAuxB(chans, __AXContextAuxB);
            DCFlushRangeNoSync(chans[0], DPL2_FRAME_SIZE);
        } else {
            void* chans[AX_STEREO_MAX];
            // clang-format off
            chans[AX_STEREO_L] = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_L_BEGIN];
            chans[AX_STEREO_R] = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_R_BEGIN];
            chans[AX_STEREO_S] = &__AXBufferAuxB[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_S_BEGIN];
            // clang-format on

            DCInvalidateRange(chans[0], STEREO_FRAME_SIZE);
            __AXCallbackAuxB(chans, __AXContextAuxB);
            DCFlushRangeNoSync(chans[0], STEREO_FRAME_SIZE);
        }
    } else if (__clearAuxB[__AXAuxCpuReadWritePosition]) {
        void* chan = __AXBufferAuxB[__AXAuxCpuReadWritePosition];
        memset(chan, 0, DPL2_FRAME_SIZE);

        DCFlushRange(chan, DPL2_FRAME_SIZE);
        __clearAuxB[__AXAuxCpuReadWritePosition] = FALSE;
    }

    if (__AXCallbackAuxC != 0 && __AXClMode != AX_OUTPUT_DPL2) {
        void* chans[AX_STEREO_MAX];
        // clang-format off
        chans[AX_STEREO_L] = &__AXBufferAuxC[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_L_BEGIN];
        chans[AX_STEREO_R] = &__AXBufferAuxC[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_R_BEGIN];
        chans[AX_STEREO_S] = &__AXBufferAuxC[__AXAuxCpuReadWritePosition][STEREO_SAMPLE_S_BEGIN];
        // clang-format on

        DCInvalidateRange(chans[0], STEREO_FRAME_SIZE);
        __AXCallbackAuxC(chans, __AXContextAuxC);
        DCFlushRangeNoSync(chans[0], STEREO_FRAME_SIZE);
    } else if (__AXCallbackAuxC == 0 &&
               __clearAuxC[__AXAuxCpuReadWritePosition]) {
        void* chan = __AXBufferAuxC[__AXAuxCpuReadWritePosition];
        memset(chan, 0, STEREO_FRAME_SIZE);

        DCFlushRange(chan, STEREO_FRAME_SIZE);
        __clearAuxC[__AXAuxCpuReadWritePosition] = FALSE;
    }

    __AXAuxDspWritePosition++;
    __AXAuxDspWritePosition %= AX_STEREO_MAX;

    __AXAuxDspReadPosition++;
    __AXAuxDspReadPosition %= AX_STEREO_MAX;

    __AXAuxCpuReadWritePosition++;
    __AXAuxCpuReadWritePosition %= AX_STEREO_MAX;
} // 0x8022D950

/*
 * AXRegisterAuxACallback
 *
 * Installs the callback run on aux bus A each frame; a NULL callback clears the bus.
 */
void AXRegisterAuxACallback(AXAuxCallback callback, void* context) {
    BOOL enabled = OSDisableInterrupts();

    __AXCallbackAuxA = callback;
    __AXContextAuxA = context;

    if (callback == NULL) {
        memset(&__clearAuxA, TRUE, AX_STEREO_MAX);
    }

    OSRestoreInterrupts(enabled);
} // 0x8022DD00

/*
 * AXRegisterAuxBCallback
 *
 * Installs the callback run on aux bus B each frame; a NULL callback clears the bus.
 */
void AXRegisterAuxBCallback(AXAuxCallback callback, void* context) {
    BOOL enabled = OSDisableInterrupts();

    __AXCallbackAuxB = callback;
    __AXContextAuxB = context;

    if (callback == NULL) {
        memset(&__clearAuxB, TRUE, AX_STEREO_MAX);
    }

    OSRestoreInterrupts(enabled);
} // 0x8022DD70

/*
 * AXRegisterAuxCCallback
 *
 * Installs the callback run on aux bus C each frame; a NULL callback clears the bus.
 */
void AXRegisterAuxCCallback(AXAuxCallback callback, void* context) {
    BOOL enabled = OSDisableInterrupts();

    __AXCallbackAuxC = callback;
    __AXContextAuxC = context;

    if (callback == NULL) {
        memset(&__clearAuxC, TRUE, AX_STEREO_MAX);
    }

    OSRestoreInterrupts(enabled);
} // 0x8022DDE0

/*
 * AXGetAuxACallback
 *
 * Returns the callback and context of aux bus A.
 */
void AXGetAuxACallback(AXAuxCallback* callback, void** context) {
    *callback = __AXCallbackAuxA;
    *context = __AXContextAuxA;
} // 0x8022DE50

/*
 * AXGetAuxBCallback
 *
 * Returns the callback and context of aux bus B.
 */
void AXGetAuxBCallback(AXAuxCallback* callback, void** context) {
    *callback = __AXCallbackAuxB;
    *context = __AXContextAuxB;
}

/*
 * AXGetAuxCCallback
 *
 * Returns the callback and context of aux bus C.
 */
void AXGetAuxCCallback(AXAuxCallback* callback, void** context) {
    *callback = __AXCallbackAuxC;
    *context = __AXContextAuxC;
}
