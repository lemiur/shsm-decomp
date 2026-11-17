// Unit name and boundary: Gormiti link map (RpMatFx.a multiTexGcnPipe.obj)
// Names and layout: Gormiti link map and DWARF (RpMatFx.a multiTexGcnPipe.obj), Final and Debug builds.
// _rpGameCubeMTMeshRenderCallBack is unpaired with Gormiti (another revision): its callbacks take no object LTM; the retail assembly decides.
// _rpGameCubeMTEffectSend is paired only by sequence and size; texture matrix and TEV stage handling follow the retail assembly.
// The names of the flag bits of the records are invented (include/rwsdk/plugin/matfx/multitexgcn.h).
// RpMultiTextureGetGameCubeMTCallBacks: UNUSED in Gormiti's link map for this object; retail keeps it (name from the map).
/*
 * multiTexGcnPipe.c: the GameCube pipeline of RpMatFX's multitexture effects. It registers an engine plugin
 * that keeps the table of reference frames of the texture matrices, sets up the TEV stages of an effect, sends
 * the effect to the GX and cleans up after it, and keeps the pre- and post-render callbacks of a material's
 * multitexture.
 *
 * TEV stages, indirect stages and texture coordinates are numbered from where the engine's callbacks left off.
 */
#include <string.h>
#include <revolution/gx/GXBump.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/gx/GXLighting.h>
#include <revolution/gx/GXTexture.h>
#include <revolution/gx/GXTransform.h>
#include <revolution/mtx.h>
#include <revolution/gx/GXTev.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/driver/wii/dldevice.h>
#include <rwsdk/driver/wii/dlrendst.h>
#include <rwsdk/driver/wii/dltextur.h>
#include <rwsdk/world/climaxplatform.h>
#include <rwsdk/world/ibuffer.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/climaxcommon.h>
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>

/* The ID of the multitexture plugin, which the engine plugin of this unit also uses. */
#define MULTITEXTURE_PLUGIN_ID 0x129

/* Where the TEV stage of a multitexture effect takes the rasterised colour from. */
typedef enum _TevRasSrc {
    NATevRasSrc = 0,
    TEVRAS_NORM = 1,
    TEVRAS_CH0A0 = 2
} _TevRasSrc;

/*
 * What the engine plugin keeps: the reference frames of the texture matrices and how many there are (name
 * invented: Gormiti's DWARF records the plugin's data only through its offset).
 */
typedef struct rpGameCubeMTEngineData {
    RwUInt32 numFrames; /* 0x00 */
    RwFrame** frames;   /* 0x04 */
} rpGameCubeMTEngineData;

/* The compiler lays the zero-initialised small data out in reverse order of definition. */
/* Where the TEV stages of the effect being sent start: a pre-TEV callback may have used the first ones. */
static unsigned int s_TevBaseOffset;

/* The offset of the plugin's data in the engine. */
RwInt32 _rpGameCubeMTEngineOffset;

/*
 * ==================== GameCubeMTOpen ====================
 * Engine open callback: the plugin's data starts out cleared.
 */
static void* GameCubeMTOpen(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    memset(RWPLUGINOFFSET(rpGameCubeMTEngineData, RwEngineInstance, _rpGameCubeMTEngineOffset), 0,
           sizeof(rpGameCubeMTEngineData));

    return object;
} // 0x802C1BD4

/*
 * ==================== GameCubeMTClose ====================
 * Engine close callback: nothing to free.
 */
static void* GameCubeMTClose(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
} // 0x802C1C18

/*
 * ==================== GameCubeMTEffectSetupTevStage ====================
 * Sets one TEV stage of an effect up in the GX: its indirect part, its colour and alpha inputs, its
 * texture, coordinates and rasterised colour, its constant colour selections and its operations. A stage
 * that reads the rasterised colour of channel 0 or 1 reads channel 0's colour and alpha when the effect asks
 * for TEVRAS_CH0A0. Called by _rpGameCubeMTEffectSend: while that is a stub the compiler drops this static (exact when
 * compiled non-static).
 */
