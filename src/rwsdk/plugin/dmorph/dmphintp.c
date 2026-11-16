// Unit name and boundary: Gormiti link map (RpDMorph.a dmphintp.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpDMorph.a dmphintp.obj), Final and Debug builds.
// DMorphAtomicCreateDMorphValues, DMorphAtomicCreateInterpolator: Gormiti Debug build (functions there; inlined in retail).
/*
 * dmphintp.c: the delta-morph extension of atomics. An atomic keeps the weight of each of its geometry's delta-morph
 * targets (values) and an interpolator that animates them. Its render callback is replaced by one that first
 * applies the weights that changed to the geometry (the delta-morph build), then calls the callback it replaced.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_DMORPHPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/* The offset of the delta-morph data in an atomic. */
RwInt32 rpDMorphAtomicDataOffset;

/*
 * ==================== DMorphAtomicLinearBuild ====================
 * The atomic's render callback. Moves the geometry's vertices, normals, pre-light colours and texture
 * coordinates by the change of each delta-morph weight since the last build, then calls the render callback this
 * one replaced. The weights are linear, so only the change is applied: the targets' deltas are run-length encoded,
 * a code byte with its top bit set says that the next (code & 0x7F) data elements change that many vertices in a
 * row, one without it skips (code & 0x7F) vertices. Pre-light colours are bytes, so the old contribution is taken
 * off and the new one put on instead of the change.
 */
