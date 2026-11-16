// Unit name and boundary: Gormiti link map (RpMatFx.a multiTexGcnData.obj)
// Names and layout: Gormiti link map and DWARF (RpMatFx.a multiTexGcnData.obj), Final and Debug builds.
/*
 * multiTexGcnData.c: the GameCube data of RpMatFX's multitexture effects. An effect's TEV stages,
 * texture coordinate generators and matrices lie behind its header in one allocation; this file
 * creates them, streams them (the counts and colour registers first, then each array in turn)
 * and unpacks the indirect word of a TEV stage.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>

/* The error ID of the plugin (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_MATERIALEFFECTSPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/*
 * The library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the core's is
 * 0x65), so the chunks this unit writes carry that stamp.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

/* The ID of the multitexture plugin of a material, which stamps the effects' allocations. */
#define MULTITEXTURE_PLUGIN_ID 0x129

/* A size rounded up to a whole word. */
#define ALIGN4(n) (((n) + 3) & ~3)

/* The stream records: the in-memory structures with each field in one byte. */
typedef struct _StreamMTConfig {
    unsigned char numTexGens;
    unsigned char numTevStages;
    unsigned char flags;
    unsigned char blending;
    RpGXColorS10 reg[4];
    RwRGBA kreg[4];
    unsigned char numTexMtx;
    unsigned char numIndMtx;
    unsigned char numIndStages;
    unsigned char pad; // pad: Gormiti DWARF member name (_StreamMTConfig)
} _StreamMTConfig;

typedef struct _StreamTexGen {
    unsigned char flags;
    unsigned char func;
    unsigned char srcParam;
    unsigned char mtx;
    unsigned char postMtx;
    unsigned char pad[3]; // pad: Gormiti DWARF member name (_StreamTexGen)
    unsigned short scaleS;
    unsigned short scaleT;
} _StreamTexGen;

typedef struct _StreamTevStage {
    unsigned char colorA;
    unsigned char colorB;
    unsigned char colorC;
    unsigned char colorD;
    unsigned char alphaA;
    unsigned char alphaB;
    unsigned char alphaC;
    unsigned char alphaD;
    unsigned char colorOp;
    unsigned char colorBias;
    unsigned char colorScale;
    unsigned char colorClamp;
    unsigned char colorOutReg;
    unsigned char alphaOp;
    unsigned char alphaBias;
    unsigned char alphaScale;
    unsigned char alphaClamp;
    unsigned char alphaOutReg;
    unsigned char colorSel;
    unsigned char alphaSel;
    unsigned char texCoordID;
    unsigned char texMapID;
    unsigned char channelID;
    unsigned char flags;
    unsigned int indirect;
} _StreamTevStage;

typedef struct _StreamTexMtx {
    unsigned char offset;
    unsigned char refFrame;
    unsigned char flags;
    unsigned char pad; // pad: Gormiti DWARF member name (_StreamTexMtx)
} _StreamTexMtx;

typedef struct _StreamIndMtx {
    unsigned char id;
    unsigned char refFrame;
    unsigned char flags;
    signed char scale;
} _StreamIndMtx;

typedef struct _StreamIndStage {
    unsigned char flags;
    unsigned char texCoordID;
    unsigned char texMapID;
    unsigned char scaleS;
    unsigned char scaleT;
    unsigned char pad[3]; // pad: Gormiti DWARF member name (_StreamIndStage)
} _StreamIndStage;

/* A texture matrix follows its record in the stream as 3x4 floats, an indirect matrix as 2x3. */
#define STREAM_TEXMTX_REALS (3 * 4)
#define STREAM_INDMTX_REALS (2 * 3)

/* The stream keeps the texture map's disable flag (bit 8 of a texture map) in the flags byte. */
#define STREAM_TEVSTAGE_TEXDISABLE 0x80

/*
 * The GameCube data of an effect follows the effect's header (name invented; Gormiti's Debug build computes it
 * in place wherever this file needs it, without calling RpGameCubeMTEffectGetConfig).
 */
