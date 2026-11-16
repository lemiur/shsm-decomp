/*
 * FMOD::DSPCodec: DSP unit for streaming/compressed sound playback via codecs (fmod_dsp_codec.cpp).
 * Layout: Gormiti DWARF (sibling.exe types DSPCodec), size 0x498.
 */
#ifndef _FMOD_DSP_CODEC_H
#define _FMOD_DSP_CODEC_H

#include <fmod/fmod.h>
#include <fmod/fmod_codec.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_dsp_resampler.h>
#include <fmod/fmod_file_memory.h>

namespace FMOD {

class DSPCodecPool;

class DSPCodec : public DSPResampler {
public:
    DSPCodecPool          *mPool;                      // 0x1B0
    MemoryFile             mMemoryFile;                // 0x1B4
    unsigned int           mPosition;                  // 0x350
    unsigned int           mNewPosition;               // 0x354
    unsigned int           mSetPosIncrementPrev;       // 0x358
    unsigned int           mLoopCountIncrementPrev;    // 0x35C
    int                    mPoolIndex;                 // 0x360
    int                    mSubSoundListCurrent;       // 0x364
    int                    mSubSoundListNum;           // 0x368
    FMOD_CODEC_WAVEFORMAT  mWaveFormat;                // 0x36C
    Codec                 *mCodec;                     // 0x494

    virtual FMOD_RESULT release( bool freethis );      // 0x8035AFDC
};

class DSPCodecPool {
public:
    SystemI       *mSystem;                            // 0x00
    int            mNumDSPCodecs;                      // 0x04
    DSPCodec     **mPool;                              // 0x08
    bool           mAllocated[256];                    // 0x0C
    unsigned char *mReadBuffer;                        // 0x10C
};

} // namespace FMOD

#endif /* _FMOD_DSP_CODEC_H */
