/*
 * fmod_eventcategoryi.cpp: FMOD::EventCategoryI, a category of events (volume, pitch, pause, mute,
 * DSP channel group network, max playbacks and event stealing). Allocation tags carry the original
 * file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)"): the #line directives restore
 * them.
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventcategoryi.obj)
// Names and layout: Gormiti link map and DWARF (fmod_event.a fmod_eventcategoryi.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_eventcategoryi.h>
#include <fmod/fmod_eventgroupi.h>
#include <fmod/fmod_eventi.h>
#include <fmod/fmod_eventimpl.h>
#include <fmod/fmod_eventinstancepool.h>
#include <fmod/fmod_eventprojecti.h>
#include <fmod/fmod_eventsystemi.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_string.h>
#include <limits.h>
#include <math.h>
#include <new>

namespace FMOD {

// Size of the category path buffer used while loading (name invented).
#define EVENTCATEGORYI_PATH_LENGTH 4096

// First event file version whose categories store max playbacks and the stealing behaviour (name invented).
#define EVENTCATEGORYI_FILEVERSION_MAXPLAYBACKS 0x00290000

// Returns from the enclosing function when a call fails.
#define CHECK_RESULT( _call )              \
    {                                      \
        FMOD_RESULT _result = ( _call );   \
        if ( _result != FMOD_OK ) {        \
            return _result;                \
        }                                  \
    }

// Finds a direct child category by case-insensitive name (helper inlined in addCategory; name invented).
inline EventCategoryI *EventCategoryI::getCategoryPtr( const char *name ) {
    if ( !name ) {
        return NULL;
    }

    if ( m_categoryhead ) {
        for ( LinkedListNode *node = m_categoryhead->getNext(); node != m_categoryhead; node = node->getNext() ) {
            if ( ( (EventCategoryI *)node )->m_name &&
                 !FMOD_stricmp( ( (EventCategoryI *)node )->m_name, name ) ) {
                return (EventCategoryI *)node;
            }
        }
    }

    return NULL;
}

/*
====================
EventCategoryI::EventCategoryI
====================
*/
EventCategoryI::EventCategoryI() {
    init();
} // 0x80384A54

/*
====================
EventCategoryI::init
====================
*/
FMOD_RESULT EventCategoryI::init() {
    m_name           = NULL;
    m_index          = 0;
    m_paused         = false;
    m_mute           = false;
    m_volume         = 1.0f;
    m_pitch          = 0.0f;
    m_maxplaybacks   = 0;
    m_playcount      = 0;
    m_flags          = 0;
    m_channelgroup   = NULL;
    m_categoryhead   = NULL;
    m_parentcategory = NULL;
    m_userdata       = NULL;
    m_eventhead.initNode();

    return FMOD_OK;
} // 0x80384B04

