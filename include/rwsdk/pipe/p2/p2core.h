#ifndef RWSDK_PIPE_P2_P2CORE_H
#define RWSDK_PIPE_P2_P2CORE_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/pipe/p2/p2heap.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RxPipelineNode;
struct RxNodeDefinition;
struct rxReq;

/* Cluster flags: the data of an external cluster is not the pipeline heap's to free. */
#define rxCLFLAGS_NULL      ((RwUInt16)0x0000U)
#define rxCLFLAGS_EXTERNAL  ((RwUInt16)0x0002U)

/* Node editability (Gormiti DWARF). */
typedef enum RxNodeDefEditable {
    rxNODEDEFCONST = 0,
    rxNODEDEFEDITABLE = 1,
    rxNODEDEFEDITABLEFORCEENUMSIZEINT = 0x7FFFFFFF
} RxNodeDefEditable;

/* Cluster presence requirement (Gormiti DWARF). */
typedef enum RxClusterForcePresent {
    rxCLALLOWABSENT = 0,
    rxCLFORCEPRESENT = 1,
    rxCLUSTERFORCEPRESENTFORCEENUMSIZEINT = 0x7FFFFFFF
} RxClusterForcePresent;

/* Cluster validity state on a node output (Gormiti DWARF). */
typedef enum RxClusterValid {
    rxCLVALID_NOCHANGE = 0,
    rxCLVALID_VALID = 1,
    rxCLVALID_INVALID = 2,
    rxCLUSTERVALIDFORCEENUMSIZEINT = 0x7FFFFFFF
} RxClusterValid;

/* RxClusterDefinition: describes a data cluster type (Gormiti DWARF). */
typedef struct RxClusterDefinition {
    RwChar* name;                           /* 0x00 */
    RwUInt32 defaultStride;                 /* 0x04 */
    RwUInt32 defaultAttributes;             /* 0x08 */
    RwChar* attributeSet;                   /* 0x0C */
} RxClusterDefinition;

/* RxClusterRef: cluster of interest in a node's I/O specification (Gormiti DWARF). */
typedef struct RxClusterRef {
    RxClusterDefinition* clusterDef;        /* 0x00 */
    RxClusterForcePresent forcePresent;     /* 0x04 */
    RwUInt32 reserved;                      /* 0x08 */
} RxClusterRef;

/* RxOutputSpec: one output slot of a pipeline node definition (Gormiti DWARF). */
typedef struct RxOutputSpec {
    RwChar* name;                           /* 0x00 */
    RxClusterValid* outputClusters;         /* 0x04 */
    RxClusterValid allOtherClusters;        /* 0x08 */
} RxOutputSpec;

/* RxIoSpec: inputs and outputs of an RxNodeDefinition (Gormiti DWARF). */
typedef struct RxIoSpec {
    RwUInt32 numClustersOfInterest;         /* 0x00 */
    RxClusterRef* clustersOfInterest;       /* 0x04 */
    RxClusterValid* inputRequirements;      /* 0x08 */
    RwUInt32 numOutputs;                    /* 0x0C */
    RxOutputSpec* outputs;                  /* 0x10 */
} RxIoSpec;


/* RxPipelineNodeParam: data and heap passed to a node body during execution (Gormiti DWARF). */
typedef struct RxPipelineNodeParam {
    void* dataParam;                        /* 0x00 */
    RxHeap* heap;                           /* 0x04 */
} RxPipelineNodeParam;

/* Node callback types (Gormiti DWARF RxNodeMethods). */
typedef RwBool (*RxNodeBodyFn)(struct RxPipelineNode* self, const RxPipelineNodeParam* params);
typedef RwBool (*RxNodeInitFn)(struct RxNodeDefinition* self);
typedef void (*RxNodeTermFn)(struct RxNodeDefinition* self);
typedef RwBool (*RxPipelineNodeInitFn)(struct RxPipelineNode* self);
typedef void (*RxPipelineNodeTermFn)(struct RxPipelineNode* self);
typedef RwBool (*RxPipelineNodeConfigFn)(struct RxPipelineNode* self, RxPipeline* pipeline);
typedef RwUInt32 (*RxConfigMsgHandlerFn)(struct RxPipelineNode* self, RwUInt32 msg,
                                        RwUInt32 intparam, void* ptrparam);

