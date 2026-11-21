// Unit name and boundary: class CZone (registry string, include/climax/AttributeHandler.h); boundary from the unit plan, one function

#include <game/CZone.h>
#include <extras.h>
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>
#include <rwsdk/world/climaxcommon.h>

/*
 * Gives the zone's ice materials their GameCube callbacks. A material whose multitexture effect
 * is named Effect_O_Ice_1.mte gets the first callback, and then the effect's config and the
 * material's flags are changed; one named Effect_O_Ice_2.mte gets the second callback only.
 * Sets the zone's flag 0x1 when any material was changed. Name invented.
 * Open: the world and loop-offset registers are swapped against retail (wall: register/schedule,
 * one consistent renaming; the rest of the body is equivalent).
 * 0x80089D0C
 */
extern "C" void fn_80089D0C( CZone* zone ) {
    if ( zone == NULL ) {
        return;
    }

    RpWorld* world = zone->mWorld;
    if ( world == NULL ) {
        return;
    }

    RwInt32 numIce = 0;
    RwInt32 numMaterials = RpWorldGetNumMaterials( world );
    for ( RwInt32 i = 0; i < numMaterials; i++ ) {
        RpMaterial* material = RpWorldGetMaterial( world, i );
        if ( material == NULL ) {
            continue;
        }
        RpMultiTexture* multiTexture = RpMaterialGetMultiTexture( material, rwID_GAMECUBE );
        if ( multiTexture == NULL ) {
            continue;
        }
        RpMTEffect* effect = RpMultiTextureGetEffect( multiTexture );
        if ( effect == NULL ) {
            continue;
        }

        const RwChar* name = RpMTEffectGetName( effect );
        RwBool isIce1 = FALSE;
        if ( stricmp( name, "Effect_O_Ice_1.mte" ) == 0 ) {
            RpMultiTextureSetGameCubeMTCallBacks( multiTexture, fn_8008914C, NULL, NULL );
            isIce1 = TRUE;
        }
        if ( stricmp( name, "Effect_O_Ice_2.mte" ) == 0 ) {
            RpMultiTextureSetGameCubeMTCallBacks( multiTexture, fn_800891D8, NULL, NULL );
        }

        if ( isIce1 ) {
            RpGameCubeMTConfig* config = RpGameCubeMTEffectGetConfig( effect );
            config->flags |= 0x100;
            RwClimaxMaterialSetFlags( material, RwClimaxMaterialGetFlags( material ) | 0x200 );
            numIce++;
        }
    }

    if ( numIce != 0 ) {
        zone->mFlags |= 0x1;
    }
}
