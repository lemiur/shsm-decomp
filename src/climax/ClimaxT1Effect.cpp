// Unit name and boundary: Gormiti link map (ClimaxT1Effect.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxT1Effect.obj).
// Retail has only the UV animation half of Gormiti's object. ClimaxT1EffectBuildUVAnimMaterialsList4C,
// the CM animation functions (AddCMAnimationMaterialToListLv0..Lv2, ClimaxT1EffectBuildCMAnimMaterialsList4W and 4C,
// the SetUVAnimTime, SetCMAnimTime, AddCMAnimTime, CMAnimUpdate, MaxUVAnimTime and MaxCMAnimTime functions),
// ClimaxT1ClumpForcedFrameSync with its callback, and the three ClimaxT1Debug pointer functions have no copy in
// retail (dead-stripped).
/*
 * ClimaxT1Effect.cpp: the ClimaxT1 effects of a world or a clump. It attaches the plugins of the
 * ClimaxT1 effect system, collects the materials with UV animations of a world into a list that is then
 * advanced in time, finds the bounds of a clump in world space (through the bones of skinned atomics),
 * and switches the rendering of the atomics of a clump on and off.
 */

#include <climax/ClimaxT1Atomic.h>
#include <climax/ClimaxT1Clump.h>
#include <climax/ClimaxT1Dictionary.h>
#include <climax/ClimaxT1Effect.h>
#include <climax/ClimaxT1Frame.h>
#include <climax/ClimaxT1Geometry.h>
#include <climax/ClimaxT1KeyFrame.h>
#include <climax/ClimaxT1Material.h>
#include <climax/GeometryData.h>
#include <climax/RwInterface.h>
#include <rwsdk/baframe.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/plugin/skin2/rpskin.h>
#include <rwsdk/plugin/uvanim/rpuvanim.h>

/*
====================
ClimaxT1EffectPluginAttach

Attaches the plugins of the ClimaxT1 effect system. Returns false when one of them fails.
====================
*/
// 0x801DD70C
extern "C" int ClimaxT1EffectPluginAttach() {
    int result;

    if ( !ClimaxT1KeyFrameRegisterInterpolationScheme() ) {
        return FALSE;
    }
    if ( !ClimaxT1DictionaryPluginAttach() ) {
        return FALSE;
    }
    if ( !ClimaxT1MaterialPluginAttach() ) {
        return FALSE;
    }
    if ( !ClimaxT1GeometryPluginAttach() ) {
        return FALSE;
    }
    if ( !ClimaxT1AtomicPluginAttach() ) {
        return FALSE;
    }
    if ( !ClimaxT1FramePluginAttach() ) {
        return FALSE;
    }
    result = ClimaxT1ClumpPluginAttach();
    return result != 0;
}

/*
====================
AddUVAnimationMaterialToListLv0

Adds a material that has a UV animation to the list in data, once.
====================
*/
// 0x801DD7B0
static RpMaterial* AddUVAnimationMaterialToListLv0( RpMaterial* material, void* data ) {
    RwSList* list = (RwSList*)data;

    if ( list != NULL && RpMaterialUVAnimExists( material ) ) {
        RpMaterial** current;

        for ( current = (RpMaterial**)_rwSListGetBegin( list ); current != (RpMaterial**)_rwSListGetEnd( list );
              current++ ) {
            if ( material == *current ) {
                return material;
            }
        }
        *(RpMaterial**)_rwSListGetNewEntry( list, 0 ) = material;
    }
    return material;
}

/*
====================
AddUVAnimationMaterialToListLv1

Adds the materials with UV animations of an atomic's geometry to the list in data.
====================
*/
// 0x801DD84C
static RpAtomic* AddUVAnimationMaterialToListLv1( RpAtomic* atomic, void* data ) {
    RpGeometry* geometry = RpAtomicGetGeometry( atomic );

    if ( geometry != NULL ) {
        RpGeometryForAllMaterials( geometry, AddUVAnimationMaterialToListLv0, data );
    }
    return atomic;
}

/*
====================
AddUVAnimationMaterialToListLv2

Adds the materials with UV animations of a clump's atomics to the list in data.
====================
*/
// 0x801DD898
static RpClump* AddUVAnimationMaterialToListLv2( RpClump* clump, void* data ) {
    RpClumpForAllAtomics( clump, AddUVAnimationMaterialToListLv1, data );
    return clump;
}

