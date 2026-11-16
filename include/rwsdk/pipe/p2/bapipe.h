// Reference: tgsm/pbg src/rwsdk/src/badevice.c, src/rwsdk/src/bacamera.c (declarations)
#ifndef RWSDK_PIPE_P2_BAPIPE_H
#define RWSDK_PIPE_P2_BAPIPE_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batextur.h>
#include <rwsdk/plcore/bacolor.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RwCamera;

/* The most outputs a pipeline node can have; every node's output row in a locked pipeline has this stride. */
#define rxNODE_MAX_OUTPUTS 32

struct RxPipelineNode;
struct RxPacket;
struct RxPipelineRequiresCluster;

/* The state of a pipeline's embedded packet (bfbbdecomp/bfbb rwcore.h). */
typedef enum rxEmbeddedPacketState {
    rxPKST_PACKETLESS = 0,
    rxPKST_UNUSED = 1,
    rxPKST_INUSE = 2,
    rxPKST_PENDING = 3,
    rxEMBEDDEDPACKETSTATEFORCEENUMSIZEINT = 0x7FFFFFFF
} rxEmbeddedPacketState;

/*
 * A render pipeline (bfbbdecomp/bfbb rwcore.h): its nodes, its embedded packet, the clusters its
 * input requires, and the plugin that owns it (pluginId, pluginData: what the world's rights
 * stream callbacks save). Size 0x34.
 */
typedef struct RxPipeline {
    RwBool locked;                                       /* 0x0 */
    RwUInt32 numNodes;                                   /* 0x4 */
    struct RxPipelineNode* nodes;                        /* 0x8 */
    RwUInt32 packetNumClusterSlots;                      /* 0xC */
    rxEmbeddedPacketState embeddedPacketState;           /* 0x10 */
    struct RxPacket* embeddedPacket;                     /* 0x14 */
    RwUInt32 numInputRequirements;                       /* 0x18 */
    struct RxPipelineRequiresCluster* inputRequirements; /* 0x1C */
    void* superBlock;                                    /* 0x20 */
    RwUInt32 superBlockSize;                             /* 0x24 */
    RwUInt32 entryPoint;                                 /* 0x28 */
    RwUInt32 pluginId;                                   /* 0x2C */
    RwUInt32 pluginData;                                 /* 0x30 */
} RxPipeline;

/* An index into a vertex array (bfbbdecomp/bfbb rwplcore.h). */
typedef RwUInt16 RxVertexIndex;