#define RpGameCubeMTEffectGetConfigMacro(_effect) ((RpGameCubeMTConfig*)((_effect) + 1))

/*
 * ==================== GameCubeMTEffectStreamGetSize ====================
 * The bytes the effect takes in a stream: the configuration, then every texture matrix, texture
 * coordinate generator, indirect matrix, indirect stage and TEV stage.
 */
static int GameCubeMTEffectStreamGetSize(const RpMTEffect* effect) {
    int size = 0;
    RpGameCubeMTConfig* config;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(effect);
    RWASSERT(effect->platformID == rwID_GAMECUBE);

    config = RpGameCubeMTEffectGetConfigMacro(effect);

    size += sizeof(_StreamMTConfig);
    size += config->numTexMtx * (sizeof(_StreamTexMtx) + STREAM_TEXMTX_REALS * sizeof(RwReal));
    size += config->numTexGens * sizeof(_StreamTexGen);
    size += config->numIndMtx * (sizeof(_StreamIndMtx) + STREAM_INDMTX_REALS * sizeof(RwReal));
    size += config->numIndStages * sizeof(_StreamIndStage);
    size += config->numTevStages * sizeof(_StreamTevStage);

    return size;
} // 0x802C10D8

/*
 * ==================== GameCubeMTEffectStreamWrite ====================
 * Writes the effect's chunk: the configuration, the texture coordinate generators, the TEV stages,
 * the texture matrices, the indirect matrices and the indirect stages. It returns NULL as soon as
 * a write fails. The effect is const: reads through it may move above the stores to the stream records.
 */
