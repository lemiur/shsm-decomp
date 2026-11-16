// Unit name and boundary: Gormiti link map (RPworld.a gclights.obj; its source there is pipe/p2/gcn/gclights.c)
// Names, parameters and locals: Gormiti DWARF (Final and Debug builds)
/*
 * gclights.c: the lights of the GameCube pipeline. The hardware has eight lights. A world's
 * directional and ambient lights are applied to a pipeline's data whole (_rwGCLightsGlobalEnable);
 * the point and spot lights near an object are queued with their contribution, and the strongest
 * the hardware can still take are loaded (_rwGCLightsListFlush).
 */
#include <revolution/gx/GXLighting.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/driver/wii/dldevice.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/climaxcommon.h>
#include <rwsdk/world/gclights.h>

/* A light of the queue and how much it lights the object. */
typedef struct StLighQueueItem {
    RpLight* light;
    float contrib;
} StLighQueueItem;

#define rwDLLIGHTQUEUESIZE 16 /* name invented */

/* The hardware's light objects, loaded as the lights are enabled. */
static GXLightObj _RwGCLightObjs[rwDLMAXHWLIGHTS];

/* The lights at most loaded by the list flush. */
static int _MaxLights = 4;

static StLighQueueItem sLightQueue[rwDLLIGHTQUEUESIZE];
static int sLightQueueCount;

RwInt32 _RwDlLightExtOffset;

/*
 * ==================== _rpGCHWLightingApplyDirectionalLight ====================
 * Loads a directional light into hardware light lightNum: a light placed far away against the
 * direction it shines in, with the light's colour (scaled to 128 for the pseudo HDR) and attenuation.
 */
