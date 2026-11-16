/*
 * fmod_eventuserproperty.cpp: FMOD::UserProperty, a user property of an event. Allocation tags carry
 * the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventuserproperty.obj)

#include <fmod/fmod_event.h>
#include <fmod/fmod_eventuserproperty.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_string.h>
#include <fmod/fmod_types.h>

namespace FMOD {

/*
====================
UserProperty::UserProperty
====================
*/
UserProperty::UserProperty() {
    m_name  = NULL;
    m_type  = FMOD_EVENTPROPERTY_TYPE_INT;
    m_value = NULL;
} // 0x803A9EC0

/*
====================
UserProperty::init

Becomes a copy of src, with its own copies of the name and of a string value.
====================
*/
FMOD_RESULT UserProperty::init( UserProperty *src ) {
    m_name = NULL;
    if ( src->m_name ) {
        m_name = FMOD_strdup( src->m_name );
        if ( !m_name ) {
            return FMOD_ERR_MEMORY;
        }
    }

    m_type = src->m_type;
    if ( m_type == FMOD_EVENTPROPERTY_TYPE_STRING ) {
        if ( src->m_value ) {
            m_value = FMOD_strdup( (const char *)src->m_value );
            if ( !m_value ) {
                return FMOD_ERR_MEMORY;
            }
        }
    } else {
        m_value = src->m_value;
    }

    return FMOD_OK;
} // 0x803A9EE0

/*
====================
UserProperty::release

Frees the name, a string value and the property.
====================
*/
FMOD_RESULT UserProperty::release() {
    if ( m_name ) {
#line 65
        FMOD_Memory_Free( m_name );
    }

    if ( m_type == FMOD_EVENTPROPERTY_TYPE_STRING && m_value ) {
#line 70
        FMOD_Memory_Free( m_value );
    }

#line 73
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803A9F80

/*
====================
UserProperty::load

Reads the property from an event project file: the name, the type and the value (an int, a float
or a string). The value local is unsigned int here (Gormiti's DWARF records int): with a signed local
the compiler turns the byte swap into a byte-reversed store (stwbrx), which retail does not have.
====================
*/
FMOD_RESULT UserProperty::load( File *file, unsigned int version ) {
    FMOD_RESULT  result;
    unsigned int name_len;

    result = file->read( &name_len, 4, 1, 0 );
    if ( result != FMOD_OK ) {
        return result;
    }

#line 91
    m_name = (char *)FMOD_Memory_Calloc( name_len );
    if ( !m_name ) {
        return FMOD_ERR_MEMORY;
    }

    result = file->read( m_name, 1, name_len, 0 );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = file->read( &m_type, 4, 1, 0 );
    if ( result != FMOD_OK ) {
        return result;
    }

    switch ( m_type ) {
        unsigned int value;

        case FMOD_EVENTPROPERTY_TYPE_INT:
            result = file->read( &value, 1, 4, 0 );
            if ( result != FMOD_OK ) {
                return result;
            }

            value   = FMOD_SWAPENDIAN_DWORD( value );
            m_value = (void *)value;
            break;

        case FMOD_EVENTPROPERTY_TYPE_FLOAT:
            result = file->read( &value, 1, 4, 0 );
            if ( result != FMOD_OK ) {
                return result;
            }

            value   = FMOD_SWAPENDIAN_DWORD( value );
            m_value = (void *)value;
            break;

        case FMOD_EVENTPROPERTY_TYPE_STRING:
            result = file->read( &name_len, 4, 1, 0 );
            if ( result != FMOD_OK ) {
                return result;
            }

#line 154
            m_value = FMOD_Memory_Calloc( name_len );
            if ( !m_value ) {
                return FMOD_ERR_MEMORY;
            }

            result = file->read( m_value, 1, name_len, 0 );
            if ( result != FMOD_OK ) {
                return result;
            }
            break;
    }

    return FMOD_OK;
} // 0x803AA02C

} // namespace FMOD
