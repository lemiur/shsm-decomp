// Names: Gormiti DWARF and link map (RpMatFX.a)
#ifndef RWSDK_PLUGIN_MATFX_RPMATFX_H
#define RWSDK_PLUGIN_MATFX_RPMATFX_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/world/bamateri.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The effect of a material (RpMatFXMaterialFlags). */
typedef enum RpMatFXMaterialFlags {
    rpMATFXEFFECTNULL = 0,
    rpMATFXEFFECTBUMPMAP = 1,
    rpMATFXEFFECTENVMAP = 2,
    rpMATFXEFFECTBUMPENVMAP = 3,
    rpMATFXEFFECTDUAL = 4,
    rpMATFXEFFECTUVTRANSFORM = 5,
    rpMATFXEFFECTDUALUVTRANSFORM = 6,
    rpMATFXEFFECTMAX = 7,
    rpMATFXNUMEFFECTS = rpMATFXEFFECTMAX - 1,
    rpMATFXFORCEENUMSIZEINT = 0x7FFFFFFF
} RpMatFXMaterialFlags;

RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial* material);

/*
 * RpMatFX: material effects. A material's effect can transform the UV coordinates of its two
 * textures by a matrix each (rpuvanim.c sets them from its animations).
 */
RpMaterial* RpMatFXMaterialSetUVTransformMatrices(RpMaterial* material, RwMatrix* baseTransform,
                                                  RwMatrix* dualTransform);

#ifdef __cplusplus
}
#endif

#endif
