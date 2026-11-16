// Unit name and boundary: Gormiti link map (RpMatFx.a multiTexGcnPipe.obj)
// Names and layout: Gormiti link map and DWARF (RpMatFx.a multiTexGcnPipe.obj), Final and Debug builds.
// RpMultiTextureGetGameCubeMTCallBacks: UNUSED in Gormiti's link map for this object; retail keeps it (name from the map).
/*
 * multiTexGcnPipe.c: the GameCube pipeline of RpMatFX's multitexture effects. It registers an engine plugin
 * that keeps the table of reference frames of the texture matrices, sets up the TEV stages of an effect, sends
 * the effect to the GX and cleans up after it, and keeps the pre- and post-render callbacks of a material's
 * multitexture.
 *
 * Not finished: GetTexFrameMatrix, _rpGameCubeMTEffectSend, _rpGameCubeMTEffectClean and
 * _rpGameCubeMTMeshRenderCallBack are still stubs. The unit stays NonMatching.
 */
#include <string.h>
#include <revolution/gx/GXBump.h>
#include <revolution/gx/GXTev.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
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

void GetTexFrameMatrix() {
    // TODO
} // 0x802C1D5C

void _rpGameCubeMTEffectSend() {
    // TODO
} // 0x802C1FD0

void _rpGameCubeMTEffectClean() {
    // TODO
} // 0x802C28CC

void _rpGameCubeMTMeshRenderCallBack() {
    // TODO
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