static RpAtomic* DMorphAtomicLinearBuild(RpAtomic* atomic) {
    rpDMorphAtomicData* atomicData;
    rpDMorphGeometryData* geometryData;
    RpGeometry* geometry;
    RpDMorphTarget* dMorphTarget;
    rpDMorphRLE* rle;
    RwV3d* baseVertices;
    RwV3d* baseNormals;
    RwRGBA* basePreLightColors;
    RwTexCoords* baseTexCoords;
    RwV3d* vertices;
    RwV3d* normals;
    RwRGBA* preLightColors;
    RwTexCoords* texCoords;
    RwReal dDelta;
    RwUInt32 i, j, k;
    RwUInt8 l;

    atomicData = RPDMORPHATOMICGETDATA(atomic);
    geometryData = RPDMORPHGEOMETRYGETDATA(atomic->geometry);

    if (geometryData != NULL && atomicData->values != NULL) {
        geometry = atomic->geometry;
        baseVertices = geometry->morphTarget[0].verts;
        baseNormals = geometry->morphTarget[0].normals;
        basePreLightColors = geometry->preLitLum;
        baseTexCoords = geometry->texCoords[0];

        for (i = 0; i < geometryData->numDMorphTargets; i++) {
            if (atomicData->values[i] != geometryData->currValues[i]) {
                dDelta = atomicData->values[i] - geometryData->currValues[i];

                dMorphTarget = &geometryData->dMorphTargets[i];
                rle = &dMorphTarget->rle;
                vertices = dMorphTarget->vertices;
                normals = dMorphTarget->normals;
                preLightColors = dMorphTarget->preLightColors;
                texCoords = dMorphTarget->texCoords;

                RpGeometryLock(geometry, dMorphTarget->lockFlags);

                if (dMorphTarget->flags & rpGEOMETRYPOSITIONS) {
                    for (j = 0, k = 0; j < rle->numCodeElements; j++) {
                        if (rle->code[j] & 0x80) {
                            for (l = 0; l < (rle->code[j] & 0x7F); l++) {
                                baseVertices[k].x += dDelta * vertices->x;
                                baseVertices[k].y += dDelta * vertices->y;
                                baseVertices[k].z += dDelta * vertices->z;
                                vertices++;
                                k++;
                            }
                        } else {
                            k += rle->code[j] & 0x7F;
                        }
                    }
                }

                if (dMorphTarget->flags & rpGEOMETRYNORMALS) {
                    for (j = 0, k = 0; j < rle->numCodeElements; j++) {
                        if (rle->code[j] & 0x80) {
                            for (l = 0; l < (rle->code[j] & 0x7F); l++, k++) {
                                baseNormals[k].x += dDelta * normals->x;
                                baseNormals[k].y += dDelta * normals->y;
                                baseNormals[k].z += dDelta * normals->z;
                                normals++;
                            }
                        } else {
                            k += rle->code[j] & 0x7F;
                        }
                    }
                }

                if (dMorphTarget->flags & rpGEOMETRYPRELIT) {
                    for (j = 0, k = 0; j < rle->numCodeElements; j++) {
                        if (rle->code[j] & 0x80) {
                            for (l = 0; l < (rle->code[j] & 0x7F); l++, preLightColors++) {
                                float lastValue = geometryData->currValues[i];
                                float thisValue = atomicData->values[i];
                                int shift;

                                shift = (int)(lastValue * preLightColors->alpha);
                                basePreLightColors[k].alpha -= (RwUInt8)shift;
                                shift = (int)(lastValue * preLightColors->blue);
                                basePreLightColors[k].blue -= (RwUInt8)shift;
                                shift = (int)(lastValue * preLightColors->green);
                                basePreLightColors[k].green -= (RwUInt8)shift;
                                shift = (int)(lastValue * preLightColors->red);
                                basePreLightColors[k].red -= (RwUInt8)shift;

                                shift = (int)(thisValue * preLightColors->alpha);
                                basePreLightColors[k].alpha += (RwUInt8)shift;
                                shift = (int)(thisValue * preLightColors->blue);
                                basePreLightColors[k].blue += (RwUInt8)shift;
                                shift = (int)(thisValue * preLightColors->green);
                                basePreLightColors[k].green += (RwUInt8)shift;
                                shift = (int)(thisValue * preLightColors->red);
                                basePreLightColors[k].red += (RwUInt8)shift;
                                k++;
                            }
                        } else {
                            k += rle->code[j] & 0x7F;
                        }
                    }
                }

                if (dMorphTarget->flags & rpGEOMETRYTEXTURED) {
                    for (j = 0, k = 0; j < rle->numCodeElements; j++) {
                        if (rle->code[j] & 0x80) {
                            for (l = 0; l < (rle->code[j] & 0x7F); l++) {
                                baseTexCoords[k].u += dDelta * texCoords->u;
                                baseTexCoords[k].v += dDelta * texCoords->v;
                                texCoords++;
                                k++;
                            }
                        } else {
                            k += rle->code[j] & 0x7F;
                        }
                    }
                }

                RpGeometryUnlock(geometry);
            }

            geometryData->currValues[i] = atomicData->values[i];
        }
    }

    atomicData->defRenderCallBack(atomic);

    return atomic;
} // 0x802B9F04

/*
 * ==================== DMorphAtomicCreateDMorphValues ====================
 * Gives an atomic a weight for each delta-morph target of its geometry, all zero. Returns NULL when the engine has
 * no memory for them.
 */