/*
====================
EventCategoryI::release
====================
*/
FMOD_RESULT EventCategoryI::release() {
    FMOD_RESULT result;

    result = releaseDSPNetwork();
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( m_categoryhead ) {
        LinkedListNode *node = m_categoryhead->getNext();
        while ( node != m_categoryhead ) {
            LinkedListNode *next = node->getNext();
            result = ( (EventCategoryI *)node )->release();
            if ( result != FMOD_OK ) {
                return result;
            }
            node = next;
        }

        result = m_categoryhead->release();
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    if ( m_name ) {
#line 126
        FMOD_Memory_Free( m_name );
    }

#line 129
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x80384B5C

/*
====================
EventCategoryI::load
====================
*/
FMOD_RESULT EventCategoryI::load( File *file, unsigned int fileversion, FMOD_EVENT_LOADINFO *loadinfo,
                                  EventCategoryI *parentcategory, const char *categorypath ) {
    unsigned int name_len;
    unsigned int num_categories;
    unsigned int category_count;
    char         thispath[EVENTCATEGORYI_PATH_LENGTH];
    bool         override = false;
    float        tmp_volume;
    float        tmp_pitch;

    if ( loadinfo &&
         loadinfo->size >= (unsigned int)( (char *)&loadinfo->override_category_vals - (char *)loadinfo ) +
                               sizeof( loadinfo->override_category_vals ) ) {
        override = loadinfo->override_category_vals ? true : false;
    }

    thispath[0] = 0;

    CHECK_RESULT( file->read( &name_len, 4, 1, NULL ) );

    if ( m_name ) {
#line 176
        FMOD_Memory_Free( m_name );
    }

#line 178
    m_name = (char *)FMOD_Memory_Calloc( name_len );
    if ( !m_name ) {
        return FMOD_ERR_MEMORY;
    }

    CHECK_RESULT( file->read( m_name, 1, name_len, NULL ) );
    CHECK_RESULT( file->read( &tmp_volume, 4, 1, NULL ) );
    CHECK_RESULT( file->read( &tmp_pitch, 4, 1, NULL ) );

    if ( parentcategory || override ) {
        CHECK_RESULT( setVolume( tmp_volume ) );
        CHECK_RESULT( setPitch( tmp_pitch, FMOD_EVENT_PITCHUNITS_RAW ) );
    }

    if ( fileversion >= EVENTCATEGORYI_FILEVERSION_MAXPLAYBACKS ) {
        unsigned int maxplaybacks_behavior;

        CHECK_RESULT( file->read( &m_maxplaybacks, 4, 1, NULL ) );
        CHECK_RESULT( file->read( &maxplaybacks_behavior, 4, 1, NULL ) );

        switch ( maxplaybacks_behavior ) {
            case 1:
                m_flags |= EVENTCATEGORYI_MAXPLAYBACKS_STEALNEWEST;
                break;
            case 2:
                m_flags |= EVENTCATEGORYI_MAXPLAYBACKS_STEALQUIETEST;
                break;
            case 3:
                m_flags |= EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAIL;
                break;
            case 4:
                m_flags |= EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAILIFQUIETEST;
                break;
            default:
                break;
        }
    }

    CHECK_RESULT( file->read( &num_categories, 4, 1, NULL ) );

    if ( parentcategory ) {
        CHECK_RESULT( createDSPNetwork( parentcategory ) );
    }

    if ( !g_eventsystemi->m_musiccategory && m_name && !FMOD_strcmp( m_name, EVENT_CATEGORY_MUSIC ) ) {
        g_eventsystemi->m_musiccategory = this;
    }

    if ( num_categories && categorypath && FMOD_strcmp( EVENT_CATEGORY_MASTER, categorypath ) ) {
        FMOD_strcpy( thispath, categorypath );
        FMOD_strcat( thispath, "/" );
        FMOD_strcat( thispath, m_name );
    }

    for ( category_count = 0; category_count < num_categories; category_count++ ) {
#line 269
        EventCategoryI *category = new ( FMOD_Memory_Alloc( sizeof( EventCategoryI ) ) ) EventCategoryI;
        if ( !category ) {
            return FMOD_ERR_MEMORY;
        }
        category->init();

        CHECK_RESULT( category->load( file, fileversion, loadinfo, this, thispath ) );

        if ( !parentcategory ) {
            CHECK_RESULT( mergeCategory( category, override ) );

            if ( category->getNext() == category ) {
                CHECK_RESULT( category->release() );
            }
        } else {
            CHECK_RESULT( addCategory( category ) );
        }
    }

    return FMOD_OK;
} // 0x80384C50

/*
====================
EventCategoryI::mergeCategory
====================
*/
FMOD_RESULT EventCategoryI::mergeCategory( EventCategoryI *category, bool override ) {
    EventCategoryI *existingcategory = NULL;

    if ( m_categoryhead ) {
        for ( LinkedListNode *oldnode = m_categoryhead->getNext(); oldnode != m_categoryhead;
              oldnode = oldnode->getNext() ) {
            if ( !FMOD_stricmp( ( (EventCategoryI *)oldnode )->m_name, category->m_name ) ) {
                existingcategory = (EventCategoryI *)oldnode;
                break;
            }
        }
    }

    if ( existingcategory ) {
        if ( override ) {
            CHECK_RESULT( existingcategory->setVolume( category->m_volume ) );
            CHECK_RESULT( existingcategory->setPitch( category->m_pitch, FMOD_EVENT_PITCHUNITS_RAW ) );
        }

        if ( category->m_categoryhead ) {
            for ( LinkedListNode *node = category->m_categoryhead->getNext(); node != category->m_categoryhead; ) {
                EventCategoryI *subcategory = (EventCategoryI *)node;
                node                        = node->getNext();
                CHECK_RESULT( existingcategory->mergeCategory( subcategory, override ) );
            }
        }
    } else {
        category->releaseDSPNetwork();
        category->removeNode();
        CHECK_RESULT( addCategory( category ) );
        category->createDSPNetwork( this );
    }

    return FMOD_OK;
} // 0x80385164

/*
====================
EventCategoryI::createDSPNetwork
====================
*/
FMOD_RESULT EventCategoryI::createDSPNetwork( EventCategoryI *parentcategory ) {
    FMOD_RESULT result = FMOD_OK;

    if ( m_channelgroup ) {
        return FMOD_OK;
    }

    if ( m_name && !FMOD_strcmp( m_name, EVENT_CATEGORY_MUSIC ) ) {
        if ( !g_eventsystemi->m_musicchannelgroup ) {
            result = g_eventsystemi->m_system->createChannelGroup( EVENT_CATEGORY_MUSIC, &m_channelgroup );
            g_eventsystemi->m_musicchannelgroup = m_channelgroup;
        }
    } else {
        result = g_eventsystemi->m_system->createChannelGroup( m_name, &m_channelgroup );
    }

    if ( result != FMOD_OK ) {
        return result;
    }

    if ( m_channelgroup ) {
        if ( parentcategory ) {
            ChannelGroup *parentgroup;
            if ( parentcategory->m_name && !FMOD_strcmp( parentcategory->m_name, EVENT_CATEGORY_MUSIC ) ) {
                parentgroup = g_eventsystemi->m_musicchannelgroup;
            } else {
                parentgroup = parentcategory->m_channelgroup;
            }

            result = parentgroup->addGroup( m_channelgroup );
            if ( result != FMOD_OK ) {
                return result;
            }
        }

        result = m_channelgroup->setVolume( m_volume );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = m_channelgroup->setPitch( powf( 2.0f, 4.0f * m_pitch ) );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    if ( m_categoryhead ) {
        for ( LinkedListNode *node = m_categoryhead->getNext(); node != m_categoryhead; node = node->getNext() ) {
            result = ( (EventCategoryI *)node )->createDSPNetwork( this );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    return FMOD_OK;
} // 0x8038531C

/*
====================
EventCategoryI::releaseDSPNetwork
====================
*/
FMOD_RESULT EventCategoryI::releaseDSPNetwork() {
    FMOD_RESULT result;

    if ( m_categoryhead ) {
        for ( LinkedListNode *node = m_categoryhead->getNext(); node != m_categoryhead; node = node->getNext() ) {
            result = ( (EventCategoryI *)node )->releaseDSPNetwork();
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    if ( g_eventsystemi->m_musicchannelgroup == m_channelgroup ) {
        g_eventsystemi->m_musicchannelgroup = NULL;
    }

    if ( m_channelgroup ) {
        result = m_channelgroup->release();
        if ( result != FMOD_OK ) {
            return result;
        }
        m_channelgroup = NULL;
    }

    return FMOD_OK;
} // 0x803854D8

/*
====================
EventCategoryI::addCategory
====================
*/
FMOD_RESULT EventCategoryI::addCategory( EventCategoryI *category ) {
    if ( !category ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( m_categoryhead ) {
        if ( getCategoryPtr( category->m_name ) ) {
            return FMOD_ERR_EVENT_NAMECONFLICT;
        }
    } else {
#line 501
        m_categoryhead = new ( FMOD_Memory_Alloc( sizeof( EventCategoryI ) ) ) EventCategoryI;
        if ( !m_categoryhead ) {
            return FMOD_ERR_MEMORY;
        }
        m_categoryhead->init();
    }

    category->addBefore( m_categoryhead );
    category->m_parentcategory = this;
    category->m_index          = category->count() - 1;

    FMOD_RESULT result = category->setMute( m_mute );
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x8038559C

/*
====================
EventCategoryI::stealEvent
====================
*/
FMOD_RESULT EventCategoryI::stealEvent( EventI *event ) {
    FMOD_RESULT result;

    if ( !m_maxplaybacks ) {
        return FMOD_OK;
    }

    if ( m_playcount >= m_maxplaybacks ) {
        unsigned int behaviour = m_flags & EVENTCATEGORYI_MAXPLAYBACKS_MASK;

        if ( behaviour == EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAIL ) {
            return FMOD_ERR_EVENT_FAILED;
        }

        unsigned int    min_clock   = UINT_MAX;
        unsigned int    max_clock   = 0;
        float           min_volume  = 9999999.0f;
        EventI         *steal_event = NULL;
        LinkedListNode *projectnode;

        if ( behaviour == EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAILIFQUIETEST ) {
            result = event->estimateAudibility( &min_volume );
            if ( result != FMOD_OK ) {
                return result;
            }
        }

        for ( projectnode = g_eventsystemi->m_eventprojecthead.getNext();
              projectnode != &g_eventsystemi->m_eventprojecthead; projectnode = projectnode->getNext() ) {
            EventProjectI *project = (EventProjectI *)projectnode;

            for ( LinkedListNode *node = project->m_updatetimeparameter_head.getNext();
                  node != &project->m_updatetimeparameter_head; node = node->getNext() ) {
                EventI *current = (EventI *)node->getData();

                if ( current->m_data_base.m_categoryi == this ) {
                    switch ( behaviour ) {
                        case EVENTCATEGORYI_MAXPLAYBACKS_STEALNEWEST:
                            if ( current->getStartClock().mHi > max_clock ) {
                                steal_event = current;
                                max_clock   = current->getStartClock().mHi;
                            }
                            break;

                        case EVENTCATEGORYI_MAXPLAYBACKS_STEALQUIETEST:
                        case EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAILIFQUIETEST: {
                            float volume;

                            result = current->m_impl->getAudibility( &volume );
                            if ( result != FMOD_OK ) {
                                return result;
                            }

                            if ( volume < min_volume ) {
                                min_volume  = volume;
                                steal_event = current;
                            }
                            break;
                        }

                        default:
                            if ( current->getStartClock().mHi < min_clock ) {
                                steal_event = current;
                                min_clock   = current->getStartClock().mHi;
                            }
                            break;
                    }
                }
            }
        }

        if ( steal_event ) {
            result = steal_event->stop( true );
            if ( result != FMOD_OK ) {
                return result;
            }
        } else {
            return FMOD_ERR_EVENT_FAILED;
        }
    }

    return FMOD_OK;
} // 0x80385818

/*
====================
EventCategoryI::getInfo
====================
*/
FMOD_RESULT EventCategoryI::getInfo( int *index, char **name ) {
    if ( index ) {
        *index = m_index;
    }
    if ( name ) {
        *name = m_name;
    }

    return FMOD_OK;
} // 0x80385A04

/*
====================
EventCategoryI::getCategory
====================
*/
FMOD_RESULT EventCategoryI::getCategory( const char *name, EventCategory **category ) {
    if ( !name || !category || !m_categoryhead ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *category = NULL;

    const char *p;
    for ( p = name; *p && *p != '/'; p++ ) {
    }

    for ( LinkedListNode *node = m_categoryhead->getNext(); node != m_categoryhead; node = node->getNext() ) {
        if ( ( (EventCategoryI *)node )->m_name &&
             !FMOD_strnicmp( ( (EventCategoryI *)node )->m_name, name, p - name ) &&
             !( (EventCategoryI *)node )->m_name[p - name] ) {
            if ( *p ) {
                return ( (EventCategoryI *)node )->getCategory( p + 1, category );
            }

            *category = (EventCategoryI *)node;
            return FMOD_OK;
        }
    }

    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x80385A2C

/*
====================
EventCategoryI::getCategoryByIndex
====================
*/
FMOD_RESULT EventCategoryI::getCategoryByIndex( int index, EventCategory **category ) {
    if ( !category || !m_categoryhead ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *category = (EventCategoryI *)m_categoryhead->getNodeByIndex( index );

    return *category ? FMOD_OK : FMOD_ERR_EVENT_NOTFOUND;
} // 0x80385B8C

/*
====================
EventCategoryI::getNumCategories
====================
*/
FMOD_RESULT EventCategoryI::getNumCategories( int *numcategories ) {
    if ( !numcategories ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    int count;
    if ( m_categoryhead ) {
        count = m_categoryhead->count();
    } else {
        count = 0;
    }

    *numcategories = count;

    return FMOD_OK;
} // 0x80385C04

/*
====================
EventCategoryI::getEventByIndex
====================
*/
FMOD_RESULT EventCategoryI::getEventByIndex( int index, unsigned int eventmode, Event **event ) {
    if ( !event ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    EventI *e = (EventI *)m_eventhead.getNodeByIndex( index )->getData();
    if ( e ) {
        if ( eventmode & FMOD_EVENT_INFOONLY ) {
            CHECK_RESULT( g_eventsystemi->createEventHandle( e, event, eventmode, false ) );
            return FMOD_OK;
        }

        return e->m_data_base.m_eventgroupi->getEventInstance( e, event, eventmode );
    }

    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x80385C50

/*
====================
EventCategoryI::getNumEvents
====================
*/
FMOD_RESULT EventCategoryI::getNumEvents( int *numevents ) {
    if ( !numevents ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numevents = m_eventhead.count();

    return FMOD_OK;
} // 0x80385D14

/*
====================
EventCategoryI::stopAllEvents
====================
*/
FMOD_RESULT EventCategoryI::stopAllEvents() {
    LinkedListNode *current;

    if ( m_categoryhead ) {
        LinkedListNode *node = m_categoryhead->getNext();
        while ( node != m_categoryhead ) {
            LinkedListNode *next = node->getNext();
            CHECK_RESULT( ( (EventCategoryI *)node )->stopAllEvents() );
            node = next;
        }
    }

    for ( current = m_eventhead.getNext(); current != &m_eventhead; current = current->getNext() ) {
        EventI *event = (EventI *)current->getData();
        int     count;

        if ( event->useInstancePool() ) {
            EventInstancePool *pool         = event->m_data_base.m_eventgroupi->m_project->m_instancepool;
            int                numinstances = pool->m_numinstances;
            EventI           **instances    = pool->m_instance;

            for ( int i = 0; i < numinstances; i++ ) {
                EventI *instance = instances[i];

                if ( instance && instance->getOriginal() == event &&
                     ( instance->m_data_base.m_exflags & EVENTI_EXFLAG_INUSE ) ) {
                    CHECK_RESULT( instance->stop( false ) );
                }
            }
        } else {
            for ( count = 0; count < event->m_data_parent->m_numinstances; count++ ) {
                if ( event->m_data_parent->m_instance[count] ) {
                    event->m_data_parent->m_instance[count]->stop( false );
                }
            }
        }
    }

    return FMOD_OK;
} // 0x80385D50

/*
====================
EventCategoryI::setVolume
====================
*/
FMOD_RESULT EventCategoryI::setVolume( float volume ) {
    if ( volume > 1.0f ) {
        volume = 1.0f;
    }
    if ( volume < 0.0f ) {
        volume = 0.0f;
    }

    m_volume = volume;

    if ( m_channelgroup ) {
        FMOD_RESULT result = m_channelgroup->setVolume( m_volume );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x80385ED4

/*
====================
EventCategoryI::getVolume
====================
*/
FMOD_RESULT EventCategoryI::getVolume( float *volume ) {
    if ( !volume ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *volume = m_volume;

    return FMOD_OK;
} // 0x80385F3C

/*
====================
EventCategoryI::setPitch
====================
*/
FMOD_RESULT EventCategoryI::setPitch( float pitch, FMOD_EVENT_PITCHUNITS units ) {
    FMOD_RESULT result;

    switch ( units ) {
        case FMOD_EVENT_PITCHUNITS_RAW:
            break;
        case FMOD_EVENT_PITCHUNITS_OCTAVES:
            pitch *= 0.25f;
            break;
        case FMOD_EVENT_PITCHUNITS_SEMITONES:
            pitch /= 48.0f;
            break;
        case FMOD_EVENT_PITCHUNITS_TONES:
            pitch /= 24.0f;
            break;
        default:
            break;
    }

    ChannelGroup *channelgroup = m_channelgroup;
    m_pitch = pitch;

    if ( channelgroup ) {
        result = channelgroup->setPitch( powf( 2.0f, 4.0f * m_pitch ) );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x80385F5C

/*
====================
EventCategoryI::getPitch
====================
*/
FMOD_RESULT EventCategoryI::getPitch( float *pitch, FMOD_EVENT_PITCHUNITS units ) {
    if ( !pitch ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    float raw_pitch = m_pitch;
    *pitch          = raw_pitch;

    switch ( units ) {
        case FMOD_EVENT_PITCHUNITS_RAW:
            break;
        case FMOD_EVENT_PITCHUNITS_OCTAVES:
            *pitch = 4.0f * raw_pitch;
            break;
        case FMOD_EVENT_PITCHUNITS_SEMITONES:
            *pitch = 48.0f * raw_pitch;
            break;
        case FMOD_EVENT_PITCHUNITS_TONES:
            *pitch = 24.0f * raw_pitch;
            break;
        default:
            break;
    }

    return FMOD_OK;
} // 0x80385FFC

/*
====================
EventCategoryI::setPaused
====================
*/
FMOD_RESULT EventCategoryI::setPaused( bool paused ) {
    FMOD_RESULT result = FMOD_OK;

    if ( m_channelgroup ) {
        result = m_channelgroup->setPaused( paused );
    }

    m_paused = paused;

    return result;
} // 0x80386064

/*
====================
EventCategoryI::getPaused
====================
*/
FMOD_RESULT EventCategoryI::getPaused( bool *paused ) {
    if ( !paused ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *paused = m_paused;

    return FMOD_OK;
} // 0x803860B4

/*
====================
EventCategoryI::setMute
====================
*/
FMOD_RESULT EventCategoryI::setMute( bool mute ) {
    if ( m_channelgroup ) {
        m_channelgroup->setMute( mute );
    }

    m_mute = mute;

    return FMOD_OK;
} // 0x803860D4

/*
====================
EventCategoryI::getMute
====================
*/
FMOD_RESULT EventCategoryI::getMute( bool *mute ) {
    if ( !mute ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *mute = m_mute;

    return FMOD_OK;
} // 0x80386124

/*
====================
EventCategoryI::getChannelGroup
====================
*/
FMOD_RESULT EventCategoryI::getChannelGroup( ChannelGroup **channelgroup ) {
    if ( !channelgroup ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *channelgroup = m_channelgroup;

    return FMOD_OK;
} // 0x80386144

/*
====================
EventCategoryI::setUserData
====================
*/
FMOD_RESULT EventCategoryI::setUserData( void *userdata ) {
    m_userdata = userdata;

    return FMOD_OK;
} // 0x80386164

/*
====================
EventCategoryI::getMemoryInfo
====================
*/
FMOD_RESULT EventCategoryI::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits,
                                           unsigned int *memoryused, unsigned int *memoryused_details ) {
    return FMOD_ERR_UNIMPLEMENTED;
} // 0x80386170

/*
====================
EventCategoryI::getUserData
====================
*/
FMOD_RESULT EventCategoryI::getUserData( void **userdata ) {
    if ( !userdata ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *userdata = m_userdata;

    return FMOD_OK;
} // 0x80386178

} // namespace FMOD