static int GameCubeMTEffectSetupTevStage(unsigned int index, RpGameCubeTevStage* stage, _TevRasSrc rasSrc) {
    GXTevStageID tevStageID;
    GXChannelID channelID;

    tevStageID = index;

    if (stage->indirect) {
        RpGameCubeTevInd ind;

        RpGameCubeTevIndUnpack(&ind, &stage->indirect);
        GXSetTevIndirect(tevStageID, ind.indStage, ind.format, ind.biasSel, ind.matrixSel, ind.wrapS, ind.wrapT,
                         ind.addPrev, ind.utcLod, ind.alphaSel);
    } else {
        GXSetTevDirect(tevStageID);
    }

    GXSetTevColorIn(tevStageID, stage->op.colorA, stage->op.colorB, stage->op.colorC, stage->op.colorD);
    GXSetTevAlphaIn(tevStageID, stage->op.alphaA, stage->op.alphaB, stage->op.alphaC, stage->op.alphaD);

    channelID = stage->channelID;
    if (channelID <= GX_COLOR1A1 && rasSrc == TEVRAS_CH0A0) {
        channelID = GX_COLOR0A0;
    }

    GXSetTevOrder(tevStageID, stage->texCoordID, stage->texMapID, channelID);
    GXSetTevKColorSel(tevStageID, stage->op.colorSel);
    GXSetTevKAlphaSel(tevStageID, stage->op.alphaSel);
    GXSetTevColorOp(tevStageID, stage->op.colorOp, stage->op.colorBias, stage->op.colorScale,
                    stage->op.colorClamp, stage->op.colorOutReg);
    GXSetTevAlphaOp(tevStageID, stage->op.alphaOp, stage->op.alphaBias, stage->op.alphaScale,
                    stage->op.alphaClamp, stage->op.alphaOutReg);

    return TRUE;
} // 0x802C1C1C

/*
 * ==================== GetTexFrameMatrix ====================
 * Returns the matrix a texture matrix is relative to, given the frame it follows. The camera and a spare
 * frame are relative to the object's LTM when there is one. The environment frame (MISC9) is the inverse
 * transpose of the object's LTM, taking the world to the camera, optionally with the reflection offset
 * appended. A matrix the caller's scratch matrix is built in is returned in scratch; NULL when the frame needs
 * no matrix.
 */
static RwMatrix* GetTexFrameMatrix(RpGameCubeTexFrameID refFrame, RwMatrix* objectLTM, RwMatrix* scratch) {
    RwMatrix* matrix;

    if (refFrame == rpGAMECUBETEXFRAME_MISC9) {
        if (objectLTM) {
            RwMatrix LTMInv;
            RwMatrix LTMInvTran;

            RwMatrixInvert(&LTMInv, objectLTM);

            LTMInvTran.right.x = LTMInv.right.x;
            LTMInvTran.right.y = LTMInv.up.x;
            LTMInvTran.right.z = LTMInv.at.x;
            LTMInvTran.up.x = LTMInv.right.y;
            LTMInvTran.up.y = LTMInv.up.y;
            LTMInvTran.up.z = LTMInv.at.y;
            LTMInvTran.at.x = LTMInv.right.z;
            LTMInvTran.at.y = LTMInv.up.z;
            LTMInvTran.at.z = LTMInv.at.z;

            RwV3dNormalize(&LTMInvTran.right, &LTMInvTran.right);
            RwV3dNormalize(&LTMInvTran.up, &LTMInvTran.up);
            RwV3dNormalize(&LTMInvTran.at, &LTMInvTran.at);

            LTMInvTran.pos.x = 0.0f;
            LTMInvTran.pos.y = 0.0f;
            LTMInvTran.pos.z = 0.0f;

            if (_Current_RwClimaxReflectionOffset) {
                RwMatrix Temp;

                RwMatrixMultiply(&Temp, &LTMInvTran, &_RwDlInvCamLTM);
                RwMatrixMultiply(scratch, &Temp, _Current_RwClimaxReflectionOffset);
            } else {
                RwMatrixMultiply(scratch, &LTMInvTran, &_RwDlInvCamLTM);
            }
            matrix = scratch;
        } else {
            if (_Current_RwClimaxReflectionOffset) {
                RwMatrixMultiply(scratch, &_RwDlInvCamLTM, _Current_RwClimaxReflectionOffset);
                matrix = scratch;
            } else {
                matrix = &_RwDlInvCamLTM;
            }
        }
    } else if (refFrame >= rpGAMECUBETEXFRAME_MISC0) {
        RwUInt32 index = refFrame - rpGAMECUBETEXFRAME_MISC0;
        RwUInt32 numFrames = RWPLUGINOFFSET(rpGameCubeMTEngineData, RwEngineInstance, _rpGameCubeMTEngineOffset)->numFrames;
        RwFrame** frames = RWPLUGINOFFSET(rpGameCubeMTEngineData, RwEngineInstance, _rpGameCubeMTEngineOffset)->frames;

        if (index < numFrames && frames && frames[index]) {
            if (objectLTM) {
                RwMatrix invLTM;

                RwMatrixInvert(&invLTM, RwFrameGetLTM(frames[index]));
                RwMatrixMultiply(scratch, objectLTM, &invLTM);
            } else {
                RwMatrixInvert(scratch, RwFrameGetLTM(frames[index]));
            }
            matrix = scratch;
        } else if (objectLTM) {
            matrix = objectLTM;
        } else {
            matrix = scratch;
            RwMatrixSetIdentity(matrix);
        }
    } else if (refFrame == rpGAMECUBETEXFRAME_CAMERA) {
        if (objectLTM) {
            RwMatrixMultiply(scratch, objectLTM, &_RwDlInvCamLTM);
            matrix = scratch;
        } else {
            matrix = &_RwDlInvCamLTM;
        }
    } else if (refFrame == rpGAMECUBETEXFRAME_WORLD) {
        if (objectLTM) {
            matrix = objectLTM;
        } else {
            matrix = NULL;
        }
    } else {
        matrix = NULL;
    }

    return matrix;
} // 0x802C1D5C