static RpAtomic* DMorphAtomicCreateDMorphValues(RpAtomic* atomic) {
    rpDMorphAtomicData* atomicData;
    RwUInt32 memSize;

    atomicData = RPDMORPHATOMICGETDATA(atomic);

    memSize = RpDMorphGeometryGetNumDMorphTargets(atomic->geometry) * sizeof(RwReal);
    atomicData->values = (RwReal*)RwMalloc(memSize, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
    if (atomicData->values == NULL) {
        RWERROR((E_RW_NOMEM, memSize));
        return NULL;
    }

    memset(atomicData->values, 0, memSize);

    return atomic;
} // no copy in retail (inlined)

/*
 * ==================== DMorphAtomicCreateInterpolator ====================
 * Gives an atomic an interpolator with room for the key and the time of each delta-morph target of its geometry,
 * all cleared. Returns NULL when the engine has no memory for it.
 */
static RpAtomic* DMorphAtomicCreateInterpolator(RpAtomic* atomic) {
    rpDMorphInterpolator* interpolator;
    RwUInt32 numTargets;
    RwUInt32 memSize;

    numTargets = RpDMorphGeometryGetNumDMorphTargets(atomic->geometry);
    memSize = sizeof(rpDMorphInterpolator) + numTargets * (sizeof(RwReal) + sizeof(rpDMorphFrame*));
    interpolator = (rpDMorphInterpolator*)RwMalloc(memSize, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
    if (interpolator == NULL) {
        RWERROR((E_RW_NOMEM, memSize));
        return NULL;
    }

    memset(interpolator, 0, memSize);
    interpolator->currTimes = (RwReal*)(interpolator + 1);
    interpolator->currFrames = (rpDMorphFrame**)(interpolator->currTimes + numTargets);
    RPDMORPHATOMICGETDATA(atomic)->interpolator = interpolator;

    return atomic;
} // no copy in retail (inlined)

/*
 * ==================== DMorphAtomicCtor ====================
 * Plugin constructor: a new atomic has no weights, no interpolator and no render callback to call.
 */
static void* DMorphAtomicCtor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpAtomic* atomic;
    rpDMorphAtomicData* atomicData;

    atomic = (RpAtomic*)object;
    atomicData = RPDMORPHATOMICGETDATA(atomic);

    atomicData->values = NULL;
    atomicData->interpolator = NULL;
    atomicData->defRenderCallBack = NULL;

    return object;
} // 0x802BA454

/*
 * ==================== DMorphAtomicDtor ====================
 * Plugin destructor: frees the weights and the interpolator, if the atomic has them.
 */
static void* DMorphAtomicDtor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpAtomic* atomic;
    rpDMorphAtomicData* atomicData;

    atomic = (RpAtomic*)object;
    atomicData = RPDMORPHATOMICGETDATA(atomic);

    if (atomicData->values != NULL) {
        RwFree(atomicData->values);
        atomicData->values = NULL;
    }

    if (atomicData->interpolator != NULL) {
        RwFree(atomicData->interpolator);
        atomicData->interpolator = NULL;
    }

    atomicData->defRenderCallBack = NULL;

    return object;
} // 0x802BA46C

/*
 * ==================== DMorphAtomicCopy ====================
 * Plugin copy: an atomic whose geometry has delta-morph targets gets a copy of the source's weights, but no
 * interpolator, and the render callback the source had replaced.
 */
static void* DMorphAtomicCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpAtomic* srcAtomic;
    RpAtomic* dstAtomic;
    RpGeometry* geom;

    srcAtomic = (RpAtomic*)srcObject;
    dstAtomic = (RpAtomic*)dstObject;
    geom = srcAtomic->geometry;

    if (geom != NULL) {
        RwUInt32 numTargets = RpDMorphGeometryGetNumDMorphTargets(geom);

        if (numTargets != 0) {
            rpDMorphAtomicData* srcAtomicData = RPDMORPHATOMICGETDATA(srcAtomic);
            rpDMorphAtomicData* dstAtomicData = RPDMORPHATOMICGETDATA(dstAtomic);

            if (DMorphAtomicCreateDMorphValues(dstAtomic) != NULL) {
                if (srcAtomicData->values != NULL) {
                    memcpy(dstAtomicData->values, srcAtomicData->values, numTargets * sizeof(RwReal));
                } else {
                    memset(dstAtomicData->values, 0, numTargets * sizeof(RwReal));
                }
            }

            dstAtomicData->interpolator = NULL;
            dstAtomicData->defRenderCallBack = srcAtomicData->defRenderCallBack;
        }
    }

    return dstObject;
} // 0x802BA4F8

/*
 * ==================== DMorphAtomicAlways ====================
 * Plugin stream callback: an atomic whose geometry has delta-morph targets gets the delta-morph build as its
 * render callback, and keeps the one it had to call from it.
 */
