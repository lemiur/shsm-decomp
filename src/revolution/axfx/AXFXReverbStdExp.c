// Unit name unconfirmed; modelled on zeldaret/tp libs/revolution/src/axfx/AXFXReverbHiExp.c (no public source for
// the standard reverb exists)
#include <math.h>
#include <string.h>
#include <revolution/axfx.h>
#include <revolution/os.h>

static u32 __EarlySizeTable[8] = {163, 317, 479, 641, 797, 967, 1123, 1283};

static u32 __FilterSizeTable[7][4] = {
    {1789, 1999, 433, 149}, {149, 293, 251, 103}, {947, 1361, 433, 137}, {1279, 1531, 509, 149},
    {1531, 1847, 563, 179}, {1823, 2357, 571, 137}, {1823, 2357, 571, 179}
};

static BOOL __AllocDelayLine(AXFX_REVERBSTD_EXP* reverb);
static void __FreeDelayLine(AXFX_REVERBSTD_EXP* reverb);
static void __BzeroDelayLines(AXFX_REVERBSTD_EXP* reverb);
static BOOL __InitParams(AXFX_REVERBSTD_EXP* reverb);

/*
 * AXFXReverbStdExpGetMemSize
 *
 * Bytes of delay line memory Init allocates. Modelled on the high-quality reverb's SDK GetMemSize (no public
 * declaration of the standard reverb's exists); the game never calls it, so the linker stripped it, but its 32000 is
 * still the first float literal of retail's constant pool, ahead of Init's 0.0f. The body is reconstructed from
 * Init's allocation sizes.
 */
u32 AXFXReverbStdExpGetMemSize(const AXFX_REVERBSTD_EXP* reverb) {
    u32 earlyLength = __EarlySizeTable[8 - 1];
    u32 preDelayLength = (u32)(reverb->preDelayTimeMax * 32000);
    u32 combLength = __FilterSizeTable[6][0] + __FilterSizeTable[6][1];
    u32 allpassLength = __FilterSizeTable[6][2] + __FilterSizeTable[6][3];

    return 3 * sizeof(f32) * (earlyLength + preDelayLength + combLength + allpassLength);
}

BOOL AXFXReverbStdExpInit(AXFX_REVERBSTD_EXP* reverb) {
    u32 i;
    BOOL result = TRUE;
    BOOL mask = OSDisableInterrupts();

    reverb->active = 1;

    if (reverb->preDelayTimeMax < 0.0f) {
        AXFXReverbStdExpShutdown(reverb);
        OSRestoreInterrupts(mask);
        return FALSE;
    }

    reverb->earlyMaxLength = __EarlySizeTable[8 - 1];
    reverb->preDelayMaxLength = (u32)(reverb->preDelayTimeMax * 32000);

    for (i = 0; i < 2; i++) {
        reverb->combMaxLength[i] = __FilterSizeTable[6][i];
    }

    for (i = 0; i < 2; i++) {
        reverb->allpassMaxLength[i] = __FilterSizeTable[6][2 + i];
    }

    result = __AllocDelayLine(reverb);
    if (result == FALSE) {
        AXFXReverbStdExpShutdown(reverb);
        OSRestoreInterrupts(mask);
        return FALSE;
    }

    __BzeroDelayLines(reverb);
    result = __InitParams(reverb);
    if (result == FALSE) {
        AXFXReverbStdExpShutdown(reverb);
        OSRestoreInterrupts(mask);
        return FALSE;
    }

    reverb->active &= ~1;
    OSRestoreInterrupts(mask);
    return TRUE;
}

void AXFXReverbStdExpShutdown(AXFX_REVERBSTD_EXP* reverb) {
    BOOL mask = OSDisableInterrupts();
    reverb->active |= 1;
    __FreeDelayLine(reverb);
    OSRestoreInterrupts(mask);
}

/*
 * AXFXReverbStdExpCallback
 *
 * Runs the reverb over one frame of the three aux buses: per channel an early-reflection tap, the pre-delay, two
 * parallel combs, two allpasses with a one-pole low-pass between them, then the early/fused mix. The delay line
 * positions are read once per sample and advanced after all three channels.
 * The two comb lines are indexed through the reverb directly while the allpass stages share one line pointer:
 * retail's register numbering needs exactly this (a comb line local, or one local per allpass stage, renumbers the
 * loop temporaries).
 * 0x80232B10
 */
