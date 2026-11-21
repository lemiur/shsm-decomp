// Reference: tgsm/pbg src/rwsdk/src/bacamera.c
// Unit name and boundary: Gormiti link map (RwCore.a bacamera)
// Locals of the frustum and view-matrix builders: Gormiti DWARF.
/*
 * bacamera.c: RenderWare cameras. A camera hangs from a frame; whenever the frame moves the
 * camera's sync function rebuilds its view matrix and its view frustum (planes, corners and
 * bounding box) for its projection. The camera module's free list hands out the cameras.
 * Parked: retail inlines no call here (needs a per-object -inline noauto, docs/exceptions.md).
 */
#include <rwsdk/bacamera.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/baraster.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plcore/bavector.h>

/*
 * The side of a box closest to a frustum plane along one axis: 1 where the plane normal's
 * component is positive, 0 where it is negative, read from the real's sign bit. Name invented.
 */
#define RwRealClosestSide(_r) ((*(const RwInt32*)&(_r) >> 31) + 1)

/* The free list of cameras; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rwCameraFreeList;

/* The plugins registered with cameras: no plugin data yet, so a camera is just an RwCamera. */
RwPluginRegistry cameraTKList = {
    sizeof(RwCamera),
    sizeof(RwCamera),
    0,
    0,
    NULL,
    NULL,
};

/* The camera module's registration (offset of its globals in the engine instance). */
static RwModuleInfo cameraModule;

/* Cameras per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rwCameraFreeListBlockSize = 4;
static RwInt32 _rwCameraFreeListPreallocBlocks = 1;

/* The camera module's globals. RWCAMERAGLOBAL invented. */
typedef struct rwCameraGlobals {
    RwFreeList* cameraFreeList; /* 0x0 */
} rwCameraGlobals;

#define RWCAMERAGLOBAL(var) \
    (RWPLUGINOFFSET(rwCameraGlobals, RwEngineInstance, cameraModule.globalsOffset)->var)
/*
 * Sets the camera's z scale and shift, which map a depth between the clip planes (its reciprocal,
 * in perspective) onto the device's z-buffer range, kept a little inside both ends.
 * Locals: Gormiti DWARF.
 */
static RwCamera* CameraUpdateZShiftScale(RwCamera* camera) {
    RwReal zOutInf;
    RwReal zOutSup;
    RwReal zInInf;
    RwReal zInSup;

    zOutInf = RwIm2DGetNearScreenZ();
    zOutSup = RwIm2DGetFarScreenZ();

    switch (camera->projectionType) {
        case rwPARALLEL:
            zInSup = camera->farPlane;
            zInInf = camera->nearPlane;
            break;
        case rwPERSPECTIVE:
        default:
            zInSup = 1.0f / camera->farPlane;
            zInInf = 1.0f / camera->nearPlane;
            break;
    }

    {
        RwReal zScale;
        RwReal zShift;
        RwReal zOutWid;
        RwReal zDelta;

        zOutWid = zOutSup - zOutInf;
        zDelta = 0.0001f * zOutWid;
        zOutSup -= zDelta;
        zOutInf += zDelta;
        zOutWid = zOutSup - zOutInf;

        zScale = zOutWid / (zInSup - zInInf);
        zShift = 0.5f * ((zOutSup + zOutInf) - zScale * (zInSup + zInInf));

        camera->zScale = zScale;
        camera->zShift = zShift;
    }

    return camera;
} // 0x80291CC0

/*
 * Builds the perspective view frustum: its eight corners in world space (the view window's corners
 * on the near and far clip planes, as seen from the eye shifted by the view offset) and its six
 * planes (far, near, then the four sides), each with its closest-corner indices.
 * Locals and statement order: Gormiti DWARF and Debug build.
 */
