/*
 * fmod_dsp.cpp: FMOD::DSP, the public handle of a DSP unit. Each method checks its handle and
 * forwards to the internal DSPI. setActive and setBypass set bits of the unit's mFlags directly:
 * 0x2 is the active bit and 0x4 the bypass bit (the bits' meanings follow the public setter names;
 * retail has no names for them).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp.obj)

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_dspi.h>

namespace FMOD {

/*
====================
DSP::release
====================
*/
FMOD_RESULT DSP::release() {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->release( true );
} // 0x80379A10

/*
====================
DSP::disconnectFrom
====================
*/
FMOD_RESULT DSP::disconnectFrom( DSP *target ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->disconnectFrom( (DSPI *)target, NULL );
} // 0x80379A58

/*
====================
DSP::disconnectAll
====================
*/
FMOD_RESULT DSP::disconnectAll( bool inputs, bool outputs ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->disconnectAll( inputs, outputs );
} // 0x80379AA4

/*
====================
DSP::setActive
====================
*/
FMOD_RESULT DSP::setActive( bool active ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    dspi->setActive( active );
    return FMOD_OK;
} // 0x80379AFC

/*
====================
DSP::setBypass
====================
*/
FMOD_RESULT DSP::setBypass( bool bypass ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    dspi->setBypass( bypass );
    return FMOD_OK;
} // 0x80379B64

/*
====================
DSP::setParameter
====================
*/
FMOD_RESULT DSP::setParameter( int index, float value ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->setParameter( index, value );
} // 0x80379BCC

/*
====================
DSP::getParameterInfo
====================
*/
FMOD_RESULT DSP::getParameterInfo( int index, char *name, char *label, char *description, int descriptionlen,
                                   float *min, float *max ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->getParameterInfo( index, name, label, description, descriptionlen, min, max );
} // 0x80379C30

/*
====================
DSP::getType
====================
*/
FMOD_RESULT DSP::getType( FMOD_DSP_TYPE *type ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->getType( type );
} // 0x80379CB4

/*
====================
DSP::setDefaults
====================
*/
FMOD_RESULT DSP::setDefaults( float frequency, float volume, float pan, int priority ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->setDefaults( frequency, volume, pan, priority );
} // 0x80379D08

/*
====================
DSP::getDefaults
====================
*/
FMOD_RESULT DSP::getDefaults( float *frequency, float *volume, float *pan, int *priority ) {
    FMOD_RESULT result;
    DSPI       *dspi;

    result = DSPI::validate( this, &dspi );
    if ( result != FMOD_OK ) {
        return result;
    }

    return dspi->getDefaults( frequency, volume, pan, priority );
} // 0x80379D8C

} // namespace FMOD