static const RpMTEffect* GameCubeMTEffectStreamWrite(const RpMTEffect* effect, RwStream* stream) {
    RpGameCubeMTConfig* config = RpGameCubeMTEffectGetConfigMacro(effect);
    unsigned int i;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(effect);
    RWASSERT(stream);
    RWASSERT(effect->platformID == rwID_GAMECUBE);

    if (!RwStreamWriteChunkHeader(stream, rwID_EXTENSION, GameCubeMTEffectStreamGetSize(effect))) {
        return NULL;
    }

    {
        _StreamMTConfig binConfig;

        binConfig.flags = config->flags;
        binConfig.numTevStages = config->numTevStages;
        binConfig.numTexGens = config->numTexGens;
        binConfig.numTexMtx = config->numTexMtx;
        binConfig.numIndMtx = config->numIndMtx;
        binConfig.numIndStages = config->numIndStages;
        binConfig.blending = ((config->srcBlend & 0xF) << 4) | (config->destBlend & 0xF);
        memcpy(binConfig.kreg, config->kreg, sizeof(config->kreg));
        memcpy(binConfig.reg, config->reg, sizeof(config->reg));
        RwMemLittleEndian16(binConfig.reg, sizeof(binConfig.reg));

        if (!RwStreamWrite(stream, &binConfig, sizeof(_StreamMTConfig))) {
            return NULL;
        }
    }

    for (i = 0; i < config->numTexGens; i++) {
        _StreamTexGen binTexGen;
        RpGameCubeTexGen* texGen = &config->texGens[i];

        binTexGen.flags = texGen->flags;
        binTexGen.func = texGen->func;
        binTexGen.srcParam = texGen->srcParam;
        binTexGen.mtx = texGen->mtx;
        binTexGen.postMtx = texGen->postMtx;
        binTexGen.pad[0] = 0;
        binTexGen.pad[1] = 0;
        binTexGen.pad[2] = 0;
        binTexGen.scaleS = texGen->scaleS;
        binTexGen.scaleT = texGen->scaleT;
        RwMemLittleEndian16(&binTexGen.scaleS, 2 * sizeof(unsigned short));

        if (!RwStreamWrite(stream, &binTexGen, sizeof(_StreamTexGen))) {
            return NULL;
        }
    }

    for (i = 0; i < config->numTevStages; i++) {
        _StreamTevStage binTevStage;
        RpGameCubeTevStage* tevStage = &config->tevStages[i];

        binTevStage.colorA = tevStage->op.colorA;
        binTevStage.colorB = tevStage->op.colorB;
        binTevStage.colorC = tevStage->op.colorC;
        binTevStage.colorD = tevStage->op.colorD;
        binTevStage.alphaA = tevStage->op.alphaA;
        binTevStage.alphaB = tevStage->op.alphaB;
        binTevStage.alphaC = tevStage->op.alphaC;
        binTevStage.alphaD = tevStage->op.alphaD;
        binTevStage.colorOp = tevStage->op.colorOp;
        binTevStage.colorBias = tevStage->op.colorBias;
        binTevStage.colorScale = tevStage->op.colorScale;
        binTevStage.colorClamp = tevStage->op.colorClamp;
        binTevStage.colorOutReg = tevStage->op.colorOutReg;
        binTevStage.alphaOp = tevStage->op.alphaOp;
        binTevStage.alphaBias = tevStage->op.alphaBias;
        binTevStage.alphaScale = tevStage->op.alphaScale;
        binTevStage.alphaClamp = tevStage->op.alphaClamp;
        binTevStage.alphaOutReg = tevStage->op.alphaOutReg;
        binTevStage.colorSel = tevStage->op.colorSel;
        binTevStage.alphaSel = tevStage->op.alphaSel;
        binTevStage.texCoordID = tevStage->texCoordID;
        binTevStage.texMapID = tevStage->texMapID;
        binTevStage.channelID = tevStage->channelID;
        binTevStage.flags = tevStage->flags;
        binTevStage.indirect = tevStage->indirect;
        RwMemLittleEndian32(&binTevStage.indirect, sizeof(binTevStage.indirect));

        if (tevStage->texMapID & rpGX_TEX_DISABLE) {
            binTevStage.flags |= STREAM_TEVSTAGE_TEXDISABLE;
        }

        if (!RwStreamWrite(stream, &binTevStage, sizeof(_StreamTevStage))) {
            return NULL;
        }
    }

    for (i = 0; i < config->numTexMtx; i++) {
        _StreamTexMtx binTexMtx;
        RpGameCubeTexMtx* texMtx = &config->texMtx[i];

        binTexMtx.offset = texMtx->offset;
        binTexMtx.refFrame = texMtx->refFrame;
        binTexMtx.flags = texMtx->flags;
        binTexMtx.pad = 0;

        if (!RwStreamWrite(stream, &binTexMtx, sizeof(_StreamTexMtx)) ||
            !RwStreamWriteReal(stream, &texMtx->data[0][0], sizeof(texMtx->data))) {
            return NULL;
        }
    }

    for (i = 0; i < config->numIndMtx; i++) {
        _StreamIndMtx binIndMtx;
        RpGameCubeIndMtx* indMtx = &config->indMtx[i];

        binIndMtx.id = indMtx->id;
        binIndMtx.refFrame = indMtx->refFrame;
        binIndMtx.flags = indMtx->flags;
        binIndMtx.scale = indMtx->scale;

        if (!RwStreamWrite(stream, &binIndMtx, sizeof(_StreamIndMtx)) ||
            !RwStreamWriteReal(stream, &indMtx->data[0][0], sizeof(indMtx->data))) {
            return NULL;
        }
    }

    for (i = 0; i < config->numIndStages; i++) {
        _StreamIndStage binIndStage;
        RpGameCubeIndStage* indStage = &config->indStages[i];

        binIndStage.flags = indStage->flags;
        binIndStage.texCoordID = indStage->texCoordID;
        binIndStage.texMapID = indStage->texMapID;
        binIndStage.scaleS = indStage->scaleS;
        binIndStage.scaleT = indStage->scaleT;
        binIndStage.pad[0] = 0;
        binIndStage.pad[1] = 0;
        binIndStage.pad[2] = 0;

        if (!RwStreamWrite(stream, &binIndStage, sizeof(_StreamIndStage))) {
            return NULL;
        }
    }

    return effect;
} // 0x802C1118

