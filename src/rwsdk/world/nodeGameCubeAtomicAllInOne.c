// Unit name and boundary: Gormiti link map (RPworld.a nodeGameCubeAtomicAllInOne.obj)
// Names, parameters and locals: Gormiti DWARF (nodeGameCubeAtomicAllInOne.c)
/*
 * nodeGameCubeAtomicAllInOne.c: the atomic pipeline of the GameCube. One node does what the whole
 * atomic pipeline needs: it instances an atomic's geometry into a display list (or brings the
 * display list it has up to date), finds the lights that fall on the atomic and draws it, through
 * four stages the game may replace.
 */
#include <math.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/gx/GXManage.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/baframe.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/pipe/p2/p2define.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/gclights.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/geominst.h>
#include <rwsdk/world/instancegeom.h>
#include <rwsdk/world/nodeGameCubeAtomicAllInOne.h>
#include <rwsdk/world/vtxfmt.h>

static RwBool _rxGCAtomicAllInOneNode(RxPipelineNode* self, const RxPipelineNodeParam* params);
static RwBool _rxGCAtomicAllInOnePipelineInit(RxPipelineNode* node);
static void _rxGCDefaultReinstance(RpGeometry* geometry, RxGameCubeVertexBuffer* vbHeader, void* nodeData);

/*
 * Waits until the GX pipe has drawn from a vertex buffer, asking for a draw sync if the buffer
 * is the last one drawn (the wait of _rxGCResEntryWaitDone, which the compiler expanded here).
 */
#define rxGCVertexBufferWaitDone(_vbHeader)                                         \
    MACRO_START {                                                                   \
        if ((_vbHeader)->token == _RwDlTokenCurrent) {                              \
            GXSetDrawSync(_RwDlTokenCurrent);                                       \
            _RwDlTokenCurrent = (_RwDlTokenCurrent + 1) % rwDLTOKENMODULUS;         \
        }                                                                           \
                                                                                    \
        while (!_rwDlTokenQueryDone((_vbHeader)->token)) {                          \
        }                                                                           \
    } MACRO_STOP

/* The definition of the atomic all-in-one node. */
RxNodeDefinition* RxNodeDefinitionGetGameCubeAtomicAllInOne(void) {
    static RxNodeDefinition nodeGameCubeAtomicAllInOneCSL = {
        "GameCubeAtomicAllInOne.csl",
        {
            _rxGCAtomicAllInOneNode,
            NULL,
            NULL,
            _rxGCAtomicAllInOnePipelineInit,
            NULL,
            NULL,
            NULL,
        },
        {
            0,
            NULL,
            NULL,
            0,
            NULL,
        },
        sizeof(_rxGameCubeAllInOneNodeData),
        rxNODEDEFCONST,
        0,
    };

    return &nodeGameCubeAtomicAllInOneCSL;
} // no copy in retail (dead-stripped or inlined)

/*
 * Builds an atomic pipeline of one all-in-one node and sets its four stages. The pipeline's
 * plugin id and data are the id and type of the pipeline the caller makes.
 */
RxPipeline* _rpDlAtomicPipelineCreate(unsigned int id, unsigned int type, RxGameCubePipeCallback instance,
                                      RxGameCubePipeCallback reinstance, RxGameCubePipeCallback lighting,
                                      RxGameCubePipeCallback render) {
    RxPipeline* pipe;
    RxPipeline* lpipe;
    RxNodeDefinition* nodeDfn;
    RxPipelineNode* node;

    pipe = RxPipelineCreate();
    pipe->pluginId = id;
    pipe->pluginData = type;

    nodeDfn = RxNodeDefinitionGetGameCubeAtomicAllInOne();

    lpipe = RxPipelineLock(pipe);
    lpipe = RxLockedPipeAddFragment(lpipe, NULL, nodeDfn, NULL);
    RxLockedPipeUnlock(lpipe);

    node = RxPipelineFindNodeByName(pipe, nodeDfn->name, NULL, NULL);
    _rxGameCubeAllInOneSetInstanceCallBack(node, instance);
    _rxGameCubeAllInOneSetReinstanceCallBack(node, reinstance);
    _rxGameCubeAllInOneSetLightingCallBack(node, lighting);
    RxGameCubeAllInOneSetRenderCallBack(node, render);

    return pipe;
} // 0x802DD248

/*
 * The atomic's default lighting stage: asks for the world's own lights when the atomic is lit,
 * adds up the lights of the sectors the atomic lies in that reach its bounding sphere (each light
 * once a frame), and clamps the ambient light to white.
 *
 * Not exact (99.01%): register/schedule wall only; control flow, calls and constants match. Retail keeps
 * the loop's end pointer in r31 above pipeData and object, and the queued light shares the register of
 * matrixLight; this body gets pipeData r31, object r30. Ruled out: declaration order of cur/end, end
 * inlined into the loop test, end/cur at function scope, light declared in the block or in the list.
 */
