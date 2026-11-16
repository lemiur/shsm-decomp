// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: tgsm/pbg include/rwsdk/world/balight.h
#ifndef RWSDK_WORLD_BALIGHT_H
#define RWSDK_WORLD_BALIGHT_H

#include <rwsdk/baframe.h>
#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/batkreg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The object type of a light (aap/librw light.cpp, Light::ID). */
#define rpLIGHT 3

/* Light types from this value on have a position (bfbbdecomp/bfbb rpworld.h). */
#define rpLIGHTPOSITIONINGSTART 0x80

/* The range of a spot light's cone angle, in radians (bfbbdecomp/bfbb rpworld.h). */
#define rpLIGHTMINCONEANGLE ((RwReal)0.0f)
#define rpLIGHTMAXCONEANGLE (rwPIOVER2)

/* RpLightType: the kind of a light, kept in its object's subType. */
typedef enum RpLightType {
    rpNALIGHTTYPE = 0,
    rpLIGHTDIRECTIONAL = 1,
    rpLIGHTAMBIENT = 2,
    rpLIGHTPOINT = rpLIGHTPOSITIONINGSTART,
    rpLIGHTSPOT = 0x81,
    rpLIGHTSPOTSOFT = 0x82,
    rpLIGHTTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RpLightType;

/* RpLightFlag: what a light lights, kept in its object's flags. */
typedef enum RpLightFlag {
    rpLIGHTLIGHTATOMICS = 0x01,
    rpLIGHTLIGHTWORLD = 0x02,
    rpLIGHTFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RpLightFlag;

/* rpLightPrivateFlag: kept in the light object's privateFlags (tgsm/pbg balight.h). */
typedef enum rpLightPrivateFlag {
    rpLIGHTPRIVATENOCHROMA = 0x01, /* the colour is a grey (red, green and blue equal) */
    rpLIGHTPRIVATEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} rpLightPrivateFlag;

/*
 * RpLight: a light attached to a frame: its radius and colour, the cone of a spot light (as minus
 * the cosine of its angle), the world sectors it lights and its link in its world's light list.
 * Size 0x40, from retail's light plugin registry.
 */
typedef struct RpLight {
    RwObjectHasFrame object;        /* 0x0 */
    RwReal radius;                  /* 0x14 */
    RwRGBAReal color;               /* 0x18 */
    RwReal minusCosAngle;           /* 0x28 */
    RwLinkList WorldSectorsInLight; /* 0x2C */
    RwLLLink inWorld;               /* 0x34 */
    RwUInt16 lightFrame;            /* 0x3C */
    RwUInt16 pad;                   /* 0x3E */ // pad: bfbbdecomp/bfbb rpworld.h RpLight member name
} RpLight;

/*
 * RpLightChunkInfo: a light's structure chunk: its radius, colour and cone, then its type (bits
 * 16 to 23) and its flags (the low byte).
 */
typedef struct RpLightChunkInfo {
    RwReal radius;         /* 0x0 */
    RwReal red;            /* 0x4 */
    RwReal green;          /* 0x8 */
    RwReal blue;           /* 0xC */
    RwReal minusCosAngle;  /* 0x10 */
    RwUInt32 typeAndFlags; /* 0x14 */
} RpLightChunkInfo;

/* rpLightGlobals: the light module's globals, the free list of lights. */
typedef struct rpLightGlobals {
    RwFreeList* lightFreeList; /* 0x0 */
} rpLightGlobals;

/* Called for each light of an iteration; NULL stops it. */
typedef RpLight* (*RpLightCallBack)(RpLight* light, void* data);

/* Light accessors (bfbbdecomp/bfbb rpworld.h, the SDK's release macros). */
#define RpLightGetRadius(_light) ((_light)->radius)
#define RpLightGetColor(_light) (&((_light)->color))
#define RpLightSetFrame(_light, _frame) (rwObjectHasFrameSetFrame((_light), (_frame)), (_light))
#define RpLightGetFrame(_light) ((RwFrame*)rwObjectGetParent((_light)))
#define RpLightGetType(_light) ((RpLightType)rwObjectGetSubType((_light)))
#define RpLightSetFlags(_light, _flags) ((rwObjectSetFlags((_light), (_flags))), (_light))
#define RpLightGetFlags(_light) (rwObjectGetFlags((_light)))

/* The plugins registered with lights. */
extern RwPluginRegistry lightTKList;

RpLight* RpLightSetRadius(RpLight* light, RwReal radius);
RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* color);
RwReal RpLightGetConeAngle(const RpLight* light);
RpLight* RpLightSetConeAngle(RpLight* light, RwReal angle);
RwInt32 RpLightRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpLightRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                    RwPluginDataChunkWriteCallBack writeCB,
                                    RwPluginDataChunkGetSizeCallBack getSizeCB);
RpLight* RpLightStreamRead(RwStream* stream);
RwBool RpLightDestroy(RpLight* light);
RpLight* RpLightCreate(RwInt32 type);
void* _rpLightClose(void* instance, RwInt32 offset, RwInt32 size);
void* _rpLightOpen(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
