// Unit name and boundary: Gormiti link map (RwCore.a p2dep.obj)
// Function names, locals and structs: Gormiti DWARF (p2dep.c).
/*
 * p2dep.c: pipeline cluster dependency chasing, scope tracing, slot assignment, and embedded
 * packet / per-node cluster allocation.
 */
#include <string.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/pipe/p2/p2define.h>
#include <rwsdk/pipe/p2/p2resort.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>

#define ReqGetEntry(req, index) (((index) < (req)->numused) ? &(req)->reqentries[(index)] : NULL)

/*
 * Allocates an rxReq container and its RwReqEntry array from the scratch stack.
 */
static rxReq* _ReqCreate(RxPipelineNode* node, RwUInt32 numClusters) {
    rxReq* req;

    req = (rxReq*)StalacTiteAlloc(sizeof(rxReq));
    if (req != NULL) {
        req->reqentries = (RwReqEntry*)StalacTiteAlloc(numClusters * sizeof(RwReqEntry));
        if (req->reqentries != NULL) {
            req->numalloced = numClusters;
            req->numused = 0;
            req->node = node;
            req->unavailslots = 0;
            return req;
        }
    }
    return NULL;
} // no copy in retail (inlined)

/*
 * Finds an existing requirement entry for clusterDef in req, or NULL if absent.
 */
static RwReqEntry* _ReqSearch4Cluster(rxReq* req, RxClusterDefinition* clusterDef) {
    if (req->numused > 0) {
        RwReqEntry* reqentry = req->reqentries;
        RwUInt32 n;
        for (n = 0; n < req->numused; n++, reqentry++) {
            if (reqentry->clusterDef == clusterDef) {
                return reqentry;
            }
        }
    }
    return NULL;
} // no copy in retail (inlined)

/*
 * Adds or upgrades a cluster requirement entry in req.
 */
RwReqEntry* _ReqAddEntry(rxReq* req, RxClusterDefinition* clusterDef,
                         RxClusterValidityReq required, RwUInt32 inputs,
                         RxPipelineNode* originatingNode) {
    RwReqEntry* reqentry;

    reqentry = _ReqSearch4Cluster(req, clusterDef);
    if (reqentry != NULL) {
        if (required == rxCLREQ_REQUIRED) {
            reqentry->required = rxCLREQ_REQUIRED;
            reqentry->originatingNode = originatingNode;
        }
        return reqentry;
    }

    reqentry = &req->reqentries[req->numused];
    req->numused++;
    reqentry->clusterDef = clusterDef;
    reqentry->required = required;
    reqentry->inputs = inputs;
    reqentry->next = NULL;
    reqentry->req = req;
    reqentry->st = NULL;
    reqentry->outbf = 0;
    reqentry->assignedslot = (RwUInt32)-1;
    reqentry->originatingNode = originatingNode;
    return reqentry;
} // 0x8029D220

/*
 * Removes reqentry from req by swapping the last entry into its slot.
 */
static void _ReqDeleteEntry(rxReq* req, RwReqEntry* reqentry) {
    if (reqentry != &req->reqentries[req->numused - 1]) {
        *reqentry = req->reqentries[req->numused - 1];
    }
    req->numused--;
} // no copy in retail (inlined)

/*
 * Returns the index of clusterDef in iospec->clustersOfInterest, or -1 if not listed.
 */
static RwUInt32 _IoSpecSearch4Cluster(RxIoSpec* iospec, RxClusterDefinition* clusterDef) {
    RwUInt32 i;

    for (i = 0; i < iospec->numClustersOfInterest; i++) {
        if (iospec->clustersOfInterest[i].clusterDef == clusterDef) {
            return i;
        }
    }
    return (RwUInt32)-1;
} // no copy in retail (inlined)

/*
 * Propagates elimination of a dead cluster path downstream from node.
 */
