// Name: Gormiti link map (RPworld.a instancegeom, gcn/instance/instancegeom.c)
#ifndef RWSDK_WORLD_INSTANCEGEOM_H
#define RWSDK_WORLD_INSTANCEGEOM_H

#include <rwsdk/plcore/baresour.h>
#include <rwsdk/world/bageomet.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Instances a geometry's vertex and index data into a display list resource entry: the optimized
 * form (condenses duplicate attributes per mesh, one sorted pass over the meshes) or the fast one.
 * The owner and its entry pointer are what the resource arena keeps to free the entry later.
 */
extern RwResEntry* _rwDlGeometryInstanceOptimized(RpGeometry* geometry, void* owner, RwResEntry** resEntryOwner);
extern RwResEntry* _rwDlGeometryInstanceFast(RpGeometry* geometry, void* owner, RwResEntry** resEntryOwner);

#ifdef __cplusplus
}
#endif

#endif