/*
 * ==================== _rpGameCubeMTEffectSend ====================
 * Sets an effect up in the GX: the TEV colour registers and constant colours, the environment map and texture
 * matrices, the indirect matrices and stages, the TEV stages, the texture coordinate generators, the depth
 * compare location and the blend functions. The pre- and post-TEV callbacks of the engine see what has been
 * set up, and may take the first TEV stages, indirect stages and texture coordinates for their own: the stages
 * of the effect then follow theirs. coordMap maps the texture maps the stages use to the texture coordinates the
 * mesh supplies. While the pre-TEV callback is installed, a stage that reads RASC is given the callback's
 * lighting source instead for as long as its stage is set up. texGenTexture is indexed by a stage's texture
 * coordinate whether or not the stage has one (the original's bug, kept).
 */
static RpMTEffect* _rpGameCubeMTEffectSend(RpMTEffect* effect, unsigned int numTextures, unsigned char* coordMap,
                                           RwMatrix* objectLTM, RxGameCubePipeData* pipeData) {
    int i;
    _TevRasSrc rasSrc;
    signed char texGenTexture[8];
    RpGameCubeMTConfig* config;
    StTevSetupData TevSetupData;

    config = RpGameCubeMTEffectGetConfig(effect);

    TevSetupData.Source = 2;
    TevSetupData.SourceParm = config->flags;
    TevSetupData.LightingSrc = GX_CC_RASC;
    TevSetupData.Count_TexGens = 0;
    TevSetupData.Count_IndTevs = 0;
    TevSetupData.Count_Tevs = 0;
    TevSetupData.pipeData = pipeData;

    if (_Current_RwClimaxRvlPreTevCB) {
        _Current_RwClimaxRvlPreTevCB(&TevSetupData);
    }

    if (config->flags & rpGAMECUBEMTCONFIG_REGS) {
        GXSetTevColorS10(GX_TEVREG0, *(GXColorS10*)&config->reg[0]);
        GXSetTevColorS10(GX_TEVREG1, *(GXColorS10*)&config->reg[1]);
        GXSetTevColorS10(GX_TEVREG2, *(GXColorS10*)&config->reg[2]);
        GXSetTevColorS10(GX_TEVPREV, *(GXColorS10*)&config->reg[3]);
    }

    if (config->flags & rpGAMECUBEMTCONFIG_KREGS) {
        GXSetTevKColor(GX_KCOLOR0, *(GXColor*)&config->kreg[0]);
        GXSetTevKColor(GX_KCOLOR1, *(GXColor*)&config->kreg[1]);
        GXSetTevKColor(GX_KCOLOR2, *(GXColor*)&config->kreg[2]);
        GXSetTevKColor(GX_KCOLOR3, *(GXColor*)&config->kreg[3]);
    }

    if (config->flags & rpGAMECUBEMTCONFIG_ENVMAP) {
        RwMatrix tmpMatrix;
        RwMatrix tmpMatrix2;
        RwMatrix* matrix;
        float texMtx[3][4];

        if (objectLTM) {
            matrix = RwMatrixMultiply(&tmpMatrix, objectLTM, &_RwDlInvCamLTM);
        } else {
            matrix = &_RwDlInvCamLTM;
        }

        if (_Current_RwClimaxReflectionOffset) {
            RwMatrixMultiply(&tmpMatrix2, matrix, _Current_RwClimaxReflectionOffset);
            matrix = &tmpMatrix2;
        }

        texMtx[0][0] = -0.5f * matrix->right.x;
        texMtx[0][1] = -0.5f * matrix->up.x;
        texMtx[0][2] = -0.5f * matrix->at.x;
        texMtx[0][3] = 0.5f;
        texMtx[1][0] = -0.5f * matrix->right.y;
        texMtx[1][1] = -0.5f * matrix->up.y;
        texMtx[1][2] = -0.5f * matrix->at.y;
        texMtx[1][3] = 0.5f;

        GXLoadTexMtxImm(texMtx, GX_TEXMTX0, GX_MTX2x4);
    }

    for (i = 0; i < config->numTexMtx; i++) {
        RpGameCubeTexMtx* texMtx = &config->texMtx[i];
        RwMatrix temp;
        RwMatrix* matrix;

        matrix = GetTexFrameMatrix(texMtx->refFrame, objectLTM, &temp);
        if (matrix) {
            float mtx[3][4];
            float mtx2[3][4];

            mtx[0][0] = matrix->right.x;
            mtx[0][1] = matrix->up.x;
            mtx[0][2] = matrix->at.x;
            mtx[1][0] = matrix->right.y;
            mtx[1][1] = matrix->up.y;
            mtx[1][2] = matrix->at.y;
            mtx[2][0] = matrix->right.z;
            mtx[2][1] = matrix->up.z;
            mtx[2][2] = matrix->at.z;

            if (texMtx->flags & rpGAMECUBETEXMTX_TRANSLATION) {
                mtx[0][3] = matrix->pos.x;
                mtx[1][3] = matrix->pos.y;
                mtx[2][3] = matrix->pos.z;
            } else {
                mtx[0][3] = 0.0f;
                mtx[1][3] = 0.0f;
                mtx[2][3] = 0.0f;
            }

            PSMTXConcat(texMtx->data, mtx, mtx2);
            GXLoadTexMtxImm(mtx2, texMtx->offset, (texMtx->flags & rpGAMECUBETEXMTX_2X4) ? GX_MTX2x4 : GX_MTX3x4);
        } else {
            GXLoadTexMtxImm(texMtx->data, texMtx->offset,
                            (texMtx->flags & rpGAMECUBETEXMTX_2X4) ? GX_MTX2x4 : GX_MTX3x4);
        }
    }

    for (i = 0; i < config->numIndMtx; i++) {
        RpGameCubeIndMtx* indMtx = &config->indMtx[i];
        RwMatrix scratch;
        RwMatrix* matrix;
        float (*pMtx)[3];
        float mtx[2][3];

        matrix = GetTexFrameMatrix(indMtx->refFrame, objectLTM, &scratch);
        if (matrix) {
            RwV3d* u;
            RwV3d* v;

            u = (RwV3d*)indMtx->data[0];
            v = (RwV3d*)indMtx->data[1];

            if (indMtx->refFrame != rpGAMECUBETEXFRAME_MISC9) {
                mtx[0][0] = RwV3dDotProductMacro(u, &matrix->right);
                mtx[0][1] = RwV3dDotProductMacro(u, &matrix->up);
                mtx[0][2] = RwV3dDotProductMacro(u, &matrix->at);
                mtx[1][0] = RwV3dDotProductMacro(v, &matrix->right);
                mtx[1][1] = RwV3dDotProductMacro(v, &matrix->up);
                mtx[1][2] = RwV3dDotProductMacro(v, &matrix->at);
            } else {
                mtx[0][0] = matrix->right.x * u->x;
                mtx[0][1] = -1.0f * matrix->at.x * u->y;
                mtx[0][2] = matrix->up.x * u->z;
                mtx[1][0] = matrix->right.y * v->x;
                mtx[1][1] = -1.0 * matrix->at.y * v->y;
                mtx[1][2] = matrix->up.y * v->z;
            }

            pMtx = mtx;
        } else {
            pMtx = indMtx->data;
        }

        GXSetIndTexMtx((GXIndTexMtxID)indMtx->id, pMtx, (s8)indMtx->scale);
    }

    if (config->flags & rpGAMECUBEMTCONFIG_RASCH0A0) {
        rasSrc = TEVRAS_CH0A0;
    } else {
        rasSrc = TEVRAS_NORM;
    }

    for (i = 0; i < config->numTexGens; i++) {
        texGenTexture[i] = -1;
    }

    if (config->numIndStages) {
        GXSetNumIndStages(config->numIndStages);

        for (i = 0; i < config->numIndStages; i++) {
            RpGameCubeIndStage* stage = &config->indStages[i];

            if (!(stage->flags & rpGAMECUBEINDSTAGE_DISABLED)) {
                GXSetIndTexOrder((GXIndTexStageID)(i + TevSetupData.Count_IndTevs), (GXTexCoordID)stage->texCoordID,
                                 (GXTexMapID)stage->texMapID);
                GXSetIndTexCoordScale((GXIndTexStageID)(i + TevSetupData.Count_IndTevs), (GXIndTexScale)stage->scaleS,
                                      (GXIndTexScale)stage->scaleT);
                texGenTexture[stage->texCoordID] = stage->texMapID;
            }
        }
    }

    if (config->numTevStages) {
        s_TevBaseOffset = TevSetupData.Count_Tevs;

        if (_Current_RwClimaxRvlPreTevCB) {
            for (i = 0; i < config->numTevStages; i++) {
                RpGameCubeTevStage* stage = &config->tevStages[i];

                if (!(stage->flags & rpGAMECUBETEVSTAGE_DISABLED)) {
                    RpGXTevColorArg A, B, C, D;
                    RpGXTexCoordID texCoordID;
                    unsigned int Indirect;

                    Indirect = stage->indirect;
                    if (Indirect) {
                        stage->indirect = (Indirect & ~rpGAMECUBETEVIND_STAGEMASK) |
                                          ((Indirect & rpGAMECUBETEVIND_STAGEMASK) + TevSetupData.Count_IndTevs);
                    }

                    texCoordID = stage->texCoordID;
                    if (texCoordID != rpGX_TEXCOORD_NULL) {
                        stage->texCoordID = (RpGXTexCoordID)(texCoordID + TevSetupData.Count_TexGens);
                    }

                    A = stage->op.colorA;
                    if (A == rpGX_CC_RASC) {
                        stage->op.colorA = (RpGXTevColorArg)TevSetupData.LightingSrc;
                    }
                    B = stage->op.colorB;
                    if (B == rpGX_CC_RASC) {
                        stage->op.colorB = (RpGXTevColorArg)TevSetupData.LightingSrc;
                    }
                    C = stage->op.colorC;
                    if (C == rpGX_CC_RASC) {
                        stage->op.colorC = (RpGXTevColorArg)TevSetupData.LightingSrc;
                    }
                    D = stage->op.colorD;
                    if (D == rpGX_CC_RASC) {
                        stage->op.colorD = (RpGXTevColorArg)TevSetupData.LightingSrc;
                    }

                    GameCubeMTEffectSetupTevStage(TevSetupData.Count_Tevs, stage, rasSrc);

                    stage->indirect = Indirect;
                    stage->texCoordID = texCoordID;
                    stage->op.colorA = A;
                    stage->op.colorB = B;
                    stage->op.colorC = C;
                    stage->op.colorD = D;

                    texGenTexture[stage->texCoordID] = stage->texMapID;
                }
                TevSetupData.Count_Tevs++;
            }
        } else {
            for (i = 0; i < config->numTevStages; i++) {
                RpGameCubeTevStage* stage = &config->tevStages[i];

                if (!(stage->flags & rpGAMECUBETEVSTAGE_DISABLED)) {
                    GameCubeMTEffectSetupTevStage(TevSetupData.Count_Tevs, stage, rasSrc);

                    texGenTexture[stage->texCoordID] = stage->texMapID;
                }
                TevSetupData.Count_Tevs++;
            }
        }
    }

    for (i = 0; i < config->numTexGens; i++) {
        RpGameCubeTexGen* texGen = &config->texGens[i];

        if (!(texGen->flags & rpGAMECUBETEXGEN_DISABLED)) {
            GXTexGenSrc src;

            src = (GXTexGenSrc)texGen->srcParam;
            if (coordMap && texGen->srcParam >= rpGX_TG_TEX0 && texGen->srcParam <= rpGX_TG_TEX7 &&
                texGenTexture[i] >= 0) {
                src = (GXTexGenSrc)(coordMap[texGenTexture[i]] + GX_TG_TEX0);
            }

            GXSetTexCoordGen2((GXTexCoordID)(i + TevSetupData.Count_TexGens), (GXTexGenType)texGen->func, src,
                              texGen->mtx, (texGen->flags & rpGAMECUBETEXGEN_NORMALIZE) >> 1, texGen->postMtx);

            if (texGen->flags & rpGAMECUBETEXGEN_SCALE) {
                GXSetTexCoordScaleManually((GXTexCoordID)i, GX_TRUE, texGen->scaleS, texGen->scaleT);
            }
        }
    }
    TevSetupData.Count_TexGens += config->numTexGens;

    if (config->flags & rpGAMECUBEMTCONFIG_ZCOMPLOC) {
        _rwDlRenderStateSetZCompLoc(0);
    } else {
        _rwDlRenderStateSetZCompLoc(1);
    }

    if (config->flags & rpGAMECUBEMTCONFIG_BLEND) {
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)config->srcBlend);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)config->destBlend);
    }

    TevSetupData.Count_IndTevs += config->numIndStages;

    if (_Current_RwClimaxRvlPostTevCB) {
        _Current_RwClimaxRvlPostTevCB(&TevSetupData);
    }

    GXSetNumTevStages(TevSetupData.Count_Tevs);
    GXSetNumTexGens(TevSetupData.Count_TexGens);
    GXSetNumIndStages(TevSetupData.Count_IndTevs);

    return effect;
} // 0x802C1FD0

