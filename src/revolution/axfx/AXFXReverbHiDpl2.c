// Reference: doldecomp/ogws src/revolution/AXFX/AXFXReverbHiDpl2.c
#include <revolution/ax.h>
#include <revolution/axfx.h>

static void __ParamConvert(AXFX_REVERBHI_DPL2* fx);

/*
 * AXFXReverbHiInitDpl2
 *
 * Starts the Dolby Pro Logic II reverb; it is only available while AX outputs in Pro Logic II mode.
 */
BOOL AXFXReverbHiInitDpl2(AXFX_REVERBHI_DPL2* fx) {
    if (AXGetMode() != AX_OUTPUT_DPL2) {
        return FALSE;
    }

    __ParamConvert(fx);
    return AXFXReverbHiExpInitDpl2(&fx->exp);
} // 0x80230A80

BOOL AXFXReverbHiShutdownDpl2(AXFX_REVERBHI_DPL2* fx) {
    AXFXReverbHiExpShutdownDpl2(&fx->exp);
    return TRUE;
} // 0x80230B30

void AXFXReverbHiCallbackDpl2(void* chans, void* context) {
    AXFXReverbHiExpCallbackDpl2((AXFX_BUFFERUPDATE_DPL2*)chans, (AXFX_REVERBHI_EXP_DPL2*)context);
} // 0x80230B60

static void __ParamConvert(AXFX_REVERBHI_DPL2* fx) {
    fx->exp.earlyMode = 5;
    fx->exp.preDelayTimeMax = fx->preDelay;
    fx->exp.preDelayTime = fx->preDelay;
    fx->exp.fusedMode = 0;
    fx->exp.fusedTime = fx->time;
    fx->exp.coloration = fx->coloration;
    fx->exp.damping = fx->damping;
    fx->exp.crosstalk = fx->crosstalk;
    fx->exp.earlyGain = 0.0f;
    fx->exp.fusedGain = 1.0f;
    fx->exp.busIn = NULL;
    fx->exp.busOut = NULL;
    fx->exp.outGain = fx->mix;
    fx->exp.sendGain = 0.0f;
}
