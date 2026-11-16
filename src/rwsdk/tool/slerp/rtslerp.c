// Unit name and boundary: Gormiti link map (RtSlerp.a rtslerp.obj)
// Names, locals and statement order: Gormiti link map and DWARF (RtSlerp.a rtslerp.obj).
/*
 * rtslerp.c: the slerp toolkit. It interpolates between two matrices along the rotation that
 * takes the first to the second. RtSlerpSetLerp and RtQuatSetupSlerpArgandCache have no copy in
 * retail (dead-stripped). The toolkit's error records carry rwID_SLERPPLUGIN (0x183).
 */
#include <rwsdk/badevice.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/tool/slerp/rtslerp.h>

#define rwPLUGIN_ID rwID_SLERPPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/* A rotation of this many degrees or more is interpolated as a rotation; less, as a linear blend. Name invented. */
#define rtSLERPMINANGLE 2.0f

/*
 * ==================== RtSlerpCreate ====================
 * Allocates a slerp and creates the matrices its mask does not reference.
 */
RtSlerp* RtSlerpCreate(RwInt32 nMatRefMask) {
    RtSlerp* spSlerp;

    spSlerp = (RtSlerp*)RwMalloc(sizeof(RtSlerp), rwMEMHINTDUR_EVENT | rwPLUGIN_ID);
    if (spSlerp == NULL) {
        RWERROR((E_RW_NOMEM, sizeof(RtSlerp)));
        return NULL;
    }

    spSlerp->matRefMask = nMatRefMask;

    if (nMatRefMask & rtSLERPREFSTARTMAT) {
        spSlerp->startMat = NULL;
    } else {
        spSlerp->startMat = RwMatrixCreate();
    }

    if (spSlerp->matRefMask & rtSLERPREFENDMAT) {
        spSlerp->endMat = NULL;
    } else {
        spSlerp->endMat = RwMatrixCreate();
    }

    return spSlerp;
} // 0x802F1F44

/*
 * ==================== RtSlerpInitialize ====================
 * Takes (or copies) the two matrices, then finds the rotation between them: the end matrix
 * relative to the start one, as an axis and an angle.
 */
RtSlerp* RtSlerpInitialize(RtSlerp* spSlerp, RwMatrix* startMat, RwMatrix* endMat) {
    RwV3d vCentre;
    RwMatrix mScratch;

    RwMatrixSetIdentity(&mScratch);

    if (spSlerp->matRefMask & rtSLERPREFSTARTMAT) {
        spSlerp->startMat = startMat;
    } else {
        RwMatrixCopy(spSlerp->startMat, startMat);
    }

    if (spSlerp->matRefMask & rtSLERPREFENDMAT) {
        spSlerp->endMat = endMat;
    } else {
        RwMatrixCopy(spSlerp->endMat, endMat);
    }

    RwMatrixInvert(&mScratch, spSlerp->startMat);
    RwMatrixTransform(&mScratch, spSlerp->endMat, rwCOMBINEPOSTCONCAT);
    RwMatrixQueryRotate(&mScratch, &spSlerp->axis, &spSlerp->angle, &vCentre);
    spSlerp->useLerp = spSlerp->angle < rtSLERPMINANGLE;

    return spSlerp;
} // 0x802F2010

/*
 * ==================== RtSlerpDestroy ====================
 * Destroys the matrices the slerp owns, then the slerp.
 */
void RtSlerpDestroy(RtSlerp* spSlerp) {
    if (!(spSlerp->matRefMask & rtSLERPREFSTARTMAT)) {
        RwMatrixDestroy(spSlerp->startMat);
    }

    if (!(spSlerp->matRefMask & rtSLERPREFENDMAT)) {
        RwMatrixDestroy(spSlerp->endMat);
    }

    RwFree(spSlerp);
} // 0x802F21FC

/*
 * ==================== RtSlerpGetMatrix ====================
 * Sets mpResultMat to the interpolation nDelta of the way from the start to the end matrix: the
 * start matrix at or below 0, the end matrix at or above 1. A small rotation blends the two
 * matrices and renormalises the result; a larger one rotates the start matrix about the axis by
 * the scaled angle and blends the positions.
 */
RwMatrix* RtSlerpGetMatrix(RtSlerp* spSlerp, RwMatrix* mpResultMat, RwReal nDelta) {
    if (nDelta <= 0.0f) {
        if (mpResultMat != spSlerp->startMat) {
            RwMatrixCopy(mpResultMat, spSlerp->startMat);
        }
        return mpResultMat;
    }

    if (nDelta >= 1.0f) {
        if (mpResultMat != spSlerp->endMat) {
            RwMatrixCopy(mpResultMat, spSlerp->endMat);
        }
        return mpResultMat;
    }

    if (spSlerp->useLerp) {
        RwMatrix mLerpMat;

        RwMatrixSetIdentity(&mLerpMat);
        RwV3dSubMacro(&mLerpMat.right, &spSlerp->endMat->right, &spSlerp->startMat->right);
        RwV3dSubMacro(&mLerpMat.up, &spSlerp->endMat->up, &spSlerp->startMat->up);
        RwV3dSubMacro(&mLerpMat.at, &spSlerp->endMat->at, &spSlerp->startMat->at);
        RwV3dSubMacro(&mLerpMat.pos, &spSlerp->endMat->pos, &spSlerp->startMat->pos);
        RwV3dScaleMacro(&mLerpMat.right, &mLerpMat.right, nDelta);
        RwV3dScaleMacro(&mLerpMat.up, &mLerpMat.up, nDelta);
        RwV3dScaleMacro(&mLerpMat.at, &mLerpMat.at, nDelta);
        RwV3dScaleMacro(&mLerpMat.pos, &mLerpMat.pos, nDelta);

        RwV3dAddMacro(&mpResultMat->right, &spSlerp->startMat->right, &mLerpMat.right);
        RwV3dAddMacro(&mpResultMat->up, &spSlerp->startMat->up, &mLerpMat.up);
        RwV3dAddMacro(&mpResultMat->at, &spSlerp->startMat->at, &mLerpMat.at);
        RwV3dAddMacro(&mpResultMat->pos, &spSlerp->startMat->pos, &mLerpMat.pos);

        RwV3dNormalize(&mpResultMat->right, &mpResultMat->right);
        RwV3dNormalize(&mpResultMat->up, &mpResultMat->up);
        RwV3dNormalize(&mpResultMat->at, &mpResultMat->at);
    } else {
        RwV3d vStartPos;
        RwV3d vEndPos;

        vStartPos = spSlerp->startMat->pos;
        vEndPos = spSlerp->endMat->pos;

        RwMatrixCopy(mpResultMat, spSlerp->startMat);
        mpResultMat->pos.x = 0.0f;
        mpResultMat->pos.y = 0.0f;
        mpResultMat->pos.z = 0.0f;

        RwMatrixRotate(mpResultMat, &spSlerp->axis, spSlerp->angle * nDelta, rwCOMBINEPOSTCONCAT);

        RwV3dSubMacro(&mpResultMat->pos, &vEndPos, &vStartPos);
        RwV3dScaleMacro(&mpResultMat->pos, &mpResultMat->pos, nDelta);
        RwV3dAddMacro(&mpResultMat->pos, &mpResultMat->pos, &vStartPos);
    }

    return mpResultMat;
} // 0x802F2260
