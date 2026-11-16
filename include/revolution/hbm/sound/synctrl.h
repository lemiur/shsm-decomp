#ifndef HBM_SOUND_SYNCTRL_H
#define HBM_SOUND_SYNCTRL_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/synctrl.h

#include <revolution/types.h>

#include "sound/syn.h"

/* 1/128 steps, the scale the MIDI controllers and the envelope parameters use. */
extern f32 __HBMSYNn128[];

#ifdef __cplusplus
extern "C" {
#endif

void __HBMSYNSetController(HBMSYNSYNTH* synth, u8 midiChannel, u8 function, u8 value);
BOOL __HBMSYNGetWavetableData(HBMSYNVOICE* voice);
void __HBMSYNResetAllControllers(HBMSYNSYNTH* synth);
void __HBMSYNRunInputBufferEvents(HBMSYNSYNTH* synth);

#ifdef __cplusplus
}
#endif

#endif
