// Unit name unconfirmed; modelled on doldecomp/ogws src/revolution/AXFX/AXFXReverbHi.c (no public source for the
// standard reverb exists)
#include <revolution/axfx.h>

static void __ParamConvert(AXFX_REVERBSTD* fx);

/*
 * AXFXReverbStdInit
 *
 * Derives the expert reverb's parameters from the simple ones and starts it.
 */
BOOL AXFXReverbStdInit(AXFX_REVERBSTD* fx) {
    __ParamConvert(fx);
    return AXFXReverbStdExpInit(&fx->exp);
} // 0x80232890

BOOL AXFXReverbStdShutdown(AXFX_REVERBSTD* fx) {
    AXFXReverbStdExpShutdown(&fx->exp);
    return TRUE;
} // 0x802328F0

void AXFXReverbStdCallback(void* chans, void* context) {
    AXFXReverbStdExpCallback((AXFX_BUFFERUPDATE*)chans, (AXFX_REVERBSTD_EXP*)context);
} // 0x80232920

static void __ParamConvert(AXFX_REVERBSTD* fx) {
    fx->exp.earlyMode = 5;
    fx->exp.preDelayTimeMax = fx->preDelay;
    fx->exp.preDelayTime = fx->preDelay;
    fx->exp.fusedMode = 0;
    fx->exp.fusedTime = fx->time;
    fx->exp.coloration = fx->coloration;
    fx->exp.damping = fx->damping;
    fx->exp.earlyGain = 0.0f;
    fx->exp.fusedGain = 1.0f;
    fx->exp.busIn = NULL;
    fx->exp.busOut = NULL;
    fx->exp.outGain = fx->mix;
    fx->exp.sendGain = 0.0f;
}