static void CameraBuildPerspClipPlanes(RwCamera* camera) {
    RwFrame* cameraFrame = RwCameraGetFrame(camera);
    const RwMatrix* cameraLTM = &cameraFrame->ltm;
    RwMatrix* frameMatrix = RwFrameGetMatrix(cameraFrame);
    RwV3d vTmp, vTmp2, vRight, vUp, vCOP;
    RwReal recip;
    RwReal scale;
    RwInt32 i;
    RwV3d* frustumVerts = camera->frustumCorners;
    RwFrustumPlane* frustumPlanes = camera->frustumPlanes;

    /* The directions through the view window's corners, and the eye's offset. */
    scale = -camera->viewOffset.x;
    RwV3dScaleMacro(&vCOP, &cameraLTM->right, scale);
    RwV3dIncrementScaledMacro(&vCOP, &cameraLTM->up, camera->viewOffset.y);
    RwV3dScaleMacro(&vRight, &cameraLTM->right, camera->viewWindow.x);

    RwV3dAddMacro(&vTmp, &cameraLTM->at, &vRight);
    RwV3dScaleMacro(&vUp, &cameraLTM->up, camera->viewWindow.y);
    RwV3dAddMacro(&vTmp, &vTmp, &vUp);
    RwV3dScaleMacro(&vRight, &vRight, 2.0f);
    RwV3dScaleMacro(&vUp, &vUp, 2.0f);
    frustumVerts[0] = vTmp;
    RwV3dSubMacro(&vTmp, &vTmp, &vRight);
    frustumVerts[1] = vTmp;
    RwV3dSubMacro(&vTmp, &vTmp, &vUp);
    frustumVerts[2] = vTmp;
    RwV3dAddMacro(&vTmp, &vTmp, &vRight);
    frustumVerts[3] = vTmp;

    /* Out from the shifted eye to the near and far clip planes. */
    for (i = 0; i < 4; i++) {
        RwV3d* target = &frustumVerts[i];

        RwV3dSubMacro(&vTmp, target, &vCOP);
        RwV3dAddMacro(target, &vCOP, &cameraLTM->pos);
        RwV3dIncrementScaledMacro(target, &vTmp, camera->nearPlane);
        target = &frustumVerts[i + 4];
        RwV3dAddMacro(target, &vCOP, &cameraLTM->pos);
        RwV3dIncrementScaledMacro(target, &vTmp, camera->farPlane);
    }

    /* The far plane faces along the camera's at. */
    if (rwMatrixGetFlags(frameMatrix) & rwMATRIXTYPENORMAL) {
        frustumPlanes[0].plane.normal = cameraLTM->at;
    } else {
        RwV3dSubMacro(&vTmp, &frustumVerts[5], &frustumVerts[4]);
        RwV3dSubMacro(&vTmp2, &frustumVerts[7], &frustumVerts[4]);
        RwV3dCrossProductMacro(&frustumPlanes[0].plane.normal, &vTmp, &vTmp2);
        {
            RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[0].plane.normal, &frustumPlanes[0].plane.normal);

            recip = _rwInvSqrt(length2);
            RwV3dScaleMacro(&frustumPlanes[0].plane.normal, &frustumPlanes[0].plane.normal, recip);
        }
    }
    frustumPlanes[0].plane.distance = RwV3dDotProductMacro(&frustumVerts[4], &cameraLTM->at);
    frustumPlanes[0].closestX = RwRealClosestSide(frustumPlanes[0].plane.normal.x);
    frustumPlanes[0].closestY = RwRealClosestSide(frustumPlanes[0].plane.normal.y);
    frustumPlanes[0].closestZ = RwRealClosestSide(frustumPlanes[0].plane.normal.z);

    /* The near plane faces the other way. */
    RwV3dNegateMacro(&frustumPlanes[1].plane.normal, &frustumPlanes[0].plane.normal);
    frustumPlanes[1].plane.distance = RwV3dDotProductMacro(&frustumVerts[0], &frustumPlanes[1].plane.normal);
    frustumPlanes[1].closestX = RwRealClosestSide(frustumPlanes[1].plane.normal.x);
    frustumPlanes[1].closestY = RwRealClosestSide(frustumPlanes[1].plane.normal.y);
    frustumPlanes[1].closestZ = RwRealClosestSide(frustumPlanes[1].plane.normal.z);

    RwV3dSubMacro(&vTmp, &frustumVerts[1], &frustumVerts[5]);
    RwV3dSubMacro(&vTmp2, &frustumVerts[6], &frustumVerts[5]);
    RwV3dCrossProductMacro(&frustumPlanes[2].plane.normal, &vTmp, &vTmp2);
    {
        RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[2].plane.normal, &frustumPlanes[2].plane.normal);

        recip = _rwInvSqrt(length2);
        RwV3dScaleMacro(&frustumPlanes[2].plane.normal, &frustumPlanes[2].plane.normal, recip);
    }
    frustumPlanes[2].plane.distance = RwV3dDotProductMacro(&frustumVerts[1], &frustumPlanes[2].plane.normal);
    frustumPlanes[2].closestX = RwRealClosestSide(frustumPlanes[2].plane.normal.x);
    frustumPlanes[2].closestY = RwRealClosestSide(frustumPlanes[2].plane.normal.y);
    frustumPlanes[2].closestZ = RwRealClosestSide(frustumPlanes[2].plane.normal.z);

    RwV3dSubMacro(&vTmp2, &frustumVerts[4], &frustumVerts[5]);
    RwV3dCrossProductMacro(&frustumPlanes[3].plane.normal, &vTmp2, &vTmp);
    {
        RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[3].plane.normal, &frustumPlanes[3].plane.normal);

        recip = _rwInvSqrt(length2);
        RwV3dScaleMacro(&frustumPlanes[3].plane.normal, &frustumPlanes[3].plane.normal, recip);
    }
    frustumPlanes[3].plane.distance = RwV3dDotProductMacro(&frustumVerts[1], &frustumPlanes[3].plane.normal);
    frustumPlanes[3].closestX = RwRealClosestSide(frustumPlanes[3].plane.normal.x);
    frustumPlanes[3].closestY = RwRealClosestSide(frustumPlanes[3].plane.normal.y);
    frustumPlanes[3].closestZ = RwRealClosestSide(frustumPlanes[3].plane.normal.z);

    RwV3dSubMacro(&vTmp, &frustumVerts[3], &frustumVerts[7]);
    RwV3dSubMacro(&vTmp2, &frustumVerts[4], &frustumVerts[7]);
    RwV3dCrossProductMacro(&frustumPlanes[4].plane.normal, &vTmp, &vTmp2);
    {
        RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[4].plane.normal, &frustumPlanes[4].plane.normal);

        recip = _rwInvSqrt(length2);
        RwV3dScaleMacro(&frustumPlanes[4].plane.normal, &frustumPlanes[4].plane.normal, recip);
    }
    frustumPlanes[4].plane.distance = RwV3dDotProductMacro(&frustumVerts[3], &frustumPlanes[4].plane.normal);
    frustumPlanes[4].closestX = RwRealClosestSide(frustumPlanes[4].plane.normal.x);
    frustumPlanes[4].closestY = RwRealClosestSide(frustumPlanes[4].plane.normal.y);
    frustumPlanes[4].closestZ = RwRealClosestSide(frustumPlanes[4].plane.normal.z);

    RwV3dSubMacro(&vTmp2, &frustumVerts[6], &frustumVerts[7]);
    RwV3dCrossProductMacro(&frustumPlanes[5].plane.normal, &vTmp2, &vTmp);
    {
        RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[5].plane.normal, &frustumPlanes[5].plane.normal);

        recip = _rwInvSqrt(length2);
        RwV3dScaleMacro(&frustumPlanes[5].plane.normal, &frustumPlanes[5].plane.normal, recip);
    }
    frustumPlanes[5].plane.distance = RwV3dDotProductMacro(&frustumVerts[3], &frustumPlanes[5].plane.normal);
    frustumPlanes[5].closestX = RwRealClosestSide(frustumPlanes[5].plane.normal.x);
    frustumPlanes[5].closestY = RwRealClosestSide(frustumPlanes[5].plane.normal.y);
    frustumPlanes[5].closestZ = RwRealClosestSide(frustumPlanes[5].plane.normal.z);
} // 0x80291D44

