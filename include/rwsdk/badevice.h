// Reference: tgsm/pbg include/rwsdk/badevice.h
#ifndef RWSDK_BADEVICE_H
#define RWSDK_BADEVICE_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/batextur.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plcore/rwstring.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RwModuleInfo: a core module's registration: the offset of its globals in the block that
 * RwEngineInstance points to, and how many times it is open.
 */
typedef struct RwModuleInfo {
    RwInt32 globalsOffset;  /* 0x0 */
    RwInt32 numInstances;   /* 0x4 */
} RwModuleInfo;

/* RwRenderState: the render states of RwRenderStateSet (only the states in use are listed). */
typedef enum RwRenderState {
    rwRENDERSTATENARENDERSTATE = 0,
    rwRENDERSTATETEXTURERASTER = 1,
    rwRENDERSTATETEXTUREADDRESS = 2,
    rwRENDERSTATEZTESTENABLE = 6,
    rwRENDERSTATEZWRITEENABLE = 8,
    rwRENDERSTATETEXTUREFILTER = 9,
    rwRENDERSTATESRCBLEND = 10,
    rwRENDERSTATEDESTBLEND = 11,
    rwRENDERSTATEVERTEXALPHAENABLE = 12,
    rwRENDERSTATEFOGCOLOR = 15,
    rwRENDERSTATECULLMODE = 20,
    rwRENDERSTATESTENCILENABLE = 21,
    rwRENDERSTATEALPHATESTFUNCTION = 29,
    rwRENDERSTATEALPHATESTFUNCTIONREF = 30,
    rwRENDERSTATEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwRenderState;

/* RwBlendFunction: values of rwRENDERSTATESRCBLEND and rwRENDERSTATEDESTBLEND. */
typedef enum RwBlendFunction {
    rwBLENDNABLEND = 0,
    rwBLENDZERO = 1,
    rwBLENDONE = 2,
    rwBLENDSRCCOLOR = 3,
    rwBLENDINVSRCCOLOR = 4,
    rwBLENDSRCALPHA = 5,
    rwBLENDINVSRCALPHA = 6,
    rwBLENDDESTALPHA = 7,
    rwBLENDINVDESTALPHA = 8,
    rwBLENDDESTCOLOR = 9,
    rwBLENDFUNCTIONFORCEENUMSIZEINT = 0x7FFFFFFF
} RwBlendFunction;

/* RwCullMode: values of rwRENDERSTATECULLMODE. */
typedef enum RwCullMode {
    rwCULLMODENACULLMODE = 0,
    rwCULLMODECULLNONE = 1,
    rwCULLMODECULLBACK = 2,
    rwCULLMODECULLFRONT = 3,
    rwCULLMODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwCullMode;

/*
 * Values of rwRENDERSTATEALPHATESTFUNCTION. The member names are the RW 3.7 user guide's
 * (electronicarts/RenderWare3Docs).
 */
typedef enum RwAlphaTestFunction {
    rwALPHATESTFUNCTIONNAALPHATESTFUNCTION = 0,
    rwALPHATESTFUNCTIONNEVER = 1,
    rwALPHATESTFUNCTIONLESS = 2,
    rwALPHATESTFUNCTIONEQUAL = 3,
    rwALPHATESTFUNCTIONLESSEQUAL = 4,
    rwALPHATESTFUNCTIONGREATER = 5,
    rwALPHATESTFUNCTIONNOTEQUAL = 6,
    rwALPHATESTFUNCTIONGREATEREQUAL = 7,
    rwALPHATESTFUNCTIONALWAYS = 8,
    rwALPHATESTFUNCTIONFORCEENUMSIZEINT = 0x7FFFFFFF
} RwAlphaTestFunction;

/* RwPrimitiveType: how a vertex list forms primitives. */
typedef enum RwPrimitiveType {
    rwPRIMTYPENAPRIMTYPE = 0,
    rwPRIMTYPELINELIST = 1,
    rwPRIMTYPEPOLYLINE = 2,
    rwPRIMTYPETRILIST = 3,
    rwPRIMTYPETRISTRIP = 4,
    rwPRIMTYPETRIFAN = 5,
    rwPRIMTYPEPOINTLIST = 6,
    rwPRIMTYPESPRITELIST = 7,   /* not in the stock SDK: Gormiti DWARF */
    rwPRIMITIVETYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwPrimitiveType;

/* RwIm2DVertex: a screen-space vertex of the Wii driver. */
typedef struct RwIm2DVertex {
    RwReal x;       /* 0x0 */
    RwReal y;       /* 0x4 */
    RwReal z;       /* 0x8 */
    RwUInt8 r;      /* 0xC */
    RwUInt8 g;      /* 0xD */
    RwUInt8 b;      /* 0xE */
    RwUInt8 a;      /* 0xF */
    RwReal u;       /* 0x10 */
    RwReal v;       /* 0x14 */
} RwIm2DVertex;

#define RwIm2DVertexSetScreenX(vert, scrnx) ((vert)->x = (scrnx))
#define RwIm2DVertexSetScreenY(vert, scrny) ((vert)->y = (scrny))
#define RwIm2DVertexSetScreenZ(vert, scrnz) ((vert)->z = (scrnz))
#define RwIm2DVertexSetU(vert, texU, recipz) ((vert)->u = (texU))
#define RwIm2DVertexSetV(vert, texV, recipz) ((vert)->v = (texV))
#define RwIm2DVertexSetIntRGBA(vert, red, green, blue, alpha) \
    ((vert)->r = (RwUInt8)(red), (vert)->g = (RwUInt8)(green), (vert)->b = (RwUInt8)(blue), \
     (vert)->a = (RwUInt8)(alpha))

typedef RwBool (*RwSystemFunc)(RwInt32 nOption, void* pOut, void* pInOut, RwInt32 nIn);
typedef RwBool (*RwRenderStateSetFunction)(RwRenderState nState, void* pParam);
typedef RwBool (*RwRenderStateGetFunction)(RwRenderState nState, void* pParam);
typedef RwBool (*RwIm2DRenderLineFunction)(RwIm2DVertex* vertices, RwInt32 numVertices,
                                           RwInt32 vert1, RwInt32 vert2);
typedef RwBool (*RwIm2DRenderTriangleFunction)(RwIm2DVertex* vertices, RwInt32 numVertices,
                                               RwInt32 vert1, RwInt32 vert2, RwInt32 vert3);
typedef RwBool (*RwIm2DRenderPrimitiveFunction)(RwPrimitiveType primType, RwIm2DVertex* vertices,
                                                RwInt32 numVertices);
typedef RwBool (*RwIm2DRenderIndexedPrimitiveFunction)(RwPrimitiveType primType, RwIm2DVertex* vertices,
                                                       RwInt32 numVertices, RwUInt16* indices,
                                                       RwInt32 numIndices);
typedef RwBool (*RwIm3DRenderLineFunction)(RwInt32 vert1, RwInt32 vert2);
typedef RwBool (*RwIm3DRenderTriangleFunction)(RwInt32 vert1, RwInt32 vert2, RwInt32 vert3);
typedef RwBool (*RwIm3DRenderPrimitiveFunction)(RwPrimitiveType primType);
typedef RwBool (*RwIm3DRenderIndexedPrimitiveFunction)(RwPrimitiveType primtype, RwUInt16* indices,
                                                       RwInt32 numIndices);

/* RwDevice: the driver's entry points and z-buffer range. */
typedef struct RwDevice {
    RwReal gammaCorrection;                                             /* 0x0 */
    RwSystemFunc fpSystem;                                              /* 0x4 */
    RwReal zBufferNear;                                                 /* 0x8 */
    RwReal zBufferFar;                                                  /* 0xC */
    RwRenderStateSetFunction fpRenderStateSet;                          /* 0x10 */
    RwRenderStateGetFunction fpRenderStateGet;                          /* 0x14 */
    RwIm2DRenderLineFunction fpIm2DRenderLine;                          /* 0x18 */
    RwIm2DRenderTriangleFunction fpIm2DRenderTriangle;                  /* 0x1C */
    RwIm2DRenderPrimitiveFunction fpIm2DRenderPrimitive;                /* 0x20 */
    RwIm2DRenderIndexedPrimitiveFunction fpIm2DRenderIndexedPrimitive;  /* 0x24 */
    RwIm3DRenderLineFunction fpIm3DRenderLine;                          /* 0x28 */
    RwIm3DRenderTriangleFunction fpIm3DRenderTriangle;                  /* 0x2C */
    RwIm3DRenderPrimitiveFunction fpIm3DRenderPrimitive;                /* 0x30 */
    RwIm3DRenderIndexedPrimitiveFunction fpIm3DRenderIndexedPrimitive;  /* 0x34 */
} RwDevice;

/* The requests the core passes to the driver's fpSystem (tgsm/pbg badevice.h). */
typedef enum RwCoreDeviceSystemFn {
    rwDEVICESYSTEMOPEN = 0,
    rwDEVICESYSTEMCLOSE = 1,
    rwDEVICESYSTEMSTART = 2,
    rwDEVICESYSTEMSTOP = 3,
    rwDEVICESYSTEMREGISTER = 4,
    rwDEVICESYSTEMGETNUMMODES = 5,
    rwDEVICESYSTEMGETMODEINFO = 6,
    rwDEVICESYSTEMUSEMODE = 7,
    rwDEVICESYSTEMFOCUS = 8,
    rwDEVICESYSTEMINITPIPELINE = 9,
    rwDEVICESYSTEMGETMODE = 10,
    rwDEVICESYSTEMSTANDARDS = 11,
    rwDEVICESYSTEMGETTEXMEMSIZE = 12,
    rwDEVICESYSTEMGETNUMSUBSYSTEMS = 13,
    rwDEVICESYSTEMGETSUBSYSTEMINFO = 14,
    rwDEVICESYSTEMGETCURRENTSUBSYSTEM = 15,
    rwDEVICESYSTEMSETSUBSYSTEM = 16,
    rwDEVICESYSTEMFINALIZESTART = 17,
    rwDEVICESYSTEMINITIATESTOP = 18,
    rwDEVICESYSTEMGETMAXTEXTURESIZE = 19,
    rwDEVICESYSTEMRXPIPELINEREQUESTPIPE = 20,
    rwDEVICESYSTEMGETMETRICBLOCK = 21,
    rwDEVICESYSTEMGETID = 22,
    rwDEVICESYSTEMDD = 0x1000,
    rwCOREDEVICESYSTEMFNFORCEENUMSIZEINT = 0x7FFFFFFF
} RwCoreDeviceSystemFn;

/* RwMetrics: the driver's per-frame counters (bfbbdecomp/bfbb rwplcore.h). */
typedef struct RwMetrics {
    RwUInt32 numTriangles;        /* 0x0 */
    RwUInt32 numProcTriangles;    /* 0x4 */
    RwUInt32 numVertices;         /* 0x8 */
    RwUInt32 numTextureUploads;   /* 0xC */
    RwUInt32 sizeTextureUploads;  /* 0x10 */
    RwUInt32 numResourceAllocs;   /* 0x14 */
    void* devSpecificMetrics;     /* 0x18 */
} RwMetrics;

/* The engine's state: RwEngineInit, RwEngineOpen and RwEngineStart each move it one step on. */
typedef enum RwEngineStatus {
    rwENGINESTATUSIDLE = 0,
    rwENGINESTATUSINITED = 1,
    rwENGINESTATUSOPENED = 2,
    rwENGINESTATUSSTARTED = 3,
    rwENGINESTATUSFORCEENUMSIZEINT = 0x7FFFFFFF
} RwEngineStatus;

/* RwEngineInit's flags. */
typedef enum RwEngineInitFlag {
    rwENGINEINITFREELISTS = 0,
    rwENGINEINITNOFREELISTS = 0x1,
    rwENGINEINITFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwEngineInitFlag;

/* RwEngineOpen's parameters: the display the driver opens. */
typedef struct RwEngineOpenParams {
    void* displayID;  /* 0x0 */
} RwEngineOpenParams;

/* RwSubSystemInfo: a sub system's name (tgsm/pbg badevice.h). */
typedef struct RwSubSystemInfo {
    RwChar name[80];  /* 0x0 */
} RwSubSystemInfo;

/* RwVideoModeFlag: a video mode's properties. */
typedef enum RwVideoModeFlag {
    rwVIDEOMODEEXCLUSIVE = 0x0001,
    rwVIDEOMODEINTERLACE = 0x0002,
    rwVIDEOMODEFFINTERLACE = 0x0004,
    rwVIDEOMODEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwVideoModeFlag;

/* RwVideoMode: one of the driver's video modes (bfbbdecomp/bfbb rwplcore.h). */
typedef struct RwVideoMode {
    RwInt32 width;          /* 0x0 */
    RwInt32 height;         /* 0x4 */
    RwInt32 depth;          /* 0x8 */
    RwVideoModeFlag flags;  /* 0xC */
    RwInt32 refRate;        /* 0x10 */
    RwInt32 format;         /* 0x14 */
} RwVideoMode;

/* A driver's standard function: the core calls it through RwGlobals' table. */
typedef RwBool (*RwStandardFunc)(void* pOut, void* pInOut, RwInt32 nI);

/* The slots of the standard function table. */
#define rwSTANDARDNASTANDARD            0
#define rwSTANDARDCAMERABEGINUPDATE     1
#define rwSTANDARDRGBTOPIXEL            2
#define rwSTANDARDPIXELTORGB            3
#define rwSTANDARDRASTERCREATE          4
#define rwSTANDARDRASTERDESTROY         5
#define rwSTANDARDIMAGEGETRASTER        6
#define rwSTANDARDRASTERSETIMAGE        7
#define rwSTANDARDTEXTURESETRASTER      8
#define rwSTANDARDIMAGEFINDRASTERFORMAT 9
#define rwSTANDARDCAMERAENDUPDATE       10
#define rwSTANDARDSETRASTERCONTEXT      11
#define rwSTANDARDRASTERSUBRASTER       12
#define rwSTANDARDRASTERCLEARRECT       13
#define rwSTANDARDRASTERCLEAR           14
#define rwSTANDARDRASTERLOCK            15
#define rwSTANDARDRASTERUNLOCK          16
#define rwSTANDARDRASTERRENDER          17
#define rwSTANDARDRASTERRENDERSCALED    18
#define rwSTANDARDRASTERRENDERFAST      19
#define rwSTANDARDRASTERSHOWRASTER      20
#define rwSTANDARDCAMERACLEAR           21
#define rwSTANDARDHINTRENDERF2B         22
#define rwSTANDARDRASTERLOCKPALETTE     23
#define rwSTANDARDRASTERUNLOCKPALETTE   24
#define rwSTANDARDNATIVETEXTUREGETSIZE  25
#define rwSTANDARDNATIVETEXTUREREAD     26
#define rwSTANDARDNATIVETEXTUREWRITE    27
#define rwSTANDARDRASTERGETMIPLEVELS    28
#define rwSTANDARDNUMOFSTANDARD         29

/*
 * RwGlobals: the core's globals, at the start of the block RwEngineInstance points to. Only the
 * members the reconstructed code reaches are declared; the rest keep their offsets.
 */
typedef struct RwGlobals {
    void* curCamera;                                  /* 0x0 */
    void* curWorld;                                   /* 0x4 */
    RwUInt16 renderFrame;                             /* 0x8 */
    RwUInt16 lightFrame;                              /* 0xA */
    RwUInt16 pad[2];                            /* 0xC */ // pad: name as in the RenderWare SDK
    RwDevice dOpenDevice;                             /* 0x10 */
    RwStandardFunc stdFunc[rwSTANDARDNUMOFSTANDARD];  /* 0x48 */
    RwLinkList dirtyFrameList;                        /* 0xBC */
    RwStringFunctions stringFuncs;                    /* 0xC4 */
    RwMemoryFunctions memoryFuncs;                    /* 0x108 */
    RwMemoryAllocFn memoryAlloc;                      /* 0x118: RwFreeListAlloc */
    RwMemoryFreeFn memoryFree;                        /* 0x11C: RwFreeListFree */
    RwMetrics* metrics;                               /* 0x120 */
    RwEngineStatus engineStatus;                      /* 0x124 */
    RwUInt32 resArenaInitSize;                        /* 0x128 */
} RwGlobals;

/* The engine's globals block (an RwGlobals, followed by every module's globals). */
extern void* RwEngineInstance;

RwBool _rwDeviceSystemRequest(RwDevice* device, RwInt32 requestID, void* pOut, void* pInOut,
                              RwInt32 nIn);
/* The number of open engine instances. */
RwUInt32 _rwGetNumEngineInstances(void);
RwUInt32 RwEngineGetVersion(void);
RwInt32 RwEngineRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor initCB,
                               RwPluginObjectDestructor termCB);
RwInt32 RwEngineGetPluginOffset(RwUInt32 pluginID);
RwInt32 RwEngineGetNumSubSystems(void);
RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex);
RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex);
RwInt32 RwEngineGetNumVideoModes(void);
RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* modeinfo, RwInt32 modeIndex);
RwBool RwEngineSetVideoMode(RwInt32 modeIndex);
RwBool RwEngineStop(void);
RwBool RwEngineStart(void);
RwBool RwEngineClose(void);
RwBool RwEngineOpen(RwEngineOpenParams* initParams);
RwBool RwEngineTerm(void);
RwBool RwEngineInit(const RwMemoryFunctions* memFuncs, RwUInt32 initFlags, RwUInt32 resArenaSize);

/* A member of the core's globals. */
#define RWSRCGLOBAL(variable) (((RwGlobals*)RwEngineInstance)->variable)

#define RwMalloc(_s, _h) (RWSRCGLOBAL(memoryFuncs).rwmalloc((_s), (_h)))
#define RwFree(_p) (RWSRCGLOBAL(memoryFuncs).rwfree((_p)))
#define RwRealloc(_p, _s, _h) (RWSRCGLOBAL(memoryFuncs).rwrealloc((_p), (_s), (_h)))
#define RwCalloc(_n, _s, _h) (RWSRCGLOBAL(memoryFuncs).rwcalloc((_n), (_s), (_h)))

#define RwRenderStateSet(_state, _value) (RWSRCGLOBAL(dOpenDevice).fpRenderStateSet((_state), (_value)))
#define RwIm2DRenderPrimitive(_primType, _vertices, _numVertices) \
    (RWSRCGLOBAL(dOpenDevice).fpIm2DRenderPrimitive((_primType), (_vertices), (_numVertices)))
#define RwIm2DGetNearScreenZ() (RWSRCGLOBAL(dOpenDevice).zBufferNear)
#define RwIm2DGetFarScreenZ() (RWSRCGLOBAL(dOpenDevice).zBufferFar)

/* The string table's strcpy (bfbbdecomp/bfbb rwplcore.h). */
#define rwstrcpy RWSRCGLOBAL(stringFuncs).vecStrcpy

#define RwFreeListAlloc(_f, _h) (RWSRCGLOBAL(memoryAlloc)((_f), (_h)))
/* RwFreeListAlloc as in the RW 3.7 user guide; RwFreeListFree, its counterpart, is in no reachable public source (name unconfirmed). */
#define RwFreeListFree(_f, _p) (RWSRCGLOBAL(memoryFree)((_f), (_p)))

/* The plugin data of type _type, _offset bytes into _base. */
#define RWPLUGINOFFSET(_type, _base, _offset) \
    ((_type*)((RwUInt8*)(_base) + (_offset)))

/* The same, read-only. */
#define RWPLUGINOFFSETCONST(_type, _base, _offset) \
    ((const _type*)((const RwUInt8*)(_base) + (_offset)))

#ifdef __cplusplus
}
#endif

#endif
