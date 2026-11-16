// Names and layout: Gormiti DWARF (RpMatFx.a multiTexGcnData.obj and multiTexGcnPipe.obj; the types are
// also seen from DirectionalLight.cpp, so a public header of the plugin declared them)
// Header name invented: no evidence names Gormiti's header for the GameCube multitexture data.
#ifndef RWSDK_PLUGIN_MATFX_MULTITEXGCN_H
#define RWSDK_PLUGIN_MATFX_MULTITEXGCN_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/world/gcpipe.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The GameCube's GX enumerations, as the plugin spells them (RpGX*, with GX's own values). */
typedef enum RpGXTexGenType {
    rpGX_TG_MTX3x4 = 0,
    rpGX_TG_MTX2x4 = 1,
    rpGX_TG_BUMP0 = 2,
    rpGX_TG_BUMP1 = 3,
    rpGX_TG_BUMP2 = 4,
    rpGX_TG_BUMP3 = 5,
    rpGX_TG_BUMP4 = 6,
    rpGX_TG_BUMP5 = 7,
    rpGX_TG_BUMP6 = 8,
    rpGX_TG_BUMP7 = 9,
    rpGX_TG_SRTG = 10
} RpGXTexGenType;

typedef enum RpGXTexGenSrc {
    rpGX_TG_POS = 0,
    rpGX_TG_NRM = 1,
    rpGX_TG_BINRM = 2,
    rpGX_TG_TANGENT = 3,
    rpGX_TG_TEX0 = 4,
    rpGX_TG_TEX1 = 5,
    rpGX_TG_TEX2 = 6,
    rpGX_TG_TEX3 = 7,
    rpGX_TG_TEX4 = 8,
    rpGX_TG_TEX5 = 9,
    rpGX_TG_TEX6 = 10,
    rpGX_TG_TEX7 = 11,
    rpGX_TG_TEXCOORD0 = 12,
    rpGX_TG_TEXCOORD1 = 13,
    rpGX_TG_TEXCOORD2 = 14,
    rpGX_TG_TEXCOORD3 = 15,
    rpGX_TG_TEXCOORD4 = 16,
    rpGX_TG_TEXCOORD5 = 17,
    rpGX_TG_TEXCOORD6 = 18,
    rpGX_TG_COLOR0 = 19,
    rpGX_TG_COLOR1 = 20
} RpGXTexGenSrc;

typedef enum RpGXIndTexMtxID {
    rpGX_ITM_OFF = 0,
    rpGX_ITM_0 = 1,
    rpGX_ITM_1 = 2,
    rpGX_ITM_2 = 3,
    rpGX_ITM_S0 = 5,
    rpGX_ITM_S1 = 6,
    rpGX_ITM_S2 = 7,
    rpGX_ITM_T0 = 9,
    rpGX_ITM_T1 = 10,
    rpGX_ITM_T2 = 11
} RpGXIndTexMtxID;

typedef enum RpGXTexCoordID {
    rpGX_TEXCOORD0 = 0,
    rpGX_TEXCOORD1 = 1,
    rpGX_TEXCOORD2 = 2,
    rpGX_TEXCOORD3 = 3,
    rpGX_TEXCOORD4 = 4,
    rpGX_TEXCOORD5 = 5,
    rpGX_TEXCOORD6 = 6,
    rpGX_TEXCOORD7 = 7,
    rpGX_MAX_TEXCOORD = 8,
    rpGX_TEXCOORD_NULL = 255
} RpGXTexCoordID;

typedef enum RpGXTexMapID {
    rpGX_TEXMAP0 = 0,
    rpGX_TEXMAP1 = 1,
    rpGX_TEXMAP2 = 2,
    rpGX_TEXMAP3 = 3,
    rpGX_TEXMAP4 = 4,
    rpGX_TEXMAP5 = 5,
    rpGX_TEXMAP6 = 6,
    rpGX_TEXMAP7 = 7,
    rpGX_MAX_TEXMAP = 8,
    rpGX_TEXMAP_NULL = 255,
    rpGX_TEX_DISABLE = 256
} RpGXTexMapID;