/*
 * Builds the perspective view matrix: world space to the camera's view window, x and y scaled by
 * the reciprocal view window and shifted by the view offset, z the depth along the camera's at.
 * Locals: Gormiti DWARF (scale holds the window scale, then the shift, in each half).
 */
static RwCamera* CameraBuildPerspViewMatrix(RwCamera* camera) {
    RwReal scale;
    RwV3d vVector;
    const RwMatrix* cameraLTM = &RwCameraGetFrame(camera)->ltm;
    RwMatrix* viewMatrix = &camera->viewMatrix;

    scale = -0.5f * camera->recipViewWindow.x;
    RwV3dScaleMacro(&vVector, &cameraLTM->right, scale);
    scale = 0.5f - scale * camera->viewOffset.x;
    RwV3dIncrementScaledMacro(&vVector, &cameraLTM->at, scale);
    viewMatrix->right.x = vVector.x;
    viewMatrix->up.x = vVector.y;
    viewMatrix->at.x = vVector.z;
    viewMatrix->pos.x = 0.5f - (scale + RwV3dDotProductMacro(&cameraLTM->pos, &vVector));

    scale = -0.5f * camera->recipViewWindow.y;
    RwV3dScaleMacro(&vVector, &cameraLTM->up, scale);
    scale = 0.5f + scale * camera->viewOffset.y;
    RwV3dIncrementScaledMacro(&vVector, &cameraLTM->at, scale);
    viewMatrix->right.y = vVector.x;
    viewMatrix->up.y = vVector.y;
    viewMatrix->at.y = vVector.z;
    viewMatrix->pos.y = 0.5f - (scale + RwV3dDotProductMacro(&cameraLTM->pos, &vVector));

    viewMatrix->right.z = cameraLTM->at.x;
    viewMatrix->up.z = cameraLTM->at.y;
    viewMatrix->at.z = cameraLTM->at.z;
    viewMatrix->pos.z = -RwV3dDotProductMacro(&cameraLTM->pos, &cameraLTM->at);

    RwMatrixOptimize(viewMatrix, NULL);

    return camera;
} // 0x8029266C

