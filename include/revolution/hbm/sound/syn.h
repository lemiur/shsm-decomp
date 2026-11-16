#ifndef HBM_SOUND_SYN_H
#define HBM_SOUND_SYN_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/sound/syn.h
//
// The voice type HBMSYNVOICE is defined in synvoice.h; this header only names it, because the
// synthesizer holds voice pointers.

#include <revolution/types.h>

#define NUM_MIDI_CHANNELS        16
#define NUM_MIDI_NOTES           128
#define HBMSYN_INPUT_BUFFER_SIZE 256

/* Wavetable instrument: a region index per key. */
typedef struct WTINST {
    u16 keyRegion[128];
} WTINST;

/* Wavetable key region. */
typedef struct WTREGION {
    u8 unityNote;
    u8 keyGroup;
    s16 fineTune;
    s32 attn;
    u32 loopStart;
    u32 loopLength;
    u32 articulationIndex;
    u32 sampleIndex;
} WTREGION;

/* Wavetable articulation: LFO and envelope parameters. */
typedef struct WTART {
    s32 lfoFreq;
    s32 lfoDelay;
    s32 lfoAtten;
    s32 lfoPitch;
    s32 lfoMod2Atten;
    s32 lfoMod2Pitch;
    s32 eg1Attack;
    s32 eg1Decay;
    s32 eg1Sustain;
    s32 eg1Release;
    s32 eg1Vel2Attack;
    s32 eg1Key2Decay;
    s32 eg2Attack;
    s32 eg2Decay;
    s32 eg2Sustain;
    s32 eg2Release;
    s32 eg2Vel2Attack;
    s32 eg2Key2Decay;
    s32 eg2Pitch;
    s32 pan;
} WTART;

/* Wavetable sample header. */
typedef struct WTSAMPLE {
    u16 format;
    u16 sampleRate;
    u32 offset;
    u32 length;
    u16 adpcmIndex;
} WTSAMPLE;

/* ADPCM decoder state of a sample. */
typedef struct WTADPCM {
    u16 a[8][2];
    u16 gain;
    u16 pred_scale;
    u16 yn1;
    u16 yn2;
    u16 loop_pred_scale;
    u16 loop_yn1;
    u16 loop_yn2;
} WTADPCM;

typedef struct HBMSYNVOICE HBMSYNVOICE;

/* One MIDI synthesizer (0x2408 bytes; embedded in HBMSEQSEQUENCE). */
typedef struct HBMSYNSYNTH {
    struct HBMSYNSYNTH* next;                           // 0x000
    WTINST* percussiveInst;                             // 0x004
    WTINST* melodicInst;                                // 0x008
    WTREGION* region;                                   // 0x00C
    WTART* art;                                         // 0x010
    WTSAMPLE* sample;                                   // 0x014
    WTADPCM* adpcm;                                     // 0x018
    u32 samplesBaseWord;                                // 0x01C
    u32 samplesBaseByte;                                // 0x020
    u32 samplesBaseNibble;                              // 0x024
    WTINST* inst[NUM_MIDI_CHANNELS];                    // 0x028
    s32 masterVolume;                                   // 0x068
    s32 volAttn[NUM_MIDI_CHANNELS];                     // 0x06C
    s32 auxAAttn[NUM_MIDI_CHANNELS];                    // 0x0AC
    u8 pan[NUM_MIDI_CHANNELS];                          // 0x0EC
    u8 input[HBMSYN_INPUT_BUFFER_SIZE][3];              // 0x0FC
    u8* inputPosition;                                  // 0x3FC
    u32 inputCounter;                                   // 0x400
    u32 notes;                                          // 0x404: notes still sounding
    HBMSYNVOICE* voice[NUM_MIDI_CHANNELS][NUM_MIDI_NOTES]; // 0x408
} HBMSYNSYNTH;

#ifdef __cplusplus
extern "C" {
#endif

void HBMSYNInit(void);
void HBMSYNQuit(void);
void HBMSYNRunAudioFrame(void);
void HBMSYNInitSynth(HBMSYNSYNTH* synth, u8* wavetable, u8* samples, u8* zeroBuffer);
void HBMSYNQuitSynth(HBMSYNSYNTH* synth);
void HBMSYNMidiInput(HBMSYNSYNTH* synth, u8* input);
void HBMSYNSetMasterVolume(HBMSYNSYNTH* synth, s32 dB);

/* The voice pool: the 16 voices a synth's notes play through. */
extern HBMSYNVOICE* __HBMSYNVoice;

#ifdef __cplusplus
}
#endif

#endif
