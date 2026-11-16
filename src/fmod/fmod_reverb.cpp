/*
 * fmod_reverb.cpp: FMOD::Reverb, the public handle of a reverb. Each method checks its handle and
 * forwards to the internal ReverbI.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_reverb.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_reverbi.h>

namespace FMOD {

/*
====================
Reverb::release

Frees the reverb.
====================
*/
FMOD_RESULT Reverb::release() {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->release( true );
} // 0x8033FC14

/*
====================
Reverb::set3DAttributes
====================
*/
FMOD_RESULT Reverb::set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance ) {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->set3DAttributes( position, mindistance, maxdistance );
} // 0x8033FC50

/*
====================
Reverb::get3DAttributes
====================
*/
FMOD_RESULT Reverb::get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance ) {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->get3DAttributes( position, mindistance, maxdistance );
} // 0x8033FCB8

/*
====================
Reverb::setProperties
====================
*/
FMOD_RESULT Reverb::setProperties( const FMOD_REVERB_PROPERTIES *prop ) {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->setProperties( prop );
} // 0x8033FD20

/*
====================
Reverb::getProperties
====================
*/
FMOD_RESULT Reverb::getProperties( FMOD_REVERB_PROPERTIES *prop ) {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->getProperties( prop );
} // 0x8033FD68

/*
====================
Reverb::setActive
====================
*/
FMOD_RESULT Reverb::setActive( bool active ) {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->setActive( active );
} // 0x8033FDB0

/*
====================
Reverb::getActive
====================
*/
FMOD_RESULT Reverb::getActive( bool *active ) {
    FMOD_RESULT result;
    ReverbI    *reverbi;

    result = ReverbI::validate( this, &reverbi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return reverbi->getActive( active );
} // 0x8033FDF8

} // namespace FMOD
