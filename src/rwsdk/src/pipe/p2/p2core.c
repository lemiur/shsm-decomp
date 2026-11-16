// Unit name and boundary: Gormiti link map (RwCore p2core.obj)
// Function and struct layouts: Gormiti DWARF (p2core.c).
/*
 * p2core.c: pipeline system initialisation, creation, execution and destruction.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <string.h>

typedef struct rxPipelineExecuteData {
    RxPipeline* pipeline;
    RxPipelineNode* currentNode;
    RwBool result;
    RwUInt32 field_0x0C;
    RxPipelineNodeParam param;
} rxPipelineExecuteData;

static rxPipelineExecuteData lbl_804F6F80 = {0};
static RwFreeList lbl_804F6F98;

static RwUInt32 lbl_80528ED0 = 0x1000;
static RwUInt32 lbl_80528ED4 = 0x40;
static RwUInt32 lbl_80528ED8 = 0x40;
static RwUInt32 lbl_80528EDC = 0x1;

RxHeap* _rxHeapGlobal;
RwBool RxPipelineInstanced;

/*
 * Closes the pipeline system, destroying the pipeline free list and the global pipeline heap.
 */
RwBool _rxPipelineClose(void) {
    if (RxPipelineInstanced) {
        RwFreeListDestroy(RXPIPELINEGLOBAL(pipesFreeList));
        RXPIPELINEGLOBAL(pipesFreeList) = (RwFreeList*)NULL;
        RxHeapDestroy(_rxHeapGlobal);
        _rxHeapGlobal = (RxHeap*)NULL;
        RxPipelineInstanced = FALSE;
    }

    return TRUE;
} // 0x8029B318

/*
 * Opens the pipeline system, creating the global pipeline heap and preallocating the pipeline
 * free list.
 */
RwBool _rxPipelineOpen(void) {
    if (!RxPipelineInstanced) {
        _rxHeapGlobal = RxHeapCreate(lbl_80528ED0);
        if (!_rxHeapGlobal) {
            return FALSE;
        }

        RXPIPELINEGLOBAL(pipesFreeList) = RwFreeListCreateAndPreallocateSpace(
            sizeof(RxPipeline), lbl_80528ED8, sizeof(RwUInt32), lbl_80528EDC, &lbl_804F6F98,
            rwMEMHINTDUR_GLOBAL | rwID_PIPEMODULE);
        if (!RXPIPELINEGLOBAL(pipesFreeList)) {
            RxHeapDestroy(_rxHeapGlobal);
            _rxHeapGlobal = (RxHeap*)NULL;
            return FALSE;
        }

        RXPIPELINEGLOBAL(field_0x38) = lbl_80528ED4;
        RxRenderStateVectorSetDefaultRenderStateVector(&RXPIPELINEGLOBAL(defaultRenderState));
        RXPIPELINEGLOBAL(field_0x34) = 0;
        RXPIPELINEGLOBAL(field_0x30) = 0;
        RxPipelineInstanced = TRUE;
        return TRUE;
    }

    return FALSE;
} // 0x8029B37C

/*
 * PipelineNodeDestroy (Gormiti DWARF): destroys a node within a pipeline, shifting subsequent
 * node data down if the pipeline is locked.
 */
RxPipelineNode* fn_8029B470(RxPipelineNode* node, RxPipeline* pipeline) {
    RwInt32 nodeIndex;

    if (!pipeline->locked) {
        if (node->nodeDef->nodeMethods.pipelineNodeTerm != NULL) {
            node->nodeDef->nodeMethods.pipelineNodeTerm(node);
        }
        node->nodeDef->InputPipesCnt--;
        if (node->nodeDef->InputPipesCnt == 0) {
            if (node->nodeDef->nodeMethods.nodeTerm != NULL) {
                node->nodeDef->nodeMethods.nodeTerm(node->nodeDef);
            }
            if (node->nodeDef->editable != rxNODEDEFCONST) {
                RwFree(node->nodeDef);
                node->nodeDef = (RxNodeDefinition*)NULL;
            }
        }
        if (node->initializationData != NULL) {
            RwFree(node->initializationData);
            node->initializationData = NULL;
            node->initializationDataSize = 0;
        }
        memset(node, 0, sizeof(RxPipelineNode));
    } else {
        if (node->initializationData != NULL) {
            RwFree(node->initializationData);
            node->initializationData = NULL;
            node->initializationDataSize = 0;
        }
        if (node->nodeDef->InputPipesCnt == 0 && node->nodeDef->editable != rxNODEDEFCONST) {
            RwFree(node->nodeDef);
            node->nodeDef = (RxNodeDefinition*)NULL;
        }
        nodeIndex = node - pipeline->nodes;
        if ((RwUInt32)nodeIndex < pipeline->numNodes - 1) {
            RxPipelineNodeTopSortData* topSortData;
            RxPipelineNodeTopSortData* nextTopSortData;
            RwUInt32* output;
            RwUInt32* nextOutput;
            RwUInt32 i;
            RwUInt32 j;

            output = (RwUInt32*)(pipeline->nodes + RXPIPELINEGLOBAL(field_0x38));
            output += nodeIndex * rxNODE_MAX_OUTPUTS;
            nextOutput = output + rxNODE_MAX_OUTPUTS;
            for (i = nodeIndex; i < pipeline->numNodes - 1; i++) {
                memcpy(output, nextOutput, rxNODE_MAX_OUTPUTS * sizeof(RwUInt32));
                output = nextOutput;
                nextOutput += rxNODE_MAX_OUTPUTS;
            }

            topSortData = (RxPipelineNodeTopSortData*)(pipeline->nodes + RXPIPELINEGLOBAL(field_0x38));
            topSortData = (RxPipelineNodeTopSortData*)((RwUInt32*)topSortData +
                                                       RXPIPELINEGLOBAL(field_0x38) * rxNODE_MAX_OUTPUTS);
            nextTopSortData = topSortData + 1;
            for (i = nodeIndex; i < pipeline->numNodes - 1; i++) {
                memcpy(topSortData, nextTopSortData, sizeof(RxPipelineNodeTopSortData));
                topSortData = nextTopSortData;
                nextTopSortData++;
            }

            for (i = nodeIndex; i < pipeline->numNodes - 1; i++) {
                memcpy(&pipeline->nodes[i], &pipeline->nodes[i + 1], sizeof(RxPipelineNode));
                pipeline->nodes[i].outputs -= rxNODE_MAX_OUTPUTS;
                pipeline->nodes[i].topSortData -= 1;
            }

            for (i = 0; i < pipeline->numNodes - 1; i++) {
                for (j = 0; j < pipeline->nodes[i].numOutputs; j++) {
                    if (pipeline->nodes[i].outputs[j] >= (RwUInt32)nodeIndex) {
                        if (pipeline->nodes[i].outputs[j] == (RwUInt32)nodeIndex) {
                            pipeline->nodes[i].outputs[j] = (RwUInt32)-1;
                        } else {
                            pipeline->nodes[i].outputs[j]--;
                        }
                    }
                }
            }
        }
    }

    pipeline->numNodes--;
    return node;
} // 0x8029B470

