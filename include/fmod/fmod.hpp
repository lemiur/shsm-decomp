/*
 * FMOD Ex 4.x public C++ API: the declarations this project needs, written from the public API
 * (fmod.hpp of the FMOD Ex programmer's API). The public classes are handles: a System, Sound or
 * Channel pointer is the internal object itself, and each method validates it before forwarding.
 */
#ifndef _FMOD_HPP
#define _FMOD_HPP

#include <fmod/fmod.h>
#include <fmod/fmod_dsp.h>

namespace FMOD {

class Sound;
class SoundGroup;
class ChannelGroup;
class Reverb;

/*
 * 'DSP' API (only the member the Event envelope unit calls).
 */
class DSP {
private:
    DSP();
    DSP( const DSP& );

public:
    FMOD_RESULT F_API release();
    // ---- added for fmod_eventimpl_simple ----
    FMOD_RESULT F_API getDefaults( float *frequency, float *volume, float *pan, int *priority );
    // ---- added for fmod_dsp ----
    FMOD_RESULT F_API disconnectFrom( DSP *target );
    FMOD_RESULT F_API disconnectAll( bool inputs, bool outputs );
    FMOD_RESULT F_API setActive( bool active );
    FMOD_RESULT F_API setBypass( bool bypass );
    FMOD_RESULT F_API setParameter( int index, float value );
    FMOD_RESULT F_API getParameterInfo( int index, char *name, char *label, char *description, int descriptionlen,
                                        float *min, float *max );
    FMOD_RESULT F_API getType( FMOD_DSP_TYPE *type );
    FMOD_RESULT F_API setDefaults( float frequency, float volume, float pan, int priority );
    // ---- end fmod_dsp ----
};

/*
 * 'Sound' API.
 */
class Sound {
private:
    Sound();
    Sound( const Sound& );

public:
    FMOD_RESULT F_API release();
    FMOD_RESULT F_API setDefaults( float frequency, float volume, float pan, int priority );
    FMOD_RESULT F_API getDefaults( float *frequency, float *volume, float *pan, int *priority );
    FMOD_RESULT F_API setSubSound( int index, Sound *subsound );
    FMOD_RESULT F_API getSubSound( int index, Sound **subsound );
    FMOD_RESULT F_API getLength( unsigned int *length, FMOD_TIMEUNIT lengthtype );
    FMOD_RESULT F_API getFormat( FMOD_SOUND_TYPE *type, FMOD_SOUND_FORMAT *format, int *channels, int *bits );
    FMOD_RESULT F_API getNumSubSounds( int *numsubsounds );
    FMOD_RESULT F_API getOpenState( FMOD_OPENSTATE *openstate, unsigned int *percentbuffered, bool *starving );
    FMOD_RESULT F_API getNumSyncPoints( int *numsyncpoints );
    FMOD_RESULT F_API getSyncPoint( int index, FMOD_SYNCPOINT **point );
    FMOD_RESULT F_API getSyncPointInfo( FMOD_SYNCPOINT *point, char *name, int namelen, unsigned int *offset,
                                        FMOD_TIMEUNIT offsettype );
    FMOD_RESULT F_API setMode( FMOD_MODE mode );
    FMOD_RESULT F_API getMode( FMOD_MODE *mode );
    FMOD_RESULT F_API setLoopCount( int loopcount );
    FMOD_RESULT F_API getMusicNumChannels( int *numchannels );
    FMOD_RESULT F_API setMusicChannelVolume( int channel, float volume );
    FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                     unsigned int *memoryused_array );
};

/*
 * 'Channel' API.
 */
