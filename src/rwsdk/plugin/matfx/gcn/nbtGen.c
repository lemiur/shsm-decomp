// Unit name and boundary: Gormiti link map (RpMatFx.a nbtGen.obj, source plugin\matfx\gcn\nbtGen.c)
// Names and layout: Gormiti link map and DWARF (RpMatFx.a nbtGen.obj), Final and Debug builds.
/*
 * nbtGen.c: the normal-binormal-tangent generator of RpMatFX's GameCube multitexture pipeline. A
 * bump mapped effect needs, for every vertex, the tangent frame of the triangle it belongs to;
 * this file walks the display list of an instanced vertex buffer and gathers each triangle's
 * positions, normals and texture coordinates (the *DataSetup functions), then queries whether a
 * mesh needs the vectors at all.
 */
/*
 * UNIT STATE (NonMatching). CalcMeshNBTs (0x802C32E0, 0x964 bytes, the only caller of CalcNBTSetup,
 * the four *DataSetup functions and CalcNBT) is not reconstructed, so Gormiti's static functions
 * are written without `static` here: the compiler would drop them as unused.
 *  - CalcNBTSetup and CalcMeshNBTs read and write GQR5 to GQR7 (mfspr / mtspr) in the middle of
 *    compiled C; no existing helper spells that, so both are parked (CalcNBTSetup has its C body,
 *    CalcMeshNBTs has none).
 *  - The four *DataSetup functions are complete bodies parked at a register/schedule wall
 *    (88%): retail keeps i1 and i2 in r12 and r28, the compiler picks r28 and r31, and orders
 *    the pos/tex/nrm stores differently. Ruled out: statement order of the seven stores and
 *    their operand order (levers.exe depth 3-4), the swap temporary, the compare of itemp.
 */
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>
#include <rwsdk/plugin/matfx/nbtgen.h>
#include <rwsdk/world/ibuffer.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/os/OSCache.h>

/* What the *DataSetup functions gather of one triangle: where each corner's attributes lie. */
typedef struct _rwDlNBTDataTag {
    void* pos0;                     /* 0x00 */
    void* pos1;                     /* 0x04 */
    void* pos2;                     /* 0x08 */
    void* nrm0;                     /* 0x0C */
    void* tex0;                     /* 0x10 */
    void* tex1;                     /* 0x14 */
    void* tex2;                     /* 0x18 */
    unsigned char posCompSize;      /* 0x1C */
    unsigned char nrmCompSize;      /* 0x1D */
    unsigned char texCompSize;      /* 0x1E */
    unsigned char pad;              /* 0x1F */
} _rwDlNBTDataTag;

/* The pass over one mesh's display list that computes the vectors (not reconstructed yet, see the end of this file). */extern void CalcMeshNBTs(RxGameCubeVertexBuffer* vbHeader, RxGameCubeDisplayList* dList, RpGameCubeVtxFmt* vtxFmt);
/*
 * ==================== CalcNBTSetup ====================
 * Works out how large the position, normal and texture coordinate components of a vertex are
 * and the quantisation registers (GQR5 for positions, GQR6 for normals, GQR7 for texture
 * coordinates) that read them as paired singles; GQR5 is returned in gqr5Val to be restored.
 * Without a vertex format, positions and normals take four bytes and the registers are cleared.
 * The texture coordinate size is left alone in that case.
 *
 * PARKED, not matched: the original reads GQR5 and writes GQR5 to GQR7 with inline assembly
 * (mfspr / mtspr, Gormiti's tmpReg and gqr5ValReg are its operands); no existing helper
 * spells those, so those four statements are missing and the function does not compile to
 * retail. The rest of the body is the retail data flow.
 */