void AXFXReverbStdExpCallback(AXFX_BUFFERUPDATE* bufferUpdate, AXFX_REVERBSTD_EXP* reverb) {
    u32 ch;
    u32 samp;
    s32* input[3];
    f32 data;
    f32* earlyLine;
    f32 earlyOut;
    f32* preDelayLine;
    f32 preDelayOut;
    f32 filterOut;
    f32 combOutOne;
    f32 combOutTwo;
    f32* allpass;
    f32 outTmp;
    f32 allpassIn;
    f32 lpfOut;
    f32 fusedOut;
    f32 earlyCoef;
    f32 combCoef0;
    f32 combCoef1;
    f32 allpassCoef;
    f32 lpfCoef1;
    f32 lpfCoef2;
    f32 earlyGain;
    f32 fusedGain;
    u32 earlyPos, preDelayPos, combPos0, combPos1, allpassPos0, allpassPos1;
    s32* inBusData[3];
    s32* outBusData[3];

    if (reverb->active != 0) {
        reverb->active &= ~2;
        return;
    }

    input[0] = bufferUpdate->left;
    input[1] = bufferUpdate->right;
    input[2] = bufferUpdate->surround;

    if (reverb->busIn != NULL) {
        inBusData[0] = reverb->busIn->left;
        inBusData[1] = reverb->busIn->right;
        inBusData[2] = reverb->busIn->surround;
    }

    if (reverb->busOut != NULL) {
        outBusData[0] = reverb->busOut->left;
        outBusData[1] = reverb->busOut->right;
        outBusData[2] = reverb->busOut->surround;
    }

    lpfCoef1 = 1.0f - reverb->lpfCoef;
    lpfCoef2 = reverb->lpfCoef;
    earlyGain = reverb->earlyGain * 0.6f;
    fusedGain = reverb->fusedGain * 0.6f;
    earlyCoef = reverb->earlyCoef;
    combCoef0 = reverb->combCoef[0];
    combCoef1 = reverb->combCoef[1];
    allpassCoef = reverb->allpassCoef;

    for (samp = 0; samp < 96; samp++) {
        earlyPos = reverb->earlyPos;
        preDelayPos = reverb->preDelayPos;
        combPos0 = reverb->combPos[0];
        combPos1 = reverb->combPos[1];
        allpassPos0 = reverb->allpassPos[0];
        allpassPos1 = reverb->allpassPos[1];

        for (ch = 0; ch < 3; ch++) {
            if (reverb->busIn != NULL) {
                data = (f32)(*(input[ch]) + *(inBusData[ch]++));
            } else {
                data = (f32)(*input[ch]);
            }

            earlyLine = reverb->earlyLine[ch];
            earlyOut = earlyLine[earlyPos];
            earlyLine[earlyPos] = data + earlyOut * earlyCoef;

            if (reverb->preDelayLength != 0) {
                preDelayLine = reverb->preDelayLine[ch];
                preDelayOut = preDelayLine[preDelayPos];
                preDelayLine[preDelayPos] = data;
            } else {
                preDelayOut = data;
            }

            combOutOne = reverb->combLine[ch][0][combPos0];
            reverb->combLine[ch][0][combPos0] = preDelayOut + (combOutOne * combCoef0);

            combOutTwo = reverb->combLine[ch][1][combPos1];
            reverb->combLine[ch][1][combPos1] = preDelayOut + (combOutTwo * combCoef1);

            filterOut = combOutOne + combOutTwo;

            allpass = reverb->allpassLine[ch][0];
            outTmp = allpass[allpassPos0];
            allpassIn = filterOut + outTmp * allpassCoef;
            allpass[allpassPos0] = allpassIn;
            filterOut = outTmp - allpassIn * allpassCoef;

            lpfOut = lpfCoef1 * filterOut + lpfCoef2 * reverb->lastLpfOut[ch];
            reverb->lastLpfOut[ch] = lpfOut;

            allpass = reverb->allpassLine[ch][1];
            outTmp = allpass[allpassPos1];
            allpassIn = lpfOut + outTmp * allpassCoef;
            allpass[allpassPos1] = allpassIn;
            fusedOut = outTmp - allpassIn * allpassCoef;

            fusedOut = earlyOut * earlyGain + fusedOut * fusedGain;

            *(input[ch]++) = (s32)(fusedOut * reverb->outGain);

            if (reverb->busOut != NULL) {
                *(outBusData[ch]++) = (s32)(fusedOut * reverb->sendGain);
            }
        }

        if (++reverb->earlyPos >= reverb->earlyLength) {
            reverb->earlyPos = 0;
        }

        if (reverb->preDelayLength != 0) {
            if (++reverb->preDelayPos >= reverb->preDelayLength) {
                reverb->preDelayPos = 0;
            }
        }

        if (++reverb->combPos[0] >= reverb->combLength[0]) {
            reverb->combPos[0] = 0;
        }

        if (++reverb->combPos[1] >= reverb->combLength[1]) {
            reverb->combPos[1] = 0;
        }

        if (++reverb->allpassPos[0] >= reverb->allpassLength[0]) {
            reverb->allpassPos[0] = 0;
        }

        if (++reverb->allpassPos[1] >= reverb->allpassLength[1]) {
            reverb->allpassPos[1] = 0;
        }
    }
}

