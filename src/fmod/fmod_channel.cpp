/*
 * fmod_channel.cpp: FMOD::Channel, the public handle of a channel. Each method checks
 * its handle and forwards to the internal ChannelI.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_channel.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_channeli.h>

namespace FMOD {

/*
====================
Channel::stop
====================
*/
FMOD_RESULT Channel::stop() {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->stop();
} // 0x80378DC0

/*
====================
Channel::setPaused
====================
*/
FMOD_RESULT Channel::setPaused( bool paused ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setPaused( paused );
} // 0x80378DF8

/*
====================
Channel::getPaused
====================
*/
FMOD_RESULT Channel::getPaused( bool *paused ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( paused ) {
            *paused = false;
        }
        return result;
    }

    return channeli->getPaused( paused );
} // 0x80378E40

/*
====================
Channel::setVolume
====================
*/
FMOD_RESULT Channel::setVolume( float volume ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setVolume( volume, false );
} // 0x80378E98

/*
====================
Channel::getVolume
====================
*/
FMOD_RESULT Channel::getVolume( float *volume ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( volume ) {
            *volume = 0.0f;
        }
        return result;
    }

    return channeli->getVolume( volume );
} // 0x80378EE4

/*
====================
Channel::setFrequency
====================
*/
FMOD_RESULT Channel::setFrequency( float frequency ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setFrequency( frequency );
} // 0x80378F3C

/*
====================
Channel::setPan
====================
*/
FMOD_RESULT Channel::setPan( float pan ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setPan( pan, true );
} // 0x80378F84

/*
====================
Channel::setDelay
====================
*/
FMOD_RESULT Channel::setDelay( FMOD_DELAYTYPE delaytype, unsigned int delayhi, unsigned int delaylo ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setDelay( delaytype, delayhi, delaylo );
} // 0x80378FD0

/*
====================
Channel::getDelay
====================
*/
FMOD_RESULT Channel::getDelay( FMOD_DELAYTYPE delaytype, unsigned int *delayhi, unsigned int *delaylo ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( delayhi ) {
            *delayhi = 0;
        }
        if ( delaylo ) {
            *delaylo = 0;
        }
        return result;
    }

    return channeli->getDelay( delaytype, delayhi, delaylo );
} // 0x80379038

/*
====================
Channel::setSpeakerMix
====================
*/
FMOD_RESULT Channel::setSpeakerMix( float frontleft, float frontright, float center, float lfe, float backleft,
                                    float backright, float sideleft, float sideright ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setSpeakerMix( frontleft, frontright, center, lfe, backleft, backright, sideleft, sideright,
                                    true );
} // 0x803790C0

/*
====================
Channel::setSpeakerLevels
====================
*/
FMOD_RESULT Channel::setSpeakerLevels( FMOD_SPEAKER speaker, float *levels, int numlevels ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setSpeakerLevels( speaker, levels, numlevels, true );
} // 0x8037914C

/*
====================
Channel::setInputChannelMix
====================
*/
FMOD_RESULT Channel::setInputChannelMix( float *levels, int numlevels ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setInputChannelMix( levels, numlevels );
} // 0x803791B8

/*
====================
Channel::setMute
====================
*/
FMOD_RESULT Channel::setMute( bool mute ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setMute( mute );
} // 0x80379210

/*
====================
Channel::setPriority
====================
*/
FMOD_RESULT Channel::setPriority( int priority ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setPriority( priority );
} // 0x80379258

/*
====================
Channel::setPosition
====================
*/
FMOD_RESULT Channel::setPosition( unsigned int position, FMOD_TIMEUNIT postype ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setPosition( position, postype );
} // 0x803792A0

/*
====================
Channel::getPosition
====================
*/
FMOD_RESULT Channel::getPosition( unsigned int *position, FMOD_TIMEUNIT postype ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( position ) {
            *position = 0;
        }
        return result;
    }

    return channeli->getPosition( position, postype );
} // 0x803792F8

/*
====================
Channel::setReverbProperties
====================
*/
FMOD_RESULT Channel::setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setReverbProperties( prop );
} // 0x80379360

/*
====================
Channel::getReverbProperties
====================
*/
FMOD_RESULT Channel::getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->getReverbProperties( prop );
} // 0x803793A8

/*
====================
Channel::setChannelGroup
====================
*/
FMOD_RESULT Channel::setChannelGroup( ChannelGroup *channelgroup ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setChannelGroup( reinterpret_cast<ChannelGroupI *>( channelgroup ) );
} // 0x803793F0

