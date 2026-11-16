#ifndef CLIMAX_AUDIO_EVENTSOURCE_H
#define CLIMAX_AUDIO_EVENTSOURCE_H

// Names and layout: Gormiti link map and DWARF (Audio::EventSource, game/System/Audio/FMOD/EventSource.h).
// Retail vtable lbl_8042A1C0 (40 slots: ISource's, then 24 of its own); the class's own unit
// (EventSource.obj) is not reconstructed yet, so only the declaration is here. Slots of unknown purpose
// keep the placeholder name of the retail function the slot holds. Retail's class ends at 0x54 (Gormiti's m_isWiiMoteSound
// is not in this revision: AsyncEventSource's name array starts there).

#include <revolution/types.h>
#include <climax/STLList.h>
#include <climax/Audio/Defs.h>
#include <climax/Audio/Source.h>
#include <fmod/fmod_event.h>

namespace Audio {

class EventGroup;
class EventSource;
class EventParameter;

/* The parameters of an event: a counted array of pointers (Gormiti DWARF). */
class Array {
public:
    short               size;                   // 0x00
    EventParameter**    array;                  // 0x04
};

/* What an event source reports to the system's ICallbackSink when its event finishes (Gormiti DWARF). */
class EventSourceCallbackInfo {
public:
    EventSource*            eventSource;        // 0x00
    FMOD_EVENT_CALLBACKTYPE fmodCBType;         // 0x04
    void*                   param1;             // 0x08
    void*                   param2;             // 0x0C
};

/*
 * One playable event of an event group: the FMOD event, its clones (one per overlapping playback) and the
 * callback that hears about it.
 */
class EventSource : public ISource {
public:
                        EventSource();
    virtual             ~EventSource();

    // The overrides of ISource's slots (0x0C-0x3C), repeated so that a derived class can call them by name.
    virtual int         Play( Channel::Type channelType, bool startPaused );
    virtual int         Stop( bool fadeOut );
    virtual int         Pause( bool pause );
    virtual bool        IsPlaying() const;
    virtual bool        IsPaused() const;
    virtual void        SetPosition( float* pos );
    virtual void        SetVelocity( float* vel );
    virtual void        GetPosition( float* pos );
    virtual void        GetVelocity( float* vel );
    virtual int         GetStats( StatsType type, Stats* stats );
    virtual SoundType   GetSoundType() { return SoundTypeEvent; }
    virtual ISource*    Clone( bool cloneIfThisIsPlaying );

    // The slots from 0x40 on are EventSource's own (Source's own slot 0x40 is SetVolume, and its vtable ends
    // with a null word at 0x44). Slots of unknown purpose keep the placeholder name of the retail function.
    virtual int         Update();               // slot 0x40 (Gormiti: EventSource::Update)
    virtual bool        IsPlayingOrInPlayingState() const;  // slot 0x44
    virtual void        fn_801FC8FC();          // slot 0x48
    virtual void        fn_801FCDB4();          // slot 0x4C
    virtual void        fn_801FB9A4();          // slot 0x50
    virtual bool        Is3D() const;           // slot 0x54
    virtual void        fn_801FCD08();          // slot 0x58
    virtual void        fn_801FCC7C();          // slot 0x5C
    virtual void        fn_801FCB4C();          // slot 0x60
    virtual int         SetPlayTime( float playTime );  // slot 0x64
    virtual EventSource*    UpdateFMODHandle(); // slot 0x68
    virtual int         SetSpeakerSubChannel( int subChannel, Channel::Type channelType, float volume ); // slot 0x6C
    virtual void        SetPosition( float pos );   // slot 0x70
    virtual float       GetPosition() const;    // slot 0x74
    virtual void        SetVolume( float volume );  // slot 0x78
    virtual float       GetVolume();            // slot 0x7C
    virtual void        fn_801FC98C();          // slot 0x80
    virtual char*       GetName();              // slot 0x84
    virtual void        GetName( char** name ); // slot 0x88 (writes a name out)
    virtual void        RemoveClone( EventSource* clone );  // slot 0x8C
    virtual void        fn_801FCF40();          // slot 0x90
    virtual void        fn_801FCDE4();          // slot 0x94
    virtual void        fn_801FD090();          // slot 0x98
    virtual int         IsReady();              // slot 0x9C
    virtual bool        IsLooping();            // slot 0xA0
    virtual void        fn_801FBF00();          // slot 0xA4

    void                SetParent( EventGroup* parent ) { m_parent = parent; }   // Gormiti: out of line in AsyncEventSource.obj; retail inlines it

    static FMOD_RESULT F_CALLBACK   FMODEventCallback( FMOD_EVENT* event, FMOD_EVENT_CALLBACKTYPE type, void* param1, void* param2, void* userdata );

    Array               m_eventParams;          // 0x04
    FMOD::Event*        m_fmodEvent;            // 0x0C
    EventGroup*         m_parent;               // 0x10
    ListUtils::STLList< EventSource* >  m_clones;   // 0x14
    EventSource*        m_cloneParent;          // 0x24
    bool                m_locked;               // 0x28
    bool                m_isInfoOnly;           // 0x29
    unsigned int        m_nameHash;             // 0x2C
    ICallbackSink*      m_callback;             // 0x30
    unsigned int        m_playbackCount;        // 0x34
    unsigned int        m_autoDeletePlaybackCount;  // 0x38
    Channel::Type       m_channelType;          // 0x3C
    void*               m_owner;                // 0x40
    ListUtils::STLList< int >   m_subChannelSetup;  // 0x44 (element type unconfirmed)
};

}

#endif
