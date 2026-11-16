/*
 * FMOD Event system public C++ API (fmod_event.hpp of the FMOD Designer programmer's API): the
 * declarations this project needs. The Event classes are abstract interfaces; the internal *I
 * classes (EventReverbI, ...) implement them.
 */
#ifndef _FMOD_EVENT_H
#define _FMOD_EVENT_H

#include <fmod/fmod.h>

/* The type of a user property's value. */
typedef enum {
    FMOD_EVENTPROPERTY_TYPE_INT,
    FMOD_EVENTPROPERTY_TYPE_FLOAT,
    FMOD_EVENTPROPERTY_TYPE_STRING
} FMOD_EVENTPROPERTY_TYPE;

namespace FMOD {

/*
 * 'EventReverb' API: a reverb definition of an event project, positioned in the world.
 */
class EventReverb {
public:
    virtual FMOD_RESULT F_API release() = 0;
    virtual FMOD_RESULT F_API set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance ) = 0;
    virtual FMOD_RESULT F_API get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance ) = 0;
    virtual FMOD_RESULT F_API setProperties( const FMOD_REVERB_PROPERTIES *props ) = 0;
    virtual FMOD_RESULT F_API getProperties( FMOD_REVERB_PROPERTIES *props ) = 0;
    virtual FMOD_RESULT F_API setActive( bool active ) = 0;
    virtual FMOD_RESULT F_API getActive( bool *active ) = 0;
    virtual FMOD_RESULT F_API setUserData( void *userdata ) = 0;
    virtual FMOD_RESULT F_API getUserData( void **userdata ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~EventReverb() {}
};

} // namespace FMOD


/*
 * Music system API (the interactive music part of fmod_event.hpp): types and the abstract
 * MusicSystem and MusicPrompt interfaces. Enumerators and layouts: Gormiti DWARF
 * (sibling.exe types FMOD_MUSIC_CALLBACKTYPE, FMOD_EVENT_RESOURCE, FMOD_MUSIC_ITERATOR).
 */
typedef enum {
    FMOD_EVENT_RESOURCE_STREAMS_AND_SAMPLES,
    FMOD_EVENT_RESOURCE_STREAMS,
    FMOD_EVENT_RESOURCE_SAMPLES
} FMOD_EVENT_RESOURCE;

typedef enum {
    FMOD_MUSIC_CALLBACKTYPE_SEGMENT_START,
    FMOD_MUSIC_CALLBACKTYPE_SEGMENT_END,
    FMOD_MUSIC_CALLBACKTYPE_RESET,
    FMOD_MUSIC_CALLBACKTYPE_BEAT
} FMOD_MUSIC_CALLBACKTYPE;

typedef FMOD_RESULT ( F_CALLBACK *FMOD_MUSIC_CALLBACK )( FMOD_MUSIC_CALLBACKTYPE type, void *param1, void *param2, void *userdata );

/* A music cue or parameter: its name and id. */
typedef struct {
    const char  *name;
    unsigned int id;
} FMOD_MUSIC_ENTITY;

/* The state of a getCues / getParameters enumeration. */
typedef struct {
    FMOD_MUSIC_ENTITY *value;
    const char        *filter;
} FMOD_MUSIC_ITERATOR;

typedef struct {
    int starving;
} FMOD_MUSIC_INFO;

