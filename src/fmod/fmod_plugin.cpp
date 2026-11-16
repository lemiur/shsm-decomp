/*
 * fmod_plugin.cpp: FMOD::Plugin, the base of the systems' loadable units. Allocation tags carry the
 * original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_plugin.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

/*
====================
Plugin::release

Frees the unit.
====================
*/
FMOD_RESULT Plugin::release() {
#line 30
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x8033E7E8

} // namespace FMOD
