#ifndef HBM_SOUND_SYNVOICE_H
#define HBM_SOUND_SYNVOICE_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/synvoice.h

#include <revolution/types.h>
#include <revolution/ax.h>

#include "sound/syn.h"

/* One sounding voice (0x4C bytes). A voice is a slot of the synth's voice pool, bound to one AX
 * voice when a note starts and released when its envelope ends. */
struct HBMSYNVOICE {
    void* next;         // 0x00
    AXVPB* axvpb;       // 0x04: the AX voice this slot plays through
    HBMSYNSYNTH* synth; // 0x08: owner, NULL when the slot is free
    u8 midiChannel;     // 0x0C
    u8 keyNum;          // 0x0D
    u8 keyVel;          // 0x0E
    u8 pan;             // 0x0F
    WTREGION* region;   // 0x10: the wavetable region the note plays
    WTART* art;         // 0x14: its articulation
    WTSAMPLE* sample;   // 0x18
    WTADPCM* adpcm;     // 0x1C
    u32 type;           // 0x20: 1 while the sample loops
    f32 srcRatio;       // 0x24: sample rate ratio, without the note's pitch
    s32 cents;          // 0x28: the note's pitch offset, in cents << 16
    s32 attn;           // 0x2C: attenuation, in dB << 16
    u32 veState;        // 0x30: VolumeEnvelopeStages
    s32 veAttn;         // 0x34
    s32 veAttack;       // 0x38
    s32 veAttackDelta;  // 0x3C
    s32 veDecay;        // 0x40
    s32 veSustain;      // 0x44
    s32 veRelease;      // 0x48
};

#ifdef __cplusplus
extern "C" {
#endif

void __HBMSYNClearVoiceReferences(void* voice);
void __HBMSYNSetVoiceToRelease(HBMSYNVOICE* voice);
void __HBMSYNServiceVoice(int index);

#ifdef __cplusplus
}
#endif

#endif
