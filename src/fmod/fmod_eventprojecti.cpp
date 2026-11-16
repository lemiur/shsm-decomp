/*
 * fmod_eventprojecti.cpp: FMOD::EventProjectI, a loaded event project: its event groups and events,
 * lookups by name and index, the event table and instance pool, loading and releasing. Also Bundle,
 * the sound banks and groups a sample data request resolves to. Allocation tags carry the original
 * file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)"): the #line directives restore
 * them.
 */
// Names and layout: Gormiti link map and DWARF (fmod_event.a fmod_eventprojecti.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_compositioncore.h>
#include <fmod/fmod_eventgroupi.h>
#include <fmod/fmod_eventi.h>
#include <fmod/fmod_eventinstancepool.h>
#include <fmod/fmod_eventprojecti.h>
#include <fmod/fmod_eventreverb.h>
#include <fmod/fmod_eventsystemi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_musicsystemi.h>
#include <fmod/fmod_simplemempool.h>
#include <fmod/fmod_soundbank.h>
#include <fmod/fmod_string.h>
#include <new>
#include <string.h>

namespace FMOD {

// Returns from the enclosing function when a call fails.
#define CHECK_RESULT( _call )              \
    {                                      \
        FMOD_RESULT _result = ( _call );   \
        if ( _result != FMOD_OK ) {        \
            return _result;                \
        }                                  \
    }

// Largest instance pool a project can ask for (value from retail).
#define EVENTPROJECTI_MAX_INSTANCES 50000

// Distinct groups a bundle takes events from, at most (value from retail: the array is 400 bytes).
#define BUNDLE_MAX_GROUPS 100

// Entries a sound bank can have, as far as a bundle tracks them (value from retail).
#define BUNDLE_MAX_ENTRIES 10000

// Longest event path getEvent copies, with its terminator (value from retail).
#define EVENTPROJECTI_PATH_MAX 1024

/*
====================
Bundle::init
====================
*/
FMOD_RESULT Bundle::init( EventI **events, int numevents ) {
    int           i, j;
    EventSystemI *eventsystemi;

    if ( !events || numevents <= 0 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    m_event     = events;
    m_numevents = numevents;

    // The distinct groups of the events.
    EventGroupI *group[BUNDLE_MAX_GROUPS];
    int          numgroups = 0;

    memset( group, 0, sizeof( group ) );

    for ( i = 0; i < m_numevents; i++ ) {
        EventI *e = m_event[i];

        for ( j = 0; j < numgroups; j++ ) {
            if ( group[j] == e->m_data_base.m_eventgroupi ) {
                break;
            }
        }

        if ( j >= numgroups ) {
            group[numgroups++] = e->m_data_base.m_eventgroupi;
        }
    }

    m_numeventgroups = numgroups;

#line 76
    m_eventgroup = (EventGroupI **)FMOD_Memory_Alloc( numgroups * sizeof( EventGroupI * ) );
    if ( !m_eventgroup ) {
        return FMOD_ERR_MEMORY;
    }

    memcpy( m_eventgroup, group, m_numeventgroups * sizeof( EventGroupI * ) );

    eventsystemi = m_eventgroup[0]->m_project->m_eventsystem;

    // Room for every sound bank of the project, and per bank a table counting the entries the
    // events use.
    m_numsoundbanks = 0;

#line 93
    m_soundbank = (SoundBank **)FMOD_Memory_Calloc( eventsystemi->m_soundbankhead.count() * sizeof( SoundBank * ) );
    if ( !m_soundbank ) {
        return FMOD_ERR_MEMORY;
    }

#line 98
    m_bankentry_array = (int **)FMOD_Memory_Calloc( eventsystemi->m_soundbankhead.count() * sizeof( int * ) );
    if ( !m_bankentry_array ) {
        return FMOD_ERR_MEMORY;
    }

    // The sample banks the events use, and how often each of their entries is used.
    for ( int event_index = 0; event_index < m_numevents; event_index++ ) {
        EventI *e = m_event[event_index];

        for ( int bank_index = 0; bank_index < e->m_data_parent->m_numsoundbanks; bank_index++ ) {
            SoundBank *soundbank = e->m_data_parent->m_soundbank[bank_index];

            if ( soundbank->m_mode & FMOD_CREATESTREAM ) {
                continue;
            }

            int existingbank_index;
            for ( existingbank_index = 0; existingbank_index < m_numsoundbanks; existingbank_index++ ) {
                if ( m_soundbank[existingbank_index] == soundbank ) {
                    break;
                }
            }

            if ( existingbank_index >= m_numsoundbanks ) {
                m_soundbank[m_numsoundbanks] = soundbank;

#line 145
                m_bankentry_array[m_numsoundbanks] = (int *)FMOD_Memory_Calloc( BUNDLE_MAX_ENTRIES * sizeof( int ) );
                if ( !m_bankentry_array[m_numsoundbanks] ) {
                    return FMOD_ERR_MEMORY;
                }
                m_numsoundbanks++;
            }

            int *src_bankentry_array = e->m_data_parent->m_bankentry_array[bank_index];
            int *dst_bankentry_array = m_bankentry_array[existingbank_index];

            for ( int entryindex = 0; src_bankentry_array[entryindex] != -1; entryindex++ ) {
                dst_bankentry_array[src_bankentry_array[entryindex]]++;
            }
        }
    }

    // Replace each counting table by the list of the entries used, ended by -1.
    for ( i = 0; i < m_numsoundbanks; i++ ) {
        int  final_numentries = 0;
        int *bankentry_array  = m_bankentry_array[i];

        for ( j = 0; j < BUNDLE_MAX_ENTRIES; j++ ) {
            if ( bankentry_array[j] ) {
                final_numentries++;
            }
        }

#line 191
        int *final_bankentry_array = (int *)FMOD_Memory_Calloc( ( final_numentries + 1 ) * sizeof( int ) );
        if ( !final_bankentry_array ) {
            return FMOD_ERR_MEMORY;
        }

        int entry_index = 0;
        for ( j = 0; j < BUNDLE_MAX_ENTRIES; j++ ) {
            if ( bankentry_array[j] ) {
                final_bankentry_array[entry_index++] = j;
            }
        }
        final_bankentry_array[entry_index] = -1;

#line 218
        FMOD_Memory_Free( bankentry_array );
        m_bankentry_array[i] = final_bankentry_array;
    }

    return FMOD_OK;
} // 0x8039FEE4

/*
====================
Bundle::release

Frees the arrays the bundle owns, then the bundle itself.
====================
*/
FMOD_RESULT Bundle::release() {
    if ( m_event ) {
#line 230
        FMOD_Memory_Free( m_event );
    }

    if ( m_soundbank ) {
#line 235
        FMOD_Memory_Free( m_soundbank );
    }

    if ( m_bankentry_array ) {
        for ( int i = 0; i < m_numsoundbanks; i++ ) {
            if ( m_bankentry_array[i] ) {
#line 244
                FMOD_Memory_Free( m_bankentry_array[i] );
            }
        }
#line 248
        FMOD_Memory_Free( m_bankentry_array );
    }

    if ( m_eventgroup ) {
#line 253
        FMOD_Memory_Free( m_eventgroup );
    }

#line 256
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803A044C

/*
====================
Bundle::duplicate
====================
*/
FMOD_RESULT Bundle::duplicate( Bundle **bundle ) {
    FMOD_RESULT result;
    Bundle     *that;

#line 266
    that = new ( FMOD_Memory_Alloc( sizeof( Bundle ) ) ) Bundle;
    if ( !that ) {
        return FMOD_ERR_MEMORY;
    }

    that->m_event           = NULL;
    that->m_numevents       = m_numevents;
    that->m_soundbank       = NULL;
    that->m_numsoundbanks   = m_numsoundbanks;
    that->m_bankentry_array = NULL;
    that->m_eventgroup      = NULL;
    that->m_numeventgroups  = m_numeventgroups;

    if ( m_event ) {
#line 282
        that->m_event = (EventI **)FMOD_Memory_Calloc( m_numevents * sizeof( EventI * ) );
        if ( !that->m_event ) {
            result = FMOD_ERR_MEMORY;
            goto fail;
        }

        memcpy( that->m_event, m_event, m_numevents * sizeof( EventI * ) );
    }

    if ( m_soundbank ) {
#line 294
        that->m_soundbank = (SoundBank **)FMOD_Memory_Calloc( m_numsoundbanks * sizeof( SoundBank * ) );
        if ( !that->m_soundbank ) {
            result = FMOD_ERR_MEMORY;
            goto fail;
        }

        memcpy( that->m_soundbank, m_soundbank, m_numsoundbanks * sizeof( SoundBank * ) );
    }

    if ( m_bankentry_array ) {
#line 306
        that->m_bankentry_array = (int **)FMOD_Memory_Calloc( m_numsoundbanks * sizeof( int * ) );
        if ( !that->m_bankentry_array ) {
            result = FMOD_ERR_MEMORY;
            goto fail;
        }

        for ( int i = 0; i < m_numsoundbanks; i++ ) {
            if ( m_bankentry_array[i] ) {
                int numentries = 0;

                while ( m_bankentry_array[i][numentries] != -1 ) {
                    numentries++;
                }

#line 326
                that->m_bankentry_array[i] = (int *)FMOD_Memory_Calloc( ( numentries + 1 ) * sizeof( int ) );
                if ( !that->m_bankentry_array[i] ) {
                    result = FMOD_ERR_MEMORY;
                    goto fail;
                }

                memcpy( that->m_bankentry_array[i], m_bankentry_array[i], ( numentries + 1 ) * sizeof( int ) );
            }
        }
    }

    if ( m_eventgroup ) {
#line 340
        that->m_eventgroup = (EventGroupI **)FMOD_Memory_Calloc( m_numeventgroups * sizeof( EventGroupI * ) );
        if ( !that->m_eventgroup ) {
            result = FMOD_ERR_MEMORY;
            goto fail;
        }

        memcpy( that->m_eventgroup, m_eventgroup, m_numeventgroups * sizeof( EventGroupI * ) );
    }

    *bundle = that;

    return FMOD_OK;

fail:
    that->release();

    return result;
} // 0x803A0590

/*
====================
EventProjectI::EventProjectI
====================
*/
EventProjectI::EventProjectI() {
    init();
} // 0x803A0834

// EventProject::~EventProject is the inline destructor of the EventProject interface; the project
// object's second vtable (retail 0x803A08A4) holds its out-of-line copy.

/*
====================
EventProjectI::init
====================
*/
FMOD_RESULT EventProjectI::init() {
    m_name                     = NULL;
    m_hasmusicdata             = false;
    m_flags                    = 0;
    m_numevents                = 0;
    m_numeventgroups           = 0;
    m_eventtable               = NULL;
    m_instancepool             = NULL;
    m_sizeof_sounddef_pool     = 0;
    m_sounddef_pool            = NULL;
    m_soundbankmemory          = NULL;
    m_sizeof_soundbankmemory   = 0;
    m_eventgroupmemory         = NULL;
    m_sizeof_eventgroupmemory  = 0;
    m_sounddefmemory           = NULL;
    m_sizeof_sounddefmemory    = 0;
    m_sounddefdefmemory        = NULL;
    m_sizeof_sounddefdefmemory = 0;
    m_userdata                 = NULL;
    m_eventsystem              = NULL;
    m_id                       = 0;
    m_updatetimeparameter_head.initNode();

    return FMOD_OK;
} // 0x803A08E4

/*
====================
EventProjectI::createEventTable

Fills the project's table of events (indexed by project id): the project itself allocates it and
walks its groups, a group adds its events and then those of its subgroups.
====================
*/
FMOD_RESULT EventProjectI::createEventTable( unsigned int numevents, EventGroupI *group ) {
    FMOD_RESULT result;

    if ( !group ) {
        m_numevents = 0;

#line 518
        m_eventtable = (EventI **)FMOD_Memory_Calloc( numevents * sizeof( EventI * ) );
        if ( !m_eventtable ) {
            return FMOD_ERR_MEMORY;
        }

        for ( LinkedListNode *node = m_eventgrouphead.getNext(); node != &m_eventgrouphead; node = node->getNext() ) {
            result = createEventTable( 0, (EventGroupI *)node );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    } else {
        for ( LinkedListNode *eventnode = group->m_eventhead.getNext(); eventnode != &group->m_eventhead;
              eventnode = eventnode->getNext() ) {
            m_eventtable[m_numevents] = (EventI *)eventnode;
            m_eventtable[m_numevents]->m_data_parent->m_projectid = m_numevents;
            m_numevents++;
        }

        if ( group->m_eventgrouphead ) {
            for ( LinkedListNode *node = group->m_eventgrouphead->getNext(); node != group->m_eventgrouphead;
                  node = node->getNext() ) {
                result = createEventTable( 0, (EventGroupI *)node );
                if ( result != FMOD_OK ) {
                    return result;
                }
            }
        }
    }

    return FMOD_OK;
} // 0x803A0950

/*
====================
EventProjectI::createInstancePool

Builds the pool of numinstances prebuilt simple-event instances; a project has one at most.
====================
*/
FMOD_RESULT EventProjectI::createInstancePool( unsigned int numinstances ) {
    if ( m_instancepool ) {
        return FMOD_ERR_INITIALIZED;
    }

    if ( numinstances > EVENTPROJECTI_MAX_INSTANCES ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 571
    m_instancepool = new ( FMOD_Memory_Alloc( sizeof( EventInstancePool ) ) ) EventInstancePool;
    if ( !m_instancepool ) {
        return FMOD_ERR_MEMORY;
    }

    CHECK_RESULT( m_instancepool->init( numinstances ) );

    return FMOD_OK;
} // 0x803A0ABC

/*
====================
EventProjectI::release

Releases the project: its instance pool, groups, name and event table, its sound definitions
and banks (and the music data, when it carries any), then unlinks and frees it.
====================
*/
FMOD_RESULT EventProjectI::release() {
    FMOD_RESULT     result;
    LinkedListNode *node;
    LinkedListNode *next;

    if ( m_instancepool ) {
        result = m_instancepool->release();
        if ( result != FMOD_OK ) {
            return result;
        }
        m_instancepool = NULL;
    }

    for ( node = m_eventgrouphead.getNext(); node != &m_eventgrouphead; ) {
        EventGroupI *group = (EventGroupI *)node;

        node   = node->getNext();
        result = group->release( false );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    if ( m_name ) {
#line 629
        FMOD_Memory_Free( m_name );
    }

    if ( m_eventtable ) {
#line 634
        FMOD_Memory_Free( m_eventtable );
    }

    result = g_eventsystemi->releaseSoundDefs( this );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( m_hasmusicdata ) {
        result = g_eventsystemi->m_musicsystem->reset();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreLinkRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreCueRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreParameterRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreSceneRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreThemeRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreTimelineRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        result = CoreSegmentRepository::releaseRepository();
        if ( result != FMOD_OK ) {
            return result;
        }

        g_eventsystemi->m_hasmusicdata = false;
    }

    result = g_eventsystemi->releaseSoundBanks( this );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( m_soundbankmemory ) {
#line 711
        FMOD_Memory_Free( m_soundbankmemory );
    }

    if ( m_eventgroupmemory ) {
#line 716
        FMOD_Memory_Free( m_eventgroupmemory );
    }

    if ( m_sounddefmemory ) {
#line 721
        FMOD_Memory_Free( m_sounddefmemory );
    }

    if ( m_sounddefdefmemory ) {
#line 726
        FMOD_Memory_Free( m_sounddefdefmemory );
    }

    if ( m_sounddef_pool ) {
        m_sounddef_pool->release();
    }

    for ( node = m_eventsystem->m_reverbdefhead.getNext(); node != &m_eventsystem->m_reverbdefhead; node = next ) {
        ReverbDef *eventreverb = (ReverbDef *)node;

        next = node->getNext();
        if ( eventreverb->m_eventproject == this ) {
            eventreverb->release();
        }
    }

    g_eventsystemi->m_numloadedevents -= m_numevents;
    removeNode();

#line 749
    FMOD_Memory_Free( this );

    CHECK_RESULT( g_eventsystemi->rebuildEventTable() );

    return FMOD_OK;
} // 0x803A0B74

/*
====================
EventProjectI::getInfo

Reports the project's name and its index in the event system's project list.
====================
*/
FMOD_RESULT EventProjectI::getInfo( int *index, char **name ) {
    if ( name ) {
        *name = m_name;
    }

    if ( index ) {
        *index = g_eventsystemi->m_eventprojecthead.getNodeIndex( this );
    }

    return FMOD_OK;
} // 0x803A0EB0

/*
====================
EventProjectI::getGroup

Finds a top-level group by name; a name of the form "group/rest" descends into the group. A
leading slash is skipped.
====================
*/
FMOD_RESULT EventProjectI::getGroup( const char *name, bool cacheevents, EventGroup **group ) {
    FMOD_RESULT result;

    if ( !name || !group ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *group = NULL;

    const char *groupname = name;
    if ( *name == '/' ) {
        groupname = name + 1;
    }

    const char *p;
    for ( p = groupname; *p && *p != '/'; p++ ) {
    }

    for ( LinkedListNode *node = m_eventgrouphead.getNext(); node != &m_eventgrouphead; node = node->getNext() ) {
        if ( ( (EventGroupI *)node )->m_name && !FMOD_strnicmp( ( (EventGroupI *)node )->m_name, groupname, p - groupname ) &&
             !( (EventGroupI *)node )->m_name[p - groupname] ) {
            if ( *p ) {
                return ( (EventGroupI *)node )->getGroup( p + 1, cacheevents, group );
            }

            *group = (EventGroupI *)node;
            if ( cacheevents ) {
                result = ( (EventGroupI *)node )->createInstances( NULL );
                if ( result != FMOD_OK ) {
                    return result;
                }
            }
            return FMOD_OK;
        }
    }

    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x803A0F10

/*
====================
EventProjectI::getGroupByIndex
====================
*/
FMOD_RESULT EventProjectI::getGroupByIndex( int index, bool cacheevents, EventGroup **group ) {
    FMOD_RESULT result;

    if ( !group ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *group = NULL;

    if ( m_eventgrouphead.count() ) {
        EventGroupI *subgroup = (EventGroupI *)m_eventgrouphead.getNodeByIndex( index );

        *group = subgroup;
        if ( subgroup && cacheevents ) {
            result = subgroup->createInstances( NULL );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    if ( !*group ) {
        return FMOD_ERR_EVENT_NOTFOUND;
    }

    return FMOD_OK;
} // 0x803A1098

/*
====================
EventProjectI::getNumGroups
====================
*/
FMOD_RESULT EventProjectI::getNumGroups( int *numgroups ) {
    if ( !numgroups ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numgroups = m_eventgrouphead.count();

    return FMOD_OK;
} // 0x803A1188

/*
====================
EventProjectI::getEvent

Finds an event by its path in the project ("group/subgroup/event"): the group part is looked up
with getGroup, the event in that group.
====================
*/
FMOD_RESULT EventProjectI::getEvent( const char *name, unsigned int eventmode, Event **event ) {
    EventGroup *group;
    char        tmpname[EVENTPROJECTI_PATH_MAX];
    int         i;

    if ( !name || !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *event = NULL;

    int namelen = FMOD_strlen( name );
    if ( !name || namelen > EVENTPROJECTI_PATH_MAX - 1 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_strcpy( tmpname, name );

    for ( i = namelen - 1; i && tmpname[i] != '/'; i-- ) {
    }

    if ( !i ) {
        return FMOD_ERR_EVENT_NOTFOUND;
    }

    tmpname[i] = 0;
    char *eventname = &tmpname[i + 1];

    FMOD_RESULT result = getGroup( tmpname, false, &group );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = group->getEvent( eventname, eventmode, event );
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x803A11C4

/*
====================
EventProjectI::getEventByProjectID

Looks an event up in the project's event table by its project id.
====================
*/
FMOD_RESULT EventProjectI::getEventByProjectID( unsigned int projectid, unsigned int mode, Event **event ) {
    if ( !event || projectid >= m_numevents ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    EventI *e = m_eventtable[projectid];

    if ( mode & FMOD_EVENT_INFOONLY ) {
        return g_eventsystemi->createEventHandle( e, event, mode, false );
    }

    return e->m_data_base.m_eventgroupi->getEventInstance( e, event, mode );
} // 0x803A12F0

/*
====================
EventProjectI::getNumEvents
====================
*/
FMOD_RESULT EventProjectI::getNumEvents( int *numevents ) {
    if ( !numevents ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numevents = m_numevents;

    return FMOD_OK;
} // 0x803A134C

/*
====================
EventProjectI::addLSDGroup

Adds a group, its subgroups and their events to the arrays of a sample data request (LSD:
loadSampleData); a group already in the array is skipped.
====================
*/
FMOD_RESULT EventProjectI::addLSDGroup( EventGroupI **eventgrouparray, int &num_added_groups, EventGroupI *eventgroup,
                                        EventI **eventarray, int sizeof_eventarray, int &num_added_events ) {
    int groupindex;

    if ( eventgroup->m_eventgrouphead ) {
        for ( LinkedListNode *node = eventgroup->m_eventgrouphead->getNext(); node != eventgroup->m_eventgrouphead;
              node = node->getNext() ) {
            CHECK_RESULT( addLSDGroup( eventgrouparray, num_added_groups, (EventGroupI *)node, eventarray, sizeof_eventarray,
                                       num_added_events ) );
        }
    }

    for ( groupindex = 0; groupindex < num_added_groups; groupindex++ ) {
        if ( eventgrouparray[groupindex] == eventgroup ) {
            break;
        }
    }

    if ( groupindex == num_added_groups ) {
        eventgrouparray[num_added_groups++] = eventgroup;

        for ( LinkedListNode *node = eventgroup->m_eventhead.getNext(); node != &eventgroup->m_eventhead;
              node = node->getNext() ) {
            eventarray[num_added_events++] = (EventI *)node;
            if ( num_added_events >= sizeof_eventarray ) {
                return FMOD_ERR_MEMORY;
            }
        }
    }

    return FMOD_OK;
} // 0x803A136C

// loadSampleData leaves through these when a call fails: they free the temporary arrays it has made so
// far (and the bundle, once there is one). Names invented.
#define LSD_CHECK_GROUPS( _call )                       \
    {                                                   \
        FMOD_RESULT _result = ( _call );                \
        if ( _result != FMOD_OK ) {                     \
            FMOD_Memory_FreeNotNull( eventgrouparray ); \
            return _result;                             \
        }                                               \
    }

#define LSD_CHECK_BUNDLE( _call )                       \
    {                                                   \
        FMOD_RESULT _result = ( _call );                \
        if ( _result != FMOD_OK ) {                     \
            if ( bundle ) {                             \
                bundle->release();                      \
            }                                           \
            FMOD_Memory_FreeNotNull( eventarray );      \
            FMOD_Memory_FreeNotNull( eventgrouparray ); \
            return _result;                             \
        }                                               \
    }

/*
====================
EventProjectI::loadSampleData

Loads the sample data of the events and event groups named in the two arrays: gathers the groups
and the events whose sample banks are not loaded yet into a Bundle, then queues the loads and creates
the instances. Every failure frees the temporary arrays and releases the bundle.

Open (92%; wall: referent/call set, 4 inlined frees). Retail keeps the null-checked free of the event
array after Bundle::init (cmpwi r28 and a free call with a literal 0 argument, at the four exits after
the init), and sets r28 to 0 between the argument setup and the call; written as "eventarray = NULL"
after init (also through a temporary, and through a by-reference helper) the compiler folds those four
frees away. The bundle owns the array after init, so the honest body stops there.
====================
*/
FMOD_RESULT EventProjectI::loadSampleData( int *eventid_array, int sizeof_eventid_array, char **groupname_array,
                                           int sizeof_groupname_array, unsigned int eventmode ) {
    int           i, j;
    int           actualnumevents = 0;
    EventGroupI **eventgrouparray;
    int           num_added_groups;
    EventI      **eventarray;
    Bundle       *bundle;

    if ( ( sizeof_eventid_array && !eventid_array ) || ( sizeof_groupname_array && !groupname_array ) ||
         ( !sizeof_eventid_array && !sizeof_groupname_array ) ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( sizeof_eventid_array ) {
        for ( i = 0; i < sizeof_eventid_array; i++ ) {
            if ( eventid_array[i] >= (int)m_numevents ) {
                return FMOD_ERR_INVALID_PARAM;
            }
        }
    }

    if ( sizeof_groupname_array ) {
        for ( i = 0; i < sizeof_groupname_array; i++ ) {
            if ( !groupname_array[i] ) {
                return FMOD_ERR_INVALID_PARAM;
            }
        }
    }

#line 1139
    eventgrouparray = (EventGroupI **)FMOD_Memory_Calloc( m_numeventgroups * sizeof( EventGroupI * ) );
    if ( !eventgrouparray ) {
        return FMOD_ERR_MEMORY;
    }

    num_added_groups = 0;

    for ( i = 0; i < sizeof_groupname_array; i++ ) {
        EventGroupI *eg;

        LSD_CHECK_GROUPS( getGroup( groupname_array[i], false, (EventGroup **)&eg ) );
        // The system's scratch buffer (declared int*) holds the events collected here.
        LSD_CHECK_GROUPS( addLSDGroup( eventgrouparray, num_added_groups, eg, (EventI **)m_eventsystem->m_grouptempbuffer,
                                       m_eventsystem->m_grouptempbuffersize, actualnumevents ) );
    }

#line 1159
    eventarray = (EventI **)FMOD_Memory_Calloc( ( actualnumevents + sizeof_eventid_array ) * sizeof( EventI * ) );
    if ( !eventarray ) {
        FMOD_Memory_FreeNotNull( eventgrouparray );
        return FMOD_ERR_MEMORY;
    }

    memcpy( eventarray, m_eventsystem->m_grouptempbuffer, actualnumevents * sizeof( EventI * ) );

    for ( i = 0; i < sizeof_eventid_array; i++ ) {
        EventI *event = m_eventtable[eventid_array[i]];

        if ( !event->allBanksLoaded() ) {
            bool addtolist = false;

            for ( int numbanks = 0; numbanks < event->m_data_parent->m_numsoundbanks; numbanks++ ) {
                if ( event->m_data_parent->m_soundbank[numbanks]->isSample() && !event->getBankLoaded( numbanks ) ) {
                    addtolist = true;
                    break;
                }
            }

            if ( addtolist ) {
                for ( j = 0; j < num_added_groups; j++ ) {
                    if ( eventgrouparray[j] == event->m_data_base.m_eventgroupi ) {
                        break;
                    }
                }

                if ( j == num_added_groups ) {
                    eventarray[actualnumevents++] = event;
                }
            }
        }
    }

    if ( actualnumevents == 0 ) {
        FMOD_Memory_FreeNotNull( eventarray );
        FMOD_Memory_FreeNotNull( eventgrouparray );

        return FMOD_OK;
    }

#line 1221
    bundle = new ( FMOD_Memory_Alloc( sizeof( Bundle ) ) ) Bundle;
    if ( !bundle ) {
        FMOD_Memory_FreeNotNull( eventarray );
        FMOD_Memory_FreeNotNull( eventgrouparray );
        return FMOD_ERR_MEMORY;
    }

    // The bundle owns the event array from here on.
    {
        FMOD_RESULT _result = bundle->init( eventarray, actualnumevents );
        eventarray          = NULL;

        if ( _result != FMOD_OK ) {
            if ( bundle ) {
                bundle->release();
            }
            if ( eventarray ) {
                FMOD_Memory_FreeNotNull( eventarray );
            }
            if ( eventgrouparray ) {
                FMOD_Memory_FreeNotNull( eventgrouparray );
            }
            return _result;
        }
    }

    for ( i = 0; i < bundle->m_numevents; i++ ) {
        EventI *event = bundle->m_event[i];

        if ( !event->allBanksLoaded() ) {
            for ( int j = 0; j < event->m_data_parent->m_numsoundbanks; j++ ) {
                if ( event->m_data_parent->m_soundbank[j]->isSample() && !event->getBankLoaded( j ) ) {
                    event->setBankLoading( j, true );
                }
            }
        }
    }

    for ( i = 0; i < bundle->m_numeventgroups; i++ ) {
        bundle->m_eventgroup[i]->m_asyncresult = FMOD_OK;
    }

    for ( i = 0; i < bundle->m_numsoundbanks; i++ ) {
        if ( bundle->m_soundbank[i]->isSample() ) {
            LSD_CHECK_BUNDLE( bundle->m_soundbank[i]->createSampleInstance( eventmode, NULL, NULL, bundle ) );
        }
    }

    for ( i = 0; i < bundle->m_numevents; i++ ) {
        EventI *event = bundle->m_event[i];

        LSD_CHECK_BUNDLE( event->m_data_base.m_eventgroupi->createInstances( event ) );
    }

    for ( i = 0; i < bundle->m_numevents; i++ ) {
        EventI *event = bundle->m_event[i];

        if ( !event->allBanksLoaded() ) {
            for ( int j = 0; j < event->m_data_parent->m_numsoundbanks; j++ ) {
                if ( event->m_data_parent->m_soundbank[j]->isSample() && !event->getBankLoaded( j ) ) {
                    event->setBankLoaded( j, true );
                }
            }
        }
    }

    if ( bundle ) {
        bundle->release();
    }
    FMOD_Memory_FreeNotNull( eventarray );
    FMOD_Memory_FreeNotNull( eventgrouparray );

    return FMOD_OK;
} // 0x803A14A0

/*
====================
EventProjectI::setUserData
====================
*/
FMOD_RESULT EventProjectI::setUserData( void *userdata ) {
    m_userdata = userdata;

    return FMOD_OK;
} // 0x803A1CB0

/*
====================
EventProjectI::getUserData
====================
*/
FMOD_RESULT EventProjectI::getUserData( void **userdata ) {
    if ( !userdata ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *userdata = m_userdata;

    return FMOD_OK;
} // 0x803A1CBC

/*
====================
EventProjectI::stopAllEvents

Stops every instance of every event of the project.

Open (98.8%, 8 rows; wall: register/schedule). Retail holds the count in r31, the scaled index in
r30 and the instance array in r29 (i in r28, j in r27); with the array and count as locals the
scaled index takes r31, the array r30 and the count r29. Tried: no locals, parent only, parent with
the array and count locals in both orders, a const count, the locals at block scope in both orders,
the count in the for-j header, the index outside the loop (those fall to 97.9-98.3%).
====================
*/
FMOD_RESULT EventProjectI::stopAllEvents( bool immediate ) {
    EventI **instances;
    int      numinstances;

    for ( unsigned int i = 0; i < m_numevents; i++ ) {
        instances    = m_eventtable[i]->m_data_parent->m_instance;
        numinstances = m_eventtable[i]->m_data_parent->m_numinstances;

        for ( int j = 0; j < numinstances; j++ ) {
            if ( instances[j] ) {
                CHECK_RESULT( instances[j]->stop( immediate ) );
            }
        }
    }

    return FMOD_OK;
} // 0x803A1CDC

/*
====================
EventProjectI::getMemoryInfo
====================
*/
FMOD_RESULT EventProjectI::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                          unsigned int *memoryused_details ) {
    return FMOD_ERR_UNIMPLEMENTED;
} // 0x803A1D84

} // namespace FMOD
