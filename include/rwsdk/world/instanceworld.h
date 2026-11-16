// Name: Gormiti link map (RPworld.a instanceworld, gcn/instance/instanceworld.c)
#ifndef RWSDK_WORLD_INSTANCEWORLD_H
#define RWSDK_WORLD_INSTANCEWORLD_H

#include <rwsdk/plcore/baresour.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/basector.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Instances a world sector's vertex data into a display list resource entry: the optimized form
 * (one sorted pass over the meshes) or the fast one. The owner and its entry pointer are what the
 * resource arena keeps to free the entry later.
 */
extern RwResEntry* _rwDlWorldSectorInstanceOptimized(RpWorld* world, RpWorldSector* sector, void* owner,
                                                     RwResEntry** resEntryOwner);
extern RwResEntry* _rwDlWorldSectorInstanceFast(RpWorld* world, RpWorldSector* sector, void* owner,
                                                RwResEntry** resEntryOwner);

#ifdef __cplusplus
}
#endif

#endif