void* _rxGCAtomicDefaultLightingCallback(void* object, RxGameCubePipeData* pipeData) {
    RpAtomic* atomic = (RpAtomic*)object;
    int flags = atomic->geometry->flags;
    RwLLLink* end;
    RwLLLink* cur;

    pipeData->lightMask = 0;
    pipeData->ambientLightColor.red = 0.0f;
    pipeData->ambientLightColor.green = 0.0f;
    pipeData->ambientLightColor.blue = 0.0f;
    pipeData->ambientLightColor.alpha = 1.0f;
    pipeData->ambientLight = 0;
    pipeData->numActiveLights = 0;
    pipeData->numConsideredLights = 0;

    if ((flags & rpGEOMETRYLIGHT) && RWSRCGLOBAL(curWorld)) {

        _rwGCLightsGlobalEnable(atomic->pad, pipeData);

        RWSRCGLOBAL(lightFrame)++;

        cur = rwLinkListGetFirstLLLink(&atomic->llWorldSectorsInAtomic);
        end = rwLinkListGetTerminator(&atomic->llWorldSectorsInAtomic);

        _rwGCLightsListInit();

        while (cur != end) {
            RpTie* tpTie = rwLLLinkGetData(cur, RpTie, lWorldSectorInAtomic);
            RwLLLink* curLight = rwLinkListGetFirstLLLink(&tpTie->worldSector->lightsInWorldSector);
            RwLLLink* endLight = rwLinkListGetTerminator(&tpTie->worldSector->lightsInWorldSector);

            while (curLight != endLight) {
                RpLightTie* lightTie = rwLLLinkGetData(curLight, RpLightTie, lightInWorldSector);
                RpLight* theLight = lightTie->light;

                if (theLight && theLight->lightFrame != RWSRCGLOBAL(lightFrame) &&
                    (RpLightGetFlags(theLight) & (RwUInt8)atomic->pad)) {
                    RpLight* light;
                    RwMatrix* matrixLight;
                    RwV3d* pos;
                    const RwSphere* sphere;
                    RwV3d distanceVector;
                    float distanceSquare;
                    float distanceCollision;

                    theLight->lightFrame = RWSRCGLOBAL(lightFrame);

                    matrixLight = RwFrameGetLTM(RpLightGetFrame(theLight));
                    pos = &matrixLight->pos;
                    sphere = RpAtomicGetWorldBoundingSphere(atomic);

                    distanceVector.x = sphere->center.x - pos->x;
                    distanceVector.y = sphere->center.y - pos->y;
                    distanceVector.z = sphere->center.z - pos->z;
                    distanceSquare = distanceVector.x * distanceVector.x + distanceVector.y * distanceVector.y +
                                     distanceVector.z * distanceVector.z;
                    distanceCollision = sphere->radius + theLight->radius;

                    if (distanceSquare < distanceCollision * distanceCollision) {
                        float inten;
                        float distance;

                        light = lightTie->light;
                        inten = rwGCLIGHTINTENSITYSHARE * (light->color.red + light->color.green + light->color.blue);
                        distance = sqrt(distanceSquare);

                        inten *= 1.0f - distance / distanceCollision;
                        _rwGCLightsLocalQueue(light, inten);
                    }
                }

                curLight = rwLLLinkGetNext(curLight);
            }

            cur = rwLLLinkGetNext(cur);
        }

        _rwGCLightsListFlush(pipeData);
    }

    if (pipeData->ambientLight) {
        if (pipeData->ambientLightColor.red > 1.0f) {
            pipeData->ambientLightColor.red = 1.0f;
        }

        if (pipeData->ambientLightColor.green > 1.0f) {
            pipeData->ambientLightColor.green = 1.0f;
        }

        if (pipeData->ambientLightColor.blue > 1.0f) {
            pipeData->ambientLightColor.blue = 1.0f;
        }
    }

    return object;
} // 0x802DD2FC

/*
 * Brings a geometry's vertex buffer up to date with what was locked since it was instanced: for
 * each kind of vertex data the geometry has (positions, normals, pre-lit colours and the texture
 * coordinate sets, in the buffer's attribute order), re-instances that data if it was locked, and
 * flushes it to memory. The node's data is not used.
 */