static void _PropDownElimPath(RxPipeline* pipeline, RxPipelineNode* node,
                              RxClusterDefinition* clusterDef) {
    RwUInt32 i;
    RxIoSpec* iospec;
    RwReqEntry* reqentry;

    iospec = &node->nodeDef->io;
    reqentry = _ReqSearch4Cluster(node->topSortData->req, clusterDef);
    if (reqentry != NULL) {
        if (--reqentry->inputs == 0) {
            _ReqDeleteEntry(node->topSortData->req, reqentry);
            for (i = 0; i < node->numOutputs; i++) {
                if (node->outputs[i] != (RwUInt32)-1) {
                    RxOutputSpec* outspec = &node->nodeDef->io.outputs[i];
                    RwUInt32 n = _IoSpecSearch4Cluster(iospec, clusterDef);
                    RxClusterValid presinout =
                        (n == (RwUInt32)-1) ? outspec->allOtherClusters : outspec->outputClusters[n];

                    if (presinout == rxCLVALID_NOCHANGE) {
                        _PropDownElimPath(pipeline, &pipeline->nodes[node->outputs[i]], clusterDef);
                    }
                }
            }
        }
    }
} // 0x8029D2C8

/*
 * Allocates a new scope trace node and prepends it to *headref.
 */
static RwScopeTrace* _ScopeTraceCreate(RwScopeTrace** headref) {
    RwScopeTrace* scopetrace;

    scopetrace = (RwScopeTrace*)StalacTiteAlloc(sizeof(RwScopeTrace));
    if (scopetrace != NULL) {
        scopetrace->continuation = NULL;
        scopetrace->head = NULL;
        scopetrace->parent = NULL;
        scopetrace->next = *headref;
        *headref = scopetrace;
        return scopetrace;
    }
    return NULL;
} // no copy in retail (inlined)

/*
 * Prepends reqentry to scopetrace's entry list.
 */
static void _ScopeTraceAddEntry(RwScopeTrace* scopetrace, RwReqEntry* reqentry) {
    reqentry->next = scopetrace->head;
    scopetrace->head = reqentry;
} // no copy in retail (inlined)

/*
 * Merges the scope trace trees containing p and q and unlinks the merged root from *headref.
 */
void _ScopeTraceMerge(RwScopeTrace** headref, RwScopeTrace* p, RwScopeTrace* q) {
    RwScopeTrace* proot;
    RwScopeTrace* qroot;

    proot = p;
    while (proot->parent != NULL) {
        proot = proot->parent;
    }
    qroot = q;
    while (qroot->parent != NULL) {
        qroot = qroot->parent;
    }
    if (proot != qroot) {
        RwScopeTrace* prootfinalpage = proot;
        while (prootfinalpage->continuation != NULL) {
            prootfinalpage = prootfinalpage->continuation;
        }
        prootfinalpage->continuation = qroot;
        qroot->parent = proot;
        while (*headref != qroot) {
            headref = &(*headref)->next;
        }
        *headref = (*headref)->next;
    }
} // 0x8029D490

/*
 * Propagates cluster requirements backward across the topologically sorted pipeline and
 * eliminates dead paths. (Gormiti DWARF: _PropagateDependenciesAndKillDeadPaths; unpaired: Gormiti's
 * revision hoists the inner duplicate test's bound with the outer one, retail reloads it per j.)
 * Wall: register/schedule, 14 rows. In the first loop retail colours j before the hoisted loop bound, the
 * inner bound and the inner offset (j = r5, as in Gormiti); the build colours it after them (j = r8). Ruled out:
 * either loop bound written through iospec or through node->nodeDef->io (the Debug build uses the latter for
 * both), and iospec (with _IoSpecSearch4Cluster's parameter) as a pointer to const.
 */
