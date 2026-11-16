/*
 * fmod_channelgroup.cpp: FMOD::ChannelGroup, the public handle of a channel group. Each method checks
 * its handle and forwards to the internal ChannelGroupI.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_channelgroup.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_channelgroupi.h>

namespace FMOD {

/*
====================
ChannelGroup::release
====================
*/
FMOD_RESULT ChannelGroup::release() {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->release();
} // 0x80334D18

/*
====================
ChannelGroup::setVolume
====================
*/
FMOD_RESULT ChannelGroup::setVolume( float volume ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->setVolume( volume );
} // 0x80334D50

/*
====================
ChannelGroup::getVolume
====================
*/
FMOD_RESULT ChannelGroup::getVolume( float *volume ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->getVolume( volume );
} // 0x80334D98

/*
====================
ChannelGroup::setPitch
====================
*/
FMOD_RESULT ChannelGroup::setPitch( float pitch ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->setPitch( pitch );
} // 0x80334DE0

/*
====================
ChannelGroup::setPaused
====================
*/
FMOD_RESULT ChannelGroup::setPaused( bool paused ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->setPaused( paused, true );
} // 0x80334E28

/*
====================
ChannelGroup::setMute
====================
*/
FMOD_RESULT ChannelGroup::setMute( bool mute ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->setMute( mute, true );
} // 0x80334E74

/*
====================
ChannelGroup::getMute
====================
*/
FMOD_RESULT ChannelGroup::getMute( bool *mute ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->getMute( mute );
} // 0x80334EC0

/*
====================
ChannelGroup::stop
====================
*/
FMOD_RESULT ChannelGroup::stop() {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->stop();
} // 0x80334F08

/*
====================
ChannelGroup::overrideReverbProperties
====================
*/
FMOD_RESULT ChannelGroup::overrideReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->overrideReverbProperties( prop );
} // 0x80334F40

/*
====================
ChannelGroup::overrideSpeakerMix
====================
*/
FMOD_RESULT ChannelGroup::overrideSpeakerMix( float frontleft, float frontright, float center, float lfe, float backleft, float backright, float sideleft, float sideright ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->overrideSpeakerMix( frontleft, frontright, center, lfe, backleft, backright, sideleft, sideright );
} // 0x80334F88

/*
====================
ChannelGroup::addGroup
====================
*/
FMOD_RESULT ChannelGroup::addGroup( ChannelGroup *group ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->addGroup( reinterpret_cast<ChannelGroupI *>( group ) );
} // 0x80335010

/*
====================
ChannelGroup::getDSPHead
====================
*/
FMOD_RESULT ChannelGroup::getDSPHead( DSP **dsp ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->getDSPHead( reinterpret_cast<DSPI **>( dsp ) );
} // 0x80335058

/*
====================
ChannelGroup::addDSP
====================
*/
FMOD_RESULT ChannelGroup::addDSP( DSP *dsp, DSPConnection **connection ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->addDSP( reinterpret_cast<DSPI *>( dsp ), reinterpret_cast<DSPConnectionI **>( connection ) );
} // 0x803350A0

/*
====================
ChannelGroup::getNumChannels
====================
*/
FMOD_RESULT ChannelGroup::getNumChannels( int *numchannels ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->getNumChannels( numchannels );
} // 0x803350F8

/*
====================
ChannelGroup::getChannel
====================
*/
FMOD_RESULT ChannelGroup::getChannel( int index, Channel **channel ) {
    FMOD_RESULT    result;
    ChannelGroupI *channelgroupi;

    result = ChannelGroupI::validate( this, &channelgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channelgroupi->getChannel( index, channel );
} // 0x80335140

} // namespace FMOD
