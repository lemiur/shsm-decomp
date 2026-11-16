/*
 * The software mixer's sample-rate converters and the way they stopped (fmod_dsp_resampler.cpp,
 * fmod_dsp_resampler_nointerp.cpp, fmod_dsp_resampler_linear.cpp). The functions are C functions
 * (unmangled in retail); their parameter names are invented from use in DSPWaveTable::execute.
 * Layout: Gormiti DWARF (sibling.exe types DSPResampler, DSPResampler_NODMA, DSPRESAMPLER_SPEEDDIR,
 * FMOD_RESAMPLER_END).
 */
#ifndef _FMOD_DSP_RESAMPLER_H
#define _FMOD_DSP_RESAMPLER_H

#include <fmod/fmod.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_types.h>

// Why a resampling pass stopped.
enum FMOD_RESAMPLER_END {
    FMOD_RESAMPLER_END_MIXBUFFER,       // the output buffer is full
    FMOD_RESAMPLER_END_RESAMPLEBUFFER,
    FMOD_RESAMPLER_END_SOUND,           // the end of the sound
    FMOD_RESAMPLER_END_SUBSOUND         // the end of a subsound of a sentence
};

extern "C" {

// Copies outlength samples of the sound to outbuffer, stepping through it by speed (32.32 fixed point) from position.
void FMOD_Resampler_NoInterp( float *outbuffer, int outlength, void *inbuffer, FMOD_SOUND_FORMAT informat,
                              FMOD_UINT64P *position, FMOD_SINT64P *speed, int channels );
// The same with linear interpolation.
void FMOD_Resampler_Linear( float *outbuffer, int outlength, void *inbuffer, FMOD_SOUND_FORMAT informat,
                            FMOD_UINT64P *position, FMOD_SINT64P *speed, int channels );

}

namespace FMOD {

enum DSPRESAMPLER_SPEEDDIR {
    DSPRESAMPLER_SPEEDDIR_FORWARDS  = 0,
    DSPRESAMPLER_SPEEDDIR_BACKWARDS = 1
};

class DSPResampler_NODMA {
public:
    FMOD_UINT64P mDSPClockStart;                     // 0x00
    FMOD_UINT64P mDSPClockEnd;                       // 0x08
    FMOD_UINT64P mDSPClockPause;                     // 0x10
    unsigned int mMode;                              // 0x18
    unsigned int mLoopStart;                         // 0x1C
    unsigned int mLoopLength;                        // 0x20
    unsigned int mDSPFinishTick;                     // 0x24
    int          mNewLoopCount;                      // 0x28
    unsigned int mLoopCountIncrement;                // 0x2C
    unsigned int mNewPosition;                       // 0x30
    unsigned int mSetPosIncrement;                   // 0x34
};

class DSPResampler : public DSPFilter {
public:
    FMOD_UINT64P          mPosition;                  // 0x118
    FMOD_SINT64P          mSpeed;                     // 0x120
    float                 mFrequency;                 // 0x128
    int                   mTargetFrequency;           // 0x12C
    FMOD_UINT64P          mResamplePosition;          // 0x130
    void                 *mResampleBufferMemory;      // 0x138
    void                 *mResampleBuffer;            // 0x13C
    int                   mResampleBufferChannels;    // 0x140
    unsigned int          mResampleBlockLength;       // 0x144
    unsigned int          mResampleBufferLength;      // 0x148
    unsigned int          mResampleBufferPos;         // 0x14C
    unsigned int          mResampleFinishPos;         // 0x150
    unsigned int          mOverflowLength;            // 0x154
    unsigned int          mReadPosition;              // 0x158
    int                   mFill;                      // 0x15C
    DSPI                 *mDSPSoundCard;              // 0x160
    DSPRESAMPLER_SPEEDDIR mDirection;                 // 0x164
    unsigned int          mLength;                    // 0x168
    int                   mLoopCount;                 // 0x16C
    DSPResampler_NODMA   *mNoDMA;                     // 0x170
    DSPResampler_NODMA    mNoDMAMemory;               // 0x178

    DSPResampler();                                   // 0x8035F62C

    // Overrides of DSPFilter / DSPI (vtable lbl_8044EB08).
    virtual FMOD_RESULT execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                 int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick ); // 0x8035FA04
    virtual FMOD_RESULT alloc( FMOD_DSP_DESCRIPTION_EX *description );                                // 0x8035F758
    virtual FMOD_RESULT setPosition( unsigned int position, bool processinputs );                     // 0x80360404
    virtual FMOD_RESULT release( bool freethis );                                                     // 0x8035F6C0

    // New virtual method on DSPResampler (slot 0x60).
    virtual FMOD_RESULT addInput( DSPI *target );                                                     // 0x8036036C

    FMOD_RESULT setFrequency( float frequency );                                                      // 0x80360374
    FMOD_RESULT setFinished( bool finished, bool force );                                             // 0x80360474
    FMOD_RESULT getFinished( bool *finished );                                                        // 0x80360514
};

} // namespace FMOD

#endif /* _FMOD_DSP_RESAMPLER_H */
