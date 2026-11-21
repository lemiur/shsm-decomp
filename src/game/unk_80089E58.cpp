// unit name unconfirmed: no string or class-name evidence names this file; the Gormiti link map lists no clean object for this range.

#include <game/unk_80089E58.h>
#include <extras.h>  // stricmp
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>

/*
====================
fn_80089E58
====================
*/
// 0x80089E58
extern "C" void fn_80089E58(RpAtomic* atomic) {
    if (atomic == NULL) {
        return;
    }
    RpGeometry* geometry = atomic->geometry;
    if (geometry == NULL) {
        return;
    }
    RwInt32 numMaterials = geometry->matList.numMaterials;
    for (RwInt32 i = 0; i < numMaterials; i++) {
        RpMaterial* material = geometry->matList.materials[i];
        if (material == NULL) {
            continue;
        }
        RpMultiTexture* multiTexture = RpMaterialGetMultiTexture(material, rwID_GAMECUBE);
        if (multiTexture == NULL) {
            continue;
        }
        RpMTEffect* effect = RpMultiTextureGetEffect(multiTexture);
        if (effect == NULL) {
            continue;
        }
        if (stricmp(RpMTEffectGetName(effect), "Effect_O_Ice_2.mte") == 0) {
            RpMultiTextureSetGameCubeMTCallBacks(multiTexture, fn_800891D8, NULL, NULL);
        }
    }
}
