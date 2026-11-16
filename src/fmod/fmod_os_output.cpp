/*
 * fmod_os_output.cpp: FMOD's operating-system output layer for the Wii: which output exists and its
 * registration.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_os_output.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_output_revolution.h>
#include <fmod/fmod_pluginfactory.h>

using namespace FMOD;

/*
====================
FMOD_OS_Output_Register

Registers the Wii output with the plugin factory.
====================
*/
FMOD_RESULT FMOD_OS_Output_Register( PluginFactory *pluginfactory ) {
    pluginfactory->registerOutput( OutputRevolution::getDescriptionEx(), 0 );

    return FMOD_OK;
} // 0x80358934

/*
====================
FMOD_OS_Output_GetDefault

The Wii has the one output.
====================
*/
FMOD_RESULT FMOD_OS_Output_GetDefault( FMOD_OUTPUTTYPE *outputtype ) {
    if ( !outputtype ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *outputtype = FMOD_OUTPUTTYPE_WII;

    return FMOD_OK;
} // 0x80358974