namespace FMOD {

/*
 * 'MusicPrompt' API: a cue prepared for playing.
 */
class MusicPrompt {
public:
    virtual FMOD_RESULT F_API release() = 0;
    virtual FMOD_RESULT F_API begin() = 0;
    virtual FMOD_RESULT F_API end() = 0;
    virtual FMOD_RESULT F_API isActive( bool *active ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~MusicPrompt() {}
};

/*
 * 'MusicSystem' API: the interactive music of an event system.
 */
class MusicSystem {
public:
    virtual FMOD_RESULT F_API reset() = 0;
    virtual FMOD_RESULT F_API setVolume( float volume ) = 0;
    virtual FMOD_RESULT F_API getVolume( float *volume ) = 0;
    virtual FMOD_RESULT F_API setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) = 0;
    virtual FMOD_RESULT F_API getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop ) = 0;
    virtual FMOD_RESULT F_API setPaused( bool paused ) = 0;
    virtual FMOD_RESULT F_API getPaused( bool *paused ) = 0;
    virtual FMOD_RESULT F_API setMute( bool mute ) = 0;
    virtual FMOD_RESULT F_API getMute( bool *mute ) = 0;
    virtual FMOD_RESULT F_API getInfo( FMOD_MUSIC_INFO *info ) = 0;
    virtual FMOD_RESULT F_API promptCue( unsigned int id ) = 0;
    virtual FMOD_RESULT F_API prepareCue( unsigned int id, MusicPrompt **prompt ) = 0;
    virtual FMOD_RESULT F_API getParameterValue( unsigned int id, float *parameter ) = 0;
    virtual FMOD_RESULT F_API setParameterValue( unsigned int id, float parameter ) = 0;
    virtual FMOD_RESULT F_API getCues( FMOD_MUSIC_ITERATOR *it, const char *filter = 0 ) = 0;
    virtual FMOD_RESULT F_API getNextCue( FMOD_MUSIC_ITERATOR *it ) = 0;
    virtual FMOD_RESULT F_API getParameters( FMOD_MUSIC_ITERATOR *it, const char *filter = 0 ) = 0;
    virtual FMOD_RESULT F_API getNextParameter( FMOD_MUSIC_ITERATOR *it ) = 0;
    virtual FMOD_RESULT F_API loadSoundData( FMOD_EVENT_RESOURCE resource, unsigned int mode ) = 0;
    virtual FMOD_RESULT F_API freeSoundData( bool waituntilready ) = 0;
    virtual FMOD_RESULT F_API setCallback( FMOD_MUSIC_CALLBACK callback, void *userdata ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~MusicSystem() {}
};

} // namespace FMOD


/*
 * Event API (the event part of fmod_event.hpp): types, the Event handle and the abstract
 * EventGroup and EventSystem interfaces. Enumerators and layouts: Gormiti DWARF
 * (sibling.exe types FMOD_EVENT_CALLBACKTYPE, FMOD_EVENT_INFO). Virtuals: retail vtables
 * __vt__Q24FMOD10EventGroup and __vt__Q24FMOD11EventSystem (pure slots) + Gormiti link map; the
 * slot names come from the implementing EventGroupI and EventSystemI slots and the FMOD Ex 4.x API.
 */
typedef struct FMOD_EVENT FMOD_EVENT;               // an Event handle, as the C API sees it
typedef struct FMOD_EVENTSYSTEM FMOD_EVENTSYSTEM;   // an EventSystem handle, as the C API sees it
typedef struct FMOD_GUID FMOD_GUID;
typedef struct FMOD_EVENT_WAVEBANKINFO FMOD_EVENT_WAVEBANKINFO;
typedef struct FMOD_EVENT_SYSTEMINFO FMOD_EVENT_SYSTEMINFO;
typedef struct FMOD_EVENT_LOADINFO FMOD_EVENT_LOADINFO;

// ---- added for fmod_eventgroupi (public FMOD Ex 4.x header; layouts: Gormiti DWARF) ----
// FMOD_EVENT_INITFLAGS bit: event and group names are not loaded.
#define FMOD_EVENT_INIT_DONTUSENAMES 0x00000004

struct FMOD_GUID {
    unsigned int   Data1;           // 0x00
    unsigned short Data2;           // 0x04
    unsigned short Data3;           // 0x06
    unsigned char  Data4[8];        // 0x08
};

// Optional settings for EventSystem::load. size 0x18
struct FMOD_EVENT_LOADINFO {
    unsigned int  size;                         // 0x00, sizeof the structure the caller filled
    char         *encryptionkey;                // 0x04
    float         sounddefentrylimit;           // 0x08
    unsigned int  loadfrommemory_length;        // 0x0C
    int           override_category_vals;       // 0x10
    unsigned int  sizeof_instancepool_simple;   // 0x14
};
// ---- end fmod_eventgroupi ----