/*
 * Builds the parallel view frustum: its eight corners, the view window at the near and far
 * planes sheared by the view offset, in world space, then its six planes from the corners.
 */
static void CameraBuildParallelClipPlanes(RwCamera* camera) {
    RwReal width, height;
    RwReal nearPlane, farPlane;
    RwReal offsetx, offsety;
    RwReal offset;
    RwV3d vTmp, vTmp2;
    RwReal recip;
    RwV3d* frustumVerts = camera->frustumCorners;
    const RwMatrix* cameraLTM = &RwCameraGetFrame(camera)->ltm;
    RwFrustumPlane* frustumPlanes = camera->frustumPlanes;

    width = camera->viewWindow.x;
    height = camera->viewWindow.y;
    nearPlane = camera->nearPlane;
    farPlane = camera->farPlane;
    offsetx = -camera->viewOffset.x;
    offsety = camera->viewOffset.y;

    frustumVerts[0].z = frustumVerts[1].z = frustumVerts[2].z = frustumVerts[3].z = nearPlane;
    frustumVerts[4].z = frustumVerts[5].z = frustumVerts[6].z = frustumVerts[7].z = farPlane;

    offset = (1.0f - nearPlane) * offsetx;
    frustumVerts[0].x = frustumVerts[3].x = width + offset;
    frustumVerts[1].x = frustumVerts[2].x = -width + offset;
    offset = (1.0f - farPlane) * offsetx;
    frustumVerts[4].x = frustumVerts[7].x = width + offset;
    frustumVerts[5].x = frustumVerts[6].x = -width + offset;
    offset = (1.0f - nearPlane) * offsety;
    frustumVerts[0].y = frustumVerts[1].y = height + offset;
    frustumVerts[2].y = frustumVerts[3].y = -height + offset;
    offset = (1.0f - farPlane) * offsety;
    frustumVerts[4].y = frustumVerts[5].y = height + offset;
    frustumVerts[6].y = frustumVerts[7].y = -height + offset;

    RwV3dTransformPoints(frustumVerts, frustumVerts, 8, cameraLTM);

    frustumPlanes[0].plane.normal = cameraLTM->at;
    frustumPlanes[0].plane.distance = RwV3dDotProductMacro(&frustumVerts[4], &frustumPlanes[0].plane.normal);
    frustumPlanes[0].closestX = RwRealClosestSide(frustumPlanes[0].plane.normal.x);
    frustumPlanes[0].closestY = RwRealClosestSide(frustumPlanes[0].plane.normal.y);
    frustumPlanes[0].closestZ = RwRealClosestSide(frustumPlanes[0].plane.normal.z);

    RwV3dNegateMacro(&frustumPlanes[1].plane.normal, &frustumPlanes[0].plane.normal);
    frustumPlanes[1].plane.distance = RwV3dDotProductMacro(&frustumVerts[0], &frustumPlanes[1].plane.normal);
    frustumPlanes[1].closestX = RwRealClosestSide(frustumPlanes[1].plane.normal.x);
    frustumPlanes[1].closestY = RwRealClosestSide(frustumPlanes[1].plane.normal.y);
    frustumPlanes[1].closestZ = RwRealClosestSide(frustumPlanes[1].plane.normal.z);

    RwV3dSubMacro(&vTmp, &frustumVerts[1], &frustumVerts[5]);
    RwV3dSubMacro(&vTmp2, &frustumVerts[6], &frustumVerts[5]);
    RwV3dCrossProductMacro(&frustumPlanes[2].plane.normal, &vTmp, &vTmp2);
    {
        RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[2].plane.normal, &frustumPlanes[2].plane.normal);

        recip = _rwInvSqrt(length2);
        RwV3dScaleMacro(&frustumPlanes[2].plane.normal, &frustumPlanes[2].plane.normal, recip);
    }
    frustumPlanes[2].plane.distance = RwV3dDotProductMacro(&frustumVerts[1], &frustumPlanes[2].plane.normal);
    frustumPlanes[2].closestX = RwRealClosestSide(frustumPlanes[2].plane.normal.x);
    frustumPlanes[2].closestY = RwRealClosestSide(frustumPlanes[2].plane.normal.y);
    frustumPlanes[2].closestZ = RwRealClosestSide(frustumPlanes[2].plane.normal.z);

    RwV3dSubMacro(&vTmp2, &frustumVerts[4], &frustumVerts[5]);
    RwV3dCrossProductMacro(&frustumPlanes[3].plane.normal, &vTmp2, &vTmp);
    {
        RwReal length2 = RwV3dDotProductMacro(&frustumPlanes[3].plane.normal, &frustumPlanes[3].plane.normal);

        recip = _rwInvSqrt(length2);
        RwV3dScaleMacro(&frustumPlanes[3].plane.normal, &frustumPlanes[3].plane.normal, recip);
    }
    frustumPlanes[3].plane.distance = RwV3dDotProductMacro(&frustumVerts[1], &frustumPlanes[3].plane.normal);
    frustumPlanes[3].closestX = RwRealClosestSide(frustumPlanes[3].plane.normal.x);
    frustumPlanes[3].closestY = RwRealClosestSide(frustumPlanes[3].plane.normal.y);
    frustumPlanes[3].closestZ = RwRealClosestSide(frustumPlanes[3].plane.normal.z);

    RwV3dNegateMacro(&frustumPlanes[4].plane.normal, &frustumPlanes[2].plane.normal);
    frustumPlanes[4].plane.distance = RwV3dDotProductMacro(&frustumVerts[3], &frustumPlanes[4].plane.normal);
    frustumPlanes[4].closestX = RwRealClosestSide(frustumPlanes[4].plane.normal.x);
    frustumPlanes[4].closestY = RwRealClosestSide(frustumPlanes[4].plane.normal.y);
    frustumPlanes[4].closestZ = RwRealClosestSide(frustumPlanes[4].plane.normal.z);

    RwV3dNegateMacro(&frustumPlanes[5].plane.normal, &frustumPlanes[3].plane.normal);
    frustumPlanes[5].plane.distance = RwV3dDotProductMacro(&frustumVerts[3], &frustumPlanes[5].plane.normal);
    frustumPlanes[5].closestX = RwRealClosestSide(frustumPlanes[5].plane.normal.x);
    frustumPlanes[5].closestY = RwRealClosestSide(frustumPlanes[5].plane.normal.y);
    frustumPlanes[5].closestZ = RwRealClosestSide(frustumPlanes[5].plane.normal.z);
} // 0x8029281C