static RwUInt32 fn_8029D500(RxPipeline* pipeline) {
    RwUInt32 numUniqueClusters;
    RwUInt32 i;
    RwUInt32 j;
    RwUInt32 k;
    RxPipelineNode* node;

    i = pipeline->numNodes;
    node = &pipeline->nodes[i - 1];
    do {
        RxIoSpec* iospec = &node->nodeDef->io;

        for (j = 0; j < iospec->numClustersOfInterest; j++) {
            RxClusterDefinition* cluster = iospec->clustersOfInterest[j].clusterDef;
            if (cluster == NULL) {
                RWERROR((E_RX_DEP_NULLCLUSTERDEF, node->nodeDef->name));
                return E_RX_DEP_NULLCLUSTERDEF;
            }
            for (k = j + 1; k < node->nodeDef->io.numClustersOfInterest; k++) {
                RxClusterDefinition* cluster2 = iospec->clustersOfInterest[k].clusterDef;
                if (cluster == cluster2) {
                    RWERROR((E_RX_DEP_DUPLICATECLUSTERDEFS, node->nodeDef->name, cluster2->name));
                    return E_RX_DEP_DUPLICATECLUSTERDEFS;
                }
            }
        }

        numUniqueClusters = PipelineCalcNumUniqueClusters(pipeline);
        if ((node->topSortData->req = _ReqCreate(node, numUniqueClusters)) == NULL) {
            RWERROR((E_RX_DEP_OUTOFMEMORY));
            return E_RX_DEP_OUTOFMEMORY;
        }

        for (j = 0; j < node->numOutputs; j++) {
            if (node->outputs[j] != (RwUInt32)-1) {
                RxPipelineNode* outnode = &pipeline->nodes[node->outputs[j]];
                RxOutputSpec* outspec = &node->nodeDef->io.outputs[j];

                for (k = 0; k < outnode->topSortData->req->numused; k++) {
                    RwReqEntry* reqentry = ReqGetEntry(outnode->topSortData->req, k);
                    RwUInt32 n = _IoSpecSearch4Cluster(iospec, reqentry->clusterDef);
                    RxClusterValid presinout =
                        (n == (RwUInt32)-1) ? outspec->allOtherClusters : outspec->outputClusters[n];

                    if (presinout == rxCLVALID_NOCHANGE) {
                        if (_ReqAddEntry(node->topSortData->req, reqentry->clusterDef,
                                         reqentry->required, node->topSortData->numIns,
                                         reqentry->originatingNode) == NULL) {
                            RWERROR((E_RX_DEP_OUTOFMEMORY));
                            return E_RX_DEP_OUTOFMEMORY;
                        }
                    } else if (reqentry->required == rxCLREQ_REQUIRED) {
                        if (presinout != rxCLVALID_VALID) {
                            RWERROR((E_RX_DEP_DEPENDENCIESMISMATCH, reqentry->clusterDef->name,
                                     reqentry->originatingNode->nodeDef->name, node->nodeDef->name,
                                     j, outspec->name));
                            return E_RX_DEP_DEPENDENCIESMISMATCH;
                        }
                    } else if (presinout == rxCLVALID_INVALID) {
                        _PropDownElimPath(pipeline, outnode, reqentry->clusterDef);
                    }
                }
            }
        }

        for (j = 0; j < iospec->numClustersOfInterest; j++) {
            if (iospec->inputRequirements[j] != rxCLREQ_DONTWANT) {
                if (_ReqAddEntry(node->topSortData->req, iospec->clustersOfInterest[j].clusterDef,
                                 (RxClusterValidityReq)iospec->inputRequirements[j],
                                 node->topSortData->numIns, node) == NULL) {
                    RWERROR((E_RX_DEP_OUTOFMEMORY));
                    return E_RX_DEP_OUTOFMEMORY;
                }
            }
        }

        node--;
    } while (--i);

    return 0;
} // 0x8029D500

/*
 * Adds forced-present output clusters and builds per-requirement output continuity bitfields.
 */
RwUInt32 _ForAllNodeReqsAddOutputClustersAndBuildContinuityBitfields(RxPipeline* pipeline) {
    RwUInt32 i;
    RwUInt32 j;
    RwUInt32 k;
    RxPipelineNode* node;

    i = pipeline->numNodes;
    node = pipeline->nodes;
    do {
        RxIoSpec* iospec = &node->nodeDef->io;

        for (j = 0; j < iospec->numClustersOfInterest; j++) {
            if (iospec->clustersOfInterest[j].forcePresent != rxCLALLOWABSENT) {
                if (_ReqAddEntry(node->topSortData->req, iospec->clustersOfInterest[j].clusterDef,
                                 rxCLREQ_DONTWANT, 1, node) == NULL) {
                    RWERROR((E_RX_DEP_OUTOFMEMORY));
                    return E_RX_DEP_OUTOFMEMORY;
                }
            }
        }

        for (j = 0; j < node->numOutputs; j++) {
            if (node->outputs[j] != (RwUInt32)-1) {
                RxPipelineNode* outnode = &pipeline->nodes[node->outputs[j]];
                RxOutputSpec* outspec = &node->nodeDef->io.outputs[j];

                for (k = 0; k < outnode->topSortData->req->numused; k++) {
                    RwReqEntry* reqentry = ReqGetEntry(outnode->topSortData->req, k);
                    RwUInt32 n = _IoSpecSearch4Cluster(iospec, reqentry->clusterDef);
                    RxClusterValid presinout =
                        (n == (RwUInt32)-1) ? outspec->allOtherClusters : outspec->outputClusters[n];

                    if (presinout != rxCLVALID_INVALID) {
                        RwReqEntry* drivingreqe;

                        if (presinout != rxCLVALID_NOCHANGE) {
                            drivingreqe = _ReqAddEntry(node->topSortData->req, reqentry->clusterDef,
                                                       rxCLREQ_DONTWANT, 1, node);
                            if (drivingreqe == NULL) {
                                RWERROR((E_RX_DEP_OUTOFMEMORY));
                                return E_RX_DEP_OUTOFMEMORY;
                            }
                        } else {
                            drivingreqe = _ReqSearch4Cluster(node->topSortData->req, reqentry->clusterDef);
                        }

                        if (drivingreqe != NULL) {
                            drivingreqe->outbf |= (1U << j);
                        }
                    }
                }
            }
        }

        node++;
    } while (--i);

    return 0;
} // 0x8029D8E8

