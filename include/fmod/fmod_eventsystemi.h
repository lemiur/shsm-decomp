/*
 * FMOD::EventSystemI: the event system (one per application, g_eventsystemi), implementing the
 * EventSystem interface.
 * Virtuals: retail vtable __vt__Q24FMOD12EventSystemI + Gormiti link map: EventSystem's 41 slots
 * and destructor, then 18 of its own. Slots 0xC4 and 0xC8 are not paired with Gormiti (retail differs): named
 * after Gormiti's releaseSoundDefs and releaseSoundBanks, which they match in purpose (a project's sound defs, banks).
 * Layout: Gormiti DWARF (sibling.exe types EventSystemI), size 0xBC.
 */
#ifndef _FMOD_EVENTSYSTEMI_H
#define _FMOD_EVENTSYSTEMI_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class BucketHash;
class ChannelGroup;
class DSP;
class EventCategoryI;
class EventGroupI;
class EventI;
class EventProjectI;
class EventSound;
class File;
class MusicSystemI;
class SimpleMemPool;
class SoundBank;
class SoundDef;
class SoundDefDef;

// buildEventI's eventdataflags: which EventData_* blocks to allocate (names invented).
#define EVENTI_DATA_PARENT    0x00000002
#define EVENTI_DATA_ALL       0xFFFFFFFF
// buildEventI's eventtype: the body to build (name invented).
#define EVENTI_TYPE_SIMPLE    0x00000010
// ---- added for fmod_eventgroupi (names invented) ----
#define EVENTI_DATA_INSTANCE  0x00000004
#define EVENTI_TYPE_COMPLEX   0x00000008
// ---- end fmod_eventgroupi ----

// size 0xBC
class EventSystemI : public EventSystem, public LinkedListNode {
public:
    System          *m_system;                  // 0x10
    MusicSystemI    *m_musicsystem;             // 0x14
    bool             m_hasmusicdata;            // 0x18
    char            *m_mediapath;               // 0x1C
    char            *m_pluginpath;              // 0x20
    unsigned int     m_time;                    // 0x24
    char           **m_plugin_name;             // 0x28
    int              m_numplugins;              // 0x2C
    unsigned int     m_numevents;               // 0x30
    unsigned int     m_numloadedevents;         // 0x34
    unsigned int     m_numeventtableentries;    // 0x38
    EventI         **m_eventtable;              // 0x3C
    BucketHash      *m_eventguidhash;           // 0x40
    LinkedListNode   m_soundbankhead;           // 0x44
    LinkedListNode   m_sounddefhead;            // 0x50
    LinkedListNode   m_reverbdefhead;           // 0x5C
    LinkedListNode   m_eventreverbhead;         // 0x68
    LinkedListNode   m_eventprojecthead;        // 0x74
    LinkedListNode   m_memoryfsbhead;           // 0x80
    EventCategoryI  *m_mastercategory;          // 0x8C
    EventCategoryI  *m_musiccategory;           // 0x90
    ChannelGroup    *m_musicchannelgroup;       // 0x94
    unsigned int     m_flags;                   // 0x98
    unsigned int     m_initflags;               // 0x9C
    unsigned int     m_system_initflags;        // 0xA0
    int              m_maxstreamsperbank;       // 0xA4
    unsigned int     m_project_numevents;       // 0xA8
    void            *m_userdata;                // 0xAC
    int             *m_grouptempbuffer;         // 0xB0
    int              m_grouptempbuffersize;     // 0xB4
    unsigned int     m_lastprojectid;           // 0xB8

