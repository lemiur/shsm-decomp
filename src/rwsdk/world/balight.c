// Reference: tgsm/pbg src/rwsdk/world/balight.c
// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt and splits.txt (function names, unit layout)
// Reference: fdlibm e_acosf.c (the arc cosine's constants and steps)
// Reference: aap/librw src/light.cpp (the light chunk's fields)
/*
 * balight.c: RenderWare lights. A light hangs from a frame and lights atomics, the world or both;
 * its colour, radius and (for spot lights) cone are kept with it, and the light module's free
 * list hands out the lights.
 */
#include <rwsdk/world/rpplugin.h>
#include <math.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/balight.h>
#include <string.h>

/* Light chunks older than this stored the cone as a tangent, not as minus its cosine. Name invented. */
#define rwLIGHTCONEVERSION 0x30300

/* The free list of lights; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rpLightFreeList;

/* The plugins registered with lights: no plugin data yet, so a light is just an RpLight. */
RwPluginRegistry lightTKList = {
    sizeof(RpLight),
    sizeof(RpLight),
    0,
    0,
    NULL,
    NULL,
};

/* The light module's registration (offset of its globals in the engine instance). */
static RwModuleInfo lightModule;

/* Lights per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rpLightFreeListBlockSize = 32;
static RwInt32 _rpLightFreeListPreallocBlocks = 1;

/* The light module's globals (rpLightGlobals, balight.h). Macro name invented, after bacamera's. */
#define RWLIGHTGLOBAL(var) \
    (RWPLUGINOFFSET(rpLightGlobals, RwEngineInstance, lightModule.globalsOffset)->var)

/*
 * The arc cosine in single precision: fdlibm's __ieee754_acosf with the square root taken by
 * _rwSqrt, and a magnitude of one or more clamped to the nearer end instead of a NaN. Name
 * invented; no copy in retail (inlined into RpLightGetConeAngle). Two unnamed RenderWare functions
 * elsewhere inline the same code, so it may come from a shared header.
 */
static inline RwReal LightACos(RwReal x) {
    static const RwReal one = 1.0f;
    static const RwReal pi = 3.1415925026e+00f;
    static const RwReal pio2_hi = 1.5707962513e+00f;
    static const RwReal pio2_lo = 7.5497894159e-08f;
    static const RwReal pS0 = 1.6666667163e-01f;
    static const RwReal pS1 = -3.2556581497e-01f;
    static const RwReal pS2 = 2.0121252537e-01f;
    static const RwReal pS3 = -4.0055535734e-02f;
    static const RwReal pS4 = 7.9153501429e-04f;
    static const RwReal pS5 = 3.4793309169e-05f;
    static const RwReal qS1 = -2.4033949375e+00f;
    static const RwReal qS2 = 2.0209457874e+00f;
    static const RwReal qS3 = -6.8828397989e-01f;
    static const RwReal qS4 = 7.7038154006e-02f;
    /* Declared in reverse: MWCC's register and stack-slot choices follow this order (retail's). */
    RwReal df, c, s, w, r, q, p, z;
    RwInt32 ix, hx;
    RwSplitBits dfBits;
    RwSplitBits sBits;
    RwSplitBits xBits;

    xBits.nReal = x;
    hx = xBits.nInt;
    ix = hx & 0x7FFFFFFF; /* the bits of |x| */
    if (ix >= 0x3F800000) { /* |x| >= 1 */
        if (hx > 0) {
            return 0.0f;
        }
        return pi + 2.0f * pio2_lo;
    }

    if (ix < 0x3F000000) { /* |x| < 0.5 */
        if (ix <= 0x23000000) { /* |x| < 2**-57 */
            return pio2_hi + pio2_lo;
        }
        z = x * x;
        p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
        q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
        r = p / q;
        return pio2_hi - (x - (pio2_lo - x * r));
    } else if (hx < 0) { /* x < -0.5 */
        z = 0.5f * (one + x);
        p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
        q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
        s = _rwSqrt(z);
        r = p / q;
        w = r * s - pio2_lo;
        return pi - 2.0f * (s + w);
    } else { /* x > 0.5 */
        z = 0.5f * (one - x);
        s = _rwSqrt(z);
        sBits.nReal = s;
        dfBits.nInt = sBits.nInt & 0xFFFFF000; /* s with the low 12 bits of its mantissa cleared */
        df = dfBits.nReal;
        c = (z - df * df) / (s + df);
        p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
        q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
        r = p / q;
        w = r * s + c;
        return 2.0f * (df + w);
    }
}

/* Destroys a light the module still holds when it closes (a free-list callback). */
static void LightTidyDestroyLight(void* light, void* data) {
    RpLightDestroy((RpLight*)light);
} // 0x802D34D8

/* A light keeps nothing that depends on its frame, so a frame change needs no work. */
static RwObjectHasFrame* LightSync(RwObjectHasFrame* object) {
    return object;
} // 0x802D3534

/* Sets the light's radius and marks its frame for an update. */
RpLight* RpLightSetRadius(RpLight* light, RwReal radius) {
    RwFrame* frame = RpLightGetFrame(light);

    light->radius = radius;
    if (frame != NULL) {
        RwFrameUpdateObjects(frame);
    }

    return light;
} // 0x802D3538

/* Sets the light's colour and notes whether it is a grey. */
RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* color) {
    light->color = *color;
    if (light->color.red == light->color.green && light->color.red == light->color.blue) {
        rwObjectSetPrivateFlags(light, rpLIGHTPRIVATENOCHROMA);
    } else {
        rwObjectSetPrivateFlags(light, 0);
    }

    return light;
} // 0x802D357C

