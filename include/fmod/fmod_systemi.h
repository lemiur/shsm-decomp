/*
 * FMOD::SystemI, the internal object behind an FMOD::System handle. Only what fmod.cpp needs is
 * declared: the list link (systems are kept in a list), the system index and the object's size.
 */
#ifndef _FMOD_SYSTEMI_H
#define _FMOD_SYSTEMI_H

#include "fmod_linkedlist.h"
#include "fmod_channeli.h"
#include "fmod_dspi.h"
#include "fmod_dsp_connectionpool.h"
#include "fmod_reverbi.h"
#include "fmod_downmix.h"
#include "fmod_memory.h"
#include "fmod_soundi.h"
#include "fmod_speakerlevels_pool.h"
#include "fmod_time.h"

namespace FMOD {

class SoundI;
class SoundGroupI;
class Output;
class PluginFactory;
class ChannelGroupI;
class System;

class SystemI : public LinkedListNode {
public:
    static FMOD_RESULT validate( System *system, SystemI **systemi );

    FMOD_RESULT   release();

    // Name and signature from Gormiti's DWARF (sibling.exe fn stopSound); the sound's channels are stopped.
    FMOD_RESULT   stopSound( SoundI *sound );

    // Name and signature from Gormiti's DWARF (sibling.exe fn getDSPBufferSize, fmod_systemi.cpp).
    FMOD_RESULT   getDSPBufferSize( unsigned int *bufferlength, int *numbuffers );

    // The software mixer's format. Name and parameters from the public System::getSoftwareFormat, whose
    // retail body is this one expanded; an argument left NULL is not written.
    FMOD_RESULT   getSoftwareFormat( int *samplerate, FMOD_SOUND_FORMAT *format, int *numoutputchannels,
                                     int *maxinputchannels, FMOD_DSP_RESAMPLER *resamplemethod,
                                     int *bits ) {
        if ( samplerate ) {
            *samplerate = mOutputRate;
        }
        if ( format ) {
            *format = mOutputFormat;
        }
        if ( numoutputchannels ) {
            *numoutputchannels = mMaxOutputChannels;
        }
        if ( maxinputchannels ) {
            *maxinputchannels = mMaxInputChannels;
        }
        if ( resamplemethod ) {
            *resamplemethod = mResampleMethod;
        }
        if ( bits ) {
            SoundI::getBitsFromFormat( mOutputFormat, bits );
        }
        return FMOD_OK;
    }

    FMOD_RESULT   setFileSystem( FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose,
                                 FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek,
                                 int blockalign );
    FMOD_RESULT   setAdvancedSettings( FMOD_ADVANCEDSETTINGS *settings );
    FMOD_RESULT   getAdvancedSettings( FMOD_ADVANCEDSETTINGS *settings );
    FMOD_RESULT   setSpeakerMode( FMOD_SPEAKERMODE speakermode );

    FMOD_RESULT   getSpeakerMode( FMOD_SPEAKERMODE *speakermode ) {
        if ( !speakermode ) {
            return FMOD_ERR_INVALID_PARAM;
        }
        *speakermode = mSpeakerMode;
        return FMOD_OK;
    }