void CalcNBTSetup(RpGameCubeVtxFmt* vtxFmt, unsigned char* posSize, unsigned char* nrmSize,
                         unsigned char* texSize, unsigned int* gqr5Val) {
    unsigned int tmpReg;
    unsigned int posGQR;
    unsigned int nrmGQR;
    unsigned int texGQR;
    unsigned int* gqr5ValReg;

    if (vtxFmt != NULL) {
        unsigned char vtxFmtTypeConvTable[5] = {4, 6, 5, 7, 0};
        unsigned char vtxFmtSizeConvTable[5] = {1, 1, 2, 2, 4};
        unsigned char vtxFmtNormFracTable[5] = {0, 6, 0, 14, 0};

        *posSize = vtxFmtSizeConvTable[vtxFmt->pos];
        posGQR = vtxFmtTypeConvTable[vtxFmt->pos] | (vtxFmt->posFrac << 8);
        posGQR |= posGQR << 16;

        *nrmSize = vtxFmtSizeConvTable[vtxFmt->norm];
        nrmGQR = vtxFmtTypeConvTable[vtxFmt->norm] | (vtxFmtNormFracTable[vtxFmt->norm] << 8);
        nrmGQR |= nrmGQR << 16;

        *texSize = vtxFmtSizeConvTable[vtxFmt->texCoord[0]];
        texGQR = vtxFmtTypeConvTable[vtxFmt->texCoord[0]] | (vtxFmt->texCoordFrac[0] << 8);
        texGQR |= texGQR << 16;
    } else {
        *posSize = 4;
        *nrmSize = 4;
        posGQR = 0;
        nrmGQR = 0;
        texGQR = 0;
    }

    gqr5ValReg = gqr5Val;
    /* mfspr tmpReg, GQR5; *gqr5ValReg = tmpReg; mtspr GQR5, posGQR; mtspr GQR6, nrmGQR; mtspr GQR7, texGQR */
} // 0x802C2C74

/*
 * ==================== TriStripNBTDataSetup8 ====================
 * Gathers the attributes of triangle i of a strip whose indices are bytes. Degenerate triangles
 * (two equal indices) are skipped, and odd triangles swap their last two corners to keep the
 * winding.
 */
void TriStripNBTDataSetup8(_rwDlNBTDataTag* nbtData, unsigned char* indices, unsigned int stride,
                                  unsigned char* pos, unsigned char* nbt, unsigned char* tex, int i) {
    unsigned char i0;
    unsigned char i1;
    unsigned char i2;
    unsigned char itemp;

    i0 = indices[stride * i];
    if (i < 2) {
        if (i == 0) {
            i1 = indices[stride];
            i2 = indices[stride * 2];
        } else {
            i2 = indices[0];
            i1 = indices[stride * 2];
        }
    } else {
        i1 = indices[stride * (i - 2)];
        i2 = indices[stride * (i - 1)];
        if (i1 == i2) {
            if (i0 == indices[stride * (i + 1)]) {
                i++;
                i1 = indices[stride * (i + 1)];
                if (i0 == i1) {
                    i++;
                    i1 = indices[stride * (i + 1)];
                }
                i2 = indices[stride * (i + 2)];
            } else {
                i1 = i2;
                i2 = indices[stride * (i + 1)];
            }
        }
        if (i & 1) {
            itemp = i2;
            i2 = i1;
            i1 = itemp;
        }
    }

    nbtData->pos0 = pos + i0 * 3 * nbtData->posCompSize;
    nbtData->tex0 = tex + i0 * (2 * nbtData->texCompSize);
    nbtData->pos1 = pos + i1 * 3 * nbtData->posCompSize;
    nbtData->pos2 = pos + i2 * 3 * nbtData->posCompSize;
    nbtData->nrm0 = nbt + i0 * 9 * nbtData->nrmCompSize;
    nbtData->tex1 = tex + i1 * (2 * nbtData->texCompSize);
    nbtData->tex2 = tex + i2 * (2 * nbtData->texCompSize);
} // 0x802C2D68

/*
 * ==================== TriStripNBTDataSetup16 ====================
 * TriStripNBTDataSetup8 for strips whose indices are halfwords.
 */
void TriStripNBTDataSetup16(_rwDlNBTDataTag* nbtData, unsigned char* indices, unsigned int stride,
                                   unsigned char* pos, unsigned char* nbt, unsigned char* tex, int i) {
    unsigned short i0;
    unsigned short i1;
    unsigned short i2;
    unsigned short itemp;

    i0 = *(unsigned short*)&indices[stride * i];
    if (i < 2) {
        if (i == 0) {
            i1 = *(unsigned short*)&indices[stride];
            i2 = *(unsigned short*)&indices[stride * 2];
        } else {
            i2 = *(unsigned short*)&indices[0];
            i1 = *(unsigned short*)&indices[stride * 2];
        }
    } else {
        i1 = *(unsigned short*)&indices[stride * (i - 2)];
        i2 = *(unsigned short*)&indices[stride * (i - 1)];
        if (i1 == i2) {
            if (i0 == *(unsigned short*)&indices[stride * (i + 1)]) {
                i++;
                i1 = *(unsigned short*)&indices[stride * (i + 1)];
                if (i0 == i1) {
                    i++;
                    i1 = *(unsigned short*)&indices[stride * (i + 1)];
                }
                i2 = *(unsigned short*)&indices[stride * (i + 2)];
            } else {
                i1 = i2;
                i2 = *(unsigned short*)&indices[stride * (i + 1)];
            }
        }
        if (i & 1) {
            itemp = i2;
            i2 = i1;
            i1 = itemp;
        }
    }

    nbtData->pos0 = pos + i0 * 3 * nbtData->posCompSize;
    nbtData->tex0 = tex + i0 * (2 * nbtData->texCompSize);
    nbtData->pos1 = pos + i1 * 3 * nbtData->posCompSize;
    nbtData->pos2 = pos + i2 * 3 * nbtData->posCompSize;
    nbtData->nrm0 = nbt + i0 * 9 * nbtData->nrmCompSize;
    nbtData->tex1 = tex + i1 * (2 * nbtData->texCompSize);
    nbtData->tex2 = tex + i2 * (2 * nbtData->texCompSize);
} // 0x802C2EE0

