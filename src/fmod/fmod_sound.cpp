/*
 * fmod_sound.cpp: FMOD::Sound, the public handle of a sound. Each method checks
 * its handle and forwards to the internal SoundI.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_sound.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_sound.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_soundi.h>

namespace FMOD {

/*
====================
Sound::release
====================
*/
FMOD_RESULT Sound::release() {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return soundi->release( true );
} // 0x8037A428

/*
====================
Sound::setDefaults
====================
*/
FMOD_RESULT Sound::setDefaults( float frequency, float volume, float pan, int priority ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->setDefaults( frequency, volume, pan, priority );
} // 0x8037A470

/*
====================
Sound::getDefaults
====================
*/
FMOD_RESULT Sound::getDefaults( float *frequency, float *volume, float *pan, int *priority ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getDefaults( frequency, volume, pan, priority );
} // 0x8037A510

/*
====================
Sound::setSubSound
====================
*/
FMOD_RESULT Sound::setSubSound( int index, Sound *subsound ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->setSubSound( index, (SoundI *)subsound );
} // 0x8037A5B0

/*
====================
Sound::getSubSound
====================
*/
FMOD_RESULT Sound::getSubSound( int index, Sound **subsound ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getSubSound( index, (SoundI **)subsound );
} // 0x8037A630

/*
====================
Sound::getLength
====================
*/
FMOD_RESULT Sound::getLength( unsigned int *length, FMOD_TIMEUNIT lengthtype ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getLength( length, lengthtype );
} // 0x8037A6B0

/*
====================
Sound::getFormat
====================
*/
FMOD_RESULT Sound::getFormat( FMOD_SOUND_TYPE *type, FMOD_SOUND_FORMAT *format, int *channels, int *bits ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getFormat( type, format, channels, bits );
} // 0x8037A730

/*
====================
Sound::getNumSubSounds
====================
*/
FMOD_RESULT Sound::getNumSubSounds( int *numsubsounds ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getNumSubSounds( numsubsounds );
} // 0x8037A7D0

/*
====================
Sound::getOpenState
====================
*/
FMOD_RESULT Sound::getOpenState( FMOD_OPENSTATE *openstate, unsigned int *percentbuffered, bool *starving ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return soundi->getOpenState( openstate, percentbuffered, starving );
} // 0x8037A840

/*
====================
Sound::getNumSyncPoints
====================
*/
FMOD_RESULT Sound::getNumSyncPoints( int *numsyncpoints ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getNumSyncPoints( numsyncpoints );
} // 0x8037A8B4

/*
====================
Sound::getSyncPoint
====================
*/
FMOD_RESULT Sound::getSyncPoint( int index, FMOD_SYNCPOINT **point ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getSyncPoint( index, point );
} // 0x8037A924

/*
====================
Sound::getSyncPointInfo
====================
*/
FMOD_RESULT Sound::getSyncPointInfo( FMOD_SYNCPOINT *point, char *name, int namelen, unsigned int *offset,
                                     FMOD_TIMEUNIT offsettype ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getSyncPointInfo( point, name, namelen, offset, offsettype );
} // 0x8037A9A4

/*
====================
Sound::setMode
====================
*/
FMOD_RESULT Sound::setMode( FMOD_MODE mode ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->setMode( mode );
} // 0x8037AA34

/*
====================
Sound::getMode
====================
*/
FMOD_RESULT Sound::getMode( FMOD_MODE *mode ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getMode( mode );
} // 0x8037AAA4

/*
====================
Sound::setLoopCount
====================
*/
FMOD_RESULT Sound::setLoopCount( int loopcount ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->setLoopCount( loopcount );
} // 0x8037AB14

/*
====================
Sound::getMusicNumChannels
====================
*/
FMOD_RESULT Sound::getMusicNumChannels( int *numchannels ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getMusicNumChannels( numchannels );
} // 0x8037AB84

/*
====================
Sound::setMusicChannelVolume
====================
*/
FMOD_RESULT Sound::setMusicChannelVolume( int channel, float volume ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->setMusicChannelVolume( channel, volume );
} // 0x8037ABF4

/*
====================
Sound::getMemoryInfo
====================
*/
FMOD_RESULT Sound::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                  unsigned int *memoryused_array ) {
    FMOD_RESULT result;
    SoundI     *soundi;

    result = SoundI::validate( this, &soundi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( soundi->mOpenState != FMOD_OPENSTATE_READY && soundi->mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    return soundi->getMemoryInfo( memorybits, event_memorybits, memoryused, memoryused_array );
} // 0x8037AC74

} // namespace FMOD