/*
====================
Channel::setCallback
====================
*/
FMOD_RESULT Channel::setCallback( FMOD_RESULT ( F_CALLBACK *callback )( FMOD_CHANNEL *channel,
                                                                       FMOD_CHANNEL_CALLBACKTYPE type,
                                                                       void *data1, void *data2 ) ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setCallback( callback );
} // 0x80379438

/*
====================
Channel::set3DAttributes
====================
*/
FMOD_RESULT Channel::set3DAttributes( const FMOD_VECTOR *pos, const FMOD_VECTOR *vel ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DAttributes( pos, vel );
} // 0x80379480

/*
====================
Channel::get3DAttributes
====================
*/
FMOD_RESULT Channel::get3DAttributes( FMOD_VECTOR *pos, FMOD_VECTOR *vel ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( pos ) {
            pos->z = 0.0f;
            pos->y = 0.0f;
            pos->x = 0.0f;
        }
        if ( vel ) {
            vel->z = 0.0f;
            vel->y = 0.0f;
            vel->x = 0.0f;
        }
        return result;
    }

    return channeli->get3DAttributes( pos, vel );
} // 0x803794D8

/*
====================
Channel::set3DMinMaxDistance
====================
*/
FMOD_RESULT Channel::set3DMinMaxDistance( float mindistance, float maxdistance ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DMinMaxDistance( mindistance, maxdistance );
} // 0x80379560

/*
====================
Channel::set3DConeOrientation
====================
*/
FMOD_RESULT Channel::set3DConeOrientation( FMOD_VECTOR *orientation ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DConeOrientation( orientation );
} // 0x803795B8

/*
====================
Channel::set3DOcclusion
====================
*/
FMOD_RESULT Channel::set3DOcclusion( float directocclusion, float reverbocclusion ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DOcclusion( directocclusion, reverbocclusion );
} // 0x80379600

/*
====================
Channel::set3DSpread
====================
*/
FMOD_RESULT Channel::set3DSpread( float angle ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DSpread( angle );
} // 0x80379658

/*
====================
Channel::set3DPanLevel
====================
*/
FMOD_RESULT Channel::set3DPanLevel( float level ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DPanLevel( level );
} // 0x803796A0

/*
====================
Channel::set3DDopplerLevel
====================
*/
FMOD_RESULT Channel::set3DDopplerLevel( float level ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->set3DDopplerLevel( level );
} // 0x803796E8

/*
====================
Channel::isPlaying
====================
*/
FMOD_RESULT Channel::isPlaying( bool *isplaying ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( isplaying ) {
            *isplaying = false;
        }
        return result;
    }

    return channeli->isPlaying( isplaying );
} // 0x80379730

/*
====================
Channel::getAudibility
====================
*/
FMOD_RESULT Channel::getAudibility( float *audibility ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( audibility ) {
            *audibility = 0.0f;
        }
        return result;
    }

    return channeli->getAudibility( audibility );
} // 0x80379788

/*
====================
Channel::getCurrentSound
====================
*/
FMOD_RESULT Channel::getCurrentSound( Sound **sound ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( sound ) {
            *sound = NULL;
        }
        return result;
    }

    return channeli->getCurrentSound( reinterpret_cast<SoundI **>( sound ) );
} // 0x803797E0

/*
====================
Channel::setMode
====================
*/
FMOD_RESULT Channel::setMode( FMOD_MODE mode ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setMode( mode );
} // 0x80379838

/*
====================
Channel::getMode
====================
*/
FMOD_RESULT Channel::getMode( FMOD_MODE *mode ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( mode ) {
            *mode = 0;
        }
        return result;
    }

    return channeli->getMode( mode );
} // 0x80379880

/*
====================
Channel::setUserData
====================
*/
FMOD_RESULT Channel::setUserData( void *userdata ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        return result;
    }

    return channeli->setUserData( userdata );
} // 0x803798D8

/*
====================
Channel::getUserData
====================
*/
FMOD_RESULT Channel::getUserData( void **userdata ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( userdata ) {
            *userdata = NULL;
        }
        return result;
    }

    return channeli->getUserData( userdata );
} // 0x80379920

/*
====================
Channel::getMemoryInfo
====================
*/
FMOD_RESULT Channel::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                    unsigned int *memoryused_array ) {
    FMOD_RESULT    result;
    ChannelI      *channeli;

    result = ChannelI::validate( this, &channeli );
    if ( result != FMOD_OK ) {
        if ( memoryused ) {
            *memoryused = 0;
        }
        if ( memoryused_array ) {
            *memoryused_array = 0;
        }
        return result;
    }

    return channeli->getMemoryInfo( memorybits, event_memorybits, memoryused, memoryused_array );
} // 0x80379978

} // namespace FMOD
