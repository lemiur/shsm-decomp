// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/sound/synvoice.cpp
#include <revolution/ax.h>

#include <assert.h>

#include "sound/mix.h"
#include "sound/syn.h"
#include "sound/synenv.h"
#include "sound/synmix.h"
#include "sound/synpitch.h"
#include "sound/synvoice.h"

/*
 * __HBMSYNClearVoiceReferences
 */
void __HBMSYNClearVoiceReferences(void* p) {
    AXVPB* axvpb;
    HBMSYNSYNTH* synth;
    HBMSYNVOICE* voice;
    int index;

    assert(p);

    axvpb = (AXVPB*)p;
    synth = (HBMSYNSYNTH*)axvpb->userContext;

    index = HBMGetIndex(axvpb->index);
    HBMFreeIndex(index);

    voice = __HBMSYNVoice + index;
    HBMMIXReleaseChannel(axvpb);

    if (synth->voice[voice->midiChannel][voice->keyNum] == voice)
        synth->voice[voice->midiChannel][voice->keyNum] = NULL;

    voice->synth = NULL;
    synth->notes--;
} // 0x803D78B0

/*
 * __HBMSYNSetVoiceToRelease
 */
void __HBMSYNSetVoiceToRelease(HBMSYNVOICE* voice) {
    assert(voice);

    voice->veState = VolEnvState_Release;
} // 0x803D7960

/*
 * __HBMSYNServiceVoice
 */
void __HBMSYNServiceVoice(int i) {
    HBMSYNVOICE* voice = __HBMSYNVoice + i;
    HBMSYNSYNTH* synth = voice->synth;

    if (!synth)
        return;

    if (voice->type == 0 && voice->axvpb->pb.state == 0) {
        if (synth->voice[voice->midiChannel][voice->keyNum] == voice)
            synth->voice[voice->midiChannel][voice->keyNum] = NULL;

        voice->veState = VolEnvState_End;
    }

    __HBMSYNRunVolumeEnvelope(voice);

    if (voice->veState == VolEnvState_End) {
        voice->synth = NULL;

        HBMMIXReleaseChannel(voice->axvpb);
        HBMFreeIndexByKey(voice->axvpb->index);
        AXFreeVoice(voice->axvpb);

        synth->notes--;
    }
    else {
        __HBMSYNUpdateMix(voice);
        __HBMSYNUpdateSrc(voice);
    }
} // 0x803D7970
