/*
 * fmod_system.cpp: FMOD::System, the public handle of a system. Each method checks
 * its handle and forwards to the internal SystemI.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_system.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_system.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_systemi.h>

namespace FMOD {

/*
====================
System::release
====================
*/
FMOD_RESULT System::release() {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->release();
} // 0x8037AD14

/*
====================
System::getSoftwareFormat
====================
*/
FMOD_RESULT System::getSoftwareFormat( int *samplerate, FMOD_SOUND_FORMAT *format, int *numoutputchannels,
                                       int *maxinputchannels, FMOD_DSP_RESAMPLER *resamplemethod, int *bits ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getSoftwareFormat( samplerate, format, numoutputchannels, maxinputchannels, resamplemethod, bits );
} // 0x8037AD4C

/*
====================
System::getDSPBufferSize
====================
*/
FMOD_RESULT System::getDSPBufferSize( unsigned int *bufferlength, int *numbuffers ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getDSPBufferSize( bufferlength, numbuffers );
} // 0x8037AEA4

/*
====================
System::setFileSystem
====================
*/
FMOD_RESULT System::setFileSystem( FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose,
                                   FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek,
                                   int blockalign ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->setFileSystem( useropen, userclose, userread, userseek, blockalign );
} // 0x8037AEFC

/*
====================
System::setAdvancedSettings
====================
*/
FMOD_RESULT System::setAdvancedSettings( FMOD_ADVANCEDSETTINGS *settings ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->setAdvancedSettings( settings );
} // 0x8037AF64

/*
====================
System::getAdvancedSettings
====================
*/
FMOD_RESULT System::getAdvancedSettings( FMOD_ADVANCEDSETTINGS *settings ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getAdvancedSettings( settings );
} // 0x8037AFAC

/*
====================
System::setSpeakerMode
====================
*/
FMOD_RESULT System::setSpeakerMode( FMOD_SPEAKERMODE speakermode ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->setSpeakerMode( speakermode );
} // 0x8037AFF4

/*
====================
System::getSpeakerMode
====================
*/
FMOD_RESULT System::getSpeakerMode( FMOD_SPEAKERMODE *speakermode ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getSpeakerMode( speakermode );
} // 0x8037B03C

/*
====================
System::getNumPlugins
====================
*/
FMOD_RESULT System::getNumPlugins( FMOD_PLUGINTYPE plugintype, int *numplugins ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getNumPlugins( plugintype, numplugins );
} // 0x8037B09C

/*
====================
System::getPluginHandle
====================
*/
FMOD_RESULT System::getPluginHandle( FMOD_PLUGINTYPE plugintype, int index, unsigned int *handle ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getPluginHandle( plugintype, index, handle );
} // 0x8037B0F4

/*
====================
System::getPluginInfo
====================
*/
FMOD_RESULT System::getPluginInfo( unsigned int handle, FMOD_PLUGINTYPE *plugintype, char *name,
                                   int namelen, unsigned int *version ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getPluginInfo( handle, plugintype, name, namelen, version );
} // 0x8037B15C

/*
====================
System::createDSPByPlugin
====================
*/
FMOD_RESULT System::createDSPByPlugin( unsigned int handle, DSP **dsp ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->createDSPByPlugin( handle, (DSPI **)dsp );
} // 0x8037B1C4

/*
====================
System::init
====================
*/
FMOD_RESULT System::init( int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->init( maxchannels, flags, extradriverdata );
} // 0x8037B21C

/*
====================
System::update
====================
*/
FMOD_RESULT System::update() {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->update();
} // 0x8037B284

/*
====================
System::set3DSettings
====================
*/
FMOD_RESULT System::set3DSettings( float dopplerscale, float distancefactor, float rolloffscale ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->set3DSettings( dopplerscale, distancefactor, rolloffscale );
} // 0x8037B2BC

/*
====================
System::get3DSettings
====================
*/
FMOD_RESULT System::get3DSettings( float *dopplerscale, float *distancefactor, float *rolloffscale ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->get3DSettings( dopplerscale, distancefactor, rolloffscale );
} // 0x8037B324

/*
====================
System::set3DNumListeners
====================
*/
FMOD_RESULT System::set3DNumListeners( int numlisteners ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->set3DNumListeners( numlisteners );
} // 0x8037B38C

/*
====================
System::get3DNumListeners
====================
*/
FMOD_RESULT System::get3DNumListeners( int *numlisteners ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->get3DNumListeners( numlisteners );
} // 0x8037B3D4

/*
====================
System::set3DListenerAttributes
====================
*/
FMOD_RESULT System::set3DListenerAttributes( int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel,
                                             const FMOD_VECTOR *forward, const FMOD_VECTOR *up ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->set3DListenerAttributes( listener, pos, vel, forward, up );
} // 0x8037B41C