    FMOD_RESULT   getNumPlugins( FMOD_PLUGINTYPE plugintype, int *numplugins );
    FMOD_RESULT   getPluginHandle( FMOD_PLUGINTYPE plugintype, int index, unsigned int *handle );
    FMOD_RESULT   getPluginInfo( unsigned int handle, FMOD_PLUGINTYPE *plugintype, char *name,
                                 int namelen, unsigned int *version );
    FMOD_RESULT   createDSPByPlugin( unsigned int handle, DSPI **dsp );
    FMOD_RESULT   init( int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata );
    FMOD_RESULT   update();
    FMOD_RESULT   set3DSettings( float dopplerscale, float distancefactor, float rolloffscale );
    FMOD_RESULT   get3DSettings( float *dopplerscale, float *distancefactor, float *rolloffscale );
    FMOD_RESULT   set3DNumListeners( int numlisteners );
    FMOD_RESULT   get3DNumListeners( int *numlisteners );
    FMOD_RESULT   set3DListenerAttributes( int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel,
                                           const FMOD_VECTOR *forward, const FMOD_VECTOR *up );
    FMOD_RESULT   get3DListenerAttributes( int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel,
                                           FMOD_VECTOR *forward, FMOD_VECTOR *up );
    FMOD_RESULT   getStreamBufferSize( unsigned int *filebuffersize, unsigned int *filebuffersizetype );
    FMOD_RESULT   getVersion( unsigned int *version );
    FMOD_RESULT   getHardwareChannels( int *num2d, int *num3d, int *total );
    FMOD_RESULT   getSoundRAM( int *currentalloced, int *maxalloced, int *total );
    FMOD_RESULT   createSound( const char *name_or_data, FMOD_MODE mode,
                               FMOD_CREATESOUNDEXINFO *exinfo, SoundI **sound );
    FMOD_RESULT   createStream( const char *name_or_data, FMOD_MODE mode,
                                FMOD_CREATESOUNDEXINFO *exinfo, SoundI **sound );
    FMOD_RESULT   createDSPByType( FMOD_DSP_TYPE type, DSPI **dsp );
    FMOD_RESULT   createChannelGroup( const char *name, ChannelGroupI **channelgroup );
    FMOD_RESULT   createReverb( ReverbI **reverb );
    FMOD_RESULT   playSound( FMOD_CHANNELINDEX channelid, SoundI *sound, bool paused, ChannelI **channel );
    FMOD_RESULT   playDSP( FMOD_CHANNELINDEX channelid, DSPI *dsp, bool paused, ChannelI **channel );
    FMOD_RESULT   getMasterChannelGroup( ChannelGroupI **channelgroup );
    FMOD_RESULT   getMasterSoundGroup( SoundGroupI **soundgroup );
    FMOD_RESULT   setReverbProperties( const FMOD_REVERB_PROPERTIES *prop, bool ambient );
    FMOD_RESULT   getReverbProperties( FMOD_REVERB_PROPERTIES *prop );