typedef enum {
    FMOD_EVENT_CALLBACKTYPE_SYNCPOINT,
    FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_START,
    FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_END,
    FMOD_EVENT_CALLBACKTYPE_STOLEN,
    FMOD_EVENT_CALLBACKTYPE_EVENTFINISHED,
    FMOD_EVENT_CALLBACKTYPE_NET_MODIFIED,
    FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_CREATE,
    FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_RELEASE,
    FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_INFO,
    FMOD_EVENT_CALLBACKTYPE_EVENTSTARTED,
    FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_SELECTINDEX,
    FMOD_EVENT_CALLBACKTYPE_OCCLUSION
} FMOD_EVENT_CALLBACKTYPE;

typedef FMOD_RESULT ( F_CALLBACK *FMOD_EVENT_CALLBACK )( FMOD_EVENT *event, FMOD_EVENT_CALLBACKTYPE type, void *param1,
                                                       void *param2, void *userdata );

// The units of an event's pitch (enumerators from the FMOD Ex 4.x API).
typedef enum {
    FMOD_EVENT_PITCHUNITS_RAW,
    FMOD_EVENT_PITCHUNITS_OCTAVES,
    FMOD_EVENT_PITCHUNITS_SEMITONES,
    FMOD_EVENT_PITCHUNITS_TONES
} FMOD_EVENT_PITCHUNITS;

/* What Event::getInfo reports. */
struct FMOD_EVENT_INFO {
    int                      memoryused;        // 0x00
    int                      positionms;        // 0x04
    int                      lengthms;          // 0x08
    int                      channelsplaying;   // 0x0C
    int                      instancesactive;   // 0x10
    int                      maxwavebanks;      // 0x14
    FMOD_EVENT_WAVEBANKINFO *wavebankinfo;      // 0x18
    unsigned int             projectid;         // 0x1C
    unsigned int             systemid;          // 0x20
    float                    audibility;        // 0x24
    int                      numinstances;      // 0x28
    FMOD_EVENT             **instances;         // 0x2C
    FMOD_GUID               *guid;              // 0x30
};
typedef struct FMOD_EVENT_INFO FMOD_EVENT_INFO;

// Event modes (FMOD_EVENT_MODE of the public API)
#define FMOD_EVENT_DEFAULT      0x00000000
#define FMOD_EVENT_NONBLOCKING  0x00000001
#define FMOD_EVENT_ERROR_ON_DISKACCESS 0x00000002   // added for fmod_eventimpl_simple (public API value)
#define FMOD_EVENT_INFOONLY     0x00000004
// ---- added for fmod_eventgroupi (public FMOD Ex 4.x header) ----
#define FMOD_EVENT_USERDSP      0x00000008
// ---- end fmod_eventgroupi ----

// Event and group state bits (FMOD_EVENT_STATE of the public API)
#define FMOD_EVENT_STATE_READY      0x00000001
#define FMOD_EVENT_STATE_LOADING    0x00000002
#define FMOD_EVENT_STATE_ERROR      0x00000004
#define FMOD_EVENT_STATE_PLAYING    0x00000008
// ---- added for fmod_eventimpl_simple (public API values) ----
#define FMOD_EVENT_STATE_CHANNELSACTIVE 0x00000010
#define FMOD_EVENT_STATE_STARVING       0x00000040

// ---- added for fmod_eventimpl_simple (FMOD_EVENT_INITFLAGS of the public API) ----
#define FMOD_EVENT_INIT_USER_ASSETMANAGER   0x00000001
#define FMOD_EVENT_INIT_FAIL_ON_MAXSTREAMS  0x00000002

