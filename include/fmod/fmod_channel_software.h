/*
 * FMOD::ChannelSoftware: the real channel of a software-mixed voice: a DSP chain (wave table, resampler,
 * low pass, reverb, codec) that feeds the mix (fmod_channel_software.cpp).
 * Layout: Gormiti DWARF (sibling.exe types ChannelSoftware), size 0x2F0; the vtable is lbl_8044C1B8.
 * The unit that defines it is not reconstructed yet: only the constructor is called so far, the overrides are
 * declared so the vtable is complete.
 */
#ifndef _FMOD_CHANNEL_SOFTWARE_H
#define _FMOD_CHANNEL_SOFTWARE_H

#include "fmod_channel_realmanual3d.h"
#include "fmod_dsp_wavetable.h"
#include "fmod_dspi.h"

namespace FMOD {

class DSPCodec;
class DSPConnectionI;
class DSPResampler;

class ChannelSoftware : public ChannelRealManual3D {
public:
    DSPI           *mDSPHead;               // 0x68
    DSPFilter       mDSPHeadMemory;         // 0x6C, storage of the head unit
    DSPWaveTable   *mDSPWaveTable;          // 0x184
    DSPWaveTable    mDSPWaveTableMemory;    // 0x188, storage of the wave table unit
    DSPResampler   *mDSPResampler;          // 0x2D8
    DSPI           *mDSPLowPass;            // 0x2DC
    DSPI           *mDSPReverb;             // 0x2E0
    DSPCodec       *mDSPCodec;              // 0x2E4
    DSPConnectionI *mDSPConnection;         // 0x2E8

    ChannelSoftware();                      // 0x8032F6DC

    // Overrides of the ChannelReal slots (vtable lbl_8044C1B8); the bodies are in fmod_channel_software.cpp.
    virtual FMOD_RESULT moveChannelGroup( ChannelGroupI *oldchannelgroup, ChannelGroupI *newchannelgroup,
                                          bool forcedspreconnect );
    virtual             ~ChannelSoftware() {}
    virtual FMOD_RESULT init( int index, SystemI *system, Output *output, DSPI *dspmixtarget );
    virtual FMOD_RESULT close();
    virtual FMOD_RESULT alloc();
    virtual FMOD_RESULT alloc( DSPI *dsp );
    virtual FMOD_RESULT start();
    virtual FMOD_RESULT stop();
    virtual FMOD_RESULT setPaused( bool paused );
    virtual FMOD_RESULT getPaused( bool *paused );
    virtual FMOD_RESULT setVolume( float volume );
    virtual FMOD_RESULT setFrequency( float frequency );
    virtual FMOD_RESULT setPan( float pan, float fbpan );
    virtual FMOD_RESULT setDSPClockDelay();
    virtual FMOD_RESULT setSpeakerMix( float frontleft, float frontright, float center, float lfe,
                                       float backleft, float backright, float sideleft, float sideright );
    virtual FMOD_RESULT setSpeakerLevels( int speaker, float *levels, int numlevels );
    virtual FMOD_RESULT setPosition( unsigned int position, unsigned int postype );
    virtual FMOD_RESULT getPosition( unsigned int *position, unsigned int postype );
    virtual FMOD_RESULT setLoopPoints( unsigned int loopstart, unsigned int looplength );
    virtual FMOD_RESULT setLoopCount( int loopcount );
    virtual FMOD_RESULT setLowPassGain( float gain );
    virtual FMOD_RESULT set3DOcclusion( float directocclusion, float reverbocclusion );
    virtual FMOD_RESULT setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    virtual FMOD_RESULT getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );
    virtual FMOD_RESULT isPlaying( bool *isplaying, bool includethreadlatency );
    virtual FMOD_RESULT getSpectrum( float *spectrumarray, int numvalues, int channeloffset,
                                     FMOD_DSP_FFT_WINDOW windowtype );
    virtual FMOD_RESULT getWaveData( float *wavearray, int numvalues, int channeloffset );
    virtual FMOD_RESULT getDSPHead( DSPI **dsp );
    virtual FMOD_RESULT setMode( FMOD_MODE mode );
};

} // namespace FMOD

#endif /* _FMOD_CHANNEL_SOFTWARE_H */