    FMOD_RESULT   setReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop ) {
        return FMOD_ERR_UNSUPPORTED;
    }
    FMOD_RESULT   getReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop ) {
        return FMOD_ERR_UNSUPPORTED;
    }

    FMOD_RESULT   getDSPClock( unsigned int *hi, unsigned int *lo );
    FMOD_RESULT   setUserData( void *userdata );

    bool          mInitialized;         // 0xC
    bool          mPluginsLoaded;       // 0xD
    unsigned char field_0xE[0x10 - 0xE];
    unsigned int  mMainThreadID;        // 0x10, the thread that created the system
    FMOD_INITFLAGS mFlags;              // 0x14
    unsigned char field_0x18[0x14C - 0x18];
    int           mNumChannels;         // 0x14C, number of real channels
    unsigned char field_0x150[0x4C8 - 0x150];
    Output       *mOutput;              // 0x4C8, the output plugin
    unsigned char field_0x4CC[0x4D0 - 0x4CC];
    FMOD_SOUND_FORMAT mOutputFormat;    // 0x4D0, the format the software mixer outputs
    int           mOutputRate;          // 0x4D4, the sample rate of the output
    unsigned char field_0x4D8[0x4DC - 0x4D8];
    int           mMaxInputChannels;    // 0x4DC
    int           mMaxOutputChannels;   // 0x4E0
    unsigned char field_0x4E4[0x4F0 - 0x4E4];
    unsigned int  mDSPBlockSize;        // 0x4F0, samples in one DSP block
    unsigned int  mDSPBufferSize;       // 0x4F4
    float        *mDSPReadBuff[2];      // 0x4F8, the two buffers the sound card's output alternates between
    unsigned char field_0x500[0x708 - 0x500];
    DSPConnectionPool mDSPConnectionPool;   // 0x708, the DSP graph's connections (Gormiti DWARF)
    FMOD_OS_CRITICALSECTION *mDSPCrit;  // 0x1130, guards the DSP tree
    FMOD_OS_CRITICALSECTION *mDSPLockCrit; // 0x1134, guards DSP execution (Gormiti DWARF)
    FMOD_OS_CRITICALSECTION *mDSPConnectionCrit; // 0x1138, guards the connection pool (Gormiti DWARF)
    FMOD_OS_CRITICALSECTION *mDSPCodecPoolInitCrit; // 0x113C
    bool          mDSPActive;           // 0x1140
    unsigned char field_0x1141[0x1144 - 0x1141];
    DSPI         *mDSPSoundCard;        // 0x1144
    unsigned char field_0x1148[0x114C - 0x1148];
    TimeStamp     mDSPTimeStamp;        // 0x114C, times the mixer (stampIn/stampOut around each pass)
    int           mDSPReadBuffIndex;    // 0x1184
    unsigned char field_0x1188[0x91D0 - 0x1188];
    FMOD_UINT64P  mDSPClock;            // 0x91D0
    unsigned char field_0x91D8[0x93AC - 0x91D8];
    PluginFactory *mPluginFactory;      // 0x93AC
    unsigned char field_0x93B0[0x9544 - 0x93B0];
    int           mIndex;               // 0x9544, 1-based slot among the systems created
    unsigned char field_0x9548[0x9564 - 0x9548];
    ChannelGroupI *mChannelGroup;       // 0x9564, the master group
    unsigned char field_0x9568[0x95E0 - 0x9568];
    FMOD_DSP_RESAMPLER mResampleMethod; // 0x95E0
    MemSingleton  mMultiSubSampleLockBuffer;   // 0x95E4 (Gormiti DWARF); freed by the destructor
    FMOD_OS_CRITICALSECTION *mMultiSubSampleLockBufferCrit; // 0x95EC; name and layout: Gormiti DWARF
    FMOD_RESULT (*mOpenRiderCallback)( const char *, int, unsigned int *, void **, void ** ); // 0x95F0 (Gormiti DWARF)
    FMOD_RESULT (*mCloseRiderCallback)( void *, void * );                             // 0x95F4
    FMOD_RESULT (*mReadRiderCallback)( void *, void *, unsigned int, unsigned int *, void * ); // 0x95F8
    FMOD_RESULT (*mSeekRiderCallback)( void *, unsigned int, void * );                // 0x95FC
    SpeakerLevelsPool mSpeakerLevelsPool; // 0x9600
    unsigned char field_0x9608[0x9610 - 0x9608];
    FMOD_SPEAKERMODE mSpeakerMode;      // 0x9610
    unsigned char field_0x9614[0x9714 - 0x9614];
    ReverbI       mReverbGlobal;        // 0x9714
    Downmix      *mDownmix;             // 0x995C, NULL on the Wii
    unsigned char field_0x9960[0x9AEC - 0x9960];
                                        // 0x9AEC, vtable pointer (sizeof 0x9AF0)

    SystemI();                          // 0x8034F1C8
    FMOD_RESULT createDSP( FMOD_DSP_DESCRIPTION *description, DSPI **dsp );
    FMOD_RESULT createDSP( FMOD_DSP_DESCRIPTION_EX *description, DSPI **dsp, bool uselocks );
    FMOD_RESULT flushDSPConnectionRequests( bool calledfrommainthread );
    // vtable lbl_8044DDC8, one slot. Implicitly empty: its weak copy (0x8033D1B0) is emitted in
    // fmod_globals, the first unit that destroys a SystemI.
    virtual ~SystemI() {}

    // Sound creation (fmod_async)
    FMOD_RESULT createSoundInternal( const char *name_or_data, FMOD_MODE mode, unsigned int buffersize,
                                     unsigned int buffersizetype, FMOD_CREATESOUNDEXINFO *exinfo,
                                     bool calledfromasync, SoundI **sound );
};

} // namespace FMOD

#endif /* _FMOD_SYSTEMI_H */