/*
 * Traces cluster lifetimes across pipeline nodes into scope traces.
 */
RwUInt32 _TraceClusterScopes(RxPipeline* pipeline, RwScopeTrace** stheadref) {
    RwUInt32 i;
    RwUInt32 j;
    RwUInt32 k;
    RxPipelineNode* node;

    i = pipeline->numNodes;
    node = pipeline->nodes;
    do {
        for (k = 0; k < node->topSortData->req->numused; k++) {
            RwReqEntry* reqentry = ReqGetEntry(node->topSortData->req, k);
            if (reqentry->st == NULL) {
                if ((reqentry->st = _ScopeTraceCreate(stheadref)) == NULL) {
                    RWERROR((E_RX_DEP_OUTOFMEMORY));
                    return E_RX_DEP_OUTOFMEMORY;
                }
                _ScopeTraceAddEntry(reqentry->st, reqentry);
            }
        }

        for (j = 0; j < node->numOutputs; j++) {
            if (node->outputs[j] != (RwUInt32)-1) {
                RxPipelineNode* outnode = &pipeline->nodes[node->outputs[j]];

                for (k = 0; k < outnode->topSortData->req->numused; k++) {
                    RwReqEntry* reqentry = ReqGetEntry(outnode->topSortData->req, k);
                    if (reqentry->required != rxCLREQ_DONTWANT) {
                        RwReqEntry* drivingreqe =
                            _ReqSearch4Cluster(node->topSortData->req, reqentry->clusterDef);
                        if (drivingreqe != NULL && (drivingreqe->outbf & (1U << j))) {
                            if (reqentry->st == NULL) {
                                _ScopeTraceAddEntry(drivingreqe->st, reqentry);
                                reqentry->st = drivingreqe->st;
                            } else {
                                _ScopeTraceMerge(stheadref, reqentry->st, drivingreqe->st);
                            }
                        }
                    }
                }
            }
        }

        node++;
    } while (--i);

    return 0;
} // 0x8029DB54

/*
 * Assigns packet cluster slot indices to each scope trace without overlapping live ranges.
 */
RwUInt32 _AssignClusterSlots(RxPipeline* pipeline, RwScopeTrace** stheadref) {
    RwUInt32 numslotsused;
    RwScopeTrace* st;

    numslotsused = 0;
    st = *stheadref;
    while (st != NULL) {
        RwScopeTrace* st2 = st;
        RwUInt32 unavailslots = 0;
        RwUInt32 assignedslot;

        do {
            RwReqEntry* reqentry = st2->head;
            while (reqentry != NULL) {
                unavailslots |= reqentry->req->unavailslots;
                reqentry = reqentry->next;
            }
            st2 = st2->continuation;
        } while (st2 != NULL);

        assignedslot = 0;
        while (unavailslots & 1) {
            unavailslots >>= 1;
            assignedslot++;
        }
        if (assignedslot >= numslotsused) {
            numslotsused = assignedslot + 1;
        }

        st2 = st;
        do {
            RwReqEntry* reqentry = st2->head;
            while (reqentry != NULL) {
                reqentry->assignedslot = assignedslot;
                reqentry->req->unavailslots |= (1U << assignedslot);
                reqentry = reqentry->next;
            }
            st2 = st2->continuation;
        } while (st2 != NULL);

        st = st->next;
    }

    pipeline->packetNumClusterSlots = numslotsused;
    return 0;
} // 0x8029DD7C