/*
====================
System::get3DListenerAttributes
====================
*/
FMOD_RESULT System::get3DListenerAttributes( int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel,
                                             FMOD_VECTOR *forward, FMOD_VECTOR *up ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->get3DListenerAttributes( listener, pos, vel, forward, up );
} // 0x8037B484

/*
====================
System::getStreamBufferSize
====================
*/
FMOD_RESULT System::getStreamBufferSize( unsigned int *filebuffersize, unsigned int *filebuffersizetype ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getStreamBufferSize( filebuffersize, filebuffersizetype );
} // 0x8037B4EC

/*
====================
System::getVersion
====================
*/
FMOD_RESULT System::getVersion( unsigned int *version ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getVersion( version );
} // 0x8037B544

/*
====================
System::getHardwareChannels
====================
*/
FMOD_RESULT System::getHardwareChannels( int *num2d, int *num3d, int *total ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getHardwareChannels( num2d, num3d, total );
} // 0x8037B58C

/*
====================
System::getSoundRAM
====================
*/
FMOD_RESULT System::getSoundRAM( int *currentalloced, int *maxalloced, int *total ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getSoundRAM( currentalloced, maxalloced, total );
} // 0x8037B5F4

/*
====================
System::createSound
====================
*/
FMOD_RESULT System::createSound( const char *name_or_data, FMOD_MODE mode,
                                 FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->createSound( name_or_data, mode, exinfo, (SoundI **)sound );
} // 0x8037B65C

/*
====================
System::createStream
====================
*/
FMOD_RESULT System::createStream( const char *name_or_data, FMOD_MODE mode,
                                  FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->createStream( name_or_data, mode, exinfo, (SoundI **)sound );
} // 0x8037B6D4

/*
====================
System::createDSPByType
====================
*/
FMOD_RESULT System::createDSPByType( FMOD_DSP_TYPE type, DSP **dsp ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->createDSPByType( type, (DSPI **)dsp );
} // 0x8037B74C

/*
====================
System::createChannelGroup
====================
*/
FMOD_RESULT System::createChannelGroup( const char *name, ChannelGroup **channelgroup ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->createChannelGroup( name, (ChannelGroupI **)channelgroup );
} // 0x8037B7A4

/*
====================
System::createReverb
====================
*/
FMOD_RESULT System::createReverb( Reverb **reverb ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->createReverb( (ReverbI **)reverb );
} // 0x8037B7FC

/*
====================
System::playSound
====================
*/
FMOD_RESULT System::playSound( FMOD_CHANNELINDEX channelid, Sound *sound, bool paused, Channel **channel ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->playSound( channelid, (SoundI *)sound, paused, (ChannelI **)channel );
} // 0x8037B844

/*
====================
System::playDSP
====================
*/
FMOD_RESULT System::playDSP( FMOD_CHANNELINDEX channelid, DSP *dsp, bool paused, Channel **channel ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->playDSP( channelid, (DSPI *)dsp, paused, (ChannelI **)channel );
} // 0x8037B8BC

/*
====================
System::getMasterChannelGroup
====================
*/
FMOD_RESULT System::getMasterChannelGroup( ChannelGroup **channelgroup ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getMasterChannelGroup( (ChannelGroupI **)channelgroup );
} // 0x8037B934

/*
====================
System::getMasterSoundGroup
====================
*/
FMOD_RESULT System::getMasterSoundGroup( SoundGroup **soundgroup ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getMasterSoundGroup( (SoundGroupI **)soundgroup );
} // 0x8037B97C

/*
====================
System::setReverbProperties
====================
*/
FMOD_RESULT System::setReverbProperties( const FMOD_REVERB_PROPERTIES *prop ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->setReverbProperties( prop, false );
} // 0x8037B9C4

/*
====================
System::getReverbProperties
====================
*/
FMOD_RESULT System::getReverbProperties( FMOD_REVERB_PROPERTIES *prop ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getReverbProperties( prop );
} // 0x8037BA10

/*
====================
System::setReverbAmbientProperties
====================
*/
FMOD_RESULT System::setReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->setReverbAmbientProperties( prop );
} // 0x8037BA58

/*
====================
System::getReverbAmbientProperties
====================
*/
FMOD_RESULT System::getReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getReverbAmbientProperties( prop );
} // 0x8037BA90

/*
====================
System::getDSPClock
====================
*/
FMOD_RESULT System::getDSPClock( unsigned int *hi, unsigned int *lo ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->getDSPClock( hi, lo );
} // 0x8037BAC8

/*
====================
System::setUserData
====================
*/
FMOD_RESULT System::setUserData( void *_userdata ) {
    FMOD_RESULT result;
    SystemI    *systemi;

    result = SystemI::validate( this, &systemi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return systemi->setUserData( _userdata );
} // 0x8037BB20

} // namespace FMOD