typedef enum RpGXIndTexScale {
    rpGX_ITS_1 = 0,
    rpGX_ITS_2 = 1,
    rpGX_ITS_4 = 2,
    rpGX_ITS_8 = 3,
    rpGX_ITS_16 = 4,
    rpGX_ITS_32 = 5,
    rpGX_ITS_64 = 6,
    rpGX_ITS_128 = 7,
    rpGX_ITS_256 = 8
} RpGXIndTexScale;

typedef enum RpGXTevColorArg {
    rpGX_CC_CPREV = 0,
    rpGX_CC_APREV = 1,
    rpGX_CC_C0 = 2,
    rpGX_CC_A0 = 3,
    rpGX_CC_C1 = 4,
    rpGX_CC_A1 = 5,
    rpGX_CC_C2 = 6,
    rpGX_CC_A2 = 7,
    rpGX_CC_TEXC = 8,
    rpGX_CC_TEXA = 9,
    rpGX_CC_RASC = 10,
    rpGX_CC_RASA = 11,
    rpGX_CC_ONE = 12,
    rpGX_CC_HALF = 13,
    rpGX_CC_KONST = 14,
    rpGX_CC_ZERO = 15
} RpGXTevColorArg;

typedef enum RpGXTevAlphaArg {
    rpGX_CA_APREV = 0,
    rpGX_CA_A0 = 1,
    rpGX_CA_A1 = 2,
    rpGX_CA_A2 = 3,
    rpGX_CA_TEXA = 4,
    rpGX_CA_RASA = 5,
    rpGX_CA_KONST = 6,
    rpGX_CA_ZERO = 7
} RpGXTevAlphaArg;

typedef enum RpGXTevOp {
    rpGX_TEV_ADD = 0,
    rpGX_TEV_SUB = 1,
    rpGX_TEV_COMP_R8_GT = 8,
    rpGX_TEV_COMP_R8_EQ = 9,
    rpGX_TEV_COMP_GR16_GT = 10,
    rpGX_TEV_COMP_GR16_EQ = 11,
    rpGX_TEV_COMP_BGR24_GT = 12,
    rpGX_TEV_COMP_BGR24_EQ = 13,
    rpGX_TEV_COMP_RGB8_GT = 14,
    rpGX_TEV_COMP_RGB8_EQ = 15,
    rpGX_TEV_COMP_A8_GT = 14,
    rpGX_TEV_COMP_A8_EQ = 15
} RpGXTevOp;

typedef enum RpGXTevBias {
    rpGX_TB_ZERO = 0,
    rpGX_TB_ADDHALF = 1,
    rpGX_TB_SUBHALF = 2,
    rpGX_MAX_TEVBIAS = 3
} RpGXTevBias;

typedef enum RpGXTevScale {
    rpGX_CS_SCALE_1 = 0,
    rpGX_CS_SCALE_2 = 1,
    rpGX_CS_SCALE_4 = 2,
    rpGX_CS_DIVIDE_2 = 3,
    rpGX_MAX_TEVSCALE = 4
} RpGXTevScale;

typedef enum RpGXTevRegID {
    rpGX_TEVPREV = 0,
    rpGX_TEVREG0 = 1,
    rpGX_TEVREG1 = 2,
    rpGX_TEVREG2 = 3,
    rpGX_MAX_TEVREG = 4
} RpGXTevRegID;

