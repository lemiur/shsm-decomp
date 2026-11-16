#ifndef HBM_SOUND_SEQ_H
#define HBM_SOUND_SEQ_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/seq.h

#include <revolution/types.h>
#include "sound/syn.h"

#define HBMSEQ_MAX_TRACKS 64

/* HBMSEQSetState / HBMSEQGetState states (names invented, after the RVL SDK seq library's
 * SEQ_STATE_STOP and SEQ_STATE_RUN). */
#define HBMSEQ_STATE_STOP 0
#define HBMSEQ_STATE_RUN  1

typedef struct _HBMSEQSEQUENCE HBMSEQSEQUENCE;

/* One MIDI track of a sequence (0x28 bytes). */
typedef struct _HBMSEQTRACK {
    HBMSEQSEQUENCE* sequence; // 0x00
    u8* start;                // 0x04
    u8* end;                  // 0x08
    u8* current;              // 0x0C
    u8 status;                // 0x10: running status byte
    f32 beatsPerSec;          // 0x14
    u32 defaultTicksPerFrame; // 0x18
    u32 ticksPerFrame;        // 0x1C
    u32 delay;                // 0x20
    u32 state;                // 0x24
} HBMSEQTRACK;

/* A playing MIDI sequence with its own synthesizer (0x2E1C bytes). */
struct _HBMSEQSEQUENCE {
    HBMSEQSEQUENCE* next;                // 0x000
    u32 state;                           // 0x004
    u16 nTracks;                         // 0x008
    s16 timeFormat;                      // 0x00A
    u32 tracksRunning;                   // 0x00C
    u32 end;                             // 0x010
    HBMSYNSYNTH synth;                   // 0x014
    HBMSEQTRACK track[HBMSEQ_MAX_TRACKS]; // 0x241C
};

#ifdef __cplusplus
extern "C" {
#endif

void HBMSEQInit(void);
void HBMSEQQuit(void);
void HBMSEQRunAudioFrame(void);
void HBMSEQAddSequence(HBMSEQSEQUENCE* sequence, u8* midiStream, u8* wavetable, u8* samples, u8* zeroBuffer);
void HBMSEQRemoveSequence(HBMSEQSEQUENCE* sequence);
void HBMSEQSetState(HBMSEQSEQUENCE* sequence, u32 state);
u32 HBMSEQGetState(HBMSEQSEQUENCE* sequence);
void HBMSEQSetVolume(HBMSEQSEQUENCE* sequence, s32 dB);

#ifdef __cplusplus
}
#endif

#endif