/*
 * ==================== _rpGameCubeMTEffectClean ====================
 * Undoes what _rpGameCubeMTEffectSend set up in the GX that the next draw would otherwise inherit: the manual
 * scales of the texture coordinates, the indirect stages and their scales, and the TEV stages' indirect parts.
 */
static void _rpGameCubeMTEffectClean(RpMTEffect* effect) {
    RpGameCubeMTConfig* config;
    int i;

    config = RpGameCubeMTEffectGetConfig(effect);

    for (i = 0; i < config->numTexGens; i++) {
        if (config->texGens[i].flags & rpGAMECUBETEXGEN_SCALE) {
            GXSetTexCoordScaleManually((GXTexCoordID)i, GX_FALSE, 0, 0);
        }
    }

    if (config->numIndStages) {
        GXSetNumIndStages(0);

        for (i = 0; i < config->numIndStages; i++) {
            GXSetIndTexCoordScale((GXIndTexStageID)i, GX_ITS_1, GX_ITS_1);
        }

        for (i = 0; i < config->numTevStages; i++) {
            if (config->tevStages[i].indirect) {
                GXSetTevDirect((GXTevStageID)(i + s_TevBaseOffset));
            }
        }
    }
} // 0x802C28CC

/*
 * ==================== _rpGameCubeMTMeshRenderCallBack ====================
 * The mesh render callback of the multitexture pipeline: binds the multitexture's textures, runs the material's
 * pre-render callback, sets the material colour and the effect up and calls the display list, then runs the
 * post-render callback and restores the blend functions. A material that is not opaque has its TEV stages reduced
 * to the alpha of its colour. Unpaired: retail's callbacks are not given the object's LTM, which Gormiti's are.
 */