static RwBool DMorphAtomicAlways(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpAtomic* atomic;
    RwUInt32 numDMorphTargets;

    atomic = (RpAtomic*)object;

    numDMorphTargets = RpDMorphGeometryGetNumDMorphTargets(atomic->geometry);
    if (numDMorphTargets) {
        rpDMorphAtomicData* atomicData = RPDMORPHATOMICGETDATA(atomic);

        atomicData->defRenderCallBack = atomic->renderCallBack;
        atomic->renderCallBack = DMorphAtomicLinearBuild;
    }

    return TRUE;
} // 0x802BA60C

/*
 * ==================== DMorphAtomicRights ====================
 * Plugin stream callback: always accepts.
 */
static RwBool DMorphAtomicRights(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject, RwUInt32 extraData) {
    return TRUE;
} // 0x802BA664

/*
 * ==================== _rpDMorphAtomicPluginAttach ====================
 * Registers the delta-morph extension of atomics and its stream callbacks. Returns FALSE when the engine has no
 * room for it.
 */
RwBool _rpDMorphAtomicPluginAttach(void) {
    rpDMorphAtomicDataOffset = RpAtomicRegisterPlugin(sizeof(rpDMorphAtomicData), rwID_DMORPHPLUGIN,
                                                      DMorphAtomicCtor, DMorphAtomicDtor, DMorphAtomicCopy);
    if (rpDMorphAtomicDataOffset < 0) {
        return FALSE;
    }

    RpAtomicSetStreamAlwaysCallBack(rwID_DMORPHPLUGIN, DMorphAtomicAlways);
    RpAtomicSetStreamRightsCallBack(rwID_DMORPHPLUGIN, DMorphAtomicRights);

    return TRUE;
} // 0x802BA66C

/*
 * ==================== RpDMorphAtomicGetDMorphValues ====================
 * The weight of each delta-morph target of an atomic's geometry, created (all zero) on first use. Writing them
 * is how an atomic is morphed without an animation. Returns NULL when they cannot be created.
 */
float* RpDMorphAtomicGetDMorphValues(RpAtomic* atomic) {
    rpDMorphAtomicData* atomicData;

    atomicData = RPDMORPHATOMICGETDATA(atomic);

    if (atomicData->values == NULL) {
        DMorphAtomicCreateDMorphValues(atomic);
    }

    return atomicData->values;
} // 0x802BA6E4

/*
 * ==================== RpDMorphAtomicSetAnimation ====================
 * Makes an animation the one the atomic plays from its start, creating the interpolator and the weights if the
 * atomic has none yet; a NULL animation stops the animation, and the weights stay as they are. Returns NULL when
 * the engine has no memory for them.
 */
RpAtomic* RpDMorphAtomicSetAnimation(RpAtomic* atomic, RpDMorphAnimation* animation) {
    rpDMorphAtomicData* atomicData;

    atomicData = RPDMORPHATOMICGETDATA(atomic);

    if (animation == NULL) {
        if (atomicData->interpolator != NULL) {
            RwUInt32 i;

            for (i = 0; i < RpDMorphGeometryGetNumDMorphTargets(atomic->geometry); i++) {
                atomicData->interpolator->currFrames[i] = NULL;
            }
            atomicData->interpolator->animation = NULL;
        }
    } else {
        RwUInt32 i;
        rpDMorphInterpolator* interpolator;
        RwReal* values;

        if (atomicData->interpolator == NULL) {
            if (DMorphAtomicCreateInterpolator(atomic) == NULL) {
                return NULL;
            }
        }

        if (atomicData->values == NULL) {
            if (DMorphAtomicCreateDMorphValues(atomic) == NULL) {
                return NULL;
            }
        }

        interpolator = atomicData->interpolator;
        values = atomicData->values;

        interpolator->animation = animation;
        interpolator->animTime = 0.0f;

        for (i = 0; i < RpDMorphGeometryGetNumDMorphTargets(atomic->geometry); i++) {
            if (i < animation->numDMorphTargets && animation->frameSequences[i].frames != NULL) {
                interpolator->currFrames[i] = animation->frameSequences[i].frames;
                interpolator->currTimes[i] = 0.0f;
                values[i] = animation->frameSequences[i].frames[0].startValue;
            } else {
                interpolator->currFrames[i] = NULL;
                interpolator->currTimes[i] = 0.0f;
                values[i] = 0.0f;
            }
        }
    }

    return atomic;
} // 0x802BA79C

