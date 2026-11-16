/*
 * fmod_eventgroupi.cpp: FMOD::EventGroupI, a group of events in an event project: lookup by name and
 * index, loading and freeing event data, event instances and instance stealing. Allocation tags
 * carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)"): the #line
 * directives restore them.
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventgroupi.obj)

/*
 * Parked (NonMatching): 32 of 36 functions exact. Open (walls in each function's header):
 *   buildSoundBankList 99.3%, stealEventInstance 95.8%, getEventInstance 99.3%, load 98.5%.
 * The range after the unit, 0x80389F18-0x8038A228, is calculateSpeakerMix of fmod_eventi (Gormiti
 * map), not this unit.
 * Written members return the Gormiti DWARF's types and use LinkedListNode::count and
 * getNodeByIndex (names invented; FMOD inlines list helpers of this shape).
 */

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_eventgroupi.h>
#include <fmod/fmod_eventi.h>
#include <fmod/fmod_eventimpl.h>
#include <fmod/fmod_eventcategoryi.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_eventinstancepool.h>
#include <fmod/fmod_eventprojecti.h>
#include <fmod/fmod_eventsystemi.h>
#include <fmod/fmod_eventuserproperty.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_simplemempool.h>
#include <fmod/fmod_soundbank.h>
#include <fmod/fmod_string.h>
#include <new>
#include <stdio.h>
#include <string.h>

// The speaker levels of a 2D event that pans nothing: front left and right at full, the rest
// silent (load compares an event's levels with them). Unnamed in retail.
static const float lbl_803DE490[8] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

