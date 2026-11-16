/*
 * FMOD::DSPWaveTable: the DSP unit that plays a sample's PCM data into the mix (fmod_dsp_wavetable.cpp).
 * Layout: Gormiti DWARF (sibling.exe types DSPWaveTable), size 0x150.
 * Virtuals: retail vtable lbl_8044EC68 (22 slots, two overridden here). The constructor is not declared
 * until the unit that defines it is reconstructed.
 */
#ifndef _FMOD_DSP_WAVETABLE_H
#define _FMOD_DSP_WAVETABLE_H

#include <fmod/fmod.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_types.h>

namespace FMOD {

class ChannelReal;
class SoundI;

// Whether the table is read forwards or backwards (Gormiti DWARF).
enum DSPWAVETABLE_SPEEDDIR {
    DSPWAVETABLE_SPEEDDIR_FORWARDS,
    DSPWAVETABLE_SPEEDDIR_BACKWARDS
};

class DSPWaveTable : public DSPI {
public:
    FMOD_UINT64P            mPosition;          // 0x108, 32.32 fixed point: sample and fraction
    unsigned int            mNewPosition;       // 0x110, a pending seek, or -1
    unsigned char           field_0x114[0x118 - 0x114];
    FMOD_SINT64P            mSpeed;             // 0x118, 32.32 fixed point: samples per output sample
    DSPWAVETABLE_SPEEDDIR   mDirection;         // 0x120
    float                   mFrequency;         // 0x124
    ChannelReal            *mChannel;           // 0x128
    SoundI                 *mSound;             // 0x12C
    FMOD_UINT64P            mDSPClockStart;     // 0x130
    FMOD_UINT64P            mDSPClockEnd;       // 0x138
    FMOD_UINT64P            mDSPClockPause;     // 0x140
    unsigned int            mDSPFinishTick;     // 0x148
    DSPI                   *mDSPSoundCard;      // 0x14C

    // Overrides of DSPI (slots 0x10 and 0x14 of the vtable).
    virtual FMOD_RESULT execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                 int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick );
    virtual FMOD_RESULT alloc( FMOD_DSP_DESCRIPTION_EX *description );

    FMOD_RESULT setFrequency( float frequency );
    FMOD_RESULT setFinished( bool finished, bool force );
    FMOD_RESULT getFinished( bool *finished );

    // The unit's description callbacks; dsp_state is the FMOD_DSP_STATE base of the unit.
    static FMOD_RESULT setPositionCallback( FMOD_DSP_STATE *dsp_state, unsigned int position );
    static FMOD_RESULT setParameterCallback( FMOD_DSP_STATE *dsp_state, int index, float value );
    static FMOD_RESULT getParameterCallback( FMOD_DSP_STATE *dsp_state, int index, float *value, char *valuestr );
    static FMOD_RESULT resetCallback( FMOD_DSP_STATE *dsp_state );
};

} // namespace FMOD

#endif /* _FMOD_DSP_WAVETABLE_H */
