// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt and splits.txt (function names, unit layout)
/*
 * bamatlst.c: RenderWare material lists, the materials of a geometry or a world. The list holds
 * one reference to each material and grows in steps of rpMATERIALLISTGRANULARITY entries.
 * _rpMaterialListStreamGetSize and _rpMaterialListStreamWrite have no copy in retail
 * (dead-stripped).
 */
#include <rwsdk/world/rpplugin.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/bamatlst.h>

/* The number of entries a full material list grows by. Name invented. */
#define rpMATERIALLISTGRANULARITY 20

/*
 * Makes room for size materials in the list, keeping the ones it holds; NULL if the memory runs
 * out. Name invented; no copy in retail (inlined into _rpMaterialListStreamRead).
 */
static RpMaterialList* MaterialListSetSize(RpMaterialList* matList, RwInt32 size) {
    if (matList->space < size) {
        RpMaterial** materials;
        RwUInt32 memSize = size * sizeof(RpMaterial*);

        if (matList->materials != NULL) {
            materials = (RpMaterial**)RwRealloc(matList->materials, memSize,
                                                rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT | rwID_MATLIST);
        } else {
            materials = (RpMaterial**)RwMalloc(memSize, rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT | rwID_MATLIST);
        }

        if (materials == NULL) {
            RWERROR((E_RW_NOMEM, memSize));
            return NULL;
        }

        matList->materials = materials;
        matList->space = size;
    }

    return matList;
}

/* Drops the list's reference to each of its materials and frees the list's array. */
RpMaterialList* _rpMaterialListDeinitialize(RpMaterialList* matList) {
    RpMaterial** materials = matList->materials;

    if (materials != NULL) {
        RwInt32 numMaterials = matList->numMaterials;
        RwInt32 i;

        for (i = 0; i < numMaterials; i++) {
            RpMaterialDestroy(materials[i]);
            materials[i] = NULL;
        }

        RwFree(materials);
        matList->materials = NULL;
    }

    matList->numMaterials = 0;
    matList->space = 0;

    return matList;
} // 0x802D46E8

/* Empties the list, without memory. */
RpMaterialList* _rpMaterialListInitialize(RpMaterialList* matList) {
    matList->space = 0;
    matList->materials = NULL;
    matList->numMaterials = 0;

    return matList;
} // 0x802D4778

/* The material at matIndex in the list. */
RpMaterial* _rpMaterialListGetMaterial(const RpMaterialList* matList, RwInt32 matIndex) {
    return matList->materials[matIndex];
} // 0x802D478C

/* Adds a reference to the material at the end of the list, growing it if full; its index, or -1. */
RwInt32 _rpMaterialListAppendMaterial(RpMaterialList* matList, RpMaterial* material) {
    if (matList->space > matList->numMaterials) {
        matList->materials[matList->numMaterials] = material;
        RpMaterialAddRef(material);
        matList->numMaterials++;
        return matList->numMaterials - 1;
    } else {
        /* Full: grow the list by one step, then append. */
        RpMaterial** materials;
        RwUInt32 memSize = (matList->space + rpMATERIALLISTGRANULARITY) * sizeof(RpMaterial*);

        if (matList->materials != NULL) {
            materials = (RpMaterial**)RwRealloc(matList->materials, memSize,
                                                rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT | rwID_MATLIST);
        } else {
            materials = (RpMaterial**)RwMalloc(memSize, rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT | rwID_MATLIST);
        }

        if (materials == NULL) {
            RWERROR((E_RW_NOMEM, memSize));
            return -1;
        }

        matList->materials = materials;
        matList->space += rpMATERIALLISTGRANULARITY;

        matList->materials[matList->numMaterials] = material;
        RpMaterialAddRef(material);
        matList->numMaterials++;
        return matList->numMaterials - 1;
    }
} // 0x802D479C

/* The index of the material in the list (the last match), or -1 if the list does not hold it. */
RwInt32 _rpMaterialListFindMaterialIndex(const RpMaterialList* matList, const RpMaterial* material) {
    RwInt32 matIndex = matList->numMaterials;

    while (matIndex-- > 0) {
        if (matList->materials[matIndex] == material) {
            break;
        }
    }

    return matIndex;
} // 0x802D48D4

/*
 * Reads a material list from the stream: its size, then per entry either the index of an earlier
 * entry it repeats or a new material read from the stream.
 */
RpMaterialList* _rpMaterialListStreamRead(RwStream* stream, RpMaterialList* matList) {
    RwInt32 len;
    RwUInt32 length;
    RwUInt32 version;
    RwInt32* matIndices;
    RwBool status;
    RwInt32 i;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        status = (RwStreamReadInt32(stream, &len, sizeof(len)) != NULL);
        if (!status) {
            return NULL;
        }

        _rpMaterialListInitialize(matList);
        if (len == 0) {
            return matList;
        }

        if (MaterialListSetSize(matList, len) == NULL) {
            _rpMaterialListDeinitialize(matList);
            return NULL;
        }

        matIndices = (RwInt32*)RwMalloc(len * sizeof(RwInt32), rwMEMHINTDUR_FUNCTION | rwID_MATERIALMODULE);
        status = (RwStreamReadInt32(stream, matIndices, len * sizeof(RwInt32)) != NULL);
        if (!status) {
            RwFree(matIndices);
            _rpMaterialListDeinitialize(matList);
            return NULL;
        }

        for (i = 0; i < len; i++) {
            RpMaterial* material;

            if (matIndices[i] < 0) {
                /* A new material follows in the stream. */
                if (!RwStreamFindChunk(stream, rwID_MATERIAL, NULL, &version)) {
                    RwFree(matIndices);
                    _rpMaterialListDeinitialize(matList);
                    return NULL;
                }

                if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
                    material = RpMaterialStreamRead(stream);
                    if (material == NULL) {
                        RwFree(matIndices);
                        _rpMaterialListDeinitialize(matList);
                        return NULL;
                    }
                } else {
                    RWERROR((E_RW_BADVERSION));
                    RwFree(matIndices);
                    _rpMaterialListDeinitialize(matList);
                    return NULL;
                }
            } else {
                /* A repeat of an earlier entry. */
                material = _rpMaterialListGetMaterial(matList, matIndices[i]);
                RpMaterialAddRef(material);
            }

            /* The list takes its own reference; drop the one held here. */
            _rpMaterialListAppendMaterial(matList, material);
            RpMaterialDestroy(material);
        }

        RwFree(matIndices);

        return matList;
    }

    RWERROR((E_RW_BADVERSION));
    return NULL;
} // 0x802D4908
