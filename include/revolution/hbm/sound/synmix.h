#ifndef HBM_SOUND_SYNMIX_H
#define HBM_SOUND_SYNMIX_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/synmix.h

#include <revolution/types.h>

#include "sound/syn.h"

/* Attenuation in dB << 16 for the MIDI controller values (0..127) and for the envelope's attack
 * ramp (0..99). */
extern s32 __HBMSYNVolumeAttenuation[];
extern s32 __HBMSYNAttackAttnTable[];

#ifdef __cplusplus
extern "C" {
#endif

void __HBMSYNSetupVolume(HBMSYNVOICE* voice);
void __HBMSYNSetupPan(HBMSYNVOICE* voice);
s32 __HBMSYNGetVoiceInput(HBMSYNVOICE* voice);
s32 __HBMSYNGetVoiceFader(HBMSYNVOICE* voice);
void __HBMSYNUpdateMix(HBMSYNVOICE* voice);

#ifdef __cplusplus
}
#endif

#endif