static void _rxGCDefaultReinstance(RpGeometry* geometry, RxGameCubeVertexBuffer* vbHeader, void* nodeData) {
    unsigned int numVerts;
    unsigned int size;
    unsigned int geomFlags;
    unsigned int count;
    unsigned short lockedFlags;
    RpGameCubeVtxFmt* vtxFmt;

    geomFlags = geometry->flags | rpGEOMETRYPOSITIONS;
    vtxFmt = RpGeometryGetGameCubeVtxFmt(geometry);
    count = 0;
    lockedFlags = geometry->lockedSinceLastInst;
    numVerts = geometry->numVertices;

    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    if (geomFlags & rpGEOMETRYPOSITIONS) {
        if (lockedFlags & rpGEOMETRYLOCKVERTICES) {
            size = _rwGCNVtxFmtInstPos3D(vbHeader->attr[0].array, geometry->morphTarget->verts, vtxFmt->pos,
                                         1 << vtxFmt->posFrac, geometry->numVertices, vbHeader->attr[0].stride, NULL);
            DCFlushRange(vbHeader->attr[0].array, size);
        }

        count++;
    }

    if (geomFlags & rpGEOMETRYNORMALS) {
        if (lockedFlags & rpGEOMETRYLOCKNORMALS) {
            RxGameCubeVertexAttr* attr = &vbHeader->attr[count];

            size = _rwGCNVtxFmtInstNrm(attr->array, geometry->morphTarget->normals, vtxFmt->norm, geometry->numVertices,
                                       attr->stride);
            DCFlushRange(attr->array, size);
        }

        count++;
    }

    if (geomFlags & rpGEOMETRYPRELIT) {
        if (lockedFlags & rpGEOMETRYLOCKPRELIGHT) {
            RxGameCubeVertexAttr* attr;

            if (RpGeometryGetGameCubeVtxFmt(geometry) == NULL) {
                unsigned int i;

                vbHeader->flags &= ~1;

                for (i = 0; i < numVerts; i++) {
                    if (geometry->preLitLum[i].alpha < 0xFF) {
                        vbHeader->flags |= 1;
                        break;
                    }
                }
            } else if (vtxFmt->preLight > 2) {
                vbHeader->flags |= 1;
            } else {
                vbHeader->flags &= ~1;
            }

            attr = &vbHeader->attr[count];
            size = _rwGCNVtxFmtInstClr(attr->array, geometry->preLitLum, vtxFmt->preLight, geometry->numVertices, attr->stride);
            DCFlushRange(attr->array, size);
        }

        count++;
    }

    if (geomFlags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        int i;

        for (i = 0; i < geometry->numTexCoordSets; i++) {
            if ((rpGEOMETRYLOCKTEXCOORDS1 << i) & lockedFlags) {
                size = _rwGCNVtxFmtInstTex(vbHeader->attr[count].array, geometry->texCoords[i], vtxFmt->texCoord[i],
                                           1 << vtxFmt->texCoordFrac[i], geometry->numVertices,
                                           vbHeader->attr[count].stride);
                DCFlushRange(vbHeader->attr[count].array, size);
            }

            count++;
        }
    }

    GXInvalidateVtxCache();
} // 0x802DD548

/*
 * The atomic's default reinstance stage: when the geometry was locked since it was instanced,
 * waits for the GX pipe to finish with its vertex buffer and re-instances the locked data; when
 * the morph interpolator changed, does the same for the interpolated positions.
 */
void* _rxGCAtomicDefaultReinstanceCallback(void* object, RxGameCubePipeData* pipeData) {
    RpAtomic* atomic = (RpAtomic*)object;
    RpGeometry* geometry = atomic->geometry;
    RxGameCubeVertexBuffer* vbHeader = (RxGameCubeVertexBuffer*)(pipeData->resEntry + 1);

    if (geometry->lockedSinceLastInst) {
        rxGCVertexBufferWaitDone(vbHeader);
        _rxGCDefaultReinstance(geometry, vbHeader, pipeData->nodeData);
        geometry->lockedSinceLastInst = 0;
    }

    if (geometry->numMorphTargets != 1 && (atomic->interpolator.flags & rpINTERPOLATORDIRTYINSTANCE)) {
        rxGCVertexBufferWaitDone(vbHeader);
        fn_802DC4B8(geometry, vbHeader, &atomic->interpolator);
        atomic->interpolator.flags &= ~rpINTERPOLATORDIRTYINSTANCE;
    }

    return object;
} // 0x802DD7C0

