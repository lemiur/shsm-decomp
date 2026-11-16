/*
 * FMOD::EventProjectI: a loaded event project, implementing EventProject.
 * Virtuals: retail vtable lbl_80451F70, first 0x40 table (14 slots: EventProject's 13 in order and
 * the implicit destructor fn_803A1D8C); the second table is EventProject's (13 pure slots, then its
 * inline destructor fn_803A08A4).
 * Layout: Gormiti DWARF (sibling.exe types EventProjectI), size 0x78.
 * Bundle (a plain class, Gormiti DWARF size 0x1C) is declared here with its methods, which this
 * unit defines.
 */
#ifndef _FMOD_EVENTPROJECTI_H
#define _FMOD_EVENTPROJECTI_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class EventGroupI;
class EventI;
class EventInstancePool;
class EventSystemI;
class SimpleMemPool;
class SoundBank;
class SoundDef;
class SoundDefDef;

// The sound banks, event groups and events a loadSampleData request resolves to; built by init and
// copied per request. size 0x1C
class Bundle {
public:
    EventI       **m_event;             // 0x00
    int            m_numevents;         // 0x04
    SoundBank    **m_soundbank;         // 0x08
    int            m_numsoundbanks;     // 0x0C
    int          **m_bankentry_array;   // 0x10
    EventGroupI  **m_eventgroup;        // 0x14
    int            m_numeventgroups;    // 0x18

    Bundle() {
        m_event           = NULL;
        m_numevents       = 0;
        m_soundbank       = NULL;
        m_numsoundbanks   = 0;
        m_bankentry_array = NULL;
        m_eventgroup      = NULL;
        m_numeventgroups  = 0;
    }

    FMOD_RESULT init( EventI **events, int numevents );
    FMOD_RESULT release();
    FMOD_RESULT duplicate( Bundle **bundle );
};

// size 0x78
class EventProjectI : public EventProject, public LinkedListNode {
public:
    char              *m_name;                      // 0x10
    bool               m_hasmusicdata;              // 0x14
    unsigned int       m_flags;                     // 0x18
    LinkedListNode     m_eventgrouphead;            // 0x1C
    unsigned int       m_numevents;                 // 0x28
    unsigned int       m_numeventgroups;            // 0x2C
    EventI           **m_eventtable;                // 0x30
    EventInstancePool *m_instancepool;              // 0x34
    unsigned int       m_sizeof_sounddef_pool;      // 0x38
    SimpleMemPool     *m_sounddef_pool;             // 0x3C
    SoundBank         *m_soundbankmemory;           // 0x40
    int                m_sizeof_soundbankmemory;    // 0x44
    EventGroupI       *m_eventgroupmemory;          // 0x48
    int                m_sizeof_eventgroupmemory;   // 0x4C
    SoundDef          *m_sounddefmemory;            // 0x50
    int                m_sizeof_sounddefmemory;     // 0x54
    SoundDefDef       *m_sounddefdefmemory;         // 0x58
    int                m_sizeof_sounddefdefmemory;  // 0x5C
    EventSystemI      *m_eventsystem;               // 0x60
    void              *m_userdata;                  // 0x64
    LinkedListNode     m_updatetimeparameter_head;  // 0x68
    unsigned int       m_id;                        // 0x74

    EventProjectI();

    FMOD_RESULT init();
    FMOD_RESULT createEventTable( unsigned int numevents, EventGroupI *group );
    FMOD_RESULT createInstancePool( unsigned int numinstances );
    FMOD_RESULT addLSDGroup( EventGroupI **eventgrouparray, int &num_added_groups, EventGroupI *eventgroup,
                             EventI **eventarray, int sizeof_eventarray, int &num_added_events );

    // EventProject overrides
    virtual FMOD_RESULT release();
    virtual FMOD_RESULT getInfo( int *index, char **name );
    virtual FMOD_RESULT getGroup( const char *name, bool cacheevents, EventGroup **group );
    virtual FMOD_RESULT getGroupByIndex( int index, bool cacheevents, EventGroup **group );
    virtual FMOD_RESULT getNumGroups( int *numgroups );
    virtual FMOD_RESULT getEvent( const char *name, unsigned int mode, Event **event );
    virtual FMOD_RESULT getEventByProjectID( unsigned int projectid, unsigned int mode, Event **event );
    virtual FMOD_RESULT getNumEvents( int *numevents );
    virtual FMOD_RESULT loadSampleData( int *eventid_array, int sizeof_eventid_array, char **groupname_array,
                                        int sizeof_groupname_array, unsigned int mode );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT stopAllEvents( bool immediate );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );
};

} // namespace FMOD

#endif /* _FMOD_EVENTPROJECTI_H */