/* RxNodeMethods: method table of an RxNodeDefinition (Gormiti DWARF). Size 0x1C. */
typedef struct RxNodeMethods {
    RxNodeBodyFn nodeBody;                          /* 0x00 */
    RxNodeInitFn nodeInit;                          /* 0x04 */
    RxNodeTermFn nodeTerm;                          /* 0x08 */
    RxPipelineNodeInitFn pipelineNodeInit;          /* 0x0C */
    RxPipelineNodeTermFn pipelineNodeTerm;          /* 0x10 */
    RxPipelineNodeConfigFn pipelineNodeConfig;      /* 0x14 */
    RxConfigMsgHandlerFn configMsgHandler;          /* 0x18 */
} RxNodeMethods;

/* RxNodeDefinition: template for a pipeline node (Gormiti DWARF). Size 0x40. */
typedef struct RxNodeDefinition {
    RwChar* name;                           /* 0x00 */
    RxNodeMethods nodeMethods;              /* 0x04 */
    RxIoSpec io;                            /* 0x20 */
    RwUInt32 pipelineNodePrivateDataSize;   /* 0x34 */
    RxNodeDefEditable editable;             /* 0x38 */
    RwInt32 InputPipesCnt;                  /* 0x3C */
} RxNodeDefinition;

/* RxPipelineCluster: a cluster instance allocated within a pipeline (Gormiti DWARF). */
typedef struct RxPipelineCluster {
    RxClusterDefinition* clusterRef;        /* 0x00 */
    RwUInt32 creationAttributes;            /* 0x04 */
} RxPipelineCluster;

/* RxPipelineNodeTopSortData: topological-sort state for a locked pipeline node (Gormiti DWARF). */
typedef struct RxPipelineNodeTopSortData {
    RwUInt32 numIns;                        /* 0x00 */
    RwUInt32 numInsVisited;                 /* 0x04 */
    struct rxReq* req;                      /* 0x08 */
} RxPipelineNodeTopSortData;

/* RxPipelineNode: one node instance inside an RxPipeline (Gormiti DWARF). Size 0x28. */
typedef struct RxPipelineNode {
    RxNodeDefinition* nodeDef;              /* 0x00 */
    RwUInt32 numOutputs;                    /* 0x04 */
    RwUInt32* outputs;                      /* 0x08 */
    RxPipelineCluster** slotClusterRefs;    /* 0x0C */
    RwUInt32* slotsContinue;                /* 0x10 */
    void* privateData;                      /* 0x14 */
    RwUInt32* inputToClusterSlot;           /* 0x18 */
    RxPipelineNodeTopSortData* topSortData; /* 0x1C */
    void* initializationData;               /* 0x20 */
    RwUInt32 initializationDataSize;        /* 0x24 */
} RxPipelineNode;

/* RxCluster: one array of data (vertices, indices ...) travelling through a pipeline. Size 0x1C. */
typedef struct RxCluster {
    RwUInt16 flags;                         /* 0x00 */
    RwUInt16 stride;                        /* 0x02 */
    void* data;                             /* 0x04 */
    void* currentData;                      /* 0x08 */
    RwUInt32 numAlloced;                    /* 0x0C */
    RwUInt32 numUsed;                       /* 0x10 */
    RxPipelineCluster* clusterRef;          /* 0x14 */
    RwUInt32 attributes;                    /* 0x18 */
} RxCluster;

/* RxPacket: what a pipeline's nodes pass on: its clusters (as many as the widest node needs). */
typedef struct RxPacket {
    RwUInt16 flags;                         /* 0x00 */
    RwUInt16 numClusters;                   /* 0x02 */
    RxPipeline* pipeline;                   /* 0x04 */
    RwUInt32* inputToClusterSlot;           /* 0x08 */
    RwUInt32* slotsContinue;                /* 0x0C */
    RxPipelineCluster** slotClusterRefs;    /* 0x10 */
    RxCluster clusters[1];                  /* 0x14 */
} RxPacket;

/* The pipeline execution heap (p2heap.c), shared by every pipeline (p2core.c). */
extern RxHeap* _rxHeapGlobal;
extern RwBool RxPipelineInstanced;

void _rxPacketDestroy(RxPacket* packet);

/* A new, empty pipeline (p2core.c). */
RxPipeline* RxPipelineCreate(void);
RxPipelineNode* fn_8029B470(RxPipelineNode* node, RxPipeline* pipeline);

/* The pipeline heap shared by every pipeline (p2heap.c). */
RxHeap* RxHeapGetGlobalHeap(void);


#ifdef __cplusplus
}
#endif

#endif