/* The atomic's default instance stage: builds the display list of the atomic's geometry. */
void* _rxGCAtomicDefaultInstanceCallback(void* object, RxGameCubePipeData* pipeData) {
    RpAtomic* atomic = (RpAtomic*)object;
    RpGeometry* geometry = atomic->geometry;
    void* owner;
    RwResEntry** resEntryOwner;

    if (geometry->numMorphTargets != 1) {
        owner = atomic;
        resEntryOwner = &atomic->repEntry;
    } else {
        owner = geometry;
        resEntryOwner = &geometry->repEntry;
    }

    if (geometry->flags & rpGEOMETRYNATIVEINSTANCE) {
        if (_RwDlPreInstanceOptimize == 1) {
            pipeData->resEntry = _rwDlGeometryInstanceOptimized(geometry, owner, resEntryOwner);
        } else {
            pipeData->resEntry = _rwDlGeometryInstanceFast(geometry, owner, resEntryOwner);
        }
    } else {
        pipeData->resEntry = _rwDlGeometryInstanceFast(geometry, owner, resEntryOwner);
    }

    geometry->lockedSinceLastInst = 0;

    return object;
} // 0x802DD92C

/*
 * The node's body: takes the atomic's display list if it has a current one (and drops it if the
 * meshes were rebuilt since), otherwise has the instance stage make it, then runs the reinstance,
 * lighting and render stages. Any stage that does not hand back the atomic fails the node.
 */
static RwBool _rxGCAtomicAllInOneNode(RxPipelineNode* self, const RxPipelineNodeParam* params) {
    RpAtomic* atomic;
    RpGeometry* geometry;
    RxGameCubeVertexBuffer* vbHeader;
    RpMeshHeader* meshHeader;
    RxGameCubePipeData pipeData;
    _rxGameCubeAllInOneNodeData* nodeData;

    atomic = (RpAtomic*)params->dataParam;
    geometry = atomic->geometry;
    nodeData = (_rxGameCubeAllInOneNodeData*)self->privateData;
    pipeData.nodeData = nodeData;

    if (!(geometry->flags & rpGEOMETRYNATIVE)) {
        if (geometry->numVertices == 0) {
            return TRUE;
        }

        meshHeader = geometry->mesh;

        if (meshHeader->numMeshes == 0) {
            return TRUE;
        }

        if (geometry->numMorphTargets != 1) {
            pipeData.resEntry = atomic->repEntry;
        } else {
            pipeData.resEntry = geometry->repEntry;
        }

        pipeData.meshHeader = geometry->mesh;
        pipeData.flags = geometry->flags;

        if (pipeData.resEntry) {
            vbHeader = (RxGameCubeVertexBuffer*)(pipeData.resEntry + 1);

            if (vbHeader->serialNumber != meshHeader->serialNum) {
                RwResourcesFreeResEntry(pipeData.resEntry);
                pipeData.resEntry = NULL;
            }
        }

        if (pipeData.resEntry) {
            if (nodeData->reinstanceCallback) {
                if (nodeData->reinstanceCallback(atomic, &pipeData) != atomic) {
                    return FALSE;
                }
            }

            RwResourcesUseResEntry(pipeData.resEntry);
        } else {
            if (nodeData->instanceCallback) {
                if (nodeData->instanceCallback(atomic, &pipeData) != atomic) {
                    return FALSE;
                }
            }

            if (nodeData->reinstanceCallback) {
                if (nodeData->reinstanceCallback(atomic, &pipeData) != atomic) {
                    return FALSE;
                }
            }
        }
    } else {
        pipeData.resEntry = geometry->repEntry;
        pipeData.meshHeader = geometry->mesh;
        pipeData.flags = geometry->flags;

        if (nodeData->reinstanceCallback) {
            if (nodeData->reinstanceCallback(atomic, &pipeData) != atomic) {
                return FALSE;
            }
        }
    }

    if (nodeData->lightingCallback) {
        if (nodeData->lightingCallback(atomic, &pipeData) != atomic) {
            return FALSE;
        }
    }

    if (nodeData->renderCallback) {
        if (nodeData->renderCallback(atomic, &pipeData) != atomic) {
            return FALSE;
        }
    }

    return TRUE;
} // 0x802DD9DC

/* Gives a new node of this kind the atomic pipeline's default stages. */
static RwBool _rxGCAtomicAllInOnePipelineInit(RxPipelineNode* node) {
    _rxGameCubeAllInOneNodeData* nodeData = (_rxGameCubeAllInOneNodeData*)node->privateData;

    nodeData->instanceCallback = _rxGCAtomicDefaultInstanceCallback;
    nodeData->reinstanceCallback = _rxGCAtomicDefaultReinstanceCallback;
    nodeData->lightingCallback = _rxGCAtomicDefaultLightingCallback;
    nodeData->renderCallback = _rxGCDefaultRenderCallback;

    return TRUE;
} // 0x802DDC64
