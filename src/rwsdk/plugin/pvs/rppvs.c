// Unit name and boundary: Gormiti link map (RpPvs.a rppvs.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpPvs.a rppvs.obj), Final and Debug builds.
/*
 * rppvs.c: the RpPVS plugin (Potentially Visible Set). Extends RpWorld with an RpPVSCache
 * and RpWorldSector with an RpPVS visibility bitmask and bounds, hooks sector rendering
 * to cull non-visible sectors from the view sector's vismap, and streams PVS chunks with
 * worlds and world sectors.
 */
#include <math.h>
#include <rwsdk/badevice.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamath.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/basector.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/plugin/pvs/rppvs.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_PVSPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

#define RPPVS_WORLD_DATA(world) \
    RWPLUGINOFFSET(RpPVSCache, (world), lbl_80517780.worldOffset)

#define RPPVS_SECTOR_DATA(sector) \
    RWPLUGINOFFSET(RpPVS, (sector), lbl_80517780.sectorOffset)

RpPVSGlobalVars lbl_80517780;

/* Retail-only sector render counters in PVSWorldSectorRenderCallBack (absent in Gormiti). Defined in
 * this order because uninitialised small data is laid out in reverse order of definition. */
RwInt32 lbl_8052B34C;
RwInt32 lbl_8052B348;

/*
 * ==================== PVSWorldSectorRenderCallBack ====================
 * Sector render callback installed by RpPVSHook. Culls sectors whose bit is clear in the
 * current view sector's vismap and accumulates triangle and sector render statistics.
 */
static RpWorldSector* PVSWorldSectorRenderCallBack(RpWorldSector* spSector) {
    RpPVSCache* pvsCache;
    RpWorldSector* result;

    pvsCache = RPPVS_WORLD_DATA(lbl_80517780.World);

    if (pvsCache->hooked) {
        if (RpPVSWorldSectorVisible(spSector)) {
            result = pvsCache->renderCallBack(spSector);
            pvsCache->ptotal += spSector->numTriangles;
            pvsCache->paccept += spSector->numTriangles;
            lbl_8052B348++;
        } else {
            pvsCache->ptotal += spSector->numTriangles;
            result = NULL;
            lbl_8052B34C++;
        }
    } else {
        result = pvsCache->renderCallBack(spSector);
    }

    return result;
} // 0x802C6DE4

/*
 * ==================== PVSDivideBox ====================
 * Splits a bounding box along the given axis offset into left and right sub-boxes.
 */
static void PVSDivideBox(const RwBBox* box, RwReal value, RwBBox* lbox, RwBBox* rbox, RwInt32 align) {
    lbox->inf.x = box->inf.x;
    lbox->inf.y = box->inf.y;
    lbox->inf.z = box->inf.z;
    lbox->sup.x = box->sup.x;
    lbox->sup.y = box->sup.y;
    lbox->sup.z = box->sup.z;

    rbox->sup.x = box->sup.x;
    rbox->sup.y = box->sup.y;
    rbox->sup.z = box->sup.z;
    rbox->inf.x = box->inf.x;
    rbox->inf.y = box->inf.y;
    rbox->inf.z = box->inf.z;

    if (align == 0) {
        if (value > box->sup.x) {
            value = box->sup.x;
        }
        if (value < box->inf.x) {
            value = box->inf.x;
        }
        lbox->sup.x = value;
        rbox->inf.x = value;
    } else if (align == 4) {
        if (value > box->sup.y) {
            value = box->sup.y;
        }
        if (value < box->inf.y) {
            value = box->inf.y;
        }
        lbox->sup.y = value;
        rbox->inf.y = value;
    } else if (align == 8) {
        if (value > box->sup.z) {
            value = box->sup.z;
        }
        if (value < box->inf.z) {
            value = box->inf.z;
        }
        lbox->sup.z = value;
        rbox->inf.z = value;
    }
} // 0x802C6EE4

/*
 * ==================== PVSFindDiagonalSizeOfBBox ====================
 * Computes the length of the diagonal of a bounding box.
 * no copy in retail (inlined)
 */
