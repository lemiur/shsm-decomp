// Unit name and boundary: Gormiti link map (AsyncEventSource.obj)
// Names and layout: Gormiti link map and DWARF (Audio::AsyncEventSource, AsyncEventSource.obj,
// game/System/Audio/FMOD/AsyncEventSource.cpp)
//
// Retail's AsyncEventSource.obj holds 25 of the 40 functions of Gormiti's: the constructor, SetName, SetParent,
// AddEventSource and RemovePendingEventGroup (with the list members it uses) are inlined. The functions that
// differ from Gormiti's (the group's LoadEventSource, the destructor, Play, Stop, Pause, IsPlaying, IsReady,
// IsPlayingOrInPlayingState) are named by the same-named function of Gormiti in the same position, or by the
// retail vtable slot they fill. The unit ends with the weak inlines its two vtables pull in (GetSoundType, IsAsync,
// SetEventErrorCode, RefCountedObject::ReleaseRef/AddRef, EventGroup::SetEventErrorCode, AsyncEventGroup::IsAsync),
// defined in the class headers.

#include <string.h>
#include <climax/Audio/AsyncEventSource.h>
#include <climax/Audio/System.h>
#include <climax/Debug.h>

// Data owned by this unit: the vtables of Audio::AsyncEventGroup (__vt__Q25Audio15AsyncEventGroup, .data
// 0x8042A118) and Audio::AsyncEventSource (__vt__Q25Audio16AsyncEventSource, .data 0x8042A068), emitted by
// the compiler from the classes, and the two float constants of the unit (.sdata2 1.0f and 0.0f).

