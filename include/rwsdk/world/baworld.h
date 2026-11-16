// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
#ifndef RWSDK_WORLD_BAWORLD_H
#define RWSDK_WORLD_BAWORLD_H

#include <rwsdk/babbox.h>
#include <rwsdk/batypehf.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/balight.h>
#include <rwsdk/world/bamatlst.h>
#include <rwsdk/world/basector.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The object type of a world. */
#define rpWORLD 7

/* RpWorldPrivateFlag: kept in a world's object privateFlags. */
typedef enum RpWorldPrivateFlag {
    rpWORLDSINGLEMALLOC = 0x01, /* the world and its sectors are one allocation */
    rpWORLDPRIVATEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RpWorldPrivateFlag;

/* RpWorldFlag: what a world's vertices carry and how it is rendered (as RpGeometryFlag). */
typedef enum RpWorldFlag {
    rpWORLDTRISTRIP = 0x01,
    rpWORLDPOSITIONS = 0x02,
    rpWORLDTEXTURED = 0x04,
    rpWORLDPRELIT = 0x08,
    rpWORLDNORMALS = 0x10,
    rpWORLDLIGHT = 0x20,
    rpWORLDMODULATEMATERIALCOLOR = 0x40,
    rpWORLDTEXTURED2 = 0x80,
    rpWORLDNATIVE = 0x01000000,
    rpWORLDNATIVEINSTANCE = 0x02000000,
    rpWORLDFLAGSMASK = 0x000000FF,
    rpWORLDNATIVEFLAGSMASK = 0x0F000000,
    rpWORLDSECTORSOVERLAP = 0x40000000,
    rpWORLDFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RpWorldFlag;

/* RpWorldRenderOrder: the order a world's sectors are rendered in. */
typedef enum RpWorldRenderOrder {
    rpWORLDRENDERNARENDERORDER = 0,
    rpWORLDRENDERFRONT2BACK = 1,
    rpWORLDRENDERBACK2FRONT = 2,
    rpWORLDRENDERORDERFORCEENUMSIZEINT = 0x7FFFFFFF
} RpWorldRenderOrder;

/* Renders a world sector; NULL reports a failure. */
typedef RpWorldSector* (*RpWorldSectorCallBackRender)(RpWorldSector* worldSector);

/*
 * RpWorld: the static scene: its materials, the BSP tree of its sectors, the clumps and lights
 * added to it, its origin and bounding box, and how its sectors render.
 * Size 0x70, from retail's world plugin registry.
 */
typedef struct RpWorld {
    RwObject object;                            /* 0x0 */
    RwUInt32 flags;                             /* 0x8 */
    RpWorldRenderOrder renderOrder;             /* 0xC */
    RpMaterialList matList;                     /* 0x10 */
    RpSector* rootSector;                       /* 0x1C */
    RwInt32 numTexCoordSets;                    /* 0x20 */
    RwInt32 numClumpsInWorld;                   /* 0x24 */
    RwLLLink* currentClumpLink;                 /* 0x28 */
    RwLinkList clumpList;                       /* 0x2C */
    RwLinkList lightList;                       /* 0x34 */
    RwLinkList directionalLightList;            /* 0x3C */
    RwV3d worldOrigin;                          /* 0x44 */
    RwBBox boundingBox;                         /* 0x50 */
    RpWorldSectorCallBackRender renderCallBack; /* 0x68 */
    RxPipeline* pipeline;                       /* 0x6C */
} RpWorld;

/* World accessors (bfbbdecomp/bfbb rpworld.h, the SDK's release macros). */
#define RpWorldGetBBox(_world) (&((_world)->boundingBox))
#define _rpWorldGetOrigin(_world) (&((_world)->worldOrigin))
#define RpWorldGetNumMaterials(_world) ((_world)->matList.numMaterials)
#define RpWorldGetMaterial(_world, _num) (((_world)->matList.materials)[(_num)])
#define RpWorldGetNumClumps(_world) ((_world)->numClumpsInWorld)
#define RpWorldSetRenderOrder(_world, _renderOrder) (((_world)->renderOrder = (_renderOrder)), (_world))
#define RpWorldGetRenderOrder(_world) ((_world)->renderOrder)
#define RpWorldSetFlags(_world, _flags) (((_world)->flags = (_flags)), (_world))
#define RpWorldGetFlags(_world) ((_world)->flags)

/* The world plugin registry (bfbbdecomp/bfbb symbols). */
extern RwPluginRegistry worldTKList;

/*
 * Tear down a BSP subtree read into a world's single allocation (the world sectors' instanced
 * data only), or one allocated sector by sector. Called by the stream reader on a failure.
 */
void WorldSectorDeinstanceAll(RpSector* sector);
void WorldSectorDestroyRecurse(RpSector* sector);

/*
 * The world pipelines' module open and close, and the world pipeline attach step of
 * RpWorldPluginAttach (bfbbdecomp/bfbb names for the first two); defined in the world pipeline unit,
 * not yet split.
 */
RwBool _rpWorldPipelineOpen(void);
void _rpWorldPipelineClose(void);
RwBool _rxWorldDevicePluginAttach(void);

/*
 * A coordinate of a vector by a plane sector's type, the byte offset of its axis in an RwV3d (the
 * BSP trees white paper). Name invented.
 */
#define GETCOORD(vect, type) (*(RwReal*)((RwUInt8*)&(vect) + (type)))

RpWorldSector* _rpSectorDefaultRenderCallBack(RpWorldSector* sector);
RpWorld* _rpWorldSetupSectorBoundingBoxes(RpWorld* world);
void _rpWorldRegisterWorld(RpWorld* world, RwUInt32 memorySize);
RpWorld* RpWorldLock(RpWorld* world);
RpWorld* RpWorldUnlock(RpWorld* world);
RpWorld* RpWorldSectorGetWorld(const RpWorldSector* sector);
RwBool RpWorldDestroy(RpWorld* world);
RpWorld* RpWorldSetSectorRenderCallBack(RpWorld* world, RpWorldSectorCallBackRender fpCallBack);
RpWorldSectorCallBackRender RpWorldGetSectorRenderCallBack(const RpWorld* world);
RpWorld* RpWorldCreate(RwBBox* boundingBox);
RpWorld* RpWorldForAllClumps(RpWorld* world, RpClumpCallBack fpCallBack, void* pData);
RpWorld* RpWorldForAllMaterials(RpWorld* world, RpMaterialCallBack fpCallBack, void* pData);
RpWorld* RpWorldForAllWorldSectors(RpWorld* world, RpWorldSectorCallBack fpCallBack, void* pData);
RwInt32 RpWorldRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpWorldRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                    RwPluginDataChunkWriteCallBack writeCB,
                                    RwPluginDataChunkGetSizeCallBack getSizeCB);
RwBool RpWorldPluginAttach(void);

#ifdef __cplusplus
}
#endif

#endif