/*
====================
ClimaxT1EffectBuildUVAnimMaterialsList4W

The list of the materials with UV animations in a world, in its own materials and in its clumps. NULL when
there is no world or the list cannot be made.
====================
*/
// 0x801DD8D4
extern "C" RwSList* ClimaxT1EffectBuildUVAnimMaterialsList4W( RpWorld* world ) {
    RwSList* list = NULL;

    if ( world != NULL ) {
        list = _rwSListCreate( sizeof( RpMaterial* ), 0 );
        if ( list != NULL ) {
            RpWorldForAllMaterials( world, AddUVAnimationMaterialToListLv0, list );
            RpWorldForAllClumps( world, AddUVAnimationMaterialToListLv2, list );
        }
    }
    return list;
}

/*
====================
ClimaxT1EffectAddUVAnimTime

Advances the UV animation of every material in the list by time.
====================
*/
// 0x801DD954
extern "C" void ClimaxT1EffectAddUVAnimTime( RwSList* materialList, float time ) {
    RpMaterial** material;

    if ( materialList != NULL ) {
        for ( material = (RpMaterial**)_rwSListGetBegin( materialList );
              material != (RpMaterial**)_rwSListGetEnd( materialList ); material++ ) {
            RpMaterialUVAnimAddAnimTime( *material, time );
        }
    }
}

/*
====================
ClimaxT1EffectUVAnimUpdate

Applies the UV animation of every material in the list.
====================
*/
// 0x801DD9C4
extern "C" void ClimaxT1EffectUVAnimUpdate( RwSList* materialList ) {
    RpMaterial** material;

    if ( materialList != NULL ) {
        for ( material = (RpMaterial**)_rwSListGetBegin( materialList );
              material != (RpMaterial**)_rwSListGetEnd( materialList ); material++ ) {
            RpMaterialUVAnimApplyUpdate( *material );
        }
    }
}

/*
====================
BoundsIntegrate

Grows the box in Extents so that it holds pos. The first point sets the box.
====================
*/
// 0x801DDA24
void BoundsIntegrate( wbCBData* Extents, RwV3d& pos ) {
    if ( Extents->first ) {
        *Extents->min = pos;
        *Extents->max = pos;
        Extents->first = false;
    } else {
        if ( Extents->min->x > pos.x ) {
            Extents->min->x = pos.x;
        }
        if ( Extents->min->y > pos.y ) {
            Extents->min->y = pos.y;
        }
        if ( Extents->min->z > pos.z ) {
            Extents->min->z = pos.z;
        }
        if ( Extents->max->x < pos.x ) {
            Extents->max->x = pos.x;
        }
        if ( Extents->max->y < pos.y ) {
            Extents->max->y = pos.y;
        }
        if ( Extents->max->z < pos.z ) {
            Extents->max->z = pos.z;
        }
    }
}

