// Unit name and boundary: Gormiti link map (Source.obj)
// Names and layout: Gormiti link map and DWARF (Source.obj, game/System/Audio/FMOD/Source.cpp)

#include <string.h>
#include <climax/Audio/Source.h>
#include <climax/Audio/System.h>

// Data owned by this unit: the vtable of Audio::Source, __vt__Q25Audio6Source (.data 0x8042A298, size 0x48),
// emitted by the compiler from the class.

namespace Audio {

/*
====================
Source::Source

A source holds no FMOD sound and no channel until it is loaded and played, at the default priority.
====================
*/
// 0x801FDCA4
Source::Source() :
    m_fmodSound( NULL ),
    m_fmodChannel( NULL ),
    m_priority( 128 ) {
}

/*
====================
Source::~Source

Tells the system's callback sink the sound is going, then releases the FMOD sound and stops the channel.
====================
*/
// 0x801FDCC8
Source::~Source() {
    System::GetInstance()->TriggerCallback( CBT_SOUND_DELETING, this );
    if ( m_fmodSound != NULL ) {
        m_fmodSound->release();
    }
    if ( m_fmodChannel != NULL ) {
        m_fmodChannel->stop();
    }
    m_fmodSound = NULL;
    m_fmodChannel = NULL;
}

/*
====================
Source::LoadStream

Opens filename as an FMOD stream. The sink is told of the new sound when FMOD succeeds. The second
parameter and the SourceParams are not used.
====================
*/
// 0x801FDD78
int Source::LoadStream( const char* filename, int, SourceParams* ) {
    FMOD_RESULT result = System::GetInstance()->m_fmodSystem->createStream( filename, FMOD_HARDWARE | FMOD_CREATESTREAM, NULL, &m_fmodSound );
    if ( result == FMOD_OK ) {
        System::GetInstance()->TriggerCallback( CBT_SOUND_CREATING, this );
    }
    return result;
}

/*
====================
Source::Play

Starts the sound on a channel of channelType, gives the channel the source's priority and hands it
to the system.
====================
*/
// 0x801FDDF8
int Source::Play( Channel::Type channelType, bool startPaused ) {
    FMOD_RESULT result = System::GetInstance()->m_fmodSystem->playSound( static_cast< FMOD_CHANNELINDEX >( channelType ), m_fmodSound, startPaused, &m_fmodChannel );
    m_fmodChannel->setPriority( m_priority );
    System::GetInstance()->ChannelStart( m_fmodChannel );
    return result;
}

/*
====================
Source::Pause

Pauses or resumes the channel.
====================
*/
// 0x801FDE60
int Source::Pause( bool pause ) {
    return m_fmodChannel->setPaused( pause );
}

/*
====================
Source::Stop

Stops the channel the source is playing on, if any.
====================
*/
// 0x801FDE68
int Source::Stop( bool ) {
    if ( m_fmodChannel == NULL ) {
        return 0;
    }
    m_fmodChannel->stop();
    m_fmodChannel = NULL;
    return 0;
}

/*
====================
Source::SetPosition

Gives the channel a position and no velocity (retail's SetPosition; Gormiti's does more).
====================
*/
// 0x801FDEB8
void Source::SetPosition( float* pos ) {
    m_fmodChannel->set3DAttributes( reinterpret_cast< const FMOD_VECTOR* >( pos ), NULL );
}

/*
====================
Source::SetVelocity

Gives the channel a velocity and no position.
====================
*/
// 0x801FDEC4
void Source::SetVelocity( float* vel ) {
    m_fmodChannel->set3DAttributes( NULL, reinterpret_cast< const FMOD_VECTOR* >( vel ) );
}

/*
====================
Source::GetPosition

Reads the channel's position.
====================
*/
// 0x801FDED4
void Source::GetPosition( float* pos ) {
    m_fmodChannel->get3DAttributes( reinterpret_cast< FMOD_VECTOR* >( pos ), NULL );
}

/*
====================
Source::GetStats

Gathers the channel's memory statistics and reports one active channel and no CPU usage.
====================
*/
// 0x801FDEE0
int Source::GetStats( StatsType type, Stats* stats ) {
    int errCode = GetMemoryStats( m_fmodChannel, type, stats );
    stats->numActiveChannels = 1;
    memset( &stats->cpuUsage, 0, sizeof( CPUUsage ) );
    return errCode;
}

/*
====================
Source::GetVelocity

Reads the channel's velocity.
====================
*/
// 0x801FDF78
void Source::GetVelocity( float* vel ) {
    m_fmodChannel->get3DAttributes( NULL, reinterpret_cast< FMOD_VECTOR* >( vel ) );
}

/*
====================
Source::IsPlaying

False when no channel was started (retail's IsPlaying; Gormiti's does more).
====================
*/
// 0x801FDF88
bool Source::IsPlaying() const {
    if ( m_fmodChannel == NULL ) {
        return false;
    }
    bool isPlaying = false;
    m_fmodChannel->isPlaying( &isPlaying );
    return isPlaying;
}

/*
====================
Source::IsPaused

Asks the channel whether it is paused.
====================
*/
// 0x801FDFCC
bool Source::IsPaused() const {
    bool paused;
    m_fmodChannel->getPaused( &paused );
    return paused;
}

/*
====================
Source::SetVolume

True when FMOD took the volume (retail's SetVolume returns the result; Gormiti's returns nothing).
====================
*/
// 0x801FDFF8
bool Source::SetVolume( float volume ) {
    return m_fmodChannel->setVolume( volume ) == FMOD_OK;
}

/*
====================
Source::Clone

A source cannot be cloned.
====================
*/
// 0x801FE024
ISource* Source::Clone( bool ) {
    return NULL;
}

/*
====================
Source::GetSoundType

A plain source, not an event.
====================
*/
// 0x801FE02C
SoundType Source::GetSoundType() {
    return SoundTypeDefault;
}

}
