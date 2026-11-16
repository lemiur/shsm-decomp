#ifndef HBM_SOUND_SYNENV_H
#define HBM_SOUND_SYNENV_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/synenv.h

#include <revolution/types.h>

#include "sound/syn.h"

/* The stages a voice's volume envelope passes through. */
enum VolumeEnvelopeStages {
    VolEnvState_Attack,
    VolEnvState_Decay,
    VolEnvState_Sustain,
    VolEnvState_Release,
    VolEnvState_End
};

#ifdef __cplusplus
extern "C" {
#endif

void __HBMSYNRunVolumeEnvelope(HBMSYNVOICE* voice);
void __HBMSYNSetupVolumeEnvelope(HBMSYNVOICE* voice);

#ifdef __cplusplus
}
#endif

#endif