class Channel {
private:
    Channel();
    Channel( const Channel& );

public:
    FMOD_RESULT F_API getCurrentSound( Sound **sound );
    FMOD_RESULT F_API getUserData( void **userdata );
    FMOD_RESULT F_API setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    // ---- added for fmod_eventimpl_simple ----
    FMOD_RESULT F_API getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );
    FMOD_RESULT F_API isPlaying( bool *isplaying );
    FMOD_RESULT F_API setVolume( float volume );
    FMOD_RESULT F_API setFrequency( float frequency );
    FMOD_RESULT F_API getMode( FMOD_MODE *mode );
    FMOD_RESULT F_API set3DSpread( float angle );
    FMOD_RESULT F_API set3DPanLevel( float level );
    FMOD_RESULT F_API getAudibility( float *audibility );
    FMOD_RESULT F_API setPaused( bool paused );
    FMOD_RESULT F_API getPaused( bool *paused );
    FMOD_RESULT F_API setDelay( FMOD_DELAYTYPE delaytype, unsigned int delayhi, unsigned int delaylo );
    // ---- added for climax/Audio/Source ----
    FMOD_RESULT F_API stop();
    FMOD_RESULT F_API setPriority( int priority );
    FMOD_RESULT F_API set3DAttributes( const FMOD_VECTOR *pos, const FMOD_VECTOR *vel );
    FMOD_RESULT F_API get3DAttributes( FMOD_VECTOR *pos, FMOD_VECTOR *vel );
    // ---- added for climax/Audio/System ----
    FMOD_RESULT F_API setChannelGroup( ChannelGroup *channelgroup );
    FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                     unsigned int *memoryused_array );
    // ---- added for fmod_channel ----
    FMOD_RESULT F_API getVolume( float *volume );
    FMOD_RESULT F_API setPan( float pan );
    FMOD_RESULT F_API getDelay( FMOD_DELAYTYPE delaytype, unsigned int *delayhi, unsigned int *delaylo );
    FMOD_RESULT F_API setSpeakerMix( float frontleft, float frontright, float center, float lfe, float backleft,
                                     float backright, float sideleft, float sideright );
    FMOD_RESULT F_API setSpeakerLevels( FMOD_SPEAKER speaker, float *levels, int numlevels );
    FMOD_RESULT F_API setInputChannelMix( float *levels, int numlevels );
    FMOD_RESULT F_API setMute( bool mute );
    FMOD_RESULT F_API setPosition( unsigned int position, FMOD_TIMEUNIT postype );
    FMOD_RESULT F_API getPosition( unsigned int *position, FMOD_TIMEUNIT postype );
    FMOD_RESULT F_API setCallback( FMOD_RESULT ( F_CALLBACK *callback )( FMOD_CHANNEL *channel,
                                                                        FMOD_CHANNEL_CALLBACKTYPE type,
                                                                        void *data1, void *data2 ) );
    FMOD_RESULT F_API set3DMinMaxDistance( float mindistance, float maxdistance );
    FMOD_RESULT F_API set3DConeOrientation( FMOD_VECTOR *orientation );
    FMOD_RESULT F_API set3DOcclusion( float directocclusion, float reverbocclusion );
    FMOD_RESULT F_API set3DDopplerLevel( float level );
    FMOD_RESULT F_API setMode( FMOD_MODE mode );
    FMOD_RESULT F_API setUserData( void *userdata );
};

// ---- added for fmod_eventimpl_simple ----
/*
 * 'System' API (the members the event system's update calls).
 */