namespace FMOD {

// Returns from the enclosing function when a call fails.
#define CHECK_RESULT( _call )              \
    {                                      \
        FMOD_RESULT _result = ( _call );   \
        if ( _result != FMOD_OK ) {        \
            return _result;                \
        }                                  \
    }

/*
====================
EventGroupI::EventGroupI
====================
*/
EventGroupI::EventGroupI() {
    init();
} // 0x80386754

/*
====================
EventGroupI::init
====================
*/
FMOD_RESULT EventGroupI::init() {
    m_name                = NULL;
    m_index               = 0;
    m_flags               = 0;
    m_playcount           = 0;
    m_loading             = 0;
    m_asyncresult         = FMOD_OK;
    m_eventgrouphead      = NULL;
    m_parentgroup         = NULL;
    m_project             = NULL;
    m_eventgroupmemory    = NULL;
    m_numeventgroups      = 0;
    m_userdata            = NULL;
    m_numsoundbanks       = 0;
    m_soundbank           = NULL;
    m_bankentry_array     = NULL;

    return FMOD_OK;
} // 0x80386804

/*
====================
EventGroupI::getEventPtr

Finds an event by a name of the form "group/subgroup/event" below this group.
====================
*/
FMOD_RESULT EventGroupI::getEventPtr( const char *name, EventI **e, EventGroupI **g ) {
    EventGroupI *group     = this;
    char        *p         = (char *)name;
    char        *groupname = p;

    if ( e ) {
        *e = NULL;
    }
    if ( g ) {
        *g = NULL;
    }

    do {
        while ( *p && *p != '/' ) {
            p++;
        }

        if ( *p == '/' && group->m_eventgrouphead ) {
            bool found = false;

            for ( LinkedListNode *node = group->m_eventgrouphead->getNext(); node != group->m_eventgrouphead;
                  node = node->getNext() ) {
                if ( ( (EventGroupI *)node )->m_name &&
                     !FMOD_strnicmp( ( (EventGroupI *)node )->m_name, groupname, p - groupname ) &&
                     !( (EventGroupI *)node )->m_name[p - groupname] ) {
                    group = (EventGroupI *)node;
                    found = true;
                    break;
                }
            }

            if ( !found ) {
                return FMOD_ERR_EVENT_NOTFOUND;
            }

            groupname = p + 1;
        }
    } while ( *p++ == '/' );

    for ( LinkedListNode *node = group->m_eventhead.getNext(); node != &group->m_eventhead; node = node->getNext() ) {
        if ( ( (EventI *)node )->getName() && !FMOD_stricmp( ( (EventI *)node )->getName(), groupname ) ) {
            if ( e ) {
                *e = (EventI *)node;
            }
            if ( g ) {
                *g = group;
            }
            return FMOD_OK;
        }
    }

    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x8038684C

/*
====================
EventGroupI::addGroup

Links group in at the end of this group's subgroups, creating the list head on first use. A
named group must not share its name with a sibling.
====================
*/
FMOD_RESULT EventGroupI::addGroup( EventGroupI *group ) {
    if ( !m_eventgrouphead ) {
#line 237
        m_eventgrouphead = new ( FMOD_Memory_Calloc( sizeof( EventGroupI ) ) ) EventGroupI;
        if ( !m_eventgrouphead ) {
            return FMOD_ERR_MEMORY;
        }
    } else if ( group->m_name ) {
        for ( LinkedListNode *node = m_eventgrouphead->getNext(); node != m_eventgrouphead; node = node->getNext() ) {
            if ( ( (EventGroupI *)node )->m_name && !FMOD_stricmp( ( (EventGroupI *)node )->m_name, group->m_name ) ) {
                return FMOD_ERR_INVALID_PARAM;
            }
        }
    }

    group->addBefore( m_eventgrouphead );
    group->m_index = m_eventgrouphead->count() - 1;

    return FMOD_OK;
} // 0x80386A98

/*
====================
EventGroupI::buildSoundBankList

Collects the sound banks the group's events use, and for each bank the merged list of entries
(terminated by -1) the events need from it; then does the same for every subgroup.

Open (99.3%; wall: register/schedule). The entry arrays are sized by m_numsoundbanks, which
lets the compiler share numbanks * 4 across the three uses as retail does. The merge loop
colours its values differently (retail: entry pointer r3, event_node r7, eventbankindex r6).
Tried: the inner pointers declared in the other order, the increment in the indextomerge read
or in a for loop, the bank indices declared at the top.
====================
*/
FMOD_RESULT EventGroupI::buildSoundBankList() {
    FMOD_RESULT     result;
    SoundBank      *banklist[256];
    int             i, j, numbanks = 0;
    LinkedListNode *event_node;

    if ( m_soundbank ) {
#line 274
        FMOD_Memory_Free( m_soundbank );
        m_soundbank = NULL;
    }

    memset( banklist, 0, sizeof( banklist ) );

    for ( event_node = m_eventhead.getNext(); event_node != &m_eventhead; event_node = event_node->getNext() ) {
        EventI *event = (EventI *)event_node;

        result = event->m_impl->buildSoundBankList();
        if ( result != FMOD_OK ) {
            return result;
        }

        for ( i = 0; i < event->m_data_parent->m_numsoundbanks; i++ ) {
            bool found = false;

            for ( j = 0; j < 256; j++ ) {
                if ( banklist[j] == event->m_data_parent->m_soundbank[i] ) {
                    found = true;
                    break;
                } else if ( !banklist[j] ) {
                    banklist[j] = event->m_data_parent->m_soundbank[i];
                    numbanks++;
                    found = true;
                    break;
                }
            }

            if ( !found ) {
                return FMOD_ERR_MEMORY;
            }
        }
    }

#line 323
    m_soundbank = (SoundBank **)FMOD_Memory_Calloc( numbanks * sizeof( SoundBank * ) );
    if ( !m_soundbank ) {
        return FMOD_ERR_MEMORY;
    }

    memcpy( m_soundbank, banklist, numbanks * sizeof( SoundBank * ) );
    m_numsoundbanks = numbanks;

#line 334
    m_bankentry_array = (int **)FMOD_Memory_Calloc( m_numsoundbanks * sizeof( int * ) );
    if ( !m_bankentry_array ) {
        return FMOD_ERR_MEMORY;
    }

    int groupbankindex, eventbankindex;
    for ( groupbankindex = 0; groupbankindex < m_numsoundbanks; groupbankindex++ ) {
        int numgroupbankentry = 0;

        memset( g_eventsystemi->m_grouptempbuffer, -1, g_eventsystemi->m_grouptempbuffersize * sizeof( int ) );

        for ( event_node = m_eventhead.getNext(); event_node != &m_eventhead; event_node = event_node->getNext() ) {
            EventI *event = (EventI *)event_node;

            for ( eventbankindex = 0; eventbankindex < event->m_data_parent->m_numsoundbanks; eventbankindex++ ) {
                if ( event->m_data_parent->m_soundbank[eventbankindex] == m_soundbank[groupbankindex] ) {
                    int *eventbankentryarray = event->m_data_parent->m_bankentry_array[eventbankindex];
                    int *groupbankentryarray = g_eventsystemi->m_grouptempbuffer;

                    while ( *eventbankentryarray != -1 ) {
                        int indextomerge = *eventbankentryarray;

                        for ( j = 0; groupbankentryarray[j] != indextomerge && groupbankentryarray[j] != -1 &&
                                     j < g_eventsystemi->m_grouptempbuffersize;
                              j++ ) {
                        }
                        if ( j >= g_eventsystemi->m_grouptempbuffersize ) {
                            return FMOD_ERR_EVENT_INTERNAL;
                        }

                        if ( groupbankentryarray[j] == -1 ) {
                            groupbankentryarray[j] = indextomerge;
                            numgroupbankentry++;
                        }

                        eventbankentryarray++;
                    }
                    break;
                }
            }
        }

#line 386
        m_bankentry_array[groupbankindex] = (int *)FMOD_Memory_Alloc( ( numgroupbankentry + 1 ) * sizeof( int ) );
        memcpy( m_bankentry_array[groupbankindex], g_eventsystemi->m_grouptempbuffer,
                ( numgroupbankentry + 1 ) * sizeof( int ) );
    }

    if ( m_eventgrouphead ) {
        for ( LinkedListNode *node = m_eventgrouphead->getNext(); node != m_eventgrouphead; node = node->getNext() ) {
            result = ( (EventGroupI *)node )->buildSoundBankList();
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    return FMOD_OK;
} // 0x80386C3C

/*
====================
EventGroupI::loadEventData
====================
*/
FMOD_RESULT EventGroupI::loadEventData( FMOD_EVENT_RESOURCE resource, unsigned int mode ) {
    return loadEventDataInternal( resource, mode, NULL );
} // 0x80386FA8

/*
====================
EventGroupI::loadEventDataInternal

Creates the instances, streams and sample data of event, or of every event of the group and
its subgroups when event is NULL. Sample banks of single events are loaded at once; for the
whole group they are marked loading and created per group bank. The flags are read through
the const accessor getFlags: retail ORs the new bit first, which a direct "|=" does not give.
====================
*/
FMOD_RESULT EventGroupI::loadEventDataInternal( FMOD_EVENT_RESOURCE resource, unsigned int eventmode, EventI *event ) {
    LinkedListNode *node;
    bool            samplesRequested, streamsRequested;

    samplesRequested = ( resource == FMOD_EVENT_RESOURCE_STREAMS_AND_SAMPLES || resource == FMOD_EVENT_RESOURCE_SAMPLES ) ? true : false;
    streamsRequested = ( resource == FMOD_EVENT_RESOURCE_STREAMS_AND_SAMPLES || resource == FMOD_EVENT_RESOURCE_STREAMS );

    m_asyncresult = FMOD_OK;

    if ( event ) {
        EventI *parentevent = event->getOriginal() ? event->getOriginal() : event;

        CHECK_RESULT( createInstances( parentevent ) );

        if ( streamsRequested ) {
            CHECK_RESULT( event->createStreams( eventmode ) );
        }

        if ( !parentevent->allBanksLoaded() && samplesRequested ) {
            for ( int i = 0; i < parentevent->m_data_parent->m_numsoundbanks; i++ ) {
                if ( parentevent->m_data_parent->m_soundbank[i]->isSample() && !parentevent->getBankLoaded( i ) ) {
                    parentevent->setBankLoading( i, true );

                    CHECK_RESULT( parentevent->m_data_parent->m_soundbank[i]->createSampleInstance(
                        eventmode, parentevent, parentevent->m_data_base.m_eventgroupi, NULL ) );

                    parentevent->setBankLoaded( i, true );
                }
            }
        }
    } else {
        for ( node = m_eventhead.getNext(); node != &m_eventhead; node = node->getNext() ) {
            EventI *event = (EventI *)node;

            if ( !event->allBanksLoaded() && samplesRequested ) {
                for ( int i = 0; i < event->m_data_parent->m_numsoundbanks; i++ ) {
                    if ( event->m_data_parent->m_soundbank[i]->isSample() && !event->getBankLoaded( i ) ) {
                        event->setBankLoading( i, true );
                    }
                }
            }
        }

        if ( samplesRequested ) {
            for ( int i = 0; i < m_numsoundbanks; i++ ) {
                if ( m_soundbank[i]->isSample() ) {
                    CHECK_RESULT( m_soundbank[i]->createSampleInstance( eventmode, NULL, this, NULL ) );
                }
            }
        }

        for ( node = m_eventhead.getNext(); node != &m_eventhead; node = node->getNext() ) {
            EventI *event = (EventI *)node;

            event->setFlags( event->getFlags() | ( ( eventmode & FMOD_EVENT_USERDSP ) ? EVENTI_FLAG_USERDSP : 0 ) );

            CHECK_RESULT( createInstances( event ) );

            if ( streamsRequested && !event->useInstancePool() ) {
                CHECK_RESULT( event->createStreams( eventmode ) );
            }

            if ( !event->allBanksLoaded() && samplesRequested ) {
                for ( int i = 0; i < event->m_data_parent->m_numsoundbanks; i++ ) {
                    if ( event->m_data_parent->m_soundbank[i]->isSample() && !event->getBankLoaded( i ) ) {
                        event->setBankLoaded( i, true );
                    }
                }
            }
        }

        if ( m_eventgrouphead ) {
            for ( node = m_eventgrouphead->getNext(); node != m_eventgrouphead; node = node->getNext() ) {
                CHECK_RESULT( ( (EventGroupI *)node )->loadEventData( resource, eventmode ) );
            }
        }
    }

    return FMOD_OK;
} // 0x80386FB0

/*
====================
EventGroupI::adjustLoadCount

Adds count to the loading count of this group and of every group above it.
====================
*/
FMOD_RESULT EventGroupI::adjustLoadCount( int count ) {
    m_loading += count;

    for ( EventGroupI *parent = m_parentgroup; parent; parent = parent->m_parentgroup ) {
        parent->m_loading += count;
    }

    return FMOD_OK;
} // 0x80387390

/*
====================
EventGroupI::freeEventData
====================
*/
FMOD_RESULT EventGroupI::freeEventData( Event *event, bool waituntilready ) {
    FMOD_RESULT result;
    EventI     *eventi = NULL;

    if ( event ) {
        result = EventI::validate( event, &eventi );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    if ( eventi && ( eventi->m_data_base.m_flags & EVENTI_FLAG_0001 ) ) {
        return FMOD_ERR_EVENT_INTERNAL;
    }

    return freeEventDataInternal( eventi, waituntilready, false );
} // 0x803873C4

/*
====================
EventGroupI::freeEventDataInternal

Frees the instances and sample data of eventi (only its own instance when singleinstance is
set), or of every event of this group and its subgroups when eventi is NULL. Without
waituntilready it refuses while sound bank loads are still queued.
====================
*/
FMOD_RESULT EventGroupI::freeEventDataInternal( EventI *eventi, bool waituntilready, bool singleinstance ) {
    if ( waituntilready ) {
        SoundBank::flushLoadQueue();
    } else {
        FMOD_OS_CriticalSection_Enter( SoundBank::gSoundbankQueueCrit );
        if ( !SoundBank::gSoundBankQueueUsedHead.isEmpty() ) {
            FMOD_OS_CriticalSection_Leave( SoundBank::gSoundbankQueueCrit );
            return FMOD_ERR_NOTREADY;
        }
        FMOD_OS_CriticalSection_Leave( SoundBank::gSoundbankQueueCrit );
    }

    if ( eventi ) {
        EventI *parenteventi = eventi->getOriginal() ? eventi->getOriginal() : eventi;

        parenteventi->m_data_base.m_asyncresult = FMOD_OK;

        if ( m_eventhead.contains( parenteventi ) ) {
            CHECK_RESULT( freeInstanceData( singleinstance ? eventi : parenteventi, waituntilready ) );
            CHECK_RESULT( freeSampleData( parenteventi ) );
        }
    } else {
        for ( LinkedListNode *node = m_eventhead.getNext(); node != &m_eventhead; node = node->getNext() ) {
            EventI *parenteventi = (EventI *)node;

            parenteventi->m_data_base.m_asyncresult = FMOD_OK;

            CHECK_RESULT( freeInstanceData( parenteventi, waituntilready ) );
            CHECK_RESULT( freeSampleData( parenteventi ) );
        }

        m_loading = 0;

        if ( m_eventgrouphead ) {
            for ( LinkedListNode *node = m_eventgrouphead->getNext(); node != m_eventgrouphead;
                  node = node->getNext() ) {
                CHECK_RESULT( ( (EventGroupI *)node )->freeEventDataInternal( NULL, true, false ) );
            }
        }
    }

    return FMOD_OK;
} // 0x80387450

/*
====================
EventGroupI::freeInstanceData

Releases the instances of event: its pool instance, or every instance of its parent.
====================
*/
FMOD_RESULT EventGroupI::freeInstanceData( EventI *event, bool waituntilready ) {
    if ( event->useInstancePool() ) {
        if ( m_project->m_instancepool ) {
            CHECK_RESULT( m_project->m_instancepool->releaseEventInstance( event, waituntilready ) );
        }
    } else {
        EventI *parentevent = event;
        if ( event->getOriginal() ) {
            parentevent = event->getOriginal();
        }

        for ( int i = 0; i < parentevent->m_data_parent->m_numinstances; i++ ) {
            if ( parentevent->m_data_parent->m_instance[i] ) {
                CHECK_RESULT( parentevent->m_data_parent->m_instance[i]->release( waituntilready, true ) );
                parentevent->m_data_parent->m_instance[i] = NULL;
            }
        }
    }

    return FMOD_OK;
} // 0x80387664

/*
====================
EventGroupI::freeSampleData

Releases the sample sound banks of event that no instance is using.
====================
*/
FMOD_RESULT EventGroupI::freeSampleData( EventI *event ) {
    if ( event->anyBanksLoaded() ) {
        for ( int i = 0; i < event->m_data_parent->m_numsoundbanks; i++ ) {
            if ( event->m_data_parent->m_soundbank[i]->isSample() && event->getBankLoaded( i ) &&
                 !event->getNumInstancesInUse() ) {
                CHECK_RESULT( event->m_data_parent->m_soundbank[i]->decRefcnt( event ) );
                CHECK_RESULT( event->m_data_parent->m_soundbank[i]->releaseSampleInstance( event, true ) );
                event->setBankLoaded( i, false );
            }
        }
    }

    return FMOD_OK;
} // 0x80387760

/*
====================
EventGroupI::release

Frees the group's event data, releases its events, subgroups and user properties, frees its
memory and unlinks it; freethis also frees the group object itself. The loops keep the next
node in a local that Gormiti's DWARF does not list; its own code has the same shape.
====================
*/
FMOD_RESULT EventGroupI::release( bool freethis ) {
    LinkedListNode *node;
    FMOD_RESULT     result;

    result = freeEventDataInternal( NULL, true, false );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( node = m_eventhead.getNext(); node != &m_eventhead; ) {
        EventI         *eventi = (EventI *)node;
        LinkedListNode *next   = node->getNext();

        if ( !eventi->useInstancePool() ) {
            for ( int i = 0; i < eventi->m_data_parent->m_numinstances; i++ ) {
                if ( eventi->m_data_parent->m_instance[i] ) {
                    result = eventi->m_data_parent->m_instance[i]->release( true, true );
                    if ( result != FMOD_OK ) {
                        return result;
                    }
                    eventi->m_data_parent->m_instance[i] = NULL;
                }
            }
        }

        result = eventi->release( true, true );
        if ( result != FMOD_OK ) {
            return result;
        }

        node = next;
    }

    if ( m_eventgrouphead ) {
        node = m_eventgrouphead->getNext();
        while ( node != m_eventgrouphead ) {
            EventGroupI *eventgroupi = (EventGroupI *)node;

            node = node->getNext();

            result = eventgroupi->release( freethis );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    if ( m_eventgrouphead ) {
        m_eventgrouphead->release( true );
        m_eventgrouphead = NULL;
    }

    for ( node = m_userpropertyhead.getNext(); node != &m_userpropertyhead; ) {
        UserProperty   *property = (UserProperty *)node;
        LinkedListNode *next     = node->getNext();

        property->removeNode();
        property->release();

        node = next;
    }

    if ( m_name ) {
#line 826
        FMOD_Memory_Free( m_name );
    }
    if ( m_soundbank ) {
#line 831
        FMOD_Memory_Free( m_soundbank );
    }
    if ( m_bankentry_array ) {
        for ( int i = 0; i < m_numsoundbanks; i++ ) {
            if ( m_bankentry_array[i] ) {
#line 840
                FMOD_Memory_Free( m_bankentry_array[i] );
            }
        }
#line 844
        FMOD_Memory_Free( m_bankentry_array );
    }

    removeNode();

    if ( m_eventgroupmemory ) {
#line 851
        FMOD_Memory_Free( m_eventgroupmemory );
    }
    if ( freethis ) {
#line 856
        FMOD_Memory_Free( this );
    }

    return FMOD_OK;
} // 0x8038785C

/*
====================
EventGroupI::getInfo
====================
*/
FMOD_RESULT EventGroupI::getInfo( int *index, char **name ) {
    if ( name ) {
        *name = m_name;
    }
    if ( index ) {
        *index = m_index;
    }

    return FMOD_OK;
} // 0x80387B38

/*
====================
EventGroupI::getGroup

Finds a subgroup by name; a name of the form "group/rest" descends into the subgroup.
====================
*/
FMOD_RESULT EventGroupI::getGroup( const char *name, bool cacheevents, EventGroup **group ) {
    FMOD_RESULT result;

    if ( !name || !group || !m_eventgrouphead ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *group = NULL;

    const char *p;
    for ( p = name; *p && *p != '/'; p++ ) {
    }

    for ( LinkedListNode *node = m_eventgrouphead->getNext(); node != m_eventgrouphead; node = node->getNext() ) {
        if ( ( (EventGroupI *)node )->m_name && !FMOD_strnicmp( ( (EventGroupI *)node )->m_name, name, p - name ) &&
             !( (EventGroupI *)node )->m_name[p - name] ) {
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

    return FMOD_ERR_INVALID_PARAM;
} // 0x80387B60

/*
====================
EventGroupI::getGroupByIndex
====================
*/
FMOD_RESULT EventGroupI::getGroupByIndex( int index, bool cacheevents, EventGroup **group ) {
    FMOD_RESULT result;

    if ( !group ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *group = NULL;

    if ( m_eventgrouphead && m_eventgrouphead->count() ) {
        EventGroupI *subgroup = (EventGroupI *)m_eventgrouphead->getNodeByIndex( index );

        *group = subgroup;
        if ( subgroup && cacheevents ) {
            result = subgroup->createInstances( NULL );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    if ( !*group ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
} // 0x80387CF8

/*
====================
EventGroupI::getParentGroup
====================
*/
FMOD_RESULT EventGroupI::getParentGroup( EventGroup **group ) {
    if ( !group ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *group = m_parentgroup;

    return FMOD_OK;
} // 0x80387DEC

/*
====================
EventGroupI::getParentProject
====================
*/
FMOD_RESULT EventGroupI::getParentProject( EventProject **project ) {
    if ( !project ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *project = m_project;

    return FMOD_OK;
} // 0x80387E0C

/*
====================
EventGroupI::getNumGroups
====================
*/
FMOD_RESULT EventGroupI::getNumGroups( int *numgroups ) {
    if ( !numgroups ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    int count;
    if ( m_eventgrouphead ) {
        count = m_eventgrouphead->count();
    } else {
        count = 0;
    }

    *numgroups = count;

    return FMOD_OK;
} // 0x80387E2C

/*
====================
EventGroupI::getEvent

Finds an event by name (a path through subgroups) and hands out an instance of it, or only its
handle when the mode asks for info.
====================
*/
FMOD_RESULT EventGroupI::getEvent( const char *name, unsigned int mode, Event **event ) {
    FMOD_RESULT  result;
    EventI      *eventi;
    EventGroupI *eventgroupi;

    if ( !name || !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *event = NULL;

    result = getEventPtr( name, &eventi, &eventgroupi );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mode & FMOD_EVENT_INFOONLY ) {
        g_eventsystemi->createEventHandle( eventi, event, mode, false );
        return FMOD_OK;
    }

    return eventgroupi->getEventInstance( eventi, event, mode );
} // 0x80387E78

/*
====================
EventGroupI::getEventByIndex
====================
*/
FMOD_RESULT EventGroupI::getEventByIndex( int index, unsigned int mode, Event **event ) {
    FMOD_RESULT result;

    if ( !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *event = NULL;

    EventI *eventi = (EventI *)m_eventhead.getNodeByIndex( index );
    if ( eventi ) {
        if ( mode & FMOD_EVENT_INFOONLY ) {
            result = g_eventsystemi->createEventHandle( eventi, event, mode, false );
            if ( result != FMOD_OK ) {
                return result;
            }
            return FMOD_OK;
        }

        return getEventInstance( eventi, event, mode );
    }

    return FMOD_ERR_INVALID_PARAM;
} // 0x80387F20

/*
====================
EventGroupI::getNumEvents
====================
*/
FMOD_RESULT EventGroupI::getNumEvents( int *numevents ) {
    if ( !numevents ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numevents = m_eventhead.count();

    return FMOD_OK;
} // 0x80387FEC

/*
====================
EventGroupI::getProperty

Copies the value of the user property with that name (case-insensitive) to value.
====================
*/
FMOD_RESULT EventGroupI::getProperty( const char *propertyname, void *value ) {
    if ( !value ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    for ( LinkedListNode *node = m_userpropertyhead.getNext(); node != &m_userpropertyhead; node = node->getNext() ) {
        UserProperty *property = (UserProperty *)node;

        if ( !FMOD_stricmp( property->m_name, propertyname ) ) {
            switch ( property->m_type ) {
                case FMOD_EVENTPROPERTY_TYPE_INT:
                    *(int *)value = (int)property->m_value;
                    break;
                case FMOD_EVENTPROPERTY_TYPE_FLOAT:
                    *(float *)value = *(float *)&property->m_value;
                    break;
                case FMOD_EVENTPROPERTY_TYPE_STRING:
                    *(void **)value = property->m_value;
                    break;
            }
            return FMOD_OK;
        }
    }

    return FMOD_ERR_INVALID_PARAM;
} // 0x80388028

/*
====================
EventGroupI::getPropertyByIndex
====================
*/
FMOD_RESULT EventGroupI::getPropertyByIndex( int propertyindex, void *value ) {
    if ( propertyindex < 0 || !value ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    UserProperty *property = (UserProperty *)m_userpropertyhead.getNodeByIndex( propertyindex );
    if ( property ) {
        switch ( property->m_type ) {
            case FMOD_EVENTPROPERTY_TYPE_INT:
                *(int *)value = (int)property->m_value;
                break;
            case FMOD_EVENTPROPERTY_TYPE_FLOAT:
                *(float *)value = *(float *)&property->m_value;
                break;
            case FMOD_EVENTPROPERTY_TYPE_STRING:
                *(void **)value = property->m_value;
                break;
        }
    } else {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
} // 0x803880F4

/*
====================
EventGroupI::getNumProperties
====================
*/
FMOD_RESULT EventGroupI::getNumProperties( int *numproperties ) {
    if ( !numproperties ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numproperties = m_userpropertyhead.count();

    return FMOD_OK;
} // 0x803881A0

/*
====================
EventGroupI::duplicateEvent

Copies srcevent into a new instance, with a memory pool of its own when the parent event asks for one.
====================
*/
FMOD_RESULT EventGroupI::duplicateEvent( EventI *srcevent, EventI **dst ) {
    FMOD_RESULT    result;
    SimpleMemPool *mempool;
    EventI        *dstevent;

    if ( !dst ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mempool = NULL;

    if ( srcevent->getOriginal() ? srcevent->getOriginal()->m_data_parent->m_mempoolsize
                                 : srcevent->m_data_parent->m_mempoolsize ) {
#line 1330
        mempool = new ( FMOD_Memory_Alloc( sizeof( SimpleMemPool ) ) ) SimpleMemPool;
        if ( !mempool ) {
            return FMOD_ERR_MEMORY;
        }

        if ( !mempool->init( srcevent->getOriginal() ? srcevent->getOriginal()->m_data_parent->m_mempoolsize
                                                      : srcevent->m_data_parent->m_mempoolsize,
                             NULL ) ) {
            return FMOD_ERR_MEMORY;
        }
    }

    result = g_eventsystemi->buildEventI( ~EVENTI_DATA_PARENT, srcevent->m_data_base.m_exflags & 0x18, mempool, &dstevent );
    if ( result != FMOD_OK ) {
        return result;
    }

    dstevent->m_data_instance->m_mempool = mempool;

    result = srcevent->m_impl->duplicateEvent( dstevent, mempool );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = srcevent->copyProperties( dstevent );
    if ( result != FMOD_OK ) {
        return result;
    }

    *dst = dstevent;

    return FMOD_OK;
} // 0x803881DC

/*
====================
EventGroupI::createInstances

Builds the instances of event, or of every event of the group when event is NULL. An event that
comes from the instance pool has none of its own.
====================
*/
FMOD_RESULT EventGroupI::createInstances( EventI *event ) {
    FMOD_RESULT result;

    if ( !event ) {
        for ( LinkedListNode *node = m_eventhead.getNext(); node != &m_eventhead; node = node->getNext() ) {
            result = createInstances( (EventI *)node );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    } else {
        if ( event->useInstancePool() ) {
            return FMOD_OK;
        }

        for ( int i = 0; i < event->m_data_parent->m_numinstances; i++ ) {
            if ( !event->m_data_parent->m_instance[i] ) {
                result = duplicateEvent( event, &event->m_data_parent->m_instance[i] );
                if ( result != FMOD_OK ) {
                    return result;
                }

                result = event->m_data_parent->m_instance[i]->createDSPNetwork();
                if ( result != FMOD_OK ) {
                    return result;
                }
            }
        }
    }

    return FMOD_OK;
} // 0x803883AC

/*
====================
EventGroupI::stealEventInstance

Chooses the playing instance of srcevent (from the project's instance pool when pooled) that a
new play takes over, by the event's max playbacks behaviour: the newest, the quietest (the
oldest when all are equally loud), none unless the new one is louder, or the oldest. A pooled
instance is only taken from an event of no higher steal priority.

Open (95.8%; wall: struct temporary and register order). Retail copies m_initialdspclock to one
stack temporary per read; getStartClock (by value) gives two, and only a same-type functional
cast FMOD_UINT64P( ... ).mHi gives one (rejected as a carrier, 97.9%). The instance pointer
is coloured like a compiler temporary (r7 after the r3 offset), which the named local
`instance` changes; Gormiti's DWARF lists neither `instance` nor `pool`.
====================
*/
FMOD_RESULT EventGroupI::stealEventInstance( EventI *srcevent, EventI **event, bool pooled ) {
    int                count, index = -1;
    EventInstancePool *pool         = pooled ? m_project->m_instancepool : NULL;
    int                numinstances = pooled ? pool->m_numinstances : srcevent->m_data_parent->m_numinstances;

    if ( !pooled && srcevent->m_data_parent->m_numinstances == 1 &&
         !( srcevent->m_data_base.m_flags & EVENTI_FLAG_JUST_FAIL ) &&
         !( srcevent->m_data_base.m_flags & EVENTI_FLAG_JUST_FAIL_IF_QUIETEST ) ) {
        index = 0;
    }

    if ( index == -1 ) {
        if ( srcevent->m_data_base.m_flags & EVENTI_FLAG_JUST_FAIL ) {
            *event = NULL;
            return FMOD_ERR_EVENT_FAILED;
        } else if ( srcevent->m_data_base.m_flags & EVENTI_FLAG_STEAL_NEWEST ) {
            unsigned int newest = 0;

            for ( count = 0; count < numinstances; count++ ) {
                EventI *instance = pooled ? pool->m_instance[count] : srcevent->m_data_parent->m_instance[count];

                if ( instance->getStartClock().mHi > newest ) {
                    if ( !pooled || srcevent->m_data_base.m_stealpriority >= instance->m_data_base.m_stealpriority ) {
                        index  = count;
                        newest = instance->getStartClock().mHi;
                    }
                }
            }
        } else if ( srcevent->m_data_base.m_flags & EVENTI_FLAG_STEAL_QUIETEST ) {
            float quietest    = 9999999.0f;
            bool  allthesame  = true;
            float firstvolume = 0.0f;

            for ( count = 0; count < numinstances; count++ ) {
                float   volume;
                EventI *instance = pooled ? pool->m_instance[count] : srcevent->m_data_parent->m_instance[count];

                CHECK_RESULT( instance->estimateAudibility( &volume ) );

                if ( volume < quietest ) {
                    if ( !pooled || srcevent->m_data_base.m_stealpriority >= instance->m_data_base.m_stealpriority ) {
                        quietest = volume;
                        index    = count;
                    }
                }

                if ( count == 0 ) {
                    firstvolume = volume;
                } else if ( volume != firstvolume ) {
                    allthesame = false;
                }
            }

            if ( allthesame ) {
                unsigned int oldest = (unsigned int)-1;

                for ( count = 0; count < numinstances; count++ ) {
                    EventI *instance = pooled ? pool->m_instance[count] : srcevent->m_data_parent->m_instance[count];

                    if ( instance->getStartClock().mHi < oldest ) {
                        if ( !pooled || srcevent->m_data_base.m_stealpriority >= instance->m_data_base.m_stealpriority ) {
                            index  = count;
                            oldest = instance->getStartClock().mHi;
                        }
                    }
                }
            }
        } else if ( srcevent->m_data_base.m_flags & EVENTI_FLAG_JUST_FAIL_IF_QUIETEST ) {
            float volume;
            float quietest = 9999999.0f;

            for ( count = 0; count < numinstances; count++ ) {
                EventI *instance = pooled ? pool->m_instance[count] : srcevent->m_data_parent->m_instance[count];

                CHECK_RESULT( instance->estimateAudibility( &volume ) );

                if ( volume < quietest ) {
                    if ( !pooled || srcevent->m_data_base.m_stealpriority >= instance->m_data_base.m_stealpriority ) {
                        quietest = volume;
                        index    = count;
                    }
                }
            }

            CHECK_RESULT( ( srcevent->getOriginal() ? srcevent->getOriginal() : srcevent )->estimateAudibility( &volume ) );

            if ( volume <= quietest ) {
                *event = NULL;
                return FMOD_ERR_EVENT_FAILED;
            }
        } else {
            unsigned int oldest = (unsigned int)-1;

            for ( count = 0; count < numinstances; count++ ) {
                EventI *instance = pooled ? pool->m_instance[count] : srcevent->m_data_parent->m_instance[count];

                if ( instance->getStartClock().mHi < oldest ) {
                    if ( !pooled || srcevent->m_data_base.m_stealpriority >= instance->m_data_base.m_stealpriority ) {
                        index  = count;
                        oldest = instance->getStartClock().mHi;
                    }
                }
            }
        }
    }

    if ( index == -1 ) {
        *event = NULL;
        return FMOD_ERR_EVENT_FAILED;
    }

    *event = pooled ? pool->m_instance[index] : srcevent->m_data_parent->m_instance[index];

    return FMOD_OK;
} // 0x803884B4

/*
====================
EventGroupI::getEventInstance

Hands out an instance of srcevent: from the project's instance pool, else the next of the
parent's instances that is not playing, else one stolen from a playing instance. A stolen
instance gets the stolen callback and is stopped.

Open (99.3%; wall: register/schedule). Retail keeps the chosen instance in r4 for the callback
test and reloads it for the call; here the test's load is reused for the call. Tried: the test
nested, through a const accessor, the arms as a ternary or through const accessors, a named
result for the call. The flags are read through getFlags, as in loadEventDataInternal.
====================
*/
FMOD_RESULT EventGroupI::getEventInstance( EventI *srcevent, Event **dstevent, unsigned int eventmode ) {
    FMOD_RESULT result;
    EventI     *event = NULL;
    int         index = 0, count;
    bool        pooled = false;

    if ( !srcevent || !dstevent ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    srcevent->setFlags( srcevent->getFlags() | ( ( eventmode & FMOD_EVENT_USERDSP ) ? EVENTI_FLAG_USERDSP : 0 ) );

    if ( srcevent->useInstancePool() ) {
        CHECK_RESULT( m_project->m_instancepool->getEventInstance( srcevent, &event ) );
        pooled = true;
    } else {
        result = createInstances( srcevent );
        if ( result != FMOD_OK ) {
            return result;
        }

        index = srcevent->m_data_parent->m_lastinstanceindex;
        for ( count = 0; count < srcevent->m_data_parent->m_numinstances; count++ ) {
            unsigned int openstate;

            index++;
            if ( index >= srcevent->m_data_parent->m_numinstances ) {
                index = 0;
            }

            result = srcevent->m_data_parent->m_instance[index]->getState( &openstate );
            if ( result != FMOD_OK && result != FMOD_ERR_FILE_UNWANTED ) {
                return result;
            }

            if ( !( openstate & FMOD_EVENT_STATE_PLAYING ) ) {
                event = srcevent->m_data_parent->m_instance[index];
                break;
            }
        }

        if ( !event ) {
            CHECK_RESULT( stealEventInstance( srcevent, &event, false ) );
        }
    }

    bool stolen;
    if ( pooled ) {
        stolen = ( event->m_data_base.m_exflags & EVENTI_EXFLAG_INUSE ) ? true : false;
    } else {
        stolen = event->m_data_base.m_usereventhandle != 0;
    }

    if ( stolen && event->m_data_base.m_eventcallback ) {
        CHECK_RESULT( event->callEventCallback( FMOD_EVENT_CALLBACKTYPE_STOLEN, NULL, NULL ) );
    }

    srcevent->m_data_parent->m_lastinstanceindex = index;

    if ( !pooled || stolen ) {
        CHECK_RESULT( event->stop( true ) );
    }

    if ( pooled ) {
        CHECK_RESULT( m_project->m_instancepool->copyEventInstance( srcevent, event ) );
    }

    // The DSP network goes to the parent's instance at index, also for a pooled event.
    if ( ( srcevent->m_data_base.m_flags & EVENTI_FLAG_USERDSP ) &&
         !( event->m_data_base.m_flags & EVENTI_FLAG_USERDSP ) ) {
        CHECK_RESULT( srcevent->m_data_parent->m_instance[index]->getInstanceChannelGroup()->release() );
        srcevent->m_data_parent->m_instance[index]->m_data_instance->m_channelgroup = NULL;
        srcevent->m_data_parent->m_instance[index]->m_data_base.m_flags |= EVENTI_FLAG_USERDSP;
        CHECK_RESULT( srcevent->m_data_parent->m_instance[index]->createDSPNetwork() );
    }

    CHECK_RESULT( loadEventDataInternal( FMOD_EVENT_RESOURCE_STREAMS_AND_SAMPLES, eventmode, event ) );
    CHECK_RESULT( event->setupInstance() );
    CHECK_RESULT( g_eventsystemi->createEventHandle( event, dstevent, eventmode, pooled ) );

    return FMOD_OK;
} // 0x80388940

/*
====================
EventGroupI::addEvent

Links an event at the end of the group's event list. An event whose name the group already holds
is refused (inline: retail has no copy of it).
====================
*/
inline FMOD_RESULT EventGroupI::addEvent( EventI *eventi ) {
    EventI      *existing;
    EventGroupI *group;

    if ( eventi->getName() && getEventPtr( eventi->getName(), &existing, &group ) == FMOD_OK && group == this ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    eventi->addBefore( &m_eventhead );
    return FMOD_OK;
}

/*
====================
EventGroupI::load

event's properties are read field by field; fileversion gates the fields later file versions added.

Open (98.5%; wall: register/schedule, no call-set or CFG difference). Left: the subgroup loop
colours its pointer, the new group and the count differently (r22/r20/r21 here, r21/r22/r20 in
retail; new ( eventgroupmemory_ptr++ ), a separate placement new and an indexed address were
tried); getName's chain loads the event into another register; the three category-list
insertions compute the list head before reading its prev link (retail reads it at
categoryi + 0x34); the speaker-level address adds in the other order; the error epilogue
schedules its constants differently. Gormiti's copy is another revision (0x148 bytes longer).
event's properties are read field by field; fileversion gates the fields later file versions added.
====================
*/
FMOD_RESULT EventGroupI::load( File *file, unsigned int fileversion, FMOD_EVENT_LOADINFO *loadinfo ) {
    FMOD_RESULT  result;
    unsigned int num_eventgroups, num_events, num_properties, eventgroup_count, event_count, property_count, name_len, tmpuint;
    float        tmpfloat;
    EventGroupI *eventgroupmemory_ptr;

    CHECK_RESULT( file->read( &name_len, 4, 1, NULL ) );
    if ( g_eventsystemi->m_initflags & FMOD_EVENT_INIT_DONTUSENAMES ) {
        CHECK_RESULT( file->seek( name_len, SEEK_CUR ) );
        m_name = NULL;
    } else {
#line 1783
        m_name = (char *)FMOD_Memory_Calloc( name_len );
        if ( !m_name ) {
            return FMOD_ERR_MEMORY;
        }
        CHECK_RESULT( file->read( m_name, 1, name_len, NULL ) );
    }

    if ( fileversion >= 0x00170000 ) {
        CHECK_RESULT( file->read( &num_properties, 4, 1, NULL ) );
        for ( property_count = 0; property_count < num_properties; property_count++ ) {
#line 1797
            UserProperty *userproperty = new ( FMOD_Memory_Alloc( sizeof( UserProperty ) ) ) UserProperty;
            if ( !userproperty ) {
                return FMOD_ERR_MEMORY;
            }
            CHECK_RESULT( userproperty->load( file, fileversion ) );
            userproperty->addBefore( &m_userpropertyhead );
        }
    }

    CHECK_RESULT( file->read( &num_eventgroups, 4, 1, NULL ) );
    CHECK_RESULT( file->read( &num_events, 4, 1, NULL ) );

    if ( num_eventgroups ) {
        m_numeventgroups = num_eventgroups;
#line 1815
        m_eventgroupmemory = (EventGroupI *)FMOD_Memory_Alloc( num_eventgroups * sizeof( EventGroupI ) );
        if ( !m_eventgroupmemory ) {
            return FMOD_ERR_MEMORY;
        }
    }

    eventgroupmemory_ptr = m_eventgroupmemory;
    for ( eventgroup_count = 0; eventgroup_count < num_eventgroups; eventgroup_count++ ) {
        EventGroupI *eventgroupi = new ( eventgroupmemory_ptr ) EventGroupI;
        eventgroupmemory_ptr++;

        eventgroupi->init();
        eventgroupi->m_project = m_project;
        eventgroupi->m_project->m_numeventgroups++;
        eventgroupi->m_parentgroup = this;

        CHECK_RESULT( eventgroupi->load( file, fileversion, loadinfo ) );
        CHECK_RESULT( addGroup( eventgroupi ) );
    }

    for ( event_count = 0; event_count < num_events; event_count++ ) {
        unsigned int category_count;
        int          numinstances;
        EventI      *eventi;
        unsigned int exflags = EVENTI_TYPE_COMPLEX;

        if ( fileversion >= 0x00340000 ) {
            CHECK_RESULT( file->read( &exflags, 4, 1, NULL ) );
        }
        CHECK_RESULT( g_eventsystemi->buildEventI( ~EVENTI_DATA_INSTANCE, exflags, NULL, &eventi ) );

        if ( loadinfo &&
             loadinfo->size >= (unsigned int)( (char *)&loadinfo->sizeof_instancepool_simple - (char *)loadinfo ) +
                                   sizeof( loadinfo->sizeof_instancepool_simple ) &&
             loadinfo->sizeof_instancepool_simple && ( exflags & EVENTI_TYPE_SIMPLE ) ) {
            exflags |= EVENTI_EXFLAG_INSTANCEPOOL;
        }

        eventi->m_data_base.m_exflags = exflags;
        eventi->m_data_parent->m_systemid = g_eventsystemi->getEventID();
        eventi->m_data_base.m_eventgroupi = this;
        g_eventsystemi->m_project_numevents++;

        CHECK_RESULT( file->read( &name_len, 4, 1, NULL ) );
        if ( g_eventsystemi->m_initflags & FMOD_EVENT_INIT_DONTUSENAMES ) {
            CHECK_RESULT( file->seek( name_len, SEEK_CUR ) );
            eventi->m_data_parent->m_name = NULL;
        } else {
#line 1883
            eventi->m_data_parent->m_name = (char *)FMOD_Memory_Calloc( name_len );
            if ( !eventi->getName() ) {
                return FMOD_ERR_MEMORY;
            }
            CHECK_RESULT( file->read( eventi->getName(), 1, name_len, NULL ) );
        }

        if ( fileversion >= 0x003A0000 ) {
            FMOD_GUID *guid = eventi->m_data_parent->m_guid;

            if ( guid ) {
                CHECK_RESULT( file->read( &guid->Data1, 4, 1, NULL ) );
                CHECK_RESULT( file->read( &guid->Data2, 2, 1, NULL ) );
                CHECK_RESULT( file->read( &guid->Data3, 2, 1, NULL ) );
                CHECK_RESULT( file->read( guid->Data4, 1, 8, NULL ) );
            } else {
                CHECK_RESULT( file->seek( sizeof( FMOD_GUID ), SEEK_CUR ) );
            }
        }

        CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
        eventi->m_data_base.m_volume = tmpfloat;
        CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
        eventi->m_data_base.m_pitch = tmpfloat;
        if ( fileversion >= 0x001B0000 ) {
            CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
            eventi->m_data_base.m_pitch_randomization = tmpfloat;
        }
        if ( fileversion >= 0x00200000 ) {
            CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
            eventi->m_data_base.m_volume_randomization = tmpfloat;
            if ( fileversion < 0x00210000 ) {
                eventi->m_data_base.m_volume_randomization = 1.0f - eventi->m_data_base.m_volume_randomization;
            }
        }
        if ( fileversion >= 0x000A0000 ) {
            CHECK_RESULT( file->read( &tmpuint, 4, 1, NULL ) );
            eventi->m_data_base.m_priority = tmpuint;
        } else {
            eventi->m_data_base.m_priority = 128;
        }

        CHECK_RESULT( file->read( &numinstances, 4, 1, NULL ) );
        if ( numinstances < 1 ) {
            numinstances = 1;
        }
        CHECK_RESULT( eventi->setNumInstances( numinstances ) );

        if ( fileversion >= 0x00380000 ) {
            CHECK_RESULT( file->read( &tmpuint, 4, 1, NULL ) );
            eventi->m_data_base.m_stealpriority = tmpuint;
        } else {
            eventi->m_data_base.m_stealpriority = 10000;
        }

        CHECK_RESULT( file->read( &tmpuint, 4, 1, NULL ) );
        eventi->m_data_base.m_mode = tmpuint;
        CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
        eventi->m_data_3d.m_mindistance = tmpfloat;
        CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
        eventi->m_data_3d.m_maxdistance = tmpfloat;

        if ( fileversion >= 0x000F0000 ) {
            unsigned int flags;

            CHECK_RESULT( file->read( &flags, 4, 1, NULL ) );
            eventi->setFlags( eventi->getFlags() | flags );
        } else {
            unsigned int music;

            CHECK_RESULT( file->read( &music, 4, 1, NULL ) );
            if ( music ) {
                eventi->m_data_base.m_categoryi = g_eventsystemi->m_musiccategory;
                eventi->m_data_base.m_category_node.initNode();
                eventi->m_data_base.m_category_node.addBefore( &eventi->m_data_base.m_categoryi->m_eventhead );
                eventi->m_data_base.m_category_node.setData( eventi );
            }
        }

        if ( fileversion >= 0x00090000 ) {
            for ( int speaker = 0; speaker < 8; speaker++ ) {
                CHECK_RESULT( file->read( &eventi->m_data_2d.m_speakerlevels[speaker], 4, 1, NULL ) );
            }
            if ( eventi->m_data_base.m_mode & FMOD_2D ) {
                if ( FMOD_memcmp( eventi->m_data_2d.m_speakerlevels, lbl_803DE490, sizeof( lbl_803DE490 ) ) ) {
                    eventi->m_data_base.m_flags |= EVENTI_FLAG_SPEAKERLEVELS;
                } else {
                    eventi->m_data_base.m_flags &= ~EVENTI_FLAG_SPEAKERLEVELS;
                }
            }

            // Three values this revision reads past.
            CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
            CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
            CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );

            if ( fileversion >= 0x000B0000 ) {
                int maxplaybacksbehavior;

                CHECK_RESULT( file->read( &maxplaybacksbehavior, 4, 1, NULL ) );
                if ( fileversion < 0x00230000 && maxplaybacksbehavior > 4 ) {
                    maxplaybacksbehavior = 0;
                }
                switch ( maxplaybacksbehavior ) {
                    case 0:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_STEAL_OLDEST;
                        break;
                    case 1:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_STEAL_OLDEST;
                        break;
                    case 2:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_STEAL_NEWEST;
                        break;
                    case 3:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_STEAL_QUIETEST;
                        break;
                    case 4:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_JUST_FAIL;
                        break;
                    case 5:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_JUST_FAIL_IF_QUIETEST;
                        break;
                    default:
                        eventi->m_data_base.m_flags |= EVENTI_FLAG_STEAL_OLDEST;
                        break;
                }

                CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );

                if ( fileversion >= 0x001C0000 ) {
                    CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
                    eventi->m_data_base.m_reverbdrylevel_db = tmpfloat;
                } else {
                    eventi->m_data_base.m_reverbdrylevel_db = 0.0f;
                }
                CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
                eventi->m_data_base.m_reverbwetlevel_db = tmpfloat;

                if ( fileversion >= 0x00120000 ) {
                    CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
                    eventi->m_data_3d.m_speakerspread = tmpfloat;

                    if ( fileversion >= 0x00130000 ) {
                        CHECK_RESULT( file->read( &tmpuint, 4, 1, NULL ) );
                        eventi->m_data_base.m_fadein = tmpuint;
                        CHECK_RESULT( file->read( &tmpuint, 4, 1, NULL ) );
                        eventi->m_data_base.m_fadeout = tmpuint;

                        if ( fileversion >= 0x00160000 ) {
                            if ( fileversion >= 0x002B0000 ) {
                                CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
                            }
                            if ( fileversion >= 0x002D0000 ) {
                                CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
                            }
                            CHECK_RESULT( file->read( &tmpfloat, 4, 1, NULL ) );
                            eventi->m_data_3d.m_3dpanlevel = tmpfloat;

                            if ( fileversion >= 0x00280000 ) {
                                CHECK_RESULT( file->read( &tmpuint, 4, 1, NULL ) );
                                eventi->m_data_3d.m_rndpos_radius = tmpuint;
                            }
                        }
                    }
                }
            } else {
                eventi->m_data_base.m_flags |= EVENTI_FLAG_STEAL_OLDEST;
            }
        } else {
            memcpy( eventi->m_data_2d.m_speakerlevels, lbl_803DE490, sizeof( lbl_803DE490 ) );
        }

        CHECK_RESULT( eventi->m_impl->load( file, fileversion ) );

        unsigned int numcategories;

        CHECK_RESULT( file->read( &numcategories, 4, 1, NULL ) );
        if ( numcategories ) {
            for ( category_count = 0; category_count < numcategories; category_count++ ) {
                char categoryname[4096];

                CHECK_RESULT( file->read( &name_len, 4, 1, NULL ) );
                if ( name_len >= sizeof( categoryname ) ) {
                    return FMOD_ERR_FILE_BAD;
                }
                CHECK_RESULT( file->read( categoryname, 1, name_len, NULL ) );

                if ( !eventi->m_data_base.m_categoryi ) {
                    EventCategoryI *t;

                    result = g_eventsystemi->getCategory( categoryname, (EventCategory **)&t );
                    eventi->m_data_base.m_categoryi = t;
                    if ( result != FMOD_OK ) {
                        return result;
                    }

                    eventi->m_data_base.m_category_node.initNode();
                    eventi->m_data_base.m_category_node.addBefore( &eventi->m_data_base.m_categoryi->m_eventhead );
                    eventi->m_data_base.m_category_node.setData( eventi );
                }
            }
        } else {
            eventi->m_data_base.m_categoryi = g_eventsystemi->m_mastercategory;
            eventi->m_data_base.m_category_node.initNode();
            eventi->m_data_base.m_category_node.addBefore( &eventi->m_data_base.m_categoryi->m_eventhead );
            eventi->m_data_base.m_category_node.setData( eventi );
        }

        CHECK_RESULT( addEvent( eventi ) );
    }

    return FMOD_OK;
} // 0x80388C80

/*
====================
EventGroupI::getState

The state of the group: an error of its own or of a subgroup, else loading or ready, with playing
added while any event of it plays.
====================
*/
FMOD_RESULT EventGroupI::getState( unsigned int *state ) {
    if ( !state ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *state = 0;

    if ( m_asyncresult != FMOD_OK ) {
        *state = FMOD_EVENT_STATE_ERROR;
        return m_asyncresult;
    }

    if ( m_eventgrouphead ) {
        for ( LinkedListNode *child = m_eventgrouphead->getNext(); child != m_eventgrouphead; child = child->getNext() ) {
            unsigned int childstate;
            FMOD_RESULT  childresult = ( (EventGroupI *)child )->getState( &childstate );

            if ( childresult != FMOD_OK ) {
                *state = FMOD_EVENT_STATE_ERROR;
                return childresult;
            }
        }
    }

    if ( m_playcount > 0 ) {
        *state |= FMOD_EVENT_STATE_PLAYING;
    }

    if ( m_loading > 0 ) {
        *state |= FMOD_EVENT_STATE_LOADING;
    } else {
        *state |= FMOD_EVENT_STATE_READY;
    }

    return FMOD_OK;
} // 0x80389D88

/*
====================
EventGroupI::setUserData
====================
*/
FMOD_RESULT EventGroupI::setUserData( void *userdata ) {
    m_userdata = userdata;

    return FMOD_OK;
} // 0x80389EA4

/*
====================
EventGroupI::getUserData
====================
*/
FMOD_RESULT EventGroupI::getUserData( void **userdata ) {
    if ( !userdata ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *userdata = m_userdata;

    return FMOD_OK;
} // 0x80389EB0

/*
====================
EventGroupI::getMemoryInfo
====================
*/
FMOD_RESULT EventGroupI::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                        unsigned int *memoryused_details ) {
    return FMOD_ERR_UNIMPLEMENTED;
} // 0x80389ED0

} // namespace FMOD
