// Unit name and boundary: Gormiti link map (RwCore.a p2renderstate.obj)
// Function and struct layouts: Gormiti DWARF (p2renderstate.c).

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/batextur.h>
#include <rwsdk/baraster.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/pipe/p2/bapipe.h>

/*
 * RxRenderStateVectorSetDefaultRenderStateVector (Gormiti DWARF): resets or initialises a render
 * state vector to its default state.
 */
RxRenderStateVector* RxRenderStateVectorSetDefaultRenderStateVector(RxRenderStateVector* rsvp) {
    if (rsvp) {
        if (RWSRCGLOBAL(engineStatus) == rwENGINESTATUSSTARTED) {
            *rsvp = RXPIPELINEGLOBAL(defaultRenderState);
        } else if (rsvp != &RXPIPELINEGLOBAL(defaultRenderState)) {
            RWERROR((E_RW_PLUGINNOTINIT));
            return (RxRenderStateVector*)NULL;
        } else {
            const RwRGBA OpaqueWhite = {0xFF, 0xFF, 0xFF, 0xFF};

            rsvp->Flags = rxRENDERSTATEFLAG_TEXTUREPERSPECTIVE |
                          rxRENDERSTATEFLAG_ZTESTENABLE |
                          rxRENDERSTATEFLAG_ZWRITEENABLE;
            rsvp->ShadeMode = rwSHADEMODEGOURAUD;
            rsvp->SrcBlend = rwBLENDSRCALPHA;
            rsvp->DestBlend = rwBLENDINVSRCALPHA;
            rsvp->TextureRaster = (RwRaster*)NULL;
            rsvp->AddressModeU = rwTEXTUREADDRESSWRAP;
            rsvp->AddressModeV = rwTEXTUREADDRESSWRAP;
            rsvp->FilterMode = rwFILTERLINEAR;
            rsvp->BorderColor = OpaqueWhite;
            rsvp->FogType = rwFOGTYPENAFOGTYPE;
            rsvp->FogColor = OpaqueWhite;
        }
        return rsvp;
    }

    RWERROR((E_RW_NULLP));
    return (RxRenderStateVector*)NULL;
} // 0x8029E9F0