/*
 * ==================== GameCubeMTEffectStreamRead ====================
 * Reads an effect written above: creates it with the stream's counts and fills in the arrays. A
 * failed read destroys the half-built effect.
 */
static RpMTEffect* GameCubeMTEffectStreamRead(RwStream* stream, RwPlatformID, unsigned int, unsigned int) {
    RpMTEffect* effect;
    _StreamMTConfig binConfig;
    RpGameCubeMTConfig* config;
    unsigned int i;
    unsigned int size;

    size = sizeof(_StreamMTConfig);
    if (RwStreamRead(stream, &binConfig, size) != size) {
        return NULL;
    }

    effect = RpGameCubeMTEffectCreate(binConfig.numTevStages, binConfig.numTexGens, binConfig.numTexMtx,
                                      binConfig.numIndStages, binConfig.numIndMtx);
    if (effect == NULL) {
        return NULL;
    }

    config = RpGameCubeMTEffectGetConfig(effect);
    config->flags = binConfig.flags;
    config->srcBlend = (binConfig.blending & 0xF0) >> 4;
    config->destBlend = binConfig.blending & 0xF;
    memcpy(config->kreg, binConfig.kreg, sizeof(config->kreg));
    RwMemNative32(binConfig.reg, sizeof(binConfig.reg));
    memcpy(config->reg, binConfig.reg, sizeof(config->reg));

    for (i = 0; i < config->numTexGens; i++) {
        _StreamTexGen binTexGen;
        RpGameCubeTexGen* texGen = &config->texGens[i];

        if (RwStreamRead(stream, &binTexGen, sizeof(_StreamTexGen)) != sizeof(_StreamTexGen)) {
            RwFree(effect);
            return NULL;
        }

        texGen->flags = binTexGen.flags;
        texGen->func = binTexGen.func;
        texGen->srcParam = binTexGen.srcParam;
        texGen->mtx = binTexGen.mtx;
        texGen->postMtx = binTexGen.postMtx;
        RwMemNative32(&binTexGen.scaleS, 2 * sizeof(unsigned short));
        texGen->scaleS = binTexGen.scaleS;
        texGen->scaleT = binTexGen.scaleT;
    }

    for (i = 0; i < config->numTevStages; i++) {
        _StreamTevStage binTevStage;
        RpGameCubeTevStage* tevStage = &config->tevStages[i];

        if (RwStreamRead(stream, &binTevStage, sizeof(_StreamTevStage)) != sizeof(_StreamTevStage)) {
            RwFree(effect);
            return NULL;
        }

        tevStage->op.colorA = binTevStage.colorA;
        tevStage->op.colorB = binTevStage.colorB;
        tevStage->op.colorC = binTevStage.colorC;
        tevStage->op.colorD = binTevStage.colorD;
        tevStage->op.alphaA = binTevStage.alphaA;
        tevStage->op.alphaB = binTevStage.alphaB;
        tevStage->op.alphaC = binTevStage.alphaC;
        tevStage->op.alphaD = binTevStage.alphaD;
        tevStage->op.colorOp = binTevStage.colorOp;
        tevStage->op.colorBias = binTevStage.colorBias;
        tevStage->op.colorScale = binTevStage.colorScale;
        tevStage->op.colorClamp = binTevStage.colorClamp;
        tevStage->op.colorOutReg = binTevStage.colorOutReg;
        tevStage->op.alphaOp = binTevStage.alphaOp;
        tevStage->op.alphaBias = binTevStage.alphaBias;
        tevStage->op.alphaScale = binTevStage.alphaScale;
        tevStage->op.alphaClamp = binTevStage.alphaClamp;
        tevStage->op.alphaOutReg = binTevStage.alphaOutReg;
        tevStage->op.colorSel = binTevStage.colorSel;
        tevStage->op.alphaSel = binTevStage.alphaSel;
        tevStage->texCoordID = binTevStage.texCoordID;
        tevStage->texMapID = binTevStage.texMapID;
        tevStage->channelID = binTevStage.channelID;
        tevStage->flags = binTevStage.flags;
        RwMemNative32(&binTevStage.indirect, sizeof(binTevStage.indirect));
        tevStage->indirect = binTevStage.indirect;

        if (tevStage->flags & STREAM_TEVSTAGE_TEXDISABLE) {
            tevStage->texMapID |= rpGX_TEX_DISABLE;
            tevStage->flags &= ~STREAM_TEVSTAGE_TEXDISABLE;
        }
    }

    for (i = 0; i < config->numTexMtx; i++) {
        _StreamTexMtx binTexMtx;
        RpGameCubeTexMtx* texMtx = &config->texMtx[i];

        if (RwStreamRead(stream, &binTexMtx, sizeof(_StreamTexMtx)) != sizeof(_StreamTexMtx) ||
            !RwStreamReadReal(stream, &texMtx->data[0][0], sizeof(texMtx->data))) {
            RwFree(effect);
            return NULL;
        }

        texMtx->offset = binTexMtx.offset;
        texMtx->refFrame = binTexMtx.refFrame;
        texMtx->flags = binTexMtx.flags;
    }

    for (i = 0; i < config->numIndMtx; i++) {
        _StreamIndMtx binIndMtx;
        RpGameCubeIndMtx* indMtx = &config->indMtx[i];

        if (RwStreamRead(stream, &binIndMtx, sizeof(_StreamIndMtx)) != sizeof(_StreamIndMtx) ||
            !RwStreamReadReal(stream, &indMtx->data[0][0], sizeof(indMtx->data))) {
            RwFree(effect);
            return NULL;
        }

        indMtx->id = binIndMtx.id;
        indMtx->refFrame = binIndMtx.refFrame;
        indMtx->flags = binIndMtx.flags;
        indMtx->scale = binIndMtx.scale;
    }

    for (i = 0; i < config->numIndStages; i++) {
        _StreamIndStage binIndStage;
        RpGameCubeIndStage* indStage = &config->indStages[i];

        if (RwStreamRead(stream, &binIndStage, sizeof(_StreamIndStage)) != sizeof(_StreamIndStage)) {
            RwFree(effect);
            return NULL;
        }

        indStage->flags = binIndStage.flags;
        indStage->texCoordID = binIndStage.texCoordID;
        indStage->texMapID = binIndStage.texMapID;
        indStage->scaleS = binIndStage.scaleS;
        indStage->scaleT = binIndStage.scaleT;
    }

    return effect;
} // 0x802C158C

