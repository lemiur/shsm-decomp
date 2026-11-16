// Unit name and boundary: Gormiti link map (RPworld.a climaxcommon)
// Names, parameters and locals: Gormiti DWARF; RwClimaxWorldRenderAtomics and the unused setters: Gormiti Debug build
// and link map
/*
 * climaxcommon.c: Climax's additions to the world plugin. A world sector render also renders the
 * atomics tied to the sector: each atomic that is in view is queued on the lists of the passes
 * its pass byte selects, and RwClimaxAtomicRenderPass later renders a list of atomics through a
 * callback. An atomic keeps its pass byte and light mask in the two bytes of its pad member, a
 * light and a material their flags in theirs. The setters for the material mask, the reflection
 * offset and pseudo HDR are the functions Gormiti's link dropped (UNUSED there) and retail keeps.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/bacamera.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/climaxcommon.h>

/* The head of each pass's list of atomics, and the entries the lists are made of. */
StAtomicRef* RwClimaxAtomicPasses[rwCLIMAXNUMPASSES];
StAtomicRef RwClimaxAtomicsBuffer[rwCLIMAXATOMICSBUFFERSIZE];

RwUInt32 RwClimaxMaterialMask = 1;

/* The compiler lays the zero-initialised small data out in reverse order of definition. */
RwInt32 _Current_RwClimaxReflectionOffset;

/* The first free entry of the buffer. */
StAtomicRef* RwClimaxCurrentAtomicRef;

/* How many entries of the buffer a frame used (high-water mark and current), and how many atomics did not fit. */
RwUInt32 RwClimaxAtomics_OverflowCurrent;
RwUInt32 RwClimaxAtomics_OverflowMax;
RwUInt32 RwClimaxAtomics_UsedCurrent;
RwUInt32 RwClimaxAtomics_UsedMax;

RwUInt32 RwClimaxWorldRenderFlags;
RwBool RwClimaxPseudoHDR;

/* Sets the reflection offset (the unit's only setter of it; nothing in the game calls this). */
void RwClimaxSetReflectionOffset(RwInt32 offset) {
    _Current_RwClimaxReflectionOffset = offset;
} // 0x802DB4FC

/*
 * Queues each atomic of the sector that is in view and not yet seen this frame on the lists of the passes its pass byte
 * selects.
 */
static void RwClimaxWorldRenderAtomics(RpWorldSector* worldSector) {
    RwLLLink* cur;
    RwLLLink* end;
    RpTie* tie;
    RpAtomic* atomic;

    RWASSERT(worldSector);

    cur = rwLinkListGetFirstLLLink(&worldSector->collAtomicsInWorldSector);
    end = rwLinkListGetTerminator(&worldSector->collAtomicsInWorldSector);
    while (cur != end) {
        tie = rwLLLinkGetData(cur, RpTie, lAtomicInWorldSector);
        atomic = tie->apAtom;
        RWASSERT(rwObjectGetType(atomic) == rpATOMIC);

        if ((rwObjectGetFlags(atomic) & rpATOMICRENDER) && atomic->renderFrame != RWSRCGLOBAL(renderFrame)) {
            const RwSphere* atomicBoundingSphere = RpAtomicGetWorldBoundingSphere(atomic);

            RWASSERT(atomicBoundingSphere);

            if (RwCameraFrustumTestSphere(RWSRCGLOBAL(curCamera), atomicBoundingSphere)) {
                int AtomicPass = (atomic->pad >> 8) & 0xFF;
                int PassInd;

                for (PassInd = 0; AtomicPass != 0; PassInd++, AtomicPass >>= 1) {
                    if (AtomicPass & 1) {
                        if (RwClimaxCurrentAtomicRef != &RwClimaxAtomicsBuffer[rwCLIMAXATOMICSBUFFERSIZE]) {
                            RwClimaxCurrentAtomicRef->atomic = atomic;
                            RwClimaxCurrentAtomicRef->next = RwClimaxAtomicPasses[PassInd];
                            RwClimaxAtomicPasses[PassInd] = RwClimaxCurrentAtomicRef;
                            RwClimaxCurrentAtomicRef++;
                        } else {
                            RwClimaxAtomics_OverflowCurrent++;
                        }
                    }
                }
            }
            atomic->renderFrame = RWSRCGLOBAL(renderFrame);
        }
        cur = rwLLLinkGetNext(cur);
    }
} // 0x802DB504

/* Renders a world sector's geometry and then, if the render flags ask for it, its atomics (a sector callback; data is unused). */
RpWorldSector* RwClimaxWorldSectorRender(RpWorldSector* sector, void* data) {
    RWASSERT(sector);

    if (!(RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGNOSECTORS)) {
        if (RpWorldSectorRender(sector) && (RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGATOMICS)) {
            RwClimaxWorldRenderAtomics(sector);
        }
    } else if (RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGATOMICS) {
        RwClimaxWorldRenderAtomics(sector);
    }
    return sector;
} // 0x802DB604