/*
 * Builds the parallel view matrix, as the perspective one without the shift by the depth.
 * Locals: Gormiti DWARF.
 */
static RwCamera* CameraBuildParallelViewMatrix(RwCamera* camera) {
    RwReal scale;
    RwV3d vVector;
    const RwMatrix* cameraLTM = &RwCameraGetFrame(camera)->ltm;
    RwMatrix* viewMatrix = &camera->viewMatrix;

    scale = -0.5f * camera->recipViewWindow.x;
    RwV3dScaleMacro(&vVector, &cameraLTM->right, scale);
    scale = -(scale * camera->viewOffset.x);
    RwV3dIncrementScaledMacro(&vVector, &cameraLTM->at, scale);
    viewMatrix->right.x = vVector.x;
    viewMatrix->up.x = vVector.y;
    viewMatrix->at.x = vVector.z;
    viewMatrix->pos.x = 0.5f - (scale + RwV3dDotProductMacro(&cameraLTM->pos, &vVector));

    scale = -0.5f * camera->recipViewWindow.y;
    RwV3dScaleMacro(&vVector, &cameraLTM->up, scale);
    scale = scale * camera->viewOffset.y;
    RwV3dIncrementScaledMacro(&vVector, &cameraLTM->at, scale);
    viewMatrix->right.y = vVector.x;
    viewMatrix->up.y = vVector.y;
    viewMatrix->at.y = vVector.z;
    viewMatrix->pos.y = 0.5f - (scale + RwV3dDotProductMacro(&cameraLTM->pos, &vVector));

    viewMatrix->right.z = cameraLTM->at.x;
    viewMatrix->up.z = cameraLTM->at.y;
    viewMatrix->at.z = cameraLTM->at.z;
    viewMatrix->pos.z = -RwV3dDotProductMacro(&cameraLTM->pos, &cameraLTM->at);

    RwMatrixOptimize(viewMatrix, NULL);

    return camera;
} // 0x80292D98