void _rpGameCubeMTMeshRenderCallBack(RxGameCubeDisplayList* dList, RpMaterial* material, void* object,
                                            RxGameCubePipeData* pipeData, RwMatrix* objectLTM) {
    RpMultiTexture* mt;
    rpMultiTextureGameCubeExt* mtExt;
    RwBlendFunction srcBlend;
    RwBlendFunction dstBlend;
    int i;
    RpGameCubeMTConfig* config;

    mt = RpMaterialGetMultiTexture(material, rwID_GAMECUBE);
    mtExt = (rpMultiTextureGameCubeExt*)mt->extension;

    RwRenderStateGet(rwRENDERSTATESRCBLEND, &srcBlend);
    RwRenderStateGet(rwRENDERSTATEDESTBLEND, &dstBlend);

    for (i = 0; i < mt->numTextures; i++) {
        _rwDlTextureSet(mt->textures[i], i);
    }

    if (mtExt->preRenderCallBack) {
        mtExt->preRenderCallBack(material, object, pipeData, mtExt->callBackData);
    }

    GXSetChanMatColor(GX_COLOR0A0, *(GXColor*)&material->color);

    if (mt->effect) {
        _rpGameCubeMTEffectSend(mt->effect, mt->numTextures, mt->coordMap, objectLTM, pipeData);

        if (material->color.alpha < 255) {
            config = RpGameCubeMTEffectGetConfig(mt->effect);

            GXSetTevKColor(GX_KCOLOR0, *(GXColor*)&material->color);

            for (i = 0; i < config->numTevStages - 1; i++) {
                GXSetTevAlphaIn((GXTevStageID)i, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
            }

            GXSetTevAlphaIn((GXTevStageID)(config->numTevStages - 1), GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
                            GX_CA_KONST);
            GXSetTevKAlphaSel((GXTevStageID)(config->numTevStages - 1), GX_TEV_KASEL_K0_A);
            GXSetTevAlphaOp((GXTevStageID)(config->numTevStages - 1), GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE,
                            GX_TEVPREV);
        }

        fn_802AEDC8(dList->displayList, dList->size);
        _rpGameCubeMTEffectClean(mt->effect);
    } else {
        fn_802AEDC8(dList->displayList, dList->size);
    }

    if (mtExt->postRenderCallBack) {
        mtExt->postRenderCallBack(material, object, pipeData, mtExt->callBackData);
    }

    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)srcBlend);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)dstBlend);
} // 0x802C29C8