static RwReal PVSFindDiagonalSizeOfBBox(RwBBox* boundingBox) {
    RwV3d diag;
    RwReal size;

    RwV3dSubMacro(&diag, &boundingBox->sup, &boundingBox->inf);
    size = RwV3dLength(&diag);

    return size;
}

/*
 * ==================== PVSCalculateSectorBounds ====================
 * Recursively partitions the world bounding box down the BSP tree and records each leaf
 * sector's partition box, geometry bounding box, and diagonal length.
 */
static RpSector* PVSCalculateSectorBounds(RpSector* spSector, const RwBBox* box) {
    RpPVS* pvsCur;
    RwBBox leftbox;
    RwBBox rightbox;
    const RwBBox* gbox;

    if (spSector->type < 0) {
        RpWorldSector* wpSector = (RpWorldSector*)spSector;

        pvsCur = RPPVS_SECTOR_DATA(wpSector);
        gbox = &wpSector->boundingBox;

        pvsCur->sbox.inf.x = box->inf.x;
        pvsCur->sbox.inf.y = box->inf.y;
        pvsCur->sbox.inf.z = box->inf.z;
        pvsCur->sbox.sup.x = box->sup.x;
        pvsCur->sbox.sup.y = box->sup.y;
        pvsCur->sbox.sup.z = box->sup.z;

        if (pvsCur->sbox.sup.x < pvsCur->sbox.inf.x) {
            RwReal t = pvsCur->sbox.sup.x;
            pvsCur->sbox.sup.x = pvsCur->sbox.inf.x;
            pvsCur->sbox.inf.x = t;
        }
        if (pvsCur->sbox.sup.y < pvsCur->sbox.inf.y) {
            RwReal t = pvsCur->sbox.sup.y;
            pvsCur->sbox.sup.y = pvsCur->sbox.inf.y;
            pvsCur->sbox.inf.y = t;
        }
        if (pvsCur->sbox.sup.z < pvsCur->sbox.inf.z) {
            RwReal t = pvsCur->sbox.sup.z;
            pvsCur->sbox.sup.z = pvsCur->sbox.inf.z;
            pvsCur->sbox.inf.z = t;
        }

        pvsCur->gbox.inf.x = gbox->inf.x;
        pvsCur->gbox.inf.y = gbox->inf.y;
        pvsCur->gbox.inf.z = gbox->inf.z;
        pvsCur->gbox.sup.x = gbox->sup.x;
        pvsCur->gbox.sup.y = gbox->sup.y;
        pvsCur->gbox.sup.z = gbox->sup.z;

        pvsCur->diagonal = PVSFindDiagonalSizeOfBBox(&pvsCur->sbox);
    } else {
        RpPlaneSector* pspPlane = (RpPlaneSector*)spSector;

        PVSDivideBox(box, pspPlane->value, &leftbox, &rightbox, pspPlane->type);
        PVSCalculateSectorBounds(pspPlane->leftSubTree, &leftbox);
        PVSCalculateSectorBounds(pspPlane->rightSubTree, &rightbox);
    }

    return spSector;
} // 0x802C6FBC

/*
 * ==================== PVSCopyAndTagSectors ====================
 * Computes a world sector's PVS centre and per-axis binary subdivision significance.
 * Quirk kept: v is computed and never read; diagshift is worked out again from the box (as in
 * Gormiti's Debug build, which calls RwV3dSub for v and then subtracts per component).
 */
