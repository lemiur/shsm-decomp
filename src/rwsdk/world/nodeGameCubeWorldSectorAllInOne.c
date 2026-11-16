// Unit name and boundary: Gormiti link map (RPworld.a nodeGameCubeWorldSectorAllInOne.obj)
// Names, parameters and locals: Gormiti DWARF (nodeGameCubeWorldSectorAllInOne.c)
/*
 * nodeGameCubeWorldSectorAllInOne.c: the world sector pipeline of the GameCube. One node does what
 * the whole sector pipeline needs: it instances a sector's vertex data into a display list, finds
 * the lights that fall on it and draws it, through four stages the game may replace.
 */
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/pipe/p2/p2define.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/gclights.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/instanceworld.h>
#include <rwsdk/world/nodeGameCubeWorldSectorAllInOne.h>

static RwBool _rxGCWorldSectorAllInOneNode(RxPipelineNode* self, const RxPipelineNodeParam* params);
static RwBool _rxGCWorldSectorAllInOnePipelineInit(RxPipelineNode* node);

/* The definition of the world sector all-in-one node. */
RxNodeDefinition* RxNodeDefinitionGetGameCubeWorldSectorAllInOne(void) {
    static RxNodeDefinition nodeGameCubeWorldSectorAllInOneCSL = {
        "GamerCubeWorldSectorAllInOne.csl",
        {
            _rxGCWorldSectorAllInOneNode,
            NULL,
            NULL,
            _rxGCWorldSectorAllInOnePipelineInit,
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

    return &nodeGameCubeWorldSectorAllInOneCSL;
} // no copy in retail (dead-stripped or inlined)

/*
 * Builds a world sector pipeline of one all-in-one node and sets its four stages. The pipeline's
 * plugin id and data are the id and type of the pipeline the caller makes.
 */
RxPipeline* _rpDlSectorPipelineCreate(unsigned int id, unsigned int type, RxGameCubePipeCallback instance,
                                      RxGameCubePipeCallback reinstance, RxGameCubePipeCallback lighting,
                                      RxGameCubePipeCallback render) {
    RxPipeline* pipe;
    RxPipeline* lpipe;
    RxNodeDefinition* nodeDfn;
    RxPipelineNode* node;

    pipe = RxPipelineCreate();
    pipe->pluginId = id;
    pipe->pluginData = type;

    nodeDfn = RxNodeDefinitionGetGameCubeWorldSectorAllInOne();

    lpipe = RxPipelineLock(pipe);
    lpipe = RxLockedPipeAddFragment(lpipe, NULL, nodeDfn, NULL);
    RxLockedPipeUnlock(lpipe);

    node = RxPipelineFindNodeByName(pipe, nodeDfn->name, NULL, NULL);
    _rxGameCubeAllInOneSetInstanceCallBack(node, instance);
    _rxGameCubeAllInOneSetReinstanceCallBack(node, reinstance);
    _rxGameCubeAllInOneSetLightingCallBack(node, lighting);
    RxGameCubeAllInOneSetRenderCallBack(node, render);

    return pipe;
} // 0x802DDCA0

/*
 * The sector's default lighting stage: asks for the world's own lights when the world is lit, adds
 * up the lights that reach the sector, and clamps the ambient light to white.
 */
void* _rxGCSectorDefaultLightingCallback(void* object, RxGameCubePipeData* pipeData) {
    unsigned int flags;

    pipeData->lightMask = 0;
    pipeData->ambientLightColor.red = 0.0f;
    pipeData->ambientLightColor.green = 0.0f;
    pipeData->ambientLightColor.blue = 0.0f;
    pipeData->ambientLightColor.alpha = 1.0f;
    pipeData->ambientLight = 0;
    pipeData->numActiveLights = 0;
    pipeData->numConsideredLights = 0;

    flags = ((RpWorld*)RWSRCGLOBAL(curWorld))->flags;

    if (flags & rpWORLDLIGHT) {
        RpWorldSector* sector = (RpWorldSector*)object;
        RwLLLink* curLight;
        RwLLLink* endLight;

        _rwGCLightsGlobalEnable(rpLIGHTLIGHTWORLD, pipeData);

        RWSRCGLOBAL(lightFrame)++;

        curLight = rwLinkListGetFirstLLLink(&sector->lightsInWorldSector);
        endLight = rwLinkListGetTerminator(&sector->lightsInWorldSector);

        _rwGCLightsListInit();

        while (curLight != endLight) {
            RpLightTie* lightTie = rwLLLinkGetData(curLight, RpLightTie, lightInWorldSector);
            RpLight* light = lightTie->light;

            if (light && (RpLightGetFlags(light) & rpLIGHTLIGHTWORLD)) {
                float inten = rwGCLIGHTINTENSITYSHARE * (light->color.red + light->color.green + light->color.blue);

                _rwGCLightsLocalQueue(light, inten);
            }

            curLight = rwLLLinkGetNext(curLight);
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
} // 0x802DDD54

/* The sector's default instance stage: builds the display list of the sector's vertex data. */
void* _rxGCSectorDefaultInstanceCallback(void* object, RxGameCubePipeData* pipeData) {
    RpWorldSector* sector = (RpWorldSector*)object;
    RpWorld* world = (RpWorld*)RWSRCGLOBAL(curWorld);

    if (world->flags & rpWORLDNATIVEINSTANCE) {
        if (_RwDlPreInstanceOptimize == 1) {
            pipeData->resEntry = _rwDlWorldSectorInstanceOptimized(world, sector, sector, &sector->repEntry);
        } else {
            pipeData->resEntry = _rwDlWorldSectorInstanceFast(world, sector, sector, &sector->repEntry);
        }
    } else {
        pipeData->resEntry = _rwDlWorldSectorInstanceFast(world, sector, sector, &sector->repEntry);
    }

    return object;
} // 0x802DDEA8

/*
 * The node's body: takes the sector's display list if it has a current one (and drops it if the
 * meshes were rebuilt since), otherwise has the instance stage make it, then runs the lighting and
 * render stages. Any stage that does not hand back the sector fails the node.
 */
static RwBool _rxGCWorldSectorAllInOneNode(RxPipelineNode* self, const RxPipelineNodeParam* params) {
    RpWorld* world;
    RpWorldSector* sector;
    RpMeshHeader* meshHeader;
    _rxGameCubeAllInOneNodeData* nodeData;
    RxGameCubePipeData pipeData;

    sector = (RpWorldSector*)params->dataParam;
    nodeData = (_rxGameCubeAllInOneNodeData*)self->privateData;
    pipeData.nodeData = nodeData;
    world = (RpWorld*)RWSRCGLOBAL(curWorld);

    if (!(world->flags & rpWORLDNATIVE)) {
        RxGameCubeVertexBuffer* vbHeader;

        if (sector->numVertices == 0) {
            return TRUE;
        }

        meshHeader = sector->mesh;

        if (meshHeader->numMeshes == 0) {
            return TRUE;
        }

        pipeData.resEntry = sector->repEntry;
        pipeData.meshHeader = sector->mesh;
        pipeData.flags = world->flags;

        if (pipeData.resEntry) {
            vbHeader = (RxGameCubeVertexBuffer*)(pipeData.resEntry + 1);

            if (vbHeader->serialNumber != meshHeader->serialNum) {
                RwResourcesFreeResEntry(pipeData.resEntry);
                pipeData.resEntry = NULL;
            }
        }

        if (pipeData.resEntry) {
            RwResourcesUseResEntry(pipeData.resEntry);
        } else if (nodeData->instanceCallback) {
            if (nodeData->instanceCallback(sector, &pipeData) != sector) {
                return FALSE;
            }
        }
    } else {
        pipeData.resEntry = sector->repEntry;
        pipeData.meshHeader = sector->mesh;
        pipeData.flags = world->flags;
    }

    if (nodeData->lightingCallback) {
        if (nodeData->lightingCallback(sector, &pipeData) != sector) {
            return FALSE;
        }
    }

    if (nodeData->renderCallback) {
        if (nodeData->renderCallback(sector, &pipeData) != sector) {
            return FALSE;
        }
    }

    return TRUE;
} // 0x802DDF44

/* Gives a new node of this kind the sector pipeline's default stages. */
static RwBool _rxGCWorldSectorAllInOnePipelineInit(RxPipelineNode* node) {
    _rxGameCubeAllInOneNodeData* nodeData = (_rxGameCubeAllInOneNodeData*)node->privateData;

    nodeData->instanceCallback = _rxGCSectorDefaultInstanceCallback;
    nodeData->reinstanceCallback = NULL;
    nodeData->lightingCallback = _rxGCSectorDefaultLightingCallback;
    nodeData->renderCallback = _rxGCDefaultRenderCallback;

    return TRUE;
} // 0x802DE134
