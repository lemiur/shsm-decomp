/*
 * FMOD::ChannelReal: a voice of the output (hardware, software or emulated), the base of every real
 * channel class (fmod_channel_real.cpp). Member names follow FMOD and the sibling build; the vtable
 * (retail __vt__Q24FMOD11ChannelReal) has 44 slots, in the order below.
 * Layout: Gormiti DWARF (sibling.exe types ChannelReal), size 0x64; the vtable pointer follows the
 * data members, at 0x60.
 */
#ifndef _FMOD_CHANNEL_REAL_H
#define _FMOD_CHANNEL_REAL_H

#include "fmod.h"

namespace FMOD {

class SystemI;
class ChannelPool;
class ChannelI;
class Output;
class SoundI;
class DSPI;
class ChannelGroupI;

// ChannelReal::mFlags bits (names invented from use).
#define CHANNELREAL_FLAG_PAUSED     0x00000020
#define CHANNELREAL_FLAG_PLAYING    0x00000040
// Set by ChannelPool::allocateChannel when it takes a channel; ChannelMusic sets it on its own channel (meaning
// from use).
#define CHANNELREAL_FLAG_0x10       0x00000010

class ChannelReal {
public:
    SystemI        *mSystem;                // 0x00
    int             mSubChannelIndex;       // 0x04
    ChannelPool    *mPool;                  // 0x08
    ChannelI       *mParent;                // 0x0C
    Output         *mOutput;                // 0x10
    SoundI         *mSound;                 // 0x14
    DSPI           *mDSP;                   // 0x18
    FMOD_MODE       mMode;                  // 0x1C
    unsigned int    mFlags;                 // 0x20
    int             mIndex;                 // 0x24
    unsigned int    mPosition;              // 0x28
    int             mDirection;             // 0x2C, 0 forwards, 1 backwards
    int             mLoopCount;             // 0x30
    unsigned int    mLoopStart;             // 0x34
    unsigned int    mLoopLength;            // 0x38
    unsigned int    mLength;                // 0x3C
    float           mMaxFrequency;          // 0x40
    float           mMinFrequency;          // 0x44
    int             mSubSoundListCurrent;   // 0x48
    int             mLPFCutoff;             // 0x4C
    bool            mBiquadActive;          // 0x50
    unsigned short  mBiquadB0;              // 0x52
    unsigned short  mBiquadB1;              // 0x54
    unsigned short  mBiquadB2;              // 0x56
    unsigned short  mBiquadA1;              // 0x58
    unsigned short  mBiquadA2;              // 0x5A
    unsigned int    mControllerSpeaker;     // 0x5C

    ChannelReal();                          // 0x8032CD28

    virtual FMOD_RESULT set2DFreqVolumePanFor3D();
    virtual bool        isStream() { return false; }
    virtual FMOD_RESULT moveChannelGroup( ChannelGroupI *oldchannelgroup, ChannelGroupI *newchannelgroup,
                                          bool forcedspreconnect ) { return FMOD_OK; }
    virtual             ~ChannelReal() {}
    virtual FMOD_RESULT init( int index, SystemI *system, Output *output, DSPI *dspmixtarget );
    virtual FMOD_RESULT close();
    virtual FMOD_RESULT alloc();
    virtual FMOD_RESULT alloc( DSPI *dsp );
    virtual FMOD_RESULT start();
    virtual FMOD_RESULT update( int delta );
    virtual FMOD_RESULT updateStream();
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
    virtual FMOD_RESULT updateSpeakerLevels( float volume );
    virtual FMOD_RESULT setPosition( unsigned int position, unsigned int postype );
    virtual FMOD_RESULT setPositionEx( unsigned int position, unsigned int postype, bool fromsetposition ) {
        return setPosition( position, postype );
    }
    virtual FMOD_RESULT getPosition( unsigned int *position, unsigned int postype );
    virtual FMOD_RESULT setLoopPoints( unsigned int loopstart, unsigned int looplength );
    virtual FMOD_RESULT setLoopCount( int loopcount );
    virtual FMOD_RESULT setLowPassGain( float gain );
    virtual FMOD_RESULT set3DAttributes();
    virtual FMOD_RESULT set3DMinMaxDistance();
    virtual FMOD_RESULT set3DOcclusion( float directocclusion, float reverbocclusion );
    virtual FMOD_RESULT setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    virtual FMOD_RESULT getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );
    virtual FMOD_RESULT isPlaying( bool *isplaying, bool includethreadlatency );
    virtual FMOD_RESULT isVirtual( bool *isvirtual );
    virtual FMOD_RESULT getSpectrum( float *spectrumarray, int numvalues, int channeloffset,
                                     FMOD_DSP_FFT_WINDOW windowtype );
    virtual FMOD_RESULT getWaveData( float *wavearray, int numvalues, int channeloffset );
    virtual FMOD_RESULT getDSPHead( DSPI **dsp );
    virtual FMOD_RESULT setMode( FMOD_MODE mode );
    virtual FMOD_RESULT setLowPassFilter( int cutoff );
    virtual FMOD_RESULT getLowPassFilter( int *cutoff );
    virtual FMOD_RESULT setBiquadFilter( bool active, unsigned short b0, unsigned short b1, unsigned short b2,
                                         unsigned short a1, unsigned short a2 );
    virtual FMOD_RESULT getBiquadFilter( bool *active, unsigned short *b0, unsigned short *b1,
                                         unsigned short *b2, unsigned short *a1, unsigned short *a2 );
    virtual FMOD_RESULT setControllerSpeaker( unsigned int speaker, int controller );
    virtual FMOD_RESULT getControllerSpeaker( unsigned int *speaker );
};

} // namespace FMOD

#endif /* _FMOD_CHANNEL_REAL_H */
