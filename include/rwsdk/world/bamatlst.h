// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: tgsm/pbg include/rwsdk/world/bamateri.h
#ifndef RWSDK_WORLD_BAMATLST_H
#define RWSDK_WORLD_BAMATLST_H

#include <rwsdk/world/bamateri.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RpMaterialList: the materials of a geometry or world, each referenced once, and the room allocated for them. */
typedef struct RpMaterialList {
    RpMaterial** materials; /* 0x0 */
    RwInt32 numMaterials;   /* 0x4 */
    RwInt32 space;          /* 0x8 */
} RpMaterialList;

RpMaterialList* _rpMaterialListDeinitialize(RpMaterialList* matList);
RpMaterialList* _rpMaterialListInitialize(RpMaterialList* matList);
RpMaterial* _rpMaterialListGetMaterial(const RpMaterialList* matList, RwInt32 matIndex);
RwInt32 _rpMaterialListAppendMaterial(RpMaterialList* matList, RpMaterial* material);
RwInt32 _rpMaterialListFindMaterialIndex(const RpMaterialList* matList, const RpMaterial* material);
RpMaterialList* _rpMaterialListStreamRead(RwStream* stream, RpMaterialList* matList);

#ifdef __cplusplus
}
#endif

#endif
