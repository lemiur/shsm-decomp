/*
 * fmod_eventinstancepool.cpp: FMOD::EventInstancePool, prebuilt simple-event instances handed out
 * round robin. Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD
 * rulings (fmod-1)"): the #line directives restore them.
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventinstancepool.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_eventgroupi.h>
#include <fmod/fmod_eventi.h>
#include <fmod/fmod_eventimpl.h>
#include <fmod/fmod_eventinstancepool.h>
#include <fmod/fmod_eventsound.h>
#include <fmod/fmod_eventsystemi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_simplemempool.h>
#include <new>

namespace FMOD {

// Returns from the enclosing function when a call fails.
#define CHECK_RESULT( _call )              \
    {                                      \
        FMOD_RESULT _result = ( _call );   \
        if ( _result != FMOD_OK ) {        \
            return _result;                \
        }                                  \
    }

// Pool memory per instance: the event, its body, instance data and sounds (value from retail; its
// sizeof breakdown is not recovered).
#define EVENTINSTANCEPOOL_INSTANCE_MEMORY 0x264

/*
====================
EventInstancePool::EventInstancePool
====================
*/
EventInstancePool::EventInstancePool() {
    m_numinstances      = 0;
    m_lastinstanceindex = 0;
    m_instance          = NULL;
    m_instance_mempool  = NULL;
} // 0x8039C210

/*
====================
EventInstancePool::init

Builds numinstances simple-event instances in one memory pool.
====================
*/
FMOD_RESULT EventInstancePool::init( unsigned int numinstances ) {
    m_numinstances      = numinstances;
    m_lastinstanceindex = numinstances;

#line 52
    m_instance_mempool = new ( FMOD_Memory_Alloc( sizeof( SimpleMemPool ) ) ) SimpleMemPool;
    if ( !m_instance_mempool ) {
        return FMOD_ERR_MEMORY;
    }
    if ( !m_instance_mempool->init( ( m_numinstances * EVENTINSTANCEPOOL_INSTANCE_MEMORY + 3 ) & ~3, NULL ) ) {
        return FMOD_ERR_MEMORY;
    }

#line 62
    m_instance = (EventI **)FMOD_Memory_Alloc( m_numinstances * sizeof( EventI * ) );
    if ( !m_instance ) {
        return FMOD_ERR_MEMORY;
    }

    unsigned int i;
    for ( i = 0; i < m_numinstances; i++ ) {
        CHECK_RESULT( buildEventInstance( &m_instance[i], m_instance_mempool ) );
        CHECK_RESULT( m_instance[i]->createDSPNetwork() );
    }

    return FMOD_OK;
} // 0x8039C228