/* The camera's frame has moved: rebuilds its view matrix and frustum for its projection. */
static RwObjectHasFrame* CameraSync(RwObjectHasFrame* object) {
    RwCamera* camera = (RwCamera*)object;

    if (camera->projectionType == rwPERSPECTIVE) {
        CameraBuildPerspViewMatrix(camera);
        CameraBuildPerspClipPlanes(camera);
    } else {
        CameraBuildParallelViewMatrix(camera);
        CameraBuildParallelClipPlanes(camera);
    }

    RwBBoxCalculate(&camera->frustumBoundBox, camera->frustumCorners, 8);

    return object;
} // 0x80292F44

/* Ends rendering to the camera (the device's end update); no camera is current afterwards. */
static RwCamera* CameraEndUpdate(RwCamera* camera) {
    if (RWSRCGLOBAL(stdFunc)[rwSTANDARDCAMERAENDUPDATE](NULL, camera, 0)) {
        RWSRCGLOBAL(curCamera) = NULL;
        return camera;
    }

    return NULL;
} // 0x80292FA8

/* Starts rendering to the camera: makes it current, synchronises dirty frames, begins the device's update. */
static RwCamera* CameraBeginUpdate(RwCamera* camera) {
    RWSRCGLOBAL(curCamera) = camera;
    _rwFrameSyncDirty();

    if (RWSRCGLOBAL(stdFunc)[rwSTANDARDCAMERABEGINUPDATE](NULL, camera, 0)) {
        _rwPipeInitForCamera(camera);
        return camera;
    }

    return NULL;
} // 0x8029300C

/* Closes the camera module: destroys the camera free list. */
void* _rwCameraClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWCAMERAGLOBAL(cameraFreeList) != NULL) {
        RwFreeListDestroy(RWCAMERAGLOBAL(cameraFreeList));
        RWCAMERAGLOBAL(cameraFreeList) = NULL;
    }

    cameraModule.numInstances--;

    return instance;
} // 0x80293078

