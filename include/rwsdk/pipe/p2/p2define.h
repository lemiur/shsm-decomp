// Unit name: Gormiti link map (RwCore.a p2define, pipe/p2/p2define.c)
// Prototypes and types: Gormiti DWARF (p2define.c, baim3d.c, p2dep.c)
#ifndef RWSDK_PIPE_P2_P2DEFINE_H
#define RWSDK_PIPE_P2_P2DEFINE_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/pipe/p2/p2core.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RwScopeTrace;
struct rxReq;

/* Cluster validity requirement on a node input (Gormiti DWARF). */
typedef enum RxClusterValidityReq {
    rxCLREQ_DONTWANT = 0,
    rxCLREQ_REQUIRED = 1,
    rxCLREQ_OPTIONAL = 2,
    rxCLUSTERVALIDITYREQFORCEENUMSIZEINT = 0x7FFFFFFF
} RxClusterValidityReq;

/* A pipeline input requirement entry (Gormiti DWARF). Size 0xC. */
typedef struct RxPipelineRequiresCluster {
    RxClusterDefinition* clusterDef;    /* 0x00 */
    RxClusterValidityReq rqdOrOpt;      /* 0x04 */
    RwUInt32 slotIndex;                 /* 0x08 */
} RxPipelineRequiresCluster;

/* Dependency-chase requirement list entry (Gormiti DWARF). Size 0x24. */
typedef struct RwReqEntry {
    RxClusterDefinition* clusterDef;    /* 0x00 */
    RxClusterValidityReq required;      /* 0x04 */
    RwUInt32 inputs;                    /* 0x08 */
    struct RwScopeTrace* st;            /* 0x0C */
    struct RwReqEntry* next;            /* 0x10 */
    struct rxReq* req;                  /* 0x14 */
    RwUInt32 outbf;                     /* 0x18 */
    RwUInt32 assignedslot;              /* 0x1C */
    RxPipelineNode* originatingNode;    /* 0x20 */
} RwReqEntry;

/* Dependency-chase scope trace node (Gormiti DWARF). Size 0x10. */
typedef struct RwScopeTrace {
    RwReqEntry* head;                   /* 0x00 */
    struct RwScopeTrace* continuation;  /* 0x04 */
    struct RwScopeTrace* next;          /* 0x08 */
    struct RwScopeTrace* parent;        /* 0x0C */
} RwScopeTrace;

/* Per-node dependency-chase requirements container (Gormiti DWARF). Size 0x14. */
typedef struct rxReq {
    RwUInt32 numused;                   /* 0x00 */
    RwUInt32 numalloced;                /* 0x04 */
    RwUInt32 unavailslots;              /* 0x08 */
    RwReqEntry* reqentries;             /* 0x0C */
    RxPipelineNode* node;               /* 0x10 */
} rxReq;

/* Double-ended scratch allocator limits inside a pipeline superblock (Gormiti DWARF). Size 0x8. */
typedef struct P2MemoryLimits {
    RwUInt8* StalacTiteBase;            /* 0x00 */
    RwUInt8* StalacMiteBase;            /* 0x04 */
} P2MemoryLimits;

extern P2MemoryLimits lbl_8052B1B8;

void* StalacTiteAlloc(RwUInt32 size);
void* StalacMiteAlloc(RwUInt32 size);
RwUInt32 PipelineCalcNumUniqueClusters(RxPipeline* pipeline);
RwUInt32 _rxChaseDependencies(RxPipeline* pipeline);

/*
 * Building a pipeline: lock it, add fragments (lists of nodes, ended by a NULL), unlock it. A
 * locked pipeline is an RxPipeline in this build.
 */
RxPipeline* RxPipelineLock(RxPipeline* pipeline);
RxPipeline* RxLockedPipeUnlock(RxPipeline* pipeline);
/* The first node of a pipeline named name (after start, when given); nodeIndex receives its index. */
RxPipelineNode* RxPipelineFindNodeByName(RxPipeline* pipeline, const RwChar* name,
                                         RxPipelineNode* start, RwInt32* nodeIndex);
RxPipeline* RxLockedPipeAddFragment(RxPipeline* pipeline, RwUInt32* firstIndex,
                                    RxNodeDefinition* nodeDef0, ...);

#ifdef __cplusplus
}
#endif

#endif