namespace FMOD {

class ChannelGroup;
class EventCategory;
class EventGroup;
class EventParameter;
class EventProject;
class EventReverb;
class MusicSystem;
class Sound;
class System;

/*
 * 'Event' API: a handle, not an object. Its methods (in fmod_eventi.cpp) validate the handle
 * (EventI::validate) and forward to the EventI it names; the class has no data and no virtuals
 * (Gormiti's DWARF gives it size 1). Only the methods retail keeps are declared.
 */
class Event {
public:
    FMOD_RESULT F_API start();
    FMOD_RESULT F_API stop( bool immediate = false );
    FMOD_RESULT F_API getInfo( int *index, char **name, FMOD_EVENT_INFO *info );
    FMOD_RESULT F_API getParameterByIndex( int index, EventParameter **parameter );
    FMOD_RESULT F_API getNumParameters( int *numparameters );
    FMOD_RESULT F_API getProperty( const char *propertyname, void *value, bool this_instance = false );
    FMOD_RESULT F_API getPropertyByIndex( int propertyindex, void *value, bool this_instance = false );
    FMOD_RESULT F_API setPropertyByIndex( int propertyindex, void *value, bool this_instance = false );
    FMOD_RESULT F_API setCallback( FMOD_EVENT_CALLBACK callback, void *userdata );
    FMOD_RESULT F_API setVolume( float volume );
    FMOD_RESULT F_API getVolume( float *volume );
    FMOD_RESULT F_API setPaused( bool paused );
    FMOD_RESULT F_API getPaused( bool *paused );
    FMOD_RESULT F_API set3DAttributes( const FMOD_VECTOR *position, const FMOD_VECTOR *velocity,
                                       const FMOD_VECTOR *orientation = 0 );
    FMOD_RESULT F_API get3DAttributes( FMOD_VECTOR *position, FMOD_VECTOR *velocity, FMOD_VECTOR *orientation = 0 );
    FMOD_RESULT F_API setUserData( void *userdata );
    FMOD_RESULT F_API getState( unsigned int *state );
    FMOD_RESULT F_API getParentGroup( EventGroup **group );
    FMOD_RESULT F_API getChannelGroup( ChannelGroup **channelgroup );
};

// ---- added for fmod_eventgroupi ----
/*
 * 'EventCategory' API: a category of events (volume, pitch, pause and mute for its events).
 * Virtuals: the EventCategoryI vtable __vt__Q24FMOD14EventCategoryI (its first 19 slots, the FMOD
 * Ex 4.x API names) and __vt__Q24FMOD13EventCategory (its 19 pure slots and inline destructor).
 */
class EventCategory {
public:
    virtual FMOD_RESULT F_API getInfo( int *index, char **name ) = 0;
    virtual FMOD_RESULT F_API getCategory( const char *name, EventCategory **category ) = 0;
    virtual FMOD_RESULT F_API getCategoryByIndex( int index, EventCategory **category ) = 0;
    virtual FMOD_RESULT F_API getNumCategories( int *numcategories ) = 0;
    virtual FMOD_RESULT F_API getEventByIndex( int index, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getNumEvents( int *numevents ) = 0;
    virtual FMOD_RESULT F_API stopAllEvents() = 0;
    virtual FMOD_RESULT F_API setVolume( float volume ) = 0;
    virtual FMOD_RESULT F_API getVolume( float *volume ) = 0;
    virtual FMOD_RESULT F_API setPitch( float pitch, FMOD_EVENT_PITCHUNITS units ) = 0;
    virtual FMOD_RESULT F_API getPitch( float *pitch, FMOD_EVENT_PITCHUNITS units ) = 0;
    virtual FMOD_RESULT F_API setPaused( bool paused ) = 0;
    virtual FMOD_RESULT F_API getPaused( bool *paused ) = 0;
    virtual FMOD_RESULT F_API setMute( bool mute ) = 0;
    virtual FMOD_RESULT F_API getMute( bool *mute ) = 0;
    virtual FMOD_RESULT F_API getChannelGroup( ChannelGroup **channelgroup ) = 0;
    virtual FMOD_RESULT F_API setUserData( void *userdata ) = 0;
    virtual FMOD_RESULT F_API getUserData( void **userdata ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~EventCategory() {}
};
// ---- end fmod_eventgroupi ----

/*
 * 'EventGroup' API: a group of events in an event project.
 */
class EventGroup {
public:
    virtual FMOD_RESULT F_API getInfo( int *index, char **name ) = 0;
    virtual FMOD_RESULT F_API loadEventData( FMOD_EVENT_RESOURCE resource, unsigned int mode ) = 0;
    virtual FMOD_RESULT F_API freeEventData( Event *event, bool waituntilready ) = 0;
    virtual FMOD_RESULT F_API getGroup( const char *name, bool cacheevents, EventGroup **group ) = 0;
    virtual FMOD_RESULT F_API getGroupByIndex( int index, bool cacheevents, EventGroup **group ) = 0;
    virtual FMOD_RESULT F_API getParentGroup( EventGroup **group ) = 0;
    virtual FMOD_RESULT F_API getParentProject( EventProject **project ) = 0;
    virtual FMOD_RESULT F_API getNumGroups( int *numgroups ) = 0;
    virtual FMOD_RESULT F_API getEvent( const char *name, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getEventByIndex( int index, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getNumEvents( int *numevents ) = 0;
    virtual FMOD_RESULT F_API getProperty( const char *propertyname, void *value ) = 0;
    virtual FMOD_RESULT F_API getPropertyByIndex( int propertyindex, void *value ) = 0;
    virtual FMOD_RESULT F_API getNumProperties( int *numproperties ) = 0;
    virtual FMOD_RESULT F_API getState( unsigned int *state ) = 0;
    virtual FMOD_RESULT F_API setUserData( void *userdata ) = 0;
    virtual FMOD_RESULT F_API getUserData( void **userdata ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~EventGroup() {}
};

/*
 * 'EventProject' API: a project of an event system. Added for fmod_eventgroupi.cpp (parent project
 * pointer): 13 slots and the destructor, from retail's vtable lbl_80451F70 (first of its two tables)
 * and the symbol names of EventProjectI. Parameter names are from the FMOD Designer API.
 */
class EventProject {
public:
    virtual FMOD_RESULT F_API release() = 0;
    virtual FMOD_RESULT F_API getInfo( int *index, char **name ) = 0;
    virtual FMOD_RESULT F_API getGroup( const char *name, bool cacheevents, EventGroup **group ) = 0;
    virtual FMOD_RESULT F_API getGroupByIndex( int index, bool cacheevents, EventGroup **group ) = 0;
    virtual FMOD_RESULT F_API getNumGroups( int *numgroups ) = 0;
    virtual FMOD_RESULT F_API getEvent( const char *name, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getEventByProjectID( unsigned int projectid, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getNumEvents( int *numevents ) = 0;
    virtual FMOD_RESULT F_API loadSampleData( int *eventid_array, int sizeof_eventid_array, char **groupname_array,
                                              int sizeof_groupname_array, unsigned int mode ) = 0;
    virtual FMOD_RESULT F_API setUserData( void *userdata ) = 0;
    virtual FMOD_RESULT F_API getUserData( void **userdata ) = 0;
    virtual FMOD_RESULT F_API stopAllEvents( bool immediate ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~EventProject() {}
};

/*
 * 'EventSystem' API: the event system of an FMOD system. Slot names release, unload and
 * getCategoryByIndex are from the FMOD Ex 4.x API by position and use (Gormiti has no name for them).
 */
class EventSystem {
public:
    virtual FMOD_RESULT F_API init( int maxchannels, unsigned int flags, void *extradriverdata, unsigned int eventflags ) = 0;
    virtual FMOD_RESULT F_API release() = 0;
    virtual FMOD_RESULT F_API update() = 0;
    virtual FMOD_RESULT F_API setMediaPath( const char *path ) = 0;
    virtual FMOD_RESULT F_API setPluginPath( const char *path ) = 0;
    virtual FMOD_RESULT F_API getVersion( unsigned int *version ) = 0;
    virtual FMOD_RESULT F_API getInfo( FMOD_EVENT_SYSTEMINFO *info ) = 0;
    virtual FMOD_RESULT F_API getSystemObject( System **system ) = 0;
    virtual FMOD_RESULT F_API getMusicSystem( MusicSystem **musicsystem ) = 0;
    virtual FMOD_RESULT F_API load( const char *name_or_data, FMOD_EVENT_LOADINFO *loadinfo, EventProject **project ) = 0;
    virtual FMOD_RESULT F_API unload() = 0;
    virtual FMOD_RESULT F_API getProject( const char *name, EventProject **project ) = 0;
    virtual FMOD_RESULT F_API getProjectByIndex( int index, EventProject **project ) = 0;
    virtual FMOD_RESULT F_API getNumProjects( int *numprojects ) = 0;
    virtual FMOD_RESULT F_API getCategory( const char *name, EventCategory **category ) = 0;
    virtual FMOD_RESULT F_API getCategoryByIndex( int index, EventCategory **category ) = 0;
    virtual FMOD_RESULT F_API getMusicCategory( EventCategory **category ) = 0;
    virtual FMOD_RESULT F_API getNumCategories( int *numcategories ) = 0;
    virtual FMOD_RESULT F_API getGroup( const char *name, bool cacheevents, EventGroup **group ) = 0;
    virtual FMOD_RESULT F_API getEvent( const char *name, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getEventBySystemID( unsigned int systemid, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getEventByGUID( const FMOD_GUID *guid, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getEventByGUIDString( const char *guid, unsigned int mode, Event **event ) = 0;
    virtual FMOD_RESULT F_API getNumEvents( int *numevents ) = 0;
    virtual FMOD_RESULT F_API setReverbProperties( const FMOD_REVERB_PROPERTIES *prop ) = 0;
    virtual FMOD_RESULT F_API getReverbProperties( FMOD_REVERB_PROPERTIES *prop ) = 0;
    virtual FMOD_RESULT F_API getReverbPreset( const char *name, FMOD_REVERB_PROPERTIES *prop, int *index ) = 0;
    virtual FMOD_RESULT F_API getReverbPresetByIndex( int index, FMOD_REVERB_PROPERTIES *prop, char **name ) = 0;
    virtual FMOD_RESULT F_API getNumReverbPresets( int *numpresets ) = 0;
    virtual FMOD_RESULT F_API createReverb( EventReverb **reverb ) = 0;
    virtual FMOD_RESULT F_API setReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop ) = 0;
    virtual FMOD_RESULT F_API getReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop ) = 0;
    virtual FMOD_RESULT F_API set3DNumListeners( int numlisteners ) = 0;
    virtual FMOD_RESULT F_API get3DNumListeners( int *numlisteners ) = 0;
    virtual FMOD_RESULT F_API set3DListenerAttributes( int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel,
                                                       const FMOD_VECTOR *forward, const FMOD_VECTOR *up ) = 0;
    virtual FMOD_RESULT F_API get3DListenerAttributes( int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel, FMOD_VECTOR *forward,
                                                       FMOD_VECTOR *up ) = 0;
    virtual FMOD_RESULT F_API setUserData( void *userdata ) = 0;
    virtual FMOD_RESULT F_API getUserData( void **userdata ) = 0;
    virtual FMOD_RESULT F_API preloadFSB( const char *filename, int streaminstance, Sound *sound ) = 0;
    virtual FMOD_RESULT F_API unloadFSB( const char *filename, int streaminstance ) = 0;
    virtual FMOD_RESULT F_API getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                             unsigned int *memoryused_details ) = 0;
    virtual ~EventSystem() {}
};

// The creation path behind FMOD_EventSystem_Create (fmod_event.cpp).
FMOD_RESULT EventSystem_CreateInternal( EventSystem **eventsystem, System *system );

} // namespace FMOD

// The public creation call (FMOD Ex 4.x fmod_event.h / fmod_event.hpp).
extern "C" FMOD_RESULT F_API FMOD_EventSystem_Create( FMOD_EVENTSYSTEM **eventsystem );

namespace FMOD {

inline FMOD_RESULT EventSystem_Create( EventSystem **eventsystem ) {
    return FMOD_EventSystem_Create( reinterpret_cast< FMOD_EVENTSYSTEM** >( eventsystem ) );
}

} // namespace FMOD

#endif /* _FMOD_EVENT_H */