static RpWorldSector* PVSCopyAndTagSectors(RpWorldSector* sector, void* pData) {
    RpPVS* pvsCur;
    RwV3d v;
    RwReal max;
    RwV3d diagshift;
    RwBBox box;

    (void)pData;

    pvsCur = RPPVS_SECTOR_DATA(sector);

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(lbl_80517780.World);

    box = pvsCur->sbox;

    pvsCur->centre.x = (box.sup.x + box.inf.x) / 2.0f;
    pvsCur->centre.y = (box.sup.y + box.inf.y) / 2.0f;
    pvsCur->centre.z = (box.sup.z + box.inf.z) / 2.0f;
    RwV3dSubMacro(&v, &box.sup, &box.inf);

    diagshift.x = box.sup.x - box.inf.x;
    diagshift.y = box.sup.y - box.inf.y;
    diagshift.z = box.sup.z - box.inf.z;

    max = (diagshift.x > diagshift.y) ? diagshift.x : diagshift.y;
    max = (diagshift.z > max) ? diagshift.z : max;

    diagshift.x = diagshift.x / max;
    diagshift.y = diagshift.y / max;
    diagshift.z = diagshift.z / max;

    if (diagshift.x != 0.0f) {
        pvsCur->axessig[0] = -RwInt32FromRealMacro((RwReal)(log(1.0f / diagshift.x) / log(2.0)));
    }
    if (diagshift.y != 0.0f) {
        pvsCur->axessig[1] = -RwInt32FromRealMacro((RwReal)(log(1.0f / diagshift.y) / log(2.0)));
    }
    if (diagshift.z != 0.0f) {
        pvsCur->axessig[2] = -RwInt32FromRealMacro((RwReal)(log(1.0f / diagshift.z) / log(2.0)));
    }

    return sector;
} // 0x802C7120

/*
 * ==================== PVSInitializeSectors ====================
 * Initializes PVS sector counts, world diagonal, sector bounds, and axis tags.
 * no copy in retail (inlined)
 */
static void PVSInitializeSectors(void) {
    RpPVSCache* PVSCache;

    PVSCache = RPPVS_WORLD_DATA(lbl_80517780.World);
    PVSCache->NumWorldSectors = _rpPVSWorldGetNumWorldSectors(lbl_80517780.World);
    lbl_80517780.diagonal = PVSFindDiagonalSizeOfBBox(RpWorldGetBBox(lbl_80517780.World));
    PVSCalculateSectorBounds(lbl_80517780.World->rootSector, RpWorldGetBBox(lbl_80517780.World));
    RpWorldForAllWorldSectors(lbl_80517780.World, PVSCopyAndTagSectors, NULL);
}

/*
 * ==================== PVSDeinitialize ====================
 * Frees cached PVS polygon records and vertex chains on a world.
 * no copy in retail (inlined)
 */
static void PVSDeinitialize(RpWorld* wpWorld) {
    RpPVSCache* pvsCache;
    _rpPVSPolyList* temp;

    pvsCache = RPPVS_WORLD_DATA(wpWorld);
    temp = pvsCache->polygons;
    if (pvsCache->formatted) {
        while (temp != NULL) {
            _rpPVSDestroyPoly(&temp->data.geom);
            temp = temp->next;
        }
        _rpPVSDestroyPolyList(&pvsCache->polygons);
        pvsCache->formatted = FALSE;
    }
}

/*
 * ==================== GetEntireSector ====================
 * Sector callback that finds the world sector containing the current view position.
 */
static RpWorldSector* GetEntireSector(RpWorldSector* sector, void* data) {
    RpPVS* PVSSector = RPPVS_SECTOR_DATA(sector);
    RwInt32* index = (RwInt32*)data;

    if ((lbl_80517780.InSector == -1) ||
        ((lbl_80517780.ViewPos.x <= PVSSector->sbox.sup.x) &&
         (lbl_80517780.ViewPos.x >= PVSSector->sbox.inf.x) &&
         (lbl_80517780.ViewPos.y <= PVSSector->sbox.sup.y) &&
         (lbl_80517780.ViewPos.y >= PVSSector->sbox.inf.y) &&
         (lbl_80517780.ViewPos.z <= PVSSector->sbox.sup.z) &&
         (lbl_80517780.ViewPos.z >= PVSSector->sbox.inf.z))) {
        lbl_80517780.InSector = *index;
        lbl_80517780.CurrPVS = RPPVS_SECTOR_DATA(sector);
    }

    RPPVS_SECTOR_DATA(sector)->sectailpoly = NULL;
    (*index)++;

    return sector;
} // 0x802C7324