/*
 * ==================== TriListNBTDataSetup8 ====================
 * Gathers the attributes of vertex i of a triangle list whose indices are bytes: the vertex and
 * the other two of its triangle, by its place in the triangle.
 */
void TriListNBTDataSetup8(_rwDlNBTDataTag* nbtData, unsigned char* indices, unsigned int stride,
                                 unsigned char* pos, unsigned char* nbt, unsigned char* tex, int i) {
    unsigned char i0;
    unsigned char i1;
    unsigned char i2;

    i0 = indices[stride * i];
    switch (i % 3) {
    case 0:
        i1 = indices[stride * (i + 1)];
        i2 = indices[stride * (i + 2)];
        break;
    case 1:
        i1 = indices[stride * (i + 1)];
        i2 = indices[stride * (i - 1)];
        break;
    case 2:
        i1 = indices[stride * (i - 2)];
        i2 = indices[stride * (i - 1)];
        break;
    default:
        return;
    }

    nbtData->pos0 = pos + i0 * 3 * nbtData->posCompSize;
    nbtData->tex0 = tex + i0 * (2 * nbtData->texCompSize);
    nbtData->pos1 = pos + i1 * 3 * nbtData->posCompSize;
    nbtData->nrm0 = nbt + i0 * 9 * nbtData->nrmCompSize;
    nbtData->pos2 = pos + i2 * 3 * nbtData->posCompSize;
    nbtData->tex1 = tex + i1 * (2 * nbtData->texCompSize);
    nbtData->tex2 = tex + i2 * (2 * nbtData->texCompSize);
} // 0x802C3058

/*
 * ==================== TriListNBTDataSetup16 ====================
 * TriListNBTDataSetup8 for lists whose indices are halfwords.
 */
void TriListNBTDataSetup16(_rwDlNBTDataTag* nbtData, unsigned char* indices, unsigned int stride,
                                  unsigned char* pos, unsigned char* nbt, unsigned char* tex, int i) {
    unsigned short i0;
    unsigned short i1;
    unsigned short i2;

    i0 = *(unsigned short*)&indices[stride * i];
    switch (i % 3) {
    case 0:
        i1 = *(unsigned short*)&indices[stride * (i + 1)];
        i2 = *(unsigned short*)&indices[stride * (i + 2)];
        break;
    case 1:
        i1 = *(unsigned short*)&indices[stride * (i + 1)];
        i2 = *(unsigned short*)&indices[stride * (i - 1)];
        break;
    case 2:
        i1 = *(unsigned short*)&indices[stride * (i - 2)];
        i2 = *(unsigned short*)&indices[stride * (i - 1)];
        break;
    default:
        return;
    }

    nbtData->pos0 = pos + i0 * 3 * nbtData->posCompSize;
    nbtData->tex0 = tex + i0 * (2 * nbtData->texCompSize);
    nbtData->pos1 = pos + i1 * 3 * nbtData->posCompSize;
    nbtData->nrm0 = nbt + i0 * 9 * nbtData->nrmCompSize;
    nbtData->pos2 = pos + i2 * 3 * nbtData->posCompSize;
    nbtData->tex1 = tex + i1 * (2 * nbtData->texCompSize);
    nbtData->tex2 = tex + i2 * (2 * nbtData->texCompSize);
} // 0x802C319C

/*
 * ==================== _rpGameCubeMTPipeDataCalcNBTs ====================
 * Fills in the tangent frames of an instanced object that has a bump mapped effect. It first
 * marks the first component of every vertex's second vector (the nbt data that follows each
 * normal) with the largest value its format holds (0xFF, 0xFFFF or the largest float) so that vertices
 * the mesh passes leave untouched can be told, then runs CalcMeshNBTs over each mesh whose
 * effect has a bump generator, and flushes the normal array to memory.
 */