/*
====================
ClimaxT1AtomicGetWorldSpaceBounds

Grows the box in Extents by the bounds of an atomic in world space: the spheres around the bones of a
skinned atomic, or the sphere around the whole geometry of another one.

Not matched (97.2%): wall.exe says register/schedule, control flow isomorphic. Retail holds skinMatrix, GeomData and
the bone offset in r31, r30 and r29, this source in other registers, and loads the bone's radius address before its
bone index. Ruled out: declaration order of the five outer locals (all 120 orders), the order of the Radius and BoneID
reads, the order of the Min and Max statements, BoneID inlined into the call, const locals, ++Ind, the Debug build's
dead `int first = 1;`. RpGeometryGetData's retail copy is another revision than Gormiti's.
====================
*/
// 0x801DDB08
RpAtomic* ClimaxT1AtomicGetWorldSpaceBounds( RpAtomic* atomic, wbCBData* Extents ) {
    RpGeometry* geometry;
    RpSkin* skin;
    RwMatrix* skinMatrix;
    RwFrame* frame;
    RwMatrix* ltm;

    if ( atomic != NULL ) {
        cGeometryData* GeomData;

        geometry = RpAtomicGetGeometry( atomic );
        frame = RpAtomicGetFrame( atomic );
        GeomData = RpGeometryGetData( geometry );

        if ( geometry != NULL && frame != NULL && GeomData != NULL ) {
            skin = RpSkinGeometryGetSkin( geometry );
            if ( skin != NULL ) {
                RwV3d Min;
                RwV3d Max;

                skinMatrix = RpSkinGetSkinToBoneMatrices( skin );
                ltm = RwFrameGetLTM( frame );
                for ( int Ind = 0; Ind < GeomData->m_BoneCount; Ind++ ) {
                    int BoneID = GeomData->m_BoneData[Ind].BoneID;
                    float Radius = GeomData->m_BoneData[Ind].Radius;

                    RwV3dTransformPoint( &Min, &skinMatrix[BoneID].pos, ltm );
                    Max = Min;
                    Min.x = Min.x - Radius;
                    Min.y = Min.y - Radius;
                    Min.z = Min.z - Radius;
                    Max.x = Max.x + Radius;
                    Max.y = Max.y + Radius;
                    Max.z = Max.z + Radius;
                    BoundsIntegrate( Extents, Min );
                    BoundsIntegrate( Extents, Max );
                }
            } else {
                RwV3d Min;
                RwV3d Max;

                ltm = RwFrameGetLTM( frame );
                RwV3dTransformPoint( &Min, &GeomData->m_BoundingSphere.center, ltm );
                Max = Min;
                Min.x = Min.x - GeomData->m_BoundingSphere.radius;
                Min.y = Min.y - GeomData->m_BoundingSphere.radius;
                Min.z = Min.z - GeomData->m_BoundingSphere.radius;
                Max.x = Max.x + GeomData->m_BoundingSphere.radius;
                Max.y = Max.y + GeomData->m_BoundingSphere.radius;
                Max.z = Max.z + GeomData->m_BoundingSphere.radius;
                BoundsIntegrate( Extents, Min );
                BoundsIntegrate( Extents, Max );
            }
        }
    }
    return atomic;
}

/*
====================
wbCB

Grows the box in data by an atomic's bounds, except for the atomics of particles.
====================
*/
// 0x801DDCF0
RpAtomic* wbCB( RpAtomic* atomic, void* data ) {
    wbCBData* wb = (wbCBData*)data;

    if ( atomic != NULL && ClimaxP1AtomicGetIndex( atomic ) < 0 && wb != NULL ) {
        ClimaxT1AtomicGetWorldSpaceBounds( atomic, wb );
    }
    return atomic;
}

/*
====================
ClimaxT1ClumpGetWorldSpaceBounds

Sets min and max to the corners of the box around a clump's atomics in world space.
====================
*/
// 0x801DDD50
extern "C" RpClump* ClimaxT1ClumpGetWorldSpaceBounds( RpClump* clump, RwV3d* min, RwV3d* max ) {
    wbCBData data;

    data.first = true;
    data.min = min;
    data.max = max;
    RpClumpForAllAtomics( clump, wbCB, &data );
    return clump;
}

/*
====================
ClimaxT1AtomicRenderFCB

Stops an atomic from rendering.
====================
*/
// 0x801DDD9C
static RpAtomic* ClimaxT1AtomicRenderFCB( RpAtomic* atomic, void* data ) {
    if ( atomic != NULL ) {
        int flags = RpAtomicGetFlags( atomic );

        flags &= ~rpATOMICRENDER;
        rwObjectSetFlags( atomic, flags );
    }
    return atomic;
}

/*
====================
ClimaxT1AtomicRenderTCB

Makes an atomic render.
====================
*/
// 0x801DDDB4
static RpAtomic* ClimaxT1AtomicRenderTCB( RpAtomic* atomic, void* data ) {
    if ( atomic != NULL ) {
        int flags = RpAtomicGetFlags( atomic );

        flags |= rpATOMICRENDER;
        rwObjectSetFlags( atomic, flags );
    }
    return atomic;
}

/*
====================
ClimaxT1ClumpStartRendering

Makes the atomics of a clump render.
====================
*/
// 0x801DDDCC
extern "C" void ClimaxT1ClumpStartRendering( RpClump* clump ) {
    if ( clump != NULL ) {
        RpClumpForAllAtomics( clump, ClimaxT1AtomicRenderTCB, NULL );
    }
}

/*
====================
ClimaxT1ClumpStopRendering

Stops the atomics of a clump from rendering.
====================
*/
// 0x801DDDE8
extern "C" void ClimaxT1ClumpStopRendering( RpClump* clump ) {
    if ( clump != NULL ) {
        RpClumpForAllAtomics( clump, ClimaxT1AtomicRenderFCB, NULL );
    }
}