/*
 * Returns the global pipeline heap.
 */
RxHeap* RxHeapGetGlobalHeap(void) {
    return _rxHeapGlobal;
} // 0x8029B780

/*
 * Executes a pipeline over data, optionally resetting the global heap first.
 */
RxPipeline* RxPipelineExecute(RxPipeline* pipeline, void* data, RwBool heapReset) {
    RxPipelineNode* node;
    RwUInt32 exitCode;

    if (heapReset && _rxHeapGlobal->dirty) {
        _rxHeapReset(_rxHeapGlobal);
    }

    lbl_804F6F80.result = TRUE;
    lbl_804F6F80.pipeline = pipeline;
    lbl_804F6F80.param.dataParam = data;
    lbl_804F6F80.param.heap = _rxHeapGlobal;

    pipeline->embeddedPacketState = rxPKST_PACKETLESS;

    node = pipeline->nodes;
    exitCode = node->nodeDef->nodeMethods.nodeBody(node, &lbl_804F6F80.param);
    if (!exitCode) {
        lbl_804F6F80.result = exitCode;
    }

    if (pipeline->embeddedPacketState > rxPKST_UNUSED) {
        pipeline->embeddedPacketState = rxPKST_INUSE;
        _rxPacketDestroy(pipeline->embeddedPacket);
    }

    lbl_804F6F80.pipeline = (RxPipeline*)NULL;
    lbl_804F6F80.param.dataParam = NULL;
    lbl_804F6F80.param.heap = (RxHeap*)NULL;

    if (lbl_804F6F80.result) {
        return pipeline;
    }

    return (RxPipeline*)NULL;
} // 0x8029B788

/*
 * Allocates and initialises an unlocked empty pipeline from the pipeline free list.
 */
RxPipeline* RxPipelineCreate(void) {
    RxPipeline* pipeline;

    pipeline = (RxPipeline*)RwFreeListAlloc(RXPIPELINEGLOBAL(pipesFreeList),
                                            rwMEMHINTDUR_EVENT | rwID_PIPEMODULE);
    if (pipeline) {
        memset(pipeline, 0, sizeof(RxPipeline));
        pipeline->locked = FALSE;
        return pipeline;
    }

    RWERROR((E_RW_NOMEM, sizeof(RxPipeline)));
    return (RxPipeline*)NULL;
} // 0x8029B878

/*
 * Destroys a pipeline and all of its nodes, freeing its superblock and returning it to the
 * pipeline free list.
 */
void _rxPipelineDestroy(RxPipeline* Pipeline) {
    if (Pipeline != NULL) {
        RwUInt32 numNodes;
        RwUInt32 i;
        RxPipelineNode* Node;

        Node = Pipeline->nodes;
        numNodes = Pipeline->numNodes;
        for (i = 0; i < numNodes; i++) {
            fn_8029B470(Node, Pipeline);
            Node++;
        }

        Pipeline->nodes = (RxPipelineNode*)NULL;

        if (Pipeline->superBlock != NULL) {
            RwFree(Pipeline->superBlock);
            Pipeline->superBlock = NULL;
            Pipeline->superBlockSize = 0;
        }

        RwFreeListFree(RXPIPELINEGLOBAL(pipesFreeList), Pipeline);
    }
} // 0x8029B910