typedef enum RpGXTevKColorSel {
    rpGX_TEV_KCSEL_1 = 0,
    rpGX_TEV_KCSEL_7_8 = 1,
    rpGX_TEV_KCSEL_3_4 = 2,
    rpGX_TEV_KCSEL_5_8 = 3,
    rpGX_TEV_KCSEL_1_2 = 4,
    rpGX_TEV_KCSEL_3_8 = 5,
    rpGX_TEV_KCSEL_1_4 = 6,
    rpGX_TEV_KCSEL_1_8 = 7,
    rpGX_TEV_KCSEL_K0 = 12,
    rpGX_TEV_KCSEL_K1 = 13,
    rpGX_TEV_KCSEL_K2 = 14,
    rpGX_TEV_KCSEL_K3 = 15,
    rpGX_TEV_KCSEL_K0_R = 16,
    rpGX_TEV_KCSEL_K1_R = 17,
    rpGX_TEV_KCSEL_K2_R = 18,
    rpGX_TEV_KCSEL_K3_R = 19,
    rpGX_TEV_KCSEL_K0_G = 20,
    rpGX_TEV_KCSEL_K1_G = 21,
    rpGX_TEV_KCSEL_K2_G = 22,
    rpGX_TEV_KCSEL_K3_G = 23,
    rpGX_TEV_KCSEL_K0_B = 24,
    rpGX_TEV_KCSEL_K1_B = 25,
    rpGX_TEV_KCSEL_K2_B = 26,
    rpGX_TEV_KCSEL_K3_B = 27,
    rpGX_TEV_KCSEL_K0_A = 28,
    rpGX_TEV_KCSEL_K1_A = 29,
    rpGX_TEV_KCSEL_K2_A = 30,
    rpGX_TEV_KCSEL_K3_A = 31
} RpGXTevKColorSel;

typedef enum RpGXTevKAlphaSel {
    rpGX_TEV_KASEL_1 = 0,
    rpGX_TEV_KASEL_7_8 = 1,
    rpGX_TEV_KASEL_3_4 = 2,
    rpGX_TEV_KASEL_5_8 = 3,
    rpGX_TEV_KASEL_1_2 = 4,
    rpGX_TEV_KASEL_3_8 = 5,
    rpGX_TEV_KASEL_1_4 = 6,
    rpGX_TEV_KASEL_1_8 = 7,
    rpGX_TEV_KASEL_K0_R = 16,
    rpGX_TEV_KASEL_K1_R = 17,
    rpGX_TEV_KASEL_K2_R = 18,
    rpGX_TEV_KASEL_K3_R = 19,
    rpGX_TEV_KASEL_K0_G = 20,
    rpGX_TEV_KASEL_K1_G = 21,
    rpGX_TEV_KASEL_K2_G = 22,
    rpGX_TEV_KASEL_K3_G = 23,
    rpGX_TEV_KASEL_K0_B = 24,
    rpGX_TEV_KASEL_K1_B = 25,
    rpGX_TEV_KASEL_K2_B = 26,
    rpGX_TEV_KASEL_K3_B = 27,
    rpGX_TEV_KASEL_K0_A = 28,
    rpGX_TEV_KASEL_K1_A = 29,
    rpGX_TEV_KASEL_K2_A = 30,
    rpGX_TEV_KASEL_K3_A = 31
} RpGXTevKAlphaSel;

typedef enum RpGXChannelID {
    rpGX_COLOR0 = 0,
    rpGX_COLOR1 = 1,
    rpGX_ALPHA0 = 2,
    rpGX_ALPHA1 = 3,
    rpGX_COLOR0A0 = 4,
    rpGX_COLOR1A1 = 5,
    rpGX_COLOR_ZERO = 6,
    rpGX_ALPHA_BUMP = 7,
    rpGX_ALPHA_BUMPN = 8,
    rpGX_COLOR_NULL = 255
} RpGXChannelID;

typedef enum RpGXIndTexStageID {
    rpGX_INDTEXSTAGE0 = 0,
    rpGX_INDTEXSTAGE1 = 1,
    rpGX_INDTEXSTAGE2 = 2,
    rpGX_INDTEXSTAGE3 = 3
} RpGXIndTexStageID;

typedef enum RpGXIndTexFormat {
    rpGX_ITF_8 = 0,
    rpGX_ITF_5 = 1,
    rpGX_ITF_4 = 2,
    rpGX_ITF_3 = 3
} RpGXIndTexFormat;

typedef enum RpGXIndTexBiasSel {
    rpGX_ITB_NONE = 0,
    rpGX_ITB_S = 1,
    rpGX_ITB_T = 2,
    rpGX_ITB_ST = 3,
    rpGX_ITB_U = 4,
    rpGX_ITB_SU = 5,
    rpGX_ITB_TU = 6,
    rpGX_ITB_STU = 7
} RpGXIndTexBiasSel;