/* Returns the angle of the light's cone, in radians. */
RwReal RpLightGetConeAngle(const RpLight* light) {
    return LightACos(-light->minusCosAngle);
} // 0x802D35D0

/* Sets the angle of the light's cone, in radians up to a right angle; NULL if it is outside. */
RpLight* RpLightSetConeAngle(RpLight* light, RwReal angle) {
    if (angle < rpLIGHTMINCONEANGLE || angle > rpLIGHTMAXCONEANGLE) {
        return NULL;
    }

    light->minusCosAngle = -cosf(angle);

    return light;
} // 0x802D3884

/* Registers a plugin's data with every light. */
RwInt32 RpLightRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&lightTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D38E0

/* Registers the stream callbacks of a plugin's light data. */
RwInt32 RpLightRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                    RwPluginDataChunkWriteCallBack writeCB,
                                    RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&lightTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x802D3910

/* Reads a light (its structure chunk, then its plugin data) from the stream. */
RpLight* RpLightStreamRead(RwStream* stream) {
    RwUInt32 length;
    RwUInt32 version;
    RpLightChunkInfo lightInfo;
    RpLight* light;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        memset(&lightInfo, 0, sizeof(lightInfo));
        if (RwStreamRead(stream, &lightInfo, length) != length) {
            return NULL;
        }
        RwMemNative32(&lightInfo, sizeof(lightInfo));

        light = RpLightCreate((lightInfo.typeAndFlags >> 16) & 0xFF);
        if (light == NULL) {
            return NULL;
        }

        light->radius = lightInfo.radius;
        light->color.red = lightInfo.red;
        light->color.green = lightInfo.green;
        light->color.blue = lightInfo.blue;
        light->pad = 0;

        if (version < rwLIGHTCONEVERSION) {
            /* The old chunk held the tangent of the cone's angle. */
            light->minusCosAngle =
                -_rwSqrt(1.0f / (1.0f + lightInfo.minusCosAngle * lightInfo.minusCosAngle));
        } else {
            light->minusCosAngle = lightInfo.minusCosAngle;
        }

        if (light->color.red == light->color.green && light->color.red == light->color.blue) {
            rwObjectSetPrivateFlags(light, rpLIGHTPRIVATENOCHROMA);
        } else {
            rwObjectSetPrivateFlags(light, 0);
        }

        RpLightSetFlags(light, lightInfo.typeAndFlags);

        if (!_rwPluginRegistryReadDataChunks(&lightTKList, stream, light)) {
            return NULL;
        }

        return light;
    }

    RWERROR((E_RW_BADVERSION));
    return NULL;
} // 0x802D3938

/* Destroys the light: its plugin data, its frame link, then the light itself. */
RwBool RpLightDestroy(RpLight* light) {
    _rwPluginRegistryDeInitObject(&lightTKList, light);
    rwObjectHasFrameReleaseFrame(light);
    RwFreeListFree(RWLIGHTGLOBAL(lightFreeList), light);

    return TRUE;
} // 0x802D3AF0

/* Creates a light of the given type: white, with no radius, lighting atomics and the world. */
RpLight* RpLightCreate(RwInt32 type) {
    RpLight* light = (RpLight*)RwFreeListAlloc(RWLIGHTGLOBAL(lightFreeList), rwMEMHINTDUR_EVENT | rwID_LIGHT);

    if (light == NULL) {
        return NULL;
    }

    rwObjectInitialize(light, rpLIGHT, type);
    light->object.sync = LightSync;

    light->radius = 0.0f;
    light->minusCosAngle = 0.0f;
    light->pad = 0;

    light->color.red = 1.0f;
    light->color.green = 1.0f;
    light->color.blue = 1.0f;
    light->color.alpha = 1.0f;
    rwObjectSetPrivateFlags(light, rpLIGHTPRIVATENOCHROMA);

    rwLinkListInitialize(&light->WorldSectorsInLight);
    rwLLLinkInitialize(&light->inWorld);

    light->lightFrame = RWSRCGLOBAL(lightFrame) - 1;

    RpLightSetFlags(light, rpLIGHTLIGHTATOMICS | rpLIGHTLIGHTWORLD);

    _rwPluginRegistryInitObject(&lightTKList, light);

    return light;
} // 0x802D3B50

/* Closes the light module: destroys every light still held, then the light free list. */
void* _rpLightClose(void* instance, RwInt32 offset, RwInt32 size) {
    RwFreeListForAllUsed(RWLIGHTGLOBAL(lightFreeList), LightTidyDestroyLight, NULL);
    RwFreeListDestroy(RWLIGHTGLOBAL(lightFreeList));
    RWLIGHTGLOBAL(lightFreeList) = NULL;

    lightModule.numInstances--;

    return instance;
} // 0x802D3C40

/* Opens the light module: creates the light free list. */
void* _rpLightOpen(void* instance, RwInt32 offset, RwInt32 size) {
    lightModule.globalsOffset = offset;

    RWLIGHTGLOBAL(lightFreeList) = RwFreeListCreateAndPreallocateSpace(
        lightTKList.sizeOfStruct, _rpLightFreeListBlockSize, sizeof(RwUInt32),
        _rpLightFreeListPreallocBlocks, &_rpLightFreeList, rwMEMHINTDUR_GLOBAL | rwID_LIGHT);
    if (RWLIGHTGLOBAL(lightFreeList) != NULL) {
        lightModule.numInstances++;
        return instance;
    }

    return NULL;
} // 0x802D3CB8