/* Shading models (Gormiti DWARF: enum RwShadeMode). */
typedef enum RwShadeMode {
    rwSHADEMODENASHADEMODE = 0,
    rwSHADEMODEFLAT = 1,
    rwSHADEMODEGOURAUD = 2,
    rwSHADEMODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwShadeMode;

/* Fog models (Gormiti DWARF: enum RwFogType). */
typedef enum RwFogType {
    rwFOGTYPENAFOGTYPE = 0,
    rwFOGTYPELINEAR = 1,
    rwFOGTYPEEXPONENTIAL = 2,
    rwFOGTYPEEXPONENTIAL2 = 3,
    rwFOGTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwFogType;

/* The bits of RxRenderStateVector::Flags. */
typedef enum RxRenderStateFlag {
    rxRENDERSTATEFLAG_TEXTUREPERSPECTIVE = 0x00000001,
    rxRENDERSTATEFLAG_ZTESTENABLE = 0x00000002,
    rxRENDERSTATEFLAG_ZWRITEENABLE = 0x00000004,
    rxRENDERSTATEFLAG_VERTEXALPHAENABLE = 0x00000008,
    rxRENDERSTATEFLAG_FOGENABLE = 0x00000010,
    rxRENDERSTATEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RxRenderStateFlag;

/* The render state a pipeline's nodes draw with. Size 0x2C. Name and layout: Gormiti DWARF (struct RxRenderStateVector). */
typedef struct RxRenderStateVector {
    RwUInt32 Flags;                     /* 0x00 */
    RwShadeMode ShadeMode;              /* 0x04 */
    RwBlendFunction SrcBlend;           /* 0x08 */
    RwBlendFunction DestBlend;          /* 0x0C */
    RwRaster* TextureRaster;            /* 0x10 */
    RwTextureAddressMode AddressModeU;  /* 0x14 */
    RwTextureAddressMode AddressModeV;  /* 0x18 */
    RwTextureFilterMode FilterMode;     /* 0x1C */
    RwRGBA BorderColor;                 /* 0x20 */
    RwFogType FogType;                  /* 0x24 */
    RwRGBA FogColor;                    /* 0x28 */
} RxRenderStateVector;

/* Resets a render state vector to the default (p2renderstate.c). */
RxRenderStateVector* RxRenderStateVectorSetDefaultRenderStateVector(RxRenderStateVector* rsvp);

/*
 * The pipeline module (rwID_PIPEMODULE): its open and close, the attachment of the pipeline
 * plugins, and the per-camera set-up RwCameraBeginUpdate runs. Defined in bapipe.c.
 */
void* _rwRenderPipelineOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwRenderPipelineClose(void* instance, RwInt32 offset, RwInt32 size);
RwBool _rwPipeAttach(void);
void _rwPipeInitForCamera(struct RwCamera* camera);

/* The pipeline system's open and close (p2core.c). */
RwBool _rxPipelineOpen(void);
RwBool _rxPipelineClose(void);

/*
 * The pipeline module's globals, _rxPipelineGlobalsOffset bytes into the engine instance: the
 * pipelines' free list and the default pipelines objects render with when they have none of their
 * own. Only the members the world plugin reads are named; type, macro and member names invented
 * (no reference source has them), the offset's name is tgsm/pbg's.
 */
typedef struct rxPipelineGlobals {
    struct RwFreeList* pipesFreeList;               /* 0x0 */
    RxRenderStateVector defaultRenderState;         /* 0x4 (name invented): the vector RxRenderStateVectorSetDefaultRenderStateVector copies */
    RwUInt32 field_0x30;                            /* 0x30 */
    RwUInt32 field_0x34;                            /* 0x34 */
    RwUInt32 field_0x38;                            /* 0x38 */
    RxPipeline* currentAtomicPipeline;              /* 0x3C: AtomicDefaultRenderCallBack's default */
    RxPipeline* currentWorldSectorPipeline;         /* 0x40: the world sectors' default */
    RxPipeline* currentMaterialPipeline;            /* 0x44 (name invented) */
    RxPipeline* genericAtomicPipeline;              /* 0x48 (name invented) */
    RxPipeline* genericWorldSectorPipeline;         /* 0x4C (name invented) */
    RxPipeline* genericMaterialPipeline;            /* 0x50 (name invented) */
    RxPipeline* platformAtomicPipeline;             /* 0x54: the GameCube atomic pipeline (name invented) */
    RxPipeline* platformWorldSectorPipeline;        /* 0x58: the GameCube world sector pipeline (name invented) */
    RxPipeline* platformMaterialPipeline;           /* 0x5C (name invented) */
} rxPipelineGlobals;

extern RwInt32 _rxPipelineGlobalsOffset;

#define RXPIPELINEGLOBAL(var) \
    (RWPLUGINOFFSET(rxPipelineGlobals, RwEngineInstance, _rxPipelineGlobalsOffset)->var)

/* Runs a pipeline over an object (bfbbdecomp/bfbb rwcore.h); NULL reports a failure. */
RxPipeline* RxPipelineExecute(RxPipeline* pipeline, void* data, RwBool heapReset);

/* Destroys a pipeline and the nodes it owns. */
void _rxPipelineDestroy(RxPipeline* pipeline);

#ifdef __cplusplus
}
#endif

#endif
