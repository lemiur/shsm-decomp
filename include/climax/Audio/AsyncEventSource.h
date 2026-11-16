#ifndef CLIMAX_AUDIO_ASYNCEVENTSOURCE_H
#define CLIMAX_AUDIO_ASYNCEVENTSOURCE_H

// Names and layout: Gormiti link map and DWARF (Audio::AsyncEventSource, game/System/Audio/FMOD/AsyncEventSource.h).
// Retail vtable lbl_8042A068 (42 slots: EventSource's 40, every one overridden but the clone slots, and
// two of its own).

#include <climax/Audio/EventGroup.h>
#include <climax/Audio/EventSource.h>

namespace Audio {

/*
 * An event group whose project is still loading: the sources asked of it are AsyncEventSources. Retail
 * vtable lbl_8042A118 (EventGroup's 12 slots, four overridden).
 */
class AsyncEventGroup : public EventGroup {
public:
                        AsyncEventGroup( const char* name );

    virtual             ~AsyncEventGroup();
    virtual int         Init( FMOD::EventSystem* eventSystem, const char* nameOrData, const char* friendlyName, int lengthOfData, bool loadedFromDisk, bool autoDelete );
    virtual EventSource*    LoadEventSource( const char* name, bool recursiveSearch, ESLoadParams* params );
    virtual bool        IsAsync() { return true; }
};

/*
 * An event source whose group is still loading: it remembers what the game asked of it (play, pause,
 * position, volume) and replays that when the group arrives.
 */
class AsyncEventSource : public EventSource {
public:
                        AsyncEventSource();
    virtual             ~AsyncEventSource();

    void                SetName( const char* name );

    // Overrides of EventSource's slots (the ones that act on the event once it exists).
    virtual int         Play( Channel::Type channelType, bool startPaused );
    virtual int         Stop( bool fadeOut );
    virtual int         Pause( bool pause );
    virtual bool        IsPlaying() const;
    virtual bool        IsPaused() const;
    virtual void        SetPosition( float* pos );
    virtual void        SetVelocity( float* vel );
    virtual void        GetPosition( float* pos );
    virtual void        GetVelocity( float* vel );
    virtual ISource*    Clone( bool cloneIfThisIsPlaying );
    virtual bool        IsPlayingOrInPlayingState() const;
    virtual bool        Is3D() const;
    virtual void        SetPosition( float pos );
    virtual float       GetPosition() const;
    virtual void        SetVolume( float volume );
    virtual float       GetVolume();
    virtual char*       GetName();
    virtual void        GetName( char** name );
    virtual int         IsReady();

    virtual int         Play( Channel::Type channelType, bool startPaused, bool checkIfStopped );    // slot 0xA8
    virtual void        SetEventErrorCode( Error error ) { m_errCode = error; }  // slot 0xAC
    // Override of slot 0x38, declared last: MWCC emits a class's weak inlines in the reverse of its
    // declaration list, and retail emits IsAsync before SetEventErrorCode.
    virtual bool        IsAsync() { return true; }

    char                m_asyncName[128];       // 0x54
    bool                m_paused;               // 0xD4
    bool                m_fadeOut;              // 0xD5
    bool                m_stopped;              // 0xD6
    Error               m_errCode;              // 0xD8
    float*              m_position;             // 0xDC
    float               m_volume;               // 0xE0
};

}

#endif