    // EventSystem overrides (slots 0x08-0xA8)
    virtual FMOD_RESULT init( int maxchannels, unsigned int flags, void *extradriverdata, unsigned int eventflags );
    virtual FMOD_RESULT release();
    virtual FMOD_RESULT update();
    virtual FMOD_RESULT setMediaPath( const char *path );
    virtual FMOD_RESULT setPluginPath( const char *path );
    virtual FMOD_RESULT getVersion( unsigned int *version );
    virtual FMOD_RESULT getInfo( FMOD_EVENT_SYSTEMINFO *info );
    virtual FMOD_RESULT getSystemObject( System **system );
    virtual FMOD_RESULT getMusicSystem( MusicSystem **musicsystem );
    virtual FMOD_RESULT load( const char *name_or_data, FMOD_EVENT_LOADINFO *loadinfo, EventProject **project );
    virtual FMOD_RESULT unload();
    virtual FMOD_RESULT getProject( const char *name, EventProject **project );
    virtual FMOD_RESULT getProjectByIndex( int index, EventProject **project );
    virtual FMOD_RESULT getNumProjects( int *numprojects );
    virtual FMOD_RESULT getCategory( const char *name, EventCategory **category );
    virtual FMOD_RESULT getCategoryByIndex( int index, EventCategory **category );
    virtual FMOD_RESULT getMusicCategory( EventCategory **category );
    virtual FMOD_RESULT getNumCategories( int *numcategories );
    virtual FMOD_RESULT getGroup( const char *name, bool cacheevents, EventGroup **group );
    virtual FMOD_RESULT getEvent( const char *name, unsigned int mode, Event **event );
    virtual FMOD_RESULT getEventBySystemID( unsigned int systemid, unsigned int mode, Event **event );
    virtual FMOD_RESULT getEventByGUID( const FMOD_GUID *guid, unsigned int mode, Event **event );
    virtual FMOD_RESULT getEventByGUIDString( const char *guid, unsigned int mode, Event **event );
    virtual FMOD_RESULT getNumEvents( int *numevents );
    virtual FMOD_RESULT setReverbProperties( const FMOD_REVERB_PROPERTIES *prop );
    virtual FMOD_RESULT getReverbProperties( FMOD_REVERB_PROPERTIES *prop );
    virtual FMOD_RESULT getReverbPreset( const char *name, FMOD_REVERB_PROPERTIES *prop, int *index );
    virtual FMOD_RESULT getReverbPresetByIndex( int index, FMOD_REVERB_PROPERTIES *prop, char **name );
    virtual FMOD_RESULT getNumReverbPresets( int *numpresets );
    virtual FMOD_RESULT createReverb( EventReverb **reverb );
    virtual FMOD_RESULT setReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop );
    virtual FMOD_RESULT getReverbAmbientProperties( FMOD_REVERB_PROPERTIES *prop );
    virtual FMOD_RESULT set3DNumListeners( int numlisteners );
    virtual FMOD_RESULT get3DNumListeners( int *numlisteners );
    virtual FMOD_RESULT set3DListenerAttributes( int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel,
                                                 const FMOD_VECTOR *forward, const FMOD_VECTOR *up );
    virtual FMOD_RESULT get3DListenerAttributes( int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel, FMOD_VECTOR *forward,
                                                 FMOD_VECTOR *up );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT preloadFSB( const char *filename, int streaminstance, Sound *sound );
    virtual FMOD_RESULT unloadFSB( const char *filename, int streaminstance );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );
    // slot 0xAC: the implicit destructor (weak in Gormiti)

    // Non-virtual methods used by fmod_eventgroupi.cpp
    FMOD_RESULT createEventHandle( EventI *eventi, Event **event, unsigned int mode, bool reserved );

    // Own virtuals (slots 0xB0-0xF4)
    virtual FMOD_RESULT loadPlugins();
    virtual FMOD_RESULT dereferenceSoundnames( EventGroupI *eventgroupi, SoundDef *projectsounddeflisthead, int fileversion );
    virtual FMOD_RESULT createDSPByName( const char *dspname, DSP **unit );
    virtual FMOD_RESULT getSoundbank( const char *soundbankname, SoundBank **soundbank );
    virtual FMOD_RESULT getSoundDef( const char *sounddefname, SoundDef **sounddef );
    virtual FMOD_RESULT releaseSoundDefs( EventProjectI *project );   // slot 0xC4
    virtual FMOD_RESULT releaseSoundBanks( EventProjectI *project );  // slot 0xC8
    virtual bool getMute() { return false; }            // slot 0xCC
    virtual FMOD_RESULT rebuildGUIDTable();
    virtual FMOD_RESULT rebuildEventTable();
    virtual FMOD_RESULT createEventTable( EventGroupI *eventgroup );
    virtual void setEventTableEntry( unsigned int index, EventI *eventi );
    virtual unsigned int getEventID();
    virtual FMOD_RESULT setFlags( unsigned int flags );
    virtual FMOD_RESULT countEvents( EventGroupI *group, int *numevents, int *numinstances );
    virtual FMOD_RESULT readSoundDefDef( File *file, unsigned int fileversion, SoundDefDef *sounddefdef );
    // Allocates an event (EventI, its EventData_* blocks and body) from mempool.
    virtual FMOD_RESULT buildEventI( unsigned int eventdataflags, unsigned int eventtype, SimpleMemPool *mempool, EventI **e );
    virtual FMOD_RESULT dereferenceSoundname( SoundDef *sounddef, int fileversion, EventSound *sound );
};

extern EventSystemI *g_eventsystemi;    // Gormiti link map: fmod_eventsystemi.obj

} // namespace FMOD

#endif /* _FMOD_EVENTSYSTEMI_H */
