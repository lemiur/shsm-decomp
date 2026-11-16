/*
 * FMOD::ChannelStream, the real channel of a stream (fmod_async.cpp). The data members are the
 * layout only; the overrides are the slots of its retail vtable lbl_8044C2C8 that differ from
 * ChannelReal's (init, close, alloc( DSPI * ), getPaused and updateSpeakerLevels are inherited).
 * Layout: Gormiti DWARF (sibling.exe types ChannelStream), size 0xD4.
 */
#ifndef _FMOD_CHANNEL_STREAM_H
#define _FMOD_CHANNEL_STREAM_H

#include "fmod_channel_real.h"
#include "fmod_linkedlist.h"

namespace FMOD {

class ChannelStream : public ChannelReal, public LinkedListNode {
public:
    bool            mFinished;              // 0x70
    unsigned char   field_0x71[0xD4 - 0x71];

    virtual FMOD_RESULT set2DFreqVolumePanFor3D();
    virtual bool        isStream();
    virtual FMOD_RESULT moveChannelGroup( ChannelGroupI *oldchannelgroup, ChannelGroupI *newchannelgroup,
                                          bool forcedspreconnect );
    virtual             ~ChannelStream();
    virtual FMOD_RESULT alloc();
    virtual FMOD_RESULT start();
    virtual FMOD_RESULT update( int delta );
    virtual FMOD_RESULT updateStream();
    virtual FMOD_RESULT stop();
    virtual FMOD_RESULT setPaused( bool paused );
    virtual FMOD_RESULT setVolume( float volume );
    virtual FMOD_RESULT setFrequency( float frequency );
    virtual FMOD_RESULT setPan( float pan, float fbpan );
    virtual FMOD_RESULT setDSPClockDelay();
    virtual FMOD_RESULT setSpeakerMix( float frontleft, float frontright, float center, float lfe,
                                       float backleft, float backright, float sideleft, float sideright );
    virtual FMOD_RESULT setSpeakerLevels( int speaker, float *levels, int numlevels );
    virtual FMOD_RESULT setPosition( unsigned int position, unsigned int postype );
    virtual FMOD_RESULT setPositionEx( unsigned int position, unsigned int postype, bool fromsetposition );
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

#endif /* _FMOD_CHANNEL_STREAM_H */
