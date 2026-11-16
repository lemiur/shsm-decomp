// Reference: tgsm/pbg config/GPLE9G/symbols.txt (basync.c: FrameSyncHierarchyRecurse, FrameSyncHierarchyRecurseNoLTM, FrameSyncHierarchy, _rwFrameSyncDirty, FrameSyncHierarchyLTMRecurse, _rwFrameSyncHierarchyLTM)
/*
 * basync.c: frame synchronisation: brings the hierarchies in the dirty frame list up to date,
 * rebuilding the LTMs of the frames that moved and calling the sync callback of every object
 * attached to them.
 */
#include <rwsdk/baframe.h>
#include <rwsdk/badevice.h>
#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>

/* Calls the sync callback of every object attached to a frame (a statement macro). Name invented. */
#define rwFrameSyncObjects(_frame)                                                             \
    MACRO_START {                                                                              \
        if (!rwLinkListEmpty(&(_frame)->objectList)) {                                         \
            RwLLLink* current = rwLinkListGetFirstLLLink(&(_frame)->objectList);               \
            RwLLLink* end = rwLinkListGetTerminator(&(_frame)->objectList);                    \
                                                                                               \
            while (current != end) {                                                           \
                RwObjectHasFrame* object = rwLLLinkGetData(current, RwObjectHasFrame, lFrame); \
                                                                                               \
                object->sync(object);                                                          \
                current = rwLLLinkGetNext(current);                                            \
            }                                                                                  \
        }                                                                                      \
    }                                                                                          \
    MACRO_STOP

/*
 * Brings a list of sibling subtrees up to date below a parent whose LTM was rebuilt: a frame
 * that moved, or whose ancestor moved (flags), gets its LTM rebuilt from its parent's.
 */
static void FrameSyncHierarchyRecurse(RwFrame* frame, RwInt32 flags) {
    while (frame) {
        RwInt32 accumflags = flags | rwObjectGetPrivateFlags(frame);

        if (accumflags & rwFRAMEPRIVATESUBTREESYNCLTM) {
            RwMatrixMultiply(&frame->ltm, &frame->modelling, &RwFrameGetParent(frame)->ltm);
        }

        rwFrameSyncObjects(frame);

        rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) & ~rwFRAMEPRIVATESUBTREESYNC);

        FrameSyncHierarchyRecurse(frame->child, accumflags);
        frame = frame->next;
    }
}

/* Brings a list of sibling subtrees up to date when no LTM needs rebuilding: objects only. */
static void FrameSyncHierarchyRecurseNoLTM(RwFrame* frame) {
    while (frame) {
        rwFrameSyncObjects(frame);

        rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) & ~rwFRAMEPRIVATESUBTREESYNCOBJ);

        FrameSyncHierarchyRecurseNoLTM(frame->child);
        frame = frame->next;
    }
}

/* Brings one dirty hierarchy up to date from its root. */
static void FrameSyncHierarchy(RwFrame* root) {
    RwUInt8 flags = rwObjectGetPrivateFlags(root);

    if (flags & rwFRAMEPRIVATEHIERARCHYSYNCLTM) {
        if (flags & rwFRAMEPRIVATESUBTREESYNCLTM) {
            RwMatrixCopy(&root->ltm, &root->modelling);
        }

        rwFrameSyncObjects(root);
        FrameSyncHierarchyRecurse(root->child, flags & rwFRAMEPRIVATESUBTREESYNCLTM);
    } else {
        rwFrameSyncObjects(root);
        FrameSyncHierarchyRecurseNoLTM(root->child);
    }

    rwObjectSetPrivateFlags(root, flags & ~(rwFRAMEPRIVATEHIERARCHYSYNC | rwFRAMEPRIVATESUBTREESYNC));
}

/* Brings every hierarchy in the dirty frame list up to date and empties the list. */
RwBool _rwFrameSyncDirty(void) {
    RwLLLink* current = rwLinkListGetFirstLLLink(&RWSRCGLOBAL(dirtyFrameList));
    RwLLLink* end = rwLinkListGetTerminator(&RWSRCGLOBAL(dirtyFrameList));

    while (current != end) {
        FrameSyncHierarchy(rwLLLinkGetData(current, RwFrame, inDirtyListLink));
        current = rwLLLinkGetNext(current);
    }

    rwLinkListInitialize(&RWSRCGLOBAL(dirtyFrameList));

    return TRUE;
} // 0x8029815C

/* Rebuilds the LTMs (only) of a list of sibling subtrees below a rebuilt parent. */
static void FrameSyncHierarchyLTMRecurse(RwFrame* frame, RwInt32 flags) {
    while (frame) {
        RwInt32 accumflags = flags | rwObjectGetPrivateFlags(frame);

        if (accumflags & rwFRAMEPRIVATESUBTREESYNCLTM) {
            RwMatrixMultiply(&frame->ltm, &frame->modelling, &RwFrameGetParent(frame)->ltm);
            rwObjectSetPrivateFlags(frame, rwObjectGetPrivateFlags(frame) & ~rwFRAMEPRIVATESUBTREESYNCLTM);
        }

        FrameSyncHierarchyLTMRecurse(frame->child, accumflags);
        frame = frame->next;
    }
}

/* Rebuilds the LTMs (only) of a hierarchy from its root; the objects stay dirty. */
void _rwFrameSyncHierarchyLTM(RwFrame* root) {
    RwUInt8 flags = rwObjectGetPrivateFlags(root);

    if (flags & rwFRAMEPRIVATESUBTREESYNCLTM) {
        RwMatrixCopy(&root->ltm, &root->modelling);
    }

    FrameSyncHierarchyLTMRecurse(root->child, flags);

    rwObjectSetPrivateFlags(root, flags & ~(rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATESUBTREESYNCLTM));
} // 0x8029824C