/*
 * ==================== RpPVSSetViewPosition ====================
 * Sets the PVS view position and locates the world sector containing it. The search runs under a
 * test of InSector that the compiler folds (it was just set to -1); PVSCache is read only by the
 * closing assertion.
 */
RpWorldSector* RpPVSSetViewPosition(RpWorld* wpWorld, RwV3d* pos) {
    RwInt32 id = 0;
    RpPVSCache* PVSCache;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(wpWorld);
    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(pos);

    lbl_80517780.World = wpWorld;
    PVSCache = RPPVS_WORLD_DATA(lbl_80517780.World);

    lbl_80517780.ViewPos = *pos;
    lbl_80517780.InSector = -1;
    lbl_80517780.CurrPVS = NULL;

    id = 0;
    if (lbl_80517780.InSector == -1) {
        RpWorldForAllWorldSectors(wpWorld, GetEntireSector, &id);
    }

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT((lbl_80517780.InSector >= 0) && (lbl_80517780.InSector < PVSCache->NumWorldSectors));

    return (RpWorldSector*)((RwUInt8*)lbl_80517780.CurrPVS - lbl_80517780.sectorOffset);
} // 0x802C73F0

/*
 * ==================== RpPVSWorldSectorVisible ====================
 * Tests whether spSect is marked visible in the current view sector's vismap.
 * no copy in retail (inlined)
 */
RwBool RpPVSWorldSectorVisible(RpWorldSector* spSect) {
    RpPVS* pvsCur;
    RpPVS* ppOther;
    RwBool visible;

    pvsCur = RPPVS_SECTOR_DATA(spSect);
    ppOther = lbl_80517780.CurrPVS;
    visible = (ppOther->vismap[pvsCur->sectorID >> 3] & (1 << (pvsCur->sectorID & 7))) != 0;

    return visible;
}

/*
 * ==================== PVSWorldConstructor ====================
 * Initializes the RpPVSCache plugin extension on a newly created world.
 */
static void* PVSWorldConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpWorld* wpWorld = (RpWorld*)object;
    RpPVSCache* pvsCache;

    (void)offsetInObject;
    (void)sizeInObject;

    pvsCache = RPPVS_WORLD_DATA(wpWorld);

    pvsCache->hooked = FALSE;
    pvsCache->nextID = 0;
    pvsCache->progressCallBack = NULL;
    pvsCache->ptotal = 0;
    pvsCache->paccept = 0;
    pvsCache->formatted = FALSE;
    pvsCache->processed = FALSE;
    pvsCache->viscount = 0;
    pvsCache->polygons = NULL;
    pvsCache->renderCallBack = NULL;

    lbl_80517780.World = wpWorld;
    lbl_80517780.collis = TRUE;
    lbl_80517780.bfc = TRUE;
    lbl_80517780.InSector = -1;

    PVSInitializeSectors();

    return wpWorld;
} // 0x802C746C

/*
 * ==================== PVSWorldDestructor ====================
 * Unhooks and tears down PVS data on a world being destroyed.
 */
static void* PVSWorldDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpWorld* wpWorld = (RpWorld*)object;

    (void)offsetInObject;
    (void)sizeInObject;

    RpPVSUnhook(wpWorld);
    PVSDeinitialize(wpWorld);
    lbl_80517780.World = NULL;

    return wpWorld;
} // 0x802C7578

/*
 * ==================== PVSSectorConstructor ====================
 * Initializes the RpPVS plugin extension on a newly created world sector.
 */
static void* PVSSectorConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpWorldSector* sector = (RpWorldSector*)object;
    RpPVS* pvsCur;

    (void)offsetInObject;
    (void)sizeInObject;

    pvsCur = RPPVS_SECTOR_DATA(sector);

    pvsCur->sectorID = -1;
    pvsCur->sectailpoly = NULL;
    pvsCur->potential = rpPVSSPLIT;
    pvsCur->sampleKey = 0;
    pvsCur->vismap = NULL;
    pvsCur->vismaplength = 0;
    pvsCur->numpols = 0;

    return sector;
} // 0x802C7638

