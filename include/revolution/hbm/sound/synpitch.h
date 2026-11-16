#ifndef HBM_SOUND_SYNPITCH_H
#define HBM_SOUND_SYNPITCH_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/synpitch.h

#include <revolution/types.h>

#include "sound/syn.h"

#ifdef __cplusplus
extern "C" {
#endif

f32 __HBMSYNGetRelativePitch(HBMSYNVOICE* voice);
void __HBMSYNSetupPitch(HBMSYNVOICE* voice);
void __HBMSYNSetupSrc(HBMSYNVOICE* voice);
void __HBMSYNUpdateSrc(HBMSYNVOICE* voice);

#ifdef __cplusplus
}
#endif

#endif