/* Opens the camera module: creates the camera free list. */
void* _rwCameraOpen(void* instance, RwInt32 offset, RwInt32 size) {
    cameraModule.globalsOffset = offset;

    RWCAMERAGLOBAL(cameraFreeList) = RwFreeListCreateAndPreallocateSpace(
        cameraTKList.sizeOfStruct, _rwCameraFreeListBlockSize, sizeof(RwUInt32),
        _rwCameraFreeListPreallocBlocks, &_rwCameraFreeList, rwMEMHINTDUR_GLOBAL | rwID_CAMERA);
    if (RWCAMERAGLOBAL(cameraFreeList) == NULL) {
        return NULL;
    }

    cameraModule.numInstances++;

    return instance;
} // 0x802930DC

/* Ends rendering to the camera. */
RwCamera* RwCameraEndUpdate(RwCamera* camera) {
    return camera->endUpdate(camera);
} // 0x8029316C

/* Starts rendering to the camera. */
RwCamera* RwCameraBeginUpdate(RwCamera* camera) {
    return camera->beginUpdate(camera);
} // 0x80293178

/* Sets the camera's view offset. */
RwCamera* RwCameraSetViewOffset(RwCamera* camera, const RwV2d* offset) {
    RwFrame* frame;

    camera->viewOffset = *offset;

    frame = RwCameraGetFrame(camera);
    if (frame != NULL) {
        RwFrameUpdateObjects(frame);
    }

    return camera;
} // 0x80293184

/* Sets the distance of the camera's near clip plane. */
RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip) {
    RwFrame* frame;

    camera->nearPlane = nearClip;
    CameraUpdateZShiftScale(camera);

    frame = RwCameraGetFrame(camera);
    if (frame != NULL) {
        RwFrameUpdateObjects(frame);
    }

    return camera;
} // 0x802931D4

/* Sets the distance of the camera's far clip plane. */
RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip) {
    RwFrame* frame;

    camera->farPlane = farClip;
    CameraUpdateZShiftScale(camera);

    frame = RwCameraGetFrame(camera);
    if (frame != NULL) {
        RwFrameUpdateObjects(frame);
    }

    return camera;
} // 0x80293218

/*
 * The frustum test's statistics, and the size below which a sphere well in front of the near
 * plane counts as too small to see (zero: no sphere is culled for its size). Names invented.
 */
static RwInt32 numSpheresTooSmall;
static RwInt32 numSpheresOutside;
static RwInt32 numSpheresVisible;
static RwReal minSphereSize;

/* Tests a sphere against the camera's view frustum. */
RwFrustumTestResult RwCameraFrustumTestSphere(const RwCamera* camera, const RwSphere* sphere) {
    RwReal distances[6];
    const RwFrustumPlane* frustumPlane = camera->frustumPlanes;
    RwFrustumTestResult result = rwSPHEREINSIDE;
    RwInt32 i;
    RwReal* planeDistance = distances;

    i = 6;
    while (i--) {
        RwReal distance = RwV3dDotProductMacro(&sphere->center, &frustumPlane->plane.normal);

        distance -= frustumPlane->plane.distance;

        if (distance > sphere->radius) {
            numSpheresOutside++;
            return rwSPHEREOUTSIDE;
        }

        if (distance > -sphere->radius) {
            result = rwSPHEREBOUNDARY;
        }

        *planeDistance++ = distance;
        frustumPlane++;
    }

    if (distances[1] < -1.0f && sphere->radius / -distances[1] < minSphereSize) {
        numSpheresTooSmall++;
        return rwSPHEREOUTSIDE;
    }

    numSpheresVisible++;

    return result;
} // 0x8029325C

/* Clears the camera's raster and/or z-buffer. */
RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode) {
    if (RWSRCGLOBAL(stdFunc)[rwSTANDARDCAMERACLEAR](camera, colour, clearMode)) {
        return camera;
    }

    return NULL;
} // 0x802933A8

/* Shows the camera's raster on the device. */
RwCamera* RwCameraShowRaster(RwCamera* camera, void* pDev, RwUInt32 flags) {
    if (RwRasterShowRaster(camera->frameBuffer, pDev, flags)) {
        return camera;
    }

    return NULL;
} // 0x802933F4

/* Sets the camera's projection. */
RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection) {
    RwFrame* frame;

    switch (projection) {
        case rwPERSPECTIVE:
        case rwPARALLEL:
            camera->projectionType = projection;

            frame = RwCameraGetFrame(camera);
            if (frame != NULL) {
                RwFrameUpdateObjects(frame);
            }

            CameraUpdateZShiftScale(camera);
            return camera;
        default:
            RWERROR((E_RW_BADPARAM, "Invalid projection type specified"));
            return NULL;
    }
} // 0x80293438

