// Unit name and boundary: Gormiti link map (RwCore.a p2define.obj)
// Function names, locals and structs: Gormiti DWARF (p2define.c).
/*
 * p2define.c: pipeline locking, unlocking, node creation, topological sorting, and fragment
 * construction.
 */
#include <stdarg.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/pipe/p2/p2define.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/rwstring.h>

/* Topological-sort state passed down recursive PipelineTopSort calls (Gormiti DWARF). */
typedef struct tagTopSortData {
    RxPipeline* pipeline;       /* 0x00 */
    RwUInt32 nodesArraySlot;    /* 0x04 */
} tagTopSortData;

P2MemoryLimits lbl_8052B1B8;

/*
 * Allocates size bytes downward from the top of the pipeline superblock scratch region.
 */
void* StalacTiteAlloc(RwUInt32 size) {
    size = (size + 3) & ~3U;

    lbl_8052B1B8.StalacTiteBase -= size;
    if (lbl_8052B1B8.StalacTiteBase < lbl_8052B1B8.StalacMiteBase) {
        lbl_8052B1B8.StalacTiteBase += size;
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    return lbl_8052B1B8.StalacTiteBase;
} // 0x8029B9CC

/*
 * Allocates size bytes upward from the bottom of the pipeline superblock scratch region.
 */
void* StalacMiteAlloc(RwUInt32 size) {
    size = (size + 3) & ~3U;

    lbl_8052B1B8.StalacMiteBase += size;
    if (lbl_8052B1B8.StalacMiteBase > lbl_8052B1B8.StalacTiteBase) {
        lbl_8052B1B8.StalacMiteBase -= size;
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    return (void*)(lbl_8052B1B8.StalacMiteBase - size);
} // 0x8029BA3C

/*
 * Counts the number of distinct RxClusterDefinition pointers referenced across all nodes in the
 * pipeline.
 */
RwUInt32 PipelineCalcNumUniqueClusters(RxPipeline* pipeline) {
    RxClusterDefinition* lastAddress;
    RxClusterDefinition* newAddress;
    RwUInt32 numUniqueClusters;
    RwUInt32 i;
    RwUInt32 j;

    numUniqueClusters = 0;
    newAddress = (RxClusterDefinition*)0;
    do {
        lastAddress = newAddress;
        newAddress = (RxClusterDefinition*)~0U; /* max pointer sentinel */
        for (i = 0; i < pipeline->numNodes; i++) {
            RxNodeDefinition* nodeDef = pipeline->nodes[i].nodeDef;
            for (j = 0; j < nodeDef->io.numClustersOfInterest; j++) {
                RxClusterDefinition* address = nodeDef->io.clustersOfInterest[j].clusterDef;
                if (address > lastAddress && address < newAddress) {
                    newAddress = address;
                }
            }
        }
        if (newAddress == (RxClusterDefinition*)~0U) {
            break;
        }
        numUniqueClusters++;
    } while (1);

    return numUniqueClusters;
} // 0x8029BAB4

/*
 * Reallocates the pipeline's superblock to newSize bytes and fixes up all internal pointers by the
 * base-address delta.
 */
RwBool ReallocAndFixupSuperBlock(RxPipeline* pipeline, RwUInt32 newSize) {
    void* oldBlock = pipeline->superBlock;
    void* newBlock = RwRealloc(oldBlock, newSize,
                               rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_PIPEMODULE);
    RwUInt32 i;

    if (newBlock != NULL) {
        RwUInt32 numNodes = pipeline->numNodes;
        RwInt32 diff = (RwUInt8*)newBlock - (RwUInt8*)oldBlock;

        pipeline->superBlock = newBlock;
        pipeline->superBlockSize = newSize;
        pipeline->nodes = (RxPipelineNode*)newBlock;
        if (pipeline->embeddedPacket != NULL) {
            pipeline->embeddedPacket = (RxPacket*)((RwUInt8*)pipeline->embeddedPacket + diff);
        }
        if (pipeline->inputRequirements != NULL) {
            pipeline->inputRequirements =
                (RxPipelineRequiresCluster*)((RwUInt8*)pipeline->inputRequirements + diff);
        }
        for (i = 0; i < numNodes; i++) {
            if (pipeline->nodes[i].outputs != NULL) {
                pipeline->nodes[i].outputs =
                    (RwUInt32*)((RwUInt8*)pipeline->nodes[i].outputs + diff);
            }
            if (pipeline->nodes[i].slotClusterRefs != NULL) {
                pipeline->nodes[i].slotClusterRefs =
                    (RxPipelineCluster**)((RwUInt8*)pipeline->nodes[i].slotClusterRefs + diff);
            }
            if (pipeline->nodes[i].slotsContinue != NULL) {
                pipeline->nodes[i].slotsContinue =
                    (RwUInt32*)((RwUInt8*)pipeline->nodes[i].slotsContinue + diff);
            }
            if (pipeline->nodes[i].privateData != NULL) {
                pipeline->nodes[i].privateData =
                    (void*)((RwUInt8*)pipeline->nodes[i].privateData + diff);
            }
            if (pipeline->nodes[i].inputToClusterSlot != NULL) {
                pipeline->nodes[i].inputToClusterSlot =
                    (RwUInt32*)((RwUInt8*)pipeline->nodes[i].inputToClusterSlot + diff);
            }
            if (pipeline->nodes[i].topSortData != NULL) {
                pipeline->nodes[i].topSortData =
                    (RxPipelineNodeTopSortData*)((RwUInt8*)pipeline->nodes[i].topSortData + diff);
            }
        }
    } else {
        RWERROR((E_RW_NOMEM, newSize));
        return FALSE;
    }

    return TRUE;
} // 0x8029BB40

/*
 * Expands a pipeline's node outputs and topological-sort data into the fixed-stride locked layout
 * within dstPipe's superblock.
 */
RwBool LockPipelineExpandData(RxPipeline* dstPipe, RxPipeline* srcPipe) {
    RxPipelineNodeTopSortData* topSortArray;
    RwUInt32* outputs;
    RwInt32 i;

    if (dstPipe != srcPipe) {
        for (i = (RwInt32)srcPipe->numNodes - 1; i >= 0; i--) {
            memcpy(&dstPipe->nodes[i], &srcPipe->nodes[i], sizeof(RxPipelineNode));
            dstPipe->nodes[i].slotClusterRefs = NULL;
            dstPipe->nodes[i].slotsContinue = NULL;
            dstPipe->nodes[i].privateData = NULL;
            dstPipe->nodes[i].inputToClusterSlot = NULL;
            if (dstPipe->nodes[i].initializationDataSize != 0) {
                dstPipe->nodes[i].initializationData = RwMalloc(
                    dstPipe->nodes[i].initializationDataSize,
                    rwMEMHINTDUR_EVENT | rwID_PIPEMODULE);
                if (dstPipe->nodes[i].initializationData == NULL) {
                    RWERROR((E_RW_NOMEM, dstPipe->nodes[i].initializationDataSize));
                    return FALSE;
                }
                memcpy(dstPipe->nodes[i].initializationData,
                       srcPipe->nodes[i].initializationData,
                       dstPipe->nodes[i].initializationDataSize);
            }
        }
        dstPipe->numNodes = srcPipe->numNodes;
    }

    outputs = (RwUInt32*)(dstPipe->nodes + RXPIPELINEGLOBAL(field_0x38));
    for (i = (RwInt32)srcPipe->numNodes - 1; i >= 0; i--) {
        dstPipe->nodes[i].outputs = outputs + i * rxNODE_MAX_OUTPUTS;
        if (srcPipe->nodes[i].outputs != NULL) {
            memcpy(dstPipe->nodes[i].outputs, srcPipe->nodes[i].outputs,
                   rxNODE_MAX_OUTPUTS * sizeof(RwUInt32));
        }
    }

    topSortArray = (RxPipelineNodeTopSortData*)(outputs + RXPIPELINEGLOBAL(field_0x38) * rxNODE_MAX_OUTPUTS);
    for (i = 0; (RwUInt32)i < srcPipe->numNodes; i++) {
        topSortArray[i].numIns = 0;
        topSortArray[i].numInsVisited = 0;
        topSortArray[i].req = NULL;
        dstPipe->nodes[i].topSortData = &topSortArray[i];
    }

    return TRUE;
} // 0x8029BCD8

/*
 * Computes the total size in bytes of the compacted per-node output index arrays for an unlocked
 * pipeline (inlined into RxLockedPipeUnlock).
 */
static RwUInt32 CalcNodesOutputsCompactedMemSize(RxPipeline* pipeline) {
    RwUInt32 size;
    RwUInt32 i;

    size = pipeline->numNodes * sizeof(RxPipelineNode);
    for (i = 0; i < pipeline->numNodes; i++) {
        size += pipeline->nodes->numOutputs * sizeof(RwUInt32);
    }
    return size;
} // no copy in retail (inlined): a static in the Gormiti Debug map (p2define.obj), absent from the Final map

/*
 * Computes the total size in bytes of persistent cluster, slot, private-data and embedded-packet
 * allocations for an unlocked pipeline (inlined into RxLockedPipeUnlock).
 */
static RwUInt32 CalcUnlockPersistentMemSize(RxPipeline* pipeline, RwUInt32 numClusters) {
    RwUInt32 blockSize = 0;
    RxPipelineNode* node;
    RwUInt32 i;

    blockSize += numClusters * sizeof(RxPipelineCluster);
    blockSize += numClusters * sizeof(RxPipelineRequiresCluster);
    blockSize += pipeline->numNodes * numClusters * sizeof(RxPipelineCluster*);
    blockSize += pipeline->numNodes * (numClusters + 1) * sizeof(RwUInt32);
    for (i = 0; i < pipeline->numNodes; i++) {
        node = &pipeline->nodes[i];
        if (node->nodeDef->pipelineNodePrivateDataSize != 0) {
            blockSize += node->nodeDef->pipelineNodePrivateDataSize;
        }
        blockSize += node->nodeDef->io.numClustersOfInterest * sizeof(RwUInt32);
    }
    blockSize += sizeof(RxPacket) + (numClusters - 1) * sizeof(RxCluster);
    return blockSize;
} // no copy in retail (inlined): a static in the Gormiti Debug map (p2define.obj), absent from the Final map

/*
 * _NodeCreate (Gormiti DWARF; sibling.exe pairs it only in its weak seq-size tier, so the symbol keeps its placeholder): initialises a new RxPipelineNode slot in a locked pipeline from
 * nodespec and increments pipeline->numNodes on success.
 */
RwBool fn_8029BEF0(RxPipeline* pipeline, RxPipelineNode* node, RxNodeDefinition* nodespec) {
    RxPipelineNodeTopSortData* topSortData;
    RwUInt32* outputs;
    RwBool result;
    RwUInt32 n;

    result = TRUE;
    n = nodespec->io.numOutputs;
    memset(node, 0, sizeof(RxPipelineNode));

    if (n > rxNODE_MAX_OUTPUTS) {
        RWERROR((E_RX_NODETOOMANYOUTPUTS));
        result = FALSE;
    }
    if (nodespec->io.numClustersOfInterest > rxNODE_MAX_OUTPUTS) {
        RWERROR((E_RX_NODETOOMANYCLUSTERSOFINTEREST));
        result = FALSE;
    }
    if (n >= RXPIPELINEGLOBAL(field_0x38)) {
        RWERROR((E_RX_PIPELINETOOMANYNODES));
        result = FALSE;
    }

    if (result) {
        outputs = (RwUInt32*)(pipeline->nodes + RXPIPELINEGLOBAL(field_0x38));
        outputs += pipeline->numNodes * rxNODE_MAX_OUTPUTS;

        RWASSERT(node == &pipeline->nodes[pipeline->numNodes]);

        node->outputs = outputs;
        node->numOutputs = n;
        for (n = 0; n < node->numOutputs; n++) {
            *outputs = (RwUInt32)-1;
            outputs++;
        }

        outputs = (RwUInt32*)(pipeline->nodes + RXPIPELINEGLOBAL(field_0x38));
        topSortData = (RxPipelineNodeTopSortData*)(outputs +
                                                   RXPIPELINEGLOBAL(field_0x38) * rxNODE_MAX_OUTPUTS);
        topSortData += pipeline->numNodes;
        topSortData->numIns = 0;
        topSortData->numInsVisited = 0;
        topSortData->req = NULL;
        node->topSortData = topSortData;
        node->initializationData = NULL;
        node->initializationDataSize = 0;
        node->nodeDef = nodespec;
        pipeline->numNodes++;
    }

    return result;
} // 0x8029BEF0

/*
 * Resets and tallies the incoming edge count (numIns) for each node in the pipeline.
 */
void PipelineTallyInputs(RxPipeline* pipeline) {
    RxPipelineNode* nodes;
    RwUInt32 i;

    nodes = pipeline->nodes;
    for (i = 0; i < pipeline->numNodes; i++) {
        if (nodes->nodeDef != NULL) {
            nodes->topSortData->numInsVisited = 0;
            nodes->topSortData->numIns = 0;
        }
        nodes++;
    }

    nodes = pipeline->nodes;
    for (i = 0; i < pipeline->numNodes; i++) {
        if (nodes->nodeDef != NULL) {
            RwInt32 j = nodes->numOutputs;
            if (j != 0) {
                RwUInt32* outputs = nodes->outputs;
                do {
                    if ((RwInt32)*outputs != -1) {
                        pipeline->nodes[*outputs].topSortData->numIns++;
                    }
                    outputs++;
                } while (--j);
            }
        }
        nodes++;
    }
} // 0x8029C088

/*
 * PipelineTopSort (Gormiti DWARF; the symbol keeps its placeholder: sibling.exe pairs it with no counterpart): recursively places nodeIndex at data->nodesArraySlot (swapping
 * node, output array, and topSortData, and remapping references) and visits ready successors.
 */
void fn_8029C13C(tagTopSortData* data, RwUInt32 nodeIndex) {
    RxPipelineNode* curNode;
    RwUInt32 i = data->nodesArraySlot;
    RwUInt32 j = nodeIndex;

    if (i != j) {
        RwUInt32 tmpOutput;
        RwUInt32* outputsI = data->pipeline->nodes[i].outputs;
        RwUInt32* outputsJ = data->pipeline->nodes[j].outputs;
        RxPipelineNodeTopSortData tempTopSortData;
        RxPipelineNodeTopSortData* topSortDataI;
        RxPipelineNodeTopSortData* topSortDataJ;
        RxPipelineNode tmpNode;
        RwUInt32 k;
        RwUInt32 l;

        for (k = 0; k < rxNODE_MAX_OUTPUTS; k++) {
            tmpOutput = outputsI[k];
            outputsI[k] = outputsJ[k];
            outputsJ[k] = tmpOutput;
        }
        data->pipeline->nodes[i].outputs = outputsJ;
        data->pipeline->nodes[j].outputs = outputsI;

        topSortDataI = data->pipeline->nodes[i].topSortData;
        topSortDataJ = data->pipeline->nodes[j].topSortData;
        tempTopSortData = *topSortDataI;
        *topSortDataI = *topSortDataJ;
        *topSortDataJ = tempTopSortData;
        data->pipeline->nodes[i].topSortData = topSortDataJ;
        data->pipeline->nodes[j].topSortData = topSortDataI;

        tmpNode = data->pipeline->nodes[i];
        data->pipeline->nodes[i] = data->pipeline->nodes[j];
        data->pipeline->nodes[j] = tmpNode;

        for (k = 0; k < data->pipeline->numNodes; k++) {
            RxPipelineNode* node = &data->pipeline->nodes[k];
            for (l = 0; l < node->numOutputs; l++) {
                if (node->outputs[l] == i) {
                    node->outputs[l] = j;
                } else if (node->outputs[l] == j) {
                    node->outputs[l] = i;
                }
            }
        }
    }

    curNode = &data->pipeline->nodes[data->nodesArraySlot];
    data->nodesArraySlot++;
    if (curNode->numOutputs != 0) {
        for (i = 0; i < curNode->numOutputs; i++) {
            RwUInt32 outIndex = curNode->outputs[i];
            if (outIndex != (RwUInt32)-1) {
                RxPipelineNode* outNode = &data->pipeline->nodes[outIndex];
                outNode->topSortData->numInsVisited++;
                if (outNode->topSortData->numIns == outNode->topSortData->numInsVisited) {
                    fn_8029C13C(data, outIndex);
                }
            }
        }
    }
} // 0x8029C13C

/*
 * Validates and converts a node pointer within pipeline->nodes to its node index, or -1 if invalid
 * (inlined into RxLockedPipeAddFragment).
 */
static RwUInt32 PipelineNode2Index(RxPipeline* pipeline, RxPipelineNode* node) {
    RwUInt32 nodeIndex = node - pipeline->nodes;

    if (&pipeline->nodes[nodeIndex] == node && nodeIndex < pipeline->numNodes) {
        return nodeIndex;
    }

    return (RwUInt32)-1;
} // no copy in retail (inlined): a static in the Gormiti Debug map (p2define.obj), absent from the Final map

/*
 * Topologically sorts the locked pipeline starting from pipeline->entryPoint, rejecting cycles,
 * incoming edges on the entry point, and disconnected nodes.
 */
RxPipeline* PipelineUnlockTopSort(RxPipeline* pipeline) {
    tagTopSortData data;
    RwUInt32 i;

    data.pipeline = pipeline;
    data.nodesArraySlot = 0;
    PipelineTallyInputs(pipeline);

    if (pipeline->nodes[pipeline->entryPoint].topSortData->numIns != 0) {
        RWERROR((E_RX_INVALIDENTRYPOINT));
        return (RxPipeline*)NULL;
    }

    for (i = 0; i < pipeline->numNodes; i++) {
        if (i != pipeline->entryPoint && pipeline->nodes[i].topSortData->numIns == 0) {
            RWERROR((E_RX_FRAGMENTEDPIPELINE));
            return (RxPipeline*)NULL;
        }
    }

    fn_8029C13C(&data, pipeline->entryPoint);

    for (i = 0; i < pipeline->numNodes; i++) {
        if (pipeline->nodes[i].topSortData->numIns != pipeline->nodes[i].topSortData->numInsVisited) {
            RWERROR((E_RX_CYCLICPIPELINE));
            return (RxPipeline*)NULL;
        }
    }

    pipeline->entryPoint = 0;
    return pipeline;
} // 0x8029C528

/*
 * Returns a pointer to a node's output slot at outputindex, or NULL if invalid (inlined into
 * RxLockedPipeAddFragment).
 */
static RwUInt32* RxPipelineNodeFindOutputByIndex(RxPipelineNode* node, RwUInt32 outputindex) {
    if (node != NULL && node->nodeDef != NULL && outputindex < node->numOutputs) {
        return &node->outputs[outputindex];
    }
    return NULL;
} // no copy in retail (inlined): an UNUSED entry in the Gormiti Final map (p2define.obj), a function in the Debug map

/*
 * Validates a node pointer for use as a connection target (inlined into RxLockedPipeAddFragment).
 */
static RxPipelineNode* RxPipelineNodeFindInput(RxPipelineNode* node) {
    if (node != NULL && node->nodeDef != NULL) {
        return node;
    }
    return NULL;
} // no copy in retail (inlined): an UNUSED entry in the Gormiti Final map (p2define.obj), a function in the Debug map

/*
 * Connects an unassigned node output slot to an input node within a locked pipeline (inlined into
 * RxLockedPipeAddFragment).
 */
static RxPipeline* RxLockedPipeAddPath(RxPipeline* pipeline, RwUInt32* out, RxPipelineNode* in) {
    if (pipeline != NULL && pipeline->locked && out != NULL && *out == (RwUInt32)-1 &&
        in != NULL && in->nodeDef != NULL) {
        RwUInt32 nodeIndex = PipelineNode2Index(pipeline, in);
        if (nodeIndex != (RwUInt32)-1) {
            *out = nodeIndex;
            return pipeline;
        }
    }
    return (RxPipeline*)NULL;
} // no copy in retail (inlined): an UNUSED entry in the Gormiti Final map (p2define.obj), a function in the Debug map

/*
 * Unlocks a locked pipeline: sorts nodes topologically, compacts outputs, chases cluster
 * dependencies, shrinks the superblock, and initialises and configures all nodes.
 * Statement order and the roles of the block sizes follow the Gormiti Debug line table (another
 * revision: its CalcUnlockPersistentMemSize adds 7 per private-data block, retail does not).
 * NonMatching, register/schedule: the one residue is in the inlined CalcUnlockPersistentMemSize,
 * where retail keeps depChaseBlockSize in r0 across the loop (the loop's temporaries in r4/r5,
 * numClusters reused for the tail) and this source gives it r5. Flat: splitting the partial sum
 * into another size variable, `x = x + f()` and `f() + x` forms, folding unlockStartBlockSize into
 * topSortBlockSize, the tail expression's operand order, levers.exe (136 single edits, none better),
 * `+=` and regrouped forms of the third depChaseBlockSize statement, one statement with the call, every
 * Wii and GC compiler (flagsweep). CalcUnlockPersistentMemSize sits at the auto-inline limit: a block
 * local in its loop (a private-size or nodeDef local) makes the compiler keep it out of line.
 * Also flat: -ipa file, __inline on either size helper, a function-scope nodeDef local or no local in
 * the helper's loop, an extra helper local (the helper's local numbering is not the decider), the
 * helper's tail as a return expression (99.24%), the third and fourth statements merged (94.60%).
 * Retail colours the third statement's value before the inlined loop's temporaries, as if it were a
 * compiler temporary rather than the local.
 */
RxPipeline* RxLockedPipeUnlock(RxPipeline* pipeline) {
    if (pipeline != NULL && pipeline->locked) {
        RxPipelineNodeTopSortData* newTopSortData;
        RxPipelineNodeTopSortData* topSortData;
        RwUInt32 numUniqueClusters;
        RwUInt32 unlockStartBlockSize;
        RwUInt32 unlockEndBlockSize;
        RwUInt32 topSortBlockSize;
        RwUInt32 depChaseBlockSize;
        RwUInt32 totalOutputs;
        RwUInt32* newOutputs;
        RwUInt32* outputs;
        RwBool error;
        RwUInt32 doneNodes;
        RwInt32 i;

        if (pipeline->numNodes != 0) {
            totalOutputs = 0;
            error = FALSE;
            doneNodes = 0;

            if (pipeline->entryPoint >= pipeline->numNodes ||
                pipeline->nodes[pipeline->entryPoint].nodeDef == NULL) {
                RWERROR((E_RX_INVALIDENTRYPOINT));
                return (RxPipeline*)NULL;
            }

            numUniqueClusters = PipelineCalcNumUniqueClusters(pipeline);

            topSortBlockSize =
                RXPIPELINEGLOBAL(field_0x38) *
                    (sizeof(RxPipelineNode) + sizeof(RxPipelineNodeTopSortData)) +
                RXPIPELINEGLOBAL(field_0x38) * rxNODE_MAX_OUTPUTS * sizeof(RwUInt32);

            depChaseBlockSize = RXPIPELINEGLOBAL(field_0x38) * sizeof(RxPipelineNodeTopSortData);
            depChaseBlockSize += CalcNodesOutputsCompactedMemSize(pipeline);
            depChaseBlockSize =
                (pipeline->numNodes * sizeof(rxReq) +
                 pipeline->numNodes * numUniqueClusters * sizeof(RwReqEntry)) +
                (depChaseBlockSize + pipeline->numNodes * numUniqueClusters * sizeof(RwScopeTrace));
            depChaseBlockSize += CalcUnlockPersistentMemSize(pipeline, numUniqueClusters);

            unlockStartBlockSize = topSortBlockSize;
            if (depChaseBlockSize > unlockStartBlockSize) {
                unlockStartBlockSize = depChaseBlockSize;
            }

            if (unlockStartBlockSize > pipeline->superBlockSize) {
                if (!ReallocAndFixupSuperBlock(pipeline, unlockStartBlockSize)) {
                    return (RxPipeline*)NULL;
                }
            }

            lbl_8052B1B8.StalacTiteBase = (RwUInt8*)pipeline->superBlock + unlockStartBlockSize;
            lbl_8052B1B8.StalacMiteBase = NULL;

            pipeline = PipelineUnlockTopSort(pipeline);
            if (pipeline == NULL) {
                return (RxPipeline*)NULL;
            }

            outputs = (RwUInt32*)(pipeline->nodes + RXPIPELINEGLOBAL(field_0x38));
            topSortData =
                (RxPipelineNodeTopSortData*)(outputs + RXPIPELINEGLOBAL(field_0x38) * rxNODE_MAX_OUTPUTS);
            topSortData += pipeline->numNodes - 1;
            newTopSortData =
                (RxPipelineNodeTopSortData*)((RwUInt8*)pipeline->superBlock + unlockStartBlockSize);
            newTopSortData--;
            RWASSERT(newTopSortData > topSortData);
            for (i = (RwInt32)pipeline->numNodes - 1; i >= 0; i--) {
                memcpy(newTopSortData, topSortData, sizeof(RxPipelineNodeTopSortData));
                pipeline->nodes[i].topSortData = newTopSortData;
                topSortData--;
                newTopSortData--;
            }

            newOutputs = (RwUInt32*)(pipeline->nodes + pipeline->numNodes);
            for (i = 0; (RwUInt32)i < pipeline->numNodes; i++) {
                if (pipeline->nodes[i].numOutputs == 0) {
                    pipeline->nodes[i].outputs = NULL;
                } else {
                    memcpy(newOutputs, outputs, pipeline->nodes[i].numOutputs * sizeof(RwUInt32));
                    pipeline->nodes[i].outputs = newOutputs;
                }
                outputs += rxNODE_MAX_OUTPUTS;
                newOutputs += pipeline->nodes[i].numOutputs;
                totalOutputs += pipeline->nodes[i].numOutputs;
            }

            lbl_8052B1B8.StalacMiteBase = (RwUInt8*)(outputs + totalOutputs);
            lbl_8052B1B8.StalacTiteBase = (RwUInt8*)topSortData;

            if (_rxChaseDependencies(pipeline) != 0) {
                return (RxPipeline*)NULL;
            }

            unlockEndBlockSize = lbl_8052B1B8.StalacMiteBase - (RwUInt8*)pipeline->superBlock;
            if (!ReallocAndFixupSuperBlock(pipeline, unlockEndBlockSize)) {
                goto cleanup;
            }

            for (i = 0; (RwUInt32)i < pipeline->numNodes; i++) {
                pipeline->nodes[i].topSortData = NULL;
            }

            for (i = (RwInt32)pipeline->numNodes - 1; i >= 0; i--) {
                RxNodeDefinition* nodeDef = pipeline->nodes[i].nodeDef;
                RxPipelineNode* node = &pipeline->nodes[i];
                if (nodeDef->InputPipesCnt++ == 0 && nodeDef->nodeMethods.nodeInit != NULL) {
                    if (!nodeDef->nodeMethods.nodeInit(nodeDef)) {
                        error = TRUE;
                        doneNodes = (pipeline->numNodes - 1) - i;
                        break;
                    }
                }
                if (nodeDef->nodeMethods.pipelineNodeInit != NULL) {
                    if (!nodeDef->nodeMethods.pipelineNodeInit(node)) {
                        if (--nodeDef->InputPipesCnt == 0 && nodeDef->nodeMethods.nodeTerm != NULL) {
                            nodeDef->nodeMethods.nodeTerm(nodeDef);
                        }
                        error = TRUE;
                        doneNodes = (pipeline->numNodes - 1) - i;
                        break;
                    }
                }
            }

            if (!error) {
                for (i = (RwInt32)pipeline->numNodes - 1; i >= 0; i--) {
                    RxNodeDefinition* nodeDef = pipeline->nodes[i].nodeDef;
                    RxPipelineNode* node = &pipeline->nodes[i];
                    if (nodeDef->nodeMethods.pipelineNodeConfig != NULL) {
                        if (!nodeDef->nodeMethods.pipelineNodeConfig(node, pipeline)) {
                            doneNodes = pipeline->numNodes;
                            error = TRUE;
                            break;
                        }
                    }
                }
            }

            if (error) {
                for (i = (RwInt32)(pipeline->numNodes - doneNodes); (RwUInt32)i < pipeline->numNodes;
                     i++) {
                    RxNodeDefinition* nodeDef = pipeline->nodes[i].nodeDef;
                    RxPipelineNode* node = &pipeline->nodes[i];
                    if (nodeDef->nodeMethods.pipelineNodeTerm != NULL) {
                        nodeDef->nodeMethods.pipelineNodeTerm(node);
                    }
                    if (--nodeDef->InputPipesCnt == 0 && nodeDef->nodeMethods.nodeTerm != NULL) {
                        nodeDef->nodeMethods.nodeTerm(nodeDef);
                    }
                }
                goto cleanup;
            }
        }

        pipeline->locked = FALSE;
        return pipeline;
    }

    if (pipeline == NULL) {
        RWERROR((E_RW_NULLP));
    } else {
        RWERROR((E_RX_UNLOCKEDPIPE));
    }
    return (RxPipeline*)NULL;

cleanup:
    LockPipelineExpandData(pipeline, pipeline);
    return (RxPipeline*)NULL;
} // 0x8029C684

/*
 * Locks an unlocked pipeline for editing, terminating active nodes and expanding the superblock
 * into the fixed-stride locked layout.
 */
RxPipeline* RxPipelineLock(RxPipeline* pipeline) {
    if (!pipeline->locked) {
        RwUInt32 lockedBlockSize =
            RXPIPELINEGLOBAL(field_0x38) *
                (sizeof(RxPipelineNode) + sizeof(RxPipelineNodeTopSortData)) +
            RXPIPELINEGLOBAL(field_0x38) * rxNODE_MAX_OUTPUTS * sizeof(RwUInt32);

        if (pipeline->nodes != NULL) {
            if (lockedBlockSize > pipeline->superBlockSize) {
                if (!ReallocAndFixupSuperBlock(pipeline, lockedBlockSize)) {
                    return (RxPipeline*)NULL;
                }
            }
            if (!LockPipelineExpandData(pipeline, pipeline)) {
                return (RxPipeline*)NULL;
            }
        } else {
            pipeline->superBlock = RwMalloc(
                lockedBlockSize,
                rwMEMHINTDUR_EVENT | rwMEMHINTFLAG_RESIZABLE | rwID_PIPEMODULE);
            if (pipeline->superBlock == NULL) {
                RWERROR((E_RW_NOMEM, lockedBlockSize));
                return (RxPipeline*)NULL;
            }
            pipeline->superBlockSize = lockedBlockSize;
            pipeline->nodes = (RxPipelineNode*)pipeline->superBlock;
        }

        pipeline->locked = TRUE;
        if (pipeline->nodes != NULL) {
            RwUInt32 n;
            for (n = 0; n < pipeline->numNodes; n++) {
                RxNodeMethods* nodeMethods = &pipeline->nodes[n].nodeDef->nodeMethods;
                if (nodeMethods->pipelineNodeTerm != NULL) {
                    nodeMethods->pipelineNodeTerm(&pipeline->nodes[n]);
                }
                if (--pipeline->nodes[n].nodeDef->InputPipesCnt == 0) {
                    if (nodeMethods->nodeTerm != NULL) {
                        nodeMethods->nodeTerm(pipeline->nodes[n].nodeDef);
                    }
                }
                pipeline->nodes[n].slotClusterRefs = NULL;
            }
        }
    }

    return pipeline;
} // 0x8029CC64

/*
 * Finds the first pipeline node whose nodeDef->name matches name (after start, when given).
 * Writes its index to *nodeIndex if non-NULL (or -1 if not found).
 */
RxPipelineNode* RxPipelineFindNodeByName(RxPipeline* pipeline, const RwChar* name,
                                         RxPipelineNode* start, RwInt32* nodeIndex) {
    RwBool check;

    check = FALSE;
    if (pipeline != NULL && name != NULL && pipeline->numNodes != 0) {
        check = TRUE;
    }

    if (check) {
        RxPipelineNode* node = pipeline->nodes;
        RwInt32 n = pipeline->numNodes;

        if (start != NULL) {
            while (node != start && n > 0) {
                node++;
                n--;
            }
            node++;
            n--;
        }

        while (n > 0) {
            if (node->nodeDef != NULL) {
                if (rwstrcmp(node->nodeDef->name, name) == 0) {
                    if (nodeIndex != NULL) {
                        *nodeIndex = pipeline->numNodes - n;
                    }
                    return node;
                }
            }
            node++;
            n--;
        }
    }

    if (nodeIndex != NULL) {
        *nodeIndex = -1;
    }
    return (RxPipelineNode*)NULL;
} // 0x8029CDF8

/*
 * Appends a NULL-terminated varargs sequence of RxNodeDefinition pointers as a connected fragment
 * of nodes to a locked pipeline.
 */
RxPipeline* RxLockedPipeAddFragment(RxPipeline* pipeline, RwUInt32* firstIndex,
                                    RxNodeDefinition* nodeDef0, ...) {
    if (pipeline != NULL && pipeline->locked) {
        va_list va;
        RxNodeDefinition* nodeDef;
        RwUInt32 fragLength = 0;

        va_start(va, nodeDef0);
        nodeDef = nodeDef0;
        while (nodeDef != NULL) {
            fragLength++;
            nodeDef = va_arg(va, RxNodeDefinition*);
        }
        va_end(va);

        if (fragLength != 0) {
            RwUInt32 oldnumnodes;
            RwUInt32 n;
            RxPipelineNode* prevnode = NULL;

            if (pipeline->numNodes + fragLength > RXPIPELINEGLOBAL(field_0x38)) {
                RWERROR((E_RX_PIPELINETOOMANYNODES));
                return (RxPipeline*)NULL;
            }

            oldnumnodes = pipeline->numNodes;
            n = 0;
            va_start(va, nodeDef0);
            nodeDef = nodeDef0;
            while (nodeDef != NULL) {
                RxPipelineNode* node = &pipeline->nodes[oldnumnodes + n];
                if (!fn_8029BEF0(pipeline, node, nodeDef)) {
                    break;
                }
                if (prevnode != NULL) {
                    if (!RxLockedPipeAddPath(pipeline, RxPipelineNodeFindOutputByIndex(prevnode, 0),
                                             RxPipelineNodeFindInput(node))) {
                        fn_8029B470(node, pipeline);
                        break;
                    }
                }
                prevnode = node;
                n++;
                nodeDef = va_arg(va, RxNodeDefinition*);
            }
            va_end(va);

            if (n == fragLength) {
                if (firstIndex != NULL) {
                    *firstIndex = oldnumnodes;
                }
                return pipeline;
            }

            while (n--) {
                fn_8029B470(&pipeline->nodes[n + oldnumnodes], pipeline);
            }
        }
    } else {
        if (pipeline == NULL) {
            RWERROR((E_RW_NULLP));
        } else {
            RWERROR((E_RX_UNLOCKEDPIPE));
        }
    }
    return (RxPipeline*)NULL;
} // 0x8029CEF8