typedef enum RpGXIndTexWrap {
    rpGX_ITW_OFF = 0,
    rpGX_ITW_256 = 1,
    rpGX_ITW_128 = 2,
    rpGX_ITW_64 = 3,
    rpGX_ITW_32 = 4,
    rpGX_ITW_16 = 5,
    rpGX_ITW_0 = 6
} RpGXIndTexWrap;

typedef enum RpGXIndTexAlphaSel {
    rpGX_ITBA_OFF = 0,
    rpGX_ITBA_S = 1,
    rpGX_ITBA_T = 2,
    rpGX_ITBA_U = 3
} RpGXIndTexAlphaSel;

/* The frame a texture matrix follows. */
typedef enum RpGameCubeTexFrameID {
    rpGAMECUBETEXFRAME_NONE = 0,
    rpGAMECUBETEXFRAME_OBJECT = 1,
    rpGAMECUBETEXFRAME_WORLD = 2,
    rpGAMECUBETEXFRAME_CAMERA = 3,
    rpGAMECUBETEXFRAME_MISC0 = 16,
    rpGAMECUBETEXFRAME_MISC1 = 17,
    rpGAMECUBETEXFRAME_MISC2 = 18,
    rpGAMECUBETEXFRAME_MISC3 = 19,
    rpGAMECUBETEXFRAME_MISC4 = 20,
    rpGAMECUBETEXFRAME_MISC5 = 21,
    rpGAMECUBETEXFRAME_MISC6 = 22,
    rpGAMECUBETEXFRAME_MISC7 = 23,
    rpGAMECUBETEXFRAME_MISC8 = 24,
    rpGAMECUBETEXFRAME_MISC9 = 25,
    rpGAMECUBETEXFRAME_MISCMAX = 47,
    rpGAMECUBETEXFRAME_FORCEENUMSIZEINT = 2147483647
} RpGameCubeTexFrameID;

/* A colour register of the TEV: a signed 16-bit value per channel. */
typedef struct RpGXColorS10 {
    short r;
    short g;
    short b;
    short a;
} RpGXColorS10;

/* A texture matrix (3x4) and the frame it follows. */
typedef struct RpGameCubeTexMtx {
    unsigned int offset;
    RpGameCubeTexFrameID refFrame;
    unsigned int flags;
    float data[3][4];
} RpGameCubeTexMtx;

/* One texture coordinate generator. */
typedef struct RpGameCubeTexGen {
    unsigned int flags;
    RpGXTexGenType func;
    RpGXTexGenSrc srcParam;
    unsigned int mtx;
    unsigned int postMtx;
    unsigned short scaleS;
    unsigned short scaleT;
} RpGameCubeTexGen;

/* An indirect texture matrix (2x3). */
typedef struct RpGameCubeIndMtx {
    RpGXIndTexMtxID id;
    RpGameCubeTexFrameID refFrame;
    unsigned int flags;
    float data[2][3];
    int scale;
} RpGameCubeIndMtx;

typedef struct RpGameCubeIndStage {
    unsigned int flags;
    RpGXTexCoordID texCoordID;
    RpGXTexMapID texMapID;
    RpGXIndTexScale scaleS;
    RpGXIndTexScale scaleT;
} RpGameCubeIndStage;

/* The colour and alpha combiners of a TEV stage. */
typedef struct RpGameCubeTevOp {
    RpGXTevColorArg colorA;
    RpGXTevColorArg colorB;
    RpGXTevColorArg colorC;
    RpGXTevColorArg colorD;
    RpGXTevAlphaArg alphaA;
    RpGXTevAlphaArg alphaB;
    RpGXTevAlphaArg alphaC;
    RpGXTevAlphaArg alphaD;
    RpGXTevOp colorOp;
    RpGXTevBias colorBias;
    RpGXTevScale colorScale;
    unsigned char colorClamp;
    RpGXTevRegID colorOutReg;
    RpGXTevOp alphaOp;
    RpGXTevBias alphaBias;
    RpGXTevScale alphaScale;
    unsigned char alphaClamp;
    RpGXTevRegID alphaOutReg;
    RpGXTevKColorSel colorSel;
    RpGXTevKAlphaSel alphaSel;
} RpGameCubeTevOp;