/*
 * ==================== PVSSectorDestructor ====================
 * Frees the vismap buffer on a world sector being destroyed.
 */
static void* PVSSectorDestructor(void* sector, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpPVS* pvsCur;

    (void)offsetInObject;
    (void)sizeInObject;

    pvsCur = RPPVS_SECTOR_DATA(sector);

    pvsCur->sectorID = -1;
    pvsCur->sampleKey = 0;
    if (pvsCur->vismap != NULL) {
        RwFree(pvsCur->vismap);
        pvsCur->vismap = NULL;
    }

    return sector;
} // 0x802C7670

/*
 * ==================== RpPVSHook ====================
 * Hooks a world's sector render callback to cull sectors using its PVS data.
 */
RpWorld* RpPVSHook(RpWorld* wpWorld) {
    RpPVSCache* pvsCache;

    pvsCache = RPPVS_WORLD_DATA(wpWorld);
    if ((pvsCache->processed) && (!pvsCache->hooked)) {
        pvsCache->hooked = TRUE;
        pvsCache->renderCallBack = RpWorldGetSectorRenderCallBack(wpWorld);
        RpWorldSetSectorRenderCallBack(wpWorld, PVSWorldSectorRenderCallBack);
    }

    return wpWorld;
} // 0x802C76EC

/*
 * ==================== RpPVSUnhook ====================
 * Restores the world's original sector render callback if PVS was hooked.
 */
RpWorld* RpPVSUnhook(RpWorld* wpWorld) {
    RpPVSCache* pvsCache;

    pvsCache = RPPVS_WORLD_DATA(wpWorld);
    if (pvsCache->hooked) {
        pvsCache->hooked = FALSE;
        RpWorldSetSectorRenderCallBack(wpWorld, pvsCache->renderCallBack);
        pvsCache->renderCallBack = NULL;
    }

    return wpWorld;
} // 0x802C7768

/*
 * ==================== PVSSectorGetChunkSize ====================
 * Returns the serialized size of a world sector's PVS chunk, or 0 if unprocessed.
 */
static RwInt32 PVSSectorGetChunkSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpPVS* pvsCur;

    (void)offsetInObject;
    (void)sizeInObject;

    pvsCur = RPPVS_SECTOR_DATA(object);
    if (pvsCur->sectorID == -1) {
        return 0;
    }

    return sizeof(RwInt32) * 3 + pvsCur->vismaplength;
} // 0x802C77D4

/*
 * ==================== PVSSectorWriteChunk ====================
 * Writes a world sector's PVS ID, vismap length, sample key, and vismap bytes to a stream.
 */
static RwStream* PVSSectorWriteChunk(RwStream* s, RwInt32 binaryLength, const void* object,
                                     RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpPVS* pvsCur;

    (void)binaryLength;
    (void)offsetInObject;
    (void)sizeInObject;

    pvsCur = RPPVS_SECTOR_DATA(object);
    RwStreamWriteInt32(s, &pvsCur->sectorID, sizeof(RwInt32));
    RwStreamWriteInt32(s, &pvsCur->vismaplength, sizeof(RwInt32));
    RwStreamWriteInt32(s, &pvsCur->sampleKey, sizeof(RwInt32));
    if (pvsCur->vismaplength != 0) {
        RwStreamWrite(s, pvsCur->vismap, pvsCur->vismaplength);
    }

    return s;
} // 0x802C7800

/*
 * ==================== PVSSectorReadChunk ====================
 * Reads a world sector's PVS ID, vismap length, sample key, and vismap bytes from a stream.
 */
