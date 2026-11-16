#ifndef CLIMAX_AUDIO_EVENTGROUP_H
#define CLIMAX_AUDIO_EVENTGROUP_H

// Names and layout: Gormiti link map and DWARF (Audio::EventGroup, game/System/Audio/FMOD/EventSource.h).
// Retail vtable lbl_8042A188; the class's own unit (EventGroup.obj) is not reconstructed yet, so only the
// declaration is here. Slot names come from the pairing and the Gormiti vtable.

#include <revolution/types.h>
#include <climax/STLList.h>
#include <climax/Audio/Defs.h>
#include <climax/Audio/Helper.h>
#include <fmod/fmod_event.h>

namespace Audio {

class EventSource;

/* How an event source is loaded from its group (Gormiti DWARF). */
class ESLoadParams {
public:
    FMOD_EVENT_RESOURCE resource;               // 0x00
    unsigned int        mode;                   // 0x04
};

/*
 * The event sources of one FMOD event group, loaded together from a sound project. The project is
 * reference counted; the group goes when its last reference does.
 */
class EventGroup : public RefCountedObject {
public:
                        EventGroup();
    virtual             ~EventGroup();
    virtual int         Init( FMOD::EventSystem* eventSystem, const char* nameOrData, const char* friendlyName, int lengthOfData, bool loadedFromDisk, bool autoDelete );
    virtual void        Free();
    virtual int         CacheAllEventSources( ESLoadParams* params );
    virtual EventSource*    LoadEventSource( const char* name, bool recursiveSearch, ESLoadParams* params );
    virtual EventSource*    IsEventSourceLoaded( const char* name );
    virtual void        SetEventErrorCode( Error error ) {}
    virtual int         DebugGetAllEventNames( int& count, char** names );
    virtual int         DebugRecurseGetEventNames( FMOD::EventGroup* parent, int& index, int& count, char** names );
    virtual bool        IsAsync();

    void                SetParentProject( SProject* parentProject );
    void                AddEventSource( EventSource* esource );
    FMOD::Event*        LoadFMODEvent( const char* name, bool recursiveSearch, ESLoadParams* params );

    // Inline accessors (Gormiti: EventSource.h; the out-of-line copies in Gormiti's objects are not in retail).
    const char*         GetName() const;
    bool                HasDifferentNameFromFMOD() const;
    void                GetFMODName( char** name );
    bool                IsAutoDelete() const;
    void                SetAutoDelete( bool autoDelete );
    void*               GetCustomData();
    void                SetCustomData( void* customData );

    char                m_name[64];             // 0x08
    FMOD::EventProject* m_fmodProject;          // 0x48
    ListUtils::STLList< EventSource* >  m_soundSources;     // 0x4C
    bool                m_isLoadedFromDisk;     // 0x5C
    bool                m_autoDelete;           // 0x5D
    bool                m_differentFMODName;    // 0x5E
    ESLoadParams        m_defaultParams;        // 0x60
    SProject*           m_parentProject;        // 0x68
    void*               m_customData;           // 0x6C
};

// Gormiti: EventSource.cpp (out of line in AsyncEventSource.obj there; retail inlines it into LoadEventSource).
inline void EventGroup::AddEventSource( EventSource* esource ) {
    m_soundSources.AddBack( esource );
}

inline const char* EventGroup::GetName() const {
    return m_name;
}

inline bool EventGroup::HasDifferentNameFromFMOD() const {
    return m_differentFMODName;
}

// The name FMOD knows the project by; *name is left alone when the group has no FMOD project.
inline void EventGroup::GetFMODName( char** name ) {
    if ( m_fmodProject != NULL ) {
        int index = 0;
        m_fmodProject->getInfo( &index, name );
    }
}

inline bool EventGroup::IsAutoDelete() const {
    return m_autoDelete;
}

inline void EventGroup::SetAutoDelete( bool autoDelete ) {
    m_autoDelete = autoDelete;
}

inline void* EventGroup::GetCustomData() {
    return m_customData;
}

inline void EventGroup::SetCustomData( void* customData ) {
    m_customData = customData;
}

}

#endif