/*
 * ==================== _rpGameCubeMTDataPluginAttach ====================
 * Registers the GameCube's stream functions with the multitexture effects.
 */
RwBool _rpGameCubeMTDataPluginAttach(void) {
    RwBool result;

    result = _rpMTEffectRegisterPlatform(rwID_GAMECUBE, GameCubeMTEffectStreamRead,
                                         GameCubeMTEffectStreamWrite, GameCubeMTEffectStreamGetSize, NULL);
    return result;
} // 0x802C19E8

/*
 * ==================== RpGameCubeMTEffectCreate ====================
 * Creates an effect with room for the given numbers of TEV stages, texture coordinate
 * generators, texture matrices, indirect stages and indirect matrices. The arrays lie behind the
 * configuration in the order texture matrices, texture coordinate generators, indirect matrices,
 * indirect stages and TEV stages; an array that is empty has no pointer.
 */
RpMTEffect* RpGameCubeMTEffectCreate(unsigned int numTevStages, unsigned int numTexGens,
                                     unsigned int numTexMtx, unsigned int numIndStages,
                                     unsigned int numIndMtx) {
    RpMTEffect* effect;
    RpGameCubeMTConfig* config;
    unsigned int size;
    unsigned char* offsetData;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(MatFXInfo.Module.numInstances);
    RWASSERT(numTevStages <= rpGAMECUBEMAXNUMTEVSTAGES);
    RWASSERT(numTexGens <= rpGAMECUBEMAXNUMTEXGENS);

    size = sizeof(RpMTEffect) + sizeof(RpGameCubeMTConfig);
    size = ALIGN4(size + numTexGens * sizeof(RpGameCubeTexGen));
    size = ALIGN4(size + numTevStages * sizeof(RpGameCubeTevStage));
    size = ALIGN4(size + numIndStages * sizeof(RpGameCubeIndStage));
    size = ALIGN4(size + numTexMtx * sizeof(RpGameCubeTexMtx));
    size = ALIGN4(size + numIndMtx * sizeof(RpGameCubeIndMtx));

    effect = (RpMTEffect*)RwMalloc(size, rwMEMHINTDUR_EVENT | MULTITEXTURE_PLUGIN_ID);
    if (effect == NULL) {
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    memset(effect, 0, size);
    _rpMTEffectInit(effect, rwID_GAMECUBE);

    config = RpGameCubeMTEffectGetConfig(effect);
    config->maxNumTevStages = numTevStages;
    config->maxNumTexGens = numTexGens;
    config->maxNumIndStages = numIndStages;
    config->maxNumTexMtx = numTexMtx;
    config->maxNumIndMtx = numIndMtx;
    config->numTevStages = numTevStages;
    config->numTexGens = numTexGens;
    config->numIndStages = numIndStages;
    config->numTexMtx = numTexMtx;
    config->numIndMtx = numIndMtx;

    offsetData = (unsigned char*)(config + 1);
    if (numTexMtx != 0) {
        config->texMtx = (RpGameCubeTexMtx*)offsetData;
        offsetData += numTexMtx * sizeof(RpGameCubeTexMtx);
    }
    if (numTexGens != 0) {
        config->texGens = (RpGameCubeTexGen*)offsetData;
        offsetData += numTexGens * sizeof(RpGameCubeTexGen);
    }
    if (numIndMtx != 0) {
        config->indMtx = (RpGameCubeIndMtx*)offsetData;
        offsetData += numIndMtx * sizeof(RpGameCubeIndMtx);
    }
    if (numIndStages != 0) {
        config->indStages = (RpGameCubeIndStage*)offsetData;
        offsetData += numIndStages * sizeof(RpGameCubeIndStage);
    }
    if (numTevStages != 0) {
        config->tevStages = (RpGameCubeTevStage*)offsetData;
    }

    return effect;
} // 0x802C1A0C

/*
 * ==================== RpGameCubeMTEffectGetConfig ====================
 * The GameCube data of an effect: it follows the effect's header.
 */
RpGameCubeMTConfig* RpGameCubeMTEffectGetConfig(RpMTEffect* effect) {
    RpGameCubeMTConfig* config;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(MatFXInfo.Module.numInstances);
    RWASSERT(effect);
    RWASSERT(effect->platformID == rwID_GAMECUBE);

    config = RpGameCubeMTEffectGetConfigMacro(effect);
    return config;
} // 0x802C1B7C

/*
 * ==================== RpGameCubeTevIndUnpack ====================
 * Unpacks the indirect word of a TEV stage, 20 bits from the lowest: the stage, format, bias,
 * matrix, the two wraps, the add-previous and unsigned-coordinate-LOD bits and the alpha select.
 */
void RpGameCubeTevIndUnpack(RpGameCubeTevInd* data, unsigned int* pkData) {
    unsigned int pk;

    pk = *pkData;

    data->indStage = pk & 0x3;
    data->format = (pk >> 2) & 0x3;
    data->biasSel = (pk >> 4) & 0x7;
    data->matrixSel = (pk >> 7) & 0xF;
    data->wrapS = (pk >> 11) & 0x7;
    data->wrapT = (pk >> 14) & 0x7;
    data->addPrev = (pk >> 17) & 0x1;
    data->utcLod = (pk >> 18) & 0x1;
    data->alphaSel = (pk >> 19) & 0x3;
} // 0x802C1B84
