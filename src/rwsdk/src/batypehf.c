// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h (declarations)
/*
 * batypehf.c: objects attached to a frame (RwObjectHasFrame: cameras, lights, atomics). An
 * attached object's parent is its frame, and its lFrame link sits in the frame's objectList.
 */
#include <rwsdk/baframe.h>
#include <rwsdk/batypehf.h>

/*
 * Attaches object to frame, first releasing it from the frame it hangs from; a NULL frame leaves
 * it detached. The frame's objects are brought up to date with the frame.
 */
void _rwObjectHasFrameSetFrame(void* object, RwFrame* frame) {
    RwObjectHasFrame* ohf = (RwObjectHasFrame*)object;

    if (rwObjectGetParent(object) != NULL) {
        rwLinkListRemoveLLLink(&ohf->lFrame);
    }

    rwObjectSetParent(object, frame);

    if (frame != NULL) {
        rwLinkListAddLLLink(&frame->objectList, &ohf->lFrame);
        RwFrameUpdateObjects(frame);
    }
} // 0x8029A814

/* Releases object from the frame it hangs from, if any. */
void _rwObjectHasFrameReleaseFrame(void* object) {
    RwObjectHasFrame* ohf = (RwObjectHasFrame*)object;

    if (rwObjectGetParent(object) != NULL) {
        rwLinkListRemoveLLLink(&ohf->lFrame);
    }
} // 0x8029A870