static BOOL __AllocDelayLine(AXFX_REVERBSTD_EXP* reverb) {
    u32 ch, i;

    for (ch = 0; ch < 3; ch++) {
        reverb->earlyLine[ch] = (f32*)__AXFXAlloc(sizeof(f32) * reverb->earlyMaxLength);
        if (reverb->earlyLine[ch] == NULL)
            return FALSE;

        if (reverb->preDelayMaxLength != 0) {
            reverb->preDelayLine[ch] = (f32*)__AXFXAlloc(sizeof(f32) * reverb->preDelayMaxLength);
            if (reverb->preDelayLine[ch] == NULL)
                return FALSE;
        } else {
            reverb->preDelayLine[ch] = NULL;
        }

        for (i = 0; i < 2; i++) {
            reverb->combLine[ch][i] = (f32*)__AXFXAlloc(sizeof(f32) * reverb->combMaxLength[i]);
            if (reverb->combLine[ch][i] == NULL)
                return FALSE;
        }

        for (i = 0; i < 2; i++) {
            reverb->allpassLine[ch][i] = (f32*)__AXFXAlloc(sizeof(f32) * reverb->allpassMaxLength[i]);
            if (reverb->allpassLine[ch][i] == NULL)
                return FALSE;
        }
    }

    return TRUE;
}

static void __BzeroDelayLines(AXFX_REVERBSTD_EXP* reverb) {
    u32 ch, i;

    for (ch = 0; ch < 3; ch++) {
        if (reverb->earlyLine[ch] != NULL) {
            memset(reverb->earlyLine[ch], 0, sizeof(f32) * reverb->earlyMaxLength);
        }

        if (reverb->preDelayLine[ch] != NULL) {
            memset(reverb->preDelayLine[ch], 0, sizeof(f32) * reverb->preDelayMaxLength);
        }

        for (i = 0; i < 2; i++) {
            if (reverb->combLine[ch][i] != NULL) {
                memset(reverb->combLine[ch][i], 0, sizeof(f32) * reverb->combMaxLength[i]);
            }
        }

        for (i = 0; i < 2; i++) {
            if (reverb->allpassLine[ch][i] != NULL) {
                memset(reverb->allpassLine[ch][i], 0, sizeof(f32) * reverb->allpassMaxLength[i]);
            }
        }
    }
}

static void __FreeDelayLine(AXFX_REVERBSTD_EXP* reverb) {
    u32 ch, i;

    for (ch = 0; ch < 3; ch++) {
        if (reverb->earlyLine[ch] != NULL) {
            __AXFXFree(reverb->earlyLine[ch]);
            reverb->earlyLine[ch] = NULL;
        }

        if (reverb->preDelayLine[ch] != NULL) {
            __AXFXFree(reverb->preDelayLine[ch]);
            reverb->preDelayLine[ch] = NULL;
        }

        for (i = 0; i < 2; i++) {
            if (reverb->combLine[ch][i] != NULL) {
                __AXFXFree(reverb->combLine[ch][i]);
                reverb->combLine[ch][i] = NULL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (reverb->allpassLine[ch][i] != NULL) {
                __AXFXFree(reverb->allpassLine[ch][i]);
                reverb->allpassLine[ch][i] = NULL;
            }
        }
    }
}

static BOOL __InitParams(AXFX_REVERBSTD_EXP* reverb) {
    u32 i;

    if (reverb->earlyMode >= 8)
        return FALSE;

    if (reverb->preDelayTime < 0.0f || reverb->preDelayTime > reverb->preDelayTimeMax)
        return FALSE;

    if (reverb->fusedMode >= 6)
        return FALSE;

    if (reverb->fusedTime < 0.0f)
        return FALSE;

    if (reverb->coloration < 0.0f || reverb->coloration > 1.0f)
        return FALSE;

    if (reverb->damping < 0.0f || reverb->damping > 1.0f)
        return FALSE;

    if (reverb->earlyGain < 0.0f || reverb->earlyGain > 1.0f)
        return FALSE;

    if (reverb->fusedGain < 0.0f || reverb->fusedGain > 1.0f)
        return FALSE;

    if (reverb->outGain < 0.0f || reverb->outGain > 1.0f)
        return FALSE;

    if (reverb->sendGain < 0.0f || reverb->sendGain > 1.0f)
        return FALSE;

    reverb->earlyPos = 0;
    reverb->earlyLength = __EarlySizeTable[reverb->earlyMode];
    if (reverb->earlyMode <= 3) {
        reverb->earlyCoef = -0.33f;
    } else {
        reverb->earlyCoef = 0.33f;
    }

    reverb->preDelayPos = 0;
    reverb->preDelayLength = (u32)(reverb->preDelayTime * (f32)32000);

    for (i = 0; i < 2; i++) {
        reverb->combPos[i] = 0;
        reverb->combLength[i] = __FilterSizeTable[reverb->fusedMode][i];
        reverb->combCoef[i] = powf(10.0f, (-3.0f * (f32)(reverb->combLength[i]) / (f32)(reverb->fusedTime * 32000)));
    }

    for (i = 0; i < 2; i++) {
        reverb->allpassPos[i] = 0;
        reverb->allpassLength[i] = __FilterSizeTable[reverb->fusedMode][2 + i];
    }

    reverb->allpassCoef = reverb->coloration;
    reverb->lpfCoef = 1.0f - reverb->damping;
    if (reverb->lpfCoef > 0.95f)
        reverb->lpfCoef = 0.95f;

    for (i = 0; i < 3; i++) {
        reverb->lastLpfOut[i] = 0.0f;
    }

    return TRUE;
}
