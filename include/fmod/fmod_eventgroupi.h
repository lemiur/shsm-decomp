/*
 * FMOD::EventGroupI: a group of events in an event project, implementing EventGroup.
 * Virtuals: retail vtable __vt__Q24FMOD11EventGroupI + Gormiti link map: EventGroup's 18 slots and
 * its destructor (implicit, weak in Gormiti), no own virtuals.
 * Layout: Gormiti DWARF (sibling.exe types EventGroupI), size 0x60. Only the non-virtual methods
 * used so far are declared.
 */
#ifndef _FMOD_EVENTGROUPI_H
#define _FMOD_EVENTGROUPI_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class EventI;
class EventProjectI;
class File;
class SoundBank;

// size 0x60
class EventGroupI : public EventGroup, public LinkedListNode {
public:
    char           *m_name;                 // 0x10
    int             m_index;                // 0x14
    unsigned int    m_flags;                // 0x18
    short           m_playcount;            // 0x1C
    short           m_loading;              // 0x1E
    FMOD_RESULT     m_asyncresult;          // 0x20
    EventGroupI    *m_eventgrouphead;       // 0x24
    EventGroupI    *m_parentgroup;          // 0x28
    EventProjectI  *m_project;              // 0x2C
    LinkedListNode  m_eventhead;            // 0x30
    LinkedListNode  m_userpropertyhead;     // 0x3C
    EventGroupI    *m_eventgroupmemory;     // 0x48
    int             m_numeventgroups;       // 0x4C
    void           *m_userdata;             // 0x50
    int             m_numsoundbanks;        // 0x54
    SoundBank     **m_soundbank;            // 0x58
    int           **m_bankentry_array;      // 0x5C

    EventGroupI();

    FMOD_RESULT init();
    FMOD_RESULT getEventPtr( const char *name, EventI **event, EventGroupI **group );
    FMOD_RESULT addGroup( EventGroupI *group );
    FMOD_RESULT buildSoundBankList();
    FMOD_RESULT loadEventDataInternal( FMOD_EVENT_RESOURCE resource, unsigned int mode, EventI *event );
    FMOD_RESULT adjustLoadCount( int count );
    FMOD_RESULT freeEventDataInternal( EventI *eventi, bool waituntilready, bool singleinstance );
    FMOD_RESULT freeInstanceData( EventI *event, bool waituntilready );
    FMOD_RESULT freeSampleData( EventI *event );
    FMOD_RESULT release( bool freeeventdata );
    FMOD_RESULT duplicateEvent( EventI *event, EventI **duplicate );
    FMOD_RESULT createInstances( EventI *event );
    // Takes an instance of event from a lower-priority playing one when none is free.
    FMOD_RESULT stealEventInstance( EventI *event, EventI **stolen, bool stopevent );
    FMOD_RESULT getEventInstance( EventI *event, Event **instance, unsigned int mode );
    FMOD_RESULT load( File *file, unsigned int version, FMOD_EVENT_LOADINFO *loadinfo );
    // Links an event into the group; inline, defined in fmod_eventgroupi.cpp (name invented).
    inline FMOD_RESULT addEvent( EventI *eventi );

    // EventGroup overrides
    virtual FMOD_RESULT getInfo( int *index, char **name );
    virtual FMOD_RESULT loadEventData( FMOD_EVENT_RESOURCE resource, unsigned int mode );
    virtual FMOD_RESULT freeEventData( Event *event, bool waituntilready );
    virtual FMOD_RESULT getGroup( const char *name, bool cacheevents, EventGroup **group );
    virtual FMOD_RESULT getGroupByIndex( int index, bool cacheevents, EventGroup **group );
    virtual FMOD_RESULT getParentGroup( EventGroup **group );
    virtual FMOD_RESULT getParentProject( EventProject **project );
    virtual FMOD_RESULT getNumGroups( int *numgroups );
    virtual FMOD_RESULT getEvent( const char *name, unsigned int mode, Event **event );
    virtual FMOD_RESULT getEventByIndex( int index, unsigned int mode, Event **event );
    virtual FMOD_RESULT getNumEvents( int *numevents );
    virtual FMOD_RESULT getProperty( const char *propertyname, void *value );
    virtual FMOD_RESULT getPropertyByIndex( int propertyindex, void *value );
    virtual FMOD_RESULT getNumProperties( int *numproperties );
    virtual FMOD_RESULT getState( unsigned int *state );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );
};

} // namespace FMOD

#endif /* _FMOD_EVENTGROUPI_H */