/* Sets the pass byte of an atomic (the high byte of its pad member). */
void RwClimaxAtomicSetPass(RpAtomic* atomic, RwUInt32 pass) {
    atomic->pad &= rwCLIMAXATOMICLIGHTMASKMASK;
    atomic->pad |= (pass & 0xFF) << 8;
} // 0x802DB66C

/* Sets the light mask of an atomic (the low byte of its pad member). */
void RwClimaxAtomicSetLightMask(RpAtomic* atomic, RwUInt32 lightMask) {
    atomic->pad &= rwCLIMAXATOMICPASSMASK;
    atomic->pad |= lightMask & 0xFF;
} // 0x802DB680

/* Sets the flags of a material (its pad member). */
void RwClimaxMaterialSetFlags(RpMaterial* material, RwUInt16 flags) {
    material->pad = flags;
} // 0x802DB694

RwInt16 RwClimaxMaterialGetFlags(RpMaterial* material) {
    return material->pad;
} // 0x802DB69C

void RwClimaxMaterialSetMask(RwUInt32 mask) {
    RwClimaxMaterialMask = mask;
} // 0x802DB6A4

RwUInt32 RwClimaxMaterialGetMask(void) {
    return RwClimaxMaterialMask;
} // 0x802DB6AC

/* The pass byte of an atomic. */
RwUInt32 RwClimaxAtomicGetPass(RpAtomic* atomic) {
    return (atomic->pad & rwCLIMAXATOMICPASSMASK) >> 8;
} // 0x802DB6B4

/* The light mask of an atomic. */
RwUInt32 RwClimaxAtomicGetLightMask(RpAtomic* atomic) {
    return atomic->pad & rwCLIMAXATOMICLIGHTMASKMASK;
} // 0x802DB6C0

/* Sets the flags of a light (its pad member). */
void RwClimaxLightSetFlags(RpLight* light, RwUInt16 flags) {
    light->pad = flags;
} // 0x802DB6CC

RwUInt16 RwClimaxLightGetFlags(RpLight* light) {
    return light->pad;
} // 0x802DB6D4

/* Sets the pseudo HDR switch; always answers FALSE. */
RwBool RwClimaxSetPseudoHDR(RwBool pseudoHDR) {
    RwClimaxPseudoHDR = pseudoHDR;
    return FALSE;
} // 0x802DB6DC

/*
 * Renders the lists of the passes selected by the pass bits, each atomic through the callback: it answers 1 for the
 * atomic's own render callback and 2 for the default one. With no callback every atomic uses its own.
 */
void RwClimaxAtomicRenderPass(RwUInt32 pass, RwUInt32 flags, RwClimaxAtomicPassCallBack atomicCallBack) {
    RwUInt32 OldRenderFlags = RwClimaxWorldRenderFlags;
    int PassInd;

    RwClimaxWorldRenderFlags |= flags | rwCLIMAXRENDERFLAGPASS;
    for (PassInd = 0; pass != 0; PassInd++, pass >>= 1) {
        if (pass & 1) {
            StAtomicRef* ref = RwClimaxAtomicPasses[PassInd];

            if (atomicCallBack != NULL) {
                for (; ref != NULL; ref = ref->next) {
                    switch (atomicCallBack(ref->atomic)) {
                    case 1:
                        RpAtomicRender(ref->atomic);
                        break;
                    case 2:
                        AtomicDefaultRenderCallBack(ref->atomic);
                        break;
                    }
                }
            } else {
                for (; ref != NULL; ref = ref->next) {
                    RpAtomicRender(ref->atomic);
                }
            }
        }
    }
    RwClimaxWorldRenderFlags = OldRenderFlags;
} // 0x802DB6E8

/* Renders an atomic through its own pipeline (or the current atomic pipeline) with the render flags raised. */
void RwClimaxAtomicRender(RpAtomic* atomic, RwUInt32 flags) {
    RwUInt32 OldRenderFlags;
    RxPipeline* pipeline = atomic->pipeline;

    OldRenderFlags = RwClimaxWorldRenderFlags;
    RwClimaxWorldRenderFlags |= flags | rwCLIMAXRENDERFLAGPASS;
    if (pipeline == NULL) {
        pipeline = RXPIPELINEGLOBAL(currentAtomicPipeline);
    }
    RxPipelineExecute(pipeline, atomic, TRUE);
    RwClimaxWorldRenderFlags = OldRenderFlags;
} // 0x802DB7CC
