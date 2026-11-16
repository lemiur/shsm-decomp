/*
 * fmod_eventreverb.cpp: FMOD::ReverbDef and FMOD::EventReverbI, the Event system's reverbs.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventreverb.obj)

#include <string.h>

#include <fmod/fmod.hpp>
#include <fmod/fmod_event.h>
#include <fmod/fmod_eventreverb.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

/*
====================
ReverbDef::ReverbDef

A definition with the "off" preset.
====================
*/
ReverbDef::ReverbDef() {
    m_name = NULL;

    FMOD_REVERB_PROPERTIES props = FMOD_PRESET_OFF;
    memcpy( &m_props, &props, sizeof( FMOD_REVERB_PROPERTIES ) );

    m_eventproject = NULL;
} // 0x803A1DCC

/*
====================
ReverbDef::release

Unlinks the definition and frees its name and itself.
====================
*/
FMOD_RESULT ReverbDef::release() {
    if ( m_name ) {
#line 59
        FMOD_Memory_Free( m_name );
    }

    removeNode();

#line 64
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803A1E58

/*
====================
EventReverbI::EventReverbI
====================
*/
EventReverbI::EventReverbI() {
    m_reverb   = NULL;
    m_userdata = NULL;
} // 0x803A1EF4

/*
====================
EventReverbI::init

Attaches the system's reverb.
====================
*/
FMOD_RESULT EventReverbI::init( Reverb *reverb ) {
    m_reverb = reverb;

    return FMOD_OK;
} // 0x803A1F20

/*
====================
EventReverbI::release

Releases the system's reverb, unlinks and frees this.
====================
*/
FMOD_RESULT EventReverbI::release() {
    if ( m_reverb ) {
        m_reverb->release();
    }

    removeNode();

#line 110
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803A1F2C

/*
====================
EventReverbI::setProperties
====================
*/
FMOD_RESULT EventReverbI::setProperties( const FMOD_REVERB_PROPERTIES *props ) {
    if ( !m_reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return m_reverb->setProperties( props );
} // 0x803A1FB8

/*
====================
EventReverbI::getProperties
====================
*/
FMOD_RESULT EventReverbI::getProperties( FMOD_REVERB_PROPERTIES *props ) {
    if ( !m_reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return m_reverb->getProperties( props );
} // 0x803A1FD0

/*
====================
EventReverbI::set3DAttributes
====================
*/
FMOD_RESULT EventReverbI::set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance ) {
    if ( !m_reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return m_reverb->set3DAttributes( position, mindistance, maxdistance );
} // 0x803A1FE8

/*
====================
EventReverbI::get3DAttributes
====================
*/
FMOD_RESULT EventReverbI::get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance ) {
    if ( !m_reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return m_reverb->get3DAttributes( position, mindistance, maxdistance );
} // 0x803A2000

/*
====================
EventReverbI::setActive
====================
*/
FMOD_RESULT EventReverbI::setActive( bool active ) {
    if ( !m_reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return m_reverb->setActive( active );
} // 0x803A2018

/*
====================
EventReverbI::getActive
====================
*/
FMOD_RESULT EventReverbI::getActive( bool *active ) {
    if ( !m_reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return m_reverb->getActive( active );
} // 0x803A2030

/*
====================
EventReverbI::setUserData
====================
*/
FMOD_RESULT EventReverbI::setUserData( void *userdata ) {
    m_userdata = userdata;

    return FMOD_OK;
} // 0x803A2048

/*
====================
EventReverbI::getUserData
====================
*/
FMOD_RESULT EventReverbI::getUserData( void **userdata ) {
    if ( !userdata ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *userdata = m_userdata;

    return FMOD_OK;
} // 0x803A2054

/*
====================
EventReverbI::getMemoryInfo
====================
*/
FMOD_RESULT EventReverbI::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                         unsigned int *memoryused_details ) {
    return FMOD_ERR_UNIMPLEMENTED;
} // 0x803A2074

} // namespace FMOD
