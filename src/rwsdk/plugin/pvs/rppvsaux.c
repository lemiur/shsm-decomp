// Unit name and boundary: Gormiti link map (RpPvs.a rppvsaux.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpPvs.a rppvsaux.obj), Final and Debug builds.
/*
 * rppvsaux.c: auxiliary helpers for the RpPVS plugin: freeing polygon lists and
 * polygon vertex chains, and counting world sectors in a world.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/plugin/pvs/rppvs.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_PVSPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/*
 * ==================== _rpPVSDestroyPolyList ====================
 * Frees every node of a polygon record list and sets *P to NULL.
 */
void _rpPVSDestroyPolyList(_rpPVSPolyList** P) {
    _rpPVSPolyList* temp;

    while (*P) {
        temp = *P;
        *P = (*P)->next;
        RwFree(temp);
    }
} // 0x802C7BA4

/*
 * ==================== _rpPVSDestroyPoly ====================
 * Frees every node of a polygon vertex list and sets *P to NULL.
 */
void _rpPVSDestroyPoly(_rpPVSPoly** P) {
    _rpPVSPoly* temp;

    while (*P) {
        temp = *P;
        *P = (*P)->next;
        RwFree(temp);
    }
} // 0x802C7BF8

/*
 * ==================== PVSGetNumSectors ====================
 * Sector iteration callback that increments *pData for each world sector.
 */
static RpWorldSector* PVSGetNumSectors(RpWorldSector* worldSector, void* pData) {
    if ((worldSector != NULL) && (pData != NULL)) {
        (*(RwInt32*)pData)++;
        return worldSector;
    }

    RWERROR((E_RW_NULLP));
    return NULL;
} // 0x802C7C4C

/*
 * ==================== _rpPVSWorldGetNumWorldSectors ====================
 * Counts the world sectors in a world, returning -1 on NULL input.
 */
RwInt32 _rpPVSWorldGetNumWorldSectors(RpWorld* world) {
    if (world != NULL) {
        RwInt32 nCount = 0;

        RpWorldForAllWorldSectors(world, PVSGetNumSectors, &nCount);

        return nCount;
    }

    RWERROR((E_RW_NULLP));
    return -1;
} // 0x802C7CB0