/*
 * Enumerates unique clusters across scope traces, optionally invoking fp on each.
 */
static RwUInt32 _EnumPipelineClusters(RwScopeTrace* head,
                                      void (*fp)(RxClusterDefinition*, RwUInt32, void*),
                                      void* callbackprivatedata) {
    RwUInt32 numpipelineclusters = 0;
    RwScopeTrace* st = head;

    while (st != NULL) {
        RwScopeTrace* stdedupe = head;
        while (stdedupe != st) {
            if (stdedupe->head->clusterDef == st->head->clusterDef) {
                break;
            }
            stdedupe = stdedupe->next;
        }
        if (stdedupe == st) {
            if (fp != NULL) {
                fp(st->head->clusterDef, numpipelineclusters, callbackprivatedata);
            }
            numpipelineclusters++;
        }
        st = st->next;
    }
    return numpipelineclusters;
} // no copy in retail (inlined)

/*
 * Counts required and optional cluster inputs on the pipeline's entry node.
 */
static RwUInt32 _CountHeadNodeRqdsAndOpts(RxPipeline* pipeline) {
    RxPipelineNode* node = pipeline->nodes;
    RwUInt32 i;
    RwUInt32 count = 0;

    for (i = 0; i < node->topSortData->req->numused; i++) {
        RwReqEntry* reqentry = ReqGetEntry(node->topSortData->req, i);
        if (reqentry->required == rxCLREQ_REQUIRED || reqentry->required == rxCLREQ_OPTIONAL) {
            count++;
        }
    }
    return count;
} // no copy in retail (inlined)

/*
 * Writes the entry node's required and optional input clusters to pipeline->inputRequirements
 * and sorts them by clusterDef pointer. (Gormiti DWARF: _WriteHeadNodeRqdsAndOpts2PipelineRequirements.)
 */
static void fn_8029DE38(RxPipeline* pipeline) {
    RxPipelineNode* node = pipeline->nodes;
    RwUInt32 i;
    RwUInt32 count = 0;

    for (i = 0; i < node->topSortData->req->numused; i++) {
        RwReqEntry* reqentry = ReqGetEntry(node->topSortData->req, i);
        if (reqentry->required == rxCLREQ_REQUIRED || reqentry->required == rxCLREQ_OPTIONAL) {
            RxPipelineRequiresCluster* reqcl = &pipeline->inputRequirements[count++];
            reqcl->clusterDef = reqentry->clusterDef;
            reqcl->rqdOrOpt = reqentry->required;
            reqcl->slotIndex = reqentry->assignedslot;
        }
    }
    pipeline->numInputRequirements = count;
    _rx_rxRadixExchangeSort(pipeline->inputRequirements, count,
                            sizeof(RxPipelineRequiresCluster), 0, 0, (RwUInt32)-1);
} // 0x8029DE38

/*
 * Callback that allocates and initializes one RxPipelineCluster per unique cluster.
 */
void _MyEnumPipelineClustersCallBack(RxClusterDefinition* clusterdef, RwUInt32 index, void* data) {
    RxPipelineCluster** array = (RxPipelineCluster**)data;
    RxPipelineCluster* pipelineCluster;

    (void)index;
    pipelineCluster = (RxPipelineCluster*)StalacMiteAlloc(sizeof(RxPipelineCluster));
    if (*array == NULL) {
        *array = pipelineCluster;
    }
    pipelineCluster->clusterRef = clusterdef;
    pipelineCluster->creationAttributes = clusterdef->defaultAttributes;
} // 0x8029DED8

/*
 * Allocates the embedded packet, pipeline clusters, and per-node slot mappings from the
 * StalacMite scratch region. (Gormiti DWARF: _ForAllNodesWriteClusterAllocations.)
 */