/*
====================
EventInstancePool::release

Releases every instance, then the instance table, the pool memory and the pool.
====================
*/
FMOD_RESULT EventInstancePool::release() {
    if ( m_instance ) {
        for ( unsigned int i = 0; i < m_numinstances; i++ ) {
            if ( m_instance[i] ) {
                CHECK_RESULT( m_instance[i]->releaseDSPNetwork() );
                CHECK_RESULT( m_instance[i]->release( true, false ) );
            }
        }
#line 93
        FMOD_Memory_Free( m_instance );
    }

    if ( m_instance_mempool ) {
        m_instance_mempool->release();
    }

#line 101
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x8039C3F4

/*
====================
EventInstancePool::buildEventInstance

Builds one instance: a simple event with one sound, all from mempool.
====================
*/
FMOD_RESULT EventInstancePool::buildEventInstance( EventI **event, SimpleMemPool *mempool ) {
    EventI *eventi;

    CHECK_RESULT( g_eventsystemi->buildEventI( ~EVENTI_DATA_PARENT, EVENTI_TYPE_SIMPLE, mempool, &eventi ) );

    EventImplSimple *impl = (EventImplSimple *)eventi->m_impl;
    eventi->m_data_instance->m_mempool = mempool;

#line 119
    EventSound *sound = new ( mempool->alloc( sizeof( EventSound ), __FILE__, __LINE__ ) ) EventSound;
    sound->init( NULL );
    sound->m_eventi = eventi;
    CHECK_RESULT( sound->createOneshotSounds( mempool ) );

    impl->m_sound = sound;
    *event        = eventi;

    return FMOD_OK;
} // no copy in retail (dead-stripped or inlined)

/*
====================
EventInstancePool::getEventInstance

Hands out the next free instance after the last one handed out, or steals one through the parent's
group when all are in use.
====================
*/
FMOD_RESULT EventInstancePool::getEventInstance( EventI *parentevent, EventI **dstevent ) {
    EventI      *event = NULL;
    unsigned int index = m_lastinstanceindex, i;

    for ( i = 0; i < m_numinstances; i++ ) {
        index++;
        if ( index >= m_numinstances ) {
            index = 0;
        }

        if ( !( m_instance[index]->m_data_base.m_exflags & EVENTI_EXFLAG_INUSE ) ) {
            event = m_instance[index];
            break;
        }
    }

    if ( !event ) {
        CHECK_RESULT( parentevent->m_data_base.m_eventgroupi->stealEventInstance( parentevent, &event, true ) );
    }

    m_lastinstanceindex = index;
    *dstevent           = event;

    return FMOD_OK;
} // 0x8039C4EC

/*
====================
EventInstancePool::releaseEventInstance

Gives an instance back to the pool; for a parent event, every instance of it.
====================
*/
FMOD_RESULT EventInstancePool::releaseEventInstance( EventI *event, bool waituntilready ) {
    if ( !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !m_instance ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    if ( event->m_data_parent ) {
        for ( unsigned int i = 0; i < m_numinstances; i++ ) {
            EventI *e = m_instance[i];

            if ( e->getOriginal() == event ) {
                CHECK_RESULT( releaseEventInstance( e, waituntilready ) );
            }
        }
    } else if ( event->m_data_base.m_exflags & EVENTI_EXFLAG_INUSE ) {
        CHECK_RESULT( event->stop( true ) );
        CHECK_RESULT( event->releaseStreams( waituntilready ) );
        event->incRefcnt( true );
        event->m_data_base.m_exflags &= ~EVENTI_EXFLAG_INUSE;
    }

    return FMOD_OK;
} // 0x8039C5B0

/*
====================
EventInstancePool::copyEventInstance

Makes dstevent an in-use instance with srcevent's properties.
====================
*/
FMOD_RESULT EventInstancePool::copyEventInstance( EventI *srcevent, EventI *dstevent ) {
    CHECK_RESULT( srcevent->copyProperties( dstevent ) );

    dstevent->m_data_base.m_exflags |= EVENTI_EXFLAG_INUSE | EVENTI_EXFLAG_POOLCOPY;

    return FMOD_OK;
} // 0x8039C6D8

/*
====================
EventInstancePool::getEventInstanceIndex
====================
*/
FMOD_RESULT EventInstancePool::getEventInstanceIndex( EventI *event, int *index ) {
    if ( !index || !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !m_instance ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    *index = 0;

    for ( int i = 0; i < (int)m_numinstances; i++ ) {
        if ( m_instance[i] == event ) {
            *index = i;
            return FMOD_OK;
        }
    }

    return FMOD_ERR_INVALID_PARAM;
} // 0x8039C728

/*
====================
EventInstancePool::getEventInstancePtr
====================
*/
FMOD_RESULT EventInstancePool::getEventInstancePtr( unsigned int index, EventI **event ) {
    if ( !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !m_instance ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    *event = m_instance[index];

    return FMOD_OK;
} // 0x8039C7A4

/*
====================
EventInstancePool::getNumInstancesInUse

How many instances of event's parent are in use.
====================
*/
FMOD_RESULT EventInstancePool::getNumInstancesInUse( EventI *event, int *numinstances ) {
    if ( !event || !numinstances ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !m_instance ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    EventI *parentevent = event->getOriginal() ? event->getOriginal() : event;
    int     i, n = 0;

    for ( i = 0; i < (int)m_numinstances; i++ ) {
        if ( m_instance[i]->getOriginal() == parentevent && ( m_instance[i]->m_data_base.m_exflags & EVENTI_EXFLAG_INUSE ) ) {
            n++;
        }
    }

    *numinstances = n;

    return FMOD_OK;
} // 0x8039C7DC

} // namespace FMOD