/*
 * ==================== _rpGameCubeMTPipePluginAttach ====================
 * Registers the engine plugin of the pipeline. Returns FALSE when the engine has no room for it.
 */
RwBool _rpGameCubeMTPipePluginAttach(void) {
    _rpGameCubeMTEngineOffset = RwEngineRegisterPlugin(sizeof(rpGameCubeMTEngineData), MULTITEXTURE_PLUGIN_ID,
                                                       GameCubeMTOpen, GameCubeMTClose);
    if (_rpGameCubeMTEngineOffset < 0) {
        return FALSE;
    }

    return TRUE;
} // 0x802C2BE4

/*
 * ==================== RpMultiTextureSetGameCubeMTCallBacks ====================
 * Sets the callbacks a material's multitexture calls before and after its pipeline stage, and the data they are
 * given.
 */
RpMultiTexture* RpMultiTextureSetGameCubeMTCallBacks(RpMultiTexture* multiTexture,
                                                     RpMultiTextureGameCubeCallBack preRenderCallBack,
                                                     RpMultiTextureGameCubeCallBack postRenderCallBack,
                                                     void* callBackData) {
    rpMultiTextureGameCubeExt* ext;

    ext = (rpMultiTextureGameCubeExt*)multiTexture->extension;
    ext->preRenderCallBack = preRenderCallBack;
    ext->postRenderCallBack = postRenderCallBack;
    ext->callBackData = callBackData;

    return multiTexture;
} // 0x802C2C28

/*
 * ==================== RpMultiTextureGetGameCubeMTCallBacks ====================
 * Reads the callbacks of RpMultiTextureSetGameCubeMTCallBacks into the places given; a NULL place is skipped.
 */
RpMultiTexture* RpMultiTextureGetGameCubeMTCallBacks(RpMultiTexture* multiTexture,
                                                     RpMultiTextureGameCubeCallBack* preRenderCallBack,
                                                     RpMultiTextureGameCubeCallBack* postRenderCallBack,
                                                     void** callBackData) {
    rpMultiTextureGameCubeExt* ext;

    ext = (rpMultiTextureGameCubeExt*)multiTexture->extension;
    if (preRenderCallBack) {
        *preRenderCallBack = ext->preRenderCallBack;
    }
    if (postRenderCallBack) {
        *postRenderCallBack = ext->postRenderCallBack;
    }
    if (callBackData) {
        *callBackData = ext->callBackData;
    }

    return multiTexture;
} // 0x802C2C3C