static RwUInt32 fn_8029DF30(RxPipeline* pipeline, RwScopeTrace* stHead) {
    RwUInt32 numPipelineRequiresClusters;
    RwUInt32 numPipelineClusters;
    RxPipelineCluster* pipelineClusters;
    RwUInt8* titeBase;
    RwUInt8* miteBase;
    RwUInt32 slotsContinueAND;
    RwUInt32 i;
    RwUInt32 j;
    RwUInt32 k;
    RxPipelineNode* node;

    numPipelineRequiresClusters = _CountHeadNodeRqdsAndOpts(pipeline);
    numPipelineClusters = _EnumPipelineClusters(stHead, NULL, NULL);

    titeBase = (RwUInt8*)StalacTiteAlloc(0);
    miteBase = (RwUInt8*)StalacMiteAlloc(0);
    memset(miteBase, 0, (RwUInt32)(titeBase - miteBase));

    pipeline->embeddedPacket = (RxPacket*)StalacMiteAlloc(
        sizeof(RxPacket) + (pipeline->packetNumClusterSlots - 1) * sizeof(RxCluster));
    pipeline->embeddedPacket->numClusters = (RwUInt16)pipeline->packetNumClusterSlots;
    pipeline->embeddedPacket->pipeline = pipeline;
    pipeline->embeddedPacketState = rxPKST_PACKETLESS;

    pipelineClusters = NULL;
    _EnumPipelineClusters(stHead, _MyEnumPipelineClustersCallBack, &pipelineClusters);

    for (i = 0; i < pipeline->numNodes; i++) {
        node = &pipeline->nodes[i];
        if (pipeline->packetNumClusterSlots != 0) {
            node->slotClusterRefs = (RxPipelineCluster**)StalacMiteAlloc(
                pipeline->packetNumClusterSlots * sizeof(RxPipelineCluster*));
        }
        node->slotsContinue =
            (RwUInt32*)StalacMiteAlloc((pipeline->packetNumClusterSlots + 1) * sizeof(RwUInt32));
        if (node->nodeDef->io.numClustersOfInterest != 0) {
            node->inputToClusterSlot =
                (RwUInt32*)StalacMiteAlloc(node->nodeDef->io.numClustersOfInterest * sizeof(RwUInt32));
        }
        if (node->nodeDef->pipelineNodePrivateDataSize != 0) {
            node->privateData =
                StalacMiteAlloc((node->nodeDef->pipelineNodePrivateDataSize + 3) & ~3U);
        }

        slotsContinueAND = ~0U;
        for (j = 0; j < node->topSortData->req->numused; j++) {
            RxPipelineCluster* pipelineCluster = NULL;
            RwReqEntry* reqEntry = ReqGetEntry(node->topSortData->req, j);

            for (k = 0; k < numPipelineClusters; k++) {
                if (pipelineClusters[k].clusterRef == reqEntry->clusterDef) {
                    pipelineCluster = &pipelineClusters[k];
                    break;
                }
            }
            node->slotClusterRefs[reqEntry->assignedslot] = pipelineCluster;
            node->slotsContinue[reqEntry->assignedslot + 1] = reqEntry->outbf;
            slotsContinueAND &= reqEntry->outbf;
        }
        node->slotsContinue[0] = slotsContinueAND;

        for (j = 0; j < node->nodeDef->io.numClustersOfInterest; j++) {
            node->inputToClusterSlot[j] = (RwUInt32)-1;
            for (k = 0; k < pipeline->packetNumClusterSlots; k++) {
                if (node->slotClusterRefs[k] != NULL &&
                    node->slotClusterRefs[k]->clusterRef ==
                        node->nodeDef->io.clustersOfInterest[j].clusterDef) {
                    node->inputToClusterSlot[j] = k;
                    break;
                }
            }
        }
    }

    if (numPipelineRequiresClusters != 0) {
        pipeline->inputRequirements = (RxPipelineRequiresCluster*)StalacMiteAlloc(
            numPipelineRequiresClusters * sizeof(RxPipelineRequiresCluster));
    }
    fn_8029DE38(pipeline);

    return 0;
} // 0x8029DF30

/*
 * Runs cluster dependency propagation, scope tracing, slot assignment, and allocation setup
 * on a locked pipeline.
 */
RwUInt32 _rxChaseDependencies(RxPipeline* pipeline) {
    RwScopeTrace* sthead = NULL;
    RwUInt32 result;

    result = fn_8029D500(pipeline);
    if (result == 0) {
        result = _ForAllNodeReqsAddOutputClustersAndBuildContinuityBitfields(pipeline);
        if (result == 0) {
            result = _TraceClusterScopes(pipeline, &sthead);
            if (result == 0) {
                result = _AssignClusterSlots(pipeline, &sthead);
                if (result == 0) {
                    result = fn_8029DF30(pipeline, sthead);
                }
            }
        }
    }
    return result;
} // 0x8029E2E8