namespace Audio {

/*
====================
AsyncEventSource::AsyncEventSource

Nothing played, paused or stopped yet, no position remembered, full volume, on the previous channel.
====================
*/
// no copy in retail (inlined); Gormiti 0x802158BC
inline AsyncEventSource::AsyncEventSource() :
    m_paused( false ),
    m_fadeOut( false ),
    m_stopped( false ),
    m_errCode( Ok ),
    m_position( NULL ),
    m_volume( 1.0f ) {
    m_channelType = Channel::Previous;
}

/*
====================
AsyncEventSource::SetName
====================
*/
// no copy in retail (inlined); Gormiti 0x802157DC
inline void AsyncEventSource::SetName( const char* name ) {
    strcpy( m_asyncName, name );
}

/*
====================
AsyncEventGroup::AsyncEventGroup

A group created before its project has loaded: the name is all it knows.
====================
*/
// 0x801F8284
AsyncEventGroup::AsyncEventGroup( const char* name ) {
    strcpy( m_name, name );
    m_isLoadedFromDisk = true;
}

/*
====================
AsyncEventGroup::~AsyncEventGroup
====================
*/
// 0x801F82E0
AsyncEventGroup::~AsyncEventGroup() {
    System::GetInstance()->RemovePendingEventGroup( this );
}

/*
====================
AsyncEventGroup::LoadEventSource

Hands out a source that waits for the group's project: the event is looked up by name when the source is
played.
====================
*/
// 0x801F83DC
EventSource* AsyncEventGroup::LoadEventSource( const char* name, bool recursiveSearch, ESLoadParams* params ) {
    AsyncEventSource* esource = new( GAME_HEAP, GAME_HEAP_FLAGS ) AsyncEventSource;
    esource->SetName( name );
    esource->SetParent( this );
    AddEventSource( esource );
    return esource;
}

/*
====================
AsyncEventGroup::Init
====================
*/
// 0x801F84E8
int AsyncEventGroup::Init( FMOD::EventSystem* eventSystem, const char* nameOrData, const char* friendlyName, int lengthOfData, bool loadedFromDisk, bool autoDelete ) {
    int ret = EventGroup::Init( eventSystem, nameOrData, friendlyName, lengthOfData, loadedFromDisk, autoDelete );
    m_isLoadedFromDisk = true;
    return ret;
}

/*
====================
AsyncEventSource::~AsyncEventSource
====================
*/
// 0x801F851C
AsyncEventSource::~AsyncEventSource() {
    System::GetInstance()->RemoveAsyncPlayback( this );
    if ( m_position != NULL ) {
        delete[] m_position;
        m_position = NULL;
    }
}

/*
====================
AsyncEventSource::GetName
====================
*/
// 0x801F85A8
char* AsyncEventSource::GetName() {
    return m_asyncName;
}

/*
====================
AsyncEventSource::GetName
====================
*/
// 0x801F85B0
void AsyncEventSource::GetName( char** name ) {
    *name = m_asyncName;
}

/*
====================
AsyncEventSource::Play

Plays the event if the group has delivered it; otherwise asks the parent group for the event now.
====================
*/
// 0x801F85BC
int AsyncEventSource::Play( Channel::Type channelType, bool startPaused, bool checkIfStopped ) {
    if ( m_fmodEvent != NULL ) {
        unsigned int state;
        m_fmodEvent->getState( &state );
        if ( state & FMOD_EVENT_STATE_LOADING ) {
            return EventLoading;
        }

        // Gormiti keeps the name in a local that neither of its builds reads after the call.
        char* name = NULL;
        GetName( &name );

        if ( m_callback != NULL ) {
            m_fmodEvent->setCallback( FMODEventCallback, this );
        }
        if ( checkIfStopped && m_stopped ) {
            return Ok;
        }
        if ( m_position != NULL ) {
            EventSource::SetPosition( m_position );
        }
        int errCode = EventSource::Play( m_channelType, m_paused );
        m_fmodEvent->setVolume( m_volume );
        return errCode;
    }

    m_playbackCount++;
    ESLoadParams params;
    params.resource = FMOD_EVENT_RESOURCE_STREAMS;
    params.mode = FMOD_EVENT_NONBLOCKING;
    m_fmodEvent = m_parent->LoadFMODEvent( m_asyncName, true, &params );
    m_channelType = channelType;
    m_paused = startPaused;
    if ( m_fmodEvent != NULL ) {
        return Play( channelType, startPaused );
    }
    return EventLoading;
}

/*
====================
AsyncEventSource::Play
====================
*/
// 0x801F8730
int AsyncEventSource::Play( Channel::Type channelType, bool startPaused ) {
    return Play( channelType, startPaused, false );
}

/*
====================
AsyncEventSource::IsReady

A stopped source reports so; otherwise the event decides.
====================
*/
// 0x801F8744
int AsyncEventSource::IsReady() {
    if ( m_stopped ) {
        return EventStopped;
    }
    return EventSource::IsReady();
}

/*
====================
AsyncEventSource::Stop

A source stopped before its event exists is dropped from the async queue; the callback hears of the stop
either way.
====================
*/
// 0x801F8764
int AsyncEventSource::Stop( bool fadeOut ) {
    int result = 0;
    if ( m_playbackCount != 0 ) {
        bool cancel = false;
        if ( m_fmodEvent == NULL ) {
            cancel = true;
        }
        m_stopped = true;
        if ( cancel ) {
            System::GetInstance()->CancelAsyncItemForEventSource( this );
        }
    }
    if ( m_fmodEvent != NULL ) {
        result = EventSource::Stop( fadeOut );
    }
    if ( m_callback != NULL && m_stopped ) {
        EventSourceCallbackInfo cbInfo;
        cbInfo.fmodCBType = FMOD_EVENT_CALLBACKTYPE_EVENTFINISHED;
        cbInfo.eventSource = this;
        m_callback->Callback( CBT_EVENT_SOURCE, &cbInfo );
    }
    m_fadeOut = fadeOut;
    return result;
}

/*
====================
AsyncEventSource::Pause
====================
*/
// 0x801F8840
int AsyncEventSource::Pause( bool pause ) {
    if ( m_fmodEvent != NULL ) {
        return EventSource::Pause( pause );
    }
    m_paused = pause;
    return 0;
}

/*
====================
AsyncEventSource::IsPlaying
====================
*/
// 0x801F885C
bool AsyncEventSource::IsPlaying() const {
    if ( m_fmodEvent != NULL ) {
        unsigned int state;
        m_fmodEvent->getState( &state );
        if ( !( state & FMOD_EVENT_STATE_LOADING ) ) {
            return EventSource::IsPlaying();
        }
    }
    if ( m_errCode != Ok ) {
        return false;
    }
    return m_channelType != Channel::Previous;
}

/*
====================
AsyncEventSource::IsPlayingOrInPlayingState
====================
*/
// 0x801F88E0
bool AsyncEventSource::IsPlayingOrInPlayingState() const {
    if ( m_stopped ) {
        return false;
    }
    if ( m_fmodEvent != NULL ) {
        unsigned int state;
        m_fmodEvent->getState( &state );
        if ( !( state & FMOD_EVENT_STATE_LOADING ) ) {
            return EventSource::IsPlayingOrInPlayingState();
        }
    }
    if ( m_errCode != Ok ) {
        return false;
    }
    return m_channelType != Channel::Previous;
}

/*
====================
AsyncEventSource::IsPaused
====================
*/
// 0x801F8974
bool AsyncEventSource::IsPaused() const {
    if ( m_fmodEvent != NULL ) {
        return EventSource::IsPaused();
    }
    return m_paused;
}

/*
====================
AsyncEventSource::SetVolume
====================
*/
// 0x801F898C
void AsyncEventSource::SetVolume( float volume ) {
    if ( m_fmodEvent != NULL ) {
        EventSource::SetVolume( volume );
    } else {
        m_volume = volume;
    }
}

/*
====================
AsyncEventSource::GetVolume
====================
*/
// 0x801F89A4
float AsyncEventSource::GetVolume() {
    if ( m_fmodEvent != NULL ) {
        return EventSource::GetVolume();
    }
    return m_volume;
}

/*
====================
AsyncEventSource::SetPosition

Before the event exists the position is remembered, in storage allocated on first use, and applied by Play.
====================
*/
// 0x801F89BC
void AsyncEventSource::SetPosition( float* pos ) {
    if ( m_fmodEvent != NULL ) {
        EventSource::SetPosition( pos );
    } else {
        if ( m_position == NULL ) {
            m_position = new( GAME_HEAP, GAME_HEAP_FLAGS ) float[3];
        }
        memcpy( m_position, pos, sizeof( float ) * 3 );
    }
}

/*
====================
AsyncEventSource::SetPosition
====================
*/
// 0x801F8A34
void AsyncEventSource::SetPosition( float pos ) {
    if ( m_fmodEvent != NULL ) {
        EventSource::SetPosition( pos );
    }
}

/*
====================
AsyncEventSource::SetVelocity
====================
*/
// 0x801F8A48
void AsyncEventSource::SetVelocity( float* vel ) {
    if ( m_fmodEvent != NULL ) {
        EventSource::SetVelocity( vel );
    }
}

/*
====================
AsyncEventSource::GetPosition
====================
*/
// 0x801F8A5C
float AsyncEventSource::GetPosition() const {
    if ( m_fmodEvent != NULL ) {
        return EventSource::GetPosition();
    }
    return 0.0f;
}

/*
====================
AsyncEventSource::GetPosition
====================
*/
// 0x801F8A74
void AsyncEventSource::GetPosition( float* pos ) {
    if ( m_fmodEvent != NULL ) {
        EventSource::GetPosition( pos );
    } else if ( m_position != NULL ) {
        memcpy( pos, m_position, sizeof( float ) * 3 );
    }
}

/*
====================
AsyncEventSource::GetVelocity
====================
*/
// 0x801F8AA4
void AsyncEventSource::GetVelocity( float* vel ) {
    if ( m_fmodEvent != NULL ) {
        EventSource::GetVelocity( vel );
    }
}

/*
====================
AsyncEventSource::Clone

Quirk kept from the original: a source whose event has not been delivered returns without a value (the
caller sees whatever the call left in the return register).
====================
*/
// 0x801F8AB8
ISource* AsyncEventSource::Clone( bool cloneIfThisIsPlaying ) {
    if ( m_fmodEvent != NULL ) {
        return EventSource::Clone( cloneIfThisIsPlaying );
    }
}

/*
====================
AsyncEventSource::Is3D
====================
*/
// 0x801F8ACC
bool AsyncEventSource::Is3D() const {
    if ( m_fmodEvent != NULL ) {
        return EventSource::Is3D();
    }
    return true;
}

}