void _rpGameCubeMTPipeDataCalcNBTs(RxGameCubePipeData* pipeData, RpGameCubeVtxFmt* vtxFmt, int numVerts) {
    RxGameCubeVertexBuffer* vbHeader;
    RxGameCubeDisplayList* dList;
    RpMeshHeader* meshHeader;
    RpMesh* mesh;
    int i;

    vbHeader = (RxGameCubeVertexBuffer*)(pipeData->resEntry + 1);
    dList = (RxGameCubeDisplayList*)(&vbHeader->attr[vbHeader->numAttrArrays - 1] + 1);
    meshHeader = pipeData->meshHeader;

    if (vtxFmt != NULL) {
        if (vtxFmt->norm == 1) {
            unsigned char* nbt;

            nbt = (unsigned char*)vbHeader->attr[1].array;
            nbt += 3;

            for (i = 0; i < numVerts; i++) {
                *nbt = 0xFF;
                nbt += 9;
            }
        } else if (vtxFmt->norm == 3) {
            unsigned short* nbt;

            nbt = (unsigned short*)vbHeader->attr[1].array;
            nbt += 3;

            for (i = 0; i < numVerts; i++) {
                *nbt = 0xFFFF;
                nbt += 9;
            }
        } else {
            float* nbt;

            nbt = (float*)vbHeader->attr[1].array;
            nbt += 3;

            for (i = 0; i < numVerts; i++) {
                *nbt = 3.4028235e38f; /* FLT_MAX */
                nbt += 9;
            }
        }
    } else {
        float* nbt;

        nbt = (float*)vbHeader->attr[1].array;
        nbt += 3;

        for (i = 0; i < numVerts; i++) {
            *nbt = 3.4028235e38f; /* FLT_MAX */
            nbt += 9;
        }
    }

    mesh = (RpMesh*)(meshHeader + 1);
    for (i = 0; i < meshHeader->numMeshes; dList++, mesh++, i++) {
        RpMultiTexture* multiTexture = RpMaterialGetMultiTexture(mesh->material, rwID_GAMECUBE);

        if (multiTexture != NULL && multiTexture->effect != NULL &&
            multiTexture->effect->platformID == rwID_GAMECUBE) {
            RpGameCubeMTConfig* config = RpGameCubeMTEffectGetConfig(multiTexture->effect);
            int j;

            for (j = 0; j < config->numTexGens; j++) {
                RpGameCubeTexGen* texGen = &config->texGens[j];

                if (texGen->srcParam == rpGX_TG_BINRM || texGen->srcParam == rpGX_TG_TANGENT ||
                    (texGen->func >= rpGX_TG_BUMP0 && texGen->func <= rpGX_TG_BUMP7)) {
                    CalcMeshNBTs(vbHeader, dList, vtxFmt);
                    break;
                }
            }
        }
    }

    DCFlushRange(vbHeader->attr[1].array, numVerts * vbHeader->attr[1].stride);
    GXInvalidateVtxCache();
} // 0x802C3C44

/*
 * ==================== _rpGameCubeMTPipeDataQueryNBTs ====================
 * Whether a mesh of the object has a GameCube multitexture effect with a bump mapped texture
 * generator: one of the bump generators, or one that reads the binormal or the tangent.
 */
int _rpGameCubeMTPipeDataQueryNBTs(RxGameCubePipeData* pipeData) {
    RpMeshHeader* meshHeader;
    RpMesh* mesh;
    int i;

    meshHeader = pipeData->meshHeader;
    mesh = (RpMesh*)(meshHeader + 1);
    for (i = 0; i < meshHeader->numMeshes; i++, mesh++) {
        RpMultiTexture* multiTexture = RpMaterialGetMultiTexture(mesh->material, rwID_GAMECUBE);

        if (multiTexture != NULL && multiTexture->effect != NULL &&
            multiTexture->effect->platformID == rwID_GAMECUBE) {
            RpGameCubeMTConfig* config = RpGameCubeMTEffectGetConfig(multiTexture->effect);
            int j;

            for (j = 0; j < config->numTexGens; j++) {
                RpGameCubeTexGen* texGen = &config->texGens[j];

                if (texGen->srcParam == rpGX_TG_BINRM || texGen->srcParam == rpGX_TG_TANGENT ||
                    (texGen->func >= rpGX_TG_BUMP0 && texGen->func <= rpGX_TG_BUMP7)) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
} // 0x802C4030

/*
 * ==================== CalcNBT ====================
 * Computes the tangent frame of a triangle. Its body is empty in this build.
 */
void CalcNBT(_rwDlNBTDataTag* nbtData) {
} // 0x802C4110
