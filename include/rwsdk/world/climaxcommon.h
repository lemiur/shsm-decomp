// Name: Gormiti link map (RPworld.a climaxcommon)
// Layout: Gormiti DWARF (StAtomicRef)
#ifndef RWSDK_WORLD_CLIMAXCOMMON_H
#define RWSDK_WORLD_CLIMAXCOMMON_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/balight.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/basector.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Atomics the pass buffer holds. Retail's buffer is smaller than Gormiti's (800). Name invented. */
#define rwCLIMAXATOMICSBUFFERSIZE 500

/* Render passes an atomic can take part in: one bit each in its pass byte. Name invented. */
#define rwCLIMAXNUMPASSES 8

/* The two bytes of an atomic's pad member: the pass byte (high) and the light mask (low). Names invented. */
#define rwCLIMAXATOMICPASSMASK 0xFF00
#define rwCLIMAXATOMICLIGHTMASKMASK 0x00FF

/* The bits of RwClimaxWorldRenderFlags. Names invented. */
#define rwCLIMAXRENDERFLAGNOLIGHTING 0x00000001 /* the render takes no lights: the light stages enable none */
#define rwCLIMAXRENDERFLAGATOMICS 0x00000008   /* a world sector render also renders the atomics in the sector */
#define rwCLIMAXRENDERFLAGNOSECTORS 0x00000010 /* a world sector render skips the sector's own geometry */
#define rwCLIMAXRENDERFLAGPASS 0x40000000      /* set while an atomic is rendered through a pass */

/* The bits of a light's flags (its pad member), set by RwClimaxLightSetFlags. Names invented. */
#define rwCLIMAXLIGHTFLAGSECONDMASK 0x0100 /* the light also sets its bit in the second half of a pipe data's light mask */
#define rwCLIMAXLIGHTFLAGPRIORITY 0x0200   /* the light queue ranks the light a full unit higher */

/* StAtomicRef: an entry of a pass's list of the atomics to render. */
typedef struct StAtomicRef {
    struct StAtomicRef* next;   /* 0x0 */
    RpAtomic* atomic;           /* 0x4 */
} StAtomicRef;

/* A pass's render function: tells how to render the atomic, 1 for its render callback, 2 for the default one. */
typedef unsigned int (*RwClimaxAtomicPassCallBack)(RpAtomic* atomic);

/* The head of each pass's list of atomics, and the entries the lists are made of. */
extern StAtomicRef* RwClimaxAtomicPasses[rwCLIMAXNUMPASSES];
extern StAtomicRef RwClimaxAtomicsBuffer[rwCLIMAXATOMICSBUFFERSIZE];

extern RwUInt32 RwClimaxMaterialMask;
extern RwBool RwClimaxPseudoHDR;
extern RwUInt32 RwClimaxWorldRenderFlags;
extern RwUInt32 RwClimaxAtomics_UsedMax;
extern RwUInt32 RwClimaxAtomics_UsedCurrent;
extern RwUInt32 RwClimaxAtomics_OverflowMax;
extern RwUInt32 RwClimaxAtomics_OverflowCurrent;
extern StAtomicRef* RwClimaxCurrentAtomicRef;
extern RwInt32 _Current_RwClimaxReflectionOffset;

extern void RwClimaxSetReflectionOffset(RwInt32 offset);
extern RpWorldSector* RwClimaxWorldSectorRender(RpWorldSector* sector, void* data);
extern void RwClimaxAtomicSetPass(RpAtomic* atomic, RwUInt32 pass);
extern void RwClimaxAtomicSetLightMask(RpAtomic* atomic, RwUInt32 lightMask);
extern void RwClimaxMaterialSetFlags(RpMaterial* material, RwUInt16 flags);
extern RwInt16 RwClimaxMaterialGetFlags(RpMaterial* material);
extern void RwClimaxMaterialSetMask(RwUInt32 mask);
extern RwUInt32 RwClimaxMaterialGetMask(void);
extern RwUInt32 RwClimaxAtomicGetPass(RpAtomic* atomic);
extern RwUInt32 RwClimaxAtomicGetLightMask(RpAtomic* atomic);
extern void RwClimaxLightSetFlags(RpLight* light, RwUInt16 flags);
extern RwUInt16 RwClimaxLightGetFlags(RpLight* light);
extern RwBool RwClimaxSetPseudoHDR(RwBool pseudoHDR);
extern void RwClimaxAtomicRenderPass(RwUInt32 pass, RwUInt32 flags, RwClimaxAtomicPassCallBack atomicCallBack);
extern void RwClimaxAtomicRender(RpAtomic* atomic, RwUInt32 flags);

#ifdef __cplusplus
}
#endif

#endif