class System {
private:
    System();
    System( const System& );
public:
    FMOD_RESULT F_API release();
    FMOD_RESULT F_API getSoftwareFormat( int *samplerate, FMOD_SOUND_FORMAT *format, int *numoutputchannels,
                                         int *maxinputchannels, FMOD_DSP_RESAMPLER *resamplemethod, int *bits );
    FMOD_RESULT F_API getDSPBufferSize( unsigned int *bufferlength, int *numbuffers );
    FMOD_RESULT F_API setFileSystem( FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose,
                                     FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek,
                                     int blockalign );
    FMOD_RESULT F_API setAdvancedSettings( FMOD_ADVANCEDSETTINGS *settings );
    FMOD_RESULT F_API getAdvancedSettings( FMOD_ADVANCEDSETTINGS *settings );
    FMOD_RESULT F_API setSpeakerMode( FMOD_SPEAKERMODE speakermode );
    FMOD_RESULT F_API getSpeakerMode( FMOD_SPEAKERMODE *speakermode );
    FMOD_RESULT F_API getNumPlugins( FMOD_PLUGINTYPE plugintype, int *numplugins );
    FMOD_RESULT F_API getPluginHandle( FMOD_PLUGINTYPE plugintype, int index, unsigned int *handle );
    FMOD_RESULT F_API getPluginInfo( unsigned int handle, FMOD_PLUGINTYPE *plugintype, char *name,
                                     int namelen, unsigned int *version );
    FMOD_RESULT F_API createDSPByPlugin( unsigned int handle, DSP **dsp );
    FMOD_RESULT F_API init( int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata );
    FMOD_RESULT F_API update();
    FMOD_RESULT F_API set3DSettings( float dopplerscale, float distancefactor, float rolloffscale );
    FMOD_RESULT F_API get3DSettings( float *dopplerscale, float *distancefactor, float *rolloffscale );
    FMOD_RESULT F_API set3DNumListeners( int numlisteners );
    FMOD_RESULT F_API get3DNumListeners( int *numlisteners );
    FMOD_RESULT F_API set3DListenerAttributes( int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel,
                                               const FMOD_VECTOR *forward, const FMOD_VECTOR *up );
    FMOD_RESULT F_API get3DListenerAttributes( int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel,
                                               FMOD_VECTOR *forward, FMOD_VECTOR *up );
    FMOD_RESULT F_API getStreamBufferSize( unsigned int *filebuffersize, unsigned int *filebuffersizetype );
    FMOD_RESULT F_API getVersion( unsigned int *version );
    FMOD_RESULT F_API getHardwareChannels( int *num2d, int *num3d, int *total );
    FMOD_RESULT F_API getSoundRAM( int *currentalloced, int *maxalloced, int *total );
    FMOD_RESULT F_API createSound( const char *name_or_data, FMOD_MODE mode,
                                   FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound );
    FMOD_RESULT F_API createStream( const char *name_or_data, FMOD_MODE mode,
                                    FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound );
    FMOD_RESULT F_API createDSPByType( FMOD_DSP_TYPE type, DSP **dsp );
    FMOD_RESULT F_API createChannelGroup( const char *name, ChannelGroup **channelgroup );
    FMOD_RESULT F_API createReverb( Reverb **reverb );
    FMOD_RESULT F_API playSound( FMOD_CHANNELINDEX channelid, Sound *sound, bool paused, Channel **channel );
    FMOD_RESULT F_API playDSP( FMOD_CHANNELINDEX channelid, DSP *dsp, bool paused, Channel **channel );
    FMOD_RESULT F_API getMasterChannelGroup( ChannelGroup **channelgroup );
    FMOD_RESULT F_API getMasterSoundGroup( SoundGroup **soundgroup );
    FMOD_RESULT F_API setReverbProperties( const FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT F_API getReverbProperties( FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT F_API setReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT F_API getReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT F_API getDSPClock( unsigned int *hi, unsigned int *lo );
    FMOD_RESULT F_API setUserData( void *userdata );
};

/*
 * 'Reverb' API.
 */
class Reverb {
private:
    Reverb();
    Reverb( const Reverb& );

public:
    FMOD_RESULT F_API release();
    FMOD_RESULT F_API set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance );
    FMOD_RESULT F_API get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance );
    FMOD_RESULT F_API setProperties( const FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT F_API getProperties( FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT F_API setActive( bool active );
    FMOD_RESULT F_API getActive( bool *active );
};

class DSPConnection;

/*
 * 'ChannelGroup' API (the part the music system uses).
 */
class ChannelGroup {
private:
    ChannelGroup();
    ChannelGroup( const ChannelGroup& );

public:
    FMOD_RESULT F_API addGroup( ChannelGroup *group );
    FMOD_RESULT F_API overrideReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    // ---- added for fmod_eventimpl_simple ----
    FMOD_RESULT F_API getNumChannels( int *numchannels );
    FMOD_RESULT F_API getChannel( int index, Channel **channel );
    // ---- added for fmod_eventgroupi ----
    FMOD_RESULT F_API release();
    // ---- end fmod_eventgroupi ----
    FMOD_RESULT F_API setVolume( float volume );
    FMOD_RESULT F_API getVolume( float *volume );
    FMOD_RESULT F_API setPitch( float pitch );
    FMOD_RESULT F_API setPaused( bool paused );
    FMOD_RESULT F_API setMute( bool mute );
    FMOD_RESULT F_API getMute( bool *mute );
    FMOD_RESULT F_API stop();
    FMOD_RESULT F_API overrideSpeakerMix( float frontleft, float frontright, float center, float lfe, float backleft, float backright, float sideleft, float sideright );
    FMOD_RESULT F_API getDSPHead( DSP **dsp );
    FMOD_RESULT F_API addDSP( DSP *dsp, DSPConnection **connection );
};

} // namespace FMOD

#endif /* _FMOD_HPP */