/*
 * ==================== fn_802BAA34 ====================
 * The animation the atomic plays, NULL when it has no interpolator. Name not known: retail has this accessor
 * where Gormiti has RpDMorphAtomicInitialize (callers: fn_800231D4, fn_80023C64, fn_80023E60).
 */
RpDMorphAnimation* fn_802BAA34(RpAtomic* atomic) {
    rpDMorphAtomicData* atomicData;

    atomicData = RPDMORPHATOMICGETDATA(atomic);

    if (atomicData->interpolator != NULL) {
        return atomicData->interpolator->animation;
    }

    return NULL;
} // 0x802BAA34

/*
 * ==================== RpDMorphAtomicAddTime ====================
 * Advances an atomic's animation by deltaTime. Each target's weight follows its keys: when the time in a key
 * passes its duration the next key starts with what is left, and a target without a next key stops at the last
 * one. Calls the loop callback when a target went back to an earlier key.
 */
RpAtomic* RpDMorphAtomicAddTime(RpAtomic* atomic, RwReal deltaTime) {
    rpDMorphAtomicData* atomicData;
    rpDMorphInterpolator* interpolator;
    rpDMorphFrame** seqFrame;
    RwReal* frameTime;
    int looped;
    RwUInt32 i;

    atomicData = RPDMORPHATOMICGETDATA(atomic);
    interpolator = atomicData->interpolator;
    looped = FALSE;

    interpolator->animTime += deltaTime;

    seqFrame = interpolator->currFrames;
    frameTime = interpolator->currTimes;

    for (i = 0; i < RpDMorphGeometryGetNumDMorphTargets(atomic->geometry); i++) {
        if (*seqFrame != NULL) {
            *frameTime += deltaTime;

            while (*frameTime > (*seqFrame)->duration) {
                *frameTime -= (*seqFrame)->duration;

                if ((*seqFrame)->nextFrame == -1) {
                    *seqFrame = NULL;
                    break;
                } else {
                    rpDMorphFrame* nextFrame = &interpolator->animation->frameSequences[i].frames[(*seqFrame)->nextFrame];

                    looped |= (nextFrame < *seqFrame);
                    *seqFrame = nextFrame;
                }
            }

            if (*seqFrame != NULL) {
                atomicData->values[i] = (*seqFrame)->startValue +
                                        (*frameTime * (*seqFrame)->recipDuration) *
                                            ((*seqFrame)->endValue - (*seqFrame)->startValue);
            }
        }

        seqFrame++;
        frameTime++;
    }

    if (looped && interpolator->loopCallBack != NULL) {
        interpolator->loopCallBack(atomic, interpolator->loopCallBackData);
    }

    return atomic;
} // 0x802BAA58

/*
 * ==================== RpDMorphAtomicSetAnimLoopCallBack ====================
 * Sets the function RpDMorphAtomicAddTime calls when an animation loops, and the data it is given. The atomic
 * has to have an interpolator (the callback is not checked).
 */
RpAtomic* RpDMorphAtomicSetAnimLoopCallBack(RpAtomic* atomic, RpAtomic* (*callBack)(RpAtomic*, void*), void* data) {
    rpDMorphAtomicData* atomicData;

    atomicData = RPDMORPHATOMICGETDATA(atomic);

    atomicData->interpolator->loopCallBack = callBack;
    atomicData->interpolator->loopCallBackData = data;

    return atomic;
} // 0x802BABE0
