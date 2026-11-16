// Reference: doldecomp/ogws src/revolution/AXFX/AXFXReverbHi.c, zeldaret/tp libs/revolution/src/axfx/AXFXReverbHi.c
#include <revolution/axfx.h>

static void __ParamConvert(AXFX_REVERBHI* fx);

/*
 * AXFXReverbHiInit
 *
 * Derives the expert reverb's parameters from the simple ones and starts it.
 */
BOOL AXFXReverbHiInit(AXFX_REVERBHI* fx) {
    __ParamConvert(fx);
    return AXFXReverbHiExpInit(&fx->exp);
} // 0x802309D0

BOOL AXFXReverbHiShutdown(AXFX_REVERBHI* fx) {
    AXFXReverbHiExpShutdown(&fx->exp);
    return TRUE;
} // 0x80230A40

void AXFXReverbHiCallback(void* chans, void* context) {
    AXFXReverbHiExpCallback((AXFX_BUFFERUPDATE*)chans, (AXFX_REVERBHI_EXP*)context);
} // 0x80230A70

static void __ParamConvert(AXFX_REVERBHI* fx) {
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