static void _rpGCHWLightingApplyDirectionalLight(RpLight* light, int lightNum) {
    RwV3d at;
    GXColor color;
    RwRGBAReal* lightColor;
    _rwDlLightExt* lightExt;

    RwV3dTransformVector(&at, &RwFrameGetLTM(RpLightGetFrame(light))->at, &_RwDlInvCamLTM);
    lightExt = RWPLUGINOFFSET(_rwDlLightExt, light, _RwDlLightExtOffset);
    if (lightExt->flags == 0) {
        GXInitLightAttn(&_RwGCLightObjs[lightNum], 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    } else {
        GXInitLightAttn(&_RwGCLightObjs[lightNum], lightExt->a0, lightExt->a1, lightExt->a2, lightExt->k0,
                        lightExt->k1, lightExt->k2);
    }
    GXInitLightPos(&_RwGCLightObjs[lightNum], -1048576.0f * -at.x, -1048576.0f * at.y, -1048576.0f * -at.z);
    lightColor = RpLightGetColor(light);
    if (RwClimaxPseudoHDR) {
        color.r = (unsigned char)(128.0f * lightColor->red);
        color.g = (unsigned char)(128.0f * lightColor->green);
        color.b = (unsigned char)(128.0f * lightColor->blue);
    } else {
        color.r = (unsigned char)(255.0f * lightColor->red);
        color.g = (unsigned char)(255.0f * lightColor->green);
        color.b = (unsigned char)(255.0f * lightColor->blue);
    }
    color.a = 0;
    GXInitLightColor(&_RwGCLightObjs[lightNum], color);
    GXLoadLightObjImm(&_RwGCLightObjs[lightNum], (GXLightID)(1 << lightNum));
} // 0x802DBC2C

/*
 * ==================== _rwGCLightsGlobalEnable ====================
 * Goes through the current world's directional and ambient lights that light what lightFlags names:
 * a directional light is loaded into the next hardware light, an ambient light adds to the pipeline
 * data's ambient colour. Retail is another revision than Gormiti's (size 0x120, not 0x11C): it tests the light's
 * flags against the low byte of lightFlags (the cast), and reads the current world before the render flag.
 */
void _rwGCLightsGlobalEnable(RpLightFlag lightFlags, RxGameCubePipeData* pipeData) {
    RpWorld* world = (RpWorld*)RWSRCGLOBAL(curWorld);
    RwLLLink* cur;
    RwLLLink* end;

    if (!(RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGNOLIGHTING)) {
        cur = rwLinkListGetFirstLLLink(&world->directionalLightList);
        end = rwLinkListGetTerminator(&world->directionalLightList);
        while (cur != end) {
            RpLight* light = rwLLLinkGetData(cur, RpLight, inWorld);

            if (light && (RpLightGetFlags(light) & (RwUInt8)lightFlags)) {
                if (RpLightGetType(light) == rpLIGHTDIRECTIONAL) {
                    _rpGCHWLightingApplyDirectionalLight(light, pipeData->numActiveLights);
                    pipeData->lightMask |= 1 << pipeData->numActiveLights;
                    if (light->pad & rwCLIMAXLIGHTFLAGSECONDMASK) {
                        pipeData->lightMask |= (1 << pipeData->numActiveLights) << 16;
                    }
                    pipeData->numActiveLights++;
                    pipeData->numConsideredLights++;
                } else {
                    pipeData->ambientLightColor.red += light->color.red;
                    pipeData->ambientLightColor.green += light->color.green;
                    pipeData->ambientLightColor.blue += light->color.blue;
                    pipeData->ambientLight = 1;
                    pipeData->numConsideredLights++;
                }
            }
            cur = rwLLLinkGetNext(cur);
        }
    }
} // 0x802DBE0C

/*
 * ==================== _rwGCLightsLocalEnable ====================
 * Loads a point, spot or soft spot light into the next hardware light of the pipeline data: its
 * colour, its position (and direction) in view space, its angle and distance attenuation. The switch has
 * empty cases for the directional and ambient types, as retail's compare tree shows.
 * Not exact (register/schedule wall, 7 rows): retail loads and tests the light's flags (lhz, rlwinm.) right
 * after GXLoadLightObjImm, before the light mask is updated; here the test follows the update.
 */
void _rwGCLightsLocalEnable(RpLight* light, RxGameCubePipeData* pipeData) {
    RwV3d at;
    RwV3d pos;
    GXColor color;
    RwMatrix* matrix;
    RwRGBAReal* lightColor;
    _rwDlLightExt* lightExt;

    if (!(RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGNOLIGHTING) && pipeData->numActiveLights < rwDLMAXHWLIGHTS) {
        lightColor = RpLightGetColor(light);
        if (RwClimaxPseudoHDR) {
            color.r = (unsigned char)(128.0f * lightColor->red);
            color.g = (unsigned char)(128.0f * lightColor->green);
            color.b = (unsigned char)(128.0f * lightColor->blue);
        } else {
            color.r = (unsigned char)(255.0f * lightColor->red);
            color.g = (unsigned char)(255.0f * lightColor->green);
            color.b = (unsigned char)(255.0f * lightColor->blue);
        }
        color.a = 0;
        GXInitLightColor(&_RwGCLightObjs[pipeData->numActiveLights], color);
        matrix = RwFrameGetLTM(RpLightGetFrame(light));
        RwV3dTransformPoint(&pos, &matrix->pos, &_RwDlInvCamLTM);
        GXInitLightPos(&_RwGCLightObjs[pipeData->numActiveLights], -pos.x, pos.y, -pos.z);
        lightExt = RWPLUGINOFFSET(_rwDlLightExt, light, _RwDlLightExtOffset);
        switch (RpLightGetType(light)) {
        case rpLIGHTDIRECTIONAL:
        case rpLIGHTAMBIENT:
            break;
        case rpLIGHTPOINT:
            if (lightExt->flags == 0) {
                GXInitLightAttnA(&_RwGCLightObjs[pipeData->numActiveLights], 1.0f, 0.0f, 0.0f);
                GXInitLightDistAttn(&_RwGCLightObjs[pipeData->numActiveLights], 0.5f * light->radius, 0.5f,
                                    GX_DA_MEDIUM);
            } else {
                GXInitLightAttn(&_RwGCLightObjs[pipeData->numActiveLights], lightExt->a0, lightExt->a1,
                                lightExt->a2, lightExt->k0, lightExt->k1, lightExt->k2);
            }
            break;
        case rpLIGHTSPOT:
            RwV3dTransformVector(&at, &matrix->at, &_RwDlInvCamLTM);
            GXInitLightDir(&_RwGCLightObjs[pipeData->numActiveLights], -at.x, at.y, -at.z);
            if (lightExt->flags == 0) {
                GXInitLightSpot(&_RwGCLightObjs[pipeData->numActiveLights], 57.29578f * RpLightGetConeAngle(light),
                                GX_SP_FLAT);
                GXInitLightDistAttn(&_RwGCLightObjs[pipeData->numActiveLights], 0.5f * light->radius, 0.5f,
                                    GX_DA_MEDIUM);
            } else {
                GXInitLightAttn(&_RwGCLightObjs[pipeData->numActiveLights], lightExt->a0, lightExt->a1,
                                lightExt->a2, lightExt->k0, lightExt->k1, lightExt->k2);
            }
            break;
        case rpLIGHTSPOTSOFT:
            RwV3dTransformVector(&at, &matrix->at, &_RwDlInvCamLTM);
            GXInitLightDir(&_RwGCLightObjs[pipeData->numActiveLights], -at.x, at.y, -at.z);
            if (lightExt->flags == 0) {
                GXInitLightSpot(&_RwGCLightObjs[pipeData->numActiveLights], 57.29578f * RpLightGetConeAngle(light),
                                GX_SP_COS);
                GXInitLightDistAttn(&_RwGCLightObjs[pipeData->numActiveLights], 0.5f * light->radius, 0.5f,
                                    GX_DA_MEDIUM);
            } else {
                GXInitLightAttn(&_RwGCLightObjs[pipeData->numActiveLights], lightExt->a0, lightExt->a1,
                                lightExt->a2, lightExt->k0, lightExt->k1, lightExt->k2);
            }
            break;
        }
        GXLoadLightObjImm(&_RwGCLightObjs[pipeData->numActiveLights], (GXLightID)(1 << pipeData->numActiveLights));
        pipeData->lightMask |= 1 << pipeData->numActiveLights;
        if (light->pad & rwCLIMAXLIGHTFLAGSECONDMASK) {
            pipeData->lightMask |= (1 << pipeData->numActiveLights) << 16;
        }
        pipeData->numActiveLights++;
    }
} // 0x802DBF2C

/* Empties the light queue. */
void _rwGCLightsListInit(void) {
    sLightQueueCount = 0;
} // 0x802DC2F8

/*
 * ==================== _rwGCLightsListFlush ====================
 * Loads the queued lights, the strongest first, until the hardware lights or the permitted number
 * are used up; the lights left over only count as considered.
 */
void _rwGCLightsListFlush(RxGameCubePipeData* pipeData) {
    int ind;

    pipeData->numConsideredLights += sLightQueueCount;
    while (pipeData->numActiveLights < _MaxLights && sLightQueueCount > 0) {
        int Highindex;
        float HighContrib;

        HighContrib = sLightQueue[0].contrib;
        Highindex = 0;
        for (ind = 1; ind < sLightQueueCount; ind++) {
            if (sLightQueue[ind].contrib > HighContrib) {
                HighContrib = sLightQueue[ind].contrib;
                Highindex = ind;
            }
        }
        _rwGCLightsLocalEnable(sLightQueue[Highindex].light, pipeData);
        if (Highindex != sLightQueueCount - 1) {
            sLightQueue[Highindex] = sLightQueue[sLightQueueCount - 1];
        }
        sLightQueueCount--;
    }
} // 0x802DC304

/*
 * Queues a light with its contribution; a light flagged as a priority is ranked a full unit higher.
 * Not exact (register/schedule wall): retail loads the light's flags (lhz) before the two stores into the
 * queue entry and keeps the count in r5; here the load follows the stores. Compiled and ruled out: the
 * early return, a pointer to the entry.
 */
void _rwGCLightsLocalQueue(RpLight* light, float contrib) {
    if (sLightQueueCount < rwDLLIGHTQUEUESIZE) {
        sLightQueue[sLightQueueCount].light = light;
        sLightQueue[sLightQueueCount].contrib = contrib;
        if (light->pad & rwCLIMAXLIGHTFLAGPRIORITY) {
            sLightQueue[sLightQueueCount].contrib += 1.0f;
        }
        sLightQueueCount++;
    }
} // 0x802DC404

/* The light plugin's constructor: a light starts with the default attenuation. */
static void* rwDlLightExtCnst(void* object, int offsetInObject, int sizeInObject) {
    _rwDlLightExt* lightExt = RWPLUGINOFFSET(_rwDlLightExt, object, _RwDlLightExtOffset);

    lightExt->flags = 0;
    return object;
} // 0x802DC454

/* The light plugin's destructor: nothing to release. */
static void* rwDlLightExtDest(void* object, int offsetInObject, int sizeInObject) {
    return object;
} // 0x802DC464

/* The light plugin's copy: the data is not copied. */
static void* rwDlLightExtCopy(void* dstObject, const void* srcObject, int offsetInObject, int sizeInObject) {
    return dstObject;
} // 0x802DC468

/* Registers the light plugin; FALSE when the registry has no room for it. */
RwInt32 _rpDlLightPluginAttach(void) {
    _RwDlLightExtOffset = RpLightRegisterPlugin(sizeof(_rwDlLightExt), rwID_DLLIGHTPLUGIN, rwDlLightExtCnst,
                                                rwDlLightExtDest, rwDlLightExtCopy);
    return _RwDlLightExtOffset >= 0;
} // 0x802DC46C