static RwStream* PVSSectorReadChunk(RwStream* s, RwInt32 binaryLength, void* object,
                                    RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpPVS* pvsCur;

    (void)binaryLength;
    (void)offsetInObject;
    (void)sizeInObject;

    pvsCur = RPPVS_SECTOR_DATA(object);
    RwStreamReadInt32(s, &pvsCur->sectorID, sizeof(RwInt32));
    RwStreamReadInt32(s, &pvsCur->vismaplength, sizeof(RwInt32));
    RwStreamReadInt32(s, &pvsCur->sampleKey, sizeof(RwInt32));
    if (pvsCur->vismaplength != 0) {
        pvsCur->vismap = (RwUInt8*)RwMalloc(pvsCur->vismaplength, rwMEMHINTDUR_EVENT | rwID_PVSPLUGIN);
    }
    if (pvsCur->vismaplength != 0) {
        RwStreamRead(s, pvsCur->vismap, pvsCur->vismaplength);
    }

    return s;
} // 0x802C7888

/*
 * ==================== PVSWorldGetChunkSize ====================
 * Returns the serialized size of a world's PVS chunk, or 0 if unprocessed.
 */
static RwInt32 PVSWorldGetChunkSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpPVSCache* pvsCache;

    (void)offsetInObject;
    (void)sizeInObject;

    pvsCache = RPPVS_WORLD_DATA(object);
    if (!pvsCache->processed) {
        return 0;
    }

    return sizeof(RwInt32) * 2;
} // 0x802C7938

/*
 * ==================== PVSWorldWriteChunk ====================
 * Writes a world's PVS processed and hooked flags to a stream.
 */
static RwStream* PVSWorldWriteChunk(RwStream* s, RwInt32 binaryLength, const void* object,
                                    RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpPVSCache* pvsCache;

    (void)binaryLength;
    (void)offsetInObject;
    (void)sizeInObject;

    lbl_80517780.World = (RpWorld*)object;
    pvsCache = RPPVS_WORLD_DATA(object);
    RwStreamWriteInt32(s, &pvsCache->processed, sizeof(RwInt32));
    RwStreamWriteInt32(s, &pvsCache->hooked, sizeof(RwInt32));

    return s;
} // 0x802C795C

/*
 * ==================== PVSWorldReadChunk ====================
 * Reads a world's PVS flags from a stream and initializes sector bounds and axis tags.
 */
static RwStream* PVSWorldReadChunk(RwStream* s, RwInt32 binaryLength, void* object,
                                   RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpWorld* wpWorld = (RpWorld*)object;
    RpPVSCache* pvsCache;

    (void)binaryLength;
    (void)offsetInObject;
    (void)sizeInObject;

    pvsCache = RPPVS_WORLD_DATA(wpWorld);
    RwStreamReadInt32(s, &pvsCache->processed, sizeof(RwInt32));
    RwStreamReadInt32(s, &pvsCache->hooked, sizeof(RwInt32));
    pvsCache->hooked = FALSE;

    lbl_80517780.World = wpWorld;
    lbl_80517780.bfc = TRUE;
    PVSInitializeSectors();

    return s;
} // 0x802C79C0

/*
 * ==================== RpPVSPluginAttach ====================
 * Registers the PVS world and world-sector plugin extensions and stream callbacks.
 */
RwBool RpPVSPluginAttach(void) {
    RwInt32 offset;

    lbl_80517780.World = NULL;

    offset = RpWorldRegisterPlugin(sizeof(RpPVSCache), rwID_PVSPLUGIN, PVSWorldConstructor,
                                   PVSWorldDestructor, NULL);
    lbl_80517780.worldOffset = offset;
    if (offset < 0) {
        return FALSE;
    }

    if (RpWorldRegisterPluginStream(rwID_PVSPLUGIN, PVSWorldReadChunk, PVSWorldWriteChunk,
                                    PVSWorldGetChunkSize) < 0) {
        return FALSE;
    }

    offset = RpWorldSectorRegisterPlugin(sizeof(RpPVS), rwID_PVSPLUGIN, PVSSectorConstructor,
                                         PVSSectorDestructor, NULL);
    lbl_80517780.sectorOffset = offset;
    if (offset < 0) {
        return FALSE;
    }

    if (RpWorldSectorRegisterPluginStream(rwID_PVSPLUGIN, PVSSectorReadChunk, PVSSectorWriteChunk,
                                          PVSSectorGetChunkSize) < 0) {
        return FALSE;
    }

    return TRUE;
} // 0x802C7AB0