typedef struct RpGameCubeTevStage {
    RpGameCubeTevOp op;
    RpGXTexCoordID texCoordID;
    RpGXTexMapID texMapID;
    RpGXChannelID channelID;
    unsigned int flags;
    unsigned int indirect; /* packed; RpGameCubeTevIndUnpack unpacks it */
} RpGameCubeTevStage;

/* The indirect part of a TEV stage, unpacked. */
typedef struct RpGameCubeTevInd {
    RpGXIndTexStageID indStage;
    RpGXIndTexFormat format;
    RpGXIndTexBiasSel biasSel;
    RpGXIndTexMtxID matrixSel;
    RpGXIndTexWrap wrapS;
    RpGXIndTexWrap wrapT;
    unsigned char addPrev;
    unsigned char utcLod;
    RpGXIndTexAlphaSel alphaSel;
} RpGameCubeTevInd;

/* The data of a GameCube multitexture effect: it follows the RpMTEffect header in memory. */
typedef struct RpGameCubeMTConfig {
    unsigned int flags;
    unsigned char maxNumTexGens;
    unsigned char maxNumTevStages;
    unsigned char maxNumTexMtx;
    unsigned char maxNumIndStages;
    unsigned char maxNumIndMtx;
    unsigned char numTexGens;
    unsigned char numTevStages;
    unsigned char numTexMtx;
    unsigned char numIndStages;
    unsigned char numIndMtx;
    RpGXColorS10 reg[4];
    RwRGBA kreg[4];
    RpGameCubeTexMtx* texMtx;
    RpGameCubeTexGen* texGens;
    RpGameCubeIndMtx* indMtx;
    RpGameCubeIndStage* indStages;
    RpGameCubeTevStage* tevStages;
    RwBlendFunction srcBlend;
    RwBlendFunction destBlend;
} RpGameCubeMTConfig;

/* gcn/multiTexGcnData.c */
extern RpMTEffect* RpGameCubeMTEffectCreate(unsigned int numTevStages, unsigned int numTexGens,
                                            unsigned int numTexMtx, unsigned int numIndStages,
                                            unsigned int numIndMtx);
extern RpGameCubeMTConfig* RpGameCubeMTEffectGetConfig(RpMTEffect* effect);
extern void RpGameCubeTevIndUnpack(RpGameCubeTevInd* data, unsigned int* pkData);

/*
 * The callbacks of a material's multitexture on the GameCube, kept in the platform extension of the
 * RpMultiTexture (Gormiti DWARF, RpMatFx.a multiTexGcnPipe.obj): one before and one after the material's
 * pipeline stage, both given callBackData.
 */
typedef RpMaterial* (*RpMultiTextureGameCubeCallBack)(RpMaterial* material, void* data, RwMatrix* ltm,
                                                      RxGameCubePipeData* pipeData, void* callBackData);

typedef struct rpMultiTextureGameCubeExt {
    RpMultiTextureGameCubeCallBack preRenderCallBack;   /* 0x00 */
    RpMultiTextureGameCubeCallBack postRenderCallBack;  /* 0x04 */
    void* callBackData;                                 /* 0x08 */
} rpMultiTextureGameCubeExt;

/* gcn/multiTexGcnPipe.c */
extern RpMultiTexture* RpMultiTextureSetGameCubeMTCallBacks(RpMultiTexture* multiTexture,
                                                            RpMultiTextureGameCubeCallBack preRenderCallBack,
                                                            RpMultiTextureGameCubeCallBack postRenderCallBack,
                                                            void* callBackData);
extern RpMultiTexture* RpMultiTextureGetGameCubeMTCallBacks(RpMultiTexture* multiTexture,
                                                            RpMultiTextureGameCubeCallBack* preRenderCallBack,
                                                            RpMultiTextureGameCubeCallBack* postRenderCallBack,
                                                            void** callBackData);

#ifdef __cplusplus
}
#endif

#endif