/* Sets the camera's view window (and its reciprocal). */
RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow) {
    RwFrame* frame;

    camera->viewWindow = *viewWindow;
    camera->recipViewWindow.x = 1.0f / camera->viewWindow.x;
    camera->recipViewWindow.y = 1.0f / camera->viewWindow.y;

    frame = RwCameraGetFrame(camera);
    if (frame != NULL) {
        RwFrameUpdateObjects(frame);
    }

    return camera;
} // 0x802934CC

/* Registers a plugin with cameras. */
RwInt32 RwCameraRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                               RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&cameraTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x80293538

/* Destroys a camera: its plugin data, its link to its frame, the camera. */
RwBool RwCameraDestroy(RwCamera* camera) {
    _rwPluginRegistryDeInitObject(&cameraTKList, camera);
    rwObjectHasFrameReleaseFrame(camera);
    RwFreeListFree(RWCAMERAGLOBAL(cameraFreeList), camera);

    return TRUE;
} // 0x80293568

/* Creates a perspective camera with a unit view window and default clip planes. */
RwCamera* RwCameraCreate(void) {
    RwCamera* camera;

    camera = (RwCamera*)RwFreeListAlloc(RWCAMERAGLOBAL(cameraFreeList), rwMEMHINTDUR_EVENT | rwID_CAMERA);
    if (camera == NULL) {
        return NULL;
    }

    rwObjectInitialize(camera, rwCAMERA, 0);
    camera->object.sync = CameraSync;
    camera->beginUpdate = CameraBeginUpdate;
    camera->endUpdate = CameraEndUpdate;
    camera->viewWindow.x = camera->viewWindow.y = 1.0f;
    camera->recipViewWindow.x = camera->recipViewWindow.y = 1.0f;
    camera->viewOffset.x = camera->viewOffset.y = 0.0f;
    camera->nearPlane = 0.05f;
    camera->farPlane = 10.0f;
    camera->fogPlane = 5.0f;
    camera->frameBuffer = NULL;
    camera->zBuffer = NULL;
    camera->projectionType = rwPERSPECTIVE;
    CameraUpdateZShiftScale(camera);
    rwMatrixSetFlags(&camera->viewMatrix, 0);
    _rwPluginRegistryInitObject(&cameraTKList, camera);

    return camera;
} // 0x802935C8
/*
 * RwCameraClone: a new camera on the same frame, with the same callbacks, rasters, view window,
 * view offset, clip and fog planes and projection. Its z scale and shift are rebuilt from the
 * copied projection, and its plugin data is copied from the source.
 * Register/schedule wall: retail keeps the parameter in r31 and the clone in r30, and copies the
 * parameter in before the r30 save; this compile gives the clone r31 and copies the parameter after.
 */
RwCamera* RwCameraClone(RwCamera* camera) {
    RwCamera* clone;

    clone = RwCameraCreate();
    if (clone == NULL) {
        return NULL;
    }
    RwCameraSetFrame(clone, RwCameraGetFrame(camera));
    clone->beginUpdate = camera->beginUpdate;
    clone->endUpdate = camera->endUpdate;
    clone->viewWindow.x = camera->viewWindow.x;
    clone->viewWindow.y = camera->viewWindow.y;
    clone->recipViewWindow.x = camera->recipViewWindow.x;
    clone->recipViewWindow.y = camera->recipViewWindow.y;
    clone->viewOffset.x = camera->viewOffset.x;
    clone->viewOffset.y = camera->viewOffset.y;
    clone->nearPlane = camera->nearPlane;
    clone->farPlane = camera->farPlane;
    clone->fogPlane = camera->fogPlane;
    clone->frameBuffer = camera->frameBuffer;
    clone->zBuffer = camera->zBuffer;
    clone->projectionType = camera->projectionType;
    CameraUpdateZShiftScale(clone);
    _rwPluginRegistryCopyObject(&cameraTKList, clone, camera);

    return clone;
} // 0x802936CC
